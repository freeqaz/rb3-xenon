# STL element-stride ground-truth (fill_insert / fill_n / resize / erase near-miss family)

Date: 2026-07-20 · branch `stride-re` · baseline whole-binary `matched_functions = 18893`

## TL;DR

The STL `_M_fill_insert` / `__uninitialized_fill_n` / `resize` / `_M_erase` near-miss
family is walled by the compiler baking `sizeof(element)` into the container code
(`li <n>; divw`, `mulli <n>`, or `srawi <log2>` when the size is a power of two).
Where our element `sizeof` differs from retail, every container op for that element
mismatches.

**Two hard methodology lessons (both cost real analysis time here):**

1. **fill_insert / fill_n / insert_overflow strides LIE via ICF folding.** These
   growth-path helpers are highly foldable — the paired target is often an
   *ICF representative of a different element type that happens to share a size*,
   so its `divw`/`mulli` immediate is NOT your element's true size. Corroborate
   against **type-specific element-copy code**: `__uninitialized_copy`, `operator=`,
   copy-ctor loops (these fold rarely because they emit per-member loads/stores),
   or — strongest — **direct member-access offsets in ordinary (non-template)
   retail code that uses the type**. A stride supported *only* by a fill-family fn
   is suspect.

2. **The automated stride sweep (`scripts/truth_table.py`) is a SCREEN, not truth.**
   It maps report near-misses → VA (via `scripts/target_symbol_map.json`) → target
   `.s` and extracts `divw`/`mulli`/self-increment-`addi` immediates. It surfaces
   candidates fast but carries fold + map-VA→asm-fn mis-pairing noise (e.g. it
   reported SongCollisionOutput as 28/24 when that type is actually 228 B and
   already ~matched). **Every candidate must be confirmed with a
   `run_diff_inspect mismatches` read** showing the actual `srawi`-vs-`divw`
   immediate divergence before you touch a header.

## Verified (diff_inspect-confirmed) findings

| element | container/unit | ours | retail | evidence | class |
|---|---|---|---|---|---|
| **SongPattern** | `vector<SongPattern>` SongLayout | 40 (0x28) | **24 (0x18)** | fill_n single mismatch `addi …,0x18` vs `…,0x28` | SHRINK — DC3 added `mMoveParents`(12)+`mNumMoves`(4); retail = `{Symbol mName; Range mInitialMeasureRange; vector<Symbol> mElements;}`. Coordinator landed +3 (tail-w3). |
| **Transform** | `vector<Transform>` TrackDir/CharUtl | 64 | **120 (0x78)** | target `li r9,0x78; divw` vs our `srawi r10,r10,6` (/64) | FOUNDATIONAL. Global `Transform`=Matrix3(48)+Vector3(16)=64 is PROVABLY correct (thousands of matches + DC3 identical & matching). The 120 is REAL but LOCALIZED to these containers → a distinct larger element type in retail, **NOT** a global `Transform` change. **Do not touch `math/Mtx.h` Transform.** |
| **DataArrayPtr** | `vector<DataArrayPtr>` MetaMusic | 4 (trivial `DataArray*`) | **12, non-trivial** | target `li r10,0xc; divw /0xc; mulli *0xc` + out-of-line `bl _Copy_Construct@DataArrayPtr` + AddRef `lhz r11,0xa`; ours `srawi/2`, `slwi*2`, inline trivial ptr copy | FOUNDATIONAL. Retail RB3 `DataArrayPtr` = 12-B non-trivial smart ptr; DC3 (and us) reduced to 4-B trivial. **Blind 4→12 pad measured −211 matched_functions** (see below). Needs true-layout reconstruction (2 extra members retail code accesses + non-trivial copy ctor), a dedicated campaign, not a pad. |
| **LocalizedName** | HamMove | 24 (main-landed) | **84 (0x54)** | the paired fill/copy fold into an 8-byte stub split `auto_03_827148A8_text` → the "24" match is a fold artifact | The W4 "16→24 proven flip" is a **fold-artifact match** (byte-matches a 0x18 representative while capping every real sibling). True retail = 84 per coordinator W3. Report the fold targets as correlator-repoint candidates; land the 84-byte truth layout instead. |
| SongCollisionOutput | SongCollision | 228 (0xE4) | 228 | already reverse-engineered raw `_data[0xE4]`, copy at 99.86% | Already solved; table's 28/24 were mis-extractions (negative control for the SCREEN). |

## Blast-radius prototypes (as requested)

- **DataArrayPtr 4→12 (size-only pad), full rebuild, whole-binary A/B: −211
  matched_functions.** Catastrophic as a blind pad. This is the "expose" signal:
  the 211 losers embed `DataArrayPtr` by value / use its `sizeof`, and retail's
  real 12-B *non-trivial* layout would have to satisfy all of them too (their
  members retail accesses), which a pad cannot. Recommended shape: reconstruct the
  true 12-B layout from Ghidra member accesses across the ~20 `DataArrayPtr`-named
  container symbols + the DataArray refcount (`+0xa`), add the non-trivial copy
  ctor, then A/B. High value IF cracked (binary-wide), high risk — dedicated
  campaign, gated.
- **Transform**: NOT prototyped globally (provably catastrophic — global size is
  correct). The 120-B divergence is container-local; recommended shape is to
  identify whether retail's TrackDir/CharUtl stored a distinct
  Transform-plus-cache element type, not to grow `Transform`.

## Coordinator negative controls (folded in, all consistent with the method)

- `Key<Vector3>` retail-20-vs-ours-4 (W2 believes true 16): fill-only ⇒ suspect fold; verify before touching.
- `ConstraintSystem@CharBlendBone` retail-112-vs-ours-16, oracle confirms 16: size-mispair / wrong target pairing ⇒ classify, don't pad.
- `TransformCrowd` retail-28-vs-ours-16: same ObjPtr re-layout migration family as DataArrayPtr (foundational).
- `RndText::Style` retail-fill-68-vs-ours-36: `_M_allocate_and_copy<Style>` (type-specific) is at **100%** with size 36 ⇒ retail Style = 36, the 68 is a fold; the Text vector<Style> 76% near-miss is regalloc, not stride. Another negative control.
- Fold-diff pairs the SCREEN itself flags (`fill != copy`): FlowMathOp (copy 12/fill 52), MoveRating (24/44), SongCollisionOutput (28/24), TransformArea (36/48) — in every case the copy-loop value is the truth and the fill value is the fold rep's size.

## Classification of the pool

- **DEAD-PAD free-flips are scarce here.** Most 98–100% near-misses already have
  `sizeof(element) == retail` — they are regalloc/scheduling near-misses, not stride.
- **SHRINK (DC3-newer added members retail lacks):** SongPattern (landed). Rare;
  each needs removing members that unit logic references (compile-affecting), so
  contained but not free.
- **FOUNDATIONAL type-identity:** Transform (120, container-local), DataArrayPtr
  (12 non-trivial), CamShotCrowd (32→264), Key<Transform>. High blast; reconstruct,
  don't pad.
- **FOLD artifacts (do not chase):** LocalizedName-24, and any fill-only "truth"
  in `truth_table.py` output without a copy-loop or member-access corroborant.

## Tooling (committed on branch)

- `scripts/truth_table.py` — corroborated screen: per element type, copy-loop
  self-increment stride (TRUTH, ≥2× = src+dst ptr) vs fill `divw`/`mulli` (SUSPECT);
  flags `<FOLD-DIFF>` when they disagree.
- `scripts/consolidate.py` — earlier per-element aggregation (retail-only).
- Our-side `sizeof` obtained via `/FAs` asm-listing probe (functions returning
  `sizeof(T)` → the `li r3,<n>` immediate). Note: wibo needs `/Isrc/system`
  (no space) — the space form silently fails include resolution.

## Bottom line for landing

A systematic pass of the whole fill/fill_n/resize/erase near-miss pool (184 paired
near-misses) found **no clean dead-pad free-flip** available: the tractable real
divergence (SongPattern shrink) is coordinator-owned and landed; every large "GROW"
signal reduced to either a fold artifact (RndText::Style, LocalizedName, Key<Transform>)
or a foundational type-identity reconstruction (Transform container-local, DataArrayPtr,
CamShotCrowd) that regresses under a blind pad (DataArrayPtr measured −211). No
speculative patch was landed, per the whole-binary net-positive gate. The value here
is the corroborated table + the two foundational blast-radius analyses + the fold-trap
catalogue that keeps the tail-waves from chasing fold sizes.
