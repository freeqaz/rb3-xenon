#!/usr/bin/env python3
"""fuzzy_progress.py — tiered FUZZY progress reporter for the rb3-xenon decomp.

Operationalizes the rank-1 recommendation of
`docs/decomp/fuzzy-reconstruction-frontier-2026-06-21.md` (§4 THE FUZZY REFRAME).

THE POINT (project owner): byte-exact-for-everything is unrealistic for
RB3-specific (cross-compiler MWCC->MSVC) game code. A reconstruction that takes a
function 9% -> 85% currently registers as ZERO because the headline only counts
`match_percent_normalized == 100`. So FUZZY reconstruction must COUNT as
measurable progress instead of reading 0-until-perfect. This tool surfaces that
partial progress WITHOUT letting it dilute the strict north star.

  STRICT      matched_functions / total_functions  +  matched_code / total_code
              read straight from `measures` (objdiff's authoritative count).
              THE IMMUTABLE NORTH STAR. Never softened.

  FUZZY-CODE  size-weighted mean per-fn match% over (a) the WHOLE binary and
              (b) the WIRED denominator. Size-weighted so a 2 KB fn @99% is worth
              far more than a 40 B stub @99%. The WIRED figure is the honest
              "how close is the attempted set to perfect" — immune to the ~75%
              not-yet-attempted bulk dragging it down. This is the PRIMARY fuzzy
              GOAL of the reframe (~95% wired vs ~73% strict-code-coverage).

  STAIRCASE   completion staircase: count of fns at >=100 / >=95 / >=90 / >=80
              / >=50. Climbs before STRICT does, so it shows a wave landed work
              even when nothing crossed 100 yet.

  HISTOGRAM   the per-band distribution from the report (100, [95,100), [90,95),
              [80,90), [50,80), (0,50), 0).

  SUB-GOALS   RB3-specific (band3/network) wired completion reported separately
              from engine (src/system) — so engine noise never masks the
              hard-frontier campaign, and the bar is NOT relaxed on engine code
              where DC3 makes byte-exact achievable.

"WIRED" is defined precisely (see is_wired): a function is wired iff its report
entry carries a `fuzzy_match_percent` field. objdiff only emits that field for
functions in a unit that has a pinned .text range AND a compiled obj that
objdiff actually diffed — i.e. an *attempted* function. The ~53.8k functions
reading a bare `match_percent_normalized: 0.0` with no fuzzy field are
"not-yet-attempted" (no pin + compiled obj), not "attempted and failed". Wiring
junk at low % therefore cannot inflate the wired denominator.

ANTI-GAMING (the headline keeps STRICT and FUZZY visibly separate):
  * STRICT (whole-binary matched_functions) stays the immutable north star;
    FUZZY is a secondary progress/diagnostic signal, never the success bar.
  * FUZZY-CODE is size-weighted: a fn at p% contributes only p% * size, never a
    full point. A 40 B stub at 99% moves the needle ~nothing.
  * The primary fuzzy GOAL uses the WIRED denominator, so adding unwired
    no-oracle bytes at low % cannot pad it.
  * FUZZY CAN STILL BE INFLATED by ICF stub-folds: tiny (<=44 B) thunks /
    getters / guard-stubs / STL accessors that ICF-fold byte-identically across
    unrelated TUs read ~100% in a pin without that pin OWNING the code. This
    tool does NOT re-implement that detection — cross-reference and gate any
    *claimed* fuzzy gain through `tools/icf_alias_check.py` (commit 23bb6ee),
    which is the required own-vs-foreign honesty audit. The `--min-size` flag
    here lets you preview the staircase with stub-sized fns excluded as a quick
    sanity check, but it is NOT a substitute for icf_alias_check.

Per-function match metric: this tool ranks by `match_percent_normalized` (the
canonical reloc-normalized score the report doc's bands use), falling back to
`fuzzy_match_percent` when normalized is absent. STRICT comes straight from
`measures` so it always equals objdiff's own count regardless of band edges.

Usage:
  tools/fuzzy_progress.py                          # headline for current build
  tools/fuzzy_progress.py --report other/report.json
  tools/fuzzy_progress.py --by-unit 25             # + top units by fuzzy headroom
  tools/fuzzy_progress.py --baseline saved.json    # + diff vs a saved report
  tools/fuzzy_progress.py --min-size 48            # staircase preview, stubs excluded
  tools/fuzzy_progress.py --json                   # machine-readable summary
"""
import argparse
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT = os.path.join(ROOT, "build", "45410914", "report.json")

# Completion-staircase thresholds and per-band edges (mirror the report doc §4.1).
STAIRCASE = (100, 95, 90, 80, 50)
BANDS = (
    ("==100",    lambda v: v >= 100.0),
    ("[95,100)", lambda v: 95.0 <= v < 100.0),
    ("[90,95)",  lambda v: 90.0 <= v < 95.0),
    ("[80,90)",  lambda v: 80.0 <= v < 90.0),
    ("[50,80)",  lambda v: 50.0 <= v < 80.0),
    ("(0,50)",   lambda v: 0.0 < v < 50.0),
    ("==0",      lambda v: v <= 0.0),
)


def _i(x):
    """report.json stores some numbers as strings — coerce to int."""
    try:
        return int(x)
    except (TypeError, ValueError):
        return 0


def _f(x):
    try:
        return float(x)
    except (TypeError, ValueError):
        return 0.0


def fn_pct(f):
    """Canonical per-fn match%: normalized score, fall back to fuzzy."""
    if "match_percent_normalized" in f:
        return _f(f.get("match_percent_normalized"))
    return _f(f.get("fuzzy_match_percent"))


def is_wired(f):
    """A function is WIRED iff objdiff emitted a fuzzy_match_percent for it —
    i.e. its unit was pinned + compiled + actually diffed (an *attempted* fn).
    Unwired fns carry only a bare match_percent_normalized:0.0 with no fuzzy
    field (no pin + obj). This is the honest 'attempted' denominator."""
    return "fuzzy_match_percent" in f


def unit_tier(u):
    """Classify a unit into 'rb3' (RB3-specific game code: band3/network),
    'engine' (src/system Milo), or 'other'. Used for the sub-goals so engine
    noise never masks the RB3-specific hard-frontier campaign."""
    md = u.get("metadata", {}) or {}
    sp = md.get("source_path") or ""
    cats = md.get("progress_categories") or []
    if sp.startswith("src/band3") or sp.startswith("src/network") \
            or "game" in cats or "network" in cats:
        return "rb3"
    if sp.startswith("src/system") or "engine" in cats:
        return "engine"
    return "other"


def _new_acc():
    return {
        "fns": 0, "wired": 0,
        "code": 0, "code_fuzzy": 0.0,
        "wired_code": 0, "wired_code_fuzzy": 0.0,
        "stair": {t: 0 for t in STAIRCASE},
        "bands": {name: 0 for name, _ in BANDS},
    }


def _add(acc, pct, sz, wired, min_size):
    acc["fns"] += 1
    acc["code"] += sz
    acc["code_fuzzy"] += sz * pct / 100.0
    if wired:
        acc["wired"] += 1
        acc["wired_code"] += sz
        acc["wired_code_fuzzy"] += sz * pct / 100.0
    # staircase honors --min-size: a fn below the size floor never counts toward
    # a >=N rung (the ICF stub-fold preview).
    if sz >= min_size:
        for t in STAIRCASE:
            if pct >= t:
                acc["stair"][t] += 1
    for name, pred in BANDS:
        if pred(pct):
            acc["bands"][name] += 1
            break


def summarize(path, min_size=0):
    d = json.load(open(path))
    measures = d.get("measures", {})
    whole = _new_acc()
    tiers = {"rb3": _new_acc(), "engine": _new_acc(), "other": _new_acc()}
    per_unit = {}  # name -> (fuzzy_headroom_bytes, partial_fn_count)
    for u in d.get("units", []):
        tier = unit_tier(u)
        hr = 0.0
        nc = 0
        for f in u.get("functions", []):
            pct = fn_pct(f)
            sz = _i(f.get("size", 0))
            wired = is_wired(f)
            _add(whole, pct, sz, wired, min_size)
            _add(tiers[tier], pct, sz, wired, min_size)
            if 0.0 < pct < 100.0:
                hr += sz * pct / 100.0
                nc += 1
        if hr:
            per_unit[u.get("name", "?")] = (hr, nc)
    return {
        "path": path,
        "measures": measures,
        "whole": whole,
        "tiers": tiers,
        "per_unit": per_unit,
    }


def _pct(num, den):
    return (100.0 * num / den) if den else 0.0


def _fuzzy_code_pct(acc, wired=False):
    if wired:
        return _pct(acc["wired_code_fuzzy"], acc["wired_code"])
    return _pct(acc["code_fuzzy"], acc["code"])


def print_headline(s, min_size):
    m = s["measures"]
    whole = s["whole"]
    mf = _i(m.get("matched_functions"))
    tf = _i(m.get("total_functions"))
    mc = _i(m.get("matched_code"))
    tc = _i(m.get("total_code"))
    stair = whole["stair"]

    print(f"=== fuzzy progress :: {s['path']} ===")
    if min_size:
        print(f"    (staircase excludes fns < {min_size} B — ICF stub-fold preview)")
    print()
    print("STRICT  [immutable north star — objdiff measures]")
    print(f"   functions  {mf:7d} / {tf:<7d}  ({_pct(mf, tf):6.3f}%)")
    print(f"   code       {mc:9d} / {tc:<9d}  ({_pct(mc, tc):6.3f}%)")
    print()
    print("FUZZY-CODE  [size-weighted mean per-fn match% — progress signal, NOT the bar]")
    print(f"   whole binary   {_fuzzy_code_pct(whole):7.3f}%   "
          f"(over {whole['code']:,} code bytes)")
    print(f"   WIRED set      {_fuzzy_code_pct(whole, wired=True):7.3f}%   "
          f"(over {whole['wired_code']:,} attempted bytes, n={whole['wired']} fns)  <-- primary fuzzy GOAL")
    # fuzzy-credit ledger: bytes strict discards that fuzzy credits.
    fuzzy_bytes = whole["code_fuzzy"]
    credit = fuzzy_bytes - mc
    print(f"   credit ledger  strict {mc:,} B  ->  fuzzy ~{fuzzy_bytes:,.0f} B  "
          f"(+{credit:,.0f} fuzzy-credit bytes strict discards)")
    print()
    print("STAIRCASE  [completion staircase — climbs before STRICT does]")
    print("   " + " | ".join(f">={t}: {stair[t]}" for t in STAIRCASE))
    print()
    print("HISTOGRAM  [per-band fn counts, by match_percent_normalized]")
    for name, _ in BANDS:
        print(f"   {name:9s} {whole['bands'][name]:7d}")
    print()
    print("SUB-GOALS  [tiered so engine noise never masks the RB3-specific frontier]")
    _print_tier("RB3-specific (band3/network)", s["tiers"]["rb3"])
    _print_tier("engine (src/system) — DC3 oracle, byte-exact still expected", s["tiers"]["engine"])
    _print_tier("other (thirdparty/vendor/xdk/...)", s["tiers"]["other"])


def _print_tier(label, acc):
    st = acc["stair"]
    print(f"   {label}")
    print(f"      fns {acc['fns']:6d}  wired {acc['wired']:6d}  "
          f">=100 {st[100]:6d}  >=95 {st[95]:6d}  >=90 {st[90]:6d}")
    print(f"      fuzzy-code: whole {_fuzzy_code_pct(acc):6.2f}%   "
          f"wired {_fuzzy_code_pct(acc, wired=True):6.2f}%")


def print_by_unit(s, n):
    print(f"\n=== top {n} units by fuzzy headroom (invested-but-unfinished bytes) ===")
    ranked = sorted(s["per_unit"].items(), key=lambda kv: -kv[1][0])
    for name, (hr, nc) in ranked[:n]:
        short = name.split("/")[-1]
        print(f"   {short[:48]:48s}  {hr:9.0f} B  {nc:4d} partial fns")


def print_baseline(s, base_path, min_size):
    b = summarize(base_path, min_size)
    print(f"\n=== delta vs baseline :: {base_path} ===")
    sm, bm = s["measures"], b["measures"]
    d_mf = _i(sm.get("matched_functions")) - _i(bm.get("matched_functions"))
    d_mc = _i(sm.get("matched_code")) - _i(bm.get("matched_code"))
    print(f"   STRICT functions   {d_mf:+d}")
    print(f"   STRICT code bytes  {d_mc:+,d}")
    sw, bw = s["whole"], b["whole"]
    d_whole = _fuzzy_code_pct(sw) - _fuzzy_code_pct(bw)
    d_wired = _fuzzy_code_pct(sw, wired=True) - _fuzzy_code_pct(bw, wired=True)
    print(f"   FUZZY-CODE whole   {d_whole:+.4f} pct-pt")
    print(f"   FUZZY-CODE wired   {d_wired:+.4f} pct-pt")
    print(f"   fuzzy-credit bytes {sw['code_fuzzy'] - bw['code_fuzzy']:+,.0f} B")
    for t in STAIRCASE:
        d = sw["stair"][t] - bw["stair"][t]
        print(f"   staircase >={t:<3d}    {d:+d}")


def to_json(s):
    m = s["measures"]
    whole = s["whole"]
    out = {
        "strict": {
            "matched_functions": _i(m.get("matched_functions")),
            "total_functions": _i(m.get("total_functions")),
            "matched_functions_pct": _pct(_i(m.get("matched_functions")),
                                          _i(m.get("total_functions"))),
            "matched_code": _i(m.get("matched_code")),
            "total_code": _i(m.get("total_code")),
            "matched_code_pct": _pct(_i(m.get("matched_code")),
                                     _i(m.get("total_code"))),
        },
        "fuzzy_code": {
            "whole_binary_pct": _fuzzy_code_pct(whole),
            "wired_pct": _fuzzy_code_pct(whole, wired=True),
            "wired_fns": whole["wired"],
            "credit_bytes": whole["code_fuzzy"] - _i(m.get("matched_code")),
        },
        "staircase": {f"ge{t}": whole["stair"][t] for t in STAIRCASE},
        "histogram": dict(whole["bands"]),
        "tiers": {
            name: {
                "fns": acc["fns"], "wired": acc["wired"],
                "ge100": acc["stair"][100], "ge95": acc["stair"][95],
                "ge90": acc["stair"][90],
                "fuzzy_code_whole_pct": _fuzzy_code_pct(acc),
                "fuzzy_code_wired_pct": _fuzzy_code_pct(acc, wired=True),
            }
            for name, acc in s["tiers"].items()
        },
    }
    return out


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--report", default=DEFAULT,
                    help="report.json to summarize (default: build/45410914/report.json)")
    ap.add_argument("--baseline", default="",
                    help="a second report.json to diff against (wave fuzzy delta)")
    ap.add_argument("--by-unit", type=int, default=0, metavar="N",
                    help="also print top N units by fuzzy headroom")
    ap.add_argument("--min-size", type=int, default=0, metavar="BYTES",
                    help="exclude fns smaller than this from the staircase "
                         "(ICF stub-fold preview; NOT a substitute for icf_alias_check.py)")
    ap.add_argument("--json", action="store_true",
                    help="emit a machine-readable JSON summary instead of the headline")
    a = ap.parse_args()

    if not os.path.exists(a.report):
        sys.exit(f"[fuzzy_progress] no report.json at {a.report} — build first")

    s = summarize(a.report, a.min_size)

    if a.json:
        print(json.dumps(to_json(s), indent=2))
        return

    print_headline(s, a.min_size)
    if a.baseline:
        if not os.path.exists(a.baseline):
            sys.exit(f"[fuzzy_progress] no baseline report at {a.baseline}")
        print_baseline(s, a.baseline, a.min_size)
    if a.by_unit:
        print_by_unit(s, a.by_unit)


if __name__ == "__main__":
    main()
