#!/usr/bin/env python3
"""fingerprint_pipeline.py — same-compiler GAME-code fingerprint pipeline.

The premise (verified by spike, 2026-05-30): rb3-Wii game source compiled under
OUR MSVC X360 toolchain fingerprints the retail XEX far better than the
cross-compiler rb3-Wii *binary* bindiff oracle. Same-compiler dc3 bindiff runs
~0.98 similarity vs ~0.25 cross-compiler; a faithfully-ported game TU hits
95-100% objdiff fuzzy on revision-matched functions (GameMode ctor 99.97%,
InMode 95.99%). Yield is BIMODAL: ~0% on functions where rb3-Wii DEV diverges
from retail-360 (e.g. GameMode::SetMode is a different revision).

This driver mechanizes the loop around that fact. Porting fixes themselves stay
manual/agent (MWCC->MSVC isn't auto-translatable), but everything else is here:

  candidates  rank UNPORTED game TUs by rb3-Wii oracle coverage + source
              presence in ../rb3  ->  the porting work-list, highest-yield first.
  scaffold T  copy ../rb3 source into src/, print the objects.json entry +
              splits.txt block to add (target span from the oracle). The compile
              fixes are the operator's job; this removes the boilerplate.
  manifest    parse build/.../report.json and emit the FINGERPRINT QUALITY
              report for every game/network unit: per-TU + per-function objdiff
              fuzzy/match, bucketed (matched / high>=80 / mid / low<20). The
              high+matched buckets are the same-compiler "hit rate" — functions
              the fingerprint locks confidently. Run after a build.

Prereqs (both DONE as of 2026-05-30): scripts/target_symbol_map.json carries the
game fn_<addr>->mangled entries (tools/gen_game_target_map.py), and the
obj_target_symbol_renamer + patchers are wired in configure.py:284-365 — so
report.json fuzzy% for game TUs is REAL, not a false-0%.
"""
import argparse, json, os, sys
from collections import defaultdict
# --- dead-index guard (lane BX-4) -------------------------------------------
# These address indices are TU0-era and INFORMATIONLESS after the 2026-07-15
# TU0->TU5 flip (2-6% of their addresses are real .text function starts; an
# arbitrary address list scores ~2-3% by chance). Acting on them yields
# plausible-looking WRONG artifacts, so the load is hard-gated.
# Audit: python3 tools/dead_index_guard.py --audit
import os as _dig_os, sys as _dig_sys
_dig_d = _dig_os.path.dirname(_dig_os.path.abspath(__file__))
while _dig_d != "/" and not _dig_os.path.exists(
        _dig_os.path.join(_dig_d, "tools", "dead_index_guard.py")):
    _dig_d = _dig_os.path.dirname(_dig_d)
_dig_sys.path.insert(0, _dig_os.path.join(_dig_d, "tools"))
from dead_index_guard import load_guarded as _guarded_load, assert_live as _assert_live  # noqa: E402
# ----------------------------------------------------------------------------

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from game_splits import load_symbols, load_pinned_text_ranges, map_oracle_src, KNOWN_FP_RELS  # noqa

def _p(*a): return os.path.join(ROOT, *a)
RB3WII = os.environ.get("RB3WII_SRC", "/home/free/code/milohax/rb3/src")
ORACLE = _p("unified_id_rb3wii.json")
REPORT = _p("build", "45410914", "report.json")


def _oracle_by_tu(min_conf):
    """rel -> list of (addr, wii_name, conf), band3/network only, conf-gated."""
    by = defaultdict(list)
    for e in _guarded_load(ORACLE):
        if e.get("confidence", 0) < min_conf:
            continue
        src = e.get("bindiff_src") or ""
        if not (("band3/" in src) or ("network/" in src)):
            continue
        rel, _, _ = map_oracle_src(src)
        if rel in KNOWN_FP_RELS:
            continue
        try:
            by[rel].append((int(e["rb3_addr"], 16), e.get("wii_name", "?"),
                            round(e.get("confidence", 0), 3)))
        except (KeyError, ValueError):
            pass
    return by


def _wii_path(rel):
    """our rel (band3/game/X.cpp) -> rb3-Wii source path (../rb3/src/band3/game/X.cpp)."""
    return os.path.join(RB3WII, rel)


def cmd_candidates(args):
    by = _oracle_by_tu(args.min_conf)
    rows = []
    for rel, hits in by.items():
        ported = os.path.exists(_p("src", rel))
        if ported and not args.all:
            continue
        wii = _wii_path(rel)
        has_src = os.path.exists(wii)
        rows.append((len(hits), has_src, ported, rel, wii))
    # work-list: source available, not yet ported, most oracle fns first
    rows.sort(key=lambda r: (-r[0], r[3]))
    print("%-4s %-7s %-6s %s" % ("ofns", "wiisrc", "ported", "TU"))
    n_workable = 0
    for nf, has_src, ported, rel, wii in rows:
        if nf < args.min_fns:
            continue
        flag = "yes" if has_src else "NO"
        if has_src and not ported:
            n_workable += 1
        print("%-4d %-7s %-6s %s" % (nf, flag, "yes" if ported else "-", rel))
    print(f"\n[candidates] {n_workable} workable (source present, unported, "
          f">={args.min_fns} oracle fns)", file=sys.stderr)


def cmd_scaffold(args):
    rel = args.tu
    wii = _wii_path(rel)
    if not os.path.exists(wii):
        sys.exit(f"[scaffold] no rb3-Wii source at {wii}")
    dst = _p("src", rel)
    if os.path.exists(dst) and not args.force:
        sys.exit(f"[scaffold] {dst} already exists (use --force to overwrite)")
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    if not args.dry_run:
        with open(wii) as f:
            open(dst, "w").write(f.read())
    print(f"[scaffold] {'(dry-run) would copy' if args.dry_run else 'copied'} "
          f"{wii} -> {dst}")
    # target span from oracle dominant cluster (reuse game_splits by hand)
    by = _oracle_by_tu(0.90)
    hits = sorted(a for a, _, _ in by.get(rel, []))
    grp = "band3" if rel.startswith("band3/") else "network"
    print(f"\n# add to config/45410914/objects.json '{grp}' group:")
    print(f'    "{rel}": "NonMatching",')
    if hits:
        funcs = load_symbols(_p("config", "45410914", "symbols.txt"))
        fends = sorted({a + s for a, s, _ in funcs if s})
        import bisect
        lo = hits[0]
        hi = fends[bisect.bisect_left(fends, hits[-1] + 1)] if fends else hits[-1] + 4
        print(f"\n# add to config/45410914/splits.txt (then `touch config.yml && ninja`):")
        print(f"{rel}:\n\t.text       start:0x{lo:08X} end:0x{hi:08X}")
        print(f"# (provisional span from {len(hits)} oracle fns — VERIFY before pinning)")
    print("\n# then port MWCC->MSVC until it compiles (NonMatching ok), build, "
          "and re-run: fingerprint_pipeline.py manifest", file=sys.stderr)


def _bucket(fz):
    if fz >= 100: return "matched"
    if fz >= 80: return "high"
    if fz >= 20: return "mid"
    return "low"


def cmd_manifest(args):
    if not os.path.exists(REPORT):
        sys.exit(f"[manifest] no report.json at {REPORT} — build first")
    r = json.load(open(REPORT))
    units = []
    for u in r["units"]:
        cats = u["metadata"].get("progress_categories") or []
        if not (("game" in cats) or ("network" in cats)):
            continue
        m = u["measures"]
        tot = float(m.get("total_code") or 0)
        if tot <= 0:
            continue
        fns = u.get("functions", [])
        buckets = defaultdict(list)
        for f in fns:
            buckets[_bucket(float(f.get("fuzzy_match_percent") or 0))].append(f)
        units.append((u["name"], cats, tot, m, fns, buckets))
    units.sort(key=lambda x: -(len(x[5]["matched"]) + len(x[5]["high"])))

    tot_fns = tot_hit = 0
    print("%-34s %6s %6s %4s %4s %4s %4s" %
          ("unit", "fuzzy%", "match%", "M", "Hi", "mid", "lo"))
    for name, cats, tot, m, fns, b in units:
        hit = len(b["matched"]) + len(b["high"])
        tot_fns += len(fns); tot_hit += hit
        print("%-34s %6.1f %6.1f %4d %4d %4d %4d" % (
            name.split("/")[-1], float(m.get("fuzzy_match_percent") or 0),
            float(m.get("matched_code_percent") or 0),
            len(b["matched"]), len(b["high"]), len(b["mid"]), len(b["low"])))
        if args.verbose:
            for f in sorted(b["matched"] + b["high"],
                            key=lambda f: -float(f.get("fuzzy_match_percent") or 0)):
                print("      %5.1f%%  %s" % (float(f.get("fuzzy_match_percent") or 0),
                                             f.get("name", "?")[:64]))
    print("\n[manifest] %d game/network units | %d functions | %d high-or-matched "
          "(same-compiler hit rate %.0f%%)" %
          (len(units), tot_fns, tot_hit, 100 * tot_hit / tot_fns if tot_fns else 0),
          file=sys.stderr)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    c = sub.add_parser("candidates", help="rank unported game TUs (porting work-list)")
    c.add_argument("--min-conf", type=float, default=0.90)
    c.add_argument("--min-fns", type=int, default=3)
    c.add_argument("--all", action="store_true", help="include already-ported")
    c.set_defaults(func=cmd_candidates)
    s = sub.add_parser("scaffold", help="copy rb3-Wii source + print config blocks")
    s.add_argument("tu", help="our rel path, e.g. band3/game/Stats.cpp")
    s.add_argument("--force", action="store_true")
    s.add_argument("--dry-run", action="store_true")
    s.set_defaults(func=cmd_scaffold)
    mf = sub.add_parser("manifest", help="fingerprint quality report from report.json")
    mf.add_argument("-v", "--verbose", action="store_true", help="list high/matched fns")
    mf.set_defaults(func=cmd_manifest)
    args = ap.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
