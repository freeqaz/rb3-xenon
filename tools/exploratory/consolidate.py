#!/usr/bin/env python3
"""Consolidate exploratory POC outputs into a single rank-ordered identifications
table for downstream production use (e.g., feeding into fingerprint_match's
unified_id.json after manual review).

Reads:
  /tmp/exploratory_callgraph.json
  /tmp/exploratory_vtable.json
  /tmp/exploratory_rtti_vt.json
  unified_id.json (the source of already-known mappings)

Writes:
  /tmp/exploratory_consolidated.json  (one entry per rb3 fn, with sources, tier)
"""

from __future__ import annotations
import json
from collections import defaultdict
from pathlib import Path

ROOT = Path("/home/free/code/milohax/rb3-xenon")


def norm(k):
    if k.startswith('fn_'):
        return f'fn_{k[3:].lower()}'
    return k


def main():
    unified = json.load(open(ROOT / 'unified_id.json'))
    known = {r['rb3_fn'].lower(): r.get('dc3_name')
             for r in unified if r.get('dc3_name')}

    cg = json.load(open('/tmp/exploratory_callgraph.json'))
    vt = json.load(open('/tmp/exploratory_vtable.json'))
    rt = json.load(open('/tmp/exploratory_rtti_vt.json'))

    # Aggregate proposals per rb3 fn
    proposals = defaultdict(lambda: {'sources': [], 'candidates': defaultdict(list)})

    # call-graph
    for k, v in cg.items():
        if v.get('already_in_unified_id'):
            continue
        if v.get('top_votes', 0) < 1:
            continue
        kk = norm(k)
        proposals[kk]['sources'].append('callgraph')
        proposals[kk]['candidates'][v['top_candidate']].append({
            'source': 'callgraph',
            'votes': v['top_votes'],
            'total_votes': v['total_votes'],
        })

    # vtable v1
    if 'proposals' in vt:
        for k, n in vt['proposals'].items():
            kk = norm(k)
            if kk in known:
                continue
            proposals[kk]['sources'].append('vtable_v1')
            proposals[kk]['candidates'][n].append({'source': 'vtable_v1'})

    # rtti_vt
    for k, info in rt.items():
        kk = norm(k)
        if kk in known:
            continue
        proposals[kk]['sources'].append(f'rtti_vt_{info["tier"]}')
        proposals[kk]['candidates'][info['dc3_name']].append({
            'source': 'rtti_vt',
            'tier': info['tier'],
        })

    # Rank by confidence:
    #   Tier S: consensus (≥2 sources agree on same dc3_name)
    #   Tier A: callgraph with ≥2 votes
    #   Tier B: RTTI HIGH-tier (slot count match)
    #   Tier C: single-source single-evidence (callgraph 1 vote OR RTTI LOW)
    out = {}
    tier_counts = defaultdict(int)
    for fn, info in proposals.items():
        cands = info['candidates']
        sources = set(info['sources'])
        # Pick best candidate
        # If multiple candidates, prefer the one with most distinct sources
        best_name = None
        best_evidence_count = 0
        for name, evs in cands.items():
            # Evidence count = number of distinct sources mentioning it
            distinct_src = len({e['source'] for e in evs})
            if distinct_src > best_evidence_count or (
                    distinct_src == best_evidence_count and not best_name):
                best_name = name
                best_evidence_count = distinct_src

        # Tiering
        consensus = sum(1 for c in cands.values() if len({e['source'] for e in c}) >= 2)
        has_multi_vote_cg = any(
            e.get('votes', 0) >= 2 for e in cands.get(best_name, [])
            if e.get('source') == 'callgraph'
        )
        has_rtti_high = any(
            e.get('tier') == 'HIGH' for e in cands.get(best_name, [])
            if e.get('source') == 'rtti_vt'
        )
        if consensus >= 1:
            tier = 'S'
        elif has_multi_vote_cg:
            tier = 'A'
        elif has_rtti_high:
            tier = 'B'
        else:
            tier = 'C'

        tier_counts[tier] += 1
        out[fn] = {
            'dc3_name_proposed': best_name,
            'tier': tier,
            'sources': sorted(sources),
            'n_candidates': len(cands),
        }

    json.dump(out, open('/tmp/exploratory_consolidated.json', 'w'), indent=2)
    print()
    print("=" * 70)
    print("CONSOLIDATED EXPLORATORY PROPOSALS")
    print("=" * 70)
    print(f"  unified_id baseline: {len(known)}")
    print(f"  total new proposals: {len(out)}")
    for tier in 'SABC':
        print(f"    Tier {tier}: {tier_counts[tier]}")
    print()
    print(f"  total after consolidation: {len(known) + len(out)}")
    print(f"  expansion: {100 * len(out) / len(known):.1f}% of unified_id")
    print()
    print("Output: /tmp/exploratory_consolidated.json")


if __name__ == '__main__':
    main()
