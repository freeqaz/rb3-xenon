#!/usr/bin/env python3
"""CD-5 detector: default-vs-name_check per-function spread = wrong-callee-NAME signal.

WHY THIS WORKS
--------------
objdiff's default scoring runs functionRelocDiffs=none, which forgives the
relocation OPERAND entirely. Two sibling functions with identical opcodes but
different branch targets therefore score identically -- the row reads 100%
(or high) while pointing at the wrong retail body. `-c functionRelocDiffs=name_check`
additionally compares the relocation TARGET SYMBOL NAME, so the same row drops.

    delta = default.fuzzy_match_percent - namecheck.fuzzy_match_percent

delta > 0  <=>  at least one relocation in this function resolves to a
different NAME on the target side than on the base side.

MEASURED CONTROLS (lane CD-5, HEAD 9f9b687f, BeatMatcher mWatcher forwarder block)
---------------------------------------------------------------------------------
POSITIVE (known mispairs, proven by branch-target chain + rb3-Wii oracle):
    ?SetAutoplayError@BeatMatcher@@QAAXH@Z        100.00 -> 97.50   FLAGGED
    ?Restart@BeatMatcher@@QAAXXZ                  100.00 -> 97.50   FLAGGED
    ?CycleAutoplayAccuracy@BeatMatcher@@QAAMXZ    100.00 -> 97.50   FLAGGED
NEGATIVE (known-correct rows in the SAME block, same shape, same size):
    ?SetAutoplay@BeatMatcher@@QAAX_N@Z            100.00 -> 100.00  not flagged
    ?E3CheatIncSlop@BeatMatcher@@QAAXXZ           100.00 -> 100.00  not flagged
    ?E3CheatDecSlop@BeatMatcher@@QAAXXZ           100.00 -> 100.00  not flagged
=> 3/3 sensitivity, 0/3 false-positive on same-shape decoys.

NOT A VERDICT -- delta>0 is equally consistent with an ICF FOLD-ALIAS, where
retail folded our body into a sibling and the map names the sibling. ~71.5% of
sites are that class (project memory). Adjudicate by reading the retail body.
"""
import argparse, json, sys


def index(path):
    r = json.load(open(path))
    out = {}
    for u in r["units"]:
        for f in u.get("functions") or []:
            out[(u["name"], f["name"])] = (
                f.get("fuzzy_match_percent", 0.0),
                f.get("match_percent_normalized", 0.0),
                int(f.get("size", 0)),
            )
    return out, r["measures"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("default_json")
    ap.add_argument("namecheck_json")
    ap.add_argument("--min-pct", type=float, default=0.0)
    ap.add_argument("--max-pct", type=float, default=100.0)
    ap.add_argument("--max-size", type=int, default=10**9)
    ap.add_argument("--min-size", type=int, default=0)
    ap.add_argument("--exclude-anon", action="store_true",
                    help="drop default/auto_* units (anon gate: bodies can never pair)")
    ap.add_argument("--format", choices=["table", "json", "names"], default="table")
    a = ap.parse_args()

    D, _ = index(a.default_json)
    N, _ = index(a.namecheck_json)
    if set(D) != set(N):
        print("FATAL: key sets differ -- reports are from different builds", file=sys.stderr)
        sys.exit(2)

    rows = []
    for k in D:
        df, dn, sz = D[k]
        nf, nn, _ = N[k]
        delta = df - nf
        if delta <= 1e-9:
            continue
        if not (a.min_pct <= df <= a.max_pct):
            continue
        if not (a.min_size <= sz <= a.max_size):
            continue
        if a.exclude_anon and k[0].startswith("default/auto_"):
            continue
        rows.append({"unit": k[0], "symbol": k[1], "default": df,
                     "namecheck": nf, "delta": delta, "size": sz})
    rows.sort(key=lambda r: (-r["delta"], r["size"]))

    if a.format == "json":
        json.dump(rows, sys.stdout, indent=1)
    elif a.format == "names":
        for r in rows:
            print(r["symbol"])
    else:
        print(f"# {len(rows)} rows with a name_check drop "
              f"(default in [{a.min_pct},{a.max_pct}], size in [{a.min_size},{a.max_size}])")
        for r in rows:
            print(f'{r["default"]:8.4f} -> {r["namecheck"]:8.4f}  d={r["delta"]:7.4f} '
                  f'sz={r["size"]:6d}  {r["unit"][:34]:34s} {r["symbol"]}')


if __name__ == "__main__":
    main()
