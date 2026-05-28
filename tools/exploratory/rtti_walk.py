#!/usr/bin/env python3
"""RTTI walk: extract all class names + vtables from rb3 binary via MSVC X360 RTTI.

X360 RTTI layout (empirically derived for rb3 / band.exe — does NOT have the
classic 0x19930522 signature in COL):

  Each vtable is preceded by a 4-byte pointer to a COL record.
  The COL record contains:
    +0x00: 3 dwords of zero (where the 0x19930522 signature would be on x86)
    +0x0C: pTypeDescriptor
    +0x10: pSelf (or pClassHierarchyDescriptor)
    +0x14: ...
  The TypeDescriptor (TD) is structured:
    +0x00: 4-byte vptr to TypeDescriptor's vtable
    +0x04: 4-byte spare (zero)
    +0x08: null-terminated string of the form ".?AVCLASS@@" or ".?AUSTRUCT@@"

Method:
  1. Find all TD strings (`.?AV*@@` / `.?AU*@@`).
  2. For each TD, scan for references to TD VA in the binary (these are COL
     references via pTypeDescriptor + CHD references).
  3. From each COL (TD ref at offset +0xC of a record where +0..+8 = 0), find
     all places that reference COL_VA via a 4-byte pointer. Each such reference
     position is `vtable_va - 4`, i.e. `vtable_va = ref + 4`.
  4. Read the vtable contents until first non-text pointer (or fixed slot cap).
  5. Output rb3 vtable VA -> class name + slot fn_ list.

Then we can:
  - Identify each `vftable_XXXXXXXX` previously found in jeff's rdata scan with
    its class name.
  - Match rb3 class -> dc3 class (by demangled name match), giving us a direct
    vtable-pair that vtable_transitivity.py can then use.

Output:
  /tmp/exploratory_rtti.json   { rb3_vt_va_hex: { class, slots: [fn_X], cls_name_demangled? }}
  /tmp/exploratory_rtti_stats.json
"""

from __future__ import annotations
import json
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path("/home/free/code/milohax/rb3-xenon")
RB3_EXE = ROOT / "orig/45410914/band.exe"


def parse_pe(path):
    exe = open(path, 'rb').read()
    pe_off = struct.unpack_from('<I', exe, 0x3c)[0]
    n_sections = struct.unpack_from('<H', exe, pe_off + 6)[0]
    size_oh = struct.unpack_from('<H', exe, pe_off + 20)[0]
    img_base = struct.unpack_from('<I', exe, pe_off + 24 + 28)[0]
    sect_start = pe_off + 24 + size_oh
    sections = []
    for i in range(n_sections):
        o = sect_start + i * 40
        name = exe[o:o + 8].rstrip(b'\x00').decode('ascii', errors='replace')
        vaddr = struct.unpack_from('<I', exe, o + 12)[0]
        vsize = struct.unpack_from('<I', exe, o + 8)[0]
        raw_off = struct.unpack_from('<I', exe, o + 20)[0]
        sections.append((name, img_base + vaddr, vsize, raw_off))
    return exe, img_base, sections


def main():
    print("[1/4] parsing PE...", file=sys.stderr)
    exe, img_base, sections = parse_pe(RB3_EXE)

    text_lo = text_hi = None
    for name, va, sz, _ in sections:
        if name == '.text':
            text_lo, text_hi = va, va + sz
    print(f"  text range: 0x{text_lo:08x}..0x{text_hi:08x}", file=sys.stderr)

    def va_to_off(va):
        for n, sec_va, vsize, off in sections:
            if sec_va <= va < sec_va + vsize:
                return off + (va - sec_va)
        return None

    def off_to_va(o):
        for n, sec_va, vsize, off in sections:
            if off <= o < off + vsize:
                return sec_va + (o - off)
        return None

    print("[2/4] finding TypeDescriptors...", file=sys.stderr)
    re_name = re.compile(rb'\.\?A[VU]([^\x00]+)@@\x00')
    td_records = []  # (td_va, class_mangled)
    td_va_to_name = {}
    for m in re_name.finditer(exe):
        name_start = m.start()
        td_off = name_start - 8
        if td_off < 0:
            continue
        vptr = struct.unpack_from('>I', exe, td_off)[0]
        if not (0x82000000 <= vptr < 0x83000000):
            continue
        td_va = off_to_va(td_off)
        if td_va is None:
            continue
        cls_name = m.group(1).decode('ascii', errors='replace')
        # The TD name encoding: '.?AV<CLASS>@@' or '.?AU<STRUCT>@@'
        td_records.append((td_va, cls_name))
        td_va_to_name[td_va] = cls_name
    print(f"  TDs found: {len(td_records)}", file=sys.stderr)

    print("[3/4] locating COLs (TD refs with +0..+8 == 0)...", file=sys.stderr)
    # For each TD VA, find references to it across the binary.
    # A COL has the TD pointer at offset +0xC and 12 bytes of zeros before it.
    cols = []  # (col_va, td_va, cls_name)
    td_va_set = set(td_va_to_name.keys())
    for td_va, cls in td_records:
        pat = struct.pack('>I', td_va)
        start = 0
        while True:
            p = exe.find(pat, start)
            if p < 0:
                break
            start = p + 4
            # Check pattern: 12 bytes of zero starting at p - 0xC
            col_start = p - 0xC
            if col_start < 0:
                continue
            head = exe[col_start:p]
            if head != b'\x00' * 0xC:
                continue
            col_va = off_to_va(col_start)
            if col_va is not None:
                cols.append((col_va, td_va, cls))
    print(f"  COLs identified: {len(cols)}", file=sys.stderr)
    col_va_to_class = {col_va: cls for col_va, _, cls in cols}

    print("[4/4] finding vtables (refs to COL_VA -> vtable at ref+4)...", file=sys.stderr)
    # For each COL VA, find references (in binary)
    vt_to_cls = {}  # vt_va -> class
    vt_slots = {}   # vt_va -> [slot_va_hex_int]
    for col_va, _, cls in cols:
        pat = struct.pack('>I', col_va)
        start = 0
        while True:
            p = exe.find(pat, start)
            if p < 0:
                break
            start = p + 4
            vt_off = p + 4
            vt_va = off_to_va(vt_off)
            if vt_va is None:
                continue
            # Read up to 200 dwords; stop at first non-text
            slots = []
            for j in range(200):
                slot_off = vt_off + j * 4
                if slot_off + 4 > len(exe):
                    break
                slot_va = struct.unpack_from('>I', exe, slot_off)[0]
                if not (text_lo <= slot_va < text_hi):
                    break
                slots.append(slot_va)
            if vt_va in vt_to_cls and vt_to_cls[vt_va] != cls:
                # Duplicate; keep first
                continue
            vt_to_cls[vt_va] = cls
            vt_slots[vt_va] = slots
    print(f"  vtables discovered: {len(vt_to_cls)}", file=sys.stderr)

    # Stats: how many of these match the 342 jeff-found vftable_XXXXXXXX list?
    # We don't strictly need this; just include the count.
    output_vtables = {}
    for vt_va, cls in vt_to_cls.items():
        output_vtables[f'0x{vt_va:08x}'] = {
            'class_mangled': cls,
            'slot_count': len(vt_slots[vt_va]),
            'slot_vas': [f'0x{s:08x}' for s in vt_slots[vt_va]],
            'slot_fn_names': [f'fn_{s:08X}' for s in vt_slots[vt_va]],
        }

    # Also output classes (TDs) with no vtable found — these are forward-declared
    # or template-only; less useful for fn identification but useful for naming
    # the binary.
    all_classes = {td_va_to_name[td_va] for td_va in td_va_to_name}
    classes_with_vt = {cls for cls in vt_to_cls.values()}
    classes_without_vt = all_classes - classes_with_vt

    stats = {
        'tds_found': len(td_records),
        'cols_identified': len(cols),
        'vtables_discovered': len(vt_to_cls),
        'classes_with_vtable': len(classes_with_vt),
        'classes_without_vtable': len(classes_without_vt),
    }

    out = {
        'stats': stats,
        'classes_without_vtable_sample': sorted(classes_without_vt)[:30],
        'vtable_sample': dict(list(output_vtables.items())[:20]),
    }
    json.dump(output_vtables, open('/tmp/exploratory_rtti.json', 'w'), indent=2)
    json.dump(out, open('/tmp/exploratory_rtti_stats.json', 'w'), indent=2)
    # Also dump just the class list for the report
    cls_list = sorted(classes_with_vt)
    open('/tmp/exploratory_rtti_classes.txt', 'w').write('\n'.join(cls_list))

    print()
    print("=" * 70)
    print("RTTI WALK RESULTS")
    print("=" * 70)
    for k, v in stats.items():
        print(f"  {k}: {v}")
    print()
    print(f"Output: /tmp/exploratory_rtti.json ({len(output_vtables)} vtables)")
    print(f"Stats:  /tmp/exploratory_rtti_stats.json")
    print(f"Class list: /tmp/exploratory_rtti_classes.txt ({len(cls_list)} entries)")


if __name__ == '__main__':
    main()
