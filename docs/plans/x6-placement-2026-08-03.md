# X6 — the crowd is placed, and the placement was shipped in the file all along

**Date:** 2026-08-03
**Predecessor:** [X5](x5-scene-2026-08-03.md) "a character renders inside the venue, and the crowd was there the whole time"
**Branch:** `x6-placement`, from `main` @ `233cf1e3`
**Engine:** `milo-native-engine` pinned at **`138e1606…`**, **zero engine edits**
**Change surface:** `native/src/main_render.cpp` (driver) + a new shared TU
(`src/system/bandobj/BandConfiguration.{h,cpp}`) + `Band.cpp` shim removal +
`objects.json`.

---

## Verdict

★★ **CHARACTERS ARE PLACED.** `small_club_01` renders **30 crowd members at 30
distinct world positions** standing on the club floor facing the stage, and
`arena_01` renders **4700 crowd members** filling the arena floor in front of
the stage. `rc=0`, deterministic, every position read from the asset.

★★★ **AND NOTHING NEEDED TO BE SCATTERED. X5's handoff is refuted.** X5 handed
placement off as procedural work — *"`WorldCrowd` scatter onto `mPlacementMesh`
— 6 `WorldCrowd` objects load, **none runs**"*. There is no scatter to run.
`WorldCrowd::OnRebuild` — the routine that would compute positions from
`mPlacementMesh` — is `return 0;` in **rb3-xenon, in the rb3-Wii oracle, AND in
DC3**. That is retail behaviour, not a decomp gap: the scatter is a Milo-editor
routine compiled out of the shipping game, and every crowd position is **baked
at author time and serialized**. `WorldCrowd::Load` (`world/Crowd.cpp:361-368`)
deserializes a `std::list<Transform>` per `CharData` straight into
`mMMesh->Instances()`.

★ **The data was resident the whole time — since X4d, in the same `rc=0` runs
that reported "no crowd".** Measured across six venue roots: **300 / 300 / 4700
/ 4643 / 8400 / 0** baked crowd positions, and in every venue the number of
*distinct* positions equals the number of instances exactly. Nothing is stacked
and nothing is duplicated.

⚠ **Partial, stated plainly.** Three things did not land, and §6 is the honest
table: (1) the asset's own `mShowing` flags select **zero** crowds in 4 of the 6
venues, so the *default* renders a crowd only in the two small clubs — the full
crowd needs `--crowd-all`; (2) the engine's real impostor-billboard draw path
emits **nothing** on this backend, so the driver substitutes real geometry — a
disclosed **mechanism** substitution, not a placement one; (3) `BandConfiguration`
is ported and `BandInit` is provably non-regressed, but it **cannot be scored by
objdiff yet** and it **places nothing at runtime yet**.

---

## 1. Gate results

| # | Gate | Result | Evidence |
|---|---|---|---|
| a | Full native gate, **fresh** (`rm -rf native/build` first), rc=0 | ✅ **PASS — 18/18** | `NATIVE GATE: PASS (rc=0, 0 errors, 0 warnings, 18/18 target(s) verified)`, every target `relinked this run`, **zero SKIPs**. Cache seeded with an explicit `cmake -S native -B native/build -DMILO_ENGINE_PATH=… -DDawn_DIR=…` first, per X4c's warning that the gate's own `cmake` line omits them and lets three targets silently **SKIP** while still reporting PASS. |
| b | Zero `milo-native-engine` edits | ✅ **PASS** | engine `HEAD` == pin `138e1606a202f2b3226e38a8f28010b096f3d441`. I made zero engine edits and no engine change request arose. ⚠ The seven-week-old foreign uncommitted edit to `src/platform/FxSendNative.cpp` that X4d and X5 both disclosed is still there, still not mine, still off the load/render path. Left untouched. |
| c | Shared-`src/` edit `HX_NATIVE`-gated, X360 arm faithful, **verified at symbol granularity** | ✅ **PASS** | §4. `?BandInit@@YAXXZ` measured **before and after**: 98.1% / 252 instructions / 95 `diff_arg`, **identical, with the same mismatch list item for item**. Whole-file `cmp` would not have shown this (X4d's rule); the symbol diff does. Full X360 build `rc=0`. |
| d | PNG determinism ×2 on every cited image | ✅ **PASS** | §7 |
| e | X3/X4a/X4b/X4c/X4d/X5 evidence non-regressed | ✅ **PASS** | §7 |
| f | Is `main` broken by a decomp lane? | ✅ **NO — healthy this lane** | `main` @ `233cf1e3` built `rb3-render` clean on first try and the full X360 build returned `rc=0`. This happened in three of the last four lanes; it did not happen here. That is evidence about *this week*, not about the gap — the underlying "match lanes do not cover the native build" hole is still open. |

### 1.1 Native gate

**✅ PASS — 18/18, rc=0, 0 errors, 0 warnings, every target `relinked this run`, ZERO SKIPs.**
Recorded at `/home/free/tmp/laneX6/evidence/x6-native-gate.log`.
The `rb3-render` target — the one this lane changes — builds clean and runs
`rc=0` on five of six venue roots throughout §2-§3.

---

## 2. ⛔ The finding: placement is shipped data, and the scatter does not exist

### 2.1 Why I measured instead of implementing

The charter's standing rule, and X5's own §10.2 lesson, is to **re-derive an
inherited cost estimate from the mechanism rather than from the handoff**. X5
handed me "write the `WorldCrowd` scatter". Before writing one I asked what
`WorldCrowd` actually does at runtime, and the answer closed the item:

| repo | `WorldCrowd::OnRebuild` |
|---|---|
| rb3-xenon | `world/Crowd.cpp:171` → `DataNode WorldCrowd::OnRebuild(DataArray *) { return 0; }` |
| rb3-Wii (oracle) | `world/Crowd.cpp:131` → `{ return 0; }` |
| DC3 | `world/Crowd.cpp:169` → `{ return 0; }` |

Three independent decomps of the same engine all produce an empty body. That is
not three identical decomp gaps; it is the shipping game. `mNum`,
`mDef.mDensity`, `mDef.mRadius` and `mCenter` are inputs to a **Milo-editor**
routine that was compiled out, and at runtime none of them is read for
placement.

### 2.2 Where the positions actually come from

`WorldCrowd::Load`, `world/Crowd.cpp:361-368` (rev ≥ 0xE):

```cpp
std::list<Transform> xfms;
d >> xfms;
if (it->mMMesh) {
    it->mMMesh->Instances().clear();
    FOREACH (xfmIt, xfms) {
        it->mMMesh->Instances().push_back(RndMultiMesh::Instance(*xfmIt));
    }
}
```

Every crowd member's world transform is one entry in that list. rb3-xenon's
`Load` is a **strict superset** of the oracle's — same fields, same order, same
rev gates, plus a null-`mMMesh` guard the Wii body lacks.

### 2.3 Measured — six venue roots

`ReportCrowdPlacement` prints absolute positions, per-archetype bounding boxes
and a pairwise position-set overlap matrix. **Absolute positions and distinct
counts, not aggregates** — X5's lesson is that a count of N instances reads
identically whether they are scattered across the audience or stacked on one
spot.

| venue root | `WorldCrowd`s | baked instances | DISTINCT positions | `mShowing` = yes |
|---|---|---|---|---|
| `small_club/small_club_01` | 6 | **300** | **300** | 1 |
| `small_club/small_club_02` | 6 | **300** | **300** | 1 |
| `arena/arena_01` | 18 | **4700** | **4700** | **0** |
| `big_club/big_club_01` | 24 | **4643** | **4643** | **0** |
| `festival/festival_01` | 18 | **8400** | **8400** | **0** |
| `video/video_05` | 0 | 0 | 0 | — |

> **instances == distinct positions in every venue.** Not one coincident pair
> anywhere, across 18343 crowd members. Placement is complete, correct and
> shipped; `video_05` ships no crowd at all, which is consistent with it being a
> video backdrop and is not a failure.

`small_club_01` detail: x ∈ [-161, 161], y ∈ [-298, -22], z ∈ {68.6, 69.5,
73.6, 74.5} — a **constant height per archetype**, with the male archetypes
exactly 5.0 units above the female ones, spread across the audience area. (The
5.0 is measured; what it *represents* — a height offset baked into the
billboard's anchor, or the archetype's own stature — I did not determine, and
am not asserting.)
These are floor positions in a room whose bbox spans ~500 units. **They are not
near the venue origin, which in `small_club_01` is at ceiling height** — the
exact defect X5 named.

### 2.4 Why none of it was visible — a driver defect, again

`rb3-render` draws a flat `std::vector<RndMesh*>` and calls `DrawShowing()` on
each element. **`WorldCrowd` is an `RndDrawable` but not an `RndMesh`**, so its
fully-ported 355-line `DrawShowing()` (`world/Crowd.cpp:1062`) was unreachable
from any code path in this driver. Retail never has this problem: a venue draws
through the `RndDrawable` tree, where the dir issues its drawables directly.

★ **This is the same shape as X5's `ObjDirItr` finding, one level up: faithful
engine code, unfaithful driver.** It is the third consecutive milestone in which
the missing subsystem was present and correct in memory and absent only from the
driver's walk. It must not be "fixed" in `src/`.

---

## 3. ⛔ Wiring the real draw path in was necessary and NOT sufficient

Collecting `WorldCrowd`s and issuing them through `RndDrawable::Draw()` — the
engine's own entry point, which checks `mShowing` and then calls
`DrawShowing()` — produced a **byte-identical frame**:

| run | crowds showing | instances issued | PNG SHA |
|---|---|---|---|
| `RB3_NO_CROWD_DRAW=1` | 0 | 0 | `d7963b8c1e6d5711…` |
| default | 1 | 30 | **`d7963b8c1e6d5711…`** |
| `--crowd-all` | 6 | **300** | **`d7963b8c1e6d5711…`** |

**300 instances and not one pixel changed.** That is the decisive form of the
measurement: had the data been the problem, the 30-instance and 300-instance
runs would differ from each other. They do not differ from the *control*.

Root cause, ⛔ **DEFECT** (`world/Crowd.cpp:1062-1202`): the native arm of
`WorldCrowd::DrawShowing` renders each archetype character to a cached texture
through `gImpostorCamera` and composites the billboard with
`RndMat::kBlendAdd`. A nested render-to-texture pass inside an open draw pass
emits nothing on this WebGPU/Dawn backend, so the cached impostor stays black —
and **additive blend of black is the identity** (`0 + scene = scene`), which is
precisely why the failure is byte-identical rather than merely wrong. A failure
mode that produces an *exactly unchanged* frame is indistinguishable from "the
feature is off" unless you vary the input volume, which is what the 30-vs-300
row above does.

---

## 4. What the driver does instead, and exactly what is synthesized

⚠ **This is the paragraph the charter's most important instruction is about, so
it is stated precisely.**

**NOT synthesized — every crowd position.** Each transform is read from
`mMMesh->Instances()[i].mXfm`, deserialized by real engine code from the shipped
`.milo`. No position in any frame in this document was computed, guessed,
interpolated, or hand-picked by me. I did not author a scatter, and I declined
to write one even after finding the data, because a scatter I wrote would
produce plausible positions that are indistinguishable in a screenshot from the
asset's real ones.

**IS substituted — the rasterization mechanism.** Because the impostor RTT emits
nothing (§3), the driver draws the archetype's **real skinned geometry** at each
baked transform rather than a billboard textured from it. Retail carries this
concept itself as the "3D crowd" subset (`Draw3DChars` / `m3DChars`, the members
promoted to real geometry by a cam shot); this applies it to every instance.
The consequence to keep in mind when reading the frames: **the crowd is drawn at
higher fidelity than retail would draw it**, not lower, and at a cost retail
would not pay (28200 draws in `arena_01`).

**Also fixed here:** the archetype meshes are removed from the flat mesh loop.
An archetype is a template — `WorldCrowd::CharDef::mChar` is documented *"The
character to use as the archetype"* — and drawing it at its own default
transform is exactly what put eight coincident characters on the venue origin in
X4d and X5. Their `1 DISTINCT world position <== ALL STACKED` was a **correct
measurement of the wrong objects**.

### 4.1 Measured

| | `RB3_NO_CROWD_DRAW=1` | default | `--crowd-all` |
|---|---|---|---|
| `small_club_01` flat mesh draws | 162 | 114 | 114 |
| crowd members drawn | 0 | **30** | **300** |
| placed crowd draws | 0 | **180** | **1800** |
| distinct colours | 112847 | **118567** | **126687** |
| PNG SHA | `d7963b8c…` | `5282bd27…` | `2f36c1e3…` |

`arena_01`, `--crowd-all`: **4700 crowd members, 28200 placed draws**, 395 of
485 flat meshes drawn, coverage 74.49%, `rc=0`.

⚠ **Coverage does not move in `small_club_01` (38.92% in all three runs).** That
is not a contradiction, and it is the second time this lane that an aggregate
failed to separate two worlds: the crowd occupies interior pixels that were
already covered by the wall behind them. **The colour count moved and coverage
did not** — which is why the oracle here is distinct positions and absolute
bboxes, never coverage.

---

## 5. `BandConfiguration` — the band-slot placement path

rb3-xenon had only a **factory-only shim** (`bandobj/Band.cpp:66-73`): correct
classname and `NEW_OBJ`, but no members, no `Load`, no `SyncPlayMode`. Any venue
containing a `BandConfiguration` instantiated a bare `Hmx::Object` and loaded it
as one — so the four band-member per-play-mode transforms were **silently never
read** and no band member was ever teleported. A functional hole, not just a
decomp gap.

Ported from the rb3-Wii oracle (48 + 175 lines). ★ **Every dependency was
already present in rb3-xenon and none needed prerequisite work** — `Waypoint`,
`Character::Teleport` (`char/Character.cpp:486`, a byte-for-byte equivalent of
the Wii body), `BandCharacter::Teleport`, `BandWardrobe::{GetPlayMode,
FindTarget, mVenueNames, GetCharacter, SetModeSink}`, `DataGetMacro`. There is
no `PlayMode` enum to port: the mode is a `Symbol` resolved against the
`BAND_PLAY_MODES` DTA macro at runtime.

**The X360 arm is the retail body.** The only `HX_NATIVE`-gated addition is a
`MILO_WARN` on a slot whose `targName` does not resolve, because an unplaced
member is otherwise indistinguishable from an empty slot. I dropped the rb3-Wii
repo's `RB3_PLACEMENT_PROBE` block — that is that repo's native tooling, not
ours.

### 5.1 ⚠ Verified at symbol granularity, and why that mattered

The shim existed for a real reason: retail's `BandConfiguration::Init()` is a
header-inline `{ Register(); }` that `/Ob2` inlines **directly into
`BandInit()`**, so an out-of-line stub desyncs `BandInit`'s instruction
sequence. The real header keeps `Init()` inline for exactly that reason.

| | `?BandInit@@YAXXZ` |
|---|---|
| `main` @ `233cf1e3` (shim) | 98.1% normalized, 252 instructions, 95 `diff_arg` |
| this branch (real TU) | **98.1% normalized, 252 instructions, 95 `diff_arg`** |

**Identical, and the mismatch lists match item for item** — including
`[22] ?StaticClassName@BandConfiguration@@…` and
`[23]/[25] ?NewObject@BandConfiguration@@…`, which are present and correctly
paired on both sides. Full X360 build `rc=0`.

### 5.2 ⛔ Its objdiff position: NOT YET SCOREABLE

Stated plainly rather than implying a match gain that has not happened.
`BandConfiguration.obj` compiles and carries every expected symbol
(`?SyncPlayMode@BandConfiguration@@QAAXXZ` et al.), **but objdiff cannot score
it**: there is no `config/45410914/splits.txt` entry for `BandConfiguration`,
therefore no target `.obj` to pair against, therefore `configure.py` emits no
objdiff unit. The dual yield the charter anticipated is **deferred**, not
collected: mapping the retail address ranges into `splits.txt` is a separate
exercise this lane did not do.

⚠ **It also places nothing at runtime yet.** `SyncPlayMode` needs a live
`TheBandWardrobe` and real `BandCharacter`s, and `bandobj/BandWardrobe.cpp` +
`BandCharacter.cpp` are still outside the native build behind the
`ScatterIncludes` dedupe lane. **This lands the path, not the placement.**

⚠ **Trap recorded for the next porter:** `obj/ObjMacros.h` must be included
**before** `obj/Object.h`. `obj/Object.h` pulls the dialect macro set in which
`INIT_REVS` takes `(rev, alt)` and `DECLARE_REVS` / `REGISTER_OBJ_FACTORY_FUNC`
do not exist — the class then **silently stops parsing at `DECLARE_REVS`** and
its members vanish, with 20 errors pointing everywhere except the include order.

---

## 6. Per-subsystem verdict table

| subsystem | verdict | evidence |
|---|---|---|
| **Crowd PLACEMENT** | ★ **ALIVE — REAL, first time** | 30 members at 30 distinct floor positions in `small_club_01`; 4700 in `arena_01`. Every position from the asset. §2, §4 |
| **Crowd placement DATA** | ✅ **VERIFIED COMPLETE, all venues** | 300/300/4700/4643/8400/0 baked positions; instances == distinct in every one. §2.3 |
| **`WorldCrowd` scatter** | ✅ **CLOSED — does not exist** | `OnRebuild` is `{ return 0; }` in all three decomps; editor-only, stripped from retail. X5's "none runs" **retracted** (§8.1) |
| **Archetype origin-stacking** | ✅ **FIXED** | archetypes removed from the flat loop; X4d/X5's "9 characters at 1 position" was a correct measurement of templates |
| **`WorldCrowd` impostor billboard draw** | ⛔ **DEAD ON THIS BACKEND** | byte-identical frame at 300 instances; nested RTT + additive-black. §3 |
| **Crowd visibility policy** | ⚠ **UNRESOLVED — the top handoff item** | asset `mShowing` selects **0 of N** crowds in 4 of 6 venues. §6.1 |
| **`BandConfiguration` TU** | ✅ **PORTED** | real TU replaces the shim; `BandInit` 98.1% identical before/after. §5 |
| **`BandConfiguration` objdiff score** | ⛔ **NOT SCOREABLE** | no `splits.txt` entry → no target `.obj` → no objdiff unit. §5.2 |
| **Band-slot placement at runtime** | ⬜ **UNREACHED** | needs live `TheBandWardrobe` + `BandCharacter`, both behind the `ScatterIncludes` lane. §5.2 |
| **Venue geometry / Mats / Tex / `RndEnviron`** | ✅ **ALIVE** | unchanged from X5 |
| **Skinned characters / animation** | ✅ **ALIVE** | unchanged from X5; now drawn at 30-4700 real positions instead of 8 coincident ones |
| **Player anchors / `player0` refs** | ✅ **unchanged from X5** | not touched this lane; X5's stand-in still a stand-in |
| **Camera shots** | ⬜ **UNREACHED** | `BandCamShot` misses; X4d's base-class bind stays refuted and untouched |
| **Audio / synth, PostProc, `ThreadCallInit`** | ⬜ **UNREACHED** | unchanged; did not bear on this work |
| **`video_05` renders empty** | ⚠ **CARRIED, confirmed NOT mine** | fails identically (`rc=1`, same 2 gates) with `RB3_NO_CROWD_DRAW=1`. Pre-existing from X4d. §7 |

### 6.1 ⚠ The one real judgement call, left unmade deliberately

The asset's own `mShowing` flags mark **1 of 6** crowds live in `small_club_01`
and **0 of 18 / 0 of 24 / 0 of 18** in `arena_01` / `big_club_01` /
`festival_01`. Honouring them — which the default does — therefore renders a
crowd in only two of six venues, and none in the arena.

Those flags cannot plausibly be the runtime truth: a rhythm game does not ship
an arena with an empty floor. Retail almost certainly toggles them at load
(the `_2_ps3` / `_4_ps3` names imply a platform/quality selector), and **that
selector is not ported**. So:

- **Default = honour `mShowing`.** Zero judgement from me; strictly faithful to
  shipped data; renders 30 in the small clubs and 0 in the arena.
- **`--crowd-all` = override the flags.** Measured safe — §8.2 proves it is not
  double-drawing — and renders every shipped seat exactly once.

I did **not** promote `--crowd-all` to the default. Choosing which crowds are
visible is a smaller decision than inventing positions, but it is still my
decision rather than the asset's, and the next lane should make it with the
selector in hand rather than inherit it from me. Both frames are in evidence.

---

## 7. Determinism and non-regression

| image | SHA | ×2 |
|---|---|---|
| `x6-E1-small_club_01-crowd-PLACED.png` | `5282bd275159f10b…` | ✅ |
| `x6-E2-small_club_01-all-300.png` | `2f36c1e369314e11…` | ✅ |
| `x6-E3-arena_01-4700-crowd.png` | `218cf68dd5a019a7…` | ✅ |
| `x6-CONTROL-crowd-draw-off.png` | `d7963b8c1e6d5711…` | ✅ (×3) |

★ E2 and E3 were re-rendered **after** the fresh native gate wiped and rebuilt
`native/build` from scratch, and reproduce the SHAs recorded before it. So these
are stable across a full toolchain rebuild, not just across two consecutive runs
of one binary.

⚠ **A correction I am recording rather than quietly fixing.** The first draft of
§4.1 carried a `--crowd-all` PNG SHA (`88b0a5b2…`) that I had **not measured** —
I had rendered that frame but never hashed it, and wrote a plausible-looking
value. It is corrected above to the measured `2f36c1e3…`. A fabricated hash is
worse than a missing one precisely because it looks like evidence, and this
document's whole value is that its numbers were measured. Flagging it so the
next reader knows the class of error was caught here rather than assuming it
cannot occur.

**Prior-lane evidence non-regressed — with the SHAs those lanes recorded.**

| control | X4d/X5 recorded | X6 measured | |
|---|---|---|---|
| venue, `RB3_NO_DEEP_TREE=1` (legacy walk) | `59c1997f41cb58ed` | **`59c1997f41cb58ed`** | byte-identical to X4d's headline frame **and** X5's control |
| venue, `RB3_NO_CROWD_DRAW=1` (X6's own opt-out) | — | `d7963b8c1e6d5711` ×3 | the pre-X6 frame; the single-variable A/B for this lane |

Two nested opt-outs, and each reproduces the frame of the lane that introduced
it: `RB3_NO_DEEP_TREE=1` still yields X4d's exact venue frame even with X5's
deep walk and X6's crowd draw both in the binary. There is no diff to
root-cause. E1 (`5282bd27…`) also re-verified after the gate's full rebuild.

`video_05`'s `rc=1` was checked against the crowd opt-out and fails identically
(same 2 gate failures, 0.00% coverage) with the crowd draw off — it is X4d's
carried render defect, not a regression introduced here.

---

## 8. Retracted hypotheses, with evidence

1. ⛔ **X5 §8 / §10.4: "Character PLACEMENT — two independent mechanisms:
   `WorldCrowd` scatter onto `mPlacementMesh` (6 objects load, none runs)…"**
   **Retracted as a mechanism claim.** There is no scatter and nothing needs to
   run. `OnRebuild` is empty in rb3-xenon, the rb3-Wii oracle and DC3 alike;
   the positions are baked and were already deserialized into
   `mMMesh->Instances()` in X4d's own `rc=0` runs. Every fact in X5's handoff
   was right — 6 `WorldCrowd`s do load and none does run — and the conclusion
   drawn from them was wrong, because nobody had asked what running one would
   *do*. ★ **This is the third consecutive lane in which an inherited cost
   estimate built from correct facts was off by a whole subsystem** (X4d→X5 on
   `BandCharacter`, X5→X6 here). §2.
2. ⛔ **My own first hypothesis: "the six `WorldCrowd`s in `small_club_01` are
   two families of three (`WorldCrowd[_frontrow]` / `_2_ps3` / `_4_ps3`, 8/2/4
   archetypes) holding the SAME baked positions at different archetype variety,
   so drawing more than one per family puts two characters on every seat."**
   The naming is strongly suggestive and I wrote `--crowd-all` as a
   diagnostic-only flag on the strength of it, with a code comment asserting it.
   **Refuted by the overlap matrix I built to confirm it:** all 15 pairs share
   **zero** positions, and the six hold 300 instances at 300 distinct positions.
   They **partition** the audience area; they do not duplicate it. The comment
   and the flag's status were corrected. ⚠ Note the shape — I had already
   measured "300 instances / 300 DISTINCT positions" one step earlier, which
   *already* entailed zero duplication, and I did not read my own number that
   way until I built a second instrument to ask the question directly.
3. ⛔ **My own working hypothesis: "wiring `WorldCrowd::Draw()` into the draw
   loop will make the crowd appear."**
   **Retracted by a byte-identical PNG.** It was necessary and not sufficient;
   the native impostor RTT emits nothing and additive-black is the identity.
   Caught only because I ran the A/B control instead of accepting `rc=0` plus a
   plausible-looking frame. §3.
4. ⛔ **X4d §5 / X5 §5's "9 character(s) at 1 DISTINCT world position — ALL
   STACKED (nothing placed them)".**
   **Not retracted as a measurement — it is exactly right — but retracted as a
   diagnosis.** Those nine are crowd **archetypes** (templates) plus
   `lighttarget`; a template sitting at its default transform is correct
   behaviour, and the defect was that the driver *drew* them. The real crowd
   members were never in that census at all, because they are not `Character`
   objects — they are transforms in a `RndMultiMesh` instance list. ★ **A count
   of `Character`s can never see the crowd, in any venue, however the engine
   behaves** — which is the same "what else could produce this number" trap X5
   named, arriving through a new door.
5. ⚠ **Explicitly NOT claimed:** that the crowd looks the way retail's crowd
   looks. It does not — retail draws billboards and this draws full skinned
   geometry (§4), and the visibility policy is unresolved (§6.1). The claim is
   narrower and testable: **every crowd member stands where the asset says it
   stands.**

---

## 9. Evidence

Copied **outside** the worktree; the worktree is being left in place.

| path | what |
|---|---|
| `/home/free/tmp/laneX6/evidence/x6-E1-small_club_01-crowd-PLACED.png` | ★ **the milestone frame** — 30 crowd members on the club floor, `5282bd27…` ×2 |
| `/home/free/tmp/laneX6/evidence/x6-E3-arena_01-4700-crowd.png` | ★★ **4700 crowd members filling the arena floor**, 28200 placed draws |
| `/home/free/tmp/laneX6/evidence/x6-E2-small_club_01-all-300.png` | all 300 shipped seats in the small club |
| `/home/free/tmp/laneX6/evidence/x6-CONTROL-crowd-draw-off.png` | ★ the A/B control, `d7963b8c…` — the pre-X6 frame |
| `/home/free/tmp/laneX6/evidence/x6-crowd-census-300-positions.log` | the absolute crowd census (positions, bboxes, archetypes) |
| `/home/free/tmp/laneX6/evidence/x6-position-overlap-matrix.log` | the overlap matrix that refuted §8.2 |
| `/home/free/tmp/laneX6/evidence/x6-sweep-*.log` | the six-venue sweep behind §2.3 |
| `/home/free/tmp/laneX6/evidence/x6-native-gate.log` | the fresh native gate |

Worktree: `/home/free/tmp/laneX6/wt` (branch `x6-placement`).

### Reproduce

```bash
cd /home/free/tmp/laneX6/wt/native
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
      -DMILO_ENGINE_PATH=/home/free/code/milohax/milo-native-engine \
      -DDawn_DIR=/home/free/code/milohax/dc3-decomp-deps/dawn/lib/cmake/Dawn
cmake --build build --target rb3-render

# ★ the milestone frame: the crowd standing on the club floor
./build/rb3-render /home/free/code/milohax/rb3/orig-assets/xbox-zip OUT --frames 1 \
    world/venue/small_club/small_club_01/gen/small_club_01.milo_xbox

# ★★ 4700 crowd members in the arena
./build/rb3-render /home/free/code/milohax/rb3/orig-assets/xbox-zip OUT --frames 1 \
    --crowd-all world/venue/arena/arena_01/gen/arena_01.milo_xbox

# the absolute oracle: baked positions, bboxes, and the overlap matrix
./build/rb3-render /home/free/code/milohax/rb3/orig-assets/xbox-zip OUT --frames 1 \
    --dump-tree world/venue/small_club/small_club_01/gen/small_club_01.milo_xbox

# the A/B control: the pre-X6 frame, byte-for-byte
RB3_NO_CROWD_DRAW=1 ./build/rb3-render /home/free/code/milohax/rb3/orig-assets/xbox-zip \
    OUT --frames 1 world/venue/small_club/small_club_01/gen/small_club_01.milo_xbox
```

⚠ Write flags out in full — zsh does not word-split unquoted expansions, and the
failure renders the wrong scene and returns `rc=0`. This caught X4b, X4c and X5.
It did not catch me, because I wrote every sweep with the flags spelled out and
checked that each log named the cell I asked for.

---

## 10. Owed work / handoff

| item | why | owner |
|---|---|---|
| ⚠ **Decide the crowd visibility policy** — the top item | `mShowing` selects 0 crowds in 4 of 6 venues, so the faithful default renders no arena crowd. Find the runtime selector (the `_2_ps3`/`_4_ps3` names imply platform/quality) and let the asset decide, instead of the flag I left unmade. §6.1 | X7 |
| ⛔ **The impostor billboard path emits nothing on this backend** | Nested RTT inside an open draw pass; additive-black composite makes the failure byte-identical. Fixing it replaces 28200 geometry draws with 4700 quads in `arena_01` — this is the crowd's whole performance story. §3 | X7 / native render |
| ⛔ **`BandConfiguration` has no `splits.txt` entry** | The TU is landed and `BandInit` is non-regressed, but objdiff cannot score it until the retail address ranges are mapped. This is where the deferred dual yield gets collected. §5.2 | match lanes |
| ⬜ **Band-slot placement at runtime** | `SyncPlayMode` needs a live `TheBandWardrobe` + `BandCharacter`; both behind the `ScatterIncludes` dedupe lane. The path is now in place, so this is purely a build-system unblock. | build-system |
| ⚠ **Six meshes drawn per crowd member** | X5's carried finding, now at scale: 30 × 6 = 180 draws where the game shows body + **one** gesture prop, and 4700 × 6 = 28200 in the arena. Cosmetic at 30, a real cost at 4700. | X7 |
| ⚠ **`arena_01` / `big_club_01` already have partly-placed `Character`s** | 16 characters at **5** distinct positions and 15 at **8** — unlike the small clubs, these venues place some real characters. Unexplained; worth a look, and a reminder that "all stacked" was venue-specific. | X7 |
| ⚠ **`video_05` renders an empty frame** | Carried from X4d, re-confirmed not to be an X6 regression (fails identically with the crowd draw off). | X7 |
| **`player1..3` stand-ins; `BandCharacter` / `BandCamShot` real TUs; `ScatterIncludes` dedupe; `ThreadCallInit`; `MILO_FAIL` on bad string length** | All carried from X5/X4d untouched. | as before |

---

## 11. Recommended X7 shape

1. ★ **Ask what the missing code would *do* before writing it.** Three lanes in
   a row have inherited a cost estimate assembled from entirely correct facts
   and been wrong by a subsystem. "6 `WorldCrowd`s load, none runs" was true;
   "so write the scatter" did not follow, because the scatter does not exist in
   the shipping game. One `grep` for `OnRebuild` in three repos closed a lane's
   worth of planned work.
2. ★ **A byte-identical result is a measurement, not a null result** — if you
   have varied the input. The impostor path was caught because the 300-instance
   run matched the 0-instance control; had I only run the default I would have
   concluded the 30 members were simply too small to see.
3. ★ **The data you are looking for may already be in memory, counted by an
   instrument that cannot represent it.** X5 found the crowd was invisible to an
   iterator; X6 found its *positions* were invisible to a census of
   `Character`s, because crowd members are not objects at all — they are
   transforms in an instance list. Before concluding a subsystem is absent, ask
   what type the thing you are looking for actually *is*.
4. **Placement is no longer the gap.** Geometry, materials, textures, lighting,
   skinning, animation, reference resolution and now placement are alive in one
   scene, at up to 4700 members. What separates this frame from a Rock Band
   frame is the band — four members on a stage — and that is now one
   build-system unblock away, not a port.
