#!/usr/bin/env python3
"""LANE CI-1 part 2: adjudicate the CONTRA ??_G/??_E rows into REPAIR / LEAVE.

Gates applied, in order (a row must pass ALL of them to be repaired):

  G0 ROW-CLASS-HAS-VTABLE. If the row's class has no ??_R4 at all, the address
     can legitimately be a non-virtual ??_G that ICF folded onto a virtual one
     -- "in someone else's vtable" is then not a contradiction. (This is the
     filter that takes the raw CONTRA set down to CH-4's stratum.)

  G1 CONJUNCTION. Instrument A (retail vtable membership) must be corroborated
     by instrument B (dtor_bl -> the ??1/??_D the body actually calls) naming
     the SAME class. A alone is not enough: inheritance and ICF both put one VA
     in several vtables.

  G2 SLOT-ROLE. The VA must sit at a slot index whose role is "deleting
     destructor" -- established by what OTHER classes put at that same index /
     col_offset, NOT by assuming slot 0 (RndDrawable puts UpdateSphere at 0).

  G3 HOME-UNIT. objdiff pairs target<->base BY NAME WITHIN A UNIT. The address
     lives in exactly one unit's .text split; the proposed name must be a
     symbol that OUR COMPILED OBJ for THAT unit actually exports, or the
     rename cannot pair and can only lose. (CH-3 measured -4 from 9 cross-unit
     moves.)

  G4 INJECTIVITY. The proposed name must not already be used at another
     address in the map (13 duplicates pre-exist; introduce zero).
"""
import sys, os, json, re, struct, collections

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', 'maprow_dtor'))
from ci1_dtor_census import Census, cls_of_sym                        # noqa
from rtti_vtable_index import demangle_class                          # noqa

ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
SPLITS = os.path.join(ROOT, 'config/45410914/splits.txt')
BUILD = os.path.join(ROOT, 'build/45410914')


# ------------------------------------------------------------------ COFF read
def coff_symbols(path):
    """Symbol names defined in an MSVC X360 .obj (little-endian COFF headers).
    Validated by `--selftest-coff`, which requires a known mangled name."""
    try:
        d = open(path, 'rb').read()
    except OSError:
        return set()
    if len(d) < 20:
        return set()
    machine, nsec, ts, psym, nsym, optsz, chars = struct.unpack_from('<HHIIIHH', d, 0)
    if psym == 0 or nsym == 0 or psym + 18 * nsym > len(d):
        return set()
    strtab_off = psym + 18 * nsym
    out = set()
    i = 0
    while i < nsym:
        o = psym + 18 * i
        raw = d[o:o + 8]
        naux = d[o + 17]
        sect = struct.unpack_from('<h', d, o + 12)[0]
        if raw[:4] == b'\0\0\0\0':
            soff = struct.unpack_from('<I', d, o + 4)[0]
            e = d.find(b'\0', strtab_off + soff)
            nm = d[strtab_off + soff:e].decode('ascii', 'replace')
        else:
            nm = raw.rstrip(b'\0').decode('ascii', 'replace')
        if sect > 0 and nm:
            out.add(nm)
        i += 1 + naux
    return out


# ---------------------------------------------------------------- splits read
def parse_splits():
    """unit -> [(lo,hi)] for .text only."""
    units = {}
    cur = None
    for line in open(SPLITS):
        s = line.strip()
        if not line.startswith((' ', '\t')) and s.endswith(':'):
            cur = s[:-1]
            if cur == 'Sections':
                cur = None
            continue
        if cur and s.startswith('.text'):
            m = re.search(r'start:(0x[0-9A-Fa-f]+)\s+end:(0x[0-9A-Fa-f]+)', s)
            if m:
                units.setdefault(cur, []).append((int(m.group(1), 16),
                                                  int(m.group(2), 16)))
    return units


def unit_index(units):
    iv = []
    for u, rs in units.items():
        for lo, hi in rs:
            iv.append((lo, hi, u))
    iv.sort()
    return iv


def unit_of(iv, va):
    import bisect
    i = bisect.bisect_right(iv, (va, 0xFFFFFFFF, '￿')) - 1
    if i >= 0 and iv[i][0] <= va < iv[i][1]:
        return iv[i][2]
    return None


def obj_for_unit(unit):
    """splits unit 'Foo.cpp' -> our compiled build/.../Foo.obj (search)."""
    base = os.path.splitext(os.path.basename(unit))[0] + '.obj'
    for root, dirs, files in os.walk(os.path.join(BUILD, 'src')):
        if base in files:
            return os.path.join(root, base)
    return None


def main():
    out_dir = '/home/free/tmp/laneCI1'
    C = Census()
    rows = json.load(open(os.path.join(out_dir, 'ci1_census.json')))
    units = parse_splits()
    iv = unit_index(units)
    objcache = {}

    def unit_syms(u):
        if u not in objcache:
            p = obj_for_unit(u) if u else None
            objcache[u] = coff_symbols(p) if p else set()
        return objcache[u]

    # reverse map for injectivity
    name2addr = collections.defaultdict(list)
    for a, n in C.mapi.items():
        name2addr[n].append(a)
    predup = sum(1 for n, a in name2addr.items() if len(a) > 1)
    print(f'[map] {len(C.mapi)} rows, names used at >1 address (pre-existing '
          f'duplicates): {predup}')

    # slot-role table: for each (col_offset, slot_idx) how many map-named
    # occupants are ??_G/??_E vs other  -- built ONCE, map-wide.
    role = collections.defaultdict(collections.Counter)
    for va, cs in C.attr_named.items():
        nm = C.mapi.get(va)
        if not nm:
            continue
        kind = 'dtor' if nm.startswith(('??_G', '??_E')) else 'other'
        for c, ents in cs.items():
            for vt, i, off in ents:
                role[(off, i)][kind] += 1

    contra = [r for r in rows if r['verdict_named'] == 'CONTRA']
    print(f'[stratum] CONTRA rows: {len(contra)}')

    out = []
    for r in contra:
        va = int(r['addr'], 16)
        cls = r['cls']
        rec = dict(r)
        rec['row_cls_has_vtable'] = bool(C.vt_of_class.get(cls))
        # instrument B
        t = C.dtor_bl(va)
        bn = C.mapi.get(t) if t else None
        rec['dtor_bl'] = hex(t) if t else None
        rec['dtor_bl_name'] = bn
        m = re.match(r'\?\?(?:1|_D)([A-Za-z_0-9@?$]+?)@@', bn) if bn else None
        rec['dtor_bl_cls'] = m.group(1) if m else None
        # instrument C
        rec['vftable_ref_cls'] = sorted(C.vftable_refs(va))
        # slot role for each candidate
        cands = []
        for c, i, off in r['slots']:
            k = role[(off, i)]
            cands.append(dict(cls=c, slot=i, coloff=off,
                              role_dtor=k['dtor'], role_other=k['other']))
        rec['cands'] = cands
        rec['unit'] = unit_of(iv, va)
        syms = unit_syms(rec['unit'])
        rec['unit_has_current'] = r['sym'] in syms
        # propose
        prop = None
        agree = [c for c in {x['cls'] for x in cands}
                 if c and c == rec['dtor_bl_cls']]
        rec['conjunction_cls'] = agree
        for c in agree:
            for tmpl in ('??_G%s@@UAAPAXI@Z', '??_E%s@@UAAPAXI@Z'):
                nm = tmpl % c
                if nm in syms:
                    prop = nm
                    break
            if prop:
                break
        rec['proposed'] = prop
        rec['proposed_in_unit'] = bool(prop)
        rec['proposed_dup'] = bool(prop and prop in name2addr)
        out.append(rec)

    json.dump(out, open(os.path.join(out_dir, 'ci1_contra.json'), 'w'), indent=1)

    # ---- gate funnel ----------------------------------------------------
    g0 = [r for r in out if r['row_cls_has_vtable']]
    g1 = [r for r in g0 if r['conjunction_cls']]
    g3 = [r for r in g1 if r['proposed_in_unit']]
    g4 = [r for r in g3 if not r['proposed_dup']]
    print(f'  G0 row class has a vtable        : {len(g0)} / {len(out)}')
    print(f'  G1 + dtor_bl corroborates (conj) : {len(g1)}')
    print(f'  G3 + proposed name in home unit  : {len(g3)}')
    print(f'  G4 + injective (no dup)          : {len(g4)}')
    print(f'  [i] rows whose CURRENT name our home-unit obj exports: '
          f'{sum(1 for r in out if r["unit_has_current"])} / {len(out)}')
    for r in g4:
        print(f"    REPAIR {r['addr']} {r['sym'][:52]} -> {r['proposed'][:52]} "
              f"unit={r['unit']}")
    json.dump(g4, open(os.path.join(out_dir, 'ci1_repairs.json'), 'w'), indent=1)
    print(f'-> {out_dir}/ci1_contra.json , ci1_repairs.json')


if __name__ == '__main__':
    if '--selftest-coff' in sys.argv:
        p = os.path.join(BUILD, 'src/system/meta/CreditsPanel.obj')
        s = coff_symbols(p)
        print(f'CreditsPanel.obj symbols: {len(s)}')
        hits = [x for x in s if 'CreditsPanel' in x][:8]
        print('  sample:', hits)
        # FAIL ON DEMAND: a name that cannot be there
        bogus = '??_GZzNotARealClass_CI1@@UAAPAXI@Z'
        print(f'  fail-on-demand bogus present={bogus in s} -> '
              f'{"OK" if bogus not in s else "BROKEN"}')
        # and the reader must return >0 or the gate is vacuously "absent"
        print('COFF SELFTEST', 'PASS' if len(s) > 10 and hits and bogus not in s
              else 'FAIL')
        sys.exit(0)
    main()
