#!/usr/bin/env python3
"""nearidentity_bijection — widen the ICF-class bijection by K differing words.

`icf_class_bijection.py` requires EXACT reloc-masked byte equality. But the two
sides mask relocations by different means: the base side reads the COFF
relocation table, the target side infers relocations from dtk asm operand
syntax. Where that inference is asymmetric, a genuinely identical pair shows up
as differing in a small number of 4-byte words. objdiff's *normalized* diff
ignores relocation address differences, so such a pair still scores 100.0.

Measured distance histogram over the same-size residue (4,219 VAs) after the
exact channel reached fixpoint:
    0 words : 142   (blocked: the only candidate name is already paired)
    1 word  : 448   <- this tool's target
    2 words : 177
    3 words : 200
    >=4     : 3,252

Greedy assignment in ascending distance order, under the same constraints as the
exact tool (per-unit pairing, never reuse a name already paired in the unit,
never touch an already-mapped VA, never propose a `__unwind$` compiler ordinal,
already-100% anonymous funclets out of the pool by construction, EH funclets
excluded via the per-VA classification).

Read-only: emits a fragment, never writes the map.
"""
import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from size_order_automap import _ordered_funcs, _asm_target_funcs  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build" / "45410914"
MAP_PATH = ROOT / "scripts" / "target_symbol_map.json"
UNWIND_RX = re.compile(r"^__unwind\$|^\$|^\?\?_9")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--funnel", default="/home/free/tmp/oracle_funnel_r3.json")
    ap.add_argument("--funclets", default="/home/free/tmp/laneAK_funclet_class.json")
    ap.add_argument("--emit", default="/home/free/tmp/laneAK_near_frag.json")
    ap.add_argument("--max-words", type=int, default=1)
    ap.add_argument("--min-size", type=int, default=0,
                    help="skip functions smaller than this (tiny bodies are noise)")
    args = ap.parse_args()

    funnel = json.load(open(args.funnel))
    mapped = {k.lower() for k in json.load(open(MAP_PATH))}
    fset = set()
    p = Path(args.funclets)
    if p.exists():
        for u, d in json.load(open(p)).items():
            for v in d.get("funclet", []):
                s = str(v).lower()
                fset.add(s[3:] if s.startswith("fn_") else s.lstrip("0x"))

    frag, ev, stat = {}, [], Counter()
    for r in funnel["rows"]:
        if r["reason"] != "WORKABLE":
            continue
        rel = r["unit"][len("default/"):] if r["unit"].startswith("default/") else r["unit"]
        asm = BUILD / "asm" / (rel + ".s")
        tobj = BUILD / "obj" / (rel + ".obj")
        src = r["src"]
        bobj = (BUILD / "src" / (src[4:] if src.startswith("src/") else src)).with_suffix(".obj")
        if not (asm.exists() and tobj.exists() and bobj.exists()):
            continue
        try:
            tf = _asm_target_funcs(asm)
            tnames = {f["name"] for f in _ordered_funcs(tobj)}
            bf = _ordered_funcs(bobj)
        except Exception:
            continue
        want = {int(v[3:], 16) for v in r["vas"]}
        tgt = []
        for va, size, masked in tf:
            if va is None or va not in want or size < args.min_size:
                continue
            k = "%08x" % va
            if k in mapped or k in fset:
                continue
            tgt.append((va, size, masked))
        if not tgt:
            continue
        cands = [f for f in bf
                 if not UNWIND_RX.match(f["name"]) and f["name"] not in tnames
                 and f["size"] >= args.min_size]
        pairs = []
        for va, size, masked in tgt:
            for f in cands:
                if f["size"] != size:
                    continue
                d = sum(1 for i in range(0, size, 4)
                        if masked[i:i + 4] != f["masked"][i:i + 4])
                if d <= args.max_words:
                    pairs.append((d, va, size, f["name"]))
        pairs.sort(key=lambda x: (x[0], x[1], x[3]))
        uva, unm = set(), set()
        for d, va, size, name in pairs:
            if va in uva or name in unm:
                continue
            uva.add(va)
            unm.add(name)
            frag["0x%08x" % va] = name
            ev.append(dict(va="0x%08x" % va, name=name, size=size,
                           word_diff=d, unit=r["unit"]))
            stat["EMIT_d%d" % d] += 1
    Path(args.emit).write_text(json.dumps(frag, indent=1))
    Path(args.emit + ".evidence").write_text(json.dumps(ev, indent=1))
    for k in sorted(stat):
        print(f"{k:14s} {stat[k]}")
    byu = Counter(e["unit"] for e in ev)
    for u, n in byu.most_common(20):
        print(f"  {n:4d}  {u}")
    print(f"\nfragment -> {args.emit}  ({len(frag)} entries)")


if __name__ == "__main__":
    main()
