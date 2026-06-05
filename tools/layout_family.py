#!/usr/bin/env python3
"""Enumerate a base class's full layout family + each member's current match state.

A coupled-base edit shifts every class that INHERITS or EMBEDS the base. Before
touching the base you must know that whole family and which members are already
matching (those will REGRESS) vs near-miss (those will IMPROVE). This greps the
source tree for both relationships and joins to report.json match data.

Usage:
  python3 tools/layout_family.py UIPanel
  python3 tools/layout_family.py ObjPtrVec --report build/45410914/report.json
"""
import json, os, re, subprocess, sys, argparse
from collections import defaultdict

ROOT = "/home/free/code/milohax/rb3-xenon"

def grep(pat, path):
    p = subprocess.run(["grep", "-rEl", pat, path], capture_output=True, text=True)
    return [l for l in p.stdout.splitlines() if l.strip()]

def classes_in(header, base):
    """Return class names in `header` that derive from or embed `base`."""
    try:
        txt = open(header, errors="ignore").read()
    except Exception:
        return [], []
    derived, embedding = [], []
    # derivation:  class Foo : ... <base> ...
    for m in re.finditer(r'\bclass\s+(\w+)\s*:\s*([^{]+)\{', txt):
        name, bases = m.group(1), m.group(2)
        if re.search(r'\b' + re.escape(base) + r'\b', bases):
            derived.append(name)
    # embedding:   <base>(<...>)? m... ;   (member of type base/base<T>)
    for m in re.finditer(r'\b' + re.escape(base) + r'\b\s*(?:<[^;{}]*>)?\s+\*?\s*(m\w+)\s*;', txt):
        embedding.append(m.group(1))
    return derived, embedding

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("base")
    ap.add_argument("--report", default=os.path.join(ROOT, "build/45410914/report.json"))
    ap.add_argument("--src", default=os.path.join(ROOT, "src"))
    a = ap.parse_args()
    base = a.base

    hdrs = grep(r'\b' + re.escape(base) + r'\b', a.src)
    hdrs = [h for h in hdrs if h.endswith((".h", ".hpp"))]
    derived, embedding = set(), defaultdict(list)
    for h in hdrs:
        d, e = classes_in(h, base)
        for n in d: derived.add(n)
        for mem in e: embedding[os.path.relpath(h, ROOT)].append(mem)

    # match state per class: find the unit whose basename == class (heuristic) + its near-misses
    rep = json.load(open(a.report))
    unit_by_base = {}
    for u in rep["units"]:
        unit_by_base[os.path.basename(u["name"])] = u
    def state(cls):
        u = unit_by_base.get(cls)
        if not u: return "(no unit)"
        fns = u.get("functions", [])
        tot = len(fns)
        matched = sum(1 for f in fns if f.get("match_percent_normalized", 0) >= 100)
        near = sum(1 for f in fns if 80 <= f.get("match_percent_normalized", 0) < 100)
        return f"unit={u['name']} fns={tot} matched={matched} near80-100={near}"

    print(f"=== layout family of `{base}` ===\n")
    print(f"DERIVED classes ({len(derived)}) — a base-size change shifts each:")
    for c in sorted(derived):
        print(f"  {c:32s} {state(c)}")
    print(f"\nEMBEDDING (header -> members of type {base}):")
    for h, mems in sorted(embedding.items()):
        print(f"  {h}: {', '.join(mems)}")
    print(f"\nNOTE: classes with matched>0 may REGRESS on a base-size change (already")
    print(f"matching at a compensating offset); near80-100 classes should IMPROVE.")

if __name__ == "__main__":
    main()
