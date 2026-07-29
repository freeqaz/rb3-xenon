"""Independent corroboration: pull C string literals referenced from a .text
span and test them against the rb3-Wii oracle .cpp/.h for that class."""
import sys, os, json, re, collections
from _paths import SCRATCH, REPO, BANDEXE, WII_SRC  # noqa: E402
import vt_constidx as constidx
from vt_pe import data, secs, sec_of, off_of, SEC

SITE = {}
for v, ss in constidx.CONST.items():
    for s in ss:
        SITE.setdefault(s, set()).add(v)

STR_SECS = ('.rdata', '.data')


def cstr(va, maxlen=200):
    o = off_of(va)
    if o is None:
        return None
    e = data.find(b'\0', o, o + maxlen)
    if e < 0:
        return None
    b = data[o:e]
    if len(b) < 3 or len(b) > 160:
        return None
    if not all(32 <= c < 127 or c in (9, 10) for c in b):
        return None
    return b.decode()


def strings_in(lo, hi):
    out = collections.Counter()
    for s, vs in SITE.items():
        if lo <= s < hi:
            for v in vs:
                if sec_of(v) in STR_SECS:
                    t = cstr(v)
                    if t:
                        out[t] += 1
    return out


WIIROOT = WII_SRC


def wii_text(canon):
    txt = ''
    for ext in ('.cpp', '.h'):
        p = os.path.join(WIIROOT, canon + ext)
        # canon is lowercase; find real-case file
        d = os.path.dirname(p)
        b = os.path.basename(p)
        if os.path.isdir(d):
            for f in os.listdir(d):
                if f.lower() == b.lower():
                    txt += open(os.path.join(d, f), errors='replace').read()
    return txt


if __name__ == '__main__':
    lo = int(sys.argv[1], 16); hi = int(sys.argv[2], 16)
    canon = sys.argv[3] if len(sys.argv) > 3 else None
    ss = strings_in(lo, hi)
    txt = wii_text(canon) if canon else ''
    hit = [s for s in ss if txt and s in txt]
    print(f'{len(ss)} strings in {lo:08X}..{hi:08X}; wii-source hits {len(hit)}')
    for s in sorted(ss):
        print(('  HIT  ' if s in hit else '       ') + repr(s))
