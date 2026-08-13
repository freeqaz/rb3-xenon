#!/usr/bin/env python3
"""The allocation-size immediate: an UNMASKED discriminator for masked-class rows.

WHY THIS EXISTS
---------------
CLAUDE.md records 3,262 named rows / 368,948 B whose name<->address assignment is
UNCONSTRAINED BY THE METRIC, because objdiff masks relocations: under
`functionRelocDiffs=none` a `bl` to the RIGHT constructor and a `bl` to the WRONG
one score IDENTICALLY.  Every audit of that population so far has needed EXTERNAL
evidence (RTTI strings, vtable membership, retail bytes read by hand).

`X::NewObject()` is a textbook member of the masked class: every one is
alloc-then-construct with the identical shape, and the ctor `bl` is masked, so
they pair almost arbitrarily.

    li      r3, <SIZE>        <-- A PLAIN IMMEDIATE.  *** NOT MASKED. ***
    bl      <operator new>
    bl      <T::T()>          <-- masked; scores the same against any ctor

So the allocation size is a fact about the row that the metric can SEE, and the
compiler will independently tell us `sizeof(T)`.  Disagreement is therefore
either a FALSE PAIRING (this address is not that class's NewObject) or a REAL
LAYOUT DEFECT (our T has the wrong size).  The two are distinguishable, because a
false pairing usually disagrees wildly while a layout defect is off by a small
multiple of 4.

WHAT IS AND IS NOT CHECKABLE
----------------------------
A `li r3, N; bl operator_new` ANYWHERE tells you N bytes were allocated.  It does
NOT tell you of WHAT -- inside `Foo::Bar()` the allocated type is invisible.  The
population this instrument can adjudicate is exactly:

    *** rows whose NAME determines the allocated type ***

which is why the seed case is `?NewObject@T@@SAPAVObject@Hmx@@XZ`.  `--survey`
sizes the wider population honestly rather than assuming it generalises.

DECODER HAZARDS THIS FILE DELIBERATELY AVOIDS
---------------------------------------------
Six screens produced clean, decisive-looking, WRONG NEGATIVES in one session
(see tools/screen_gate.py).  Two are live hazards for THIS decoder:

  * hand-rolled field extraction with rD/rA SWAPPED scored 0 hits across 14 MB.
    -> so the immediate is decoded with CAPSTONE, not by hand-shifting bits,
       and `decode_via_fields()` exists ONLY as an independent cross-check that
       `--selfcheck` asserts against capstone on real bodies.
  * a decoder tracking only r3 missed the r4 struct-return form.
    -> `NewObject` returns a POINTER, so the size is genuinely r3 here; but the
       tracker follows a REGISTER PARAMETER (`--size-reg`) instead of hardcoding
       r3, and reports UNDECODED rather than silence when it cannot see a
       constant.

*** THE CARDINAL RULE: this screen reports UNDECODED loudly. ***
A row it cannot read is NOT a row that agrees.  `undecoded` is a separate bucket
in every summary and is never folded into `ok`.
"""
import argparse
import collections
import json
import re
import struct
import sys
from pathlib import Path

import capstone

PE = "orig/45410914/band.exe"
MAP = "scripts/target_symbol_map.json"

# ?NewObject@<Class>@@SAPAVObject@Hmx@@XZ  -- static T* T::NewObject()
NEWOBJECT_RE = re.compile(r"^\?NewObject@([A-Za-z0-9_]+)@@SAPAVObject@Hmx@@XZ$")


# ---------------------------------------------------------------- PE plumbing
def load_pe(path=PE):
    data = Path(path).read_bytes()
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    assert data[e_lfanew:e_lfanew + 4] == b"PE\x00\x00", "not a PE"
    coff = e_lfanew + 4
    num_sec = struct.unpack_from("<H", data, coff + 2)[0]
    opt_size = struct.unpack_from("<H", data, coff + 16)[0]
    opt = coff + 20
    image_base = struct.unpack_from("<I", data, opt + 28)[0]
    sec_tab = opt + opt_size
    secs = []
    for i in range(num_sec):
        off = sec_tab + i * 40
        name = data[off:off + 8].rstrip(b"\x00").decode("latin1")
        vsize, vaddr, rawsize, rawptr = struct.unpack_from("<IIII", data, off + 8)
        secs.append((name, image_base + vaddr, vsize, rawptr, rawsize))
    return data, secs


def va_to_off(va, secs):
    for name, sva, vsize, rawptr, rawsize in secs:
        if sva <= va < sva + vsize:
            d = va - sva
            if d < rawsize:
                return rawptr + d
    return None


_MD = None


def md():
    global _MD
    if _MD is None:
        _MD = capstone.Cs(capstone.CS_ARCH_PPC,
                          capstone.CS_MODE_BIG_ENDIAN | capstone.CS_MODE_32)
        _MD.detail = True
    return _MD


# ------------------------------------------------------------- the decoder
def decode_via_fields(word):
    """INDEPENDENT hand decode, used ONLY to cross-check capstone.

    Deliberately written out longhand so a field swap here cannot silently agree
    with a field swap there.  `li rD, SIMM` is `addi rD, 0, SIMM`: primary
    opcode 14, rD = bits 6..10, rA = bits 11..15 and MUST be 0.
    """
    op = (word >> 26) & 0x3F
    if op != 14:
        return None
    rD = (word >> 21) & 0x1F
    rA = (word >> 16) & 0x1F
    if rA != 0:
        return None
    simm = word & 0xFFFF
    if simm & 0x8000:
        simm -= 0x10000
    return rD, simm


def call_sites(code, va, size_reg=3, max_insn=96):
    """Walk a WHOLE body and return every `bl` with the constant live in size_reg.

    *** THE FIRST `bl` IS NOT THE ALLOCATOR. ***
    An early version of this file stopped at the first call and read 124 of 209
    NewObject rows as `undecoded` -- a 59% silence that looked like a thin
    population.  It is not: retail emits e.g.

        addi  r3, r31, 0x50
        bl    <String::String>      <-- a stack temp, constructed FIRST
        li    r4, 0
        li    r3, 0x1c8             <-- the size, re-materialised AFTER the call
        bl    <operator new>

    so the size lives in r3 only at the SECOND call.  Volatile registers
    (r0, r3..r12) are therefore CLOBBERED at every `bl`, which is both correct
    PPC ABI and what makes the re-materialisation visible.

    Returns list of (target_va, size_or_None).
    """
    reg = {}
    sites = []
    n = 0
    for ins in md().disasm(code, va):
        n += 1
        if n > max_insn:
            break
        m = ins.mnemonic
        ops = ins.op_str
        if m == "bl":
            tgt = int(ops, 0) if ops.startswith("0x") else None
            sites.append((tgt, reg.get(size_reg)))
            for r in [0] + list(range(3, 13)):
                reg.pop(r, None)
            continue
        if m == "blr":
            break
        # constant materialisation
        if m == "li":
            d, imm = ops.split(",")
            reg[int(d.strip()[1:])] = int(imm, 0)
        elif m == "lis":
            d, imm = ops.split(",")
            reg[int(d.strip()[1:])] = (int(imm, 0) << 16) & 0xFFFFFFFF
        elif m == "ori":
            a, s, imm = [x.strip() for x in ops.split(",")]
            sr = int(s[1:])
            if sr in reg:
                reg[int(a[1:])] = reg[sr] | int(imm, 0)
            else:
                reg.pop(int(a[1:]), None)
        elif m in ("addi", "addic"):
            d, a, imm = [x.strip() for x in ops.split(",")]
            ar = int(a[1:])
            if ar in reg:
                reg[int(d[1:])] = reg[ar] + int(imm, 0)
            else:
                reg.pop(int(d[1:]), None)
        elif m == "mr":
            d, s = [x.strip() for x in ops.split(",")]
            sr = int(s[1:])
            if sr in reg:
                reg[int(d[1:])] = reg[sr]
            else:
                reg.pop(int(d[1:]), None)
        else:
            try:
                for r in ins.regs_access()[1]:
                    nm = ins.reg_name(r)
                    if nm and nm.startswith("r") and nm[1:].isdigit():
                        reg.pop(int(nm[1:]), None)
            except Exception:
                pass
    return sites


def scan_body(code, va, size_reg=3, max_insn=96, allocators=None):
    """Pick the allocation site out of a body.

    `allocators` is the empirically-derived callee set (see derive_allocators).
    With it, the size is the constant live at the first call to a KNOWN
    allocator.  Without it (bootstrap pass), fall back to the first call that
    has a constant at all.
    """
    sites = call_sites(code, va, size_reg=size_reg, max_insn=max_insn)
    if not sites:
        return {"size": None, "alloc_target": None, "status": "no_call"}
    if allocators is not None:
        for tgt, size in sites:
            if tgt in allocators:
                if size is None:
                    return {"size": None, "alloc_target": tgt,
                            "status": "undecoded"}
                return {"size": size, "alloc_target": tgt, "status": "ok"}
        return {"size": None, "alloc_target": None, "status": "no_allocator"}
    for tgt, size in sites:
        if size is not None:
            return {"size": size, "alloc_target": tgt, "status": "ok"}
    return {"size": None, "alloc_target": sites[0][0], "status": "undecoded"}


def target_size_at(va, data, secs, size_reg=3, allocators=None):
    off = va_to_off(va, secs)
    if off is None:
        return {"size": None, "alloc_target": None, "status": "not_in_image"}
    return scan_body(data[off:off + 4 * 96], va, size_reg=size_reg,
                     allocators=allocators)


def derive_allocators(rows, data, secs, min_hits=3):
    """Derive the allocator callee set EMPIRICALLY, not from the map.

    The map does not name 0x827bd2f0 at all, so asking it "where is operator
    new" returns nothing -- a decisive-looking negative of exactly the kind this
    project keeps getting burned by.  Instead: over the 209 known-shape
    NewObject bodies, histogram every callee that is reached with a CONSTANT in
    r3.  Real allocators dominate; a one-off ctor that happens to follow a
    constant does not clear `min_hits`.
    """
    hist = collections.Counter()
    for va, cls, name in rows:
        off = va_to_off(va, secs)
        if off is None:
            continue
        for tgt, size in call_sites(data[off:off + 4 * 96], va):
            if tgt is not None and size is not None:
                hist[tgt] += 1
    return {t for t, c in hist.items() if c >= min_hits}, hist


# --------------------------------------------------------------- map loading
def load_map(path=MAP):
    """address -> mangled name, skipping the meta-keys (_bijection_arbitrary etc).

    Those keys are LISTS, and one of them holds ~900 addresses; a naive
    `'X' in name` over raw values raises or, worse, matches inside them.
    """
    raw = json.load(open(path))
    out = {}
    meta = {}
    for k, v in raw.items():
        if k.startswith("_"):
            meta[k] = v
            continue
        if isinstance(v, str):
            out[k] = v
    return out, meta


def newobject_rows(m):
    rows = []
    for addr, name in m.items():
        mt = NEWOBJECT_RE.match(name)
        if mt:
            rows.append((int(addr, 16), mt.group(1), name))
    rows.sort()
    return rows




STATICNAME_RE = re.compile(r"^\?StaticClassName@([A-Za-z0-9_]+)@@SA\?AVSymbol@@XZ$")
# Allocator symbols as OUR objs name them.  Retail's are identified separately,
# by empirical histogram, because the map does not name 0x827bd2f0 at all.
OUR_ALLOCATORS = {
    "??2@YAPAXI@Z",              # operator new(size_t)
    "?MemAlloc@@YAPAXHH@Z",      # Milo tagged alloc
    "?PoolAlloc@@YAPAXHHPBDH0@Z",
    "?_MemAllocTemp@@YAPAXHH@Z",
}


# ------------------------------------------------------------------- our side
def our_alloc_info(objroot, wanted):
    """Decode OUR side: size, allocator, ctor, and the TAG CLASS.

    *** THE TAG CLASS IS THE ANCESTOR CHECK, AND THE COMPILER COMPUTES IT. ***
    `NgMat : public RndMat` does not override StaticClassName, so OUR
    NgMat::NewObject emits a reloc to `?StaticClassName@RndMat@@...`.  That is
    the compiler resolving the inheritance for us -- no header parsing, no
    hand-maintained hierarchy, and no chance of my getting the ancestry wrong.

    This matters because it is the difference between a finding and a FALSE
    POSITIVE: retail's NgMat row tags "Mat" and calls RndMat::StaticClassName,
    which is EXACTLY what a correct NgMat::NewObject does.  Reading an inherited
    static as evidence of non-identity is the "a callee NAME never witnesses a
    member type" trap wearing a new hat -- AN INHERITED STATIC NAMES THE BASE.
    """
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from coff_bodies_ext import function_bodies_ext
    found = {}
    for obj in Path(objroot).rglob("*.obj"):
        try:
            bodies = list(function_bodies_ext(str(obj)))
        except Exception:
            continue
        for name, body, relocs, _entry in bodies:
            if name not in wanted or name in found:
                continue
            rel = {o: sym for o, sym, _t in relocs}
            sites = call_sites(body, 0)
            bl_offs = [i.address for i in md().disasm(body, 0)
                       if i.mnemonic == "bl"]
            calls = [(rel.get(o), sz) for (_t, sz), o in zip(sites, bl_offs)]
            size = alloc = ctor = tag = None
            for sym, sz in calls:
                if sym in OUR_ALLOCATORS and size is None:
                    size, alloc = sz, sym
                elif sym and STATICNAME_RE.match(sym) and tag is None:
                    tag = STATICNAME_RE.match(sym).group(1)
                elif sym and sym.startswith("??0") and ctor is None:
                    ctor = sym
            found[name] = {"size": size, "alloc": alloc, "ctor": ctor,
                           "tag_class": tag, "obj": str(obj),
                           "status": "ok" if size is not None else "undecoded"}
    return found


# --------------------------------------------------------------- retail tags
def cstr_at(va, data, secs, maxlen=64):
    off = va_to_off(va, secs)
    if off is None:
        return None
    end = data.find(b"\0", off, off + maxlen)
    if end < 0:
        return None
    raw = data[off:end]
    if not raw or not all(32 <= c < 127 for c in raw):
        return None
    return raw.decode("ascii")


def literal_strings(va, data, secs, max_insn=140):
    """Recover `lis rX,hi; addi rY,rX,lo` string literals inside a callee."""
    off = va_to_off(va, secs)
    if off is None:
        return []
    hi, out = {}, []
    for ins in md().disasm(data[off:off + max_insn * 4], va):
        if ins.mnemonic == "lis":
            d, imm = ins.op_str.split(",")
            hi[int(d.strip()[1:])] = int(imm, 0) & 0xFFFF
        elif ins.mnemonic == "addi":
            d, a, imm = [x.strip() for x in ins.op_str.split(",")]
            ar = int(a[1:])
            if ar in hi:
                cand = ((hi[ar] << 16) + int(imm, 0)) & 0xFFFFFFFF
                s = cstr_at(cand, data, secs)
                if s:
                    out.append(s)
        elif ins.mnemonic == "blr":
            break
    return out


def retail_row_facts(va, data, secs, allocators, m):
    """size + allocator + TAG (class name, map-side and string-side) for a row."""
    off = va_to_off(va, secs)
    if off is None:
        return {"status": "not_in_image"}
    sites = call_sites(data[off:off + 4 * 96], va)
    size = alloc_va = tag_va = ctor_va = None
    for i, (tgt, sz) in enumerate(sites):
        if tgt in allocators:
            size, alloc_va = sz, tgt
            if i > 0:
                tag_va = sites[i - 1][0]
            if i + 1 < len(sites):
                ctor_va = sites[i + 1][0]
            break
    if alloc_va is None:
        return {"status": "no_allocator", "size": None}
    tag_class = tag_string = None
    if tag_va is not None:
        nm = m.get(f"0x{tag_va:08x}")
        mt = STATICNAME_RE.match(nm) if nm else None
        tag_class = mt.group(1) if mt else None
        lits = literal_strings(tag_va, data, secs)
        # a StaticClassName body holds exactly its own class token
        tag_string = lits[0] if len(lits) == 1 else None
    return {"status": "ok" if size is not None else "undecoded", "size": size,
            "alloc_va": alloc_va, "tag_va": tag_va, "tag_class": tag_class,
            "tag_string": tag_string, "ctor_va": ctor_va}


# ---------------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--objroot", default="build/45410914/src")
    ap.add_argument("--survey", action="store_true")
    ap.add_argument("--selfcheck", action="store_true")
    ap.add_argument("--json")
    args = ap.parse_args()

    data, secs = load_pe()
    m, meta = load_map()
    rows = newobject_rows(m)
    allocators, hist = derive_allocators(rows, data, secs)

    if args.selfcheck:
        return selfcheck(data, secs, rows, allocators, m, args.objroot)

    tgt = {name: retail_row_facts(va, data, secs, allocators, m)
           for va, cls, name in rows}
    ours = our_alloc_info(args.objroot, set(tgt))

    out = []
    for va, cls, name in rows:
        t, o = tgt[name], ours.get(name, {})
        rec = {
            "va": f"0x{va:08x}", "cls": cls, "sym": name,
            "target_size": t.get("size"), "target_status": t.get("status"),
            "target_tag_class": t.get("tag_class"),
            "target_tag_string": t.get("tag_string"),
            "our_size": o.get("size"), "our_tag_class": o.get("tag_class"),
            "our_ctor": o.get("ctor"), "our_obj": o.get("obj"),
            "our_status": o.get("status", "not_compiled"),
        }
        ts, os_ = rec["target_size"], rec["our_size"]
        tt, ot = rec["target_tag_class"], rec["our_tag_class"]
        # *** ANCESTOR CHECK FIRST. ***
        # Both tags known and EQUAL => the tag cannot discriminate (it is the
        # inherited base for both), so fall through and judge on SIZE alone.
        if tt and ot and tt != ot:
            rec["verdict"] = "FALSE_PAIRING_TAG"
            rec["delta"] = None if (ts is None or os_ is None) else os_ - ts
        elif ts is not None and os_ is not None:
            rec["delta"] = os_ - ts
            rec["verdict"] = "AGREE" if os_ == ts else "SIZE_DISAGREE"
        else:
            rec["delta"] = None
            rec["verdict"] = "UNCHECKABLE"
        out.append(rec)

    verd = collections.Counter(r["verdict"] for r in out)
    ndec = sum(1 for r in out if r["target_status"] == "ok")
    print(f"# NewObject rows in map      : {len(rows)}")
    print(f"# target size decoded        : {ndec}"
          f"   (undecoded/no_allocator: {len(rows)-ndec})")
    print(f"# our size decoded           : "
          f"{sum(1 for r in out if r['our_status']=='ok')}")
    print(f"# rows carrying a TAG class  : "
          f"{sum(1 for r in out if r['target_tag_class'])}"
          f"   (tag-bearing = MemAlloc form; plain operator-new has NO tag)")
    print(f"# retail allocators (derived): "
          f"{', '.join(f'0x{a:08x}' for a in sorted(allocators))}")
    print(f"# VERDICTS: {dict(verd)}")

    tagged = [r for r in out if r["target_tag_class"] and r["our_tag_class"]]
    agree_tag = [r for r in tagged if r["target_tag_class"] == r["our_tag_class"]]
    print(f"\n# ANCESTOR CHECK: {len(tagged)} rows have a tag on BOTH sides; "
          f"{len(agree_tag)} agree and are therefore NOT discriminated by the "
          f"tag\n#   (these are the FALSE POSITIVES the check eliminates: "
          f"{len(tagged)-len(agree_tag)} survive)")

    print(f"\n{'class':26s} {'va':11s} {'ours':>6s} {'retail':>7s} {'d':>6s}"
          f"  {'ourtag':14s} {'rettag':14s} {'string':10s} verdict")
    for r in sorted(out, key=lambda r: (r["verdict"], r["cls"])):
        if r["verdict"] in ("AGREE", "UNCHECKABLE"):
            continue
        print(f"{r['cls'][:26]:26s} {r['va']:11s} "
              f"{str(r['our_size']):>6s} {str(r['target_size']):>7s} "
              f"{str(r['delta']):>6s}  {str(r['our_tag_class'])[:14]:14s} "
              f"{str(r['target_tag_class'])[:14]:14s} "
              f"{str(r['target_tag_string'])[:10]:10s} {r['verdict']}")

    if args.survey:
        survey(data, secs, m, allocators)
    if args.json:
        Path(args.json).write_text(json.dumps(out, indent=1))
        print(f"\n[wrote {args.json}]")


def survey(data, secs, m, alloc_set):
    """Size the WIDER population HONESTLY.

    A `li r3, N; bl operator_new` anywhere proves N bytes were allocated.  It
    does NOT say of WHAT: inside `Foo::Bar()` the allocated type is invisible,
    so the size is not a CHECKABLE fact about the row.  The population this
    instrument can adjudicate is exactly the rows whose NAME (or tag) determines
    the allocated type.  Both numbers are printed so the gap is explicit.
    """
    print("\n=== SURVEY: how far does the discriminator generalise? ===")
    hits, tagged = [], 0
    for addr, name in m.items():
        va = int(addr, 16)
        off = va_to_off(va, secs)
        if off is None:
            continue
        sites = call_sites(data[off:off + 4 * 96], va)
        for i, (tgt, sz) in enumerate(sites):
            if tgt in alloc_set and sz is not None:
                hits.append((name, va, sz))
                if i > 0:
                    nm = m.get(f"0x{sites[i-1][0]:08x}") if sites[i - 1][0] else None
                    if nm and STATICNAME_RE.match(nm):
                        tagged += 1
                break
    kinds = collections.Counter()
    for name, va, sz in hits:
        if NEWOBJECT_RE.match(name):
            kinds["NewObject  -- type KNOWN from name"] += 1
        elif name.startswith("??0"):
            kinds["constructor -- type known, but size is of a MEMBER"] += 1
        elif name.startswith("??_U") or name.startswith("??2"):
            kinds["operator new / array new"] += 1
        else:
            kinds["other -- type NOT determined by the row name"] += 1
    print(f"named rows calling a known allocator with a CONSTANT size: {len(hits)}")
    print(f"  of which carry a StaticClassName TAG (type recoverable): {tagged}")
    for k, v in kinds.most_common():
        print(f"  {v:6d}  {k}")


def selfcheck(data, secs, rows, allocators, m, objroot):
    """*** PROVE THE SCREEN FIRES BEFORE ANYONE BELIEVES ITS SILENCE. ***"""
    ok = True
    byname = {n: va for va, c, n in rows}

    for cls, expect in (("UIPanel", 264), ("PreloadPanel", 164),
                        ("SkeletonClip", 456), ("NgMat", 592),
                        ("PropertyEventProvider", 68)):
        va = byname[f"?NewObject@{cls}@@SAPAVObject@Hmx@@XZ"]
        r = retail_row_facts(va, data, secs, allocators, m)
        good = r.get("size") == expect
        ok &= good
        print(f"[{'PASS' if good else 'FAIL'}] must_fire size {cls} "
              f"-> {r.get('size')} (expect {expect})")

    # the tag must be recovered, map-side AND string-side, on the MemAlloc form
    va = byname["?NewObject@SkeletonClip@@SAPAVObject@Hmx@@XZ"]
    r = retail_row_facts(va, data, secs, allocators, m)
    good = r.get("tag_class") == "RndText" and r.get("tag_string") == "Text"
    ok &= good
    print(f"[{'PASS' if good else 'FAIL'}] must_fire tag SkeletonClip-row -> "
          f"class={r.get('tag_class')} string={r.get('tag_string')!r} "
          f"(expect RndText / 'Text')")

    # MUST NOT invent a tag for the plain operator-new form
    va = byname["?NewObject@UIPanel@@SAPAVObject@Hmx@@XZ"]
    r = retail_row_facts(va, data, secs, allocators, m)
    good = r.get("tag_class") is None
    ok &= good
    print(f"[{'PASS' if good else 'FAIL'}] must_not_fire UIPanel (plain "
          f"operator-new) must have NO tag -> {r.get('tag_class')}")

    # *** THE ANCESTOR CHECK MUST SUPPRESS NgMat. ***
    ours = our_alloc_info(objroot, {f"?NewObject@{c}@@SAPAVObject@Hmx@@XZ"
                                    for c in ("NgMat", "SkeletonClip")})
    ng = ours.get("?NewObject@NgMat@@SAPAVObject@Hmx@@XZ", {})
    va = byname["?NewObject@NgMat@@SAPAVObject@Hmx@@XZ"]
    rt = retail_row_facts(va, data, secs, allocators, m)
    good = ng.get("tag_class") == "RndMat" == rt.get("tag_class")
    ok &= good
    print(f"[{'PASS' if good else 'FAIL'}] must_not_fire ancestor check: NgMat "
          f"tag ours={ng.get('tag_class')} retail={rt.get('tag_class')} "
          f"-- an INHERITED static names the BASE, so this must NOT read as a "
          f"false pairing")

    dis = checked = 0
    for va, cls, name in rows:
        off = va_to_off(va, secs)
        if off is None:
            continue
        blob = data[off:off + 4 * 96]
        r = scan_body(blob, va, allocators=allocators)
        if r["status"] != "ok":
            continue
        hand = None
        for i in range(0, 96 * 4, 4):
            w = struct.unpack_from(">I", blob, i)[0]
            d = decode_via_fields(w)
            if d and d[0] == 3:
                hand = d[1]
            if (w >> 26) & 0x3F == 18 and (w & 1) and hand == r["size"]:
                break
        checked += 1
        if hand != r["size"]:
            dis += 1
    good = dis == 0
    ok &= good
    print(f"[{'PASS' if good else 'FAIL'}] independent hand decode agrees with "
          f"capstone on {checked-dis}/{checked} rows")
    print("\nSELFCHECK", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main() or 0)
