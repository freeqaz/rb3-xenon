#!/usr/bin/env python3
"""Decode the MSVC "Rich" header of a PE file (and MSVC .obj @comp.id records).

The Rich header sits between the DOS stub and the PE signature.  It is a list of
(product-id, build-number, use-count) triples -- one per distinct tool that
contributed input to the link -- XOR-encrypted with a 4-byte key that is itself
a checksum over the DOS header and the records.  It is the single most direct
answer to "which exact compiler build produced this binary".

Usage:
    rich_header.py <pe-file> [<pe-file> ...]     # decode Rich headers
    rich_header.py --compid <obj-file> ...       # decode @comp.id in COFF objs

The checksum is recomputed and reported; a PASS is strong evidence the header is
genuine linker output rather than something a rebuild/extraction tool fabricated.
(Control: flipping a single bit anywhere in the DOS stub or in any record turns
the PASS into a FAIL, so the check is sensitive, not vacuous.)

--------------------------------------------------------------------------
CALIBRATED RESULTS (lane CA-1, 2026-07-30) -- the X360 compiler build per binary
--------------------------------------------------------------------------
  binary                        PE timestamp   XDK (xam/xboxkrnl)  cl build
  RB3 TU0  band.exe             2010-08-07     2.0.11164.0            10224
  RB3 TU5  band.exe  (target)   2011-09-02     2.0.11164.0            10224
  DC3      ham_xbox_r.exe       2012-09-16     2.0.21173.0            11886
  our toolchain (X360/16.00.11886.00, host binaries linked 2010-09-02)  11886

  => RETAIL RB3 WAS BUILT WITH X360 cl.exe BUILD 10224; WE BUILD WITH 11886.
     DC3's retail build number equals our toolchain exactly, which is what
     validates this method end-to-end (and validates dc3-decomp's compiler pick).
     Harmonix froze the toolchain across the whole TU lifecycle: TU0 (launch)
     and TU5 (13 months later) both report 10224.

  Product id 0x00AB is the X360 C++ compiler and 0x00AA the X360 C compiler.
  That is established empirically -- our own freshly compiled .obj carries
  `@comp.id = 0x00AB2E6E` (prodid 0x00AB, build 11886 = 0x2E6E) -- NOT from the
  PRODUCT_IDS table below, whose names are desktop-MSVC names and are WRONG for
  the XDK branch.  In particular the table labels 0x00AA/0x00AB as "POGO"
  (profile-guided) entries; retail used no PGO, and a plain /O1 compile here
  stamps 0x00AB.  ==> TRUST THE BUILD NUMBERS, NOT THE NAMES.
"""

from __future__ import annotations

import struct
import sys

# Product-id -> tool name.  Sourced from the public @comp.id tables (Pistelli /
# hasherezade / dishather).  Treat as ADVISORY: the Xbox 360 XDK toolchain is a
# Microsoft-internal branch and there is no guarantee it reuses the desktop
# enumeration.  Always cross-calibrate against a binary of known toolchain.
PRODUCT_IDS = {
    0x0000: "Unknown",
    0x0001: "Import0",
    0x0002: "Linker510",
    0x0003: "Cvtomf510",
    0x0004: "Linker600",
    0x0005: "Cvtomf600",
    0x0006: "Cvtres500",
    0x0007: "Utc11_Basic",
    0x0008: "Utc11_C",
    0x0009: "Utc12_Basic",
    0x000A: "Utc12_C",
    0x000B: "Utc12_CPP",
    0x000C: "AliasObj60",
    0x000D: "VisualBasic60",
    0x000E: "Masm613",
    0x000F: "Masm710",
    0x0010: "Linker511",
    0x0011: "Cvtomf511",
    0x0012: "Masm614",
    0x0013: "Linker512",
    0x0014: "Cvtomf512",
    0x0015: "Utc12_C_Std",
    0x0016: "Utc12_CPP_Std",
    0x0017: "Utc12_C_Book",
    0x0018: "Utc12_CPP_Book",
    0x0019: "Implib700",
    0x001A: "Cvtomf700",
    0x001B: "Utc13_Basic",
    0x001C: "Utc13_C",
    0x001D: "Utc13_CPP",
    0x001E: "Linker610",
    0x001F: "Cvtomf610",
    0x0020: "Linker601",
    0x0021: "Cvtomf601",
    0x0022: "Utc12_2_Basic",
    0x0023: "Utc12_2_C",
    0x0024: "Utc12_2_CPP",
    0x0025: "Utc12_2_C_Std",
    0x0026: "Utc12_2_CPP_Std",
    0x0027: "Utc12_2_C_Book",
    0x0028: "Utc12_2_CPP_Book",
    0x0029: "Implib622",
    0x002A: "Cvtomf622",
    0x002B: "Cvtres501",
    0x002C: "Utc13_C_Std",
    0x002D: "Utc13_CPP_Std",
    0x002E: "Cvtpgd1300",
    0x002F: "Linker620",
    0x0030: "Cvtomf620",
    0x0031: "AliasObj70",
    0x0032: "Linker621",
    0x0033: "Cvtomf621",
    0x0034: "Masm615",
    0x0035: "Utc13_LTCG_C",
    0x0036: "Utc13_LTCG_CPP",
    0x0037: "Masm620",
    0x0038: "ILAsm100",
    0x0039: "Utc12_2_LTCG_C",
    0x003A: "Utc12_2_LTCG_CPP",
    0x003B: "Masm630",
    0x003C: "Utc13_1_Basic",
    0x003D: "Utc13_1_C",
    0x003E: "Utc13_1_CPP",
    0x003F: "Linker622",
    0x0040: "Linker700",
    0x0041: "Export622",
    0x0042: "Export700",
    0x0043: "Masm700",
    0x0044: "Utc13_1_LTCG_C",
    0x0045: "Utc13_1_LTCG_CPP",
    0x0046: "Cvtpgd1310",
    0x0047: "Linker710",
    0x0048: "Cvtomf710",
    0x0049: "Export710",
    0x004A: "Implib710",
    0x004B: "Cvtres700",
    0x004C: "Cvtres710p",
    0x004D: "Linker710p",
    0x004E: "Cvtomf710p",
    0x004F: "Export710p",
    0x0050: "Implib710p",
    0x0051: "Masm710p",
    0x0052: "Utc1400_C",
    0x0053: "Utc1400_CPP",
    0x0054: "Utc1400_C_Std",
    0x0055: "Utc1400_CPP_Std",
    0x0056: "Utc1400_LTCG_C",
    0x0057: "Utc1400_LTCG_CPP",
    0x0058: "Utc1400_POGO_I_C",
    0x0059: "Utc1400_POGO_I_CPP",
    0x005A: "Utc1400_POGO_O_C",
    0x005B: "Utc1400_POGO_O_CPP",
    0x005C: "Cvtpgd1400",
    0x005D: "Linker800",
    0x005E: "Cvtomf800",
    0x005F: "Export800",
    0x0060: "Implib800",
    0x0061: "Cvtres800",
    0x0062: "Masm800",
    0x0063: "AliasObj800",
    0x0064: "PhoenixPrerelease",
    0x0065: "Utc1400_CVTCIL_C",
    0x0066: "Utc1400_CVTCIL_CPP",
    0x0067: "Utc1400_LTCG_MSIL",
    0x0068: "Utc1500_C",
    0x0069: "Utc1500_CPP",
    0x006A: "Utc1500_C_Std",
    0x006B: "Utc1500_CPP_Std",
    0x006C: "Utc1500_CVTCIL_C",
    0x006D: "Utc1500_CVTCIL_CPP",
    0x006E: "Utc1500_LTCG_C",
    0x006F: "Utc1500_LTCG_CPP",
    0x0070: "Utc1500_LTCG_MSIL",
    0x0071: "Utc1500_POGO_I_C",
    0x0072: "Utc1500_POGO_I_CPP",
    0x0073: "Utc1500_POGO_O_C",
    0x0074: "Utc1500_POGO_O_CPP",
    0x0075: "Cvtpgd1500",
    0x0076: "Linker900",
    0x0077: "Export900",
    0x0078: "Implib900",
    0x0079: "Cvtres900",
    0x007A: "Masm900",
    0x007B: "AliasObj900",
    0x007C: "Resource900",
    0x007D: "AliasObj1000",
    0x007E: "Cvtpgd1600",
    0x007F: "Cvtres1000",
    0x0080: "Export1000",
    0x0081: "Implib1000",
    0x0082: "Linker1000",
    0x0083: "Masm1000",
    0x0084: "Phx1600_C",
    0x0085: "Phx1600_CPP",
    0x0086: "Phx1600_CVTCIL_C",
    0x0087: "Phx1600_CVTCIL_CPP",
    0x0088: "Phx1600_LTCG_C",
    0x0089: "Phx1600_LTCG_CPP",
    0x008A: "Phx1600_LTCG_MSIL",
    0x008B: "Phx1600_POGO_I_C",
    0x008C: "Phx1600_POGO_I_CPP",
    0x008D: "Phx1600_POGO_O_C",
    0x008E: "Phx1600_POGO_O_CPP",
    0x008F: "Utc1600_C",
    0x0090: "Utc1600_CPP",
    0x0091: "Utc1600_CVTCIL_C",
    0x0092: "Utc1600_CVTCIL_CPP",
    0x0093: "Utc1600_LTCG_C",
    0x0094: "Utc1600_LTCG_CPP",
    0x0095: "Utc1600_LTCG_MSIL",
    0x0096: "Utc1600_POGO_I_C",
    0x0097: "Utc1600_POGO_I_CPP",
    0x0098: "Utc1600_POGO_O_C",
    0x0099: "Utc1600_POGO_O_CPP",
    0x009A: "AliasObj1010",
    0x009B: "Cvtpgd1610",
    0x009C: "Cvtres1010",
    0x009D: "Export1010",
    0x009E: "Implib1010",
    0x009F: "Linker1010",
    0x00A0: "Masm1010",
    0x00A1: "Utc1700_C",
    0x00A2: "Utc1700_CPP",
    0x00A3: "Utc1700_CVTCIL_C",
    0x00A4: "Utc1700_CVTCIL_CPP",
    0x00A5: "Utc1700_LTCG_C",
    0x00A6: "Utc1700_LTCG_CPP",
    0x00A7: "Utc1700_LTCG_MSIL",
    0x00A8: "Utc1700_POGO_I_C",
    0x00A9: "Utc1700_POGO_I_CPP",
    0x00AA: "Utc1700_POGO_O_C",
    0x00AB: "Utc1700_POGO_O_CPP",
}


def rol32(v: int, n: int) -> int:
    n &= 31
    return ((v << n) | (v >> (32 - n))) & 0xFFFFFFFF


def parse_rich(data: bytes):
    """Return (start_off, key, [(prodid, build, count)], checksum_ok) or None."""
    if data[:2] != b"MZ":
        return None
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    window = data[:e_lfanew] if 0 < e_lfanew < len(data) else data[:0x1000]
    idx = window.rfind(b"Rich")
    if idx < 0:
        return None
    key = struct.unpack_from("<I", data, idx + 4)[0]
    kb = struct.pack("<I", key)

    # Walk back to the XOR-encrypted "DanS"
    dans = None
    off = idx - 4
    while off >= 0x40:
        if bytes(a ^ b for a, b in zip(data[off:off + 4], kb)) == b"DanS":
            dans = off
            break
        off -= 4
    if dans is None:
        return None

    records = []
    p = dans + 16  # DanS + 3 zero padding dwords
    while p + 8 <= idx:
        compid = struct.unpack_from("<I", data, p)[0] ^ key
        count = struct.unpack_from("<I", data, p + 4)[0] ^ key
        records.append((compid >> 16, compid & 0xFFFF, count))
        p += 8

    # Recompute the checksum: it is the key.
    csum = dans
    for i in range(dans):
        if 0x3C <= i < 0x40:      # e_lfanew is excluded
            continue
        csum = (csum + rol32(data[i], i)) & 0xFFFFFFFF
    for prodid, build, count in records:
        csum = (csum + rol32((prodid << 16) | build, count)) & 0xFFFFFFFF
    return dans, key, records, (csum == key)


def pe_info(data: bytes) -> dict:
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    if data[e_lfanew:e_lfanew + 4] != b"PE\0\0":
        return {}
    machine, nsec = struct.unpack_from("<HH", data, e_lfanew + 4)
    tds = struct.unpack_from("<I", data, e_lfanew + 8)[0]
    opt = e_lfanew + 24
    magic = struct.unpack_from("<H", data, opt)[0]
    major, minor = data[opt + 2], data[opt + 3]
    return {
        "machine": machine, "sections": nsec, "timestamp": tds,
        "opt_magic": magic, "linker_major": major, "linker_minor": minor,
    }


MACHINE = {0x01F2: "POWERPCBE (Xbox 360)", 0x01F0: "POWERPC",
           0x014C: "I386", 0x8664: "AMD64"}


def report_pe(path: str) -> None:
    with open(path, "rb") as fh:
        data = fh.read()
    print(f"=== {path}  ({len(data)} bytes) ===")
    info = pe_info(data)
    if info:
        import datetime
        ts = datetime.datetime.fromtimestamp(info["timestamp"], datetime.timezone.utc)
        print(f"  machine      : 0x{info['machine']:04X} "
              f"{MACHINE.get(info['machine'], '?')}")
        print(f"  timestamp    : 0x{info['timestamp']:08X}  {ts:%Y-%m-%d %H:%M:%S} UTC")
        print(f"  linker ver   : {info['linker_major']}.{info['linker_minor']:02d}"
              "   (PE optional header, coarse)")
    r = parse_rich(data)
    if r is None:
        print("  RICH HEADER  : *** ABSENT / unparseable ***")
        return
    start, key, records, ok = r
    print(f"  RICH HEADER  : at 0x{start:X}, key 0x{key:08X}, "
          f"{len(records)} records, checksum "
          f"{'PASS (authentic linker output)' if ok else 'FAIL'}")
    print(f"    {'prodid':>7} {'build':>7} {'count':>7}  tool")
    tot = 0
    for prodid, build, count in records:
        tot += count
        print(f"    0x{prodid:04X}  {build:7d} {count:7d}  "
              f"{PRODUCT_IDS.get(prodid, '<unknown prodid>')}")
    print(f"    total object/use count: {tot}")


def report_compid(path: str) -> None:
    """Decode @comp.id / @feat.00 absolute symbols from a COFF object."""
    with open(path, "rb") as fh:
        data = fh.read()
    machine, nsec, tds, symtab, nsym = struct.unpack_from("<HHIII", data, 0)
    print(f"=== {path} (COFF, machine 0x{machine:04X} "
          f"{MACHINE.get(machine, '?')}, {nsym} symbols) ===")
    strtab = symtab + nsym * 18
    hits = 0
    for i in range(nsym):
        off = symtab + i * 18
        raw = data[off:off + 8]
        if raw[:4] == b"\0\0\0\0":
            soff = struct.unpack_from("<I", raw, 4)[0]
            end = data.index(b"\0", strtab + soff)
            name = data[strtab + soff:end].decode("ascii", "replace")
        else:
            name = raw.rstrip(b"\0").decode("ascii", "replace")
        if name in ("@comp.id", "@feat.00"):
            value = struct.unpack_from("<I", data, off + 8)[0]
            hits += 1
            if name == "@comp.id":
                print(f"  @comp.id = 0x{value:08X}  ->  prodid 0x{value >> 16:04X} "
                      f"({PRODUCT_IDS.get(value >> 16, '<unknown>')}), "
                      f"build {value & 0xFFFF}")
            else:
                print(f"  @feat.00 = 0x{value:08X}")
    if hits == 0:
        print(f"  (no @comp.id/@feat.00 among {nsym} symbols)")


def main() -> int:
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 2
    if args[0] == "--compid":
        for p in args[1:]:
            report_compid(p)
    else:
        for p in args:
            report_pe(p)
    return 0


if __name__ == "__main__":
    sys.exit(main())
