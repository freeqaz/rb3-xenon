#!/usr/bin/env python3
"""oracle_ceiling_scan — the STRICT CEILING of the map-coverage channel.

A `target_symbol_map` entry only converts an anonymous `fn_<VA>` into a strict
(100.0%) match if, once paired, the two bodies are actually identical.  So the
ceiling of the whole identification channel is:

    how many in-scope anonymous target functions are reloc-masked
    BYTE-IDENTICAL to some code symbol emitted by the obj of the unit whose
    pinned span contains them?

Anything below that ceiling is a *body-divergence* problem, not an
*identification* problem — no oracle (rb3-Wii, DC3, BinDiff, Ghidra) can fix it,
because the name is not what is missing.

We report three tiers per VA:
  IDENTICAL   an exact reloc-masked byte match exists in the right obj
              -> naming it pays a strict match (ambiguity aside)
  SAME_SIZE   a same-size candidate exists but no byte match
              -> naming pays partial credit only
  NO_SIZE     not even a same-size candidate
              -> the body is not in our obj at all

Read-only.
"""
import json
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from size_order_automap import _ordered_funcs, _asm_target_funcs  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build" / "45410914"
UNWIND_RX = re.compile(r"^__unwind\$|^\$")


def main():
    funnel = json.load(open(sys.argv[1] if len(sys.argv) > 1
                            else "/home/free/tmp/oracle_funnel.json"))
    rows = [r for r in funnel["rows"] if r["reason"] == "WORKABLE"]
    tier = Counter()
    per_unit = {}
    detail = []
    for r in rows:
        uname = r["unit"]
        rel = uname[len("default/"):] if uname.startswith("default/") else uname
        asm = BUILD / "asm" / (rel + ".s")
        src = r["src"]
        bobj = (BUILD / "src" / (src[4:] if src.startswith("src/") else src)).with_suffix(".obj")
        if not asm.exists() or not bobj.exists():
            tier["skip_missing"] += len(r["vas"])
            continue
        try:
            tfns = _asm_target_funcs(asm)
            bfns = [f for f in _ordered_funcs(bobj) if not UNWIND_RX.match(f["name"])]
        except Exception:
            tier["skip_parse"] += len(r["vas"])
            continue
        by_size = defaultdict(list)
        by_bytes = defaultdict(list)
        for f in bfns:
            by_size[f["size"]].append(f["name"])
            by_bytes[f["masked"]].append(f["name"])
        want = {int(v[3:], 16) for v in r["vas"]}
        c = Counter()
        for va, size, masked in tfns:
            if va is None or va not in want:
                continue
            if masked in by_bytes:
                t = "IDENTICAL"
                cand = by_bytes[masked]
            elif size in by_size:
                t = "SAME_SIZE"
                cand = by_size[size]
            else:
                t = "NO_SIZE"
                cand = []
            tier[t] += 1
            c[t] += 1
            if t == "IDENTICAL":
                detail.append(dict(unit=uname, va="0x%08X" % va, size=size,
                                   n_cand=len(cand), cands=cand[:4]))
        per_unit[uname] = dict(c)
    outp = Path("/home/free/tmp/laneAK_ceiling.json")
    outp.write_text(json.dumps(dict(tiers=dict(tier), per_unit=per_unit,
                                    identical=detail), indent=1))
    tot = sum(v for k, v in tier.items() if not k.startswith("skip"))
    print("in-scope VAs examined :", tot)
    for k in ("IDENTICAL", "SAME_SIZE", "NO_SIZE", "skip_missing", "skip_parse"):
        if tier[k]:
            print(f"  {k:12s} {tier[k]:6d}  {100.0*tier[k]/max(tot,1):5.1f}%")
    uniq = [d for d in detail if d["n_cand"] == 1]
    print(f"\nIDENTICAL with a UNIQUE candidate name: {len(uniq)}")
    print(f"IDENTICAL but ambiguous (ICF twins)   : {len(detail)-len(uniq)}")
    byu = Counter(d["unit"] for d in uniq)
    for u, n in byu.most_common(30):
        print(f"   {n:4d}  {u}")
    print(f"\nwrote {outp}")


if __name__ == "__main__":
    main()
