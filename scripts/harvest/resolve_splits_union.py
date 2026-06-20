#!/usr/bin/env python3
"""Resolve a conflicted config/45410914/splits.txt during a wave rebase by
taking the LINE-union: ours + every line theirs added vs the merge base.

Each wave lane appends an independent per-TU pin block (.text/.pdata ranges),
so the correct resolution is "keep all of ours and graft theirs' new lines".
The coordinator MUST still run the splits overlap self-check after this (two
independent lanes can pin overlapping ranges — that needs a human/coordinator
fix, not a union). See docs/decomp/handoff/wave-loop-SOP-*.md step 4.

Usage: resolve_splits_union.py <worktree>
Exit 0 on success (file rewritten, staged by caller), 2 if a stage is missing.
"""
import sys, subprocess, os

wt = sys.argv[1]
rel = "config/45410914/splits.txt"


def stage(n):
    r = subprocess.run(["git", "-C", wt, "show", f":{n}:{rel}"],
                       capture_output=True, text=True)
    return r.stdout.splitlines(keepends=True) if r.returncode == 0 else None


base, ours, theirs = stage(1), stage(2), stage(3)
if ours is None or theirs is None:
    print("missing stage")
    sys.exit(2)

baseset = set(base or [])
ours_set = set(ours)
# lines theirs added vs base, not already in ours
added = [l for l in theirs if l not in baseset and l not in ours_set]
out = ours[:]
if added:
    if out and out[-1].strip() == "":
        out = out[:-1] + ["\n"] + added + ["\n"]
    else:
        out = out + ["\n"] + added

open(os.path.join(wt, rel), "w").writelines(out)
print(f"  splits union: +{len(added)} lines from theirs")
