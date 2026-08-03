#!/usr/bin/env python3
"""lane EA-1: bulk adjudicator for truncated / phantom-shattered .text extents.

Three independent witnesses per candidate, so a carve never rests on one:

  W1  .pdata      -- authoritative (begin, length) for every function that has a
                     record.  symbols.txt size < pdata length  =>  TRUNCATED,
                     decisively, with the true end handed to us.
  W2  exit scan   -- disassemble the CLAIMED extent; if it contains NO exit
                     instruction (blr / bctr / unconditional b out of range),
                     the body cannot return, so the extent is short.
  W3  continuation-- disassemble past the claimed end: does control flow read as
                     a continuation (epilogue restores / blr) rather than a new
                     prologue?

Usage:  adj.py <addr-hex> [addr-hex ...]      detail
        adj.py --queue <file>                 bulk table
"""
import struct, sys, re, bisect, os
import capstone

WT = '/home/free/tmp/laneEB1/wt'
D = open(os.path.join(WT, 'orig/45410914/band.exe'), 'rb').read()

SECS = {
    '.rdata': (0x82000400, 0x1f1184, 0x400),
    '.pdata': (0x821f1600, 0x70c28, 0x1f1600),
    '.text':  (0x82270000, 0x9dce3c, 0x264e00),
    '.data':  (0x82c64400, 0x1f5eac, 0xc52000),
}


def off(va):
    for n, (b, sz, raw) in SECS.items():
        if b <= va < b + sz:
            return raw + (va - b), n
    return None, None


def word(va):
    o, _ = off(va)
    return None if o is None else struct.unpack_from('>I', D, o)[0]


def textbytes(va, n):
    o, _ = off(va)
    return D[o:o + n]


# ---------------- .pdata ------------------------------------------------
PB, PSZ, PRAW = SECS['.pdata']
PDATA = []
for i in range(PSZ // 8):
    beg, dat = struct.unpack_from('>II', D, PRAW + i * 8)
    if beg == 0:
        continue
    PDATA.append((beg, (dat & 0xFF) * 4, ((dat >> 8) & 0x3FFFFF) * 4,
                  (dat >> 30) & 1, (dat >> 31) & 1))
PDATA.sort()
PBEG = [p[0] for p in PDATA]


def pdata_exact(va):
    i = bisect.bisect_left(PBEG, va)
    return PDATA[i] if i < len(PDATA) and PDATA[i][0] == va else None


def pdata_covering(va):
    """the .pdata record whose [beg, beg+len) contains va"""
    i = bisect.bisect_right(PBEG, va) - 1
    if i < 0:
        return None
    p = PDATA[i]
    return p if p[0] <= va < p[0] + p[2] else None


# ---------------- symbols.txt ------------------------------------------
SYMRE = re.compile(
    r'^(\S+) = \.(\w+):0x([0-9A-Fa-f]+); // type:(\w+) size:0x([0-9A-Fa-f]+)')


def load_syms(path):
    text, exc = [], set()
    for line in open(path):
        m = SYMRE.match(line)
        if not m:
            continue
        name, sec = m.group(1), m.group(2)
        addr, typ, size = int(m.group(3), 16), m.group(4), int(m.group(5), 16)
        if sec == 'rdata' and name.startswith('except_record_'):
            exc.add(addr)
        if sec == 'text':
            text.append((addr, typ, name, size))
    text.sort()
    return text, exc


SYMPATH = os.path.join(WT, 'config/45410914/symbols.txt')
TEXT, EXC = load_syms(SYMPATH)
TADDR = [t[0] for t in TEXT]


def sym_at(va):
    i = bisect.bisect_left(TADDR, va)
    return TEXT[i] if i < len(TEXT) and TEXT[i][0] == va else None


def syms_in(lo, hi):
    i = bisect.bisect_left(TADDR, lo)
    out = []
    while i < len(TEXT) and TEXT[i][0] < hi:
        out.append(TEXT[i])
        i += 1
    return out


# ---------------- disassembly ------------------------------------------
CS = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_32 | capstone.CS_MODE_BIG_ENDIAN)
CS.detail = False

EXITS = ('blr', 'bctr', 'blrl', 'bctrl', 'rfi')


def disas(va, n_bytes):
    out = []
    for ins in CS.disasm(textbytes(va, n_bytes), va):
        out.append((ins.address, struct.unpack('>I', bytes(ins.bytes))[0],
                    ins.mnemonic, ins.op_str))
    return out


def is_exit(mn, ops, va, lo, hi):
    """a true function exit: blr/bctr, or an unconditional b to OUTSIDE [lo,hi)"""
    if mn in EXITS:
        return True
    if mn == 'b':
        try:
            t = int(ops.strip().lstrip('#').replace('0x', ''), 16)
        except Exception:
            return False
        return not (lo <= t < hi)
    return False


# ---- RAW-WORD exit detection ------------------------------------------
# ** Capstone's PPC mode does NOT know VMX128 (the Xbox 360 vector extension).
# ** cs.disasm() STOPS at the first undecodable word and returns a SHORT list,
# ** which silently manufactures "zero exits".  Measured on fn_82B6CAF8
# ** (GainEffect::DoProcess): disasm quit at a vcmpequd and the real blr was
# ** never reached.  So exit detection must decode the raw encoding itself and
# ** must never depend on decoding every intervening instruction.
def raw_exit_kind(w, va, lo, hi):
    """None, or a short label for the kind of exit this word encodes."""
    if w is None:
        return None
    op = w >> 26
    lk = w & 1
    if op == 19:
        xo = (w >> 1) & 0x3FF
        if xo in (16, 528):                      # bclr / bcctr
            if lk:
                return None                      # blrl/bctrl = a CALL, not an exit
            bo = (w >> 21) & 0x1F
            # unconditional iff BO has both the "ignore CR" and "no decrement" bits
            return ('blr' if xo == 16 else 'bctr') if (bo & 0x14) == 0x14 else None
    if op == 18 and lk == 0:                     # b / ba (never bl)
        li = w & 0x03FFFFFC
        if li & 0x02000000:
            li -= 0x04000000
        tgt = (li if ((w >> 1) & 1) else va + li) & 0xFFFFFFFF
        return 'tailcall_b' if not (lo <= tgt < hi) else None
    if op == 3:                                  # twi -- trap, e.g. __report_fatal
        return 'trap'
    if op == 31 and ((w >> 1) & 0x3FF) == 4:     # tw
        return 'trap'
    return None


def exits_in(lo, hi):
    """raw-word scan; immune to capstone's VMX128 blind spot."""
    got = []
    for va in range(lo, hi, 4):
        w = word(va)
        k = raw_exit_kind(w, va, lo, hi)
        if k:
            got.append((va, w, k, ''))
    return got


def is_padding(w):
    return w in (0x00000000, 0x60000000)  # zero / nop


# ---------------- adjudication -----------------------------------------
def adjudicate(addr):
    s = sym_at(addr)
    if not s:
        return None
    a, typ, name, size = s
    claimed_end = a + size
    r = {'addr': a, 'name': name, 'size': size, 'claimed_end': claimed_end}

    # W1: .pdata
    pe = pdata_exact(a)
    r['pdata_exact'] = pe
    r['pdata_end'] = (pe[0] + pe[2]) if pe else None
    r['pdata_cover'] = pdata_covering(a)

    # W2: exits inside claimed extent (padding-trimmed, per DZ-1's correction:
    # dtk trims trailing zero words, so the raw last word is not the last insn)
    eff = size
    while eff >= 8 and word(a + eff - 4) == 0:
        eff -= 4
    r['eff'] = eff
    eff_end = a + eff
    ex = exits_in(a, eff_end)
    r['exits'] = ex
    r['n_exits'] = len(ex)
    lw = word(eff_end - 4)
    r['last'] = (eff_end - 4, lw, '', '')
    r['last_is_exit'] = raw_exit_kind(lw, eff_end - 4, a, eff_end) is not None

    # W3: what follows
    nxt = sym_at(claimed_end)
    r['next_sym'] = nxt
    r['after'] = disas(claimed_end, 64)
    # first exit at or after claimed_end (candidate true end)
    tail = []
    for aa, w, mn, ops in disas(claimed_end, 512):
        tail.append((aa, w, mn, ops))
        if mn in EXITS:
            break
    r['tail'] = tail
    r['true_end_guess'] = (tail[-1][0] + 4) if tail and tail[-1][2] in EXITS else None
    return r


def fmt(r):
    L = []
    a, name, size = r['addr'], r['name'], r['size']
    L.append(f"=== 0x{a:08X}  size=0x{size:X} ({size})  {name}")
    pe, pend = r['pdata_exact'], r['pdata_end']
    if pe:
        L.append(f"  W1 .pdata EXACT: beg=0x{pe[0]:08X} len=0x{pe[2]:X} "
                 f"end=0x{pend:08X} prolog=0x{pe[1]:X} ex={pe[4]}"
                 + ("   *** PDATA LONGER THAN SYMBOL ***" if pend > r['claimed_end'] else ""))
    else:
        pc = r['pdata_cover']
        L.append(f"  W1 .pdata: no exact record"
                 + (f"; COVERED BY 0x{pc[0]:08X} len=0x{pc[2]:X} end=0x{pc[0]+pc[2]:08X}"
                    " *** SYMBOL IS INSIDE ANOTHER FUNCTION ***" if pc else "; not covered (leaf)"))
    L.append(f"  W2 exits inside claimed extent: {r['n_exits']}"
             + ("   *** ZERO EXITS => CANNOT RETURN ***" if r['n_exits'] == 0 else ""))
    for aa, w, mn, ops in r['exits'][:4]:
        L.append(f"       0x{aa:08X} {w:08X} {mn} {ops}")
    lt = r['last']
    if lt:
        L.append(f"  last insn: 0x{lt[0]:08X} {lt[1]:08X} {lt[2]} {lt[3]}"
                 f"   terminator={r['last_is_exit']}")
    ns = r['next_sym']
    L.append(f"  next symbol @0x{r['claimed_end']:08X}: "
             + (f"{ns[2]} (type={ns[1]} size=0x{ns[3]:X})" if ns else "<none>"))
    L.append("  W3 following bytes:")
    for aa, w, mn, ops in r['after'][:12]:
        mark = ''
        s2 = sym_at(aa)
        if s2:
            mark = f"   <-- symbol {s2[2]} (type={s2[1]} size=0x{s2[3]:X})"
        L.append(f"       0x{aa:08X} {w:08X} {mn:10} {ops}{mark}")
    if r['true_end_guess']:
        L.append(f"  first exit after claimed end => true end guess 0x{r['true_end_guess']:08X}"
                 f"  (would be size 0x{r['true_end_guess']-a:X})")
    return '\n'.join(L)


if __name__ == '__main__':
    args = sys.argv[1:]
    if args and args[0] == '--queue':
        addrs = []
        for line in open(args[1]):
            m = re.match(r'^(0x[0-9A-Fa-f]{8})\s', line)
            if m:
                addrs.append(int(m.group(1), 16))
        print(f"{'addr':>10} {'size':>5} {'pdata_end':>10} {'symend':>10} "
              f"{'nex':>3} {'lastterm':>8}  verdict")
        print('-' * 100)
        for ad in addrs:
            r = adjudicate(ad)
            if not r:
                print(f"0x{ad:08X}  <no symbol>")
                continue
            v = []
            if r['pdata_end'] and r['pdata_end'] > r['claimed_end']:
                v.append('PDATA_LONGER')
            if not r['pdata_exact'] and r['pdata_cover']:
                v.append('INSIDE_OTHER_FN')
            if r['n_exits'] == 0:
                v.append('NO_EXIT')
            if not r['last_is_exit']:
                v.append('LAST_NOT_TERM')
            print(f"0x{r['addr']:08X} {r['size']:5X} "
                  f"{('0x%08X' % r['pdata_end']) if r['pdata_end'] else '-':>10} "
                  f"0x{r['claimed_end']:08X} {r['n_exits']:3} {str(r['last_is_exit']):>8}  "
                  f"{','.join(v) or 'clean'}")
    else:
        for x in args:
            r = adjudicate(int(x, 16))
            print(fmt(r) if r else f"{x}: no symbol")
            print()
