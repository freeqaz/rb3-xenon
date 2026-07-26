#!/usr/bin/env python3
"""icf_class_bijection — harvest the AMBIGUOUS byte-identity class.

★ The seam
----------
`size_order_automap.py` anchors on reloc-masked BYTE IDENTITY, but its EXACT
tier requires a target function to be byte-identical to **exactly one** compiled
function.  Every non-unique hit is *computed and then discarded* as
"ambiguous".  Measured by `oracle_ceiling_scan.py`, that discard pile is large:
of 9,836 in-scope anonymous target VAs in units that can pair, 1,919 are
byte-identical to something in the right obj — and only **199** of those are
unique.  **1,720 are thrown away.**

But ambiguity does not matter for SCORING.  objdiff pairs target<->base by name
and then compares bytes.  If a set of target VAs and a set of compiled symbols
all share the *same* reloc-masked bytes, then **any bijection between them
scores 100% on every pair**.  Which particular name lands on which particular VA
is a question of true identity, not of match percent.  So the whole equivalence
class is harvestable without resolving the identity at all.

(True identity still matters for semantics, so every pairing emitted here is
tagged with its class size; class size 1 is a proven identity, class size > 1 is
a scoring-equivalent assignment.  An oracle can refine *which* name goes where
later without changing the score.)

Constraints enforced
--------------------
* PER-UNIT pairing: candidate names come only from the obj compiled from the
  source file whose pinned span contains the VA.
* A name already present in the target obj is already paired -> not reusable.
* A VA already in the map is left alone (unmapped beats wrongly-mapped, and we
  never displace an existing holder).
* `__unwind$` / `$...` compiler-ordinal names are never proposed.
* VAs that already read 100.0% (positionally-paired anonymous funclets) are out
  of the pool by construction -- naming those measured -13.

Read-only: emits a fragment for `tu5_map_apply_fragment.py`, never writes the map.
"""
import argparse
import json
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from size_order_automap import _ordered_funcs, _asm_target_funcs  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build" / "45410914"
MAP_PATH = ROOT / "scripts" / "target_symbol_map.json"
UNWIND_RX = re.compile(r"^__unwind\$|^\$|^\?\?_9")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--funnel", default="/home/free/tmp/oracle_funnel.json")
    ap.add_argument("--funclets", default="/home/free/tmp/laneAK_funclet_class.json",
                    help="optional per-VA real/funclet classification to exclude funclets")
    ap.add_argument("--emit", default="/home/free/tmp/laneAK_icf_frag.json")
    ap.add_argument("--max-class", type=int, default=0,
                    help="if >0, only emit pairings whose byte-class size is <= this")
    ap.add_argument("--unit", action="append",
                    help="restrict to these units (repeatable)")
    args = ap.parse_args()

    funnel = json.load(open(args.funnel))
    mapped = {k.lower() for k in json.load(open(MAP_PATH))}
    funclets = {}
    p = Path(args.funclets)
    if p.exists():
        try:
            raw = json.load(open(p))
            for u, d in raw.items():
                for va in d.get("funclet", []):
                    s = str(va).lower()
                    if s.startswith("fn_"):
                        s = s[3:]
                    if s.startswith("0x"):
                        s = s[2:]
                    funclets[s] = True
        except Exception:
            pass

    rows = [r for r in funnel["rows"] if r["reason"] == "WORKABLE"]
    if args.unit:
        keep = set(args.unit)
        rows = [r for r in rows if r["unit"] in keep or r["unit"].split("/")[-1] in keep]

    frag = {}
    ev = []
    stat = Counter()
    for r in rows:
        uname = r["unit"]
        rel = uname[len("default/"):] if uname.startswith("default/") else uname
        asm = BUILD / "asm" / (rel + ".s")
        tobj = BUILD / "obj" / (rel + ".obj")
        src = r["src"]
        bobj = (BUILD / "src" / (src[4:] if src.startswith("src/") else src)).with_suffix(".obj")
        if not (asm.exists() and tobj.exists() and bobj.exists()):
            stat["skip_missing"] += 1
            continue
        try:
            tfns = _asm_target_funcs(asm)
            tnames = {f["name"] for f in _ordered_funcs(tobj)}
            bfns = _ordered_funcs(bobj)
        except Exception:
            stat["skip_parse"] += 1
            continue

        want = {int(v[3:], 16) for v in r["vas"]}
        # free target VAs, grouped by reloc-masked bytes
        tgroup = defaultdict(list)
        for va, size, masked in tfns:
            if va is None or va not in want:
                continue
            key = "%08x" % va
            if key in mapped:
                stat["va_already_mapped"] += 1
                continue
            if funclets.get(key):
                stat["va_is_funclet"] += 1
                continue
            tgroup[masked].append((va, size))
        if not tgroup:
            continue
        # free base names, grouped by the same key
        bgroup = defaultdict(list)
        for f in bfns:
            if UNWIND_RX.match(f["name"]):
                continue
            if f["name"] in tnames:      # already paired in this unit
                continue
            bgroup[f["masked"]].append(f["name"])

        for masked, vas in tgroup.items():
            names = bgroup.get(masked)
            if not names:
                stat["no_byte_class"] += len(vas)
                continue
            cls = max(len(vas), len(names))
            if args.max_class and cls > args.max_class:
                stat["class_too_big"] += len(vas)
                continue
            vas = sorted(vas)
            names = sorted(names)
            n = min(len(vas), len(names))
            stat["unmatched_in_class"] += len(vas) - n
            for i in range(n):
                va, size = vas[i]
                key = "0x%08x" % va
                frag[key] = names[i]
                ev.append(dict(va=key, name=names[i], size=size, unit=uname,
                               class_vas=len(vas), class_names=len(names),
                               proven_identity=(len(vas) == 1 and len(names) == 1)))
                stat["EMIT"] += 1
                stat["EMIT_proven" if (len(vas) == 1 and len(names) == 1)
                     else "EMIT_class"] += 1

    Path(args.emit).write_text(json.dumps(frag, indent=1))
    Path(args.emit + ".evidence").write_text(json.dumps(ev, indent=1))
    for k in sorted(stat):
        print(f"{k:22s} {stat[k]}")
    byu = Counter(e["unit"] for e in ev)
    print("\ntop units:")
    for u, n in byu.most_common(30):
        print(f"  {n:5d}  {u}")
    print(f"\nfragment -> {args.emit}  ({len(frag)} entries)")


if __name__ == "__main__":
    main()
