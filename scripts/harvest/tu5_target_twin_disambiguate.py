#!/usr/bin/env python3
"""LEVER 3 — target-side ICF-twin disambiguation (mirror of tu5_icf_disambiguate).

Base-side pass (tu5_icf_disambiguate.py) handles: one unmapped target fn_ whose
masked body matches >1 REAL base candidate. It DEFERS the mirror case:

  k >= 2 unmapped target fn_ share ONE identical masked body B ("target twins"),
  competing for the base candidate(s) with body B. From the base->target
  direction the pick is ambiguous (which base name goes to which twin), so those
  targets were skip_tgt_twin (~1,085 at the 16,996 baseline).

This pass resolves them from the TARGET->base direction, per the same
deterministic reloc-target-identity discriminator:

  For each twin ti, resolve its reloc-destination NAME sequence RDS_t_i
  (fn_/lbl_ tokens -> 0x<addr> looked up in target_symbol_map.json UNION
  config/45410914/symbols.txt; mangled/@comp.id/__real@ kept verbatim;
  unresolved -> None wildcard). Each REAL base candidate cj has RDS_cj (relocs
  name symbols directly). S_i = { cj : len equal AND RDS_t_i matches RDS_cj at
  every RESOLVED position }.

  A twin ti is RESOLVABLE iff S_i nonempty AND all members of S_i share ONE
  RDS (=> ti is pinned to exactly one reloc-name class c; every excluded
  candidate necessarily differs at a RESOLVED position -> hard evidence, never
  a wildcard). Wildcard-ambiguous twins (S spans >1 RDS) are dropped.

  Group resolvable twins and base candidates by RDS class. Within a class the
  base candidates are mutually byte+reloc identical, so ANY bijection T_c -> B_c
  is strict-safe AND semantically defensible (they are the same ICF-folded
  code with the same reloc targets). Emit a deterministic bijection
  (addr-sorted targets -> name-sorted free base names), capped at min(|T_c|,|B_c|).

Lever tags (per emitted pair):
  icf_identical       : ALL base candidates in `cands` share ONE RDS (pure fold;
                        target relocs did not need to select among classes).
  reloc_discriminated : candidates split into >1 RDS class; ti's resolved relocs
                        selected its class, excluding the others by hard evidence.

Precision guards (all deterministic, no scores):
  * base candidates filtered to REAL functions (drop __unwind$/__ehhandler$/...).
  * a twin emits only if pinned to exactly one RDS class (S single-RDS).
  * bijection uses DISTINCT base names (no dup value in fragment; skip any name
    already a map VALUE); excess twins beyond available distinct names skipped.
  * skip if the target addr is already a map key, or on the map _denylist.
  * global dedup: no two proposals pick the same name.

Usage:
  ICFDIS_PROJECT=<worktree> tu5_target_twin_disambiguate.py [pairs.json] [out_dir]
Outputs: proposals.json, map_fragment.json, per_unit.json, decisions.json, errors.json
"""
import json, sys, os, re
from pathlib import Path
from collections import defaultdict, Counter

ROOT = '/home/free/code/milohax/rb3-xenon'
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__))))
import tu5_reloc_seq as IR
# reuse the base-side helpers verbatim for exact parity
import tu5_icf_disambiguate as ID
from tu5_icf_disambiguate import (is_real_func, load_resolver, resolve_token,
                                   rds_of, match_candidate, killed_at_resolved)


def process_unit(p, resolver, rep, used_names):
    unit = p['name']
    tgt = IR.func_reloc_seq(p['tgt'])
    base = IR.func_reloc_seq(p['baseobj'])
    # base: real functions only, grouped by masked body content
    base_by_content = defaultdict(list)
    for n, (body, seq) in base.items():
        if is_real_func(n):
            base_by_content[body].append(n)
    # unmapped target fn_ grouped by masked body
    unmapped = {n: v for n, v in tgt.items() if n.startswith('fn_')}
    tgt_content = defaultdict(list)
    for n, (body, seq) in unmapped.items():
        tgt_content[body].append(n)
    match_pct = rep.get(unit, {})

    props = []
    decisions = []
    counts = defaultdict(int)

    for body, twins in tgt_content.items():
        if len(twins) < 2:
            continue  # not a target twin -> base-side pass territory
        counts['twin_groups'] += 1
        counts['twin_fns'] += len(twins)
        cands = base_by_content.get(body, [])
        if not cands:
            counts['nomatch_realbase_fns'] += len(twins)
            continue
        cand_rds = {c: rds_of(base[c][1]) for c in cands}
        all_cand_rds = set(cand_rds.values())
        base_lever = 'icf_identical' if len(all_cand_rds) == 1 else 'reloc_discriminated'

        # classify each twin -> pinned RDS class (or drop)
        pinned = {}   # fn -> class(rds tuple)
        rds_t_of = {}
        for n in sorted(twins):
            _, tseq = unmapped[n]
            rds_t = [resolve_token(tok, resolver)[0] for tok in rds_of(tseq)]
            rds_t_of[n] = rds_t
            S = [c for c in cands if match_candidate(rds_t, cand_rds[c])]
            if len(S) == 0:
                counts['skip_no_S'] += 1
                decisions.append(dict(unit=unit, fn=n, verdict='skip_no_S',
                                      rds_t=rds_t, cands=cands,
                                      cand_rds={c: list(cand_rds[c]) for c in cands}))
                continue
            s_rds = set(cand_rds[c] for c in S)
            if len(s_rds) != 1:
                counts['skip_S_disagree'] += 1
                decisions.append(dict(unit=unit, fn=n, verdict='skip_S_disagree',
                                      S=S, rds_t=rds_t, s_rds=[list(x) for x in s_rds]))
                continue
            excluded = [c for c in cands if c not in S]
            # defensive parity guard: every excluded killed at a RESOLVED position
            if not all(killed_at_resolved(rds_t, cand_rds[c]) for c in excluded):
                counts['skip_excluded_wildcard'] += 1
                decisions.append(dict(unit=unit, fn=n, verdict='skip_excluded_wildcard',
                                      S=S, excluded=excluded, rds_t=rds_t))
                continue
            pinned[n] = next(iter(s_rds))  # the single shared RDS

        if not pinned:
            continue
        # group resolvable twins + base candidates by RDS class
        tgt_by_class = defaultdict(list)
        for n, cls in pinned.items():
            tgt_by_class[cls].append(n)
        base_by_class = defaultdict(list)
        for c in cands:
            base_by_class[cand_rds[c]].append(c)

        for cls, tset in tgt_by_class.items():
            T_c = sorted(tset, key=lambda z: z[3:].lower())   # addr sort
            B_c = sorted(base_by_class.get(cls, []))          # name sort (deterministic)
            lever = base_lever
            # free base names: not already a map value, not already picked here
            for ti in T_c:
                addr = '0x' + ti[3:].lower()
                free = [b for b in B_c if b not in used_names]
                if not free:
                    counts['skip_names_exhausted'] += 1
                    continue
                pick = free[0]
                used_names.add(pick)   # consume within-run so bijection stays distinct
                B_c = [b for b in B_c if b != pick]
                cur = match_pct.get(ti, 0)
                counts['emit'] += 1
                counts['emit_' + lever] += 1
                if cur >= 100:
                    counts['emit_already100'] += 1
                props.append(dict(unit=unit, fn=ti, fn_addr=addr, pick=pick,
                                  ncands=len(cands), nclass=len(base_by_class),
                                  class_base_n=len(base_by_class.get(cls, [])),
                                  twin_k=len(twins), nreloc=len(rds_t_of[ti]),
                                  size=len(body), lever=lever, cur_pct=cur,
                                  rds_class=list(cls)))
    return props, decisions, dict(unit=unit, **counts)


def main():
    pairs_path = sys.argv[1] if len(sys.argv) > 1 else '/home/free/tmp/correlator_sizing/pairs.json'
    out_dir = sys.argv[2] if len(sys.argv) > 2 else '/home/free/tmp/tgttwin'
    project_dir = os.environ.get('ICFDIS_PROJECT', ROOT)
    os.chdir(project_dir)
    pairs = json.load(open(pairs_path))
    resolver = load_resolver(project_dir)

    _mp = json.load(open(os.path.join(project_dir, 'scripts/target_symbol_map.json')))
    existing_keys = set(k.lower() for k in _mp.keys())
    denylist = set(a.lower() for a in _mp.get('_denylist', []))
    # names already used as a map VALUE (duplicate-value hazard); consumed live too
    used_names = set(v for v in _mp.values() if isinstance(v, str))

    r = json.load(open('build/45410914/report.json'))
    rep = {}
    for u in r['units']:
        rep[u['name']] = {f['name']: f.get('match_percent_normalized', 0)
                          for f in u.get('functions', [])}

    all_props, all_dec, per_unit, errors = [], [], [], []
    for p in pairs:
        try:
            props, dec, cu = process_unit(p, resolver, rep, used_names)
        except Exception as e:
            errors.append((p['name'], repr(e)))
            continue
        all_props += props
        all_dec += dec
        per_unit.append(cu)

    # drop targets whose addr is already a map key or on the denylist
    filtered = []
    for x in sorted(all_props, key=lambda z: z['fn_addr']):
        if x['fn_addr'] in existing_keys:
            x['dropped'] = 'addr_already_mapped'; continue
        if x['fn_addr'] in denylist:
            x['dropped'] = 'denylist'; continue
        filtered.append(x)
    # global dedup: no two proposals may pick the same name
    seen = {}
    deduped = []
    for x in filtered:
        if x['pick'] in seen:
            x['dropped'] = 'dup_pick_name'; continue
        seen[x['pick']] = x['fn_addr']
        deduped.append(x)
    all_props = deduped

    os.makedirs(out_dir, exist_ok=True)
    yield_props = [x for x in all_props if x['cur_pct'] < 100]
    json.dump(all_props, open(os.path.join(out_dir, 'proposals.json'), 'w'), indent=1)
    json.dump(all_dec, open(os.path.join(out_dir, 'decisions.json'), 'w'), indent=1)
    json.dump(per_unit, open(os.path.join(out_dir, 'per_unit.json'), 'w'), indent=1)
    json.dump(errors, open(os.path.join(out_dir, 'errors.json'), 'w'), indent=1)
    frag = {x['fn_addr']: x['pick'] for x in yield_props}
    json.dump(frag, open(os.path.join(out_dir, 'map_fragment.json'), 'w'), indent=1)

    lv = Counter(x['lever'] for x in all_props)
    lvy = Counter(x['lever'] for x in yield_props)
    print('pairs:', len(pairs), 'errors:', len(errors))
    print('emit total:', len(all_props), 'yield(cur<100):', len(yield_props))
    print('lever(all):', dict(lv))
    print('lever(yield):', dict(lvy))
    agg = Counter()
    for cu in per_unit:
        for k, v in cu.items():
            if k != 'unit':
                agg[k] += v
    print('agg:', dict(agg))

if __name__ == '__main__':
    main()
