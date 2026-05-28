#!/usr/bin/env python3
"""RTTI + Vtable transitivity combined: identify fn_XXXXXXXX via
class-name-matched vtable slot transfer.

Method:
  1. RTTI walk on rb3.exe -> {vt_va: class_name_mangled, slots: [slot_va_int]}.
  2. Parse dc3 .map for `??_7CLASS@@6B@` -> dc3 vtable RVA.
  3. Read dc3.exe at each vtable RVA to extract slot fn VAs, then name them via
     dc3 .map.
  4. For each rb3 class with a matching dc3 class:
       - Pair rb3.slot[i] with dc3.slot[i]
       - rb3.slot[i] is `fn_<VA>` (anonymous); dc3.slot[i] is the named fn
       - Propose rb3 fn = dc3 name

Validation:
  - Cross-verify any proposals against unified_id.json (rb3_fn -> dc3_name);
    record agreement rate as precision indicator.

Output:
  /tmp/exploratory_rtti_vt.json     proposals
  /tmp/exploratory_rtti_vt_stats.json
"""

from __future__ import annotations
import json
import re
import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path("/home/free/code/milohax/rb3-xenon")
DC3 = Path("/home/free/code/milohax/dc3-decomp")

DC3_MAP = DC3 / "orig/373307D9/ham_xbox_r.map"
DC3_EXE = DC3 / "orig/373307D9/ham_xbox_r.exe"
RB3_EXE = ROOT / "orig/45410914/band.exe"
UNIFIED = ROOT / "unified_id.json"


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


def make_va_reader(exe, sections):
    def read_va(va, n):
        for _name, sec_va, vsize, raw_off in sections:
            if sec_va <= va < sec_va + vsize:
                offset = raw_off + (va - sec_va)
                return exe[offset:offset + n]
        return None
    return read_va


def text_bounds(sections):
    for name, va, sz, _ in sections:
        if name == '.text':
            return va, va + sz
    raise RuntimeError('no .text section')


def parse_dc3_map():
    addr_to_name = {}
    vtable_to_class = {}  # vt_rva -> mangled class (the part inside ??_7..@@6B@)
    pub_re = re.compile(r'^\s+([0-9a-f]{4}):([0-9a-f]{8})\s+(\S+)\s+([0-9a-f]{8})')
    vt_re = re.compile(r'^\?\?_7(.+?)@@6B(.*)@$')
    saw_header = False
    with open(DC3_MAP) as f:
        for line in f:
            if 'Publics by Value' in line:
                saw_header = True
                continue
            if not saw_header:
                continue
            m = pub_re.match(line)
            if not m:
                continue
            rva = int(m.group(4), 16)
            name = m.group(3)
            if rva < 0x82000000:
                continue
            if rva not in addr_to_name:
                addr_to_name[rva] = name
            vm = vt_re.match(name)
            if vm:
                vtable_to_class[rva] = (vm.group(1), vm.group(2))  # cls, base
    return addr_to_name, vtable_to_class


def extract_dc3_vt(rva, exe, sections, addr_to_name, sorted_addrs, text_lo, text_hi):
    # Determine length by next named symbol
    import bisect
    i = bisect.bisect_right(sorted_addrs, rva)
    if i >= len(sorted_addrs):
        n_slots = 200
    else:
        n_slots = min(200, (sorted_addrs[i] - rva) // 4)
    read = make_va_reader(exe, sections)
    raw = read(rva, n_slots * 4)
    if raw is None or len(raw) < 4:
        return []
    out = []
    for j in range(min(n_slots, len(raw) // 4)):
        v = struct.unpack_from('>I', raw, j * 4)[0]
        if not (text_lo <= v < text_hi):
            break
        out.append((j, v, addr_to_name.get(v)))
    return out


def walk_rb3_rtti(rb3_exe_path):
    """Return {vt_va: {'class': mangled, 'slot_vas': [int]}}."""
    exe, img_base, sections = parse_pe(rb3_exe_path)
    text_lo, text_hi = text_bounds(sections)

    def off_to_va(o):
        for n, sec_va, vsize, off in sections:
            if off <= o < off + vsize:
                return sec_va + (o - off)
        return None

    re_name = re.compile(rb'\.\?A[VU]([^\x00]+)@@\x00')
    td_records = []
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
        td_records.append((td_va, m.group(1).decode('ascii', errors='replace')))

    # Find COLs (12 bytes of zero immediately before pTD pointer)
    cols = []
    for td_va, cls in td_records:
        pat = struct.pack('>I', td_va)
        start = 0
        while True:
            p = exe.find(pat, start)
            if p < 0:
                break
            start = p + 4
            col_start = p - 0xC
            if col_start < 0:
                continue
            if exe[col_start:p] != b'\x00' * 0xC:
                continue
            col_va = off_to_va(col_start)
            if col_va is not None:
                cols.append((col_va, cls))

    # For each COL, find references; vtable = ref + 4
    out = {}
    for col_va, cls in cols:
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
            # Read slots
            slots = []
            for j in range(200):
                so = vt_off + j * 4
                if so + 4 > len(exe):
                    break
                v = struct.unpack_from('>I', exe, so)[0]
                if not (text_lo <= v < text_hi):
                    break
                slots.append(v)
            if vt_va in out and out[vt_va]['class'] != cls:
                continue
            out[vt_va] = {'class': cls, 'slot_vas': slots}
    return out


def main():
    print("[1/4] walking rb3 RTTI...", file=sys.stderr)
    rb3_vts = walk_rb3_rtti(RB3_EXE)
    print(f"  rb3 vtables (via RTTI): {len(rb3_vts)}", file=sys.stderr)
    # Index rb3 vts by class name
    rb3_by_class = defaultdict(list)  # cls -> [(vt_va, slot_vas)]
    for vt_va, info in rb3_vts.items():
        rb3_by_class[info['class']].append((vt_va, info['slot_vas']))
    print(f"  distinct rb3 classes: {len(rb3_by_class)}", file=sys.stderr)

    print("[2/4] parsing dc3 .map + extracting dc3 vtables...", file=sys.stderr)
    dc3_addr_to_name, dc3_vt_to_class = parse_dc3_map()
    dc3_exe, _, dc3_sections = parse_pe(DC3_EXE)
    dc3_text_lo, dc3_text_hi = text_bounds(dc3_sections)
    sorted_dc3_addrs = sorted(dc3_addr_to_name.keys())

    # dc3 class name -> [(vt_va, slots)]
    dc3_by_class = defaultdict(list)
    for vt_va, (cls, base) in dc3_vt_to_class.items():
        slots = extract_dc3_vt(vt_va, dc3_exe, dc3_sections, dc3_addr_to_name,
                                sorted_dc3_addrs, dc3_text_lo, dc3_text_hi)
        # Use the base modifier as a tag for multiple-inheritance sub-vtables
        dc3_by_class[cls].append({'vt_va': vt_va, 'base': base, 'slots': slots})
    print(f"  distinct dc3 classes (??_7CLS@@6B@): {len(dc3_by_class)}", file=sys.stderr)

    # Find class overlap
    common_classes = set(rb3_by_class) & set(dc3_by_class)
    print(f"  common (rb3 ∩ dc3) classes: {len(common_classes)}", file=sys.stderr)

    print("[3/4] proposing fn identifications...", file=sys.stderr)
    proposals = {}  # rb3_fn -> dc3_name
    pairings = []  # records
    skip_layout_mismatch = 0
    pairs_done = 0
    classes_with_multi_vt = 0

    for cls in sorted(common_classes):
        rb3_list = rb3_by_class[cls]
        dc3_list = dc3_by_class[cls]
        # Simple case: 1 vt on each side -> pair
        # Multi case: pair by base offset tag if available, else by slot count similarity
        if len(rb3_list) > 1 or len(dc3_list) > 1:
            classes_with_multi_vt += 1
            # Skip for now (could enhance)
            continue
        rb3_vt_va, rb3_slots = rb3_list[0]
        dc3_info = dc3_list[0]
        dc3_slots = dc3_info['slots']
        if not dc3_slots:
            continue
        # Pair slot by slot up to min length
        n_pair = min(len(rb3_slots), len(dc3_slots))
        if n_pair == 0:
            continue
        # Confidence tier:
        #   HIGH: rb3_slot_count == dc3_slot_count (engine layout unchanged)
        #   LOW : different slot counts (engine drift; slot positions unreliable)
        same_layout = len(rb3_slots) == len(dc3_slots)
        pairs_done += 1
        new_props_here = []
        for j in range(n_pair):
            rb3_slot_va = rb3_slots[j]
            dc3_slot_name = dc3_slots[j][2]
            if dc3_slot_name is None:
                continue
            rb3_fn_name = f'fn_{rb3_slot_va:08X}'
            existing = proposals.get(rb3_fn_name)
            if existing and existing[0] != dc3_slot_name:
                # collision; downgrade if previous was HIGH and this is LOW
                if existing[1] == 'HIGH' and not same_layout:
                    continue
                continue
            tier = 'HIGH' if same_layout else 'LOW'
            proposals[rb3_fn_name] = (dc3_slot_name, tier)
            new_props_here.append((rb3_fn_name, dc3_slot_name, j))
        pairings.append({
            'class': cls,
            'rb3_vt': f'0x{rb3_vt_va:08x}',
            'dc3_vt': f'0x{dc3_info["vt_va"]:08x}',
            'rb3_slot_count': len(rb3_slots),
            'dc3_slot_count': len(dc3_slots),
            'paired_slots': n_pair,
            'tier': 'HIGH' if same_layout else 'LOW',
            'new_props_in_pairing': sum(1 for j in range(n_pair)
                                        if dc3_slots[j][2] is not None),
        })

    print(f"  pairings done: {pairs_done}", file=sys.stderr)
    print(f"  multi-vt classes skipped: {classes_with_multi_vt}", file=sys.stderr)
    print(f"  proposals total: {len(proposals)}", file=sys.stderr)

    print("[4/4] cross-verifying against unified_id...", file=sys.stderr)
    unified = json.load(open(UNIFIED))
    rb3_to_dc3_name = {r['rb3_fn'].lower(): r.get('dc3_name')
                       for r in unified if r.get('dc3_name')}
    agree_high = disagree_high = 0
    agree_low = disagree_low = 0
    new_unique_high = new_unique_low = 0
    for rb3_fn, (dc3_name, tier) in proposals.items():
        existing = rb3_to_dc3_name.get(rb3_fn.lower())
        if existing:
            if existing == dc3_name:
                if tier == 'HIGH': agree_high += 1
                else: agree_low += 1
            else:
                if tier == 'HIGH': disagree_high += 1
                else: disagree_low += 1
        else:
            if tier == 'HIGH': new_unique_high += 1
            else: new_unique_low += 1
    agree = agree_high + agree_low
    disagree = disagree_high + disagree_low
    new_unique = new_unique_high + new_unique_low

    stats = {
        'rb3_vtables_via_rtti': len(rb3_vts),
        'rb3_classes_distinct': len(rb3_by_class),
        'dc3_classes_distinct': len(dc3_by_class),
        'common_classes': len(common_classes),
        'pairings_done': pairs_done,
        'multi_vt_classes_skipped': classes_with_multi_vt,
        'total_proposals': len(proposals),
        'new_proposals': new_unique,
        'new_proposals_HIGH_tier': new_unique_high,
        'new_proposals_LOW_tier': new_unique_low,
        'cross_verify_agree': agree,
        'cross_verify_disagree': disagree,
        'cross_verify_precision_pct':
            round(100 * agree / max(1, agree + disagree), 2),
        'HIGH_tier_precision_pct':
            round(100 * agree_high / max(1, agree_high + disagree_high), 2),
        'LOW_tier_precision_pct':
            round(100 * agree_low / max(1, agree_low + disagree_low), 2),
        'HIGH_tier_agree': agree_high,
        'HIGH_tier_disagree': disagree_high,
        'LOW_tier_agree': agree_low,
        'LOW_tier_disagree': disagree_low,
    }

    # Show sample disagreements to understand error mode
    sample_disagreements_high = []
    sample_disagreements_low = []
    for rb3_fn, (dc3_name, tier) in proposals.items():
        existing = rb3_to_dc3_name.get(rb3_fn.lower())
        if existing and existing != dc3_name:
            row = {
                'rb3_fn': rb3_fn,
                'callgraph_says': existing,
                'rtti_vt_says': dc3_name,
                'tier': tier,
            }
            if tier == 'HIGH' and len(sample_disagreements_high) < 10:
                sample_disagreements_high.append(row)
            elif tier == 'LOW' and len(sample_disagreements_low) < 10:
                sample_disagreements_low.append(row)

    # Sample HIGH-tier new proposals (highest-value subset)
    sample_new_high = [
        {'rb3_fn': k, 'dc3_name': v[0]}
        for k, v in proposals.items()
        if v[1] == 'HIGH' and k.lower() not in rb3_to_dc3_name
    ][:20]

    out = {
        'stats': stats,
        'sample_pairings': pairings[:25],
        'sample_disagreements_high_tier': sample_disagreements_high,
        'sample_disagreements_low_tier': sample_disagreements_low,
        'sample_new_proposals_high_tier': sample_new_high,
    }

    # Persist proposals (un-tuple for JSON)
    proposals_serial = {k: {'dc3_name': v[0], 'tier': v[1]}
                        for k, v in proposals.items()}
    json.dump(proposals_serial, open('/tmp/exploratory_rtti_vt.json', 'w'), indent=2)
    json.dump(out, open('/tmp/exploratory_rtti_vt_stats.json', 'w'), indent=2)

    print()
    print("=" * 70)
    print("RTTI + VTABLE COMBINED RESULTS")
    print("=" * 70)
    for k, v in stats.items():
        print(f"  {k}: {v}")
    print()
    print(f"Proposals: /tmp/exploratory_rtti_vt.json")
    print(f"Stats:     /tmp/exploratory_rtti_vt_stats.json")


if __name__ == '__main__':
    main()
