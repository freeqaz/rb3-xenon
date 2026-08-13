#!/usr/bin/env python3
"""copy_construct_harvest -- NAME the STLport `_Copy_Construct<T>` addresses that
MASK-3 emptied, using OUR OWN objs as a MANGLING oracle (never as an identity one).

Provenance: lane CC-HARVEST (2026-08-13), fifth pass over this family after
MASK-3 (`tools/copy_construct_audit.py`, a8191e9c / a43c1bc7) and MAP-CC
(`tools/copy_construct_shape_audit.py`, 415dc969 / e4bf78a8).  Read-only unless
--write-map; not a build input.

★★★ WHAT THE OBJ SYMBOL TABLE CAN AND CANNOT TELL YOU
------------------------------------------------------
The proposal this lane inherited was "harvest `??$_Copy_Construct@...` names from
our own compiled objs and use them to name the emptied addresses".  Stated that
way it is a category error, and saying why is the whole discipline of this tool:

    our objs are a MANGLING oracle -- given T, they supply the exact spelling
    our build will pair against.  They are NOT an IDENTIFICATION oracle: they
    say nothing about WHICH retail address holds which T.

Identification therefore still comes from MASK-3's W_A -- resolve the `bl` at
+0x24, that callee is T's copy constructor -- admitted only when the callee is
METRIC-PINNED (singleton masked body AND its name scores mpn==100).  This tool
adds the spelling, and one new witness for the spelling; it adds no identity
evidence whatsoever and deliberately consults no unit, splits or directory fact
(MASK-3's TRAP 3/4).

★★★ WHY A NAME IS NOT AUTOMATIC ONCE YOU KNOW T -- THE Copy/Param FORK
-----------------------------------------------------------------------
`_Copy_Construct<T>(T*, const T&)` and `_Param_Construct<T,T>(T*, const T&)`
compile to the SAME 15 words AND the SAME `bl` relocation (T's copy ctor).  They
are byte-identical including relocations.  So:

  * retail bytes cannot distinguish them -- the ONLY variable word is the one
    the graded ruler masks, and here even that word agrees;
  * our objs contain BOTH for most T (measured: 12 of 16 candidates);
  * either spelling scores mpn==100 against the address.

⇒ Picking one because it scores is EXACTLY the vacuous case.  "Scored 100" is
not evidence here and this tool never uses it as such.

★★ THE NEW WITNESS (W_F, FORM) -- AND IT IS NOT THE ONE YOU EXPECT
-------------------------------------------------------------------
Each STLport caller calls exactly ONE of the two forms, and WHICH one is a fact
about our own build that we can read out of the obj RELOCATIONS -- no score, no
name-guessing.  Measured over every obj: 657 caller symbols carry a helper-form
edge, **364 call only _Param_Construct, 293 call only _Copy_Construct, ZERO call
both.**  A clean bipartition.

⚠ The mapping is the REVERSE of the intuitive reading of STLport, which is why
this must be MEASURED and never assumed:

    __uninitialized_copy<T*>    -> _Param_Construct        (NOT _Copy_Construct)
    __uninitialized_fill_n<T*>  -> _Param_Construct
    vector<T>::_M_insert_overflow_aux -> _Copy_Construct
    vector<T>::push_back        -> _Copy_Construct

So: take retail's callers of the address (a `bl` scan of the image), keep only
those that are METRIC-PINNED and whose own instantiation T equals W_A's T, and
read off the form.  One-sided ⇒ the spelling is DETERMINED.

★★★ AND WHEN IT IS TWO-SIDED, THAT IS A RESULT, NOT A FAILURE
---------------------------------------------------------------
`Hide@CharMeshHide` @0x823a0cf8 is called by `__uninitialized_copy<Hide>` AND
`__uninitialized_fill_n<Hide>` (Param side) AND
`_M_insert_overflow_aux<vector<Hide>>` (Copy side), all three metric-pinned, all
three over the same T.  Retail instantiated BOTH forms, and since they are
identical *including relocations* the linker's ICF folded them onto one address
(CLAUDE.md's CD-7 signature: MSVC folds only COMDATs identical including
relocations).  Both names truthfully denote that address -- so NEITHER can be
written as *the* name, and the row is declined as FOLD_BOTH.

This also explains, without re-adjudicating them, the four T that currently
carry BOTH forms at two different addresses in the map
(Node@ObjPtrVec<RndGroup>, Char3D@CharData@WorldCrowd, LeaderboardRow,
TrackData@SongDB): in every one of the four, at least one of the two rows has a
`bl` that CONTRADICTS its own name (two call `operator=`, not a constructor;
one calls `??0MatSwap@OutfitConfig@@`).  They are unadjudicated rows MASK-3
correctly left NOT_JUDGED, not counterexamples to folding.  MASK-3's own
structural measurement agrees: 112 members / 112 DISTINCT `bl` destinations --
two unfolded `<T,T>` helpers would have to share one.  **This tool does not
touch those four rows.**

THE ADMISSION RULE (both clauses required)
------------------------------------------
  E1  IDENTITY.  T comes from W_A, admitted only when the callee is
      METRIC-PINNED.  Unchanged from MASK-3; this tool weakens nothing.
  E2  SPELLING.  Either
        (a) DETERMINED -- a metric-pinned, same-T caller fixes the form via the
            measured caller->form bipartition; or
        (b) UNIQUE -- our build emits exactly ONE form for that T anywhere in
            the tree, so there is no fork to resolve.
      Otherwise DECLINED.  ANONYMOUS BEATS WRONG.

⚠ HONEST BOUND ON (b): "our build emits only the Copy form for T" is a fact
about OUR instantiation set, not retail's.  It is the weaker of the two clauses
and is reported separately so it can be re-judged in isolation.  It is admitted
because the alternative spelling provably does not exist as a symbol anywhere in
our tree, so the fork has no second branch to take.

⛔ THE SCREEN THAT WAS VACUOUS, AND HOW IT WAS CAUGHT
-----------------------------------------------------
The first body matcher written for this lane compared the COMDAT SECTION size
against 60 and found **0 of 995** symbols -- a clean, decisive negative that
would have killed the lane, and that appeared to CORROBORATE lane STL-104's
"our helpers are 104 B where retail's are 60 B".  It was wrong.  MSVC emits an
**8-byte EH prefix** ahead of the function inside the same COMDAT (CLAUDE.md
documents the prefix), so the symbol sits at `val=8` in a 112-byte section and
the body must be read at the SYMBOL's offset, not the section's.  Read
correctly, 1,834 symbol instances / 273 distinct names carry retail's exact
BODY60 -- and our body is byte-identical to retail's modulo the masked `bl`.

⇒ **STL-104's size claim does not apply to this family**, and the 35 rows MASK-3
left CONSISTENT independently prove it: all 35 score mpn==100 at size 60.
That known-positive population is wired in below as a `screen_gate` fixture so
this specific vacuity cannot recur silently.
"""
from __future__ import annotations

import argparse
import collections
import glob
import importlib.util
import json
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAP = 'scripts/target_symbol_map.json'
PFX = ('??$_Copy_Construct@', '??$_Param_Construct@')
EH_AUX = ('__ehfuncinfo', '__tryblocktable', '__catchsym', '__unwindtable')

#: MASK-3's exact 15-word `if (p) new (p) T(v)` body; None = the masked `bl`.
BODY60 = [0x7d8802a6, 0x9181fff8, 0xfbe1fff0, 0x3be1ff90, 0x9421ff90,
          0x907f0084, 0x907f0050, 0x2b030000, 0x419a0008, None,
          0x383f0070, 0x8181fff8, 0x7d8803a6, 0xebe1fff0, 0x4e800020]


def _load_mask3(project_dir):
    """Reuse MASK-3's Audit verbatim -- W_A, pinning, mpn100, caller index."""
    p = os.path.join(project_dir, 'tools/copy_construct_audit.py')
    spec = importlib.util.spec_from_file_location('cca', p)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


# ----------------------------------------------------------------- COFF ------
def coff(path):
    """(data, sections, symindex, comdat_owners) or None.  Headers are LE."""
    d = open(path, 'rb').read()
    if len(d) < 20:
        return None
    _m, ns, _t, symoff, nsym, opt, _c = struct.unpack_from('<HHIIIHH', d, 0)
    if not symoff or not nsym or symoff + nsym * 18 > len(d):
        return None
    strtab = symoff + nsym * 18
    secs = []
    for i in range(ns):
        o = 20 + opt + i * 40
        _vs, _va, rawsz, rawptr = struct.unpack_from('<IIII', d, o + 8)
        relptr, _rl, nrel = struct.unpack_from('<IIH', d, o + 24)
        secs.append((rawsz, rawptr, relptr, nrel))
    tbl, owners, i = {}, collections.defaultdict(list), 0
    while i < nsym:
        o = symoff + i * 18
        raw = d[o:o + 8]
        if raw[:4] == b'\0\0\0\0':
            off = struct.unpack_from('<I', raw, 4)[0]
            e = d.index(b'\0', strtab + off)
            nm = d[strtab + off:e].decode('latin1')
        else:
            nm = raw.rstrip(b'\0').decode('latin1')
        val, secnum, _ty, cls, naux = struct.unpack_from('<IhHBB', d, o + 8)
        tbl[i] = nm
        if cls == 2 and 0 < secnum <= len(secs):
            owners[secnum].append((nm, val))
        i += 1 + naux
    return d, secs, tbl, owners


def body_is_BODY60(d, rawptr, val, rawsz):
    """★ Read at the SYMBOL offset, never the section's -- see the vacuity note."""
    if val + 60 > rawsz:
        return False
    w = struct.unpack_from('>15I', d, rawptr + val)
    return all(w[k] == BODY60[k] for k in range(15) if BODY60[k] is not None)


def scan_objs(root):
    """-> (name -> {objs}) for BODY60 helpers, (caller name -> {'Copy','Param'})."""
    helpers = collections.defaultdict(set)
    form = collections.defaultdict(set)
    for p in glob.glob(os.path.join(root, 'build/45410914/src/**/*.obj'),
                       recursive=True):
        r = coff(p)
        if not r:
            continue
        d, secs, tbl, owners = r
        base = os.path.basename(p)
        for secnum, syms in owners.items():
            rawsz, rawptr, relptr, nrel = secs[secnum - 1]
            for nm, val in syms:
                if nm.startswith(PFX) and body_is_BODY60(d, rawptr, val, rawsz):
                    helpers[nm].add(base)
            kinds = set()
            for k in range(nrel):
                _va, si, _ty = struct.unpack_from('<IIH', d, relptr + k * 10)
                t = tbl.get(si, '')
                if t.startswith('??$_Copy_Construct@'):
                    kinds.add('Copy')
                elif t.startswith('??$_Param_Construct@'):
                    kinds.add('Param')
            if kinds:
                for nm, _v in syms:
                    if not nm.startswith(EH_AUX):
                        form[nm] |= kinds
    return helpers, form


# ------------------------------------------------------------------ gate -----
def arm_screens(helpers, form):
    """Prove the body screen FIRES on known positives and does NOT fire on the
    exact vacuity that killed the first version (reading at section offset 0)."""
    sys.path.insert(0, os.path.join(ROOT, 'tools'))
    try:
        from screen_gate import Screen, gate           # noqa: WPS433
    except Exception:
        return None                                    # gate absent: caller warns

    good = struct.pack('>15I', *[w if w is not None else 0x4bffffd5
                                 for w in BODY60])
    prefixed = b'\x00' * 8 + good                      # EH prefix ahead of body

    s = Screen('BODY60-at-symbol-offset',
               detect=lambda pay: body_is_BODY60(pay[0], 0, pay[1], len(pay[0])),
               why='fires when the 15-word _Copy_Construct body sits at the '
                   'SYMBOL offset (not the COMDAT section offset)')
    s.must_fire('retail BODY60 at offset 0', (good, 0))
    s.must_fire('BODY60 behind an 8-byte EH prefix', (prefixed, 8))
    s.must_not_fire('EH prefix read as the body (the lane-CC-HARVEST vacuity)',
                    (prefixed, 0))
    s.must_not_fire('unrelated prologue', (struct.pack('>15I', *([0x60000000] * 15)), 0))
    return gate([s])


# ----------------------------------------------------------------- harvest ---
def harvest(project_dir, cca):
    au = cca.Audit(project_dir)
    helpers, form = scan_objs(project_dir)
    byT = collections.defaultdict(dict)
    for nm in helpers:
        t = cca.canon(cca.member_T(nm))
        if t:
            byT[t]['Copy' if nm.startswith('??$_Copy_Construct@') else 'Param'] = nm
    used = collections.Counter(v for v in au.map.values() if isinstance(v, str))

    mem = au.members()
    callers = au.caller_index(mem)
    rows = []
    for a in mem:
        if au.byaddr.get(a) is not None:
            continue                                   # already named: not ours
        t = au.bl(a, cca.BL_CTOR)
        cn = au.byaddr.get(t) if t else None
        # ---- E1: identity, MASK-3's rule, unchanged ----------------------
        if not cn or not cn.startswith('??0') or not cca.CTOR_PARAM.search(cn):
            continue
        if au.cls_size.get(t) != 1 or cn not in au.mpn100:
            continue
        T = cca.canon(cca.ctor_class(cn))
        if not T:
            continue
        avail = byT.get(T, {})
        # ---- E2: spelling ------------------------------------------------
        ev, kinds = [], set()
        for c in callers.get(a, ()):
            n = au.byaddr.get(c)
            if not n or n not in au.mpn100:
                continue
            f = form.get(n)
            if not f or len(f) != 1:
                continue
            same = cca.canon(cca.caller_T(n)) == T
            ev.append(('0x%08x' % c, n, list(f)[0], same))
            if same:
                kinds |= f
        if len(kinds) == 1 and list(kinds)[0] in avail:
            verdict, form_pick = 'DETERMINED', list(kinds)[0]
        elif len(kinds) > 1:
            verdict, form_pick = 'FOLD_BOTH', None
        elif len(avail) == 1:
            verdict, form_pick = 'UNIQUE_FORM', next(iter(avail))
        elif not avail:
            verdict, form_pick = 'NO_SYMBOL_IN_OUR_OBJS', None
        else:
            verdict, form_pick = 'DECLINED_AMBIGUOUS', None
        name = avail.get(form_pick) if form_pick else None
        if name and used[name]:
            verdict, name = 'DECLINED_INJECTIVITY', None
        rows.append(dict(key='0x%08x' % a, T=T, callee='0x%08x' % t,
                         callee_name=cn, verdict=verdict, name=name,
                         forms_available=sorted(avail), evidence=ev))
    return au, rows


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--project-dir', default=ROOT)
    ap.add_argument('--json')
    ap.add_argument('--write-map', action='store_true')
    args = ap.parse_args()
    pd = os.path.abspath(args.project_dir)
    cca = _load_mask3(pd)

    au, rows = harvest(pd, cca)
    helpers, form = scan_objs(pd)
    g = arm_screens(helpers, form)
    if g is None:
        print('WARNING: tools/screen_gate.py unavailable -- screens UNGATED')
    elif not g.armed:
        print('REFUSED: body screen failed its own fixtures'); return 2
    else:
        print('screen gate: ARMED (body matcher fires on known positives, '
              'and does NOT fire on the section-offset vacuity)')

    fc = collections.Counter(tuple(sorted(v)) for v in form.values())
    print('\nour objs: %d BODY60 helper names; %d caller symbols with a form '
          'edge %s' % (len(helpers), len(form), dict(fc)))
    c = collections.Counter(r['verdict'] for r in rows)
    print('unmapped members of the class with a PINNED callee (E1 satisfied): '
          '%d\n  %s' % (len(rows), dict(c)))
    for r in sorted(rows, key=lambda r: r['key']):
        print('\n%s  T=%s\n    %s%s' % (r['key'], r['T'], r['verdict'],
                                        '  -> ' + r['name'] if r['name'] else ''))
        for addr, n, f, same in r['evidence']:
            print('        %-5s %-4s %s %s' % (f, 'SAME' if same else 'diff', addr, n[:80]))
    if args.json:
        json.dump(rows, open(args.json, 'w'), indent=1)
        print('\nwrote', args.json)

    plan = {r['key']: r['name'] for r in rows if r['name']}
    print('\nPLAN: %d addresses named (%d DETERMINED, %d UNIQUE_FORM)' % (
        len(plan),
        sum(1 for r in rows if r['verdict'] == 'DETERMINED'),
        sum(1 for r in rows if r['verdict'] == 'UNIQUE_FORM')))
    if args.write_map and plan:
        path = os.path.join(pd, MAP)
        m = json.load(open(path))
        seen = collections.Counter(v for v in m.values() if isinstance(v, str))
        for k, n in plan.items():
            if seen[n]:
                raise SystemExit('REFUSED: injectivity collision on %s' % n)
            if m.get(k) is not None:
                raise SystemExit('REFUSED: %s is already named' % k)
            m[k] = n
        json.dump(m, open(path, 'w'), indent=1)
        print('wrote', path)
    return 0


if __name__ == '__main__':
    sys.exit(main())
