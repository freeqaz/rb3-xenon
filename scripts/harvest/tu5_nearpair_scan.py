#!/usr/bin/env python3
"""LANE-B — near-pair ranking over the identification-NOMATCH residue.

For every unmapped target fn_ whose reloc-masked body matches NO base symbol
byte-identically (the ~5-6k 'nomatch' bucket left after the clean/ICF/twin
correlator passes), find the CLOSEST compiled base symbol in the same unit:

  candidate filter: |size delta| <= MAX_SIZE_DELTA (8), base name is a real
  function (no __unwind$/__ehhandler$), not already consumed as a map value,
  not an STL wall template, and reloc-name-sequence compatible (equal length
  and no resolved-position contradiction, OR small mismatch count recorded).

  score: number of differing 4-byte words after masking the UNION of both
  sides' reloc positions (so reloc placement differences don't count as code
  diffs), plus tail words for size delta.

Fewest-diff-words first = simultaneously an identification hypothesis AND a
crackable body diff (the diff words point at the diverging instructions).

Skips (wall classes, per campaign brief): EH funclets (40-44B, subi r12
signature), STL fold templates, targets already in map/denylist.

Usage: python3 tu5_nearpair_scan.py [pairs.json] [out_dir]
Outputs: nearpairs.json (ranked), summary printed.
"""
import json, sys, os, re, struct
from pathlib import Path
from collections import defaultdict, Counter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import tu5_reloc_seq as IR
from tu5_icf_disambiguate import (is_real_func, load_resolver, resolve_token,
                                  rds_of)

ROOT = '/home/free/code/milohax/rb3-xenon'   # objs read-only from main build
MAX_SIZE_DELTA = 8
MAX_DIFF_WORDS = 24        # keep candidates with <= this many differing words
STL_WALL = re.compile(r'_M_fill_insert|push_back|_Rb_tree|insert_unique|'
                      r'_M_insert_|_M_allocate|_M_deallocate|\?\?\$_')

def is_eh_funclet(body):
    if not (36 <= len(body) <= 48):
        return False
    if len(body) < 4:
        return False
    w0 = struct.unpack_from('>I', body, 0)[0]
    # addi r12,r12,-N  (subi r12 idiom): opcode 14, RT=12, RA=12
    return (w0 >> 16) == 0x398C

def word_diffs(a, b, mask_offs):
    """Count differing aligned words in min-length prefix, ignoring words
    touched by any reloc offset in mask_offs. Returns (ndiff, offsets)."""
    m = min(len(a), len(b)) & ~3
    masked_words = set()
    for o in mask_offs:
        masked_words.add(o & ~3)
        masked_words.add((o + 3) & ~3)
    diffs = []
    for o in range(0, m, 4):
        if o in masked_words:
            continue
        if a[o:o+4] != b[o:o+4]:
            diffs.append(o)
    return diffs

def main():
    pairs_path = sys.argv[1] if len(sys.argv) > 1 else '/home/free/tmp/correlator_sizing/pairs.json'
    out_dir = sys.argv[2] if len(sys.argv) > 2 else '/home/free/tmp/laneB_scan'
    os.makedirs(out_dir, exist_ok=True)
    os.chdir(ROOT)
    pairs = json.load(open(pairs_path))
    resolver = load_resolver(ROOT)

    mp = json.load(open(os.path.join(ROOT, 'scripts/target_symbol_map.json')))
    existing_keys = set(k.lower() for k in mp.keys())
    denylist = set(a.lower() for a in mp.get('_denylist', []))
    used_names = set(v for v in mp.values() if isinstance(v, str))

    r = json.load(open('build/45410914/report.json'))
    rep = {}
    for u in r['units']:
        rep[u['name']] = {f['name']: f.get('match_percent_normalized', 0)
                          for f in u.get('functions', [])}

    stats = Counter()
    results = []
    errors = []
    for p in pairs:
        unit = p['name']
        try:
            tgt = IR.func_reloc_seq(p['tgt'])
            base = IR.func_reloc_seq(p['baseobj'])
        except Exception as e:
            errors.append((unit, repr(e)))
            continue
        match_pct = rep.get(unit, {})
        base_real = {}
        base_by_content = defaultdict(list)
        for n, (body, seq) in base.items():
            if not is_real_func(n):
                continue
            base_by_content[body].append(n)
            base_real[n] = (body, seq)
        # size index for candidate lookup
        by_size = defaultdict(list)
        for n, (body, seq) in base_real.items():
            by_size[len(body)].append(n)

        for tn in sorted(tgt):
            if not tn.startswith('fn_'):
                continue
            addr = '0x' + tn[3:].lower()
            tbody, tseq = tgt[tn]
            stats['tgt_fn'] += 1
            if addr in existing_keys or addr in denylist:
                stats['skip_mapped_or_deny'] += 1
                continue
            cur = match_pct.get(tn, 0)
            if cur >= 100:
                stats['skip_at100'] += 1
                continue
            if base_by_content.get(tbody):
                stats['skip_exact_match_exists'] += 1
                continue   # other passes' territory
            if is_eh_funclet(tbody):
                stats['skip_eh_funclet'] += 1
                continue
            stats['nomatch_scanned'] += 1
            trds = [resolve_token(t, resolver)[0] for t in rds_of(tseq)]
            cands = []
            for dsz in range(-MAX_SIZE_DELTA, MAX_SIZE_DELTA + 1, 4):
                for bn in by_size.get(len(tbody) + dsz, []):
                    if bn in used_names:
                        continue
                    if STL_WALL.search(bn):
                        continue
                    bbody, bseq = base_real[bn]
                    brds = rds_of(bseq)
                    # reloc union mask (own offsets already zeroed in bodies;
                    # zero at the OTHER side's offsets so placement diffs
                    # don't count as code diffs)
                    offs = [x[0] for x in tseq] + [x[0] for x in bseq]
                    d = word_diffs(tbody, bbody, offs)
                    tailw = abs(dsz) // 4
                    score = len(d) + tailw
                    if score > MAX_DIFF_WORDS:
                        continue
                    # reloc-name compatibility on min length
                    n_reloc_mismatch = 0
                    for tpv, cpv in zip(trds, brds):
                        if tpv is not None and tpv != cpv:
                            n_reloc_mismatch += 1
                    cands.append(dict(base=bn, size_delta=dsz,
                                      diff_words=len(d), score=score,
                                      diff_offsets=d[:12],
                                      reloc_len_delta=len(brds) - len(trds),
                                      reloc_mismatch=n_reloc_mismatch,
                                      base_pct=match_pct.get(bn, None)))
            if not cands:
                stats['no_near_candidate'] += 1
                continue
            cands.sort(key=lambda c: (c['score'], abs(c['reloc_len_delta']),
                                      c['reloc_mismatch']))
            stats['ranked'] += 1
            results.append(dict(unit=unit, sp=p.get('sp'), fn=tn, fn_addr=addr,
                                size=len(tbody), cur_pct=cur,
                                nreloc=len(tseq),
                                best=cands[0], cands=cands[:3]))
    results.sort(key=lambda x: (x['best']['score'],
                                x['best']['reloc_mismatch'],
                                -x['size']))
    json.dump(results, open(os.path.join(out_dir, 'nearpairs.json'), 'w'), indent=1)
    json.dump(errors, open(os.path.join(out_dir, 'errors.json'), 'w'), indent=1)
    print('units:', len(pairs), 'errors:', len(errors))
    for k, v in sorted(stats.items()):
        print(f'  {k}: {v}')
    hist = Counter()
    for x in results:
        s = x['best']['score']
        hist[s if s <= 10 else '11+'] += 1
    print('score histogram:', dict(sorted(hist.items(), key=lambda kv: (isinstance(kv[0], str), kv[0]))))
    print('wrote', os.path.join(out_dir, 'nearpairs.json'), len(results), 'ranked targets')

if __name__ == '__main__':
    main()
