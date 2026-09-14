#!/usr/bin/env python3
"""thunk_false100_census.py -- the adjustor thunks that read fuzzy==100 ONLY
because their branch target is a FORGIVEN placeholder.

THE POPULATION, AND WHY NO EXISTING TOOL REACHES IT
===================================================
An adjustor thunk is 12-16 bytes of pure boilerplate:

    lwz  r11,-4(rN) ; subf rN,r11,rN ; [addi rN,rN,-M ;] b <BODY>

Its instruction stream is IDENTICAL for every thunk in the program.  The ONLY
identifying information in the row is the `b` relocation -- and under
`functionRelocDiffs=name_check` objdiff FORGIVES a relocation whose TARGET
carries a placeholder name (`fn_`/`lbl_`/... -- see `is_placeholder_symbol_name`
in objdiff-core `diff/code.rs`).  So whenever retail's BODY has no
`target_symbol_map.json` entry, the one discriminating field is uncharged and the
row reads **fuzzy == 100 no matter which function our thunk actually branches
to**.  The 100 is then a statement about the MAP'S COVERAGE, not about our code.

Two lanes hit this independently (W16-F, W16-H §7.3) and one more (W16-I) hit the
same forgiveness mechanism one level up.  Nothing measured how many such rows
exist.  This does.

  * `tools/thunk_target_audit.py` iterates thunks whose target IS map-named --
    it can see a name DISAGREEMENT, and by construction never sees this cell.
  * `tools/unnamed_thunk_census.py` iterates thunks with NO name of their own.
  * This tool is the remaining cell: **named thunk, UNNAMED target.**

THE ADJUDICATION (retail bytes on one side, our COFF on the other)
------------------------------------------------------------------
For each candidate row R at VA with map name N:

  OURS    the COMDAT defining N in `build/45410914/**/*.obj`; its single code
          relocation names F_ours -- the function OUR thunk tail-calls.
  RETAIL  decode the `b` at VA to T.  Chase T while it too decodes as a thunk
          (terminating predicate, not a depth limit -- see `chase`).  If the
          chain end T* is map-named we have N_ret directly; otherwise compare
          our compiled body for F_ours against retail's bytes at T*.

  TRUE       the two targets provably denote the same function
  FALSE_100  they provably differ  (name disagreement / size / masked bytes)
  UNDECIDED  not enough evidence -- reported, never silently bucketed as TRUE

⚠ ANTI-VACUITY.  A masked byte compare over a tiny body proves nothing: a 4-byte
`b` thunk is masked-identical to every other one in the image (the
`thunk_dc3_crosscheck` finding).  A byte verdict is therefore only issued when
the body carries `MIN_INFO_WORDS` words that survive masking; otherwise the row
is UNDECIDED.  A verdict this tool cannot justify is not a verdict.

    python3 tools/thunk_false100_census.py --selftest
    python3 tools/thunk_false100_census.py [--csv out.csv]
"""
import argparse
import glob
import json
import os
import pickle
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)

import pdata_extent as P                      # noqa: E402  (validated .pdata decoder)
from fold_thunk_gate import mask_word         # noqa: E402  (validated instruction mask)
from comdat_bytes import comdats              # noqa: E402

# A byte verdict needs this many post-mask-informative words, or it is UNDECIDED.
MIN_INFO_WORDS = 4

ADJ_LOAD = {0x8163FFFC: 3, 0x8164FFFC: 4}
ADJ_SUBF = {3: 0x7C6B1850, 4: 0x7C8B2050}


def u32(va):
    b = P.rd(va, 4)
    return None if len(b) < 4 else int.from_bytes(b, 'big')


def decode_thunk(va):
    """(kind, target, nbytes) for the thunk at `va`, else None.

    Three forms, all of which appear in retail:
      vtordisp   lwz r11,-4(rN); subf rN,r11,rN; [addi rN,rN,-M;] b BODY
      static_adj addi rN,rN,-M; b BODY            (non-virtual base adjustor)
      bare_b     b BODY                           (pure trampoline)
    The optional `addi` is matched by SHAPE (opcode 14, RT==RA==this-reg), never
    by a constant word -- M varies per class and a constant rebuilds the
    28.2% blind spot lane SLOTMAP measured in the 3-instruction-only decoder.
    """
    w0 = u32(va)
    if w0 is None:
        return None
    reg = ADJ_LOAD.get(w0)
    if reg is not None and u32(va + 4) == ADJ_SUBF[reg]:
        at = va + 8
        w = u32(at)
        if w is None:
            return None
        if (w >> 26) == 14 and ((w >> 21) & 31) == reg and ((w >> 16) & 31) == reg:
            at += 4
            w = u32(at)
            if w is None:
                return None
        if (w >> 26) != 18 or (w & 1):
            return None
        off = w & 0x03FFFFFC
        if off & 0x02000000:
            off -= 0x04000000
        return ('vtordisp', (at + off) & 0xFFFFFFFF, at + 4 - va)
    if (w0 >> 26) == 14 and ((w0 >> 21) & 31) == ((w0 >> 16) & 31):
        w = u32(va + 4)
        if w is not None and (w >> 26) == 18 and not (w & 1):
            off = w & 0x03FFFFFC
            if off & 0x02000000:
                off -= 0x04000000
            return ('static_adj', (va + 4 + off) & 0xFFFFFFFF, 8)
    if (w0 >> 26) == 18 and not (w0 & 1):
        off = w0 & 0x03FFFFFC
        if off & 0x02000000:
            off -= 0x04000000
        return ('bare_b', (va + off) & 0xFFFFFFFF, 4)
    return None


def real_body(va):
    """True when `va` is a .pdata BeginAddress with a body longer than a thunk.

    This is the TERMINATION TEST for the chase.  `thunk_identity.py` records
    that a recursive follower with only a depth limit walks INTO a real body and
    converges 26 of 29 rows onto one trampoline region; the fix is a predicate
    that says "stop, this is a function", which is what this is.
    """
    e = P.pdata_extent(va)
    return bool(e and e[0] == va and e[1] > 16)


def chase(va, mapaddr, limit=8):
    """Follow a thunk chain to its first real body or first map-named address.

    Returns (end_va, hops, chain).  Stops on: a map-named destination, a real
    .pdata body, or a non-thunk word.  `limit` is a backstop against a cycle,
    NOT the stopping rule.
    """
    chain = []
    cur = va
    for _ in range(limit):
        if cur in mapaddr or real_body(cur):
            break
        d = decode_thunk(cur)
        if d is None:
            break
        chain.append((cur, d[0], d[1]))
        cur = d[1]
    return cur, len(chain), chain


def our_index(root=ROOT, cache=None):
    """{mangled name: [(obj path, size, relocs, raw bytes)]} over our compiled objs."""
    if cache and os.path.exists(cache):
        with open(cache, 'rb') as f:
            return pickle.load(f)
    idx = {}
    for p in glob.glob(os.path.join(root, 'build/45410914/src/**/*.obj'), recursive=True):
        try:
            cs = comdats(p)
        except Exception:
            continue
        for k, v in cs.items():
            if v['is_code']:
                idx.setdefault(k, []).append((p, v['size'], v['relocs'], v['raw']))
    if cache:
        with open(cache, 'wb') as f:
            pickle.dump(idx, f)
    return idx


def masked_words(buf):
    return [mask_word(int.from_bytes(buf[i:i + 4], 'big')) for i in range(0, len(buf) - 3, 4)]


def info_words(words):
    """Words that still carry identity after masking.

    A masked `b` collapses to its opcode, and a masked D-form to its opcode and
    registers; those are the words that make a small thunk compare equal to every
    other thunk.  Counting them is what keeps a byte verdict from being vacuous.
    """
    return sum(1 for w in words if (w >> 26) not in (18, 16))


def compare_bodies(our_raw, retail_bytes):
    """('SAME'|'DIFF'|'SIZE'|'WEAK', detail) -- masked word-wise body comparison."""
    if len(our_raw) != len(retail_bytes):
        return 'SIZE', f'size {len(our_raw)} vs {len(retail_bytes)}'
    a, b = masked_words(our_raw), masked_words(retail_bytes)
    ndiff = sum(1 for x, y in zip(a, b) if x != y)
    if ndiff:
        return 'DIFF', f'{ndiff}/{len(a)} masked words differ'
    if info_words(a) < MIN_INFO_WORDS:
        return 'WEAK', f'only {info_words(a)} informative words after masking'
    return 'SAME', f'{len(a)}/{len(a)} masked words equal, {info_words(a)} informative'


# MemFree is the allocator every MSVC scalar deleting destructor tail-calls.
MEMFREE_NAMES = {'?MemFree@@YAXPAX@Z', '?_MemFree@@YAXPAX@Z'}
DTOR_PREFIXES = ('??_E', '??_G', '??1', '??_D')
SHAPE_ENABLED = True          # flipped by the selftest's sabotage leg


def is_bit0_test(w):
    """`clrlwi. rA,rS,31` == rlwinm. rA,rS,0,31,31 -- the `flag & 1` of a
    scalar deleting destructor.  Matched by FIELDS (op 21, SH=0, MB=ME=31,
    Rc=1), never by a constant word, so the source/dest registers are free."""
    return ((w >> 26) == 21 and (w & 1) and ((w >> 11) & 31) == 0
            and ((w >> 6) & 31) == 31 and ((w >> 1) & 31) == 31)


def retail_is_deleting_dtor(va, flen, mapaddr, rd=None):
    """True iff retail's body at `va` is a scalar deleting destructor.

    Signature: a bit-0 test of the incoming flag AND a `bl` reaching a
    map-named MemFree.  Both are required -- a lone `bl MemFree` appears in
    ordinary cleanup code, and a lone bit-0 test in ordinary logic, so either
    one alone would over-fire.
    """
    if not SHAPE_ENABLED:
        return False, 'shape detector disabled (sabotage leg)'
    rd = rd or P.rd
    buf = rd(va, flen)
    saw_bit0 = False
    freed = None
    for i in range(0, len(buf) - 3, 4):
        w = int.from_bytes(buf[i:i + 4], 'big')
        if is_bit0_test(w):
            saw_bit0 = True
        if (w >> 26) == 18 and (w & 1):            # bl
            d = w & 0x03FFFFFC
            if d & 0x02000000:
                d -= 0x04000000
            t = (va + i + d) & 0xFFFFFFFF
            if mapaddr.get(t) in MEMFREE_NAMES:
                freed = t
    if saw_bit0 and freed is not None:
        return True, f'bit-0 test + bl 0x{freed:08x} (MemFree)'
    return False, f'bit0={saw_bit0} memfree={freed}'


def prefix(sym):
    """`?Name@Class` -- the identity part, with ??_E (vector dtor thunk) folded
    onto ??_G (scalar dtor), which is the legitimate forwarding relation."""
    if not sym:
        return None
    i = sym.find('@@')
    p = sym[:i] if i > 0 else sym
    if p.startswith('??_E'):
        p = '??_G' + p[4:]
    return p


def strip_thunk_token(sym):
    """Drop the adjustor token so a thunk name and its body name compare."""
    for tok in ('$4', '$2', '$0', '$B', '$3', '$1'):
        i = sym.find(tok)
        if i > 0:
            return prefix(sym[:i] + '@@')
    return prefix(sym)


def describe_retail(va, mapaddr, rd=None, _depth=0):
    """What the retail address `va` DENOTES, as a comparable descriptor.

    ('ADJ', disp, final_name_or_va)  an adjustor: `addi rN,rN,-disp; b F`
    ('BODY', size)                   a real function (a .pdata BeginAddress)
    ('TRAMP', final)                 a bare `b` -- pure forwarding, no adjustment
    ('UNKNOWN', None)

    ⛔ THE CHASE RULE, and why the first draft of this tool got 4 verdicts WRONG.
    An 8-byte adjustor has NO .pdata record (CD-7's leaf-stub scope bound), so a
    "keep going until it stops looking like a thunk" walker steps straight THROUGH
    it -- and that adjustor IS the function the map row names.  Chasing turned
    `?Copy@UnisonIcon@@$4...` into a RndDir::Copy "disagreement" when our own obj
    defines `?Copy@UnisonIcon@@UAAX...` as byte-identical to retail's adjustor,
    displacement and all.  An adjustor CHANGES `this`, so it is a distinct
    function and must terminate the walk; only a bare `b` (identity) may be
    followed.  This is `thunk_identity.py` v1's trap, met from a new direction.
    """
    rd = rd or P.rd
    w0 = u32(va)
    if w0 is None:
        return ('UNKNOWN', None, None)
    if (w0 >> 26) == 14 and ((w0 >> 21) & 31) == ((w0 >> 16) & 31):
        w = u32(va + 4)
        if w is not None and (w >> 26) == 18 and not (w & 1):
            imm = w0 & 0xFFFF
            if imm & 0x8000:
                imm -= 0x10000
            off = w & 0x03FFFFFC
            if off & 0x02000000:
                off -= 0x04000000
            t = (va + 4 + off) & 0xFFFFFFFF
            return ('ADJ', -imm, mapaddr.get(t, t))
    if (w0 >> 26) == 18 and not (w0 & 1) and _depth < 4:
        off = w0 & 0x03FFFFFC
        if off & 0x02000000:
            off -= 0x04000000
        t = (va + off) & 0xFFFFFFFF
        if t in mapaddr:
            return ('TRAMP', 0, mapaddr[t])
        return describe_retail(t, mapaddr, rd, _depth + 1)
    e = P.pdata_extent(va)
    if e and e[0] == va:
        return ('BODY', e[1], None)
    return ('UNKNOWN', None, None)


def describe_ours(name, our_idx):
    """The same descriptor for one of OUR compiled COMDATs, read from COFF."""
    ent = our_idx.get(name)
    if not ent:
        return ('ABSENT', None, None)
    _, size, relocs, raw = ent[0]
    ws = [int.from_bytes(raw[i:i + 4], 'big') for i in range(0, len(raw) - 3, 4)]
    tgt = None
    for o, n, _t in relocs:
        if n and not n.startswith('$'):
            tgt = n
    if len(ws) == 2 and (ws[0] >> 26) == 14 and ((ws[0] >> 21) & 31) == ((ws[0] >> 16) & 31) \
            and (ws[1] >> 26) == 18:
        imm = ws[0] & 0xFFFF
        if imm & 0x8000:
            imm -= 0x10000
        return ('ADJ', -imm, tgt)
    if len(ws) == 1 and (ws[0] >> 26) == 18:
        return ('TRAMP', 0, tgt)
    return ('BODY', size, None)


def classify(row_name, our_entry, target_va, our_idx, mapaddr, retail_reader=None):
    """Adjudicate ONE candidate on the IMMEDIATE branch target of the thunk.

    `FALSE_100` means *provably a different function*, a higher bar than "the
    bodies differ": our body may be an unwritten stub or an imperfect port of
    the RIGHT function.  Those land in UNDECIDED with the reason attached.
    """
    rd = retail_reader or P.rd
    if our_entry is None:
        return 'UNDECIDED', 'NO_OUR_THUNK', 'our objs define no COMDAT for this name'
    _, _, relocs, _ = our_entry
    code_rel = [r for r in relocs if r[1] and not r[1].startswith('$')]
    if not code_rel:
        return 'UNDECIDED', 'NO_OUR_RELOC', 'our thunk COMDAT carries no target relocation'
    f_ours = code_rel[-1][1]

    rk, rv, rt = describe_retail(target_va, mapaddr, rd)
    ok_, ov, ot = describe_ours(f_ours, our_idx)
    shape = f'retail 0x{target_va:08x}={rk}{"" if rv is None else "/" + str(rv)}' \
            f'{"" if rt is None else "->" + str(rt)}; ours {f_ours} = {ok_}' \
            f'{"" if ov is None else "/" + str(ov)}{"" if ot is None else "->" + str(ot)}'

    # (a) both are adjustors: displacement AND final target must agree
    if rk == 'ADJ' and ok_ == 'ADJ':
        if ov != rv:
            return 'FALSE_100', 'ADJ_DISP_DISAGREE', shape
        if isinstance(rt, str) and ot:
            if strip_thunk_token(rt) == strip_thunk_token(ot):
                return 'TRUE', 'ADJ_AGREE', shape
            return 'FALSE_100', 'ADJ_TARGET_DISAGREE', shape
        # Retail's FINAL target is itself unnamed, so the names cannot be
        # compared.  That is MISSING EVIDENCE, not a disagreement -- the first
        # draft bucketed these FALSE_100 while their displacements agreed
        # exactly.  Resolve the final on retail BYTES instead: if our compiled
        # body for the name OUR adjustor reaches is masked-identical to retail's
        # bytes at the address RETAIL's adjustor reaches, the two adjustors
        # denote the same function.
        if isinstance(rt, int) and ot:
            e2 = P.pdata_extent(rt)
            oe = our_idx.get(ot)
            if e2 and e2[0] == rt and oe:
                v2, d2 = compare_bodies(oe[0][3], rd(rt, e2[1]))
                shape2 = shape + f' | final 0x{rt:08x}[{e2[1]}] vs {ot}: {d2}'
                if v2 == 'SAME':
                    return 'TRUE', 'ADJ_AGREE_BYTES', shape2
                if v2 == 'DIFF':
                    return 'FALSE_100', 'ADJ_FINAL_BYTES_DISAGREE', shape2
                return 'UNDECIDED', 'ADJ_FINAL_UNRESOLVED', shape2
        return 'UNDECIDED', 'ADJ_FINAL_UNNAMED', shape
    # (b) an adjustor is not a body, in either direction
    if rk == 'ADJ' and ok_ in ('BODY', 'TRAMP'):
        return 'FALSE_100', 'OURS_BODY_RETAIL_ADJ', shape
    if ok_ == 'ADJ' and rk == 'BODY':
        return 'FALSE_100', 'OURS_ADJ_RETAIL_BODY', shape
    # (c) both forward through identity trampolines
    if rk == 'TRAMP' and ok_ == 'TRAMP':
        if isinstance(rt, str) and ot and strip_thunk_token(rt) == strip_thunk_token(ot):
            return 'TRUE', 'TRAMP_AGREE', shape
        return 'FALSE_100', 'TRAMP_DISAGREE', shape
    if rk == 'TRAMP' and isinstance(rt, str) and ok_ == 'BODY':
        if strip_thunk_token(rt) == strip_thunk_token(f_ours):
            return 'TRUE', 'NAME_AGREE', shape
        return 'FALSE_100', 'NAME_DISAGREE', shape

    if rk != 'BODY':
        return 'UNDECIDED', 'NO_RETAIL_EXTENT', shape
    flen = rv
    # (d) retail-side shape: a deleting destructor cannot be a Save/Replace/...
    isdtor, why = retail_is_deleting_dtor(target_va, flen, mapaddr, rd)
    if isdtor and not row_name.startswith(DTOR_PREFIXES):
        return ('FALSE_100', 'RETAIL_IS_DELETING_DTOR',
                f'retail 0x{target_va:08x}[{flen}] is a scalar deleting dtor ({why}); '
                f'row is spelled {row_name.split("@@")[0]}; ours -> {f_ours}')
    if ok_ == 'ABSENT':
        return 'UNDECIDED', 'NO_OUR_BODY', shape
    ours = our_idx.get(f_ours)
    our_raw = ours[0][3]
    if len(our_raw) <= 8:
        return 'UNDECIDED', 'OUR_BODY_STUB', shape + ' (unwritten stub)'
    verdict, detail = compare_bodies(our_raw, rd(target_va, flen))
    detail = f'{f_ours} vs retail 0x{target_va:08x}[{flen}]: {detail}'
    if verdict == 'SAME':
        return 'TRUE', 'BYTES_AGREE', detail
    if verdict == 'WEAK':
        return 'UNDECIDED', 'BYTES_UNINFORMATIVE', detail
    if verdict == 'SIZE':
        return 'UNDECIDED', 'SIZE_DIFFERS', detail + ' (wrong name OR imperfect port)'
    return 'UNDECIDED', 'BYTES_DIFFER', detail + ' (wrong name OR imperfect port)'


def load_rows(root=ROOT):
    rep = json.load(open(os.path.join(root, 'build/45410914/report.json')))
    rows = {}
    for u in rep.get('units', []):
        for f in u.get('functions', []):
            n = f.get('name')
            if n:
                rows.setdefault(n, []).append(
                    (u['name'], float(f.get('fuzzy_match_percent', 0) or 0),
                     int(f.get('size', 0) or 0)))
    return rows


def census(root=ROOT, cache=None):
    mp = json.load(open(os.path.join(root, 'scripts/target_symbol_map.json')))
    mapaddr = {int(k, 16): v for k, v in mp.items() if k.startswith('0x') and v}
    rows = load_rows(root)
    idx = our_index(root, cache)
    out = []
    for va, nm in sorted(mapaddr.items()):
        d = decode_thunk(va)
        if d is None:
            continue
        kind, tgt, nbytes = d
        if tgt in mapaddr:
            continue                      # thunk_target_audit.py's population
        rr = rows.get(nm)
        if not rr or not any(x[1] == 100.0 for x in rr):
            continue                      # not a FALSE 100 if it is not a 100
        ent = (idx.get(nm) or [None])[0]
        bucket, ev, detail = classify(nm, ent, tgt, idx, mapaddr)
        out.append(dict(va=va, name=nm, kind=kind, size=nbytes, target=tgt,
                        unit=rr[0][0], row_size=rr[0][2],
                        bucket=bucket, evidence=ev, detail=detail))
    return out


# ----------------------------------------------------------------- selftest --
# Fixtures are pinned to RETAIL addresses (band.exe is immutable) and to our own
# mangled names, NOT to live map rows -- so fixing a map row in the tree cannot
# silently rot the test into agreeing with whatever the tree now says.
FIXTURES = [
    # W16-H §7.1: retail's thunk at 0x822AE7A0 reaches a scalar deleting dtor at
    # 0x822AF178 (80 B, `if (flag & 1) MemFree(this)`), while our thunk of the
    # same name tail-calls BandSwatch::Save.  Must read FALSE_100.
    dict(label='BandSwatch::Save (known FALSE-100)',
         row='?Save@BandSwatch@@$4PPPPPPPM@A@AAXAAVBinStream@@@Z',
         our_target='?Save@BandSwatch@@UAAXAAVBinStream@@@Z',
         target_va=0x822AF178, expect='FALSE_100'),
    # A TRUE control from the opposite bucket, pinned the same way: retail's
    # 0x822d2bb0 is an 8 B adjustor `addi r3,r3,-0x20; b RndDir::Copy`, and our
    # ?Copy@UnisonIcon@@UAAX... is byte-identical to it, displacement and all.
    # The first draft called this FALSE_100 by chasing through it (see
    # describe_retail); it is pinned here so that regression cannot return.
    dict(label='UnisonIcon::Copy (known TRUE)',
         row='?Copy@UnisonIcon@@$4PPPPPPPM@A@AAXPBVObject@Hmx@@W4CopyType@23@@Z',
         our_target='?Copy@UnisonIcon@@UAAXPBVObject@Hmx@@W4CopyType@23@@Z',
         target_va=0x822D2BB0, expect='TRUE'),
    # A control from the opposite bucket, pinned the same way (filled by
    # --emit-fixture; see docs/decomp/THUNK_FALSE100_CENSUS_2026-09-14.md).
]


def MAPADDR(root=ROOT):
    mp = json.load(open(os.path.join(root, 'scripts/target_symbol_map.json')))
    return {int(k, 16): v for k, v in mp.items() if k.startswith('0x') and v}


def selftest(root=ROOT, cache=None):
    """Run the classifier over pinned fixtures, THEN sabotage each evidence
    channel in turn and require the corresponding verdict to flip.

    Fixtures are pinned to RETAIL addresses (band.exe is immutable) and to our
    own mangled names, NOT to live map rows -- so repairing a map row in the
    tree cannot rot the test into agreeing with whatever the tree now says.

    A test that cannot fail proves nothing.  The sabotage legs are what
    demonstrate this one can: each disables exactly the channel that decided one
    fixture, and the run FAILS if the verdict survives the sabotage.
    """
    global SHAPE_ENABLED
    idx = our_index(root, cache)
    mapaddr = MAPADDR(root)
    ok = True

    def run(fx):
        ent = (idx.get(fx['row']) or [None])[0]
        if ent is None:
            return None, 'NO_OUR_THUNK', f"our objs define no {fx['row']}"
        return classify(fx['row'], ent, fx['target_va'], idx, mapaddr)

    for fx in FIXTURES:
        got, ev, detail = run(fx)
        good = got == fx['expect']
        ok &= good
        print(f"  {fx['label']:40s} {str(got):10s} ({ev}) expect {fx['expect']:10s}"
              f" {'OK' if good else 'FAIL'}")
        print(f"      {detail}")

    # --- sabotage leg 1: the retail-shape detector (decides the FALSE fixture)
    fx = FIXTURES[0]
    real = SHAPE_ENABLED
    try:
        SHAPE_ENABLED = False
        got, ev, _ = run(fx)
        flipped = got != fx['expect']
        ok &= flipped
        print(f"  sabotage: disable dtor-shape detector      {str(got):10s} ({ev}) "
              f"must NOT be {fx['expect']}  {'OK' if flipped else 'FAIL (VACUOUS)'}")
    finally:
        SHAPE_ENABLED = real

    # --- sabotage leg 2: the descriptor comparison (decides the TRUE fixture)
    fx = FIXTURES[1]
    real_desc = globals()['describe_ours']
    try:
        globals()['describe_ours'] = lambda name, our_idx: ('BODY', 999, None)
        got, ev, _ = run(fx)
        flipped = got != fx['expect']
        ok &= flipped
        print(f"  sabotage: blind the descriptor comparison  {str(got):10s} ({ev}) "
              f"must NOT be {fx['expect']}  {'OK' if flipped else 'FAIL (VACUOUS)'}")
    finally:
        globals()['describe_ours'] = real_desc

    print('SELFTEST', 'PASS' if ok else 'FAIL')
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--selftest', action='store_true')
    ap.add_argument('--root', default=ROOT)
    ap.add_argument('--cache', default=None, help='pickle cache for the obj index')
    ap.add_argument('--csv')
    ap.add_argument('--bucket', help='only print this bucket')
    a = ap.parse_args()
    if a.selftest:
        raise SystemExit(0 if selftest(a.root, a.cache) else 1)
    rec = census(a.root, a.cache)
    agg = {}
    for r in rec:
        k = (r['bucket'], r['evidence'])
        agg.setdefault(k, [0, 0])
        agg[k][0] += 1
        agg[k][1] += r['row_size']
    print(f'{len(rec)} named thunk rows at fuzzy==100 whose retail target is UNNAMED\n')
    print(f"{'bucket':12s} {'evidence':22s} {'rows':>6s} {'bytes':>8s}")
    for k in sorted(agg):
        print(f'{k[0]:12s} {k[1]:22s} {agg[k][0]:6d} {agg[k][1]:8d}')
    tot = {}
    for r in rec:
        tot.setdefault(r['bucket'], [0, 0])
        tot[r['bucket']][0] += 1
        tot[r['bucket']][1] += r['row_size']
    print()
    for k in sorted(tot):
        print(f'  TOTAL {k:12s} {tot[k][0]:6d} rows {tot[k][1]:8d} B')
    for r in rec:
        if a.bucket and r['bucket'] != a.bucket:
            continue
        if a.bucket:
            print(f"\n0x{r['va']:08x} {r['name']}\n    unit={r['unit']} kind={r['kind']} "
                  f"-> 0x{r['target']:08x}\n"
                  f"    {r['bucket']} [{r['evidence']}] {r['detail']}")
    if a.csv:
        import csv
        with open(a.csv, 'w', newline='') as f:
            w = csv.DictWriter(f, fieldnames=list(rec[0].keys()))
            w.writeheader()
            for r in rec:
                w.writerow(r)
        print(f'\nwrote {a.csv}')


if __name__ == '__main__':
    main()
