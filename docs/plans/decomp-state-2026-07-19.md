# rb3-xenon decomp — state & live veins (2026-07-19)

**Current: 18,689 strict-matched functions** (`build/45410914/report.json`,
`match_percent_normalized == 100.0` exactly). Denominator is the whole TU5 XEX
(~69k functions). Afternoon session +68 (18,621→18,689): scatter expose-and-fix
+15, unwired-owner scatter-wiring +52, near-miss +1.

## ⛔ PIVOT POINT (2026-07-19 pm) — cheap wire-and-flip / near-miss veins EXHAUSTED

Every coordinator-hand-wave vein was probed to exhaustion this session, each
gated cheaply with zero regressions:
- scatter expose-and-fix ≥88 band = **MIRAGE** (mispair / reloc-coloc / struct-artifact)
- struct-recon = **DEAD** (5/5 leads ICF/foreign-offset)
- near-misses = **AT_LIMIT** (regalloc / RB3<DC3 vtable)
- TrackWatcher = **NO-WAVE mirage** (own methods done, span = foreign scatter)
- grouped-globals = **1 fix** (SystemMs), rest banked
- unwired scatter-include = **+52, DRAINED** (7 cands, 5 flipped)
- unwired own-span wire-and-flip = **DRAINED ≈0** (body-port, not wire)

**What remains is DEEP GRIND: body-porting the ~103 unwired engine TUs + the
~5,300 divergent-body long tail (partial→100 via DC3/rb3-Wii oracle).** Per the
user mandate ("avoid deep grind unless high cascade") and Fable review #3, this is
a **work-kind pivot for the USER to decide**, not a unilateral coordinator grind.
Recommendation to bring the user: route the divergent tail to the AUTOMATED
machinery (crack-farm / grind-loop / the training-corpus model) — that pool is
exactly what it's built for — while coordinator attention moves to whichever the
user ranks of native-port / OSS-build / HW streams. Two explicit asks: (a) re-open
permuter or keep banned; (b) fund a divergence-triage pipeline as batch infra.
The id round-5 gate (+~1,000 names) is NOT reachable at the ~+70/session naming
pace, so the flywheel needs a bigger name-feed (body-port waves) to re-open.

## ▶ AUTOMATION BUILD-OUT (2026-07-19 pm, user-directed)

User decisions: **permuter stays BANNED**; **build the divergence-triage
classifier first** (price the automatable yield before funding any fleet), and
concurrently run Opus-foreman/Sonnet-worker grind waves whose outcomes serve as
ground truth to refine the classifier.

Fresh pool (report.json regenerated at 18,689 baseline, cache cleared;
`~/tmp/triage_pool.csv`): 7,723 named divergent fns / 2.97 MB. 6,341 at exactly
0% (unwired/scatter/unmapped mass); the divergent-body pool = 1,382 fns / 440 KB:
0–50: 292 · 50–75: 138 · 75–90: 260 · 90–98: 289 · 98–99.8: 145 · 99.8+: 258
(the 99.8+ band is mostly reloc-coloc residue — skip bucket).

In flight: (a) Fable tooling lead + Opus implementers building
`scripts/triage/divergence_triage.py` in wt-triage — buckets = mispair /
reloc-coloc / struct-artifact / form-divergence / body-port / zero-unwired,
features via batched `objdiff-cli diff -f json` + `scripts/analysis/
diff_inspect.py` analyzers; output `~/tmp/triage_{results.json,buckets.md}`.
(b) Opus grind foreman running 2–3 waves × 4–5 Sonnet workers on the 90–99.8
band (walls excluded via get_attempts), producing verified diffs for
coordinator landing + ranked tooling-gap feedback.

### Results (same day): classifier LANDED + calibrated, campaign +21, main 18,710

`scripts/triage/divergence_triage.py` on main (full pool 36s warm). Landed
gains: missing-instantiation vein +9 (`ba690393` + harvest), VocalPlayer grind
+7, foreman package +5 → **18,710**, zero regressions. Grind campaign ground
truth (24 assignments: 13 flips/3 improves): **route by diff shape, not %**
(screened 12/15 vs unscreened 1/9); I/D-cluster≥3 ≈ flip; regswap-only = skip;
97.5–99.8 = survivor-bias wall band, 78–96 = flip band. Full rules in memory
`project_grind_foreman_groundtruth_2026-07-19.md`.

**FINAL bucket table** (4 calibration rounds; snapshot committed at
`docs/plans/triage-buckets-2026-07-19.md`, regen with
`python3 scripts/triage/divergence_triage.py --jobs 12`): BODY-LEVER 240
(MEASURED per-stratum: 70-90 non-STL 25%, else ≤5%) · LEVER-STRING 41 +
LEVER-SYMBOL 9 (validated off 1 flip each — calibrate in first wave) ·
ZS-INST 17 (probe 2/2) · BODY-PORT 172 · STRUCT-ARTIFACT 175 + FORM-DIVERGENCE
146 (**UNMEASURED estimates — calibrate before funding**) · certified-skip 318
(RELOC-COLOC 160, WALL-VTORDISP 60, WALL-DEADARG 7, ZS-STL 84, STL-CONTAM 7) ·
MISPAIR 191 (map fix first) · UNRELIABLE-EVIDENCE 226 (stale live-diff, re-verify
before routing) · NEEDS-REVIEW 221 · ZERO-UNMAPPED 5,766.

**Honest fleet economics: bankable ≈96 expected flips** (BODY-LEVER ~26-35 +
LEVER-STRING ~36 + ZS-INST ~15 + LEVER-SYMBOL ~8 + BODY-PORT 78-96 ~3);
**estimate-only upside ≈149** (STRUCT 105, FORM 44) pending 20-30-fn calibration
waves. The original 530 was ~2.2× overpriced (BODY-LEVER measured 6.7% vs 80%
priced — calibration wave 30 fns: only 70-90 non-STL flips at 25%, STL 0/6,
mispairs 9/30). Calibration wave itself landed +7 incl. the **codec.h
`__forceinline` alloca lever** (6 vorbis fns / 1 line; intrinsic-wrapper class
swept — UNIQUE instance, closed). decomp.db drift: ~3k strict fns have renamed
symbol keys; treat get_attempts "not found" as unknown, not pass.

**Session arc (2026-07-19 pm, automation build-out): 18,689 → 18,717 (+28)**
— ZS-instantiation vein +9, VocalPlayer grind +7, foreman package +5, calibration
wave +7. Zero named regressions across all landings.

## Recent arc

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
  **PROBE RESULTS (2026-07-19) — REFINED PREDICATE, both wide leads DEAD:**
  S2 CharPollGroup = **misread** (the −0x28 was a member offset 0x50 vs an
  ICF-folded `??_G` dtor's full-object adjust 0x78; layout already matches retail;
  ground-truth against target-asm MEMBER offsets, NOT Ghidra `??_G` adjusts —
  ICF-contaminated). S1 SongSection = size mismatch is **real** (0x18 vs 0xc, DC3
  added mPatternRange+mSongPattern) but **cascade REFUTED** — its only
  `vector<SongSection>` consumers are 2 unimplemented stubs; **zero near-misses
  index it** → 0 flips. **THE RULE: a struct resize flips a near-miss only when a
  near-miss (90–99.99%) actually indexes that struct. Size-mismatch is necessary
  but NOT sufficient.** So the scanner predicate is NOT "struct size ≠ retail" —
  it's "struct size ≠ retail STL-stride AND indexed by ≥1 fn in the 90–99.99%
  band" (join size-deltas against the near-miss pool). The 3 narrow S3 leads
  (RecurseInfo/Constraint/StoreMainPanel) were each derived FROM a near-miss
  (99.9x), so they satisfy the predicate — S3 is the live test of the vein.
  **S3 RESULT — VEIN DEAD (all 5 struct leads mirages, 2026-07-19).** RecurseInfo
  0x10 is real but holds two 0xC Strings (=0x18; can't shrink without global
  String change). Constraint copy-ctor matches 100% at 0x1c (F2's `li 0xc` = a
  mis-paired ICF body). StoreMainPanel ctor matches 100% (F2's `addi 0x88` = a
  BandStorePanel singleton's return+0x88, foreign object). **Conclusion: F2's
  "struct-size" immediates were real numbers but SYSTEMATICALLY ICF-fold or
  foreign-offset artifacts, not oversized fields — the Movie::IsLoading mispair
  lesson generalized to the whole exposed sub-100 band. Do NOT fund a struct-size
  scanner sweep; do NOT re-hunt these. The ≥88 exposed band is a mirage across ALL
  three sub-taxonomies (mispair / reloc-co-location / struct-artifact).** Net from
  the entire struct-recon probe lane: 0, but 0 regressions (verify-before-edit
  gate held on all 5).
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
- **Wave 2 — UNWIRED-OWNER SCATTER-WIRING = THE TOP LIVE VEIN (probe P2 GO,
  +9 @3917a0e4).** The winning shape: **117 `.cpp` files exist in-tree with full
  bodies but were never wired** (not in objects.json → no obj emits them; list
  `~/tmp/unwired_cpp_list.txt`). Retail scattered their COMDATs into an
  already-wired unit's `.text` span → a near-free `#include "<owner>.cpp"` append
  to that consumer emits + pairs them. P2: CubeTex.cpp += 4 includes
  (rnddx9/{MultiMesh,Cam,Lit,Part}.cpp) → +9 in ~5 min, 0 regr. Sweep running
  (`~/tmp/uwire_worklist.md`). **~60–65% clean flip rate**; FILTER OUT
  multiple-inheritance dtors (`??1`/`??_D`/`??_G` of 2+-base classes) — they ride
  a shared-base layout delta, only reach 99.x, route to a separate struct stream.
  Prioritize engine files (rnddx9/rndobj/synth/movie/os/net/midi) over gesture/*
  + Dance-Central hamobj/* (mostly Kinect, likely no RB3 target). EV: unknown
  addressable pool, but each hit is ~free. This SUPERSEDES the old "per-symbol
  owner-driven port" framing below — the bodies already exist; only the wiring
  was missing.
  **OWN-SPAN WIRE-AND-FLIP = DRAINED (2026-07-19, gated out ≈0).** The captain's
  "dark own-span pool, engine/lib-heavy, good byte-match prior" thesis was based
  on DC3's tree, not ours: the big C-lib pools don't exist in `src/` (jpeg=1 file
  not 73, zlib=1, oggvorbis=1, net=14/3-unwired not 107). Real unwired pool =
  **~103 engine files**. Best case (26 with pre-carved target objs, 20 compiled)
  → **5 byte-identity hits, ALL noise** (vtable-adjustor thunks + unwind funclets),
  0 real flips, no ≥3-hit clusters. Root cause: DC3-lineage bodies DIVERGE from
  retail RB3, and TU5 map-anchoring already carved every span that byte-matches an
  anonymous region — so the ~77 files with no target obj are precisely the ones
  whose bodies don't match. **These are BODY-PORT targets (partial→port to 100%
  via DC3/rb3-Wii oracle, the `bodyport-batch` skills), NOT wire-and-flip. Do NOT
  build a whole-binary own-span correlator.** With this, ALL cheap wire-and-flip
  and near-miss veins are exhausted → pivot territory (see PIVOT below).
- ~~**Wave 2 (old) — Oracle-backed UNWIRED wiring** (superseded by the above; the
  "port the bodies" premise was wrong — bodies pre-exist, just unwired).~~
  Original target census (for reference): rnddx9 CubeTex 8 Dx* + Rnd_Xbox(3),
  Anim(7), Sequence(8), MemTracker(8), DataPointMgr(5), WaveFile(4), Cam(2); game
  DataArraySongInfo(11), TrainerPanel(5), VocalTrack(3), VocalPlayer(3). SKIP
  oracle-poor (System/LEAPCORE, Mic, FFT, Compress, DSP, rtti/osfinfo).
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
  **CORRELATOR RUN DONE (2026-07-19) — it's a real body-port grind, NOT a cheap
  id win.** `tu5_reloc_masked_correlate.py TrackWatcherImpl.obj (target) vs our
  compiled obj` → only **14 UNIQUE byte-matches, ALL boilerplate** (`__unwind$`
  funclets + `bad_alloc` dtor); **0 real named methods match.** The 78 unmapped
  bodies are genuinely DIVERGENT (NOMATCH) — our source is a rough Wii port that
  doesn't byte-match 360 retail. So each flip needs a real body-port THEN
  correlator-pairing (unnamed target). EV per Fable (+80-140) is optimistic;
  recommend a SMALL probe (1 agent, ~8 representative bodies, measure port
  hit-rate) before committing 4–6 agents. If hit-rate is low, TrackWatcher is a
  low-ROI grind → pivot to oracle-backed unwired wiring (vein #2) or the
  round-5-prep / user pivot conversation.
  **PROBE VERDICT (2026-07-19): NO-WAVE — TrackWatcher is a MIRAGE.** The premise
  is wrong: TrackWatcherImpl has only 23 named methods, **22 already at 100%**
  (own methods effectively DONE); the "78 anon-0%" are FOREIGN functions
  scatter-interleaved into its 20KB pinned span (BandCrowdMeter, PartAnim,
  HamSupereasyData, Object, DataArray, STL templates — our source already
  `#include`s PartAnim.cpp + BandCrowdMeter.cpp). Correlator confirmed 0 real
  matches. Only residual = `CheckForAutoplay` 92.9% (permuter-class, deferred).
  The real (separate) opportunity buried here is BandCrowdMeter/PartAnim as
  first-class units (~20% near-misses, cross-TU layout problem, NOT clean
  porting). **Do NOT commit a TrackWatcher body-port wave.**
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
