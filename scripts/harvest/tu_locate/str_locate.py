from _paths import SCRATCH, REPO, BANDEXE  # noqa: E402
"""String-xref TU locator: cluster selective-literal code refs, attribute via .pdata."""
import json, os, re, sys, bisect
from collections import defaultdict, Counter

HERE = os.path.dirname(os.path.abspath(__file__))
WII = os.path.join(os.path.dirname(REPO), 'rb3', 'src')

import str_xref as xref

XR = json.load(open(SCRATCH+'/xref.json'))
REV = {k: v for k, v in XR['rev'].items()}

# ---- pdata function table ----
FNS = xref.build_pdata()
FSTART = [a for a, b in FNS]


def fn_of(va):
    i = bisect.bisect_right(FSTART, va) - 1
    if i < 0:
        return None
    a, b = FNS[i]
    if a <= va < b:
        return (a, b)
    return None


# ---- splits.txt claims ----
def load_splits():
    claims = []
    unit = None
    for line in open(os.path.join(REPO, 'config/45410914/splits.txt')):
        s = line.rstrip('\n')
        if not s.strip() or s.lstrip().startswith('#'):
            continue
        if not s[0].isspace() and s.rstrip().endswith(':'):
            unit = s.strip()[:-1]
            continue
        t = s.strip()
        if t.startswith('.text'):
            m = re.search(r'start:(0x[0-9a-fA-F]+)\s+end:(0x[0-9a-fA-F]+)', t)
            if m and unit:
                claims.append((int(m.group(1), 16), int(m.group(2), 16), unit))
    claims.sort()
    return claims


CLAIMS = load_splits()
CSTART = [c[0] for c in CLAIMS]


def claim_of(va):
    i = bisect.bisect_right(CSTART, va) - 1
    if i < 0:
        return None
    a, b, u = CLAIMS[i]
    if a <= va < b:
        return u
    return None


# ---- Wii source literals ----
LIT = re.compile(r'"((?:[^"\\\n]|\\.){3,})"')
GENERIC = set()


def wii_lits(canon):
    """canon like 'system/meta/storepackedmetadata' -> find real file."""
    parts = canon.split('/')
    d = os.path.join(WII, *parts[:-1])
    stem = parts[-1]
    out = []
    if not os.path.isdir(d):
        return out
    for f in os.listdir(d):
        b, e = os.path.splitext(f)
        if b.lower() == stem and e in ('.cpp', '.h'):
            out.append(os.path.join(d, f))
    return out


def lits_of(paths):
    s = set()
    for p in paths:
        try:
            txt = open(p, encoding='utf-8', errors='replace').read()
        except Exception:
            continue
        for m in LIT.finditer(txt):
            v = m.group(1)
            try:
                v = v.encode().decode('unicode_escape')
            except Exception:
                pass
            if len(v) >= 4 and all(32 <= ord(c) < 127 for c in v):
                s.add(v)
    return s


def locate(lits, maxsites=8, minlen=5):
    """Return (span, corroborating literals, all sites)."""
    sel = {}
    for L in lits:
        if len(L) < minlen:
            continue
        sites = REV.get(L)
        if not sites:
            continue
        if len(sites) > maxsites:
            continue
        sel[L] = sites
    # attribute to functions
    fnhits = defaultdict(set)   # fn -> set(lits)
    for L, sites in sel.items():
        for va in sites:
            f = fn_of(va)
            if f:
                fnhits[f].add(L)
    return sel, fnhits


def cluster(fnhits, gap=0x2000):
    """Group functions into contiguous clusters (<=gap between consecutive fns)."""
    fs = sorted(fnhits)
    if not fs:
        return []
    groups = []
    cur = [fs[0]]
    for f in fs[1:]:
        if f[0] - cur[-1][1] <= gap:
            cur.append(f)
        else:
            groups.append(cur)
            cur = [f]
    groups.append(cur)
    out = []
    for g in groups:
        lset = set()
        for f in g:
            lset |= fnhits[f]
        out.append({
            'lo': g[0][0], 'hi': g[-1][1], 'nfn': len(g),
            'lits': sorted(lset),
            'claims': sorted(set(x for x in (claim_of(f[0]) for f in g) if x)),
            'unclaimed_fns': sum(1 for f in g if claim_of(f[0]) is None),
        })
    out.sort(key=lambda d: (-len(d['lits']), -d['nfn']))
    return out
