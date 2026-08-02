#!/usr/bin/env python3
"""Lane CK-2: PER-ROW definedness triage for a named set of map addresses.

WHY THIS EXISTS.  cj4_audit.py answers the whole-map question and prints a
census, but it only writes D/E/F rows to detail.json -- A, B, C and G rows
appear nowhere machine-readable.  CJ-4's per-row split of lane CI-2's 49 new
addresses therefore survives only in a commit message, and could not be
re-derived.  This tool emits the class of EVERY requested address.

It deliberately does NOT edit cj4_audit.py: lane CK-4 is working the same
files concurrently, and a new file cannot collide with its edits.

INSTRUMENT: identical to cj4_audit -- COFF symbol table via cj4_coff (a
substring/grep scan CANNOT settle definedness; an undefined external
*reference* carries the same name bytes as a definition), objdiff's
normalized name via cj4_norm as the pairing ruler, empirical home unit
(the unit whose dtk-split TARGET obj defines the name) with splits.txt
.text ranges as cross-check/fallback.

POSITIVE CONTROL (--control): re-derives the classes for every address in a
cj4_audit detail.json and asserts they agree.  This is a replicated cascade,
so it is only trustworthy if it reproduces the tool it replicates.  Run it.

Usage:
  ck2_triage.py <worktree> --addrs FILE.json  [--control DETAIL.json]
                           [--out OUT.json]
  FILE.json: either ["0x...", ...] or {"0x...": anything}
"""
import bisect
import collections
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cj4_coff as coff       # noqa: E402
import cj4_norm as norm       # noqa: E402


def build_index(wt):
    cfg = json.load(open(os.path.join(wt, 'objdiff.json')))
    units = [dict(name=u['name'],
                  tgt=os.path.join(wt, u['target_path']) if u.get('target_path') else None,
                  base=os.path.join(wt, u['base_path']) if u.get('base_path') else None)
             for u in cfg['units']]
    tgt_def, base_state = {}, {}
    for u in units:
        if u['tgt'] and os.path.exists(u['tgt']):
            try:
                c = coff.classify(open(u['tgt'], 'rb').read())
            except Exception:
                c = {}
            for n, st in c.items():
                if st == 'DEFINED':
                    tgt_def.setdefault(norm.key(n), []).append(u['name'])
        if u['base'] and os.path.exists(u['base']):
            try:
                base_state[u['name']] = {
                    norm.key(n): st for n, st
                    in coff.classify(open(u['base'], 'rb').read()).items()}
            except Exception:
                base_state[u['name']] = {}
    base_def_anywhere = collections.defaultdict(list)
    for un, d in base_state.items():
        for n, st in d.items():
            if st == 'DEFINED':
                base_def_anywhere[n].append(un)
    return tgt_def, base_state, base_def_anywhere


def build_span(wt):
    rng, cur = {}, None
    for line in open(os.path.join(wt, 'config/45410914/splits.txt')):
        m = re.match(r'^(\S.*):\s*$', line)
        if m:
            cur = None if m.group(1) == 'Sections' else m.group(1)
            continue
        if cur and '.text' in line:
            mm = re.search(r'start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)', line)
            if mm:
                rng.setdefault(cur, []).append(
                    (int(mm.group(1), 16), int(mm.group(2), 16)))
    span = [(a, b, u) for u, rs in rng.items() for a, b in rs]
    span.sort()
    return span


def classify_row(addr, name, tgt_def, base_state, base_def_anywhere, span):
    """VERBATIM the cj4_audit cascade. Returns (class, home)."""
    a = int(addr, 16)
    nk = norm.key(name)
    homes = tgt_def.get(nk, [])
    i = bisect.bisect_right(span, (a, float('inf'), '')) - 1
    su = span[i][2] if (i >= 0 and span[i][0] <= a < span[i][1]) else None
    su_unit = ('default/' + su[:-4]) if (su and su.endswith('.cpp')) else (
        ('default/' + su) if su else None)
    if len(homes) == 1:
        home = homes[0]
    elif su_unit is not None:
        cand = [h for h in homes if h == su_unit]
        home = cand[0] if cand else (su_unit if not homes else homes[0])
    else:
        home = homes[0] if homes else None

    if home is None:
        c = 'A_unpinned_no_target'
    elif home not in base_state:
        c = ('A_unpinned_no_target' if '/auto_' in home
             else 'B_home_not_compiled')
    else:
        st = base_state[home].get(nk)
        if st == 'DEFINED':
            c = 'C_ok_defined'
        elif st == 'WEAK':
            c = 'D_weak_external'
        elif st in ('UNDEF', 'COMMON'):
            c = 'E_undefined_ref'
        else:
            c = ('F_absent_but_defined_elsewhere'
                 if base_def_anywhere.get(nk) else 'G_absent_everywhere')
    return c, home, len(homes)


def main():
    wt = sys.argv[1]
    def opt(flag):
        return sys.argv[sys.argv.index(flag) + 1] if flag in sys.argv else None
    addrs_f, control_f, out_f = opt('--addrs'), opt('--control'), opt('--out')

    m = json.load(open(os.path.join(wt, 'scripts/target_symbol_map.json')))
    rows = {k: v for k, v in m.items()
            if k.lower().startswith('0x') and isinstance(v, str)}
    tgt_def, base_state, base_def_anywhere = build_index(wt)
    span = build_span(wt)
    print(f'map rows {len(rows)} | units with base obj {len(base_state)} '
          f'| .text spans {len(span)}')

    # ---- POSITIVE CONTROL: reproduce cj4_audit's own D/E/F assignments ----
    if control_f:
        det = json.load(open(control_f))
        agree = dis = 0
        bad = []
        for cls_name, entries in det.items():
            for addr, name, _home, _defs in entries:
                c, _, _ = classify_row(addr, rows.get(addr, name), tgt_def,
                                       base_state, base_def_anywhere, span)
                if c == cls_name:
                    agree += 1
                else:
                    dis += 1
                    bad.append((addr, cls_name, c))
        tot = agree + dis
        print(f'\n[CONTROL] replicated cascade vs cj4_audit detail.json: '
              f'{agree}/{tot} agree, {dis} DISAGREE (denominator {tot})')
        for b in bad[:10]:
            print('   DISAGREE', b)
        if dis:
            print('   ^ the replication is NOT faithful; do not trust the '
                  'triage below')

    if not addrs_f:
        return
    raw = json.load(open(addrs_f))
    want = list(raw) if isinstance(raw, (list, dict)) else []

    out, counts = {}, collections.Counter()
    for addr in want:
        name = rows.get(addr)
        if name is None:
            counts['Z_addr_absent_from_map'] += 1
            out[addr] = dict(name=None, cls='Z_addr_absent_from_map', home=None)
            continue
        c, home, nhomes = classify_row(addr, name, tgt_def, base_state,
                                       base_def_anywhere, span)
        counts[c] += 1
        out[addr] = dict(name=name, cls=c, home=home, n_homes=nhomes)

    print(f'\n=== PER-ROW TRIAGE (denominator {len(want)} requested rows) ===')
    for k in sorted(counts):
        print(f'  {k:34s} {counts[k]:4d}   {100.0*counts[k]/len(want):5.1f}%')
    print()
    for addr in sorted(out, key=lambda x: int(x, 16)):
        r = out[addr]
        print(f'  {addr}  {r["cls"]:32s} home={r["home"]}  '
              f'{(r["name"] or "")[:70]}')
    if out_f:
        json.dump(out, open(out_f, 'w'), indent=1)
        print(f'\ntriage -> {out_f}')


if __name__ == '__main__':
    main()
