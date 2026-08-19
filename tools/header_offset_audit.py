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


def tus_from_ninja(project_dir):
    """Every compiled .cpp, read from build.ninja's own msvc/msvc_pch edges.

    ⛔ Read the TU list from BUILD.NINJA, not from objects.json.  objects.json
    keys are BARE FILENAMES whose directory lives in a group's ``src_dir``
    option, so reconstructing paths from it is the documented trap that has
    broken four separate lanes (MISPIN-1, PINFIX-1, NOOBJ-1, ...).  build.ninja
    is what actually compiles, i.e. it is the same surface the ruler uses.
    ⚠ Ninja wraps long edges with a trailing ``$``, putting the source on the
    NEXT line -- a naive single-line regex finds 14 edges instead of 1,205 and
    yields a 1%-coverage sweep that looks like it worked.  Join continuations
    first.  Self-validation: this should return ~1,204 compiled objects.
    """
    import re
    txt = open(os.path.join(project_dir, "build.ninja"),
               errors="replace").read().replace("$\n", "")
    srcs = re.findall(r"^build\s+\S+\.obj:\s+msvc(?:_pch)?\s+(\S+\.cpp)\b",
                      txt, re.M)
    out = []
    for s in sorted(set(srcs)):
        if not s.startswith("src/") or "/xdk/" in s or "/stlport/" in s:
            continue
        if os.path.exists(os.path.join(project_dir, s)):
            out.append(s)
    return out


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
    ap.add_argument("--ledger", help="JSONL progress ledger (default .header_audit.jsonl)")
    args = ap.parse_args()

    pd = os.path.abspath(args.project_dir)
    tus = args.tu or tus_from_ninja(pd)
    if args.limit:
        tus = tus[:args.limit]

    # ⛔ APPEND PER TU, RESUME ON RESTART.  A sweep that buffers findings and
    # writes once at the end loses EVERYTHING to a kill -- measured 2026-08-18:
    # this tool's first run was killed before its 25-TU progress checkpoint and
    # produced a completely empty log despite real work.  Each TU's result is
    # now flushed to a JSONL ledger immediately, and an existing ledger is
    # replayed so a restart skips finished TUs.
    ledger = args.ledger or os.path.join(pd, ".header_audit.jsonl")
    done, seen_class, findings, failed = set(), {}, {}, []
    if os.path.exists(ledger):
        with open(ledger, errors="replace") as fh:
            for line in fh:
                try:
                    rec = json.loads(line)
                except json.JSONDecodeError:
                    continue          # torn last line from a kill -- expected
                done.add(rec["tu"])
                if rec.get("err"):
                    failed.append((rec["tu"], rec["err"]))
                for cls, (hdr, bad) in (rec.get("hits") or {}).items():
                    seen_class[cls] = hdr
                    if bad:
                        findings.setdefault(hdr, {})[cls] = bad
                seen_class.update({c: h for c, h in (rec.get("seen") or {}).items()})
    todo = [t for t in tus if t not in done]
    print(f"project_dir={pd}\nTUs total {len(tus)}  already done {len(done)}  "
          f"to audit now {len(todo)}\nledger: {ledger}", flush=True)

    audited_tus = len(done) - len(failed)
    lf = open(ledger, "a")
    for i, tu in enumerate(todo, 1):
        rec = {"tu": tu, "hits": {}, "seen": {}}
        parsed, err = audit_tu(pd, tu)
        if parsed is None:
            rec["err"] = err
            failed.append((tu, err))
        else:
            audited_tus += 1
            for cls, info in (parsed.get("classes") or {}).items():
                if cls in seen_class or not isinstance(info, dict):
                    continue
                if not info.get("members"):
                    continue
                # find_header returns a LIST, sorted best-first (stem==class,
                # then shortest path).  Take the best hit.
                hdrs = clr.find_header(pd, cls.split("::")[-1])
                if not hdrs:
                    continue
                hdr = hdrs[0]
                # OUR source only -- vendor/CRT/STL carry no // 0xHEX contract
                if not (hdr.startswith("src/") and "/xdk/" not in hdr
                        and "/stlport/" not in hdr):
                    continue
                seen_class[cls] = hdr
                rec["seen"][cls] = hdr
                bad = clr.audit_header(pd, hdr, info, cls.split("::")[-1])
                if bad:
                    findings.setdefault(hdr, {})[cls] = bad
                    rec["hits"][cls] = [hdr, bad]
        lf.write(json.dumps(rec) + "\n")
        lf.flush()
        os.fsync(lf.fileno())
        print(f"  [{i}/{len(todo)}] {tu}  classes={len(seen_class)} "
              f"bad_headers={len(findings)}"
              + (f"  ERR {rec.get('err')}" if rec.get("err") else ""),
              flush=True)
    lf.close()

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
