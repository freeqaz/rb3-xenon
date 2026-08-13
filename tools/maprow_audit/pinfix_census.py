#!/usr/bin/env python3
"""Lane PINFIX-1: rows whose NAME is already correct but whose PIN is wrong.

WHAT THIS CLASS IS
------------------
MISPIN-1 worked the rows whose *proposed* name was defined in a different unit
than the splits pin -- a rename-vs-repin question.  In passing it saw a WIDER
population that no map lane can see at all: rows whose name in
target_symbol_map.json is ALREADY the name we want, but whose address is pinned
to a unit that does not DEFINE that name.  No name-based screen fires on them
(the name is right), yet they cannot pair, so they read mpn 0.0 / fuzzy 0.000
and cost a row right now.

THE JOIN, AND WHY THE OBVIOUS ONE IS VACUOUS
--------------------------------------------
splits.txt names units by BARE BASENAME (`Movie.cpp`), while our compiled objs
live at nested paths (`build/45410914/src/system/rndobj/Movie.obj`).  A scan
joining obj RELATIVE PATHS against unit BASENAMES fails EVERY nested lookup and
falls through to "defined elsewhere" -- MISPIN-1 measured that artifact at
10,563 phantom mis-pins against a true 149.  A scan joining on BASENAME instead
collides system/rnddx9/Movie.obj with system/rndobj/Movie.obj.

Neither is used here.  The authoritative unit -> obj join is objdiff.json, which
configure.py emits with BOTH sides: target_path `build/45410914/obj/<UNIT>.obj`
(the splits unit) and base_path `build/45410914/src/<nested>/<UNIT>.obj` (our
compiled obj).  Defining sets are keyed by that FULL base_path, so Movie stays
two distinct units.

SELF-VALIDATION (mandatory before any count here is believed)
-------------------------------------------------------------
The broken join could not have produced a high healthy rate -- that is exactly
what exposed it.  So this scan reports the HEALTHY share of pinned named rows
first, and MISPIN-1's corrected figure (22,384/27,321 = 81.9%) is the reference
point.  A healthy share far below that means the join is broken again, not that
the tree regressed.

CLASSES REPORTED
  HEALTHY        name defined in the owning unit's own obj.
  MISPIN_SINGLE  not defined there; defined in EXACTLY ONE other unit -> decisive.
  MISPIN_MULTI   defined in >1 other unit (COMDAT) -> a two-unit probe would be
                 vacuous; NOT actionable on definedness alone.
  UNDEFINED      defined by nobody in our tree (unimplemented) -> unpayable.
  NO_OBJ         the owning unit has no compiled obj at all (a different class,
                 4,515 rows; MISPIN-1 explicitly did not touch it).
"""
import argparse
import collections
import glob
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..'))

from cj4_coff import read_symbols                                # noqa: E402

SPLITS = 'config/45410914/splits.txt'
MAP = 'scripts/target_symbol_map.json'
OBJDIFF = 'objdiff.json'
OURS = 'build/45410914/src'


def text_owners(splits=SPLITS):
    """-> sorted [(start, end, unit)] over .text ranges only."""
    unit, out = None, []
    for ln in open(splits):
        m = re.match(r'^(\S+):\s*$', ln)
        if m:
            unit = m.group(1)
            continue
        m = re.match(r'^\s+\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)', ln)
        if m and unit:
            out.append((int(m.group(1), 16), int(m.group(2), 16), unit))
    return sorted(out)


def unit_to_obj(objdiff=OBJDIFF, objroot=OURS):
    """-> {splits unit heading: base obj path}.

    ⚠ splits.txt headings come in TWO forms and a join handling only one is
    silently deficient -- handling only the bare form misclassified 4,940 rows
    as "unit has no obj", which is the same disease as MISPIN-1's broken join
    running in the other direction:

      BARE    `MasterAudio.cpp`                  (707 headings)
      NESTED  `band3/meta_band/SaveLoadManager.cpp` (569 headings)

    NESTED headings ARE the source-relative path, so the obj is
    `build/45410914/src/<heading>.obj` directly.  BARE headings resolve through
    objdiff.json, which configure.py emits carrying BOTH sides: target_path
    `build/45410914/obj/<UNIT>.obj` and base_path our real nested obj.  Either
    way the VALUE is a full path, so Movie stays two distinct units.
    """
    d = json.load(open(objdiff))
    stems = {}
    for u in d['units']:
        tp = u.get('target_path') or ''
        bp = u.get('base_path') or ''
        if not tp or not bp:
            continue
        stem = os.path.basename(tp)
        if stem.endswith('.obj'):
            stem = stem[:-4]
        stems[stem] = bp

    out = {}
    for heading in all_units():
        base = heading[:-4] if heading.endswith('.cpp') else heading
        if '/' in heading:
            p = os.path.join(objroot, base + '.obj')
            if os.path.exists(p):
                out[heading] = p
                continue
        p = stems.get(os.path.basename(base))
        if p and os.path.exists(p):
            out[heading] = p
    return out


def all_units(splits=SPLITS):
    out = []
    for ln in open(splits):
        m = re.match(r'^(\S+):\s*$', ln)
        if m:
            out.append(m.group(1))
    return out


def defining_sets(objroot=OURS):
    """-> {symbol: {full obj path, ...}} over the WHOLE tree, keyed by PATH.

    SectionNumber > 0 is the only field separating a DEFINITION from an
    undefined external REFERENCE; a substring scan cannot tell them apart.
    """
    out = collections.defaultdict(set)
    nobj = 0
    for p in sorted(glob.glob(os.path.join(objroot, '**', '*.obj'), recursive=True)):
        try:
            syms = read_symbols(open(p, 'rb').read())
        except Exception:
            continue
        nobj += 1
        for s in syms:
            if s.section > 0:
                out[s.name].add(p)
    return out, nobj


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--json', help='write the candidate rows here')
    a = ap.parse_args()

    raw = json.load(open(MAP))
    deny = set(int(x, 16) for x in raw.get('_denylist', []) if str(x).startswith('0x'))
    names = {}
    for k, v in raw.items():
        if not k.startswith('0x') or not v:
            continue
        names[int(k, 16)] = v
    print('[map] %d named addresses, %d denylisted' % (len(names), len(deny)))

    owners = text_owners()
    print('[splits] %d .text blocks over %d units'
          % (len(owners), len({u for _, _, u in owners})))

    u2o = unit_to_obj()
    print('[join] objdiff.json pairs %d units to compiled objs' % len(u2o))

    defs, nobj = defining_sets()
    print('[census] defining sets over %d objs, %d distinct defined symbols'
          % (nobj, len(defs)))

    starts = [s for s, _, _ in owners]
    import bisect

    def owner(x):
        i = bisect.bisect_right(starts, x) - 1
        if i < 0:
            return None
        s, e, u = owners[i]
        return u if s <= x < e else None

    # obj path -> splits unit (inverse of the authoritative join)
    o2u = collections.defaultdict(list)
    for u, o in u2o.items():
        o2u[o].append(u)

    tally = collections.Counter()
    cands = []
    for A, nm in sorted(names.items()):
        ou = owner(A)
        if ou is None:
            tally['UNPINNED'] += 1
            continue
        obj = u2o.get(ou)
        if obj is None:
            tally['NO_OBJ'] += 1
            continue
        dset = defs.get(nm)
        if not dset:
            tally['UNDEFINED'] += 1
            continue
        if obj in dset:
            tally['HEALTHY'] += 1
            continue
        # not defined in the owning unit -- where IS it?
        dunits = sorted({v for o in dset for v in o2u.get(o, ())})
        if len(dset) == 1:
            tally['MISPIN_SINGLE'] += 1
            cands.append(dict(addr='0x%08X' % A, name=nm, pinned_unit=ou,
                              pinned_obj=obj,
                              defining_objs=sorted(dset), defining_units=dunits,
                              denied=A in deny))
        else:
            tally['MISPIN_MULTI'] += 1

    pinned = sum(v for k, v in tally.items() if k != 'UNPINNED')
    healthy = tally['HEALTHY']
    print('\n[classes] over %d pinned named rows (%d unpinned excluded)'
          % (pinned, tally['UNPINNED']))
    for k in ('HEALTHY', 'MISPIN_SINGLE', 'MISPIN_MULTI', 'UNDEFINED', 'NO_OBJ'):
        print('  %-14s %6d' % (k, tally[k]))

    withobj = pinned - tally['NO_OBJ']
    print('\n[SELF-VALIDATION] healthy share of rows whose unit HAS an obj: '
          '%d/%d = %.1f%%   (MISPIN-1 corrected reference: 22,384/27,321 = 81.9%%; '
          'the BROKEN join could not reach this)' % (healthy, withobj,
                                                     100.0 * healthy / max(withobj, 1)))

    print('\n[candidates] %d MISPIN_SINGLE (decisive defining set)' % len(cands))
    n_dest_pinned = sum(1 for c in cands if c['defining_units'])
    print('  of which %d have a destination unit that is itself pinned' % n_dest_pinned)
    n_denied = sum(1 for c in cands if c['denied'])
    print('  of which %d are DENYLISTED (a change there is silently inert)' % n_denied)

    if a.json:
        json.dump(cands, open(a.json, 'w'), indent=1)
        print('\nwrote %s' % a.json)


if __name__ == '__main__':
    main()
