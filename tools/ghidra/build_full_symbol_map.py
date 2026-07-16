#!/usr/bin/env python3
"""
build_full_symbol_map.py — produce the FULL base-program naming map for Ghidra.

apply_symbols.py consumes {"0x82...": {"symbol": <mangled>, "demangled"?: ...}}.
Two sources feed it here:

  1. scripts/target_symbol_map.json  ({VA: <mangled>})  — the authoritative
     VA->mangled table for the whole oracle-identified universe (~15k). This is
     the objdiff pairing table; every entry is our best identification of the
     function at that VA. Naming all of them makes the base TU0 program a proper
     fully-named reference / VT source (the "bank" analog).

  2. tools/ghidra/rb3_symbol_map.json  ({VA: {symbol, demangled, percent, unit}})
     — the RICH subset produced by build_symbol_map.py for functions we have
     actually compiled+matched (carries demangled text, match %, and unit). We
     overlay these so matched functions also get the [decomp] demangled comment.

Output (default tools/ghidra/rb3_symbol_map.full.json) is the union: every
target VA named from (1), enriched by (2) where available. Gitignored/regenerable.

Usage:
  python3 tools/ghidra/build_full_symbol_map.py
      [--target scripts/target_symbol_map.json]
      [--rich tools/ghidra/rb3_symbol_map.json]
      [--out tools/ghidra/rb3_symbol_map.full.json]
"""
import argparse
import json
import os

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def norm_va(va):
    """Normalize an address key to lowercase 0x-prefixed hex."""
    if isinstance(va, int):
        return "0x%08x" % va
    s = str(va).strip().lower()
    if not s.startswith("0x"):
        s = "0x" + s
    return s


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--target", default=os.path.join(REPO, "scripts/target_symbol_map.json"))
    ap.add_argument("--rich", default=os.path.join(REPO, "tools/ghidra/rb3_symbol_map.json"))
    ap.add_argument("--out", default=os.path.join(REPO, "tools/ghidra/rb3_symbol_map.full.json"))
    args = ap.parse_args()

    with open(args.target) as f:
        target = json.load(f)

    # Honor the map's own _denylist (VAs that must NOT be named — ambiguous /
    # ICF-folded / known-bad). It is a meta-key, not a VA->symbol row.
    denylist = {norm_va(v) for v in target.get("_denylist", []) if v}

    out = {}
    dropped_meta = denied = 0
    for va, val in target.items():
        if not str(va).lower().lstrip().startswith("0x"):
            dropped_meta += 1  # meta-keys like "_denylist"
            continue
        # values are mangled strings; tolerate a dict shape defensively
        sym = val.get("symbol") if isinstance(val, dict) else val
        if not sym or not isinstance(sym, str):
            continue
        k = norm_va(va)
        if k in denylist:
            denied += 1
            continue
        out[k] = {"symbol": sym}

    rich_used = rich_skipped = 0
    if os.path.exists(args.rich):
        with open(args.rich) as f:
            rich = json.load(f)
        for va, info in rich.items():
            if not isinstance(info, dict) or not info.get("symbol"):
                continue
            k = norm_va(va)
            base = out.get(k)
            # Only enrich a target VA when the rich entry AGREES on the symbol.
            # target_symbol_map.json is authoritative; the rich map is a separate
            # (potentially stale) source. After a target re-base (e.g. TU0->TU5)
            # the rich map's VAs/symbols no longer line up — this drops those
            # instead of injecting wrong names. Matching entries just add the
            # demangled/percent/unit metadata.
            if base is not None and base.get("symbol") == info.get("symbol"):
                out[k] = info
                rich_used += 1
            else:
                rich_skipped += 1

    with open(args.out, "w") as f:
        json.dump(out, f, indent=0)
    print("[build_full_symbol_map] target=%d (meta_skipped=%d denied=%d) "
          "rich_enriched=%d rich_skipped=%d -> %d entries written to %s"
          % (len(target), dropped_meta, denied, rich_used, rich_skipped, len(out), args.out))


if __name__ == "__main__":
    main()
