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
import re
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
# ground-truth-calibrated buckets (24-fn grind campaign, 2026-07-19)
B_BODY_LEVER = "BODY-LEVER"
B_FORM_REGSWAP = "FORM-REGSWAP-STUCK"
B_WALL_VTORDISP = "WALL-VTORDISP"
B_WALL_DEADARG = "WALL-DEADARG"
B_LEVER_SYMBOL = "LEVER-SYMBOL"
B_LEVER_STRING = "LEVER-STRING"
# BODY-LEVER sub-split buckets (30-fn calibration wave, 2026-07-19)
B_STL_CONTAM = "STL-CONTAM"
B_BODY_MISSING = "BODY-MISSING-PORT"
# post-processing staleness bucket (report-vs-live pct divergence)
B_UNRELIABLE = "UNRELIABLE-EVIDENCE"
B_Z_UNMAPPED = "ZERO-UNMAPPED"
B_Z_COMPILE = "ZERO-COMPILE-FAIL"
# ZERO-SCATTER sub-split (base obj emits nothing = COMDAT scattered)
B_ZS_STL = "ZS-STL-HELPER"
B_ZS_MISSINST = "ZS-MISSING-INSTANTIATION"
B_ZS_UNWIRED = "ZS-UNWIRED-OWNSPAN"
B_ZS_OTHER = "ZS-OTHER"

NONZERO_BUCKETS = [
    B_MISPAIR,
    B_LEVER_SYMBOL, B_LEVER_STRING,
    B_BODY_LEVER, B_BODY,
    B_STL_CONTAM, B_BODY_MISSING,
    B_STRUCT,
    B_FORM, B_FORM_REGSWAP,
    B_WALL_VTORDISP, B_WALL_DEADARG,
    B_RELOC, B_UNRELIABLE, B_REVIEW,
]
ZERO_BUCKETS = [B_Z_UNMAPPED, B_ZS_STL, B_ZS_MISSINST, B_ZS_UNWIRED, B_ZS_OTHER, B_Z_COMPILE]
ALL_BUCKETS = NONZERO_BUCKETS + ZERO_BUCKETS

FLEET = {
    B_MISPAIR: "renamer/map fix (tooling, not source)",
    B_RELOC: "skip (at-limit)",
    B_STRUCT: "struct stream",
    B_FORM: "crack-farm / pattern families",
    B_BODY: "LLM grind (75-92 band best ROI)",
    B_REVIEW: "manual triage",
    B_BODY_LEVER: "LLM grind FIRST (shape-screened, per-stratum priced)",
    B_STL_CONTAM: "skip (stlport-version divergence / template-twin mispairs)",
    B_BODY_MISSING: "port missing source first (not a lever grind)",
    B_UNRELIABLE: "re-verify with cache-cleared report before any routing",
    B_FORM_REGSWAP: "skip while permuter banned",
    B_WALL_VTORDISP: "skip (GameMode vtordisp wall)",
    B_WALL_DEADARG: "skip (dead-arg scheduling wall)",
    B_LEVER_SYMBOL: "local-static Symbol lever (mechanical)",
    B_LEVER_STRING: "dev-vs-retail string divergence lever",
    B_Z_UNMAPPED: "splits/mapping work",
    B_ZS_STL: "skip (ICF-merged STL)",
    B_ZS_MISSINST: "forced-instantiation one-liners (probe-verified 2026-07-19)",
    B_ZS_UNWIRED: "standard-wire work (oracle-poor, low ROI)",
    B_ZS_OTHER: "manual triage",
    B_Z_COMPILE: "build fix",
}
FLIP_PRIOR = {
    B_MISPAIR: "n/a (re-pair then reclassify)",
    B_RELOC: "~0% (validated)",
    B_STRUCT: "50-70% — UNMEASURED estimate; run a 20-30-fn calibration wave before funding",
    B_FORM: "~30% — UNMEASURED estimate; calibrate before funding",
    B_BODY: "45% in 78-96 band; <=10% above 97.5 (survivor bias)",
    B_REVIEW: "case-by-case",
    B_BODY_LEVER: "25% in 70-90 live (MEASURED 2/8); <=5% elsewhere (MEASURED 0/22)",
    B_STL_CONTAM: "~0% (MEASURED 0/6)",
    B_BODY_MISSING: "unmeasured — needs source work",
    B_UNRELIABLE: "n/a",
    B_FORM_REGSWAP: "~0%",
    B_WALL_VTORDISP: "~0% (4 independent confirms) (validated)",
    B_WALL_DEADARG: "~0% (3 confirms) (validated)",
    B_LEVER_SYMBOL: "~90%",
    B_LEVER_STRING: "~85%",
    B_Z_UNMAPPED: "case-by-case",
    B_ZS_STL: "~0% (validated)",
    B_ZS_MISSINST: "high (probe: 2/2 strict flips, +2 report, no collateral)",
    B_ZS_UNWIRED: "case-by-case",
    B_ZS_OTHER: "case-by-case",
    B_Z_COMPILE: "case-by-case",
}
# Evidence basis per bucket for the priced markdown table.
#   MEASURED  = calibrated against decomp.db outcomes (30-fn wave / 24-fn campaign)
#   PROBE     = probe-verified (forced-instantiation dry run)
#   VALIDATED = repeat independent confirms, not a single controlled wave
#   ESTIMATE  = unmeasured prior; needs a calibration wave before funding
BASIS = {
    B_BODY_LEVER: "MEASURED",
    B_STL_CONTAM: "MEASURED",
    B_MISPAIR: "VALIDATED",
    B_RELOC: "VALIDATED",
    B_WALL_VTORDISP: "VALIDATED",
    B_WALL_DEADARG: "VALIDATED",
    B_FORM_REGSWAP: "VALIDATED",
    B_LEVER_SYMBOL: "VALIDATED",
    B_LEVER_STRING: "VALIDATED",
    B_ZS_STL: "VALIDATED",
    B_ZS_MISSINST: "PROBE",
    B_BODY: "ESTIMATE",
    B_STRUCT: "ESTIMATE",
    B_FORM: "ESTIMATE",
    B_BODY_MISSING: "ESTIMATE",
    B_REVIEW: "n/a",
    B_UNRELIABLE: "n/a",
    B_Z_UNMAPPED: "n/a",
    B_ZS_UNWIRED: "n/a",
    B_ZS_OTHER: "n/a",
    B_Z_COMPILE: "n/a",
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
    # Only take the L1 AT_LIMIT reloc shortcut when pct >= 99; below that, force
    # L2 so the wall detectors (R5a/R5b) can run on survivor-band fns that the
    # AT_LIMIT verdict would otherwise freeze as RELOC-COLOC (e.g. ClearDrawGlitch
    # at 96.5% is a GameMode vtordisp wall, not a reloc at-limit).
    if verdict == "AT_LIMIT" and pct >= 99.0:
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


# ── ground-truth-calibrated lever/wall detectors (24-fn campaign 2026-07-19) ─
_RAW_LABEL_RE = re.compile(r"^(?:lbl|fn)_8[0-9A-Fa-f]+")


def _row_symbols(side):
    """Symbol-typed typed_arg values on one instruction side (target/base)."""
    return [str(ta.get("value"))
            for ta in (side or {}).get("typed_args", [])
            if ta.get("type") == "Symbol"]


def _side_has_imm(side, opcode, values):
    """True if side has `opcode` and an immediate typed_arg whose value is in `values`."""
    s = side or {}
    if s.get("opcode") != opcode:
        return False
    for ta in s.get("typed_args", []):
        if ta.get("type") in ("Signed", "Unsigned") and ta.get("value") in values:
            return True
    return False


def _detect_lever_symbol(instrs):
    """R6a: raw label (lbl_/fn_8...) on one side vs a named GLOBAL Symbol var
    (``@@3VSymbol@@``) on the other.  Scans ALL non-equal rows' typed_args --
    including insert/replace rows -- because the global Symbol often surfaces in
    a replace/insert row that carries no diff_breakdown (parse_breakdowns misses
    it), e.g. ShowBriefBandMessage idx33.  The ``@@3`` global-storage marker is
    exactly what separates this lever from local-static Symbols (``@4VSymbol@@``,
    e.g. ClearDrawGlitch) and local-static Messages (``@4VMessage@@``).
    Returns (target_side_value, base_side_value) or None."""
    for it in instrs:
        if it.get("match_type") == "equal":
            continue
        tsyms = _row_symbols(it.get("target"))
        bsyms = _row_symbols(it.get("base"))
        t_raw = next((v for v in tsyms if _RAW_LABEL_RE.match(v)), None)
        b_raw = next((v for v in bsyms if _RAW_LABEL_RE.match(v)), None)
        t_named = next((v for v in tsyms if "@@3VSymbol@@" in v), None)
        b_named = next((v for v in bsyms if "@@3VSymbol@@" in v), None)
        if t_raw and b_named:
            return (t_raw, b_named)
        if b_raw and t_named:
            return (t_named, b_raw)
    return None


def _detect_lever_string(instrs):
    """R6b: a replace row where one side is ``li #0`` and the other side's
    typed_args carry a Symbol-type arg (string/label reference), either
    direction.  Returns the row index or None."""
    for it in instrs:
        if it.get("match_type") != "replace":
            continue
        t, b = it.get("target"), it.get("base")
        if _side_has_imm(t, "li", (0,)) and _row_symbols(b):
            return it.get("index")
        if _side_has_imm(b, "li", (0,)) and _row_symbols(t):
            return it.get("index")
    return None


def _detect_vtordisp(noneq):
    """R5a: TheGameMode->Property vbase adjustor.  Within any sliding window of 6
    consecutive non-equal rows, on the SAME side, find >=2 of {lwz disp 0x4,
    second lwz disp 0x4, addi imm 0x4} with at least one lwz-disp-4; an ``add``
    anywhere in the window strengthens to high confidence.
    Returns (start_index, has_add) or None."""
    n = len(noneq)
    for i in range(n):
        window = noneq[i:i + 6]
        if len(window) < 2:
            break
        for side_key in ("target", "base"):
            lwz4 = addi4 = 0
            has_add = False
            first_idx = None
            for it in window:
                side = it.get(side_key)
                if _side_has_imm(side, "lwz", (4,)):
                    lwz4 += 1
                    if first_idx is None:
                        first_idx = it.get("index")
                if _side_has_imm(side, "addi", (4,)):
                    addi4 += 1
                    if first_idx is None:
                        first_idx = it.get("index")
                if (side or {}).get("opcode") == "add":
                    has_add = True
            if lwz4 >= 1 and (lwz4 >= 2 or addi4 >= 1):
                start = first_idx if first_idx is not None else window[0].get("index")
                return (start, has_add)
    return None


def _detect_deadarg(instrs, ins, dele):
    """R5b: total insert+delete <= 4 and an inserted/deleted ``li #0/#1`` row
    within 2 (by instruction index) of any ``bl`` row.  Returns index or None."""
    if ins + dele > 4:
        return None
    bl_idx = [it.get("index") for it in instrs
              if (it.get("target") or {}).get("opcode") == "bl"
              or (it.get("base") or {}).get("opcode") == "bl"]
    if not bl_idx:
        return None
    for it in instrs:
        if it.get("match_type") not in ("insert", "delete"):
            continue
        for sk in ("target", "base"):
            if _side_has_imm(it.get(sk), "li", (0, 1)):
                idx = it.get("index")
                if any(abs(idx - bi) <= 2 for bi in bl_idx):
                    return idx
    return None


# ── BODY-LEVER sub-split helpers (30-fn calibration wave, 2026-07-19) ────────
def _own_class(name: str):
    """Innermost class qualifier of a PLAIN method symbol ``?Method@Class@...``.

    Returns None for ``??`` special forms (templates ``??$``, ctor/dtor/operator
    thunks) and non-mangled names -- class extraction on those is unreliable, and
    the calibration exempts them from the class-vs-unit mispair filter."""
    if not name.startswith("?") or name.startswith("??"):
        return None
    head = name.split("@@", 1)[0] if "@@" in name else name
    toks = head.split("@")            # toks[0]=method, toks[1]=innermost class
    if len(toks) >= 2 and toks[1] and not toks[1].startswith("?"):
        return toks[1]
    return None


def _class_token(sym: str):
    """Innermost class token of an arbitrary (callee) mangled symbol, lowercased.
    Tolerant: returns None when no class scope can be read."""
    if not sym or "@@" not in sym:
        return None
    toks = sym.split("@@", 1)[0].split("@")
    if len(toks) >= 2 and toks[1]:
        return toks[1].lower()
    return None


def _scope_has_stl(name: str) -> bool:
    """True iff ``stlpmtx_std`` appears in the function's OWN qualified name
    (the scope chain before the first ``@@``), i.e. the function itself is a
    member of an stlport container/algorithm.  When the token appears only after
    the first ``@@`` it is a parameter/return type (e.g. a non-STL outer function
    taking a ``vector`` argument) and this returns False -- that case is real
    logic, not stlport-version contamination."""
    scope = name.split("@@", 1)[0] if "@@" in name else name
    return "stlpmtx_std" in scope


def _stratum(live) -> str:
    """live-fuzzy stratum tag for BODY-LEVER per-stratum pricing."""
    if not isinstance(live, (int, float)):
        return "lt70"
    if 70.0 <= live < 90.0:
        return "70-90"
    if live < 70.0:
        return "lt70"
    return "90plus"


def _call_class_divergence(instrs, symbol_diffs) -> int:
    """Count ``bl`` rows whose target callee class token differs from the base
    callee class token (>=2 => the renamer paired two functions that call into
    different classes = a mispair, not a body lever)."""
    op_by_idx = {}
    for it in instrs:
        idx = it.get("index")
        op = ((it.get("target") or {}).get("opcode")
              or (it.get("base") or {}).get("opcode"))
        if idx is not None:
            op_by_idx[idx] = op
    n = 0
    for idx, tsym, bsym in symbol_diffs:
        if op_by_idx.get(idx) != "bl":
            continue
        tc, bc = _class_token(tsym), _class_token(bsym)
        if tc and bc and tc != bc:
            n += 1
    return n


def _body_lever_subsplit(row, rec, ctx, ev, instrs, noneq, symbol_diffs,
                         cluster_len, start_idx, diff_arg_rows, explained,
                         live_pct):
    """Apply the calibrated pre-filters BEFORE granting BODY-LEVER (order:
    MISPAIR pre-filters -> STL-CONTAM -> BODY-MISSING-PORT -> BODY-LEVER).
    Returns (bucket, conf, evidence)."""
    name = row["name"]
    stem = _stem(row["unit"])
    bsize = rec.get("base_size") or 0

    # (a) MISPAIR pre-filters ------------------------------------------------
    # a1: stub base (should be impossible in a live BODY-LEVER row, but guard).
    if bsize == 0:
        return B_MISPAIR, "medium", ev + ["body-lever prefilter", "base_size==0 stub"]
    # a2: class-vs-unit mismatch (plain non-template/non-thunk symbols only).
    cls = _own_class(name)
    if cls:
        cl, st = cls.lower(), stem.lower()
        if cl and st and cl not in st and st not in cl:
            return (B_MISPAIR, "medium",
                    ev + ["body-lever prefilter",
                          f"class {cls} attributed to unit {stem}"])
    # a3: call-class divergence (>=2 bl rows into different classes).
    ccd = _call_class_divergence(instrs, symbol_diffs)
    if ccd >= 2:
        return (B_MISPAIR, "medium",
                ev + ["body-lever prefilter",
                      f"calls different class's methods ({ccd})"])

    # (b) STL-CONTAM: fn itself is a member of an stlport container/algorithm.
    if _scope_has_stl(name):
        return (B_STL_CONTAM, "medium",
                ev + ["own scope is stlpmtx_std (stlport-version divergence)"])

    # (c) BODY-MISSING-PORT: dtor thunk or no plausible owner .cpp in-tree.
    if name.startswith(("??_G", "??_D")):
        return (B_BODY_MISSING, "medium",
                ev + [f"deleting-dtor thunk {name[:6]}; port owner source first"])
    has_owner = stem in ctx.src_cpp_by_stem or stem in ctx.wired_stems
    if not has_owner:
        return (B_BODY_MISSING, "medium",
                ev + [f"no in-tree owner .cpp for unit {stem}; port source first"])

    # (d) surviving -> BODY-LEVER, priced per-stratum.
    strat = _stratum(live_pct)
    return (B_BODY_LEVER, "high",
            ev + [f"indel cluster {cluster_len} @idx {start_idx}; "
                  f"explained {explained}/{diff_arg_rows} diff_arg (lever); "
                  f"stratum {strat}"])


def classify_l2(row, rec, l2, ev, ctx=None):
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

    # ── ground-truth-calibrated lever/wall detectors (priority order:
    #    R6a, R6b, R5a, R5b, R3) run BEFORE the existing (a) struct rule ──────

    # R6a LEVER-SYMBOL: raw label vs named global Symbol var (@@3VSymbol@@)
    ls = _detect_lever_symbol(instrs)
    if ls is not None:
        a, b = ls
        return B_LEVER_SYMBOL, "high", ev + [
            f"named-Symbol vs raw-label: {a[:50]} vs {b[:50]}"]

    # R6b LEVER-STRING: replace row, li #0 on one side vs symbol ref on other
    lstr = _detect_lever_string(instrs)
    if lstr is not None:
        return B_LEVER_STRING, "medium", ev + [
            f"string-reloc asymmetry @idx {lstr} (li 0 vs symbol ref)"]

    # R5a WALL-VTORDISP: TheGameMode->Property vbase adjustor (95.9-98.6% band,
    # 4 independent ground-truth confirms)
    vt = _detect_vtordisp(noneq)
    if vt is not None:
        start, has_add = vt
        return (B_WALL_VTORDISP, "high" if has_add else "medium",
                ev + [f"vtordisp signature (lwz 0x4 x2 / addi +4) @idx {start}"])

    # R5b WALL-DEADARG: dead-arg li #0/#1 before a call, tiny indel (3 confirms)
    da = _detect_deadarg(instrs, ins, dele)
    if da is not None:
        return B_WALL_DEADARG, "medium", ev + [f"dead-arg li before call @idx {da}"]

    # R3 FORM-REGSWAP-STUCK: the ONLY residue is register swaps (permuter banned)
    if (ins == 0 and dele == 0 and mt_counts.get("replace", 0) == 0
            and mt_counts.get("diff_op", 0) == 0 and len(offset_diffs) == 0
            and len(symbol_diffs) == 0 and len(reg_swaps) >= 1):
        return (B_FORM_REGSWAP, "high",
                ev + [f"regswap-only residual ({len(reg_swaps)} swaps), "
                      "permuter banned"])

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

    # (c) BODY-PORT / BODY-LEVER (layer-2 cluster confirm). R2: if the biggest
    # indel cluster >= 3 AND the diff_arg residue is fully explained by
    # reg/offset/symbol/branch relocs (>= 0.7 of diff_arg rows), the body is a
    # shape-screened lever (LLM grind first) -> BODY-LEVER; else BODY-PORT.
    clusters = find_clusters(instrs, match_types=("insert", "delete"), gap=2)
    if clusters:
        biggest = max(clusters, key=len)
        if len(biggest) >= 3:
            start_idx = biggest[0][1].get("index", biggest[0][0])
            diff_arg_rows = sum(1 for it in noneq if it.get("match_type") == "diff_arg")
            # Row-based explanation: a diff_arg ROW counts as explained iff it
            # carries a parseable diff_breakdown attribution. The earlier
            # arg-based sum double-counted multi-arg rows (the "35/28" bug)
            # and over-admitted BODY-LEVER.
            explained = sum(
                1 for it in noneq
                if it.get("match_type") == "diff_arg"
                and (it.get("diff_breakdown") or {}).get("arguments"))
            if diff_arg_rows == 0 or explained >= 0.7 * diff_arg_rows:
                # BODY-LEVER sub-split (30-fn calibration wave, 2026-07-19):
                # peel MISPAIR (dominant FP class) / STL-CONTAM / BODY-MISSING-
                # PORT off BEFORE granting BODY-LEVER, then price per-stratum.
                return _body_lever_subsplit(
                    row, rec, ctx, ev, instrs, noneq, symbol_diffs,
                    len(biggest), start_idx, diff_arg_rows, explained, live_pct)
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

    # (e-recovery) AT_LIMIT reloc/merged-dominant last resort: L1 now forces
    # these to L2 (pct<99) so the wall detectors get first crack; anything the
    # walls / struct / form / body rules didn't claim returns to RELOC-COLOC
    # exactly as the L1 AT_LIMIT shortcut would have (baseline parity), instead
    # of leaking to NEEDS-REVIEW.
    if (rec.get("verdict") or {}).get("classification") == "AT_LIMIT":
        mcr = _factor(rec, "merged_call_ratio")
        arr = _factor(rec, "address_relocation_ratio")
        best = max([v for v in (mcr, arr) if isinstance(v, (int, float))], default=0.0)
        if best >= 0.8:
            src = "merged_call" if (mcr or 0) >= 0.8 else "reloc"
            return B_RELOC, "high", ev + [f"AT_LIMIT {src}_ratio {best:.2f} (L2 recovery)"]

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
            "stratum": res.get("stratum"),
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
    lines.append("| Bucket | Basis | Fns | Tgt KB | 99.5+ | 90-99.5 | 75-90 | <75 | Fleet | Flip likelihood |")
    lines.append("|---|---|---:|---:|---:|---:|---:|---:|---|---|")
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
        lines.append(
            f"| {b} | {BASIS.get(b,'n/a')} | {n} | {kb:.1f} | {bcols} | "
            f"{FLEET.get(b,'?')} | {FLIP_PRIOR.get(b,'?')} |")
    lines.append("")
    lines += [
        "Basis: MEASURED = calibrated vs decomp.db outcomes; PROBE = probe-verified "
        "dry run; VALIDATED = repeat independent confirms; ESTIMATE = unmeasured "
        "prior, calibrate before funding.",
        "",
        "Note: the \"diff_arg (lever)\" evidence shape is NECESSARY BUT NOT "
        "SUFFICIENT — it screens shape, not root cause. Regswap-cascade, "
        "FPR-scheduling, and CSE/FMA walls all hide inside the same "
        "reloc-explained-indel shape, which is why the measured BODY-LEVER flip "
        "rate collapsed from the ~80% shape-prior to 25%/≤5% per stratum.",
        "",
    ]

    # ── ground-truth calibration appendix (24-fn grind campaign) ──────────
    lines += [
        "## Ground-truth calibration (24-fn grind campaign, 2026-07-19)",
        "",
        "| Predicted | FLIP | IMPROVED | AT_LIMIT | STUCK |",
        "|---|---:|---:|---:|---:|",
        "| BODY-PORT | 4 | 2 | 2 | 0 |",
        "| FORM-DIVERGENCE | 1 | 0 | 3 | 0 |",
        "| NEEDS-REVIEW | 2 | 1 | 8 | 0 |",
        "| RELOC-COLOC | 0 | 0 | 1 | 0 |",
        "",
        "NEEDS-REVIEW at_limit mass sits at 95.9-98.6% live — the survivor band; "
        "vtordisp/dead-arg detectors (R5a/R5b) now auto-skip most of it. 3 pool "
        "entries were absent from decomp.db (get_attempts not-found) — coordinator "
        "to re-ingest.",
        "",
    ]

    # ── fundable-fleet expected-flip summary ──────────────────────────────
    counts = Counter(res["bucket"] for _, res in
                     [(row, results[(row["unit"], row["name"])]) for row in rows])
    body_78_96 = sum(
        1 for row in rows
        if results[(row["unit"], row["name"])]["bucket"] == B_BODY
        and isinstance(results[(row["unit"], row["name"])]["live_pct"], (int, float))
        and 78.0 <= results[(row["unit"], row["name"])]["live_pct"] <= 96.0)

    # BODY-LEVER is now priced PER-STRATUM (30-fn calibration wave):
    #   0.25 x (70-90 live count) + 0.05 x (rest). All BODY-LEVER rows are
    #   non-STL by construction (STL-CONTAM is peeled off before the grant).
    bl_7090 = sum(
        1 for row in rows
        if results[(row["unit"], row["name"])]["bucket"] == B_BODY_LEVER
        and results[(row["unit"], row["name"])].get("stratum") == "70-90")
    bl_total = counts.get(B_BODY_LEVER, 0)
    bl_rest = bl_total - bl_7090
    bl_expected = 0.25 * bl_7090 + 0.05 * bl_rest

    # (label, count, prior, basis) — BODY-LEVER handled explicitly above.
    terms = [
        ("LEVER-SYMBOL", counts.get(B_LEVER_SYMBOL, 0), 0.90, "VALIDATED"),
        ("LEVER-STRING", counts.get(B_LEVER_STRING, 0), 0.85, "VALIDATED"),
        ("ZS-MISSING-INSTANTIATION", counts.get(B_ZS_MISSINST, 0), 0.90, "PROBE"),
        ("STL-CONTAM", counts.get(B_STL_CONTAM, 0), 0.00, "MEASURED"),
        ("BODY-PORT(78-96)", body_78_96, 0.45, "VALIDATED"),
        ("FORM-DIVERGENCE", counts.get(B_FORM, 0), 0.30, "ESTIMATE"),
        ("STRUCT-ARTIFACT", counts.get(B_STRUCT, 0), 0.60, "ESTIMATE"),
    ]
    # Bankable = evidence-backed (MEASURED/PROBE/VALIDATED); estimates listed apart.
    bankable_terms = [t for t in terms if t[3] != "ESTIMATE"]
    estimate_terms = [t for t in terms if t[3] == "ESTIMATE"]

    bl_str = (f"BODY-LEVER {bl_7090}x0.25 (70-90) + {bl_rest}x0.05 (rest) "
              f"= {bl_expected:.1f}")
    bankable_parts = "; ".join(
        f"{lbl} {n}x{p:.2f}={n*p:.1f}" for lbl, n, p, _ in bankable_terms)
    bankable_total = bl_expected + sum(n * p for _, n, p, _ in bankable_terms)
    est_parts = "; ".join(
        f"{lbl} {n}x{p:.2f}={n*p:.1f}" for lbl, n, p, _ in estimate_terms)
    est_total = sum(n * p for _, n, p, _ in estimate_terms)

    lines += [
        "## Fundable-fleet expected strict flips",
        "",
        f"**BODY-LEVER (MEASURED, per-stratum):** {bl_str}. "
        f"(non-STL 70-90 count = {bl_7090}; rest = {bl_rest}; total BODY-LEVER = "
        f"{bl_total}.)",
        "",
        f"**Bankable subtotal (MEASURED / PROBE / VALIDATED evidence only):** "
        f"{bl_str}; {bankable_parts}. BANKABLE TOTAL expected strict flips: "
        f"{bankable_total:.1f}.",
        "",
        f"**Unpriced upside (ESTIMATE — calibrate before funding):** {est_parts}. "
        f"Estimate-only expected flips (do NOT bank): {est_total:.1f}.",
        "",
    ]

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
            # raw live fuzzy% (None if the base emits nothing) + pool/report pct,
            # kept for the post-processing staleness guard (do NOT coalesce raw
            # live to pool pct here -- that would mask report0-live-diff rows).
            "_raw_live": live_pct if isinstance(live_pct, (int, float)) else None,
            "_pool_pct": row["pct"],
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
                b, c, e = classify_l2(row, rec, l2, base_ev, ctx)
                entry.update(bucket=b, confidence=c, evidence=e, layer=2)
            else:
                # exceeded l2 budget -> deferred manual triage
                vc = (rec.get("verdict") or {}).get("classification", "unknown") if rec else "no-rec"
                entry.update(bucket=B_REVIEW, confidence="low",
                             evidence=base_ev + [f"l2-budget-skipped; verdict {vc}"], layer=1)
        elif entry["bucket"] == B_BODY and key in fetch_set and row["name"] in l2_by_sym:
            # L1 body fast-path: if its optional L2 fetch arrived, re-run the full
            # L2 chain so the higher-priority lever/wall detectors (R6a/R6b/R5a/
            # R5b/R3) and the BODY-LEVER split (R2) apply at proper priority. This
            # is how ShowBriefBandMessage (77% + global-Symbol lever) reaches
            # LEVER-SYMBOL despite tripping the L1 indel fast path.
            l2 = l2_by_sym[row["name"]]
            if l2:
                b, c, e = classify_l2(row, rec, l2, base_ev, ctx)
                entry.update(bucket=b, confidence=c, evidence=e, layer=2)

    # ── Staleness guard: report-vs-live pct divergence (calibration 2026-07-19)
    # Runs before bucket finalization, over ALL nonzero-routed rows. If the raw
    # live fuzzy% (missing -> 0) diverges from the pool/report pct by >50, the
    # pairing is stale / unstable -> UNRELIABLE-EVIDENCE, keeping the original
    # bucket in the evidence. Supersedes fleet routing for the report0-live-diff
    # (Z4) rows -- intended. Known instance: a GemManager row read live 100.0 but
    # a cache-cleared report shows a 0-byte stub.
    for row in rows:
        entry = results[(row["unit"], row["name"])]
        b = entry.get("bucket")
        if b not in NONZERO_BUCKETS or b == B_UNRELIABLE:
            continue
        raw_live = entry.get("_raw_live")
        live = raw_live if isinstance(raw_live, (int, float)) else 0.0
        pool = entry.get("_pool_pct", row["pct"])
        if abs(live - pool) > 50:
            entry["evidence"] = (entry.get("evidence") or []) + [
                f"was {b}; live {live:.1f} vs pool {pool:.1f} — stale/pairing-unstable"]
            entry["bucket"] = B_UNRELIABLE
            entry["confidence"] = "low"

    # ── stratum tag for BODY-LEVER rows (per-stratum pricing) ────────────
    for entry in results.values():
        if entry.get("bucket") == B_BODY_LEVER:
            entry["stratum"] = _stratum(entry.get("live_pct"))

    # ── R4: survivor-band inversion (post-processing) ────────────────────
    # 97.5-99.8% live is the wall-dominated survivor band; the vtordisp/dead-arg
    # detectors already skim the walls off, so what's left here is low-yield.
    # Demote confidence one step (do NOT rebucket) for the still-fundable body/
    # form/review buckets; leave LEVER-* and ZS-* untouched.
    _CONF_DEMOTE = {"high": "medium", "medium": "low", "low": "low"}
    _R4_BUCKETS = {B_BODY, B_BODY_LEVER, B_FORM, B_REVIEW}
    for entry in results.values():
        if entry["bucket"] in _R4_BUCKETS:
            lp = entry.get("live_pct")
            if isinstance(lp, (int, float)) and 97.5 <= lp < 99.8:
                entry["confidence"] = _CONF_DEMOTE.get(
                    entry.get("confidence"), entry.get("confidence"))
                entry["evidence"] = (entry.get("evidence") or []) + [
                    "survivor-band 97.5-99.8 (wall-dominated)"]

    write_json(args.out, project, args.pool, rows, results)
    write_markdown(args.buckets_md, rows, results)
    print(f"[out] {args.out}")
    print(f"[out] {args.buckets_md}")
    print_table(results)
    print(f"\nelapsed {time.time()-t0:.1f}s")


if __name__ == "__main__":
    main()
