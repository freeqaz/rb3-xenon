#!/usr/bin/env python3
"""Key-delta three-way merge for ``scripts/target_symbol_map.json`` during a
lane rebase.

Usage:  python3 tools/rebase_kit/resolve_map.py <worktree>
        (run INSIDE a stopped rebase: ours = ``:2:`` (the main side),
         base = ``REBASE_HEAD~1``, theirs = ``REBASE_HEAD``)

The map is a flat ``{address: name}`` dict, so the merge is per key: every key
the LANE changed relative to its own base is applied onto HEAD's map, and every
key the lane deleted is deleted. Keys only HEAD touched are left alone. There is
no conflict arm -- a key both sides changed resolves to the lane's value, which
is the intended precedence for a lane that was just rebased onto HEAD.

⚠ Round-trip spelling is load-bearing: ``indent=1, ensure_ascii=False``.
Writing it any other way reformats the whole file and turns the next lane's
rebase into a whole-file conflict.
"""
import json, subprocess, sys, os

P = 'scripts/target_symbol_map.json'
_ABSENT = '<A>'


def show(spec):
    return json.loads(subprocess.check_output(['git', 'show', spec]))


def key_delta_apply(ours, base, theirs):
    """Apply the lane's (base->theirs) key deltas onto ``ours``, in place.

    Returns the number of deltas applied.
    """
    n = 0
    for k in set(base) | set(theirs):
        if base.get(k, _ABSENT) != theirs.get(k, _ABSENT):
            n += 1
            if k not in theirs:
                ours.pop(k, None)
            else:
                ours[k] = theirs[k]
    return n


def main(argv):
    if len(argv) != 2:
        print(__doc__)
        return 2
    os.chdir(argv[1])
    sha = subprocess.check_output(['git', 'rev-parse', 'REBASE_HEAD']).decode().strip()
    ours = show(':2:' + P)                      # HEAD (main side)
    base = show(sha + '~1:' + P)
    theirs = show(sha + ':' + P)
    n = key_delta_apply(ours, base, theirs)
    raw = subprocess.check_output(['git', 'show', ':2:' + P])
    txt = json.dumps(ours, indent=1, ensure_ascii=False) + ("\n" if raw.endswith(b"\n") else "")
    open(P, 'w').write(txt)
    print(f"{sha[:12]}: applied {n} key deltas onto HEAD's map ({len(ours)} keys)")
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
