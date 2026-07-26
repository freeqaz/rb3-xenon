#!/usr/bin/env python3
"""Displace a *provably-wrong* map holder with a *uniquely byte-identical* claimant.

WHY THIS EXISTS (the gap map_repoint_round.py leaves open)
----------------------------------------------------------
`map_repoint_round.py` walks `hits_of.items()` and immediately does

    cur = name2va.get(name)
    if not cur or (cur & hits):
        continue

so it can only ever *move* a name that is **already in the map**.  A symbol our
build emits that is **not in the map at all**, yet whose reloc-masked bytes are
identical to exactly one place in the whole 11.8 MB binary, is skipped — and if
that one place is already occupied by some other (wrong) name, `homing_gen`'s
insert path refuses it too, because `tu5_map_apply_fragment.py` asserts on an
address collision.  Those functions fall between the two tools and stay dark.

`homing_scan.py` labels exactly this population **ALL-MAPPED**: every hit VA is
occupied.  There are 37k such records; the overwhelming majority are simply
correctly mapped already (holder == name).  The interesting residue is:

    holder H sits at VA, claimant C is byte-identical at VA, H is NOT.

Then H cannot be the function at VA (objdiff would already score it 100), C
provably is, and the swap is a guaranteed flip rather than a gamble.

WORKED EXAMPLE (the case that motivated the tool)
-------------------------------------------------
`default/DateTime` had `?ToString@DateTime@@QBAXAAVString@@@Z` mapped at BOTH
`0x82522b58` (79.0%) and `0x82523178` (0.0%).  Homing puts ToString's bytes at
`0x82523178` **only**; `argreg_mispair_scan.py` independently proves the body at
`0x82522b58` reads r5, which `ToString(String&)` does not declare.  And
`??$MakeString@HE@@YAPBDPBDHE@Z` — a symbol not in the map at all — is
byte-identical at `0x82522b58` and nowhere else.  Same story one function later
for `?GetTimeZoneBias@@YAXAAJ@Z` / `??$MakeString@E@@YAPBDPBDE@Z`.

SAFETY MODEL
------------
A displacement is emitted only when ALL of these hold:

  * C's hit set is exactly {VA} — unique in the whole binary, so there is
    nowhere else it can be.  (Reloc-masked identity is the same comparison
    objdiff's normalized diff performs, so a paired C at VA reads 100.)
  * C is not currently in the map anywhere (pure insert; no rotation, nothing
    stranded — rotations must be applied as whole cycles and that is
    `map_rotation_repair.py`'s job, not this tool's).
  * The holder H is NOT byte-identical at VA.  This is the ICF guard: when both
    bodies are identical the linker folded two source functions onto one VA, a
    VA->name map cannot express both, and displacing is a coin flip that was
    measured at -23/+0.  Refuse, never assert.
  * Exactly one claimant for VA (uncontested).
  * VA outside the 0x82800000-0x82C00000 vendor band (XDK + Quazal, out of scope).

Tiers describe only the *holder's* evidence, since the claimant's is always
the same (unique byte identity):

  T1_CONTRADICTED  H has hits of its own and none of them is VA.
  T2_NOMATCH       H matches nowhere in the binary (our source for H drifted).
  T3_NO_RECORD     H is not a code symbol our build emits at all, so its map
                   entry is inert.

`--pays-only` additionally requires, via `span_predictor.py`, that VA fall in
the pinned `.text` span of a unit whose obj defines C — an entry outside a
pinned span is more correct but cannot pay a match.

Emits a `map_rotation_repair.py apply` plan ({set, remove, ...}), so the map is
rewritten textually, line-oriented.  NEVER json.dump the map.

Usage:
  python3 scripts/harvest/map_displace_round.py --worktree $PWD \
      --results ~/tmp/homing/merged.json --out ~/tmp/plan.json [--pays-only]
  python3 scripts/harvest/map_rotation_repair.py apply --plan ~/tmp/plan.json \
      --map scripts/target_symbol_map.json

VA comparison is always case-insensitive: 264 legacy map keys are "0X...".
"""
import argparse
import json
import os
import subprocess
import sys
from collections import defaultdict

VENDOR_LO, VENDOR_HI = 0x82800000, 0x82C00000


def in_vendor(va):
    return VENDOR_LO <= int(va, 16) < VENDOR_HI


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--worktree', required=True)
    ap.add_argument('--results', required=True, help='homing merged.json')
    ap.add_argument('--map')
    ap.add_argument('--out', required=True)
    ap.add_argument('--pays-only', action='store_true')
    ap.add_argument('--break-ties', action='store_true',
                    help='disambiguate ICF-tied claimants spatially')
    ap.add_argument('--include-free', action='store_true',
                    help='also emit unique claimants whose VA is unoccupied')
    ap.add_argument('--tiers', default='T1_CONTRADICTED,T2_NOMATCH,T3_NO_RECORD')
    ap.add_argument('--report', default=None, help='write evidence json here')
    ap.add_argument('--strict-guard', default=None,
                    help='report.json of the CURRENT state; refuse to displace '
                         'any holder that already reads strict-100 there')
    a = ap.parse_args()

    wt = a.worktree
    mp = a.map or os.path.join(wt, 'scripts/target_symbol_map.json')
    raw = {k.lower(): v for k, v in json.load(open(mp)).items()
           if k.lower().startswith('0x') and isinstance(v, str)}
    mapped_names = set(raw.values())
    name2va = defaultdict(set)
    for k, v in raw.items():
        name2va[v].add(k)

    hits_of, tu_of = {}, {}
    for tu, recs in json.load(open(a.results)).items():
        if not isinstance(recs, list):
            continue
        for r in recs:
            hs = {h.lower() for h in (r.get('hits') or [])}
            # a symbol can appear in several TUs (COMDAT); union the evidence
            hits_of.setdefault(r['name'], set()).update(hs)
            tu_of.setdefault(r['name'], tu)

    # claimants: unmapped name whose bytes are unique in the whole binary
    claims = defaultdict(list)
    for name, hs in hits_of.items():
        if len(hs) != 1 or name in mapped_names:
            continue
        va = next(iter(hs))
        if in_vendor(va):
            continue
        claims[va].append(name)

    # ---- guard 1: a holder that ALREADY reads strict-100 is, by definition,
    # the right name for that VA -- whatever the byte scan thinks.  The first
    # wave of this tool lost 14 matches to exactly this case: our build emits
    # the same function under two spellings (an anonymous-namespace hash that
    # the `anon_ns` obj patcher normalises, or a Ham*/Band* class rename), so a
    # "unique" claimant is really an alias of the sitting holder.  Displacing
    # then just swaps which spelling wins and drops the other.
    strict100 = set()
    if a.strict_guard:
        rep = json.load(open(a.strict_guard))
        strict100 = {f['name'] for u in rep['units']
                     for f in u.get('functions', [])
                     if f.get('match_percent_normalized') == 100.0}

    # ---- guard 2: same function, different spelling.  Anonymous-namespace
    # hashes are derived from machine name + source path, so two TUs spell the
    # same local function differently; never treat one as evidence against
    # the other.
    import re
    anon = re.compile(r'\?A0x[0-9a-fA-F]+')

    def alias(x, y):
        return anon.sub('?A0xH', x) == anon.sub('?A0xH', y)

    tiers = set(a.tiers.split(','))
    stats = defaultdict(int)
    cands = []
    # ---- contested claims: several unmapped names are byte-identical at the
    # same VA, i.e. the linker ICF-folded them.  Byte identity cannot rank
    # them -- but retail is NOT LTCG-built, so .text still preserves per-TU
    # grouping, and `span_predictor.py` can: keep the tie only when EXACTLY
    # ONE of the tied names is defined by the unit whose pinned span owns the
    # VA.  That is the same spatial discriminator that measured +21/-0 as
    # map_repoint_round.py's discriminator 2, not a coin flip between twins.
    if a.break_ties:
        tied = {va: ns for va, ns in claims.items() if len(ns) > 1}
        if tied:
            pin = a.out + '.tie'
            json.dump({'t': [dict(name=n, va=va)
                             for va, ns in tied.items() for n in ns]},
                      open(pin, 'w'))
            sp = a.out + '.tiespan'
            subprocess.run(
                [sys.executable, os.path.join(wt, 'scripts/harvest/span_predictor.py'),
                 '--proposals', pin, '--worktree', wt, '--out', sp],
                check=True, capture_output=True)
            pays = defaultdict(list)
            for r in json.load(open(sp.replace('.json', '_detail.json'))):
                if r.get('cls') == 'PAYS':
                    pays[r['va'].lower()].append(r['name'])
            for va, ns in tied.items():
                p = pays.get(va, [])
                if len(p) == 1:
                    claims[va] = p
                    stats['tie-broken-spatially'] += 1

    for va, names in claims.items():
        if len(names) > 1:
            stats['refuse-contested-claim'] += 1
            continue
        c = names[0]
        h = raw.get(va)
        if h is None:
            # Nobody holds this VA: a plain insert, which is homing_gen's job.
            # Offered here because the two tools share the claimant computation
            # and the flywheel is not always re-run after a body-port wave.
            if not a.include_free:
                stats['free-va(homing_gen territory)'] += 1
                continue
            cands.append(dict(va=va, claimant=c, holder=None, tier='T0_FREE_VA',
                              tu=tu_of.get(c), holder_hits=[]))
            stats['T0_FREE_VA'] += 1
            continue
        if h == c:
            stats['already-correct'] += 1
            continue
        # ...but only when THIS VA is the holder's sole home.  A name mapped at
        # two VAs can be reading 100 at the other one, and refusing then just
        # forfeits a clean gain (measured: 2 of 51).
        if h in strict100 and len(name2va.get(h, ())) < 2:
            stats['refuse-holder-already-100'] += 1
            continue
        if alias(h, c):
            stats['refuse-anon-ns-alias'] += 1
            continue
        hh = hits_of.get(h)
        if hh is None:
            tier = 'T3_NO_RECORD'
        elif va in hh:
            stats['refuse-ICF-holder-also-identical'] += 1
            continue
        elif hh:
            tier = 'T1_CONTRADICTED'
        else:
            tier = 'T2_NOMATCH'
        if tier not in tiers:
            stats['refuse-tier-' + tier] += 1
            continue
        cands.append(dict(va=va, claimant=c, holder=h, tier=tier,
                          tu=tu_of.get(c), holder_hits=sorted(hh or [])[:4]))
        stats[tier] += 1

    if a.pays_only and cands:
        pin = a.out + '.prop'
        json.dump({'m': [dict(name=c['claimant'], va=c['va']) for c in cands]},
                  open(pin, 'w'))
        span = a.out + '.span'
        subprocess.run(
            [sys.executable, os.path.join(wt, 'scripts/harvest/span_predictor.py'),
             '--proposals', pin, '--worktree', wt, '--out', span], check=True,
            capture_output=True)
        # the filtered output is keyed by TU and carries no class; the
        # sidecar `_detail.json` is where span_predictor records it.
        cls = {}
        for r in json.load(open(span.replace('.json', '_detail.json'))):
            cls[(r['name'], r['va'].lower())] = r.get('cls')
        keep = []
        for c in cands:
            c['span'] = cls.get((c['claimant'], c['va']))
            if c['span'] == 'PAYS':
                keep.append(c)
            else:
                stats['refuse-span-' + str(c['span'])] += 1
        cands = keep

    setmap = {c['va']: c['claimant'] for c in cands}
    plan = dict(set=setmap, remove=[],
                evicted=sorted({c['holder'] for c in cands if c['holder']}),
                blocked={}, contested={}, cycles=[], chains=[])
    json.dump(plan, open(a.out, 'w'), indent=1)
    if a.report:
        json.dump(cands, open(a.report, 'w'), indent=1)
    print('displace round:', dict(sorted(stats.items(), key=lambda kv: -kv[1])))
    print('plan: %d displacements -> %s' % (len(setmap), a.out))


if __name__ == '__main__':
    main()
