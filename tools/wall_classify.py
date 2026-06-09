#!/usr/bin/env python3
"""wall_classify.py — Auto-tag HAS_REAL near-miss functions with playbook wall classes.

Implements the 8 wall detectors from docs/decomp/playbooks/hasreal-grind.md §3 plus
MEMBER_DELTA, and routes each function to an actionable decision.

Wall classes (from playbook §3):
  VBASE_WALL        — vtable-slot +4 delta via virtual base (3a); DEFER
  BOOL_NEG          — subic/subfe vs extrwi boolean normalization (3b); AT_LIMIT
  SIGNEDNESS        — extsb/extsh in CRT-intrinsic loops (3c); AT_LIMIT
  FPR_SCHED         — FPR scheduling/regalloc/CSE (3d); PERMUTE
  INLINE_POLICY     — bl <named> vs inlined body (3e); INLINE_POLICY route
  UNVERIFIABLE      — bl lbl_/fn_ in logic position (3f); DEFER_DEEP
  NO_ORACLE_LAYOUT  — wildly different offset + inverted constant (3g); DEFER_DEEP
  SIZE_DIVERGENCE   — li r3, 0xNNN feeding operator new (3h); DEFER_DEEP
  MEMBER_DELTA      — clean uniform this-relative offset delta; MEMBER_DELTA_CANDIDATE
  UNKNOWN           — unclassified residue; needs human/agent

Routes:
  PERMUTE               — FPR_SCHED; hand off to /permute skill
  DEFER_VBASE           — VBASE_WALL; virtual-base layout artifact, multi-TU
  AT_LIMIT              — BOOL_NEG or SIGNEDNESS; no known fix
  DEFER_DEEP            — UNVERIFIABLE or NO_ORACLE_LAYOUT or SIZE_DIVERGENCE
  MEMBER_DELTA_CANDIDATE — clean member offset delta; one-line header fix
  INLINE_POLICY         — bl vs inlined body
  UNKNOWN               — unclassified

Usage:
  python3 tools/wall_classify.py                          # process ~/tmp/hasreal_worklist.json
  python3 tools/wall_classify.py --worklist /path/to.json
  python3 tools/wall_classify.py --validate               # accuracy on pilot's 12 targets
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

# VBASE: vtable-slot load pattern: lwz rX, 0xNN(rY) where rY was loaded from vtable
# Uniform +4 slot delta in vtable calls
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


def _toi(s: str) -> Optional[int]:
    s = s.strip()
    try:
        return int(s, 16) if s.lower().startswith(('0x', '-0x')) else int(s)
    except Exception:
        return None


# ── objdiff diff extraction ───────────────────────────────────────────────────

def diff_fn(unit: str, sym: str, proj: str = ROOT) -> Optional[dict]:
    """Run objdiff for one symbol; return parsed JSON or None."""
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
                    return d
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

def demangle_class(sym: str, unit: str = '') -> str:
    """Extract class name from MSVC mangled symbol."""
    m = CLASS_RE.match(sym)
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
    is_free_fn = not sym.startswith('?') or '@' not in sym[1:]
    # More precise: free functions don't have a class name between @ delimiters
    # ?Foo@Bar@@... → class=Bar; ?Foo@@... → free function
    free_fn_re = re.compile(r'^\?[^@]+@@')  # ?name@@ = free (no class between @@)
    is_free_fn = bool(free_fn_re.match(sym))

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

    # Look for subi r29/r28, r3, N (base-subobject adjustor)
    for ins in insns:
        t = ins.get('target') or {}
        top = t.get('opcode', '')
        ta = (t.get('args') or '').strip()
        if top in ('subi', 'addi') and ta:
            tt = _tokens(ta)
            if len(tt) >= 2 and tt[1] == 'r3':
                # This is a base-subobject adjust — strong vbase signal
                # Only if it's a class method (not a free function parameter pass)
                if not is_free_fn:
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


def _detect_member_delta(insns: List[dict], unit: str, sym: str) -> Tuple[bool, List[str], int, int]:
    """MEMBER_DELTA: clean uniform this-relative offset delta.
    Returns (is_member_delta, evidence, dominant_delta, threshold).

    Uses the same this-tracing logic as member_delta_finder.py:
    - r3 is `this` on entry; mr propagates; bl kills r3..r12
    - look for struct-offset diffs that are uniform across the function
    - require the delta to be non-zero, consistent, and THIS-relative (not r1)
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

    # Collect (target_offset, base_offset, base_reg, access_type) tuples
    struct_accesses = []  # (t_off, b_off, base_reg, op)

    # Reset this tracking
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
    if diff_insns is None:
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

    # MEMBER_DELTA (runs extra diff_fn call — skip if already wall-classified)
    is_member, member_ev, delta, threshold = _detect_member_delta(diff_insns, unit, sym)
    if is_member and 'VBASE_WALL' not in classes:
        classes.append('MEMBER_DELTA')
        all_evidence['MEMBER_DELTA'] = {
            'evidence': member_ev,
            'delta': delta,
            'threshold': threshold,
        }

    # STRUCT_OFF fallback: if struct-offset diffs exist and the only OTHER classes
    # are ANON_FN or NAMED_MISMATCH (not real blockers for layout), also classify
    # as MEMBER_DELTA. Catches funclet/anonymous fn cases where this-tracing fails.
    # Condition: at least 1 struct-offset diff AND no hard blockers (VBASE/BOOL/etc.)
    non_struct_classes = set(classes) - {'VBASE_WALL', 'BOOL_NEG', 'SIGNEDNESS',
                                          'FPR_SCHED', 'SIZE_DIVERGENCE',
                                          'NO_ORACLE_LAYOUT', 'UNVERIFIABLE'}
    already_has_member = 'MEMBER_DELTA' in classes

    if not already_has_member and not is_member:
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
                if len(tt) >= 2 and len(bt) >= 2:
                    tv = _toi(tt[1])
                    bv = _toi(bt[1])
                    if tv is not None and bv is not None and tv != bv:
                        struct_count += 1
        # Allow mixed cases too: struct + anon/named-mismatch callee diffs
        # (the struct offset is still fixable; the callee naming is secondary)
        has_hard_non_struct = any(
            cls in classes for cls in ('VBASE_WALL', 'BOOL_NEG', 'SIGNEDNESS',
                                        'FPR_SCHED', 'SIZE_DIVERGENCE',
                                        'NO_ORACLE_LAYOUT', 'UNVERIFIABLE')
        )
        if total_diff > 0 and struct_count >= 1 and not has_hard_non_struct:
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
                    if len(tt) >= 2 and len(bt) >= 2:
                        tv = _toi(tt[1])
                        bv = _toi(bt[1])
                        if tv is not None and bv is not None:
                            offsets.append((tv, bv))
            if offsets:
                deltas_fb = [t - b for t, b in offsets]
                fb_counter = Counter(deltas_fb)
                fb_dom_delta, fb_dom_count = fb_counter.most_common(1)[0]
                if fb_dom_count == len(deltas_fb) and fb_dom_delta != 0:
                    fb_thresh = min(t for t, _ in offsets)
                    classes.append('MEMBER_DELTA')
                    all_evidence['MEMBER_DELTA'] = {
                        'evidence': [
                            f'fallback: uniform delta={fb_dom_delta:+d} on {len(offsets)} struct-offset diff(s)',
                            f'threshold ~0x{fb_thresh:x}',
                        ],
                        'delta': fb_dom_delta,
                        'threshold': fb_thresh,
                        'confidence': 'low',
                    }
                    delta = fb_dom_delta
                    threshold = fb_thresh

    # ── Route assignment ──────────────────────────────────────────────────────
    # Priority: VBASE > BOOL_NEG/SIGNEDNESS > FPR_SCHED > INLINE_POLICY
    #           > MEMBER_DELTA > SIZE/NO_ORACLE/UNVERIFIABLE > UNKNOWN
    route = 'UNKNOWN'
    confidence = 'medium'

    # VBASE routing: only hard-route to DEFER_VBASE if instruction diffs show
    # vtable-slot deltas OR if the ONLY other class is nothing (pure VBASE).
    # If VBASE was detected only from metadata (COFF/header) but another actionable
    # class is present in the diffs, let the other class route it.
    vbase_dominates = ('VBASE_WALL' in classes and
                       (vbase_from_insn or
                        set(classes) <= {'VBASE_WALL'} or
                        set(classes) - {'VBASE_WALL'} <= {'MEMBER_DELTA'}))

    if vbase_dominates:
        route = 'DEFER_VBASE'
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


# ── Validation against pilot targets ──────────────────────────────────────────

PILOT_TARGETS = [
    {
        'label': 'DxRnd::Present',
        'unit': 'default/Rnd_Xbox',
        'sym': '?Present@DxRnd@@QAAXXZ',
        'expected_class': 'BOOL_NEG',
        'expected_route': 'AT_LIMIT',
    },
    {
        'label': 'VocalTrackDir::TrackReset',
        'unit': 'default/VocalTrackDir',
        'sym': '?TrackReset@VocalTrackDir@@UAAXXZ',
        'expected_class': 'VBASE_WALL',
        'expected_route': 'DEFER_VBASE',
    },
    {
        'label': 'Player::SetMultiplierActive',
        'unit': 'default/band3/game/Player',
        'sym': '?SetMultiplierActive@Player@@UAAX_N@Z',
        'expected_class': 'VBASE_WALL',
        'expected_route': 'DEFER_VBASE',
    },
    {
        'label': 'String::operator= (strcpy signedness)',
        'unit': 'default/Str',
        'sym': '??4String@@QAAAAV0@PBD@Z',
        'expected_class': 'SIGNEDNESS',
        'expected_route': 'AT_LIMIT',
    },
    {
        'label': 'TransformNormal (FPR_SCHED)',
        'unit': 'default/Mesh',
        'sym': '?TransformNormal@@YA?AVVector3@@ABV1@ABVMatrix3@Hmx@@@Z',
        'expected_class': 'FPR_SCHED',
        'expected_route': 'PERMUTE',
    },
    {
        'label': 'Rot::MakeScale (FPR_SCHED)',
        'unit': 'default/Rot',
        'sym': '?MakeScale@@YAXABVMatrix3@Hmx@@AAVVector3@@@Z',
        'expected_class': 'FPR_SCHED',
        'expected_route': 'PERMUTE',
    },
    {
        'label': 'Geo::Intersect (Segment/Triangle, FPR_SCHED)',
        'unit': 'default/Geo',
        'sym': '?Intersect@@YA_NABVSegment@@ABVTriangle@@_NAAM@Z',
        'expected_class': 'FPR_SCHED',
        'expected_route': 'PERMUTE',
    },
    {
        'label': 'RndGroup::SetFrame (INLINE_POLICY)',
        'unit': 'default/Group',
        'sym': '?SetFrame@RndGroup@@UAAXMM@Z',
        'expected_class': 'INLINE_POLICY',
        'expected_route': 'INLINE_POLICY',
    },
    {
        'label': 'BlockMgr::ReadError (UNVERIFIABLE)',
        'unit': 'default/BlockMgr',
        'sym': '?ReadError@?A0xd12e7047@@YAHXZ',
        'expected_class': 'UNVERIFIABLE',
        'expected_route': 'DEFER_DEEP',
    },
    {
        'label': 'DataWriteFile (SIZE_DIVERGENCE)',
        'unit': 'default/DataFile',
        'sym': '?DataWriteFile@@YAXPBDPBVDataArray@@H@Z',
        'expected_class': 'SIZE_DIVERGENCE',
        'expected_route': 'DEFER_DEEP',
    },
    {
        'label': 'MicNull::MicNull (SIGNEDNESS extsh)',
        'unit': 'default/MicNull',
        'sym': '??0MicNull@@QAA@XZ',
        'expected_class': 'SIGNEDNESS',
        'expected_route': 'AT_LIMIT',
    },
]


def run_validation(proj: str = ROOT) -> None:
    """Run classifier against the 11 known pilot targets and report accuracy."""
    print("\n=== Validation against pilot's targets ===\n")
    correct = 0
    total = len(PILOT_TARGETS)
    rows = []

    for pt in PILOT_TARGETS:
        label = pt['label']
        unit = pt['unit']
        sym = pt['sym']
        exp_class = pt['expected_class']
        exp_route = pt['expected_route']

        print(f"  [{label}] ...", file=sys.stderr)
        diff_insns = get_diff_instructions(unit, sym, proj)
        if not diff_insns:
            # Try to get any diff at all
            result = classify_fn(unit, sym, None, proj)
        else:
            result = classify_fn(unit, sym, diff_insns, proj)

        got_classes = result['classes']
        got_route = result['route']
        class_ok = exp_class in got_classes
        route_ok = got_route == exp_route
        ok = class_ok and route_ok
        if ok:
            correct += 1

        status = 'PASS' if ok else ('PARTIAL' if (class_ok or route_ok) else 'FAIL')
        rows.append({
            'label': label,
            'status': status,
            'expected_class': exp_class,
            'got_classes': got_classes,
            'expected_route': exp_route,
            'got_route': got_route,
            'evidence': result.get('evidence', {}),
        })

    # Print table
    print(f"\n{'Label':<45} {'Status':<8} {'Expected':<18} {'Got class':<35} {'Route'}")
    print('-' * 130)
    for r in rows:
        got_cls_str = ','.join(r['got_classes'])[:32]
        print(f"{r['label']:<45} {r['status']:<8} {r['expected_class']:<18} "
              f"{got_cls_str:<35} {r['got_route']} (exp:{r['expected_route']})")

    print(f"\nAccuracy: {correct}/{total} = {correct/total*100:.1f}%")
    if correct >= 9:
        print("PASS (≥9/12 target)")
    else:
        print("FAIL (< 9/12 target)")

    # Show evidence for failures
    failures = [r for r in rows if r['status'] != 'PASS']
    if failures:
        print("\nFailure details:")
        for r in failures:
            print(f"\n  [{r['label']}]")
            print(f"    expected: class={r['expected_class']}, route={r['expected_route']}")
            print(f"    got:      class={r['got_classes']}, route={r['got_route']}")
            for cls, ev in r['evidence'].items():
                print(f"    {cls}: {ev}")

    return rows, correct


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
        print(f"  {route:<28} {n:>6}  {pct:>4.1f}%")
    print()
    print(f"Class breakdown (functions can have multiple):")
    for cls in ['VBASE_WALL', 'BOOL_NEG', 'SIGNEDNESS', 'FPR_SCHED', 'INLINE_POLICY',
                'UNVERIFIABLE', 'NO_ORACLE_LAYOUT', 'SIZE_DIVERGENCE', 'MEMBER_DELTA', 'UNKNOWN']:
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
                    help='Run validation against pilot\'s 12 targets and exit')
    ap.add_argument('--sym', default='',
                    help='Classify a single symbol (requires --unit)')
    ap.add_argument('--unit', default='',
                    help='Unit for --sym')
    ap.add_argument('--proj', default=ROOT,
                    help='Project root (default: auto-detected)')
    ap.add_argument('--verbose', action='store_true',
                    help='Print per-function classification as it runs')
    a = ap.parse_args()

    if a.validate:
        run_validation(a.proj)
        return

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
