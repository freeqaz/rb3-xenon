#!/usr/bin/env python3
"""fp2_price.py -- price a proposed alias install set JOINTLY, from report.json.

matched_code is ALL-OR-NOTHING PER ROW: a row pays only when EVERY surviving charge on
it closes.  The queue's solo_closable_B is "what closes if that pair ALONE is fixed", so
summing solo columns is neither an upper nor a lower bound on a multi-pair install --
rows carrying two of the installed pairs are missing from the sum, rows carrying an
uninstalled charge must not be counted at all.

Prices exactly the rule nogroup_census uses:
    fuzzy < 100  and  mpn >= 100  and  EVERY surviving charge on the row is in the set.
"""
import collections
import glob
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))

from icf_alias_build import collect, placeholder  # noqa: E402
from incomplete_group_census import charged_pairs, load_groups  # noqa: E402

INSTALL = json.load(open(sys.argv[1] if len(sys.argv) > 1 else "/home/free/tmp/fp2_install.json"))
WANT = set()
for g in INSTALL:
    for f in g["folded"]:
        WANT.add((g["survivor"], f))

tgt = collect(sorted(glob.glob(os.path.join(ROOT, "build/45410914/obj/**/*.obj"), recursive=True)), "t")
ours = collect(sorted(glob.glob(os.path.join(ROOT, "build/45410914/src/**/*.obj"), recursive=True)), "o")
owner, groups = load_groups(os.path.join(ROOT, "scripts/symbol_aliases.json"))
sites, victims, aligned = charged_pairs(tgt, ours)

for p in [p for p in sites if placeholder(p[0]) or placeholder(p[1])]:
    del sites[p]
for p in list(sites):
    gi, gj = owner.get(p[0]), owner.get(p[1])
    if gi is not None and gi == gj:
        del sites[p]

row_charges = collections.defaultdict(set)
for p in sites:
    for r in victims[p]:
        row_charges[r].add(p)

rep = json.load(open(os.path.join(ROOT, "build/45410914/report.json")))
mpn, fuzzy, size = {}, {}, {}
for u in rep["units"]:
    for f in u.get("functions", []):
        mpn[f["name"]] = float(f["match_percent_normalized"])
        fuzzy[f["name"]] = float(f.get("fuzzy_match_percent", 0.0))
        size[f["name"]] = int(f["size"])

missing = [p for p in WANT if p not in sites]
print("install pairs: %d ; charged in tree: %d ; NOT charged: %d" % (len(WANT), len(WANT) - len(missing), len(missing)))
for p in missing:
    print("   NOT CHARGED: %s <- %s" % (p[0][:60], p[1][:60]))

touched = set()
for p in WANT:
    touched |= set(victims.get(p, ()))

pay, blocked = [], collections.Counter()
for r in sorted(touched):
    nm = r.split("|")[-1] if "|" in r else r
    f, m, s = fuzzy.get(nm), mpn.get(nm), size.get(nm, 0)
    if f is None:
        blocked["no report row"] += 1
        continue
    if f >= 100.0:
        blocked["already paying (fuzzy==100)"] += 1
        continue
    if m < 100.0:
        blocked["mpn<100 (instruction-level mismatch)"] += 1
        continue
    rest = row_charges[r] - WANT
    if rest:
        blocked["carries %d charge(s) outside the set" % len(rest)] += 1
        continue
    pay.append((s, nm))

print("\nrows touched: %d" % len(touched))
for k, v in blocked.most_common():
    print("   blocked: %-44s %d" % (k, v))
print("\nJOINT CLOSABLE: %d rows / %d B" % (len(pay), sum(s for s, _ in pay)))
for s, nm in sorted(pay, reverse=True)[:20]:
    print("   %6d B  %s" % (s, nm[:88]))
