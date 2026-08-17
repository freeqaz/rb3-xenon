#!/usr/bin/env python3
"""Compare our compiled COMDAT .text sections against retail bytes, modulo relocations.

WHY THIS EXISTS (lane STL-104, 2026-08-13)
------------------------------------------
Lane NC-REC reported that our STLport ``_Copy_Construct``/``_Param_Construct``
helpers were **104 bytes where retail's are 60** -- a 44-byte excess repeated
across ~299 helpers, briefed as a systematic source divergence.

It is NOT a divergence. It is a units error, and this tool is the instrument
that settles it. A COMDAT ``.text`` section for an EH-bearing function holds
THREE things, only one of which report.json's ``size`` counts:

    +0x00   8 B   EH prefix    -- ptr to __CxxFrameHandler (.text)
                                  + ptr to __ehfuncinfo$<fn> (.rdata)
    +0x08  60 B   the function -- this is report.json's `size`, from .pdata
    +0x44  44 B   __unwind$<n> -- EH cleanup funclet; calls the placement
                                  operator delete(void*,void*) when T's copy
                                  ctor throws. SEPARATE .pdata entry, so it is
                                  a SEPARATE report row and is never in the 60.
    ------ 112 B total section size

So 104 == 112 - 8 == body + funclet, compared against retail's function-only
extent of 60. Retail has all 112 bytes, at the same addresses, byte-identical.

Measured on the class (see the lane's merge message for the full record):
  * 109 of 113 helpers with a known retail address are byte-identical to retail
    across the FULL section -- prefix, body AND funclet -- modulo relocated
    words. The 4 exceptions are ordinary per-T defects, not a 44-byte class.
  * 89 of 90 named retail 60-byte helpers carry the 8-byte prefix at addr-8
    and a 44-byte .pdata entry at addr+60 with the identical 11-word cleanup
    shape. Whole-binary null: only 23.0% of 60-byte functions are followed by
    a 44-byte one, so the stratum is ~4.3x enriched -- structural, not chance.

THE GENERAL RULE: never compare a COMDAT section size against a report.json
``size``. They measure different spans. Use this tool, which compares BYTES and
skips relocated words (whose displacements differ by construction -- see
CLAUDE.md on why raw memcmp of function bodies is silently vacuous).

READING THE MAP (lane task89, 2026-08-16)
-----------------------------------------
`scripts/target_symbol_map.json` does NOT hold "address -> name" for every row.
A row may claim no name at all, and that state is load-bearing here:

  * ``"0xADDR": null`` -- **deliberately unclaimed**. The address is real code,
    but no name is attributed to it. This is the map-side spelling of
    ``verdict='IDENTITY_UNESTABLISHED'`` (docs/decomp/VERDICT_STATES.md): the
    premise "this body IS that function" is unsound, so the map claims nothing
    rather than claiming something false.
  * an address on ``_denylist`` -- **refused**, whatever string value it still
    carries. Five such rows in the checked-in map do carry a name, including
    ``??$__destroy_aux@ULevelData@@...`` at both `0x82b5b1d0` and `0x82b63ec8`
    -- the canonical unadjudicable pair.

  * an address on ``_icf_arbitrary`` / ``_bijection_arbitrary`` -- **APPLIED,
    and deliberately so**. The bytes are witnessed; what is unestablished is
    WHICH of N ICF-folded names belongs on the VA. Refusing them was measured
    and rejected (lane task100, 2026-08-17): it would drop 957 strict-100
    name-checked matches build-wide, and on ``--pattern ?SyncProperty@`` alone
    it moved 59 rows out of "byte-identical to retail" and into "unidentified"
    with ``differing`` unchanged -- i.e. it deletes true byte evidence and
    reports it as missing evidence, which reads WORSE, not safer.
    ⚠ OPEN, and the honest residue: those rows still satisfy the byte claim
    under any member of their class, so the per-symbol line "instantiation N
    lives at 0xVA" is not established for them. The map's own comments require
    a tool "deriving identity, callers, or unit ownership" to treat them as
    UNRESOLVED. The fix is to LABEL them in the per-symbol rows and count them
    apart in the summary -- not to refuse them. Not implemented here.

This tool prints "SECTION byte-identical to retail", which IS an identity
claim, so it must not source an address from a row that claims nothing.  Both
states are therefore resolved through the renamer's own ``load_address_map``
rather than a local ``json.load`` + substring test, so the filter here and the
filter the build applies cannot drift (same rule, same reason, as
tools/map_name_injectivity.py).

It used to be a bare dict comprehension over ``raw.items()``, which crashed --
``TypeError: argument of type 'NoneType' is not iterable`` -- for ANY
``--pattern`` the moment the map held one null, i.e. the tool was unusable on
exactly the unestablished-identity population it is most needed for. The counts
are REPORTED, not just skipped: a row dropped in silence is this repo's
recurring failure mode (the `_denylist` sat declared-and-ignored by the loader
itself until f3fe9ab1).

Usage:
    python3 tools/comdat_retail_verify.py --pattern _Copy_Construct@
    python3 tools/comdat_retail_verify.py --pattern '??0Foo@@' --show-diffs 20
"""
import argparse
import contextlib
import glob
import io
import json
import os
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from obj_target_symbol_renamer import REFUSAL_KEYS, load_address_map  # noqa: E402


def read_coff(path):
    """Return (data, sections, symbols) for a COFF .obj, or None."""
    d = open(path, 'rb').read()
    if len(d) < 20:
        return None
    _mach, nsec, _ts, symptr, nsym, optsz, _ch = struct.unpack_from('<HHIIIHH', d, 0)
    if not symptr or not nsym:
        return None
    secs = []
    off = 20 + optsz
    for _ in range(nsec):
        raw = d[off:off + 40]
        vsz, va, szraw, ptrraw, ptrrel, _ptrln, nrel, _nln, ch = struct.unpack_from('<IIIIIIHHI', raw, 8)
        secs.append(dict(name=raw[:8], size=szraw, ptr=ptrraw, chars=ch,
                         nrel=nrel, ptrrel=ptrrel))
        off += 40
    strtab = d[symptr + 18 * nsym:]
    syms = []
    i = 0
    while i < nsym:
        raw = d[symptr + 18 * i:symptr + 18 * i + 18]
        if raw[:4] == b'\x00\x00\x00\x00':
            stroff = struct.unpack_from('<I', raw, 4)[0]
            name = strtab[stroff:strtab.index(b'\x00', stroff)].decode('latin1')
        else:
            name = raw[:8].rstrip(b'\x00').decode('latin1')
        val, sec, _typ, sclass, naux = struct.unpack_from('<IhHBB', raw, 8)
        syms.append(dict(name=name, val=val, sec=sec, sclass=sclass, idx=i))
        i += 1 + naux
    return d, secs, syms


class Retail:
    """Address-addressable view of the retail PE."""

    def __init__(self, path):
        d = open(path, 'rb').read()
        pe = struct.unpack_from('<I', d, 0x3C)[0]
        nsec = struct.unpack_from('<H', d, pe + 6)[0]
        optsz = struct.unpack_from('<H', d, pe + 20)[0]
        self.base = struct.unpack_from('<I', d, pe + 24 + 28)[0]
        self.d = d
        self.secs = []
        off = pe + 24 + optsz
        for _ in range(nsec):
            raw = d[off:off + 40]
            name = raw[:8].rstrip(b'\x00').decode()
            vsz, va, _szraw, ptr = struct.unpack_from('<IIII', raw, 8)
            self.secs.append((name, va, vsz, ptr))
            off += 40

    def read(self, addr, n):
        for _nm, va, vsz, ptr in self.secs:
            if self.base + va <= addr < self.base + va + vsz:
                fo = ptr + (addr - (self.base + va))
                return self.d[fo:fo + n]
        return None

    def pdata_lengths(self):
        """addr -> function length in bytes. FunctionLen is 18 bits at bit
        offset 8 of the second BIG-ENDIAN word (getting either wrong inverts
        the verdict -- see CLAUDE.md)."""
        pd = [s for s in self.secs if s[0] == '.pdata'][0]
        raw = self.d[pd[3]:pd[3] + pd[2]]
        out = {}
        for i in range(0, len(raw), 8):
            a, f = struct.unpack_from('>II', raw, i)
            if a:
                out[a] = ((f >> 8) & 0x3FFFF) * 4
        return out


def classify_map_rows(raw):
    """-> dict of row-population counts for `raw`, a parsed target_symbol_map.

    Pure and total: no I/O, no globals, and it must survive the null vector
    (`{}`) and every value type the map has ever held. REPORTING ONLY -- the
    admission filter is `load_address_map`, which is imported rather than
    re-derived. These counts exist so that what the filter drops is visible in
    the summary instead of vanishing.

    Buckets partition `raw` exactly, and test_comdat_retail_verify.py asserts
    that on the checked-in map -- so a row cannot be dropped by falling into
    no bucket, which is the same shape as the defect being fixed:
      unclaimed  -- `"0xADDR": null`, deliberately unclaimed / identity not
                    established. Carries no name, so no --pattern can ever
                    match it. NOT an empty string: '' would silently match
                    every pattern-free lookup, which is the bug next door.
      denied     -- an address key on a refusal list (whether or not it still
                    carries a name), plus the denied addresses that are absent
                    from the map body. The refusal keys are imported as
                    `REFUSAL_KEYS` rather than hardcoded here: this bucket used
                    to re-derive `_denylist` locally, so had the loader ever
                    started refusing a second key the summary would have gone
                    on reporting the old count -- a silent drop, which is the
                    exact failure this disclosure block exists to prevent.
                    Measured drift when that was tried (lane task100): loader
                    refusing 945 addresses, summary still printing 5.
      nonstring  -- a list/dict/number under a `0x` key. Never legitimate.
      metadata   -- non-`0x` keys (`_denylist`, `_icf_arbitrary`, ...).
      claimed    -- string-valued `0x` rows that are not denied.
    """
    denied_addrs = set()
    for key in REFUSAL_KEYS:
        for entry in raw.get(key, []) or []:
            if isinstance(entry, str) and entry.lower().startswith('0x'):
                denied_addrs.add(int(entry, 16))

    out = dict(total=len(raw), claimed=0, unclaimed=0, denied=0,
               nonstring=0, metadata=0)
    seen_denied = set()
    for k, v in raw.items():
        if not isinstance(k, str) or not k.lower().startswith('0x'):
            out['metadata'] += 1
            continue
        try:
            addr = int(k, 16)
        except ValueError:
            out['nonstring'] += 1
            continue
        if addr in denied_addrs:
            out['denied'] += 1
            seen_denied.add(addr)
        elif v is None:
            out['unclaimed'] += 1
        elif isinstance(v, str):
            out['claimed'] += 1
        else:
            out['nonstring'] += 1
    # A denied address need not appear in the map body at all; it is still a
    # refusal, and saying so keeps the printed denial count honest.
    out['denied_absent'] = len(denied_addrs - seen_denied)
    return out


def resolve_addresses(map_path, pattern):
    """-> (name -> retail VA for names matching `pattern`, row counts).

    The APPLIED map, via the renamer's own loader: null rows are unclaimed,
    `_denylist` rows are refused, non-strings are dropped. The loader lives in
    THIS checkout's `scripts/` (not `--project-dir`'s) so the rule is the one
    that was reviewed with this tool, not whatever revision the pointed-at
    worktree happens to carry.

    `load_address_map` emits `fn_XXXXXXXX` and `lbl_XXXXXXXX` for the same
    address; read the `fn_` half only or every row counts twice. Its own
    skip tally is swallowed -- this tool reprints those numbers in its summary,
    where they sit next to the match counts they explain.
    """
    raw = json.loads(Path(map_path).read_text())
    with contextlib.redirect_stdout(io.StringIO()):
        applied = load_address_map(Path(map_path))
    addr_of = {name: int(key[3:], 16)
               for key, name in applied.items()
               if key.startswith('fn_') and pattern in name}
    return addr_of, classify_map_rows(raw)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--project-dir', default='.', help='worktree to read from')
    ap.add_argument('--pattern', required=True,
                    help='substring matched against mangled symbol names')
    ap.add_argument('--show-diffs', type=int, default=10)
    ap.add_argument('--json', help='write per-symbol rows here')
    args = ap.parse_args()

    root = args.project_dir
    retail = Retail(os.path.join(root, 'orig/45410914/band.exe'))
    lens = retail.pdata_lengths()
    addr_of, mapstats = resolve_addresses(
        os.path.join(root, 'scripts/target_symbol_map.json'), args.pattern)

    ours = {}
    pat = os.path.join(root, 'build/45410914/src/**/*.obj')
    for p in glob.glob(pat, recursive=True):
        r = read_coff(p)
        if not r:
            continue
        dd, secs, syms = r
        for s in syms:
            n = s['name']
            if args.pattern not in n or s['sec'] <= 0 or s['sclass'] != 2:
                continue
            sec = secs[s['sec'] - 1]
            if not sec['name'].startswith(b'.text') or n in ours:
                continue
            rel = {}
            for i in range(sec['nrel']):
                va, _si, ty = struct.unpack_from('<IIH', dd, sec['ptrrel'] + 10 * i)
                rel[va] = ty
            ours[n] = dict(unit=os.path.basename(p), size=sec['size'], rel=rel,
                           body=dd[sec['ptr']:sec['ptr'] + sec['size']],
                           off=s['val'])

    if not ours:
        sys.exit('no COMDAT .text symbols matched %r -- build first?' % args.pattern)

    # `noaddr` (no claim in the map) and `unreadable` (claimed, but the retail
    # read fell outside a section) used to share one counter printed as
    # "unidentified". They are different findings -- one is missing evidence,
    # the other is a bad address or a bad extent -- so they are counted apart.
    ident, diff, noaddr, unreadable, rows = 0, 0, 0, 0, []
    for n, o in sorted(ours.items()):
        a = addr_of.get(n)
        if a is None:
            noaddr += 1
            continue
        rb = retail.read(a - o['off'], o['size'])
        if rb is None or len(rb) < o['size']:
            unreadable += 1
            continue
        bad = []
        for i in range(0, o['size'], 4):
            x = struct.unpack_from('>I', o['body'], i)[0]
            y = struct.unpack_from('>I', rb, i)[0]
            if x != y and i not in o['rel']:
                bad.append((i, x, y))
        rows.append(dict(sym=n, unit=o['unit'], addr='0x%08x' % a,
                         section_size=o['size'], pdata_size=lens.get(a),
                         sym_off=o['off'], nbad=len(bad),
                         bad=[['0x%02x' % i, '%08x' % x, '%08x' % y] for i, x, y in bad]))
        if bad:
            diff += 1
        else:
            ident += 1

    # Disclose the map population BEFORE the verdict. The rows this tool
    # cannot see are part of the reading of the rows it can: an unclaimed or
    # denied address is not a passing check and not a failing one, it is a
    # symbol whose identity nobody has established, and it must not read as
    # absent.
    ms = mapstats
    print('target_symbol_map.json: %d rows -> %d claimed address(es) scored'
          % (ms['total'], ms['claimed']))
    print('  SKIPPED, deliberately unclaimed ("0xADDR": null) : %d'
          % ms['unclaimed'])
    print('  SKIPPED, _denylist (claims refused)              : %d%s'
          % (ms['denied'],
             '  (+%d denied address(es) absent from the map body)' % ms['denied_absent']
             if ms['denied_absent'] else ''))
    if ms['nonstring']:
        print('  SKIPPED, non-string value under a 0x key         : %d'
              % ms['nonstring'])
    print('  (unclaimed rows carry no name and can never match a --pattern)')
    print('matched pattern %r: %d distinct instantiations in our build' % (args.pattern, len(ours)))
    print('  with a retail address in target_symbol_map.json : %d  (unidentified: %d)'
          % (ident + diff, noaddr))
    if unreadable:
        print('  claimed but retail read out of range            : %d' % unreadable)
    print('  SECTION byte-identical to retail (reloc words skipped): %d' % ident)
    print('  differing                                            : %d' % diff)
    shown = 0
    for r in rows:
        if not r['nbad'] or shown >= args.show_diffs:
            continue
        shown += 1
        print('  %-26s sec=%d pdata=%s nbad=%d %s'
              % (r['unit'], r['section_size'], r['pdata_size'], r['nbad'], r['sym'][:64]))
        for o, x, y in r['bad'][:6]:
            print('       +%s ours=%s retail=%s' % (o, x, y))
    if args.json:
        json.dump(rows, open(args.json, 'w'), indent=1)
        print('wrote %s' % args.json)


if __name__ == '__main__':
    main()
