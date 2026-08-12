#!/usr/bin/env python3
"""Size the `different_function` charge population honestly, from the RETAIL IMAGE.

`different_function` is rb3-xenon's whole residual at `name_check`: 3,487 charged
pairs, 10,637 sites, 8,473 functions exposed by it alone -- about thirteen times
everything else on the board across all three projects.  The question this
answers is what it actually IS: how much is an ICF fold (our spelling and
retail's denote one linked address, so the reference is identical after linking
and an alias states a fact), how much is a genuinely wrong callee, and how much
cannot be adjudicated with the evidence available.

WHY NOT MAP RESIDENCY
---------------------
The obvious split -- "both spellings map-resident at different VAs => wrong
callee" -- is unsound and was refuted on this tree (lane E, ba27bd1e).
`scripts/target_symbol_map.json` is a VA->name FUNCTION over a link that
ICF-folded thousands of identical COMDATs, so it can express one name per
address; "both mapped at different VAs" is satisfied whenever our callee's
spelling happens to sit on some unrelated address.  Its own
`_bijection_arbitrary` list names 1,109 VAs with the comment "WHICH name belongs
on WHICH VA is NOT established."  So every verdict here is read off
`orig/45410914/band.exe` and the dtk split objects, and map residency is used
only to LOCATE a body, never to decide one.

BUCKETS, strongest evidence first
---------------------------------
icf_fold                  T1: retail's body at the survivor address is identical
                          to what our compiler emits for the folded spelling --
                          masked bytes, size, and the full (offset, reloc_type)
                          sequence -- with every relocation resolving to the same
                          address (literally, by the proven alias equivalence, or
                          by CONTENT for a placeholder).  That is the /OPT:ICF
                          condition itself.  An alias states a fact.
fold_thunk_naming         retail's callee is a <=4-byte tail jump with a large
                          .text fan-in: one ICF survivor wearing one of many
                          names.  No source spelling reproduces that name.
                          Body evidence is vacuous by construction (4 bytes
                          compare equal to everything), so this is a SEPARATE
                          bucket, not an `icf_fold`.
different_callee          masked bytes and the (offset, reloc_type) sequence
                          agree, but a relocation resolves to two demonstrably
                          different addresses.  NOT a fold.  Note this is not the
                          same as "wrong callee at the call site": the
                          discriminator may itself be an unproven fold one level
                          down.
wrong_callee              both spellings are map-resident and the two retail
                          bodies at those VAs genuinely differ once
                          relocation-carrying fields are masked, and neither VA
                          is flagged arbitrary.  This is the candidate source
                          defect.
map_assignment_unresolved both map-resident, but the two retail bodies are
                          masked-identical, or the map flags one of the VAs in
                          `_bijection_arbitrary` / `_icf_arbitrary`.  Which name
                          belongs on which VA is not established, so the charge
                          says nothing about the call site.
transposition             a 2-cycle: (a,b) and (b,a) are both charged.  Swapping
                          clears both halves, so the metric cannot adjudicate it
                          in either direction; semantic evidence is required.
cannot_adjudicate         no readable retail body, or the body is vacuous, or
                          our callee is unmapped and the bodies differ -- in
                          which case a body difference is equally well explained
                          by our callee not matching yet, and says nothing about
                          WHICH function is called.

Usage
-----
    python3 scripts/namecheck_df_census.py --charges <d>/sites.jsonl \\
        --delta scripts/icf_alias_delta_fixpoint_grounded.json -o <d>/df_census.json
"""

import argparse
import collections
import json
import pickle
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "scripts"))
from icf_alias_build import vacuous                                    # noqa: E402
from icf_alias_fixpoint import UF, relocs_agree_eq, \
    ContentResolver, data_comdats, load_bodies                         # noqa: E402
# lane E's PE reader: same off()/word() surface plus the .text-wide fan-in count
from wrong_callee_triage import Image, masked_body, thunk_target, load_sizes  # noqa: E402

BUILD_ID = "45410914"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--charges", required=True)
    ap.add_argument("--delta", default=str(ROOT / "scripts" /
                                           "icf_alias_delta_fixpoint_grounded.json"))
    ap.add_argument("--installed", default=str(ROOT / "scripts" / "symbol_aliases.json"))
    ap.add_argument("--lane", default="different_function")
    ap.add_argument("--fold-fanin", type=int, default=50)
    ap.add_argument("--cache", default=str(ROOT / "work" / "laneK" / "bodies.pkl"))
    ap.add_argument("-o", "--out", default="")
    args = ap.parse_args()

    tm = json.loads((ROOT / "scripts" / "target_symbol_map.json").read_text())
    addr_of = {}
    for k, v in tm.items():
        if isinstance(k, str) and k.lower().startswith("0x") and isinstance(v, str):
            addr_of.setdefault(v, int(k, 16))
    mapped = set(addr_of)
    arb = {int(x, 16) for x in tm.get("_bijection_arbitrary", [])} | \
          {int(x, 16) for x in tm.get("_icf_arbitrary", [])}

    ours, retail, _ref, _tn = load_bodies(Path(args.cache))
    img = Image(ROOT / "orig" / BUILD_ID / "band.exe")
    size = load_sizes()
    fanin = img.fanin()
    resolver = ContentResolver(img, data_comdats())

    uf = UF()
    for src in (args.installed, args.delta):
        for g in json.loads(Path(src).read_text()).get("groups", []):
            for f in g.get("folded", []):
                uf.union(g["survivor"], f)
    accepted = {(g["survivor"], f)
                for g in json.loads(Path(args.delta).read_text()).get("groups", [])
                for f in g.get("folded", [])}

    sites = collections.Counter()
    fns = collections.defaultdict(set)
    for line in open(args.charges):
        r = json.loads(line)
        if r["lane"] != args.lane:
            continue
        t, b = r["target"], r["base"]
        if not isinstance(t, str) or not isinstance(b, str):
            continue
        sites[(t, b)] += 1
        fns[(t, b)].add((r["unit"], r["func"]))

    rows = []
    for (t, b), n in sites.most_common():
        ta, ba = addr_of.get(t), addr_of.get(b)
        rt, ob = retail.get(t), ours.get(b)
        why = ""
        if (t, b) in accepted or uf.eq(t, b):
            cls = "icf_fold"
            why = ("retail's body at the survivor address is identical to what our "
                   "compiler emits for this spelling, and every relocation resolves "
                   "to the same address")
        elif ta is not None and (size.get(ta) or 99) <= 4 and fanin[ta] >= args.fold_fanin:
            cls = "fold_thunk_naming"
            why = ("retail's callee is a %d-byte tail jump with %d call sites across "
                   ".text -- one ICF survivor wearing one of many names"
                   % (size.get(ta) or 0, fanin[ta]))
        elif rt is not None and ob is not None and not vacuous(rt) \
                and rt[0] == ob[0] and rt[2] == ob[2]:
            ok, _r, _tol = relocs_agree_eq(rt, ob, mapped, uf, resolver=resolver)
            if ok:
                cls, why = "icf_fold", "T1 identical (re-derived here)"
            else:
                cls = "different_callee"
                why = ("same masked bytes and the same (offset, reloc_type) sequence, "
                       "but a relocation resolves to two different addresses")
        elif ta is not None and ba is not None:
            mt, mb = masked_body(img, ta, size), masked_body(img, ba, size)
            if ta in arb or ba in arb:
                cls = "map_assignment_unresolved"
                why = "the map flags one of these VAs as an arbitrary assignment"
            elif mt is not None and mt == mb:
                cls = "map_assignment_unresolved"
                why = ("the two retail bodies are byte-identical once "
                       "relocation-carrying fields are masked -- an unflagged "
                       "bijection class")
            elif mt is None or mb is None:
                cls = "cannot_adjudicate"
                why = "no readable retail body at one of the two VAs"
            elif (b, t) in sites:
                cls = "transposition"
                why = ("2-cycle: (a,b) and (b,a) are both charged, so swapping "
                       "clears both halves and the metric cannot adjudicate -- "
                       "semantic evidence required")
            elif thunk_target(img, ba, size) == ta or thunk_target(img, ta, size) == ba:
                cls = "map_assignment_unresolved"
                why = "one side's callee is a one-instruction thunk to the other's"
            else:
                cls = "wrong_callee"
                why = ("both spellings are map-resident and the two retail bodies "
                       "genuinely differ once relocation-carrying fields are masked")
        elif rt is None:
            cls, why = "cannot_adjudicate", "the survivor is in no live pinned target obj"
        elif vacuous(rt):
            cls, why = "cannot_adjudicate", "retail's body is too small or too masked"
        else:
            cls = "cannot_adjudicate"
            why = ("our callee is not map-resident and the bodies differ -- equally "
                   "well explained by our callee not matching yet")
        rows.append({"target": t, "base": b, "sites": n, "fns": len(fns[(t, b)]),
                     "target_addr": None if ta is None else "0x%08x" % ta,
                     "base_addr": None if ba is None else "0x%08x" % ba,
                     "target_size": None if ta is None else size.get(ta),
                     "target_fanin": None if ta is None else fanin[ta],
                     "bucket": cls, "why": why})

    tp, ts, tf = collections.Counter(), collections.Counter(), collections.defaultdict(set)
    for r in rows:
        tp[r["bucket"]] += 1
        ts[r["bucket"]] += r["sites"]
        tf[r["bucket"]] |= fns[(r["target"], r["base"])]
    allf = {x for s in fns.values() for x in s}
    print("lane %s: %d pairs, %d sites, %d exposed functions"
          % (args.lane, len(rows), sum(ts.values()), len(allf)))
    print("%-27s %6s %7s %7s %7s" % ("bucket", "pairs", "sites", "fns", "%sites"))
    for k, v in tp.most_common():
        print("%-27s %6d %7d %7d %6.1f%%"
              % (k, v, ts[k], len(tf[k]), 100.0 * ts[k] / sum(ts.values())))
    if args.out:
        Path(args.out).write_text(json.dumps(
            {"lane": args.lane, "pairs": len(rows), "sites": sum(ts.values()),
             "exposed_functions": len(allf),
             "by_bucket": {k: {"pairs": tp[k], "sites": ts[k], "fns": len(tf[k])}
                           for k in tp},
             "rows": rows}, indent=1) + "\n")
        print("-> %s" % args.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
