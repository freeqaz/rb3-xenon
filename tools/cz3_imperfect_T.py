#!/usr/bin/env python3
"""Lane CZ-3 item (A): WHAT ARE THE SRC_imperfect_T PAIRS MADE OF?

The brief calls this class "ordinary body-port work -- our body for T is
imperfect, not a map defect". But cy4_residue_attribute's OWN docstring says
"=> our decomp of T is wrong (OR THE MAP ROW IS)". Those are different claims
and only one instrument separates them, so build it rather than inherit either.

SRC_imperfect_T means, precisely:
    rt = Xbin.test(T, addr(T)) in {DIFF, SKIP:length_differs}
i.e. we DO compile a body named T, retail DOES have a .pdata extent at the
address the map assigns to T, and the two do not agree.

THE DISCRIMINATOR
-----------------
objdiff already tells us, independently, whether our T reproduces ITS OWN dtk
target obj. Cross those two verdicts:

  report mpn(T) < 100  &  byte-test fails  -> CONSISTENT: our body for T really
                                              is wrong. Ordinary body-port work,
                                              and already visible in the frontier.
  report mpn(T) == 100 &  byte-test fails  -> CONTRADICTION. objdiff says our T
                                              equals the target obj dtk carved,
                                              yet our T != retail@addr(T). Then
                                              addr(T) is not where T lives =>
                                              MAP/PIN defect, NOT a body defect.
  T absent from report.json                -> T is compiled but NOT TRACKED (no
                                              pin covers it). Fixing it cannot
                                              move the metric directly.

⚠ Gate 3 discipline: "absent from report" is its OWN bucket and is NEVER folded
into the body-defect class. UNCLASSIFIED is explicit and must stay 0.

THE SIZE TRIANGLE (also settles item C, UIPicture::Load)
--------------------------------------------------------
Three independent sizes exist for one function and they are NOT the same number:
    pd    = retail .pdata extent at addr(T)          (retail truth)
    rep   = report.json "size" for T                 (dtk's TARGET obj carve)
    ours  = len(our compiled body for T)             (MSVC)
`length_differs` fires on pd != ours. report.json size is the TARGET size (it is
NOT our size -- the "paired body size" vacuity), so rep vs pd isolates a
dtk-vs-pdata carving disagreement with our compiler taken out of the picture.

Read-only.
"""
import collections
import json
import os
import re
import sys
from pathlib import Path

ROOT = os.environ.get("CZ3_ROOT", ".")
sys.path.insert(0, os.path.join(ROOT, "tools"))
from xbin_adjudicate import Xbin                                   # noqa: E402
sys.path.insert(0, str(Path(ROOT).resolve() / "tools"))
from cy4_residue_attribute import attribute                        # noqa: E402
from cy4_weakext_adjudicate import build_index                     # noqa: E402

ADJ = sys.argv[1]
REPORT = sys.argv[2]
RESOLVED = sys.argv[3]
OUT = sys.argv[4] if len(sys.argv) > 4 else None


def category(n):
    """Coarse shape of a mangled name. Deliberately few, mutually exclusive buckets."""
    if re.match(r'^\?\?_[EG]', n):
        return "vector/scalar deleting dtor (??_E/??_G)"
    if '$4' in n or '$B' in n or re.search(r'\$\d', n):
        return "this-adjustor thunk"
    if n.startswith('??0'):
        return "constructor"
    if n.startswith('??1'):
        return "destructor"
    if n.startswith('??_7') or n.startswith('??_R'):
        return "vtable/RTTI data"
    if '?$' in n:
        return "template instantiation"
    if n.startswith('??'):
        return "operator/special"
    if n.startswith('?'):
        return "ordinary method/function"
    return "C symbol / other"


def main():
    x = Xbin(ROOT)
    weak, defined, _p, _n, _c = build_index(ROOT)

    decided = set()
    for r in json.load(open(RESOLVED))["rows"]:
        if r[5] != "UNKNOWN":
            decided.add((r[0], r[1]))

    # report.json: name -> list of (unit, mpn, fuzzy, target_size)
    rep = json.load(open(REPORT))
    byname = collections.defaultdict(list)
    for u in rep["units"]:
        for f in (u.get("functions") or []):
            byname[f["name"]].append((u["name"], f["match_percent_normalized"],
                                      f.get("fuzzy_match_percent", 0.0),
                                      int(f["size"])))

    pairs = json.load(open(ADJ))["pairs"]
    rows = []
    for T, B, cls, v, reason, _h, ns in pairs:
        if v != "UNKNOWN":
            continue
        if weak.get(B) == T and B not in defined:
            continue
        if (T, B) in decided:
            continue
        if attribute(reason) != "SRC_imperfect_T":
            continue
        ta = x.name2addr.get(T) or []
        va = ta[0] if len(ta) == 1 else None
        pd = x.pd.get(va) if va is not None else None
        our = x.ours.get(T)
        ours = len(our[0]) if our else None
        rt = x.test(T, va) if va is not None else "SKIP:no_addr"
        ent = byname.get(T, [])
        rows.append(dict(T=T, B=B, cls=cls, sites=ns, va=va, pd=pd, ours=ours,
                         rt=rt, rep=ent))

    print("=== SRC_imperfect_T : %d pairs / %d sites ==="
          % (len(rows), sum(r["sites"] for r in rows)))

    # ---------- 1. WHICH SUB-TEST FAILED --------------------------------
    print("\n--- 1. which leg of the byte test failed ---")
    c = collections.Counter(r["rt"] for r in rows)
    for k, n in c.most_common():
        print("   %-28s %5d  (%5.1f%%)" % (k, n, 100.0 * n / len(rows)))

    # ---------- 2. THE DISCRIMINATOR -------------------------------------
    print("\n--- 2. DISCRIMINATOR: does objdiff independently agree T is wrong? ---")
    disc = collections.Counter()
    discsites = collections.Counter()
    buckets = collections.defaultdict(list)
    for r in rows:
        ent = r["rep"]
        if not ent:
            k = "T_NOT_TRACKED (no pin covers T)"
        else:
            best = max(e[1] for e in ent)
            if best == 100.0:
                k = "CONTRADICTION (objdiff 100, bytes differ) => MAP/PIN"
            elif best >= 50.0:
                k = "CONSISTENT body defect (objdiff 50-99.99)"
            else:
                k = "CONSISTENT body defect (objdiff <50)"
        disc[k] += 1
        discsites[k] += r["sites"]
        buckets[k].append(r)
    tot = sum(disc.values())
    for k, n in disc.most_common():
        print("   %-52s %5d (%5.1f%%)  sites %5d" % (k, n, 100.0 * n / tot, discsites[k]))
    unc = tot - sum(disc.values())
    print("   %-52s %5d   <-- must be 0" % ("UNCLASSIFIED (explicit)", unc))

    # ---------- 3. SIZE TRIANGLE -----------------------------------------
    print("\n--- 3. SIZE TRIANGLE  pd(retail .pdata) / rep(dtk target) / ours(MSVC) ---")
    dl = collections.Counter()
    for r in rows:
        if r["pd"] is None or r["ours"] is None:
            dl["unmeasurable"] += 1
            continue
        dl[r["ours"] - r["pd"]] += 1
    print("   ours - pd  (only length_differs rows can be nonzero):")
    for k, n in sorted(dl.items(), key=lambda z: (isinstance(z[0], str), z[0])):
        print("      %-14s %5d" % (k, n))
    tri = collections.Counter()
    for r in rows:
        if not r["rep"] or r["pd"] is None:
            continue
        repsz = r["rep"][0][3]
        tri[(repsz - r["pd"])] += 1
    print("   rep - pd   (dtk target carve vs retail .pdata; OUR compiler not involved):")
    for k, n in sorted(tri.items()):
        print("      %-14s %5d" % (k, n))

    # ---------- 4. NAME SHAPE --------------------------------------------
    print("\n--- 4. what KIND of function is T ---")
    cc = collections.Counter(category(r["T"]) for r in rows)
    for k, n in cc.most_common():
        print("   %-42s %5d (%5.1f%%)" % (k, n, 100.0 * n / len(rows)))

    # ---------- 5. WHERE THE WORK IS -------------------------------------
    print("\n--- 5. top units owning the CONSISTENT body-defect rows (the workable ones) ---")
    work = [r for k, v in buckets.items() if k.startswith("CONSISTENT") for r in v]
    uc = collections.Counter()
    for r in work:
        uc[r["rep"][0][0]] += 1
    for k, n in uc.most_common(20):
        print("   %-52s %4d" % (k, n))
    print("   distinct T in workable set: %d" % len({r["T"] for r in work}))

    print("\n--- 6. sample of each bucket ---")
    for k in disc:
        print("  [%s]" % k)
        for r in buckets[k][:4]:
            e = r["rep"][0] if r["rep"] else None
            print("     T=%s" % r["T"][:88])
            print("        va=%s pd=%s ours=%s rt=%s  rep=%s"
                  % ("0x%08x" % r["va"] if r["va"] else None, r["pd"], r["ours"],
                     r["rt"], ("%s mpn=%.2f fz=%.2f sz=%d" % e) if e else None))

    if OUT:
        json.dump({"rows": [{k: v for k, v in r.items()} for r in rows]}, open(OUT, "w"))
        print("\nwrote %s" % OUT)


if __name__ == "__main__":
    main()
