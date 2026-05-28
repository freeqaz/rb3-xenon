#!/usr/bin/env python3
"""Call-graph triangulation: identify fn_XXXXXXXX by matching call-site positions
between rb3 asm and dc3 asm.

Method:
  1. For each rb3 function F_rb3 that is named (via unified_id.json -> dc3 name),
     locate the corresponding dc3 function F_dc3 in dc3's .s files.
  2. Walk the bl instructions in both functions in order. At each bl position k:
       - rb3 calls fn_X (anonymous)
       - dc3 calls Bar (named)
     -> propose identification rb3 fn_X == dc3 "Bar"
  3. A single fn_X may receive proposals from many call sites. We collect votes
     and report (a) majority winners (b) unanimous winners.

Outputs:
  /tmp/exploratory_callgraph.json     proposed identifications keyed by rb3_fn
  /tmp/exploratory_callgraph_stats.json  summary stats

This is read-only: no build is run.
"""

from __future__ import annotations
import json
import os
import re
import sys
from collections import defaultdict, Counter
from pathlib import Path

ROOT = Path("/home/free/code/milohax/rb3-xenon")
DC3 = Path("/home/free/code/milohax/dc3-decomp")

RB3_ASM = ROOT / "build/45410914/asm"
DC3_ASM = DC3 / "build/373307D9/asm"
UNIFIED = ROOT / "unified_id.json"
REPORT = ROOT / "build/45410914/report.json"
DC3_MAP = DC3 / "orig/373307D9/ham_xbox_r.map"


FN_RE = re.compile(r'^\.fn\s+(\S+?),')
ENDFN_RE = re.compile(r'^\.endfn\b')
# matches: /* 82758434 00753C34  4B BA 67 1D */<tab>bl fn_822FEB50
# or for dc3 named-tag form bl "?Foo@@..."
BL_RE = re.compile(
    r'/\*\s+([0-9A-Fa-f]{8})\s+\S+\s+\S+ \S+ \S+ \S+ \*/\s+bl\s+(\S.*)$'
)


def parse_dc3_map():
    """Return dict {rva_int: name}."""
    out = {}
    pub_re = re.compile(r'^\s+([0-9a-f]{4}):([0-9a-f]{8})\s+(\S+)\s+([0-9a-f]{8})')
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
            if rva >= 0x82000000 and rva not in out:
                out[rva] = name
    return out


def parse_asm_file(path):
    """Yield (fn_name_or_anon, start_addr_int_or_None, [(offset, target_str)], ...)

    For .s files, each function begins at '.fn NAME, ...' and ends at .endfn.
    Each bl encountered between start..end records (rel_offset_bytes, target).
    The function start_addr is parsed from the leading comment of its first
    instruction line.
    """
    fns = []  # list of dicts: {name, start, bl: [(offset, target)]}
    cur = None
    first_addr = None
    with open(path) as f:
        for line in f:
            m = FN_RE.match(line)
            if m:
                if cur is not None:
                    fns.append(cur)
                cur = {'name': m.group(1).strip('"'), 'start': None, 'bl': []}
                first_addr = None
                continue
            if ENDFN_RE.match(line):
                if cur is not None:
                    fns.append(cur)
                cur = None
                continue
            if cur is None:
                continue
            # Look for address comment of any instruction (to grab first addr).
            am = re.match(r'/\*\s+([0-9A-Fa-f]{8})', line)
            if am and cur['start'] is None:
                cur['start'] = int(am.group(1), 16)
            blm = BL_RE.search(line)
            if blm:
                ins_addr = int(blm.group(1), 16)
                tgt = blm.group(2).strip()
                # Strip quotes
                tgt = tgt.strip('"')
                if cur['start'] is not None:
                    off = ins_addr - cur['start']
                    cur['bl'].append((off, tgt))
        if cur is not None:
            fns.append(cur)
    return fns


def main():
    print("[1/5] loading unified_id.json...", file=sys.stderr)
    unified = json.load(open(UNIFIED))
    # rb3_addr -> dc3_name
    rb3_to_dc3 = {int(r['rb3_addr'], 16): r.get('dc3_name')
                  for r in unified if r.get('dc3_name')}
    print(f"  unified entries with dc3_name: {len(rb3_to_dc3)}", file=sys.stderr)

    print("[2/5] loading rb3 report.json (fn name -> addr)...", file=sys.stderr)
    report = json.load(open(REPORT))
    # addr -> name (from report, includes 'fn_XXXX' anonymous + named)
    rb3_addr_to_fnname = {}
    for u in report['units']:
        for f in u.get('functions', []):
            addr = int(f.get('address', 0))
            # The 'address' in report.json may not be the VA. Use the fn name.
            n = f.get('name', '')
            if n.startswith('fn_') and len(n) == 11:
                va = int(n[3:], 16)
                rb3_addr_to_fnname[va] = n

    print("[3/5] parsing rb3 asm files...", file=sys.stderr)
    # rb3_fn_va -> {name, start, bl}
    rb3_fns = {}
    for s in sorted(RB3_ASM.glob("*.s")):
        for fn in parse_asm_file(s):
            if fn['name'].startswith('fn_') and fn['start'] is not None:
                rb3_fns[fn['start']] = fn
            elif fn['start'] is not None:
                # named rb3 functions also exist (e.g. ChannelData::SetSlippable)
                rb3_fns[fn['start']] = fn
    print(f"  rb3 fns parsed: {len(rb3_fns)}", file=sys.stderr)

    print("[4/5] parsing dc3 asm files (recursive)...", file=sys.stderr)
    # dc3 name -> {start, bl} (some functions have aliases; first wins)
    dc3_by_name = {}
    dc3_files = list(DC3_ASM.rglob("*.s"))
    print(f"  dc3 .s files: {len(dc3_files)}", file=sys.stderr)
    for s in sorted(dc3_files):
        for fn in parse_asm_file(s):
            if fn['start'] is None:
                continue
            n = fn['name']
            if n not in dc3_by_name:
                dc3_by_name[n] = fn
    print(f"  dc3 named fns parsed: {len(dc3_by_name)}", file=sys.stderr)

    # Also need: dc3 address -> dc3 name (for resolving named bl targets that are
    # just addresses, plus inline calls).
    dc3_map = parse_dc3_map()
    print(f"  dc3 map symbols: {len(dc3_map)}", file=sys.stderr)

    print("[5/5] triangulating...", file=sys.stderr)
    # rb3_fn_anon -> Counter({dc3_name: votes})
    votes = defaultdict(Counter)
    pairings_attempted = 0
    pairings_aligned = 0

    for rb3_addr, dc3_name in rb3_to_dc3.items():
        rb3_fn = rb3_fns.get(rb3_addr)
        dc3_fn = dc3_by_name.get(dc3_name)
        if rb3_fn is None or dc3_fn is None:
            continue
        rb3_bls = rb3_fn['bl']
        dc3_bls = dc3_fn['bl']
        # If both call lists are the same length, align positionally.
        # Otherwise skip (would need more sophisticated alignment).
        pairings_attempted += 1
        if len(rb3_bls) != len(dc3_bls):
            continue
        pairings_aligned += 1
        for (off_r, tgt_r), (off_d, tgt_d) in zip(rb3_bls, dc3_bls):
            if not tgt_r.startswith('fn_'):
                continue  # rb3 side already named or labelled
            if tgt_d.startswith('fn_') or tgt_d.startswith('lbl_'):
                continue  # dc3 side also anonymous -> can't help
            if tgt_d in ('__savegprlr_27', '__savegprlr_29'):
                # CRT helpers; verify it matches expected on rb3 side
                pass
            votes[tgt_r][tgt_d] += 1

    # Decide proposals.
    proposals = {}
    unanimous = {}
    majority = {}
    contested = {}
    for fn_anon, ct in votes.items():
        total = sum(ct.values())
        winner, w_votes = ct.most_common(1)[0]
        # Cross-verify: is fn_anon NOT already in unified_id?
        # rb3 fn names may be of the form 'fn_82725930' or 'fn_82725930+0x20'.
        # Only the bare form is canonical; offsets indicate intra-function jumps.
        addr_part = fn_anon[3:]
        if '+' in addr_part or '-' in addr_part:
            continue  # skip non-canonical call targets
        try:
            rb3_va = int(addr_part, 16)
        except ValueError:
            continue
        already_named = rb3_va in rb3_to_dc3
        entry = {
            'rb3_fn': fn_anon,
            'rb3_addr': f'0x{rb3_va:08x}',
            'top_candidate': winner,
            'top_votes': w_votes,
            'total_votes': total,
            'alternatives': dict(ct.most_common(5)),
            'already_in_unified_id': already_named,
            'already_in_unified_id_as': rb3_to_dc3.get(rb3_va) if already_named else None,
        }
        proposals[fn_anon] = entry
        if len(ct) == 1:
            unanimous[fn_anon] = entry
        elif w_votes / total >= 0.5:
            majority[fn_anon] = entry
        else:
            contested[fn_anon] = entry

    # Cross-verification: of the unanimous proposals, how many *agree with*
    # the existing unified_id entry?
    verify_agree = 0
    verify_disagree = 0
    for fn_anon, e in unanimous.items():
        if e['already_in_unified_id']:
            if e['already_in_unified_id_as'] == e['top_candidate']:
                verify_agree += 1
            else:
                verify_disagree += 1

    # New proposals = unanimous AND not already in unified_id
    new_unanimous = {k: v for k, v in unanimous.items()
                     if not v['already_in_unified_id']}
    new_majority = {k: v for k, v in majority.items()
                    if not v['already_in_unified_id']}

    stats = {
        'unified_id_size': len(unified),
        'rb3_named_to_dc3_mapping_count': len(rb3_to_dc3),
        'rb3_fns_parsed': len(rb3_fns),
        'dc3_named_fns_parsed': len(dc3_by_name),
        'pairings_attempted': pairings_attempted,
        'pairings_aligned': pairings_aligned,
        'pairings_alignment_rate_pct': round(100 * pairings_aligned /
                                             max(1, pairings_attempted), 2),
        'fn_anon_with_any_vote': len(votes),
        'unanimous_proposals': len(unanimous),
        'majority_proposals': len(majority),
        'contested_proposals': len(contested),
        'unanimous_cross_verify_agree': verify_agree,
        'unanimous_cross_verify_disagree': verify_disagree,
        'unanimous_cross_verify_precision_pct':
            round(100 * verify_agree / max(1, verify_agree + verify_disagree), 2),
        'new_unanimous_proposals': len(new_unanimous),
        'new_majority_proposals': len(new_majority),
    }

    out = {
        'stats': stats,
        'sample_new_unanimous': list(new_unanimous.values())[:30],
        'sample_unanimous_disagreements': [v for v in unanimous.values()
                                           if v['already_in_unified_id']
                                           and v['already_in_unified_id_as'] != v['top_candidate']][:10],
    }
    json.dump(proposals, open('/tmp/exploratory_callgraph.json', 'w'), indent=2)
    json.dump(out, open('/tmp/exploratory_callgraph_stats.json', 'w'), indent=2)

    print()
    print("=" * 70)
    print("CALL-GRAPH TRIANGULATION RESULTS")
    print("=" * 70)
    for k, v in stats.items():
        print(f"  {k}: {v}")
    print()
    print(f"Proposals saved to /tmp/exploratory_callgraph.json ({len(proposals)} entries)")
    print(f"Stats + samples saved to /tmp/exploratory_callgraph_stats.json")


if __name__ == '__main__':
    main()
