#!/usr/bin/env python3
"""W16-AD: the RETAIL-SIDE FOLD WITNESS, mechanised over every UNDECIDED_MASKED row.

THE QUESTION THIS ANSWERS, AND WHY THE CENSUS CANNOT
====================================================
An alias membership says "our spelling N folded into survivor S at retail X".
`tools/w16s_alias_census.py` leaves 643 of them `UNDECIDED_MASKED` because
retail@X compares EQ to BOTH our N and our S: the one word that would separate
them is a `b`/`bl` to a destination the map does not name, so `retail_compare`
skips it.

⛔ W16-AA's central finding is that this is a REPORTING gap, not an accuracy
exposure: the census computes its `/OPT:ICF` closure on OUR build, while the
fold it adjudicates happened in RETAIL's.  Where retail folded the callees and
our `/O1` objs do not, the closure under-merges and the parent lands here.

So the discriminating question is ONE LEVEL DOWN.  Our N calls `c_N` at the
discriminating offset; our S calls `c_S`.  The membership is TRUE iff retail
folded `c_N` and `c_S` too.

THE WITNESS
===========
Find a retail function whose address the map knows and which, in our build,
calls `c_N`; decode retail's `bl` at the corresponding offset; that is where
`c_N` LIVES in retail.  Do the same for `c_S`.

  same address  -> retail folded the callees   -> WITNESS_CONFIRMED
  diff address  -> retail kept them apart      -> WITNESS_REFUTED (see guards)

⛔ THE OFFSET CORRESPONDENCE IS NOT FREE, AND IS THE WHOLE VALIDITY CONDITION.
"our caller's offset o" equals "retail's offset o" only if the two bodies are
the same body.  A witness is therefore admitted ONLY when the caller itself
compares EQ to retail at its mapped address under `retail_compare`.  That single
requirement buys two things at once:
  * the offsets correspond, so the decoded `bl` really is the call we mean; and
  * the caller's MAP ROW is corroborated BY BYTES, so the instrument is not
    resting on a map whose clusters W16-AA measured as internally inconsistent
    (the Accomplishment/Goal case).
A caller that is merely size-equal is recorded as WEAK and never decides a row.

⛔ A REFUTATION NEEDS THE CARE A CONFIRMATION GETS.  Two different unnamed
destinations are NOT automatically two functions.  Before refuting:
  * both destinations must be `.pdata` BeginAddresses (X360 `.pdata` is
    BIG-ENDIAN) -- else one is a funclet, a thunk tail or an EH-prefix
    mis-carve, not a function start; and
  * the two retail bodies must NOT be identical relocation-normalised -- if they
    ARE, retail simply kept two copies (CD-7's 51-surplus class) and the pair
    says nothing either way about whether N and S are one body.
Failing either guard yields an INCONCLUSIVE verdict, never a withdrawal.

A SWEEP THAT ONLY EVER SAYS CONFIRMED IS THE CENSUS'S BLIND SPOT RE-IMPLEMENTED.
`--selftest` therefore reproduces W16-AA's hand verdicts AND requires a
constructed negative control -- two spellings whose callees are map-named at
distinct retail addresses -- to come back WITNESS_REFUTED.  It exits non-zero if
the refutation arm does not fire.
"""
import argparse
import collections
import glob
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "scripts"))
from comdat_bytes import comdats                                  # noqa: E402
from wrong_callee_triage import Image, load_sizes, masked_body    # noqa: E402
import w16s_alias_census as CEN                                   # noqa: E402
import w16aa_masked_diff as MD                                    # noqa: E402

BUILD_ID = "45410914"
CENSUS = ROOT / "docs/decomp/W16S_alias_census_2026-09-14.json"
MAX_CALLERS_SCANNED = 400      # bound the per-callee caller scan
MAX_WITNESSES = 6              # strong witnesses collected per side


# ---------------------------------------------------------------- our side
def our_index():
    """name -> {(fn_raw, fn_relocs)}, name -> objs, callee -> [(caller, off)]."""
    fns = collections.defaultdict(set)
    where = collections.defaultdict(set)
    callsites = collections.defaultdict(list)
    for p in glob.glob(str(ROOT / "build" / BUILD_ID / "src" / "**" / "*.obj"),
                       recursive=True):
        try:
            c = comdats(p)
        except Exception:
            continue
        for n, v in c.items():
            if not v.get("is_code"):
                continue
            rel = tuple(sorted((o, s, t) for o, s, t in (v["fn_relocs"] or [])
                               if s != "@comp.id"))
            fns[n].add((v["fn_raw"], rel))
            where[n].add(p)
            for o, s, _t in rel:
                if s != n:
                    callsites[s].append((n, o))
    return fns, where, callsites


# ---------------------------------------------------------------- retail side
def pdata_begins(img):
    """Set of retail .pdata BeginAddresses.  X360 .pdata is BIG-ENDIAN."""
    for name, sva, raw, rawsz in img.secs:
        if name == ".pdata":
            out = set()
            for i in range(0, rawsz - 7, 8):
                b = struct.unpack_from(">I", img.data, raw + i)[0]
                if b:
                    out.add(b)
            return out
    return set()


def decode_bl(img, va, off):
    """Decode retail's branch at va+off.  Returns dest VA or None."""
    o = img.off(va + off)
    if o is None:
        return None
    w = struct.unpack_from(">I", img.data, o)[0]
    if (w >> 26) != 18 or (w & 2):          # not b/bl, or AA=1
        return None
    d = w & 0x03FFFFFC
    if d & 0x02000000:
        d -= 0x04000000
    return (va + off + d) & 0xFFFFFFFF


class Witnesser:
    def __init__(self):
        self.fns, self.where, self.callsites = our_index()
        self.img = Image(ROOT / "orig" / BUILD_ID / "band.exe")
        self.size = load_sizes()
        smap = json.loads((ROOT / "scripts/target_symbol_map.json").read_text())
        self.byva, self.byname = {}, collections.defaultdict(list)
        for k, v in smap.items():
            try:
                a = int(k, 16)
            except (ValueError, TypeError):
                continue
            self.byva[a] = v
            self.byname[v].append(a)
        self.closure = CEN.icf_closure(self.fns)
        self.begins = pdata_begins(self.img)
        self._eqcache = {}
        self._wcache = {}

    # -- is our body for `name` the retail body at `va`?  (validity condition)
    def caller_is_eq(self, name, va):
        key = (name, va)
        if key in self._eqcache:
            return self._eqcache[key]
        res = ("ABSENT", None)
        vs = self.fns.get(name)
        if vs:
            best = "NOSIZE"
            for raw, rel in sorted(vs, key=lambda kv: (-len(kv[0]), kv[0], kv[1])):
                v, _d, _mo = CEN.retail_compare(self.img, self.size, self.byva,
                                                va, raw, rel, self.closure)
                if v == "EQ":
                    best = "EQ"
                    res = ("EQ", (raw, rel))
                    break
                if best in ("NOSIZE", "NOOFF") or (best == "SIZE" and v == "NE"):
                    best = v
            if res[0] != "EQ":
                res = (best, None)
        self._eqcache[key] = res
        return res

    def our_rep(self, name):
        vs = self.fns.get(name)
        if not vs:
            return None
        return max(vs, key=lambda kv: (len(kv[0]), kv[0], kv[1]))

    def retail_branch_sites(self, va):
        """Offsets of every b/bl (AA=0) in retail's function at `va`, in order."""
        n = self.size.get(va)
        o = self.img.off(va)
        if not n or o is None:
            return None
        out = []
        for i in range(n // 4):
            w = struct.unpack_from(">I", self.img.data, o + 4 * i)[0]
            if (w >> 26) == 18 and not (w & 2):
                out.append(i * 4)
        return out

    def witnesses(self, callee):
        """Retail addresses `callee` resolves to, evidenced by map-named callers.

        TWO TIERS, and the distinction is deliberate and asymmetric:

          STRONG  the caller's own body compares EQ to retail at its mapped
                  address, so offsets correspond WORD FOR WORD and the map row
                  is corroborated by bytes.
          MEDIUM  the caller is not EQ (we inline differently), but our body and
                  retail's contain the SAME NUMBER of b/bl sites, so the i-th
                  call corresponds to the i-th call.  This is the alignment
                  W16-AA used by hand on gi=1302's 3-call sequence.

        ⛔ A CONFIRMATION may rest on MEDIUM; a REFUTATION may not.  The costly
        error here is a false refutation -- withdrawing a sound membership
        closes a vein nobody reopens -- so the weaker alignment is admitted only
        where it agrees with the null hypothesis (the fold is real).
        """
        if callee in self._wcache:
            return self._wcache[callee]
        strong, medium, weak = [], [], []
        scanned = 0
        for caller, off in sorted(set(self.callsites.get(callee, []))):
            if len(strong) >= MAX_WITNESSES and len(medium) >= MAX_WITNESSES:
                break
            if scanned >= MAX_CALLERS_SCANNED:
                break
            vas = self.byname.get(caller)
            if not vas:
                continue
            scanned += 1
            for va in sorted(vas):
                verdict, _b = self.caller_is_eq(caller, va)
                rec = {"caller": caller, "caller_va": "0x%08x" % va, "off": off,
                       "caller_vs_retail": verdict}
                if verdict == "EQ":
                    dest = decode_bl(self.img, va, off)
                    if dest is None:
                        continue
                    rec["retail_dest"] = "0x%08x" % dest
                    rec["tier"] = "STRONG"
                    strong.append(rec)
                    continue
                # -- MEDIUM: index-align the call sequences
                r = self.our_rep(caller)
                rsites = self.retail_branch_sites(va)
                if r is None or rsites is None:
                    continue
                raw, rel = r
                ours = [o2 for o2, _s, _t in sorted(rel)
                        if o2 + 4 <= len(raw)
                        and (struct.unpack_from(">I", raw, o2)[0] >> 26) == 18]
                if off not in ours or len(ours) != len(rsites) or not ours:
                    weak.append(rec)
                    continue
                i = ours.index(off)
                dest = decode_bl(self.img, va, rsites[i])
                if dest is None:
                    continue
                rec["retail_dest"] = "0x%08x" % dest
                rec["tier"] = "MEDIUM"
                rec["call_index"] = i
                rec["n_calls"] = len(ours)
                rec["retail_off"] = rsites[i]
                medium.append(rec)
        out = {"strong": strong, "medium": medium, "weak": weak[:3],
               "n_callsites": len(set(self.callsites.get(callee, []))),
               "map_named": ["0x%08x" % a for a in self.byname.get(callee, [])]}
        self._wcache[callee] = out
        return out

    # -- refutation guards
    def guard_pair(self, a, b):
        """Is a genuine two-function separation, or an artifact?"""
        g = {"a_is_pdata_begin": a in self.begins,
             "b_is_pdata_begin": b in self.begins,
             "a_size": self.size.get(a), "b_size": self.size.get(b)}
        ma = masked_body(self.img, a, self.size)
        mb = masked_body(self.img, b, self.size)
        g["bodies_relocnorm_identical"] = (ma is not None and ma == mb)
        if not (g["a_is_pdata_begin"] and g["b_is_pdata_begin"]):
            g["verdict"] = "INCONCLUSIVE_NOT_BEGIN"
        elif g["bodies_relocnorm_identical"]:
            g["verdict"] = "INCONCLUSIVE_TWIN"       # CD-7's 51-surplus class
        else:
            g["verdict"] = "SEPARATE"
        return g

    # -- one discriminating (c_N, c_S) pair
    def adjudicate_pair(self, cN, cS, retail_dest_at_X=None):
        r = {"c_N": cN, "c_S": cS}
        if not cN or not cS:
            r["verdict"] = "ONE_SIDED"
            return r
        wN, wS = self.witnesses(cN), self.witnesses(cS)
        r["N_map_named"], r["S_map_named"] = wN["map_named"], wS["map_named"]
        r["N_callsites"], r["S_callsites"] = wN["n_callsites"], wS["n_callsites"]
        dsN = {w["retail_dest"] for w in wN["strong"]}
        dsS = {w["retail_dest"] for w in wS["strong"]}
        dmN = {w["retail_dest"] for w in wN["medium"]}
        dmS = {w["retail_dest"] for w in wS["medium"]}
        r["N_strong"] = sorted(dsN)
        r["S_strong"] = sorted(dsS)
        r["N_medium"] = sorted(dmN)
        r["S_medium"] = sorted(dmS)
        r["N_witness"] = wN["strong"][:2] + wN["medium"][:2]
        r["S_witness"] = wS["strong"][:2] + wS["medium"][:2]
        allN, allS = dsN | dmN, dsS | dmS
        if not allN or not allS:
            # ⛔ WHICH SIDE is unwitnessed is load-bearing, not bookkeeping.
            # "our folded spelling has no retail witness" is the signature of a
            # Ham/DC3-only type that retail does not contain at all (gi=95's
            # situation) -- the membership then forgives nothing live and is
            # NOT a wrong callee.  "the survivor is unwitnessed" is a different
            # and weaker state.  Collapsing them loses the distinction an
            # escalation lane needs.
            r["verdict"] = ("NO_WITNESS_BOTH" if not allN and not allS
                            else "NO_WITNESS_N" if not allN else "NO_WITNESS_S")
            r["survivor_witness_is_retail_dest_at_X"] = (
                bool(allS) and retail_dest_at_X in allS)
            return r
        if len(allN) > 1 or len(allS) > 1:
            # our ONE comdat resolving to several retail addresses: either the
            # 51-surplus unfolded-duplicate class, or the alignment is wrong.
            r["verdict"] = "INCONSISTENT"
            return r
        a, b = next(iter(allN)), next(iter(allS))
        r["three_way_with_retail_at_X"] = (retail_dest_at_X is not None
                                           and a == b == retail_dest_at_X)
        if a == b:
            r["verdict"] = "CONFIRMED"
            r["tier"] = "STRONG" if (dsN and dsS) else "MEDIUM"
            r["retail_dest"] = a
            return r
        # -- candidate refutation.  STRONG on BOTH sides is mandatory.
        if not (dsN and dsS):
            r["verdict"] = "DISAGREE_MEDIUM_ONLY"
            return r
        g = self.guard_pair(int(a, 16), int(b, 16))
        r["guards"] = g
        r["verdict"] = "REFUTED" if g["verdict"] == "SEPARATE" else g["verdict"]
        return r


CLOSABLE = "a:CLOSABLE-BY-NAMING"
NAMED = "a:already-named"


def row_verdict(rec, W):
    """Aggregate a membership's discriminators into ONE verdict."""
    if rec.get("status") == "MISSING_COMDAT":
        rec["witness_verdict"] = "MISSING_COMDAT"
        return rec
    ds = rec.get("discriminators", [])
    closable = [d for d in ds if d.get("channel") == CLOSABLE]
    nb = sum(1 for d in ds if str(d.get("channel", "")).startswith("b:"))
    named = sum(1 for d in ds if d.get("channel") == NAMED)
    rec["n_channel_a_closable"], rec["n_channel_b"], rec["n_already_named"] = \
        len(closable), nb, named
    pairs = []
    for d in closable:
        p = W.adjudicate_pair(d.get("N_target"), d.get("S_target"),
                              d.get("retail_dest"))
        p["offset"] = d["offset"]
        p["retail_dest_at_X"] = d.get("retail_dest")
        pairs.append(p)
    rec["pairs"] = pairs
    cats = [p["verdict"] for p in pairs]
    if not closable:
        if nb:
            rec["witness_verdict"] = "CHANNEL_B"
        elif named:
            rec["witness_verdict"] = "ALREADY_NAMED"
        else:
            rec["witness_verdict"] = "NO_DISCRIMINATOR"
    elif "REFUTED" in cats:
        rec["witness_verdict"] = "WITNESS_REFUTED"
    elif any(str(c).startswith("INCONCLUSIVE") or c == "INCONSISTENT"
             or c == "DISAGREE_MEDIUM_ONLY" for c in cats):
        rec["witness_verdict"] = "WITNESS_INCONCLUSIVE"
    elif all(c == "CONFIRMED" for c in cats):
        rec["witness_verdict"] = ("WITNESS_CONFIRMED" if nb == 0
                                  else "WITNESS_CONFIRMED_RESIDUAL_B")
    elif "CONFIRMED" in cats:
        rec["witness_verdict"] = "WITNESS_PARTIAL"
    elif all(c == "NO_WITNESS_N" for c in cats):
        rec["witness_verdict"] = "NO_WITNESS_FOLDED_SIDE"
    elif all(c == "NO_WITNESS_S" for c in cats):
        rec["witness_verdict"] = "NO_WITNESS_SURVIVOR_SIDE"
    else:
        rec["witness_verdict"] = "NO_WITNESS_BOTH"
    return rec


def load_um():
    census = json.loads(CENSUS.read_text())
    return [r for r in census if r["verdict"] == "UNDECIDED_MASKED"]


def sweep(W, rows, limit=None):
    out, cache = [], {}
    for i, r in enumerate(rows):
        if limit and i >= limit:
            break
        key = (r["addr"], r["survivor"], r["folded"])
        if key in cache:
            rec = dict(cache[key])
            rec["gi"] = r["gi"]
            rec["bytes"] = r["bytes"]
            rec["duplicate_of_key"] = True
        else:
            rec = row_verdict(MD.analyze(r, W.fns, W.where, W.img, W.size,
                                         W.byva, W.closure), W)
            cache[key] = rec
        out.append(rec)
        if (i + 1) % 50 == 0:
            print("  ... %d/%d" % (i + 1, len(rows)), file=sys.stderr)
    return out


# ------------------------------------------------------------------ selftest
# (gi, W16-AA's hand verdict, the brief's literal expectation of THIS tool)
# ⛔ Two of these expectations are MIS-SPECIFIED, and the reason matters more
# than the score: the brief asks the BRANCH witness to reproduce verdicts
# W16-AA reached with OTHER instruments on rows this one structurally cannot
# see.  Recording that is the point of running the test; tuning the tool until
# the table goes green would destroy the only evidence that the instrument has
# a defined reach.
HAND = [
    (444,  "FOLD CONFIRMED", "confirmed"),
    (1302, "FOLD CONFIRMED", "confirmed"),
    (1050, "FOLD CONFIRMED", "confirmed"),
    (730,  "retail supports cross-T folding", "confirmed"),
    (95,   "NOT a retail fold; sound identification; LEAVE", "not-confirmed"),
]


def find_control_pair(W, want_same):
    """Two REAL callees whose retail destinations are known and (un)equal.

    The negative control must be built from bytes, not invented: both callees
    must have a STRONG witness, the two destinations must be distinct, and the
    refutation guards must call them SEPARATE.  If no such pair exists the
    control is VACUOUS and the selftest must say so rather than pass.
    """
    cands = []
    order = sorted(W.callsites.items(), key=lambda kv: (-len(set(kv[1])), kv[0]))
    for callee, sites in order[:600]:
        if not W.fns.get(callee):
            continue
        w = W.witnesses(callee)
        d = {x["retail_dest"] for x in w["strong"]}
        if len(d) == 1:
            cands.append((callee, next(iter(d))))
        if len(cands) >= 40:
            break
    if want_same:
        return (cands[0][0], cands[0][0]) if cands else (None, None)
    for i in range(len(cands)):
        for j in range(i + 1, len(cands)):
            a, b = cands[i], cands[j]
            if a[1] == b[1]:
                continue
            g = W.guard_pair(int(a[1], 16), int(b[1], 16))
            if g["verdict"] == "SEPARATE":
                return a[0], b[0]
    return None, None


def selftest(W):
    rows = load_um()
    bygi = collections.defaultdict(list)
    for r in rows:
        bygi[int(r["gi"])].append(r)
    print("\n== SELF-TEST A: reproduce W16-AA's hand verdicts ==")
    hand_ok = 0
    for gi, aa, expect in HAND:
        rs = bygi.get(gi, [])
        if not rs:
            print("  gi=%-5d MISSING from the UNDECIDED_MASKED set" % gi)
            continue
        for r in rs:
            rec = row_verdict(MD.analyze(r, W.fns, W.where, W.img, W.size,
                                         W.byva, W.closure), W)
            v = rec["witness_verdict"]
            conf = v.startswith("WITNESS_CONFIRMED")
            ok = conf if expect == "confirmed" else not conf
            hand_ok += bool(ok)
            print("  gi=%-5d %-6s tool=%-32s W16-AA=%s"
                  % (gi, "OK" if ok else "MISS", v, aa))
            print("           channels: a_closable=%d b=%d named=%d | pairs=%s"
                  % (rec.get("n_channel_a_closable", 0), rec.get("n_channel_b", 0),
                     rec.get("n_already_named", 0),
                     [p["verdict"] for p in rec.get("pairs", [])]))

    print("\n== SELF-TEST B: POSITIVE control (same callee both sides) ==")
    a, b = find_control_pair(W, want_same=True)
    pos = W.adjudicate_pair(a, b) if a else {"verdict": "VACUOUS"}
    print("  c_N=c_S=%s -> %s" % (a, pos["verdict"]))
    pos_ok = pos["verdict"] == "CONFIRMED"

    print("\n== SELF-TEST C: NEGATIVE control -- the refutation arm MUST fire ==")
    a, b = find_control_pair(W, want_same=False)
    if not a:
        print("  VACUOUS: no pair of map-witnessed callees at distinct, "
              "guard-SEPARATE retail addresses could be constructed")
        neg_ok = False
    else:
        neg = W.adjudicate_pair(a, b)
        print("  c_N=%s\n  c_S=%s\n  -> %s  (N@%s  S@%s)"
              % (a, b, neg["verdict"], neg.get("N_strong"), neg.get("S_strong")))
        print("  guards: %s" % json.dumps(neg.get("guards", {})))
        neg_ok = neg["verdict"] == "REFUTED"
    print("\nSELFTEST hand_reproduced=%d/%d positive_control=%s "
          "negative_control=%s" % (hand_ok, len(HAND) + 1,
                                   "PASS" if pos_ok else "FAIL",
                                   "FIRED" if neg_ok else "DID-NOT-FIRE"))
    return 0 if (neg_ok and pos_ok) else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--sweep", action="store_true")
    ap.add_argument("--gi", type=int, action="append")
    ap.add_argument("--limit", type=int)
    ap.add_argument("-o", "--out")
    args = ap.parse_args()
    W = Witnesser()
    if args.selftest:
        sys.exit(selftest(W))
    rows = load_um()
    if args.gi:
        rows = [r for r in rows if int(r["gi"]) in set(args.gi)]
    out = sweep(W, rows, args.limit)
    if args.out:
        Path(args.out).write_text(json.dumps(out, indent=1) + "\n")
    tot = collections.Counter(r["witness_verdict"] for r in out)
    byb = collections.Counter()
    for r in out:
        byb[r["witness_verdict"]] += int(r["bytes"])
    print("\n%-36s %6s %10s" % ("verdict", "rows", "bytes"))
    for k in sorted(tot, key=lambda k: -byb[k]):
        print("%-36s %6d %10d" % (k, tot[k], byb[k]))
    print("%-36s %6d %10d" % ("TOTAL", sum(tot.values()), sum(byb.values())))


if __name__ == "__main__":
    main()
