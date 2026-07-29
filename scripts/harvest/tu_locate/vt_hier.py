"""Parse MSVC ??_R3 ClassHierarchyDescriptor / ??_R2 BaseClassArray /
??_R1 BaseClassDescriptor to get each class's base chain, then compute
CLASS-OWNED vtable slots = slots that differ from the primary base's vtable
(or extend past its end).
"""
import json, sys, struct
from _paths import SCRATCH, REPO, BANDEXE, WII_SRC  # noqa: E402
from vt_pe import u32, sec_of, is_text

VT = json.load(open(SCRATCH+'/vtables.json'))
TDS = {int(k, 16): v for k, v in json.load(open(SCRATCH+'/typedescs.json')).items()}


def demangle(nm):
    return nm[4:-2]


def primary(cls):
    """primary (offset-0) vtable record for a class, or None"""
    recs = VT.get(cls)
    if not recs:
        return None
    z = [r for r in recs if r['offset'] == 0]
    if not z:
        return None
    # if several (ICF/dupes), take the longest
    return max(z, key=lambda r: len(r['slots']))


_bases_cache = {}


def bases(cls):
    """[(class_name, mdisp, pdisp, numContained)] in R2 order; [0] is cls itself."""
    if cls in _bases_cache:
        return _bases_cache[cls]
    r = primary(cls)
    _bases_cache[cls] = []
    if not r:
        return []
    chd = r['chd']
    n = u32(chd + 8)
    arr = u32(chd + 12)
    if n is None or arr is None or n > 200 or sec_of(arr) not in ('.data', '.rdata'):
        return []
    res = []
    for i in range(n):
        p = u32(arr + 4 * i)
        if p is None or sec_of(p) not in ('.data', '.rdata'):
            break
        td = u32(p)
        nc = u32(p + 4)
        md = u32(p + 8)
        pd = u32(p + 12)
        nm = TDS.get(td)
        if nm is None:
            break
        if md is not None and md >= 0x80000000:
            md -= 1 << 32
        if pd is not None and pd >= 0x80000000:
            pd -= 1 << 32
        res.append((demangle(nm), md, pd, nc))
    _bases_cache[cls] = res
    return res


def primary_base(cls):
    """the immediate base occupying offset 0 with no virtual-base indirection."""
    b = bases(cls)
    if len(b) < 2:
        return None
    # entry[1] is the first base in depth-first order == the primary base
    nm, md, pd, nc = b[1]
    if md != 0 or pd != -1:
        return None
    return nm


def owned_slots(cls):
    """returns (slots, owned_indices, base_name, n_base_slots)"""
    r = primary(cls)
    if not r:
        return None
    slots = r['slots']
    pb = primary_base(cls)
    rb = primary(pb) if pb else None
    if rb is None:
        return (slots, list(range(len(slots))), pb, 0)
    bs = rb['slots']
    own = []
    for i, v in enumerate(slots):
        if i >= len(bs) or bs[i] != v:
            own.append(i)
    return (slots, own, pb, len(bs))


if __name__ == '__main__':
    for c in sys.argv[1:]:
        r = owned_slots(c)
        if not r:
            print(c, 'NO VTABLE')
            continue
        slots, own, pb, nb = r
        print(f'{c}: {len(slots)} slots, base={pb} ({nb} slots), owned={len(own)} -> {own}')
        print('  chain:', ' <- '.join(x[0] for x in bases(c)))
        for i in own:
            print(f'    [{i:3d}] {slots[i]:08X}')
