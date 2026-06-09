# rb3-xenon decomp — state, lever landscape & research roadmap (2026-06-09)

**Operating model:** Claude coordinates + manages context; implementation is delegated
to **workflows** (`.claude/workflows/*.js`, run via the Workflow tool). Tooling is the
product — every manual/agent pass must report tooling gaps + ideas. This is a research
feedback loop: form a hypothesis → build/run a tool → measure → record verdict.

## Current state (main `ce16bfa`)
- **6568 / 65548 functions matched** (4.93% of whole binary; 6.88% fuzzy).
  Oracle-backed tier ~8.4% matched. Session moved 6386→6568 (+182).
- Honest near-miss pool `[90,100)%` after the true_progress classifier fix
  (`50faab6`): **FRAME_ONLY 445** (funclet-gated, downstream metric), **STRUCT_WORK
  667** (real layout pool), **CODEGEN_WORK 217** (permuter-class), RECOVERABLE/CLEAN ~14.

## Lever landscape — what's TAPPED vs LIVE (hypotheses tested this campaign)

### Refuted / dead (do NOT re-attempt — proven negatives)
- **"Retail is an LTO/LTCG build hiding matches"** — REFUTED (4 signatures). No whole-
  program opt; TUs are contiguous. The asymmetry vs dc3 is the missing **map**, not opts.
- **Funclet "+615 tooling lever"** — REFUTED. FRAME_ONLY = parent-gated funclets (resolve
  free as parents match) + misclassified real work. 0 pure-name-reloc. jeff is NOT buggy
  (clamp/prune already fixed it). objdiff bl-by-name fix flips ~0. (`feedback_fuzzy_gap`.)
- **gameport via TU-cluster pinning** — DEAD for scattered band3 TUs (10/10 negative).
  Coverage-stub mirage + scattered fns; no contiguous cluster to pin.
  (`project_game_code_instrumentation`.)
- **Spatial bracketing on the rb3-Wii byte-oracle** — DEAD (114/116 known pins have 0
  oracle self-agreement; produces confident-WRONG spans).
- **Permuter on hard regalloc residuals** — ~0 even with m2c/BSF installed.
- **Struct-layout grind on top engine units** — largely EXHAUSTED (headers already
  correct; engine-easy-wins 14 agents → only the Str force-multiplier).
- **Sized-vector STLport gap** — REFUTED 2026-06-09 (calibrated worktree A/B, see
  §"sized-vector experiment VERDICT" below). Retail RB3-360 uses the classic 12-byte
  3-pointer `std::vector`; tree-wide sized flip = **NET −504** (−503/+2). Decisive:
  223 retail `?$vector@` symbols match 100% at baseline in the 2-param (ptrs) mangled
  form, not the 3-param sized form. VocalPlayer "+4" is ordinary member/base layout,
  NOT vector sizing. Do not re-attempt.

### Live levers (yield this session)
- **Engine force-multipliers** = "DC3 (newer) diverged from RB3" patterns. Three sub-types,
  each a header/cpp fix that flips MANY callers:
  1. **Inline-policy divergence** — DC3 inlined what retail kept out-of-line (or vice
     versa). `Str::operator==/!=(const String&)` out-of-line → **+6** (`ce16bfa`); the
     `DataArray::Node` keystone earlier. **HIGH EV, repeatable, under-mined.**
  2. **DC3-dropped/added member** — re-add/remove a member to match retail layout:
     `CharSleeve/CharIKSliderMidi mMe` +3, `Gem::Tail` pad +1, postproc pad +1.
  3. **Ground-truth data constants** — save/load revs: `PostProcer SAVE_REVS(1,0)→(0,0)`
     +1 (1-off, not repeatable — only instance found).
- **DC3 byte-identical fingerprint transfer / reveal** — drained this session (+161 early)
  but REFILLS as the swarm body-ports (reveal cascade).

### gameid-crossval experiment — **CLOSED NEGATIVE** (2026-06-09, calibrated)
Research question was: does cross-arch BSim / anchored-BinDiff re-locate the 25 known
game pins precisely enough to bracket TU spans? **NO — neither signal calibrates.**
- BinDiff (fresh source-built): per-fn precision 0.54 @ conf≥0.97, recall ≤0.023,
  max contiguous correct run = 3 fns. BSim (cross-arch): precision 0.32, recall ≤0.064.
- Root cause = the coverage-stub mirage, now CONFIRMED for BSim: 6112 of 6759
  top-matches to one rb3-Wii class are 32-byte coverage stubs at sim=1.0. The
  distinctive minority (~2-13%/TU) matches; the stub-shaped filler doesn't → no
  contiguity → no spans. Stub-masking/fixed-point BinDiff mods would NOT fix this
  (the negative is structural, not matcher tuning) — do not build them.
- **Salvage (the useful product): `docs/decomp/gameid/crossval_agree.json`** — 146
  BinDiff∩BSim cross-validated per-fn game-code identity hints @ 0.95 precision
  (93 not yet pinned). Usable as per-function LABELS for manual matching / as an
  fn_resolver evidence tier; cannot bracket spans. Full verdict + repro commands:
  `docs/decomp/gameid/VERDICT.json`; bulk artifacts remain at `~/tmp/gameid/`.
- Infra fixed during the run: Ghidra dist now extracted persistently
  (`../ghidra/build/ghidra` → `ghidra-dist/ghidra_12.2_DEV`, no longer via /tmp);
  Ghidra MCP 8002 Ready.

## Tooling — built/fixed vs gaps
**Built/fixed this campaign:**
- `m2c` + `BSF` permuter backends installed+wired (`9763683`): `tools/objdiff_to_m2c.py`,
  `tools/compiler_trace/`, decomp-synth.json. (Validated; permuter still hit-or-miss.)
- `tools/true_progress.py` FRAME_RECON classifier fixed (`50faab6`) — honest buckets.
- Game-ID pipeline (`.claude/workflows/gameid-crossval.js`) + bindiff/binexport build path.

**Tooling GAPS / ideas to explore (the feedback-loop backlog):**
1. **Force-multiplier finder** (HIGH EV) — a tool that scans engine near-misses for the
   3 "DC3-diverged" patterns (inline-policy flips, dropped/added members via uniform
   member-offset deltas, save-rev constants) and ranks them, instead of per-unit agent
   recon. The Str/CharSleeve/Gem wins were all found by hand; automate the detection.
   Signal sources: objdiff uniform-offset-delta detection + cross-ref DC3 header inline
   bodies vs retail out-of-line `bl` (the Str pattern is mechanically detectable).
2. **Inline-policy diff** — diff DC3 headers' inline method bodies against retail's
   out-of-line/inline choice per function (detectable: target `bl <fn>` where our build
   inlined the body). This is sub-type 1 above, the highest-yield repeatable lever.
3. **Reveal-cascade refill monitor** — re-run reveal/relocate/pin as the swarm lands
   body-ports (it refills); currently manual.
4. **Game-ID tool** (gated on the calibration verdict) — if BSim/bindiff-mod calibrates,
   productionize it (`tools/`) as the standing game-TU locator.
5. **Permuter is not the bottleneck** — CODEGEN_WORK 217 is genuinely hard; deprioritize.

## Prioritized next experiments (workflow queue)
1. **Integrate `gameid-crossval` results** — pin any HIGH-confidence cross-validated game
   TUs; record the calibration verdict (science: did the signals work?).
2. **Build + run the force-multiplier finder** (gap #1/#2) → feed an `engine-forcemult`
   workflow targeting inline-policy + dropped-member flips across ALL engine units (not
   just the top STRUCT_WORK ones — Str came from DirLoader recon, not a top unit).
3. **DC3-source wiring residual** — vet the ~remaining real (non-vendor, non-Kinect)
   unwired engine `.cpp` for byte-identical clusters (UIListProvider was +9).
4. **Reveal refill** after each body-port wave.

## 2026-06-09 force-multiplier wave results (main `9150f3c`, 6576, +190 session)
Built+validated `tools/inline_policy_finder.py` + `tools/member_delta_finder.py` (`a8716c7`),
ran an apply wave → **+8 landed** (`9150f3c`):
- **NgPostProc +2** — RndPostProc carried 4 DC3-only floats (hue/blend, rev skew) RB3 lacks;
  gated behind `RB3_HAS_HUE_CONVERGE`. (member-delta finder hit.)
- **RndTexRenderer +6** — scaffolded from DC3 rev-13; retail is rev-11. Dropped mEnviron/
  mClearBuffer/mClearColor + restored bool order. (member-delta finder hit.)

### sized-vector experiment VERDICT (2026-06-09, calibrated A/B): **REFUTED**
Hypothesis was: rb3-Wii's `_STLP_USE_SIZED_VECTOR` (8-byte vector) vs our 12-byte =
tree-wide +4 force-multiplier. Ported the full machinery (rb3-Wii `_vector_sized.{h,c}`,
`_vector_ptrs.{h,c}`, dispatch wrapper, `RB3_PTRS_VECTOR` per-TU gate à la
RB3_RBTREE_0x1C) into worktree `/home/free/code/milohax/rb3-sizedvec`; sanity gate
passed (`sizeof(vector<int>)==8`). Whole-binary A/B: baseline 6576 → sized 6072,
**NET −504** (−503 regressed / +2 improved, 77 units, no unit meaningfully gained).
Decisive evidence beyond the number:
1. 223 retail `std::vector` member fns match **100% at baseline** with 2-param ptrs
   mangling (`?$vector@T V?$StlNodeAlloc@...`), not the 3-param sized form.
2. Isolated control: AccomplishmentManager `_M_erase` 100%→0%→100% flipping just that
   TU's vector layout.
3. VocalPlayer (the poster child) *regresses* under sized; its +4s are ordinary
   dropped/added-member or base-class layout. The 8-byte-stride annotation evidence
   was an artifact of adjacent small members.
MSVC breakers found mid-port (synth_xbox explicit-allocator forms, explicit
`template class vector<>` instantiations, rndobj `_M_finish` internal pokes) were
themselves corroborating evidence retail compiled ptrs. Worktree left in place
(`#if 0`'d lever + refutation comment) for reproducibility; removable.

### Walls confirmed (not levers): GameMode/Player = vbase/coupled-base hierarchy
(PropertyEventProvider→MsgSource differs RB3 vs DC3); ??_8 vbtable + ??_E/D/G adjustor
thunks. Deep multi-TU reconstruction, no clean oracle. Defer.

### Tooling gaps from this wave (feedback-loop backlog, prioritized)
1. **member_delta_finder v2** — must CLASSIFY (not flag as member-pad): (a) sized-vector
   STLport gaps (detect VECTOR_SIZE_SMALL/LARGE members + the +4/vector signature),
   (b) vbase displacement (??_8 vbtable + adjustor-thunk diffs = coupled-base wall).
   Both produced false candidates this wave (VocalPlayer, GameMode).
2. **fn_<addr> → identity resolver** for anonymous bl callees (unblocks inline tail + game-ID).
3. **struct_db / lookup_struct_offset is STALE** — reports old ObjPtr-size / rb3-Wii offsets
   (misled RndTexRenderer + Player). Refresh from current headers; reconcile with retail.
4. **Ghidra MCP (port 8002) was DOWN** all wave (3 agents blocked on struct cross-checks) —
   restart `tools/ghidra/pyghidra-service.sh` after the gameid Ghidra agent releases RB3Xenon
   (stale RB3Xenon.lock from Jun 7 to clean). Until then agents fall back to objdiff asm.
5. diff_inspect asm_listing uses wrong obj path for game units (`default/band3/...` prefix).

### Updated next-experiment queue (post-wave)
1. ~~sized-vector STLport experiment~~ — DONE, **REFUTED** (see verdict above).
2. ~~member_delta_finder v2~~ — DONE, landed `d3fc934` (validation 3/3; STRUCT_WORK
   sweep → 7 candidates: DxRnd MEMBER_DELTA actionable [agent in flight], 4 vbase
   walls defer, VocalPlayer reclassified, 1 unknown).
3. ~~fn_-resolver tool~~ — DONE, landed `013b7ae` (`tools/fn_resolver.py`; 4,146 named
   + 5,932 strong ≥0.70-conf identities; 3/5 inline-policy unknowns resolved).
4. Integrate `gameid-crossval` (rb3-Wii artifacts at `~/tmp/gameid/`; target side was
   blocked on the stale RB3Xenon.lock — lock cleared, Ghidra MCP 8002 restarted).
5. ~~Re-run inline_policy_finder with fn_resolver~~ — DONE. Resolver integration landed
   (`tools/inline_policy_finder.py`). **VERDICT: inline-policy lever TAPPED for the
   current near-miss pool** — all [90,100) candidates are n=1 (single caller); the
   historic wins (DataArray::Node +37, Str +6) were n=20+ clustered. MakeShortAng
   inline test = NET −1, reverted. Candidate lists archived at
   `~/tmp/forcemult/inline_candidates_v2*.json`. Re-check only after the pool refills.
6. ~~data-symbol renaming (LBL_RENAME)~~ — **REFUTED** (verify-first gate, full
   probe of all 771 [99.9,100) fns + empirical rebuild). LBL_ONLY = **0%**; true
   composition: **42.8% HAS_REAL** (genuine STRUCT_OFF/REG/OPCODE work), **36.6%
   EH-funclet `bl lbl_<frameless dtor>` + frame-recon** (= the already-refuted
   funclet wall; mapping the dtors resolved the bl diffs in all 282 and flipped 0 —
   the `subi r31,r12` frame residual independently blocks every one), 13.9% anon fn_
   callee, 5.1% named mismatch. Genuine data labels (44 fns) are all `__real@` float
   pools / local statics co-occurring with real codegen diffs — never the sole
   blocker. **Do NOT extend the renamer to data symbols (yield ~0).** Root cause of
   the false hypothesis: true_progress's NAME_RELOC class conflates bl-lbl/code,
   data-lbl, anon-fn, named-static (tool gap; a lbl_probe.py classifier existed in
   the removed worktree — rebuild if the question recurs).
7. Other gaps from the wave: UIComponent vtable +4 slot (5 confirmed fns; known wall,
   docs/plans/ui-base-layout-reconstruction.md); n=3 unnamed shared ctor body
   (CharBoneOffset/CharIKFoot) needs Ghidra+DC3 pair to identify.
8. NEW tool ideas from the sized-vector refutation: (a) vector-layout classifier via
   objdiff symbol-name arity (2- vs 3-param `?$vector@`) — answers layout questions
   without a build; (b) `--force-fresh` full-report flag (partial-rebuild reports mix
   old/new objects and mislead per-unit A/B).
9. DxRnd MEMBER_DELTA — DONE (+1, `5770d95`+`22b5148`, 6577): mColorRampTex @0x394 +
   DoPostProcess GPR literals; Present residual = boolean-negation unfixable class.

## 2026-06-09 (evening session) — playbooks + tool-gap closure + 2 more vein verdicts

Theme: **codify the subagent formulas** (docs/decomp/playbooks/) + close the tooling
backlog. 6 agents, all landed. Matched count unchanged at **6577** (verified honest by
the new fresh_report.sh: 6542 raw fuzzy==100 + 35 FP-anchor-normalized).

**Playbooks established (`docs/decomp/playbooks/`)** — the in-repo subagent formulas:
- `bodyport-wave.md` (`178a758`) — the body-port wave recipe distilled from the
  +1500-scale campaigns (provenance, wall-defer list, per-fn loop, measurement
  honesty, landing protocol, hard rules).
- `hasreal-grind.md` (`008e53c`, from the grind pilot) — metric trap (§0), 8-signature
  wall-recognition checklist (§3a–3h) with exact asm, offset root-causing tree,
  time-boxing. **Pilot verdict: 0/12 hand-examined named near-misses were clean
  source fixes** — the pool is walls+permuter-class; do NOT swarm it raw.
  Also: UtilDrawAxes is ALREADY MATCHED (vtable-slot reloc normalized away;
  `run_objdiff` live % ≠ report `match_percent_normalized` — always rank by the
  report metric).

**Tools landed:**
- `b47025f` true_progress: NAME_RELOC split into BL_LBL_FUNCLET / ANON_FN_CALLEE /
  NAMED_MISMATCH / DATA_LBL + HAS_REAL bucket + `--worklist`. Reproduces the 771-fn
  probe exactly (330/282/107/39/0). Worklist: ~/tmp/hasreal_worklist.json (bands on
  normalized %, so free of the §0 trap).
- `b935d9e` fn_resolver T3b gameid_crossval tier: +69 new best-identities, 24
  agreements, 6 conflicts (named wins; ~5% FP tail as expected).
- `f82ed7f` fresh_report.sh + vector_arity.py (861 arity-2 / 0 arity-3 binary-wide —
  sized-vector refutation now a one-command check) + asm_listing obj-path fix
  (`default/` → `src/`; broke /FAs for ALL units) + struct_db refreshed (current
  headers; MCP lookup_struct_offset serves it directly).
- `91b50c9`+followup wall_classify.py — auto-tags the 330-fn worklist with the
  playbook §3 signatures (11/11 ground-truth validation). FIRST EXECUTION taught it
  a new wall class: the headline "Rnd +4@0x54 cluster (44 fns)" was a **funclet
  address-pairing artifact** (r31 from `subi r31,r12` = frame slot, not `this`;
  dtk paired structurally different fns — body sizes 0x78-vs-0x54, `new` sizes
  0x1a4-vs-0x20c). Playbook gained §3i + §4 "gate zero"; classifier gained
  FUNCLET_PAIRING detection (direct-vs-indirect frame-reg access distinguishes
  artifact from real). **Corrected routing of the 330: MEMBER_DELTA 69 (the honest
  residue), FUNCLET_PAIRING 120, DEFER_VBASE 107, PERMUTE 9, UNKNOWN 19, other 6.**
  Output: ~/tmp/hasreal_routed.json.

**Banked deferred lever — CameraShot +25:** real uniform +4 member delta (retail has
a 4-byte member at ~+0x40 we lack; shifts a 9-member ObjPtrList<RndDrawable> chain
+0x44..0xdc stride 0x14). Owning class = vtable `0x82077150` (copy-ctor fn_824AB3E0);
offsets do NOT match the CamShot header → distinct/sub-object layout, needs Ghidra
reconstruction. Handoff: ~/tmp/rnd54_findings.jsonl. No guessing into shared headers.
Other surviving high-conf MEMBER_DELTA: ??_G deleting-dtor adjustor-delta family
(CharInterest −68, Waypoint +592, HamCharacter +96, RndShockwave +724, …) and
ChunkStream +2084 (size-divergence class).

**Vein verdict: unwired-DC3-engine residual = DRAINED** (`c13507f`,
tools/dc3_residual_rank.py + docs/decomp/dc3-residual/ranked.json). 115 unwired
engine TUs; content-match ceiling 57 fns total (2–10%/TU). The one real contiguous
cluster (PlatformMgr_Xbox, 109 fns) diverged too far to wire (DC3 SmartGlass/store
era; C1083 + 282-vs-109 fns). 3 "clusters" were trivial-collision noise (the ranker's
promiscuity+contiguity filter catches these). Roadmap queue item 3 CLOSED.

**New tool gaps (backlog):**
1. struct_db parser ingests both `#ifdef HX_NATIVE` branches (last-write-wins
   fragility).
2. `global_fuzzy_pairs.json` and `dc3_content_match.py` source from DIFFERENT DC3
   obj dirs and disagree on coverage — unify to one DC3 obj source.
3. dc3_content_match default obj dir non-recursive (misses the 811 per-TU system/
   objs).
4. Ghidra MCP needs access serialization (single-process; ClosedException under
   concurrent agents).
5. true_progress worklist `address` field is section-offset decimal — parse the
   `fn_<hex>` from sym for load addresses.

**Where the next +N comes from (post-pilot view):** (a) wall_classify's
MEMBER_DELTA_CANDIDATE + UNKNOWN residue = the real grind pool (likely far smaller
than 330); (b) PERMUTE-routed bucket via /permute (sanctioned, no hand-edits);
(c) body-port waves per the playbook (still the standing campaign — the named
40–95% pool, NOT the near-miss pool); (d) reveal refill after each wave.

## Key refs
- Memory: `feedback_fuzzy_gap_needs_permuter`, `project_game_code_instrumentation`,
  `project_lto_icf_investigation`, `project_scope_map`, `project_engine_split_relocation`.
- Recon artifacts: `~/tmp/recon/` (funclet_classification.json, ghidra-caps/bindiff-vs-rust
  findings, spatial_pin_probe + stub detector in common.py).
- Workflows: `.claude/workflows/{engine-easy-wins,gameid-crossval,permuter-sweep-fresh,
  saverev-sweep,ultracode-levers}.js`.

---

# Session addendum 2026-06-09/10 (late night): first full playbook-driven wave

Coordinator-orchestrated wave of 9 Opus/Sonnet agents executing the playbooks.
**6577 → 6586 matched (+9, zero regressions), 9 commits (5b6b7a6..dc080dd), plus
two major lever discoveries and three permanent refutations.** Operating model
confirmed: coordinator selects/lands/verifies only; ALL implementation (even small
tool fixes) delegated to agents (user directive, memory `feedback_coordinator_role`).

## Matches landed (+9)
- **FileMerger +7 cascade** (`dc080dd`): dropped DC3-added `Merger::filler`
  Symbol@0x4 (rb3-Wii confirms retail lacks it) — cascaded across FileMerger +
  BandWardrobe. The "DC3-added member RB3 lacks" pattern strikes again.
- **ColorPalette +1** (`60eabed`): `ColorSet` padded 0x20→0x44 (retail-only
  trailing non-serialized storage; DC3/rb3-Wii shrank it).
- **Mesh +1** (`b766775`): `CompressedVertex_Xbox.mPosX/Y/Z` were `int`, retail+DC3
  type them `float` (+rest `unsigned int`) — wrong types routed `BinStream<<` to the
  int writer. Plus `817b8a5` BSPFace::Update 80→94.6 (oracle-faithful, residual is
  FP-ordering) and `f97da90` SetBloomBlurWeights PShaderConstant base **0x2f**
  (retail; DC3 moved it to 0x9a) — banked, flips +1 with a future .rdata
  array-layout fix.

## REFUTED: CameraShot "+25" banked lever → VBASE_WALL (`fe0aaaa`)
Ghidra RTTI proof: retail `CamShot` is the **DC3-style virtual-base MI layout**
(base-class descriptor vbase displacement 0x1a0; RndAnimatable + RndTransformable
both deriving `virtual Hmx::Object`). The +4 IS DC3's `mShotStartedPending` —
retail DOES carry it — but our header is flat single-inheritance, and re-adding the
member is **verified net −3** (+7 funclet fns, −10 most-derived reads; two
incompatible base anchors). Only a full MI/vbase reconstruction wins this (deep,
out of scope). The handoff's "vtable 0x82077150 / fn_824AB3E0" attribution was a
sub-object red herring; real owner = `~CamShot` fn_824B29B8. Header comment now
records the wall. **New gate-zero rule: frame-recovered `this` that is then
vbase-adjusted (`subi 0x1a0`) = VBASE_WALL, not MEMBER_DELTA.**

## Grind-wave calibration: MEMBER_DELTA route was ~87% polluted
48 non-CameraShot candidates ground per playbook → 2 real (the +8 above), 42 walls:
24 FUNCLET_PAIRING (every size-44 `fn_` __unwind funclet; divergent `bl` callee =
the tell), 10 VBASE (the ??_G deleting-dtor adjustor family — non-uniform per-class
deltas, virtual inheritance, NOT member-addable; kills that queue item), 5
**VTABLE_DIVERGENCE** (new class: vcall slot-load deltas — DC3 added 8 virtuals to
Rnd, 20 to RndEnviron, that retail lacks), 3 PERMUTE (ChunkStream "+2084" was an
offset-SWAP misparse; real member deltas are uniform SAME-SIGN), 1 DC3_REV_MEMBER
(Mat_NG +0x3c — material-chain rev12/13 block, TexRenderer/PostProc precedent),
1 no-oracle (Singer/VocalPlayer mVocalParts). Ground truth:
/tmp/mdgrind_abandoned.jsonl; corrections: ~/tmp/mdgrind_wall_classify_corrections.md.

## Body-port wave verdicts (pool nearly dry at the top)
- **rndobj/Utl + LightPreset: net 0, unit at-limit.** All [40,95) named fns are
  permuter-class regalloc, §3i structural mispairs (LegacyLoadP9, Save@Keyframe),
  codegen-shape (EnvironmentEntry op!=), or split artifacts (PreMultiplyAlpha
  truncated at 32B). Wave agent independently re-derived the **gRev/BinStreamRev
  conversion** idea → already REFUTED 2026-06-07 (only 33 TUs use DC3-style
  BinStreamRev; their near-misses are walls). Playbook defer-list updated so waves
  stop re-proposing it.
- **Geo/Mesh/MemTracker: +1.** Geo = FP-scheduling permuter-class throughout.
  MemTracker = ONE shared wall (all 6 MemDiffEntry heap fns inline `operator<`
  whose mSizeDiff tiebreaker compiles to the overflow-safe signed-compare idiom —
  boolean-negation class; NOTE our tiebreaker is CORRECT for RB3, DC3's simpler
  form is the wrong oracle there).

## Tooling landed
- `40deb7d` **DC3 obj-source unified**: canonical = `.dc3_text_scratch/named/obj`
  (retail TARGET tree; the compiled-port tree substitutes dc3-decomp's own
  unmatched ports as "truth" — 20 content diffs, all in <92%-matched units). New
  shared tools/dc3_obj_source.py; deterministic ICF tie-break (352 contradictory
  identities → 0, was nondeterministic run-to-run); non-real-name filter
  (`merged_*`/`__unwind`). **Data debt: live global_fuzzy_pairs.json holds dead
  `merged_*` top-identities + 746 dup rows — regenerate (`global_fuzzy_index.py 64
  0.85`) at wave end. T4 fuzzy labels = ICF-ambiguous, never authoritative.**
- `bf0d039` **struct_db gated members**: guard/guard_kind columns (45 real gated
  members, 9 files: HX_NATIVE forks, RB3_RBTREE_0x1C, RB3_HAS_HUE_CONVERGE,
  MILO_DEBUG); lookup returns all variants tagged, retail leads; MCP reader
  guard-aware. DB regen deferred to wave end: `python3 tools/struct_db.py build src/`.
- `5b6b7a6` **true_progress load_addr**: report `address` is NOT base+offset
  recoverable (dtk drops alignment padding from cumulative offsets!) — resolve via
  fn_ self-encoding + inverted target_symbol_map gated by splits ranges (99.2%).
- `tools/fresh_report.sh` OOM: full-parallelism builds get code-137 killed on fresh
  CoW worktrees (32 cores × MSVC-under-wibo); use `ninja -j 12` (fix in flight).

## Worktree pool cleaned
9 stale prior-session worktrees (fa-*, fm-*, xfer-recover) triaged read-only:
all fully harvested (0 ahead-commits; uncommitted files byte-identical to main or
strictly older) → removed. `rb3-sizedvec` KEPT (sized-vector refutation repro copy).

## In flight at time of writing
- **flowback agent**: wall_classify gains divergent-bl/same-sign/vtable-slot/vbase
  gates (validate vs original 11 + the 48-fn confusion matrix) → regenerates
  ~/tmp/hasreal_routed_v2.json; playbook §3/§4 updates; fresh_report.sh -j cap.
- **vtable-lever agent**: slot-by-slot Rnd/RndEnviron RB3-vs-DC3 vtable comparison
  (dump_vtable.py + Ghidra + ham_xbox_r.map), drop DC3-added virtuals if A/B clean.
- Queued: Mat_NG +0x3c rev-member lever; struct_db + global_fuzzy_pairs regen;
  reveal refill sweep (incl. +2 byte-exact MeshAnim anons 0x8245BC78/0x8245DA30).

## VTABLE_DIVERGENCE lever EXECUTED: Rnd +10 @100 (`30a4ae8`, 6586→6596)
Slot-by-slot localization WITHOUT a pinned retail Rnd vtable: used accumulated
slot-offset deltas in CALLERS as anchors (ScreenDump +0 → DrawRect +1 → GetFrameID
+5 → SetShadowMap +6 → DoWorldEnd +8). The 8 DC3-era virtuals retail lacks:
`ScreenDumpUnique` (slot29), `GetSync`/`NumDrawPasses`/`BeginDrawPass`/`EndDrawPass`
(37–40), `ShouldDrawPanel` (42), `Push`/`PopClipPlanesInternal` (60/61); rb3-Wii
confirms none are virtual. Fix idiom: `RND_DC3_VIRTUAL` macro gates `virtual` behind
HX_NATIVE (native keeps dispatch, matching build drops slots) — same as the existing
ClearDepthForOverlay gate at rndobj/Rnd.h:170. Cascade: EndWorld, DrawStringScreen,
DrawRectScreen, RndFlare::SetPointTest, RndShadowMap::EndShadow,
SpotlightDrawer::UpdateBoxMap, 3 Utl draw helpers, CalibrationPanel::Exit.
NOTE: earlier "Clear is the first insertion" hypothesis DISPROVEN (ScreenDump +0
anchor); machine-code anchors beat source diffing for vtable work.
**Deferred from this lever (different root causes):** RndEnviron 20-slot delta =
secondary RndTransformable-base vtable under VIRTUAL inheritance (the foundational
RndHighlightable/RndTransformable vbase wall, §3a — not droppable virtuals);
TrainerGemTab::DrawStartFinish = separate 15-slot UILabel/UIComponent-MI delta
(the documented UI base-layout wall, docs/plans/ui-base-layout-reconstruction.md).
