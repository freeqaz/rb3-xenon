#!/usr/bin/env python3
"""member_delta_finder2.py — CLASSIFY member-offset delta candidates.

v2 adds a CLASSIFY stage on top of v1's detection core. Candidates are tagged:

  MEMBER_DELTA  — true positive: a genuinely added/dropped member. Fix =
                  one-line header edit. Validated wins: CharSleeve/CharIKSliderMidi
                  mMe (+3), RndTexRenderer rev-13→rev-11 members (+6), NgPostProc
                  DC3-only floats (+2), Gem::Tail pad (+1).

  SIZED_VECTOR  — STLport gap: retail used 8-byte sized vectors (ptr + u16 size +
                  u16 cap), our DC3-derived STLport uses 12-byte (3 ptrs). EVERY
                  class with a std::vector member is shifted +4 at/above that member.
                  Validated instance: VocalPlayer (~11 fns @99.9%, uniform delta -4).
                  Signal: delta == -4 AND the class (or an annotated base) has a
                  std::vector member whose offset <= threshold. Also catches -8 for
                  two vectors, etc.

  VBASE_WALL    — coupled-base / virtual-base displacement. Detection: the unit's
                  dtk-split target .obj COFF contains ??_8 vbtable OR ??_E/??_D/??_G
                  virtual-dtor/adjustor-thunk symbols for the class. Deltas are often
                  non-uniform (different subobject sizes). Defer — multi-TU base
                  reconstruction, no clean oracle.
                  Validated instance: GameMode/Player (PropertyEventProvider→MsgSource
                  virtual base hierarchy).

  UNKNOWN       — none of the above explain it; may be worth manual recon.

New command-line option --classify-only: skip the expensive objdiff scan and re-
classify from an existing v1 JSON output. Useful for iterating on classification
logic.

Usage:
  # Full run (scan + classify):
  python3 tools/member_delta_finder2.py --tp /tmp/true_progress.json \\
      --proj /path/to/worktree --out ~/tmp/forcemult/mdf2_candidates.json

  # Re-classify existing v1 output:
  python3 tools/member_delta_finder2.py --classify-only \\
      --v1-json ~/tmp/forcemult/member_candidates.json \\
      --out ~/tmp/forcemult/mdf2_candidates.json

  # Validation run (known wins + false candidates):
  python3 tools/member_delta_finder2.py --validate \\
      --proj /path/to/worktree

  # Quick smoke (limit to first N fns by size):
  python3 tools/member_delta_finder2.py --limit 100 --proj ...
"""
import sys
import os
import re
import json
import struct
import subprocess
import argparse
from collections import defaultdict, Counter
from typing import Dict, List, Optional, Set, Tuple

# ── import v1 core ────────────────────────────────────────────────────────────
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)
from member_delta_finder import (
    diff_fn,
    collect_accesses,
    analyze_fn,
    demangle_class,
)

# THUNK_RE / STL_HELPER_RE are defined inside v1's main(); duplicate them here.
THUNK_RE = re.compile(r'^\?\?_[GDEFB]|^\?\?__[EF]')
STL_HELPER_RE = re.compile(r'@stlpmtx_std@@|^\?\?\$_(Destroy|Copy|Uninitialized|Move|Fill)')

ROOT = os.path.dirname(_HERE)
BUILD = os.path.join(ROOT, 'build', '45410914')
SRC_ROOT = os.path.join(ROOT, 'src')

# ── COFF symbol reader ────────────────────────────────────────────────────────

VBASE_SYM_RE = re.compile(r'^\?\?_[8EDG]|^\?\?_E')  # vbtable, vbase-dtor, adjustor-thunks

def get_coff_symbols(obj_path: str) -> List[str]:
    """Return all COFF symbol names from a .obj file."""
    try:
        with open(obj_path, 'rb') as f:
            data = f.read()
        if len(data) < 20:
            return []
        machine, num_sections, ts, sym_ptr, num_syms = struct.unpack_from('<HHIII', data, 0)
        if sym_ptr == 0 or num_syms == 0:
            return []
        str_table_off = sym_ptr + num_syms * 18
        if str_table_off > len(data):
            return []
        off = sym_ptr
        i = 0
        syms = []
        while i < num_syms and off + 18 <= len(data):
            name_raw = data[off:off + 8]
            zeroes = struct.unpack_from('<I', name_raw)[0]
            if zeroes == 0:
                str_off = struct.unpack_from('<I', name_raw, 4)[0]
                end = data.find(b'\x00', str_table_off + str_off)
                if end == -1:
                    end = str_table_off + str_off + 256
                name = data[str_table_off + str_off:end].decode('latin-1', errors='replace')
            else:
                name = name_raw.rstrip(b'\x00').decode('latin-1', errors='replace')
            syms.append(name)
            aux_count = data[off + 17]
            off += 18 * (1 + aux_count)
            i += 1 + aux_count
        return syms
    except Exception:
        return []


def has_vbase_symbols(unit: str, class_name: str) -> Tuple[bool, List[str]]:
    """Check if the dtk-split target .obj for *unit* carries virtual-base symbols
    for *class_name* (or any class in the unit, as a unit-level signal).

    Returns (has_vbase: bool, evidence: [symbol_names]).
    """
    # Target obj naming: build/45410914/obj/<basename>.obj
    # or build/45410914/obj/<subpath>.obj for band3/network units
    base = unit.replace('default/', '', 1)
    candidates = [
        os.path.join(BUILD, 'obj', os.path.basename(base) + '.obj'),
        os.path.join(BUILD, 'obj', base + '.obj'),
    ]
    for path in candidates:
        if os.path.exists(path):
            syms = get_coff_symbols(path)
            vbase = [s for s in syms if VBASE_SYM_RE.match(s)]
            return (len(vbase) > 0), vbase
    return False, []


# ── Header scanner for std::vector members ───────────────────────────────────

# Pre-compiled regexes
_OFF_RE = re.compile(r'//\s*(0x[0-9A-Fa-f]+)')
_CLASS_DECL_RE = re.compile(r'^\s*(?:class|struct)\s+(\w+)\s*(?::.*?)?\{')
_BASE_DECL_RE = re.compile(r'^\s*(?:class|struct)\s+\w+\s*:\s*(.*?)\s*\{')
_BASE_NAME_RE = re.compile(r'(?:public|private|protected|virtual)\s+(\w+)')
_VEC_DECL_RE = re.compile(r'std::vector\s*<|vector<')
_MEM_DECL_RE = re.compile(r'(\w+)\s*(?:\[[^\]]*\])?\s*;\s*//')


class HeaderDB:
    """Parsed class layout database from // 0xHEX annotated headers."""

    def __init__(self, src_root: str):
        self._vec_members: Dict[str, List[Tuple[int, str]]] = {}   # class -> [(off, name)]
        self._class_bases: Dict[str, List[str]] = {}                # class -> [base_names]
        self._virt_bases: Dict[str, bool] = {}                      # class -> has_virtual_base
        self._loaded = False
        self._src_root = src_root

    def _ensure_loaded(self):
        if self._loaded:
            return
        self._loaded = True
        self._scan(self._src_root)

    def _scan(self, src_root: str):
        for root, dirs, files in os.walk(src_root):
            # Skip build artifacts directories
            dirs[:] = [d for d in dirs if d not in ('__pycache__', '.git')]
            for fn in files:
                if not fn.endswith('.h'):
                    continue
                path = os.path.join(root, fn)
                self._parse_header(path)

    def _parse_header(self, path: str):
        try:
            with open(path) as f:
                content = f.read()
        except Exception:
            return

        lines = content.split('\n')
        # Stack of (class_name, brace_depth_at_open) to handle nested classes
        class_stack: List[Tuple[str, int]] = []
        brace_depth = 0
        current_class: Optional[str] = None

        for line in lines:
            stripped = line.strip()

            # Class declaration
            cm = _CLASS_DECL_RE.match(stripped)
            if cm:
                cls_name = cm.group(1)
                # Extract base classes
                bm = _BASE_DECL_RE.match(stripped)
                bases: List[str] = []
                has_virt = False
                if bm:
                    base_str = bm.group(1)
                    for bname in _BASE_NAME_RE.findall(base_str):
                        bases.append(bname)
                    if 'virtual' in base_str:
                        has_virt = True
                # Push onto stack
                class_stack.append((cls_name, brace_depth))
                self._class_bases.setdefault(cls_name, []).extend(bases)
                if has_virt:
                    self._virt_bases[cls_name] = True
                current_class = cls_name

            # Track braces
            open_count = stripped.count('{')
            close_count = stripped.count('}')
            brace_depth += open_count - close_count

            # Pop class stack when we go back to the brace depth where it opened
            while class_stack and brace_depth <= class_stack[-1][1]:
                class_stack.pop()
            current_class = class_stack[-1][0] if class_stack else None

            # Vector member declarations: must have a semicolon AND an offset annotation
            if current_class and ';' in stripped and '//' in stripped:
                if _VEC_DECL_RE.search(stripped):
                    off_m = _OFF_RE.search(stripped)
                    if off_m:
                        off = int(off_m.group(1), 16)
                        name_m = _MEM_DECL_RE.search(stripped)
                        mem_name = name_m.group(1) if name_m else '?'
                        if current_class not in self._vec_members:
                            self._vec_members[current_class] = []
                        self._vec_members[current_class].append((off, mem_name))

    def vector_members(self, cls: str) -> List[Tuple[int, str]]:
        """Return annotated vector members for *cls* or [], sorted by offset."""
        self._ensure_loaded()
        return sorted(self._vec_members.get(cls, []), key=lambda x: x[0])

    def bases(self, cls: str) -> List[str]:
        """Return direct base class names for *cls*."""
        self._ensure_loaded()
        return self._class_bases.get(cls, [])

    def has_virtual_base(self, cls: str) -> bool:
        """Returns True if *cls* directly uses virtual inheritance."""
        self._ensure_loaded()
        return self._virt_bases.get(cls, False)

    def vector_members_inherited(self, cls: str, _visited: Optional[Set[str]] = None,
                                  _depth: int = 0) -> List[Tuple[int, str]]:
        """Return vector members from *cls* AND its direct/indirect bases (up to 4 levels)."""
        if _visited is None:
            _visited = set()
        if cls in _visited or _depth > 4:
            return []
        _visited.add(cls)
        result = list(self.vector_members(cls))
        for base in self.bases(cls):
            result.extend(self.vector_members_inherited(base, _visited, _depth + 1))
        return sorted(result, key=lambda x: x[0])


# Singleton
_header_db: Optional[HeaderDB] = None


def get_header_db(src_root: str = SRC_ROOT) -> HeaderDB:
    global _header_db
    if _header_db is None:
        _header_db = HeaderDB(src_root)
    return _header_db


# ── CLASSIFY stage ────────────────────────────────────────────────────────────

# The per-vector size delta in bytes (12-byte vs 8-byte = +4 per vector member
# at/below the shifted region). Target is smaller, so delta = target - base < 0.
SIZED_VEC_DELTA_PER = -4   # one vector member → delta = -4


def classify_candidate(candidate: dict, units: List[str], hdb: HeaderDB) -> dict:
    """Add classification to a v1 candidate dict.

    Adds:
      candidate['classification'] = 'MEMBER_DELTA' | 'SIZED_VECTOR' | 'VBASE_WALL' | 'UNKNOWN'
      candidate['class_evidence'] = {...}  explanatory evidence for the classification
    """
    cls = candidate['class']
    delta: int = candidate['delta']
    threshold: int = candidate['offset_int']
    evidence: dict = {}

    # ── 1. VBASE_WALL check: does any unit carry virtual-base COFF symbols? ──
    all_vbase_syms: List[str] = []
    for unit in units:
        has_vb, vb_syms = has_vbase_symbols(unit, cls)
        if has_vb:
            all_vbase_syms.extend(vb_syms)
    # Also check if the class itself or its inherited bases use virtual inheritance
    hdb_virt = hdb.has_virtual_base(cls) or any(
        hdb.has_virtual_base(b) for b in hdb.bases(cls)
    )

    if all_vbase_syms or hdb_virt:
        evidence['vbase_symbols'] = list(set(all_vbase_syms))[:8]
        evidence['header_virt_base'] = hdb_virt
        candidate['classification'] = 'VBASE_WALL'
        candidate['class_evidence'] = evidence
        candidate['action'] = (
            f"DEFER: {cls} uses virtual inheritance. The offset delta "
            f"(C={delta}) is a vbase-displacement artifact, not a member add/remove. "
            f"Requires multi-TU base reconstruction. vbase evidence: "
            f"{evidence['vbase_symbols'][:4]}"
        )
        return candidate

    # ── 2. SIZED_VECTOR check ─────────────────────────────────────────────────
    # Detect: delta is a negative multiple of 4, AND the class (or an inherited
    # base) has one or more std::vector members at offsets <= threshold such that
    # n_vecs_at_or_below * SIZED_VEC_DELTA_PER == delta.
    if delta < 0 and delta % SIZED_VEC_DELTA_PER == 0:
        expected_n_vecs = delta // SIZED_VEC_DELTA_PER  # positive int
        all_vecs = hdb.vector_members_inherited(cls)
        # Vectors at or below the threshold offset (target side)
        vecs_at_or_below = [(off, name) for off, name in all_vecs if off <= threshold]
        # Vectors in the shifted range (between the lowest shifted access and threshold)
        # More precisely: vectors whose offset is <= threshold
        n_below = len(vecs_at_or_below)

        evidence['vector_members_inherited'] = [(hex(o), n) for o, n in all_vecs]
        evidence['vector_members_at_or_below_threshold'] = [
            (hex(o), n) for o, n in vecs_at_or_below
        ]
        evidence['expected_n_vecs_for_delta'] = expected_n_vecs
        evidence['n_vectors_at_or_below'] = n_below

        # Strong match: number of vectors at/below matches the expected count exactly
        strong_match = (n_below == expected_n_vecs) and (n_below > 0)
        # Plausible match: at least one vector exists at/below and the delta is a
        # multiple of 4 (even if count doesn't perfectly explain it)
        plausible = (n_below > 0) and (delta == SIZED_VEC_DELTA_PER * n_below or
                                        abs(delta) <= 4 * max(1, n_below))

        if strong_match or plausible:
            confidence = 'strong' if strong_match else 'plausible'
            evidence['sized_vector_match'] = confidence
            candidate['classification'] = 'SIZED_VECTOR'
            candidate['class_evidence'] = evidence
            candidate['action'] = (
                f"SIZED_VECTOR ({confidence}): {cls} has {n_below} vector member(s) "
                f"at/below threshold 0x{threshold:x} (expected {expected_n_vecs} for "
                f"delta={delta}). These are 12-byte (3-ptr) in our build but 8-byte "
                f"(ptr+u16+u16) in retail. Fix: enable _STLP_USE_SIZED_VECTOR or use "
                f"VECTOR_SIZE_SMALL on those members. "
                f"Vectors at/below: {[(hex(o), n) for o, n in vecs_at_or_below]}"
            )
            return candidate

        # If no annotated vector members found but delta is -4 and class is in
        # band3 (game code), flag as SIZED_VECTOR_CANDIDATE (annotations may be missing)
        is_game = any('/band3/' in u or '/network/' in u for u in units)
        if delta == -4 and is_game and all_vecs:
            evidence['note'] = 'game class with -4 delta but no vectors exactly at threshold'
            candidate['classification'] = 'SIZED_VECTOR'
            candidate['class_evidence'] = evidence
            candidate['action'] = (
                f"SIZED_VECTOR (heuristic): {cls} is a game-code class with delta=-4 "
                f"and has vector members, but none annotated exactly at/below 0x{threshold:x}. "
                f"Annotations may be incomplete. Check {cls}'s inherited layout carefully."
            )
            return candidate

    # ── 3. MEMBER_DELTA (true positive) ───────────────────────────────────────
    # No vbase, not explained by sized-vector. This is a genuine member pad.
    all_vecs = hdb.vector_members_inherited(cls)
    evidence['vector_members_inherited'] = [(hex(o), n) for o, n in all_vecs]
    evidence['note'] = (
        'no vbase symbols detected, delta not explained by sized-vector members'
    )

    # Minimum evidence guard: a single method with a single shifted access is
    # AMBIGUOUS — we cannot distinguish a layout shift from a value/logic change
    # (e.g. FreestyleMotionFilter::Deactivate writes different constants to
    # different offsets — that's a logic change, not a layout shift). Require
    # either (a) ≥2 methods agree on the delta, OR (b) n_shifted ≥ 2 in total
    # across all methods (i.e., multiple distinct shifted accesses observed).
    n_methods = candidate.get('n_affected', 0)
    methods_list = candidate.get('methods', [])
    # We don't have per-method n_shifted here directly, but we have threshold_hist
    # and n_affected. A single method with n_affected==1 is weak.
    # Use n_affected as the primary gate.
    if n_methods == 1:
        # Single method: require n_shifted >= 2 from the per-fn analysis
        # We don't have it cached in the candidate directly; use the threshold
        # histogram as a proxy (if threshold_hist has only one entry with count 1,
        # the total shifted count was 1 across the whole group).
        thr_hist = candidate.get('threshold_hist', {})
        total_group_count = sum(thr_hist.values())
        if total_group_count <= 1:
            evidence['low_evidence_note'] = (
                'single method, single shifted access — cannot distinguish layout '
                'shift from value/logic change; demoting to UNKNOWN'
            )
            candidate['classification'] = 'UNKNOWN'
            candidate['class_evidence'] = evidence
            candidate['action'] = (
                f"UNKNOWN (low evidence): {cls} shows delta={delta} at 0x{threshold:x} "
                f"in only 1 method with only 1 shifted access. This may be a logic "
                f"difference rather than a layout shift. Needs manual recon."
            )
            return candidate

    candidate['classification'] = 'MEMBER_DELTA'
    candidate['class_evidence'] = evidence
    candidate['action'] = (
        f"MEMBER_DELTA: {cls} likely has a genuine added/dropped member at offset "
        f"~0x{threshold:x} (delta={delta}, {abs(delta)} bytes). This is a one-line "
        f"header fix. " + candidate.get('fix_hint', '')
    )
    return candidate


# ── v1 scan core (re-exposed here) ───────────────────────────────────────────

CLASS_RE = re.compile(r'^\?[~A-Za-z0-9_]+@([A-Za-z0-9_]+)@')


def run_scan(pool, proj, hdb) -> Tuple[List[dict], List[dict]]:
    """Run the full v1 scan + v2 classify pipeline. Returns (candidates, per_fn_out)."""
    class_fns = defaultdict(list)
    per_fn_out = []

    for i, r in enumerate(pool):
        if i and i % 50 == 0:
            print(f"  {i}/{len(pool)}", file=sys.stderr)
        unit, sym = r['unit'], r['sym']
        d = diff_fn(unit, sym, proj)
        if not d:
            continue
        acc = collect_accesses(d)
        an = analyze_fn(acc)
        if not an:
            continue
        cls = demangle_class(sym, unit)
        rec = {'unit': unit, 'sym': sym, 'mp': r.get('mp'), 'class': cls, 'analysis': an}
        per_fn_out.append(rec)
        class_fns[cls].append(rec)

    candidates = _build_candidates(class_fns, hdb)
    return candidates, per_fn_out


def _build_candidates(class_fns: dict, hdb: HeaderDB) -> List[dict]:
    """Build and classify candidates from per-class function data (v1 grouping + v2 classify)."""
    candidates = []

    for cls, fns in class_fns.items():
        this_clean = [f for f in fns if f['analysis'].get('this') and f['analysis']['this']['clean']]
        if not this_clean:
            continue
        by_C = defaultdict(list)
        for f in this_clean:
            by_C[f['analysis']['this']['delta']].append(f)

        for C, group in by_C.items():
            thresholds = [g['analysis']['this']['threshold'] for g in group]
            T_min = min(thresholds)
            T_counter = Counter(thresholds)
            T_mode = T_counter.most_common(1)[0][0]
            n_aff = len(group)
            if T_min < 0:
                continue

            other_deltas = set(by_C.keys()) - {C}
            coupled_warn = len(other_deltas) > 0
            has_boundary = any(g['analysis']['this'].get('n_match_below', 0) > 0
                               for g in group)
            confidence = 'high' if (has_boundary and not coupled_warn) else (
                'low' if coupled_warn else 'medium')
            avg_cons = sum(g['analysis']['this']['consistency'] for g in group) / n_aff
            thr_agree = T_counter.most_common(1)[0][1] / n_aff
            score = n_aff * avg_cons * thr_agree * (1.5 if has_boundary else 1.0)
            units = sorted(set(g['unit'] for g in group))
            is_game = any('/band3/' in u or '/network/' in u for u in units)
            src_word = 'rb3-Wii (game code)' if is_game else 'DC3 (engine; newer twin)'
            direction = (
                'our build (base) is %d bytes LARGER than retail -> EXTRA member in base; '
                'REMOVE/shrink it (cross-check %s)' % (-C, src_word)
            ) if C < 0 else (
                'our build (base) is %d bytes SMALLER -> retail has a member we LACK; '
                'ADD it (cross-check %s)' % (C, src_word)
            )
            off_hex = '0x%x' % T_min
            fix_hint = (
                f"Class {cls}: uniform this-relative member-offset delta C={C} "
                f"(0x{abs(C):x}) across {n_aff} method(s) at threshold ~{off_hex}. "
                f"{direction}. "
                f"Insert/remove a {abs(C)}-byte member at ~{off_hex} in the {cls} header. "
                + (f"Cross-check rb3-Wii oracle. "
                   if is_game else
                   f"Cross-check DC3 (grep ../dc3-decomp for class {cls}). ")
                + (f"CONFIDENCE high: boundary directly observed." if has_boundary else
                   f"CONFIDENCE medium: all accesses uniformly shifted — may be base class.")
            )
            if coupled_warn:
                fix_hint += (
                    f" WARNING: other clean deltas {sorted(other_deltas)} also seen "
                    f"-> possible coupled-base/vbase; investigate before applying."
                )

            cand = {
                'class': cls, 'delta': C, 'offset': off_hex, 'offset_int': T_min,
                'offset_mode': '0x%x' % T_mode,
                'n_affected': n_aff, 'units': units,
                'consistency': round(avg_cons, 3),
                'threshold_agreement': round(thr_agree, 3),
                'threshold_hist': {('0x%x' % k): v for k, v in T_counter.items()},
                'coupled_base_warning': coupled_warn,
                'has_boundary': has_boundary,
                'confidence': confidence,
                'score': round(score, 3),
                'methods': [g['sym'] for g in group],
                'fix_hint': fix_hint,
            }

            # ── v2: classify ───────────────────────────────────────────────
            classify_candidate(cand, units, hdb)
            candidates.append(cand)

    candidates.sort(key=lambda c: (
        # Rank: MEMBER_DELTA first (actionable), then SIZED_VECTOR, then rest
        {'MEMBER_DELTA': 0, 'SIZED_VECTOR': 1, 'UNKNOWN': 2, 'VBASE_WALL': 3}.get(
            c.get('classification', 'UNKNOWN'), 2),
        -c['score']
    ))
    return candidates


def classify_from_v1(v1_json: str, hdb: HeaderDB) -> List[dict]:
    """Re-classify candidates from an existing v1 JSON output without re-scanning."""
    v1 = json.load(open(v1_json))
    for cand in v1['candidates']:
        classify_candidate(cand, cand.get('units', []), hdb)
    return v1['candidates']


# ── Validation harness ────────────────────────────────────────────────────────

# Known ground truth: (sym_fragment, expected_class, expected_classification)
KNOWN_WINS: List[Tuple[str, str, str]] = [
    # CharSleeve/CharIKSliderMidi mMe: +3 (C=-12), commit 9218bbf
    # NOTE: these are now at 100% (fix already landed); NOT_FOUND is expected.
    # ('CharSleeve', 'CharSleeve', 'MEMBER_DELTA'),
    # ('CharIKSliderMidi', 'CharIKSliderMidi', 'MEMBER_DELTA'),
    # RndTexRenderer rev-13→rev-11: +6 (C=-?), commit 9150f3c
    # NOTE: remaining diffs are NAME_RELOC not layout → NOT_FOUND is correct.
    # ('RndTexRenderer', 'RndTexRenderer', 'MEMBER_DELTA'),
    # NgPostProc DC3-only floats: +2 (C=-8), commit 2d82a94
    # NOTE: remaining diffs are NAME_RELOC/constant not layout → NOT_FOUND is correct.
    # ('NgPostProc', 'NgPostProc', 'MEMBER_DELTA'),
]

KNOWN_FALSE_POSITIVES: List[Tuple[str, str, str]] = [
    # VocalPlayer: SIZED_VECTOR (8-byte vs 12-byte vector)
    ('VocalPlayer', 'VocalPlayer', 'SIZED_VECTOR'),
    # GameMode/Player: VBASE_WALL (PropertyEventProvider→MsgSource virtual hierarchy)
    ('GameMode', 'GameMode', 'VBASE_WALL'),
    ('Player', 'Player', 'VBASE_WALL'),
]


def run_validation(proj: str, tp_path: str, hdb: HeaderDB):
    """Run the tool on known-win + false-positive examples and report classification."""
    print("\n=== VALIDATION RUN ===\n")
    # Build a tiny pool covering all validation examples
    tp = json.load(open(tp_path))

    def pool_for_classes(cls_names):
        # Accept either the current 'HAS_REAL' bucket or the legacy 'STRUCT_WORK'
        # name (older true_progress outputs) -- otherwise validation silently
        # scans 0 fns, exactly the bug we're fixing.
        return [r for r in tp['rows']
                if r.get('bucket') in ('HAS_REAL', 'STRUCT_WORK')
                and any(cls in r.get('sym', '') for cls in cls_names)]

    all_cls = [cls for _, cls, _ in KNOWN_WINS + KNOWN_FALSE_POSITIVES]
    pool = pool_for_classes(all_cls)
    print(f"Validation pool: {len(pool)} fns for classes {all_cls}")

    candidates, _ = run_scan(pool, proj, hdb)

    # Check each expected result
    confusion: Dict[str, Dict[str, int]] = {}   # expected -> got -> count
    results = []
    all_cases = KNOWN_WINS + KNOWN_FALSE_POSITIVES

    for _, cls, expected in all_cases:
        found = [c for c in candidates if c['class'] == cls]
        if not found:
            got = 'NOT_FOUND'
        else:
            # Take the highest-scored candidate for that class
            c = sorted(found, key=lambda x: -x['score'])[0]
            got = c.get('classification', 'UNKNOWN')

        correct = (got == expected)
        results.append({
            'class': cls,
            'expected': expected,
            'got': got,
            'correct': correct,
        })
        confusion.setdefault(expected, {})
        confusion[expected][got] = confusion[expected].get(got, 0) + 1
        marker = '✓' if correct else '✗'
        print(f"  {marker} {cls}: expected={expected}, got={got}")

    n_correct = sum(1 for r in results if r['correct'])
    n_total = len(results)
    print(f"\nAccuracy: {n_correct}/{n_total}")

    print("\nConfusion matrix:")
    hdr = 'expected\\got'
    print(f"{hdr:<20}", end='')
    all_labels = sorted(set(g for row in confusion.values() for g in row) | {'NOT_FOUND'})
    for lbl in all_labels:
        print(f" {lbl[:12]:>12}", end='')
    print()
    for exp, row in sorted(confusion.items()):
        print(f"{exp:<20}", end='')
        for lbl in all_labels:
            print(f" {row.get(lbl, 0):>12}", end='')
        print()

    return results, confusion


# ── Summary formatter ─────────────────────────────────────────────────────────

def print_summary(candidates: List[dict], top_n: int = 20):
    n_md = sum(1 for c in candidates if c.get('classification') == 'MEMBER_DELTA')
    n_sv = sum(1 for c in candidates if c.get('classification') == 'SIZED_VECTOR')
    n_vb = sum(1 for c in candidates if c.get('classification') == 'VBASE_WALL')
    n_uk = sum(1 for c in candidates if c.get('classification') == 'UNKNOWN')

    print(f"\n=== member_delta_finder2 — STRUCT_WORK candidate summary ===")
    print(f"Total candidates: {len(candidates)}  "
          f"MEMBER_DELTA={n_md}  SIZED_VECTOR={n_sv}  "
          f"VBASE_WALL={n_vb}  UNKNOWN={n_uk}")
    print()
    print(f"{'#':>3} {'cls':<22} {'C':>5} {'off':>7} {'#fn':>4} "
          f"{'score':>6} {'conf':>6} {'class':>15}")
    print('-' * 80)
    for i, c in enumerate(candidates[:top_n]):
        tag = {
            'MEMBER_DELTA': 'MEM',
            'SIZED_VECTOR': 'VEC',
            'VBASE_WALL': 'VBASE',
            'UNKNOWN': '???',
        }.get(c.get('classification', 'UNKNOWN'), '???')
        warn = ' [coupled?]' if c['coupled_base_warning'] else ''
        print(f"{i+1:>3} {c['class']:<22} {c['delta']:>5} {c['offset']:>7} "
              f"{c['n_affected']:>4} {c['score']:>6.2f} {c['confidence']:>6} "
              f"{tag:>15}{warn}")
    print()
    print("Top MEMBER_DELTA candidates (actionable):")
    md_cands = [c for c in candidates if c.get('classification') == 'MEMBER_DELTA']
    for c in md_cands[:10]:
        print(f"  {c['class']}: delta={c['delta']}, offset={c['offset']}, "
              f"n_affected={c['n_affected']}, score={c['score']:.2f}")
        print(f"    -> {c['action'][:120]}")
    print()
    print("Top SIZED_VECTOR candidates (need sized-vector STLport port):")
    sv_cands = [c for c in candidates if c.get('classification') == 'SIZED_VECTOR']
    for c in sv_cands[:10]:
        n_vecs = c.get('class_evidence', {}).get('n_vectors_at_or_below', '?')
        print(f"  {c['class']}: delta={c['delta']}, offset={c['offset']}, "
              f"n_affected={c['n_affected']}, n_vecs_at_below={n_vecs}")
    print()
    print("VBASE_WALL candidates (defer):")
    vb_cands = [c for c in candidates if c.get('classification') == 'VBASE_WALL']
    for c in vb_cands[:5]:
        print(f"  {c['class']}: delta={c['delta']}, "
              f"vbase_syms={c.get('class_evidence', {}).get('vbase_symbols', [])[:2]}")


# ── main ──────────────────────────────────────────────────────────────────────

def main():
    ap = argparse.ArgumentParser(description='member_delta_finder v2 — classify candidates')
    ap.add_argument('--tp', default='/tmp/true_progress.json',
                    help='true_progress.json path')
    ap.add_argument('--proj', default=ROOT, help='objdiff project dir (worktree)')
    ap.add_argument('--out', default=os.path.expanduser('~/tmp/forcemult/mdf2_candidates.json'),
                    help='output JSON path')
    ap.add_argument('--limit', type=int, default=0, help='limit to first N fns by size')
    ap.add_argument('--bucket', default='HAS_REAL',
                    help="true_progress bucket to scan. Default HAS_REAL: that is "
                         "the bucket tools/true_progress.py emits for the "
                         "struct/codegen near-miss pool (bucket_from_subs). "
                         "'STRUCT_WORK' is accepted as a legacy alias (old "
                         "true_progress.bucket() name) but current outputs have 0 "
                         "STRUCT_WORK rows.")
    ap.add_argument('--top', type=int, default=20, help='top N to show in summary')
    ap.add_argument('--classify-only', action='store_true',
                    help='skip scan; re-classify from --v1-json')
    ap.add_argument('--v1-json', default='',
                    help='existing v1 JSON to re-classify (use with --classify-only)')
    ap.add_argument('--validate', action='store_true',
                    help='run validation on known wins + false positives')
    ap.add_argument('--src-root', default=SRC_ROOT, help='source root for header scanning')
    a = ap.parse_args()

    print("Loading header DB...", file=sys.stderr)
    hdb = HeaderDB(a.src_root)
    hdb._ensure_loaded()
    print(f"  {len(hdb._vec_members)} classes with annotated vector members, "
          f"{len(hdb._class_bases)} classes with base info", file=sys.stderr)

    if a.validate:
        run_validation(a.proj, a.tp, hdb)
        return

    if a.classify_only:
        if not a.v1_json:
            print("ERROR: --classify-only requires --v1-json", file=sys.stderr)
            sys.exit(1)
        print(f"Re-classifying from {a.v1_json}...", file=sys.stderr)
        candidates = classify_from_v1(a.v1_json, hdb)
        per_fn_out = []  # not available in re-classify mode
    else:
        def keep(r):
            s = r['sym']
            if THUNK_RE.match(s) or STL_HELPER_RE.search(s):
                return False
            return True

        tp = json.load(open(a.tp))
        rows = tp.get('rows', [])
        # Bucket selection. 'STRUCT_WORK' is the OLD true_progress.bucket() name;
        # current true_progress.bucket_from_subs() emits 'HAS_REAL'. Accept the
        # legacy alias so old true_progress JSONs still work, but fall back to the
        # live name when the requested bucket has 0 rows.
        bucket = a.bucket
        if bucket == 'STRUCT_WORK' and not any(r.get('bucket') == 'STRUCT_WORK'
                                               for r in rows):
            if any(r.get('bucket') == 'HAS_REAL' for r in rows):
                print("[member_delta_finder2] NOTE: requested bucket "
                      "'STRUCT_WORK' has 0 rows; this true_progress output uses "
                      "the current 'HAS_REAL' bucket name -- scanning HAS_REAL "
                      "instead.", file=sys.stderr)
                bucket = 'HAS_REAL'
        pool = [r for r in rows if r.get('bucket') == bucket and keep(r)]

        # LOUD empty-pool warning -- the silent-0 scan was the actual bug this
        # tool used to hide behind (it would happily 'scan 0 fns' and emit 0
        # candidates with no signal that the bucket name was wrong).
        if not pool:
            present = sorted({r.get('bucket') for r in rows})
            print("\n" + "=" * 72, file=sys.stderr)
            print(f"[member_delta_finder2] WARNING: bucket {bucket!r} matched "
                  f"0 of {len(rows)} rows in {a.tp}.", file=sys.stderr)
            print(f"  Buckets actually present: {present}", file=sys.stderr)
            print("  Nothing to scan -> 0 candidates. If you meant the "
                  "near-miss struct/codegen pool, use --bucket HAS_REAL "
                  "(the default).", file=sys.stderr)
            print("=" * 72 + "\n", file=sys.stderr)

        pool.sort(key=lambda r: -r.get('size', 0))
        if a.limit:
            pool = pool[:a.limit]
        print(f"Scanning {len(pool)} {bucket} fns...", file=sys.stderr)
        candidates, per_fn_out = run_scan(pool, a.proj, hdb)

    print_summary(candidates, top_n=a.top)

    os.makedirs(os.path.dirname(a.out) if os.path.dirname(a.out) else '.', exist_ok=True)
    output = {
        'pool': a.bucket,
        'n_class_candidates': len(candidates),
        'classification_counts': {
            'MEMBER_DELTA': sum(1 for c in candidates if c.get('classification') == 'MEMBER_DELTA'),
            'SIZED_VECTOR': sum(1 for c in candidates if c.get('classification') == 'SIZED_VECTOR'),
            'VBASE_WALL': sum(1 for c in candidates if c.get('classification') == 'VBASE_WALL'),
            'UNKNOWN': sum(1 for c in candidates if c.get('classification') == 'UNKNOWN'),
        },
        'candidates': candidates,
        'per_fn': per_fn_out,
    }
    json.dump(output, open(a.out, 'w'), indent=1)
    print(f"\nWrote {a.out}")
    print(f"  {len(candidates)} candidates: "
          f"MEMBER_DELTA={output['classification_counts']['MEMBER_DELTA']}, "
          f"SIZED_VECTOR={output['classification_counts']['SIZED_VECTOR']}, "
          f"VBASE_WALL={output['classification_counts']['VBASE_WALL']}, "
          f"UNKNOWN={output['classification_counts']['UNKNOWN']}")


if __name__ == '__main__':
    main()
