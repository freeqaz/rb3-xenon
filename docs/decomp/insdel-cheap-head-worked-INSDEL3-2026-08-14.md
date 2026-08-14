# `SOURCE_INSDEL` cheap head, third pass — lane INSDEL-3 (2026-08-14)

Tree `9c269ffe` (INSDEL-2 merged) + this lane. Ruler `functionRelocDiffs=name_check`,
read from `report.json` `provenance.diff_config` and re-confirmed by driving
`objdiff-cli` with the grader's exact config on every per-row reading.

Baseline (leg A, settled): `matched_functions` **44,415** · `masked_equal` **22,897**
⇒ honest **21,518** · `matched_code_percent` **36.138840**.

## Result — +4 functions / +316 B, prediction pre-registered and EXACT

| measure | predicted | measured |
|---|---:|---:|
| Δ`matched_functions` | +4 | **+4** |
| Δ`matched_code` | +316 B | **+316 B** |
| Δ`matched_code_percent` | +0.003062 pp | **+0.003060 pp** |

0 regressions · **0 units fell off 100% on EITHER ruler** · unit net +4 ==
whole-binary +4, in exactly the four edited units.

## Rows closed

### `?IsLoaded@CustomizePanel@@UBA_NXZ` (72 B, 93.889 → 100)

Source was `return !UIPanel::IsLoaded() ? false : !TheContentMgr.RefreshInProgress();`.
Retail materializes **each return path directly into the return register**
(`li r3,0` on the false arm, `extrwi r3,r11,1,26` on the other) and the tail is
bare epilogue. A single ternary return expression funnels both arms through a
common register (`li r11,0` / `extrwi r11,...`) and emits one join-point
conversion `clrlwi r3,r11,24`. Rewritten as two separate `return` statements.

★ **Positive control in the same unit, found before editing:**
`IsCurrentAssetPatchable` (84 B) matches at **100%** using
`if (...) return true; else return false;`. `Unloading` and `HasNewAssets` are
branchless single expressions, so they never exercise a join and are silent on
the question — the control had to be a *branching* bool function.

### `??_GCameraManager@@UAAPAXI@Z` (76 B, 94.211 → 100)

Dropped `DELETE_POOL_OVERLOAD(CameraManager)`, kept `NEW_POOL_OVERLOAD`. Retail
calls a **one-arg** static `operator delete(void*)` (`mr r3,r31; bl …`); the pool
form inlines as the **two-arg** `li r3,52; mr r4,r31; bl PoolFree`.

⚠ **This is a MECHANISM difference, not the fold-alias class below** — the
discriminator is that target and base **sizes differ** (76 vs 80) and there is a
surplus instruction (`li r3,52`), where a fold-alias has equal sizes and a lone
relocation-name charge. Blast radius is exactly one function: the destructor is
virtual, so every `delete` of a CameraManager routes through `??_G`.
`NEW_POOL_OVERLOAD` was deliberately left alone so no allocation site moves.

### `??_GQuestFilterPanel@@UAAPAXI@Z` + `??_GNewAwardPanel@@UAAPAXI@Z` (84 B each, 90.238 → 100)

Removed the user-declared empty `virtual ~X() {}` from both classes. Retail's
scalar deleting destructor **inlines the vbase-destructor body** —
`bl ??1TexLoadPanel@@UAA@XZ` then `addi r3,r31,<vbase off>; bl ??1Object@Hmx@@` —
while ours emitted a call to the out-of-line `??_D<Class>@@QAAXXZ`. Letting the
**implicit** destructor be generated reproduces retail exactly.

★ **NewAwardPanel was held back as an UNTREATED CONTROL**: only QuestFilterPanel
was edited first, and NewAwardPanel was touched only after that closed. The
second closure is therefore confirmation of the class, not a second guess.

## ⛔⛔ The 302 sub-100 `??_G` rows are NOT a 23 kB prize — 218 of them are fold-alias

The `??_G` census looks like a large vein and **must not be funded as one**:
302 sub-100 rows / **23,564 B**. The distribution kills it:

| band | rows | what it is |
|---|---:|---|
| 99.71 – 99.75 | **218** | **ICF fold-alias — NOT source-addressable** |
| 90.238 | 2 | the `??_D` class above — **both closed by this lane, band now empty** |
| 0.0 | 41 | xdk / `auto_*` — unpairable by construction |

The 99.7x rows have **equal target and base sizes** and exactly **one
relocation-name charge**: retail `bl ?MemFree@@YAXPAX@Z` (or
`bl ??3BinStream@@SAXPAX@Z`) vs our `bl ??3<Class>@@SAXPAX@Z`. Same code, folded
survivor named arbitrarily.

⛔ **The obvious "fix" — dropping `__declspec(noinline)` from `DELETE_OVERLOAD` —
is REFUTED, and the refutation was already in the tree.** `src/system/utl/MemMgr.h`
(≈ lines 267–295) records a prior lane's **retail-byte** verification at
`CacheMgr::CreateCacheMgr`: retail deliberately keeps class `operator new`/`delete`
**out of line** and ICF-folds every identical body into a single thunk
(`fn_82709EE0`), so a call site is just `li r3,size; bl <operator new>`.
`__declspec(noinline)` is what reproduces that, and it is load-bearing.
⇒ Closing these rows would require **installing aliases** or "fixing" source to
satisfy a fold — both forbidden. **Do not re-open.**

★ Reading that note cost one `grep` and saved a tree-wide macro edit across
**61 `DELETE_OVERLOAD` / 53 `NEW_OVERLOAD` sites** that would have been measured,
possibly landed, and been wrong.

## Refuted / deferred, with reasons

- **`GemTrainerPanel::GetFretboardView` (60 B) — DEFERRED, codegen.** Signature is
  *identical* to CustomizePanel::IsLoaded (surplus trailing `clrlwi r3,r11,24`,
  predecessor writing `r11` not `r3`), but the full listing shows **no branch at
  all** ⇒ no join point, so the mask is pure bool *materialization*. The `extsb`
  is forced by `GetHighestFret()` returning `char`, so removing the named local
  would be invisible. ⚠ **A second instance of "the signature does not carry the
  direction"** (INSDEL-1's lesson, new vein): two rows with the same charge shape,
  only one with a source handle.
- **`WinSockSocket::RecvFrom` (136 B) — DEFERRED, codegen.** Retail reuses the
  already-live `0` in `r3` (`li r3,0` for the return value) for `stw r3,0(r31)`;
  we materialize a separate `li r10,0`. Both store 0. Register reuse, no token.
- **`DxRnd::Present` (224 B) — DEFERRED, already documented in-source.** Retail
  masks the bit in place (`rlwinm r3,0,30,30`) then normalizes with `subic/subfe`;
  we emit `extrwi`. A prior lane's in-source note records `(PIX()&2)!=0` and
  `?true:false` both still producing `extrwi`. Re-read and agreed **on the rule's
  own grounds** — the charge names a bool *normalization idiom*, i.e. codegen.
- **`Morph` `operator>>(BinStreamRev&, Key<Weight>&)` (72 B, 1 charge) — DRAINED,
  and re-opening it is a REGRESSION RISK.** `src/system/utl/BinStream.h` (≈ 238–262)
  records that this row's lone `mr r3,r30` is a **liveness artifact, not the
  template**, that the call-site cast in `math/Key.h` costs `Key<ObjectStage>`
  100 → 71.1, and that the alternative formulation measured **−14 matched /
  −848 B**. Not touched.
- **`NextSongPanel` ctor (172 B) — DEFERRED.** Charges are a vbase-displacement
  store (`stwx r29,r11,r30` vs `subi r10,r11,176; stwx r10,...`): layout class,
  not cheap head.

## Running hit rate of the NAMES-vs-IMPLIES rule

**4 rows opened and edited, 4 closed (100%).** Cumulative across the three lanes:
INSDEL-1 3/6, INSDEL-2 5/7, INSDEL-3 4/4 ⇒ **12 of 17 (71%)**.

The rule held again, and this lane adds a refinement about *how to apply it
cheaply*: **every row I opened had an in-unit or in-tree control I could read
BEFORE editing** (a 100%-matching sibling with the rival source shape; an
existing in-source note; a size-inequality distinguishing mechanism from fold).
Every row I declined failed to produce one. **The presence of a cheap
pre-registration control is itself the screen** — it is what separated
`CustomizePanel::IsLoaded` from `GemTrainerPanel::GetFretboardView`, which are
byte-for-byte the same charge signature.

## State of the cheap head

Re-derived on the current tree from INSTR-1's census joined to a fresh
`report.json`: **37 `SOURCE_INSDEL` rows at ≤3 charges were still open** at lane
start, of which ~12 are INSDEL-1/2 deferrals. After this lane the fresh,
never-opened remainder is **≈ 20 rows**, and they are getting harder — they
skew to STL template bodies (`_M_fill_insert`, `__destroy_range_aux`,
`_Rb_tree::_M_create_node`), tiny vtordisp adjustor thunks (8–16 B), and layout
rows. ⇒ **The head is thinning but not exhausted.** A next lane should expect a
lower hit rate than 4/4 and should probably widen to 4–6 charges rather than
grind the ≤3 remainder.

⚠ And re-derive the census: INSTR-1's TSV is stale, so join it to a fresh
`report.json` and drop rows already at `fuzzy == 100` before ranking.

## Tooling

`~/tmp/insdel3/show.py` (per-row charged-instruction dump at the shipped ruler,
inherited from INSDEL-2 with `WT` repointed). Per-row findings are recorded as
**in-source notes** at `CustomizePanel.cpp`, `CameraManager.h`,
`QuestFilterPanel.h`, `NewAwardPanel.h`.
