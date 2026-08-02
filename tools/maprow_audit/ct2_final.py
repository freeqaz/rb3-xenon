#!/usr/bin/env python3
"""Lane CT-2 FINAL: all cycles (any length) in the combined demand graph, gated.

CYCLE CLOSURE IS ITSELF THE SECOND CHANNEL, and that is a measurable claim, not
a rhetorical one. If row A's evidence says 'I should be called B's name', B's
says 'I should be called C's name' and C's says 'I should be called A's name',
the three independently-derived candidates have closed a permutation. Wrong
guesses do not do that. NULL BELOW: reshuffle which candidate name each row
receives (same rows, same name pool) and count how many cycles survive.
"""
import json, sys, os, collections, random
sys.path.insert(0, "/home/free/tmp/laneCT2")
exec(open("/home/free/tmp/laneCT2/ct2_combine.py").read().split("# ---- reciprocal 2-cycles")[0])


def find_cycles(e):
    color, out = {}, []
    for u in list(e):
        if color.get(u, 0):
            continue
        stack, cur = [], u
        while cur is not None and color.get(cur, 0) == 0:
            color[cur] = 1
            stack.append(cur)
            cur = e.get(cur)
        if cur is not None and color.get(cur, 0) == 1 and cur in stack:
            out.append(stack[stack.index(cur):])
        for x in stack:
            color[x] = 2
    return out


cycles = find_cycles(edge)
print("\n=== CYCLES in the combined demand graph ===")
print("  real: %d cycles, lengths %s, covering %d rows"
      % (len(cycles), sorted(len(c) for c in cycles), sum(len(c) for c in cycles)))

# ---- NULL: same rows, same name pool, reshuffled assignment -----------------
pool = [cand[a] for a in cand]
nulls = []
for seed in (11, 12, 13, 14, 15):
    rnd = random.Random(seed)
    sh = pool[:]
    rnd.shuffle(sh)
    c2 = dict(zip(cand.keys(), sh))
    e2 = {}
    for a, n in c2.items():
        ow = name_owner.get(n, [])
        if len(ow) == 1:
            e2[a] = ow[0]
    nulls.append(sum(len(c) for c in find_cycles(e2)))
print("  NULL (candidate names reshuffled over the same rows), 5 seeds: %s rows in cycles"
      % nulls)
print("  => cycle closure is %s"
      % ("LOAD-BEARING" if sum(len(c) for c in cycles) > 3 * (max(nulls) + 1)
         else "NOT discriminating -- do not use it as a channel"))

# ---- gates, per cycle -------------------------------------------------------
folds = json.load(open("/home/free/tmp/laneCT2/folds.json"))
def grpmap(d):
    m = {}
    for _k, v in d.items():
        if isinstance(v, list) and len(v) > 1:
            for va in v:
                m[int(va, 16) if isinstance(va, str) else va] = len(v)
    return m
reloc_g, shape_g = grpmap(folds["reloc"]), grpmap(folds["shape"])
import homeunit_pins as HU
pins = HU.Pins(HU.splits_base(HU.parse_splits()))
unit_syms, _su, _c = HU.build_symbol_index()
names_ct = collections.Counter(v for k, v in raw.items()
                               if k.startswith("0x") and isinstance(v, str))
dup_before = sum(1 for n, c in names_ct.items() if c > 1)

g = collections.Counter()
keep = []
for cyc in cycles:
    why = None
    homes = {}
    for x in cyc:
        t = recs[x].get("target")
        if t:
            tv = int(t, 16)
            if reloc_g.get(tv, 1) > 1:
                why = why or "FOLD: a target is a reloc-identical fold representative"
            elif shape_g.get(tv, 1) > 1:
                why = why or "FOLD: a target sits in a shape-identical group"
        if o.class_ok(x, folded[x]) is False:
            why = why or "ANCESTRY contradicts a class being kept"
        ac = T._adjustment_from_code(img, int(x, 16))
        an = T._adjustment_from_name(folded[x])
        if ac is None or an is None or ac != an:
            why = why or "ADJUSTMENT mismatch on a preserved vtordisp token"
        if cand[x].startswith("??_G") or cand[x].startswith("??_E"):
            why = why or "RENAME-COST: renaming INTO a deleting-dtor name"
        u = pins.owner(int(x, 16))
        if u is None:
            homes[x] = "unpinned"
        elif cand[x] in unit_syms.get(u, set()):
            homes[x] = "pinned:%s:DEFINED" % u
        else:
            homes[x] = "pinned:%s:NOTDEFINED" % u
            why = why or "HOME-UNIT: pinned unit's obj does not define a new name"
    if why:
        g[why] += 1
        continue
    keep.append((cyc, homes))
print("\n  gate ledger over %d cycles:" % len(cycles))
for k, v in g.most_common():
    print("     reject %-58s %d" % (k, v))
print("     SURVIVE %d cycles = %d row edits"
      % (len(keep), sum(len(c) for c, _h in keep)))

after = collections.Counter(names_ct)
edits = []
for cyc, homes in keep:
    for x in cyc:
        edits.append({"addr": x, "expect": folded[x], "new": cand[x],
                      "prov": prov[x], "home": homes[x], "cycle_len": len(cyc)})
for e in edits:
    after[e["expect"]] -= 1
    if after[e["expect"]] == 0:
        del after[e["expect"]]
    after[e["new"]] += 1
dup_after = sum(1 for n, c in after.items() if c > 1)
print("\nINJECTIVITY duplicate names before=%d after=%d introduced=%d"
      % (dup_before, dup_after, dup_after - dup_before))
assert dup_after == dup_before
assert len(set(e["addr"] for e in edits)) == len(edits)

json.dump(edits, open("/home/free/tmp/laneCT2/ct2_triples.json", "w"), indent=1)
print("\nEDITS: %d" % len(edits))
for cyc, homes in keep:
    print("  CYCLE len %d:" % len(cyc))
    for x in cyc:
        print("    %s [%s | %s]\n       - %s\n       + %s"
              % (x, prov[x], homes[x], folded[x][:70], cand[x][:70]))
