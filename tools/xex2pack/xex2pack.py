#!/usr/bin/env python3
"""
xex2pack - PE basefile -> unsigned/uncompressed/unencrypted XEX2-DLL packer.

Targets an RGH Xbox 360 / Xenia (unsigned boot). Emits:
  encryption = none, compression = none (raw), base = 0x84000000, DLL module flag,
  zeroed RSA signature + all hashes/digests (RGH/devkit loaders skip HV hash checks).

Struct field layout is taken verbatim from:
  - XenonRecomp/XenonUtils/xex.h        (Xex2Header / Xex2SecurityInfo / opt-header keys)
  - reverse-compiler-refs/idaxex/formats/xex_structs.hpp   (xex2::SecurityInfo == 0x184,
                                                             HvImageInfo == 0x174)
  - idaxex xex.cpp read_basefile_raw()  (COMPRESSION_NONE reads data_length_ bytes @ SizeOfHeaders)

The input "basefile" is the fully-mapped image (what `xex1tool -b` recovers): its length
== the XEX image size. We append it raw after the header region.

Import table: for the identity round-trip we copy the source XEX's IMPORT_LIBRARIES opt-header
block verbatim (known-good encoding). A from-scratch import block can also be supplied via
--import-block <file> (raw bytes) once Lane L's ordinal tables are wired.

Usage:
  xex2pack.py --pe basefile.pe --out out.xex --from-xex stock.xex
  xex2pack.py --pe basefile.pe --out out.xex --entry 0x8401B590 --base 0x84000000 \
              [--import-block imports.bin] [--pe-name RB3Enhanced.exe] [--compress basic|none]
"""
import argparse, hashlib, struct, sys

# ---- XEX2 opt-header keys (xex.h) ----
XEX_HEADER_FILE_FORMAT_INFO   = 0x000003FF
XEX_HEADER_ENTRY_POINT        = 0x00010100
XEX_HEADER_IMAGE_BASE_ADDRESS = 0x00010201
XEX_HEADER_IMPORT_LIBRARIES   = 0x000103FF
XEX_HEADER_ORIGINAL_PE_NAME   = 0x000183FF

XEX2_MAGIC = 0x58455832  # 'XEX2'
MODULE_TITLE = 0x00000001
MODULE_DLL   = 0x00000008

ENC_NONE = 0
COMP_NONE = 0    # idaxex XexDataFormat::None  -> read_basefile_raw
COMP_BASIC = 1   # idaxex XexDataFormat::Raw   -> read_basefile_uncompressed (DataSize/ZeroSize blocks)

PAGE_SIZE = 0x10000  # 64 KiB (retail 360 page for user modules; matches stock)

SEC_READONLY = 3  # header/resource page info nibble
SEC_CODE = 1
SEC_DATA = 2


def be32(v): return struct.pack('>I', v & 0xFFFFFFFF)
def be16(v): return struct.pack('>H', v & 0xFFFF)


def align(v, a):
    return (v + a - 1) & ~(a - 1)


# ------------------------------------------------------------------ opt-header parse (for --from-xex)
def parse_opt_headers(xex):
    magic, module_flags, header_size, reserved, sec_off, header_count = struct.unpack('>IIIIII', xex[:24])
    if magic != XEX2_MAGIC:
        raise SystemExit("input --from-xex is not a XEX2 file")
    opts = {}
    off = 24
    for _ in range(header_count):
        key, val = struct.unpack('>II', xex[off:off+8]); off += 8
        opts[key] = val
    return module_flags, header_size, sec_off, opts


def opt_value_or_offset(key, val, xex):
    """Mirror getOptHeaderPtr: low byte 0/1 => inline value, else offset into file."""
    lb = key & 0xFF
    if lb in (0x00, 0x01):
        return ('value', val)
    return ('offset', val)


def extract_import_block(xex, opts):
    if XEX_HEADER_IMPORT_LIBRARIES not in opts:
        return None
    off = opts[XEX_HEADER_IMPORT_LIBRARIES]
    size_of_header = struct.unpack('>I', xex[off:off+4])[0]  # Xex2ImportHeader.sizeOfHeader = whole block
    return xex[off:off+size_of_header]


def import_lib_count(import_block):
    if not import_block:
        return 0
    # Xex2ImportHeader: sizeOfHeader(4), sizeOfStringTable(4), numImports(4)
    return struct.unpack('>I', import_block[8:12])[0]


def compute_import_digest(import_block):
    """HvImageInfo.ImportDigest = SHA1 of the FIRST import table's bytes [4:TableSize]
    (idaxex XEXFile::get_imports). Requires the block's per-table NextImportDigest
    chain to already be filled (synthesize_import_block does; copied/raw blocks carry
    it verbatim). Returns 20 zero bytes when there is no import block."""
    if not import_block:
        return b'\x00' * 0x14
    string_table_size = struct.unpack('>I', import_block[4:8])[0]
    off = 12 + string_table_size          # first per-module table
    table_size = struct.unpack('>I', import_block[off:off+4])[0]
    return hashlib.sha1(import_block[off+4:off+table_size]).digest()


# ------------------------------------------------------------------ IMPORT_LIBRARIES synthesis (--import-map)
#
# Join point: pack the *unmapped* link.exe PE into a bootable XEX2-DLL whose
# IMPORT_LIBRARIES opt block is synthesized (not copied from a stock XEX).
#
# The X360-MWCC PE (linked -XEX:NO) carries genuine XEX-shaped import plumbing:
#   * a per-module IAT (PE FirstThunk) of 4-byte slots, one per imported ordinal
#   * a contiguous table of 16-byte call thunks in .text, one per IAT slot:
#         lis   r11, imagebase>>16
#         lwz   r11, iat_off(r11)      ; load resolved pointer from its IAT slot
#         mtctr r11
#         bctr
#     Every by-name import call site `bl`s its thunk.
#
# We map each import to the STOCK-FAITHFUL dual-record encoding the Xbox/Xenia
# loader expects (verified against the RB3E 0.7 stock block + xex_module.cc):
#   * a type-0 (variable) record at the 4-byte IAT slot   -> value (modidx<<16)|ord
#   * a type-1 (function) record at the 16-byte .text thunk -> value 0x01000000|ord
# Xenia (SetupLibraryImports) reads the big-endian record value at each record
# address: high byte = type, low 16 = ordinal (middle "hint" ignored). For a
# type-1 kernel function it overwrites the thunk with `sc 2; blr; nop; nop` and
# wires the kernel shim, so the `bl thunk` call sites reach the real export. The
# preceding type-0 slot is what its assert pairs the thunk ordinal against.
#
# The XEX loader reads records big-endian, but link.exe writes the PE IAT slots
# little-endian, so we must (re)write both the IAT slots and the thunk lead-words
# in the MAPPED image with the correct big-endian record values (returned as
# `rewrites` = [(image_offset, be32_value)]).

# xam.xex / xboxkrnl.exe library id + version words for this title, copied from the
# stock RB3E 0.7 IMPORT_LIBRARIES block (identify the exact library versions the
# title binds). NextImportDigest is intentionally zeroed (unsigned-boot philosophy;
# Xenia/RGH do not abort on an import-digest mismatch).
STOCK_MODULE_META = {
    'xam.xex':      {'id': 0xFCA15C76, 'ver': 0x20530800, 'vermin': 0x20074500},
    'xboxkrnl.exe': {'id': 0x45DC17E0, 'ver': 0x20530800, 'vermin': 0x20074500},
}


def parse_pe(pe):
    if pe[:2] != b'MZ':
        raise SystemExit("--pe is not a PE (missing MZ)")
    e_lfanew = struct.unpack('<I', pe[0x3C:0x40])[0]
    if pe[e_lfanew:e_lfanew+4] != b'PE\x00\x00':
        raise SystemExit("--pe is not a PE (missing PE signature)")
    fh = e_lfanew + 4
    machine, nsec = struct.unpack('<HH', pe[fh:fh+4])
    opt_sz = struct.unpack('<H', pe[fh+16:fh+18])[0]
    opt = fh + 20
    aoe = struct.unpack('<I', pe[opt+16:opt+20])[0]
    image_base = struct.unpack('<I', pe[opt+28:opt+32])[0]
    size_of_image = struct.unpack('<I', pe[opt+56:opt+60])[0]
    size_of_headers = struct.unpack('<I', pe[opt+60:opt+64])[0]
    dd = opt + 96  # PE32 optional-header data-directory array
    import_rva, import_sz = struct.unpack('<II', pe[dd+8:dd+16])
    sec_off = opt + opt_sz
    secs = []
    for i in range(nsec):
        s = sec_off + i*40
        name = pe[s:s+8].rstrip(b'\x00').decode(errors='replace')
        vsz, va, rawsz, raw = struct.unpack('<IIII', pe[s+8:s+24])
        chars = struct.unpack('<I', pe[s+36:s+40])[0]
        secs.append({'name': name, 'va': va, 'vsz': vsz, 'raw': raw, 'rawsz': rawsz, 'chars': chars})
    return {'machine': machine, 'image_base': image_base, 'aoe': aoe,
            'size_of_image': size_of_image, 'size_of_headers': size_of_headers,
            'import_rva': import_rva, 'import_sz': import_sz, 'sections': secs}


def _rva2off(pe_info, rva):
    for s in pe_info['sections']:
        if s['va'] <= rva < s['va'] + max(s['vsz'], s['rawsz']):
            return s['raw'] + (rva - s['va'])
    return None


def map_pe_to_image(pe, pe_info):
    """Expand an unmapped link.exe PE into a flat, load-mapped image where file
    offset == RVA (what `xex1tool -b` recovers). Sections land at their VAs; the
    file's SizeOfHeaders bytes stay at offset 0 so the loader still sees the PE."""
    end = pe_info['size_of_image']
    for s in pe_info['sections']:
        end = max(end, s['va'] + max(s['vsz'], s['rawsz']))
    end = align(end, PAGE_SIZE)
    img = bytearray(end)
    shdr = pe_info['size_of_headers']
    img[0:shdr] = pe[0:shdr]
    for s in pe_info['sections']:
        copy_len = min(s['rawsz'], s['vsz']) if s['vsz'] else s['rawsz']
        img[s['va']:s['va']+copy_len] = pe[s['raw']:s['raw']+copy_len]
    return bytes(img)


def synthesize_import_block(pe, pe_info, import_map):
    """Build a byte-correct XEX2 IMPORT_LIBRARIES opt block from the PE's own
    .idata + .text thunk table, cross-checked against import_map. Returns
    (import_block_bytes, rewrites, summary). rewrites are applied to the MAPPED
    image (offset == RVA)."""
    image_base = pe_info['image_base']

    # ---- 1. per-module ordered ordinals + IAT rva from .idata descriptors ----
    o = _rva2off(pe_info, pe_info['import_rva'])
    if o is None:
        raise SystemExit("could not locate the PE import directory (.idata)")
    modules = []
    idx = 0
    while True:
        d = o + idx*20
        oft, ts, fc, name_rva, ft = struct.unpack('<IIIII', pe[d:d+20])
        if oft == 0 and name_rva == 0 and ft == 0:
            break
        nm = pe[_rva2off(pe_info, name_rva):].split(b'\x00', 1)[0].decode()
        int_rva = oft if oft else ft   # INT (hint/ordinal) table; falls back to IAT
        to = _rva2off(pe_info, int_rva)
        ords = []
        j = 0
        while True:
            ent = struct.unpack('<I', pe[to+j*4:to+j*4+4])[0]
            if ent == 0:
                break
            if not (ent & 0x80000000):
                raise SystemExit("import-by-name unsupported (ordinal imports only): %s" % nm)
            ords.append(ent & 0xFFFF)
            j += 1
        modules.append({'name': nm, 'iat_rva': ft, 'ordinals': ords})
        idx += 1

    # ---- 2. .text 16-byte call-thunk table: iat_off -> thunk_va ----
    text = next(s for s in pe_info['sections'] if s['name'] == '.text')
    lis_hi = image_base >> 16
    thunks = {}
    i = text['raw']
    tend = text['raw'] + text['vsz']
    while i + 16 <= tend:
        w0, w1, w2, w3 = struct.unpack('>IIII', pe[i:i+16])
        if ((w0 & 0xFFFF0000) == 0x3D600000 and (w0 & 0xFFFF) == lis_hi and
                (w1 & 0xFFFF0000) == 0x816B0000 and w2 == 0x7D6903A6 and w3 == 0x4E800420):
            thunks[w1 & 0xFFFF] = text['va'] + (i - text['raw'])
        i += 4

    # ---- 3. records + image rewrites, stock-faithful (type0 IAT, type1 thunk) ----
    # index import_map by (module, ordinal) for naming/validation
    by_mod_ord = {}
    for nm, info in (import_map or {}).items():
        by_mod_ord[(info['module'], info['ordinal'])] = nm

    rewrites = []
    table_blobs = []
    names = [m['name'] for m in modules]
    unmapped = []   # ordinals present in PE but absent from import_map
    per_module = []
    for mi, m in enumerate(modules):
        meta = STOCK_MODULE_META.get(m['name'])
        if meta is None:
            raise SystemExit("no library id/version metadata for module %s" % m['name'])
        recs = []
        for si, ordv in enumerate(m['ordinals']):
            iat_off = m['iat_rva'] + si*4
            iat_va = image_base + iat_off
            if iat_off not in thunks:
                raise SystemExit("no .text call thunk for %s ordinal %d (IAT off 0x%X)"
                                 % (m['name'], ordv, iat_off))
            thunk_va = image_base + thunks[iat_off]
            # type-0 variable record at the IAT slot
            recs.append(iat_va)
            rewrites.append((iat_va - image_base, (mi << 16) | ordv))
            # type-1 function record at the .text thunk
            recs.append(thunk_va)
            rewrites.append((thunk_va - image_base, 0x01000000 | ordv))
            if (m['name'], ordv) not in by_mod_ord:
                unmapped.append((m['name'], ordv))
        count = len(recs)
        table_size = 40 + count*4
        th = be32(table_size)
        th += b'\x00' * 0x14                 # NextImportDigest (chained below)
        th += be32(meta['id'])               # ModuleNumber / library id
        th += be32(meta['ver'])              # Version
        th += be32(meta['vermin'])           # VersionMin
        th += bytes([0])                     # Unused
        th += bytes([mi])                    # ModuleIndex
        th += be16(count)                    # ImportCount (== record count)
        for r in recs:
            th += be32(r)
        table_blobs.append(th)
        per_module.append({'name': m['name'], 'index': mi,
                            'imports': len(m['ordinals']), 'records': count,
                            'table_size': table_size})

    # ---- 3b. import-digest chain: each table's NextImportDigest (bytes [4:24])
    # is SHA1 of the NEXT table's [4:TableSize]; the last table's is zero, and
    # HvImageInfo.ImportDigest = SHA1(table[0][4:TableSize]) (computed later by
    # compute_import_digest over the finished block). Fill backwards, exactly like
    # the page-hash chain (idaxex XEXFile::get_imports). ----
    next_digest = b'\x00' * 0x14
    for i in range(len(table_blobs) - 1, -1, -1):
        tb = bytearray(table_blobs[i])
        tb[4:24] = next_digest               # this table's NextImportDigest
        next_digest = hashlib.sha1(bytes(tb[4:])).digest()  # SHA1(table[4:end])
        table_blobs[i] = bytes(tb)
    tables = b''.join(table_blobs)

    # ---- 4. name table (module names, NUL-separated, 4-aligned) + descriptor ----
    nt = b''
    for nm in names:
        nt += nm.encode() + b'\x00'
    nt += b'\x00' * (align(len(nt), 4) - len(nt))
    block_size = 12 + len(nt) + len(tables)
    desc = be32(block_size) + be32(len(nt)) + be32(len(modules))
    block = desc + nt + tables

    summary = {'modules': per_module, 'block_size': block_size,
               'name_table_size': len(nt), 'unmapped_ordinals': unmapped}
    return block, rewrites, summary


# ------------------------------------------------------------------ page descriptors
def build_page_descriptors(pe_data):
    """One 0x18-byte descriptor per 64 KiB page: SizeInfo(4, BE) + DataDigest(0x14).
    SizeInfo = (pageCount<<4) | info-nibble (page0=readonly(3), code pages=code(1),
    else data(2)); we classify from the PE section table when available.

    The DataDigest field is the SHA1 *page-hash chain* the kernel's XexpVerifyImage
    walks (idaxex XEXFile::basefile_verify): descriptor[i].DataDigest is the hash of
    the *next* page, and

        hash[i] = SHA1( page_data[i]  ++  descriptor[i] (its 0x18 bytes) )

    with descriptor[last].DataDigest = 0, and ImageHash (in HvImageInfo) = hash[0].
    We therefore fill the chain backwards. Returns (bytes, n_pages, image_hash).
    Emitting a valid chain is required: on retail 17559 the loader rejects a stale/
    zeroed image hash *before mapping* (matches xex1tool's "Invalid image hash!")."""
    n_pages = align(len(pe_data), PAGE_SIZE) // PAGE_SIZE
    infos = [SEC_DATA] * n_pages
    infos[0] = SEC_READONLY  # header page

    # classify pages from PE sections (best-effort; only affects the info nibble)
    try:
        e_lfanew = struct.unpack('<I', pe_data[0x3C:0x40])[0]
        if pe_data[e_lfanew:e_lfanew+4] == b'PE\x00\x00':
            fh_off = e_lfanew + 4
            num_sec, = struct.unpack('<H', pe_data[fh_off+2:fh_off+4])
            opt_sz, = struct.unpack('<H', pe_data[fh_off+16:fh_off+18])
            sec_off = fh_off + 20 + opt_sz
            IMAGE_SCN_CNT_CODE = 0x00000020
            for i in range(num_sec):
                s = sec_off + i*40
                va, = struct.unpack('<I', pe_data[s+12:s+16])
                vsz, = struct.unpack('<I', pe_data[s+8:s+12])
                chars, = struct.unpack('<I', pe_data[s+36:s+40])
                kind = SEC_CODE if (chars & IMAGE_SCN_CNT_CODE) else SEC_DATA
                first = va // PAGE_SIZE
                last = (va + vsz - 1) // PAGE_SIZE
                for p in range(first, min(last, n_pages-1) + 1):
                    if p != 0:
                        infos[p] = kind
    except Exception:
        pass

    size_infos = [(info & 0xF) | (1 << 4) for info in infos]  # pageCount=1 | info

    # Walk the chain backwards: descriptor[i].DataDigest = hash of page i+1
    # (zeros past the last page); descriptor[i] hashes page i chained with itself.
    digests = [b'\x00' * 0x14] * n_pages
    next_hash = b'\x00' * 0x14
    for i in range(n_pages - 1, -1, -1):
        digests[i] = next_hash                      # = hash of the next page
        descriptor = be32(size_infos[i]) + digests[i]
        page = pe_data[i * PAGE_SIZE:(i + 1) * PAGE_SIZE]
        if len(page) < PAGE_SIZE:                    # last page: zero-pad to 64 KiB
            page = page + b'\x00' * (PAGE_SIZE - len(page))
        next_hash = hashlib.sha1(page + descriptor).digest()
    image_hash = next_hash                            # hash[0]

    out = b''.join(be32(size_infos[i]) + digests[i] for i in range(n_pages))
    return out, n_pages, image_hash


# ------------------------------------------------------------------ security info
def build_security_info(image_size, load_addr, import_table_count, page_desc_bytes,
                        region=0xFFFFFFFF, media_types=0xFFFFFFFF, image_flags=0,
                        image_hash=b'\x00' * 0x14, import_digest=b'\x00' * 0x14):
    HVINFO_SIZE = 0x174
    n_desc = len(page_desc_bytes) // 0x18
    sec_size = 0x184 + len(page_desc_bytes)  # Size field = whole secinfo region incl page descriptors

    hv = b''
    hv += b'\x00' * 0x100          # Signature (unsigned -> zero; RGH bypasses RSA)
    hv += be32(HVINFO_SIZE)        # InfoSize (0x174)
    hv += be32(image_flags)        # ImageFlags
    hv += be32(load_addr)          # LoadAddress
    hv += image_hash               # ImageHash (root of the page-hash chain)
    assert len(image_hash) == 0x14
    hv += be32(import_table_count) # ImportTableCount
    hv += import_digest            # ImportDigest (SHA1 of first import table)
    assert len(import_digest) == 0x14
    hv += b'\x00' * 0x10           # MediaID
    hv += b'\x00' * 0x10           # ImageKey (enc none)
    hv += be32(0)                  # ExportTableAddress
    hv += b'\x00' * 0x14           # HeaderHash
    hv += be32(region)             # GameRegion
    assert len(hv) == HVINFO_SIZE, hex(len(hv))

    sec = b''
    sec += be32(sec_size)          # Size
    sec += be32(image_size)        # ImageSize
    sec += hv                      # HvImageInfo
    sec += be32(media_types)       # AllowedMediaTypes
    sec += be32(n_desc)            # PageDescriptorCount
    assert len(sec) == 0x184, hex(len(sec))
    return sec + page_desc_bytes


# ------------------------------------------------------------------ file format info opt data
def build_file_format_info(compression, encryption, image_size):
    if compression == COMP_NONE:
        # XexFileDataDescriptor { Size=8, Flags=enc, Format=0 }
        return be32(8) + be16(encryption) + be16(COMP_NONE)
    elif compression == COMP_BASIC:
        # descriptor(8) + one XexRawDataDescriptor { DataSize=image_size, ZeroSize=0 }
        size = 8 + 8
        return be32(size) + be16(encryption) + be16(COMP_BASIC) + be32(image_size) + be32(0)
    else:
        raise SystemExit("unsupported compression")


# ------------------------------------------------------------------ main pack
def pack(pe_data, out_path, entry_point, base_addr, import_block, pe_name,
         compression, module_flags):
    image_size = len(pe_data)
    if image_size % PAGE_SIZE != 0:
        # pad image up to a page so page descriptors cover it (basefile stays raw copy)
        pe_data = pe_data + b'\x00' * (align(image_size, PAGE_SIZE) - image_size)
        image_size = len(pe_data)

    import_count = import_lib_count(import_block)

    page_desc_bytes, _, image_hash = build_page_descriptors(pe_data)
    sec_info = build_security_info(image_size, base_addr, import_count, page_desc_bytes,
                                   image_hash=image_hash,
                                   import_digest=compute_import_digest(import_block))

    ffi = build_file_format_info(compression, ENC_NONE, image_size)
    pename_blob = None
    if pe_name:
        nb = pe_name.encode() + b'\x00'
        nb = nb + b'\x00' * (align(len(nb), 4) - len(nb))
        pename_blob = be32(len(nb) + 4) + nb  # size-prefixed variable field

    # ---- opt-header directory: keys in ascending order ----
    entries = []  # (key, kind, payload)
    entries.append((XEX_HEADER_FILE_FORMAT_INFO, 'offset', ffi))
    entries.append((XEX_HEADER_ENTRY_POINT, 'value', entry_point))
    entries.append((XEX_HEADER_IMAGE_BASE_ADDRESS, 'value', base_addr))
    if import_block:
        entries.append((XEX_HEADER_IMPORT_LIBRARIES, 'offset', import_block))
    if pename_blob is not None:
        entries.append((XEX_HEADER_ORIGINAL_PE_NAME, 'offset', pename_blob))
    entries.sort(key=lambda e: e[0])

    header_count = len(entries)
    sec_offset = 24 + header_count * 8

    # ---- layout: header | opt-dir | secinfo(+pagedesc) | opt-data blobs | pad | basefile ----
    data_region_start = sec_offset + len(sec_info)
    cursor = data_region_start
    opt_offsets = {}
    data_blobs = b''
    for key, kind, payload in entries:
        if kind == 'offset':
            payload = payload  # bytes
            opt_offsets[key] = cursor
            # 4-align each data blob
            data_blobs += payload
            pad = align(len(payload), 4) - len(payload)
            data_blobs += b'\x00' * pad
            cursor += len(payload) + pad

    size_of_headers = align(cursor, 0x1000)  # stock aligns headers to 0x1000; safe for loader

    # ---- build opt directory ----
    opt_dir = b''
    for key, kind, payload in entries:
        if kind == 'value':
            opt_dir += be32(key) + be32(payload)
        else:
            opt_dir += be32(key) + be32(opt_offsets[key])

    # ---- assemble ----
    xex = bytearray()
    xex += be32(XEX2_MAGIC)
    xex += be32(module_flags)
    xex += be32(size_of_headers)      # headerSize
    xex += be32(0)                    # reserved
    xex += be32(sec_offset)           # securityOffset
    xex += be32(header_count)
    assert len(xex) == 24
    xex += opt_dir
    assert len(xex) == sec_offset, (len(xex), sec_offset)
    xex += sec_info
    assert len(xex) == data_region_start
    xex += data_blobs
    # pad header region to size_of_headers
    xex += b'\x00' * (size_of_headers - len(xex))
    assert len(xex) == size_of_headers, (len(xex), size_of_headers)

    # ---- HeaderHash: SHA1 over the whole header region EXCEPT the HvImageInfo
    # block (idaxex XEXFile::verify_secinfo). The kernel checks it before mapping,
    # so a stale/zeroed value is rejected ("Invalid header hash!"). The hashed
    # order is header_remainder ++ front, and since HeaderHash lives inside the
    # excluded HvImageInfo, patching it afterwards does not disturb the hash. ----
    imageinfo_end = sec_offset + 8 + 0x174
    header_hash = hashlib.sha1(bytes(xex[imageinfo_end:size_of_headers])
                               + bytes(xex[0:sec_offset + 8])).digest()
    hh_off = sec_offset + 8 + 0x15C   # HvImageInfo.HeaderHash
    xex[hh_off:hh_off + 0x14] = header_hash

    # append raw basefile
    xex += pe_data

    with open(out_path, 'wb') as f:
        f.write(xex)

    return {
        'out': out_path,
        'file_size': len(xex),
        'size_of_headers': size_of_headers,
        'image_size': image_size,
        'sec_offset': sec_offset,
        'header_count': header_count,
        'import_count': import_count,
        'compression': 'none' if compression == COMP_NONE else 'basic',
        'entry_point': entry_point,
        'base_addr': base_addr,
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--pe', required=True, help='input basefile (fully-mapped image, e.g. xex1tool -b output)')
    ap.add_argument('--out', required=True)
    ap.add_argument('--from-xex', help='source XEX to copy entry-point/image-base/imports/pe-name from')
    ap.add_argument('--entry', type=lambda x: int(x, 0), help='entry point VA (overrides --from-xex)')
    ap.add_argument('--base', type=lambda x: int(x, 0), default=0x84000000)
    ap.add_argument('--import-block', help='raw import-libraries opt block (overrides --from-xex)')
    ap.add_argument('--import-map', help='JSON name->{module,ordinal}: synthesize the IMPORT_LIBRARIES '
                    'block from the unmapped link.exe PE (.idata + .text thunks), cross-checked against this map')
    ap.add_argument('--pe-name', help='original PE name string')
    ap.add_argument('--compress', choices=['none', 'basic'], default='basic',
                    help='none = XexDataFormat::None (raw); basic = single DataSize/ZeroSize block. default basic.')
    ap.add_argument('--module-flags', type=lambda x: int(x, 0),
                    default=MODULE_TITLE | MODULE_DLL)
    args = ap.parse_args()

    with open(args.pe, 'rb') as f:
        pe_raw = f.read()

    entry = args.entry
    import_block = None
    pe_name = args.pe_name

    # Two kinds of --pe input:
    #  * a fully-mapped basefile (offset == RVA, e.g. `xex1tool -b` output) whose
    #    length already spans SizeOfImage -- pack it verbatim (the round-trip
    #    identity path relies on this being byte-preserved);
    #  * an UNMAPPED link.exe PE (offset != RVA, what build_*_xex.sh feed) whose
    #    length is just the on-disk sections -- expand it to a flat load-mapped
    #    image so the loader-copied basefile has each section at its true VA and
    #    the entry-point RVA lands on real code (was the original crash: the raw
    #    PE was packed unmapped, so the entry VA pointed into unmapped space).
    pe_info = parse_pe(pe_raw)
    # A mapped basefile lays section data at file offset == RVA, so it is spread
    # across VA space and is LARGER than the compact on-disk raw-packed layout; an
    # unmapped link.exe PE is exactly that raw-packed size. (Tools like idaxex -b
    # keep the original raw pointers in the section headers even though the data is
    # mapped, so header offsets alone can't tell them apart -- size can.)
    raw_packed_end = pe_info['size_of_headers']
    for _s in pe_info['sections']:
        raw_packed_end = max(raw_packed_end, _s['raw'] + _s['rawsz'])
    already_mapped = len(pe_raw) > raw_packed_end
    image = bytearray(pe_raw) if already_mapped else bytearray(map_pe_to_image(pe_raw, pe_info))
    if entry is None:
        entry = pe_info['image_base'] + pe_info['aoe']

    if args.import_map:
        import json
        with open(args.import_map) as f:
            import_map = json.load(f)
        # synthesize reads the unmapped PE's .idata/.text; rewrites patch the map
        import_block, rewrites, summary = synthesize_import_block(pe_raw, pe_info, import_map)
        for off, val in rewrites:
            struct.pack_into('>I', image, off, val)
        print("  [import-map] synthesized IMPORT_LIBRARIES block:")
        for m in summary['modules']:
            print(f"    module {m['index']} {m['name']:14} imports={m['imports']} "
                  f"records={m['records']} table_size=0x{m['table_size']:X}")
        print(f"    block_size=0x{summary['block_size']:X} name_table=0x{summary['name_table_size']:X} "
              f"rewrites={len(rewrites)}")
        if summary['unmapped_ordinals']:
            print(f"    NOTE {len(summary['unmapped_ordinals'])} PE ordinal(s) not in import-map "
                  f"(named/validated from .idata only): "
                  + ", ".join(f"{mod}@{o}" for mod, o in summary['unmapped_ordinals']))

    pe_data = bytes(image)

    if args.from_xex:
        with open(args.from_xex, 'rb') as f:
            src = f.read()
        module_flags, hsz, sec_off, opts = parse_opt_headers(src)
        if entry is None and XEX_HEADER_ENTRY_POINT in opts:
            entry = opts[XEX_HEADER_ENTRY_POINT]
        if import_block is None:
            import_block = extract_import_block(src, opts)
        if pe_name is None and XEX_HEADER_ORIGINAL_PE_NAME in opts:
            # read size-prefixed name
            o = opts[XEX_HEADER_ORIGINAL_PE_NAME]
            sz = struct.unpack('>I', src[o:o+4])[0]
            raw = src[o+4:o+sz].split(b'\x00', 1)[0]
            pe_name = raw.decode(errors='replace')

    if args.import_block:
        with open(args.import_block, 'rb') as f:
            import_block = f.read()

    if entry is None:
        raise SystemExit("entry point required (via --entry or --from-xex)")

    compression = COMP_NONE if args.compress == 'none' else COMP_BASIC

    info = pack(pe_data, args.out, entry, args.base, import_block, pe_name,
                compression, args.module_flags)

    for k, v in info.items():
        if isinstance(v, int) and k in ('image_size', 'size_of_headers', 'sec_offset', 'entry_point', 'base_addr'):
            print(f"  {k:16} = 0x{v:X}")
        else:
            print(f"  {k:16} = {v}")


if __name__ == '__main__':
    main()
