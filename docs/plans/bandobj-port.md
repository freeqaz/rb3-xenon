# Plan: port `system/bandobj/` (rb3-Wii → 360) so Object-subclass TUs compile + match

> ## ⚠ SUPERSEDED IN PART — dated correction, 2026-08-02
>
> **This document was written 2026-05-26 (`1c530de2`) and its opening status block is
> now false.** It is kept intact below because its dependency analysis, its Wii→360
> porting-concern catalogue (§3) and its dc3-Rosetta sourcing decision (§4) are still
> the best writeups of those topics in the tree. Read the corrections first.
>
> **1. `src/system/bandobj/` is NOT absent — it landed, and the plan executed.**
> Verified on `main` @ `4c19740e`: **52 `.cpp` + 60 `.h`** in `src/system/bandobj/`,
> and **51 bandobj TUs pinned** in `config/45410914/objects.json` (not 7). The staged
> plan in §5 ran to completion and beyond:
> `297f5114` (2026-05-26, Stage 1 StreakMeter+BandLabel) → `87b96e49` (Stage 2
> CrowdAudio+BinkClip) → `2e542d81` (Stage 3 BandDirector+BandSongPref) →
> `e33d851c`/`1aae14cc`/`1fbbdfa2`/`4ce7dd0f`/`2ad663d4` (2026-06-06/07,
> VocalTrackDir +29, TrackPanelDir +21, **BandCharacter +86**, BandCharDesc +17,
> BandWardrobe +5, all @100%) → `704dbb81` (2026-06-10, BandIKEffector +27).
> **Stage 4 — "defer BandCharacter to a later wave" — was NOT deferred**; it landed
> 2026-06-06 and was the largest single win in the campaign.
>
> **2. The `Hmx::Object` 0x2c→0x28 blocker is resolved.** `docs/plans/hmx-object-layout.md`
> is history, not a gate; the retail 0x28 layout is in `src/system/obj/Object.h`
> (see the comments at `:85`, `:558`, `:1791`).
>
> **3. There are no "Missing configuration" bandobj warnings left.** Every class this
> plan targeted is a **scored unit** in `build/45410914/report.json`. Current spread:
> `BandWardrobe` 91.92% · `BandCharacter` 85.99% · `BandCamShot` 77.02% ·
> `BandLabel` 67.84%.
>
> **4. §6's match expectation was low.** It predicted "≈20-30 new matches" for
> Stages 1-3 with BandCharacter deferred. The five pinned-TU ports alone report
> **+158 @100%** in their commit messages (29+21+86+17+5); Stages 1-3 did not
> quantify, and `704dbb81`'s **+27** is a combined BandIKEffector + MidiInstrument
> lever, so it is not solely bandobj. Even read conservatively the estimate was low
> by ~5x, and the deferral advice in §5 Stage 4 was the costliest part of it.
>
> **What is live now is a different question entirely** — not "does bandobj compile
> and match under MSVC-X360" (it does), but "does it compile and link under
> **clang/LP64** for the native port". That is
> [`band3-native-unblock-priority-2026-08-02.md`](band3-native-unblock-priority-2026-08-02.md)
> and [`x4b-animation-2026-08-02.md`](x4b-animation-2026-08-02.md) §4. Short version:
> 12 of 13 relevant TUs are clang-clean; the blockers are a
> `cmake/ScatterIncludes.cmake` dedupe gap (807 duplicate-definition link errors) and
> ~4 stale rb3-Wii-lineage defects inside `bandobj/BandCharacter.cpp`'s own
> `HX_NATIVE` arms.

---

**Status oracle (as written 2026-05-26 — see the correction block above):** rb3-xenon = **110 matched fns**.
`src/system/bandobj/` is **absent** from our tree (`find src -path '*bandobj*'` →
empty). The whole subsystem's `.cpp`+`.h` live only in `../rb3/src/system/bandobj/`
(125 files, Wii/MWCC). **7 bandobj TUs are already wired+pinned** in
`config/45410914/objects.json` (`NonMatching`) and `splits.txt` — BandCharacter,
BandCharDesc, BandDirector, BandWardrobe, CrowdAudio, TrackPanelDir, VocalTrackDir
— which is exactly why they emit "Missing configuration" warnings: the pin exists,
the source does not. **Blocked on** the `Hmx::Object` 0x2c→0x28 fix
(`docs/plans/hmx-object-layout.md`); every bandobj class is an Object subclass.

## 1. Dependency classification (3 targets: StreakMeter, CrowdAudio, BandDirector)

Per-class transitive `#include` walk, classified **(a)** present in our `src/`,
**(b)** reuse 360-ported from `../dc3-decomp`, **(c)** bandobj-RB3-specific (port
Wii→360 from `../rb3`), **(d)** heavy (stub-vs-port).

| Target TU | .text span | direct `#include`s | (a) in our src | (c) RB3-only-port | (d) heavy |
|---|---|---|---|---|---|
| **StreakMeter** (not yet pinned) | n/a — must pin | rndobj/{Dir,EventTrigger,PropAnim,TransAnim}, math/{Rot,Trig}, rndobj/MatAnim, **bandobj/BandLabel** | all rndobj/math present | **BandLabel.h+.cpp** (its only bandobj dep; bases `UILabel`+`UITransitionHandler` both present in `src/system/ui/`) | none |
| **CrowdAudio** (pinned 0x822FEDF8) | 0x2520 | obj/{Object,Task,Msg}, rndobj/Poll, synth/{Faders,Sequence,**BinkClip**}, utl/{TimeConversion,Symbols,**Messages**} | obj/*, rndobj/Poll, synth/{Faders,Sequence}, utl/{TimeConversion,Symbols} all present | **synth/BinkClip.{h,cpp}** (72/271 LOC; deps Pollable/StandardStream/Faders/FilePath all present), **utl/Messages.h** (+Messages2/3/4 chain) | none |
| **BandDirector** (pinned 0x8227C378) | 0x8EA8 | rndobj/{Poll,Draw,Dir,PropAnim,PostProc,Group}, char/FileMerger, world/Dir, utl/{Loader,Symbols,Messages}, obj/{Data,Task}, decomp.h, math/Utl, **bandobj/{BandCamShot,BandSongPref,CrowdAudio,BandWardrobe}** | every engine dep present (verified each); `decomp.h` at `src/decomp.h`, resolves via `/I src` | **BandCamShot.{h,cpp}**, **BandSongPref.{h,cpp}**, **CrowdAudio** (above), **BandWardrobe.{h,cpp}** (cross-include), utl/Messages | BandCamShot drags `world/CameraShot.h`, `rndobj/Env.h` (present) |

**BandCharacter** (the 18-ID jackpot, pinned 0x8226C738, span 0x9770) is the
outlier: **4 base classes** (`Character, BandCharDesc, MergeFilter,
Rnd::CompressTextureCallback`) + ~30 `char/*` headers + bandobj
`{BandHeadShaper, BandWardrobe, BandCharDesc, BandPatchMesh, OutfitConfig,
CharKeyHandMidi}`. Two of its **engine** deps are themselves Wii-only —
`rndobj/Highlightable.h` and `char/CharMeshCacheMgr.h` exist **only in `../rb3`**,
not in our tree or dc3 — so BandCharacter requires porting engine headers too, not
just bandobj. **Defer BandCharacter to a later wave.**

**Quantified minimal compile set:**
- **StreakMeter:** 1 bandobj header + 1 bandobj cpp to port (BandLabel) + StreakMeter
  itself = **2 cpp / 3 headers**. Cheapest. *Not yet pinned* (needs a fresh
  `splits.txt` `.text` range + objects.json entry).
- **CrowdAudio:** CrowdAudio + BinkClip(.cpp) + Messages(.h-only) =
  **2 cpp / ~5 headers**. Already pinned.
- **BandDirector:** drags BandCamShot, BandSongPref, BandWardrobe, CrowdAudio +
  Messages → **~6 cpp / ~12 headers** for a clean compile. Already pinned, highest
  value of the cheap tier (13 IDs).

## 2. Foundation-first order (shared bandobj headers must land first)

There is **no single "BandObj base class"** — bandobj classes inherit engine
bases directly (RndDir, RndPollable, Character, UILabel). The shared *bandobj*
prerequisites, in dependency order:

1. **BandLabel.{h,cpp}** — needed by StreakMeter and most HUD displays.
2. **BandSongPref.{h,cpp}**, **BandCamShot.{h,cpp}** — needed by BandDirector.h.
3. **BandCharDesc / BandPatchMesh / OutfitConfig / BandHeadShaper** — the
   BandCharacter/BandWardrobe prefab cluster (port together; OutfitConfig has the
   only `revolution/` include — see §3).
4. **synth/BinkClip.{h,cpp}** + **utl/Messages.h** (engine-adjacent, RB3-only) —
   shared by CrowdAudio and others; port once, early.

## 3. Wii→360 porting concerns (sampled, concrete)

Sampled StreakMeter.cpp, CrowdAudio.cpp, BandDirector.cpp head + a tree-wide scan:

- **MWCC `#pragma` blocks — the dominant issue.** 16 of the bandobj cpps use
  `#pragma push / dont_inline on / pool_data off / force_active on / fp_contract
  off / pop`. BandDirector.cpp alone has **6 such blocks** (lines 111, 343, 363,
  514, 747, 1619); StreakMeter has 1 (line 21). MSVC X360 does not understand
  these. They must be removed or translated (MSVC equivalents: `#pragma
  optimize("", off)` for dont_inline-ish; usually just delete and let codegen +
  objdiff sort inlining). **This is the single most common edit per TU.**
- **`asm volatile { … }` inline asm — 2 files only:** `InlineHelp.cpp` and
  `BandIKEffector.cpp` (neither a current target). MWCC PPC asm syntax ≠ MSVC; these
  TUs need real porting work and should stay out of early waves.
- **`revolution/` Wii includes — 1 file only:** `OutfitConfig.cpp` line 15
  `#include <revolution/gx/GXMisc.h>`. Must be replaced with the 360 equivalent or
  guarded out. Relevant only when BandCharacter/OutfitConfig is tackled.
- **No platform `#ifdef` gates** (no `PLATFORM_*`/`WII`/`PS3` conditionals found in
  any bandobj source) and **no Wii-specific types** in the three cheap targets.
- **Init/ctor style is MSVC-compatible** as-is (member init-lists, `Hmx::Object::New<T>()`,
  `Find<T>`, `MakeString`, `INIT_REVS`) — these already compile in our tree's other
  Object subclasses.

**Net:** the 3 cheap targets (StreakMeter, CrowdAudio, BandDirector) are
*low-friction* ports — strip MWCC pragmas, fix include paths, port the 1-2 bandobj
header/cpp deps each. No asm, no `revolution/`, no platform gates. The friction
(asm, GX, 4-base-class layout) is concentrated in files we are **not** pinning yet.

## 4. dc3 cross-check — prefer dc3 for shared engine pieces

dc3 has a **structurally parallel `hamobj/`** (Dance Central's equivalent
subsystem), and **our tree already carries a full `hamobj/` copy** (present but
**0 entries in objects.json** — unwired, reference-only). Direct analogues:
`HamDirector : RndPollable, RndDrawable` (**identical bases to BandDirector**, same
rndobj Poll/Draw/Dir/PostProc/PropAnim + char/FileMerger + world/Dir include set);
HamCharacter, HamCamShot, HamLabel (also `UILabel, UITransitionHandler` — same bases
as BandLabel), HamList, HamIKEffector.

**Sourcing decision (per CLAUDE.md provenance):**
- The **engine bases** bandobj sits on — RndDir, RndPollable, RndDrawable,
  Character, UILabel, UITransitionHandler, world/Dir, FileMerger, PostProc — are
  **already 360-ported in our `src/`** (came from dc3). **Reuse as-is; do not
  re-port from Wii.**
- The **bandobj classes themselves are RB3-game-specific** — there is no
  `BandDirector` in dc3, only `HamDirector`. They are **not interchangeable**
  (different members, different DataArray handlers, different layouts). **Port the
  Band* classes from `../rb3` (Wii), not from dc3.**
- **Use `hamobj/*` as a porting Rosetta stone:** when a Wii bandobj pattern is
  ambiguous under MSVC, the already-360-ported `HamDirector.cpp`/`HamLabel.cpp` in
  our tree show the exact MSVC idiom for the same engine API. Read them, don't copy
  them.
- **`rndobj/Highlightable.h` + `char/CharMeshCacheMgr.h`** (BandCharacter engine
  deps, missing from our tree and dc3): present only in `../rb3`. Port from Wii when
  BandCharacter's wave arrives — flag as a hidden engine-header cost.

## 5. Sequencing + concurrency (build-lane mutex)

**Hard constraint:** one ninja writer; the Object-layout fix holds the lane now and
runs SOLO (rebuilds 372 Object.h TUs). **No bandobj work starts until that lands.**
Worktrees are unavailable; verify-compile serializes on the single lane.

**Stage 0 (blocker):** Object 0x28 fix merges. → Re-diff already-compiling subclasses
(`Console`, `ShaderOptions`) to confirm the fix unblocks member access (wave-1 A1).

**Stage 1 (serial, one agent) — shared foundation + smallest win:**
port **BandLabel** + **StreakMeter**, pin StreakMeter (fresh `.text` range — it's
the only target *not* yet pinned), strip MWCC pragmas, `touch config.yml && ninja`.
2 cpp. Smallest possible slice that yields matches; proves the bandobj pipeline end
to end.

**Stage 2 (serial) — CrowdAudio:** port BinkClip + Messages.h, then CrowdAudio
(already pinned). 2 cpp.

**Stage 3 (serial) — BandDirector:** port BandCamShot, BandSongPref, BandWardrobe
(BandDirector.h pulls all three) + reuse CrowdAudio from Stage 2. Strip 6 pragma
blocks. ~6 cpp. Highest cheap-tier value.

**Stage 4 (later wave) — BandCharacter cluster:** BandCharDesc, BandPatchMesh,
OutfitConfig (`revolution/` fix), BandHeadShaper, CharKeyHandMidi, + the two
Wii-only engine headers (Highlightable, CharMeshCacheMgr). 4-base-class layout
reconstruction. Multi-day; do not bundle with Stages 1-3.

**Concurrency:** **none on the build lane** — each stage's verify-compile needs an
exclusive `ninja`. The *header-porting text edits* of Stages 1-3 are independent and
could be drafted in parallel by separate agents, but the compile-verify that proves
each must run one-at-a-time. Practically: **one Opus agent runs Stages 0→3 serially.**

## 6. Match expectation (post-Object-fix)

From wave-1's in-range bindiff counts (`docs/plans/porting-wave-1.md`): BandDirector
**13**, BandCharacter **18**, BandWardrobe **8**. CrowdAudio and StreakMeter weren't
separately quantified there, and these specific class names do **not** appear as
clusters in `proposed_splits_bindiff.txt` (their counts derive from the pinned
`.text` spans + dc3 name map, not that file) — so treat CrowdAudio/StreakMeter as
**unquantified-but-positive** (their pinned spans are 0x2520 / TBD; expect a handful
each at the leaf-fn hit rate).

**Realistic cheap-tier delta (Stages 1-3):** **StreakMeter (handful) + CrowdAudio
(handful) + BandDirector (~13) ≈ 20-30 new matches**, contingent on the Object fix
actually unblocking member-access codegen (confirm via wave-1 A1 first). **Honest
scope:** the *full* bandobj subsystem is a **20+ file, multi-day** effort — but the
**smallest first slice that yields matches is Stage 1 (BandLabel + StreakMeter, 2
cpp)**, and Stages 1-3 together (~10 cpp) capture most of the cheap match value
without touching the asm/GX/4-base-class hard cases. **Defer BandCharacter** despite
its 18 IDs — its hidden engine-header ports and quad-inheritance layout make it the
worst effort-per-match in the cluster until the cheap tier proves the pipeline.

## References (verified this session)
- Sources to port: `../rb3/src/system/bandobj/{StreakMeter,CrowdAudio,BandDirector,
  BandLabel,BandCamShot,BandSongPref,BandWardrobe}.{h,cpp}`
- RB3-only engine deps: `../rb3/src/system/synth/BinkClip.{h,cpp}`,
  `../rb3/src/system/utl/Messages.h`, `../rb3/src/system/rndobj/Highlightable.h`,
  `../rb3/src/system/char/CharMeshCacheMgr.h`
- dc3 Rosetta (already in our tree): `src/system/hamobj/{HamDirector,HamCharacter,
  HamLabel,HamCamShot}.{h,cpp}` (unwired, reference-only)
- Pins (already present): `config/45410914/{objects.json,splits.txt}` — 7 bandobj
  TUs `NonMatching` with `.text` ranges (BandDirector 0x8227C378+0x8EA8,
  BandCharacter 0x8226C738+0x9770, CrowdAudio 0x822FEDF8+0x2520, …)
- Blocker: `docs/plans/hmx-object-layout.md` (must land first)
- Match-count source: `docs/plans/porting-wave-1.md` §"Ranked table" / A2
