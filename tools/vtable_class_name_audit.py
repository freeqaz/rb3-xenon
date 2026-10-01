#!/usr/bin/env python3
"""vtable_class_name_audit -- does the class in a map name agree with the vtable
its retail body installs?

Provenance: lane W16-MA (2026-10-01), generalising lane W16-LB's single find.
W16-LB found that `0x82270298` was named `??1ObjRef@@QAA@XZ` while its retail
body (`lis/addi/stw r11,0(r3)/blr`) installs the vtable whose COL names
`.?AVObjRef@@` -- which in our build is `ObjRefOwner`.  That one name was worth
743 rows / +29,704 B.  This tool asks the same question of EVERY map row.

THE WITNESS
-----------
Retail bytes only.  For each map row the body is decoded over retail's own
extent (`.pdata`; for a leaf with no `.pdata` entry, a straight-line scan to the
first `blr`/`bctr`/`b`, labelled LEAF).  A this-tracking decoder follows r3 and
its `mr` copies (dropped across `bl`) and records every `stw <vtable>, d(this)`.

  * a constructor's OWN class is its LAST offset-0 vtable store (inlined base
    ctors store the base vtables first);
  * a destructor's (`??1`, `??_G`, `??_E`) is its FIRST offset-0 store (inlined
    base dtors restore base vtables afterwards).

The class is read from the vtable's Complete Object Locator (tools/retail_rtti),
and compared with the class in the map name AFTER BOTH ARE DEMANGLED by
`llvm-undname`, so template back-references compare correctly.  Two dialect
differences between our build and retail are normalised on both sides:
  * a defaulted trailing `ObjectDir` template argument (`ObjPtr<T>` ours vs
    `ObjPtr<T,ObjectDir>` retail; DTOR-A's TRAP 5);
  * `ObjRefOwner` (ours) == `ObjRef` (retail; icf_pair_adjudicate CLASS_RENAMES).

A second, independent witness is vtable OWNERSHIP: which retail vtables hold the
row's address as a slot.  For a virtual special member it names the class too.

VERDICTS (special members)
--------------------------
  AGREE       the stored/owning class equals the name's class
  DISAGREE    the own-class store names a different class        <- the defect
  OWNER_ONLY_DISAGREE  no own store; vtable ownership names a different class
  NO_STORE    no offset-0 vtable store and no ownership evidence
  UNBOUNDED   no extent could be established
Other member functions that store a vtable at 0(this) are reported as
NONSPECIAL_STORE for hand review -- a method that re-constructs `*this` is
legitimate, so these are not verdicts.

--slots (lane W16-NC)
--------------------
Every map row whose address sits in a retail vtable slot (~7,700) is asked two
independent questions: is the name's class an OWNER of a vtable holding the
address (or a retail BASE_OF_OWNER), and does OUR vtable of that class hold the
same name at the same slot?  See Audit.slot_run for what a disagreement does
and does not mean -- most residue is ICF folds of tiny bodies, and a rename is
made only on retail-byte evidence AFTER checking the address's direct callers.

Read-only.  Usage:
    python3 tools/vtable_class_name_audit.py [--json OUT] [--map PATH]
    python3 tools/vtable_class_name_audit.py --selftest
    python3 tools/vtable_class_name_audit.py --slots [--json OUT]
    python3 tools/vtable_class_name_audit.py --slots --selftest
"""
from __future__ import annotations

import argparse
import collections
import json
import os
import re
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
from tools.retail_rtti import RetailRtti  # noqa: E402

MAP = os.path.join(ROOT, "scripts/target_symbol_map.json")

BLR, BCTR = 0x4E800020, 0x4E800420
LEAF_MAX_WORDS = 64

# opcode -> which field it writes (rD = bits 21-25, rA = bits 16-20)
WRITES_RD = {7, 8, 12, 13, 32, 33, 34, 35, 40, 41, 42, 43, 46, 58}
WRITES_RA = {20, 21, 23, 25, 26, 27, 28, 29, 30}
UPDATE_FORMS = {33, 35, 37, 39, 41, 43, 45, 49, 51, 53, 55}   # also write rA
X31_WRITES_RA = {28, 60, 124, 284, 316, 412, 444, 476, 24, 536, 792, 824,
                 26, 954, 922, 986, 27, 539, 794, 413, 58}
X31_STORES = {151, 183, 215, 247, 407, 439, 150, 662, 918, 663, 695, 727,
              759, 149, 181, 214, 983}
X31_STORE_UPDATE = {183, 247, 439, 695, 759, 181}

SPECIAL = ("??0", "??1", "??_G", "??_E")


# --------------------------------------------------------------- decoding ---
def leaf_extent(R, a):
    for i in range(LEAF_MAX_WORDS):
        w = R.u32(a + 4 * i)
        if w is None:
            return None
        if w in (BLR, BCTR) or ((w >> 26) == 18 and not (w & 1)):
            return 4 * (i + 1)
    return None


def _is_gpr_helper(R, pc, w):
    """A `bl` into the prologue/epilogue GPR save/restore helpers.  Their
    bodies are a run of `std rN,off(r1)` / `ld rN,off(r1)`.  Treating such a
    `bl` as a call kills r3 (`this`) before the first vtable store -- measured:
    0x8262c028 (~StickerProvider, `bl __savegprlr_28` at +4) read NO_STORE."""
    li = w & 0x03FFFFFC
    if li & 0x02000000:
        li -= 0x04000000
    t = R.u32((pc + li) & 0xFFFFFFFF)
    return t is not None and (t >> 26) in (58, 62) and ((t >> 16) & 31) == 1


def this_stores(R, a, size):
    """[(index, disp, vtable_va)] for every `stw <const>, d(this)`."""
    regs, this, out = {}, {3}, []

    def clr(r):
        regs.pop(r, None)
        this.discard(r)

    for i in range(size // 4):
        w = R.u32(a + 4 * i)
        if w is None:
            break
        op = w >> 26
        d, s_a, imm = (w >> 21) & 31, (w >> 16) & 31, w & 0xFFFF
        if op == 15:                                   # addis / lis
            base = 0 if s_a == 0 else regs.get(s_a)
            this.discard(d)
            if base is None:
                regs.pop(d, None)
            else:
                regs[d] = (base + (imm << 16)) & 0xFFFFFFFF
        elif op == 14:                                 # addi
            simm = imm - 0x10000 if imm & 0x8000 else imm
            base = 0 if s_a == 0 else regs.get(s_a)
            this.discard(d)
            if base is None:
                regs.pop(d, None)
            else:
                regs[d] = (base + simm) & 0xFFFFFFFF
        elif op == 24:                                 # ori rA, rS, imm
            if w == 0x60000000:
                continue                               # nop
            base = regs.get(d)
            this.discard(s_a)
            if base is None:
                regs.pop(s_a, None)
            else:
                regs[s_a] = base | imm
        elif op == 31:
            xo = (w >> 1) & 0x3FF
            rb = (w >> 11) & 31
            if xo == 444 and d == rb:                  # mr rA, rS
                if d in regs:
                    regs[s_a] = regs[d]
                else:
                    regs.pop(s_a, None)
                if d in this:
                    this.add(s_a)
                else:
                    this.discard(s_a)
            elif xo in X31_STORES:
                if xo in X31_STORE_UPDATE:
                    clr(s_a)
            elif xo in X31_WRITES_RA:
                clr(s_a)
            else:
                clr(d)
        elif op in (36, 37):                           # stw / stwu
            disp = imm - 0x10000 if imm & 0x8000 else imm
            if d in regs and s_a in this:
                out.append((i, disp, regs[d]))
            if op == 37:
                clr(s_a)
        elif op == 18 and (w & 1):                     # bl: volatile regs die
            if _is_gpr_helper(R, a + 4 * i, w):
                continue      # __savegprlr_N / __restgprlr_N touch no arg reg
            for r in (0,) + tuple(range(3, 13)):
                clr(r)
        else:
            if op in WRITES_RD:
                clr(d)
            if op in WRITES_RA or op in UPDATE_FORMS:
                clr(s_a)
    return out


# ------------------------------------------------------------ demangling ---
def undname_many(names):
    """{mangled: demangled} -- keyed on the ECHOED input line, so an
    `error: Invalid mangled name` line cannot shift the pairing."""
    names = list(dict.fromkeys(names))
    if not names:
        return {}
    p = subprocess.run(["llvm-undname"], input="\n".join(names) + "\n",
                       capture_output=True, text=True, check=False)
    want, out = set(names), {}
    lines = p.stdout.split("\n")
    for i in range(len(lines) - 1):
        if lines[i] in want and lines[i] not in out:
            nxt = lines[i + 1]
            out[lines[i]] = None if nxt.startswith("error:") else nxt
    return out


def _depth_scan(s):
    d = 0
    for i, c in enumerate(s):
        yield i, c, d
        if c in "<(":
            d += 1
        elif c in ">)":
            d -= 1


def qualified_name(dem):
    """Qualified name of a demangled FUNCTION (text before its parameter list)."""
    if not dem or dem.endswith("'") and "(" not in dem:
        return None
    # parameter list = the last top-level (...) group
    depth, close = 0, None
    for i in range(len(dem) - 1, -1, -1):
        c = dem[i]
        if c == ")":
            if depth == 0 and close is None:
                close = i
            depth += 1
        elif c == "(":
            depth -= 1
            if depth == 0 and close is not None:
                head = dem[:i]
                break
        elif c in "<>":
            pass
    else:
        return None
    # walk back to the start of the qualified name (space at template depth 0,
    # outside a `quoted' special name such as `scalar deleting dtor')
    depth, quoted = 0, False
    for j in range(len(head) - 1, -1, -1):
        c = head[j]
        if c == "'":
            quoted = True
        elif c == "`":
            quoted = False
        elif quoted:
            continue
        elif c == ">":
            depth += 1
        elif c == "<":
            depth -= 1
        elif c == " " and depth == 0:
            return head[j + 1:]
    return head


def split_scope(q):
    parts, cur, depth, i = [], "", 0, 0
    while i < len(q):
        c = q[i]
        if c == "`":
            depth += 1
        elif c == "'":
            depth -= 1
        if c in "<(":
            depth += 1
        elif c in ">)":
            depth -= 1
        if depth == 0 and q.startswith("::", i):
            parts.append(cur)
            cur = ""
            i += 2
            continue
        cur += c
        i += 1
    parts.append(cur)
    return parts


def class_of_function(dem):
    q = qualified_name(dem)
    if not q:
        return None
    parts = split_scope(q)
    return "::".join(parts[:-1]) if len(parts) > 1 else None


def _split_targs(s):
    out, cur, depth = [], "", 0
    for c in s:
        if c in "<(":
            depth += 1
        elif c in ">)":
            depth -= 1
        if c == "," and depth == 0:
            out.append(cur)
            cur = ""
        else:
            cur += c
    out.append(cur)
    return out


#: retail RTTI class -> the class OUR build names for it.  DIRECTIONAL: a map
#: name is a pairing key against our build, so retail's spelling is translated
#: into ours, never the reverse.  Our `ObjRef` has no vtable in the match build
#: (OBJREF_VIRTUAL is empty without HX_NATIVE), so retail's `.?AVObjRef@@` can
#: only be our `ObjRefOwner`.  A symmetric rename makes the W16-LB defect
#: (`??1ObjRef` at 0x82270298) read AGREE -- measured, see --selftest.
RETAIL_TO_OURS = {"ObjRef": "ObjRefOwner"}


def retail_to_ours(cls):
    if cls is None:
        return None
    for k, v in RETAIL_TO_OURS.items():
        cls = re.sub(r"\b%s\b" % k, v, cls)
    return cls


def norm(cls):
    if cls is None:
        return None
    s = re.sub(r"\b(class|struct|union|enum) ", "", cls).replace(" ", "")

    def fix(t):
        # recursively drop a defaulted trailing ObjectDir argument (>=2 args)
        out, i = "", 0
        while i < len(t):
            j = t.find("<", i)
            if j < 0:
                return out + t[i:]
            out += t[i:j + 1]
            depth, k = 1, j + 1
            while k < len(t) and depth:
                if t[k] == "<":
                    depth += 1
                elif t[k] == ">":
                    depth -= 1
                k += 1
            inner = t[j + 1:k - 1]
            args = [fix(a) for a in _split_targs(inner)]
            if len(args) >= 2 and args[-1] == "ObjectDir":
                args = args[:-1]
            out += ",".join(args) + ">"
            i = k
        return out

    return fix(s)


# -------------------------------------------------------------- the audit ---
class Audit:
    def __init__(self, map_path=MAP):
        self.R = RetailRtti()
        raw = json.load(open(map_path))
        self.map = {int(k, 16): v for k, v in raw.items()
                    if k.startswith("0x") and isinstance(v, str) and v}
        self._vt_index = None

    def extent(self, a):
        e = self.R.function_extent(a)
        if e is not None:
            return "PDATA", e
        e = leaf_extent(self.R, a)
        return ("LEAF", e) if e else ("UNBOUNDED", None)

    def vt_index(self):
        """fn VA -> [(retail class, vtable VA, slot)] over every retail vtable."""
        if self._vt_index is not None:
            return self._vt_index
        R = self.R
        text = [s for s in R.sections if s.name == ".text"][0]
        lo, hi = text.va, text.va + text.vsize
        idx = collections.defaultdict(list)
        self.col_of_head = {}
        for s in R.sections:
            if not s.has_data or s.name not in (".rdata", ".data"):
                continue
            blk = R.data[s.rawptr:s.rawptr + s.rawsize]
            for off in range(0, len(blk) - 3, 4):
                col = struct.unpack_from(">I", blk, off)[0]
                if not R.is_image_va(col) or lo <= col < hi:
                    continue
                head = s.va + off + 4
                cls = R.class_of_vtable(head)
                if not cls:
                    continue
                self.col_of_head[head] = col
                k = 0
                while True:
                    w = R.u32(head + 4 * k)
                    if w is None or not (lo <= w < hi):
                        break
                    if k and R.class_of_vtable(head + 4 * k):
                        break
                    idx[w].append((cls, head, k))
                    k += 1
        self._vt_index = idx
        return idx

    def run(self, rows=None):
        R = self.R
        rows = rows if rows is not None else sorted(self.map)
        names = [self.map[a] for a in rows]
        dem = undname_many(names)
        vt_names = {}
        idx = self.vt_index()
        recs = []
        for a in rows:
            n = self.map[a]
            kind = next((p for p in SPECIAL if n.startswith(p)), None)
            src, size = self.extent(a)
            stores = this_stores(R, a, size) if size else []
            classed = [(i, d, v, R.class_of_vtable(v)) for i, d, v in stores]
            # an offset-0 store of a constant into .rdata/.data with NO COL is a
            # vtable from a /GR- object (the CRT's bad_cast/bad_typeid): the
            # own-class store is there but unlabelled.  It must not be skipped
            # in favour of a resolvable BASE store (measured: 0x82829b90).
            unl0 = [c for c in classed if c[1] == 0 and not c[3]
                    and R.section_of(c[2]) in (".rdata", ".data")]
            classed = [c for c in classed if c[3]]
            off0 = [c for c in classed if c[1] == 0]
            own = None
            decisive_unlabelled = False
            if off0 or unl0:
                seq = sorted(off0 + unl0)
                pick = seq[-1] if kind == "??0" else seq[0]
                if pick[3]:
                    own = pick
                else:
                    decisive_unlabelled = True
            owners = sorted({c for c, _h, _k in idx.get(a, [])})
            if kind is None and not off0:
                continue                      # ordinary row with no this-store
            name_cls = class_of_function(dem.get(n))
            for t in [own[3]] if own else []:
                vt_names.setdefault(t, None)
            for t in owners:
                vt_names.setdefault(t, None)
            recs.append(dict(addr=a, name=n, kind=kind or "other", extent=src,
                             size=size, name_cls=name_cls,
                             own_vt=own[2] if own else None,
                             own_td=own[3] if own else None,
                             all_off0=[c[3] for c in off0],
                             members=[(c[1], c[3]) for c in classed if c[1] != 0],
                             owners=owners, unlabelled=decisive_unlabelled))
        # demangle every retail TypeDescriptor seen, via its vftable symbol
        vt_dem = undname_many("??_7" + t[4:] + "6B@" for t in vt_names)

        # retail hierarchy, in OUR dialect: class -> set(base classes)
        all_tds = set()
        hier_raw = {}
        for head, col in self.col_of_head.items():
            r3 = R.bases_of_col(col)
            if not r3:
                continue
            c, _chd, bases = r3
            me = R.td_name(c.ptd)
            if not me:
                continue
            bn = [b.name for b in bases if b.name]
            hier_raw.setdefault(me, set()).update(bn)
            all_tds.add(me)
            all_tds.update(bn)
        vt_dem.update(undname_many("??_7" + t[4:] + "6B@" for t in all_tds
                                   if "??_7" + t[4:] + "6B@" not in vt_dem))

        def td_cls(t):
            d = vt_dem.get("??_7" + t[4:] + "6B@")
            if d and d.startswith("const ") and d.endswith("::`vftable'"):
                return d[6:-len("::`vftable'")]
            return None

        hier = {}
        for me, bn in hier_raw.items():
            k = norm(retail_to_ours(td_cls(me)))
            if k:
                hier.setdefault(k, set()).update(
                    norm(retail_to_ours(td_cls(b))) for b in bn)
        self.hier = hier
        for r in recs:
            nc = norm(r["name_cls"])
            r["own_cls"] = retail_to_ours(td_cls(r["own_td"])) if r["own_td"] else None
            r["owner_cls"] = [retail_to_ours(td_cls(t)) for t in r["owners"]]
            oc = norm(r["own_cls"])
            owner_n = {norm(x) for x in r["owner_cls"] if x}
            if r["kind"] == "other":
                r["verdict"] = ("NONSPECIAL_AGREE" if nc and oc == nc
                                else "NONSPECIAL_STORE")
            elif r["size"] is None:
                r["verdict"] = "UNBOUNDED"
            elif r["unlabelled"]:
                r["verdict"] = "UNLABELLED_VTABLE"
            elif oc:
                r["verdict"] = "AGREE" if oc == nc else "DISAGREE"
            elif owner_n:
                r["verdict"] = ("AGREE_OWNER" if nc in owner_n
                                else "OWNER_ONLY_DISAGREE")
            else:
                r["verdict"] = "NO_STORE"
            r["owner_agrees"] = (nc in owner_n) if owner_n else None
            # refine a disagreement against retail's own hierarchy
            if r["verdict"] in ("DISAGREE", "OWNER_ONLY_DISAGREE"):
                got = oc if oc else None
                if nc not in hier:
                    r["sub"] = "NAME_CLASS_NOT_IN_RETAIL"
                elif got and got in hier[nc]:
                    r["sub"] = "STORE_IS_BASE_OF_NAME"
                elif got and nc in hier.get(got, ()):
                    r["sub"] = "NAME_IS_BASE_OF_STORE"
                elif not got and any(nc in hier.get(o, ()) for o in owner_n):
                    r["sub"] = "NAME_IS_BASE_OF_OWNER"
                else:
                    r["sub"] = "UNRELATED"
            else:
                r["sub"] = ""
        return recs


    # ------------------------------------------------ --slots (W16-NC) ---
    def hierarchy(self):
        """Retail class hierarchy in OUR dialect: class -> set(all bases).

        MSVC's Base Class Array lists every ancestor (transitively), so one
        lookup answers "is X an ancestor of Y"."""
        if getattr(self, "_hier", None) is not None:
            return self._hier, self._tdc
        R = self.R
        self.vt_index()
        tds, raw = set(), {}
        for _head, col in self.col_of_head.items():
            r3 = R.bases_of_col(col)
            if not r3:
                continue
            c, _chd, bases = r3
            me = R.td_name(c.ptd)
            if not me:
                continue
            bn = [b.name for b in bases if b.name]
            tds.add(me)
            tds.update(bn)
            raw.setdefault(me, set()).update(bn)
        vd = undname_many("??_7" + t[4:] + "6B@" for t in tds)

        def tdc(t):
            d = vd.get("??_7" + t[4:] + "6B@")
            if d and d.startswith("const ") and d.endswith("::`vftable'"):
                return norm(retail_to_ours(d[6:-len("::`vftable'")]))
            return None
        hier = {}
        for me, bn in raw.items():
            k = tdc(me)
            if k:
                hier.setdefault(k, set()).update(x for x in map(tdc, bn) if x)
        self._hier, self._tdc = hier, tdc
        return hier, tdc

    def slot_run(self, rows=None, project_dir=ROOT):
        """Audit every map row that sits in a retail vtable slot.

        Two independent questions per row:
          rel  -- is the map name's class an OWNER of a vtable holding the
                  address, a retail BASE_OF_OWNER (inherited, not overridden),
                  UNRELATED to every owner, or a NAME_CLASS_NOT_IN_RETAIL?
          ours -- does OUR vtable of the same class, joined offset-to-offset
                  through our own COLs (vtable_order_sweep.our_vtable_by_offset),
                  hold the SAME name at the same slot (OURS_SAME), a different
                  one (OURS_DIFF), or can it not be read (OURS_NONE)?
        `??_G` and `??_E` of one class are treated as the same name: our
        vtables reference the vector deleting dtor where retail's slot body is
        named for the scalar one (measured: ~580 rows, all at 100).

        ⚠ A disagreement is a CANDIDATE, never a verdict.  Most survivors are
        ICF folds of 4-20 byte bodies (getters, `blr`, adjustor thunks) whose
        map spelling is one valid member of the fold.  W16-NC renamed only on
        retail-byte evidence -- a thunk's branch target, the class-name literal a
        ByteCode body constructs, a body shape the old name contradicts -- and
        then CHECKED CALLERS: naming an address that is a fold reached by direct
        `bl` from unrelated callers charged 13 rows off 100 in one wave.
        """
        sys.path.insert(0, os.path.join(ROOT, "tools"))
        import vtable_order_sweep as V
        R = self.R
        idx = self.vt_index()
        hier, tdc = self.hierarchy()
        known = set(hier) | {x for s in hier.values() for x in s}
        rows = [a for a in (rows if rows is not None else sorted(self.map)) if a in idx]
        dem = undname_many(self.map[a] for a in rows)
        cache = {}

        def our_slot(td, head, k):
            if (td, head) not in cache:
                sub_off, _b = V.retail_subobject_base(R, head)
                cache[(td, head)] = V.our_vtable_by_offset(V.bare_class(td), project_dir, sub_off)
            ours, _how = cache[(td, head)]
            return ours[k]["symbol"] if ours and k < len(ours) else None

        def ge(n):
            return re.sub(r"^\?\?_[GE]", "??_X", n or "")
        out = []
        for a in rows:
            n = self.map[a]
            C = norm(class_of_function(dem.get(n)))
            occ = [(tdc(td), td, head, k) for td, head, k in idx[a]]
            oc = {o for o, *_ in occ if o}
            if C and C in oc:
                rel = "OWNER"
            elif C and any(C in hier.get(o, ()) for o in oc):
                rel = "BASE_OF_OWNER"
            elif not C:
                rel = "NO_CLASS"
            elif C not in known:
                rel = "NAME_CLASS_NOT_IN_RETAIL"
            else:
                rel = "UNRELATED"
            slots = [(o, k, our_slot(td, head, k)) for o, td, head, k in occ]
            names = {ge(s) for _o, _k, s in slots if s}
            ours = ("OURS_SAME" if ge(n) in names else
                    "OURS_DIFF" if names else "OURS_NONE")
            out.append(dict(addr=a, name=n, cls=C, rel=rel, ours=ours,
                            owners=sorted(oc), slots=slots,
                            size=(R.function_extent(a) or 0)))
        return out


# ------------------------------------------- witness 2: OUR compiled build ---
OBJROOT = os.path.join(ROOT, "build/45410914/src")


def coff_vtable_refs(path):
    """{function symbol: [vtable symbol, ...] in relocation order} for every
    function COMDAT in one of OUR objs (a `??_7...6B@` relocation target)."""
    d = open(path, "rb").read()
    if len(d) < 20:
        return {}
    _m, nsec, _ts, symoff, nsym, opt, _ch = struct.unpack_from("<HHlIIHH", d, 0)
    if not symoff or not nsym or symoff + 18 * nsym > len(d):
        return {}
    strt = symoff + 18 * nsym
    names, sec_of = {}, {}
    i = 0
    while i < nsym:
        o = symoff + 18 * i
        if d[o:o + 4] == b"\0\0\0\0":
            off, = struct.unpack_from("<I", d, o + 4)
            e = d.find(b"\0", strt + off)
            nm = d[strt + off:e].decode("latin1")
        else:
            nm = d[o:o + 8].rstrip(b"\0").decode("latin1")
        secnum, = struct.unpack_from("<h", d, o + 12)
        typ, = struct.unpack_from("<H", d, o + 14)
        sclass, naux = d[o + 16], d[o + 17]
        names[i] = nm
        if secnum > 0 and (typ & 0x20) and sclass in (2, 3):     # function
            sec_of.setdefault(secnum, []).append(nm)
        i += 1 + naux
    out = {}
    for k in range(nsec):
        h = 20 + opt + 40 * k
        nreloc, = struct.unpack_from("<H", d, h + 32)
        preloc, = struct.unpack_from("<I", d, h + 24)
        fns = sec_of.get(k + 1)
        if not fns:
            continue
        vts = []
        for j in range(nreloc):
            _va, si, _t = struct.unpack_from("<IIH", d, preloc + 10 * j)
            t = names.get(si, "")
            if t.startswith("??_7") and (not vts or vts[-1] != t):
                vts.append(t)
        for f in fns:
            out.setdefault(f, vts)
    return out


def our_build_index(objroot=OBJROOT):
    idx = {}
    for dp, _dn, fns in os.walk(objroot):
        for fn in fns:
            if fn.endswith(".obj"):
                try:
                    for k, v in coff_vtable_refs(os.path.join(dp, fn)).items():
                        idx.setdefault(k, (v, os.path.join(dp, fn)))
                except (OSError, struct.error):
                    pass
    return idx


def ours_witness(recs, idx):
    """Classify every disagreeing row against OUR compiled body of that name."""
    want = [r for r in recs if r["verdict"] in ("DISAGREE", "OWNER_ONLY_DISAGREE")]
    allv = {v for r in want if r["name"] in idx for v in idx[r["name"]][0]}
    dem = undname_many(allv)

    def vcls(v):
        x = dem.get(v)
        if x and x.startswith("const ") and "::`vftable'" in x:
            return norm(x[6:x.index("::`vftable'")])
        return None

    for r in want:
        hit = idx.get(r["name"])
        retail = norm(r["own_cls"]) if r["own_cls"] else None
        if hit is None:
            r["ours"] = "UNEMITTED"
            continue
        ours = [vcls(v) for v in hit[0]]
        r["ours_vts"] = ours
        if not ours:
            r["ours"] = "OURS_STORES_NONE"
        elif retail is None:
            r["ours"] = "OURS_STORES_" + ("SAME_OWNER" if any(
                norm(o) in ours for o in r["owner_cls"] if o) else "OTHER")
        else:
            own = ours[-1] if r["kind"] == "??0" else ours[0]
            r["ours"] = ("OURS_SAME_OWN" if own == retail else
                         "OURS_HAS_IT" if retail in ours else "OURS_DIFFERENT")


def fanin(recs):
    sys.path.insert(0, os.path.join(ROOT, "tools"))
    from retail_callers import bl_sites
    hits = bl_sites([r["addr"] for r in recs])
    for r in recs:
        r["bl_fanin"] = len(hits[r["addr"]])


def selftest():
    """The W16-LB positive.  0x82270298 installs retail's `.?AVObjRef@@`
    vtable.  Under its OLD map name (`??1ObjRef@@QAA@XZ`) it must read
    DISAGREE, under its current name AGREE, and under a sabotage name DISAGREE.
    Then the directional translation is switched OFF: the old name must flip to
    AGREE, proving the translation is what lets the instrument see the defect."""
    global RETAIL_TO_OURS
    A = Audit()
    ok = True
    base = A.map.get(0x82270298)
    cases = (("??1ObjRef@@QAA@XZ", "DISAGREE"),
             ("??1ObjRefOwner@@UAA@XZ", "AGREE"),
             ("??1RndMesh@@UAA@XZ", "DISAGREE"))
    # gpr-helper control: ~StickerProvider at 0x8262c028 calls __savegprlr_28
    # at +4 and must still be seen storing StickerProvider's vtable.
    st = this_stores(A.R, 0x8262c028, 128)
    print("  0x8262c028 stores:", [(d, hex(v)) for _i, d, v in st])
    ok &= any(d == 0 and A.R.class_of_vtable(v) == ".?AVStickerProvider@@" for _i, d, v in st)
    for nm, want in cases:
        A.map[0x82270298] = nm
        v = A.run([0x82270298])[0]["verdict"]
        print(f"  0x82270298 as {nm:28s} -> {v:10s} (want {want})")
        ok &= v == want
    saved, RETAIL_TO_OURS = RETAIL_TO_OURS, {}
    A.map[0x82270298] = "??1ObjRef@@QAA@XZ"
    v = A.run([0x82270298])[0]["verdict"]
    print(f"  translation OFF, ??1ObjRef       -> {v:10s} (want AGREE: blind without it)")
    ok &= v == "AGREE"
    RETAIL_TO_OURS = saved
    A.map[0x82270298] = base
    # the ObjectDir default-arg normaliser must be load-bearing on a real row:
    print("  norm('ObjPtr<class RndMesh,class ObjectDir>') =",
          norm("ObjPtr<class RndMesh,class ObjectDir>"))
    ok &= norm("ObjPtr<class RndMesh,class ObjectDir>") == "ObjPtr<RndMesh>"
    ok &= norm("ObjDirPtr<class ObjectDir>") == "ObjDirPtr<ObjectDir>"
    print("SELFTEST", "PASS" if ok else "FAIL")
    return 0 if ok else 1


def slot_selftest():
    """W16-NC positives for --slots.  Each defect must read as a disagreement
    under its OLD name and agree under the corrected one, and a sabotage name
    must disagree -- so the instrument is shown to fail before it is trusted."""
    A = Audit()
    ok = True
    cases = (
        # swapped pair: class agrees, our slot names a different method
        (0x82b61d08, "?UpdateMix@FxSendDelay360@@UAAXXZ", ("OWNER", "OURS_DIFF")),
        (0x82b61d08, "?Recreate@FxSendDelay360@@UAAXAAV?$vector@PAVFxSend@@V?$StlNodeAlloc@PAVFxSend@@@stlpmtx_std@@@stlpmtx_std@@@Z",
         ("OWNER", "OURS_SAME")),
        # a Dance Central class pinned on RB3 code: no retail RTTI at all
        (0x822c0b50, "?Copy@CrazeHollaback@@UAAXPBVObject@Hmx@@W4CopyType@23@@Z", ("NAME_CLASS_NOT_IN_RETAIL", "OURS_DIFF")),
        (0x822c0b50, "?Copy@BandSongPref@@UAAXPBVObject@Hmx@@W4CopyType@23@@Z", ("OWNER", "OURS_SAME")),
        # sabotage: an unrelated retail class must not read as agreement
        (0x822c0b50, "?Copy@RndMesh@@UAAXPBVObject@Hmx@@W4CopyType@23@@Z", ("UNRELATED", "OURS_DIFF")),
        # inherited, not overridden: SongSort::NewShortcutNode sits only in its
        # subclasses' vtables (SongSortByArtist/Diff/Plays/...).  (0x8269d940,
        # Object::PreLoad, was the first pick and reads OWNER -- Hmx::Object's
        # own vtable holds it too; a wrong expectation, not a tool defect.)
        (0x825bf308, "?NewShortcutNode@SongSort@@UBAPAVShortcutNode@@PAVLeafSortNode@@@Z", ("BASE_OF_OWNER", "OURS_SAME")),
    )
    saved = dict(A.map)
    for a, nm, want in cases:
        A.map[a] = nm
        r = A.slot_run([a])[0]
        got = (r["rel"], r["ours"])
        print(f"  0x{a:08x} as {nm[:44]:44s} -> {got} (want {want})")
        ok &= got == want
        A.map = dict(saved)
    print("SLOT SELFTEST", "PASS" if ok else "FAIL")
    return 0 if ok else 1


def slot_main(a):
    A = Audit(a.map)
    recs = A.slot_run()
    c = collections.Counter((r["rel"], r["ours"]) for r in recs)
    print(f"{len(recs)} map rows sit in a retail vtable slot")
    for k in sorted(c):
        print(f"  {k[0]:26s} {k[1]:10s} {c[k]:6d}")
    cand = [r for r in recs if r["rel"] in ("UNRELATED", "NAME_CLASS_NOT_IN_RETAIL", "NO_CLASS")
            or (r["ours"] == "OURS_DIFF")]
    cand.sort(key=lambda r: -r["size"])
    print(f"\n{len(cand)} candidates (class or our-slot disagreement), largest body first:")
    for r in cand:
        sl = sorted({(o or "?", k, (s or "-")[:50]) for o, k, s in r["slots"]})[:2]
        print(f"  0x{r['addr']:08x} {r['size']:5d} {r['rel'][:12]:12s} {r['ours']:9s} {r['name'][:60]:60s} {sl}")
    if a.json:
        json.dump(recs, open(a.json, "w"), indent=1, default=str)
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--json")
    ap.add_argument("--map", default=MAP)
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--slots", action="store_true",
                    help="audit every row in a retail vtable slot (W16-NC)")
    a = ap.parse_args()
    if a.selftest and a.slots:
        return slot_selftest()
    if a.selftest:
        return selftest()
    if a.slots:
        return slot_main(a)
    A = Audit(a.map)
    recs = A.run()
    fanin(recs)
    ours_witness(recs, our_build_index())
    nofail = [r for r in recs if r["kind"] != "other" and not r["name_cls"]]
    print(f"name-class PARSE FAILURES on special members: {len(nofail)}")
    for r in nofail[:10]:
        print(f"   0x{r['addr']:08x} {r['name']}")
    c = collections.Counter((r["kind"], r["verdict"], r["sub"], r.get("ours", ""))
                            for r in recs)
    for k in sorted(c):
        print(f"{k[0]:6s} {k[1]:20s} {k[2]:26s} {k[3]:18s} {c[k]:6d}")
    bad = [r for r in recs if r["verdict"] in ("DISAGREE", "OWNER_ONLY_DISAGREE")]
    bad.sort(key=lambda r: -r["bl_fanin"])
    print(f"\n{len(bad)} special-member rows disagree; by retail bl fan-in:")
    for r in bad:
        print(f"  0x{r['addr']:08x} fan={r['bl_fanin']:4d} {r['kind']:4s} {r['verdict']:8.8s} {r['sub']:24.24s} {r.get('ours',''):17s} "
              f"{r['extent']:5s} name={r['name_cls']!s:40.40s} retail={r['own_cls'] or r['owner_cls']}")
    if a.json:
        json.dump(recs, open(a.json, "w"), indent=1, default=str)
    return 0


if __name__ == "__main__":
    sys.exit(main())
