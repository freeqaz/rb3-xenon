# base(TU0) → TU5 function map — migration keystone (P1)

Date: 2026-07-07 · Worktree: `.claude/worktrees/tu5-migrate` (branch `tu5-migrate`,
off main `a1312de`) · TU0 frozen at tag `target/tu0-frozen`
(`e589bf5bc7c80457ab87123c7e18c1adf65c6357`).

Full re-base of RB3-Xenon from **TU0 v0.0.0.1** (`band.exe`, 14,137,856 B) to
**TU5 v0.0.5.1** (`band_tu5.exe`, 14,363,648 B, dtk-extracted from the retail
`default_tu5.xex`, SHA1 `c5a17091cb44c0119424390a1738d161995e430e`). Both PEs read
**section-mapped** (`tools/tu5_va.load_sections`) — flat `0x3000+VA` is WRONG on
the TU5 "basic"-format image. TU0 `.text` VA `0x82260000`; TU5 `.text` VA
`0x82270000` (base shift +0x10000, then variable internal growth; TU5 `.text` is
+0x28568 larger).

## Method (`tools/tu5_map_build.py`)

Two stages, both reloc-normalized (mask `bl/bc` targets `&0xFC000003 / &0xFFFF0003`,
D-form imm16 `&0xFFFF0000`) so relocation/address drift is invisible to the match:

1. **Stage 1 — unique-skeleton anchors.** A TU0 function's masked body that occurs
   **exactly once** in the TU5 `.text` masked stream is a HIGH-confidence 1:1 remap
   (`body_identical`). Run over the FULL `symbols.txt` `.text` function list
   (65,357 funcs) → **22,132 unique anchors**.
2. **Stage 2 — contiguity co-walk.** Functions keep `.text` emission order across
   TU0→TU5, so from any anchor a neighbour's TU5 VA = `anchor_tu5 +
   (neighbour_base − anchor_base)`; the predicted slot's masked skeleton is
   **verified** before acceptance (safety net — a wrong guess is rejected, never
   mis-assigned). This walks through `fn_` funcs too, resolves clusters of
   identical-skeleton getters by position, and **breaks exactly at genuinely
   changed functions** (verify fails). Converges in one pass: **61,629 / 65,357
   (94.3%)** of ALL `.text` functions resolved.

Named universe = `scripts/target_symbol_map.json` (13,845 MSVC-mangled) ∩
`symbols.txt` sizes.

## Aggregate match (named functions)

| Bucket | Count | Notes |
|---|---:|---|
| Named symbols in target_symbol_map | 13,843 | incl. 548 non-function data syms |
| **Named `.text` functions** | **13,295** | denominator |
| **Matched (HIGH+MED)** | **12,817** | **96.4%** — clears the >95% target |
|   HIGH (skeleton-unique, body_identical) | 8,855 | |
|   MED (co-walk verified, body_identical) | 3,962 | |
| **Changed-set (AMBIG+MISS)** | **478** | sizes remaining effort |
|   AMBIG (skeleton **present** in TU5, position unpinned) | 397 | mostly small/dup getters; mechanical to place in P2 |
|   MISS (skeleton absent — **genuinely diverged body**) | 81 | the real re-derivation work |
| SKIP (data symbol, not a function) | 548 | vtables/strings mis-listed as named funcs |

Changed-set shape: 40 tiny(<0x20) + 293 small(<0x80) + 121 med + 24 large(≥0x200);
top classes VSymbol/Vector3/Transform/Environment* (template + math getters that
duplicate skeletons → AMBIG, not truly changed). **True migration cost is
~81 MISS bodies to re-derive**, plus mechanical placement of the 397 AMBIG.

## Independent validation

- **7 same-instrument patch anchors** (P0 skeleton-recovery ground truth):
  5 in-universe match **exactly** (ResolvePartWaitStates→0x825B6488,
  ProcessConfig→0x8276FA08, RecalcGemList→0x82794740, GameGemDB::Duplicate→
  0x827932C8, GameGemList::CopyFrom→0x8278E168); IsActive correctly **MISS**
  (body diverged ~56% — re-derive detour on TU5). GameGemDB::GetDiffList base
  `0x8276E010` is tagged `type:label` (not `type:function`) in base `symbols.txt`,
  so the function-list co-walk skipped it — but its verified TU5 leaf
  **0x827931C8** decodes byte-exact (`81630000 lwz r11,0(r3); slwi r10,r4,2`).
- **5-function spot-check** — every one mirrors its TU0 prologue mnemonics under
  the `tu5_va` decoder: MasterAudio::IsLoaded (0x82756D98→0x8277B6E8,
  `lwz/lwz/lwz/mtctr`), Object::StaticClassName (0x8226AC48→0x8227AE48,
  `mflr/bl/addi/stwu`), RecalcGemList (patch fn, `mflr/stw/std/stwu`),
  App::DrawRegular (0x82260018→0x82270018), Timer::SplitMs (0x82260170→0x82270188).

## Outputs (worktree-relative)

- `_tu5probe/tu5_migrate/base_to_tu5_map.json` — per-named-function records
  `{base_va, symbol, size, tu5_va, confidence, method, body_identical}` + meta.
- `_tu5probe/tu5_migrate/base_to_tu5_map.full.json` — `{base_va: tu5_va}` for ALL
  61,629 resolved `.text` functions (named + `fn_`). **P2–P5 consume this** to
  re-anchor `symbols.txt` (65k `fn_` VAs) and `splits.txt` (3,870 file spans).
- `_tu5probe/tu5_migrate/tu5_changed_worklist.json` — 478 `{symbol, base_va, size,
  why_unmatched, method}`.
- `_tu5probe/map.json` — compact aggregate checkpoint.
- `tools/tu5_map_build.py` — reproducible builder (~27s).

## Re-anchor readiness (P2–P5, NOT done here — read-only on decomp.db)

- **splits.txt** (VA-keyed, 3,870 spans): rewrite each `.text start/end` via
  `base_to_tu5_map.full.json`; file spans whose interior functions are all MISS
  need boundary re-derivation from TU5 dtk splits.
- **symbols.txt** (251k lines, VA-keyed): remap every `.text:0x…` VA through the
  full map; the 3,728 unresolved `fn_` (mostly changed/new TU5 code) regenerate
  from TU5 dtk disassembly.
- **decomp.db** (name-keyed PK `symbol` — PORTABLE, survives the re-base): no VA
  rewrite needed for named rows; `fn_XXXXXXXX` rows get their VA suffix rebased
  via the full map. Left untouched in P1.
- **target_symbol_map.json / ghidriff_identities / fingerprints / Ghidra program**:
  re-key VA-side via the full map; Ghidra TU5 is a SEPARATE program (don't disturb
  base on MCP :8002).
- **Same-instrument patch (P-retarget)**: use the correct-TU5 column above (NOT
  the spike column, which was uniformly −0x8000). Cave = **0x82C55010** (TU5
  BINK-tail zeros; base 0x82C25000 is REAL CODE on TU5). Packer/binpatch
  (`objcave_pack.py`, `xex_binpatch.py`) assume FLAT image → must section-map the
  TU5 writes. IsActive + GetDiffList detours re-derive on the TU5 body.
