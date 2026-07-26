#!/usr/bin/env python3
"""run_interleave_scan.py -- find UNIFORM-STRIDE RUNS in retail `.text` whose
`splits.txt` attribution is not constant across the run.

WHAT A "RUN" IS
---------------
The MSVC X360 linker lays a single object file's COMDATs down contiguously.
When a TU emits a *family* of same-shaped COMDATs -- the classic case being the
`OBJ_CLASSNAME` / `StaticClassName()` accessor that Milo's `Object.h` defines
inline, so every TU that references a class emits one -- the result is a block
of identically-sized bodies at a perfectly uniform stride:

    826FC068  0x58  loads "ParallelGroupSeq"
    826FC0E8  0x58  loads "MidiInstrument"
    826FC168  0x58  loads "SynthEmitter"
    826FC1E8  0x58  loads "FxSendReverb"     <- stride 0x80, 13 members
    ...

Such a run comes from ONE object file.  So if `config/45410914/splits.txt`
attributes different members of one run to different units, at most one of those
attributions can be right: a foreign unit's pin has reached into the middle of
another unit's COMDAT block.  That is a **mis-carve**, and it is invisible to
every per-function tool because each individual pin looks locally plausible.

WHAT THIS TOOL DOES *NOT* DO
----------------------------
It does not decide ownership by adjacency.  Run membership is only a LEAD
GENERATOR ("these addresses probably share an owner"); it is never the proof.
Every proposal this tool emits carries a CONTENT proof or it is not emitted:

  * the body's `.rdata` string operand is decoded out of the retail image
    (`orig/45410914/band.exe`).  For a `StaticClassName()` body the *first*
    lis/addi pair resolves into `.data` (the local-static guard + the `Symbol`
    slot); the *last* one resolves into `.rdata` and is the class-name string;
  * that string reconstructs the true mangled symbol
    `?StaticClassName@<Class>@@SA?AVSymbol@@XZ`;
  * the symbol is looked up in the DEFINER INDEX built from our own compiled
    COMDAT-per-function objs (`build/45410914/src/**/*.obj`).  The units that
    define it are the claimants.  A pin is WRONG-UNIT only when the pinned
    owner is not among them.

Measured discriminator reliability (project-wide, 2026-07): byte-identity 99.5%
> strings > floats > trust-gated callees 95.1% >> ungated callees 75.7%.  Only
the string tier is used here.

SUBCOMMANDS
-----------
  runs      structural pass only -- every uniform-stride run, ranked by how
            many members are attributed to a unit other than the run's modal
            owner.  Answers "how many interleaved runs exist tree-wide".
  prove     runs + content decode + definer join; emits WRONG-UNIT (pinned
            owner is not a claimant) and MISMAP (target_symbol_map name is not
            the content-proved one) findings.

Usage:
  run_interleave_scan.py runs  --worktree . [--min-len 4] [--json out.json]
  run_interleave_scan.py prove --worktree . [--min-len 4] [--json out.json]
                               [--definer-cache f] [--token-cache f]

MEASURED (2026-07-26)
---------------------
Structural pass: 1741 uniform-stride runs (len>=4) covering 13,436 functions;
**51-52** have a non-constant splits attribution -- the seed FxSend case was NOT
unique.  Content pass on the current tree: 12 wrong-unit + 8 unpinned + 27
mismap among the 259 content-proved 0x58 bodies.

Whole-binary A/B for the repair in this commit (same worktree, symbols.txt
frozen, report.cache cleared each leg, full ninja):
    matched_functions 36782 -> 36787 (+5)   matched_code +328 B
    fuzzy 37.025024 -> 37.028122            units 4114 -> 4106
    SET DIFF by (unit,name): 15 lost / 20 gained
    SET DIFF by name only:    4 lost /  9 gained
The 4 lost names are exactly the four fake phantoms being retired
(BandDirector, CharWeightable, Character, LocalePanel).

WHY THIS VEIN READS AS NEARLY SCORE-NEUTRAL
-------------------------------------------
A StaticClassName() body is 22 instructions that differ between classes ONLY in
two relocated address operands, and objdiff's normalized diff ignores
relocation targets.  A mis-attributed body therefore scores 100% against
whatever same-shaped COMDAT our obj happens to define under the wrong name.
Repairing one is +1/-1; the net comes only from members that were previously
UNPINNED (owned by no unit) or blocked by a duplicate name inside one unit.
The product is honesty, not score.

RELATION TO THE MAP-DRIVEN AUDIT (localstatic_symbol_audit.py, lanePHANTOM)
--------------------------------------------------------------------------
The two tools are complementary, not redundant:
  * that audit is keyed on target_symbol_map.json, so a VA with NO map entry is
    structurally invisible to it.  6 of the 12 residual wrong-unit VAs here are
    unmapped.
  * it stops at "AMBIGUOUS string" (FxSendX vs FxSendX360, Rnd*/Dx*/Ng*
    triplets) and reports them unrepairable.  Two further oracles break that
    tie: MAP OCCUPANCY (the *360 twins are already mapped elsewhere, forcing
    the base name here) and BLOCK MEMBERSHIP (the VA sits inside a uniform-
    stride block whose other members are already the base family).
Conversely that audit covers `?Type@...Msg@@` DECLARE_MESSAGE bodies, which
this tool's content tier does not reconstruct.

ROOT CAUSE -- HYPOTHESIS TESTED AND REJECTED
--------------------------------------------
The earlier guess was "uniform one-block 0x78 micro-pins are the signature of a
size/count-order automapper, so the same defect shape exists in other
families".  Measured, that is wrong on both halves:
  * the shape is not distinctive.  Range-length histogram peaks are flat
    (0x60:144, 0x78:131, 0x70:120, 0x80:111) and **45.8% of all 5,477 .text
    ranges cover exactly one function** -- single-function scatter pins are the
    NORMAL product of the homing/scatter lanes, not an anomaly.
  * the defect does not generalise.  Among solo/pair pins whose body carries
    >=2 distinctive .rdata strings (a family where content CAN discriminate),
    the wrong-unit rate is 0/11 decidable, versus 33/259 (12.7%) in the
    identical-body family.
So attribution error is not a property of the pin SHAPE; it is a property of
CONTENT-AMBIGUOUS BODIES, where every similarity-based assigner is guessing and
the guess self-confirms at fake-100%.  Good news: the residue is bounded by the
~453-member identical-body population, not unbounded across the binary.

KNOWN LIMITATION
----------------
The 0x58 shape is also emitted by non-StaticClassName methods that intern a
Symbol (e.g. `?Init@StarDisplay@@SAXXZ` at 0x8231d670 loads "StarDisplay").
The IDENT fallback tier will mislabel those.  Tokens with several VAs and no
free class slot (MiniLeaderboardDisplay x3, InlineHelp, ScrollbarDisplay,
ScoreDisplay) are in that band -- do not repair them from this tool alone.
"""
import argparse
import bisect
import glob
import json
import os
import re
import struct
import sys
from collections import Counter, defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from splits_move import coff_defined_symbols  # noqa: E402

SYM_RE = re.compile(
    r'^(\S+)\s*=\s*\.text:0x([0-9A-Fa-f]+);.*type:function.*size:0x([0-9A-Fa-f]+)')
RANGE_RE = re.compile(
    r'^\s*\.(\w+)\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)')

# XDK vendor + Quazal middleware -- hard-skipped by project directive.
SKIP_LO, SKIP_HI = 0x82800000, 0x82D00000


# --------------------------------------------------------------------------
# retail image
# --------------------------------------------------------------------------
class Image:
    """Minimal PE section reader over orig/45410914/band.exe."""

    def __init__(self, path):
        d = open(path, 'rb').read()
        self.d = d
        e = struct.unpack_from("<I", d, 0x3C)[0]
        coff = e + 4
        num = struct.unpack_from("<H", d, coff + 2)[0]
        optsz = struct.unpack_from("<H", d, coff + 16)[0]
        opt = coff + 20
        imgbase = struct.unpack_from("<I", d, opt + 28)[0]
        st = opt + optsz
        self.secs = []
        for i in range(num):
            o = st + i * 40
            nm = d[o:o + 8].rstrip(b"\0").decode("latin1")
            vs = struct.unpack_from("<I", d, o + 8)[0]
            va = imgbase + struct.unpack_from("<I", d, o + 12)[0]
            rs = struct.unpack_from("<I", d, o + 16)[0]
            praw = struct.unpack_from("<I", d, o + 20)[0]
            self.secs.append((va, max(vs, rs), praw, rs, nm))
            if nm == '.text':
                self.text = (va, praw, rs)
        self.secs.sort()

    def secname(self, va):
        for sva, vs, praw, rs, nm in self.secs:
            if sva <= va < sva + vs:
                return nm
        return None

    def read(self, va, n):
        for sva, vs, praw, rs, nm in self.secs:
            if sva <= va < sva + vs:
                o = praw + (va - sva)
                if o + n <= praw + rs:
                    return self.d[o:o + n]
                return None
        return None

    def text_bytes(self, va, size):
        tva, praw, rs = self.text
        o = va - tva
        if o < 0 or o + size > rs:
            return None
        return self.d[praw + o: praw + o + size]

    def cstr(self, va, n=192):
        raw = self.read(va, n)
        if raw is None:
            return None
        raw = raw.split(b'\0')[0]
        if not raw or len(raw) > 160:
            return None
        if not all(32 <= b < 127 for b in raw):
            return None
        return raw.decode('ascii')


def _sext(v, bits):
    m = 1 << (bits - 1)
    return (v ^ m) - m


def addr_operands(img, va, size):
    """[(byte_off, resolved_va)] for every lis+{addi,ori,lwz,stw} pair.

    Deliberately simple: a `lis rD,hi` records hi for rD, and the next
    D-form instruction using rD as rA resolves.  Enough for the constant-address
    materialisation MSVC emits for statics/strings, which is all we decode.
    """
    d = img.text_bytes(va, size)
    if d is None:
        return []
    hi = {}
    out = []
    for i in range(0, size - 3, 4):
        w = struct.unpack_from('>I', d, i)[0]
        op = w >> 26
        rD = (w >> 21) & 31
        rA = (w >> 16) & 31
        simm = _sext(w & 0xFFFF, 16)
        if op == 15 and rA == 0:                      # lis
            hi[rD] = simm << 16
        elif op == 14 and rA in hi:                   # addi
            out.append((i, (hi[rA] + simm) & 0xFFFFFFFF))
        elif op == 24 and rA in hi:                   # ori
            out.append((i, (hi[rA] | (w & 0xFFFF)) & 0xFFFFFFFF))
        elif op in (32, 36) and rA in hi:             # lwz / stw
            out.append((i, (hi[rA] + simm) & 0xFFFFFFFF))
    return out


def body_strings(img, va, size):
    """Ordered, de-duplicated list of `.rdata` C-strings the body materialises."""
    seen = []
    for _off, a in addr_operands(img, va, size):
        if img.secname(a) != '.rdata':
            continue
        s = img.cstr(a)
        if s and s not in seen:
            seen.append(s)
    return seen


# --------------------------------------------------------------------------
# project inputs
# --------------------------------------------------------------------------
def parse_functions(path):
    out = []
    for line in open(path):
        m = SYM_RE.match(line)
        if m:
            out.append((int(m.group(2), 16), int(m.group(3), 16)))
    out.sort()
    return out


def parse_text_owners(path):
    """[(start, end, unit)] sorted, for `.text` only."""
    rows = []
    cur = None
    for line in open(path):
        s = line.rstrip('\n')
        if s.endswith(':') and not s.startswith((' ', '\t')):
            cur = s[:-1]
            if cur == 'Sections':
                cur = None
            continue
        m = RANGE_RE.match(line)
        if m and cur and m.group(1) == 'text':
            rows.append((int(m.group(2), 16), int(m.group(3), 16), cur))
    rows.sort()
    return rows


class Owners:
    def __init__(self, rows):
        self.rows = rows
        self.starts = [r[0] for r in rows]

    def of(self, va):
        i = bisect.bisect_right(self.starts, va) - 1
        if i >= 0 and self.rows[i][0] <= va < self.rows[i][1]:
            return self.rows[i][2]
        return None

    def range_of(self, va):
        i = bisect.bisect_right(self.starts, va) - 1
        if i >= 0 and self.rows[i][0] <= va < self.rows[i][1]:
            return self.rows[i]
        return None


def load_unit_map(worktree):
    """splits.txt unit name -> compiled base obj path (the definer-index key).

    `objdiff.json` is the authoritative pairing: `target_path` is derived
    mechanically from the splits unit name (`build/45410914/obj/<unit>.obj`)
    and `base_path` is the compiled obj.  splits.txt mixes bare basenames
    (`Char.cpp`) with full paths (`system/synth/Synth.cpp`), so guessing the
    source path from the unit name is not safe -- go through objdiff.json.
    """
    cfg = json.load(open(os.path.join(worktree, 'objdiff.json')))
    out = {}
    for u in cfg['units']:
        tp = u.get('target_path')
        bp = u.get('base_path')
        if not tp or not bp:
            continue
        unit = tp.split('build/45410914/obj/', 1)[-1]
        if unit.endswith('.obj'):
            unit = unit[:-4] + '.cpp'
        out[unit] = bp
    return out


# --------------------------------------------------------------------------
# class -> OBJ_CLASSNAME token, read out of OUR OWN compiled objs
#
# The retail body materialises a TOKEN string, which is NOT always the class
# name: `RndPartLauncher`'s token is "PartLauncher", `FxSendReverb360`'s is
# "FxSendReverb".  Reconstructing `?StaticClassName@<string>@@` from the retail
# string is therefore only a heuristic.  The exact table is recoverable from our
# side: each `?StaticClassName@C@@` COMDAT relocates against the `??_C@` string
# COMDAT whose MANGLED NAME encodes the literal.  Decoding that gives
# class -> token with no guessing.
# --------------------------------------------------------------------------
SCN_RE = re.compile(r'^\?StaticClassName@(.+)@@SA\?AVSymbol@@XZ$')
STRLIT_RE = re.compile(r'^\?\?_C@_[0-9A-Z]+@[0-9A-Z]+@(.*)@$')
_MANGLE_ESC = {'0': '?', '1': '/', '2': '\\', '3': ':', '4': '.', '5': ' ',
               '6': '\n', '7': '\t', '8': "'", '9': '-'}


def decode_mangled_literal(s):
    """MSVC `??_C@` name tail -> the string literal it encodes."""
    out = []
    i = 0
    while i < len(s):
        c = s[i]
        if c == '?' and i + 1 < len(s):
            n = s[i + 1]
            if n == '$' and i + 3 < len(s):
                hi = ord(s[i + 2]) - ord('A')
                lo = ord(s[i + 3]) - ord('A')
                if 0 <= hi < 16 and 0 <= lo < 16:
                    out.append(chr((hi << 4) | lo))
                    i += 4
                    continue
            if n in _MANGLE_ESC:
                out.append(_MANGLE_ESC[n])
                i += 2
                continue
            out.append(n)
            i += 2
            continue
        out.append(c)
        i += 1
    return ''.join(out).split('\0')[0]


def _coff_sections_symbols(data):
    nsec = struct.unpack_from('<H', data, 2)[0]
    symoff = struct.unpack_from('<I', data, 8)[0]
    nsym = struct.unpack_from('<I', data, 12)[0]
    if not symoff or not nsym:
        return [], []
    strtab = symoff + nsym * 18
    secs = []
    for i in range(nsec):
        o = 20 + i * 40
        prel = struct.unpack_from('<I', data, o + 24)[0]
        nrel = struct.unpack_from('<H', data, o + 32)[0]
        secs.append((prel, nrel))
    syms = []
    i = 0
    while i < nsym:
        o = symoff + i * 18
        nb = data[o:o + 8]
        if nb[:4] == b'\0\0\0\0':
            a = strtab + struct.unpack_from('<I', nb, 4)[0]
            e = data.find(b'\0', a)
            name = data[a:e if e >= 0 else len(data)].decode('latin1')
        else:
            name = nb.split(b'\0')[0].decode('latin1')
        sec = struct.unpack_from('<h', data, o + 12)[0]
        syms.append((name, sec))
        naux = data[o + 17]
        syms.extend([None] * naux)
        i += 1 + naux
    return secs, syms


def build_token_index(worktree, cache=None):
    """{class: token} plus {token: [class,...]} from our compiled objs."""
    if cache and os.path.exists(cache):
        d = json.load(open(cache))
        return d['class2tok'], d['tok2class']
    class2tok = {}
    root = os.path.join(worktree, 'build/45410914/src')
    for p in glob.glob(os.path.join(root, '**/*.obj'), recursive=True):
        try:
            data = open(p, 'rb').read()
            secs, syms = _coff_sections_symbols(data)
        except Exception:
            continue
        for s in syms:
            if s is None:
                continue
            name, sec = s
            m = SCN_RE.match(name)
            if not m or sec <= 0 or sec > len(secs):
                continue
            prel, nrel = secs[sec - 1]
            lits = []
            for r in range(nrel):
                o = prel + r * 10
                if o + 10 > len(data):
                    break
                si = struct.unpack_from('<I', data, o + 4)[0]
                t = syms[si] if si < len(syms) else None
                if not t:
                    continue
                mm = STRLIT_RE.match(t[0])
                if mm:
                    lits.append(decode_mangled_literal(mm.group(1)))
            if lits:
                class2tok[m.group(1)] = lits[-1]
    tok2class = defaultdict(list)
    for c, t in class2tok.items():
        tok2class[t].append(c)
    tok2class = {k: sorted(v) for k, v in tok2class.items()}
    if cache:
        json.dump({'class2tok': class2tok, 'tok2class': tok2class},
                  open(cache, 'w'))
    return class2tok, tok2class


def build_definer_index(worktree, cache=None):
    """{symbol: {base_obj_path}} over every compiled obj."""
    if cache and os.path.exists(cache):
        return {k: set(v) for k, v in json.load(open(cache)).items()}
    idx = defaultdict(set)
    root = os.path.join(worktree, 'build/45410914/src')
    for p in glob.glob(os.path.join(root, '**/*.obj'), recursive=True):
        key = os.path.relpath(p, worktree)
        try:
            data = open(p, 'rb').read()
        except OSError:
            continue
        for s in coff_defined_symbols(data):
            idx[s].add(key)
    if cache:
        json.dump({k: sorted(v) for k, v in idx.items()}, open(cache, 'w'))
    return idx


# --------------------------------------------------------------------------
# run detection
# --------------------------------------------------------------------------
def find_runs(funcs, min_len, max_stride_factor=3.0):
    """Maximal same-size constant-stride runs.

    For every size class, walk the sorted VAs of that size and cut a new run
    whenever the delta changes.  A stride wildly larger than the body size means
    the members are not really neighbours (unrelated code between them), so
    strides above `max_stride_factor * size + 0x40` are rejected.
    """
    by_size = defaultdict(list)
    for va, sz in funcs:
        if sz == 0:
            continue
        by_size[sz].append(va)

    runs = []
    for sz, vas in by_size.items():
        vas.sort()
        lim = max_stride_factor * sz + 0x40
        i = 0
        while i < len(vas) - 1:
            d = vas[i + 1] - vas[i]
            if d > lim:
                i += 1
                continue
            j = i + 1
            while j < len(vas) - 1 and vas[j + 1] - vas[j] == d:
                j += 1
            n = j - i + 1
            if n >= min_len:
                runs.append({'size': sz, 'stride': d,
                             'members': vas[i:j + 1]})
            i = j
    runs.sort(key=lambda r: -len(r['members']))
    return runs


def annotate(run, owners):
    ms = []
    for va in run['members']:
        ms.append({'va': va, 'owner': owners.of(va)})
    pinned = [m['owner'] for m in ms if m['owner']]
    modal = Counter(pinned).most_common(1)[0][0] if pinned else None
    foreign = [m for m in ms if m['owner'] and m['owner'] != modal]
    run['members_ann'] = ms
    run['modal_owner'] = modal
    run['n_pinned'] = len(pinned)
    run['n_distinct_owners'] = len(set(pinned))
    run['n_foreign'] = len(foreign)
    run['n_unpinned'] = len(ms) - len(pinned)
    return run


# --------------------------------------------------------------------------
# content proof
# --------------------------------------------------------------------------
STATIC_CLASS_NAME = '?StaticClassName@%s@@SA?AVSymbol@@XZ'


def prove_member(img, va, size, tmap, definers, tok2class=None):
    """CONTENT evidence for one member.  Never uses adjacency.

    Two tiers, strongest first:

      TOKEN  the retail body's string is looked up in the class->token table
             derived from our own objs' relocations.  Exact; handles the cases
             where the OBJ_CLASSNAME token differs from the class name
             (`RndPartLauncher` -> "PartLauncher").  A token shared by several
             classes (`FxSendReverb` / `FxSendReverb360`) yields several
             candidate names -- the claimant set is their union and the map name
             is accepted if it is any of them.
      IDENT  fallback: assume token == class name and check that
             `?StaticClassName@<string>@@SA?AVSymbol@@XZ` is a symbol we define.
    """
    out = {'va': va, 'size': size,
           'map_name': tmap.get('0x%08x' % va),
           'strings': body_strings(img, va, size)}
    names, tier = [], None
    if len(out['strings']) == 1:
        tok = out['strings'][0]
        if tok2class and tok in tok2class:
            names = [STATIC_CLASS_NAME % c for c in tok2class[tok]]
            tier = 'TOKEN'
        else:
            cand = STATIC_CLASS_NAME % tok
            if cand in definers:
                names = [cand]
                tier = 'IDENT'
    names = [n for n in names if n in definers]
    out['tier'] = tier if names else None
    out['content_names'] = names
    out['content_name'] = names[0] if len(names) == 1 else (
        names[0] if names else None)
    claim = set()
    for n in names:
        claim |= definers.get(n, set())
    out['claimants'] = sorted(claim)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('cmd', choices=['runs', 'prove'])
    ap.add_argument('--worktree', default='.')
    ap.add_argument('--min-len', type=int, default=4)
    ap.add_argument('--max-stride-factor', type=float, default=3.0)
    ap.add_argument('--json')
    ap.add_argument('--definer-cache')
    ap.add_argument('--token-cache')
    ap.add_argument('--include-vendor', action='store_true',
                    help='do not skip the 0x82800000-0x82D00000 XDK/Quazal band')
    ap.add_argument('--top', type=int, default=40)
    a = ap.parse_args()

    wt = a.worktree
    funcs = parse_functions(os.path.join(wt, 'config/45410914/symbols.txt'))
    if not a.include_vendor:
        funcs = [(v, s) for v, s in funcs if not (SKIP_LO <= v < SKIP_HI)]
    owners = Owners(parse_text_owners(os.path.join(wt,
                                                   'config/45410914/splits.txt')))

    runs = [annotate(r, owners)
            for r in find_runs(funcs, a.min_len, a.max_stride_factor)]
    interleaved = [r for r in runs if r['n_distinct_owners'] >= 2]
    interleaved.sort(key=lambda r: (-r['n_foreign'], -len(r['members'])))

    print('runs (len>=%d): %d   members: %d' %
          (a.min_len, len(runs), sum(len(r['members']) for r in runs)))
    print('INTERLEAVED (>=2 distinct pinned owners): %d' % len(interleaved))
    print('  with >=1 foreign member: %d' %
          sum(1 for r in interleaved if r['n_foreign']))
    print()

    if a.cmd == 'runs':
        print('%-10s %-6s %-5s %-4s %-4s %s' %
              ('start', 'stride', 'size', 'n', 'frgn', 'owners'))
        for r in interleaved[:a.top]:
            ow = Counter(m['owner'] for m in r['members_ann'] if m['owner'])
            print('%08X   0x%-4X 0x%-3X %-4d %-4d %s' %
                  (r['members'][0], r['stride'], r['size'], len(r['members']),
                   r['n_foreign'],
                   ', '.join('%s x%d' % (k, v) for k, v in ow.most_common())))
        if a.json:
            json.dump(interleaved, open(a.json, 'w'), indent=1)
            print('\nwrote %s' % a.json)
        return

    # ---------------- prove ----------------
    img = Image(os.path.join(wt, 'orig/45410914/band.exe'))
    tmap = json.load(open(os.path.join(wt, 'scripts/target_symbol_map.json')))
    denylist = set(x.lower() for x in tmap.get('_denylist', []))
    icf = set(x.lower() for x in tmap.get('_icf_arbitrary', []))
    bij = set(x.lower() for x in tmap.get('_bijection_arbitrary', []))
    definers = build_definer_index(wt, a.definer_cache)
    _c2t, tok2class = build_token_index(wt, a.token_cache)
    umap = load_unit_map(wt)

    findings = []
    for r in interleaved:
        rec = {'start': r['members'][0], 'stride': r['stride'],
               'size': r['size'], 'n': len(r['members']),
               'modal_owner': r['modal_owner'], 'members': []}
        n_proved = 0
        for m in r['members_ann']:
            p = prove_member(img, m['va'], r['size'], tmap, definers,
                             tok2class)
            p['owner'] = m['owner']
            key = '0x%08x' % m['va']
            p['risky'] = sorted(
                t for t, s in (('denylist', denylist), ('icf', icf),
                               ('bijection', bij)) if key in s)
            if p['content_name']:
                n_proved += 1
                own_obj = umap.get(m['owner']) if m['owner'] else None
                p['owner_obj'] = own_obj
                p['owner_is_claimant'] = (own_obj is not None
                                          and own_obj in p['claimants'])
                p['mismap'] = (p['map_name'] not in p['content_names'])
            rec['members'].append(p)
        rec['n_proved'] = n_proved
        rec['n_wrong_unit'] = sum(
            1 for p in rec['members']
            if p.get('content_name') and p['owner'] and not p['owner_is_claimant'])
        rec['n_mismap'] = sum(1 for p in rec['members'] if p.get('mismap'))
        if n_proved:
            findings.append(rec)

    findings.sort(key=lambda r: (-r['n_wrong_unit'], -r['n_mismap'], -r['n']))
    print('runs with >=1 CONTENT-PROVED member: %d' % len(findings))
    print('  proved WRONG-UNIT members: %d' %
          sum(r['n_wrong_unit'] for r in findings))
    print('  proved MISMAP members:     %d' %
          sum(r['n_mismap'] for r in findings))
    print()
    for r in findings[:a.top]:
        print('=== run %08X stride 0x%X size 0x%X n=%d modal=%s '
              '(proved %d, wrong-unit %d, mismap %d)' %
              (r['start'], r['stride'], r['size'], r['n'], r['modal_owner'],
               r['n_proved'], r['n_wrong_unit'], r['n_mismap']))
        for p in r['members']:
            flags = []
            if p.get('content_name') and p['owner'] and not p['owner_is_claimant']:
                flags.append('WRONG-UNIT')
            if p.get('mismap'):
                flags.append('MISMAP')
            if p['risky']:
                flags.append('RISKY:' + ','.join(p['risky']))
            print('  %08X %-34s str=%-22s owner=%-28s %s' %
                  (p['va'], (p['map_name'] or '-')[:34],
                   (p['strings'][0] if p['strings'] else '-')[:22],
                   p['owner'] or 'UNPINNED', ' '.join(flags)))
            if flags and p['claimants']:
                print('      claimants: %s' % ', '.join(p['claimants']))
    if a.json:
        json.dump(findings, open(a.json, 'w'), indent=1)
        print('\nwrote %s' % a.json)


if __name__ == '__main__':
    main()
