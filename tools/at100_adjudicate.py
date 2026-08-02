#!/usr/bin/env python3
"""Adjudicate the at-100 wrong-callee census on TWO channels.

Channel 1 (xbin.py)   : does OUR body for X reproduce RETAIL bytes at addr(T)?
Channel 2 (locate.py) : where does OUR body for B actually live in retail,
                        found BY CONTENT rather than by the map?

Verdict lattice -- UNKNOWN is explicit at every step and never a fallthrough:

  BENIGN_direct   ours[B] == retail@addr(T)
                  the code at the called address IS our callee. Fold
                  representative or map misname; our call is right.
  BENIGN_located  addr(T) in hits(B)            (channel 2 agreeing)
  WRONG_2ch       ours[T] == retail@addr(T)  AND  hits(B) non-empty
                  AND addr(T) not in hits(B)
                  -> T verifiably occupies the called address, B verifiably
                     lives SOMEWHERE ELSE, so B and T are distinct surviving
                     retail functions and CANNOT have been ICF-folded. Two
                     independent channels, fold escape closed.
  WRONG_1ch       ours[T] == retail@addr(T) AND ours[B] differs, but channel 2
                  could not locate B. ONE channel only -- a lead, not a licence.
  UNKNOWN         everything else, with the reason recorded verbatim.
"""
import collections, json, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, str(__import__('pathlib').Path(__file__).resolve().parent))
from xbin_adjudicate import Xbin, Locator               # noqa: E402

ROOT = os.environ.get('CW2_ROOT', str(__import__('pathlib').Path(__file__).resolve().parent.parent))
PLACEHOLDER = re.compile(r'^_?(fn|lbl|jumptable|code|data|bss|rdata)_[0-9a-fA-F_]+$')
def forgiven(n): return bool(PLACEHOLDER.match(n)) or n.startswith('$')


def load_rep(p):
    r = json.load(open(p)); o = {}
    for u in r["units"]:
        stem = u["name"].split("/")[-1]
        for f in (u.get("functions") or []):
            o[(stem, f["name"])] = (f["match_percent_normalized"],
                                    f.get("fuzzy_match_percent", 0.0),
                                    int(f["size"]))
    return o, r.get("measures", {})


def main():
    rep, sitesf, outf = sys.argv[1], sys.argv[2], sys.argv[3]
    d, meas = load_rep(rep)
    sites = json.load(open(sitesf))["records"]
    x = Xbin(ROOT)
    loc = Locator(x, cap=20000)
    print("our bodies %d  map names %d  pdata %d" % (len(x.ours), len(x.name2addr), len(x.pd)))
    print("report: matched_functions=%s matched_code_percent=%s"
          % (meas.get("matched_functions"), meas.get("matched_code_percent")))

    import xbin_adjudicate as _xb
    hc = {}
    def rbody(va, mode):
        k = (va, mode)
        if k not in hc:
            ln = x.pd.get(va)
            hc[k] = _xb.fs.norm(x.img, va, ln, mode) if ln else None
        return hc[k]

    def cv4(t, b):
        ot, ob = x.ours.get(t), x.ours.get(b)
        if ot is not None and ob is not None:
            if ot == ob: return "c_fold_ours"
            if ot[0] == ob[0]: return "c_shapetwin_ours"
        ta, ba = x.name2addr.get(t), x.name2addr.get(b)
        if not ta: return "d_no_target_addr"
        if not ba: return "b_backlog"
        readable = False
        for p in ta:
            for q in ba:
                if p == q: return "c_same_address"
                hp, hq = rbody(p, "reloc"), rbody(q, "reloc")
                if hp is None or hq is None: continue
                readable = True
                if hp == hq: return "c_fold_retail"
        if not readable: return "d_body_unreadable"
        for p in ta:
            for q in ba:
                hp, hq = rbody(p, "shape"), rbody(q, "shape")
                if hp is not None and hp == hq: return "a_wrong_shapetwin"
        return "a_wrong"

    def adjudicate(t, b):
        ta = x.name2addr.get(t)
        if not ta:
            return "UNKNOWN", "no_addr_for_T", []
        if len(ta) > 1:
            return "UNKNOWN", "T_multiple_addrs", []
        va = ta[0]
        rb = x.test(b, va)
        rt = x.test(t, va)
        if rb == "MATCH":
            return "BENIGN_direct", "ours[B]==retail@addr(T)", [va]
        hs, st = loc.hits(b)
        if va in hs:
            return "BENIGN_located", "addr(T) in hits(B)", hs
        if rt == "MATCH" and hs:
            return "WRONG_2ch", "T at addr(T); B located elsewhere (%d)" % len(hs), hs
        if rt == "MATCH" and rb == "DIFF":
            return "WRONG_1ch", "T at addr(T); B unlocatable (%s)" % st, hs
        return "UNKNOWN", "B:%s T:%s loc:%s" % (rb, rt, st), hs

    kept = []
    for unit, fn, rows in sites:
        rs = [r for r in rows if not forgiven(r[1])]
        if rs: kept.append((unit, fn, rs))
    at100 = [(u, f, rs, d[(u, f)]) for u, f, rs in kept
             if (u, f) in d and d[(u, f)][0] == 100.0]
    pairs = collections.Counter()
    for u, f, rs, v in at100:
        for k, t, b in rs: pairs[(t, b)] += 1
    print("\nat-100 charged fns %d  sites %d  distinct pairs %d  bytes %d"
          % (len(at100), sum(len(r[2]) for r in at100), len(pairs),
             sum(r[3][2] for r in at100)))

    cls = {p: cv4(*p) for p in pairs}
    adj = {p: adjudicate(*p) for p in pairs}
    if loc.capped:
        print("CAPPED buckets (cap=%d): %s" % (loc.cap, dict(loc.capped)))
    else:
        print("no length bucket hit the cap (%d)" % loc.cap)

    ORDER = ["a_wrong", "a_wrong_shapetwin", "b_backlog", "d_body_unreadable",
             "d_no_target_addr", "c_shapetwin_ours", "c_fold_ours"]
    VORD = ["WRONG_2ch", "WRONG_1ch", "UNKNOWN", "BENIGN_located", "BENIGN_direct"]

    print("\n=== verdict x class (PAIRS / sites) ===")
    print("%-20s %s" % ("class", "  ".join("%14s" % v for v in VORD)))
    for c in ORDER:
        cp = collections.Counter(); cs = collections.Counter()
        for p, cc in cls.items():
            if cc == c:
                cp[adj[p][0]] += 1; cs[adj[p][0]] += pairs[p]
        if not sum(cp.values()): continue
        print("%-20s %s" % (c, "  ".join("%6d/%-7d" % (cp[v], cs[v]) for v in VORD)))

    # per-function roll-up, worst verdict dominates
    def fnverdict(rs, restrict=None):
        vs = [adj[(t, b)][0] for k, t, b in rs
              if restrict is None or cls[(t, b)] == restrict]
        if not vs: return None
        for w in ("WRONG_2ch", "WRONG_1ch", "UNKNOWN"):
            if w in vs: return w
        return "BENIGN"

    fn_cls = {}
    for u, f, rs, v in at100:
        vs = {cls[(t, b)] for k, t, b in rs}
        fn_cls[(u, f)] = next(c for c in ORDER if c in vs)

    for target in ("b_backlog", "a_wrong", "c_shapetwin_ours", "c_fold_ours"):
        sel = [(u, f, rs, v) for (u, f, rs, v) in at100 if fn_cls[(u, f)] == target]
        if not sel: continue
        cnt = collections.Counter(); byt = collections.Counter()
        for u, f, rs, v in sel:
            w = fnverdict(rs, target) or "UNKNOWN"
            cnt[w] += 1; byt[w] += v[2]
        tot, totb = sum(cnt.values()), sum(byt.values())
        print("\n=== CLASS %s : %d fns / %d B ===" % (target, tot, totb))
        for k in ("BENIGN", "WRONG_2ch", "WRONG_1ch", "UNKNOWN"):
            print("   %-10s fns %5d (%5.1f%%)   bytes %7d (%5.1f%%)"
                  % (k, cnt[k], 100.0 * cnt[k] / tot, byt[k], 100.0 * byt[k] / totb))

    print("\n=== class (b) UNKNOWN residue, reasons (pairs) ===")
    ur = collections.Counter()
    for p, c in cls.items():
        if c == "b_backlog" and adj[p][0] == "UNKNOWN":
            ur[adj[p][1]] += 1
    for k, n in ur.most_common(14):
        print("   %-52s %5d" % (k[:52], n))

    print("\n=== WRONG_2ch pairs (two independent channels) ===")
    w2 = [(p, pairs[p], adj[p]) for p in pairs if adj[p][0] == "WRONG_2ch"]
    w2.sort(key=lambda z: -z[1])
    print("total %d pairs / %d sites" % (len(w2), sum(n for _p, n, _a in w2)))
    for (t, b), n, a in w2[:20]:
        print("  %4d sites  T=%s\n              B=%s\n              %s -> %s"
              % (n, t[:96], b[:96], a[1], ["0x%08x" % h for h in a[2][:4]]))

    json.dump({"pairs": [[t, b, cls[(t, b)], adj[(t, b)][0], adj[(t, b)][1],
                          ["0x%08x" % h for h in adj[(t, b)][2][:8]], pairs[(t, b)]]
                         for t, b in pairs]}, open(outf, "w"))
    print("\nwrote " + outf)


if __name__ == "__main__":
    main()
