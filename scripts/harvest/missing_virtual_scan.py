#!/usr/bin/env python3
"""missing_virtual_scan.py -- "missing-virtual" candidate scanner.

Run with the repo venv:

    venv/bin/python scripts/harvest/missing_virtual_scan.py [--jobs 12] ...

WHY
---
A method that is *non-virtual* in our source but is dispatched *indirectly
through the vtable* in retail causes a systematic near-miss at EVERY call site.
Adding `virtual` to the method's declaration flips codegen at all sites at once.
Known landed example: GameMode::InMode / SetMode -> +9 strict from a 2-line edit
cascading across 18 game TUs.

THE ASSEMBLY SIGNATURE (in an objdiff instruction-level diff of a near-miss caller)
----------------------------------------------------------------------------------
Our compiled obj (BASE) emits a DIRECT call:

    bl   ?Method@Class@@...            <- reloc names the callee method

Retail (TARGET) emits an INDIRECT vtable call at the same logical site:

    lwz  rX, <vfptr-off>(rObj)         (load vtable pointer)
    lwz  rY, <slot>(rX)               (load slot)
    mtctr rY
    bctrl

objdiff aligns the single base `bl` against the target's vtable-slot `lwz`
(a `replace`/`diff_op`/`diff_arg` row where base.opcode==bl, target.opcode!=bl),
and the leftover target-side `mtctr`/`bctrl` show up as target-only rows
(match_type == "delete", base side == None) in the immediate neighbourhood.

  STRONG signature: base-bl replace/diff row + target-only {mtctr AND bctrl}
                    delete rows within a small window.
  WEAK   signature: base-bl replace/diff row + target-only {bctrl} nearby
                    (mtctr absent / aligned differently).

The callee resolved from the base `bl` reloc is the method that should be
`virtual`.  We rank callee methods by number of DISTINCT affected caller
functions, then grep the source tree to confirm the decl exists, is currently
NON-virtual, and lives on a polymorphic class -> ACTIONABLE.

NOTE (objdiff match_type semantics, verified empirically on this tree):
  insert  -> row present in BASE only   (target side == None)
  delete  -> row present in TARGET only  (base side == None)

READ-ONLY.  No builds, no edits to tracked files.  Outputs land under ~/tmp.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sqlite3
import subprocess
import sys
from collections import Counter, defaultdict
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
HOME_TMP = Path(os.path.expanduser("~/tmp"))
OBJDIFF = REPO / "bin" / "objdiff-cli"
REPORT_DEFAULT = REPO / "build" / "45410914" / "report.json"
DECOMP_DB = REPO / "decomp.db"
SRC = REPO / "src"

# window (rows) around the base-bl row to look for the target-only vtable tail
WIN_BACK = 2
WIN_FWD = 6


# ── objdiff-cli per-symbol instruction diff (mirrors triage layer-2) ─────────
def _last_json(text: str):
    last = None
    for line in text.splitlines():
        line = line.strip()
        if line.startswith("{"):
            try:
                last = json.loads(line)
            except Exception:
                pass
    return last


def fetch_diff(symbol: str, timeout: int = 60):
    cmd = [str(OBJDIFF), "diff", "-p", ".", symbol, "-f", "json",
           "--include-instructions"]
    try:
        proc = subprocess.run(cmd, cwd=str(REPO), capture_output=True,
                              text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        return None
    return _last_json(proc.stdout)


def _sym_arg(side: dict):
    """First Symbol-typed arg value (the reloc callee for a bl)."""
    if not side:
        return None
    for a in side.get("typed_args") or []:
        if a.get("type") == "Symbol":
            return a.get("value")
    return None


# ── the detector ─────────────────────────────────────────────────────────────
# runtime/helper callees that are never the missing-virtual we want
_CALLEE_SKIP_PREFIX = ("__", "_savegpr", "_restgpr", "_savefpr", "_restfpr",
                       "_savevmx", "_restvmx")


def _is_method_symbol(sym: str) -> bool:
    """MSVC method mangling starts with '?name@Class@@'. Filter runtime helpers
    and plain C names."""
    if not sym or not sym.startswith("?"):
        return False
    if sym.startswith(_CALLEE_SKIP_PREFIX):
        return False
    return "@@" in sym


def detect(symbol: str, unit: str, fuzzy: float, strict: float):
    """Return list of hit dicts for one caller function (may be several sites)."""
    d = fetch_diff(symbol)
    if not d:
        return [], []
    instrs = d.get("instructions") or []
    n = len(instrs)
    hits = []          # forward (missing-virtual) candidates
    reverse = []       # reverse lever (we indirect, retail direct)

    for i, it in enumerate(instrs):
        mt = it.get("match_type")
        b = it.get("base") or {}
        t = it.get("target") or {}

        # ---- forward: base bl  <->  target vtable call ----------------------
        if (b.get("opcode") == "bl" and mt in ("replace", "diff_op", "diff_arg")
                and t.get("opcode") != "bl"):
            callee = _sym_arg(b)
            if _is_method_symbol(callee):
                lo, hi = max(0, i - WIN_BACK), min(n, i + WIN_FWD + 1)
                del_ops = Counter()
                for w in instrs[lo:hi]:
                    if w.get("match_type") == "delete":
                        op = (w.get("target") or {}).get("opcode")
                        if op:
                            del_ops[op] += 1
                has_bctrl = del_ops.get("bctrl", 0) > 0
                has_mtctr = del_ops.get("mtctr", 0) > 0
                if has_bctrl:
                    strong = has_mtctr and (t.get("opcode") == "lwz"
                                            or mt == "replace")
                    hits.append({
                        "caller": symbol,
                        "unit": unit,
                        "fuzzy": fuzzy,
                        "strict": strict,
                        "callee": callee,
                        "row": i,
                        "target_opcode": t.get("opcode"),
                        "match_type": mt,
                        "confidence": "strong" if strong else "weak",
                        "window_delete_ops": dict(del_ops),
                    })

        # ---- reverse: base bctrl/mtctr (indirect)  <->  target bl (direct) --
        # base-only vtable tail (insert rows) co-located with a target bl.
        if (mt == "insert" and b.get("opcode") in ("bctrl", "mtctr")):
            lo, hi = max(0, i - WIN_FWD), min(n, i + WIN_BACK + 1)
            for w in instrs[lo:hi]:
                if (w.get("match_type") in ("replace", "diff_op", "diff_arg")
                        and (w.get("target") or {}).get("opcode") == "bl"):
                    tsym = _sym_arg(w.get("target") or {})
                    if _is_method_symbol(tsym):
                        reverse.append({
                            "caller": symbol, "unit": unit, "fuzzy": fuzzy,
                            "strict": strict, "callee": tsym, "row": i,
                            "confidence": "reverse",
                        })
                        break
    return hits, reverse


# ── crude MSVC demangle (Class::method) + decomp.db lookup ───────────────────
def _db_demangle_map():
    m = {}
    if not DECOMP_DB.exists():
        return m
    try:
        c = sqlite3.connect("file:%s?mode=ro" % DECOMP_DB, uri=True)
        for sym, dem in c.execute(
                "SELECT symbol, demangled FROM functions "
                "WHERE demangled IS NOT NULL"):
            if sym:
                m[sym] = dem
        c.close()
    except Exception:
        pass
    return m


def crude_class_method(sym: str):
    """?SetFrames@CamShot@@QAAX... -> ('CamShot', 'SetFrames').
    Returns (class, method).  class '' if it can't be parsed / is a template."""
    if not sym or not sym.startswith("?"):
        return "", ""
    body = sym[1:]
    # operator names ??_x etc. -- skip
    if body.startswith("?"):
        return "", ""
    toks = body.split("@")
    if len(toks) < 2:
        return "", ""
    method = toks[0]
    # scope tokens are everything up to the first empty token (the '@@' break)
    scope = []
    for tk in toks[1:]:
        if tk == "":
            break
        scope.append(tk)
    if not scope:
        return "", method
    # MSVC lists innermost-first; class is scope[0]
    cls = scope[0]
    return cls, method


def qualified(sym: str, dbmap: dict):
    """Best-effort human name for a callee symbol."""
    dem = dbmap.get(sym)
    if dem:
        # public: bool __cdecl BandSongMetadata::HasPart(class Symbol) const
        head = dem.split("(")[0].strip()
        tok = head.rsplit(" ", 1)[-1] if " " in head else head
        if "::" in tok:
            return tok
    cls, meth = crude_class_method(sym)
    if cls and meth:
        return "%s::%s" % (cls, meth)
    return meth or sym


# ── actionability: grep the header for the decl ─────────────────────────────
_HEADER_CACHE = {}


def _find_class_header(cls: str):
    """Grep src/ for a header declaring `class <cls>` / `struct <cls>`."""
    if cls in _HEADER_CACHE:
        return _HEADER_CACHE[cls]
    res = None
    if cls and not cls.startswith("?"):
        try:
            proc = subprocess.run(
                ["grep", "-rlE", r"(class|struct)\s+%s\b" % re.escape(cls),
                 str(SRC), "--include=*.h", "--include=*.hpp"],
                capture_output=True, text=True, timeout=30)
            files = [f for f in proc.stdout.splitlines() if f.strip()]
            if files:
                # prefer a header whose stem matches the class name
                files.sort(key=lambda f: (Path(f).stem.lower() != cls.lower(),
                                          len(f)))
                res = files[0]
        except Exception:
            res = None
    _HEADER_CACHE[cls] = res
    return res


def actionability(cls: str, method: str):
    """Return (flag, header_path).

    flags:
      ACTIONABLE            decl found, non-virtual, class polymorphic
      ALREADY_VIRTUAL       decl found but already `virtual`
      NON_POLYMORPHIC       decl found, non-virtual, but class has no vtable
      DECL_NOT_FOUND        couldn't locate a decl for method in the header
      NO_CLASS / TEMPLATE   couldn't resolve a plain class name
    """
    if not cls or cls.startswith("?$") or cls.startswith("?"):
        return "TEMPLATE_OR_UNRESOLVED", None
    header = _find_class_header(cls)
    if not header:
        return "HEADER_NOT_FOUND", None
    try:
        txt = Path(header).read_text(errors="replace")
    except Exception:
        return "HEADER_UNREADABLE", header

    # crude: is the class polymorphic anywhere in this header?
    polymorphic = bool(re.search(r"\bvirtual\b", txt))

    # find a declaration line mentioning `<method>(`
    decl_lines = [ln for ln in txt.splitlines()
                  if re.search(r"\b%s\s*\(" % re.escape(method), ln)]
    if not decl_lines:
        return ("DECL_NOT_FOUND_POLY" if polymorphic
                else "DECL_NOT_FOUND"), header

    # any decl line already virtual?
    any_virtual = any(re.search(r"\bvirtual\b", ln) for ln in decl_lines)
    # a definition (has a body brace / :: scope) is not the in-class decl
    only_defs = all(("::" in ln or "{" in ln) and "virtual" not in ln
                    for ln in decl_lines)

    if any_virtual:
        return "ALREADY_VIRTUAL", header
    if not polymorphic:
        return "NON_POLYMORPHIC", header
    if only_defs:
        return "DECL_ONLY_DEFINITION", header
    return "ACTIONABLE", header


# ── pool loading ─────────────────────────────────────────────────────────────
def load_pool(report_path: Path, min_fuzzy: float, max_fuzzy: float):
    data = json.load(open(report_path))
    pool = []
    for u in data.get("units") or []:
        uname = u.get("name")
        for fn in u.get("functions") or []:
            strict = fn.get("match_percent_normalized")
            fuzzy = fn.get("fuzzy_match_percent")
            if strict is None or fuzzy is None:
                continue
            if strict >= 100:
                continue
            if min_fuzzy <= fuzzy <= max_fuzzy:
                pool.append({"name": fn["name"], "unit": uname,
                             "fuzzy": fuzzy, "strict": strict})
    return pool


# ── main ─────────────────────────────────────────────────────────────────────
def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--jobs", type=int, default=12)
    ap.add_argument("--min-fuzzy", type=float, default=50.0)
    ap.add_argument("--max-fuzzy", type=float, default=99.99)
    ap.add_argument("--limit", type=int, default=0,
                    help="cap pool size (smoke test)")
    ap.add_argument("--report", default=str(REPORT_DEFAULT))
    ap.add_argument("--out-json", default=str(HOME_TMP / "missing_virtual_candidates.json"))
    ap.add_argument("--out-md", default=str(HOME_TMP / "missing_virtual_candidates.md"))
    ap.add_argument("--top", type=int, default=40, help="rows printed to stdout")
    args = ap.parse_args()

    if not OBJDIFF.exists():
        sys.exit("objdiff-cli not found at %s" % OBJDIFF)

    import time
    t0 = time.time()
    pool = load_pool(Path(args.report), args.min_fuzzy, args.max_fuzzy)
    if args.limit:
        pool = pool[:args.limit]
    print("[pool] %d near-miss caller functions "
          "(fuzzy %.2f-%.2f, strict<100)" %
          (len(pool), args.min_fuzzy, args.max_fuzzy), file=sys.stderr)

    all_hits = []
    all_reverse = []
    done = 0
    with ThreadPoolExecutor(max_workers=max(1, args.jobs)) as ex:
        futs = {ex.submit(detect, r["name"], r["unit"], r["fuzzy"], r["strict"]): r
                for r in pool}
        for fut in as_completed(futs):
            done += 1
            if done % 200 == 0:
                print("  scanned %d/%d" % (done, len(pool)), file=sys.stderr)
            try:
                hits, rev = fut.result()
            except Exception:
                continue
            all_hits.extend(hits)
            all_reverse.extend(rev)

    scan_secs = time.time() - t0

    # ── group by callee ──────────────────────────────────────────────────────
    dbmap = _db_demangle_map()
    groups = defaultdict(lambda: {"callers": {}, "confidences": Counter()})
    for h in all_hits:
        g = groups[h["callee"]]
        # keep the strongest confidence per (caller) site
        prev = g["callers"].get(h["caller"])
        if prev is None or (h["confidence"] == "strong"
                            and prev["confidence"] != "strong"):
            g["callers"][h["caller"]] = h
        g["confidences"][h["confidence"]] += 1

    rows = []
    for callee, g in groups.items():
        callers = list(g["callers"].values())
        n_callers = len(callers)
        n_strong = sum(1 for c in callers if c["confidence"] == "strong")
        fuzzy_gap = sum(100.0 - c["fuzzy"] for c in callers)
        cls, method = crude_class_method(callee)
        qual = qualified(callee, dbmap)
        flag, header = actionability(cls, method)
        rows.append({
            "callee": callee,
            "demangled": qual,
            "class": cls,
            "method": method,
            "n_callers": n_callers,
            "n_strong_callers": n_strong,
            "fuzzy_gap_sum": round(fuzzy_gap, 2),
            "actionability": flag,
            "header": header,
            "callers": [{
                "caller": c["caller"], "unit": c["unit"],
                "fuzzy": c["fuzzy"], "strict": c["strict"],
                "confidence": c["confidence"], "row": c["row"],
                "target_opcode": c.get("target_opcode"),
                "window_delete_ops": c.get("window_delete_ops"),
            } for c in sorted(callers, key=lambda x: x["fuzzy"])],
        })

    # rank: strong-caller count, then distinct callers, then fuzzy-gap
    rows.sort(key=lambda r: (r["n_strong_callers"], r["n_callers"],
                             r["fuzzy_gap_sum"]), reverse=True)

    # reverse-lever groups (secondary, not ranked into the main table)
    rev_groups = defaultdict(set)
    for r in all_reverse:
        rev_groups[r["callee"]].add(r["caller"])
    reverse_rows = [{
        "callee": k, "demangled": qualified(k, dbmap),
        "n_callers": len(v), "callers": sorted(v),
    } for k, v in rev_groups.items()]
    reverse_rows.sort(key=lambda r: r["n_callers"], reverse=True)

    actionable = [r for r in rows if r["actionability"] == "ACTIONABLE"]
    actionable_callers = sum(r["n_callers"] for r in actionable)

    doc = {
        "meta": {
            "pool_size": len(pool),
            "min_fuzzy": args.min_fuzzy,
            "max_fuzzy": args.max_fuzzy,
            "scan_seconds": round(scan_secs, 1),
            "total_hit_sites": len(all_hits),
            "distinct_callees": len(rows),
            "actionable_callees": len(actionable),
            "actionable_distinct_callers": actionable_callers,
            "reverse_lever_callees": len(reverse_rows),
        },
        "candidates": rows,
        "reverse_lever": reverse_rows,
    }
    Path(args.out_json).parent.mkdir(parents=True, exist_ok=True)
    json.dump(doc, open(args.out_json, "w"), indent=2)

    # ── human table ──────────────────────────────────────────────────────────
    def fmt_table(rows, top):
        out = []
        hdr = ("rank  N  strong  fuzzygap  flag                  "
               "callee (Class::method)                header")
        out.append(hdr)
        out.append("-" * len(hdr))
        for idx, r in enumerate(rows[:top], 1):
            hp = r["header"] or ""
            if hp:
                hp = os.path.relpath(hp, REPO)
            out.append("%4d  %2d  %6d  %8.2f  %-20s  %-36s  %s" % (
                idx, r["n_callers"], r["n_strong_callers"], r["fuzzy_gap_sum"],
                r["actionability"], r["demangled"][:36], hp))
        return "\n".join(out)

    table = fmt_table(rows, args.top)
    md = []
    md.append("# Missing-virtual candidates\n")
    md.append("Scanned **%d** near-miss callers (fuzzy %.2f-%.2f, strict<100) "
              "in %.1fs.\n" % (len(pool), args.min_fuzzy, args.max_fuzzy,
                               scan_secs))
    md.append("- total signature hit-sites: **%d**" % len(all_hits))
    md.append("- distinct callee methods: **%d**" % len(rows))
    md.append("- ACTIONABLE callees (non-virtual decl on polymorphic class): "
              "**%d** covering **%d** distinct callers\n" %
              (len(actionable), actionable_callers))
    md.append("```")
    md.append(table)
    md.append("```")
    md.append("\n## Reverse-lever (we call indirect, retail direct) -- "
              "informational, %d callees\n" % len(reverse_rows))
    for r in reverse_rows[:20]:
        md.append("- %s  (%d callers)" % (r["demangled"], r["n_callers"]))
    Path(args.out_md).write_text("\n".join(md) + "\n")

    print(table)
    print()
    print("[summary] pool=%d  hit-sites=%d  distinct-callees=%d  "
          "ACTIONABLE=%d (covering %d callers)  reverse=%d  %.1fs" % (
              len(pool), len(all_hits), len(rows), len(actionable),
              actionable_callers, len(reverse_rows), scan_secs))
    print("[out] %s" % args.out_json)
    print("[out] %s" % args.out_md)


if __name__ == "__main__":
    main()
