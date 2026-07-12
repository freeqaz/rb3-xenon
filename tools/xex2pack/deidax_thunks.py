#!/usr/bin/env python3
"""
Reverse idaxex's in-place import-thunk rewrite so a basefile dumped via `xex1tool -b`
becomes a RAW basefile (function thunks hold the packed {type=1|ordinal} value the
Xbox/Xenia loader expects, instead of idaxex's li r3,idx / li r4,ord stub).

idaxex read_imports() writes, for every type-1 (function) record at record_addr:
    [+0] = 0x38600000 | ModuleIndex   (li r3, ModuleIndex)
    [+4] = 0x38800000 | ordinal       (li r4, ordinal)
We detect those (word0 high byte == 0x38, opcode 0x38600000 form) and restore:
    [+0] = 0x00010000 | ordinal       (packed thunk: type=1 in byte0, ordinal in low16)
Variable (type-0) records are never mutated by idaxex, so they are left untouched.

We enumerate exact record addresses from the XEX import table rather than pattern-scan.
"""
import struct, sys

def be32(b, o): return struct.unpack('>I', b[o:o+4])[0]

def enumerate_function_thunks(xex):
    magic, mf, hs, rsv, so, hc = struct.unpack('>IIIIII', xex[:24])
    opts = {}
    o = 24
    for _ in range(hc):
        k, v = struct.unpack('>II', xex[o:o+8]); o += 8; opts[k] = v
    imp_off = opts[0x000103FF]
    size_of_header = be32(xex, imp_off)
    size_of_strtab = be32(xex, imp_off + 4)
    num_imports    = be32(xex, imp_off + 8)
    # libraries begin after header(12) + string table
    lib = imp_off + 12 + size_of_strtab
    thunks = []  # list of (record_va, module_index)
    for li in range(num_imports):
        size = be32(xex, lib)
        # Xex2ImportLibrary: size(4) digest(20) id(4) ver(4) minver(4) name(2) count(2) = 40
        count = struct.unpack('>H', xex[lib+38:lib+40])[0]
        desc = lib + 40
        for i in range(count):
            va = be32(xex, desc + i*4)
            thunks.append((va, li))
        lib = desc + count*4
    return thunks

def main():
    if len(sys.argv) != 4:
        print("usage: deidax_thunks.py <src.xex> <mutated_basefile> <out_raw_basefile>")
        sys.exit(2)
    src = open(sys.argv[1], 'rb').read()
    base = bytearray(open(sys.argv[2], 'rb').read())
    load_addr = 0x84000000
    thunks = enumerate_function_thunks(src)
    fixed = 0
    for va, modidx in thunks:
        off = va - load_addr
        if off < 0 or off+8 > len(base):
            continue
        w0 = struct.unpack('>I', base[off:off+4])[0]
        w1 = struct.unpack('>I', base[off+4:off+8])[0]
        # idaxex stub signature: li r3, modidx (0x38600000|idx) ; li r4, ord (0x38800000|ord)
        if (w0 & 0xFFFF0000) == 0x38600000 and (w1 & 0xFFFF0000) == 0x38800000:
            ordinal = w1 & 0xFFFF
            packed = 0x00010000 | (ordinal & 0xFFFF)  # type byte = 0x00? -> see note
            # Packed thunk layout per Xex2ThunkData: ordinal:16 | hint:8 | type:8
            #   value = (type<<24) | (hint<<16) | ordinal ; type=1 -> 0x01000000|ordinal
            packed = 0x01000000 | (ordinal & 0xFFFF)
            struct.pack_into('>I', base, off, packed)
            fixed += 1
    open(sys.argv[3], 'wb').write(base)
    print(f"restored {fixed} function thunks -> {sys.argv[3]}")

if __name__ == '__main__':
    main()
