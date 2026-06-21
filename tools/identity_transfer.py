#!/usr/bin/env python3
"""Per-function MICRO-PIN identity transfer for ICF-SCATTERED TUs.

THE PROBLEM
-----------
Our pinning model is SPAN-based: ``config/45410914/splits.txt`` pins one
contiguous ``[.text start,end)`` per TU; dtk SPLITs that range into a per-TU
target ``.obj``; objdiff pairs target ``fn_<addr>`` to our compiled obj's MSVC
mangled symbols by NAME (via ``scripts/target_symbol_map.json`` + the
pre-compile ``obj_target_symbol_renamer``). This works when a TU's methods sit
in ONE contiguous cluster.

It FAILS for ICF-scattered TUs whose methods are strewn binary-wide with no
contiguous cluster (BandProfile: 104 methods 0x822639F0..0x82BD66B0, largest
run 3 fns; SongSortNode; LockStepMgr; ...). A single span pin cannot cover
them. But the rb3-Wii BinDiff oracle (``unified_id_rb3wii.json``) KNOWS each
method's VA + size + name. We want PER-FUNCTION identity transfer: carve each
scattered method into its TU's target obj, name it, and (if byte-exact) count.

THE MECHANISM (N-range generalization of the working 2-range Part/PropKeys pins)
--------------------------------------------------------------------------------
jeff ``apply_splits`` does a RAW ``ObjSplits::push`` per ``.text`` line under a
unit header (no auto-merge), so N ``.text`` ranges under one ``Foo.cpp:`` header
accumulate as N independent same-unit ObjSplits. dtk ``split_obj`` keys each
range's bytes to the unit by name, concatenating all fragments into ONE Foo
target ``.text``. ``create_gap_splits`` auto-owns the bytes between ranges
(``auto_<sec>_<addr>`` units) so 100% section coverage holds. objdiff then
pairs the renamed micro-range fragment against the compiled Foo.obj base symbol.

This tool is the N-range APPENDER + the case classifier. It emits, for one TU:
  (a) a splits.txt block = additional ``.text start:.. end:..`` lines appended
      under the ``Foo.cpp:`` header (coalescing contiguous oracle runs);
  (b) target_symbol_map.json ``{0xVA: mangled}`` entries (via
      gen_game_target_map's oracle->compiled-symbol naming);
  (c) a deferred case-b worklist (foreign-pinned methods, eviction-gated).

THE ATTRIBUTION-ORPHAN VERDICT (enforced, fail-closed)
------------------------------------------------------
objdiff pairing is STRICTLY per-unit by name (no cross-obj reach). Three cases
per scattered method M at VA:
  * CASE-A: VA is in UNOWNED auto-blob -> a micro-range carves M into Foo's obj
    with ZERO foreign conflict. THIS TOOL PINS THESE.
  * SELF:   VA is already inside Foo's own pin -> reveal_sweep territory (a map
    name is enough); SKIP here.
  * CASE-B: VA is inside a FOREIGN unit Bar's pin -> M is carved into Bar.obj,
    which does NOT define M's body, so a name alone gives 0%. Re-carving M needs
    EVICTING M from Bar's pin (validate_splits forbids two units owning one
    address) -- a separate A/B-gated trade. THIS TOOL SKIPS THESE (records them
    to a worklist). A covering_pin bug returning None for a foreign-owned addr
    would silently break the build, so the SKIP is load-bearing.

USAGE
-----
  tools/identity_transfer.py --tu BandProfile.cpp                 # dry-run report
  tools/identity_transfer.py --tu BandProfile.cpp --apply \\
        --splits config/45410914/splits.txt                      # write (worktree!)

ALWAYS run --apply from a git worktree (it mutates splits.txt + the map).
The map write is ADD-ONLY (never wholesale-regenerates the 12k-entry file).

COLLISION-SAFETY (FIX 1, wave-16 -14 regression guard)
------------------------------------------------------
If the TU ALREADY carries a wide SPAN pin (>= SPAN_PIN_MIN), the span renamer
already mints the mangled name of every method in that cluster. A case-A
micro-pin OUTSIDE the span whose mangled NAME duplicates a span-carved method
mints a SECOND target symbol that STEALS pairing from the real body -- a net
REGRESSION even though it byte-matches. So the tool:
  * detects an existing span pin for the TU (Fix-1a),
  * filters any micro-pin whose mangled name collides with a span-carved method
    OR an earlier micro-pin in the batch (intra-batch dedup), dropping the WHOLE
    range (Fix-1b) -- the carved fragment mints the duplicate even with no map
    entry,
  * and FAILS CLOSED on a span-pinned TU (emits nothing) unless
    ``--allow-span-coexist`` is passed to apply the non-colliding remainder.

TRUTHFUL ESTIMATOR (FIX 2)
--------------------------
The dry-run reports a not-yet-matched EV by joining the carved case-A bodies
against report.json's live 100-set ON THE MANGLED NAME (report ``address`` is
section-relative decimal, not a VA). It brackets the honest range with a
CONSERVATIVE floor (sim>=0.5, an independent ownership signal) and an OPTIMISTIC
ceiling (no sim gate): both require size>44B, a resolvable mangled name, survival
of the Fix-1 filter, and the name NOT already matched. Override the report path
with ``--report``.
"""
import argparse
import bisect
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))

DEF_SPLITS = os.path.join(ROOT, "config", "45410914", "splits.txt")
DEF_SYMBOLS = os.path.join(ROOT, "config", "45410914", "symbols.txt")
DEF_ORACLE = os.path.join(ROOT, "unified_id_rb3wii.json")
DEF_MAP = os.path.join(ROOT, "scripts", "target_symbol_map.json")
DEF_REPORT = os.path.join(ROOT, "build", "45410914", "report.json")

# A pin wider than this is a SPAN pin (a contiguous TU cluster dtk carves whole
# into one target obj); anything <= this is a per-method MICRO pin (the widest
# real method body is well under this). 638 TUs carry one wide span pin. The
# wave-16 -14 regression came from appending case-A micro-pins to a TU that
# already had such a span pin: the duplicate mangled name STEALS pairing.
SPAN_PIN_MIN = 0x800

# REUSED primitives (do NOT reimplement): boundary snap + splits parse.
from pin_identified import load_sizes, compute_starts, parse_splits  # noqa: E402

UNIT_RE = re.compile(r"^(\S+\.(?:cpp|c|cc)):\s*$")
TEXT_RE = re.compile(r"^(\s*)\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)")


def tu_base(src):
    return src.replace("\\", "/").rsplit("/", 1)[-1]


def load_all_sym_bounds(path):
    """Return (starts, spans) over EVERY symbol in symbols.txt (not just .text
    functions). starts = sorted list of every symbol address; spans = list of
    (addr, end) so we can reject a micro-range END that bisects a NON-function
    object (e.g. ``except_data_*`` / ``except_record_*`` interleaved in .text).
    The function-only ``compute_starts`` misses these and lets the end-snap land
    inside one (real failure: dtk 'ends within symbol except_data_*')."""
    rx = re.compile(
        r"^\S+ = \.(\w+):0x([0-9A-Fa-f]+); // type:\w+ size:0x([0-9A-Fa-f]+)")
    starts = set()
    spans = []
    for line in open(path):
        m = rx.match(line)
        if not m:
            continue
        a = int(m.group(2), 16)
        sz = int(m.group(3), 16)
        starts.add(a)
        if sz > 0:
            spans.append((a, a + sz))
    return sorted(starts), sorted(spans)


def all_text_ranges(path):
    """Return [(lo, hi, cpp)] for EVERY .text line (multi-range aware)."""
    cur = None
    out = []
    for line in open(path):
        m = UNIT_RE.match(line)
        if m:
            cur = m.group(1)
            continue
        t = TEXT_RE.match(line)
        if t and cur:
            out.append((int(t.group(2), 16), int(t.group(3), 16), cur))
    out.sort()
    return out


def load_matched_names(report_path):
    """Return the set of CURRENTLY-MATCHED symbol names from report.json, or
    None if the report is missing/unreadable.

    The join key MUST be the mangled NAME, not the address: report's
    ``address`` is SECTION-RELATIVE DECIMAL (not a VA), so VAs can't join. A
    function counts as matched at ``match_percent_normalized >= 99.99``; when a
    target paired against our compiled obj its ``name`` is the MSVC mangled
    symbol -- the SAME value ``gen_game_target_map.build_tu_entries`` emits -- so
    a mangled-name membership test answers "is this method already matched?".
    Unpaired functions keep an ``fn_<addr>`` name; those are added too but never
    collide with a mangled candidate, so they are harmless."""
    try:
        rep = json.load(open(report_path))
    except (OSError, ValueError):
        return None
    matched = set()
    for u in rep.get("units", []):
        for f in u.get("functions") or []:
            mp = f.get("match_percent_normalized")
            name = f.get("name")
            if mp is not None and mp >= 99.99 and name:
                matched.add(name)
    return matched


def _truthful_estimate(args, case_a, sizes, all_entries, final_carved):
    """FIX 2: replace the non-predictive "named bodies" count (which counts
    methods ALREADY matched) with a join against report.json's live 100-set.

    report.json ``address`` is SECTION-RELATIVE DECIMAL (not a VA), so the join
    is on the mangled NAME: oracle VA -> all_entries[hexVA] -> mangled name ->
    is it in load_matched_names(report). The truthful EV counts a case-A body
    iff ALL of:
      * resolves to a mangled name (nameable),
      * size > 44 (retail symbols.txt size preferred; oracle size is the rb3-Wii
        size and under-counts ~3x),
      * survives the FINAL Fix-1-filtered carving (in final_carved),
      * AND that mangled name is NOT already in the report's matched-name set.
    The sim>=0.5 gate is an INDEPENDENT ownership signal (correctness RULE 3);
    it is reported as a CONSERVATIVE floor because oracle ``similarity`` is the
    BinDiff Wii<->Xbox structural similarity (median ~0.125), NOT a byte-match
    predictor, so it badly under-counts genuinely byte-matchable wired methods.
    A second OPTIMISTIC ceiling drops the sim gate (size>44 + nameable +
    not-yet-matched + survives carve)."""
    matched_names = load_matched_names(args.report)
    if matched_names is None:
        print("truthful EV               : report.json unreadable "
              f"({args.report}) -- skipping not-yet-matched estimate")
        return

    n_total = len(case_a)
    n_already = n_stub = n_unnameable = 0
    n_dropped = 0          # not pinned (bisect-rejected / Fix-1-filtered / gated)
    ceiling = []           # not-yet-matched, >44B, nameable, survives carve
    floor = []             # ceiling AND sim>=0.5

    for e in case_a:
        addr = int(e["rb3_addr"], 16)
        hexva = f"0x{addr:08X}"
        # retail size first (oracle size under-counts ~3x); fall back to oracle.
        sz = sizes.get(addr, e.get("size", 0))
        name = all_entries.get(hexva)
        sim = e.get("similarity", 0.0)
        if addr not in final_carved:
            n_dropped += 1          # not pinned: bisect-rejected, collision-
            continue                # filtered, or span-gated away
        if name is None:
            n_unnameable += 1
            continue
        if sz <= 44:
            n_stub += 1
            continue
        if name in matched_names:
            n_already += 1
            continue
        ceiling.append((hexva, name, sz, sim))
        if sim >= 0.5:
            floor.append((hexva, name, sz, sim))

    print(f"truthful EV (CONSERVATIVE floor; sim>=0.5, >44B, not-yet-matched): "
          f"{len(floor)}")
    print(f"truthful EV (OPTIMISTIC ceiling; no sim gate, >44B, not-yet-matched): "
          f"{len(ceiling)}")
    print(f"  of {n_total} case-A bodies: "
          f"{n_dropped} not-pinned (bisect/Fix-1-filter/span-gate), "
          f"{n_already} already-matched, {n_stub} stub<=44B, "
          f"{n_unnameable} unnameable, {len(ceiling)} new candidates "
          f"({len(ceiling) - len(floor)} of them low-sim<0.5)")


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--tu", required=True,
                    help="target TU basename, e.g. BandProfile.cpp")
    ap.add_argument("--oracle", default=DEF_ORACLE)
    ap.add_argument("--splits", default=DEF_SPLITS)
    ap.add_argument("--symbols", default=DEF_SYMBOLS)
    ap.add_argument("--map", default=DEF_MAP)
    ap.add_argument("--obj-dir",
                    default=os.path.join(ROOT, "build", "45410914", "src"))
    ap.add_argument("--report", default=DEF_REPORT,
                    help="report.json for the truthful not-yet-matched estimator")
    ap.add_argument("--deferred-out", default=None,
                    help="write case-b eviction worklist JSON here")
    ap.add_argument("--allow-span-coexist", action="store_true",
                    help="when the TU already has a SPAN pin, still apply the "
                         "NON-colliding case-A micro-pin remainder (default: "
                         "fail-closed = emit nothing, the wave-16 demand)")
    ap.add_argument("--apply", action="store_true",
                    help="write splits.txt + map (ADD-ONLY); default dry-run")
    args = ap.parse_args()

    tu = args.tu if args.tu.endswith((".cpp", ".c", ".cc")) else args.tu + ".cpp"
    stem = os.path.splitext(tu)[0]

    # --- 1. oracle records for this TU; drop size==0 (ICF aliases / stubs) ---
    oracle = json.load(open(args.oracle))
    recs = [e for e in oracle if tu_base(e["bindiff_src"]) == tu]
    if not recs:
        print(f"ERROR: no oracle records for {tu}", file=sys.stderr)
        return 2
    bodies = [e for e in recs if e.get("size", 0) > 0]
    n_zero = len(recs) - len(bodies)

    # --- 2. covering-pin index (multi-range aware) -> case discriminator -----
    pins = all_text_ranges(args.splits)
    pstarts = [p[0] for p in pins]

    def covering_pin(addr):
        i = bisect.bisect_right(pstarts, addr) - 1
        if 0 <= i < len(pins) and pins[i][0] <= addr < pins[i][1]:
            return pins[i][2]
        return None

    # --- 2b. FIX-1a: does THIS TU already carry a SPAN pin? ------------------
    # A wide own-range (>= SPAN_PIN_MIN) means dtk carves a whole contiguous
    # cluster into this TU's obj, and the span renamer ALREADY mints the mangled
    # name of every method whose VA sits in that span. This is IN ADDITION to the
    # existing SELF classification (which already skips a VA physically INSIDE the
    # span): the new risk is a case-A VA OUTSIDE the span whose mangled NAME
    # duplicates a span-carved method (ICF alias / overload-arity ambiguity).
    # Carving a second target symbol with that name STEALS pairing from the real
    # span-carved body -- the wave-16 -14 root cause. Fix-1b builds the set of
    # names the span carves and filters any colliding micro-pin out.
    own_ranges = [(lo, hi) for lo, hi, cpp in pins
                  if tu_base(cpp).lower() == tu.lower()]
    span_ranges = [(lo, hi) for lo, hi in own_ranges if (hi - lo) >= SPAN_PIN_MIN]
    has_span_pin = bool(span_ranges)

    # --- 3. non-bisecting boundary index from the binary symbol table --------
    sizes = load_sizes(args.symbols)
    starts = compute_starts(sizes)
    start_set = set(starts)
    # ALL-symbol boundaries (incl except_data/except_record objects interleaved
    # in .text) so the end-snap can't bisect a non-function symbol.
    all_starts, all_spans = load_all_sym_bounds(args.symbols)
    all_span_starts = [s[0] for s in all_spans]

    # max symbol span width (so we know how far back overlapping spans can start)
    _max_span = max((b - a for a, b in all_spans), default=0)

    def bisects_any(end):
        """True if `end` falls STRICTLY inside ANY symbol's [a, a+sz) span.
        Symbol spans can OVERLAP (stale CFA sizing oversizes a fn so it covers a
        following except_data + fn), so we must scan back over every span that
        could still contain `end`, not just the single nearest-preceding one."""
        i = bisect.bisect_right(all_span_starts, end) - 1
        while i >= 0:
            a, b = all_spans[i]
            if a < end < b:
                return True
            if end - a > _max_span:
                break  # no earlier span can reach `end`
            i -= 1
        return False

    # --- 4. classify each method ---------------------------------------------
    case_a, case_b, self_owned, no_size = [], [], [], []
    for e in bodies:
        addr = int(e["rb3_addr"], 16)
        cov = covering_pin(addr)
        if cov is None:
            case_a.append(e)
        elif tu_base(cov).lower() == tu.lower():
            self_owned.append(e)
        else:
            e = dict(e, _foreign=cov)
            case_b.append(e)

    # --- 5. snap CASE-A [VA,VA+size) to fn boundaries; reject bisects --------
    # start must equal a non-bisected fn start; end must equal the NEXT fn start
    # (or VA+size if that lands on a boundary). Mirrors validate_splits c/d.
    ranges = []          # (lo, hi, [addrs])
    rejected_bisect = []
    for e in sorted(case_a, key=lambda x: int(x["rb3_addr"], 16)):
        addr = int(e["rb3_addr"], 16)
        sz = sizes.get(addr, e.get("size", 4))
        end = addr + sz
        # start must be a valid (non-covered) fn boundary
        if addr not in start_set:
            rejected_bisect.append((e, "start not a fn boundary"))
            continue
        # END snap: prefer the tightest non-bisecting boundary >= addr+sz.
        # Try, in order: (1) the exact oracle end if it's a known symbol start,
        # (2) the next ANY-symbol start >= end, (3) the next fn start >= end.
        # Reject if the chosen end still bisects a non-function object.
        # Build ordered end candidates: exact oracle end, then next ANY-symbol
        # start, then next fn start. Accept the FIRST that does not bisect any
        # symbol span (dtk rejects an end strictly inside a symbol -- and stale
        # CFA sizing can make even a symbol START sit inside an oversized
        # neighbor's span, so EVERY candidate is bisect-validated).
        end_set = set(all_starts)
        cands = []
        if end in start_set or end in end_set:
            cands.append(end)
        ai = bisect.bisect_left(all_starts, end)
        if ai < len(all_starts):
            cands.append(all_starts[ai])
        fi = bisect.bisect_left(starts, end)
        if fi < len(starts):
            cands.append(starts[fi])
        snapped_end = None
        for c in sorted(set(cands)):
            if c > addr and not bisects_any(c):
                snapped_end = c
                break
        if snapped_end is None:
            rejected_bisect.append((e, "no clean end boundary (stale sizing)"))
            continue
        ranges.append([addr, snapped_end, [addr]])

    # --- 5b. coalesce contiguous / overlapping ranges into single .text lines -
    ranges.sort()
    coalesced = []
    for lo, hi, addrs in ranges:
        if coalesced and lo <= coalesced[-1][1]:
            coalesced[-1][1] = max(coalesced[-1][1], hi)
            coalesced[-1][2].extend(addrs)
        else:
            coalesced.append([lo, hi, list(addrs)])

    # --- 6. overlap guard (defense over validate_splits) ---------------------
    # any proposed range must not overlap ANY existing pin (incl this TU's own).
    overlaps = []
    clean = []
    for lo, hi, addrs in coalesced:
        bad = [(plo, phi, pn) for plo, phi, pn in pins
               if not (phi <= lo or plo >= hi)]
        if bad:
            overlaps.append((lo, hi, bad))
        else:
            clean.append((lo, hi, addrs))

    # --- 7. naming via gen_game_target_map (oracle -> compiled mangled) ------
    # Scope it to this TU only and the carved VAs; reuse its COFF decode so the
    # map name is derived from the COMPILED obj's DEFINED symbols (correct ABI).
    from gen_game_target_map import (find_obj, build_tu_entries)  # noqa: E402
    obj = find_obj(__import__("pathlib").Path(args.obj_dir), tu)
    map_entries = {}
    name_stats = None
    # all_entries = the full oracle-VA -> mangled map for THIS TU (every in-TU
    # body, not just carved ones). Used both to name the carved VAs AND, in
    # Fix-1b, to learn which mangled names the existing SPAN pins already carve.
    all_entries = {}
    carved_addrs = {a for _, _, addrs in clean for a in addrs}
    if obj is not None and obj.exists():
        # span scoping: pass None so build_tu_entries considers all in-TU oracle
        # rows, then filter to the carved VAs (the ones we actually pinned).
        all_entries, name_stats = build_tu_entries(tu, oracle, obj, None, False)
        for k, v in all_entries.items():
            if int(k, 16) in carved_addrs:
                map_entries[k] = v

    # --- 7b. FIX-1b: NAME-collision filter (wave-16 -14 regression guard) -----
    # The danger is NOT address overlap (step 6 + validate_splits cover that):
    # it is a case-A micro-pin whose mangled NAME duplicates a name already
    # minted by (i) a SPAN pin carving this TU's contiguous cluster, or (ii) an
    # EARLIER micro-pin in this same batch (two oracle VAs -> one mangled name
    # via ICF alias / overload-arity ambiguity). A duplicate target symbol
    # STEALS pairing from the real body EVEN WITH NO new map entry, because the
    # obj-level renamer still produces the mangled symbol for the carved range --
    # so we must drop the whole RANGE, not just the map key.
    #
    # span_names = the value-set build_tu_entries assigns to VAs that fall inside
    # an existing wide span range (filtering all_entries by VA is the per-range
    # scoping the design calls for, without re-decoding the obj per range).
    span_names = set()
    for lo, hi in span_ranges:
        for k, v in all_entries.items():
            if lo <= int(k, 16) < hi:
                span_names.add(v)

    collided_vas = set()       # carved VAs whose mangled name collides
    seen_names = set(span_names)
    for lo, hi, addrs in clean:
        for a in sorted(addrs):
            v = map_entries.get(f"0x{a:08X}")
            if v is None:
                continue  # unnameable carve: no mangled symbol minted, no clash
            if v in seen_names:
                collided_vas.add(a)  # collides w/ span OR an earlier micro-pin
            else:
                seen_names.add(v)    # first claim of this name wins

    # final_clean EXCLUDES any range that carries ANY collided VA (the range's
    # carved fragment still mints the duplicate symbol even without a map entry).
    final_clean = [(lo, hi, addrs) for lo, hi, addrs in clean
                   if not any(a in collided_vas for a in addrs)]

    # HARD GATE: a span-pinned TU is FAIL-CLOSED unless --allow-span-coexist.
    # The wave-16 demand: do not silently append micro-pins to a span-pinned TU.
    gate_tripped = False
    if has_span_pin and not args.allow_span_coexist:
        gate_tripped = True
        final_clean = []

    # re-scope map_entries to the FINAL (filtered) carved set so we never emit a
    # key for a dropped/colliding range.
    final_carved = {a for _, _, addrs in final_clean for a in addrs}
    map_entries = {k: v for k, v in map_entries.items()
                   if int(k, 16) in final_carved}

    # ---- report -------------------------------------------------------------
    print("=" * 72)
    print(f"identity_transfer  TU={tu}")
    print("=" * 72)
    print(f"oracle rows                : {len(recs)}")
    print(f"  size==0 (ICF/alias, drop): {n_zero}")
    print(f"  bodies (size>0)          : {len(bodies)}")
    print(f"classification:")
    print(f"  CASE-A unowned-blob      : {len(case_a)}  <- micro-pin candidates")
    print(f"  SELF (already in own pin): {len(self_owned)}  (reveal_sweep territory, skip)")
    print(f"  CASE-B foreign-pinned    : {len(case_b)}  (eviction-gated, SKIP -> deferred)")
    if rejected_bisect:
        print(f"  rejected (bisect/no-bdry): {len(rejected_bisect)}")
    print(f"proposed .text ranges      : {len(clean)} "
          f"(coalesced from {len(case_a) - len(rejected_bisect)} carvable fns)")
    if overlaps:
        print(f"  !! OVERLAP-GUARD tripped : {len(overlaps)} range(s) dropped")
        for lo, hi, bad in overlaps:
            print(f"     0x{lo:08X}..0x{hi:08X} overlaps {bad[0][2]}")
    # --- FIX-1 collision-safety summary --------------------------------------
    if has_span_pin:
        spans_desc = ", ".join(f"0x{lo:08X}..0x{hi:08X}" for lo, hi in span_ranges)
        print(f"  SPAN-PIN present         : {spans_desc} "
              f"({len(span_names)} names already span-carved)")
    if collided_vas:
        print(f"  !! NAME-COLLISION filter : {len(collided_vas)} carved VA(s) "
              f"drop their range (duplicate mangled name steals pairing)")
    print(f"FINAL clean ranges         : {len(final_clean)} "
          f"(after collision filter; was {len(clean)})")
    if gate_tripped:
        spans_desc = ", ".join(f"0x{lo:08X}..0x{hi:08X}" for lo, hi in span_ranges)
        print("  " + "!" * 64)
        print(f"  !! HARD GATE: TU has span pin {spans_desc}; "
              f"{len(clean)} case-A micro-pins filtered, "
              f"{len(collided_vas)} name-collisions.")
        print(f"  !! pass --allow-span-coexist to apply the non-colliding "
              f"remainder. Fail-closed (emit nothing).")
        print("  " + "!" * 64)
    print(f"map entries (named bodies, incl already-matched): {len(map_entries)}  "
          f"of {len(final_carved)} final carved VAs")
    if name_stats:
        print(f"  (gen_game_target_map: paired={name_stats['matched']} "
              f"noObj={name_stats['no_obj_symbol']} ambig={name_stats['ambiguous']})")

    # --- FIX 2: TRUTHFUL not-yet-matched estimator ---------------------------
    _truthful_estimate(args, case_a, sizes, all_entries, final_carved)

    print()
    print("proposed micro-ranges:")
    for lo, hi, addrs in final_clean:
        named = sum(1 for a in addrs if f"0x{a:08X}" in map_entries)
        print(f"  .text start:0x{lo:08X} end:0x{hi:08X}  "
              f"({len(addrs)} fn, {named} named, {hi-lo}B)")

    # case-b worklist
    deferred = [
        {"rb3_addr": e["rb3_addr"], "size": e.get("size", 0),
         "wii_name": e["wii_name"], "foreign_pin": e["_foreign"]}
        for e in case_b
    ]
    if args.deferred_out:
        json.dump(deferred, open(args.deferred_out, "w"), indent=2)
        print(f"\nwrote case-b worklist: {args.deferred_out} ({len(deferred)})")

    if not args.apply:
        print("\n(dry-run; --apply to write splits.txt + map ADD-ONLY)")
        return 0

    # FIX-1: apply ONLY final_clean (collision-filtered + span-gated). A tripped
    # hard gate or a fully-filtered batch leaves final_clean empty -> nothing to
    # write (fail-closed). clean!=[] is no longer sufficient to apply.
    if not final_clean:
        if gate_tripped:
            print("\nnothing to apply: span-pin HARD GATE tripped (pass "
                  "--allow-span-coexist to apply the non-colliding remainder).",
                  file=sys.stderr)
        else:
            print("\nnothing to apply (no clean CASE-A range after collision "
                  "filter).", file=sys.stderr)
        return 1

    # ---- APPLY: append .text micro-ranges under the TU header ---------------
    # (the genuinely new code: pin_identified rewrites only the first range; we
    #  APPEND additional same-unit .text lines, which jeff accumulates raw.)
    lines, units = parse_splits(args.splits)
    # find the TU header; if absent, create one at end-of-file.
    hdr_idx = None
    indent = "\t"
    for i, line in enumerate(lines):
        m = UNIT_RE.match(line)
        if m and tu_base(m.group(1)).lower() == tu.lower():
            hdr_idx = i
            break
    new_lines = [f"{indent}.text       start:0x{lo:08X} end:0x{hi:08X}\n"
                 for lo, hi, _ in final_clean]
    if hdr_idx is None:
        # create a fresh header block at EOF
        if lines and not lines[-1].endswith("\n"):
            lines[-1] += "\n"
        lines.append(f"{tu}:\n")
        lines.extend(new_lines)
        lines.append("\n")
    else:
        # insert after the LAST .text line already under this header
        j = hdr_idx + 1
        last_text = hdr_idx
        while j < len(lines) and not UNIT_RE.match(lines[j]):
            if TEXT_RE.match(lines[j]):
                last_text = j
            j += 1
        ins = last_text + 1
        lines[ins:ins] = new_lines
    open(args.splits, "w").writelines(lines)
    print(f"\n[APPLIED] {args.splits}: +{len(new_lines)} .text micro-range(s) "
          f"under {tu}")

    # ---- APPLY: STRICT ADD-ONLY merge into the map --------------------------
    # NEVER overwrite an existing key (an existing entry may be the ICF-survivor
    # name a prior wave proved at 100%; clobbering it would regress that unit).
    # NEVER re-sort (the on-disk file's key order is load-bearing for a clean
    # diff; a full re-sort looks like the 'wholesale-regen POISON' the design
    # forbids). We append only the genuinely-new keys, preserving order.
    existing = json.load(open(args.map)) if os.path.isfile(args.map) else {}
    before = len(existing)
    added = skipped_present = collided = 0
    collisions = []
    for k, v in map_entries.items():
        if k in existing:
            if existing[k] != v:
                collided += 1
                collisions.append((k, existing[k], v))  # ICF-alias collision
            else:
                skipped_present += 1
            continue  # ADD-ONLY: do not touch existing keys
        existing[k] = v
        added += 1
    # Preserve the on-disk format (1-space indent, insertion order, no
    # sort_keys) so the diff is a clean +N append, not a whole-file rewrite
    # that resembles the forbidden 'wholesale-regen POISON'.
    with open(args.map, "w") as fh:
        json.dump(existing, fh, indent=1)
        fh.write("\n")
    print(f"[APPLIED] {args.map}: {before} -> {len(existing)} "
          f"(+{added} new; {skipped_present} already-present, "
          f"{collided} ICF-collision KEPT-EXISTING) [STRICT ADD-ONLY]")
    if collisions:
        print("  ICF-alias collisions (existing name kept, RockCentral name "
              "NOT applied -- these VAs are shared ICF survivors):")
        for k, old, new in collisions[:10]:
            print(f"    {k}: kept {old.split('@')[0]}  (skipped "
                  f"{new.split('@')[0]})")
    print("\nnext: rm -f build/45410914/target_symbol_renames.stamp && "
          "touch config/45410914/config.yml && tools/fresh_report.sh")
    return 0


if __name__ == "__main__":
    sys.exit(main())
