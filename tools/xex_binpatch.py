#!/usr/bin/env python3
"""XEX2 flat-image binary patcher for the RB3 "Same Instrument" static code-cave patch.

The target default.xex (title 45410914) is an UNCOMPRESSED, UNENCRYPTED XEX2 whose
PE basefile is stored as a flat loaded image beginning at file offset pe_data_offset
(== 0x3000). VA -> file mapping is therefore DIRECT:

    file_offset = pe_data_offset + (VA - 0x82000000)

The patch (patches/45410914_same_instrument_full.patch.toml) is 675 big-endian 32-bit
writes: ~670 words that relocate SameInstrumentHooks.obj into the code cave at guest
0x82C25000, one enable-flag word gSameInstrumentEnabled=1 @ 0x82C25AA0, and 4 detour
`b <cave>` branches installed at the hooked function entries.

Subcommands:
    apply  --xex ORIG --toml TOML --out OUT [--extra-write VA=WORD ...] [--report JSON]
    verify --xex PATCHED [--orig ORIG] [--toml TOML] [--cave-bin BIN] [--report JSON]

Never mutates the input --xex; apply copies orig -> out. Refuses to write if any
detour site does not currently read 7D8802A6 (mflr r12) or any cave word is non-zero.
"""
import argparse
import hashlib
import json
import re
import struct
import sys

# ---- try modern stdlib tomllib, fall back to tomli (py<3.11) ----
try:
    import tomllib as _toml
except ModuleNotFoundError:  # py3.10
    import tomli as _toml

XEX_BASE = 0x82000000
CAVE_LO = 0x82C25000
CAVE_HI = 0x82C25AF0            # exclusive; cave.bin is 0xAF0 bytes
FLAG_VA = 0x82C25AA0            # gSameInstrumentEnabled
FLAG_BIN_OFF = 0xAA0           # word in cave.bin that is 0 in the blob but 1 in the toml
DETOURS = {                    # site VA -> expected branch target VA
    0x8264B5F8: 0x82C25080,
    0x8259D948: 0x82C250C0,
    0x8274ACF8: 0x82C258B0,
    0x8276FBB0: 0x82C25590,
}
MFLR_R12 = 0x7D8802A6          # pre-image expected at every detour site


def sha256(b):
    return hashlib.sha256(b).hexdigest()


def load_writes(toml_path):
    """Return list of (address, value) plus a sanity cross-check against a regex count."""
    with open(toml_path, "rb") as f:
        doc = _toml.load(f)
    writes = doc["patch"][0]["be32"]
    text = open(toml_path, "r", encoding="utf-8", errors="replace").read()
    regex_n = len(re.findall(r"address\s*=\s*(0x[0-9A-Fa-f]+)", text))
    if regex_n != len(writes):
        sys.exit(f"FATAL: tomllib nesting mismatch: parsed {len(writes)} writes "
                 f"but regex found {regex_n} 'address =' lines")
    return [(int(w["address"]), int(w["value"]) & 0xFFFFFFFF) for w in writes], regex_n


def read_pe_data_offset(buf):
    if buf[:4] != b"XEX2":
        sys.exit("FATAL: not an XEX2 file (bad magic)")
    return struct.unpack_from(">I", buf, 8)[0]


def va_to_off(va, pe_data_off, filesize):
    off = pe_data_off + (va - XEX_BASE)
    if not (0 <= off and off + 4 <= filesize):
        sys.exit(f"FATAL: VA {va:#010x} -> off {off:#x} out of file bounds (size {filesize:#x})")
    return off


def rd32(buf, off):
    return struct.unpack_from(">I", buf, off)[0]


def classify(va):
    if va in DETOURS:
        return "detour"
    if va == FLAG_VA:
        return "flag"
    if CAVE_LO <= va < CAVE_HI:
        return "cave"
    return "extra"


# ----------------------------------------------------------------------------
def cmd_apply(args):
    orig = open(args.xex, "rb").read()
    filesize = len(orig)
    pe_data_off = read_pe_data_offset(orig)
    if pe_data_off != 0x3000:
        sys.exit(f"FATAL: pe_data_offset {pe_data_off:#x} != 0x3000 (formula assumption broken)")

    writes, _ = load_writes(args.toml)

    # merge --extra-write entries
    extra = []
    for spec in (args.extra_write or []):
        va_s, val_s = spec.split("=")
        extra.append((int(va_s, 16), int(val_s, 16) & 0xFFFFFFFF))
    all_writes = writes + extra

    # --- validation pass: ALL checks before mutating anything ---
    seen = {}
    counts = {"cave": 0, "flag": 0, "detour": 0, "extra": 0}
    for va, val in all_writes:
        if va % 4 != 0:
            sys.exit(f"FATAL: unaligned address {va:#010x}")
        if not (XEX_BASE <= va < XEX_BASE + (filesize - pe_data_off)):
            sys.exit(f"FATAL: address {va:#010x} outside image VA range")
        off = va_to_off(va, pe_data_off, filesize)
        if va in seen:
            sys.exit(f"FATAL: duplicate write address {va:#010x}")
        seen[va] = off
        cls = classify(va)
        pre = rd32(orig, off)
        if cls == "detour" and pre != MFLR_R12:
            sys.exit(f"FATAL: detour site {va:#010x} pre-image {pre:08X} != 7D8802A6 "
                     f"(wrong XEX or already patched)")
        if cls in ("cave", "flag") and pre != 0:
            sys.exit(f"FATAL: cave/flag site {va:#010x} pre-image {pre:08X} != 0 "
                     f"(cave not free / already patched)")
        if cls == "extra":
            # entry detour must read mflr; cave-payload slots must read 0
            if va in DETOURS or va == 0x82816080:
                if pre != MFLR_R12:
                    sys.exit(f"FATAL: extra detour {va:#010x} pre {pre:08X} != 7D8802A6")
            elif CAVE_LO <= va < CAVE_HI + 0x400:  # allow spike marker slots just above blob
                if pre != 0:
                    sys.exit(f"FATAL: extra cave slot {va:#010x} pre {pre:08X} != 0")
        counts[cls] += 1

    # verify detour displacement math matches the known targets
    for va, val in all_writes:
        if va in DETOURS and classify(va) == "detour":
            if (val & 0xFC000003) != 0x48000000:
                sys.exit(f"FATAL: detour word {val:08X} at {va:#010x} not a plain b")
            disp = val & 0x03FFFFFC
            if disp & 0x02000000:
                disp -= 0x04000000
            tgt = (va + disp) & 0xFFFFFFFF
            if tgt != DETOURS[va]:
                sys.exit(f"FATAL: detour {va:#010x} -> {tgt:#010x} != expected {DETOURS[va]:#010x}")

    # --- mutate a copy ---
    buf = bytearray(orig)
    audit = []
    for va, val in all_writes:
        off = seen[va]
        old = rd32(buf, off)
        struct.pack_into(">I", buf, off, val)
        audit.append({"va": f"0x{va:08X}", "file_off": f"0x{off:06X}",
                      "old": f"0x{old:08X}", "new": f"0x{val:08X}", "class": classify(va)})

    with open(args.out, "wb") as f:
        f.write(buf)

    report = {
        "action": "apply",
        "orig": args.xex,
        "out": args.out,
        "toml": args.toml,
        "filesize": filesize,
        "pe_data_offset": pe_data_off,
        "writes_total": len(all_writes),
        "writes_from_toml": len(writes),
        "extra_writes": len(extra),
        "counts": counts,
        "sha256_orig": sha256(orig),
        "sha256_out": sha256(bytes(buf)),
    }
    print(json.dumps(report, indent=2))
    if args.report:
        json.dump(report, open(args.report, "w"), indent=2)
    # sidecar audit next to out (unless suppressed)
    if args.writes_json:
        json.dump(audit, open(args.writes_json, "w"), indent=2)
        print(f"[wrote sidecar {args.writes_json} ({len(audit)} writes)]")
    return 0


# ----------------------------------------------------------------------------
def cmd_verify(args):
    patched = open(args.xex, "rb").read()
    pe_data_off = read_pe_data_offset(patched)
    filesize = len(patched)
    writes, _ = load_writes(args.toml)
    results = []

    def add(name, ok, detail=""):
        results.append((name, ok, detail))

    # (1) all 675 words re-read == toml values
    bad = 0
    for va, val in writes:
        off = va_to_off(va, pe_data_off, filesize)
        got = rd32(patched, off)
        if got != val:
            bad += 1
            if bad <= 5:
                add(f"word {va:#010x}", False, f"got {got:08X} want {val:08X}")
    add("all_675_words_match", bad == 0, f"{len(writes)-bad}/{len(writes)} match")

    # (2) detour decode
    for va, want_tgt in DETOURS.items():
        off = va_to_off(va, pe_data_off, filesize)
        w = rd32(patched, off)
        ok_op = (w & 0xFC000003) == 0x48000000
        disp = w & 0x03FFFFFC
        if disp & 0x02000000:
            disp -= 0x04000000
        tgt = (va + disp) & 0xFFFFFFFF
        add(f"detour {va:#010x} -> b {want_tgt:#010x}",
            ok_op and tgt == want_tgt, f"word={w:08X} tgt={tgt:#010x}")

    # (3) cave region equality vs cave.bin (special-case flag word at 0xAA0)
    if args.cave_bin:
        cave = open(args.cave_bin, "rb").read()
        base_off = va_to_off(CAVE_LO, pe_data_off, filesize)
        region = patched[base_off:base_off + len(cave)]
        mism = []
        for i in range(0, len(cave), 4):
            fw = struct.unpack_from(">I", region, i)[0]
            bw = struct.unpack_from(">I", cave, i)[0]
            if i == FLAG_BIN_OFF:
                if fw != 1:
                    mism.append((i, fw, 1, "flag"))
            elif fw != bw:
                mism.append((i, fw, bw, ""))
        add("cave_region==cave.bin (flag word=1 exception)",
            not mism, f"{len(cave)//4} words; {len(mism)} mismatches"
            + ("" if not mism else f" first={mism[0]}"))

    # (4) whole-file diff orig vs patched -> changed words == exactly the write set
    if args.orig:
        orig = open(args.orig, "rb").read()
        changed = set()
        if len(orig) != len(patched):
            add("whole_file_diff", False, f"size differs {len(orig)} vs {len(patched)}")
        else:
            # scan 4-byte aligned words in the PE image region
            for off in range(pe_data_off, filesize - 3, 4):
                if patched[off:off+4] != orig[off:off+4]:
                    changed.add(off)
            # any change in the XEX header (< pe_data_offset) would be destructive
            header_changed = any(orig[i] != patched[i] for i in range(pe_data_off))
            expect = {va_to_off(va, pe_data_off, filesize): val for va, val in writes}
            extra_changed = changed - set(expect)
            # writes that produced no byte change must be exactly the ones whose new
            # value already equals the original word (zero-over-zero bss words, etc.)
            missing = set(expect) - changed
            noop_ok = all(rd32(orig, off) == expect[off] for off in missing)
            add("whole_file_diff exact (changes subset writes, no extra/header)",
                not extra_changed and not header_changed and noop_ok,
                f"changed_words={len(changed)} expect={len(expect)} extra={len(extra_changed)} "
                f"noop_writes={len(missing)}(all_value==orig={noop_ok}) header_changed={header_changed}")

    # (5) disasm round-trip via capstone (flat offsets)
    try:
        import capstone
        md = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_BIG_ENDIAN | capstone.CS_MODE_32)
        # detour sites
        for va, want_tgt in DETOURS.items():
            off = va_to_off(va, pe_data_off, filesize)
            ins = next(md.disasm(patched[off:off+4], va), None)
            txt = f"{ins.mnemonic} {ins.op_str}" if ins else "<none>"
            ok = ins is not None and ins.mnemonic == "b" and hex(want_tgt).lstrip("0x") in ins.op_str.replace("0x", "")
            add(f"disasm site {va:#010x}", ok, txt)
        # cave entrypoints: first insn of each hooked-body / trampoline region
        for va in (0x82C25080, 0x82C250C0, 0x82C258B0, 0x82C25590):
            off = va_to_off(va, pe_data_off, filesize)
            lines = []
            for ins in md.disasm(patched[off:off+12], va):
                lines.append(f"{ins.mnemonic} {ins.op_str}")
            add(f"disasm cave {va:#010x}", len(lines) >= 1, " | ".join(lines))
        # the last trampoline at 0x82C25AD0 area: mflr r12 ; b back
        for va in (0x82C25AD0,):
            off = va_to_off(va, pe_data_off, filesize)
            lines = [f"{ins.mnemonic} {ins.op_str}" for ins in md.disasm(patched[off:off+8], va)]
            add(f"disasm trampoline {va:#010x}", len(lines) >= 1, " | ".join(lines))
    except ImportError:
        add("disasm_roundtrip", False, "capstone not installed (skipped)")

    # print table
    allok = all(ok for _, ok, _ in results)
    print(f"\n{'PASS' if allok else 'FAIL'}  verify {args.xex}")
    print("-" * 100)
    for name, ok, detail in results:
        print(f"  [{'PASS' if ok else 'FAIL'}] {name:52s} {detail}")
    print("-" * 100)
    print(f"sha256(patched) = {sha256(patched)}")

    if args.report:
        json.dump({"action": "verify", "xex": args.xex, "all_pass": allok,
                   "sha256": sha256(patched),
                   "results": [{"check": n, "pass": ok, "detail": d} for n, ok, d in results]},
                  open(args.report, "w"), indent=2)
    return 0 if allok else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    a = sub.add_parser("apply")
    a.add_argument("--xex", required=True)
    a.add_argument("--toml", required=True)
    a.add_argument("--out", required=True)
    a.add_argument("--extra-write", action="append", default=[], metavar="VA=WORD")
    a.add_argument("--report")
    a.add_argument("--writes-json", help="path for {va,file_off,old,new} audit sidecar")
    a.set_defaults(func=cmd_apply)

    v = sub.add_parser("verify")
    v.add_argument("--xex", required=True)
    v.add_argument("--orig")
    v.add_argument("--toml", required=True)
    v.add_argument("--cave-bin")
    v.add_argument("--report")
    v.set_defaults(func=cmd_verify)

    args = ap.parse_args()
    sys.exit(args.func(args))


if __name__ == "__main__":
    main()
