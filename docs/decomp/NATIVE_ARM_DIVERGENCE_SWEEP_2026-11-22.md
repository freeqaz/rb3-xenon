# Native-arm divergence sweep (2026-11-22, lane `narms`)

Goal: find native-only preprocessor arms (`#ifdef HX_NATIVE`, `#ifndef HX_NATIVE … #else`,
`#if defined(MILO_DEBUG) && defined(HX_NATIVE)`) whose behaviour differs from the X360 arm
(the retail-matching one), the class that crashed native boot in `RndDir::Enter` (native passed
`Dir()` as `Hmx::Object*`; `ChainSourceSubdir` then did `reinterpret_cast<MsgSource *>(obj)`
across a virtual base). `RndDir::Enter` and the rest of `src/system/rndobj/Dir.cpp` were left
to session `rbx-app-a9` and are not touched here.

## Method and instruments

1. **Enumeration.** `~/tmp/narms/arms.py` parses every `#if/#ifdef/#ifndef/#elif/#else/#endif`
   in `src/` and `native/src/`, classifies each arm by HX_NATIVE polarity, and keeps both arms'
   text. Result: **2,611 HX_NATIVE conditionals in 661 files**; 2,285 have a native-true arm;
   **252 are two-sided** (≥3 code lines in each arm), 217 of them in TUs the native compile
   database names directly (the rest are scatter-included, e.g. `rndobj/Group.cpp` via
   `Anim.cpp`, so "compiled" was treated as "all").
2. **Compiler screen for the crash class.** A private configure of `native/` (build dir
   `~/tmp/narms/nb`, `CMAKE_EXPORT_COMPILE_COMMANDS=ON`, no edits under `native/`) gave 5,035
   compile commands / 990 unique TUs. Every TU re-run `-fsyntax-only -Wno-everything
   -Wreinterpret-base-class` (clang 23). **Control:** it fires on exactly the known pair in
   `rndobj/Dir.cpp` (`:197` MsgSource*→Hmx::Object* virtual-base reinterpret, `:264`
   Hmx::Object*→MsgSource*) — so it can discriminate — and **nowhere else in the tree**.
3. **clang-tidy screen** (`bugprone-casting-through-void`,
   `cppcoreguidelines-pro-type-cstyle-cast`, `cppcoreguidelines-pro-type-reinterpret-cast`) over
   the same 990 TUs, header-filter = worktree; 1,918 unique diagnostics, **113 inside native
   arms (58 arms)**. Every class-pointer one was read: all are LP64 integer/pointer ports
   (`EditSetlistPanel` XStringVerify buffers, `Text.cpp` `FontKey` ↔ `RndFont*` — every key is
   inserted from an `RndFont*`), allocator/byte buffers, the native ObjRef sentinel walk, or
   C-style downcasts between complete related types (which adjust correctly). **No second
   multiple-inheritance pointer-adjustment bug was found by either instrument.**
4. **Manual review** of the two-sided arms in rndobj, ui, char, world, bandobj, beatmatch,
   meta, math and band3, plus the `dynamic_cast`-on-X360-only screen (88 arms scored by
   cast patterns; X360-only `dynamic_cast` = the RndDir::Enter shape).

Reference for each verdict is the X360 arm (retail-matching per its in-tree address comment);
no verdict below needed a fresh disassembly because each X360 arm carries its retail VA and
its match is graded in `report.json`.

## Findings

Reachable: **boot** = on the rb3-app boot/UI path; **game** = reached once characters,
venues or songs load; **edit** = only in edit mode / tools; **dead** = no native caller.

| file:line (pre-edit) | what differs (native vs retail) | reachable | verdict | fix |
|---|---|---|---|---|
| `world/Instance.cpp:632` | **Same shape as the RndDir::Enter crash.** Native `dynamic_cast<Hmx::Object*>` on both dirs then `Hmx::Object::ChainSource`, which on native chains through the DC3 Object-level `mSinks` system; RB3 code exports through `MsgSource::Export` (MsgSource's own sink list), so the chain is lost. Retail: `dynamic_cast<MsgSource*>` + `MsgSource::ChainSource`. No crash (no reinterpret), silent event loss. | game | divergent | native arm deleted |
| `rndobj/Shader.cpp:214` | Native SelectConfig picks the shadowmap shader on raw mode **2** (DC3: its ShadowMap stores kDrawExtrude), but RB3's `RndShadowMap` stores `kDrawShadowDepth` (1); native shadow pass never selected the shadowmap shader. | game | divergent (constant) | compare `Rnd::kDrawShadowDepth` / `Rnd::kDrawVelocity` |
| `rndobj/Group.cpp:243` | Native `MakeWorldSphere` dropped `AddMotionSphere` for transformable children and the `!b` branch (returned false instead of transforming `mSphere` to world). | boot (any group bounds) | divergent | native arm deleted |
| `rndobj/Draw.cpp:75` | Native `RndDrawable::Load`: `LOAD_REVS/ASSERT_REVS`, no rev<2 RndEnviron adoption into the group, extra notify. | boot (every drawable load) | divergent (old rev only) | native arm deleted |
| `rndobj/Font.cpp:158` | Native kept the dev loose-`.bmp` path: with `UsingCD()` false and a `.bmp` texture filename it loads the file from disk and **nulls `mTexture`**. Retail always locks the texture bitmap. | boot (fonts) | dev-build | native arm deleted |
| `rndobj/Bitmap.cpp:47` | Native reads the later rev-2 header (name CRC, 0xF pad). RB3 writes `BITMAP_REV = 1`; retail reads `rev ? 0x13 : 6`. | boot | dev/DC3, equivalent on RB3 data | native arm deleted |
| `rndobj/Anim.cpp:57` | Native adds DC3 `start_frame`/`end_frame` propsyncs. | edit/script | DC3 | native arm deleted |
| `ui/UIPanel.cpp:274` | Native `Exiting()` also sends a DC3 `exiting` script message and lets its answer win. | boot (screen transitions) | DC3 | native arm deleted |
| `ui/UIScreen.cpp:231` | Native adds `TheRnd.ShouldDrawPanel` (no override anywhere, always true) and a DC3 `suppress_blacklight_text` property write to `TheUI`. | boot | DC3 | native arm deleted |
| `ui/UIListProvider.cpp:70` | Native adds edit-mode and `IsActive` arms (inactive rows get `gNullStr`) and records `BoundsRight()` instead of `GetDrawWidth()`. | boot (lists) | DC3 | native arm deleted |
| `ui/UISlider.cpp:45` | Native saves DC3 rev 3 + `mVertical`; retail rev 1. | edit | DC3 | native arm deleted |
| `char/CharEyes.cpp:862` | Native `Replace` derives an index from `(char*)ref - (char*)begin` (assumes the ref sits at offset 0 of each element), runs `CharWeightable::Replace` last and erases only the hit; retail runs CharWeightable first, `RefIs` + typed `SetOwnerObj`, erases every null. | game | divergent | native arm deleted; loop iterators spelled through a typedef (raw pointer on X360, `ObjVector::iterator` on native) |
| `char/Character.cpp:488` | Native `Poll` clears `mTeleported` only while showing, polls `mTest` in edit mode, sets a current-character scope. Retail clears it every unfrozen poll. | game | divergent | native arm deleted |
| `meta/CreditsPanel.cpp:200` | Native **inverts** the `mAutoScroll` test and scrolls the list itself; retail returns unhandled when not auto-scrolling. | menu | divergent | native arm deleted |
| `world/CameraShot.cpp:1557`, `world/LightPreset.cpp:651` | Native platform filter adds edit-mode / `kPlatformNone` / PC→Xbox handling; retail compares against Xbox. Equivalent unless `null_platform` or edit mode. | game | dev-build | native arms deleted |

### Recommendation: `RndDir::ChainSourceSubdir` should take `MsgSource *`

`virtual void ChainSourceSubdir(Hmx::Object *, ObjectDir *)` (`rndobj/Dir.h:40`) treats its
first argument as a `MsgSource *` (`reinterpret_cast<MsgSource *>(obj)` at `Dir.cpp:264`).
Every caller is in `rndobj/Dir.cpp` and there are **no overrides** in `src/`, `native/src/` or
`../milo-native-engine/src`:

| caller | passes | safe? |
|---|---|---|
| `RndDir::SyncObjects` (`Dir.cpp:197`, shared by both builds) | `reinterpret_cast<Hmx::Object *>(dynamic_cast<MsgSource *>(Dir()))` | yes: the MsgSource pointer value round-trips through the two paired reinterprets |
| `RndDir::Enter` X360 arm (`:462`) | same as above | yes |
| `RndDir::Enter` native arm (`:481`) | `Dir()` implicitly upcast to `Hmx::Object *` | **no**: `MsgSource` is a *virtual-base* derivative of `Hmx::Object` (`obj/Msg.h:206`), so the Object pointer is not the MsgSource pointer: the boot crash (fix on `rbx-app-a9`) |
| recursion (`:266`) | its own `obj` | inherits the caller's value |

The comment above the definition ("MsgSource is unrelated to Hmx::Object, so it compiles to the
same no-op register move") is wrong about the hierarchy; the codegen claim holds only because
`reinterpret_cast` emits no adjustment. Making the parameter `MsgSource *` removes both
reinterprets and makes the native bug a compile error. Cost on X360: the mangled name changes
(`PAVObject@Hmx@@` → `PAVMsgSource@@`), so whoever does it must rename the row in
`scripts/target_symbol_map.json` in the same change, otherwise the row pairs with nothing (the
base obj no longer defines the old name). Not done here: `rndobj/Dir.cpp` belongs to
`rbx-app-a9`. `-Wreinterpret-base-class` (method item 2) is a cheap regression guard for this
class: it flags exactly these two lines and nothing else in the tree.

### Reviewed and left (deliberate or equivalent)

| file:line | why left |
|---|---|
| ObjRef-model translations: `world/Instance.cpp:347,536`, `world/Dir.cpp:111`, `bandobj/OutfitConfig.cpp:94,123`, `bandobj/BandCharacter.cpp:2711`, `rndobj/PropAnim.cpp:119`, `obj/Task.cpp:120,224`, `rndobj/Font.cpp:201` | Native ObjRef is DC3's polymorphic ring node (`Object.h` `OBJREF_VIRTUAL`); the X360 arm's `reinterpret_cast<ObjRef*>(obj)` pun is a type error there. Each native arm re-expresses the same intent (`ReplaceList`, `RefIs`, `ReplaceRefsFrom`). |
| `char/CharClip.cpp:76,98,174,196,287` | Native does not ref-count transition clips (no `AddRef/Release`), and `CharClip::Replace` is X360-only, so native `Transitions::Replace` is dead. Latent risk: a clip deleted while a transition table names it leaves a dangling pointer. Ref-model work, not an arm fix. |
| `rndobj/Cam.cpp:361,420` | Native WorldToScreen/ScreenToWorld recompute from `GetViewProjectXfms` (native projection convention). Renderer-owned. |
| `rndobj/Cam.cpp:326` | `sMaxFarNearPlaneRatio` is 1000; native adds a notify. Equivalent. |
| LOAD_REVS family (16 arms: `LitAnim`, `TransProxy`, `Wind`, `TexBlender`, `Trans`, `TransAnim`, `ScreenMask`, …) | Native splits the revision with `LOAD_REVS` and adds `ASSERT_REVS`; retail compares the whole int. Same reads on shipped data. |
| `rndobj/Mesh.cpp:400` (`CachedRead` byte-swap), `Mesh.cpp:1931`, `ShaderOptions.cpp:162`, `EditSetlistPanel.cpp:90,128,478`, `StoreOfferProvider.cpp:533`, `SongMgr.h:29`, `LightPresetManager.h:60`, `VocalTrack.cpp:1481`, `PatchDir.h:156`, `char/CharBones.cpp:76`, `CharCollide.cpp:278` (X360 arm keeps a dangling local) , `beatmatch/SongParser.cpp:1126`, `VocalNoteList.cpp:475`, `math/*` | LP64/endianness/STL ports or equivalent rewrites. |
| `obj/Dir.cpp:1070` | Native teardown: `dynamic_cast<void*>` for the most-derived block + virtual dtor — MI-correct. |
| `ui/UIFontImporter.cpp:448,502` | Compensators: an Itanium typeinfo name check (some native vtables are zero) and accepting `RndText`'s registered class name. Root causes are elsewhere. |
| `math/Rand.h:42` | `std::shuffle` with a RandomInt-seeded engine vs STLport `random_shuffle`: order differs from retail; native has no STLport. |
| `world/Crowd.cpp:703` | Native `Draw3DChars` breaks after `Apply3DCharXfm` (3D crowd characters not drawn; impostors only). The raw `Character+0x251/0x252` byte writes in that arm sit in the `#else` of a nested `#ifdef HX_NATIVE`, so neither build compiles them. |
| `os/System.cpp:136,546`, `os/JoypadClient.cpp:198` | Partial-config tolerance (`SafeFind`), `gUsingCD` restored to its saved value instead of forced true, joypad defaults when no config. Native robustness shims. |
| `rndobj/Shader.cpp:1125` | Native caches `kAllWhiteShader` when asked for it (retail always caches `kStandardShader`) and adds DC3 `CheckDistortion` + stats. Renderer-owned; differs only for the all-white type. |
| `beatmatch/SongData.cpp:141` | Checksum validation through a helper instead of inline. |
| `meta/PreloadPanel.cpp:89`, `synth/Synth.cpp:275`, BandUI/ProfileMgr/MainHubPanel net and save sinks, `os/System.cpp:910,918` | Deliberate native stubs (offline, no content mount, real mogg open, no shutdown terminate). |

### Deferred (divergent, but coupled)

- **`Rnd::Mode` numbering.** `rndobj/Rnd.h`'s enum is DC3's (velocity = 6, extrude = 2,
  occlusion-depth = 5); shared rndobj code compares RB3 retail raw values (`Shader.cpp` `== 2`,
  `== 3`, `== 6`, `== 7`; `Env_NG.cpp:236`; `SoftParticleBuffer.cpp:120` stores 6, which on
  native is `kDrawVelocity`; `Lit_NG.cpp:214` stores 3). Native numbering needs one owner and a
  retail table of RB3's Mode values; not an arm fix.
- **`ui/UIList.cpp:653`** plus ~11 sibling arms: a DC3 "uncapped display" subsystem
  (`mUncappedNumDisplay`, Refresh re-limiting). Retail routes load through
  `SetNumDisplay/SetGridSpan/SetCircular`, which call `Update()` during load.
- **`rndobj/Text.cpp:1638`** (`UpdateLineColor`): retail indexes vertices `charIdx*4` across
  fonts, native counts per font; correctness depends on how native `UpdateText` lays out the
  per-font meshes.
- **`world/Spotlight.cpp:917`**: native falls back to cone/beam when
  `!SpotlightDrawer::DrawNGSpotlights()` and rebuilds in edit mode; retail always builds the NG
  shaft. Renderer-owned.
- **`rndobj/Draw.cpp:162`** (one-sided): native skips `RndDrawable::Draw`'s world-sphere
  frustum cull. Now that `RndGroup::MakeWorldSphere` returns retail spheres it may be safe to
  restore; needs a render check.
- **Two sink systems.** Native `Hmx::Object` carries DC3's `mSinks`/`ChainSource`/
  `GetOrAddSinks` alongside RB3's `MsgSource` sinks. Instance.cpp was the only cross-over call
  site found; anything else that registers on the Object-level system will not hear
  `MsgSource::Export`.

### Not reviewed (budget)

About 2,000 one-sided native arms (native-only additions and X360-only blocks that native
skips) were screened only by the cast instruments and a ≤6-line skip listing
(`~/tmp/narms/skips.txt`, 250 entries, mostly net/save sinks and ref-model AddRef/Release).
The two-sided arms in `src/system/obj/` (~70, the native ObjRef/loader infrastructure),
`src/system/os/` (~20), `src/system/utl/` (~30), `src/system/synth/` (~15) were not read
arm by arm.

## Measurement

Prediction: Δ0 on every ruler. Each change removes text the X360 preprocessor never sees,
except the CharEyes iterator typedef, which names the same `EyeDesc *` /
`CharInterestState *` type on X360.

- **Commit 1** (`563906fd6`, 11 files): `python3 tools/ab_measure.py --worktree
  ~/tmp/wt-narms --revert HEAD --label narms-c1` (shipped `name_check` ruler plus the `none`
  control leg). Both legs matched=55265, masked_equal=25360, honest=29905,
  code%=59.622105; `none` control Δmatched_code +0 B; Δfuzzy +0.000000pp; units at 100%
  633→633 (mpn) and 547→547 (all-rows-fuzzy). Leg B recompiled 20 objects, split=0,
  settled in 2 iterations. **Δ0, as predicted.**
- **Commit 2** (`dcded50ff`, 5 files: Instance, Character, CreditsPanel, CameraShot,
  LightPreset): `ab_measure --revert HEAD --label narms-c2`. Both legs matched=55265,
  masked_equal=25360, honest=29905, code%=59.622105; Δfuzzy +0.000000pp; `none` control
  Δmatched_code +0 B; units at 100% 633→633 / 547→547; leg B recompiled 17 objects, split=0,
  settled in 2 iterations. **Δ0, as predicted.**
- **Native compile/link/run (both commits).** Full-tree `-fsyntax-only` over the 990 native
  TUs: 0 failures. `ninja rb3-milo rb3-render` in the private build dir `~/tmp/narms/nb`: rc 0,
  both linked. Run through `rb3-scout/coord/no610m.sh` with native_health's arguments:
  `rb3-milo <assets> ui/track/gen/tracksystem_meshes.milo_xbox` → rc 0, `RESULT: ALL GATES
  PASSED (0 gate failure(s))` (its two `Could not find overlay "char_test"` lines also appear
  in earlier `~/tmp/native_health_files_escape_*.log` runs, so they are not new);
  `rb3-render` → rc 2 `RESULT: NO GPU` (the NVIDIA Vulkan ICD failed to load in this session),
  so **the render path is unverified**. The native health selftest was not run (lane
  instruction).
