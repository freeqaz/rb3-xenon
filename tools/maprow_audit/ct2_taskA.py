#!/usr/bin/env python3
"""Lane CT-2 TASK A: repair the 107 TARGET_UNNAMED rows via SLOT + CO-THUNK.

Two independently validated channels, both calibrated against a known-answer
population and a null that makes them fail:

  SLOT     full-name synthesis from a sibling vtable slot
           precision on branch-certified AGREE rows  97.8% (133/136)
           random-slot null                           3.7%
  CO-THUNK (method,sig) from other thunks hitting the same body
           reproduces the real target row            97.2% (281/289)
           random-body null                          10.3%

A candidate needs BOTH to exist and agree (gate 5: no single channel licenses a
name), then must clear the fold / ancestry / adjustment / rename-cost /
home-unit / injectivity gates. Every rejection is counted.
"""
import json, sys, os, re, collections, random
sys.path.insert(0, "/home/free/tmp/laneCT2")
from ct2_slot import SlotOracle, split_sig, synth_from_parts
from ct2_cothunk import sigparts
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
o = SlotOracle(RImage("orig/45410914/band.exe")).prepare(folded)

tu = json.load(open("/home/free/tmp/laneCT2/tu107.json"))
pop = T.code_population(img, folded)
recs = {a: T.adjudicate_strict(img, folded, a, n) for a, n in pop.items()}
bodies = collections.defaultdict(list)
for a, r in recs.items():
    if r.get("target"):
        bodies[r["target"]].append(a)

# ---- fold groups (gate 1: the FOLD SCAN, not the 27-row _icf_arbitrary list) --
folds = json.load(open("/home/free/tmp/laneCT2/folds.json"))
def grpmap(d):
    m = {}
    for _k, v in d.items():
        if isinstance(v, list) and len(v) > 1:
            for va in v:
                m[int(va, 16) if isinstance(va, str) else va] = len(v)
    return m
reloc_g, shape_g = grpmap(folds["reloc"]), grpmap(folds["shape"])
print("fold groups: reloc VAs %d  shape VAs %d" % (len(reloc_g), len(shape_g)))

# ---- home unit (gate 3) -------------------------------------------------------
pins = HU.Pins(HU.splits_base(HU.parse_splits()))
unit_syms, sym_units, _coll = HU.build_symbol_index(verbose=True)

names_ct = collections.Counter(v for k, v in raw.items()
                               if k.startswith("0x") and isinstance(v, str))
dup_before = sum(1 for n, c in names_ct.items() if c > 1)
print("duplicate NAMES in map before: %d" % dup_before)


def cothunk(addr):
    tgt = recs[addr].get("target")
    votes, n, untainted = collections.Counter(), 0, False
    for b in bodies.get(tgt, []):
        if b == addr:
            continue
        p = sigparts(folded[b])
        if not p:
            continue
        votes[p] += 1; n += 1
        if b not in bij:
            untainted = True
    if not votes:
        return None, "no co-thunk"
    (top, c) = votes.most_common(1)[0]
    if c != n:
        return None, "co-thunk split"
    if not untainted:
        return None, "co-thunk all-bijection (circular)"
    return top, None


rej = collections.Counter()
cands = {}
detail = []
for a in tu:
    inc = folded[a]
    s_name, _v, _nvt = o.slot_synth(a, inc, margin=1.0, min_vtables=2)
    c_parts, why = cothunk(a)
    c_name = synth_from_parts(inc, *c_parts) if c_parts else None
    d = {"addr": a, "incumbent": inc, "slot": s_name, "cothunk": c_name,
         "target": recs[a].get("target")}
    detail.append(d)
    if s_name is None and c_name is None:
        rej["NO CHANNEL (slot none, co-thunk none)"] += 1; continue
    if s_name is None:
        rej["SINGLE CHANNEL only co-thunk"] += 1; continue
    if c_name is None:
        rej["SINGLE CHANNEL only slot (%s)" % (why or "-")] += 1; continue
    if s_name != c_name:
        rej["CHANNELS DISAGREE"] += 1; continue
    cands[a] = s_name
print("\n=== TASK A funnel over %d TARGET_UNNAMED rows ===" % len(tu))
for k, v in rej.most_common():
    print("   reject %-40s %d" % (k, v))
print("   DOUBLY CORROBORATED                      %d" % len(cands))

# ---------------- downstream gates -------------------------------------------
g = collections.Counter()
keep = {}
for a, new in cands.items():
    inc = folded[a]
    if new == inc:
        g["noop (channels reproduce the incumbent)"] += 1; continue
    tva = int(recs[a]["target"], 16)
    if reloc_g.get(tva, 1) > 1:
        g["FOLD: target is a reloc-identical fold rep"] += 1; continue
    if shape_g.get(tva, 1) > 1:
        g["FOLD: target in a shape group (ambiguous)"] += 1; continue
    if o.class_ok(a, inc) is False:
        g["ANCESTRY contradicts the carried-over class"] += 1; continue
    ac, an = T._adjustment_from_code(img, int(a, 16)), T._adjustment_from_name(inc)
    if ac is None or an is None or ac != an:
        g["ADJUSTMENT mismatch on the preserved token"] += 1; continue
    if new.startswith("??_G") or new.startswith("??_E"):
        g["RENAME-COST: renaming INTO a deleting-dtor name"] += 1; continue
    if names_ct.get(new):
        g["NON-INJECTIVE (name already in map)"] += 1; continue
    keep[a] = new
print("\n   gate ledger on the %d doubly-corroborated:" % len(cands))
for k, v in g.most_common():
    print("      reject %-45s %d" % (k, v))
print("      SURVIVE                                       %d" % len(keep))

# ---------------- home-unit classification (gate 3) ---------------------------
print("\n   home-unit status of survivors:")
hu = collections.Counter()
final = {}
for a, new in keep.items():
    va = int(a, 16)
    u = pins.owner(va)
    if u is None:
        hu["UNPINNED -> no objdiff pairing, Delta-0 by construction"] += 1
        final[a] = (new, "unpinned")
    elif new in unit_syms.get(u, set()):
        hu["PINNED in %s and our obj DEFINES the new name" % u] += 1
        final[a] = (new, "pinned-defined:" + u)
    else:
        hu["PINNED but our obj does NOT define the new name -> would cost a match"] += 1
for k, v in hu.most_common():
    print("      %-70s %d" % (k, v))

# ---------------- injectivity + invariants (gate 4) ---------------------------
after = collections.Counter(names_ct)
for a, (new, _w) in final.items():
    after[folded[a]] -= 1
    if after[folded[a]] == 0:
        del after[folded[a]]
    after[new] += 1
dup_after = sum(1 for n, c in after.items() if c > 1)
print("\n   INJECTIVITY duplicate names before=%d after=%d introduced=%d"
      % (dup_before, dup_after, dup_after - dup_before))
assert len(set(v[0] for v in final.values())) == len(final), "new names not distinct"

json.dump(detail, open("/home/free/tmp/laneCT2/taskA_detail.json", "w"), indent=1)
json.dump([{"addr": a, "expect": folded[a], "new": n, "why": w}
           for a, (n, w) in sorted(final.items())],
          open("/home/free/tmp/laneCT2/taskA_triples.json", "w"), indent=1)
print("\nFINAL TASK-A EDITS: %d" % len(final))
for a, (n, w) in sorted(final.items()):
    print("  %s [%s]\n     - %s\n     + %s" % (a, w, folded[a][:76], n[:76]))
