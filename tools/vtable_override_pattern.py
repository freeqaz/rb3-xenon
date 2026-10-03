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


# ---------------------------------------------------------------------------
# --all-tables  (lane W16-OT, 2026-10-03)
# ---------------------------------------------------------------------------
# The primary-only pass above leaves four populations unjudged:
#   * every SECONDARY (multiple-inheritance) table and every VIRTUAL-BASE
#     table -- it only reads COL.offset == 0;
#   * `parent_no_primary` -- the first base has no retail primary table (its
#     vtable was dropped by /OPT:REF because every ctor of it is inlined);
#   * `no_base` -- a root table has no comparator at all;
#   * `ours_unreadable` -- we emit no ??_7 for the class.
# This mode judges every retail table of a scoped class:
#   comparator  = the base whose subobject the table belongs to (non-virtual
#                 mdisp == COL.offset; else the virtual base -- one group, or
#                 the one our own COFF suffix names).  If the comparator has no
#                 table on EITHER side, walk down ITS primary-base chain inside
#                 the class's own Base Class Array (same subobject offset) to
#                 the first ancestor both sides have; compare the shared prefix.
#   OVERRIDE    as the primary pass, slot by slot (slot 0 = dtor skipped).
#   PURE        retail slot is _purecall (0x828299B8) xor ours is `_purecall`
#               -- judged on EVERY table, roots included.
#   COUNT       retail slot count != ours (bounded by the next table start).
# All rows are candidates, adjudicated on retail bytes.
RETAIL_PURECALL = 0x828299B8
BLR = 0x4E800020
#   BODY        retail's slot body is a TINY RELOCATION-FREE LEAF (<= 4 words
#               ending in blr, no branch, no lis) -- `blr`, `li r3,K; blr`,
#               `lwz r3,off(r3); blr` -- and our slot's compiled body is not
#               byte-identical.  Judged on EVERY slot of EVERY table, so it
#               reaches a base class's OWN new virtuals, which no comparator
#               covers (W16-OT found Synth slot 25: retail `blr` = rb3-Wii's
#               `void SetMono(bool) {}`, ours DC3's `bool IsUsingDolby()`).
#               A getter's load offset is compared too, so a wrong member
#               offset behind a one-instruction getter is a BODY row.


def retail_leaf(R, a):
    words = []
    for i in range(4):
        w = R.u32(a + 4 * i)
        if w is None:
            return None
        words.append(w)
        if w == BLR:
            return tuple(words)
        op = w >> 26
        if op in (15, 16, 17, 18, 19):      # lis/addis, bc, sc, b/bl, bclr/bcctr
            return None
    return None


_OUR_BODIES = None


def our_bodies(project_dir):
    """{symbol: body bytes} for every function slice we compiled (first wins)."""
    global _OUR_BODIES
    if _OUR_BODIES is None:
        from coff_bodies_ext import function_bodies_ext
        _OUR_BODIES = {}
        base = os.path.join(project_dir, 'build', '45410914', 'src')
        for dp, _d, fs in os.walk(base):
            for f in fs:
                if f.endswith('.obj'):
                    try:
                        for n, b, rl, _v in function_bodies_ext(os.path.join(dp, f)):
                            _OUR_BODIES.setdefault(n, (b, bool(rl)))
                    except Exception:
                        pass
    return _OUR_BODIES


def _subtree_chain(bases, j):
    """bases[j] and its primary-base chain at the same subobject (same
    mdisp/pdisp/vdisp), walking the preorder Base Class Array."""
    out = [j]
    while True:
        b = bases[out[-1]]
        k = out[-1] + 1
        if b.num_contained_bases == 0 or k >= len(bases):
            return out
        c = bases[k]
        if (c.mdisp, c.pdisp, c.vdisp) != (b.mdisp, b.pdisp, b.vdisp):
            return out
        out.append(k)


def all_tables(a):
    R = vos.retail()
    tables = vos.enumerate_retail_vtables(R)
    starts = sorted(va for va, _ in tables)
    exts = R.extents
    text = [(s.va, s.va + s.rawsize) for s in R.sections if s.name == '.text'][0]
    slots_of, by_cls = {}, collections.defaultdict(dict)
    for va, n in tables:
        slots_of[va] = [w for _s, w, _p in vos.read_retail_slots(R, va, exts, starts, text)]
        c = R.decode_col(R.u32(va - 4))
        by_cls[bare(n)].setdefault(c.offset, va)
    occ = collections.Counter(w for v in slots_of.values() for w in v)
    scope = scoped_classes(a.project_dir, a.scope) if a.scope else None
    excl = scoped_classes(a.project_dir, a.exclude) if a.exclude else set()
    out, stats = [], collections.Counter()

    def ours(cls, off):
        t, why = vos.our_vtable_by_offset(cls, a.project_dir, off)
        return ([d['symbol'] for d in t] if t else None), why

    for cls, offs in sorted(by_cls.items()):
        short = cls.split('@')[0]
        if scope is not None and (short not in scope or short in excl):
            continue
        for m, vc in sorted(offs.items()):
            stats['tables'] += 1
            rc = slots_of[vc]
            oc, why = ours(cls, m)
            tag = 'primary' if m == 0 else f'secondary@{m:#x}'
            if oc is None:
                stats['ours_unreadable'] += 1
                out.append(dict(cls=cls, off=m, kind='OURS_NOT_EMITTED', why=why,
                                retail_vt=hex(vc), retail_slots=len(rc)))
                continue
            if len(rc) != len(oc):
                stats['COUNT'] += 1
                out.append(dict(cls=cls, off=m, kind='COUNT', retail_vt=hex(vc),
                                retail_slots=len(rc), our_slots=len(oc)))
            bodies = our_bodies(a.project_dir)
            for i in range(1, min(len(rc), len(oc))):
                rp_, op_ = rc[i] == RETAIL_PURECALL, oc[i] == '_purecall'
                if rp_ != op_:
                    stats['PURE'] += 1
                    out.append(dict(cls=cls, off=m, slot=i, kind='PURE',
                                    retail_c=hex(rc[i]), ours_c=oc[i]))
                    continue
                if rp_:
                    continue
                leaf = retail_leaf(R, rc[i])
                if leaf is None:
                    continue
                stats['body_checked'] += 1
                ob = bodies.get(oc[i])
                if ob is None:
                    stats['body_ours_missing'] += 1
                    continue
                rb = b''.join(w.to_bytes(4, 'big') for w in leaf)
                if ob[0] != rb:
                    stats['BODY'] += 1
                    out.append(dict(cls=cls, off=m, slot=i, kind='BODY',
                                    retail_c=hex(rc[i]),
                                    retail_words=' '.join('%08x' % w for w in leaf),
                                    ours_c=oc[i], ours_len=len(ob[0]),
                                    ours_head=ob[0][:12].hex()))
            got = R.bases_of_col(R.u32(vc - 4))
            if not got:
                stats['no_hierarchy'] += 1
                continue
            _c, _chd, bases = got
            nv = [j for j in range(1, len(bases))
                  if bases[j].pdisp == -1 and bases[j].mdisp == m]
            j = None
            if nv:
                j = nv[0]
            elif m != 0:
                firsts, seen = [], set()
                for k in range(1, len(bases)):
                    b = bases[k]
                    if b.pdisp != -1 and (b.pdisp, b.vdisp) not in seen:
                        seen.add((b.pdisp, b.vdisp))
                        firsts.append(k)
                if len(firsts) == 1:
                    j = firsts[0]
                else:
                    sfx = why.split('@@6B', 1)[-1] if '@@6B' in why else ''
                    hit = [k for k in firsts if sfx and bare(bases[k].name) + '@@@' == sfx]
                    j = hit[0] if len(hit) == 1 else None
            if j is None:
                stats['root' if m == 0 else 'vbase_ambiguous'] += 1
                continue
            comp = rp = op = None
            for k in _subtree_chain(bases, j):
                p = bare(bases[k].name)
                vp = by_cls.get(p, {}).get(0)
                o, _w = ours(p, 0)
                if vp is not None and o is not None:
                    comp, rp, op = p, slots_of[vp], o
                    break
            if comp is None:
                stats['no_comparator'] += 1
                out.append(dict(cls=cls, off=m, kind='NO_COMPARATOR', retail_vt=hex(vc),
                                chain=[bare(bases[k].name) for k in _subtree_chain(bases, j)]))
                continue
            stats['compared_' + ('primary' if m == 0 else 'secondary')] += 1
            if comp != bare(bases[j].name):
                stats['compared_via_ancestor'] += 1
            n = min(len(rp), len(op), len(rc), len(oc))
            for i in range(1, n):
                r_inh = rc[i] == rp[i]
                o_inh = oc[i] == op[i]
                if r_inh == o_inh:
                    continue
                k = 'RETAIL_OVERRIDES' if o_inh else 'OURS_OVERRIDES'
                stats[k] += 1
                out.append(dict(cls=cls, off=m, table=tag, parent=comp, slot=i, kind=k,
                                retail_c=hex(rc[i]), retail_p=hex(rp[i]),
                                occ_c=occ[rc[i]], occ_p=occ[rp[i]],
                                ours_c=oc[i], ours_p=op[i]))
    for r in out:
        print(' '.join(f'{k}={v}' for k, v in r.items()))
    print(dict(stats))
    if a.json:
        json.dump(dict(stats=dict(stats), rows=out), open(a.json, 'w'), indent=1)
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--project-dir', default=ROOT)
    ap.add_argument('--json')
    ap.add_argument('--scope', nargs='*', default=['src/band3', 'src/system/bandobj'])
    ap.add_argument('--exclude', nargs='*', default=['src/network'],
                    help='classes ALSO defined here are dropped (--all-tables only)')
    ap.add_argument('--all-tables', action='store_true',
                    help='judge every retail table (secondary, virtual-base, roots) -- W16-OT')
    a = ap.parse_args()
    if a.all_tables:
        return all_tables(a)

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
