#!/usr/bin/env python3
"""Batch-probe a list of candidate addresses (one per line, from
find_replace_candidates.py output) via Ghidra RTTI and print
address -> resolved T name for each."""
import re
import sys
sys.path.insert(0, 'scripts')
from rtti_probe import probe
sys.path.insert(0, 'tools/ghidra')
from mcp_client import create_client

def main():
    infile = sys.argv[1] if len(sys.argv) > 1 else None
    lines = open(infile).read().splitlines() if infile else sys.stdin.read().splitlines()
    c = create_client()
    for line in lines:
        line = line.strip()
        if not line:
            continue
        m = re.search(r'(fn_[0-9A-Fa-f]+)', line)
        if not m:
            continue
        fn = m.group(1)
        addr = '0x' + fn[3:]
        try:
            r = probe(addr, c)
        except Exception as e:
            print(f"{addr}  ERROR {e}")
            continue
        if r['ok']:
            print(f"{addr}  OK  {r['name']}")
        else:
            print(f"{addr}  NO_RTTI  {r.get('reason')}")

if __name__ == '__main__':
    main()
