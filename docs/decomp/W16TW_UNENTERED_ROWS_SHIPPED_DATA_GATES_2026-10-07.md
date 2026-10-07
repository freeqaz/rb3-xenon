# W16-TW: Locale::Init, FileMakePath, SaveObjects, JoypadPollCommon, WahEffect::Process and FindXfm, gated natively on shipped data (2026-10-07)

Branch `w16-tw`, rebased on main `089bc8f7a` (W16-TX) before the X360 A/B. Not merged or
pushed.

## 0. Headline

This lane continues `CAMPAIGN_STATE_2026-10-07c.md` §6 lever 2 from where W16-TS stopped
(`W16TS_UNENTERED_ROWS_SHIPPED_DATA_GATES_2026-10-07.md` §8). That lever covers the 69
in-scope behaviour-class rows (49,512 B) that the native build compiles but no target
enters. W16-TS took 9 of them; 60 were left.

I took the 20 largest of those 60: W16-TS's ranked list #2–#22, without #8, which W16-TS
had already taken. The list is the one in the W16-TS lane transcript: rank, bytes, fuzzy,
class, symbol, file.

Every gated row runs in `rb3-render`'s default mode, in `native/src/w16tw_phase.cpp`. The
phase runs after W16-TS's, and `--no-w16tw` turns it off. Each check compares against a
reference computed without the code under test: either the shipped file read directly,
or a retail rule read off the target asm and written down.

| # | row | B | fuzzy | outcome |
|---:|---|---:|---:|---|
| 2 | `JoypadPollCommon` | 2,468 | 94.21 | **gated**, 4 `tw-joy-*` |
| 3 | `CharKeyHandMidi::Poll` | 2,236 | 92.84 | recorded reason (§6) |
| 4 | `BandWardrobe::LoadMainCharacters` | 1,776 | 99.79 | recorded reason (§6) |
| 5 | `GemTrack::DrawFill` | 1,652 | 88.80 | recorded reason (§6) |
| 6 | `NextSongPanel::FillExpandedDetails` | 1,640 | 99.76 | recorded reason (§6) |
| 7 | `LayerDir::RefreshLayer` | 1,588 | 98.23 | recorded reason (§6) |
| 9 | `Locale::Init` | 1,304 | 96.81 | **gated**, 3 `tw-locale-*` |
| 10 | `BandPatchMesh::FindXfm` | 1,268 | 77.97 | **gated**, 3 `tw-xfm*` |
| 11 | `PerfectSectionTracker::HandleExitExtent` | 1,256 | 97.23 | recorded reason (§6) |
| 12 | `UsbMidiGuitar::Poll` | 1,244 | 99.90 | recorded reason (§6) |
| 13 | `Song::SyncState` | 1,192 | 96.72 | recorded reason (§6) |
| 14 | `OutfitConfig::SetSkinTextures` | 1,192 | 95.07 | recorded reason (§6) |
| 15 | `DirLoader::SaveObjects(BinStream&, ObjectDir*)` | 1,044 | 98.01 | **gated**, 12 `tw-save-*` |
| 16 | `VocalTrainerPanel::CopyTubes` | 944 | 99.58 | recorded reason (§6) |
| 17 | `BandPatchMesh::WorkVerts::TryAddFace` | 824 | 90.96 | recorded reason (§6) |
| 18 | `FileMakePath` | 792 | 94.70 | **gated**, `tw-makepath` |
| 19 | `BandRetargetVignette::EnterDir` | 772 | 99.48 | recorded reason (§6) |
| 20 | `WahEffect::Process` | 760 | 91.76 | **gated**, 4 `tw-wah-*` |
| 21 | `BandPatchMesh::WorkVerts::ExtendTwin` | 716 | 85.23 | recorded reason (§6) |
| 22 | `NetCacheMgr::AddLoaderRef` | 696 | 97.84 | recorded reason (§6) |

Of the 20 rows (25,364 B), 6 are gated (7,636 B) and 14 have a recorded reason
(17,728 B). The 40 rows below #22 were not taken (§8).

- **No gate found a behaviour difference from retail.** All 27 W16-TW gates pass:
  `RESULT: ALL GATES PASSED (0 gate failure(s))`, 190 gates in the run.
- **Every gate on a function under test was shown to fail on a broken body** (§4). Two
  sabotage builds put in 8 defects, with predictions written before each run. 15 gates
  failed, all 15 predicted, and no gate outside W16-TW failed.
- **Three native bugs fixed, found by running the gates** (§5):
  - `BinStream` wrote `long`/`size_t` as 8 bytes. So `bs << list.size()` put a 0 in
    every retail count field, in about 25 writers.
  - `MemStream` aborted on a 0-byte access at the end of its buffer.
  - The native `TypeProps::Save` arm spun forever on a shipped milo.

  All three are native only.
- **Two native findings, recorded and not fixed** (§5.4):
  - `rb3-render`'s config is read without retail's `PreInitSystem` macros, so its joypad
    section has no `controllers` block.
  - Retail cannot run `FindXfm` on a mesh whose shipped verts are compressed.
- **X360 A/B: Δ0 on every key**, as predicted (§7).

## 1. The references

- **`Locale::Init`.**
  - The phase installs the section retail's `InitSystem` merges from `band_keep`:
    `(locale ../ui/locale/eng/locale_keep.dta)`. rb3-render reads only the preinit
    config, whose `(locale)` is empty.
  - The reference is a `DataReadFile` of the same file, built into a map in which the
    last definition of a symbol wins. Retail `fn_827C9AF8` walks files and entries in
    descending order and sorts with `FastSort<3>`, so the last definition wins there
    too.
  - The gate checks: every entry's string, the table's ascending order (which
    `FindDataIndex`'s binary search needs), no upload flags set, an absent token gives
    null, a null token gives `''`, `mFile`, and that `Terminate` frees everything.
  - Afterwards the original section and `gLocaleIdx` are restored.
- **`FileMakePath`.**
  - The reference is a 17-case table plus 2 cases where an argument aliases the static
    buffer. The rules were read off retail `0x82516B10`:
    - A component that starts with `.` is dropped unless it is exactly `..`.
    - `..` pops the previous component unless the stack is empty or its top starts with
      `.`.
    - The drive prefix is kept, and backslashes become `/`.
    - The result is lower-cased.
    - An empty result is `.`, or `/` when the input began with a slash.
  - The table includes `("a", "..x/y") -> "a/y"`, the case that tells `..x` apart from
    `..`.
- **`SaveObjects`.**
  - Four shipped milos are decompressed by the gate itself (zlib, honouring the stored
    bit 24 of each chunk size): `ui/gen/colors_default`, `ui/resource/gen/color`,
    `ui/resource/gen/star_display` and `ui/main/gen/attract_overlay`.
  - Each is loaded and re-saved, and the re-save is compared to the shipped header and
    bytes. Shipped milos were written by retail-era `SaveObjects` at rev 28, with the
    object list in `ClassAndNameSort` order.
  - For `attract_overlay` the expected stream is the shipped one with ObjectDir's load
    rule applied: `if (!mCurCam && mCurViewportID == 7) mCurViewportID = 0`. The camera
    `[ui.cam]` does not resolve in a standalone load, so viewport 7 becomes 0 and the
    camera string becomes empty. Those are the only 8 bytes that differ.
- **`JoypadPollCommon`.**
  - The fixture rebuilds the joypad section the way the console does. Retail
    `PreInitSystem` (`0x82510BB8`) defines `HX_XBOX`, `HX_WIN`, `HX_NG` and `_SHIP`
    before reading `band_preinit_keep.dta`.
  - That file's `(joypad #joypad.dta)` is the game's `config/joypad.dta`. Its trailing
    `#merge` of `system/run/config/default.dta` then `DataMergeTags` the system
    `joypad.dta` into it, so the game's own tags win.
  - The phase does the same under those macros, restores them, and hands the array to
    `JoypadInitCommon`. `gSystemConfig` is untouched.
  - Correction to the triage: on the console the merged `ignore` list is the game's
    **empty** one. The system file's `(ignore 1 2 3 4 5 6 7)` loses the merge, so pads
    1–3 are not ignored.
  - Expected actions come from the shipped `button_meanings` for the controller type the
    shipped `controllers` entry names: type 6 → `strat_xbox_rb2`, type 8 →
    `hx_drums_xbox`, which has `is_drum 1` and `cymbal_mask 8`.
  - The ordering rules and the EEPROM packet layout were read off retail `0x82526A00`:
    - Messages go out bit by bit, ups before downs.
    - Connect comes after the buttons, and disconnect after the releases.
    - With the whole cymbal mask held, DUp/DDown downs are dropped.
    - The EEPROM sequence is a header `AD DE 00 00 55 AA 55 AA`, then data packets
      `[offset, 0, total, len, bytes…]` with the tail zeroed. An unanswered write times
      out from state 1 to 4.
  - The back end is a strong `ReadSingleJoypad`/`requestBreedWrite` in the phase TU. It
    overrides the weak "no pad" stubs in `dta_link_stubs.s`, which is why the row was
    never entered, and it stays inert unless the phase scripts it.
- **`WahEffect::Process`.**
  - No shipped data reaches it: no `FxSendWah` exists in any of the 4,455 shipped milos.
    The parameters are the retail ctor's, and the input is a fixed synthetic signal.
  - Each check is a consequence of retail `fn_82BB6578`:
    - **Per-channel state:** the stack arrays at 0x50/0x58 are indexed by channel, so a
      silent right channel stays exactly 0 and the left channel equals a mono run bit
      for bit.
    - **Phase step:** with sweep rate below 0, the step is resonance × 1.308997e-4
      (`lbl_821A1C74`), accumulated per sample and wrapped once by 2π (`lbl_820498E8`).
      Checked after 1,000 frames, and after 40,000 frames, which crosses 2π.
    - **Soft clip:** `(1+k)·y/(1+k|y|)` with `k = 2r/(1−r)`, so |out| < (1+k)/k.
    - Silence in gives silence out.
    - A gain below 1 is stored back as 1.
- **`BandPatchMesh::FindXfm`.**
  - Its only caller is `ProjectPatches`, for a placed patch with `mTexture != -1`.
    Placements are profile data, so nothing reaches it natively.
  - The gate drives it directly on two shipped meshes from
    `char/main/torso/female/gen/baseballtee_10k.milo_xbox`:
    - `baseballtee_resource_patch.mesh` stores its 173 verts **compressed** (the `b58`
      flag in `RndMesh::LoadVertices`, with `keep_mesh_data` 0). Retail's loader then
      leaves `mVerts` empty and keeps only the GPU blob, so retail's `FindXfm` must
      take its "no verts" early-out: it returns false and leaves the transform
      untouched. That mesh is most likely the `mTexture == -1` mapping placement, which
      never reaches `FindXfm`, so it is used here only as a shipped compressed mesh.
    - `female_tattoo_head.mesh` stores 2,352 plain verts and 4,612 faces. I did not
      trace which shipped patch names it.
  - The reference is plain geometry computed by the gate:
    - The face is the first one, in face order, whose UV triangle holds the point.
    - Position and normal are the barycentric interpolation, with the normal normalised.
    - `|m.x|` and `|m.y|` are half the lengths of the affine map's ∂P/∂u and ∂P/∂v, and
      m.x, m.y and m.z are mutually orthogonal.
    - Points just outside a UV boundary edge use the face that owns the unique nearest
      edge point.
  - Samples: the centroids of the 64 largest-UV-area faces, plus 8 outside points.

## 2. Gates

| gate | checks |
|---|---|
| `tw-makepath` | 17 table cases + 2 aliasing cases, 0 wrong |
| `tw-locale-fixture` | section installed; `locale_keep.dta` 13,001 entries, 0 redefined, 0 malformed |
| `tw-locale-init` | 1 file, size 13,001, 0 wrong, 0 missing, 0 out of order, 0 upload flags, absent/null tokens, `mFile` |
| `tw-locale-terminate` | size 0, tables freed, 0 files |
| `tw-save-fixture-<m>` ×4 | shipped milo decoded; loaded dir has the shipped object count |
| `tw-save-header-<m>` ×4 | rev 28, class, object count, `ClassAndNameSort` order, end marks = objects + 1 |
| `tw-save-bytes-<m>` ×4 | re-saved stream equals the shipped stream (`attract_overlay`: plus the load rule); 1,509 / 1,346 / 1,425 / 2,983 B |
| `tw-joy-fixture` | merged section built under `HX_XBOX`; types 6 and 8 resolve; `ignore` empty |
| `tw-joy-guitar` | connect; A+B plus an ignored bit with LX 127 and LT 64; hold; release; unplug; idle; pad state |
| `tw-joy-drums` | cymbal held drops DUp/DDown downs; every release reported; DUp alone reported |
| `tw-joy-eeprom` | 10 bytes in 4-byte chunks gives 4 packets; states `123232333333`; timeout 1 → 4 on the third unanswered poll |
| `tw-wah-silence` / `-channels` / `-phase` / `-clip` | as in §1 |
| `tw-xfm-placement` | compressed shipped mesh: `FindXfm` returns 0, xfm untouched |
| `tw-xfm-fixture` | geometry mesh found with plain verts |
| `tw-xfm` | 64 centroids + 8 outside points; worst error 1.1e-3 against a 1e-2 limit |

The fixture gates come first, so a broken fixture fails before a gate on the function
under test reads it.

### 2.1 Precision of `FindXfm`, measured before choosing the tolerance

- **First attempt, failed:** sampling evenly through face order with a 2e-3 limit
  relative to |v| failed on 2 of 64 points.
- **Second attempt, worse:** measuring against the triangle's edge failed on 47 of 72
  points, with a worst error of 11.
- **Cause:** the error is retail's own algorithm, not a difference. `FindXfm` inverts
  the 3×3 UV matrix in float, so precision falls as the UV triangle shrinks. Across all
  72 samples, |UV det| × relative error ≤ 4e-5. The smallest triangles (det ~2e-6) are
  off by up to 11 edges, and the large ones by under 1e-3.
- **Fix:** a gate on badly conditioned faces cannot tell a wrong face from rounding, so
  it now samples the faces with the largest UV area.
  - Worst error is 1.1e-3, and the limit is 1e-2 of an edge.
  - Sabotage S7 shows a real defect reads 19 (§4).

## 3. Link and load closure

- **Joypad overrides.** `ReadSingleJoypad` and `requestBreedWrite` are now strong
  definitions in rb3-render, in `w16tw_phase.cpp`. They return "no pad" unless the
  phase sets `w16tw::gScripted`. No other target links the phase.
- **Phase order.** The phase needs the earlier phases' class registrations. With them
  disabled, `PanelDir` is not registered and the milos fall back to `RndDir`. In default
  mode it runs after W16-TS, and every fixture gate passes.
- **Ranker blind spot.** `FileMakePath` and `JoypadPollCommon` have C linkage, so the
  ranker lists them as "unparsed". Their "never entered" status was therefore never
  proven. Entry is now shown by the sabotage controls.

## 4. Sabotage controls (predictions written before each run, all reverted)

Build 1 (`~/tmp/w16tw_sabotage.diff`, four defects, one function each):

| defect | predicted | measured |
|---|---|---|
| S1 `FileMakePath`: any `..`-prefixed component treated as `..` | `tw-makepath` FAIL, exactly 1 wrong | FAIL, 1 wrong |
| S2 `Locale::Init`: chunk sort skipped | `tw-locale-init` FAIL; fixture and terminate PASS | FAIL: 13,000 missing, 12,211 out of order; others PASS |
| S3 `SaveObjects`: `ClassAndNameSort` dropped | header and bytes FAIL on ≥3 of 4 milos; fixtures PASS | header and bytes FAIL on all 4 (14 / 10 / 3 / 8 out of order); fixtures PASS |
| S4 `JoypadPollCommon`: `ignore_dup_and_down` never set | `tw-joy-drums` FAIL only | `tw-joy-drums` FAIL (1 wrong); fixture, guitar and EEPROM PASS |

Result: `RESULT: FAILED (11 gate failure(s))`. All 11 were W16-TW gates, and no other
phase's gate failed.

Build 2 (`~/tmp/w16tw_sabotage2.diff`, four defects):

| defect | predicted | measured |
|---|---|---|
| S5 `WahEffect`: filter state shared by both channels | `tw-wah-channels` FAIL | FAIL: right channel nonzero in 4,096 samples |
| S6 `WahEffect`: phase step doubled | `tw-wah-phase` FAIL | FAIL: 0.3534 against 0.1767 |
| S7 `FindXfm`: u and v swapped in the final evaluation | `tw-xfm` FAIL; placement and fixture PASS | FAIL: worst 19, 72 of 72 over the limit; others PASS |
| S8 `FindXfm`: the no-verts early-out returns true | `tw-xfm-placement` FAIL | FAIL: returned 1 |

`tw-wah-silence` and `tw-wah-clip` passed, as predicted: neither defect breaks those
properties. Result: `RESULT: FAILED (4 gate failure(s))`.

After each build the diff was reverted, rb3-render was rebuilt, and the run was back to
`ALL GATES PASSED`.

## 5. Bugs found

### 5.1 `BinStream`: `long` and `unsigned long` were 8 bytes on the wire natively

- **Symptom:** every shipped milo re-saved natively came out 4 bytes too long, with 0 in
  its retail object-count field.
- **Cause:** `bs << objects.size()` resolved to `BS_WRITE_OP(unsigned long)`, which
  writes `sizeof(unsigned long)` = 8 on LP64. Retail's is 4, and every stream format
  stores it as 4.
- **Reach:** the same overload serves `std::list`/`std::map` writers in `BinStream.h`
  and about 20 `Save` bodies that write a `.size()`.
- **Fix (`src/system/utl/BinStream.h`, native only):** `long` and `unsigned long` read
  and write 32 bits, sign- or zero-extended on read. The X360 `BS_*_OP(long)` is kept
  under `#ifndef HX_NATIVE`.

### 5.2 `MemStream`: a 0-byte access at the end aborted natively

- **Symptom:** `SaveObjects` aborted in `std::vector<char>::operator[]`.
- **Cause:** `WriteImpl` takes `&mBuffer[mTell]` with `mTell == size()` for a 0-byte
  write, for example an empty `String` body. libstdc++'s checked `operator[]` aborts on
  that. Retail's STLport vector indexes a raw pointer, so it is harmless there.
- **Fix:** `MEMSTREAM_AT(i)` is `mBuffer.data() + i` natively, and the same
  `&mBuffer[i]` on X360. It is used at all three sites.

### 5.3 `TypeProps::Save`: the native DC3 arm hung on `attract_overlay`

- **Symptom:** re-saving `ui/main/gen/attract_overlay.milo_xbox` never returned.
- **Cause:** the native arm was a DC3 editor-era body: an `EditMode` type-check pass
  plus `EditorDir` key stashing. It had three defects:
  - It never advanced its key index past an object-valued key whose object is null or
    has no dir.
  - It wrote kept keys into the type definition's own array.
  - It re-inserted stashed keys after their values.
- **Fix:** RB3 has no editor, so the retail body (`0x82765F40`), which X360 already
  compiled, is now the native body too. The X360 text of that body is unchanged.

### 5.4 Recorded, not fixed

- **rb3-render's joypad section has no `controllers` block.**
  - rb3-render reads `band_preinit_keep.dta` without defining `PreInitSystem`'s macros,
    so `#ifdef HX_XBOX` drops that block.
  - `JoypadInitCommon(SystemConfig("joypad"))` therefore aborts natively with
    `Couldn't find 'controllers'`. That is how this lane's first joypad run died.
  - The gate builds its own section (§1) rather than change `gSystemConfig` under every
    other phase. Defining the four macros in the native boot is a native-wide decision
    and is left to whoever owns it.
- **`Locale` sort helpers compare differently, harmlessly.** `LocaleChunkSortFunc`
  compares pointers unsigned, and `FindDataIndex` compares them signed. They would
  disagree only if symbol addresses straddled 0x80000000. This is noted and not changed.

## 6. Recorded reasons (rows taken, not gated)

- **#3 `CharKeyHandMidi::Poll` (2,236).**
  - The fixture exists: `char/main/rigging/gen/keyboard.milo_xbox` holds Character
    `keyboard_rigging` with `left_hand.keyhand`, `right_hand.keyhand`, `*.ikfingers`
    and `spot_keys_left/right.mesh`, and it loads alone.
  - The reference does not exist yet. Key positions come from the spot transforms
    through retail's constants (1/14, 1/28, −0.4, 0.5, −1) and its black-key table, and
    finger assignment for 1–5 keys comes from a table in the retail body. Both have to
    be transcribed from the asm.
  - Not done in this lane's budget.
- **#4 `BandWardrobe::LoadMainCharacters` (1,776) and #19 `BandRetargetVignette::EnterDir`
  (772).**
  - Both need the full band load: four `BandCharacter`s, a venue
    `BandConfiguration` and `StartClipLoads`.
  - That path runs only under `RB3_BAND_PLACE=1` (X10–X22), not in default mode.
  - With memory short, this lane did not add a full band load to the default run.
- **#5 `GemTrack::DrawFill` (1,652).**
  - It needs W16-TJ's track fixture plus a `GemPlayer` whose `BeatMatcher` answers
    `FillsEnabled`.
  - Without a matcher only the coda arm runs, and neither song checked has a `[coda]`.
  - A lane-made matcher is not shipped data.
- **#6 `NextSongPanel::FillExpandedDetails` (1,640).**
  - It needs a `Player` with `Stats`, `TheGame->IsActiveUser`, `TheBandUserMgr` and
    `MetaPerformer`.
  - The only reference the triage found re-does the panel's paging logic, so it would
    not be independent of the code under test.
- **#7 `LayerDir::RefreshLayer` (1,588).** None of the 4,455 shipped milos contains a
  `LayerDir`. The art-maker milos are `PanelDir`/`RndDir`.
- **#11 `PerfectSectionTracker::HandleExitExtent` (1,256).**
  - The data exists: `config/gen/quests.dtb` `qst_tier1_accuratesectioncount` gives
    required accuracy 0.95 and chain multipliers (1 1.25)(2 1.5), and section bounds
    can come from a song `.mid`.
  - It also needs a `TrackerSource` with local `Player`s and `Stats`, and
    `TrackerSectionManager`. No default-mode fixture provides those.
  - Deferred.
- **#12 `UsbMidiGuitar::Poll` (1,244).**
  - `JoypadPollCommon` calls it (`Joypad.cpp:916`), so the `tw-joy-*` gates enter it.
    It returns at `if (TheGuitar)`, because native never runs `UsbMidiGuitar::Init`.
  - Driving the body needs pro-guitar packets, which no shipped file supplies, plus the
    `ProGuitarData` bitfield layout read off retail asm.
  - Deferred.
- **#13 `Song::SyncState` (1,192).**
  - It needs `HxMaster`/`HxAudio`, which would be lane-made fakes.
  - Native's `mFastSync` async arm differs from retail's busy-wait, so a native gate
    would test a native-only arm.
- **#14 `OutfitConfig::SetSkinTextures` (1,192).**
  - It already runs 42 times on the `RB3_BAND_PLACE=1` path (X21), not in default mode.
  - A default-mode gate needs `char/main/gen/main.milo_xbox` (a `BandCharacter`) plus a
    head milo, and a list of the texture names retail binds, read from the asm.
  - Deferred.
- **#16 `VocalTrainerPanel::CopyTubes` (944).**
  - `songs.dtb` song 20100 points at `vocal_trainer_sample`, which is not on the disc.
  - No shipped mode in `modes.dtb` defines a vocal `begin_token`.
- **#17 `WorkVerts::TryAddFace` (824) and #21 `WorkVerts::ExtendTwin` (716).**
  - They are reached only through `ProjectPatches` with a profile patch placement, and
    X20 measured `PreRender hits=0` on the placed band.
  - This lane adds one fact: a shipped placement mesh can store its verts compressed,
    and then retail's loader keeps no CPU verts for projection (`tw-xfm-placement`).
  - `ExtendTwin` also needs `mRenderTo`.
- **#22 `NetCacheMgr::AddLoaderRef` (696).**
  - It needs `NetCacheMgr` forced to its ready state. Real readiness needs a cache
    mount.
  - Its reference would be retail's list rule (front/back insertion, case-insensitive
    dedupe, refcounts), transcribed from the asm, with shipped data contributing only
    `store.dtb`'s `netcache_init`.
  - The `local` server it would use exists natively only because native does not
    define `_SHIP`; retail does.
  - Deferred.

## 7. X360 A/B

Command: `python3 tools/ab_measure.py --worktree ~/tmp/wt-w16tw --patch <reverse src diff> --jobs 8`.

- **Legs:** leg A is this branch, rebased on `089bc8f7a`. Leg B is main's text of the
  three `src/` paths: `TypeProps.cpp`, `BinStream.h` and `MemStream.cpp`.
- **Prediction: Δ0.**
  - The `BinStream.h` and `MemStream.cpp` changes sit behind `HX_NATIVE` guards. On
    X360 they expand to the same declarations and the same `&mBuffer[i]`.
  - The `TypeProps.cpp` change deletes the native arm and leaves the retail body X360
    already compiled, with only a comment added.
  - The worktree's `build.ninja` has no `/DHX_NATIVE`:
    `command grep -c ' /DHX_NATIVE' build.ninja` = 0.
- **Recompiles:** leg B recompiled 1,078 TUs, because `BinStream.h` is widely included.

```
leg A: matched=54946 masked=25223 honest=29723 code%=59.314053  (recompiles: 0, settled)
leg B: matched=54946 masked=25223 honest=29723 code%=59.314053  (recompiles: 1078, split=0, patch_steps=6, settle iterations: 2)
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.000000pp   (legA 64.315216 -> legB 64.315216)
units at 100% [mpn ruler]: legA 611 -> legB 611  (Δ+0; 0 reached 100, 0 fell off; pairable units 1737->1737)
```

The `none` control was +0 B. The tool restored the tree and verified it (3 paths). This
doc was kept outside the worktree during the run and copied in afterwards.

## 8. Not done

- **The 40 rows below #22 were not taken.** In W16-TS's ranking the largest are:

  | # | row | B |
  |---:|---|---:|
  | 23 | `LayerDir::GetBitmapList` | 692 |
  | 24 | `MetaPerformer::SaveAndUploadScores` | 684 |
  | 25 | `GemTrack::DrawBeatLine` | 684 |
  | 26 | `BandSongMgr::SyncSharedSongs` | 668 |
  | 27 | `SongData::UnflipGems` | 656 |
  | 28 | `BandCrowdMeter::Poll` | 652 |
  | 29 | `MemHeap::Alloc` | 652 |
  | 30 | `PreInitSystem` | 652 |
- **`native_runtime_rank.py` was not rerun.** It needs an instrumented build, and memory
  was short. Entry of the gated rows is shown by the gates: every gate on a function
  under test fails when that body is broken (§4).
- **`WahEffect` with sweep rate ≥ 0 is not checked**, and neither is its `fsel` sweep
  clamp (0.9 / 1.1). No shipped data sets it.
- **The `FindXfm` gate does not check rows of `xfm.m` other than lengths and
  orthogonality.** Their direction comes from `MeshVert::Normalize`'s construction, and
  this lane did not derive an independent reference for it.

## 9. Result lines

```
NATIVE_HEALTH_RESULT verdict=PASS link=PASS link_verified=18 link_expected=18 link_skipped=0 runtime=PASS runtime_ran=18 runtime_total=18 gates_pass=227 gates_fail=0 unrunnable=none selftest=SKIPPED scatter_unlinked=13 scatter_dirb=0 scatter_multihost=12 rc=0 handpose_controls=- handpose_baseline_fail=- runtime_crashed=0 runtime_failed=none
```

`gates_pass=227` is W16-TS's 200 plus this lane's 27, exactly. The scatter counts are
unchanged from W16-TS's line: 13 / 0 / 12.

The native build gate line is in the lane's final report. The gate was run as the last
action, after this doc was committed.

## Reproduce

```
cmake --build native/build --target rb3-render
native/build/rb3-render ~/code/milohax/rb3/orig-assets/xbox-zip <outdir>   # W16-TW phase runs by default
W16TW_XFM_TRACE=1 ...   # per-sample FindXfm errors
W16TW_DUMP=<dir> ...    # dump each re-saved milo stream
tools/native_health.sh <worktree>
tools/native_build_gate.sh
```
