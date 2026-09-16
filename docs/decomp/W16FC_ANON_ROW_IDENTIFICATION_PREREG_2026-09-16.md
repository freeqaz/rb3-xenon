# W16-FC — naming anonymous rows inside pairable units: PRE-REGISTRATION

Lane W16-FC, worktree `~/tmp/wt-w16-fc`, branch `w16-fc` off main `1d028679`.
This document is committed BEFORE any measurement. Every number below that is a
prediction is labelled as such; measured values are added in a later commit.

## 0. Population reproduced (not inherited)

`build/45410914/report.json` on the built worktree (renamer applied — first full
build rc=0, log `~/tmp/rb3_build_w16fc_initial.log`), skip `default/auto_*` and
`/xdk`, keep units with a `base_path` in `objdiff.json`, keep `fn_*` rows at
`fuzzy == 0`: **6,691 rows / 1,228,556 B** — identical on main and on the
worktree (`~/tmp/w16fc_pop.py`). Worktree absolutes at measurement start:
`total_functions 69,240 / matched 43,991 / total_code 10,247,068 /
matched_code 4,134,460 / 40.347736% / fuzzy 50.031284 / masked_equal 23,228`.

## 1. Picks (12 rows, 30,048 B) — geometry proof and identification

Geometry (`tools/pdata_extent.py`, corrected `>>8` decode, on retail
`orig/45410914/band.exe`): for EVERY row below, `.pdata` BeginAddress == row
address, decoded FunctionLength == report size, exactly ONE `.pdata` entry inside
the extent, first word `7d8802a6` (`mflr r12`). Eleven of twelve have the next
`.pdata` entry exactly at `addr+size`; `0x8242F020` is followed by an 884 B run of
unwind-record-free leaf stubs (`lis/lfs/fcmpu/.../blr`, never touching LR), i.e.
the documented sub-`.pdata` stratum — the row's own extent is exact. **No pick is
a phantom.**

Identification: callee profile from the split `.s` (callees resolved through
`scripts/target_symbol_map.json`, keys are LOWERCASE hex), retail size vs the
size of the unpaired function of that name in OUR compiled base obj
(`tools/coff_bodies_ext.py`), and prologue argument-register usage vs the
signature. Duplicate-name census: none of the 12 names has any existing map row,
`symbols.txt` entry, or report row.

| retail addr | unit | retail B | name (map value) | ours B | evidence |
|---|---|---:|---|---:|---|
| 0x82b9e150 | GemManager | 3404 | `?SetupGems@GemManager@@QAAXH@Z` | 3408 | this+int; IsRealGuitarChord/LeftHandSlide/GetLoopTick/EndRepeatedChordPhrase/GetSectionBounds/IsKeyboardTrack/TickToMs; 5 named callers spell it |
| 0x825827d8 | MetaPerformer | 1052 | `?TriggerSongCompletion@MetaPerformer@@QAAXXZ` | **1052** | void(void), local-static guard; UpdatePerformanceData x2, GetScoreTypeForUser; caller `Handle@MetaPerformer` spells it |
| 0x82581360 | MetaPerformer | 1924 | `?SelectRandomVenue@MetaPerformer@@QAAXXZ` | 1264 | void(void), f31; SystemConfig x5, RandomFloat/RandomInt, HasReachedCampaignLevel x3; callers `Handle@MetaPerformer`, `SelectVenue@TourPerformerLocal` spell it |
| 0x824c8010 | CameraShot | 2996 | `?Load@CamShot@@UAAXAAVBinStream@@@Z` | 3100 | (this,BinStream&), first call ReadEndian(&rev,4); ReadEndian x31, >>Vector3/bool/Key<float>, SetObjConcrete<BandCharacter>, LoadSubPart, CamShotFrame ctor, ConvertFov |
| 0x824c6298 | CameraShot | 968 | `??0CamShot@@IAA@XZ` | 1016 | most-derived flag test on r4 (virtual base), vtable plant; Object ctor, RndAnimatable ctor; caller `NewObject@CamShot` (100.0) spells it |
| 0x82475a20 | Font | 2140 | `?Load@RndFont@@UAAXAAVBinStream@@@Z` | 2208 | (this,BinStream&), ReadEndian(&rev,4) first; ReadEndian x23, >>bool/String, RndBitmap::Reset x2, floor x2, Key<float>, PathName, Object::Load |
| 0x82475058 | Font | 1124 | `?SyncProperty@RndFont@@UAA_NAAVDataNode@@PAVDataArray@@HW4PropOp@@@Z` | 1164 | `lha r11,8(r5); cmpw r6,r11` = `i==prop->Size()`; PropSync<RndFont>/<RndMat>/<bool>, Symbol x8 |
| 0x82430fd0 | PostProc | 1728 | `?LoadRev@RndPostProc@@QAAXAAVBinStream@@H@Z` (TRUE retail signature, see §3) | n/a (ours is `…AAVBinStreamRev@@@Z`, 1916) | r5 compared to 0x18,5,6,0x12,9,0x1d,0xc,8,0x24,7 = DC3's LoadRev rev thresholds exactly; both retail call sites pass `lhz r5,4(rX)`; rb3-Wii declares `LoadRev(BinStream&, int)` |
| 0x8242f020 | PostProc | 1772 | `?Interp@RndPostProc@@QAAXPBV1@0M@Z` | 1848 | (this,p*,p*,float): f31=f1, r30=r4, r29=r5, null checks; SetObjConcrete<Object> x4, Interp(Vector3), AdjustColorXfm; 3 callers at 100.0 spell it |
| 0x824b7ae8 | LightPreset | 3024 | `?Load@LightPreset@@UAAXAAVBinStream@@@Z` | 2720 | (this,BinStream&); ReadEndian x10, vector<uint>::erase x4, ReadString x4, AddRef x4, >>bool x4, Object::Load; body recovers full object via `this-0xd8` |
| 0x825d01b8 | MusicLibraryNetSetlists | 1968 | `?ParseDataResultsIntoSetlists@MusicLibraryNetSetlists@@QAAX_N@Z` | 1784 | (this,bool) `clrlwi. r11,r4,24`; String(PBD) x23, GetDataResultValue x17, NetSavedSetlist ctor x4, AddSongTitle; caller `OnMsg` (W16-EW's row, 100.0) spells it; the unit's ONLY anonymous row |
| 0x825a0b28 | BandSongMetadata | 7948 | `??0BandSongMetadata@@QAA@PAVDataArray@@0_NPAVBandSongMgr@@@Z` | 3656 | r3,r4,r5,r7 kept, r6 passed straight to `??0SongMetadata@@IAA@PAVDataArray@@0_N@Z` (first call), plants vtable `lbl_8209E734`, calls `InitBandSongMetadata`; Symbol x80, FindArray x70, DataNode::Int x21; caller `AddSongData` spells it |

Why these and not others: the brief asks for rows where our source plausibly
already holds the body. Every pick has an unpaired base function of the same name
within ~0.5x–1.1x of the retail size, except the BandSongMetadata ctor (2.2x —
chosen deliberately as the largest row in the population and because the cascade
it opens is a large missing slice of field parsing) and SelectRandomVenue (1.5x).

## 2. Predictions (map-only patch; `tools/ab_measure.py --from-dirty`)

P1. `Δtotal_functions = 0`, `Δtotal_code = 0` — renaming does not re-carve.
P2. Each of the 12 `fn_*` rows is replaced by a row of the new name with the
    SAME size (sizes are asm extents; `report.json` sizes are a hazard elsewhere
    but here `.pdata` length == report size for all 12).
P3. Eleven rows pair and read `0 < fuzzy < 100`. Bands (retail/ours sizes drive
    these): TriggerSongCompletion ≥ 95; SetupGems 90–99.9 (4 B differ ⇒ cannot be
    100); CamShot::Load, ??0CamShot, RndFont::Load, RndFont::SyncProperty, Interp
    80–99; LightPreset::Load 60–92; ParseDataResultsIntoSetlists 60–92;
    SelectRandomVenue 40–80; ??0BandSongMetadata 30–65.
P4. **Designed row that must NOT move:** `0x82430fd0` named with the true retail
    signature `?LoadRev@RndPostProc@@QAAXAAVBinStream@@H@Z` reads `fuzzy 0`,
    unpaired, in the map-only leg — our base obj defines no symbol of that name,
    and objdiff pairs target↔base rows BY NAME, so the row cannot pair. It moves
    only after the SOURCE fix in §3. (Wording corrected 2026-09-16 per the
    coordinator: there is no "ruler declines to pay" mechanism — placeholder
    targets are forgiven by construction, so nothing is ever declined. What this
    control tests is PAIRING, and the requirement that survives is the practice:
    a named row that must improve without reaching 100, or here, must not pair.)
P5. `Δmatched_functions ∈ {0, +1}` and `Δmatched_code ∈ {0, +1052}`: only
    TriggerSongCompletion (exact size) can cross; no other row can.
P6. Aggregate `fuzzy_match_percent` rises by **+0.10 to +0.28 pp** (≈28,320 B
    of newly paired rows at ~40–95% over `total_code` 10,247,068).
P7. **Controls that must stay EXACTLY put** (their target side already calls the
    address; naming converts a forgiven site into a checked one, and our base
    spells the identical callee at each site — verified by COFF relocation name):
    `?PostDynamicAdd@GemPlayer@@UAAXXZ` 100.0 · `?Restart@GemPlayer@@UAAX_N@Z`
    100.0 · `?ChangeDifficulty@GemTrack@@QAAXW4Difficulty@@H@Z` 100.0 ·
    `?HandleNewSong@GemTrack@@QAAXXZ` 100.0 ·
    `?Handle@MetaPerformer@@UAA?AVDataNode@@PAVDataArray@@_N@Z` 100.0 ·
    `?SelectVenue@TourPerformerLocal@@QAAXXZ` 100.0 ·
    `?NewObject@CamShot@@SAPAVObject@Hmx@@XZ` 100.0 ·
    `?PostProcsFromPresets@BandDirector@@QAA_NAAPBVRndPostProc@@0AAM@Z` 100.0 ·
    `?Poll@BandDirector@@UAAXXZ` 100.0 ·
    `?Handle@RndPostProc@@UAA?AVDataNode@@PAVDataArray@@_N@Z` 100.0 ·
    `?OnMsg@MusicLibraryNetSetlists@@QAA?AVDataNode@@ABVRockCentralOpCompleteMsg@@@Z` 100.0 ·
    `??0GemManager@@QAA@ABVTrackConfig@@PAVTrackDir@@@Z` **99.92411** (sub-100, must not move either way) ·
    `?AddSongData@BandSongMgr@@…` **99.60123** (DX-patched, structurally unmatchable; must not move) ·
    `??0BandSongMetadata@@QAA@PAVBandSongMgr@@@Z` 99.90909 ·
    `?InitBandSongMetadata@BandSongMetadata@@QAAXXZ` 100.0.
    A wrong identification would DROP the callers of that name; this is how the
    gate can fail.
P8. The anonymous population becomes 6,679 rows / 1,198,508 B (−12 / −30,048).
    NOTE the LoadRev row leaves the `fn_*` population but joins the named-zero
    population (W16-FB's vein) until §3 lands.

## 3. Cascade pre-registered from retail bytes (source, after the map A/B)

C1. `RndPostProc::LoadRev` signature: retail is `(BinStream&, int rev)` (rb3-Wii
    shape), not DC3's `(BinStreamRev&)`. Fix `src/system/rndobj/PostProc.{h,cpp}`
    to the rb3-Wii signature and call `LoadRev(bs, d.rev)` from `Load`. Prediction:
    the row pairs (fuzzy > 0). Its retail callers `0x82433268` (PostProc) and
    `0x82404f80` (Anim) are anonymous, so no caller row can move.
C2. Every other residual is adjudicated on retail bytes per row AFTER the map A/B,
    priced from `report.json`'s charged sites, and recorded in this doc's §4.

## 4. Measured — map-only A/B (run `.ab_measure_runs/20260916-081845-from-dirty-1211668`)

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-fc --from-dirty`, rc=0, log
`~/tmp/w16fc_ab1.log`. Leg A reproduced the campaign baseline to the digit
(43,991 / 4,134,460 / 40.347736 / fuzzy 50.031284). Leg B: renamer patched
1,832 files; both legs at a split fixed point after 0 extra re-splits;
`[control none] FLAT`.

| prediction | measured | verdict |
|---|---|---|
| P1 Δtotal_functions 0 / Δtotal_code 0 | 69,240 → 69,240 / 10,247,068 → 10,247,068 | HIT |
| P2 same size per row | all 12 identical (`fn_` row size == named row size) | HIT |
| P3 SetupGems 90–99.9 | **87.76616** (mpn 89.46416) | **MISS, 2.2 pp below band** |
| P3 TriggerSongCompletion ≥95 | 98.326996 (mpn 98.47909) — did NOT cross | HIT |
| P3 SelectRandomVenue 40–80 | 58.328484 | HIT |
| P3 CamShot::Load 80–99 | 88.58878 | HIT |
| P3 ??0CamShot 80–99 | **78.86777** | **MISS, 1.1 pp below band** |
| P3 RndFont::Load 80–99 | 82.0972 | HIT |
| P3 RndFont::SyncProperty 80–99 | 94.66192 | HIT |
| P3 Interp 80–99 | 84.54627 (mpn 87.01806) | HIT |
| P3 LightPreset::Load 60–92 | 81.27778 | HIT |
| P3 ParseDataResultsIntoSetlists 60–92 | 81.93293 | HIT |
| P3 ??0BandSongMetadata 30–65 | 42.482635 | HIT |
| **P4 LoadRev true name stays UNPAIRED** | 1728 B, fuzzy **0.0**, mpn 0.0, `fn_82430FD0` gone | **HIT — the designed non-mover held: unpaired because no base symbol carries that name (NOT a declined charge)** |
| P5 Δmatched ∈ {0,+1}, Δcode ∈ {0,+1052} | **+0 / +0** | HIT |
| P6 Δfuzzy +0.10..+0.28 pp | **+0.201326 pp** (50.031284 → 50.232610) | HIT |
| P7 15 controls exactly unchanged | all 15 identical (size, fuzzy, mpn), incl. `??0GemManager` 99.92411, `AddSongData` 99.60123/66.07895 | HIT |
| P8 population 6,679 / 1,198,508 | 6,679 / 1,198,508 | HIT (exact) |

Row-level diff A→B: exactly **24** rows differ = the 12 `fn_` rows removed + the
12 named rows added. No other row in the binary moved.

Reading: the two band misses are my priors being ~2 pp optimistic on the two
rows whose retail size differed from ours by only 4 B / 48 B — a small size gap
does not bound the instruction-level divergence. Neither miss changes an
identification: SetupGems' callers (5, all 100.0) and ??0CamShot's caller
(`NewObject@CamShot`, 100.0) all still spell the name and did not move.
