#!/usr/bin/env python3
"""Census of `total_code` denominator inflation: report rows that claim MORE
bytes than `config/45410914/symbols.txt` says the symbol is.

    python3 tools/denominator_census.py [project_dir]

Complements `tools/report_conservation.py`: that tool proves a pin moved MAP and
not MATCHES across two snapshots; this one measures, at a single commit, how much
of `total_code` is phantom.

RESULT AT bf7cadb0 (lane DENOM-1, 2026-08-17): 74 rows / 58,400 B / 0.5659%.
  * 17 rows / 58,044 B (99.4%)  -- jeff COMDAT DEAD SPACE, see below. NOT pin-fixable.
  *  57 rows /    356 B ( 0.6%) -- inter-function alignment padding billed to the
                                   preceding function. Real retail bytes. Not a defect.

★ ROOT CAUSE OF THE BIG CLASS -- IT IS NOT A PINNING PROBLEM
------------------------------------------------------------
1. jeff marks ~every global symbol COMDAT (`split.rs`) and extracts each into its
   own `.text$dup` section, then ZERO-FILLS the hole in the parent `.text`
   WITHOUT SHRINKING IT (`xex.rs`: "parent section bytes become dead space").
2. jeff DELIBERATELY RETAINS in the parent any function that is the site or the
   destination of a cross-region conditional branch (`bc`, primary opcode 16),
   because `bc` has only a 14-bit displacement and extraction could break it.
   The retained "functions" are typically mid-function branch-target fragments
   that merely acquired their own `fn_` symbol (e.g. `blt cr6, fn_82855878`).
3. COFF carries no symbol size, so objdiff's `infer_symbol_sizes` sizes a
   Function symbol as the distance to the next Function-OR-OBJECT symbol
   (LABEL / Unknown are SKIPPED -- which is why an interior `lbl_` does not
   truncate). Every function between the retained head and the next retained
   symbol has moved to `.text$dup`, so the head ABSORBS THE WHOLE ZERO-FILLED
   HOLE -- measured: `fn_82855878` bills 25,920 B and contains 39 nonzero bytes.

⇒ PINNING PROVABLY CANNOT FIX THIS. `default/Shader` and
  `default/xdk/.../xltconvert` are explicitly pinned in splits.txt and carry the
  two largest instances; 5 more named/pinned units appear in the residue. The
  "supply a boundary" remedy has already been applied to them and failed.
  The `auto_*` vs named split is NOT mechanistic -- both are the same defect.

★ THE FIX (jeff-side; VALIDATED EMPIRICALLY WITHOUT REBUILDING JEFF)
--------------------------------------------------------------------
In `jeff/src/util/xex.rs`, immediately after `data[start..end].fill(0);` in the
COMDAT-extraction loop, emit into the PARENT section at offset `start` a symbol
that is BOTH:
  (a) Object kind  -- COFF type 0x0000, storage class 2 (EXTERNAL). `object` maps
      class-EXTERNAL + non-function derived type to `SymbolKind::Data` ->
      objdiff `SymbolKind::Object`, which DOES stop size inference; and
  (b) named with an objdiff-HIDDEN prefix -- `except_data_` / `__unwind` /
      `__catch` (`objdiff-core/src/obj/read.rs`, the COFF `SymbolFlag::Hidden`
      block). Prefer adding a dedicated prefix there rather than reusing these.

BOTH are required, and each was measured separately on a patched copy of
`xltconvert.obj` against an unpatched control in the same mini objdiff project
(control reproduced the shipped per-unit numbers to the byte):
  control                       : 67 rows, total_code 63,744, fn_82A148DC 10,276
  (a) alone   -> truncates, but bytes MOVE to a new row: 68 rows, 63,740 (-4)
  (a)+(b)     -> 67 rows, total_code 53,484 (-10,260 EXACTLY), fn_82A148DC 16
Predicted whole-binary: total_code -58,044 B, matched_functions/matched_code/
masked_equal Δ0 (all 17 rows are mpn 0.0 and were never matched). code% rises
~0.20pp AS A CORRECTION, never as progress.

⛔ INSTRUMENTS THAT DO NOT WORK -- DO NOT REBUILD THEM
-------------------------------------------------------
* "true code == rawsz(.text)" per unit. Reads +136,060 B of inflation, dominated
  by `auto_04_82C4D000_BINK` where rawsz(.text)==0 and ALL code legitimately
  lives in `.text$dup`. The assumption fails whenever the parent is fully
  extracted. Use the symbols.txt join below instead.
* Searching for `.text$dup` bytes inside `.text` to prove duplication: 0 of 89
  match, because the parent copy is ZEROED. The duplication is of SIZE, not bytes.
* Deriving an address from a `fn_`/`lbl_` symbol NAME. Names lie (`lbl_829E7A58`
  actually sits at 0x82A15F90). Read the address from symbols.txt or the map.

JOIN NOTE: the pre-compile target-symbol renamer rewrites `fn_<addr>` to MSVC
mangled names, so a name-only join to symbols.txt SILENTLY MISSES ROWS (61 rows /
57,904 B instead of 74 / 58,400). Rows whose name is not in symbols.txt are
resolved through `scripts/target_symbol_map.json` (address -> mangled) and then
looked up BY ADDRESS.
"""
import collections
import json
import re
import sys

SYM_RE = re.compile(r'^([^\s=]+)\s*=\s*([^:]+):(0x[0-9A-Fa-f]+);(.*)$')


def load_symbols(root):
    by_name, by_addr = {}, {}
    with open(f'{root}/config/45410914/symbols.txt') as fh:
        for line in fh:
            m = SYM_RE.match(line.strip())
            if not m:
                continue
            name, sec, addr = m.group(1), m.group(2), int(m.group(3), 16)
            size = re.search(r'size:(0x[0-9A-Fa-f]+)', m.group(4))
            entry = (sec, addr, int(size.group(1), 16) if size else None)
            by_name[name] = entry
            if sec == '.text' and entry[2] is not None:
                by_addr.setdefault(addr, entry)
    return by_name, by_addr


def load_renamer_map(root):
    inv = collections.defaultdict(list)
    with open(f'{root}/scripts/target_symbol_map.json') as fh:
        for addr, name in json.load(fh).items():
            # skip metadata keys such as `_bijection_arbitrary` (value is a list)
            if isinstance(addr, str) and addr.startswith('0x') and isinstance(name, str):
                inv[name].append(int(addr, 16))
    return inv


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else '.'
    report = json.load(open(f'{root}/build/45410914/report.json'))
    by_name, by_addr = load_symbols(root)
    inv = load_renamer_map(root)

    # int()-coerce everything: several report.json numerics are JSON STRINGS.
    total_code = int(report['measures']['total_code'])
    total_functions = int(report['measures']['total_functions'])
    rows, summed = [], 0
    for unit in report['units']:
        for fn in unit.get('functions') or []:
            size = int(fn['size'])
            summed += size
            rows.append((unit['name'], fn['name'], size))
    # invariant: total_code is EXACTLY the sum of listed function sizes
    if summed != total_code or len(rows) != total_functions:
        sys.exit(f'REFUSING: invariant broken -- total_code={total_code} '
                 f'sum(rows)={summed} total_functions={total_functions} rows={len(rows)}')

    over, stats = [], collections.Counter()
    for unit, name, size in rows:
        entry = by_name.get(name)
        if entry is None or entry[2] is None:
            cands = inv.get(name)
            if cands and len(cands) == 1 and cands[0] in by_addr:
                entry = by_addr[cands[0]]
                stats['joined_via_map'] += 1
            else:
                stats['UNJOINED'] += 1
                continue
        else:
            stats['joined_via_name'] += 1
        if size > entry[2]:
            over.append((size - entry[2], unit, name, size, entry[2], entry[1]))

    over.sort(reverse=True)
    excess = sum(o[0] for o in over)
    print(f'join: {dict(stats)}')
    print(f'total_code {total_code} over {total_functions} rows')
    print(f'OVER-CLAIMING: {len(over)} rows / {excess} B = '
          f'{100.0 * excess / total_code:.4f}% of total_code')
    big = [o for o in over if o[0] >= 52]
    print(f'  BIG   (>=52 B, jeff COMDAT dead space): {len(big)} rows / '
          f'{sum(o[0] for o in big)} B')
    print(f'  SMALL ( <52 B, alignment padding)     : {len(over) - len(big)} rows / '
          f'{excess - sum(o[0] for o in big)} B')
    print('\nexcess   unit / row / claimed / true / addr')
    for d, unit, name, size, true, addr in over[:25]:
        print(f'{d:>8}  {unit[:54]:<54} {name[:32]:<32} {size:>7}/{true:<6} 0x{addr:08X}')


if __name__ == '__main__':
    main()
