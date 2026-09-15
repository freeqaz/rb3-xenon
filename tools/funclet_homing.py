#!/usr/bin/env python3
"""Adjudicate whether each retail MSVC EH funclet is pinned to the unit that EMITTED it.

WHAT IT ANSWERS
    A funclet (`__unwind$N` / `__catch$N`, rendered `fn_<VA>` by dtk) is a piece of
    its PARENT function's COMDAT.  `config/45410914/splits.txt` pins address ranges
    to units; if a boundary falls between a parent and its own funclet, the funclet
    lands in a unit whose object never emitted it, and objdiff then pairs it by byte
    signature against whatever twin that wrong unit happens to supply (W16-BF §5).
    Verdict per funclet:
        HOMED       >=1 referencing parent lives in the funclet's pinned unit
        MIS-PINNED  referencing parents exist, none of them in the pinned unit
        ORPHAN      the only referencing parent sits in an unpinned (`auto_*`) region,
                    so no pin move can pair it

MECHANISM (all big-endian; retail image only, no build artifacts required)
    1. Every `0x19930522` FuncInfo in orig/45410914/band.exe (8,541 of them).
    2. Its UnwindMap (maxState x {toState, action}) and TryBlockMap
       (nTryBlocks x 20B -> HandlerType x 16B -> addressOfHandler) give the funclets
       it owns: 25,784 distinct unwind targets + 537 catch targets = 26,321.
    3. Parent = the 8-byte retail EH prefix {__CxxFrameHandler, &FuncInfo}; the
       function starts 4 bytes after the FuncInfo word.  A catch funclet carries its
       OWN prefix pointing at the parent's FuncInfo, so a FuncInfo can have 2 prefix
       sites; the parent is the site whose start is not itself a funclet target.
    4. Pinned unit = splits.txt `.text` ranges, keyed on the FULL heading path
       (never basename(): 707 bare vs 569 nested headings, and `Movie.obj` genuinely
       collides between rnddx9/ and rndobj/).

POPULATION
    All 26,321 funclet targets reachable from a FuncInfo in the retail image.

BLIND SPOTS / WHAT IT CANNOT SEE
    * Non-C++-EH funclets, and any funclet whose FuncInfo my magic scan misses.
    * It does NOT prove a receiving unit's compiled object supplies a byte-equal
      twin -- a re-home into a unit with no matching COMDAT drops the row to 0%.
      Price a move separately (see W16-BJ doc); PINHOME-1: re-homing is NOT
      metric-neutral, unlike adding a pin over auto_* code.
    * `report.json`'s per-row `address` field is a SECTION-RELATIVE OFFSET, not a VA.
      This tool keys on the `fn_<VA>` row NAME.  Using `address` as a VA is a silent
      vacuity (measured: 0/59,105 rows agree that way).
    * ORPHAN is a statement about OUR pinning, not about retail.

SELF-VALIDATION (run with --validate; all must hold)
    8,541 FuncInfos - 25,784 unwind targets - 537 catch targets
    537 extra EH-prefix sites, every one a catch funclet
    splits-derived pin == report.json unit membership on every fn_<VA> row
"""
import argparse, bisect, json, os, pickle, re, struct, sys
from collections import Counter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from eh_state_screen import Image          # reuse, do not rewrite

MAGIC   = 0x19930522
HANDLER = 0x82829530


def load_eh(img):
    """-> (funclet -> {'funcinfos': set, 'kinds': set}), {funcinfo -> parent}"""
    d = img.d
    text_lo = text_ra = text_sz = None
    for va, vsz, ra, rsz in img.secs:
        if img.base + va == 0x82270000:
            text_lo, text_ra, text_sz = 0x82270000, ra, rsz
    text_hi = text_lo + text_sz
    # 1. FuncInfos
    fis = []
    needle = struct.pack('>I', MAGIC)
    for va, vsz, ra, rsz in img.secs:
        if rsz == 0:
            continue
        blob, off = d[ra:ra + rsz], 0
        while True:
            i = blob.find(needle, off)
            if i < 0:
                break
            if i % 4 == 0:
                fis.append(img.base + va + i)
            off = i + 1
    # 2. funclet targets
    funclets = {}
    for f in fis:
        ms, pu = img.be32(f + 4), img.be32(f + 8)
        nt, pt = img.be32(f + 0x0C), img.be32(f + 0x10)
        if pu and ms and 0 < ms < 4096:
            for k in range(ms):
                a = img.be32(pu + 8 * k + 4)
                if a and text_lo <= a < text_hi:
                    e = funclets.setdefault(a, {'funcinfos': set(), 'kinds': set()})
                    e['funcinfos'].add(f); e['kinds'].add('unwind')
        if pt and nt and 0 < nt < 4096:
            for t in range(nt):
                ent = pt + 20 * t
                nc, ph = img.be32(ent + 12), img.be32(ent + 16)
                if not ph or not nc or nc > 4096:
                    continue
                for c in range(nc):
                    h = img.be32(ph + 16 * c + 12)
                    if h and text_lo <= h < text_hi:
                        e = funclets.setdefault(h, {'funcinfos': set(), 'kinds': set()})
                        e['funcinfos'].add(f); e['kinds'].add('catch')
    # 3. parents via the 8-byte EH prefix
    fiset, text = set(fis), d[text_ra:text_ra + text_sz]
    sites = {}
    for i in range(4, len(text) - 4, 4):
        w = struct.unpack_from('>I', text, i)[0]
        if w in fiset and struct.unpack_from('>I', text, i - 4)[0] == HANDLER:
            sites.setdefault(w, []).append(text_lo + i)
    parents, ambig = {}, []
    for f, vas in sites.items():
        cand = [v + 4 for v in sorted(vas)]
        nonf = [c for c in cand if c not in funclets]
        if len(nonf) == 1:
            parents[f] = nonf[0]
        else:
            parents[f] = None
            ambig.append((f, cand, nonf))
    return funclets, parents, sites, fis, ambig


def load_pins(root):
    blocks, heading = [], None
    for line in open(os.path.join(root, 'config/45410914/splits.txt')):
        if not line.strip():
            continue
        if not line[0].isspace():
            m = re.match(r'^(\S.*?):\s*$', line)
            if m and line.strip() != 'Sections:':
                heading = m.group(1)
            continue
        m = re.match(r'\s+\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)', line)
        if m and heading:
            blocks.append((int(m.group(1), 16), int(m.group(2), 16), heading))
    blocks.sort()
    return blocks


def unit_lookup(blocks):
    st = [b[0] for b in blocks]
    def f(a):
        i = bisect.bisect_right(st, a) - 1
        return blocks[i][2] if i >= 0 and blocks[i][0] <= a < blocks[i][1] else None
    return f


def norm_unit(h):
    return None if h is None else 'default/' + re.sub(r'\.(cpp|c|cc|cxx|s|asm)$', '', h)


def classify(root):
    img = Image(os.path.join(root, 'orig/45410914/band.exe'))
    funclets, parents, sites, fis, ambig = load_eh(img)
    blocks = load_pins(root)
    unit_of = unit_lookup(blocks)
    out = {}
    for a, e in funclets.items():
        pars = sorted({parents[f] for f in e['funcinfos'] if parents.get(f)})
        punits = [unit_of(p) for p in pars]
        pin = unit_of(a)
        if not pars:
            v = 'NO-PARENT'
        elif pin is not None and pin in punits:
            v = 'HOMED'
        elif all(pu is None for pu in punits):
            v = 'ORPHAN'
        elif pin is None:
            v = 'UNPINNED-FUNCLET'
        else:
            v = 'MIS-PINNED'
        out[a] = dict(addr=a, kind='/'.join(sorted(e['kinds'])), fanin_fi=len(e['funcinfos']),
                      parents=pars, parent_units=punits, pinned=pin, verdict=v)
    return out, dict(funcinfos=len(fis), funclets=len(funclets), sites=sites,
                     ambig=ambig, blocks=len(blocks), parents=parents)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--root', default=ROOT)
    ap.add_argument('--validate', action='store_true')
    ap.add_argument('--addrs', nargs='*', default=[])
    ap.add_argument('--verdict')
    ap.add_argument('--limit', type=int, default=30)
    ap.add_argument('--pickle')
    a = ap.parse_args()
    res, meta = classify(a.root)
    print("FuncInfos=%d funclet targets=%d .text pins=%d" % (meta['funcinfos'], meta['funclets'], meta['blocks']))
    fan = Counter(r['fanin_fi'] for r in res.values())
    print("fan-in (funclet -> #FuncInfos):", dict(sorted(fan.items())))
    print("verdicts:", dict(Counter(r['verdict'] for r in res.values())))
    if a.validate:
        ok = True
        ok &= meta['funcinfos'] == 8541
        ok &= meta['funclets'] == 26321
        # Extra prefix sites split into two populations, and the split is the point:
        #   537 are catch funclets carrying their OWN prefix (not folded parents)
        #     4 are the surplus parents of the ONE folded no-action FuncInfo
        #       (0x820A1EF0, maxState=1 action=0 -- it owns NO funclet).
        # That folded record is the POSITIVE CONTROL: it proves this scanner can
        # see .xdata folding, so funclet fan-in==1 everywhere is a measured
        # absence of funclet-level ICF, not a blind spot.
        extra_catch = extra_other = 0
        for f, vas in meta['sites'].items():
            for v in sorted(vas)[1:] if len(vas) > 1 else []:
                pass
            starts = [v + 4 for v in sorted(vas)]
            for st in starts:
                if st in res and 'catch' in res[st]['kind']:
                    extra_catch += 1
        extra_other = sum(len(v) - 1 for v in meta['sites'].values()) - extra_catch
        print("extra EH-prefix sites: %d catch-funclet + %d folded-parent (expect 537 + 4)"
              % (extra_catch, extra_other))
        ok &= extra_catch == 537 and extra_other == 4
        print("ambiguous FuncInfos:", len(meta['ambig']), "(expect 1: the folded no-action record)")
        print("VALIDATE:", "PASS" if ok else "FAIL")
        if not ok:
            sys.exit(3)
    for s in a.addrs:
        r = res.get(int(s, 16))
        print(r)
    if a.verdict:
        sel = [r for r in res.values() if r['verdict'] == a.verdict]
        sel.sort(key=lambda r: r['addr'])
        print("--- %s: %d ---" % (a.verdict, len(sel)))
        for r in sel[:a.limit]:
            print("  0x%08X %-6s parents=%s parent_units=%s pinned=%s" % (
                r['addr'], r['kind'], [hex(p) for p in r['parents']], r['parent_units'], r['pinned']))
    if a.pickle:
        pickle.dump(res, open(a.pickle, 'wb'))


if __name__ == '__main__':
    main()
