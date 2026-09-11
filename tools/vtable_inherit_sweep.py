#!/usr/bin/env python3
"""vtable_inherit_sweep.py -- the retail-vs-ours vtable sweep, with INHERITANCE
no longer mistaken for ICF.

WHY THIS EXISTS (lane W3-E, 2026-09-11)
---------------------------------------
Lane L5 (`e1ec0647`) found `TrackWatcherImpl` slots 6 and 18 transposed in
BOTH our header AND the map, cancelling to a clean 100% while our
`TrackWatcher::SetAutoplayError` dispatched to `Restart`.  The brief for this
lane said the existing tools "compare our header against the map, which agree
by construction".  Tested literally, that is NOT why they missed it:

  `tools/vtable_order_sweep.py` compares RETAIL `.rdata` slot addresses (via
  the map name of each slot BODY) against OUR compiled `??_7X@@6B@`.  L5's map
  fix touched only four forwarding-THUNK names; the slot bodies were named
  correctly all along.  So the sweep held the evidence -- retail slot 18 =
  `?SetAutoplayError@TrackWatcherImpl@@`, ours = `?Restart@...` -- and never
  charged it, because `icf_fold_safe.fold_counts` declares any address that
  appears in MORE THAN ONE retail vtable `folded_across` (ICF) and therefore
  incomparable.  A base-class method address appears in EVERY derived class's
  vtable by construction.  TrackWatcherImpl has eight derived tables, so 42 of
  its 43 slots read `x8` and were withheld as "the ICF-fold wall" in waves
  6-9 (`docs/decomp/VTABLE_SLOT_COUNT_FIXES_2026-08-20.md` §12c, §13, §14).
  Baseline on this tree: 5,144 slots charged, **21,132 excluded as
  folded_across** -- four times the compared population.

  ⇒ The blind spot is INHERITANCE CONFLATED WITH ICF, not "header vs map".

THE DISCRIMINATOR
-----------------
An address `w` seen in N retail vtables is an ICF fold only if some table that
holds it belongs to a class OUTSIDE the hierarchy of the class that OWNS the
map name for `w`.  If every table holding `w` descends (per retail RTTI --
`bases_of_col`, the same decoder the sweep uses) from the name's owner, then
`w` is one method inherited N times, and its identity is intact.

  hub 0x826c3888 (x1433, the empty-body survivor): tables from hundreds of
      unrelated hierarchies  -> still `folded_across`, still incomparable.
  0x827947b0 (`SetAutoplayError@TrackWatcherImpl`, x8): all 8 tables are
      TrackWatcherImpl descendants -> INHERITED, comparable.

Everything else is inherited from the sweep unchanged: RTTI-identified retail
tables, `.pdata`-bounded slot reads, `folded_within`, `unnamed`,
`nonvirtual_name`, `unrelated_owner`, thunk-twin soft marks, the
`AMBIGUOUS_MULTI_VTABLE` refusal and `our_vtable_by_offset` join.  The
`Slot` type's raise-on-poisoned-compare guard is kept, so this file cannot
silently score a folded slot either way.

ONE EXTRA CONSERVATISM.  A derived override whose body ICF-folded onto its
base's body makes retail spell `?A@Base@@` where ours spells `?A@Derived@@` at
the same slot.  That is not a reordering.  A pair whose full names differ but
whose METHOD KEY (name + signature, owner stripped) agrees is WITHHELD as
`same_method_other_owner`, never charged.

CONTROL (must be re-run whenever this file changes; see the lane doc)
  * current tree: TrackWatcherImpl -> SAME, slot 18 covered as `inherited`.
  * `e1ec0647`'s header+map reverted, rebuilt: TrackWatcherImpl -> charged at
    slot 18 (`SetAutoplayError` retail vs `Restart` ours).  Slot 6 stays
    incomparable (it is the hub) -- a 2-slot transposition leaves one half
    visible, which is enough.
  * a class known correct stays SAME.

⛔ A charged row here has the SAME caveat as the sweep's: it does not by itself
separate "our declaration order is wrong" from "the map name of the slot BODY
is wrong".  Adjudicate on retail bytes (the slot body's own instructions, the
call sites' `lwz r11, N(r11)` immediates) before editing either side.

Usage:
  python3 tools/vtable_inherit_sweep.py --project-dir <wt> --sweep --json out.json
  python3 tools/vtable_inherit_sweep.py --project-dir <wt> --class TrackWatcherImpl -v
  python3 tools/vtable_inherit_sweep.py --selftest
"""
import argparse
import collections
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, 'scripts'))

import vtable_order_sweep as V          # noqa: E402
import icf_fold_safe as ifs             # noqa: E402


def method_key(sym):
    """Owner-stripped identity of a member: 'Name|<sig minus access letter>'.

    `?SetAutoplayError@TrackWatcherImpl@@UAAXH@Z` -> 'SetAutoplayError|AAXH@Z'
    `??_GTrackWatcherImpl@@UAAPAXI@Z`             -> '??_G|AAPAXI@Z'
    Adjustor thunks (`$4...`) and undecodable names return the name itself.
    """
    if not sym or not sym.startswith('?') or '@@$' in sym:
        return sym
    s = sym[1:]
    if s.startswith('?'):                       # ??<op>Class@@...
        s = s[1:]
        m = re.match(r'(_[A-Z0-9]|[0-9A-Z])', s)
        if not m:
            return sym
        op = '??' + m.group(1)
        rest = s[m.end():]
        i = rest.find('@@')
        return sym if i < 0 else op + '|' + rest[i + 3:]
    i = s.find('@')
    j = s.find('@@')
    if i < 0 or j < 0:
        return sym
    return s[:i] + '|' + s[j + 3:]


def classify_slots(slots, occ, addr2name, hier_this, addr_tables, hier_by_vt):
    """Fold-safe Slots for one retail table, inheritance-aware.

    Returns (slots, provenance) where provenance[i] is 'inherited' for a slot
    the plain sweep would have excluded as folded_across and this tool admits.
    """
    within = collections.Counter(w for (_va, w, _p) in slots)
    out, prov = [], {}
    for idx, (_va, w, _p) in enumerate(slots):
        if occ.get(w, 0) != 1:
            nm = addr2name.get("0x%08x" % w)
            holders = addr_tables.get(w, ())
            inherited = bool(nm) and bool(holders) and all(
                hier_by_vt.get(t) and ifs.name_owned_by(nm, hier_by_vt[t])
                for t in holders)
            if not inherited:
                out.append(ifs.Slot(addr=w, reason='folded_across'))
                continue
            prov[idx] = 'inherited'
        if within[w] != 1:
            out.append(ifs.Slot(addr=w, reason='folded_within'))
            continue
        nm = addr2name.get("0x%08x" % w)
        if not nm:
            out.append(ifs.Slot(addr=w, reason='unnamed'))
        elif ifs.name_is_nonvirtual(nm):
            out.append(ifs.Slot(name=ifs.normalize_dtor(nm), addr=w,
                                reason='nonvirtual_name'))
        elif not ifs.name_owned_by(nm, hier_this):
            out.append(ifs.Slot(name=ifs.normalize_dtor(nm), addr=w,
                                reason='unrelated_owner'))
        else:
            out.append(ifs.Slot(name=ifs.normalize_dtor(nm), addr=w))
    return out, prov


def compare(retail_sl, our_sl):
    pairs = ifs.comparable_pairs(retail_sl, our_sl)
    agree, mism, withheld = ifs.charge(pairs)
    # override-fold conservatism: same method, different owner -> withhold
    real, same_method = [], []
    for (i, r, o) in mism:
        if method_key(r.name) == method_key(o.name):
            same_method.append((i, r, o))
        else:
            real.append((i, r, o))
    covered = len(agree) + len(real)
    if covered < 2:
        verdict = 'UNRESOLVED'
    elif not real:
        verdict = 'SAME'
    else:
        rs = collections.Counter(r.name for (_i, r, _o) in agree + real)
        os_ = collections.Counter(o.name for (_i, _r, o) in agree + real)
        verdict = 'PERMUTED' if rs == os_ else 'SET_DIFFER'
    return verdict, covered, agree, real, withheld, same_method


def sweep_class(R, cls_rtti, vt_va, slots, occ, addr2name, project_dir,
                addr_tables, hier_by_vt, n_vtables=1):
    bare = V.bare_class(cls_rtti)
    sub_off, _sub_base = V.retail_subobject_base(R, vt_va)
    base = dict(cls=bare, rtti=cls_rtti, vt_va=vt_va, retail_slots=len(slots),
                our_slots=0, verdict='AMBIGUOUS_MULTI_VTABLE', covered=0,
                covered_inherited=0, mismatches=[], withheld=[], excluded={})
    if n_vtables > 1 and sub_off is None:
        return base
    hier = hier_by_vt[vt_va]
    retail_sl, prov = classify_slots(slots, occ, addr2name, hier,
                                     addr_tables, hier_by_vt)
    txt = [(s.va, s.va + s.rawsize) for s in R.sections if s.name == '.text'][0]
    retail_sl = ifs.mark_thunk_twins(
        retail_sl, lambda va: R.u32(va) if txt[0] <= va < txt[1] else None)
    ours, how = V.our_vtable_by_offset(bare, project_dir, sub_off)
    if how == 'no_vtable':
        ours = []
    elif ours is None:
        base['join'] = how
        return base
    our_sl = ifs.our_slot_names(ours)
    verdict, covered, agree, real, withheld, same_m = compare(retail_sl, our_sl)
    excl = ifs.exclusion_counts(retail_sl)
    inh = lambda i: prov.get(i) == 'inherited'   # noqa: E731
    return dict(
        cls=bare, rtti=cls_rtti, vt_va=vt_va,
        retail_slots=len(slots), our_slots=len(ours),
        verdict=verdict, covered=covered,
        covered_inherited=sum(1 for (i, _r, _o) in agree + real if inh(i)),
        excluded=dict(excl),
        n_descendant_tables=None,   # filled by main
        mismatches=[dict(slot=i, retail=r.name, ours=o.name,
                         retail_addr="0x%08x" % r.addr, occ=occ.get(r.addr, 0),
                         provenance=prov.get(i, 'plain'))
                    for (i, r, o) in real],
        same_method_other_owner=[dict(slot=i, retail=r.name, ours=o.name)
                                 for (i, r, o) in same_m],
        withheld=[dict(slot=i, retail=r.name, ours=o.name,
                       reason=(r.reason or o.reason), provenance=prov.get(i, 'plain'))
                  for (i, r, o) in withheld])


def load_all(project_dir):
    R = V.retail()
    exts = R.extents
    with open(os.path.join(project_dir, 'scripts', 'target_symbol_map.json')) as fh:
        raw = json.load(fh)
    addr2name = {}
    for k, v in raw.items():
        if isinstance(v, str):
            addr2name[k.lower()] = v
        elif isinstance(v, list) and v and isinstance(v[0], str):
            addr2name[k.lower()] = v[0]
    vts = sorted(V.enumerate_retail_vtables(R))
    starts = [va for va, _n in vts]
    text = [(s.va, s.va + s.rawsize) for s in R.sections if s.name == '.text'][0]
    tables = {va: V.read_retail_slots(R, va, exts, starts, text) for va, _n in vts}
    occ = ifs.fold_counts(tables)
    hier_by_vt = {va: (V.hierarchy_names(R, va) | {V.bare_class(n)}) for va, n in vts}
    addr_tables = collections.defaultdict(set)
    for va, slots in tables.items():
        for (_sva, w, _p) in slots:
            addr_tables[w].add(va)
    return R, addr2name, vts, tables, occ, hier_by_vt, addr_tables


def selftest():
    """The discriminator on a synthetic 3-table world; must be able to FAIL."""
    fails = []

    def chk(label, got, want):
        if got != want:
            fails.append(f"{label}: got {got!r} want {want!r}")

    # tables: Base (vt 100), Derived (vt 200, RTTI bases {Derived, Base}),
    # Unrelated (vt 300).  Address 0x10 = Base::A, inherited by Derived.
    # Address 0x20 = empty-body hub shared with Unrelated.
    tables = {100: [(0, 0x10, True), (4, 0x20, True)],
              200: [(0, 0x10, True), (4, 0x30, True)],
              300: [(0, 0x40, True), (4, 0x20, True)]}
    occ = ifs.fold_counts(tables)
    hier = {100: {'Base'}, 200: {'Derived', 'Base'}, 300: {'Unrelated'}}
    at = collections.defaultdict(set)
    for va, sl in tables.items():
        for (_s, w, _p) in sl:
            at[w].add(va)
    names = {"0x00000010": "?A@Base@@UAAXXZ", "0x00000020": "?B@Base@@UAAXXZ",
             "0x00000030": "?B@Derived@@UAAXXZ", "0x00000040": "?Z@Unrelated@@UAAXXZ"}
    sl, prov = classify_slots(tables[200], occ, names, hier[200], at, hier)
    chk('inherited slot comparable', sl[0].comparable, True)
    chk('inherited provenance', prov.get(0), 'inherited')
    sl1, _ = classify_slots(tables[100], occ, names, hier[100], at, hier)
    chk('hub shared with unrelated stays folded', sl1[1].reason, 'folded_across')
    chk('hub not comparable', sl1[1].comparable, False)
    # mutation: if the discriminator were the old `occ != 1`, slot 0 would be
    # folded_across -- assert the old rule really would have excluded it.
    chk('old rule would exclude', occ[0x10] != 1, True)
    # method_key
    chk('method_key strips owner',
        method_key('?A@Base@@UAAXXZ') == method_key('?A@Derived@@UAAXXZ'), True)
    chk('method_key keeps signature',
        method_key('?A@Base@@UAAXXZ') == method_key('?A@Base@@UAAXH@Z'), False)
    chk('method_key dtor', method_key('??_GFoo@@UAAPAXI@Z'), '??_G|AAPAXI@Z')
    # compare: transposition charged; same-method-other-owner withheld
    r = [ifs.Slot(name='?A@B@@UAAXXZ'), ifs.Slot(name='?C@B@@UAAXH@Z'),
         ifs.Slot(name='?D@B@@UAAXXZ')]
    o = [ifs.Slot(name='?C@B@@UAAXH@Z'), ifs.Slot(name='?A@B@@UAAXXZ'),
         ifs.Slot(name='?D@D@@UAAXXZ')]
    v, cov, ag, real, wh, sm = compare(r, o)
    chk('permuted verdict', v, 'PERMUTED')
    chk('two charged', len(real), 2)
    chk('override-fold withheld', len(sm), 1)
    if fails:
        print("SELFTEST FAIL")
        for f in fails:
            print("  " + f)
        return 1
    print("SELFTEST OK (9 checks)")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--project-dir', default=ROOT)
    ap.add_argument('--class', dest='cls')
    ap.add_argument('--sweep', action='store_true')
    ap.add_argument('--json')
    ap.add_argument('-v', '--verbose', action='store_true')
    ap.add_argument('--selftest', action='store_true')
    args = ap.parse_args()
    if args.selftest:
        return selftest()

    R, addr2name, vts, tables, occ, hier_by_vt, addr_tables = load_all(args.project_dir)
    sel = vts
    if args.cls:
        sel = [(va, n) for va, n in vts if V.bare_class(n) == args.cls]
        if not sel:
            print(f"no retail vtable found for class {args.cls!r}")
            return 1
    nvt = collections.Counter(V.bare_class(n) for _va, n in vts)
    # descendants: how many retail tables carry this class in their hierarchy
    desc = collections.Counter()
    for va, h in hier_by_vt.items():
        for c in h:
            desc[c] += 1

    results = []
    for va, n in sel:
        r = sweep_class(R, n, va, tables[va], occ, addr2name, args.project_dir,
                        addr_tables, hier_by_vt, n_vtables=nvt[V.bare_class(n)])
        r['n_descendant_tables'] = desc[V.bare_class(n)]
        results.append(r)

    by = collections.Counter(r['verdict'] for r in results)
    print(f"retail vtables examined: {len(results)}")
    for k in ('PERMUTED', 'SET_DIFFER', 'SAME', 'UNRESOLVED', 'AMBIGUOUS_MULTI_VTABLE'):
        print(f"  {k:<24} {by.get(k, 0)}")
    cov = sum(r['covered'] for r in results)
    cov_inh = sum(r['covered_inherited'] for r in results)
    print(f"\ncomparable slots charged on : {cov}  (of which admitted as INHERITED: {cov_inh})")
    excl = collections.Counter()
    for r in results:
        excl.update(r.get('excluded') or {})
    for k, v in excl.most_common():
        print(f"  excluded {k:<18} {v}")
    wh = sum(len(r['withheld']) for r in results)
    sm = sum(len(r.get('same_method_other_owner', [])) for r in results)
    print(f"withheld (suspect name, disagreed): {wh}")
    print(f"withheld (same method, other owner -- override fold): {sm}")
    if not args.cls:
        ifs.assert_can_agree(by['SAME'], by['SAME'] + by['SET_DIFFER'] + by['PERMUTED'],
                             label='inherit sweep')

    charged = [r for r in results if r['mismatches']]
    new = [r for r in charged if any(m['provenance'] == 'inherited' for m in r['mismatches'])]
    print(f"\nclasses with charged mismatches: {len(charged)}; "
          f"with at least one NEWLY-VISIBLE (inherited) charge: {len(new)}")
    new.sort(key=lambda r: (-r['n_descendant_tables'], r['cls']))
    for r in new:
        print(f"\n=== {r['cls']}  vt=0x{r['vt_va']:08x}  {r['verdict']}  "
              f"covered={r['covered']} (inherited {r['covered_inherited']})  "
              f"descendant_tables={r['n_descendant_tables']}")
        for m in r['mismatches']:
            print(f"  [{m['slot']:>3}] {m['provenance']:<9} occ={m['occ']:<4} "
                  f"retail {m['retail_addr']} {m['retail']}")
            print(f"        ours   {m['ours']}")
    if args.verbose or args.cls:
        for r in results if args.cls else charged:
            if r in new and not args.cls:
                continue
            print(f"\n--- {r['cls']} vt=0x{r['vt_va']:08x} {r['verdict']} covered={r['covered']} "
                  f"(inherited {r['covered_inherited']}) excluded={r['excluded']}")
            for m in r['mismatches']:
                print(f"  [{m['slot']:>3}] {m['provenance']:<9} occ={m['occ']:<4} "
                      f"retail {m['retail_addr']} {m['retail']}\n        ours   {m['ours']}")
            for m in r['withheld']:
                print(f"  [{m['slot']:>3}] WITHHELD({m['reason']}) retail {m['retail']}\n"
                      f"        ours   {m['ours']}")
            for m in r.get('same_method_other_owner', []):
                print(f"  [{m['slot']:>3}] WITHHELD(same_method_other_owner) retail {m['retail']}\n"
                      f"        ours   {m['ours']}")
    if args.json:
        with open(args.json, 'w') as fh:
            json.dump(results, fh, indent=1)
        print(f"\nwrote {args.json}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
