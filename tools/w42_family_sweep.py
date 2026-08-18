#!/usr/bin/env python3
"""
w42_family_sweep.py -- systematic detector for NAME-PERMUTATION FAMILIES.

THE DEFECT CLASS
----------------
A *uniformly* wrong name family is invisible to objdiff's `name_check` ruler,
because every member is wrong consistently, so nothing that references them can
disagree.  (W31: a forwarder whose only relocations are a recursive self-call
plus a correctly-mapped callee is never charged at all.)  Six such families have
been found on this campaign -- every one of them BY ACCIDENT, while a lane was
doing something else.

So do not hunt by looking at charged rows.  Hunt for STRUCTURAL INCONSISTENCY
BETWEEN OUR SOURCE'S CALL GRAPH AND THE MAP'S.

THE ARBITER
-----------
*** THE SOURCE IS THE ARBITER, NOT NAME EQUALITY. ***  (lane W34)

W31's `fwdscan.py` flagged `Outer::X -> Inner::Y` whenever the method names X
and Y differed.  W34 proved that screen wrong: `Game::OnPlayerRemoved ->
TrackerManager::HandleRemovePlayer` is flagged by name-equality and is NOT a
defect -- `Game.cpp:1138` says exactly that delegation.  Three further rows at
the TrackerManager level are legitimate for the same reason.

This tool therefore takes the arbiter from OUR COMPILED OBJECTS.  A function's
outgoing `IMAGE_REL_PPC_REL24` relocations *are* our source's call graph,
extracted mechanically -- no source-text parsing, no heuristics.  We flag a row
only when

    our obj says   OurClass::X  calls  A
    the map says   retail's X   calls  B        (A != B)

i.e. the two call graphs are structurally inconsistent.  Legitimate delegation
with differing names is silent, because our source and the map agree.

DECODING NOTES (both are load-bearing, both learned the hard way)
----------------------------------------------------------------
* Branches are decoded by RAW WORD MASKING, never by capstone.  W31 recorded
  that linear disassembly desyncs and SILENTLY DROPS ROWS -- capstone skips
  words it cannot decode (VMX128), and a dropped row is a false negative shaped
  like a clean result.
* Call sequences are compared POSITIONALLY, not by offset.  Our COMDAT may carry
  an 8-byte EH prefix that retail's extent does not, so every offset is shifted;
  comparing the ordered SEQUENCE of call targets is immune to that.

OUTPUT TIERS
------------
  T1  our side has exactly 1 call, retail has exactly 1 call.  The
      correspondence is unambiguous.  High confidence.
  T2  both sides have the same call count k>1; paired positionally.  Lower
      confidence -- differing inlining can shift the sequence.

Neither tier is a verdict.  Every hit still needs the standing checks:
byte geometry (is the row a dtk mis-carve phantom?), does the thing needing a
name EXIST, does the base obj DEFINE the name being moved, and map injectivity.

Usage:
    python3 tools/w42_family_sweep.py [--json OUT] [--tier2] [--fanin]
"""
import argparse
import bisect
import collections
import glob
import json
import os
import re
import struct
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(REPO, 'orig/45410914/band.exe')
MAPPATH = os.path.join(REPO, 'scripts/target_symbol_map.json')

TEXT_VA = 0x82270000
TEXT_OFF = 0x264e00
TEXT_SZ = 0x9dce3c
PD_OFF = 0x1f1600
PD_SZ = 0x70c28

REL24 = 6  # IMAGE_REL_PPC_REL24 -- the branch relocation


# ---------------------------------------------------------------- retail side

_exe = None


def exe():
    global _exe
    if _exe is None:
        _exe = open(EXE, 'rb').read()
    return _exe


def text_word(va):
    o = va - TEXT_VA + TEXT_OFF
    return struct.unpack_from('>I', exe(), o)[0]


_pd = None


def pdata():
    """Authoritative function extents.  BIG-ENDIAN; proglen = w & 0xFF,
    funclen = (w >> 8) & 0x3FFFFF, in WORDS.  A wrong decode reads 8-byte
    forwarders as 2,560 B -- validate against a known extent before trusting."""
    global _pd
    if _pd is None:
        d = exe()[PD_OFF:PD_OFF + PD_SZ]
        out = []
        for i in range(0, len(d), 8):
            beg, w = struct.unpack_from('>II', d, i)
            if beg == 0:
                continue
            out.append((beg, ((w >> 8) & 0x3FFFFF) * 4))
        out.sort()
        _pd = out
    return _pd


_pdstarts = None


def pdstarts():
    global _pdstarts
    if _pdstarts is None:
        _pdstarts = [e[0] for e in pdata()]
    return _pdstarts


def pd_extent(va):
    s = pdstarts()
    i = bisect.bisect_right(s, va) - 1
    if i < 0:
        return None
    b, l = pdata()[i]
    return (b, l) if b <= va < b + l else None


_map = None


def symmap():
    global _map
    if _map is None:
        m = json.load(open(MAPPATH))
        _map = {int(k, 16): v for k, v in m.items() if k.startswith('0x')}
    return _map


_mapstarts = None


def mapstarts():
    global _mapstarts
    if _mapstarts is None:
        _mapstarts = sorted(v for v in symmap() if TEXT_VA <= v < TEXT_VA + TEXT_SZ)
    return _mapstarts


def retail_extent(va, cap=512):
    """Bound a retail body.  Prefer .pdata (authoritative).  Tiny leaf stubs
    have NO .pdata entry by construction -- an 8-byte leaf touches neither the
    stack nor LR, so it gets no unwind record -- so fall back to the distance to
    the next known boundary."""
    e = pd_extent(va)
    if e and e[0] == va:
        return e[1], True
    lim = va + cap
    s = mapstarts()
    i = bisect.bisect_right(s, va)
    if i < len(s):
        lim = min(lim, s[i])
    ps = pdstarts()
    j = bisect.bisect_right(ps, va)
    if j < len(ps):
        lim = min(lim, ps[j])
    return max(4, lim - va), False


def retail_calls(va, size):
    """Ordered outgoing call targets, by RAW WORD decode.

    opcode 18 = I-form branch (b/ba/bl/bla): AA = w&2, LK = w&1.
    We collect both `bl` (call) and a terminal `b` (tail call) whose target
    lands outside [va, va+size) -- a forwarder's whole content is that tail
    call."""
    out = []
    for off in range(0, size, 4):
        if not (TEXT_VA <= va + off < TEXT_VA + TEXT_SZ):
            break
        w = text_word(va + off)
        if (w >> 26) != 18:
            continue
        aa = w & 2
        lk = w & 1
        li = w & 0x03FFFFFC
        if li & 0x02000000:
            li -= 0x04000000
        tgt = li if aa else (va + off + li)
        if lk:
            out.append((off, tgt, 'bl'))
        else:
            if not (va <= tgt < va + size):
                out.append((off, tgt, 'b'))
    return out


# ------------------------------------------------------------------ our side

def parse_obj(path):
    """Return (defined_funcs, calls) for one compiled obj.

    defined_funcs: name -> section index (1-based) for symbols in a code section
    calls: section index -> ordered [(offset, target_symbol_name)] REL24 relocs
    """
    d = open(path, 'rb').read()
    if len(d) < 20:
        return {}, {}
    mach, nsec, ts, psym, nsym, osz, ch = struct.unpack_from('<HHIIIHH', d, 0)
    if nsec == 0 or nsym == 0:
        return {}, {}
    off = 20 + osz
    secs = []
    for i in range(nsec):
        b = d[off:off + 40]
        nm = b[:8].rstrip(b'\0').decode('latin1', 'replace')
        vs, va, rs, pr, prel, pln, nrel, nln, chr_ = struct.unpack_from('<IIIIIIHHI', b, 8)
        secs.append(dict(name=nm, prel=prel, nrel=nrel))
        off += 40
    strt = psym + nsym * 18
    syms = []
    i = 0
    while i < nsym:
        o = psym + i * 18
        raw = d[o:o + 8]
        if raw[:4] == b'\0\0\0\0':
            so = struct.unpack_from('<I', raw, 4)[0]
            try:
                e = d.index(b'\0', strt + so)
                nm = d[strt + so:e].decode('latin1', 'replace')
            except ValueError:
                nm = ''
        else:
            nm = raw.rstrip(b'\0').decode('latin1', 'replace')
        val, sec, typ, cls, naux = struct.unpack_from('<IhHBB', d, o + 8)
        syms.append((nm, sec, val, cls, naux))
        i += 1 + naux
    idx = {n: (nm, sec, val) for n, (nm, sec, val, cls, naux) in enumerate(syms)}
    # symbol table index -> name needs the *unexpanded* index (aux records count)
    flat = []
    i = 0
    while i < nsym:
        o = psym + i * 18
        raw = d[o:o + 8]
        if raw[:4] == b'\0\0\0\0':
            so = struct.unpack_from('<I', raw, 4)[0]
            try:
                e = d.index(b'\0', strt + so)
                nm = d[strt + so:e].decode('latin1', 'replace')
            except ValueError:
                nm = ''
        else:
            nm = raw.rstrip(b'\0').decode('latin1', 'replace')
        val, sec, typ, cls, naux = struct.unpack_from('<IhHBB', d, o + 8)
        flat.append((i, nm, sec, val, typ, cls))
        i += 1 + naux
    byindex = {i: (nm, sec, val, typ, cls) for i, nm, sec, val, typ, cls in flat}

    defined = {}
    for i, nm, sec, val, typ, cls in flat:
        if sec > 0 and sec <= len(secs) and secs[sec - 1]['name'].startswith('.text'):
            # a function symbol: MSVC emits DTYPE_FUNCTION (typ>>4 == 2)
            if (typ >> 4) == 2 or nm.startswith('?') or nm.startswith('_'):
                if nm and nm not in defined:
                    defined[nm] = (sec, val)

    calls = collections.defaultdict(list)
    for si, s in enumerate(secs, 1):
        if not s['name'].startswith('.text'):
            continue
        for r in range(s['nrel']):
            ro = s['prel'] + r * 10
            if ro + 10 > len(d):
                break
            rva, symi, rtyp = struct.unpack_from('<IIH', d, ro)
            if rtyp != REL24:
                continue
            t = byindex.get(symi)
            if t is None:
                continue
            calls[si].append((rva, t[0]))
    for si in calls:
        calls[si].sort()
    return defined, calls


# ---------------------------------------------------------------------- main

def lead_name(n):
    """The method/function name at the head of a mangled symbol, handling
    templated functions (`??$name@`), specials (`??1`, `??_G`, ...) and
    templated CLASSES (`?insert@?$list@...`), which a naive
    `^\\?(\\w+)@(\\w+)@@` pattern silently fails on -- and that failure made the
    template-fold filter never fire on precisely the rows that needed it."""
    if not n:
        return None
    m = re.match(r'^\?\?\$(\w+)@', n)      # templated function
    if m:
        return m.group(1)
    m = re.match(r'^\?\?(_?[A-Z0-9])', n)  # special (ctor/dtor/vector-dtor/...)
    if m:
        return '??' + m.group(1)
    m = re.match(r'^\?(~?\w+)@', n)        # ordinary method or free function
    if m:
        return m.group(1)
    return None


def template_head(n):
    """First template head in the qualifier chain, e.g. `list` for
    `?insert@?$list@PAVRndDrawable@@...`."""
    m = re.search(r'\?\$(\w+)@', n or '')
    return m.group(1) if m else None


def method(n):
    """(method, class) from a mangled name, else None."""
    if not n:
        return None
    nm = lead_name(n)
    if nm is None:
        return None
    m = re.match(r'^\?(?:\?\$)?~?\w+@(\w+)@@', n)
    if m:
        return nm, m.group(1)
    th = template_head(n)
    if th:
        return nm, '?$' + th
    if re.match(r'^\?~?\w+@@', n):
        return nm, '<free>'
    return nm, '?'


def _template_args_differ(a, b):
    """True when two spellings are the same method on the same template, with
    only the template ARGUMENTS differing -- e.g. list<RndPollable*>::insert vs
    list<CharClip*>::insert.  Retail folds these wholesale (their differing
    per-T relocation targets fold transitively), so a one-level byte comparison
    reports them DIFFERENT and is wrong to."""
    if '?$' not in a or '?$' not in b:
        return False
    if lead_name(a) != lead_name(b):
        return False
    # same method name, both templated, and the template HEAD agrees
    ha, hb = template_head(a), template_head(b)
    return bool(ha and hb and ha == hb)


def owning_class(n):
    """Best-effort owning class for grouping.  Template-heavy STL spellings are
    bucketed as <stl> -- that stratum is fold-dominated (ICF folds template
    instantiations wholesale) and is reported separately, never mixed with the
    game-layer forwarders where this lever has actually paid."""
    if not n:
        return '?'
    if '?$' in n:
        return '<stl/template>'
    m = method(n)
    return m[1] if m else '?'


# ------------------------------------------------------- fold adjudication

def alias_classes(path):
    """Union-find over scripts/symbol_aliases.json.  Two spellings in one group
    are a PROVEN (or at least map-consistent) ICF fold, so a disagreement
    between them is NOT a defect.  Memory rule: grep symbol_aliases.json BEFORE
    believing a reloc-name find."""
    parent = {}

    def find(x):
        parent.setdefault(x, x)
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    def union(a, b):
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[ra] = rb

    try:
        a = json.load(open(path))
    except Exception:
        return find
    for g in a.get('groups', []):
        s = g.get('survivor')
        if not s:
            continue
        for f in g.get('folded', []) or []:
            union(s, f)
    return find


class OurBodies:
    """Our compiled COMDAT bodies, for byte-level fold adjudication.

    If our machine code for A and for B is identical (relocation words masked,
    and the relocation TARGET NAMES equal), then the linker would fold them in
    retail too -- so retail has ONE address, the map names it B, we spell A, and
    the disagreement is a FOLD, not a defect.

    *** A raw memcmp is SILENTLY VACUOUS here *** -- PC-relative branch
    displacements differ at different addresses, so identical functions are not
    identical bytes.  Mask the relocated words and compare the target NAMES.
    """

    def __init__(self):
        self.cache = {}
        self.index = {}   # name -> (objpath)

    def register(self, name, obj):
        self.index.setdefault(name, obj)

    def body(self, name):
        if name in self.cache:
            return self.cache[name]
        obj = self.index.get(name)
        res = None
        if obj:
            try:
                res = _extract_body(obj, name)
            except Exception:
                res = None
        self.cache[name] = res
        return res

    def same(self, a, b):
        """True = provably foldable, False = provably different, None = unknown."""
        ba, bb = self.body(a), self.body(b)
        if ba is None or bb is None:
            return None
        raw_a, rel_a = ba
        raw_b, rel_b = bb
        if len(raw_a) != len(raw_b):
            return False
        for o in range(0, len(raw_a), 4):
            if o in rel_a or o in rel_b:
                if rel_a.get(o) != rel_b.get(o):
                    return False
                continue
            if raw_a[o:o + 4] != raw_b[o:o + 4]:
                return False
        return True


def _extract_body(path, name):
    d = open(path, 'rb').read()
    mach, nsec, ts, psym, nsym, osz, ch = struct.unpack_from('<HHIIIHH', d, 0)
    off = 20 + osz
    secs = []
    for i in range(nsec):
        b = d[off:off + 40]
        nm = b[:8].rstrip(b'\0').decode('latin1', 'replace')
        vs, va, rs, pr, prel, pln, nrel, nln, chr_ = struct.unpack_from('<IIIIIIHHI', b, 8)
        secs.append(dict(name=nm, size=rs, praw=pr, prel=prel, nrel=nrel))
        off += 40
    strt = psym + nsym * 18
    flat = []
    i = 0
    while i < nsym:
        o = psym + i * 18
        raw = d[o:o + 8]
        if raw[:4] == b'\0\0\0\0':
            so = struct.unpack_from('<I', raw, 4)[0]
            e = d.index(b'\0', strt + so)
            nm = d[strt + so:e].decode('latin1', 'replace')
        else:
            nm = raw.rstrip(b'\0').decode('latin1', 'replace')
        val, sec, typ, cls, naux = struct.unpack_from('<IhHBB', d, o + 8)
        flat.append((i, nm, sec, val))
        i += 1 + naux
    byindex = {i: nm for i, nm, sec, val in flat}
    for i, nm, sec, val in flat:
        if nm == name and sec > 0 and val == 0:
            s = secs[sec - 1]
            raw = d[s['praw']:s['praw'] + s['size']]
            rel = {}
            for r in range(s['nrel']):
                ro = s['prel'] + r * 10
                rva, symi, rtyp = struct.unpack_from('<IIH', d, ro)
                rel[rva & ~3] = byindex.get(symi)
            return raw, rel
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--json', help='write full results here')
    ap.add_argument('--tier2', action='store_true', help='also report multi-call rows')
    ap.add_argument('--fanin', action='store_true', help='compute retail call-site fan-in')
    ap.add_argument('--objroot', default=os.path.join(REPO, 'build/45410914/src'))
    args = ap.parse_args()

    sm = symmap()
    # ---- sanity: the map and the retail image must both be readable, and the
    # ---- map must contain known-good landmarks.  A name-keyed sweep over a
    # ---- pre-renamer / unbuilt tree reads EVERY name as ABSENT, silently, and
    # ---- that failure AGREES WITH THE PRIOR.  Assert before trusting.
    print('== SANITY ==')
    print('map entries              : %d' % len(sm))
    print('map entries inside .text : %d' % len(mapstarts()))
    print('.pdata entries           : %d' % len(pdata()))
    known = 0x826851b0  # SongDB::RecalculateGemTimes -- W31's control
    print('landmark 0x%08x     : %s' % (known, sm.get(known)))
    assert len(sm) > 20000, 'map too small -- wrong file?'
    assert len(pdata()) > 30000, '.pdata decode looks wrong'

    # ---- index our compiled objects
    objs = sorted(glob.glob(os.path.join(args.objroot, '**', '*.obj'), recursive=True))
    print('compiled objs            : %d' % len(objs))
    defines = {}          # symbol name -> obj path (which obj DEFINES it)
    our_calls = {}        # symbol name -> [target names] in address order
    for p in objs:
        try:
            defined, calls = parse_obj(p)
        except Exception:
            continue
        for nm, (sec, val) in defined.items():
            if nm not in defines:
                defines[nm] = p
            # only trust the call list when the symbol owns its COMDAT section
            # (value 0), otherwise several functions share it and the sequence
            # is not attributable.
            if val == 0 and nm not in our_calls:
                seq = [t for (off, t) in calls.get(sec, [])]
                our_calls[nm] = seq
    print('symbols DEFINED by our objs: %d' % len(defines))
    print('  of which own a COMDAT     : %d' % len(our_calls))
    assert len(defines) > 10000, 'our objs look unbuilt -- BUILD FIRST'

    # ---- retail side, for every mapped .text address whose name we also define
    name2va = collections.defaultdict(list)
    for va, nm in sm.items():
        if TEXT_VA <= va < TEXT_VA + TEXT_SZ:
            name2va[nm].append(va)

    rows = []
    for nm, seq in our_calls.items():
        vas = name2va.get(nm)
        if not vas or len(vas) != 1:
            continue                      # unmapped, or an ambiguous multi-address name
        va = vas[0]
        size, authoritative = retail_extent(va)
        rc = retail_calls(va, size)
        rt = [sm.get(t) for (off, t, kind) in rc]
        if len(seq) != len(rc) or not rc:
            continue                      # call counts differ -> not comparable
        tier = 1 if len(rc) == 1 else 2
        if tier == 2 and not args.tier2:
            continue
        bad = []
        for k, (ours, theirs) in enumerate(zip(seq, rt)):
            if theirs is None:
                continue                  # unnamed callee: nothing to disagree with
            if ours != theirs:
                bad.append((k, ours, theirs, rc[k][1]))
        if bad:
            rows.append(dict(name=nm, va=va, size=size, tier=tier,
                             authoritative=authoritative,
                             obj=os.path.relpath(defines[nm], REPO),
                             bad=bad, ncalls=len(rc)))

    print('\n== RAW INCONSISTENT ROWS: %d ==' % len(rows))

    # ---- FOLD ADJUDICATION -------------------------------------------------
    # A disagreement has exactly three explanations:
    #   (a) ICF fold  -- our callee and the map's callee are the same machine
    #       code; the map names the survivor.  NOT a defect.
    #   (b) the map name at the target is WRONG -- the vein this lane hunts.
    #   (c) our source calls the wrong thing -- a real source bug.
    # (a) must be removed before anything else can be believed.
    find = alias_classes(os.path.join(REPO, 'scripts/symbol_aliases.json'))
    bodies = OurBodies()
    for nm, p in defines.items():
        bodies.register(nm, p)

    verdicts = collections.Counter()
    for r in rows:
        k, ours, theirs, tva = r['bad'][0]
        # Where does OUR callee spelling live in retail, if anywhere?  This is
        # the "does the thing needing a name EXIST?" check, and it is the
        # sharpest discriminator available:
        #   * ours mapped at a DIFFERENT address  -> retail really does contain
        #     two distinct functions, and the two call graphs disagree about
        #     which one is called.  That is the prize class.
        #   * ours mapped nowhere -> consistent with our spelling having been
        #     folded into the map's survivor; no defect is demonstrated.
        ours_va = name2va.get(ours)
        r['ours_va'] = ours_va[0] if ours_va and len(ours_va) == 1 else None
        if find(ours) == find(theirs):
            r['verdict'] = 'FOLD_ALIAS'
        elif _template_args_differ(ours, theirs):
            # Memory rule: TEMPLATE_ARGS_DIFFER IS what a fold looks like.
            # MSVC folds instantiations whose relocations fold transitively
            # (per-T node allocators fold too), which our one-level byte test
            # cannot see -- so this class must not be read as a defect.
            r['verdict'] = 'TEMPLATE_ARGS_DIFFER'
        else:
            s = bodies.same(ours, theirs)
            if s is True:
                r['verdict'] = 'FOLD_BYTES'
            elif s is None:
                r['verdict'] = 'UNKNOWN'
            elif r['ours_va'] is None:
                r['verdict'] = 'OURS_UNMAPPED'
            else:
                r['verdict'] = 'DUAL_MAPPED'
        verdicts[r['verdict']] += 1
    print('   fold-adjudicated: ' + '  '.join(
        '%s=%d' % (k, v) for k, v in verdicts.most_common()))

    allrows = rows
    live = [r for r in rows if r['verdict'] == 'DUAL_MAPPED']
    print('\n== LIVE CANDIDATES (DUAL_MAPPED, fold-excluded): %d ==' % len(live))
    rows = live

    # ---- group into families by (our class, callee class)
    fams = collections.defaultdict(list)
    for r in rows:
        fams[(owning_class(r['name']), owning_class(r['bad'][0][2]))].append(r)

    fanin = {}
    if args.fanin:
        fanin = compute_fanin(set(r['va'] for r in rows))

    ranked = sorted(fams.items(), key=lambda kv: -len(kv[1]))
    for (ac, cc), v in ranked:
        if len(v) < 2:
            continue
        tot = sum(fanin.get(r['va'], 0) for r in v) if fanin else None
        print('\n=== FAMILY %-26s -> %-26s  %d row(s)%s' % (
            ac, cc, len(v), '  fan-in=%d' % tot if tot is not None else ''))
        for r in sorted(v, key=lambda r: r['va']):
            k, ours, theirs, tva = r['bad'][0]
            print('   0x%08x T%d %-52s' % (r['va'], r['tier'], r['name'][:52]))
            print('        ours-> %-46s' % ours[:46])
            print('        map -> %-46s @0x%08x%s' % (
                theirs[:46], tva,
                '  fan-in=%d' % fanin[r['va']] if r['va'] in fanin else ''))

    singles = [(k, v) for k, v in ranked if len(v) == 1]
    print('\n--- single-row classes: %d (lower prior) ---' % len(singles))
    for (ac, cc), v in singles[:60]:
        r = v[0]
        k, ours, theirs, tva = r['bad'][0]
        print('   0x%08x T%d %-44s ours->%-34s map->%s' % (
            r['va'], r['tier'], r['name'][:44], ours[:34], theirs[:40]))

    if args.json:
        with open(args.json, 'w') as f:
            json.dump(dict(rows=allrows,
                           fanin={hex(k): v for k, v in fanin.items()}), f, indent=1)
        print('\nwrote %s' % args.json)


def compute_fanin(targets):
    """Retail-wide call-site count per target address.  Fan-in is what the
    cascade channel is priced from: the cascade/pairing split has measured
    98.9%/0.6% (15-22 call sites), 19.5%/80.5%, and 0%/100% (1-2 call sites).
    IT IS NOT A CONSTANT -- it tracks fan-in.  Measure it, never inherit it."""
    cnt = collections.Counter()
    for off in range(0, TEXT_SZ, 4):
        w = struct.unpack_from('>I', exe(), TEXT_OFF + off)[0]
        if (w >> 26) != 18:
            continue
        if w & 2:
            continue
        li = w & 0x03FFFFFC
        if li & 0x02000000:
            li -= 0x04000000
        t = TEXT_VA + off + li
        if t in targets:
            cnt[t] += 1
    return dict(cnt)


if __name__ == '__main__':
    main()
