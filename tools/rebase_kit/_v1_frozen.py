#!/usr/bin/env python3
"""⛔ FROZEN v1 ALIAS MERGE -- A TEST FIXTURE. DO NOT USE IT TO LAND ANYTHING.

This is the algorithm ``resolve_aliases.py`` used before 2026-09-14, lifted
verbatim and wrapped in a function so the test suite can replay it. It is kept
for exactly one reason: ``test_rebase_kit.py`` requires it to FAIL the same
structural check that v2 passes. A test whose control cannot fail proves
nothing, and this repo has been bitten by that class of vacuity repeatedly.

What v1 got wrong, measured on the W16-J landing:
  * a group changed on BOTH sides was REPLACED with the lane's copy, silently
    discarding main's concurrent edit;
  * a survivor rename was treated as delete + add rather than a rekey, so the
    HEAD-side group was dropped and the lane's re-added.
Cost: -3,728 B of main's concurrent memberships, found by hand afterwards.

v2 pairs by composite key, re-pairs unpaired groups by unique address, merges
field by field, and hard-stops (exit 3) on anything it cannot merge.
"""
import sys


def key(g):
    return (g.get('name'), g.get('address'), g.get('survivor'))


def three_way_v1(ours, base, theirs):
    """The v1 merge, verbatim. Mutates ``ours``; returns the delta count."""
    bmap = {key(g): g for g in base['groups']}
    tmap = {key(g): g for g in theirs['groups']}
    assert len(bmap) == len(base['groups']) and len(tmap) == len(theirs['groups']), \
        "non-unique group keys (name,address,survivor)"
    omap = {key(g): i for i, g in enumerate(ours['groups'])}
    n = 0
    for k in set(bmap) | set(tmap):
        if bmap.get(k) != tmap.get(k):
            n += 1
            if k not in tmap:
                if k in omap:
                    ours['groups'].pop(omap[k])
                    omap = {key(g): i for i, g in enumerate(ours['groups'])}
            elif k in omap:
                ours['groups'][omap[k]] = tmap[k]
            else:
                ours['groups'].append(tmap[k])
                omap[k] = len(ours['groups']) - 1
    if base.get('_comment') != theirs.get('_comment'):
        ours['_comment'] = theirs['_comment']
        n += 1
    return n


if __name__ == '__main__':
    sys.exit("_v1_frozen.py is a TEST FIXTURE, not a resolver. Use resolve_aliases.py.")
