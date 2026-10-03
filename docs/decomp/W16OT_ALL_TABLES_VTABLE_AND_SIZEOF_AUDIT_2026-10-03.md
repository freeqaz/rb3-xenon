# W16-OT — every vtable (secondary, virtual-base, root), slot bodies, sizeof first-member blind spot, FriendRecord = Friend (2026-10-03)

**Branch** `w16-ot`, worktree `~/tmp/wt-w16-ot`, started off main `076f4f760`,
rebased onto `7d4a63494` (W16-OS) mid-lane and onto `99ee35830` (W16-OU) before the A/B.
The sizeof leg ran as a forked sub-lane (`w16-ot-sz`), merged with `--no-ff`.
**Scope** classes defined in `src/band3/**` and `src/system/**`, minus any
class also defined in `src/network` (Quazal untouched).
**Ruler** shipped `name_check` (report.json `provenance.diff_config`).

W16-OP and W16-OR audited primary vtables and object sizes. They left five
gaps, and this lane closes them:

1. secondary (multiple-inheritance) and virtual-base tables;
2. the tables `vtable_override_pattern.py` could not classify (`no_base`,
   `parent_no_primary`, `ours_unreadable`);
3. `vtable_order_sweep` on the engine directories;
4. `retail_sizeof_witness.py`'s first-member blind spot;
5. `AppLabel::FriendRecord` vs `os/Friend`, which blocked `FriendsProvider::Text`.

## 0. Result

| leg | instrument | population | real defects found → fixed |
|---|---|---|---|
| all tables: override pattern | `vtable_override_pattern.py --all-tables` (new) | 1,812 tables: 811 primary + 842 secondary compared, 28 via an ancestor | RETAIL_OVERRIDES **25 → 0**; OURS_OVERRIDES 79 → 78 (all fold-consistent) |
| all tables: pure slots | same, PURE check | every slot of every table | **6 → 0** (StorePanel) |
| all tables: slot bodies | same, BODY check (new) | 8,922 slots whose retail body is a tiny leaf | **554 → 0** (14 distinct defects) |
| all tables: slot count | same, COUNT check | every table | 1 → 1 (OggMap, deferred, §5) |
| order + count | `vtable_order_sweep.py --sweep` | 1,779 in-scope tables (949 classes) | PERMUTED 0 / SET_DIFFER 0 before and after; 9 engine withheld slots adjudicated (§4) |
| FriendRecord | retail `FriendsProvider::Text` | — | `FriendRecord` **is** `Friend`; Text, Handle, OnMsg, comparator now defined and paired |
| sizeof | `retail_sizeof_witness.py` (MEMBER0 fix) + new per-TU probe census | 169 newly witnessed engine classes; 506 re-run | **0** (169/169 and 503/506 agree; the remainder are known witness limits) |
| extra | BODY's "ours has no body" list | 47 slots | `GetVelocityBucket` / `GetVirtualSlot` / `User::IsNullUser` had no definition at all → defined |

A/B: §7.

## 1. `vtable_override_pattern.py --all-tables` (new mode)

The primary pass is unchanged. The new mode judges **every** retail table of a
scoped class.

- **Comparator.** The comparator is the base whose subobject the table belongs
  to. For a non-virtual base it is the base whose `mdisp == COL.offset`; for a
  virtual base it is the single virtual-base group, or the group our own COFF
  `??_7C@@6B<Base>@@@` suffix names.
- **Ancestor fallback.** If the comparator has no table on *either* side, the
  tool walks down its primary-base chain inside the class's own Base Class
  Array (same subobject) to the first ancestor both sides emit, and compares
  the shared prefix. This covers W16-OR's `parent_no_primary`.
- **Checks.** OVERRIDE (slot 0 skipped) as in the primary pass; **PURE**
  (`0x828299B8` xor `_purecall`) and **COUNT** on every table, roots included
  (which covers `no_base`).
- **BODY.** Wherever retail's slot body is a tiny relocation-free leaf (≤ 4
  words ending `blr`, no branch, no `lis`), our compiled slot body must be
  byte-identical. This is the one check that reaches a base class's **own**
  new virtuals, which no comparator can cover. It also compares a getter's
  load offset.
- **Name alias.** Retail's `.?AVObjRef@@` is the class our tree calls
  `ObjRefOwner` (§5). That alias brought NO_COMPARATOR from 11 to 4.

Baseline over the scope: `RETAIL_OVERRIDES 25, OURS_OVERRIDES 79, PURE 6,
COUNT 1, BODY 554, NO_COMPARATOR 11, OURS_NOT_EMITTED 20`.
Post-fix: `RETAIL_OVERRIDES 0, OURS_OVERRIDES 78, PURE 0, BODY 0, COUNT 1,
NO_COMPARATOR 4, OURS_NOT_EMITTED 19`.

## 2. Defects found and fixed (all on retail bytes)

### 2.1 BODY (retail slot body is a tiny leaf, ours differs)

| class:slot | retail | ours (was) | fix |
|---|---|---|---|
| `Hmx::Object`:14 `Export` (inherited by **524** tables) | `blr` (empty) | `if (b) HandleType(a)` + sink export, 72 B | empty in the match build; DC3 body `HX_NATIVE`-only. rb3-Wii: `virtual void Export(DataArray *, bool) {}` |
| `Synth`:25 / `Synth360`:25 | `blr` in both | `bool IsUsingDolby() const` (`li r3,0` / a 44 B `XAudioGetSpeakerConfig` body) | slot 25 is rb3-Wii's `virtual void SetMono(bool) {}`. Synth360's override removed. Retail ProfileMgr calls `SetDolby` at `0x60` (slot 24), so the header's `// 0x64` comment was wrong. Nothing calls slot 25 through `TheSynth` in retail. |
| `XboxEnumeration`:3 `IsSuccess` | `lbz r3,0x1c(r3)` | read `+0x24` (inside `mOverlapped`) | `return mEnumerating;` (Start sets `0x1c` = 1) |
| `CamShot`:10 `CurrentShot` | `blr` (= `this`) | `return nullptr` | `return this` |
| `PlayerLeaderboard`:31 `CanRowsBeSelected` (+3 subclasses) | `li r3,1` | `return false` | `return true` |
| `TrackWatcherImpl`:35 `HitGemHook` (+2 subclasses) | `blr` (f1 = `ms` unchanged) | `return 0.0f` | `return ms` |
| `PrefabChar`:5 `IsCustomizable` | `li r3,0` | tail call to PrefabMgr's dev toggle | `return false` |
| `RndShaderVelocityCamera`:3 `CalcShaderOpts` | `li r3,0` | DC3 HiResScreen bit | `return 0` (its siblings had already lost the same term) |
| `RndShaderStandard`/`RndShaderFur`:1 `CheckError` | `li r3,1` | inherited `return false` | `return true` (DC3 agrees) |
| `FxSendPitchShift360` / `Reverb360` / `Synapse360` FxSend360-table:2 `IsStandard` | `li r3,0` | inherited `return true` | `return false` |
| `PreloadPanel` (+`BandPreloadPanel`) Callback-table:11 `ContentDir` | `li r3,0` | Callback's `"."` | `return 0` |
| `BeatMatchController`:31 / :32, `User`:28 | `li r3,0` / `mr r3,r4` / `li r3,0` | declared, **no definition anywhere** (undefined externals behind the vtable) | `return 0` / `return slot` / `return false`. Native's out-of-line `IsNullUser` stub removed. |
| `Debug`:1 `Print`, `CharClipSet`@0xa0:6, `CharacterTest`:1 / `VocalPlayer`@0x300:1 `UpdateOverlay`, `SynthEmitter`@0xb4:5/7/8, `RndMultiMesh`/`DxMultiMesh`/`WorldCrowd`:9 `CollideList` | empty / `li r3,0` / bare `blr` | debug-print sinks, edit-mode preview and collision, debug overlays | match build gets retail's shape via `LOADMGR_EDITMODE` or an `HX_NATIVE` body; the native build keeps the feature |

### 2.2 PURE

`StorePanel` slots 15, 16, 17, 19, 21, 27 (`IsSongInLibrary`, `ExitStore`,
`StoreUser`, `FindOffer`, `GetOfferIDsToEnumerate`,
`StoreUserProfileSwappedToUser`) are `_purecall` in retail's own StorePanel
table. Made pure and their default bodies removed. `BandStorePanel` (the only
subclass, no `NEW_OBJ` on StorePanel) overrides all six.

### 2.3 RETAIL_OVERRIDES (retail has a distinct body, we inherited)

| class:table:slot | retail | fix |
|---|---|---|
| `FxSendPitchShift360` / `FxSendReverb360` primary:22/26/27 | the FxSend360 forwarders, ICF-shared with Distortion360 / Flanger360 (same base offset) | `Recreate`/`UpdateMix`/`OnParametersChanged` forwarders declared |
| `SampleInst360`:32/33/34 | `0x82B6E138` `mVoice->SetSend(dynamic_cast<FxSend360*>(s))`; `0x82B6E190`/`198` `lwz r3,0x54; b Voice::SetReverbMixDb/Enable` | defined; named in the map (own pin) → **3 rows at 100** |
| `RndMeshAnim`:6 `AnimTarget` | `0x82471CC8`, ICF-shared with RndTransAnim's, reads `mMesh.mPtr` | `return mMesh` |
| `RndPollAnim` RndAnimatable-table:1/2/3 | one vtordisp thunk (`0x8234EBC8`) onto the empty-body fold | `StartAnim`/`EndAnim`/`SetFrame` overridden empty. `SetFrame` being a no-op is real behaviour; RndAnimatable's own is not empty. DC3 and rb3-Wii agree. |
| `CharClipGroup` Object-table:2 `Replace` | vtordisp `0x8238FB08` → `0x8238F270`: swap the matching clip for `dynamic_cast<CharClip*>(to)`, drop a null, fix `mWhich` with `Min(0, s-1)` | ported, named → **2 rows at 100** (body 288 B + thunk) |
| `CharIKSliderMidi` / `CharSleeve` Object-table:16 `SetName` | vtordisp → `Object::SetName` then `mMe = dynamic_cast<Character*>(dir)` (`mMe` at 0x90 / 0x64) | ported (rb3-Wii has both). CharIKSliderMidi named → **2 rows at 100**. CharSleeve's body is pinned inside CharDriver.cpp's range, so it was not named (re-home not done). |
| `RndLightAnim` Object-table:13 `Print` | vtordisp → `0x824715A8` light / keysOwner / colorKeys | ported. The map named `0x824715A8` `??6@YAAAVTextStream@@AAV0@PBVObject@Hmx@@@Z`, which is wrong: `operator<<(TextStream&, const Object*)` is inline in Object.h and has no out-of-line copy. Renamed → **18.8 → 100** |
| `FriendsProvider` Object-table:6 `Handle` | `0x826664A8`: `HANDLE_MESSAGE(PlatformMgrOpCompleteMsg)`, `Object::Handle`, `HANDLE_CHECK` tail | §3 |

### 2.4 OURS_OVERRIDES (we override, retail reuses the parent's address)

79 rows. W16-OP/OR had adjudicated 40 of them. For the rest, our C body was
compared against our P body mechanically:

- **64 IDENTICAL** (fold-consistent; includes the 3 Interpolator `??_E` rows,
  whose `??_G`s store only the base vtable and fold). The new row is
  `Hmx::Object`:3 `IsDirPtr` vs retail `ObjRef`; both are `li r3,0`.
- **Real defects (now fixed):**
  - `DOFProc::Handle` (500 B of DC3 set/unset handlers; retail's slot is
    `Object::Handle`) → `HX_NATIVE`-only.
  - `MiniLeaderboardDisplay::DrawShowing` → retail folds it with App's 96 B
    body, so there is no null branch.
  - `UIListProvider::Text` → retail `0x828012E0` is the bare `SetTextToken(gNullStr)`
    shared with `ViewSetting::Text`; the edit arm became `LOADMGR_EDITMODE`.
  - The editor/overlay rows of §2.1.

## 3. `AppLabel::FriendRecord` is `os/Friend` → FriendsProvider completed

Retail `FriendsProvider::Text` (`0x82665FD8`):

1. `dynamic_cast<AppLabel*>(label)`;
2. `slot->Matches("name")` → `SetFriendName(mFriends[data])`;
3. `"game"` → `SetFriendBandName(mFriends[data])`.

`mFriends` is the `vector<Friend*>` that `InviteFriend` hands to
`NetSession::InviteFriend(Friend*)` and `Mat` reads `mOnline` from, so the
setters take `const Friend*`. The record's `mBandName` at `+0x10` is
`Friend::mGame` (slot `"game"`). These are the only retail callers (asm
search: AppLabel.s and FriendsProvider.s only).

- `FriendRecord` was deleted.
- The two map rows were renamed to `?SetFriend[Band]Name@AppLabel@@QAAXPBVFriend@@@Z`
  (100 → 100 under the new key).
- The alias-group survivor for `0x825C62F0` was relabelled with
  `tools/alias_survivor_relabel.py --write`, as W16-OS's new drift gate
  requires. The old spelling is held as an UNDECIDABLE record.

The rest of the TU:

- **`OnMsg(const PlatformMgrOpCompleteMsg &)`** (`0x82666440`, 92 B):
  `std::sort(mFriends, FriendCmp)`, then `return 1`.
- **`FriendCmp`** (`0x82665F00`): online first, then
  `AlphaKeyStrCmp(name, name, false) < 0`.
- **`Handle`**: see §2.3. It needed `HANDLE_CHECK`, the comma-form tail that
  keeps `PathName(this)`; without it the row read 88.2.
- **Re-home.** `0x8266643C–0x82666648` (OnMsg, Handle and its 3 EH funclets)
  was pinned to **BandLabel.cpp**, which defines none of it. It is now pinned
  to FriendsProvider.cpp. W16-OR's "BandLabel starts at 0x8266643C" was
  wrong: BandLabel's code is at `0x82340580…`, and that block was a stray pin.

Row results: Text, FriendCmp and Handle are at 100, and the 3 funclets stay at
100 in the new unit. OnMsg reads 99.78: its one charged site is the `sort`
callee, whose retail survivor `0x822BED18` is spelled `sort<RndPollable**>`.

**Fold bait, measured and reverted.** I first named the two STL helpers
`0x826660F0` (`__make_heap`) and `0x82666290` (`__insertion_sort`) for
`Friend**`. Both are single ICF bodies shared by every `T**` + function-pointer
sort. Naming them charged Trans's `__final_insertion_sort` / `__partial_sort
<RndTransformable**>` (100 → 99.63 / 99.87) and bought nothing, because our
`Friend**` rows stayed below 100. They are anonymous again (commit `2843b9067`).

## 4. `vtable_order_sweep` on the engine directories

- **Whole binary:** 2,220 retail tables, **PERMUTED 0 / SET_DIFFER 0**, before
  and after.
- **In scope** (1,779 tables / 949 classes): SAME 1030 → 1031, UNRESOLVED
  749 → 748.
- **Count mismatches** are only W16-OP's 11 not-emitted tables (compiler-checked
  there), plus `ObjRef` (§5) and `OggMap` (§5).

The engine "withheld" slots (retail map name is a non-virtual) were adjudicated
on bytes:

| class:slot | retail body | ours | verdict |
|---|---|---|---|
| `MemStream`:4 | `lbz r3,0xc` | identical | map-name fold |
| `RndLine`:12 | `lwz r3,0xe8` | identical | map-name fold |
| `MemcardXbox`:27 | `addi r3,r3,0x52` | identical | map-name fold |
| `RandomIntervalGroupSeqInst`:22 | `lbz r3,0x68` | identical | map-name fold |
| `RealGuitarController`:39 | 4-word indexed load | identical | map-name fold |
| `NgDOFProc`:27 | `lfs f1,0x44` | identical | map-name fold |
| `MultiChannelMapping`:3 | 5-word size | identical | map-name fold |
| `CharSleeve`:16 | vtordisp → own `SetName` | fixed (§2.3) | real |
| `WorldInstance`:2 | vtordisp{0} → adjustor `subi r3,0x14` → `RndDir::Replace` | one combined `$4…@BE@` thunk → `RndDir::Replace` | same final overrider; thunk shape only |

## 5. Found, not fixed (with reasons)

- **`ObjRef` naming.** Retail's polymorphic ref base is `.?AVObjRef@@`:
  `Hmx::Object`'s only base, with 4 slots (dtor, 2 pure, `IsDirPtr`→false).
  Our tree calls that class `ObjRefOwner` and uses `ObjRef` for the
  non-polymorphic ring node. Renaming changes mangled names tree-wide, so it
  was not done. The tool carries the alias.
- **`OggMap`.** Retail RTTI gives it a base `OggValidatorFileSource`, and its
  table has 2 slots:
  - slot 0 is `fn_82BB1980` (516 B), `int <fill>(int size)`. It reads from the
    BinStream at `+0x28`. When a global flag at `0x82E4C165` is set it
    `ctr_encrypt`s with the `symmetric_CTR` at `0x82E4BE40`, then writes into
    the MemStream at `+8`;
  - slot 1 is `??_G`.

  This is TU5 / DX-lineage code with no oracle in either repo, and the crypto
  globals have no owner identified in our tree. Declaring the base without the
  body would make OggMap abstract, so it is deferred whole.
- **`EnterFlowMsg` / `JoinEntryPointEvent`.** Retail RTTI puts them in
  WaitingUserGate.cpp's anonymous namespace (`?A0x5b3730ba`, shared with
  OpenGateData); ours declares them global. Their 8 rows pair at 100 under the
  global names, so moving them is a naming bet with no behaviour change. It
  was not done.
- **Duplicate anon-namespace classes.** Retail has one `MainHubAdvanceMsg` and
  one `KickPlayerMsg` table; we compile two TU-local copies of each. One copy
  is presumably dropped by `/OPT:REF` in retail, so this is not provable as a
  defect.
- **Remaining NO_COMPARATOR (4):** `DxShaderInclude`/`ID3DXInclude` (XDK
  interface), `soundtouch::FIFOProcessor`/`FIFOSampleBuffer` (vendor), and
  `OggMap`. All are judged by PURE/COUNT/BODY.
- **FriendsProvider residuals.** `OnMsg` and our `Friend**` sort helpers each
  carry one fold-alias callee name. That is alias work, not source.

## 6. sizeof leg (forked sub-lane, merged as `dccf7726c`)

### 6.1 The first-member blind spot is closed (`retail_sizeof_witness.py`)

**Discriminator, from retail bytes.** FileCacheEntry's ctor (`0x825185F8`)
stores the **same** `FilePath` vtable at `this+0` **and** `this+0xc`. A
complete `T` cannot contain another complete `T`, so `sizeof(T) <= d < N`: the
allocation is an aggregate whose first member is `T`.

**Rule.** The ctor's own stores and the inline follower (which now records
every vtable store onto the allocated pointer) are checked for a store at
`0 < d < N` of a complete-object vtable (COL offset 0) that is either:
- the witnessed class itself, or
- a class whose RTTI holds the witnessed class as a base at offset 0.

Such a witness is `how="MEMBER0"`. It is kept in the JSON and excluded from
the size census and from CONFLICT. The COL-offset-0 guard stops a class's own
secondary or virtual-base table at `this+d` from firing the rule.

**Selftest and control.**
- Selftest: PreloadPanel {164: 1}, 925 witnesses. FilePath now has no size;
  both FileCacheEntry ctors read MEMBER0.
- The selftest can fail: `--no-member0` exits rc=1 with 3 FAILs.

**Whole-binary effect.** Exactly 2 witnesses / 1 class flip (FilePath).
CONFLICT stays at 1 (`NetSavedSetlist`, W16-OP's if/else-join limit).

**Residual.** An aggregate with a *single* polymorphic first member is still
attributed to the member. Three cases exist, and each enclosing struct's
compiler sizeof equals retail's allocation:

| witnessed as | retail | enclosing aggregate | ours |
|---|---:|---|---:|
| `ObjDirPtr<ObjectDir>` | 24 | `MidiInstrumentMgr` | 24 |
| `ObjPtrList<Fader>` | 24 | `FaderGroup` | 24 |
| `ObjPtr<RndTex>` | 20 | `Rnd::CompressTexDesc` | 20 |

### 6.2 Census against the compiler (`tools/sizeof_probe_census.py`, new)

The tool probes `sizeof`/`__alignof` inside a TU that really compiles the
class's header, using that TU's build command, so per-TU `/D` flags apply.
That removes W16-OR's header-only-screen caveat.

- It tries up to 4 candidate TUs per class.
- It voids any probe line carrying an error other than C2079; otherwise an
  undeclared name reads `N=0`, which produced one false DxLight DIFFER before
  the fix.
- **Control:** 8 random AGREE classes re-measured with `class_layout_report.py`
  all match, including the virtual-base classes CharMirror and CharIKFingers.

| dirs | witnessed | AGREE | defects |
|---|---:|---:|---:|
| rndobj 55, char 52, synth_xbox 18, world 16, rnddx9 13, net 10, math 3, midi 1, movie 1 | 169 | 169 | **0** |
| + 16 template / anon-namespace classes probed by hand | 16 | 13 + 3 MEMBER-residuals | 0 |
| re-run over W16-OP/OR's dirs | 506 | 503 (+1 known conflict, 2 no TU, both already settled) | 0 |

**Verdict changes vs W16-OP/OR:**
- The 11 virtual-base classes W16-OP had to resolve by hand (`*Panel`,
  BandMatchmaker) now AGREE directly.
- FilePath leaves the population as MEMBER0.

**Result: no header is proven wrong by an allocation size anywhere in
src/band3 or src/system.**

## 7. A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-ot-ab --patch <git diff
main w16-ot -- src config scripts native>`, fresh `setup_worktree.sh` at main
`99ee35830` (post-rebase). Patch kinds: map, source, splits (+ alias
relabel). Run dir
`~/tmp/wt-w16-ot-ab/.ab_measure_runs/20261003-054258-w16ot_branch-3179504/`.

```
leg A: matched=53269 masked=25144 honest=28125 code%=57.384968  (recompiles: 0, settled)
leg B: matched=53280 masked=25144 honest=28136 code%=57.398586  (recompiles: 1011, split=1, patch_steps=7, settle iterations: 2)
split fixed point: leg A converged after 0 extra re-split(s), leg B after 0
Δmatched=+11  Δmasked_equal=+0  Δhonest=+11  Δcode%=+0.013618pp  Δcode_bytes=+1396
Δfuzzy=+0.014124pp   (legA 63.571390 -> legB 63.585514)
units at 100% [mpn ruler]: 524 -> 527 (BandLabel DENOMINATOR_SHRANK; CharIKSliderMidi, SampleInst360 MATCHED_ROSE; 0 fell off)
units at 100% [all-rows-fuzzy ruler]: 463 -> 465 (CharIKSliderMidi, SampleInst360)
[control none] Δmatched_code=+1448 B -- NOT_APPLICABLE (kinds=map,source,splits)
```

**Prediction, written before the run**, from a row-level diff of this tree's
report against main's: **+11 fns / +1,396 B**.

| rows | bytes |
|---|---:|
| CharClipGroup `Replace` + vtordisp thunk | +300 |
| CharIKSliderMidi `SetName` + thunk | +128 |
| SampleInst360 `SetSendImpl` / `SetReverbMixDbImpl` / `SetReverbEnableImpl` | +100 |
| FriendsProvider `Text` + `FriendCmp` + `Handle` | +564 |
| RndLightAnim `Print` (was the wrong `??6` name at 18.8) | +264 |

**Measured: exactly +11 / +1,396 B.**

The rest of the patch moves no row, and none was predicted to:
- the 554 BODY fixes change only vtable data, ICF-folded hub bodies or
  native-only behaviour;
- the sizeof sub-lane changed tools only.

**Unit level.** `BandLabel` −3 matched / −5 rows is the re-homed FriendsProvider
block leaving it, and it reaches 100% by denominator shrink. FriendsProvider
+6 (14 → 20 of 23).

**Row level.** The archived leg reports were keyed `(unit, symbol)`, with a
row missing from leg B counted as down so the check can fail.

- **0 rows down in place, 0 rows up in place.**
- All 17 leg-A keys absent from leg B were re-keyed by address (`fn_<addr>`
  directly, named rows through each leg's map). Every one is present in leg B
  at an equal or higher fuzzy/`mpn`: 12 rows 0 → 100, the AppLabel pair
  100 → 100 under the Friend spelling, `fn_826665D0` 99.5 → 100, and `OnMsg`
  0 → 99.78.
- The first version of this re-key read a key absent from these reports
  (`metadata.virtual_address`), and reported all 17 as down. The join, not
  the data, was wrong.

**Rows that went down during the lane, and were reverted before the A/B:**
Trans `__final_insertion_sort` / `__partial_sort<RndTransformable**>`
(100 → 99.63 / 99.87). The cause was naming the folded STL helpers for
`Friend**` (§3).
