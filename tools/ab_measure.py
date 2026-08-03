#!/usr/bin/env python3
"""ab_measure.py — whole-binary A/B measurement, safe by default.

This tool IS the A/B protocol. It replaces the prose checklist that failed
repeatedly (settling noise, inert map edits, absent-vs-absent legs, stale
report caches, accumulated absolutes). Every step below neutralizes a
measured footgun; the tool REFUSES to emit numbers when any precondition
fails, rather than reporting on a broken run.

Protocol (what this tool does, and why — keep this list in sync with
CLAUDE.md "Whole-binary A/B measurement"):

  1. Preflight: target must be a LINKED WORKTREE (never the shared main
     repo), with clean tracked state. `config/45410914/symbols.txt` drift is
     auto-restored (footgun: drift breaks the split).
  2. Classify the patch by touched paths: map / splits / configgen / source.
     A patch touching symbols.txt is refused outright. A patch touching no
     build-relevant path is refused (an inert A/B measures nothing) unless
     --allow-inert.
  3. SETTLE: build until a build performs ZERO work (no MSVC/SPLIT/PATCH
     lines). A fresh worktree's first build reads ~+193 matched / +0.51pp of
     pure settling noise; four lanes in one session had leg A come back at
     198 recompiles. Bounded retries; REFUSES if it cannot settle.
     For map/splits patches the settle phase already forces a re-split so
     BOTH legs are measured in the same build state (footgun 8).
  4. LEG A: wipe report.json + report.cache (stale cache inflates), rebuild
     the report, assert the report build did zero compile work, parse
     measures BY EXACT KEY (a missing key is an ERROR, never a default 0 —
     `measures.get('masked_equal', 0)` once read as "no masked functions").
  5. APPLY the patch; verify it actually changed tracked files.
  6. FORCE what the change kind needs: map/splits => restore symbols.txt,
     rm the renamer stamp, touch config.yml (a map edit is INERT without a
     forced re-split: lane CF-1 lost a leg to "[APPLIED] ... 0 files
     patched"); configgen => rerun configure.py.
  7. LEG B build. The recompile count comes from THIS build's log, before
     any report generation (run_objdiff-style flows hide the compile, so a
     later count reads 0 and cannot prove application). Assertions:
       source patch  => MSVC recompiles >= 1, else REFUSE (absent-vs-absent)
       map patch     => SPLIT ran AND renamer patched > 0 files, else REFUSE
       splits patch  => SPLIT ran, else REFUSE
  8. LEG B read: wipe cache + report again, regenerate, strict parse.
  9. Emit Δmatched, Δmasked_equal, Δhonest(=matched−masked_equal), Δcode%,
     Δfuzzy, each leg's recompile count, and the absolutes for each leg
     ACTUALLY MEASURED. Output is assembled only from this run's parsed
     reports — there is no --baseline flag on purpose: deltas compose,
     absolutes do not, and a baseline file is an absolute somebody else
     measured.
     Δfuzzy is in the HEADLINE, not just result.json: for map/correctness
     lanes fuzzy_match_percent is THE witness — lane CG-3's entire result
     (Δmatched 0, Δfuzzy +0.030838) was invisible in the old headline.
 10. The per-unit breakdown is a TRUNCATED top-N list, but it is printed
     with the untruncated counts and sums plus an explicit TRUNCATED flag.
     Lane CG-1's listed regressions summed to -18 against a -19 delta (one
     unit fell off the end of a silent regs[:limit]), which silently breaks
     the house discipline of pairing losses to gains by (size, unit).

Scoring ruler: the ninja report edge hard-codes functionRelocDiffs=None
(objdiff-cli report.rs generate()), i.e. the DEFAULT ruler. --name-check
adds a second, opt-in name_check reading per leg with the required warning
(its aggregate code% is build-unstable ~0.05pp; small nc deltas mean
nothing).

Usage:
  tools/ab_measure.py --worktree WT --patch FILE      # measure a diff file
  tools/ab_measure.py --worktree WT --pick REF        # measure applying commit REF
  tools/ab_measure.py --worktree WT --revert REF      # measure reverting commit REF
  tools/ab_measure.py --worktree WT --from-dirty      # measure WT's uncommitted diff
  tools/ab_measure.py --selftest                      # no-build sanity of refusal logic

Exit codes: 0 = measured, 2 = REFUSED (no verdict), 3 = usage/internal.
Run artifacts (logs, patch copy, result.json) go to ~/tmp/ab_measure/<run>/.
"""

import argparse
import gzip
import hashlib
import inspect
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

TITLE = "45410914"
REPORT_REL = f"build/{TITLE}/report.json"
CACHE_REL = f"build/{TITLE}/report.cache"
STAMP_REL = f"build/{TITLE}/target_symbol_renames.stamp"
SYMBOLS_REL = f"config/{TITLE}/symbols.txt"
CONFIG_YML_REL = f"config/{TITLE}/config.yml"

TOOL_REL = "tools/ab_measure.py"

MAP_PATHS = {"scripts/target_symbol_map.json", "scripts/symbol_aliases.json"}
SPLIT_PATHS = {f"config/{TITLE}/splits.txt", CONFIG_YML_REL}
CONFIGGEN_PATHS = {
    "configure.py",
    "tools/project.py",
    "tools/defines_common.py",
    f"config/{TITLE}/objects.json",
}
# Directories whose untracked/modified state can change build output.
BUILD_RELEVANT_DIRS = ("src/", "config/", "scripts/", "tools/")

# Ninja edge descriptions that are NOT work (expected on every/no-op build).
NON_WORK_DESCS = {"REPORT", "PROGRESS", "CHANGESFMT", "CHANGES"}

WORKLINE_RE = re.compile(r"^\[\d+/\d+\]\s+(\S+)")
RENAMER_RE = re.compile(
    r"\[(?:APPLIED|DRY RUN)\]\s+(\d+) files checked,\s+(\d+) files patched")
# ⚠ SIX patchers emit the identical '[APPLIED] N files checked, M files
# patched' line, so a bare search binds the LAST one — and guard/bool_mangle
# legitimately report 0. Lane CT-1 hit the resulting FALSE REFUSAL: a leg B
# recompiling only 2 TUs fired no later patcher, so the gate read the renamer's
# real 1045 correctly; add more recompiles and a later 0 overwrites it. The
# mirror case is the dangerous one — a genuinely INERT map edit would PASS the
# gate whenever any later patcher reports >0. Bind the figure to the renamer's
# own ninja step instead of to line order.
RENAMER_STEP_RE = re.compile(r"^\[\d+/\d+\]\s+PATCH target fn_")

# Required top-level measures keys. Per-unit measures legitimately omit
# zero-valued keys (serde skips defaults; 3,005/3,914 units omit
# matched_functions), so ONLY the whole-binary verdict is strict.
REQUIRED_MEASURE_KEYS = (
    "matched_functions",
    "masked_equal_functions",
    "matched_code",
    "total_code",
    "matched_code_percent",
    "fuzzy_match_percent",
    "total_functions",
)


class Refusal(Exception):
    def __init__(self, stage, reason):
        super().__init__(f"[{stage}] {reason}")
        self.stage = stage
        self.reason = reason


def run(cmd, cwd, log_path=None, check=True, env=None):
    """Run cmd, capturing combined output to log_path (if given)."""
    p = subprocess.run(
        cmd, cwd=str(cwd), stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, env=env,
    )
    out = p.stdout or ""
    if log_path:
        Path(log_path).write_text(out)
    if check and p.returncode != 0:
        tail = "\n".join(out.splitlines()[-30:])
        raise Refusal("subprocess",
                      f"command failed rc={p.returncode}: "
                      f"{' '.join(map(str, cmd))}\n--- tail ---\n{tail}")
    return p.returncode, out


def git(wt, *args, check=True, log_path=None):
    return run(["git", "-C", str(wt), *args], cwd=wt, check=check,
               log_path=log_path)


def count_lines(log_text):
    """Classify ninja status lines. Returns dict with msvc/split/patch/other
    work counts and the renamer 'files patched' figure if present."""
    msvc = split = patch = other = 0
    renamer_patched = None
    in_renamer_step = False
    for line in log_text.splitlines():
        if RENAMER_STEP_RE.match(line):
            in_renamer_step = True
        elif WORKLINE_RE.match(line):
            # any other ninja step ends the renamer's output region
            in_renamer_step = False
        m = WORKLINE_RE.match(line)
        if m:
            desc = m.group(1)
            if desc == "MSVC":
                msvc += 1
            elif desc == "SPLIT":
                split += 1
            elif desc == "PATCH":
                patch += 1
            elif desc in NON_WORK_DESCS:
                pass
            else:
                other += 1
        rm = RENAMER_RE.search(line)
        if rm and in_renamer_step:
            renamer_patched = int(rm.group(2))
    return {
        "msvc": msvc, "split": split, "patch": patch, "other_work": other,
        "work": msvc + split + patch + other,
        "renamer_patched": renamer_patched,
    }


def read_measures_strict(report_path):
    """Parse whole-binary measures BY EXACT KEY. Missing key => Refusal,
    never a default. (footgun 7: .get('masked_equal', 0) read as data)."""
    try:
        with open(report_path) as f:
            r = json.load(f)
    except (OSError, json.JSONDecodeError) as e:
        raise Refusal("report-read", f"cannot read {report_path}: {e}")
    if "measures" not in r:
        raise Refusal("report-read", f"{report_path} has no 'measures' object")
    m = r["measures"]
    out = {}
    for k in REQUIRED_MEASURE_KEYS:
        if k not in m:
            raise Refusal(
                "report-read",
                f"{report_path} measures is MISSING required key '{k}' — "
                f"refusing to default it to 0. Present keys: {sorted(m.keys())}",
            )
        out[k] = m[k]
    out["matched_code"] = int(out["matched_code"])
    out["total_code"] = int(out["total_code"])
    out["honest"] = out["matched_functions"] - out["masked_equal_functions"]
    # Per-unit table for the advisory breakdown (NOT the verdict; per-unit
    # measures omit zero-valued keys by serde design, so .get is correct
    # here and only here).
    out["_units"] = {
        u["name"]: u.get("measures", {}).get("matched_functions", 0)
        for u in r.get("units", [])
    }
    return out


def classify_patch(numstat_paths):
    kinds = set()
    relevant = []
    for p in numstat_paths:
        if p == SYMBOLS_REL:
            raise Refusal(
                "classify",
                f"patch touches {SYMBOLS_REL}. symbols.txt is derived "
                "split-state drift and must never be part of a measured change.",
            )
        if p in MAP_PATHS:
            kinds.add("map"); relevant.append(p)
        elif p in SPLIT_PATHS:
            kinds.add("splits"); relevant.append(p)
        elif p in CONFIGGEN_PATHS:
            kinds.add("configgen"); relevant.append(p)
        elif p.startswith("src/"):
            kinds.add("source"); relevant.append(p)
    return kinds, relevant


class ABMeasure:
    def __init__(self, wt, rundir, jobs, max_settle, verbose_print=print):
        self.wt = Path(wt).resolve()
        self.rundir = Path(rundir)
        self.jobs = jobs
        self.max_settle = max_settle
        self.say = verbose_print
        self.ninja = self.wt / "tools" / "ninja-locked"
        self.evidence = {}   # verification evidence, goes into result.json
        self.legs = {}       # only legs actually measured appear here

    # ---------- helpers ----------
    def _ninja(self, targets, log_name):
        cmd = [str(self.ninja)]
        if self.jobs:
            cmd += ["-j", str(self.jobs)]
        cmd += targets
        log = self.rundir / log_name
        t0 = time.time()
        rc, out = run(cmd, cwd=self.wt, log_path=log, check=False)
        dt = time.time() - t0
        counts = count_lines(out)
        if rc != 0:
            tail = "\n".join(out.splitlines()[-30:])
            raise Refusal("build",
                          f"ninja failed rc={rc} (log: {log})\n--- tail ---\n{tail}")
        self.say(f"    build {log_name}: {dt:.1f}s  msvc={counts['msvc']} "
                 f"split={counts['split']} patch={counts['patch']} "
                 f"other={counts['other_work']}")
        return counts

    def restore_symbols(self):
        """Restore the committed symbols.txt. ⚠ Only ever call this
        IMMEDIATELY BEFORE a split-forcing build — never after one, and never
        inside the settle loop (see settle()'s docstring: it is a discovered
        dep of the SPLIT edge, so restoring it after a split re-dirties the
        graph and settling becomes impossible)."""
        git(self.wt, "checkout", "--", SYMBOLS_REL)

    def symbols_drift(self):
        """Line-delta of post-split symbols.txt drift vs the committed file.
        Reported as evidence, NOT treated as dirt."""
        _, out = git(self.wt, "diff", "--numstat", "--", SYMBOLS_REL)
        for line in out.splitlines():
            f = line.split("\t")
            if len(f) >= 3:
                return {"insertions": int(f[0]), "deletions": int(f[1])}
        return None

    def tool_freshness(self, allow_stale=False):
        """Gather the git facts for check_tool_freshness and record them as
        evidence. I/O only — the decision lives in the pure function."""
        me = Path(__file__).resolve()
        _, running = git(self.wt, "hash-object", str(me))
        running = running.strip()
        rc, head = git(self.wt, "rev-parse", f"HEAD:{TOOL_REL}", check=False)
        if rc != 0:                      # tool not tracked at HEAD: nothing to compare
            self.evidence["tool_freshness"] = {"state": "untracked_at_head"}
            return
        head = head.strip()
        history = {}
        _, commits = git(self.wt, "log", "--format=%H", "-n", "80", "HEAD",
                         "--", TOOL_REL)
        for c in commits.split():
            rc2, b = git(self.wt, "rev-parse", f"{c}:{TOOL_REL}", check=False)
            if rc2 == 0:
                b = b.strip()
                if b != head:
                    history.setdefault(b, c)
        info = check_tool_freshness(running, head, history,
                                    allow_stale=allow_stale)
        info["history_versions"] = len(history)
        self.evidence["tool_freshness"] = info
        self.say(f"  [preflight] tool freshness: {info['state']} "
                 f"(running blob {running[:12]}, HEAD blob {head[:12]}, "
                 f"{len(history)} superseded version(s) known)")

    def wipe_report(self):
        for rel in (REPORT_REL, CACHE_REL):
            p = self.wt / rel
            if p.exists():
                p.unlink()

    # ---------- protocol stages ----------
    def preflight(self, allow_dirty=False, allow_stale_tool=False):
        if not self.wt.is_dir():
            raise Refusal("preflight",
                          f"worktree {self.wt} does not exist (create with "
                          "scripts/setup_worktree.sh <path> <branch>)")
        _, gd = git(self.wt, "rev-parse", "--git-dir")
        _, gcd = git(self.wt, "rev-parse", "--git-common-dir")
        gd_p = (self.wt / gd.strip()).resolve()
        gcd_p = (self.wt / gcd.strip()).resolve()
        if gd_p == gcd_p:
            raise Refusal(
                "preflight",
                f"{self.wt} is the MAIN repo, not a linked worktree. Measuring "
                "in shared main is forbidden (concurrent agents; settle/apply "
                "steps would trample their work). Use scripts/setup_worktree.sh.")
        if not (self.wt / "build.ninja").is_file():
            raise Refusal(
                "preflight",
                f"{self.wt} has no build.ninja — not a buildable worktree. Use "
                "scripts/setup_worktree.sh (a bare `git worktree add` is "
                "unbuildable here).")
        # Is the RUNNING script itself superseded? Checked BEFORE any build,
        # so a stale runner costs seconds instead of a hand-rolled protocol.
        self.tool_freshness(allow_stale=allow_stale_tool)
        # symbols.txt drift is expected and auto-restored; anything else dirty
        # makes the A leg unattributable.
        self.restore_symbols()
        _, status = git(self.wt, "status", "--porcelain")
        dirty, untracked = [], []
        for line in status.splitlines():
            st, path = line[:2], line[3:]
            if st == "??":
                if path.startswith(BUILD_RELEVANT_DIRS):
                    untracked.append(path)
            else:
                dirty.append(path)
        if untracked:
            raise Refusal(
                "preflight",
                f"untracked files in build-relevant dirs: {untracked[:10]} — "
                "commit or remove them; they can change build output invisibly.")
        if dirty and not allow_dirty:
            raise Refusal(
                "preflight",
                f"worktree has modified tracked files: {dirty[:10]} — leg A "
                "would be unattributable. Commit them, or use --from-dirty to "
                "measure exactly those changes.")
        self.evidence["worktree"] = str(self.wt)
        self.evidence["head"] = git(self.wt, "rev-parse", "HEAD")[1].strip()
        return dirty

    def settle(self, presplit=False):
        """Build until a build does zero work. The zero-work build IS the
        proof leg A is settled. Bounded; refuses if never quiescent.

        ⚠ symbols.txt is restored ONCE, HERE, BEFORE the loop — and NEVER
        inside it. `config/<title>/symbols.txt` is a *discovered* dependency
        of the SPLIT edge: build.ninja declares `depfile = $out_dir/dep`, and
        `build/<title>/dep` reads

            build/45410914/config.json: \\
              orig/45410914/default.xex \\
              config/45410914/splits.txt \\
              config/45410914/symbols.txt

        (which is why no *static* edge mentions symbols.txt — grepping
        build.ninja for it finds nothing and proves nothing). dtk REWRITES
        that same file as a side effect of splitting. So `git checkout --`
        on it inside the loop re-dirties the very input whose consumption
        produced the drift: the restored file becomes newer than
        build/<title>/config.json, SPLIT re-runs, dtk drifts it again, we
        restore again — the loop CANNOT converge whenever the committed
        symbols.txt is not already the split's fixed point.

        MEASURED at 23ad2f92 (lane CK-2), in a fresh worktree with NO source
        change at all: a forced re-split drifts symbols.txt by 7 insertions /
        16 deletions, and then

          with the in-loop restore:  build 1 = 382 work edges, builds 2,3,4 =
                                     2 work edges EACH, forever => REFUSAL
          without it:                the very next build = 0 work edges

        Both splits lanes of wave CJ were blocked by exactly this, and it is
        data-dependent (a wave whose drift happened to be nil converged,
        because `git checkout --` skips a write when the file already matches
        the index, so no mtime bump).

        ⇒ Post-split drift is EXPECTED STATE, not dirt. The guarantee the
        restore protects is real but POSITIONAL: symbols.txt drift breaks a
        split ("ends within symbol"), so every *split-forcing* build must be
        preceded by a restore. Those restores still happen, in both the
        places that force a split — here (pre-loop, incl. the presplit
        branch) and in apply_patch() for leg B — so BOTH LEGS STILL SEE
        IDENTICAL symbols.txt HANDLING: each leg starts from the committed
        file, forces its split, and is measured at the resulting fixed point.
        """
        self.restore_symbols()
        if presplit:
            # splits/map patch: leg A must be measured in freshly-split state
            # so both legs share build state (footgun 8).
            self.say("  [settle] forcing re-split for leg A (splits/map patch "
                     "=> both legs must be measured in freshly-split state)")
            # symbols.txt was already restored just above, unconditionally.
            (self.wt / STAMP_REL).unlink(missing_ok=True)
            (self.wt / CONFIG_YML_REL).touch()
        for attempt in range(1, self.max_settle + 1):
            counts = self._ninja([], f"settle_{attempt}.log")
            if counts["work"] == 0:
                self.evidence["settle_builds"] = attempt
                self.evidence["settle_symbols_drift"] = self.symbols_drift()
                self.say(f"  [settle] quiescent after {attempt} build(s); the "
                         "reading of every pre-quiescent build is DISCARDED")
                if self.evidence["settle_symbols_drift"]:
                    self.say("  [settle] symbols.txt is drifted at the settle "
                             "point — EXPECTED (it is a split OUTPUT as well as "
                             "a discovered dep). NOT restored: restoring here is "
                             "what made this loop non-convergent for wave CJ.")
                return
            self.say(f"  [settle] build {attempt} did work "
                     f"(msvc={counts['msvc']} split={counts['split']} "
                     f"patch={counts['patch']} other={counts['other_work']}) — "
                     "its reading is discarded; retrying")
            # ⚠ DO NOT restore symbols.txt here. See the docstring: it is a
            # discovered dep of the SPLIT edge AND a split output, so
            # restoring re-dirties the graph and the loop cannot converge.
        raise Refusal(
            "settle",
            f"could not reach a zero-work build in {self.max_settle} attempts. "
            "Something keeps the graph dirty (future mtimes? concurrent writer? "
            f"perpetually-dirty edge?). Logs: {self.rundir}/settle_*.log. "
            "REFUSING to measure — a leg A with nonzero recompiles carries "
            "~+193 matched / +0.51pp settling noise. NOTE: if every retry "
            "shows exactly split=1 patch=1, suspect something restoring "
            "config/<title>/symbols.txt between builds — that is the wave-CJ "
            "non-convergence and it is a defect in the RESTORER, not the "
            "graph. ⚠ FIRST SUSPECT, measured (lane CL-4): a STALE COPY OF "
            "THIS SCRIPT. The wave-CJ bug was fixed in 4be4bcdc, but lane "
            "CK-3's worktrees carried the pre-fix file and were refused here "
            "— on PLAIN SOURCE patches — 24 minutes after the fix landed. "
            "The preflight tool-freshness check now catches that; if it "
            "reported 'not_in_history' you may be running a superseded "
            "version from OUTSIDE this worktree's history.",
        )

    def read_leg(self, name):
        self.wipe_report()
        counts = self._ninja([REPORT_REL], f"leg{name}_report.log")
        if counts["work"] != 0:
            raise Refusal(
                f"leg{name}-read",
                f"report regeneration performed build work (msvc={counts['msvc']} "
                f"split={counts['split']} patch={counts['patch']} "
                f"other={counts['other_work']}) — the leg was not actually "
                "settled when read; recompile counts are untrustworthy.",
            )
        m = read_measures_strict(self.wt / REPORT_REL)
        # ARCHIVE the leg's report.json. Without this only the ninja LOGS
        # survive a run, so post-hoc attribution (which unit? which function?)
        # is impossible once the worktree moves on — and the worktree always
        # moves on. gzip: the raw report is ~14 MB/leg.
        arch = self.rundir / f"leg{name}_report.json.gz"
        with open(self.wt / REPORT_REL, "rb") as fi, gzip.open(arch, "wb") as fo:
            shutil.copyfileobj(fi, fo)
        self.evidence.setdefault("archived_reports", {})[name] = str(arch)
        self.legs[name] = m
        self.say(f"  [leg {name}] archived report -> {arch.name} "
                 f"({arch.stat().st_size / 1e6:.1f} MB gz)")
        self.say(f"  [leg {name}] matched={m['matched_functions']} "
                 f"masked_equal={m['masked_equal_functions']} honest={m['honest']} "
                 f"code%={m['matched_code_percent']:.6f}")
        return m

    def gen_name_check(self, name, objdiff_bin):
        out = self.rundir / f"leg{name}_name_check.json"
        cmd = [objdiff_bin, "report", "generate",
               "-c", "functionRelocDiffs=name_check", "-o", str(out)]
        run(cmd, cwd=self.wt, log_path=self.rundir / f"leg{name}_nc.log")
        m = read_measures_strict(out)
        self.legs[f"{name}_nc"] = m
        self.say(f"  [leg {name} name_check ruler] matched={m['matched_functions']} "
                 f"code%={m['matched_code_percent']:.6f}")
        return m

    def apply_patch(self, patch_path, kinds):
        rc, out = git(self.wt, "apply", "--check", str(patch_path), check=False)
        if rc != 0:
            raise Refusal("apply", f"patch does not apply cleanly:\n{out}")
        git(self.wt, "apply", str(patch_path))
        _, status = git(self.wt, "status", "--porcelain")
        changed = [l[3:] for l in status.splitlines() if not l.startswith("??")]
        changed_non_symbols = [p for p in changed if p != SYMBOLS_REL]
        if not changed_non_symbols:
            raise Refusal("apply",
                          "patch applied but git sees NO modified tracked files "
                          "— absent-vs-absent leg; refusing.")
        self.evidence["applied_files"] = changed_non_symbols
        self.say(f"  [apply] patch applied; modified: {changed_non_symbols}")
        # Force what the change kind needs (footgun 2: map edit inert
        # without a re-split).
        if kinds & {"map", "splits"}:
            self.say("  [force] map/splits change: restore symbols.txt, rm "
                     "renamer stamp, touch config.yml (forces re-split; without "
                     "it the leg is INERT: '[APPLIED] ... 0 files patched')")
            self.restore_symbols()
            (self.wt / STAMP_REL).unlink(missing_ok=True)
            (self.wt / CONFIG_YML_REL).touch()
        if "configgen" in kinds:
            self.say("  [force] configgen change: re-running configure.py")
            run([sys.executable, "configure.py"], cwd=self.wt,
                log_path=self.rundir / "configure_B.log")

    def leg_b_build(self, kinds):
        counts = self._ninja([], "legB_build.log")
        # The recompile count for leg B is taken HERE, from this log,
        # before any report read (footgun 3).
        self.evidence["legB_counts"] = counts
        check_legb_counts(kinds, counts)
        return counts


def check_legb_counts(kinds, counts):
    """Pure leg-B application assertions. Extracted from leg_b_build so the
    selftest can DRIVE EVERY REFUSAL BRANCH without a build — in particular
    the splits branch, which was the least-tested path in the tool and is the
    one that broke for wave CJ. Raises Refusal; returns None on success."""
    if "source" in kinds and counts["msvc"] == 0:
        raise Refusal(
            "legB-build",
            "source patch but ZERO MSVC recompiles in leg B build — the "
            "change cannot have been compiled (absent-vs-absent A/B). Two "
            "lanes hit this via run_objdiff hiding the compile; here the "
            "build log itself shows no compile, so the patch touched "
            "nothing the build consumes.",
        )
    if kinds & {"map", "splits"} and counts["split"] == 0:
        raise Refusal(
            "legB-build",
            "map/splits patch but the SPLIT step did not run in leg B — "
            "the target objs are unchanged and the A/B is measuring nothing.",
        )
    if "map" in kinds:
        rp = counts["renamer_patched"]
        if rp is None:
            raise Refusal(
                "legB-build",
                "map patch but the renamer's '[APPLIED] N files checked, "
                "M files patched' line is absent from the leg B build log — "
                "cannot prove the map was applied.")
        if rp == 0:
            raise Refusal(
                "legB-build",
                "map patch but renamer reports '0 files patched' — the edit "
                "was INERT (lane CF-1's exact failure). A forced re-split "
                "should have prevented this; investigate before trusting "
                "any number from this worktree.")


def check_tool_freshness(running_blob, head_blob, history, allow_stale=False):
    """Pure decision: is the RUNNING ab_measure.py a STALE COMMITTED version
    of itself relative to the worktree's HEAD? Extracted (like
    check_legb_counts) so the selftest can drive EVERY branch with no git and
    no build.

    `history` maps blob-id -> commit for older committed versions of
    TOOL_REL reachable from the worktree's HEAD, EXCLUDING HEAD's own blob.

    WHY THIS EXISTS (lane CL-4, 2026-08-02). Lane CK-3 reported that the
    settle non-convergence "ALSO HITS PLAIN SOURCE PATCHES", implying a
    defect BROADER than the one lane CK-2 fixed in `4be4bcdc`. It is not
    broader — it is the SAME defect, observed through worktrees that never
    received the fix. Measured:

      * all four CK-3 worktrees carried tools/ab_measure.py sha1
        c807a506b340 — the PRE-fix file, whose settle() has an in-loop
        restore_symbols();
      * their base 23ad2f92 (00:44:46) predates the fix 4be4bcdc (01:08:58)
        by 24 minutes;
      * POSITIVE CONTROL: running that exact stale file against a plain
        SOURCE patch (PresenceMgr, kinds=['source']) in a split-forced
        worktree at 70a6266c REFUSES with settle builds 1-4 = split=1
        patch=1 forever, while the fixed file settles in 2 and measures
        Δmatched=+1. Same worktree, same patch, same HEAD — the tool
        version is the ONLY variable.

    The in-loop restore was never kind-conditional, so "plain source patches
    too" was always implied by the one root cause; nothing extra to fix in
    the loop. What DID cost three sub-lanes their budget (they hand-rolled
    the protocol instead) is that NOTHING TOLD THEM their runner was stale.
    That is this guard.

    Deliberately NOT a refusal: an UNCOMMITTED local edit (a lane improving
    the tool — including this one) and a version committed on some other
    branch. Neither is in `history`, so both report and continue. Only a
    provably-superseded committed version refuses.
    """
    if running_blob == head_blob:
        return {"state": "matches_head", "blob": running_blob}
    if running_blob in history:
        commit = history[running_blob]
        if allow_stale:
            return {"state": "stale_override", "blob": running_blob,
                    "superseded_by_head": head_blob, "running_commit": commit}
        raise Refusal(
            "tool-version",
            f"the RUNNING {TOOL_REL} is a STALE COMMITTED version: its blob "
            f"{running_blob[:12]} is the file as of {commit[:12]}, which the "
            f"worktree's HEAD has already SUPERSEDED (HEAD blob "
            f"{head_blob[:12]}). Re-run with the tool from the worktree "
            f"itself, or `git -C <wt> log -p -- {TOOL_REL}` to see what you "
            "are missing. This is exactly how lane CK-3's three porting "
            "sub-lanes were refused by the wave-CJ settle bug 24 minutes "
            "AFTER it had been fixed — the fix was in the repo and not in "
            "their hands, and the refusal text pointed at the build graph, "
            "so all three hand-rolled the protocol instead. "
            "--allow-stale-tool to override deliberately.",
        )
    return {"state": "not_in_history", "blob": running_blob,
            "head_blob": head_blob}


def unit_breakdown(a_units, b_units, limit=15):
    """Top-N regressed/improved units PLUS the metadata needed to know the
    list is partial.

    Returns (regs, imps, meta). `regs`/`imps` are truncated to `limit`, but
    every count and sum in `meta` is computed over the FULL populations —
    truncating the sums too would reproduce exactly the defect this exists
    to expose (lane CG-1: listed regressions summed to -18 against a -19
    delta because `system/rndobj/PostProcMgr` was cut off the list, with no
    flag). Print the denominator next to the list.
    """
    regs, imps = [], []
    for u in sorted(set(a_units) | set(b_units)):
        d = b_units.get(u, 0) - a_units.get(u, 0)
        if d < 0:
            regs.append({"unit": u, "delta": d,
                         "from": a_units.get(u, 0), "to": b_units.get(u, 0)})
        elif d > 0:
            imps.append({"unit": u, "delta": d,
                         "from": a_units.get(u, 0), "to": b_units.get(u, 0)})
    regs.sort(key=lambda x: x["delta"])
    imps.sort(key=lambda x: -x["delta"])
    sum_reg = sum(r["delta"] for r in regs)      # FULL population, not regs[:limit]
    sum_imp = sum(i["delta"] for i in imps)
    meta = {
        "limit": limit,
        "n_units_regressed": len(regs),
        "n_units_improved": len(imps),
        "sum_regressed": sum_reg,
        "sum_improved": sum_imp,
        "net_unit_delta": sum_reg + sum_imp,
        "regressions_truncated": len(regs) > limit,
        "improvements_truncated": len(imps) > limit,
        "regressions_hidden": max(0, len(regs) - limit),
        "improvements_hidden": max(0, len(imps) - limit),
        # FULL populations. The DISPLAY is truncated to `limit` (a 3,914-unit
        # list is unreadable), but result.json is the machine-readable
        # artifact and must not lose attribution: with only the top-15 rows
        # archived, "which unit lost those 4 matches?" is unanswerable once
        # the worktree moves on. Consumers wanting the short list should read
        # unit_regressions_shown / unit_improvements_shown.
        "full_regressions": regs,
        "full_improvements": imps,
    }
    return regs[:limit], imps[:limit], meta


def compute_delta(a, b):
    """B-minus-A for every headline measure. Split out of main() so the
    selftest can assert the delta SET (notably fuzzy_match_percent, which
    used to be computed nowhere and therefore could not reach the headline).
    """
    return {
        "matched_functions": b["matched_functions"] - a["matched_functions"],
        "masked_equal_functions":
            b["masked_equal_functions"] - a["masked_equal_functions"],
        "honest": b["honest"] - a["honest"],
        "matched_code_percent":
            b["matched_code_percent"] - a["matched_code_percent"],
        "matched_code_bytes": b["matched_code"] - a["matched_code"],
        "fuzzy_match_percent":
            b["fuzzy_match_percent"] - a["fuzzy_match_percent"],
    }


def leg_public(m):
    """The publishable slice of a measured leg (drops _units)."""
    return {k: v for k, v in m.items() if not k.startswith("_")}


def find_objdiff(wt):
    # Same binary the build's report rule uses, parsed from build.ninja so
    # we cannot drift from the build.
    bn = (wt / "build.ninja").read_text()
    m = re.search(r"(\S*/objdiff/target/release/objdiff-cli)", bn)
    if not m:
        raise Refusal("name-check",
                      "cannot resolve objdiff-cli path from build.ninja")
    return m.group(1)


def ruler_identity(wt):
    """Content identity of the objdiff binary = the RULER both legs are scored on.

    Deltas compose only when both legs were measured with the SAME ruler. The
    binary is NOT a ninja input (see CLAUDE.md), so swapping it triggers no
    rebuild and no warning: a swap between leg A and leg B silently reprices the
    delta and nothing in the run notices. That is not hypothetical -- lane CZ-4
    widened `masked_equal_functions` by +21,502 with ZERO change to any score, so
    a mid-run swap of that binary alone would manufacture a Δhonest of -21,502
    out of an unchanged tree.

    Returns a dict that is compared for equality across legs. When the path
    cannot be resolved the result is explicitly marked unresolved rather than
    omitted -- an unverifiable guard must announce itself, not pass quietly.
    """
    try:
        path = find_objdiff(wt)
    except Refusal:
        return {"resolved": False, "reason": "path not in build.ninja"}
    try:
        st = os.stat(path)
        h = hashlib.sha256(Path(path).read_bytes()).hexdigest()[:16]
    except OSError as e:
        return {"resolved": False, "reason": f"stat/read failed: {e}"}
    return {"resolved": True, "path": path, "size": st.st_size, "sha256_16": h}


def check_ruler_stable(a, b):
    """REFUSE if the objdiff binary changed between the two legs.

    ⚠ Compare CONTENT (sha256+size), never the whole dict. `path` is not part of
    the ruler's identity: `setup_worktree.sh` bakes an absolute objdiff path into
    build.ninja, and a `configure.py` re-run -- which any configgen-class patch
    forces -- re-resolves it through the `~/tmp` symlink to THE SAME FILE. A
    whole-dict comparison therefore refused with "binary CHANGED" printed beside
    two IDENTICAL hashes, on every configgen patch in a `~/tmp` worktree, i.e.
    the location CLAUDE.md mandates. Fail-safe in direction, but it withheld
    valid verdicts and cost lane DJ-2 a re-run to work around (2026-08-03).

    A differing path with identical content is the SAME RULER reached by another
    route -- reported, not refused. Only a content change is a swap.
    """
    if not a.get("resolved") or not b.get("resolved"):
        return "UNVERIFIED (%s)" % (a.get("reason") or b.get("reason") or "unknown")
    if (a["sha256_16"], a["size"]) != (b["sha256_16"], b["size"]):
        raise Refusal(
            "ruler",
            "the objdiff-cli binary CHANGED between leg A and leg B "
            f"(A sha256:{a['sha256_16']} size={a['size']}, "
            f"B sha256:{b['sha256_16']} size={b['size']}). The two legs were "
            "scored on DIFFERENT rulers, so the delta is meaningless. Re-run "
            "the whole A/B on one binary.")
    if a.get("path") != b.get("path"):
        return ("stable (sha256:%s) -- NOTE: resolved via a different path in "
                "leg B (%s -> %s); content identical, so this is the same ruler "
                "reached by another route, not a swap."
                % (a["sha256_16"], a.get("path"), b.get("path")))
    return "stable (sha256:%s)" % a["sha256_16"]


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--worktree", help="linked worktree to measure in (NEVER main)")
    src = ap.add_mutually_exclusive_group()
    src.add_argument("--patch", help="diff file to measure")
    src.add_argument("--pick", metavar="REF", help="measure applying commit REF")
    src.add_argument("--revert", metavar="REF", help="measure reverting commit REF")
    src.add_argument("--from-dirty", action="store_true",
                     help="snapshot the worktree's uncommitted tracked diff as "
                          "the patch, reset to HEAD for leg A, re-apply for leg B")
    ap.add_argument("--name-check", action="store_true",
                    help="ALSO score both legs on the opt-in name_check ruler "
                         "(aggregate code%% is build-unstable ~0.05pp; small nc "
                         "deltas mean nothing)")
    ap.add_argument("--allow-inert", action="store_true",
                    help="proceed even if the patch touches no build-relevant path")
    ap.add_argument("--restore", action="store_true",
                    help="revert the patch from the worktree after measuring")
    ap.add_argument("--allow-stale-tool", action="store_true",
                    help="run even if this script is a superseded committed "
                         "version of itself (lane CK-3's silent failure mode)")
    ap.add_argument("--max-settle", type=int, default=4)
    ap.add_argument("--jobs", type=int,
                    default=int(os.environ.get("AB_NINJA_JOBS", "12")),
                    help="ninja -j (default 12; 0 = ninja default)")
    ap.add_argument("--run-root", default=os.path.expanduser("~/tmp/ab_measure"))
    ap.add_argument("--label", default=None)
    ap.add_argument("--selftest", action="store_true",
                    help="run no-build sanity checks of the refusal logic and exit")
    args = ap.parse_args()

    if args.selftest:
        sys.exit(selftest())

    if not args.worktree or not (args.patch or args.pick or args.revert
                                 or args.from_dirty):
        ap.error("--worktree and one of --patch/--pick/--revert/--from-dirty "
                 "are required")

    label = args.label or (Path(args.patch).stem if args.patch else
                           (args.pick or args.revert or "from-dirty")
                           .replace("/", "_")[:24])
    rundir = Path(args.run_root) / f"{time.strftime('%Y%m%d-%H%M%S')}-{label}"
    rundir.mkdir(parents=True, exist_ok=True)
    result_path = rundir / "result.json"
    print(f"ab_measure: run dir {rundir}")

    ab = ABMeasure(args.worktree, rundir, args.jobs, args.max_settle)
    status = {"status": "refused", "stage": None, "reason": None}
    try:
        dirty = ab.preflight(allow_dirty=args.from_dirty,
                             allow_stale_tool=args.allow_stale_tool)

        # --- obtain the patch ---
        patch_path = rundir / "patch.diff"
        if args.patch:
            shutil.copy(args.patch, patch_path)
        elif args.pick:
            _, d = git(ab.wt, "diff", f"{args.pick}^", args.pick)
            patch_path.write_text(d)
        elif args.revert:
            _, d = git(ab.wt, "diff", args.revert, f"{args.revert}^")
            patch_path.write_text(d)
        elif args.from_dirty:
            _, d = git(ab.wt, "diff")
            patch_path.write_text(d)
            if not d.strip():
                raise Refusal("patch",
                              "--from-dirty but the worktree has no tracked diff")
            print(f"  [from-dirty] snapshot saved to {patch_path} — resetting "
                  f"the {len(dirty)} dirty file(s) to HEAD for leg A")
            git(ab.wt, "checkout", "--", *dirty)
        if not patch_path.read_text().strip():
            raise Refusal("patch", "empty patch — nothing to measure")
        sha = hashlib.sha256(patch_path.read_bytes()).hexdigest()[:16]
        print(f"  [patch] {patch_path} sha256/16={sha}")

        # --- classify ---
        _, numstat = git(ab.wt, "apply", "--numstat", str(patch_path))
        paths = [l.split("\t")[2] for l in numstat.splitlines() if l.strip()]
        kinds, relevant = classify_patch(paths)
        print(f"  [classify] paths={paths} kinds={sorted(kinds) or ['NONE']}")
        if not kinds and not args.allow_inert:
            raise Refusal(
                "classify",
                "patch touches no build-relevant path (src/, map, splits, "
                "configgen) — this A/B would measure nothing. --allow-inert "
                "to override.")
        expect = {
            "source": "expect MSVC recompiles in leg B",
            "map": "will force re-split; renamer must report >0 files patched",
            "splits": "will force re-split; BOTH legs measured in fresh-split state",
            "configgen": "will rerun configure.py after apply",
        }
        for k in sorted(kinds):
            print(f"  [classify] {k}: {expect[k]}")

        # --- settle + leg A ---
        ab.settle(presplit=bool(kinds & {"map", "splits"}))
        # Pin the RULER before leg A is read, re-check after leg B (below).
        ruler_a = ruler_identity(ab.wt)
        a = ab.read_leg("A")
        objdiff_bin = None
        if args.name_check:
            objdiff_bin = find_objdiff(ab.wt)
            ab.gen_name_check("A", objdiff_bin)

        # --- apply + leg B ---
        ab.apply_patch(patch_path, kinds)
        legb_counts = ab.leg_b_build(kinds)
        b = ab.read_leg("B")
        if args.name_check:
            ab.gen_name_check("B", objdiff_bin)

        # Same ruler for both legs, or the delta is not a delta.
        ruler_state = check_ruler_stable(ruler_a, ruler_identity(ab.wt))
        print(f"  [ruler] objdiff-cli {ruler_state}")

        # --- verdict (only reachable if every stage above passed) ---
        regs, imps, ubmeta = unit_breakdown(a["_units"], b["_units"])
        delta = compute_delta(a, b)
        status = {
            "status": "measured",
            "patch_sha256_16": sha,
            "kinds": sorted(kinds),
            # Which ruler produced these numbers. Archived so a later reader can
            # tell whether two runs are comparable at all.
            "ruler": {"objdiff": ruler_a, "state": ruler_state},
            "evidence": ab.evidence,
            "legA": leg_public(a),
            "legB": leg_public(b),
            # leg A recompiles are 0 by construction: read_leg refuses on any
            # build work, and settle refuses if quiescence is unreachable.
            "legA_recompiles": 0,
            "legB_recompiles": legb_counts["msvc"],
            "delta": delta,
            # FULL lists here — the display below is what gets truncated.
            "unit_regressions": ubmeta["full_regressions"],
            "unit_improvements": ubmeta["full_improvements"],
            "unit_regressions_shown": regs,
            "unit_improvements_shown": imps,
            "unit_breakdown_meta": {k: v for k, v in ubmeta.items()
                                    if not k.startswith("full_")},
        }
        if args.name_check:
            anc, bnc = ab.legs["A_nc"], ab.legs["B_nc"]
            status["name_check"] = {
                "legA": leg_public(anc), "legB": leg_public(bnc),
                "delta_matched":
                    bnc["matched_functions"] - anc["matched_functions"],
                "delta_code_percent":
                    bnc["matched_code_percent"] - anc["matched_code_percent"],
                "warning": "name_check aggregate code% is build-unstable "
                           "(~0.05pp); small nc deltas mean nothing",
            }

        print("\n================ A/B RESULT (MEASURED) ================")
        print(f"  patch: {label} ({sha})  kinds: {sorted(kinds)}")
        print(f"  leg A: matched={a['matched_functions']} "
              f"masked={a['masked_equal_functions']} honest={a['honest']} "
              f"code%={a['matched_code_percent']:.6f}  (recompiles: 0, settled)")
        print(f"  leg B: matched={b['matched_functions']} "
              f"masked={b['masked_equal_functions']} honest={b['honest']} "
              f"code%={b['matched_code_percent']:.6f}  "
              f"(recompiles: {legb_counts['msvc']}, split={legb_counts['split']}, "
              f"patch_steps={legb_counts['patch']})")
        print(f"  Δmatched={delta['matched_functions']:+d}  "
              f"Δmasked_equal={delta['masked_equal_functions']:+d}  "
              f"Δhonest={delta['honest']:+d}  "
              f"Δcode%={delta['matched_code_percent']:+.6f}pp  "
              f"Δcode_bytes={delta['matched_code_bytes']:+d}")
        # Δfuzzy belongs in the HEADLINE: for map/correctness lanes it is the
        # only witness a pure permutation ever moves (Δmatched is 0 by design).
        print(f"  Δfuzzy={delta['fuzzy_match_percent']:+.6f}pp   "
              f"(legA {a['fuzzy_match_percent']:.6f} -> "
              f"legB {b['fuzzy_match_percent']:.6f})")

        def _unit_list(title, rows, n_total, s_total, hidden):
            if not n_total:
                return
            flag = (f"   ⚠ TRUNCATED: showing {len(rows)} of {n_total}, "
                    f"{hidden} NOT SHOWN (sum below is over ALL {n_total})"
                    if hidden else "")
            print(f"  {title}: {n_total} unit(s), sum {s_total:+d}{flag}")
            for r in rows:
                print(f"    {r['delta']:+4d}  {r['unit']}  "
                      f"({r['from']}->{r['to']})")

        _unit_list("unit improvements", imps, ubmeta["n_units_improved"],
                   ubmeta["sum_improved"], ubmeta["improvements_hidden"])
        _unit_list("unit REGRESSIONS", regs, ubmeta["n_units_regressed"],
                   ubmeta["sum_regressed"], ubmeta["regressions_hidden"])
        if ubmeta["n_units_improved"] or ubmeta["n_units_regressed"]:
            agree = ubmeta["net_unit_delta"] == delta["matched_functions"]
            note = "" if agree else (
                "   ⚠ DISAGREE — matched functions live outside the unit "
                "table; do not pair losses to gains from this list alone")
            print(f"  unit net (ALL units) = {ubmeta['net_unit_delta']:+d}"
                  f"   vs whole-binary Δmatched = "
                  f"{delta['matched_functions']:+d}{note}")
        if args.name_check:
            nc = status["name_check"]
            print(f"  [opt-in name_check ruler] Δmatched={nc['delta_matched']:+d} "
                  f"Δcode%={nc['delta_code_percent']:+.6f}pp — {nc['warning']}")
        print("========================================================")

        # ⚠ Write the verdict BEFORE the optional restore. --restore is
        # post-verdict CLEANUP and must never be able to void a verdict that
        # was already measured and printed. Measured by lane CK-2: a real
        # config/<title>/splits.txt patch measured cleanly end-to-end
        # (settled in 2, both legs read, Δ0), and then `git apply -R` failed
        # with "patch does not apply" — because splits.txt is itself a SPLIT
        # OUTPUT (jeff rewrites it, dropping comments and re-deriving the
        # .pdata lines), so the file leg B ends on is NOT the file the patch
        # produced. That raised a Refusal AFTER the A/B RESULT block had
        # printed, and the except branch then overwrote result.json with
        # status "refused" — a fully valid measurement, on disk, labelled as
        # having no verdict, and rc=2.
        result_path.write_text(json.dumps(status, indent=1))
        if args.restore:
            rc_r, out_r = git(ab.wt, "apply", "-R", str(patch_path),
                              check=False)
            if rc_r == 0:
                ab.restore_symbols()
                print("  [restore] patch reverted from worktree (build state "
                      "is now leg-B-built with leg-A source; the next run's "
                      "settle phase handles it)")
            else:
                # NOT a refusal: the verdict above stands on its own.
                status["restore"] = {
                    "ok": False,
                    "note": "git apply -R failed; the worktree still carries "
                            "the patch (or a split-rewritten form of it). The "
                            "MEASUREMENT IS UNAFFECTED — it completed before "
                            "this step. Expected for splits.txt patches: "
                            "splits.txt is a split OUTPUT as well as an input.",
                    "git_output": out_r[-2000:],
                }
                result_path.write_text(json.dumps(status, indent=1))
                print("  [restore] ⚠ FAILED to revert the patch — the verdict "
                      "above STANDS (it was measured before this step). Clean "
                      "the worktree by hand before the next run:\n"
                      f"      git -C {ab.wt} checkout -- <paths>\n"
                      "    Expected for splits.txt patches: jeff rewrites "
                      "splits.txt during the split, so the file is no longer "
                      "what the patch produced and `git apply -R` cannot "
                      "reverse it.")
        print(f"ab_measure: result written to {result_path}")
        return 0

    except Refusal as r:
        status.update({"stage": r.stage, "reason": r.reason})
        # A refused run publishes NO deltas and NO verdict. Absolutes for
        # legs that were fully measured before the refusal are kept, clearly
        # namespaced under measured_legs_before_refusal.
        if ab.legs:
            status["measured_legs_before_refusal"] = {
                k: leg_public(v) for k, v in ab.legs.items()
            }
        result_path.write_text(json.dumps(status, indent=1))
        print("\n================ REFUSED — NO VERDICT ================",
              file=sys.stderr)
        print(f"  stage:  {r.stage}", file=sys.stderr)
        print(f"  reason: {r.reason}", file=sys.stderr)
        print("  A refused run reports NO deltas. Fix the precondition and rerun.",
              file=sys.stderr)
        print(f"  Details: {result_path}", file=sys.stderr)
        print("=======================================================",
              file=sys.stderr)
        return 2


# ---------------- selftest (no builds) ----------------

def selftest():
    fails = []

    def check(name, fn, expect_refusal):
        detail = ""
        try:
            fn()
            refused = False
        except Refusal as e:
            refused = True
            detail = str(e)
        if refused == expect_refusal:
            print(f"  PASS  {name}" + (f"  ({detail[:90]})" if refused else ""))
        else:
            fails.append(name)
            print(f"  FAIL  {name}: expected "
                  f"{'refusal' if expect_refusal else 'pass'}, got "
                  f"{'refusal' if refused else 'pass'}")

    import tempfile
    with tempfile.TemporaryDirectory() as td:
        good_measures = {k: 1 for k in REQUIRED_MEASURE_KEYS}
        good_measures["matched_code"] = "10"
        good_measures["total_code"] = "20"
        good = {"measures": good_measures, "units": []}
        p_good = Path(td) / "good.json"
        p_good.write_text(json.dumps(good))
        bad = json.loads(json.dumps(good))
        del bad["measures"]["masked_equal_functions"]
        p_bad = Path(td) / "bad.json"
        p_bad.write_text(json.dumps(bad))
        bad2 = json.loads(json.dumps(good))
        del bad2["measures"]["matched_functions"]
        p_bad2 = Path(td) / "bad2.json"
        p_bad2.write_text(json.dumps(bad2))

        check("strict read accepts complete measures",
              lambda: read_measures_strict(p_good), expect_refusal=False)
        check("strict read REFUSES missing masked_equal_functions",
              lambda: read_measures_strict(p_bad), expect_refusal=True)
        check("strict read REFUSES missing matched_functions",
              lambda: read_measures_strict(p_bad2), expect_refusal=True)
        check("classify REFUSES a symbols.txt patch",
              lambda: classify_patch([SYMBOLS_REL]), expect_refusal=True)

        # --- SAME-RULER guard (lane CZ-4) ----------------------------------
        # The objdiff-cli binary is not a ninja input, so swapping it between
        # legs rebuilds nothing and warns about nothing -- the two legs are
        # simply scored on different rulers and the delta is fiction. CZ-4's
        # own disclosure change is the worst case available: it moves
        # masked_equal_functions by +21,502 with EVERY score key unchanged, so
        # a mid-run swap fabricates Δhonest = -21,502 from an untouched tree.
        #
        # These fixtures are hand-built (no binary needed). The equal case must
        # PASS and the differing case must REFUSE; if the differing case ever
        # passes, the guard has become vacuous.
        r_a = {"resolved": True, "path": "/x/objdiff-cli", "size": 11652856,
               "sha256_16": "aaaaaaaaaaaaaaaa"}
        r_same = dict(r_a)
        r_swapped = dict(r_a, size=11652880, sha256_16="bbbbbbbbbbbbbbbb")
        r_unres = {"resolved": False, "reason": "path not in build.ninja"}
        check("ruler guard PASSES when the objdiff binary is unchanged",
              lambda: check_ruler_stable(r_a, r_same), expect_refusal=False)
        check("ruler guard REFUSES a mid-run objdiff binary swap",
              lambda: check_ruler_stable(r_a, r_swapped), expect_refusal=True)
        # REGRESSION (lane DJ-2, 2026-08-03): the guard used to compare the whole
        # dict, so an IDENTICAL binary resolved through the ~/tmp symlink -- which
        # every configgen-class patch causes -- refused with "binary CHANGED"
        # printed next to two identical hashes. Same content via another path is
        # the SAME RULER and must pass, or the guard withholds valid verdicts in
        # the worktree location CLAUDE.md mandates.
        r_otherpath = dict(r_a, path="/home/free/tmp/objdiff/objdiff-cli")
        check("ruler guard PASSES on identical content reached by a different path",
              lambda: check_ruler_stable(r_a, r_otherpath), expect_refusal=False)
        if "different path" not in check_ruler_stable(r_a, r_otherpath):
            fails.append("ruler guard does not disclose the path change")
            print("  FAIL  ruler guard does not disclose the path change")
        else:
            print("  PASS  ruler guard discloses the path change while passing "
                  "(same ruler, another route -- reported, not refused)")
        # An unresolvable path must not masquerade as a pass: it reports
        # UNVERIFIED, a state distinguishable from "stable" in the log.
        check("ruler guard does not refuse when unresolvable (reports UNVERIFIED)",
              lambda: check_ruler_stable(r_a, r_unres), expect_refusal=False)
        if "UNVERIFIED" not in check_ruler_stable(r_a, r_unres):
            fails.append("ruler guard unresolved state is not labelled UNVERIFIED")
            print("  FAIL  ruler guard unresolved state is not labelled UNVERIFIED")
        else:
            print("  PASS  ruler guard unresolved state is labelled UNVERIFIED "
                  "(a guard that cannot verify must say so, not pass quietly)")
        if check_ruler_stable(r_a, r_same) == check_ruler_stable(r_a, r_unres):
            fails.append("ruler guard pass-states are indistinguishable")
            print("  FAIL  ruler guard pass-states are indistinguishable")
        else:
            print("  PASS  ruler guard pass-states are distinguishable: "
                  f"{check_ruler_stable(r_a, r_same)!r} vs "
                  f"{check_ruler_stable(r_a, r_unres)!r}")

        # --- honest = matched - masked_equal, the HEADLINE quantity --------
        # Lane CL-4 sabotage: flipping this '-' to a '+' in
        # read_measures_strict left the ENTIRE selftest at ALL PASS. The
        # Δfuzzy/compute_delta cases hand-build their dicts and never touch
        # the strict reader, so nothing anywhere asserted the identity that
        # the house honest-floor discipline is quoted from. Fixture uses
        # DISTINCT, NONZERO values on purpose: with masked_equal == 0 (or
        # matched == masked) '+' and '-' agree and the test would be vacuous.
        h = json.loads(json.dumps(good))
        h["measures"]["matched_functions"] = 42963
        h["measures"]["masked_equal_functions"] = 1529
        p_h = Path(td) / "honest.json"
        p_h.write_text(json.dumps(h))
        got = read_measures_strict(p_h)["honest"]
        ok = got == 41434                       # 42963 - 1529, lane CL-4 leg A
        print(("  PASS" if ok else "  FAIL") +
              f"  read_measures_strict computes honest = matched - "
              f"masked_equal: {got} (want 41434; the '+' sabotage gives "
              "44492 and was INERT before this case existed)")
        if not ok:
            fails.append("honest identity")

    kinds, _ = classify_patch(["docs/foo.md"])
    if kinds:
        fails.append("inert classify")
        print("  FAIL  docs-only patch classified as build-relevant")
    else:
        print("  PASS  docs-only patch classifies as inert "
              "(run refuses without --allow-inert)")

    kinds, _ = classify_patch(["scripts/target_symbol_map.json"])
    if kinds == {"map"}:
        print("  PASS  map patch classified as map")
    else:
        fails.append("map classify")
        print(f"  FAIL  map patch classified as {kinds}")

    log = ("[1/3] MSVC build/45410914/src/foo.obj\n"
           "[2/3] SPLIT orig/45410914/default.xex\n"
           "[3/3] PATCH target fn_<addr> -> MSVC mangled names\n"
           "[APPLIED] 13106 files checked, 0 files patched, 0 symbols renamed\n"
           "[1/1] REPORT\n[1/1] PROGRESS\n")
    c = count_lines(log)
    ok = (c["msvc"], c["split"], c["patch"], c["work"],
          c["renamer_patched"]) == (1, 1, 1, 3, 0)
    print(("  PASS" if ok else "  FAIL") +
          f"  log classifier: msvc={c['msvc']} split={c['split']} "
          f"patch={c['patch']} work={c['work']} "
          f"renamer_patched={c['renamer_patched']} "
          "(the CF-1 '0 files patched' line parses to 0 => a map leg B refuses)")
    if not ok:
        fails.append("log classifier")

    # --- Δfuzzy reaches the delta set (lane CG-3's whole result was fuzzy) ---
    fa = {"matched_functions": 10, "masked_equal_functions": 2, "honest": 8,
          "matched_code_percent": 36.916286, "matched_code": 100,
          "fuzzy_match_percent": 44.414110}
    fb = dict(fa, fuzzy_match_percent=44.444948)
    d = compute_delta(fa, fb)
    ok = ("fuzzy_match_percent" in d
          and abs(d["fuzzy_match_percent"] - 0.030838) < 1e-9
          and d["matched_functions"] == 0)
    print(("  PASS" if ok else "  FAIL") +
          f"  compute_delta carries Δfuzzy: Δmatched={d['matched_functions']:+d} "
          f"Δfuzzy={d.get('fuzzy_match_percent')} "
          "(a pure permutation moves fuzzy ONLY — an absent Δfuzzy makes it "
          "read as a null result)")
    if not ok:
        fails.append("delta fuzzy")

    # --- unit_breakdown flags truncation and sums the FULL population ---
    a_u = {f"u{i}": 5 for i in range(20)}
    b_u = {f"u{i}": 4 for i in range(20)}          # 20 units, each -1
    b_u["gain"] = 3; a_u["gain"] = 0               # one +3 improvement
    regs, imps, meta = unit_breakdown(a_u, b_u, limit=15)
    ok = (len(regs) == 15 and meta["n_units_regressed"] == 20
          and meta["sum_regressed"] == -20        # NOT -15 (the truncated sum)
          and meta["regressions_truncated"] is True
          and meta["regressions_hidden"] == 5
          and meta["n_units_improved"] == 1 and meta["sum_improved"] == 3
          and meta["improvements_truncated"] is False
          and meta["net_unit_delta"] == -17)
    print(("  PASS" if ok else "  FAIL") +
          f"  unit_breakdown truncation flag: shown={len(regs)} "
          f"n_regressed={meta['n_units_regressed']} "
          f"sum_regressed={meta['sum_regressed']} "
          f"truncated={meta['regressions_truncated']} "
          f"hidden={meta['regressions_hidden']} "
          "(sum must be over ALL 20, not the 15 shown — lane CG-1's -18-vs-19)")
    if not ok:
        fails.append("unit_breakdown truncation")

    regs, imps, meta = unit_breakdown({"a": 5, "b": 1}, {"a": 4, "b": 1},
                                      limit=15)
    ok = (meta["regressions_truncated"] is False
          and meta["regressions_hidden"] == 0
          and meta["n_units_regressed"] == 1 and meta["sum_regressed"] == -1)
    print(("  PASS" if ok else "  FAIL") +
          f"  unit_breakdown does NOT false-flag a short list: "
          f"truncated={meta['regressions_truncated']} "
          f"hidden={meta['regressions_hidden']} "
          "(the flag must discriminate, not always fire)")
    if not ok:
        fails.append("unit_breakdown no-false-flag")

    # ---- SPLITS KIND (lane CK-2) ------------------------------------------
    # The splits path was the least-tested in this tool and the one that
    # broke: two lanes were real splits field trials and neither left a test.
    for paths, want in ((["config/45410914/splits.txt"], {"splits"}),
                        ([CONFIG_YML_REL], {"splits"}),
                        (["config/45410914/splits.txt", "src/system/os/Foo.cpp"],
                         {"splits", "source"})):
        kinds, _ = classify_patch(paths)
        ok = kinds == want
        print(("  PASS" if ok else "  FAIL") +
              f"  splits classify {paths} -> {sorted(kinds)} (want {sorted(want)})")
        if not ok:
            fails.append(f"splits classify {paths}")

    check("splits patch that also touches symbols.txt REFUSES",
          lambda: classify_patch(["config/45410914/splits.txt", SYMBOLS_REL]),
          expect_refusal=True)

    # leg-B application assertions, every branch, no build required.
    base = {"msvc": 0, "split": 0, "patch": 0, "other_work": 0, "work": 0,
            "renamer_patched": None}
    check("legB splits patch with split=0 REFUSES (target objs unchanged)",
          lambda: check_legb_counts({"splits"}, dict(base)), expect_refusal=True)
    check("legB splits patch with split=1 PASSES",
          lambda: check_legb_counts({"splits"}, dict(base, split=1, work=1)),
          expect_refusal=False)
    check("legB source patch with msvc=0 REFUSES (absent-vs-absent)",
          lambda: check_legb_counts({"source"}, dict(base)), expect_refusal=True)
    check("legB map patch with renamer line ABSENT REFUSES",
          lambda: check_legb_counts({"map"}, dict(base, split=1)),
          expect_refusal=True)
    check("legB map patch with '0 files patched' REFUSES (CF-1 inert edit)",
          lambda: check_legb_counts({"map"}, dict(base, split=1,
                                                 renamer_patched=0)),
          expect_refusal=True)
    check("legB map patch with split=1 and 7 files patched PASSES",
          lambda: check_legb_counts({"map"}, dict(base, split=1,
                                                 renamer_patched=7)),
          expect_refusal=False)

    # ---- renamer figure must come from the RENAMER's step (lane CT-1) ------
    # SIX patchers emit an identical '[APPLIED] N files checked, M files
    # patched' line. Binding by line order took the LAST one. On a real leg-B
    # log that was the EH boundary patcher (51), NOT the renamer (1045) — so
    # this gate had been reading the wrong patcher's number since 5f05def4,
    # passing only because 51 > 0. Both directions are tested: the mirror case
    # (inert renamer masked by a later non-zero) is the dangerous one.
    _RENAMER_STEP = "[1/67] PATCH target fn_<addr> -> MSVC mangled names"
    _EH_STEP = "[63/67] PATCH EH funclet extent boundaries"
    _APPLIED = "[APPLIED] {n} files checked, {m} files patched, 0 total"

    def _rp(*lines):
        """Return renamer_patched for a synthetic log, as a Refusal-or-pass
        so it composes with check()'s contract."""
        return count_lines("\n".join(lines))["renamer_patched"]

    def _expect_rp(log_lines, want):
        got = _rp(*log_lines)
        if got != want:
            raise Refusal("renamer-figure",
                          f"expected renamer_patched={want}, got {got}")

    check("renamer figure ignores a LATER patcher's 0 (CT-1 false refusal)",
          lambda: _expect_rp([
              _RENAMER_STEP, _APPLIED.format(n=13151, m=1045),
              "[60/67] PATCH  guard variables to match ??_B naming",
              _APPLIED.format(n=287, m=0)], 1045),
          expect_refusal=False)
    check("renamer figure ignores a LATER patcher's NON-ZERO "
          "(mirror case: an INERT map edit must still REFUSE)",
          lambda: _expect_rp([
              _RENAMER_STEP, _APPLIED.format(n=13151, m=0),
              _EH_STEP, _APPLIED.format(n=1096, m=51)], 0),
          expect_refusal=False)
    check("renamer figure is None when the renamer step never ran "
          "(absence must not read as 0)",
          lambda: _expect_rp([
              _EH_STEP, _APPLIED.format(n=1096, m=51)], None),
          expect_refusal=False)

    # ---- STALE RUNNER guard (lane CL-4) -----------------------------------
    # Behavioural, not a shape test: every branch of the decision is driven
    # with synthetic blob ids. The branch that MATTERS is "stale" — it is the
    # one that was silently reachable for all of lane CK-3.
    HEADB, OLDB, LOCALB = "h" * 40, "o" * 40, "l" * 40
    hist = {OLDB: "23ad2f92" + "0" * 32}
    check("tool freshness PASSES when the runner is HEAD's version",
          lambda: check_tool_freshness(HEADB, HEADB, hist),
          expect_refusal=False)
    check("tool freshness REFUSES a SUPERSEDED committed runner (CK-3)",
          lambda: check_tool_freshness(OLDB, HEADB, hist), expect_refusal=True)
    check("tool freshness PASSES an UNCOMMITTED local edit (no false alarm)",
          lambda: check_tool_freshness(LOCALB, HEADB, hist),
          expect_refusal=False)
    check("tool freshness honours --allow-stale-tool",
          lambda: check_tool_freshness(OLDB, HEADB, hist, allow_stale=True),
          expect_refusal=False)
    # ...and the three non-refusing branches must be DISTINGUISHABLE, or the
    # guard degenerates into "always fine" for everything it does not refuse.
    states = (check_tool_freshness(HEADB, HEADB, hist)["state"],
              check_tool_freshness(LOCALB, HEADB, hist)["state"],
              check_tool_freshness(OLDB, HEADB, hist, allow_stale=True)["state"])
    ok = states == ("matches_head", "not_in_history", "stale_override")
    print(("  PASS" if ok else "  FAIL") +
          f"  tool freshness states are distinguishable: {states} "
          "(a guard whose pass-states are indistinguishable cannot be "
          "audited after the fact)")
    if not ok:
        fails.append("tool freshness states")

    # ---- the settle loop must NOT restore symbols.txt (lane CK-2) ---------
    # A SHAPE test, not a behavioural one (behaviour needs a ~5-min build) —
    # stated plainly so nobody reads it as stronger than it is. It guards the
    # exact regression that blocked wave CJ: symbols.txt is a discovered dep
    # of the SPLIT edge AND a split output, so a restore inside the loop
    # re-dirties the graph and settling becomes impossible. Measured at
    # 23ad2f92: with the in-loop restore, builds 2/3/4 each did 2 work edges
    # forever; without it, the next build did 0.
    src_lines = inspect.getsource(ABMeasure.settle).splitlines()
    loop_at = next((i for i, l in enumerate(src_lines)
                    if l.strip().startswith("for attempt in range(")), None)
    pre = [l for l in src_lines[:loop_at or 0]
           if "self.restore_symbols()" in l and not l.strip().startswith("#")]
    post = [l for l in src_lines[loop_at:]
            if "self.restore_symbols()" in l and not l.strip().startswith("#")] \
        if loop_at is not None else ["<no loop found>"]
    ok = loop_at is not None and len(pre) == 1 and not post
    print(("  PASS" if ok else "  FAIL") +
          f"  settle() restores symbols.txt EXACTLY ONCE pre-loop "
          f"(pre={len(pre)}, in-loop={len(post)}) — an in-loop restore "
          "re-dirties a discovered SPLIT dep and settling becomes impossible "
          "(wave CJ: 2 work edges forever)")
    if not ok:
        fails.append("settle in-loop restore")

    # ---- result.json must carry the FULL unit populations (lane CK-2) -----
    a_u = {f"u{i}": 5 for i in range(20)}
    b_u = {f"u{i}": 4 for i in range(20)}
    regs, imps, meta = unit_breakdown(a_u, b_u, limit=15)
    ok = (len(regs) == 15 and len(meta["full_regressions"]) == 20
          and sum(r["delta"] for r in meta["full_regressions"]) == -20
          and sum(r["delta"] for r in regs) == -15)
    print(("  PASS" if ok else "  FAIL") +
          f"  unit_breakdown exports FULL lists for result.json: "
          f"shown={len(regs)} full={len(meta['full_regressions'])} "
          f"sum_shown={sum(r['delta'] for r in regs)} "
          f"sum_full={sum(r['delta'] for r in meta['full_regressions'])} "
          "(the archived artifact must not lose attribution to the display "
          "limit)")
    if not ok:
        fails.append("unit_breakdown full export")

    print(f"selftest: {'ALL PASS' if not fails else f'FAILURES: {fails}'}")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main())
