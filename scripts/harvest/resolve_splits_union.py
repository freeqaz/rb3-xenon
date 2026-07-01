#!/usr/bin/env python3
"""Resolve a conflicted config/45410914/splits.txt during a wave rebase by
taking a UNIT-AWARE line-union: ours + every line theirs added vs the merge
base, grafted into the SAME unit block it came from.

splits.txt is unit-structured (`Foo.cpp:` header, then indented range lines);
dtk attributes every range line to the nearest unit header ABOVE it. The old
resolver appended all of theirs' new lines as one flat block at EOF, which
silently re-attributed them to whatever unit happened to be last in ours
(2026-07-01: the w3-pins lane's ~137 micro-pins across 47 TUs all landed
inside the GemTrackDir block -> every pinned fn reported 0% "in GemTrackDir").

Rules:
- theirs' new lines in a unit ours also has -> appended at the end of ours'
  block for that unit.
- theirs' new units -> whole block appended at EOF.
- No removals are propagated (coordinator handles boundary edits manually).
- The coordinator MUST still run the splits overlap self-check after this.
  See docs/decomp/handoff/wave-loop-SOP-*.md step 4.

Usage: resolve_splits_union.py <worktree>
Exit 0 on success (file rewritten, staged by caller), 2 if a stage is missing.
"""
import sys, subprocess, os, re, collections

wt = sys.argv[1]
rel = "config/45410914/splits.txt"


def stage(n):
    r = subprocess.run(["git", "-C", wt, "show", f":{n}:{rel}"],
                       capture_output=True, text=True)
    return r.stdout if r.returncode == 0 else None


def parse_units(text):
    """OrderedDict unit -> [range lines]; lines before any header live under None."""
    units = collections.OrderedDict()
    cur = None
    for ln in (text or "").splitlines():
        if re.match(r"^\S.*:$", ln):
            cur = ln[:-1]
            units.setdefault(cur, [])
        elif ln.strip():
            units.setdefault(cur, []).append(ln)
    return units


base_t, ours_t, theirs_t = stage(1), stage(2), stage(3)
if ours_t is None or theirs_t is None:
    print("missing stage")
    sys.exit(2)

base_u, theirs_u = parse_units(base_t), parse_units(theirs_t)

# per-unit lines theirs added vs base
added = collections.OrderedDict()
for u, lines in theirs_u.items():
    old = set(base_u.get(u, []))
    new = [l for l in lines if l not in old]
    if new:
        added[u] = new

ours_u = parse_units(ours_t)
total = 0
result = []
cur = None
for ln in ours_t.splitlines() + ["<<EOF>>"]:
    at_hdr = ln == "<<EOF>>" or re.match(r"^\S.*:$", ln)
    if at_hdr and cur is not None and cur in added:
        have = set(ours_u.get(cur, []))
        graft = [l for l in added.pop(cur) if l not in have]
        # insert before any trailing blank lines of the closing block
        tail = []
        while result and result[-1].strip() == "":
            tail.append(result.pop())
        result.extend(graft)
        result.extend(tail)
        total += len(graft)
    if ln != "<<EOF>>":
        if at_hdr:
            cur = ln[:-1]
        result.append(ln)

# units theirs has that ours doesn't: append whole blocks at EOF
for u, lines in added.items():
    if u is None:
        continue
    result.append("")
    result.append(u + ":")
    result.extend(lines)
    total += len(lines)

open(os.path.join(wt, rel), "w").write("\n".join(result) + "\n")
print(f"  splits union: +{total} lines from theirs (unit-aware)")
