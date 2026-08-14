# `SOURCE_INSDEL` 4–6 charge band, worked to 64% by bytes — lane INSDEL-4 (2026-08-14)

Tree `6c4eaf9a` (INSDEL-3 merged) + this lane. Ruler `functionRelocDiffs=name_check`,
read from `report.json` `provenance.diff_config` and re-confirmed by driving
`objdiff-cli` with the grader's exact config on every per-row reading.

Baseline (leg A, settled): `matched_functions` **44,419** · `masked_equal` **22,897**
⇒ honest **21,522** · `matched_code_percent` **36.141900**.

## Result — +3 functions / +576 B, prediction pre-registered and EXACT

| measure | predicted | measured |
|---|---:|---:|
| Δ`matched_functions` | +3 | **+3** |
| Δ`matched_code` | +576 B | **+576 B** |
| Δ`matched_code_percent` | +0.005581 pp | **+0.005580 pp** |

0 regressions · **0 units fell off 100% on EITHER ruler** · unit net +3 ==
whole-binary +3, in exactly the three edited units · Native gate
**PASS 18/18, 0 SKIPs, rc=0**.

## ⛔⛔ FIRST, THE SIZING CORRECTION: the 4–6 band is 10,912 B, NOT a slice of 446,724 B

The brief priced this lane against *"the stratum is 804 rows / 446,724 B — 83.1% of
the entire addressable surface"*. That is the **whole** `SOURCE_INSDEL` stratum, and
it is dominated by rows nobody proposes to work:

| charge band | rows | bytes | avg |
|---|---:|---:|---:|
| ≤3 (INSDEL-1/2/3) | 45 | 3,652 | 81 B |
| **4–6 (this lane)** | **54** | **10,912** | **202 B** |
| 7–15 | 160 | — | — |
| 16+ | **545** | — | — |

⇒ **A perfect 54/54 sweep of this band buys ~10.9 kB**, and this lane took 5.3% of
it. Price the *next* lane against 10,912 B, not 446,724 B.

★ The stale-census hazard INSDEL-3 warned about **did not bite here**: joined to a
fresh `report.json`, **0 of the 54 rows had been closed** since INSTR-1's census.

## Rows closed — all three are ONE family, all proven BY ABSENCE

The family is **a surplus entry in a source-visible member/behaviour list**. In all
three the charges are a single contiguous cluster and the target is *smaller* than
our build by exactly the surplus.

### `?Copy@RndLight@@UAAXPBVObject@Hmx@@W4CopyType@23@@Z` (364 B, 95.604 → 100)

Dropped `COPY_MEMBER_FROM(l, mTextureXfm)`. Our surplus was an inlined
`memcpy(this+272, l+272, 64)` — the only 4 charges in the function.

★ **Control read before editing, and it is INSIDE THE ROW**: retail emits a visible
copy for **all nine** sibling `COPY_MEMBER_FROM` lines (mColor → `stw -172/-168/
-164/-160(r31)`, mType −136, three bools −132/−131/−130, mRange/mFalloffStart
−144/−140, mTopRadius/mBotRadius −16/−12, mTexture + mCubeTexture via
`SetObjConcrete`, mShadowObjects via the `ObjPtrList operator=`, mProjectedBlend →
`stw -8(r31)`). The mechanism is provably visible **per member**, and `mTextureXfm`
alone has no trace anywhere in the body — retail goes straight from the
mShadowObjects assign to `lwz r11, 344(r30)`.

⚠ Decoding trap worth reusing: **`r31 == this + 352` in this function**, so
`subi r3, r31, 80` is `this->mTextureXfm` (272 = 352 − 80), not a stack temp. Read
that wrong and the whole row looks like an unnamed by-value temporary instead of a
member copy. `class_layout_report.py` confirms `0x110 Transform mTextureXfm` and
`0x158 mProjectedBlend`.

### `?Copy@Screenshot@@UAAXPBVObject@Hmx@@W4CopyType@23@@Z` (128 B, 92.938 → 100)

Dropped the trailing `Sync();` from `BEGIN_COPYS`. Two controls, both readable first:

1. ★ **SIZE INEQUALITY IN THE DIRECTION THAT EXCLUDES INLINING** — target 128 B vs
   our 136 B, exactly the 8 bytes of `subi r3,r31,60` + `bl ?Sync@Screenshot@@`.
   `Sync()` is large (two `RELEASE`s, two `Hmx::Object::New`, `SetBitmap`,
   `SetZMode`, `SetDiffuseTex`), so an **inlined** Sync would make retail much
   *bigger*. Retail is smaller by precisely the call sequence ⇒ absent, not inlined.
   This is the sharpest form of the size-inequality control INSDEL-3 named: it
   discriminates *absence* from *inlining*, which raw absence cannot.
2. An in-source note at the top of the same file records a prior lane finding the
   **same divergence at an adjacent site** — retail's `Screenshot::SyncProperty`
   also has no `Sync()` call where dc3/rb3-Wii do.

⚠ `Screenshot::Load` also calls `Sync()` and would have been the natural control,
but **retail's `Load` is anonymous in the map** so it cannot pair. Left untouched.

### `??0FormatString@@QAA@PBD@Z` (84 B, 89.905 → 100)

Dropped the dead `mType(kNone)` initializer. The compiler puts `Type mType` at
**2064** and the enum has **`kNone == 3`**, matching our surplus `li r9,3;
stw r9,2064(r31)` 1:1 — the charge *names* the initializer, it does not imply it.

★ Control: the **sibling default ctor** (`FormatString::FormatString()`, same file)
already omits `mType` from its init list, so our two ctors disagreed and retail
matches the sibling's shape. Independently the initializer is **dead** —
`InitializeWithFmt()` → `UpdateType()` assigns `mType` immediately after.

⚠ **DC3 could not have adjudicated any of the three** (`src/system` is a verbatim
DC3 copy and DC3 is newer). All three were settled on retail bytes alone.

## Does the control screen still discriminate at 4–6 charges? YES — and it got STRICTER

**3 rows opened and edited, 3 closed (100%).** Cumulative: INSDEL-1 3/6, INSDEL-2
5/7, INSDEL-3 4/4, INSDEL-4 3/3 ⇒ **15 of 20 (75%)**.

But the headline hit rate is the wrong number to carry forward, because the screen's
job at this width is mostly to *reject*:

| | rows | bytes |
|---|---:|---:|
| charge-dumped and triaged | 19 | 6,968 |
| produced a readable pre-registration control | **3** | 576 |
| declined | 16 | ~6,392 |

⇒ **The control existed for 3 of 19 rows (16%)**, versus 4-of-4-opened at ≤3
charges. The screen did not weaken — **the population did**. Every row that produced
a control closed; no row without one was edited.

★★ **The new failure mode the brief predicted is REAL and it is the dominant one:
`matched_code` is all-or-nothing per row, so ONE unclosable charge withholds the
whole row — and at 4–6 charges the odds of drawing one are high.** This is not a
theory; it is measured on the band's two largest rows:

- **`PerfectOverdriveTracker::Poll_` (1,248 B — 11.4% of the whole band) is
  UNCOLLECTABLE BY CONSTRUCTION.** Two of its four charges are commutative
  integer operand order, which an in-source note (lane EE2-B) records as
  **MEASURED INERT** — rewriting both in retail's order is byte-identical output.
  The other two are register materialization. No source edit can cross this row.
- **`BandRetargetVignette::EnterDir` (772 B)**: two of four charges are ICF
  fold-aliases (`vector<Dep*>::reserve` / `vector<ChatReceiver*>::push_back` vs our
  `vector<RndPollable*>`, identical code per `T*`, our spelling semantically right).
  Closing them means **installing aliases** — forbidden. The other two charges are
  real. Row cannot cross.

## Sized: how much of this band is uncollectable

| cause | rows | bytes | % of band |
|---|---:|---:|---:|
| proven-inert charges (EE2-B note) | 1 | 1,248 | 11.4% |
| fold-alias charge blocks the row | 2 | 868 | 8.0% |
| **measured-drained by a prior lane** | 2 | 472 | 4.3% |
| symbol absent from target (instrument) | 2 | 1,120 | 10.3% |
| **subtotal, demonstrably uncollectable** | **7** | **3,708** | **34.0%** |

⇒ **A third of the band is off the table before any codegen triage**, which is the
number the next lane should plan against.

⚠ **Instrument discrepancy I did NOT resolve** (worth a tool lane): `CamShotCrowd::
AddCrowdChars` (616 B) and `FocusTracker::GetNextFocusPlayer` (504 B) are scored in
`report.json` with `fuzzy < 100`, but `objdiff-cli diff` refuses both with
**`Symbol not found in target`**. I suspected the zsh `$`-in-double-quotes trap
(`?$list@…`) and **tested it — the hypothesis was WRONG**: single-quoting reproduces
the failure with the full symbol echoed. So the two paths genuinely disagree about
whether the symbol pairs.

## Declined, with reasons — the codegen classes at this width

- **`Vector3Keys::SetFrame` (584 B)** — the four charges are the *same instructions
  transposed* (`addi r11,r30,28` + `mr r3,r11` vs a late `addi r3,r30,28`). A
  permutation artifact.
- **`VocalTrack::Poll` (512 B)** — retail computes the bool into `r11` and
  normalizes through a join (`clrlwi r30,r11,24`); we materialize into `r30`
  directly. Looked like the exact **inverse** of INSDEL-3's `CustomizePanel::
  IsLoaded` closure, and there *is* a real branch (the discriminator that separated
  `IsLoaded` from `GetFretboardView`). **Refuted by in-source notes**:
  `GemManager.cpp:1449` and `Player.cpp:223` both record that retail's mask **is the
  signature of the inlined `InRollback()` accessor**, which our source already
  spells. Our shape is right; the residual is register choice, and the only lever is
  a `Game.h` edit rippling to ~18 sites.
- **`BandCharacter::AddOverlays` (112 B)** — retail hoists an end-pointer load plus a
  dead `mr r10,r11` at loop entry while both sides reload `152(r11)` at the loop
  bottom: loop rotation, no source token.
- **`Campaign::GetLaunchUser` (68 B)** — retail saves `r31` and sets up
  `subi r31,r1,96`; we do not (retail *bigger*, 68 vs 56). The r31 second-frame-
  pointer class.
- **`MasterAudio::SeeGem` (204 B)**, **`PrefabChar::LoadPortrait` (164 B)**,
  **`Campaign::HasReachedCampaignLevel` (280 B)**, **`CritSecTracker` ctor (64 B)** —
  bool normalization / temp-slot placement / fold-alias mixes.
- **`MusicLibraryStore` ctor (268 B) + `ClearPreview` (204 B)** — ⛔ **DRAINED, and
  the refutation was already in the tree.** Retail takes `&TheContentMgr + 4` and
  calls `MsgSource::AddSink` where we load a pointer and call `Object::AddSink`,
  because `ContentMgr.h:191` declares `extern ContentMgr &TheContentMgr` — a
  reference where retail's is a plain object. `MusicLibraryStore.cpp:33` records that
  flipping that decl (26 TUs) **measured −37 functions / −17,312 B**. Not retried.
  ★ Seventh time this session that reading in-source notes killed a plausible chase;
  here it was a 472 B prize that costs 17 kB.

## ★ The best lead I am NOT taking: the `NewObject` class (448 B, 4 rows)

`?NewObject@X@@SAPAVObject@Hmx@@XZ` for **LayerDir, BandRetargetVignette, UnisonIcon,
OverdriveMeter** — 112 B each, 6 charges each, **all four at fuzzy 86.929**, all four
`src/system/bandobj/` classes carrying `NEW_OVERLOAD` + `NEW_OBJ`. Retail builds a
`Symbol` from `StaticClassName()` into a stack temp and then calls a **two-arg**
allocator `fn_827BCD38(548, 0)`; we call the class's own one-arg
`??2LayerDir@@SAPAXI@Z`. This is the mirror image of INSDEL-3's `DELETE_POOL_OVERLOAD`
finding.

**Why deferred rather than attempted:** the fix lives in the `NEW_OVERLOAD` macro
(**53 sites**), the callee is anonymous so the identity argument cannot be closed
from the map, and `MemMgr.h` carries a standing note that retail deliberately keeps
class `operator new`/`delete` out-of-line for ICF folding — i.e. this macro family is
**load-bearing**. It needs a dedicated lane with its own whole-binary A/B, not a
cheap-head edit. ⚠ And per the standing warning, four rows sharing a *shape* is not
proof they share a *cause*.

Second lead, smaller: **`RndCamAnim::Save` (152 B)** — retail writes a rev read from
a **mutable `.data` global** (`lis/lwz lbl_82C709F8`, 4 B in `.data`) where we write
the literal `2` from `SAVE_REVS(2, 0)`. A real named divergence, but the TU's
`gRevs_CamAnim` aggregate carries an explicit note that the pair **must** share one
aggregate, and `Load` matches today — so it is a save-revision lane's row, not this
one's.

## Is the 4–6 band worth another lane? Marginally — and here are the numbers

| | ≤3 head (INSDEL-3) | 4–6 band (this lane) |
|---|---:|---:|
| rows closed | 4 | **3** |
| bytes | +316 | **+576** |
| **bytes per closed row** | 79 | **192** (2.4×) |
| rows triaged to close one | ~4 | **6.3** |
| band total | 3,652 B | 10,912 B |
| never-opened remainder | 16 rows / 752 B | **33 rows / 3,944 B** |

**Per closed row the 4–6 band pays 2.4× the ≤3 head — but it costs ~6× the triage,
so per unit effort the two are close to a wash.** The honest advantage of 4–6 is
that its rows are big enough that *one* closure beats an entire ≤3 sweep.

⇒ **Recommendation: do NOT fund a general 4–6 sweep.** The remainder is
**33 rows / 3,944 B**, of which **14 rows / 1,892 B (48%) are STL template bodies**
(`_M_fill_assign`, `_M_insert`, `_Rb_tree::insert_unique`, `__destroy_range`,
`_M_erase`) — the same low-value shape that exhausted the ≤3 head — and 3 more are
the deferred `NewObject` class. The genuinely fresh non-STL remainder is
**≈16 rows / ~1,716 B (~107 B avg)**.

Fund instead, in order:
1. **The `NewObject` / `NEW_OVERLOAD` class** — 448 B in this band alone, and the
   macro reaches 53 sites, so the population outside the band is likely larger.
2. **The 7–15 charge band** (160 rows) — untouched, and the *size* argument that
   made 4–6 beat ≤3 points the same way again. Expect the control to exist for well
   under 16% of rows, and budget triage accordingly.
3. The two `MEMBER(this)` layout rows the brief listed (`CampaignSongInfoPanel::
   Unload` +8, `OggMap::Read` +44) — untouched by this lane.

## Tooling

`~/tmp/insdel4/show.py` (per-row charged-instruction dump at the shipped ruler,
inherited from INSDEL-3 with `WT` repointed). Per-row findings are recorded as
**in-source notes** at `Lit.cpp`, `Screenshot.cpp`, `MakeString.cpp`.
