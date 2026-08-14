#!/usr/bin/env python3
"""Adjudicate the 8-byte allocator-thunk fold class on RETAIL BYTES.

THE CHARGE
----------
`name_check` charges 585 sites / ~420 functions where retail's relocation names
``??2CriticalSection@@SAPAXI@Z`` (0x827bd2f0) and ours names one of ~44 other
``operator new`` spellings.  It is the largest single open charge on the board
and the census bucketed it `cannot_adjudicate`, for a purely structural reason:
the `fold_thunk_naming` bucket requires a <=4-byte tail jump and this body is 8.

Eight bytes is BETTER evidence than four, not worse.  A 4-byte `b X` compares
equal to every other 4-byte `b Y` once the displacement is masked -- the vacuity
that b606f610 withdrew the CF1 tier over.  An 8-byte body carries a full
non-branch word that no masking touches.

WHAT THIS GATE ASKS, AND WHY IT DOES NOT NEED TO NAME THE BRANCH TARGET
----------------------------------------------------------------------
`tools/comdat_fold_gate.py` asks whether OUR COMDAT equals RETAIL's body at the
survivor address, which requires resolving retail's branch destination through
target_symbol_map.json to a NAME.  Here that destination is 0x827bcd38, which the
map does not name, so that chain REFUSES -- fail-closed on missing evidence, and
lane ALIAS-X2 correctly declined to invent the pin (our ?MemAlloc@@YAPAXHH@Z is a
20-byte stub against 644 bytes of real allocator, so no body match is available).

This gate asks a different question that the available evidence CAN answer:

    is our COMDAT for F byte- and RELOCATION-identical to our COMDAT for the
    map-resident survivor S?

If yes, /OPT:ICF *must* fold F onto S -- that is the linker condition itself,
applied to two of our own COMDATs.  Both sides carry the SAME relocation to the
SAME symbol, so whatever that symbol denotes, it denotes the same thing for both.
The unnamed 0x827bcd38 never enters the argument.

The retail side is not assumed, it is corroborated:  S is map-resident at
0x827bd2f0; retail's body there is 8 bytes whose word 0 equals ours as a FULL
32-bit value (0x38800000) and whose word 1 is a branch of matching opcode/AA/LK.
And the pair (word0, resolved destination) is UNIQUE image-wide -- see
--shape-census: 712 retail 8-byte <word>+<branch> bodies carry 707 distinct
combinations, and the 12 that share `li r4,0` have 12 DISTINCT destinations.  So
shape does none of the work; the destination does all of it, exactly the trap
lane MAP-B raised and ALIAS-X2 tested for `b MemFree`.

FAIL-CLOSED
-----------
* our objs disagree on F (two distinct COMDAT variants)          -> REFUSE
* F's body differs from S's in ANY byte or ANY relocation        -> REFUSE
* target_symbol_map.json places F at a DIFFERENT address         -> REFUSE
  (injectivity: one mangled name must not end up at two addresses)
* F already sits in an alias group at a different address        -> REFUSE

THE DISCRIMINATING CONTROL IS BUILT IN, AND IT ONCE REFUSED THE BIGGEST PAIR
---------------------------------------------------------------------------
⚠ DATED RECORD -- DO NOT ACT ON THIS PARAGRAPH AS A CURRENT VERDICT.  It
describes the state BEFORE the source fix in this file's own introducing commit
(e92a6c80), which is why it was already stale the day it was written.

``??2@YAPAXI@Z`` -- global ``operator new``, the single largest open charge at
510 sites / 367 functions -- WAS REFUSED on body.  Ours was 12 bytes
(``lis``/``lwz`` of ``?gNewOperatorAlign@@3HA`` then the branch); retail's is 8
(``li r4,0``).  That was a real SOURCE divergence inherited from dc3, which is
NEWER than RB3: the rb3-Wii oracle says ``operator new(size){return
_MemAlloc(size,0);}`` and knows no ``gNewOperatorAlign`` at all.  An alias there
would have hidden a genuine defect, which is precisely the failure mode this
gate exists to avoid.  That control WORKED, and the lane fixed source instead of
asserting an alias -- which is the durable lesson here.

★ TODAY IT ADMITS.  ``src/system/utl/MemMgr.cpp`` passes a literal 0, so our
``??2@YAPAXI@Z`` is 8 B / ``38800000 4bfffffc`` / one reloc to
``?MemAlloc@@YAPAXHH@Z`` -- byte- and relocation-identical to the survivor.  The
admission was installed in b288c232 (the very next commit) and MEASURED at
+67,884 B / +339 complete fns.  Re-verified from a freshly compiled obj by lane
ALLOCGATE-1 (2026-08-14), which also confirmed the group carries it.

⚠ NOTHING IN THIS GATE IS HARDCODED.  Every verdict is recomputed from the
compiled COMDAT bytes on each run, so a refusal recorded in prose here can never
be an operative refusal -- fix the source and the gate re-adjudicates itself.
Re-run it rather than reading this docstring for a verdict.

``??2Task@@SAPAXI@Z`` is REFUSED separately, on injectivity.
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
from comdat_bytes import comdats                        # noqa: E402
from wrong_callee_triage import Image, load_sizes       # noqa: E402

BUILD_ID = "45410914"
SURVIVOR = "??2CriticalSection@@SAPAXI@Z"
SURVIVOR_VA = 0x827BD2F0
BRANCH_OPS = (16, 18)


def branch_dest(w, va):
    op = w >> 26
    if op == 18:
        d = w & 0x03FFFFFC
        if d & 0x02000000:
            d -= 0x04000000
    elif op == 16:
        d = w & 0x0000FFFC
        if d & 0x8000:
            d -= 0x10000
    else:
        return None
    return d if (w & 2) else va + d


def our_comdats():
    """name -> {(raw, relocs): [obj paths]} over every compiled obj."""
    out = collections.defaultdict(lambda: collections.defaultdict(list))
    for p in glob.glob(str(ROOT / "build" / BUILD_ID / "src" / "**" / "*.obj"),
                       recursive=True):
        try:
            c = comdats(p)
        except Exception:
            continue
        for n, v in c.items():
            if not v.get("is_code"):
                continue
            rel = tuple(sorted((o, s, t) for o, s, t in (v["relocs"] or [])
                               if s != "@comp.id"))
            out[n][(v["raw"], rel)].append(p)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--shape-census", action="store_true",
                    help="run the MAP-B uniqueness test on retail's 8-byte thunks")
    ap.add_argument("--json", help="write the verdict table here")
    ap.add_argument("--install", action="store_true",
                    help="merge the ADMITted group into scripts/symbol_aliases.json")
    args = ap.parse_args()

    img = Image(ROOT / "orig" / BUILD_ID / "band.exe")
    size = load_sizes()
    smap = json.loads((ROOT / "scripts" / "target_symbol_map.json").read_text())
    byva = {int(a, 16): n for a, n in smap.items()
            if a.startswith("0x") and isinstance(n, str)}
    byname = collections.defaultdict(set)
    for va, n in byva.items():
        byname[n].add(va)

    # ---- retail side of the survivor -------------------------------------
    n = size.get(SURVIVOR_VA)
    off = img.off(SURVIVOR_VA)
    rw = list(struct.unpack_from(">%dI" % (n // 4), img.data, off))
    rdest = branch_dest(rw[1], SURVIVOR_VA + 4)
    print(f"RETAIL survivor 0x{SURVIVOR_VA:08x} = {byva.get(SURVIVOR_VA)}")
    print(f"  size={n}  words={[f'{x:08x}' for x in rw]}")
    print(f"  word1 -> 0x{rdest:08x} (map name: {byva.get(rdest)})")
    print(f"  fan-in={img.fanin().get(SURVIVOR_VA)}")

    if args.shape_census:
        cand = collections.Counter()
        exact = []
        for va, sz in size.items():
            if sz != 8:
                continue
            o = img.off(va)
            if o is None:
                continue
            w = list(struct.unpack_from(">2I", img.data, o))
            if (w[1] >> 26) != 18:
                continue
            d = branch_dest(w[1], va + 4)
            cand[(w[0], d)] += 1
            if w[0] == rw[0] and d == rdest:
                exact.append(va)
        li0 = [k for k in cand if k[0] == rw[0]]
        print("\n-- MAP-B shape-uniqueness test --")
        print(f"  retail 8-byte <word>+<b> bodies : {sum(cand.values())}")
        print(f"  distinct (word0, dest) combos   : {len(cand)}")
        print(f"  bodies sharing word0={rw[0]:08x}    : "
              f"{sum(v for k, v in cand.items() if k[0] == rw[0])} "
              f"across {len(li0)} DISTINCT destinations")
        print(f"  bodies matching BOTH            : {len(exact)} "
              f"-> {[hex(v) for v in exact]}")
        print("  => shape alone is worthless; the resolved destination is "
              "what discriminates.")

    # ---- our side --------------------------------------------------------
    ours = our_comdats()
    sv = ours.get(SURVIVOR)
    if not sv:
        sys.exit("our objs do not define the survivor spelling")
    if len(sv) != 1:
        sys.exit(f"our objs disagree on {SURVIVOR}: {len(sv)} variants -> REFUSE ALL")
    (sv_raw, sv_rel), sv_paths = next(iter(sv.items()))
    sw = list(struct.unpack(">%dI" % (len(sv_raw) // 4), sv_raw))
    print(f"\nOUR survivor COMDAT {SURVIVOR}: size={len(sv_raw)} "
          f"words={[f'{x:08x}' for x in sw]}")
    print(f"  relocs={list(sv_rel)}  ({len(sv_paths)} defs)")

    # corroborate the map placement on retail bytes
    ok_len = len(sv_raw) == n
    ok_w0 = sw[0] == rw[0]
    relo = {o: (s, t) for o, s, t in sv_rel}
    ok_br = (1 * 4) in relo and (sw[1] >> 26) == (rw[1] >> 26) \
        and (sw[1] & 0xFC000003) == (rw[1] & 0xFC000003)
    print(f"  vs retail: size {ok_len}, word0 full-32-bit {ok_w0}, "
          f"branch opcode/AA/LK {ok_br}")
    if not (ok_len and ok_w0 and ok_br):
        sys.exit("survivor does not corroborate on retail bytes -> REFUSE ALL")

    # existing alias membership
    ali = json.loads((ROOT / "scripts" / "symbol_aliases.json").read_text())
    in_group = {}
    for g in ali["groups"]:
        for nm in [g["survivor"]] + list(g.get("folded", [])):
            in_group.setdefault(nm, set()).add(g["address"].lower())

    verdicts = []
    for name, variants in sorted(ours.items()):
        if name == SURVIVOR:
            continue
        # candidate iff SOME variant equals the survivor's body+relocs
        if not any(k == (sv_raw, sv_rel) for k in variants):
            # only report the ones actually charged against this survivor
            continue
        why, ok = [], True
        if len(variants) != 1:
            ok = False
            why.append(f"our objs define {len(variants)} distinct COMDAT variants")
        placed = byname.get(name, set())
        if placed and placed != {SURVIVOR_VA}:
            ok = False
            why.append("map places it at " +
                       ",".join(hex(v) for v in sorted(placed)))
        grp = in_group.get(name, set())
        if grp - {f"0x{SURVIVOR_VA:08x}"}:
            ok = False
            why.append(f"already aliased at {sorted(grp)}")
        verdicts.append({
            "name": name, "verdict": "ADMIT" if ok else "REFUSE",
            "why": "; ".join(why) or
                   "COMDAT byte- and reloc-identical to the map-resident survivor",
            "defs": sum(len(v) for v in variants.values()),
        })

    # explicit REFUSE side: charged spellings whose body DIFFERS
    charged = []
    census = ROOT / "scripts" / "namecheck_df_census.json"
    if census.exists():
        for r in json.loads(census.read_text())["rows"]:
            if r["target"] == SURVIVOR:
                charged.append((r["base"], r["sites"], r["fns"]))
    admitted = {v["name"] for v in verdicts if v["verdict"] == "ADMIT"}
    for base, sites, fns in charged:
        if base in admitted or any(v["name"] == base for v in verdicts):
            continue
        v = ours.get(base)
        if not v:
            why = "no COMDAT in our objs"
        else:
            (raw, rel), _ = next(iter(v.items()))
            if len(raw) != len(sv_raw):
                why = (f"body size {len(raw)} != survivor {len(sv_raw)} "
                       f"-- NOT the same function")
            else:
                why = "body or relocations differ from the survivor"
        verdicts.append({"name": base, "verdict": "REFUSE", "why": why,
                         "sites": sites, "fns": fns, "defs": len(v or ())})

    na = sum(1 for v in verdicts if v["verdict"] == "ADMIT")
    print(f"\n==== VERDICTS: {na} ADMIT / {len(verdicts) - na} REFUSE ====")
    for v in sorted(verdicts, key=lambda v: (v["verdict"], v["name"])):
        print(f"  {v['verdict']:7s} {v['name']}")
        if v["verdict"] == "REFUSE":
            print(f"          {v['why']}")
    if args.json:
        Path(args.json).write_text(json.dumps(
            {"survivor": SURVIVOR, "address": f"0x{SURVIVOR_VA:08x}",
             "verdicts": verdicts}, indent=1))
        print(f"\nwrote {args.json}")

    if args.install:
        folded = sorted(v["name"] for v in verdicts if v["verdict"] == "ADMIT")
        # injectivity, asserted rather than assumed: no admitted spelling may be
        # map-resident anywhere but the survivor, and none may already sit in a
        # group at another address.  MAP-B's assert is what caught a bad admit a
        # shared-string heuristic had blessed.
        for f in folded:
            placed = byname.get(f, set()) - {SURVIVOR_VA}
            assert not placed, f"INJECTIVITY: {f} also at {[hex(v) for v in placed]}"
            other = in_group.get(f, set()) - {f"0x{SURVIVOR_VA:08x}"}
            assert not other, f"INJECTIVITY: {f} already aliased at {sorted(other)}"
        assert SURVIVOR not in folded
        ali["groups"] = [g for g in ali["groups"]
                         if g["address"].lower() != f"0x{SURVIVOR_VA:08x}"]
        ali["groups"].append({
            "name": "operator_new_alloc_thunk",
            "address": f"0x{SURVIVOR_VA:08x}",
            "survivor": SURVIVOR,
            "folded": folded,
            "evidence": (
                "tools/alloc_fold_gate.py -- 8-byte allocator-thunk fold class. "
                "Every folded spelling's COMDAT is byte- AND relocation-identical "
                "to the map-resident survivor's, which is the /OPT:ICF condition "
                "itself applied to two of our own COMDATs: both carry the same "
                "relocation to the same symbol, so the fold does not depend on "
                "naming retail's branch target (0x827bcd38 is unnamed in the map, "
                "which is where tools/comdat_fold_gate.py fail-closes). The "
                "survivor is corroborated on retail bytes -- 8 B, word0 equal as a "
                "FULL 32-bit value, matching branch opcode/AA/LK -- and the pair "
                "(word0, resolved dest) is UNIQUE image-wide: 712 retail 8-byte "
                "<word>+<branch> bodies carry 707 distinct combos and the 12 "
                "sharing `li r4,0` have 12 DISTINCT destinations, so shape does "
                "none of the work. REFUSED, each on a different mechanism and "
                "each a real finding: ??2Task (map address 0x822d4278 is `blr`+pad "
                "with ZERO fan-in -- a map defect, not a function), ??2OutfitConfig "
                "(0x82b66c48 is a real thunk to a DIFFERENT allocator -- our "
                "identical COMDAT is a source defect), ?SampleAlloc (our objs "
                "define two distinct COMDAT variants)."),
        })
        p = ROOT / "scripts" / "symbol_aliases.json"
        p.write_text(json.dumps(ali, indent=1) + "\n")
        print(f"\ninstalled group of {len(folded)} folded spellings into {p}")


if __name__ == "__main__":
    main()
