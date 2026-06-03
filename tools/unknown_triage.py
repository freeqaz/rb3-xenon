#!/usr/bin/env python3
"""unknown_triage.py -- honestly characterize the "unknown" scope bucket.

scope_map.py sums per-fn `size` fields to size the unknown bucket, but those
fields are UNRELIABLE for the catch-all `auto_*` units that dtk could not split
into real source TUs:

  * Some fn_ report a `size` far larger than the real VA gap to the next
    function (e.g. fn_828D3060 reports 257404 but the next fn_ is only 1928 B
    away). That excess is accounting noise that inflates the byte denominator.
  * Some large reported sizes ARE real: dtk's CFA found zero function boundaries
    across a long .text run (e.g. fn_82991358 -> 140 KB genuine gap). These are
    NOT a single function -- they are regions that need *boundary splitting*
    (a jeff/dtk lever), not *source identification*.
  * Many "unknown" scope_map entries are actually NAMED functions (demangled
    MSVC symbols) that just got swept into the unknown bucket by spatial fill;
    they are identified-by-name already.

This tool recomputes the unknown bucket honestly and partitions it into:

  real_unidentified  anonymous fn_ with size ~= real VA gap (a plausible single
                     function we should attribute to a source file)   [IDENTIFY]
  cfa_gap            large no-boundary runs (gap > CFA_THRESH)        [SPLIT]
  named_unpropagated named fns swept into unknown but already carrying a
                     mangled MSVC symbol                              [IDENTIFIED]
  noise              reported-size excess over the real VA gap (informational)

It uses report.json's real fn_ VAs to compute honest sizes; named functions in
catch-all units have no recoverable VA, so an anonymous fn_'s honest size is
min(reported_size, gap_to_next_fn_) -- an upper bound shared with any named fn
that interleaves (those carry their own reliable reported sizes elsewhere).

Subcommands:
  report    honest bucket totals (real_unidentified vs cfa_gap vs named vs noise)
  worklist  ranked top real_unidentified anonymous fns to attribute
  gaps      ranked CFA-gap regions (addr ranges + bytes) for the splitting lever
"""
import argparse
import bisect
import json
import os
import re
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPORT = os.path.join(ROOT, "build", "45410914", "report.json")
SCOPE_MAP = os.path.join(ROOT, "config", "45410914", "scope_map.json")

FN_ADDR_RE = re.compile(r"fn_([0-9A-Fa-f]{8})")

# A consecutive-fn_ gap larger than this is treated as a CFA boundary-detection
# hole (a region to split), not as one function's body. 16 KB is well above the
# largest plausible single MSVC /O1 function (~10 KB observed in this binary).
CFA_THRESH = 16 * 1024

# unified_id / structural-ID artifacts: if an addr appears here it is already
# attributed even if scope_map left it "unknown".
UID_FILES = [
    "unified_id.json",
    "unified_id_rb3wii.json",
    "unified_id_callgraph.json",
    "unified_id_rtti.json",
    "unified_id_vtable.json",
]


def load_report():
    with open(REPORT) as f:
        return json.load(f)


def load_scope_map():
    with open(SCOPE_MAP) as f:
        return json.load(f)


def build_index(rep):
    """Return:
      all_fn       sorted list of every real fn_ VA in the binary
      rep_size     {va: reported_size} for fn_
      named_count  number of named (non-fn_) functions
    """
    all_fn = []
    rep_size = {}
    named_count = 0
    for u in rep["units"]:
        for fn in (u.get("functions") or []):
            m = FN_ADDR_RE.match(fn["name"])
            if m:
                va = int(m.group(1), 16)
                all_fn.append(va)
                rep_size[va] = int(fn.get("size", "0"))
            else:
                named_count += 1
    all_fn = sorted(set(all_fn))
    return all_fn, rep_size, named_count


def load_uid_addrs():
    """Set of rb3 VAs that already have a name from any structural-ID artifact."""
    addrs = set()
    by_addr = {}
    for fname in UID_FILES:
        p = os.path.join(ROOT, fname)
        if not os.path.exists(p):
            continue
        try:
            d = json.load(open(p))
        except Exception:
            continue
        if not isinstance(d, list):
            continue
        for e in d:
            a = e.get("rb3_addr") or e.get("rb3_fn", "")
            if isinstance(a, str) and a.startswith("0x"):
                va = int(a, 16)
            elif isinstance(a, str) and a.startswith("fn_"):
                va = int(a[3:], 16)
            else:
                continue
            addrs.add(va)
            # keep the richest name we have
            name = (e.get("dc3_name_demangled") or e.get("wii_name")
                    or e.get("dc3_name") or "")
            src = (e.get("dc3_obj") or e.get("bindiff_src") or e.get("source") or "")
            if va not in by_addr or (name and not by_addr[va][0]):
                by_addr[va] = (name, src, e.get("source", fname))
    return addrs, by_addr


def triage(rep, sm):
    all_fn, rep_size, _ = build_index(rep)
    real_set = set(all_fn)

    # The unknown bucket according to scope_map.
    unk = {int(k, 16): v for k, v in sm.items() if v["scope"] == "unknown"}

    real_unid = []   # (va, honest_size, reported_size)
    cfa = []         # (va, next_va, gap)
    named_unprop = []  # (va_or_synth, reported_size)  -- named swept into unknown
    noise_bytes = 0

    for a, v in unk.items():
        rsz = v["size"]
        if a not in real_set:
            # synthetic addr -> this entry is a NAMED function spatial-anchored
            # near a fn_. It already has a mangled symbol => identified-by-name.
            named_unprop.append((a, rsz))
            continue
        i = bisect.bisect_right(all_fn, a)
        nxt = all_fn[i] if i < len(all_fn) else a + rsz
        gap = nxt - a
        if gap > CFA_THRESH:
            cfa.append((a, nxt, gap))
            # the reported size on the boundary fn double-counts the gap; ignore.
            continue
        honest = min(rsz, gap) if gap > 0 else rsz
        if rsz > gap + 16:
            noise_bytes += (rsz - gap)
        real_unid.append((a, honest, rsz))

    return {
        "real_unid": real_unid,
        "cfa": cfa,
        "named_unprop": named_unprop,
        "noise_bytes": noise_bytes,
        "all_fn": all_fn,
        "rep_size": rep_size,
    }


def neighbor_scope(sm, all_fn, va, real_set):
    """Nearest confidently-classified (non-unknown, non-xdk) fn scope on each
    side -- spatial TU hint (no-LTCG => contiguous TUs)."""
    i = bisect.bisect_left(all_fn, va)

    def look(direction):
        j = i + direction
        steps = 0
        while 0 <= j < len(all_fn) and steps < 40:
            e = sm.get("%08X" % all_fn[j])
            if e and e["scope"] not in ("unknown", "xdk", None):
                return e["scope"], e.get("provenance", ""), all_fn[j]
            j += direction
            steps += 1
        return None
    return look(-1), look(+1)


def cmd_report(args):
    rep = load_report()
    sm = load_scope_map()
    t = triage(rep, sm)

    real_bytes = sum(s for _, s, _ in t["real_unid"])
    cfa_bytes = sum(g for _, _, g in t["cfa"])
    named_bytes = sum(s for _, s in t["named_unprop"])

    unk_reported = sum(v["size"] for k, v in sm.items() if v["scope"] == "unknown")

    print("=== HONEST UNKNOWN-BUCKET TRIAGE ===")
    print("(scope_map 'unknown' bucket re-accounted by real VA gaps)\n")
    print("scope_map reported unknown bytes (UNRELIABLE):  %10d (%.2f MB)"
          % (unk_reported, unk_reported / 1e6))
    print("-" * 64)
    print("%-22s %8s %14s %10s" % ("partition", "fns", "bytes", "MB"))
    print("%-22s %8d %14d %10.2f   [IDENTIFY -> source]"
          % ("real_unidentified", len(t["real_unid"]), real_bytes, real_bytes / 1e6))
    print("%-22s %8d %14d %10.2f   [SPLIT -> jeff/dtk]"
          % ("cfa_gap", len(t["cfa"]), cfa_bytes, cfa_bytes / 1e6))
    print("%-22s %8d %14d %10.2f   [already named]"
          % ("named_unpropagated", len(t["named_unprop"]), named_bytes, named_bytes / 1e6))
    print("%-22s %8s %14d %10.2f   [size-inflation, dropped]"
          % ("noise (excess)", "-", t["noise_bytes"], t["noise_bytes"] / 1e6))
    print("-" * 64)
    honest_total = real_bytes + cfa_bytes + named_bytes
    print("%-22s %8s %14d %10.2f"
          % ("honest accounted", "-", honest_total, honest_total / 1e6))
    print()
    # size distribution of real_unidentified
    sizes = sorted((s for _, s, _ in t["real_unid"]), reverse=True)
    if sizes:
        big = [s for s in sizes if s >= 1000]
        med = sizes[len(sizes) // 2]
        print("real_unidentified size dist: max=%d median=%d mean=%.0f  |  >=1KB: %d fns / %.2f MB"
              % (sizes[0], med, sum(sizes) / len(sizes), len(big), sum(big) / 1e6))
    print("CFA-gap regions: %d, total %.2f MB  (run `gaps` for the list)"
          % (len(t["cfa"]), cfa_bytes / 1e6))


def cmd_worklist(args):
    rep = load_report()
    sm = load_scope_map()
    t = triage(rep, sm)
    uid_addrs, uid_names = load_uid_addrs()
    real_set = set(t["all_fn"])

    items = sorted(t["real_unid"], key=lambda x: -x[1])
    limit = args.limit or 30
    min_size = args.min_size or 0

    print("=== REAL-UNIDENTIFIED WORKLIST (honest size, top %d) ===" % limit)
    print("%-12s %7s %5s %-7s  %-26s %-26s %s"
          % ("addr", "bytes", "uid?", "side", "left_neighbor", "right_neighbor", "name(if uid)"))
    shown = 0
    for va, hsz, rsz in items:
        if hsz < min_size:
            continue
        in_uid = va in uid_addrs
        ln, rn = neighbor_scope(sm, t["all_fn"], va, real_set)
        lstr = ("%s" % ln[0]) if ln else "-"
        rstr = ("%s" % rn[0]) if rn else "-"
        side = "%s/%s" % (lstr[:3], rstr[:3])
        nm = ""
        if in_uid and va in uid_names:
            nm = uid_names[va][0][:48]
        print("0x%08X %7d %5s %-7s  %-26s %-26s %s"
              % (va, hsz, "Y" if in_uid else "-", side,
                 (ln[1][:26] if ln else "-"),
                 (rn[1][:26] if rn else "-"), nm))
        shown += 1
        if shown >= limit:
            break


def cmd_gaps(args):
    rep = load_report()
    sm = load_scope_map()
    t = triage(rep, sm)
    real_set = set(t["all_fn"])
    gaps = sorted(t["cfa"], key=lambda x: -x[2])
    limit = args.limit or 30
    print("=== CFA-GAP REGIONS (boundary-splitting lever, top %d) ===" % limit)
    print("These are .text runs with zero detected function boundaries -- they")
    print("need jeff/dtk splitting, NOT source identification.\n")
    print("%-12s %-12s %9s %8s  %-12s %-12s"
          % ("start", "end", "bytes", "KB", "left_scope", "right_scope"))
    total = 0
    for a, nxt, g in gaps[:limit]:
        ln, rn = neighbor_scope(sm, t["all_fn"], a, real_set)
        lstr = ln[0] if ln else "-"
        rstr = rn[0] if rn else "-"
        print("0x%08X 0x%08X %9d %8.1f  %-12s %-12s"
              % (a, nxt, g, g / 1024.0, lstr, rstr))
        total += g
    print("-" * 64)
    print("shown total: %d bytes (%.2f MB) of %d total CFA bytes"
          % (total, total / 1e6, sum(g for _, _, g in t["cfa"])))


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("report", help="honest bucket totals")
    w = sub.add_parser("worklist", help="ranked real_unidentified fns")
    w.add_argument("--limit", type=int, default=30)
    w.add_argument("--min-size", type=int, default=0)
    g = sub.add_parser("gaps", help="ranked CFA-gap regions")
    g.add_argument("--limit", type=int, default=30)
    args = ap.parse_args()
    {"report": cmd_report, "worklist": cmd_worklist, "gaps": cmd_gaps}[args.cmd](args)


if __name__ == "__main__":
    main()
