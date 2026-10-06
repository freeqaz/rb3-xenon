#!/usr/bin/env python3
"""bss_anchor_census.py -- list gap rows whose charge is an offset off a shared data anchor.

Lane W16-PY (2026-10-06), lever 5 of docs/decomp/CAMPAIGN_STATE_2026-10-06.md.

MSVC addresses a TU's internal statics through one anchor register: it forms the address of one
static (`lis rA,SYM@ha; addi rA,rA,SYM@l`) and reaches its neighbours as `off(rA)`. If our statics sit
in a different order than retail's, every neighbour access carries a different displacement, and the
row is charged an `immediate` diff on an otherwise identical instruction. W16-PG §2.1 found the knob
that controls the order: plain uninitialised statics are ordered by the code generator, but an explicit
`= 0` makes declaration order the ascending `.bss` order.

For every reachable gap row (ceiling_recompute's scaffold rule) the tool runs the graded
`objdiff-cli diff` (objdiff.json's own options, i.e. the shipped ruler) and classifies each charged
instruction:

  ANCHOR_OFF  diff_arg, every differing argument an immediate, on a load/store/addi whose base register
              traces back (linear backward walk, through `mr`) to an `addi rA,rX,SYM@l` on BOTH sides.
  ANCHOR_SYM  diff_arg on the symbol of the anchor-forming `addi`/`lis` itself, both sides data symbols.
  OTHER       anything else.

Row classes: PURE (every charge ANCHOR_*, at least one ANCHOR_OFF), MIXED (some ANCHOR_OFF and some
OTHER), none.

For every ANCHOR_OFF site it resolves WHICH of our symbols lives at `our_anchor + off` (from our obj's
COFF symbol table) and the retail address the same instruction reached (`symbols.txt`, or the address
encoded in a placeholder name, + off). Collected per TU, that pairing is retail's layout of our
statics, i.e. the declaration order the `= 0` mechanism would have to reproduce.

usage: python3 tools/bss_anchor_census.py [--worktree W] [--scope in|all] [--json OUT] [--rows NAME ...]
       python3 tools/bss_anchor_census.py --selftest
"""
import argparse, collections, json, os, re, struct, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

STORE_OPS = re.compile(r'^(st[bhw]|stfs|stfd|stmw|stwu|stbu|sthu|stfsu|stfdu|stwx|stbx|sthx|stvx|stvx128|stfiwx)')
NONDEF_OPS = re.compile(r'^(cmp|b|tw|td|mt|sync|isync|eieio|dcb|icb|nop)')
MEM_OPS = re.compile(r'^(l[bhw]z|lha|lwa|lfs|lfd|lmw|st[bhw]|stfs|stfd|stmw|l[bhw]zu|lfsu|lfdu|st[bhw]u|stfsu|stfdu|addi|la)$')
PLACEHOLDER = re.compile(r'^(lbl|fn|data|bss|rdata|jumptable)_([0-9A-Fa-f]{8})$')


# ---------------------------------------------------------------- scope (W16-OV ring, verbatim rule)
def load_scope(W):
    sys.path.insert(0, W + '/tools'); sys.path.insert(0, W + '/scripts')
    import native_scope_map as N
    dc3 = set()
    for r, _, fs in os.walk('/home/free/code/milohax/dc3-decomp/src/system'):
        for n in fs:
            dc3.add(os.path.join(r, n)[len('/home/free/code/milohax/dc3-decomp/'):].lower())

    def tier(src):
        t = N.classify(src)
        if t == '360-ONLY':
            s = src or ''
            if s.startswith('src/network/net/'): return 'OUT-NET'
            if s.startswith('src/network/'): return 'OUT-QUAZAL'
            if s.startswith('src/xdk/'): return 'OUT-XDK'
            return 'OUT-360-OTHER'
        if t == 'NATIVE-VIA-DC3':
            return 'VIA-DC3' if (src or '').lower() in dc3 else 'IN-RB3ENG'
        return {'NATIVE-CORE': 'IN-CORE', 'NATIVE-SOON': 'IN-SOON'}.get(t, t)
    return tier


def gap_rows(W):
    import ceiling_recompute as C
    r = json.load(open(W + '/build/45410914/report.json'))
    od = {u['name']: u for u in json.load(open(W + '/objdiff.json'))['units']}
    reach = matched = 0; gap = []
    for u in r['units']:
        bp = od.get(u['name'], {}).get('base_path')
        if not bp: continue
        cnt = C.coff_symcount(bp if os.path.isabs(bp) else os.path.join(W, bp))
        if cnt and cnt[0] <= 6: continue
        src = (u.get('metadata') or {}).get('source_path')
        for f in u.get('functions') or []:
            sz = int(f.get('size', 0)); fz = float(f.get('fuzzy_match_percent', 0) or 0)
            reach += sz
            if fz == 100: matched += sz; continue
            gap.append(dict(unit=u['name'], src=src, base=bp, name=f['name'], size=sz, fuzzy=fz,
                            mpn=float(f.get('match_percent_normalized', 0) or 0),
                            dem=(f.get('metadata') or {}).get('demangled_name')))
    assert reach - matched == sum(g['size'] for g in gap)
    return gap, reach, matched, int(r['measures']['matched_code'])


# ---------------------------------------------------------------- COFF: our statics' layout
def coff_layout(path):
    """{symbol: (section_name, section_index, value)} for defined symbols, plus per-section sorted lists."""
    d = open(path, 'rb').read()
    mach, nsec, ts, psym, nsym, opt, chars = struct.unpack_from('<HHIIIHH', d, 0)
    strtab = psym + 18 * nsym
    secs = []
    for i in range(nsec):
        name, vsize, vaddr, size, praw, prel, pln, nrel, nln, sc = struct.unpack_from('<8sIIIIIIHHI', d, 20 + opt + 40 * i)
        n = name.rstrip(b'\0').decode('latin1')
        if n.startswith('/'): n = d[strtab + int(n[1:]):d.index(b'\0', strtab + int(n[1:]))].decode('latin1')
        secs.append((n, size))
    syms = {}; bysec = collections.defaultdict(list)
    i = 0
    while i < nsym:
        b = psym + 18 * i
        raw = d[b:b + 8]
        val, secnum, typ, sclass, naux = struct.unpack_from('<IhHBB', d, b + 8)
        if raw[:4] == b'\0\0\0\0':
            o = strtab + struct.unpack_from('<I', raw, 4)[0]
            nm = d[o:d.index(b'\0', o)].decode('latin1')
        else:
            nm = raw.rstrip(b'\0').decode('latin1')
        if secnum > 0 and sclass in (2, 3) and typ != 0x20 and not nm.startswith('.') and not nm.startswith('$'):
            sn = secs[secnum - 1][0]
            syms[nm] = (sn, secnum, val)
            bysec[secnum].append((val, nm))
        i += 1 + naux
    for k in bysec: bysec[k].sort()
    return syms, bysec, secs


def resolve_ours(layout, anchor, off):
    syms, bysec, secs = layout
    if anchor not in syms: return None
    sn, k, v = syms[anchor]
    a = v + off; best = None
    for val, nm in bysec[k]:
        if val <= a: best = (nm, a - val)
        else: break
    return dict(section=sn, sec_off=a, sym=best[0] if best else None, within=best[1] if best else None)


# ---------------------------------------------------------------- retail addresses
def load_symbols_txt(W):
    out = {}
    pat = re.compile(r'^(\S+) = (\S+):0x([0-9A-Fa-f]+);')
    for line in open(W + '/config/45410914/symbols.txt'):
        m = pat.match(line)
        if m: out[m.group(1)] = (m.group(2), int(m.group(3), 16))
    return out


def retail_addr(symtab, name):
    m = PLACEHOLDER.match(name)
    if m: return int(m.group(2), 16)
    if name in symtab: return symtab[name][1]
    return None


# ---------------------------------------------------------------- per-instruction anchor tracing
def sym_of(ta):
    for a in ta:
        if a.get('type') == 'Symbol': return a['value']
    return None


def regs_of(ta):
    return [a['value'] for a in ta if a.get('type') == 'Register']


def mem_base(ins):
    """(base_reg, displacement) for `op rD, imm(rA)` / `addi rD, rA, imm`; None otherwise."""
    op = ins.get('opcode', ''); ta = ins.get('typed_args', [])
    if not MEM_OPS.match(op): return None
    kinds = [a.get('type') for a in ta]
    if op in ('addi', 'la'):
        if kinds == ['Register', 'Register', 'Signed']: return ta[1]['value'], ta[2]['value']
        return None
    if kinds == ['Register', 'Signed', 'Register']: return ta[2]['value'], ta[1]['value']
    return None


def defines(ins, reg):
    op = ins.get('opcode', '')
    if STORE_OPS.match(op) and not op.endswith('u'): return False
    if NONDEF_OPS.match(op): return False
    r = regs_of(ins.get('typed_args', []))
    if not r: return False
    if op.endswith('u') and STORE_OPS.match(op):  # stwu rS, d(rA): updates rA
        return len(r) > 1 and r[-1] == reg
    return r[0] == reg


def trace_anchor(seq, p, reg, depth=0):
    """Walk back from seq[p-1] to the def of reg. Returns anchor symbol name or None."""
    if depth > 4: return None
    for q in range(p - 1, -1, -1):
        ins = seq[q]
        if not defines(ins, reg): continue
        op = ins.get('opcode', ''); ta = ins.get('typed_args', [])
        if op == 'addi' and '@l' in ins.get('args', ''):
            return sym_of(ta)
        if op == 'mr':
            r = regs_of(ta)
            return trace_anchor(seq, q, r[1], depth + 1) if len(r) == 2 else None
        return None
    return None


def side_seqs(instrs):
    t = []; b = []; tpos = {}; bpos = {}
    for x in instrs:
        if x.get('target'): tpos[x['index']] = len(t); t.append(x['target'])
        if x.get('base'): bpos[x['index']] = len(b); b.append(x['base'])
    return t, b, tpos, bpos


def analyse(d, layout, symtab):
    instrs = d['instructions']
    t, b, tpos, bpos = side_seqs(instrs)
    cls = collections.Counter(); sites = []

    def anchored(x, tg, bs, charged):
        mt_ = mem_base(tg); mb_ = mem_base(bs)
        if not (mt_ and mb_): return False
        ta_ = trace_anchor(t, tpos[x['index']], mt_[0]); ba_ = trace_anchor(b, bpos[x['index']], mb_[0])
        if not (ta_ and ba_): return False
        ra = retail_addr(symtab, ta_)
        sites.append(dict(idx=x['index'], op=bs.get('opcode'), charged=charged, t_anchor=ta_, t_off=mt_[1],
                          retail=(ra + mt_[1]) if ra is not None else None,
                          b_anchor=ba_, b_off=mb_[1], ours=resolve_ours(layout, ba_, mb_[1])))
        return True

    for x in instrs:
        mt = x['match_type']
        tg = x.get('target') or {}; bs = x.get('base') or {}
        if mt == 'equal':
            if tg and bs: anchored(x, tg, bs, False)   # uncharged: witnesses the slide only
            continue
        verdict = 'OTHER'
        if mt == 'diff_arg':
            kinds = {a.get('arg_type') for a in (x.get('diff_breakdown') or {}).get('arguments', [])}
            if kinds == {'immediate'}:
                if anchored(x, tg, bs, True): verdict = 'ANCHOR_OFF'
            elif kinds == {'symbol'} and tg.get('opcode') in ('addi', 'lis') and '@' in tg.get('args', ''):
                ts, bsym = sym_of(tg.get('typed_args', [])), sym_of(bs.get('typed_args', []))
                if ts and bsym and bsym in layout[0] and layout[0][bsym][0].startswith(('.bss', '.data')):
                    verdict = 'ANCHOR_SYM'
        cls[verdict] += 1
    if cls['ANCHOR_OFF'] and not cls['OTHER']: rc = 'PURE'
    elif cls['ANCHOR_OFF']: rc = 'MIXED'
    else: rc = None
    return rc, dict(cls), sites


def subclass(sites):
    """LAYOUT / FIELD / ANCHOR_CHOICE for one row's anchored sites (charged and uncharged).

    slide = retail_address - our_section_offset. Grouped by OUR symbol:
      FIELD          some symbol is reached at more than one slide: retail reads a different offset INSIDE one
                     object (a member/component difference), which no static order can fix.
      LAYOUT         every symbol has one slide but symbols disagree: our statics sit in a different order or
                     spacing than retail's -- the class W16-PG §2.1's declaration-order knob can reach.
      ANCHOR_CHOICE  one slide for all: same layout, the compiler chose a different static as anchor.
    Charged sites alone cannot separate these (an unmoved neighbour is uncharged), which is why uncharged
    anchored accesses are collected as witnesses."""
    per = collections.defaultdict(set); secs = collections.defaultdict(set)
    for s in sites:
        o = s['ours']
        if not o or not o['sym'] or s['retail'] is None: return 'UNRESOLVED'
        slide = s['retail'] - o['sec_off']
        per[(o['section'], o['sym'])].add(slide); secs[o['section']].add(slide)
    if any(len(v) > 1 for v in per.values()): return 'FIELD'
    return 'ANCHOR_CHOICE' if all(len(v) == 1 for v in secs.values()) else 'LAYOUT'


def diff_row(W, r):
    p = subprocess.run([W + '/bin/objdiff-cli', 'diff', '-p', W, '-u', r['unit'], r['name'], '-f', 'json',
                        '--include-instructions', '-o', '-'], capture_output=True, text=True, cwd=W)
    try: return json.loads(p.stdout)
    except Exception: return None


def run(W, rows, symtab, jobs=16):
    layouts = {}
    def lay(bp):
        if bp not in layouts:
            try: layouts[bp] = coff_layout(bp if os.path.isabs(bp) else os.path.join(W, bp))
            except Exception: layouts[bp] = ({}, {}, [])
        return layouts[bp]
    for r in rows: lay(r['base'])
    def one(r):
        d = diff_row(W, r)
        if d is None or not d.get('base_size'): return r, None
        return r, analyse(d, layouts[r['base']], symtab)
    out = []
    with ThreadPoolExecutor(jobs) as ex:
        for r, a in ex.map(one, rows):
            if a is None: continue
            rc, cls, sites = a
            if rc: out.append(dict(r, rclass=rc, sub=subclass(sites), charges=cls, sites=sites))
    return out


def tu_layouts(found):
    """Per source file: our symbol -> set of retail addresses it was reached at."""
    per = collections.defaultdict(lambda: collections.defaultdict(set))
    for f in found:
        for s in f['sites']:
            o = s['ours']
            if o and o['sym'] and s['retail'] is not None:
                per[f['src']][(o['section'], o['sym'], o['within'])].add(s['retail'])
    return per


def selftest():
    # Hand-built ChordShapeGenerator shape (W16-PG §2.1): retail vertIt at faceIt-4, ours at faceIt+4.
    lay = ({'?faceIt@@3IA': ('.bss', 3, 0), '?vertIt@@3HA': ('.bss', 3, 4)},
           {3: [(0, '?faceIt@@3IA'), (4, '?vertIt@@3HA')]}, [])
    def I(op, args, ta): return dict(opcode=op, args=args, typed_args=ta)
    R = lambda r: {'type': 'Register', 'value': r}; S = lambda v: {'type': 'Signed', 'value': v}
    Y = lambda s: {'type': 'Symbol', 'value': s}
    anch_t = I('addi', 'r31, r11, lbl_82CC0004@l', [R('r31'), R('r11'), Y('lbl_82CC0004')])
    anch_b = I('addi', 'r31, r11, ?faceIt@@3IA@l', [R('r31'), R('r11'), Y('?faceIt@@3IA')])
    ld_t = I('lwz', 'r10, -0x4(r31)', [R('r10'), S(-4), R('r31')])
    ld_b = I('lwz', 'r10, 0x4(r31)', [R('r10'), S(4), R('r31')])
    ld0 = I('lwz', 'r9, 0x0(r31)', [R('r9'), S(0), R('r31')])
    d = dict(instructions=[
        dict(index=0, target=anch_t, base=anch_b, match_type='equal'),
        dict(index=1, target=ld_t, base=ld_b, match_type='diff_arg',
             diff_breakdown={'arguments': [{'arg_type': 'immediate'}]}),
        dict(index=2, target=ld0, base=ld0, match_type='equal')])
    rc, cls, sites = analyse(d, lay, {})
    assert rc == 'PURE', rc
    assert subclass(sites) == 'LAYOUT', subclass(sites)
    # same layout, different anchor (PreInitSystem shape): ours +8 -> retail +0 for one symbol only = slide const
    assert subclass([dict(retail=0x100, ours=dict(section='.bss', sec_off=8, sym='b')),
                     dict(retail=0xf8, ours=dict(section='.bss', sec_off=0, sym='a'))]) == 'ANCHOR_CHOICE'
    # XfmSort shape: one Vector3, retail reads +8 where we read +4 -> FIELD, not LAYOUT
    assert subclass([dict(retail=0x108, ours=dict(section='.bss', sec_off=4, sym='v')),
                     dict(retail=0x104, ours=dict(section='.bss', sec_off=8, sym='v'))]) == 'FIELD'
    assert sites[0]['ours']['sym'] == '?vertIt@@3HA' and sites[0]['retail'] == 0x82CC0000, sites
    assert subclass([x for x in sites if x['charged']]) == 'ANCHOR_CHOICE'  # charged sites alone cannot tell
    # Negative 1: same load but the base register is a parameter (no anchor) -> must NOT fire.
    d2 = dict(instructions=[d['instructions'][1]])
    assert analyse(d2, lay, {})[0] is None
    # Negative 2: an immediate diff on a compare -> OTHER, not anchor.
    d3 = dict(instructions=[d['instructions'][0], dict(index=1, target=I('cmpwi', 'r3, 1', [R('r3'), S(1)]),
              base=I('cmpwi', 'r3, 2', [R('r3'), S(2)]), match_type='diff_arg',
              diff_breakdown={'arguments': [{'arg_type': 'immediate'}]})])
    assert analyse(d3, lay, {})[0] is None
    # Negative 3: anchor intervened by a redefinition of r31 -> must NOT fire.
    redef = I('lwz', 'r31, 0x0(r3)', [R('r31'), S(0), R('r3')])
    d4 = dict(instructions=[d['instructions'][0], dict(index=1, target=redef, base=redef, match_type='equal'),
                            dict(d['instructions'][1], index=2)])
    assert analyse(d4, lay, {})[0] is None
    print('selftest PASSED: fires on the ChordShapeGenerator shape, silent on 3 negatives')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--worktree', default='.')
    ap.add_argument('--scope', choices=['in', 'all'], default='in')
    ap.add_argument('--json')
    ap.add_argument('--rows', nargs='*')
    ap.add_argument('--selftest', action='store_true')
    a = ap.parse_args()
    if a.selftest: return selftest()
    W = os.path.abspath(a.worktree)
    tier = load_scope(W)
    gap, reach, matched, mc = gap_rows(W)
    for g in gap: g['tier'] = tier(g['src'])
    pop = [g for g in gap if a.scope == 'all' or g['tier'] in ('IN-CORE', 'IN-SOON', 'IN-RB3ENG')]
    if a.rows: pop = [g for g in pop if g['name'] in a.rows]
    symtab = load_symbols_txt(W)
    found = run(W, pop, symtab)
    print(f'population: {len(pop)} rows / {sum(g["size"] for g in pop)} B (scope={a.scope}; gap {len(gap)} rows, '
          f'reach {reach}, matched-in-reach {matched}, report matched_code {mc})')
    for rc in ('PURE', 'MIXED'):
        for sub in ('LAYOUT', 'ANCHOR_CHOICE', 'FIELD', 'UNRESOLVED'):
            xs = [f for f in found if f['rclass'] == rc and f['sub'] == sub]
            print(f'{rc:5s} {sub:13s} {len(xs):4d} rows {sum(f["size"] for f in xs):8d} B')
    print()
    for f in sorted(found, key=lambda f: (f['rclass'] != 'PURE', f['sub'], -f['size'])):
        print(f"{f['rclass']:5s} {f['sub']:13s} {f['size']:6d} {f['fuzzy']:8.3f} {f['tier']:9s} {f['charges']} {f['src']} {f['dem'] or f['name']}")
        for s in [x for x in f['sites'] if x['charged']][:6]:
            o = s['ours'] or {}
            print(f"        {s['op']:5s} retail {s['t_anchor']}{s['t_off']:+#x} = {hex(s['retail']) if s['retail'] else '?'}"
                  f" | ours {s['b_anchor']}{s['b_off']:+#x} -> {o.get('section')} {o.get('sym')}+{o.get('within')}")
    if a.json:
        json.dump(found, open(a.json, 'w'), indent=1)


if __name__ == '__main__':
    main()
