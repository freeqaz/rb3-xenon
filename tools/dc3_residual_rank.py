#!/usr/bin/env python3
"""Rank unwired DC3 engine TUs by expected RB3 byte-match yield.

For each residual DC3 system TU (compiled in dc3 but not wired in our
objects.json), parse its per-TU .obj functions, reloc-mask + hash them, and
look up each hash in the RB3 .text aggregate index (also reloc-masked). A
function is a 1:1 content match when its masked hash is unique on BOTH sides.

Ranking signals per TU:
  n_dc3        - DC3 functions >= min_size
  n_match      - DC3 fns with a 1:1 RB3 content match
  n_unpinned   - of those, RB3 addr NOT already pinned in splits.txt (= new yield)
  best_run     - longest contiguous run (by RB3 address order) of unpinned hits
  span         - [min,max] RB3 addr of the unpinned hits, + how much of that
                 span is already pinned (cluster purity)
"""
import argparse, glob, json, os, struct, sys, bisect
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dc3_content_match import read_coff_functions, rb3_addr_of
from fuzzy_content_match import parse_splits

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RB3_GLOB = os.path.join(ROOT, "build/45410914/obj/auto_03_*_text.obj")
DC3_SYS = os.path.join(ROOT, "../dc3-decomp/build/373307D9/obj/system")
TSM = os.path.join(ROOT, "scripts/target_symbol_map.json")
SPLITS = os.path.join(ROOT, "config/45410914/splits.txt")
OUR_OBJECTS = os.path.join(ROOT, "config/45410914/objects.json")
DC3_OBJECTS = os.path.join(ROOT, "../dc3-decomp/config/373307D9/objects.json")


def status_of(v):
    return v if isinstance(v, str) else v.get("status", "?")


def enumerate_residual():
    """DC3 `system` TUs (engine) whose basename is NOT wired in our objects.json.
    Returns [{basename, dc3_path, dc3_status}]."""
    ours = json.load(open(OUR_OBJECTS))
    dc3 = json.load(open(DC3_OBJECTS))
    our_names = set()
    for g, gv in ours.items():
        for name in gv.get("objects", {}):
            our_names.add(os.path.basename(name))
    out = []
    for name, ov in dc3["system"]["objects"].items():
        bn = os.path.basename(name)
        if bn not in our_names:
            out.append({"basename": bn, "dc3_path": name, "dc3_status": status_of(ov)})
    out.sort(key=lambda r: r["basename"])
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--residual", default=None,
                    help="optional pre-computed residual JSON; default = enumerate live")
    ap.add_argument("--min-size", type=int, default=16)
    ap.add_argument("--out", default=os.path.expanduser("~/tmp/dc3_residual_ranked.json"))
    args = ap.parse_args()

    tsm = json.load(open(TSM))
    rev_tsm = {v: int(k, 16) for k, v in tsm.items() if k.lower().startswith("0x")}

    # RB3: masked_hash -> {addr: size}
    rb3_by_hash = defaultdict(dict)
    rb3_addr_size = {}
    for f in sorted(glob.glob(RB3_GLOB)):
        for name, code, h, sz in read_coff_functions(f):
            if sz < args.min_size:
                continue
            a = rb3_addr_of(name, rev_tsm)
            if a is None:
                continue
            rb3_by_hash[h][a] = sz
            rb3_addr_size[a] = sz
    print(f"RB3 fns indexed: {len(rb3_addr_size)}", file=sys.stderr)

    # pinned ranges
    splits = parse_splits(SPLITS)
    pins = sorted(splits.values())
    pstarts = [p[0] for p in pins]
    def pinned(a):
        i = bisect.bisect_right(pstarts, a) - 1
        return 0 <= i < len(pins) and pins[i][0] <= a < pins[i][1]

    residual = json.load(open(args.residual)) if args.residual else enumerate_residual()
    print(f"residual DC3 system TUs not wired: {len(residual)}", file=sys.stderr)
    rows = []
    for r in residual:
        bn = r["basename"]
        objpath = os.path.join(DC3_SYS, *r["dc3_path"].split("/")[1:])
        objpath = objpath[:-4] + ".obj" if objpath.endswith(".cpp") else objpath
        # locate the .obj (path is system/<sub>/Foo.cpp -> build .../system/<sub>/Foo.obj)
        rel = r["dc3_path"]
        objpath = os.path.join(ROOT, "../dc3-decomp/build/373307D9/obj", rel)
        objpath = objpath[:-4] + ".obj"
        if not os.path.exists(objpath):
            rows.append({"tu": bn, "status": "NO_OBJ", "dc3_path": r["dc3_path"]})
            continue
        n_dc3 = 0
        hits = []   # (rb3_addr, size, dc3_name)
        for name, code, h, sz in read_coff_functions(objpath):
            if sz < args.min_size:
                continue
            if name.startswith(("fn_", "sub_", "FUN_", "$")):
                continue
            n_dc3 += 1
            rb3map = rb3_by_hash.get(h)
            if not rb3map:
                continue
            if len(rb3map) == 1:  # 1:1 (unique on RB3 side; dc3 side per-TU is unique by name)
                addr, asz = next(iter(rb3map.items()))
                hits.append((addr, asz, name))
        # dedup hits by addr
        seen = {}
        for addr, asz, name in hits:
            seen[addr] = (asz, name)
        hit_addrs = sorted(seen)
        unpinned = [a for a in hit_addrs if not pinned(a)]
        pinned_hits = [a for a in hit_addrs if pinned(a)]
        # contiguity of unpinned hits: longest run where consecutive unpinned
        # hits are also adjacent in the RB3 function stream (no big gap)
        best_run = 0
        if unpinned:
            # build address-ordered list of ALL rb3 fns to measure adjacency
            run = 1; best_run = 1
            for i in range(1, len(unpinned)):
                # adjacency: gap between consecutive unpinned hits small
                # (within ~0x400 bytes implies same cluster region)
                if unpinned[i] - unpinned[i-1] <= 0x400:
                    run += 1
                else:
                    run = 1
                best_run = max(best_run, run)
        span_lo = min(unpinned) if unpinned else None
        span_hi = max(unpinned) + seen[max(unpinned)][0] if unpinned else None
        rows.append({
            "tu": bn,
            "dc3_path": r["dc3_path"],
            "n_dc3": n_dc3,
            "n_match": len(hit_addrs),
            "n_pinned": len(pinned_hits),
            "n_unpinned": len(unpinned),
            "best_run": best_run,
            "span_lo": "0x%08X" % span_lo if span_lo else None,
            "span_hi": "0x%08X" % span_hi if span_hi else None,
            "unpinned_addrs": ["0x%08X" % a for a in unpinned],
        })

    # rank: by n_unpinned desc, then best_run desc
    rows.sort(key=lambda x: (-(x.get("n_unpinned") or 0), -(x.get("best_run") or 0)))
    json.dump(rows, open(args.out, "w"), indent=1)
    print(f"wrote {args.out}")
    print(f"\n{'TU':32s} {'dc3':>4} {'match':>5} {'pin':>4} {'NEW':>4} {'run':>4}  span")
    for x in rows:
        if x.get("status") == "NO_OBJ":
            print(f"{x['tu']:32s}  NO_OBJ")
            continue
        print(f"{x['tu']:32s} {x['n_dc3']:4d} {x['n_match']:5d} {x['n_pinned']:4d} "
              f"{x['n_unpinned']:4d} {x['best_run']:4d}  {x['span_lo']}..{x['span_hi']}")


if __name__ == "__main__":
    main()
