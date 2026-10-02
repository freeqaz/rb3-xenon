# W16-ND — follow-up to W16-NA: unopened rows, permuted-argument sites, kaleidoscope fix (2026-10-02)

**Branch** `w16-nd`, started off main `6ce0f685f`, rebased with `--rebase-merges` onto `461a1dd15` (after W16-NB
landed). Pre-rebase tip kept as `w16-nd-prerebase`. Fork branches `w16-nd-G` and `w16-nd-H` are merged into it with
`--no-ff`. **Not merged to main.**
**Ruler** `name_check` (graded). The permuter was not run.

Picks up the three items W16-NA left open in `docs/decomp/W16NA_REGISTER_ONLY_LEVER_CENSUS_2026-10-02.md` §5 and §7.

## 1. Whole-branch A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-nd-ab --patch <git diff 461a1dd15 w16-nd -- . ':!docs'>`
- **Worktree:** fresh, made with `scripts/setup_worktree.sh` at main `461a1dd15`.
- **Patch:** 10 files, `+29/−42`, source only. No map, splits or `symbols.txt` edits.
- **objdiff-cli:** sha `c1b7d952`, stable across legs.
- **Run dir:** `~/tmp/wt-w16-nd-ab/.ab_measure_runs/20261002-091746-ab_branch-2862220/`.

```
leg A: matched=50725 masked=24485 honest=26240 code%=53.566463  (recompiles: 0, settled)
leg B: matched=50727 masked=24485 honest=26242 code%=53.605335  (recompiles: 24, split=0, patch_steps=6, settle iterations: 2)
Δmatched=+2  Δmasked_equal=+0  Δhonest=+2  Δcode%=+0.038872pp  Δcode_bytes=+3984
Δfuzzy=+0.000161pp   (legA 59.879787 -> legB 59.879948)
unit net (ALL units) = +2   vs whole-binary Δmatched = +2
units at 100% [mpn ruler]: legA 440 -> legB 440
units at 100% [all-rows-fuzzy ruler]: legA 389 -> legB 390  (PlayerLeaderboards reached 100)
```

**Prediction, written before the run:** +2 functions / +3,984 B. That is the sum of the per-commit row diffs
(kaleidoscope 300, RndEnviron 700, BandConfiguration 136, fork G 1,364, fork H 1,484), assuming W16-NB had not moved
any of these rows. **Measured: +2 / +3,984 B.**

**Row-level diff of the archived legs** (`~/tmp/w16na/rowdiff.py`): 10 rows up, 9 of them to fuzzy 100 (3,984 B,
which equals Δcode_bytes exactly). **0 rows down. 0 rows off 100 on either the fuzzy or the `mpn` ruler.** No rows
appeared or vanished. The two functions are `RndPostProc::Load` and `WorkVerts::SetSameVerts` reaching `mpn` 100.

| row | B | fuzzy / mpn before | after |
|---|---:|---|---|
| `VocalTrack::UpdatePitchArrow` | 928 | 99.353 / 100 | 100 / 100 |
| `WorkVerts::SetSameVerts` | 756 | 96.735 / 97.873 | 99.947 / 100 |
| `RndEnviron::Load` | 700 | 98.314 / 100 | 100 / 100 |
| `Synth::DrawMeter` | 700 | 99.943 / 100 | 100 / 100 |
| `UIListSlot::Draw` | 592 | 99.865 / 100 | 100 / 100 |
| `PlayerLeaderboard::OnSelectRow` | 436 | 99.817 / 100 | 100 / 100 |
| `StickerProvider::SetStickers` | 344 | 98.953 / 100 | 99.302 / 100 |
| `NgPostProc::CheckPosterizeAndKaleidoscope` | 300 | 99.733 / 100 | 100 / 100 |
| `RndPostProc::Load` | 192 | 99.896 / 99.896 | 100 / 100 |
| `BandConfiguration::Copy` | 136 | 99.412 / 100 | 100 / 100 |

## 2. NgPostProc kaleidoscope: behaviour fix that now also matches

W16-NA found that `NgPostProc::CheckPosterizeAndKaleidoscope` had `kaleidoParams.x` and `posterParams.z` crossed, but its
spellings scored 99.707 or 97.24 against 99.733, so it stayed off that branch. Re-checked here on retail bytes:

- Retail multiplies the angle (`this+0x108`) by the pool constant at `0x82022FA8` and stores it to `sp+0x60`
  (`kaleidoParams.x`). It divides the constant at `0x820498E8` by the complexity (`this+0x100`) and stores it to
  `sp+0x58` (`posterParams.z`).
- Read from `orig/45410914/band.exe` `.rdata`: `0x82022FA8` = `0x3c8efa35` = 0.017453292 (degrees to radians), and
  `0x820498E8` = `0x40c90fdb` = 6.2831855 (2π).
- Our source stored 2π/complexity in `kaleidoParams.x` and angle·deg2rad in `posterParams.z`.

Applying W16-NA's patch reproduced its number, and showed something the census did not record: the row also fell
off `mpn` 100 (99.733/100 → 99.707/99.973, **−1 function**). The residual was ordering only. Retail loads the
deg2rad constant first and stores `kaleidoParams.x` before `posterParams.z`.

Eleven spellings were built, all with the correct assignment. The order of the four kaleidoscope assignments decides it:

| order | fuzzy |
|---|---:|
| W16-NA patch (`angle` local, `posterParams.z`, `kaleidoParams.x`) | 99.707 |
| `kaleidoParams.x` then `posterParams.z`, with or without locals (3 forms) | 97.24 |
| `posterParams.z` then `kaleidoParams.x`, no locals | 99.707 |
| (ky, kx, pw, pz) | 97.19 |
| (pz, kx, pw, ky) | 95.52 |
| (pw, pz, ky, kx) | 99.71 |
| (pz, ky, kx, pw) | 99.95 |
| (ky, pz, pw, kx) | 99.65 |
| **(pz, pw, kx, ky)** | **100.0** |

So the behaviour fix landed with a gain: 99.733 → 100, +300 B, and the row keeps `mpn` 100.

## 3. The six permuted-argument call sites

W16-NA's `argperm.py` flags a call whose argument registers receive a permutation of the same source registers. For
each site the question is whether the two source registers hold the same values on both sides, or are crossed. Each
was traced to where its source registers were defined.

| row | call | finding |
|---|---|---|
| `VocalTrack::UpdatePitchArrow` | `GetHarmonyScore` | `this` is r24 in retail and r25 in ours, the int argument the other way, across the whole body. Both pass `(this, arg)`. **Not a bug.** Fork G then closed the row (§4). |
| `RndEnviron::Load` | `Object::Load` | this/bs r30/r31 recolouring from the prologue. **Not a bug.** But see below. |
| `XboxEnumeration::Start` | `XMarketplaceCreateOfferEnumerator[ByOffering]` | retail r29 = `this+0x3c`, r28 = `this+0x40`; ours swaps the homes. Both pass `r8 = &handle`, `r7 = &size`. **Not a bug.** |
| `UIProxy::SyncDir` | `Transform::operator==`, `memcpy` | `world` / `&mOldXfm` homes swapped; both call `world == mOldXfm` and `memcpy(&mOldXfm, world)`. **Not a bug.** |
| `StoreMenuPanel::GetCrumbText` | `MakeString` | `result` / format-string homes swapped; both pass `(fmt, result)`. **Not a bug.** Two spellings tried, neither moved it (84.95, 99.318). |
| `BandConfiguration::Copy` | `memcpy` | both call `memcpy(&this->x, &c->x, 0x44)`; retail computes the source address first. **Not a bug.** Fixed (below). |

The other seven rows `argperm.py` flagged (all STRUCT_INSDEL) were checked too. `Splash` ctor stores the same `1`/`−1`
into the same fields. For `ReplaceObject`, `Spotlight::SyncProperty`, `MemAlloc`, `RndMatAnim::Load`,
`FingerShape::Update` and `AsyncFileWin::_ReadAsync`, a script (`~/tmp/w16nd/argdef.py`) compared the defining instruction of
each swapped source register on both sides, with the destination masked. All are identical: incoming parameters or
the same computed address. **0 behaviour bugs across all 13 flagged rows.**

Two of the six still gave match levers:

- **`RndEnviron::Load`: chained stream reads.** Retail keeps the `BinStream&` returned by the `Color` `operator>>`
  in r29 and reads the next two floats through it, at `mAmbientColor`/`mFogStart`/`mFogEnd` and
  `mFadeOut`/`mFadeStart`/`mFadeEnd`. Writing each group as one chained expression gives 98.314 → 100 (+700 B).
  W16-NA had filed this row permuter-only ("this/bs callee-saved swap from the prologue; no source lever"). The swap
  was caused by the missing chain.
  The tell is a `mr rN, r3` within a few instructions after a stream-operator call. The search window must allow
  for scheduling: here the `mr` sits two instructions after the `bl`. Scanning every row in W16-NA's instruction
  dumps (1,115 rows) found only this row, at both of its chains. There are no other instances in that population.
- **`BandConfiguration::Copy`:** binding the source `TargTransform` to a `const` reference before the assignment
  gives retail's source-first address order. 99.412 → 100 (+136 B).

## 4. The 44 unopened rows (forks G and H)

| fork | slice | rows | kept fixes | rowdiff vs `6ce0f685f` |
|---|---|---:|---|---|
| G | band3 | 18 | `VocalTrack::UpdatePitchArrow` 99.353 → 100 (define `spotlight` before the `mPlayer` local; one move fixed the r10/r11 and this/arg r24/r25 swaps). `PlayerLeaderboard::OnSelectRow` 99.817 → 100 (named reference to the row). `StickerProvider::SetStickers` 98.953 → 99.302 (name the two zero-scale products). | +0 fns / +1,364 B, 0 down |
| H | engine | 26 | `UIListSlot::Draw` 99.865 → 100 (`*(begin() + i)`). `Synth::DrawMeter` 99.943 → 100 (compute `levelNorm` after the bar geometry). `RndPostProc::Load` 99.896 → 100, mpn → 100 (read the discarded rev-0x10 fields as a `Sphere` through the stream returned by `bs >> b70`). `WorkVerts::SetSameVerts` 96.735 → 99.947, mpn → 100 (one `const Vector3&` for the source position). | +2 fns / +1,484 B, 0 down |

All 18 band3 rows were already at `mpn` 100, so fork G's gain is bytes only.

**Rows left, with what was tried:**

| row | B | verdict | tried |
|---|---:|---|---|
| `VocalTrack::RebuildHUD` | 2188 | permuter-only | `+=` (worse), named half-step (inert) |
| `MusicLibraryNetSetlists::ParseDataResultsIntoSetlists` | 1968 | permuter-only | drop `result` ref 99.13, `setlist` first 98.24, swap with `ownerXuid` inert |
| `VocalTrack::PrepareNoteTubes` | 1160 | permuter-only | const `GetNotes()`, `*(begin()+n)`: inert |
| `TrainerGemTab::DrawTails` | 888 | permuter-only | `overhang * x`, named in-loop `t`: inert |
| `TrainerGemTab::Render` | 776 | permuter-only | swapped compare operands: inert |
| `VocalPart::SetDifficultyVariables` | 768 | permuter-only | inline `log` 95.5, named `idx` 91.8 |
| `GameMic::ThreadProcessOneFrame` | 756 | permuter-only (SCHED) | named `GetSensitivity` 97.45; `AnalyzeBlock` args checked against `PitchDetector.h`, correct |
| `SetlistMergePanel::OnMsg` | 708 | permuter-only | named `first` at both sites 97.14, at one 98.87 |
| `CompressionEffect::Process` | 632 | permuter-only | operand swap, chain, named sample local in both orders, pointer form: inert |
| `VocalPart::GetBestHit` | 528 | permuter-only (SCHED) | octaves before pitch: inert |
| `FretHand::SetFingers` | 480 | permuter-only | two definition orders, both 96.58 |
| `NgLight::SetShadowTransforms` | 460 | stack-slot blocker | retail reuses `projMat`'s slot for the final `Matrix4` temporary; named local and temporary product much worse |
| `GemTrack::DrawBeatLines` | 448 | permuter-only | two definition orders: inert |
| `_S_sort<BSPFace>` | 424 | header-bound | STLport list sort, not opened |
| `AccomplishmentSongFilterConditional::Configure` | 384 | header-bound | direct `insert` 97.18; retail passes the Symbol by value through the shared `AddFilter` inline (10 files) |
| `Sphere::GrowToContain` | 372 | permuter-only | every product traced, meaning identical; FP operand order inside `Vec.h` `Scale`/`Subtract` inlines |
| `PackVector` | 344 | permuter-only (SCHED) | Y/Z reorderings of conversions, shifts, packs and OR: identical bytes |
| `BuildSphereStratified` | 340 | permuter-only | `phi`/`z` init swap (worse), add swap (inert), step order (worse) |
| `VignetteViewerProvider::RefreshVignettes` | 332 | permuter-only | `Node(i).Array(unk20)`: inert |
| `RndFlare::CalcScale` | 304 | permuter-only (SCHED) | `Cross` before `Length`, swapped `Dot` args (identical); named `sx` 71.8, named `d` 94.9 |
| `InterpTangent` | 280 | permuter-only | `Add(vtmp,vout)` and `+=`: identical bytes |
| `Gem::Poll` | 272 | permuter-only (SCHED) | three placements of `f5 - mStart`: 92.32 |
| `RndMesh::CollidePlane` | 264 | header-bound | order comes from shared `Plane::Dot` in `Mtx.h`; named third-vertex ref worse |
| `ctr_encrypt_fast` | 244 | permuter-only | source already records three inert spellings |
| `DxRnd::BeginTiling` | 228 | permuter-only | colour pack expanded in another order: identical bytes |
| `DxShaderMgr::SetVConstant`, `SetPConstant` | 220 ×2 | permuter-only (SCHED) | `c33` after the mask store 89.05 |
| `__partial_sort<GameGem>` | 220 | header-bound | load order inside `GameGem::operator<` (shared `GameGem.h`) |
| `RndTransformable::OnCopyLocalTo` | 188 | permuter-only | inline `for`, drop pre-declared locals, `while(--i)`: inert |
| `list<Instance>::operator=` | 180 | not a bug, header-bound | same values; iterator advance order is STLport's |
| `~RndFont` | 156 | permuter-only (SCHED) | `RELEASE` as two statements: inert |
| `TourProgress::UpdateMostStars` | 132 | permuter-only | inline sum loop: inert (four more spellings recorded at `GetNumStars`) |
| `Rnd::CompressTexture` | 124 | permuter-only (SCHED) | ctor body assignments in both orders: inert |
| `BoneDesc::operator=` | 96 | not a bug | same values; memcpy arg setup order. Explicit `operator=` with raw memcpy inert |
| `fn_822DB278`, `fn_822E3EAC` | 32 ×2 | blocker | guard-clear unwind funclets: retail frame 0x70, ours 0x60, parents at 100. Retail has 15 such funclets at 0x70 vs 2,039 at 0x60; cause not found |
| `RndTexBlender::DrawBlendList` | 548 | skipped | already filed permuter-only by W16-NA |

`BoneDesc::operator=` and `list<Instance>::operator=` both copy a `Transform` member whose `operator=` is a
user-defined memcpy. Whether an implicit copy would match was not tested, because it means editing `math/Mtx.h`.

## 5. Notes for the next lane

- **A row filed "no source lever" for a callee-saved swap can be hiding a shape difference.** RndEnviron's this/bs
  swap and UpdatePitchArrow's this/arg swap were both fixed by a source change elsewhere in the body (a chain; one
  definition moved), not by touching the swapped values.
- **Permuted argument registers are almost always recolouring.** 13 of 13 flagged rows here, against 3 real bugs in
  W16-NA's population. The check is cheap and mechanical (`~/tmp/w16nd/argdef.py`): compare each swapped source
  register's definition on both sides.
- **A behaviour fix that costs a row is worth re-spelling before accepting the cost.** Here the reordering search
  was ten builds, and it turned a −1 function into +300 B.
- Fork H suggests the census's OTHER_BLOCKER rows `RndPostProc::Save` and `RndText::Save` (0x54/0x58 temporary
  slots) may have the same kind of hidden aggregate-type cause as its `Sphere` fix. Not checked.

## 6. Gates

On the rebased tip `248c2e902`, after a full `./tools/ninja-locked` build (report identical to A/B leg B on every row):

```
[patch-state] [pairing] 1070/1070 declared compiled objects pair with a target (100.0%); 0 object(s) declared by >1 unit
[patch-state] OK: tree is a fixed point of 6 post-compile passes
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

The native gate ran last on the final code. Only this docs-only commit follows it. The branch touches no map,
splits, `symbols.txt`, `obj/Object.h`, `os/Debug.h` or math header, so the map validators had nothing to re-run. A grep
of `461a1dd15..w16-nd` finds no added source line or commit message that cites rb3-Wii or "the oracle", and no
Co-Authored-By line.

## 7. Not done

- Not merged to main.
- The permuter was not run.
- `RndPostProc::Save` / `RndText::Save` aggregate-type idea not tested.

Scratch: `~/tmp/w16nd/` (A/B patch and log, `lst.py`, `argdef.py`), fork reports under `~/tmp/w16nd/H/` and
`~/tmp/w16nd_G_base.json`.
