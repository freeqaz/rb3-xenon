# Branch audit — slice 3 (the 47 oldest branches, 2026-05/2026-06) — laneBT-4, 2026-07-30

**Headline: 0 landings from code. All 47 branches are RETIRED — none holds
matching-relevant content that main lacks.** One artifact was rescued: a research
doc (`docs/decomp/research/2026-06-23-mainhubpanel-span-refuted.md`), zero build
impact.

Baseline measured in `~/tmp/laneBT4/wt` (own build, `report.cache` removed, full
`./tools/ninja-locked`): **matched 40,925 / masked_equal 1,517 / honest 39,408 /
matched_code 34.4795%**. Every number below is against that baseline.

This complements `branch-audit-2026-07-29.md` (laneBC), which reached the same
*global* verdict but did not examine these 47 individually (0 mentions each).
Two independent audits, same conclusion.

---

## Method — and the trap that invalidates the obvious test

`git diff main..<branch>` is **worthless** here: it reports every file main changed
*since* the branch point as a "difference". The valid test is the branch's **own
patch** (`merge-base..branch`), then per file whether the branch blob still differs
from main's — and then whether main superseded it.

One self-inflicted trap worth recording: `git rev-parse main:<missing>` prints the
**unparsed arg on stdout** with rc=128, so a naive `[ "$a" != "$b" ]` comparison
reports files absent from *both* sides as "differing". That silently mis-classified
the branch-created-file cases — exactly the ones most likely to be live work. Guard
with `git cat-file -e` before comparing.

## Results

| Population | Count | Finding |
|---|---|---|
| Files touched by the 47 branches | 122 | **120 MAIN_MOVED**, 2 absent-from-main |
| Files main never touched since branch point | **0** | no branch holds untouched work |
| Files now **byte-identical** to main | 14 | direct proof of landing-by-patch |
| `.text` spans added by the 5 address-branches | 66 | **0 survive** in main's splits.txt |
| Map address keys added | 13,041 | only 32 exist in main; **31/31 checked would REPOINT to a different symbol** |

Main's file is **larger** in ~3/4 of cases, often by hundreds of lines
(`obj/Object.h` 2037 vs 1691–1755; `obj/ObjPtr_p.h` 1206 vs 954;
`ui/UIComponent.cpp` 391 vs 164).

I tested the counter-hypothesis that a *smaller* header could be the correct retail
layout (the documented "DC3 added a member retail lacks" pattern). **Refuted** for
this slice: `cas-MetaPanel:ScoreDisplay.h` is self-labelled *"Minimal stub … Full
class body deferred"* and would delete the real class and its member layout;
`w16-bandprofile:FixedSizeSaveable.h` reverts to a DC3 stub include main's comment
says *"collide[s] with the real ones"*. The branches are earlier scaffolding, not
leaner layouts.

## Era hazard — confirmed by measurement, not by citation

Main flipped to the TU5 address space on 2026-07-15; all 47 branches predate it.
Measured: **66 `.text` spans added, 0 still present in main's splits.txt.** The map
rows are worse than stale — of `bw-RockCentral.o`'s 31 overlapping keys, **0 agree
with main and 31 point somewhere else** (e.g. `0x82297708`: branch
`_Copy_Construct<DistEntry>` vs main `VenueLoader::FinishLoading`). Applying that
map would corrupt 31 correct rows. `bw-RockCentral.o`'s 12,992-key diff is a
whole-file 2-space-indent rewrite — a stale snapshot, not authored work.

## Two branches do not even COMPILE against main

- **`sweep-3`** — *measured*: `./tools/ninja-locked` → rc=2, `Geo.cpp(313) error
  C2664: 'MultiplyTranspose' : cannot convert parameter 1 from 'Vector3' to
  'const Transform &'`. Main's `591ea74d` fixed the param order to
  `(Transform, Vector3)` for +3. (No match delta is quotable from that run — the
  build failed, so its `report.json` was stale.)
- **`ngstats-strip`** — references `gNgStats[0].mSpotlights`, which main *deleted*
  (`1795eef9`, +3 strict, won a different way).

Separately, the four branches carrying `virtual bool Replace(ObjRef*, Hmx::Object*)`
cannot be applied either: main's base pure-virtual (`obj/Object.h:42`) is `void`
across 43 files, so a `bool` override is a hard return-type conflict.

## Measured no-op

`rndmi-fix`'s one-line `OBJ_SET_TYPE_ENGINE(Poll)` → `OBJ_SET_TYPE(Poll)` — the
cleanest isolated matching-relevant change in the whole slice — measured
**exactly 0**: 40,925 / 1,517 / 39,408 / 34.479504%, bit-identical to baseline.
Reason: main's `obj/Object.h:961` now defines
`#define OBJ_SET_TYPE(classname) OBJ_SET_TYPE_ENGINE(classname)`. Main did not
merely supersede that hypothesis — it made it **moot**.

## Landed-by-patch is the dominant outcome

Main's history contains these branches' work under their own commit subjects:

| Branch | Landed as |
|---|---|
| `objectdir-plus4` | `ee014aa1 obj: ObjectDir +4 fix — mInlineProxyType enum -> bool mInlineProxy + restore unk8c (X360)` |
| `objdirptr-0xc` | `dc2e50b7 objptr P4: ObjDirPtr 0x10->0xc — own poly layout, drop injected mOwner` |
| `sweep-rnd` | `f09aab32 rnd: fix Rnd MI layout cascade (+83 matched)` |
| `sweep-obj` | `13709098` (same subject) |
| `sweep-str` | `187d4228` (same subject) |
| `sweep-smoke` | `69862fec char: CharClip struct-offset fixes — +7 matched` |
| `sweep-5` | `74483c06` (same subject) |
| `lightpreset-port` | `9721f6c2 LightPreset: port retail Keyframe sub-struct layout (+28 matched)` |
| `lighthue-fix` | `95176902 utl: BinStream drop DC3-only ReadAsync virtual (+1)` |
| `streamfam-fix` | `fa9450de utl: BinStream drop DC3-only mRevStack member — 0x10->0xc retail (+11)` |
| `bw-TourSavable.o` | `src/band3/tour/TourSavable.cpp` byte-identical to main |
| `masteraudio-fix` | `383a80b7`; both authored files byte-identical to main |
| `texfam-fix` | `a54075be` + `f4ea8721`; `Bitmap.h` byte-identical |

The 14 game-TU branches (`cas-*`, `cA2-*`, `bw-*`, `pilot-ssn`, `hf2-begin1`,
`classA-*`, `w16-*`) are **80–99.3% identical** to main's current file. Their ports
landed and main refined them afterward. The residual difference is (a) main's later
refinements the branch lacks, (b) `#ifdef HX_NATIVE` native-port debug scaffolding
main does not want (`classA-TrackPanelDirBase`: 29 HX_NATIVE/K9 lines vs main's 0),
and (c) include-path restyling (`cas-MetaPanel`'s 131-line "diff" is mostly
`"AppLabel.h"` → `"meta_band/AppLabel.h"`).

## Two traps recorded for future auditors

1. **`RndTexBlendController::GetBlendState` is a landmine.** Main has
   `blend = t2 * 2.0f + t3 * 3.0f`, which is *not* smoothstep; DC3 and
   `texblend-fix` both have the textbook `t3 * (-2.0f) + t2 * 3.0f`. Main is
   deliberately correct for matching (`6da24108`, 93.8 → 94.4), corroborated
   against target asm `build/45410914/asm/TexBlendController.s:883-889`
   (`fmuls f11,f12,f12` → `fmuls f0,f11,f0` → `fmadds f0,f12,f13,f0`: retail scales
   **t² first**). "Fixing" main back to smoothstep on mathematical grounds would
   silently regress this unit.
2. **`hdcache-fix` is a reversed landing.** Its out-of-line `MakeString(const char*)`
   landed as `543850e3` and was then deliberately reversed by `58904829`: retail
   inlines the single-arg overload; the 0x870 `FormatString` frame proves
   `mFmtBuf` is 0x800, contradicting the branch's `MAX_BUF_SIZE 0x1000`.

## Disposition — all 47 retired

- **Game TU / port (14):** `cas-MetaPanel` `cas-NextSongPanel` `cas-MetaPerformer`
  `cas-CharacterCreatorPanel` `bw-TrackerUtils.o` `bw-TrackerManager.o`
  `bw-TourSavable.o` `bw-TambourineManager.o` `cA2-MainHubPanel` `cA2-ClosetMgr`
  `pilot-ssn` `classA-TrackPanelDirBase` `w16-bandprofile` `hf2-begin1`
  → landed-by-patch + HX_NATIVE debug + include restyling.
- **Address-only, TU0-dead (6):** `bw-RockCentral.o` `cas-Synth-range2`
  `cas-BandCharacter-range2` `xfer-recover` `transfer-sweep` `dc3-naming-pilot`.
- **`obj/Dir.h` trio (3):** `vtable-walls` `objectdir-plus4` `objdirptr-0xc`
  — mutually-exclusive theories; two landed verbatim, all three revert `void Replace`.
- **Surgical refuted (5):** `netcache-fix` `rndmi-fix` `charfaceservo-fix`
  `memcard-msgsource` `rnddir-deficit`.
- **Engine sweeps (10):** `sweep-rnd` `sweep-obj` `sweep-str` `sweep-smoke`
  `sweep-3` `sweep-5` `ngstats-strip` `lightpreset-port` `lighthue-fix`
  `streamfam-fix`.
- **Fix family (9):** `ui-session` `dxrnd-fix` `memheap-fix` `hdcache-fix`
  `texblend-fix` `storeoffer-fix` `masteraudio-fix` `texfam-fix` `cachexbox-fix`.

## What this audit did NOT do

- No whole-binary A/B for most branches — the majority were adjudicated by source
  archaeology (blob identity, commit-subject provenance, compile-compatibility), not
  by building. Two builds were spent: `rndmi-fix` (measured exact 0) and `sweep-3`
  (build failed → refuted, no delta quotable). That is a deliberate budget choice:
  once a branch is proven landed-by-patch or non-compiling, an A/B adds nothing.
- Branch **deletion** was not performed — this audit only adjudicates.
