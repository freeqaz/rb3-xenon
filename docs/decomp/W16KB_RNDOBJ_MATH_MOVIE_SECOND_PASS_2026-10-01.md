# W16-KB — rndobj second pass, plus math and movie (2026-10-01)

**Branch** `w16-kb`, rebased onto main `185ad2667` (pre-rebase tips kept as `w16-kb-prerebase`,
`w16-kb-prerebase2`, `w16-kb-prerebase3`).
**Ruler** `name_check` (graded; `report.json` `provenance.diff_config`).
**Not merged to main**, per the brief.

## 0. Result

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-kb-ab --patch <main 185ad2667..w16-kb>` was run once
on the final code tip `1fd8fee96`.

- **Worktree:** fresh, at main `185ad2667`; the patch is the whole branch (57 files, no docs).
- **objdiff-cli:** pinned to one binary across both legs (`c1b7d95240a35cd6`).
- **Run dir:** `~/tmp/wt-w16-kb-ab/.ab_measure_runs/20261001-134956-branch_vs_main-2109657`.

```
leg A: matched=49082 masked=24247 honest=24835 code%=50.031715  (recompiles: 0, settled)
leg B: matched=49328 masked=24313 honest=25015 code%=50.388145  (recompiles: 1082, split=1, patch_steps=7, settle iterations: 2)
split fixed point: leg A converged after 0 extra re-split(s), leg B after 0
Δmatched=+246  Δmasked_equal=+66  Δhonest=+180  Δcode%=+0.356430pp  Δcode_bytes=+36524
Δfuzzy=+0.255618pp   (legA 58.440422 -> legB 58.696040)
units at 100% [mpn ruler]: legA 351 -> legB 358  (Δ+7; 8 reached 100, 1 fell off)
units at 100% [all-rows-fuzzy ruler]: legA 294 -> legB 298  (Δ+4; 5 reached 100, 1 fell off)
[control none] Δmatched_code=+33204 B -- NOT_APPLICABLE (source in patch)
```

The first run of this A/B was refused, correctly: the branch's `splits.txt` was not at the split's
fixed point after the rebase. `1fd8fee96` commits the re-derived `.pdata` line and the rerun above
measured.

The one unit that "fell off" 100 is FlowTrigger: its only block was LayerDir code (fork B), so the unit no
longer exists, and its row is at 100 in LayerDir.

**No row fell off 100.** Comparing the two leg reports row by row:
- No row present in both legs dropped from 100, on either the fuzzy or the mpn ruler.
- 21 leg-A rows at 100 (fuzzy or mpn) have no leg-B row under the same unit and name. Every one is at
  100, or at its unchanged score, at the same address in leg B: the 10 Movie-block rows (§2.1), FlowTrigger's
  writer (LayerDir), the two TexPtr ctors and a funclet (MatAnim), `fn_824E1824` (Crowd),
  `fn_8273FD88` (rnddx9/CubeTex), the two static `BinkMovieSys` spellings, and three MeshAnim funclets
  now in LitAnim (99.5 / mpn 100 before and after).

Across the whole report **115 rows rose and 5 fell**, none of the five from 100:
- RockCentral `fn_8250A8E4` 72.1 → 0: it had been pairing by bytes with our `??__ETheRockCentral`, which
  now pairs with its real retail body (§2.3).
- `Rnd::DrawPreClear` 81.0 → 80.8 and `EventTrigger::TriggerSelf` 96.98 → 96.96: a callee is now named
  (relocation-name charge).
- Mesh funclets `fn_824200DC` 99.8 → 99.4 and `fn_82420104` 99.4 → 99.3.

## 1. Population and ranking

The population is every unit whose `objdiff.json` `metadata.source_path` is under
`src/system/rndobj/`, `src/system/math/` or `src/system/movie/`: 99 units at the start of the lane
(80 rndobj, 16 math, 3 movie) and 100 on each A/B leg (each leg read with its own `objdiff.json`). Rows were ranked by
size × (100 − fuzzy), favouring rows at 90–99.99.

| | weight (Σ size × (100 − fuzzy)) | rows below 100 |
|---|---:|---:|
| main `e762a9298` (start of lane) | 5,600,451 | 1,053 |
| main `185ad2667` (A/B leg A) | 5,543,267 | 1,037 |
| `w16-kb` (A/B leg B) | 3,422,651 | 810 |

The leg A → leg B drop is −38.3%. Part of it is rows leaving the population by re-homing; the rest is
rows matched in place.

The work was split into the lane plus five forks, each in its own worktree on disjoint source files:

| part | units | fork-reported, own full builds vs `e762a9298` |
|---|---|---|
| lane | movie (Splash, TexMovie, the new Movie unit), math Rand/Color/Interp/Key, Watcher | see §2 |
| fork A | Mesh, MeshAnim, MeshDeform, Morph, Group, MultiMesh, MultiMeshProxy, Spline | +37 fns / +10,200 B |
| fork B | Text, Font, Overlay, Line, Console, Graph, ScreenMask, Bitmap, Tex, TexBlender, TexBlendController, TexRenderer, TexProc, CubeTex, Draw | +55 fns / +5,944 B |
| fork C | Utl, AmbientOcclusion, Rnd, Rnd_NG, Shader*, Mat*, BaseMaterial, MetaMaterial, SoftParticle*, VelocityBuffer, ShadowMap, MotionBlur, ColorXfm, SIVideo, rndobj/Movie | +13 fns / +1,412 B |
| fork D | the other math units, Cam, Trans, TransAnim, TransProxy, Wind, Ribbon, CamAnim | +27 fns / +5,148 B |
| fork E | Anim, PropKeys, PropAnim, EventTrigger, Part*, Shockwave, PostProc*, DOFProc_NG, Lit*, LitAnim, MatAnim, Env*, BoxMap, PollAnim, AnimFilter, Flare, Gen | +25 fns / +7,560 B |

Every change, in the lane and in every fork, was measured with a full `./tools/ninja-locked` build and
a whole-report row diff (`rowdiff.py`) against the previous build. No row fell off 100 at any step.

## 2. Lane work

### 2.1 movie/Movie — RB3's Xbox Bink player, written from the retail block

The retail block `0x82742C08–0x82746128` (13,600 B) is one translation unit: RB3's own Bink movie
player. It had been pinned piecemeal into Splash, SkeletonUpdate, GemManager, Sequence, MemHeap,
DataNode, WavMgr, PoolAlloc and CharBoneDir, plus an unpinned gap, and our `movie/Movie.cpp` was the
DC3 `MovieSys`/`MovieImpl` front end, which this binary does not contain.

**Evidence that it is one TU.**
- The functions call each other; every external caller reaches the block only through two-instruction
  `Movie::` forwarders (`lwz r3,0(r3); b Impl::X`).
- The strings it forms are the movie config keys: `movie`/`is_timed_movie`, `bink_core0`/`bink_core1`,
  `set_bink_track`, `videos`/`stream_begin`/`stream_end`, `BIKi`, `physical`.
- Every Impl entry point performs the same thread check on `mThreadId` at `+0xD4`.

**What was written** (`#ifndef HX_NATIVE`; native keeps the DC3 front end):
- `Movie` holds one `Movie::Impl*`. `LockThread`/`UnlockThread`/`SetWidthHeight` are inlined into the
  `Movie` wrappers; everything else forwards.
- `Movie::Impl` (0xE0 bytes) with its loader (`MovieLoader`, a `Loader` with a member-function-pointer
  state machine: `OpenFile → LoadFile → DoneLoading`), the shared decode targets
  (`MovieInternalBuffers`, 0xB4: three planes × two Bink frame buffers × two halves of `RndTex`, one
  `RndMat`, and the 0x78-byte `BINKFRAMEBUFFERS`), and the disc-contention map.
- Decoding is asynchronous on Xbox. `StartFrame` locks the plane textures into Bink's frame buffers
  (`BeginFrame`) and calls `BinkDoFrameAsync` on the two configured cores; `FinishFrame` waits
  (timeout 0 when the buffer set is shared, −1 when the movie decodes alone), unlocks, flushes the
  finished set out of the CPU cache (`PlatformStoreCache`), and advances.
- The thread check compiles to `mThreadId == GetCurrentThreadId() || (mThreadId == -1 && MainThread())`
  evaluated for effect, i.e. a stripped `MILO_ASSERT` with `kNoThread = −1`.

**Retail details that differ from the older engine shape**
- `TexMovie::NewObject` (emitted in this TU) evaluates `StaticClassName()` and calls
  `MemAlloc(0x84, 0)` inline, so TexMovie uses `OBJ_MEM_OVERLOAD_INLINE_DEL`; the old header comment
  had classified it out of line without reading this body.
- `BinkMovieSys::PlatformInit`/`PlatformStoreCache` are static (retail passes the buffer in r3 and
  calls `PlatformInit` with no object).
- Bink's free callback is `operator delete` itself (folded at `0x8240DDB0`).
- `SetRect` fills the screen height first, letterboxes by aspect, and centres with ½/2× constants;
  `FinishOpen` disables sound when more than one movie shares the buffers (`mRefs > 1`, signed).

**Pins and names**
- New unit `system/movie/Movie.cpp`, `.text 0x82742C08–0x82746128`; the ranges were removed from the
  nine units above. SkeletonUpdate.cpp's only pin was inside the block, so that unit is gone.
- 61 names written: 59 new rows through `gated_map_write`, plus two nulled rows (`0x82745B40`, a
  bad NUISPEECH transfer, and `0x82744BE8`, a bad BinDiff transfer) filled textually. Eight wrong names
  corrected from the bodies:

  | address | was | is |
  |---|---|---|
  | `0x82743A38` | `SkeletonUpdateHandle::PostUpdate` | `Movie::SetTimeCallback` |
  | `0x82743B10` | `SetKeyGlow` | `OnMovieSetTrack` (writes the int MovieOpen reads as the forced track) |
  | `0x82744E38` | `FixedString::find` | `Movie::End` (`b Impl::End`) |
  | `0x82745B00` | `CritSecTracker` ctor | `Movie::~Movie` |
  | `0x82745BE0` | `FixedSizeAlloc::operator delete` | `Movie::Init` (`b Impl::Init`) |
  | `0x82745D08` | `??_GWavMgr` | `??_GMovieLoader` (calls `~MovieLoader`) |
  | `0x82746128`, `0x82746150` | non-static `BinkMovieSys` members | the static spellings |

- The five `map<Symbol,String>` tree functions in the block have no caller outside it, so the copy the
  linker kept is Movie's own `map<void*,String>` instantiation; renamed to that spelling, and
  `_M_erase`/`clear`/the map dtor named (all eight read 100).

**Result.** The unit stands at 99 of 106 functions and 10,384 of 13,328 B matched (77.9%), fuzzy 99.08. Its rows that had been at 100 under the old units
(SkeletonUpdate, GemManager, DataNode and PoolAlloc funclets, BinkMovieSys_Xbox) are all at 100 again
at the same addresses.

### 2.2 Alias folds (lane)

Six memberships admitted, each `tools/icf_pair_adjudicate.py --chase`: CHASED T1 PROVEN with 0
CYCLE-ASSUMED leaves, recorded per member under `admitted`:

| survivor | our spelling |
|---|---|
| `push_back<ChatReceiver*>` @ `0x82B5F808` | `push_back<Movie::Impl*>`, `push_back<BINK*>` |
| `vector<int>` copy ctor @ `0x827C1378` | `vector<BINK*>` copy ctor |
| `list<Hmx::Object*>::insert` @ `0x823D14C0` | `list<Movie::Impl*>::insert` |
| `list<void(*)()>::remove` @ `0x827BF4F0` | `list<Movie::Impl*>::remove` |
| empty body @ `0x826C3888` | `MovieLoader::DoneLoading` (VACUOUS-BUT-IDENTICAL; the membership rests on `IsLoaded` comparing the state pointer with `0x826C3888` and `OpenFile`/`LoadFile` storing it) |

`PlatformCacheFile → IsDirPtr` was REFUTED (no compiled body) and not added. Three memberships were
withdrawn with records, each because the spelling is now named at its own retail address:
`Movie::End` from TrackWatcher::Swing (`0x8279D708`) and `Movie::Save` from TrackWatcher::Poll
(`0x8279D6C0`) — both dated from the DC3 Movie's virtual forwarders — and `_Rb_tree<void*,String>::clear`
from the CatData clear group (`0x823D9920`; its own body is `0x827444A0`).

### 2.3 Other lane fixes

- **TexMovie::Poll** (`0x82746380`, RndPollable vtable slot 0) does not test `mPaused`: 90.3 → 100.
- **`0x822B9308`** was named `TexMovie::Reset` and pinned to TexMovie. Its body is
  `mIconLabel->SetIcon(*cc)` and it fills the one gap in CrowdMeterIcon's pins: re-homed as
  `CrowdMeterIcon::SetIcon` (100). Retail TexMovie::Reset has no body of its own (no branch to
  `Movie::End` exists anywhere).
- **Rand's static-init pin** covered the dynamic initializers of later TUs. Rand now keeps only `sRand`'s;
  `??__ETheRockCentral` (`0x82C3F598`) and `??__EThePlatformMgr` (`0x82C3F848`) were re-homed to their
  units (both 100). The rest (a String, TheEntityUploader, CriticalSections, a SynchronizationEvent,
  TheDebug) stay unattributed: their owning objects define no matching `??__E` name. RockCentral's
  unnamed `fn_8250A8E4` drops 72.1 → 0 because it had been pairing by bytes with our
  `??__ETheRockCentral`.
- **QuatSpline** is a Catmull-Rom over a contiguous `Quat[4]`
  (`0.5·(2P1 + (P2−P0)t + (2P0−5P1+4P2−P3)t² + (3P1−P0−3P2+P3)t³)`, constants 3/4/5 read from retail):
  65.8 → 94.5.
- **Splash::PrepareNext** clears the splash movie's time callback before `CheckOpen`: 93.0 → 99.9.

## 3. Fork work (merged into the branch, `--no-ff`)

Each fork's figures are from its own full builds against `e762a9298`, before the rebase.

- **Fork A — mesh** (`w16-kb-a`, 17 commits).
  - `ObjPtrList::remove` returns nothing and removes every matching node (retail `0x82452950` keeps
    walking after a match); map row renamed, 0.64 → 100. An unused `bool` specialisation in
    `TypeProps.cpp` deleted.
  - `MemResizeElem` takes six parameters (retail reads r3–r7 only; all three call sites pass six):
    87.3 → 100.
  - **Behaviour bugs, Group:** `Save` dropped the environment and LOD fields and stamped revision 16
    instead of 14 (56.4 → 100); `AddObject` re-inserted an existing member; `Update` lacked the LOD-state
    refresh; `ListDrawChildren` lacked the LOD drawable.
  - Mesh `Load` in retail's shape (88.2 → 95.7), `OnCompareEdgeVerts` (inner loop starts at `i + 1`,
    → 100), `LoadVertices`' compressed path (71.3 → 94.5); MeshDeform `AppendWeights` as one loop
    (61.5 → 97.8) and its two ctors (→ 100); MeshAnim `EndFrame` (→ 100).
  - Pins: LightAnim code moved out of MeshAnim's pin, VorbisReader and Performer code out of Mesh's;
    four names from vtable slots and call edges.
- **Fork B — text/tex** (`w16-kb-b`, 22 commits).
  - LayerDir code moved out of Overlay and FlowTrigger (FlowTrigger's only block); `Save`/`Handle@LayerDir`
    → 100, plus the `list<FilePath>`/`Layer` writers.
  - **Behaviour bug, TexBlender:** `DrawBlendList` used the near map for every state except `kTexNear`
    (inverted), and drew with shader 0x16 instead of `kUnwrapUVShader`; `SetupMaterial` set a cull mode
    retail does not. DrawShowing 75.9 → 97.6.
  - Written: `RndBitmap::DetachMip` (declared and called by PatchDir, never defined), `DOFProc::Init`
    (singleton only), `RndGraph::Terminate`, `RndFont::GetTexCoords`; Text `Init` statics.
  - Re-homed: TrackDir's ctor funclets (+24 fns), `RndText::Line`/`FindPathName`, `RndMorph::Save`,
    `RndMultiMesh::OnDistribute`, `??_GBandScoreboard`; `0x82466298` renamed to `DOFProc::Init`,
    `0x8270AAC0` to `ObjVector<ObjPtr<SeqInst>>::resize`.
- **Fork C — Utl/AO/Rnd/Shader/Mat** (`w16-kb-c`, 10 commits).
  - **`Hmx::operator*(Matrix4, Matrix4)` computed b·a; retail computes a·b** (`math/Mtx.h`); rewritten
    as four row × matrix products.
  - `RndUtlInit` is empty in retail; `RndUtlTerminate` releases only the sphere pair.
  - Rnd `EndDrawing` (0 → 100), `CompressTextureCancel` written (was declared, never defined),
    `CompressThread` 34.7 → 99.9, `RegisterPostProcessor` 78.8 → 99.8.
  - Shader `Cache` 50 → 100, `SetColorWriteMask` 6.3 → 100; `~RndMat` is compiler-generated (→ 100);
    AO's list gatherer written as `GatherObjects<T>` (the name is ours).
  - Re-homed DxLight `StaticClassName`/`ClassName`, `CamShot::ClassName`, `~Drawable`,
    `ObjVector<IKTarget>::operator=`; `0x82399348` was `MakeString<int,int>`, it is that `operator=`.
- **Fork D — math/Cam/Trans** (`w16-kb-d`).
  - **Behaviour bugs:** `Intersect(Segment, BSPNode*)` built the near half as {start, end} instead of
    {start, mid}, overwrote `t`, and inverted the branch (0 → 100); `RndCam::GetViewProjectXfms` read
    the vertical scale from the wrong component, so its y row was zero (57.1 → 89.7); `RndWind`'s default
    space loop is 100, not `10 * gUnitsPerMeter`.
  - `RndCam::SetFrustum` is one out-of-line function with literal clamps (→ 100); Cam/CamAnim
    Save/Load use plain revisions; Trans `OnSetLocalRotIndex` 27.3 → 100, `OnGetLocalRot` 69.0 → 100.
  - `Hmx::operator*(Transform, Matrix4)` moved, unchanged, from Lit_NG.cpp to Cam.cpp (retail places it
    in Cam's text). Cam's init-area pin trimmed to its own initializer; templates re-homed to Crowd,
    AmbientOcclusion and CharBonesMeshes. The survivor of the `0x8248FD58` group was respelled to the
    Triangle member (element 0x40, not 0x44) with the old spelling recorded as withdrawn.
- **Fork E — anim/fx/light** (`w16-kb-e`, 9 commits).
  - `RndAnimatable::OnAnimate` in retail's six-argument AnimTask shape (38.1 → 95.6); the rate tables
    have five entries (`PollAnim::Poll` → 100).
  - NgLight has one shadow render target, not two; `BlurShadowRT` takes two floats and is one pass.
  - **Behaviour bug:** `BloomTextures::AllocateTextures` read one element past the array and never
    allocated set 0.
  - RndPostProc `Interp`/`BlendPrevious`/`BloomIntensity`/`DoMotionBlur`, NgPostProc `RebuildTex`/
    `CheckHallOfTime`, DOFProc `DoPost`/`SetVHBlurWeights`, AnimTask `Replace`/`Poll`/ctor → 100.
  - TexPtr's two ctors and `TexKeys::Add` re-homed into MatAnim.
  - Shared files: `obj/Task.h`/`Task.cpp` (`Task()` is inline and empty on X360; native body kept).

## 4. Rebase onto main

Main moved three times during the lane (W16-JH + W16-KE, then W16-KC, then W16-KA). The branch was
rebased each time with `git rebase --rebase-merges`, so all five fork merge commits are preserved.
`~/tmp/w16kb/rebase_drive.py` drove it: the key-level map resolver for `target_symbol_map.json`, a
group-level three-way merge for `symbol_aliases.json`, derived-`.pdata`-only hunks taken either way, and
a stop on anything else. Decisions taken by hand:

- **`0x82463680`**: fork E renamed it `RndMatAnim::TexKeys::Add` (only caller `RndMatAnim::SetKey`, reads
  100 in MatAnim); fork D nulled it. Both agree the `operator*` name was wrong; E's positive identification
  was kept.
- **Graph block**: fork C extended it to `0x82464470` to take `~Drawable`; fork B had only dropped a
  derived `.pdata` line. C's start was kept.
- **CatData clear group**: main (W16-JH) and the lane both edited it; the lane's withdrawal was applied on
  top of main's version.
- **DataNode**: main (W16-KE) merged `0x8274A974–0x8274A9A4`; the lane removed `0x82744E48–0x82745354`.
  Both kept.
- **`ObjPtrList::insert`**: main (W16-KC) landed the same forced-inline insert fork A had written; main's
  spelling was kept. Fork A's measured gain for it (+15 fns / +6,252 B) is therefore already in leg A.
- **VorbisReader**: main (W16-KA) re-homed its `0x822B8BE8` block; fork A's commit only carried it as
  context, so main's side was kept.

## 5. Gates

All four were run on the final code tip `1fd8fee96`; the only later commit is this doc, which touches no
build input.

```
[map-injectivity] OK: 32893 applied rows, 32892 distinct names, injective (+1 enumerated internal-linkage exception(s))
VALIDATE: PASS -- 1606 map-consistent, 271 tolerated (enumerated above), 0 contradicted, 1878 total
[patch-state] OK: tree is a fixed point of 6 post-compile passes
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

Every fork's tip also passed all four gates before it was merged.

## 6. Not done

- Not merged to main. No `symbols.txt` edits; the permuter was not run.
- **dtk mis-carves still open after main's W16-KE** (each needs a `symbols.txt` merge; W16-KE already
  fixed `ScaleXfms`):
  - the `Keys<Vector3>` tangent helper `0x824F56B0` (7 pieces, in Color's pin; called from
    `InterpVector`);
  - `ExpInterpolator`'s ctor: the body starts at `0x824F62F8`, but the name sits on its 20-byte tail at
    `0x824F6340`;
  - `Hmx::operator*(Transform, Matrix4)` at `0x82433A68` (4 pieces) and `operator*(Matrix4, Matrix4)` at
    `0x824A7AA0` (4 pieces);
  - `Normalize(Vector3)` at `0x822C1280`, and two MeshAnim leaves at `0x8246D718`/`0x8246D940`.
- **Register allocation and scheduling, skipped after about three spellings each:**
  `Movie::Impl::SharedFinishOpen` (retail keeps a zero constant in r22), `Draw`, `Begin`; `Rand::Seed`
  (MSVC fuses the two halves into `rlwimi`); the Splash ctor; QuatSpline's last 5%; and the forks'
  lists (NgLight::SphereConeTest, Mesh `OnSync`/`Load`, Text DrawToTexture/ParseMarkup, MakeBSPTree,
  and others).
- **Identification left open:** the unattributed static initializers after Rand's (their owning objects
  define no matching `??__E`); Watcher's template islands (fold-survivor names for other types);
  `0x822BB3C8` (`__push_heap` survivor); the Mesh pin at `0x826985C8` inside Stats.cpp (five candidate
  ctors).
- **Not reached:** the big geometry rows (AO `Tessellate`/`SmoothResults`, Utl `MakeTangentsLate`/
  `TessellateMesh`/`RndScaleObject`/`ResetNormals`), Text `WrapText`, Part `MoveParticles`, RndDir
  `PreLoad`/`PostLoad`, Shockwave's `ObjList` readers (they belong in world/Dir.cpp).
