# X4a — venue render: what reached the GPU, and the wall that stopped the venue root

**Date:** 2026-08-02
**Predecessor:** [X3](x3-first-render-2026-08-01.md) "RENDERS"
**Branch:** `x4a-venue-render`, rebased onto `main` @ `d7a9775a`
**Engine:** `milo-native-engine` @ `2ea8e343…` — **zero engine edits**

---

## Verdict: **PARTIAL — the charter's main event is BLOCKED, and the blocker is structural, not a renderer defect**

A real RB3 **venue root** does not load in this target and cannot be made to,
for a reason that has nothing to do with rendering: `rb3-render` compiles **no
`band3/` code**, and every RB3 venue root is 500–630 references deep in band3
classes. The failure is a hard stream desync, not a missing feature (§3).

What *did* land is real and measured:

- **Two pre-existing defects on `main` found and fixed**, one of which had
  already broken `rb3-milo` + `rb3-render` on main before X4a started (§1, §2).
- **A venue-geometry cell renders**, deterministically, from a `WorldInstance`
  root — the first `WorldInstance` in the ladder (§4).
- **Post-processing moved from UNREACHED to VERIFIED** by a controlled A/B on a
  shipped RB3 PostProc asset — and that A/B turned up a real nondeterminism (§5).
- **Both owed rider A/Bs executed.** Both measure Δ=0 on every measure, which is
  a *negative* result and is reported as one (§6).
- **One of my own hypotheses refuted and retracted in-tree** (§7).

---

## 1. Gate results

| # | Gate | Result | Evidence |
|---|---|---|---|
| a | All 18 targets + gate script green on the **rebased** tree | ✅ **PASS — 18/18** | `tools/native_build_gate.sh` → `PASS (rc=0, 0 errors, 0 warnings, 18/18 target(s) verified)`, every target `relinked this run`. Rebased onto `main` @ `d7a9775a` with **zero conflicts**. |
| b | Zero `milo-native-engine` source edits | ✅ **PASS** | Engine `HEAD` still `2ea8e343…` = the pin. Three engine-change requests are filed as text in §8, not as edits. |
| c | Shared-`src/` edits HX_NATIVE-gated or A/B-proven | ✅ **PASS** | Two shared files touched — `world/Instance.cpp` and `obj/DirLoader.cpp` — **every hunk inside `#ifdef HX_NATIVE`**, with the X360 arm preserved token-for-token including statement order. Both are scored units (`default/Instance` 85% fn-matched), which is exactly why the X360 arms are byte-preserved rather than restructured. |
| d | PNG determinism, 2 runs | ✅ **PASS (with a named exception, quantified)** | Both X4a cells byte-identical over 2 runs at `--frames 1`. ⚠ The postproc cell is **NOT** deterministic at the X3 default `--frames 4` — 3 runs, 3 SHAs. Root-caused and bounded in §5; it is an engine issue, reported not tuned around. |
| e | No X3 regression | ✅ **PASS — byte-identical** | `rb3-render` with no arkPath reproduces X3's two cells at `sha256 cbdb29fa95a5b574…` and `30692a8d02c1ada0…` — **the exact SHAs X3 recorded**, on freshly relinked binaries. |

### Reproduce

```bash
cd rb3-xenon/native
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
      -DMILO_ENGINE_PATH=/home/free/code/milohax/milo-native-engine \
      -DDawn_DIR=/home/free/code/milohax/dc3-decomp-deps/dawn/lib/cmake/Dawn
cmake --build build --target rb3-render

ASSETS=/home/free/code/milohax/rb3/orig-assets/xbox-zip
VENUE=world/venue/arena/arena_02/props/gen/backwall_angles_02.milo_xbox
PP=world/shared/fx/gen/post_process_fx_venue.milo_xbox

./build/rb3-render $ASSETS build/x4a/geom --frames 1 $VENUE                # d9624b90…
./build/rb3-render $ASSETS build/x4a/post --frames 1 --postproc $PP $VENUE # d30f600d…
./build/rb3-render $ASSETS build/x4a/x3                                    # X3 regression check
```

⚠ **In a worktree, `MILO_ENGINE_PATH` and `Dawn_DIR` MUST be passed explicitly.**
Both default *relative to the source tree* (`native/CMakeLists.txt:842,855`), so
in a worktree they resolve to nonexistent paths, `RB3X_BUILD_ENGINE` silently
auto-disables, and `rb3-frame`/`rb3-milo`/`rb3-render` vanish at `rc=0`. The gate
script already documents this trap for `rb3-frame`; it applies to all three.

---

## 2. Two defects on `main`, found before any X4a work could start

### 2.1 ⛔ `main` was already broken: `rb3-milo` and `rb3-render` did not link

The very first gate run on a fresh worktree of `main` (`b206d005`) returned
**FAIL, 16/18**:

```
EnvAnim.cpp:(.text._ZN10RndEnvAnim4SaveER9BinStream+0x78): undefined reference to
  `BinStream& operator<< <RndEnvAnim>(BinStream&, ObjOwnerPtr<RndEnvAnim> const&)'
```

`81d23046` (lane DD-2, "SAVE_OBJ stubs") replaced `RndEnvAnim::Save`'s
`MILO_ASSERT` stub with a real body whose `bs << mKeysOwner` is the tree's
**third** `ObjOwnerPtr` save site. `operator<<(BinStream&, const ObjOwnerPtr<T1>&)`
is **declared** at `obj/Object.h:760-761` and its definition at
`obj/ObjPtr_p.h:485-486` is **commented out** — in DC3's copy of the header too.
Because that declaration is an *exact* match for an `ObjOwnerPtr` argument it
beats the base-class `operator<<(BinStream&, const ObjRefConcrete<T1,ObjectDir>&)`
at `:156` — which *is* defined — so the call compiles clean everywhere and fails
only at native link.

The X360 match build compiles TUs and diffs objects; **it never links**, so this
defect class is structurally invisible there. The gate exists precisely for it
(its header already records `b2958f2d` doing the same thing) and was not run.

**Fixed** with one explicit instantiation in `native/src/milo_link_stubs.cpp`,
the native-only file that already reconstructs this operator for `CharClip` and
`Waypoint`. **Zero shared-src edits**, so X360 is untouched by construction. I
built and then reverted the alternative (defining the template in `ObjPtr_p.h`
under `#ifdef HX_NATIVE`); it also works and has a wider blast radius.

⛔ **The explicit-instantiation list is a recurrence trap and will fire again**
on the next `ObjOwnerPtr` save site. The permanent fix is the `ObjPtr_p.h`
definition; owed work, §9.

### 2.2 ⛔ `WorldInstance::DeleteTransientObjects` walked a ring **copy**, forever

Pointing `rb3-render` at `world/venue/small_club/small_club_01` hung: **11m39s
wall / 11m35s CPU, no output, no crash.** Stack captured under gdb (ptrace of a
non-child is blocked on this box, so gdb had to be the *parent*):

```
__dynamic_cast  <- RndTransformable::Replace  <- ObjOwnerPtr::Replace
<- WorldInstance::DeleteTransientObjects <- SyncDir <- PostLoad
<- DirLoader::LoadObjs <- LoadMgr::PollUntilLoaded <- ... <- RenderCell
```

The line is `auto refs = obj->Refs();`. `Hmx::Object::Refs()` returns
`const ObjRef &mRefs` — the **live ring head** (`obj/Object.h:1973`) — and `auto`
deduces `ObjRef` **by value**, so `refs` is a *copy* of the head node.
`ObjRef::end()` is `iterator((ObjRef *)this)` (`:206`), i.e. the address of that
copy, which **no node in the real ring ever points at**. The walk runs
…→ last → `&obj->mRefs` → first → … and the terminating compare is never true.

★ **It does not even need a non-empty ring.** On an empty one `begin()` yields
the real head, `end()` yields `&refs`, they differ, and `++it` lands back on the
head forever. The head is a plain `ObjRef` whose `RefOwner()` is null (`:157`),
so the loop body never fires — which is why it spins **silently** at 100% CPU
rather than crashing. The most expensive possible way to report a bad iterator.

**X2 and X3 could not reach this**: the venue is the first asset in the ladder
that loads a `WorldInstance` proxy.

**This is a transcription defect, on three independent witnesses:**

| # | witness | what it says |
|---|---|---|
| 1 | rb3-Wii's faithful RB3 decomp | `std::vector<ObjRef *> refs = obj->Refs();` — a **detached snapshot** — iterated in reverse. Its `Refs()` returns a vector *by value*. |
| 2 | the residue in xenon's own code | rb3-Wii wraps `MemDoTempAllocations` around the **copy**, because building the vector allocates. xenon kept the scope and dropped the allocation it existed to scope — a loop that allocates nothing. |
| 3 | DC3 | splices matching refs onto a private ring with `MoveBefore`, then `ReplaceList()`s them — never walking a ring it is mutating. |

Fixed with witness 3 — which is **also what this same file's
`WorldInstance::SyncDir` already does ~50 lines below, under the same `#ifdef`**.
Not a new mechanism: the surrounding working code, applied to the one site that
diverged from it. `HX_NATIVE`-gated, X360 arm token-for-token.

---

## 3. ⛔ The main event's wall: a venue root needs `band3`, and this target has none

`world/venue/small_club/small_club_01` — chosen because it is RB3's *gameplay*
venue and the one rb3-Wii's native port has by far the most known-good history
with (250 doc references; its V19 milestone renders it) — does not load. After
the hang fix it reaches a segfault, and the log names the cause on its **first
line of venue work**:

```
small_club_01.milo_xbox:            Can't make BandCamShot
small_club_01_base.milo:            Can't make BandConfiguration
small_club_01_base.milo:            Can't make WorldCrowd
...
DirLoader: rev 3 < 7 in '' at stream offset 906611 (proxy 'amp_fnr_bassman')
ASSERT_REVS WARNING: WorldInstance 'amp_fnr_bassman01' version 41 > 3
FAIL: String chars 774778671 > 256
SIGSEGV in ChunkStream::ReadImpl via operator>>(BinStream&, FilePath&)
```

**684 factory misses over 14 distinct classes**, all game-layer:

| class | misses | | class | misses |
|---|---|---|---|---|
| `BandCamShot` | 611 | | `SynthFader` | 5 |
| `Sfx` | 23 | | `BandCharacter` | 4 |
| `SynthSample` | 18 | | `UIColor`, `ParallelGroupSeq`, `BandLabel` | 2 each |
| `WorldCrowd`, `MoggClip` | 6 each | | `RandomGroupSeq`, `FxSendEQ`, `BandWardrobe`, `BandConfiguration` | 1 each |

**And an unregistered class cannot be skipped.** `WorldInstance::LoadPersistentObjects`
is two-phase: phase 1 reads a `count`-long list of (className, name) pairs;
phase 2 replays `PreLoad`/`PostLoad` for each object **inline, with no framing**.
`DirLoader::ReadDead`'s `0xADDEADDE` barrier only separates *top-level* objects,
not persistent ones. So there is no way to consume an unknown class's bytes, and
the existing `DeleteObjects(); return;` is **correct damage-limiting**, not a
bug. The desync that follows is the unavoidable consequence, and it is what
eventually walks a garbage `String chars 774778671` off a cliff.

★ **This is not fixable by picking a different venue.** Scanning every venue
milo in the archive for band3 class names versus `Environ`/`Light` content:

| asset class | band3 refs | `Environ` | `Light` | `PostProc` |
|---|---|---|---|---|
| every venue **root** (`arena_*`, `big_club_*`, `small_club_*`, `festival_*`) | **506–633** | 9–24 | 41–69 | 0 |
| `video_*` roots | 506 | 24 | 69 | 164 |
| venue **props** (`backwall_*`, `riser_*`, `banner*`, `stone_block`, `test`) | **0** | **0** | **0** | 0 |
| `world/shared/fx/gen/post_process_fx*` | **0** | 0 | 0 | **30–40** |

⇒ **There is no RB3 asset with `RndEnviron` + lights that does not also need
band3.** Lighting and the game layer are co-located in every venue root. So
"venue lighting" and "a compiled band3" are the *same milestone*, and X3 §9
already ranked `bandobj/`+`band3/` as item 4 without knowing it was item 1.

---

## 4. What renders: `backwall_angles_02`, a `WorldInstance` venue prop

| | |
|---|---|
| Asset | `world/venue/arena/arena_02/props/gen/backwall_angles_02.milo_xbox` (730 KB) |
| Root class | **`WorldInstance`** — the first in the ladder, so §2.2's fix is on this path |
| Census | 4 meshes, 0 skinned, **4 with a Mat, 4 with a diffuse Tex** |
| bbox | `(-383.80 -8.27 256.08) .. (384.10 118.94 509.17)` over 2408 verts, **no outlier trimming** |
| Draws | 4 of 4 issued |
| PNG | coverage 3.97%, 2867 colours, `sha256 d9624b90…`, identical over 2 runs |
| Content | Recognisable arena stage backwall: a slatted vertical-batten panel with horizontal rails, plus a curved structural rib. Real geometry, real textures. |

Chosen over the other zero-band3 props (`riser_panels02`, `backwall_middle_02`,
`backwall_wing_02`, `banner`, `stone_block`, `test`) as the largest with the most
material coverage. It is **venue geometry**, and it is honestly not a venue.

---

## 5. Per-subsystem table — the charter's central deliverable

Verdicts are bounded by what was actually observed. "UNREACHED" here sometimes
means *the engine has no caller for it*, which is a stronger and more useful
statement than "we didn't try".

| subsystem | verdict | evidence |
|---|---|---|
| **Geometry → GPU** | ✅ **VERIFIED** | 4 of 4 meshes issued draws; PNG shows the asset's actual shape; deterministic ×2. |
| **Materials + textures** | ✅ **VERIFIED** | All 4 meshes carry a real `Mat` with a real diffuse `Tex`; **no fallback material was synthesised** (unlike X3 cell 1, where all 130 needed one). 2867 distinct colours. |
| **Post-processing (`RndPostProc`)** | ✅ **VERIFIED** | Controlled A/B on the engine's own predicate (`Rnd_Wgpu.cpp:454`, `RndPostProc::Current() != nullptr`). Same geometry/camera/lights; selecting the shipped `intro_contrast_flame.pp` moved coverage **3.97% → 88.47%**, background **`#0f171f` → `#3d3d3d`**, colours 2867 → 1401, SHA `d9624b90…` → `d30f600d…`. The frame is visibly contrast-lifted and desaturated. **The pass ran.** |
| **Environ lighting (`RndEnviron`)** | ⚠️ **SYNTHESIZED** | The asset ships **no `RndEnviron`** (measured: 0 `Environ` strings). The driver announces `environ: SYNTHETIC … ambient 0.35 + 1 directional key` at runtime. The lighting in the PNG is **ours**, not RB3's. No RB3 asset can improve this without band3 (§3). |
| **Lights (`RndLight`)** | ⚠️ **SYNTHESIZED** | One synthetic directional key. The asset ships zero `Light` objects. |
| **Transparency ordering** | ❌ **UNREACHED — and the engine cannot reach it** | `QueueTransparentDraw` (`TransparentQueue.cpp:111`) has **zero callers** anywhere in `milo-native-engine/src/`. `FlushTransparentDraws()` *is* called twice a frame (`Rnd_Wgpu.cpp:438`, `:1009`) — so the queue is **flushed but never filled**. Nothing this consumer does could exercise ordering. Engine-change request, §8. |
| **Shadows (`ShadowPass`)** | ❌ **UNREACHED** | `mShadowPass` is initialised (`Rnd_Wgpu.cpp:300`) but the only read of `InShadowPass()` is `Mesh_Wgpu.cpp:338`, and nothing on this path enters the pass. Entering it needs `Character::DrawLodOrShadow`, i.e. a driven `Character` — X4b. |
| **`Rnd`/`NgRnd` semantic members** | ✅ **no new mismatches** | X3's eight-member `--dump-rnd` audit still clean; X4a touched nothing that would move it and found no new divergence. |

### 5.1 ⚠ The postproc A/B found a real nondeterminism

At X3's default `--frames 4` the postproc cell is **not byte-reproducible**:
three runs, three SHAs. Quantified rather than asserted — **89.92% of subpixels
differ, but max delta 13 / mean 3.4**, i.e. a low-amplitude per-pixel grain, not
a structural difference.

Isolated by bisecting the frame count:

| frames | no postproc | postproc |
|---|---|---|
| 4 | `d9624b90…` stable ×3 | **3 runs, 3 SHAs** |
| 1 | `d9624b90…` stable ×2 | `d30f600d…` **stable ×2** |

So the varying quantity is the **number of post passes executed before readback**,
and the grain is seeded from it: `milo-native-engine/src/platform/RB3PostProc.cpp:429`,
`uni.time = (float)mFrameCount`. The no-postproc leg is byte-identical at *both*
frame counts, which rules out the geometry/upload path.

The canonical postproc cell therefore runs at `--frames 1`, **with a matched
`--frames 1` control** so the A/B compares like with like. This is reported, not
tuned around: engine-change request in §8.

---

## 6. Rider — the two owed objdiff A/Bs, both executed, both negative

X3 §8 owed an objdiff A/B on two `#ifdef HX_NATIVE` hunks. Both were run with
`tools/ab_measure.py` (whole-binary, settle + 2 legs, the house instrument), by
building the **promoted** form — i.e. giving the X360 arm the native fix — and
measuring against `HEAD`.

| patch | leg A | leg B | Δmatched | Δmasked | Δhonest | Δcode% | Δfuzzy | recompiles |
|---|---|---|---|---|---|---|---|---|
| `Cam.cpp:468` `projMtx.y.y` ← `m.z.y` | 43568 | 43568 | **+0** | +0 | +0 | +0.000000pp | +0.000000pp | 1 (applied) |
| `Mat.cpp:285` null-guard `sMetaMaterials` | 43568 | 43568 | **+0** | +0 | +0 | +0.000000pp | +0.000000pp | 2 (applied) |

**Interpretation, stated carefully: Δ=0 is NOT "retail agrees".** It means the
instrument **cannot discriminate**. Neither
`?GetViewProjectXfms@RndCam@@` nor `?Init@RndMat@@` appears as a scored function
anywhere in `build/45410914/report.json` — I checked, they are absent from
`default/Cam` (100 fns listed) and `default/Mat` (10 fns) alike. A change to an
unscored body cannot move a score. If either *had* been scored, swapping
`v.x`→`m.z.y` changes a struct offset and would necessarily have moved the
number; it did not, which independently corroborates "unscored".

⇒ **Per the charter, the ifdefs STAY.** The debt was the measurement, and the
measurement is done: *promotion is metric-neutral, and unevidenced*. Promoting
on Δ=0 would be dressing up an absence of evidence as evidence. The three-decomp
agreement in X3 §5.1 remains the only argument for the Cam fix being retail-correct,
and it is a source argument, not a binary one.

---

## 7. ⛔ Retracted: the `PushRev`/`PopRev` ordering was NOT the venue corruptor

Recorded because a narrated dead end is worth more than a silent one.

xenon's `WorldInstance::PreLoad`/`PostLoad` are **byte-identical to DC3's**, and
both are **transposed relative to rb3-Wii's**: rb3-Wii calls `RndDir::PostLoad`
first and pops after (and pushes before `RndDir::PreLoad`); xenon/DC3 do the
reverse. With `?PostLoad@WorldInstance@@` sitting at **59.07%** and
`?PreLoad@WorldInstance@@` at **76.92%**, this looked like a decomp bug that
would explain the stream desync *and* pay a match dividend.

**It is not.** `BinStream::PushRev`/`PopRev` (`utl/BinStream.cpp:284`, `:144`)
only push and pop a process-wide `sRevStack`; **they never touch the byte
stream**. And both orderings are internally consistent LIFO — xenon pushes last
and pops first, rb3-Wii pushes first and pops last.

I built and ran the swap anyway rather than reasoning my way out: the venue
failure reproduced with a **byte-identical 3494-line log** and the identical
`version 41` / `String chars 774778671` numbers. Zero runtime effect. The edits
were reverted and the divergence is recorded **in place, in the source**, as an
open *match* question for a lane with an A/B — not as a runtime fix.

---

## 8. Engine-change requests — none blocking, zero edits made

1. **The transparent queue has no producer.** `QueueTransparentDraw`
   (`TransparentQueue.cpp:111`) is called from nowhere in the engine, while
   `FlushTransparentDraws()` runs twice per frame. Either `Mesh_Wgpu`'s draw path
   should enqueue blended materials, or the flush calls are dead weight. Until
   then **no consumer can exercise transparency ordering**, which makes that row
   of every future subsystem table unfalsifiable.
2. **The postproc grain seed is not headless-reproducible.** `RB3PostProc.cpp:429`
   seeds from `mFrameCount`, and the pass count before readback varies run to run
   at frames ≥ 2 (§5.1). A deterministic or externally-settable seed would make
   post-processed captures byte-comparable, which is the only way to A/B a
   post-processing change at all.
3. **Carried unfixed from X3, all three re-observed:** static-lifetime GPU caches
   segfault after a clean `rc=0` (both consumers independently `_exit()`); the
   `_lod` name-skip is a DC3 viewer heuristic in engine code; `GpuDevice` prints
   `device lost (reason 2)` before reporting successful init, on every run.

---

## 9. Owed work

| item | why | owner |
|---|---|---|
| **`band3` object factories** — 14 classes, `BandCamShot` first | The single blocker on every venue root, and therefore on `RndEnviron`/lights/shadows. Not optional and not routable around (§3). | **X4b** |
| **Move `operator<<(BinStream&, const ObjOwnerPtr<T1>&)` into `ObjPtr_p.h` under `#ifdef HX_NATIVE`** | Retires the explicit-instantiation recurrence trap (§2.1) permanently. | native |
| **`DirLoader.cpp:1014` passes a `FilePath` (a class) into `MakeString` varargs against `%s`** | UB; prints empty, so "Can't load old ObjectDir " names no file. A one-token match A/B (`mFile` → `mFile.c_str()`); `:1002` already does it right. A native-only companion `MILO_LOG` was added instead. | match lane |
| **`WorldInstance::PreLoad`/`PostLoad` rev ordering** (76.92% / 59.07%) | rb3-Wii disagrees with xenon-and-DC3. Runtime-neutral (§7), but a plausible match win. Needs an A/B, not a theory. | match lane |
| Carried from X3, untouched | `world/BeatClock.cpp` `SongPos`/`Phrase` audit; `RndMesh` bone limit "4"; `_inactive_crash_gem_top.mesh` 0 verts; `ScatterIncludes.cmake` transitive gap; `NodeCmp` A/B; the three faithful-vs-transcription triages | — |

---

## 10. Recommended X4b shape — and what X4a learned that changes it

X3 proposed X4b as "animate the character: `CharClip` + `CharDriver` +
`CharServoBone`". **That ordering should be reconsidered**, for one reason and
with three concrete carry-overs.

1. ★ **`band3` is now the critical path, not a side quest.** X3 ranked it item 4.
   X4a shows it gates the entire venue/lighting half of the ladder (§3), and
   `BandCharacter` + `BandWardrobe` are in the missing-factory list — so the
   *animation* milestone very likely needs it too. The first X4b question should
   be "how much of `band3` links?", not "does `CharClip` play?".
2. **`WorldInstance` is now exercised, and it was where the bodies were.** Both
   X4a defects (§2.2 and the retraction in §7) live in `world/Instance.cpp`.
   Anything that instances characters into a venue goes through `SyncDir` /
   `DeleteTransientObjects` / `LoadPersistentObjects` again — read §2.2's three
   witnesses before trusting any other ring walk in that file. **Grep the tree
   for `auto … = …->Refs()`**: the `auto`-by-value-on-a-ring-head shape is
   mechanical to find and silent to hit.
3. **Expect the failure to surface far from its cause.** X4a's segv was ~800
   objects downstream of a factory miss, and presented as a corrupt string
   length. The cheapest instrument that cracked it was printing the *class name*
   at the miss, not chasing the crash. Build that in before debugging X4b's
   loader problems.
4. **Determinism gates need a frame-count column now.** §5.1 shows a subsystem
   whose output is stable at 1 frame and not at 4. Any X4b gate that reads back
   after N frames should state N and prove determinism *at that N*.
5. **Keep the oracle in the loop** — X3 §9.5's standing advice held: rb3-Wii's
   faithful decomp, not DC3, was the witness that cracked §2.2, and the
   *difference* between them (§7) was itself the signal. Where xenon is
   byte-identical to DC3 in RB3 game-layer code, treat that as a hypothesis to
   test, not a provenance to trust.
