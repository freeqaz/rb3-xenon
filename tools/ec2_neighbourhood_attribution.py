#!/usr/bin/env python3
"""EC-2: size MISATTRIBUTION in pinned units via the ADDRESS-NEIGHBOURHOOD oracle.

RB3 retail has no LTCG, so TU spatial grouping in `.text` is preserved.  A
function's near neighbours are therefore overwhelmingly from its own object.
So: for a blocker row at VA, look at the map-named functions immediately around
it.  If NONE of them shares a class with the unit's own identified classes, the
pin is attribution-suspect.

WHY THIS IS ICF-IMMUNE: it never looks at the body.  ICF folds identical
bodies; it does not move a function's neighbours.

⚠ THIS IS A SUSPICION, NOT A CLASSIFIER.  Template/thunk COMDAT pools genuinely
interleave classes from many TUs, so the flag fires on healthy rows too.  The
run therefore ALWAYS measures the UNTREATED population -- the same statistic on
rows that are already at 100% in the same units -- and reports the enrichment.
A flag with enrichment ~1x is measuring pooling, not misattribution.
"""
import argparse, collections, json, pathlib, re, sys

ANON_RX = re.compile(r"^fn_([0-9A-Fa-f]{8})$")


def cls_of(sym):
    """Owning class of an MSVC mangled name: ?meth@Class@@... / ??1Class@@..."""
    if not sym or not sym.startswith("?"):
        return None
    s = sym
    if s.startswith("??"):
        m = re.match(r"^\?\?[_0-9A-Za-z]{1,2}([A-Za-z_][\w]*)@@", s)
        if m:
            return m.group(1)
        m = re.match(r"^\?\?\$[^@]+@[^@]*@@", s)
        return None
    m = re.match(r"^\?[^@?]+@([A-Za-z_][\w]*)@", s)
    return m.group(1) if m else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    ap.add_argument("--census", required=True)
    ap.add_argument("--buckets", default="COMPLETABLE")
    ap.add_argument("--k", type=int, default=6, help="neighbours each side")
    ap.add_argument("--window", type=lambda x: int(x, 0), default=0x600)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()

    root = pathlib.Path(a.root).resolve()
    amap = json.loads((root / "scripts/target_symbol_map.json").read_text())
    pairs = sorted((int(k, 16), v) for k, v in amap.items() if k.startswith("0x"))
    addrs = [p[0] for p in pairs]
    import bisect

    rev = collections.defaultdict(list)
    for va, n in pairs:
        rev[n].append(va)

    cen = json.loads(pathlib.Path(a.census).read_text())
    rep = json.loads((root / "build/45410914/report.json").read_text())
    runits = {u["name"]: u for u in rep["units"]}
    want = set(a.buckets.split(","))

    def neigh_classes(va):
        i = bisect.bisect_left(addrs, va)
        out = []
        for j in range(max(0, i - a.k), min(len(pairs), i + a.k + 1)):
            nva, nn = pairs[j]
            if nva == va or abs(nva - va) > a.window:
                continue
            c = cls_of(nn)
            if c:
                out.append(c)
        return out

    rows = []
    for c in cen["units"]:
        if c["bucket"] not in want:
            continue
        un = c["unit"]
        ru = runits.get(un)
        if not ru:
            continue
        fns = ru.get("functions") or []
        own = {x for x in (cls_of(f["name"]) for f in fns) if x}
        for f in fns:
            vas = rev.get(f["name"], [])
            if len(vas) != 1:
                continue          # ambiguous or unmapped -> no neighbourhood claim
            va = vas[0]
            nc = neigh_classes(va)
            if not nc:
                continue          # no named neighbours -> cannot adjudicate
            # ⚠ VACUITY GUARD 1: a unit whose symbols are all FREE functions has an
            # empty own-class set, so "no neighbour shares our class" is true by
            # construction and says nothing.  FFT / Main / Rnd_NG are entirely this.
            if not own:
                continue
            # ⚠ VACUITY GUARD 2: template/STL COMDATs are packed into shared pools
            # by the linker, so their neighbours are legitimately foreign.  Keep
            # them, but in a SEPARATE stratum -- never inside the headline rate.
            is_tmpl = f["name"].startswith("??$") or "?$vector@" in f["name"] \
                or "?$_Rb_tree@" in f["name"] or "?$list@" in f["name"] \
                or "@stlpmtx_std@@" in f["name"]
            agree = bool(set(nc) & own)
            rows.append(dict(unit=un, sym=f["name"], va=hex(va), size=int(f["size"]),
                             mpn=f["match_percent_normalized"],
                             stratum="CHARGED" if f["match_percent_normalized"] < 100.0 else "CONTROL",
                             tmpl=is_tmpl,
                             own_cls=sorted(own)[:6], neigh=sorted(set(nc)),
                             foreign=not agree,
                             top_neigh=collections.Counter(nc).most_common(2)))
    pathlib.Path(a.out).write_text(json.dumps(rows, indent=1))

    print("=== HEADLINE: non-template rows in units with a real own-class set ===")
    for st in ("CHARGED", "CONTROL"):
        sel = [r for r in rows if r["stratum"] == st and not r["tmpl"]]
        nf = sum(1 for r in sel if r["foreign"])
        print(f"{st:8s} adjudicable={len(sel):4d}  neighbourhood-foreign={nf:4d} "
              f"= {100.0*nf/len(sel) if sel else 0:5.2f}%")
    ch = [r for r in rows if r["stratum"] == "CHARGED" and not r["tmpl"]]
    co = [r for r in rows if r["stratum"] == "CONTROL" and not r["tmpl"]]
    print("\n=== TEMPLATE/STL COMDAT stratum (quote separately, never pooled) ===")
    for st in ("CHARGED", "CONTROL"):
        sel = [r for r in rows if r["stratum"] == st and r["tmpl"]]
        nf = sum(1 for r in sel if r["foreign"])
        print(f"{st:8s} adjudicable={len(sel):4d}  foreign={nf:4d} = "
              f"{100.0*nf/len(sel) if sel else 0:5.2f}%")
    pc = 100.0 * sum(1 for r in ch if r["foreign"]) / len(ch) if ch else 0
    pn = 100.0 * sum(1 for r in co if r["foreign"]) / len(co) if co else 0
    print(f"\nENRICHMENT (charged / untreated) = {pc/pn if pn else float('inf'):.2f}x")
    if pn and pc / pn < 1.5:
        print("** NOT DISCRIMINATING: the untreated population trips this flag at a "
              "comparable rate, so it is measuring COMDAT pooling, not attribution. **")
    print("\n--- neighbourhood-FOREIGN blockers ---")
    for r in sorted(ch, key=lambda r: r["unit"]):
        if r["foreign"]:
            print(f"{r['unit'][:34]:34s} {r['va']} {r['size']:5d}B mpn{r['mpn']:6.1f} "
                  f"{r['sym'][:44]:44s} neigh={r['top_neigh']}")
    print(f"\nwrote {a.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
