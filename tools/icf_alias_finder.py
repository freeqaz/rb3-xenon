#!/usr/bin/env python3
"""Find / validate PROVEN ICF-merged-symbol alias pairs (the PoolAlloc gap).

The problem
-----------
Retail RB3 ICF-folds debug-stripped allocator overloads (2-arg ``PoolAlloc``,
1-arg ``MemOrPoolAlloc``/``MemOrPoolAllocSTL``) onto their named debug siblings.
Only the named (debug) spelling survives at a single address in the XEX, so
``scripts/target_symbol_map.json`` has just that one. Our compiled objs emit the
byte-IDENTICAL call site but reference the folded (debug-stripped) spelling, so
objdiff's by-name ``reloc_eq`` flags a ``[sym]`` mismatch. ``scripts/
symbol_aliases.json`` + ``tools/gen_symbol_alias_map.py`` declare the folds to
objdiff's ``map_file``/``symbol_equivalences`` mechanism, neutralizing the noise.

This tool is the EVIDENCE engine behind that data file:

  --validate   Classify every group in scripts/symbol_aliases.json against the
               objs, and FAIL only on the one class that is a genuine
               contradiction. Exit 0 = pass, 1 = contradicted group(s), 2 =
               precondition refused (unrenamed target objs).

               ⚠ REWRITTEN 2026-08-14 (lane ALIASVAL-1) because the previous
               shape was a gate nobody read. It applied three checks and failed
               the group on any of them, which produced **221 standing failures
               out of 1,499** -- and 211 of those 221 were the instrument, not
               the data:

                 * a NON-RECURSIVE glob over `build/45410914/obj/*.obj` hid 569
                   of the 3,084 live target objs (see live_target_obj_paths),
                   manufacturing "target objs name []" for symbols defined only
                   in a subdirectory unit;
                 * check (a) demanded the survivor be in target_symbol_map.json,
                   which a `vftable_<addr>` placeholder can never be -- 34/34 of
                   that class failed;
                 * check (c) demanded the SURVIVOR specifically be the named
                   member, which tests a labelling convention, not ICF;
                 * check (b)'s "remedy" (pruning unreferenced spellings) is
                   MEASURED HARMFUL -- a745039e restored 14 such, worth +94,616 B.

               A blanket failure list of that composition cannot be actioned, so
               it was ignored -- which is worse than no gate, because it also
               hid the 10 groups that ARE contradicted. Now: OK / four named and
               counted TOLERATED classes / CONTRADICTED (fatal).

  --scan       Discover NEW fold candidates from objdiff diffs: scan the non-100
               function pool for paired ``bl <A>`` / ``bl <B>`` rows where the
               relocation differs ONLY by symbol name (byte-identical call site).
               Reports, per candidate alias pair, the victim functions and how
               many would flip to 100% if aliased (the "pure" victims). Use this
               to seed new groups -- ONLY add a pair the scan proves by the
               byte-identical-call-site signature, never by fuzzy matching.

  --report     Apply the current alias map to the non-100 pool and report the
               per-function norm-% delta (with vs without the synthetic map).
               Shows the realized lever: which functions flip to 100 and which
               only gain partial credit (other divergences remain).

All three are read-only (no build inputs mutated).
"""

import argparse
import glob
import json
import os
import struct
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent

sys.path.insert(0, str(PROJECT_ROOT / "scripts" / "harvest"))
from live_units import filter_live  # noqa: E402
ALIASES = PROJECT_ROOT / "scripts" / "symbol_aliases.json"
TARGET_MAP = PROJECT_ROOT / "scripts" / "target_symbol_map.json"
OBJDIFF_JSON = PROJECT_ROOT / "objdiff.json"
REPORT_JSON = PROJECT_ROOT / "build" / "45410914" / "report.json"
OBJDIFF_CLI = PROJECT_ROOT / "bin" / "objdiff-cli"
GEN_MAP = PROJECT_ROOT / "tools" / "gen_symbol_alias_map.py"


# ---------------------------------------------------------------------------
# COFF symbol table reader (mirrors obj_target_symbol_renamer.py's parser).
# ---------------------------------------------------------------------------
def coff_referenced_symbols(data: bytes) -> set:
    """All symbol names present in a COFF .obj (defined + undefined refs)."""
    if len(data) < 20:
        return set()
    sym_off = struct.unpack_from("<I", data, 8)[0]
    num = struct.unpack_from("<I", data, 12)[0]
    if not sym_off or not num:
        return set()
    str_start = sym_off + num * 18
    out = set()
    i = 0
    while i < num:
        eo = sym_off + i * 18
        if eo + 18 > len(data):
            break
        nb = data[eo:eo + 8]
        if nb[:4] == b"\x00\x00\x00\x00":
            so = struct.unpack_from("<I", nb, 4)[0]
            ao = str_start + so
            if ao < len(data):
                end = data.index(b"\x00", ao)
                name = data[ao:end].decode("ascii", "replace")
            else:
                name = ""
        else:
            name = nb.split(b"\x00")[0].decode("ascii", "replace")
        out.add(name)
        i += 1 + data[eo + 17]
    return out


def load_aliases() -> list:
    return json.loads(ALIASES.read_text()).get("groups", [])


def target_map_names() -> set:
    raw = json.loads(TARGET_MAP.read_text())
    return {v for k, v in raw.items() if k.lower().startswith("0x")}


def compiled_obj_paths() -> list:
    """Our compiled objs. Superset of objdiff.json's `base_path` set (measured)."""
    return glob.glob(str(PROJECT_ROOT / "build/45410914/src/**/*.obj"), recursive=True)


def authoritative_base_obj_paths() -> list:
    """objdiff.json's `base_path` entries that exist on disk -- the compiled-side
    analogue of live_target_obj_paths(), used ONLY as a self-calibrating FLOOR.

    We do NOT switch compiled_obj_symbol_index() over to this list: measured
    2026-08-17, the glob returns 1,205 objs and this list 1,048, and the glob is
    a strict SUPERSET (0 base_paths missing from it). Narrowing to base_path
    would DROP 160 objs and silently move groups into STALE_SPELLING -- the very
    misclassification this floor exists to detect.
    """
    try:
        cfg = json.loads(OBJDIFF_JSON.read_text())
    except (OSError, ValueError):
        return []
    return [u["base_path"] for u in cfg.get("units", [])
            if u.get("base_path") and os.path.exists(u["base_path"])]


def compiled_obj_symbol_index(paths=None) -> dict:
    """name -> list of compiled-obj stems referencing it."""
    idx = defaultdict(list)
    for p in (compiled_obj_paths() if paths is None else paths):
        for s in coff_referenced_symbols(Path(p).read_bytes()):
            idx[s].append(Path(p).stem)
    return idx


def live_target_obj_paths() -> list:
    """The dtk-split target objs objdiff ACTUALLY diffs, taken from objdiff.json.

    ⛔ THIS USED TO BE `glob("build/45410914/obj/*.obj")` -- A NON-RECURSIVE GLOB
    -- AND THAT WAS A MEASURED FALSE-NEGATIVE SOURCE, not a style nit.  569 of the
    3,084 live target objs live in SUBDIRECTORIES (`obj/band3/meta_band`,
    `obj/xdk/xgraphics`, `obj/network/quazal/...`), so every symbol defined only in
    one of those units was invisible to check (c), which then reported
    ``target objs name []`` -- "unwitnessed, therefore inert".

    That false negative has already cost this repo real confusion.
    `tools/ourside_fold_sweep.py` OBSERVED it (its 11 `target objs name []` groups
    were dropped as inert and the drop cost -6,652 B / 2 units off 100%) and wrote
    a standing "DO NOT DROP GROUPS ON CHECK (c)" prohibition, but never found the
    cause.  The cause is this glob.  Restoring the recursive/authoritative set
    resolves exactly 11 of the 22 `NOTHING` groups (lane ALIASVAL-1).

    objdiff.json's `target_path` list is the authoritative set -- it is by
    definition what the report diffs -- so it needs no live-filtering.
    """
    try:
        cfg = json.loads(OBJDIFF_JSON.read_text())
    except (OSError, ValueError):
        cfg = {}
    paths = [u["target_path"] for u in cfg.get("units", [])
             if u.get("target_path") and os.path.exists(u["target_path"])]
    if paths:
        return paths
    # Fallback only if objdiff.json is absent/unreadable: RECURSIVE glob, then the
    # live filter.  Never the flat glob again.
    globbed = glob.glob(str(PROJECT_ROOT / "build/45410914/obj/**/*.obj"), recursive=True)
    try:
        return filter_live(globbed, str(PROJECT_ROOT))
    except (OSError, ValueError):
        return globbed


def target_obj_symbol_index() -> dict:
    """name -> count of live dtk-split target objs referencing it (post-renamer)."""
    idx = defaultdict(int)
    for p in live_target_obj_paths():
        for s in coff_referenced_symbols(Path(p).read_bytes()):
            idx[s] += 1
    return idx


# ---------------------------------------------------------------------------
# --validate
# ---------------------------------------------------------------------------
# The TOLERATED classes, each with the reason it is NOT a failure. A gate that
# emits 221 undifferentiated failures is a gate nobody reads -- and this one was
# read by nobody for exactly that reason. Every class below is either a property
# of OUR pinning/labelling (not of retail's linker) or a condition whose
# "remedy" has been MEASURED HARMFUL. They are reported, counted and named; only
# CONTRADICTED is fatal.
TOLERATED = {
    "PLACEHOLDER_SURVIVOR":
        "survivor is a dtk placeholder data symbol (vftable_/fn_/lbl_), which is "
        "never in target_symbol_map.json (a FUNCTION map) -- check (a) is "
        "structurally inapplicable, and 34/34 such groups fail it",
    "SURVIVOR_MISLABELED":
        "exactly one member is named in the target objs, but it is a `folded` "
        "entry rather than the `survivor` field -- a LABELLING convention, not a "
        "claim: the co-location assertion is intact and the named member sits AT "
        "the group address",
    "UNWITNESSED":
        "no member is named in the live target objs. This is COVERAGE of our own "
        "pinning, NOT evidence about retail's linker -- and dropping such groups "
        "was MEASURED to cost -6,652 B / 2 units off 100% (ourside_fold_sweep.py)",
    "STALE_SPELLING":
        "a folded spelling is referenced by 0 compiled objs, so it can appear on "
        "neither side of a charge. Pruning on this screen is MEASURED HARMFUL: "
        "commit a745039e restored 14 spellings pruned this way, worth +94,616 B",
}


def classify_group(g, tmap, compiled, target_objs):
    """-> (verdict, detail). verdict is 'OK', a TOLERATED key, or 'CONTRADICTED'."""
    survivor = g["survivor"]
    folded = g.get("folded", [])
    named = [s for s in (survivor, *folded) if target_objs.get(s, 0) > 0]

    # *** THE ONLY FATAL CLASS ***
    # A group asserts "all these spellings denote ONE body at ONE address". If two
    # members are named in the target objs, our own renamer placed them at two
    # DISTINCT retail addresses -- symbol_aliases.json and target_symbol_map.json
    # contradict each other, and under name_check the alias forgives that
    # difference BY CONSTRUCTION. Which file is wrong is a question for retail
    # bytes; that it is unproven is not.
    if len(named) > 1:
        # An exemption is the ONLY way a contradiction stops being fatal, and it
        # must carry its evidence in the data file where the group lives -- not in
        # a side list, and never by silence. Each one here is licensed by a retail
        # bl-caller census (an address with 0 call sites in the whole image cannot
        # be the callee our relocation denotes -- a745039e's junk-map-row class) or
        # by a prior MEASURED adjudication. The gate still prints them.
        if g.get("contradiction_exempt"):
            return "CONTRADICTION_EXEMPT", g["contradiction_exempt"]
        return "CONTRADICTED", f"target objs name {len(named)} members: {named}"

    if not named:
        return "UNWITNESSED", "no member named in live target objs"
    if survivor.startswith(("vftable_", "fn_", "lbl_")) and survivor not in tmap:
        return "PLACEHOLDER_SURVIVOR", f"survivor {survivor} is a dtk placeholder"
    if named != [survivor]:
        return "SURVIVOR_MISLABELED", f"target objs name {named[0]}, not the survivor"
    if survivor not in tmap:
        return "CONTRADICTED", f"survivor {survivor} NOT in target_symbol_map.json"
    stale = [f for f in folded if f not in compiled]
    if stale:
        return "STALE_SPELLING", f"{len(stale)}/{len(folded)} folded spelling(s) unreferenced"
    return "OK", ""


# ---------------------------------------------------------------------------
# PRECONDITIONS -- the ABSENT-POPULATION class (lane W38-GATES, 2026-08-17)
# ---------------------------------------------------------------------------
# This gate guarded rigorously against MALFORMED inputs and not at all against
# ABSENT or EMPTY ones. An empty population passes every check by construction,
# and the resulting PASS is indistinguishable from a real one. Measured, on a
# fully built tree at grounded2-restoration (where the real run exits 1 with 5
# CONTRADICTED):
#
#   scripts/symbol_aliases.json = {"groups": []}   -> "VALIDATE: PASS", rc 0
#   scripts/symbol_aliases.json = {}               -> "VALIDATE: PASS", rc 0
#   truncated to 5 of 1,531 groups                 -> "VALIDATE: PASS", rc 0
#
# The third is the dangerous one: it is the shape of a bad merge-resolve or a
# buggy regeneration, and it turns a RED gate GREEN. A gate whose failure can be
# cured by deleting its own input is worse than no gate -- this one stands over
# the ~818,416 B / 7.93 pp of matched_code that alias forgiveness carries
# (ablation, lane ALIAS-2 `64088f62`).
#
# And a SECOND vacuity of the same shape sat one layer down, unbriefed: the tool
# had a loud REFUSING precondition for the TARGET objs and NONE for the COMPILED
# objs. With the compiled index empty, `stale = [f for f in folded if f not in
# compiled]` is true of everything, so every group with a folded spelling lands
# in STALE_SPELLING. Measured with the compiled index emptied:
#
#   OK (MAP-CONSISTENT)  1307 -> 30      STALE_SPELLING  144 -> 1421
#   CONTRADICTED            5 ->  5      exit code        1 -> 1   (IDENTICAL)
#
# i.e. 92.8% of the population is silently reclassified, the COVERAGE line still
# reports a confident "1531 groups over 3091 live target objs", and the verdict
# does not move. It fails into a bucket whose own documented rationale is that
# acting on it is MEASURED HARMFUL (-94,616 B to reverse) -- so the misreading it
# invites is precisely the prune that a745039e had to undo.
#
# Every threshold below is either ZERO (not a choice) or read from an artifact
# that moves with the data (objdiff.json). No constant is chosen by hand.

REFUSAL_REASONS = {
    "UNRENAMED_TARGET_OBJS":
        "the dtk-split target objs still carry anonymous `fn_<addr>` symbols",
    "EMPTY_POPULATION":
        "scripts/symbol_aliases.json declares ZERO alias groups",
    "EMPTY_COMPILED_INDEX":
        "no compiled objs were indexed under build/45410914/src",
    "INCOMPLETE_COMPILED_INDEX":
        "objdiff.json names base objs that the compiled-obj scan did not reach",
}


def preconditions(groups, target_objs, compiled_paths, base_paths) -> list:
    """-> list of (REASON, detail). Empty list means the instrument has its inputs.

    Split out of cmd_validate so the selftest can assert the SPECIFIC reason a
    control went red. Asserting only "rc != 0" is how a control ends up passing
    for the wrong cause -- cf. the --self-break trap that has bitten three lanes.
    """
    out = []

    # (1) Target objs must be RENAMED. A fresh worktree's reflinked target objs
    # are PRE-renamer, so every mangled name reads ABSENT until the first build.
    mangled = sum(1 for s in target_objs if s.startswith("?"))
    if mangled < 1000:
        out.append(("UNRENAMED_TARGET_OBJS",
                    f"only {mangled} mangled symbol(s) in the target objs; every "
                    f"group would report `target objs name []` and the run would "
                    f"claim a 100% failure that reflects the BUILD STATE, not the "
                    f"data. fix: run `./tools/ninja-locked` first."))

    # (2) The population must EXIST. Zero is not a threshold anyone chose.
    if not groups:
        out.append(("EMPTY_POPULATION",
                    "0 groups loaded. There is nothing to validate, so a PASS "
                    "here would certify only that the file is empty -- which is "
                    "exactly how a truncated or mis-merged alias file turns a red "
                    "gate green."))

    # (3) The COMPILED objs must be present, and must cover objdiff.json's own
    # base_path set. The floor is READ from objdiff.json, never hardcoded, so it
    # tracks the build instead of rotting. Measured 2026-08-17: the glob returns
    # 1,205 objs against 1,048 existing base_paths, a strict superset -- 0
    # missing -- so this check has 0 false positives on a healthy tree by
    # construction, and it is that superset property (not a margin) that licenses it.
    if not compiled_paths:
        out.append(("EMPTY_COMPILED_INDEX",
                    "0 compiled objs found under build/45410914/src. Every folded "
                    "spelling would read unreferenced and the whole population "
                    "would collapse into TOLERATED STALE_SPELLING while the exit "
                    "code stayed put."))
    elif base_paths:
        have = {os.path.realpath(p) for p in compiled_paths}
        missing = [p for p in base_paths if os.path.realpath(p) not in have]
        if missing:
            out.append(("INCOMPLETE_COMPILED_INDEX",
                        f"{len(missing)}/{len(base_paths)} objdiff.json base objs "
                        f"were not reached by the compiled-obj scan (e.g. "
                        f"{missing[0]}). STALE_SPELLING would be overstated by an "
                        f"unknown amount."))
    return out


def cmd_validate(args=None) -> int:
    groups = load_aliases()
    tmap = target_map_names()
    compiled_paths = compiled_obj_paths()
    base_paths = authoritative_base_obj_paths()
    compiled = compiled_obj_symbol_index(compiled_paths)
    target_objs = target_obj_symbol_index()

    refusals = preconditions(groups, target_objs, compiled_paths, base_paths)
    if refusals:
        print("REFUSING: the instrument does not have its inputs -- a verdict here "
              "would describe the BUILD/DATA STATE, not the alias data.",
              file=sys.stderr)
        for reason, detail in refusals:
            print(f"  [{reason}] {REFUSAL_REASONS[reason]}", file=sys.stderr)
            print(f"      {detail}", file=sys.stderr)
        print("VALIDATE: REFUSED (exit 2) -- NOT a pass and NOT a failure.",
              file=sys.stderr)
        return 2

    mangled = sum(1 for s in target_objs if s.startswith("?"))

    # ── INSTRUMENT THE INSTRUMENT ──────────────────────────────────────────────
    # Count what was REACHED, not just what passed. A prior lane's calibration in
    # a sibling tool reported "0 disagreements" over ZERO rows and was caught only
    # by not-reached counters, so the classification loop asserts its own coverage
    # rather than trusting that it ran.
    buckets = defaultdict(list)
    classified = 0
    spellings_looked_up = 0
    for g in groups:
        verdict, detail = classify_group(g, tmap, compiled, target_objs)
        buckets[verdict].append((g.get("name", g["survivor"]), g["address"], detail))
        classified += 1
        spellings_looked_up += 1 + len(g.get("folded", []))
    if classified != len(groups):
        print(f"REFUSING: classified {classified} of {len(groups)} groups -- the "
              f"classification loop did not reach the whole population.",
              file=sys.stderr)
        return 2

    n_bad = len(buckets.get("CONTRADICTED", []))
    n_ok = len(buckets.get("OK", []))
    n_tol = sum(len(v) for k, v in buckets.items() if k in TOLERATED)

    for name, addr, detail in buckets.get("CONTRADICTED", []):
        print(f"FAIL [{name} @ {addr}]: {detail}")
    for name, addr, detail in buckets.get("CONTRADICTION_EXEMPT", []):
        print(f"EXEMPT [{name} @ {addr}]: {detail}")

    print()
    # ⚠ This block used to report ONLY the target-obj side. The compiled-obj side
    # was consulted by classify_group (it decides STALE_SPELLING) but never
    # counted, so an absent compiled population was invisible in the output --
    # 1,277 groups could move OK -> STALE_SPELLING with the printed COVERAGE line
    # unchanged. Both populations are reported now, and both are refused when empty.
    print(f"COVERAGE: {len(groups)} groups classified ({classified}/{len(groups)} "
          f"reached, {spellings_looked_up} member spellings looked up)")
    print(f"  target side  : {len(live_target_obj_paths())} live target objs, "
          f"{mangled} mangled names indexed")
    print(f"  compiled side: {len(compiled_paths)} compiled objs, {len(compiled)} "
          f"symbols indexed (floor: {len(base_paths)} objdiff.json base objs, all reached)")
    # ⚠ NOT "grounded" -- RENAMED 2026-08-14 (lane GROUNDED-1). This bucket means
    # ONLY: the survivor is map-resident and every spelling is referenced. It says
    # NOTHING about whether retail's linker folded anything, and the word "grounded"
    # was read upstream as if it did (a coordinator brief called ~7 pp of
    # matched_code "grounded", meaning proven). The demonstration that it cannot
    # carry that weight: the eight groups 5a8a8bf9 emptied of EVERY folded spelling
    # -- i.e. groups that now declare no fold at all -- still land in this bucket,
    # and the count did not move. A label insensitive to the claim it appears to
    # certify is worse than no label.
    #
    # The fold evidence lives per-group in scripts/symbol_aliases.json's `evidence`
    # field (tiers T1/T2/T3) and is re-derivable on retail bytes today with
    # tools/icf_pair_adjudicate.py. Measured byte split of the 720,992 B this
    # mechanism forgives: 82.51% PROVEN on retail bytes, 6.12% blocked on absent
    # source, 0.10% on one unidentified map address, 0.27% CONTRADICTED (withdrawn
    # in 5a8a8bf9), 11.00% unattributable by a name-keyed census (anonymous rows).
    # See docs/decomp/ALIAS_FORGIVENESS_SIZED_2026-08-14.md.
    print(f"  OK (MAP-CONSISTENT)    {n_ok:5d}  -- survivor map-resident + every "
          f"spelling referenced. NOT a fold proof; see the comment at this print.")
    for k in sorted(TOLERATED):
        rows = buckets.get(k, [])
        if rows:
            print(f"  TOLERATED {k:<20} {len(rows):5d}  -- {TOLERATED[k]}")
    n_ex = len(buckets.get("CONTRADICTION_EXEMPT", []))
    if n_ex:
        print(f"  CONTRADICTION_EXEMPT   {n_ex:5d}  -- contradicted BUT carrying a "
              f"per-group `contradiction_exempt` reason in scripts/symbol_aliases.json "
              f"(printed above); each is licensed by a retail bl-caller census or a "
              f"prior measured adjudication, never by silence")
    print(f"  CONTRADICTED (FATAL)   {n_bad:5d}")
    if args is not None and getattr(args, "json", None):
        Path(args.json).write_text(json.dumps(
            {k: [{"name": n, "address": a, "detail": d} for n, a, d in v]
             for k, v in buckets.items()}, indent=1))
        print(f"wrote {args.json}")
    print(f"VALIDATE: {'PASS' if n_bad == 0 else 'FAIL'} -- {n_ok} map-consistent, "
          f"{n_tol} tolerated (enumerated above), {n_bad} contradicted, "
          f"{len(groups)} total")
    return 0 if n_bad == 0 else 1


# ---------------------------------------------------------------------------
# --selftest : PROVE each refusal can fire, and that they DISCRIMINATE
# ---------------------------------------------------------------------------
# ⛔ The --self-break trap has bitten three lanes, including one briefed on it.
# Two rules encoded here:
#
#   1. FROZEN fixtures. Every population below is a literal, never derived from
#      the live tree. A control built from live data can have its own red set
#      emptied by the very breakage it is meant to detect, and then it trips a
#      DIFFERENT refusal (or a vacuity refusal) and still looks red.
#   2. Assert the SPECIFIC reason, never merely "rc != 0". A red for the wrong
#      cause is not a proof that the check works.
#
# And the HEALTHY leg is the load-bearing one: a gate that refuses everything
# proves nothing. `_HEALTHY` must produce exactly ZERO refusals or the whole
# selftest is meaningless.

_HEALTHY = dict(
    groups=[{"survivor": "?a@@YAXXZ", "folded": ["?b@@YAXXZ"], "address": "0x1"},
            {"survivor": "?c@@YAXXZ", "folded": [], "address": "0x2"}],
    target_objs={f"?frozen{i}@@YAXXZ": 1 for i in range(1500)},
    compiled_paths=["/frozen/x.obj", "/frozen/y.obj"],
    base_paths=["/frozen/x.obj"],
)


def _selftest() -> int:
    def run(**over):
        kw = dict(_HEALTHY)
        kw.update(over)
        return [r for r, _ in preconditions(**kw)]

    ok = True

    def check(label, cond, detail=""):
        nonlocal ok
        print(f"  [{'PASS' if cond else 'FAIL'}] {label}"
              f"{(' -- ' + detail) if detail else ''}")
        if not cond:
            ok = False

    # ── DISCRIMINATION CONTROL (must be GREEN, or every red below is worthless) ─
    base = run()
    check("healthy frozen population raises NO refusal", base == [], f"got {base}")

    # ── Each refusal must fire, and must be the one named. ─────────────────────
    for label, over, want in (
        ("EMPTY_POPULATION fires on groups=[]",
         {"groups": []}, "EMPTY_POPULATION"),
        ("EMPTY_POPULATION fires on a `groups` key absent from the file",
         {"groups": json.loads("{}").get("groups", [])}, "EMPTY_POPULATION"),
        ("UNRENAMED_TARGET_OBJS fires on pre-renamer target objs",
         {"target_objs": {f"fn_{i:08x}": 1 for i in range(1500)}},
         "UNRENAMED_TARGET_OBJS"),
        ("EMPTY_COMPILED_INDEX fires on 0 compiled objs",
         {"compiled_paths": []}, "EMPTY_COMPILED_INDEX"),
        ("INCOMPLETE_COMPILED_INDEX fires when a base obj is unreached",
         {"base_paths": ["/frozen/x.obj", "/frozen/UNREACHED.obj"]},
         "INCOMPLETE_COMPILED_INDEX"),
    ):
        got = run(**over)
        check(label, want in got, f"reasons={got}")

    # A truncation is NOT caught by a count threshold -- deliberately. Group count
    # is NOT monotonic in this repo: measured over all 52 commits that touched
    # scripts/symbol_aliases.json, 7 legitimate DECREASES occur (worst 521->511,
    # 1.92%). A "count must not drop" rule would fire on 13.5% of real commits --
    # the noise failure mode, which trains people to ignore the gate. What IS
    # non-arbitrary is zero, so EMPTY_POPULATION is the guard, and a partial
    # truncation is caught by the CONTRADICTED groups it can no longer hide.
    check("truncation-to-nonempty is NOT refused (count is not monotonic; "
          "7/52 historical commits legitimately shrink the population)",
          run(groups=_HEALTHY["groups"][:1]) == [])

    print("SELFTEST:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


# ---------------------------------------------------------------------------
# objdiff diff helper
# ---------------------------------------------------------------------------
def diff_fn(tp, bp, fn, mapf=None, include_instructions=False):
    cmd = [str(OBJDIFF_CLI), "diff", "-1", tp, "-2", bp, "-f", "json", "-o", "-", fn]
    if include_instructions:
        cmd.append("--include-instructions")
    if mapf:
        cmd += ["--map-file", mapf]
    try:
        out = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
        return json.loads(out.stdout)
    except Exception:
        return None


def units_index() -> dict:
    cfg = json.loads(OBJDIFF_JSON.read_text())
    return {u["name"]: u for u in cfg.get("units", [])}


def pool_referencing_unit_stems(folded_names: set) -> set:
    """Compiled-obj stems referencing any folded spelling (candidate victims)."""
    stems = set()
    for p in glob.glob(str(PROJECT_ROOT / "build/45410914/src/**/*.obj"), recursive=True):
        syms = coff_referenced_symbols(Path(p).read_bytes())
        if syms & folded_names:
            stems.add(Path(p).stem)
    return stems


def is_alias_bl_row(row, pair_set) -> bool:
    """True if this is a paired ``bl`` whose target/base refs are an alias pair."""
    t = (row.get("target") or {})
    b = (row.get("base") or {})
    if t.get("opcode") != "bl" or b.get("opcode") != "bl":
        return False
    ta = t.get("args", "") or ""
    ba = b.get("args", "") or ""
    for a, c in pair_set:
        if a in ta and c in ba:
            return True
    return False


def build_pair_set(groups) -> set:
    """All (target_name, base_name) ordered alias pairs from the groups."""
    pairs = set()
    for g in groups:
        survivor = g["survivor"]
        for f in g.get("folded", []):
            pairs.add((survivor, f))   # target=survivor, base=folded
    return pairs


def cmd_scan(args) -> int:
    groups = load_aliases()
    pair_set = build_pair_set(groups)
    folded = {f for g in groups for f in g.get("folded", [])}
    units = units_index()
    report = json.loads(REPORT_JSON.read_text())
    stems = pool_referencing_unit_stems(folded)
    cands = []
    for u in report.get("units", []):
        if u["name"].split("/")[-1] not in stems:
            continue
        for f in u.get("functions", []):
            pct = f.get("match_percent_normalized", 0)
            if pct < 100.0:
                cands.append((pct, u["name"], f["name"]))
    print(f"scanning {len(cands)} non-100 fns in {len(stems)} alias-referencing units...",
          file=sys.stderr)
    pure, withd = [], []
    checked = 0
    for pct, un, fn in cands:
        uu = units.get(un)
        if not uu:
            continue
        tp, bp = uu["target_path"], uu["base_path"]
        if not (os.path.exists(tp) and os.path.exists(bp)):
            continue
        d = diff_fn(tp, bp, fn, include_instructions=True)
        if not d:
            continue
        checked += 1
        rows = d.get("instructions", [])
        nonmatch = [r for r in rows if r.get("match_type") != "equal"]
        if not nonmatch:
            continue
        has_alias = any(is_alias_bl_row(r, pair_set) for r in nonmatch)
        if not has_alias:
            continue
        all_alias = all(is_alias_bl_row(r, pair_set) for r in nonmatch)
        rec = {"pct": pct, "unit": un, "fn": fn,
               "alias_rows": sum(1 for r in nonmatch if is_alias_bl_row(r, pair_set)),
               "other_rows": sum(1 for r in nonmatch if not is_alias_bl_row(r, pair_set))}
        withd.append(rec)
        if all_alias:
            pure.append(rec)
    print(f"checked {checked} fns")
    print(f"victims with an alias-pair bl diff row: {len(withd)}")
    print(f"PURE victims (alias is ONLY diff -> would flip to 100): {len(pure)}")
    for r in sorted(pure, key=lambda x: x["pct"]):
        print(f"  PURE {r['pct']:6.2f}  {r['unit']}  {r['fn'][:60]}")
    for r in sorted(withd, key=lambda x: x["pct"]):
        if r in pure:
            continue
        print(f"  WITH {r['pct']:6.2f}  +{r['other_rows']} other  {r['unit']}  {r['fn'][:50]}")
    if args.out:
        Path(args.out).write_text(json.dumps({"pure": pure, "with_other": withd}, indent=2))
        print(f"wrote {args.out}")
    return 0


def cmd_report(args) -> int:
    """Realized-lever report: per-fn norm-% with vs without the synthetic map."""
    # Ensure the synthetic map exists.
    mapf = args.map or str(PROJECT_ROOT / "build" / "45410914" / "icf_aliases.map")
    if not os.path.exists(mapf):
        subprocess.run([sys.executable, str(GEN_MAP), "--out", mapf], check=True)
    groups = load_aliases()
    pair_set = build_pair_set(groups)
    folded = {f for g in groups for f in g.get("folded", [])}
    units = units_index()
    report = json.loads(REPORT_JSON.read_text())
    stems = pool_referencing_unit_stems(folded)
    flips, gains = [], []
    cands = []
    for u in report.get("units", []):
        if u["name"].split("/")[-1] not in stems:
            continue
        for f in u.get("functions", []):
            pct = f.get("match_percent_normalized", 0)
            if pct < 100.0:
                cands.append((pct, u["name"], f["name"]))
    print(f"measuring {len(cands)} non-100 fns with/without alias map...", file=sys.stderr)
    for pct, un, fn in cands:
        uu = units.get(un)
        if not uu:
            continue
        tp, bp = uu["target_path"], uu["base_path"]
        if not (os.path.exists(tp) and os.path.exists(bp)):
            continue
        d0 = diff_fn(tp, bp, fn, include_instructions=True)
        if not d0:
            continue
        rows = d0.get("instructions", [])
        nonmatch = [r for r in rows if r.get("match_type") != "equal"]
        if not any(is_alias_bl_row(r, pair_set) for r in nonmatch):
            continue
        n0 = d0["normalized_match_percent"]
        d1 = diff_fn(tp, bp, fn, mapf=mapf)
        n1 = d1["normalized_match_percent"] if d1 else n0
        if n1 >= 100.0 and n0 < 100.0:
            flips.append((n0, n1, un, fn))
        elif n1 > n0:
            gains.append((n0, n1, un, fn))
    print(f"FLIPS to 100% (matched-count gain): {len(flips)}")
    for n0, n1, un, fn in sorted(flips):
        print(f"  FLIP {n0:6.2f} -> {n1:6.2f}  {un}  {fn[:55]}")
    print(f"partial-credit gains (still <100, other divergence remains): {len(gains)}")
    for n0, n1, un, fn in sorted(gains):
        print(f"  GAIN {n0:6.3f} -> {n1:6.3f}  {un}  {fn[:50]}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="mode")
    ap.add_argument("--validate", action="store_true",
                    help="validate scripts/symbol_aliases.json against the objs")
    ap.add_argument("--scan", action="store_true",
                    help="discover new fold candidates from objdiff diffs")
    ap.add_argument("--report", action="store_true",
                    help="realized-lever per-fn delta with/without the alias map")
    ap.add_argument("--out", default=None, help="write scan/report JSON to PATH")
    ap.add_argument("--map", default=None, help="synthetic map path (for --report)")
    ap.add_argument("--json", default=None, help="write the --validate classification to PATH")
    ap.add_argument("--selftest", action="store_true",
                    help="prove each --validate precondition can fire (frozen fixtures)")
    args = ap.parse_args()
    if args.selftest:
        return _selftest()
    if args.validate:
        return cmd_validate(args)
    if args.scan:
        return cmd_scan(args)
    if args.report:
        return cmd_report(args)
    ap.print_help()
    return 1


if __name__ == "__main__":
    sys.exit(main())
