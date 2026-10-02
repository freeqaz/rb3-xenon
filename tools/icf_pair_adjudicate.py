#!/usr/bin/env python3
"""Adjudicate ONE (retail survivor, our spelling) alias candidate on retail bytes.

``tools/icf_alias_build.py`` is the batch generator; it only ever adjudicates
pairs some *enumerator* proposed, and its summary cannot distinguish "this pair
was REFUTED" from "this pair was never proposed". When a single charged site is
worth thousands of bytes, that distinction is the whole question, so this tool
takes an explicit pair and prints the T1 decision with every input shown.

It reuses ``icf_alias_build``'s primitives verbatim (``collect``, ``relocs_agree``,
``vacuous``) so a verdict here is the same verdict the generator would reach --
this is a magnifying glass on that adjudicator, NOT a second one.

T1 asks: are the RETAIL bytes at the survivor's address byte-identical, modulo
relocated fields, to what OUR compiler emits for the folded spelling -- AND do
the two agree on relocation TARGETS, not merely on shape? (Masked bytes alone
are vacuous for template twins: ``vector<Foo>::erase`` and ``vector<Bar>::erase``
have identical machine bytes and differ ONLY in the destructor they call.)

It additionally reports UNIQUENESS on both sides, because a body shared by many
functions proves nothing about which one a call site meant -- picking one of an
ICF-folded group is a coin flip.

    python3 tools/icf_pair_adjudicate.py --survivor '?Foo@@...' --ours '?Bar@@...'
    python3 tools/icf_pair_adjudicate.py --selftest      # controls; run this first
"""

import argparse
import collections
import glob
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from icf_alias_build import collect, relocs_agree, vacuous, placeholder  # noqa: E402


def load_sides():
    tgt = collect(sorted(glob.glob(str(ROOT / "build/45410914/obj/**/*.obj"), recursive=True)),
                  "retail target objs")
    ours = collect(sorted(glob.glob(str(ROOT / "build/45410914/src/**/*.obj"), recursive=True)),
                   "our objs")
    return tgt, ours


def body_index(side):
    idx = collections.defaultdict(list)
    for name, (mb, _r, _s) in side.items():
        idx[mb].append(name)
    return idx


def adjudicate(tgt, ours, survivor, our_name, mapped, verbose=True):
    """Return (verdict, detail dict). Verdict in PROVEN / REFUTED / UNDECIDABLE."""
    d = {"survivor": survivor, "ours": our_name}
    rt, ob = tgt.get(survivor), ours.get(our_name)
    if rt is None:
        return "UNDECIDABLE", dict(d, why="survivor absent from the dtk target objs "
                                          "(address outside every pinned .text span)")
    if ob is None:
        return "UNDECIDABLE", dict(d, why="our spelling is in no compiled obj")
    d["retail_size"], d["our_size"] = rt[2], ob[2]
    if vacuous(rt) or vacuous(ob):
        return "UNDECIDABLE", dict(d, why="VACUOUS: body under 4 words or over half "
                                          "the words masked -- compares equal to too much")
    if rt[0] != ob[0]:
        return "REFUTED", dict(d, why="masked bodies DIFFER (retail did not keep the "
                                      "code our spelling compiles to)")
    tally = collections.Counter()
    if not relocs_agree(rt, ob, mapped, strict=True, tally=tally):
        return "REFUTED", dict(d, why="masked bodies match but relocation TARGETS "
                                      "disagree -- template-twin, not a fold",
                               reloc_tally=dict(tally))
    d["reloc_tally"] = dict(tally)
    d["n_relocs"] = len(rt[1])
    return "PROVEN", d


# ★★★ W16-JG.  PLACEHOLDER-SLOT DISCHARGE -- the general path no longer takes a
# placeholder relocation target on trust.
#
# THE HOLE (found by lane W16-JE, docs/decomp/W16JE_NAME_ONLY_RELOC_PAIRS_2026-10-01.md
# §3).  `_slots_agree` on the general path used to `continue` past any slot whose
# retail target is a placeholder (`fn_X`, `lbl_X`, `vftable_X`).  For a
# constructor or destructor that slot is the VTABLE -- the one field that names
# the type -- and for a template wrapper it is the UNNAMED RETAIL CALLEE, i.e.
# the very field that tells `MakeString<int,int,int,int>` from
# `MakeString<const char*,u64,...>`.  JE found 11 bad folds in ~300 of its own
# pairs that way; this lane (W16-JG) audited every landed membership.
#
# Every tolerated slot is now DISCHARGED by the evidence that fits its kind:
#
#   our ??_7X vtable      retail COL at X-4 -> type descriptor must name X
#   our ??_R0 type desc   retail type-descriptor name string must equal ours
#   our ??_C@ literal     decoded literal == retail bytes (length + content)
#   our __real@ constant  encoded value == retail bytes
#   our ?lbl_XXXX data    the address in the name == retail's address
#   our callee C vs fn_X  recursive chase(fn_X, C)
#   other data global     ACCEPTED with a record, but consistency-checked inside
#                         the proof (one name, one address) -- see below.
#
# and returns OK / CONTRADICTED / UNDISCHARGED.  CONTRADICTED needs POSITIVE
# evidence (an RTTI or literal mismatch, an anchored different callee, a
# pigeonhole inside the proof tree); a callee chase that merely FAILS is
# UNDISCHARGED -- our callee may just be an unmatched port of the right
# function, and "not proven" is not "refuted".  Both make the chase return False
# (a PROVEN verdict now requires every slot discharged); the trace says which.
#
# ⚠ WHY data globals keep a (recorded) tolerance.  An unmapped global such as
# `?gChunkAlloc@@3PAVChunkAllocator@@A` (1,352 slots across the landed groups)
# has no retail address to compare against, and no content that names a type.
# Refusing it would mark ~400 sound memberships UNPROVEN for a reason that has
# nothing to do with the fold.  What CAN be checked is that one of our names is
# never paired with two retail addresses, inside the proof (here) and across the
# whole landed file (tools/alias_placeholder_slot_audit.py).
#
# SLOT_POLICY = "lax" restores the pre-W16-JG behaviour (blanket tolerance).  It
# exists ONLY so --self-break-slots can show the new controls go red without it;
# never use it for an admission.
SLOT_POLICY = "discharge"

_ADDR_PH = ("fn_", "lbl_", "vftable_", "data_", "jumptable_", "string_", "unnamed_")
_IMG = []


def retail_image():
    """Lazily-loaded retail PE (band.exe) for the content checks."""
    if not _IMG:
        from thunk_identity import Image
        _IMG.append(Image(str(ROOT / "orig/45410914/band.exe")))
    return _IMG[0]


def _ph_addr(n):
    """Address carried by an address-bearing placeholder name, else None."""
    if not n.startswith(_ADDR_PH):
        return None
    try:
        return int(n.rsplit("_", 1)[1], 16)
    except ValueError:
        return None


def _retail_bytes(va, n):
    img = retail_image()
    o = img.offset(va)
    if o is None or o + n > len(img.data):
        return None
    return img.data[o:o + n]


def _retail_cstr(va, cap=512):
    b = _retail_bytes(va, 1)
    if b is None:
        return None
    img = retail_image()
    o = img.offset(va)
    e = img.data.find(b"\0", o, o + cap)
    return None if e < 0 else img.data[o:e].decode("latin1")


def retail_rtti_name(vt_va):
    """Type-descriptor name of the vtable at `vt_va` via its Complete Object
    Locator (the word at vt-4): COL = {sig, off, cdOff, pTypeDescriptor,
    pClassDescriptor}; TypeDescriptor = {pVFTable, spare, name[]}.  None if the
    word before the vtable is not a COL whose descriptor name reads as RTTI."""
    img = retail_image()
    col = img.word(vt_va - 4)
    if col is None:
        return None
    td = img.word(col + 12)
    if td is None:
        return None
    nm = _retail_cstr(td + 8)
    return nm if nm and nm.startswith(".?A") else None


_VT_OF = {}


def retail_vtables_of(cls):
    """Every retail vtable whose COL names class `cls` (`X@@`), found from the
    type-descriptor STRING outward: name -> TypeDescriptor (name - 8) -> every
    COL whose pTypeDescriptor (COL+12) is it -> every word pointing at that COL,
    +4.  This is how a slot whose retail vtable has NO COL (a /GR- library class,
    e.g. 0x821ad144 in the ??_GCMemoryManagedUnknown group) is still decidable:
    our class's OWN vtable is located, and if it is somewhere else the slot is
    contradicted -- the method lanes W16-HZ / W17-ANON2 / W16-IF applied by hand
    as RTTI_PROVES_OWN_ADDRESS.  Returns a sorted list (empty: not found)."""
    import struct
    if cls in _VT_OF:
        return _VT_OF[cls]
    img = retail_image()
    data = img.data

    def va_of(off):
        for _n, sva, vsz, praw, rsz in img.secs:
            if praw <= off < praw + rsz:
                return 0x82000000 + sva + (off - praw)
        return None

    def find_words(val):
        pat, res, p = struct.pack(">I", val), [], 0
        while True:
            p = data.find(pat, p)
            if p < 0:
                return res
            if p % 4 == 0:
                v = va_of(p)
                if v is not None:
                    res.append(v)
            p += 1

    vts = set()
    names = [cls]
    if cls.startswith("?$") and cls.endswith("@@"):
        names.append(cls[:-2] + "VObjectDir@@@@")      # see _rtti_norm
    for pre, nm in ((p_, n_) for p_ in (b".?AV", b".?AU") for n_ in names):
        pat, p = pre + nm.encode("latin1") + b"\0", 0
        while True:
            p = data.find(pat, p)
            if p < 0:
                break
            s = va_of(p)
            p += 1
            if s is None:
                continue
            for w in find_words(s - 8):            # COL.pTypeDescriptor
                col = w - 12
                for q in find_words(col):          # vtable[-1] == COL
                    vts.add(q + 4)
    _VT_OF[cls] = sorted(vts)
    return _VT_OF[cls]


def retail_bases(vt_va):
    """Type names of every base in the retail Class Hierarchy Descriptor of the
    vtable at `vt_va` (COL+16 -> CHD {sig, attr, numBases, pBaseClassArray};
    each BaseClassDescriptor starts with its pTypeDescriptor).  Includes the
    class itself.  Empty set if unreadable."""
    img = retail_image()
    col = img.word(vt_va - 4)
    chd = img.word(col + 16) if col is not None else None
    if chd is None:
        return set()
    n, arr = img.word(chd + 8), img.word(chd + 12)
    if n is None or arr is None or n > 64:
        return set()
    out = set()
    for i in range(n):
        bcd = img.word(arr + 4 * i)
        td = img.word(bcd) if bcd is not None else None
        nm = _retail_cstr(td + 8) if td is not None else None
        if nm and nm.startswith(".?A"):
            out.add(_rtti_norm(nm[4:]))
    return out


def _class_relation(cls, retail_vts_cls, retail_cls_name=None, retail_vt=None):
    """Decide whether a vtable-slot mismatch can be a REFUTATION.

    ⚠ MEASURED (W16-JG): our headers are DC3-derived, and DC3 inserts classes
    RB3 never had -- `ObjRefOwner` between ObjRef and ObjOwnerPtr.  Our inlined
    ~ObjOwnerPtr then stores ObjRefOwner's vtable where retail stores ObjRef's,
    on a body that IS the right function.  So a mismatch refutes only when our
    class EXISTS in retail (it has a retail vtable) and is UNRELATED to retail's
    class in retail's own hierarchy (neither is a base of the other).  Returns
    None when it may refute, else the UNDISCHARGED kind."""
    if not retail_vts_cls:
        return "VTABLE-OURS-CLASS-NOT-IN-RETAIL"
    ours_bases = set()
    for v in retail_vts_cls:
        ours_bases |= retail_bases(v)
    if retail_cls_name is not None:
        rb = retail_bases(retail_vt) if retail_vt is not None else set()
        if _rtti_norm(cls) in rb or _rtti_norm(retail_cls_name) in ours_bases:
            return "VTABLE-RELATED-CLASS"
    return None


def _rtti_norm(n):
    """Drop a trailing DEFAULT `ObjectDir` template argument.  RB3 retail's
    ObjPtr / ObjOwnerPtr / ObjPtrList carry a second parameter (`ObjectDir`, the
    default), our DC3-derived templates declare one, so retail's type name
    `?$ObjPtr@VFoo@@VObjectDir@@@@` IS our `?$ObjPtr@VFoo@@@@`.  MEASURED: 18 of
    20 nested RTTI "contradictions" in W16-JG's first audit were exactly this."""
    import re
    return re.sub(r"VObjectDir@@(?=@)", "", n)


def _template_head(n):
    """`?$Name@<first arg>` -- for the arity-only-difference test."""
    import re
    m = re.match(r"(\?\$[^@]+@[^@]*@@)", n)
    return m.group(1) if m else None


def _vtable_class(on):
    """`??_7X@@6B@` / `??_7X@@6BBase@@@` -> `X@@` (the complete class)."""
    import re
    m = re.match(r"\?\?_7(.+?@@)6B", on)
    return m.group(1) if m else None


_STR_SPECIAL = ",/\\:. \n\t'-"


def decode_strlit(name):
    """MSVC `??_C@_<w><len><hash>@<chars>@` -> (char_width, total_bytes, prefix).

    The mangling embeds the literal's byte length (NUL included) and up to the
    first 32 bytes; the hash covers the rest.  Returns None when unparseable."""
    import re
    m = re.match(r"\?\?_C@_([01])([0-9]|[A-P]+@)[A-P]*@(.*)@$", name)
    if not m:
        return None
    w = 2 if m.group(1) == "1" else 1
    L = m.group(2)
    n = int(L) + 1 if L.isdigit() else int("".join("%x" % (ord(c) - 65) for c in L[:-1]), 16)
    s, out, i = m.group(3), bytearray(), 0
    while i < len(s):
        c = s[i]
        if c != "?":
            out.append(ord(c))
            i += 1
            continue
        d = s[i + 1] if i + 1 < len(s) else ""
        if d == "$" and i + 3 < len(s):
            out.append((ord(s[i + 2]) - 65) * 16 + (ord(s[i + 3]) - 65))
            i += 4
        elif d.isdigit():
            out.append(ord(_STR_SPECIAL[int(d)]))
            i += 2
        elif "a" <= d <= "z":
            out.append(0xE1 + ord(d) - ord("a"))
            i += 2
        elif "A" <= d <= "Z":
            out.append(0xC1 + ord(d) - ord("A"))
            i += 2
        else:
            return None
    return w, n, bytes(out)


def ours_distinct(ours, a, b, depth=0, seen=None):
    """True iff OUR OWN BUILD shows `a` and `b` are different code: different
    size or masked bytes or relocation shape, or (recursively) a differing pair
    of real relocation targets that is itself distinct.  Uses no alias file --
    deliberately, since the alias file is the thing under audit.  False means
    "would fold in our build"; None means one side is not compiled."""
    if a == b:
        return False
    ra, rb = ours.get(a), ours.get(b)
    if ra is None or rb is None:
        return None
    if ra[2] != rb[2] or ra[0] != rb[0] or len(ra[1]) != len(rb[1]):
        return True
    seen = seen if seen is not None else set()
    if (a, b) in seen or depth > 6:
        return False
    seen.add((a, b))
    for (o1, n1, t1), (o2, n2, t2) in zip(ra[1], rb[1]):
        if o1 != o2 or t1 != t2:
            return True
        if n1 == n2 or placeholder(n1) or placeholder(n2):
            continue
        if ours_distinct(ours, n1, n2, depth + 1, seen):
            return True
    return False


_BODY_IDX = {}


def locate_retail(tgt, ours, on, mapped, exclude=None, cap=64):
    """Retail bodies our function `on` is PROVEN (clean chase: no cycle, no
    undischarged slot) to be, other than `exclude`.  Candidates are only those
    with our masked body AND relocation shape; vacuous bodies are never located
    (a masked 4-word body matches too much to say where anything lives)."""
    ob = ours.get(on)
    if ob is None or vacuous(ob):
        return []
    key = id(tgt)
    if key not in _BODY_IDX:
        idx = collections.defaultdict(list)
        for n, (mb, rl, _s) in tgt.items():
            idx[(mb, tuple((o, t) for (o, _n, t) in rl))].append(n)
        _BODY_IDX.clear()
        _BODY_IDX[key] = idx
    cands = _BODY_IDX[key].get((ob[0], tuple((o, t) for (o, _n, t) in ob[1])), [])
    found = []
    for y in sorted(cands)[:cap]:
        if y == exclude:
            continue
        tr = []
        if chase(tgt, ours, y, on, mapped, out=tr) and not any(
                k == "CYCLE-ASSUMED" or k.startswith("SLOT-UNDISCHARGED")
                for _d, k, _x, _y in tr):
            found.append(y)
    return found


# ★ W16-JH.  CLASSES OUR DC3-DERIVED HEADERS SPELL DIFFERENTLY FROM RETAIL.
# Lane W17-OPTR established that our X360 `ObjRefOwner` IS retail's `ObjRef`
# (ObjPtr<T> / ObjOwnerPtr<T> derive from it directly; DC3 renamed the class).
# So when our ~ObjPtr<T> stores ??_7ObjRefOwner@@6B@ where retail stores the
# vtable whose RTTI reads .?AVObjRef@@, the slot names the SAME class.  W16-JG
# left that slot UNDISCHARGED (VTABLE-OURS-CLASS-NOT-IN-RETAIL), correctly by its
# rules, since nothing in the tool knew the correspondence.
#
# An entry here is NOT taken on trust: _rename_witness() re-derives it on retail
# bytes every time -- (a) our spelling has NO retail RTTI of its own (else it is
# a real, distinct class and a rename would hide a mismatch), and (b) the map
# already names the retail vtable's slot-0 entry (the deleting dtor) as a member
# of OUR class (retail 0x822702A8 = ??_GObjRefOwner@@UAAPAXI@Z).  Measured
# reach: 3 audit memberships (W16-JH).  --self-break-rename accepts ANY retail
# class for a renamed spelling (class check and witness both removed) and the
# RENAME DECOY control must go red.
CLASS_RENAMES = {"ObjRefOwner@@": "ObjRef@@"}
_SELF_BREAK_RENAME = False
_ADDR_NAME = {}


def _addr_name(va):
    if not _ADDR_NAME:
        m = json.load(open(ROOT / "scripts/target_symbol_map.json"))
        _ADDR_NAME.update({int(k, 16): v for k, v in m.items()
                           if k.startswith("0x") and isinstance(v, str)})
        _ADDR_NAME.setdefault(-1, None)
    return _ADDR_NAME.get(va)


def _rename_witness(cls, retail_cls, vt_va):
    if cls not in CLASS_RENAMES:
        return False
    if _SELF_BREAK_RENAME:
        return True        # --self-break-rename: ANY retail class accepted
    if CLASS_RENAMES[cls] != _rtti_norm(retail_cls):
        return False
    if retail_vtables_of(cls):
        return False
    w = retail_image().word(vt_va)
    s0 = _addr_name(w) if w is not None else None
    # the slot-0 entry is a destructor; its mangling carries the class right
    # after the special-member code (??_G/??_E/??1), a method's after `@`
    return bool(s0) and (s0.startswith(("??_G" + cls, "??_E" + cls, "??1" + cls))
                         or ("@" + cls) in s0)


def discharge_slot(tgt, ours, rn, on, mapped, depth, stack, memo, out, maxdepth,
                   ctx, anchor=None):
    """Decide ONE placeholder slot.  Returns (status, kind, detail) with status
    in OK / CONTRADICTED / UNDISCHARGED.  See the W16-JG note above."""
    X = _ph_addr(rn)
    # Our side is the placeholder and retail's is not (or both non-address):
    # literals and constants still compare by CONTENT, because their names are
    # a function of their content.
    if X is None:
        if rn.startswith("??_C@") and on.startswith("??_C@"):
            return "CONTRADICTED", "STRING-NAME-DIFFERS", "two literal manglings differ"
        if rn.startswith("__real@") and on.startswith("__real@"):
            return "CONTRADICTED", "CONSTANT-NAME-DIFFERS", "two constant values differ"
        return "UNDISCHARGED", "NON-ADDRESS-PLACEHOLDER", "%s vs %s" % (rn[:40], on[:40])

    if on.startswith("??_7"):
        cls = _vtable_class(on)
        t = retail_rtti_name(X)
        if cls is None:
            return "UNDISCHARGED", "VTABLE-UNPARSEABLE", on[:60]
        if t is None:
            own = retail_vtables_of(cls)
            if own and X not in own:
                # A COL-less retail vtable could be a /GR- BASE of our class
                # (its CHD still names it).  Refute only when every base of our
                # class has a COL-bearing vtable of its own, i.e. none of them
                # can be the anonymous vtable at X.
                bases = set()
                for v in own:
                    bases |= retail_bases(v)
                bases.discard(_rtti_norm(cls))
                anon = sorted(b for b in bases if not retail_vtables_of(b))
                if anon:
                    return "UNDISCHARGED", "VTABLE-NO-RTTI-BASE-AMBIGUOUS", \
                        "retail %s has no COL; %s has COL-less base(s) %s" % (
                            rn, cls, anon[:3])
                return "CONTRADICTED", "VTABLE-OF-CLASS-ELSEWHERE", \
                    "retail %s has no COL; .?A?%s's own retail vtable(s) are %s" % (
                        rn, cls, ",".join("%x" % v for v in own[:4]))
            return "UNDISCHARGED", "VTABLE-NO-RTTI", "retail %s has no readable COL" % rn
        if _rtti_norm(t[4:]) != _rtti_norm(cls) and _rename_witness(cls, t[4:], X):
            return "OK", "VTABLE-CLASS-RENAMED", "retail RTTI %s is our %s" % (t, cls)
        if _rtti_norm(t[4:]) != _rtti_norm(cls):
            rel = _class_relation(cls, retail_vtables_of(cls), t[4:], X)
            if rel:
                return "UNDISCHARGED", rel, "retail RTTI %s, ours %s" % (t, cls)
        if _rtti_norm(t[4:]) == _rtti_norm(cls):
            return "OK", "VTABLE-RTTI", t
        if cls.startswith("?$") and _template_head(t[4:]) == _template_head(cls):
            # Same template, same first argument, different arity: our template
            # declares a different parameter list than retail's.  Not decidable
            # from the name alone -- NOT a contradiction.
            return "UNDISCHARGED", "VTABLE-TEMPLATE-ARITY", "retail RTTI %s, ours %s" % (t, cls)
        return "CONTRADICTED", "VTABLE-RTTI-DIFFERS", "retail RTTI %s, ours %s" % (t, cls)

    if on.startswith("??_R0"):
        want = "." + on[5:-2] if on.endswith("@8") else None
        got = _retail_cstr(X + 8)
        if want is None or got is None:
            return "UNDISCHARGED", "TYPEDESC-UNREADABLE", rn
        if got == want:
            return "OK", "TYPEDESC-NAME", got
        return "CONTRADICTED", "TYPEDESC-DIFFERS", "retail %s, ours %s" % (got, want)

    if on.startswith("??_C@"):
        d = decode_strlit(on)
        if d is None:
            return "UNDISCHARGED", "STRING-UNPARSEABLE", on[:60]
        w, n, pre = d
        rb = _retail_bytes(X, n)
        if rb is None:
            return "UNDISCHARGED", "STRING-UNREADABLE", rn
        term = rb[-w:] == b"\0" * w
        if rb[:len(pre)] == pre and term:
            return "OK", "STRING-CONTENT", repr(rb[:40])
        return "CONTRADICTED", "STRING-DIFFERS", "retail %r vs ours %r" % (rb[:40], pre[:40])

    if on.startswith("__real@"):
        hx = on[7:]
        if len(hx) not in (8, 16):
            return "UNDISCHARGED", "CONSTANT-UNPARSEABLE", on
        rb = _retail_bytes(X, len(hx) // 2)
        if rb is None:
            return "UNDISCHARGED", "CONSTANT-UNREADABLE", rn
        if rb.hex() == hx.lower():
            return "OK", "CONSTANT-VALUE", hx
        return "CONTRADICTED", "CONSTANT-DIFFERS", "retail %s vs ours %s" % (rb.hex(), hx)

    import re
    m = re.search(r"lbl_([0-9A-Fa-f]{8})", on)
    if m and not on.startswith("lbl_"):
        if int(m.group(1), 16) == X:
            return "OK", "EMBEDDED-ADDRESS", on[:40]
        # NOT a contradiction: a porter named the global after an address that
        # may be TU0-era (every TU0 address is invalid since the TU5 flip), so
        # the address in the NAME is not a claim about where retail keeps it.
        # Measured: ?lbl_82F14008@@3HA vs retail lbl_82C6FB90 (Rnd::UpdateHeap).
        return "UNDISCHARGED", "EMBEDDED-ADDRESS-DIFFERS", "retail %x vs %s" % (X, on[:40])

    if rn.startswith("fn_") or (on in ours and rn in tgt):
        # ONE of our names is ONE COMDAT at ONE retail address, so a proof tree
        # that pairs it with two different retail addresses is impossible --
        # a retail-structure fact, independent of how faithful our code is.
        by_x, by_on = ctx.setdefault("callee_by_x", {}), ctx.setdefault("x_by_callee", {})
        px = by_on.setdefault(on, X)
        if px != X:
            return "CONTRADICTED", "PIGEONHOLE-1-OURS-2-RETAIL", \
                "ours %s is both %x and %x" % (on[:50], px, X)
        prev = by_x.setdefault(X, on)
        if chase(tgt, ours, rn, on, mapped, depth + 1, stack, memo, out, maxdepth, ctx):
            return "OK", "CALLEE-CHASED", on[:60]
        # The callee chase failed.  That alone is NOT a refutation: our callee
        # may be an unfaithful port of the right function.  The refutation that
        # does not lean on our code being faithful: OUR callee's code is PROVEN
        # (clean chase) to be some OTHER retail body Y.  /OPT:ICF folds identical
        # COMDATs, so retail's copy of that code lives at Y, and X is not it.
        ys = locate_retail(tgt, ours, on, mapped, exclude=rn)
        if ys:
            return "CONTRADICTED", "CALLEE-LOCATED-ELSEWHERE", \
                "our %s is PROVEN to be retail %s, not %s" % (on[:50], ",".join(ys[:3]), rn)
        # ⚠ Suspicion classes, NOT refutations -- both trust OUR build's verdict
        # that two of our functions differ, and our build can be an unfaithful
        # port.  MEASURED false positive (W16-JG): retail calls ONE
        # FormatString::operator<< (fn_827C40E8) for int AND const char* args
        # (on PPC both reach _snprintf in the same GPR, so the overloads are
        # identical code and ICF folds them), while OUR divergent FormatString
        # (operator<<(int) 208 B vs retail 124 B) compiles them differently.  A
        # rule keyed on our distinctness would have withdrawn ~40 sound
        # MakeString memberships.
        if prev != on and ours_distinct(ours, prev, on):
            return "UNDISCHARGED", "PIGEONHOLE-1-RETAIL-2-OURS", \
                "retail %s is paired with both %s and %s, distinct in OUR build" % (
                    rn, prev[:50], on[:50])
        if anchor and anchor != on and not placeholder(anchor) \
                and ours_distinct(ours, anchor, on):
            return "UNDISCHARGED", "CALLEE-UNPROVEN-OURS-DISTINCT", \
                "survivor calls %s, folded calls %s: distinct in OUR build" % (
                    anchor[:50], on[:50])
        return "UNDISCHARGED", "CALLEE-UNPROVEN", "chase(%s, %s) failed" % (rn, on[:50])

    # Residual data global: accepted, but one name must mean one address.
    by_d = ctx.setdefault("data_by_name", {})
    px = by_d.setdefault(on, X)
    if px != X:
        return "CONTRADICTED", "DATA-NAME-2-ADDRESSES", "%s is both %x and %x" % (on[:50], px, X)
    return "OK", "DATA-ACCEPTED", on[:60]


# ★ W16-GG.  Set ONLY by --self-break.  When true, chase()'s vacuous branch stops
# accounting for the relocation DESTINATION and admits any vacuous pair whose
# masked bytes and relocation SHAPE agree -- i.e. exactly the permissive failure
# the relaxation must not have.  It exists so the VACUOUS DECOY control can be
# SHOWN to go red on demand: a control nobody has watched fail is an assumption,
# not a control.  (House pattern: grep_binary_guard.py --self-break,
# verify_ruler_agreement.py --selftest, scripts/sabotage_obj_pairing.py.)
_SELF_BREAK = False


def _slots_agree(tgt, ours, rt, ob, survivor, our_name, mapped, depth, stack,
                 memo, out, maxdepth, tolerate_placeholders, ctx=None):
    """Pairwise relocation-slot comparison, recursing on differing real names.

    ★ ONE comparator, TWO callers, and the ONLY difference between them is
    ``tolerate_placeholders``.  That is deliberate: the two call sites used to be
    two hand-written tests (a literal ``list(rt[1]) == list(ob[1])`` in the
    vacuous branch and this loop in the general one), and a reviewer had no way
    to see how they differed except by reading both.  Now the difference is a
    named boolean with a stated reason at each call site.

    ``tolerate_placeholders=True``  -- the general (non-vacuous) path.  Required:
    recursing into placeholder slots instead of tolerating them REGRESSED the
    positive control (see the note at the call site).
    ``tolerate_placeholders=False`` -- the vacuous path.  In a body whose bytes
    are (almost) all masked, a tolerated slot means NOTHING was compared, so a
    placeholder on either side is a refusal.
    """
    rr, orr = rt[1], ob[1]
    if len(rr) != len(orr):
        out.append((depth, "RELOC-COUNT", survivor, our_name))
        return False

    ctx = ctx if ctx is not None else {}
    # ★ W16-JG ANCHOR.  At depth 0 the survivor spelling is usually compiled by
    # us too; when its COMDAT has this pair's relocation shape, its slot i names
    # the callee OUR build places at retail's slot i.  That is what lets a failed
    # callee chase be told apart into "refuted" (the survivor's callee is proven
    # to be fn_X, and the folded spelling's callee is different code) and merely
    # "unproven".
    anc = None
    if depth == 0 and survivor != our_name:
        a = ours.get(survivor)
        if a is not None and len(a[1]) == len(rr) and all(
                x[0] == y[0] and x[2] == y[2] for x, y in zip(a[1], rr)):
            anc = [n for (_o, n, _t) in a[1]]

    stack.append((survivor, our_name))
    ok = True
    for i, ((ro, rn, rty), (oo, on, oty)) in enumerate(zip(rr, orr)):
        if ro != oo or rty != oty:
            out.append((depth, "RELOC-SHAPE", survivor, our_name))
            ok = False
            break
        if rn == on:
            continue
        if rn.startswith(("fn_", "lbl_")) and on in mapped:
            # CD-9: retail spells a callee fn_<B> only when B is absent from the
            # map; our callee being map-resident at A != B means retail's slot
            # demonstrably calls a DIFFERENT function. Not a tolerance.
            out.append((depth, "MAPPED-VS-PLACEHOLDER", rn, on))
            ok = False
            break
        if placeholder(rn) or placeholder(on):
            # ★ CHASE MUST BE A STRICT SUPERSET OF FLAT T1.  Measured: recursing
            # into placeholder slots instead of tolerating them REGRESSED the
            # positive control -- a landed, flat-T1-PROVEN group went REFUTED,
            # because retail's slot reads `fn_8275B378` whose target-obj body is
            # not our callee's.  That is a stricter *different* test, not the
            # relaxation this mode exists to add, and shipping it would have
            # silently re-litigated every landed group under a rule nobody
            # gated.  Recursion is applied ONLY to the branch flat T1 refuses:
            # both sides carry real, differing names.
            if tolerate_placeholders:
                if SLOT_POLICY == "lax":
                    continue
                st, kind, det = discharge_slot(
                    tgt, ours, rn, on, mapped, depth, stack, memo, out, maxdepth,
                    ctx, anchor=anc[i] if anc else None)
                out.append((depth + 1, "SLOT-%s:%s" % (st, kind), rn[:70],
                            ("%s | %s" % (on, det))[:160]))
                if st == "OK":
                    continue
                ok = False
                break
            # ★ W16-GG: the vacuous caller cannot afford this tolerance -- the
            # destination IS the body there.  Refusing keeps the relaxed vacuous
            # branch STRICTER than the general path on this axis.
            out.append((depth, "VACUOUS-PLACEHOLDER-SLOT", rn[:70], on[:70]))
            ok = False
            break
        if _SELF_BREAK and not tolerate_placeholders:
            # --self-break ONLY: drop the destination proof, keep the shape
            # check.  This is "the relaxation with its recursion removed".
            out.append((depth, "SELF-BREAK-TOLERATED", rn[:70], on[:70]))
            continue
        if not chase(tgt, ours, rn, on, mapped, depth + 1, stack, memo, out,
                     maxdepth, ctx):
            out.append((depth, "SLOT-REFUTED", rn[:70], on[:70]))
            ok = False
            break
        out.append((depth + 1, "SLOT-FOLD-OK", rn[:70], on[:70]))
    stack.pop()
    return ok


_BLR = b"\x4e\x80\x00\x20"


# ★ W16-JH.  RETAIL OVER-CARVE: one function, two dtk symbols.
#
# dtk split retail 0x82B9F540..0x82B9F5C8 (our _Deque_iterator_base<T*>::_M_advance,
# a .pdata-less leaf) at the internal branch target 0x82B9F568, emitting
# fn_82B9F540 (40 B, ending in a relocated `b` into fn_82B9F568) + fn_82B9F568.
# The raw contiguous 136 B equal OUR body exactly, and the only branches into
# 0x82B9F568 come from 0x82B9F554 / 0x82B9F55C -- inside fn_82B9F540.  Chase saw
# BYTES-DIFFER (40 B vs 136 B) and left the deque operator+ membership UNDISCHARGED.
#
# retail_overcarve() admits exactly that shape, decided on RAW retail bytes:
#   (1) retail R = fn_X; its LAST relocation is a REL24 branch whose target symbol
#       is the placeholder fn_Y with Y == X + len(R)  (the immediately next symbol);
#   (2) OUR body is longer, its relocations all lie before R's final word, and the
#       raw retail bytes [X, X+len(ours)) equal ours with ONLY our relocated
#       fields masked;
#   (3) CENSUS: every branch in retail .text that lands in (X, X+len(ours)) comes
#       from inside [X, X+len(ours)) -- nothing outside enters the tail, so Y is
#       not a function anyone calls;
#   (4) no .pdata function starts in (X, X+len(ours)).
# The caller then compares R's relocations MINUS the split branch against ours.
# Measured reach: 1 audit membership; 54 retail fn_ bodies carry shape (1).
# --self-break-overcarve drops (3) and the OVERCARVE DECOY must go red.
_SELF_BREAK_OVERCARVE = False
_BR_INDEX = {}


def _retail_text_branches():
    """{target VA: [source VA, ...]} over every relative I-/B-form branch in
    retail .text, plus the set of .pdata BeginAddresses.  Built once."""
    import struct
    if _BR_INDEX:
        return _BR_INDEX["t"], _BR_INDEX["p"]
    img = retail_image()
    tgts, pstarts = collections.defaultdict(list), set()
    for nm, sva, _vsz, praw, rsz in img.secs:
        nm = nm if isinstance(nm, str) else nm.decode("latin1")
        nm = nm.strip("\0")
        if nm == ".pdata":
            d = img.data[praw:praw + rsz]
            pstarts.update(struct.unpack_from(">I", d, i)[0] for i in range(0, len(d) - 7, 8))
        if nm != ".text":
            continue
        base = 0x82000000 + sva
        words = struct.unpack_from(">%dI" % (rsz // 4), img.data, praw)
        for i, w in enumerate(words):
            op = w >> 26
            if op == 18 and not (w & 2):
                d = w & 0x03FFFFFC
                d -= 0x04000000 if d & 0x02000000 else 0
            elif op == 16 and not (w & 2):
                d = w & 0xFFFC
                d -= 0x10000 if d & 0x8000 else 0
            else:
                continue
            src = base + 4 * i
            tgts[src + d].append(src)
    _BR_INDEX.update(t=tgts, p=pstarts)
    return tgts, pstarts


def retail_overcarve(rn, rt, ob):
    """Return R's relocation list minus the split branch when (1)-(4) above
    hold, else None.  See the W16-JH note."""
    X = _ph_addr(rn)
    if X is None or not rn.startswith("fn_") or not rt[1]:
        return None
    n, L = len(rt[0]), len(ob[0])
    o, nm, t = rt[1][-1]
    if not (o == n - 4 and t == 6 and nm.startswith("fn_") and _ph_addr(nm) == X + n
            and L > n):
        return None
    if any(oo + 4 > n - 4 for (oo, _x, _y) in ob[1]):
        return None
    raw = _retail_bytes(X, L)
    if raw is None:
        return None
    raw = bytearray(raw)
    for (oo, _x, _y) in ob[1]:
        raw[oo:oo + 4] = ob[0][oo:oo + 4]          # mask exactly as ours is masked
    if bytes(raw) != ob[0]:
        return None
    tgts, pstarts = _retail_text_branches()
    if any(X < a < X + L for a in pstarts):
        return None
    if not _SELF_BREAK_OVERCARVE:
        for a in range(X + 4, X + L, 4):
            if any(not (X <= src < X + L) for src in tgts.get(a, ())):
                return None
    return rt[1][:-1]


def retail_tail_pad(rt, ob):
    """★ W16-JH.  True iff the retail extent is OUR body followed only by
    alignment padding: retail = ours + k zero words, ours ends in `blr`, and no
    retail relocation lies in (or past) the extra words.

    WHY.  A leaf that touches neither the stack nor LR gets no `.pdata` record,
    so dtk's extent for it runs to the next symbol and bills the inter-function
    alignment word(s) into the retail body.  Measured: retail fn_8274A9D0 is 48 B
    (11 instructions + `0x00000000`) and OUR ??0DataNode@@QAA@ABV0@@Z is those
    11 instructions exactly (44 B) -- a one-sided reader artifact that read as
    BYTES-DIFFER and left 35 placeholder slots UNDISCHARGED.  `0x00000000` after
    an unconditional `blr` is unreachable (it is not even a valid instruction),
    so it is not part of the COMDAT the linker folded.

    STRICTNESS.  The zero words must be RAW zeros, not masked relocation fields
    (masking zeroes relocated fields, which is why ~30% of masked bodies end in a
    zero word) -- hence the no-relocation-in-the-tail test.  Only the RETAIL side
    is trimmed: our COMDATs carry no inter-function padding.  Used only on the
    non-vacuous general path; the vacuous branch and locate_retail stay exact."""
    n = len(ob[0])
    if len(rt[0]) <= n or rt[0][:n] != ob[0] or ob[0][-4:] != _BLR:
        return False
    if any(o + 4 > n for (o, _nm, _t) in rt[1]):
        return False
    tail = rt[0][n:]
    if _SELF_BREAK_TAILPAD:
        # --self-break-tailpad ONLY: drop the zero-tail test (any tail admitted).
        return len(tail) % 4 == 0
    return len(tail) % 4 == 0 and tail == b"\0" * len(tail)


# ★ W16-JH.  Set ONLY by --self-break-tailpad, so the TAIL-PAD DECOY controls
# can be watched going red (house pattern: a control nobody has seen fail is an
# assumption, not a control).
_SELF_BREAK_TAILPAD = False
TAILPAD_POS = (0x8274A9D0, "??0DataNode@@QAA@ABV0@@Z")


def _retail_name_at(va):
    """The retail target-obj spelling at `va`: its map name, else fn_<ADDR>.
    Controls are keyed by ADDRESS so a later map naming cannot strand them
    (measured: W16-JD named 0x8274A9D0 after these controls were written)."""
    return _addr_name(va) or "fn_%08X" % va


OVERCARVE_POS = (0x82B9F540,
                 "?_M_advance@?$_Deque_iterator_base@PAVLyricPlate@@@stlpmtx_std@@QAAXH@Z")


def overcarve_controls(tgt, ours):
    """★ W16-JH controls for retail_overcarve.  POSITIVE: fn_82B9F540 (dtk's
    first half of a split leaf) vs our _M_advance<LyricPlate*>: expect PROVEN.
    DECOY: the IDENTICAL pair, evaluated while the cached branch census carries
    ONE extra branch from outside the extent into the tail (what a real
    tail-call target looks like): expect REFUTED.  Injected by a setup/teardown
    hook around that one evaluation, so it isolates clause (3) exactly."""
    X, o = OVERCARVE_POS
    s = "fn_%08X" % X
    if s not in tgt:
        # ★ W16-JC (main 17aaa4298) FIXED this carve at its root -- symbols.txt now
        # gives 0x82B9F540 size 0x88 and fn_82B9F568 is gone -- which is the
        # independent confirmation of this rule's diagnosis, and leaves the rule
        # with no live split in the tree.  The positive is therefore rebuilt from
        # the REAL retail bytes with ONLY dtk's old split simulated: the first
        # 0x28 bytes, ending in the relocated `b` into +0x28.  Raw image and branch
        # census are untouched, so every clause still runs on retail data.
        full = tgt.get(_retail_name_at(X))
        if full is not None and len(full[0]) > 0x28:
            tgt[s] = (full[0][:0x24] + b"\0\0\0\0",
                      [r for r in full[1] if r[0] < 0x24] + [(0x24, "fn_%08X" % (X + 0x28), 6)],
                      0x28)
    if s not in tgt or o not in ours or retail_overcarve(s, tgt[s], ours[o]) is None:
        raise SystemExit("REFUSING: over-carve positive %s/%s absent or no longer an "
                         "over-carve -- the controls would be VACUOUS." % (s, o))
    tail = _ph_addr(tgt[s][1][-1][1])
    fake_src = 0x82000000                   # a branch source far outside the extent

    def setup():
        _retail_text_branches()[0][tail].append(fake_src)

    def teardown():
        _retail_text_branches()[0][tail].remove(fake_src)

    dl = "OVERCARVE DECOY, tail entered from outside (expect REFUTED)"
    _PAIR_HOOKS[dl] = (setup, teardown)
    return [("OVERCARVE POSITIVE, one leaf split by dtk (expect PROVEN)", s, o),
            (dl, s, o)]


_PAIR_HOOKS = {}


RENAME_POS = (0x826FCCE8, "??1?$ObjPtr@VSynthSample@@@@UAA@XZ")


def rename_controls(tgt, ours, mapped):
    """★ W16-JH controls for CLASS_RENAMES.  POSITIVE: retail fn_826FCCE8 (a
    ~ObjPtr<T> body storing retail's ObjRef vtable lbl_820009EC) vs OUR
    ~ObjPtr<SynthSample> (storing ??_7ObjRefOwner@@6B@): expect PROVEN.  DECOY:
    the same retail body with ONLY that vtable slot re-pointed at a vtable of a
    different, unrelated retail class (the body's own ObjPtr<SynthSample> vtable
    lbl_820F5284) -- the rename must not blanket-accept ObjRefOwner: expect
    REFUTED.  Refuses if the positive does not carry the ObjRef slot."""
    s, o = _retail_name_at(RENAME_POS[0]), RENAME_POS[1]
    rt = tgt.get(s)
    if rt is None or o not in ours or not any(n == "lbl_820009EC" for _o, n, _t in rt[1]):
        raise SystemExit("REFUSING: rename positive %s/%s absent or no longer stores "
                         "retail ObjRef's vtable -- the rename controls would be "
                         "VACUOUS." % (s, o))
    tgt["__chasetest_rename_decoy__"] = (
        rt[0], [(x, "lbl_820F5284" if n == "lbl_820009EC" else n, t)
                for x, n, t in rt[1]], rt[2])
    return [("RENAME POSITIVE, retail ObjRef vtable vs our ObjRefOwner (expect PROVEN)",
             s, o),
            # labelled as BOTH: it is a vtable placeholder slot, so the lax slot
            # rule (--self-break-slots) must turn it red as well
            ("RENAME DECOY / SLOT DECOY, ObjRefOwner vs an unrelated retail vtable "
             "(expect REFUTED)",
             "__chasetest_rename_decoy__", o)]


def tailpad_controls(tgt, ours):
    """★ W16-JH controls for retail_tail_pad.  POSITIVE: retail fn_8274A9D0 (48 B
    = DataNode's 11-instruction copy ctor + one alignment word) vs OUR copy ctor.
    DECOY is that same retail body with ONLY the tail word altered (a `nop`,
    0x60000000), injected under a reserved name, so it isolates the zero-tail
    clause.
    Refuses if the positive pair is absent (the decoy would then test nothing)."""
    s, o = _retail_name_at(TAILPAD_POS[0]), TAILPAD_POS[1]
    if s not in tgt or o not in ours or not retail_tail_pad(tgt[s], ours[o]):
        raise SystemExit("REFUSING: tail-pad positive %s/%s is absent or no longer "
                         "a pad-only pair -- the tail-pad controls would be VACUOUS."
                         % (s, o))
    mb, rl, sz = tgt[s]
    n = len(ours[o][0])
    tgt["__chasetest_tailpad_nop__"] = (mb[:n] + b"\x60\x00\x00\x00", rl, sz)
    # ⚠ No decoy for the "relocation in the tail" clause, DELIBERATELY: it is
    # implied by _slots_agree (a retail relocation at/past len(ours) can never
    # pair with one of ours, so RELOC-COUNT / RELOC-SHAPE refuses it anyway).
    # MEASURED: such a decoy stayed REFUTED under --self-break-tailpad, i.e. it
    # cannot be made to fail by breaking this rule -- a control that cannot fail
    # is not a control.  The clause stays as defence in depth.
    return [("TAIL-PAD POSITIVE, retail = ours + alignment word (expect PROVEN)", s, o),
            ("TAIL-PAD DECOY, nonzero tail word (expect REFUTED)",
             "__chasetest_tailpad_nop__", o)]


# ★★ W16-NN.  THE SIZE GATE -- one like-for-like comparison, no tolerance.
#
# THE DEFECT (lane W16-NK, docs/decomp/W16NK_GAME_ANON_ROWS_2026-10-02.md §4).
# Identification lanes W16-NF and W16-NK refused a naming proposal when
# |retail size - our size| > 8, with "our size" read by
# anon_candidate_scorer.coff_function_sizes -- the COMDAT SECTION length.  An
# EH-bearing COMDAT is [8 B prefix][body][8 B prefix][__unwind$ funclet], so
# `vector<ObjPtr<GemTrackDir>>::_M_fill_insert_aux` read "392 vs ours 440" and
# was refused, then paired at 100 the moment it was named.  This tool's own T1
# was never affected: `collect` reads coff_bodies_ext EXTENTS (392 == 392).
#
# MEASURED (main 4f29d4495, 21,383 names both sides define, non-vacuous): the
# legacy rule refused 5,843 of 21,084 equal-extent pairs (27.7%; 5,692 of them
# masked-byte-identical) AND ADMITTED 145 of 322 pairs whose retail body is
# genuinely larger.  It was wrong in both directions, so the fix is NOT a wider
# tolerance -- the tolerance was compensating for the unlike measure.
#
# THE RULE.  Retail = the target-obj extent (dtk carves by .pdata; == report.json
# row size).  Ours = the coff_bodies_ext extent (99.966% equal to retail's row
# size over the 26,399 named rows report.json scores at fuzzy 100).  Accept iff
# equal, or retail = ours + zero alignment words (retail_tail_pad, the one
# retail-side reader artifact already proven).  Anything else is refused and
# the refusal names the DIRECTION.  Exact equality is right because T1 needs it
# anyway (masked bodies of different length cannot be equal) -- this gate exists
# for consumers that score a proposal by fuzzy rather than T1.
#
# _SELF_BREAK_SIZE re-introduces one wrong rule at a time so --chasetest's SIZE
# controls can be watched going red (see size_controls):
#   "eh"       ours = COMDAT section length (the W16-NF/NK measure)
#   "tol"      symmetric tolerance of SIZE_BREAK_TOL bytes (widen-to-cover-funclets)
#   "onesided" ours-larger always admitted ("ours may carry a funclet")
#   "firstdef" ours = collect()'s first definition only (see VARIANTS below)
#
# ★ VARIANTS (found by this lane's own re-run: two rows the grader scores at
# fuzzy 100 read "retail LARGER").  One COMDAT name can be compiled to DIFFERENT
# bodies in different TUs -- ?resize@?$ObjVector@V?$ObjPtr@VRndDir@@@@@@ is 72 B
# in Gem.obj / ExternalMic.obj and 84 B in OutfitConfig.obj / BandStarDisplay.obj;
# retail kept the 84-B one.  collect() keeps the FIRST definition in sorted path
# order (band3/ sorts first), i.e. whichever the linker may have discarded.  So
# "ours" is a SET: 311 of our 96,954 names have more than one extent, and 41 of
# 21,720 same-name pairs match retail only through a non-first variant.  The
# gate accepts iff retail's extent equals SOME compiled variant, and says which.
_SELF_BREAK_SIZE = None
SIZE_BREAK_TOL = 64
_SECTION_SIZES = {}
_EXT_VARIANTS = collections.defaultdict(dict)   # name -> {extent: first obj}


def _our_section_size(name):
    if not _SECTION_SIZES:
        from anon_candidate_scorer import comdat_section_sizes
        from coff_bodies_ext import function_extent_sizes
        for p in sorted(glob.glob(str(ROOT / "build/45410914/src/**/*.obj"), recursive=True)):
            for n, s in comdat_section_sizes(Path(p)).items():
                _SECTION_SIZES.setdefault(n, s)
            rel = str(Path(p).relative_to(ROOT / "build/45410914/src"))
            for n, s in function_extent_sizes(p).items():
                _EXT_VARIANTS[n].setdefault(s, rel)
    return _SECTION_SIZES.get(name)


def our_extent_variants(name):
    """{extent: first obj defining that body} over every obj that defines `name`."""
    _our_section_size("")
    return _EXT_VARIANTS.get(name, {})


def size_gate(rt, ob, our_name=None):
    """(ok, why) for retail record `rt` vs our record `ob` (collect() records,
    i.e. like-for-like EXTENTS).  See the W16-NN note above."""
    rs, os_ = rt[2], ob[2]
    if _SELF_BREAK_SIZE == "eh" and our_name:
        os_ = _our_section_size(our_name) or os_
    if _SELF_BREAK_SIZE == "tol" and abs(rs - os_) <= SIZE_BREAK_TOL:
        return True, "within +-%d B (BROKEN tolerance rule)" % SIZE_BREAK_TOL
    if rs == os_:
        return True, "extents equal: retail %d == ours %d" % (rs, os_)
    if our_name and _SELF_BREAK_SIZE not in ("eh", "firstdef"):
        v = our_extent_variants(our_name)
        if rs in v:
            return True, ("retail %d == our %d-B variant in %s (collect's first "
                          "definition is %d B; our TUs compile %d bodies for this name)"
                          % (rs, rs, v[rs], os_, len(v)))
    if rs > os_:
        if os_ == ob[2] and retail_tail_pad(rt, ob):
            return True, ("retail %d = ours %d + %d B zero alignment padding"
                          % (rs, os_, rs - os_))
        return False, ("retail LARGER: retail %d vs ours %d (+%d B the port does not "
                       "emit)" % (rs, os_, rs - os_))
    if _SELF_BREAK_SIZE == "onesided":
        return True, "ours larger, admitted (BROKEN one-sided rule)"
    return False, ("ours LARGER: retail %d vs ours %d (+%d B over like-for-like "
                   "extents; EH prefixes and funclets are already excluded)"
                   % (rs, os_, os_ - rs))


SIZE_EH_POS = 0x82307EB0      # W16-NK §4: retail 392, our COMDAT section 440
SIZE_VAR_POS = 0x822A7A70     # PropSync<Piece>: retail 392; Gem.obj 360, OutfitConfig.obj 392
SIZE_RL_POS = (0x82697FE8,    # W16-NK §4: retail 132 vs our 116
               "??$__unguarded_partition@PAPAUObjEntry@@PAU1@UObjSort@@@stlpmtx_std@@"
               "YAPAPAUObjEntry@@PAPAU1@0PAU1@UObjSort@@@Z")


def size_controls(tgt, ours):
    """★ W16-NN controls for size_gate, one per direction, picked from the LIVE
    tree (the documented W16-NK rows are preferred when still valid, so a map
    or source repair cannot strand the control):

      EH-FUNCLET POSITIVE  same function both sides (masked bodies equal), our
                           COMDAT section >= extent + 16 -> expect ACCEPT.
                           Red under --self-break-size eh.
      RETAIL-LARGER DECOY  retail extent > ours by 4 B, not tail padding, our
                           section == extent (no EH confound) -> expect REFUSE.
                           Red under --self-break-size tol.
      RETAIL-LARGER W16-NK the documented 132-vs-116 pair, if both sides still
                           define it -> expect REFUSE.  Red under tol.
      OURS-LARGER DECOY    ours extent > retail by 4 B, our section == extent
                           (so not explained by any funclet) -> expect REFUSE.
                           Red under tol and onesided.
      VARIANT POSITIVE     retail extent != collect()'s first definition but
                           == another TU's body for the same name -> expect
                           ACCEPT.  Red under firstdef, and under eh (which
                           measures the first definition's SECTION and skips
                           the variant rule; the pick requires that section !=
                           retail so the red is guaranteed, not coincidental).

    Every refusal decoy is a name with ONE extent variant, so the variant rule
    can never be what accepts or refuses it.

    Returns [(label, retail_name, our_name, want_ok, red_under)].  Refuses (exit)
    if any control cannot be built or is VACUOUS -- an EH positive whose section
    length already equals retail's could not detect the defect it exists for."""
    _our_section_size("")  # build the index once
    cands = sorted(n for n in tgt if n in ours and not n.startswith(("fn_", "lbl_"))
                   and not vacuous(tgt[n]) and not vacuous(ours[n]))
    out = []
    eh = _retail_name_at(SIZE_EH_POS)

    def eh_ok(r, o):
        rt, ob = tgt.get(r), ours.get(o)
        sec = _SECTION_SIZES.get(o)
        return (rt is not None and ob is not None and sec is not None
                and rt[2] == ob[2] and rt[0] == ob[0] and sec >= ob[2] + 16)
    if not eh_ok(eh, eh):
        eh = next((n for n in cands if eh_ok(n, n)), None)
    if eh is None:
        raise SystemExit("REFUSING: no EH-funclet size positive in the live tree -- "
                         "the EH control would be VACUOUS.")
    out.append(("SIZE EH-FUNCLET POSITIVE, retail %d / our section %d (expect ACCEPT)"
                % (tgt[eh][2], _SECTION_SIZES[eh]), eh, eh, True, {"eh"}))

    def clean(n):
        return _SECTION_SIZES.get(n) == ours[n][2] and len(our_extent_variants(n)) == 1
    rl = next((n for n in cands if tgt[n][2] - ours[n][2] == 4 and clean(n)
               and not retail_tail_pad(tgt[n], ours[n])), None)
    ol = next((n for n in cands if ours[n][2] - tgt[n][2] == 4 and clean(n)), None)
    if rl is None or ol is None:
        raise SystemExit("REFUSING: no live retail-larger / ours-larger size decoy -- "
                         "the size refusal controls would be VACUOUS.")
    out.append(("SIZE RETAIL-LARGER DECOY, +4 B (expect REFUSE)", rl, rl, False,
                {"tol"}))
    s, o = _retail_name_at(SIZE_RL_POS[0]), SIZE_RL_POS[1]
    if s in tgt and o in ours and tgt[s][2] > ours[o][2]:
        out.append(("SIZE RETAIL-LARGER W16-NK, %d vs %d (expect REFUSE)"
                    % (tgt[s][2], ours[o][2]), s, o, False, {"tol"}))
    out.append(("SIZE OURS-LARGER DECOY, +4 B, no funclet (expect REFUSE)", ol, ol,
                False, {"tol", "onesided"}))

    def var_ok(n):
        return (n in tgt and n in ours and tgt[n][2] != ours[n][2]
                and tgt[n][2] in our_extent_variants(n)
                and not retail_tail_pad(tgt[n], ours[n])
                and _SECTION_SIZES.get(n) != tgt[n][2])
    vp = _retail_name_at(SIZE_VAR_POS)
    if not var_ok(vp):
        vp = next((n for n in cands if var_ok(n)), None)
    if vp is None:
        raise SystemExit("REFUSING: no live TU-variant size positive -- the variant "
                         "control would be VACUOUS.")
    out.append(("SIZE VARIANT POSITIVE, retail %d / first def %d (expect ACCEPT)"
                % (tgt[vp][2], ours[vp][2]), vp, vp, True, {"firstdef", "eh"}))
    return out


def chase(tgt, ours, survivor, our_name, mapped, depth=0, stack=None, memo=None,
          out=None, maxdepth=12, ctx=None):
    """RECURSIVE T1: verify a fold through relocation-target EQUIVALENCE.

    WHY the flat T1 tier cannot decide this class.  ``relocs_agree`` compares
    relocation target NAMES literally, so it refutes any fold whose callees are
    THEMSELVES folded -- and MSVC's /OPT:ICF is iterative, so that is the normal
    case for template families.  Measured on the CustomizePanel row: retail's
    ``hash_map<int,T*>::operator[]`` survivor calls the ``_M_find`` survivor
    named for SongMetadata and the ``_M_insert`` survivor named for SongStatus,
    i.e. THREE different T in one function.  A literal-name comparator calls that
    a template-twin refutation; it is in fact the fold signature.

    ★ THE SAFETY PROPERTY: this NEVER SEARCHES.  It is handed a specific pair and
    walks the pair chain RETAIL'S OWN RELOCATIONS dictate -- slot i of retail is
    checked against slot i of ours, and nothing else is ever considered.  There
    is no "find the best candidate" step, so there is no coin flip to lose.  The
    flat T1 base test (masked bytes equal, reloc offsets/types equal, non-vacuous)
    must hold at EVERY level; recursion only relaxes the NAME equality.

    Cycles are accepted coinductively (a pair already on the stack is assumed) --
    that is what a linker does with mutually recursive COMDATs -- and every such
    assumption is reported so it can be audited rather than trusted silently.
    """
    stack = stack if stack is not None else []
    memo = memo if memo is not None else {}
    out = out if out is not None else []
    ctx = ctx if ctx is not None else {}
    key = (survivor, our_name)
    if key in memo:
        return memo[key]
    if key in stack:
        out.append((depth, "CYCLE-ASSUMED", survivor, our_name))
        return True
    if survivor == our_name and depth > 0:
        # A same-name SLOT (retail's reloc names S and so does ours) is exactly
        # what flat T1's relocs_agree accepts: name equality IS the evidence.
        return True
    # ★ W16-AE: NOT at depth 0.  A self-pair [S, S] handed in at the top used to
    # short-circuit here and read PROVEN without a single byte compared -- a
    # vacuous verdict shaped like the strongest one.  Measured: six [S,S] pairs
    # whose OUR-side COMDAT contradicts the retail body at S's mapped address
    # (0x8231a578 ??1Automator 224 B retail vs a different ~Automator of ours,
    # 0x822c5600 operator>><BAMPhrase> ...) all chased PROVEN.  At depth 0 the
    # question being asked is "is our COMDAT for S the retail body named S?",
    # and that is answered below by the same byte/reloc tests as any pair.
    if depth > maxdepth:
        out.append((depth, "DEPTH-CAP", survivor, our_name))
        return False

    rt, ob = tgt.get(survivor), ours.get(our_name)
    if rt is None or ob is None:
        # Retail-side placeholders carry no address information; our side being
        # absent means we never compile that spelling. Neither is evidence FOR a
        # fold, so both are refusals, not tolerances.
        out.append((depth, "MISSING(%s)" % ("retail" if rt is None else "ours"),
                    survivor, our_name))
        return False
    if vacuous(rt) or vacuous(ob):
        # ★ W10-B.  The vacuity guard exists because a MASKED comparison of a
        # tiny body is non-discriminating: a 4-byte `b X` compares equal to
        # every other 4-byte `b Y` once the displacement is masked out.  That
        # reasoning is about what MASKING HIDES, and it simply does not apply
        # when nothing is hidden -- if the bodies are byte-identical AND every
        # relocation agrees on offset, type AND TARGET NAME, then the two
        # COMDATs satisfy /OPT:ICF's own folding condition exactly and there is
        # no masked field left for a coincidence to occupy.  Refusing here is
        # not conservative, it is vacuous in the other direction.
        #
        # MEASURED: this is what blocked `list<char*>::insert` for two lanes
        # (W8-D, W9-B).  The chase walks cleanly through BOTH allocator levels
        # -- their bodies match -- and dies on the 8-byte `operator new` thunk
        # ??2CriticalSection@@SAPAXI@Z / ??2ChunkAllocator@@SAPAXI@Z, whose two
        # sides are identical in every byte and whose single relocation names
        # `?MemAlloc@@YAPAXHH@Z` on BOTH sides.  W9-B read the resulting chain
        # of SLOT-REFUTED frames -- which are only the leaf failure propagating
        # back up the recursion -- as an "allocator debug-overload" blocker.  It
        # never was one.
        #
        # The 8-byte class is exactly what tools/alloc_fold_gate.py was built to
        # adjudicate ("Eight bytes is BETTER evidence than four, not worse"), and
        # its --shape-census measured the discrimination: 712 retail 8-byte
        # <word>+<branch> bodies carry 707 distinct (word, destination) combos,
        # so the destination does all the work.  Here both destinations are the
        # same NAME, which is the strongest agreement available, not a blur.
        #
        # STRICTNESS: full equality is required, relocation target names
        # INCLUDED.  A vacuous pair whose reloc names differ still REFUSES, so
        # this cannot admit a template twin -- see --chasetest's in-family decoy.
        #
        # ★★★ W16-GG: A DESTINATION MAY BE ACCOUNTED FOR BY A PROOF, NOT ONLY BY
        # LITERAL NAME EQUALITY.  The invariant this branch has always maintained
        # is *every masked field is accounted for*.  For a 4-byte tail-call thunk
        # (`b <target>`; masked body 0x00000000 because the whole instruction IS
        # the relocated field) the destination is the entire information content,
        # so it must be pinned exactly.  Literal name equality pins it -- and so
        # does a RECURSIVELY PROVEN FOLD of the two destinations, because
        # /OPT:ICF is a FIXED POINT: if retail folded X and Y then `b X` and
        # `b Y` resolve to the SAME address, and the two thunks satisfy the
        # folding condition themselves.  Refusing that is not strictness, it is a
        # gap: it refuses a fold BECAUSE the linker folded iteratively.
        #
        # MEASURED (lane W16-GD, refused by GD on purpose because GD would have
        # collected the payout; relaxed by W16-GG, which does not install it):
        # ?CopyTypeProperties@@YAXPAVObject@Hmx@@0@Z, 1,472 B, 4 charges, ALL the
        # same pair -- retail ??$__destroy_aux@UEntry@LocalePanel@@ vs our
        # ??1?$list@VSymbol@@, each 4 B / masked 0x00000000 / one type-6 reloc at
        # offset 0, destinations ?clear@?$_List_base@PAVSynthPollable@@ and
        # ?clear@?$_List_base@VSymbol@@ -- a pair that is FLAT-T1 PROVEN (88 B
        # both sides, retail_bodytwins 1).  The destination fold was proven and
        # the thunk tail-calling it was refused.  Same shape the `list<char*>`
        # note above records, with one difference: there the thunk named the SAME
        # symbol on both sides, so literal equality admitted it.
        #
        # ⚠ STRICTNESS IS *RAISED* ON THE OTHER AXIS, DELIBERATELY.  The general
        # path tolerates a placeholder target; this branch must NOT, because in a
        # vacuous body a tolerated slot means nothing was compared at all.
        # Measured scope of that hazard on this tree: retail vacuous single-reloc
        # thunks against ours of the same masked body and relocation shape form
        # 2,289,106 shape-compatible cross-pairs.  The destination does 100% of
        # the discriminating -- tolerate it and the comparator admits two
        # million pairs.  Hence tolerate_placeholders=False.
        #
        # NET: strictly WIDER than its old self (literal equality still
        # short-circuits below, with no recursion at all, so every previously
        # admitted pair is admitted unchanged) and strictly NARROWER than the
        # general path.  --chasetest's VACUOUS DECOY proves the widening did not
        # dissolve the discriminator; --self-break proves that decoy can go red.
        if rt[0] == ob[0]:
            if list(rt[1]) == list(ob[1]):
                out.append((depth, "VACUOUS-BUT-IDENTICAL", survivor, our_name))
                return True
            if _slots_agree(tgt, ours, rt, ob, survivor, our_name, mapped, depth,
                            stack, memo, out, maxdepth,
                            tolerate_placeholders=False, ctx=ctx):
                out.append((depth, "VACUOUS-DESTINATION-FOLD-PROVEN",
                            survivor, our_name))
                return True
        out.append((depth, "VACUOUS", survivor, our_name))
        return False
    if rt[0] != ob[0]:
        oc = None if retail_tail_pad(rt, ob) else retail_overcarve(survivor, rt, ob)
        if oc is not None:
            out.append((depth, "RETAIL-OVERCARVE", survivor, our_name))
            rt = (ob[0], oc, len(ob[0]))
        elif not retail_tail_pad(rt, ob):
            out.append((depth, "BYTES-DIFFER", survivor, our_name))
            return False
        else:
            out.append((depth, "RETAIL-TAIL-PAD", survivor, our_name))
    # The general path KEEPS its placeholder tolerance -- see the note inside
    # _slots_agree; removing it regressed a landed positive control.
    ok = _slots_agree(tgt, ours, rt, ob, survivor, our_name, mapped, depth,
                      stack, memo, out, maxdepth, tolerate_placeholders=True,
                      ctx=ctx)
    memo[key] = ok
    return ok


def family(tgt, ours, our_name, survivor):
    """PIGEONHOLE evidence: N of our spellings collapse to how many retail addresses?

    WHY this and not --chase for a template family.  --chase verifies our whole
    call subtree is byte-identical to retail's, which is STRICTLY STRONGER than
    the question that matters and fails for an irrelevant reason: measured on the
    hash_map row, the chain breaks three levels down because OUR
    ``_Stl_prime<bool>::_S_next_size`` (92 B) differs from what retail's
    ``resize`` calls (96 B).  That is OUR divergence, and it is present in BOTH
    instantiations equally, so it cannot bear on whether RETAIL folded them.

    The retail-internal question is a count.  Partition our spellings and retail's
    addresses by (body, body-of-slot-0-callee) -- a discriminator, not a blur: it
    correctly separates the int-keyed hash_map family from the Symbol-keyed one
    whose ``_M_find`` has different bytes.  If N of ours map onto 1 retail
    address, retail folded them, and the survivor's map name is one arbitrary
    member's spelling.

    ⚠ SCOPE: the retail side counts only PINNED target objs, so an unpinned
    instantiation is invisible.  That can only ADD retail copies, and the address
    at issue is read off retail's own relocation rather than searched for, so the
    bound does not weaken the conclusion.
    """
    import hashlib

    def slot0(side, name):
        rec = side.get(name)
        if not rec or not rec[1]:
            return ("NOREC",)
        callee = side.get(rec[1][0][1])
        return ("BODY", hashlib.sha1(callee[0]).hexdigest()) if callee \
            else ("EXT", rec[1][0][1])

    mb = ours[our_name][0]
    ref = slot0(tgt, survivor)
    ours_fam = sorted(n for n, (m, _r, _s) in ours.items()
                      if m == mb and slot0(ours, n) == ref)
    ret_fam = sorted(n for n, (m, _r, _s) in tgt.items()
                     if m == mb and slot0(tgt, n) == ref)
    ret_all = [n for n, (m, _r, _s) in tgt.items() if m == mb]
    return {"our_family": ours_fam, "retail_family": ret_fam,
            "retail_bodytwins_all": ret_all,
            "excluded_by_slot0": [n for n in ret_all if n not in ret_fam],
            "our_slot0_matches_retail": slot0(ours, our_name) == ref}


def uniqueness(tgt, ours, survivor, our_name):
    ti, oi = body_index(tgt), body_index(ours)
    out = {}
    if survivor in tgt:
        peers = ti[tgt[survivor][0]]
        out["retail_bodytwins"] = len(peers)
        out["retail_bodytwin_names"] = sorted(peers)[:8]
    if our_name in ours:
        peers = oi[ours[our_name][0]]
        out["our_bodytwins"] = len(peers)
        out["our_bodytwin_names"] = sorted(peers)[:8]
    return out


def load_mapped():
    m = json.load(open(ROOT / "scripts/target_symbol_map.json"))
    out = set()
    for _a, n in m.items():
        for x in (n if isinstance(n, list) else [n]):
            if x:
                out.add(x)
    return out


def vacuous_pair(tgt, ours, want_fold):
    """Pick from the LIVE tree a VACUOUS thunk pair for --chasetest.

    Shape, identical for both controls: masked bodies EQUAL, exactly one
    relocation each at the SAME offset and type, both destination names real
    (non-placeholder) and DIFFERENT, both destinations resolvable on their own
    side.  This is precisely the input class W16-GG's relaxation widened, so the
    two controls differ ONLY in whether the destinations are a fold:

    want_fold=True  -- destination bodies byte-identical AND their own
                       relocations literally agree (flat T1), so the fold is
                       proven by the strictest tier available.  EXPECT PROVEN:
                       without this the relaxation would be inert.
    want_fold=False -- ★ THE DECOY.  Destination bodies DIFFER while being the
                       SAME SIZE, so the destinations are demonstrably not one
                       folded COMDAT and a size test cannot stand in for the byte
                       test.  EXPECT REFUTED: this is the direction the
                       relaxation widened, and a widening nobody probed in its
                       own direction is untested.

    Chosen from the live tree in sorted order rather than hardcoded, so the
    control survives map repairs and rebuilds (same reason as the W16-AE
    self-pair controls).  Returns (survivor, ours) or raises.
    """
    import collections
    oidx = collections.defaultdict(list)
    for n in ours:
        mb, rl, _sz = ours[n]
        if len(rl) == 1 and vacuous(ours[n]) and not placeholder(n):
            oidx[(mb, rl[0][0], rl[0][2])].append(n)
    for k in oidx:
        oidx[k].sort()

    for rn_ in sorted(tgt):
        if placeholder(rn_):
            continue
        rmb, rrl, _ = tgt[rn_]
        if len(rrl) != 1 or not vacuous(tgt[rn_]):
            continue
        rdst = rrl[0][1]
        if placeholder(rdst) or rdst not in tgt:
            continue
        rdrec = tgt[rdst]
        for on_ in oidx.get((rmb, rrl[0][0], rrl[0][2]), ()):
            odst = ours[on_][1][0][1]
            if odst == rdst or placeholder(odst) or odst not in ours:
                continue
            odrec = ours[odst]
            if want_fold:
                if odrec[0] == rdrec[0] and list(odrec[1]) == list(rdrec[1]):
                    return rn_, on_
            else:
                if odrec[0] != rdrec[0] and odrec[2] == rdrec[2]:
                    return rn_, on_
    raise SystemExit("REFUSING: no live %s vacuous-thunk pair found -- the "
                     "control would be VACUOUS, which is worse than absent."
                     % ("fold" if want_fold else "decoy"))


# W16-JE's documented bad pair: retail ~ObjPtr<SeqInst> stores a vtable whose
# RTTI names ObjPtr<SeqInst,ObjectDir>; ours stores ObjPtr<Sequence>'s.  The old
# general path read it PROVEN because the vtable slot is an unnamed `lbl_`.
JE_SLOT_DECOY = ("??1?$ObjPtr@VSeqInst@@@@UAA@XZ", "??1?$ObjPtr@VSequence@@@@UAA@XZ")


def slot_controls(tgt, ours, mapped, al):
    """★ W16-JG controls for the placeholder-slot discharge.

    DECOYS (expect REFUTED): JE's pair, plus one membership per withdrawal class
    that W16-JG recorded in the ledger (`PLACEHOLDER_SLOT_*`), read back from
    `withdrawn` records so they survive later map work.  A decoy is only USED if
    the old lax rule PROVES it -- a decoy the old rule already refuses does not
    probe the hole, and a control that cannot fail is worse than none.
    POSITIVES (expect PROVEN): a live membership whose proof DISCHARGES a vtable
    slot by RTTI, and one that discharges a callee slot by chase, so the fix
    cannot be "refuse every placeholder slot".  Refuses if any is missing."""
    global SLOT_POLICY
    keep = SLOT_POLICY

    def verdict(s, o, pol):
        global SLOT_POLICY
        SLOT_POLICY = pol
        tr = []
        ok = chase(tgt, ours, s, o, mapped, out=tr)
        SLOT_POLICY = keep
        return ok, [k for _d, k, _x, _y in tr]

    out = []
    cands = [("W16-JE vtable pair", JE_SLOT_DECOY[0], JE_SLOT_DECOY[1])]
    seen = set()
    for g in al["groups"]:
        for w in g.get("withdrawn", []):
            if not isinstance(w, dict) or not str(w.get("lane", "")).startswith("W16-JG"):
                continue
            c = w.get("class", "")
            seen.add(c)
            if g["survivor"] in tgt and w["spelling"] in ours:
                cands.append((c, g["survivor"], w["spelling"]))
    used = set()
    for c, s_, o_ in cands:
        if c in used or s_ not in tgt or o_ not in ours:
            continue
        lax_ok, _ = verdict(s_, o_, "lax")
        if not lax_ok:
            continue
        out.append(("SLOT DECOY %s, lax rule PROVES it (expect REFUTED)" % c, s_, o_))
        used.add(c)
    missing = sorted(seen - used)
    if not out or missing:
        # Every withdrawal class in the ledger must contribute a decoy the lax
        # rule PROVES.  A class that silently drops out is a control that
        # stopped testing without saying so (measured: renaming a record class
        # once made the first candidate a lax-refuted one and the class vanished).
        raise SystemExit("REFUSING: no lax-PROVEN slot decoy for %s -- the slot "
                         "controls would be VACUOUS for that class."
                         % (missing or "any class"))
    want = {"SLOT-OK:VTABLE-RTTI": None, "SLOT-OK:CALLEE-CHASED": None}
    for g in sorted(al["groups"], key=lambda g: g["address"] or ""):
        if all(want.values()):
            break
        for f in g.get("folded", []):
            if g["survivor"] not in tgt or f not in ours:
                continue
            ok, kinds = verdict(g["survivor"], f, "discharge")
            if not ok or "CYCLE-ASSUMED" in kinds:
                continue
            for k in want:
                if want[k] is None and any(x.startswith(k) for x in kinds):
                    want[k] = (g["survivor"], f)
    for k, v in want.items():
        if v is None:
            raise SystemExit("REFUSING: no live membership discharges a %s slot -- "
                             "the positive control would be absent." % k)
        out.append(("SLOT POSITIVE, %s discharged (expect PROVEN)" % k[8:], v[0], v[1]))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--survivor")
    ap.add_argument("--ours")
    ap.add_argument("--pairs", help="json list of [survivor, ours] pairs")
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--chase", action="store_true",
                    help="recursive T1: relax relocation-target NAME equality to "
                         "a recursively-verified fold of that slot's callee pair")
    ap.add_argument("--family", action="store_true",
                    help="pigeonhole: how many of our spellings collapse onto "
                         "how many retail addresses")
    ap.add_argument("--chasetest", action="store_true",
                    help="controls for --chase, including an IN-FAMILY DECOY")
    ap.add_argument("--self-break", action="store_true",
                    help="run --chasetest with the vacuous branch's destination "
                         "proof REMOVED (shape still checked). The VACUOUS DECOY "
                         "control MUST go red; exits 0 only if it does. Proves "
                         "the control can fail instead of assuming it.")
    ap.add_argument("--self-break-slots", action="store_true",
                    help="run --chasetest with W16-JG's placeholder-slot DISCHARGE "
                         "removed (SLOT_POLICY='lax', the old blanket tolerance). "
                         "Every SLOT DECOY control MUST go red and every other "
                         "control must stay green; exits 0 only then.")
    ap.add_argument("--self-break-tailpad", action="store_true",
                    help="run --chasetest with retail_tail_pad's zero-tail test "
                         "REMOVED. Both TAIL-PAD DECOY controls MUST go red and "
                         "every other control must stay green; exits 0 only then.")
    ap.add_argument("--self-break-rename", action="store_true",
                    help="run --chasetest with CLASS_RENAMES accepting ANY retail "
                         "class for a renamed spelling (witness removed). The RENAME "
                         "DECOY control MUST go red and every other control must "
                         "stay green; exits 0 only then.")
    ap.add_argument("--self-break-overcarve", action="store_true",
                    help="run --chasetest with retail_overcarve's branch CENSUS "
                         "removed. The OVERCARVE DECOY control MUST go red and every "
                         "other control must stay green; exits 0 only then.")
    ap.add_argument("--self-break-size", choices=("eh", "tol", "onesided", "firstdef"),
                    help="run --chasetest with ONE wrong size rule re-introduced "
                         "(eh: ours = COMDAT section length, the W16-NF/NK measure; "
                         "tol: +-%d B tolerance; onesided: ours-larger admitted; "
                         "firstdef: only collect()'s first TU variant). "
                         "Exactly the SIZE controls that rule should break MUST go "
                         "red and every other control must stay green; exits 0 only "
                         "then." % SIZE_BREAK_TOL)
    ap.add_argument("--size", action="store_true",
                    help="also print the W16-NN like-for-like size gate per pair. "
                         "A retail side spelled 0xADDR is resolved to its target-obj "
                         "name (map name, else fn_ADDR).")
    ap.add_argument("--lax-slots", action="store_true",
                    help="reproduce a pre-W16-JG verdict (blanket placeholder "
                         "tolerance). NEVER use for an admission.")
    a = ap.parse_args()
    if a.self_break:
        globals()["_SELF_BREAK"] = True
        a.chasetest = True
    if a.self_break_slots:
        a.chasetest = True
    if a.self_break_tailpad:
        globals()["_SELF_BREAK_TAILPAD"] = True
        a.chasetest = True
    if a.self_break_rename:
        globals()["_SELF_BREAK_RENAME"] = True
        a.chasetest = True
    if a.self_break_overcarve:
        globals()["_SELF_BREAK_OVERCARVE"] = True
        a.chasetest = True
    if a.self_break_size:
        globals()["_SELF_BREAK_SIZE"] = a.self_break_size
        a.chasetest = True

    mapped = load_mapped()
    tgt, ours = load_sides()

    pairs = []
    if a.selftest:
        # A gate that cannot FAIL is worse than no gate, and a gate that cannot
        # PASS is equally useless. Both directions are exercised here against
        # ground truth taken from the already-landed scripts/symbol_aliases.json.
        al = json.load(open(ROOT / "scripts/symbol_aliases.json"))
        pos = None
        for g in al["groups"]:
            for f in g["folded"]:
                if g["survivor"] in tgt and f in ours:
                    pos = (g["survivor"], f)
                    break
            if pos:
                break
        neg_s = next(n for n in tgt if n.startswith("?") and not vacuous(tgt[n])
                     and tgt[n][2] > 400)
        neg_o = next(n for n in ours if n.startswith("?") and not vacuous(ours[n])
                     and ours[n][2] > 400 and ours[n][0] != tgt[neg_s][0])
        pairs = [("POSITIVE CONTROL (expect PROVEN)", pos[0], pos[1]),
                 ("NEGATIVE CONTROL (expect REFUTED)", neg_s, neg_o)]
    elif a.chasetest:
        # ★ The decoy is the point. A random negative control only shows the
        # comparator dislikes unrelated code; it says nothing about whether a
        # RECURSIVE comparator has gone permissive. So the decoy is IN-FAMILY:
        # fn_827B0E78 has the SAME masked body as the int-keyed operator[]
        # survivor (identical machine code) and differs ONLY in that its two
        # callees belong to the Symbol-keyed hashtable family. If --chase
        # accepts it, the recursion has dissolved exactly the discriminator it
        # must preserve, and every verdict it produces is worthless.
        UIC = ("??A?$hash_map@HPAVUIComponent@@U?$hash@H@stlpmtx_std@@U?$equal_to@H@3@"
               "V?$StlNodeAlloc@U?$pair@$$CBHPAVUIComponent@@@stlpmtx_std@@@3@@"
               "stlpmtx_std@@QAAAAPAVUIComponent@@ABH@Z")
        al = json.load(open(ROOT / "scripts/symbol_aliases.json"))
        pos = next((g["survivor"], f) for g in al["groups"] for f in g["folded"]
                   if g["survivor"] in tgt and f in ours
                   and not vacuous(tgt[g["survivor"]]))
        pairs = [("IN-FAMILY DECOY (expect REFUTED)", "fn_827B0E78", UIC),
                 ("FLAT-T1 GROUP (expect PROVEN)", pos[0], pos[1])]
        # ★ W16-AE SELF-PAIR CONTROLS.  chase() used to return True for any
        # [S, S] pair before comparing a byte, so a survivor whose own COMDAT
        # contradicts the retail body at its mapped address read PROVEN.  The
        # negative control is a name both sides define, non-vacuous, whose
        # retail extent differs from our COMDAT's -- our code for S is NOT the
        # body retail names S, and the chase must say so.  The positive control
        # is a self-pair that is byte- and reloc-identical, so the fix cannot
        # be "refuse every self-pair" either.  Both are chosen from the live
        # tree rather than hardcoded, so the control survives map repairs.
        def _self(pred):
            return next(n for n in tgt if n.startswith("?") and n in ours
                        and not vacuous(tgt[n]) and not vacuous(ours[n])
                        and tgt[n][2] > 100 and pred(tgt[n], ours[n]))
        self_neg = _self(lambda r, o: r[2] != o[2])
        self_pos = _self(lambda r, o: r[0] == o[0] and list(r[1]) == list(o[1]))
        pairs += [("SELF-PAIR NEGATIVE, our COMDAT contradicts retail (expect REFUTED)",
                   self_neg, self_neg),
                  ("SELF-PAIR POSITIVE, byte+reloc identical (expect PROVEN)",
                   self_pos, self_pos)]
        # ★ W16-GG VACUOUS-BRANCH CONTROLS.  chase()'s vacuous branch used to
        # demand LITERAL relocation-name equality and so refused a thunk pair
        # whose destinations are a PROVEN fold (W16-GD, ?CopyTypeProperties@@).
        # Relaxing it to accept a recursively-proven destination widens the
        # branch in exactly one direction, so it is probed in exactly that
        # direction: the DECOY is the same input class with destinations that
        # are NOT a fold (different bodies, identical sizes).  If the decoy ever
        # reads PROVEN the recursion has dissolved the only discriminator a
        # 4-byte thunk has, and every verdict the tool produces is worthless.
        # Run `--self-break` to watch the decoy go red on demand.
        vd_s, vd_o = vacuous_pair(tgt, ours, want_fold=False)
        vf_s, vf_o = vacuous_pair(tgt, ours, want_fold=True)
        pairs += [("VACUOUS DECOY, destinations are NOT a fold (expect REFUTED)",
                   vd_s, vd_o),
                  ("VACUOUS FOLD, destinations proven folded (expect PROVEN)",
                   vf_s, vf_o)]
        pairs += slot_controls(tgt, ours, mapped, al)
        pairs += tailpad_controls(tgt, ours)
        pairs += rename_controls(tgt, ours, mapped)
        pairs += overcarve_controls(tgt, ours)
        a.chase = True
    elif a.pairs:
        pairs = [("", s, o) for s, o in json.load(open(a.pairs))]
    else:
        pairs = [("", a.survivor, a.ours)]
    pairs = [(lb, _retail_name_at(int(s, 16)) if s.lower().startswith("0x") else s, o)
             for lb, s, o in pairs]

    # ★ W16-NN size controls: evaluated by size_gate directly (inside T1 the
    # gate is implied by masked-body equality, so a broken size rule could never
    # show there -- its consumers are fuzzy-scored naming gates).
    size_red, size_other_red = set(), 0
    if a.chasetest:
        print("\n=== SIZE GATE CONTROLS (W16-NN)%s"
              % (" -- self-break-size=%s" % a.self_break_size if a.self_break_size else ""))
        for label, rs, on, want, red_under in size_controls(tgt, ours):
            ok, why = size_gate(tgt[rs], ours[on], on)
            print("  %-62s %s  %s" % (label, "ACCEPT" if ok else "REFUSE", why))
            print("      retail %s" % rs[:90])
            if on != rs:
                print("      ours   %s" % on[:90])
            if ok != want:
                print("  ** CONTROL FAILED: wanted %s **" % ("ACCEPT" if want else "REFUSE"))
                if a.self_break_size in red_under:
                    size_red.add(label)
                else:
                    size_other_red += 1
            elif a.self_break_size in red_under:
                size_red.add("MISSING:" + label)
        expect = {lb for lb, _r, _o, _w, ru in size_controls(tgt, ours)
                  if a.self_break_size in ru}

    if a.lax_slots or a.self_break_slots:
        globals()["SLOT_POLICY"] = "lax"
    rc = 0
    slot_decoy_red = slot_other_red = n_slot_decoys = 0
    tp_decoy_red = tp_other_red = n_tp_decoys = 0
    rn_decoy_red = rn_other_red = n_rn_decoys = 0
    oc_decoy_red = oc_other_red = n_oc_decoys = 0
    for label, s, o in pairs:
        hook = _PAIR_HOOKS.get(label)
        if hook:
            hook[0]()
        verdict, det = adjudicate(tgt, ours, s, o, mapped)
        if a.size and s in tgt and o in ours:
            ok_sz, why_sz = size_gate(tgt[s], ours[o], o)
            det["size_gate"] = "%s -- %s" % ("ACCEPT" if ok_sz else "REFUSE", why_sz)
        det.update(uniqueness(tgt, ours, s, o))
        det["survivor_map_resident"] = s in mapped
        print("\n=== %s" % (label or "%s  <->  %s" % (s[:60], o[:60])))
        print("  survivor : %s" % s)
        print("  ours     : %s" % o)
        print("  FLAT T1  : %s" % verdict)
        for k, v in det.items():
            if k in ("survivor", "ours"):
                continue
            print("      %-28s %s" % (k, v))
        if a.family and s in tgt and o in ours:
            f = family(tgt, ours, o, s)
            print("  FAMILY   : %d of ours -> %d retail address(es)"
                  % (len(f["our_family"]), len(f["retail_family"])))
            print("      our_slot0_matches_retail   %s" % f["our_slot0_matches_retail"])
            print("      excluded_by_slot0          %s"
                  % [x[:48] for x in f["excluded_by_slot0"]])
            for n in f["our_family"]:
                print("        ours   %s" % n[:86])
            for n in f["retail_family"]:
                print("        RETAIL %s" % n[:86])
        if a.chase:
            trace = []
            ok = chase(tgt, ours, s, o, mapped, out=trace)
            verdict = "PROVEN" if ok else "REFUTED"
            print("  CHASED T1: %s" % verdict)
            for d, kind, x, y in trace:
                print("      %s%-22s %s" % ("  " * d, kind, x[:64]))
                print("      %s%-22s %s" % ("  " * d, "", y[:64]))
        if hook:
            hook[1]()
        if a.selftest or a.chasetest:
            want = "REFUTED" if ("NEGATIVE" in label or "DECOY" in label) else "PROVEN"
            n_slot_decoys += "SLOT DECOY" in label
            n_tp_decoys += "TAIL-PAD DECOY" in label
            n_rn_decoys += "RENAME DECOY" in label
            n_oc_decoys += "OVERCARVE DECOY" in label
            if verdict != want:
                print("  ** CONTROL FAILED: wanted %s **" % want)
                rc = 1
                if "SLOT DECOY" in label:
                    slot_decoy_red += 1
                else:
                    slot_other_red += 1
                if "TAIL-PAD DECOY" in label:
                    tp_decoy_red += 1
                else:
                    tp_other_red += 1
                if "RENAME DECOY" in label:
                    rn_decoy_red += 1
                else:
                    rn_other_red += 1
                if "OVERCARVE DECOY" in label:
                    oc_decoy_red += 1
                else:
                    oc_other_red += 1
    if a.self_break_size:
        # Exactly the controls this wrong rule should break must go red, and
        # nothing else may move (the T1/chase controls never consult size_gate,
        # so any red there is a separate defect, not this self-break).
        if size_red == expect and not size_other_red and not rc:
            print("\nself-break-size=%s OK -- exactly %d SIZE control(s) went RED: %s; "
                  "no other control moved." % (a.self_break_size, len(expect),
                                               "; ".join(sorted(expect))))
            return 0
        print("\nself-break-size=%s FAILED -- red %s, expected %s, %d other size "
              "controls red, chase rc=%d." % (a.self_break_size, sorted(size_red),
                                              sorted(expect), size_other_red, rc))
        return 1
    if a.chasetest and size_other_red:
        rc = 1
    if a.self_break_overcarve:
        if n_oc_decoys and oc_decoy_red == n_oc_decoys and not oc_other_red:
            print("\nself-break-overcarve OK -- the OVERCARVE DECOY went RED with the "
                  "branch census removed, and no other control moved.")
            return 0
        print("\nself-break-overcarve FAILED -- %d/%d overcarve decoys red, %d other "
              "controls red." % (oc_decoy_red, n_oc_decoys, oc_other_red))
        return 1
    if a.self_break_rename:
        if n_rn_decoys and rn_decoy_red == n_rn_decoys and not rn_other_red:
            print("\nself-break-rename OK -- the RENAME DECOY went RED with the "
                  "rename witness removed, and no other control moved.")
            return 0
        print("\nself-break-rename FAILED -- %d/%d rename decoys red, %d other "
              "controls red." % (rn_decoy_red, n_rn_decoys, rn_other_red))
        return 1
    if a.self_break_tailpad:
        if n_tp_decoys and tp_decoy_red == n_tp_decoys and not tp_other_red:
            print("\nself-break-tailpad OK -- all %d TAIL-PAD DECOY controls went RED "
                  "with the zero-tail test removed, and no other control moved."
                  % n_tp_decoys)
            return 0
        print("\nself-break-tailpad FAILED -- %d/%d tail-pad decoys red, %d other "
              "controls red." % (tp_decoy_red, n_tp_decoys, tp_other_red))
        return 1
    if a.self_break_slots:
        # Under --self-break-slots the discharge is removed, so EVERY slot decoy
        # must go red (each was chosen because the lax rule PROVES it -- that is
        # the hole) while every other control stays green (the discharge is the
        # only thing that changed).
        if n_slot_decoys and slot_decoy_red == n_slot_decoys and not slot_other_red:
            print("\nself-break-slots OK -- all %d SLOT DECOY controls went RED with "
                  "the discharge removed, and no other control moved." % n_slot_decoys)
            return 0
        print("\nself-break-slots FAILED -- %d/%d slot decoys red, %d other controls "
              "red. The slot controls do not discriminate; do not trust them."
              % (slot_decoy_red, n_slot_decoys, slot_other_red))
        return 1
    if a.self_break:
        # Under --self-break the relaxation is deliberately broken, so a GREEN
        # run is the failure: it would mean the decoy cannot detect the very
        # permissiveness it exists to detect.
        if rc:
            print("\nself-break OK -- the VACUOUS DECOY went RED with the "
                  "destination proof removed, so the control discriminates.")
            return 0
        print("\nself-break FAILED -- controls stayed GREEN with the destination "
              "proof removed. The decoy control is VACUOUS; do not trust it.")
        return 1
    if a.selftest or a.chasetest:
        print("\nselftest %s" % ("FAILED" if rc else "PASSED -- the instrument can "
                                                     "both pass and fail"))
    return rc


if __name__ == "__main__":
    sys.exit(main())
