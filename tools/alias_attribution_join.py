#!/usr/bin/env python3
"""Join EXACT per-group ablation attribution against retail-byte adjudication.

    python3 tools/alias_attribution_join.py --wt <worktree> \
        --ablate ~/tmp/alias2_ablate.jsonl --fell ~/tmp/alias2_fell.json

WHAT THIS ANSWERS (lane ALIAS-2, 2026-08-16)
--------------------------------------------
GROUNDED-1 split the alias-forgiven bytes by adjudicating (target,base) pairs
drawn from a NAME-KEYED site census, and 11.00% came back `UNATTRIBUTED` because
the census cannot pair an anonymous target symbol.  This joins two instruments
that between them need no name matching at all:

  * WHICH group forgives a row  -- from ABLATION (tools/alias_group_ablate.py):
    remove one group, see which rows drop.  Anonymous rows attribute exactly as
    well as named ones, because nothing is matched by name.
  * WHETHER that group is earned -- from `verdict(survivor, folded)` on RETAIL
    BYTES (alias_forgiveness_audit.Sides), which takes a membership, not a site.

NECESSITY, NOT CREDIT.  A row is reported under EVERY group whose removal drops
it, so the per-group byte columns overlap and must not be summed.  Integrity
turns on necessity: a row's forgiveness is earned only if EVERY group it needs is
earned, so a row's verdict is the WORST verdict over its necessary groups, and a
group's verdict is the WORST over its memberships.  Both directions are
deliberately conservative -- this audit should fail toward withdrawal.

★ THE RESIDUAL UNATTRIBUTED CLASS IS REAL AND IS MEASURED, NOT ASSUMED.  A row
forgiven REDUNDANTLY by two groups falls under NEITHER single-group ablation
(removing one leaves the other covering the site).  Such rows are reported as
REDUNDANT_COVER with their bytes.  Reporting 0 without checking would be exactly
the vacuity this lane exists to avoid.
"""
import argparse, collections, json, os, sys
from pathlib import Path

PROVEN = {"L1_T1", "L2_RECURSIVE", "L3_EXACT", "L4_OURSIDE", "L5_INCONSISTENCY"}
RANK = ["CONTRADICTED", "NEEDS_SOURCE", "NEEDS_MAP_ID", "UNPROVEN", "PROVEN"]


def worst(vs):
    for r in RANK:
        if r in vs:
            return r
    return "PROVEN"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--wt", required=True)
    ap.add_argument("--ablate", required=True)
    ap.add_argument("--fell", required=True)
    ap.add_argument("--memberships", required=True,
                    help="tools/alias_membership_adjudicate.py --out JSON")
    ap.add_argument("--out", default="")
    a = ap.parse_args()
    wt = Path(a.wt).resolve()

    fell = {tuple(k.split("\t")): sz for k, sz in
            json.load(open(os.path.expanduser(a.fell)))}
    abl = [json.loads(l) for l in
           open(os.path.expanduser(a.ablate)).read().splitlines() if l.strip()]
    print("ablation records: %d groups   measured fall set: %d rows / %d B"
          % (len(abl), len(fell), sum(fell.values())))

    row2groups = collections.defaultdict(set)
    for r in abl:
        for k, _sz in r["fell"]:
            row2groups[tuple(k.split("\t"))].add(r["i"])
    covered = set(row2groups)
    redundant = {k: v for k, v in fell.items() if k not in covered}
    print("rows attributed by ablation : %d / %d B" %
          (len(covered & set(fell)), sum(fell[k] for k in covered if k in fell)))
    print("REDUNDANT_COVER (fell in FULL-vs-EMPTY but under no single ablation): "
          "%d rows / %d B" % (len(redundant), sum(redundant.values())))

    # ---- membership verdicts, precomputed on RETAIL BYTES --------------------
    groups = json.loads((wt / "scripts/symbol_aliases.json").read_text())["groups"]
    mem = json.load(open(os.path.expanduser(a.memberships)))
    bygroup = collections.defaultdict(list)
    for m in mem:
        bygroup[m["i"]].append(m)
    print("membership verdicts loaded: %d over %d groups" % (len(mem), len(bygroup)))

    live = sorted({r["i"] for r in abl if r["fell_bytes"] > 0})
    print("groups forgiving >0 B: %d of %d" % (len(live), len(abl)))

    gverd, gdetail = {}, {}
    for i in live:
        det = [(m["folded"], m["verdict"], m["why"]) for m in bygroup.get(i, [])]
        vs = {m["cls"] for m in bygroup.get(i, [])}
        if not vs:
            vs = {"UNPROVEN"}            # emptied/withdrawn group declares no fold
        gverd[i] = worst(vs); gdetail[i] = det

    # ---- row verdict = worst over its necessary groups ----------------------
    cls_rows, cls_bytes = collections.Counter(), collections.Counter()
    for k, sz in fell.items():
        if k in redundant:
            cls_rows["REDUNDANT_COVER"] += 1; cls_bytes["REDUNDANT_COVER"] += sz; continue
        v = worst({gverd.get(i, "UNPROVEN") for i in row2groups[k]})
        cls_rows[v] += 1; cls_bytes[v] += sz
    tot = sum(cls_bytes.values())
    print("\nROW-LEVEL SPLIT OF THE %d ALIAS-FORGIVEN BYTES (ablation-attributed)" % tot)
    for c in ("PROVEN", "UNPROVEN", "NEEDS_SOURCE", "NEEDS_MAP_ID",
              "CONTRADICTED", "REDUNDANT_COVER"):
        if cls_rows[c] or c == "CONTRADICTED":
            print("  %-16s %5d rows %9d B  %5.2f%%"
                  % (c, cls_rows[c], cls_bytes[c], 100.0 * cls_bytes[c] / max(1, tot)))

    gb = {r["i"]: r["fell_bytes"] for r in abl}
    print("\nGROUPS BY VERDICT (necessity bytes; OVERLAPPING -- do not sum):")
    byv = collections.Counter()
    for i in live:
        byv[gverd[i]] += 1
    for v, n in byv.most_common():
        print("   %-14s %4d groups" % (v, n))
    print("\nNON-PROVEN GROUPS, largest first:")
    for i in sorted(live, key=lambda i: -gb[i]):
        if gverd[i] == "PROVEN":
            continue
        g = groups[i]
        print("  %8d B  [%s] %s" % (gb[i], gverd[i], (g.get("survivor") or "?")[:88]))
        for f, v, why in gdetail[i][:4]:
            if v not in PROVEN:
                print("             <- %s\n                %s %s" % (f[:86], v, str(why)[:90]))
    if a.out:
        json.dump({"gverd": {str(k): v for k, v in gverd.items()},
                   "cls_bytes": dict(cls_bytes), "cls_rows": dict(cls_rows),
                   "redundant_bytes": sum(redundant.values())},
                  open(os.path.expanduser(a.out), "w"), indent=1)
        print("\nwrote %s" % a.out)


if __name__ == "__main__":
    main()
