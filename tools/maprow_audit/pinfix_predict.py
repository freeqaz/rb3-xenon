#!/usr/bin/env python3
"""Lane PINFIX-1: predict, per row, whether re-pinning will MATCH -- not merely pair.

WHY A PREDICTOR AT ALL
----------------------
Re-pinning a row into the unit that defines its name makes it PAIRABLE.  Whether
it then MATCHES depends entirely on whether our compiled body equals retail's.
"N rows one pin away" has been 21x optimistic in this repo, so the realised
fraction is the finding and it must be predicted BEFORE it is measured.

THE INSTRUMENT (reused, not rebuilt)
------------------------------------
truncated_pins.Obj already yields each function's bytes together with a
RELOCATION MASK -- the linker rewrites those fields, so a raw memcmp between an
obj body and retail is structurally vacuous (the same trap that makes raw
`memcmp` useless for ICF).  We compare MASKED bodies:

    masked(our compiled body from the DESTINATION obj) == masked(retail @ A)

Masked-equal  -> predict MATCH  (fuzzy -> 100, +1 function, +size bytes)
Size differs  -> predict NO_MATCH, and we know why before measuring
Masked-differ -> predict PAIRED_NOT_MATCHED (a real body divergence)

THE NULL CONTROL, because a screen nobody tried to break is not a screen
----------------------------------------------------------------------
The same masked compare is run against a WRONG retail address (the row's own
address shifted by a fixed non-trivial offset).  If the null confirms at any
appreciable rate the test is low-entropy and its positives mean nothing.  Rows
whose mask leaves too few fully-unmasked words are reported separately for the
same reason.
"""
import argparse
import collections
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import truncated_pins as _T                                    # noqa: E402

# ⛔⛔ THE REUSED RELOCATION-MASK TABLE UNDER-MASKS `bl`, AND THAT DEFECT IS
# INVISIBLE IN ITS ORIGINAL USE.  truncated_pins.REL_MASK is a straight lift of
# winnt.h's PC-PowerPC IMAGE_REL_PPC_* numbering, which is NOT the numbering
# this binary's objs use.  Measured on CharSleeve.obj: type 0x0006 occurs 643
# times (the commonest in code, i.e. `bl`) and the table gives it REL14's
# 14-bit mask 0x0000FFFC.  A 24-bit branch therefore keeps 10 displacement bits
# UNMASKED, so an unlinked `4BFFFFED` never equals a resolved `4BF9B3FD`:
#
#     +0x14 ours=4BFFFFED retail=4BF9B3FD mask=FFFF0003  DIFF
#
# (Type 0x0012, 585 sites, is not in the table at all and falls to the
# mask-everything default.)
#
# WHY IT SURVIVED: an under-wide mask yields FALSE NEGATIVES, which is the SAFE
# direction for truncated_pins' own job (proposing candidate windows).  Reusing
# it as a MATCH PREDICTOR inherits exactly that pessimism -- measured here as
# 17 predicted against 62 actual, a 3.6x under-call, every error a miss and
# never a false alarm.  Widening 0x0006 to the 24-bit field takes the predictor
# from recall 17/62 to a measured 62/62 recall AND 62/62 precision on the same
# 76 rows.
#
# The override is applied HERE and truncated_pins is deliberately NOT mutated:
# widening the mask makes that tool MORE permissive for its own purpose, which
# this lane did not measure and must not change blind.
_T.REL_MASK[0x0006] = 0x03FFFFFC

from truncated_pins import Obj, Retail, masked, unmasked_count  # noqa: E402,E501

MIN_UNMASKED_WORDS = 3


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--priced', default=os.path.expanduser('~/tmp/pinfix_priced.json'))
    ap.add_argument('--json', help='write predictions here')
    ap.add_argument('--null-shift', type=lambda s: int(s, 0), default=0x40)
    a = ap.parse_args()

    rows = json.load(open(a.priced))
    R = Retail()
    objcache = {}

    def getobj(p):
        if p not in objcache:
            objcache[p] = Obj(p)
        return objcache[p]

    tally = collections.Counter()
    out = []
    null_hits = 0
    null_tests = 0

    for r in rows:
        A = int(r['addr'], 16)
        nm = r['name']
        dobjs = r['defining_objs']
        rec = dict(r)
        if len(dobjs) != 1:
            rec['predict'] = 'NOT_SINGLETON'
            tally['NOT_SINGLETON'] += 1
            out.append(rec)
            continue
        o = getobj(dobjs[0])
        fb = o.funcs.get(nm)
        if fb is None:
            # defined, but not as a .text COMDAT function symbol (data, or a
            # section shape this reader groups differently)
            rec['predict'] = 'NO_BODY_IN_OBJ'
            tally['NO_BODY_IN_OBJ'] += 1
            out.append(rec)
            continue
        body, mask = fb
        rec['our_size'] = len(body)
        got = R.bytes_at(A, len(body))
        if got is None:
            rec['predict'] = 'OUT_OF_TEXT'
            tally['OUT_OF_TEXT'] += 1
            out.append(rec)
            continue
        uw = unmasked_count(mask) // 4
        rec['unmasked_words'] = uw
        eq = masked(got, mask) == masked(body, mask)

        # ---- null control: the SAME compare against a wrong address
        null_tests += 1
        ng = R.bytes_at(A + a.null_shift, len(body))
        if ng is not None and masked(ng, mask) == masked(body, mask):
            null_hits += 1
            rec['null_confirms'] = True

        rsize = r.get('size') or 0
        if eq:
            rec['predict'] = 'MATCH' if uw >= MIN_UNMASKED_WORDS else 'MATCH_LOW_ENTROPY'
            tally[rec['predict']] += 1
        else:
            rec['predict'] = ('SIZE_DIFFERS' if rsize and rsize != len(body)
                              else 'BODY_DIFFERS')
            tally[rec['predict']] += 1
        out.append(rec)

    print('[predictions over %d candidate rows]' % len(rows))
    for k, v in tally.most_common():
        print('  %-20s %4d' % (k, v))

    m = [r for r in out if r['predict'] in ('MATCH', 'MATCH_LOW_ENTROPY')]
    mb = sum(int(r.get('size') or 0) for r in m)
    allb = sum(int(r.get('size') or 0) for r in out)
    print('\n[predicted realised] %d of %d rows, %d B of %d B bound (%.1f%% of bytes)'
          % (len(m), len(out), mb, allb, 100.0 * mb / max(allb, 1)))

    print('[NULL CONTROL] same masked compare at +0x%X: %d/%d confirm  %s'
          % (a.null_shift, null_hits, null_tests,
             '<-- screen is DISCRIMINATING' if null_hits == 0
             else '<-- WARNING: screen fires on wrong addresses too'))

    if a.json:
        json.dump(out, open(a.json, 'w'), indent=1)
        print('\nwrote %s' % a.json)


if __name__ == '__main__':
    main()
