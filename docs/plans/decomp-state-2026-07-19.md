# rb3-xenon decomp — state & live veins (2026-07-19)

**Current: 18,621 strict-matched functions** (`build/45410914/report.json`,
`match_percent_normalized == 100.0` exactly). Denominator is the whole TU5 XEX
(~69k functions). HEAD `d0864f7c`.

## Recent arc

| date | strict | delta | driver |
|---|---|---|---|
| 2026-07-17/18 mega-run | 17,445 | +2,081 | identification stack (+1,871 names), lane-B near-pair, naming wave, BandSwatch, struct leads |
| 2026-07-18 review | 17,445 | — | 3 Opus scouts ranked pools; `docs/plans/review-2026-07-18-next-focus.md` |
| 2026-07-19 body-port/recarve/scatter/id-flywheel | **18,621** | **+1,176** | the "mapped-but-0%" pool cracked open (see below) |

The +1,176 came from **one discovery and its flywheel**: the "mapped-but-0%" pool (functions with
real mangled names stuck at 0%) is overwhelmingly **COMDAT-scatter / TU-composition
drift**, NOT missing source. Retail MSVC/X360 (`/O1`, no LTCG) emits each function
into its own COMDAT and the linker scatters them across `.text`; dtk carves the
retail binary into per-source-file target objs by address range, so a function
whose COMDAT landed in unit X's span is attributed to X even though its source
lives in unit Y — and *our* obj for Y is the one that emits the matching bytes,
under a name objdiff never pairs into X.

### The three fix shapes (all landed, all regression-clean)

1. **Owner-TU whole-file include** — append `#include "<owner>.cpp"` to the
   span-owning `.cpp` so its obj emits the scattered COMDATs. INIT_REVS `gRev`
   collisions on double-include → byte-neutral `#define gRev gRev_<Owner>`.
   Landed: bp3 (+26), bp2r (+84), scatter-sweep w1 (+174). Idiom at HEAD in
   `TDStretch.cpp`, `MeshAnim.cpp`, `Console.cpp`.
2. **Retail-arity body duplication under `#ifndef HX_NATIVE`** — when whole-file
   include collides (statics/anon-ns/PROPSYNC barewords), copy just the needed
   bodies into the span owner with extern decls; native keeps canonical defs.
   Landed: bp1 (+36). Idiom in `Debug.cpp`, `DirLoader.cpp`, `MemHeap.cpp`.
3. **Splits gap-fill recarve** — when the auto blob is the missing *middle* of an
   already-pinned TU, add one gap `.text` range + reloc-masked byte-identity map
   entries (`tu5_reloc_masked_correlate.py`); ICF-twin MULTI groups resolve by
   order-preserving assignment; funclets cascade free. Landed: rc1 (+130,
   AccomplishmentPanel), rc3 (+14, TrackWatcherImpl).

**Instrument:** `scripts/harvest/comdat_scatter_scan.py` (~0.9s, re-runnable)
scans the COFF symbol tables of all ~836 compiled objs and splits every named-0%
function into **SCATTER** (emitted by another wired obj → owner-include/dup
fixable) vs **UNWIRED** (no wired obj emits it → gameport pool).

**Kill test before recarving any auto blob:** if the span's pre-mapped names are
emitted by *no* wired obj, the blob is a COMDAT catch-all from unwired classes —
a gameport target, not an attribution gap. (rc2/SongSort `0x826DD570` was
correctly skipped this way: SkillsAwardList / CampaignEra* / a NavListSortMgr
SongSortMgr redesign that matches DC3, not our older port.)

## Captain's plan (2026-07-19, Fable strategic review) — ACTIVE

**Key reframe (overturns the "scatter drained" verdict below):** the ~218
"net-0" scatter residue is NOT dead — it is a **near-miss discovery engine**.
Applying an owner-include PAIRS the scattered body in objdiff, turning an opaque
0% stub into a *diagnosed* fuzzy near-miss with a known owner source file + DC3/
rb3-Wii oracle. This is exactly how UpdateOverlay / UpdateCache / enableAAFilter
were found and then fixed to strict. Net-0 ≠ rejected; it means "here's a paired
body and its diff." **Frame for every wave: judged by strict flips + names fed to
the identification flywheel** (round-5 gate ~+1,000 names; body-ports buy it).

  **sw2-parent-leak guard (F1 discovery, load-bearing):** several sw3 consumers
  are themselves scatter-*owners* included by sw2-era parents (Morph←HamMove,
  DepthBuffer3D←UIList, Gem←OutfitConfig, …). Those parents bracket the include
  with `#define gRev gRev_<Child>` but do NOT set `SW_SCATTER_OWNER_INCLUDE`, so a
  naive owner-append leaks the new body into the parent TU and breaks it. Fix:
  guard the append to fire only in the consumer's PRIMARY TU. `gRev` is a static
  member *variable* (never a macro) in a primary compile, so `#ifndef gRev` is a
  reliable primary-vs-owner discriminator; where an internal block `#undef gRev`s
  before the tail (UIList's BandDirector block), use a stronger top-of-file
  `<UNIT>_SW3_PRIMARY_TU` sentinel instead.

- **Wave 1 — Expose-and-fix:** RAN 2026-07-19. Harvest → `~/tmp/expose_harvest.md`
  (9 freebies / 71 ≥88% / 118 compile-fail). **Actual yield: +10 total (F1
  freebies only; F2=0, F3=0, F4=0).** BIG EV MISS vs the +80–150 estimate — the
  ≥88 band is systematically blocked (recalibration below).
  **NEW CASCADE-SHAPED VEIN — DC3-oversized struct recon (F2 leads).** F2 proved
  the clean-building 99.9x targets miss on a single **struct-size immediate**: our
  DC3-sourced headers declare several structs LARGER than retail. Shrinking each to
  retail size flips its near-miss AND (cascade) every function that touches that
  struct — a shared-struct fix is wide-ripple by nature. Exact leads (each needs
  its own whole-binary A/B; gate DC3-newer fields behind `#ifndef HX_NATIVE`):
  **SongSection 0x18→0xc, RecurseInfo 0x18→0x10, BandIKEffector::Constraint
  0x1c→0xc, StoreMainPanel member −0x18, CharPollGroup base subobject −0x28.**
  This is the "B_STRUCT_OFFSET is the real vein" call (see A_TOOLING ICF memory),
  now with concrete targets. HIGHER EV than the mispair band.
  **Mechanism rule (F2, durable):** an owner `.cpp` with its OWN nested
  scatter-includes is UNSAFE via the dialect shim — the push forces Object.h
  dialect and breaks the owner's nested ObjMacros-dialect includes, cascading to
  every TU that includes the consumer. Nested-scatter counts: HamCamTransform=9,
  BandCamShot=3, ViewSetting=2, HamNavList/Spotlight/HolmesClient=1; SAFE (0):
  SongLayout, CharEyes, ClipDistMap, CharPollGroup, TransAnim, FlowSetProperty,
  StoreMainPanel, BandIKEffector.
  **⚠ RECALIBRATION — the ≥88%-but-<100% exposed band is a MISPAIR MIRAGE.** The
  target-symbol renamer labels a physically-adjacent, ICF-shaped-but-semantically-
  DIFFERENT function with the exposed name, so "closing" the near-miss matches our
  code to the WRONG target. F4 proved every tiny "one-liner" was a mispair:
  Movie::IsLoading ("fixing" Movie 4→8B broke 10 MoviePanel funcs, net −9; our
  4-byte Movie is CORRECT, DC3's 8-byte doesn't apply to RB3), NetLoader::
  PostDownload (ours already stores 0x10 correctly), PlatformMgr::QueueEnumJob
  (target tail-calls a DIFFERENT function), OnSeedRandomContext (already 100 in its
  home unit). F3 proved the 99.8x `??_G`/STL residue is gapped by a reloc-arg
  (vtable/callee at a different scattered address) report.json won't forgive. **So
  only the exact-100.00%-on-include freebies flip; the sub-100 band is
  mispairs + struct-divergence + pairing artifacts. Do NOT re-hunt it as cheap
  near-misses.** UniqueFilename is the lone real crack — see vein #3.
  Still-untried Wave-1 items (separate from the mirage band): 3 body-dup cases
  (CameraShot←Flow, PropAnim←PropKeys, CharBonesMeshes←GemManager as `#ifndef
  HX_NATIVE` dup), MidiSynth WorldDir::PropSync trio (splits re-attribution —
  Dir.obj already emits), MemTracker::StopLog (map/splits).
- **Wave 2 — Oracle-backed UNWIRED wiring**, per-symbol-owner-driven (NOT naive
  whole-TU carve): resolve each unwired symbol's owner class, port its bodies
  into the canonical TU (wire into objects.json if absent) — then Wave-1
  machinery pairs it. Targets: rnddx9 CubeTex 8 Dx* ctors + Rnd_Xbox(3) +
  rnddx9/Rnd(1), Anim(7), Sequence(8), MemTracker(8), DataPointMgr(5),
  WaveFile(4), Cam(2); game DataArraySongInfo(11), TrainerPanel(5),
  VocalTrack(3), VocalPlayer(3). SKIP oracle-poor half (System/LEAPCORE 32,
  Mic/ExternalMic 25, FFT 10, Compress, GranularSynth/Spectral/PeakDetector,
  rtti/osfinfo). EV +60–120.
- **Wave 3 — TrackWatcherImpl beatmatch gameport:** 121 flat-0% NAMED bodies,
  direct oracle `../rb3/src/system/beatmatch/TrackWatcherImpl.cpp`, splits
  already gap-filled (rc3). NOT banned grind — highest-cascade single target
  (biggest name-feed to round-5; RealGuitarTrackWatcherImpl.obj already owns
  scattered spans → landing beatmatch types unblocks chained proposals). Split
  4–6 agents by method cluster, 4488B monster last, accept partial. EV +80–140.
- **Micro-lane (no wave slot):** the 4 named near-miss probes (PreInit,
  InitParams, FindShader, SetTransform) + DxRnd::UpdateScalerParams / UpdateCache
  99.8 / enableAAFilter 99.5 / RingBuffer::Write 91.4 singles; grouped-globals
  **RECON ONLY** (count 80–97 fns citing shared-anchor `lbl_*` base+offset
  addressing — ≥30 → build a source-level global-aggregation mechanism, <10 →
  drop). → `~/tmp/grouped_globals_recon.md`.
- **Between waves:** re-run `comdat_scatter_scan.py` (chained proposals) + id
  stack stage-1 even below the +1,000 gate (~0.15 flips/name).
- **Pivot decision deferred ~3 waves:** after, the long tail is the ~5,300
  nomatch divergent-body pool — choose (a) scale Wave-1 expose-and-fix into a
  systematic divergence-triage pipeline, (b) grouped-globals mechanism if recon
  supports, or (c) pivot work-kind (native/tooling).

## Live veins (ranked by EV)

### 1. COMDAT-scatter sweep — reframed as EXPOSE-AND-FIX (see Captain's plan)
After 3 sweep waves (+661) the scanner reports **275 SCATTER candidates /
218 proposals** still open. Previously called "nearly drained / body-port-grade";
the captain's reframe (above) makes these the **cheapest diagnosed near-miss
fodder on the board** — apply the include to pair the body, harvest the exposed
%, fix the ≥88% ones. Cross-dialect walls unlocked by the wave-3 byte-neutral
shim `obj/dialect_object_{push,pop}.h`. Method is mechanical + gated (per-unit
whole-binary A/B, auto-revert on loss); **re-run the scanner between waves** —
fixing one owner unblocks chained proposals (w1's MidiSynth←PropSync only
appeared after PropSync←Dir landed).

### 2. UNWIRED gameport pool — 327 fns / 138 units
Functions no wired obj emits. Two sub-classes:
- **Engine, oracle-backed (portable):** rnddx9/CubeTex (8 Dx* ctors, DC3 oracle),
  Anim (7), Lit_NG, rnddx9/Rnd, Sequence — DC3 near-verbatim. These are true
  body-ports / TU wirings, ~medium cost.
- **Oracle-poor (defer, hard):** FFT (10 fns, VMX128 hand-asm — DC3's FFT unit is
  only 23%), System/LEAPCORE (32, no oracle), Mic + ExternalMic (25, Xbox voice),
  Compress/XGRAPHICS (10, shader-microcode), GranularSynth/SpectralAnalysis/
  PeakDetector (DSP hand code), rtti/osfinfo (CRT). Lowest ROI — leave for last.
- **Game (band3):** 16 units incl. TrainerPanel (5), DataArraySongInfo (11) —
  rb3-Wii oracle, gameport cost.

### 3. Exposed near-misses (fuzzy → strict fodder) — partly worked (nm +3, sm +3)
Pairing the scattered bodies revealed genuine near-misses hidden as 0% stubs.
DONE: NgRnd::UpdateOverlay/Terminate + MakeWorldSphere (nm, NgStats mSpotlights
strip + Geo.h fix); RndShaderMgr::Terminate/Invalidate + InitShaderOptions (sm,
ShaderType enum 38→26).
**AT_LIMIT (do NOT re-hunt, 2026-07-19):** RndShaderMgr::FindShader 80.3 and
SetTransform 81.7 — our source is byte-identical to the DC3 oracle; both are
pure callee-save-vs-volatile regalloc divergence (permuter-band, banned).
FindShader additionally has a HARD structural blocker: retail RB3 (2010)'s
`RndShaderMgr` vtable has **one fewer virtual than DC3 (2012)** — NewShaderProgram
sits at slot `0x5c` retail vs our `0x60`. DC3 is not an oracle for the vtable
shape; removing a virtual is a wide-ripple header change (re-lays every
ShaderMgr-subclass vtable) with no ground truth for *which* virtual RB3 lacks.
Prerequisite for any revisit: dump a concrete retail ShaderMgr-subclass vtable to
identify the missing virtual — a standalone structural task, not near-miss polish.
REMAINING leads: UpdateCache 99.8; enableAAFilter 99.5 (RateTransposer +16B
member — pad-probe); RingBuffer::Write 91.4; DxRnd::UpdateScalerParams 0%.
MemTracker::StopLog 77 = MISPAIRING (target is a MemFree/dtor, not StopLog —
map/splits fix, not source).
**UniqueFilename — CRACKED (F4 2026-07-19), needs an independent splits pin to
land.** The 2-line fix in `src/system/os/File.cpp` reaches 100.0% normalized
(Ghidra-verified vs `default_tu5.xex`): (a) declare `int i=0` BEFORE `String ret`;
(b) format string is hardcoded `"%s_%06d.bmp"` (drops the `c2` param — retail
ignores it and emits `.bmp` for both callers: Rnd.cpp:499 wants `.bmp`,
LiveCameraInput.cpp:1185 passes `"data"` but retail still emits `.bmp`). Can't land
now: UniqueFilename's COMDAT lives in Rnd's `.text` span, so the only measurement
path (`Rnd ← os/File.cpp` include) reshuffles objdiff pairing and drops
`GetNormalMapTextures` (rndobj/Utl) 100→94.5% — a pairing artifact, not a real
regression (`matched_functions` stays put, Utl.obj byte-identical). Give
UniqueFilename its own `splits.txt` `.text` range (carve out of Rnd's span, like
rc1/rc4 gap-fills) → then the File.cpp fix is a clean +1. Exact patch in F4's
report / this session's transcript.

### 4. Remaining recarve gap-fills
**0x82560660** UI-message run DONE (rc4 +48, UIStats gap-fill). **0x8234FCEC**
DataArray/ObjectDir SKIPPED by kill test (unwired gesture catch-all —
SkeletonFrame from gesture/Skeleton.cpp; recovery = wire that TU first). The
Accomplishment/TrackWatcher blobs are done; SongSort is UNWIRED (vein #2). The
easy gap-fill recarve targets are now exhausted; new ones require wiring an
unwired owner TU first (converges with vein #2).

### 5. Deep grinds (banked, lower EV)
- **TrackWatcher family — CORRECTED CHARACTERIZATION (2026-07-19).** The "121
  flat-0% NAMED bodies" framing is WRONG per the live report: `TrackWatcherImpl`
  is 159 fns / 45 matched / **78 at-0% but ALL anonymous `fn_` (0 named-0)** + 36
  named partials; `RealGuitarTrackWatcherImpl` 40/16/21-anon; family total ~104
  unnamed-0% + ~40 partials. Our source (872 lines, ≈ oracle 859) is largely
  ported. So the 0% pool is an **IDENTIFICATION gap (unmapped targets), not a
  body-port gap** — Wave-2 approach is **correlator-FIRST**: run
  `scripts/harvest/tu5_reloc_masked_correlate.py` on the TrackWatcher-family objs
  to pair our compiled named methods to the target's unnamed `fn_` by
  reloc-masked byte identity → add map entries → the byte-matching bodies flip
  (+ feed the id flywheel). ONLY the residual (unnamed, bodies diverge) + the ~40
  named partials are the actual body-port grind (oracle
  `../rb3/src/system/beatmatch/TrackWatcherImpl.cpp`, largest 4488B). Do NOT fan
  out a 4–6-agent body-port wave before the correlator run scopes the real
  residual.
- **Grouped-globals wall** — RECON DONE (2026-07-19): verdict **NARROW, no
  mechanism wave**. Of 441 named 80–97 fns, only **17** are genuinely fold-walled
  and just **2 pure-fold** (the known MemFindAddrHeap/SystemMs). MSVC only shares
  a base register when the globals are *defined in the same TU as the accessor* —
  so cross-TU manager singletons (`TheBandDirector`/`TheLoadMgr`, `TheTaskMgr`/
  `TheUI`, `TheSessionMgr`/`TheSynth`, `ThePlatformMgr`/`region`) are UNFOLDABLE
  by any source change. Only **3 intra-TU clusters are source-fixable** (cheap
  micro-fixes, ~+2–3, fold into scatter campaign not a wave): MemHeap
  `gHeaps`+`gNumHeaps` (extern in MemHeap.cpp), Debug/System `gSystemMs`+
  `gSystemFrac` (extern in Debug.cpp, defined System.cpp), Voice
  `gCommitSyncVoices`+`gCommitTag` (in-TU, declaration-adjacency fix). Detail:
  `~/tmp/grouped_globals_recon.md`. Not a new mechanism — a facet of TU-drift.
- **DxRnd::UpdateScalerParams** (0x82739948) — paired at 0% since the vtable fix,
  genuine body-port lead.
- **BandCharacter −4 container compaction** (cr6), **BandCamShot vbase-MI
  reconstruction** (documented wall, pad-probe-killed the tempting +0x80 tail).

### 6. Identification round-4 — DONE +170 (`39038c09`), FLYWHEEL CONFIRMED
The scatter/recarve campaign's +250 names & 3 new pinned clusters cleared the
round-3 fixed-point gate. Re-running `scripts/harvest/TU5_SCANNER_STACK.md`
yielded **+170 strict** at ~0.157 flips/name (5x the collapsed 0.031 rate),
fixed point in 3 rounds, 6/6 Ghidra spot-checks. **Key insight: the scatter
vein FEEDS the identifier** — every owner-TU-include body flip creates a fresh
clean byte-identity pair the scanner then cracks. Round-5 not warranted until
+~1,000 more names. This coupling means future body-port waves should be
followed by an identification re-run.

## Dead / banned (do NOT re-hunt)
Permuter (user directive — low yield, grinds the box); ≥99 fixwave round-2
(rejected — 80% funclet mirage, ~20-30 fixable, no cascade); lane-B near-pair
residue (drained); A_TOOLING ICF fold mirage; pad-probe deferred struct walls
(drained); local-static mechanical wave; the 3 scatter-sweep w1 lossy candidates
(CameraShot←Flow, PropAnim←PropKeys/AmbientOcclusion, CharBonesMeshes←GemManager
— need body-dup, not whole-file include).

## Method (stable)
Fable coordinator delegates to Opus agents in `scripts/setup_worktree.sh`
worktrees under `~/tmp`; coordinator independently re-verifies every diff with a
fresh clean-worktree whole-binary A/B (strict set keyed `(unit, name)`, LOST must
be empty) before a path-limited commit on main; `touch config/45410914/config.yml`
before any A/B leg that changed splits/map (renamer re-split trap). Scoreboard:
`docs/plans/tu5-p5-progress.md`. Memory: `project_comdat_scatter_lever_2026-07-19`.
