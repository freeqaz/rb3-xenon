#!/usr/bin/env python3
"""Parse an RB3Enhanced crash dump (crash_YYYYMMDD_HHMMSS.exc) from Xbox 360.

Format (big-endian, sequential; writer = RB3Enhanced source/xbox360_exceptions.c,
structs = include/exceptions.h):

  rb3e_exception_header {
      u32  magic;            // 0x33455858 '3EXX' (Xbox)
      u32  version;          // 0
      char rb3e_buildtag[48];
      char rb3e_commit[48];
      u16  num_stackwalk;    // written as 0 at open, real value on clean close
      u16  num_memchunks;    //   "     (0/0 => dump was cut short mid-crash)
  }                          // = 108 bytes
  EXCEPTION_RECORD (32-bit, 80 bytes):
      u32 ExceptionCode, ExceptionFlags, pExceptionRecord, ExceptionAddress,
      u32 NumberParameters, u32 ExceptionInformation[15]
  CONTEXT (PPC, EXCEPTION_CONTEXT_SIZE = 560 bytes; XDK layout:
      u32 ContextFlags, Msr, Iar, Lr; u64 Ctr; u64 Gpr[32];
      u32 Cr, Xer; f64 Fpscr; f64 Fpr[32]; 8 pad)
  u32 stackwalk[num_stackwalk]      // LR return addresses, innermost first
  memchunks * num_memchunks:
      { u32 address; u32 length; u8 data[length]; }

If the trailing-count rewrite never happened (crash during dump), counts are
0/0 and we fall back to a heuristic scan: DWORDs that look like code addresses
(0x8xxxxxxx/0x9xxxxxxx) are stackwalk entries until the first plausible
memchunk header.
"""
import struct
import sys

HDR_FMT = ">II48s48sHH"
HDR_SIZE = struct.calcsize(HDR_FMT)  # 108
EXCREC_FMT = ">IIIII15I"
EXCREC_SIZE = struct.calcsize(EXCREC_FMT)  # 80
CTX_SIZE = 560

EXC_CODES = {
    0xC0000005: "ACCESS_VIOLATION",
    0xC000001D: "ILLEGAL_INSTRUCTION",
    0xC0000094: "INTEGER_DIVIDE_BY_ZERO",
    0xC00000FD: "STACK_OVERFLOW",
    0xC0000025: "NONCONTINUABLE_EXCEPTION",
    0x80000003: "BREAKPOINT",
    0x80000002: "DATATYPE_MISALIGNMENT",
}


def cstr(b):
    return b.split(b"\0", 1)[0].decode("ascii", "replace")


def parse_context(raw):
    ctx = {}
    (ctx["ContextFlags"], ctx["Msr"], ctx["Iar"], ctx["Lr"]) = struct.unpack_from(">IIII", raw, 0)
    (ctx["Ctr"],) = struct.unpack_from(">Q", raw, 16)
    ctx["Gpr"] = list(struct.unpack_from(">32Q", raw, 24))
    (ctx["Cr"], ctx["Xer"]) = struct.unpack_from(">II", raw, 280)
    (ctx["Fpscr"],) = struct.unpack_from(">d", raw, 288)
    ctx["Fpr"] = list(struct.unpack_from(">32d", raw, 296))
    return ctx


def looks_like_code(addr):
    return (addr & 0xF0000000) in (0x80000000, 0x90000000)


def main(path):
    data = open(path, "rb").read()
    magic, version, buildtag, commit, n_stack, n_chunks = struct.unpack_from(HDR_FMT, data, 0)
    if magic != 0x33455858:
        sys.exit(f"bad magic 0x{magic:08X} (want 0x33455858 '3EXX') — not an Xbox RB3E dump")
    print(f"RB3Enhanced Xbox crash dump  v{version}")
    print(f"  build : {cstr(buildtag)}")
    print(f"  commit: {cstr(commit)}")
    truncated = n_stack == 0 and n_chunks == 0
    if truncated:
        print("  NOTE  : header counts are 0/0 — dump was cut short mid-write; using heuristic scan")

    off = HDR_SIZE
    rec = struct.unpack_from(EXCREC_FMT, data, off)
    code, flags, _chain, addr, nparams = rec[:5]
    info = rec[5:5 + min(nparams, 15)]
    print(f"\nEXCEPTION {EXC_CODES.get(code, 'code')} (0x{code:08X})  at 0x{addr:08X}  flags=0x{flags:X}")
    if code == 0xC0000005 and nparams >= 2:
        kind = {0: "READ", 1: "WRITE", 8: "EXECUTE"}.get(info[0], f"op={info[0]}")
        print(f"  access violation: {kind} of address 0x{info[1]:08X}")
    elif info:
        print(f"  params: {' '.join(f'0x{p:08X}' for p in info)}")

    off += EXCREC_SIZE
    ctx = parse_context(data[off:off + CTX_SIZE])
    print(f"\nCONTEXT  Iar(PC)=0x{ctx['Iar']:08X}  Lr=0x{ctx['Lr']:08X}  "
          f"Ctr=0x{ctx['Ctr']:016X}  Msr=0x{ctx['Msr']:08X}  Cr=0x{ctx['Cr']:08X}  Xer=0x{ctx['Xer']:08X}")
    for i in range(0, 32, 4):
        print("  " + "  ".join(f"r{j:<2}=0x{ctx['Gpr'][j] & 0xFFFFFFFF:08X}" for j in range(i, i + 4)))

    off += CTX_SIZE
    if truncated:
        # everything left was written sequentially; recover what we can
        stack = []
        while off + 4 <= len(data):
            (w,) = struct.unpack_from(">I", data, off)
            if not looks_like_code(w):
                break
            stack.append(w)
            off += 4
    else:
        stack = list(struct.unpack_from(f">{n_stack}I", data, off))
        off += 4 * n_stack
    print(f"\nSTACK WALK ({len(stack)} return addresses, innermost first):")
    for i, ra in enumerate(stack):
        print(f"  #{i:<2} 0x{ra:08X}")

    print(f"\nMEMORY CHUNKS:")
    count = 0
    while off + 8 <= len(data) and (truncated or count < n_chunks):
        caddr, clen = struct.unpack_from(">II", data, off)
        if clen == 0 or clen > 0x100000 or off + 8 + clen > len(data) + 4096:
            print(f"  (stopping: implausible chunk header addr=0x{caddr:08X} len=0x{clen:X} @file+0x{off:X})")
            break
        avail = min(clen, len(data) - off - 8)
        print(f"  0x{caddr:08X}  len=0x{clen:X}" + ("" if avail == clen else f"  (TRUNCATED, {avail} bytes present)"))
        off += 8 + avail
        count += 1
    print(f"\n({count} chunks; file size {len(data)} bytes, consumed {off})")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("usage: parse_rb3e_exc.py crash_YYYYMMDD_HHMMSS.exc")
    main(sys.argv[1])
