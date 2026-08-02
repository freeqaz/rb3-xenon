#!/usr/bin/env python3
"""Lane CT-2 (A): the SLOT channel, extended to FULL-NAME synthesis, + its nulls.

WHY IT MUST BE FULL-NAME. CS-1's slot channel voted on the METHOD only, which is
enough to ADJUDICATE a row (method differs => defect) but NOT enough to REPAIR
one: a thunk name carries the method AND the parameter signature, and for a
TARGET_UNNAMED row there is no named target body to lift a signature from. The
sibling vtable slot supplies both, because within a vtable family slot i is the
same virtual -- same method, same signature -- in every member.

SYNTHESIS = class + vtordisp token from the THUNK ITSELF (both independently
corroborated: class by RTTI ancestry, adjustment by the addi channel) + method
and signature from the SIBLING SLOT. So no single channel supplies the whole
name; that is how gate 5 ("masked-byte equality alone cannot license a name") is
satisfied here.

VALIDATION (the number that licenses everything downstream): run slot-only
synthesis on rows the INDEPENDENT branch channel certified AGREE, and require it
to reproduce the incumbent name EXACTLY. Null = vote at a random slot index.
"""
import json, sys, os, re, collections, random
WT = "/home/free/tmp/laneCT2/wt"
sys.path.insert(0, WT + "/tools"); sys.path.insert(0, WT + "/tools/maprow_audit")
os.chdir(WT)
import thunk_identity as T, thunk_oracle as TO

# the adjustment field is EITHER an A-P run terminated by '@' OR a bare digit.
# CS-1's cs1_synth demanded '[0-9A-P]+@', which cannot match the bare-digit form
# and also greedily eats into the signature; that is 2 of its "unsynthesizable".
_VT = re.compile(r'\$([0-4])PPPPPPPM@(?:[A-P]+@|[0-9])')
_SPECIAL_PRE = ("??_E", "??_G", "??_D", "??1", "??0")


def split_sig(name):
    """full mangled member name -> (method_token, access_char, sig_tail) or None.

    '?Export@RndDir@@UAAXPAVDataArray@@_N@Z' -> ('Export','U','AAXPAVDataArray@@_N@Z')
    '??_GRndDir@@UAAPAXI@Z'                  -> ('??_G','U','AAPAXI@Z')
    """
    if not isinstance(name, str) or not name.startswith("?"):
        return None
    if T.thunk_kind(name) is not None:
        return None                       # a thunk sibling has no plain signature
    if "@@" not in name:
        return None
    for pre in _SPECIAL_PRE:
        if name.startswith(pre):
            rest = name[len(pre):]
            if "@@" not in rest:
                return None
            _cls, sig = rest.split("@@", 1)
            if not sig or sig[0] not in "ABEFIJMNQRUV":
                return None
            return (pre, sig[0], sig[1:])
    if name.startswith("??"):
        return None
    head, sig = name.split("@@", 1)
    if not sig or sig[0] not in "ABEFIJMNQRUV":
        return None
    parts = head[1:].split("@")
    if len(parts) < 2:
        return None
    return (parts[0], sig[0], sig[1:])


def synth_from_parts(thunkname, meth, sig_tail):
    """class + vtordisp token from the thunk; method + signature from elsewhere."""
    m = _VT.search(thunkname)
    if not m:
        return None
    cls = TO.qcls(thunkname)
    if not cls:
        return None
    tok = m.group(0)                       # '$4PPPPPPPM@<adj>@'
    if meth in _SPECIAL_PRE:
        return f"{meth}{cls}@@{tok}{sig_tail}"
    return f"?{meth}@{cls}@@{tok}{sig_tail}"


class SlotOracle(TO.Oracle):
    def slot_votes(self, addr, jitter=False, rnd=random):
        """Votes from sibling vtables at this thunk's slot index.

        Returns (Counter[(meth,sig_tail)], n_distinct_voting_vtables, n_votes).
        Only PLAIN (non-thunk) sibling rows vote, since a thunk sibling carries a
        vtordisp signature, not the virtual's own signature.
        """
        A = int(addr, 16)
        votes = collections.Counter()
        vts = set()
        for _cls, lst in self.attr.get(A, {}).items():
            for (vt, i, _off) in lst:
                j = rnd.randrange(max(len(self.vt[vt]), 1)) if jitter else i
                for w in self.family(vt):
                    sl = self.vt[w]
                    if j >= len(sl) or sl[j] == A:
                        continue
                    p = split_sig(self.tmap.get('0x%08x' % sl[j]))
                    if p:
                        votes[(p[0], p[2])] += 1
                        vts.add(w)
        return votes, len(vts), sum(votes.values())

    def slot_synth(self, addr, name, margin=0.75, min_vtables=1, jitter=False, rnd=random):
        votes, nvt, tot = self.slot_votes(addr, jitter=jitter, rnd=rnd)
        if not votes or nvt < min_vtables:
            return None, votes, nvt
        (meth, sig), n = votes.most_common(1)[0]
        if n / tot < margin:
            return None, votes, nvt
        return synth_from_parts(name, meth, sig), votes, nvt


def main():
    raw = json.load(open("scripts/target_symbol_map.json"))
    folded = {k.lower() if k.startswith("0x") else k: v for k, v in raw.items()}
    img = T.Image("orig/45410914/band.exe")
    from retail_reader import Image as RImage
    o = SlotOracle(RImage("orig/45410914/band.exe")).prepare(folded)

    # ---- how much does the uppercase fold matter TO THE SLOT CHANNEL? --------
    up = {k.lower() for k in raw if k.startswith("0x") and k != k.lower()}
    slotvas = {'0x%08x' % va for _vt, sl in o.vt.items() for va in sl}
    print("uppercase map keys that are a VTABLE SLOT value:", len(up & slotvas),
          "-> slot channel MUST use the folded map (CS-1's TO.load() did not)")

    # ---- calibration population: EVERY thunk row the branch channel resolves --
    pop = T.code_population(img, folded)
    recs = {a: T.adjudicate_strict(img, folded, a, n) for a, n in pop.items()}
    agree = [a for a, r in recs.items() if r["verdict"] == "AGREE"]
    md = [a for a, r in recs.items() if r["verdict"] == "METHOD_DIFFERS"]
    tun = [a for a, r in recs.items() if r["verdict"] == "TARGET_UNNAMED"]
    print("\ncode population %d  AGREE %d  METHOD_DIFFERS %d  TARGET_UNNAMED %d"
          % (len(pop), len(agree), len(md), len(tun)))

    def rate(addrs, jitter=False, seed=0, **kw):
        rnd = random.Random(seed)
        hit = cov = 0
        for a in addrs:
            s, _v, _n = o.slot_synth(a, folded[a], jitter=jitter, rnd=rnd, **kw)
            if s is None:
                continue
            cov += 1
            hit += (s == folded[a])
        return hit, cov, len(addrs)

    print("\n=== INSTRUMENT VALIDATION: slot-only FULL-NAME synthesis ===")
    print("    (does it rebuild, character for character, a name the INDEPENDENT")
    print("     branch channel already certified correct?)")
    for lbl, addrs in (("AGREE  (known-good floor)", agree),
                       ("METHOD_DIFFERS (known-bad)", md)):
        h, c, n = rate(addrs)
        print("  %-27s exact %4d / %4d covered = %6.1f%%   (coverage %d/%d = %.1f%%)"
              % (lbl, h, c, 100.0 * h / max(c, 1), c, n, 100.0 * c / n))
    print("  -- NULL: vote at a RANDOM slot index --")
    for seed in (11, 12, 13):
        h, c, n = rate(agree, jitter=True, seed=seed)
        print("  %-27s exact %4d / %4d covered = %6.1f%%"
              % ("random-slot null s=%d" % seed, h, c, 100.0 * h / max(c, 1)))

    json.dump({"agree": agree, "md": md, "tun": tun},
              open("/home/free/tmp/laneCT2/pops.json", "w"), indent=1)
    print("\nwrote pops.json")


if __name__ == "__main__":
    main()
