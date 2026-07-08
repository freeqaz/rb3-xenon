#!/usr/bin/env python3
"""gap_atlas.py — the definitive 5-bucket breakdown of the unmatched binary.

Regenerates the "gap composition atlas" (docs/plans/paths-to-100/
02-gap-composition-atlas.md) from build/45410914/report.json on demand, so the
atlas can be refreshed after every landing wave and diffed against the frozen
baseline snapshot.

Every unmatched byte falls into exactly one of five buckets:

  1. Pinned real-body near/mid-miss (>44 B)   -> oracle-exists / portable-source
  2. Pinned boilerplate (funclets + <=44 B)   -> boilerplate
  3. Unpinned auto_ .text                       -> mixed (3a named / 3b anonymous)
  4. Bink / middleware sections                 -> opaque (no source oracle)
  5. Data sections (.rdata/.data/.idata)        -> anchoring signal (own denom)

Buckets 1+2 live inside PINNED units; buckets 3-5 are the auto/data remainder.
Consistency invariant: b1_bytes + b2_bytes + b3_bytes == unmatched_code
(the whole-binary unmatched code figure).

Usage:
    tools/gap_atlas.py                       # human-readable table (default report)
    tools/gap_atlas.py --report PATH         # use a different report.json
    tools/gap_atlas.py --json                # machine-readable JSON dump
    tools/gap_atlas.py --markdown            # markdown table (for snapshot docs)

The RFC (02-gap-composition-atlas.md, "Current state" section) is the ground
truth this reproduces; --check compares against those frozen a1312de numbers and
exits non-zero on any mismatch (the RFC's own kill criterion).
"""

import argparse
import json
import os
import sys

# Boilerplate threshold: functions <=44 B are tiny getters/setters/stubs.
BOILERPLATE_MAX_BYTES = 44

# Frozen RFC baseline (build/45410914/report.json @ main a1312de, 2026-07-08).
# tools/gap_atlas.py --check compares against these; a mismatch is the RFC's
# kill criterion ("if the aggregation logic here is wrong").
RFC_BASELINE = {
    "total_code": 11074108,
    "matched_code": 962656,
    "unmatched_code": 10111452,
    "pinned_units": 773,
    "pinned_code": 3132020,
    "pinned_matched": 962656,
    "auto_units": 1683,
    "auto_code": 7942088,
    "bucket1_bytes": 1925408,
    "bucket1_fns": 7116,
    "bucket2_bytes": 243956,
    "bucket2_fns": 5191,
    "bucket3_named_bytes": 2627100,
    "bucket3_named_fns": 7086,
    "bucket3_anon_bytes": 5314988,
    "bucket3_anon_fns": 35039,
    "bucket4_bink_text": 65292,
    "total_data": 4118360,
}


def default_report_path():
    here = os.path.dirname(os.path.abspath(__file__))
    return os.path.join(here, "..", "build", "45410914", "report.json")


def is_auto(unit):
    """A unit is an auto_ span (no source wired) if flagged auto_generated or
    its name starts with auto_ (matches the RFC's aggregation predicate)."""
    return bool(unit.get("metadata", {}).get("auto_generated")) or unit["name"].startswith(
        "auto_"
    )


def imeasure(unit, key):
    return int(unit["measures"].get(key, "0") or 0)


def fsize(fn):
    return int(fn.get("size", 0) or 0)


def compute_atlas(report):
    m = report["measures"]
    units = report["units"]

    total_code = int(m["total_code"])
    matched_code = int(m["matched_code"])
    unmatched_code = total_code - matched_code

    auto = [u for u in units if is_auto(u)]
    pinned = [u for u in units if not is_auto(u)]

    def tot(us, k):
        return sum(imeasure(u, k) for u in us)

    pinned_code = tot(pinned, "total_code")
    pinned_matched = tot(pinned, "matched_code")
    auto_code = tot(auto, "total_code")

    # --- Buckets 1 & 2: inside pinned units, split by boilerplate class ---
    # A function counts as unmatched when its fuzzy_match_percent != 100.
    b1_bytes = b1_fns = 0          # real body, >44 B, non-funclet
    b2_funclet_bytes = b2_funclet_fns = 0  # funclet/unwind ($ in name)
    b2_small_bytes = b2_small_fns = 0      # small <=44 B non-funclet stubs
    for u in pinned:
        for f in u["functions"]:
            if f.get("fuzzy_match_percent") == 100.0:
                continue
            sz = fsize(f)
            if "$" in f["name"]:
                b2_funclet_fns += 1
                b2_funclet_bytes += sz
            elif sz <= BOILERPLATE_MAX_BYTES:
                b2_small_fns += 1
                b2_small_bytes += sz
            else:
                b1_fns += 1
                b1_bytes += sz
    b2_bytes = b2_funclet_bytes + b2_small_bytes
    b2_fns = b2_funclet_fns + b2_small_fns

    # --- Bucket 3: unpinned auto_ .text, split named vs anonymous fn_ ---
    autotext = [u for u in auto if any(s["name"] == ".text" for s in u.get("sections", []))]
    b3_named_bytes = b3_named_fns = 0
    b3_anon_bytes = b3_anon_fns = 0
    for u in autotext:
        for f in u.get("functions", []):
            sz = fsize(f)
            if f["name"].startswith("fn_"):
                b3_anon_fns += 1
                b3_anon_bytes += sz
            else:
                b3_named_fns += 1
                b3_named_bytes += sz
    b3_bytes = b3_named_bytes + b3_anon_bytes

    # --- Bucket 4: Bink / middleware. Code metric is the unit's total_code
    # (== sum of its function sizes), NOT the raw PE section size. The BINK unit
    # also carries a data-flavoured BINK section; only its .text code counts here.
    bink_text = 0
    mw_data_sections = []
    for u in auto:
        name_up = u["name"].upper()
        if any(tag in name_up for tag in ("BINK", "XBMOVIE")):
            tc = imeasure(u, "total_code")
            bink_text += tc
            for s in u.get("sections", []):
                sn = s.get("name", "")
                if sn != ".text" and sn.upper() not in ("BINK",):
                    mw_data_sections.append((u["name"], sn, int(s.get("size", 0) or 0)))

    # --- Bucket 5: data sections (own denominator, not in the code figure) ---
    total_data = int(m["total_data"])
    matched_data = int(m["matched_data"])
    data_sections = {}
    for u in auto:
        for s in u.get("sections", []):
            sn = s.get("name", "")
            if sn in (".rdata", ".data", ".idata"):
                data_sections[sn] = data_sections.get(sn, 0) + int(s.get("size", 0) or 0)

    return {
        "topline": {
            "total_code": total_code,
            "matched_code": matched_code,
            "unmatched_code": unmatched_code,
            "matched_code_percent": 100.0 * matched_code / total_code,
            "total_functions": int(m["total_functions"]),
            "matched_functions": int(m["matched_functions"]),
            "matched_functions_percent": 100.0 * int(m["matched_functions"]) / int(m["total_functions"]),
            "total_data": total_data,
            "matched_data": matched_data,
            "total_units": int(m["total_units"]),
        },
        "pinned": {
            "units": len(pinned),
            "code": pinned_code,
            "matched": pinned_matched,
            "matched_percent": 100.0 * pinned_matched / pinned_code if pinned_code else 0.0,
            "fns": tot(pinned, "total_functions"),
            "matched_fns": tot(pinned, "matched_functions"),
        },
        "auto": {
            "units": len(auto),
            "code": auto_code,
            "matched": tot(auto, "matched_code"),
            "fns": tot(auto, "total_functions"),
            "matched_fns": tot(auto, "matched_functions"),
        },
        "bucket1": {"bytes": b1_bytes, "fns": b1_fns},
        "bucket2": {
            "bytes": b2_bytes,
            "fns": b2_fns,
            "funclet_bytes": b2_funclet_bytes,
            "funclet_fns": b2_funclet_fns,
            "small_bytes": b2_small_bytes,
            "small_fns": b2_small_fns,
        },
        "bucket3": {
            "autotext_units": len(autotext),
            "named_bytes": b3_named_bytes,
            "named_fns": b3_named_fns,
            "anon_bytes": b3_anon_bytes,
            "anon_fns": b3_anon_fns,
            "bytes": b3_bytes,
        },
        "bucket4": {"bink_text": bink_text, "mw_data_sections": mw_data_sections},
        "bucket5": {
            "total_data": total_data,
            "matched_data": matched_data,
            "sections": data_sections,
        },
        "consistency": {
            "b1_b2_sum": b1_bytes + b2_bytes,
            "pinned_unmatched": b1_bytes + b2_bytes,
            "b1_b2_b3_sum": b1_bytes + b2_bytes + b3_bytes,
            "unmatched_code": unmatched_code,
            "reconciled": (b1_bytes + b2_bytes + b3_bytes) == unmatched_code,
        },
    }


def pct(part, whole):
    return (100.0 * part / whole) if whole else 0.0


def render_text(a):
    t = a["topline"]
    unm = t["unmatched_code"]
    out = []
    out.append("=== Gap composition atlas ===")
    out.append("")
    out.append("-- Whole-binary top line --")
    out.append(
        f"  STRICT functions  {t['matched_functions']:>7,} / {t['total_functions']:>7,}"
        f"  ({t['matched_functions_percent']:.2f}%)"
    )
    out.append(
        f"  STRICT code       {t['matched_code']:>9,} / {t['total_code']:>9,} B"
        f"  ({t['matched_code_percent']:.2f}%)"
    )
    out.append(f"  => UNMATCHED code = {unm:,} B ({pct(unm, t['total_code']):.2f}%)")
    out.append(f"  total_data {t['total_data']:,} B (matched {t['matched_data']} B) ; total_units {t['total_units']:,}")
    out.append("")
    p, au = a["pinned"], a["auto"]
    out.append("-- Pinned / auto split --")
    out.append(f"  {'Class':<7} {'units':>6} {'total_code (B)':>15} {'matched (B)':>13} {'matched%':>9} {'fns':>7} {'mfns':>7}")
    out.append(
        f"  {'PINNED':<7} {p['units']:>6} {p['code']:>15,} {p['matched']:>13,}"
        f" {p['matched_percent']:>8.2f}% {p['fns']:>7,} {p['matched_fns']:>7,}"
    )
    out.append(
        f"  {'AUTO':<7} {au['units']:>6} {au['code']:>15,} {au['matched']:>13,}"
        f" {'0.00%':>9} {au['fns']:>7,} {au['matched_fns']:>7,}"
    )
    out.append("")
    b1, b2, b3, b4, b5 = a["bucket1"], a["bucket2"], a["bucket3"], a["bucket4"], a["bucket5"]
    out.append("-- The five buckets --")
    out.append(f"  {'#':<2} {'Bucket':<40} {'Bytes':>12} {'% unmatched':>12}  Class")
    out.append(
        f"  {'1':<2} {'Pinned real-body near/mid-miss (>44 B)':<40} {b1['bytes']:>12,}"
        f" {pct(b1['bytes'], unm):>11.1f}%  oracle/portable-source"
    )
    out.append(
        f"  {'2':<2} {'Pinned boilerplate (funclets + <=44 B)':<40} {b2['bytes']:>12,}"
        f" {pct(b2['bytes'], unm):>11.1f}%  boilerplate"
    )
    out.append(
        f"  {'3':<2} {'Unpinned auto_ .text':<40} {b3['bytes']:>12,}"
        f" {pct(b3['bytes'], unm):>11.1f}%  mixed (3a/3b)"
    )
    out.append(
        f"  {'4':<2} {'Bink / middleware .text':<40} {b4['bink_text']:>12,}"
        f" {pct(b4['bink_text'], unm):>11.1f}%  opaque (no source)"
    )
    out.append(
        f"  {'5':<2} {'Data sections (own denom)':<40} {b5['total_data']:>12,}"
        f" {'n/a':>12}  anchoring signal"
    )
    out.append("")
    out.append("  Bucket 2 breakdown:")
    out.append(f"    funclet/unwind ($ in name): {b2['funclet_fns']:>5,} fns  {b2['funclet_bytes']:>9,} B")
    out.append(f"    small <=44 B (non-funclet): {b2['small_fns']:>5,} fns  {b2['small_bytes']:>9,} B")
    out.append("")
    out.append(f"  Bucket 1 fns: {b1['fns']:,}")
    out.append("  Bucket 3 breakdown (auto .text, {} units):".format(b3["autotext_units"]))
    out.append(f"    named  (identified, unpinned): {b3['named_fns']:>6,} fns  {b3['named_bytes']:>9,} B")
    out.append(f"    anon   (fn_<addr>, unlocated): {b3['anon_fns']:>6,} fns  {b3['anon_bytes']:>9,} B")
    out.append("")
    out.append("  Bucket 5 data sections:")
    for sn, sz in sorted(b5["sections"].items()):
        out.append(f"    {sn:<8} {sz:>12,} B")
    out.append("")
    c = a["consistency"]
    out.append("-- Consistency invariant --")
    out.append(f"  bucket1 + bucket2         = {c['b1_b2_sum']:,}  (== pinned unmatched)")
    out.append(f"  bucket1 + bucket2 + bucket3 = {c['b1_b2_b3_sum']:,}")
    out.append(f"  whole-binary unmatched code = {c['unmatched_code']:,}")
    out.append(f"  reconciled: {'YES' if c['reconciled'] else 'NO -- MISMATCH'}")
    return "\n".join(out)


def render_markdown(a):
    t = a["topline"]
    unm = t["unmatched_code"]
    p, au = a["pinned"], a["auto"]
    b1, b2, b3, b4, b5 = a["bucket1"], a["bucket2"], a["bucket3"], a["bucket4"], a["bucket5"]
    L = []
    L.append("### Whole-binary top line\n")
    L.append("```")
    L.append(f"STRICT functions  {t['matched_functions']:,} / {t['total_functions']:,}  ({t['matched_functions_percent']:.2f}%)")
    L.append(f"STRICT code       {t['matched_code']:,} / {t['total_code']:,} B  ({t['matched_code_percent']:.2f}%)")
    L.append(f"  => UNMATCHED code = {unm:,} B  ({pct(unm, t['total_code']):.2f}%)")
    L.append(f"total_data {t['total_data']:,} B (matched {t['matched_data']} B)")
    L.append(f"total_units {t['total_units']:,}")
    L.append("```\n")
    L.append("### Pinned / auto split\n")
    L.append("| Class  | units | total_code (B) | matched (B) | matched% | total fns | matched fns |")
    L.append("|--------|------:|---------------:|------------:|---------:|----------:|------------:|")
    L.append(f"| PINNED | {p['units']:>4} | {p['code']:>14,} | {p['matched']:>11,} | **{p['matched_percent']:.2f}%** | {p['fns']:>9,} | {p['matched_fns']:>11,} |")
    L.append(f"| AUTO   | {au['units']:>4} | {au['code']:>14,} | {au['matched']:>11,} | 0.00% | {au['fns']:>9,} | {au['matched_fns']:>11,} |")
    L.append("")
    L.append("### The five buckets\n")
    L.append("| # | Bucket | Bytes | % of unmatched | Matchability class |")
    L.append("|---|--------|------:|---------------:|--------------------|")
    L.append(f"| 1 | Pinned real-body near/mid-miss (>44 B) | {b1['bytes']:,} | {pct(b1['bytes'], unm):.1f}% | oracle-exists / portable-source |")
    L.append(f"| 2 | Pinned boilerplate (funclets + <=44 B stubs) | {b2['bytes']:,} | {pct(b2['bytes'], unm):.1f}% | boilerplate |")
    L.append(f"| 3 | Unpinned auto_ .text (game+engine+mw+CRT) | {b3['bytes']:,} | {pct(b3['bytes'], unm):.1f}% | mixed — see 3a/3b |")
    L.append(f"| 4 | Bink / middleware .text | {b4['bink_text']:,} | {pct(b4['bink_text'], unm):.2f}% | opaque (no source) |")
    L.append(f"| 5 | Data sections (.rdata/.data/.idata) | {b5['total_data']:,} | n/a (own denom) | anchoring signal |")
    L.append("")
    L.append("Bucket 2 breakdown:\n")
    L.append("```")
    L.append(f"funclet/unwind ($ in name):  {b2['funclet_fns']:>5,} fns   {b2['funclet_bytes']:>9,} B")
    L.append(f"small <=44 B (non-funclet):  {b2['small_fns']:>5,} fns   {b2['small_bytes']:>9,} B")
    L.append(f"                             {b2['fns']:>5,} fns   {b2['bytes']:>9,} B")
    L.append("```\n")
    L.append("Bucket 3 breakdown (auto .text):\n")
    L.append("```")
    L.append(f"named symbols (reachable, uncredited):  {b3['named_fns']:>6,} fns  {b3['named_bytes']:>9,} B")
    L.append(f"anonymous fn_<addr> (never identified): {b3['anon_fns']:>6,} fns  {b3['anon_bytes']:>9,} B")
    L.append("```\n")
    c = a["consistency"]
    L.append("### Consistency invariant\n")
    L.append("```")
    L.append(f"bucket1 + bucket2           = {c['b1_b2_sum']:,}  (== pinned unmatched)")
    L.append(f"bucket1 + bucket2 + bucket3 = {c['b1_b2_b3_sum']:,}")
    L.append(f"whole-binary unmatched code = {c['unmatched_code']:,}")
    L.append(f"reconciled: {'YES' if c['reconciled'] else 'NO — MISMATCH'}")
    L.append("```")
    return "\n".join(L)


def run_check(a):
    """Compare computed atlas against the frozen RFC baseline; return (ok, lines)."""
    got = {
        "total_code": a["topline"]["total_code"],
        "matched_code": a["topline"]["matched_code"],
        "unmatched_code": a["topline"]["unmatched_code"],
        "pinned_units": a["pinned"]["units"],
        "pinned_code": a["pinned"]["code"],
        "pinned_matched": a["pinned"]["matched"],
        "auto_units": a["auto"]["units"],
        "auto_code": a["auto"]["code"],
        "bucket1_bytes": a["bucket1"]["bytes"],
        "bucket1_fns": a["bucket1"]["fns"],
        "bucket2_bytes": a["bucket2"]["bytes"],
        "bucket2_fns": a["bucket2"]["fns"],
        "bucket3_named_bytes": a["bucket3"]["named_bytes"],
        "bucket3_named_fns": a["bucket3"]["named_fns"],
        "bucket3_anon_bytes": a["bucket3"]["anon_bytes"],
        "bucket3_anon_fns": a["bucket3"]["anon_fns"],
        "bucket4_bink_text": a["bucket4"]["bink_text"],
        "total_data": a["topline"]["total_data"],
    }
    lines = []
    ok = True
    for k, want in RFC_BASELINE.items():
        have = got[k]
        mark = "OK " if have == want else "!! "
        if have != want:
            ok = False
        lines.append(f"  {mark}{k:<22} got {have:>12,}  rfc {want:>12,}")
    return ok, lines


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--report", default=None, help="path to report.json (default: build/45410914/report.json)")
    ap.add_argument("--json", action="store_true", help="emit machine-readable JSON")
    ap.add_argument("--markdown", action="store_true", help="emit markdown table (for snapshot docs)")
    ap.add_argument("--check", action="store_true", help="verify against frozen RFC baseline; exit 1 on mismatch")
    args = ap.parse_args()

    report_path = args.report or default_report_path()
    if not os.path.exists(report_path):
        sys.exit(f"report.json not found: {report_path}")
    with open(report_path) as f:
        report = json.load(f)

    atlas = compute_atlas(report)

    if args.check:
        ok, lines = run_check(atlas)
        print("=== gap_atlas --check vs frozen RFC baseline (a1312de) ===")
        print("\n".join(lines))
        print("RESULT:", "REPRODUCED" if ok else "MISMATCH")
        sys.exit(0 if ok else 1)

    if args.json:
        print(json.dumps(atlas, indent=2))
    elif args.markdown:
        print(render_markdown(atlas))
    else:
        print(render_text(atlas))


if __name__ == "__main__":
    main()
