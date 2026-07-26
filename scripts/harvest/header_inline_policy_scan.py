#!/usr/bin/env python3
"""Whole-binary candidate generator for the INLINED-BY-US / OUT-OF-LINE-IN-RETAIL
("header-inline policy") divergence.

Shape
-----
A function is defined in a header (or otherwise made visible for inlining), so
MSVC ``/Ob2`` inlines it into every consuming TU of *our* build.  Retail emitted
it **out of line** as a real COMDAT and *calls* it.  Two consequences per
consuming caller:

1. retail has a ``bl`` where we have an inlined body  -> body/frame divergence;
2. if the callee can throw, our now-local copy is provably nothrow at the call
   site, so MSVC **deletes the caller's EH cleanup funclets** (and their
   ``__unwindtable$``) outright.

Both halves are observable straight out of COFF, with no disassembly heuristics:
``__unwindtable$<mangled>`` is emitted per EH function and its section
relocations name that function's ``__unwind$N`` / ``__catch$N`` funclets, so a
*missing* ``__unwindtable$`` means MSVC deleted the EH data (scored 0, not
"unknown").  See docs/plans/scatter-include-inlining-collapse-2026-07-26.md §2.1.

Method
------
For every pinned unit that has both a dtk target obj and one of our objs, take
each function defined on both sides and compare the set of **call** relocations
(relocs landing on a ``bl`` instruction).  Callees present on the target side and
absent on ours are *target-only callees*: retail called something we did not.
Aggregate those by callee and classify by where the callee's body lives in our
build:

* defined in **many** of our objs      -> ``HEADER-INLINE``   (this lever)
* defined in exactly **one** of our objs -> ``SINGLE-TU``     (scatter / ordinary)
* defined in **none** of our objs      -> ``NOT-IN-TU``       (body divergence,
  or a symbol-map mispair) -- explicitly *not* this lever.

Ranking signal per candidate callee:
  ``sites``        number of distinct caller functions that lose a ``bl`` to it
  ``sites_nm``     of those, how many are not already strict-100 (the workable set)
  ``eh_sites``     of the non-strict ones, how many *also* lost every funclet
                   (``base_funclets == 0 < target_funclets``) -- the SHARP half
  ``defobjs``      how many of our objs emit a COMDAT for the callee (fan-out; a
                   proxy for how many TUs would change codegen if it is forced
                   out of line -- i.e. the blast radius / risk)

Usage
-----
    venv/bin/python scripts/harvest/header_inline_policy_scan.py \
        --repo . --json ~/tmp/laneT/hip.json --top 40
"""
from __future__ import annotations

import argparse
import json
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from funclet_cascade_rank import PE, Coff, parse_eh  # noqa: E402

NOISE_PREFIX = ("__unwind$", "__catch$", "__unwindtable$", "__catchsym$", "$", ".")
CALLEE_NOISE = re.compile(r"^(fn_[0-9A-Fa-f]{8}|__savegpr|__restgpr|__savefpr|"
                          r"__restfpr|__savevmx|__restvmx|\?\?_C@|\?\?_R|\$)")


def obj_path(objdir: Path, source: str) -> Path:
    rel = source[4:] if source.startswith("src/") else source
    return (objdir / rel).with_suffix(".obj")


def call_relocs(coff: Coff, sym: str, size: int | None) -> set[str] | None:
    """Symbols targeted by ``bl`` instructions inside ``sym``.  None if absent."""
    n = 4096 if not size else max(1, size // 4)
    words, sec, val = coff.func_words(sym, n=n)
    if not words:
        return None
    rel = coff.reloc_syms(sec, val, val + 4 * len(words))
    out = set()
    for i, w in enumerate(words):
        if (w >> 26) == 18 and (w & 1):  # bl
            nm = rel.get(val + 4 * i)
            if nm and not nm.startswith(NOISE_PREFIX) and not CALLEE_NOISE.match(nm):
                out.add(nm)
    return out


def funclets(coff: Coff, sym: str) -> int:
    """Funclets we emitted for ``sym``.  A missing __unwindtable$ scores 0."""
    ent = coff.syms.get("__unwindtable$" + sym)
    if not ent:
        return 0
    _val, sec = ent
    if sec <= 0 or sec > len(coff.secs):
        return 0
    names = set(coff.reloc_syms(sec, 0, coff.secs[sec - 1]["size"]).values())
    return sum(1 for n in names
               if n.startswith("__unwind$") or n.startswith("__catch$"))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--json")
    ap.add_argument("--top", type=int, default=40)
    ap.add_argument("--min-sites", type=int, default=2)
    args = ap.parse_args()

    repo = Path(args.repo).resolve()
    objdir = repo / "build/45410914/src"
    tgtdir = repo / "build/45410914/obj"
    report = json.loads((repo / "build/45410914/report.json").read_text())

    # ---- 1. fan-out census: how many of our objs define each symbol, and does
    #         that definition carry EH data?
    defobjs: dict[str, set] = defaultdict(set)
    defobjs_eh: dict[str, set] = defaultdict(set)
    for op in sorted(objdir.rglob("*.obj")):
        try:
            c = Coff(op)
        except Exception:
            continue
        rel = str(op.relative_to(objdir))
        for nm, (_v, sc) in c.syms.items():
            if sc > 0 and not nm.startswith(NOISE_PREFIX):
                defobjs[nm].add(rel)
                if ("__unwindtable$" + nm) in c.syms:
                    defobjs_eh[nm].add(rel)
    print("fan-out census: %d distinct defined symbols over %d objs"
          % (len(defobjs), len({o for s in defobjs.values() for o in s})))

    # ---- 1b. ★ THE KILL FILTER.  If the callee is itself already strict-100 in
    # some unit, retail's own out-of-line copy is byte-identical to what we
    # already emit -- so there is NO inline-policy divergence to correct.  Retail
    # emitted the COMDAT *and* inlined at other sites, which our in-class
    # definition already reproduces.  Forcing such a callee out of line can only
    # lose: it deletes the COMDAT from every consuming obj (killing the paired
    # instance) and removes inlining retail actually performed.  Measured on
    # ??5BinStream@@QAAAAV0@AA_N@Z: -4 whole-binary.  All three candidates this
    # lane A/B'd would have been pre-killed by this one filter.
    strict_syms = {f["name"] for u in report["units"] for f in u.get("functions", [])
                   if f.get("match_percent_normalized") == 100.0}
    print("callees already strict-100 somewhere: %d symbols (kill-filter set)"
          % len(strict_syms))

    # ---- 1c. ★ PAIRED-UNIT COUNT.  `defobjs` counts OUR emissions and is NOT
    # yield -- only functions that report.json actually pairs are scored.  The
    # MakeString<const char*> candidate emitted a COMDAT in 115 objs but is
    # paired in exactly ONE unit, so its callee-side score could only ever be
    # -1 or 0.  Never read fan-out as a force-multiplier; it is blast radius.
    paired_units: dict[str, int] = defaultdict(int)
    for u in report["units"]:
        for f in u.get("functions", []):
            paired_units[f["name"]] += 1

    # ---- 2. per-unit target-vs-base call-reloc diff
    cand: dict[str, dict] = defaultdict(
        lambda: {"sites": [], "sites_nm": [], "eh_sites": [], "units": set(),
                 "in_tu": [], "in_tu_nm": [], "in_tu_eh": [], "in_tu_units": set()})
    units_scanned = 0
    parents_scanned = 0
    for u in report["units"]:
        src = (u.get("metadata") or {}).get("source_path")
        if not src or not u.get("functions"):
            continue
        stem = Path(u["name"]).name
        tp = tgtdir / (stem + ".obj")
        bp = obj_path(objdir, src)
        if not tp.exists() or not bp.exists():
            continue
        try:
            tc, bc = Coff(tp), Coff(bp)
        except Exception:
            continue
        units_scanned += 1
        sizes = {f["name"]: int(f["size"]) for f in u["functions"]}
        pct = {f["name"]: f.get("match_percent_normalized") for f in u["functions"]}
        for name, sz in sizes.items():
            if name.startswith("fn_") or name.startswith(NOISE_PREFIX):
                continue
            if name not in bc.syms or name not in tc.syms:
                continue
            tcall = call_relocs(tc, name, sz)
            bcall = call_relocs(bc, name, None)
            if tcall is None or bcall is None:
                continue
            parents_scanned += 1
            tonly = tcall - bcall
            if not tonly:
                continue
            strict = pct.get(name) == 100.0
            bfl = funclets(bc, name)
            for callee in tonly:
                e = cand[callee]
                e["sites"].append(name)
                e["units"].add(u["name"])
                if not strict:
                    e["sites_nm"].append(name)
                    if bfl == 0:
                        e["eh_sites"].append(name)
                # ---- §4b PER-SITE precision test.  We can only have INLINED
                # the callee if its body is in THIS caller's own TU.  If our obj
                # does not define it, the missing `bl` is body divergence (or a
                # symbol-map mispair) -- a different lever entirely.
                if callee in bc.syms and bc.syms[callee][1] > 0:
                    e["in_tu"].append(name)
                    e["in_tu_units"].add(u["name"])
                    if not strict:
                        e["in_tu_nm"].append(name)
                        if bfl == 0:
                            e["in_tu_eh"].append(name)

    print("scanned %d pinned units, %d paired parent functions" %
          (units_scanned, parents_scanned))

    # ---- 3. classify + rank
    rows = []
    for callee, e in cand.items():
        n_def = len(defobjs.get(callee, ()))
        if n_def == 0:
            cls = "NOT-IN-TU"
        elif n_def == 1:
            cls = "SINGLE-TU"
        else:
            cls = "HEADER-INLINE"
        rows.append({
            "callee": callee,
            "class": cls,
            "defobjs": n_def,
            "defobjs_eh": len(defobjs_eh.get(callee, ())),
            "sites": len(e["sites"]),
            "sites_nm": len(e["sites_nm"]),
            "eh_sites": len(e["eh_sites"]),
            # §4b-filtered: only sites whose OWN obj defines the callee
            "in_tu": len(e["in_tu"]),
            "in_tu_nm": len(e["in_tu_nm"]),
            "in_tu_eh": len(e["in_tu_eh"]),
            "callee_strict": callee in strict_syms,
            # how many units report.json actually PAIRS the callee in -- this,
            # not `defobjs`, bounds the callee-side score (see §1c)
            "callee_paired_units": paired_units.get(callee, 0),
            "units": sorted(e["units"]),
            "in_tu_units": sorted(e["in_tu_units"]),
            "site_names": sorted(set(e["sites_nm"]))[:40],
            "in_tu_site_names": sorted(set(e["in_tu_nm"]))[:40],
        })
    rows.sort(key=lambda r: (-r["in_tu_nm"], -r["in_tu_eh"], -r["sites_nm"]))

    tally = defaultdict(lambda: [0, 0, 0, 0, 0])
    for r in rows:
        t = tally[r["class"]]
        t[0] += 1
        t[1] += r["sites_nm"]
        t[2] += r["eh_sites"]
        t[3] += r["in_tu_nm"]
        t[4] += r["in_tu_eh"]
    print("\nBUCKET SIZE (callees retail calls out-of-line that we do not call)")
    print("%-16s %8s %10s %9s %12s %11s"
          % ("CLASS", "CALLEES", "NSTR_SITE", "EH_SITE", "INTU_NSTR", "INTU_EH"))
    for k in ("HEADER-INLINE", "SINGLE-TU", "NOT-IN-TU"):
        t = tally[k]
        print("%-16s %8d %10d %9d %12d %11d" % (k, t[0], t[1], t[2], t[3], t[4]))
    print("  INTU_* = §4b-filtered: caller's OWN obj defines the callee, so we")
    print("  really could have inlined it.  Everything else is body divergence.")

    # ---- kill-filter accounting
    work = [r for r in rows if r["in_tu_nm"] > 0]
    killed = [r for r in work if r["callee_strict"]]
    alive = [r for r in work if not r["callee_strict"]]
    print("\nKILL FILTER (callee already strict-100 somewhere => no inline-policy"
          " divergence to correct):")
    print("  workable callees %d -> %d survive (%d killed); sites %d -> %d"
          % (len(work), len(alive), len(killed),
             sum(r["in_tu_nm"] for r in work), sum(r["in_tu_nm"] for r in alive)))

    hi = [r for r in alive if r["class"] == "HEADER-INLINE"
          and r["in_tu_nm"] >= args.min_sites]
    tot_c = len([r for r in alive if r["class"] == "HEADER-INLINE"])
    print("\nTRUE inline-policy candidates (kill-filtered): %d callees with >=1"
          " workable in-TU site, %d with >=%d"
          % (tot_c, len(hi), args.min_sites))
    print("  DEFOB = our emissions = BLAST RADIUS (not yield).  PAIR = units")
    print("  report.json pairs the callee in = the callee-side score bound.")
    print("%-52s %5s %4s %5s %5s %5s %5s" %
          ("CALLEE", "DEFOB", "PAIR", "NSTR", "EH", "iNSTR", "iEH"))
    for r in hi[: args.top]:
        print("%-52s %5d %4d %5d %5d %5d %5d" %
              (r["callee"][:52], r["defobjs"], r["callee_paired_units"],
               r["sites_nm"], r["eh_sites"], r["in_tu_nm"], r["in_tu_eh"]))

    if args.json:
        out = Path(args.json).expanduser()
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps({"rows": rows, "tally": {k: v for k, v in tally.items()}},
                                  indent=1))
        print("\nwrote %s" % out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
