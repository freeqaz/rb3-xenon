#!/usr/bin/env python3
"""lane EB-1: emit a carve spec, re-asserting ALL THREE witnesses at spec time.

Witnesses (the real predictor, validated 26/26 against EA-1's landed carves):
  W1  retail .pdata has NO exact record at the symbol   -- where .pdata speaks it
      says the pin is right, and 11 of 21 candidates die here
  W2  the claimed extent's last instruction is NOT a terminator, so control would
      fall off the end: provable truncation.  An extent that DOES end in a
      terminator is self-consistent and its size gap is our code being long.
  W3  our compiled body is longer, ends at a real exit, and every symbol strictly
      inside the new extent is an absorbable anon fragment.

Nothing is transcribed: sizes come from the obj COMDAT table and the pin from
symbols.txt, both re-read here.
"""
import json, sys, os, re, bisect
import adj
import sweep_base2

WT = adj.WT
BASE2 = sweep_base2.BASE2
MAP = json.load(open(os.path.join(WT, 'scripts/target_symbol_map.json')))

spans, cur = [], None
for line in open(os.path.join(WT, 'config/45410914/splits.txt')):
    if line.strip() and not line[0].isspace() and line.rstrip().endswith(':'):
        cur = line.strip().rstrip(':')
    else:
        m = re.search(r'\.text\s+start:0x([0-9A-Fa-f]+) end:0x([0-9A-Fa-f]+)', line)
        if m:
            spans.append((int(m.group(1), 16), int(m.group(2), 16), cur))
spans.sort()
SP = [s[0] for s in spans]
ABSORB = re.compile(r'^(fn_[0-9A-Fa-f]{8}|except_data_[0-9A-Fa-f]{8})$')

ALLOW_CROSS = set(int(x, 16) for x in os.environ.get('EB1_ALLOW_CROSS', '').split() if x)

spec = []
for hx in sys.argv[2:]:
    a = int(hx, 16)
    s = adj.sym_at(a)
    assert s, f"{hx}: no symbol"
    _, typ, name, claim = s
    mn = MAP.get('0x%08x' % a)
    assert mn, f"{hx}: not in target_symbol_map"

    # W1
    pe = adj.pdata_exact(a)
    assert pe is None, (f"{mn}: retail .pdata HAS an exact record (end 0x{pe[0]+pe[2]:08X}); "
                        "where .pdata speaks the pin is right and OUR BODY is long")

    # W2 -- padding-trimmed last instruction of the CLAIMED extent
    eff = claim
    while eff >= 8 and adj.word(a + eff - 4) == 0:
        eff -= 4
    lt = adj.raw_exit_kind(adj.word(a + eff - 4), a + eff - 4, a, a + eff)
    assert lt is None, (f"{mn}: claimed extent ends in a terminator ({lt}) -- it is "
                        "self-consistent, so the size gap is our code being LONG, not truncation")

    # W3
    bs = BASE2.get(mn)
    assert bs is not None, f"{mn}: no unambiguous compiled size"
    assert bs > claim, f"{mn}: compiled body 0x{bs:X} not longer than pin 0x{claim:X}"
    end = a + bs
    k = adj.raw_exit_kind(adj.word(end - 4), end - 4, a, end)
    assert k, f"{mn}: word at new end-4 is not an exit"
    inner = adj.syms_in(a + 4, end)
    bad = [x[2] for x in inner if not ABSORB.match(x[2])]
    assert not bad, f"{mn}: NAMED symbols inside the new extent: {bad}"

    i = bisect.bisect_right(SP, a) - 1
    ss, se, su = spans[i]
    cross = end > se
    assert (not cross) or a in ALLOW_CROSS, (
        f"{mn}: new end 0x{end:08X} crosses its splits block end 0x{se:08X} "
        f"({su}); needs a coupled splits.txt edit -- set EB1_ALLOW_CROSS")

    spec.append({'addr': '0x%08X' % a, 'size': '0x%X' % bs,
                 'label': f'{su}::{mn}', 'unit': su, 'name': mn,
                 'old_size': '0x%X' % claim, 'base_size': '0x%X' % bs,
                 'new_end': '0x%08X' % end, 'exit_kind': k,
                 'block': ['0x%08X' % ss, '0x%08X' % se], 'cross': cross,
                 'absorbed': ['%s@0x%08X' % (x[2], x[0]) for x in inner]})

json.dump(spec, open(sys.argv[1], 'w'), indent=1)
print(f"wrote {len(spec)} carves to {sys.argv[1]}")
for c in spec:
    print(f"  {c['addr']} {c['old_size']:>6} -> {c['size']:>6} end={c['new_end']} "
          f"({c['exit_kind']}) cross={c['cross']}  {c['label'][:60]}")
    print(f"        absorbs: {', '.join(c['absorbed']) or '(none)'}")
