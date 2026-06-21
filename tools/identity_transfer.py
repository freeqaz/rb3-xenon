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
    ap.add_argument("--deferred-out", default=None,
                    help="write case-b eviction worklist JSON here")
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
    carved_addrs = {a for _, _, addrs in clean for a in addrs}
    if obj is not None and obj.exists():
        # span scoping: pass None so build_tu_entries considers all in-TU oracle
        # rows, then filter to the carved VAs (the ones we actually pinned).
        entries, name_stats = build_tu_entries(tu, oracle, obj, None, False)
        for k, v in entries.items():
            if int(k, 16) in carved_addrs:
                map_entries[k] = v

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
    print(f"map entries (named bodies) : {len(map_entries)}  "
          f"of {len(carved_addrs)} carved VAs")
    if name_stats:
        print(f"  (gen_game_target_map: paired={name_stats['matched']} "
              f"noObj={name_stats['no_obj_symbol']} ambig={name_stats['ambiguous']})")
    print()
    print("proposed micro-ranges:")
    for lo, hi, addrs in clean:
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

    if not clean:
        print("\nnothing to apply (no clean CASE-A range).", file=sys.stderr)
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
                 for lo, hi, _ in clean]
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
