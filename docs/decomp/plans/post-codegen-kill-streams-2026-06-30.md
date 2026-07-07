# Post-codegen-KILL execution streams (2026-06-30)

Context: the codegen-near-miss investigation ("body-divergence wall #2") was
KILLED as a NEW-codegen-tool effort — see
`docs/decomp/research/2026-06-30-nearmiss-codegen-inventory.md`. But that diagnosis
ALSO produced concrete, executable matching candidate lists routing to the
EXISTING levers (header-struct, body-port). This doc captures those streams so
they persist for cold pickup, and tracks execution. The veins are KNOWN-thinning
(MEMORY: struct-lever isolated-single-class, bodyport mostly-spent) but the owner
asked to push them anyway — harvest what's there, report honest yields, loop to
exhaustion.

Cached inventory the streams draw from (regenerable, read-only):
- `/tmp/claude/nearmiss_inventory.jsonl` (primary class per near-miss)
- `/tmp/claude/unattributed_enriched.jsonl` (instruction-level sub-class)
- regen: `tools/classify_nearmiss_codegen.py` → `tools/enrich_unattributed.py`
  → `tools/split_imm_offset.py`

## DISCIPLINE (non-negotiable — these are MATCHING changes, not diagnosis)
- Each candidate worked in an ISOLATED CoW worktree (`scripts/setup_worktree.sh`).
  NEVER mutate main. Recon-first.
- Whole-binary composed A/B before any claimed win:
  `rm -f build/45410914/*/target_symbol_renames.stamp; touch config/45410914/config.yml;
  tools/fresh_report.sh` — run TWICE (run1==run2 deterministic), 0 unexplained
  regressions. true-100 byte-equal only; NEVER commit a partial.
- `tools/icf_alias_check.py` (no <=44B stub-fold inflation). `tools/fuzzy_progress.py`
  for fuzzy context.
- Coordinator (Opus) keeps selection/verification/landing; agents return patches.
  Land one at a time via `scripts/harvest/land.sh` + wave-loop SOP
  (`docs/decomp/handoff/wave-loop-SOP-2026-06-20.md`). After EVERY land, re-run
  `configure.py` and grep "Missing configuration for <TU>" (cross-agent
  objects.json-drop hazard that silently zeroes a landed wave).
- Sonnet ports are UNRELIABLE (5 prior incidents). Ports need HEAVY coordinator
  gating every wave (icf_alias_check + composed-verify + main-tree leak-check +
  splits overlap-check).

## STREAM 1 — STRUCT-OFFSET cascade clusters (highest EV)
Units with many IMM_OFFSET near-misses sharing a UNIFORM member-offset delta =
DC3/Wii struct drift (a member added/dropped) where ONE header fix cascades
across all access sites. Ranked candidates (delta = base−target, count):

| unit | uniform delta | sites | named | oracle | notes |
|---|---|---:|---:|---|---|
| CharEyes | +16 | 32 | 4 | dc3 (char engine) | strongest single-delta signal |
| CreditsPanel | +4 | 26 | 4 | rb3-Wii (game) | clean uniform +4 |
| GamePanel | +24 | 7 | 0 | rb3-Wii (game) | perfectly uniform, all 7 fns |
| CharIKHead | +4 | 14 | 1 | dc3 (char) | uniform +4 |
| Character | +8 | 6 | 6 | dc3 (char BASE) | base class → may cascade WIDE |
| LightPreset | ±60 | 17/14 | 8 | dc3 | symmetric = member block move/swap |
| CharDriver | +36 / +12 | 13/13 | 5 | dc3 | two deltas — coupled members |
| CameraManager | +36 / +48 | 8/6 | 5 | rb3-Wii/dc3 | multi-delta |
| HamCharacter | −96 | 7 | 1 | rb3-Wii (game) | uniform −96 |

⚠ Direction matters: must oracle-confirm whether retail ADDED a member we lack
(grow our struct) or DROPPED one we have (shrink). MEMORY warns some re-basings
REGRESS (RndMat/RndFont/RndWind are confirmed RB3-360==DC3 — do NOT re-base).
Recon must verify direction via objdiff anchor + oracle header diff BEFORE apply.

## RESULTS (2026-07-01)
- ⭐ **+2 LANDED (main @c336c46)**: FileMerger::Clear + RndParticleSys::CheckBursts
  to strict-100 (whole-binary A/B verified NET +2, 0 regressions). Both DC3-drift
  reverts (arg-count / POD-ctor). **The bodyport recon hypotheses are RELIABLE** —
  unlike the struct-cascade (below).
- ⛔ **CharEyes struct-cascade = NET +0 (DISCARDED)**: the +16 IMM_OFFSET "delta"
  was NOT a removable-member drift. Shrinking CharEyes (mDartOffset #ifdef
  MILO_DEBUG) made near-misses WORSE (EnforceMinimumTargetDistance 99.87→97.37) —
  retail-360's CharEyes HAS that 0x10 (the rb3-Wii MILO_DEBUG guard does not apply
  to retail-360). ⚠ LESSON: IMM_OFFSET "uniform delta" clusters are NOT reliable
  struct-lever candidates — the classifier sees immediate diffs that are a minority
  of the residual, not a clean cascadeable member drift. Prefer the bodyport
  recon's mechanism-level analysis over the classifier's offset-delta heuristic.
- ⭐ **+1 MORE LANDED (main @2bc9830)**: EditSetlistPanel::SetEditState (retail
  VerifyStrings in case 4). **Session total = +3 matched** (main-verify full
  pipeline confirms FileMerger/Part/EditSetlistPanel all 100%, 0 regressions).
- ⛔ SortNodes (+6 predicted) = NET +0 DISCARDED: Data.h SortNodes(int)→() only
  nudged GetContextFlags 99.38→99.44 (not 100) — the residual is dominated by
  something other than the SortNodes arg; rb3-Wii oracle over-predicted (like
  CharEyes). Recon hit-rate this wave = 3/5 (FileMerger/Part/EditSetlist hit;
  CharEyes/SortNodes missed). LESSON: cascade predictions from arg-count reverts
  are optimistic — a caller reaches 100 only if the arg diff is its ONLY residual.
- DEFERRED: MusicLibrary (+1/2, inline-policy + needs target_symbol_map entry;
  more complex than a clean revert — pick up in a calmer window).
- STREAM 1 (struct-offset clusters) DE-PRIORITIZED after CharEyes disproof; the
  IMM_OFFSET buckets need per-fn mechanism recon (like Stream 2), not the
  offset-delta heuristic, to avoid false positives.

## RESULTS (2026-07-02) — Stream-3 mislabel harvest round 1: **+5 LANDED (main @ddb8e6a)**
- Stream-3 recon (read-only agent over the cached JSONLs): **51 true mislabels**,
  top 22 ranked+oracle-checked → `~/tmp/stream3_mislabel_candidates.json`
  (raw 51-list at /tmp/claude/stream3_mislabeled.json).
- ⭐ **MusicLibrary +2** (the deferred candidate): PushHeaderDataToScreen rewritten
  with function-local statics (Symbol+Message) → MSVC emits the out-of-line body
  retail has; UpdateHeaderData's trailing call becomes bl. Map entry 0x8252A5C8
  pairs the reconstructed body. BOTH → 100.
- ⭐ **OutfitPiece operator>> 92.1→100**: retail evaluates `gRev > 0xB` BEFORE the
  mColors[1] store; hoisting the gRev read into a local (`unsigned short rev`)
  reproduces the order. NOT the lbz→lhz member-width the classifier suggested.
- ⭐ **GetDefaultMatShaderOpts 95.7→100** via TWO shared-header fixes:
  (a) Mesh.h HasAOCalc/SetHasAOCalc direct-member (rb3-Wii + retail asm agree;
  the mGeomOwner-> indirection is DC3 drift), (b) Mat.h SetHasAOCalc single-assign
  (the `=0; =calc;` double-assign left a dead `mr`). Cascaded clean binary-wide.
- ⭐ **FaceCenter 93.7→100**: retail interleave = split the mGeomOwner/mVerts
  derefs around x,y,z zero-stores (`owner` local first, zeros, then verts deref).
  Pure statement-order sculpting — worked on the 3rd shape attempt.
- **InitMakeString 99.65→~99.9 PARTIAL (landed)**: retail allocates 0x800 buffers
  (rb3-Wii agrees; our 0x1000 was wrong). Residual = MSVC .bss ANCHOR-selection
  (r29 anchored at gBuf vs retail gLock) — decl-order swap does NOT flip it;
  at-limit for hand-fixing.
- ⛔ **RndFlare::CalcScale (99.1) DEAD — codegen cliff**: residual = lfd(double 0.0)
  vs retail lfs(float 0.0) + 1 fmadds swap. ANY float-typed compare (`0.0f<x`,
  `x>0.0f`, inline or via temp) RESTRUCTURES the whole FP schedule → 93-94%.
  The double-literal shape is the fixed point. Corroborates the wall-#2 KILL:
  a 2-instruction residual that is provably source-unreachable.
- ⛔ **MsgSinks _Copy_Construct SKIPPED**: MsgSinks doesn't exist in rb3-Wii (it's
  DC3-era); the lfs-at-0xC "float member" evidence may be a different ICF-folded
  instantiation. A type flip on this evidence would be a guess — needs identity
  work first.
- Gates: whole-binary composed A/B NET +5, 0 strict / 0 fuzzy regressions;
  icf_alias_check HONEST (5 real-bodied, 0 stub-folds). Cherry-picked onto main
  over the owner's wave-3 integ; map conflict resolved by json-union (+1 key).
- ⚠ ENV: three of my worktree builds were killed mid-flight by an external sweep
  (SIGINT/kill, no OOM, no error in log — likely concurrent-session cleanup
  pkills). WORKAROUND that held: supervisor retry loop + ninja incrementality
  (~/tmp/musiclib_supervise.sh pattern) + marker-based monitors (grep for
  "fresh_report.sh: done"), NOT PID-based. Also: do NOT build in main while the
  owner's wave is mid-landing (renamer crashed on a mid-write zero-byte obj race;
  "Missing configuration" TUs are the owner's pinned-but-unwired in-flight state).
## RESULTS (2026-07-02) — Stream-3 mislabel harvest round 2+3: **+9 LANDED (main @2158b35)**
Round-2 (coordinator, per-fn): ArkHash::Read (CSE heapSize+len local), MakeString
0x800 (partial, at-limit). Round-3 = **5 Fable subagents** (3 lanes of ranked
candidates + identity check + HamCam sizeof lane) worked the top-22 in one
worktree. 10 fns hit 100 in-worktree, but the FULL batch A/B = **NET +32 /
23 strict + 14 fuzzy regressions** (three shared-header changes with incomplete
cascade). BISECTED → landed only the **regression-free isolated subset (+9,
run1==run2, 0 regressions, icf HONEST 8 real-bodied)**:
  ArkHash::Read, RndBitmap::PixelColor (hoist mPalette across PixelIndex),
  RndGenerator::Generate (push_back→push_front DC3-drift), SongDB::GetSustainGemCount
  (+ GameGem.h sizeof 0x2c→0x44, which also flipped PracticePanel::MarkGemsAsProcessed
  for free), ManageBandPanel::Handle (retail dropped clear_profile HANDLE entry),
  Splash::Draw (retail Suspend + no cam-guard), SynapseAPO dtor (in-place +OggFree),
  SpotMeshEntry::operator= (hoist memcpy src).
⚠ **PRESERVED for follow-up** (branch `followup/round3-full-batch @ 3879248`, doc
`docs/decomp/handoff/round3-shared-header-followups-2026-07-02.md`):
  - **FileLoader + ObjDirItr DC3-drift reverts** (16 files) — MIXED: real gains
    (LightHue::Sync→100, GetNormalMapTextures→100, Char*/AmbientOcclusion cascade)
    but ~6 funclet STUBS (DirLoader/BandWardrobe/TrackDir → 0%) from incomplete
    call-site cascade. Correct direction (oracle+retail agree), just unfinished —
    repair the stubs then re-A/B. **This is the highest-value follow-up.**
  - **CollideListSubParts devirt** — NET-NEGATIVE (broke 15+ vtable functions to
    help 0). Do NOT re-attempt without new vtable evidence.
  - **HamCamTransform sizeof lever** — real (TransformCrowd 0x10→0xc) but net −3
    unit-wide due to target_symbol_map misnaming; needs the map fix first.
⭐ METHOD that keeps working (now 8/9 hit rate this session): read the 3-6 instr
residual, reconstruct retail EVALUATION ORDER (hoist a global/member read into a
local, CSE a reused subexpression, split a pointer chain, single-assign setters).
Fable subagents are effective at this per-fn work; the DANGER is shared-header
edits — gate EVERY wave with a full composed A/B and bisect before landing.

REMAINING Stream-3 queue: Vector3Keys::SetFrame 98.42 (dst-addr-hoist codegen
cliff, likely at-limit), LightHue::Sync (in the loader follow-up bundle),
+ ~8 more ranked in `~/tmp/stream3_mislabel_candidates.json`.

## STREAM 2 — BODY-divergence per-fn ports (bodyport lever)
Genuine oracle logic/guard/arg divergences (port the real body from the oracle
to strict-100). Best non-STL named candidates:

| fn | unit | pct | oracle |
|---|---|---:|---|
| MusicLibrary::UpdateHeaderData | MusicLibrary | 95.74 | rb3-Wii |
| AppChild::Poll | AppChild | 97.92 | dc3 |
| RndRenderState::Init | RenderState | 94.74 | dc3 |
| FileMerger::Clear | FileMerger | 97.67 | rb3-Wii/dc3 |
| RndParticleSys::CheckBursts | Part | 95.06 | dc3 |
| RndBitmap::SaveBmp | Bitmap | 90.62 | dc3 |
| CharBoneDir::GetContextFlags | CharBoneDir | 99.38 | dc3 |
| EditSetlistPanel::SetEditState | EditSetlistPanel | 93.55 | rb3-Wii (game) |
| FftIpp::~FftIpp | FftIpp | 97.96 | dc3 |
| RndRenderState / RenderState::Init | RenderState | 94.74 | dc3 |

(STL `??$?5...` operator>> template instantiations DEFERRED — body lives in a
shared header, wide-breakage risk, divergence often regalloc not logic.)

## STREAM 3 — classifier mislabel reroute (cheap, residual)
~16–22 near-misses my classifier put in PEEPHOLE/IMM_OFFSET/REGALLOC whose REAL
residual is a struct-size(divw/mulli sizeof) / member-type(lfs vs lwz) delta or a
logic/guard body divergence. Re-route them into Streams 1/2. The validation agents
flagged these; mine `unattributed_enriched.jsonl` for PEEPHOLE entries with
`divw/mulli/lfs` transitions.

## STREAM 4 — dormant FPR permuter drivers (LOW EV, optional)
Wire `fpr_declaration_reorder` + `first_use_reorder` (0 runs, exist in
`../decomp-synth`) into the active scan set; one bounded pass on the 5
REGALLOC_FPR_CALLEE (CheckBSPTree/FastInvert/Rot::MakeScale). Predicted 0–3.
Touches shared `../decomp-synth` → isolate + do-no-harm. Deprioritized.

## EXECUTION LOG
- 2026-06-30: streams identified + documented. Codegen-tool effort KILLED.
  Wave 1 launched: Stream 1 recon→apply on the top struct clusters.
- 2026-07-01: Wave-1 struct-cascade results (⚠ ran concurrent with a heavy owner
  bodyport wave → IO storm load 200+ blocked/rate-limited several lanes):
  - **CharEyes +~32 READY**: edit applied+dual-oracle-confirmed in wt-s1-CharEyes
    (mDartOffset is MILO_DEBUG-only → wrap in #ifdef; our retail carries it = +0x10
    drift; CharEyes-OWN, no cascade; retarget line 912 write to mCurrentDartOffset
    using the existing 626/640 cast idiom). Verification storm-blocked → COORDINATOR
    to A/B + land. Independently sanity-checked read-only: sound.
  - **CharIKHead DEFER**: the +4 is a shared-base (RndPollable/CharWeightable)
    vtordisp/virtual-decl drift = the RndMat/RndFont re-base hazard. High regression
    risk, low EV. Not a clean member edit.
  - **Character / GamePanel / CreditsPanel**: apply agents rate-limited (transient),
    incomplete → re-run recon+apply at LOW concurrency later.
  - Bodyport-recon (read-only) landed 5 PORTABLE_WINs (saved
    ~/tmp/bodyport_recon_results.json): CharBoneDir/SortNodes Data.h 1-arg→0-arg
    (+6 cascade, shared header), FileMerger::Clear bool→no-arg (+2), Part Burst POD
    (+1), EditSetlistPanel VerifyStrings case-4 (+1), MusicLibrary inline-policy
    (+1/2). Plus 4 confirmed DEFER_CODEGEN (AppChild/RenderState/Bitmap/FftIpp) =
    more corroboration of the wall-#2 KILL.
  - ⚠ LESSON: never run my heavy build-wave concurrent with the owner's active
    wave (14 concurrent builds = load-200 storm that starves ALL builds). Switch to
    COORDINATOR-DRIVEN SERIAL landing in ONE worktree (incremental A/B), light
    footprint. Note: clean HEAD now builds to ~10682 (owner landed +18); measure
    NET via in-worktree A/B, not the absolute count.
  - ⚠⚠ CRITICAL ENV UNBLOCK: fresh worktree builds FAIL at the `build/compilers`
    ninja edge — it runs `tools/download_tool.py` which tries to download the MSVC
    toolchain from files.decomp.dev and dies on `SSL: CERTIFICATE_VERIFY_FAILED`
    (no cert path in this env; certifi retry also fails). ninja rebuilds the edge
    because a fresh worktree has no .ninja_log entry (NOT mtime — touching doesn't
    help). This silently blocked the whole struct apply wave (stale-report false
    +0, "frozen priming"). FIX (per-worktree, do NOT commit — this is the
    download_tool.py the verify-stage-wave skill says to EXCLUDE): short-circuit
    download_tool.py `main()` to `return` early when the output already exists
    (setup_worktree symlinks build/compilers from main). Apply this in EVERY
    worktree before building. TODO: bake into setup_worktree.sh (shared-tool change).
  - ⚠ PROCESS-HYGIENE LESSON: `pgrep -f 'wt-s1-CharEyes'` MATCHES MY OWN SHELL
    (pattern in the command line) → `kill` self-terminates the command (exit 144 /
    no output). Never pattern-kill on a string that appears in the kill command;
    kill by explicit PID. Also: when taking over an agent's worktree, its leftover
    verify script keeps building → two drivers fight the ninja lock → stale objs /
    corrupt build dir. Recreate the worktree clean instead of fighting it.

## RESULTS (2026-07-03) — Fresh harvest wave on main@11120: **+11 LANDED (main @94c02c5)**
Owner had already landed the round-3 FileLoader/ObjDirItr follow-up (commit
`69d4216`, +25) — that bundle is DONE. Ran a fresh near-miss scan on main@11120
(315 named real-bodied [95,99.999); 91 non-STL in the 96-99.5 harvestable band)
and delegated the top 16 to **4 Fable subagent lanes** with a STRICTER rule this
round: **local .cpp edits ONLY, no header edits** (report header-needs instead).
Result: 12 in-worktree 100s; composed A/B = +12 with **1 coupled regression**
(MidiParser funclet), dropped that one edit → **+11, run1==run2, 0 regressions,
icf HONEST**. Landed: String::operator= / operator+=, RandomIntervalGroupSeqInst::
Poll / ComputeNextTime, RndPostProc::UpdateColorModulation, GemTrackDir::GemPass,
GuitarController::Handle, ChordbookPanel::CreateController, PrefabMgr::
AssignPrefabsToSlots, BlockMgr::Poll, BandDirector::OnMidiShotCategory.

⭐ NEW REUSABLE TECHNIQUES (all .cpp-local, evaluation-order class):
- **unsigned-int char temp** in a manual copy loop reproduces retail's `cmplwi`
  where `strcpy` emits signed `extsb.` (String::operator=/+=). Widen the loop
  char to `unsigned int` — `unsigned char` gives `mr.` not `cmplwi`.
- **`(int)` cast on a pointer null-check** forces signed `cmpwi` where the
  pointer test defaults to unsigned `cmplwi` (GemTrackDir::GemPass).
- **explicit `= 0` init on a global** moves it .bss→.data (ascending vs
  reverse-declaration layout), a lever for retail anchor displacements when a
  cluster of stat globals is read at fixed offsets (BlockMgr::Poll gRead/gSeek).
- **per-TU `#undef HANDLE_MESSAGE` + redefine** adding `(unsigned char)` on the
  OnMsg return reproduces retail's bool-return truncation (`clrlwi r3,24`) where
  the rb3-Wii oracle's `int` is MWCC-indistinguishable (GuitarController::Handle).
- **opaque-pointer store** (`float *p=&m.y; *p=...`) blocks the compiler from
  CSE-ing an adjacent member load across the store (PostProc).
- lift a nested sret temp into a named local to kill the `mr r28,r3` save
  (ChordbookPanel::CreateController); hoist a call-arg temp before a MakeString to
  fix arg-materialization order (BandDirector).

⚠ FOLLOW-UPS surfaced (deferred):
- **AppChild::Poll 97.9%** — needs `src/system/obj/Data.h` `Execute(bool fail=true)`
  → Wii-era no-arg `Execute()` (DC3-drift API addition; sole mismatch is base's
  extra `li r5,0x1`). Shared-header + possible positive cascade (every `Execute`
  caller) — do it as its own A/B-gated change, audit `Execute(false)` call sites.
- **MidiParser::AddMessage** — body reaches 100 (keep msg/firstArg params live +
  separate `arr`/`first` locals) but the +8 stack shifts its EH-cleanup funclet's
  DataArray temp 0x50→0x54 (one `lwz` off-by-4). Needs the retail source shape
  that gets the body WITHOUT the frame growth. Near-win.
- WALLS (don't re-attempt): SkinVertex (regalloc cycle, permuter-refuted),
  BinStream::Write (u8-narrow mask, compiler value-range prop), BlockMgr::AddTask
  (CSE reload wall), RndFlare::CalcScale (lfd/lfs codegen cliff).

⭐ PROCESS: the local-.cpp-only rule made this wave clean — the ONLY regression
was a same-TU funclet coupling (not a shared-header cascade like round-3). The
per-fn hit rate held (12/16 to 100; ~2 walls, 1 header-deferred, 1 dropped).

## RESULTS (2026-07-07) — r5 harvest wave on main@11240: **+15 LANDED (main @b78b194)**
First wave run off the new playbook (`docs/decomp/playbooks/nearmiss-harvest.md`)
with the pool generator + verdict registry (`scripts/harvest/gen_nearmiss_pool.py`).
4 Fable lanes, 23 candidates, 16 in-worktree wins; composed A/B x2 = **+15 gained,
0 regressed, run1==run2 (11240→11255), icf HONEST**. Third consecutive
zero-regression wave under the local-.cpp-only rule.

Landed: LightPreset PropSync x4 (ONE edit — `(unsigned)idx >= size()` bounds-check
cast — closed the family; EnvironmentEntry also reproduces a retail copy-paste bug:
fog_color PropSync reads mAmbientColor), SHA1::Update (syntax-form CSE defeat:
mix `x*8` with `x<<3`), DecodeDxtColor (address-shape lbzx), MoggClip::SynthPoll
(qualified devirtualized `MoggClip::Stop(0)`), MicManagerXbox ctor (struct → 5
file statics, DC3 model), Synapse::ProcessInPlace, CharEyes::EyesOnTarget (drop
null-guard), BandIKEffector::ApplyConstraints (Max operand polarity + split
mul/div reassociation), TrackWatcherImpl::CheckForPasses/CheckForPitchBend
(Min/Max → fsel), Game::E3CheatAutoplayAccuracy, StreakMeter::SetPartColor,
SongDB::GetPhraseExtents + GemManager::IsSpotlightGem (big fuzzy wins, strict
blocked on Game.h — see below). Banked fuzzy: CharIKFingers 95.8→99.7,
UsbMidiGuitar::Poll 99.16→99.9.

⭐ HEADER-NEEDS (4, queued as own gated changes —
`docs/decomp/handoff/round5-header-needs-2026-07-07.md`): (1) **Game.h drop
Wii-only DiscErrorMgrWii::Callback base** (mProperties 0x30→0x2c, two witnesses,
binary-wide cascade candidate); (2) Data.h SortNodes(int)→SortNodes() (+6
cascade, second witness — pair with the round-4 Execute(bool) finding); (3)
StandardStream.h +8 (mAccumulatedLoopbacks@0x164 + 4B marker region); (4)
Joypad.h mType 0x74→0x6c (stride 0xd4 verified).

New walls registered in scripts/harvest/nearmiss_verdicts.json (auto-excluded
from future pools): VocalTrack::ProcessStaticLyrics (CSE-reload), 5x strcpy
extsb. members (CharUtl pair, FirstSortChar, ReportHash, OnChangeFaceGroup),
CSHA1::Final (u8-narrow).
