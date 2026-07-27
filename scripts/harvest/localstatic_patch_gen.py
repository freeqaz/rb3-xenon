#!/usr/bin/env python3
"""Local-static Symbol/Message PATCH GENERATOR (laneAX-W1, 2026-07-27).

Companion to scripts/harvest/localstatic_population_scan.py, which only
*counts* the tell (target obj calls Symbol(const char*) / Message(Symbol) /
atexit more times than our compiled obj). This tool goes the rest of the way:
it RESOLVES THE STRING LITERAL each target-side ctor is passed and emits the
exact source line to insert, in target order.

★ HOW THE STRING IS RECOVERED
The dtk-split target obj contains only .text (+ .pdata); the string literals
live in .rdata, outside the split. But the reference survives as a pair of
relocations -- IMAGE_REL_PPC_REFHI(0x10)/REFLO(0x11), with a PAIR(0x12) -- onto
an *external* symbol named `lbl_<VA>`. That VA is an absolute address in the
retail image, and orig/45410914/band.exe is the decompressed PE of exactly that
image. So: reloc -> lbl_<VA> -> PE section table -> file offset -> NUL-terminated
string. No Ghidra, no build.

★ THE CANONICAL SHAPE (BandTrack::EnterCoda, target offsets)
    lis   r10, HI(guard)                 <- REFHI onto a .data VA
    lis   r11, HI(static_obj)
    addi  r30, r11, LO(static_obj)       <- REFLO, addi => "r30 = &static"
    lwz   r11, LO(guard)(r10)            <- REFLO, lwz  => "load guard word"
    rlwinm. r9, r11, 0, 31, 31           <- test this static's guard BIT
    bne   already_initialised
    ori   r11, r11, 1                    <- set the bit (imm == 1 << bitno)
    stw   r11, LO(guard)(r10)
    lis   r11, HI(str)                   <- the string, in .rdata
    addi  r3,  r31, 0x50                 <- &temp Symbol on the stack
    addi  r4,  r11, LO(str)              <- ARG 2 of Symbol(const char*)
    bl    ??0Symbol@@QAA@PBD@Z
    mr    r11, r3
    mr    r3,  r30
    lwz   r4,  0(r11)
    bl    ??0Message@@QAA@VSymbol@@@Z    <- present => it is a `static Message`
    lis   r11, HI(dtor_thunk); addi r3, r11, LO(dtor_thunk)
    bl    atexit
So the string is "whatever VA r4 holds at the call", recovered by a linear
forward propagation of "GPR n holds VA v" (addi/ori-with-relocation sets, `mr`
copies, gpr_defs() kills, `bl` kills the volatiles) -- NOT by "the nearest
preceding addr-of into r4", because MSVC hoists the whole block of string
materialisations to the top of the function and then feeds each ctor a bare
`mr r4, rNN`. The Symbol-vs-Message distinction is "is there a Message ctor bl
within the next few instructions", and its arity comes from the mangling.

Measured string-resolution rate in real (pinned, non-auto) TUs: 99.1% of
guard-verified statics on named symbols, 98.3% on anonymous ones. It is far
worse (~55-60%) inside `auto_NN_<VA>_text` carve objs, but that is a dtk
symbol-BOUNDS artifact -- an over-carved symbol swallows the next function, so
relocations from code that is not really in it get attributed to it -- and
those objs have no source file to edit anyway.

★ THE GUARD WORD IS THE PRECISION FILTER
A bare `Symbol s(some_str)` temporary also calls Symbol(const char*). Only a
function-LOCAL STATIC is wrapped in the guard-word test/set block above. This
tool therefore only reports LOCAL_STATIC when it can see the guard (a .data VA
touched by both a load and a store relocation in the window, plus the
power-of-two `ori`). Everything else is labelled TEMPORARY and excluded --
that is what keeps mispaired target symbols (an STL template that "carries 17
Symbol ctors") out of the actionable list.

★ Relocations are indexed by Sym.index, the true COFF SymbolTableIndex, NOT by
list position -- coffx.read_coff skips aux records (i += 1 + naux).

Usage:
  python3 scripts/harvest/localstatic_patch_gen.py <worktree> --sym <mangled>
  python3 scripts/harvest/localstatic_patch_gen.py <worktree> --unit BandTrack
  python3 scripts/harvest/localstatic_patch_gen.py <worktree> --all --json out.json
"""
import argparse, collections, glob, json, os, re, struct, sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'analysis'))
from coffx import read_coff, infer_sizes, K_SEC

R_REFHI, R_REFLO, R_PAIR, R_REL24 = 0x10, 0x11, 0x12, 0x06

SYMBOL_CTOR = '??0Symbol@@QAA@PBD@Z'
MESSAGE_CTOR = '??0Message@@QAA@VSymbol@@@Z'
ATEXIT = 'atexit'

# Every Message ctor arity. `Message(Symbol)` is the 0-extra-arg form; the rest
# take 1..N DataNode arguments, back-referenced in the mangling as `1`s:
#   ??0Message@@QAA@VSymbol@@ABVDataNode@@@Z      -> 1 DataNode
#   ??0Message@@QAA@VSymbol@@ABVDataNode@@1@Z     -> 2 DataNodes ... etc.
# Missing these was a real recall hole: NewAwardPanel's
# `static Message cUpdateProviderMsg("update_provider", 0)` shows up as a bare
# Symbol(const char*) unless the 1-DataNode ctor is also a recognised sink.
MSG_ARITY_RE = re.compile(r'^\?\?0Message@@QAA@VSymbol@@ABVDataNode@@(1*)@Z$')


def msg_arity(name):
    """-> number of DataNode args for a Message ctor mangling, else None."""
    if name == MESSAGE_CTOR:
        return 0
    m = MSG_ARITY_RE.match(name)
    return len(m.group(1)) + 1 if m else None


# ---------------------------------------------------------------- PE image
class Image:
    """Decompressed retail PE (orig/45410914/band.exe): VA -> bytes/section."""

    def __init__(self, path):
        d = open(path, 'rb').read()
        self.d = d
        pe = struct.unpack_from('<I', d, 0x3c)[0]
        _m, nsec, _t, _so, _ns, optsz, _c = struct.unpack_from('<HHIIIHH', d, pe + 4)
        self.base = struct.unpack_from('<I', d, pe + 24 + 28)[0]
        so = pe + 24 + optsz
        self.segs = []
        for i in range(nsec):
            o = so + i * 40
            nm = d[o:o + 8].rstrip(b'\0').decode('ascii', 'replace')
            vsz, va, rsz, rp = struct.unpack_from('<IIII', d, o + 8)
            self.segs.append((self.base + va, max(vsz, rsz), rp, rsz, nm))

    def seg(self, va):
        for s in self.segs:
            if s[0] <= va < s[0] + s[1]:
                return s
        return None

    def section(self, va):
        s = self.seg(va)
        return s[4] if s else None

    def cstr(self, va, maxlen=256):
        s = self.seg(va)
        if not s or not s[3]:
            return None
        off = s[2] + (va - s[0])
        e = self.d.find(b'\0', off, off + maxlen)
        if e < 0:
            return None
        try:
            return self.d[off:e].decode('ascii')
        except UnicodeDecodeError:
            return None


# ------------------------------------------------------------ ppc decoding
def op(w):
    return w >> 26


def dest_of_addrform(w):
    """If w is an address-forming instruction, return its destination register."""
    o = op(w)
    if o == 14:                       # addi rD, rA, simm
        return (w >> 21) & 31
    if o == 24 or o == 25:            # ori / oris rA, rS, uimm  -> dest is rA
        return (w >> 16) & 31
    return None


MEM_OPS = {32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
           48, 49, 50, 51, 52, 53, 54, 55}


def is_mem(w):
    return op(w) in MEM_OPS


def is_store(w):
    return op(w) in (36, 37, 38, 39, 44, 45, 46, 47, 52, 53, 54, 55)


# --- which GPRs an instruction defines (needed for the register propagator).
# MSVC HOISTS the string-pointer materialisation: AccomplishmentConditional's
# fn_8266AAC0 builds five `addi rNN, rX, LO(str)` at the top of the function and
# then feeds each ctor with a plain `mr r4, rNN`. "Nearest preceding addr-of
# whose destination is r4" therefore resolves nothing there -- 32.5% of all
# sites were stringless before this.
_X_DEF_RA = {28, 60, 444, 412, 316, 476, 124, 284, 24, 536, 792, 824, 954,
             922, 986, 26, 27, 539, 794, 413, 122, 508, 58, 4}
_X_DEF_RD = {266, 10, 138, 40, 8, 136, 104, 235, 75, 11, 491, 459, 233, 489,
             457, 202, 234, 200, 232, 339, 19, 23, 87, 279, 341, 21, 20, 279,
             311, 343, 375, 55, 119, 279, 534, 790, 533, 597, 0, 32}


def gpr_defs(w):
    """Set of GPRs written by `w`. None means 'unknown -- assume anything'."""
    o = op(w)
    if o in (7, 8, 12, 13, 14, 15):
        return {(w >> 21) & 31}
    if o in (24, 25, 26, 27, 28, 29, 20, 21, 23, 30):
        return {(w >> 16) & 31}
    if o in (32, 34, 40, 42, 46, 58):
        return {(w >> 21) & 31}
    if o in (33, 35, 41, 43):
        return {(w >> 21) & 31, (w >> 16) & 31}
    if o in (48, 50, 62):
        return set()
    if o in (49, 51, 53, 55):
        return {(w >> 16) & 31}
    if o in (36, 38, 44, 47, 52, 54, 62):
        return set()
    if o in (37, 39, 45):
        return {(w >> 16) & 31}
    if o in (3, 10, 11, 16, 17, 18, 19):
        return set()
    if o in (4, 5, 6, 56, 57, 59, 60, 61, 63):
        return set()          # FP / VMX128: no GPR effect worth tracking
    if o == 31:
        xo = (w >> 1) & 0x3FF
        if xo in _X_DEF_RA:
            return {(w >> 16) & 31}
        if xo in _X_DEF_RD:
            return {(w >> 21) & 31}
        if xo in (151, 183, 215, 247, 407, 439, 663, 727, 662, 918, 210, 242,
                  146, 467, 512, 4, 598, 854, 982, 470, 54, 86, 1010, 306):
            return set()      # stores / barriers / cache / mtspr
        return {(w >> 21) & 31, (w >> 16) & 31}
    return None


VOLATILE = {0, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}


def ori_bit(w):
    """`ori rA,rS,1<<k` -> k ; `oris rA,rS,1<<k` -> 16+k ; else None."""
    o = op(w)
    if o not in (24, 25):
        return None
    imm = w & 0xFFFF
    if imm == 0 or (imm & (imm - 1)):
        return None
    return imm.bit_length() - 1 + (16 if o == 25 else 0)


# ---------------------------------------------------------- target symbols
LBL_RE = re.compile(r'^(?:lbl|fn|jumptable|byte|word|dword|float|double|off|jtbl|__unwind\$|__catch\$)?_?(?:__)?')
VA_RE = re.compile(r'_(8[0-9A-Fa-f]{7})$')


def sym_va(name):
    """External dtk symbol name -> absolute VA, or None."""
    m = VA_RE.search(name)
    return int(m.group(1), 16) if m else None


def load_target_map(wt):
    """VA(lowercase '0x...') -> mangled name, plus the reverse for our tells."""
    p = os.path.join(wt, 'scripts/target_symbol_map.json')
    raw = json.load(open(p))
    tmap = {k.lower(): v for k, v in raw.items()
            if isinstance(v, str) and k.startswith('0x')}
    rev = {}
    for k, v in tmap.items():
        rev.setdefault(v, k)
    return tmap, rev


# ------------------------------------------------------------- identifiers
IDENT_RE = re.compile(r'^[A-Za-z_][A-Za-z0-9_]*$')


def load_decl_idents(wt):
    """string -> declared global identifier, from Symbols*.h / Messages*.h."""
    sym, msg = {}, {}
    for f in glob.glob(os.path.join(wt, 'src/system/utl/Symbols*.h')):
        for line in open(f, errors='replace'):
            m = re.match(r'\s*extern\s+Symbol\s+([A-Za-z_]\w*)\s*;', line)
            if m:
                sym.setdefault(m.group(1), m.group(1))
    for f in glob.glob(os.path.join(wt, 'src/system/utl/Messages*.h')):
        for line in open(f, errors='replace'):
            m = re.match(r'\s*extern\s+Message\s+([A-Za-z_]\w*)_msg\s*;', line)
            if m:
                msg.setdefault(m.group(1), m.group(1) + '_msg')
    return sym, msg


def ident_for(s, kind, declared_sym, declared_msg):
    """Source identifier for a static holding string `s`."""
    if kind == 'Message':
        if s in declared_msg:
            return declared_msg[s], True
        base = re.sub(r'\W', '_', s)
        if not IDENT_RE.match(base):
            base = '_' + base
        return base + '_msg', False
    if s in declared_sym:
        return declared_sym[s], True
    base = re.sub(r'\W', '_', s)
    if not IDENT_RE.match(base):
        base = 's_' + base
    return base, False


# ----------------------------------------------------------------- scanner
class Site(dict):
    pass


def scan_function(sec, s, byidx, img, tellname, back=0x80, fwd=0x30):
    """-> list of Site dicts for one code symbol of the TARGET obj."""
    lo, hi = s.value, s.value + s.size
    if hi > len(sec.data):
        return []
    rel = collections.defaultdict(list)
    for (va, si, typ) in sec.relocs:
        if lo <= va < hi:
            t = byidx.get(si)
            if t is not None:
                rel[va].append((typ, t))

    words = {}
    for o in range(lo, hi - 3, 4):
        words[o] = struct.unpack_from('>I', sec.data, o)[0]

    # pass 1: guard words, FUNCTION-WIDE. MSVC hoists the guard LOAD --
    # BandStarDisplay::SyncProperty loads guard 0x82CBD140 once at +0x80 and
    # reuses r11 for the second static's bit test 0xa0 later, so a windowed
    # load+store test drops that static outright.
    memtouch = []
    for a in sorted(rel):
        w = words.get(a)
        if w is None:
            continue
        for (typ, t) in rel[a]:
            if typ != R_REFLO:
                continue
            tva = sym_va(t.name)
            if tva is not None and dest_of_addrform(w) is None and is_mem(w):
                memtouch.append((a, tva, is_store(w)))
    loaded = {x[1] for x in memtouch if not x[2]}
    stored = {x[1] for x in memtouch if x[2]}
    guards = {v for v in (loaded & stored) if img.section(v) == '.data'}

    # pass 2: linear forward propagation of "GPR n currently holds VA v".
    reg = {}
    calls = []            # (offset, tell, string_va_in_r4)
    addrof = []           # (offset, reg, va)
    for a in range(lo, hi, 4):
        w = words.get(a)
        if w is None:
            continue
        rr = rel.get(a) or []
        tell = None
        setva = None
        for (typ, t) in rr:
            if typ == R_REL24:
                tell = tell or tellname(t.name)
            elif typ == R_REFLO:
                tva = sym_va(t.name)
                d = dest_of_addrform(w)
                if tva is not None and d is not None:
                    setva = (d, tva)
        if tell:
            calls.append((a, tell, reg.get(4)))
            for v in VOLATILE:
                reg.pop(v, None)
            continue
        if setva:
            reg[setva[0]] = setva[1]
            addrof.append((a, setva[0], setva[1]))
            continue
        if op(w) == 31 and ((w >> 1) & 0x3FF) == 444 \
                and ((w >> 21) & 31) == ((w >> 11) & 31):   # mr rA, rS
            src_r, dst_r = (w >> 21) & 31, (w >> 16) & 31
            if src_r in reg:
                reg[dst_r] = reg[src_r]
            else:
                reg.pop(dst_r, None)
            continue
        if op(w) in (16, 18, 19):
            continue
        dd = gpr_defs(w)
        if dd is None:
            reg.clear()
        else:
            for r in dd:
                reg.pop(r, None)

    sites = []
    pend = None   # a Symbol(const char*) awaiting a possible Message ctor

    def flush():
        if pend is not None:
            sites.append(pend)

    for (a, tn, r4) in calls:
        if tn == 'ATEXIT':
            continue
        if tn.startswith('MSG:'):
            arity = int(tn[4:])
            if pend is not None and 0 < a - pend['off_end'] <= fwd:
                pend['kind'] = 'Message'
                pend['arity'] = arity
                pend['off_msg'] = a - lo
                sites.append(pend)
                pend = None
                continue
            flush(); pend = None
            sites.append(Site(kind='Message', arity=arity, string=None,
                              off=a - lo, off_end=a, guard_va=None,
                              guard_bit=None, static_va=None,
                              form='MESSAGE_FROM_GLOBAL'))
            continue
        if tn != 'SYM':
            continue
        flush()
        strva = r4 if (r4 is not None and
                       img.section(r4) in ('.rdata', '.text')) else None
        st = img.cstr(strva) if strva is not None else None
        # the guard SET (`ori rX,rX,1<<k` ; `stw rX, LO(guard)(rY)`) sits a few
        # instructions before the ctor -- that is the local-static proof.
        guard_va = guard_bit = None
        gs = [x for x in memtouch if x[2] and x[1] in guards
              and 0 < a - x[0] <= back]
        if gs:
            gaddr, guard_va = gs[-1][0], gs[-1][1]
            for o in range(gaddr - 4, max(lo, gaddr - 0x20) - 4, -4):
                b = ori_bit(words.get(o, 0))
                if b is not None and not rel.get(o):
                    guard_bit = b
                    break
        sc = [x for x in addrof if x[0] < a and a - x[0] <= back
              and x[2] not in guards and img.section(x[2]) == '.data']
        pend = Site(kind='Symbol', arity=None, string=st, string_va=strva,
                    string_sec=(img.section(strva) if strva else None),
                    off=a - lo, off_end=a, guard_va=guard_va,
                    guard_bit=guard_bit, static_va=(sc[-1][2] if sc else None),
                    form='LOCAL_STATIC' if guard_va is not None else 'TEMPORARY')
    flush()
    for x in sites:
        x.pop('off_end', None)
        if x['form'] != 'MESSAGE_FROM_GLOBAL':
            x['form'] = 'LOCAL_STATIC' if x['guard_va'] is not None else 'TEMPORARY'
    return sites



# ------------------------------------------------------- base-side scanner
# Our compiled obj has the SAME shape, but everything is a defined symbol
# rather than an absolute VA:
#     lis  r30, HI(?$S5@?5??SyncProperty@...@4IA)      <- the guard word
#     lis  r11, HI(?view_time_easy@?5??SyncProperty@...@4VSymbol@@A)
#     addi r29, r11, LO(...)                            <- &static
#     lwz  r11, LO(?$S5@...)(r30)                       <- guard load
#     ...  ori r11,r11,1 ; stw r11, LO(?$S5@...)(r30)
#     lis/addi r4, ??_C@_0P@JMJBKIAB@view_time_easy?$AA@ <- the literal
#     bl   ??0Symbol@@QAA@PBD@Z
# So the guard test is "the relocation target is a $S / ??_B guard symbol",
# and the string is read straight out of this obj's own .rdata. Running the
# IDENTICAL guard-filtered logic on both sides is what makes the census
# symmetric -- the old scan just counted raw ctor relocations, so a plain
# `Symbol s(str)` temporary on either side skewed the difference.
def build_tellname(rev):
    """-> f(target symbol name) -> 'SYM' | 'MSG:<arity>' | 'ATEXIT' | None.

    The target symbol renamer only renames symbols DEFINED in an obj, so a call
    OUT of the TU shows up as `fn_<VA>`; resolve those through the VA the
    target symbol map assigns to each ctor.
    """
    direct = {SYMBOL_CTOR: 'SYM', ATEXIT: 'ATEXIT'}
    for n in rev:
        a = msg_arity(n)
        if a is not None:
            direct[n] = 'MSG:%d' % a
    va_of = {rev[n]: t for n, t in direct.items() if n in rev}

    def f(nm):
        if nm.startswith('fn_'):
            return va_of.get('0x' + nm[3:].lower())
        return direct.get(nm) or direct.get(nm.lstrip('_'))
    return f


def is_guard_sym(nm):
    return nm.startswith('??_B') or nm.startswith('$S') or nm.startswith('?$S')


def scan_function_base(secs, sec, s, byidx, back=0x80, fwd=0x30):
    lo, hi = s.value, s.value + s.size
    if hi > len(sec.data):
        return []
    words = {o: struct.unpack_from('>I', sec.data, o)
             for o in range(lo, hi - 3, 4)}
    words = {o: v[0] for o, v in words.items()}
    rel = collections.defaultdict(list)
    for (va, si, typ) in sec.relocs:
        if lo <= va < hi and si in byidx:
            rel[va].append((typ, byidx[si]))

    def cstr(t):
        if t.sec <= 0 or t.sec - 1 >= len(secs):
            return None
        sd = secs[t.sec - 1].data
        e = sd.find(b'\0', t.value, t.value + 256)
        if e < 0:
            return None
        try:
            return sd[t.value:e].decode('ascii')
        except UnicodeDecodeError:
            return None

    # same linear GPR propagation as the target side (MSVC hoists here too)
    gstore, calls = [], []
    reg = {}
    for a in range(lo, hi, 4):
        w = words.get(a)
        if w is None:
            continue
        rr = rel.get(a) or []
        tell = None
        setva = None
        for (typ, t) in rr:
            if typ == R_REL24:
                if t.name == SYMBOL_CTOR or t.name.lstrip('_') == SYMBOL_CTOR:
                    tell = 'SYM'
                else:
                    ar = msg_arity(t.name)
                    if ar is not None:
                        tell = 'MSG:%d' % ar
            elif typ == R_REFLO:
                d = dest_of_addrform(w)
                if d is not None:
                    setva = (d, t)
                elif is_mem(w) and is_store(w) and is_guard_sym(t.name):
                    gstore.append((a, t.name))
        if tell:
            calls.append((a, tell, reg.get(4)))
            for v in VOLATILE:
                reg.pop(v, None)
            continue
        if setva:
            reg[setva[0]] = setva[1]
            continue
        if op(w) == 31 and ((w >> 1) & 0x3FF) == 444 \
                and ((w >> 21) & 31) == ((w >> 11) & 31):
            sr, dr = (w >> 21) & 31, (w >> 16) & 31
            if sr in reg:
                reg[dr] = reg[sr]
            else:
                reg.pop(dr, None)
            continue
        if op(w) in (16, 18, 19):
            continue
        dd = gpr_defs(w)
        if dd is None:
            reg.clear()
        else:
            for r in dd:
                reg.pop(r, None)

    sites, pend = [], None

    def flush():
        if pend is not None:
            sites.append(pend)

    for (a, tn, r4) in calls:
        if tn.startswith('MSG:'):
            if pend is not None and 0 < a - pend['off_end'] <= fwd:
                pend['kind'] = 'Message'
                pend['arity'] = int(tn[4:])
                sites.append(pend)
                pend = None
            continue
        flush()
        st = cstr(r4) if r4 is not None else None
        g = [x for x in gstore if 0 < a - x[0] <= back]
        pend = Site(kind='Symbol', arity=None, string=st, off=a - lo,
                    off_end=a, guard_va=(g[-1][1] if g else None),
                    guard_bit=None, static_va=None,
                    form='LOCAL_STATIC' if g else 'TEMPORARY')
    flush()
    for x in sites:
        x.pop('off_end', None)
    return sites


def scan_obj_base(path, sizes=None):
    """-> {func_name: [Site]} of guard-verified local statics in our obj.

    If `sizes` is a dict it is filled with {func_name: byte size} for EVERY code
    symbol -- the census uses it as a mispair discriminator (a target symbol
    several times the size of the function we compiled under that name is not
    the same function).
    """
    try:
        data = open(path, 'rb').read()
    except OSError:
        return None
    secs, syms = read_coff(data)
    if secs is None:
        return None
    infer_sizes(secs, syms)
    byidx = {s.index: s for s in syms}
    out = {}
    for s in syms:
        if s.sec <= 0 or s.size == 0 or s.kind == K_SEC or s.cls not in (2, 3):
            continue
        sec = secs[s.sec - 1]
        if not sec.is_code:
            continue
        if sizes is not None:
            sizes[s.name] = max(sizes.get(s.name, 0), s.size)
        st = [x for x in scan_function_base(secs, sec, s, byidx)
              if x['form'] == 'LOCAL_STATIC']
        if st:
            out.setdefault(s.name, []).extend(st)
    return out


def scan_obj(path, img, tellname, want=None):
    try:
        data = open(path, 'rb').read()
    except OSError:
        return {}
    secs, syms = read_coff(data)
    if secs is None:
        return {}
    infer_sizes(secs, syms)
    byidx = {s.index: s for s in syms}
    out = {}
    for s in syms:
        if s.sec <= 0 or s.size == 0 or s.kind == K_SEC or s.cls not in (2, 3):
            continue
        sec = secs[s.sec - 1]
        if not sec.is_code:
            continue
        if want and s.name not in want:
            continue
        sites = scan_function(sec, s, byidx, img, tellname)
        if sites:
            out.setdefault(s.name, (s.size, sites, (s.sec, s.value)))
    return {k: v for k, v in
            sorted(out.items(), key=lambda kv: kv[1][2])}


# ------------------------------------------------------------------- main
def emit(unit, name, pct, size, sites, declared_sym, declared_msg, show_temp):
    hdr = '%s  %s' % (unit, name)
    if pct is not None:
        hdr += '  (%.2f%%)' % pct
    hdr += '  [target size 0x%x]' % size
    lines = [hdr]
    for st in sites:
        if st['form'] == 'TEMPORARY' and not show_temp:
            continue
        if st['form'] == 'MESSAGE_FROM_GLOBAL':
            lines.append('    +0x%-5x  <Message(Symbol) with no adjacent '
                         'Symbol(const char*) -- built from a global>' % st['off'])
            continue
        s = st['string']
        if s is None:
            lines.append('    +0x%-5x  <%s: string not resolved (va=%s)>'
                         % (st['off'], st['kind'],
                            hex(st['string_va']) if st.get('string_va') else '?'))
            continue
        ident, known = ident_for(s, st['kind'], declared_sym, declared_msg)
        tag = '' if known else '   // NOT a declared global'
        note = '' if st['form'] == 'LOCAL_STATIC' else '   // TEMPORARY (no guard word)'
        if st['kind'] == 'Message':
            args = ''.join(', 0' for _ in range(st.get('arity') or 0))
            lines.append('    +0x%-5x  static Message %s("%s"%s);%s%s'
                         % (st['off'], ident, s, args, tag, note))
        else:
            lines.append('    +0x%-5x  static Symbol  %s("%s");%s%s'
                         % (st['off'], ident, s, tag, note))
    return lines if len(lines) > 1 else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('worktree')
    ap.add_argument('--sym', action='append', default=[])
    ap.add_argument('--unit')
    ap.add_argument('--all', action='store_true')
    ap.add_argument('--show-temporaries', action='store_true')
    ap.add_argument('--json')
    a = ap.parse_args()
    wt = a.worktree
    img = Image(os.path.join(wt, 'orig/45410914/band.exe'))
    _tmap, rev = load_target_map(wt)
    tellname = build_tellname(rev)
    declared_sym, declared_msg = load_decl_idents(wt)

    pct = {}
    rp = os.path.join(wt, 'build/45410914/report.json')
    if os.path.exists(rp):
        for u in json.load(open(rp))['units']:
            for f in (u.get('functions') or []):
                pct[(u['name'].split('/', 1)[-1], f['name'])] = \
                    f['match_percent_normalized']

    root = os.path.join(wt, 'build/45410914/obj')
    paths = sorted(glob.glob(os.path.join(root, '**', '*.obj'), recursive=True))
    if a.unit:
        paths = [p for p in paths if a.unit.lower() in os.path.relpath(p, root).lower()]
    want = set(a.sym) or None

    out, nsym, nmsg, nfn = [], 0, 0, 0
    js = []
    for p in paths:
        unit = os.path.relpath(p, root)[:-4]
        got = scan_obj(p, img, tellname, want)
        for name, (size, sites, _ord) in got.items():
            keep = [s for s in sites
                    if s['form'] == 'LOCAL_STATIC' or a.show_temporaries
                    or s['form'] == 'MESSAGE_FROM_GLOBAL']
            if not keep:
                continue
            pc = pct.get((unit, name))
            blk = emit(unit, name, pc, size, sites, declared_sym, declared_msg,
                       a.show_temporaries)
            if blk:
                out.append('\n'.join(blk))
                nfn += 1
                for s in keep:
                    if s['form'] != 'LOCAL_STATIC':
                        continue
                    if s['kind'] == 'Message':
                        nmsg += 1
                    else:
                        nsym += 1
            js.append({'unit': unit, 'sym': name, 'pct': pc, 'size': size,
                       'sites': [dict(s) for s in sites]})
    print('\n\n'.join(out))
    print('\n== %d functions, %d static Symbol, %d static Message =='
          % (nfn, nsym, nmsg))
    if a.json:
        json.dump(js, open(a.json, 'w'), indent=1)


if __name__ == '__main__':
    main()
