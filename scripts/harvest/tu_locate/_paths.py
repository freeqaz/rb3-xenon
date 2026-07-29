"""laneBD tu_locate: shared paths. Override scratch with $TU_LOCATE_SCRATCH."""
import os as _os, sys as _sys, subprocess as _sp
SCRATCH = _os.environ.get('TU_LOCATE_SCRATCH', _os.path.expanduser('~/tmp/tu_locate'))
_os.makedirs(SCRATCH, exist_ok=True)
_HERE = _os.path.dirname(_os.path.abspath(__file__))
REPO = _os.path.abspath(_os.path.join(_HERE, '..', '..', '..'))
BANDEXE = _os.path.join(REPO, 'orig', '45410914', 'band.exe')


def _main_repo():
    """The MAIN checkout, even when running from a worktree -- the sibling
    oracle trees (../rb3, ../dc3-decomp) live next to it, not next to a worktree."""
    try:
        out = _sp.run(['git', '-C', REPO, 'worktree', 'list', '--porcelain'],
                      capture_output=True, text=True, check=True).stdout
        for line in out.split('\n'):
            if line.startswith('worktree '):
                return line.split(' ', 1)[1].strip()
    except Exception:
        pass
    return REPO


MAIN_REPO = _main_repo()
SIB = _os.path.dirname(MAIN_REPO)
WII_SRC = _os.path.join(SIB, 'rb3', 'src')
if _HERE not in _sys.path:
    _sys.path.insert(0, _HERE)
