#!/usr/bin/env python3
"""Relocate / pin GAME-code TU splits onto their real bodies, from cross-binary
COFF content-match identifications (tools/game_content_match.py ->
game_content_match.json).

This is a fork of tools/relocate_engine_splits.py for the GAME layer
(band3/, network/, hamobj groups in objects.json). The engine tool only handles
under-pinned engine stubs; the game layer adds two complications the planner
spelled out, so we split units into THREE classes:

  * NOPIN  -- the .cpp has NO .text line in splits.txt at all. We SYNTHESize a
              fresh unit block at the clipped dominant cluster. The unit must
              already exist in objects.json under band3/network/hamobj (it does,
              since we compiled it); if not, we skip it. Pinning into UNPINNED
              space cannot regress (unpinned == unmeasured).
  * STUB   -- the current .text span is < --stub-max (junk pin). Relocate exactly
              like the engine tool: stub == unmeasured noise, so this is safe.
  * non-stub -- a real pin with measured functions. Relocating would ABANDON the
              currently-matched functions, so we relocate ONLY IF the current pin
              holds ZERO functions already at 100% (computed from report.json's
              match_percent_normalized for the unit). Otherwise we SKIP and log it
              as deferred (the planner found ~18 such: TourDescPanel, NetGameMsgs,
              QuestFilterPanel, RockCentral, ...). We also NEVER relocate a unit
              whose only pin is keyed by a basename that is SHARED with a
              different TU (e.g. band3/meta_band/Utl.cpp shares the bare key
              `Utl.cpp` with the engine system/ui/Utl.cpp) -- moving that pin
              would silently move the OTHER unit.

All engine SAFETY is kept verbatim:
  * dominant_cluster (contiguity gap 0x8000),
  * neighbour-pin clip,
  * non-bisecting function-boundary snap (symbols.txt has overlapping symbols and
    dtk uses pdata-authoritative length; an edge that bisects a function fails
    "Split ... ends within symbol"),
  * fail-closed overlap guard (never overlap another unit's pin),
  * never overwrite an existing target_symbol_map name.

Differences from the engine tool:
  (a) reads game_content_match.json (fields mangled_name / unit),
  (b) wired groups = band3, network, hamobj (from objects.json),
  (c) name_of[addr] = the mangled_name straight from the match (no DC3 lookup),
  (d) the three unit classes above, incl. NOPIN block synthesis.

  tools/relocate_game_splits.py                 # dry-run report
  tools/relocate_game_splits.py --apply         # rewrite splits.txt + tsm (+objects.json if needed)
  tools/relocate_game_splits.py --apply --splits <wt>/config/.../splits.txt --tsm ... --symbols ... --objects ...
"""
import argparse
import bisect
import json
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEF_MATCH = os.path.join(ROOT, "game_content_match.json")
DEF_SPLITS = os.path.join(ROOT, "config", "45410914", "splits.txt")
DEF_SYMBOLS = os.path.join(ROOT, "config", "45410914", "symbols.txt")
DEF_OBJECTS = os.path.join(ROOT, "config", "45410914", "objects.json")
DEF_TSM = os.path.join(ROOT, "scripts", "target_symbol_map.json")
DEF_REPORT = os.path.join(ROOT, "build", "45410914", "report.json")

WIRED_GROUPS = ("band3", "network", "hamobj")

UNIT_RE = re.compile(r"^(\S+\.(?:cpp|c|cc)):\s*$")
TEXT_RE = re.compile(r"^(\s*)\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)")


def load_sizes(path):
    sizes = {}
    rx = re.compile(r"fn_([0-9A-Fa-f]+) = \.text:0x[0-9A-Fa-f]+; // type:function size:0x([0-9A-Fa-f]+)")
    for line in open(path):
        m = rx.match(line)
        if m:
            sizes[int(m.group(1), 16)] = int(m.group(2), 16)
    return sizes


def parse_splits(path):
    """Return (lines, units) where units key on the EXACT splits.txt key string
    (which may be a bare basename or a full path)."""
    lines = open(path).read().splitlines(keepends=True)
    units = {}
    order = []
    cur = None
    for i, line in enumerate(lines):
        m = UNIT_RE.match(line)
        if m:
            cur = m.group(1)
            if cur not in units:
                units[cur] = {"name": cur, "text_line": None, "first_line": i}
                order.append(cur)
            continue
        if cur:
            t = TEXT_RE.match(line)
            if t and units[cur]["text_line"] is None:
                units[cur].update(text_line=i, indent=t.group(1),
                                  lo=int(t.group(2), 16), hi=int(t.group(3), 16))
    return lines, units, order


def wired_game(objects_path):
    """basename.lower() collision count + full-path -> group, for the wired groups."""
    o = json.load(open(objects_path))
    full2grp = {}
    base_count = {}
    for grp in WIRED_GROUPS:
        for fn in o.get(grp, {}).get("objects", {}):
            full2grp[fn] = grp
            base_count[fn.split("/")[-1].lower()] = base_count.get(fn.split("/")[-1].lower(), 0) + 1
    return full2grp, base_count, o


def all_basename_owners(objects_path):
    """basename.lower() -> count of objects.json TUs (any group) that share it.
    Used to detect ambiguous bare keys (e.g. Utl.cpp in both system/ui and band3)."""
    o = json.load(open(objects_path))
    cnt = {}
    for grp in o.values():
        if isinstance(grp, dict):
            for fn in grp.get("objects", {}):
                b = fn.split("/")[-1].lower()
                cnt[b] = cnt.get(b, 0) + 1
    return cnt


def dominant_cluster(addrs, sizes, gap):
    addrs = sorted(addrs)
    runs, s = [], 0
    for k in range(1, len(addrs)):
        if addrs[k] - addrs[k - 1] > gap:
            runs.append((s, k - 1)); s = k
    runs.append((s, len(addrs) - 1))
    bi, bj = max(runs, key=lambda r: r[1] - r[0] + 1)
    cl = addrs[bi:bj + 1]
    lo = cl[0]
    hi = cl[-1] + sizes.get(cl[-1], 4)
    return lo, hi, cl, len(cl) / len(addrs)


def report_at100(report_path, splitkey):
    """Count functions at >=100% in the report unit for this splits.txt key.
    Report unit name == 'default/' + splitkey-without-extension (path preserved).
    Returns (at100, total) or (None, None) if the unit is not in the report."""
    if not os.path.exists(report_path):
        return None, None
    if not hasattr(report_at100, "_cache"):
        r = json.load(open(report_path))
        report_at100._cache = {u["name"]: u for u in r["units"]}
    ru = report_at100._cache
    rk = "default/" + splitkey.rsplit(".", 1)[0]
    u = ru.get(rk)
    if not u:
        return None, None
    fns = u.get("functions", [])
    a = sum(1 for f in fns if f.get("match_percent_normalized", 0) >= 100.0)
    return a, len(fns)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--match", default=DEF_MATCH)
    ap.add_argument("--splits", default=DEF_SPLITS)
    ap.add_argument("--symbols", default=DEF_SYMBOLS)
    ap.add_argument("--objects", default=DEF_OBJECTS)
    ap.add_argument("--tsm", default=DEF_TSM)
    ap.add_argument("--report", default=DEF_REPORT)
    ap.add_argument("--stub-max", type=lambda x: int(x, 0), default=0x400,
                    help="units whose current .text range is smaller than this "
                         "are STUBs (relocated freely); default 0x400")
    ap.add_argument("--min-cluster", type=int, default=4,
                    help="min content-matches in the dominant cluster for "
                         "STUB/non-stub relocation")
    ap.add_argument("--min-cluster-nopin", type=int, default=2,
                    help="min content-matches in the dominant cluster for NOPIN "
                         "synthesis (lower: pinning unpinned space cannot regress)")
    ap.add_argument("--min-frac", type=float, default=0.5,
                    help="dominant cluster must hold >= this fraction of the "
                         "unit's content-matches")
    ap.add_argument("--cluster-gap", type=lambda x: int(x, 0), default=0x8000)
    ap.add_argument("--only", default="")
    ap.add_argument("--no-nopin", action="store_true", help="skip NOPIN synthesis")
    ap.add_argument("--no-stub", action="store_true", help="skip STUB relocation")
    ap.add_argument("--no-nonstub", action="store_true", help="skip non-stub relocation")
    ap.add_argument("--apply", action="store_true")
    args = ap.parse_args()

    only = set(s.strip() for s in args.only.split(",") if s.strip())
    matches = json.load(open(args.match))
    sizes = load_sizes(args.symbols)
    lines, units, order = parse_splits(args.splits)
    full2grp, _basecnt, objects = wired_game(args.objects)
    base_owners = all_basename_owners(args.objects)
    tsm = json.load(open(args.tsm))

    # Valid split boundaries: a function start that is NOT covered by an earlier,
    # larger overlapping function (symbols.txt has overlapping symbols; dtk fails
    # "Split ... ends within symbol" if a range edge bisects a function).
    starts = []
    running_end = 0
    for a in sorted(sizes.keys()):
        if a >= running_end:
            starts.append(a)
        running_end = max(running_end, a + sizes[a])

    # content matches per GAME cpp (full path) + name_of straight from the match
    bycpp = {}
    name_of = {}
    for e in matches:
        cpp = e["unit"]
        if cpp not in full2grp:
            continue  # not a wired game TU -> skip
        a = int(e["rb3_addr"], 16)
        bycpp.setdefault(cpp, []).append(a)
        name_of[a] = e["mangled_name"]

    # All existing pins (obstacles for the overlap guard / clip). Keyed by the
    # EXACT splits key string (so a unit never clips against itself).
    pins = [(u["lo"], u["hi"], k) for k, u in units.items() if u.get("lo") is not None]
    pins.sort()

    def owning_key(cpp):
        """The splits.txt key (if any) that pins this game TU. Prefer the full
        path; fall back to the bare basename ONLY if that basename is unambiguous
        (owned by exactly one objects.json TU). Returns (key, ambiguous_skip)."""
        base = cpp.split("/")[-1]
        if cpp in units and units[cpp].get("lo") is not None:
            return cpp, False
        if base in units and units[base].get("lo") is not None:
            # bare-key pin exists; is the basename shared with another TU?
            if base_owners.get(base.lower(), 0) > 1:
                return base, True   # AMBIGUOUS -> caller must skip relocation
            return base, False
        return None, False

    def clip_and_snap(desired_lo, desired_hi, selfkey):
        """neighbour-pin clip + non-bisecting snap + re-clip. Mirrors engine."""
        left, right = 0, 1 << 32
        for plo, phi, pn in pins:
            if pn == selfkey:
                continue
            if phi <= desired_lo:
                left = max(left, phi)
            elif plo >= desired_hi:
                right = min(right, plo)
            else:
                if plo >= desired_lo:
                    right = min(right, plo)
                else:
                    left = max(left, phi)
        nlo, nhi = max(desired_lo, left), min(desired_hi, right)
        li = bisect.bisect_right(starts, nlo) - 1
        if li >= 0:
            nlo = starts[li]
        hi_i = bisect.bisect_left(starts, nhi)
        nhi = starts[hi_i] if hi_i < len(starts) else nhi
        nlo = max(nlo, left)
        nhi = min(nhi, right)
        return nlo, nhi

    def overlaps_pin(nlo, nhi, selfkey):
        return any(pn != selfkey and not (phi <= nlo or plo >= nhi)
                   for plo, phi, pn in pins)

    nopin_props = []   # (n, cpp, group, nlo, nhi, kept)
    stub_props = []    # (n, cpp, key, olo, ohi, nlo, nhi, kept)
    relo_props = []    # (n, cpp, key, olo, ohi, nlo, nhi, kept)
    deferred = []      # (cpp, reason, detail)

    for cpp in sorted(bycpp):
        if only and cpp not in only and cpp.split("/")[-1] not in only:
            continue
        addrs = bycpp[cpp]
        key, ambiguous = owning_key(cpp)

        # ---- NOPIN: synthesize a new unit block --------------------------------
        if key is None:
            if args.no_nopin:
                continue
            lo, hi, cl, frac = dominant_cluster(addrs, sizes, args.cluster_gap)
            if len(cl) < args.min_cluster_nopin or frac < args.min_frac:
                deferred.append((cpp, "nopin-weak-cluster",
                                 f"{len(cl)}/{len(addrs)} frac={frac:.2f}"))
                continue
            nlo, nhi = clip_and_snap(lo, hi, cpp)
            if nhi <= nlo:
                deferred.append((cpp, "nopin-empty-after-clip", ""))
                continue
            if overlaps_pin(nlo, nhi, cpp):
                deferred.append((cpp, "nopin-guard-overlap", ""))
                continue
            kept = [a for a in cl if nlo <= a < nhi]
            if not kept:
                deferred.append((cpp, "nopin-no-fns-in-range", ""))
                continue
            # register this synthesized pin so later units clip against it
            pins.append((nlo, nhi, cpp)); pins.sort()
            nopin_props.append((len(kept), cpp, full2grp[cpp], nlo, nhi, kept))
            continue

        if ambiguous:
            deferred.append((cpp, "ambiguous-bare-key",
                             f"key={key} shared by {base_owners.get(key.lower())} TUs"))
            continue

        u = units[key]
        cur_lo, cur_hi = u["lo"], u["hi"]
        span = cur_hi - cur_lo
        is_stub = span < args.stub_max

        lo, hi, cl, frac = dominant_cluster(addrs, sizes, args.cluster_gap)
        if len(cl) < args.min_cluster or frac < args.min_frac:
            deferred.append((cpp, "weak-cluster",
                             f"{len(cl)}/{len(addrs)} frac={frac:.2f} key={key}"))
            continue

        if is_stub:
            if args.no_stub:
                continue
        else:
            if args.no_nonstub:
                continue
            # non-stub: relocate ONLY if the current pin has ZERO at-100 functions
            at100, total = report_at100(args.report, key)
            if at100 is None:
                deferred.append((cpp, "nonstub-no-report",
                                 f"key={key} (no report unit -> cannot prove safe)"))
                continue
            if at100 > 0:
                deferred.append((cpp, "nonstub-has-100",
                                 f"at100={at100}/{total} key={key}"))
                continue

        nlo, nhi = clip_and_snap(lo, hi, key)
        if nhi <= nlo:
            deferred.append((cpp, "empty-after-clip", f"key={key}"))
            continue
        if overlaps_pin(nlo, nhi, key):
            deferred.append((cpp, "guard-overlap", f"key={key}"))
            continue
        # newly-covered content-matches (outside the old pinned range)
        kept = [a for a in cl if nlo <= a < nhi and not (cur_lo <= a < cur_hi)]
        if not kept:
            deferred.append((cpp, "no-new-fns", f"key={key}"))
            continue
        # the relocated pin replaces the old one in the obstacle set
        for i, (plo, phi, pn) in enumerate(pins):
            if pn == key:
                pins[i] = (nlo, nhi, key); break
        pins.sort()
        rec = (len(kept), cpp, key, cur_lo, cur_hi, nlo, nhi, kept)
        (stub_props if is_stub else relo_props).append(rec)

    # ---- report ------------------------------------------------------------
    nopin_props.sort(reverse=True)
    stub_props.sort(reverse=True)
    relo_props.sort(reverse=True)
    tot = (sum(p[0] for p in nopin_props) + sum(p[0] for p in stub_props)
           + sum(p[0] for p in relo_props))
    print(f"NOPIN synth      : {len(nopin_props):3d} units, "
          f"{sum(p[0] for p in nopin_props):4d} fns")
    print(f"STUB relocate    : {len(stub_props):3d} units, "
          f"{sum(p[0] for p in stub_props):4d} fns")
    print(f"non-stub reloc   : {len(relo_props):3d} units, "
          f"{sum(p[0] for p in relo_props):4d} fns")
    print(f"TOTAL newly pinned+named: {tot} fns\n")

    print("== NOPIN (synthesize) ==")
    for n, cpp, grp, nlo, nhi, kept in nopin_props:
        print(f"  +{n:3d}  [{grp}] 0x{nlo:08X}..0x{nhi:08X} ({nhi-nlo}B)  {cpp}")
    print("== STUB (relocate) ==")
    for n, cpp, key, olo, ohi, nlo, nhi, kept in stub_props:
        print(f"  +{n:3d}  0x{olo:08X}..0x{ohi:08X} -> 0x{nlo:08X}..0x{nhi:08X}  {cpp}")
    print("== non-stub (relocate, 0 at-100) ==")
    for n, cpp, key, olo, ohi, nlo, nhi, kept in relo_props:
        print(f"  +{n:3d}  0x{olo:08X}..0x{ohi:08X} -> 0x{nlo:08X}..0x{nhi:08X}  {cpp}")
    print("== DEFERRED ==")
    for cpp, reason, detail in sorted(deferred):
        print(f"  SKIP {cpp:48s} {reason:22s} {detail}")

    if not args.apply:
        print("\n(dry-run; --apply to write splits.txt + target_symbol_map.json"
              " [+ objects.json if a NOPIN unit is undeclared])")
        return

    # ---- apply -------------------------------------------------------------
    existing = set(k.lower() for k in tsm if k.lower().startswith("0x"))
    added = 0

    def add_names(kept):
        nonlocal added
        for a in kept:
            keyk = "0x%08X" % a
            if keyk.lower() in existing:
                continue  # never overwrite a verified name (ICF aliases etc.)
            tsm[keyk] = name_of[a]
            existing.add(keyk.lower())
            added += 1

    # 1) rewrite existing .text lines for STUB + non-stub relocations
    for n, cpp, key, olo, ohi, nlo, nhi, kept in (stub_props + relo_props):
        u = units[key]
        lines[u["text_line"]] = (f'{u["indent"]}.text       '
                                 f'start:0x{nlo:08X} end:0x{nhi:08X}\n')
        add_names(kept)

    # 2) synthesize NOPIN unit blocks (append at end; .text-only, dtk back-fills
    #    .pdata on the next split). objects.json already declares them (we
    #    skipped any that weren't wired), so no objects.json change is required;
    #    we double-check and only append objects.json entries if truly missing.
    obj_changed = False
    if nopin_props:
        # ensure trailing newline before appending blocks
        if lines and not lines[-1].endswith("\n"):
            lines[-1] += "\n"
        block = ["\n"]
        for n, cpp, grp, nlo, nhi, kept in nopin_props:
            block.append(f"{cpp}:\n")
            block.append(f"\t.text       start:0x{nlo:08X} end:0x{nhi:08X}\n")
            add_names(kept)
            # safety: the unit MUST be in objects.json under a wired group already
            if cpp not in full2grp:
                grp_objs = objects.setdefault(grp, {}).setdefault("objects", {})
                if cpp not in grp_objs:
                    grp_objs[cpp] = "NonMatching"
                    obj_changed = True
        lines.extend(block)

    print(f"\ntarget_symbol_map: +{added} new names (existing preserved)")
    open(args.splits, "w").writelines(lines)
    json.dump(tsm, open(args.tsm, "w"), indent=1)
    if obj_changed:
        json.dump(objects, open(args.objects, "w"), indent=2)
        print(f"objects.json: appended undeclared NOPIN units")
    print(f"wrote {args.splits} and {args.tsm}"
          + (f" and {args.objects}" if obj_changed else ""))
    print("next: rm -f build/45410914/target_symbol_renames.stamp && "
          "touch config/45410914/config.yml && ./tools/ninja-locked")


if __name__ == "__main__":
    main()
