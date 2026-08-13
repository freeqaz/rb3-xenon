#!/usr/bin/env python3
"""Lane STEPS-1: pricing the NON-CLOSABLE single-step repairs left by SOLVER-1.

THE QUESTION THIS ANSWERS
-------------------------
ORACLE-1 verified 40 thunk-name repairs on two independent channels and landed
only the closed subset, because landing all 40 measured -15 matched functions.
SOLVER-1 showed that headline was a RULER SPLIT rather than a refutation: the
same run carried +248 B, because a wrong branch-target name is an ARG-ONLY
penalty -- `match_percent_normalized` excludes those, `fuzzy_match_percent`
(which `matched_code` follows) includes them.  So the byte-twin fake credit is a
property of the FUNCTION ruler alone.  Whether the remaining ~100 NON-closable
repairs are net-positive on BYTES was left open.  They are, but only under a
filter neither prior lane had.

THE ANSWER, AND THE DISCRIMINATOR THAT PRODUCES IT
--------------------------------------------------
A non-closable step rewrites map[A] from cur[A] to new[A].  Nobody holds new[A]
(sigma undefined), so nothing is displaced -- one name is stranded, one invented,
injectivity unchanged.  Two outcomes, and they are NOT alike:

  DEFINED    our compiled obj for the unit that OWNS address A defines new[A].
             The name moves INTO the address: we lose the incumbent's fake
             mpn-100 row and gain a real one.  Delta functions 0, and the bytes
             arrive, because the incumbent sat at fuzzy < 100 contributing
             NOTHING to matched_code while drawing mpn-100 credit.

  NOT_DEFINED  no row materialises.  -1 function, +0 bytes.  This is not a
             naming defect at all: the proposed name is defined in a DIFFERENT
             unit than the splits pin, i.e. it is a MIS-PIN, and renaming can
             never pay.  Measured 8 of 24 on this tree.

=> the ENTIRE function-count loss lives in NOT_DEFINED.  Filtering on
"is the proposed name defined in the OWNING unit's obj" converts ORACLE-1's
-15 matched into a clean Delta-0.  That test is what this file adds; the anchor
(col_anchor) and the solver (col_cycles) are reused unchanged.

TWO CONTROLS, BOTH OF WHICH FIRED AND CHANGED THE RESULT
--------------------------------------------------------
1. DERANGEMENT.  A different permutation of the SAME multiset over the SAME
   addresses passes conservation and injectivity identically, so only the branch
   channel can separate a real block from a sham.  Running it revealed that
   AGREE_METHOD is REACHABLE BY A WRONG ASSIGNMENT -- the sham put
   `?SetType@RndTexBlender@@$4...` at an address wanting `?SetType@RndMatAnim@@`
   and scored AGREE_METHOD, because the METHOD matches and only the CLASS is
   wrong.  SOLVER-1's assemble() accepts AGREE_METHOD; this file does NOT.
   Accepting only AGREE, the sham verifies ZERO times.

2. RETAIL-BYTE ADJUDICATION.  ab_measure flags this shape ALIAS_SUSPECT
   (name_check up, `none` flat).  That heuristic is STRUCTURALLY VACUOUS on a
   byte-twin population: `none` ignores relocation names, so it scores a RIGHT
   pairing and a WRONG pairing identically -- flatness is the expected signature
   of ANY rename here, correct or not (the same trap as "match% can never settle
   /GR").  The test that does discriminate reads RETAIL bytes: decode the thunk
   at A, take its tail branch to T, and require that T's identity match the
   class::method the new name claims.  Measured: NEW agrees 13/13, INCUMBENT
   disagrees 13/13, and 0 of the branch destinations are themselves suspect rows
   (so the adjudication is not circular).

   The one row whose branch destination was UNMAPPED -- and therefore the only
   one this channel could NOT verify -- is also the only one that regressed
   (-1 fn / -12 B), and the `none` ruler independently showed it DESTROYING 12 B
   of real name-blind agreement.  Three channels, one verdict.  Dropped.

MEASURED (whole-binary A/B, map-only, name_check ruler)
  14 rows incl. the unverifiable one : Delta matched -1 / +152 B  [ALIAS_SUSPECT]
  13 rows, verified only            : Delta matched +0 / +164 B  [REAL_PAIRING]
  predicted per row beforehand      : +0 / +164 B -- exact on both rulers
"""
import argparse
import collections
import glob
import json
import os
import random
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..'))
sys.path.insert(0, os.path.join(HERE, '..', 'maprow_dtor'))
import col_cycles as CC                                          # noqa: E402
from cj4_coff import read_symbols                                # noqa: E402
from thunk_identity import adjudicate_strict                     # noqa: E402
from retail_reader import Image as RImage, tail_b_target         # noqa: E402

SPLITS = 'config/45410914/splits.txt'
OURS = 'build/45410914/src'
BAND = 'orig/45410914/band.exe'


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


def defined_symbols(objroot=OURS):
    """-> {obj basename: {symbols DEFINED in it}}.

    SectionNumber > 0 is the only field that separates a definition from an
    undefined external REFERENCE; a substring scan cannot (see cj4_coff).
    """
    out = collections.defaultdict(set)
    for p in glob.glob(os.path.join(objroot, '**', '*.obj'), recursive=True):
        try:
            syms = read_symbols(open(p, 'rb').read())
        except Exception:
            continue
        b = os.path.basename(p)
        for s in syms:
            if s.section > 0:
                out[b].add(s.name)
    return out


def ident(n):
    """(method, class) from a mangled name -- the join key for adjudication."""
    if not n:
        return None
    m = re.match(r'\?\?_[EG]([A-Za-z0-9_]+)@@', n)
    if m:
        return ('dtor', m.group(1))
    m = re.match(r'\?([A-Za-z0-9_]+)@([A-Za-z0-9_]+)@@', n)
    return (m.group(1), m.group(2)) if m else None


def retail_branch_target(img, va):
    for sz in (32, 24, 20, 16, 12, 8):
        try:
            t = tail_b_target(img, va, sz)
            if t:
                return t
        except Exception:
            pass
    return None


def select(S=None):
    """-> (selected {addr: new}, rejected {addr: reason}, stats)."""
    S = S or CC.solve()
    tmap, cand, deny = S['tmap'], S['cand'], S['deny']

    # never half-apply a chain: SOLVER-1's closable chains are its business
    reserved = set()
    for p, _o, z in S['chains']:
        reserved.update(p)
        reserved.add(z)

    name2addr = collections.defaultdict(set)
    for k, v in tmap.items():
        if isinstance(v, str):
            name2addr[v].add(int(k, 16))

    owners = text_owners()

    def owner(a):
        for s, e, u in owners:
            if s <= a < e:
                return u

    defined = defined_symbols()

    # CASE1 only: sigma undefined => nothing displaced, injectivity untouched
    case1 = {}
    for a, cd in cand.items():
        if a in reserved or a in deny:
            continue
        if not (name2addr.get(cd['new'], set()) - {a}):
            case1['0x%08x' % a] = cd

    sim = dict(tmap)
    for a, cd in case1.items():
        sim[a] = cd['new']

    sel, rej = {}, {}
    for a, cd in sorted(case1.items()):
        ow = owner(int(a, 16))
        v = adjudicate_strict(S['timg'], sim, a, cd['new'])['verdict']
        if v != 'AGREE':                       # AGREE_METHOD is sham-reachable
            rej[a] = 'branch channel: %s' % v
            continue
        if not (ow and cd['new'] in defined.get(os.path.basename(ow)[:-4] + '.obj', ())):
            rej[a] = 'not defined in owning unit %s (MIS-PIN, cannot pay)' % os.path.basename(ow or '?')
            continue
        sel[a] = cd['new']
    return sel, rej, dict(candidates=len(cand), case1=len(case1), selected=len(sel))


def adjudicate_retail(sel, tmap, head_map=None):
    """RETAIL-BYTE test + the incumbent control.  -> (rows, summary)."""
    img = RImage(BAND)
    susp = None
    rows, s = [], collections.Counter()
    for a, new in sorted(sel.items()):
        T = retail_branch_target(img, int(a, 16))
        k = '0x%08x' % T if T else None
        tid = ident(tmap.get(k))
        nid, cid = ident(new), ident((head_map or {}).get(a))
        vn = 'UNVERIFIABLE' if not (tid and nid) else ('AGREE' if tid == nid else 'DISAGREE')
        vi = None if not (tid and cid) else ('AGREE' if tid == cid else 'DISAGREE')
        s['new_' + vn] += 1
        if vi:
            s['incumbent_' + vi] += 1
        rows.append(dict(addr=a, target=k, target_is=tid, new_is=nid, incumbent_is=cid,
                         new=vn, incumbent=vi))
    return rows, dict(s)


def derangement_control(sel, S):
    """A DIFFERENT permutation of the SAME multiset must NOT verify."""
    ks = sorted(sel)
    vs = [sel[k] for k in ks]
    rnd, sham = random.Random(1234), None
    for _ in range(500):
        p = vs[:]
        rnd.shuffle(p)
        if all(x != y for x, y in zip(p, vs)):
            sham = dict(zip(ks, p))
            break
    if not sham:
        return None
    assert collections.Counter(sham.values()) == collections.Counter(vs), 'sham is not the same multiset'

    def probe(m):
        sim = dict(S['tmap'])
        sim.update(m)
        return collections.Counter(adjudicate_strict(S['timg'], sim, a, n)['verdict']
                                   for a, n in m.items())
    return probe(sel), probe(sham)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--json', help='write the edit set here')
    ap.add_argument('--head-map', help='HEAD copy of the map, for the incumbent control '
                                       '(reading the live map after a run is VACUOUS -- '
                                       'ab_measure restores the tree WITH the edits applied)')
    a = ap.parse_args()

    S = CC.solve()
    sel, rej, st = select(S)
    print('candidates %(candidates)d -> CASE1 (pure single step) %(case1)d -> SELECTED %(selected)d' % st)
    for k, v in sorted(rej.items()):
        print('  REJECT %s  %s' % (k, v))

    real, sham = derangement_control(sel, S)
    print('\n[control] derangement of the SAME multiset over the SAME addresses')
    print('  real %s\n  sham %s' % (dict(real), dict(sham)))
    assert real.get('AGREE', 0) == len(sel), 'the real block does not verify'
    assert sham.get('AGREE', 0) == 0, 'VACUOUS: the sham verified too'
    print('  => the channel discriminates PERMUTATIONS, not just multisets')

    head = json.load(open(a.head_map)) if a.head_map else None
    rows, summ = adjudicate_retail(sel, S['tmap'], head)
    print('\n[retail bytes] thunk at A -> tail branch -> T ; T must be what the name claims')
    for r in rows:
        print('  %-11s -> %-11s %-30s new=%-12s inc=%s'
              % (r['addr'], r['target'] or '-',
                 ('%s::%s' % (r['target_is'][1], r['target_is'][0])) if r['target_is'] else 'UNMAPPED',
                 r['new'], r['incumbent']))
    print('  %s' % summ)

    # The retail channel GATES, it does not merely report.  A row whose branch
    # destination is unmapped cannot be adjudicated on retail bytes, and the one
    # such row on this tree was independently condemned by BOTH other channels
    # (-1 fn / -12 B on mpn+fuzzy, and -12 B on the name-blind `none` ruler).
    drop = [r['addr'] for r in rows if r['new'] != 'AGREE']
    for d in drop:
        print('  DROP %s: retail branch destination UNMAPPED -- unverifiable, not landed for the bytes' % d)
        sel.pop(d, None)

    if a.json:
        json.dump(sel, open(a.json, 'w'), indent=1)
        print('\nwrote %s (%d edits, all retail-verified)' % (a.json, len(sel)))


if __name__ == '__main__':
    main()
