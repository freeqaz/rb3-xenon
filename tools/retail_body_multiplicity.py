#!/usr/bin/env python3
"""How many distinct RETAIL addresses hold a given body? (whole-image, resolved)

WHY THIS EXISTS (lane W16-EN, 2026-09-16).  Admitting an ICF fold requires that
retail kept ONE copy of the body.  The existing evidence column for that,
``icf_pair_adjudicate.py --family``'s ``retail_bodytwins``, counts only symbols
in PINNED target objs, so it systematically UNDERCOUNTS retail copies and can
report 1 when the image holds several.  W16-EL and W16-EP both established that
"byte-identical => folded" is false, so an undercount here is the difference
between a proven fold and a fabricated alias.

THE COMPARATOR IS THE POINT.  /OPT:ICF folds only COMDATs identical INCLUDING
relocations, so the test must be:

  1. mask PC-relative branch displacement fields (I-form b/bl, B-form bc) --
     they differ at different addresses for identical code, so comparing raw
     bytes is the silently-vacuous test CLAUDE.md warns about; then
  2. require identical RESOLVED destinations: internal branches compared as
     offsets from the function base, external calls as absolute VAs.

⚠ Step 2 is what separates this from a shape-only census, and it is not a
formality.  Re-measuring W16-EP's refutation with it INVERTED the result: the
"eleven byte-identical 84-byte list<T*>::erase bodies, identical including their
bl targets" are ELEVEN DISTINCT MASKED FORMS -- e.g. 0x822b1e60 vs 0x82447458
differ at word 7, ``li r3, 84`` vs ``li r3, 24``, the per-T NODE SIZE.  A
shape-only comparator reads them as one body; they can never fold.  This is the
same mechanism CLAUDE.md records for _List_base<T>::clear (42 addresses,
reloc-identical surplus 0).

★ ALWAYS RUN --control.  A census that reports "1" is indistinguishable from a
census that is broken, and a vacuity confirming your hypothesis is the hardest
kind to catch.  --control prints the whole-image multiplicity distribution; the
instrument is only trustworthy if it FINDS large unfolded classes (it does: 769
two-copy classes, ten eleven-copy classes, and a 40-byte body at 278 copies).

Usage:
    python3 tools/retail_body_multiplicity.py --survivor 0x8247b020
    python3 tools/retail_body_multiplicity.py --control
    python3 tools/retail_body_multiplicity.py --family-recheck erase,list --size 84
"""
import argparse, collections, json, os, re, struct, sys

ROOT = os.environ.get("RB3_ROOT", os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))
os.environ.setdefault("RB3_ROOT", ROOT)
from retail_body import Img  # noqa: E402

SYMS = os.path.join(ROOT, "config/45410914/symbols.txt")
SMAP = os.path.join(ROOT, "scripts/target_symbol_map.json")
_LINE = re.compile(r'^(\S+)\s*=\s*\.text:0x([0-9A-Fa-f]+);.*?type:function.*?size:0x([0-9A-Fa-f]+)')


def analyze(img, va, size):
    """(masked_words, resolved_branch_destinations) or None if unreadable."""
    b = img.read(va, size)
    if len(b) < size:
        return None
    ws = [struct.unpack_from('>I', b, i)[0] for i in range(0, size, 4)]
    masked, norm = [], []
    for i, w in enumerate(ws):
        op, addr = w >> 26, va + 4 * i
        if op == 18:                                   # I-form b / bl
            d = w & 0x03FFFFFC
            if d & 0x02000000:
                d -= 0x04000000
            masked.append(w & 0xFC000003)
        elif op == 16:                                 # B-form bc
            d = w & 0x0000FFFC
            if d & 0x8000:
                d -= 0x10000
            masked.append(w & 0xFFFF0003)
        else:
            masked.append(w)
            continue
        t = d if (w >> 1) & 1 else addr + d            # AA=1 => absolute
        norm.append((i, 'INT', t - va) if va <= t < va + size else (i, 'EXT', t))
    return tuple(masked), tuple(norm)


def extents():
    for line in open(SYMS):
        m = _LINE.match(line.strip())
        if not m:
            continue
        va, sz = int(m.group(2), 16), int(m.group(3), 16)
        if sz >= 4 and sz % 4 == 0:
            yield va, sz


def classes(img):
    g = collections.defaultdict(list)
    for va, sz in extents():
        r = analyze(img, va, sz)
        if r:
            g[(sz,) + r].append(va)
    return g


def namer():
    m = json.load(open(SMAP))
    return lambda va: str(m.get("0x%08x" % va) or "fn_%08X" % va)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--survivor", help="hex VA of the body to count")
    ap.add_argument("--control", action="store_true",
                    help="whole-image multiplicity distribution (proves the census "
                         "can find duplicates at all -- ALWAYS run this)")
    ap.add_argument("--family-recheck", help="comma-separated substrings of the mapped name")
    ap.add_argument("--size", type=int, help="restrict --family-recheck to this byte size")
    ap.add_argument("--slide", action="store_true",
                    help="with --survivor: also scan EVERY 4-byte-aligned .text window, "
                         "ignoring symbol boundaries (guards against a mis-carved extent)")
    a = ap.parse_args()
    img, nm = Img(), namer()

    if a.control:
        g = classes(img)
        mult = collections.Counter(len(v) for v in g.values())
        print("scanned %d extents -> %d distinct (size,masked,resolved) classes"
              % (sum(len(v) for v in g.values()), len(g)))
        for k in sorted(mult)[:12]:
            print("   %4d copies : %6d class(es)" % (k, mult[k]))
        big = sorted(((len(v), k[0], v[0]) for k, v in g.items()), reverse=True)[:5]
        print("  largest unfolded classes (identical INCLUDING resolved destinations):")
        for c, sz, va in big:
            print("     %3d copies, %d B, e.g. 0x%08x %s" % (c, sz, va, nm(va)[:60]))

    if a.family_recheck:
        subs = a.family_recheck.split(",")
        rows = [(va, sz) for va, sz in extents()
                if (a.size is None or sz == a.size) and all(s in nm(va) for s in subs)]
        forms = {analyze(img, va, sz)[0] for va, sz in rows}
        print("\n--family-recheck %s size=%s: %d extent(s), %d DISTINCT masked form(s)"
              % (a.family_recheck, a.size, len(rows), len(forms)))
        print("   (distinct forms == count  =>  they are NOT copies and can never fold)")
        for va, sz in rows:
            ext = [x for x in analyze(img, va, sz)[1] if x[1] == 'EXT']
            print("   0x%08x %s  ext=%s" % (va, nm(va)[:52],
                                            " ".join("%d:%s" % (i, nm(t)[:22]) for i, _k, t in
                                                     [(i, k, t) for i, k, t in ext])))

    if a.survivor:
        surv = int(a.survivor, 16)
        g = classes(img)
        hit = [(k, v) for k, v in g.items() if surv in v]
        if not hit:
            print("\nsurvivor 0x%08x: NOT a .text function extent in symbols.txt" % surv)
            return 2
        (k, v) = hit[0]
        print("\nsurvivor 0x%08x (%d B): %d distinct RETAIL address(es) hold this body"
              % (surv, k[0], len(v)))
        for x in v:
            print("     0x%08x  %s" % (x, nm(x)[:80]))
        if a.slide:
            size = k[0]
            ref = analyze(img, surv, size)
            name, vaddr, vsize, rawptr, _rs = [s for s in img.secs if s[0] == '.text'][0]
            first, hits, cand = struct.unpack_from('>I', img.buf, rawptr)[0], [], 0
            first = struct.unpack_from('>I', img.read(surv, 4), 0)[0]
            for va in range(vaddr, vaddr + vsize - size, 4):
                if struct.unpack_from('>I', img.buf, rawptr + (va - vaddr))[0] != first:
                    continue
                cand += 1
                r = analyze(img, va, size)
                if r and r[0] == ref[0]:
                    hits.append(va)
            print("   [slide] %d candidate windows by first word; %d masked-equal: %s"
                  % (cand, len(hits), " ".join("0x%08x" % h for h in hits)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
