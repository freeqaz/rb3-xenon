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

| phase | state | before → after | commit |
|---|---|---|---|
| 0 baseline | | | |
| 1 function names | | | |
| 2 data names | | | |
| 3 prototypes | | | |
| 4 class layouts | | | |
| 5 repeatable | | | |

## Not in this lane

- Importing clean retail TU5 as its own program (removes the 53-word caveat; costs a full
  re-analysis and renames the program every tool addresses).
- Naming functions the map does not name, including the AuditionMgr TU around `0x82560CB0`: that
  lane (`~/tmp/wt-audmgr`) adds map rows, and the repeatable command picks them up after it lands.
- Fork work in `../ghidra` or `../pyghidra-mcp` beyond what a phase strictly needs; a rebuild of
  either is a deployment every lane feels.
