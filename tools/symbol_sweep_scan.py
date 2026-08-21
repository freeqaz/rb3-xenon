#!/usr/bin/env python3
"""symbol_sweep_scan.py — READ-ONLY scanner for the RFC-14 systematic symbol sweep.

Implements the scanner proposed in
`docs/plans/paths-to-100/14-systematic-symbol-sweeps.md` (Part A/B/C).

WHAT IT DOES
------------
Enumerates sub-100% (near-miss) functions whose residual objdiff mismatches are
DOMINATED by the local-static-`Symbol` / missing-guard-thunk signature, and emits
a ranked, per-function worklist that a phase-2 apply loop reads from a FIXED path:

    /home/free/tmp/symbol_sweep/worklist.json

The proven fix (commit 3342b30): retail band3 emits a function-local
`static Symbol sym("literal");` (COFF slot `?sym@?N??Fn@Class@@...@4V?@A` plus a
`?$S<k>@?N??Fn@...@4IA` static-init guard), where our source used a global
`Symbols.h` reference (`extern Symbol foo;` -> `?foo@@3VSymbol@@A`). Converting the
global ref to a function-local static closed 2 fns + paired 20 guard/atexit thunks
(+22 matched, 0 regressions) for a 2-line source edit.

DATA SOURCE (Part A) — the objdiff RELOCATION-MISMATCH JSON, *not* a target-obj
name grep. The RFC verified (0/3091 target objs) that dtk SPLIT target objs carry
ZERO local-static mangled names — they leave local-static data slots as anonymous
`lbl_`/`fn_` placeholders. So the signal must be inferred from the DIFF: our
(base) side references a resolved symbol, the target side references an anonymous
`lbl_`/`fn_` slot. We run the proven invocation

    <objdiff-cli> diff -p . '<mangled_sym>' --build -f json --include-instructions

(last stdout line = JSON) and walk `instructions[*]` — each carries `target`/`base`
`typed_args` and, on a mismatch, a `diff_breakdown.arguments[*]` with per-arg
`{arg_type, target.value, base.value}`.

CLASSIFIER (Part B) — for each near-miss function we tally its symbol-arg
mismatches into these disjoint buckets, keyed on the BASE-side spelling (base = our
obj, the side we control by editing source):

  * GLOBAL_SYMBOL   base is a global `?name@@3VSymbol@@A`  AND target is `lbl_`/`fn_`.
                    This is the FIXABLE `local_static_symbol` candidate: retail put a
                    function-local static here; convert the source ref.
  * LOCAL_STATIC    base is ALREADY a function-local static `...@?N??...@4V?@A`
                    (or the paired `?$S<k>@...@4IA` guard) AND target is `lbl_`.
                    The source is already correct; the residual is objdiff's inability
                    to pair anonymous target slots by name (RAW-ceiling). NOT a source
                    edit — a naming/patcher concern. Reported as `naming_only`, NOT
                    worklisted.
  * OTHER_RELOC     base is any other resolved symbol (global var, RTTI `??_R`,
                    string `??_C`, a `bl` callee `?fn@...`) vs a target `lbl_`/`fn_`.
                    Ordinary reloc-address normalization noise.
  * NON_SIGNATURE   anything else that makes a fn diverge: instruction insert/delete,
                    register swaps, immediate/offset deltas, opcode replaces. If a fn
                    carries substantial NON_SIGNATURE mismatch it is NOT a candidate —
                    the Symbol fix alone won't close it (conservative "dominated by one
                    signature" gate, per RFC Part B / the nearmiss-classify doctrine).

CANDIDATE GATE (conservative): a function is worklisted `local_static_symbol` iff
  (a) it has >=1 GLOBAL_SYMBOL mismatch, and
  (b) it has ZERO structural NON_SIGNATURE mismatch (no insert/delete/regswap/
      immediate/opcode-replace) — i.e. the ONLY thing wrong is the Symbol reloc.
Confidence:
  high   — GLOBAL_SYMBOL present, no NON_SIGNATURE, and OTHER_RELOC small relative
           to GLOBAL_SYMBOL (signature dominates).
  medium — GLOBAL_SYMBOL present, no NON_SIGNATURE, but OTHER_RELOC comparable/larger
           (fix likely improves but may not solely close; still worth an A/B slot).
`prefer_global` (the inverse, RFC Part B #3) is emitted when base ALREADY has a
local static but no global anywhere and there is a matching guard we lack — rare;
detected but low priority.

OUTPUT (Part C) — worklist rows:
  {fn, unit, src, kind, baseline_pct, literals, guard_slots, confidence}
ranked by (kind priority, baseline_pct desc) so the closest-to-100 land first.
`literals` are the string-literal spellings the target's anonymous slot points at,
harvested from nearby `??_C@...` string relocs when resolvable (the exact Symbol
spelling still comes from the dc3 / rb3-Wii oracle at apply time — the scanner
LOCATES, the oracle SPECIFIES, the A/B GATES; no step guesses bytes).

Every emitted row is stamped with the documented RAW-verdict gotcha: after applying
the fix, RAW/normalized may still sit ~97.5 even when byte-exact (dtk anonymous-slot
pairing) — TRUST report.json / normalized, not RAW.

USAGE
-----
  tools/symbol_sweep_scan.py                       # scan band3+network near-miss [90,100)
  tools/symbol_sweep_scan.py --tiers band3         # band3 only
  tools/symbol_sweep_scan.py --tiers band3,network,engine  # include engine
  tools/symbol_sweep_scan.py --band 95-100         # tighter primary band
  tools/symbol_sweep_scan.py --limit 40            # cap fns probed (fast preview)
  tools/symbol_sweep_scan.py --project-dir /home/free/tmp/wt-sweepscan  # objdiff cwd
  tools/symbol_sweep_scan.py --out /path/worklist.json
  tools/symbol_sweep_scan.py --top 10              # also print top-N summary

READ-ONLY: never edits source, never builds anything except objdiff's own
incremental diff of an already-warm tree. Requires a warm build so objdiff has
objs to diff (report.json + build/45410914/src/**/*.obj present).
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from collections import Counter
from concurrent.futures import ThreadPoolExecutor, as_completed

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_REPORT = os.path.join(ROOT, "build", "45410914", "report.json")
DEFAULT_OBJDIFF = os.path.join(ROOT, "bin", "objdiff-cli")
DEFAULT_OUT = "/home/free/tmp/symbol_sweep/worklist.json"

RAW_GOTCHA = ("After applying, RAW/normalized may sit ~97.5 even when byte-exact "
              "(dtk anonymous-slot pairing) — TRUST report.json/normalized, not RAW.")

# ── signature regexes (base-side spelling; base = our obj) ───────────────────
# global Symbol from Symbols*.h: `extern Symbol foo;` -> `?foo@@3VSymbol@@A`
GLOBAL_SYMBOL = re.compile(r"^\?[A-Za-z_]\w*@@3VSymbol@@A$")
# function-local static Symbol slot: `?name@?N??Fn@Class@@...@4V<d>@A`
#   the `@?<digit-or-N>??` marks a local scope; `@4V<d>@A` the Symbol-typed static.
LOCAL_STATIC = re.compile(r"@\?[0-9N]\?\?.+@4V\d@A$")
# static-init guard for a local static: `?$S<k>@?N??Fn@...@4IA`
GUARD_SLOT = re.compile(r"^\?\$S\d*@\?[0-9N]\?\?.+@4IA$")
# a resolved target anonymous slot (what dtk emits for local-static data)
ANON_TARGET = re.compile(r"^(lbl_|fn_)[0-9A-Fa-f]+$")
# a C-string literal COFF symbol: `??_C@_0..@..@<text>?$AA@`
STRING_SYM = re.compile(r"^\?\?_C@")

# match_type values that mean a real STRUCTURAL divergence (not reloc-naming).
# objdiff emits: equal, diff_arg (arg-level), replace, insert, delete, diff_op.
STRUCTURAL_MT = {"insert", "delete", "diff_op", "replace"}

# arg_type values in diff_breakdown that mean a real code divergence (not a symbol
# relocation): a register swap or an immediate/offset delta.
STRUCTURAL_ARG = {"register", "immediate", "branch_dest"}

KIND_PRIORITY = {"local_static_symbol": 0, "prefer_global": 1}


def _f(x):
    try:
        return float(x)
    except (TypeError, ValueError):
        return 0.0


# ── report.json near-miss enumeration ────────────────────────────────────────

def unit_tier(u):
    md = u.get("metadata", {}) or {}
    sp = md.get("source_path") or ""
    if sp.startswith("src/band3"):
        return "band3"
    if sp.startswith("src/network"):
        return "network"
    if sp.startswith("src/system"):
        return "engine"
    return "other"


def near_miss_functions(report_path, tiers, lo, hi):
    """Yield (pct, unit_name, fn_mangled, src_path) for wired fns in [lo,hi)."""
    d = json.load(open(report_path))
    out = []
    for u in d.get("units", []):
        tier = unit_tier(u)
        if tier not in tiers:
            continue
        md = u.get("metadata", {}) or {}
        sp = md.get("source_path") or ""
        uname = u.get("name", "?")
        for f in u.get("functions", []):
            if "fuzzy_match_percent" not in f:  # WIRED gate (see fuzzy_progress.is_wired)
                continue
            pct = _f(f.get("match_percent_normalized", f.get("fuzzy_match_percent")))
            if lo <= pct < hi:
                out.append((pct, uname, f.get("name", "?"), sp))
    return out


# ── objdiff invocation (Part A) ──────────────────────────────────────────────


def _ensure_patched(project_dir):
    """Build through `post-compile` and ASSERT -- never `--build` one object.

    `objdiff-cli diff --build` (without `--full-build`) is `ninja <base .obj>`,
    a single-object target that stops one edge short of rb3-xenon's six
    post-compile patchers: it answers from raw compiler output and leaves the
    object that way for report.json and every concurrent lane. Measured on
    rb3-xenon: one such call cost unit default/BandUI 2.006 pp of
    matched_code_percent and read a 100.0 function as 99.7.

    Memoized per tree per process; raises UnpatchedTreeError rather than
    returning a number from a partially-patched tree.
    """
    import sys as _sys, os as _os
    _here = _os.path.dirname(_os.path.abspath(__file__))
    while _here != "/" and not _os.path.isdir(_os.path.join(_here, "scripts", "orchestrator")):
        _here = _os.path.dirname(_here)
    _scripts = _os.path.join(_here, "scripts")
    if _scripts not in _sys.path:
        _sys.path.insert(0, _scripts)
    from orchestrator.patch_guard import ensure_patched_tree_once
    return ensure_patched_tree_once(project_dir)


def run_objdiff_json(objdiff_bin, project_dir, symbol, timeout=180):
    """Run the proven objdiff invocation, return the last-line JSON dict or None.

    NO `--build` -- see _ensure_patched(). A sweep is the worst place for it:
    it builds one object per symbol, so it unpatched the tree once per symbol
    and every later symbol was scored against a progressively more degraded
    tree.
    """
    _ensure_patched(project_dir)
    cmd = [objdiff_bin, "diff", "-p", ".", symbol,
           "-f", "json", "--include-instructions"]
    try:
        p = subprocess.run(cmd, cwd=project_dir, capture_output=True,
                           text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        return None
    for line in reversed((p.stdout or "").strip().splitlines()):
        line = line.strip()
        if line.startswith("{") and line.endswith("}"):
            try:
                return json.loads(line)
            except ValueError:
                continue
    return None


# ── classifier (Part B) ──────────────────────────────────────────────────────

def classify(diff):
    """Classify one function's residual mismatches.

    Returns a dict with per-bucket counts, harvested literals + guard slots, and
    the candidate verdict/kind/confidence. `diff` is the objdiff JSON dict.
    """
    global_sym = 0          # base global Symbol -> target lbl (FIXABLE)
    local_static = 0        # base already local-static -> target lbl (naming-only)
    guard = 0               # base ?$S guard -> target lbl
    other_reloc = 0         # any other resolved base sym -> target lbl (reloc noise)
    structural = 0          # insert/delete/regswap/imm/opcode-replace (NON_SIGNATURE)
    literals = set()        # nearby string literals (fix-spelling hints)

    instrs = diff.get("instructions", []) or []
    for ins in instrs:
        mt = ins.get("match_type")
        if mt in ("equal", None):
            continue

        # Structural (non-reloc) divergence: insert/delete/opcode-replace/diff_op.
        if mt in STRUCTURAL_MT:
            structural += 1
            # keep scanning args below in case a replace still carries a symbol,
            # but the structural flag already disqualifies the candidate gate.

        bd = ins.get("diff_breakdown") or {}
        args = bd.get("arguments", []) or []
        saw_symbol_arg = False
        for arg in args:
            at = arg.get("arg_type", "")
            if at in STRUCTURAL_ARG:
                # register swap or immediate/branch delta = real code divergence.
                structural += 1
                continue
            if at != "symbol":
                continue
            saw_symbol_arg = True
            bv = (arg.get("base") or {}).get("value")
            tv = (arg.get("target") or {}).get("value")
            bv = bv if isinstance(bv, str) else ""
            tv = tv if isinstance(tv, str) else ""
            tgt_anon = bool(ANON_TARGET.match(tv))
            # Harvest string-literal spellings from either side (fix hints).
            for v in (bv, tv):
                if STRING_SYM.match(v):
                    lit = _decode_string_sym(v)
                    if lit:
                        literals.add(lit)
            if not tgt_anon:
                # base and target both resolved & differ -> not our signature.
                other_reloc += 1
                continue
            if GLOBAL_SYMBOL.match(bv):
                global_sym += 1
            elif GUARD_SLOT.match(bv):
                guard += 1
                local_static += 1
            elif LOCAL_STATIC.search(bv):
                local_static += 1
            else:
                other_reloc += 1
        # A diff_arg with no symbol arg but with a structural arg was already
        # counted; a bare diff_arg with only an equal-ish arg shouldn't happen.
        if mt == "diff_arg" and not saw_symbol_arg and not args:
            structural += 1

    verdict = _verdict(global_sym, local_static, guard, other_reloc, structural)
    return {
        "global_sym": global_sym,
        "local_static": local_static,
        "guard": guard,
        "other_reloc": other_reloc,
        "structural": structural,
        "literals": sorted(literals),
        **verdict,
    }


def _verdict(global_sym, local_static, guard, other_reloc, structural):
    """Apply the conservative candidate gate."""
    # FIXABLE: base has a global Symbol reloc and NO structural divergence.
    if global_sym > 0 and structural == 0:
        # dominated => high; comparable/larger other_reloc => medium.
        if other_reloc <= global_sym:
            conf = "high"
        else:
            conf = "medium"
        return {"kind": "local_static_symbol", "confidence": conf,
                "guard_slots": guard, "candidate": True}
    # INVERSE (rare): base already local-static, no global, guard present, no
    # structural -> maybe retail wanted a global. Low-priority, low-conf.
    if global_sym == 0 and local_static > 0 and structural == 0 and guard > 0:
        return {"kind": "prefer_global", "confidence": "low",
                "guard_slots": guard, "candidate": True}
    # naming-only: local-static already correct, residual is anon-slot pairing.
    if local_static > 0 and global_sym == 0 and structural == 0:
        return {"kind": "naming_only", "confidence": "n/a",
                "guard_slots": guard, "candidate": False}
    # everything else: not a signature candidate.
    return {"kind": "non_signature", "confidence": "n/a",
            "guard_slots": guard, "candidate": False}


def _decode_string_sym(sym):
    """Best-effort literal from a `??_C@_0LEN@HASH@text?$AA@` COFF string symbol.

    MSVC mangles the readable text as the trailing token before `?$AA@` (the NUL).
    We recover the human-readable tail heuristically; exact spelling still comes
    from the oracle at apply time. Returns '' if not confidently decodable.
    """
    m = re.match(r"^\?\?_C@_0\w+@\w+@(.*)\?\$AA@$", sym)
    if not m:
        return ""
    raw = m.group(1)
    # MSVC escapes non-alnum as `?$XX`; strip a few common escapes to a hint.
    raw = raw.replace("?$AA", "").replace("?$AN", "\n")
    # `?$` escapes encode a byte; drop them for a readable hint rather than guess.
    raw = re.sub(r"\?\$[0-9A-Za-z][0-9A-Za-z]", "", raw)
    raw = re.sub(r"\?[0-9A-Za-z]", "", raw)
    return raw if re.fullmatch(r"[\w./ -]*", raw) and raw else ""


# ── driver ───────────────────────────────────────────────────────────────────

def scan(report_path, objdiff_bin, project_dir, tiers, lo, hi, limit, workers):
    fns = near_miss_functions(report_path, tiers, lo, hi)
    fns.sort(key=lambda r: -r[0])  # closest-to-100 first (probe order)
    if limit:
        fns = fns[:limit]

    results = []       # candidate rows
    naming_only = []   # already-correct RAW-ceiling fns (reported, not worklisted)
    stats = Counter()
    stats["scanned_target"] = len(fns)

    def work(item):
        pct, uname, sym, sp = item
        try:
            diff = run_objdiff_json(objdiff_bin, project_dir, sym)
            if diff is None:
                return ("nojson", item, None)
            cls = classify(diff)
            return ("ok", item, cls)
        except Exception as e:  # one bad fn must never abort the whole sweep
            return ("error", item, str(e))

    with ThreadPoolExecutor(max_workers=workers) as ex:
        futs = {ex.submit(work, it): it for it in fns}
        done = 0
        for fut in as_completed(futs):
            done += 1
            status, item, cls = fut.result()
            pct, uname, sym, sp = item
            if status == "nojson":
                stats["nojson"] += 1
                continue
            if status == "error":
                stats["error"] += 1
                print(f"[scan] error on {sym}: {cls}", file=sys.stderr)
                continue
            stats["scanned"] += 1
            kind = cls["kind"]
            stats[kind] += 1
            if kind == "naming_only":
                naming_only.append({
                    "fn": sym, "unit": uname, "src": sp,
                    "baseline_pct": round(pct, 4),
                    "local_static": cls["local_static"], "guard_slots": cls["guard"],
                })
            if not cls.get("candidate"):
                continue
            results.append({
                "fn": sym,
                "unit": uname,
                "src": sp,
                "kind": kind,
                "baseline_pct": round(pct, 4),
                "literals": cls["literals"],
                "guard_slots": cls["guard_slots"],
                "confidence": cls["confidence"],
                "counts": {
                    "global_sym": cls["global_sym"],
                    "local_static": cls["local_static"],
                    "other_reloc": cls["other_reloc"],
                    "structural": cls["structural"],
                },
                "note": RAW_GOTCHA,
            })
            if done % 25 == 0:
                print(f"[scan] {done}/{len(fns)} probed…", file=sys.stderr)

    # Rank: (kind priority, baseline_pct desc, confidence order).
    conf_order = {"high": 0, "medium": 1, "low": 2, "n/a": 3}
    results.sort(key=lambda r: (KIND_PRIORITY.get(r["kind"], 9),
                                -r["baseline_pct"],
                                conf_order.get(r["confidence"], 9)))
    naming_only.sort(key=lambda r: -r["baseline_pct"])
    return results, naming_only, stats


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--report", default=DEFAULT_REPORT)
    ap.add_argument("--objdiff", default=DEFAULT_OBJDIFF)
    ap.add_argument("--project-dir", default=ROOT,
                    help="objdiff working dir (the warm worktree/repo root)")
    ap.add_argument("--tiers", default="band3,network",
                    help="comma list of tiers to scan: band3,network,engine,other")
    ap.add_argument("--band", default="90-100",
                    help="near-miss band LO-HI (default 90-100)")
    ap.add_argument("--limit", type=int, default=0,
                    help="cap number of fns probed (0 = all)")
    ap.add_argument("--workers", type=int, default=4,
                    help="parallel objdiff invocations (default 4)")
    ap.add_argument("--out", default=DEFAULT_OUT)
    ap.add_argument("--top", type=int, default=10,
                    help="print top-N candidate summary to stdout")
    ap.add_argument("--json-summary", action="store_true",
                    help="emit machine-readable summary to stdout")
    a = ap.parse_args()

    if not os.path.exists(a.report):
        sys.exit(f"[symbol_sweep] no report.json at {a.report} — build first")
    if not os.path.exists(a.objdiff):
        sys.exit(f"[symbol_sweep] no objdiff-cli at {a.objdiff}")

    tiers = set(t.strip() for t in a.tiers.split(",") if t.strip())
    try:
        lo_s, hi_s = a.band.split("-")
        lo, hi = float(lo_s), float(hi_s)
    except ValueError:
        sys.exit(f"[symbol_sweep] bad --band {a.band!r}; want LO-HI e.g. 90-100")

    print(f"[symbol_sweep] scanning tiers={sorted(tiers)} band=[{lo},{hi}) "
          f"project={a.project_dir}", file=sys.stderr)
    results, naming_only, stats = scan(
        a.report, a.objdiff, a.project_dir, tiers, lo, hi, a.limit, a.workers)

    out_dir = os.path.dirname(a.out)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    payload = {
        "schema": "symbol_sweep.worklist/1",
        "generated_from": {
            "report": os.path.abspath(a.report),
            "project_dir": os.path.abspath(a.project_dir),
            "tiers": sorted(tiers),
            "band": [lo, hi],
        },
        "gotcha": RAW_GOTCHA,
        "stats": dict(stats),
        "candidate_count": len(results),
        "candidates": results,
        "naming_only_count": len(naming_only),
        "naming_only": naming_only,
    }
    with open(a.out, "w") as fh:
        json.dump(payload, fh, indent=2)

    if a.json_summary:
        print(json.dumps({"candidate_count": len(results),
                          "stats": dict(stats),
                          "out": os.path.abspath(a.out)}, indent=2))
        return

    print(f"\n[symbol_sweep] wrote {os.path.abspath(a.out)}", file=sys.stderr)
    print(f"  scanned:        {stats.get('scanned', 0)}"
          f"  (target {stats.get('scanned_target', 0)}, no-json {stats.get('nojson', 0)})")
    print(f"  candidates:     {len(results)}  "
          f"(local_static_symbol={stats.get('local_static_symbol',0)}, "
          f"prefer_global={stats.get('prefer_global',0)})")
    print(f"  naming_only:    {stats.get('naming_only',0)}  "
          f"(already-correct local-static; RAW-ceiling, not worklisted)")
    print(f"  non_signature:  {stats.get('non_signature',0)}")
    print(f"\ntop {a.top} candidates (kind, baseline%, guard_slots, conf):")
    for r in results[:a.top]:
        print(f"  {r['baseline_pct']:6.2f}  {r['kind']:20s} g={r['guard_slots']} "
              f"{r['confidence']:6s} {r['unit']} | {r['fn']}")
    if not results:
        print("  (none — signature not present in this band; see naming_only tally)")


if __name__ == "__main__":
    main()
