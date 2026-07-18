#!/usr/bin/env python3
"""ICF-ambiguous disambiguation pass for the reloc-masked correlator.

Secondary vein after tu5_correlate_global_driver.py: converts a
PRECISION-SAFE subset of the MULTI (base-side ICF-ambiguous) pool into
landable map entries via the RELOC-TARGET-IDENTITY discriminator.

Background: tu5_reloc_masked_correlate pairs an unmapped target fn_<addr>
to a base compiled symbol by byte-identity AFTER masking reloc sites.
A target fn whose masked body matches >1 base symbol is "MULTI" (ICF-
ambiguous) and was excluded from the clean sweep. But the strict metric is
reloc-NORMALIZED, yet reloc TARGETS still distinguish twins: two funcs
byte-identical modulo relocs differ in WHAT their relocs point at.

Discriminator (deterministic, precision-first):
  For target A (real code fn_), resolve its reloc-destination NAME sequence
  (RDS_t): fn_<x>/lbl_<x> tokens -> 0x<x> looked up in
  (target_symbol_map.json UNION config/45410914/symbols.txt); already-mangled
  tokens (?..., @comp.id, __real@...) kept verbatim; unresolved -> None (wildcard).
  Each REAL base candidate Bi has its own RDS_i (base relocs name symbols directly).
  S = { Bi : len==len and RDS_i matches RDS_t at every RESOLVED position }.
  EMIT A->pick iff:
    * S nonempty,
    * all members of S share an IDENTICAL RDS_i (=> mutually byte+reloc
      identical => arbitrary pick is strict-safe; this is the ICF-fold case),
    * every EXCLUDED base candidate differs from RDS_t at a RESOLVED (non-None)
      position (excluded by hard evidence, never by a wildcard).
  Base candidates are filtered to REAL functions first: __unwind$/__ehhandler$
  and other non-function COMDATs are dropped (they carry CODE+fn flags in MSVC
  X360 objs and otherwise pollute the candidate sets).

Lever tags:
  unwind_filter_clean : exactly 1 real base candidate after unwind filter
                        (the MULTI was pure unwind noise; RDS still checked).
  icf_identical       : >1 real candidate but all in S share one RDS (true fold).
  reloc_discriminated : >1 real candidate, reloc targets uniquely select S.

Usage:
  tu5_icf_disambiguate.py [pairs.json] [out_dir]
Outputs in out_dir: proposals.json, map_fragment.json, per_unit.json, decisions.json
"""
import json, sys, os, re
from pathlib import Path
from collections import defaultdict

ROOT = '/home/free/code/milohax/rb3-xenon'

sys.path.insert(0, os.path.join(ROOT, 'scripts/harvest'))
sys.path.insert(0, '/home/free/tmp/icfdis')
import tu5_reloc_seq as IR  # func_reloc_seq + parse (self-contained)

NON_FUNC_PREFIX = ('__unwind', '__ehhandler', '__catch$', '__unwindfunclet')

def is_real_func(name):
    if name.startswith(NON_FUNC_PREFIX):
        return False
    return True

def load_resolver(project_dir):
    """addr(lowercase 0xhex) -> name, from symbols.txt UNION target_symbol_map.json."""
    res = {}
    # symbols.txt: 'NAME = .sec:0xADDR; // ...'
    sp = os.path.join(project_dir, 'config/45410914/symbols.txt')
    rx = re.compile(r'^(.+?)\s*=\s*\.[\w.$]+:0x([0-9A-Fa-f]+);')
    with open(sp) as f:
        for line in f:
            m = rx.match(line)
            if m:
                res.setdefault('0x' + m.group(2).lower(), m.group(1).strip())
    # map wins for game names (override)
    mp = json.load(open(os.path.join(project_dir, 'scripts/target_symbol_map.json')))
    for a, n in mp.items():
        res[a.lower()] = n
    return res

TOKEN_ADDR = re.compile(r'^(?:fn|lbl|jmp|dbl|flt|dat|off)_([0-9A-Fa-f]{8})$')

def resolve_token(tok, resolver):
    """Return (resolved_name_or_None, is_wildcard)."""
    m = TOKEN_ADDR.match(tok)
    if m:
        a = '0x' + m.group(1).lower()
        nm = resolver.get(a)
        return (nm, nm is None)   # unresolved fn_/lbl_ => wildcard
    # already a real symbol name / @comp.id / __real@ / string label already named
    return (tok, False)

def rds_of(seq):
    return tuple(t[1] for t in seq)  # names only, offset-sorted already

def match_candidate(rds_t_resolved, cand_rds):
    """rds_t_resolved: list of (name_or_None). cand_rds: tuple of names.
    True if len equal and every RESOLVED target position equals candidate."""
    if len(rds_t_resolved) != len(cand_rds):
        return None  # length differs
    excluded_at_resolved = False
    for tp, cp in zip(rds_t_resolved, cand_rds):
        if tp is None:
            continue  # wildcard
        if tp != cp:
            excluded_at_resolved = True
    return not excluded_at_resolved  # True in S; if False, record whether killed at resolved
def killed_at_resolved(rds_t_resolved, cand_rds):
    if len(rds_t_resolved) != len(cand_rds):
        return True  # length mismatch = hard evidence
    for tp, cp in zip(rds_t_resolved, cand_rds):
        if tp is not None and tp != cp:
            return True
    return False

def process_unit(p, resolver, rep, used_names):
    unit = p['name']
    tgt = IR.func_reloc_seq(p['tgt'])
    base = IR.func_reloc_seq(p['baseobj'])
    # base: real functions only, group by masked body content
    base_by_content = defaultdict(list)
    for n, (body, seq) in base.items():
        if is_real_func(n):
            base_by_content[body].append(n)
    # target content multiplicity among unmapped real fn_
    unmapped = {n: v for n, v in tgt.items() if n.startswith('fn_')}
    tgt_content = defaultdict(list)
    for n, (body, seq) in unmapped.items():
        tgt_content[body].append(n)
    match_pct = rep.get(unit, {})
    props = []
    decisions = []
    counts = defaultdict(int)
    for n in sorted(unmapped):
        body, tseq = unmapped[n]
        cands = base_by_content.get(body, [])
        # target-side twin? handled as amb_tgt separately (skip here for base MULTI)
        tgt_twin = len(tgt_content[body]) > 1
        if len(cands) == 0:
            counts['nomatch_realbase'] += 1
            continue
        # resolve target RDS
        rds_t = [resolve_token(tok, resolver)[0] for tok in rds_of(tseq)]
        # candidate RDS
        cand_rds = {c: rds_of(base[c][1]) for c in cands}
        S = [c for c in cands if match_candidate(rds_t, cand_rds[c])]
        excluded = [c for c in cands if c not in S]
        cur = match_pct.get(n, 0)
        if len(cands) == 1:
            counts['single_realcand'] += 1
            # unwind_filter_clean: still require RDS consistency (S nonempty)
            if len(S) == 1 and not tgt_twin:
                lever = 'unwind_filter_clean'
            else:
                decisions.append(dict(fn=n, verdict='skip_single_rds_mismatch', cands=cands,
                                      rds_t=rds_t, cand_rds={c: list(cand_rds[c]) for c in cands}))
                counts['skip_rds_mismatch'] += 1
                continue
        else:
            if tgt_twin:
                counts['skip_tgt_twin'] += 1
                continue
            if len(S) == 0:
                counts['skip_no_S'] += 1
                continue
            # all S share identical RDS?
            s_rds = set(cand_rds[c] for c in S)
            if len(s_rds) != 1:
                counts['skip_S_disagree'] += 1
                decisions.append(dict(fn=n, verdict='skip_S_disagree', S=S,
                                      rds_t=rds_t, s_rds=[list(x) for x in s_rds]))
                continue
            # every excluded killed at a RESOLVED position?
            if not all(killed_at_resolved(rds_t, cand_rds[c]) for c in excluded):
                counts['skip_excluded_wildcard'] += 1
                decisions.append(dict(fn=n, verdict='skip_excluded_wildcard', S=S,
                                      excluded=excluded, rds_t=rds_t,
                                      cand_rds={c: list(cand_rds[c]) for c in cands}))
                continue
            lever = 'icf_identical' if len(S) > 1 or len(set(cand_rds[c] for c in cands)) == 1 else 'reloc_discriminated'
            # refine: reloc_discriminated only when there existed a real excluded competitor
            if excluded:
                lever = 'reloc_discriminated' if len(s_rds) == 1 and len(cands) > len(S) else lever
        # prefer an S-member name not already used as a map value; drop if all collide
        free = [c for c in sorted(S) if c not in used_names]
        if not free:
            counts['skip_all_S_name_used'] += 1
            continue
        pick = free[0]
        counts['emit'] += 1
        counts['emit_' + lever] += 1
        if cur >= 100:
            counts['emit_already100'] += 1
        props.append(dict(unit=unit, fn=n, fn_addr='0x' + n[3:].lower(),
                          pick=pick, ncands=len(cands), nS=len(S),
                          nreloc=len(tseq), size=len(body), lever=lever,
                          cur_pct=cur, S=S))
    return props, decisions, dict(unit=unit, **counts)

def main():
    pairs_path = sys.argv[1] if len(sys.argv) > 1 else '/home/free/tmp/correlator_sizing/pairs.json'
    out_dir = sys.argv[2] if len(sys.argv) > 2 else '/home/free/tmp/icfdis'
    project_dir = os.environ.get('ICFDIS_PROJECT', ROOT)
    os.chdir(project_dir)
    pairs = json.load(open(pairs_path))
    resolver = load_resolver(project_dir)
    # names already used as a map VALUE (ICF-fold hazard: duplicate values can
    # collide within one target obj). Prefer an S-member not in this set.
    _mp = json.load(open(os.path.join(project_dir, 'scripts/target_symbol_map.json')))
    used_names = set(v for v in _mp.values() if isinstance(v, str))
    r = json.load(open('build/45410914/report.json'))
    rep = {}
    for u in r['units']:
        rep[u['name']] = {f['name']: f.get('match_percent_normalized', 0) for f in u.get('functions', [])}
    all_props = []
    all_dec = []
    per_unit = []
    errors = []
    for p in pairs:
        try:
            props, dec, cu = process_unit(p, resolver, rep, used_names)
        except Exception as e:
            errors.append((p['name'], repr(e)))
            continue
        all_props += props
        all_dec += dec
        per_unit.append(cu)
    # global dedup: no two proposals may pick the same name (would create a
    # duplicate map value across addresses). Keep the smaller address deterministically.
    seen = {}
    deduped = []
    for x in sorted(all_props, key=lambda z: z['fn_addr']):
        if x['pick'] in seen:
            x['dropped'] = 'dup_pick_name'
            continue
        seen[x['pick']] = x['fn_addr']
        deduped.append(x)
    all_props = deduped
    os.makedirs(out_dir, exist_ok=True)
    # only emit yield (cur<100) into map fragment; keep all in proposals
    yield_props = [x for x in all_props if x['cur_pct'] < 100]
    json.dump(all_props, open(os.path.join(out_dir, 'proposals.json'), 'w'), indent=1)
    json.dump(all_dec, open(os.path.join(out_dir, 'decisions.json'), 'w'), indent=1)
    json.dump(per_unit, open(os.path.join(out_dir, 'per_unit.json'), 'w'), indent=1)
    json.dump(errors, open(os.path.join(out_dir, 'errors.json'), 'w'), indent=1)
    frag = {x['fn_addr']: x['pick'] for x in yield_props}
    json.dump(frag, open(os.path.join(out_dir, 'map_fragment.json'), 'w'), indent=1)
    from collections import Counter
    lv = Counter(x['lever'] for x in all_props)
    lvy = Counter(x['lever'] for x in yield_props)
    print('pairs:', len(pairs), 'errors:', len(errors))
    print('emit total:', len(all_props), 'yield(cur<100):', len(yield_props))
    print('lever(all):', dict(lv))
    print('lever(yield):', dict(lvy))
    # aggregate skip reasons
    agg = Counter()
    for cu in per_unit:
        for k, v in cu.items():
            if k != 'unit':
                agg[k] += v
    print('agg:', dict(agg))

if __name__ == '__main__':
    main()
