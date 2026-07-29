from _paths import SCRATCH, REPO, BANDEXE, WII_SRC  # noqa: E402
import re, bisect

SPL = REPO + '/config/45410914/splits.txt'
RANGES = []  # (start, end, unit)
_unit = None
for ln in open(SPL):
    m = re.match(r'^(\S.*?):\s*$', ln)
    if m:
        _unit = m.group(1).strip()
        continue
    m = re.match(r'^\s+\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)', ln)
    if m and _unit:
        RANGES.append((int(m.group(1), 16), int(m.group(2), 16), _unit))
RANGES.sort()
_starts = [r[0] for r in RANGES]


def owner(va):
    i = bisect.bisect_right(_starts, va) - 1
    if i < 0:
        return None
    s, e, u = RANGES[i]
    return u if s <= va < e else None


def unit_ranges(unit):
    return [r for r in RANGES if r[2] == unit]


if __name__ == '__main__':
    print(len(RANGES), 'text ranges')
    import sys
    for a in sys.argv[1:]:
        print(a, owner(int(a, 16)))
