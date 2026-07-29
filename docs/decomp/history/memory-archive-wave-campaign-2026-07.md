# [HIST] Memory archive — wave-cadence campaign era (2026-07-09 .. 2026-07-12) + crack-farm

**Verbatim export (2026-07-29) of Claude persistent-memory topic files** covering the
wave-cadence decomp campaign (waves 2-40, sessions of 2026-07-09..12, main 11,583 -> 15,822)
plus the crack-farm deploy/saturation records and one superseded diagnosis file.
Each section below is the COMPLETE original memory file, including its YAML frontmatter
(`name:` slug, recall `description:`, `[[wikilinks]]`).

**Why archived:** these are TU0-ERA records — main flipped its target to the TU5 XEX on
2026-07-15, so **every raw address in these files is invalid**. The durable levers, wall
classes, and do-NOT-re-hunt verdicts they discovered were distilled into surviving memories
(see the campaign-history-archive memory and the hub-campaign memory index). Kept here as the
searchable primary record; nothing was edited.

**Warning that must not be lost:** the crack-farm records note a Backblaze **B2 key was
echoed in logs — rotate it** before reusing that infra.

---



<!-- ======== BEGIN memory file: project_wave2_e1_autoid_2026-07-09.md (6871 bytes) ======== -->

## Archived memory: `project_wave2_e1_autoid_2026-07-09.md`

---
name: project_wave2_e1_autoid_2026-07-09
description: "2026-07-09 wave 2: RFC-21 E1 verdict = KILL-leaning (vocabulary-bound not CPU-bound, 0/9 conversions + 3 batch_unit_climber blockers); autoid vein drained for game code; Matchmaker+GuitarFx wired +27 (6249080); network/Quazal needs structural transfer not strings"
metadata:
  node_type: memory
  type: project
  originSessionId: fe599317-6faf-4a30-a027-2f05b4f041c9
---

2026-07-09 wave-2 workflow (post crack-farm landing). Three results that shape
the next strategic decisions:

**E1b FOLLOW-UP (same day): verdict now CONCLUSIVE KILL.** Both gaps closed by a
second run: (a) the joint engine GENUINELY ran after fixing 4 batch_unit_climber
blockers (objcache --fo redirect, literal-"\n" batch stdin, super-variant
additive-greedy bisect, NULL current_percent crash) — proof: 1,495 scored
variant-pairs (490 engine / 1,005 game); (b) GAME arm = all 17 named [80,95)
game near-misses w/ source across 14 band3+network TUs (substrate is THIN — max
2/TU). **Conversions: 0/17 game + 0/7 engine = 0/24 (0%)**, far under the 5%
threshold. The one apparent win (EditSetlistPanel::OnMsg fuzzy-100 +8.52) was a
FUZZY-METRIC ARTIFACT — normalized objdiff says 91.5 unchanged: ⚠ the climber's
objdiff --batch fuzzy score OVER-REPORTS vs normalized; any tu_crack reuse must
re-verify normalized. Best real mover +1.67pt then r25<->r26 callee-saved
REGISTER_SWAP wall. **DO NOT fund the CPU tu_crack farm for conversion yield —
vocabulary-bound, not search-bound; route budget to proposal coverage (LLM
generator / regalloc-crossing moves).** Artifacts: ~/tmp/e1b_results.md,
~/tmp/e1_patches/batch_unit_climber_fixes.patch. The fixes are LANDED in
decomp-synth @f1b66ca (2026-07-10) with the fuzzy-over-reporting caveat in the
commit message. First-run detail below.

**RFC-21 E1 kill-criterion smoke (first run): verdict KILL (with caveats).** Question: does
uncapped whole-TU joint search convert >=5% of a TU's [80,95) near-misses to
true-100? Answer on default/system/rndobj/Utl (9 near-misses, the max any wired
TU has — the "VocalPlayer ~32" hint did NOT reproduce): **0/7 conversions** from
BOTH mature engines (decomp-synth pattern+composition search guided AND blind,
and beam_search uncapped 16x6x64). Search saturated the ~140-pattern vocabulary
in 3-19s/fn — **vocabulary-bound, NOT CPU-bound** (0.5 of 2 CPU-hr used). Walls
are structural MWCC-vs-MSVC regalloc deltas (e.g. MSVC allocates one fewer
callee-saved reg -> ~63-swap cascade; no pattern composition adds a register).
This is §7's primary kill signal: route budget to PROPOSAL COVERAGE (LLM
generator / new pattern families), not CPU scale. CAVEATS before treating as
conclusive: (1) one engine TU tested (regalloc-heavy, plausible worst case) —
run one game-TU E1 (e.g. RockCentral) to harden; (2) the actual novel engine
never ran: **batch_unit_climber.py is NON-FUNCTIONAL against rb3-xenon** — 3
blockers: objcache --fo not redirected (serves cache hit, no obj; workaround
OBJCACHE=off), run_objdiff_batch joins symbols with LITERAL "\n" text (line
174), lockstep super-variant makes one bad variant fail the whole TU compile;
plus crack_live --generator moves scored 0 candidates (n_scored=0) and chain
depth is hardcoded max 3. Artifacts: ~/tmp/e1_results.md, ~/tmp/e1work/,
partial TransformKeys 84.65->88.22 diff in ~/tmp/e1_patches/ (NOT landable).
Best single mover: TransformKeys +3.57pt; residue = RarelyHandFixable r10<->r11
cascade.

**Autoid string-fingerprint vein: DRAINED for game code (763 TUs wired).** Fresh
regen -> 507 proposals but most unwired hits are 1-2 fn or FPs (top-ranked
PerfectOverdriveTracker was INSIDE pinned UIEvent; MultiplayerAnalyzer
mis-anchored — the rich 0x826ACEC8 cluster is actually PracticeSectionProvider/
TrainerPanel). Only ONE strong multi-fn cluster remained: Matchmaker.cpp. LANDED
both recommendations (rb3-xenon **6249080**, +27: 11556->11583, matched_code
+1092 — NOTE commit msg says 1004430, actual settled 1001260):
- Matchmaker.cpp 0x826357C8-0x826375B8: 59 fns, 25 at 100 immediately. Port
  delta: Timer::CyclesToMs(mCycles)->Ms(). 6 near-misses deferred (5@99.9).
- GuitarFx.cpp 0x826D7260-0x826D7970: GetFxSend byte-exact, Poll paired@74%
  (grind target). KEY: **rb3-Wii BinDiff oracle misattributed ALL 8 GuitarFx
  addresses** — retail linker SCATTERED the TU across the binary; only
  Poll+GetFxSend live in the span. Hand-verified target-map entries via asm
  evidence. Lesson: for scattered TUs gen_game_target_map yields 0 and
  oracle VAs lie; verify by asm content.
  **2026-07-10 close: Poll = AT_LIMIT @72.4 normalized** (recorded in DB).
  Root cause traced: retail materializes 5 FxSend-property Symbol globals as
  FULL pointers (6 GPRs) vs our lis/lwz form (5) -> retail spills from r14
  (18 callee-saved) vs our r21, recomputes negf6 inline (3 callee-saved FPRs
  vs our 4) -> whole-fn register renumbering + frame -0x30 + 4 target-only EH
  funclets. NOT source-controllable (would need to RAISE pressure). Span
  audit: 8 small fns 0x826D7698-0x77B0 = Poll's own EH funclets (4x 99.9 =
  single-bl-reloc DataNode-dtor mirage); 3 tail fns 0x826D7828/7838/7950 =
  FOREIGN (section-tracker class) — span over-wide, honest end = 0x826D7828
  (split-fix proposal, 0 delta, not applied). Patch kept ~/tmp/gfx_poll.patch.
  DON'T re-grind Poll.
Leads left: FreestylePanel.cpp 0x8269C530-0x8269CA40 (medium conf, boundaries
need Ghidra; a wt-wire-fspanel worktree by ANOTHER agent already exists),
SessionUsersProviders/KickPlayerMsg adjacent to Matchmaker, MakeupProvider
single-fn. **network/Quazal (178 TUs, 10 wired) is UNREACHABLE by string
fingerprinting** — protocol code is string-poor; needs Ghidra+BinDiff
structural transfer instead. Do NOT re-run autoid expecting more game clusters.

**Report-staleness gotcha (cost a false-regression scare):** after a resplit
that adds splits + target-map entries, the FIRST ninja's report.json can score
some units against PRE-renamer target objs -> phantom single-instruction
100%-flips in UNRELATED units (saw Character -5 / PanelDir +2 / UIPanel +1;
MCP run_objdiff said 100.0 raw all along). ROOT CAUSE (confirmed 2026-07-10
landing a7b9117): report.json can regenerate BEFORE the renamer patches the
target obj within the same ninja run, and a SECOND ninja does NOT fix it —
target objs are not inputs of the report edge, so it never re-dirties. Fix:
`rm build/45410914/report.json && ninja build/45410914/report.json`.
Clean A/B protocol: build baseline in a fresh worktree at HEAD, compare
per-unit at-100 tallies. (Matchmaker close-out landed +15 reveals a7b9117,
25/59->40/59; remaining 19 = handler-macro local-static-Symbol wall +
static-config ??__E machinery — documented in the commit message.)

[[project_crack_farm_saturation_2026-07-09]] [[project_closeout3_2026-07-09]]

<!-- ======== END memory file: project_wave2_e1_autoid_2026-07-09.md ======== -->


<!-- ======== BEGIN memory file: project_closeout3_2026-07-09.md (5125 bytes) ======== -->

## Archived memory: `project_closeout3_2026-07-09.md`

---
name: project-closeout3
description: "Wave-3 close-out + wired-unpinned TU audit (2026-07-09): +166 landed (11387->11553); 99.9x fn_ band = EH funclet MIRAGE not Symbol thunks; pin audit NOT exhausted (+146); new layout-wall catalog"
metadata: 
  node_type: memory
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-3 close-out + pin wrap-up — 2026-07-09 (workflow wf_a440d61d-9ff)

**Landed: +166 matched (11,387 → 11,553 = 17.61%), 0 regressions, 2 commits:**
- `b134066` pin: +146 strict from 17 splits.txt pins (67-line splits diff only)
- `5c961cc` decomp: +20 from grind close-out (11 fns to TRUE 100 across g4/g6/g7/g8)

15 agents (8 Opus matchers, 5 scouts, pin-apply, merger), ~2.1M subagent tokens, ~36 min.
Full per-agent journal: session `a58f6e37`/subagents/workflows/wf_a440d61d-9ff/journal.jsonl.

## ⚠ CRITICAL CORRECTION: the 99.9x fn_ band is an EH-funclet MIRAGE

The ~200+ anonymous 40–44 B `fn_8XXXXXXX` entries at 99.9% in game TUs
(VocalPlayer 32, Player 19, NetSync 18, Campaign 15, EditSetlistPanel 14,
NetGameMsgs 10, PrefabMgr/StoreMenuPanel 7 ea) are **C++ EH unwind/destructor
funclets, NOT local-static-Symbol guard/??__E thunks**. Their sole mismatch is
the parent-frame offset baked into `subi r31,r12,IMM` — they close ONLY when
their parent function body is byte-exact. 4 of 8 matcher groups (g1/g2/g3/g5)
returned zero for this reason. **Do not build worklists that count these as
independently workable**; filter them (prologue `subi r31,r12` = funclet) and
route effort to the parents instead. (The [[project-grind-loop]] wave-2
"guard-thunk cascade" lever is still real, but only for genuine `??__E`/`??_B`/
`??__F` thunks — verify which kind before scoping.)

Also (g7): `report.json matched_functions` counts `match_percent_normalized==100`,
not fuzzy — several "near-misses" in fuzzy-sorted pools are ALREADY counted.
Check normalized before assigning.

## Pin audit: the wired-unpinned vein was NOT exhausted (+146)

RFC-04 (2026-07-08) probed the *oracle-ranker* pool and declared exhaustion —
but the **objects.json-wired-yet-unpinned set (42 TUs) was a different pool**,
audited to definitive dispositions this wave:
- **17 PINNED (+146):** Cam.cpp +29, BandLabel +18, SongSortBy{Artist 14, Rank 12,
  Song 11, Recent 10, Diff 9, Plays 6}, CamAnim +12, CymbalSelectionProvider +8,
  UploadErrorMgr +6, Lit_NG +3, AccomplishmentConditional +3, HttpGet +2,
  Decibels/Primes/StreamChecksum +1 ea.
- **Mechanism note:** a splits-only .text pin registers matches even with no
  objects.json entry (tools/project.py compiles tree source) — Decibels proved it.
- **Rejected (pinned clean but 0 byte-match → need source port):** Sort.cpp,
  Interp.cpp (15-fn span 0x824E2E10–0x824E3510 verified via RTTI/vtables, fills
  the post-Key.cpp gap), TimedSignal (retail is virtual w/ vtable 0x821857cc;
  our port is non-virtual).
- **OVERLAP repair opportunity:** vec.cpp's 2 ScaleAddEq fns (0x823F84B8/0x823F8558)
  sit inside MeshDeform.cpp's pin leading edge — re-carve MeshDeform start to
  0x823F85D0 + pin vec.cpp for +2. Also RateTransposer, AccomplishmentLesson*Conditional x2.
- **ABSENT (10, close permanently):** Easing, DoubleExponentialSmoother (DC3-only),
  MessageTimer, ErrorNode, HamAudio, HamPlayerData, RhythmDetectorGroup,
  BaseSkeleton, DirectionGestureFilter, WiiBufStreamMgr (Wii-only).
- **PHANTOM:** PeriodicJob, FontBase (+ known FlowNode/TexProc). **SCATTERED:**
  QuestJournal, ClipDistMap. **UNKNOWN:** PhraseList.

## New wall catalog (multi-fn unlock targets, from matcher root-causing)

- **Global STLport `_Rb_tree` size 0x18 (ours) vs 0x1c (retail)** — shifts every
  post-map member in classes with map members (Campaign, SongUpgradeMgr's
  set-vs-map, many funclet `addi` deltas). Systemic header-level fix; would
  cascade widely; needs whole-binary A/B. Biggest single lever found.
- **TrackDir vtable +2 slots** (retail GetChordMesh 0x5c vs ours 0x64) — blocks
  Gem::AddChordInstance 908B, GemTrack::ApplyShiftImmediately, OverrideRangeShift.
- **Stream vtable: SetJump slot -12** (overload reorder) — blocks MetaMusic::Start,
  StandardStream::Resync, StreamPlayer::Init.
- **PanelDir vtable +4** (SetFocusComponent 0x40 vs 0x3c) — blocks UIPanel fns.
- **Game class layout non-uniform +4s** (mProperties.mAllowOverdrivePhrases
  0x2f vs 0x33, mEndWithSong, mLastPollMs, mDisablePauseMs) — blocks SongDB::
  GetPhraseExtents/GetCommonPhraseID, Game::CanUserPause/HandleAudioLoad.
- **BandProfile 4 bytes too big** (mTourBand 0x7c80 vs 0x7c7c).

## Grind closes this wave (levers reconfirmed)

g4 (+2): RefreshSetlistArt (retail constant NetLoaderPos 0→1), GetAlbumArtPath
(retail SongFilePath is 2-arg, dropped bool param). g6 (+13): SetupRealGuitar-
ImportantStrings (local-static Symbol conversion — the real thunk lever, works),
NextPhraseIndexAfter, AdvanceEnd. g7 (+0 net, IsProfileOwner closed). g8 (+5):
RndParticleSys::Save (SAVE_REVS 0x29→0x25 — retail rev lower than DC3's),
SongMetadata::Load (decl-order swap), Song::Pause, UIPanel CheckIsLoaded/CheckUnload.

<!-- ======== END memory file: project_closeout3_2026-07-09.md ======== -->


<!-- ======== BEGIN memory file: project_wave4_2026-07-09.md (5181 bytes) ======== -->

## Archived memory: `project_wave4_2026-07-09.md`

---
name: project-wave4
description: "Wave-4 walls+pins+grind (2026-07-09): +39 landed (11583->11622); CollideListSubParts de-virtualization keystone; Stream SetLoop rename; _Rb_tree ODR /DRB3_MAP_0x1C per-TU gate; Interp +15; funclet tooling (16,814 tagged)"
metadata: 
  node_type: memory
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-4: wall-crackers + pin follow-ups + grind — 2026-07-09 (wf_c5b31ef8-dee)

**Landed: +39 (11,583 → 11,622 = 17.71%), all 9 patches kept, 0 regressions, 0 skips.**
- `a239410` decomp: close-out wave 4 — 22 fns (grind + Interp pin + vec re-carve)
- `e2a47f1` decomp: class-layout/vtable wall fixes (TrackDir/Stream/BandProfile/Campaign)
- `634b360` tools(grind): funclet classifier + normalized-aware worklist generator

12 agents (~1.7M tokens, ~45 min). Run was interrupted once mid-flight (harness exit);
resumed cleanly with `resumeFromRunId` after removing the 4 incomplete agents' stale
worktrees+branches — completed agents replayed from cache. Follows [[project-closeout3]].

## Durable root causes (cross-cutting, apply beyond this wave)

- **`RndDir::CollideListSubParts` is NON-virtual in retail RB3-360.** DC3 (our
  src provenance) promoted it to virtual, inserting a bogus slot into EVERY
  RndDir-descendant vtable (shifted TrackDir, PanelDir, GemTrackDir tails +1).
  De-virtualizing it + moving `TrackDir::SyncFingerFeedback` to the END of the
  virtual block (retail slot 35, not where the rb3-Wii dev header puts it)
  closed 6 fns incl. all 3 TrackDir-blocked targets (AddChordInstance 908B).
  EXCEPTION: `Character::CollideListSubParts` stays virtual (Character-owned
  slot 11; retail has a compensating difference — removing it regressed 5 fns).
  Lesson: DC3-provenance virtualization diffs are a recurring wall class —
  check `virtual` keyword drift vs rb3-Wii AND retail vtable when tails shift ±1.
- **Stream wall = MSVC same-name overload grouping.** MSVC emits same-named
  virtual overloads as ONE group at the first declaration's slot (reverse decl
  order); separating declarations does NOT break grouping — only renaming does.
  Retail names the string variant `SetLoop` (distinct from `SetJump(float,…)`)
  and keeps an `AbandonLoop` slot DC3 dropped. Rename fixed MetaMusic::Start /
  StandardStream::Resync / StreamPlayer::Init.
- **`_Rb_tree` 0x18-vs-0x1c is a per-TU ODR split, CONFIRMED AGAIN** (matches
  [[project_rbtree_4byte_deficit]]): retail has sizeof(map/multimap)==0x1c
  (dead word after _M_key_compare) while sizeof(set)==0x18 in the SAME binary.
  New refinement: **`/DRB3_MAP_0x1C` gate pads only the std::map wrapper**
  (leaves _Rb_tree/set at 0x18) — Campaign opt-in gave +6 via map-dtor-funclet
  cascade. Candidate sweep: other map-heavy TUs w/ funclet walls (wave-3 catalog).
- **BandProfile fixed:** phantom Wii-era `int unk748` (0 refs) made the class
  +4 oversized; removed → GetBandLogoTex closed.
- **Game wall = 3 COUPLED deltas, still open:** (1) retail drops the
  DiscErrorMgrWii::Callback base (+4 base shift, retail base 0x2c); (2) extra
  +4 member ~[0xbc,0xcc]; (3) tail: ATanInterpolator 0x28 ours vs 0x3c retail.
  NOTE: (3) may now be RESOLVED — p1's Interp.cpp/h port (same wave, landed
  after this analysis) redefined the interpolators to retail layout. RE-CHECK
  the Game wall with only deltas (1)+(2); it may have become closeable.
  Blocked fns: CanUserPause, IsLoaded, HandleAudioLoad, SetGameOver, FillComplete.
- **PanelDir wave-3 wall was STALE** — already fixed on main before the wave
  (the mFlows/ObjectDir-vbase work). Lesson: re-verify a catalogued wall's
  symptom on CURRENT main before dispatching an agent at it.

## Pins

- **Interp.cpp: 15/15 fns closed (+15)** — port landed on the wave-3-verified
  span 0x824E2E10–0x824E3510; the wave-3 "pinned clean but 0 match" verdict
  just needed the source port.
- **Sort.cpp HashString = dtk-fragmentation wall:** dtk/jeff splits the single
  0x50-byte fn into 4 `.fn` pieces at PPC integer-div trap guards (twllei/twi).
  Fix belongs in ../jeff (xex.rs), not source. Pin removed pending that.
- **vec.cpp Matrix3::ScaleAddEq closed (+1)** via MeshDeform re-carve.

## Measurement tooling (the #2 fix) — now canonical

- `scripts/grind/worklist.py` — vetted-pool generator; filters normalized>=100
  (report.json counts NORMALIZED, not fuzzy), fn_*, DB eh_funclet pattern,
  at_limit/complete/excluded. Current residual pool: 64 game / 388 engine.
- `scripts/grind/classify_funclets.py` — tagged **16,814 EH funclets** in
  decomp.db (primary_pattern='eh_funclet') by subi-r31,r12-prologue scan of
  target asm. Future waves MUST build pools through worklist.py.
- Doctrine section appended to docs/plans/grind-loop-calibration-2026-07-07.md.

## Merge notes

Merger resolved a real 3-way conflict in scripts/target_symbol_map.json
(concurrent 6249080 added GuitarFx entries; kept both sets). Apply order
low→high blast radius worked; stlport-adjacent patch last. w3-stream's
predicted delta was wrong at merge time (working-tree state drift between
agent A/B and merge) — merger's own A/B is the real gate, keep trusting it.

<!-- ======== END memory file: project_wave4_2026-07-09.md ======== -->


<!-- ======== BEGIN memory file: project_wave5_2026-07-10.md (4755 bytes) ======== -->

## Archived memory: `project_wave5_2026-07-10.md`

---
name: project-wave5
description: "Wave-5 (2026-07-10): +24 landed (11622->11646 wave + w3 late-land, main 11661 w/ concurrent); GAME WALL CRACKED (DiscErrorMgrWii base drop + bool-run reorder); symbols.txt is TU5-stale = new fleet-wide vein; PCH worktree-seeding infra bug"
metadata: 
  node_type: memory
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-5: Game-wall + map-gate + grind — 2026-07-10 (wf_55cc2d2a-b37)

**Landed: +23 via merger (11,622 → 11,645, 7 commits be349a7..b268881) + w3 late-land
+1 (b31eaaf). Main verified 11,661 (17.77%) — extra +15 = concurrent Matchmaker work
swept in by resplit, 0 regressions anywhere.** 11 agents, ~1.6M tokens, ~27 min.
Follows [[project-wave4]].

## THE GAME WALL IS CRACKED (was the top blocker)

Two coupled fixes (w1+g3, landed 3edcc60 + e0633ea):
1. **Dropped `DiscErrorMgrWii::Callback` base from Game** — Wii-only polymorphic base
   (vptr retail Xbox lacks); removed base + DiscErrorEnd override + ctor/dtor
   AddCallback/RemoveCallback. Shifts mProperties 0x30→0x2c AND absorbs the phantom
   Timer-region +4 (two of three +4 deltas from one root cause). +5 alone.
2. **Bool-run packing reorder**: mMusicSpeed moved ahead of the mMuckWithPitch/
   mNeverAllowInput bool group → run packs to 20 bytes, mLoadState lands 0xcc (was 0xd0).
   Closed CanUserPause.
Wave-4 delta (3) (ATanInterpolator) was indeed already fixed by the Interp port.
Closed: GetPhraseExtents, GetCommonPhraseID, CanFlail, IsSpotlightGem, LoseGame,
CanUserPause + funclets. **Remaining Game-adjacent walls (all layout deltas now gone):**
pre-bool 2-byte packing (bool-run starts 0x72 retail vs 0x74 — SongPos/mAllActivePlayers
vector 2-byte diff), mMaster->IsLoaded() DE-VIRT (retail statically resolves
HxMaster::IsLoaded via direct bl; needs BeatMaster/HxMaster base-order/final-override),
SetGameOver FP scheduling (permuter-class), GemPlayer::FillComplete body-port.

## NEW VEIN: symbols.txt is stale (generated from default_plus_TU5.xex)

w3 disproved the "jeff trap-fragmentation bug" — jeff/dtk is CORRECT (traps are
fall-through; powerpc crate is_branch false for Tw/Twi). The real cause of Sort.cpp
HashString reading 0%: **config/45410914/symbols.txt was emitted against
default_plus_TU5.xex** (per config.yml comment) and carries stale fn_ boundaries +
spurious except_data_/except_record_ clusters that don't exist in retail pdata. dtk
honors symbols.txt as authoritative → carves real functions into COMDAT fragments.
Fix pattern (b31eaaf): merge fn_ fragments to true size, delete spurious
except_data/record pair, pin. **18,098 except_data_ symbols total; unknown subset
spurious.** Sweep recipe: for except_data whose word1 doesn't resolve to a code-section
handler (jeff genuine_except_data_set, util/xex.rs:1268), re-verify region, merge
boundaries. Prioritize regions inside EXISTING pinned splits (only those affect match%).

## INFRA BUG: worktree PCH seeding bakes main's absolute paths

setup_worktree.sh seeds main's system.pch; any edit invalidating a PCH-eligible TU's
objcache key forces a real worktree recompile where include paths != PCH's baked main
paths → #pragma once identity fails → mass header redefinition (~34 hamobj/os/obj/flow/
gesture/utl TUs). Workaround g6 used: PCH-off A/B in worktree. Real fix: rebuild PCH
in worktree (or don't seed .pch). NOT YET FIXED.

## Other durable root causes

- **CharServoBone: DC3 dropped `mMe` member** that rb3-Wii AND retail have (Character*
  at 0x9c) — one-line add closed Character::Teleport; RegulateInternal corroborates.
  DC3-deletion drift = sibling of DC3-virtualization drift.
- **/DRB3_MAP_0x1C sweep result: mostly a NO-OP vein** — 10 of 12 candidate TUs
  unchanged; winners: TourProgress (+3), CustomizePanel/CharacterCreatorPanel (+4 via
  g4). Note: PCH-eligible TU + extra_cflags are incompatible (gate forces non-PCH rule).
- **Mic vtable slot drift**: our DC3-mirrored Mic has one extra virtual before GetName
  (retail 0x8c vs ours 0x90; DC3 AND rb3-Wii both 0x90!) — engine-wide Mic.h fix, needs
  full A/B. Blocks UpdateMultiMicDeviceSliders残 + MicXbox::ClearBuffers.
- **BandProfile leading +4** (mCharacters at 0x24 retail vs 0x28) — separate from the
  fixed unk748 tail issue. **EntityUploader mSubmittedTime +1** (needs Ghidra ctor read).
- **BaseMaterial member-reorder** (retail ZMode early/UseEnviron late; DC3 inverted) —
  rndobj-wide; **Debug sizeof 0x100 vs 0x144**; **Splash NgRnd slot drift + Rnd +100B**;
  **CameraManager mCurrentShot 0x28 vs 0x58** (local, tractable).
- Merger conflict lesson: g3 and w1 both dropped the Game base — merger resolved by
  keeping first-landed + taking second's novel fields only. Worked cleanly.

<!-- ======== END memory file: project_wave5_2026-07-10.md ======== -->


<!-- ======== BEGIN memory file: project_wave5_lsr_2026-07-10.md (71174 bytes) ======== -->

## Archived memory: `project_wave5_lsr_2026-07-10.md`

---
name: project_wave5_lsr_2026-07-10
description: "[TU0-ERA — ALL ADDRESSES INVALID, main flipped to TU5 on 2026-07-15; levers still valid] MEGA-SESSION 2026-07-10: main 11645→15283. Levers: HANDLE local-static +498, diamond-vbase, warn-arg-eval, dtor vtable-store elision (ObjPtr template), ??_E weak-alias, AddSink 4-arg, MakeString 0x800, OBJ_SET_TYPE_ENGINE, Yoda-compare, temp-lifetime split. Recarve playbook: every misaligned unit = pin hole/foreign tail. Timer=0x30/Object=0x28 X360."
metadata: 
  node_type: memory
  type: project
  originSessionId: fe599317-6faf-4a30-a027-2f05b4f041c9
---

> ## ⛔ STALE-ADDRESS WARNING (added 2026-07-25)
> **Every retail address in this file is a TU0 address and is INVALID against current main.**
> Main retargeted **TU0 → TU5 on 2026-07-15** (`config/45410914/config.yml`; runbook
> `docs/plans/tu5-landing-runbook.md`). Verified 2026-07-25: TU0-era addresses from this
> session (0x825A8540, 0x82567C68, 0x825BED40, 0x824E4AF0 …) return **ZERO hits** in
> `scripts/target_symbol_map.json`; their TU5 equivalents differ (e.g. ~RockCentral is
> **0x824F7AC0** on TU5, not 0x824E4AF0). A lane lost a full run reading a TU0 Ghidra image
> while the tree targeted TU5 — its "couldn't find the ctor" was purely this.
> **Rules:** use the TU5 Ghidra bank `default_tu5.xex-c5a170` (port 8002); never load the live
> default.xex; cross-check any address against target_symbol_map.json before trusting it.
> The **levers, idioms, and class/layout findings below remain valid** — only the ADDRESSES rotted.
> Also: this file's match counts (14,450→15,428) are TU0-era; main is far beyond them.


2026-07-10 session wave (continuation of [[project_wave2_e1_autoid_2026-07-09]]).
Session landed +43 on main: 11,645 → 11,696 (interleaved with concurrent
agents' +8 StandardStream/JoypadData b5b28fe).

**THE BIG LEVER — local-static-Symbol HANDLE macros (1bb7ca0, +11).**
Retail constructs handler-dispatch Symbols as FUNCTION-LOCAL STATICS: MSVC
/O1 guard-bit pattern (guard word in TU static data, one bit per handler in
source order, inline Symbol ctor + ??__E init + ??__F atexit funclets). Our
DC3-era ObjMacros referenced global Symbols2/3/4.h — this was the wall behind
EVERY game-TU Handle/OnMsg near-miss (the "guard-thunk wall"). DC3's own
Object.h uses _NEW_STATIC_SYMBOL (same construct). Fix: gated macro variant
in src/system/obj/ObjMacros.h — under /DRB3_HANDLE_LOCAL_STATIC the 5
symbol-taking HANDLE macros expand to `static Symbol _hs(#symbol)` + compare.
Per-TU opt-in via objects.json extra_cflags (same as /DRB3_MAP_0x1C).
**ROLLOUT COMPLETE (same day): lever total +104 across 9 TUs** — wave A
1136c7e +72 (EditSetlistPanel +45!, SavedSetlist +4, PatchSelectPanel +11,
StoreMenuPanel +12), wave B 6b66bfa +21 (AppMiniLeaderboardDisplay +5,
VoiceoverPanel +4, SongSelectPanel +7, PrefabMgr +5). Main at 11,789.
Large Handle bodies land high-90s not 100 (residual body divergence) — the
yield is the static-init/??__F machinery flipping. NEGATIVE: char SYNC_PROP
near-misses are NOT this wall — their symbol compares already match; tail
divergence is SYNC_SUPERCLASS inlining (target inlines Object::SyncProperty
to li r3,0 vs our out-of-line bl). Don't apply the lever to char.
FOLLOW-UP LEADS: SessionUsersProviders::Handle @14.6 pre-lever — retry with
gate (DONE: gate +2 e70b8a3, body genuinely divergent, paired for grind);
remaining Handle bodies at 93-99 = permuter/grind finish candidates.

**FLEET SWEEP (same day, 8774d3e): +390 across 25 MORE TUs → LEVER TOTAL
+498 on 35 TUs. Main at 12,210.** Systematic scan of all 68 wired non-gated
game TUs with HANDLE macros: 34 signature-confirmed, 25 KEEP (Campaign +51
stacked with MAP_0x1C, Player +49, VocalPlayer +48, AccomplishmentManager
+39, BandSongMgr +30, AppLabel +24, ...), 9 REVERTED net-0 — **DIAGNOSED, KILL for the
lever (do NOT retry)**: their sub-100 guard funclets are owned by NON-HANDLE
function-local statics (RockCentral = 20 DP_KEYS/ADD_DATA_POINT RPC statics;
GuitarFx's mismatched funclet exists lever-off) → guard-BIT-INDEX shift
(MSVC assigns guard-word bits in TU-wide declaration order; our static
inventory ≠ retail) + FRAME-SIZE block (??__F thunk frame tracks its
UNPORTED parent's frame, 0x60 vs 0x80/0xa0). RockCentral "50 sigs" was a
mirage (RPC-owned, not Handle-owned); its Handle is UNPINNED entirely, as is
OvershellSlot's. Correct lever for these = body-port the parent RPC methods
(fixes inventory + frames as byproduct). **TESTED (d259a7b): premise did
NOT hold** — RockCentral's RPC bodies were ALREADY oracle-identical; the
one real stub (DataPointToQString) ported 0.3→90.6 fuzzy (uses
src/network/net/JsonUtils.h BUILDER api, not the dc3 reader; old stub
comment was wrong) but 0 strict, 0 funclets moved. RockCentral tail =
at_limit regalloc/static-init-order cascades + Get* accessor family is
oracle-VA MISATTRIBUTION (don't grind). RockCentral is effectively CLOSED
as a vein. CORRECTED LEVER MODEL: the +498 was
mostly guard-bit REALIGNMENT flipping neighboring ??__E/??__F funclets to
100 (most big winners have no pinned Handle at all), not Handle-body wins. 34 no-sig rejected
(Handle body unpinned). **PIN-EXTENSION FOLLOW-UP (b56b1d4): 7 Handle
bodies located by Ghidra action-string xref (all ADJACENT to TU spans —
NOT scattered) and pinned+gated: Performer+BandUser = 100 (+2 strict);
kept fuzzy pins GemPlayer 99.9 (at_limit this-respill), Tour 95.7
(r25/r26+init-reorder), CustomizePanel + Game (CLOSED to 98.9/99.7 at
ebdeb40 — both root causes were RETAIL-STRIPPED DEV/CHEAT HANDLERS our
dev-oracle source retains, NOT the diagnosed layout/body-port issues:
check insert DIRECTION on one-sided Handle insert clusters; new per-TU
gate RB3_STRIP_CHEAT_HANDLERS; also Game.h mNeverAllowInput/mMuckWithPitch
bool swap + CustomizePanel InClothingState contiguous-range form),
MetaPerformer + Tour RECHECKED (597a144): stripped-handler theory
DISPROVED for both — real cause was 0xc LAYOUT deltas, fixed byte-exact
(Tour = 3 std::maps -> /DRB3_MAP_0x1C gate; MetaPerformer = Wii-only
mWiiPending/mLastVenue drop gated /DRB3_NO_WII_META_MEMBERS + /vd0 for a
spurious vtordisp — ctor now addi 0x384 == retail). Residuals at_limit:
Tour NRVO/inline-cost, MetaPerformer r28/r29 cascade over 2049-instr chain
(permuter-class, layout prerequisite now in place). LESSON: apparent
"insert blocks" in big Handle diffs are often MISALIGNMENT CASCADES from a
small layout delta — check for uniform off:+N before either the
stripped-handler or body-port theories. ProfileMgr REVERTED (structural
+ DUPLICATE objdiff unit default/ProfileMgr vs default/band3/meta_band/
ProfileMgr needs cleanup first). Remaining ~26 no-sig TUs = same recipe if
their Handle VAs get located.**
**BATCH 2 (4314a53): +16 strict, 25/26 pinned, vein now DRAINED.** 16 new
Handle@100 + 9 fuzzy pins (TourPerformerImpl 99.998 single +4 offset =
near-close lead; ProfileMgr Handle 75.6→99.8 after dup-unit fix + 19
retail-stripped Wii-only handlers + full Wii-member drop, sizeof 0xc0).
REUSABLE ASSET: ~/tmp/pdata_table.json — decoded retail .pdata function
table (56,665 entries), makes Handle/pin location O(seconds); regenerate
if lost (agent script decoded it from the flat .pdata section).
FOLLOW-UPS: PhysicsVolume over-pin AUDITED+RECARVED (ba32289, +76!): the
27KB pin owned ZERO PhysicsVolume code — was AccomplishmentPanel (119@100)
+ NEW AppInlineHelp.cpp (15@100) back-to-back; old ??_GPhysicsVolume map
entry was a vector-deleting-dtor MIRAGE (byte-identical across classes);
real PhysicsVolume ~0x8277xxxx UNPINNED (lead); BinDiff oracle useless
inside a bad pin. TourPerformerImpl::Handle closed to 100 (MAP_0x1C on
TourPerformer.cpp — TourPropertyCollection map before mCurrentQuest).
LESSON: mirage-match classes (vector-deleting dtors, ICF'd thunks) can
validate a WRONG pin for a long time — an over-pin audit tool sweeping
suspicious big spans (unit source size vs span size ratio) is high-EV.
**OVER-PIN SWEEP EXECUTED (922ce2c, +143): main 12,576.** Ranked all 797
pinned TUs by span/obj-text ratio (~/tmp/overpin_table.txt + rank tools);
3 recarves landed: keygen→ByteGrinder +58, StreamNull→Sfx +70 (Sfx was
ALREADY PORTED but starved by the over-pin), UIGuide 4-way +15.
DISCRIMINATOR: high ratio = either STUB SOURCE (pin fine, port is the
work: PitchShiftEffect 923x, FFT, sslgen, Compress) or TRUE misattribution
(tiny oracle + many target fns: AsyncFileHolmes 212fns/44lines).
BIG DOCUMENTED LEADS: (1) BandUI WIRED + AsyncFileHolmes RECARVED
(e2d1c2b, +69): BandUI 72/182, AsyncFileHolmes honest 3/3; PAIRING WAVE DONE (4a5378c, +16, unit 88/182 —
content-pairing pipeline ~/tmp/pair_bandui3.py: /FAsc obj streams vs
target fn_ bodies, normalized-imm + PE-string resolution — REUSABLE for
any oracle-blind span). DIAMOND WALL ROOT-CAUSED (study, no patch): +0x4C = TWO bugs — (A)
UIManager carries 4 RETAIL-STRIPPED DEBUG members mLoadTimer/mOverlay/
mAutomator/mShowDevMenu = +0x40 (retail UIManager ends 0x80, vbase 0x84;
proven from retail ctor Function_827DF040 which never touches 0x80+);
INVISIBLE to the 29 validated UI fns because standalone UIManager
addresses vbase-RELATIVE (bloat cancels; only decouples in the diamond) —
explains UIManager::Handle stuck @35 and Init @87.6 too; (B) BandUI-own
+0xc reorder (mVignetteOverlay/mUIOverlay/mInviteAccepted inserted before
mDisbandStatus). MsgSource INNOCENT (0x18 byte-identical). FIX RECIPE
(2-part, must land TOGETHER — UIManager-only strip = 0 BandUI wins): gate
the 4 debug members + their UI.cpp code paths (ctor inits, Init@949,
Poll@806-814, Handle@987-990, RELEASE@356) behind matching-off macro (keep
HX_NATIVE) + reconstruct [0x7b,0x80) + reorder BandUI-own block. GATE:
zero regressions across 29 default/UI fns + native compiles. ONLY BandUI
hits this wall (sole UIManager derivative); other MsgSource diamonds
(GamePanel/OvershellPanel/...) have different first-base issues (UIPanel
+4, see docs/plans/ui-base-layout-reconstruction.md). Body leads: Poll 86.1, Handle 73.7,
GetCurrentFlowType 49.3. BandMachine/MetaNetMsgs tail 0x82527278+ still
needs true-end location; (2) XAPO stack PORTED (ecdc8dd, +8 direct): xaudio2.h converged to DC3
deferral structure PROVEN byte-neutral across 40 consumers; 4 effect TUs
in; STUB-PORT WAVE LANDED (224f68b, +36): ExternalMic 2→6, PitchCorrectedVoice
1→4 (ctor "0%" was a dtk MIS-SPLIT — symbols.txt size fix), PitchDetector
9→18, FxSend wrappers +20 (real RB3-vs-DC3 Params deltas: EQ 0x30v0x38,
Wah 0x2Cv0x28, Compression 9-float ctor). Remaining: EnvelopeGenerator
recarve, Mic/FxSendDistortion recarve deferred until the mWav quirk (RB3
this+0x45 vs DC3 0x4e, caps Process/LockForProcess @99.9) is solved,
ExternalMicClientMgr/Proxy web ~12 fns NO-oracle (Ghidra reconstruction
backlog), EQEffect full-class layout (new size imm +0x240); (3) UIComponent
base-layout ±8 wall blocks ~20 Label* methods at 99.4+ (cross-cutting);
(4) ratio leads AUDITED batch-2 (f88d1bb, +34): 7/8 HEALTHY (ExternalMic/
PitchCorrectedVoice/PitchDetector = stub-source w/ DC3 oracles → PORT
vein; MemHeap/Env_NG genuine codegen walls); GuitarController 18KB block
was 100% foreign → recarved, TrackWatcher 5→27 (2 local-static-Symbol
body-ports; stale Kinect mirage map entry replaced by TrackWatcher::Poll).
OVER-PIN VEIN NOW DRAINED of ranked leads. GAMEPORT WAVE LANDED (6652ec9, +45): all 7 TUs wired incl. DrumPlayer.cpp
(unknown span identified, 9/9 byte-exact). Port lesson: MWCC fall-off-end
fns need explicit return under MSVC. dtk under-split artifact class found
(tiny funcs fused in one .fn block — can't pair individually). Banked
near-misses: AreSlotsInRoll 97.4, HandleHitsAndMisses 98.3, CheckForTrills
89.5, CheckForHopoTimeout 99.7 (single lfsx swap = permuter candidate).
Remaining smaller leads: SongSortNode 8-class cluster; SetlistRecord 86.5
+ ClosetMgr 94.4 body-ports; permuter-class Handle residuals.
ENUMERATION TRAP: target .s files live in BOTH flat (asm/Foo.s) AND
mirrored-subdir (asm/band3/game/Foo.s) paths — flat-only scans
false-negative the biggest units. Guard-sig test = guard word lwz+stw to
same label + nearby bl (only the FIRST static tests clrlwi.,31).
AppMiniLeaderboardDisplay::Handle @99.21 = bool zero-extension artifact on
virtual bool param, not source-fixable.
Matchmaker testbed: Handle bodies 83.4/71.8 → 100, guard funclet 92.5→100,
5x ??__F 99.9→100 (TU static-data layout snapped), 3 ??__E thunks 0→100.
CASCADE INSIGHT: the 99.9 ??__F "layout mirage" funclets are NOT independent
— they flip when the TU's missing local statics are added. Rollout to 8
confirmed-signature TUs dispatched (EditSetlistPanel 60.8, LocalSavedSetlist
64.0, PatchSelectPanel 65.3, StoreMenuPanel 68.8, AppMiniLeaderboardDisplay
71.3, VoiceoverPanel 71.8, SongSelectPanel 72.3, PrefabMgr 79.2; ~45
layout-blocked funclets ride along, est +40-90). Char SYNC_PROP variants
(CharSleeve 94.0, CharIKFingers 94.6, CharBoneOffset 82.9, CharLookAt 81.4)
= stretch lead. Risk: stringization assumes identifier==wire-name (Milo
convention); mismatch = partial, never regression.

**Matchmaker close-out (a7b9117, +15):** all 15 were PAIRING REVEALS
(target_symbol_map.json only — our compiled obj already matched; triage by
COFF call-fingerprint matching between target and base objs). Unit 25/59 →
40/59 → 51/59 after the lever. Remaining 8: static-config block
lbl_82DA0017 ctor/method divergence (??0BandMatchmaker 66, StartSearch 68,
UpdateMatchmakingSettings 64, ??1MatchmakingSettings 76) + vtordisp/init
machinery.

**SessionUsersProviders wired (a09d880, +17):** .text 0x826375B8-0x82637F48
(adjacent to Matchmaker), 17/24 at 100. Identity triple-confirmed (strings/
.pdata/Ghidra). KickPlayerMsg is the anon-namespace NetMessage INSIDE this
TU, not a separate file. Port deltas: GetMachineID()→mMachineID,
Find<RndMat>(name,false)→true (retail-360 li r5,1 — Wii oracle's false was
platform divergence), decl-only IsGuestOnlineID added to PlatformMgr.h.
Deferred: Handle 14.6 (pre-lever — RETRY WITH THE LEVER), RefreshUserList
22.9, scalar dtor 42.9. MakeupProvider is at 0x826530F0 (NOT adjacent),
single-fn, deferred.

**GuitarFx::Poll = AT_LIMIT @72.4 normalized (DB-recorded, no commit).**
Root cause fully traced: retail materializes 5 FxSend-property Symbols as
full pointers (6 GPRs) vs our lis/lwz (5) → retail saves from r14 (18
callee-saved) vs our r21, 3 callee-saved FPRs vs our 4, whole-fn register
renumbering + frame -0x30 + 4 target-only EH funclets. NOT source-
controllable (needs RAISED pressure); permuter corroborated (0 applicable
candidates). Span audit: 8 small fns after Poll = its OWN EH funclets (4x
99.9 = single-bl DataNode-dtor reloc mirage); 3 tail fns 0x826D7828/38/50 =
FOREIGN section-tracker class — honest span end 0x826D7828 (split-fix
documented, not applied, 0 delta). DON'T re-grind Poll.

**Tooling root-cause (report-edge staleness):** report.json can regen BEFORE
the renamer patches target objs in the same ninja run, and a second ninja
does NOT fix it — target objs are not report-edge inputs. Reliable sequence
after any map/splits change: `rm build/45410914/report.json &&
./tools/ninja-locked build/45410914/report.json`.

**decomp-synth housekeeping:** batch_unit_climber E1b fixes LANDED f1b66ca
(fuzzy-over-report caveat in commit msg). EVAL_MODE=saturate plan fully
implemented (2ca107d) + B2 companions verified: saturator.py current;
eval_sidecar.sh on B2 was STALE (missing fe3f0e0 rclone-.deb bootstrap) —
re-pushed, now byte-matches repo. Live saturate verification still pending
next training-box launch. Keyless-B2 design committed 2ae9678 (owner-gated).

**Warm-worktree PCH trap (bit two agents):** seeded system.pch can carry
main-absolute header paths → /Yu re-parse → Symbol.h redefinition cascade
across ~150 PCH TUs. Fix: `rm build/45410914/pch/system.pch*` in the
worktree and rebuild. Consider fixing in setup_worktree.sh.

**Diamond-vbase wall: FIXED and LANDED 9a198eb (+9 → 12808).** Both parts as
diagnosed: (A) UIManager 4 debug members gated behind RB3_UI_DEBUG_MEMBERS
(HX_NATIVE-only — src/macros.h force-defines MILO_DEBUG tree-wide, so
MILO_DEBUG canNOT be a strip gate for anything, remember this) + reconstruct
[0x7c,0x80) as int mUnk7c so vbase lands 0x84; (B) BandUI own-block reorder
to retail order. **REUSABLE TECHNIQUE — file-scope backing storage:** when
stripping class members whose code paths contain throwing debug code
(MakeString/String temps), a naive strip renumbers TU-wide EH funclet scope
counters and LOSES neighboring funclets (here: Init's 3 cleanup funclets,
verified −3). Fix: `#ifndef GATE` declare same-named file-scope statics so
every debug code path + its EH scopes stay byte-structurally intact while
only the class LAYOUT shrinks. Zero strict losses. UIManager::Handle stays
NonMatching @~35 (retail lacks 5 dev handlers; gating them out improves
Handle to 58.6 but costs the 3 Init funclets — DON'T). Residual leads:
InComponentSelect 98.06 (cmpwi/cmplwi sign on null-check), ??_GBandUI 99.9
(frame Δ+0x10) — permuter-class, banked.

**SetlistRecord+ClosetMgr close-out LANDED 5881f2f (+19 → 12827). NEW TRIAGE
DIRECTION — retail-only Xbox handlers:** insert blocks where TARGET has MORE
are not always our surplus dev handlers; on 360 they can be RETAIL-ONLY
platform features absent from the Wii oracle (Xbox Live gamercard handlers
in SetlistRecord; AssetStore purchase/download handlers in ClosetMgr where
the Wii build stubs them nullptr). Reconstructible from Ghidra decompile +
.rdata handler-name strings + RTTI type descriptors — no oracle needed.
Sweep patterns that recur: retail-stripped MILO_ASSERT null-checks;
HAND-WRITTEN function-local static Symbols in method bodies (same construct
as the HANDLE lever but not macro-driven — grep for Symbols.h globals in
sub-100 fns); !IsInvalid() double-negation; arg-eval-order hoists.
**Pin-extension technique (fast):** once a Handle is at 100, its bl sites
pair target fn_ addresses to our mangled names 1:1 — with
~/tmp/pdata_table.json this makes span extension O(minutes) (17 pins here).
Kills: ClosetMgr::SetUser 99.86 (beq tail-merge past own body — linker
artifact). Hygiene: mirage map 0x8254FFF8→RockCentral::
GetLeaderboardByRankRange removed (really ClosetMgr::FinalizeBodyChanges) —
more RockCentral oracle-VA misattribution fallout.

**SongSort cluster LANDED 4fed3a6 (+118 → 12945)** — biggest single-wave win
of the session. Wired ByStars + ByReview (retail link order ≠ Wii
alphabetical — locate by string+RTTI, never assume order), 52 hand-written
map entries via allocation-size fingerprints (ShortcutNode 0x4C,
Header/Subheader 0x58, StoreSongSortNode 0x48, OwnedSongSortNode 0x44), 2
dtk mis-split fixes (leaf fns with no pdata entry get lbl_ not fn_ — check
symbols.txt when a known fn is invisible). RENAMER GOTCHA: after changing a
map entry for an EXISTING address, must `touch config.yml` (fresh dtk
split) — the renamer cannot re-rename an already-renamed target obj.
Local-static-Symbol placement matters: after MemDoTempAllocations guard +
after preceding locals (top-of-fn placement REGRESSED ByDiff 75.7→58.8).
QUEUED FOLLOW-UPS (rich): gap D 0x826421B8–0x82643AE8 (68 fns incl
CampaignKey), gap E inside SongSortNode pins (52 fns, likely SongSort.cpp +
SongSortNode.cpp), SongSortMgr 81/169, StoreSongSortNode 4/25 (ctor
0x82658820, mToken@0x40 mOffer@0x44), ByArtist NSN(StoreOffer) 31.3 (360
StoreOffer has +0x100 review int / +0x108 metadata ptr beyond Wii layout),
ByRank NSN 98.3 = STLport map<int,pair<int,bool>> node +12 artifact.

**Local-static sweep LANDED 2af1ac9 (+11 → 12974). THREE NEW LEVERS:**
(1) **Phantom guard bit** — retail's stripped MILO_WARN sites still OWNED a
guard-bit index; `if (false) { static Symbol _stripped(""); }` burns the
bit, zero code, realigns later statics (proven Gem::CreateWidgetInstances).
(2) **Arg-evaluating stripped warn** — retail strips warn output but
EVALUATES the args (TickFormat, PathName, MetaPerformer::Current()->Song());
OBJ_SET_TYPE macro MILO_DEBUG arm landed; a GLOBAL MILO_WARN→evaluating
no-op is a candidate fleet lever, needs its own gated A/B (est. multiple
TUs). (3) **Inlined by-value helper** — ctor-temp-copy-pass-address shape =
retail routed through a small inlined helper homing a caller-side Symbol
copy. Also: static-Symbol placement = decl order sets guard-bit order;
retail REDECLARES statics per scope (AddUpgradeData). LANDING LESSON:
overlapping concurrent lanes (g4/g7 body-port agents hit the same fns) —
resolve conflicts keeping MAIN's landed side, let build+report arbitrate;
net came in +11 vs worktree +13. Tooling gap filed: gen_game_target_map
doesn't map 40-byte EH funclets → positional-pairing artifacts in per-fn
A/Bs. Kills DB-recorded: CleanupStringVerify (extra member @0x9c),
SongUpgradeData ctor (target-inconsistent adds), 4 pure-regswap
permuter-class fns.

**Synth recarve LANDED 81410d0 (+29 → 13003).** PitchShiftEffect span was 4
TUs (Gain/HeadsetPlayback/EnvelopeGenerator/PitchShift, all recarved).
**"mWav quirk" KILLED — it never existed:** mWav = 0x40 + 3*sizeof(Params);
lhz 0x45 = 1-byte-Params classes, 0x4E = 4-byte. The 99.9s were WRONG-CLASS
pairing mirages (normalized relocs hide the class) → Mic/FxSendDistortion
recarve now UNBLOCKED; re-check any 99.9 Process/LockForProcess "mWav
offset diff" for the same misattribution. EQEffect decoded: RB3 = older
0x10C EQ engine (DC3 0x34C added crossover+smoothing), member map in commit
msg; SetParameters needed #pragma inline_depth(0). Kills: GainEffect::
DoProcess 84.7 (4-way splat materialization, 8 constructs tried). Banked:
FlangerEffect::SetParameters 73.7 — Params has INT fields @0x10/0x14 vs our
floats, ONE-LINE header fix away; FlangerEffect::Process 54.3; FxSendEQ 2
unpaired fns; EnvGen DoProcess 93.1 permuter candidate.

**Warn lever LANDED 82f8b6f (+6 → 13009), VERDICT: global-on.**
MILO_WARN/NOTIFY/NOTIFY_BETA/LOG now ((void)(__VA_ARGS__)) (the proven
MILO_FAIL form) — retail evaluates warn args. ONCE variants must STAY
non-evaluating (eval loses 3 TexBlender funclets). One per-TU opt-out:
RB3_LOG_NO_EVAL on Game.cpp (LOG-eval splits a shared Symbol temp slot,
defeats retail's cross-jump tail-merge). CONSTRAINT: the opt-out can't work
for PCH TUs (Debug.h #if resolved at PCH-create). Zero losses.

**SongSort gaps D/E LANDED 9ff3161 (+168 → 13177) — biggest wave.** Both gap
premises were WRONG (again: verify by RTTI/content, never by adjacency):
gap D = OvershellSlotState.cpp (61/61), gap E = unpinned middle of
SongSortNode.cpp (108/108), and the pinned "StoreSongSortNode" range was
actually SongSort.cpp (25/25). Mesh.cpp squatter pin evicted zero-loss.
Real StoreSongSortNode is in the UNEXPLORED store TU 0x82657A10–0x82659E60
(118 fns, + likely StoreSongProvider) — QUEUED. SongSortMgr 81/169 needs a
dedicated reconstruction session (no handler macros, large structural fns).
New shapes: retail wrapper bool = `bool ret = expr` not `if(...) ret=true`;
`((b==0)^1)+5` cntlzw idiom; TextForNode retail sig is bool. MILO_FAIL-
stripped leaves hide in pdata holes. FinishSort@HeaderSortNode 99.7
at_limit (RTTI-reloc register rotation, source-immune).

**Store-TU recarve LANDED eab797d (+118 → 13306).** The span was FOUR
owners: LessonProvider true tail (16/16), TrainerProvider new (11/11), real
StoreSongSortNode (28/28), Game.cpp factory-COMDAT prefix (~76 fns; the
60-byte "Register" fns = NETMSG StaticByteCode COMDATs). StoreSongProvider
DOES NOT EXIST. New retail shapes: single-expression `bool ret = A && B`
materializes MSVC $initflag conditional-EH funclets; UNUSED static Symbol
locals occur (GetTier's rank); erase(begin,end)→clear() for vector COMDAT;
inline ShortName()=mStoreOfferData->Sym(0). QUEUED NEXT EASY WIN:
StoreMenuProvider TU ~0x82656B58–0x82657610 unpinned, oracle exists (~20
fns, ctor 0x826574C0/SetData 0x82656D18/Handle 0x82657180 pre-located).
Banked: GemTrainerLoopPanel ctor 76 (vbase[0] literal-0 wall);
GamePanel NewObject 99.96 = MILO_DEBUG hud members +0x18 (gated excision
pass, touches 38 matched fns). NOTE: map now has a case-variant dup key
0x82586DB8 (HamSongMetadata) vs 0x82586db8 (BandSongMetadata, concurrent
l3 landing) — harmless today (last-wins), clean up when touching songmeta.

**Synth follow-up LANDED 9744b25 (+139 → 13445).** Mic/Distortion pairings
were correct-class WRONG-BOUNDARY; 6 FxSend TUs recarved (5 COMPLETE). The
old FxSendDelay span diffed the WRONG OBJ — synth/ vs synth_xbox/ BASENAME
COLLISION (watch for this whenever both dirs have a same-named .cpp).
Global levers landed: StandardEffect<T>::Reset() header thunk (was
systematically unpaired in every FxSend unit); OBJ_SET_TYPE_ENGINE macro
(engine-era SetType: static-config init BEFORE null-check,
PathName-before-ClassName notify args — band3 keeps old macro). FxSend360
pure virtuals bind overrides FxSend360-relative. QUEUED: Synth.cpp recarve
(12 fns folded into Compress span); unpinned MeterEffect/Synapse TU
0x82B34FE0–0x82B35604; CharLookAt::SetType 62.7 + DancerSequence::SetType
50.3 (same disease as the 5 fixed); FlangerEffect::Process 77.9 permuter
candidate (real transliteration bug fixed: double-incremented conflated
loop counter).

**SongSortMgr reconstruction LANDED 16c99bd (+99 → 13544).** "0% alignment"
was mostly pin-hole + foreign-tail misattribution (5.6KB unclaimed hole;
tail was NetSync.cpp). Real Wii→360 finds: **FilterType enum RENUMBERED
(HMX edited only case labels, kept Wii textual body order — jump-table
layout requires source order match the OLD enum)**; hash_map (100-bucket
STLport) where Wii used map; retail Object.h DECLARES Hmx::Object copy
ctor; **DataArray::Release is never inlined in retail** (out-of-line in
DataArray.cpp — gave PatchDir +2 for free). Static Symbol in a ctor also
STOPS MSVC inlining that ctor into callers. LANDING TECHNIQUE: when a
patch reorders a big JSON file, do a SEMANTIC 3-way merge (parse
base/theirs/ours dicts, apply theirs-vs-base delta onto ours) — textual
merge was 1000+ phantom removals, semantic delta was +64/-8/~1 clean.
Kill: ~SongSortByRank 39.5 (retail elides MI vtable-store pairs in this
one dtor). LEADS: NetSync pin extension backwards over 0x82582EF8–
0x82583DD8; grep retail dtors lacking vtable stores for the elision
pattern; DoesOfferMatchFilter 88.7 permuter band.

**StoreMenuProvider LANDED 5a8584c (+44 → 13588).** Retail 360 provider is
a REWRITE off the raw menu DataArray (no StorePackedMetadata/StorePage).
TWO NEW MICRO-LEVERS: (1) **Yoda comparison** `NULL != ptr` (constant
LEFT) in HANDLE_EXPR reproduces retail's phi-copy addi-r11,r11,0 before
subic/subfe — natural order folds it; recurs at 0x82598Dxx-class Handles.
(2) **Temp-lifetime statement split**: a bool conversion must be its own
`bool x = expr.Int();` statement — inlined-in-ctor-args extends temps
across operator new (frame+0x20), int local converts too late; ONE such
fix unblocked 12 downstream pairings in ByArtist (38/38 COMPLETE).
Banked leads: fn_82659CD8 needs 360 SongDB layout ≥0xb1 (shared header,
gated pass); AppLabel::SetStoreMenuText body when AppLabel TU (76/100)
gets its pass.

**NetSync LANDED 5890482 (+76 → 13664), unit 123/123 COMPLETE. DTOR
VTABLE-STORE ELISION SOLVED** (pattern doc: docs/decomp/patterns/
fixable-declarations.md#implicit-destructor-vtable-store-elision):
user-declared EMPTY dtors emit own-vtable stores; implicit dtors elide
them + ICF-fold the siblings. Removing `~X() {}` fixed 4 dtors including
REVERSING the banked ??1SongSortByRank at_limit kill (39.5→100 — at_limit
kills CAN be wrong; re-check when a new pattern lands). Triage sig: 3-6
lis/addi/stw ??_7 inserts at dtor entry, empty dtors ONLY. Also
MakeString lever: retail inlines 1-arg MakeString AND FormatString::
mFmtBuf=0x800 not DC3's 0x1000 (+5 cross-unit bonus). LANDING TRAP: a
patch touching a DIRTY docs file aborts git apply ENTIRELY (and `echo
exit:$?` after a grep pipeline reads grep's status, not apply's — check
pipestatus); use --exclude=<dirty-file> + hand-apply that hunk. QUEUED:
BandUI pin-head recarve 0x82522758+ (~15 fns at 0%, StartTransitionMsg
family bodies; BandUI::Handle 73.7 @3564B); sub-100 dtors w/ REAL bodies
(??1Object@Hmx 63.3 etc.) = logic divergence class, NOT elision.

**Dtor-elision fleet sweep LANDED de77459 (+47 → 13711).** The lever
escalated to TEMPLATE level: ObjPtr<T>::~ObjPtr() {} deletion alone fixed
13 units (incl 2 fns previously misdiagnosed as "real body divergence" —
the extra stores were ObjPtr's). COUNTER-EXAMPLE DISCRIMINATOR (pre-edit
check): if the class's own ??1 already matches 100, or currently-100 fns
call ??1Class out-of-line, retail KEPT the user dtor — UIListProvider
deletion = 105 losses, RndAnimatable = 30 (both tested+reverted). Base
classes w/ many wired derived = near-certain keeps. TOOLING LEVER QUEUED:
??_E is a COFF WEAK EXTERNAL aliasing ??_G — objdiff can't pair target
??_E w/ our ??_G (map rename or weak-alias obj patcher would reveal
matches: ??_EAsyncFile, ??_ECharBoneDir, ??_ENetCacheMgrXbox,
??_EBandSongMgr, +more). Map errors noted: ??1IdUpdater target is an
atexit handler; ??_GUIListProvider 71.8 target has fp math.

**Synth.cpp recarve LANDED dcca18a (+120 → 13831).** Synth TU true extent
0x82B2B428–0x82B2F4F0 (pin was a sliver); Synth unit 2→112. NO BitCrush
factory in RB3. Synth360 relayout: base 0x88, 5 DC3 members → class
statics. NewStreamDecoder: RB3 = hxma→XMAReader / mogg→VorbisReader (not
DC3's bik/mogg/wav). FXSEND360_NEW lever (retail inlines OBJ_MEM_OVERLOAD
op new into NewObject). "DancerSequence::SetType 50.3" was a STALE
PREMISE — target is a BinStream Load (map fixed; Load port = lead).
QUEUED: CXAPOBase QI/AddRef/Release family (7 fns, needs inline ATG
xapobase impls); XAUDIO2_SEND_DESCRIPTOR stlport band (uninit_copy 47 /
fill_n 41 / push_back 54, same %s as DC3); fn_82B2D120/170 RB3-only mic
passthroughs; NewObject EH spill-slot −4 artifact (~20 fns 99.9);
PreInit/Init/SetupHeadsetSubmixes permuter band.

**??_E pairing fix LANDED f9f5173 (+16 → 13887).** 16 map renames
??_E→??_G (COFF weak-external alias gap). DISCRIMINATOR: folded targets
are 68–92B scalar shape; a ??_E >100B is a REAL vector body (CharMirror
132B counter-example — retail has new CharMirror[]). FUTURE FREE +1s: 18
"neither" cases pair when their TU emits the dtor COMDAT; 61 auto-unit
??_E entries pair when those spans get wired (re-run the rename sweep
then). Two mirage map entries resolved: "??1IdUpdater"→
ShowBriefBandMessage@TrackerBroadcastDisplay (TrackerDisplay unwired);
"??_GUIListProvider"→GetConcealFramesPerSecond@BandList (BandList obj
missing).

**AppLabel/StoreMenuPanel LANDED f3c39f0 (+107 → 13994) + GamePanel
LANDED 9885488 (+3 → 13997).** AppLabel pin was too short (Handle 4624B
past the end; 7 RockCentral mirage sub-pins evicted, 10 funclets
re-attributed at 100); StoreMenuPanel pin held TWO TUs (tail =
TrainingPanel.cpp → 44/44 COMPLETE). **OBJ_SET_TYPE HEADER-ORDER TRAP:
Object.h and ObjMacros.h BOTH define it (C4005 warns); include
ObjMacros.h BEFORE the class header — check first for ~60s SetType
scores.** LocalizeSeparatedInt(int) is real out-of-line retail.
GamePanel: its .cpp #undef's MILO_DEBUG locally so its own TU matched
all along — the +0x18 leaked via OTHER TUs; no backing storage needed
when debug paths are already compiled out. **Timer is 0x30 on X360 vs
0x28 Wii (8-aligned mCycles) — Wii-era offset comments after a Timer
member are ALL +8 stale.** fn_82659CD8 = Game::HasIntro (GamePanel+0x54
= mGame, NOT Game+0x54 = mSongDB; SongDB needs no extension). LEADS:
Game::Start = fn_8265EC68 in UNPINNED Game.cpp gap 0x8265EBE8–0x8265F7A8;
GamePanel::Load 924B @0%; SavedSetlist retail has NO vptr; AppLabel::
Handle 99.96 −172 .data addend (static census); GemTrainerLoopPanel 76.

**BandUI recarve LANDED d4afbf0 (+102 → 14099, CROSSED 14K).** BandUI
183/186; new UITransitionNetMsgs unit 16/16 (real TU was in the
band3/game cluster hole, NOT BandUI's head — queued premise corrected).
GLOBAL LEVER: Hmx::Object::AddSink is 4-ARG in retail (no bool chain —
DC3 addition). PATTERN CORRECTION: "retail keeps ~StartTransitionMsg"
was WRONG — its 72B ??1 is the IMPLICIT dtor (real body from the String
member, no vtable stores); elision counter-examples must check for
missing own-vtable stores, not mere existence. /d1reportSingleClassLayout
works through wibo for layout verification. **OPEN CONTRADICTION — Timer
size: Game.h wave says X360 Timer=0x30 (vs Wii 0x28); BandUI Init raw
diff (li 0x70 vs 0x78) says ours is 8B BIGGER than retail. Dedicated
Timer study dispatched; do NOT touch Timer until reconciled.**

**Timer contradiction RESOLVED 548d055 (+1 → 14100).** Timer=0x30 on X360
confirmed (retail ctor fn_824FE428 byte-matches our header); the BandUI
"8B bigger" was a phantom `int unk24` in ShellInputInterceptor. **KEY
LESSON: Wii-oracle members sitting in alignment padding on Wii are
UNPROVABLE from Wii evidence and may be PHANTOM on 360 — when 360 layout
shifts alignment, re-derive from the retail ctor.** Fleet sweep: all
other unk-before-Timer members are pad-equivalent, no further lever.
Hmx::Object=0x28 on X360 (corroborated 2 ways).

**Game gaps LANDED 7d396ee (+50 → 14450). GamePanel 63/63 COMPLETE, Game
112/114.** FOUR ENGINE LEVERS: TheSongMgr is a POINTER global on retail
(reference lets MSVC CSE ptr+vptr; pointer forces reloads — check other
The* globals for the same disease); MI-base virtual calls emit NO null
check (bare addi) while pointer/cast forms do; HxSongData slot1 =
CalcSongPos(float); Rnd vtable ForceColorClear BEFORE Clear.
Handle@Game closed: print_base_points is HANDLE_ACTION not HANDLE_EXPR.
**LANDING TRAP — STALE OBJCACHE SERVE: post-apply A/B showed phantom −1
(MemcardMgr::Init emitting the pre-d4afbf0 5-arg AddSink); forced
OBJCACHE=off recompile emitted 4-arg and held 100. If an A/B loss
involves a symbol changed by a recent engine-header landing,
force-recompile that TU before diagnosing.** Leads: fn_8265BC00 (0x60B,
Player+0x2dd loop, no xrefs); WorldDir+0x381/MidiParserMgr+0x69 real
members; provisional XOutputMixer singleton @0x82DDF9A8.

**Pointer-global sweep: CLEAN KILL (no landing).** TheSongMgr was the
ONLY The* global with the pointer disease — all ~67 others exonerated
fleet-wide (12 refs, ~25 objects, ~30 pointers all correct; DC3
attestation + at-100 referencing-fn discriminators; single use proves
object-vs-pointer, 2+ uses across a call proves ref-vs-pointer). Do NOT
re-sweep. Reusable artifact: ~/tmp/ptrglobal_relocscan.json (global →
referencing fns w/ match%). Indeterminate-zero-payoff set to re-recon
when pinned: TheQuestMgr (Tour.cpp gap, GetFilterName = perfect
discriminator site), TheNetMessenger, TheStoreMetadata + 6 more. Leads:
Tour::Handle 95.86 (helper-shape); OvershellSlot::UpdateState 75.8 +
UIStats::MaybePublish 67.4 = static-guard-counter TU-order fixes.

[[project_wave4_2026-07-09]] [[project_wave2_e1_autoid_2026-07-09]]
[[project_crack_farm_saturation_2026-07-09]]

## Landing 21: metagated (04775d7f, +18, 14450→14468) — AppLabel 157/157 + SavedSetlist 66/66 COMPLETE
Both banked premises REFUTED (kill them):
- "SavedSetlist has NO vptr (mTitle@0/mDescription@0x10)" — FALSE. Retail asm pins our vptr layout (SetTitle @+0x4, SetDescription @+0x24, mSongs @0x10/0x14, all 100). The 99.83 pair was a MAP MISPAIRING: 0x825AD298/2B0 are 360-only `AppLabel::SetFriendName/SetFriendBandName(const FriendRecord*)` — record `{String name; bool online@0xC; String bandName@0x10}`, caller dyncasts UILabel→AppLabel + tests status_online/offline.mat. Real SetSetlistName = pdata-less 24B bctr thunk 0x825ADB30 (was dtk lbl_, split via symbols.txt). Retail has NO out-of-line SetSetlistDescription (K=0x2C byte pattern absent binary-wide — /OPT:REF dropped it); our unpaired extra is harmless.
- "AppLabel::Handle 99.96 = −172 .data static-block addend" — FALSE. All 42 diffs were the VBASE DISPLACEMENT (subi r25,0x25C vs 0x1B0): our AppLabel non-vbase size was 172B short (missing UILabel/BandLabel members, retail BandLabel traffic at 0x214..0x258). Fixed with **0xAC trailing reserve in BandLabel** (`mRetailLayoutReserve`, wave-12 lever) — zero-loss by construction (only AppLabel derives BandLabel). Bonus: BandLabel's two 99.94 vtordisp fns → 100.
NEW LEVER: **Yoda zero-compare `0 != x` emits cmplwi cr6; `x != 0` emits cr0** (MSVC X360) — closed SetCreditsText's last artifact. Related existing lever: constant-LEFT Yoda in HANDLE_EXPR.
Other fixes: SetUserName map 0x825ac480 = (const User*) overload (vcall UserName slot 0x70); BandStorePanel::mMenuTitle @0xC8 not 0xD4; SetRatingIcon = SystemLocale() hoisted to a LOCAL (slot-read vs return-ptr-read kills regswap cascade); ProcessRetCode retail shape = local static `error_message` Symbol (guard bit 1) + NO this-touching tail; bogus `MetaPerformer::AdvanceSong`@0x82592060 is actually `SavedSetlist::AddSong` (mSongs.push_back @+0x10) — real AdvanceSong address UNKNOWN.
Leads surfaced: **BandProfile::Handle 99.99** — our `set<Symbol>` vs retail `map<Symbol,bool>` (MapTraits _M_find, ±4 node offsets ×10 sites; header member-type change); MetaPerformer::Handle 87.27 untouched; EditSetlistPanel 98/186 + MusicLibrary 153/224 need dedicated reconstruction (SavedSetlist layout was never their blocker).

## Landing 22: profmap (226abad2, +0 infra) + Landing 23: guardfix (95b4916f, +6, →14580)
**profmap**: BandProfile set→map premise REFUTED — STLport `_Rb_tree` header is value-type-independent (set/map both 0x18, size-neutral) and `_M_find`/insert_unique already ICF-normalize; real Handle 99.985 blockers = (a) size-neutral interior relocation of `Symbol unk6c` (mLastPrefabCharUsed) — retail puts mCampaignKeys/unk88/mUnlockedModifiers/mAccomplishmentProgress 4B EARLIER, shift returns to 0 by mProfilePicture; (b) ProfilePicture's inline OnlineID is the 0x18 DC3-variant (with String mPlayerName) in this TU vs our 0x10 — per-TU gateable. MetaPerformer::Handle 87.27 = whole-body callee-saved regalloc cascade (r28↔r29 ×219 etc.), permuter-class. WIN: real `MetaPerformer::AdvanceSong` = **0x82567C68** mapped (old 0x82592060 was FP=SavedSetlist::AddSong); body port needs a splits pin covering 0x82567C68 (clean 3-liner in retail).
**guardfix**: KEY INSIGHT — **guard words are PER-FUNCTION on MSVC X360, not per-TU** (OvershellSlot's UpdateState vs UpdateView use different words) → the "TU static-definition reorder" lever concept is DEAD; the real fix class is adding/removing retail's function-local statics to match bit order. UpdateState 75.8→100 (added exit_msg/enter_msg statics + retail-only ShowEnterFlowPrompt block); Tour::Handle 95.86→100 = INVERSE case (our cheat_reload_data arm retail-absent — stripped cheat; removing realigned bits), Tour.cpp COMPLETE; MaybePublish 67.4→99.4 (+4). NEW LEVERS: (1) **const-overload group vtable-slot REVERSAL** — const BandUser* GetLocal/GetRemoteBandUser land at slots 5/7, explains off-by-4 vcall mysteries; (2) `DataPointMgr::RecordDataPoint(DataPoint&, bool)` on retail RB3 — DC3 dropped the bool (concrete DC3-is-newer engine divergence). FriendsProvider.h reconstructed (X360-only, vtable 0x820D4424); OvershellSlot sizeof 0xC0, mFriendsProvider@0xb4.
Leads: real OvershellSlot::ShowState = 0x825BED40 4-call body (easy +1, map fix 0x825C10D8 is wrong); 0x825C7188 = OvershellSlot ctor not RockCentral::RecordDataPoint; 0x8259ACE8 = ??0OvershellAllowingInputChangedMsg not SoundPlayMsg; MaybePublish 99.4 stack-slot coloring = permuter; BandProfile unk6c slot pinning + ProfilePicture OnlineID per-TU gate = dedicated lane; AdvanceSong splits pin.

## Landing 24: profile (d0307b14, +2, →14586) — BandProfile::Handle CLOSED
Both diagnosed fixes confirmed and landed zero-loss: (1) transient `Symbol unk6c` (mLastPrefabCharUsed) relocated from Wii's 0x6c slot to AFTER mAccomplishmentProgress on retail X360 — size-neutral reorder, 9 of 10 mismatches; (2) ProfilePicture's inline OnlineID = 0x18 DC3-variant (XUID@0, String mPlayerName@0x8, mValid@0x10) — per-TU ODR skew gated as **RB3_ONLINEID_PLAYERNAME** (OnlineID.h + /D on BandProfile.cpp; global 0x10 verified correct in Leaderboard/EntityID). Bonus reveal: MemcardMgr::Init 98.64→100 (includes BandProfile.h). LEVER GENERALIZATION: per-TU OnlineID-variant gate may apply to any future TU embedding ProfilePicture inline. Lesson reinforced: "member relocation" walls can be exactly size-neutral reorders — check whether tail members already match (shift reabsorbed) before assuming a missing/extra member.

## Landing 25: oshell (a881ed9d, +2, →14604) — ShowState + AdvanceSong 100
- 0x825C10D8 was mislabeled ShowState — actually **OvershellSlot::Handle** (BEGIN_HANDLERS dispatcher); real ShowState = 0x825BED40 (discrete pin, Wii-only reconnect_controller block stripped) → 100. AdvanceSong: body already in source, discrete pin 0x82567C68..CCC → 100 (discrete pin > range extension for out-of-range singles).
- Misattribution fixes: 0x825C7188 = **OvershellSlot ctor** (pin MOVED RockCentral.cpp→OvershellSlot.cpp, honest 86.3 permuter-class residual); 0x8259ACE8 = ??0OvershellAllowingInputChangedMsg (dormant map fix — our copy ICF-folds).
- WORKTREE A/B TRAP (bit again): setup_worktree.sh branches from CURRENT main; if main advanced past your merge-base the seeded baseline shows phantom regressions — always rebuild baseline at `git merge-base` commit.
- Leads: OvershellSlot::Handle body-port (0% but correctly attributed); **RockCentral cluster 0x825C6948–0x825C7188 has MORE mislabels** (0x825C6948 = foreign ctor, vtable 0x820b3b3c) — cleanup pass; real RockCentral::RecordDataPoint address still unhunted.

## Landing 26: rcclean (911963af, +0 infra) — RockCentral attribution audit
- NEW FAILURE MODE: **duplicate mangled names in the map COLLIDE** — 0x825C6948 mislabeled DataPointToQString shared the name with the real one at 0x824E57A0 and zeroed ITS pairing. A mislabel can poison a correct pairing elsewhere; when a named fn reads 0% unexpectedly, grep the map for duplicate values.
- 0x825C6948 = PassiveMessageQueue ctor (RTTI-proven); real OvershellSlot::Handle = **0x825C8058** (pinned, 42.0 honest; retail ~970 insns fatter than Wii source — dedicated arm-by-arm reconstruction lane needed); 0x825C10D8 label removed (anon 155-insn sfx handler).
- OvershellSlot ctor 86.3 wall = retail has 4 BOOLS at 0x60-0x63 where our header has vector<PotentialUserEntry> mPotentialUsers — header relayout + full-unit A/B needed (deferred).
- RecordDataPoint hunt premise WRONG: fn_827A7DE8 is an indirect-call trampoline, not DataPointMgr::RecordDataPoint's body.
- Leads: PassiveMessenger.cpp UNWIRED (wiring could match ctor + neighbors); RockCentral pin set is a fingerprint grab-bag w/ foreign D3DX/XAUDIO2/XGRAPHICS entries (match-neutral, cosmetic).

## Landing 27: editsl (4f8427db, +7, →14611) — EditSetlistPanel 98→105/186
- WINNING RECIPE (reusable for any over-pinned panel unit): COFF symbol-set diff finds unpaired base symbols → call-graph tracing (Handle dispatch callees) + content fingerprints identify target addresses → append-only map entries → then the **360 local-static-Symbol lever** closes them (Wii oracle used file-scope Symbol globals; retail declares per-function locals, guard bits in decl order). 6 strict closes: SymToDayCount/DayCountToSym/SymToTimeUnits/GetTitleToken/CreateSetlist/CreateBattle.
- GetTitleToken: retail ternary INVERTED vs Wii oracle (asm-proven) — another "oracle return-shape not retail-correct" instance.
- METRIC NOTE: report `matched_functions` counts **normalized-100** (fork's normalized diff folds pure regswaps) — DoneEditing 99.6 fuzzy counts as matched. A/B gates should check BOTH fuzzy-100 and normalized-100 sets.
- Revealed partials for a 360-body-reconstruction lane: GetMessageToken 88 (callee-saved +2 shift, permuter), VerifyStrings 72 (wcslen), Poll 55, SetUIState 57, OnMsg-DWCProfanity 51. Wall: CleanupStringVerify 75 — retail-only pointer member @0x9c (delete + OggFree). Unit span over-pinned (foreign EventDialogPanel/DeJitterPanel/DxLight fns) — recarve candidate.

## Landing 28: oslot (df9f1435, +0 strict / unit fuzzy 63.5→84.0) — Handle 42→92.2
- Two keystone levers for fat dispatchers: (1) **/DRB3_HANDLE_LOCAL_STATIC per-TU gate** already exists — check any low-% Handle for it FIRST (+30pts here); (2) **Wii-only arm strip via #ifdef HX_NATIVE** when objdiff shows pure src-only inserts with zero retail deletes in-region (+20pts; 28 wiiprofile/wii_speak/invitation arms).
- ctor premise REFUTED AGAIN: "4 bools vs vector at 0x60" was stale comments — header already retail-correct (full offset table in lane report; retail does NOT zero-init 0x62/0x63). Lesson: rcclean's ctor-wall diagnosis was itself wrong; layout walls need decompile-verified offset tables, not diff-shape inference.
- Handle honest ceiling ~92 until we can READ retail arm token strings: Ghidra read_bytes returns obfuscated XEX section bytes — need the .rdata read path the decompiler uses. LEAD: build/find an XEX .rdata string reader; unlocks Xbox-only arms (gamercard/friends/signin) + guard-counter order → Handle→~100 and generalizes to every fat dispatcher.
- UpdateView 76.2 permuter; ctor 87.3 residual = guarded inline static-Symbol dispatch shape for init_msg/setup_providers.

## Landing 29: pmsg (64923320, +3, →14637) + Landing 30: muslib (b6e1fe26, +4, →14641)
**pmsg — PassiveMessenger.cpp WIRED (new TU)**: anchor premise corrected — 0x825C6948 is an inline PassiveMessageQueue-ctor COMDAT parked next to OvershellSlot (its instantiator); real TU .text = 0x825B1488–0x825B6500 (bracketed AppLabel/MusicLibraryNetSetlists, vtables 0x820b3b3c + 0x820af4ec). 3 fns at 100. LAYOUT LESSON: retail ctor "4-byte hole at 0x4" = automatic MSVC-X360 alignment (DC3's identical pattern is 100%) — stale Wii `// 0x4` comments, no header edit needed. Leads: ~30 scrambled fns unmapped in span (Trigger*/OnMsg/Handle + possible PassiveMessagesPanel boundary); **9 spurious RockCentral ICF-funclet micro-pins INSIDE the span** (carved around; do not reassign w/o whole-binary A/B); Ghidra band.exe .text raw-offset mapping unreliable (.rdata/decompiler path only). GetAndPreProcessFirstMessage 69.3 = retail out-of-line bool helper for Sym(0)==sym.
**muslib — recipe transfer WORKS**: COFF fingerprint matcher (size + callee-set bipartite) paired 23 targets, 33 unverifiable guesses PRUNED (discipline matters — cross-shadowed tiny fns/STL helpers/Type@Msg accessors are FP-prone). DifficultySortPart 20.8→100 = 9 function-local static Symbols shadowing Symbols2.h globals (target 584B vs base 228B = pure guard preamble — SIZE RATIO is the tell). RebuildProfileData: bool decl-order swap fixes li-0 order. Walls: Mat 69.6 coupled multi-issue; GetSongFilterAsString 99.8 shared-header inline reshape risk. Lead: retail keeps PushFilterToScreen/PushSonglistToScreen OUT-OF-LINE where we inline (inline-policy angle).

## Landing 31: xstr (a7253937, +0 strict / Handle 92→98.3) — NEW TOOL tools/xex_string_at.py
- **tools/xex_string_at.py**: reads C strings/hexdump at any retail VA from orig/45410914/band.exe (dtk-extracted PE = ground-truth bytes; VA→file-offset via PE section table since RVA≠file-offset for .text). FIXES the Ghidra read_bytes obfuscated-.rdata problem permanently. Usage: `python3 tools/xex_string_at.py 0x820b4324` → token string.
- **Fat-dispatcher recipe now complete**: Ghidra decompile → extract guarded Symbol-ctor string addrs in order → resolve via xex_string_at → difflib vs our BEGIN_HANDLERS → drop Wii-only arms (#ifdef HX_NATIVE), add retail-only arms as NON-INLINE methods (empty stubs would /Ob2-inline into Handle and break bl structure). No map entries needed for arm callees (normalized diff pairs bls by shape).
- OvershellSlot Handle: 6 Wii-only dropped, 9 Xbox-only added (gamercard/friends/signin family). Residual 1.7% = macro DataNode temp slots + inline-policy, permuter-class. Dead-end: UserLoginMsg is base-only-LOOKING but retail keeps it (gating desyncs aligner to 63).
- LEAD (high value): sweep sibling fat dispatchers — OvershellPanel, OvershellDir, profile/session panels — same Wii-vs-Xbox arm divergence expected.

## Landing 32: muslib2 (9705c023, +6, →14647) — MusicLibrary 163/224
- LEVER REFINEMENT: function-local static Symbol/Message does DOUBLE duty — guard preamble matches retail AND blocks /Ob2 from inlining trivial Push* one-liners into callers (Wii's extern globals allow the inline). One static fix closes fn + callers together (ResetFilters 94.6→100 fell out of PushFilterToScreen's out-of-lining).
- Tooling artifacts catalogued: SetlistIsFull 46 = dtk 8-byte split misalign (<illegal> decode at fn start — body matches, NOT a source problem); PushSetlistToScreen unscoreable (dtk emits only except_data_ symbol at the address).
- NEW WALL (potentially multi-fn): **MusicLibrary base +4** — ContentDir subobject at this+0x30 retail vs +0x2c ours; blocks PlaySetlist 71.2, likely contributes to ResetFilter 51/ContentDone 50 offsets. Single base-layout fix + unit-wide A/B could unblock several.
- Retail-divergent bodies (need asm reconstruction not port): SetHighlightIx (switch regroups node types {2,5}/{3,4,7}, calls UIListState::Provider + fn_825A8540), ClearSongPreview (single-arg SongPreview::Start + fn_825A3DC8 op-cancel), CheckSongPreview (18-instr missing tail).

## Landing 33: pmsg2 (ad0992fe, +15, →14662) — PassiveMessenger 16/26 at 100
- 20 fns mapped via vtable + call-graph + unique passive_message_* Symbol strings; Trigger cluster (10 fns) all closed by local-static-Symbol lever. GUARD DETAIL: if/else fns with 2 statics share ONE guard dword (bits 0/1), both declared at function TOP — in-branch placement regresses.
- GetAndPreProcess idiom: retail factors `msg->mText->Sym(0)==SYM` into anon-namespace bool helpers (static Symbol + Yoda). 
- Boundary: 0x825B1488-18xx is RockCentral-adjacent/font-glyph code, NOT PassiveMessagesPanel (scattered TU, unpinned).
- Wii/360 ctor sink-set divergence: 360 registers 3 sinks (incl ConnectionStatusChangedMsg) vs Wii 5 (InviteSent/InviteReceived/SessionDisconnected dropped).
- Leads: RB3-360-exclusive TriggerEarnedGamerpicMsg 0x825B3150 / TriggerEarnedAvatarAssetMsg 0x825B3318; OnMsg(SessionDisconnected) 0x825B4060 pin; 0x825B1DD0 = VoiceChatDisabledMsg::Type()?

## Landing 34: mlbase (f8bfb13f, +1, →14673) — THIRD layout-wall refutation
- "+4 base wall" was two DIFFERENT bases compared: retail PlaySetlist guard = HasSyncPermission() (Synchronizable@0x30 slot 3) vs Wii oracle's ContentDir() (Callback@0x2c slot 11). RTTI COL ground truth: MusicLibrary bases IDENTICAL to ours (UIListProvider@0, Object@0x4, Callback@0x2c, Synchronizable@0x30).
- SESSION PATTERN (3 refutations): offset-diff ≠ layout wall. Before believing any layout diagnosis: (1) extract RTTI COL offsets from band.exe .rdata, (2) check whether the two sides even dispatch through the SAME base, (3) check tail members already matching (shift reabsorbed). Wii-oracle METHOD CHOICE divergence (different getter, different base) mimics layout drift exactly.
- New micro-lever: retail `mSetlist.size() != 0` (end-begin subtraction) vs `!empty()`.
- Lead: ContentDir-vs-HasSyncPermission check in AppendToSetlist + PlaySetlist(SavedSetlist*); ResetFilter/ContentDone = genuine body divergences (_Rb_tree::clear, reordered PartForFilter).

## Landing 35+36: disp (56879e89, +28, →14701) + sub-lanes (31368626, fuzzy-only)
- **AnimFilter recarve (+26)**: DancerSequence.cpp pin was a PHANTOM TU (RB3 retail has NO dancer code — 2nd phantom after FlowIf; add "does this system exist in retail at all?" to recarve checklist). Span = RndAnimFilter TU tail, CubeTex pin had swallowed its head. SyncProperty arm strings via xex_string_at.py proved membership. Retail-idiom fixes: **Load = plain `static int gRev; bs >> gRev`** (no BinStreamRev/assert — SWEEPABLE across rndobj, tell: base-only srwi/clrlwi rev-split + PathName assert machinery); **Save static REV=2 → .data lwz not li-immediate**; SyncProperty chains only through RndAnimatable (Object chain HX_NATIVE-gated).
- SongSortByRank::Handle 20.5→100: map mislabel (0x826415B8 = hashtable _M_rehash template; real Handle 0x82641FE0). CONFIRMS: <25% dispatcher with no Symbol-ctors in target = map-mislabel suspect.
- **Dispatcher arm-table vein VERDICT: DRAINED for meta_band panels** (survey: all named panels already 100). Remaining Handle near-misses are permuter-class (CustomizePanel 98.95 at_limit, CharacterCreatorPanel 94.4 at_limit — "drop inlined null-guards" A/B-refuted).
- MetaPerformer::Handle 87→93.4 (9 src-only arms gated incl BOTH duplicate has_online_scoring; Ghidra decompile TRUNCATED the dispatcher — parse Symbol-ctor refs from target disasm instead); TourProgress 93→96.8 (dump_properties dropped, IsTourComplete = mOnTour && AreAllTourGigsComplete).
- UIManager 33.3 left intentionally (fix costs 3 funclet strict matches — in-source note).
- NEW SWEEPABLE LEVERS QUEUED: BinStreamRev→static-gRev Load sweep + Save static-REV li-vs-lwz sweep across rndobj.

## Landing 37: muslib3 (2e91e269, +2, →14703)
- NEW SHAPE LEVER: **early-return + fresh per-block pointer scope** vs the oracle's bool-flag accumulation form — bool-flags force ~30 extra insns, larger frame, wider callee-save band; early-returns keep the temp dying in-register (cmplwi-not-store tell). TryToSetHighlight 64.6→100 purely from this rewrite.
- SongPreview::Start is SINGLE-ARG on retail RB3 (2-arg TexMovie overload = DC3-newer). ClearSongPreview also clears pending op preview (fn_825A3DC8 = MusicLibraryUnkOp::ClearPreview).
- HIGH-LEVERAGE LEAD: [global+0x50]->Provider()->fn_825A8540(gNullStr) chain (provider method w/ String@+0x48, refresh@+0x70) blocks BOTH SetHighlightIx 79 AND CheckSongPreview 85.6 — identify the global + declare the method. Node-type groups decoded: {2,5}=Header/Function→Clear, {3,4,7}=Subheader/Song/StoreSong→Start.
- ResetFilter/ContentDone: retail this arrives as ContentMgr::Callback* (this-0x2c adjust) + wholesale body divergence — coupled, hard.

## Landing 38: grev (0f472927, +2, →14705) — Save-REV vein drained
- Lever-2 wins: RndMatAnim/RndParticleSysAnim Save (SAVE_REVS→static int REV=N, .data lwz). RndMesh::Save partial 93.7 (tail vtable-slot-0x18 call = real body gap).
- COUNTER-EXAMPLE: UIList::Save keeps li-immediate (value 0x13 ≠ our packRevs 0x15) — lever regressed it, reverted. VERIFY the lis/lwz-vs-li tell per function.
- VEIN VERDICTS: named Save-REV DRAINED (24 surveyed). Lever-1 (BinStreamRev→gRev Load) blocked on Sfx family — ObjVector<SfxMap>/<MoggClipMap> operator>> only exist for BinStreamRev&; coordinated child-type port = dedicated lane lead. Nested BinStreamRev Loads (CharHair::Strand etc.) show ObjRefConcrete-inlining/regswap, NOT the rev tell.

## Landing 39: editsl2 (6cf76c68, +6, →14711) — EditSetlistPanel 112/186
- MISLABEL PATTERN AGAIN: "genuine 360 rewrites" from the prior lane were partly MAP MISLABELS — 0x825FE380=Exiting (not Poll), 0x82601610=real Poll (not OnMsg(DWCProfanityResultMsg)). Retail 360 POLLS XStringVerify via XGetOverlappedResult; the Wii profanity message has NO retail counterpart. Lesson: before "reconstructing a divergent body", re-verify the pairing identity (vtable + callee shape).
- VerifyStrings = full XStringVerify idiom: packed STRING_DATA (wcslen+1 + wchar*), 0xe response + 0x1c XOVERLAPPED memset buffers, XStringVerify(0,"en-us",2,...). REUSABLE for any 360 profanity/string-verify site.
- SetUIState: 3 cases × local static Symbol+Message, one shared guard dword; closed 4 boilerplate funclets as a bonus.
- Poll 92.9 / Exiting 95.8 = permuter candidates FROM MAIN (worktree permuter can't resolve symbols). GetMessageToken 87.5 permuter-class confirmed.

## Landing 40: songprev (4a63eedd, +2, →14734) — provider-chain MIRAGE
- The decoded "[global+0x50]->Provider()->fn_825A8540" construct was NOT a new API: UIListState::Provider() is a trivial +0x20 accessor ICF-FOLDED with BandMachineMgr::GetLocalMachine; fn_825A8540 = LocalBandMachine::SetCurrentSongPreview(const char*). Our existing chain already emitted the retail block. LESSON: an unfamiliar call-chain in retail decompile may be ICF-folded trivial accessors — check ICF folding (lookup_merged_symbol) before inventing new globals/methods.
- Real closes: SetHighlightIx switch lowering (retail range-cascade; retail ADDS kNodeStoreSong(7) to Start group) + explicit local temp to keep gNullStr out of callee-saved band; CheckSongPreview type-7 StoreSongSortNode branch + SetStorePreview decl.
- Leads: pin LocalBandMachine TU fns fn_825A4288 (MusicLibraryUnkOp::SetStorePreview... actually SetStorePreview lives on the op class) + fn_825A8540 (LocalBandMachine::SetCurrentSongPreview) — tractable standalone matches.

## Landing 41: sfxrev (85e4bd24, +3, →14737) — Sfx family BinStreamRev port
- Family conversion pattern CONFIRMED: retail reads rev plain (`int rev; bs >> rev`) and CHILDREN read the parent rev via TU-statics (SfxMap::gRev / MoggClipMap::sRev set before vector reads) — classic rb3-Wii idiom minus the assert. Sfx::Load 76→100, SfxMap::Load + operator>> newly pinned 100.
- MoggClipMap 99.86 residual = PRE-EXISTING -0x24 member drift (mMoggClip retail 0x28 vs ours 0x4) hidden in baseline regswap noise — coupled struct-size target rippling ObjVector<MoggClipMap> helpers (68-99). Discrete future lane.
- SampleZone = separate family with own BinStreamRev pins — do NOT convert with Sfx.

## Landing 42: moggmap (596d29eb, +14, →14751) — dropped-Object-base lever
- MoggClipMap's -0x24 drift = DROPPED Hmx::Object BASE (our port followed DC3's newer refactor which removed it; rb3-Wii oracle has `: public Hmx::Object`). Restoring the base closed myLoad + snapped 13 stride-sensitive ObjVector<MoggClipMap> helpers to 100 simultaneously.
- NEW SWEEPABLE TRAP/LEVER: **DC3-sourced class with uniform member drift == 0x24 (= sizeof(Hmx::Object) X360) ⇒ check for a dropped Object base vs the rb3-Wii oracle.** Stride math in value-stored vector helpers = independent sizeof confirmation.
- ICF-artifact note: fn_8270092C's 67% was a pairing artifact of an unimplemented stub — its "drop to 0" on layout change is NOT a loss.
- Lead: vector<MoggClipMap>::_M_insert_overflow_aux 98.4 r23↔r24 (shared template — permuter only if isolatable).

## objbase sweep: CLEAN KILL (vein drained after 1 instance)
- Fleet survey (textual oracle 3-way cross-ref + offset-histogram + DB pattern flags + stride spot-checks) found ZERO further dropped-Object-base instances — MoggClipMap was the only one. Do NOT re-sweep.
- 3 false positives catalogued: RndFont/RndMat/RndWind have DC3-INSERTED intermediate bases (RndFontBase/BaseMaterial/RndHighlightable) that themselves derive from Object — layout intact, different refactor direction.
- 153 remaining DC3-only no-base classes are DC-specific (hamobj/gesture), no retail RB3 target.
- Byproduct leads (logic body-ports, NOT layout): rndobj/Utl mesh helpers (ComputeFaceTangentBasis 64, ResetNormals 67, TessellateMesh 71, MakeNormals 79); rndobj/Rnd (OnToggleHeap 82, UpdateRate 85, DrawTimers 88, CreateDefaults 89); FlowSound::OnSoundSelected 59; AppMiniLeaderboardDisplay::Update 45.

## Landing 43: bandmach (2cf7d52e, +14, →14765) — BandMachine.cpp pin uncork
- VEIN REMINDER: wired-with-source-but-UNPINNED TUs still exist (BandMachine.cpp had 4 of ~24 fns pinned). One identified fn (SetCurrentSongPreview from the mirage hunt) uncorked 14. The wired-unpinned audit vein (project-closeout3, "+146") is NOT fully exhausted — worth a fresh systematic pass: for every wired TU, diff obj symbol table vs pinned/mapped set.
- Levers: empty-virtual-dtor removal x3 (BandMachine family — retail implicit dtor, no vptr store); ??_G-vs-??_E slot-0 deleting dtor is SCALAR (mislabel false-0 fixed).
- BIG RECON LEAD: **MusicLibraryStore** — 360-only DLC store-preview op class, NO oracle, vtable 0x820abc8c, 3-base CHD 0x821da4f4, whole TU unwired ~130 fns 0x825A2F3C-0x825A6640 (~14KB). Span table in bandmach lane report (ClearPreview 0x825A3DD0, Finish 0x825A3ED0, SetStorePreview 0x825A4288, ctor 0x825A4860, dtor 0x825A5028, Poll 0x825A50F8). Dedicated Ghidra-reconstruction lane needed.

## Landing 44: pmsg3 (8ed60753, +7, →14772) — PassiveMessenger 24/26
- 360-exclusive reconstruction WORKS without oracle when siblings define the pattern (both gamerpic/avatar Triggers → 100 first try, modeled on the 8 sibling Triggers).
- LEVER REFINEMENT: `u->UserName()` DIRECT member call on a vbase-derived ptr — an explicit cast-to-virtual-base emits a runtime null-check retail lacks (extends the known MI-base no-null-check lever).
- Helper-placement lever: retail put bool helpers as MEMBERS of PassiveMessageQueue (unused this in r3), not anon-namespace free fns — the member conversion also unblocked their CALLER (GetAndPreProcess). When an anon-ns helper is 99.5 with an arg-reg shift, try member conversion.
- A/B TRAP variant hit: cross-worktree baseline contamination — baseline obj reflinked from main already contained a concurrent lane's improvement; ninja skipped recompile → phantom "regressions". Same-worktree A/B is authoritative.
- PassiveMessenger unit essentially CLOSED (24/26; ctor/dtor + Poll + Handle = honest Wii/360 sink-set divergence walls; TriggerMessage 99.2 at_limit).

## Landing 45: rndutl (aa4790c9, +0 strict / Utl unit fuzzy 66.3→67.4)
- ANTI-LESSON: a prior pass had massaged Utl.cpp mesh fns AWAY from the DC3 oracle — restoring DC3 canon recovered +6-10 pts each (ComputeFaceTangentBasis 74.6, MakeNormals 89.8, ResetNormals 77.6). For ENGINE code, drifting from DC3's exact source shape is usually a regression; permute AROUND the oracle, don't rewrite it.
- Retail cmpw is SIGNED in mesh loops — drop (unsigned) casts.
- DB staleness: Rnd::CreateDefaults + AppMiniLeaderboardDisplay::Update already 100 normalized (raw <100 = reloc noise) — DB corrected.
- LEADS: map 0x824E4AF0 "FlowSound::OnSoundSelected" MISLABEL (target = intrusive-list registration, global head lbl_82C926B8 — same head as PassiveMessenger's VoiceChatDisabled sink global 0x82C926B8! likely a sink-registration helper); Rnd::UpdateRate TextStream<<Symbol divergence (identify fn_8279E9F0).
- Residual walls: callee-save-count/frame-Δ cascades (permuter-class) on all four mesh fns + DrawTimers/OnToggleHeap/UpdateRate.

## Landing 46: mlstore (3acc97ba, +1, →14786) — MusicLibraryStore FOUNDATION
- "3-base hierarchy" premise CORRECTED: RTTI CHD decodes to plain single-inheritance MusicLibraryStore : Hmx::Object : ObjRef (all BCDs mdisp=0/pdisp=-1 linear). Only dtor virtual; Poll/Finish/SetStorePreview/ClearPreview non-virtual. Full 0x64 member layout decoded (see commit 3acc97ba).
- NEW MICRO-LEVER: **explicit-local deref caching** — `StoreOffer *offer = *it;` before a call forces retail's callee-save caching of the deref (FindOfferBySongID 91.6→100).
- SPAN WARNING: 0x825A2F3C-0x825A6640 INTERLEAVES multiple TUs — MLS core ~0x825A31C0-0x825A56A0; per-function attribution only.
- Next-lane spec: ClearPreview (identify 0x827A9728/68 = NetCacheMgr lock/unlock on TheNetCacheMgr global 0x82DD5A40 + notify thunk 0x828186C0, resolve delete-mPreviewMgr this-adjust); ctor+dtor+??_G coupled set (needs ??_7MusicLibraryStore vtable pin); then Finish/SetStorePreview; Poll (1448B) last.

## Landing 47: ident (e73456e7, map-only) — Flow phantom killed, TextStream<<int mapped
- 0x824E4AF0 = **~RockCentral base dtor** (RTTI vtable 0x8207eef4; MsgSource vbase @0xcc), not FlowSound. CONFIRMED: retail has ZERO FlowSound/FlowNode RTTI/strings — Flow system fully stripped (3rd Flow phantom: FlowIf pin, DancerSequence pin, now FlowSound pin).
- fn_8279E9F0 = TextStream::operator<<(int) — numeric operator<< cluster complete.
- RECARVE LEADS: **FlowSound.cpp split 0x824E4AF0-0x824E542C is a phantom pinned onto RockCentral code** — retarget/drop; **TextStream backward pin extension 0x8279E990-0x8279EB10** = free strict matches (operators sit in unpinned gap after FileStream.cpp).
- Rnd::UpdateRate at_limit 85.5: retail mRateGate is an interned Symbol (loads lbl_820010A0/88 for "cpu"/"gs"); struct change ripples ctor for 1-of-3 closes — deferred.

## Landing 48: pinaudit round 2 (ed45168c, +497, →15283) — BIGGEST LANE OF THE SESSION
- THE VEIN: map entries with UNPINNED address + name COMPILED by a wired obj, class-attributed to one owner. 960 candidates / 266 units → 481+ instant 100s (trivial accessors, StaticClassName, ??_G scalar dtors, STL helpers in separate contiguous clusters the original pins never reached). splits.txt-ONLY (renamer auto-names from map).
- ZERO-LOSS TRAPS (encode in round 3): (1) split header must use the SOURCE FILE'S REAL EXTENSION (.c vs .cpp basename collision broke vorbis); (2) NEVER pin a 2nd copy of an ICF-folded name already matching at baseline (unpairs both); (3) dtk tail-block-merge can silently absorb an adjacent fn when pinning near it (Part/Burst::Set) — A/B catches it, drop the pin.
- ROUND-3 BACKLOG (ranked, in ed45168c message + lane report): 423 pinned-but-<100 (STL 99.7-99.9 permuter band; Font Load 93.5/SetKerning 93.2, Morph SetFrame 99.6/InterpWeight 92.1, MessageTimer 88-95, SongLayout 96.3; Accomplishment Get* 23-35 = breadcrumb STUBS — reconstruct trivial stub body, see project_game_code_instrumentation); 53 collision-dropped; ~3,575 shared/ambiguous STL (needs per-address owner disambiguation — LARGE); 71 in reserved units.
- Tooling recipe in /tmp (regenerable): audit3.py + insert_pins.py + build_retry5.sh (sidecar-tracked, only removes own pins).

## Landing 49: pinaudit3 (8ae9244e, +116, →15399) — pin vein FULLY DRAINED
- ROUND-3 INSIGHT: same-signature template instantiations are byte-identical across TUs → pin ambiguous STL helpers under ANY compiling owner (dedupe by name, ICF-guard, one address per name). 186 pinned of 212 actionable.
- VEIN VERDICT: wired-unpinned pin audits DONE (rounds 1-3: +146/+497/+116). The remaining ~3,900 unpinned map entries have NO compiled base symbol — the next vein is TU WIRING (which TUs own those 3,900 addresses? = a wiring-priority census).
- Safety pattern proven again: matched-name SET DIFF after every wave catches collateral unpairings that net metrics hide (2 caught: _Rb_tree _M_copy under CharClip, vector<Burst> under Part).
- Leftovers: 14 ICF-guarded (unpinnable), 6 funclet-collision candidates (low ROI).

## Landings 50-51: mlstore2 (d937f352, +2, →15402) + flowre (65c0039a, +17, →15419)
**mlstore2**: MLS dtor + ??_G at 100; ctor 98.1/ClearPreview 91.7 at_limit — single root cause = **retail StorePreviewMgr has a VIRTUAL Hmx::Object base** (ctor fn_8278D000 constructs Object @+0x38 w/ most-derived flag; deleting-dtor uses vbase adjustor) vs our plain-SI header — shared-engine rework lane needed. IDs: fn_827A9768=NetCacheMgr::Unload, fn_827A8900=DeleteNetCacheLoader, fn_828186C0=XamBackgroundDownloadSetMode thunk. LANDING TRAP HIT AGAIN (2nd time): **multi-file git apply --3way aborts ATOMICALLY on one file's index mismatch while still printing per-file "applied cleanly"** — ALWAYS verify with git status/grep after any apply that reported an error. Also: concurrent coordinator's map commit (wave-23 730ba3b5) swept my hand-staged map entries (shared-index race — content fine).
**flowre**: 3rd Flow phantom killed with RTTI proof — FlowSound split retargeted to RockCentral (+9; the 2 "lost" FlowSound matches were phantom pairings of the same bytes); TextStream backward pin +5 ops (retail "%u" for unsigned char, not "%hhu") + FileStream tail +3. LEAD: **RockCentral.h 0x50 oversize** (MsgSource vbase this-0x11c vs retail 0xcc; Wii-only unks in 0x88-0x113) — blocks 5 near-misses at 99.8+, dedicated shrink lane w/ 122-fn A/B.

## Landing 52: tucensus (89f2ef4d, +4, →15428) — WIRING VEIN VERDICT: EXHAUSTED
- Census REFUTES "orphans = unwired TUs": of 5,535 orphan map entries, **92% (5,108) are XDK/middleware** (XGRAPHICS/D3DX/XAUDIO2/xWMA/LEAPFX/XAPO — no oracle, not targets). The 427 game/engine orphans are **body-port RESIDUE in already-wired units** (EH funclets, STL instances, ??_E/??_G thunks, missing methods), NOT new TUs. Pinned set 777/778 wired. Exactly ONE clean unwired-HAVE-SOURCE candidate remained.
- Wired StreamReceiver360.cpp (+4, pure wiring, DC3-identical source); its 6 named methods = 99.9 address-reloc noise (at_limit).
- **STRATEGIC REDIRECT (docs/plans/tu-wiring-census-2026-07-10.md + scripts/tu_wiring_*.py)**: the frontier is now BODY-PORT COMPLETION of stubbed/incomplete wired units (route the 427 to bodyport-* playbooks), NOT wiring or pinning. Pin audit (rounds 1-3, +759) + wiring both drained. Deferred: WinSockSocket (wired 70-line stub→body-port), TexLoadPanel (DC3-diverging oracle only).
- NEXT VEINS: (1) body-port residue in wired units (427 game/engine orphans, ranked table in census doc); (2) the 99.8+ permuter band (drive with permuter from main); (3) RockCentral 0x50 shrink (rcshrink lane running); (4) StorePreviewMgr virtual-Object-base rework (unblocks MLS ctor/ClearPreview).

## CORRECTION (2026-07-25): the objbase "clean kill" used an INCOMPLETE signature
The dropped-base sweep searched only for **"our class lost its `: public Hmx::Object`"**. It MISSED the inverse form:
**our class says `: public Hmx::Object` where Wii/retail uses an INTERMEDIATE base that itself derives from Object**
(DC3-newer refactors the intermediate away and substitutes plain Object). The sweep even saw RndFont/RndMat/RndWind
in this shape and dismissed them as false positives — correct for those three, but the SIGNATURE was the point.
Found by a later lane: **StorePreviewMgr** — ours `: public Hmx::Object`, Wii `: public MsgSource`
(and our own `src/system/obj/Msg.h` has `class MsgSource : public virtual Hmx::Object`), retail-confirmed
virtual-base codegen (vbtable 0x82114FC8, `Hmx::Object::Object(this+0x38)` under an `if (flag & 1)` guard,
vtordisp @0x34, sizeof 0x60, TU5 ctor 0x827B1FC8).
**Re-ran BOTH directions 2026-07-25 with a proper parser over src/system + src/band3 vs the Wii oracle:
exactly 1 inverse-signature hit (StorePreviewMgr). Vein now genuinely drained in both directions.**
LESSON: when declaring a vein drained, state the exact SIGNATURE searched — "no more instances of X" is only as
strong as the pattern used, and the inverse/adjacent form is where the next instance hides.
Related: retail == "Wii minus DEV-only fields" for StorePreviewMgr (DC3's mAttenuation/mLoopForever/mLastFailType/
mHasFailure/mTexMovie are all newer additions retail lacks).

<!-- ======== END memory file: project_wave5_lsr_2026-07-10.md ======== -->


<!-- ======== BEGIN memory file: project_wave6_2026-07-10.md (6360 bytes) ======== -->

## Archived memory: `project_wave6_2026-07-10.md`

---
name: project-wave6
description: "Wave-6 (2026-07-10): +15 landed (11791->11806, 18.02%); symbols.txt hygiene calibrated EXACTLY (9037 of 18097 except_data spurious); template-band = struct-size-delta catalog w/ safe-tail-pad lever (+7 DepthBuffer3D); Mic/HxMaster DC3-added-virtual drift confirmed twice; PCH worktree bug FIXED"
metadata: 
  node_type: memory
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-6: symbols hygiene + wall-crackers + template band — 2026-07-10 (wf_80b622b7-3cc)

**Landed: +15 (11,791 → 11,806 = 18.02%; baseline had moved 11,661→11,791 via concurrent
sessions).** 9 commits 406890f..379c061, 8 match patches + infra-pch tooling, 1 reject
(w3-cameramanager, no patch). ~1.4M tokens, ~30 min. Follows [[project-wave5]].

## symbols.txt hygiene — CALIBRATED EXACTLY (s1)

- Ground truth: `orig/45410914/band.exe` = decompressed XEX PE; .pdata at rawptr 0x1e9a00,
  56,942 non-leaf entries. **LEAF fns have NO pdata entry** — they live in gaps, so pdata
  alone can't bound them (why Sort.cpp needed gap-span logic).
- **Exactly 9,060 except_data_ match func_type==3 pdata starts (valid); 9,037 are SPURIOUS**
  (TU5-stale). The spurious check is exact, not heuristic.
- Audit scoped to 1,259 pinned .text splits: 721 flagged units, 2,765 MID_FUNC fragments,
  3,923 spurious except_data. Safe-fix subset this wave: 88 gaps/72 units → +3, 0 regr
  (f789e25; removed 93 stale symbols; denominator dropped 65,620→65,527).
- Tools land at `scripts/symbols_hygiene.py` (audit) + `scripts/symbols_hygiene_fix.py`
  (guarded transform). Guards REQUIRED: target_symbol_map protection + baseline-matched-addr
  protection (an unguarded full apply measured **-27**: gap-merge swallows separate matched
  leaf fns).
- **Backlog by EV:** (1) except_data SHADOWS a true function start (VarTimer::Ms 0x827AEE00,
  SampleZone::Includes 0x8270CCC0, Timer::Init 0x824FE3D0, "dozens") — needs fn_ SYNTHESIS
  tooling; (2) dedicated-tiny-range cases needing paired splits.txt edits (TrackerManager 8-byte
  range); (3) ~3,826 spurious inside matched/multi-leaf regions = NOT free, leave alone.

## Template band = struct-size-delta catalog (e1) — NEW FILTER + NEW LEVER

- The uniform 99.9x STLport/dtor band is NOT mirage: each fn has exactly ONE mismatched
  immediate (li/mulli/addi = element/node SIZE; ??_G subi = vbase this-adjust) encoding a
  DC3-vs-retail struct-size divergence. Deltas go BOTH directions → catalog, not one lever.
- **worklist.py filter signature**: {engine STL template inst OR ??_G/??_E dtor} AND 72–140B
  AND exactly one diff_arg differing only in an immediate → tag `struct-size-delta`.
- **Sub-class (a) = SAFE TAIL-PAD LEVER (reusable, auto-sweepable):** retail sizeof > ours AND
  head offsets already retail-correct → pad tail to retail sizeof → closes the WHOLE template
  family, zero regression risk. Proof: DepthBuffer3DAttachment 0x14→0x28 closed 8 fns (+7 net;
  the -1 was an ICF re-pairing artifact of a foreign 20B-elem vector, verified noise) (79bb233).
- Sub-class (b) retail < ours = DC3-ADDED members needing removal (regression risk): NetCacheMgr::
  ServerData (retail elem 12B vs 24B), EventTrigger::Anim (28 vs 52), FileMerger::Merger (16 vs
  100!), HamCamTransform::TransformArea, UIListElementDrawState (0x2c vs 0x3c).
- Sub-class (c) ??_G vbase this-adjust deltas both directions per class (CharInterest +68,
  HamListRibbon +428, RndScreenMask -148, RndShockwave -724, HamCharacter -96) = deep per-class RE.
- LightPreset ODDITY: our EnvLightEntry(0x68)/EnvironmentEntry(0x2c) sizes are the exact
  INVERSE of retail (0x2c/0x68) — member-set divergence, not a swap typo... verify by RE.
- FlowCommand list<DataNode> node retail 0x28 vs ours 0x10 — anomalous; do NOT touch DataNode.

## DC3-added-virtual drift confirmed TWICE more (now a top wall class)

- **Mic**: DC3 added `virtual void ClearBuffers()=0` retail lacks (rb3-Wii also lacks it);
  delete → GetName slot 0x90→0x8c → UpdateMultiMicDeviceSliders 100% (b6c2a25, +1).
  Identification recipe: bracket the dropped slot via already-matched virtual calls
  (GetGain 0x20 + IsMultiMicDevice 0x4c matched → slot in (0x4c,0x8c)), pick the DC3-only
  zero-caller candidate, prove by whole-binary A/B.
- **HxMaster**: DC3 scaffold added `virtual bool IsLoaded()` — made BeatMaster::IsLoaded an
  IMPLICIT override (same signature!) so calls dispatched via secondary base. Deleting the
  HxMaster decl → direct bl (889eb7c, +1). Subtle: implicit-override via signature match is a
  drift AMPLIFIER — a bogus base virtual can virtualize a derived method declared non-virtual.
- **CameraManager (w3, unfixed)**: retail CameraManager is a STANDALONE 0x34 class == rb3-Wii
  exactly (vtable@0, mParent@4, cats@8, mNextShot@0x14, mCurrentShot@0x20, mCamStartTime@0x2c,
  mFreeCam@0x30); DC3 made it `: public Hmx::Object` + blend/mCrowds members + Save/Copy/Load.
  No compilable partial (ObjPtr<CameraManager> needs Object) → needs coordinated foundational
  change: CameraManager.h + Dir.h (ObjPtr→raw ptr, shifts WorldDir) + World.cpp. Includers:
  HamDirector, ClosetPanel, CharLipSyncDriver, Song, Dir, CameraShot, World.

## Other

- **Game bool-block +2 is the LAST Game-layout residual**: bool run (mIsPaused..unk6f) sits
  2 bytes late (retail 0x78/0x79 vs ours 0x7a/0x7b), rooted in the mAllActivePlayers
  std::vector / SongPos region before it. Blocks HandleAudioLoad (99.977, ONLY this) — CLI
  normalized reads 100 but report scores <100 (report is authority).
- BandProfile mPatches/unk18 leading fix +1 (abc7864). EntityUploader ctor decl-order +1
  (406890f). ChunkStream field-assign reorder +1 (6314ad1). GetBestHit int→bool kept at +0
  strict for fuzzy 96.4→98.2 (partial-value doctrine).
- **infra-pch FIXED (379c061)**: setup_worktree.sh no longer poisons worktrees with main's
  absolute-path PCH; merger verified by editing Str.h in a fresh worktree, zero redefinition
  errors. Workaround (`rm -rf build/45410914/pch`) no longer needed in NEW worktrees.
- g2 walls are permuter-class codegen (arg-setup scheduling, register 3-cycles, frame
  reservation) — the game 90-99 band is starting to bottom out into regalloc territory.
- Concurrent session active on DataArray/DataFlex (78fdc92, 7b6d9ed) — avoid that family.

<!-- ======== END memory file: project_wave6_2026-07-10.md ======== -->


<!-- ======== BEGIN memory file: project_wave7_2026-07-10.md (5145 bytes) ======== -->

## Archived memory: `project_wave7_2026-07-10.md`

---
name: project-wave7
description: "Wave-7 (2026-07-10): +26 landed (12211->12237, 18.66%); hygiene R2 --synth mode +14 and OPENED ~77 new paired body-port targets; LightPreset inversion was a TARGET-MAP mislabel (+7); Game layout FULLY closed (+2); CameraManager/WorldDir precise handoff written; report.cache staleness gotcha"
metadata: 
  node_type: memory
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-7: hygiene R2 + foundational probes + body-port — 2026-07-10 (wf_94c78e9f-b7e)

**Landed: +26 (12,211 → 12,237 = 18.66%), 0 regressions, 7 commits 1df4ae3..15e29b8, 0
rejects (2 no-patch walls).** Baseline had jumped 11,806→12,211 via concurrent sessions —
biggest chunk = 8774d3e "RB3_HANDLE_LOCAL_STATIC fleet sweep +390 across 25 TUs" (the
local-static-Symbol lever industrialized by another session). Follows [[project-wave6]].

## Hygiene R2: --synth mode (+14) AND a new opened vein

- `scripts/symbols_hygiene_fix.py --synth`: where a target_symbol_map-named fn has NO fn_ at
  its true start (leaf fns have no pdata → R1 missed them) and a spurious lbl_/except_data_
  shadows it, synthesize `fn_<addr>` sized to the next hard boundary. Audit: **110 shadowed
  starts** (107 lbl_, 3 except_data_) in pinned ranges; 91 anchored after guards.
- **+14 self-closed** (compiled bodies already byte-exact: MultiTempoTempoMap,
  RndOcclusionQueryMgr, ScrollSelect, CharBone/CharBones/CharEyes, SongDB…, 15e29b8).
- **~77 MORE now PAIR but body-diverge (<100) — a brand-new named body-port vein** that was
  previously invisible (unpaired). Compare closeout7/reports/baseline.json vs current report
  to enumerate.
- New guards/walls: tiny (<=8B) STL forwarder synth abutting a matched sibling REGRESSES the
  sibling (dtk re-split corruption; SYNTH_REGRESSORS exclusion set); tiny-range synth can pair
  at 45% (body-port needed); **MEASUREMENT GOTCHA: after symbols.txt/splits.txt edits,
  `touch config.yml && ninja` serves a STALE report (cache '2513 hits/1 miss') — must
  `rm build/45410914/report.cache` before the resplit build for a trustworthy A/B.** Also:
  target_symbol_map.json edits need `rm build/45410914/target_symbol_renames.stamp` + resplit.

## LightPreset "size inversion" was a TARGET-SYMBOL-MAP MISLABEL (+7, bebae47)

Our source was CORRECT all along (EnvLightEntry=0x68 matches DC3, rb3-Wii, AND retail —
proven by 16 template fns at 100%). Only 4 template families were address-swapped in
scripts/target_symbol_map.json. Fix = 14-line map edit. **Lesson: before diagnosing a struct
divergence from template immediates, check whether the MAP labels are swapped — a "both
directions" size delta between two sibling structs of the same class is the signature.**
(Residual: retail EnvironmentEntry __uninitialized_copy address unlabeled, potential +1.)

## Game class layout: FULLY CLOSED (325f063, +2)

Bool-block reorder + SetGameOver temp closed HandleAudioLoad AND SetGameOver (the "FP
scheduling wall" fell out once layout was exact). Game saga complete across waves 4-7:
ATanInterpolator (Interp port) → DiscErrorMgrWii base drop → mMusicSpeed bool-run pack →
bool-block +2. Every delta was DC3/Wii provenance drift.

## CameraManager/WorldDir: PRECISE EXECUTION HANDOFF (c1, no patch — read wf_94c78e9f-b7e journal)

Ground truth established: retail CameraManager standalone 0x34 == rb3-Wii; **WorldDir holds it
BY VALUE @0x270** (retail; ours = DC3 ObjPtr @0x300), LightPresetManager after, no
PhysicsManager/ThreeDSoundManager/mOwnsCameraMgr; crowd ownership moves CameraManager→WorldDir
(CamShot::StartAnim at CameraShot.cpp:1336 → wdir->SetCrowds). Exact retail member list in the
journal. RISK: CameraShot unit has 157/247 matches. Needs a dedicated single-planned-commit
session with whole-binary A/B; NOT a 40-min agent task.

## Struct-shrink probes (st1, no patch — honest no-edit)

None of the shrink candidates is a clean single-member drop: NetCacheMgr::ServerData retail
12B < BOTH oracles (24B) — retail dropped cached fields Wii+DC3 both keep; ONLY isolated
tractable case (list elem, no member shift) → RE the 3-field layout from fn_827A8AC8 asm.
UIListElementDrawState = non-prefix rearrange (retail also reordered tail). EventTrigger::Anim
retail 0x24 drops ~6 DC3-added trailing fields. FileMerger/HamCamTransform unprobed.

## Other

- t1 tail-pad sweep: NO more clean tail-pads in the band (remaining = ICF mispairs, ??_G vbase
  thunks, regalloc) — DepthBuffer3D was the one big win; sub-classifier added to worklist.py (8259f69).
- q1: DataResultList +4 was a RED HERRING (not a wall); OvershellSlot fix was unk80/81 bool
  repack (+1). MetaPerformer base +8 = Synchronizable/MsgSource MULTIPLE+VIRTUAL inheritance — real, hard.
- Remaining game 85-95 band walls are mostly permuter-class (whole-fn register rotation,
  regalloc cascades): Track::Poll, Gem::AddInstance, StoreMenuPanel::Handle r27/r28,
  VocalTrack float scheduling. LicenseMgr mLicenses is a set→map container divergence (real lead).
- Merger validated the wave protocol again: gate.py per-fn set diff, resplit stamps, 0 regressions.

<!-- ======== END memory file: project_wave7_2026-07-10.md ======== -->


<!-- ======== BEGIN memory file: project_wave8_2026-07-10.md (4988 bytes) ======== -->

## Archived memory: `project_wave8_2026-07-10.md`

---
name: project-wave8
description: "Wave-8 (2026-07-10): +22 landed (12239->12261, 18.70%); CameraManager/WorldDir FOUNDATIONAL LANDED +13 (by-value member, crowd relocation); synth-anchor 0% band = SPLIT-TRUNC/WRONG-UNIT/BOGUS-MAP taxonomy (boundary-fix lever +5, more queued); ServerData retail=20B fully RE'd; render-base +64 force-multiplier lead"
metadata: 
  node_type: memory
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-8: CameraManager execution + synth-vein triage — 2026-07-10 (wf_6cf3c9a4-549)

**Landed: +22 (12,239 → 12,261 = 18.70%), 0 regressions, 5 commits 7bd4f54..cb003ac, 2
honest no-patch.** Follows [[project-wave7]].

## CameraManager/WorldDir foundational: LANDED +13 (cb003ac)

Wave-7's handoff executed. Decision-critical addition: **retail ObjPtr is 0xc
{vtable@0,mOwner@4,mObject@8}** → standalone CameraManager mCurrentShot@0x20/mObject@0x28/
sizeof 0x34 byte-exact with rb3-Wii. Change (7 files): CameraManager standalone polymorphic
(BEGIN_CUSTOM_HANDLERS — not an Hmx::Object); WorldDir holds it BY VALUE (was ObjPtr@0x300;
GetCameraManager returns &mCameraManager; SyncObjects direct call, dropped object-in-dir
pattern + REGISTER_OBJ_FACTORY); crowd ownership relocated CameraManager→WorldDir
(CameraShot.cpp:1336 → crowdDir->SetCrowds); retail-360 StartShot_ keeps TheDOFProc (not
Wii's TheWiiRnd). Scope choice: KEPT DC3's ThreeDSoundManager/PhysicsManager/LightPresetManager
in WorldDir tail (retail-presence unverified; avoids caller cascade) — WorldDir tail is
still non-retail-exact = future work.
**Residual (c3 follow-up):** 4 CameraManager methods have DC3-added-PARAM drift (NumCameraShots
77.1 has a std::list<CamShot*>* out-param, MakeCategoryAndFilters 82.4 a float* blend
out-param; retail = rb3-Wii 2-arg forms), OnNumCameraShots 88.8, OnFindCameraShot 96.1.
Merger note: c2's 2 predicted ICF-attribution losses (BandUser/Performer::Handle vanishing
from unit fn lists — target-side icf_aliases re-attribution, not real regressions) did NOT
materialize in the sequential merge (+13/0 lost).

## Synth-anchor 0% band taxonomy (bp1+bp2) — NEW splits/symbols wall classes

bp1 landed +5 (78c67d6) with the **fn-boundary-sizing lever**: synthesized anchors whose fn_
size stops at a mid-function boundary truncate the target; fixing the size in symbols.txt to
the full body closes fns whose compiled bodies already match (CSHA1 ctor/dtor,
SetBlendEnable, SIVideo::FrameSize, DataNode ctor). bp2 triaged all 42 0%-band entries, ZERO
are portable bodies:
- **SPLIT-TRUNC** (~34 incl. all STL templates): target truncated to head, tail became
  anonymous fn_; head instrs match 100%. FIX = bp1's boundary lever (queued: Rot
  MakeRotQuatUnitX/FastInterp, Utl DistributeXfms/BadUV, CharClip LengthSeconds/
  ReverseKeyLessEq, ~28 STL __median/__unguarded_partition/_S_merge/__find/_M_erase).
- **WRONG-UNIT pin** (3): name correct, address correct, but pinned into a unit whose source
  doesn't define it → our obj emits stub → 0%. FIX = re-pin to owning unit (NormalizeSystemArgs
  → os/System.cpp; RateToTaskUnits; ClearCriticalUser).
- **BOGUS map addr** (5+): map names an address holding a DIFFERENT function (not ICF fold) —
  Speed/SetSpeed@UIList, Run@App resolves MID-FUNCTION, 2 cross-unit-class. Delete (net-neutral).

## ServerData: premise correction + complete retail RE (n1, no patch)

Wave-7 st1's "retail ServerData=12B" was WRONG — the mapped clear (0x82741B78) is a
**TypeProps 12B-list clear, misnamed** (fingerprint mislabel; all list clears are structurally
identical). True retail ServerData = **20 bytes {Symbol type@0, bool local@4, const char*
server@8, ushort port@0xc, String root@0x10}** (from OnInit fn_827A8AC8 → create_node
fn_827A86F8 allocating 0x1c); our 24B has DC3-only debug+verifySSL tails and root as char*.
The 20B value ICF-merges with MotdData@MainMenuPanel. Full fix = struct shrink + OnInit/
accessor rewrite + repair map to the true 0x1c-freeing ICF survivor. **Recurring lesson:
structurally-generic template fns (list clears) get fingerprint-mislabeled — verify a map
label against node SIZE before trusting it.**

## New engine layout walls (g1, +1 Synth.h mHud fix landed)

- **Shared render-base +64: member 0x188 retail vs 0x1c8 ours across THREE unrelated units**
  — one RndMat/RndDrawable-family base 64B too big. FORCE-MULTIPLIER candidate, un-root-caused.
- RndAnimatable VIRTUAL-inherits Hmx::Object: RndPropAnim Save-entry this +16 off; its 3
  DC3-added members are used by 100% fns so retail HAS them — placement puzzle, not removal.
- CharClip +16 (mBlendSamples/unk198, rb3-Wii ends at mZeros) — defensible but invasive
  (ApplyBlendedSkeletons removal); needs retail disasm confirmation.
- Sequence/GroupSeq/RandomGroupSeq accumulate +36 in the base chain (mPlayHistory +0x24).
- MetaPerformer +8 = Wii-only EARLY members dropped (950631f, +1) — q1's MI-base theory wrong,
  it was plain member drift.

<!-- ======== END memory file: project_wave8_2026-07-10.md ======== -->


<!-- ======== BEGIN memory file: project_wave9_2026-07-10.md (4356 bytes) ======== -->

## Archived memory: `project_wave9_2026-07-10.md`

---
name: project-wave9
description: "Wave-9 (2026-07-10): +25 landed (12261->12286, 18.74%); boundary lever industrialized +13; CameraManager msg-param reverts +4 (saga DONE); ServerData 20B POD landed; BaseMaterial FULL REORDER ground truth (retail ctor fn_82425998 decompiled, ~35 fns behind wall) = wave-10 centerpiece; 1 understood regression (session's first)"
metadata: 
  node_type: memory
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-9: boundary lever at scale + executions — 2026-07-10 (wf_60f31d20-521)

**Landed: +25 (12,261 → 12,286 = 18.74%), 5 commits 667ca9c..72f94e5, 1 understood
regression (session's only one).** Follows [[project-wave8]].

## Landed

- **sb1 boundary lever +13 (72f94e5):** 13 truncated named anchors resized to true size,
  swallowed fn_/lbl_/except_ fragments deleted. Walls: anchor DELETION is not net-neutral
  (leave bogus anchors in place unless A/B'd); wrong-unit re-pins have no match upside (our
  obj still stubs them) — skip that class.
- **c3 CameraManager +4 (19f36ab): the CameraManager saga is DONE** (waves 7→8→9: probe →
  foundational land +13 → DC3-added-param reverts). NumCameraShots/MakeCategoryAndFilters/
  OnNumCameraShots/OnFindCameraShot all closed via rb3-Wii 2-arg signatures.
- **n2 ServerData +3 (bcd943f):** 20B POD landed (GetPort/GetServerRoot/IsServerLocal).
  Remaining: OnInit dead-spill regalloc (permuter-class), clear-survivor map repair
  (0x1c ICF survivor unlocatable — left).
- **g1 HttpGet +3 (667ca9c):** HttpGet layout reconstructed from retail disasm; closed the
  SafeDisconnect/SafeShutdown/HasTimedOut trio (one root cause as predicted). **Session's
  only regression:** NetLoaderXbox dtor had been COINCIDENTALLY matching against the old
  wrong HttpGet layout; correct layout unmatched it. Net +3, kept. Lesson: a green fn can be
  green for the wrong reason; layout corrections can surface these.
- **sq1 +2 (1bc4139):** GroupSeq dtor relabel + RndPropAnim::Save (vbase placement fixed).

## BaseMaterial/RndMat FULL REORDER — ground truth complete (r1, no patch; WAVE-10 EXECUTE)

The wave-8 "+0x40 shared render-base" hypothesis was INCOMPLETE: it is a **full member
REORDER of BaseMaterial + net +0x40 size** — the "retail keeps rb3-Wii-era ORDER, DC3 both
reordered and added ~64B of next-gen members" drift class applied to the whole material base.
- Chain: NgMat : RndMat : BaseMaterial : Hmx::Object. Uniform +0x40 on RndMat members
  (mDirty retail 0x188 vs ours 0x1c8) because BaseMaterial is exactly 64B too big; but
  FRONT members are genuinely reordered (non-uniform).
- **Retail flattened RndMat ctor = fn_82425998 (??0RndMat@@IAA@XZ), Ghidra-decompiled store
  list** (see wf_60f31d20-521 journal r1 notes for the full offset table): 0x28 int=1,
  0x2c mColor RGBA, 0x3c/0x40/0x44/0x48 scalar block, **0x4c..0x88 mTexXfm Transform
  (EARLY, right after color = rb3-Wii order)**, 0x8c mDiffuseTex ObjPtr, 0x99 byte,
  0xa4 ObjPtr, 0xb0=1.0/0xc0=10.0/0xd0=10.0, 0xd4/0xe0/0xec/0xf8/0x104 ObjPtrs,
  0x128/0x148/0x164 ObjPtrs, 0x144=10.0, 0x170=1.0, 0x180 int=0x12, 0x188 mDirty=3.
  DC3/ours moved diffuse textures ahead of the Transform (mTexXfm LATE at 0x74).
- DC3-only candidates summing ~0x40 (NOT contiguous): mDiffuseTex2 (0x14), mSpecular2RGB
  (0x10), mWorldProjection* block (~0x1c), mBloomMultiplier (4). Confirm each against ctor stores.
- ~35 material-family near-misses (95-99.99) behind this wall. Any edit is rndobj-wide:
  currently-green material fns are green only because they avoid divergent members —
  full set-diff A/B mandatory, expect coupled coincidental-green losses (cf. NetLoaderXbox).
- CAUTION on sub-sizes: r1 assumed ObjPtr=0x14 in BaseMaterial context; wave-8 c2 established
  plain ObjPtr=0xc. RECONCILE from the ctor store gaps before trusting either.

## Session cumulative (waves 5-9, this session)

+112 landed by my waves (24+15+26+22+25); main 11,622 → 12,286 (17.71% → 18.74%) including
heavy concurrent-session progress. Zero unexplained regressions. Pattern proven: probe wave
(root-cause, no patch) → execute wave (land green). Remaining big items: BaseMaterial exec,
generalized truncation sweep (2,765 MID_FUNC fragments audited in wave-6, only partially
harvested), permuter-class residue (MD5 /Od, FP scheduling, register rotations).

<!-- ======== END memory file: project_wave9_2026-07-10.md ======== -->


<!-- ======== BEGIN memory file: project_wave10_2026-07-10.md (3733 bytes) ======== -->

## Archived memory: `project_wave10_2026-07-10.md`

---
name: project-wave10
description: "Wave-10 (2026-07-10): +17 landed (12286->12303, 18.77%); truncation sweep generalized +12 (truncation_audit.py tool); HttpGet closed via /DRB3_HTTPGET_VIRTUAL_DTOR per-TU gate; BaseMaterial deferred 2nd time but spec now BYTE-PRECISE (full offset table, ObjPtr=0xc confirmed, bisect arms proven net-negative) — needs dedicated multi-hour run"
metadata: 
  node_type: memory
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-10: truncation r3 + HttpGet close — 2026-07-10 (wf_089a1f0a-9a2)

**Landed: +17 (12,286 → 12,303 = 18.77%), 0 regressions, 3 commits d36476c/78a144c/286033d.**
Follows [[project-wave9]].

## Landed

- **s3 truncation sweep generalized +12 (286033d):** 11 truncated named-symbol extents fixed
  to full COMDAT length across CameraManager/CharClip/DateTime/Draw/Joypad_Xbox/LightPreset/
  PartAnim/Rand2/UIListState/Voice/rndobj-Utl; tool landed `scripts/truncation_audit.py`.
  Remaining walls: **38 body-DIVERGENT mis-truncations** (truncated AND different — after
  resize they become honest body-port targets) + anon-head leaf-region fragmentation (unmeasurable).
- **h1 HttpGet +3 (78a144c):** poly-vs-nonpoly contradiction resolved — retail HttpGet IS
  polymorphic in the NetLoader_Xbox TU; new per-TU gate `/DRB3_HTTPGET_VIRTUAL_DTOR` in
  objects.json (same pattern family as /DRB3_MAP_0x1C). Closed HttpGet dtor/StartReceiving +
  re-closed the NetLoaderXbox dtor (wave-9's understood regression now properly fixed).
  Residual wall: HttpGet data-model reconstruction (mPath/mIP/mPort/mHeaders).
- g1 +2 (d36476c): FftIpp::SetMode, TrackWatcherImpl::OnMiss. New walls: RndHighlightable
  vbase drift, FlowNode base layout, MSVC intrinsic strcpy null-check codegen, an
  objdiff/dtk branch mis-decode artifact (tooling), FP/GPR regalloc residue.

## BaseMaterial: spec now BYTE-PRECISE, execution deferred to a dedicated run (2nd defer)

b1 spent its budget upgrading r1's spec instead of risking a partial land (both bisect arms
— size-only and order-only — PROVEN net-negative; only full reconstruction works). The
wf_089a1f0a-9a2 journal b1 notes contain the complete spec:
- **Validated sub-sizes:** ObjPtr<T> = 0xc {vptr,mOwner,mObject} — the 0x14 header comments
  are STALE HX_NATIVE annotations; Transform = 0x40; Hmx::Color = 0x10. **ALL BaseMaterial.h
  offset comments are stale — compute from declarations, never trust them.**
- **Full retail offset:content table** 0x28..0x188 from ctor fn_82425998 cross-confirmed by
  NgSpotlightDrawer::BlurRT (temp-material setup): mColor@0x2c, 4-word flag block
  0x3c-0x48, mTexXfm Transform EARLY @0x4c-0x88, mDiffuseTex@0x8c, mNextPass@0xa4,
  float defaults 1.0/10.0/10.0 @0xb0/0xc0/0xd0, ObjPtr run 0xd4-0x104, mColorMod vector
  @0x158 (reserve-3), mMetaMaterial@0x164, mShaderOptions pack=0x12@0x180, mDirty=3@0x188;
  retail RndMat total ~0x18c (ours 0x1cc).
- **DC3-only members to gate (#ifdef RB3_DC3_MAT):** mDiffuseTex2, mSpecular2RGB,
  mWorldProjection* block (6 fields), mBloomMultiplier — use-sites enumerated: BaseMaterial.cpp
  ctor-init ~L42-56, Save ~L86-94, Copy ~L106/129/157-163, Load, SyncProperty; Mat.cpp
  SYNC_MAT_PROP ~L237. Retail Save/Load do NOT serialize them → reconcile stream order too.
- Non-uniform deltas measured (SetRegularShaderConst: -0x40,-0x14,-0x7c,+0x18,+0x4,-0x2c)
  prove genuine reorder. ~41 material-family near-misses behind the wall
  (closeout10/groups/material-family.json). Needs a dedicated multi-hour agent with an
  incremental compile-fix loop + full set-diff A/B; expect coincidental-green losses.

## Session cumulative (waves 5-10): +129 landed, 11,622 → 12,303, one understood regression.

<!-- ======== END memory file: project_wave10_2026-07-10.md ======== -->


<!-- ======== BEGIN memory file: project_wave11_2026-07-10.md (3244 bytes) ======== -->

## Archived memory: `project_wave11_2026-07-10.md`

---
name: project-wave11
description: "Wave-11 (2026-07-10): +34 landed (12319->12353, 18.86%), 0 losses; BaseMaterial byte-exact retail layout LANDED +8 (dedicated no-timebox agent worked); truncation body-port +25; remaining frontier = permuter-class regalloc + vbase-dtor catalog + shrink structs"
metadata: 
  node_type: memory
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-11: BaseMaterial dedicated run + truncation harvest — 2026-07-10 (wf_57e41033-1c3)

**Landed: +34 (12,319 → 12,353 = 18.86%), ZERO losses, 3 commits feae2b7/8035e53/75ffc4a.**
Baseline had moved 12,303→12,319 via concurrent pin-extension batch (4314a53, +16).
Follows [[project-wave10]].

## BaseMaterial LANDED (+8, 8035e53) — the dedicated-run pattern works

Third attempt succeeded because it was framed as the ONLY task with NO timebox + explicit
context-management protocol (persist state to ${'$'}OUT/bm1-state.md, stop-early-clean rule).
7 rndobj files, byte-exact retail layout per the wave-10 spec. Closed: NgMat::SetupShader/
SetupAmbient, WorldCrowd/RndMesh/Spotlight::Mats, RndAmbientOcclusion::IsValid_AOCast,
RndMatAnim::SetFrame, RndShaderSimple::Select. ZERO coincidental-green losses (the feared
cascade never materialized). Residual material walls are permuter-class: SetRegularShaderConst
regalloc cascade, BaseMaterial::Save stack-temp slot, BlurRT local ordering.
**Meta-lesson: for whole-subsystem reconstructions, probe-wave → byte-precise-spec-wave →
dedicated-no-timebox-executor is the proven 3-step pattern (CameraManager needed 3 waves,
BaseMaterial needed 3 waves).**

## Truncation body-port harvest +25 (75ffc4a)

t4 re-ran truncation_audit.py and landed 25 closes (symbols.txt-only patch — most of the 38
"body-divergent" wave-10 classifications were actually truncation-shaped near-misses that
close on re-pin). Remaining truncation-band walls now genuinely: regalloc-permuter cascades,
rlwimi-vs-or instruction selection, real body-structure divergence, signature-mismatch
pairing breaks, our-obj-LONGER cases (not truncation), named-neighbor conflicts.

## Frontier state after 7 waves (what's left, by class)

1. **Permuter-class regalloc band** (biggest residual class): SetRegularShaderConst,
   BaseMaterial::Save, SampleData::Load, Quazal MD5 /Od, FastInvert FP, StoreMenuPanel::Handle
   register swap, Track::Poll rotation, Gem::AddInstance cascade, fpr-reassociation,
   stlport-template-regalloc. → /permute pipeline territory, not hand-crackable.
2. **??_G vbase this-adjust catalog** (wave-6): CharInterest +68, HamListRibbon +428,
   RndScreenMask -148, RndShockwave -724, HamCharacter -96 — per-class vbase layout RE.
3. **Shrink structs** (wave-7 st1): EventTrigger::Anim (retail drops ~6 fields),
   UIListElementDrawState (non-prefix rearrange), FileMerger::Merger, HamCamTransform::
   TransformArea; CharClip +16 (invasive).
4. Tooling artifacts: icf-false-pairing, objdiff/dtk branch mis-decode (one instance).
5. HttpGet data-model reconstruction (mPath/mIP/mPort/mHeaders).

## Session cumulative (waves 5-11): +163 landed by waves; main 11,622 → 12,353
(17.71% → 18.86%) incl. concurrent sessions. One understood regression total (later re-closed).

<!-- ======== END memory file: project_wave11_2026-07-10.md ======== -->


<!-- ======== BEGIN memory file: project_wave12_2026-07-10.md (3717 bytes) ======== -->

## Archived memory: `project_wave12_2026-07-10.md`

---
name: project-wave12
description: "Wave-12 (2026-07-10): +4 landed (12353->12357) — DRAIN SIGNAL for the wave-cadence vein class; 3/4 workers honest no_patch (stale premises, permuter-class verdicts); vbase-dtor trailing-reserve lever landed; 98.415 vector band = compiler remat-vs-spill NOT header-fixable; session waves 5-12 TOTAL +167"
metadata: 
  node_type: memory
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-12: drain signal — 2026-07-10 (wf_003402d8-43c)

**Landed: +4 (12,353 → 12,357 = 18.87%), 1 commit 0149637. Three of four workers returned
honest no_patch — the structural-wall vein this wave cadence exploits is DRAINED.**
Follows [[project-wave11]]. Wave yields across the session: 24,15,26,22,25,17,34,**4**.

## Landed: vbase-dtor trailing-reserve lever (+4, 0149637)

**Mechanism (reusable):** a ??_G/??_D dtor's `subi r31,r3,0xNN` immediate encodes the
Hmx::Object VIRTUAL-BASE subobject offset = sizeof(the class's non-virtual part). DC3-vs-
retail member-set drift moves it. **Fix = reserve an exact trailing `char[N]` block** — pads
the non-virtual part to retail size WITHOUT disturbing any accessed member offset → zero
ripple even on widely-included headers (HamCharacter.h across ~20 units). Closed
??_GHamCharacter, ??_GRndScreenMask, ??_GRndShockwave, ??_DRndShockwave. Remaining
vbase-SHRINK classes (ours-too-big: CharInterest, HamListRibbon) can't use the reserve trick
— they need member removal (invasive).

## Honest no-patch verdicts (respect these — don't re-dispatch)

- **s5:** EventTrigger::Anim and FileMerger::Merger premises were STALE — both already
  correct since the ObjPtr-relayout migration (wave-7's analysis predated re-measurement).
  Re-verify catalogued walls on CURRENT main before dispatching (2nd occurrence of this
  lesson — see PanelDir in wave 4). TransformArea = full-reconstruction rabbit hole.
  UIListElementDrawState = coupled multi-consumer, deferred.
- **e2:** the uniform 98.415/328B vector band (6+ instances) is a **compiler
  remat-vs-spill regalloc heuristic in EH-funclet codegen** of the shared vector-grow
  template — source is byte-identical to the DC3 same-compiler twin, NOT header-fixable.
  Permuter-class. (Uniform-band ≠ always fixable: wave-6's band was struct sizes and PAID;
  this one is regalloc and DOESN'T.)
- **g1:** everything left in the fresh 98-99.9 band is permuter-class FPR/GPR regswaps,
  shared-base ripples, HttpGet data-model reshuffle+enum renumber, or reloc-anchor noise.

## FRONTIER VERDICT after 8 waves (what to do NEXT, not more of the same)

1. **Permuter pipeline** on the regalloc band (SetRegularShaderConst, the 98.415 vector
   band x6, FPR scheduling set, StoreMenuPanel::Handle, Track::Poll…) — per
   [[feedback_fuzzy_gap_needs_permuter]] this is /permute + decomp-synth territory.
2. **Body-port campaigns** on the 85-95 game band (big Handle/OnMsg bodies: EditSetlistPanel,
   TourDescPanel/Provider, Gem::AddInstance) — grind-loop drafts + agentic finisher.
3. **Pin/identification expansion** — concurrent sessions' vein (Handle pin-extension
   batches were adding +16..+390 during this session); more TUs to wire and pin.
4. Rabbit-hole reconstructions if sanctioned as dedicated runs (3-step pattern):
   HamCamTransform::TransformArea, UIListElementDrawState, CharClip +16, HttpGet data model.
5. Tooling: icf-false-pairing + objdiff branch mis-decode artifacts.

## Session cumulative (waves 5-12): +167 by my waves; main 11,622 → 12,357
(17.71% → 18.87%) incl. concurrent sessions (~+568 combined). One understood regression
total (re-closed). ~11.3M subagent tokens across 8 workflows, 0 unexplained losses.

<!-- ======== END memory file: project_wave12_2026-07-10.md ======== -->


<!-- ======== BEGIN memory file: project_wave13_2026-07-10.md (2771 bytes) ======== -->

## Archived memory: `project_wave13_2026-07-10.md`

---
name: project-wave13
description: "Wave-13 (2026-07-10): body-port campaign w/ SONNET workers, +10 landed (12945->12955), 0 losses; 85-96 game Handle band mostly honest permuter-class walls; 3 NEW struct-layout walls surfaced (OvershellPanel +0x14 UIPanel-chain, BandUI/UIManager +0x4C, BandSongMetadata mRating) -> wave-14 targets"
metadata:
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-13: body-port campaign (Sonnet workers) — 2026-07-10 (wf_e796ecd4-d1d)

**Landed: +10 (12,945 → 12,955), 0 losses, commits 5517dce/ee0733c/ac1fc71.** Baseline had
moved 12,357→12,945 via concurrent sessions (+588: beatmatch TU wiring, DC3 stub-ports,
MusicLibrary tail, GuitarController recarve). Follows [[project-wave12]]. First wave with
**Sonnet workers** (7 crack + 1 fable merger, ~1.64M tokens — ~7x cheaper than Opus waves,
comparable wall-classification quality).

## Landed

- EditSetlistPanel::Handle + OnMsg(RockCentralOpComplete) closed (+8 incl. 6 funclets).
- StoreMenuPanel::Poll, AppMiniLeaderboardDisplay::UpdateLeaderboard (+2).
- TrainerGemTab::Draw 95.7→99.98 kept as neutral improvement — **merger caught a false
  closure claim: run_objdiff's rounded headline said 100, report.json normalized scoring
  still counted 4 stack-offset diff_args.** Doctrine: closure claims must be verified via
  report.json, not the CLI headline (now baked into worker prompts).

## The 85-96 game Handle band is mostly NOT body-divergent — it's regalloc

MetaPerformer::Handle (521 regswaps from prologue + 26 STATIC_GUARD_COUNTER), Tour::Handle
(hidden Symbol temp slot 0x50-vs-0x58 → whole-fn r25/r26 swap), CharacterCreatorPanel::Handle
(r28/r29 this-swap + retail procedural-abstraction outlining), PracticePanel::Handle
(permuter sweep found ZERO improving variants). Arm sets/order verified byte-identical to
rb3-Wii oracle in each case. **Lesson: big Handle dispatchers at 87-96 fuzzy are usually
whole-function allocator divergence, not arm drift — check the FIRST mismatch index; if it's
in the prologue, classify permuter-class immediately.**

## New struct-layout walls (wave-14 executes)

1. **OvershellPanel +0x14**: uniform this-adjustor delta (retail 0x4d4 vs ours 0x4e8) in the
   UIPanel(+virtual Hmx::Object)/Synchronizable/MsgSource chain — potentially panel-wide.
2. **BandUI/UIManager +0x4C**: blocks BandUI::Poll AND the 7-fn BandUI 98-99.9 cluster.
3. **BandSongMetadata**: mRating retail 0xb8 vs ours ~0x72 (-70), DateTime +2.
4. ClosetMgr: 3 arms need an unimplemented AssetStore subsystem (360-only Marketplace,
   absent from Wii oracle — genuine new-code port, ~0x4c member @+0x50, ctor 0x825D1A38).
5. GemPlayer::UpdateGameCymbalLanes 96.9 wall (branch-shape+bool-mask, 4/87 instrs).

<!-- ======== END memory file: project_wave13_2026-07-10.md ======== -->


<!-- ======== BEGIN memory file: project_wave14_2026-07-10.md (3053 bytes) ======== -->

## Archived memory: `project_wave14_2026-07-10.md`

---
name: project-wave14
description: "Wave-14 (2026-07-10): +11 landed (13177->13188), 0 losses; BandSongMetadata layout +8, BandUI cluster +3; merger premise-gating caught 2 stale patches (1 superseded, 1 would-regress) — main advancing +200/wave via concurrent sessions; 99.8-99.9 big-Handle band = honest at_limit walls (vbase this-adjust deltas, missed-CSE, scheduler one-offs)"
metadata:
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-14: layout walls + near-miss band — 2026-07-10 (wf_71476213-343)

**Landed: +11 (13,177 → 13,188), 0 losses, commits 8d3f4d52/2e4dc90b/2bc238df.**
Follows [[project-wave13]]. 3 Opus layout workers + 2 Sonnet body workers, ~1.04M tokens.

## Landed
- **l3 BandSongMetadata layout +8 (2bc238df):** closed Handle/Save/Load/IsUGC/IsDownload/
  HasPart + 2 BandSongMgr dtor funclets; zero coincidental-green losses.
- **l2 BandUI +3 (8d3f4d52):** Poll, InComponentSelect, ShouldCheckWipeDone. The +0x4C
  UIManager gap had ALREADY been fixed on main (9a198eb8, concurrent session) — 6/7 cluster
  fns were pre-closed; worker's residual patch still paid.
- **n2 (+0, 2e4dc90b):** RB3_STRIP_CHEAT_HANDLERS gate on auto_fake_fill (BandProfile::Handle
  96.6→99.985, honest residual = this-bias constant class). Retail-parity fix kept.

## Merger premise-gating is now load-bearing (main moves +200 per wave via concurrent sessions)
- l1 SKIPPED: OvershellPanel +0x14 fix fully superseded by concurrent commit.
- n1 REVERTED: Game::Handle already closed by 82f8b6f5 (global MILO_LOG arg-eval lever);
  worker's MILO_FAIL swap would have BROKEN it (-1). Caught by set-diff A/B.
- **Doctrine: merger MUST check each claimed symbol's live normalized% before applying;
  workers MUST re-verify targets at worktree creation (stale-premise guard in prompts).**

## Walls (respect — don't redispatch)
- AppInlineHelp::Handle+ctor (99.9x): vbase this-adjust +16 in InlineHelp field layout
  (UIComponent→RndTransformable→virt RndHighlightable→virt Hmx::Object diamond); localized
  to unexplained 4-byte zero-store @0x16c; blocked on tooling (llvm-objdump can't parse
  these COFFs; ctor not in report.json → no asm_listing). CampaignSongInfoPanel same class +8.
- GemPlayer::Handle 99.86: missed-CSE (retail caches r25-0x43c in a stack slot ~9 uses).
- ProfileMgr::Handle 99.82: one-off scheduler artifact at a RB3_STRIP_CHEAT_HANDLERS boundary.
- CustomizePanel::Handle 98.9 / VocalPart::GetBestHit 98.2: this-bias register-cache +
  scheduler nondeterminism (2 prior opus attempts each).
- ProfilePicture::mUserPicture offset 0x18 vs 0x20 — shared system header, future dedicated pass.
- OvershellPanel::ResolveSlotStates 78.9: body-port (Wii-only TheWiiProfileMgr + deleted call
  fn_8259A4C0) → wave-15 v6.
- Re-identification needed: fn @0x827F2EE8 is some OTHER class's Save (biased this; DateTime+
  3 Strings+2 maps+3 vectors shape), removed from BandSongMetadata unit.
- Tooling note: band.exe .text file offset = VA-0x82000000-0x4800 (rdata/pdata map 1:1).

<!-- ======== END memory file: project_wave14_2026-07-10.md ======== -->


<!-- ======== BEGIN memory file: project_wave15_2026-07-10.md (3114 bytes) ======== -->

## Archived memory: `project_wave15_2026-07-10.md`

---
name: project-wave15
description: "Wave-15 (2026-07-10): +42 attributable (13711->13871 incl +118 concurrent), 0 losses, 5 commits; 30-90 low band PAYS (v5 storeui swept 7 targets +21, v2 scorelabels swept 5 +17); leads: SongSort +4 layout drift blocks verified 99.97 rewrite, StoreOffer::GetData retail-only method, UpdateScrolling IsNet devirtualization; v6 agent stalled 6x (re-run in wave 16)"
metadata:
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-15: low-band body-port — 2026-07-10 (wf_c9ba6e3a-838)

**Landed: +42 attributable (measured baseline 13,711 → 13,871 total; +118 was a concurrent
synth_xbox recarve dcca18aa), 0 losses, commits 4d291328/da2d27ae/35b2be54/00825dba/90989070.**
Follows [[project-wave14]]. 6 Sonnet workers, ~3.2M tokens.

## Key result: the 30-90 game band pays where 85-96 didn't

Sweeps: v5-storeui closed ALL 7 (+21 w/ siblings: StoreMenuPanel OnBack/GetCrumbText/
FinishLoad, QuestFilterProvider::Mat, GetAssetGenderFromSymbol, EditSetlistPanel::MessageOK,
AppMiniLeaderboardDisplay::Update, EventDialogStartMsg ctor). v2-scorelabels closed ALL 5
(+17 w/ siblings: Instarank x3, SongStatusMgr x2 — Locale/Std.h engine edits, zero fleet
losses). v3 SongRecord.h bool field-order swap +2 (merger itself fixed the one layout-ripple
loss with the matching usage swap — merger-as-finisher works).
**Lesson: at 30-80 fuzzy the divergence is real misported/Wii-flavored bodies → Sonnet
closes them; at 85-96 it's mostly allocator residue. Band selection matters more than model.**

## Leads harvested for wave-16 (all being executed there)

1. **SongSort +4 layout drift:** our mDataResults@0x68 vs retail 0x6c (mRankings gap 0x18
   ours+oracle, 0x1c retail). Blocks a VERIFIED rewrite: OnMsg@SongSortByRank should be
   `mRankings[songId]=make_pair(rank,isPercentile)` (NOT oracle's lower_bound+hint-insert;
   Ghidra-confirmed; reached 99.97). Likely same root as MusicLibraryTaskMsg::Save @64.5.
   MusicLibrary::Poll reads unidentified fields @+0x19c/+0x1a0 missing from our header.
2. **StoreOffer::GetData** (0x82781E90) — retail-only method (postdates Wii snapshot),
   called 3x from NewSongNode(StoreOffer*) @31.3 (retail 0x8263D8F0) w/ lazy static Symbols.
3. **UpdateScrolling (8948B @55.3):** retail devirtualizes Player::IsNet() → direct
   `lbz 0x238(r3)` mRemote load (verified win at site 1 of 3). Needs dedicated sectioned
   port (oracle lines ~1339-1975+). Reported status=stuck to orchestrator w/ full notes.
4. InqConditionProgress @97.6: retail NEEDS single-return-with-flag shape (early-return
   oracle shape collapses to 77.5) — **oracle return-shape isn't always retail-correct.**
   Residual = ~12 static Symbol ptrs kept register-resident (regalloc).

## Ops lessons
- v6-banduishell agent stalled on all 6 attempts (180s no-progress each) — group never
  worked; re-dispatched wave-16. Stalls happen: check <failures> in workflow results.
- AT_LIMIT honest: HandlePhraseEnd 94.0 (f11/f13 swap cascade), ResolveAmbiguity 94.7
  (r30/r31 + GPR-vs-FPR caching of 0.1f), both permuter-plateaued.

<!-- ======== END memory file: project_wave15_2026-07-10.md ======== -->


<!-- ======== BEGIN memory file: project_wave16_2026-07-10.md (3791 bytes) ======== -->

## Archived memory: `project_wave16_2026-07-10.md`

---
name: project-wave16
description: "Wave-16 (2026-07-10): +88 landed (14100->14188), 0 losses; StoreOffer TU RECARVE +83 (recipe: pin-extend + Ghidra-verified map identities + retail-only method reconstruction) — recarve is the top lever; SongSort +4 layout fixed (+4); UpdateScrolling 55->73.6 partial (block-placement wall documented); recarve candidates scan = BandDirector 292 / BandWardrobe 186 / VocalPlayer 162 / BandCamShot 158 / VocalTrack 148 -> wave-17"
metadata:
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-16: lead executions → recarve discovery — 2026-07-10 (wf_dd4cf69a-e0e)

**Landed: +88 (14,100 → 14,188), 0 losses, commits 9295dd78/473387bd/f9ab452b/0064e7eb.**
Follows [[project-wave15]]. 4 workers (~1.2M tokens) — best tokens-per-close of the session.

## THE HEADLINE: TU recarve is the top lever (+83 from ONE worker)

s2's primary target was stale (already closed), so it recarved default/StoreOffer instead:
unit 13 → 96/99 matched. **The recipe (full worked example:
~/tmp/closeout16/reports/s2-storeoffer-getdata.md):**
1. Pin-extend the TU's .text in splits.txt to its true end (rest of TU sat in auto_03_* at 0%);
   remove provably-false stale pins (SpeechMgr pin was actually StoreOffer::NumSongs).
2. target_symbol_map.json identities GHIDRA-VERIFIED per address (positional guessing had
   5/30 wrong — never positional).
3. Layout fix w/ blast-radius check (String mReleaseDateStr@0xc4; Wii's DateTime member is
   ctor-local in retail).
4. Retail-only methods reconstructed from Ghidra (YearReleased, Decade, LengthSym, Genre,
   RatingSym, VocalPartsSym, PartRank — the Wii dev branch had rewritten StoreOffer around
   StorePackedOffer; retail kept the DataArray form).
5. Retail-parity rewrites: handler-arm set changes, MILO_NOTIFY drops, dead-static
   reproduction, new virtual IsCompletelyUnavailable BEFORE Cmp (vtable slot +0x54 verified).
Recarve-candidate scan (pinned units by hi90+zero unmatched): BandDirector 292(78hi/212z),
BandWardrobe 186, MeshAnim 186(engine), VocalPlayer 162, BandCamShot 158, Rnd 152(engine),
VocalTrack 148, DirLoader/BandCharacter/HamCamTransform ~130s. → wave-17 runs the top 5 game units.

## Also landed
- s1 SongSort +4 (f9ab452b): the +4 drift root-caused; OnMsg@SongSortByRank closed via the
  verified make_pair rewrite; CharLipSyncDriver::Save FALSE-PIN fixed (0x823795C8 was
  fingerprint-FP'd as MusicLibraryTaskMsg::Save — the real one is UNIDENTIFIED in retail);
  MusicLibrary::Poll closed (+0x19c/+0x1a0 fields identified).
- b1 GemPlayer::FillComplete +1. d1 UpdateScrolling 55.2→73.6 kept (zero-loss partial).

## Walls
- **UpdateScrolling block-placement wall (documented in ~/tmp/closeout16/reports/):** retail
  hoists the phrase-loop exit code (~90 instrs) INSIDE the loop at fn+0xFE0 as the
  static-windowOk guard's fallthrough; ours emits it after the loop. Accounts for 94I+89D
  clusters + ~600-instr regswap cascade. Every source lever canonicalizes identically; goto
  reconstruction blocked by C++ decl-jump rules (Hmx::Color RVO locals). Needs asm-level
  block-placement insight or custom permuter pass.
- BandUI::Handle: retail mixes HANDLE_ACTION_STATIC vs HANDLE_ACTION per call site (~15 sites,
  shared packed guard word) — per-site verification needed.
- ResolveSlotStates: 2 unidentified retail-only fns (fn_8259A4C0 5th Resolve*States-style,
  fn_825BED40 ShowState wrapper) need Ghidra reconstruction.
- ProfileMgr::SaveGlobalOptions: register-pressure mismatch (retail saves r30/r31, ours
  __savegprlr_28) — temp/expression-ordering not isolated.
- StoreOffer leftovers: fn_82781620 (Symbol from "%016llX" songID), fn_82781668 (ShortName
  Symbol compare) — retail-only, unnamed, no callers identified.

<!-- ======== END memory file: project_wave16_2026-07-10.md ======== -->


<!-- ======== BEGIN memory file: project_wave17_2026-07-10.md (3411 bytes) ======== -->

## Archived memory: `project_wave17_2026-07-10.md`

---
name: project-wave17
description: "Wave-17 (2026-07-10): RECARVE CAMPAIGN +212 (14188->14400), 5/5 kept, 0 real losses; BandWardrobe +174 (boundary shift revealed BandCharDesc +74), BandDirector +32 (false pin -> OutfitConfig exposed 2/109); recarve = proven top lever; hi-fuzzy 40B subi-r12 fns are EH funclets (skip); wholesale map re-sort doesn't merge (append-only)"
metadata:
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-17: TU recarve campaign — 2026-07-10 (wf_7c7322b3-3ab)

**Landed: +212 (14,188 → 14,400), 5/5 patches kept, 0 real losses, commits
7589a980/ec0cd881/78404d8e/e3fa6cab/ac839bdf.** Follows [[project-wave16]]. ~1.6M tokens —
best yield-per-token of the whole session. Mixed fable (r1/r2) + sonnet (r3-r5) workers;
BOTH tiers executed the recipe fine (the +174 was fable, but sonnet did clean recarves too).

## Results by unit
- **r2 BandWardrobe +174** (ec0cd881): boundary shift 0x82322DA0→0x82320E00 also captured
  BandCharDesc (+74); removed 4 dead sliver units (RhythmBattlePlayer, HollaBackMinigame,
  MidiVarLen, MoveGraph — 0 matched each).
- **r1 BandDirector +32** (7589a980): +59 in-unit, -27 fn_ via GHIDRA-PROVEN false-pin
  reattribution — tail 0x8228B600-0x8228E468 is actually OutfitConfig (StaticClassName
  @0x8228B480). OutfitConfig now honest 2/109 = fresh wave-18 unit.
- r3 VocalPlayer +2 (phantom duplicate default/VocalPlayer unit dissolved into
  band3/game/VocalPlayer), r4 BandCamShot +3, r5 VocalTrack +1.

## Ops lessons (bake into future prompts)
- **hi-fuzzy 40B fns with subi-r31,r12 prologues = EH unwind funclets** — parent-dependent,
  zero standalone yield. r1's "78 quick closes" premise was all funclets. Screen these FIRST.
- **Wholesale target_symbol_map.json re-sort does NOT merge** (r3's 1575-line rewrite failed
  to apply; merger recovered by computing semantic diff = 3 entries). Map edits: APPEND-ONLY.
- Unit-attribution moves (fn still 100, different unit) are not losses — merger must verify
  by symbol before reverting.

## Follow-ups queued (wave-18 executes)
- OutfitConfig skeletal port (dtor identities pre-characterized in r1 report).
- BandDirector: OnFileLoaded 96.1 (3 concrete items: local-static Symbol, ObjPtrList
  PoolAlloc+Link inline, lwzu loop), Handle 71.7 + SyncProperty 90.9 arm-parity (~+50 w/
  funclets), Dircut machinery recon (DircutEntry + Keys<>::Cross).
- BandWardrobe: 10 identity-proven port-diff bodies (Handle 60.6, SyncProperty 60.0,
  OnEnterVignette, LoadMainCharacters, Save 0.85 188B recon, ...).
- VocalPlayer::Poll 4936B genuinely divergent body (dedicated session); ~128 VocalPlayer
  zeros = per-fn body work, NOT map-gaps (112/112 name parity vs oracle confirmed).
- BandCamShot: CamShot VBASE_WALL stands (documented, skip); fn_822A42F0 region touches
  undocumented offsets 0x1b8-0x1e9 — possible pin overshoot, Ghidra before edit.

## Walls (respect)
- BandDirector: Replace 92.9 (raw RTDynamicCast return, ABI blast radius), SetObj<T>/
  ObjRefConcrete (bodies in forbidden obj/Object.h), OnForcePreset 99.97 (foreign WorldDir
  offset 0x318 vs 0x380), PickDist 99.0 (strcpy-intrinsic, permuter).
- BandWardrobe near-misses: GetShotFlags 96.15 (Category() remat), Load 94.64 (gRev/gAltRev
  needs INIT_REVS global co-location, cross-TU), SelectExtra 98.35, ~40 except_data 32B blobs
  (objdiff excludes $-names, structural cap).

<!-- ======== END memory file: project_wave17_2026-07-10.md ======== -->


<!-- ======== BEGIN memory file: project_wave18_2026-07-10.md (2920 bytes) ======== -->

## Archived memory: `project_wave18_2026-07-10.md`

---
name: project-wave18
description: "Wave-18 (2026-07-10): +105 (14468->14573, 22.24%), 0 unexplained losses; BandDirector arm-parity +56 (new /DRB3_HANDLE_LOCAL_STATIC gate), OutfitConfig port +31; merger selectively reverted a -34 recarve hunk while keeping its patch's +12 (hunk-level A/B works); BandWardrobe 60% Handle/SyncProperty band = honest regalloc (source verbatim-matches oracle)"
metadata:
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-18: recarve tier 2 + body follow-ups — 2026-07-10 (wf_e99be0bf-14d)

**Landed: +105 (14,468 → 14,573 = 22.24%), 0 unexplained losses, commits
642e0971/6f300ddd/9b3e81fe/e3ee07e4/9402d8f4.** Follows [[project-wave17]]. ~1.56M tokens.

## Landed
- **d2 BandDirector +56 (e3ee07e4):** arm-parity pass on Handle/SyncProperty paid as
  predicted (~unit 352→408/516). New per-TU gate: objects.json cflags swap
  /DMILO_MESSAGE_TIMERS → **/DRB3_HANDLE_LOCAL_STATIC** (same gate family as RB3_MAP_0x1C).
- **o1 OutfitConfig +31 (642e0971):** skeletal port landed (2→33/109).
- r7 Player/ContextChecker +12; r6 BandCharacter +4 (adjustor thunk identities); w2
  BandWardrobe +2 (Save reconstruction, SyncPlayMode).
- **Merger hunk-level surgery worked:** r7's ContextChecker splits-recarve hunk measured
  -34/+0 in isolation (dropped accidental foreign-TU funclet pairings) — merger reverted JUST
  that hunk, kept the +12 rest. Doctrine: pin hygiene that drops matches is a net loss;
  re-propose only bundled with a proper carve of the orphaned band.

## Walls
- **Local-static Symbol guard-ordinal trap (r7):** adding retail's function-local static
  Symbols regressed 60→24.7 because MSVC's per-TU guard-variable ordinal numbering depends on
  compile order of ALL static locals in the file — name-based pairing breaks. A local-static
  conversion needs the TU-wide static-local census to line up (that's what the
  /DRB3_HANDLE_LOCAL_STATIC gate does globally per TU).
- BandWardrobe Handle 58.2/SyncProperty 57.8/OnEnterVignette 59.6/LoadMainCharacters 50.5/
  StartClipLoads 71.0/ValidGenreGender 50.4: source verbatim-matches oracle; gap = pure
  regalloc (REGISTER_SWAP across 2-15 pairs). **The 50-70% band can ALSO be regalloc when
  the fn is a macro dispatcher — 'low fuzzy' does not always mean 'body divergence'.**
- BandDirector: LightPresetMgr WorldDir-tail offset +0x68 (retail +0x318 vs ours +0x380) caps
  5 fns → wave-19 wd1 executes; OnGetCatList 11.2 retail-rewrote (decompile saved) +
  OnMidiAddPreset 80.6 → wave-19 d3; Dircut machinery recon still queued.
- OutfitConfig: ~50 zeros = ObjPtr/ObjVector dtor/ctor identity work 0x8228BCD0..0x8228C968;
  fn_8228D7A0/fn_8228DE98 large recons → wave-19 o2.
- BandCharacter fn_8226D630 = TWO 12B adjustor thunks merged in one dtk entry (needs finer
  split boundary, not attempted). Player's 87 zeros unworked (budget went to ContextChecker).

<!-- ======== END memory file: project_wave18_2026-07-10.md ======== -->


<!-- ======== BEGIN memory file: project_wave19_2026-07-10.md (2983 bytes) ======== -->

## Archived memory: `project_wave19_2026-07-10.md`

---
name: project-wave19
description: "Wave-19 (2026-07-10): +23 (14611->14634) — recarve drain curve visible (+212/+105/+23); WorldDir tail layout CLOSED (+5, zero ripple — wave-8 deferred work done); AccomplishmentManager/GemManager zeros = retail-only-member recon (hard); per-instantiation inline-vs-call template wall discovered"
metadata:
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-19: follow-ups + tier-3 — 2026-07-10 (wf_92cddc47-c18)

**Landed: +23 (14,611 → 14,634), 0 unexplained losses, commits 677147f3/4a4f784b/7ffbbe6f/
ea46d70a.** Follows [[project-wave18]]. Drain curve on the recarve vein: +212 → +105 → +23.

## Landed
- **wd1 WorldDir tail +5 (ea46d70a):** LightPresetMgr moved to retail +0x318; all 5
  BandDirector near-misses closed; ZERO tree-wide ripple (consumers use inline accessors).
  The wave-8 "WorldDir tail non-retail-exact = future work" debt is PAID.
- o2 OutfitConfig +15 (677147f3): 31 Ghidra-verified map identities also paired 12
  ObjRefConcrete ??_G thunks. d3 +2 (OnGetCatList 11.2→100 recon, OnMidiAddPreset). r8 +1.
- Merger explained a -2 as borrowed-pairing funclet shift (pre-flagged by worker) — kept.

## New wall class: per-instantiation inline-vs-call template divergence
ObjRefConcrete<T>::Load for RndTransformable/RndTexBlender: retail calls SetObjConcrete(0)
for exactly these 2 instantiations (vbase-adjust cost) but inlines for RndMat/RndTex/
ColorPalette. ONE shared template source cannot reproduce both; rewriting closed 2 but
regressed 3 (reverted, monotonic rule). Fix = explicit Load() specialization per
instantiation — out of scope that pass.

## Honest walls
- r9 no_patch: AccomplishmentManager (231/314) + GemManager (54/134) zeros = genuine
  retail-only class members/features absent from our headers ('strings' on dtk target obj
  proved it), MI adjustor-thunk dtors, EH funclets. High-effort Ghidra recon, no oracle.
- OnLightPresetKeyframeInterp 97.4: wave-18's "offset-capped" premise was PARTLY stale —
  offset component gone, residual is permuter-class (BOOL_MASK clrlwi, regswap trio).
- Dircut recon scoped: DircutEntry struct has NO oracle (retail-only rewrite) — needs
  Ghidra layout derivation first → wave-20 dc1 attempts.
- BandSongMgr fn_8255E0B0 84% = positional-pairing artifact (target is a different fn);
  stray default/BandSongMgr sliver = STL hashtable-destroy, unrelated, no objects.json entry
  (can never match; removal needs A/B proof, not done).
- MatSwap::Compose 77.0 (129 regswaps/21 pairs — dedicated pass), DrawPreClear 2.4 unworked.

## Vein assessment: game-layer recarve residue after this wave = Player 83z (unworked),
TrackPanel 73z (unworked unit), RockCentral 23 mid-band, VocalPlayer 32hi screen + small
zeros, Dircut recon → wave-20. If wave-20 lands thin (<+30), wave cadence on this vein is
done; engine units (MeshAnim 186, Rnd 151/64hi, DirLoader 137) stay deprioritized per
native-port doctrine.

<!-- ======== END memory file: project_wave19_2026-07-10.md ======== -->


<!-- ======== BEGIN memory file: project_wave20_2026-07-10.md (3206 bytes) ======== -->

## Archived memory: `project_wave20_2026-07-10.md`

---
name: project-wave20
description: "Wave-20 (2026-07-10): +10 (p1 +1, dc1 Dircut recon +9) — game-residue vein thin; MAJOR FINDING: target_symbol_map.json is systematically MIS-PAIRED for small generic fns (BinDiff structural collisions) — TrackPanel/RockCentral proven; 'the map is the wall, not the source'; wave-21 = map_verify.py tool + repair campaign"
metadata:
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-20: game residue + Dircut recon — 2026-07-10 (wf_c81fc2bb-ff4)

**Landed: +10 attributable (14,647 → 14,672 incl +15 concurrent PassiveMessenger drift),
commits 888fa3bd/71f0cd5f.** Follows [[project-wave19]]. Drain curve: +212/+105/+23/+10.

## Dircut recon PAID (+9, 71f0cd5f)
dc1 derived BandDirector::DircutEntry from Ghidra with NO oracle and closed Keys<DircutEntry>
::Cross, FindNextDircut, FindNextShot, HarvestDircuts, OnFirstShotOK, PickIntroShot + 4
funclets. One explained -1 (funclet 100→99.9 parent-frame side effect, net-positive trade).
AddDircut/PlayNextShot 99.99 = CamShot vbase-wall residuals. **Dedicated Ghidra struct-recon
with no source oracle is viable for bounded machinery.**

## THE STRATEGIC FINDING: the map is the wall, not the source
Two independent workers (t1 TrackPanel, rc1 RockCentral) proved scripts/target_symbol_map.json
(generated by tools/gen_game_target_map.py from BinDiff unified_id_rb3wii.json) is
systematically mis-paired for SMALL GENERIC functions — structural-similarity collisions:
- TrackPanel: ?PushCrowdReaction → 0x82B5FD98 is an unrelated vtable-dispatch routine (the
  -96.0f constant our source needs appears NOWHERE in the TU's .text);
  ?StaticClassName@FxSendReverb360 @0x82B5E1A8 actually refs "profile_picture_fetched_msg"
  (every lazy-static StaticClassName is byte-identical except 2 data addresses → shape-collision).
- RockCentral: ~21/23 mid-fuzzy + ~10 near-zero = Ghidra-verified mis-pairs (getters mapped
  to guard-word thunks/vtable-install helpers). Source already verbatim-matches oracle —
  NOTHING TO PORT; porting against a mis-pair is wasted effort. Also 9 Microsoft
  D3DXShader/XAudio2/XGRAPHICS ranges mis-pinned into RockCentral.cpp splits.
- The pre-compile obj_target_symbol_renamer BURNS wrong names onto target fn_ symbols →
  objdiff silently compares against the wrong retail fn. Mis-pairs where the TRUE target
  exists unmapped = instant closes when corrected.
→ Wave-21 builds scripts/map_verify.py (size-ratio + string-overlap + callee-count evidence
per entry) + repairs TrackPanel/RockCentral + emits tree-wide flagged summary.

## Player leftovers (p1)
- fn_8268B138 (1024B): dispatcher ref'ing ConnectionStatusChangedMsg/LabelShrinkWrapper/
  LabelNumberTicker — foreign class in Player pin range, identify (wave-21 i1).
- ~40 zero-fuzzy vector<Extent> STL helpers compiled correctly in our obj but target-UNMAPPED
  — size/shape address matching could pair the family (wave-21 i1).
- LocalSetEnabledState 0→89 (10-vs-9 callee-saved GPR cascade, at_limit); PollMultiplier 92.9
  at_limit; 19/20 hi-fuzzy = EH funclets; vector<float> dtor funclet cluster has an
  unidentified parent local type (target +0x40 vs ours +0x70).

<!-- ======== END memory file: project_wave20_2026-07-10.md ======== -->


<!-- ======== BEGIN memory file: project_wave21_2026-07-10.md (3241 bytes) ======== -->

## Archived memory: `project_wave21_2026-07-10.md`

---
name: project-wave21
description: "Wave-21 (2026-07-10): +21 (14711->14732) — scripts/map_verify.py LANDED (tree audit: 7898 entries = 81 MISPAIR / 53 SUSPECT / 378 NO_BASE / 113 BAD_ADDR / 115 EH_AUX); RockCentral repair proved delete-wrong-name unlocks anonymous thunk pairing (+19); tool's MISPAIR verdicts reliable, OK verdicts are NOT proof; Player vector<Extent> lead REFUTED (normalized diff ignores reloc names + ICF steals the address); SyncGameStartPanel port = real lever for Player residue"
metadata:
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-21: map-hygiene tooling + repair — 2026-07-10 (wf_e4fa8e29-02e)

**Landed: +21 (14,711 → 14,732), 0 losses, commit d4609d0a.** Follows [[project-wave20]].
New tool: **scripts/map_verify.py** (per-entry evidence: size ratio, string overlap,
callee counts, opcode sim → OK/SUSPECT/MISPAIR/NO_BASE/BAD_ADDR verdicts; per-unit CSV).

## What landed (m1)
- RockCentral: deleted 35 Ghidra-verified MISPAIR getter names → **19 anonymous fn_8257xxxx
  guard-word thunks instantly paired** (the wrong names had been burned onto them by the
  renamer). +PassiveMessageQueue ctor (OvershellSlot), +RndTransProxy::SetPart (TransProxy).
- 6 RockCentral functions re-addressed to their true targets — now pair honestly at 65-80%
  (real porting work, not closes).
- Microsoft D3DXShader/XAudio2/XGRAPHICS ranges re-pinned out of RockCentral.cpp.

## Tree-wide audit result (campaign map)
7898 named entries: OK 7273 / **MISPAIR 81 / SUSPECT 53** / NO_BASE 378 / BAD_ADDR 113 /
EH_AUX 115. Long tail: top unit OutfitConfig 7, Console 5, AssetTypes 4, then 3s and 2s
across ~80 units. Systematic FP families in the 81: **StlNodeAlloc allocate/deallocate**
(8B inline vs 76-128B target, ~14) and **ObjRefConcrete<T,ObjectDir> ctors** (~9).
Full CSV: ~/tmp/closeout21/reports/m1_all.csv.

## Tool calibration (important)
- MISPAIR verdicts = reliable. **OK is NOT proof**: ??0RockCentral@0x823DFB50 scored OK
  (sim .67) but was Ghidra-verified wrong in wave-20. Ctor-shaped fns without consts need
  Ghidra confirmation.
- BAD_ADDR (113) = stale oracle addresses pointing mid-function/unpinned; NO_BASE (378) =
  names never compiled in our tree (dead weight, harmless).
- Tiny 8B getters are fundamentally ambiguous statically — naming them is
  verifiable-icf-grade, handle with care.

## i1 Player leads: mostly REFUTED (save future effort)
- ~40 vector<Extent> STL helpers: mapping them can gain NOTHING — (a) objdiff normalized
  mode ignores relocation-target-name diffs, callers already 100; (b) the instantiations are
  ICF-folded into a representative already claimed under another type's name
  (Player::Restart's clear() reloc resolves to vector<Vector2>::clear). No free target
  address exists. **Dead lead class: "unmapped STL helper family inside a pin".**
- fn_8268B138 = SyncGameStartPanel::Handle; fn_8268A9E0 = its ctor; 7 MI adjustor thunks
  mostly = SyncGameStartPanel virtuals. **Real lever: port src/band3/game/SyncGameStartPanel.cpp**
  (header exists, retail layout verified) + recarve its .text out of Player's oversized pin.
  Unmerged 2-entry map patch preserved at ~/tmp/closeout21/patches/i1-player-ids.patch.

<!-- ======== END memory file: project_wave21_2026-07-10.md ======== -->


<!-- ======== BEGIN memory file: project_wave22_2026-07-10.md (2936 bytes) ======== -->

## Archived memory: `project_wave22_2026-07-10.md`

---
name: project-wave22
description: "Wave-22 (2026-07-10): +13 (14772->14785) — SyncGameStartPanel carve/port +10; map-repair sweep = mostly UNBLOCK-hygiene not closes (+3 from 68 deletions, drain signal); CRITICAL merger lesson: map DELETIONS need touch config.yml re-split (renamer can't un-rename, stamp-only hygiene shows false net-0); next vein = anon-zero identification in game units (~630 unmapped fn_ across VocalPlayer/VocalTrack/BandCharacter/ContextChecker/Player/BandSongMgr)"
metadata:
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-22: map repair + SyncGameStartPanel — 2026-07-10 (wf_996a0394-dae)

**Landed: +13 (14,772 merge-baseline → 14,785), 0 true losses, commits
6a2ba2e5/0a73e459/7a0bb962.** Follows [[project-wave21]].

## Results
- **s1 SyncGameStartPanel +10 (7a0bb962):** new TU carved out of Player's oversized pin,
  ported from rb3-Wii oracle; ctor/ClassName/Handle/IsLoaded/PollIsSynced + 5 anon closed.
  Follow-on: fn_8268B000/AFB0/B040/B0E0 size-consistent with already-written OnMsg/StartSync/
  CheckIsSynced/SetExternalBlock — decompile+size-match then append map entries.
- **h1+h2 map repair +3 (0a73e459/6a2ba2e5):** 68 Ghidra-verified deletions across ~50 units;
  only 3 instant closes (RockCentral fn_825F1F8C/FAC ex-Stats getters, EventTrigger
  fn_8248C6A0 ex-Anim ctor). **Most mis-pair deletions are unblock-hygiene, not closes** —
  the wave-21 RockCentral +19 was the exception (dense thunk cluster), not the rule.
  DRAIN SIGNAL for map-repair as a close generator. ~26 engine SUSPECT rows left unprocessed
  (deprioritized). Console DataContinue static-fn collisions unresolved.

## Merger operational lessons (bake into prompts)
- **Map DELETIONS require a re-split:** the renamer patches split objs in place and cannot
  un-rename. `rm target_symbol_renames.stamp` alone shows a FALSE net-0; must also
  `touch config/45410914/config.yml` to force dtk to re-emit pristine fn_ objs.
- Transient renamer crash (`struct.error buffer size is 0`) racing a concurrent session's
  split — immediate retry succeeded; renamer could use skip-and-warn on 0-byte objs.
- `git apply` context drift on map patches → semantic re-apply per deletion (verify key still
  present with identical value, delete single line) is the right premise gate.

## Next vein scouted (live report.json after wave 22)
Game-unit zeros are ~95% ANONYMOUS fn_ (identification work, not body work): VocalPlayer
120 anon-z, VocalTrack 128, BandCharacter 106, ContextChecker 90, Player 61, BandSongMgr 63,
BandCamShot 117 (vbase wall, skip). Mechanism proven (OutfitConfig +15/31 identities;
SyncGameStartPanel ClassName). Screens: EH funclets (subi-r12 40B), ICF-stolen STL helpers
([[project-wave21]] vector<Extent> lesson), foreign named zeros inside pins (FlowEventListener
dtors in VocalPlayer, NewObject thunks in ContextChecker = possible pin-boundary issues).

<!-- ======== END memory file: project_wave22_2026-07-10.md ======== -->


<!-- ======== BEGIN memory file: project_wave23_2026-07-10.md (2701 bytes) ======== -->

## Archived memory: `project_wave23_2026-07-10.md`

---
name: project-wave23
description: "Wave-23 (2026-07-10): +1 strict (15399->15400, main jumped +613 from concurrent pin-audit) — CALIBRATION: anon-zero identification alone yields fuzzy near-misses not strict closes (identify->polish is the value path); LEADS: ObjPtrList::Replace virtual-dispatch header RFC (11 uniform-36.5% instances), RemoteVocalState diagnosed 1-line fix, VocalTrack pin blocks 3-5 are Gem/TrackConfig (recarve), VocalPlayer mass map-gap (143 obj symbols vs 36 mapped) — wave-17's 'body work' claim REFUTED"
metadata:
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-23: anon-zero identification campaign — 2026-07-10 (wf_a4fb1846-ae7)

**Landed: +1 strict (15,399 → 15,400), 24 map identities, 0 losses, commit 730ba3b5.**
Follows [[project-wave22]]. ~550k tokens for +1 strict = the one-off identification approach
does NOT pay in strict count. **Calibration: identities land at 19-97% fuzzy; the value is
converting invisible zeros into workable near-misses — identify→polish must be one motion.**

## Key findings
- **VocalPlayer is a MASS MAP-GAP** (wave-17's "per-fn body work, not map-gaps" claim
  REFUTED): our compiled obj has 143 VocalPlayer symbols (full oracle set, compiles clean)
  but only ~36 had retail identities. The ported bodies pair at high fuzzy once identified
  (ctor 97.5%, RemoteVocalState 94.6%) — near-miss quality, not divergent.
- **ObjPtrList<T,ObjectDir>::Replace virtual-dispatch RFC (BIG LEAD):** retail dispatches
  ReplaceNode through a vtable slot; our ObjPtr_p.h:965 calls direct (our comment at :984
  asserts non-polymorphic Node — contradicted by retail codegen). 11 confirmed target
  addresses at uniform ~36.5% fuzzy in BandCharacter alone; a shared-header fix could
  cascade tree-wide. Wide blast radius — needs tree-wide A/B. Evidence:
  ~/tmp/closeout23/reports/c1.md.
- **RemoteVocalState → 100% is a diagnosed 1-line fix** (local-var declaration-order swap,
  stack-layout verified): ~/tmp/closeout23/reports/v1.md.
- **VocalTrack pin recarve:** blocks 3-5 (0x82B78254–0x82B7A2A0) are Gem/TrackConfig code
  (Ghidra-confirmed), not VocalTrack. Recarve into proper splits.
- **ContextChecker pin overreach trap:** 0x82556B40–0x82558EAC is a foreign
  class-registration/factory cluster; trimming the pin costs -34 (bogus byte-coincidence
  matches) — identify the true owning TU BEFORE re-pinning.
- AddSongData 81.2%: retail-only 360 DLC/license-check tail absent from Wii oracle.
- VocalPlayer layout pins from evidence: mSingers word-offset 0xe1/0xe2, mVocalParts 0xe4/0xe5.
- ~90 of VocalTrack's remaining 207 anon zeros are 28-44B EH funclets (screen works).

<!-- ======== END memory file: project_wave23_2026-07-10.md ======== -->


<!-- ======== BEGIN memory file: project_wave24_2026-07-10.md (3153 bytes) ======== -->

## Archived memory: `project_wave24_2026-07-10.md`

---
name: project-wave24
description: "Wave-24 (2026-07-10): +23 (15428->15451) — ObjPtrList::Replace RETAIL SHAPE landed (owner-control vtable dispatch + inline ring walk) + FOUNDATIONAL: ObjRefOwner::Replace family returns VOID in retail (rb3-Wii signature; the bool was dc3-drift) — 85-file flip, +12; VocalTrack/TrackConfig/Gem recarve +9; RemoteVocalState was CSE-hoist not decl-order; FOLLOW-UP VEIN: Replace-family anon COMDATs tree-wide close on identification alone"
metadata:
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-24: leads execution — 2026-07-10 (wf_c1d0aad1-13f)

**Landed: +23 (15,428 → 15,451), 3/3 patches kept, 0 real losses, commit 03116aab (87 files).**
Follows [[project-wave23]].

## o1 ObjPtrList::Replace + ObjRefOwner void RFC (+12) — FOUNDATIONAL
- Wave-23's "polymorphic Node" hypothesis REFUTED: retail's vtable dispatch is the
  kObjListOwnerControl early branch calling mOwner->Replace (ObjRefOwner slot +8) — exactly
  rb3-Wii's shape. Rewrote X360 ObjPtrList<T1,T2>::Replace (src/system/obj/ObjPtr_p.h:964):
  owner-control dispatch + inline ring walk (Release/__RTDynamicCast/AddRef or Unlink+delete),
  no ReplaceNode. 11 instances 37.95→97.5%.
- **The residual 2.5% was bool-vs-void: retail's ObjRefOwner::Replace(ObjRef*,Hmx::Object*)
  family returns VOID (rb3-Wii signature) — the bool return was DC3 DRIFT (newer engine).**
  Flipped pure virtual + ~50 overrides across 85 files, updated 13 map manglings
  (EAA_N→EAAX in-place). Tree-wide A/B: BandCharacter +11, BandDirector +1, all else 0.
- **GENERALIZABLE LESSON: when a shared engine signature differs dc3-vs-rb3Wii, retail RB3
  follows the Wii signature.** Audit other dc3-drift signatures in src/system.
- Tooling hazard: brace-walking body rewriters must skip inactive #ifdef branches
  (PropAnim.cpp overrun, caught by C2561).

## Also landed
- v2 recarve +9: VocalTrack blocks 3-5 → TrackConfig setters/getters + Gem::OnScreen,
  GetSlotColor; SetEngaged@0x82B782D0 was really TrackConfig::SetLefty.
- v1 +2: RemoteVocalState closed — root cause was retail CSE-hoisting a duplicate
  GetSingerIndex() call (our recompute added a stack spill, +0x8 offset shift); NOT the
  wave-23 decl-order hypothesis. Fix = hoist into a local. UnpackFloats identity landed
  (body diverges 37%, 228B vs our 328B).

## Follow-up veins (wave-25)
1. **Replace-family tree-wide identification (map-only, cheap):** every unit's
   ObjPtrList<T>::Replace anon COMDAT should close on identification alone now the body is
   proven. CharMeshHide/RndMeshDeform likely ICF-folded but byte-identifiable.
2. dc3-drift signature audit across src/system shared headers (the void/bool class).
3. VocalTrack block 1: ~77 real-sized unidentified fn_.
4. HandlePhraseEnd 2.9% = genuine divergence, dedicated port (602 mismatched instrs).
5. fn_826C7178→ShowPitchCorrectionNotice: name-embedded addr ≠ real VA (0x826C6628) yet
   scores 100 — trivial-body shape-collision class, audit lead for map_verify.
6. Size-based identification WITHOUT per-fn Ghidra verify = unreliable (3 false positives
   caught in v1).

<!-- ======== END memory file: project_wave24_2026-07-10.md ======== -->


<!-- ======== BEGIN memory file: project_wave25_2026-07-10.md (2906 bytes) ======== -->

## Archived memory: `project_wave25_2026-07-10.md`

---
name: project-wave25
description: "Wave-25 (2026-07-10/11): +23 (15452->15475) — RTTI-probe tooling LANDED (scripts/rtti_probe.py: __RTDynamicCast arg-4 type-descriptor = per-address positive ID), 21 Replace<T> closes incl 'ICF-folded' CharMeshHide/RndMeshDeform (were NOT folds); CALIBRATION: 'retail follows rb3-Wii' is NOT a rule (Memcard MCResult family + PropKeys::SetFrame follow dc3) — verify drift per-case; 182-flag vsig audit list at ~/tmp/closeout25/vsig_all.txt; 15 self-type Replace T's need byte-exact ID method"
metadata:
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-25: Replace sweep + drift audit — 2026-07-10/11 (wf_4554aac0-5f6)

**Landed: +23 (15,452 → 15,475), 3/3 kept, 0 unexplained losses, commit 4ce08136.**
Follows [[project-wave24]]. Merger initially died on 429 quota; resumeFromRunId replayed
workers from cache and re-ran merger only (~70k tokens) — resume works perfectly for
merger-only retries.

## r1 Replace-family sweep +21 — NEW TOOLING LANDED
- **scripts/rtti_probe.py** (+ batch_rtti_probe.py, scan_replace_sizes.py,
  scan_objptrlist_replace.py, find_replace_candidates.py): retail Replace<T> bodies call
  __RTDynamicCast with the RTTI type descriptor as arg 4; parsing the .?AV<Name>@@ string
  gives exact per-address positive evidence. Size-bucket cross-check caught 2 false
  positives pre-landing. **Reusable pattern for any template family with RTTI calls.**
- Wave-24's ICF-fold suspicion for CharMeshHide/RndMeshDeform was WRONG — not folds, closed.
- 15 T's remain: Object/FlowNode/EventTrigger etc are SELF-TYPE (MSVC elides the cast — no
  RTTI signal); need byte-exact comparison of our proven COMDAT bytes vs same-size anon
  zeros instead. Object's 192B bucket spans ~16 units.

## a1 drift audit +1 — CRITICAL CALIBRATION
- DataThisPtr::Replace (2-arg Wii shape) 92→100; Memcard::ShowDeviceSelector param order
  (Wii) 99.7→100 byte-exact; override fn_82518458 identified+ported 0→93.1 (permuter-class
  residual).
- **"Retail follows rb3-Wii" is NOT a rule:** PropKeys::SetFrame 3-arg and the whole
  Memcard MCResult family are CONFIRMED retail=dc3's newer shape. Verify per-case with
  Ghidra before flipping. 182 ranked drift flags: ~/tmp/closeout25/vsig_all.txt (tool
  vsig_diff2.py same dir — copy into repo if the vein pays).
- PropKeys Load(BinStreamRev&) drift untestable until its ~40 anon zeros are mapped.

## v3 VocalTrack +1
GetHarmonyScore closed. Walls: UpdatePitchArrow 33.9 (retail needs __savefpr_26 + 64B
bigger frame — structural), ctor 85.6 (mDir(this) init retail-inlined to single stw =
ObjPtr-relayout epic class), BuildStaticDeployZone 96.3 LikelyFixable (std::min/ternary
at VocalTrack.cpp:2246-2256, run mode=mismatches). ProcessStaticLyrics 97.7 = pure regswap
(skip). Ghidra code_search.py/ghidra-xrefs.py were flaky; ghidra-decompile.py reliable.

<!-- ======== END memory file: project_wave25_2026-07-10.md ======== -->


<!-- ======== BEGIN memory file: project_wave26_2026-07-11.md (2907 bytes) ======== -->

## Archived memory: `project_wave26_2026-07-11.md`

---
name: project-wave26
description: "Wave-26 (2026-07-11): +1 (15475->15476) — TWO VEINS DRAINED: Replace-family ID (14/15 remaining T's = ICF fold-to-one-address OUTSIDE pin coverage, needs splits recarve not map work) and dc3-drift signatures (ALL testable flags verify ours-correct; TU5-era retail tracks dc3 shapes — third calibration flip); vsig_diff.py + flag doc landed in-tree; VocalPlayer worker STALLED 6/6 (re-dispatch); spin-offs: AddSongData TU5 upgrade-rating body-port, MicNull::GetRecentBuf 93.3"
metadata:
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-26: byte-exact IDs + drift round 2 — 2026-07-11 (wf_45a8f540-7d1)

**Landed: +1 (15,475 → 15,476), commits 38ef6787/1df4d78c. ~1.27M tokens for +1 = this
wave-class is DONE.** Follows [[project-wave25]].

## Vein drain verdicts (respect these — do not re-open without new info)
- **Replace-family byte-exact ID: DRAINED.** Only ObjPtrList<Hmx::Object,ObjectDir>::Replace
  @0x8231A850 closed (identified by ABSENCE of __RTDynamicCast — self-type cast elided —
  plus skeleton match vs an RTTI-bearing sibling). The other 14 T's: ICF folds every
  byte-identical instantiation to ONE retail address, and for these T's every instantiating
  unit's pin range is already saturated — the true address lies OUTSIDE current pin coverage.
  Closing them = splits.txt recarve work, not map work. Sibling lead (Unlink family, same
  logic) unexhausted but needs full COMDAT bucket enumeration; proximity heuristic DISPROVEN
  (instantiation points do not cluster physically).
- **dc3-drift signature vein: DRAINED.** Round-2 verdict: every testable flag verifies
  ours-already-correct. THIRD CALIBRATION: TU5-era retail tracks DC3 shapes (round-1's
  Memcard/PropKeys finding was the rule, wave-24's void-Replace was the exception).
  Remaining semantically-real flags (synth/os families) have zero mapped surface — rerun
  recipe AFTER synth/os pin waves: scripts/vsig_diff.py → rank vs near-misses → verify → flip
  only on divergence. Flag doc: docs/decomp/research/vsig-flags-2026-07-11.md.
- Retired leads: PropKeys::Load family (3 overrides byte-100), GetFileHandle, SetADSR.

## Spin-off body-port leads (wave-27 fodder)
- **BandSongMgr::AddSongData 82.67:** ours pairs 1:1; target has 27 extra instructions =
  TU5 upgrade-rating logic (Symbol ctor + map<int,float>::insert_unique +
  hash_map<int,SongUpgradeData*>::operator[]; fn_82709EE0 x2 = operator-new). Port the block.
- MicNull::GetRecentBuf 93.33.
- VocalTrack: BuildStaticDeployZone 96.3 LikelyFixable (std::min/ternary VocalTrack.cpp:
  2246-2256); RebuildHUD 84.0, CreateMarker 87.0, PollLyricAnimations 86.0 undiagnosed.

## Ops
- v4 VocalPlayer worker STALLED all 6 attempts (2nd occurrence of this failure mode after
  wave-15 v6; re-dispatch works). The stall burns the whole group — re-dispatch promptly.

<!-- ======== END memory file: project_wave26_2026-07-11.md ======== -->


<!-- ======== BEGIN memory file: project_wave27_2026-07-11.md (2882 bytes) ======== -->

## Archived memory: `project_wave27_2026-07-11.md`

---
name: project-wave27
description: "Wave-27 (2026-07-11): +0 — near-miss bundle: ALL 6 targets confirmed genuine walls (BuildStaticDeployZone BOOL_MASK, MicNull addr-scheduling, CreateMarker float-vs-int Vector3 store [permuter spin-off], PollLyricAnimations + MatSwap::Compose regswap cascades, AddSongData retail-only tail needing fn_82586AB0/fn_8255F488 IDs + vtable slot 0x40); VocalPlayer worker stalled 6/6 AGAIN (2nd dispatch, prompt-specific) — wave-28 retries on opus with narrowed top-40 scope"
metadata:
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-27: near-miss bundle + VocalPlayer stall #2 — 2026-07-11 (wf_ace6ee13-872)

**Landed: +0 (15,476 unchanged). p1 worker exported an honest EMPTY patch after reverting
every experiment (correct monotonic behavior). Value = diagnostics.** Follows
[[project-wave26]]. ~1.2M tokens; the near-miss polish band on these units is now CLOSED.

## Wall verdicts (all 6 targets — do not re-attempt by hand)
1. **BandSongMgr::AddSongData 81.2:** retail-360-only tail `if(!unk124){vtable[0x40](this,
   songID); if(fn_82586AB0()) fn_8255F488(this,songID);}` — NO oracle. Needs vtable dump
   @0x8209cd1c + identify the 2 fns first (wave-28 f1 executes).
2. VocalTrack::BuildStaticDeployZone 96.3: BOOL_MASK bool-materialization, 2 shapes tried.
3. MicNull::GetRecentBuf 93.3: 2-instr address-computation scheduling, 3 shapes tried.
4. **VocalTrack::CreateMarker 86.2 root-caused:** float-vs-int (GPR bit-copy) store choice
   on Vector3-by-value passthrough after inlined SetLocalXfm memcpy; SetLocalPos overload
   regressed to 83.3. Spin-off: permuter pass on Vector3-by-value ctor call boundaries.
5. PollLyricAnimations 84.3: whole-fn register-pressure cascade (58/75 diff_args).
6. MatSwap::Compose 78.8 (task #30 CLOSED as wall): 150/171 regswap cascade; 2 real diff_op
   sites (idx 161, 257: bare `b` vs our `bl GetColor@ColorPalette`) = possible merged-call
   divergence — only lead if ever revisited.

## Ops: VocalPlayer stall is PROMPT-SPECIFIC
Same sweep prompt stalled 6/6 on sonnet in BOTH wave-26 and wave-27 (while a sibling sonnet
worker ran fine concurrently). Wave-28 retries with model=opus + narrowed scope (top-40 by
size, explicit small-step instructions). If that also stalls, run it as a direct Agent call
outside Workflow.

## Strategic state after waves 25-27 (drain curve +23/+1/+0)
Census (docs/plans/tu-wiring-census-2026-07-10.md, concurrent session) confirms: TU-wiring
and pin veins DRAINED; 92% of unpinned mass = XDK middleware (not targets); remaining game
mass = (a) VocalPlayer-class map-gaps, (b) permuter-class near-misses (skip per user),
(c) big divergent bodies (Poll/HandlePhraseEnd/UpdateScrolling — dedicated sessions),
(d) body-port completion inside wired units (WinSockSocket 13 fns #2-ranked, Part.cpp
RndParticleSys residue).

<!-- ======== END memory file: project_wave27_2026-07-11.md ======== -->


<!-- ======== BEGIN memory file: project_wave28_2026-07-11.md (2993 bytes) ======== -->

## Archived memory: `project_wave28_2026-07-11.md`

---
name: project-wave28
description: "Wave-28 (2026-07-11): +25 (15476->15501) — NetworkSocket_Win body-port +19 (census WIRED-INCOMPLETE class PAYS; lever: read raw dtk .s to find shared-return shape 70->100); VocalPlayer opus retry WORKED no stall (+3, 14 IDs, vein drained — remaining top-40 anons are sibling/nested-class not VP methods); AddSongData closed +3 (extern-decl for oracle-less callee is fine for call-site codegen)"
metadata:
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-28: body-port residue — 2026-07-11 (wf_d8086293-cb8)

**Landed: +25 (15,476 → 15,501), 3/3 kept, 0 losses, commits 15c9c48a/9aa0a67d/379f1696.**
Follows [[project-wave27]]. ~517k tokens — best yield since wave-24.

## n1 NetworkSocket_Win +19 — census body-port class VALIDATED
19/21 WinSockSocket/NetworkSocket fns 0→100. Levers worth reusing:
- **Read the raw dtk target .s directly** (build/45410914/asm/<unit>.s): revealed retail's
  Send/SendTo use a single shared `return ret;` (not early-return + literal -1) — that one
  restructure took both 70→100 (register preservation across WSAGetLastError()).
- Header layout bugfixes matter: winsockx.h sockaddr_in.sin_zero[8], WSASYS_STATUS_LEN=128.
- Fixed stale mis-pair 0x8251E7D0 (DmDebugFree → NetworkSocket::Create).
Walls: Fail 99.85 (branch-polarity, 4 rewrites tried, at_limit), RecvFrom 97.06 (register
choice, at_limit); ??_E thunk uncapturable (except_data symbol straddles split boundary at
0x8251E860 — no clean byte boundary).

## v4 VocalPlayer +3 — opus broke the stall; vein DRAINED
14 evidence-based IDs (anon zeros 116→102): dtor/GetPracticeHitPercentage/HitCoda at 100.
**Remaining top-40 anons are NOT VocalPlayer methods:** sibling/nested-class ctors (foreign
vtables 0x820efb24/0x820ef5e4 vs VP's 0x820ee55c), dc3-drift volume-button handlers inlined
in our port (no standalone symbol), message stubs for a sibling class (param_2-0x450 cast).
FindBestPart 98.2 (2 swapped loads — best permuter candidate) + SendVocalState 96.4 =
permuter-class; CSE-hoist made SendVocalState WORSE (reverted) — the RemoteVocalState lever
does NOT generalize blindly. Layout anchors: mSingers 0x384/0x388, mVocalParts 0x390/0x394,
mTrack 0x308; globals DAT_82dd0c34=TheGameMicManager, DAT_82dd0c98=TheSongDB.
COMDAT-order↔address auto-alignment tried and abandoned (too noisy).

## f1 AddSongData +3
Closed via vtable-slot resolution + reconstructed tail; **extern declaration for the
oracle-less callee (fn_82586AB0 'RB3AddSongDataUpgradeGate', compares rb1_dlc/ugc/rb3_dlc/
ugc_plus Symbols) is sufficient — normalized diff only needs call-site shape.** fn_8255F488
identified enough to call. Separate walls: BandSongMgr::Handle 83.7, GetPosInRecentList stub.

## Next (wave-29): census leftovers — small game TUs (TexLoadPanel 5, CartRow 3,
KickPlayerMsg 4), the census's 157 compiled-not-pinned reveal candidates, VocalTrack/
BandCharacter identification continuation.

<!-- ======== END memory file: project_wave28_2026-07-11.md ======== -->


<!-- ======== BEGIN memory file: project_wave29_2026-07-11.md (2870 bytes) ======== -->

## Archived memory: `project_wave29_2026-07-11.md`

---
name: project-wave29
description: "Wave-29 (2026-07-11): +64 (15501->15565) — census leftovers over-delivered: TexLoadPanel new-TU port +27, compiled-not-pinned REVEAL PINS +36 (38 additive splits lines; vein NOT drained, 108 candidates left); NEW WALL CLASS: duplicate-name COMDAT twins (second target instance re-pairs our single copy against wrong twin — AccomplishmentSetlist hunk reverted, Character Lod-vector accepted -1); KickPlayerMsg was already closed; CartRow oracle-mismatch (DC3 String vs retail containers, no port)"
metadata:
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-29: census leftovers — 2026-07-11 (wf_08183ed8-651)

**Landed: +64 (15,501 → 15,565), commits 9ad1e4b6/24eb0aca/56a514c6, 1 explained loss.**
Follows [[project-wave28]]. ~560k tokens.

## What paid
- **t1 TexLoadPanel +27:** new TU (src/band3/meta_band/TexLoadPanel.{h,cpp}) — 27/41 fns at
  100 incl. 15 boilerplate thunks that register once pinned. Micro-lever: removing an
  explicit (int) cast on size() made the compare cmplw (unsigned) matching retail.
- **r1 reveal pins +36:** 38 pure-additive splits.txt lines across 22 units (compiled-not-
  pinned census entries). **Vein NOT drained: 108 candidates remain at 0%** (need
  identification/body work, not just pins). Worker's census regen tool:
  scripts/tu_wiring_census_r1.py (worktree-local; 153 live vs doc's 157).
- c2 BandCharacter +1 (9 IDs; GameOver closed).

## NEW WALL CLASS: duplicate-name COMDAT twins
A new pin exposing a SECOND target instance of an identical COMDAT symbol makes objdiff
re-pair our single compiled copy against the wrong twin (old byte-equal instance goes
unpaired). Hit twice: AccomplishmentSetlist::GetType (hunk REVERTED, net -1) and
Character's Lod-vector copy-ctor (kept, net +2 for the hunk). **Repair = map-rename the
duplicate entry** (follow-up lead). Same-name multi-instance pins need this check.

## Walls / leads
- CartRow: DC3 oracle field types WRONG (String vs retail container types w/ vtable dtors)
  — offsets coincide but types diverge; needs container RE before source. Correct no-port.
- KickPlayerMsg family: ALREADY 100 in wired SessionUsersProviders.cpp (census stale).
- TexLoadPanel operator== 25%: dtk merges two 0-frame leaves into one .fn block (no .pdata
  between) — jeff/dtk fix needed, not map-fixable. FinalizeTexturesChunk 91.5: target has
  extra String temp in MILO_WARN else-branch (WARN-stripping interaction, flagged).
- Boundary conflicts dropped cleanly: PropAnim 0x82418EF0 (fn straddle), QuestFilterPanel
  0x82B4D2E0 (except_data straddle both directions).
- Near-miss targets inside new pins: RhythmDetector 99.94, Mesh 97.67,
  AccomplishmentManager::GetAccomplishment 77.0, CharBones::Zero 14.81.
- VocalTrack block-1 (task #42) untouched; pickup notes ~/tmp/closeout29/reports/c2.md.

<!-- ======== END memory file: project_wave29_2026-07-11.md ======== -->


<!-- ======== BEGIN memory file: project_wave30_2026-07-11.md (2950 bytes) ======== -->

## Archived memory: `project_wave30_2026-07-11.md`

---
name: project-wave30
description: "Wave-30 (2026-07-11): +35 (15565->15600) — reveal round-2 +30 (59 more pins via automated dtk-boundary fix loop scripts/_fix_split_loop.py; census 122->53 left); twin repairs landed; VocalTrack +5 via MILO_DEBUG #undef; JEFF MIS-NEST HAZARD LIVE: a zero-map-coverage pin silently corrupts an unrelated 100% neighbor's target asm — only full matched-SET diffing catches it; leads: fn_82B717A0 UpdateTubePlates 772B, GetType guard-thunk family, Mesh operator>><Face> BinStreamRev layout"
metadata:
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-30: reveal round 2 + twins + VocalTrack — 2026-07-11 (wf_c0011fc7-2e0)

**Landed: +35 (15,565 → 15,600), 3/3 kept, 0 regressions, commit 987ed27.**
Follows [[project-wave29]]. In-tree tooling: scripts/tu_wiring_census_r2.py,
scripts/_fix_split_loop.py (iterates dtk SPLIT boundary errors to convergence — widens to
dtk's authoritative symbol end or drops colliding candidates), worklist JSON
scripts/_census_compiled_not_pinned_r2.json (53 remaining entries).

## r2 +30
- Both duplicate-twin repairs landed (Character Lod-vector: deleted dup map entry;
  AccomplishmentSetlist::GetType: wrong-identity entry deleted — 0x8243F220 is a static-init
  guard-clear, real GetType=0x825CBE28 `return 10` — then wave-29's reverted pin re-added).
- 59 more additive pins (census 122→53). 26 groups skipped (>700B boundary uncertainty).
- **JEFF MIS-NEST HAZARD CONFIRMED LIVE (project_jeff_asm_misnest):** one Text.cpp pin with
  zero map coverage silently corrupted an unrelated 100% neighbor's target asm via
  mis-nested .fn/.endfn. Raw count masks it — ONLY full matched-set diffing catches it.
  Additive pin batches MUST set-diff, never count-diff.

## v5 VocalTrack +5
dtor/scalar-dtor/ClearAllTubePlates/~TambourineGemPool + 1 bonus fn from a MILO_DEBUG
#undef in VocalTrack.cpp. Leads left: **fn_82B717A0 = UpdateTubePlates candidate (772B,
now tractable post-MILO_DEBUG gating)**, Poll 97.66, PollKaraoke 94.03 (struct-offset
drift this+0x305), GetCurrentPlate 86.23.

## p2 net-0 (explained)
Removed 3 phantom AccomplishmentManager units; TexLoadPanel String-temp fix = fuzzy-only.
Wall verdicts: RhythmDetector _M_create_node = STL-instantiation map mispair (3 entries,
3 contradictory sizes); Mesh operator>><Face> 97.67 = suspected BinStreamRev
composition-vs-inheritance layout (foundational header, HIGH blast radius, dedicated pass
only); CharBones::Zero 14.81 = identity CORRECT, memset intrinsic-expansion compiler wall;
BandSongMgr::Handle = permuter-class big-Handle.

## Leads for wave-31+
- 53 compiled-not-pinned entries (worklist JSON in-tree) + 26 skipped >700B groups.
- GetType@Accomplishment{,PlayerConditional,SongFilterConditional} — same guard-thunk
  mis-identity shape as the fixed Setlist twin (17.5% fuzzy each).
- fn_82B717A0 UpdateTubePlates port; PollKaraoke +0x305 offset drift.

<!-- ======== END memory file: project_wave30_2026-07-11.md ======== -->


<!-- ======== BEGIN memory file: project_wave31_2026-07-11.md (3239 bytes) ======== -->

## Archived memory: `project_wave31_2026-07-11.md`

---
name: project-wave31
description: "Wave-31 (2026-07-11): +23 (15600->15623) — compiled-not-pinned census DRAINED (2 non-actionable residuals); MAP-RELOCATION RULE CORRECTED: moving a name = DELETE old entry + add new (duplicate identical names silently break per-unit pairing — 'append-only leave old' was WRONG); PollKaraoke closed (wave-30 drift framing was backwards: OUR extra Wii guard, not retail's); FOREACH-macro iterator-copy lever; residue map: next veins = *::Handle body-port class (BandSongMgr 84.8/Campaign 93.7/VocalTrack 76.8) + TrackPanel 59z/Player 53z ID passes"
metadata:
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-31: reveal round 3 + finishers + scout — 2026-07-11 (wf_d58f9adc-021)

**Landed: +23 (15,600 → 15,623), 3/3 kept, 0 regressions, commits 5dc54599/8ac8f034/9e30e60c.**
Follows [[project-wave30]].

## Rules corrected/learned (bake into prompts)
- **Map relocation = DELETE old + add new.** Leaving a dead address mapped to the SAME name
  as a newly-identified address silently breaks objdiff per-unit pairing (it may pick the
  dead instance → misleading "still 17.5%" result). Prior "append-only, leave old entry"
  notes were WRONG for relocations. (Append-only still applies vs wholesale rewrites.)
- **ID-before-layout:** cross-check offset-shifted accessors against an already-100%
  serializer in the same TU before assuming a struct-layout bug (Stats::GetBandContribution
  closed on 3rd attempt this way).
- **FOREACH-macro re-materializes deque-iterator copies** inside loops; explicit hoisted
  iterator for-loop (decl order matters) → GetCurrentPlate 73.5→99.99.
- Wave-30's PollKaraoke "+0x305 drift" framing was BACKWARDS: retail has NO guard; OUR code
  had an extra `if(!unk2e5)` from the Wii oracle. Genuine Wii-vs-360 behavior divergence;
  verify against raw target asm before trusting a drift diagnosis.
- dtk can REJECT a byte-correct candidate fn ("Not a function" control-flow analysis) —
  such addresses can't be pinned/paired at all (GetType@SongFilterConditional wall).

## Census vein CLOSED
2 residuals, both non-actionable (dtk-vs-Ghidra boundary disagreement; a data symbol).
Basename-collision "Missing configuration" warnings (Movie.cpp/Synth.cpp/...) are a
pre-existing tool limitation, not wave damage.

## Residue map (s1 scout — the wave-32+ plan)
- **\*::Handle body-port class (the next real vein):** BandSongMgr::Handle 84.77 (2552B,
  8 insert/delete clusters, DataArray dispatch-table reconstruction), Campaign::Handle 93.7
  (4996B, identified+mapped this wave, same class), VocalTrack::Handle 76.8 (LikelyFixable).
  Wave-18's BandDirector arm-parity (+56) is the method precedent.
- **TrackPanel 59 zeros ≥48B, Player 53 zeros** — fresh ID passes (near-miss counts ~0
  there; it's identification not polish).
- EditSetlistPanel::Exiting 95.4 = permuter-class (2 restructurings tried+reverted; shared
  epilogue BOOL_MASK). ContextChecker 66z = template noise wall (do not re-hunt).
- 90-99.9 band tree-wide is DOMINATED by EH funclets; real near-misses = 1-3 per unit.
- Walls: TubePlate sizeof (RndMesh +0x110→+0xdc chase, high blast radius); Poll InRollback
  BOOL_MASK accepted at 97.66.

<!-- ======== END memory file: project_wave31_2026-07-11.md ======== -->


<!-- ======== BEGIN memory file: project_wave32_2026-07-11.md (2895 bytes) ======== -->

## Archived memory: `project_wave32_2026-07-11.md`

---
name: project-wave32
description: "Wave-32 (2026-07-11): +5 (15623->15628) — VocalTrack::Handle ROOT-CAUSED (wrong per-TU MessageTimer override inflated frame 0xc0->0xf0; retail is timer-OFF; 76.3->98.4 correctness fix); BandSongMgr/Campaign Handle = STACK-LAYOUT walls (emergent slot assignment, not arm-parity); TrackPanel +5 IDs + 6 revealed near-misses (Handle 97.2 best); Player's ~51 anons = GemPlayer CODE ISLAND needing splits carve-out (SyncGameStartPanel precedent); ID-mangling from own COFF table eliminates typo risk"
metadata:
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-32: Handle class + TrackPanel/Player IDs — 2026-07-11 (wf_2c674b04-773)

**Landed: +5 (15,623 → 15,628), 3/3 kept, commits 4fe980e/cf13d6a.** Follows
[[project-wave31]]. ~497k tokens. Drain curve on ID passes: +64/+35/+23/+5.

## h1 Handle class verdicts
- **VocalTrack::Handle 76.3→98.4 (root-caused, landed):** source carried a WRONG per-TU
  MessageTimer BEGIN_HANDLERS override (re-added timer) inflating frame 0xc0→0xf0 and
  shifting all locals. Target disasm proves retail is timer-OFF. Residual = 1 scheduler
  artifact (permuter can't target macro-generated Handle). Net-0 count (ICF funclet
  shuffle, explained) but real correctness+fuzzy win. **Check for spurious MessageTimer
  overrides in other TUs with low-scoring Handles.**
- **BandSongMgr::Handle 83.6 = WALL:** coupled r29/r30 swap (~87 instrs) + frame Δ+0x10
  (extra spilled DataNode temp) + systematic 12B r26-offset delta. Emergent regalloc.
- **Campaign::Handle 93.5 = WALL:** NOT arm-reorder — Symbol sret temp slot 0x68-vs-0x80
  uniform shift + 3-way slot permutation + swapped pair. Emergent stack-slot assignment.
  Both need a dedicated stack-layout wave (if ever); not body-port.

## t2 TrackPanel +5, 6 revealed
Mangled names extracted from our own COFF symbol table (no typo risk — adopt this).
Revealed near-misses: **Handle 97.2 (best target; Symbol-relocation off-by-one chain
across 5 dispatch sites, arm order verified identical to oracle — build-env/ordinal
artifact, needs /compare-asm)**, ctor 76.9, Reset 69.1, Poll 57.1, PushCrowdReaction 53.7,
AutoVocals 11.7 (all CONFIRMED pairings, body work). Foreign residue: StandardEffect dtor
thunk mispair @0x82B5E0B8 + Loader-subclass cluster 0x82B5E100-0x82B5E720 — unit .text
start may need re-derivation.

## p3 Player: GemPlayer code island
DisablePlayer ID landed (94.5 fuzzy, f30/f31 swap residual). **~51 remaining anons are a
disjoint GemPlayer island inside Player's pin — needs a splits.txt carve-out campaign
(SyncGameStartPanel +10 precedent), not map edits.**

## Wave-33 queue
1. GemPlayer island carve-out (Player pin → GemPlayer.cpp unit extension).
2. TrackPanel body-ports (ctor/Reset/Poll/PushCrowdReaction) + boundary re-derivation.
3. AccomplishmentPanel scout (task #45, never run).

<!-- ======== END memory file: project_wave32_2026-07-11.md ======== -->


<!-- ======== BEGIN memory file: project_wave33_2026-07-11.md (3051 bytes) ======== -->

## Archived memory: `project_wave33_2026-07-11.md`

---
name: project-wave33
description: "Wave-33 (2026-07-11): +10 (15628->15638) — AccomplishmentPanel +9; TrackPanel Reset closed + Poll 57->80; GemPlayer island GROUND-TRUTHED to 2 fns (51-fn hypothesis wrong; false positive was Player::Poll itself — set-diff self-caught); NEW FLEET-WIDE LEAD: dtk PDATA-less leaf-thunk boundary defect (tiny accessors absorbed into neighbors' ranges or symbol-less; caps PushCrowdReaction 53.7, AutoVocals unfixable) = candidate jeff fix, pairs with project_jeff_asm_misnest +50-200 estimate"
metadata:
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-33: carve + ports + panel scout — 2026-07-11 (wf_fcda601e-89c)

**Landed: +10 (15,628 → 15,638), 3/3 kept, commits feb5f5bc/6a8329da/14f98b97.**
Follows [[project-wave32]].

## THE LEAD: dtk PDATA-less leaf-thunk boundary defect (jeff fix candidate)
dtk bounds a function's range by the NEXT PDATA-REGISTERED function start, not real code
boundaries. Tiny leaf accessors with no unwind info (cntlzw/extrwi bool-materialize idioms,
lbz/blr getters) either get absorbed into the preceding fn's declared range (PushCrowdReaction
53.7% ceiling: its 144B declared range swallows 2-3 unrelated thunks after its real ~76B end)
or exist with NO symbol entry at all (AutoVocals' real body at 0x82B60D80 is symbol-less; the
mapped 0x82B60D50 is a StaticClassName forwarder = unfixable mispair, renamer has nothing to
rename). **Transferable fleet-wide: pairs with [[project_jeff_asm_misnest]] (+50-200 est).**
Fix locus: jeff (../jeff) xex splitting — synthesize symbol boundaries for PDATA-less leaves.
CAUTION for any jeff swap: configure.py bakes the absolute path to ../jeff/target/release/dtk
into main AND all worktrees — stage the binary, A/B in a worktree via configure.py --dtk
override, swap only with byte-gate evidence + backup (wibo staging discipline).

## Landed
- a3 AccomplishmentPanel +9 (map + 1-line source). Residue: ~33 no-target fns,
  Goal_HandleButtonDownMsg 83.4, Text@AccomplishmentProvider 73.5; LaunchGoal = TU-wide
  STATIC_GUARD_COUNTER wall.
- t3 TrackPanel: Reset closed (+1); Poll 57→80 (residual = frame-pointer register cascade:
  target does subi r31,r1,0xc0 dedicated FP + __savegprlr_22 vs our _23 — dedicated
  investigation class); ctor confirmed AT_LIMIT via closed ObjPtr two-ctor wall (do not
  re-hunt); PushCrowdReaction source CONFIRMED structurally correct (ceiling is the dtk
  defect above); AutoVocals mispair root-caused (same defect).
- g1 GemPlayer carve: net-0 attribution + 2 real IDs (OnGetGemIsSustained 48%, ctor 2.5% =
  member-init-list gap, both future polish). **The ~51-fn island hypothesis was WRONG** —
  ground-truthing shrank it to 816B/2 fns; a 608B false positive was Player::Poll itself
  (call-site pattern misread; fresh decompile + oracle cross-check caught it pre-landing).

## Wave-34 queue
1. jeff PDATA-less boundary fix (staged, worktree-A/B'd, gated swap).
2. EditSetlistPanel 39z / Campaign 38z ID passes.
3. AccomplishmentPanel finisher.

<!-- ======== END memory file: project_wave33_2026-07-11.md ======== -->


<!-- ======== BEGIN memory file: project_wave34_2026-07-11.md (3568 bytes) ======== -->

## Archived memory: `project_wave34_2026-07-11.md`

---
name: project-wave34
description: "Wave-34 (2026-07-11): +21 (15638->15659) — JEFF DTK LEAF-SYNTHESIS FIX LANDED (gated swap performed; jeff commit a670a12; 2266 synthesized leaf symbols = NEW IDENTIFICATION SURFACE, 646 parents clamped); AutoVocals 11.7->100 proof-of-value; symbols.txt now dtk-regenerated + committed (0f1be30b); INCIDENT: in-place cargo build in ../jeff while main built = main symbols.txt contamination (repaired via git-show restore); follow-ups: merge_tail_blocks companion (~+60), vtable-slot-driven leaf identification, EditSetlistPanel foreign-TU contamination"
metadata:
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-34: jeff boundary fix — 2026-07-11 (wf_d7231182-ab0)

**Landed: +21 (15,638 → 15,659), 0 losses. Commits: 8a463e13 (e1 +2), e6a8465d (j1 map +3),
0f1be30b (symbols.txt regen), jeff a670a12 (the dtk fix source).** Follows
[[project-wave33]]. ~1.8M tokens — worth it: the dtk fix is a durable capability.

## The jeff fix (now LIVE fleet-wide)
- Pass `synthesize_reloc_targeted_leaf_functions` in jeff src/cmd/xex.rs (post-CFA):
  promotes/synthesizes PDATA-less tiny leaves (getters, bool-materialize, this-adjust
  thunks) absorbed into oversized parents or left as bare lbl_. Safety: reloc
  proof-of-entry, T-4 flow terminator, pdata partition authoritative, **only DATA/vtable-
  sourced candidates may split a parent** (keeps re-split idempotent; the broad variant
  scored +77 but broke idempotency via merge_tail_blocks re-merging).
- Gated swap protocol worked: staged binary → independent fresh-worktree validation
  (+16/0) → backup → install → main A/B (+16/0, identical) → convergence proof
  (symbols.txt byte-identical across further re-split).
- **2,266 synthesized leaf symbols + 646 clamped parents = new identification surface**
  (many read false-0% pending map entries). AutoVocals 11.7→100 proof-of-value.
- Jeff source committed (a670a12) AFTER the wave by the coordinator — a cargo rebuild
  produces byte-41 (build-id) differences; the validated staged bytes were restored over
  the fresh build (validated-bytes discipline). Backup: ~/tmp/closeout34/dtk-backup-*.
- **symbols.txt is now dtk-regenerated output committed to the repo (0f1be30b)** — a stale
  checked-in symbols.txt + leaf-synthesizing dtk = build-breaking mix; keep it in sync
  after dtk changes.

## INCIDENT (ops lesson)
j1's in-place `cargo build --release` in ../jeff put the modified binary at the
configure.py-referenced path DURING the wave; something built in main in that window and
dtk rewrote main's symbols.txt (7,501-line uncommitted diff) → baseline split failure.
Merger repaired via git-show restore (no checkout). **Rule for future tooling workers:
NEVER build to the configure.py-referenced binary path — build elsewhere and copy to a
staging path** (the prompt said this; the worker built in-place then restored, but the
window existed).

## Follow-ups
1. **Vtable-slot-driven identification of the 2,266 leaves**: vtable identity + slot index
   = method name (strong evidence even for 8B getters). scripts/dump_vtable.py exists.
2. jeff merge_tail_blocks companion fix (~+60, riskier, dedicated pass).
3. EditSetlistPanel pin contamination: EventDialogPanel + DeJitterPanel TUs confirmed
   inside [0x825FE180,0x82603030) — needs ports + re-derived boundary.
4. e1's 10 Campaign IDs paired at 31-97% = polish targets.
5. a4 walls: gPrefabIsCustomizable lwz/lbz width (do NOT re-attempt int swap),
   SaveGlobalOptions = permuter spill wall.

<!-- ======== END memory file: project_wave34_2026-07-11.md ======== -->


<!-- ======== BEGIN memory file: project_wave35_2026-07-12.md (3098 bytes) ======== -->

## Archived memory: `project_wave35_2026-07-12.md`

---
name: project-wave35
description: "Wave-35 (2026-07-12): +42 (15660->15702) — vtable-slot leaf identification WORKS (+27 across 2 workers; $4 adjustor thunks via slot-alignment + tail-call-identity chaining); EventDialogPanel TU carved out of EditSetlistPanel +15; WALLS: no-vtable-in-our-build blocks the method on ~8 units (need body-ports to emit ??_7, or tail-call chaining); $4 thunk names = ONE COMDAT per name (2nd retail addr = dead weight); MI/vtordisp RTTI-slot-count-vs-header gap (RndOverlay::Callback 2 declared vs 9 slots); NEXT: panel-family drain (SongSelect/Patch/Store*), jeff merge_tail_blocks companion"
metadata:
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-35: leaf IDs + panel decontamination — 2026-07-12 (wf_3da036c8-01f)

**Landed: +42 (15,660 → 15,702), 3/3 kept, commits db4ce286/8d117ed3/4dbcbcba, 0 real
losses (10 explained: 8 unit-renames + 2 pre-declared boundary coincidental breaks).**
Follows [[project-wave34]]. Workflow was paused overnight; resumeFromRunId replayed the
finished worker from cache cleanly (2nd successful pause-resume).

## The leaf-ID method (validated, reusable)
- **Vtable-slot alignment:** synthesized leaf referenced from a vtable → decode ??_7 owner
  + slot index → method name, cross-checked against our header's declared virtual order.
- **Tail-call-identity chaining:** a retail thunk's tail-call target being an
  already-mapped method identifies the thunk WITHOUT needing our compiled vtable.
- Tools left in wt-l1 (unreviewed): scripts/vtable_id_batch.py, vtable_id_helper.py.
  Classified candidate corpus: ~/tmp/closeout35/classified_l1_final.json (392 A-M leaves
  by asm shape: GUARD_CLEAR/PURE_LEAF_TINY/HAS_CALL/OTHER).

## Walls
- **No-vtable-in-our-build:** BandSongMgr, MetaPanel, GemManager, CalibrationPanel,
  GemTrack, BandUI, AppMiniLeaderboardDisplay, EditSetlistPanel emit no ??_7 in our objs —
  slot method blocked until body-ports instantiate them; chaining still works.
- **$4 adjustor-thunk names are one-COMDAT-per-name:** mapping a 2nd retail address to a
  consumed name = 0 net (verified + reverted). GUARD_CLEAR leaves = guard-ordinal trap
  (skip). ContextChecker 31 HAS_CALL = field-offset/functor vein (different method).
- **MI/vtordisp layout gap:** RndOverlay::Callback declares 2 virtuals but VocalPlayer's
  RTTI secondary vtable has 9 slots — real header gap, needs RTTI-slot-count-vs-header
  audit (layout-affecting).

## e2 EventDialogPanel +15
New TU carved from EditSetlistPanel's span [0x825FE180,0x82603030) with port; remainder:
14 EH adjustor thunks + 4 RB3_HANDLE_LOCAL_STATIC blocks (permuter-class). DeJitterPanel
portion NOT completed (check e2 report for boundary state).

## Wave-36 queue
1. Panel-family leaf drain: SongSelectPanel, PatchPanel, StoreInfoPanel, StoreMainPanel,
   PatchSelectPanel (same declared-virtual-order check) + TrackerDisplay 4-fragment lead.
2. jeff merge_tail_blocks companion fix (~+60, staged+gated like wave-34).
3. Campaign 10 near-misses (31-97%) + l2's 13 fuzzy candidates — polish pass.

<!-- ======== END memory file: project_wave35_2026-07-12.md ======== -->


<!-- ======== BEGIN memory file: project_wave36_2026-07-12.md (3224 bytes) ======== -->

## Archived memory: `project_wave36_2026-07-12.md`

---
name: project-wave36
description: "Wave-36 (2026-07-12): +70 (15702->15772) — jeff merge_tail_blocks companion LANDED (+62, jeff commit c8b21dd; ~1.8k MORE synthesized leaves at false-0% = next ID surface); re-split convergence caveat: up to 3 re-splits to byte-stable symbols.txt from warm state (matched set stable throughout); $4-adjustor-thunk vein DRAINED (3 sessions hit same wall — do not re-hunt); panel drain +4; Campaign polish +4; mispair lead 0x82574688=BandProfile::HasSeenHint"
metadata:
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-36: panels + merge_tail_blocks + polish — 2026-07-12 (wf_750ae7e7-60b)

**Landed: +70 (15,702 → 15,772 matched_functions), 3/3 kept, 0 losses, commits
b9c0e564/bbb873a3/a5fd11b1; jeff source commit c8b21dd (coordinator, post-wave, validated
staged bytes restored over fresh build).** Follows [[project-wave35]].

## j2 merge_tail_blocks companion (+62, swap performed)
- merge_tail_blocks now respects pre-seeded distinct function symbols across persisted
  split boundaries → leaf pass relaxed to bl-referenced candidates too.
- **Merger caught a real caveat j2's report missed:** from a WARM build dir, symbols.txt
  takes up to 3 re-splits to reach its byte-stable fixed point (deterministic trajectory;
  matched SET identical at every round; splits.txt unchanged; fixed point = the patch's
  symbols.txt). From clean state it converges in 1. **After any symbols.txt-perturbing
  change, run up to 3 re-splits before judging stability; judge by matched SET.**
- **~1.8k newly-synthesized fn_ leaves read false-0% pending map identities — the next
  big identification surface.** Only 62 with pre-existing map entries realized this wave.
  Gains concentrated: Stats +8, TrackConfig +4, RndBitmap/RenderState +3 each.

## l3 panel drain +4 — $4-thunk vein DRAINED
PatchPanel::Enter + 3 TrackerDisplay SendMsg. Wall consensus (3rd consecutive session):
$4 mangling encodes base class + displacement only, our compile emits ONE COMDAT per name
→ chain targets keep dead-ending on consumed names. Six wall patterns catalogued in
~/tmp/closeout36/reports/l3.md incl: TrackerDisplay's 4 leaves = ONE ctor's MI vtable-init
fragments (not methods); secondary 21-slot vbase-table column theory DISPROVEN (do not
reuse); zero compiled ??_7 in the whole game obj tree (retail RTTI decode is the only
vtable oracle). **Do not re-hunt this list without a new evidence source.**

## c3 Campaign polish +4
Local-static hoist closed OnMsg(PrimaryProfileChangedMsg) + GetNextHintToShow + 2 funclets.
Flagged mispair: 0x82574688 IsUnlockableAsset@AccomplishmentManager → actually
?HasSeenHint@BandProfile (fix in a map-hygiene batch).

## Wave-37 queue
1. **Leaf-ID sweep round 2 over the ~1.8k new synthesized leaves** — but note the $4/
   GUARD_CLEAR walls; the workable subset is code-referenced leaves with decompile
   evidence + retail-RTTI vtable slots. Consider a bulk classifier first (asm-shape
   corpus like classified_l1_final.json) to size the honest surface.
2. Mispair fix 0x82574688 + any map_verify re-run against the new symbol population.
3. GemPlayer ctor/OnGetGemIsSustained probes (c3 didn't reach them).

<!-- ======== END memory file: project_wave36_2026-07-12.md ======== -->


<!-- ======== BEGIN memory file: project_wave37_2026-07-12.md (3101 bytes) ======== -->

## Archived memory: `project_wave37_2026-07-12.md`

---
name: project-wave37
description: "Wave-37 (2026-07-12): +38 (15772->15810) — classify-first structure WORKED (honest surface: 96 workable of 1791 headline leaves; 28% TRUNCATED_FRAGMENT = bogus dtk boundaries, skip-class); Singer +9 / AccomplishmentProgress +24 via FIELD-OFFSET DRIFT-MODEL identification (+0x14 flat drift corroborated from 3 mapped siblings); ICEBERG: 1211 leaves in unclaimed auto_* gaps need splits-bootstrap not identification; leads: Stats +12B layout drift (unlocks getters), IncrementTrillsHit mispair, m1.patch unmerged, ~65 worklist entries unevaluated"
metadata:
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-37: leaf sweep round 2 — 2026-07-12 (wf_e69e0893-2b5)

**Landed: +38 (15,772 → 15,810), 2/2 manifest patches kept, 0 losses, commits
04433a97/7d64344b.** Follows [[project-wave36]]. Classify-first workflow structure paid:
the classifier killed the "1.8k surface" myth cheaply before workers burned tokens.

## Classifier truth (corpus = 1,791 net-new fn_ from wave-36 dtk)
- In real game/engine units at 0%: only 279 (game 125 / engine 154). **1,211 (68%) sit in
  unclaimed auto_* gap units** — need splits-bootstrap (pin ranges to owning TUs) before
  ANY identification applies. 209 more not wired into any unit.
- Shape classes: PURE_LEAF 169, HAS_CALL 31, **TRUNCATED_FRAGMENT 77 (28%) = genuinely
  bogus dtk split boundaries (no terminator in claimed range; real tail in next symbol) —
  SKIP-class**, GUARD_CLEAR/CTOR_FRAGMENT ~0 in this corpus.
- Workable = 96 with traceable call sites in source-mapped units.

## The new identification technique: FIELD-OFFSET DRIFT-MODEL
i1 closed 24 AccomplishmentProgress + 9 Singer getters by establishing a corroborated
drift model (flat +0x14 for the ScoreType/Difficulty array region, derived from 3
independent already-mapped siblings, cross-validated on 23 offset computations) and
matching decompiled member offsets against class headers through it. Reusable for any
TU with known layout drift.

## Leads (wave-38)
1. **Stats layout drift:** retail mHarmony at 0xbc vs ours 0xb0 (+12B before mAccuracy,
   likely 3 vectors 4B narrower each) — fixing unlocks GetHarmony 99.5 + several getters.
2. **m1.patch UNMERGED** (~/tmp/closeout37/patches/m1.patch): 0x82574688 mispair fix
   (IsUnlockableAsset→HasSeenHint@BandProfile), worker-verified 0/0 — needs config.yml
   re-split on land (value change).
3. IncrementTrillsHit@Stats mispair: claimed at 0x82679188 (offsets inconsistent with
   drift model); real body 0x826791D8 — delete+re-add.
4. Duplicate SetSkinColor@BandCharDesc at 0x82447130 (pre-existing) — verify or delete.
5. ~65 worklist entries unevaluated (MusicLibrary, GemTrack, SongDB, SongStatusMgr,
   TrackPanel, VocalTrack, Gem, NetGameMsgs...) — finish the slice.
6. auto_* iceberg sizing: how many of the 1,211 are <0x82800000 AND have compiled-body
   candidates? (Cheap python scout before committing a wave.)
7. i2 veins: fn_82798278 MemOrPoolFreeSTL sibling cluster; JoypadData 0xd4-stride table
   (dc3 layout 8B newer).

<!-- ======== END memory file: project_wave37_2026-07-12.md ======== -->


<!-- ======== BEGIN memory file: project_wave38_2026-07-12.md (3831 bytes) ======== -->

## Archived memory: `project_wave38_2026-07-12.md`

---
name: project-wave38
description: "Wave-38 (2026-07-12): +2 (15810->15812) — Stats '12B layout drift' premise REFUTED (Stats.h codegen-correct, // 0xNN comments stale; real cause = 2 MISPAIRS: GetHarmony was GetCodaPoints, IncrementTrillsHit was IncrementSustainGemsHit — fixed); map_verify BLIND to same-signature-shape mispairs (manual Ghidra offset audit found them); AddRoll = retail .pdata splits one 36B body into 2 symbols (jeff-class, at_limit); f2 iceberg worker died on API error (resumed); s2 self-caught+reverted a direct-main-edit violation"
metadata:
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-38: Stats + hygiene — 2026-07-12 (wf_617f8b62-c20)

**Landed: +2 (15,810 → 15,812), commits e13345b3/89c5e986, 0 losses.** Follows
[[project-wave37]]. f2 (worklist finisher + auto_* iceberg sizing) died on an API
ZlibError pre-start — workflow resumed to run it.

## Premise refutations (respect these)
- **Stats has NO layout drift.** The std::vector members compile to correct 12B STLport
  layout; header `// 0xNN` comments are stale docs with zero codegen effect. Wave-37's
  "+12B before mAccuracy" inference came from comparing decompiled offsets against STALE
  COMMENTS, not real codegen. **Lesson: verify header layout claims against an
  already-100% sibling's codegen, never against offset comments.**
- Both 99.5% Stats fns were MISPAIRS at adjacent addresses (GetHarmony@0x826790B0 was
  actually GetCodaPoints; real one at 0x826790C0. IncrementTrillsHit@0x82679188 was
  IncrementSustainGemsHit; real at 0x826791D8). **map_verify.py is BLIND to
  same-signature-shape adjacent mispairs** — only manual same-TU Ghidra offset audits
  catch them. New audit class for near-misses at 99.5+: check the NEIGHBORING addresses.
- Stats.cpp identification is COMPLETE: all 33 emitted symbols mapped; 5 more retail fns
  (GetCodaPoints/GetTambourine/GetSustain/FailedNoScore/IncrementSustainGemsHit) have no
  emitted bodies in our obj (no callers wired) — unscoreable until a caller exists.

## Walls / tool notes
- AddRoll@Stats 0%: retail .pdata splits its 36B body into fn_826791B0(16B) +
  fn_826791C0(20B, zero xrefs); ours emits one contiguous fn — jeff boundary-derivation
  class, at_limit.
- scripts/extract_decomp_symbols.py parse_coff() undercounts (453 raw vs ~330 parsed on
  Stats.obj) — use raw strings scan for bulk enumeration until fixed.
- h2: HasSeenHint fix landed; duplicate SetSkinColor resolved (0x82447130 deleted, real
  unnamed setter there = open lead member +0xf4); map_verify re-run: 0 NEW mispairs vs
  closeout37 (110-address set unchanged).
- s2 process note: worker mistakenly edited MAIN's map directly mid-task, self-caught via
  git diff, reverted via inverse Edit (not checkout), verified byte-identical. The
  guardrails held but the failure mode exists.

## f2 resumed run: +6 more (final wave total +8, 15,810 → 15,818, commit 9984b0a7)
SongStatusMgr cached-score getters x3, TrackerManager::HandleRemovePlayer,
GetTrackPanelDir, GetTourValue. Wall: RestartGameMsg ctor = inlining wall; ~12
VocalTrack/VocalPlayer/GemManager candidates blocked on undocumented struct-tail members.

## ICEBERG VERDICT (f2, drives wave-39)
Of 1,211 auto_* net-new leaves: **249 game/engine (<0x82800000), 962 XDK (ignore)**.
Distance-to-nearest-pin histogram: 96 <256B, 88 <1KB, 60 <4KB, 5 further. **184/249 (74%)
within 1KB cluster onto 73 already-source-mapped units (~2.5 addrs each) = tail/head slop
of KNOWN units → cheap splits boundary-extension pass** (per-unit verification required,
not blind widening — some units already have multiple disjoint spans). 65 farther
fragments → fingerprint bootstrap later. 10-sample decompile: uniformly real C++, no
padding. Full cluster table in ~/tmp/closeout38/reports/f2.md.

<!-- ======== END memory file: project_wave38_2026-07-12.md ======== -->


<!-- ======== BEGIN memory file: project_wave39_2026-07-12.md (2612 bytes) ======== -->

## Archived memory: `project_wave39_2026-07-12.md`

---
name: project-wave39
description: "Wave-39 (2026-07-12): +0 (pin-hygiene only, 31 units boundary-verified) — ICEBERG DEFLATED: 55% of f2's claimed auto_* leaves are NOT real function starts (Ghidra snaps decompile requests to the enclosing fn — compare the decompiled FUN_ signature address to the claimed address as the rigorous test); all landed extensions cover UN-PORTED source (0% fuzzy, no compiled bodies) — closes require body-ports; dtk's 'ends within symbol' error = the authoritative boundary oracle (primary over Ghidra); global overlap-scan vs full splits.txt mandatory (caught Key.cpp/Color.cpp cross-TU fuse)"
metadata:
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-39: boundary extension — 2026-07-12 (wf_2ebaa754-742)

**Landed: net 0 strict (15,818 unchanged), commits 503341df/a0cdc8c0 — pure pin hygiene:
4 verified widenings (x1) + 27 units of widenings/new spans (x2), 0 losses.** Follows
[[project-wave38]].

## Why the iceberg deflated (methodology lessons — bake into future prompts)
- **Ghidra decompile-at-address SNAPS to the enclosing function.** 81/146 (55%) of f2's
  claimed leaf addresses failed the rigorous test: decompile, then compare the decompiled
  function's own FUN_XXXXXXXX signature address to the claimed address. Mismatch = not a
  real function start.
- **dtk report.json (pdata-derived) is the authoritative size/boundary oracle**; where
  Ghidra disagreed, dtk was right. And dtk's "ends within symbol" compile error is the
  authoritative boundary-fix signal — use it as the PRIMARY technique, not fallback.
- **Global overlap-scan against the FULL splits.txt is mandatory** before adding spans —
  caught a cross-TU mis-merge (Key.cpp leaf actually inside Color.cpp's span; Ghidra
  auto-analysis fused across the TU boundary).
- Gap-size risk cliff ~500B: leaves further than that from a pin are unverifiable without
  blind-widening; skip.
- **Every newly-pinned leaf is UN-PORTED source (0%/null fuzzy)** — this vein produces
  body-port TARGETS, not closes. 5 units have zero real leaves (MicClientMapper,
  CharServoBone, ADSR, RhythmDetector, RockCentral — drop from vein).

## Wave-40 fodder
- x1's 3 trivial one-line accessors, ready for quick port+ID (report x1.md).
- x2's 27 units of newly-pinned small fns (game units: BandProfile, GemPlayer, SongData,
  EventTrigger, Accomplishment*, BandUI, CheatProvider, EndingBonus, Metronome,
  StreakMeter, TourSavable...) — small-method port targets.
- 65 far fragments (>500B gaps, ~35 units) = fingerprint identification pass, LOW priority.

<!-- ======== END memory file: project_wave39_2026-07-12.md ======== -->


<!-- ======== BEGIN memory file: project_wave40_2026-07-12.md (3598 bytes) ======== -->

## Archived memory: `project_wave40_2026-07-12.md`

---
name: project-wave40
description: "Wave-40 (2026-07-12): +4 (15818->15822) — WAVE CADENCE CLOSED (drain curve 37-40: +38/+8/+0/+4 at ~1M tokens each); 14/15 micro-port candidates walled: 4 wave-39 pins are TRUNCATED FRAGMENTS (no blr in span — boundary re-derivation needed), 6 struct-layout gaps (BandProfile 3.2KB unmodeled tail, GemPlayer foundational mUser+0x30 drift, Accomplishment offset INSIDE a modeled field), splits hygiene bug (two MoggClip.cpp stanzas); remaining value = deep-grind classes only"
metadata:
  type: project
  originSessionId: a58f6e37-d8c8-480d-9163-a5958ba05572
---

# Wave-40: micro-ports — 2026-07-12 (wf_94092f95-d3f) — FINAL WAVE of this cadence

**Landed: +4 (15,818 → 15,822), commits 840506a2/368c60e2/c6aa0f92, 0 losses.** Follows
[[project-wave39]]. ~1.39M tokens for +4 = the cheap-close frontier is EXHAUSTED.

## Closes
BandSongMetadata::IsRanked (pure ID gap — source existed), TourSavable::IsNameUnchecked,
HeldButtonPanel ~vector<PressRec> + ~ActionRec.

## Wall census from the 14 rejections (the honest map of what remains)
1. **Truncated-fragment pins (wave-39 defect):** DrivenPropertyEntry@0x8275CAB0,
   Metronome@0x826D1F88, AccomplishmentProgress@0x82565EE8, MeasureMap@0x827AB6F0 — no
   terminating blr in pinned span; pieces of larger unidentified fns. Boundary
   re-derivation from retail pdata needed.
2. **Struct-layout gaps (high-effort recon class):** BandProfile (target 0x7c70 vs modeled
   end 0x6fc0 — 3.2KB unmodeled tail), MoggClip (0xc1 vs 0x90), Accomplishment (target
   offset 0x4d INSIDE mDynamicPrereqsNumSongs — layout WRONG not incomplete), EndingBonus
   (0x200 conflicts mSucceedTrig/mResetTrig), StreakMeter (+0x26c), GemPlayer ctor
   (foundational mUser+0x30 drift, high blast radius).
3. Map mis-pair candidate: GemPlayer::OnGetGemIsSustained@0x82688DD0 (zero semantic
   overlap — BinDiff shape-collision class).
4. **Splits hygiene bug: TWO stanzas named MoggClip.cpp** (system/synth/ vs bare) on
   disjoint ranges.
5. dtk label≠body mismatches: BufStream@0x827A7104 (true 0x827A70C0),
   CheatProvider@0x823E1AF0 (true 0x823E1AC0).
6. ProfileMgr::SetSynapseEnabled: dtk splits retail body into 4B/12B COMDATs around
   except_data — split-boundary tooling class.

## STRATEGIC CLOSE-OUT (waves 13-40, this session)
**+928 attributable strict closes across 28 waves; main 12,799 → 15,822 (19.5% → 23.98%);
ZERO unexplained losses.** Every cheap vein was found, worked, and drained: body-port
residue → recarve (+212 peak) → map hygiene → anon identification → census/reveal pins →
Handle class → dc3-drift → Replace family → jeff leaf synthesis (2 dtk fixes landed,
fleet-wide) → leaf identification → boundary extension → micro-ports.

**What remains (all deep-grind, NOT wave-cadence material):**
- Permuter-class near-misses (hundreds; ~1-3 per unit tree-wide) — BLOCKED on permuter
  per user directive; best candidates cataloged per-wave (FindBestPart 98.2 etc).
- Big divergent bodies: VocalPlayer::Poll 4936B, HandlePhraseEnd, UpdateScrolling,
  BandSongMgr::Handle + Campaign::Handle (stack-layout class) — dedicated sessions each.
- Struct-layout recon (list above + AccomplishmentManager/GemManager retail-only members).
- Engine units (MeshAnim 186, Rnd 151, DirLoader 137...) — deprioritized (native port
  gets engine from DC3).
- jeff boundary/pdata work: truncated-fragment re-derivation, AddRoll-class pdata splits,
  ProfileMgr except_data COMDAT splits — tooling sessions.
- XDK (5,000+ fns) — permanently out of scope, no oracle.

<!-- ======== END memory file: project_wave40_2026-07-12.md ======== -->


<!-- ======== BEGIN memory file: project_crack_farm_deploy_2026-07-09.md (6627 bytes) ======== -->

## Archived memory: `project_crack_farm_deploy_2026-07-09.md`

---
name: project_crack_farm_deploy_2026-07-09
description: "2026-07-09: crack-farm FIRST DEPLOYMENT — live on 2 rented vast.ai boxes (crackfarm-rb3xenon-a/b), zero new code, reused existing decomp-synth B2 farm infra end-to-end"
metadata: 
  node_type: memory
  type: project
  originSessionId: fe599317-6faf-4a30-a027-2f05b4f041c9
---

2026-07-09: owner approved shipping the retail XEX to rented boxes ("this is our box,
vastai encrypts the host, it's in a public github repo anyway") and asked to actually
USE the CPU of 2 large boxes already running. Deployed a first real crack-farm run
using ONLY existing decomp-synth infra — zero new tooling built (RFC-12/21's "TO-BUILD"
whole-TU engine + scheduler are still future work; this is a v0 proof using the
existing PER-FUNCTION `crack_live.py --from-frontier` cracker at real fleet scale).

**Boxes used (both pre-existing, running OTHER workloads — never launched via
launch_train.sh, so no auto-CPU-farm/train.sh babysitting):**
- `44292979` "w1-base-arm-5090-v2": 4x5090, vllm/vllm-openai image, 256 real cores,
  392G free RAM, load ~1.6 (idle). Confirmed image-compatible (py3.10.12/glibc2.35
  match the axolotl eval-env bake) via a live `batch_validate --limit 2` smoke test —
  it actually compiled rb3-xenon and found a real improvement (HDCache::OpenHeader
  83.1%->90.5%) on box hardware. Got the PRIMARY band: [80,97) current_percent,
  245 fns / 6 units, RUN_ID=crackfarm-rb3xenon-a, EVAL_JOBS=32.
- `44294325` "run:weightzoo-reader-01": 2x5090, pytorch image, 256 cores but only 64
  cpu_cores_effective, 50G free disk. Got the smaller [50,80) confidence band, 115
  fns / 6 units, RUN_ID=crackfarm-rb3xenon-b, EVAL_JOBS=16.

**Mechanism (100% reuse, verified against live code before touching boxes):**
decomp-synth's `tools/vast/eval-env/` already bakes rb3-xenon as a first-class
target (tars tracked files + untracked `orig/`+`decomp.db`, configures+ninjas+
self-tests INSIDE the bake container — nothing builds on rented compute). The
LATEST env (`env-20260708-0322-71f8d744`) already had rb3-xenon self-tested OK.
`onstart/eval_sidecar.sh` in `EVAL_MODE=farm` pulls+verifies+unpacks that env to
`/workspace/eval` then delegates to `onstart/farm_worker.sh`, which pulls a B2
manifest (`farm/<RUN_ID>/inbox/manifest.json`), runs `kind=crack` units serially
through `crack_live.py --from-frontier ...` (LLM-free `--generator patterns`),
and pushes each unit's output db + a DONE marker to `farm/<RUN_ID>/results/`
(marker written LAST = crash-safe idempotency). Yield fence (nice19/ionice3/
oom800/cgroup) keeps it off the paying workload's way automatically.

**Key mechanical finding (not in RFC-21):** `select_near_misses()` in
`crack_live.py` has NO offset/pagination (`ORDER BY current_percent DESC LIMIT N`
only) — you CANNOT split one score-band into multiple same-range units without
duplicate work. Fix used: partition into NARROW NON-OVERLAPPING current_percent
sub-bands (queried decomp.db for the real histogram first) so each unit's range
is naturally disjoint. This is the necessary trick for anyone driving
`--from-frontier` at fleet scale until a real offset/claim mechanism exists.

**Corpus capture, already free:** each crack unit's local `crack.db` (climb_variant-
shaped: pattern/delta/won per attempt) gets pushed to `results/<unit>/` by
farm_worker.sh automatically — so this run ALREADY produces training-shaped data
on B2 with no new capture code, just not yet in the NDJSON/candidate-record shape
RFC-21 designs for a real flywheel (that's still TO-BUILD, T4 corpus_stream.py).

**⚠ INCIDENT: credential exposure.** The Box-A launch command used
`pgrep -af eval_sidecar` after an inline `bash -c "... export B2_APPLICATION_KEY=... ...`
— the FULL env-embedded command line (including B2_KEY_ID + B2_APPLICATION_KEY
plaintext) was echoed back into the tool output and is now in this session's
transcript. Blast radius: the key is scoped to only the `decomp-synth-runs`
bucket with no bucket-create/list-all rights (`no_check_bucket=true` in the
rclone remote) — so worst case is read/write/delete within that one bucket, not
account-wide. Fixed for Box B (wrote the env file via a quiet heredoc, no
`pgrep -af`, no echoed secrets). **Owner should decide whether to rotate the B2
application key** (Backblaze console -> App Keys) given the plaintext is now in
a Claude session transcript on disk.

**Status at end of this deployment turn:** both sidecars launched via
`nohup setsid ... &; disown` (fully detached, survives SSH disconnect). Box A
showed `FARM_STATUS RUNNING` ~90s after launch. A monitor is watching for first
`results/*.json` on both boxes. Neither box will self-destruct (EVAL_STANDALONE
was NOT set) — they'll run their 6-unit manifest once and go idle; nothing
currently auto-relaunches them (no train.sh babysitting these two pre-existing
boxes), so a fresh manifest + re-launch is needed for the next wave.

**RESULTS (pulled 2026-07-10, addendum written into RFC-21 doc):** all 12 units
completed on both boxes. 182 fns scored → **0 genuine closes** (36 "cracked" =
stale decomp.db already-100s, 2 = 99.99% reloc-noise starters); 12 improved
w/o closing (best +8.1/+7.2/+6.4; statement_reorder dominates). Per RFC-21 §7
go/no-go this **leans KILL for the per-fn pattern-family config** — vocabulary
is the bottleneck, not CPU. Do NOT rescale this config. Converges with E1b's
whole-TU CONCLUSIVE KILL (0/24) — see [[project_wave2_e1_autoid_2026-07-09]];
both arms agree: fund proposal coverage, not CPU. Gotchas: farm_worker only pushes crack.db JSON
(improved sources NOT recovered — cheap local re-run recovers the 12 movers);
manifests must band off FRESH report.json (20% of budget hit already-matched
fns). Boxes idle; nothing auto-relaunches.

**MOVER RECOVERY (2026-07-10):** 11/12 improved sources regenerated locally
(deterministic re-run, budget=80/topk=16/9 families) + re-verified NORMALIZED
(+0.06..+8.04 real gains; climber over-reports absolutes +0.06..+1.18). Files:
~/tmp/crackrec_out/*.cpp (provenance headers), summary/verify JSONs in ~/tmp.
Use as permuter/grind seeds — NOT landable. BinStream::Read didn't reproduce.
⚠ GOTCHA: isolated-Scorer (crack) paths need OBJCACHE=off until decomp-synth's
uncommitted --fo redirect fix lands (objcache no-op to private obj path reads
0.0% baseline). Worktree wt-crackrec left in place.

**Still open:** whole-TU engine build decision; owner B2-key-rotation decision;
land the uncommitted batch_unit_climber/objcache fixes in decomp-synth.

[[project_crack_farm_design_2026-07-09]] [[project_paths_to_100_rfcs]]

<!-- ======== END memory file: project_crack_farm_deploy_2026-07-09.md ======== -->


<!-- ======== BEGIN memory file: project_crack_farm_saturation_2026-07-09.md (12740 bytes) ======== -->

## Archived memory: `project_crack_farm_saturation_2026-07-09.md`

---
name: project_crack_farm_saturation_2026-07-09
description: "2026-07-09: crack-farm SATURATION driver (saturator.py) — self-sustaining 200-worker loop on box A at ~80% CPU; v2 widened search from 9->54 pattern families + crack-zone budget focus; NDJSON corpus streaming to B2"
metadata:
  node_type: memory
  type: project
  originSessionId: fe599317-6faf-4a30-a027-2f05b4f041c9
---

2026-07-09: owner wanted the rented 4x5090 box (vast id **44292979**, 256 cores)
CPU-SATURATED to ~80% with self-sustaining deep-compile crack work + meaningful
B2 storage. Built `saturator.py` (lives at `/home/free/tmp/crackfarm/saturator.py`
+ deployed to box `/workspace/saturator.py`; pusher `sat_pusher.sh`). SUPERSEDES
the one-shot par_launcher.sh swarm (that idled the box in minutes).

**Why the old approach idled:** `crack_live.py` has ZERO internal parallelism +
does NOT write back to decomp.db, and its default search uses only **9 of 116**
registered pattern families — so the near-miss frontier exhausts fast and reruns
repeat identical deterministic work.

**saturator.py design (the fix):**
- Work cell = (function x config_id). Pattern gen is deterministic, so each cell
  is distinct never-repeated work; the config sequence is effectively unbounded
  (family singles -> pairs -> k-subsets) => box stays busy for days.
- N threads (200 on box A) each spawn one `crack_live.py --symbol ... --isolate`
  subprocess (nice19/ionice3). `--isolate` = private scratch dir + private obj,
  so hundreds run concurrently with zero collision.
- Restart-safe ledger (`/workspace/sat/ledger.db`, sqlite, (symbol,config) PK) =>
  hot-swapping the config sequence skips done cells, new config_ids are fresh.
- NDJSON corpus: each job -> one record {ts,host,nonce,fn_symbol,config_id,
  config,start_pct,final_pct,cracked,compiles_used,path,plateaued,wall_s},
  buffered to rotated immutable shard files, pushed by `sat_pusher.sh` to
  **`b2:decomp-synth-runs/farm/crackfarm-rb3xenon-sat/<host>/corpus/shard-*.ndjson`**
  + a live `SAT_STATUS.json` heartbeat. VERIFIED clean end-to-end (downloaded
  5 shards locally: 1500 lines, 0 parse-failures, all core fields present).

**NEUTRALITY IS A LANDING CONCERN, NOT A SEARCH CONCERN (owner-corrected).**
Do NOT restrict the SEARCH to behavior-neutral moves. For decomp the retail bytes
are GROUND TRUTH, so a semantics-changing edit (flip `<=`->`<`, delete a stmt,
De Morgan, insert/remove a guard) that reaches BYTE-EXACT match is usually
RECOVERING the true source — i.e. fixing our reconstruction's bug — not cheating.
Those are the MOST valuable cracks (the function was genuinely wrong). The
neutral/non-neutral line belongs only at the LANDING gate (a non-neutral patch
needs harder verification: complete byte match + whole-binary net>=0 before
commit). The farm never RUNS the code (compile+diff only), so even "hazardous"
(OOB/off-by-one) families are safe to explore. Signal quality: a non-neutral edit
reaching ~100% is likely a real fix; one yielding only a PARTIAL gain (e.g.
DeleteNode 22->67) is more likely coincidental byte-alignment = weaker hint. v3
therefore runs the **FULL 116-family menu** (WIDE_FAMILIES = list_patterns(),
regalloc-first ordering) incl. comparison_equivalence/demorgan_guard/
null_guard_insert/switch_if_convert/positive_branch_invert/goto_to_return.
config_ids bumped wide->full so the ledger re-runs them. (First 2 real cracks
came from NEW families slot_pad + member_ref_bind — proof the widening pays off.)

**THE BIG LEVER: 9 -> 116 pattern families.** `decomp_synth.patterns.
list_patterns()` has **116** families; crack_live's `DEFAULT_FAMILIES` uses 9.
The unused 107 include the X360 near-miss killers: `mwcc_regorder_probe`,
`fpr_cascade_operand_hoist`, `prologue_pressure`, `float_literal_pressure`,
`parameter_live_range`, `assignment_reorder`, `member_init_reorder`,
`commutative_swap`, `loop_var_hoist`, `stack_array_hoist`, `single_return`,
`temp_elimination`. Family GATING means a non-applicable family emits ZERO
candidates (costs gen time, not compile budget — only topk get compiled), so a
wide `--families` menu is safe + strictly widens reachable states. v2 config
sequence (WIDE_FAMILIES=54) front-loads the CRACK ZONE `[92,100)` (min_band
filter) with budget 800 / topk 96, runs the register-order families SOLO x3
(unseeded shuffle => fresh permutation each pass), cranks moves caps
(sites64/fills32/gcap4000), then cheaper all-band corpus passes, then an
unbounded k-subset tail for saturation. 706 configs enumerated.

**Target reality (important):** `select_near_misses(10,100)` resolves only **428**
workable functions (rest are is_stub / has_linker_merged / excluded / no source
file). Band split: **[92,100)=221**, [80,92)=71, [50,80)=77, [10,50)=59. So the
crackable population is ~221 near-misses — that's where deep budget belongs, NOT
the 22%-match functions that local permutation can never reach 100%.

**Measured saturation:** load avg ~210-330 (load overstates it — workers cycle
short I/O waits), but real CPU busy = **79-82%** (from /proc/stat delta; `nice`
jiffies dominate = our work). 4x5090 GPU vllm workload stays pinned 100%,
unaffected (nice-19 CPU-only). 42G/503G RAM. Zero job errors.

**Crack honesty:** most "cracked" records have empty path = STALE-DB artifacts
(DB says <100% but current source already matches; 117 seen, ~0.25%, drain in
1 compile). Real permutation cracks are rare + are CANDIDATES needing the
correctness gate (comparison_flip/DeleteNode to reach 100 = semantics-changing;
variable_extraction/statement_reorder = plausibly neutral). The moves-generator
big movers (22->66 via DeleteNode) are byte-fishing = training signal + divergence
HINTS, not clean lands. Nothing auto-commits (design doc's mandatory gate).

**REMAINING LEVERS (not yet pulled):**
- `--generator llm` (deepseek-v4-flash via OpenRouter) = biggest quality jump
  (proposes semantic edits local search can't reach) but needs API key + $ spend
  (owner decision; small-LLM-permute was "killed" 2026-07-08 in a weak config).
- `batch_unit_climber.py` = whole-TU compile-once-score-many (RFC-21 TO-BUILD);
  explores cross-function regalloc coupling; runs LLM-free via ProcessPool.
- `scan_and_permute.py --strategy evolutionary` = population/recombination search,
  deeper than greedy local climb.

**BOX CHURN (owner cycles their own train/eval boxes):** box A (44292979, 4x5090)
was TERMINATED mid-session; a fresh 4x4090 (**44303331** "m0confirm-suffix-ab",
96 cores) came up EMPTY and became the farm target (owner said "44303331 is the
new box", chose "provision it"). Provisioned from scratch — NOT baked, so the
full flow was needed:
- `/home/free/tmp/crackfarm/provision.sh` replicates eval_sidecar's env-pull
  (rclone cfg from b2_env.sh, resolve `eval-env/LATEST`, pull `env-<VER>.tar.zst`
  + manifest, **sha256-verify against manifest**, `zstd -dc | tar -C /workspace`
  to the baked-in `/workspace/eval`, verify env.sh+rb3-xenon). ~660MB, ~2 min.
- b2_env.sh recreated on box via base64->0600 file (NEVER echo the decoded creds;
  build the base64 with a python snippet that prints ONLY names, not values).
- then saturator.py + sat_pusher.sh over, launch under `. /workspace/eval/env.sh`.
Corpus now at `b2:.../farm/crackfarm-rb3xenon-sat/c/corpus/` (host label `c`).

**LAUNCH GOTCHA (cost me many restarts):** `nohup setsid python3 ... & disown`
inside a vastctl `ssh --exec` persists ONLY when the launch is its OWN --exec
call. Combining `pkill ...; nohup setsid ... & disown` in ONE --exec => the new
process often does NOT survive the channel close. ALWAYS: pkill in one call,
launch in a SEPARATE call. Also `pgrep -cf "python3 /workspace/saturator.py"`
double-counts the bash -c launcher wrapper — count with
`ps -eo comm,args | grep saturator.py | grep -c "^python3"`.

**CPU-TARGET TUNING IS NOISY on a shared train box.** Worker->CPU% is NOT
monotonic here (56w=70.7%, 62w=65.8%, 68w=90%) because the co-tenant TRAINING
workload's CPU demand fluctuates and dominates the signal; our nice-19 farm just
fills spare cycles. Chasing an exact 80% by worker count is chasing that noise.
nice-19/ionice-3 ALREADY yields to the paying workload, so "80%" is effectively
auto-managed. Landed on **62 workers** (load ~78/96, ~66-80% CPU, clean
crack_live≈63, 0 errors). Each restart spawns a multi-MINUTE drain tail of
budget-400/800 crack_live children that inflates the NEXT reading — wait 4-5 min
after any restart before trusting a CPU sample.

**AUTOMATED 2026-07-09 (decomp-synth 2ca107d): no more manual deploy.** Added a
third sidecar mode `EVAL_MODE=saturate` so spawning a TRAINING box auto-runs the
saturator on spare CPU with ZERO staging (owner chose co-tenant-only, no watchdog
— box lifetime = training job's). Key advantage over `EVAL_MODE=farm`: saturator
self-selects the frontier from the baked decomp.db, so NO B2 manifest needed.
Files (all decomp-synth `tools/vast/`): `onstart/saturator.py` (NEW; generalized
from the hand-deployed box-A/C driver, all paths env-driven: SAT_REPO/SAT_DSYNTH/
SAT_OUTBOX/SAT_SCRATCH/SAT_HOST, workers default round(0.7*nproc) via SAT_WORKERS);
`onstart/eval_sidecar.sh` (saturate branch beside farm at ~line 276; EVAL_TARGETS
gate relaxed); `onstart/train.sh` (CPU_FARM block picks saturate when env staged +
NO manifest, farm when manifest queued — backward compat; side_companions pulls
saturator.py); `eval-env/bake.sh` (saturator.py in companion upload loop);
`EVAL.md` docs. Corpus streams to `evals/<RUN_ID>/corpus/` via the sidecar's
EXISTING streamer (no separate pusher — dropped sat_pusher.sh). Opt out:
`--no-cpu-farm` (CPU_FARM=0). DEPLOYED: committed + uploaded new saturator.py +
eval_sidecar.sh to `b2:decomp-synth-runs/eval-env/` (the two companions boxes pull
at boot; yield_fence/farm_worker unchanged). Verified locally end-to-end (160
recs/0 parse-fail/crack-zone config/status heartbeat via SAT_* env overrides
against local rb3-xenon decomp.db = 1103 targets). NOTE decomp-synth main had
concurrent uncommitted work (TRAINING.md, vastctl.py, docs) — committed ONLY my 5
files by explicit path, no branch switch, left theirs untouched.

**CRACKS LANDED 2026-07-09 (rb3-xenon 691a9b6): the farm paid off.** All 12 corpus
candidates verified by a workflow fan-out (reproduce via crack_live -> worktree ->
function gate -> rb3wii/dc3 oracle gate -> whole-binary gate). 6 LANDED (+3
matched_functions 11553->11556, +3420 matched_code): ChunkStream::
PollDecompressionWorker (GENUINE bug — CritSecTracker disarm order, DC3-confirmed),
BandCamShot::Target::UpdateTarget, TourDescProvider::Text, LightPreset::CacheFrames,
OvershellPartSelectProvider::IsActive + Spotlight::UpdateFloorSpotTransform (both
already normalized-credited, upgraded to raw byte-exact). 6 REJECTED honestly:
slot_pad = artificial padding not source; UsbMidiGuitar/NgPostProc/BSPFace
oracle-contradicted <100 byte-fishing; FreestyleMotionFilter::Deactivate flip =
byte-fishing (DC3 says mIsActive=false is true source; residue is a REAL struct
offset bug 0x34 vs 0x10 — separate lead); NetSync::IsTransitionAllowed already 100
on main (stale DB). KEY PROTOCOL NUANCE: raw-only cracks on functions already
normalized-100 show whole-binary delta=0 — matched_functions can't be the only gate
for those; matched_code delta is the honest signal. Also regenerated the EMPTY
decomp.db (repo-root ./decomp.db is the real MCP path, NOT scripts/orchestrator/;
71455 fns, attempts preserved).

**KEYLESS-B2 DESIGN (fable subagent, 2026-07-09):** doc at
`/home/free/code/milohax/decomp-synth/docs/plans/keyless-b2-ingest.md` (uncommitted).
RECOMMENDED: per-launch ephemeral capability-scoped native B2 keys (listFiles+
readFiles+writeFiles, NO deleteFiles, validDurationInSeconds=run+slack), minted at
launch by a workstation-only account-level minter key (writeKeys/listKeys/deleteKeys,
no file caps); passed under the SAME B2_KEY_ID/B2_APPLICATION_KEY env names so all
box scripts ship unchanged. Only box-side deletes are train.sh STOP/EXTEND markers
(:367/:372) -> rework to content-token one-shots. Rejected: presigned PUT
(unpredictable shard names), hosted ingest service (multi-GB checkpoints exceed
Worker limits; degrades into presign-issuer + standing secret), R2 (same primitive,
needs migration), tailscale rclone serve (workstation uptime coupling). Cutover
deletes the leaked key = completes the rotation. ~80-line mint helper + ~20 launcher
lines total.

**⚠ B2 key rotation still pending owner decision** (leaked to transcript 2026-07-09) —
the keyless design's step 7 IS the rotation; owner just needs to green-light implementation.

[[project_crack_farm_deploy_2026-07-09]] [[project_crack_farm_design_2026-07-09]]

<!-- ======== END memory file: project_crack_farm_saturation_2026-07-09.md ======== -->


<!-- ======== BEGIN memory file: project_renamer_pairing_bug.md (1618 bytes) ======== -->

## Archived memory: `project_renamer_pairing_bug.md`

---
name: project-renamer-pairing-bug
description: "SUPERSEDED — diagnosed wrong tool. The actual bug is dtk's .fn/.endfn mis-nesting; renamer is innocent. See project-jeff-asm-misnest.md"
metadata: 
  node_type: memory
  type: project
  originSessionId: fa9ece23-af47-452b-8343-643e1474a7b1
---

**SUPERSEDED 2026-05-27 by [[project-jeff-asm-misnest]].**

I initially blamed `scripts/obj_target_symbol_renamer.py` for the `<illegal>`-target pathology in many near-complete units. An Opus verification agent **refuted** that diagnosis. The renamer correctly overwrites the existing symbol entry's name in place (long-form string-table reference); it does NOT append a new symbol. The duplicate-symbol appearance in `strings build/.../foo.obj | grep` was relocation pointer-name comments baked into other sections' data, not live symbol-table entries.

The actual bug is in **dtk (the local jeff fork at `../jeff`)**: its `xex split` asm writer emits **mis-nested `.fn`/`.endfn` directives**, which causes the assembler to lay function bytes into the **wrong COMDAT sections**. Symptoms identical (target shows `<illegal>`, symbol size wrong), but the fix lives in jeff's Rust code, not in the renamer.

Verification: `awk '/^\.fn / { stack[++sp]=$2 } /^\.endfn / { if (stack[sp] != $2) print "MISNEST line " NR ": opened "stack[sp]", closes "$2; sp-- }' build/45410914/asm/framing.s` shows mis-nested closings at lines 228/268/276 (3-function cluster fn_82BF8E48/E68/EA0) and 861/924 (fn_82BF9640/9690 swap). Auto_03 buckets show 1k-43k mis-nests each.

**Do not edit `obj_target_symbol_renamer.py`.** It's correct.

<!-- ======== END memory file: project_renamer_pairing_bug.md ======== -->
