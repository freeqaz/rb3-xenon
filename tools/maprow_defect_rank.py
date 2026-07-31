#!/usr/bin/env python3
"""Rank target_symbol_map.json rows by EVIDENCE OF BEING WRONG.

Why this exists
---------------
``tools/zerocall_screen.py`` (lane CB-11) generates candidates: retail map names
with ZERO ``IMAGE_REL_PPC_REL24`` call sites anywhere in our compiled tree. That
screen is deliberately a candidate generator, and its own docstring names the
ambiguity it cannot resolve -- a zero-call name is equally consistent with

  (b) a WRONG MAP ROW  -- the row names the wrong function at that address, and
  (c) an ICF FOLD-ALIAS -- retail folded our body into a byte-identical sibling
      and the map happened to name the sibling.

Class (c) is not a defect and must NOT be "fixed" by repointing the map; it
belongs in ``scripts/symbol_aliases.json``. Repointing a fold-alias would make
the map *less* correct while the metric happily went up, because the metric is
blind to attribution (a null leg with 64 deliberately wrong names measured
IDENTICAL to the real one).

This tool adds the three discriminators that separate (b) from (c).

Discriminator 1 -- RETAIL BODY SIZE (the strong one)
    ICF folds bodies that are byte-identical. Two unrelated functions of 200+
    bytes are essentially never byte-identical, so a fold at a LARGE body is
    implausible; a fold at ``lwz r3,K(r3); blr`` (8 bytes) is near-certain.
    Empirically the trivial fold groups are indexed by (instruction pattern,
    member offset): every 8-byte getter with the same K folds to ONE survivor
    (K=0x0 at 0x8274a9a8, K=0x10 at 0x8252e068).

Discriminator 2 -- DOMINANCE / MARGIN, not absolute score
    A wrong map row is wrong the SAME way everywhere: our build consistently
    calls ONE other function where retail names the mapped one. A fold-alias
    group is promiscuous -- the mapped survivor stands in for many unrelated
    callees, so the disagreements scatter across many base names. So a high
    dominant-share is evidence for (b); a low share is evidence for (c).
    Judge on MARGIN between the top two hypotheses, never on absolute score.

Discriminator 3 -- CALLEE PLAUSIBILITY
    The dominant alternative must itself be a function our build really calls
    (nonzero REL24 count), otherwise the "alternative" is noise.

None of these is a verdict on its own, and the tool prints them separately
rather than collapsing them into one number, precisely so a reviewer can see
WHICH evidence fired. Act on the CONJUNCTION of two independent instruments.

CAVEAT -- STILL A CANDIDATE GENERATOR
    A high-ranked row means "go read the retail body at this address and check
    whether it contradicts the name". It does not mean "repoint this row".
    Confirmation requires a NON-metric instrument: the retail body's structure,
    its string/callee references, and a source oracle (rb3-Wii for game code,
    dc3 for engine code).

Usage
-----
  python3 tools/maprow_defect_rank.py --sites ~/tmp/cb9_allsites.pkl \\
      --index ~/tmp/cc2_relindex.pkl [--min-size 48] [--top 40]

Always eyeball the printed positive controls: the two rows lane CB-11 already
proved wrong must rank near the top, or the screen has silently broken.
"""
import argparse
import collections
import json
import os
import pickle
import re
import sys

MAP_PATH = "scripts/target_symbol_map.json"
ASM_DIR = "build/45410914/asm"
OBJ_ROOT = "build/45410914/src"

# Retail bodies at or below this many bytes are presumed ICF-foldable: a body
# this small is very likely byte-identical to some unrelated sibling.
TRIVIAL_FOLD_BYTES = 16


def retail_sizes(asm_dir=ASM_DIR):
    """address -> retail function size, from the dtk-split .s headers.

    Header line looks like:
      # .text:0x9004 | 0x82295A30 | size: 0x25C
    """
    rx = re.compile(
        r"#\s+\.text:0x[0-9A-Fa-f]+\s+\|\s+0x([0-9A-Fa-f]+)\s+\|\s+size:\s+0x([0-9A-Fa-f]+)")
    sz = {}
    if not os.path.isdir(asm_dir):
        sys.exit("no %s -- build first" % asm_dir)
    for f in os.listdir(asm_dir):
        if not f.endswith(".s"):
            continue
        with open(os.path.join(asm_dir, f), errors="replace") as fh:
            for line in fh:
                m = rx.match(line)
                if m:
                    sz["0x" + m.group(1).lower()] = int(m.group(2), 16)
    return sz


def load_map(path=MAP_PATH):
    """Load the map, HARD-FAILING on duplicate keys.

    json.load keeps the LAST duplicate, so an applier that inserts at the top
    produces a phantom edit: clean-looking diff, zero measured delta. Any tool
    that reads this file must refuse to run on a duplicated map rather than
    silently analyse the wrong one.
    """
    pairs = []
    m = json.load(open(path), object_pairs_hook=lambda kv: pairs.extend(kv) or dict(kv))
    keys = [k for k, _ in pairs]
    if len(keys) != len(set(keys)):
        dupes = [k for k, c in collections.Counter(keys).items() if c > 1]
        sys.exit("DUPLICATE KEYS in %s: %s" % (path, dupes[:10]))
    return m, len(keys)


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--sites", required=True,
                    help="census pickle: list of (unit, fn, [(kind, target_name, base_name)...])")
    ap.add_argument("--index", required=True, help="REL24 call index pickle from zerocall_screen.py")
    ap.add_argument("--min-size", type=int, default=0,
                    help="only show rows whose RETAIL body is at least this many bytes")
    ap.add_argument("--top", type=int, default=40)
    ap.add_argument("--json-out")
    a = ap.parse_args()

    total, _nobjs = pickle.load(open(a.index, "rb"))
    sites = pickle.load(open(a.sites, "rb"))
    m, nrows = load_map()
    sizes = retail_sizes()

    # invert the map: name -> address (first wins; duplicates are folds/aliases)
    inv = {}
    for k, v in m.items():
        if isinstance(v, str):
            inv.setdefault(v, k)

    tcount = collections.Counter()
    bases = collections.defaultdict(collections.Counter)
    for _unit, _fn, lst in sites:
        for _kind, t, b in lst:
            if not isinstance(t, str) or not isinstance(b, str):
                continue
            tcount[t] += 1
            bases[t][b] += 1

    rows = []
    for t, c in tcount.items():
        if not t or t.startswith(("except_data", "__save", "__rest")):
            continue
        our = total.get(t, 0)
        if our:  # zero-call control: our build DOES call this name -> not this class
            continue
        addr = inv.get(t)
        size = sizes.get(addr) if addr else None
        top = bases[t].most_common(2)
        b1, c1 = top[0]
        share = c1 / c
        margin = (c1 - top[1][1]) / c if len(top) > 1 else 1.0
        rows.append({
            "target_name": t, "addr": addr or "UNMAPPED", "retail_size": size,
            "sites": c, "dominant_base": b1, "dominant_sites": c1,
            "share": round(share, 4), "margin": round(margin, 4),
            "dominant_our_calls": total.get(b1, 0),
            "n_distinct_bases": len(bases[t]),
            "verdict": classify(size, share, total.get(b1, 0)),
        })

    rows.sort(key=lambda r: (r["verdict"] != "DEFECT_CANDIDATE", -r["sites"]))

    print("MAP-ROW DEFECT RANKING")
    print("  map rows=%d   census sites=%d   zero-call target names=%d"
          % (nrows, sum(tcount.values()), len(rows)))
    print("  TRIVIAL_FOLD_BYTES=%d   min-size filter=%d" % (TRIVIAL_FOLD_BYTES, a.min_size))
    print("  CANDIDATE GENERATOR -- confirm each by READING the retail body.\n")

    shown = 0
    for r in rows:
        if a.min_size and (r["retail_size"] or 0) < a.min_size:
            continue
        if shown >= a.top:
            break
        shown += 1
        print("[%s] %d sites  retail_size=%s  share=%.0f%% over %d bases  @%s"
              % (r["verdict"], r["sites"],
                 ("0x%X" % r["retail_size"]) if r["retail_size"] else "?",
                 100 * r["share"], r["n_distinct_bases"], r["addr"]))
        print("     map says : %s" % r["target_name"][:110])
        print("     we call  : %s  (our calls=%d)"
              % (r["dominant_base"][:100], r["dominant_our_calls"]))

    if a.json_out:
        json.dump(rows, open(a.json_out, "w"), indent=1)
        print("\nwrote %d rows -> %s" % (len(rows), a.json_out))


def classify(size, share, dom_our_calls):
    """Separate (b) wrong map row from (c) ICF fold-alias.

    Deliberately conservative: anything small enough to fold plausibly is called
    a fold, because a wrong repoint is worse than a missed one -- it makes the
    map less correct while the (attribution-blind) metric goes up.
    """
    if size is None:
        return "UNSIZED"
    if size <= TRIVIAL_FOLD_BYTES:
        return "TRIVIAL_FOLD"
    if dom_our_calls == 0:
        return "NOISE"          # the "alternative" is not a function we really call
    if share >= 0.80 and size > 48:
        return "DEFECT_CANDIDATE"
    if share < 0.50:
        return "PROMISCUOUS"    # scattered disagreements = fold-alias behaviour
    return "WEAK"


if __name__ == "__main__":
    main()
