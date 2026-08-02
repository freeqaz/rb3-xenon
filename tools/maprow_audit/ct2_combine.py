#!/usr/bin/env python3
"""Lane CT-2: COMBINED demand graph over task-A and task-B evidence, and the
SWAP repairs that fall out of it.

The insight the two half-graphs hid: task B knows 'A should carry the name B
holds' and task A knows 'B should carry some other name'. Neither alone closes.
Together they can form a RECIPROCAL 2-CYCLE -- A and B simply exchange names.

Why a 2-cycle is the best repair available on this population:
  * the multiset of map names is UNCHANGED, so injectivity holds by construction
    and no duplicate can be introduced;
  * no free name is needed, which is precisely what blocked CS-1's 45;
  * each direction is corroborated by a DIFFERENT instrument -- A's by the branch
    channel + slot consensus, B's by the slot/co-thunk channel -- so neither row
    is named by the evidence that named the other.

⚠ CS-1 measured 'reciprocal 2-cycles: 0' and that is NOT contradicted: it looked
for swap partners within a reloc-masked SHAPE GROUP. These pairs are found by a
different relation (mutual name demand), and the two rows of a pair generally
sit in DIFFERENT shape groups because their `addi` immediates differ.
"""
import json, sys, os, collections, random
sys.path.insert(0, "/home/free/tmp/laneCT2")
from ct2_slot import SlotOracle, split_sig, synth_from_parts
from ct2_cothunk import sigparts
from ct2_family import FamOracle
WT = "/home/free/tmp/laneCT2/wt"
sys.path.insert(0, WT + "/tools"); sys.path.insert(0, WT + "/tools/maprow_audit")
os.chdir(WT)
import thunk_identity as T, thunk_oracle as TO
import homeunit_pins as HU
from retail_reader import Image as RImage

raw = json.load(open("scripts/target_symbol_map.json"))
folded = {k.lower() if k.startswith("0x") else k: v for k, v in raw.items()}
img = T.Image("orig/45410914/band.exe")
bij = set(a.lower() for a in raw["_bijection_arbitrary"])

MIN_SHARE = 10          # 99.4% precision (521/524) on the AGREE floor, null 4.8%
o = FamOracle(RImage("orig/45410914/band.exe")).prepare(folded).prepare2(MIN_SHARE, False)

pop = T.code_population(img, folded)
recs = {a: T.adjudicate_strict(img, folded, a, n) for a, n in pop.items()}
bodies = collections.defaultdict(list)
for a, r in recs.items():
    if r.get("target"):
        bodies[r["target"]].append(a)


def cot(a):
    tgt = recs[a].get("target")
    v = collections.Counter(); n = 0; unt = False
    for b in bodies.get(tgt, []):
        if b == a:
            continue
        p = sigparts(folded[b])
        if not p:
            continue
        v[p] += 1; n += 1
        if b not in bij:
            unt = True
    if not v:
        return None
    (top, c) = v.most_common(1)[0]
    if c != n or not unt:
        return None
    return synth_from_parts(folded[a], *top)


# ---- candidate name per row, from either task -------------------------------
cand, prov = {}, {}
tu = set(json.load(open("/home/free/tmp/laneCT2/tu107.json")))
for a in tu:                                   # TASK A: slot (tight) / co-thunk
    s, _v, _n = o.slot_synth(a, folded[a], margin=1.0, min_vtables=2)
    c = cot(a)
    if s and c and s != c:
        continue                               # channels disagree -> no candidate
    n = s or c
    if n and n != folded[a]:
        cand[a] = n
        prov[a] = "A:" + ("slot+cothunk" if (s and c) else ("slot" if s else "cothunk"))
for e in json.load(open("/home/free/tmp/laneCT2/taskB_rows.json")):   # TASK B
    if e["addr"] not in cand:
        cand[e["addr"]] = e["want"]
        prov[e["addr"]] = "B:branch+slot"
print("rows with a candidate: %d  (task A %d, task B %d)"
      % (len(cand), sum(1 for p in prov.values() if p.startswith("A")),
         sum(1 for p in prov.values() if p.startswith("B"))))

name_owner = collections.defaultdict(list)
for k, v in folded.items():
    if k.startswith("0x") and isinstance(v, str):
        name_owner[v].append(k)

edge = {}
for a, n in cand.items():
    ow = name_owner.get(n, [])
    if len(ow) == 1:
        edge[a] = ow[0]

# ---- reciprocal 2-cycles ----------------------------------------------------
pairs = []
for a, b in edge.items():
    if edge.get(b) == a and a < b:
        pairs.append((a, b))
print("\nRECIPROCAL 2-CYCLES (mutual name demand): %d" % len(pairs))

# ---- gates ------------------------------------------------------------------
folds = json.load(open("/home/free/tmp/laneCT2/folds.json"))
def grpmap(d):
    m = {}
    for _k, v in d.items():
        if isinstance(v, list) and len(v) > 1:
            for va in v:
                m[int(va, 16) if isinstance(va, str) else va] = len(v)
    return m
reloc_g, shape_g = grpmap(folds["reloc"]), grpmap(folds["shape"])
pins = HU.Pins(HU.splits_base(HU.parse_splits()))
unit_syms, _su, _c = HU.build_symbol_index()
names_ct = collections.Counter(v for k, v in raw.items()
                               if k.startswith("0x") and isinstance(v, str))
dup_before = sum(1 for n, c in names_ct.items() if c > 1)

g = collections.Counter()
keep = []
for a, b in pairs:
    why = None
    for x in (a, b):
        tva = recs[x].get("target")
        if tva:
            t = int(tva, 16)
            if reloc_g.get(t, 1) > 1:
                why = "FOLD: target is a reloc-identical fold representative"
            elif shape_g.get(t, 1) > 1:
                why = "FOLD: target sits in a shape-identical group"
        if o.class_ok(x, folded[x]) is False:
            why = why or "ANCESTRY contradicts the class being kept"
        ac = T._adjustment_from_code(img, int(x, 16))
        an = T._adjustment_from_name(folded[x])
        if ac is None or an is None or ac != an:
            why = why or "ADJUSTMENT mismatch on the preserved vtordisp token"
        if cand[x].startswith("??_G") or cand[x].startswith("??_E"):
            why = why or "RENAME-COST: renaming INTO a deleting-dtor name"
    if why:
        g[why] += 1
        continue
    # home unit
    hu = []
    for x in (a, b):
        u = pins.owner(int(x, 16))
        hu.append("unpinned" if u is None else
                  ("pinned:%s:%s" % (u, "DEFINED" if cand[x] in unit_syms.get(u, set())
                                     else "NOTDEFINED")))
    if any(h.endswith("NOTDEFINED") for h in hu):
        g["HOME-UNIT: pinned unit's obj does not define the new name"] += 1
        continue
    keep.append((a, b, hu))
print("  gate ledger on the %d pairs:" % len(pairs))
for k, v in g.most_common():
    print("     reject %-58s %d" % (k, v))
print("     SURVIVE %d pairs = %d row edits" % (len(keep), 2 * len(keep)))

# ---- injectivity ------------------------------------------------------------
after = collections.Counter(names_ct)
edits = []
for a, b, hu in keep:
    edits.append({"addr": a, "expect": folded[a], "new": cand[a],
                  "prov": prov[a], "home": hu[0], "pair": b})
    edits.append({"addr": b, "expect": folded[b], "new": cand[b],
                  "prov": prov[b], "home": hu[1], "pair": a})
for e in edits:
    after[e["expect"]] -= 1
    if after[e["expect"]] == 0:
        del after[e["expect"]]
    after[e["new"]] += 1
dup_after = sum(1 for n, c in after.items() if c > 1)
print("\nINJECTIVITY duplicate names before=%d after=%d introduced=%d"
      % (dup_before, dup_after, dup_after - dup_before))
assert dup_after == dup_before, "a swap MUST be duplicate-neutral"

json.dump(edits, open("/home/free/tmp/laneCT2/ct2_triples.json", "w"), indent=1)
print("\nEDITS: %d" % len(edits))
for a, b, hu in keep:
    print("  SWAP %s <-> %s   [%s | %s]" % (a, b, hu[0], hu[1]))
    print("     %s  ->  %s" % (folded[a][:64], cand[a][:64]))
    print("     %s  ->  %s" % (folded[b][:64], cand[b][:64]))
