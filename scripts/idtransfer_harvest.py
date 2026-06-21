#!/usr/bin/env python3
"""idtransfer_harvest.py - the identity-transfer harvest DRIVER (PIPELINE-DESIGN
.md S2 architecture / S3 Phases 1-10 / S9 B3 / S10 hard-fail gates).

WHAT THIS IS
------------
One gated command that chains the 8 manual identity-transfer steps for ONE TU in
a copy-on-write git worktree, with every honesty gate as a callable HARD FAIL. It
turns the attrition-bound manual grind (PIPELINE-DESIGN.md S9 B3 "force
multiplier") into a single parallelizable invocation that emits a machine-readable
verdict (``LANDABLE:+N`` / ``DEFER:<reason>``) for a human or ``scripts/harvest/
land.sh`` to act on. It NEVER auto-lands and NEVER mutates the main tree (CLAUDE.md
HARD RULE: no git stash/checkout/reset/restore of files in the shared main tree).

THE PHASES (each prints a banner + result; each gate is a hard fail)
-------------------------------------------------------------------
  PREFLIGHT       (G3) TU wired in objects.json? compiled obj exists?
                       fingerprints.json fresh vs symbols.txt? (warn-or-abort)
  WORKTREE        scripts/setup_worktree.sh -> a CoW worktree (NEVER touch main)
  BASELINE        (G7) fresh_report.sh in the worktree; record
                       measures.matched_functions as the in-tree baseline
  IDENTIFY        identity_transfer.py classify path: case-A / SELF / case-B
                       (covering_pin); the span-pin HARD GATE is preserved
  LOCATE          locator.py --emit-gate sidecar (re-tasked as a SKIP list:
                       drop MISATTRIBUTED + WALL; do NOT gate IN on CONFIRMED)
  FIELD-GATE      (G5) field_offset_gate.py -> the clean --pin-only VA set
                       (drops STUB/WALL/Handle/POISONED-TAIL)
  MICRO-PIN+MAP   identity_transfer.py --pin-only --apply (STRICT ADD-ONLY map;
                       NEVER gen_game_target_map.py --apply on a scattered TU)
  OVERLAP         (G7) scripts/harvest/overlap_check.py -> abort on .text overlap
  BUILD+MEASURE   (G4) rm target_symbol_renames.stamp; touch config.yml;
                       fresh_report.sh; delta vs the BASELINE
  AUDIT           (G9) icf_alias_check.py --worktree -> abort on stub-fold
  VERDICT         LANDABLE:+N  or  DEFER:<reason>  (NOT auto-landed)

``--dry-run`` runs PREFLIGHT -> WORKTREE -> BASELINE -> IDENTIFY -> LOCATE ->
FIELD-GATE, then PRINTS THE PLANNED PIN-SET and STOPS before MICRO-PIN+MAP (no
splits/map mutation, no second build). ``--no-build`` additionally skips the
BASELINE build (PREFLIGHT/IDENTIFY/FIELD-GATE only) for a fast plan smoke-test on
an already-wired TU that needs no compile.

USAGE
-----
  scripts/idtransfer_harvest.py --tu RockCentral.cpp
  scripts/idtransfer_harvest.py --tu BandProfile.cpp --D 0x788
  scripts/idtransfer_harvest.py --tu RockCentral.cpp --dry-run
  scripts/idtransfer_harvest.py --tu RockCentral.cpp --class Stats,PerformanceData --D 0x114

EXIT CODES
----------
  0  LANDABLE:+N (or, with --dry-run, plan emitted cleanly)
  1  DEFER:<reason>  (a gate hard-failed; the verdict line names the reason)
  2  PREFLIGHT/setup error (TU not wired, obj missing, worktree setup failed)
"""
import argparse
import json
import os
import re
import shlex
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS = os.path.join(ROOT, "tools")
HARVEST = os.path.join(ROOT, "scripts", "harvest")
sys.path.insert(0, TOOLS)
sys.path.insert(0, HARVEST)

VERSION = "45410914"
CONFIG_DIR = os.path.join("config", VERSION)                 # repo-relative
OBJECTS_JSON = os.path.join(CONFIG_DIR, "objects.json")
SPLITS_TXT = os.path.join(CONFIG_DIR, "splits.txt")
SYMBOLS_TXT = os.path.join(CONFIG_DIR, "symbols.txt")
CONFIG_YML = os.path.join(CONFIG_DIR, "config.yml")
REPORT_JSON = os.path.join("build", VERSION, "report.json")
RENAME_STAMP = os.path.join("build", VERSION, "target_symbol_renames.stamp")
FINGERPRINTS = "fingerprints.json"

# Phase banner width (matches the tool reports' 72/74-col convention).
BW = 74


# ===========================================================================
# Small process / IO helpers
# ===========================================================================
class HarvestError(Exception):
    """A hard-fail gate tripped. ``code`` is the process exit code; ``verdict``
    is the machine-readable ``DEFER:<reason>`` (or None for a setup error)."""
    def __init__(self, msg, code=1, verdict=None):
        super().__init__(msg)
        self.code = code
        self.verdict = verdict


def banner(phase, sub=""):
    line = f"  PHASE: {phase}" + (f"   ({sub})" if sub else "")
    print("\n" + "=" * BW)
    print(line)
    print("=" * BW)


def info(msg):
    print(f"    {msg}")


def warn(msg):
    print(f"    WARN: {msg}", file=sys.stderr)


def run(cmd, cwd=None, env=None, check=True, capture=False, log=None):
    """Run a command, streaming (or capturing) output. ``cmd`` is a list.

    On non-zero exit with ``check`` we raise HarvestError so the driver's gate
    logic can map it to a DEFER verdict. ``capture=True`` returns stdout text and
    suppresses streaming (used for the cheap value-extracting calls)."""
    printable = " ".join(shlex.quote(c) for c in cmd)
    info(f"$ {printable}" + (f"   (cwd={cwd})" if cwd else ""))
    if capture:
        proc = subprocess.run(cmd, cwd=cwd, env=env, text=True,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        out = proc.stdout or ""
        if log:
            with open(log, "a") as fh:
                fh.write(out)
        if check and proc.returncode != 0:
            sys.stdout.write(out)
            raise HarvestError(f"command failed (rc={proc.returncode}): {printable}",
                              code=proc.returncode)
        return proc.returncode, out
    proc = subprocess.run(cmd, cwd=cwd, env=env)
    if check and proc.returncode != 0:
        raise HarvestError(f"command failed (rc={proc.returncode}): {printable}",
                          code=proc.returncode)
    return proc.returncode, None


def read_matched(report_path):
    """Return measures.matched_functions from a report.json, or None."""
    try:
        with open(report_path) as fh:
            r = json.load(fh)
        return r.get("measures", {}).get("matched_functions")
    except (OSError, ValueError):
        return None


# ===========================================================================
# Phase 1 - PREFLIGHT (runs against MAIN; read-only, no mutation)
# ===========================================================================
def resolve_tu(tu_arg):
    """Normalize ``--tu`` to a ``Foo.cpp`` basename."""
    tu = tu_arg if tu_arg.endswith((".cpp", ".c", ".cc")) else tu_arg + ".cpp"
    return os.path.basename(tu)


def find_objects_key(tu_basename):
    """Find the objects.json source-relative key whose basename == tu_basename.
    Returns (key, status) or (None, None). Aborts the *ambiguity* up-front: if
    two groups list the same basename, we cannot tell which obj is the target."""
    obj = json.load(open(os.path.join(ROOT, OBJECTS_JSON)))
    matches = []
    for grp in obj.values():
        for k, status in (grp.get("objects") or {}).items():
            if os.path.basename(k) == tu_basename:
                matches.append((k, status))
    if not matches:
        return None, None
    if len(matches) > 1:
        keys = ", ".join(k for k, _ in matches)
        raise HarvestError(
            f"TU basename '{tu_basename}' is AMBIGUOUS in objects.json "
            f"({keys}); pass the full source-relative path is not supported -- "
            f"this driver targets unique-basename scattered TUs.", code=2)
    return matches[0]


def preflight(tu, fail_on_stale_fp):
    """G3: TU wired in objects.json? compiled obj exists? fingerprints fresh?"""
    banner("PREFLIGHT", tu)

    # 1. wired in objects.json
    key, status = find_objects_key(tu)
    if key is None:
        raise HarvestError(
            f"TU '{tu}' is NOT wired in {OBJECTS_JSON} (add it as NonMatching "
            f"and port its source first -- Phase 0).", code=2)
    info(f"wired: objects.json key '{key}' status={status!r}")

    # 2. compiled obj exists (build/<ver>/src/<key-with-.obj>)
    obj_rel = os.path.splitext(key)[0] + ".obj"
    obj_path = os.path.join(ROOT, "build", VERSION, "src", obj_rel)
    if not os.path.isfile(obj_path):
        raise HarvestError(
            f"compiled obj missing: {obj_path}\n"
            f"  -> the TU is wired but not yet built; run a build so the obj "
            f"DEFINES every symbol before harvesting (Phase 0).", code=2)
    info(f"compiled obj present: {os.path.relpath(obj_path, ROOT)} "
         f"({os.path.getsize(obj_path)} B)")

    # 3. fingerprints.json fresh vs symbols.txt (G3 -- stale fp weakens locator)
    fp_path = os.path.join(ROOT, FINGERPRINTS)
    sym_path = os.path.join(ROOT, SYMBOLS_TXT)
    if not os.path.isfile(fp_path):
        msg = (f"{FINGERPRINTS} missing (locator signals B/C degrade); "
               f"regenerate via tools/fingerprint_match.py extract")
        if fail_on_stale_fp:
            raise HarvestError("PREFLIGHT: " + msg, code=2)
        warn(msg)
    elif os.path.isfile(sym_path) and \
            os.path.getmtime(fp_path) < os.path.getmtime(sym_path):
        msg = (f"{FINGERPRINTS} is OLDER than {SYMBOLS_TXT} (stale fingerprints "
               f"weaken locator B/C signals); regenerate via "
               f"tools/fingerprint_match.py extract")
        if fail_on_stale_fp:
            raise HarvestError("PREFLIGHT: " + msg, code=2)
        warn(msg + "  -- continuing (warn only; pass --strict-fingerprints to abort)")
    else:
        info(f"fingerprints fresh: {FINGERPRINTS} >= {os.path.basename(sym_path)}")

    return key, obj_rel


# ===========================================================================
# Phase 2 - WORKTREE + BASELINE
# ===========================================================================
def setup_worktree(tu, base_ref):
    """Create a CoW worktree via setup_worktree.sh. Returns its absolute path."""
    banner("WORKTREE", tu)
    stem = os.path.splitext(tu)[0]
    ts = time.strftime("%Y%m%d-%H%M%S")
    wt_path = os.path.join(ROOT, ".claude", "worktrees", f"idt-{stem}-{ts}")
    branch = f"idt-{stem}-{ts}"
    script = os.path.join(ROOT, "scripts", "setup_worktree.sh")
    cmd = ["bash", script, wt_path, branch]
    if base_ref:
        cmd.append(base_ref)
    rc, _ = run(cmd, check=False)
    if rc != 0 or not os.path.isdir(wt_path):
        raise HarvestError(
            f"setup_worktree.sh failed (rc={rc}) -- worktree not created at "
            f"{wt_path}", code=2)
    info(f"worktree ready: {wt_path}  (branch {branch})")
    return wt_path, branch


def fresh_report(wt_path, ninja_jobs):
    """Run tools/fresh_report.sh inside the worktree; return the resulting
    measures.matched_functions (or raise on build failure)."""
    env = dict(os.environ)
    if ninja_jobs is not None:
        env["NINJA_JOBS"] = str(ninja_jobs)
    script = os.path.join(wt_path, "tools", "fresh_report.sh")
    run(["bash", script], cwd=wt_path, env=env)
    matched = read_matched(os.path.join(wt_path, REPORT_JSON))
    if matched is None:
        raise HarvestError(
            f"fresh_report.sh produced no readable measures.matched_functions "
            f"in {os.path.join(wt_path, REPORT_JSON)}", code=1,
            verdict="DEFER:no-report")
    return matched


def baseline(wt_path, tu, ninja_jobs, skip_build):
    """G7: record measures.matched_functions in the SAME tree as the baseline.

    Measuring in-tree (not vs an external fixed baseline) is the wave-9 lesson:
    a fixed external baseline double-counts foundational levers."""
    banner("BASELINE", tu)
    if skip_build:
        info("--no-build: skipping the baseline build (plan-only smoke test)")
        return None
    matched = fresh_report(wt_path, ninja_jobs)
    info(f"baseline measures.matched_functions = {matched}")
    # Stash the baseline report for the AUDIT --worktree diff later.
    base_report = os.path.join(wt_path, "build", VERSION, "report_baseline.json")
    src = os.path.join(wt_path, REPORT_JSON)
    try:
        with open(src) as a, open(base_report, "w") as b:
            b.write(a.read())
        info(f"baseline report snapshotted: {os.path.relpath(base_report, wt_path)}")
    except OSError as ex:
        warn(f"could not snapshot baseline report ({ex}); AUDIT --worktree will "
             f"need --baseline-report manually")
        base_report = None
    return matched, base_report


# ===========================================================================
# Phases 3-5 - IDENTIFY (dry-run classify) / LOCATE / FIELD-GATE
# ===========================================================================
def identify(wt_path, tu, allow_span_coexist):
    """Phase 3: identity_transfer.py CLASSIFY (dry-run) -- case-A/SELF/case-B,
    span-pin HARD GATE. We run the dry-run form to surface the classification and
    let its own span-pin gate decide whether anything is carvable at all."""
    banner("IDENTIFY", tu)
    cmd = [sys.executable, os.path.join(wt_path, "tools", "identity_transfer.py"),
           "--tu", tu]
    if allow_span_coexist:
        cmd.append("--allow-span-coexist")
    rc, out = run(cmd, cwd=wt_path, capture=True, check=False)
    sys.stdout.write(out)
    # The dry-run classifier returns 0 even when the span-pin gate trips (it just
    # reports). A non-zero rc here is a genuine classify error (e.g. no oracle).
    if rc != 0:
        raise HarvestError(f"IDENTIFY classify failed (rc={rc}) for {tu}",
                          code=1, verdict="DEFER:classify-error")
    # Surface the span-pin HARD GATE so the driver fails closed too.
    if "HARD GATE" in out and not allow_span_coexist:
        raise HarvestError(
            f"IDENTIFY: {tu} carries a SPAN pin -> identity_transfer HARD GATE "
            f"tripped (wave-16 -14 collision root). Pass --allow-span-coexist "
            f"only when the non-colliding remainder is independently verified.",
            code=1, verdict="DEFER:span-pin-gate")
    n_case_a = _parse_int(out, r"CASE-A unowned-blob\s*:\s*(\d+)")
    info(f"IDENTIFY: case-A carve candidates = {n_case_a}")
    if not n_case_a:
        raise HarvestError(
            f"IDENTIFY: 0 case-A candidates for {tu} (all SELF / case-B / "
            f"stub) -- nothing to micro-pin.", code=1, verdict="DEFER:no-case-a")
    return n_case_a


def locate(wt_path, tu):
    """Phase 4: locator.py --emit-gate -> a SKIP-list sidecar. Returns the
    sidecar path (in the worktree) or None if the locator could not run (the
    field-gate degrades to size+name+tail only -- conservative)."""
    banner("LOCATE", tu)
    stem = os.path.splitext(tu)[0]
    sidecar = os.path.join(wt_path, "build", VERSION, f"locator_{stem}.json")
    cmd = [sys.executable, os.path.join(wt_path, "tools", "locator.py"),
           "--tu", tu, "--emit-gate", sidecar]
    rc, out = run(cmd, cwd=wt_path, capture=True, check=False)
    sys.stdout.write(out)
    if rc != 0 or not os.path.isfile(sidecar):
        warn(f"locator failed (rc={rc}); FIELD-GATE will run WITHOUT the locator "
             f"SKIP list (size+name+tail only -- conservative, retains more)")
        return None
    info(f"locator sidecar: {os.path.relpath(sidecar, wt_path)}")
    return sidecar


def field_gate(wt_path, tu, D, diverging_class, locator_sidecar):
    """Phase 5 (G5): field_offset_gate.py -> the clean --pin-only VA list.
    Returns (pin_only_path, n_pins)."""
    banner("FIELD-GATE", f"{tu}  D={D}" if D else f"{tu}  D=inf")
    stem = os.path.splitext(tu)[0]
    pin_only = os.path.join(wt_path, "build", VERSION, f"pinonly_{stem}.json")
    cmd = [sys.executable, os.path.join(wt_path, "tools", "field_offset_gate.py"),
           "--tu", tu, "--emit-pin-only", pin_only]
    if D:
        cmd += ["--D", D]
    if diverging_class:
        cmd += ["--class", diverging_class]
    if locator_sidecar:
        cmd += ["--locator-gate", locator_sidecar]
    rc, out = run(cmd, cwd=wt_path, capture=True, check=False)
    sys.stdout.write(out)
    if rc != 0 or not os.path.isfile(pin_only):
        raise HarvestError(
            f"FIELD-GATE failed (rc={rc}); no pin-only list emitted for {tu}.",
            code=1, verdict="DEFER:field-gate-error")
    try:
        vas = json.load(open(pin_only))
    except (OSError, ValueError) as ex:
        raise HarvestError(f"FIELD-GATE wrote an unreadable pin-only list: {ex}",
                          code=1, verdict="DEFER:field-gate-error")
    info(f"FIELD-GATE clean pin-set: {len(vas)} VA(s) -> {os.path.relpath(pin_only, wt_path)}")
    if not vas:
        raise HarvestError(
            f"FIELD-GATE: 0 clean pins for {tu} (every method is stub / WALL / "
            f"POISONED-TAIL). The port wins nothing before the struct-lever "
            f"(PIPELINE-DESIGN.md B5).", code=1, verdict="DEFER:empty-pin-set")
    return pin_only, vas


# ===========================================================================
# Phase 6 - MICRO-PIN + MAP (the only mutating step; runs in the worktree)
# ===========================================================================
def micro_pin(wt_path, tu, pin_only, locator_sidecar, allow_span_coexist):
    """Phase 6: identity_transfer.py --pin-only --apply (STRICT ADD-ONLY map).

    This is the ONLY step that mutates splits.txt + target_symbol_map.json, and
    it runs INSIDE the worktree (never main). The locator gate is passed as a
    SKIP list; the --pin-only set is the field-gate clean subset; the span-pin
    HARD GATE / FIX-1 collision drop / boundary-snap are enforced by the tool."""
    banner("MICRO-PIN+MAP", tu)
    cmd = [sys.executable, os.path.join(wt_path, "tools", "identity_transfer.py"),
           "--tu", tu, "--pin-only", pin_only, "--apply"]
    if locator_sidecar:
        cmd += ["--locator-gate", locator_sidecar]
    if allow_span_coexist:
        cmd.append("--allow-span-coexist")
    rc, out = run(cmd, cwd=wt_path, capture=True, check=False)
    sys.stdout.write(out)
    if rc != 0:
        # identity_transfer returns 1 when there's nothing clean to apply (span
        # gate / all-filtered) -- that is a legitimate DEFER, not a crash.
        raise HarvestError(
            f"MICRO-PIN: identity_transfer --apply wrote nothing (rc={rc}); the "
            f"clean pin-set collapsed under the span/FIX-1/boundary gates.",
            code=1, verdict="DEFER:nothing-applied")
    n_applied = _parse_int(out, r"\+(\d+) \.text micro-range")
    info(f"MICRO-PIN: applied {n_applied} .text micro-range(s) to the worktree")
    return n_applied


# ===========================================================================
# Phase 7 - OVERLAP (the splits-overlap HARD GATE, shared with land.sh)
# ===========================================================================
def overlap(wt_path, tu):
    """Phase 7 (G7): scripts/harvest/overlap_check.py -> abort BEFORE build on
    any .text range overlap (independently-developed pins can collide)."""
    banner("OVERLAP", tu)
    import overlap_check  # shared module (scripts/harvest/overlap_check.py)
    wt_splits = os.path.join(wt_path, SPLITS_TXT)
    rc = overlap_check.check_splits(wt_splits, text_only=True)
    if rc != 0:
        raise HarvestError(
            f"OVERLAP: the appended micro-pins overlap an existing .text range "
            f"-- jeff validate_splits would reject the build.", code=1,
            verdict="DEFER:splits-overlap")
    info("OVERLAP: no .text overlap (clean to build)")
    return 0


# ===========================================================================
# Phase 8 - BUILD + MEASURE
# ===========================================================================
def build_measure(wt_path, tu, baseline_matched, ninja_jobs):
    """Phase 8 (G4): rm the rename stamp (else the renamer is stale = silently
    wrong measure), touch config.yml, fresh_report.sh; delta vs the baseline."""
    banner("BUILD+MEASURE", tu)
    stamp = os.path.join(wt_path, RENAME_STAMP)
    if os.path.isfile(stamp):
        os.remove(stamp)
        info(f"removed stale rename stamp: {os.path.relpath(stamp, wt_path)}")
    # touch config.yml so dtk re-SPLITs the new micro-ranges
    cfg = os.path.join(wt_path, CONFIG_YML)
    if os.path.isfile(cfg):
        os.utime(cfg, None)
        info(f"touched {os.path.relpath(cfg, wt_path)} (force re-SPLIT)")
    else:
        warn(f"{CONFIG_YML} not found in worktree -- re-SPLIT may not fire")
    matched = fresh_report(wt_path, ninja_jobs)
    delta = matched - baseline_matched
    info(f"post-apply measures.matched_functions = {matched} "
         f"(baseline {baseline_matched}, delta {delta:+d})")
    if delta <= 0:
        raise HarvestError(
            f"BUILD+MEASURE: net delta {delta:+d} (<= 0) -- the micro-pins did "
            f"not produce a net byte-match gain.", code=1,
            verdict=f"DEFER:net{delta:+d}")
    return matched, delta


# ===========================================================================
# Phase 9 - AUDIT (ICF-alias stub-fold honesty gate)
# ===========================================================================
def audit(wt_path, tu, base_report):
    """Phase 9 (G9): icf_alias_check.py --worktree -> abort on stub-fold
    inflation (newly-100 must be REAL bodies, not <=44B ICF folds)."""
    banner("AUDIT", tu)
    if not base_report or not os.path.isfile(base_report):
        raise HarvestError(
            f"AUDIT: no baseline report to diff against (expected "
            f"{base_report}); cannot run the ICF-alias honesty gate.", code=1,
            verdict="DEFER:no-baseline-report")
    cmd = [sys.executable, os.path.join(wt_path, "tools", "icf_alias_check.py"),
           "--worktree", wt_path, "--baseline-report", base_report, "--list"]
    rc, out = run(cmd, cwd=wt_path, capture=True, check=False)
    sys.stdout.write(out)
    # icf_alias_check exits 0 = HONEST, 1 = ICF-ALIAS INFLATION, 2 = usage error.
    if rc == 1:
        raise HarvestError(
            f"AUDIT: ICF-ALIAS INFLATION -- the newly-matched set is dominated "
            f"by <=44B stub-folds, not real bodies.", code=1,
            verdict="DEFER:icf-inflation")
    if rc != 0:
        raise HarvestError(f"AUDIT: icf_alias_check failed (rc={rc})", code=1,
                          verdict="DEFER:audit-error")
    info("AUDIT: HONEST (real-bodied-dominated newly-matched set)")
    return 0


# ===========================================================================
# misc
# ===========================================================================
def _parse_int(text, pattern):
    m = re.search(pattern, text)
    return int(m.group(1)) if m else 0


# ===========================================================================
# Driver
# ===========================================================================
def drive(args):
    tu = resolve_tu(args.tu)
    print(f"idtransfer_harvest: TU={tu}  D={args.D or 'inf'}  "
          f"class={args.diverging_class or 'ALL'}  "
          f"{'DRY-RUN' if args.dry_run else 'FULL'}"
          f"{'  NO-BUILD' if args.no_build else ''}")

    # --- Phase 1: PREFLIGHT (against main; read-only) ------------------------
    preflight(tu, args.strict_fingerprints)

    # --- Phase 2: WORKTREE ---------------------------------------------------
    wt_path, branch = setup_worktree(tu, args.base_ref)

    # --- Phase 2b: BASELINE --------------------------------------------------
    base_report = None
    baseline_matched = None
    bl = baseline(wt_path, tu, args.ninja_jobs, skip_build=args.no_build)
    if bl is not None:
        baseline_matched, base_report = bl

    # --- Phase 3: IDENTIFY ---------------------------------------------------
    identify(wt_path, tu, args.allow_span_coexist)

    # --- Phase 4: LOCATE -----------------------------------------------------
    locator_sidecar = locate(wt_path, tu)

    # --- Phase 5: FIELD-GATE -------------------------------------------------
    pin_only, vas = field_gate(wt_path, tu, args.D, args.diverging_class,
                               locator_sidecar)

    # --- DRY-RUN stop: print the planned pin-set, do NOT mutate --------------
    if args.dry_run:
        banner("DRY-RUN PLAN", tu)
        info(f"would identity_transfer --pin-only {os.path.relpath(pin_only, wt_path)} "
             f"--apply  ({len(vas)} VA(s))")
        print("\n    PLANNED PIN-SET (field-gate clean subset):")
        for va in vas:
            print(f"      {va}")
        print(f"\nDRY-RUN: stopped before MICRO-PIN+MAP. worktree at {wt_path}")
        print(f"VERDICT: DRY-RUN:{len(vas)} (planned pins; no mutation, no build)")
        return 0

    if args.no_build:
        # No-build was a plan smoke-test; without a baseline we cannot measure.
        banner("NO-BUILD STOP", tu)
        info("--no-build: baseline build was skipped, so MICRO-PIN+MAP would "
             "have no measurable delta. Stopping after the plan (smoke test).")
        print(f"\n    PLANNED PIN-SET ({len(vas)} VA(s)):")
        for va in vas:
            print(f"      {va}")
        print(f"VERDICT: NO-BUILD:{len(vas)} (plan only; re-run without --no-build "
              f"to measure)")
        return 0

    # --- Phase 6: MICRO-PIN + MAP (mutating; worktree only) ------------------
    micro_pin(wt_path, tu, pin_only, locator_sidecar, args.allow_span_coexist)

    # --- Phase 7: OVERLAP ----------------------------------------------------
    overlap(wt_path, tu)

    # --- Phase 8: BUILD + MEASURE --------------------------------------------
    _matched, delta = build_measure(wt_path, tu, baseline_matched,
                                    args.ninja_jobs)

    # --- Phase 9: AUDIT ------------------------------------------------------
    audit(wt_path, tu, base_report)

    # --- Phase 10: VERDICT ---------------------------------------------------
    banner("VERDICT", tu)
    info(f"all gates passed; worktree branch '{branch}' at {wt_path}")
    info(f"land with: scripts/harvest/land.sh {branch}   (then composed verify)")
    print(f"VERDICT: LANDABLE:+{delta}")
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--tu", required=True,
                    help="target TU basename, e.g. RockCentral.cpp")
    ap.add_argument("--D", default=None,
                    help="field_offset_gate divergence point (hex, e.g. 0x788)")
    ap.add_argument("--class", dest="diverging_class", default=None,
                    help="field_offset_gate diverging class scope (e.g. "
                         "Stats,PerformanceData)")
    ap.add_argument("--dry-run", action="store_true",
                    help="stop before MICRO-PIN+MAP and print the planned "
                         "pin-set (no splits/map mutation, no second build)")
    ap.add_argument("--no-build", action="store_true",
                    help="skip the baseline build (PREFLIGHT/IDENTIFY/FIELD-GATE "
                         "plan smoke test only; implies a plan-only stop)")
    ap.add_argument("--allow-span-coexist", action="store_true",
                    help="apply the non-colliding micro-pin remainder even when "
                         "the TU already has a SPAN pin (default: fail-closed)")
    ap.add_argument("--base-ref", default=None,
                    help="git ref to branch the worktree from (default: HEAD)")
    ap.add_argument("--ninja-jobs", type=int, default=None,
                    help="NINJA_JOBS cap for fresh_report.sh (default: its 12)")
    ap.add_argument("--strict-fingerprints", action="store_true",
                    help="abort PREFLIGHT on stale/missing fingerprints.json "
                         "(default: warn and continue)")
    args = ap.parse_args(argv)

    try:
        return drive(args)
    except HarvestError as ex:
        banner("ABORT")
        print(f"    {ex}", file=sys.stderr)
        if ex.verdict:
            print(ex.verdict)
        else:
            print(f"DEFER:setup-error")
        return ex.code


if __name__ == "__main__":
    sys.exit(main())
