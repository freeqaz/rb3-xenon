#!/usr/bin/env python3
"""W16-TP: value-flow comparison of target (retail) vs base (ours) for one objdiff row.

Written for register/reorder rows (P1/P2, mostly `mpn` 100): it asks whether the two bodies compute the same
values in the same observable places, up to register renaming and instruction scheduling.  It is a reading aid
with measured controls (tools/row_valueflow_ctl.py, docs/decomp/W16TP_*), not a verdict on its own.

Builds a CFG per side, reaching definitions, then a data-flow graph whose nodes are defs (plus entry and phi
nodes).  Node colours are refined jointly over both sides (bisimulation-style) from labels
(opcode + immediates + normalised symbol), with commutative operands sorted.  The observable events are compared:
non-stack stores (address colour + value colour), calls (target + argument colours), conditional branches
(condition colour), returns.  Equal event multisets => the two bodies compute the same values in the same
observable places, up to register renaming and scheduling.

Usage: row_valueflow.py UNIT SYMBOL          (runs objdiff-cli diff --include-instructions in the repo, or $WT)
       row_valueflow.py --json FILE         (a saved `objdiff-cli diff -f json --include-instructions` result)
Env: PL=1 lists placeholder-forgiven data references; STRICT=1 compares every argument register of
calls whose arity is unknown (vcalls), which is noisy by design.

Model, and its limits (measured in W16-TP):
- Defs/uses per instruction, CFG from branch targets, reaching definitions, then joint colour refinement.
  Commutative operands are sorted (add/and/or/xor/mullw/fadds/fmuls, the product of fmadds, the address
  of an indexed load/store).  Phi operands are ordered by the start index of the defining block.
- Frame slots addressed off r1 are pseudo-registers, so a std/lfd int->float round trip carries the value;
  calls clobber every frame slot.  Frame-slot offsets and addresses (addi rX, r1/frame-pointer, off) are not
  compared (layout).  Calls do not clobber r5-r12 (MSVC relies on a same-TU callee preserving them).
- Call arguments: exact from the MSVC mangled callee (llvm-undname; the Xbox 360 ABI is positional: every
  parameter takes the next GPR slot, floats also the next FPR), a small table of C functions, else a
  heuristic.  Returns: r3 / f1 from the row's own demangled return type.
- Constants: retail `lbl_` literal-pool loads are read out of orig/45410914/band.exe and compared by value;
  other placeholder targets take the base name (the ruler forgives them; PL=1 lists them for checking).
- Not modelled: memory ordering of non-frame loads against stores/calls (loads are keyed on base+offset
  only), associativity (a reassociated /fp:fast sum reads as different: use row_fp_algebra.py), rows with
  insert/delete are compared on their aligned instructions only.
"""
import json, re, subprocess, sys, collections, os

WT = os.environ.get('WT', os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

def get_json(unit, sym, base_obj=None):
    cmd = [WT + '/bin/objdiff-cli', 'diff', '-p', WT, '-u', unit, sym, '--include-instructions', '-f', 'json', '-o', '-']
    if base_obj: cmd = [WT + '/bin/objdiff-cli', 'diff', '-p', WT, '-u', unit, '-2', base_obj, sym, '--include-instructions', '-f', 'json', '-o', '-']
    p = subprocess.run(cmd, cwd=WT, capture_output=True, text=True)
    if p.returncode: raise SystemExit(p.stderr)
    return json.loads(p.stdout)

REGRE = re.compile(r'^(r\d+|f\d+|cr\d|v\d+)$')
MEMRE = re.compile(r'^(-?0x[0-9a-fA-F]+|-?\d+|[^()]*@l)\((r\d+)\)$')
PLACE = re.compile(r'^(fn_|lbl_|jumptable_|data_|bss_|rdata_)')

def normsym(s):
    s = re.sub(r'@(h|l|ha)$', '', s)
    return s

COMM = {'add', 'and', 'or', 'xor', 'mullw', 'fadds', 'fadd', 'fmuls', 'fmul', 'nand', 'nor', 'eqv', 'mulhw', 'mulhwu',
        'mulld', 'add.', 'and.', 'or.', 'xor.'}
FMA = {'fmadds', 'fmsubs', 'fnmadds', 'fnmsubs', 'fmadd', 'fmsub', 'fnmadd', 'fnmsub'}
VOL_R = ['r0'] + [f'r{i}' for i in range(3, 13)]
VOL_F = [f'f{i}' for i in range(0, 14)]
ARG_R = [f'r{i}' for i in range(3, 11)]
ARG_F = [f'f{i}' for i in range(1, 14)]

def is_branch(op):
    return op.startswith('b') and op not in ('bl',) and not op.startswith('bl') or op in ('blr', 'blt', 'ble', 'blt+', 'blt-', 'ble+', 'ble-', 'bltlr', 'blelr')

def kindof(I):
    k = classify(I['op'])
    if k == 'uncond' and I['args'] and I['args'][0].startswith('__rest'): return 'ret'  # b __restgprlr_N epilogue
    return k

def classify(op):
    """returns kind: call, vcall, ret, cret, uncond, cond, condret, bctr, other"""
    o = op.rstrip('+-')
    if o == 'bl': return 'call'
    if o == 'bctrl': return 'vcall'
    if o == 'blr': return 'ret'
    if o == 'bctr': return 'bctr'
    if o == 'b': return 'uncond'
    if o.startswith('b'):
        if o.endswith('lr') and o not in ('bl',): return 'condret'
        if o.endswith('ctr'): return 'condctr'
        if o.startswith('bdnz') or o.startswith('bdz'): return 'cond'
        if o in ('beq', 'bne', 'blt', 'ble', 'bgt', 'bge', 'bso', 'bns', 'bun', 'bnu', 'bnl', 'bng'): return 'cond'
        return 'cond'
    return 'other'

import struct
_IMG = None
PLACEHOLDER_LOG = []
def rdimg(va, n):
    global _IMG
    if _IMG is None:
        D = open(WT + '/orig/45410914/band.exe', 'rb').read()
        pe = struct.unpack_from('<I', D, 0x3c)[0]; ns = struct.unpack_from('<H', D, pe + 6)[0]; osz = struct.unpack_from('<H', D, pe + 20)[0]
        ib = struct.unpack_from('<I', D, pe + 24 + 28)[0]; sec = pe + 24 + osz
        S = [struct.unpack_from('<8sIIII', D, sec + 40 * i) for i in range(ns)]
        _IMG = (D, ib, S)
    D, ib, S = _IMG
    rva = va - ib
    for nm, vs, vaddr, rs, rp in S:
        if vaddr <= rva < vaddr + max(vs, rs): return D[rp + rva - vaddr: rp + rva - vaddr + n]
    return None

SYMTOK = re.compile(r'([A-Za-z_?$][^\s,()]*?)@(l|h|ha)\b')
def norm_pair(t, b, mt):
    """rewrite target/base arg strings so equal constants / forgiven names read the same"""
    if not t or not b: return
    ta = t.get('args', ''); ba = b.get('args', '')
    st = SYMTOK.findall(ta); sb = SYMTOK.findall(ba)
    if not st or len(st) != len(sb): return
    for (tn, tk), (bn, bk) in zip(st, sb):
        if tn == bn: continue
        new = None
        if tn.startswith('lbl_') and t['opcode'] in ('lfs', 'lfd') and tk == 'l' and bn.startswith('__real@'):
            n = 8 if t['opcode'] == 'lfd' else 4
            raw = rdimg(int(tn[4:], 16), n)
            if raw is not None:
                new = '__real@' + raw.hex()
                if bn.startswith('__real@') and bn[7:] == raw.hex(): new = bn
        if new is None and (tk in ('h', 'ha')):
            new = 'HI'; b['args'] = b['args'].replace(bn + '@' + bk, 'HI@' + bk, 1)
        if new is None and PLACE.match(tn):
            PLACEHOLDER_LOG.append((tn, bn)); new = bn
        if new is None and mt == 'equal': new = bn  # the ruler forgives placeholder targets
        if new is not None:
            t['args'] = t['args'].replace(tn + '@' + tk, new + '@' + tk, 1)

def parse_side(ins, key):
    if key == 'target':
        for x in ins:
            if x.get('target') and x.get('base') and not x.get('_n'):
                x['target'] = dict(x['target']); x['base'] = dict(x['base'])
                norm_pair(x['target'], x['base'], x.get('match_type')); x['_n'] = 1
    L = []
    for x in ins:
        s = x.get(key)
        if not s: L.append(None); continue
        L.append({'addr': int(s['address'], 16), 'op': s['opcode'], 'mt': x.get('match_type'),
                  'other': (x.get('base') if key == 'target' else x.get('target')),
                  'args': [a.strip() for a in s['args'].split(',')] if s.get('args') else []})
    return L

def defs_uses(I):
    """returns (defs, uses, label_extra) for one instruction dict.  uses are register names in order,
    with ('mem', base) used for the address base."""
    op = I['op'].rstrip('+-'); a = I['args']; kind = kindof(I)
    defs = []; uses = []; lab = [op]
    def reg(x): return bool(REGRE.match(x))
    if kind in ('call', 'vcall'):
        return [], [], lab  # handled specially
    if kind in ('cond', 'condret', 'condctr'):
        crs = [x for x in a if x.startswith('cr')]
        if op.startswith('bdnz') or op.startswith('bdz'):
            uses = ['ctr']; defs = ['ctr']
        else:
            uses = crs[:1] or ['cr0']
        lab = [op.rstrip('+-')]
        if kind == 'condctr': uses.append('ctr')
        if kind == 'condret': uses = uses + RETREG
        return defs, uses, lab
    if kind in ('ret', 'uncond', 'bctr'):
        if kind == 'bctr': uses = ['ctr']
        if kind == 'ret': uses = list(RETREG)
        return [], uses, lab
    rec = op.endswith('.')
    base = op.rstrip('.')
    if base == 'stwu' and a and a[0] == 'r1':
        return ['r1'], [], ['stwu']
    if base.startswith('tw') or base.startswith('td'):
        return [], [x for x in a if reg(x)], lab + [x for x in a if not reg(x)]
    if base in ('cmpw', 'cmplw', 'cmpwi', 'cmplwi', 'cmpd', 'cmpdi', 'cmpld', 'cmpldi', 'fcmpu', 'fcmpo'):
        if a and a[0].startswith('cr'): d = a[0]; rest = a[1:]
        else: d = 'cr0'; rest = a
        defs = [d]
        for x in rest:
            if reg(x): uses.append(x)
            else: lab.append(x)
        return defs, uses, lab
    if base in ('mtctr',): return ['ctr'], [a[0]], lab
    if base in ('mtlr',): return ['lr'], [a[0]], lab
    if base in ('mflr',): return [a[0]], ['lr'], lab
    if base in ('mfcr', 'mfocrf'): return [a[0]], ['cr0', 'cr1', 'cr2', 'cr3', 'cr4', 'cr5', 'cr6', 'cr7'], lab
    if base in ('mtcrf',): return ['cr0', 'cr1', 'cr2', 'cr3', 'cr4', 'cr5', 'cr6', 'cr7'], [a[-1]], lab
    if base in ('cror', 'crand', 'crnor', 'crxor', 'creqv', 'crorc', 'crandc', 'crnand', 'crset', 'crclr', 'crnot', 'crmove'):
        # bit-level: treat as def of the cr field of first bit, uses of the fields of other bits
        def fld(b):
            try: n = int(b, 0) if not b.startswith('4*') else None
            except ValueError: n = None
            m = re.match(r'(?:4\*)?cr(\d)', b)
            if m: return 'cr' + m.group(1)
            if n is not None: return f'cr{n // 4}'
            return 'cr0'
        defs = [fld(a[0])]; uses = [fld(x) for x in a[1:]] + [fld(a[0])]; lab += a
        return defs, uses, lab
    # stores
    if base.startswith('st') and not base.startswith('stw') or base.startswith('stw') or base.startswith('stf') or base.startswith('stb') or base.startswith('sth') or base.startswith('std') or base.startswith('stv'):
        if base in ('stmw',):
            return [], [], lab + a
        val = a[0]
        m = MEMRE.match(a[1]) if len(a) > 1 else None
        if m and m.group(2) == 'r1' and not base.endswith('u') and '@' not in m.group(1):
            # frame slot as a pseudo-register, so a later load of the slot takes the stored value
            return ['stk:' + m.group(1)], [val], ['stkst', base]
        if m:
            off = m.group(1)
            uses = [val, ('mem', m.group(2))]
            lab.append(off if not '@' in off else 'SYM:' + normsym(off))
            if base.endswith('u'): defs = [m.group(2)]
        else:  # indexed form stwx rS, rA, rB : address = rA + rB (commutative)
            uses = [val] + sorted([x for x in a[1:] if reg(x)])
            lab.append('X')
        return defs, uses, lab
    # loads
    if base.startswith('l') and base not in ('li', 'lis') and len(a) >= 2:
        m = MEMRE.match(a[1])
        if m and m.group(2) == 'r1' and not base.endswith('u') and '@' not in m.group(1) and base != 'lmw':
            return [a[0]], ['stk:' + m.group(1)], ['stkld', base]
        if m:
            off = m.group(1)
            defs = [a[0]]; uses = [('mem', m.group(2))]
            lab.append(off if '@' not in off else 'SYM:' + normsym(off))
            if base.endswith('u'): defs.append(m.group(2))
            if base == 'lmw': return [], [], lab
        else:
            defs = [a[0]]; uses = [x for x in a[1:] if reg(x)]; lab.append('X')
        return defs, uses, lab
    # generic ALU: first operand is dest
    if not a: return [], [], lab
    d = a[0]
    if reg(d): defs = [d]
    rest = a[1:]
    if base in ('rlwimi', 'insrwi', 'inslwi'): uses.append(d)
    if base in ('fsel',):
        pass
    for x in rest:
        if reg(x): uses.append(x)
        elif '@' in x: lab.append('SYM:' + normsym(x))
        else: lab.append(x)
    if base in ('srawi', 'sraw', 'addic', 'subfic', 'subfc', 'addc'): defs.append('ca')
    if base in ('addze', 'adde', 'subfe', 'subfze', 'addme'): uses.append('ca')
    if rec: defs.append('cr0')
    if base.startswith('f') and rec: defs.append('cr1')
    return defs, uses, lab

class Side:
    def __init__(self, L, name):
        self.name = name
        self.L = L
        idx = [i for i, x in enumerate(L) if x]
        self.idx = idx
        self.start = L[idx[0]]['addr']
        self.addr2i = {L[i]['addr']: i for i in idx}

def target_index(S, I):
    for x in I['args']:
        if re.match(r'^0x[0-9a-f]+$', x):
            v = int(x, 16)
            return S.addr2i.get(v)
    return None

def analyze(S, sym_events):
    L = S.L; n = len(L)

    order = S.idx
    nxt = {order[k]: (order[k + 1] if k + 1 < len(order) else None) for k in range(len(order))}
    leaders = {order[0]}
    for i in order:
        k = kindof(L[i])
        if k in ('uncond', 'cond', 'condret', 'ret', 'bctr', 'condctr'):
            t = target_index(S, L[i]) if k in ('uncond', 'cond') else None
            if t is not None: leaders.add(t)
            if nxt[i] is not None: leaders.add(nxt[i])
    blocks = []; cur = None; bof = {}
    for i in order:
        if i in leaders: cur = []; blocks.append(cur)
        cur.append(i); bof[i] = len(blocks) - 1
    succ = collections.defaultdict(set)
    tail = {}
    for b, B in enumerate(blocks):
        last = B[-1]; k = kindof(L[last])
        nx = nxt[last]
        if k == 'uncond':
            t = target_index(S, L[last])
            if t is not None: succ[b].add(bof[t])
            else: tail[b] = True
        elif k == 'cond':
            t = target_index(S, L[last])
            if t is not None: succ[b].add(bof[t])
            if nx is not None: succ[b].add(bof[nx])
        elif k in ('condret', 'condctr'):
            if nx is not None: succ[b].add(bof[nx])
        elif k == 'ret':
            pass
        elif k == 'bctr':
            for bb in range(len(blocks)): succ[b].add(bb)  # jump table: conservative
        else:
            if nx is not None: succ[b].add(bof[nx])
    # per-instruction defs/uses
    info = {}
    slots = set()
    for i in order:
        if classify(L[i]['op']) not in ('call', 'vcall'):
            d_, u_, l_ = defs_uses(L[i])
            slots |= {r for r in d_ if isinstance(r, str) and r.startswith('stk:')}
    for i in order:
        I = L[i]; k = kindof(I)
        if k in ('call', 'vcall'):
            tgt = I['args'][0] if I['args'] else 'ctr'
            if k == 'call' and (tgt.startswith('__save') or tgt.startswith('__rest')):
                info[i] = ([], [], ['savrest'], k)
                continue
            uses = ARG_R + ARG_F + (['ctr'] if k == 'vcall' else [])
            # results; other volatile regs pass through (MSVC relies on a same-TU callee preserving them,
            # e.g. TrainerGemTab::Render tests r9/r10 after bl GetLane on both sides)
            defs = ['r3', 'r4', 'f1', 'f2', 'ctr', 'cr0', 'cr1', 'cr5', 'cr6', 'cr7', 'ca'] + sorted(slots)
            nm = 'ctr' if k == 'vcall' else normsym(tgt)
            o = I.get('other')
            if k == 'call' and o and o.get('opcode') == 'bl' and I.get('mt') == 'equal' and S.name == 'T':
                nm = normsym(o['args'].split(',')[0].strip())  # uncharged (forgiven/folded) callee name: use base's
            info[i] = (defs, uses, ['call', nm], k)
        else:
            d, u, lab = defs_uses(I)
            info[i] = (d, u, lab, k)
    # reaching defs: per register, set of def sites ('i', reg) or ('entry', reg)
    gen = {};
    for b, B in enumerate(blocks):
        g = {}
        for i in B:
            for r in info[i][0]: g[r] = i
        gen[b] = g
    IN = [dict() for _ in blocks]; OUT = [dict() for _ in blocks]
    pred = collections.defaultdict(set)
    for b, ss in succ.items():
        for s in ss: pred[s].add(b)
    allregs = set()
    for i in order:
        allregs |= set(info[i][0]); allregs |= set(r if isinstance(r, str) else r[1] for r in info[i][1])
    allregs |= {'r1', 'r2', 'r13'}
    entry = {r: frozenset([('entry', r)]) for r in allregs}
    changed = True
    while changed:
        changed = False
        for b in range(len(blocks)):
            if b == 0: inn = dict(entry)
            else: inn = {}
            for p in pred[b]:
                for r, s in OUT[p].items():
                    inn[r] = inn.get(r, frozenset()) | s
            if b == 0:
                for p in pred[b]:
                    for r, s in OUT[p].items(): inn[r] = inn[r] | s
            out = dict(inn)
            for r, i in gen[b].items(): out[r] = frozenset([(i, r)])
            if inn != IN[b] or out != OUT[b]:
                IN[b] = inn; OUT[b] = out; changed = True
    # walk to get the reaching set at each use
    uses_at = {}
    for b, B in enumerate(blocks):
        cur = dict(IN[b])
        for i in B:
            d, u, lab, k = info[i]
            rs = []
            for r in u:
                rr = r if isinstance(r, str) else r[1]
                rs.append(cur.get(rr, frozenset([('undef', rr)])))
            uses_at[i] = rs
            for r in d: cur[r] = frozenset([(i, r)])
    return blocks, info, uses_at, tail

_ar_cache = {}
def arity(name):
    if name in _ar_cache: return _ar_cache[name]
    res = None
    if name.startswith('?'):
        p = subprocess.run(['llvm-undname', name], capture_output=True, text=True)
        dm = p.stdout.strip().splitlines()[-1] if p.stdout.strip() else ''
        if dm and dm != name and '(' in dm:
            pre, _, rest = dm.partition('__cdecl ') if '__cdecl ' in dm else dm.partition('__stdcall ')
            # params: outermost parens after the function name
            depth = 0; start = None; end = None
            for j, ch in enumerate(rest):
                if ch == '(' :
                    if depth == 0 and start is None: start = j
                    depth += 1
                elif ch == ')':
                    depth -= 1
                    if depth == 0 and start is not None: end = j; break
            if start is not None and end is not None:
                plist = rest[start + 1:end]
                parts = []; depth = 0; cur = ''
                for ch in plist:
                    if ch in '<(': depth += 1
                    if ch in '>)': depth -= 1
                    if ch == ',' and depth == 0: parts.append(cur.strip()); cur = ''
                    else: cur += ch
                if cur.strip(): parts.append(cur.strip())
                if parts == ['void']: parts = []
                # Xbox 360 ABI is positional: every parameter takes the next GPR slot; float/double
                # parameters also take the next FPR (checked on Singer::AppendToScoreHistory(float,int,float,int):
                # r5 and r7 carry the ints).
                regs = []; slot = 3; nf = 1
                member = any(pre.startswith(x) for x in ('public:', 'private:', 'protected:')) and 'static ' not in pre
                ret = pre.split(':', 1)[1] if member else pre
                if re.search(r'\b(class|struct|union)\b', ret) and '*' not in ret and '&' not in ret:
                    regs.append(f'r{slot}'); slot += 1
                if member: regs.append(f'r{slot}'); slot += 1
                for q in parts:
                    if q == '...': regs += [f'r{k}' for k in range(slot, 11)]; slot = 11; continue
                    if q in ('float', 'double'):
                        regs.append(f'f{nf}'); nf += 1
                    elif slot <= 10: regs.append(f'r{slot}')
                    slot += 1
                res = [r for r in regs if r in ARG_R or r in ARG_F]
    _ar_cache[name] = res
    return res

def arg_regs_for(name, k):
    a = arity(name) if k == 'call' else None
    return a

def build_graph(S, tag, callargs):
    blocks, info, uses_at, tail = S.an
    L = S.L
    nodes = {}  # id -> (label, operands, commutative)
    def nid(x): return (tag,) + (x if isinstance(x, tuple) else (x,))
    phis = {}
    blk_of = {}
    for bi, Bk in enumerate(blocks):
        for ii in Bk: blk_of[ii] = Bk[0]
    def val(rs):
        # incoming defs ordered by the start index of their block (aligned on both sides, and immune to
        # scheduling inside a block), so swapping the arms of a conditional assignment is visible
        rs = sorted(rs, key=lambda x: (-1, x[1]) if x[0] in ('entry', 'undef') else (blk_of[x[0]], ''))
        if len(rs) == 1:
            x = rs[0]
            if x[0] in ('entry', 'undef'):
                k = nid(x)
                if k not in nodes: nodes[k] = (('entry', x[1]), [], False)
                return k
            return nid(x)
        k = nid(('phi',) + tuple(rs))
        if k not in nodes:
            ops = [val([x]) for x in rs]
            nodes[k] = (('phi', tuple(blk_of[x[0]] if x[0] not in ('entry', 'undef') else x for x in rs)), ops, False)
        return k
    stack_base = set()
    events = []
    # first pass: create def nodes
    for i in S.idx:
        d, u, lab, k = info[i]
        ops = [val(rs) for rs in uses_at[i]]
        I = L[i]; op = I['op'].rstrip('+-').rstrip('.')
        if k in ('call', 'vcall'):
            if lab[0] == 'savrest': continue
            call = nid(('call', i))
            regs = ARG_R + ARG_F + (['ctr'] if k == 'vcall' else [])
            keep = callargs.get(i, regs)
            ops = [ops[j] for j, r in enumerate(regs) if r in keep]
            nodes[call] = (tuple(lab), ops, False)
            for r in d:
                nodes[nid((i, r))] = (('callout', 'stk' if r.startswith('stk:') else r), [call], False)
            events.append(('call', call, i))
            continue
        commut = op in COMM
        if op in ('addi', 'subi') and ops and is_stack(nodes, None, ops[0]):
            lab = ['stkaddr']  # address of a frame local: slot offsets differ with layout
        if len(lab) > 1 and lab[1] == 'X' and len(ops) >= 2:
            if op.startswith('st'):
                p = nid(('addx', i)); nodes[p] = (('addx',), ops[1:3], True); ops = [ops[0], p]
            elif op.startswith('l'):
                commut = True
        if op in FMA:
            # fmadds d,a,b,c -> sort a,b
            p = nid(('prod', i)); nodes[p] = (('prod',), ops[:2], True)
            ops = [p] + ops[2:]
        for j, r in enumerate(d):
            role = () if len(d) == 1 else (('d',) if j == 0 else (r if r in ('cr0', 'cr1', 'ca', 'ctr') else 'upd',))
            nodes[nid((i, r))] = (tuple(lab) + role, ops, commut)
        base = op
        if k in ('cond', 'condret', 'condctr'):
            events.append(('br', ops[0] if ops else None, i, op))
        if op.startswith('tw') or op.startswith('td'):
            events.append(('trap', i, ops, tuple(lab)))
        if k == 'ret' or k == 'condret':
            events.append(('ret', i, ops[len(ops) - len(RETREG):] if RETREG else []))
        if k == 'uncond' and i in [B[-1] for b, B in enumerate(blocks) if tail.get(b)]:
            events.append(('tail', i, I['args'][0]))
        if lab and lab[0] == 'stkst':
            events.append(('stkst', i, ops, tuple(lab)))
        elif (base.startswith('st') and base not in ('stwu', 'stdu')):
            events.append(('store', i, ops, tuple(lab)))
    return nodes, events, uses_at, info

def refine(nodes):
    col = {}
    lab_ids = {}
    for k, (lab, ops, c) in nodes.items():
        col[k] = lab_ids.setdefault(repr(lab), len(lab_ids))
    for it in range(200):
        sig = {}
        new = {}
        for k, (lab, ops, c) in nodes.items():
            oc = [col.get(o, -1) for o in ops]
            if c: oc = sorted(oc)
            s = (col[k], tuple(oc))
            new[k] = sig.setdefault(s, len(sig))
        if len(set(new.values())) == len(set(col.values())):
            col = new; break
        col = new
    return col

def choose_args(T, B):
    res = {}
    def usecount(S):
        cnt = collections.Counter()
        blocks, info, uses_at, tail = S.an
        for i, rss in uses_at.items():
            if info[i][3] in ('call', 'vcall'): continue  # count non-call uses only
            for rs in rss:
                for x in rs: cnt[x] += 1
        return cnt
    def prevcall(S, i):
        p = -1
        for j in S.idx:
            if j >= i: break
            if kindof(S.L[j]) in ('call', 'vcall') and not (S.L[j]['args'] and S.L[j]['args'][0].startswith(('__save', '__rest'))): p = j
        return p
    uc = {T: usecount(T), B: usecount(B)}
    for i in T.idx:
        if i not in B.addr2i.values() and not B.L[i]: continue
        if not B.L[i]: continue
        kt = classify(T.L[i]['op']); kb = classify(B.L[i]['op'])
        if kt not in ('call', 'vcall') or kb != kt: continue
        nameB = B.L[i]['args'][0] if B.L[i]['args'] else 'ctr'
        nameT = T.L[i]['args'][0] if T.L[i]['args'] else 'ctr'
        if nameB.startswith('__save') or nameB.startswith('__rest'): continue
        regs = ARG_R + ARG_F + (['ctr'] if kt == 'vcall' else [])
        ar = arg_regs_for(normsym(nameB), kt) or arg_regs_for(normsym(nameT), kt)
        if ar is None and kt == 'call' and normsym(nameB) in CFUNCS: ar = CFUNCS[normsym(nameB)]
        if os.environ.get('STRICT') and kt == 'vcall': ar = None
        if os.environ.get('STRICT') and ar is None:
            # strict: every arg register that holds a value defined in this function (or r3..r10 entry)
            keep = set(['ctr']) if kt == 'vcall' else set()
            for j, r in enumerate(regs):
                a = T.an[2][i][j]; b = B.an[2][i][j]
                if any(x[0] != 'entry' and x[0] != 'undef' for x in a | b) or (r.startswith('r') and all(x[0] == 'entry' for x in a | b)):
                    keep.add(r)
            res[i] = keep; continue
        if ar is not None:
            res[i] = set(ar) | ({'ctr'} if kt == 'vcall' else set()); continue
        keep = set(['ctr']) if kt == 'vcall' else set()
        for S in (T, B):
            rss = S.an[2][i]
            for j, r in enumerate(regs):
                rs = rss[j]
                if len(rs) == 1:
                    x = next(iter(rs))
                    if x[0] in ('entry', 'undef'): continue
                    if uc[S][x] == 0 and x[0] > prevcall(S, i): keep.add(r)
        # entry regs: keep if entry on both sides and the reg is a parameter reg (r3..r10)
        for j, r in enumerate(regs):
            a = T.an[2][i][j]; b = B.an[2][i][j]
            if len(a) == 1 and len(b) == 1 and next(iter(a))[0] == 'entry' and next(iter(b))[0] == 'entry' and r == 'r3':
                keep.add(r)
        res[i] = keep
    return res

def is_stack(nodes, col, k, depth=0):
    # address operand is r1 entry or derived (addi from r1)
    if k not in nodes: return False
    lab, ops, c = nodes[k]
    if lab == ('entry', 'r1') or (lab and lab[0] == 'stwu'): return True
    if depth < 4 and lab and lab[0] in ('addi', 'mr', 'subi', 'stkaddr') and ops:
        return is_stack(nodes, col, ops[0], depth + 1)
    if lab and lab[0] == 'phi':
        return all(is_stack(nodes, col, o, depth + 1) for o in ops) if depth < 4 else False
    return False

CFUNCS = {n: ['r3', 'r4', 'r5'] for n in ('memcpy', 'memset', 'memmove', 'memcmp', 'strncpy', 'strncmp', 'strncat')}
CFUNCS.update({n: ['r3', 'r4'] for n in ('strcmp', 'strcpy', 'strcat', 'stricmp', '_stricmp', 'strstr', 'strchr')})
CFUNCS.update({n: ['r3'] for n in ('strlen', 'atexit', 'atoi', 'atof', 'free', 'malloc')})
RETREG = []
def retreg(dem):
    m = re.match(r'^(?:(?:public|private|protected): )?(?:static |virtual )*(.*?)\s*__cdecl ', dem or '')
    if not m: return []
    rt = m.group(1).strip()
    if rt in ('', 'void'): return []
    if rt in ('float', 'double'): return ['f1']
    return ['r3']

def compare(d, verbose=False):
    global RETREG
    RETREG = retreg(d.get('demangled'))
    ins = d['instructions']
    T = Side(parse_side(ins, 'target'), 'T'); B = Side(parse_side(ins, 'base'), 'B')
    T.an = analyze(T, None); B.an = analyze(B, None)
    callargs = choose_args(T, B)
    nT, eT, uT, iT = build_graph(T, 'T', callargs); nB, eB, uB, iB = build_graph(B, 'B', callargs)
    nodes = dict(nT); nodes.update(nB)
    col = refine(nodes)
    def ev(events, nodes_side, info):
        out = collections.Counter(); det = collections.defaultdict(list)
        for e in events:
            if e[0] == 'store':
                _, i, ops, lab = e
                val, addr = ops[0], ops[1] if len(ops) > 1 else None
                if addr is not None and is_stack(nodes, col, addr):
                    # frame store: slot offsets follow the layout, so compare the stored value only
                    key = ('stkstore', lab[0], col.get(val))
                else:
                    key = ('store', lab[0], lab[1] if len(lab) > 1 else None, col.get(addr), col.get(val))
            elif e[0] == 'call':
                _, c, i = e
                lab, ops, _c = nodes[c]
                # argument registers: keep those whose value is not an untouched entry reg, or entry of r3..r5
                key = ('call', lab[1], tuple(col[o] for o in ops))
            elif e[0] == 'stkst':
                key = ('stkstore', e[3][1], col.get(e[2][0]))
            elif e[0] == 'trap':
                key = ('trap', e[3], tuple(col.get(o) for o in e[2]))
            elif e[0] == 'br':
                key = ('br', e[3], col.get(e[1]))
            elif e[0] == 'ret':
                key = ('ret', tuple(col.get(o) for o in e[2]))
            else:
                key = e[:1] + (e[2],)
            out[key] += 1; det[key].append(e)
        return out, det
    cT, dT = ev(eT, nT, iT); cB, dB = ev(eB, nB, iB)
    return cT, cB, dT, dB, nodes, col, T, B

def call_args_detail(nodes, col, e_t, e_b):
    pass

def explain(nodes, col, a, b, depth=0, seen=None):
    """find the first divergent node pair under two differently-coloured nodes"""
    seen = seen if seen is not None else set()
    if (a, b) in seen or depth > 40: return None
    seen.add((a, b))
    la, oa, ca = nodes[a]; lb, ob, cb = nodes[b]
    if la != lb or len(oa) != len(ob):
        return (a, la, b, lb)
    pa = [o for o in oa]; pb = [o for o in ob]
    if ca:
        pa = sorted(pa, key=lambda o: col[o]); pb = sorted(pb, key=lambda o: col[o])
    for x, y in zip(pa, pb):
        if col[x] != col[y]:
            r = explain(nodes, col, x, y, depth + 1, seen)
            if r: return r
    return (a, la, b, lb, 'operands-equal-colour?')

def main():
    args = sys.argv[1:]
    if args[0] == '--json': d = json.load(open(args[1])); args = args[2:]
    else:
        d = get_json(args[0], args[1]); args = args[2:]
    cT, cB, dT, dB, nodes, col, T, B = compare(d)
    # calls: compare per call index ignoring arg slots whose colour is 'junk' — report positional diffs
    onlyT = cT - cB; onlyB = cB - cT
    noncall_T = {k: v for k, v in onlyT.items() if k[0] != 'call'}
    noncall_B = {k: v for k, v in onlyB.items() if k[0] != 'call'}
    # call comparison by aligned index
    callsT = [e for e in sum(dT.values(), []) if e[0] == 'call']
    callsB = [e for e in sum(dB.values(), []) if e[0] == 'call']
    ct = {e[2]: e for e in callsT}; cb = {e[2]: e for e in callsB}
    calldiff = []
    for i in sorted(set(ct) | set(cb)):
        if i not in ct or i not in cb: calldiff.append((i, 'unaligned')); continue
        lt, ot, _ = nodes[ct[i][1]]; lb, ob, _ = nodes[cb[i][1]]
        names = (lt[1], lb[1])
        bad = [j for j in range(len(ot)) if col[ot[j]] != col[ob[j]]]
        calldiff.append((i, names, bad))
    nst = sum(v for k, v in cT.items() if k[0] == 'store'); nbr = sum(v for k, v in cT.items() if k[0] == 'br')
    nrt = sum(v for k, v in cT.items() if k[0] == 'ret')
    print(f"ret {RETREG or '-'} x{nrt} stores {nst} branches {nbr} calls {len(callsT)}/{len(callsB)} | store/br/ret diff T-only {sum(noncall_T.values())} B-only {sum(noncall_B.values())}")
    for k, v in noncall_T.items(): print('  T-only', v, k[:4], [x[1] for x in dT[k]])
    for k, v in noncall_B.items(): print('  B-only', v, k[:4], [x[1] for x in dB[k]])
    # explain: pair T-only/B-only events at the same instruction index
    evT = {e[1] if e[0] in ('store', 'trap', 'stkst', 'ret') else e[2]: e for k in noncall_T for e in dT[k]}
    evB = {e[1] if e[0] in ('store', 'trap', 'stkst', 'ret') else e[2]: e for k in noncall_B for e in dB[k]}
    for i in sorted(set(evT) & set(evB)):
        et, eb = evT[i], evB[i]
        if et[0] == 'stkst': pairs = list(zip(et[2], eb[2]))
        elif et[0] == 'store': pairs = list(zip(et[2][:1], eb[2][:1])) + list(zip(et[2][1:], eb[2][1:]))
        elif et[0] in ('ret', 'trap'): pairs = list(zip(et[2], eb[2]))
        else: pairs = [(et[1], eb[1])]
        for x, y in pairs:
            if x is None or y is None or col[x] == col[y]: continue
            r = explain(nodes, col, x, y)
            print(f'   explain @{i}:', r)
    if os.environ.get('PL'):
        for tn, bn in sorted(set(PLACEHOLDER_LOG)): print('  PLACEHOLDER', tn, bn)
    nameprob = []
    for i, *rest in calldiff:
        if rest[0] == 'unaligned': print('  call unaligned at', i); continue
        names, bad = rest
        nm = '' if names[0] == names[1] else f' NAME {names[0]} vs {names[1]}'
        if bad or nm:
            print(f'  call @{i} {names[1][:60]} argdiff {bad}{nm}')
            for j in bad:
                print('     explain arg', j, explain(nodes, col, nodes[ct[i][1]][1][j], nodes[cb[i][1]][1][j]))
    return 0

if __name__ == '__main__':
    main()
