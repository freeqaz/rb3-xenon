#!/usr/bin/env python3
"""W16-UH: census of call sites whose RETAIL callee is an unnamed placeholder,
adjudicated on retail bytes.

WHY.  The shipped ruler (`functionRelocDiffs=name_check`) FORGIVES a relocation
whose retail target is a placeholder (`fn_8XXXXXXX`): objdiff cannot charge a
name it does not have.  So a paired row whose `bl` reaches an unnamed retail
function scores the same whatever we call there.  W16-UG found the instance that
motivated this lane: `SystemInit` called `GlitchFinder::Init` where retail calls
the Stage Kit init at `0x82522608`, in a row reading 100.

WHAT IT DOES.
  1. POPULATION.  Every report.json row in the requested rings whose name is
     defined in BOTH the dtk target obj and our compiled obj of its unit (a
     paired row).  Rings come from `scripts/native_scope_map.classify` split the
     way `scope_ledger2.tier` splits it (IN-CORE / IN-SOON / IN-RB3ENG / VIA-DC3,
     plus OUT-NET / OUT-360-OTHER; OUT-QUAZAL and OUT-XDK are skipped).
  2. ALIGNMENT.  Call relocations (type 0x06, `bl`/`b`) are aligned slot for
     slot the way tools/wrong_callee_census.py does: STRICT (same size and same
     (offset,type) sequence) or BL (same number of type-0x06 relocations, by
     index -- relocation order is code order).  Rows that align neither way are
     counted as UNALIGNED, and the placeholder calls inside them are counted, so
     the blind spot is sized rather than hidden.
  3. SITES.  An aligned slot whose retail name is `fn_<A>` and whose name on our
     side is a real symbol F.
  4. VERDICT per distinct (A, F), on retail bytes:
       PROVEN        `icf_pair_adjudicate.chase(fn_<A>, F)`: retail's body at A
                     equals our compiled F modulo relocated fields AND every
                     relocation target agrees (recursively).  Right callee.
       BYTES-EQUAL   the chase stopped, but at depth 0 the masked bytes and
                     relocation shape are equal.  UNDECIDED, hand review: K3b
                     measures this test accepting 2.5% of deranged pairs.
       NO-OURS       F is not defined in any compiled obj, so there are no bytes
                     of ours to compare.  Decided by hand (lane doc).
       MAPPED-ELSEWHERE  F is in target_symbol_map at an address B != A: retail
                     has F at B and calls A here.  Wrong callee unless A is a
                     retail twin of B.
       BODY-ELSEWHERE    our F's masked body + relocation names equal a DIFFERENT
                     retail function (named or not) -- retail has F's code at
                     some B != A.
       REFUTED       none of the above; F's bytes are not retail's body at A.
                     Either the wrong callee or our F does not match yet --
                     decided by hand.
CONTROLS (`--controls`; each asserted to be able to fail):
  K1 KNOWN ANSWER.  (fn_82522608, GlitchFinder::Init) -- W16-UG's wrong callee
     -- must NOT read PROVEN.  And the walk must find SystemInit's site at
     0x82522608 (now naming StageKitInit), or the population is broken.
  K2 POSITIVE.  Aligned slots whose retail callee is NAMED N and ours is N,
     where N's own row is at fuzzy 100: the same comparator on (N, N) at depth 0
     must read PROVEN on nearly all of them -- the check can pass.
  K3 NEGATIVE.  The same retail callees, each paired with a DIFFERENT callee of
     ours of similar size drawn from another slot: the comparator must read
     REFUTED on nearly all -- the check can fail.

    python3 tools/placeholder_callee_census.py --controls
    python3 tools/placeholder_callee_census.py --out ~/tmp/w16uh/census.json

Read-only.  Mutates no build input; is not itself a build input.  Build the tree
first: a reflinked worktree's target objs are pre-renamer.
"""
import argparse
import collections
import json
import os
import random
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "scripts"))
from coff_bodies_ext import function_bodies_ext  # noqa: E402
import icf_pair_adjudicate as P  # noqa: E402
import native_scope_map as N  # noqa: E402

BL = 0x06
FN = re.compile(r'^fn_([0-9A-Fa-f]{8})$')
ANON = re.compile(r'^_?(fn|lbl|jumptable|code|data|bss|rdata|except_data)_[0-9a-fA-F_]+$')
SKIP_TIERS = {"OUT-QUAZAL", "OUT-XDK"}
DC3_SRC = Path("/home/free/code/milohax/dc3-decomp")
KNOWN_A = 0x82522608
KNOWN_WRONG = "GlitchFinder::Init"

_DC3 = None


def tier(src):
    """scope_ledger2.tier (W16-OV), verbatim in behaviour."""
    global _DC3
    if _DC3 is None:
        _DC3 = set()
        for r, _d, fs in os.walk(DC3_SRC / "src/system"):
            for n in fs:
                _DC3.add(os.path.join(r, n)[len(str(DC3_SRC)) + 1:].lower())
    t = N.classify(src)
    if t == '360-ONLY':
        s = src or ''
        if s.startswith('src/network/net/'):
            return 'OUT-NET'
        if s.startswith('src/network/'):
            return 'OUT-QUAZAL'
        if s.startswith('src/xdk/'):
            return 'OUT-XDK'
        return 'OUT-360-OTHER'
    if t == 'NATIVE-VIA-DC3':
        return 'VIA-DC3' if (src or '').lower() in _DC3 else 'IN-RB3ENG'
    return {'NATIVE-CORE': 'IN-CORE', 'NATIVE-SOON': 'IN-SOON'}.get(t, t)


def anon(n):
    return bool(ANON.match(n)) or n.startswith('$')


def index_fns(path):
    out = {}
    for name, raw, relocs, _e in function_bodies_ext(path):
        out.setdefault(name, (len(raw), relocs))
    return out


def align(t, o):
    tsz, trl = t
    osz, orl = o
    tb = [(off, n) for off, n, ty in trl if ty == BL]
    ob = [(off, n) for off, n, ty in orl if ty == BL]
    if tsz == osz and [(a, c) for a, _b, c in trl] == [(a, c) for a, _b, c in orl]:
        return "STRICT", list(zip(tb, ob))
    if tb and len(tb) == len(ob):
        return "BL", list(zip(tb, ob))
    return "UNALIGNED", None


def load_units():
    cfg = json.loads((ROOT / "objdiff.json").read_text())
    od = {u["name"]: u for u in cfg["units"]}
    rep = json.loads((ROOT / "build/45410914/report.json").read_text())
    return od, rep


def walk_population(od, rep, want_named=False):
    """Yield per-row records.  Each: dict(unit, tier, fn, fuzzy, mpn, size, align,
    sites=[(off, A, F)], named=[(off, N)] if want_named, unaligned_fn=k)."""
    for u in rep["units"]:
        src = (u.get("metadata") or {}).get("source_path")
        t = tier(src)
        if t in SKIP_TIERS:
            continue
        cfg = od.get(u["name"], {})
        tp, bp = cfg.get("target_path"), cfg.get("base_path")
        if not tp or not bp:
            continue
        tp, bp = ROOT / tp, ROOT / bp
        if not (tp.exists() and bp.exists()):
            continue
        tf, of = index_fns(tp), index_fns(bp)
        for f in (u.get("functions") or []):
            name = f["name"]
            if name not in tf or name not in of:
                continue
            kind, slots = align(tf[name], of[name])
            rec = dict(unit=u["name"], tier=t, src=src, fn=name,
                       fuzzy=float(f.get("fuzzy_match_percent", 0) or 0),
                       mpn=float(f.get("match_percent_normalized", 0) or 0),
                       size=int(f.get("size", 0)), align=kind, sites=[], named=[],
                       unaligned_fn=0)
            if slots is None:
                rec["unaligned_fn"] = sum(1 for _o, n, ty in tf[name][1]
                                          if ty == BL and FN.match(n))
                yield rec
                continue
            for (toff, tn), (_oo, bn) in slots:
                m = FN.match(tn)
                if m and not anon(bn):
                    rec["sites"].append((toff, int(m.group(1), 16), bn))
                elif want_named and tn == bn and not anon(tn):
                    rec["named"].append((toff, tn))
            yield rec


def map_index():
    m = json.loads((ROOT / "scripts/target_symbol_map.json").read_text())
    by_name = collections.defaultdict(set)
    by_addr = {}
    for a, n in m.items():
        if not a.startswith("0x"):
            continue
        va = int(a, 16)
        for x in (n if isinstance(n, list) else [n]):
            if x:
                by_name[x].add(va)
                by_addr.setdefault(va, x)
    return by_name, by_addr


class Judge:
    def __init__(self):
        self.mapped = P.load_mapped()
        self.tgt, self.ours = P.load_sides()
        self.by_name, self.by_addr = map_index()
        self._tidx = None

    def tidx(self):
        if self._tidx is None:
            self._tidx = collections.defaultdict(list)
            for n, (mb, rl, _s) in self.tgt.items():
                self._tidx[mb].append(n)
        return self._tidx

    def chase(self, s, f):
        trace = []
        try:
            ok = P.chase(self.tgt, self.ours, s, f, self.mapped, out=trace)
        except Exception as e:  # noqa: BLE001 -- reported, never swallowed silently
            return False, [("EXC", repr(e))]
        return bool(ok), trace

    def bytes_equal(self, s, f):
        rt, ob = self.tgt.get(s), self.ours.get(f)
        return bool(rt and ob and rt[0] == ob[0] and not P.vacuous(rt)
                    and [(o, t) for o, _n, t in rt[1]] == [(o, t) for o, _n, t in ob[1]])

    def verdict(self, A, F):
        s = "fn_%08X" % A
        d = dict(A="0x%08X" % A, F=F)
        rt = self.tgt.get(s)
        d["retail_size"] = rt[2] if rt else None
        ob = self.ours.get(F)
        d["our_size"] = ob[2] if ob else None
        mapped_at = sorted(self.by_name.get(F, ()))
        d["F_mapped_at"] = ["0x%08X" % x for x in mapped_at]
        if rt is None:
            return "NO-RETAIL", d
        if ob is not None:
            ok, trace = self.chase(s, F)
            d["chase"] = [list(map(str, x)) for x in trace[:12]]
            d["chase_stop"] = str(trace[-1][1]) if trace else ""
            if ok:
                return "PROVEN", d
            # K2 measured the chase failing on 4.1% of CORRECT callees, mostly on
            # a slot two levels down.  Depth-0 equality of masked bytes and
            # relocation shape is not a proof, but it is not a refutation either.
            if (rt[0] == ob[0] and [(o, t) for o, _n, t in rt[1]]
                    == [(o, t) for o, _n, t in ob[1]] and not P.vacuous(rt)
                    and not (mapped_at and A not in mapped_at)):
                return "BYTES-EQUAL", d
        if mapped_at and A not in mapped_at:
            return "MAPPED-ELSEWHERE", d
        if ob is None:
            return "NO-OURS", d
        # does our F's code exist elsewhere in retail?  (masked body equal AND
        # relocation names equal -- names only where ours are not placeholders)
        twins = [n for n in self.tidx().get(ob[0], []) if n != s
                 and [(o, ty) for o, _n, ty in self.tgt[n][1]] == [(o, ty) for o, _n, ty in ob[1]]
                 and all(rn == on or anon(rn) for (_o, rn, _t), (_o2, on, _t2)
                         in zip(self.tgt[n][1], ob[1]))]
        if twins and not P.vacuous(ob):
            d["F_body_at"] = twins[:6]
            return "BODY-ELSEWHERE", d
        d["masked_equal"] = (rt[0] == ob[0])
        return "REFUTED", d


def census(out_path):
    od, rep = load_units()
    rows = list(walk_population(od, rep))
    J = Judge()
    by_pair = collections.defaultdict(list)
    st = collections.Counter()
    tiers = collections.defaultdict(collections.Counter)
    for r in rows:
        tc = tiers[r["tier"]]
        tc["paired_rows"] += 1
        tc["rows_" + r["align"]] += 1
        if r["align"] == "UNALIGNED":
            tc["unaligned_fn_slots"] += r["unaligned_fn"]
            if r["unaligned_fn"]:
                tc["unaligned_rows_with_fn"] += 1
        if r["sites"]:
            tc["rows_with_sites"] += 1
            if r["fuzzy"] == 100.0:
                tc["rows_with_sites_at_fuzzy100"] += 1
        for off, A, F in r["sites"]:
            tc["sites"] += 1
            by_pair[(A, F)].append(dict(unit=r["unit"], tier=r["tier"], fn=r["fn"],
                                        off=off, fuzzy=r["fuzzy"], mpn=r["mpn"],
                                        size=r["size"], align=r["align"]))
    verdicts = {}
    for (A, F), sites in sorted(by_pair.items()):
        v, d = J.verdict(A, F)
        d["sites"] = sites
        verdicts["%08X|%s" % (A, F)] = dict(verdict=v, **d)
        st[v] += 1
        for s in sites:
            tiers[s["tier"]]["sites_" + v] += 1
    out = dict(tiers={k: dict(v) for k, v in tiers.items()}, pair_verdicts=dict(st),
               pairs=verdicts)
    Path(out_path).parent.mkdir(parents=True, exist_ok=True)
    Path(out_path).write_text(json.dumps(out, indent=1))
    print("tier          paired  STRICT     BL  UNALGN  rows_w/sites  sites  unaligned_fn_slots")
    for t in sorted(tiers):
        c = tiers[t]
        print("%-13s %6d %7d %6d %7d %13d %6d %8d" % (
            t, c["paired_rows"], c["rows_STRICT"], c["rows_BL"], c["rows_UNALIGNED"],
            c["rows_with_sites"], c["sites"], c["unaligned_fn_slots"]))
    print("distinct (A,F) pairs: %d  verdicts: %s" % (len(by_pair), dict(st)))
    print("wrote", out_path)
    return 0


def controls(n_sample):
    od, rep = load_units()
    rows = list(walk_population(od, rep, want_named=True))
    J = Judge()
    fails = []
    # ---- K1 known answer
    known_site = [(r["fn"], F) for r in rows for (_o, A, F) in r["sites"] if A == KNOWN_A]
    print("K1 walk: sites calling fn_%08X: %s" % (KNOWN_A, known_site))
    if not any("SystemInit" in fn for fn, _F in known_site):
        fails.append("K1: the walk did not find SystemInit's call to fn_%08X" % KNOWN_A)
    gf = [n for n in J.ours if n.startswith("?Init@GlitchFinder@@")]
    if not gf:
        fails.append("K1: GlitchFinder::Init is not in our objs -- control VACUOUS")
    for g in gf:
        v, d = J.verdict(KNOWN_A, g)
        print("K1 known wrong callee (fn_%08X, %s): %s  retail %s B / ours %s B" % (
            KNOWN_A, g, v, d.get("retail_size"), d.get("our_size")))
        if v == "PROVEN":
            fails.append("K1: the known wrong callee read PROVEN")
    # ---- K2 / K3 on the named-agreeing population
    rown = {}
    for u in rep["units"]:
        for f in (u.get("functions") or []):
            rown[f["name"]] = float(f.get("fuzzy_match_percent", 0) or 0)
    named = sorted({n for r in rows for (_o, n) in r["named"]
                    if rown.get(n) == 100.0 and n in J.tgt and n in J.ours
                    and not P.vacuous(J.tgt[n])})
    rnd = random.Random(20261007)
    rnd.shuffle(named)
    named = named[:n_sample]
    pos = sum(1 for n in named if J.chase(n, n)[0])
    beq = sum(1 for n in named if J.bytes_equal(n, n))
    print("K2 positive: (N, N), N's row at fuzzy 100: %d / %d PROVEN (%.1f%%); "
          "%d / %d BYTES-EQUAL at depth 0" % (
              pos, len(named), 100.0 * pos / max(1, len(named)), beq, len(named)))
    if len(named) < 200:
        fails.append("K2: only %d named slots -- control VACUOUS" % len(named))
    elif pos < 0.9 * len(named):
        fails.append("K2: the comparator PROVES only %d/%d correct callees" % (pos, len(named)))
    # K3: derange by size -- pair each N with the nearest-size OTHER callee
    bysize = sorted(named, key=lambda n: J.ours[n][2])
    neg = falsepos = beq_fp = 0
    fp_list = []
    for i, n in enumerate(bysize):
        other = bysize[i + 1] if i + 1 < len(bysize) else bysize[i - 1]
        if other == n:
            continue
        neg += 1
        if J.chase(n, other)[0]:
            falsepos += 1
            fp_list.append((n, other))
        if J.bytes_equal(n, other):
            beq_fp += 1
    print("K3 negative: (N, nearest-size other callee): %d / %d PROVEN (%.2f%%)" % (
        falsepos, neg, 100.0 * falsepos / max(1, neg)))
    for x in fp_list[:10]:
        print("   K3 PROVEN:", x)
    # K3b is a MEASUREMENT, not a gate: depth-0 byte equality ignores relocation
    # names, so it accepts template twins (every `ClassName` body is identical
    # but for the static it loads).  This is why BYTES-EQUAL is UNDECIDED in the
    # census and never counted as a right callee.
    print("K3b depth-0 BYTES-EQUAL on the same deranged pairs: %d / %d (%.2f%%) "
          "-- NOT a discriminator" % (beq_fp, neg, 100.0 * beq_fp / max(1, neg)))
    if neg < 200:
        fails.append("K3: only %d negative pairs -- control VACUOUS" % neg)
    elif falsepos > 0.01 * neg:
        fails.append("K3: the comparator PROVES %d/%d wrong callees" % (falsepos, neg))
    if fails:
        print("CONTROLS FAILED:")
        for f in fails:
            print("  -", f)
        return 3
    print("CONTROLS PASS (K1 known answer, K2 can pass, K3 can fail)")
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(Path.home() / "tmp/w16uh/census.json"))
    ap.add_argument("--controls", action="store_true")
    ap.add_argument("--sample", type=int, default=1500)
    a = ap.parse_args()
    if a.controls:
        return controls(a.sample)
    return census(a.out)


if __name__ == "__main__":
    sys.exit(main())
