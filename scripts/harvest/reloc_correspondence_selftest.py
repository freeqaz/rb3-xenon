#!/usr/bin/env python3
"""Falsification control for reloc_correspondence.py.

The classifier is only worth anything if it CAN say DIVERGENT about work it
otherwise calls CORRESPONDING.  This injects a known-wrong answer and checks
that the verdict flips.

Perturbation: take functions the classifier called CORRESPONDING and re-run them
with every target relocation VA displaced by +delta.  Displacing a pointer makes
it point at the wrong object BY CONSTRUCTION, so a working classifier must
return DIVERGENT (or at minimum stop returning CORRESPONDING).  A classifier
that still says CORRESPONDING is measuring nothing.
"""
import importlib.util
import json
import os
import random
import argparse
import sys

_ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
_ap.add_argument("--worktree", default=os.getcwd())
_ap.add_argument("--census", required=True,
                 help="census JSON produced by reloc_correspondence.py --out")
_ap.add_argument("--delta", default="0x40")
_ap.add_argument("-n", type=int, default=400)
_args = _ap.parse_args()

root = os.path.abspath(_args.worktree)
sys.path.insert(0, os.path.join(root, "scripts", "unicorn_runner"))
spec = importlib.util.spec_from_file_location(
    "rc", os.path.join(root, "scripts", "harvest", "reloc_correspondence.py"))
rc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rc)

os.chdir(root)
DELTA = int(_args.delta, 0)
N = _args.n

oracles = rc.Oracles(root)
units = rc.load_units(root)
image = rc.RetailImage(os.path.join(root, "orig/45410914/band.exe"))
icf = rc.build_base_index(units, verbose=True)
base_index = icf[1]
full, _ = rc.load_matched(root, None, oracles)
full = {k: v for k, v in full.items() if k in units}
matched_names = {n for v in full.values() for n, _ in v}
consistency = rc.build_consistency(root, units, full, oracles, verbose=True)

rows = json.load(open(_args.census))
corr = [r for r in rows if r["verdict"] == "CORRESPONDING" and r.get("n_correspond", 0) > 0]
random.seed(1234)
sample = random.sample(corr, min(N, len(corr)))

orig_relocs = rc.obj_relocs_for_symbol
orig_va_of_label = oracles.va_of_label


def shifted_relocs(delta):
    """Rewrite every TARGET relocation to a DIFFERENT symbol: the anonymous
    label `delta` bytes further on. This breaks all four oracles at once --
    name identity, content, consistency and map -- which is what "the pointer
    is aimed at the wrong object" actually means."""
    def f(parser, sym_name, size_hint=None):
        out = orig_relocs(parser, sym_name, size_hint)
        if out is None or parser is not TARGET[0]:
            return out
        res = []
        for off, ty, nm in out:
            v = orig_va_of_label(nm)
            res.append((off, ty, f"fn_{v + delta:08X}" if v is not None else nm))
        return res
    return f


TARGET = [None]


from collections import Counter

base_res = Counter()
pert_res = Counter()
for r in sample:
    up = units[r["unit"]]
    rc.obj_relocs_for_symbol = orig_relocs
    a = rc.classify_function(up, r["name"], r["size"], oracles, consistency,
                             True, icf, image, base_index, matched_names)
    TARGET[0] = up.target
    rc.obj_relocs_for_symbol = shifted_relocs(DELTA)
    b = rc.classify_function(up, r["name"], r["size"], oracles, consistency,
                             True, icf, image, base_index, matched_names)
    rc.obj_relocs_for_symbol = orig_relocs
    base_res[a["verdict"]] += 1
    pert_res[b["verdict"]] += 1
rc.obj_relocs_for_symbol = orig_relocs

print(f"\n=== FALSIFICATION CONTROL (n={len(sample)}, VA shift +0x{DELTA:X}) ===")
print("unperturbed:", dict(base_res))
print("perturbed  :", dict(pert_res))
still = pert_res.get("CORRESPONDING", 0)
print(f"\nstill CORRESPONDING after injecting wrong pointers: {still} "
      f"({100.0*still/max(len(sample),1):.1f}%)")
print("=> the classifier CAN produce the falsifying verdict"
      if still < len(sample) * 0.05 else
      "=> ★ CLASSIFIER IS NOT DISCRIMINATING -- do not trust its census")
