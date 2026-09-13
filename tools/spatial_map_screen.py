#!/usr/bin/env python3
"""Spatial map-defect screen: find map rows whose ADDRESS contradicts their NAME.

Premise (verified, CLAUDE.md): RB3 retail is ``/O1`` with **no LTCG**, so the
linker preserves TU spatial grouping in ``.text``. A class's out-of-line member
functions therefore form a contiguous *cluster*. A map row that sits **outside
its owner class's cluster** and **inside another class's unbroken run** is a
candidate mis-identification.

Motivating defect (lane W8-C, merge ``0e3cc8b5``,
``docs/decomp/RNDENVIRON_2026-09-13.md``): ``0x824302B0`` was mapped
``?Save@RndEnviron@@`` while sitting in the middle of an unbroken run of
``RndPostProc`` methods, ~0x1EA60 from any other ``RndEnviron`` row. It is
``?Save@RndPostProc@@``. The defect was **circular** -- ``splits.txt`` had a
hole punched at exactly that range, so the wrong name justified the wrong pin
and the wrong pin made the wrong name score.

WHY A NEW TOOL RATHER THAN ``tools/map_lint.py --check class_mixing``
--------------------------------------------------------------------
``check_class_mixing`` is **PIN-RELATIVE**: it walks ``splits.txt`` units and
asks "which map names inside this pinned range have a foreign owner class?".
W8-C's defect was *self-consistent* -- the pin had been moved to agree with the
wrong name -- so the owner matched the unit's own class family and the check
**cannot fire, by construction**. This screen is **MAP-RELATIVE**: the reference
frame is the map's own address ordering; ``splits.txt`` is never consulted. The
two checks are complementary, not redundant.

CONTROL (runs on EVERY scan, not just ``--selftest``)
-----------------------------------------------------
A scan whose negative would close a vein must assert a known positive in the
same run (lane W7-B's first detector could not rediscover its own case; lane
W8-B's oracle scan returned a clean decisive "0 sites" from a tab-vs-space bug).
So every invocation:

  1. POSITIVE -- reverts W8-C's two map rows in memory and REQUIRES the screen
     to flag ``0x824302B0``.
  2. NEGATIVE -- REQUIRES the unmutated map NOT to flag it (so the positive is
     not "fires on everything").
  3. VACUITY -- REQUIRES a plausible participating population and requires the
     structural filters to actually be suppressing rows.

Any failure exits non-zero and prints NO findings: a broken screen must not be
able to report a reassuring empty result.
"""
from __future__ import annotations

import argparse
import bisect
import collections
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
DEFAULT_MAP = os.path.join(REPO, "scripts", "target_symbol_map.json")
DEFAULT_REPORT = os.path.join(REPO, "build", "45410914", "report.json")

# --- known-positive fixture (lane W8-C) ------------------------------------
# Reverting these two rows reconstructs the map exactly as it stood at main
# 2fc2552a, immediately before W8-C's fix. Verified against
# `git show 2fc2552a:scripts/target_symbol_map.json` -- those are the only two
# name-bearing rows that differ.
KNOWN_POSITIVE_ADDR = 0x824302B0
KNOWN_POSITIVE_REVERT = {
    "0x824302b0": "?Save@RndEnviron@@UAAXAAVBinStream@@@Z",
    "0x82407e58": None,
}
KNOWN_POSITIVE_SRC = "lane W8-C, merge 0e3cc8b5, docs/decomp/RNDENVIRON_2026-09-13.md"

# Vacuity floors. A run below these is a broken screen, not a clean tree.
MIN_PARTICIPATING = 12000
MIN_SUPPRESSED = 5000

# Namespaces whose members are library/vendor instantiations, not TU-pinned.
_STL_NS = {"stlpmtx_std", "stlp_std", "std", "stlport", "soundtouch"}


# ---------------------------------------------------------------------------
# Strict MSVC owner extraction
# ---------------------------------------------------------------------------
# The owner is the IMMEDIATE ENCLOSING CLASS SCOPE of an out-of-line member
# function. Everything whose placement is NOT pinned to its class's defining TU
# is TRANSPARENT: neither a candidate nor a run-breaker. Being transparent is
# what lets a run of class-Y methods with an interleaved COMDAT still read as
# unbroken.
#
# NOTE: this deliberately does NOT reuse ``map_lint.mangled_classes``. That
# helper falls back to the *element type pulled from the signature* when a
# symbol has no class scope, which is right for its purpose (attributing a
# container helper to its element's TU) and wrong for ours: a free function
# ``?ReplaceSubdir@@YAXPAVObjectDir@@0@Z`` would be attributed to ``ObjectDir``,
# whose TU says nothing about where the free function was emitted. It also
# mis-parses ``??_G``/``??_E`` special names (``??_GRndPostProc@@`` yields owner
# ``GRndPostProc`` -- the prefix letter is glued onto the class), which would
# shred otherwise-unbroken runs. Both are recorded in
# ``docs/decomp/SPATIAL_SCREEN_2026-09-13.md`` as handoffs against map_lint.


def strict_owner(name):
    """Return ``(owner_key, reason)``; ``owner_key is None`` means transparent.

    ``owner_key`` is the scope path OUTERMOST-FIRST, joined with ``::`` --
    ``?MoveSideEffects@Block@D3DXShader@@`` -> ``D3DXShader::Block``.
    """
    if not name or not name.startswith("?"):
        return None, "not-cxx"
    if name.startswith(("??_C", "??_7", "??_8", "??_R", "??_9")):
        return None, "data-or-vcall"                # .rdata / vcall thunk
    if name.startswith("??__"):
        return None, "dynamic-init"                 # ??__E / ??__F globals
    if re.match(r"^\?\?_[BDEG]", name):
        # guard / vbase dtor / vector+scalar deleting dtor: compiler-generated
        # COMDATs, emitted in every TU that needs them and folded arbitrarily.
        return None, "compiler-generated-comdat"
    if "?$" in name:
        return None, "template-comdat"
    if name.startswith("??"):
        m = re.match(r"^\?\?([0-9A-Z])(.*)$", name)
        if not m:
            return None, "unparsed-special"
        rest = m.group(2)
    else:
        rest = name[1:]
        i = rest.find("@")
        if i < 0:
            return None, "unparsed"
        rest = rest[i + 1:]                          # drop the method name
    if rest.startswith("@"):
        return None, "free-function"                 # ?Foo@@YA... / ??2@YA...
    if "@@" not in rest:
        return None, "no-scope-terminator"
    scope, sig = rest.split("@@", 1)
    if sig.startswith("$"):
        # ?Highlight@RndDrawable@@$4PPPPPPPM@OE@AAXXZ -- an adjustor thunk is
        # emitted in the DERIVED class's TU, so its placement is expected to be
        # foreign to the class it names.
        return None, "adjustor-thunk"
    segs = [s for s in scope.split("@") if s]
    if not segs:
        return None, "free-function"
    if any(s.startswith("$") or s.startswith("?") for s in segs):
        return None, "template-scope"
    if set(segs) & _STL_NS:
        return None, "stl-namespace"
    if len(segs[0]) < 2:
        return None, "degenerate"
    return "::".join(reversed(segs)), "ok"


def method_key(name):
    """Bare method name, for the 'hole test' (does the HOST class lack a method
    of this name?). Constructors/destructors/operators collapse to a token."""
    if name.startswith("??"):
        m = re.match(r"^\?\?([0-9A-Z])", name)
        if not m:
            return "#?"
        return {"0": "#ctor", "1": "#dtor"}.get(m.group(1), "#op" + m.group(1))
    return name[1:].split("@", 1)[0]


# ---------------------------------------------------------------------------
# Population + clustering
# ---------------------------------------------------------------------------

def load_map(path):
    with open(path) as fh:
        return json.load(fh)


def build_population(md):
    """-> (participating rows sorted by address, Counter of transparent reasons).

    A row is EXCLUDED (transparent) when its name is not established evidence of
    identity: the map's own ``_bijection_arbitrary`` / ``_icf_arbitrary`` /
    ``_denylist`` sets say so in their own comments ("Any tool deriving
    identity, callers, or unit ownership from these entries must treat them as
    UNRESOLVED").
    """
    excl = set()
    for key in ("_bijection_arbitrary", "_icf_arbitrary",
                "_denylist", "_denylist_unadjudicated"):
        excl |= {str(x).lower() for x in md.get(key, [])}
    part = []
    why = collections.Counter()
    for k, v in md.items():
        if k.startswith("_"):
            continue
        if not v:
            why["null-row"] += 1
            continue
        if k.lower() in excl:
            why["arbitrary-name"] += 1
            continue
        owner, reason = strict_owner(v)
        if owner is None:
            why[reason] += 1
            continue
        part.append((int(k, 16), v, owner))
    part.sort()
    return part, why


def _clusters(addrs, gap):
    out = [[addrs[0]]]
    for a in addrs[1:]:
        if a - out[-1][-1] > gap:
            out.append([a])
        else:
            out[-1].append(a)
    return out


def screen(part, flank_min=3, iso_min=0x4000, cluster_gap=0x8000, min_home=3):
    """Flag rows sitting outside their owner's home cluster and inside another
    owner's unbroken run.

    ``home cluster`` is derived FROM THE MAP ITSELF: the owner's addresses are
    segmented at gaps > ``cluster_gap`` and the most populous segment is the
    home. Requiring ``min_home`` members is what suppresses the largest benign
    class -- a *companion* class with no TU of its own (``TourDescEntry``
    defined inside ``TourDesc.cpp``), whose map name is CORRECT.
    """
    by_owner = collections.defaultdict(list)
    for addr, _name, owner in part:
        by_owner[owner].append(addr)
    home = {}
    for owner, addrs in by_owner.items():
        best = max(_clusters(sorted(addrs), cluster_gap), key=len)
        home[owner] = (best[0], best[-1], len(best))

    methods = collections.defaultdict(set)
    for _a, name, owner in part:
        methods[owner].add(method_key(name))

    fires = []
    n = len(part)
    for i in range(1, n - 1):
        addr, name, owner = part[i]
        left, right = part[i - 1][2], part[i + 1][2]
        if left != right or left == owner:
            continue
        host = left
        # A nested class (Outer::Inner) or a shared namespace is an EXPECTED
        # co-location, not a defect: ChunkStream::ChunkInfo lives in
        # ChunkStream.cpp, and every D3DXShader::* vendor class shares one blob.
        if owner.split("::")[0] == host.split("::")[0]:
            continue
        run_l = 0
        j = i - 1
        while j >= 0 and part[j][2] == host:
            run_l += 1
            j -= 1
        run_r = 0
        j = i + 1
        while j < n and part[j][2] == host:
            run_r += 1
            j += 1
        flank = min(run_l, run_r)
        if flank < flank_min:
            continue
        hlo, hhi, hn = home[owner]
        if hn < min_home:
            continue                                  # no real home cluster
        if hlo <= addr <= hhi:
            continue                                  # inside its own home
        dist = (hlo - addr) if addr < hlo else (addr - hhi)
        if dist < iso_min:
            continue
        fires.append({
            "addr": addr,
            "name": name,
            "owner": owner,
            "host": host,
            "flank": flank,
            "run_left": run_l,
            "run_right": run_r,
            "dist_to_home": dist,
            "home_lo": "0x%08x" % hlo,
            "home_hi": "0x%08x" % hhi,
            "home_rows": hn,
            "owner_rows": len(by_owner[owner]),
            # Corroboration W8-C actually used: RndPostProc held 38 rows and no
            # `Save` -- the one virtual missing from the run.
            "host_lacks_method": method_key(name) not in methods[host],
            "method": method_key(name),
        })
    return fires


# ---------------------------------------------------------------------------
# Controls -- these run on EVERY scan
# ---------------------------------------------------------------------------

class ControlFailure(Exception):
    pass


def run_controls(md, params, verbose=True):
    """Positive / negative / vacuity. Raises ControlFailure on any failure.

    Returns a list of human-readable control lines for the report header.
    """
    lines = []

    # --- VACUITY -----------------------------------------------------------
    part, why = build_population(md)
    suppressed = sum(why.values())
    if len(part) < MIN_PARTICIPATING:
        raise ControlFailure(
            "VACUITY: only %d participating rows (floor %d) -- the population "
            "collapsed, so any empty result is meaningless."
            % (len(part), MIN_PARTICIPATING))
    if suppressed < MIN_SUPPRESSED:
        raise ControlFailure(
            "VACUITY: only %d rows suppressed by the structural filters (floor "
            "%d) -- the filters are not running." % (suppressed, MIN_SUPPRESSED))
    for reason in ("adjustor-thunk", "template-comdat", "free-function",
                   "compiler-generated-comdat"):
        if not why.get(reason):
            raise ControlFailure(
                "VACUITY: filter %r suppressed nothing; the extractor is not "
                "classifying." % reason)
    lines.append("CONTROL vacuity     PASS  participating=%d suppressed=%d"
                 % (len(part), suppressed))

    # --- NEGATIVE ----------------------------------------------------------
    base_fires = screen(part, **params)
    if any(f["addr"] == KNOWN_POSITIVE_ADDR for f in base_fires):
        raise ControlFailure(
            "NEGATIVE control: 0x%08X fires on the UNMUTATED map. Either the "
            "W8-C fix has been reverted in this tree (then this is correct and "
            "the fixture must be updated) or the screen fires indiscriminately."
            % KNOWN_POSITIVE_ADDR)
    lines.append("CONTROL negative    PASS  0x%08X silent on the current map"
                 % KNOWN_POSITIVE_ADDR)

    # --- POSITIVE ----------------------------------------------------------
    mutated = dict(md)
    for k, v in KNOWN_POSITIVE_REVERT.items():
        mutated[k] = v
    mpart, _ = build_population(mutated)
    mfires = screen(mpart, **params)
    hit = [f for f in mfires if f["addr"] == KNOWN_POSITIVE_ADDR]
    if not hit:
        raise ControlFailure(
            "POSITIVE control: the screen does NOT rediscover 0x%08X with "
            "W8-C's fix reverted (%s). A screen that cannot find the case it "
            "was built from is worthless." % (KNOWN_POSITIVE_ADDR,
                                              KNOWN_POSITIVE_SRC))
    h = hit[0]
    lines.append("CONTROL positive    PASS  0x%08X flagged with W8-C reverted "
                 "(owner=%s host=%s flank=%d dist=0x%x host_lacks_%s=%s)"
                 % (KNOWN_POSITIVE_ADDR, h["owner"], h["host"], h["flank"],
                    h["dist_to_home"], h["method"], h["host_lacks_method"]))
    if verbose:
        for ln in lines:
            print(ln)
    return lines, part, base_fires


def load_sizes(path):
    """name -> (size, fuzzy, unit) from report.json. Protobuf-JSON omits
    defaults and stores several numerics as STRINGS -- coerce every read."""
    try:
        with open(path) as fh:
            rep = json.load(fh)
    except (OSError, ValueError):
        return {}
    out = {}
    for unit in rep.get("units", []):
        for fn in unit.get("functions", []):
            out[fn["name"]] = (int(fn.get("size", 0)),
                               float(fn.get("fuzzy_match_percent", 0.0)),
                               unit.get("name", "?"))
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="Spatial map-defect screen (map-relative; never reads "
                    "splits.txt).")
    ap.add_argument("--map", default=DEFAULT_MAP)
    ap.add_argument("--report", default=DEFAULT_REPORT,
                    help="report.json, for size/fuzzy columns (optional)")
    ap.add_argument("--flank-min", type=int, default=3,
                    help="minimum host-run length on BOTH sides (default 3)")
    ap.add_argument("--iso-min", type=lambda s: int(s, 0), default=0x4000,
                    help="minimum distance from the owner's home cluster")
    ap.add_argument("--cluster-gap", type=lambda s: int(s, 0), default=0x8000,
                    help="address gap that separates two clusters of one owner")
    ap.add_argument("--min-home", type=int, default=3,
                    help="owner's home cluster must hold at least this many "
                         "rows (suppresses companion classes with no TU)")
    ap.add_argument("--json", help="write findings here")
    ap.add_argument("--selftest", action="store_true",
                    help="run the controls and exit (no findings printed)")
    ap.add_argument("--break-control", choices=["positive", "negative", "vacuity"],
                    help="deliberately sabotage one control, to prove it fails")
    args = ap.parse_args(argv)

    params = dict(flank_min=args.flank_min, iso_min=args.iso_min,
                  cluster_gap=args.cluster_gap, min_home=args.min_home)
    md = load_map(args.map)

    if args.break_control == "positive":
        # Make the fixture a no-op: the reverted map == the current map, so the
        # positive control MUST fail.
        KNOWN_POSITIVE_REVERT.clear()
    elif args.break_control == "negative":
        # Plant the defect in the live map: the negative control MUST fail.
        md["0x824302b0"] = "?Save@RndEnviron@@UAAXAAVBinStream@@@Z"
        md["0x82407e58"] = None
    elif args.break_control == "vacuity":
        md = {k: v for k, v in md.items() if k.startswith("_")}

    print("map:    %s" % args.map)
    print("params: flank_min=%d iso_min=0x%x cluster_gap=0x%x min_home=%d"
          % (args.flank_min, args.iso_min, args.cluster_gap, args.min_home))
    try:
        _lines, _part, fires = run_controls(md, params)
    except ControlFailure as exc:
        print("\nCONTROL FAILURE -- no findings reported.\n  %s" % exc,
              file=sys.stderr)
        return 3
    if args.selftest:
        print("\nselftest: all controls PASS")
        return 0

    sizes = load_sizes(args.report)
    for f in fires:
        sz, fz, unit = sizes.get(f["name"], (0, 0.0, "?"))
        f["size"], f["fuzzy"], f["unit"] = sz, fz, unit
    # Rank: corroborated first, then by how isolated, then by size-at-stake.
    fires.sort(key=lambda f: (not f["host_lacks_method"], -f["flank"],
                              -f["size"], -f["dist_to_home"]))

    print("\n%d fire(s)\n" % len(fires))
    print("%-11s %-4s %-4s %-9s %-6s %-7s %-26s %-24s %s"
          % ("addr", "hole", "flk", "dist", "size", "fuzzy", "owner", "host",
             "method"))
    for f in fires:
        print("0x%08x %-4s %-4d %-9s %-6d %-7.2f %-26s %-24s %s"
              % (f["addr"], "YES" if f["host_lacks_method"] else ".",
                 f["flank"], hex(f["dist_to_home"]), f["size"], f["fuzzy"],
                 f["owner"][:26], f["host"][:24], f["method"]))
    if args.json:
        with open(args.json, "w") as fh:
            json.dump({"params": params, "fires": fires}, fh, indent=1)
        print("\nwrote %s" % args.json)
    return 0


if __name__ == "__main__":
    sys.exit(main())
