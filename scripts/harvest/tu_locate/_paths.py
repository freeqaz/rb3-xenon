"""laneBD tu_locate: shared paths. Override scratch with $TU_LOCATE_SCRATCH."""
import os as _os, sys as _sys
SCRATCH = _os.environ.get('TU_LOCATE_SCRATCH', _os.path.expanduser('~/tmp/tu_locate'))
_os.makedirs(SCRATCH, exist_ok=True)
_HERE = _os.path.dirname(_os.path.abspath(__file__))
REPO = _os.path.abspath(_os.path.join(_HERE, '..', '..', '..'))
BANDEXE = _os.path.join(REPO, 'orig', '45410914', 'band.exe')
if _HERE not in _sys.path: _sys.path.insert(0, _HERE)
