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
# v2 (lane W16-HD measurement pass): full candidate pool (EXTERNAL + STATIC),
# extent-based sizes, shape prefilter, parallel scoring, two-leg x two-bracket
# control, global-bijective proposal.
# --------------------------------------------------------------------------

import os
import random
import itertools
from multiprocessing import Pool

FUNCLET_PREFIXES = ("__unwind$", "__catch$")
_PLACEHOLDER = ("fn_", "lbl_")


def coff_functions_full(path: Path):
    """Every defined function symbol (type 0x20, section > 0, storage class
    EXTERNAL=2 or STATIC=3) with its EXTENT, i.e. value -> next function-typed
    symbol in the same section (or section end), and its body bytes + the set
    of relocated word offsets inside it.

    Why extent, not section size: 29,700 base-obj function symbols share a
    COMDAT section with their own `__unwind$` funclets (measured 2026-09-30),
    so section size over-states the function by the funclet bytes."""
    d = path.read_bytes()
    _m, nsec, _t, psym, nsym, _o, _c = struct.unpack_from("<HHIIIHH", d, 0)
    secs = {}
    for i in range(nsec):
        o = 20 + i * 40
        _vs, _va, size, ptr, prel, _pl, nrel, _nl, _ch = struct.unpack_from("<IIIIIIHHI", d, o + 8)
        relocs = set()
        for k in range(nrel):
            va_, _sym, _typ = struct.unpack_from("<IIH", d, prel + k * 10)
            relocs.add(va_)
        secs[i + 1] = (d[ptr: ptr + size] if ptr else b"", relocs)
    strtab = psym + nsym * 18
    syms, i = [], 0
    while i < nsym:
        o = psym + i * 18
        raw = d[o: o + 8]
        val, sec, typ, sclass, naux = struct.unpack_from("<IhHBB", d, o + 8)
        if raw[:4] == b"\x00\x00\x00\x00":
            soff = struct.unpack_from("<I", raw, 4)[0]
            end = d.index(b"\x00", strtab + soff)
            name = d[strtab + soff: end].decode("latin1")
        else:
            name = raw.rstrip(b"\x00").decode("latin1")
        if sec > 0 and typ == 0x20 and sclass in (2, 3):
            syms.append((name, sec, sclass, val))
        i += 1 + naux
    by_sec = collections.defaultdict(list)
    for s in syms:
        by_sec[s[1]].append(s)
    out = {}
    for sec, lst in by_sec.items():
        lst.sort(key=lambda s: s[3])
        data, relocs = secs.get(sec, (b"", set()))
        for j, (name, _sec, sclass, val) in enumerate(lst):
            end = lst[j + 1][3] if j + 1 < len(lst) else len(data)
            body = data[val:end]
            rel = {r - val for r in relocs if val <= r < end}
            out[name] = {"sclass": sclass, "size": end - val, "body": body, "relocs": rel}
    return out


def target_body(target_fns_all, target_obj_bytes_cache, name, size):
    """Body of a target row: symbol value + report size (target objs pack
    fn_ rows into shared sections, and report `size` is authoritative)."""
    f = target_fns_all.get(name)
    if f is None:
        return None, set()
    return f["body"][:size], {r for r in f["relocs"] if r < size}


def shape_tokens(body: bytes, relocs: set):
    """Bag of instruction-shape tokens for the cheap prefilter. Per word:
    primary opcode (+ extended opcode for 4/19/31/59/63), plus the 16-bit
    immediate for D-form ops (struct offsets / constants) unless that word is
    relocated, plus opcode bigrams. Registers are deliberately ignored
    (regalloc noise)."""
    c = collections.Counter()
    prev = None
    for off in range(0, len(body) - 3, 4):
        w = struct.unpack_from(">I", body, off)[0]
        op = w >> 26
        tok = (op << 10) | ((w >> 1) & 0x3FF) if op in (4, 19, 31, 59, 63) else (op << 10)
        c[("o", tok)] += 1
        if off in relocs:
            c[("r", op)] += 1
        elif op in (7, 8, 10, 11, 12, 13, 14, 15, 24, 25, 26, 27, 28, 29) or 32 <= op <= 55 or op in (58, 62):
            c[("i", op, w & 0xFFFF)] += 1
        if prev is not None:
            c[("b", prev, tok)] += 1
        prev = tok
    return c


def shape_sim(a, b):
    if not a or not b:
        return 0.0
    inter = sum(min(v, b[k]) for k, v in a.items() if k in b)
    union = sum(a.values()) + sum(b.values()) - inter
    return inter / union if union else 0.0


def applied_map():
    m = json.loads(TARGET_MAP_PATH.read_text())
    rows = {k.lower(): v for k, v in m.items() if k.lower().startswith("0x") and isinstance(v, str) and v}
    nulls = {k.lower() for k, v in m.items() if k.lower().startswith("0x") and v is None}
    deny = set()
    for key in ("_denylist", "_denylist_unadjudicated"):
        for a in m.get(key, []) or []:
            if isinstance(a, str):
                deny.add(a.lower())
    return rows, nulls, deny


def global_claimed_names(report, map_rows):
    """Names that must never be proposed: every map value (the map is
    injective) and every non-placeholder row name anywhere in report.json."""
    claimed = set(map_rows.values())
    for u in report["units"]:
        for f in u.get("functions", []):
            if not f["name"].startswith(_PLACEHOLDER):
                claimed.add(f["name"])
    return claimed


def candidate_pool_v2(base_fns, claimed):
    return {n: f for n, f in base_fns.items()
            if not n.startswith(FUNCLET_PREFIXES) and n not in claimed and f["size"] > 0}


def prefilter(tsize, ttok, pool, k, lo=0.5, hi=2.0):
    """Size band, then rank by shape similarity (ties: size closeness)."""
    band = []
    for n, f in pool.items():
        if tsize <= 0 or not (lo <= f["size"] / tsize <= hi):
            continue
        band.append((shape_sim(ttok, f["tok"]), -abs(f["size"] - tsize), n))
    band.sort(reverse=True)
    return [n for _s, _d, n in band[:k]], len(band)


_JOB_COUNTER = itertools.count()


def _score_job(job):
    """Worker: job = (key, target_obj, base_obj, target_renames|None,
    base_renames, symname). Writes private scratch copies, runs objdiff-cli at
    report.json's ruler, returns (key, fuzzy|None)."""
    key, target_obj, base_obj, t_ren, b_ren, sym = job
    SCRATCH_DIR.mkdir(parents=True, exist_ok=True)
    tag = f"{os.getpid()}_{next(_JOB_COUNTER)}"
    b_path = SCRATCH_DIR / f"{tag}_b.obj"
    t_path = Path(target_obj)
    tmp = [b_path]
    data = bytearray(Path(base_obj).read_bytes())
    if b_ren:
        rename_symbols(data, b_ren)
    b_path.write_bytes(data)
    if t_ren:
        t_path = SCRATCH_DIR / f"{tag}_t.obj"
        tdata = bytearray(Path(target_obj).read_bytes())
        rename_symbols(tdata, t_ren)
        t_path.write_bytes(tdata)
        tmp.append(t_path)
    try:
        proc = subprocess.run(
            [str(OBJDIFF_CLI), "diff", "-1", str(t_path), "-2", str(b_path), sym, "-f", "json",
             *RULER_FLAGS, "--map-file", str(MAP_FILE)],
            cwd=PROJECT_ROOT, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, timeout=120, check=True)
        out = json.loads(proc.stdout.decode("utf-8"))
        res = float(out.get("fuzzy_match_percent", 0.0) or 0.0)
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired, json.JSONDecodeError):
        res = None
    finally:
        for p in tmp:
            p.unlink(missing_ok=True)
    return key, res


def run_jobs(jobs, workers, label):
    results = {}
    n = len(jobs)
    with Pool(workers) as pool:
        for i, (key, res) in enumerate(pool.imap_unordered(_score_job, jobs, chunksize=4), 1):
            results[key] = res
            if i % 500 == 0 or i == n:
                print(f"  [{label}] {i}/{n} scored", file=sys.stderr, flush=True)
    return results


def load_units_v2(report, cfg, map_rows):
    """Game units with parsed base/target function tables and shape tokens."""
    claimed = global_claimed_names(report, map_rows)
    units = []
    for u in game_units(report, cfg):
        b, t = PROJECT_ROOT / u["base_path"], PROJECT_ROOT / u["target_path"]
        if not b.exists() or not t.exists():
            continue
        base_fns = coff_functions_full(b)
        tgt_fns = coff_functions_full(t)
        for f in base_fns.values():
            f["tok"] = shape_tokens(f["body"], f["relocs"])
        u = dict(u)
        u["base_fns"], u["tgt_fns"] = base_fns, tgt_fns
        u["pool"] = candidate_pool_v2(base_fns, claimed)
        units.append(u)
    return units, claimed


def size_bucket(s):
    for lim in (8, 16, 32, 64, 128, 512):
        if s <= lim:
            return f"<={lim}"
    return ">512"


def cmd_census(args):
    report, cfg = load_report(), load_objdiff_cfg()
    map_rows, _nulls, _deny = applied_map()
    units, claimed = load_units_v2(report, cfg, map_rows)
    ext = st = fun = 0
    pool_ext = pool_st = 0
    names_ext, names_st, pool_names = set(), set(), set()
    ntg = ntb = 0
    band_counts = []
    for u in units:
        for n, f in u["base_fns"].items():
            if n.startswith(FUNCLET_PREFIXES):
                fun += 1
                continue
            if f["sclass"] == 2:
                ext += 1; names_ext.add(n)
            else:
                st += 1; names_st.add(n)
        for n, f in u["pool"].items():
            pool_names.add(n)
            if f["sclass"] == 2:
                pool_ext += 1
            else:
                pool_st += 1
        for t in unit_targets(u):
            ntg += 1; ntb += t["size"]
            nb = sum(1 for f in u["pool"].values() if t["size"] > 0 and 0.5 <= f["size"] / t["size"] <= 2.0)
            band_counts.append(nb)
    band_counts.sort()
    med = band_counts[len(band_counts) // 2] if band_counts else 0
    out = {
        "units": len(units),
        "defined_function_symbols": {"external": ext, "static": st, "funclets_dropped": fun,
                                     "distinct_external_names": len(names_ext), "distinct_static_names": len(names_st)},
        "candidate_pool_after_claim_filter": {"external": pool_ext, "static": pool_st, "distinct_names": len(pool_names)},
        "globally_claimed_names": len(claimed),
        "targets": {"rows": ntg, "bytes": ntb},
        "in_size_band_candidates_per_target": {"median": med, "min": band_counts[0] if band_counts else 0,
                                               "max": band_counts[-1] if band_counts else 0,
                                               "zero": sum(1 for x in band_counts if x == 0)},
    }
    print(json.dumps(out, indent=2))
    if args.json_out:
        Path(args.json_out).write_text(json.dumps(out, indent=2))


def cmd_control2(args):
    """Two legs x two brackets on hidden-name control rows.

    Leg IN  : pool + true name; precision = P(top1 == true | rule fires).
    Leg OUT : pool without true name; FP rate = P(rule fires).
    Bracket NAMED : target obj as built (callees named -> name_check charges
                    wrong callees; STRICT).
    Bracket ANON  : every map-named symbol in the target copy renamed back to
                    fn_<addr> (placeholder callees are forgiven; LENIENT, the
                    worst case for false positives).
    Real fn_ targets sit between the two brackets."""
    report, cfg = load_report(), load_objdiff_cfg()
    map_rows, _nulls, _deny = applied_map()
    inv = {v: "fn_" + k[2:].upper() for k, v in map_rows.items()}
    units, _claimed = load_units_v2(report, cfg, map_rows)
    rng = random.Random(args.seed)

    # target size distribution -> stratified control sample
    tgt_bucket = collections.Counter(size_bucket(t["size"]) for u in units for t in unit_targets(u))
    ntg = sum(tgt_bucket.values())
    rows_by_bucket = collections.defaultdict(list)
    for u in units:
        for r in unit_named_rows(u):
            if r["fuzzy"] <= 0 or r["name"] not in u["base_fns"] or r["name"] not in u["tgt_fns"]:
                continue
            if r["name"].startswith(FUNCLET_PREFIXES):
                continue
            rows_by_bucket[size_bucket(r["size"])].append((u["name"], r))
    sample = []
    for b, cnt in tgt_bucket.items():
        want = max(1, round(args.rows * cnt / ntg))
        pop = rows_by_bucket.get(b, [])
        sample += rng.sample(pop, min(want, len(pop)))
    ubyname = {u["name"]: u for u in units}

    jobs, meta = [], []
    rank_of_true = []
    for ci, (uname, r) in enumerate(sample):
        u = ubyname[uname]
        T, size = r["name"], r["size"]
        tf = u["tgt_fns"][T]
        ttok = shape_tokens(tf["body"][:size], {x for x in tf["relocs"] if x < size})
        pool_in = dict(u["pool"])
        pool_in[T] = u["base_fns"][T]
        cands_in, _ = prefilter(size, ttok, pool_in, args.k)
        pool_out = dict(u["pool"])
        pool_out.pop(T, None)
        cands_out, nband = prefilter(size, ttok, pool_out, args.k)
        rank_of_true.append(cands_in.index(T) if T in cands_in else None)
        allc = list(dict.fromkeys(cands_in + cands_out))
        meta.append({"unit": uname, "true": T, "size": size, "in": cands_in, "out": cands_out, "nband": nband})
        base = str(PROJECT_ROOT / u["base_path"])
        tgt = str(PROJECT_ROOT / u["target_path"])
        anon_t = {n: inv[n] for n in u["tgt_fns"] if n in inv}
        for c in allc:
            b_named = {} if c == T else {T: "__w16hd_hidden__", c: T}
            jobs.append(((ci, c, "named"), tgt, base, None, b_named, T))
            anon_name = anon_t[T] if T in anon_t else "fn_W16HDCTRL"
            t_ren = dict(anon_t)
            t_ren[T] = anon_name
            b_anon = {c: anon_name} if c == T else {T: "__w16hd_hidden__", c: anon_name}
            jobs.append(((ci, c, "anon"), tgt, base, t_ren, b_anon, anon_name))
    print(f"control: {len(sample)} rows, {len(jobs)} objdiff jobs, workers={args.workers}", file=sys.stderr)
    res = run_jobs(jobs, args.workers, "control")
    for ci, m in enumerate(meta):
        for br in ("named", "anon"):
            m[f"scores_{br}"] = {c: res.get((ci, c, br)) for c in dict.fromkeys(m["in"] + m["out"])}
    out = {"seed": args.seed, "k": args.k, "rows": meta,
           "true_rank_hist": collections.Counter("absent" if x is None else str(x) for x in rank_of_true)}
    Path(args.json_out).write_text(json.dumps(out, indent=1))
    print(f"wrote {args.json_out}", file=sys.stderr)


def rule_fires(scores, T, M):
    """scores: [(fuzzy, name)] sorted desc. Fires iff top >= T and it beats the
    runner-up by >= M (a lone candidate counts as margin = top)."""
    if not scores:
        return None
    top, name = scores[0]
    second = scores[1][0] if len(scores) > 1 else 0.0
    if top >= T and (top - second) >= M:
        return name
    return None


def evaluate_control(ctrl, thresholds, margins, min_size=0):
    rows = [r for r in ctrl["rows"] if r["size"] >= min_size]
    table = []
    for br in ("named", "anon"):
        for T in thresholds:
            for M in margins:
                fires_in = hits_in = fires_out = 0
                for r in rows:
                    sc = r[f"scores_{br}"]
                    s_in = sorted(((sc[c], c) for c in r["in"] if sc.get(c) is not None), reverse=True)
                    s_out = sorted(((sc[c], c) for c in r["out"] if sc.get(c) is not None), reverse=True)
                    got = rule_fires(s_in, T, M)
                    if got:
                        fires_in += 1
                        hits_in += got == r["true"]
                    if rule_fires(s_out, T, M):
                        fires_out += 1
                n = len(rows)
                table.append({"bracket": br, "T": T, "M": M, "n": n,
                              "fires_in": fires_in, "hits_in": hits_in,
                              "precision_in": hits_in / fires_in if fires_in else None,
                              "recall_in": hits_in / n if n else None,
                              "fires_out": fires_out, "fp_rate_out": fires_out / n if n else None})
    return table


def cmd_evaluate(args):
    ctrl = json.loads(Path(args.control_json).read_text())
    Ts = [float(x) for x in args.thresholds.split(",")]
    Ms = [float(x) for x in args.margins.split(",")]
    table = evaluate_control(ctrl, Ts, Ms, args.min_size)
    print(f"true-name rank in prefilter top-{ctrl['k']}: {dict(ctrl['true_rank_hist'])}")
    print(f"{'br':>5} {'T':>6} {'M':>5} {'n':>5} {'fireIN':>6} {'prec':>7} {'recall':>7} {'fireOUT':>7} {'FP':>6}")
    for t in table:
        p = f"{100*t['precision_in']:.2f}" if t["precision_in"] is not None else "  n/a"
        print(f"{t['bracket']:>5} {t['T']:>6} {t['M']:>5} {t['n']:>5} {t['fires_in']:>6} {p:>7} "
              f"{100*t['recall_in']:>6.2f} {t['fires_out']:>7} {100*t['fp_rate_out']:>5.2f}")


def cmd_propose(args):
    report, cfg = load_report(), load_objdiff_cfg()
    map_rows, nulls, deny = applied_map()
    units, _claimed = load_units_v2(report, cfg, map_rows)
    jobs, meta = [], []
    skipped = collections.Counter()
    for u in units:
        base = str(PROJECT_ROOT / u["base_path"])
        tgt = str(PROJECT_ROOT / u["target_path"])
        for t in unit_targets(u):
            addr = "0x" + t["name"].split("_", 1)[1].lower()
            if addr in map_rows:
                skipped["already_in_map"] += 1; continue
            if addr in nulls:
                skipped["deliberately_null_in_map"] += 1; continue
            if addr in deny:
                skipped["denylisted"] += 1; continue
            tf = u["tgt_fns"].get(t["name"])
            if tf is None:
                skipped["no_target_symbol"] += 1; continue
            ttok = shape_tokens(tf["body"][: t["size"]], {x for x in tf["relocs"] if x < t["size"]})
            cands, nband = prefilter(t["size"], ttok, u["pool"], args.k)
            if not cands:
                skipped["no_candidate_in_band"] += 1; continue
            ti = len(meta)
            meta.append({"unit": u["name"], "target": t["name"], "addr": addr, "size": t["size"],
                         "cands": cands, "nband": nband})
            for c in cands:
                jobs.append(((ti, c), tgt, base, None, {c: t["name"]}, t["name"]))
    print(f"propose: {len(meta)} targets, {len(jobs)} jobs, skipped={dict(skipped)}", file=sys.stderr)
    res = run_jobs(jobs, args.workers, "propose")
    fired = []
    for ti, m in enumerate(meta):
        sc = sorted(((res.get((ti, c)), c) for c in m["cands"] if res.get((ti, c)) is not None), reverse=True)
        m["scores"] = [[c, f] for f, c in sc]
        got = rule_fires(sc, args.threshold, args.margin) if m["size"] >= args.min_size else None
        if got:
            fired.append((m, got, sc[0][0]))
    # global bijection: a name proposed for >1 address is dropped everywhere
    by_name = collections.Counter(g for _m, g, _s in fired)
    proposals, conflicts = [], []
    for m, g, s in fired:
        rec = {"addr": m["addr"], "target": m["target"], "unit": m["unit"], "size": m["size"],
               "name": g, "fuzzy": s,
               "runner_up": (m["scores"][1] if len(m["scores"]) > 1 else None)}
        (conflicts if by_name[g] > 1 else proposals).append(rec)
    proposals.sort(key=lambda r: r["addr"])
    out = {
        "rule": {"threshold": args.threshold, "margin": args.margin, "min_size": args.min_size, "k": args.k},
        "skipped": dict(skipped), "targets_scored": len(meta),
        "proposal_count": len(proposals), "proposal_bytes": sum(p["size"] for p in proposals),
        "dropped_non_bijective": conflicts,
        "proposals": proposals,
        "all_scored": meta,
    }
    Path(args.json_out).write_text(json.dumps(out, indent=1))
    print(f"{len(proposals)} proposals / {out['proposal_bytes']} B; {len(conflicts)} dropped non-bijective; wrote {args.json_out}")


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

    p_c = sub.add_parser("census", help="v2: count targets and the candidate pool (EXTERNAL + STATIC)")
    p_c.add_argument("--json-out")
    p_c.set_defaults(func=cmd_census)

    p_c2 = sub.add_parser("control2", help="v2: parallel two-leg x two-bracket hidden-name control")
    p_c2.add_argument("--rows", type=int, default=2000)
    p_c2.add_argument("--k", type=int, default=6)
    p_c2.add_argument("--seed", type=int, default=20260930)
    p_c2.add_argument("--workers", type=int, default=16)
    p_c2.add_argument("--json-out", required=True)
    p_c2.set_defaults(func=cmd_control2)

    p_ev = sub.add_parser("evaluate", help="v2: precision / FP table from a control2 JSON")
    p_ev.add_argument("control_json")
    p_ev.add_argument("--thresholds", default="80,90,95,97,98,99,99.5,100")
    p_ev.add_argument("--margins", default="0,0.5,1,2,5")
    p_ev.add_argument("--min-size", type=int, default=0)
    p_ev.set_defaults(func=cmd_evaluate)

    p_pr = sub.add_parser("propose", help="v2: score all real fn_ targets and emit proposals at a rule")
    p_pr.add_argument("--threshold", type=float, required=True)
    p_pr.add_argument("--margin", type=float, required=True)
    p_pr.add_argument("--min-size", type=int, default=0)
    p_pr.add_argument("--k", type=int, default=6)
    p_pr.add_argument("--workers", type=int, default=16)
    p_pr.add_argument("--json-out", required=True)
    p_pr.set_defaults(func=cmd_propose)

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
