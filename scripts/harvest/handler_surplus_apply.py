#!/usr/bin/env python3
"""handler_surplus_apply.py -- delete PROVEN-ABSENT surplus handler arms from
BEGIN_HANDLERS blocks, driven by handler_surplus_census.py's JSON.

Only arms whose evidence == "absent" (name >= 8 chars AND not found anywhere in
band.exe) are removed.  "present" and "tooshort" arms are left alone even when
the paired instrument called them surplus -- ⚠ NEVER gate speculatively; a prior
lane's speculative gating cost -68 when it had to be ungated.

Deletion is paren-balanced from the macro name, because clang-format wraps long
HANDLE_ACTION bodies across lines (a line-anchored delete would leave dangling
arguments that still compile but change codegen).

Usage:
  handler_surplus_apply.py --census census.json --proj <wt> [--only Cls,Cls] [--dry]
"""
import argparse
import json
import re
import sys
from pathlib import Path


def find_block(text, cls):
    """-> (start_idx, end_idx) of the BEGIN_HANDLERS(cls) .. END_HANDLERS body."""
    m = re.search(r"BEGIN_HANDLERS\(\s*%s\s*\)" % re.escape(cls), text)
    if not m:
        return None
    e = re.search(r"END_HANDLERS", text[m.end():])
    if not e:
        return None
    return m.end(), m.end() + e.start()


def delete_arm(text, cls, sym):
    """Delete the `MACRO(sym, ...)` invocation inside BEGIN_HANDLERS(cls)."""
    span = find_block(text, cls)
    if not span:
        return text, "no-block"
    s, e = span
    body = text[s:e]
    pat = re.compile(
        r"[ \t]*\b(HANDLE|HANDLE_EXPR|HANDLE_ACTION|HANDLE_ACTION_IF|"
        r"HANDLE_ACTION_IF_ELSE|HANDLE_EXPR_STATIC|HANDLE_ACTION_STATIC)"
        r"\s*\(\s*%s\s*," % re.escape(sym))
    m = pat.search(body)
    if not m:
        return text, "no-arm"
    # balance parens from the opening '(' of the macro
    i = body.index("(", m.start())
    depth = 0
    j = i
    while j < len(body):
        if body[j] == "(":
            depth += 1
        elif body[j] == ")":
            depth -= 1
            if depth == 0:
                break
        j += 1
    if depth != 0:
        return text, "unbalanced"
    j += 1
    # swallow the trailing newline so no blank line is left behind
    while j < len(body) and body[j] in " \t":
        j += 1
    if j < len(body) and body[j] == "\n":
        j += 1
    newbody = body[:m.start()] + body[j:]
    return text[:s] + newbody + text[e:], "ok"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--census", required=True)
    ap.add_argument("--proj", required=True)
    ap.add_argument("--only", default="")
    ap.add_argument("--dry", action="store_true")
    ap.add_argument("--allow-present", action="store_true",
                    help="ALSO remove surplus arms whose name occurs elsewhere "
                         "in band.exe.  Weaker: rests on the PAIRED instrument "
                         "alone (the absence instrument cannot speak for a name "
                         "another class also uses).  Measure these separately.")
    ap.add_argument("--ignore-kind", action="store_true",
                    help="Include BOTH-shaped rows (retail also has arms we lack).")
    a = ap.parse_args()
    proj = Path(a.proj).resolve()
    rows = json.load(open(a.census))
    only = set(x for x in a.only.split(",") if x)

    edits = {}
    plan = []
    for r in rows:
        if only:
            if r["cls"] not in only:
                continue
        elif not r.get("actionable"):
            continue
        ok_ev = {"absent"} | ({"present"} if a.allow_present else set())
        arms = [s["sym"] for s in r["surplus"] if s["evidence"] in ok_ev]
        if not arms:
            continue
        plan.append((r["src"], r["cls"], arms, r["pct"]))

    nfile = 0
    ndel = 0
    for src, cls, arms, pct in plan:
        p = proj / src
        text = edits.get(src)
        if text is None:
            text = p.read_text(errors="replace")
        done, failed = [], []
        for sym in arms:
            text, st = delete_arm(text, cls, sym)
            (done if st == "ok" else failed).append((sym, st))
        edits[src] = text
        ndel += len(done)
        print("%-28s %-26s pct=%-6s  removed %d/%d %s"
              % (cls, Path(src).name, pct, len(done), len(arms),
                 ("FAILED:" + str(failed)) if failed else ""))
    if not a.dry:
        for src, text in edits.items():
            (proj / src).write_text(text)
            nfile += 1
    print("\n%d arms removed across %d files%s"
          % (ndel, len(edits), " (DRY RUN, nothing written)" if a.dry else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
