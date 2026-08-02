#!/usr/bin/env python3
"""Lane CY-4: final function-level accounting over CW-2's class (b) UNKNOWN block.

Applies, in order, and never letting an undecided row fall into a decided class:

  CW-2  : the two byte channels, unchanged                     (tools/at100_adjudicate.py)
  CY-4a : COFF WEAK-EXTERNAL alias -- B is an undefined weak external whose aux
          default IS retail's callee, so the two names LINK TO THE SAME CODE
  CY-4b : weak-external RESOLUTION, then CW-2's byte channels re-run on the
          resolved symbol (which, unlike B, we actually compile)
  CY-4c : PREFIX test at addr(T) for rows with no .pdata extent. Emits BENIGN or
          nothing -- structurally incapable of manufacturing a charge.

Roll-up is worst-verdict-dominates, identical to at100_adjudicate.py, so the
numbers compose with CW-2's rather than replacing them.

Read-only.
"""

import argparse
import collections
import json
import os
import re
import sys
from pathlib import Path

ROOT = os.environ.get("CY4_ROOT", str(Path(__file__).resolve().parent.parent))
os.environ.setdefault("CW2_ROOT", ROOT)
sys.path.insert(0, os.path.join(ROOT, "tools"))
from xbin_adjudicate import Xbin                                # noqa: E402
from cy4_weakext_adjudicate import build_index                  # noqa: E402
from cy4_prefix_noextent import prefix_test                     # noqa: E402

PLACEHOLDER = re.compile(r'^_?(fn|lbl|jumptable|code|data|bss|rdata)_[0-9a-fA-F_]+$')


def forgiven(n):
    return bool(PLACEHOLDER.match(n)) or n.startswith('$')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=ROOT)
    ap.add_argument("--adj", required=True)
    ap.add_argument("--sites", required=True)
    ap.add_argument("--report", required=True)
    ap.add_argument("--resolved", required=True)
    a = ap.parse_args()

    weak, defined, _p, _n, _c = build_index(a.root)
    x = Xbin(a.root)
    adjrows = json.load(open(a.adj))["pairs"]
    adj = {(t, b): (cls, v, reason) for t, b, cls, v, reason, _h, _ns in adjrows}
    resolved = {(r[0], r[1]): r[5] for r in json.load(open(a.resolved))["rows"]}

    prefix_cache = {}

    def prefix_benign(T, B):
        k = (T, B)
        if k in prefix_cache:
            return prefix_cache[k]
        ta = x.name2addr.get(T)
        r = False
        if ta and len(ta) == 1:
            r = (prefix_test(x, B, ta[0]) == "MATCH")
        prefix_cache[k] = r
        return r

    def verdict(T, B):
        cls, v, reason = adj.get((T, B), (None, "UNKNOWN", ""))
        if v != "UNKNOWN":
            return v, "cw2"
        if weak.get(B) == T and B not in defined:
            return "BENIGN", "cy4a_weakext_alias"
        rv = resolved.get((T, B))
        if rv and rv != "UNKNOWN":
            return ("BENIGN" if rv.startswith("BENIGN") else rv), "cy4b_resolved_byte"
        if "no_pdata_extent" in reason and prefix_benign(T, B):
            return "BENIGN", "cy4c_prefix"
        return "UNKNOWN", "still_unknown"

    rep = json.load(open(a.report))
    size, pct = {}, {}
    for u in rep["units"]:
        stem = u["name"].split("/")[-1]
        for f in (u.get("functions") or []):
            size[(stem, f["name"])] = int(f["size"])
            pct[(stem, f["name"])] = f["match_percent_normalized"]

    ORDER = ["a_wrong", "a_wrong_shapetwin", "b_backlog", "d_body_unreadable",
             "d_no_target_addr", "c_shapetwin_ours", "c_fold_ours"]
    sites = json.load(open(a.sites))["records"]

    before = collections.Counter(); beforeb = collections.Counter()
    after = collections.Counter(); afterb = collections.Counter()
    prov = collections.Counter()
    for unit, fn, rows in sites:
        rs = [r for r in rows if not forgiven(r[1])]
        if not rs or pct.get((unit, fn)) != 100.0:
            continue
        clss = {adj[(T, B)][0] for _k, T, B in rs if (T, B) in adj}
        if not clss:
            continue
        if next((c for c in ORDER if c in clss), None) != "b_backlog":
            continue
        vb, va_ = [], []
        for _k, T, B in rs:
            if adj.get((T, B), (None,))[0] != "b_backlog":
                continue
            vb.append(adj[(T, B)][1])
            v, p = verdict(T, B)
            va_.append(v); prov[p] += 1
        if not vb:
            continue
        def worst(vs):
            return next((w for w in ("WRONG_2ch", "WRONG_1ch", "UNKNOWN") if w in vs),
                        "BENIGN")
        wb, wa = worst(vb), worst(va_)
        before[wb] += 1; beforeb[wb] += size.get((unit, fn), 0)
        after[wa] += 1;  afterb[wa] += size.get((unit, fn), 0)

    tot = sum(before.values()); totb = sum(beforeb.values())
    print("=== CW-2 class (b), FUNCTION roll-up   denominator %d fns / %d B ===" % (tot, totb))
    print("%-11s  %18s   %18s" % ("verdict", "CW-2 (before)", "CY-4 (after)"))
    for k in ("BENIGN", "WRONG_2ch", "WRONG_1ch", "UNKNOWN"):
        print("  %-9s  %5d fns %6.1f%%  %5d fns %6.1f%%   |  %7d B -> %7d B"
              % (k, before[k], 100.0 * before[k] / tot,
                 after[k], 100.0 * after[k] / tot, beforeb[k], afterb[k]))
    print("\n  UNKNOWN  %d -> %d fns  (decided %d, %.1f%% of the block)"
          % (before["UNKNOWN"], after["UNKNOWN"],
             before["UNKNOWN"] - after["UNKNOWN"],
             100.0 * (before["UNKNOWN"] - after["UNKNOWN"]) / max(1, before["UNKNOWN"])))
    print("  bytes    %d -> %d B  (decided %d B, %.1f%% of the block's bytes)"
          % (beforeb["UNKNOWN"], afterb["UNKNOWN"],
             beforeb["UNKNOWN"] - afterb["UNKNOWN"],
             100.0 * (beforeb["UNKNOWN"] - afterb["UNKNOWN"]) / max(1, beforeb["UNKNOWN"])))
    print("\n  pair-verdict provenance: %s" % dict(prov))


if __name__ == "__main__":
    main()
