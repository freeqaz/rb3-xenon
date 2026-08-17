#!/usr/bin/env python3
"""VOID-OUTPUT WINDOW 2026-08-16 -> 2026-08-17. The `args` spelling change zeroed
VBASE_VTBL_RE and four `len(tt) >= 3` detectors, so FUNCLET_PAIRING stopped
routing DEFER_DEEP (mdgrind MISROUTE-1) and FPR struct-offset diffs misrouted
to PERMUTE. Measured on 80 real functions: 11 get a different ROUTE and 17 a
different CLASS.

Every number this tool printed between the first rebuild carrying objdiff-cli
fdc5113 ("ruler I", committed 2026-08-16 08:34:03 UTC with its release binary
deliberately NOT rebuilt; confirmed live by 21:30 that day) and the repair
described below is VOID. Re-run it; do not carry it forward. Audit:
`ARGS_READER_AUDIT.md` in decomp-bench `archive/runs/objdiff-silent-flags-and-
dead-controls-2026-08-16/` (task #96); repair task #103. Swept 2026-08-17: NO
committed artifact in this repo, and no file at any of these tools' default
output paths, falls inside that window -- this banner exists for outputs held
outside git.

`--validate48` was DELETED 2026-08-17 (task #117).  It was not broken-by-absence
— its ground truth `tools/testdata/mdgrind_gt48.jsonl` is committed and it ran
to completion — it was VACUOUS.  Measured against the live tree that day:
29 of its 48 symbols no longer resolve (all 24 FUNCLET_PAIRING rows plus 5
others name `fn_<address>` funclets whose addresses moved when the units were
reorganised, so objdiff returns nothing and classify_fn answers CLEAN), and 11
of the surviving 19 are matched at 100% with zero differing instructions.  It
therefore printed a permanent 15/48 = 31.2% whose FUNCLET_PAIRING row read 0/24
because the symbols were gone, not because the classifier was wrong — a red that
can never go green, which teaches readers to ignore it.  Its "CRITICAL: genuine
MEMBER_DELTA must survive" section printed [OK] for three rows that were simply
already solved.  Reconstruction is not cheap: the funclet pairings are keyed on
addresses that no longer exist.  The jsonl stays in the repo as 2026-06-09
provenance, and its three still-live, still-discriminating rows were promoted
into VALIDATE_CASES below as independent (non-self-referential) ground truth.

wall_classify.py — Auto-tag HAS_REAL near-miss functions with playbook wall classes.

Implements the 8 wall detectors from docs/decomp/playbooks/hasreal-grind.md §3 plus
MEMBER_DELTA, FUNCLET_PAIRING, and routes each function to an actionable decision.

Wall classes (from playbook §3):
  VBASE_WALL        — vtable-slot +4 delta via virtual base (3a); DEFER
  BOOL_NEG          — subic/subfe vs extrwi boolean normalization (3b); AT_LIMIT
  SIGNEDNESS        — extsb/extsh in CRT-intrinsic loops (3c); AT_LIMIT
  FPR_SCHED         — FPR scheduling/regalloc/CSE (3d); PERMUTE
  INLINE_POLICY     — bl <named> vs inlined body (3e); INLINE_POLICY route
  UNVERIFIABLE      — bl lbl_/fn_ in logic position (3f); DEFER_DEEP
  NO_ORACLE_LAYOUT  — wildly different offset + inverted constant (3g); DEFER_DEEP
  SIZE_DIVERGENCE   — li r3, 0xNNN feeding operator new (3h); DEFER_DEEP
  FUNCLET_PAIRING   — r31 derived from r12 (funclet frame), offset diff is frame-slot noise (3i); DEFER_DEEP
                       ALSO: a funclet whose differing diff includes a DIVERGENT bl callee (dtk paired
                       two structurally different funclets — mdgrind MISROUTE 1); DEFER_DEEP
  VTABLE_DIVERGENCE — differing lwz slot off a vtable ptr (base loaded from 0x0) feeding mtctr/bctrl
                       (DC3 added/removed virtual slots — mdgrind MISROUTE 3); DEFER_DEEP
  MEMBER_DELTA      — clean UNIFORM SAME-SIGN this-relative offset delta; MEMBER_DELTA_CANDIDATE
  UNKNOWN           — unclassified residue; needs human/agent

Routes:
  PERMUTE               — FPR_SCHED, or an offset-swap / regswap pair (mdgrind MISROUTE 2); hand off to /permute
  DEFER_VBASE           — VBASE_WALL; virtual-base layout artifact, multi-TU. Also a funclet-recovered
                          `this` that is vbase-adjusted (subi rX,rX,<large imm>) before member access (1d).
  AT_LIMIT              — BOOL_NEG or SIGNEDNESS; no known fix
  DEFER_DEEP            — UNVERIFIABLE / NO_ORACLE_LAYOUT / SIZE_DIVERGENCE / FUNCLET_PAIRING / VTABLE_DIVERGENCE
  MEMBER_DELTA_CANDIDATE — clean UNIFORM SAME-SIGN member offset delta; one-line header fix
  INLINE_POLICY         — bl vs inlined body
  UNKNOWN               — unclassified

Usage:
  python3 tools/wall_classify.py                          # process ~/tmp/hasreal_worklist.json
  python3 tools/wall_classify.py --worklist /path/to.json
  python3 tools/wall_classify.py --validate               # FIXTURE suite (hermetic)
  python3 tools/wall_classify.py --validate-live          # same cases vs the LIVE tree
  python3 tools/wall_classify.py --mutation-matrix        # prove the suite discriminates
  python3 tools/wall_classify.py --refresh-fixtures       # re-capture fixtures from live
  python3 tools/wall_classify.py --sym "?Foo@Bar@@..." --unit "default/Foo"  # single fn
"""

import sys
import os
import re
import json
import struct
import subprocess
import argparse
from collections import Counter, defaultdict
from typing import List, Dict, Optional, Set, Tuple, Any

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPORT = os.path.join(ROOT, 'build', '45410914', 'report.json')
CLI = os.path.join(ROOT, 'bin', 'objdiff-cli')
BUILD_OBJ = os.path.join(ROOT, 'build', '45410914', 'obj')
SRC_ROOT = os.path.join(ROOT, 'src')

# ── ASM pattern regexes ────────────────────────────────────────────────────────

# FPR instructions
FPR_OPS = frozenset({
    'fmuls', 'fmadds', 'fmsubs', 'fnmadds', 'fnmsubs',
    'fadds', 'fsubs', 'fdivs', 'fmul', 'fmadd', 'fmsub',
    'fnmadd', 'fnmsub', 'fadd', 'fsub', 'fdiv',
    'fmr', 'fabs', 'fnabs', 'fneg', 'frsp',
    'fctiwz', 'fctidz', 'fcfid',
    'fsqrts', 'frsqrte', 'fres',
    'lfs', 'lfd', 'stfs', 'stfd',
    'lfsx', 'lfdx', 'stfsx', 'stfdx',
    'ps_add', 'ps_sub', 'ps_mul', 'ps_madd', 'ps_msub',
})

# Memory ops
MEM_LOADS_STORES = frozenset({
    'lwz', 'lbz', 'lhz', 'lha', 'lwzu', 'lbzu', 'lfs', 'lfd', 'lwa',
    'stw', 'stb', 'sth', 'stwu', 'stfs', 'stfd', 'lmw', 'stmw',
    'ld', 'std', 'ldu', 'stdu', 'lhau',
})

# BOOL_NEG: target has rlwinm (mask-in-place) + subic + subfe sequence
RLWINM_RE = re.compile(r'^rlwinm\b')
BOOL_NEG_OPS = frozenset({'subic', 'subfe'})

# SIGNEDNESS: sign-extend ops that appear instead of unsigned compare
SIGN_EXT_OPS = frozenset({'extsb', 'extsh', 'extsb.', 'extsh.'})
UNSIGNED_CMP = frozenset({'cmplwi', 'cmplw'})

# VBASE: vtable-slot load pattern, matched against the NORMALIZED (flat) operand
# spelling this file ingests -- `lwz rX, 0xNN, rY` -- where rY was loaded from a
# vtable. Uniform +4 slot delta in vtable calls.
# The comment here used to claim `lwz rX, 0xNN(rY)` while the regex demanded the
# flat form; that was aspirational, never true, and it is what let ruler I zero
# this regex unnoticed. Feed it `_flat_args(side)`, never `side['args']`.
VBASE_VTBL_RE = re.compile(r'^lwz\s+r(\d+),\s*(0x[0-9a-f]+),\s*r(\d+)', re.I)

# Operator new / size divergence
OPERATOR_NEW_RE = re.compile(r'operator\s+new|@@YAPAXI@Z|@@YAPAXI@Z|new\b.*0x[0-9a-f]', re.I)

# lbl_ branch in logic position (UNVERIFIABLE / BL_LBL)
LBL_BRANCH_RE = re.compile(r'^bl\s+lbl_[0-9A-Fa-f]+$')
ANON_BRANCH_RE = re.compile(r'^bl\s+fn_[0-9A-Fa-f]+$')

# COFF virtual-base symbols: ??_8 = vbtable (virtual-base table), MSVC
# NOTE: ??_D = scalar deleting dtor (NOT a vbase indicator), ??_G = vector
# deleting dtor (also not vbase). Only ??_8 reliably signals virtual inheritance.
VBASE_SYM_RE = re.compile(r'^\?\?_8')
BUILD_SRC = os.path.join(ROOT, 'build', '45410914', 'src')


def _tokens(s: str) -> List[str]:
    return [t.strip() for t in (s or '').split(',') if t.strip()]


def _flat_args(side: dict) -> str:
    """Rebuild the pre-fdc5113 flat operand join from `typed_args`.

    objdiff-cli fdc5113 ("ruler I", 2026-08-16) changed the JSON `args` string
    from a comma-join of the COMPARISON arg list to the DISPLAY spelling:
    d-form operands gained parens (`0xNN, rY` -> `0xNN(rY)`), COFF relocations
    gained `@h`/`@l` suffixes, and the trailing NON-DISPLAYED relocation left
    the string (it survives only as the last `typed_args` entry, type Symbol).

    Every detector in this file reads operands through `_tokens`, a bare
    split(','), against the OLD spelling -- so the change silently zeroed
    VBASE_VTBL_RE and every `len(tt) >= 3` gate that depends on it. See
    _normalize_args_inplace for the blast radius and the measured numbers.

    Rendering matches objdiff-core/src/obj/mod.rs' Display impls exactly:
    Signed/BranchDest as signed hex, Unsigned as hex, everything else verbatim.
    """
    ta = side.get('typed_args')
    if ta is None:
        return side.get('args') or ''
    out: List[str] = []
    for a in ta:
        t = a.get('type')
        v = a.get('value')
        if t in ('Signed', 'BranchDest') and isinstance(v, int):
            out.append(('-0x%x' % -v) if v < 0 else '0x%x' % v)
        elif t == 'Unsigned' and isinstance(v, int):
            out.append('0x%x' % v)
        else:
            out.append(str(v))
    return ', '.join(out)


def _normalize_args_inplace(d: dict) -> dict:
    """Rewrite every instruction side's `args` to the flat pre-fdc5113 spelling.

    Done ONCE at ingest rather than at the 44 `args` read sites in this file,
    because three different operand conventions are in flight here (load/store
    `rD, imm, rBase`; `addi rD, rSrc, imm`; 5-operand `rlwinm`) and a per-site
    patch would have to get each one right 44 times.

    What ruler I cost this file, measured on real objdiff-cli 4.2.3 JSON
    (dc3 7-fn audit sample / rb3-xenon [80,100) 60 fns / rb3-xenon [99,100)
    80 fns), shipped spelling vs the rebuilt old one:

        VBASE_VTBL_RE matches      0 vs   218 /  10056 /  4497
        load-store rows readable   0 vs   186 /   9700 /  4708
        FPR offset pairs readable  0 vs     3 /   1773 /   730

    Four detectors died with those gates: _detect_vbase_wall's slot-delta scan,
    _detect_no_oracle_layout, _is_funclet_direct_access (so FUNCLET_PAIRING
    stopped routing DEFER_DEEP -- the mdgrind MISROUTE-1 it was built to stop),
    and _detect_fpr_sched, whose offset compare returned None and let the
    `tt[0] != bt[0]` arm relabel a header-layout bug an FPR register rename and
    route it PERMUTE.

    The original display string is preserved as `args_display`: it is strictly
    more readable and is the better source for the human-facing `evidence`
    strings, which currently read the normalized `args` like everything else.
    """
    for ins in d.get('instructions') or []:
        for k in ('target', 'base'):
            side = ins.get(k)
            if not side:
                continue
            side['args_display'] = side.get('args') or ''
            side['args'] = _flat_args(side)
    return d


def _toi(s: str) -> Optional[int]:
    s = s.strip()
    try:
        return int(s, 16) if s.lower().startswith(('0x', '-0x')) else int(s)
    except Exception:
        return None


# ── objdiff diff extraction ───────────────────────────────────────────────────

# Injection seam for the fixture-driven validation suite (see run_validation).
# When set, `diff_fn` serves this instead of shelling out to objdiff-cli, and
# `unit_vbase_for_class` / `InheritanceDB.has_virtual_base` answer from the
# environment snapshot recorded in the fixture rather than from build/ and src/.
#
# WHY this exists: the old --validate ran the classifier against the LIVE tree,
# so its score tracked how much of the tree had been fixed, not whether the
# classifier still worked.  7 of its 11 targets reached 100% and started
# reporting "no diff instructions"; the suite scored an identical 4/11 on
# broken and repaired code (measured 2026-08-17).  A control has to hold its
# input still.
_DIFF_SOURCE = None       # Optional[Callable[[str, str], Optional[dict]]]
_VBASE_SOURCE = None      # Optional[dict] {'coff': {cls: [sym]}, 'header': {cls: bool}}


def diff_fn(unit: str, sym: str, proj: str = ROOT) -> Optional[dict]:
    """Run objdiff for one symbol; return parsed JSON or None."""
    if _DIFF_SOURCE is not None:
        return _DIFF_SOURCE(unit, sym)
    tmp = f'/tmp/_wc_{os.getpid()}.json'
    args_u = [CLI, 'diff', '-p', proj, '-u', unit, sym,
               '-f', 'json', '-o', tmp, '--include-instructions']
    args_s = [CLI, 'diff', '-p', proj, sym,
               '-f', 'json', '-o', tmp, '--include-instructions']
    for args in (args_u, args_s):
        try:
            r = subprocess.run(args, capture_output=True, text=True, timeout=120)
            if os.path.exists(tmp):
                with open(tmp) as f:
                    d = json.load(f)
                if d.get('instructions'):
                    # Single ingest point for every path in this file
                    # (get_diff_instructions and the three direct callers all
                    # come through here), so normalizing once here covers all
                    # 44 `args` read sites.
                    return _normalize_args_inplace(d)
        except Exception:
            pass
    return None


def get_diff_instructions(unit: str, sym: str, proj: str = ROOT) -> List[dict]:
    """Return only the differing instruction pairs for a function."""
    d = diff_fn(unit, sym, proj)
    if not d:
        return []
    return [ins for ins in d.get('instructions', [])
            if ins.get('match_type') not in ('equal', None)]


# ── COFF vbase symbol detection ────────────────────────────────────────────────

def get_coff_symbols(obj_path: str) -> List[str]:
    """Read COFF symbol table from a .obj file."""
    try:
        with open(obj_path, 'rb') as f:
            data = f.read()
        if len(data) < 20:
            return []
        _machine, _ns, _ts, sym_ptr, num_syms = struct.unpack_from('<HHIII', data, 0)
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


def _find_compiled_obj(unit: str) -> Optional[str]:
    """Find the MSVC-compiled (base) .obj for a unit in build/45410914/src/."""
    base = unit.replace('default/', '', 1)
    # Try direct path under src/
    candidate_direct = os.path.join(BUILD_SRC, base + '.obj')
    if os.path.exists(candidate_direct):
        return candidate_direct
    # Walk to find by basename
    basename = os.path.basename(base) + '.obj'
    for dirpath, _dirs, files in os.walk(BUILD_SRC):
        if basename in files:
            return os.path.join(dirpath, basename)
    return None


# Cache for compiled obj COFF symbols (expensive to re-read)
_coff_cache: Dict[str, List[str]] = {}


def unit_vbase_for_class(unit: str, cls: str) -> Tuple[bool, List[str]]:
    """Return (has_vbase, evidence_syms) for *cls* in the compiled base .obj.

    Uses MSVC-compiled src/ obj which retains full symbol names including ??_8
    vbtable symbols.  Filters by class name so a free function in a unit that
    happens to contain vbase classes is NOT falsely tagged.

    ??_8<ClassName>@@7B... = vbtable for ClassName (MSVC encoding of virtual base table).
    """
    if cls == '':
        return False, []
    if _VBASE_SOURCE is not None:
        syms = list((_VBASE_SOURCE.get('coff') or {}).get(cls) or [])
        return bool(syms), syms[:4]
    obj_path = _find_compiled_obj(unit)
    if not obj_path:
        return False, []
    if obj_path not in _coff_cache:
        _coff_cache[obj_path] = get_coff_symbols(obj_path)
    syms = _coff_cache[obj_path]
    # Filter: ??_8<cls>@@7B... means cls has a vbtable entry
    cls_vbtable = [s for s in syms
                   if VBASE_SYM_RE.match(s) and cls + '@@' in s]
    return (len(cls_vbtable) > 0), cls_vbtable[:4]


# ── Header virtual-base scan ──────────────────────────────────────────────────

class InheritanceDB:
    """Scan headers for virtual base declarations."""

    def __init__(self, src_root: str = SRC_ROOT):
        self._virt_classes: Dict[str, bool] = {}
        self._bases: Dict[str, List[str]] = {}
        self._loaded = False
        self._src_root = src_root

    def _ensure(self):
        if self._loaded:
            return
        self._loaded = True
        self._scan()

    _CLASS_RE = re.compile(
        r'^\s*(?:class|struct)\s+\w+\s*(?::[^{]*?)?\{', re.M)
    _VIRT_BASE_RE = re.compile(r'(?:,|\s|:)\s*(?:public|private|protected)?\s*virtual\s+(\w+)')
    _BASE_RE = re.compile(r'(?:,|\s|:)\s*(?:public|private|protected)\s+(\w+)')
    _CLS_NAME_RE = re.compile(r'(?:class|struct)\s+(\w+)')

    def _scan(self):
        for dirpath, dirnames, filenames in os.walk(self._src_root):
            dirnames[:] = [d for d in dirnames if d not in ('__pycache__', '.git')]
            for fn in filenames:
                if not fn.endswith('.h'):
                    continue
                path = os.path.join(dirpath, fn)
                try:
                    content = open(path).read()
                except Exception:
                    continue
                for m in self._CLASS_RE.finditer(content):
                    line = m.group(0)
                    nm = self._CLS_NAME_RE.search(line)
                    if not nm:
                        continue
                    cls = nm.group(1)
                    # Check for virtual base in this class declaration line
                    if self._VIRT_BASE_RE.search(line):
                        self._virt_classes[cls] = True
                    # Extract all base names
                    for b in self._BASE_RE.findall(line):
                        if b not in ('class', 'struct'):
                            self._bases.setdefault(cls, []).append(b)

    def has_virtual_base(self, cls: str, _visited: Optional[Set[str]] = None,
                         _depth: int = 0) -> bool:
        if _VBASE_SOURCE is not None:
            return bool((_VBASE_SOURCE.get('header') or {}).get(cls, False))
        self._ensure()
        if _visited is None:
            _visited = set()
        if cls in _visited or _depth > 4:
            return False
        _visited.add(cls)
        if self._virt_classes.get(cls):
            return True
        for b in self._bases.get(cls, []):
            if self.has_virtual_base(b, _visited, _depth + 1):
                return True
        return False


_idb: Optional[InheritanceDB] = None


def get_idb() -> InheritanceDB:
    global _idb
    if _idb is None:
        _idb = InheritanceDB()
    return _idb


# ── Symbol demangling helpers ─────────────────────────────────────────────────

CLASS_RE = re.compile(r'^\?[~A-Za-z0-9_]+@([A-Za-z0-9_]+)@')
# MSVC special member names begin with ?? then a code: ??0 ctor, ??1 dtor,
# ??_G vector deleting dtor, ??_D scalar deleting dtor (alias), ??_E vector dtor,
# ??4 operator=, etc.  The class name follows the special-name code.
SPECIAL_CLASS_RE = re.compile(r'^\?\?(?:_[A-Z]|[0-9])([A-Za-z0-9_]+)@')


def demangle_class(sym: str, unit: str = '') -> str:
    """Extract class name from MSVC mangled symbol."""
    m = CLASS_RE.match(sym)
    if m:
        return m.group(1)
    m = SPECIAL_CLASS_RE.match(sym)
    if m:
        return m.group(1)
    # Free function — use unit basename
    base = os.path.basename(unit)
    return base.replace('default/', '').replace('/', '_')


# ── Wall detectors ─────────────────────────────────────────────────────────────

def _is_fpr_heavy(insns: List[dict]) -> Tuple[bool, List[str]]:
    """3d: FPR scheduling/regalloc — lots of fmuls/fmadds with swapped operands
    or reordered loads, or FPR register rename chains."""
    fpr_diffs = 0
    fpr_swap_evidence = []
    struct_off_fpr = 0  # struct offset diffs that are on FPR loads

    for ins in insns:
        t = ins.get('target') or {}
        b = ins.get('base') or {}
        top = t.get('opcode', '')
        bop = b.get('opcode', '')
        ta = (t.get('args') or '').strip()
        ba = (b.get('args') or '').strip()

        # FPR opcode in either side (but same opcode)
        if top in FPR_OPS:
            if top == bop:
                # Same opcode, different operands — FPR regalloc/sched
                fpr_diffs += 1
                if len(fpr_swap_evidence) < 4:
                    fpr_swap_evidence.append(f'{top} {ta} vs {bop} {ba}')
            elif bop in FPR_OPS:
                # Different FPR opcode — also scheduling
                fpr_diffs += 1
                if len(fpr_swap_evidence) < 4:
                    fpr_swap_evidence.append(f'{top} {ta} vs {bop} {ba}')

        # FPR struct offset diffs: lfs/stfs with numeric displacement diffs
        if top in ('lfs', 'lfd', 'stfs', 'stfd') and bop == top and ta != ba:
            tt = _tokens(ta)
            bt = _tokens(ba)
            # lfs frX, disp, rY — compare disp
            if len(tt) >= 2 and len(bt) >= 2:
                td = _toi(tt[1]) if len(tt) > 1 else None
                bd = _toi(bt[1]) if len(bt) > 1 else None
                if td is not None and bd is not None and td != bd:
                    struct_off_fpr += 1
                    if len(fpr_swap_evidence) < 4:
                        fpr_swap_evidence.append(f'{top} {ta} vs {bop} {ba} [lfs-offset]')
                elif tt[0] != bt[0]:
                    # FPR register rename
                    fpr_diffs += 1
                    if len(fpr_swap_evidence) < 4:
                        fpr_swap_evidence.append(f'{top} {ta} vs {bop} {ba} [fpr-rename]')

    # Also count insert/delete of FPR ops
    for ins in insns:
        mt = ins.get('match_type')
        if mt in ('insert', 'delete'):
            t = ins.get('target') or {}
            b = ins.get('base') or {}
            op = (t or b).get('opcode', '')
            if op in FPR_OPS:
                fpr_diffs += 1

    if fpr_diffs >= 2 or struct_off_fpr >= 2:
        return True, fpr_swap_evidence
    return False, []


def _detect_bool_neg(insns: List[dict]) -> Tuple[bool, List[str]]:
    """3b: Boolean negation — rlwinm + subic + subfe (target) vs extrwi (base).
    OR replace/mismatch pairs where one side is rlwinm and the other is extrwi."""
    evidence = []
    # Look for target-side subic or subfe (inserted ops = base side doesn't have them)
    has_rlwinm_mask = False
    has_subic = False
    has_subfe = False
    has_extrwi_base = False

    for ins in insns:
        t = ins.get('target') or {}
        b = ins.get('base') or {}
        top = t.get('opcode', '').strip()
        bop = b.get('opcode', '').strip()
        mt = ins.get('match_type', '')

        if mt == 'delete':
            # Instruction only in target (delete = target has it, base lacks it)
            if top in BOOL_NEG_OPS:
                has_subic = has_subic or (top == 'subic')
                has_subfe = has_subfe or (top == 'subfe')
                evidence.append(f'target-only {top} (delete)')
            if RLWINM_RE.match(top):
                has_rlwinm_mask = True
                evidence.append(f'target rlwinm mask {t.get("args","")}')

        if mt == 'insert':
            # Instruction only in base
            if bop == 'extrwi':
                has_extrwi_base = True
                evidence.append(f'base-only extrwi {b.get("args","")} (insert)')

        if mt in ('replace', 'diff_arg', 'mismatch'):
            if RLWINM_RE.match(top) and bop == 'extrwi':
                has_rlwinm_mask = True
                has_extrwi_base = True
                evidence.append(f'replace: {top} {t.get("args","")} vs extrwi {b.get("args","")}')

    # Classic pattern: rlwinm mask-in-place + subic + subfe on target side, extrwi on base
    if (has_rlwinm_mask or has_subic or has_subfe) and has_extrwi_base:
        return True, evidence[:6]
    # Also flag if we see subic+subfe pair (even without explicit extrwi detected)
    if has_subic and has_subfe:
        return True, evidence[:6]
    return False, []


def _detect_signedness(insns: List[dict]) -> Tuple[bool, List[str]]:
    """3c: char/short signedness in CRT intrinsics.
    Target has cmplwi (unsigned) where base has extsb/extsh (signed), or vice versa.
    Also: target lacks extsb/extsh before a sthu/stb (base has it as insert)."""
    evidence = []
    found = False

    for ins in insns:
        t = ins.get('target') or {}
        b = ins.get('base') or {}
        top = t.get('opcode', '').strip()
        bop = b.get('opcode', '').strip()
        mt = ins.get('match_type', '')

        # insert = base has it, target doesn't (target is newer/retail)
        if mt == 'insert' and bop in SIGN_EXT_OPS:
            evidence.append(f'base-only {bop} {b.get("args","")} (insert)')
            found = True

        # delete = target has it, base doesn't
        if mt == 'delete' and top in SIGN_EXT_OPS:
            evidence.append(f'target-only {top} {t.get("args","")} (delete)')
            found = True

        # replace: extsb/extsh vs cmplwi or vice versa
        if mt in ('replace', 'mismatch'):
            if top in SIGN_EXT_OPS and bop in UNSIGNED_CMP:
                evidence.append(f'replace {top} → {bop} (signedness)')
                found = True
            elif top in UNSIGNED_CMP and bop in SIGN_EXT_OPS:
                evidence.append(f'replace {top} → {bop} (signedness)')
                found = True
            elif top in SIGN_EXT_OPS or bop in SIGN_EXT_OPS:
                evidence.append(f'sign-ext: {top} {t.get("args","")} vs {bop} {b.get("args","")}')
                found = True

    return found, evidence[:6]


def _detect_vbase(insns: List[dict], unit: str, sym: str) -> Tuple[bool, List[str]]:
    """3a: Virtual-base vtable-slot wall.
    Signature: lwz rX, 0xAA(rY) vs 0xBB(rY) with uniform +4 delta on vtable loads,
    where rY was just loaded from a vtable (lwz rY, 0x0(rX)).
    Also: subi r29,r3,N / lwz r11,-N(r3) base-subobject adjustors.
    We also check COFF symbols and header inheritance."""
    evidence = []

    # Determine the class of this symbol (empty = free function)
    cls = demangle_class(sym, unit)
    # Free functions don't have a class name between @ delimiters:
    #   ?Foo@Bar@@...  → class=Bar (method)
    #   ?Foo@@...      → free function (no class between @@)
    # MSVC special members (??0 ctor, ??1 dtor, ??_G vector dtor, ??4 op=, …) ARE
    # class methods even though they start with ??; SPECIAL_CLASS_RE extracts the class.
    free_fn_re = re.compile(r'^\?[^@?]+@@')  # ?name@@ = free; exclude ?? special names
    is_free_fn = bool(free_fn_re.match(sym)) and not SPECIAL_CLASS_RE.match(sym)

    # Check COFF symbols in the compiled base .obj, filtered by class name
    # Skip COFF check for free functions (they won't have ??_8<cls>@@ anyway)
    has_vb_coff = False
    vb_syms: List[str] = []
    if not is_free_fn and cls:
        has_vb_coff, vb_syms = unit_vbase_for_class(unit, cls)
        if has_vb_coff:
            evidence.extend([f'COFF vbase sym: {s}' for s in vb_syms[:3]])

    # Check header inheritance (only for class methods, not free functions)
    has_vb_hdr = False
    if not is_free_fn and cls:
        idb = get_idb()
        has_vb_hdr = idb.has_virtual_base(cls)
        if has_vb_hdr:
            evidence.append(f'Header: {cls} uses virtual inheritance')

    # Look for vtable-slot +4 delta pattern in instructions
    # This works even for free functions if they do virtual dispatch through a param
    slot_deltas = []
    for ins in insns:
        t = ins.get('target') or {}
        b = ins.get('base') or {}
        top = t.get('opcode', '')
        bop = b.get('opcode', '')
        ta = (t.get('args') or '').strip()
        ba = (b.get('args') or '').strip()

        if top == bop and top == 'lwz' and ta != ba:
            tt = _tokens(ta)
            bt = _tokens(ba)
            # Format: rX, 0xNN, rY (objdiff uses comma-separated)
            if len(tt) >= 3 and len(bt) >= 3:
                td = _toi(tt[1])
                bd = _toi(bt[1])
                # vtable loads use r11 as a scratch register for vtable pointer
                # after loading the vtable ptr: lwz r11, 0x0(rX); lwz r11, 0xNN(r11)
                base_r = tt[2]
                if (td is not None and bd is not None and base_r == bt[2]
                        and base_r == 'r11'):
                    delta = td - bd
                    # vtable slot deltas: typically ±4 (one slot)
                    if abs(delta) in (4, 8, 12):
                        slot_deltas.append(delta)
                        if len(evidence) < 6:
                            evidence.append(f'vtable-slot lwz {ta} vs {ba} delta={delta:+d}')

    # Look for subi r29/r28/r31, r3, N (base-subobject / vbase adjustor).
    # The DECISIVE signal for vector-deleting-dtor vbase walls (the mdgrind
    # ??_G<Class> family) is a `subi rX, r3, <large imm>` whose IMMEDIATE DIFFERS
    # between target and base — that diverging immediate IS the compiler-computed
    # vbase/secondary-base offset, which a member edit cannot fix.
    for ins in insns:
        t = ins.get('target') or {}
        b = ins.get('base') or {}
        top = t.get('opcode', '')
        bop = b.get('opcode', '')
        ta = (t.get('args') or '').strip()
        ba = (b.get('args') or '').strip()
        if top in ('subi', 'addi') and ta and not is_free_fn:
            tt = _tokens(ta)
            bt = _tokens(ba)
            if len(tt) >= 2 and tt[1] == 'r3':
                # Diverging large immediate off this = vbase adjustor (strong signal).
                if (top == bop and len(tt) >= 3 and len(bt) >= 3 and bt[1] == 'r3'):
                    ti = _toi(tt[2])
                    bi = _toi(bt[2])
                    if (ti is not None and bi is not None and ti != bi
                            and min(abs(ti), abs(bi)) >= _VBASE_ADJUST_MIN_IMM):
                        evidence.append(
                            f'vbase adjustor: {top} {ta} vs {ba} '
                            f'(diverging secondary-base offset)')
                        has_vb_coff = True
                        break
                # Otherwise a plain subobject adjust is a weaker hint (kept for the
                # legacy slot-pattern path); only escalate when corroborated below.
                evidence.append(f'adjustor: {top} {ta} (base-subobject adjust)')
                has_vb_coff = True  # treat adjustor as strong signal
                break

    # Verdict: VBASE if COFF evidence OR header evidence, or strong slot-delta pattern
    dominant_delta = Counter(slot_deltas).most_common(1)
    has_slot_pattern = (len(slot_deltas) >= 1 and dominant_delta and
                        abs(dominant_delta[0][0]) == 4)

    if has_vb_coff or has_vb_hdr or has_slot_pattern:
        return True, evidence[:8]
    return False, []


def _detect_inline_policy(insns: List[dict]) -> Tuple[bool, List[str]]:
    """3e: Inline-policy mismatch — target has inlined body where base has bl to named fn.
    Signature: replace pairs where base is bl <named> and target has load/store sequence."""
    evidence = []
    # Look for replace/delete pairs where base = bl <named function>
    inline_replaces = 0
    for ins in insns:
        t = ins.get('target') or {}
        b = ins.get('base') or {}
        top = t.get('opcode', '').strip()
        bop = b.get('opcode', '').strip()
        ta = (t.get('args') or '').strip()
        ba = (b.get('args') or '').strip()
        mt = ins.get('match_type', '')

        def _is_named_callee(sym_arg: str) -> bool:
            """Return True if this bl target is a named, non-anonymous callee."""
            if not sym_arg:
                return False
            if sym_arg.startswith('lbl_'):  # anonymous label (UNVERIFIABLE territory)
                return False
            if sym_arg.startswith('fn_'):   # anonymous function
                return False
            return True

        if mt == 'replace':
            # Base has a named bl call; target has inlined body (non-bl)
            if bop == 'bl' and top != 'bl':
                if _is_named_callee(ba):
                    inline_replaces += 1
                    evidence.append(f'inlined: T={top} {ta} vs B=bl {ba}')
            # Target has named bl; base has inlined body (not a bl)
            elif top == 'bl' and bop != 'bl':
                if _is_named_callee(ta):
                    inline_replaces += 1
                    evidence.append(f'not-inlined: T=bl {ta} vs B={bop} {ba}')

        # "insert" = target has bl (and base doesn't) — but we use 'delete' for target-only
        # In objdiff: 'delete' = target has it but base lacks it
        if mt == 'delete' and top == 'bl':
            if _is_named_callee(ta):
                inline_replaces += 1
                evidence.append(f'target calls bl {ta} (target-only)')

        # "insert" = base has it, target doesn't
        if mt == 'insert' and bop == 'bl':
            if _is_named_callee(ba):
                inline_replaces += 1
                evidence.append(f'base calls bl {ba} (base-only)')

    if inline_replaces >= 1:
        return True, evidence[:6]
    return False, []


_LBL_ADDR_RE = re.compile(r'^lbl_[0-9A-Fa-f]+$')


def _detect_unverifiable(insns: List[dict]) -> Tuple[bool, List[str]]:
    """3f: bl lbl_<addr> in a logic position — target calls an unresolvable label
    where our base has a literal/constant or a named call.

    Signature:
      replace: target=bl lbl_<addr>  base=li r3,N  (or other literal/named)
      OR: replace: target=bl lbl_<addr>  base=bl <named>
    """
    evidence = []
    found = False

    for ins in insns:
        t = ins.get('target') or {}
        b = ins.get('base') or {}
        top = t.get('opcode', '').strip()
        bop = b.get('opcode', '').strip()
        ta = (t.get('args') or '').strip()
        ba = (b.get('args') or '').strip()
        mt = ins.get('match_type', '')

        if mt == 'replace':
            # Target branches to an anonymous label; base has different logic
            if top == 'bl' and _LBL_ADDR_RE.match(ta):
                evidence.append(f'target bl {ta} vs base {bop} {ba}')
                found = True
            # Base branches to anonymous label; target has different logic
            elif bop == 'bl' and _LBL_ADDR_RE.match(ba) and top != 'bl':
                evidence.append(f'base bl {ba} vs target {top} {ta}')
                found = True

    return found, evidence[:6]


def _detect_no_oracle_layout(insns: List[dict]) -> Tuple[bool, List[str]]:
    """3g: RB3-vs-DC3 layout divergence with no oracle.
    Signature: stb/stw with DIFFERENT VALUE AND different offset from this."""
    evidence = []
    # Look for cases where both offset AND value differ (not just offset)
    # In STRUCT_OFF we expect same opcode, same reg usage, different offset
    # NO_ORACLE_LAYOUT: also different immediate value being stored (indicates
    # inverted polarity or different semantics, not just shifted field)
    mixed_divergence = 0

    for ins in insns:
        t = ins.get('target') or {}
        b = ins.get('base') or {}
        top = t.get('opcode', '').strip()
        bop = b.get('opcode', '').strip()
        ta = (t.get('args') or '').strip()
        ba = (b.get('args') or '').strip()
        mt = ins.get('match_type', '')

        if mt == 'replace' and top in ('stb', 'stw', 'stfs') and bop in ('stb', 'stw', 'stfs'):
            # Both are stores — check if both offset and stored value differ
            tt = _tokens(ta)
            bt = _tokens(ba)
            if len(tt) >= 2 and len(bt) >= 2:
                t_val = tt[0]  # register or immediate being stored
                b_val = bt[0]
                t_off = _toi(tt[1]) if len(tt) > 1 else None
                b_off = _toi(bt[1]) if len(bt) > 1 else None
                if t_val != b_val and t_off is not None and b_off is not None and t_off != b_off:
                    mixed_divergence += 1
                    evidence.append(f'divergent store: {top} {ta} vs {bop} {ba} (val+off both differ)')

    if mixed_divergence >= 1:
        return True, evidence[:4]
    return False, []


def _detect_size_divergence(insns: List[dict]) -> Tuple[bool, List[str]]:
    """3h: Class size divergence — li r3, 0xNNN feeding operator new.
    Signature: li r3, X vs li r3, Y where X != Y and the next instruction is bl operator new."""
    evidence = []
    found = False
    insn_list = [ins for ins in insns]  # all instructions, not just diffs

    for i, ins in enumerate(insn_list):
        t = ins.get('target') or {}
        b = ins.get('base') or {}
        top = t.get('opcode', '').strip()
        bop = b.get('opcode', '').strip()
        ta = (t.get('args') or '').strip()
        ba = (b.get('args') or '').strip()
        mt = ins.get('match_type', '')

        if mt == 'diff_arg' and top == 'li' and bop == 'li':
            tt = _tokens(ta)
            bt = _tokens(ba)
            # li rX, 0xNNN — check if r3 (first arg to function call)
            if len(tt) >= 2 and len(bt) >= 2:
                if tt[0] == 'r3' and bt[0] == 'r3':
                    tv = _toi(tt[1])
                    bv = _toi(bt[1])
                    if tv is not None and bv is not None and tv != bv:
                        # Check next diff instruction for operator new bl
                        for j in range(i + 1, min(i + 5, len(insn_list))):
                            ni = insn_list[j]
                            nt = ni.get('target') or {}
                            nb = ni.get('base') or {}
                            nta = (nt.get('args') or '').strip()
                            nba = (nb.get('args') or '').strip()
                            # Check for operator new or fn_ call (unidentified alloc)
                            if nt.get('opcode') == 'bl' or nb.get('opcode') == 'bl':
                                if ('new' in nta.lower() or 'new' in nba.lower() or
                                        'YAPAXI' in nta or 'YAPAXI' in nba or
                                        'fn_' in nta or 'fn_' in nba):
                                    found = True
                                    evidence.append(
                                        f'new-size: li r3,0x{tv:x} vs li r3,0x{bv:x} '
                                        f'before bl {nta or nba}')
                                    break
                        # Also flag even without next-insn check if large size values
                        if tv is not None and bv is not None and min(tv, bv) > 0x40:
                            if not evidence:
                                evidence.append(f'size-delta: li r3,{ta} vs li r3,{ba}')
                            found = True

    return found, evidence[:4]


def _get_funclet_frame_reg(all_insns: List[dict]) -> Optional[str]:
    """Return the funclet frame register if this function has a funclet prologue.

    A funclet prologue sets up a callee-save register from r12 (the funclet frame
    pointer passed by the exception runtime), using: subi rX, r12, <imm>
    This is distinct from r3-derived subobject adjustors like subi r31, r3, <imm>.

    Returns the register name (e.g. 'r31') if found, else None.
    """
    # Check the first few instructions for the funclet pattern
    for ins in all_insns[:6]:
        t = ins.get('target') or {}
        top = t.get('opcode', '').strip()
        ta = (t.get('args') or '').strip()
        if top in ('subi', 'addi'):
            tt = _tokens(ta)
            # subi rX, r12, imm  (3 tokens; src must be r12)
            if len(tt) >= 3 and tt[1] == 'r12':
                return tt[0]  # e.g. 'r31'
    return None


def _is_funclet_direct_access(all_insns: List[dict], funclet_reg: str,
                               diff_insns: List[dict]) -> bool:
    """Return True if the differing memory accesses are DIRECTLY on funclet_reg.

    "Directly" means the base register of the differing lwz/stw/addi IS funclet_reg,
    rather than being a register that was loaded FROM funclet_reg.

    The CameraShot pattern is NOT direct: funclet dtor accesses look like:
        lwz r11, FIXED_OFFSET, r31     (load a pointer from frame slot — same both sides)
        subi r11, r11, CONST           (adjust to subobject — same both sides)
        addi r3, r11, MEMBER_OFFSET    (offset HERE differs — on r11, not r31)
    So r11 (the register loaded from r31) carries the real object pointer; the diff
    is on r11, not r31. That IS a real member delta.

    The Rnd funclet FP pattern IS direct:
        lwz r3, 0x54, r31  vs  lwz r3, 0x50, r31
    The differing offset is directly on r31 (frame slot index varies between
    structurally different functions that happen to share prologue bytes).
    """
    # Build the set of registers that were loaded FROM funclet_reg via lwz/addi/subi
    # (these carry dereferenced object pointers — their offsets are real member accesses)
    loaded_from_funclet: Set[str] = set()
    for ins in all_insns:
        t = ins.get('target') or {}
        top = t.get('opcode', '').strip()
        ta = (t.get('args') or '').strip()
        if top == 'lwz':
            tt = _tokens(ta)
            # lwz dst, offset, base  — if base is funclet_reg, dst = loaded from frame
            if len(tt) >= 3 and tt[2] == funclet_reg:
                loaded_from_funclet.add(tt[0])
        elif top in ('subi', 'addi'):
            tt = _tokens(ta)
            # subi/addi dst, src, imm — if src is in loaded_from_funclet, dst inherits
            if len(tt) >= 3 and tt[1] in loaded_from_funclet:
                loaded_from_funclet.add(tt[0])
            # Also: subi/addi dst, funclet_reg, imm (direct adjust from frame reg)
            # Do NOT add dst to loaded_from_funclet here — dst is still "from frame"
            # in the sense it's a frame-relative pointer, not a dereferenced heap obj

    # Now check: do any differing accesses have base_reg == funclet_reg DIRECTLY?
    has_direct = False
    has_indirect = False
    for ins in diff_insns:
        t = ins.get('target') or {}
        b = ins.get('base') or {}
        top = t.get('opcode', '').strip()
        bop = b.get('opcode', '').strip()
        ta = (t.get('args') or '').strip()

        if top in MEM_LOADS_STORES and bop == top:
            tt = _tokens(ta)
            if len(tt) >= 3:
                base_r = tt[2]
                if base_r == funclet_reg:
                    has_direct = True
                elif base_r in loaded_from_funclet:
                    has_indirect = True

        if top in ('addi', 'subi', 'addic') and bop == top:
            tt = _tokens(ta)
            if len(tt) >= 3:
                src_r = tt[1]
                if src_r == funclet_reg:
                    has_direct = True
                elif src_r in loaded_from_funclet:
                    has_indirect = True

    # FUNCLET_PAIRING: all differing accesses are directly on funclet_reg,
    # with no indirect (loaded-from-funclet) accesses that would indicate real obj access.
    # If there are ANY indirect accesses, do NOT flag as funclet pairing — those are real.
    if has_direct and not has_indirect:
        return True
    return False


def _detect_funclet_pairing(all_insns: List[dict], diff_insns: List[dict],
                             unit: str, sym: str) -> Tuple[bool, List[str]]:
    """3i: Funclet address-pairing artifact.

    Detects when dtk paired our compiled function against a structurally different
    retail funclet. Signature:
    1. Prologue contains `subi rX, r12, <imm>` (funclet frame setup — r12 is the
       exception runtime frame pointer, NOT this/r3).
    2. The differing memory accesses are DIRECTLY on that funclet frame register
       (i.e., frame-slot index noise), NOT on a register loaded from the frame reg
       (which would be a real heap-object member access).

    See docs/decomp/playbooks/hasreal-grind.md §3i and §4 "Gate zero".
    """
    funclet_reg = _get_funclet_frame_reg(all_insns)
    if funclet_reg is None:
        return False, []

    is_direct = _is_funclet_direct_access(all_insns, funclet_reg, diff_insns)
    if not is_direct:
        return False, []

    evidence = [
        f'funclet prologue: subi {funclet_reg}, r12, <imm>',
        f'diff accesses are directly on {funclet_reg} (frame-slot index, not member offset)',
        'different retail function paired by coincidental prologue bytes',
    ]
    return True, evidence


# Branch opcodes whose operand is a callee symbol (call / tail-call).
_CALL_OPS = frozenset({'bl', 'b', 'bla', 'ba'})


def _detect_divergent_call(diff_insns: List[dict],
                           funclet_mode: bool = False) -> Tuple[bool, List[str]]:
    """Detect a diverging CALL TARGET: a bl/b whose callee SYMBOL differs between
    target and base (objdiff "Function Call Diff" — Target only / Base only).

    This is the decisive signal that dtk address-paired two STRUCTURALLY DIFFERENT
    functions (mdgrind MISROUTE 1): the bodies call different methods, so any
    member-offset delta extracted from the pairing is meaningless.

    We only count it as a divergence when BOTH sides emit a call opcode (so the
    pairing is body-vs-body, not an inline-policy flip where one side has the bl
    and the other inlines it — that is _detect_inline_policy's job).

    Default (non-funclet) mode requires BOTH callee symbols to be NAMED (non-anon:
    not fn_/lbl_) — a thunk/anon-ns naming difference is NOT a structural divergence.

    Funclet mode (the size-44 mis-paired cleanup funclets) is more lenient: if AT
    LEAST ONE side is a NAMED symbol and the two callees DIFFER (the other side may
    be an unresolved `fn_`/`lbl_`), that is still a structural divergence — a genuine
    member-delta funclet (CameraShot) keeps IDENTICAL callees on both sides, so a
    differing named-vs-anon pair only happens when the two funclets call different
    destructors and dtk merely failed to resolve one side's name.

    Returns (has_divergent_call, evidence).
    """
    evidence: List[str] = []
    found = False

    def _callee(args: str) -> str:
        """Extract the callee symbol from a bl/b args string ('?Foo@Bar@@...')."""
        toks = _tokens(args)
        return toks[0] if toks else ''

    def _is_named(callee: str) -> bool:
        if not callee:
            return False
        if callee.startswith(('fn_', 'lbl_')):
            return False
        # MSVC mangled name or a plain C symbol — treat as named.
        return callee.startswith('?') or callee[0].isalpha() or callee.startswith('_')

    for ins in diff_insns:
        t = ins.get('target') or {}
        b = ins.get('base') or {}
        top = t.get('opcode', '').strip()
        bop = b.get('opcode', '').strip()
        if top in _CALL_OPS and bop in _CALL_OPS:
            tc = _callee((t.get('args') or '').strip())
            bc = _callee((b.get('args') or '').strip())
            if not (tc and bc and tc != bc):
                continue
            if funclet_mode:
                # at least one side named, callees differ ⇒ structural divergence
                if _is_named(tc) or _is_named(bc):
                    evidence.append(f'divergent call (funclet): {top} {tc} vs {bop} {bc}')
                    found = True
            else:
                if _is_named(tc) and _is_named(bc):
                    evidence.append(f'divergent call: {top} {tc} vs {bop} {bc}')
                    found = True

    return found, evidence[:4]


def _detect_offset_swap(diff_insns: List[dict]) -> Tuple[bool, List[str]]:
    """Detect a matched (+N, -N) offset-swap pair or a register swap — both are
    PERMUTE-class (instruction-scheduling / regalloc), NOT a member delta.

    mdgrind MISROUTE 2: a real member delta is a UNIFORM SAME-SIGN this-relative
    shift across all differing accesses.  An offset SWAP is two (or more) accesses
    whose offset deltas have EQUAL MAGNITUDE but OPPOSITE SIGN (e.g. -8 and +8, or
    two loads that traded their displacements) — the net same-sign shift is zero,
    so it is a reorder, not a layout difference.

    Signature A (offset swap): among same-opcode mem/addi diffs, the set of deltas
      (target_off - base_off) contains both +N and -N for the same magnitude N,
      with no dominant uniform sign.
    Signature B (regswap): same-opcode diff where the only difference is the
      register operands being swapped (e.g. `fmuls f0,f1,f2` vs `fmuls f0,f2,f1`).

    Returns (is_swap, evidence).
    """
    evidence: List[str] = []

    # ── Signature A: mirrored offset deltas ────────────────────────────────────
    deltas: List[int] = []
    for ins in diff_insns:
        t = ins.get('target') or {}
        b = ins.get('base') or {}
        top = t.get('opcode', '').strip()
        bop = b.get('opcode', '').strip()
        if top != bop:
            continue
        ta = (t.get('args') or '').strip()
        ba = (b.get('args') or '').strip()
        if top in MEM_LOADS_STORES:
            tt = _tokens(ta)
            bt = _tokens(ba)
            if len(tt) >= 2 and len(bt) >= 2:
                to = _toi(tt[1])
                bo = _toi(bt[1])
                if to is not None and bo is not None and to != bo:
                    deltas.append(to - bo)
        elif top in ('addi', 'subi', 'addic'):
            tt = _tokens(ta)
            bt = _tokens(ba)
            if len(tt) >= 3 and len(bt) >= 3:
                ti = _toi(tt[2])
                bi = _toi(bt[2])
                if ti is not None and bi is not None and ti != bi:
                    deltas.append(ti - bi)

    if len(deltas) >= 2:
        mags = Counter(abs(d) for d in deltas)
        for mag, _cnt in mags.items():
            if mag == 0:
                continue
            has_pos = any(d == mag for d in deltas)
            has_neg = any(d == -mag for d in deltas)
            if has_pos and has_neg:
                evidence.append(
                    f'offset-swap: mirrored +/-0x{mag:x} deltas (net same-sign shift = 0)')
                return True, evidence[:4]

    # ── Signature B: register swap on a same-opcode diff ────────────────────────
    for ins in diff_insns:
        t = ins.get('target') or {}
        b = ins.get('base') or {}
        top = t.get('opcode', '').strip()
        bop = b.get('opcode', '').strip()
        if not top or top != bop:
            continue
        tt = _tokens((t.get('args') or '').strip())
        bt = _tokens((b.get('args') or '').strip())
        if len(tt) >= 2 and len(tt) == len(bt) and tt != bt:
            # Same multiset of operands, different order ⇒ a swap (no immediates moved)
            if sorted(tt) == sorted(bt) and any(x != y for x, y in zip(tt, bt)):
                # Only count register operands (no numeric immediate moved)
                if all(_toi(x) is None for x in tt):
                    evidence.append(f'regswap: {top} {", ".join(tt)} vs {", ".join(bt)}')
                    return True, evidence[:4]

    return False, []


# A "large" subtract immediate that signals a virtual-base / secondary-base adjust
# (e.g. subi r29, r3, 0x1dc).  Member-field offsets are small; vbase adjustors are
# large class-relative deltas.
_VBASE_ADJUST_MIN_IMM = 0x80


def _detect_vbase_funclet_adjust(all_insns: List[dict], diff_insns: List[dict],
                                 funclet_reg: Optional[str]) -> Tuple[bool, List[str]]:
    """1d (CameraShot Ghidra finding): a funclet-recovered `this` that is then
    VBASE-ADJUSTED (`subi rX, rX, <large imm>`) before the differing member access
    is a VBASE wall, NOT a member delta.

    The frame-recovered object pointer (loaded from the funclet frame slot) does not
    clear gate-zero on its own — the indirect load alone is what made the CameraShot
    family look like a real member delta.  But when that pointer is run through a
    large secondary-base/vbase adjustor (`subi r11, r11, 0x1a0`) before the member
    addi/load, the differing offset is on a vbase-adjusted subobject pointer, whose
    layout the compiler determines — not an addable data member.

    Returns (is_vbase_adjust, evidence).
    """
    if funclet_reg is None:
        return False, []

    # Registers carrying a pointer LOADED FROM the funclet frame slot.
    loaded_from_funclet: Set[str] = set()
    for ins in all_insns:
        t = ins.get('target') or {}
        top = t.get('opcode', '').strip()
        tt = _tokens((t.get('args') or '').strip())
        if top == 'lwz' and len(tt) >= 3 and tt[2] == funclet_reg:
            loaded_from_funclet.add(tt[0])

    # Registers that have been vbase-adjusted via a large subi off a frame-loaded ptr.
    vbase_adjusted: Set[str] = set()
    for ins in all_insns:
        t = ins.get('target') or {}
        top = t.get('opcode', '').strip()
        tt = _tokens((t.get('args') or '').strip())
        if top in ('subi', 'addi') and len(tt) >= 3:
            src = tt[1]
            imm = _toi(tt[2])
            if (src in loaded_from_funclet and imm is not None
                    and abs(imm) >= _VBASE_ADJUST_MIN_IMM):
                vbase_adjusted.add(tt[0])

    if not vbase_adjusted:
        return False, []

    # Does any DIFFERING access land on a vbase-adjusted register?
    for ins in diff_insns:
        t = ins.get('target') or {}
        b = ins.get('base') or {}
        top = t.get('opcode', '').strip()
        bop = b.get('opcode', '').strip()
        if top != bop:
            continue
        tt = _tokens((t.get('args') or '').strip())
        if top in MEM_LOADS_STORES and len(tt) >= 3 and tt[2] in vbase_adjusted:
            return True, [
                f'funclet-recovered this vbase-adjusted (subi {tt[2]}, ..., >=0x{_VBASE_ADJUST_MIN_IMM:x}) '
                f'before member access {top} {", ".join(tt)}',
                'vbase/secondary-base subobject layout is compiler-determined, not member-addable',
            ]
        if top in ('addi', 'subi') and len(tt) >= 3 and tt[1] in vbase_adjusted:
            return True, [
                f'funclet-recovered this vbase-adjusted before member addi {top} {", ".join(tt)}',
                'vbase/secondary-base subobject layout is compiler-determined, not member-addable',
            ]

    return False, []


def _detect_vtable_divergence(all_insns: List[dict],
                              diff_insns: List[dict]) -> Tuple[bool, List[str]]:
    """1c (mdgrind MISROUTE 3): a differing load whose base register was loaded from
    OFFSET 0x0 (the vtable pointer) AND immediately feeds mtctr/bctrl is a VTABLE
    SLOT divergence (DC3 added/removed virtual slots), NOT a data-member delta.

    Signature:
        lwz rV, 0x0, rObj        ; load vtable ptr (offset 0)
        lwz rV, 0xNN, rV         ; <-- DIFFERS (vtable slot index, e.g. +0x20 = 8 slots)
        mtctr rV
        bctrl
    The delta is often uniform (DC3 inserted N virtual functions before this slot).
    Fixable only via a vtable-layout reconstruction pass, so route DEFER_DEEP.

    Returns (is_vtable_divergence, evidence).
    """
    # Index the full stream so we can look at the surrounding instructions.
    n = len(all_insns)

    def _reg_loaded_from_zero(idx: int, reg: str) -> bool:
        """True if `reg` was last written by `lwz reg, 0x0, rX` (a vtable-ptr load).

        Back-scan from idx-1 to the MOST RECENT write of `reg`; if that write is a
        load from offset 0x0, `reg` holds a vtable pointer.  This handles the common
        case where the vtable ptr is loaded into a callee-save reg (e.g. r9) many
        instructions before the slot load.
        """
        for j in range(idx - 1, -1, -1):
            x = all_insns[j].get('target') or {}
            op = x.get('opcode', '').strip()
            xt = _tokens((x.get('args') or '').strip())
            if not xt:
                continue
            # Does this instruction WRITE reg? (dest is first operand for our ops)
            if xt[0] == reg and op in ('lwz', 'lbz', 'lhz', 'addi', 'subi', 'mr',
                                       'li', 'lis', 'add', 'or', 'lwzx'):
                if op == 'lwz' and len(xt) >= 3 and _toi(xt[1]) == 0:
                    return True
                # reg was last written by something other than a 0x0 load
                return False
        return False

    def _feeds_ctr(idx: int, reg: str) -> bool:
        """True if `reg` feeds mtctr;bctrl within a few insns after idx."""
        for j in range(idx + 1, min(n, idx + 6)):
            x = all_insns[j].get('target') or {}
            op = x.get('opcode', '').strip()
            if op == 'mtctr':
                xt = _tokens((x.get('args') or '').strip())
                if xt and xt[0] == reg:
                    return True
            if op in ('bctrl', 'bctr'):
                return True
        return False

    evidence: List[str] = []
    # Map diff instruction objects to their index in all_insns (by identity).
    pos = {id(ins): i for i, ins in enumerate(all_insns)}

    for ins in diff_insns:
        t = ins.get('target') or {}
        b = ins.get('base') or {}
        top = t.get('opcode', '').strip()
        bop = b.get('opcode', '').strip()
        if top != 'lwz' or bop != 'lwz':
            continue
        tt = _tokens((t.get('args') or '').strip())
        bt = _tokens((b.get('args') or '').strip())
        if len(tt) < 3 or len(bt) < 3:
            continue
        dst, base_r = tt[0], tt[2]
        to = _toi(tt[1])
        bo = _toi(bt[1])
        if to is None or bo is None or to == bo:
            continue
        idx = pos.get(id(ins))
        if idx is None:
            continue
        # base register (rV) loaded from offset 0x0 just before, and result feeds ctr
        if _reg_loaded_from_zero(idx, base_r) and _feeds_ctr(idx, dst):
            evidence.append(
                f'vtable-slot lwz {", ".join(tt)} vs {", ".join(bt)} '
                f'(delta={to - bo:+d}); base {base_r} from 0x0, feeds mtctr/bctrl')

    if evidence:
        return True, evidence[:4]
    return False, []


def _detect_member_delta(insns: List[dict], unit: str, sym: str) -> Tuple[bool, List[str], int, int]:
    """MEMBER_DELTA: clean uniform this-relative offset delta.
    Returns (is_member_delta, evidence, dominant_delta, threshold).

    Uses the same this-tracing logic as member_delta_finder.py:
    - r3 is `this` on entry; mr propagates; bl kills r3..r12
    - look for struct-offset diffs that are uniform across the function
    - require the delta to be non-zero, consistent, and THIS-relative (not r1/funclet-frame)
    """
    # Collect all structural offset diffs
    this_regs: Set[str] = {'r3'}
    param_regs: Set[str] = {'r4', 'r5', 'r6', 'r7', 'r8', 'r9', 'r10'}
    frame_regs: Set[str] = {'r1'}

    # First pass: track this-registers through the full instruction stream
    # We need all instructions, not just diffs
    d = diff_fn(unit, sym)
    if not d:
        return False, [], 0, 0
    all_insns = d.get('instructions', [])

    # Gate: if this function has a funclet prologue (subi rX, r12, imm) AND the
    # differing accesses are directly on that frame register, bail out — that's
    # FUNCLET_PAIRING noise, not a member delta.  The caller (_classify_fn) runs
    # _detect_funclet_pairing first, but this gate ensures the this-tracer also
    # rejects those frame registers so they don't accidentally land in struct_accesses.
    funclet_reg = _get_funclet_frame_reg(all_insns)
    if funclet_reg is not None:
        # Add the funclet frame register to frame_regs so it's excluded from
        # this-tracking. If the diff accesses are on a register LOADED from
        # funclet_reg (i.e., a real object pointer stored in the frame), those
        # registers are NOT in frame_regs and will be tracked normally.
        frame_regs.add(funclet_reg)

    # Collect (target_offset, base_offset, base_reg, access_type) tuples
    struct_accesses = []  # (t_off, b_off, base_reg, op)

    # Reset this tracking (frame_regs already has funclet reg if applicable)
    this_regs = {'r3'}
    param_regs = {'r4', 'r5', 'r6', 'r7', 'r8', 'r9', 'r10'}

    for ins in all_insns:
        t = ins.get('target') or {}
        b = ins.get('base') or {}
        top = t.get('opcode', '').strip()
        bop = b.get('opcode', '').strip()
        ta = (t.get('args') or '').strip()
        ba = (b.get('args') or '').strip()
        mt = ins.get('match_type', '')

        # Update this-tracking from target side
        if top in ('bl', 'bla', 'bctrl', 'blrl'):
            # Call: clobbers r3..r12
            for rg in list(this_regs | param_regs):
                n = int(rg[1:]) if rg[1:].isdigit() else -1
                if rg[0] == 'r' and 3 <= n <= 12:
                    this_regs.discard(rg)
                    param_regs.discard(rg)
        elif top == 'mr':
            tt = _tokens(ta)
            if len(tt) >= 2:
                dst, src = tt[0], tt[1]
                if src in this_regs:
                    this_regs.add(dst)
                elif dst in this_regs and src not in this_regs:
                    this_regs.discard(dst)

        # Process differing mem/addi instructions
        if mt not in ('diff_arg', 'replace', 'mismatch'):
            continue

        if top in MEM_LOADS_STORES and bop in MEM_LOADS_STORES and top == bop:
            tt = _tokens(ta)
            bt = _tokens(ba)
            # Format: reg, offset, base_reg  (3 tokens) or reg, offset (2 tokens)
            if len(tt) >= 2 and len(bt) >= 2:
                t_off = _toi(tt[1]) if len(tt) > 1 else None
                b_off = _toi(bt[1]) if len(bt) > 1 else None
                base_r = tt[2] if len(tt) > 2 else ''
                if (t_off is not None and b_off is not None and t_off != b_off and
                        base_r not in frame_regs and base_r in this_regs):
                    struct_accesses.append((t_off, b_off, base_r, top))

        if top in ('addi', 'addic', 'subi') and bop == top:
            tt = _tokens(ta)
            bt = _tokens(ba)
            # dst, src, imm
            if len(tt) >= 3 and len(bt) >= 3:
                src_r = tt[1]
                t_imm = _toi(tt[2])
                b_imm = _toi(bt[2])
                if (src_r in this_regs and t_imm is not None and b_imm is not None
                        and t_imm != b_imm and src_r not in frame_regs):
                    struct_accesses.append((t_imm, b_imm, src_r, top))

    if not struct_accesses:
        return False, [], 0, 0

    # Compute deltas: delta = target_offset - base_offset
    deltas = [t_off - b_off for t_off, b_off, _, _ in struct_accesses]
    if not deltas:
        return False, [], 0, 0

    c = Counter(deltas)
    dominant_delta, dominant_count = c.most_common(1)[0]
    consistency = dominant_count / len(deltas)

    if dominant_count < 1 or abs(dominant_delta) == 0:
        return False, [], 0, 0

    # Reject MIRRORED (+N, -N) delta sets — that is an OFFSET-SWAP (two accesses that
    # traded displacements), a PERMUTE-class reorder, NOT a uniform member shift.
    # A real member delta is UNIFORM SAME-SIGN (mdgrind MISROUTE 2).
    for d in deltas:
        if d != 0 and -d in c and d != -d:
            return False, [], 0, 0

    # Require reasonable consistency (≥50% or all single)
    if consistency < 0.5 and len(deltas) > 2:
        return False, [], 0, 0

    # Threshold = minimum target offset among shifted accesses
    shifted = [(t_off, b_off) for t_off, b_off, _, _ in struct_accesses
               if t_off - b_off == dominant_delta]
    threshold = min(t_off for t_off, _ in shifted) if shifted else 0

    evidence = [
        f'uniform delta={dominant_delta:+d} (0x{abs(dominant_delta):x})',
        f'threshold ~0x{threshold:x}',
        f'consistency {consistency:.1%} ({dominant_count}/{len(deltas)} accesses)',
    ]
    if len(deltas) > 1:
        evidence.append(f'all deltas: {sorted(Counter(deltas).items())}')

    return True, evidence, dominant_delta, threshold


# Hard blockers that must suppress the MEMBER_DELTA struct-offset fallback
# (these are routed elsewhere; the fallback would otherwise re-tag them as
# member deltas).  Module scope since 2026-08-17 so the fallback can live in its
# own function.
_FALLBACK_BLOCKERS = {'VBASE_WALL', 'BOOL_NEG', 'SIGNEDNESS', 'FPR_SCHED',
                      'SIZE_DIVERGENCE', 'NO_ORACLE_LAYOUT', 'UNVERIFIABLE',
                      'FUNCLET_PAIRING', 'VTABLE_DIVERGENCE', 'OFFSET_SWAP'}


def _detect_member_delta_fallback(
        diff_insns: List[dict], classes: List[str],
        funclet_reg: Optional[str],
        has_divergent_call: bool) -> Tuple[bool, List[str], int, int]:
    """STRUCT_OFF fallback for MEMBER_DELTA — the SECOND, independent reading.

    If struct-offset diffs exist and the only OTHER classes are ANON_FN or
    NAMED_MISMATCH (not real blockers for layout), classify as MEMBER_DELTA.
    Catches cases where this-tracing fails for legitimate reasons.

    GATE: skip if FUNCLET_PAIRING was already detected — this path is exactly
    what produced false positives for the Rnd/MoveMgr/Archive/Overlay funclet
    clusters (direct-r31 frame slot diffs look like struct offsets here but are
    frame-slot index artifacts, not member deltas).

    Extracted from classify_fn 2026-08-17 so it can be mutated independently of
    _detect_member_delta.  It turns out the two readings are REDUNDANT on every
    clean live case: disabling either one alone leaves the verdict unchanged,
    which is why --mutation-matrix reports both `member-delta` and
    `member-delta-fallback` as surviving mutants and only the combined
    `member-delta-both` turns the case red.  Behaviour is byte-identical to the
    inline version (verified over 1,695 cached live functions).

    Returns (ok, evidence, delta, threshold).
    """
    # Hard blockers that must suppress the fallback (these are routed elsewhere;
    # the fallback would otherwise re-tag them as member deltas).
    if _FALLBACK_BLOCKERS & set(classes):
        return False, [], 0, 0

    # Check if all diff_insns are pure structural offset diffs (addi/mem w/ num delta)
    struct_count = 0
    total_diff = len(diff_insns)
    for ins in diff_insns:
        t = ins.get('target') or {}
        b = ins.get('base') or {}
        top = t.get('opcode', '')
        bop = b.get('opcode', '')
        ta = (t.get('args') or '')
        ba = (b.get('args') or '')
        if top == bop and top in ('addi', 'subi', 'addic'):
            tt = _tokens(ta)
            bt = _tokens(ba)
            if len(tt) >= 3 and len(bt) >= 3:
                tv = _toi(tt[2])
                bv = _toi(bt[2])
                if tv is not None and bv is not None and tv != bv:
                    struct_count += 1
        elif top == bop and top in MEM_LOADS_STORES:
            tt = _tokens(ta)
            bt = _tokens(ba)
            # Same base register required — an offset delta is only a member delta
            # when both sides dereference the SAME base (a `lwz r11,0x50,r1` vs
            # `lwz r11,0x0,r3` is a different load entirely, not a +0x50 member).
            if (len(tt) >= 3 and len(bt) >= 3 and tt[2] == bt[2]):
                tv = _toi(tt[1])
                bv = _toi(bt[1])
                if tv is not None and bv is not None and tv != bv:
                    struct_count += 1
    # Suppress the fallback if a FUNCLET diff carries a divergent named call —
    # the struct-offset shift came from a mis-paired structurally different funclet.
    # For NON-funclet functions, a divergent named call is usually ICF naming noise
    # (e.g. _Destroy_Range@Cheat folded with @Merger): the uniform member-size
    # delta is still the real signal, so do NOT suppress the fallback there.
    funclet_divergent_call = (funclet_reg is not None) and has_divergent_call
    if not (total_diff > 0 and struct_count >= 1 and not funclet_divergent_call):
        return False, [], 0, 0

    # Has struct offsets — compute the uniform delta from the structural diffs
    offsets = []
    for ins in diff_insns:
        t = ins.get('target') or {}
        b = ins.get('base') or {}
        top = t.get('opcode', '')
        bop = b.get('opcode', '')
        ta = (t.get('args') or '')
        ba = (b.get('args') or '')
        if top == bop and top in ('addi', 'subi', 'addic'):
            tt = _tokens(ta)
            bt = _tokens(ba)
            if len(tt) >= 3 and len(bt) >= 3:
                tv = _toi(tt[2])
                bv = _toi(bt[2])
                if tv is not None and bv is not None:
                    offsets.append((tv, bv))
        elif top == bop and top in MEM_LOADS_STORES:
            tt = _tokens(ta)
            bt = _tokens(ba)
            if len(tt) >= 3 and len(bt) >= 3 and tt[2] == bt[2]:
                tv = _toi(tt[1])
                bv = _toi(bt[1])
                if tv is not None and bv is not None:
                    offsets.append((tv, bv))
    if not offsets:
        return False, [], 0, 0

    deltas_fb = [t - b for t, b in offsets]
    fb_counter = Counter(deltas_fb)
    fb_dom_delta, fb_dom_count = fb_counter.most_common(1)[0]
    if not (fb_dom_count == len(deltas_fb) and fb_dom_delta != 0):
        return False, [], 0, 0
    fb_thresh = min(t for t, _ in offsets)
    return True, [
        f'fallback: uniform delta={fb_dom_delta:+d} on {len(offsets)} struct-offset diff(s)',
        f'threshold ~0x{fb_thresh:x}',
    ], fb_dom_delta, fb_thresh


# ── Main classifier ────────────────────────────────────────────────────────────

ROUTE_ORDER = [
    'PERMUTE', 'DEFER_VBASE', 'AT_LIMIT', 'INLINE_POLICY',
    'MEMBER_DELTA_CANDIDATE', 'DEFER_DEEP', 'UNKNOWN',
]


def classify_fn(unit: str, sym: str, diff_insns: Optional[List[dict]] = None,
                proj: str = ROOT) -> dict:
    """Classify one function and return a result dict.

    diff_insns: pre-fetched differing instructions (optimization). If None, will fetch.
    """
    # Fetch all instructions (needed for funclet prologue detection and this-tracing).
    # diff_fn returns the full JSON including all instructions; get_diff_instructions
    # already filters to mismatches only.  We need the full set here.
    _full_diff = diff_fn(unit, sym, proj)
    all_insns_full: List[dict] = _full_diff.get('instructions', []) if _full_diff else []

    if diff_insns is None:
        diff_insns = [ins for ins in all_insns_full
                      if ins.get('match_type') not in ('equal', None)]
        if not diff_insns and _full_diff is None:
            diff_insns = get_diff_instructions(unit, sym, proj)

    if not diff_insns:
        return {
            'unit': unit, 'sym': sym,
            'classes': ['CLEAN'], 'route': 'UNKNOWN',
            'confidence': 'low',
            'evidence': {'note': 'no diff instructions found'},
        }

    classes = []
    all_evidence: Dict[str, Any] = {}

    # 3a VBASE
    is_vbase, vbase_ev = _detect_vbase(diff_insns, unit, sym)
    # Track whether VBASE was from instruction diffs or only from metadata
    vbase_from_insn = any('vtable-slot lwz' in ev or 'adjustor:' in ev
                          for ev in vbase_ev)
    if is_vbase:
        classes.append('VBASE_WALL')
        all_evidence['VBASE_WALL'] = vbase_ev

    # 3b BOOL_NEG
    is_bool_neg, bool_ev = _detect_bool_neg(diff_insns)
    if is_bool_neg:
        classes.append('BOOL_NEG')
        all_evidence['BOOL_NEG'] = bool_ev

    # 3c SIGNEDNESS
    is_sign, sign_ev = _detect_signedness(diff_insns)
    if is_sign:
        classes.append('SIGNEDNESS')
        all_evidence['SIGNEDNESS'] = sign_ev

    # 3d FPR_SCHED
    is_fpr, fpr_ev = _is_fpr_heavy(diff_insns)
    if is_fpr:
        classes.append('FPR_SCHED')
        all_evidence['FPR_SCHED'] = fpr_ev

    # 3e INLINE_POLICY
    is_inline, inline_ev = _detect_inline_policy(diff_insns)
    if is_inline:
        classes.append('INLINE_POLICY')
        all_evidence['INLINE_POLICY'] = inline_ev

    # 3f UNVERIFIABLE
    is_unverif, unverif_ev = _detect_unverifiable(diff_insns)
    if is_unverif:
        classes.append('UNVERIFIABLE')
        all_evidence['UNVERIFIABLE'] = unverif_ev

    # 3g NO_ORACLE_LAYOUT
    is_no_oracle, no_oracle_ev = _detect_no_oracle_layout(diff_insns)
    if is_no_oracle:
        classes.append('NO_ORACLE_LAYOUT')
        all_evidence['NO_ORACLE_LAYOUT'] = no_oracle_ev

    # 3h SIZE_DIVERGENCE
    is_size, size_ev = _detect_size_divergence(diff_insns)
    if is_size:
        classes.append('SIZE_DIVERGENCE')
        all_evidence['SIZE_DIVERGENCE'] = size_ev

    # Funclet prologue register (used by the funclet gates below).  A funclet sets up
    # a frame register from r12 (subi rX, r12, imm) — the offsets keyed off it (or off
    # a vbase-adjusted derivative of it) are NOT plain member deltas.
    funclet_reg = _get_funclet_frame_reg(all_insns_full)

    # 1a (mdgrind MISROUTE 1) — DIVERGENT call target.  If this is a funclet AND the
    # diff contains a bl/b whose NAMED callee differs between sides, dtk address-paired
    # two structurally different funclets ⇒ FUNCLET_PAIRING regardless of direct-vs-
    # indirect access.  Genuine member-delta funclets (the CameraShot family) keep
    # IDENTICAL bl callees, so this gate won't steal them.
    has_divergent_call, divcall_ev = _detect_divergent_call(
        diff_insns, funclet_mode=(funclet_reg is not None))

    # 3i FUNCLET_PAIRING — check BEFORE MEMBER_DELTA (highest priority guard)
    # Uses all_insns_full for prologue detection (diff_insns don't show equal instructions).
    is_funclet, funclet_ev = _detect_funclet_pairing(all_insns_full, diff_insns, unit, sym)
    if funclet_reg is not None and has_divergent_call and not is_funclet:
        # Funclet with a divergent callee but indirect (looks-like-member) access:
        # this is exactly the size-44 mis-paired-funclet bug.  Treat as FUNCLET_PAIRING.
        is_funclet = True
        funclet_ev = [
            f'funclet prologue: subi {funclet_reg}, r12, <imm>',
            'diff contains a DIVERGENT named bl callee ⇒ dtk paired two structurally '
            'different funclets (mdgrind MISROUTE 1)',
        ] + divcall_ev
    if is_funclet:
        classes.append('FUNCLET_PAIRING')
        all_evidence['FUNCLET_PAIRING'] = funclet_ev

    # 1c (mdgrind MISROUTE 3) — VTABLE_DIVERGENCE: differing lwz slot off a vtable
    # pointer (base loaded from offset 0x0) feeding mtctr/bctrl.  DC3 added/removed
    # virtual slots.  Route DEFER_DEEP.  Check before MEMBER_DELTA so the uniform slot
    # delta isn't mistaken for a data-member delta.
    is_vtable_div, vtable_div_ev = _detect_vtable_divergence(all_insns_full, diff_insns)
    if is_vtable_div:
        classes.append('VTABLE_DIVERGENCE')
        all_evidence['VTABLE_DIVERGENCE'] = vtable_div_ev

    # 1d (CameraShot Ghidra finding) — VBASE adjust on a funclet-recovered `this`:
    # a frame-loaded pointer run through a large subi (vbase/secondary-base adjustor)
    # before the differing member access.  The indirect load alone does NOT clear
    # gate-zero; the vbase-adjusted subobject layout is compiler-determined.  Route
    # DEFER_VBASE.  Check before MEMBER_DELTA so these don't read as member deltas.
    is_vbase_adj, vbase_adj_ev = _detect_vbase_funclet_adjust(
        all_insns_full, diff_insns, funclet_reg)
    if is_vbase_adj and 'VBASE_WALL' not in classes:
        classes.append('VBASE_WALL')
        all_evidence.setdefault('VBASE_WALL', vbase_adj_ev)

    # 1b (mdgrind MISROUTE 2) — OFFSET_SWAP / regswap pair ⇒ PERMUTE, not member delta.
    # A real member delta is a UNIFORM SAME-SIGN shift; a mirrored +/-N swap nets zero.
    is_swap, swap_ev = _detect_offset_swap(diff_insns)
    if is_swap and 'FPR_SCHED' not in classes:
        classes.append('OFFSET_SWAP')
        all_evidence['OFFSET_SWAP'] = swap_ev

    # MEMBER_DELTA (runs extra diff_fn call — skip if already wall-classified or funclet)
    # Hard blockers that pre-empt MEMBER_DELTA: funclet pairing, vtable divergence,
    # vbase adjust, and offset-swap (all routed elsewhere above).
    _member_blockers = {'FUNCLET_PAIRING', 'VTABLE_DIVERGENCE', 'VBASE_WALL', 'OFFSET_SWAP'}
    is_member, member_ev, delta, threshold = (False, [], 0, 0)
    if not (_member_blockers & set(classes)):
        is_member, member_ev, delta, threshold = _detect_member_delta(diff_insns, unit, sym)
        if is_member and 'VBASE_WALL' not in classes:
            classes.append('MEMBER_DELTA')
            all_evidence['MEMBER_DELTA'] = {
                'evidence': member_ev,
                'delta': delta,
                'threshold': threshold,
            }

    # STRUCT_OFF fallback (the SECOND, independent member-delta reading) — see
    # _detect_member_delta_fallback.  Only runs when the this-tracer found
    # nothing and no hard blocker claimed the function.
    if not is_member and 'MEMBER_DELTA' not in classes:
        fb_ok, fb_ev, fb_delta, fb_thresh = _detect_member_delta_fallback(
            diff_insns, classes, funclet_reg, has_divergent_call)
        if fb_ok:
            classes.append('MEMBER_DELTA')
            all_evidence['MEMBER_DELTA'] = {
                'evidence': fb_ev,
                'delta': fb_delta,
                'threshold': fb_thresh,
                'confidence': 'low',
            }
            delta = fb_delta
            threshold = fb_thresh

    # ── Route assignment ──────────────────────────────────────────────────────
    # Priority: FUNCLET_PAIRING / VTABLE_DIVERGENCE > VBASE > OFFSET_SWAP
    #           > BOOL_NEG/SIGNEDNESS > FPR_SCHED > INLINE_POLICY > MEMBER_DELTA
    #           > SIZE/NO_ORACLE/UNVERIFIABLE > UNKNOWN
    route = 'UNKNOWN'
    confidence = 'medium'

    # FUNCLET_PAIRING / VTABLE_DIVERGENCE: highest priority — if the whole diff is a
    # frame-slot artifact (mis-paired funclet) or a DC3-added virtual slot, no source
    # edit fixes it.  Route DEFER_DEEP (same as SIZE_DIVERGENCE / NO_ORACLE).
    if 'FUNCLET_PAIRING' in classes or 'VTABLE_DIVERGENCE' in classes:
        route = 'DEFER_DEEP'
        confidence = 'high'
    else:
        # VBASE routing: hard-route to DEFER_VBASE if instruction diffs show vtable-slot
        # deltas / a funclet vbase-adjust, OR if VBASE is the only/dominant class.
        # If VBASE was detected only from metadata (COFF/header) but another actionable
        # class is present in the diffs, let the other class route it.
        vbase_dominates = ('VBASE_WALL' in classes and
                           (vbase_from_insn or is_vbase_adj or
                            set(classes) <= {'VBASE_WALL'} or
                            set(classes) - {'VBASE_WALL'} <= {'MEMBER_DELTA', 'OFFSET_SWAP'}))

        if vbase_dominates:
            route = 'DEFER_VBASE'
            confidence = 'high'
        elif 'OFFSET_SWAP' in classes and 'MEMBER_DELTA' not in classes:
            # Mirrored +/-N offset swap or regswap = instruction-scheduling/regalloc.
            route = 'PERMUTE'
            confidence = 'high'
        elif 'BOOL_NEG' in classes:
            route = 'AT_LIMIT'
            confidence = 'high'
        elif 'SIGNEDNESS' in classes and 'MEMBER_DELTA' not in classes:
            route = 'AT_LIMIT'
            confidence = 'medium'
        elif 'FPR_SCHED' in classes and 'MEMBER_DELTA' not in classes:
            route = 'PERMUTE'
            confidence = 'medium'
        elif 'INLINE_POLICY' in classes and 'MEMBER_DELTA' not in classes:
            route = 'INLINE_POLICY'
            confidence = 'medium'
        elif 'MEMBER_DELTA' in classes:
            route = 'MEMBER_DELTA_CANDIDATE'
            # Boost confidence if corroborated by multiple accesses; lower if fallback
            md = all_evidence.get('MEMBER_DELTA', {})
            ev_text = ' '.join(md.get('evidence', []))
            if md.get('confidence') == 'low' or 'fallback:' in ev_text:
                confidence = 'low'
            elif '100%' in ev_text or 'consistency' in ev_text:
                confidence = 'high'
            else:
                confidence = 'medium'
        elif 'SIZE_DIVERGENCE' in classes or 'NO_ORACLE_LAYOUT' in classes:
            route = 'DEFER_DEEP'
            confidence = 'medium'
        elif 'UNVERIFIABLE' in classes:
            route = 'DEFER_DEEP'
            confidence = 'medium'
        elif 'FPR_SCHED' in classes:
            # FPR + something else (e.g. INLINE_POLICY) — still permute
            route = 'PERMUTE'
            confidence = 'low'
        elif 'SIGNEDNESS' in classes:
            route = 'AT_LIMIT'
            confidence = 'low'
        else:
            route = 'UNKNOWN'
            confidence = 'low'

    if not classes:
        classes = ['UNKNOWN']

    return {
        'unit': unit,
        'sym': sym,
        'classes': classes,
        'route': route,
        'confidence': confidence,
        'evidence': all_evidence,
        'member_delta': all_evidence.get('MEMBER_DELTA', {}).get('delta'),
        'member_threshold': all_evidence.get('MEMBER_DELTA', {}).get('threshold'),
    }


# ── Worklist processing ────────────────────────────────────────────────────────

def process_worklist(worklist_path: str, proj: str = ROOT, verbose: bool = False) -> List[dict]:
    """Process all entries in the worklist and return classified results."""
    wl = json.load(open(worklist_path))
    entries = wl['entries']
    print(f"Processing {len(entries)} entries from {worklist_path} ...", file=sys.stderr)

    results = []
    for i, entry in enumerate(entries):
        if i and i % 50 == 0:
            print(f"  {i}/{len(entries)} ...", file=sys.stderr)
        unit = entry['unit']
        sym = entry['sym']

        # Build full diff_insns using the same mechanism as true_progress
        # (we need MORE than 3 samples, so re-run the diff)
        diff_insns = get_diff_instructions(unit, sym, proj)

        result = classify_fn(unit, sym, diff_insns, proj)
        # Carry over worklist metadata
        result['mp'] = entry.get('mp', 0)
        result['size'] = entry.get('size', 0)
        result['diff_count'] = entry.get('diff_count', 0)
        result['sub_classes'] = entry.get('sub_classes', [])
        result['address'] = entry.get('address', '')
        results.append(result)

        if verbose:
            print(f"  {sym[:55]:<55s} -> {result['route']:<25s} {result['classes']}")

    return results


# ── Validation: a hermetic fixture suite ──────────────────────────────────────
#
# Each case is verbatim `objdiff-cli diff ... --include-instructions -f json`
# output frozen under tools/testdata/wall_validate/, plus the COFF/header
# virtual-base answers the classifier would otherwise read out of build/ and
# src/.  Nothing here touches the live tree, so the score moves only when the
# CLASSIFIER changes.
#
# Every `expected_*` below was hand-read off the instruction stream in the
# fixture (the `truth` field records the reading) before it was written down.
# Do NOT add a case by running the classifier and freezing whatever it said --
# that scores the classifier against itself.  Two cases (the ??_G vbase pair
# and the FPR rename) carry INDEPENDENT ground truth: they are rows of the
# 2026-06-09 mdgrind wave, tools/testdata/mdgrind_gt48.jsonl, whose verdicts
# were reached by hand months before this classifier had these detectors.
#
# `discriminates` names the mutations (see MUTATIONS) that must turn the case
# red.  `--mutation-matrix` re-derives it and fails if a claim is wrong, so the
# field cannot silently rot into decoration.

FIXTURE_DIR = os.path.join(ROOT, 'tools', 'testdata', 'wall_validate')

VALIDATE_CASES = [
    {
        'slug': 'funclet-dform-faders',
        'label': 'Faders funclet (d-form frame slot)',
        'unit': 'default/Faders',
        'sym': 'fn_8270C66C',
        'expected_class': 'FUNCLET_PAIRING',
        'expected_route': 'DEFER_DEEP',
        'truth': 'Both sides open `subi r31, r12, 0xa0` -- identical funclet '
                 'prologue bytes, which is exactly how dtk address-pairs two '
                 'unrelated cleanup funclets. The single diff is `lwz r3, '
                 '0x50(r31)` vs `0x54(r31)`: a frame-SLOT index directly on the '
                 'funclet frame register, not a member offset on an object '
                 'pointer. No source edit fixes it => DEFER_DEEP.',
        'discriminates': ['ingest-args', 'funclet-prologue', 'funclet-direct'],
    },
    {
        'slug': 'funclet-framesize-mls',
        'label': 'MusicLibraryNetSetlists funclet (frame sizes differ)',
        'unit': 'default/band3/meta_band/MusicLibraryNetSetlists',
        'sym': 'fn_825D0AF8',
        'expected_class': 'FUNCLET_PAIRING',
        'expected_route': 'DEFER_DEEP',
        'truth': 'Prologue immediates DIFFER (`subi r31, r12, 0x2a0` vs '
                 '`0x260`) and the body access is `lwz r3, 0x60(r31)` vs '
                 '`0x5c(r31)`, directly on the frame register. Two different '
                 'funclets with different frame sizes. This is the case that '
                 'shows what the ruler-I defect COST: with ingest broken the '
                 'lwz is unreadable, only the two subi immediates parse, and '
                 'the fallback reports a uniform +0x40 MEMBER_DELTA -- i.e. it '
                 'hands a human a header edit for a frame-size difference '
                 'between two unrelated funclets.',
        'discriminates': ['ingest-args', 'funclet-prologue', 'funclet-direct'],
    },
    {
        'slug': 'funclet-addi-storeoffer',
        'label': 'StoreOfferProvider funclet (addi-form, no d-form operand)',
        'unit': 'default/band3/meta_band/StoreOfferProvider',
        'sym': 'fn_826653F0',
        'expected_class': 'FUNCLET_PAIRING',
        'expected_route': 'DEFER_DEEP',
        'truth': 'Same shape as the Faders case but the differing access is '
                 '`addi r3, r31, 0xb8` vs `0xd0` -- three bare comma-separated '
                 'operands in BOTH spellings. Deliberate CONTRAST case: it '
                 'proves the funclet route survives the ruler-I defect when no '
                 'd-form operand is involved, so a green here plus a red on the '
                 'two cases above localises the fault to operand ingest rather '
                 'than to the funclet detector.',
        'discriminates': ['funclet-prologue', 'funclet-direct'],
    },
    {
        'slug': 'funclet-divcall-ambientocclusion',
        'label': 'AmbientOcclusion funclet (divergent bl callee)',
        'unit': 'default/AmbientOcclusion',
        'sym': 'fn_82491300',
        'expected_class': 'FUNCLET_PAIRING',
        'expected_route': 'DEFER_DEEP',
        'truth': 'mdgrind MISROUTE-1, the arm with no coverage before this '
                 'suite. Funclet prologue (`subi r31, r12, 0x170` vs `0x1a0`); '
                 'the differing instruction is `bl ??1Message@@UAA@XZ` vs `bl '
                 '??1DataNode@@QAA@XZ` -- the two bodies destroy DIFFERENT '
                 'types, so the pairing is structural nonsense. The frame '
                 'access `addi r3, r31, 0x70` is EQUAL, so _is_funclet_direct_'
                 'access says False: this case can only be caught by the '
                 'divergent-callee arm.',
        'discriminates': ['funclet-prologue', 'funclet-divcall'],
    },
    {
        'slug': 'vtable-div-preload-synth',
        'label': 'Hmx::Object::PreLoad thunk (vtable slot)',
        'unit': 'default/system/synth_xbox/Synth',
        'sym': '?PreLoad@Object@Hmx@@UAAXAAVBinStream@@@Z',
        'expected_class': 'VTABLE_DIVERGENCE',
        'expected_route': 'DEFER_DEEP',
        'truth': 'Four-instruction virtual thunk: `lwz r11, 0x0(r3)` (vtable '
                 'pointer) -> differing `lwz r11, 0x40(r11)` vs `0x28(r11)` -> '
                 '`mtctr r11` -> `bctr`. The delta is a vtable SLOT index '
                 '(0x18 = 6 slots), fixable only by reconstructing the vtable '
                 'layout, so DEFER_DEEP. A data-member reading of that offset '
                 'would be wrong.',
        'discriminates': ['ingest-args', 'vtable-div'],
    },
    {
        'slug': 'vbase-adjustor-hamlistribbon',
        'label': 'HamListRibbon ??_G (diverging vbase adjustor)',
        'unit': 'default/HamListRibbon',
        'sym': '??_GHamListRibbon@@UAAPAXI@Z',
        'expected_class': 'VBASE_WALL',
        'expected_route': 'DEFER_VBASE',
        'truth': 'Independent ground truth: mdgrind_gt48.jsonl row, wall_class '
                 'VBASE_WALL, 2026-06-09. The vector-deleting dtor computes its '
                 'subobject pointer with `subi r31, r3, 0x168` vs `0x314` -- a '
                 'DIVERGING large immediate, i.e. the compiler-computed '
                 'secondary-base offset, which no member edit reaches. '
                 'Corroborated by three ??_8HamListRibbon@@7B* vbtables in the '
                 'COFF symbol table and by virtual inheritance in the header.',
        'discriminates': ['vbase'],
    },
    {
        'slug': 'vbase-coff-rndribbon',
        'label': 'RndRibbon ??_G (COFF/header vbase, weak adjustor)',
        'unit': 'default/Ribbon',
        'sym': '??_GRndRibbon@@UAAPAXI@Z',
        'expected_class': 'VBASE_WALL',
        'expected_route': 'DEFER_VBASE',
        'truth': 'Independent ground truth: mdgrind_gt48.jsonl row, wall_class '
                 'VBASE_WALL, 2026-06-09. Same family as the case above but it '
                 'reaches the verdict down the OTHER path: the adjustor `subi '
                 'r31, r3, 0x58` vs `0x88` is read as a plain base-subobject '
                 'adjust and the vbase call is carried by the COFF ??_8 '
                 'vbtables plus header virtual inheritance. Paired with the '
                 'HamListRibbon case so a `vbase` mutation cannot be passed off '
                 'as a one-off.',
        'discriminates': ['vbase'],
    },
    {
        'slug': 'boolneg-dxrnd-present',
        'label': 'DxRnd::Present (bool normalization)',
        'unit': 'default/Rnd_Xbox',
        'sym': '?Present@DxRnd@@QAAXXZ',
        'expected_class': 'BOOL_NEG',
        'expected_route': 'AT_LIMIT',
        'truth': 'Survivor of the original pilot set, still live. Playbook '
                 'Section 3b verbatim: target normalizes a bool with `rlwinm '
                 'r11, r3, 0, 30, 30` + `subic` + `subfe` where our base emits '
                 '`extrwi r11, r3, 1, 30`. No known source spelling reaches the '
                 'retail sequence => AT_LIMIT.',
        'discriminates': ['bool-neg'],
    },
    {
        'slug': 'fpr-rename-postproc',
        'label': 'NgPostProc::CheckPosterizeAndKaleidoscope (FPR rename)',
        'unit': 'default/PostProc_NG',
        'sym': '?CheckPosterizeAndKaleidoscope@NgPostProc@@IAAXXZ',
        'expected_class': 'FPR_SCHED',
        'expected_route': 'PERMUTE',
        'truth': 'Independent ground truth: mdgrind_gt48.jsonl row, wall_class '
                 'PERMUTE, 2026-06-09. Two diffs, same opcodes, same operand '
                 'roles, f0 and f13 exchanged: `fmuls f0, f12, f0` vs `fmuls '
                 'f13, f12, f13` and `fdivs f13, f13, f9` vs `fdivs f0, f0, '
                 'f9`. A pure FPR register-allocation choice => /permute.',
        'discriminates': ['fpr'],
    },
    {
        'slug': 'fpr-sched-transformnormal',
        'label': 'TransformNormal (FPR sched + spill slots)',
        'unit': 'default/Mesh',
        'sym': '?TransformNormal@@YA?AVVector3@@ABV1@ABVMatrix3@Hmx@@@Z',
        'expected_class': 'FPR_SCHED',
        'expected_route': 'PERMUTE',
        'truth': 'Survivor of the original pilot set, still live. The whole FPR '
                 'file is renamed (f12->f0, f10->f12, ...), the `lfs` spill '
                 'slots off r1 are reassigned, and the `fmadds` chain is '
                 'reassociated across the same three dot products. Register '
                 'allocation and scheduling, no source-visible difference => '
                 'PERMUTE.',
        'discriminates': ['fpr'],
    },
    {
        'slug': 'offset-swap-kerningtable',
        'label': 'KerningTable::SetKerning (mirrored offset swap)',
        'unit': 'default/Font',
        'sym': ('?SetKerning@KerningTable@@QAAXABV?$vector@VKernInfo@RndFont@@'
                'V?$StlNodeAlloc@VKernInfo@RndFont@@@stlpmtx_std@@@stlpmtx_std@@'
                'PAVRndFont@@@Z'),
        'expected_class': 'OFFSET_SWAP',
        'expected_route': 'PERMUTE',
        'truth': 'mdgrind MISROUTE-2. Two adjacent halfword loads TRADE their '
                 'displacements: `lhz r9, 0x0(r31)` / `lhz r10, 0x2(r31)` vs '
                 '`0x2` / `0x0`. The deltas are +2 and -2, so the net '
                 'same-sign shift is zero -- a scheduling reorder, NOT a layout '
                 'difference. Guards the classifier against reporting a member '
                 'delta here.',
        'discriminates': ['ingest-args', 'offset-swap'],
    },
    {
        'slug': 'member-delta-resetbeattasktime',
        'label': 'TaskMgr::ResetBeatTaskTime (uniform this-relative delta)',
        'unit': 'default/Task',
        'sym': '?ResetBeatTaskTime@TaskMgr@@QAAXM@Z',
        'expected_class': 'MEMBER_DELTA',
        'expected_route': 'MEMBER_DELTA_CANDIDATE',
        'truth': 'Three-instruction function; the one diff is `lwz r11, '
                 '0x60(r3)` vs `0x28(r3)` -- same base register, and r3 is '
                 '`this` on entry with no intervening call to clobber it. A '
                 'clean uniform +0x38 this-relative shift, the one shape that '
                 'IS a one-line header fix. Positive control: the funclet, '
                 'vtable, vbase and offset-swap guards above must not steal it. '
                 'NOTE it does not isolate either member-delta reading: the '
                 'this-tracer and the STRUCT_OFF fallback both reach the same '
                 'verdict here, so only `member-delta-both` turns it red.',
        'discriminates': ['ingest-args', 'member-delta-both'],
    },
    {
        'slug': 'size-divergence-uipanel-newobject',
        'label': 'UIPanel::NewObject (operator new size)',
        'unit': 'default/UI',
        'sym': '?NewObject@UIPanel@@SAPAVObject@Hmx@@XZ',
        'expected_class': 'SIZE_DIVERGENCE',
        'expected_route': 'DEFER_DEEP',
        'truth': 'Playbook Section 3h: `li r3, 0x108` vs `li r3, 0x68` feeding '
                 'the allocator -- retail UIPanel is 0x108 bytes and ours is '
                 '0x68. The base side additionally carries a vbtable lookup '
                 '(`lwz r11, 0x4(r3)` / `lwz r11, 0x4(r11)` / `add` / `addi '
                 'r3, r11, 0x4`) that retail does not. A whole-class size '
                 'reconstruction, not a local edit => DEFER_DEEP.',
        'discriminates': ['size-div'],
    },
    {
        'slug': 'inline-policy-addsongdata',
        'label': 'BandSongMgr::AddSongData (call vs const-fold)',
        'unit': 'default/BandSongMgr',
        'sym': ('?AddSongData@BandSongMgr@@UAAXPAVDataArray@@AAV?$hash_map@HPAV'
                'SongMetadata@@U?$hash@H@stlpmtx_std@@U?$equal_to@H@3@V?$StlNode'
                'Alloc@U?$pair@$$CBHPAVSongMetadata@@@stlpmtx_std@@@3@@stlpmtx_'
                'std@@PBDW4ContentLocT@@AAV?$vector@HV?$StlNodeAlloc@H@stlpmtx_'
                'std@@@4@@Z'),
        'expected_class': 'INLINE_POLICY',
        'expected_route': 'INLINE_POLICY',
        'truth': 'Playbook Section 3e: retail has `li r3, 0x0` where our base '
                 'emits `bl ?IsInExclusionList@BandSongMgr@@QBA_NPBDH@Z`. The '
                 'retail compiler inlined and const-folded the predicate; ours '
                 'calls it. An inline-policy difference, routed to its own '
                 'lane. Also exercises the metadata-only VBASE path NOT taking '
                 'the route: BandSongMgr has a ??_8 vbtable, but with no '
                 'vbase evidence in the instructions INLINE_POLICY wins.',
        'discriminates': ['inline'],
    },
]

# Classes that had a case in the pilot set and have NO hand-verifiable live
# exemplar left.  Recorded rather than faked: a smaller honest suite beats a
# larger vacuous one.  Re-add a case here the moment a clean one appears.
DROPPED_CLASSES = {
    'SIGNEDNESS': (
        'Both pilot targets (String::operator=, MicNull::MicNull) reached 100%. '
        'Swept 1,690 live near-miss functions: 14 trip the extsb/extsh detector '
        'and the smallest carries 16 differing instructions, all of them cases '
        'where the target TAIL-CALLS an ICF-folded CRT routine and our base '
        'inlines the loop -- an inline-policy difference the signedness gate '
        'happens to catch, not a clean signedness wall. Nothing here is '
        'honestly labellable as SIGNEDNESS ground truth.'),
    'UNVERIFIABLE': (
        'Pilot target BlockMgr::ReadError reached 100%. ZERO hits across 1,690 '
        'live near-miss functions: the `bl lbl_<addr>` shape the detector looks '
        'for is gone from this tree now that symbol identification has '
        'advanced. The detector is untested, and this suite says so instead of '
        'pretending otherwise.'),
    'NO_ORACLE_LAYOUT': (
        '10 live hits, but not one of them ROUTES through NO_ORACLE_LAYOUT -- '
        'every one also trips FPR_SCHED or OFFSET_SWAP, which outrank it, and '
        'the smallest carries 23 differing instructions. A case whose expected '
        'route is decided by a different detector would not test this one.'),
}


# ── Mutations: the suite has to prove it can go red ───────────────────────────
#
# Each entry re-introduces ONE defect and returns a restore callable.  A case
# that no mutation turns red is measuring nothing, and --mutation-matrix is
# what makes that visible instead of assumed.

def _mut_ingest_args():
    """The ruler-I (objdiff-cli fdc5113) defect, verbatim: leave the DISPLAY
    operand spelling in `args`, so `0xNN(rY)` never splits on ','."""
    orig = _normalize_args_inplace

    def broken(d: dict) -> dict:
        for ins in d.get('instructions') or []:
            for k in ('target', 'base'):
                side = ins.get(k)
                if side:
                    side['args_display'] = side.get('args') or ''
        return d
    globals()['_normalize_args_inplace'] = broken
    return lambda: globals().__setitem__('_normalize_args_inplace', orig)


def _mut_stub(name: str, value):
    """Replace module-global callable `name` with one returning `value`."""
    def apply():
        orig = globals()[name]
        globals()[name] = lambda *a, **kw: value
        return lambda: globals().__setitem__(name, orig)
    return apply


def _mut_both_member_delta():
    """Disable BOTH member-delta readings at once.

    Each is individually invisible on every clean live case -- they are
    redundant -- so only the pair moves the score.  See EXPECTED_SURVIVORS.
    """
    r1 = _mut_stub('_detect_member_delta', (False, [], 0, 0))()
    r2 = _mut_stub('_detect_member_delta_fallback', (False, [], 0, 0))()

    def restore():
        r1()
        r2()
    return restore


MUTATIONS = {
    'ingest-args':      ('operand ingest normalization removed (the ruler-I defect)',
                         _mut_ingest_args),
    'funclet-prologue': ('funclet frame register never detected',
                         _mut_stub('_get_funclet_frame_reg', None)),
    'funclet-direct':   ('direct-frame-access arm disabled',
                         _mut_stub('_is_funclet_direct_access', False)),
    'funclet-divcall':  ('divergent-callee arm disabled (mdgrind MISROUTE-1)',
                         _mut_stub('_detect_divergent_call', (False, []))),
    'vtable-div':       ('vtable-slot arm disabled (mdgrind MISROUTE-3)',
                         _mut_stub('_detect_vtable_divergence', (False, []))),
    'offset-swap':      ('mirrored-offset arm disabled (mdgrind MISROUTE-2)',
                         _mut_stub('_detect_offset_swap', (False, []))),
    'vbase':            ('virtual-base detector disabled',
                         _mut_stub('_detect_vbase', (False, []))),
    'bool-neg':         ('boolean-normalization detector disabled',
                         _mut_stub('_detect_bool_neg', (False, []))),
    'fpr':              ('FPR scheduling detector disabled',
                         _mut_stub('_is_fpr_heavy', (False, []))),
    'inline':           ('inline-policy detector disabled',
                         _mut_stub('_detect_inline_policy', (False, []))),
    'size-div':         ('operator-new size detector disabled',
                         _mut_stub('_detect_size_divergence', (False, []))),
    'member-delta':     ('this-relative member-delta tracer disabled',
                         _mut_stub('_detect_member_delta', (False, [], 0, 0))),
    'member-delta-fallback': ('struct-offset member-delta fallback disabled',
                              _mut_stub('_detect_member_delta_fallback',
                                        (False, [], 0, 0))),
    'member-delta-both': ('BOTH member-delta readings disabled',
                          _mut_both_member_delta),
}


# Mutations that are EXPECTED to survive, with the reason.  Not a licence to
# ignore a survivor: each line is a measured statement about redundancy in the
# classifier, and --mutation-matrix still prints them.
EXPECTED_SURVIVORS = {
    'member-delta': (
        'the STRUCT_OFF fallback re-derives the same verdict, so disabling the '
        'this-tracer alone changes nothing'),
    'member-delta-fallback': (
        'the this-tracer already produced the verdict, so disabling the '
        'fallback alone changes nothing'),
}


# ── Fixture load / capture ────────────────────────────────────────────────────

def _fixture_path(case: dict) -> str:
    return os.path.join(FIXTURE_DIR, case['slug'] + '.json')


def _install_fixture(case: dict) -> dict:
    """Point diff_fn and the vbase lookups at this case's frozen capture."""
    import copy as _copy
    path = _fixture_path(case)
    if not os.path.exists(path):
        raise FileNotFoundError(path)
    with open(path) as f:
        raw = json.load(f)
    meta = raw.get('_wc_fixture') or {}

    def source(unit: str, sym: str) -> Optional[dict]:
        if (unit, sym) != (case['unit'], case['sym']):
            return None
        # Mirror diff_fn's real contract: a fresh object, normalized at ingest.
        # Looked up through globals() so --mutate ingest-args reaches it.
        return globals()['_normalize_args_inplace'](_copy.deepcopy(raw))

    globals()['_DIFF_SOURCE'] = source
    globals()['_VBASE_SOURCE'] = meta.get('vbase') or {'coff': {}, 'header': {}}
    return meta


def _clear_fixture() -> None:
    globals()['_DIFF_SOURCE'] = None
    globals()['_VBASE_SOURCE'] = None


def _capture_vbase_env(unit: str, sym: str) -> dict:
    """Snapshot the COFF/header virtual-base answers for this symbol's class."""
    cls = demangle_class(sym, unit)
    _clear_fixture()
    coff_ok, coff_syms = unit_vbase_for_class(unit, cls)
    hdr = get_idb().has_virtual_base(cls) if cls else False
    return {'coff': {cls: list(coff_syms)} if coff_ok else {},
            'header': {cls: bool(hdr)}}


def refresh_fixtures(proj: str = ROOT, only: Optional[str] = None) -> int:
    """Re-capture every fixture from the live tree.  VERBATIM objdiff-cli JSON.

    The classifier normalizes operands at INGEST, so a hand-authored fixture
    would be testing a spelling this tool never actually receives.  Everything
    written here comes straight out of `objdiff-cli diff -f json
    --include-instructions`; only a `_wc_fixture` provenance block is added.
    """
    os.makedirs(FIXTURE_DIR, exist_ok=True)
    import datetime
    import shutil
    cli_ver = ''
    try:
        cli_ver = subprocess.run([CLI, '--version'], capture_output=True,
                                 text=True, timeout=30).stdout.strip()
    except Exception:
        pass
    head = ''
    if shutil.which('git'):
        try:
            head = subprocess.run(['git', '-C', proj, 'rev-parse', 'HEAD'],
                                  capture_output=True, text=True,
                                  timeout=30).stdout.strip()
        except Exception:
            pass

    _clear_fixture()
    bad = 0
    for case in VALIDATE_CASES:
        if only and case['slug'] != only:
            continue
        unit, sym = case['unit'], case['sym']
        d = diff_fn(unit, sym, proj)
        if not d:
            print(f"  MISSING  {case['slug']}: objdiff returned nothing for "
                  f"{sym} [{unit}]", file=sys.stderr)
            bad += 1
            continue
        n = sum(1 for i in d.get('instructions') or []
                if i.get('match_type') not in ('equal', None))
        if n == 0:
            print(f"  EMPTY    {case['slug']}: 0 differing instructions -- the "
                  f"function is matched now; drop or replace the case",
                  file=sys.stderr)
            bad += 1
            continue
        # diff_fn normalized in place; re-read the raw capture so the fixture
        # keeps the spelling objdiff-cli actually emits.
        for ins in d.get('instructions') or []:
            for k in ('target', 'base'):
                side = ins.get(k)
                if side and 'args_display' in side:
                    side['args'] = side.pop('args_display')
        d['_wc_fixture'] = {
            'slug': case['slug'], 'unit': unit, 'sym': sym,
            'captured': datetime.date.today().isoformat(),
            'objdiff_cli': cli_ver, 'repo_head': head,
            'diff_count': n,
            'vbase': _capture_vbase_env(unit, sym),
        }
        with open(_fixture_path(case), 'w') as f:
            json.dump(d, f, indent=1, sort_keys=True)
        print(f"  wrote    {case['slug']}  ({n} differing instructions)",
              file=sys.stderr)
    return bad


# ── Validation runners ────────────────────────────────────────────────────────

def _classify_case(case: dict, proj: str, live: bool) -> dict:
    if live:
        _clear_fixture()
    else:
        _install_fixture(case)
    try:
        return classify_fn(case['unit'], case['sym'], None, proj)
    finally:
        _clear_fixture()


def run_validation(proj: str = ROOT, live: bool = False,
                   mutate: Optional[str] = None, quiet: bool = False):
    """Score the classifier against the fixture suite.  Returns (rows, correct)."""
    restore = None
    if mutate:
        if mutate not in MUTATIONS:
            print(f"ERROR: unknown mutation {mutate!r}; known: "
                  f"{', '.join(sorted(MUTATIONS))}", file=sys.stderr)
            sys.exit(2)
        restore = MUTATIONS[mutate][1]()

    rows = []
    correct = 0
    try:
        for case in VALIDATE_CASES:
            try:
                result = _classify_case(case, proj, live)
            except FileNotFoundError as e:
                rows.append({'case': case, 'status': 'NO_FIXTURE',
                             'got_classes': [], 'got_route': '-',
                             'evidence': {'error': str(e)}})
                continue
            class_ok = case['expected_class'] in result['classes']
            route_ok = result['route'] == case['expected_route']
            ok = class_ok and route_ok
            correct += bool(ok)
            rows.append({
                'case': case,
                'status': 'PASS' if ok else ('PARTIAL' if (class_ok or route_ok)
                                             else 'FAIL'),
                'got_classes': result['classes'],
                'got_route': result['route'],
                'evidence': result.get('evidence', {}),
            })
    finally:
        if restore:
            restore()

    if quiet:
        return rows, correct

    total = len(VALIDATE_CASES)
    src = 'LIVE TREE' if live else f'fixtures ({FIXTURE_DIR})'
    print(f"\n=== wall_classify validation -- source: {src} ===")
    if mutate:
        print(f"=== MUTATION ACTIVE: {mutate} -- {MUTATIONS[mutate][0]} ===")
    print()
    print(f"{'Case':<46} {'Status':<10} {'Expected':<20} {'Got class':<34} Route")
    print('-' * 140)
    for r in rows:
        c = r['case']
        got = ','.join(r['got_classes'])[:32] or '-'
        print(f"{c['label'][:45]:<46} {r['status']:<10} {c['expected_class']:<20} "
              f"{got:<34} {r['got_route']} (exp:{c['expected_route']})")
    print(f"\nAccuracy: {correct}/{total}")

    failures = [r for r in rows if r['status'] != 'PASS']
    if failures:
        print("\nFailure details:")
        for r in failures:
            c = r['case']
            print(f"\n  [{c['label']}]  {c['unit']} :: {c['sym'][:60]}")
            print(f"    expected: class={c['expected_class']} "
                  f"route={c['expected_route']}")
            print(f"    got:      class={r['got_classes']} route={r['got_route']}")
            print(f"    truth:    {c['truth']}")
            for cls, ev in (r['evidence'] or {}).items():
                print(f"    {cls}: {ev}")
    if DROPPED_CLASSES and not live:
        print("\nClasses with NO case in this suite (deliberately, not by "
              "oversight):")
        for cls, why in sorted(DROPPED_CLASSES.items()):
            print(f"  {cls}: {why}")
    return rows, correct


def run_mutation_matrix(proj: str = ROOT) -> int:
    """Run the suite once per mutation and print which case each turns red.

    This is the evidence that the suite is a control at all.  Exit non-zero if
    any mutation leaves the score untouched (a mutant nothing detects) or if a
    case's declared `discriminates` list does not match what actually happened.
    """
    base_rows, base_correct = run_validation(proj, quiet=True)
    total = len(VALIDATE_CASES)
    base_pass = {r['case']['slug'] for r in base_rows if r['status'] == 'PASS'}
    print(f"\n=== mutation matrix ({total} cases, "
          f"{len(MUTATIONS)} mutations) ===")
    print(f"baseline (no mutation): {base_correct}/{total}\n")

    slugs = [c['slug'] for c in VALIDATE_CASES]
    width = max(len(s) for s in slugs) + 2
    observed: Dict[str, Set[str]] = {s: set() for s in slugs}
    survivors = []

    print(f"{'mutation':<22} {'score':<9} cases turned red")
    print('-' * 100)
    for name in sorted(MUTATIONS):
        restore = MUTATIONS[name][1]()
        try:
            rows, correct = run_validation(proj, quiet=True)
        finally:
            restore()
        reds = [r['case']['slug'] for r in rows
                if r['status'] != 'PASS' and r['case']['slug'] in base_pass]
        for s in reds:
            observed[s].add(name)
        if not reds:
            if name in EXPECTED_SURVIVORS:
                note = f'(expected survivor: {EXPECTED_SURVIVORS[name]})'
            else:
                note = '** NOTHING -- MUTANT SURVIVED **'
                survivors.append(name)
        else:
            note = ', '.join(reds)
        print(f"{name:<24} {correct}/{total:<7} {note}")

    print(f"\n{'case':<{width}} mutations that turn it red")
    print('-' * 100)
    dead = []
    mismatched = []
    for case in VALIDATE_CASES:
        s = case['slug']
        got = observed[s]
        declared = set(case.get('discriminates') or [])
        mark = ''
        if not got:
            dead.append(s)
            mark = '   ** DISCRIMINATES NOTHING **'
        elif got != declared:
            mismatched.append((s, sorted(declared), sorted(got)))
            mark = f'   ** declared {sorted(declared)} **'
        print(f"{s:<{width}} {', '.join(sorted(got)) or '-'}{mark}")

    rc = 0
    if survivors:
        print(f"\nSURVIVING MUTANTS ({len(survivors)}): "
              f"{', '.join(survivors)} -- no case covers these arms.")
        rc = 1
    if dead:
        print(f"\nCASES THAT MEASURE NOTHING ({len(dead)}): {', '.join(dead)}")
        rc = 1
    if mismatched:
        print("\nDECLARED-vs-OBSERVED MISMATCH:")
        for s, decl, got in mismatched:
            print(f"  {s}: declared {decl}, observed {got}")
        rc = 1
    if rc == 0:
        print("\nOK: every mutation is caught, every case catches something, "
              "and every declared `discriminates` list is exact.")
    return rc


# ── Output formatting ──────────────────────────────────────────────────────────

def print_summary(results: List[dict]) -> None:
    """Print summary table to stdout."""
    route_counts = Counter(r['route'] for r in results)
    class_counts = Counter()
    for r in results:
        for c in r['classes']:
            class_counts[c] += 1

    print(f"\n=== Wall-classifier route distribution (n={len(results)}) ===\n")
    print(f"{'Route':<30} {'Count':>6}  {'%':>5}")
    print('-' * 46)
    for route in ROUTE_ORDER:
        n = route_counts.get(route, 0)
        pct = n / len(results) * 100 if results else 0
        suffix = ''
        if route == 'DEFER_DEEP':
            fp_n = sum(1 for r in results if r['route'] == 'DEFER_DEEP'
                       and 'FUNCLET_PAIRING' in r['classes'])
            other_n = n - fp_n
            suffix = f'  [{fp_n} FUNCLET_PAIRING + {other_n} other]'
        print(f"  {route:<28} {n:>6}  {pct:>4.1f}%{suffix}")
    print()
    print(f"Class breakdown (functions can have multiple):")
    for cls in ['VBASE_WALL', 'BOOL_NEG', 'SIGNEDNESS', 'FPR_SCHED', 'INLINE_POLICY',
                'UNVERIFIABLE', 'NO_ORACLE_LAYOUT', 'SIZE_DIVERGENCE', 'FUNCLET_PAIRING',
                'VTABLE_DIVERGENCE', 'OFFSET_SWAP', 'MEMBER_DELTA', 'UNKNOWN']:
        n = class_counts.get(cls, 0)
        if n:
            print(f"  {cls:<25} {n:>5}")

    # MEMBER_DELTA clusters (force-multiplier candidates)
    md_candidates = [r for r in results if r['route'] == 'MEMBER_DELTA_CANDIDATE']
    from collections import defaultdict
    by_unit_delta: Dict[Tuple, List] = defaultdict(list)
    for r in md_candidates:
        key = (r['unit'], r.get('member_delta'), r.get('member_threshold'))
        by_unit_delta[key].append(r)
    clusters = sorted(by_unit_delta.items(), key=lambda x: -len(x[1]))
    print(f"\nTop MEMBER_DELTA_CANDIDATE clusters (force-multiplier opportunities; {len(md_candidates)} total):")
    for (unit, delta, thresh), fns in clusters[:10]:
        thresh_s = f'@0x{thresh:x}' if thresh is not None else ''
        # Confidence: if multiple methods agree, confidence is higher
        conf = 'HIGH' if len(fns) >= 3 else ('MED' if len(fns) >= 2 else 'LOW')
        print(f"  [{conf}] unit={unit} delta={delta:+d}{thresh_s} x{len(fns)} methods")

    print(f"\nTop-10 MEMBER_DELTA_CANDIDATE entries (by delta magnitude, then size):")
    md_sorted = sorted(md_candidates, key=lambda r: (abs(r.get('member_delta') or 999), r.get('size', 0)))
    for r in md_sorted[:10]:
        delta = r.get('member_delta', '?')
        thresh = r.get('member_threshold')
        thresh_s = f'@0x{thresh:x}' if thresh is not None else ''
        conf = r.get('confidence', '?')
        print(f"  mp={r['mp']:.3f} conf={conf} delta={delta:+d}{thresh_s}  {r['sym'][:55]}  [{r['unit']}]")

    # Top UNKNOWN
    unknowns = [r for r in results if r['route'] == 'UNKNOWN']
    unknowns.sort(key=lambda r: (r.get('diff_count', 0), r.get('size', 0)))
    print(f"\nTop-10 UNKNOWN ({len(unknowns)} total — needs human/agent eyes):")
    for r in unknowns[:10]:
        sc = ','.join(r.get('sub_classes', []))
        print(f"  mp={r['mp']:.3f} diffs={r.get('diff_count',0)} [{sc}]  {r['sym'][:55]}  [{r['unit']}]")


# ── Entry point ────────────────────────────────────────────────────────────────

def main():
    ap = argparse.ArgumentParser(description='Classify HAS_REAL near-miss pool wall types.')
    ap.add_argument('--worklist', default=os.path.expanduser('~/tmp/hasreal_worklist.json'),
                    help='Input worklist JSON (default ~/tmp/hasreal_worklist.json)')
    ap.add_argument('--out', default=os.path.expanduser('~/tmp/hasreal_routed.json'),
                    help='Output routed JSON (default ~/tmp/hasreal_routed.json)')
    ap.add_argument('--validate', action='store_true',
                    help='Score the classifier against the frozen fixture suite')
    ap.add_argument('--validate-live', action='store_true',
                    help='Same cases, but re-diffed against the LIVE tree. Use to '
                         'see how far the tree has drifted from the fixtures; a '
                         'red here is drift, NOT a classifier regression')
    ap.add_argument('--mutate', default='',
                    help='Apply one named defect before validating (see '
                         '--list-mutations)')
    ap.add_argument('--mutation-matrix', action='store_true',
                    help='Run the suite once per mutation and report which case '
                         'each one turns red; exits non-zero if any mutant '
                         'survives or any case discriminates nothing')
    ap.add_argument('--list-mutations', action='store_true',
                    help='List the available mutations and exit')
    ap.add_argument('--refresh-fixtures', action='store_true',
                    help='Re-capture every fixture from the live tree (verbatim '
                         'objdiff-cli JSON) and exit')
    ap.add_argument('--only', default='',
                    help='Restrict --refresh-fixtures to one case slug')
    ap.add_argument('--sym', default='',
                    help='Classify a single symbol (requires --unit)')
    ap.add_argument('--unit', default='',
                    help='Unit for --sym')
    ap.add_argument('--proj', default=ROOT,
                    help='Project root (default: auto-detected)')
    ap.add_argument('--verbose', action='store_true',
                    help='Print per-function classification as it runs')
    a = ap.parse_args()

    if a.list_mutations:
        for name in sorted(MUTATIONS):
            print(f'  {name:<20} {MUTATIONS[name][0]}')
        return

    if a.refresh_fixtures:
        sys.exit(1 if refresh_fixtures(a.proj, a.only or None) else 0)

    if a.mutation_matrix:
        sys.exit(run_mutation_matrix(a.proj))

    if a.validate or a.validate_live:
        _rows, correct = run_validation(a.proj, live=a.validate_live,
                                        mutate=a.mutate or None)
        sys.exit(0 if correct == len(VALIDATE_CASES) else 1)

    if a.sym:
        if not a.unit:
            print("ERROR: --sym requires --unit", file=sys.stderr)
            sys.exit(1)
        result = classify_fn(a.unit, a.sym, None, a.proj)
        print(json.dumps(result, indent=2))
        return

    results = process_worklist(a.worklist, a.proj, a.verbose)

    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    json.dump({
        'count': len(results),
        'entries': results,
    }, open(a.out, 'w'), indent=1)
    print(f"\nWrote {len(results)} classified entries to {a.out}", file=sys.stderr)

    print_summary(results)


if __name__ == '__main__':
    main()
