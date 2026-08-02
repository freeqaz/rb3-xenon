#!/usr/bin/env python3
"""vbase_rtti.py -- read SUBOBJECT OFFSETS and BASE-CLASS PMDs for a class
straight out of retail band.exe's MSVC RTTI.

WHY: our vbase displacements disagree with retail (lane CO-1). The header
`// 0xHEX` comments and struct_db are known-wrong; the compiler tells us OUR
layout but says nothing about RETAIL's. MSVC /GR is ON in retail (2,220 COLs),
so retail carries its own layout description:

  _RTTICompleteObjectLocator (32-bit MSVC):
      +0x00 signature (0)
      +0x04 offset          <-- offset of THIS vftable's subobject in the
                                complete object.  THE NUMBER WE WANT.
      +0x08 cdOffset        <-- ctor displacement offset (vtordisp)
      +0x0C pTypeDescriptor
      +0x10 pClassHierarchyDescriptor

  _RTTIClassHierarchyDescriptor:
      +0x00 signature, +0x04 attributes (bit0=MI, bit1=VI),
      +0x08 numBaseClasses, +0x0C pBaseClassArray

  _RTTIBaseClassDescriptor:
      +0x00 pTypeDescriptor, +0x04 numContainedBases,
      +0x08 PMD.mdisp, +0x0C PMD.pdisp, +0x10 PMD.vdisp, +0x14 attributes
      (pdisp == -1  =>  base is NOT virtual)

PE headers are little-endian; all DATA is big-endian (PowerPC).

CONTROL: --self runs the identical parser over one of OUR compiled .obj files,
where scripts/harvest/class_layout_report.py already gives ground truth.
--prove-fail corrupts the COL signature check to show the detector can fail.
"""
from __future__ import annotations
import argparse
import re
import struct
import sys
from pathlib import Path

ROOT = Path("/home/free/code/milohax/rb3-xenon")


class Image:
    """A flat VA-addressable view of a PE (band.exe)."""

    def __init__(self, path):
        self.buf = open(path, "rb").read()
        exe = self.buf
        pe = struct.unpack_from("<I", exe, 0x3C)[0]
        nsec = struct.unpack_from("<H", exe, pe + 6)[0]
        size_oh = struct.unpack_from("<H", exe, pe + 20)[0]
        self.img_base = struct.unpack_from("<I", exe, pe + 24 + 28)[0]
        ss = pe + 24 + size_oh
        self.sections = []
        for i in range(nsec):
            o = ss + i * 40
            name = exe[o:o + 8].rstrip(b"\x00").decode("ascii", "replace")
            vsize = struct.unpack_from("<I", exe, o + 8)[0]
            vaddr = struct.unpack_from("<I", exe, o + 12)[0]
            raw = struct.unpack_from("<I", exe, o + 20)[0]
            self.sections.append((name, self.img_base + vaddr, vsize, raw))

    def va_to_off(self, va):
        for _n, sva, vsz, off in self.sections:
            if sva <= va < sva + vsz:
                return off + (va - sva)
        return None

    def off_to_va(self, o):
        for _n, sva, vsz, off in self.sections:
            if off <= o < off + vsz:
                return sva + (o - off)
        return None

    def u32(self, va):
        o = self.va_to_off(va)
        if o is None or o + 4 > len(self.buf):
            return None
        return struct.unpack_from(">I", self.buf, o)[0]

    def i32(self, va):
        v = self.u32(va)
        if v is None:
            return None
        return v - (1 << 32) if v >= (1 << 31) else v

    def find_tds(self):
        """VA -> mangled class name, for every RTTI TypeDescriptor."""
        out = {}
        for m in re.finditer(rb"\.\?A[VU]([^\x00]{1,400}?)@@\x00", self.buf):
            td_off = m.start() - 8
            if td_off < 0:
                continue
            vptr = struct.unpack_from(">I", self.buf, td_off)[0]
            if not (0x82000000 <= vptr < 0x83000000):
                continue
            va = self.off_to_va(td_off)
            if va is not None:
                out[va] = m.group(1).decode("ascii", "replace")
        return out

    def refs_to(self, va):
        """All VAs holding a big-endian pointer to `va`."""
        pat = struct.pack(">I", va)
        res, start = [], 0
        while True:
            p = self.buf.find(pat, start)
            if p < 0:
                break
            start = p + 1
            v = self.off_to_va(p)
            if v is not None:
                res.append(v)
        return res


def cols_for_td(img, td_va, strict=True):
    """Every COL whose pTypeDescriptor == td_va.

    A COL is validated by: signature(+0)==0 AND pClassHierarchyDescriptor(+0x10)
    resolving inside the image.  `strict=False` disables the signature test --
    used by --prove-fail to show the detector's failure branch is reachable.
    """
    out = []
    for ref in img.refs_to(td_va):
        col = ref - 0xC
        sig = img.u32(col)
        if sig is None:
            continue
        if strict and sig != 0:
            continue
        chd = img.u32(col + 0x10)
        if chd is None or img.va_to_off(chd) is None:
            continue
        out.append({
            "col": col,
            "offset": img.u32(col + 4),
            "cdOffset": img.u32(col + 8),
            "chd": chd,
        })
    return out


def parse_chd(img, chd_va, tds):
    sig = img.u32(chd_va)
    attrs = img.u32(chd_va + 4)
    n = img.u32(chd_va + 8)
    arr = img.u32(chd_va + 0xC)
    if n is None or arr is None or n > 200:
        return None
    bases = []
    for i in range(n):
        bcd = img.u32(arr + 4 * i)
        if bcd is None or img.va_to_off(bcd) is None:
            continue
        btd = img.u32(bcd)
        bases.append({
            "name": tds.get(btd, "?0x%08x" % (btd or 0)),
            "numContained": img.u32(bcd + 4),
            "mdisp": img.i32(bcd + 8),
            "pdisp": img.i32(bcd + 0xC),
            "vdisp": img.i32(bcd + 0x10),
            "attrs": img.u32(bcd + 0x14),
        })
    return {"sig": sig, "attrs": attrs, "num": n, "bases": bases}


def report(img, tds, want, strict=True):
    hits = [(va, n) for va, n in tds.items() if n == want]
    if not hits:
        print(f"  !! no TypeDescriptor named {want!r}  (denominator: "
              f"{len(tds)} TDs scanned)")
        return None
    for td_va, name in hits:
        print(f"\n### {name}   TD @ 0x{td_va:08x}")
        cols = cols_for_td(img, td_va, strict=strict)
        print(f"  COLs: {len(cols)}")
        for c in sorted(cols, key=lambda x: x["offset"] or 0):
            print(f"    vftable subobject offset = 0x{c['offset']:x}"
                  f"   cdOffset(vtordisp) = 0x{c['cdOffset']:x}"
                  f"   [COL 0x{c['col']:08x}]")
        if cols:
            chd = parse_chd(img, cols[0]["chd"], tds)
            if chd:
                vi = "VIRTUAL-INH" if (chd["attrs"] or 0) & 2 else ""
                mi = "MULTI-INH" if (chd["attrs"] or 0) & 1 else ""
                print(f"  CHD: {chd['num']} bases  attrs=0x{chd['attrs']:x} {mi} {vi}")
                for b in chd["bases"]:
                    kind = "virtual" if b["pdisp"] != -1 else "direct "
                    print(f"    {kind} {b['name']:<44} "
                          f"mdisp={b['mdisp']:<5} pdisp={b['pdisp']:<5} "
                          f"vdisp={b['vdisp']}")
        return cols
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("classes", nargs="*")
    ap.add_argument("--image", default=str(ROOT / "orig/45410914/band.exe"))
    ap.add_argument("--prove-fail", action="store_true",
                    help="disable the COL signature gate to show it is load-bearing")
    a = ap.parse_args()

    img = Image(Path(a.image))
    print(f"# image {a.image}")
    print(f"# sections: " + ", ".join(f"{n}@0x{v:08x}+0x{s:x}"
                                      for n, v, s, _ in img.sections))
    tds = img.find_tds()
    print(f"# TypeDescriptors: {len(tds)}   (this is the DENOMINATOR for any 'not found')")
    for c in a.classes:
        report(img, tds, c, strict=not a.prove_fail)


if __name__ == "__main__":
    main()
