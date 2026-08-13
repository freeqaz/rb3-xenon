#!/usr/bin/env python3
"""Restore the alias spellings lane T1-AUDIT removed on a VACUOUS screen
(lane RECOVER-95K, 2026-08-13).

WHAT T1-AUDIT DID, AND WHY THE VERDICT IS EMPTY
-----------------------------------------------
T1-AUDIT removed 190 (group, folded) alias pairs whose folded spelling
``scripts/target_symbol_map.json`` ALSO places at a "distinct LIVE address",
and adjudicated survivor-vs-that-address on retail bytes: 0 SAME / 188
DIFFERENT.  The byte comparison is CORRECT.  The verdict is EMPTY, because the
second operand is not a function.

Measured here over the 15 charged (target, base) name pairs that account for
ALL 1,406 lost rows / 95,100 B:

  14 of 15 base-side map addresses have ZERO `bl` call sites in the entire
  retail image, and several are not function heads at all --
    ??3RndLight@@SAXPAX@Z          @0x8270d7f8 = `b ?DoFade@Fader@@QAAXMM@Z`
    ??3CharEyeDartRuleset@@SAXPAX@Z@0x823beea8 = `b ?PollDeps@CharIKHand@@...`
    ??3UIComponent@@SAXPAX@Z       @0x8282779c = 0x00000000 (alignment padding)
    ??3PlayerDiffIcon@@SAXPAX@Z    @0x823258d4 = 0x00000000 (alignment padding)
    ??1?$list@PAVSortNode@@...     @0x82659a30 = `b ?AttemptNextTransition@...`

A comparator whose second operand is padding can only ever return DIFFERENT.
T1-AUDIT ran a NEGATIVE control (adjacent-function decoys) and a POSITIVE
control (identical pairs exist) but never controlled for whether addr(F) is a
FUNCTION -- so "0 SAME / 188 DIFFERENT" measured the map's noise floor, not the
folds.

THE SCREEN APPLIED HERE (and it CAN fail -- it fails on 1 of 15)
----------------------------------------------------------------
Restore a folded spelling F into its group iff it passes S1 or S2:

  S1  DEAD ROW.  Every map address naming F has ZERO `bl` call sites in the
      whole retail image.  13 of 15 pass.
  S2  NAME COLLISION ACROSS THE CRT/HMX BOUNDARY.  F's map address is live but
      100% of its callers sit in the vendor band (>= 0x82a00000) while the
      alias survivor's callers are predominantly below it.  Two distinct
      functions legitimately sharing one mangled name.  Exactly 1 passes:
      ``??3@YAXPAX@Z`` -- 0x82bc6b70 is a 148-byte CRT body, 177/177 callers in
      the vendor band; the survivor 0x8240ddb0 is the 8-byte `b MemFree` stub
      with 2,303 callers, 1,934 of them HMX.  We compile exactly one
      ``operator delete`` (src/system/utl/MemMgr.cpp:69 -> MemFree); libcmt's is
      not in our build at all.

A spelling passing NEITHER is a genuinely live HMX function and is NOT restored
-- ``??0?$ObjDirItr@VRndEnviron@@@@QAA@PAVObjectDir@@_N@Z`` @0x8243dfc0 has 6
real HMX callers and is deliberately HELD BACK, even though its caller
identities (TestTexturePaths / GetNormalMapTextures / GetTexturesOfType -- all
RndTex functions) say that row is itself misnamed.  That needs an independent
identification, not this screen.

This is NOT "forgiveness raises the score by construction".  Each restored
alias is a TRUE statement about which callee a `bl` denotes, on the same
standard tools/gen_symbol_alias_map.py applies to the PoolAlloc groups it
KEEPS.  The load-bearing case: retail's `operator delete` for Harmonix code is
the 8-byte stub 0x8240ddb0 = `b ?MemFree@@YAXPAX@Z` with 2,303 `bl` sites
(1,934 below the vendor band); our src/system/utl/MemMgr.cpp:69 is
`void operator delete(void *v) { MemFree(v); }`.  The map's OTHER
``??3@YAXPAX@Z`` row, 0x82bc6b70, is a 148-byte CRT body whose 177 callers are
177/177 inside the vendor band -- a different function that legitimately shares
a mangled name.  That name collision, not a false fold, is what tripped the
exposure predicate.

Usage:
    python3 tools/recover95k_restore.py --check      # report, write nothing
    python3 tools/recover95k_restore.py --apply
"""
import argparse
import collections
import json
import os
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'maprow_audit'))
from thunk_identity import Image  # noqa: E402

BAND = os.path.join(ROOT, 'orig/45410914/band.exe')
ALIASES = os.path.join(ROOT, 'scripts/symbol_aliases.json')
TMAP = os.path.join(ROOT, 'scripts/target_symbol_map.json')
# merge-parent of eedd51a8 (lane T1-AUDIT): the alias file BEFORE the prune.
PRE_REF = '4ac7f3be'

# The 15 charged (target, base) pairs, measured by diffing report.json row sets
# across the alias flip and extracting Symbol-typed diff_arg arguments from
# objdiff-cli at functionRelocDiffs=name_check.  base spelling -> group address
# it was removed from (all 15 were removed from exactly ONE group).
CHARGED = {
    "??3@YAXPAX@Z": "0x8240ddb0",
    "??1FilePath@@UAA@XZ": "0x827bea28",
    "??3@YAXPAX0@Z": "0x826c3888",
    "OggFree": "0x8240ddb0",
    "?SendMsgToAll@SessionMgr@@QAAXAAVNetMessage@@W4PacketType@@@Z": "0x825857a0",
    "??1?$list@UInstance@RndMultiMesh@@V?$TransformListAlloc@UInstance@RndMultiMesh@@@stlpmtx_std@@@stlpmtx_std@@QAA@XZ": "0x8243e160",
    "??0?$ObjDirItr@VRndEnviron@@@@QAA@PAVObjectDir@@_N@Z": "0x824af930",
    "??$__uninitialized_copy@PAUBone@CharBones@@PAU12@@stlpmtx_std@@YAPAUBone@CharBones@@PAU12@00ABU__false_type@0@@Z": "0x8237f158",
    "??3RndLight@@SAXPAX@Z": "0x8240ddb0",
    "??1?$list@PAVSortNode@@V?$StlNodeAlloc@PAVSortNode@@@stlpmtx_std@@@stlpmtx_std@@QAA@XZ": "0x828043a8",
    "??3UIComponent@@SAXPAX@Z": "0x8240ddb0",
    "??3RndMat@@SAXPAX@Z": "0x8240ddb0",
    "??1?$list@PAVMidiParser@@V?$StlNodeAlloc@PAVMidiParser@@@stlpmtx_std@@@stlpmtx_std@@QAA@XZ": "0x828043a8",
    "??3CharEyeDartRuleset@@SAXPAX@Z": "0x8240ddb0",
    "??3PlayerDiffIcon@@SAXPAX@Z": "0x8240ddb0",
}


VENDOR = 0x82A00000


def bl_callsites(img, targets):
    """-> {target_va: (n_bl_sites, n_from_hmx)} over the whole image."""
    data = img.data
    tot = collections.Counter()
    hmx = collections.Counter()
    va = 0x82000000
    while va < 0x82FF0000:
        o = img.offset(va)
        if o is None or o + 4 > len(data):
            va += 4
            continue
        w = struct.unpack_from('>I', data, o)[0]
        if (w >> 26) == 18 and (w & 1):
            li = w & 0x03FFFFFC
            if li & 0x02000000:
                li -= 0x04000000
            t = (va + li) & 0xFFFFFFFF
            if t in targets:
                tot[t] += 1
                if va < VENDOR:
                    hmx[t] += 1
        va += 4
    return {t: (tot.get(t, 0), hmx.get(t, 0)) for t in targets}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--apply', action='store_true')
    ap.add_argument('--check', action='store_true')
    a = ap.parse_args()

    img = Image(BAND)
    tmap = json.load(open(TMAP))
    rev = collections.defaultdict(list)
    for addr, n in tmap.items():
        if addr.startswith('0x') and isinstance(n, str):
            rev[n].append(int(addr, 16))

    watch = set()
    for f, gaddr in CHARGED.items():
        watch.update(rev.get(f, []))
        watch.add(int(gaddr, 16))
    print('resolving bl call sites for %d candidate map addresses ...' % len(watch))
    cnt = bl_callsites(img, watch)

    al = json.load(open(ALIASES))
    by_addr = {g['address'].lower(): g for g in al['groups']}
    # groups the prune EMPTIED are gone from the live file; recover their
    # survivor/address/evidence from the pre-prune blob, by content not by memory.
    pre = json.loads(subprocess.check_output(
        ['git', '-C', ROOT, 'show', '%s:scripts/symbol_aliases.json' % PRE_REF]))
    pre_by_addr = {g['address'].lower(): g for g in pre['groups']}

    restore, hold = [], []
    for f, gaddr in sorted(CHARGED.items()):
        addrs = rev.get(f, [])
        calls = {('0x%08x' % x): cnt.get(x, (0, 0)) for x in addrs}
        s_tot, s_hmx = cnt.get(int(gaddr, 16), (0, 0))
        s1 = all(v[0] == 0 for v in calls.values())
        # S2: live, but every caller is vendor-band while the survivor is mostly HMX
        s2 = (not s1
              and all(v[0] > 0 and v[1] == 0 for v in calls.values())
              and s_hmx > 0 and s_hmx * 2 > s_tot)
        rec = (f, gaddr, calls, (s_tot, s_hmx), 'S1' if s1 else ('S2' if s2 else '-'))
        (restore if (s1 or s2) else hold).append(rec)

    print('\n=== SCREEN S1 (dead map row) / S2 (CRT-vs-HMX name collision) ===')
    print('PASS (restore): %d' % len(restore))
    for f, g, c, s, w in restore:
        print('   [%s] %-62s group=%s surv_bl=%d(hmx %d)  map=%s'
              % (w, f[:62], g, s[0], s[1], c))
    print('FAIL (hold back -- genuinely live HMX function): %d' % len(hold))
    for f, g, c, s, w in hold:
        print('   [--] %-62s group=%s surv_bl=%d(hmx %d)  map=%s'
              % (f[:62], g, s[0], s[1], c))

    if not a.apply:
        print('\n(--check: nothing written)')
        return

    added, recreated = 0, 0
    for f, gaddr, _c, _s, _w in restore:
        g = by_addr.get(gaddr)
        if g is None:
            src = pre_by_addr.get(gaddr)
            if src is None:
                print('REFUSE: group %s absent from BOTH the live and the %s alias file'
                      % (gaddr, PRE_REF))
                sys.exit(2)
            g = {'address': src['address'], 'survivor': src['survivor'],
                 'folded': [], 'evidence': src['evidence']}
            al['groups'].append(g)
            by_addr[gaddr] = g
            recreated += 1
        if f in g['folded']:
            continue
        g['folded'] = sorted(set(g['folded']) | {f})
        added += 1
    print('recreated %d group(s) the prune had emptied' % recreated)
    note = ('LANE RECOVER-95K 2026-08-13: restored %d folded spelling(s) that lane '
            'T1-AUDIT removed on an exposure predicate fed by BOGUS MAP ROWS. Of the '
            '15 (target,base) name pairs that account for all 1,406 lost rows / '
            '95,100 B, 14 have a base-side map address with ZERO bl call sites in the '
            'whole retail image -- several are alignment padding (??3UIComponent '
            '@0x8282779c, ??3PlayerDiffIcon @0x823258d4 both start 0x00000000) or the '
            'body of an unrelated function (??3RndLight @0x8270d7f8 = b Fader::DoFade). '
            'T1-AUDIT compared a real function against that noise and correctly got '
            'DIFFERENT; the verdict is empty, not wrong. Each restored alias is a TRUE '
            'statement about which callee a bl denotes: the load-bearing one is that '
            'retail HMX operator delete is the 8-byte stub 0x8240ddb0 = b MemFree with '
            '2,303 bl sites, while the map\'s other ??3@YAXPAX@Z row 0x82bc6b70 is a '
            '148-byte CRT body whose 177 callers are 177/177 inside the vendor band. '
            'HELD BACK: ObjDirItr<RndEnviron> @0x8243dfc0, which has 6 real callers and '
            'so fails the screen -- its own map row looks misnamed (all 6 callers are '
            'RndTex functions) but that needs an independent identification.'
            % added)
    if isinstance(al.get('_comment'), list):
        al['_comment'] = list(al['_comment']) + ['', note]
    json.dump(al, open(ALIASES, 'w'), indent=1)
    print('\nrestored %d folded spelling(s) -> %s' % (added, ALIASES))


if __name__ == '__main__':
    main()
