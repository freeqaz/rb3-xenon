#!/usr/bin/env python3
"""Scan target (retail) asm for CHAINED stream-operator call sites.

Signature: two consecutive `bl` instructions with NO write to r3 in between.
Since operator>>/<< return the stream in r3, an un-reloaded r3 at the second
call means the source expression was CHAINED (`bs >> a >> b`), not two
statements.  Restricted to callees whose mangled name returns AAVBinStream@@.

Keys everything on the `.fn fn_<ADDR>` symbol, never the (synthetic) address
column -- see CLAUDE.md.
"""
import json, os, re, sys, collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASM = os.path.join(ROOT, 'build/45410914/asm')
SYMMAP = json.load(open(os.path.join(ROOT, 'scripts/target_symbol_map.json')))
# normalise map keys to lowercase 0x-hex
SYM = {k.lower(): v for k, v in SYMMAP.items()}

FN_RE = re.compile(r'^\.fn\s+(fn_[0-9A-Fa-f]+)')
ENDFN_RE = re.compile(r'^\.endfn')
# instruction lines look like:  /* ADDR OFF  BYTES */\tmnemonic ops
INSN_RE = re.compile(r'^/\*[^*]*\*/\s*(\S+)\s*(.*)$')

# a callee that returns BinStream& can be chained
def is_stream_op(name):
    if not name:
        return False
    # free operator>> / operator<< returning AAVBinStream@@
    if name.startswith('??5') or name.startswith('??6'):
        return 'AAVBinStream@@' in name
    # template forms ??$?5 / ??$?6
    if name.startswith('??$?5') or name.startswith('??$?6'):
        return 'AAVBinStream@@' in name
    return False

WRITES_R3 = re.compile(r'^r3\b')

def writes_r3(mnem, ops):
    if mnem.startswith('bl') or mnem.startswith('b') and mnem in ('b', 'bctrl', 'blrl'):
        return True   # any call clobbers r3 (handled separately)
    # destination is first operand for the loads/moves/arith we care about
    first = ops.split(',')[0].strip()
    if first == 'r3':
        return True
    return False

def scan_file(path):
    """yield (fn_symbol, [ (idx_a,name_a,idx_b,name_b) ... ])"""
    cur = None
    insns = []   # (mnem, ops)
    out = {}
    with open(path, 'r', errors='replace') as fh:
        for line in fh:
            m = FN_RE.match(line)
            if m:
                cur = m.group(1)
                insns = []
                continue
            if cur is None:
                continue
            if ENDFN_RE.match(line):
                sites = find_chains(insns)
                if sites:
                    out.setdefault(cur, []).extend(sites)
                cur = None
                continue
            mi = INSN_RE.match(line.strip())
            if mi:
                insns.append((mi.group(1), mi.group(2)))
    return out

BL_RE = re.compile(r'^bl$')

def callee_name(ops):
    t = ops.strip()
    if t.startswith('fn_'):
        return SYM.get('0x' + t[3:].lower())
    return t   # already-named symbol in the asm

def find_chains(insns):
    sites = []
    last_bl = None      # index of previous bl
    last_name = None
    r3_written_since = True
    for i, (mnem, ops) in enumerate(insns):
        if mnem == 'bl':
            name = callee_name(ops)
            if (last_bl is not None and not r3_written_since
                    and is_stream_op(name) and is_stream_op(last_name)):
                sites.append((last_name, name))
            last_bl = i
            last_name = name
            r3_written_since = False
            continue
        # any other branch-with-link / indirect call clobbers the chain
        if mnem in ('bctrl', 'blrl', 'bla'):
            last_bl = None
            r3_written_since = True
            continue
        if writes_r3(mnem, ops):
            r3_written_since = True
    return sites

def main():
    allsites = {}
    for fname in sorted(os.listdir(ASM)):
        if not fname.endswith('.s'):
            continue
        path = os.path.join(ASM, fname)
        res = scan_file(path)
        for fn, sites in res.items():
            allsites.setdefault(fname, {})[fn] = sites
    json.dump(allsites, open(os.path.join(ROOT, 'chain_sites.json'), 'w'), indent=0)
    nfn = sum(len(v) for v in allsites.values())
    nsite = sum(len(s) for v in allsites.values() for s in v.values())
    print(f'files with chains: {len(allsites)}   functions: {nfn}   chain sites: {nsite}')

if __name__ == '__main__':
    main()
