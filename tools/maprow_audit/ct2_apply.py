#!/usr/bin/env python3
"""Lane CT-2: apply the cycle edits TEXTUALLY (never json.dump -- the map's
1-space indent and key order are load-bearing), then verify every invariant.

Gate 4 checks, all of them, before AND after:
  * duplicate KEYS rejected via object_pairs_hook (the map-applier phantom trap:
    appliers insert at the top, json.load keeps the LAST, so a duplicate key
    silently wins and the diff looks clean);
  * duplicate NAME count unchanged (13 -> 13; a permutation cannot change it);
  * no hex-case collisions introduced;
  * every edit matched exactly one line, and the old name was what we expected.
"""
import json, sys, collections

WT = "/home/free/tmp/laneCT2/wt"
MAP = WT + "/scripts/target_symbol_map.json"
edits = json.load(open("/home/free/tmp/laneCT2/ct2_triples.json"))


def dupkey_hook(pairs):
    seen = set()
    for k, _v in pairs:
        if k in seen:
            raise ValueError("DUPLICATE KEY in map: %s" % k)
        seen.add(k)
    return dict(pairs)


src = open(MAP).read()
before = json.loads(src, object_pairs_hook=dupkey_hook)
print("pre-check: no duplicate keys, %d entries" % len(before))

nb = collections.Counter(v for k, v in before.items()
                         if k.startswith("0x") and isinstance(v, str))
dup_before = sum(1 for n, c in nb.items() if c > 1)

lines = src.split("\n")
idx = {}
for i, ln in enumerate(lines):
    s = ln.strip().rstrip(",")
    if s.startswith('"0x') and '": "' in s:
        k = s.split('": "')[0][1:]
        idx.setdefault(k.lower(), []).append(i)

nedit = 0
for e in edits:
    hits = idx.get(e["addr"], [])
    assert len(hits) == 1, "addr %s matched %d lines" % (e["addr"], len(hits))
    i = hits[0]
    old = lines[i].strip().rstrip(",").split('": "', 1)[1][:-1]
    assert old == e["expect"], "addr %s holds %r not %r" % (e["addr"], old, e["expect"])
    lines[i] = lines[i].replace('": "%s"' % old, '": "%s"' % e["new"], 1)
    nedit += 1
print("edited %d lines" % nedit)

out = "\n".join(lines)
after = json.loads(out, object_pairs_hook=dupkey_hook)
assert len(after) == len(before), "entry count changed"
na = collections.Counter(v for k, v in after.items()
                         if k.startswith("0x") and isinstance(v, str))
dup_after = sum(1 for n, c in na.items() if c > 1)
print("duplicate NAMES before=%d after=%d introduced=%d" % (dup_before, dup_after, dup_after - dup_before))
assert dup_after == dup_before

# hex-case collisions
keys = [k for k in after if k.startswith("0x")]
low = collections.Counter(k.lower() for k in keys)
assert not [k for k, c in low.items() if c > 1], "hex-case collision introduced"
print("no hex-case collisions; keys %d" % len(keys))

# the multiset of names must be IDENTICAL -- these edits are a pure permutation
assert nb == na, "name multiset changed -- not a pure permutation!"
print("name multiset UNCHANGED -> confirmed pure permutation")

changed = [k for k in before if before[k] != after.get(k)]
print("keys whose value changed: %d -> %s" % (len(changed), sorted(changed)))
assert len(changed) == len(edits)

open(MAP, "w").write(out)
print("WROTE %s" % MAP)
