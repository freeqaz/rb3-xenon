#!/usr/bin/env python3
"""Disambiguate the homing scan's MULTI / UNIQUE-ICF residue by REFERENCED CONTENT.

Background
----------
`scripts/harvest/homing_scan.py` finds a retail home for one of our compiled
functions by reloc-masked byte identity against band.exe's whole `.pdata`
inventory.  It only ever proposes `UNIQUE` (exactly one byte-identical retail
VA).  Everything else -- 26,961 `MULTI` occurrences and 255 `UNIQUE-ICF` VAs as
of round 4 -- was deliberately left unpinned, because *masking the relocation
sites destroys the only thing that tells the twins apart*, and objdiff's
normalized diff masks them too, so a mispair would still read 100%.

This tool restores that discriminator.  Every masked slot is a relocation in
our obj (we know the exact symbol it points at) and a fully-resolved
instruction in retail (we can decode the address it materialises).  Comparing
them **positionally** -- offset by offset, not as unordered sets -- is exactly
the evidence the byte compare threw away.

Evidence classes, in decreasing strength:

  str:   our reloc points at a `??_C@` string COMDAT; retail's resolved VA
         holds byte-identically the same C string.  Needs no symbol map.
  vfstr: same, but the string sits at VA+0x20 (this binary's `.rdata` puts a
         vftable's class-name string at that delta).
  f32/f64: our reloc points at `__real@<hex>` / `__xmm@<hex>` (the mangled name
         literally encodes the constant's bytes); retail's VA holds them.
         Needs no symbol map.
  sym:   our reloc points at a function/global already present in
         `scripts/target_symbol_map.json`; retail's decoded `bl` target or
         materialised address is that same VA.
  op:    the *instruction form* at a masked offset.  Masking zeroes whole
         4-byte instructions, opcode included, so two retail functions can be
         "byte-identical" while executing different opcodes at a masked slot.
         Free, universal, exclusion-only.

Acceptance rule (deliberately strict -- a mispair is invisible in the score and
permanently corrupts the map, so the cost of a wrong accept is unbounded):

    accept candidate c  iff
      (a) c has >= 1 AGREE and 0 CONFLICT, and
      (b) for every other candidate c', there is an offset where c AGREES and
          c' resolves to *different, decodable* content.

Condition (b) is the honesty clause: rivals must be **positively excluded at a
slot we positively confirmed**, never merely "less good" or "undeterminable".
Anything that fails is reported UNRESOLVED and left unmapped.

Usage
-----
    multi_content_disambiguate.py --results merged_scan.json --worktree WT \
        [--objs objs.txt] [--out proposals.json] [--report report.txt]
        [--classes MULTI,UNIQUE-ICF]

`--results` is the merged output of `homing_scan.py` (dict tu -> [records]).
Emits a JSON in *homing_scan result format* with the resolved records
re-classified to `UNIQUE`, so `homing_gen4.py` / `homing_apply4.py` consume it
unchanged.
"""
import argparse
import json
import os
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path

IMAGE_SCN_CNT_CODE = 0x20
REL_REFHI, REL_REFLO, REL_PAIR = 0x10, 0x11, 0x12
VFTABLE_NAME_DELTA = 0x20
STR_MIN, STR_MAX = 2, 96


# --------------------------------------------------------------- COFF (ours)
def parse_obj(path):
    data = Path(path).read_bytes()
    nsec = struct.unpack_from("<H", data, 2)[0]
    symoff = struct.unpack_from("<I", data, 8)[0]
    nsym = struct.unpack_from("<I", data, 12)[0]
    optsz = struct.unpack_from("<H", data, 16)[0]
    str_start = symoff + nsym * 18
    secs = {}
    base = 20 + optsz
    for i in range(nsec):
        o = base + i * 40
        raw_size, praw, prel = struct.unpack_from("<III", data, o + 16)
        nrel = struct.unpack_from("<H", data, o + 32)[0]
        chars = struct.unpack_from("<I", data, o + 36)[0]
        rel = [struct.unpack_from("<IIH", data, prel + j * 10) for j in range(nrel)]
        secs[i + 1] = dict(raw_size=raw_size, praw=praw, rel=rel, chars=chars)
    syms = []
    i = 0
    while i < nsym:
        o = symoff + i * 18
        nm = data[o:o + 8]
        if nm[:4] == b"\0\0\0\0":
            so = struct.unpack_from("<I", nm, 4)[0]
            e = data.index(b"\0", str_start + so)
            name = data[str_start + so:e].decode("latin1")
        else:
            name = nm.split(b"\0")[0].decode("latin1")
        val, secn, typ, sc, naux = struct.unpack_from("<IhHBB", data, o + 8)
        syms.append(dict(idx=i, name=name, val=val, secn=secn, typ=typ, sc=sc))
        i += 1 + naux
    return data, secs, syms


def sym_content_token(data, secs, sym):
    """Content token for the symbol our relocation points at, or None."""
    name = sym["name"]
    m = re.match(r'__(?:real|xmm)@([0-9a-fA-F]+)$', name)
    if m:
        h = m.group(1)
        if len(h) in (8, 16, 32):
            return ('f%d' % (len(h) * 4), h.lower())
    if name.startswith('??_C@'):
        secn = sym["secn"]
        if secn > 0 and secn in secs:
            s = secs[secn]
            raw = data[s["praw"] + sym["val"]: s["praw"] + s["raw_size"]]
            # ??_C@_0.. = narrow, ??_C@_1.. = wide
            if name.startswith('??_C@_1'):
                txt = raw.split(b'\0\0')[0]
                try:
                    return ('wstr', txt.decode('utf-16-be' if txt[:1] == b'\0' else 'utf-16-le'))
                except Exception:
                    return None
            txt = raw.split(b'\0')[0]
            try:
                return ('str', txt.decode('latin1'))
            except Exception:
                return None
        return None
    return ('sym', name)


def func_table(path):
    """-> {name: dict(sec, start, size, body(masked), offs, relocs)}"""
    data, secs, syms = parse_obj(path)
    byidx = {s["idx"]: s for s in syms}
    by_sec = defaultdict(list)
    for sy in syms:
        if sy["secn"] > 0 and sy["secn"] in secs and sy["sc"] in (2, 3) and sy["typ"] == 0x20:
            by_sec[sy["secn"]].append(sy)
    out = {}
    for secn, members in by_sec.items():
        s = secs[secn]
        if not (s["chars"] & IMAGE_SCN_CNT_CODE) or s["praw"] == 0:
            continue
        members = sorted(members, key=lambda x: x["val"])
        for k, sy in enumerate(members):
            start = sy["val"]
            end = members[k + 1]["val"] if k + 1 < len(members) else s["raw_size"]
            if end <= start or sy["name"] in out:
                continue
            body = bytearray(data[s["praw"] + start: s["praw"] + end])
            offs, refs = [], {}
            for rva, si, ty in s["rel"]:
                if not (start <= rva < end):
                    continue
                off = rva - start
                offs.append(off)
                for b in range(4):
                    if off + b < len(body):
                        body[off + b] = 0
                if ty == REL_PAIR:
                    continue
                tsym = byidx.get(si)
                if tsym is None:
                    continue
                refs.setdefault(off, (tsym["name"], ty,
                                      sym_content_token(data, secs, tsym)))
            words = {}
            for off in set(offs):
                if off + 4 <= end - start:
                    words[off] = struct.unpack_from('>I', data,
                                                    s["praw"] + start + off)[0]
            out[sy["name"]] = dict(sec=secn, start=start, size=end - start,
                                   body=bytes(body), offs=sorted(set(offs)),
                                   refs=refs, words=words)
    return out


# ------------------------------------------------------------- band.exe (PE)
class Band:
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

    def off(self, va, n=1):
        for sva, vs, praw, rs, nm in self.secs:
            if sva <= va < sva + vs:
                o = praw + (va - sva)
                if o + n <= praw + rs:
                    return o
                return None
        return None

    def read(self, va, n):
        o = self.off(va, n)
        return None if o is None else self.d[o:o + n]

    def text_bytes(self, va, size):
        tva, praw, rs = self.text
        o = va - tva
        if o < 0 or o + size > rs:
            return None
        return self.d[praw + o: praw + o + size]

    def in_text(self, va):
        tva, praw, rs = self.text
        return tva <= va < tva + rs

    def cstr(self, va, n=128):
        raw = self.read(va, n)
        if raw is None:
            return None
        raw = raw.split(b'\0')[0]
        if not (STR_MIN <= len(raw) <= STR_MAX):
            return None
        if not all(32 <= b < 127 for b in raw):
            return None
        return raw.decode('ascii')

    def cstr_exact(self, va, n=256):
        """AUDIT reader -- symmetric with sym_content_token()'s ??_C@ decode.

        `cstr()` above is the DISCOVERY reader: it deliberately rejects short,
        long and non-printable strings so that ranking candidate homes is not
        polluted by coincidental garbage.  Using it to AUDIT the map is a bug,
        because our side of the comparison (`sym_content_token`) decodes a
        `??_C@` COMDAT with no length or charset filter at all.  So a function
        referencing "\\n", "", " " or a printf format ending in \\n produces a
        token on our side that the retail side is structurally incapable of
        producing -> guaranteed false CONFLICT.

        Measured 2026-07-26 (lane laneZ): 217 of the 304 names that
        --trust-audit called CONTRADICTED are this artifact.  The family is
        obvious in hindsight -- ?Print@CharClip@@, ?Print@DataArray@@,
        ?Print@PropKeys@@, ?ColatedPrint@MemTracker@@ ... every debug-printer
        in the binary.  Do NOT use this reader on the discovery path.
        """
        raw = self.read(va, n)
        if raw is None:
            return None
        raw = raw.split(b'\0')[0]
        if len(raw) > 200:
            return None
        if not all(b in (9, 10, 13) or 32 <= b < 127 for b in raw):
            return None
        return raw.decode('latin1')

    def wstr(self, va, n=256):
        raw = self.read(va, n)
        if raw is None:
            return None
        i = 0
        out = []
        while i + 2 <= len(raw):
            c = struct.unpack_from('>H', raw, i)[0]
            if c == 0:
                break
            if not (32 <= c < 127):
                return None
            out.append(chr(c))
            i += 2
        s = ''.join(out)
        return s if STR_MIN <= len(s) <= STR_MAX else None


# ------------------------------------------------------------- PPC decoding
def sext(v, bits):
    m = 1 << (bits - 1)
    return (v ^ m) - m


D_FORM_OPS = set(range(32, 56)) | {14, 24, 25, 26, 27, 28, 29}


def decode_slots(band, va, size, offs, base_words):
    """For each masked offset, decode what retail materialises there.

    Returns {off: dict(word, op, kind, value)} where kind is
      'call' -> value = absolute branch target VA
      'addr' -> value = absolute address formed by an addis/lo pair
      'imm'  -> value = the raw 16-bit immediate (lo half of a pair, or
                a standalone D-form displacement)
      None   -> undecodable
    """
    raw = band.text_bytes(va, size)
    if raw is None:
        return None
    words = {}
    for off in offs:
        if off + 4 <= size:
            words[off] = struct.unpack_from('>I', raw, off)[0]
    out = {}
    hi_pending = {}       # rD -> (off, hi16)
    for off in sorted(words):
        w = words[off]
        op = w >> 26
        rec = dict(word=w, op=op, kind=None, value=None)
        if op == 18:                                    # b / bl
            li = sext(w & 0x03FFFFFC, 26)
            aa = (w >> 1) & 1
            rec['kind'] = 'call'
            rec['value'] = (li & 0xFFFFFFFF) if aa else ((va + off + li) & 0xFFFFFFFF)
        elif op == 15 and ((w >> 16) & 31) == 0:        # lis rD, hi
            rD = (w >> 21) & 31
            hi_pending[rD] = (off, w & 0xFFFF)
            rec['kind'] = 'hi'
            rec['value'] = w & 0xFFFF
        elif op in D_FORM_OPS:
            rA = (w >> 16) & 31 if op in (14, 24, 25, 26, 27, 28, 29) else (w >> 16) & 31
            lo = w & 0xFFFF
            rec['kind'] = 'imm'
            rec['value'] = lo
            if rA in hi_pending:
                hoff, hi = hi_pending[rA]
                rec['kind'] = 'addr'
                rec['value'] = ((hi << 16) + sext(lo, 16)) & 0xFFFFFFFF
                rec['hi_off'] = hoff
        out[off] = rec
    # propagate the resolved address back onto the hi slot
    for off, rec in list(out.items()):
        if rec['kind'] == 'addr':
            h = rec.get('hi_off')
            if h is not None and h in out:
                out[h] = dict(out[h], kind='addr', value=rec['value'])
    return out


def classify_va(band, va, tmap, exact=False):
    """Content tokens a retail address yields.

    `exact=True` selects the AUDIT reader (see Band.cstr_exact): symmetric with
    our own ??_C@ decode, and float tokens are suppressed at unaligned
    addresses.  Leave it False on the discovery path -- a permissive reader
    there manufactures ties between candidate homes.
    """
    toks = set()
    if band.in_text(va):
        n = tmap.get(va)
        if n:
            toks.add(('sym', n))
        toks.add(('code', va))
        return toks
    rd = band.cstr_exact if exact else band.cstr
    s = rd(va)
    if s is not None:
        toks.add(('str', s))
    ws = band.wstr(va)
    if ws is not None:
        toks.add(('wstr', ws))
    v = rd(va + VFTABLE_NAME_DELTA)
    if v is not None:
        toks.add(('vfstr', v))
    # A float constant pool entry is always at least 4-byte aligned.  On the
    # audit path an unaligned VA therefore cannot be the float our relocation
    # points at, and emitting a token for it only fabricates a CONFLICT --
    # decode_slots' lis/D-form pairing does mis-resolve some pool addresses
    # (observed: 0x8202dc17, 0x82000c55, both unaligned).
    if not (exact and (va & 3)):
        for w, k in ((4, 'f32'), (8, 'f64'), (16, 'f128')):
            raw = band.read(va, w)
            if raw is not None:
                toks.add((k, raw.hex()))
    n = tmap.get(va)
    if n:
        toks.add(('sym', n))
    toks.add(('gva', va))
    return toks


# ------------------------------------------------------------------ map I/O
def load_tmap(path):
    m = json.load(open(path))
    va2name = {}
    for k, v in m.items():
        if k.startswith('_'):
            continue
        try:
            va2name[int(k, 16)] = v
        except ValueError:
            pass
    name2va = defaultdict(set)
    for va, n in va2name.items():
        name2va[n].add(va)
    return va2name, dict(name2va)


# --------------------------------------------------------------- the compare
STRONG = {'str', 'wstr', 'vfstr', 'f32', 'f64', 'f128'}


def evaluate(band, tmap, name2va, fn, cands, use_sym=True, truth=None, trusted=None,
             min_sym_agree=1):
    """-> (verdict, winner_va, detail)"""
    offs = fn['offs']
    if not offs:
        return 'NO-RELOC', None, {}
    slots = {}
    for c in cands:
        s = decode_slots(band, c, fn['size'], offs, fn['words'])
        if s is None:
            return 'NO-TEXT', None, {}
        slots[c] = s

    # base expectation per offset
    exp = {}
    for off in offs:
        r = fn['refs'].get(off)
        if not r:
            continue
        symname, ty, tok = r
        if tok is None:
            continue
        kind = tok[0]
        if kind == 'sym':
            if not use_sym:
                continue
            if trusted is not None and symname not in trusted:
                continue
            vas = name2va.get(symname)
            if not vas:
                continue
            exp[off] = ('symva', vas, symname)
        else:
            exp[off] = ('tok', {tok}, symname)

    if not exp:
        return 'NO-EVIDENCE', None, {}

    agree = defaultdict(set)     # cand -> set(off)
    conflict = defaultdict(set)
    resolved = {}                # (cand, off) -> resolved va or None
    for c in cands:
        for off, (mode, want, symname) in exp.items():
            rec = slots[c].get(off)
            if rec is None or rec['kind'] not in ('call', 'addr'):
                continue
            va = rec['value']
            resolved[(c, off)] = va
            if mode == 'symva':
                if va in want:
                    agree[c].add(off)
                else:
                    conflict[c].add(off)
            else:
                toks = classify_va(band, va, tmap, exact=truth is not None)
                if want & toks:
                    agree[c].add(off)
                else:
                    conflict[c].add(off)

    if truth is not None:
        # classify the ground-truth VA itself: the existing map is NOT clean --
        # e.g. 0x8227ae48 is labelled Object::StaticClassName yet references the
        # literal "TrackPanelDir".  A truth VA that CONFLICTS on map-free
        # content evidence is a map bug, not a tool miss.
        return ('TRUTH-CONFLICT' if conflict[truth] else
                'TRUTH-AGREE' if agree[truth] else 'TRUTH-UNKNOWN')

    winners = [c for c in cands if agree[c] and not conflict[c]]
    if not winners:
        return 'NO-WINNER', None, dict(agree={hex(c): len(agree[c]) for c in cands},
                                       conflict={hex(c): len(conflict[c]) for c in cands})
    if len(winners) > 1:
        return 'TIE', None, dict(winners=[hex(c) for c in winners])
    w = winners[0]
    # honesty clause: every rival must be positively excluded at a slot the
    # winner positively confirmed.
    for c in cands:
        if c == w:
            continue
        if not (conflict[c] & agree[w]):
            return 'NOT-EXCLUDED', None, dict(winner=hex(w), rival=hex(c),
                                              agree=sorted(hex(o) for o in agree[w]),
                                              rival_conflict=len(conflict[c]))
    strong = any(exp[o][0] == 'tok' and next(iter(exp[o][1]))[0] in STRONG
                 for o in agree[w])
    if not strong:
        # sym-only decision: demand several independent trusted callees, and
        # that they be DISTINCT symbols (the same callee referenced twice is
        # one fact, not two).
        distinct = {exp[o][2] for o in agree[w]}
        if len(distinct) < min_sym_agree:
            return 'WEAK-SYM', None, {}
    ev = []
    for o in sorted(agree[w]):
        mode, want, symname = exp[o]
        ev.append(dict(off=o, sym=symname,
                       want=('sym' if mode == 'symva' else next(iter(want))[0]),
                       va='0x%08x' % resolved[(w, o)]))
    return ('RESOLVED-STRONG' if strong else 'RESOLVED-SYM'), w, dict(evidence=ev)


# --------------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--results', required=True)
    ap.add_argument('--worktree', required=True)
    ap.add_argument('--band')
    ap.add_argument('--tmap')
    ap.add_argument('--out', default='/home/free/tmp/laneG/proposals.json')
    ap.add_argument('--report', default='/home/free/tmp/laneG/report.txt')
    ap.add_argument('--classes', default='MULTI,UNIQUE-ICF')
    ap.add_argument('--no-sym', action='store_true',
                    help='use only map-free evidence (strings / float consts)')
    ap.add_argument('--min-sym-agree', type=int, default=1)
    ap.add_argument('--only-units')
    ap.add_argument('--trust-audit', action='store_true',
                    help='audit the EXISTING map with map-free content evidence: '
                         'for every function whose name is already mapped onto one '
                         'of its byte-identical hits, check the strings/float '
                         'constants that VA actually references.  Emits the set of '
                         'content-CORROBORATED names (safe to use as callee '
                         'evidence) and the set of CONTRADICTED ones (existing '
                         'mispairs -- these read 100% under objdiff normalized diff '
                         'because it masks relocations, so they are invisible in '
                         'the score).')
    ap.add_argument('--trust-out', default='/home/free/tmp/laneG/trust.json')
    ap.add_argument('--trust-file',
                    help='restrict sym-class callee evidence to the corroborated '
                         'names in this trust-audit output')
    ap.add_argument('--validate', action='store_true',
                    help='held-out precision measurement: run only on functions '
                         'whose retail home is ALREADY KNOWN (exactly one hit VA '
                         'is mapped to this very symbol), feed ALL hits as '
                         'candidates, and score the pick against ground truth.')
    a = ap.parse_args()

    wt = a.worktree
    band = Band(a.band or os.path.join(wt, 'orig/45410914/band.exe'))
    tmap, name2va = load_tmap(a.tmap or os.path.join(wt, 'scripts/target_symbol_map.json'))
    want_cls = set(a.classes.split(','))
    only = set(a.only_units.split(',')) if a.only_units else None

    trusted = None
    if a.trust_file:
        trusted = set(json.load(open(a.trust_file))['corroborated'])

    res = json.load(open(a.results))

    if a.trust_audit:
        verdicts = {}
        for tu, recs in sorted(res.items()):
            if not isinstance(recs, list):
                continue
            todo = [r for r in recs if r.get('hits')]
            if not todo:
                continue
            obj = os.path.join(wt, 'build/45410914/src', tu + '.obj')
            if not os.path.exists(obj):
                continue
            ft = func_table(obj)
            for r in todo:
                fn = ft.get(r['name'])
                if fn is None or fn['size'] != r['size']:
                    continue
                hits = [int(h, 16) for h in r['hits']]
                own = [v for v in hits if tmap.get(v) == r['name']]
                if len(own) != 1:
                    continue
                v = evaluate(band, tmap, name2va, fn, [own[0]],
                             use_sym=False, truth=own[0])
                if not isinstance(v, str):
                    v = v[0]
                prev = verdicts.get(r['name'])
                # a name seen from several TUs: any conflict poisons it
                if prev == 'TRUTH-CONFLICT' or v == 'TRUTH-CONFLICT':
                    verdicts[r['name']] = 'TRUTH-CONFLICT'
                elif prev == 'TRUTH-AGREE' or v == 'TRUTH-AGREE':
                    verdicts[r['name']] = 'TRUTH-AGREE'
                else:
                    verdicts[r['name']] = v
        corr = sorted(n for n, v in verdicts.items() if v == 'TRUTH-AGREE')
        contra = sorted(n for n, v in verdicts.items() if v == 'TRUTH-CONFLICT')
        unk = sum(1 for v in verdicts.values() if v == 'TRUTH-UNKNOWN')
        json.dump(dict(corroborated=corr, contradicted=contra,
                       contradicted_va={n: '0x%08x' % sorted(name2va[n])[0]
                                        for n in contra if n in name2va}),
                  open(a.trust_out, 'w'), indent=1)
        print('map trust audit: %d names checked -> %d corroborated, '
              '%d CONTRADICTED (existing mispairs), %d no content'
              % (len(verdicts), len(corr), len(contra), unk))
        print('->', a.trust_out)
        return

    out = {}
    stats = defaultdict(int)
    lines = []
    claimed = defaultdict(list)      # va -> [(unit, name, verdict)]
    val = defaultdict(int)
    for tu, recs in sorted(res.items()):
        if not isinstance(recs, list):
            continue
        if only and tu not in only:
            continue
        if a.validate:
            todo = [r for r in recs if len(r.get('hits', [])) > 1]
        else:
            todo = [r for r in recs if r.get('cls') in want_cls]
        if not todo:
            continue
        obj = os.path.join(wt, 'build/45410914/src', tu + '.obj')
        if not os.path.exists(obj):
            continue
        ft = func_table(obj)
        keep = []
        for r in todo:
            fn = ft.get(r['name'])
            if fn is None or fn['size'] != r['size']:
                stats['no-fn'] += 1
                continue
            hits = [int(h, 16) for h in r['hits']]
            if a.validate:
                truth = [v for v in hits if tmap.get(v) == r['name']]
                if len(truth) != 1:
                    continue
                truth = truth[0]
                cands = hits
                verdict, w, detail = evaluate(band, tmap, name2va, fn, cands,
                                              use_sym=not a.no_sym, trusted=trusted,
                                              min_sym_agree=a.min_sym_agree)
                val[verdict] += 1
                if verdict.startswith('RESOLVED'):
                    ok = (w == truth)
                    val['%s/%s' % (verdict, 'HIT' if ok else 'MISS')] += 1
                    if not ok:
                        tv = evaluate(band, tmap, name2va, fn, cands,
                                      use_sym=False, truth=truth)
                        tv = tv if isinstance(tv, str) else tv[0]
                        val['MISS/' + tv] += 1
                        lines.append('VALMISS[%s] %-46s %s truth=0x%08x got=0x%08x %s'
                                     % (tv, r['name'][:46], tu, truth, w, detail))
                continue
            # our own name already sits on one of the mapped hits -> the home
            # is known; nothing to resolve (and a "resolution" would be a dup).
            if any(tmap.get(v) == r['name'] for v in hits):
                stats['ALREADY-HOMED'] += 1
                continue
            cands = [v for v in hits if v not in tmap]
            if len(cands) < 1:
                continue
            verdict, w, detail = evaluate(band, tmap, name2va, fn, cands,
                                          use_sym=not a.no_sym, trusted=trusted,
                                              min_sym_agree=a.min_sym_agree)
            stats[verdict] += 1
            if verdict.startswith('RESOLVED'):
                claimed[w].append((tu, r['name'], verdict))
                nr = dict(r)
                nr['cls'] = 'UNIQUE'
                nr['va'] = '0x%08x' % w
                nr['n_unmapped'] = 1
                nr['disambig'] = verdict
                nr['evidence'] = detail.get('evidence')
                keep.append(nr)
                lines.append('%-14s %-55s sz=%4d %s -> 0x%08x  %s' % (
                    verdict, r['name'][:55], r['size'], tu, w,
                    detail.get('evidence')))
        if keep:
            out[tu] = keep

    if a.validate:
        with open(a.report, 'w') as f:
            f.write('\n'.join(lines) + '\n')
        print('held-out validation:', dict(sorted(val.items(), key=lambda kv: -kv[1])))
        for v in ('RESOLVED-STRONG', 'RESOLVED-SYM'):
            h, m = val.get(v + '/HIT', 0), val.get(v + '/MISS', 0)
            if h + m:
                print('  %-16s precision %d/%d = %.2f%%' % (v, h, h + m, 100.0 * h / (h + m)))
        h = val.get('RESOLVED-STRONG/HIT', 0) + val.get('RESOLVED-SYM/HIT', 0)
        m = val.get('RESOLVED-STRONG/MISS', 0) + val.get('RESOLVED-SYM/MISS', 0)
        if h + m:
            print('  %-16s precision %d/%d = %.2f%%' % ('COMBINED', h, h + m, 100.0 * h / (h + m)))
        return

    # a retail VA may be claimed by only one (unit, name)
    dropped = 0
    for va, cl in claimed.items():
        if len(cl) > 1:
            names = {c[1] for c in cl}
            if len(names) > 1:
                for tu, nm, _ in cl:
                    out[tu] = [r for r in out[tu] if r['name'] != nm]
                    dropped += 1
                stats['DROP-CONTESTED'] += 1
    out = {k: v for k, v in out.items() if v}

    json.dump(out, open(a.out, 'w'), indent=1)
    with open(a.report, 'w') as f:
        f.write('\n'.join(lines) + '\n')
    tot = sum(len(v) for v in out.values())
    print('verdicts:', dict(sorted(stats.items(), key=lambda kv: -kv[1])))
    print('proposed %d resolutions across %d units (contested dropped: %d)'
          % (tot, len(out), dropped))
    print('->', a.out)


if __name__ == '__main__':
    main()
