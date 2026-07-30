#!/usr/bin/env python3
"""Audit whether an address-keyed analysis index is still LIVE against the
current target binary — or is dead TU0-era data that merely still parses.

Why this exists
---------------
Several repo-root JSON indices (`unified_id*.json`, `dc3_oracle.json`,
`global_fuzzy_pairs.json`, ...) map RB3-360 retail virtual addresses to names /
source TUs. They were generated against the **TU0** build. On 2026-07-15 the
project flipped to **TU5** — a different build with different function
addresses — which silently invalidated every one of them. They still load, still
parse, still have thousands of entries, and still produce plausible-looking
output. They just no longer mean anything.

That failure mode (dead data that parses) cost lane BW-2 real budget. This tool
makes it a one-command check.

The test (and why a raw hit-rate is NOT enough)
-----------------------------------------------
A naive "what fraction of the index's addresses are real function starts?" is
misleading: retail function starts are 4-byte aligned and spatially clustered,
so ANY list of plausible code addresses scores a few percent by chance. So we
compare against a NULL: re-run the same test with the whole index shifted by a
random offset. A live index scores ~100% and crushes the null; a dead one sits
on top of it.

    ratio = hit% / null%      >=4 LIVE | 1.5-4 WEAK | <1.5 DEAD (== noise)

Calibration on this repo (2026-07-30, TU5):
    scripts/target_symbol_map.json   99.79%  null 2.73%  ratio 36.5  LIVE
    unified_id_rb3wii.json            4.27%  null 2.90%  ratio  1.47 DEAD
A useful tell beyond the ratio: for a dead index, some ARBITRARY shift usually
scores HIGHER than the true offset. That never happens to a live one.

Usage
-----
    tools/index_liveness_audit.py                    # audit the known indices
    tools/index_liveness_audit.py foo.json           # audit specific file(s)
    tools/index_liveness_audit.py --field rb3_va x.json

Exit status: 1 if any audited index is DEAD (so it can gate CI / a pre-flight).
"""
import argparse
import json
import os
import random
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SYMBOLS = os.path.join(ROOT, "config", "45410914", "symbols.txt")

# rb3-side address field names seen across the indices, in preference order.
# (dc3_oracle.json carries BOTH `dc3_va` and `rb3_va`; picking the rb3 one is
# load-bearing — auditing the dc3 side would falsely report it dead.)
ADDR_FIELDS = ("rb3_addr", "rb3_va", "addr", "address", "va")

# Some indices (autoid.json) carry no address field at all — the VA is embedded
# in dtk's anonymous symbol name, `fn_<8hex>`.
_FN_NAME = re.compile(r"^fn_([0-9A-Fa-f]{8})$")
FN_FIELDS = ("fn", "rb3_fn", "name", "symbol")

DEFAULT_TARGETS = [
    "scripts/target_symbol_map.json",  # positive control: known LIVE
    "unified_id.json",
    "unified_id_callgraph.json",
    "unified_id_rtti.json",
    "unified_id_rtti_low.json",
    "unified_id_vtable.json",
    "unified_id_rb3wii.json",
    "dc3_oracle.json",
    "global_fuzzy_pairs.json",
    "autoid.json",
]

_HEX8 = re.compile(r"^(?:0x)?[0-9A-Fa-f]{6,8}$")
_SYMRX = re.compile(
    r"^\S+\s*=\s*\.text:(0x[0-9A-Fa-f]+);.*?type:function")


def load_text_starts(path):
    """Set of every `.text` function START address in the CURRENT symbols.txt."""
    starts = set()
    with open(path) as fh:
        for line in fh:
            m = _SYMRX.match(line)
            if m:
                starts.add(int(m.group(1), 16))
    return starts


def extract_addrs(doc, field=None):
    """Pull the rb3-side addresses out of an index of unknown shape.

    Handles the two shapes in this repo: a list of dicts (the unified_id family)
    and a flat {"0xADDR": "name"} dict (target_symbol_map.json).
    """
    out, used = [], field
    if isinstance(doc, dict):
        # uid_merge.json wraps the real table under "entries" (and its keys are
        # BARE hex, no 0x prefix) — unwrap before giving up on this shape.
        if "entries" in doc and isinstance(doc["entries"], (dict, list)):
            doc = doc["entries"]
    if isinstance(doc, dict):
        keys = [k for k in doc if isinstance(k, str) and _HEX8.match(k)]
        if keys:
            return [int(k, 16) for k in keys], "<dict key>"
        doc = list(doc.values())
    if isinstance(doc, list) and doc and isinstance(doc[0], dict):
        if used is None:
            for cand in ADDR_FIELDS:
                if any(cand in e for e in doc[:64] if isinstance(e, dict)):
                    used = cand
                    break
        if used:
            for e in doc:
                if not isinstance(e, dict):
                    continue
                v = e.get(used)
                if isinstance(v, str) and v.startswith("0x"):
                    try:
                        out.append(int(v, 16))
                    except ValueError:
                        pass
                elif isinstance(v, int):
                    out.append(v)
        if not out:
            # Fall back to a `fn_<8hex>` symbol-name field.
            for cand in FN_FIELDS:
                for e in doc:
                    if not isinstance(e, dict):
                        continue
                    m = _FN_NAME.match(str(e.get(cand, "")))
                    if m:
                        out.append(int(m.group(1), 16))
                if out:
                    used = cand + " (fn_<VA>)"
                    break
    return out, used


def audit(addrs, starts, trials=15, seed=0):
    """(hit%, null%, ratio, best_shift_beats_true)."""
    n = len(addrs)
    hit = 100.0 * sum(1 for a in addrs if a in starts) / n
    rng = random.Random(seed)
    nulls = []
    for _ in range(trials):
        off = rng.randrange(0x100, 0x100000) & ~3
        nulls.append(100.0 * sum(1 for a in addrs if (a + off) in starts) / n)
    null = sum(nulls) / len(nulls)
    ratio = (hit / null) if null else float("inf")
    return hit, null, ratio, max(nulls) > hit


def verdict(hit, ratio):
    """hit% is the PRIMARY axis; the null ratio only corroborates.

    A genuinely live address index resolves at ~100% (target_symbol_map.json:
    99.79%). Anything resolving in the single digits is unusable as an address
    index no matter how the null lands — judging on `ratio` alone once let
    dc3_oracle.json (4.25% hit, ratio 1.59) read as "WEAK" when it is dead.
    """
    if hit >= 50 and ratio >= 4:
        return "LIVE"
    if hit >= 20 and ratio >= 2:
        return "PARTIAL"
    return "DEAD (== noise)"


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("files", nargs="*", help="index JSONs (default: known set)")
    ap.add_argument("--field", help="force the rb3-side address field name")
    ap.add_argument("--symbols", default=SYMBOLS, help="symbols.txt to audit against")
    ap.add_argument("--trials", type=int, default=15, help="null-model shift trials")
    args = ap.parse_args()

    if not os.path.exists(args.symbols):
        sys.exit(f"symbols.txt not found: {args.symbols} (run from the repo root)")
    starts = load_text_starts(args.symbols)
    print(f"Target universe: {len(starts)} `.text` function starts "
          f"from {os.path.relpath(args.symbols, ROOT)}\n")

    targets = args.files or [os.path.join(ROOT, f) for f in DEFAULT_TARGETS]
    print("%-34s %7s %7s %7s %7s  %s"
          % ("index", "n", "hit%", "null%", "ratio", "verdict"))
    print("-" * 84)
    dead = []
    for path in targets:
        rel = os.path.relpath(path, ROOT)
        if not os.path.exists(path):
            print("%-34s %s" % (rel, "(absent)"))
            continue
        try:
            doc = json.load(open(path))
        except Exception as exc:
            print("%-34s LOAD FAIL: %s" % (rel, exc))
            continue
        addrs, field = extract_addrs(doc, args.field)
        addrs = sorted(set(addrs))
        if len(addrs) < 20:
            print("%-34s %7d  (no usable address field — skipped)" % (rel, len(addrs)))
            continue
        hit, null, ratio, beaten = audit(addrs, starts, args.trials)
        v = verdict(hit, ratio)
        note = "  <- an arbitrary shift beats the true offset" if beaten else ""
        print("%-34s %7d %6.2f%% %6.2f%% %7.2f  %s%s"
              % (rel, len(addrs), hit, null, ratio, v, note))
        if v.startswith("DEAD"):
            dead.append(rel)

    if dead:
        print("\nDEAD indices (addresses carry no signal against this binary):")
        for d in dead:
            print("  - " + d)
        print("\nDo NOT feed these to any tool that emits an artifact "
              "(symbol maps, pins, ranked candidates) — they produce\n"
              "plausible-looking WRONG output rather than an error.")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
