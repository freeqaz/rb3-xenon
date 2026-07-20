#!/usr/bin/env python3
"""Per-fn string walk across a .text span for caveat-A sub-split checks.
Usage: fp2_span.py 0x82XXXXXX 0x82YYYYYY
Prints each fn VA (+size) in the span with its distinctive strings, so you can
cut the span at a string-family boundary (co-located unwired TUs over-group).
"""
import json, sys
ROOT = "/home/free/code/milohax/rb3-xenon"
lo = int(sys.argv[1], 16); hi = int(sys.argv[2], 16)
fp = json.load(open(f"{ROOT}/fingerprints.json"))
rows = sorted(((int(k, 16), v['size'], v.get('strings', [])) for k, v in fp.items()), key=lambda x: x[0])
n = 0
for va, sz, strs in rows:
    if lo <= va < hi:
        n += 1
        keep = [s for s in strs if len(s) >= 3][:8]
        print(f"{va:08x} +{sz:<5d} {' | '.join(keep)}")
print(f"# {n} fns in [{lo:08x},{hi:08x})", file=sys.stderr)
