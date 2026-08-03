#!/usr/bin/env python3
"""lane EA-1: WHOLE-BINARY sweep for truncated extents, using our own build as
the oracle for where the body ends.

DZ-1's queues were built from a "frameless AND last instruction is not a
terminator" filter.  That filter is a suspicion about the TARGET only.  The
thing that actually predicted all 10 landings was a two-sided agreement:

    retail's body ends exactly where OUR COMPILED body ends.

So test that directly and drop the frameless precondition entirely (it was
never the defect -- it was a proxy).  Candidate true end := addr + base_size.

  C1  base_size > claimed_size                  (the symbol is short)
  C2  the word at addr+base_size-4 is a real exit instruction, raw-decoded
      (never capstone -- it cannot see VMX128 and stops silently)
  C3  every symbol strictly inside the new extent is absorbable
      (fn_<addr> fragments and except_data_* phantom EH prefixes only)
  C4  named, pinned, currently sub-100
  C5  all objs defining the symbol agree on its size (else skip: the COMDAT
      size would be ambiguous)

Control: the same test on rows ALREADY at mpn 100 -- those must not fire,
since for a correct extent base_size == claimed_size makes C1 false.
"""
import json, re, os, bisect, sys, struct, glob
import adj

WT = adj.WT


# ---- base sizes, with agreement check across every defining obj -----------
def base_index():
    sizes = {}
    for p in glob.glob(os.path.join(WT, 'build/45410914/src/**/*.obj'), recursive=True):
        try:
            d = open(p, 'rb').read()
            if len(d) < 20:
                continue
            nsec = struct.unpack_from('<H', d, 2)[0]
            symoff = struct.unpack_from('<I', d, 8)[0]
            nsym = struct.unpack_from('<I', d, 12)[0]
            if not symoff or not nsym:
                continue
            strt = symoff + nsym * 18
            secs = []
            nfn = {}
            for i in range(nsec):
                o = 20 + i * 40
                secs.append((d[o:o + 8].rstrip(b'\0').decode('latin1'),
                             struct.unpack_from('<I', d, o + 16)[0]))
            ents = []
            i = 0
            while i < nsym:
                e = symoff + i * 18
                z = d[e:e + 8]
                if z[:4] == b'\0\0\0\0':
                    off = struct.unpack_from('<I', z, 4)[0]
                    end = d.index(b'\0', strt + off)
                    n = d[strt + off:end].decode('latin1')
                else:
                    n = z.rstrip(b'\0').decode('latin1')
                value, secnum, typ, sclass, naux = struct.unpack_from('<IhHBB', d, e + 8)
                if 0 < secnum <= len(secs) and (typ >> 4) == 0x2 and secs[secnum - 1][0].startswith('.text'):
                    ents.append((n, secnum, value))
                    nfn[secnum] = nfn.get(secnum, 0) + 1
                i += 1 + naux
            for n, secnum, value in ents:
                if value != 0 or nfn.get(secnum, 0) != 1:
                    sizes.setdefault(n, set()).add(None)   # ambiguous COMDAT
                else:
                    sizes.setdefault(n, set()).add(secs[secnum - 1][1])
        except Exception:
            continue
    return {k: (list(v)[0] if len(v) == 1 and None not in v else None)
            for k, v in sizes.items()}


BASE = base_index()

MAP = json.load(open(os.path.join(WT, 'scripts/target_symbol_map.json')))
REP = json.load(open(os.path.join(WT, 'build/45410914/report.json')))
rows = {}
for u in REP['units']:
    for f in (u.get('functions') or []):
        rows[(u['name'], f['name'])] = f

spans, cur = [], None
for line in open(os.path.join(WT, 'config/45410914/splits.txt')):
    if line.strip() and not line[0].isspace() and line.rstrip().endswith(':'):
        cur = line.strip().rstrip(':')
    else:
        m = re.search(r'\.text\s+start:0x([0-9A-Fa-f]+) end:0x([0-9A-Fa-f]+)', line)
        if m:
            spans.append((int(m.group(1), 16), int(m.group(2), 16), cur))
spans.sort()
SP = [s[0] for s in spans]
STEM = re.compile(r'\.(cpp|c|s)')


def blk(va):
    i = bisect.bisect_right(SP, va) - 1
    if i < 0:
        return None
    s, e, u = spans[i]
    return (s, e, u, i) if s <= va < e else None


ABSORB = re.compile(r'^(fn_[0-9A-Fa-f]{8}|except_data_[0-9A-Fa-f]{8})$')

charged, null_hits = [], 0
n_null = n_chg = 0
for addr, typ, name, size in adj.TEXT:
    if typ != 'function':
        continue
    b = blk(addr)
    if not b:
        continue
    mn = MAP.get('0x%08x' % addr)
    if not mn:
        continue
    stem = 'default/' + STEM.sub('', b[2])
    f = rows.get((stem, mn))
    if not f:
        continue
    mpn = f.get('match_percent_normalized')
    bs = BASE.get(mn)
    if bs is None:
        continue
    fired = False
    if bs > size:                                              # C1
        end = addr + bs
        if adj.raw_exit_kind(adj.word(end - 4), end - 4, addr, end):   # C2
            inner = adj.syms_in(addr + 4, end)
            if all(ABSORB.match(s[2]) for s in inner):         # C3
                fired = True
    if mpn == 100.0:
        n_null += 1
        null_hits += fired
    else:
        n_chg += 1
        if fired:
            end = addr + bs
            s, e, u, i = b
            nxt = spans[i + 1] if i + 1 < len(spans) else None
            cross = end > e
            same = cross and nxt and nxt[2] == u and nxt[0] == e
            charged.append((addr, size, bs, mpn, u, mn,
                            'clean' if not cross else ('SAME_UNIT_MERGE' if same else 'CONTESTED'),
                            len(adj.syms_in(addr + 4, end))))

print("WHOLE-BINARY SWEEP -- 'our body ends where retail's body ends'")
print(f"  NULL    (named, pinned, mpn==100) fired : {null_hits}/{n_null} = "
      f"{100.0*null_hits/max(n_null,1):.4f}%")
print(f"  CHARGED (named, pinned, sub-100)  fired : {len(charged)}/{n_chg} = "
      f"{100.0*len(charged)/max(n_chg,1):.4f}%")
print()
byk = {}
for c in charged:
    byk[c[6]] = byk.get(c[6], 0) + 1
print("  by splits disposition:", byk)
print()
print(f"{'addr':>10} {'claim':>6} {'base':>6} {'mpn':>8} {'absorb':>6} {'splits':>16}  unit :: name")
print('-' * 128)
for a, cs, bs, mpn, u, mn, disp, nab in sorted(charged, key=lambda r: (r[6], -(r[3] or 0))):
    print(f"0x{a:08X} {cs:6X} {bs:6X} {(mpn or 0):8.3f} {nab:6} {disp:>16}  {u} :: {mn[:44]}")

json.dump([{'addr': '0x%08X' % a, 'size': '0x%X' % bs, 'label': f'{u}::{mn}',
            'unit': u, 'name': mn, 'old_size': '0x%X' % cs, 'base_size': '0x%X' % bs,
            'disp': disp}
           for a, cs, bs, mpn, u, mn, disp, nab in charged],
          open(sys.argv[1], 'w'), indent=1)
print(f"\nwrote {sys.argv[1]}")
