#!/usr/bin/env python3
"""lane EA-1: which carves cross a splits.txt .text block boundary?

The split refuses a symbol that ends outside its block ("ends within symbol").
For those rows the truncation is caused by the PIN, not just the symbol size --
and extending the pin would take bytes from whatever block owns them next, so
each needs adjudicating separately rather than blanket-extending.
"""
import re, json, bisect, sys

WT = '/home/free/tmp/laneEB1/wt'
spans, cur = [], None
for line in open(f'{WT}/config/45410914/splits.txt'):
    if line.strip() and not line[0].isspace() and line.rstrip().endswith(':'):
        cur = line.strip().rstrip(':')
    else:
        m = re.search(r'\.text\s+start:0x([0-9A-Fa-f]+) end:0x([0-9A-Fa-f]+)', line)
        if m:
            spans.append((int(m.group(1), 16), int(m.group(2), 16), cur))
spans.sort()
SP = [s[0] for s in spans]

spec = json.load(open(sys.argv[1]))
print(f"{'addr':>10} {'newend':>10} {'blk_end':>10}  cross  owner_of_next_block")
print('-' * 108)
ok, cross = [], []
for c in spec:
    a = int(c['addr'], 16)
    end = a + int(c['size'], 16)
    i = bisect.bisect_right(SP, a) - 1
    s, e, u = spans[i]
    if end > e:
        j = i + 1
        nxt = spans[j] if j < len(spans) else None
        gap = (nxt[0] - e) if nxt else None
        print(f"0x{a:08X} 0x{end:08X} 0x{e:08X}  CROSS  block={u} | next block "
              f"{('0x%08X..0x%08X %s' % (nxt[0], nxt[1], nxt[2])) if nxt else 'NONE'} | gap={gap}")
        cross.append(c)
    else:
        ok.append(c)
print()
print(f"clean: {len(ok)}   crossing: {len(cross)}")
json.dump(ok, open(sys.argv[2], 'w'), indent=1)
json.dump(cross, open(sys.argv[3], 'w'), indent=1)
print(f"wrote {sys.argv[2]} ({len(ok)}) and {sys.argv[3]} ({len(cross)})")
