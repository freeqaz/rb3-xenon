#!/usr/bin/env python3
"""vtable_override_pattern -- NAME-INDEPENDENT override audit of primary vtables.

Lane W16-OP (2026-10-03).  tools/vtable_order_sweep.py judges slot ORDER by
map name, so ~half the tables are UNRESOLVED for lack of names.  This asks a
question that needs no retail name at all:

    for class C with first base P (retail RTTI Base Class Array [1]),
    slot i of C's primary vtable is INHERITED in retail iff
        retail_vt(C)[i] == retail_vt(P)[i]          (same address)
    and INHERITED in ours iff
        our_vt(C)[i] == our_vt(P)[i]                (same symbol)

A disagreement is a CANDIDATE:
  RETAIL_OVERRIDES  retail has a distinct body, we inherit P's
                    -> we lack a virtual override (or declare it on the wrong
                       class).
  OURS_OVERRIDES    retail reuses P's address, we have our own body
                    -> an extra override in our header, OR an ICF fold: our
                       override's body is byte-identical to P's (the classic
                       `return 0` / empty body).  Fold occupancy of P's
                       address is printed so a hub is visible as such.
Both are adjudicated on retail bytes, never by this tool.

Slot 0 (the deleting destructor) is skipped: every class overrides it.

Read-only.  Usage:
    python3 tools/vtable_override_pattern.py [--project-dir .] [--json OUT]
        [--scope src/band3 src/system/bandobj]
"""
import argparse
import collections
import glob
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import vtable_order_sweep as vos  # noqa: E402


def scoped_classes(project_dir, roots):
    pat = re.compile(r'^\s*(?:class|struct)\s+(\w+)\s*(?:final\s*)?(?::[^:]|\{)', re.M)
    out = set()
    for root in roots:
        for f in glob.glob(os.path.join(project_dir, root, '**', '*.[hc]*'), recursive=True):
            try:
                txt = open(f, errors='ignore').read()
            except OSError:
                continue
            out.update(m.group(1) for m in pat.finditer(txt))
    return out


def bare(n):
    if n and n.startswith('.?A') and len(n) > 4:
        n = n[4:]
    if n and n.endswith('@@'):
        n = n[:-2]
    return n


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--project-dir', default=ROOT)
    ap.add_argument('--json')
    ap.add_argument('--scope', nargs='*', default=['src/band3', 'src/system/bandobj'])
    a = ap.parse_args()

    R = vos.retail()
    tables = vos.enumerate_retail_vtables(R)
    starts = sorted(va for va, _ in tables)
    exts = R.extents
    # same .text bound the sweep uses (rawsize, not vsize)
    text = [(s.va, s.va + s.rawsize) for s in R.sections if s.name == '.text'][0]
    slots_of = {}
    for va, _n in tables:
        slots_of[va] = [w for _s, w, _p in vos.read_retail_slots(R, va, exts, starts, text)]
    occ = collections.Counter(w for v in slots_of.values() for w in v)

    primary = {}            # bare class -> retail primary vtable va
    for va, n in tables:
        off, _b = vos.retail_subobject_base(R, va)
        if off == 0:
            primary.setdefault(bare(n), va)

    scope = scoped_classes(a.project_dir, a.scope) if a.scope else None
    out, stats = [], collections.Counter()
    for cls, vc in sorted(primary.items()):
        if scope is not None and cls not in scope:
            continue
        col = R.u32(vc - 4)
        got = R.bases_of_col(col)
        if not got:
            stats['no_hierarchy'] += 1
            continue
        _c, _chd, bases = got
        if len(bases) < 2:
            stats['no_base'] += 1
            continue
        par = bare(bases[1].name)
        vp = primary.get(par)
        if vp is None:
            stats['parent_no_primary'] += 1
            continue
        rc, rp = slots_of[vc], slots_of[vp]
        oc, why_c = vos.our_vtable_by_offset(cls, a.project_dir, 0)
        op, why_p = vos.our_vtable_by_offset(par, a.project_dir, 0)
        if not oc or not op:
            stats['ours_unreadable'] += 1
            continue
        oc = [d['symbol'] for d in oc]
        op = [d['symbol'] for d in op]
        n = min(len(rp), len(op), len(rc), len(oc))
        if len(rp) != len(op):
            stats['parent_count_differs'] += 1
        stats['compared'] += 1
        for i in range(1, n):
            r_inh = rc[i] == rp[i]
            o_inh = oc[i] == op[i]
            if r_inh == o_inh:
                continue
            rec = dict(cls=cls, parent=par, slot=i,
                       kind='RETAIL_OVERRIDES' if o_inh else 'OURS_OVERRIDES',
                       retail_c=hex(rc[i]), retail_p=hex(rp[i]),
                       occ_c=occ[rc[i]], occ_p=occ[rp[i]],
                       ours_c=oc[i], ours_p=op[i])
            out.append(rec)
            stats[rec['kind']] += 1
    for r in out:
        print(f"{r['kind']:16s} {r['cls']}:{r['slot']} (parent {r['parent']})  "
              f"retail C={r['retail_c']} x{r['occ_c']} P={r['retail_p']} x{r['occ_p']}  "
              f"ours C={r['ours_c']}  P={r['ours_p']}")
    print(dict(stats))
    if a.json:
        json.dump(out, open(a.json, 'w'), indent=1)
    return 0


if __name__ == '__main__':
    sys.exit(main())
