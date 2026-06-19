# W8 adversarial honesty audit — `fsss-residual-and-getid-pinext`

- **Date:** 2026-06-19
- **Auditor mode:** ADVERSARIAL HONESTY AUDITOR (Opus), read-only on main.
- **Candidate:** kind=hashmap-convert (actually a pin-extend), reported LANDABLE at net +1.
- **Candidate worktree:** `/home/free/code/milohax/wt-w8-fsss-residual-and-getid-pinext` @ `c9c4979`
- **Baseline (main `da8258f`):** 8234 `measures.matched_functions`.
- **VERDICT: HONEST. Land it.** own_count=1, foreign_count=0, longest_foreign_run=0.

## What the candidate changed (the whole diff)

Two lines, no body change:

1. `config/45410914/splits.txt` — FixedSizeSaveableStream.cpp:
   - `.text  start:0x82786468 → 0x82786420` (front-extend by one function)
   - `.pdata start:0x82237FB8 → 0x82237FB0` (front-extend by one 8-byte RUNTIME_FUNCTION)
   - `.text end:0x82786788` and `.pdata end:0x82238008` UNCHANGED.
2. `scripts/target_symbol_map.json` — adds
   `"0x82786420": "?GetID@FixedSizeSaveableStream@@QBAHVSymbol@@@Z"`.

So exactly ONE new function is brought into the count: the one at VA `0x82786420`.

## Ground-truth ownership of `0x82786420`

The newly-pinned fn is the **own** method
`int FixedSizeSaveableStream::GetID(Symbol) const`. Proven three independent ways:

1. **Source ↔ mangled-name match.** `src/system/meta/FixedSizeSaveableStream.cpp:19`
   defines `int FixedSizeSaveableStream::GetID(Symbol s) const`. Mangled
   `?GetID@FixedSizeSaveableStream@@QBAHVSymbol@@@Z` demangles to
   `public: int __thiscall FixedSizeSaveableStream::GetID(class Symbol) const` —
   byte-identical to the added map entry.

2. **objdiff 100% normalized (99.7% raw), 72 bytes, 18/18 equal.** The body is
   exactly the source logic: `addi r4, r3, 0x30` (m_mapSymbolToID at this+0x30),
   `bl` into the `_M_find` hashtable instantiation, branch on found → return
   `it->second` (`lwz r3, 0x8, r11`) else `li r3, -0x1`. The 0.3% raw gap is the
   relocated `bl` target, which report-normalized correctly equates. Confirmed
   report-normalized >= 100 (the function lands in the count, see below).

3. **DC3 `ham_xbox_r.map` confirms own-TU, NOT a foreign ICF fold.** The DC3 map
   has a contiguous `meta:FixedSizeSaveableStream.obj` TU whose method run is
   `...GetSymbolToIDMap / SaveTable / GetSymbolTableSize / GetID / GetSymbol /
   ~FixedSizeSaveableStream...`, with:
   `0005:002154f8  ?GetID@FixedSizeSaveableStream@@QBAHVSymbol@@@Z  825454f8 f
   meta:FixedSizeSaveableStream.obj`.
   GetID is unambiguously one of FixedSizeSaveableStream's own contiguous
   instantiations. This is the *inverse* of the Waypoint trap risk (no foreign
   COMDAT fold attributed by VA — the VA belongs to the real owner).

The auto_03 COFF dump shows `0x82786420` as anonymous `fn_82786420` (retail has
no names — expected), bracketed by the other already-pinned FSSS anon fns
(`fn_82786468 = GetSymbol`, `fn_82786500 = HasID`, `fn_82786550 = ctor`, ...).
All 14 fns in `[0x82786420, 0x82786788)` are FSSS's own; the candidate added only
the first.

## pdata extension is clean

`.pdata start 0x82237FB8 → 0x82237FB0` adds exactly one 8-byte RUNTIME_FUNCTION =
GetID's unwind record. End unchanged (no gap-shrink). No other split claims
`[0x82237FB0, 0x82237FB8)` or `[0x82786420, 0x82786468)` (range-conflict scan
clean).

## Whole-binary A/B (authoritative)

`fresh_report.sh` in the worktree (re-run to clear the splits-only divergence FP):

- Run 1: `measures.matched_functions: 8235`
- Run 2: `measures.matched_functions: 8235` (stable)
- main baseline report.json: `8234`

Per-unit `matched_functions` diff (main → worktree), ALL units:

```
+1  8->9  default/FixedSizeSaveableStream
Net: 1
```

Exactly one unit moves, by +1, in the right direction. Zero cross-unit movement,
zero regression masking. The headline +1 equals the single intended unit gain.

## Honesty gate

- net = +1 (>= +1) ✓
- zero unexplained per-unit regressions ✓ (only FSSS +1, nothing else moves)
- no >= 8-contiguous FOREIGN fn_@0% run in the changed range ✓ (the one new fn is
  own; foreign run length = 0)
- headline net == sum of intended unit gains (+1 == +1) ✓
- not the Waypoint-inverse trap: the new fn is the real owner's own instantiation,
  proven by DC3 map + source signature + 100% body, NOT a foreign fold pinned by VA ✓

## AUDIT result

- key: `fsss-residual-and-getid-pinext`
- honest: **true**
- own_count: **1**
- foreign_count: **0**
- longest_foreign_run: **0**
- verdict: **LAND** — the +1 is a real own-TU match (FixedSizeSaveableStream::GetID),
  byte-exact, isolated, regression-free.
