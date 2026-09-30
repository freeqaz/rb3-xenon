#!/usr/bin/env python3
"""Score candidate identities for anonymous (`fn_<addr>`, fuzzy 0) retail rows
WITHOUT touching the repo's map, by scratch-renaming a candidate symbol inside
a throwaway copy of our compiled .obj and asking objdiff-cli for the fuzzy
score that rename would produce.

Why this exists
----------------
`fn_<addr>` rows never pair with a real function under objdiff's normal
auto-pairing (name equality) -- they only ever disclose via `masked_equal`
(byte-identical EH funclets). The only way to see what score a candidate
identity would earn is to give the target row that exact name and let
objdiff diff it against our object -- and the only way to do THAT without
mutating `scripts/target_symbol_map.json` (which the whole tree builds
against) is to rename the CANDIDATE inside a private copy of our .obj under
~/tmp, and diff that copy directly against the untouched target .obj with
`objdiff-cli diff -1/-2`.

Mechanism note (corrects the original brief)
---------------------------------------------
The brief proposed doing this via `ProjectObject.symbol_mappings`
(objdiff-core config/mod.rs) and a scratch `objdiff.json`. Verified by
reading objdiff-cli source (`report.rs`, `diff.rs`): NEITHER `report
generate` NOR `diff` ever reads `symbol_mappings` -- both build
`MappingConfig` with `mappings` left at its `Default` (empty) value. It is a
GUI-only field. The COFF scratch-rename technique below was built instead,
and is verified byte-for-byte to reproduce `report.json`'s own
`fuzzy_match_percent` (see `--verify-technique`).

Because `-1/-2` direct-object mode does NOT load `objdiff.json`'s project
`options`/`map_file`, every invocation here passes the same four ruler-
critical `-c` flags `report.json`'s `provenance.diff_config` pins
(`functionRelocDiffs=name_check`, `combineDataSections=true`,
`combineTextSections=true`, `ppc.calculatePoolRelocations=false`) plus
`--map-file build/45410914/icf_aliases.map` explicitly.

Population (game layer, this lane's scope)
-------------------------------------------
Units with a compiled base object (`objdiff.json` `base_path` set) whose
`metadata.source_path` starts with `src/band3/` or `src/network/` but NOT
`src/network/quazal/` (Quazal middleware is 7-line namespace shells per
CLAUDE.md -- out of scope), excluding `default/auto_*` and `/xdk` units, and
excluding any unit lane W16-HB is naming (`default/TrackWidget*`,
`default/MidiParser`).

Candidates, for a given unit
-----------------------------
Function symbols (COFF storage class EXTERNAL == 2, defined in a real
section, type == 0x20) DEFINED in that unit's compiled base .obj whose name
is not already the name of any non-`fn_`/`lbl_` row in `report.json` for
that unit -- i.e. every one of our compiled functions objdiff has not yet
been given a retail address to pair with.

Prefilter (cheap, documented, not a scoring step)
---------------------------------------------------
Candidate considered for a target only if
    0.5 <= candidate_size / target_size <= 2.0
(both from COFF section size / report row `size`). This is a coarse gate,
not a proxy for correctness -- it exists purely to keep the number of
objdiff-cli invocations (each ~0.35 s) bounded. Within the surviving set,
only the `--max-candidates-per-target` closest-by-size candidates are
actually scored (default 8).

Modes
-----
  control   -- precision control: hide the true name of already-paired rows
               in the population units and see if the scorer's own top-1
               choice recovers it. Prints top-1 accuracy by score band.
  score     -- score real fn_ targets in given unit(s) against candidates,
               report bijective greedy assignments above --threshold.
  --selftest -- fast, no build required: exercises rename_symbols() /
               COFF parsing on a tiny synthetic buffer, AND a live hold-out
               check (see run_selftest docstring) that must refuse to name
               a target once its true candidate is removed from the pool.
"""

import argparse
import collections
import json
import shutil
import struct
import subprocess
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
OBJDIFF_CLI = PROJECT_ROOT / "bin" / "objdiff-cli"
REPORT_PATH = PROJECT_ROOT / "build" / "45410914" / "report.json"
OBJDIFF_CFG_PATH = PROJECT_ROOT / "objdiff.json"
MAP_FILE = PROJECT_ROOT / "build" / "45410914" / "icf_aliases.map"
TARGET_MAP_PATH = PROJECT_ROOT / "scripts" / "target_symbol_map.json"
SCRATCH_DIR = Path.home() / "tmp" / "w16hd" / "scratch_objs"

# The four ruler-critical keys report.json's provenance.diff_config pins
# (docs: CLAUDE.md "objdiff pattern-doc links" / two-objdiff-entry-points doc).
# `-1/-2` direct mode never reads objdiff.json, so these are passed by hand
# on every invocation instead.
RULER_FLAGS = [
    "-c", "functionRelocDiffs=name_check",
    "-c", "combineDataSections=true",
    "-c", "combineTextSections=true",
    "-c", "ppc.calculatePoolRelocations=false",
]

SKIP_UNIT_PREFIXES = ("default/TrackWidget", "default/MidiParser")  # owned by W16-HB


# --------------------------------------------------------------------------
# COFF parsing (function-symbol extraction + generic rename, reused from
# scripts/obj_target_symbol_renamer.py's rename_symbols so the scratch copy
# is produced by the exact same code path that patches real objs).
# --------------------------------------------------------------------------

sys.path.insert(0, str(PROJECT_ROOT / "scripts"))
from obj_target_symbol_renamer import rename_symbols  # noqa: E402


def coff_parse(path: Path):
    """Return (sections, fns) where sections[idx] = {'data':bytes,'relocs':set}
    and fns = [(name, section_idx), ...] for EXTERNAL defined function symbols
    (storage class 2, section > 0, type 0x20). Mirrors tools/body_match.py's
    parse() -- kept independent (not imported) because body_match.py's `parse`
    is a script-local helper, not a stable importable API."""
    d = path.read_bytes()
    _m, nsec, _t, psym, nsym, _o, _c = struct.unpack_from("<HHIIIHH", d, 0)
    secs = {}
    for i in range(nsec):
        o = 20 + i * 40
        _vs, _va, size, ptr, prel, _pl, nrel, _nl, _ch = struct.unpack_from(
            "<IIIIIIHHI", d, o + 8
        )
        relocs = set()
        for k in range(nrel):
            va_, _sym, _typ = struct.unpack_from("<IIH", d, prel + k * 10)
            relocs.add(va_)
        secs[i + 1] = {"data": d[ptr : ptr + size] if ptr else b"", "relocs": relocs}
    strtab = psym + nsym * 18
    fns, i = [], 0
    while i < nsym:
        o = psym + i * 18
        raw = d[o : o + 8]
        _val, sec, typ, sclass, naux = struct.unpack_from("<IhHBB", d, o + 8)
        if raw[:4] == b"\x00\x00\x00\x00":
            soff = struct.unpack_from("<I", raw, 4)[0]
            end = d.index(b"\x00", strtab + soff)
            name = d[strtab + soff : end].decode("latin1")
        else:
            name = raw.rstrip(b"\x00").decode("latin1")
        if sclass == 2 and sec > 0 and typ == 0x20:
            fns.append((name, sec))
        i += 1 + naux
    return secs, fns


def coff_function_sizes(obj_path: Path) -> dict:
    """name -> COMDAT section byte size, for every defined EXTERNAL function
    symbol in obj_path."""
    secs, fns = coff_parse(obj_path)
    return {name: len(secs[sec]["data"]) for name, sec in fns}


# --------------------------------------------------------------------------
# report.json / objdiff.json plumbing
# --------------------------------------------------------------------------


def load_report():
    return json.loads(REPORT_PATH.read_text())


def load_objdiff_cfg():
    return json.loads(OBJDIFF_CFG_PATH.read_text())


def game_units(report=None, cfg=None, skip_prefixes=SKIP_UNIT_PREFIXES):
    """Every unit in scope for this lane: has a compiled base object, source
    path under src/band3/ or src/network/ (excluding src/network/quazal/),
    not auto_*/xdk, not on the skip list."""
    report = report or load_report()
    cfg = cfg or load_objdiff_cfg()
    base_by_unit = {u["name"]: u for u in cfg["units"]}
    out = []
    for u in report["units"]:
        name = u["name"]
        if name.startswith("default/auto_") or "/xdk" in name:
            continue
        if name.startswith(skip_prefixes):
            continue
        cu = base_by_unit.get(name)
        if not cu or not cu.get("base_path"):
            continue
        src = (cu.get("metadata") or {}).get("source_path", "")
        if not (src.startswith("src/band3/") or src.startswith("src/network/")):
            continue
        if src.startswith("src/network/quazal/"):
            continue
        out.append(
            {
                "name": name,
                "source_path": src,
                "base_path": cu["base_path"],
                "target_path": cu["target_path"],
                "functions": u.get("functions", []),
            }
        )
    return out


def unit_targets(unit):
    """fn_/lbl_-named rows at fuzzy 0.0 -- the naming backlog for this unit."""
    out = []
    for f in unit["functions"]:
        nm = f["name"]
        if nm.startswith(("fn_", "lbl_")) and float(f.get("fuzzy_match_percent", 0) or 0) == 0.0:
            out.append({"name": nm, "size": int(f["size"])})
    return out


def unit_named_rows(unit):
    """Rows already carrying a real (non-fn_/lbl_) name -- used both to
    compute the unclaimed-candidate set and as the control population."""
    out = []
    for f in unit["functions"]:
        nm = f["name"]
        if not nm.startswith(("fn_", "lbl_")):
            out.append(
                {
                    "name": nm,
                    "size": int(f["size"]),
                    "fuzzy": float(f.get("fuzzy_match_percent", 0) or 0),
                }
            )
    return out


def unclaimed_candidates(unit):
    """name -> coff size, for base-obj function symbols not already the name
    of a paired row in this unit."""
    base_obj = PROJECT_ROOT / unit["base_path"]
    sizes = coff_function_sizes(base_obj)
    claimed = {r["name"] for r in unit_named_rows(unit)}
    return {n: s for n, s in sizes.items() if n not in claimed}


# --------------------------------------------------------------------------
# Scratch-rename scoring
# --------------------------------------------------------------------------


def score_candidate(target_obj: Path, base_obj: Path, candidate_name: str, target_name: str,
                     scratch_dir: Path = SCRATCH_DIR):
    """Score the hypothesis "candidate_name IS target_name" by writing a
    scratch copy of base_obj with candidate_name renamed to target_name, then
    diffing it against target_obj at exactly report.json's ruler. Returns a
    dict with fuzzy_match_percent / canonical_match_percent (mpn) / raw sizes,
    or None if the objdiff invocation failed."""
    scratch_dir.mkdir(parents=True, exist_ok=True)
    scratch_obj = scratch_dir / f"scratch_{abs(hash((str(base_obj), candidate_name, target_name)))}.obj"
    data = bytearray(base_obj.read_bytes())
    n, _details = rename_symbols(data, {candidate_name: target_name})
    if n == 0:
        return None  # candidate name not found in base obj (shouldn't happen)
    scratch_obj.write_bytes(data)
    try:
        proc = subprocess.run(
            [
                str(OBJDIFF_CLI), "diff",
                "-1", str(target_obj),
                "-2", str(scratch_obj),
                target_name,
                "-f", "json",
                *RULER_FLAGS,
                "--map-file", str(MAP_FILE),
            ],
            cwd=PROJECT_ROOT,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,  # the --map-file eprintln would corrupt JSON on stdout otherwise
            timeout=30,
            check=True,
        )
        out = json.loads(proc.stdout.decode("utf-8"))
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired, json.JSONDecodeError):
        return None
    finally:
        scratch_obj.unlink(missing_ok=True)
    return {
        "fuzzy": float(out.get("fuzzy_match_percent", 0.0) or 0.0),
        "mpn": float(out.get("canonical_match_percent", 0.0) or 0.0),
        "target_size": out.get("target_size"),
        "base_size": out.get("base_size"),
    }


def candidates_in_band(target_size, candidates: dict, lo=0.5, hi=2.0, max_n=8):
    """Cheap size-band prefilter + cap. Returns [(name, size), ...] sorted by
    |size - target_size| ascending, capped at max_n."""
    band = [
        (name, size)
        for name, size in candidates.items()
        if target_size > 0 and lo <= (size / target_size) <= hi
    ]
    band.sort(key=lambda ns: abs(ns[1] - target_size))
    return band[:max_n]


# --------------------------------------------------------------------------
# Control: hide a known name, see if the scorer's top-1 recovers it.
# --------------------------------------------------------------------------


def run_control(units, max_n=8, max_rows_per_unit=None, verbose=True):
    """For each already-named row in each unit, hide its true name and score
    it against every unclaimed candidate in that unit's size band PLUS its
    own true name (added back in, since the true name is itself "unclaimed"
    from the hidden row's point of view). Record whether the top-scoring
    candidate is the true name, bucketed by that top score."""
    bands = [(95, 100.0001), (90, 95), (80, 90), (60, 80), (0, 60)]
    band_counts = {b: [0, 0] for b in bands}  # (hits, total)
    rows_scored = 0
    details = []

    for unit in units:
        base_obj = PROJECT_ROOT / unit["base_path"]
        target_obj = PROJECT_ROOT / unit["target_path"]
        if not base_obj.exists() or not target_obj.exists():
            continue
        named = unit_named_rows(unit)
        # only rows objdiff currently pairs at all (fuzzy > 0) make sense as
        # controls -- a fuzzy==0 named row would be a real defect, not a
        # useful "true identity" to hide.
        named = [r for r in named if r["fuzzy"] > 0]
        if max_rows_per_unit:
            named = named[:max_rows_per_unit]
        unclaimed = unclaimed_candidates(unit)
        for row in named:
            true_name, true_size = row["name"], row["size"]
            pool = dict(unclaimed)
            pool[true_name] = true_size  # true identity is a legitimate candidate
            cands = candidates_in_band(true_size, pool, max_n=max_n)
            if len(cands) < 2:
                continue  # no real discrimination test if only one candidate
            scored = []
            for cname, _csize in cands:
                res = score_candidate(target_obj, base_obj, cname, true_name)
                if res is not None:
                    scored.append((res["fuzzy"], cname))
            if not scored:
                continue
            scored.sort(reverse=True)
            top_score, top_name = scored[0]
            rows_scored += 1
            hit = top_name == true_name
            for lo, hi in bands:
                if lo <= top_score < hi:
                    band_counts[(lo, hi)][1] += 1
                    if hit:
                        band_counts[(lo, hi)][0] += 1
                    break
            details.append((unit["name"], true_name, top_name, top_score, hit))
            if verbose and rows_scored % 25 == 0:
                print(f"  ... {rows_scored} control rows scored", file=sys.stderr)

    return band_counts, details, rows_scored


def print_control_report(band_counts, rows_scored):
    print(f"\nControl: {rows_scored} rows scored (top-1 recovery test)")
    print(f"{'band':>12} {'hits':>6} {'total':>6} {'precision':>10}")
    for (lo, hi), (hits, total) in sorted(band_counts.items(), reverse=True):
        prec = (hits / total * 100.0) if total else float("nan")
        label = f"[{lo},{hi})" if hi < 100.0001 else f"[{lo},100]"
        print(f"{label:>12} {hits:>6} {total:>6} {prec:>9.1f}%")


def choose_threshold(band_counts, min_precision=98.0):
    """Pick the lowest score threshold whose cumulative (>= threshold)
    precision is >= min_precision. Returns (threshold, hits, total, precision)
    or None if no band clears the bar."""
    bands = sorted(band_counts.items(), key=lambda kv: -kv[0][0])  # highest band first
    cum_hits = cum_total = 0
    best = None
    for (lo, hi), (hits, total) in bands:
        cum_hits += hits
        cum_total += total
        if cum_total == 0:
            continue
        prec = cum_hits / cum_total * 100.0
        if prec >= min_precision:
            best = (lo, cum_hits, cum_total, prec)
    return best


# --------------------------------------------------------------------------
# Real scoring + bijective assignment
# --------------------------------------------------------------------------


def score_unit_targets(unit, max_n=8, verbose=True):
    """Score every fn_ target row in unit against its unclaimed-candidate
    pool. Returns list of (target_name, target_size, candidate_name, fuzzy)."""
    base_obj = PROJECT_ROOT / unit["base_path"]
    target_obj = PROJECT_ROOT / unit["target_path"]
    targets = unit_targets(unit)
    candidates = unclaimed_candidates(unit)
    results = []
    for t in targets:
        cands = candidates_in_band(t["size"], candidates, max_n=max_n)
        for cname, _csize in cands:
            res = score_candidate(target_obj, base_obj, cname, t["name"])
            if res is not None:
                results.append((t["name"], t["size"], cname, res["fuzzy"]))
        if verbose:
            print(f"  {unit['name']}: {t['name']} ({t['size']} B) -- {len(cands)} candidates scored",
                  file=sys.stderr)
    return results


def bijective_assign(scored, threshold):
    """scored: list of (target, target_size, candidate, fuzzy).
    Greedy highest-score-first, each target and each candidate used once.
    Returns (assignments: {target: (candidate, fuzzy)}, ambiguous: list)."""
    above = [s for s in scored if s[3] >= threshold]
    above.sort(key=lambda s: -s[3])
    used_targets, used_candidates = set(), set()
    assignments = {}
    ambiguous = []
    # detect targets/candidates with more than one above-threshold partner
    per_target = collections.defaultdict(list)
    per_candidate = collections.defaultdict(list)
    for t, _ts, c, f in above:
        per_target[t].append((f, c))
        per_candidate[c].append((f, t))
    for t, _ts, c, f in above:
        if t in used_targets or c in used_candidates:
            continue
        if len(per_target[t]) > 1 or len(per_candidate[c]) > 1:
            ambiguous.append((t, c, f, per_target[t], per_candidate[c]))
            continue
        assignments[t] = (c, f)
        used_targets.add(t)
        used_candidates.add(c)
    return assignments, ambiguous


# --------------------------------------------------------------------------
# CLI
# --------------------------------------------------------------------------


def cmd_control(args):
    report = load_report()
    cfg = load_objdiff_cfg()
    units = game_units(report, cfg)
    if args.units:
        wanted = set(args.units.split(","))
        units = [u for u in units if u["name"] in wanted]
    else:
        units = units[: args.unit_limit]
    print(f"Running control on {len(units)} unit(s)...", file=sys.stderr)
    band_counts, details, n = run_control(units, max_n=args.max_candidates, max_rows_per_unit=args.max_rows_per_unit)
    print_control_report(band_counts, n)
    choice = choose_threshold(band_counts, args.min_precision)
    if choice:
        lo, hits, total, prec = choice
        print(f"\nChosen threshold: score >= {lo}  (cumulative {hits}/{total} = {prec:.2f}% >= {args.min_precision}%)")
    else:
        print(f"\nNo threshold reaches {args.min_precision}% precision on this control -- DO NOT NAME ANYTHING.")
    if args.dump_misses:
        print("\nMisses (top-1 != true name):")
        for unit_name, true_name, top_name, top_score, hit in details:
            if not hit:
                print(f"  {unit_name}: true={true_name} top={top_name} score={top_score:.2f}")


def cmd_score(args):
    report = load_report()
    cfg = load_objdiff_cfg()
    units = game_units(report, cfg)
    wanted = set(args.units.split(","))
    units = [u for u in units if u["name"] in wanted]
    if not units:
        print("No matching units.", file=sys.stderr)
        sys.exit(1)
    all_scored = []
    for u in units:
        scored = score_unit_targets(u, max_n=args.max_candidates)
        all_scored.extend(scored)
    assignments, ambiguous = bijective_assign(all_scored, args.threshold)
    print(f"\n{len(assignments)} admitted assignment(s) at threshold {args.threshold}:")
    for t, (c, f) in sorted(assignments.items(), key=lambda kv: -kv[1][1]):
        print(f"  {t}  ->  {c}   (fuzzy {f:.2f})")
    if ambiguous:
        print(f"\n{len(ambiguous)} ambiguous case(s) (NOT assigned):")
        for t, c, f, pt, pc in ambiguous:
            print(f"  {t} <-> {c} (fuzzy {f:.2f}); target has {len(pt)} candidate(s), candidate matched by {len(pc)} target(s)")
    if args.json_out:
        Path(args.json_out).write_text(json.dumps(
            {"assignments": {t: {"candidate": c, "fuzzy": f} for t, (c, f) in assignments.items()},
             "ambiguous": [{"target": t, "candidate": c, "fuzzy": f} for t, c, f, _pt, _pc in ambiguous]},
            indent=2))
        print(f"\nWrote {args.json_out}")


def run_selftest():
    """No build required beyond what's already built. Two checks:

    1. Pure COFF round-trip: rename_symbols() on a tiny synthetic COFF-like
       buffer must actually change the symbol name findable by coff_parse().
    2. Live hold-out (requires build/45410914 + a compiled object to exist,
       which it does on any built tree): pick one unit with >=2 unclaimed
       candidates and >=1 fuzzy>0 named row, hide the true name AS USUAL but
       additionally DELETE it from the candidate pool entirely (simulating
       "we don't have this function compiled at all"). The scorer must NOT
       report a top score at/above the chosen threshold with high confidence
       -- i.e. bijective_assign() must NOT assign that target once its only
       correct candidate is unavailable. This is the required "a hold-out
       case whose true name is removed from the candidate set must NOT be
       assigned" check.

    Exits 1 and prints a diagnosis on failure.
    """
    ok = True

    # --- Check 1: COFF rename round-trip on a synthetic buffer -----------
    # Build a minimal COFF object: header + 0 sections + 2 symbols (short-name
    # "AAAAAAA\0" and a placeholder), enough for rename_symbols()/coff_parse()
    # to exercise their string-table growth path.
    nsyms = 2
    sym_offset = 20  # right after the 20-byte file header, 0 sections
    header = struct.pack("<HHIIIHH", 0x01F2, 0, 0, sym_offset, nsyms, 0, 0)
    sym1 = b"OLDNAME\x00" + struct.pack("<IhHBB", 0, 1, 0x20, 2, 0)  # short name, EXTERNAL fn, sec 1 (nonexistent but fine for name test)
    sym2 = b"OTHER\x00\x00\x00" + struct.pack("<IhHBB", 0, 1, 0x20, 2, 0)
    strtab = struct.pack("<I", 4)  # empty string table (just its own length prefix)
    buf = bytearray(header + sym1 + sym2 + strtab)
    n, details = rename_symbols(buf, {"OLDNAME": "?NewName@@YAXXZ"})
    if n != 1:
        print(f"SELFTEST FAIL: rename_symbols renamed {n} symbols, expected 1", file=sys.stderr)
        ok = False
    else:
        _secs, fns = coff_parse_bytes(bytes(buf))
        names = {name for name, _sec in fns}
        if "?NewName@@YAXXZ" not in names or "OLDNAME" in names:
            print(f"SELFTEST FAIL: post-rename symbol names = {names}", file=sys.stderr)
            ok = False
        else:
            print("SELFTEST PASS: COFF rename round-trip (1 symbol renamed, verified via re-parse)")

    # --- Check 2: live hold-out must NOT be assigned once its true candidate
    #     is removed from the pool. -----------------------------------------
    if not REPORT_PATH.exists() or not OBJDIFF_CLI.exists():
        print("SELFTEST SKIP: live hold-out check needs a built tree + objdiff-cli "
              "(not present) -- run from a built worktree for the full selftest.",
              file=sys.stderr)
    else:
        report = load_report()
        cfg = load_objdiff_cfg()
        units = game_units(report, cfg)
        holdout_unit = None
        holdout_row = None
        for u in units:
            base_obj = PROJECT_ROOT / u["base_path"]
            target_obj = PROJECT_ROOT / u["target_path"]
            if not base_obj.exists() or not target_obj.exists():
                continue
            named = [r for r in unit_named_rows(u) if r["fuzzy"] > 0]
            unclaimed = unclaimed_candidates(u)
            for row in named:
                cands = candidates_in_band(row["size"], unclaimed, max_n=6)
                if len(cands) >= 1:
                    holdout_unit, holdout_row = u, row
                    break
            if holdout_row:
                break
        if not holdout_row:
            print("SELFTEST SKIP: no suitable hold-out row found (no unit had both "
                  "a fuzzy>0 named row and >=1 unclaimed candidate in its size band).",
                  file=sys.stderr)
        else:
            base_obj = PROJECT_ROOT / holdout_unit["base_path"]
            target_obj = PROJECT_ROOT / holdout_unit["target_path"]
            true_name, true_size = holdout_row["name"], holdout_row["size"]
            unclaimed = unclaimed_candidates(holdout_unit)
            # deliberately do NOT add true_name back into the pool -- this is
            # the "true candidate absent entirely" hold-out.
            cands = candidates_in_band(true_size, unclaimed, max_n=6)
            scored = []
            for cname, _csize in cands:
                res = score_candidate(target_obj, base_obj, cname, true_name)
                if res is not None:
                    scored.append((true_name, true_size, cname, res["fuzzy"]))
            assignments, _ambiguous = bijective_assign(scored, threshold=80.0)
            if true_name in assignments:
                cand, score = assignments[true_name]
                print(f"SELFTEST FAIL: hold-out {true_name} (true identity {holdout_row['name']!r} "
                      f"withheld from pool) was WRONGLY assigned to {cand} at score {score:.2f}",
                      file=sys.stderr)
                ok = False
            else:
                print(f"SELFTEST PASS: hold-out {true_name} in {holdout_unit['name']} correctly "
                      f"got NO assignment once its true candidate was removed from the pool "
                      f"({len(scored)} wrong candidates scored, none accepted as bijective at 80.0)")

    if not ok:
        sys.exit(1)
    print("\nSELFTEST: all checks passed.")


def coff_parse_bytes(data: bytes):
    """Like coff_parse() but takes raw bytes (for the synthetic selftest
    buffer, which is never written to disk)."""
    d = data
    _m, nsec, _t, psym, nsym, _o, _c = struct.unpack_from("<HHIIIHH", d, 0)
    secs = {}
    strtab = psym + nsym * 18
    fns, i = [], 0
    while i < nsym:
        o = psym + i * 18
        raw = d[o : o + 8]
        _val, sec, typ, sclass, naux = struct.unpack_from("<IhHBB", d, o + 8)
        if raw[:4] == b"\x00\x00\x00\x00":
            soff = struct.unpack_from("<I", raw, 4)[0]
            end = d.index(b"\x00", strtab + soff)
            name = d[strtab + soff : end].decode("latin1")
        else:
            name = raw.rstrip(b"\x00").decode("latin1")
        if sclass == 2 and sec > 0 and typ == 0x20:
            fns.append((name, sec))
        i += 1 + naux
    return secs, fns


def cmd_verify_technique(_args):
    """Sanity check: score a KNOWN pairing (rename a symbol to its own name --
    a no-op rename) and confirm the resulting fuzzy score equals report.json's
    own fuzzy_match_percent for that row exactly. This is the check that
    validated the whole technique during development."""
    report = load_report()
    cfg = load_objdiff_cfg()
    base_by_unit = {u["name"]: u for u in cfg["units"]}
    checked = 0
    mismatches = 0
    for u in report["units"][:40]:
        cu = base_by_unit.get(u["name"])
        if not cu or not cu.get("base_path"):
            continue
        base_obj = PROJECT_ROOT / cu["base_path"]
        target_obj = PROJECT_ROOT / cu["target_path"]
        if not base_obj.exists() or not target_obj.exists():
            continue
        for f in u["functions"]:
            nm = f["name"]
            if nm.startswith(("fn_", "lbl_")):
                continue
            expected = float(f.get("fuzzy_match_percent", 0) or 0)
            if expected <= 0 or expected >= 100:
                continue
            res = score_candidate(target_obj, base_obj, nm, nm)
            checked += 1
            if res is None or abs(res["fuzzy"] - expected) > 0.01:
                mismatches += 1
                print(f"MISMATCH {u['name']} {nm}: report={expected} scorer={res}")
            if checked >= args_verify_limit:
                break
        if checked >= args_verify_limit:
            break
    print(f"\nverify-technique: {checked} checked, {mismatches} mismatch(es).")
    if mismatches:
        sys.exit(1)


args_verify_limit = 15


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--selftest", action="store_true", help="run the fast self-test and exit")
    sub = ap.add_subparsers(dest="cmd")

    p_control = sub.add_parser("control", help="precision control (hide known names, measure top-1 recovery)")
    p_control.add_argument("--units", help="comma-separated unit names (default: first --unit-limit game units)")
    p_control.add_argument("--unit-limit", type=int, default=20)
    p_control.add_argument("--max-candidates", type=int, default=8)
    p_control.add_argument("--max-rows-per-unit", type=int, default=None)
    p_control.add_argument("--min-precision", type=float, default=98.0)
    p_control.add_argument("--dump-misses", action="store_true")
    p_control.set_defaults(func=cmd_control)

    p_score = sub.add_parser("score", help="score real fn_ targets in given unit(s)")
    p_score.add_argument("--units", required=True, help="comma-separated unit names")
    p_score.add_argument("--max-candidates", type=int, default=8)
    p_score.add_argument("--threshold", type=float, required=True)
    p_score.add_argument("--json-out")
    p_score.set_defaults(func=cmd_score)

    p_verify = sub.add_parser("verify-technique", help="sanity-check the scorer against report.json")
    p_verify.set_defaults(func=cmd_verify_technique)

    args = ap.parse_args()
    if args.selftest:
        run_selftest()
        return
    if not args.cmd:
        ap.print_help()
        sys.exit(2)
    args.func(args)


if __name__ == "__main__":
    main()
