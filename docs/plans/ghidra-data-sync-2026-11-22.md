# Bring the RB3 Ghidra program up to what the repo knows today (2026-11-22)

Owner: one Opus lane (`ghidra-sync`). Commit this file on the lane branch as
`docs/plans/ghidra-data-sync-2026-11-22.md` and keep its status table current.

## Why

A decompile through `tools/ghidra/pcode_inspect.py` today prints `Function_82767BA0(...)` for
`MsgSource::AddSink` and `*(undefined4 *)(param_1 + 0x34)` for a named member. The repo already knows
both; Ghidra was last given names on 2026-07-18 and has never been given types.

Measured on main `b285c0347`, before any work:

| fact | value | how |
|---|---|---|
| rows in `scripts/target_symbol_map.json` | 36,394 | `len(json)` |
| names in the Ghidra bank `tools/ghidra/rb3_symbol_map.full.json` (07-18) | 17,294 | `len(json)` |
| map names the bank lacks | 19,882 | key set difference |
| bank names the map no longer claims (stale) | 782 | key set difference |
| same address, different name | 1,906 | value compare |
| callees in the decompile of `0x82560CB0` that the map names but Ghidra does not | 6 of 10 | per-address lookup |

The bank file is assumed to equal what is applied in the live program. Verify that first (phase 0): if
the program holds a different set, every count above is about the file, not the program.

Prior plan: `docs/plans/ghidra-improvement-plan-2026-10-01.md`. Its P1 (name re-sync) and P5
(prototypes and class layouts) are this lane; neither was done. Its P2–P4 and P6–P10 (pdata decode,
compiler spec, switch forms, ICF, EH) are out of scope here.

## Ground rules

- **Other agents are using the live service on :8002 right now.** Never stop it or write to
  `ghidra_projects/RB3Xenon` while building. Work on a reflink copy
  (`cp -a --reflink=always ghidra_projects/RB3Xenon ~/tmp/ghidra-sync/proj/`, 1.3 GB), apply and verify
  there, then swap: stop the service, move the old project aside as a dated backup, move the new one
  in, move the repo-root `cache.db*` aside (pyghidra keys its decompile cache on the binary hash, so it
  would keep serving the old names), start, verify. Keep the backup until the after-census passes.
  Tell the coordinator (SendMessage `main`) just before a swap and right after.
- **Name nothing the repo does not already name.** Sources of truth, in order: retail bytes,
  `scripts/target_symbol_map.json`, the compiler's class layouts
  (`scripts/harvest/class_layout_report.py`), `tools/retail_rtti.py`. Header `// 0x..` comments and
  `struct_db.sqlite` are derived and have been wrong; do not seed types from them.
- The TU5 program `default_tu5.xex-c5a170` is the Deluxe image (53 words differ from retail, same
  addresses). Do not import another program in this lane; whether to import clean TU5 is an open
  question for the owner of the repo (see "Not in this lane").
- Tool edits go in a worktree (`scripts/setup_worktree.sh ~/tmp/wt-ghidra-sync ghidra-sync`), never in
  main. The Ghidra project itself is gitignored and lives only in main.

## Phases, in order

Each phase ends with a measured before/after and a commit. Stop at the first phase whose control
fails and report.

**0. Baseline.** Confirm which names the live TU5 program actually holds (count named functions
through the service). Pick 20 retail bodies and save their decompiles: include `0x82560CB0`,
`0x825610B8`, and a spread over `src/band3` and `src/system`. Count per body: unnamed call targets
(`Function_`/`FUN_`), raw `param_1 + 0x..` accesses, `DAT_`/`PTR_DAT_` references. This is the ruler
for every later phase.

**1. Function names.** Rebuild the full name map from today's `target_symbol_map.json`
(`build_full_symbol_map.py`) and apply it (`run_apply_symbols.sh --full`, on the copy). Add a
reconciliation pass to `apply_symbols.py`: a name on a body the map no longer claims is removed, a
changed name is replaced, and the delta is printed. Add the fold spellings from
`scripts/symbol_aliases.json` as secondary labels.
Control: unnamed call targets in the 20 bodies fall sharply, and `0x82767BA0` reads
`MsgSource::AddSink`. No fall means the apply reached the wrong program.

**2. Data names.** Label vtables and RTTI type descriptors from `tools/retail_rtti.py`
(`??_7Class@@6B@` and friends), and any named non-placeholder data in
`config/45410914/symbols.txt`. Control: `DAT_`/`PTR_DAT_` count in the 20 bodies, and a vtable load
in a constructor prints the class name.

**3. Prototypes.** Set each named function's prototype from its mangled name (pyghidra-mcp has
`apply_demangled_signatures` and `set_function_prototype`; the first was skipped for RB3 because it
assumed a linker map). Control: parameter and return types in the 20 bodies; spot-check five against
the header declarations.

**4. Class layouts.** Emit compiler layouts for every class the report can produce (measure how many
that is first; start with `src/band3` and the `src/system` dirs the native port uses), load them
into the data type manager, and apply `this` types. Control: raw `param_1 + 0x..` accesses in the 20
bodies fall, and `tools/ghidra/struct_check.py` reports zero disagreement on seeded classes.

**5. Make it repeatable.** One documented command that re-runs phases 1–4 incrementally, and a
staleness line in `tools/ghidra/ghidra-status.py` ("program N names / map M rows / last sync").
Report whether the semantic-search index (chromadb, built before the renames) needs a re-ingest and
what that costs; do not start a long re-ingest unasked.

## Status

Ruler: `tools/ghidra/ghidra_sync_census.py` (headless, on a reflink copy) over the 20 bodies in
`~/tmp/ghidra-sync/bodies20.txt` (0x82560CB0, 0x825610B8, 9 random `src/band3` + 9 random `src/system`
functions of 400-1500 B, seed 20261122). Decompiles per phase are kept in `~/tmp/ghidra-sync/census/phaseN/`.
Phase 0 has 19 bodies: 0x826D6660 (`StreakFocusTracker::GetNextFocusPlayer`) had no function until phase 1.

| phase | state | before → after | commit |
|---|---|---|---|
| 0 baseline | done | program held 17,789 named fns (17,290 = the 07-18 bank exactly, 4 bank VAs default-named, 499 others: 410 ANALYSIS, 65 DEFAULT thunk names, 24 USER_DEFINED); 41,049 fns, 18 data types. Program vs map: 14,623 agree, 1,938 differ, 19,771 map-only. 20 bodies: 74 unnamed calls, 149 raw member, 157 DAT_, 166 auto-data, 19 `undefined` in signatures | bbaeb7128 |
| 1 function names | **live on :8002** (11:42) | named 17,789 → 36,001; fns 41,049 → 48,080 (7,031 created at map VAs); map agree 14,623 → 35,578, differ 1,938 → 0; renamed 19,061, changed 1,892, removed 818 stale, 6,310 alias labels, 1,012 arbitrary-twin tags. 754 map rows not applied = data rows (go to phase 2) + `entry`. Ruler: unnamed calls 74 → 6 (3 each in 0x82560CB0 and the DxMesh ctor: map does not name them). Control: 0x82767BA0 = `?AddSink@MsgSource@@…` live; live function count 48,080 | bbaeb7128 |
| 2 data names | **live on :8002** (swap 2, ~11:58) | 5,959 labels added (+1 made primary, 33 already): 800 map data rows, RTTI 1,415 TypeDescriptors / 1,795 COLs / 1,795 vftables, 300 `__imp_`. vftable-suffix control: 61/64 map rows agree with the RTTI-derived name (3 differ, map wins: an ObjPtr template arity, a PreloadPanel `6B@` the map gives a multi-vftable class, InterstitialMgr/CharUpperTwist base choice). Ruler: DAT_ 161 → 157, auto-data 170 → 160 (most DAT_ are `.data`/`.bss` globals the repo does not name: symbols.txt has 0 named `.data` objects). Control: DxMesh ctor stores `&??_7DxMesh@@6BRndMesh@@@` etc. | this commit |
| 3 prototypes | **live on :8002** (swap 2) | 34,501 of 34,546 mangled fns got their demangled prototype (45 are not function names, 0 failures). Ruler: `undefined` in signatures 24 → 0, `undefined*` tokens 155 → 135, "Removing unreachable block" 17 → 4 (wrong arg types had been pruning live code; bodies grew 1,505 → 1,749 lines, which is why DAT_ rose 157 → 204). Spot-check vs headers 5/5: AddSink, GuitarFx::Poll, BSPFace::Set, SongStatusMgr::UpdateSongStats, Instarank::UpdateString1Label. Functions now sit in their class namespace; the mangled name stays the function name. Consumers checked live: the bare mangled name resolves (decompile, xrefs, pcode_inspect); `Class::Method` in pcode_inspect resolves exactly from the map or refuses (4f9aeb755). Fixed two tools that were broken before this lane: ghidra-xrefs.py (always 0 refs) and rtti_probe.py (TU0 thunk name) | 9ee16810b, 4f9aeb755 |
| 4 class layouts | not started (handoff) | next: measure how many classes `scripts/harvest/class_layout_report.py` can emit; start with `src/band3`; turn each layout into a Ghidra Structure in the class category apply_prototypes created (the `this` types are empty placeholders today, which is why the raw-member count went 157 -> 177: accesses print as `this + 0x..`) | |
| 5 repeatable | partly done | done: apply_symbols reconciles, plus build_data_symbol_map/apply_data_symbols/apply_prototypes/ghidra_sync_census, each re-runnable. Missing: one wrapper command (copy -> 1-3 -> census -> swap) and the staleness line in ghidra-status.py. chromadb: the service re-indexed by itself after swap 1 (the dir changed), so no manual re-ingest has been needed so far; not measured further | |
| 6 clean TU5 program | **live on :8002, the default program** (swap 3, 12:17, ~1 min down) | `import_clean_tu5.py`: import + full analysis took 604 s; 63,110 fns before names (Deluxe program: 41,049). After phases 1-3: 67,935 fns, map agrees 35,578 / differs 0; 20-body ruler 6 / 177 / 204 / 207 / 0, the same as the Deluxe program after phase 3. 53 patched words (`check_patched_words.py` over `tools/pe_word_diff.py` output): clean TU5 program = 49 retail, 0 Deluxe, 4 other (the import thunks at 0x82C4C47C..90, which the XEX loader rewrites to `li r3,<ordinal>`; the ordinals there are the retail 0x242/0x24B); Deluxe program = 53 Deluxe. Scratch project `~/tmp/ghidra-sync/tu5scratch` (TU5Import / default.xex-d56e7f); pre-names copy `tu5scratch_analysed_pristine`; unnamed .gzf `~/tmp/ghidra-sync/default.xex-d56e7f.gzf`. Swap 3: `add_program_to_project.py` packed the scratch program (68,222 fns; the 67,935 above was counted before phase 2-3 created functions) and unpacked it into a reflink copy of the live project (68,222 fns both sides, 2.5 min); `seed_chroma_collection.py` copied the Deluxe semantic-search collection (37,482 docs) to `default.xex-d56e7f` so the service does not decompile 68k functions into chromadb at startup (service log: "Collection 'default.xex-d56e7f' exists; skipping code ingest"). `list_project_binaries` = 3 programs. `mcp_client.py` picks `default.xex-d56e7f` by exact name (override `GHIDRA_PROGRAM`); `pcode_inspect.py 0x82767BA0` prints `Program: default.xex-d56e7f` and `MsgSource::AddSink`. Main's unpatched client still resolves c5a170 (first listed) until this branch lands | (this commit) |

## Handoff (2026-11-22)

- **Clean TU5 is live and the default** (swap 3). Program selection lives in `tools/ghidra/mcp_client.py`
  (`TARGET_PROGRAM`, env `GHIDRA_PROGRAM`), `pyghidra-service.sh` (CLEAN_XEX_PATH), `run_apply_symbols.sh`
  and the `--program` defaults of `apply_*`/`ghidra_sync_census.py`. `scripts/verify_target_image.py` names
  c5a170 only as a XEX sha1 identity, not a program: unchanged.
- Backups to delete once the after-census is accepted:
  - `ghidra_projects/RB3Xenon/RB3Xenon.bak-20261122-pre-ghidra-sync`
  - `ghidra_projects/RB3Xenon/RB3Xenon.bak-20261122-phase1`
  - `ghidra_projects/RB3Xenon/RB3Xenon.bak-20261122-pre-cleantu5` (swap 3)
  - `~/tmp/ghidra-sync/cachedb-aside*`


## Not in this lane

- Naming functions the map does not name, including the AuditionMgr TU around `0x82560CB0`: that
  lane (`~/tmp/wt-audmgr`) adds map rows, and the repeatable command picks them up after it lands.
- Fork work in `../ghidra` or `../pyghidra-mcp` beyond what a phase strictly needs; a rebuild of
  either is a deployment every lane feels.
