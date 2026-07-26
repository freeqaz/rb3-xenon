#!/usr/bin/env python3
"""localstatic_symbol_inbody_scan.py -- scanner for the IN-BODY local-static-Symbol vein.

Complementary to `localstatic_symbol_scan.py` (which finds small still-ANONYMOUS
standalone accessors).  This one finds LARGE, already-MAPPED, still-near-miss
functions whose retail body constructs one or more *function-local*
`static Symbol foo("literal")` objects that our ported source replaced with a
global `Symbols*.h` Symbol.

WHY IT PAYS (lane R, 2026-07-26: 3/3 flips + 1 large partial in one session)
---------------------------------------------------------------------------
rb3-Wii's dev tree and our port reference globals such as `play_preview`,
`sign_out`, `marquee_rotation_ms`, `mod_auto_vocals`, `send_solo_start_msg`.
Retail X360 instead declares them function-locally.  Consequences in objdiff:

  * `??0Symbol@@QAA@PBD@Z` call count: target > base
  * an ~11-instruction target-only delete cluster per missing static
    (guard load / bit test / bne / ori / stw / lis+addi literal / mr / bl ctor)
  * EVERY downstream static-guard `ori`/`rlwinm.` immediate in the function
    shifted left by one bit per missing static (the guard bits are allocated
    in declaration order, so the bit order NAMES the declaration order)
  * frame -0x10 and 2 extra `__savegprlr_N` registers on the target side

Fix: declare `static Symbol <name>("<literal>");` at the right point (guard-bit
order gives the order; the literal is read straight out of the PE at the
`lis/addi` pair that feeds the ctor).  A static that is constructed but never
referenced still has to be declared -- MSVC keeps it (SelectDifficultyPanel's
`set_list_title` is exactly that).

DETECTION (target side only, no build required beyond the dtk split)
--------------------------------------------------------------------
In `build/45410914/asm/*.s`, inside each `.fn fn_<VA>` block, count call sites
to `??0Symbol@@QAA@PBD@Z` that are preceded (within 8 instructions) by a
static-guard set (`ori rX, rX, 0x<bit>` + a `stw`).  That pair is the signature
of a function-local static's one-time init.  Emit every such function that the
symbol map names and that report.json still scores below 100%.

CAVEAT -- read the count as a *floor, not a defect*.  `BEGIN_HANDLERS` /
`SYNC_PROP` macros legitimately expand to many local-static Symbols, so
`Handle`/`SyncProperty`/`Type` bodies show high counts while our source already
has them.  The real signal is target-count > base-count, which only
`run_objdiff` can confirm ("Count differs: ??0Symbol@@QAA@PBD@Z: target N,
base M").  Use `--exclude-macro-bodies` to drop the macro families, then
objdiff the survivors.

Read-only.

Usage:
    python3 scripts/harvest/localstatic_symbol_inbody_scan.py
    python3 scripts/harvest/localstatic_symbol_inbody_scan.py --exclude-macro-bodies \
        --min-pct 70 --max-pct 98
"""

import argparse
import glob
import json
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MAP = os.path.join(REPO, "scripts", "target_symbol_map.json")
REPORT = os.path.join(REPO, "build", "45410914", "report.json")
ASM = os.path.join(REPO, "build", "45410914", "asm", "*.s")

SYMBOL_CTOR_MANGLED = "??0Symbol@@QAA@PBD@Z"
MACRO_BODIES = ("SyncProperty", "?Handle@", "?Type@", "PropSync")

FN_HDR = re.compile(r"^\.fn (fn_[0-9A-Fa-f]+),")
ORI_GUARD = re.compile(r"\bori\s+r\d+, r\d+, 0x[0-9a-fA-F]+")


def _names(v):
    return v if isinstance(v, list) else [v]


def find_symbol_ctor_va(symmap):
    for va, val in symmap.items():
        if SYMBOL_CTOR_MANGLED in _names(val):
            return "fn_" + va[2:].lower()
    return None


def scan(min_pct, max_pct, exclude_macro_bodies):
    symmap = json.load(open(MAP))
    va2name = {k.lower(): _names(v) for k, v in symmap.items()}
    ctor = find_symbol_ctor_va(symmap)
    if ctor is None:
        sys.exit("could not locate %s in target_symbol_map.json" % SYMBOL_CTOR_MANGLED)

    pct = {}
    report = json.load(open(REPORT))
    for unit in report["units"]:
        for fn in unit.get("functions", []):
            p = fn.get("match_percent_normalized")
            if p is not None:
                pct.setdefault(fn["name"], (p, unit["name"]))

    seen, rows = set(), []
    for path in sorted(glob.glob(ASM)):
        cur, body = None, []
        for line in open(path, errors="ignore"):
            line = line.rstrip("\n")
            hdr = FN_HDR.match(line)
            if hdr:
                cur, body = hdr.group(1), []
                continue
            if line.startswith(".endfn"):
                if cur:
                    n = 0
                    for i, ins in enumerate(body):
                        if ("bl " + ctor) in ins.lower():
                            win = body[max(0, i - 8):i]
                            if any(ORI_GUARD.search(w) for w in win) and any(
                                "\tstw " in w for w in win
                            ):
                                n += 1
                    if n:
                        va = "0x" + cur[3:].lower()
                        for name in va2name.get(va, []):
                            hit = pct.get(name)
                            if not hit or not (min_pct <= hit[0] < max_pct):
                                continue
                            if exclude_macro_bodies and any(
                                k in name for k in MACRO_BODIES
                            ):
                                continue
                            if name in seen:
                                continue
                            seen.add(name)
                            rows.append((hit[0], hit[1], name, n, os.path.basename(path)))
                cur, body = None, []
                continue
            if cur is not None:
                body.append(line)

    rows.sort(reverse=True)
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--min-pct", type=float, default=0.0)
    ap.add_argument("--max-pct", type=float, default=100.0)
    ap.add_argument("--exclude-macro-bodies", action="store_true")
    ap.add_argument("--json", default=None, help="also write rows as JSON here")
    args = ap.parse_args()

    rows = scan(args.min_pct, args.max_pct, args.exclude_macro_bodies)
    print("# match% | unit | symbol | local-static-Symbol inits in TARGET | asm")
    for r in rows:
        print("%7.2f | %s | %s | %d | %s" % r)
    print("# total %d" % len(rows))
    if args.json:
        with open(args.json, "w") as fh:
            json.dump(
                [
                    {"pct": r[0], "unit": r[1], "symbol": r[2], "target_statics": r[3], "asm": r[4]}
                    for r in rows
                ],
                fh,
                indent=2,
            )


if __name__ == "__main__":
    main()
