#!/usr/bin/env python3
"""header_offset_audit_par.py -- two-phase parallel driver for the header
`// 0xHEX` offset-comment audit.

WHY A SECOND DRIVER
-------------------
``tools/header_offset_audit.py`` is strictly serial and re-``grep -rlP``s
``src/`` for a header once per class.  On this tree that is 1,205 TUs x ~14 s
= ~4.7 h of wall clock, plus tens of thousands of grep invocations.  Splitting
it into two phases makes it ~15 min and -- more importantly -- makes phase 2
**re-runnable in seconds**, so an audit RULE can be corrected without paying
for 1,205 compiles again.

  phase 1 (parallel, expensive): one ``cl.exe /d1reportAllClassLayout`` per TU,
          parsed layout cached to ``<cache>/<tu>.json.gz``.
  phase 2 (serial, seconds):     header index built ONCE, then
          ``class_layout_report.audit_header`` per class.

That split is not a nicety.  The shadowed-base-member defect fixed in
``audit_header`` on 2026-09-01 (a base class's member of the same name winning
over the derived class's own) was found *after* a full sweep had already been
paid for; with this driver, re-auditing under the corrected rule cost
``--phase2-only`` and a few seconds instead of another overnight run.

⚠ WHAT THIS TOOL FINDS -- read ``header_offset_audit.py``'s docstring in full.
It compares our header COMMENTS against OUR OWN COMPILER.  Every row means
"comment disagrees with our layout".  It cannot, by construction, tell you
whether *our layout* matches retail; that needs a target-binary witness.
**This tool only finds class A.  Never let a B masquerade as an A.**

USAGE
    python3 tools/header_offset_audit_par.py --project-dir <wt> --json out.json
    python3 tools/header_offset_audit_par.py --project-dir <wt> --json out.json \
            --phase2-only        # re-audit the cache after a rule change
"""
import argparse
import concurrent.futures as cf
import gzip
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "scripts", "harvest"))
import class_layout_report as clr  # noqa: E402

sys.path.insert(0, HERE)
from header_offset_audit import tus_from_ninja  # noqa: E402


def layout_for_tu(project_dir, tu, cache_dir):
    """Parsed layout for one TU, from cache or from one bulk compile."""
    cp = os.path.join(cache_dir, tu.replace("/", "_") + ".json.gz")
    if os.path.exists(cp):
        try:
            with gzip.open(cp, "rt") as fh:
                return json.load(fh), None
        except (OSError, json.JSONDecodeError):
            os.unlink(cp)                      # torn cache entry -- recompute
    r = subprocess.run(
        [sys.executable,
         os.path.join(project_dir, "scripts", "harvest", "class_layout_report.py"),
         "--all-classes", "--tu", tu, "--project-dir", project_dir, "--json"],
        capture_output=True, text=True, timeout=5400)
    if r.returncode not in (0, 1) or not r.stdout.strip():
        return None, f"rc={r.returncode} {(r.stderr or '').strip()[:200]}"
    try:
        parsed = json.loads(r.stdout)
    except json.JSONDecodeError as e:
        return None, f"json: {e}"
    if parsed.get("status") == "COMPILE_FAILED":
        return None, "COMPILE_FAILED"
    # Cache only what phase 2 reads, or the cache is ~10x larger than it needs.
    slim = {"classes": {k: {"name": v.get("name"), "size": v.get("size"),
                            "members": v.get("members") or []}
                        for k, v in (parsed.get("classes") or {}).items()
                        if isinstance(v, dict) and v.get("members")}}
    tmp = cp + ".tmp"
    with gzip.open(tmp, "wt") as fh:
        json.dump(slim, fh)
    os.replace(tmp, cp)                        # atomic: no torn cache on kill
    return slim, None


def build_header_index(project_dir):
    """{class_name: best header} for every `class X`/`struct X` under src/.

    ONE walk instead of `grep -rlP` per class.  Ranking mirrors
    clr.find_header: same-stem header first, then shortest path.
    """
    pat = re.compile(r"^\s*(?:class|struct)\s+(?:[A-Z_][A-Z0-9_]*\s+)?"
                     r"([A-Za-z_][A-Za-z0-9_]*)\s*(?::|\{|$)", re.M)
    idx = {}
    for root, dirs, files in os.walk(os.path.join(project_dir, "src")):
        dirs[:] = [d for d in dirs if d not in ("stlport", "xdk")]
        for fn in files:
            if not fn.endswith((".h", ".hpp")):
                continue
            p = os.path.join(root, fn)
            rel = os.path.relpath(p, project_dir)
            if "/xdk/" in rel or "/stlport/" in rel:
                continue
            try:
                txt = open(p, errors="replace").read()
            except OSError:
                continue
            stem = os.path.splitext(fn)[0]
            for cls in set(pat.findall(txt)):
                rank = (0 if cls == stem else 1, len(rel))
                if cls not in idx or rank < idx[cls][0]:
                    idx[cls] = (rank, rel)
    return {k: v[1] for k, v in idx.items()}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--project-dir", required=True)
    ap.add_argument("--jobs", type=int, default=12)
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--cache", default=None)
    ap.add_argument("--json", required=True)
    ap.add_argument("--phase2-only", action="store_true",
                    help="skip compiles; audit whatever is already cached")
    args = ap.parse_args()

    pd = os.path.abspath(args.project_dir)
    cache = args.cache or os.path.join(pd, ".layout_cache")
    os.makedirs(cache, exist_ok=True)
    tus = tus_from_ninja(pd)
    if args.limit:
        tus = tus[:args.limit]

    failed = []
    if not args.phase2_only:
        done = 0
        with cf.ThreadPoolExecutor(max_workers=args.jobs) as ex:
            futs = {ex.submit(layout_for_tu, pd, t, cache): t for t in tus}
            for f in cf.as_completed(futs):
                tu = futs[f]
                done += 1
                try:
                    _parsed, err = f.result()
                except Exception as e:                    # noqa: BLE001
                    err = f"{type(e).__name__}: {e}"
                if err:
                    failed.append((tu, err))
                if done % 25 == 0 or done == len(tus):
                    print(f"  [{done}/{len(tus)}] failed={len(failed)}", flush=True)

    print("building header index ...", flush=True)
    hidx = build_header_index(pd)
    print(f"  {len(hidx)} classes declared in src/ headers", flush=True)

    findings, seen_class, no_header, not_ours = {}, {}, set(), set()
    for tu in tus:
        cp = os.path.join(cache, tu.replace("/", "_") + ".json.gz")
        if not os.path.exists(cp):
            continue
        try:
            with gzip.open(cp, "rt") as fh:
                parsed = json.load(fh)
        except (OSError, json.JSONDecodeError):
            continue
        for cls, info in (parsed.get("classes") or {}).items():
            if cls in seen_class:
                continue
            short = cls.split("::")[-1]
            hdr = hidx.get(short)
            if not hdr:
                no_header.add(cls)
                continue
            if not hdr.startswith("src/"):
                not_ours.add(cls)
                continue
            seen_class[cls] = hdr
            bad = clr.audit_header(pd, hdr, info, short)
            if bad:
                findings.setdefault(hdr, {})[cls] = bad

    total = sum(len(r) for h in findings.values() for r in h.values())
    print("\n" + "=" * 72)
    print(f"TUs                 : {len(tus)}  (compile failures {len(failed)})")
    print(f"classes examined    : {len(seen_class)}")
    print(f"classes w/o header  : {len(no_header)}")
    print(f"headers with rows   : {len(findings)}")
    print(f"disagreeing comments: {total}")
    print("=" * 72)

    for hdr in sorted(findings, key=lambda h: -sum(len(v) for v in findings[h].values())):
        rows = findings[hdr]
        n = sum(len(v) for v in rows.values())
        print(f"\n{hdr}  ({n})")
        for cls, bad in rows.items():
            deltas = {r[3] - r[2] for r in bad}
            uni = f"  UNIFORM delta {list(deltas)[0]:+d}" if len(deltas) == 1 else ""
            print(f"  class {cls}{uni}")
            for line, member, commented, real in bad[:8]:
                print(f"    L{line:<5} {member:<26} says 0x{commented:<4x} "
                      f"real 0x{real:<4x} ({real - commented:+d})")
            if len(bad) > 8:
                print(f"    ... {len(bad) - 8} more")

    with open(args.json, "w") as fh:
        json.dump({"findings": findings, "failed": failed,
                   "classes_examined": len(seen_class),
                   "classes_without_header": sorted(no_header),
                   "tus_audited": len(tus) - len(failed)}, fh, indent=1)
    print(f"\nwrote {args.json}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
