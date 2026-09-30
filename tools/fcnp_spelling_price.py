#!/usr/bin/env python3
"""Price every FABRICATED_CLOSURE_NOT_PARTITION (FCNP) alias spelling by the bytes
its restoration would move on the whole binary.

Lane W16-GM (2026-09-30). Background: `docs/decomp/W16GK_UTL_BLOCK_ADJUDICATION_2026-09-16.md`
found that restoring ONE list<T>::insert spelling paid +4,576 B across 9 units
against a +912 B row-local prediction, because an alias membership pays at EVERY
caller of the spelling. This tool prices each withdrawn FCNP spelling over its whole
caller population BEFORE anyone spends adjudication budget on it.

Inputs
------
  * a JSONL dump of `objdiff-cli diff -p . --batch --include-instructions -f json`
    over every sub-100 paired row (the lane produced it with the symbol list
    derived from report.json; ~150 MB for 6,333 rows), and
  * scripts/symbol_aliases.json (groups, live folded lists, withdrawn records), and
  * build/45410914/report.json (sizes, masked_equal, the grader's fuzzy).

Definitions (these are the pricing rules, spelled out so they can be argued with)
------
A charged site is a `diff_arg` instruction argument whose `diff_breakdown` entry is
`arg_type == "symbol"` with typed `Symbol` values on BOTH sides. Its retail name is
the target side, ours is the base side. (⚠ `match_type`, not `diff_kind` -- the
latter does not exist in this JSON and a reader keyed on it finds zero differences.)

A site is ATTRIBUTABLE to (spelling S, group address A) iff our name == S, S is an
FCNP-withdrawn member of group A, and retail's name is a member of A (survivor or a
LIVE folded member). The retail-side name pins the address, which is the whole point:
the closure placed S in many groups, but a call site's retail name says which one.

A row is FULLY CLEARABLE by (S, A) iff every non-equal instruction is `diff_arg`
and every differing argument is a site attributable to that same (S, A). Its
`size` (from report.json) is the row's price, split by `masked_equal`.
A row where (S, A) sites coexist with any other charge is PARTIAL for (S, A).
A row whose charges are all FCNP-attributable but span >1 spelling is reported as
SET-CLEARABLE (the set is named) and counted PARTIAL for each member.

Sanity checks built in (the tool refuses if they fail):
  * `?PathCompare@@YA_NPAVDataArray@@0@Z` (register-order diff_arg only) must price
    to no spelling;
  * every row in the dump must be found in report.json with the same fuzzy.
"""
import argparse
import collections
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FCNP = "FABRICATED_CLOSURE_NOT_PARTITION"
PATHCOMPARE = "?PathCompare@@YA_NPAVDataArray@@0@Z"


def load_groups(path):
    d = json.load(open(path))
    members = {}      # addr -> set(survivor + live folded)
    fcnp = {}         # addr -> set(spelling withdrawn as FCNP)
    survivor = {}
    for g in d["groups"]:
        # 51 groups carry address=null (MakeString / _G dtor families); none holds an
        # FCNP record (measured 2026-09-30) -- key them by survivor so nothing is dropped.
        a = (g.get("address") or f"null:{g['survivor']}").lower()
        survivor[a] = g["survivor"]
        members[a] = {g["survivor"], *g.get("folded", [])}
        s = set()
        for e in g.get("withdrawn") or []:
            if isinstance(e, dict) and e.get("class") == FCNP:
                s.add(e["spelling"])
        if s:
            fcnp[a] = s
    return members, fcnp, survivor


def load_report(path):
    d = json.load(open(path))
    rows = {}
    for u in d["units"]:
        for f in u.get("functions", []):
            rows[f["name"]] = dict(
                unit=u["name"], size=int(f.get("size", 0)),
                fuzzy=float(f.get("fuzzy_match_percent", 0)),
                mpn=(None if f.get("match_percent_normalized") is None
                     else float(f["match_percent_normalized"])),
                masked_equal=bool(f.get("masked_equal", False)))
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dump", required=True, help="objdiff batch JSONL with instructions")
    ap.add_argument("--aliases", default=str(ROOT / "scripts/symbol_aliases.json"))
    ap.add_argument("--report", default=str(ROOT / "build/45410914/report.json"))
    ap.add_argument("--out-json", required=True)
    ap.add_argument("--out-md", required=True)
    ap.add_argument("--top", type=int, default=40)
    a = ap.parse_args()

    members, fcnp, survivor = load_groups(a.aliases)
    fcnp_spellings = collections.defaultdict(set)  # spelling -> addrs
    for addr, s in fcnp.items():
        for sp in s:
            fcnp_spellings[sp].add(addr)
    rows = load_report(a.report)

    # per (S, A): full rows, partial rows, sites
    full = collections.defaultdict(list)
    partial = collections.defaultdict(list)
    sites = collections.Counter()
    retail_names = collections.defaultdict(set)
    set_clearable = []
    census = collections.Counter()
    nonfcnp_pairs = collections.Counter()
    pathcompare_priced = False
    n = 0
    for line in open(a.dump):
        o = json.loads(line)
        if "error" in o:
            census["dump_error"] += 1
            continue
        n += 1
        r = rows.get(o["symbol"])
        if r is None:
            sys.exit(f"REFUSE: {o['symbol']} in dump but not in report.json")
        if abs(r["fuzzy"] - float(o["fuzzy_match_percent"])) > 0.01:
            sys.exit(f"REFUSE: fuzzy disagrees for {o['symbol']}: report {r['fuzzy']} dump {o['fuzzy_match_percent']}")
        attributable = collections.Counter()   # (S, A) -> sites in this row
        other = 0                              # non-attributable charges
        other_kinds = set()
        for ins in o["instructions"]:
            mt = ins["match_type"]
            if mt == "equal":
                continue
            if mt != "diff_arg":
                other += 1
                other_kinds.add(mt)
                continue
            for arg in ins["diff_breakdown"]["arguments"]:
                t = (arg.get("target") or {}).get("type")
                b = (arg.get("base") or {}).get("type")
                if arg.get("arg_type") != "symbol" or t != "Symbol" or b != "Symbol":
                    other += 1
                    other_kinds.add(arg.get("arg_type"))
                    continue
                retail = arg["target"]["value"]
                ours = arg["base"]["value"]
                addrs = fcnp_spellings.get(ours)
                hit = None
                if addrs:
                    for addr in addrs:
                        if retail in members[addr]:
                            hit = addr
                            break
                if hit is None:
                    other += 1
                    other_kinds.add("symbol_nonfcnp")
                    nonfcnp_pairs[(retail, ours)] += 1
                    continue
                attributable[(ours, hit)] += 1
                retail_names[(ours, hit)].add(retail)
        if not attributable:
            census["rows_no_fcnp_site"] += 1
            continue
        if o["symbol"] == PATHCOMPARE:
            pathcompare_priced = True
        for key, k in attributable.items():
            sites[key] += k
        row_rec = dict(symbol=o["symbol"], unit=r["unit"], size=r["size"], fuzzy=r["fuzzy"],
                       mpn=r["mpn"], masked_equal=r["masked_equal"])
        if other == 0 and len(attributable) == 1:
            key = next(iter(attributable))
            full[key].append(row_rec)
            census["rows_full_single"] += 1
        elif other == 0:
            set_clearable.append(dict(row_rec, spellings=sorted(f"{s}@{a_}" for s, a_ in attributable)))
            for key in attributable:
                partial[key].append(dict(row_rec, why="needs_other_fcnp_spellings"))
            census["rows_full_set"] += 1
        else:
            for key in attributable:
                partial[key].append(dict(row_rec, why="other_charges:" + ",".join(sorted(other_kinds))))
            census["rows_partial"] += 1

    if pathcompare_priced:
        sys.exit("REFUSE: PathCompare (register-order only) was priced to a spelling -- classifier broken")

    table = []
    for key in set(full) | set(partial):
        s, addr = key
        fr = full.get(key, [])
        pr = partial.get(key, [])
        rec = dict(
            spelling=s, address=addr, survivor=survivor[addr],
            retail_names_seen=sorted(retail_names[key]),
            sites=sites[key],
            full_rows=len(fr),
            full_bytes=sum(r["size"] for r in fr),
            full_bytes_honest=sum(r["size"] for r in fr if not r["masked_equal"]),
            full_bytes_masked=sum(r["size"] for r in fr if r["masked_equal"]),
            full_units=sorted({r["unit"] for r in fr}),
            partial_rows=len(pr),
            partial_bytes=sum(r["size"] for r in pr),
            rows_full=fr, rows_partial=pr,
            fcnp_groups_for_spelling=len(fcnp_spellings[s]),
        )
        table.append(rec)
    table.sort(key=lambda r: (-r["full_bytes"], -r["partial_bytes"], r["spelling"]))

    # multi-address spellings: same spelling attributable at >1 retail address
    by_sp = collections.defaultdict(set)
    for r in table:
        by_sp[r["spelling"]].add(r["address"])
    multi = {s: sorted(v) for s, v in by_sp.items() if len(v) > 1}

    total_full = sum(r["full_bytes"] for r in table)
    total_full_honest = sum(r["full_bytes_honest"] for r in table)
    total_full_rows = sum(r["full_rows"] for r in table)
    top10 = sum(r["full_bytes"] for r in table[:10])
    top50 = sum(r["full_bytes"] for r in table[:50])
    priced_spellings = len({r["spelling"] for r in table if r["full_bytes"] > 0})

    summary = dict(
        rows_in_dump=n, census=dict(census),
        fcnp_spellings=len(fcnp_spellings), fcnp_groups=len(fcnp),
        fcnp_memberships=sum(len(v) for v in fcnp.values()),
        spelling_address_pairs_with_any_site=len(table),
        spellings_with_full_price=priced_spellings,
        total_full_bytes=total_full, total_full_bytes_honest=total_full_honest,
        total_full_rows=total_full_rows,
        top10_full_bytes=top10, top50_full_bytes=top50,
        top10_share=(top10 / total_full if total_full else 0),
        top50_share=(top50 / total_full if total_full else 0),
        set_clearable_rows=len(set_clearable),
        set_clearable_bytes=sum(r["size"] for r in set_clearable),
        multi_address_spellings=multi,
        nonfcnp_symbol_pairs_top=[[k[0], k[1], v] for k, v in nonfcnp_pairs.most_common(40)],
    )
    json.dump(dict(summary=summary, table=table, set_clearable=set_clearable),
              open(a.out_json, "w"), indent=1)

    with open(a.out_md, "w") as f:
        f.write("# FCNP spelling prices (full table)\n\n")
        f.write("```\n" + json.dumps({k: v for k, v in summary.items()
                                       if k not in ("nonfcnp_symbol_pairs_top", "multi_address_spellings")}, indent=1) + "\n```\n\n")
        f.write("| # | full B | honest B | masked B | full rows | partial rows | partial B | sites | units | address | our spelling | retail survivor |\n")
        f.write("|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|---|\n")
        for i, r in enumerate(table, 1):
            f.write(f"| {i} | {r['full_bytes']} | {r['full_bytes_honest']} | {r['full_bytes_masked']} | {r['full_rows']} | "
                    f"{r['partial_rows']} | {r['partial_bytes']} | {r['sites']} | {len(r['full_units'])} | {r['address']} | "
                    f"`{r['spelling']}` | `{r['survivor']}` |\n")
        f.write("\n## Set-clearable rows (all charges FCNP, >1 spelling)\n\n")
        for r in set_clearable:
            f.write(f"- {r['size']} B `{r['symbol']}` ({r['unit']}) needs {r['spellings']}\n")
    print(json.dumps({k: v for k, v in summary.items() if k != "nonfcnp_symbol_pairs_top"}, indent=1))
    print("TOP", a.top)
    for i, r in enumerate(table[:a.top], 1):
        print(f"{i:3d} full={r['full_bytes']:6d} B ({r['full_rows']} rows, honest {r['full_bytes_honest']}) "
              f"partial={r['partial_bytes']:6d} B ({r['partial_rows']}) sites={r['sites']} units={len(r['full_units'])} "
              f"{r['address']} {r['spelling'][:90]}")


if __name__ == "__main__":
    main()
