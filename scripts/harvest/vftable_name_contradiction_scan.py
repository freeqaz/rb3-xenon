#!/usr/bin/env python3
"""Find target_symbol_map entries whose NAME contradicts their BODY (laneAY).

★ THE SEED CASE (measured, 2026-07-27): `scripts/target_symbol_map.json` mapped
`0x82804588` -> `?SetType@Object@Hmx@@UAAXVSymbol@@@Z`. That function cannot be
a SetType: its relocations are `??0Object@Hmx@@QAA@XZ` (a BASE CONSTRUCTOR call),
a store of `vftable_82123B0C`, and `??0Timer@@QAA@XZ`. Reading the `??_R4`
Complete Object Locator immediately before that vftable gives the type descriptor
`.?AVUIManager@@` -- so the function is `??0UIManager@@QAA@XZ`, and the map had a
UI-unit constructor wearing an engine method's name.

THE FINGERPRINT
---------------
A function that STORES a vftable whose RTTI class is C is either
  (a) a constructor/destructor of C (or of a class deriving from C), or
  (b) a function that INLINED such a ctor/dtor -- /O1 /Ob2 does this constantly.
So "stores a foreign vftable" alone is far too noisy. The discriminator that
makes it precise is the pairing state:

  * the map's name for the site names class K, and K != C, AND
  * the target obj contains NO symbol named `??0C@@*` / `??1C@@*` (so this site
    is not merely a caller sitting next to the real ctor), AND
  * our compiled obj for the same unit DOES define a `??0C@@*` / `??1C@@*` whose
    size is within a few bytes of the target site.

That last clause is what turns a suspicion into a repoint: it names the symbol
the site should be paired with and shows the pairing is currently wasted.

⚠ NOT a defect, do not report: adjustor thunks (`$4PPPP...`), `??_G`/`??_E`
vector-deleting destructors, `??_D`/`??_F` (they legitimately carry another
class's vftable), and any site whose own name already IS a ctor/dtor of C.

Usage: python3 scripts/harvest/vftable_name_contradiction_scan.py <worktree>
"""
import argparse, json, os, re, struct, sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                '..', 'analysis'))
from coffx import read_coff, infer_sizes, K_SEC
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import localstatic_patch_gen as G

VFT_RE = re.compile(r'^(?:vftable_|__vt__)([0-9A-Fa-f]{8})$')
CTOR_DTOR_RE = re.compile(r'^\?\?(?:0|1|_[DEFGH])')
THUNK_RE = re.compile(r'\$4[A-Z]+@')


def cls_of(mangled):
    """MSVC `?Meth@Class@Ns@@...` / `??0Class@@...` -> 'Class'."""
    if mangled.startswith('??'):
        rest = mangled[3:] if mangled[2] == '_' else mangled[3:]
        m = re.match(r'^[_A-Za-z0-9?$]*?@', mangled[3:])
        parts = mangled[3:].split('@')
        return parts[0] if parts else None
    if mangled.startswith('?'):
        parts = mangled[1:].split('@')
        return parts[1] if len(parts) > 1 else None
    return None


class Rtti(object):
    def __init__(self, img):
        self.img = img
        self.cache = {}

    def _u32(self, va):
        for base, vsz, raw, _rsz, _nm in self.img.segs:
            if base <= va < base + vsz:
                o = raw + (va - base)
                if o + 4 <= len(self.img.d):
                    return struct.unpack_from('>I', self.img.d, o)[0]
        return None

    def cls(self, vft_va):
        """Class name behind a vftable VA, via its ??_R4 COL."""
        if vft_va in self.cache:
            return self.cache[vft_va]
        out = None
        col = self._u32(vft_va - 4)
        if col:
            td = self._u32(col + 0x0C)
            if td:
                s = self.img.cstr(td + 8)
                if s and s.startswith('.?AV'):
                    out = s[4:].split('@')[0]
        self.cache[vft_va] = out
        return out


def code_syms(path):
    try:
        data = open(path, 'rb').read()
    except OSError:
        return None, None, None
    secs, syms = read_coff(data)
    if secs is None:
        return None, None, None
    infer_sizes(secs, syms)
    return secs, syms, {s.index: s for s in syms}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('worktree')
    ap.add_argument('--json')
    a = ap.parse_args()
    wt = a.worktree
    img = G.Image(os.path.join(wt, 'orig/45410914/band.exe'))
    rtti = Rtti(img)

    pct = {}
    for u in json.load(open(os.path.join(wt,
                                         'build/45410914/report.json')))['units']:
        for f in (u.get('functions') or []):
            pct[(u['name'], f['name'])] = f['match_percent_normalized']

    units = json.load(open(os.path.join(wt, 'objdiff.json')))['units']
    hits = []
    for u in units:
        bp = u.get('base_path')
        if not bp:
            continue
        tsecs, tsyms, tby = code_syms(os.path.join(wt, u['target_path']))
        if tsecs is None:
            continue
        bsecs, bsyms, _ = code_syms(os.path.join(wt, bp))
        if bsecs is None:
            continue
        bsize = {s.name: s.size for s in bsyms
                 if s.sec > 0 and s.size and s.kind != K_SEC and s.cls in (2, 3)
                 and bsecs[s.sec - 1].is_code}
        tnames = {s.name for s in tsyms}
        for s in tsyms:
            if s.sec <= 0 or not s.size or s.kind == K_SEC or s.cls not in (2, 3):
                continue
            sec = tsecs[s.sec - 1]
            if not sec.is_code:
                continue
            n = s.name
            if n.startswith(('fn_', '__unwind', '__catch', '$')) \
               or CTOR_DTOR_RE.match(n) or THUNK_RE.search(n):
                continue
            k = cls_of(n)
            vfts, ctors = set(), set()
            for (va, si, _typ) in sec.relocs:
                if not (s.value <= va < s.value + s.size):
                    continue
                t = tby.get(si)
                if t is None:
                    continue
                m = VFT_RE.match(t.name)
                if m:
                    vfts.add(int(m.group(1), 16))
                elif t.name.startswith('??0'):
                    ctors.add(t.name)
            if not vfts or not ctors:
                continue
            for v in vfts:
                c = rtti.cls(v)
                if not c or c == k:
                    continue
                # the real ctor/dtor of C must NOT already be carved here
                if any(x.startswith(('??0' + c + '@', '??1' + c + '@'))
                       for x in tnames):
                    continue
                cand = [(bn, bz) for bn, bz in bsize.items()
                        if bn.startswith(('??0' + c + '@', '??1' + c + '@'))]
                if not cand:
                    continue
                cand.sort(key=lambda kv: abs(kv[1] - s.size))
                bn, bz = cand[0]
                hits.append({'unit': u['name'], 'mapped_as': n, 'cls_named': k,
                             'cls_vftable': c, 'vftable_va': '0x%08x' % v,
                             'tgt_size': s.size, 'candidate': bn,
                             'cand_size': bz, 'delta': bz - s.size,
                             'pct': pct.get((u['name'], n)),
                             'cand_paired': (u['name'], bn) in pct})

    hits.sort(key=lambda h: (abs(h['delta']), -(h['pct'] or 0)))
    print('%d name/body contradiction(s)\n' % len(hits))
    print('%-26s %-46s %-22s %6s %6s %6s %s'
          % ('UNIT', 'MAPPED AS', 'VFTABLE CLASS', 'TGT', 'CAND', 'D', 'PCT'))
    for h in hits:
        print('%-26s %-46s %-22s %6d %6d %+6d %s%s'
              % (h['unit'][-26:], h['mapped_as'][:46], h['cls_vftable'][:22],
                 h['tgt_size'], h['cand_size'], h['delta'],
                 ('%.2f' % h['pct']) if h['pct'] is not None else '-',
                 '  [cand already paired]' if h['cand_paired'] else ''))
        print('        -> repoint to %s' % h['candidate'])
    if a.json:
        json.dump(hits, open(a.json, 'w'), indent=1)


if __name__ == '__main__':
    main()
