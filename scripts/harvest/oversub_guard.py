#!/usr/bin/env python3
"""oversub_guard.py -- measure (and gate on) OBJDIFF FUNCLET OVER-SUBSCRIPTION.

WHY THIS EXISTS
---------------
`matched_functions` can be inflated without a single new line of source.

objdiff's funclet pairing (`pair_funclets_by_bytes` in
`../objdiff/objdiff-core/src/diff/mod.rs`) pairs *anonymous* target symbols
(`fn_<addr>`, `__unwind$N`, `__catch$N`, `??__E*`, `??__F*`) to base symbols by
**reloc-masked byte signature**, not by name.  Pass 2 does that 1:1.  **Pass 2b**
(objdiff commit `48a5255`, "pair over-subscribed byte-identical funclets
many-to-one") then takes every *remaining* target funclet whose signature exists
on the base side and pairs it **many-to-one** onto an already-consumed base
symbol.  Each such target is credited 100%.

Consequence: if a `.text` span absorbs N byte-identical EH funclets but the
claimant's own compiled obj emits only M < N of them, the report credits N.
The N-M surplus is machine code we never generated.  Measured tree-wide on
`559645e9`: **1,565 of 39,520 matched functions (3.96%)** are pass-2b surplus,
spread over 196 units and 138 splits-landing commits.  100% of the surplus is
in anonymous `fn_` symbols; named-function matches are untouched.

THE SUPPLY RULE
---------------
A unit may honestly claim, for each reloc-masked funclet signature S, at most as
many target funclets as its own base obj *supplies* symbols with signature S.

    excess(unit) = SUM over S of max(0, demand_target(S) - supply_base(S))

(Symbols whose name exists on both sides are excluded: those name-pair in an
earlier objdiff pass and never reach the funclet pool.)

This is the correct form of "a span must not push a unit past its own obj's
function supply".  The naive form -- compare `matched_functions` to the base
obj's total function-symbol count -- is worthless here: base objs carry hundreds
to thousands of inline/template COMDAT function symbols, so *zero* units trip it
(measured: 0/3881), while the per-signature rule finds all 196.

VALIDATION (2026-07-29, worktree `oversub`, main @ 559645e9)
-----------------------------------------------------------
Ground truth was obtained by instrumenting objdiff itself (a private build; the
fleet binary was NOT touched) to log every pass-2b pairing and to allow
disabling pass 2b:

    OBJDIFF_OVERSUB_DUMP=1 objdiff-cli report generate   -> 1,617 pairings
    OBJDIFF_OVERSUB_OFF=1  objdiff-cli report generate   -> 37,955 matched
                                              (vs 39,520 reported, delta -1,565)

This script, reading only the two `.obj` files, predicts **1,618** total and is
exact on 178/204 units, with **0 false negatives** (no unit with real
over-subscription is predicted clean).  The residual few-unit skew comes from
pass-3 fallout after pass 2b is removed, not from signature mis-computation.

USAGE
  oversub_guard.py --census [--top 25] [--json out.json]
  oversub_guard.py --unit default/VocalTrackDir
  oversub_guard.py --baseline before.json          # snapshot current state
  oversub_guard.py --verify   before.json [--allow 0]
      exit 0 = no growth; exit 3 = a landing grew over-subscription (FAKE MATCHES)

All modes take `--worktree DIR` (default `.`) and read `objdiff.json` there.
Requires the objs to be built: run `./tools/ninja-locked` first.
"""
import argparse
import collections
import json
import os
import re
import struct
import sys

# Mirror of objdiff's `is_funclet_like` (objdiff-core/src/diff/mod.rs).
FUNCLET_RE = re.compile(
    r'(fn_[0-9A-Fa-f]{8}|__unwind\$\d+|__catch\$\d+|__unwind__merged_.*|\?\?__[EF].*)\Z')

IMAGE_SCN_CNT_CODE = 0x00000020
IMAGE_SCN_LNK_NRELOC_OVFL = 0x01000000
# COFF storage classes that denote a defined symbol we care about.
DEFINED_CLASSES = (2, 3, 6, 105)   # EXTERNAL, STATIC, LABEL, CLR_TOKEN-ish
IMAGE_SYM_TYPE_FUNC = 0x20


def parse_coff(path):
    """Minimal COFF reader -> (sections, symbols)."""
    with open(path, 'rb') as f:
        d = f.read()
    if len(d) < 20:
        return [], []
    _machine, nsec, _ts, symptr, nsym, opt, _ch = struct.unpack_from('<HHIIIHH', d, 0)
    if not symptr or not nsym:
        return [], []
    strtab = symptr + nsym * 18

    def strtab_name(off):
        e = d.index(b'\x00', strtab + off)
        return d[strtab + off:e].decode('utf8', 'replace')

    sections = []
    for i in range(nsec):
        off = 20 + opt + i * 40
        raw = d[off:off + 8]
        if raw[:1] == b'/':
            name = strtab_name(int(raw[1:].rstrip(b'\0').decode()))
        else:
            name = raw.rstrip(b'\0').decode('utf8', 'replace')
        _vs, _va, sz, praw, prel, _pln, nrel, _nln, flags = \
            struct.unpack_from('<IIIIIIHHI', d, off + 8)
        data = d[praw:praw + sz] if praw else b''
        relocs = []
        if prel and nrel:
            n, base = nrel, prel
            if flags & IMAGE_SCN_LNK_NRELOC_OVFL:
                n = struct.unpack_from('<I', d, prel)[0] - 1
                base = prel + 10
            for k in range(n):
                rva = struct.unpack_from('<I', d, base + k * 10)[0]
                relocs.append(rva)
        sections.append({'name': name, 'size': sz, 'data': data,
                         'relocs': relocs, 'flags': flags})

    syms, i = [], 0
    while i < nsym:
        off = symptr + i * 18
        raw = d[off:off + 8]
        val, secnum, typ, sclass, naux = struct.unpack_from('<IhHBB', d, off + 8)
        if raw[:4] == b'\x00\x00\x00\x00':
            name = strtab_name(struct.unpack_from('<I', raw, 4)[0])
        else:
            name = raw.rstrip(b'\0').decode('utf8', 'replace')
        syms.append({'name': name, 'sec': secnum, 'type': typ,
                     'class': sclass, 'value': val})
        i += 1 + naux
    return sections, syms


def funclet_signatures(path):
    """name -> reloc-masked byte signature, for funclet-like code symbols.

    Mirrors objdiff's `funclet_signature`: zero the whole 4-byte instruction word
    at every relocation site inside the symbol, so only the pure encoding remains.
    """
    if not path or not os.path.exists(path):
        return {}
    sections, syms = parse_coff(path)
    persec = collections.defaultdict(list)
    for s in syms:
        if (s['sec'] > 0 and s['type'] == IMAGE_SYM_TYPE_FUNC
                and s['class'] in DEFINED_CLASSES):
            sec = sections[s['sec'] - 1]
            if sec['flags'] & IMAGE_SCN_CNT_CODE:
                persec[s['sec']].append(s)
    out = {}
    for secnum, lst in persec.items():
        sec = sections[secnum - 1]
        lst.sort(key=lambda s: s['value'])
        for j, s in enumerate(lst):
            start = s['value']
            end = lst[j + 1]['value'] if j + 1 < len(lst) else sec['size']
            if end <= start or not FUNCLET_RE.match(s['name']):
                continue
            b = bytearray(sec['data'][start:end])
            if len(b) != end - start:
                continue
            for rva in sec['relocs']:
                if start <= rva < end:
                    o = (rva - start) & ~3
                    b[o:o + 4] = b'\x00\x00\x00\x00'
            out[s['name']] = bytes(b)
    return out


def unit_oversubscription(target_path, base_path, detail=False):
    """excess = target funclets creditable ONLY by many-to-one reuse of a base funclet."""
    L = funclet_signatures(target_path)
    R = funclet_signatures(base_path)
    shared = set(L) & set(R)          # name-paired in an earlier objdiff pass
    lg = collections.Counter(v for k, v in L.items() if k not in shared)
    rg = collections.Counter(v for k, v in R.items() if k not in shared)
    excess, rows = 0, []
    for sig, n in lg.items():
        m = rg.get(sig, 0)
        if m and n > m:
            excess += n - m
            if detail:
                names = sorted(k for k, v in L.items() if v == sig and k not in shared)
                rows.append({'sig_bytes': len(sig), 'demand': n, 'supply': m,
                             'excess': n - m, 'targets': names})
    rows.sort(key=lambda r: -r['excess'])
    return excess, rows


class InputsUnavailable(RuntimeError):
    """The census could not read a single unit's object pair.

    Raised rather than returned, so that EVERY consumer goes loud. `funclet_signatures`
    answers `{}` for a path that does not exist, which made an unbuilt tree
    indistinguishable from a clean one: `census()` returned `{}`, `--verify`
    computed `current 0 fake`, and the gate printed "OK: no over-subscription
    growth" and exited 0 while reading nothing. `scripts/harvest/diffunit_gap_apply.py`
    already wraps `census()` in a try/except that refuses to write and says
    "Build the objs first" -- exactly the right response, which it never got to
    give because no exception was ever raised.
    """


def census(worktree, detail=False, stats=None):
    """Per-unit over-subscription census.

    `stats`, if given, is filled with the input-coverage accounting:
    `units` (total), `read` (units whose target AND base object BOTH exist on
    disk), `no_target`, `no_base`. Coverage is measured by FILE EXISTENCE, not
    by an empty signature dict -- an object with no funclets is a real
    measurement of zero, a missing object is not a measurement at all.

    Partial coverage is normal here: only decompiled units have a base object
    (measured 1,047 of 3,088 on the primary checkout), so a shortfall is not an
    error. ZERO coverage is, and raises InputsUnavailable.
    """
    cfg = json.load(open(os.path.join(worktree, 'objdiff.json')))
    out = {}
    n_units = n_read = n_no_target = n_no_base = 0
    for u in cfg['units']:
        t = u.get('target_path')
        b = u.get('base_path')
        t = os.path.join(worktree, t) if t else None
        b = os.path.join(worktree, b) if b else None
        n_units += 1
        t_ok = bool(t) and os.path.exists(t)
        b_ok = bool(b) and os.path.exists(b)
        if t_ok and b_ok:
            n_read += 1
        else:
            n_no_target += 0 if t_ok else 1
            n_no_base += 0 if b_ok else 1
        ex, rows = unit_oversubscription(t, b, detail)
        if ex:
            out[u['name']] = {'excess': ex, 'detail': rows} if detail else {'excess': ex}
    if stats is not None:
        stats.update(units=n_units, read=n_read,
                     no_target=n_no_target, no_base=n_no_base)
    if n_units and not n_read:
        raise InputsUnavailable(
            'read 0 of %d units: no unit has BOTH its target and base object on '
            'disk under %s. This tree is not built, so the census measured '
            'NOTHING -- it is not a clean result. Build the objs first '
            '(./tools/ninja-locked).' % (n_units, worktree))
    return out


BANNER = """
================================================================================
 OVER-SUBSCRIPTION GATE  (scripts/harvest/oversub_guard.py)
 objdiff pass 2b credits a target funclet 100% by re-using an ALREADY-CONSUMED
 byte-identical base funclet.  Those matches are machine code we never emitted.
 A span that absorbs more byte-identical funclets than the claimant's obj
 supplies therefore BUYS FAKE MATCHES.  Numbers below are fake matches.
================================================================================
"""


# Coverage record embedded in a baseline JSON. Shaped like a unit entry so that
# older readers summing `v['excess']` are unaffected by its presence.
COVERAGE_KEY = '_coverage'


def print_coverage(st):
    """Always say how many units' objects were actually READ.

    A census that read nothing used to be printed exactly like a census that
    read everything and found nothing.
    """
    if not st:
        return
    print('objects read: %d of %d units (missing target: %d, missing base: %d)'
          % (st.get('read', 0), st.get('units', 0),
             st.get('no_target', 0), st.get('no_base', 0)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--worktree', default='.')
    ap.add_argument('--census', action='store_true')
    ap.add_argument('--unit')
    ap.add_argument('--baseline', help='write per-unit census JSON here')
    ap.add_argument('--verify', help='compare against a baseline JSON; exit 3 on growth')
    ap.add_argument('--allow', type=int, default=0,
                    help='tolerated total growth in fake matches (default 0)')
    ap.add_argument('--top', type=int, default=25)
    ap.add_argument('--json', help='write census JSON here (with --census)')
    ap.add_argument('--detail', action='store_true')
    a = ap.parse_args()

    if a.unit:
        cfg = json.load(open(os.path.join(a.worktree, 'objdiff.json')))
        u = next((x for x in cfg['units'] if x['name'] == a.unit), None)
        if not u:
            print('unit not found:', a.unit)
            return 1
        ex, rows = unit_oversubscription(
            os.path.join(a.worktree, u['target_path']) if u.get('target_path') else None,
            os.path.join(a.worktree, u['base_path']) if u.get('base_path') else None,
            detail=True)
        print(BANNER)
        print('%s: %d fake matches' % (a.unit, ex))
        for r in rows[:a.top]:
            print('  sig %3dB  demand %3d  supply %3d  FAKE %3d   e.g. %s'
                  % (r['sig_bytes'], r['demand'], r['supply'], r['excess'],
                     ', '.join(r['targets'][:3])))
        return 0

    if a.baseline:
        st = {}
        c = census(a.worktree, a.detail, st)
        print_coverage(st)
        # Coverage is recorded so --verify can refuse a comparison made against a
        # tree it can see LESS of than the baseline saw. Shaped like a unit entry
        # (with excess 0) so older readers that do
        # `sum(v['excess'] for v in base.values())` are unaffected.
        payload = dict(c)
        payload[COVERAGE_KEY] = {'excess': 0, 'units_read': st.get('read', 0),
                                 'units_total': st.get('units', 0)}
        json.dump(payload, open(a.baseline, 'w'), indent=0, sort_keys=True)
        print('baseline: %d units, %d fake matches -> %s'
              % (len(c), sum(v['excess'] for v in c.values()), a.baseline))
        return 0

    if a.verify:
        base = json.load(open(a.verify))
        b_cov = base.pop(COVERAGE_KEY, None)
        st = {}
        cur = census(a.worktree, False, st)
        b_tot = sum(v['excess'] for v in base.values())
        c_tot = sum(v['excess'] for v in cur.values())
        grew = []
        for name, v in sorted(cur.items()):
            d = v['excess'] - base.get(name, {}).get('excess', 0)
            if d > 0:
                grew.append((name, base.get(name, {}).get('excess', 0), v['excess'], d))
        print(BANNER)
        print_coverage(st)
        # A gate that read fewer objects than its baseline did cannot see the
        # growth it exists to refuse. Silence from it is not evidence.
        if b_cov is None:
            print('\n!! baseline carries no coverage record (written before this '
                  'check existed) -- coverage parity with the baseline is '
                  'UNVERIFIABLE for this comparison.', file=sys.stderr)
        elif st.get('read', 0) < b_cov.get('units_read', 0):
            print('\n' + '=' * 78, file=sys.stderr)
            print('INPUTS UNAVAILABLE -- this comparison is VOID.', file=sys.stderr)
            print('  ! the baseline was taken over %d units\' objects; this run '
                  'read only %d.\n    A gate that reads fewer objects than its '
                  'baseline cannot see the growth it\n    exists to refuse. '
                  'Rebuild before verifying.'
                  % (b_cov.get('units_read', 0), st.get('read', 0)), file=sys.stderr)
            print('=' * 78, file=sys.stderr)
            return 2
        print('baseline %d fake -> current %d fake  (delta %+d, allow %d)'
              % (b_tot, c_tot, c_tot - b_tot, a.allow))
        for name, b0, c0, d in sorted(grew, key=lambda r: -r[3])[:a.top]:
            print('  GREW %-50s %d -> %d  (+%d fake)' % (name, b0, c0, d))
        if c_tot - b_tot > a.allow:
            print('\nREFUSED: this landing manufactures %d fake matches.'
                  '  Drop the offending spans or re-price the landing by its'
                  ' honest delta.' % (c_tot - b_tot))
            return 3
        print('\nOK: no over-subscription growth.')
        return 0

    # default: census
    st = {}
    c = census(a.worktree, a.detail, st)
    tot = sum(v['excess'] for v in c.values())
    print(BANNER)
    print_coverage(st)
    print('%d units over-subscribed | %d fake matches total' % (len(c), tot))
    print('%-56s %8s' % ('unit', 'fake'))
    for name, v in sorted(c.items(), key=lambda kv: -kv[1]['excess'])[:a.top]:
        print('%-56s %8d' % (name, v['excess']))
    if a.json:
        json.dump(c, open(a.json, 'w'), indent=0, sort_keys=True)
        print('wrote', a.json)
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except InputsUnavailable as exc:
        print('\n' + '=' * 78, file=sys.stderr)
        print('INPUTS UNAVAILABLE -- the census measured NOTHING. This is NOT a pass.',
              file=sys.stderr)
        print('  ! %s' % exc, file=sys.stderr)
        print('=' * 78, file=sys.stderr)
        sys.exit(2)
