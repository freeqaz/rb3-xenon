#!/usr/bin/env python3
"""Audit target_symbol_map.json rows against their ADDRESS NEIGHBOURHOOD.

Motivation (lane DJ-2d, 2026-08-03)
-----------------------------------
`??0FlowRun@@IAA@XZ` was mapped to 0x82618BA8.  Retail's function there
installs `CustomizePanel` vtables, is flanked in the map by
`?StoreFocusComponent@CustomizePanel@@` and `??_GCustomizePanel@@`, and
`CustomizePanel` has no constructor row anywhere in the map.  It is
`??0CustomizePanel@@...`, misnamed.  Same for FlowDistance->NextSongPanel and
FlowSound->CharacterCreatorPanel.

No source edit can ever fix such a row, and the function reads as a plausible
40-50% "nearly there" ctor, so it silently absorbs source-lane budget.

The detector
------------
MSVC emits a TU's COMDATs into a contiguous-ish address neighbourhood.  So for a
named row R at address A, look at the nearest named rows before and after A.  If
BOTH belong to one class C, and R does not, R is suspect.  Strengthen with:
  * C has no row of R's own symbol kind (e.g. no ctor at all)  -> C is missing
    exactly the row A would supply.

Controls (printed every run; the tool is useless without them)
--------------------------------------------------------------
POSITIVE: the three confirmed rows above.  It catches only 2 of 3 -- see the
          recall limit below.  DO NOT "fix" the test until all three pass; that
          is fitting the instrument to the known answer.
NEGATIVE: symbol/address pairing is shuffled and the identical test re-run.  The
          null flag rate bounds the false-positive rate.  Measured 2026-08-03:
          1081 treated (3.87%) vs 32 null (0.11%) = 33.8x enrichment.

KNOWN RECALL LIMIT -- read before trusting a negative
-----------------------------------------------------
The test assumes a TU's COMDATs are address-neighbours.  When the linker places
one COMDAT far from the rest of its TU, both neighbours belong to some third
class and the row is NOT flagged.  That is exactly what happens to
0x82574938 (NextSongPanel's ctor, ~0xCF000 away from the rest of NextSongPanel).
So a clean result here is NOT evidence a row is correct.

The complementary instrument, which does catch that case, is a retail RTTI
decode: read the Complete Object Locator at vtable[-1] for every vtable the
function installs and take the TypeDescriptor name.  RTTI is authoritative and
neighbour-independent; this tool is cheap and needs no binary parsing.  Use
both -- they failed on disjoint cases.

Read-only.  Prints candidates for human adjudication; edits nothing.
"""
import argparse, bisect, json, random, re, sys, collections

CLASS_RE = re.compile(r'@([A-Za-z_][A-Za-z0-9_]*)@@')


def class_of(sym):
    """Best-effort owning class of an MSVC mangled name."""
    if sym.startswith('??0') or sym.startswith('??1'):
        m = re.match(r'\?\?[01]([A-Za-z_][A-Za-z0-9_]*)@@', sym)
        if m:
            return m.group(1)
    if sym.startswith('??_G') or sym.startswith('??_E'):
        m = re.match(r'\?\?_[GE]([A-Za-z_][A-Za-z0-9_]*)@@', sym)
        if m:
            return m.group(1)
    m = re.match(r'\?[A-Za-z_][A-Za-z0-9_]*@([A-Za-z_][A-Za-z0-9_]*)@@', sym)
    if m:
        return m.group(1)
    return None


def kind_of(sym):
    if sym.startswith('??0'):
        return 'ctor'
    if sym.startswith('??1'):
        return 'dtor'
    if sym.startswith('??_G') or sym.startswith('??_E'):
        return 'deleting-dtor'
    return 'method'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--map', default='scripts/target_symbol_map.json')
    ap.add_argument('--seed', type=int, default=1234)
    ap.add_argument('--limit', type=int, default=60)
    a = ap.parse_args()

    raw = json.load(open(a.map))
    items = sorted((int(k, 16), v) for k, v in raw.items()
                   if re.fullmatch(r'0x[0-9a-fA-F]+', k) and isinstance(v, str))
    addrs = [x for x, _ in items]

    # every class -> set of symbol kinds it already has a row for
    have = collections.defaultdict(set)
    for _, v in items:
        c = class_of(v)
        if c:
            have[c].add(kind_of(v))

    def verdict(i):
        """Flag row i if both neighbours share one class that row i is not in."""
        if i == 0 or i == len(items) - 1:
            return None
        sym = items[i][1]
        own = class_of(sym)
        prev_c, next_c = class_of(items[i - 1][1]), class_of(items[i + 1][1])
        if not prev_c or prev_c != next_c or prev_c == own:
            return None
        return prev_c

    flagged = []
    for i in range(len(items)):
        c = verdict(i)
        if c:
            addr, sym = items[i]
            missing = kind_of(sym) not in have[c]
            flagged.append((addr, sym, c, missing))

    strong = [f for f in flagged if f[3]]

    # ---- NEGATIVE CONTROL: same test, randomly permuted symbol->address pairing
    rnd = random.Random(a.seed)
    syms = [v for _, v in items]
    rnd.shuffle(syms)
    shuffled = list(zip(addrs, syms))

    def verdict_null(i):
        if i == 0 or i == len(shuffled) - 1:
            return None
        own = class_of(shuffled[i][1])
        p, n = class_of(shuffled[i - 1][1]), class_of(shuffled[i + 1][1])
        if not p or p != n or p == own:
            return None
        return p

    null_flags = sum(1 for i in range(len(shuffled)) if verdict_null(i))

    n = len(items)
    print('named map rows            : %d' % n)
    print('FLAGGED (neighbour test)  : %d  (%.2f%%)' % (len(flagged), 100.0 * len(flagged) / n))
    print('  ...of which target class lacks that symbol kind entirely: %d  <== STRONG'
          % len(strong))
    print('NEGATIVE CONTROL (shuffled symbol/address pairing): %d  (%.2f%%)'
          % (null_flags, 100.0 * null_flags / n))
    if null_flags:
        print('  enrichment treated/null = %.1fx' % (len(flagged) / null_flags))
    print()

    ctrl = {0x82618ba8: 'CustomizePanel',
            0x82574938: 'NextSongPanel',
            0x82612300: 'CharacterCreatorPanel'}
    got = {addr: c for addr, _, c, _ in flagged}
    hit = sum(1 for k, v in ctrl.items() if got.get(k) == v)
    print('POSITIVE CONTROL (3 confirmed misnamed panel ctors): %d/3 caught'
          ' -- 2/3 is EXPECTED, see "KNOWN RECALL LIMIT" in the docstring' % hit)
    for k, v in ctrl.items():
        print('   %08X expect %-24s got %s' % (k, v, got.get(k, '<NOT FLAGGED - remote COMDAT>')))
    print()

    print('STRONG candidates (first %d), for human adjudication:' % a.limit)
    for addr, sym, c, _ in strong[:a.limit]:
        print('  %08X  %-58s  looks like %s' % (addr, sym[:58], c))
    return 0


if __name__ == '__main__':
    sys.exit(main())
