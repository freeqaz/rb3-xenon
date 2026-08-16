#!/usr/bin/env python3
"""MAPID-1: dump the exact blocking evidence for each NEEDS_MAP_ID membership.

For each membership the layered adjudicator left at NEEDS_MAP_ID, l3_exact got
all the way to one relocated offset where RETAIL's target name is a placeholder
(fn_XXXXXXXX) and therefore cannot be compared to OUR target name. This prints,
per membership:

  * the blocking offset
  * RETAIL's placeholder target at that offset
  * OUR target name at the same offset   <-- the identification HYPOTHESIS
  * whether retail's body AT the placeholder is present in the target objs
    (i.e. whether the identification is even decidable from bytes)

That last column matters: if retail's body at fn_XXXXXXXX is not pinned, no
byte-level identification is possible and the membership is undecidable here.
"""
import json, sys
from pathlib import Path

wt = Path(sys.argv[1]).resolve()
sys.path.insert(0, str(wt / "tools"))
from alias_forgiveness_audit import Sides                                  # noqa

S = Sides(wt)
print("target bodies %d (mangled %d) | our bodies %d"
      % (len(S.traw), sum(1 for n in S.traw if n.startswith("?")), len(S.oraw)))
if sum(1 for n in S.traw if n.startswith("?")) < 1000:
    sys.exit("REFUSING: target objs look PRE-RENAMER")

mem = json.load(open(sys.argv[2]))
rows = [r for r in mem if r["verdict"] == "NEEDS_MAP_ID"]
print("NEEDS_MAP_ID memberships: %d" % len(rows))

by_addr = {}
for r in rows:
    tn, bn = r["survivor"], r["folded"]
    rt, ob = S.traw.get(tn), S.oraw.get(bn)
    if rt is None or ob is None:
        print("  !! bodies absent for %s / %s" % (tn[:40], bn[:40]))
        continue
    rraw, rrel = rt
    oraw_, orel = ob
    rm = {o: (n, t) for o, n, t in rrel}
    om = {o: (n, t) for o, n, t in orel}
    # replay l3_exact's loop to find the FIRST blocking offset
    for i in range(0, len(rraw), 4):
        if i not in rm:
            continue
        (rn, _), (on, _) = rm[i], om.get(i, ("<none>", None))
        if S.equiv(rn, on):
            continue
        if S.placeholder(rn) or S.placeholder(on):
            body = S.traw.get(rn)
            by_addr.setdefault(rn, []).append((tn, bn, i, on, body))
            break

print("\ndistinct blocking addresses: %d\n" % len(by_addr))
for addr in sorted(by_addr):
    ms = by_addr[addr]
    body = ms[0][4]
    print("=" * 100)
    print("%s   memberships=%d   retail body in target objs: %s"
          % (addr, len(ms), ("YES %d B" % len(body[0])) if body else "NO (unpinned)"))
    hyps = sorted({m[3] for m in ms})
    for h in hyps:
        ours = S.oraw.get(h)
        print("   OUR name at the blocking offset: %s   [our body: %s]"
              % (h, ("%d B" % len(ours[0])) if ours else "we do not compile it"))
    for tn, bn, i, on, _b in ms:
        print("   @0x%-4x  survivor=%s" % (i, tn[:72]))
        print("            folded  =%s" % bn[:72])
