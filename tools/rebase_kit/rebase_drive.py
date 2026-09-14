#!/usr/bin/env python3
"""Drive ``git rebase main`` in a lane worktree, resolving the two generated
JSON files automatically and stopping dead on anything else.

Usage:  python3 tools/rebase_kit/rebase_drive.py <worktree>

Exit codes:
  0  rebase finished (the message names how many conflict rounds it took)
  2  a git command this driver considered mandatory failed
  3  a conflict in a file this kit has NO resolver for -- resolve it by hand
  4  more than 40 conflict rounds; something is wrong, it gave up

⚠ Exit 0 after **0 conflict rounds** does NOT mean the merge was verified --
it means no resolver ever ran. See README.md, "Invariants". Three-way verify
either way.
"""
import subprocess, sys, os

HERE = os.path.dirname(os.path.abspath(__file__))
RESOLVERS = {
    'scripts/symbol_aliases.json':     os.path.join(HERE, 'resolve_aliases.py'),
    'scripts/target_symbol_map.json':  os.path.join(HERE, 'resolve_map.py'),
}


def main(argv):
    if len(argv) != 2:
        print(__doc__)
        return 2
    wt = argv[1]
    os.chdir(wt)

    def run(*a, check=False):
        r = subprocess.run(a, capture_output=True, text=True)
        if check and r.returncode:
            print('FAIL', a, r.stdout[-800:], r.stderr[-800:])
            sys.exit(2)
        return r

    run('git', 'rebase', 'main')
    for it in range(40):
        rm = run('git', 'rev-parse', '--git-path', 'rebase-merge').stdout.strip()
        if not os.path.isdir(rm):
            head = run('git', 'rev-parse', '--short=12', 'HEAD').stdout.strip()
            print('rebase finished after', it, 'conflict rounds; HEAD', head)
            if it == 0:
                print('NOTE: 0 conflict rounds => no resolver ran. Three-way '
                      'verify the generated JSON anyway (README.md).')
            return 0
        st = run('git', 'status', '--porcelain').stdout.splitlines()
        conf = [l[3:] for l in st if l[:2] in ('UU', 'AA', 'DU', 'UD')]
        head = run('git', 'rev-parse', '--short=12', 'REBASE_HEAD').stdout.strip()
        print('round', it, 'REBASE_HEAD', head, 'conflicts', conf)
        for f in conf:
            resolver = RESOLVERS.get(f)
            if resolver is None:
                print('MANUAL conflict in', f, '-- stopping for hand resolution')
                return 3
            run('python3', resolver, wt, check=True)
            run('git', 'add', f, check=True)
        r = run('git', '-c', 'core.editor=true', 'rebase', '--continue')
        tail = (r.stdout + r.stderr).strip()
        print('  continue rc', r.returncode, tail.splitlines()[-1] if tail else '')
    print('gave up')
    return 4


if __name__ == '__main__':
    sys.exit(main(sys.argv))
