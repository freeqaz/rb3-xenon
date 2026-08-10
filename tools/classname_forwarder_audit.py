#!/usr/bin/env python3
"""classname_forwarder_audit -- find map entries that score on a FALSE PAIRING
because retail's function there is a ClassName()->StaticClassName() forwarder.

Provenance: lane MAP-B (2026-08-10), from lane MATCH-L's incidental finding of 14.
Read-only unless --write-map is passed; not a build input.

THE HAZARD
----------
`OBJ_CLASSNAME(x)` emits `virtual Symbol ClassName() const { return
StaticClassName(); }`.  On X360 that is always the SAME 12 words -- Symbol is
returned by value, so r3 is the sret pointer and the body is::

    mflr r12 / stw r12,-8(r1) / std r31,-0x10(r1) / stwu r1,-0x60(r1)
    mr r31,r3 / bl ?StaticClassName@<Class>@@ / mr r3,r31
    addi r1,r1,0x60 / lwz r12,-8(r1) / mtlr r12 / ld r31,-0x10(r1) / blr

11 of those 12 words are IDENTICAL for every class.  The only differing word is
the `bl` displacement -- and the graded ruler runs functionRelocDiffs=none, which
masks exactly that.  So all 337 of these functions are ONE reloc-masked
equivalence class: any bijection of their names over their addresses scores
100 on every pair.  A wrong name still scores.  This is the documented
"metric is blind to attribution" hazard, and the map had 64 of them wrong.

THE INSTRUMENT: resolve the `bl` target -- the one word the ruler masks.

WITNESSES, all read from retail bytes (band.exe), none from the map:
  W1  body shape (necessary, NOT sufficient -- see the trap below)
  W2  the bl target has the StaticClassName shape: a guard-protected
      function-local `static Symbol` built from a string literal
  W3  that string literal == the OBJ_CLASSNAME(x) argument
  W4  RTTI: ClassName() is virtual, so it sits in its own class's vtable

★ TRAP 1 -- W1 ALONE PRODUCES FALSE POSITIVES.  ANY sret-returning function that
forwards to a single call has these exact 12 words: `?Filename@File@@UBA?AVString@@XZ`,
`??0MidiVarLenNumber@@QAA@AAVBinStream@@@Z`, `?FirstChar@NodeSort@@SA?AVSymbol@@PBD_N@Z`
and 2 more are shape-matches whose bl target is NOT a StaticClassName.  Judging on
shape would have "corrected" 5 CORRECT entries.  Rows whose bl target fails W2 are
reported NOT_JUDGED, never rewritten.

★ TRAP 2 -- A SHARED STRING IS NOT EVIDENCE.  Rnd*/Dx*/Ng* families share the
OBJ_CLASSNAME argument ("Mat" is RndMat AND DxMat AND NgMat), so
"string == OBJ_CLASSNAME(mapped class)" cannot confirm a name.  Judging that way
blessed ?ClassName@DxMesh@@ at an address in the rndobj band -- which would also
have put that one name at TWO addresses.  The true class is therefore derived
INDEPENDENTLY for every row and then compared; consistency is an OUTPUT.

★ TRAP 3 -- THE SPLITS PIN CAN BE CIRCULAR.  A 48-byte single-function .text block
containing only a member of this class was itself filled BY BYTE SIGNATURE over the
same masked class (see the map's own _splits_fill_unresolved_comment).  Using such a
pin to disambiguate DxMesh vs RndMesh is circular, so sliver pins are refused.

★ TRAP 4 -- ?ForceEmit_<C>_StaticClassName@@ is OUR OWN scaffolding (present in
src/, absent from dc3 and rb3-Wii) and compiles to the identical forwarder.  It is
parsed for its implied callee class like any other name, not dismissed.
"""
from __future__ import annotations

import argparse
import bisect
import collections
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from tools.retail_rtti import RetailRtti  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAP = os.path.join(ROOT, 'scripts/target_symbol_map.json')
SPLITS = os.path.join(ROOT, 'config/45410914/splits.txt')
REPORT = os.path.join(ROOT, 'build/45410914/report.json')
ORACLES = [ROOT, '/home/free/code/milohax/dc3-decomp', '/home/free/code/milohax/rb3']

FWD = [0x7d8802a6, 0x9181fff8, 0xfbe1fff0, 0x9421ffa0, 0x7c7f1b78, None,
       0x7fe3fb78, 0x38210060, 0x8181fff8, 0x7d8803a6, 0xebe1fff0, 0x4e800020]
CN = re.compile(r'^\?ClassName@([\w@$?]+?)@@UBA\?AVSymbol@@XZ$')
FE = re.compile(r'^\?ForceEmit_(\w+)_StaticClassName@@YA\?AVSymbol@@XZ$')
CLS_RX = re.compile(r'\b(?:class|struct)\s+([A-Za-z_]\w*)\s*(?::[^;{]*)?\{')
NS_RX = re.compile(r'\bnamespace\s+([A-Za-z_]\w*)\s*\{')
MAC_RX = re.compile(r'\bOBJ_CLASSNAME\s*\(\s*([^)\s]+)\s*\)')


def scope(s):
    """mangled scope 'Object@Hmx' -> 'Hmx::Object'."""
    return '::'.join(reversed(s.split('@')))


def mangle(cls):
    return '?ClassName@' + '@'.join(reversed(cls.split('::'))) + '@@UBA?AVSymbol@@XZ'


def bl_target(a, w):
    if (w >> 26) != 18 or not (w & 1) or (w & 2):
        return None
    li = w & 0x03FFFFFC
    if li & 0x02000000:
        li -= 0x04000000
    return (a + li) & 0xFFFFFFFF


# ----------------------------------------------------------- source table ----
def _scan_file(path):
    try:
        txt = open(path, encoding='utf-8', errors='replace').read()
    except OSError:
        return []
    txt = re.sub(r'//[^\n]*', '', txt)
    txt = re.sub(r'/\*.*?\*/', '', txt, flags=re.S)
    out, stack, i = [], [], 0
    while i < len(txt):
        c = txt[i]
        if c == '{':
            head = txt[max(0, i - 300):i + 1]
            mc = None
            for rx, kind in ((CLS_RX, 'c'), (NS_RX, 'n')):
                for mm in rx.finditer(head):
                    if mm.end() == len(head):
                        mc = (kind, mm.group(1))
            stack.append(mc)
        elif c == '}':
            if stack:
                stack.pop()
        elif c == 'O' and txt.startswith('OBJ_CLASSNAME', i):
            mm = MAC_RX.match(txt, i)
            if mm:
                cs = [x[1] for x in stack if x and x[0] == 'c']
                ns = [x[1] for x in stack if x and x[0] == 'n']
                if cs:
                    out.append(('::'.join(ns + cs), mm.group(1), path))
                i = mm.end()
                continue
        i += 1
    return out


def build_class_table():
    """qualified class -> {OBJ_CLASSNAME string -> [defining files]}"""
    tab = {}
    for root in ORACLES:
        src = os.path.join(root, 'src')
        if not os.path.isdir(src):
            continue
        for dp, _, fns in os.walk(src):
            for fn in fns:
                if fn.endswith(('.h', '.hpp', '.cpp', '.inl')):
                    for q, s, p in _scan_file(os.path.join(dp, fn)):
                        rel = p[len(root) + 1:] if p.startswith(root) else p
                        tab.setdefault(q, {}).setdefault(s, []).append(rel)
    return tab


# ------------------------------------------------------------------ audit ----
class Audit:
    def __init__(self):
        self.R = RetailRtti()
        self.ext = self.R.extents
        self.map = json.load(open(MAP))
        self.ctab = build_class_table()
        self.strtab = collections.defaultdict(set)
        for c, d in self.ctab.items():
            for s in d:
                self.strtab[s].add(c)
        self._td = {}
        self._load_splits()

    # -- retail witnesses -------------------------------------------------
    def has_td(self, cls):
        if cls not in self._td:
            m = '.?AV' + '@'.join(reversed(cls.split('::'))) + '@@'
            self._td[cls] = bool(self.R.find_type_descriptors(m))
        return self._td[cls]

    def static_classname_string(self, t):
        """W2+W3: is `t` a StaticClassName() body?  -> its string literal."""
        n = self.ext.get(t)
        if n is None or not (48 <= n <= 200):
            return None
        words = [self.R.u32(t + i) for i in range(0, n, 4)]
        if any(w is None for w in words):
            return None
        if not any(w == 0x556907FF or (w >> 26) == 21 and (w & 1) and (w & 0x7FE) == 0x7FE
                   for w in words):
            return None                       # no static-init guard test
        regs, r4 = {}, None
        for w in words:
            op, rD, rA, imm = w >> 26, (w >> 21) & 31, (w >> 16) & 31, w & 0xFFFF
            if op == 15:
                regs[rD] = (imm << 16) if rA == 0 else \
                    ((regs[rA] + (imm << 16)) & 0xFFFFFFFF if rA in regs else regs.get(rD))
            elif op == 14:
                v = imm - 0x10000 if imm & 0x8000 else imm
                if rA in regs:
                    regs[rD] = (regs[rA] + v) & 0xFFFFFFFF
                    if rD == 4 and r4 is None:
                        r4 = regs[4]
        if r4 is None:
            return None
        s = self.R.cstr(r4, limit=64)
        if not s or not (1 <= len(s) <= 48) or not all(32 <= ord(c) < 127 for c in s):
            return None
        return s

    # -- unit attribution --------------------------------------------------
    def _load_splits(self):
        self.splits, cur = [], None
        for line in open(SPLITS):
            m = re.match(r'^(\S+):\s*$', line)
            if m:
                cur = m.group(1)
                continue
            m = re.match(r'\s+\.text\s+start:(0x[0-9a-fA-F]+)\s+end:(0x[0-9a-fA-F]+)', line)
            if m and cur:
                self.splits.append((int(m.group(1), 16), int(m.group(2), 16), cur))
        self.splits.sort()
        self._starts = [x[0] for x in self.splits]
        self.sp_by_unit = {}
        if os.path.exists(REPORT):
            for u in json.load(open(REPORT))['units']:
                sp = (u.get('metadata') or {}).get('source_path') or ''
                if sp.startswith('src/'):
                    self.sp_by_unit.setdefault(sp[4:], sp)

    def block(self, a):
        i = bisect.bisect_left(self._starts, a + 1) - 1
        if i >= 0 and self.splits[i][0] <= a < self.splits[i][1]:
            return self.splits[i]
        return None

    def unit_source(self, a):
        b = self.block(a)
        if not b:
            return '', False
        key = b[2]
        sp = self.sp_by_unit.get(key)
        if sp is None:
            c = [v for k, v in self.sp_by_unit.items() if k.endswith('/' + key)]
            sp = c[0] if len(c) == 1 else ''
        # TRAP 3: a block holding only this 48-byte forwarder is not evidence
        return sp, (b[1] - b[0]) <= 64

    # -- main --------------------------------------------------------------
    def run(self):
        mp_addr = {k.lower(): v for k, v in self.map.items()
                   if k.lower().startswith('0x')}
        rows, notjudged = [], []
        for a, n in sorted(self.ext.items()):
            if n != 48:
                continue
            words = [self.R.u32(a + i) for i in range(0, 48, 4)]
            if any(w is None for w in words):
                continue
            if any(e is not None and words[i] != e for i, e in enumerate(FWD)):
                continue
            t = bl_target(a + 0x14, words[5])
            name = mp_addr.get('0x%08x' % a)
            s = self.static_classname_string(t) if t else None
            if not s:
                notjudged.append((a, name))      # TRAP 1
                continue
            vt = []
            for v in self.R.owning_vtables(a):
                v0 = v[0]
                vt.append(scope(v0[4:].rstrip('@') if v0.startswith(('.?AV', '.?AU'))
                                else v0.rstrip('@')))
            sp, sliver = self.unit_source(a)
            true_cls, how = self._resolve(a, s, vt, sp, sliver)
            implied = None
            if name:
                m, f = CN.match(name), FE.match(name)
                implied = scope(m.group(1)) if m else (f.group(1) if f else None)
            if name is None:
                verdict = 'UNMAPPED'
            elif true_cls is None:
                verdict = 'UNRESOLVED'
            elif implied is None or implied != true_cls:
                verdict = 'MISIDENTIFIED'
            else:
                verdict = 'CONSISTENT'
            rows.append(dict(addr=a, key='0x%08x' % a, map_name=name, string=s,
                             implied=implied, true_cls=true_cls, how=how,
                             verdict=verdict, vt=vt, source_path=sp, sliver=sliver,
                             new=mangle(true_cls) if true_cls else None))
        return rows, notjudged

    # R5 -- rows the automatic witnesses cannot reach, adjudicated by ELIMINATION
    # under the map's own NAME-INJECTIVITY invariant plus the address-band control.
    # The control: over 46 Rnd* and 10 Dx* rows established independently by R1/R2/R3,
    # the Rnd band [822737a0,824a8990] and the Dx band [82734848,827410b8] are DISJOINT
    # -- zero crossings.  Each of these three also sits on a SLIVER pin, so R3 is
    # refused for them and this table is the only route.
    R5 = {
        # only 2 'Mesh' forwarders exist and 82738370 is DxMesh (non-sliver rnddx9
        # pin); a mangled name resolves to ONE definition => this one is RndMesh.
        # Corroborated: it is in the Rnd band.
        0x8241c560: ('RndMesh', 'R5 elimination (DxMesh @82738370) + Rnd band'),
        # 3 'Light' forwarders: 82b8aa10=NgLight (established via Lit_NG.cpp),
        # 82498a48 unmapped in the Rnd band => RndLight, and this one sits INSIDE the
        # Dx band, between DxMultiMesh (8273f828) and DxParticleSys (82740558).
        0x8273ff80: ('DxLight', 'R5 Dx band + elimination (NgLight @82b8aa10)'),
        # 'CharIKHand' has ONE forwarder.  The rival CharIKFoot comes only from
        # rb3-Wii, whose CharIKFoot carries OBJ_CLASSNAME(CharIKHand) -- a copy-paste
        # bug in THAT tree (xenon and dc3 both say CharIKFoot).  Retail settles it:
        # CharIKFoot has its OWN forwarder at 0x823bf5e8 with string 'CharIKFoot'.
        0x82397f08: ('CharIKHand', 'R5 retail has a separate CharIKFoot fwd @823bf5e8'),
    }

    def _resolve(self, a, s, vt, sp, sliver):
        if a in self.R5:
            return self.R5[a]
        cand = [c for c in vt if s in self.ctab.get(c, {})]
        if len(cand) == 1:
            return cand[0], 'R1 RTTI-vtable+string'
        cs = [c for c in sorted(self.strtab.get(s, ())) if self.has_td(c)]
        if len(cs) == 1:
            return cs[0], 'R2 unique TD-backed class'
        if len(cs) > 1 and sp and not sliver:       # TRAP 2 + TRAP 3
            ustem = os.path.splitext(os.path.basename(sp))[0].lower()
            udir = os.path.dirname(sp).lower()
            files = {c: self.ctab[c][s] for c in cs}
            by_stem = [c for c in cs if any(
                os.path.splitext(os.path.basename(f))[0].lower() == ustem for f in files[c])]
            if len(by_stem) == 1:
                return by_stem[0], f'R3a defining header stem == unit stem ({ustem})'
            by_dir = [c for c in cs if any(
                os.path.dirname(f).lower() == udir for f in files[c])]
            if len(by_dir) == 1:
                return by_dir[0], f'R3b defining header in unit dir ({udir})'
        return None, f'R4 UNRESOLVED {cs or repr(s)}' + (' (SLIVER pin)' if sliver else '')


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--json', help='write full rows here')
    ap.add_argument('--show', default='MISIDENTIFIED,UNRESOLVED',
                    help='comma-separated verdicts to print')
    args = ap.parse_args()

    au = Audit()
    rows, notjudged = au.run()
    c = collections.Counter(r['verdict'] for r in rows)
    print(f'ClassName forwarders in retail (W1+W2 confirmed): {len(rows)}')
    print(f'  shape-matches REFUSED because the bl target is not a StaticClassName '
          f'(TRAP 1): {len(notjudged)}')
    for a, nm in notjudged:
        print(f'      {a:08x}  {nm}')
    print(f'  {dict(c)}')
    sliv = sum(1 for r in rows if r['sliver'])
    print(f'  rows on a SLIVER .text pin (unit evidence refused, TRAP 3): {sliv}')
    show = set(args.show.split(','))
    for r in sorted(rows, key=lambda x: x['addr']):
        if r['verdict'] in show:
            print('%08x [%-13s] str=%-24s TRUE=%-24s\n     map=%s\n     %s'
                  % (r['addr'], r['verdict'], r['string'], r['true_cls'] or '???',
                     (r['map_name'] or '(unmapped)')[:100], r['how']))
    if args.json:
        json.dump(rows, open(args.json, 'w'), indent=1)
        print(f'wrote {args.json}')


if __name__ == '__main__':
    main()
