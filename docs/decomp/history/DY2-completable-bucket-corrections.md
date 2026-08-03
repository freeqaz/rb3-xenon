# Lane DY-2 — the COMPLETABLE bucket is NOT all source-reachable

**Date:** 2026-08-03 · **Baseline:** `725bb9ed`, matched 43,795 / code% 39.527752 /
honest 21,085 / `total_code` 10,688,804 / units at 100% **246 of 1,025**
(read from a settled worktree `report.json`, never hardcoded).

Lane DX-2 established that **zero map-only wins remain** and that the one-away
frontier is now a pure source-authoring problem. That is right about the map. It
is **wrong to read `COMPLETABLE` as "source can finish this"** — and the census
says so itself ("COMPLETABLE means reachable IN PRINCIPLE... it does not mean the
pin is right"). This lane adjudicated individual members on **retail bytes** and
found a floor of structurally unreachable units.

## Controls run first (all three as briefed)

| control | required | observed |
|---|---|---|
| `reachable_ceiling.py --selftest` | 3/3 | **3/3 PASS** |
| `--sabotage cls-truncate` | FAIL rc=6 | **FAILED rc=6** (v1 anchor wrong on 4/7) |
| `--sabotage retail-blind` | REFUSE rc=5 | **REFUSED rc=5**, no census emitted |

Census regenerated, not cached: AT_100 **246**, COMPLETABLE **46**,
ANON_BLOCKED 164, MIXED 559, OD_REGION 10 · source-only ceiling **292 / 1,025**.

## Correction 1 — TRUNCATED TARGET EXTENTS (a new artifact class)

**Frameless tail-call functions have NO `.pdata` record in retail.** Verified
directly: `.pdata` has no entry at `0x8270D7E8`, `0x8270D7F8` or `0x8270D800` —
the next record after `0x8270D770` is `0x8270D848`. With no `.pdata` to derive
from, the splitter synthesises boundaries and can cut a function at an interior
8-byte alignment/padding boundary, **severing its terminating tail `b`**. Our
source is then structurally unable to match: it correctly emits the branch that
the target extent does not contain.

`MoggClip::FadeOut` is the worked case. Retail extent = 16 B / 4 instructions
ending on `lfs` — a function cannot end on a load. The real function is 20 B:

```
8270D7E8  lis  r11, lbl_8202EED0@ha
8270D7EC  fmr  f2, f1
8270D7F0  lwz  r3, 0x78(r3)
8270D7F4  lfs  f1, lbl_8202EED0@l(r11)
8270D7F8  b    fn_8270C4B0        <-- split off as its OWN symbol
```

The severed branch was then *named* by the map as `??3RndLight@@SAXPAX@Z`
(`RndLight::operator delete`), which is incoherent: `fn_8270C4B0` is referenced
from `Faders.s`, `MasterAudio`, `CrowdAudio`, `SongPreview`, `MetaMusic`,
`CalibrationPanel` and owns a `.rdata` jump table — it is `Fader::DoFade`, audio
code, not a deallocator. **Our compiled `FadeOut` is already byte-correct.**

`StorePreviewMgr`'s `MsgSource::SetType` vbase adjustor thunk is the same thing
and even more visually obvious — `.endfn` closes immediately *before* the
thunk's own `b fn_823E3630`. `arraylist`'s `array_list_add` likewise: 8 B / 2
instructions ending on `lwz r4, 0x4(r3)`, tail `b fn_82B84EF8` split off at
`0x82B84FC0`.

**Instrument + control.** Scanned 69,304 `.fn` blocks (4,390 stale pre-2026-07-15
`.s` files excluded) for a last instruction that is not a control-flow
terminator: 1,165 (1.68%). Of those, 613 end on `bl` — *legitimate* (a call to a
noreturn function). The decisive class is "ends on a load/store/arith": **508**.

| population | hard-truncated | rate |
|---|---|---|
| NULL — named rows already at mpn 100 | 30 / 21,085 | **0.142%** |
| CHARGED — named rows sub-100 | 64 / 6,862 | **0.933%** |
| **enrichment** | | **6.56×** |

⚠ **The null is NOT zero (30 rows), so this is a calibrated suspicion, not a
classifier.** Every case below was adjudicated on retail bytes, not on the flag.
Exactly **3** COMPLETABLE blockers are hard-truncated: `MoggClip::FadeOut`,
`arraylist::array_list_add`, `StorePreviewMgr`'s `SetType` thunk.

## Correction 2 — `default/Main` is unreachable (a *unique* encoding)

`main`'s single mismatch is target `bcl 20, lt, fn_82270080` vs our
`bl ?Run@App@@QAAXXZ` — retail bytes `42 80 D1 F1`, i.e. opcode 16 (`bc`) with
BO=20 (branch-always) and LK=1: functionally a `bl`, different encoding.

Scanned all of `.text` (raw `0x264E00`–`0xC41E00`):

* plain `bl` (opcode 18, LK=1): **197,239**
* `bcl` branch-always (BO=1z1zz, LK=1): **1** — and it is *this* instruction
* `bcl` conditional with LK=1: **0**

No source construct makes MSVC choose `bc`+BO=20 for a normal call; if one
existed it would not be unique in 197k calls. **Declined on evidence.** The
census cannot see this: `Main` sits at the top of its "cheapest completable
units" list at 96.84%.

## Correction 3 — three COMPLETABLE blockers are MAP MISPAIRS

Adjudicated individually on retail bytes:

* **`?Deactivate@FlowQueueable@@UAAX_N@Z`** — signature takes a `bool`; the body
  saves a **float** argument (`fmr f31, f1`, no preceding call) and calls
  `Keys<Vector3>::Add` twice plus `Keys<Vector2>::Add`. It is an animation
  keyframe setter, not `Deactivate`.
* **`?Clear@HamDriver@@QAAXXZ`** — a non-static member, so it must consume
  `this` in r3; the 3-instruction body instead *loads* r3 from a global
  (`lwz r3, lbl_82CC8F4C(r11)`) and tail-calls
  `ContextWrapperPool::FailAllContexts`.
* **`??$MakeString@HH@@YAPBDPBDHH@Z`** (`default/Rnd_NG`, `0x82399348`) — the
  body compares two pointers, computes `(end − begin) >> 4` (a `vector<T>` size
  with `sizeof(T) == 16`) and tail-calls
  `vector<HamIKEffector::Constraint>::operator=`. That is a self-assignment-
  guarded vector assign, not `MakeString<int,int>(const char*, int, int)`. The
  frame delta corroborates: ours is +0x810 larger because `FormatString` carries
  a ~2 KB stack buffer the target never allocates.

## ⛔ Two instruments built, controlled, and REFUTED — do not re-fund

1. **`this`-consistency screen** ("a non-static member whose body never reads r3
   as a live-in is a mispair"). First version flagged **49% of already-matched
   rows** — vacuous. The cause was real and fixable (`bl __savegprlr_*` /
   `__savefpr_*` do **not** clobber r3, but were treated as defining it). After
   the fix the control read 8.39% null vs 9.95% charged = **1.19× — no better
   than chance.** Refuted. This sits below the 1.25×/1.95× band already rejected
   here for classifier use.
2. **Float-argument screen** ("body's FPR live-ins disagree with the mangled
   signature"). Its first version fired on
   `?GetCorrection@PitchCorrectedVoice@Synapse@DSP@@QAAMXZ` because it read the
   `f1` **return value** of a preceding `bl` as an argument. After teaching it
   that a call defines `f1`, the whole one-blocker COMPLETABLE set yields exactly
   **one** inconsistency — `FlowQueueable` — with both `MoggClip` rows correctly
   reading as *consistent* float-takers. Useful as a spot check; too narrow to be
   a lever, and it has a known false-negative mode (an early `bl` masks a genuine
   float parameter).

⇒ **Individual adjudication on retail bytes remains the only reliable method.**

## Net effect on the ceiling

At least **7 of the 46 COMPLETABLE units cannot be closed by source**:
`Main` (unique encoding), `MoggClip` / `arraylist` / `StorePreviewMgr`
(truncated extents), `FlowQueueable` / `HamDriver` / `Rnd_NG` (map mispairs) —
plus `SkeletonDir` and `FilterQueue`, already declined on evidence by earlier
lanes.

So the briefed **source-only ceiling of 292 is optimistic by at least 7**; the
defensible figure is **≤ 285**. This does not contradict DX-2 — the map really
is drained — it corrects what "COMPLETABLE" licenses you to expect.

## Also declined here, with reasons

* **`PreloadPanel::Load`** — reachable in principle, but retail's implementation
  is *different code*: a `FileCache` (`Clear`/`StartSet`/`Add`/`EndSet`) built
  from a `DataArray` and gated on `FileExists(DirLoader::CachedPath(..))`, where
  ours is the Wii content-mounting path (`TheWiiContentMgr`, `MountContent`,
  `SongMgr::HasSong`). A ~130-instruction platform rewrite with a different
  member layout. Out of scope for one lane; not a defect to patch around.
* **`SoftParticleBuffer::BlurSurface`** — 39 deletes / 10 inserts, frame
  Δ −0x40: we are *missing* ~29 instructions plus float constants. A real body
  port, deferred rather than guessed at over 660 bytes.
* **`RealGuitarTrackWatcherImpl::InTrill`** — 85.71%, same size (112 B), same 30
  instructions; the only delta is that retail loads `mTrack` (+0x68) *late* (via
  r31) while we hoist it. Two source restructurings were tried — inlining
  `Track()` into the call, and moving its declaration after `tick` — and **both
  regressed identically to 63.0%** (the prologue switches from `__savegprlr_29`
  to individual `std`s). Reverted; residual is scheduling and the permuter is
  banned.

---

## MEASURED OUTCOME OF THIS LANE (added after the work landed)

Whole-binary A/B (`tools/ab_measure.py`, both legs settled to a zero-work build
in the same worktree, same-ruler guard passed, leg B 1,073 recompiles + a forced
re-split with `renamer_patched=1046`). Leg A is main at `40abb376` — lane DY-1
landed 4 commits mid-run, which is why leg A reads 43,814 and not the 43,795
this lane was briefed with. **Deltas compose; absolutes do not.**

```
Δmatched  +11   Δhonest +11   Δmasked_equal +0
Δcode%    +0.011113pp   Δcode_bytes +1188   Δfuzzy +0.003047pp
units at 100%: 246 -> 253  (Δ+7; 7 reached, 0 fell off)  -- BOTH rulers agree
unit net (ALL units) = +11 == whole-binary Δmatched   => no hidden regressions
```

Seven units closed, confirmed independently by a regenerated-census SET-DIFF
(all seven were `COMPLETABLE -> AT_100`, none fell off):
`ChooseColorPanel`, `InterstitialPanel`, `VoiceoverPanel` (game/meta_band),
`FIFOSampleBuffer`, `json_object`, `linkhash`, `printbuf`.
Plus two non-closing gains: `PreloadPanel` 51->52 and `arraylist` 3->4.

★ **These closures CONSUMED ceiling, they did not CONVERT it.** The source-only
ceiling (AT_100 + COMPLETABLE) is **292 before and 292 after** — COMPLETABLE fell
46 -> 39 by exactly the seven closed. That is the opposite of DX-2, whose closures
were `ANON_BLOCKED -> AT_100` and therefore *raised* the ceiling. Do not report a
unit closure as a ceiling gain without checking which bucket it came from.
(3 units also moved `MIXED -> ANON_BLOCKED`: their named sub-100 rows were
resolved, leaving only anon residue. Not a regression.)

**The corrected ceiling, restated on the final tree:** of the 39 remaining
COMPLETABLE units, **9 are structurally unreachable** — the 7 proven here plus
`SkeletonDir` and `FilterQueue`, declined on evidence by earlier lanes. All 9
still carry the `COMPLETABLE` label. So ~**30** are genuinely source-workable and
the defensible ceiling is **<= 283**, not 292.

## ⚠ Two coupled-lever lessons, both paid for in this lane's own measurements

1. **A source half and a map half each measure +0; only together do they pay.**
   `FIFOSampleBuffer::ptrBegin` needed `const` in the headers *and* the map row
   `?ptrBegin@FIFOProcessor@soundtouch@@MAAPAMXZ -> MBAPAMXZ`. The coordinator's
   first combined A/B was generated as `git diff -- src/`, which silently dropped
   the map row: the result was **+9 with `default/SoundTouch` FALLING OFF 100%**
   (22->21). Including the one-line map row turned that into **+10 with 0 units
   falling off.** The map half is provable from C++ rules alone — both symbols
   override the same pure virtual, so they must share cv-qualification — with no
   binary evidence needed.
2. **A standing DO-NOT can rest on a structurally invalid witness.**
   `obj/Object.h` carried an explicit DO-NOT against removing `New<T>`'s
   `MILO_FAIL` guard, justified by `JoypadInitCommon` matching 100% *with* the
   guard. That inference is void: `JoypadInitCommon` calls `New<T>` **out of
   line**, and a callee is a masked relocation argument, so the row scores 100
   regardless of what the callee contains — it cannot witness the guard either
   way. The real defect underneath was `Joypad.cpp` creating a bare `Hmx::Object`
   where retail creates a `MsgSource` (for a bare `Hmx::Object`, `dynamic_cast`
   is an identity conversion, so MSVC inlined a 3-instruction body where retail
   keeps the `bl`). **The wrong class scored a clean 100% before and after the
   fix** — the metric-hidden-bug class. Measured here: removing the guard
   *together with* the class fix is **+1 matched / +72 B / +1 unit with 0 units
   falling off**, against the DO-NOT's claimed -600 B.

Native gate: **PASS 18/18, rc=0, 0 errors, 0 warnings, 0 SKIPs**, run twice —
once after the source/map work and again after the `obj/Object.h` change (stale
binaries deleted first, all 18 targets relinked both times).
