#!/usr/bin/env python3
"""header_offset_audit.py -- tree-wide audit of `// 0xHEX` header comments.

WHY
---
Header offset comments are **derived from nobody**.  Three classes have been
caught with wrong ones, each by accident while chasing something else:
``PreloadPanel.h`` (EVERY offset stale by 4 -- an uncounted ``{vfptr}`` for the
``ContentMgr::Callback`` base), ``CharEyes.h`` (20 wrong), ``SaveLoadManager.h``
(uniformly +4).  Nobody had ever swept for it.

⛔ AND THE BLAST RADIUS IS LARGER THAN THE COMMENTS.  ``tools/struct_db.py``
parses those same comments into ``struct_db.sqlite``, which is what the
orchestrator MCP tool ``lookup_struct_offset`` reads.  A wrong comment therefore
propagates into a tool agents trust for layout decisions.  **A wrong comment is
not cosmetic -- it is a lane-misdirecting instrument.**

HOW THIS IS AFFORDABLE
----------------------
★ The cost is per **TU**, not per class.  ``--all-classes``
(``/d1reportAllClassLayout``) returns EVERY class in the TU from ONE compile --
measured **2,026 classes in ~1 s** here -- and ``audit_header()`` is pure Python
over the parsed dict.  A 2026-08-18 lane brief asserted "budget 60 min per class,
sweep narrowly"; that was wrong by orders of magnitude and is corrected in
``class_layout_report.py``'s docstring.  A tree-wide sweep is cheap.

WHAT IT DOES *NOT* DO
---------------------
⚠ This audits our comments against **our own compiler**.  It finds
`comment disagrees with OUR layout`.  It says NOTHING about whether our layout
matches **retail** -- that needs a retail witness (an instruction loading a
specific offset, with a ``.pdata``-confirmed extent).  Keep the two claims
strictly separate:
  class A  wrong comment, right layout  -> metric-neutral, comment-only fix
  class B  wrong layout                 -> a real bug; needs A/B + native gate
**This tool only finds class A.**  Never let a B masquerade as an A.

USAGE
    python3 tools/header_offset_audit.py --project-dir <wt> [--tu-list f] [--limit N]
    python3 tools/header_offset_audit.py --project-dir <wt> --json out.json
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "scripts", "harvest"))
import class_layout_report as clr  # noqa: E402


def tus_from_objects(project_dir):
    """Every compiled .cpp declared in objects.json, in declaration order."""
    p = os.path.join(project_dir, "config", "45410914", "objects.json")
    with open(p) as fh:
        data = json.load(fh)
    out = []

    def walk(node):
        if isinstance(node, dict):
            sp = node.get("src_path") or node.get("path") or node.get("name")
            if isinstance(sp, str) and sp.endswith(".cpp"):
                out.append(sp)
            for v in node.values():
                walk(v)
        elif isinstance(node, list):
            for v in node:
                walk(v)

    walk(data)
    seen, uniq = set(), []
    for t in out:
        if t not in seen and os.path.exists(os.path.join(project_dir, t)):
            seen.add(t)
            uniq.append(t)
    return uniq


def audit_tu(project_dir, tu):
    """One bulk compile -> {class: [(line, member, commented, real), ...]}."""
    cmd = clr.build_command(project_dir, None, None, "/tmp/_hoa.obj",
                            use_pch=False, all_classes=True, tu=tu) \
        if "tu" in clr.build_command.__code__.co_varnames else None
    # The CLI is the stable surface; drive it rather than private helpers.
    import subprocess
    r = subprocess.run(
        [sys.executable,
         os.path.join(project_dir, "scripts", "harvest", "class_layout_report.py"),
         "--all-classes", "--tu", tu, "--project-dir", project_dir, "--json"],
        capture_output=True, text=True, timeout=3600)
    if r.returncode not in (0, 1) or not r.stdout.strip():
        return None, f"rc={r.returncode} {r.stderr.strip()[:160]}"
    try:
        parsed = json.loads(r.stdout)
    except json.JSONDecodeError as e:
        return None, f"json: {e}"
    if parsed.get("status") == "COMPILE_FAILED":
        return None, "COMPILE_FAILED"
    return parsed, None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--project-dir", required=True)
    ap.add_argument("--limit", type=int, default=0, help="audit only first N TUs")
    ap.add_argument("--tu", action="append", help="explicit TU (repeatable)")
    ap.add_argument("--json", help="write full findings here")
    args = ap.parse_args()

    pd = os.path.abspath(args.project_dir)
    tus = args.tu or tus_from_objects(pd)
    if args.limit:
        tus = tus[:args.limit]
    print(f"project_dir={pd}\nTUs to audit: {len(tus)}", flush=True)

    seen_class = {}       # class -> header  (first declaring header wins)
    findings = {}         # header -> {class: [rows]}
    failed = []
    audited_tus = 0

    for i, tu in enumerate(tus, 1):
        parsed, err = audit_tu(pd, tu)
        if parsed is None:
            failed.append((tu, err))
            continue
        audited_tus += 1
        for cls, info in (parsed.get("classes") or {}).items():
            if cls in seen_class or not isinstance(info, dict):
                continue
            if not info.get("members"):
                continue
            # find_header returns a LIST, sorted best-first (stem==class, then
            # shortest path).  Take the best hit; skip ambiguous-to-zero cases.
            hdrs = clr.find_header(pd, cls.split("::")[-1])
            if not hdrs:
                continue
            hdr = hdrs[0]
            # OUR source only -- vendor/CRT/STL headers carry no // 0xHEX contract
            if not (hdr.startswith("src/") and "/xdk/" not in hdr
                    and "/stlport/" not in hdr):
                continue
            seen_class[cls] = hdr
            bad = clr.audit_header(pd, hdr, info, cls.split("::")[-1])
            if bad:
                findings.setdefault(hdr, {})[cls] = bad
        if i % 25 == 0 or i == len(tus):
            print(f"  [{i}/{len(tus)}] tus_ok={audited_tus} "
                  f"classes={len(seen_class)} bad_headers={len(findings)}",
                  flush=True)

    total_rows = sum(len(r) for h in findings.values() for r in h.values())
    print("\n" + "=" * 72)
    print(f"TUs audited     : {audited_tus}/{len(tus)}  (failed {len(failed)})")
    print(f"classes examined: {len(seen_class)}")
    print(f"headers WRONG   : {len(findings)}")
    print(f"wrong comments  : {total_rows}")
    print("=" * 72)

    for hdr in sorted(findings, key=lambda h: -sum(len(v) for v in findings[h].values())):
        rows = findings[hdr]
        n = sum(len(v) for v in rows.values())
        print(f"\n{hdr}  ({n} wrong)")
        for cls, bad in rows.items():
            deltas = {r[3] - r[2] for r in bad}
            uni = f"  UNIFORM delta {list(deltas)[0]:+d}" if len(deltas) == 1 else ""
            print(f"  class {cls}{uni}")
            for line, member, commented, real in bad[:12]:
                print(f"    L{line:<5} {member:<28} says 0x{commented:<4x} "
                      f"real 0x{real:<4x}  ({real - commented:+d})")
            if len(bad) > 12:
                print(f"    ... {len(bad) - 12} more")

    if failed:
        print(f"\nfailed TUs ({len(failed)}), first 10:")
        for tu, err in failed[:10]:
            print(f"  {tu}: {err}")

    if args.json:
        with open(args.json, "w") as fh:
            json.dump({"findings": findings, "failed": failed,
                       "classes_examined": len(seen_class),
                       "tus_audited": audited_tus}, fh, indent=1)
        print(f"\nwrote {args.json}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
