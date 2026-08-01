#!/usr/bin/env python3
"""PART 3 (lane CH-4): a STRING-FREE discriminator for StaticClassName rows.

THE PROBLEM (lane CG-2): `?StaticClassName@C@@SA?AVSymbol@@XZ` rows were
adjudicated by reading which literal the retail body references. That cannot
separate platform variants -- RndTex / DxTex / NgTex all register the Milo type
name "Tex", so the string is shared and the reader FALSELY CONFIRMS whichever
name the map happens to carry. 32 strings are shared by >1 VA.
CG-2's region-coherence detector was meant to settle it and FAILED ITS OWN
CONTROL (36 surviving anchors, all Rnd-prefixed, so it emitted one label for
all 453 VAs); it was correctly discarded.

THE INSTRUMENT HERE: `?ClassName@C@@UBA?AVSymbol@@XZ` is VIRTUAL, so it sits in
C's vtable, and it returns StaticClassName(). So the caller relation
    vtable(C) -> slot body -> bl/b -> StaticClassName VA
attributes a StaticClassName address to a class WITHOUT EVER READING A STRING.
It is independent of CG-2's instrument #1 (retail string content) and of
instrument #3 (address-region coherence), because it reads the .rdata vtable
layout plus .text branch structure.

⚠ It is NOT independent of the RTTI chain -- it is built on the same vtables.
So it cannot corroborate an RTTI-derived claim; it only answers the class
question the string cannot.

CALIBRATION IS MANDATORY AND IS THE POINT: the instrument is first run on the
UNAMBIGUOUS population (strings used by exactly one VA), where the answer is
already known, to measure its accuracy and its silence rate. Only then is it
pointed at the 32.
"""
import sys, os, json, re, collections, random

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', 'maprow_dtor'))
from retail_reader import Image, insns, branch_target            # noqa
from rtti_vtable_index import pdata_sizes, demangle_class        # noqa
from vtable_attr import Attr, dethunk                            # noqa


def build_referrers(img, A, sizes):
    """target VA -> {class -> [(vtable, slot)]}, over every vtable slot body.
    Reads each slot's real (dethunked) body and records its branch targets."""
    ref = collections.defaultdict(lambda: collections.defaultdict(list))
    seen_body = {}
    for vt, (nm, off, sl) in A.r.vt_slots.items():
        c = demangle_class(nm)
        for i, raw in enumerate(sl):
            fn = A.slot_deth.get(raw, raw)
            tg = seen_body.get(fn)
            if tg is None:
                sz = sizes.get(fn)
                tg = set()
                if sz and sz <= 4096:
                    for a, ins in insns(img, fn, sz):
                        t = branch_target(a, ins)
                        if t:
                            tg.add(t[0])
                seen_body[fn] = tg
            for t in tg:
                ref[t][c].append((vt, i))
    return ref


def main():
    img = Image()
    sizes = pdata_sizes(img)
    A = Attr(img)
    ref = build_referrers(img, A, sizes)
    print(f'referrer index: {len(ref)} branch targets reachable from vtable slots')

    root = os.path.join(HERE, '..', '..')
    raw = json.load(open(os.path.join(root, 'scripts/target_symbol_map.json')))
    M = {k: v for k, v in raw.items() if k.startswith('0x') and isinstance(v, str)}
    inv = {int(k, 16): v for k, v in
           json.load(open('/home/free/tmp/cg2_inventory.json')).items()}

    SCN = re.compile(r'^\?StaticClassName@([A-Za-z0-9_]+)@@SA\?AVSymbol@@XZ')
    rows = {int(a, 16): SCN.match(nm).group(1) for a, nm in M.items() if SCN.match(nm)}
    bystr = collections.defaultdict(list)
    for va, s in inv.items():
        bystr[s].append(va)

    def attribute(va):
        return set(ref.get(va, {}))

    # ---------------- CALIBRATION on the UNAMBIGUOUS population -------------
    unamb = [(va, c) for va, c in rows.items()
             if va in inv and len(bystr[inv[va]]) == 1]
    hit = miss = silent = 0
    misses = []
    for va, c in unamb:
        cs = attribute(va)
        if not cs:
            silent += 1
        elif c in cs:
            hit += 1
        else:
            miss += 1
            if len(misses) < 8:
                misses.append((hex(va), c, sorted(cs)[:4]))
    spoke = hit + miss
    print(f'\n=== CALIBRATION: unambiguous StaticClassName rows (n={len(unamb)}) ===')
    print(f'   instrument SILENT (no vtable slot calls it) : {silent} '
          f'({100*silent/len(unamb):.1f}%)')
    print(f'   spoke: names the MAP class                  : {hit}/{spoke} '
          f'= {100*hit/spoke if spoke else 0:.1f}%')
    print(f'   spoke: names a DIFFERENT class              : {miss}/{spoke}')
    for m in misses:
        print(f'      miss {m}')

    # NULL: shuffle which address we ask about. Accuracy must collapse.
    random.seed(11)
    addrs = [va for va, _ in unamb]
    sh = addrs[:]
    random.shuffle(sh)
    nh = ns = 0
    for i, (va, c) in enumerate(unamb):
        cs = attribute(sh[i])
        if not cs:
            ns += 1
        elif c in cs:
            nh += 1
    print(f'   NULL (address shuffled): names the map class {nh}/'
          f'{len(unamb)-ns} spoke')

    # ---------------- APPLY to the ambiguous 32 ----------------------------
    amb = sorted({va for s, vs in bystr.items() if len(vs) > 1
                  for va in vs if va in rows})
    print(f'\n=== THE AMBIGUOUS POPULATION: {len(amb)} VAs whose string is shared '
          f'({len({inv[v] for v in amb})} distinct strings) ===')
    out = []
    agree = contra = sil = 0
    for va in amb:
        c = rows[va]
        cs = sorted(attribute(va))
        if not cs:
            v = 'SILENT'
            sil += 1
        elif c in cs:
            v = 'CONFIRMS_MAP'
            agree += 1
        else:
            v = 'NAMES_OTHER'
            contra += 1
        out.append(dict(addr=hex(va), map_cls=c, string=inv.get(va),
                        verdict=v, retail_callers=cs))
        print(f'  {v:12s} {hex(va)} str={inv.get(va)!r:14s} map={c:22s} '
              f'callers={cs[:4]}')
    print(f'\n  CONFIRMS_MAP {agree}   NAMES_OTHER {contra}   SILENT {sil}')
    json.dump(out, open('/home/free/tmp/laneCH4/part3_scn.json', 'w'), indent=1)


if __name__ == '__main__':
    main()
