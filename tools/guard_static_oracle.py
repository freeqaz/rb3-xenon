#!/usr/bin/env python3
"""GUARD-WORD ORACLE -- recover retail's function-local statics AND their
DECLARATION ORDER straight out of the retail object.

Promoted into tools/ by lane CY-2 from lane CX-1's scratch `guard_oracle.py`
(/home/free/tmp/laneCX1/).  Companion to tools/guard_funclet_census.py: the
census LOCATES units that are short of local statics, this oracle tells you
WHICH statics and IN WHAT ORDER to write them.

────────────────────────────────────────────────────────────────────────────
WHY IT WORKS
────────────────────────────────────────────────────────────────────────────
MSVC allocates ONE BIT of a per-function guard word per function-local static,
IN DECLARATION ORDER, and emits one 32-byte `??__F` funclet that clears exactly
that bit (see tools/guard_funclet_census.py for the byte shape).  So from the
retail obj alone we can recover, per function: how many statics it has, which
string literals they intern, and the order they were declared in.

  guard word  --REFHI/REFLO reloc--> funclet (clears bit N)
  guard word  --REFHI/REFLO reloc--> owning function (tests bit N)

Ordering is what makes this an oracle rather than a hint: MSVC assigns bit N to
the Nth static in SOURCE ORDER, so `bits` sorted ascending IS the declaration
sequence you must reproduce.

────────────────────────────────────────────────────────────────────────────
KNOWN LIMITS -- these are why a row can be un-actionable
────────────────────────────────────────────────────────────────────────────
⚠ OWNER RESOLUTION IS OFTEN AMBIGUOUS OR ABSENT.  Measured over the 310
  residual funclets at HEAD 475a47ec: 146 unique owner, 121 ambiguous
  (several functions reference the same guard word), 43 no owner resolved.
  Of the 146 unique owners only 8 are NAMED -- the rest are `fn_XXXXXXXX`.
  A guard word with no resolvable owner cannot be acted on by source editing:
  Accomplishment.cpp has two guard words carrying 25 and 5 statics and NO
  resolvable owner, which is why lane CX-1 left its 29 funclets alone.

⚠ PLACEMENT IS CODEGEN-LOAD-BEARING, NOT STYLISTIC.  Reproduce the order this
  tool prints, but do NOT blindly insert each static immediately before its
  first use: if that use is inside a LOOP the guard test lands inside the loop
  while retail's is outside (measured 87.04 -> 75.79, 75.83 -> 60.76).  Hoist
  above the loop.

⚠ MSVC TRUNCATES LITERAL NAMES AT 32 CHARS in the mangling, so a printed
  literal may be a prefix (`tour_goal_focus_player_contribut` for retail's
  `tour_goal_focus_player_contribution_format`).  A decode artifact, NOT a
  defect -- do not "fix" those rows.

⚠ DO NOT INVENT A LITERAL THE SOURCE DOES NOT CONTAIN.  Lane CX-1 skipped
  SessionMgr::AddLocalUser because `session_ready` occurs ZERO times in the
  file; guessing the spelling is how a map gets fitted to a metric.

⚠ Key by objdiff UNIT NAME (this tool already does).  One .cpp can back two
  units -- `AccomplishmentProgress.cpp` backs both
  `default/band3/meta_band/AccomplishmentProgress` and
  `default/AccomplishmentProgress` -- so source_path is not a key.

Usage:
    tools/guard_static_oracle.py <worktree> <objdiff-unit-name> [more units...]

    # find the unit names for a source file first:
    python3 -c "import json;print([u['name'] for u in json.load(open('objdiff.json'))['units'] \
        if (u.get('metadata') or {}).get('source_path')=='src/foo/Bar.cpp'])"
"""
import json
import os
import struct
import sys
from collections import defaultdict

REL24, REFHI, REFLO = 0x0006, 0x0010, 0x0011
PROLOGUE, EPI_ADDI, EPI_BLR, OP_RLWINM = 0x9421FFA0, 0x38210060, 0x4E800020, 21
SKIP_PREFIXES = (".", "except_data", "__", "$")


def bit_of_rlwinm(w):
    """`rlwinm rA,rS,0,mb,me` with mb == me+2 clears exactly bit (31-me-1)."""
    me = (w >> 1) & 31
    return 31 - ((me + 1) % 32)


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    wt, want = sys.argv[1], sys.argv[2:]
    sys.path.insert(0, os.path.join(wt, "tools"))
    sys.path.insert(0, os.path.join(wt, "scripts"))
    import retail_props as RP

    img = RP.Image(os.path.join(wt, "orig/45410914/band.exe"))
    ex = RP.Extractor(wt, image=img)
    units = {u["name"]: u
             for u in json.load(open(os.path.join(wt, "objdiff.json")))["units"]}

    for un in want:
        u = units.get(un)
        if u is None:
            print(f"!! no unit named {un!r} (key by UNIT NAME, not source_path)")
            continue
        tp = os.path.join(wt, u["target_path"])
        bp = os.path.join(wt, u.get("base_path") or "")
        c = ex.obj(tp)
        src = (u.get("metadata") or {}).get("source_path")
        print(f"\n########## {un}   ({src})")

        # 1. guard-clearing funclets: guard symbol -> {bit: funclet name}
        guards = defaultdict(dict)
        for name, s in c.symbol_map.items():
            si = s.get("section", 0)
            if si <= 0:
                continue
            try:
                blob = c.get_section_data(si - 1)
            except Exception:
                continue
            off = s.get("value", 0)
            b = blob[off:off + 32]
            if len(b) != 32:
                continue
            w = struct.unpack(">8I", b)
            if w[0] != PROLOGUE or w[6] != EPI_ADDI or w[7] != EPI_BLR:
                continue
            if (w[3] >> 26) != OP_RLWINM:
                continue
            rel = [r for r in c.get_section_relocations(si - 1)
                   if off <= r["offset"] < off + 32 and r["type"] in (REFHI, REFLO)]
            gs = {r["symbol_name"] for r in rel}
            if len(gs) != 1:
                continue
            guards[gs.pop()][bit_of_rlwinm(w[3])] = name

        # 2. owning functions: which non-funclet symbol references each guard word
        funclet_names = {n for d in guards.values() for n in d.values()}
        owner = {}
        for name, s in c.symbol_map.items():
            si = s.get("section", 0)
            if si <= 0 or name.startswith(SKIP_PREFIXES) or name in funclet_names:
                continue
            start = s.get("value", 0)
            b = ex._bounds(c, tp).get(si, ())
            try:
                blob = c.get_section_data(si - 1)
            except Exception:
                continue
            end = next((v for v in b if v > start), len(blob))
            for r in c.get_section_relocations(si - 1):
                if start <= r["offset"] < end and r["symbol_name"] in guards:
                    owner.setdefault(r["symbol_name"], set()).add(name)

        for g, bits in sorted(guards.items(), key=lambda kv: -len(kv[1])):
            owners = sorted(owner.get(g, set()))
            uniq = len(owners) == 1
            own = owners[0] if uniq else (f"AMBIGUOUS {owners}" if owners
                                          else "UNRESOLVED -- not actionable")
            names = ex.props(tp, owners[0]) if uniq else None
            ours = (ex.props(bp, owners[0]) if uniq and os.path.exists(bp) else None)
            print(f"  guard {g}: {len(bits)} statics, bits {sorted(bits)}")
            print(f"     owner: {own[:110]}")
            print(f"     retail literals (DECLARATION ORDER): {names}")
            print(f"     ours                               : {ours}")


if __name__ == "__main__":
    main()
