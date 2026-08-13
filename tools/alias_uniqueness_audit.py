#!/usr/bin/env python3
"""ADVERSARIAL audit of the NC-REC our-side-COMDAT-identity alias admits.

THE GAP IN THE ADMITTING GATE
-----------------------------
tools/ourside_fold_sweep.py admits a folded spelling F onto survivor S when our
COMDAT(F) == our COMDAT(S), arguing /OPT:ICF *must* then place them at one
address.  Its gate 3 corroborates that retail's body at addr(S) IS that shared
COMDAT.  What it never asks is the CONVERSE:

    how many DISTINCT retail addresses carry that body, and is addr(S) the one
    retail's callers and vtables actually REFERENCE?

NC-REC ran that census for the allocator class (exactly one image-wide body is
`li r4,0 ; b 0x827bcd38`) and did NOT run it for the 53 sweep groups.

⛔ UNIQUENESS ALONE IS A MISLEADING INSTRUMENT -- MEASURED, DO NOT REPEAT IT
---------------------------------------------------------------------------
The first version of this tool stopped at "how many retail addresses carry this
body" and flagged 3 groups as DUPLICATED, one with 31 addresses carrying
`li r3,0 ; blr`.  Read as a refutation of "identical COMDATs must fold", that is
WRONG.  Adding the reference test dissolves it:

    0x823591e8  fan-in 38, and 2,770 POINTERS to it in .rdata/.data
    27 of the other 30 addresses:  fan-in 0, data pointers 0

i.e. ICF folded the whole `return 0` virtual class onto ONE representative that
2,770 vtable slots share, and the other addresses are unreferenced spans dtk
labelled as functions.  An unreferenced address cannot be anybody's callee, so
it cannot produce false credit.  The question that discriminates is not "is the
body unique" but "is addr(S) the REFERENCE-DOMINANT address for that body".

⛔ AND .pdata IS NOT A FUNCTION CENSUS FOR THIS STRATUM
------------------------------------------------------
Cross-checking the duplicates against `.pdata` BeginAddress returned 0 of 31 --
including 0x827bd2f0, the allocator survivor with fan-in 1,048 that NC-REC
proved is a real function.  Eight-byte leaf stubs touch neither the stack nor
LR, so they carry NO unwind record.  CD-7 established near-total ICF folding
("6 surplus / 32,580 in HMX code") over a population of ".pdata-sized
functions", which therefore EXCLUDES this entire stratum by construction.  CD-7
licenses no claim about 8-byte stubs in either direction -- do not cite it here.

THE COMPARATOR is the sweep's own `corroborates()` (same size; non-relocated
words equal as FULL 32-bit values; relocated words equal in opcode/AA/LK) TIGHTENED
with destination equality: a genuine fold requires the relocated word to reach the
SAME target, so candidates that merely share an opcode are excluded rather than
counted as duplicates.
"""
import argparse
import collections
import glob
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "scripts"))
from comdat_bytes import comdats                    # noqa: E402
from wrong_callee_triage import Image, load_sizes   # noqa: E402

BUILD_ID = "45410914"


def resolve_dests(img, va, n, relo):
    """resolved targets of the relocated words of the body at `va`."""
    o = img.off(va)
    d = []
    for i in range(n // 4):
        if (i * 4) not in relo:
            continue
        w = struct.unpack_from(">I", img.data, o + 4 * i)[0]
        if (w >> 26) == 18 and not ((w >> 1) & 1):
            li = w & 0x03FFFFFC
            if li & 0x02000000:
                li -= 0x04000000
            d.append(("rel", va + 4 * i + li))
        else:
            d.append(("raw", w))
    return tuple(d)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--groups", required=True)
    ap.add_argument("--json")
    args = ap.parse_args()

    img = Image(ROOT / "orig" / BUILD_ID / "band.exe")
    size = load_sizes()
    fan = img.fanin()

    # pointers to .text held in .rdata/.data (vtable slots, jump tables, ctors)
    ptr = collections.Counter()
    for name, va, raw, rawsz in img.secs:
        if name not in (".rdata", ".data"):
            continue
        for i in range(rawsz // 4):
            w = struct.unpack_from(">I", img.data, raw + 4 * i)[0]
            if img.text[0] <= w < img.text[0] + img.text[2]:
                ptr[w] += 1
    refs = lambda a: fan.get(a, 0) + ptr.get(a, 0)                  # noqa: E731

    bysize = collections.defaultdict(list)
    for va, n in size.items():
        if n < 4 or n % 4:
            continue
        o = img.off(va)
        if o is None or o + n > len(img.data):
            continue
        bysize[n].append((va, struct.unpack_from(">%dI" % (n // 4), img.data, o)))

    ours = collections.defaultdict(dict)
    for p in glob.glob(str(ROOT / "build" / BUILD_ID / "src" / "**" / "*.obj"),
                       recursive=True):
        try:
            c = comdats(p)
        except Exception:
            continue
        for nm, v in c.items():
            if not v.get("is_code"):
                continue
            rel = tuple(sorted((o, s, t) for o, s, t in (v["relocs"] or [])
                               if s != "@comp.id"))
            ours[nm][(v["raw"], rel)] = True

    groups = json.loads(Path(args.groups).read_text())
    out = []
    for g in groups:
        S, A = g["survivor"], int(g["address"], 16)
        variants = ours.get(S)
        if not variants or len(variants) != 1:
            out.append({"addr": g["address"], "survivor": S, "verdict": "NO_COMDAT",
                        "n_folded": len(g["folded"])})
            continue
        raw, rel = next(iter(variants))
        n = len(raw)
        ow = list(struct.unpack(">%dI" % (n // 4), raw))
        relo = {off for off, _s, _t in rel}
        cand = []
        for va, rw in bysize.get(n, []):
            ok = True
            for i, (x, y) in enumerate(zip(rw, ow)):
                if (i * 4) in relo:
                    if (x >> 26) != (y >> 26) or ((x >> 26) in (16, 18)
                                                  and (x & 3) != (y & 3)):
                        ok = False
                        break
                elif x != y:
                    ok = False
                    break
            if ok:
                cand.append(va)
        # tighten: a real fold needs the SAME resolved destination as addr(S)
        if relo and A in cand:
            want = resolve_dests(img, A, n, relo)
            cand = [va for va in cand if resolve_dests(img, va, n, relo) == want]

        rival = [va for va in cand if va != A and refs(va) > 0]
        best = max(cand, key=refs) if cand else None
        if A not in cand:
            v = "GATE3_CONTRADICTION"
        elif refs(A) == 0:
            v = "SURVIVOR_UNREFERENCED"
        elif best != A:
            v = "NOT_DOMINANT"
        elif not rival:
            v = "SOLE_REFERENCED"
        else:
            v = "DOMINANT"
        out.append({
            "addr": g["address"], "survivor": S, "name": g.get("name"),
            "n_folded": len(g["folded"]), "size": n, "nrelocs": len(rel),
            "candidates": len(cand), "refs_survivor": refs(A),
            "fanin_survivor": fan.get(A, 0), "dataptr_survivor": ptr.get(A, 0),
            "referenced_rivals": {"0x%08x" % va: refs(va) for va in sorted(rival)},
            "verdict": v})

    c = collections.Counter(o["verdict"] for o in out)
    print("VERDICTS:", dict(c), "\n")
    for o in sorted(out, key=lambda o: (o["verdict"] != "SOLE_REFERENCED",
                                        -o["n_folded"])):
        print(f"  {o['verdict']:22s} {o['addr']} n={o['n_folded']:4d} "
              f"{o.get('size')}B cand={o.get('candidates')} "
              f"refs(S)={o.get('refs_survivor')} "
              f"(fanin {o.get('fanin_survivor')}/ptr {o.get('dataptr_survivor')}) "
              f"{o['survivor'][:46]}")
        if o.get("referenced_rivals"):
            print(f"        referenced rivals: {o['referenced_rivals']}")
    if args.json:
        Path(args.json).write_text(json.dumps(out, indent=1))
        print(f"\nwrote {args.json}")


if __name__ == "__main__":
    main()
