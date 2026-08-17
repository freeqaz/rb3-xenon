#!/usr/bin/env python3
"""LOCATE-BY-BODY: identify anonymous retail rows by relocation-normalized
BIJECTIVE body identity against the bodies we already compile.

THE DISTINCTION FROM THE DRAINED CHANNEL
----------------------------------------
`docs/decomp/identity-transfer/B2-FINDINGS-oracle-wall.md` closed the
identity-transfer vein with a hard number: **0 of 10 fresh TUs landed**, because
that channel is PORT-THEN-LOCATE -- take the rb3-Wii BinDiff oracle's VA for a
method, pin it, and *hope* the compiled body matches retail.  It hit a
body-divergence wall (axis-A struct layout, axis-B/D codegen).

This channel is LOCATE-BY-BODY and inverts the dependency: a row is identified
ONLY BECAUSE its body already matches.  Body divergence is therefore not a wall
here -- it is the filter.  A function whose body diverges simply produces no
candidate and costs nothing.  The yield is bounded by "how many bodies we
already compile are byte-identical (mod relocations) to a currently-anonymous
retail function", which is exactly the quantity this tool measures.

WHY BIJECTIVE, AND WHY NO METRIC IS CONSULTED
---------------------------------------------
Naming a retail address is the highest-risk operation in the repo: an audit
found 20.6% of one wave misidentified, and under `name_check` an unproven alias
is pure FORGIVENESS -- it lifts the score BY CONSTRUCTION, so "it scored better"
is not evidence.  A hit is reported only when the body hash maps to exactly ONE
of our symbols and exactly ONE retail row.  /OPT:ICF folds identical bodies to a
single survivor, so tiny accessors legitimately collide; picking one of a folded
group is a coin flip and is REFUSED.

THE CONTROL (--holdout) -- this is the part that was missing
------------------------------------------------------------
`scripts/harvest/autocarve_global_identity.py` shipped this channel's ancestor
with the note "Its PRECISION IS UNVALIDATED -- laneBT5 did not run the
--holdout mode".  An identification instrument whose FP rate is unmeasured is
exactly the "instrument that cannot fail" pattern.

--holdout runs the identical pipeline against rows whose TRUE name is already
known (the map named them, the renamer applied it), with that name hidden.  A
recovered name that disagrees with the known one is a measured false positive.
The control CAN fail, and its failure mode is visible.

USAGE
    python3 tools/ident_body_channel.py --worktree <wt> --holdout
    python3 tools/ident_body_channel.py --worktree <wt> --propose out.json
"""

import argparse
import collections
import hashlib
import json
import re
import struct
import sys
from pathlib import Path

PLACEHOLDER_RE = re.compile(r'^(fn|lbl|jumptable|data|bss|rdata|sdata|sbss)_[0-9A-Fa-f]+$')
IMAGE_SCN_CNT_CODE = 0x20

SEL_NO_DUPLICATES = 1
SEL_ANY = 2


def is_placeholder(n):
    return bool(PLACEHOLDER_RE.match(n))


def is_aux_code_symbol(name):
    return name.startswith('__unwind$') or name.startswith('__ehhandler$')


def eh_boundaries(section_symbols):
    """Offsets a slice may END at that are NOT function definitions.

    `scripts/obj_eh_boundary_patcher.py` (build step, stamp
    `eh_boundary_patched.stamp`) appends a STATIC/type-0 `$EH*` symbol at every
    interior EH prefix, because MSVC leaves only a class-6 `$M#####` debug label
    at a function's true end. Type 0 is exactly what a `type == 0x20` filter
    discards, so the boundary the build inserts for this purpose was being
    thrown away -- see `function_slices`.

    Deliberately keyed on the `$EH` name and class 3 / type 0, NOT on storage
    class alone: class-6 `$M` labels sit INSIDE a body (695,516 of them on this
    build) and bounding on one truncates a function at its prologue.
    """
    return {s['value'] for s in section_symbols
            if s['storage'] == 3 and s['type'] == 0 and s['name'].startswith('$EH')}


# ---------------------------------------------------------------- COFF reader
def parse_coff(data):
    """Minimal PE/COFF + bigobj reader. Returns (sections, symbols)."""
    if len(data) < 20:
        return [], []
    machine, nsec, _t, psym, nsym, opt, _c = struct.unpack_from('<HHIIIHH', data, 0)
    ent, sh_off = 18, 20 + opt
    bigobj = False
    if machine == 0 and nsec == 0xFFFF:
        bigobj = True
        nsec, psym, nsym = struct.unpack_from('<III', data, 44)
        ent, sh_off = 20, 56
    strtab = psym + nsym * ent

    sections = []
    for i in range(nsec):
        o = sh_off + i * 40
        name = data[o:o + 8].rstrip(b'\0').decode('latin1')
        _vs, _va, size, ptr, prel, _pl, nrel, _nl, chars = struct.unpack_from('<IIIIIIHHI', data, o + 8)
        relocs = []
        for k in range(nrel):
            rva, rsym, rtyp = struct.unpack_from('<IIH', data, prel + k * 10)
            relocs.append((rva, rsym, rtyp))
        sections.append({'name': name, 'raw': data[ptr:ptr + size] if ptr else b'',
                         'chars': chars, 'relocs': relocs, 'sel': None})

    symbols = []
    i = 0
    while i < nsym:
        o = psym + i * ent
        raw = data[o:o + 8]
        if bigobj:
            val, sec, typ, sclass, naux = struct.unpack_from('<IiHBB', data, o + 8)
        else:
            val, sec, typ, sclass, naux = struct.unpack_from('<IhHBB', data, o + 8)
        if raw[:4] == b'\0\0\0\0':
            soff = struct.unpack_from('<I', raw, 4)[0]
            end = data.index(b'\0', strtab + soff)
            name = data[strtab + soff:end].decode('latin1')
        else:
            name = raw.rstrip(b'\0').decode('latin1')
        symbols.append({'idx': i, 'name': name, 'value': val, 'section': sec,
                        'type': typ, 'storage': sclass})
        # COMDAT selection lives in the section-definition aux record
        if naux and sclass == 3 and 0 < sec <= len(sections) and name.startswith('.'):
            ao = o + ent
            if ao + 18 <= len(data):
                sel = data[ao + 14]
                if sections[sec - 1]['sel'] is None:
                    sections[sec - 1]['sel'] = sel
        i += 1 + naux
    return sections, symbols


def function_slices(path):
    """Yield (name, body, relocs, comdat_sel).

    Slices a code section by consecutive defining-symbol values, so the
    [+0 EH prefix][+8 entry][+N funclet] layout does not swallow the function --
    AND hands the successor's own 8-byte prefix back to the successor.

    The second half is the part that was missing. `type == 0x20` decides what to
    YIELD; it must not also decide where a slice ENDS, or the `$EH*` boundary
    the build injects (class 3, type 0) is discarded and the slice runs on into
    the next region's prefix. Measured on build 45410914: 6,393 slices move,
    257 of them into agreement with a retail `.pdata` extent they had been +8
    against.
    """
    try:
        data = Path(path).read_bytes()
    except OSError:
        return
    sections, symbols = parse_coff(data)
    if not sections:
        return
    idx_name = {s['idx']: s['name'] for s in symbols}
    by_sec = collections.defaultdict(list)
    for s in symbols:
        if s['section'] > 0:
            by_sec[s['section'] - 1].append(s)
    for si, sec in enumerate(sections):
        if not (sec['chars'] & IMAGE_SCN_CNT_CODE):
            continue
        defs = [s for s in by_sec.get(si, [])
                if s['name'] != sec['name'] and s['storage'] in (2, 3) and s['type'] == 0x20]
        if not defs:
            continue
        raw = sec['raw']
        marks = eh_boundaries(by_sec.get(si, []))
        pts = sorted({(s['value'], s['name']) for s in defs})
        bounds = sorted({v for v, _ in pts} | marks | {len(raw)})
        nxt = {v: bounds[i + 1] for i, v in enumerate(bounds[:-1])}
        at = collections.defaultdict(list)
        for v, n in pts:
            at[v].append(n)
        rel = {o: idx_name.get(i, '?') for (o, i, _t) in sec['relocs']}
        for v, name in pts:
            if v % 4 or v >= len(raw):
                continue
            if is_aux_code_symbol(name):
                continue
            end = nxt.get(v, len(raw))
            # FALLBACK, for a tree with no `$EH*` marks -- an unpatched obj, or a
            # build that ran before the patcher step. Two words that RELOCATE to
            # `__CxxFrameHandler` and `__ehfuncinfo$...` are an EH prefix by
            # definition; they cannot be instructions. That is the whole test.
            #
            # It deliberately does NOT also require the successor to be
            # `__catch$`-named. 3 of the 6,395 interior prefixes on this build
            # precede an ORDINARY FUNCTION in a non-COMDAT multi-function
            # `.text`, so a successor-name test silently misses them -- the
            # defect still present in `coff_bodies_ext`.
            if end not in marks and end < len(raw) and end - 8 > v \
                    and raw[end - 8:end] == b'\0' * 8 \
                    and rel.get(end - 8) == '__CxxFrameHandler' \
                    and rel.get(end - 4, '').startswith('__ehfuncinfo$'):
                end -= 8
            rl = [o - v for (o, _i, _t) in sec['relocs'] if v <= o < end]
            yield name, raw[v:end], rl, sec['sel']


def body_hash(body, relocs):
    """sha1 over instruction words with relocated fields masked.

    A raw memcmp is silently vacuous here: the same function at two addresses
    has different bl displacements, so identical functions are NOT identical
    bytes.  Branch forms keep only the opcode; other forms keep opcode+registers.
    """
    out = bytearray()
    rset = set(relocs)
    n = len(body) - (len(body) % 4)
    for off in range(0, n, 4):
        word = struct.unpack_from('>I', body, off)[0]
        if any(off <= r < off + 4 for r in rset):
            op = word >> 26
            word = (word & 0xFC000000) if op in (16, 18) else (word & 0xFFFF0000)
        out += struct.pack('>I', word)
    return hashlib.sha1(bytes(out)).hexdigest()


# ---------------------------------------------------------------- main
def build_supply(wt, unit_cfg, verbose=True):
    """hash -> set(our symbol names); name -> set(units whose base obj defines it)."""
    by_hash = collections.defaultdict(set)
    name_units = collections.defaultdict(set)
    name_size = {}
    nobj = 0
    for uname, u in unit_cfg.items():
        bp = u.get('base_path')
        if not bp:
            continue
        p = wt / bp
        if not p.exists():
            continue
        nobj += 1
        for name, body, relocs, sel in function_slices(p):
            if is_placeholder(name) or not body:
                continue
            h = body_hash(body, relocs)
            by_hash[h].add(name)
            name_units[name].add(uname)
            name_size[name] = len(body)
    if verbose:
        print(f'  supply: {nobj} base objs, {len(name_units)} distinct symbols, '
              f'{len(by_hash)} distinct body hashes', file=sys.stderr)
    return by_hash, name_units, name_size


def build_demand(wt, unit_cfg, verbose=True):
    """target obj rows: unit -> list of (name, hash, size)."""
    demand = {}
    nobj = 0
    for uname, u in unit_cfg.items():
        tp = u.get('target_path')
        if not tp:
            continue
        p = wt / tp
        if not p.exists():
            continue
        nobj += 1
        rows = []
        for name, body, relocs, sel in function_slices(p):
            if not body:
                continue
            rows.append((name, body_hash(body, relocs), len(body)))
        demand[uname] = rows
    if verbose:
        print(f'  demand: {nobj} target objs, '
              f'{sum(len(v) for v in demand.values())} rows', file=sys.stderr)
    return demand


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--worktree', required=True)
    ap.add_argument('--holdout', action='store_true')
    ap.add_argument('--propose')
    args = ap.parse_args()
    wt = Path(args.worktree)

    objdiff = json.loads((wt / 'objdiff.json').read_text())
    unit_cfg = {u['name']: u for u in objdiff['units']}

    print('indexing...', file=sys.stderr)
    supply, name_units, name_size = build_supply(wt, unit_cfg)
    demand = build_demand(wt, unit_cfg)

    # retail-side hash multiplicity, computed over ALL target rows binary-wide:
    # a body that appears at several retail addresses cannot be bijectively named.
    retail_mult = collections.Counter()
    for uname, rows in demand.items():
        for name, h, size in rows:
            retail_mult[h] += 1

    # ---- which target names are already real (renamer applied a map entry)?
    named_rows, anon_rows = [], []
    for uname, rows in demand.items():
        for name, h, size in rows:
            (anon_rows if is_placeholder(name) else named_rows).append((uname, name, h, size))

    print(f'  target rows: {len(named_rows)} named, {len(anon_rows)} anonymous',
          file=sys.stderr)

    # ---- TRAP CHECK: a pre-renamer worktree makes every mangled name read absent.
    if len(named_rows) < 1000:
        sys.exit('REFUSING: only %d named target rows -- this worktree looks '
                 'PRE-RENAMER. Build it first.' % len(named_rows))
    print(f'  renamer trap check PASSED ({len(named_rows)} named target rows)\n',
          file=sys.stderr)

    def resolve(h):
        """Bijective resolution: exactly one of our symbols, exactly one retail row."""
        mine = supply.get(h)
        if not mine or len(mine) != 1:
            return None, ('ambig_ours' if mine else 'no_body')
        if retail_mult[h] != 1:
            return None, 'ambig_retail'
        return next(iter(mine)), 'ok'

    if args.holdout:
        print('=== HOLDOUT CONTROL: recover names we already know ===')
        print('Population: target rows the map already names, name hidden.')
        stats = collections.Counter()
        wrong = []
        for uname, tname, h, size in named_rows:
            got, why = resolve(h)
            if got is None:
                stats[why] += 1
                continue
            if got == tname:
                stats['CORRECT'] += 1
            else:
                stats['WRONG'] += 1
                wrong.append((uname, tname, got, size))
        decided = stats['CORRECT'] + stats['WRONG']
        print(f'  population        {len(named_rows)}')
        for k in ('CORRECT', 'WRONG', 'no_body', 'ambig_ours', 'ambig_retail'):
            print(f'  {k:14s} {stats[k]:7d}')
        if decided:
            fp = 100.0 * stats['WRONG'] / decided
            print(f'\n  DECIDED {decided}   measured FALSE-POSITIVE RATE = {fp:.2f}% '
                  f'({stats["WRONG"]}/{decided})')
        else:
            print('\n  ⛔ CONTROL VACUOUS: nothing decided.')
        print('\n  sample disagreements (first 15):')
        for uname, tname, got, size in wrong[:15]:
            print(f'    {uname:38s} {size:5d}B  map={tname[:44]}')
            print(f'    {"":38s} {"":5s}   got={got[:44]}')
        print()

        # ------------------------------------------------------------------
        # LEAVE-ONE-OUT: the control that matches the TARGET stratum.
        #
        # The holdout above is in the WRONG STRATUM and is therefore OPTIMISTIC.
        # Its population is rows the map already names, which is enriched for
        # "the true owner is among the symbols we compile".  The population we
        # actually want to name is ANONYMOUS -- rows nobody has identified --
        # which is enriched for the opposite.  Every disagreement printed above
        # has the same shape (ICF siblings: ?NewObject@A@@ vs ?NewObject@B@@,
        # ??_E vs ??_G), i.e. bijectivity held ONLY because our supply lacked
        # the true owner.
        #
        # ⛔ And the retail-side multiplicity guard is STRUCTURALLY BLIND to
        # this: /OPT:ICF has already folded the group to ONE surviving address,
        # so retail_mult == 1 is exactly what a folded group looks like.
        #
        # So: delete the true name from the supply and re-ask.  Any confident
        # answer is a measured false positive under supply-incompleteness --
        # the dominant FP mechanism in the anonymous stratum.
        # ------------------------------------------------------------------
        print('=== LEAVE-ONE-OUT CONTROL: true owner REMOVED from supply ===')
        print('Simulates the anonymous stratum, where the true owner is often')
        print('a symbol we do not compile. Any answer here is a FALSE POSITIVE.')
        lo = collections.Counter()
        lo_examples = []
        for uname, tname, h, size in named_rows:
            mine = supply.get(h)
            if not mine or tname not in mine:
                lo['not_applicable'] += 1
                continue
            survivors = mine - {tname}
            if not survivors:
                lo['SILENT (correct refusal)'] += 1
            elif len(survivors) == 1 and retail_mult[h] == 1:
                lo['CONFIDENT WRONG ANSWER'] += 1
                if len(lo_examples) < 10:
                    lo_examples.append((uname, tname, next(iter(survivors)), size))
            else:
                lo['ambiguous (refused)'] += 1
        applicable = sum(v for k, v in lo.items() if k != 'not_applicable')
        for k in ('SILENT (correct refusal)', 'ambiguous (refused)',
                  'CONFIDENT WRONG ANSWER', 'not_applicable'):
            print(f'  {k:26s} {lo[k]:7d}')
        if applicable:
            print(f'\n  applicable {applicable}   FP under supply-incompleteness = '
                  f'{100.0*lo["CONFIDENT WRONG ANSWER"]/applicable:.2f}%')
        else:
            print('\n  ⛔ CONTROL VACUOUS: applicable population is 0.')
        for uname, tname, got, size in lo_examples[:8]:
            print(f'    {uname:34s} {size:5d}B  true={tname[:40]}')
            print(f'    {"":34s} {"":5s}   claimed={got[:40]}')
        print()

        # Stratify the SAME control by body size, so the FP rate becomes a
        # SELECTOR rather than just a warning. A long body carries far more
        # entropy than an 8-byte `stfs`/`blr` accessor, so the coin-flip risk
        # should collapse with size -- but that must be MEASURED, not asserted.
        print('  FP under supply-incompleteness, STRATIFIED BY BODY SIZE:')
        buckets = [(0, 16), (17, 64), (65, 128), (129, 256), (257, 1 << 30)]
        agg = {b: [0, 0] for b in buckets}   # [applicable, confident-wrong]
        for uname, tname, h, size in named_rows:
            mine = supply.get(h)
            if not mine or tname not in mine:
                continue
            for b in buckets:
                if b[0] <= size <= b[1]:
                    break
            agg[b][0] += 1
            survivors = mine - {tname}
            if len(survivors) == 1 and retail_mult[h] == 1:
                agg[b][1] += 1
        for b in buckets:
            n, w = agg[b]
            lab = f'{b[0]}-{b[1] if b[1] < (1 << 30) else "+"}B'
            r = f'{100.0*w/n:6.2f}%' if n else '   n/a'
            print(f'    {lab:>10s}  applicable {n:6d}  wrong {w:5d}   FP {r}')
        print()

        # The residual large-body errors are all STL TEMPLATE SIBLINGS: two
        # instantiations over different pointer-sized T compile to identical
        # code, so no amount of body entropy separates them. Test the gate.
        print('  FP by TEMPLATE-ness x size (the proposed landing gate):')
        cells = {}
        for uname, tname, h, size in named_rows:
            mine = supply.get(h)
            if not mine or tname not in mine:
                continue
            tmpl = '?$' in tname
            big = size >= 128
            key = ('template' if tmpl else 'non-template',
                   '>=128B' if big else '<128B')
            c = cells.setdefault(key, [0, 0])
            c[0] += 1
            survivors = mine - {tname}
            if len(survivors) == 1 and retail_mult[h] == 1:
                c[1] += 1
        for key in sorted(cells):
            n, w = cells[key]
            print(f'    {key[0]:13s} {key[1]:7s}  applicable {n:6d}  wrong {w:5d}'
                  f'   FP {100.0*w/n:6.2f}%' if n else '')
        print()

        # The THIRD constraint, and the only one independent of bytes:
        # SAME-UNIT.  Retail is built without whole-program optimization, so TU
        # spatial grouping in .text survives (CLAUDE.md).  A claim is far more
        # credible when the retail row sits inside the pinned span of the very
        # TU whose obj defines the claimed symbol.  Measure whether that
        # actually suppresses the wrong answers, rather than assuming it.
        print('  Does the SAME-UNIT constraint suppress the wrong answers?')
        su = {True: [0, 0], False: [0, 0]}
        for uname, tname, h, size in named_rows:
            mine = supply.get(h)
            if not mine or tname not in mine:
                continue
            survivors = mine - {tname}
            if not (len(survivors) == 1 and retail_mult[h] == 1):
                continue
            claimed = next(iter(survivors))
            # would this wrong claim have passed the same-unit filter?
            su[uname in name_units.get(claimed, ())][1] += 1
        for uname, tname, h, size in named_rows:
            mine = supply.get(h)
            if not mine or tname not in mine:
                continue
            su[uname in name_units.get(tname, ())][0] += 1
        print(f'    wrong claims that WOULD have passed same-unit: {su[True][1]}')
        print(f'    wrong claims rejected by same-unit           : {su[False][1]}')
        tot_wrong = su[True][1] + su[False][1]
        if tot_wrong:
            print(f'    ⇒ same-unit removes {100.0*su[False][1]/tot_wrong:.1f}% '
                  f'of the false positives')
        print()

    # ---- the proposal population: anonymous, unpaired rows
    #
    # Byte accounting comes from report.json, NOT from my slice lengths: a
    # section slice runs to the next defining symbol or section end, so it
    # absorbs alignment padding and trailing unwind bytes that dtk does not
    # bill to the function.  Using slice lengths would overstate the prize.
    # mpn also comes from report: an anonymous row already at mpn 100 is
    # funclet-paired by byte signature and needs no name at all.
    report = json.loads((wt / 'build/45410914/report.json').read_text())
    rep = {}
    for u in report['units']:
        for f in u.get('functions', []):
            rep[(u['name'], f['name'])] = (int(f.get('size', 0) or 0),
                                           float(f.get('match_percent_normalized', 0) or 0))

    print('=== ANONYMOUS ROWS RESOLVED BY BODY IDENTITY ===')
    tiers = collections.Counter()
    tier_bytes = collections.Counter()
    proposals = []
    for uname, tname, h, size in anon_rows:
        rsize, rmpn = rep.get((uname, tname), (0, 0.0))
        got, why = resolve(h)
        if got is None:
            tiers[why] += 1
            tier_bytes[why] += rsize
            continue
        same_unit = uname in name_units.get(got, ())
        if rmpn >= 100.0:
            tier = 'C_already_paired'   # funclet byte-signature; naming buys nothing
        else:
            tier = 'A_same_unit' if same_unit else 'B_other_unit'
        tiers[tier] += 1
        tier_bytes[tier] += rsize
        proposals.append({'unit': uname, 'target': tname, 'name': got,
                          'size': rsize, 'mpn': rmpn, 'tier': tier,
                          'defining_units': sorted(name_units.get(got, ()))})
    total = sum(tiers.values())
    for k in ('A_same_unit', 'B_other_unit', 'C_already_paired',
              'ambig_ours', 'ambig_retail', 'no_body'):
        print(f'  {k:16s} {tiers[k]:7d} rows  {tier_bytes[k]:10,d} B')
    print(f'  TOTAL          {total:7d} rows')

    if args.propose:
        Path(args.propose).write_text(json.dumps(proposals, indent=1))
        print(f'\nwrote {len(proposals)} proposals to {args.propose}')


if __name__ == '__main__':
    main()
