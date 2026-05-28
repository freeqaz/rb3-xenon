#!/usr/bin/env python3
"""Vtable-slot transitivity: identify fn_XXXXXXXX by matching vtable slot positions
between rb3 and dc3 binaries.

Method:
  1. Walk dc3's leaked .map for `??_7CLASSNAME@@6B@` records -> class+vtable VA.
  2. Read dc3's PE (ham_xbox_r.exe) at each vtable VA to extract slot fn VAs.
  3. Map each dc3 slot VA -> dc3 function name via the .map.
  4. For each rb3 vtable (parsed from rb3 .s rdata: `vftable_XXXXXXXX`), extract
     slot fn VAs (already in `fn_XXXXXXXX` form via the .s file references).
  5. Pair rb3 vtable with dc3 vtable by (a) matching class name from
     proposed_splits.txt where available, or (b) checking which rb3 functions in
     the slot list are already named-via-unified_id and matching against a dc3
     vtable whose slots match those names.
  6. For each matched vtable pair, propose rb3 slot[i] = dc3 slot[i].

Output:
  /tmp/exploratory_vtable.json       proposals
  /tmp/exploratory_vtable_stats.json summary
"""

from __future__ import annotations
import json
import re
import struct
import sys
from collections import defaultdict, Counter
from pathlib import Path

ROOT = Path("/home/free/code/milohax/rb3-xenon")
DC3 = Path("/home/free/code/milohax/dc3-decomp")

DC3_MAP = DC3 / "orig/373307D9/ham_xbox_r.map"
DC3_EXE = DC3 / "orig/373307D9/ham_xbox_r.exe"
RB3_EXE = ROOT / "orig/45410914/band.exe"
RB3_ASM = ROOT / "build/45410914/asm"
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


def get_text_bounds(sections):
    """Return (lo, hi) for .text section."""
    for name, va, sz, _ in sections:
        if name == '.text':
            return va, va + sz
    return 0x82330000, 0x82ee6c00


def make_va_reader(exe, sections):
    def read_va(va, n):
        for _name, sec_va, vsize, raw_off in sections:
            if sec_va <= va < sec_va + vsize:
                offset = raw_off + (va - sec_va)
                return exe[offset:offset + n]
        return None
    return read_va


def parse_dc3_map():
    """Return tuple (addr_to_name, vtable_list).

    addr_to_name: {rva: name} for all symbols.
    vtable_list: [(class_name, rva)] for ??_7...@@6B@ entries.
    """
    addr_to_name = {}
    vtables = []
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
                cls = vm.group(1)
                base = vm.group(2)  # may be empty or like '0DA@' (for sub-vtables)
                vtables.append((name, cls, base, rva))
    return addr_to_name, vtables


def vtable_size_estimate(rva, sorted_addrs):
    """Return likely slot count by looking at the next named symbol.

    sorted_addrs is a sorted list of all named symbol addresses.
    """
    import bisect
    i = bisect.bisect_right(sorted_addrs, rva)
    if i >= len(sorted_addrs):
        return 200
    return (sorted_addrs[i] - rva) // 4


def extract_dc3_vtable(rva, addr_to_name, sorted_addrs, read_va,
                       text_lo=0x82330000, text_hi=0x82ee6c00):
    """Read DC3 vtable at rva. Return list of (slot_index, slot_va, slot_name_or_None).

    Stops at first slot that is NOT a function pointer (i.e. not in .text range).
    This catches RTTI markers (0xffffffff, COL signature 0x19930522, etc.)
    """
    n_slots = min(200, vtable_size_estimate(rva, sorted_addrs))
    if n_slots <= 0:
        return []
    raw = read_va(rva, n_slots * 4)
    if raw is None or len(raw) < n_slots * 4:
        return []
    out = []
    for i in range(n_slots):
        slot_va = struct.unpack_from('>I', raw, i * 4)[0]
        # Stop at first non-text-range pointer (RTTI / COL data)
        if not (text_lo <= slot_va < text_hi):
            break
        slot_name = addr_to_name.get(slot_va)
        out.append((i, slot_va, slot_name))
    return out


def parse_rb3_vtables_from_asm():
    """Parse rb3 .s files for `.obj vftable_XXXXXXXX, global` records and their
    slot contents. Returns {vt_va: [fn_X or None per slot]}."""
    out = {}
    cur_va = None
    cur_slots = None
    re_vt = re.compile(r'^\.obj vftable_([0-9A-Fa-f]{8})')
    re_endobj = re.compile(r'^\.endobj')
    re_slot = re.compile(r'^\s*\.4byte\s+(\S+)')

    for s in sorted(RB3_ASM.glob("*rdata*.s")):
        with open(s) as f:
            for line in f:
                m = re_vt.match(line)
                if m:
                    cur_va = int(m.group(1), 16)
                    cur_slots = []
                    continue
                if cur_slots is not None:
                    if re_endobj.match(line):
                        out[cur_va] = cur_slots
                        cur_va = None
                        cur_slots = None
                        continue
                    sm = re_slot.match(line)
                    if sm:
                        cur_slots.append(sm.group(1).strip())
    return out


def main():
    print("[1/5] parsing dc3 .map...", file=sys.stderr)
    dc3_addr_to_name, dc3_vtables = parse_dc3_map()
    print(f"  dc3 symbols: {len(dc3_addr_to_name)}", file=sys.stderr)
    print(f"  dc3 vtables found in map: {len(dc3_vtables)}", file=sys.stderr)

    print("[2/5] parsing dc3 PE...", file=sys.stderr)
    dc3_exe, dc3_base, dc3_sections = parse_pe(DC3_EXE)
    dc3_read = make_va_reader(dc3_exe, dc3_sections)
    # Reverse map: dc3 fn_name -> rva (for cross-referencing names back to rb3)
    dc3_name_to_addr = {n: a for a, n in dc3_addr_to_name.items()}

    print("[3/5] extracting dc3 vtable contents...", file=sys.stderr)
    # dc3 vtable record: name -> [(idx, va, slot_name)]
    dc3_vt_contents = {}
    skipped_empty = 0
    sorted_addrs = sorted(dc3_addr_to_name.keys())
    text_lo, text_hi = get_text_bounds(dc3_sections)
    print(f"  text range: 0x{text_lo:08x}..0x{text_hi:08x}", file=sys.stderr)
    for name, cls, base, rva in dc3_vtables:
        slots = extract_dc3_vtable(rva, dc3_addr_to_name, sorted_addrs, dc3_read,
                                    text_lo=text_lo, text_hi=text_hi)
        if not slots:
            skipped_empty += 1
            continue
        dc3_vt_contents[name] = {
            'class': cls,
            'base': base,
            'rva': rva,
            'slots': slots,  # [(idx, va, name)]
        }
    print(f"  dc3 vtables with slots extracted: {len(dc3_vt_contents)}", file=sys.stderr)
    print(f"  skipped (empty / no following symbol): {skipped_empty}", file=sys.stderr)

    print("[4/5] parsing rb3 vtables from asm...", file=sys.stderr)
    rb3_vt = parse_rb3_vtables_from_asm()
    print(f"  rb3 vtables: {len(rb3_vt)}", file=sys.stderr)

    # Build unified_id reverse: rb3 fn (lowercase) -> dc3 name
    unified = json.load(open(UNIFIED))
    rb3_to_dc3_name = {r['rb3_fn'].lower(): r.get('dc3_name')
                       for r in unified if r.get('dc3_name')}

    print("[5/5] matching rb3 vtables to dc3 vtables via known slot fn names...",
          file=sys.stderr)
    # For each rb3 vtable, get the list of slot fn names (those we already know).
    # Match against dc3 vtables by intersection of named slots.
    # Once we have a confident pairing, propose names for the unknown slots.

    proposals = {}     # rb3 fn -> dc3 name (proposed)
    proposals_meta = []  # detailed records
    pairings = []
    rb3_class_evidence = {}

    # Pre-index dc3 vtables by their NAMED slot fn -> [vtable_name]
    dc3_slot_to_vts = defaultdict(list)
    for vt_name, info in dc3_vt_contents.items():
        for idx, va, sname in info['slots']:
            if sname is not None:
                dc3_slot_to_vts[sname].append(vt_name)

    matched_pairs = 0
    for rb3_va, rb3_slots in rb3_vt.items():
        if not rb3_slots:
            continue
        # Resolve known rb3 slot names via unified_id (case-insensitive)
        rb3_known = []
        for i, slot in enumerate(rb3_slots):
            if not slot.startswith('fn_'):
                continue
            dc3_name = rb3_to_dc3_name.get(slot.lower())
            if dc3_name:
                rb3_known.append((i, slot, dc3_name))
        if len(rb3_known) < 1:
            continue
        # Score each dc3 vtable by how many of its slots align with our known ones
        candidate_scores = Counter()
        for i, _rb3_fn, dc3_name in rb3_known:
            for vt_name in dc3_slot_to_vts.get(dc3_name, []):
                # Slot at position i in dc3 vt_name?
                info = dc3_vt_contents[vt_name]
                if i < len(info['slots']) and info['slots'][i][2] == dc3_name:
                    candidate_scores[vt_name] += 1
        if not candidate_scores:
            continue
        top_vt, top_score = candidate_scores.most_common(1)[0]
        info = dc3_vt_contents[top_vt]
        # Layout mismatch: if rb3 is longer than dc3, the extra slots are RB3-only
        # (unlikely — engine drift is usually new slots in dc3). If dc3 is longer,
        # rb3 may be the older version with fewer virtuals. Accept the smaller slot
        # count as the pairing range, but require known-slot evidence to dominate.
        n_pair = min(len(rb3_slots), len(info['slots']))
        # Also accept if rb3 known slots all align with dc3 at their indices
        all_known_aligned = all(
            i < len(info['slots']) and info['slots'][i][2] == dc3_name
            for i, _, dc3_name in rb3_known
        )
        if not all_known_aligned:
            continue
        size_mismatch = len(rb3_slots) != len(info['slots'])
        matched_pairs += 1
        # Propose names for unnamed rb3 slots
        new_props = []
        for i in range(n_pair):
            rb3_slot = rb3_slots[i]
            if not rb3_slot.startswith('fn_'):
                continue
            if rb3_slot.lower() in rb3_to_dc3_name:
                continue  # already known
            dc3_slot_name = info['slots'][i][2]
            if dc3_slot_name is None:
                continue
            # collide-resolve: prefer higher-evidence proposals
            existing = proposals.get(rb3_slot)
            if existing and existing != dc3_slot_name:
                # Already proposed something different; keep both? Track conflicts.
                continue
            proposals[rb3_slot] = dc3_slot_name
            new_props.append({
                'rb3_fn': rb3_slot,
                'dc3_name': dc3_slot_name,
                'slot_idx': i,
                'rb3_vt_va': f'0x{rb3_va:08x}',
                'dc3_vt': top_vt,
                'pairing_evidence': top_score,
                'pairing_class': info['class'],
            })
        pairings.append({
            'rb3_vt_va': f'0x{rb3_va:08x}',
            'dc3_vt_name': top_vt,
            'dc3_class': info['class'],
            'evidence_slots': top_score,
            'rb3_slot_count': len(rb3_slots),
            'dc3_slot_count': len(info['slots']),
            'size_mismatch': size_mismatch,
            'new_proposals': len(new_props),
            'alternative_top5': dict(candidate_scores.most_common(5)),
        })
        proposals_meta.extend(new_props)

    # Cross-verify proposals: of those rb3 fns where we already have a dc3 name
    # via unified_id, do they agree?
    cross_verify_agree = 0
    cross_verify_disagree = 0
    for rb3_fn, dc3_name in proposals.items():
        existing = rb3_to_dc3_name.get(rb3_fn.lower())
        if existing:
            if existing == dc3_name:
                cross_verify_agree += 1
            else:
                cross_verify_disagree += 1

    new_proposals = {k: v for k, v in proposals.items()
                     if k.lower() not in rb3_to_dc3_name}

    stats = {
        'dc3_vtables_in_map': len(dc3_vtables),
        'dc3_vtables_extracted': len(dc3_vt_contents),
        'rb3_vtables_parsed': len(rb3_vt),
        'rb3_dc3_vt_pairings': matched_pairs,
        'new_fn_identifications': len(new_proposals),
        'total_fn_identifications': len(proposals),
        'cross_verify_agree': cross_verify_agree,
        'cross_verify_disagree': cross_verify_disagree,
        'cross_verify_precision_pct':
            round(100 * cross_verify_agree / max(1, cross_verify_agree + cross_verify_disagree), 2),
    }

    out = {
        'stats': stats,
        'sample_pairings': pairings[:20],
        'sample_new_proposals': proposals_meta[:30],
    }
    json.dump({'proposals': proposals, 'meta': proposals_meta},
              open('/tmp/exploratory_vtable.json', 'w'), indent=2)
    json.dump(out, open('/tmp/exploratory_vtable_stats.json', 'w'), indent=2)

    print()
    print("=" * 70)
    print("VTABLE TRANSITIVITY RESULTS")
    print("=" * 70)
    for k, v in stats.items():
        print(f"  {k}: {v}")
    print()
    print(f"Proposals saved to /tmp/exploratory_vtable.json")
    print(f"Stats + samples saved to /tmp/exploratory_vtable_stats.json")


if __name__ == '__main__':
    main()
