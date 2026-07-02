# WS5 — Case-B campaign: evict + port the case-B worklist ids; fork status verified

**Written 2026-07-02** by the ws-doc prep workflow (fresh verification pass; every number
below was re-derived from the live tree, not copied from memory). Master doc:
`docs/plans/frontier-workstreams-2026-07-02.md` (stream 5). Repo:
`/home/free/code/milohax/rb3-xenon`, main was `@b2b9654` during doc prep (master doc
said `@44f57c6` — main is hot; re-check at execution).

---

## Objective

Land the ~15 real-bodied "case-B" band3-worklist identities — worklist functions whose
retail bodies live INSIDE foreign TUs' pinned `.text` spans (verified-genuine multi-TU
interleavings, e.g. `DepthBuffer3D.cpp`'s span holds 29 real strict matches interleaved
with 0% Tambourine ids) — plus the step-0 trivia (BaseSkeleton dead-span removal →
`PracticePanel::ToggleGuidePart`). Secondarily: close out the banked objdiff
global-byte-equality fork (`docs/decomp/handoff/objdiff-caseb-fork-banked.md`) with an
accurate status, since **this doc-prep pass found its status materially changed**.

## THE REFRAME (read this before executing)

Fresh code-level analysis of the fork (2026-07-02, this pass) shows **the fork is NOT
the mechanism that lands these 22 ids — splits EVICTION with stock machinery is.**
Two verified findings force this:

1. **The fork's promotion funnel requires a `<100%` NAMED ReportItem in the CLAIMING
   unit** (`objdiff-core/src/diff/mod.rs` `reconcile_global_byte_matches`, funnel:
   `named_unmatched → base.symbol_by_name(item.name) → signature → retail_index →
   oracle gate`). Report items are **target-driven** (`report.rs report_object`:
   `let obj = target.as_ref().or(base.as_ref())` — base-only symbols never become
   items; verified empirically: `default/band3/bandtrack/Gem` has 23 items, none of
   which is the base-defined `?ReleaseSlot@Gem@@QAAXH@Z`). A true case-B method — VA
   pinned to a FOREIGN unit, name never minted in the claiming unit's span — can
   therefore never enter the funnel. The fork's real catch-shape is "name minted at a
   (possibly wrong/diverged) VA in the claiming unit's own span, byte-identical body
   exists elsewhere" (misattribution-recovery / ICF-duplicate COMDATs), which is what
   its 3 unsafe-mode promotions are (all 104-byte STL `_Vector_base` folds; probe
   reproduced this pass, artifacts `~/tmp/caseb_probe_promos.json`,
   `~/tmp/caseb_probe_report.json`).
2. **The fork's REQUIRED oracle gate rejects all 22 ids anyway**: 0/22 case-B VAs are
   present in `unified_id_rb3wii.json` (verified by script this pass — they came from
   the newer ghidriff/BSim pipeline, not the BinDiff oracle; 20/22 absent entirely, 2
   present with sim 0.006/0.099 pointing at Quazal network code, i.e. misattributed
   rows the gate correctly ignores).

The correct landing mechanism is the one `tools/identity_transfer.py`'s own docstring
names for case-B: **EVICT the fn from the foreign pin** — split the foreign unit's
`.text` range around `[VA, VA+size)` and micro-pin that range under the claiming TU's
own splits header. Multi-range units are long-proven stock machinery: **105 units in
`config/45410914/splits.txt` already have >1 `.text` range** (RockCentral.cpp has 81,
GemPlayer.cpp 18, DepthBuffer3D.cpp itself 6). Eviction costs the foreign unit nothing:
every case-B VA is currently an anonymous 0% `fn_<VA>` in the foreign unit — moving it
neither perturbs the foreign unit's real matches nor changes the whole-binary
denominator. After eviction, stock per-unit pairing + the pre-compile renamer do the
rest; the campaign reduces to the proven v2 worklist port shape (port body → byte-match
→ compose → A/B).

The fork remains a legitimately banked capability (see "Fork status" below) — just not
the critical path here.

## Current state (all VERIFIED 2026-07-02 during doc prep)

- Baseline: `build/45410914/report.json` (mtime Jul 2 12:20) = **10,936 / 65,607
  matched functions, 8.42% strict code bytes, 11.58% fuzzy**. This is already past the
  master doc's 10,870 — RE-MEASURE the baseline in your worktree before any A/B.
- **Fork status changed vs its handoff doc**: `../objdiff` branch
  `caseb-global-byteeq @ b1c92be` is **MERGED into `../objdiff` main** (`git merge-base
  --is-ancestor b1c92be main` → yes; main tip IS b1c92be), and the **shared binary
  `/home/free/code/milohax/objdiff/target/release/objdiff-cli` was rebuilt from it**
  (mtime Jul 2 04:53; `report generate --help` shows `--global-byte-eq`,
  `--global-byte-eq-log`, `--global-byte-eq-oracle`). The pass is **off by default**
  (stock semantics; `objdiff_report_args` in `build.ninja` line 9 is empty, so all
  ninja-driven reports are stock) — main has been building reports with this binary all
  day, which is live do-no-harm evidence. The `../objdiff` repo is checked out on
  `main` with one untracked file (`modify_url.py`) — do not disturb it.
- **Fresh case-B derivation: 22 ids across 17 claiming TUs** (script: intersect
  `band3_port_worklist.json` rows' `rb3_addr` against `config/45410914/splits.txt`
  `.text` ranges, keep rows whose covering span's basename ≠ the row's `src_path`
  basename; none of the 22 VAs is in `scripts/target_symbol_map.json`). The drain-close
  memory said "24 ids / 17 TUs"; the delta is explained by splits landed since:
  `band3/game/TambourineManager.cpp` now has its own span, so its 3 worklist ids became
  own-span case-A (name-only — a free side-quest, see Phase 1), and
  `GemTrainerPanel.cpp` (owner-WIP) appears in the fresh foreign list.
- **BaseSkeleton dead span confirmed**: `config/45410914/splits.txt` lines 2701–2703 —
  `BaseSkeleton.cpp: .pdata start:0x8222AB40 end:0x8222AB90 / .text start:0x82693C20
  end:0x826940A0`. report.json unit `default/BaseSkeleton`: 12 fns, ALL 0%. `fn_82693FF0`
  (in-span) = `PracticePanel::ToggleGuidePart` per the worklist. 0-yield speculative pin;
  removal frees the fn into unowned space.
- **Wired vs unwired claiming TUs** (from `config/45410914/objects.json`): wired =
  Gem (`band3/bandtrack/Gem.cpp`), NetGameMsgs, PracticePanel, RockCentral,
  TrackerManager, AccomplishmentProgress (+ owner-WIP GemTrainerPanel). Unwired =
  TambourineDetector, BandPerformer, FadePanel, TrackConfig, CrowdRating, GameTimePanel,
  InterstitialPanel, PlayerBehavior, VocalScoreHistory, SongSort (`SongSort.cpp` itself;
  only the `SongSortBy*.cpp` variants are wired).
- **The wired TUs' compiled objs ALREADY DEFINE 5 of the case-B bodies** (COFF symbol
  tables parsed this pass): `build/45410914/src/band3/bandtrack/Gem.obj` defines
  `??4Gem@@QAAAAV0@ABV0@@Z`, `?ReleaseSlot@Gem@@QAAXH@Z`,
  `?RemoveAllInstances@Gem@@QAAXXZ`; `.../game/NetGameMsgs.obj` defines
  `??0PlayerStatsMsg@@QAA@PAVUser@@HABVStats@@@Z` (and its Dispatch/Save/Load are
  already 100% in NetGameMsgs' own span — strong prior the ctor body is close);
  `.../game/PracticePanel.obj` defines `?ToggleGuidePart@PracticePanel@@QAAXXZ`. For
  these, "port the TU" is already done; only eviction + naming + byte-match remain.
- **Wii oracle sources exist** for every unwired TU: `../rb3/src/band3/game/
  {BandPerformer,FadePanel,CrowdRating,PlayerBehavior,VocalScoreHistory,
  TambourineDetector}.cpp`, `../rb3/src/band3/meta_band/{GameTimePanel,
  InterstitialPanel,SongSort}.cpp`, `../rb3/src/band3/bandtrack/TrackConfig.cpp`
  (NOT under game/).

### The 22 ids — full triage table (fresh derivation)

Size = retail fn size from `config/45410914/symbols.txt`. "P" = priority tier
(1 = wired/body-defined, 2 = unwired/simple body, 3 = risky body, K = kill/exclude).

| VA | Method (Wii demangled) | Claiming TU | Foreign owning span | Size | P |
|----|------------------------|-------------|---------------------|------|---|
| 0x82693ff0 | PracticePanel::ToggleGuidePart | PracticePanel.cpp (wired) | BaseSkeleton.cpp (dead span → Phase 0) | 88 | **0** |
| 0x82b79348 | Gem::ReleaseSlot(int) | Gem.cpp (wired) | VocalTrack.cpp | 156 | 1 |
| 0x82b79d18 | Gem::RemoveAllInstances() | Gem.cpp (wired) | VocalTrack.cpp | 416 | 1 |
| 0x8229f730 | Gem::operator=(Gem const&) | Gem.cpp (wired) | BandCamShot.cpp | 248 | 1 |
| 0x82b79678 | PlayerStatsMsg::PlayerStatsMsg(User*,int,Stats const&) | NetGameMsgs.cpp (wired) | VocalTrack.cpp | 244 | 1 |
| 0x82b78200 | TrackConfig::AllowsOverlappingGems() const | TrackConfig.cpp (unwired) | VocalTrack.cpp | 84 | 2 |
| 0x82b78288 | TrackConfig::IsRealGuitarTrack() const | TrackConfig.cpp (unwired) | VocalTrack.cpp | 64 | 2 |
| 0x8268a058 | FadePanel::Unload() | FadePanel.cpp (unwired) | band3/game/Player.cpp | 76 | 2 |
| 0x8268a150 | FadePanel::Enter() | FadePanel.cpp (unwired) | band3/game/Player.cpp | 64 | 2 |
| 0x826027f8 | InterstitialPanel::Exiting() const | InterstitialPanel.cpp (unwired) | band3/meta_band/EditSetlistPanel.cpp | 88 | 2 |
| 0x826022a0 | GameTimePanel::Enter() | GameTimePanel.cpp (unwired) | band3/meta_band/EditSetlistPanel.cpp | 120 | 2 |
| 0x826d0108 | PlayerBehavior::PlayerBehavior() | PlayerBehavior.cpp (unwired) | band3/game/VocalPlayer.cpp | 100 | 2 |
| 0x826ddca0 | TambourineDetector::CheckForSwing(...) | TambourineDetector.cpp (unwired) | DepthBuffer3D.cpp | 264 | 3 |
| 0x826cef70 | BandPerformer::ComputeScoreData(...) | BandPerformer.cpp (unwired) | band3/game/VocalPlayer.cpp | 180 | 3 |
| 0x826cf150 | BandPerformer::NoOneContributingToCrowd() const | BandPerformer.cpp (unwired) | band3/game/VocalPlayer.cpp | 152 | 3 |
| 0x827a9768 | AccomplishmentProgress::SendHardCoreStatusUpdateToRockCentral | AccomplishmentProgress.cpp (wired) | NetCacheMgr.cpp | **20** | K (≤44B ICF-fold class) |
| 0x826cf9b0 | CrowdRating::GetThreshold(...) const | CrowdRating.cpp (unwired) | band3/game/VocalPlayer.cpp | **24** | K (≤44B) |
| 0x825a66e8 | NodeSort::GetShortcutIx() const | SongSort.cpp (unwired) | band3/meta_band/StoreSongSortNode.cpp | **32** | K (≤44B) |
| 0x824e5190 | stlpmtx_std::pair<Symbol,DataNode> ctor (template) | RockCentral.cpp (wired) | FlowSound.cpp | 80 | K (STL COMDAT fold — the exact misattribution class every gate exists to reject) |
| 0x82b62248 | TrackerManager::HandleGameOver() | TrackerManager.cpp (wired) | band3/bandtrack/GemTrack.cpp | — | K (`lbl_82B62248`, NOT a function boundary in symbols.txt; 8 bytes to next fn) |
| 0x826dd928 | VocalScoreHistory::AddScore(...) | VocalScoreHistory.cpp (unwired) | DepthBuffer3D.cpp | — | K (`lbl_826DD928`, not a function; ~0x44 gap to next fn — needs symbols.txt surgery first) |
| 0x8268c138 | GemTrainerPanel::IsGemInFutureLoop(...) const | GemTrainerPanel.cpp | band3/game/Player.cpp | 112 | K (**OWNER-WIP — never touch**) |

**Actionable pool: 15 ids across 10 claiming TUs** (P0×1, P1×4, P2×7, P3×3).

### Owner-WIP TUs — NEVER TOUCH

Band, Track, Game, Tracker, BandProfile, **GemTrainerPanel**, ClosetMgr, GemSmasher,
BandTrack (and their sources/splits/map entries) are being ported concurrently by the
project owner. Do not edit, pin, rename, or evict anything attributed to them. The
GemTrainerPanel case-B id above is excluded for this reason — leave it for the owner.

## Evidence & references

- Master doc: `docs/plans/frontier-workstreams-2026-07-02.md` (stream 5 row).
- Fork handoff (now partially stale — see "Fork status"): `docs/decomp/handoff/objdiff-caseb-fork-banked.md`.
- Fork source: `/home/free/code/milohax/objdiff`, commit `b1c92be` (= main tip);
  pass code `objdiff-core/src/diff/mod.rs` (`reconcile_global_byte_matches`,
  `CASEB_STUB_MAX=44`, `CASEB_ORACLE_SIM_MIN=0.5`, `load_va_oracle` reads any JSON list
  of `{rb3_addr, bindiff_src, similarity}`), driver `objdiff-cli/src/cmd/report.rs`.
- Drain-close memory: `~/.claude/projects/-home-free-code-milohax-rb3-xenon/memory/project_worklist_drain_close_2026-07-02.md`.
- Worklist: `/home/free/code/milohax/rb3-xenon/band3_port_worklist.json` (232 rows;
  schema `{rb3_addr, wii_symbol, wii_demangled, tu, src_path, match_type, simconf}` —
  NO status field; case-B-ness is derived, not stored).
- Body-divergence wall (the campaign's dominant risk):
  `docs/decomp/identity-transfer/B2-FINDINGS-oracle-wall.md` — 0/10 fresh blind TU
  harvests landed; MWCC→MSVC game bodies diverge (BandProfile 0/64). BUT waves 3–4 of
  the v2 per-function port workflow landed +15 on this same worklist — per-fn
  body-porting with reviewer reproduction beats blind whole-TU harvesting.
- Tooling (all verified present): `tools/band3_worklist_pin.py` (micro-pin + name for
  worklist ids; `locate()` classifies inpin-own/foreign/unowned),
  `tools/identity_transfer.py` (`--pin-only`, `--apply`, `--allow-span-coexist`,
  `--deferred-out` = case-B eviction worklist), `tools/field_offset_gate.py`,
  `tools/oracle_quality.py` (`--tu`), `tools/icf_alias_check.py`,
  `scripts/idtransfer_harvest.py` (gated one-TU driver), `scripts/harvest/overlap_check.py`,
  `scripts/harvest/land.sh`, `tools/fresh_report.sh`.
- Probe artifacts from this pass (regenerate anytime, read-only):
  `~/tmp/caseb_probe_promos.json` + `~/tmp/caseb_probe_report.json` — funnel on current
  objs: `named_unmatched>44B=1911, have_base_body=850, sig_in_retail_index=3 → 3
  promotions` (all STL folds, all would be oracle-rejected).

## Step-by-step procedure

All work in a CoW worktree under `~/tmp` (NEVER `/tmp`), e.g.
`scripts/setup_worktree.sh ~/tmp/wt-caseb caseb-campaign`. Never commit/land from the
lane — return patches for coordinator integration (`scripts/harvest/land.sh`).
Build with `./tools/ninja-locked 2>&1 | tee ~/tmp/rb3_build_caseb.log`.

### Phase 0 — BaseSkeleton dead-span removal + PracticePanel::ToggleGuidePart (+1, trivial)

1. In the worktree, delete the whole `BaseSkeleton.cpp:` block from
   `config/45410914/splits.txt` (both the `.pdata 0x8222AB40–0x8222AB90` and `.text
   0x82693C20–0x826940A0` lines; verify first it is still 12×0% via report.json — it
   was at doc-prep time). Also remove `BaseSkeleton.cpp` from
   `config/45410914/objects.json` if present (check first; a dangling splits-less entry
   is harmless but tidy).
2. `touch config/45410914/config.yml && ./tools/ninja-locked` (dtk re-splits; the 12
   fns return to an unowned auto blob — zero match loss, all were 0%).
3. `python3 tools/band3_worklist_pin.py --tu PracticePanel.cpp` (dry-run) — expect it
   to classify 0x82693ff0 as UNOWNED now, proposing a micro-pin `[0x82693FF0,
   0x82694048)` + name `?ToggleGuidePart@PracticePanel@@QAAXXZ`. Then `--apply`,
   rebuild, and measure `default/PracticePanel` per-unit. PracticePanel.obj already
   defines the method; if <100%, body-port from
   `../rb3/src/band3/game/PracticePanel.cpp` per the v2 per-fn workflow.
4. Composed A/B (`tools/fresh_report.sh` in the worktree) vs the worktree's recorded
   baseline; `python3 tools/icf_alias_check.py` over the delta; return the patch.

### Phase 1 — Tier-1 evictions (wired TUs, bodies already defined): Gem ×3, NetGameMsgs ×1

Per id (start with 0x82b79678 PlayerStatsMsg ctor — its sibling methods are already
100%, best prior):

1. Fn boundary from `config/45410914/symbols.txt` (e.g. `fn_82B79678 size:0xF4` →
   `[0x82B79678, 0x82B7976C)`).
2. **Evict**: in `splits.txt`, split the foreign unit's `.text` range at the boundary
   (foreign gets pre + post ranges; multi-range is normal — 105 units already do it)
   and add the carved range under the claiming TU's existing header (`Gem.cpp:` /
   `NetGameMsgs.cpp:` — use the exact splits header spelling already present). Run
   `python3 scripts/harvest/overlap_check.py` to prove no overlap.
3. Name: add the VA→mangled entry ADD-ONLY to `scripts/target_symbol_map.json`
   (uppercase `0X...` key format) — or run `python3 tools/band3_worklist_pin.py --tu
   <TU>` post-eviction, which now sees the VA as inpin-own and does the safe
   demangle-and-match naming itself. Never run `gen_game_target_map.py --apply`
   wholesale on a scattered TU.
4. `touch config/45410914/config.yml && ./tools/ninja-locked`, then measure the
   claiming unit AND the foreign unit (its strict count must be unchanged — e.g.
   VocalTrack/BandCamShot matches must not move).
5. If the carved fn is <100%: body-port from the Wii oracle source (v2 shape: per-fn,
   `mcp run_objdiff` with `project_dir=<worktree>`, lane measures per-symbol only — no
   whole-binary builds until integration). Known lesson: lane readings of 98–99.9%
   often land as TRUE 100 after the composed renamer resolves anon-reloc naming — do
   not discard them.
6. Kill an id (revert ITS eviction cleanly, keep the rest) per the criteria below.

### Phase 2 — Tier-2 unwired-TU ports (TrackConfig, FadePanel, InterstitialPanel, GameTimePanel, PlayerBehavior)

Per TU, cheapest-body first (TrackConfig 84/64B accessors → FadePanel 76/64B panel
boilerplate → InterstitialPanel 88B → PlayerBehavior 100B ctor → GameTimePanel 120B):

1. `python3 tools/oracle_quality.py --tu <TU>.cpp` FIRST — not as a gate on the
   worklist ids (their identity source is ghidriff, higher precision), but to see
   whether the TU brings byproduct case-A yield that justifies a fuller port
   (doc-prep counts of sim≥0.5 unified-oracle rows: TrackConfig 3, FadePanel 2,
   BandPerformer 1, CrowdRating 2, VocalScoreHistory 1, SongSort 1, others 0).
2. Port the source `../rb3/src/band3/.../<TU>.cpp` → `src/band3/.../<TU>.cpp`
   (Wii→360: MWCC→MSVC idioms; see `docs/decomp/patterns/` + the v2 wave lessons).
   Add to `config/45410914/objects.json` as `NonMatching`; `python3 configure.py`;
   compile clean.
3. Evict + name + measure exactly as Phase 1 steps 1–6. Note: eviction is ALSO what
   creates the TU's objdiff unit (units are splits-driven; a compile-only TU is
   invisible to the report — verified: objects-only TUs have no `objdiff.json` unit).
4. Per-TU budget: if the FIRST id of a TU body-diverges beyond a ~2-hour port effort,
   record the divergence class and move to the next TU (don't grind — the B2 findings
   say game-body divergence is a wall, not a puzzle).

### Phase 3 — Tier-3 risky bodies (TambourineDetector, BandPerformer)

Same procedure as Phase 2 but attempt ONLY after Phases 0–2 have landed something
(they calibrate your per-body effort). `CheckForSwing` (264B) and `ComputeScoreData`
(180B) are float/scoring logic — the MWCC→MSVC divergence class that killed
BandProfile 0/64. Timebox hard: one session each, kill on the first structural
divergence (register-file-wide mismatch, different inlining shape) rather than
permuting.

### Phase 4 — Fork close-out (bookkeeping, not on the critical path)

1. Update `docs/decomp/handoff/objdiff-caseb-fork-banked.md` with the verified status:
   MERGED to `../objdiff` main @b1c92be, shared binary rebuilt 2026-07-02, off by
   default, do-no-harm evidenced by production use. Fold in the two findings from this
   doc's REFRAME section (target-driven items; oracle-gate coverage gap) so the next
   reader doesn't re-derive them.
2. If (and only if) a future harvest wants the fork's pass in a measure: build the
   bridged oracle first — `unified_id_rb3wii.json` rows + ghidriff-worklist rows
   re-emitted as `{rb3_addr, bindiff_src: <src_path>, similarity: 0.9}` (the parser
   `load_va_oracle` accepts any JSON list with those three keys; 0.9 = the
   human-validated ACCEPT-tier precision — document this semantic difference in the
   bridge file's provenance field). Every `--global-byte-eq-log` promotion must be
   re-audited with `tools/icf_alias_check.py` before the count is trusted. Keep normal
   measures stock (leave `objdiff_report_args` empty / flags off).

## Honesty gates & verification

- **Reproduce every number yourself** at execution time: baseline matched count from
  YOUR worktree's `fresh_report.sh` run, not this doc.
- **Per-id**: strict 100 in the composed report (not just a lane objdiff reading);
  `tools/icf_alias_check.py` over every new match (real-bodied >44B, correct-TU
  attribution); foreign unit's matched set unchanged after eviction (diff its
  report.json unit entry before/after).
- **Per-patch**: whole-binary composed A/B in the worktree, net ≥ +1 with 0
  regressions; `scripts/harvest/overlap_check.py` clean; diff hygiene (no
  `download_tool.py`/venv/regenerable-artifact leakage — the wave-3 lesson).
- **Never** name a case-B VA in `target_symbol_map.json` WITHOUT the eviction: naming
  the VA while it is still in the foreign span renames the foreign unit's carved
  symbol, creating a 0% named item in the foreign unit (denominator noise) and
  removing the body from any future byte-index (fork-shape) — strictly worse than
  leaving it anonymous.
- ≤44B ids stay killed even though eviction *could* pin them: byte-equality on stubs
  asserts nothing about ownership (the wave-14/15 +57-fake lesson, codified in
  `icf_alias_check.py`).

## Kill criteria

- **Per id**: body-port exceeds ~2h without reaching ≥99.5% lane reading → record
  divergence class in the lane notes, revert that id's eviction, move on.
- **Per TU (unwired)**: TU source won't compile within ~1h of decl-stubbing (missing
  half the header universe) → defer TU, note blockers.
- **Phase 3 global**: if Phases 0–2 land <5 strict total, do NOT start Phase 3 — the
  vein is thinner than modeled; report and stop.
- **Campaign**: any eviction that moves a foreign unit's existing strict match = STOP,
  revert, re-check fn boundaries against symbols.txt before continuing.
- **Do not** re-run blind whole-TU identity-transfer harvests (0/10 proven,
  `B2-FINDINGS-oracle-wall.md`); do not touch owner-WIP TUs; do not make the fork pass
  part of the default measure without the Phase-4 bridge + audit.

## Expected yield

- Phase 0: **+1** (ToggleGuidePart; +2 if the freed blob lets the pin tool name a
  neighbor — do not count on it).
- Phase 1: up to **+4** (bodies already defined; PlayerStatsMsg ctor is the strongest
  prior). Realistic: +2–4.
- Phase 2: up to **+7** across 5 TUs; body-divergence history says expect **+3–5**,
  plus possible case-A byproduct from the newly wired TUs (uncounted).
- Phase 3: up to **+3**, realistic **+0–2**.
- **Campaign realistic total: +6–12 strict** (ceiling +15). The fork's +150–220
  "ceiling" from the handoff doc is NOT this campaign's yield — it is a future-harvest
  capability number, gated on identity micro-pins minting names at scale AND bodies
  ported byte-exact AND the oracle bridge; treat it as WS2-regen-dependent, not WS5.

## Open questions

1. The two `lbl_` ids (TrackerManager 0x82b62248, VocalScoreHistory 0x826dd928) — are
   these real function entries dtk mis-classified (jumptable targets?), and is
   symbols.txt label→function surgery safe/worth it for ≤68-byte bodies? (Deferred;
   inspect in Ghidra port 8002 if picked up.)
2. Does removing BaseSkeleton.cpp from splits require a matching `objects.json` edit,
   or was it splits-only speculation with no compiled source? (Check
   `git log --follow -- config/45410914/splits.txt` for the pin's origin commit.)
3. Should `tools/project.py` learn to emit base-only objdiff units for compile-only
   TUs (would give ported-but-unpinned TUs report visibility, and — combined with a
   fork tweak to enumerate base-side items — restore the fork's original case-B
   promise without eviction surgery)? Filed as a tooling idea; NOT needed for this
   campaign.
4. Gem's `operator=` id (0x8229f730) sits in an ENGINE unit's span (BandCamShot.cpp,
   0x8229xxxx region) far from Gem's other bodies (0x82b7xxxx) — verify the ghidriff
   identity by eyeballing the 248-byte body against `../rb3/src/band3/bandtrack/
   Gem.cpp`'s `operator=` before evicting (highest misattribution risk of the P1 set).
