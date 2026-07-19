#!/usr/bin/env python3
"""Batch divergence-triage classifier for the rb3-xenon decomp pool.

Buckets named divergent functions into a fixed taxonomy so the project can
price automation fleets. NO LLM calls -- pure local analysis over objdiff-cli
JSON output. STRICTLY READ-ONLY over build artifacts (never --build, never
ninja, never touches source/splits/maps).

TAXONOMY (bucket -> recommended fleet)
Zero band (pool pct == 0):
  ZERO-UNMAPPED      default/auto_* span, or symbol not in target index -> splits/mapping
  ZERO-COMPILE-FAIL  wired TU, base .obj missing, all pool symbols emit nothing -> build fix
  (real TU but our base obj emits nothing = COMDAT scattered) sub-split into:
    ZS-STL-HELPER            @stlpmtx_std@ / STL container helper -> ICF-merged, skip
    ZS-MISSING-INSTANTIATION template inst missing from a wired unit -> probe (forced inst)
    ZS-UNWIRED-OWNSPAN       non-template, unit not in objects.json (own-span .cpp) -> wire
    ZS-OTHER                 wired inline-only / everything else -> manual triage
  (Z4)               pool 0 but batch base_size>0: routed through nonzero rules on live fuzzy%.
Nonzero band (0<pct<100), rules applied in order:
  MISPAIR            size ratio/delta mismatch (renamer paired wrong fn) -> renamer/map fix
  RELOC-COLOC        all LINKER_MERGED/ADDRESS_RELOCATION_NOISE or AT_LIMIT -> skip (at-limit)
  STRUCT-ARTIFACT    dominant uniform displacement delta -> struct stream
  FORM-DIVERGENCE    same opcodes, reordered/reg-swapped -> crack-farm / pattern families
  BODY-PORT          real insert/delete clusters (logic divergence) -> LLM grind
  NEEDS-REVIEW       none of the above -> manual triage

LAYERS: L1 = one batch run over the whole selected pool
    cat symbols.txt | ./bin/objdiff-cli diff -p . --batch --verdict -f json
L2 (undecided subset only) = per-symbol instruction fetch, cached under
~/tmp/triage_l2_cache/<sha1>.json:
    ./bin/objdiff-cli diff -p . '<SYM>' -f json --include-instructions

HOW TO RUN (from the worktree root):
    python3 scripts/triage/divergence_triage.py             # full pool
    python3 scripts/triage/divergence_triage.py --sample 36 # stratified smoke test
Outputs --out (JSON, ~/tmp/triage_results.json) and --buckets-md (priced md).
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import random
import subprocess
import sys
import time
from collections import Counter, defaultdict
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timezone
from pathlib import Path

# ── import reusable analysis helpers ────────────────────────────────────────
_HERE = Path(__file__).resolve().parent
_ANALYSIS = _HERE.parent / "analysis"
sys.path.insert(0, str(_ANALYSIS))
from diff_inspect import (  # noqa: E402
    parse_breakdowns,
    compute_offset_histogram,
    compute_reg_swap_pairs,
    find_clusters,
    categorize_replaces,  # noqa: F401  (imported per spec; available if needed)
)

HOME_TMP = Path(os.path.expanduser("~/tmp"))
DEFAULT_CACHE = HOME_TMP / "triage_l2_cache"

# ── taxonomy constants ──────────────────────────────────────────────────────
B_MISPAIR = "MISPAIR"
B_RELOC = "RELOC-COLOC"
B_STRUCT = "STRUCT-ARTIFACT"
B_FORM = "FORM-DIVERGENCE"
B_BODY = "BODY-PORT"
B_REVIEW = "NEEDS-REVIEW"
B_Z_UNMAPPED = "ZERO-UNMAPPED"
B_Z_COMPILE = "ZERO-COMPILE-FAIL"
# ZERO-SCATTER sub-split (base obj emits nothing = COMDAT scattered)
B_ZS_STL = "ZS-STL-HELPER"
B_ZS_MISSINST = "ZS-MISSING-INSTANTIATION"
B_ZS_UNWIRED = "ZS-UNWIRED-OWNSPAN"
B_ZS_OTHER = "ZS-OTHER"

NONZERO_BUCKETS = [B_MISPAIR, B_RELOC, B_STRUCT, B_FORM, B_BODY, B_REVIEW]
ZERO_BUCKETS = [B_Z_UNMAPPED, B_ZS_STL, B_ZS_MISSINST, B_ZS_UNWIRED, B_ZS_OTHER, B_Z_COMPILE]
ALL_BUCKETS = NONZERO_BUCKETS + ZERO_BUCKETS

FLEET = {
    B_MISPAIR: "renamer/map fix (tooling, not source)",
    B_RELOC: "skip (at-limit)",
    B_STRUCT: "struct stream",
    B_FORM: "crack-farm / pattern families",
    B_BODY: "LLM grind (75-92 band best ROI)",
    B_REVIEW: "manual triage",
    B_Z_UNMAPPED: "splits/mapping work",
    B_ZS_STL: "skip (ICF-merged STL)",
    B_ZS_MISSINST: "forced-instantiation one-liners (probe-verified 2026-07-19)",
    B_ZS_UNWIRED: "standard-wire work (oracle-poor, low ROI)",
    B_ZS_OTHER: "manual triage",
    B_Z_COMPILE: "build fix",
}
FLIP_PRIOR = {
    B_MISPAIR: "n/a (re-pair then reclassify)",
    B_RELOC: "~0%",
    B_STRUCT: "50-70% (cascades once fixed)",
    B_FORM: "40-60%",
    B_BODY: "30-50%",
    B_REVIEW: "case-by-case",
    B_Z_UNMAPPED: "case-by-case",
    B_ZS_STL: "~0%",
    B_ZS_MISSINST: "high (probe: 2/2 strict flips, +2 report, no collateral)",
    B_ZS_UNWIRED: "case-by-case",
    B_ZS_OTHER: "case-by-case",
    B_Z_COMPILE: "case-by-case",
}

# STL container-helper reference markers (for the ZS-STL-HELPER rule)
STL_HELPER_REFS = ("stlpmtx_std", "_Rb_tree", "__uninitialized", "_M_insert",
                   "vector@", "list@")

# bands for stratified sampling (deterministic)
BAND_995, BAND_90, BAND_75, BAND_075 = "99.5+", "90-99.5", "75-90", "0-75"
BAND_ZAUTO, BAND_ZREAL = "zero-auto", "zero-real"
SAMPLE_BANDS = [BAND_995, BAND_90, BAND_75, BAND_075, BAND_ZAUTO, BAND_ZREAL]

RELOC_OPCODES = {"bl", "lis", "addi", "subi", "ori", "lwz"}


# ── helpers ─────────────────────────────────────────────────────────────────
def _last_json(text: str):
    """Return the last json-decodable dict line (campaign.py trick)."""
    text = (text or "").strip()
    if not text:
        return None
    for cand in (text, *reversed(text.splitlines())):
        cand = cand.strip()
        if cand.startswith("{") and cand.endswith("}"):
            try:
                obj = json.loads(cand)
                if isinstance(obj, dict):
                    return obj
            except ValueError:
                continue
    return None


def _stem(unit: str) -> str:
    base = unit.rsplit("/", 1)[-1]
    return os.path.splitext(base)[0]


def _is_auto(unit: str) -> bool:
    if unit.startswith("default/auto_"):
        return True
    parts = unit.split("/")
    return len(parts) > 1 and parts[1].startswith("auto_")


def _hexd(v) -> str:
    try:
        iv = int(round(v))
    except (TypeError, ValueError):
        return str(v)
    sign = "-" if iv < 0 else "+"
    return f"{sign}0x{abs(iv):x}"


def band_of(pct: float, unit: str) -> str:
    if pct <= 0:
        return BAND_ZAUTO if _is_auto(unit) else BAND_ZREAL
    if pct >= 99.5:
        return BAND_995
    if pct >= 90.0:
        return BAND_90
    if pct >= 75.0:
        return BAND_75
    return BAND_075


def live_band(pct) -> str:
    if not isinstance(pct, (int, float)):
        return "<75"
    if pct >= 99.5:
        return "99.5+"
    if pct >= 90.0:
        return "90-99.5"
    if pct >= 75.0:
        return "75-90"
    return "<75"


# ── project context (wired stems, built objs, objects.json) ─────────────────
class Ctx:
    def __init__(self, project: Path):
        self.project = project
        self.objdiff = str(project / "bin" / "objdiff-cli")
        self.wired_stems = self._load_wired(project)
        self.built_obj_stems = self._load_built(project)
        self.src_cpp_by_stem = self._load_src_cpp(project)

    @staticmethod
    def _load_wired(project: Path) -> set:
        stems = set()
        f = project / "config" / "45410914" / "objects.json"
        try:
            data = json.load(open(f))
        except Exception:
            return stems
        for grp in data.values():
            if not isinstance(grp, dict):
                continue
            for name in (grp.get("objects") or {}).keys():
                stems.add(_stem(name))
        return stems

    @staticmethod
    def _load_built(project: Path) -> set:
        stems = set()
        root = project / "build" / "45410914"
        if not root.is_dir():
            return stems
        for p in root.rglob("*.obj"):
            if "/src/" in str(p):
                stems.add(p.stem)
        return stems

    @staticmethod
    def _load_src_cpp(project: Path) -> dict:
        """Map source-file stem -> repo-relative .cpp path (own-span search)."""
        idx = {}
        root = project / "src"
        if not root.is_dir():
            return idx
        for p in root.rglob("*.cpp"):
            idx.setdefault(p.stem, str(p.relative_to(project)))
        return idx


# ── layer 1: batch ──────────────────────────────────────────────────────────
def run_batch(ctx: Ctx, symbols: list[str], verbose: bool) -> dict:
    """Return {symbol: batch_record_dict}. Missing/None -> NOT_FOUND record."""
    payload = "\n".join(symbols) + "\n"
    cmd = [ctx.objdiff, "diff", "-p", ".", "--batch", "--verdict", "-f", "json"]
    if verbose:
        print(f"[layer1] batch over {len(symbols)} symbols ...", file=sys.stderr)
    proc = subprocess.run(
        cmd, cwd=str(ctx.project), input=payload,
        capture_output=True, text=True, timeout=1200,
    )
    recs = {}
    for line in proc.stdout.splitlines():
        line = line.strip()
        if not (line.startswith("{") and line.endswith("}")):
            continue
        try:
            d = json.loads(line)
        except ValueError:
            continue
        sym = d.get("symbol")
        if sym:
            recs[sym] = d
    return recs


def is_not_found(rec) -> bool:
    return rec is None or rec.get("unit") is None or rec.get("target_size") is None


# ── layer 2: per-symbol instructions ────────────────────────────────────────
def fetch_l2(ctx: Ctx, symbol: str, cache_dir: Path, use_cache: bool) -> dict | None:
    key = hashlib.sha1(symbol.encode()).hexdigest()
    cpath = cache_dir / f"{key}.json"
    if use_cache and cpath.exists():
        try:
            return json.load(open(cpath))
        except Exception:
            pass
    cmd = [ctx.objdiff, "diff", "-p", ".", symbol, "-f", "json",
           "--include-instructions"]
    try:
        proc = subprocess.run(
            cmd, cwd=str(ctx.project), capture_output=True, text=True, timeout=60,
        )
    except subprocess.TimeoutExpired:
        return None
    d = _last_json(proc.stdout)
    if d is not None and use_cache:
        try:
            cache_dir.mkdir(parents=True, exist_ok=True)
            json.dump(d, open(cpath, "w"))
        except Exception:
            pass
    return d


# ── classification ──────────────────────────────────────────────────────────
def _summary(rec) -> dict:
    return rec.get("instruction_summary") or {}


def _pat_counts(rec) -> dict:
    pats = {}
    for p in (rec.get("analysis") or {}).get("patterns", []):
        pats[p.get("pattern")] = pats.get(p.get("pattern"), 0) + int(
            p.get("instruction_count") or 0)
    return pats


def _factor(rec, name):
    for fac in (rec.get("verdict") or {}).get("factors", []):
        if fac.get("name") == name:
            return fac.get("value")
    return None


def classify_l1(row, rec, ctx, unit_all_zero: dict):
    """Layer-1 decision. Returns (bucket, conf, evidence, need_l2, extra_ev).
    bucket=None means route to layer-2 (rule 4)."""
    pool_pct = row["pct"]
    unit = row["unit"]
    ev = []
    if row.get("dup"):
        ev.append("dup-symbol")

    # ── zero band ──────────────────────────────────────────────────────────
    if pool_pct <= 0:
        if _is_auto(unit):
            return B_Z_UNMAPPED, "high", ev + [f"auto-split unit {unit.rsplit('/',1)[-1]}"], False, ev
        if is_not_found(rec):
            return B_Z_UNMAPPED, "high", ev + ["not in target index"], False, ev
        bsize = rec.get("base_size")
        live = rec.get("fuzzy_match_percent")
        if bsize and bsize > 0:
            # Z4: route to nonzero rules on live pct
            return classify_nonzero(row, rec, ctx, ev + ["report0-live-diff"])
        # base emits nothing (COMDAT scattered)
        stem = _stem(unit)
        wired = stem in ctx.wired_stems
        obj_missing = stem not in ctx.built_obj_stems
        if wired and obj_missing and unit_all_zero.get(unit, False):
            return B_Z_COMPILE, "high", ev + ["base obj missing", "all unit symbols base_size==0"], False, ev
        return classify_zero_scatter(row, rec, ctx, ev, wired, obj_missing)

    # ── nonzero band ────────────────────────────────────────────────────────
    if is_not_found(rec):
        return B_Z_UNMAPPED, "medium", ev + ["not in target index (pool nonzero)"], False, ev
    return classify_nonzero(row, rec, ctx, ev)


def _is_stl_helper(name: str) -> bool:
    """ICF-merged STL container helper: never flips via scatter-include."""
    if "@stlpmtx_std@" in name:
        return True
    return name.startswith("??$") and any(r in name for r in STL_HELPER_REFS)


def _is_template_sym(name: str) -> bool:
    """MSVC template symbol: ``??$`` prefix or template-arg mangling ``?$``."""
    return name.startswith("??$") or "?$" in name


def classify_zero_scatter(row, rec, ctx, ev, wired, obj_missing):
    """Sub-split the base-emits-nothing (COMDAT-scattered) pool.

    Returns a 5-tuple (bucket, conf, evidence, need_l2=False, base_ev) like the
    other classify_l1 branches.  Buckets: ZS-STL-HELPER / ZS-MISSING-INSTANTIATION
    / ZS-UNWIRED-OWNSPAN / ZS-OTHER.
    """
    name = row["name"]
    stem = _stem(row["unit"])
    hint = ["base obj missing"] if obj_missing else []

    # 1. STL container helper -> ICF-merged, never flips via include.
    if _is_stl_helper(name):
        return B_ZS_STL, "high", ev + ["stlpmtx_std/STL container helper (ICF-merged)"] + hint, False, ev

    is_tmpl = _is_template_sym(name)

    # 2. Template instantiation missing from a wired unit -> probe candidate.
    if is_tmpl and wired:
        return B_ZS_MISSINST, "medium", ev + [f"template inst missing from wired unit {stem}"] + hint, False, ev

    # 3. Non-template symbol in an unwired unit (own-span .cpp exists in tree
    #    but the TU is absent from objects.json) -> standard-wire work.
    if not is_tmpl and not wired:
        own = ctx.src_cpp_by_stem.get(stem)
        if own:
            return B_ZS_UNWIRED, "medium", ev + [f"own-span {own} unwired"] + hint, False, ev
        return B_ZS_UNWIRED, "low", ev + [f"unit stem {stem} not in objects.json (no own-span cpp)"] + hint, False, ev

    # 4. Everything else.
    if is_tmpl and not wired:
        return B_ZS_OTHER, "low", ev + [f"template in unwired unit {stem}"] + hint, False, ev
    # wired + non-template + base emits nothing = defined inline in a header,
    # always inlined, so base never emits it (e.g. Mic).
    return B_ZS_OTHER, "medium", ev + ["wired unit, inline-only symbol (never emitted by base)"] + hint, False, ev


def classify_nonzero(row, rec, ctx, ev):
    """Layer-1 nonzero rules 1-3. Rule 4 -> (None,...,need_l2=True)."""
    S = _summary(rec)
    total = int(S.get("total") or 0)
    equal = int(S.get("equal") or 0)
    M = max(total - equal, 0)
    ins = int(S.get("insert") or 0)
    dele = int(S.get("delete") or 0)
    dop = int(S.get("diff_op") or 0)
    pats = _pat_counts(rec)
    tsize = rec.get("target_size") or 0
    bsize = rec.get("base_size") or 0
    pct = rec.get("fuzzy_match_percent")
    pct = pct if isinstance(pct, (int, float)) else row["pct"]

    # Rule 1: MISPAIR (guarded: tiny fns with size divergence are usually
    # inlining/breadcrumb divergence, not renamer pairing errors)
    if bsize > 0 and tsize > 0:
        lo, hi = min(tsize, bsize), max(tsize, bsize)
        ratio = hi / lo if lo else 999.0
        delta = abs(tsize - bsize)
        if (ratio > 1.5 or delta > 64) and pct < 90:
            if lo < 64:
                return (B_BODY, "low",
                        ev + [f"tiny-fn size divergence {tsize}T vs {bsize}B "
                              "(inlining/breadcrumb likely)"], False, ev)
            conf = "high" if (ratio > 2 or delta > 256) else "medium"
            return B_MISPAIR, conf, ev + [f"size {tsize}T vs {bsize}B (ratio {ratio:.2f})"], False, ev

    # Rule 2: RELOC-COLOC (layer 1)
    merged = pats.get("LINKER_MERGED", 0) + pats.get("ADDRESS_RELOCATION_NOISE", 0)
    verdict = (rec.get("verdict") or {}).get("classification")
    if ins == 0 and dele == 0 and dop == 0 and M > 0 and merged >= 0.8 * M and pct >= 99.0:
        return B_RELOC, "high", ev + [f"{merged}/{M} merged/reloc mismatches, pct {pct:.2f}"], False, ev
    if verdict == "AT_LIMIT":
        mcr = _factor(rec, "merged_call_ratio")
        arr = _factor(rec, "address_relocation_ratio")
        best = max([v for v in (mcr, arr) if isinstance(v, (int, float))], default=0.0)
        if best >= 0.8:
            src = "merged_call" if (mcr or 0) >= 0.8 else "reloc"
            return B_RELOC, "high", ev + [f"AT_LIMIT {src}_ratio {best:.2f}"], False, ev

    # Rule 3: layer-1 BODY-PORT fast path
    if (ins + dele) >= max(6, 0.05 * total):
        return B_BODY, "medium", ev + [f"L1 insert+delete {ins+dele}/{total}"], "optional", ev

    # Rule 4: needs layer 2
    return None, None, ev, True, ev


def classify_l2(row, rec, l2, ev):
    """Layer-2 rules a-e. Returns (bucket, conf, evidence)."""
    S = _summary(rec)
    total = int(S.get("total") or 0)
    equal = int(S.get("equal") or 0)
    M = max(total - equal, 0)
    ins = int(S.get("insert") or 0)
    dele = int(S.get("delete") or 0)
    pats = _pat_counts(rec)

    if l2 is None:
        vc = (rec.get("verdict") or {}).get("classification", "unknown")
        return B_REVIEW, "low", ev + [f"L2 fetch failed; verdict {vc}"]

    instrs = l2.get("instructions") or []
    reg_swaps, offset_diffs, symbol_diffs, branch_diffs = parse_breakdowns(instrs)
    noneq = [it for it in instrs if it.get("match_type") != "equal"]
    mt_counts = Counter(it.get("match_type") for it in noneq)
    live_pct = rec.get("fuzzy_match_percent")
    live_pct = live_pct if isinstance(live_pct, (int, float)) else row["pct"]

    # (a) STRUCT-ARTIFACT: dominant nonzero displacement delta
    struct_dom_failed = True  # immediates failed dominance (or too few to test)
    if len(offset_diffs) >= 3:
        hist = compute_offset_histogram(offset_diffs)
        nonzero = {d: c for d, c in hist.items() if d != 0}
        if nonzero:
            dom_delta, dom_count = max(nonzero.items(), key=lambda kv: kv[1])
            cover = dom_count / len(offset_diffs)
            if cover >= 0.60:
                return (B_STRUCT, "high" if cover >= 0.85 else "medium",
                        ev + [f"delta {_hexd(dom_delta)} on {dom_count}/{len(offset_diffs)} displacement diffs"])
    # (a-weak) exactly 2 offset diffs, identical nonzero delta, no swaps
    if len(offset_diffs) == 2 and len(reg_swaps) == 0:
        d0, d1 = offset_diffs[0][3], offset_diffs[1][3]
        if d0 == d1 and d0 != 0:
            # Deleting dtors (??_G/??_E) with per-fn varying deltas are the known
            # scatter-dtor reloc-arg co-location residue, NOT shared struct drift
            # (see memory: 99.8x scatter destructor residue). Keep them out of
            # the struct stream; delta retained as a hint only.
            if row["name"].startswith(("??_G", "??_E")):
                return (B_RELOC, "low",
                        ev + [f"scatter-dtor co-location residue (delta hint {_hexd(d0)} on 2/2)"])
            return (B_STRUCT, "low",
                    ev + [f"delta {_hexd(d0)} on 2/2 displacement diffs (weak)"])

    # (new) pure symbol-reloc rule: all mismatches diff_arg with only sym/imm diffs
    if noneq and all(it.get("match_type") == "diff_arg" for it in noneq):
        arg_types = set()
        for it in noneq:
            for a in (it.get("diff_breakdown") or {}).get("arguments", []):
                arg_types.add(a.get("arg_type"))
        if arg_types and arg_types <= {"symbol"}:
            return B_RELOC, "high", ev + [f"{len(noneq)} pure symbol-reloc rows"]
        if (arg_types and arg_types <= {"symbol", "immediate"}
                and "immediate" in arg_types and struct_dom_failed
                and len(reg_swaps) == 0 and ins + dele == 0 and live_pct >= 99.0):
            return B_RELOC, "medium", ev + ["sym+imm reloc rows, no dominant delta, no swaps"]

    # (b) FORM-DIVERGENCE
    if (ins + dele) <= max(2, 0.02 * total):
        tgt_ops, base_ops = Counter(), Counter()
        for it in noneq:
            t = it.get("target") or {}
            b = it.get("base") or {}
            if t.get("opcode"):
                tgt_ops[t["opcode"]] += 1
            if b.get("opcode"):
                base_ops[b["opcode"]] += 1
        has_rep_dop = (mt_counts.get("replace", 0) + mt_counts.get("diff_op", 0)) > 0
        multiset_eq = (tgt_ops == base_ops and sum(tgt_ops.values()) > 0
                       and (len(reg_swaps) > 0 or has_rep_dop))
        swap_heavy = M > 0 and len(reg_swaps) >= 0.5 * M
        form_pat = (pats.get("REGISTER_SWAP", 0) + pats.get("COMMUTATIVE_OP_ORDER", 0)
                    + pats.get("OFFSET_SWAP", 0))
        pat_heavy = M > 0 and form_pat >= 0.5 * M
        if multiset_eq or swap_heavy or pat_heavy:
            e = []
            if multiset_eq:
                e.append("opcode multiset equal")
            if swap_heavy:
                pairs = compute_reg_swap_pairs(reg_swaps)
                top = sorted(pairs.items(), key=lambda kv: -kv[1]["count"])[:2]
                e.append("swaps " + ",".join(f"{a}<->{b}x{d['count']}" for (a, b), d in top))
            if pat_heavy and not e:
                e.append(f"swap/commute patterns {form_pat}/{M}")
            return B_FORM, "medium", ev + e
        # (b-fixable) small all-replace/diff_op set: comparison-style one-liners
        if (noneq and mt_counts.get("insert", 0) == 0 and mt_counts.get("delete", 0) == 0
                and all(it.get("match_type") in ("replace", "diff_op") for it in noneq)
                and len(noneq) <= max(3, 0.05 * total)):
            pair_counts = Counter(
                ((it.get("target") or {}).get("opcode", "?"),
                 (it.get("base") or {}).get("opcode", "?"))
                for it in noneq)
            e = "replace " + ", ".join(
                f"{t}->{b} x{c}" for (t, b), c in pair_counts.most_common(3))
            return B_FORM, "medium", ev + [e]

    # (c) BODY-PORT (layer-2 cluster confirm)
    clusters = find_clusters(instrs, match_types=("insert", "delete"), gap=2)
    if clusters:
        biggest = max(clusters, key=len)
        if len(biggest) >= 3:
            start_idx = biggest[0][1].get("index", biggest[0][0])
            return B_BODY, "high", ev + [f"insert/delete cluster {len(biggest)} instrs @idx {start_idx}"]

    # (d) RELOC-COLOC (layer-2 confirm)
    if instrs:
        noneq = [it for it in instrs if it.get("match_type") != "equal"]
        all_diffarg = noneq and all(it.get("match_type") == "diff_arg" for it in noneq)
        reloc_ok = True
        for it in noneq:
            op = (it.get("target") or {}).get("opcode", "")
            bd = it.get("diff_breakdown") or {}
            args = bd.get("arguments", [])
            kinds = {a.get("arg_type") for a in args}
            only_sym_imm = kinds <= {"symbol", "immediate"}
            if not (only_sym_imm and op in RELOC_OPCODES):
                reloc_ok = False
                break
        hist = compute_offset_histogram(offset_diffs)
        nonzero = {d: c for d, c in hist.items() if d != 0}
        dom_fail = True
        if len(offset_diffs) >= 3 and nonzero:
            dom_count = max(nonzero.values())
            dom_fail = (dom_count / len(offset_diffs)) < 0.60
        if all_diffarg and reloc_ok and len(reg_swaps) == 0 and dom_fail:
            return B_RELOC, "low", ev + [f"{len(noneq)} diff_arg sym/imm reloc rows"]

    # (e) fallback
    vc = (rec.get("verdict") or {}).get("classification", "unknown")
    return B_REVIEW, "low", ev + [f"verdict {vc}"]


# ── pool loading + sampling ─────────────────────────────────────────────────
def load_pool(path: str) -> list[dict]:
    rows = []
    with open(path, newline="") as f:
        for r in csv.DictReader(f):
            rows.append({
                "unit": r["unit"],
                "name": r["name"],
                "size": int(r["size"]),
                "pct": float(r["pct"]),
            })
    # mark duplicate symbols (same name in >1 unit)
    name_units = defaultdict(set)
    for row in rows:
        name_units[row["name"]].add(row["unit"])
    for row in rows:
        row["dup"] = len(name_units[row["name"]]) > 1
    return rows


def stratified_sample(rows: list[dict], n: int) -> list[dict]:
    rng = random.Random(42)
    per = max(1, n // len(SAMPLE_BANDS))
    by_band = defaultdict(list)
    for row in rows:
        by_band[band_of(row["pct"], row["unit"])].append(row)
    out = []
    for band in SAMPLE_BANDS:
        pool = sorted(by_band.get(band, []), key=lambda r: (r["unit"], r["name"]))
        if not pool:
            continue
        k = min(per, len(pool))
        out.extend(rng.sample(pool, k))
    return out


# ── output ──────────────────────────────────────────────────────────────────
def write_json(path, project, pool_path, rows, results):
    counts = Counter(r["bucket"] for r in results.values())
    funcs = []
    for row in rows:
        res = results[(row["unit"], row["name"])]
        funcs.append({
            "unit": row["unit"], "name": row["name"], "size": row["size"],
            "pct": row["pct"], "live_pct": res["live_pct"],
            "target_size": res["target_size"], "base_size": res["base_size"],
            "bucket": res["bucket"], "confidence": res["confidence"],
            "evidence": res["evidence"], "layer": res["layer"],
        })
    doc = {
        "generated": datetime.now(timezone.utc).isoformat(),
        "project": str(project), "pool": pool_path,
        "counts": dict(counts), "functions": funcs,
    }
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    json.dump(doc, open(path, "w"), indent=2)


def write_markdown(path, rows, results):
    by_bucket = defaultdict(list)
    for row in rows:
        by_bucket[results[(row["unit"], row["name"])]["bucket"]].append((row, results[(row["unit"], row["name"])]))
    lines = ["# Divergence triage — priced buckets", ""]
    lines.append("| Bucket | Fns | Tgt KB | 99.5+ | 90-99.5 | 75-90 | <75 | Fleet | Flip likelihood |")
    lines.append("|---|---:|---:|---:|---:|---:|---:|---|---|")
    order = [b for b in ALL_BUCKETS if b in by_bucket] + [b for b in by_bucket if b not in ALL_BUCKETS]
    for b in order:
        items = by_bucket[b]
        n = len(items)
        kb = sum(r["size"] for r, _ in items) / 1024.0
        if b in NONZERO_BUCKETS:
            bands = Counter(live_band(res["live_pct"]) for _, res in items)
            c995, c90, c75, clow = bands["99.5+"], bands["90-99.5"], bands["75-90"], bands["<75"]
            bcols = f"{c995} | {c90} | {c75} | {clow}"
        else:
            bcols = "- | - | - | -"
        lines.append(f"| {b} | {n} | {kb:.1f} | {bcols} | {FLEET.get(b,'?')} | {FLIP_PRIOR.get(b,'?')} |")
    lines.append("")
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    open(path, "w").write("\n".join(lines))


def print_table(results):
    counts = Counter(r["bucket"] for r in results.values())
    print("\n=== bucket counts ===")
    for b in ALL_BUCKETS:
        if counts.get(b):
            print(f"  {b:18s} {counts[b]:5d}   {FLEET[b]}")
    for b, c in counts.items():
        if b not in ALL_BUCKETS:
            print(f"  {b:18s} {c:5d}   (unexpected)")
    print(f"  {'TOTAL':18s} {sum(counts.values()):5d}")


# ── main ────────────────────────────────────────────────────────────────────
def main():
    ap = argparse.ArgumentParser(description="Batch divergence-triage classifier")
    ap.add_argument("--pool", default="/home/free/tmp/triage_pool.csv")
    ap.add_argument("--project", default="/home/free/tmp/wt-triage")
    ap.add_argument("--out", default=str(HOME_TMP / "triage_results.json"))
    ap.add_argument("--buckets-md", default=str(HOME_TMP / "triage_buckets.md"))
    ap.add_argument("--sample", type=int, default=0)
    ap.add_argument("--only-nonzero", action="store_true")
    ap.add_argument("--only-zero", action="store_true")
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("--l2-limit", type=int, default=0, help="cap layer-2 fetches (0=unlimited)")
    ap.add_argument("--no-cache", action="store_true")
    ap.add_argument("--cache-dir", default=str(DEFAULT_CACHE))
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    t0 = time.time()
    project = Path(args.project).resolve()
    ctx = Ctx(project)
    cache_dir = Path(args.cache_dir)
    use_cache = not args.no_cache

    rows = load_pool(args.pool)
    if args.only_nonzero:
        rows = [r for r in rows if r["pct"] > 0]
    if args.only_zero:
        rows = [r for r in rows if r["pct"] <= 0]
    if args.sample:
        rows = stratified_sample(rows, args.sample)

    band_sizes = Counter(band_of(r["pct"], r["unit"]) for r in rows)
    print(f"[pool] {len(rows)} rows selected")
    for band in SAMPLE_BANDS:
        if band_sizes.get(band):
            print(f"  band {band:10s} {band_sizes[band]}")

    # ── layer 1 batch ────────────────────────────────────────────────────
    symbols = sorted({r["name"] for r in rows})
    recs = run_batch(ctx, symbols, args.verbose) if symbols else {}
    print(f"[layer1] batch done, {len(recs)} records, {time.time()-t0:.1f}s")

    # precompute per-unit all-zero (over selected pool rows)
    unit_bsizes = defaultdict(list)
    for r in rows:
        rec = recs.get(r["name"])
        bs = None if is_not_found(rec) else (rec.get("base_size") if rec else None)
        unit_bsizes[r["unit"]].append(bs)
    unit_all_zero = {u: all((b in (0, None)) for b in bl) for u, bl in unit_bsizes.items()}

    # ── layer-1 pass ─────────────────────────────────────────────────────
    results = {}
    need_l2, optional_l2 = [], []
    for row in rows:
        rec = recs.get(row["name"])
        bucket, conf, ev, need, base_ev = classify_l1(row, rec, ctx, unit_all_zero)
        live_pct = None
        tsize = bsize = None
        if rec and not is_not_found(rec):
            live_pct = rec.get("fuzzy_match_percent")
            tsize = rec.get("target_size")
            bsize = rec.get("base_size")
        entry = {
            "bucket": bucket, "confidence": conf, "evidence": ev,
            "layer": 1, "live_pct": live_pct if isinstance(live_pct, (int, float)) else row["pct"],
            "target_size": tsize, "base_size": bsize, "_rec": rec, "_base_ev": base_ev,
        }
        results[(row["unit"], row["name"])] = entry
        if bucket is None:
            need_l2.append(row)
        elif need == "optional":
            optional_l2.append(row)

    # ── layer-2 pass ─────────────────────────────────────────────────────
    fetch_rows = list(need_l2)
    if args.l2_limit and len(fetch_rows) > args.l2_limit:
        fetch_rows = fetch_rows[:args.l2_limit]
    budget = (args.l2_limit - len(fetch_rows)) if args.l2_limit else len(optional_l2)
    fetch_rows += optional_l2[:max(0, budget)]

    # dedup by symbol for fetching
    want_syms = []
    seen = set()
    for row in fetch_rows:
        if row["name"] not in seen:
            seen.add(row["name"])
            want_syms.append(row["name"])
    print(f"[layer2] fetching {len(want_syms)} unique symbols "
          f"({len(need_l2)} required, {len(fetch_rows)-len(need_l2) if len(fetch_rows)>=len(need_l2) else 0} optional)")

    l2_by_sym = {}
    if want_syms:
        with ThreadPoolExecutor(max_workers=max(1, args.jobs)) as ex:
            futs = {ex.submit(fetch_l2, ctx, s, cache_dir, use_cache): s for s in want_syms}
            done = 0
            for fut in as_completed(futs):
                s = futs[fut]
                try:
                    l2_by_sym[s] = fut.result()
                except Exception:
                    l2_by_sym[s] = None
                done += 1
                if args.verbose and done % 50 == 0:
                    print(f"  l2 {done}/{len(want_syms)}", file=sys.stderr)

    # finalize l2 rows
    fetch_set = {(r["unit"], r["name"]) for r in fetch_rows}
    for row in rows:
        key = (row["unit"], row["name"])
        entry = results[key]
        rec = entry.pop("_rec", None)
        base_ev = entry.pop("_base_ev", [])
        if entry["bucket"] is None:
            if key in fetch_set and row["name"] in l2_by_sym:
                l2 = l2_by_sym[row["name"]]
                b, c, e = classify_l2(row, rec, l2, base_ev)
                entry.update(bucket=b, confidence=c, evidence=e, layer=2)
            else:
                # exceeded l2 budget -> deferred manual triage
                vc = (rec.get("verdict") or {}).get("classification", "unknown") if rec else "no-rec"
                entry.update(bucket=B_REVIEW, confidence="low",
                             evidence=base_ev + [f"l2-budget-skipped; verdict {vc}"], layer=1)
        elif entry["bucket"] == B_BODY and key in fetch_set and row["name"] in l2_by_sym:
            # attach optional cluster evidence to fast-path body-ports
            l2 = l2_by_sym[row["name"]]
            if l2:
                cl = find_clusters(l2.get("instructions") or [], ("insert", "delete"), 2)
                if cl:
                    big = max(cl, key=len)
                    if len(big) >= 3:
                        idx = big[0][1].get("index", big[0][0])
                        entry["evidence"] = entry["evidence"] + [f"L2 cluster {len(big)}@idx {idx}"]
                        entry["layer"] = 2

    write_json(args.out, project, args.pool, rows, results)
    write_markdown(args.buckets_md, rows, results)
    print(f"[out] {args.out}")
    print(f"[out] {args.buckets_md}")
    print_table(results)
    print(f"\nelapsed {time.time()-t0:.1f}s")


if __name__ == "__main__":
    main()
