#!/usr/bin/env python3
"""W17-FAMILYSWEEP: sweep EVERY mapped _Rb_tree family against RETAIL BYTES.

WHY A SWEEP AND NOT A SCREEN
────────────────────────────
`tools/node_size_screen.py` screens ONE member kind (`_M_insert`) against the
`li r3,N` of the builder it calls. Its own docstring records the two limits
that make a clean run NOT a clearance: it screens only `_M_insert`, and its
map rule only fires for value_type < 8. Four defects were found through it and
each was adjudicated by hand (W9, W12, W14, W15).

THE INSTRUMENT
──────────────
Instead of sizing the mangled name with a hand-written parser, compare RETAIL's
node function against **OUR OWN COMPILED `_M_create_node` for the very tree the
map declares**:

    retail   li r3,N  +  the (mnemonic, displacement) sequence of its accesses
    ours     li r3,M  +  the same sequence, from build/45410914/src/**/*.obj

Registers are deliberately excluded from the signature -- they are the part
regalloc may permute; the WIDTH and OFFSET are the part that encodes the
value_type, which is what is being adjudicated. This subsumes

  * the SIZE test (different-size COMDATs cannot fold, so a size disagreement
    is a WRONG NAME and never an arbitrary ICF survivor), and
  * the COPY-SHAPE test W12 needed when size cannot discriminate: `set<G>` and
    `map<G,G>` BOTH allocate 0x14 and only the shape separates them (one
    halfword copy vs halfwords at +0 AND +2),

and it needs no mangled-type sizer, so it covers class-typed value_types where
`node_size_screen.pair_size()` returns None.

FOUR THINGS THAT MAKE A NAIVE VERSION OF THIS LIE, ALL HANDLED
──────────────────────────────────────────────────────────────
1. ⛔ **ICF FOLDS BUILDERS ACROSS TREES.** `0x8235c328` is physically reached by
   rows declaring `set<MoveDetector*>`, `set<ScoreType>` AND `set<TrackWidget*>`
   -- three real, different trees whose builders are byte-identical 4-byte word
   copies. So "several declared trees at one builder" is EXPECTED, not a defect,
   and grouping families BY BUILDER over-merges. A callee whose mapped callers
   declare >= 2 distinct trees is marked SHARED and never merged through; the
   rule calibrates itself off the data rather than off a hardcoded list.
2. ⛔ **A `li r3,N` IS NOT ALWAYS AN ALLOCATION.** `_M_erase` ends in
   `deallocate(node, sizeof(node))` and so also opens `li r3,N`. Its shape is
   loads at +8/+12 (left/right) with no node-base stores, where a real builder
   always STORES to +8 and +12. Conflating them compares a destructor against
   our constructor and reports a shape disagreement on every healthy tree.
3. ⛔ **BODY EXTENTS MUST COME FROM symbols.txt, NOT A DEFAULT WINDOW.** Reading
   a fixed 0x80 bytes runs off the end of short functions and attributes the
   NEXT function's `bl`s to this one, which fabricates call edges.
4. ⛔ **A `?$map@`-spelled row can never join a `?$_Rb_tree@`-spelled COMDAT.**
   `operator[]` rows are kept in the component for repair purposes but are not
   themselves size/shape-checked; the component's `_Rb_tree` key is.

Usage:
    python3 tools/rbtree_family_sweep.py [--json out.json] [--all] [--edges]
    python3 tools/rbtree_family_sweep.py --selftest    # prove it can FAIL
"""
import bisect
import collections
import glob
import json
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from retail_body import Img  # noqa: E402

ROOT = os.environ.get("RB3_ROOT", ".")
MAP = os.path.join(ROOT, "scripts/target_symbol_map.json")
SYMS = os.path.join(ROOT, "config/45410914/symbols.txt")
OBJDIR = os.path.join(ROOT, "build/45410914/src")

SYM_RX = re.compile(
    r"^(\S+) = \.text:0x([0-9A-Fa-f]+); // type:function size:0x([0-9A-Fa-f]+)")

# The MSVC boundary between a qualified NAME and its FUNCTION TYPE: `@@` then
# access + calling-convention letters (`@@QAA`, `@@IBA`, `@@YA`...). Inside
# template args a `@@` is followed by `@`, `?`, a digit back-reference or a
# nested token -- never two capitals then `A` -- so this is a safe cut.
FUNCTYPE_RX = re.compile(r"@@[A-Z][A-Z]A")

LOADS = {32: "lwz", 34: "lbz", 40: "lhz", 42: "lha", 48: "lfs", 50: "lfd"}
STORES = {36: "stw", 38: "stb", 44: "sth", 52: "stfs", 54: "stfd"}

MEMBER_RX = re.compile(
    r"^\?\??\$?("
    r"_M_insert|insert_unique|_M_create_node|_M_copy|_M_erase|clear|erase|"
    r"erase_unique|_M_lower_bound|_M_find|_M_fill_insert|_M_fill_insert_aux|"
    r"swap|begin|end|_M_clear|_M_clear_after_move|_M_destroy_node)@")

STL_ISH = ("stlpmtx_std", "_Rb_tree", "_Rb_global", "MemAlloc", "MemFree",
           "MemOrPool", "?$StlNodeAlloc", "?$_Select1st", "?$_Identity",
           "operator new", "??2@", "??3@", "?$vector@", "?$pair@")


def load_extents():
    ext = []
    with open(SYMS) as fh:
        for line in fh:
            m = SYM_RX.match(line)
            if m:
                ext.append((int(m.group(2), 16), int(m.group(3), 16)))
    ext.sort()
    return ext


def words(buf, limit=None):
    n = len(buf) // 4 * 4
    if limit:
        n = min(n, limit * 4)
    for i in range(0, n, 4):
        yield i, struct.unpack_from(">I", buf, i)[0]


def li_r3(buf, limit=24):
    for _, w in words(buf, limit):
        if (w >> 26) == 14 and ((w >> 21) & 31) == 3 and ((w >> 16) & 31) == 0:
            imm = w & 0xFFFF
            return imm - 0x10000 if imm & 0x8000 else imm
    return None


def mem_shape(buf):
    out = []
    for _, w in words(buf):
        op = w >> 26
        ra = (w >> 16) & 31
        imm = w & 0xFFFF
        if imm & 0x8000:
            imm -= 0x10000
        if ra == 1:                       # r1 == stack frame, not the node
            continue
        if op in LOADS:
            out.append((LOADS[op], imm))
        elif op in STORES:
            out.append((STORES[op], imm))
    return out


def bl_targets(buf, va):
    out = []
    for i, w in words(buf):
        if (w >> 26) == 18 and (w & 1):
            li = w & 0x03FFFFFC
            if li & 0x02000000:
                li -= 0x04000000
            out.append(li if ((w >> 1) & 1) else va + i + li)
    return out


def tree_key(name):
    """The declared `?$_Rb_tree@<args>` region, CUT AT THE FUNCTION TYPE.

    The cut is load-bearing: without it the key runs into the member's own
    signature, every row of one tree reads as a DIFFERENT tree, and the
    consistency test fires everywhere -- i.e. says nothing."""
    i = name.find("?$_Rb_tree@")
    if i < 0:
        for pat in ("?$map@", "?$set@", "?$multimap@"):
            j = name.find(pat)
            if j >= 0:
                i = j
                break
        else:
            return None
    m = FUNCTYPE_RX.search(name, i)
    return name[i:m.start()] if m else name[i:]


def member_kind(name):
    m = MEMBER_RX.match(name)
    if m:
        return m.group(1)
    if name.startswith(("??A?$map@", "??A?$multimap@")):
        return "operator[]"
    if name.startswith(("??4?$_Rb_tree@", "??4?$map@", "??4?$set@")):
        return "operator="
    if name.startswith(("??1?$_Rb_tree@", "??0?$_Rb_tree@", "??0?$_Rb_tree_base@")):
        return "ctor/dtor"
    return None


class Retail:
    """Bodies bounded by symbols.txt extents (never a default window)."""

    def __init__(self):
        self.img = Img()
        ext = load_extents()
        self.starts = [a for a, _ in ext]
        self.size = dict(ext)
        self._b = {}

    def extent(self, va):
        s = self.size.get(va)
        if s is not None:
            return s
        i = bisect.bisect_right(self.starts, va)
        if i < len(self.starts):
            return max(0, min(0x200, self.starts[i] - va))
        return 0

    def body(self, va):
        if va not in self._b:
            self._b[va] = self.img.read(va, self.extent(va))
        return self._b[va]


def classify_node_fn(R, va):
    """BUILDER / ERASER / None, plus its node size.

    A builder STORES the node-base left/right at +8/+12 after allocating; an
    eraser LOADS them at +8/+12 and deallocates. Both carry `li r3, node_size`,
    which is why conflating them compares a destructor against a constructor."""
    sz = R.extent(va)
    if not sz or sz > 0x180:
        return None, None
    b = R.body(va)
    n = li_r3(b, 24)
    if n is None or not (0x10 <= n <= 0x400):
        return None, None
    sh = mem_shape(b)
    if ("stw", 8) in sh and ("stw", 12) in sh and bl_targets(b, va):
        return "BUILDER", n
    if ("lwz", 8) in sh and ("lwz", 12) in sh:
        return "ERASER", n
    return None, None


def our_builders():
    """tree_key -> (li r3,N, mem_shape, symbol) from OUR compiled COMDATs."""
    sys.path.insert(0, os.path.join(ROOT, "tools"))
    from coff_bodies_ext import function_bodies_ext
    out, dup = {}, 0
    for path in glob.glob(os.path.join(OBJDIR, "**", "*.obj"), recursive=True):
        try:
            bodies = list(function_bodies_ext(path))
        except Exception:
            continue
        for name, body, _r, _e in bodies:
            if not name.startswith("?_M_create_node@"):
                continue
            k = tree_key(name)
            if k is None:
                continue
            if k in out:
                dup += 1
                continue
            out[k] = (li_r3(body), mem_shape(body), name)
    return out, dup


def sweep(smap):
    R = Retail()
    rows = {}
    for a, n in smap.items():
        if not any(t in n for t in ("_Rb_tree", "?$map@", "?$set@", "?$multimap@")):
            continue
        k = member_kind(n)
        if k:
            rows[a] = (k, n, tree_key(n))

    edges = {a: bl_targets(R.body(a), a) for a in rows}

    callers_trees = collections.defaultdict(set)
    for a, tgts in edges.items():
        t = rows[a][2]
        if t:
            for d in set(tgts):
                callers_trees[d].add(t)
    shared = {d for d, ts in callers_trees.items() if len(ts) > 1}

    parent = {}

    def find(x):
        parent.setdefault(x, x)
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    for a in rows:
        find(a)
    for a, tgts in edges.items():
        for d in set(tgts):
            if d not in shared and d in rows:
                ra, rb = find(a), find(d)
                if ra != rb:
                    parent[ra] = rb

    comps = collections.defaultdict(list)
    for a in rows:
        comps[find(a)].append(a)

    ours, dup = our_builders()

    def nodes_from(a, depth=3):
        """BFS to the first level that yields a BUILDER/ERASER."""
        seen, frontier, found = {a}, [a], {}
        for _ in range(depth):
            nxt = []
            for f in frontier:
                for d in bl_targets(R.body(f), f):
                    if d in seen:
                        continue
                    seen.add(d)
                    kind, n = classify_node_fn(R, d)
                    if kind:
                        found[d] = (kind, n)
                    else:
                        nxt.append(d)
            if found:
                return found
            frontier = nxt
        return found

    report = []
    for root, members in comps.items():
        members.sort()
        keys = collections.Counter(rows[a][2] for a in members)
        tkeys = [k for k in keys if k and k.startswith("?$_Rb_tree@")]
        nodes = {}
        for a in members:
            nodes.update(nodes_from(a))
        flags, checks = [], []
        if len({k for k in keys if k}) > 1:
            flags.append("MIXED_TREES")
        if not nodes:
            flags.append("NO_NODE_FN")
        for b, (kind, rn) in nodes.items():
            rs = mem_shape(R.body(b))
            for key in tkeys:
                if key not in ours:
                    checks.append((b, kind, rn, key, None, "OURS_ABSENT"))
                    continue
                on, osh, _nm = ours[key]
                if on != rn:
                    v = "SIZE_DISAGREES"
                elif kind == "BUILDER" and osh != rs:
                    v = "SHAPE_DISAGREES"
                else:
                    v = "OK"
                checks.append((b, kind, rn, key, on, v))
                if v != "OK":
                    flags.append(v)
        # a tree member whose body calls NAMED NON-STL code is not a tree member
        for a in members:
            foreign = [smap[d] for d in edges[a]
                       if d in smap and not any(s in smap[d] for s in STL_ISH)]
            if foreign:
                flags.append("FOREIGN_CALLEE")
        report.append(dict(root=root, members=members, keys=keys, nodes=nodes,
                           checks=checks, flags=sorted(set(flags))))
    return R, rows, edges, shared, ours, dup, report


def main():
    smap = {int(k, 16): v
            for k, v in json.load(open(MAP)).items()
            if k.startswith("0x") and v}

    if "--selftest" in sys.argv:
        # PROVE THE INSTRUMENT CAN FAIL: swap a healthy family's declared tree
        # for a real, different tree of a DIFFERENT node size and require the
        # sweep to flag it. A check that only ever passes proves nothing.
        good = sweep(smap)[6]
        base = sum(1 for r in good if "SIZE_DISAGREES" in r["flags"])
        victim = 0x822ddb48   # map<G,G>::_M_create_node, node 0x14
        donor = [n for a, n in smap.items()
                 if n.startswith("?_M_create_node@") and "$$CBHM@" in n]
        assert donor, "no donor symbol found"
        bad = dict(smap)
        bad[victim] = donor[0]           # claim the 0x14 builder is the 0x18 one
        got = sweep(bad)[6]
        after = sum(1 for r in got if "SIZE_DISAGREES" in r["flags"])
        print(f"SELFTEST size: clean={base} sabotaged={after} "
              f"=> {'PASS' if after > base else 'FAIL (instrument is vacuous)'}")
        # and a SHAPE-only sabotage: same node size 0x14, different value shape
        donor2 = [n for a, n in smap.items()
                  if n.startswith("?_M_create_node@") and "?$_Identity@G@" in n]
        if donor2:
            b2 = dict(smap)
            b2[victim] = donor2[0]       # claim map<G,G> builder is set<G>'s
            g2 = sweep(b2)[6]
            a2 = sum(1 for r in g2 if "SHAPE_DISAGREES" in r["flags"])
            c2 = sum(1 for r in good if "SHAPE_DISAGREES" in r["flags"])
            print(f"SELFTEST shape: clean={c2} sabotaged={a2} "
                  f"=> {'PASS' if a2 > c2 else 'FAIL (shape test is vacuous)'}")
        return

    R, rows, edges, shared, ours, dup, report = sweep(smap)
    print(f"tree-member rows in map: {len(rows)}")
    print(f"  distinct declared trees: {len({t for _k, _n, t in rows.values() if t})}")
    print(f"  ICF-shared callees (>=2 declaring trees, not merged through): {len(shared)}")
    print(f"  our compiled _M_create_node COMDATs by tree: {len(ours)} ({dup} dup keys)")
    flagged = [r for r in report if r["flags"]]
    print(f"  components: {len(report)}   FLAGGED: {len(flagged)}\n")

    tally = collections.Counter()
    for r in report:
        for f in r["flags"]:
            tally[f] += 1
    for f, n in tally.most_common():
        print(f"    {f:20s} {n}")
    print()

    show = report if "--all" in sys.argv else flagged
    for r in sorted(show, key=lambda r: r["members"][0]):
        print(f"--- component @0x{r['members'][0]:08x}  {len(r['members'])} rows  "
              f"[{' '.join(r['flags']) or 'ok'}]")
        for b, kind, rn, key, on, v in r["checks"]:
            ov = f"ours {on:#x}" if on is not None else "ours ABSENT"
            print(f"    {kind} 0x{b:08x} retail li r3,{rn:#x}  {ov}  {v}")
            if v == "SHAPE_DISAGREES":
                print(f"      retail {mem_shape(R.body(b))}")
                print(f"      ours   {ours[key][1]}")
            print(f"      key {key[:120]}")
        for a in r["members"]:
            kind, n, _t = rows[a]
            print(f"      0x{a:08x} {kind:16s} {n[:112]}")
            if "--edges" in sys.argv:
                for d in edges[a]:
                    tag = smap.get(d, "<anon>")[:80]
                    print(f"          -> 0x{d:08x} {tag}")
        print()

    if "--json" in sys.argv:
        out = sys.argv[sys.argv.index("--json") + 1]
        ser = [dict(members=[hex(x) for x in r["members"]],
                    keys={k: v for k, v in r["keys"].items()},
                    nodes={hex(k): v for k, v in r["nodes"].items()},
                    checks=[[hex(c[0])] + list(c[1:]) for c in r["checks"]],
                    flags=r["flags"]) for r in report]
        json.dump(ser, open(out, "w"), indent=1, default=str)
        print(f"wrote {out}")


if __name__ == "__main__":
    main()
