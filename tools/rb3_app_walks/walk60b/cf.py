#!/usr/bin/env python3
# cf.py run.log...: each change of p0's 'coda a/b/c pts N' and the band 'bcoda x/y' fields, with frame and beat
import re,sys
for fn in sys.argv[1:]:
    prev=None; print('==',fn)
    for l in open(fn,'rb'):
        l=l.decode('latin-1')
        if not l.startswith('rb3-app-det: frame'): continue
        f=l.split()[2]; b=re.search(r' beat ([\d.]+)',l)
        c=re.search(r'coda (\d+/\d+/\d+) pts (\d+)',l); bc=re.search(r'bcoda (\d+/\d+)',l)
        cur=(c.groups() if c else None, bc.group(1) if bc else None)
        if cur!=prev and (c or bc):
            s=re.search(r'score (\d+) hit (\d+) miss (\d+)',l)
            print(f'  f{f} beat {b.group(1) if b else "?"} coda {cur[0]} bcoda {cur[1]} {s.groups() if s else ""}')
        prev=cur
