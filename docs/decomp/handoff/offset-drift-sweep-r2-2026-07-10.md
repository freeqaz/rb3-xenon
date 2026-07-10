# Offset-drift sweep ROUND 2 (2026-07-10, baseline 14,450)

> **STATUS (2026-07-10, executed same day): ALL 3 LANES LANDED — +23 strict.**
> lane-1 `5b399b94` (+4: CharBonesSamples LoadData/SetVer/Print + StoreMenuPanel
> Handle, all →100; unplanned symbols.txt carve of misbounded fn_823CB928);
> lane-2 `fa321bb0` (+2 strict: Rnd::CreateDefaults + ParseNode →100; 3 fuzzy
> banked: RandomGroupSeqInst 98.5, SetupHeadsetSubmixes 91.8, OnEnterCloset
> 99.5; getMasher + inflate SKIPPED — see verdicts); lane-3 `8e30a244` (+17:
> ticker +8 incl. Poll →100, InlineHelp +5, SafeName de-inline →100 w/ (float)
> casts on fmod args, MinEq removal +2 clean across the 40-file surface, +1
> MemcardMgr::Init inlining-ripple collateral). All gates run1==run2, 0 strict
> + 0 fuzzy regressions, ICF HONEST. Main after round: **14,601**.
> New leads recorded below remain open (LabelNumberTicker ctor/factory map
> entries, InlineHelp recarve, UtilDrawPlane 99.3 permuter, misbounded-fn scan).

Second run of the generalized sweep (`scripts/harvest/offset_drift_sweep.py`,
method in `offset-drift-sweep-2026-07-10.md`). Pool regenerated post-14,450:
355 near-misses (85–99.99), 83 flagged, 43 with ≥2 struct/global diffs
(ranked table: `~/tmp/drift_candidates_r2_table.md`, regenerable). Round-1
unclaimed BandUI Wipe pair had already closed to 100 via concurrent waves.

7-lane ultracode recon (dossiers `~/tmp/r2_recon_{ticker,uipanels,rndutl,
datafile,synth,band3,vendor}.txt`, 1.16M tokens) + regression-surface mapper.
**Adversarial scorecard: raw layout readings refuted in the large majority of
candidates again** — but recon converted most refutations into verified
non-layout fixes. 13 GO/GO_WITH_CARE candidates mapped into 3 disjoint lanes.

## Confirmed root causes (this round's new lessons)

- **Mid-class extra member variant of the vbase-anchor pattern**
  (LabelNumberTicker `unk74`): members BEFORE the insertion drift −N in
  anchor-relative negative offsets while members AFTER drift +N from `this`.
  Same fix family: delete the Wii/DC3-only member.
- **DC3-only member in a shared engine header** (InlineHelp
  `ResourceDirPtr mResourceDir`, 0x10 bytes): retail-360's resource system
  lives in UIComponent; DC3 added a per-component ResourceDirPtr later.
  Sweep signature was AppInlineHelp's vbase anchor +0x10.
- **Inline-vs-out-of-line divergence** (SafeName): dc3 header has it inline;
  retail RB3 compiled it out-of-line (fn_82733060, already mapped) and every
  cross-TU user CALLS it. Our /Ob2 build inlines → inserts + regswap cascade
  that sweep misread as "BandCharacter drift −20".
- **DataFile TU statics ordering** — sweep reading CONFIRMED (rare): same
  .bss reverse-emission class as the round-1 Locale win. Refined rule: MSVC
  X360 /O1 pools ALL uninit/zero-init file-scope objects (statics AND
  externs, scalars AND structs) in ONE pool in REVERSE definition order;
  dyn-init follow in FORWARD order.
- **Anchor-bias artifact is the dominant false positive** (3/3 in synth lane,
  StoreMenuPanel, FretMatchImpl, RandomGroupSeqInst): TGT and SRC using
  different base registers makes offset deltas meaningless — compute absolute
  addresses first (`addi rX, base, K` is the tell).
- **`__savegprlr_N` prologue index is a free witness** for a missing
  hoisted-reference local (retail saving r25 vs ours r26 = one extra
  long-lived address local in retail source).
- **dc3 oracle text can be a mirage for src/system engine code too**, not
  just game code: retail RB3's RandomGroupSeqInst ctor has a nested retry
  loop the newer dc3 source removed. Ghidra-decompile retail before porting
  oracle bodies in the 85–95 band.
- **`cls '?'` li-immediate rows are actionable**: a `li r6, 0x2` vs `0x0`
  (XAUDIO2_VOICE_NOPITCH) was a real arg-constant bug (~1 KB of fn).
- **Retail dropped Wii guards** (BandWardrobe::OnEnterCloset): outer
  `if (dir)` / `if (i != -1)` wrappers absent in retail; MILO_ASSERT stays.

## Implementation lanes (launched 2026-07-10)

- **lane-1 (serialized, owns target_symbol_map.json):** CharBonesSamples
  LoadData convention port (BinStream& + mutable `gVer` + assert-free
  SetVer = retail fn_823CB928, map edits 0x823CBE08/0x823CB928) + Print
  de-hoist + StoreMenuPanel::Handle (size()−1 arm +
  MultipleItemsEnumCompleteMsg arm + 0x82620830 map value fix).
- **lane-2 (7 parallel-safe single-TU body edits):** Rnd::CreateDefaults
  (drop 3 DC3-era CreateAndSetMetaMat + mDefaultCam shape), DataFile
  ParseNode (statics reorder + '\t' arm + bool reshape), RandomGroupSeqInst
  ctor (pre-verified 90.4→98.5, patch `~/tmp/r2_synth_proposed.patch`),
  SetupHeadsetSubmixes (pre-verified 86.4→91.8), getMasher /Od shape,
  inflate cast-drop, BandWardrobe OnEnterCloset guard drops.
- **lane-3 (shared headers, lands last after rebase):** LabelNumberTicker
  unk74 delete (+7 expected), InlineHelp mResourceDir delete (+5 expected),
  SafeName de-inline (obj/Utl.h→Utl.cpp + objects.json wiring),
  math/Utl.h MinEq/MaxEq/ClampEq float-specialization removal (widest edit,
  40 files / 23+ pinned TUs, wholesale-revert on any regression).

## Dropped to permuter/body-port (registered in nearmiss_verdicts.json)

FreeCamera::Poll (anchor artifacts + regalloc), GainEffect::DoProcess
(copy-idiom unknown; 6 idioms A/B-exhausted — crack-farm with exclusion
list), RGGemMatcher::FretMatchImpl (anchor-bias + bool-mask tail),
CharBonesSamples Relativize/EvaluateChannel (access order; optional
file-static quat-align helper experiment per retail 0x823CB938),
TrainerGemTab::DrawTails (body-port leads: `2.5f/480.0f` constant fold,
arg locals, one fewer Transform temp), LabelNumberTicker::UpdateDisplay
(separate Locale.h LocalizeSeparatedInt signature follow-up).

## Follow-ups spotted during recon (not in any lane)

- default/InlineHelp pin is TRUNCATED (TU tail in auto_03_82303594_text:
  InlineHelp ctor fn_82303C88, ClearActionToken, SetActionToken…) — recarve
  candidate after the layout fix lands.
- Retail SafeName body sits inside DirLoader.cpp's pin (foreign-tail pin
  hole) — future DirLoader recarve note.
- StoreMenuPanel TU is Wii-shaped beyond Handle (OnMsg 7.96, OnBack 36.3,
  FinishLoad 39.7, Unload 1.6, GetCrumbText 63.4, AddMenu 82.6, ctor
  unmapped) — body-port lane candidate; retail OnMsg body is
  MultipleItemsEnumCompleteMsg-shaped (decompile 0x82620830).
- meta_band/Utl.cpp OnSafeName retail callsite at 0x825A6180 benefits a
  future meta_band/Utl pin.
- LabelNumberTicker ctor fn_82802840 / factory fn_82803030 unmapped — add
  map entries after the unk74 fix for 2 more potential closes.
- CharBonesSamples: retail 0x823CB938 = out-of-line static quat
  hemisphere-align helper; fn_823CC920 = Save; LoadHeader retail address not
  found in pin (likely inlined/stripped).
