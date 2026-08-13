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


# ------------------------------------------------------------------- our side
def our_sizes(objroot, wanted):
    """Decode the SAME immediate out of OUR compiled objs.

    This is the compiler's own `sizeof` answer, transitively -- and it is the
    exact quantity objdiff compares, so it cannot drift from the metric the way
    a header comment can.
    """
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from coff_bodies_ext import function_bodies_ext
    found = {}
    for obj in Path(objroot).rglob("*.obj"):
        try:
            for name, body, _relocs, _entry in function_bodies_ext(str(obj)):
                if name in wanted and name not in found:
                    r = scan_body(body, 0)
                    r["obj"] = str(obj)
                    found[name] = r
        except Exception:
            continue
    return found


# ---------------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--objroot", default="build/45410914/src")
    ap.add_argument("--survey", action="store_true",
                    help="size the WIDER population: every named row that calls "
                         "an allocator with a decodable constant")
    ap.add_argument("--selfcheck", action="store_true")
    ap.add_argument("--json", help="write full rows here")
    args = ap.parse_args()

    data, secs = load_pe()
    m, meta = load_map()
    rows = newobject_rows(m)

    if args.selfcheck:
        return selfcheck(data, secs, rows)

    # ---- target side
    tgt = {}
    for va, cls, name in rows:
        tgt[name] = dict(target_size_at(va, data, secs), va=va, cls=cls)

    alloc_targets = collections.Counter(
        r["alloc_target"] for r in tgt.values() if r["status"] == "ok")

    # ---- our side
    ours = our_sizes(args.objroot, set(tgt))

    out = []
    for va, cls, name in rows:
        t = tgt[name]
        o = ours.get(name)
        rec = {
            "va": f"0x{va:08x}", "cls": cls, "sym": name,
            "target_size": t["size"], "target_status": t["status"],
            "alloc_target": (f"0x{t['alloc_target']:08x}"
                             if t.get("alloc_target") else None),
            "our_size": o["size"] if o else None,
            "our_status": o["status"] if o else "not_compiled",
            "our_obj": o.get("obj") if o else None,
        }
        if rec["target_size"] is not None and rec["our_size"] is not None:
            rec["delta"] = rec["our_size"] - rec["target_size"]
            rec["verdict"] = "AGREE" if rec["delta"] == 0 else "DISAGREE"
        else:
            rec["delta"] = None
            rec["verdict"] = "UNCHECKABLE"
        out.append(rec)

    if args.survey:
        survey(data, secs, m, set(alloc_targets), out)

    verd = collections.Counter(r["verdict"] for r in out)
    print(f"# NewObject rows in map: {len(rows)}")
    print(f"# target decoded ok    : {sum(1 for r in out if r['target_status']=='ok')}"
          f"  (undecoded/no_call: "
          f"{sum(1 for r in out if r['target_status']!='ok')})")
    print(f"# our side decoded ok  : {sum(1 for r in out if r['our_status']=='ok')}"
          f"  (not_compiled: "
          f"{sum(1 for r in out if r['our_status']=='not_compiled')})")
    print(f"# allocator callees seen: "
          f"{', '.join(f'0x{a:08x}x{c}' for a, c in alloc_targets.most_common())}")
    print(f"# VERDICTS: {dict(verd)}")
    print()
    dis = [r for r in out if r["verdict"] == "DISAGREE"]
    dis.sort(key=lambda r: -abs(r["delta"]))
    print(f"{'class':32s} {'va':10s} {'ours':>7s} {'retail':>7s} {'delta':>7s}")
    for r in dis:
        print(f"{r['cls'][:32]:32s} {r['va']:10s} {r['our_size']:7d} "
              f"{r['target_size']:7d} {r['delta']:+7d}")

    if args.json:
        Path(args.json).write_text(json.dumps(out, indent=1))
        print(f"\n[wrote {args.json}]")


def survey(data, secs, m, alloc_set, newobj_rows_out):
    """Size the WIDER population honestly.

    Scans EVERY named row for a call to a known allocator with a decodable
    constant, then splits by whether the row NAME determines the allocated type.
    The second number is the one that matters: a size you cannot attribute to a
    class is not a checkable fact.
    """
    print("\n=== SURVEY: how far does the discriminator generalise? ===")
    hits = []
    for addr, name in m.items():
        va = int(addr, 16)
        off = va_to_off(va, secs)
        if off is None:
            continue
        r = scan_body(data[off:off + 4 * 64], va)
        if r["status"] == "ok" and r["alloc_target"] in alloc_set:
            hits.append((name, va, r["size"]))
    print(f"rows calling a known allocator with a CONSTANT first arg: {len(hits)}")
    kinds = collections.Counter()
    for name, va, size in hits:
        if NEWOBJECT_RE.match(name):
            kinds["NewObject (type known from name)"] += 1
        elif name.startswith("??0"):
            kinds["constructor"] += 1
        elif name.startswith("??_U") or name.startswith("??2"):
            kinds["operator new / array new"] += 1
        elif "Clone" in name or "Copy" in name:
            kinds["Clone/Copy-shaped"] += 1
        else:
            kinds["other (type NOT determined by name)"] += 1
    for k, v in kinds.most_common():
        print(f"  {v:6d}  {k}")
    return hits


def selfcheck(data, secs, rows):
    """*** PROVE THE DECODER FIRES BEFORE ANYONE BELIEVES ITS SILENCE. ***"""
    ok = True
    byname = {n: (va, c) for va, c, n in rows}

    # 1. MUST FIRE on the two hand-read positives from lane BODIES-6.
    for cls, expect in (("UIPanel", 264), ("PreloadPanel", 164)):
        sym = f"?NewObject@{cls}@@SAPAVObject@Hmx@@XZ"
        va = byname[sym][0]
        r = target_size_at(va, data, secs)
        good = r["status"] == "ok" and r["size"] == expect
        ok &= good
        print(f"[{'PASS' if good else 'FAIL'}] must_fire {cls} @0x{va:08x} "
              f"-> {r['size']} (expect {expect})")

    # 2. MUST NOT mistake the `li r4, 1` that follows for the size.
    va = byname["?NewObject@UIPanel@@SAPAVObject@Hmx@@XZ"][0]
    r4 = target_size_at(va, data, secs, size_reg=4)
    good = not (r4["status"] == "ok" and r4["size"] == 264)
    ok &= good
    print(f"[{'PASS' if good else 'FAIL'}] must_not_fire r4 tracker must not "
          f"report 264 (got {r4})")

    # 3. INDEPENDENT hand decode must agree with capstone on every ok row.
    dis = 0
    checked = 0
    for va, cls, name in rows:
        off = va_to_off(va, secs)
        if off is None:
            continue
        blob = data[off:off + 4 * 64]
        r = scan_body(blob, va)
        if r["status"] != "ok":
            continue
        # find the li rD,imm that capstone-based tracking used
        hand = None
        for i in range(0, 64 * 4, 4):
            w = struct.unpack_from(">I", blob, i)[0]
            d = decode_via_fields(w)
            if d and d[0] == 3:
                hand = d[1]
            if (w >> 26) & 0x3F == 18 and (w & 1):
                break
        checked += 1
        if hand != r["size"]:
            dis += 1
            if dis <= 5:
                print(f"   hand/capstone disagree {cls}: hand={hand} cs={r['size']}")
    good = dis == 0
    ok &= good
    print(f"[{'PASS' if good else 'FAIL'}] independent hand decode agrees with "
          f"capstone on {checked - dis}/{checked} rows")

    print("\nSELFCHECK", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main() or 0)
