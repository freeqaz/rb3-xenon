#!/usr/bin/env python3
"""Lane CT-2: attack the DIAGNOSED cause of low slot coverage -- an over-inclusive
vtable family. min_share=3 linked one row to 416 voters spanning 28 methods.

Two refinements, each re-validated on the AGREE floor (precision) with the
random-slot null, then scored for conjunction yield on the 107:
  (a) raise min_share
  (b) let only PRIMARY vtables (COL offset 0 -- a base class's own vtable) vote,
      since a vtordisp thunk sits in a SECONDARY vtable that mirrors some base's
      layout, and it is that base's own vtable which names the slot.
"""
import json, sys, os, collections, random
sys.path.insert(0, "/home/free/tmp/laneCT2")
from ct2_slot import SlotOracle, split_sig, synth_from_parts
from ct2_cothunk import sigparts
WT = "/home/free/tmp/laneCT2/wt"
sys.path.insert(0, WT + "/tools"); sys.path.insert(0, WT + "/tools/maprow_audit")
os.chdir(WT)
import thunk_identity as T, thunk_oracle as TO
from retail_reader import Image as RImage

raw = json.load(open("scripts/target_symbol_map.json"))
folded = {k.lower() if k.startswith("0x") else k: v for k, v in raw.items()}
img = T.Image("orig/45410914/band.exe")
bij = set(a.lower() for a in raw["_bijection_arbitrary"])


class FamOracle(SlotOracle):
    def prepare2(self, min_share, primary_only):
        self.min_share = min_share
        self.primary_only = primary_only
        self._fam = {}
        self.offset_of = {vt: off for vt, (nm, off, sl) in self.vt_slots.items()}
        return self

    def family(self, vt, min_share=None):
        ms = self.min_share
        key = (vt, ms, self.primary_only)
        if key in self._fam:
            return self._fam[key]
        cnt = collections.Counter()
        for i, va in enumerate(self.vt[vt]):
            for w in self.idx[(i, va)]:
                if w != vt:
                    cnt[w] += 1
        out = {w for w, c in cnt.items() if c >= ms
               and (not self.primary_only or self.offset_of.get(w, 1) == 0)}
        self._fam[key] = out
        return out


o = FamOracle(RImage("orig/45410914/band.exe")).prepare(folded)
pop = T.code_population(img, folded)
recs = {a: T.adjudicate_strict(img, folded, a, n) for a, n in pop.items()}
bodies = collections.defaultdict(list)
for a, r in recs.items():
    if r.get("target"):
        bodies[r["target"]].append(a)
agree = [a for a, r in recs.items() if r["verdict"] == "AGREE"]
tu = json.load(open("/home/free/tmp/laneCT2/tu107.json"))


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


cot_cache = {a: cot(a) for a in set(agree) | set(tu)}

print("  min_share primary |  AGREE precision (exact/cov)  cov%% | null | 107:slot 107:BOTH")
for primary in (False, True):
    for ms in (3, 6, 10, 16):
        o.prepare2(ms, primary)
        h = c = 0
        for a in agree:
            s, _v, _n = o.slot_synth(a, folded[a], margin=1.0, min_vtables=2)
            if s is None:
                continue
            c += 1; h += (s == folded[a])
        rnd = random.Random(12)
        nh = nc = 0
        for a in agree:
            s, _v, _n = o.slot_synth(a, folded[a], margin=1.0, min_vtables=2,
                                     jitter=True, rnd=rnd)
            if s is None:
                continue
            nc += 1; nh += (s == folded[a])
        ns = nb = 0
        for a in tu:
            s, _v, _n = o.slot_synth(a, folded[a], margin=1.0, min_vtables=2)
            if s is None:
                continue
            ns += 1
            cc = cot_cache[a]
            if cc and cc == s:
                nb += 1
        print("  %9d %7s |  %4d/%4d = %5.1f%%  %5.1f%% | %4.1f%% |   %3d      %3d"
              % (ms, primary, h, c, 100.0 * h / max(c, 1), 100.0 * c / len(agree),
                 100.0 * nh / max(nc, 1), ns, nb))
