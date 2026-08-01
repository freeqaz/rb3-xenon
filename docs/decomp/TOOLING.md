# rb3-xenon TOOLING — audited inventory (2026-07-29)

> **How this doc was made, and what "audited" means here.** Every Python tool in
> `tools/`, `tools/ghidra/`, `scripts/`, `scripts/{analysis,triage,grind,recarve,orchestrator,harvest}/`
> was (a) AST-parsed, (b) checked for `argparse`, (c) actually invoked with `--help`
> under a 30 s timeout when argparse-confirmed, and (d) statically screened for the
> stale-build-dir glob defect (§3). **208 files** in the non-`harvest` dirs and
> **138 files** in `scripts/harvest/` were covered. Nothing in this doc is asserted
> from a docstring alone where a run was possible. Numbers here are *measured on
> main on 2026-07-29* — re-measure rather than quoting them next month.
>
> Scope note: this is an inventory of the *decomp* toolchain. The build system
> itself (`configure.py`, `tools/project.py`, wibo/objcache/jeff/objdiff forks,
> PCH) is documented in `../CLAUDE.md`; only its Python entry points are listed here.

---

## 0. Start here for task X

| I want to… | Use | Not |
|---|---|---|
| know the current match count | `build/45410914/report.json` (`matched_functions`), `scripts/get_progress.py`, `/progress` skill | any doc's number — they age in days |
| see an honest priority-tiered denominator | `tools/scope_map.py report` (rebuild cache first: `python3 tools/scope_map.py build`) | dtk's "mapped" box — different definition (§2) |
| diff one function | MCP `run_objdiff` / `run_analyze_function`; `/compare-asm`, `/recon` skills | raw objdiff-cli unless you need data diffs (`/data-diff`) |
| understand WHY a function won't match | MCP `run_diff_inspect` (modes below), `/stack-layout` | guessing from % |
| get a class's true layout | `scripts/harvest/class_layout_report.py <Class>` (asks `cl.exe`) | `// 0xHEX` header comments or `struct_db.sqlite` — measurably wrong in places |
| read retail's ACTUAL bytes at a VA | **`orig/45410914/band.exe`** — the decompressed retail PE (§1) | inferring from the symbol map |
| find the source for a symbol | MCP `lookup_dc3` (engine) then `lookup_rb3wii` (game); `/dc3-pair`, `/rb3wii-pair` | |
| identify an anonymous `fn_8XXXXXXX` | `tools/fingerprint_match.py`, the homing/content-join family in `scripts/harvest/` | `unified_id*.json` / `ghidriff_identities.json` — **TU0-keyed, VA-DEAD** |
| move / add / audit a `splits.txt` pin | `scripts/harvest/splits_move.py`, `overlap_check.py`, `scripts/audit_splits.py`, `find_truncated_splits.py` | hand-editing `.pdata` — it is derived output (§2) |
| land a lane patch onto main | `scripts/harvest/land.sh` (+ `resolve_json_union.py`) | `resolve_splits_union.py` when the patch's value is a **deletion** — it cannot propagate removals |
| set up an isolated buildable tree | `scripts/setup_worktree.sh ~/tmp/wt-foo foo` | bare `git worktree add` (unbuildable), `/tmp` (tmpfs, no reflink) |
| enumerate LIVE build artifacts | `scripts/harvest/live_units.py` — and read its **basename-collision caveat** (§3) | globbing `build/45410914/{asm,obj}` — **69.7% of it is stale today** |
| delete the stale carve artifacts | `scripts/prune_orphan_asm.py` (the remedy tool) | mtime filtering — **refuted**, see §3 |

---

## 1. Ground-truth artifacts (know which oracle you are reading)

These are the inputs every tool joins against. Reading the wrong one is the
single most common source of a confidently-wrong finding in this repo.

| artifact | what it is | trust |
|---|---|---|
| **`orig/45410914/band.exe`** | ★ **the DECOMPRESSED RETAIL PE**, imagebase `0x82000000`, extracted from `default.xex`. Lets you read retail's *real* bytes, literal pool, and `.pdata` at any VA. Also **dissolves the ICF confounder** (you see the one physical body, not N symbol aliases). **40 tools already consume it**; treat it as the primary retail oracle. | GROUND TRUTH |
| `orig/45410914/default.xex` | the TU5 retail XEX (target). `default_tu5.xex` is the same bytes; `tu0-archive/` is the pre-2026-07-15 target — **every TU0-era address is invalid**. | GROUND TRUTH |
| `objdiff.json` | dtk's per-unit manifest, regenerated on every split run. **The single source of truth for which build artifacts are LIVE.** 3,862 live units today. | GROUND TRUTH (regenerated) |
| `build/45410914/report.json` | per-unit/per-fn match results. `match_percent_normalized` = strict; `fuzzy_match_percent` = fuzzy; a field called `match_percent` does NOT exist. Its per-fn `address` in catch-all units is a **monotonic index, not a VA**. | trustworthy for %, NOT for geometry |
| `config/45410914/splits.txt` | per-unit section ranges. `.text` is INPUT; **`.pdata` is DERIVED OUTPUT** — cleared and re-derived on every split run. | INPUT (`.text` only) |
| `config/45410914/symbols.txt` | both a dtk INPUT and a regenerated OUTPUT. Drift hard-fails the split ("ends within symbol") and leaves main unbuildable with no `report.json`. | fragile — never hand-edit casually |
| `scripts/target_symbol_map.json` | addr → mangled name, consumed by `obj_target_symbol_renamer.py` pre-compile. A wrong entry produces a **100%-scoring function with the wrong body** (§4). | curated, provably imperfect |
| `decomp.db` | orchestrator SQLite; seeded from `report.json` via `scripts/ingest_report.py`. Carries `unicorn_*` behavioural verdicts. | derived — re-ingest after landings |
| `config/45410914/scope_map.json` | **gitignored cache**, addr-keyed to ONE target build. Stale/absent ⇒ every tier % reads INFLATED. Rebuild: `python3 tools/scope_map.py build` (~1 s). Present on main, dated 2026-07-29. | rebuild before quoting |
| `../dc3-decomp` | Dance Central 3: same Milo engine, same MSVC X360, same `/O1 /Oi /GR /EHsc`, **named functions from a leaked `ham_xbox_r.map`**. Primary ENGINE oracle. | strong (newer engine — cross-check) |
| `../rb3` | rb3-Wii **dev** decomp: named functions + `MILO_ASSERT` path strings the retail Xbox build stripped. Primary GAME oracle. | strong (Wii SKU — port required) |
| `unified_id*.json`, `ghidriff_identities.json` | ⛔ **TU0-keyed and VA-DEAD.** BinDiff's 263 IDs live here; size agreement is ~chance-level. Consumed by ~12 tools that predate the TU5 flip. | DO NOT TRUST |

---

## 2. Two traps that are arithmetic, not opinion

- **"mapped" is two different numbers.** dtk's progress box says *mapped* for bytes
  **pinned** to a `splits.txt` unit. `tools/scope_map.py` counts bytes
  **tier-classified** by any of its 8 layers, pinned or not — always larger. Never
  compare them.
- **`.pdata` in `splits.txt` is derived output.** Every split run clears the whole
  `.pdata` set and re-derives one range per `.text` block (jeff `split.rs:1035`
  `split_pdata`). ★ The 2026-07-27 claim that *deleting* `.pdata` lines loses ranges
  was **RETRACTED** — 54 deleted lines regenerated byte-identical, and a hand-made
  overlap silently healed. Edit `.text` only; `.pdata` follows. (Recorded in
  `../plans/decomp-state-2026-07-19.md` §"RETRACTED 2026-07-27".)
- **A `splits.txt` unit HEADER is not a TU.** `Crowd.cpp:` and `system/world/Crowd.cpp:`
  are two keys for one `.cpp`. Diff by **basename** before claiming first-ever coverage.
- **Site count ≠ defect count.** Only PAIRED functions score, so a fan-out is blast
  radius, never yield.

---

## 3. ★ The stale-build-artifact hazard, and the offender list

**Measured on main, 2026-07-29 (this audit):**

```
build/45410914/asm : 12,994 .s   —  3,932 live,  9,062 stale  (69.7% stale)
build/45410914/obj : 13,016 .obj —  3,954 live,  9,062 stale  (69.6% stale)
objdiff.json live units: 3,862
```

`build/` is never swept: ninja declares only `config.json` as the split rule's
output, and jeff writes each `.s`/`.obj` with a plain create. So every dead
`splits.txt` generation leaves its carve behind **forever**, frozen at that era's
binary geometry — thousands of them `auto_03_*`, many pre-dating the 2026-07-15
TU0→TU5 flip and containing bytes that occur **nowhere in the current target**.
Two independent analyses were silently corrupted this way before the shared
filter existed (a 36% inflation and a 1.91× over-count, per `live_units.py`).

**The sound discriminator is membership in `objdiff.json`**, i.e.
`scripts/harvest/live_units.py`. **mtime is NOT a usable proxy** — 72 of 90 named
orphans carried the same day's date, because `splits.txt` was rewritten between
two split runs minutes apart (measured, `scripts/prune_orphan_asm.py` docstring).
⚠ `docs/INDEX.md`'s known-traps entry used to recommend an mtime filter; that
advice is **superseded** by this measurement.

The remedy tool is **`scripts/prune_orphan_asm.py`** — it deletes the orphans by
`config.json` membership, which fixes every downstream reader at once. Deleting
them is safe (`build/` is gitignored, not a ninja input, regenerated by the next
split).

### ★ NEW DEFECT found by this audit: `live_units.py` joins on BASENAME

`filter_live()` keys on `os.path.splitext(os.path.basename(p))[0]`, and its
docstring asserts "basename is the right join key". **It is not.** Measured:
`filter_live()` over `build/45410914/asm/**/*.s` keeps **3,932** files against
only **3,862** live units — **68 extra files / 34 collided basenames**, e.g.

```
build/45410914/asm/MusicLibrary.s                    723,146 B  (LIVE)
build/45410914/asm/band3/meta_band/MusicLibrary.s      2,325 B  (STALE ORPHAN, kept)
build/45410914/asm/CrowdAudio.s                     218,484 B  (LIVE)
build/45410914/asm/system/bandobj/CrowdAudio.s        1,642 B  (STALE ORPHAN, kept)
```

This is exactly the `Faders.s` collision class that `prune_orphan_asm.py`
documented as unfixable by anything but live-unit membership. Residual
contamination is small (0.5%) but adversarial: the survivors are the **tiny
nested orphans**, which read as "a unit with almost no content" — the shape most
likely to be mistaken for a real finding. **Not fixed by this lane** (4 downstream
consumers would need a rebuild to re-verify). Correct join = the full
`obj/→asm/`, `.obj→.s` transformed path from `live_target_paths()`, not the stem.

### Offenders — tools that read `build/45410914/{asm,obj}` without `live_units`

65 files reference those directories; **only 4** use the filter
(`eh_ground_truth.py`, `reloc_correspondence.py`, `grind/classify_funclets.py`,
`tools/icf_alias_finder.py` — the last two were fixed 2026-07-29 and the fix is
**confirmed present**). The rest split into two risk classes.

**HIGH — enumerate the whole directory (glob/walk/rglob):**

`tools/dc3_content_match.py` · `tools/dc3_residual_rank.py` ·
`tools/game_content_match.py` · `tools/global_fuzzy_index.py` ·
`tools/member_delta_finder2.py` · `tools/map_lint.py` · `tools/pin_audit.py` ·
`scripts/dump_vtable.py` · `scripts/map_verify.py` · `scripts/truncation_audit.py` ·
`scripts/find_truncated_splits.py` · `scripts/recarve/funclets.py`
(uses asm-dir mtime as a **cache key** — doubly wrong) ·
`scripts/recarve/scan.py` ·
`scripts/harvest/`: `map_misassign_repair.py`, `neartwin_cause_census.py`,
`newobj_inline_classify.py`, `objptr_replace_family_scan.py`, `overcarve_scan.py`,
`repin_census.py`, `run_interleave_scan.py`, `sandwich_overcarve.py`,
`scatter_inline_collapse_scan.py`, `scatter_pairing_scan.py`,
`size_order_automap.py` (foundational — inherited by `thunk_callee_bodydiff.py`
and `thunk_callee_freename.py`), `span_predictor.py`, `splits_move.py`
(ironic: its own overlap audit), `thunk_edge_audit.py`, `thunk_identity_namer.py`,
`unemitted_symbol_scan.py`, `unit_scoped_twin_map.py`,
`vtable_1anchor.py` / `vtable_align_diag.py` / `vtable_align_diag2.py` /
`vtable_global.py` / `vtable_multianchor.py`,
`frame_delta_scan.py`, `invcorr_mispair_repoint.py`,
`localstatic_census_wide.py` (self-admitted: 9,911/13,932 rows = 71% stale;
**SUPERSEDED by `localstatic_census_v2.py`**), `localstatic_patch_gen.py`,
`localstatic_population_scan.py`, `localstatic_symbol_scan.py`,
`localstatic_symbol_inbody_scan.py`, `localstatic_tu_census.py`,
`ls_guard_timeline.py`.

**MEDIUM — open a *named* `asm/<unit>.s` by basename** (no enumeration, but the
34 colliding basenames above mean it can silently read another era's geometry):
`handler_list_diff.py`, `gap_content_evidence.py`, `content_join_propose.py`,
`fill_attribution_verify.py`, `funclet_cascade_rank.py`, `argreg_mispair_scan.py`,
`header_inline_policy_scan.py`, `order_anchored_bijection.py`,
`retail_handler_strings.py`, `spill_signature_scan.py`, `stride_consolidate.py`,
`stride_truth_table.py`, `tu5_correlate_global_driver.py`, `tu5_gen_pairs.py`,
`tools/fingerprint_match.py`, `tools/objdiff_to_m2c.py`,
`tools/exploratory/{callgraph_triangulate,vtable_transitivity}.py`,
`tools/ghidra/build_symbol_map.py`, `scripts/obj_*_patcher.py` (these are the
wired build steps — they operate on the objs ninja hands them, so the exposure is
theoretical, but they do glob `obj_dir/**/*.obj` on the fallback path).

**Rule of thumb:** if a scan's conclusion surprises you and the tool is on either
list, re-run it after `scripts/prune_orphan_asm.py` before believing it.

---

## 4. Known-defective tools — do not act on their output

| tool | verdict | detail |
|---|---|---|
| ★ `scripts/harvest/handler_list_diff.py` | **BROKEN — emits FALSE SURPLUS** | `retail_strings()` ends `return order if on else None`: when the target function IS found (`on=True`) but zero `lbl_` refs decode to valid handler strings, it returns **`[]`, not `None`**. The caller's guard is `if rl is None or ours is None: continue` — which `[]` passes. Then `rs = set([])`, so `surplus = [every handler we have]`, `missing = []`, and the verdict is forced to `"SURPLUS"`. **Zero evidence is reported as "retail lacks these".** It produced 9 phantom surplus handlers for `StorePanel` that rb3-Wii confirms retail HAS; acting on it would have been a real regression. One-line fix: `if rl is None or not rl or ours is None: continue`. It also reads `asm/<basename>.s`, so it is on the MEDIUM stale list too. **Marked BROKEN until fixed.** |
| ★ `scripts/harvest/reloc_correspondence.py` | **WORKING as `--census`; UNUSABLE per-symbol** | A single `--symbol` invocation was measured to **time out at 10 minutes**. Root cause: `main()` runs three whole-binary oracle passes *before* the filter applies — `load_matched()` a second time over the entire repo (l.843), `build_base_index()` over every live unit for ICF suppression (l.868), and `build_consistency()` over the whole census (l.873-879, whose own comment says the binding "must be learned from the WHOLE census … or a VA range filter would silently blind the oracle"). `--symbol` (l.847-850) only trims the final emission loop. `--no-icf --no-consistency` bypasses the cost but is **not** the default and weakens the verdict. ⇒ **Not a per-lane gate. Budget it as a whole-binary batch job.** It is one of only 4 tools using `live_units` correctly. |
| `scripts/harvest/autocarve_funnel.py` | **SUPERSEDED** | Derives gap/span geometry from `report.json`'s coalesced `auto_03_*` unit boundaries instead of from `splits.txt` pins. Use **`diffunit_gap_funnel.py`**, whose docstring states the rule outright: *"DERIVE FROM splits.txt, NEVER FROM report.json"* and documents a **27× undercount** of the real gap set from the report.json route. ⚠ **Correction to the received brief:** the figure "1.99× inflation" appears **nowhere** in the tool, its docstring, or `plans/lane-al-autocarve-2026-07-26.md`. The only real measured figures are `live_units.py`'s "a 36% inflation and a 1.91× over-count" (attributed to two unnamed prior analyses) and `diffunit_gap_funnel.py`'s 27× undercount. Quote those, not 1.99×. |
| `scripts/harvest/multi_content_disambiguate.py` | **BROKEN** | genuine `TypeError` raised inside argparse's own `--help` formatter — the tool cannot print usage, let alone run as documented. |
| `scripts/harvest/oracle_funnel_scan.py` | **WORKING but UNSAFE ARG HANDLING** | has no banned-substring in its name and is not read-only: it consumed a probe's `--help` as an **unvalidated positional output path** and wrote a 517 KB file named `--help` into the repo root (detected and removed; repo verified clean). Always pass explicit paths; never probe it. |
| `scripts/harvest/homing_gen.py`, `homing_gen3.py` | **DEAD (ONE-SHOT)** | hardcode `/home/free/tmp/wt-homing{2,3}`, which no longer exist. Superseded by `homing_gen4.py` (proper argparse + full-tree sweep). |
| `scripts/harvest/stub_census.py`, `neartwin_cause_census.py` | **DEAD (ONE-SHOT)** | hardcoded dead-worktree `sys.path` inserts. (`stub_census.py`'s dependency `scripts/native_scope_map.py` *does* exist — only the path is dead.) |
| `scripts/harvest/localstatic_calibrate_7d5c413e.py` | **ONE-SHOT** | calibrated against one commit hash; kept for provenance only. |
| `scripts/harvest/localstatic_census_wide.py` | **SUPERSEDED** | 71% stale-glob contamination, self-admitted. Use `localstatic_census_v2.py`. |
| `scripts/harvest/resolve_splits_union.py` | **WORKING but CANNOT DELETE** | a line-UNION seeded from *ours*; its own docstring line 17 says "No removals are propagated". Under `land.sh`'s rebase, *ours* is main — so **every pin the lane removed silently survives** and `land.sh` still prints `READY:`. Unpinning is a real landed fix shape. ⇒ if your patch's value is a `splits.txt` **deletion**, resolve by hand and re-verify with `overlap_check.py`. (`resolve_json_union.py` *is* 3-way and does respect deletions.) |
| `unified_id*.json` / `ghidriff_identities.json` | **DEAD DATA** | TU0-keyed, VA-dead. ~12 tools still read them (`tools/{fingerprint_pipeline,icf_alias_check,oracle_quality,topo_locate,fingerprint_match,gen_band3_port_worklist,game_splits,unknown_triage,span_confirm,identity_transfer}.py`, `scripts/map_verify.py`, the `wf_idt_*.js` workflows). Any of their conclusions that *depend on those files* are unsound post-TU5-flip. |

### Fixes landed 2026-07-29 — all CONFIRMED PRESENT by this audit

| tool | fix | evidence |
|---|---|---|
| `tools/scope_map.py` | renamed catch-all functions now recover their **true VA** via `target_symbol_map.json` → `symbols.txt` → synthetic fallback, instead of always getting a synthetic address (which dropped them from tier denominators). Residual measured **1 of 5,377** catch-all named fns (an ICF-merged `__MERGED_fn_` with no map entry). | commit `19acdec4`; `load_target_symbol_name2addr()` + the CATCH-ALL comment block at l.478-500 |
| `tools/scope_map.py` | prints a **staleness banner** when the gitignored `scope_map.json` cache is missing/keyed to another build, instead of silently inflating every tier %. | commit `0f576856` |
| `scripts/grind/classify_funclets.py` | `from live_units import filter_live, repo_root_from_build_subdir` (l.65) — LIVE filter applied before tagging `decomp.db`. | confirmed |
| `tools/icf_alias_finder.py` | `from live_units import filter_live` (l.53) + docstring at l.118. | confirmed |
| `scripts/harvest/splits_move.py` | **symbol-boundary gate**: `sym_boundary_violation()` (l.194-207) refuses mid-symbol spans — *"dtk's validate_splits rejects any split range boundary strictly inside a carved function ('ends within symbol'). Accepted: va exactly at a symbol start, exactly at a symbol end, or in an inter-function alignment gap."* | confirmed |
| `scripts/harvest/overlap_check.py` | **back-scan fixed** (l.122-141): uses a `prefix_max_hi` array with `while j >= 0 and prefix_max_hi[j] > cur.lo`, comment: *"stopping at the first non-reaching NEIGHBOR (the pre-2026-07-29 behavior) … silently missed a WIDE early range swallowing a later small one."* | confirmed |

---

## 5. Build-step wiring (verified against `configure.py` today, l.502-586)

**pre-compile** (1 step, on the dtk-split TARGET obj):
1. `scripts/obj_target_symbol_renamer.py --batch --apply` — rewrites anonymous
   `fn_<addr>` symbols to MSVC mangled names from `scripts/target_symbol_map.json`
   so objdiff can pair target↔base **by name**. Without a map entry, a pinned game
   TU reads a false 0%.

**post-compile** (5 steps, on OUR compiled objs, **serialized** via stamp chaining —
each stamp is an implicit input of the next, because all five read-modify-write the
same obj set and concurrent runs lose each other's writes):
2. `obj_anon_ns_patcher.py` — anonymous-namespace hashes (MSVC derives them from
   machine name + source path)
3. `obj_dynamic_init_patcher.py` — `??__E` dynamic initializers STATIC→EXTERNAL
4. `obj_guard_patcher.py` — `$S` → `??_B` static-init guards
5. `obj_bool_mangle_patcher.py` — bool back-reference mangling
6. `obj_atexit_scope_patcher.py` — `??__F` atexit scope counters (fuzzy)

**NOT wired** (enable per-function by hand): `obj_regswap_patcher.py`,
`obj_transplant_patcher.py`. This matches CLAUDE.md's claim exactly.

---

## 6. Orchestrator MCP + analysis engine (verified)

`scripts/orchestrator/mcp_server.py` exposes **exactly the 11 tools CLAUDE.md
claims**, no more, no fewer: `report_result`, `query_functions`, `get_attempts`,
`lookup_rb3wii`, `lookup_dc3`, `run_objdiff`, `run_analyze_function`,
`run_diff_inspect`, `lookup_struct_offset`, `lookup_merged_symbol`,
`mark_patch_result`.

`scripts/analysis/diff_inspect.py` modes: `diagnose`, `clusters`, `regswaps`,
`offsets`, `replaces`, `compare`, `save_baseline`, `mismatches`, `stack-layout`,
`asm_listing`.

★ **Always pass `project_dir=<your worktree>`** to `run_objdiff` /
`run_analyze_function` / `run_diff_inspect`, or you measure main instead of your
edits and your changes look like no-ops.

---

## 7. The full inventory

Status legend: **WORKING** = `--help` returned usage under 30 s ·
**WORKING(no argparse)** = parses and runs, but takes bare positional args — read
the docstring · **SHELL** / **JS-WORKFLOW** = not Python-invoked ·
**BROKEN / SUPERSEDED / ONE-SHOT / DEAD** = see §4.
`stale-glob` column: **YES** = enumerates `build/45410914/{asm,obj}` without
`live_units` (§3).

Aggregate result of the sweep: of **127 argparse-confirmed tools outside
`scripts/harvest/`, 127 returned usage and 0 failed**; 58 more parse but take
positional args. In `scripts/harvest/` (138 files): **122 WORKING**, 1 BROKEN
(`multi_content_disambiguate.py`), 1 BROKEN-OUTPUT (`handler_list_diff.py`),
1 UNUSABLE-PER-SYMBOL (`reloc_correspondence.py`), 2 SUPERSEDED, 4 dead ONE-SHOT,
8 write-path tools deliberately not executed, 3 library-only modules.

### 7a. `scripts/harvest/` — identification, attribution, census (138 files)

Grouped a-l then m-z (the order the audit sweep ran). Column 6 = stale-glob risk (§3).


| tool | status | purpose | invocation | known limits/defects | stale-glob-suspect |
|---|---|---|---|---|---|
| ab_supervise.sh | WORKING | Retry-loop supervisor: re-runs `fresh_report.sh` in a worktree up to 6x, watches for a completion marker in the log | `bash -n` only (policy) | Long-running; monitor by log marker not PID (pgrep self-matches) | N/A |
| access_specifier_scan.py | WORKING | Finds map rows wrong only in the mangled-name access/virtual code letter (e.g. A/B/E/F/I/J) — cosmetic, body is byte-identical | `--help` OK | none noted | N/A (no glob) |
| argreg_mispair_scan.py | WORKING | Flags symbol-map mispairs via ABI arg-register evidence: target reads a reg the mapped symbol's signature doesn't declare | `--help` OK, has `--fp-control` self-check mode | none noted | N/A |
| autocarve_funnel.py | WORKING (see notes) | Honest funnel measurement of the `auto_03_*` unowned pool: vendor-window exclusion, funclet-crumb size filter, MIDDLE-hole geometry | `--help` OK; full body read | Derives gap/span geometry from `build/45410914/report.json`'s coalesced `auto_03_*` unit boundaries, not from `splits.txt` pins — `diffunit_gap_funnel.py`'s docstring calls this exact mistake out ("DERIVE FROM splits.txt, NEVER FROM report.json") and documents a **27x undercount** of the real gap set from it. Could NOT find a literal "1.99x inflation" figure anywhere in this file, its module docstring, or `docs/plans/lane-al-autocarve-2026-07-26.md`; the closest real figure in the tree is `live_units.py`'s docstring: "a 36% inflation and a **1.91x over-count**" — attributed to two unnamed prior analyses, not explicitly to this file. Task's "1.99x" is likely a mis-recollection of that 1.91x. | N-A (reads report.json, not a disk glob of obj/asm) |
| byte_locate.py | WORKING | **Compile-and-byte-search TU LOCATOR** — the port->compile->locate->pin inversion. Searches retail `.text` (`orig/45410914/band.exe`) for reloc-masked byte-identical copies of a freshly compiled, UNPINNED obj's bodies and clusters the hits into a proposed `.text` span. Anchors only on bodies that are COMDAT `Selection==1` AND uniquely defined tree-wide AND not `__unwind$` funclets. Modes `locate` / `calibrate` / `audit` / `control` / `phantom` (3-channel absence pre-flight; derives class names from the compiled obj's mangled symbols and REFUSES a bare file-name probe, which is a silent false negative) / `design` (compares our header vs the Wii header's member set + `#pragma pack` to decline a TU whose 360 and Wii classes are different designs) / **`relocaudit`** (detects the AT-100% defect class: decodes retail's actual branch target at each masked relocation site and compares it with the symbol our relocation names — catches a body that scores 100% while calling the WRONG function, which no sub-100 scanner can reach; HARD/NAME tiers, ICF folds filtered by comparing candidate bodies, counts deduplicated per symbol and labelled UPPER BOUNDS not defect counts). `locate` also emits a **DIVERGENCE WORKLIST**: for each TU-owned body that MISSED, the closest retail counterpart in the span, the size delta and how many words agree from each end — a miss off a dev-build oracle is a source-dialect divergence, not absence. Flags the known dialect classes (`Load` BinStreamRev 0/58, `Handle` END_HANDLERS tail, `Save` SAVE_OBJ-vs-SAVE_REVS, `SyncProperty` local statics) and the 316-byte `OBJ_SET_TYPE` signature | `--help` OK; calibrate+control self-measure | **Calibrated**: precision(unique VA) 0.9841 (434/441), recall 0.5813, TU-level 47/50, chance baseline 0.001124 (875x lift); negative controls 0/38,960 (rotation) and 0/1,115 (one-bit mutation, owned population). Read-only — proposes spans, never writes splits/objects/map | NO — sweeps `build/45410914/src` (OUR compiled objs, always fresh), never the stale `obj/`+`asm/` dirs. ★ `.text` VA→file offset is `PointerToRawData + (va - VirtualAddress)`, NOT `va - 0x82000000` (a 0xB200 delta on `.text`); enforced by a hard startup assert `off(0x824DAAD0)==0x004CF8D0`. ★ Entry points are `.pdata` UNION `bl`-targets — `.pdata` absence is NOT a 'not a function' test (4,963 call targets have no `.pdata` entry); the pinned-set join is deliberately {full stem} UNION {basename}, which can only over-call a unit pinned, never under |
| callee_set_join.py | WORKING | Forward call-graph content join: names an unnamed retail VA from a homed caller's relocation | `--help` OK | Depends on funclet_cascade_rank's pinned-parent pool | N/A |
| caller_side_invert.py | WORKING | Inverts call graph to home "contentless" functions (no string/const evidence) from caller relocations; 99.09% held-out precision claimed | `--help` OK | Refuses BIG-FAMILY (17+) siblings — see family_closure.py | N/A |
| carve_pilot_classify.py | WORKING | Classifies auto-carve candidates into tractability buckets (NO SOURCE / XDK thunk / GAME wired / ANON / DC3-only / etc.) | No argparse; ran to completion printing a real n=563 bucket table | none noted | N/A |
| check_regression_lock.py | WORKING | RFC-16 gate: hard-fails a landing if any fn that was >=99.999% in decomp.db's last snapshot regressed below it in the candidate's report.json | `--help` OK | none noted | N/A |
| class_layout_report.py | WORKING | Authoritative class layout via `cl.exe /d1reportSingleClassLayout`, replacing stale `// 0xHEX` header comments | `--help` OK | none noted | N/A |
| coff_func_bodies.py | WORKING | Per-function COFF reloc-name extraction; detects MISPAIRED STL template instantiations (target references a different `T` than ours) | No argparse, needs `<target-obj> <base-obj> [substr]`; `--help` -> IndexError (expected, not a bug) | none noted | N/A |
| comdat_scatter_scan.py | WORKING | Finds NAMED functions stuck at 0% whose bytes are actually emitted by a *different* compiled object than the span-owning unit | `--help` OK | `--obj-root` defaults to `build/45410914/src` (OUR objs) | NO — targets `src/`, not target `obj/`/`asm/` |
| content_join_propose.py | WORKING | Proposes target_symbol_map entries joining target obj <-> base obj on reloc-masked bytes AND referenced-string identity (collapses ICF-identical boilerplate twins) | No argparse, needs `<target.obj> <base.obj>`; `--help` -> RC=1 (prints docstring, no real usage line) | Landed +30 historically (commit f7d609a2) | N/A (single-file args) |
| definer_index.py | UNRUNNABLE-AS-INVOKED | Builds mangled-name -> [defining objs] index; the "n_definers" gate (a name no obj defines can never be made to pay by map/splits edits alone) | No argparse; ran with bogus input, printed "0 objs scanned" | Needs a real objs-root arg to be meaningful; not verified functional beyond the header logic read | Unclear — couldn't confirm root default without a real run |
| diffunit_gap_apply.py | WORKING (UNRUN-BY-POLICY for real exec) | Applies a subset of DIFFERENT-UNIT gaps to splits.txt (extend LEFT or pull back RIGHT), atomic rewrite + full audit (overlaps/inversions/dup/sectionless) | `--help` OK | Matches "apply" naming policy — only `--help` run, never executed | N/A |
| diffunit_gap_funnel.py | WORKING | Enumerates DIFFERENT-UNIT unclaimed `.text` gaps from splits.txt pins (the corrected sibling to autocarve_funnel.py's report.json approach) | `--help` OK | Explicitly documents autocarve_funnel's report.json mistake (27x undercount) as the reason to derive from splits.txt instead; different-unit gaps are 87.2% genuine COMDAT scatter per prior lane measurement | N/A |
| diffunit_margin.py | WORKING | The MARGIN RULE: decides which neighbour (LEFT/RIGHT) should claim a different-unit gap; corrects a false "uniqueness-gated" assumption about objdiff's funclet-pairing passes | `--help` OK | Documents pass 2/2b/3 of `pair_funclets_by_bytes` are NOT uniqueness-gated, contra two docs' claims | N/A |
| diffunit_subrange.py | WORKING | Sub-range (prefix/suffix) margin selection for different-unit gaps — beats whole-gap argmax's 79.8% evidence-share ceiling | `--help` OK | none noted | N/A |
| dupname_identity_resolver.py | WORKING | Resolves the TRUE identity at a duplicate-name VA via adjustor-thunk/deleting-dtor callee relocations (vs. dupname_rebijection's "any fitting name") | `--help` OK, has `--rounds` | none noted | N/A |
| dupname_rebijection.py | WORKING (UNRUN-BY-POLICY for `--apply`) | Makes target_symbol_map injective on NAME by moving surplus duplicate-name VAs onto other byte-twin names | `--help` OK; has `--apply`/`--emit` but only `--help` run | Score-correct but identity-arbitrary per its own docstring | N/A |
| eh_ground_truth.py | WORKING | Retail's per-function EXCEPTION-HANDLER truth, cheaply, from `.pdata`/RTTI | `--help` OK | Has own `--allow-stale` guard vs mtime/config.json staleness | NO — self-guarded |
| eh_rtti_probe.py | WORKING (negative result, historical) | Tested retail EH/RTTI chain as an identification discriminator | `--help` OK | **Verdict recorded in its own docstring: it CANNOT discriminate** — exists only so nobody rebuilds this channel | N/A |
| eh_signature_match.py | WORKING | Identifies unnamed retail functions via the C++ EH RTTI/TypeDescriptor class-name-string chain (the one channel eh_rtti_probe left unexplored) | `--help` OK | none noted | N/A |
| family_closure.py | WORKING | Bipartite elimination over reloc-masked sibling families — decides a whole STL/template family at once (handles caller_side_invert's BIG-FAMILY refusals) | `--help` OK | none noted | N/A |
| family_stride_proof.py | WORKING | Self-contained MAP MISPAIR proof: an STL family member striding by a different `sizeof(T)` than its 100%-matching siblings can't belong to that family | No argparse, needs `<worktree>`; `--help` -> FileNotFoundError (expected) | No external oracle needed | N/A |
| fill_attribution_verify.py | WORKING | Adversarial cross-check of `pdata_parent_owner` attribution (index/adjudicate/survival subcommands) | `--help` OK | Filters via objdiff.json target_path list | NO — filtered |
| fp2_final.py | WORKING (pipeline-dependent) | Final ranked fingerprint-candidate table: filters noise, grades, checks dc3/wii source existence + LOC | No argparse; ran to completion (real table printed) | Depends on `/home/free/tmp/fp2_runs.json` existing from fp2_runs.py; hardcoded round-1 exclude list | N/A |
| fp2_runs.py | WORKING | Enumerates maximal unpinned `.text` runs from fingerprints.json, annotates with strings/autoid/channel | No argparse; ran to completion (769 runs, 31 candidates) | Hardcoded ROOT path (matches this repo) | N/A |
| fp2_span.py | WORKING | Per-function string walk across a `.text` span, to find a good string-family cut boundary | No argparse, needs `<lo-hex> <hi-hex>`; `--help` -> ValueError (expected) | none noted | N/A |
| frame_delta_scan.py | WORKING | Finds functions whose compiled stack frame differs from the retail target frame (feeds EH-funclet-cascade fix ranking) | `--help` OK | none | **YES-SUSPECT** — `glob.glob(build/45410914/obj/**/*.obj)` (line ~100-104) with NO live-unit filter; report.json's `pct` dict is loaded but only used for post-hoc scoring, not to gate which objs get scanned |
| funclet_cascade_rank.py | WORKING | Ranks parent functions by EH-funclet cascade yield (how many funclets flip if the parent's frame is fixed) | `--help` OK | Explicitly does NOT screen dtk asm files — uses band.exe `.pdata` directly | N/A |
| funclet_size_census.py | WORKING | Census of funclet-sized code symbols vs. the >84B "plausibly real" threshold across our compiled objs | No argparse; ran to completion (objs 1037, real counts) | none noted | N/A (scans our compiled objs, ~1037 matches TU count, not target obj/asm) |
| gap_content_evidence.py | WORKING | Per-gap content evidence: reads actual referenced strings/callees at a span/gap/unit to support LEFT/RIGHT attribution | `--help` OK | Has its own documented "STALE-ASM TRAP" mtime-vs-config.json filter | NO — self-guarded |
| gen_nearmiss_pool.py | WORKING (UNRUN-BY-POLICY for real exec) | Regenerates the near-miss harvest pool from report.json ([96,99.999) pct, size>44, minus proven walls) | `--help` OK; `gen_` naming — only `--help` run | Writes an output pool JSON when actually run | N/A |
| handle_dialect_scan.py | WORKING | Scans TUs for RB3_HANDLE_LOCAL_STATIC macro-dialect eligibility (Object.h dialect vs ObjMacros.h dialect) | `--help` OK | Reads only objects.json | N/A |
| handler_drift_scan.py | WORKING | Finds frame-drift functions caused by DC3-vs-retail BEGIN_HANDLERS surplus/missing HANDLE() entries, ranked by funclet-cascade yield | `--help` OK | Feeds handler_list_diff.py's `--json` input | N/A |
| handler_list_diff.py | **WORKING BUT DEFECTIVE** (deep-dive #1) | Diffs OUR BEGIN_HANDLERS chain against retail's handler-name strings for each frame-drift candidate | No argparse, needs handler_drift_scan's `--json` output file; `--help` -> FileNotFoundError (expected) | **CONFIRMED FALSE-SURPLUS BUG** — see report body | N/A |
| header_inline_policy_scan.py | WORKING | Fan-out census: how many objs define each symbol, and whether that definition carries EH data (reasons about callee-side score-bound divergences) | `--help` OK | `tgtdir` (=`build/45410914/obj`) used only for single-file per-symbol lookups keyed off report.json names, never blindly enumerated | NO |
| homing_apply4.py | WORKING (UNRUN-BY-POLICY for real exec) | Splices homing_gen4 blocks into splits.txt (sorted per-unit `.text` lines) + emits a wave map fragment; refuses on overlap | `--help` OK; `apply` naming — only `--help` run | none | N/A |
| homing_gen3.py | ONE-SHOT / hardcoded-worktree | Round-3 dedup generator: splits pins + map fragment from plain-UNIQUE homing hits, hardcoded to 5 brand-new TUs | No argparse; `SPLITS` hardcoded to `/home/free/tmp/wt-homing3/...` which no longer exists -> FileNotFoundError | Cannot be re-run without editing the hardcoded WT constant; superseded in spirit by homing_gen4.py's full-tree sweep | N/A |
| homing_gen4.py | WORKING | Round-4 FULL-TREE homing generator: sweeps every built obj, handles gap-fill into existing splits blocks + spatial-cluster voting (fixes round-3's "first TU wins" scatter bug) | `--help` OK, proper argparse | Supersedes homing_gen.py / homing_gen3.py's hardcoded-worktree, single-TU-batch approach | N/A |
| homing_gen.py | ONE-SHOT / hardcoded-worktree | Round-2 dedup generator: splits pins + map fragment from plain-UNIQUE hits, VA/name-deduped | No argparse; `SPLITS` hardcoded to `/home/free/tmp/wt-homing2/...` which no longer exists -> FileNotFoundError | Same hardcoded-path issue as homing_gen3.py; both effectively retired by homing_gen4.py | N/A |
| homing_reverse.py | WORKING | Reverse homing (VA -> exact COFF symbol name); the "adjudication primitive" — a calibration found 3/4 prior VA:name handoffs needed name correction | No argparse, needs `<worktree> <va>...`; `--help` ran harmlessly ("scanning 0 objs...") | none noted | Unclear from header alone — not fully traced, but ran with 0 objs on bad input so behavior on real input unconfirmed here |
| homing_scan_all.sh | WORKING | Shards homing_scan.py's obj list N-way parallel (16-way, ~2min/914 objs), merges to `merged.json` | `bash -n` only (policy) | `find build/45410914/src -name '*.obj'` — our objs, not target | N/A |
| homing_scan.py | WORKING | Core reloc-masked byte-identity homing scan against band.exe's `.pdata` inventory | No argparse, `key=value` style args; `--help` -> ValueError (expected) | Sharded out by homing_scan_all.sh | N/A |
| icf_class_bijection.py | WORKING | Harvests the AMBIGUOUS byte-identity equivalence class that size_order_automap.py discards (1,720 of 1,919 non-unique hits) — any bijection scores 100% since objdiff pairs by name+bytes | `--help` OK | none noted | N/A |
| identical_pct_cluster_scan.py | WORKING | Clusters functions sharing an identical match%/score/delta (identical % encodes identical penalty+instr-count per project finding) — structural clustering or MAP-MISPAIR detection depending on `--axis` | `--help` OK | none noted | NO — `build_src.rglob` targets our compiled `src/`, not target `obj/`/`asm/` |
| invcorr_mispair_repoint.py | WORKING | Repairs target_symbol_map on unique byte-identical repoints (inverse-mispair correlator) | `--help` OK; has `--apply` but only `--help` run | none noted | **YES-SUSPECT** — `build_target_index()` does `TARGET_OBJ_DIR.rglob("*.obj")` over `build/45410914/obj` unfiltered; the `objdiff.json`-derived `OBJDIFF` constant IS loaded but only consulted in a different, later function (line 697), not in the index-build path |
| joint_unblock.py | WORKING | The map<->splits cross-feed: expresses joint repairs neither single-owner lane (map_displace_round.py / span_predictor.py) can act on alone | `--help` OK, `plan` subcommand | none noted | N/A |
| land.sh | WORKING (not executed, policy) | Rebases a wave worktree branch onto main with auto-union-resolve for target_symbol_map.json/objects.json/splits.txt, prints READY/DEFER | `bash -n` only (policy) | Mutates a lane's own branch/worktree when run for real, never main directly | N/A |
| leadb_signature_scan.py | WORKING | BinStreamRev "LEAD B" per-TU rev-static signature scanner | `--help` OK | `--asm-dir` CLI arg exists but is **dead code** — never referenced in the body; actual driver is report.json population + per-symbol `scan_one` lookups | NO (the asm-dir default is unused, not a live blind-glob) |
| live_units.py | WORKING (library) | The shared LIVE-unit filter itself: `live_unit_names`/`filter_live`/`live_target_paths` against objdiff.json | Module import only, no CLI; RC=0 empty output | Documents the exact defect this whole audit checks for; cites "a 36% inflation and a 1.91x over-count" from two prior (unnamed) analyses before it existed | N/A (this IS the fix) |
| localstatic_calibrate_7d5c413e.py | WORKING (ONE-SHOT) | Calibrates localstatic_patch_gen's detector against ground truth from one specific historical commit (7d5c413e) | No argparse; ran to completion (several "OK" lines) | Hardcoded GT dict tied to one commit; must run from repo root (imports from `scripts/harvest`) | N/A |
| localstatic_census_v2.py | WORKING | Corrected TU-level local-static census — "enumerates from objdiff.json, not from the filesystem" (explicit fix for census_wide's defect) | `--help` OK | Supersedes localstatic_census_wide.py | NO — is the fix |
| localstatic_census_wide.py | **SUPERSEDED** (by localstatic_census_v2.py) | Wide local-static census (predecessor) | `--help` OK | Self-documented: 9,911/13,932 rows (71%) were stale orphans before being replaced | **YES-SUSPECT** (self-admitted in its own docstring) |
| localstatic_patch_gen.py | WORKING (UNRUN-BY-POLICY for real patch write) | Generates concrete source-patch proposals for the local-static Symbol/Message conversion lever | `--help` OK; `patch`+`gen` naming, only `--help` run | none noted | **YES-SUSPECT** — `glob.glob(build/45410914/obj/**/*.obj)` (line ~690-691) unfiltered; report.json's `pct` loaded only for annotation |
| localstatic_population_scan.py | WORKING | Population-level scan for the local-static lever: per-TU straggler counts, target vs base | `--help` OK | none noted | **YES-SUSPECT** — same unfiltered `build/45410914/obj` glob pattern (line ~155-156) |
| localstatic_precision_audit.py | WORKING | Precision-audits a prior localstatic scan's rows against ground truth, re-scanning only the specific `(unit,sym)` pairs already recorded | No argparse, needs `<worktree> <old-rows.json>`; `--help` -> IndexError (expected) | Driven by an existing rows list, not a fresh directory glob (though staleness could be inherited from whatever produced that rows list) | NO — no glob of its own |
| localstatic_reloc_scan.py | WORKING | Finds TUs whose retail Handle/OnMsg bodies construct handler Symbols as function-local statics while ours doesn't (wants `/DRB3_HANDLE_LOCAL_STATIC`) | No argparse, needs `<proj-dir>`; `--help` -> FileNotFoundError (expected) | none noted | N/A |
| localstatic_symbol_audit.py | WORKING | Audits/derives VA->string->owner-unit mapping for local-static Symbol literals vs splits.txt ownership + target_symbol_map | `--help` OK | Generic `load_units(splits_path, obj_root)` helper could in principle be called against a target dir, but its actual call site passes `build/45410914/src` (our objs) | NO — as invoked, targets `src/` |
| localstatic_symbol_inbody_scan.py | WORKING | IN-BODY local-static-Symbol vein: LARGE, already-mapped, near-miss functions whose retail body builds a function-local `static Symbol("lit")` we replaced with a global | `--help` OK | Lane R measured 3/3 flips + 1 partial | **YES-SUSPECT** — `glob.glob(ASM)` (top-level, `build/45410914/asm/*.s`), no live filter |
| localstatic_symbol_scan.py | WORKING | REUSABLE scanner for the local-static-Symbol-accessor vein: small anonymous `fn_` accessors with the canonical guard-bit shape retail keeps out-of-line | `--help` OK | none noted | **YES-SUSPECT** — `ASM.rglob("*.s")` unconditional, no live filter |
| localstatic_tu_census.py | WORKING | TU-level census for the local-static conversion lever; corrected 2026-07-29 note that the real safe-to-convert predicate is "parent reaches 100%", not "TU fully converted" | `--help` OK | Documents measured net-negative mid-flight conversions (-230, -16) | **YES-SUSPECT** — same unfiltered ASM glob pattern as symbol_scan/symbol_inbody_scan |
| ls_guard_timeline.py | WORKING | Guard-bit init timeline scanner for a specific local-static symbol across the ASM tree | No argparse, needs a real symbol key; `--help` -> harmless "not found in asm" | none noted | **YES-SUSPECT** (confirmed) — `glob.glob(ASM/**/*.s)` recursive, no live filter, used both for a strings() cache and per-lookup search |

## Counts by status
- WORKING: 55
- WORKING BUT DEFECTIVE: 1 (`handler_list_diff.py`)
- SUPERSEDED: 1 (`localstatic_census_wide.py`, by `localstatic_census_v2.py`)
- ONE-SHOT / hardcoded-worktree (functionally retired): 2 (`homing_gen.py`, `homing_gen3.py`, both superseded in spirit by `homing_gen4.py`)
- UNRUNNABLE-AS-INVOKED: 1 (`definer_index.py`)
- (ONE-SHOT but still runnable/reusable, noted inline, not double-counted above): `fp2_final.py`, `fp2_runs.py`, `localstatic_calibrate_7d5c413e.py`

## Stale-glob-suspect offenders (9 of 63)
`localstatic_census_wide.py` (self-admitted, SUPERSEDED), `localstatic_symbol_scan.py`,
`localstatic_symbol_inbody_scan.py`, `localstatic_tu_census.py`, `frame_delta_scan.py`,
`invcorr_mispair_repoint.py`, `localstatic_patch_gen.py`, `localstatic_population_scan.py`,
`ls_guard_timeline.py`. All 9 either blindly `glob`/`rglob` `build/45410914/obj` or
`build/45410914/asm` with zero cross-reference against `objdiff.json`'s live-unit set, or
(frame_delta/patch_gen/population_scan) load `report.json` alongside the glob but only use
it for post-hoc annotation/scoring, never to gate which objs are actually scanned.

touching program logic). **Caveat discovered this sweep:** several no-argparse
scripts treat a bare positional arg literally — including the literal string
`--help` itself — as a real path (output file, project dir, pairs.json, COFF obj).
This produced two categories of false signal, both corrected in this table:
(1) `oracle_funnel_scan.py` (no banned substring in its name, so it ran past
`-h`) completed a full whole-binary scan and **wrote a 517,629-byte JSON file
literally named `--help` into the repo root** — an accidental side effect of my
own probing, not a pre-existing repo file. Confirmed via `git status --porcelain
-- ./--help` (untracked) and removed with `rm -f -- ./--help`; repo is clean.
(2) That same stray `--help` file, while it existed, was then picked up by
several *other* tools' `sys.argv[1]` (tu5_icf_disambiguate.py, tu5_nearpair_scan.py,
tu5_target_twin_disambiguate.py, tu5_reloc_masked_correlate.py, vtable_1anchor.py,
vtable_align_diag.py, vtable_align_diag2.py, vtable_global.py, neartwin_cause_census.py,
stl_mispair_twin_scan.py), producing tracebacks that look like real bugs but are
purely artifacts of my probe string being consumed as a positional path. All are
reclassified WORKING below with the real required positional args noted.

| tool | status | purpose | invocation | known limits/defects | stale-glob-suspect |
|---|---|---|---|---|---|
| map_displace_round.py | WORKING | Displaces ICF-tied/free claimants into vacated symbol-map VAs from a homing `merged.json`, with a strict-guard refusing to displace current strict-100 holders | `--help` OK (argparse; `round` in name → UNRUN-BY-POLICY for a real `--out` run) | Requires `--worktree`, `--results`, `--out` | N-A |
| map_edit_textual.py | WORKING | Textual RENAME/DELETE editor for `target_symbol_map.json` (line-splice only, never `json.dump`) | `--help` OK (`--dry-run` supported; real edit is a WRITE) | Guards: key must exist, no duplicate mangled names, dup-VA count preserved | N-A |
| map_line_splice.py | WRITES (target_symbol_map.json) — UNRUN-BY-POLICY | Splices one `{VA: name}` line into `target_symbol_map.json`, validating JSON re-parse + no duplicate VA | `main(sys.argv[1], sys.argv[2])`, no argparse — probe crashed `IndexError` before touching the file (harmless) | Name contains "splice" — never run with real args this sweep | N-A |
| map_misassign_repair.py | WORKING | Detects (and, with `--apply`, removes) map entries naming a symbol their own unit does not define | `--help` OK ("repair" in name → UNRUN-BY-POLICY for `--apply`) | positional `worktree` required | **YES** — `glob.glob('build/45410914/src/**/*.obj')` + `glob.glob(root+'/**/*.obj')`, no `live_units` import |
| map_repoint_round.py | WORKING | Computes a repoint plan from homing `merged.json` for `map_rotation_repair.py apply` | `--help` OK ("round" in name → UNRUN-BY-POLICY for a real `--out` run) | Requires `--worktree`, `--results`, `--out` | N-A |
| map_rotation_repair.py | WORKING | Cycle-aware **permutation** repair of mispaired map entries (A holds B's VA, B holds C's, C holds A's) — analyze/plan/apply, textual-splice apply only | `--help` OK (subcommands; "repair"/"round"-adjacent → UNRUN-BY-POLICY for `apply`) | 423 measured mispairs cited in docstring (`StaticClassName`/`Type()` boilerplate family) | N-A |
| measure_delta.py | WORKING | Measures exact strict net (gains − regressions) between two `report.json` snapshots, plus a fuzzy-regression scan (catches 99→80 drops that never leave the strict set) | `--help` OK; real use: `baseline new` | Exit 0 always — verdict is in the printed NET line, not exit code | N-A |
| micropin_apply.py | WORKING | Applies micro `splits.txt` pins with a `--dry` preview mode and `--only-units` scoping | `--help` OK ("apply" in name → UNRUN-BY-POLICY for a real run) | Requires `--pins` | N-A |
| missing_virtual_scan.py | WORKING | Scans near-miss callers for the "missing-virtual" assembly signature (our direct `bl` vs retail's vtable `lwz/lwz/mtctr/bctrl`) — read-only, ranks candidate methods by distinct-caller count | `--help` OK | Documents the known landed example (`GameMode::InMode/SetMode` → +9 strict) | N-A |
| multi_content_disambiguate.py | **BROKEN (self-bug in --help)** | Content-based disambiguator for `homing_scan`'s MULTI/UNIQUE-ICF residue via positional string/float/symbol/opcode evidence, strict accept-at-≥1-agree/0-conflict rule | `--help` **crashes**: `TypeError: %u format: a real number is required, not dict` inside argparse's own help formatter (a default value passed to a `%u`-style help string is a dict) | Real (non-help) invocations untested this sweep; bug is in the CLI help path itself, not necessarily the core logic | N-A |
| nearfree_tier_worklist.py | WORKING | Ranks near-miss clusters from `decomp.db`/report by penalty tier + cluster size into a triage worklist | `--help` OK | `--max-cluster` default 4 (≥5 documented to yield 0, per lane-ag doc) | N-A |
| nearidentity_bijection.py | WORKING | Bijection-based near-identity recovery from funnel/funclet evidence, emits an apply-ready fragment | `--help` OK | N-A | N-A |
| neartwin_cause_census.py | **ONE-SHOT** | Census of near-twin divergence causes for near-miss functions | No argparse; positional `WT=sys.argv[1]`; hardcodes `sys.path.insert(0,'/home/free/tmp/laneAM')` to import `coffx` | Hardcoded lane-specific import path (lane AM); my `--help` probe resolved `WT='--help'` → `FileNotFoundError` for `--help/scripts/target_symbol_map.json` (probe artifact, not a real bug given a real WT) | **YES** — 2 glob hits, no live_units |
| newobject_sizeof_scan.py | WORKING | Scans `sizeof`-mismatch candidates at `new <Class>` allocation call sites | `--help` OK | N-A | N-A |
| newobj_inline_classify.py | WORKING (confirmed via real re-run) | Classifies `new <Class>` factories as INLINED-only vs OUTLINED-only vs BOTH via adjustor/thunk byte shapes | No `--help`; positional `<worktree>` — re-ran with real repo path: printed real counts (INLINED-only 119 / OUTLINED-only 158 / BOTH 0), exit 0 | Initial `--help` probe falsely looked BROKEN (`ModuleNotFoundError: coffx`) because `--help` was consumed as the worktree arg, sending the `coffx` import search to the wrong dir | **YES** — `glob.glob(wt+'/build/45410914/obj/**/*.obj')`, no live_units |
| objptr_replace_family_scan.py | WORKING | Scans ObjPtr `Replace`-family call-site mismatches for a given repo | `--help` OK | N-A | **YES** — `os.walk(obj_root)`, no live_units |
| offset_drift_sweep.py | WORKING | Sweeps struct-offset drift across a symbol pool, writing results incrementally with `--resume` | `--help` OK; positional `pool` required | N-A | N-A |
| oracle_ceiling_scan.py | UNRUNNABLE standalone | Computes a ceiling/ranking over `oracle_funnel_scan.py`'s funnel JSON output | No argparse; positional funnel-JSON path, defaults to `/home/free/tmp/oracle_funnel.json` — that file doesn't exist, so bare/`--help` invocation raises `FileNotFoundError` | Needs the upstream funnel JSON to exist first | N-A |
| oracle_coverage_matrix.py | WORKING | **(special attention)** Three-way oracle-coverage matrix RB3-360 × rb3-Wii × DC3, joined on canonical TU identity via the shipped **binaries' link maps** (not decomp source trees, which would conflate "Wii lacks it" with "Wii decomp hasn't got there"); cells (a) DC3-only (b) Wii-only (c) BOTH (d) NEITHER (u) UNATTRIBUTED. Self-documents that cell (d) is near-empty **by construction** (oracle-driven attribution can't discover 360-exclusive code) — the actionable outputs are `--reverse` (Wii→360 census, not selection-biased) and `--control` (measured 1.0000/1.0000 recall/specificity, n=673/22) | `--help` OK; flags `--reverse --control --wii-census --outdir` | Read-only, touches no tracked file, runs no build (per its own docstring) | N-A |
| oracle_funnel_scan.py | WORKING, but **hazardous** | Whole-binary funnel of the unmapped-candidate pool (raw→vendor-excluded→already-100→in-scope→no-base-obj→no-candidate-name→units), then ranks top units by candidate density/bound | No argparse at all — the single positional arg is an **output path with no validation**; my `--help` probe was consumed as that path, ran the full census, and **wrote the 517,629-byte result to `./--help`** (see header note; cleaned up) | Bare invocation is NOT safe/read-only despite no banned substring in the name — a naming-policy gap: this tool needed to be caught by "any positional-looking arg becomes a write path," not just the apply/patch/… substring list | N-A |
| order_anchored_bijection.py | WORKING | Recovers `fn_<VA>` identities via **name-anchored** monotone interval bijection (extends `size_order_automap.py`'s byte-identity anchors with already-named-function anchors, immune to body divergence); emits fragment only, never writes the map | `--help` OK; `--funnel --holdout --emit --tier --unit --dump` | Measured (laneAQ 2026-07-26): T4 tier A/B +37 strict / 0 losses vs control gate +5/0 with heavy junk-band pairings — gate is load-bearing | N-A |
| overcarve_scan.py | WORKING | Detects over-carved COMDAT splits | `--help` OK; positional `worktree rows` | N-A | **YES** |
| overlap_check.py | WORKING | **(special attention)** `splits.txt` section-range overlap gate (Phase 7 honesty gate); `.text` overlap is a hard fail, `.pdata` overlap also gated by default (`--text-only` to restrict) | `--help` OK; also importable (`find_overlaps`, `check_splits`) | Back-scan fix **CONFIRMED PRESENT** — see summary below for the quoted loop | N-A |
| oversub_guard.py | WORKING | Guards against oversubscribed/fake-match census growth via baseline/verify census comparison | `--help` OK | N-A | N-A |
| pdata_parent_owner.py | WORKING | HARD unit attribution for EH funclets via `.pdata` (census/span/audit/actionable/gaps subcommands) | `--help` OK | N-A | N-A |
| pdata_shape_probe.py | WORKING | Probes `.pdata` frame "shape" (savegprlr range etc.) against homing results for a worktree/band | `--help` OK; requires `--results --worktree` | N-A | N-A |
| reloc_correspondence.py | WORKING, but **SLOW (whole-census cost)** | **(special attention)** Classifies whether a 100%-matched function is a genuine REPRODUCTION or merely a byte-identical SHAPE (relocation-masking blind spot) via a data-symbol injectivity oracle + retail-image byte oracle + ICF-capability suppression | `--help` OK; `--symbol` for one function times out at 10 min — root cause below | Its unit list comes from `objdiff.json` via `live_units.py`, **never globs** (line 94/116) | NO (imports live_units — the only harvest tool in this slice that does) |
| reloc_correspondence_selftest.py | WORKING | Falsification/self-test control: replays a saved census with an injected delta and checks the tool still flags it | `--help` OK; requires `--census` | N-A | N-A |
| repin_census.py | WORKING | Census of re-pinnable small gaps/regions bounded by min-bytes/floor, ranked by automap-yield potential when `--build-dir` given | `--help` OK | N-A | **YES** — `asmdir.rglob("*.s")`, no live_units |
| residue_headroom.py | WORKING | **(measured lever)** Bounds the true identification headroom of the MULTI residue pool: records are (name×TU) pairs not names, and retail ICF-folds swarms to fewer addresses than names — measured true ceiling ~1,695 vs the naively-counted 26,223 MULTI records | `--help` OK; requires `--results` | Read-only | N-A |
| resolve_json_union.py | UNRUNNABLE standalone (documented) | 3-way JSON dict merge resolver (base/main/worker) for `target_symbol_map.json`/`objects.json` during wave rebases — fixes a documented 2-way-union resurrection bug | No `--help`; real usage `<worktree> <relpath>`; exits 2 "missing stage" without real git-conflict-stage input | Expected/documented behavior, not a bug | N-A |
| resolve_splits_union.py | UNRUNNABLE standalone (documented) | Analogous 3-way union resolver for `splits.txt` during rebase | Same pattern: exits 2 "missing stage" without real conflict state | Expected/documented behavior | N-A |
| retail_handler_strings.py | WORKING | Lists, in reference order, the handler-name string constants a **retail** function body references (reads dtk asm `.s` + resolves `lbl_82XXXXXX` against the flat retail image) — companion to `handler_drift_scan.py` | No argparse; positional `Unit fn` — probe crashed `IndexError` (no args given, expected) | N-A | N-A |
| run_interleave_scan.py | WORKING | Scans for interleaved run patterns (runs/prove subcommands) bounded by min-len/stride-factor | `--help` OK | `--include-vendor` needed to NOT skip the 0x82800000-0x82D00000 XDK/Quazal band | N-A |
| sandwich_overcarve.py | WORKING | Detects the "sandwich" overcarve pattern (small carved fn wedged inside a larger true owner) | `--help` OK; positional `worktree` | N-A | **YES** — 3 glob hits, no live_units |
| scatter_inline_collapse_scan.py | WORKING | Scanner for the scatter-include inlining-policy collapse (`#include "owner.cpp"` scattered-COMDAT pattern) | `--help` OK | `--all-owners` to include host-owned parents | **YES** — `objdir.rglob("*.obj")`, no live_units |
| scatter_pairing_scan.py | WORKING | Pairing scan for scattered COMDATs, optional restrict to `OBJ_CLASSNAME` COMDATs | `--help` OK | N-A | **YES** — `(BUILD/"src").rglob("*.obj")`, no live_units |
| size_order_automap.py | WORKING | **(foundational library + CLI)** Recovers `fn_<VA>`→mangled pairings for a carved game TU by aligning compiled-obj emission order against dtk target address order, via size+order DP anchored by reloc-masked byte identity; emits fragment only, never writes the map | `--help` OK; `--unit --emit --validate --span --no-gt` | Imported by `thunk_callee_bodydiff.py`/`thunk_callee_freename.py` — its glob risk is inherited by both | **YES** — 3 glob hits (`(BUILD/"obj").rglob`, `(BUILD/"src").rglob`, `(BUILD/"asm").rglob`), no live_units — the most consequential offender since two other tools import it |
| snapshot_landing.py | WORKING | Snapshots per-function match% state of a landed commit into `decomp.db`'s `landing_snapshot` table (RFC-16 Phase A), for the regression-lock check | `--help` OK; requires `--report --commit` | Writes to `decomp.db` (a DB, not a tracked source file) | N-A |
| span_predictor.py | WORKING | Predicts/writes span-shaped proposal records from `homing_scan`-format results for a worktree | `--help` OK; requires `--proposals --worktree` | N-A | **YES** — `glob.glob(root+'/**/*.obj')`, no live_units |
| spill_signature_scan.py | WORKING | Register-spill signature classification scan, resumable over a symbol pool or single `--sym` | `--help` OK | N-A | N-A |
| splits_additive_merge.py | WORKING | ADDITIVE-ONLY merge of one `splits.txt` delta into another: copies only ranges the donor has and target lacks, never edits/removes, refuses on interval overlap | No argparse; `main(target, donor_base, donor)` — probe crashed `TypeError: missing 2 required positional arguments` ("merge" in name → UNRUN-BY-POLICY for a real 3-arg run) | Motivated by a documented `land.sh` 3-way-union corruption bug (unioned `.pdata` back-fills → hard split failure) | N-A |
| splits_move.py | WORKING | **(special attention)** scan/apply/audit for moving `.text` split ownership between units; carries the SYMBOL-BOUNDARY GATE | `--help` OK (subcommands; "move" in name → UNRUN-BY-POLICY for `apply`) | SYMBOL-BOUNDARY GATE **CONFIRMED PRESENT** — quoted below | **YES** (itself) — `glob.glob(root+'/**/*.obj')` at line 259, no live_units import — notable since this is the tool other scanners' overlap fix was validated against |
| stl_mispair_twin_scan.py | WORKING (probe artifact on --help) | Scans for STL-container-instantiation "twin" mispair candidates (structurally identical instantiations misattributed to the wrong template args) | No argparse; positional `project_dir` — my `--help` probe put `--help/objdiff.json` → `NotADirectoryError` (artifact, not a real bug) | Reads `objdiff.json` directly (does not glob obj/asm itself) | N-A (reads objdiff.json, not a raw glob) |
| stride_consolidate.py | WORKING | Consolidates struct-stride evidence (retail array element sizes) per class from asm bodies, cross-checking STL container stride assumptions across units | No argparse; runs full report unconditionally on any invocation, incl. bare | Confirmed **no file writes** (`grep` for `open(...,'w')`/`json.dump`/`.write(` found none) — pure stdout report | N-A |
| stride_truth_table.py | WORKING | Cross-tabulates struct "truth" size vs candidate copy/fill constant evidence into a ground-truth stride table, flagging `<FOLD-DIFF fill!=copy>` cases | No argparse; runs full report unconditionally, incl. bare | Confirmed **no file writes** (same grep check as above) | N-A |
| stub_census.py | **ONE-SHOT / UNRUNNABLE** (reclassified) | Ranks pinned-but-stubbed TUs whose oracle source (dc3/rb3-Wii) is materially fuller than the in-tree `.cpp` (the SongParser full-file-port vein) | Hardcodes `ROOT = Path('/home/free/tmp/wt-lane2-songparser')` — **that worktree no longer exists** (verified: `ls` → No such file or directory) | Initial read looked like a plain missing-module bug (`ModuleNotFoundError: native_scope_map`), but `scripts/native_scope_map.py` genuinely exists in the **current repo** — the tool just imports it from the stale lane worktree's `scripts/` dir instead, via `sys.path.insert(0, str(ROOT/'scripts'))`. Not fixable without either restoring that worktree or re-pointing ROOT | N-A |
| subobject_ref_scan.py | WORKING | Scans subobject-reference (base-class/member-offset call-site) mismatches, bounded by pct/size, single-symbol or wide mode | `--help` OK | N-A | N-A |
| superclass_chain_audit.py | WORKING | Audits `*_SUPERCLASS` macro chains against rb3-Wii/DC3 oracles; tracks an `HX_NATIVE` ifdef stack (avoids double-counting guarded-dead sites) and distinguishes oracle-blind families (e.g. rb3-Wii has zero `BEGIN_SAVES` sites) from genuine agreement | `--help` OK; `--list --json` | Prior landed result cited: removing a DC3-only trailing `SYNC_SUPERCLASS(Hmx::Object)` across 97 classes banked +29 strict | N-A |
| switch_frame_census.py | WORKING | Automates the "switch-frame lever": census/funclets/find subcommands compare retail vs our stack-slot frame for `switch` functions (retail read straight from the PE, ours from the compiled COFF) | `--help` OK; `--project-dir` | Cited: exact-frame fix flipped 145 EH funclets on `SaveLoadManager::GetDialogMsg` from a 6-line diff while the body was still at 0% | N-A |
| test_resolve_json_union.py | WORKING | Self-contained test suite for `resolve_json_union.py`'s 3-way merge logic | Bare run executes the suite directly: "25 passed, 0 failed", exit 0 | N-A | N-A |
| thunk_callee_bodydiff.py | **WRITES unconditionally** (`~/tmp/bodyport_thunk_callees.json`) | Derives virtual-method body identities from adjustor-thunk shapes; classifies SAME/DIFF/ABSENT vs our compiled obj | No argparse — module-level code executes on ANY invocation; my `--help` probe **ran it to completion and wrote the file** (263 records, confirmed in captured output) | Runtime: completed within the 25s probe window | **YES** (minor) — `(BUILD/"src").rglob(...)` fallback path resolution, no live_units |
| thunk_callee_freename.py | **WRITES on completion** (`~/tmp/bodyport_freename.json`) — did not write this run | For thunks whose OWN name is already mapped, reads the callee VA from machine code and names it via our compiled obj's relocation; classes MAPPED_OK/MAPPED_BAD/FREE/TAKEN | No argparse; imports from `thunk_callee_bodydiff.py`; my `--help` probe **timed out at 15s (EXIT 124)** before the whole-binary loop finished, so no file was written this time | Runtime >60s for a full sweep (timed out at 15s, presumably longer) — note per task instructions | N-A (inherits size_order_automap's glob via import) |
| thunk_edge_audit.py | WORKING | Audits/plans "thunk edge" reassignments with `--plan {group,unit}`, optional `--apply`, `--emit` | `--help` OK (the `--apply` flag path would WRITE — UNRUN-BY-POLICY for that flag specifically) | N-A | **YES** — 2 glob hits (`SRC.rglob`, `glob.glob(SRC/**/*.obj)`), no live_units |
| thunk_identity_namer.py | WORKING | Names candidate thunk identities and emits an apply-ready fragment (source of the vetted thunk primitives later extracted into `thunk_shape.py`) | `--help` OK; `--emit` only, no apply flag — read-only | N-A | **YES** — `(BUILD/"src").rglob(...)`, no live_units |
| thunk_shape.py | WORKING | Pure library module: the four vetted MSVC-X360 adjustor-thunk primitives (`shape()`, `td()`, `prefix()`, `norm()`) shared by all thunk-identity tools; explicitly documents 3 measured bugs it fixes in a prior resolver (scope-fold, non-thunk tail calls, `W`-form miss) | Bare run: no argparse, no I/O, no module-level side effects — exits 0 silently (it's an import target, not a CLI) | N-A | N-A |
| tu5_correlate_global_driver.py | WORKING (likely SUPERSEDED) | Original one-off whole-binary CLEAN-sweep driver for the TU5 reloc-masked-correlator stack | No argparse; bare run executed the full sweep and printed a summary table (573 pairs, 5,995 CLEAN proposals, 411 with yield) | `tu5_correlate_stage1.py`'s docstring explicitly says it "replaces the ad-hoc one-off enumeration used by the original +1,493 landing" — i.e. this file is likely superseded by that one, though both still run | N-A |
| tu5_correlate_stage1.py | WORKING | Re-runnable successor to the global driver: sweeps `pairs.json` (from `tu5_gen_pairs.py`), gates identical to the landed +1,493 sweep, adds collision guards (addr_already_mapped/denylist/name_value_taken/dup_pick_name) for repeatable re-runs | `--help` OK; `--project-dir --pairs --out-dir` | Writes `proposals.json/per_unit.json/errors.json/map_fragment.json` into `--out-dir` (not tracked repo files) | N-A |
| tu5_gen_pairs.py | WORKING | Regenerates `pairs.json` (the shared input for the whole TU5 correlator stack) at current build state, replacing the hardcoded one-off `~/tmp/correlator_sizing/pairs.json` | `--help` OK; `--project-dir --out` | Disambiguates 13 duplicate basenames (Dir, Utl, Rnd, Movie, CubeTex, FxSend*, …) by masked-content overlap | N-A |
| tu5_icf_disambiguate.py | WORKING (probe artifact) | Converts a precision-safe subset of the MULTI (base-side ICF-ambiguous) pool into map entries via the RELOC-TARGET-IDENTITY discriminator (reloc-destination name sequence must uniquely select a candidate set) | No argparse; positional `[pairs.json] [out_dir]`, default `pairs_path='/home/free/tmp/correlator_sizing/pairs.json'` — my `--help` probe resolved to the **stray `--help` artifact file** (a dict, not a list) → `TypeError: string indices must be integers` (NOT a real bug; see header note) | N-A | N-A |
| tu5_map_apply_fragment.py | WORKING | Textual map-fragment applier: inserts `{addr: name}` entries from a stage's `map_fragment.json` as single lines right after the map's opening brace, never `json.dump`-rewrites | `--help` OK ("apply" in name → UNRUN-BY-POLICY for a real run) | Collision asserts: addr collision, name collision — fails fast, never silently overwrites | N-A |
| tu5_nearpair_scan.py | WORKING (probe artifact) | Near-pair scan variant of the TU5 correlator stack | Same `pairs.json`-default pattern as `tu5_icf_disambiguate.py` — my probe hit the same stray `--help` artifact → `TypeError: string indices must be integers` (not a real bug) | N-A | N-A |
| tu5_reloc_masked_correlate.py | WORKING (probe artifact) | Core reloc-masked byte-identity function-body matcher (`func_bodies()`) underlying the whole TU5 correlator stack | No argparse; `func_bodies(sys.argv[1])` treats argv[1] as a real COFF obj path — my probe passed the stray `--help` JSON file, parsed as garbage COFF → `struct.error` (not a real bug) | N-A | N-A |
| tu5_reloc_seq.py | WORKING (probe artifact) | Reloc-target name-sequence extraction for a unit/base pair | No argparse; `unit_base = sys.argv[2]` — probe crashed `IndexError` (only 1 arg given, expected) | N-A | N-A |
| tu5_target_twin_disambiguate.py | WORKING (probe artifact) | Twin-disambiguation variant for the TU5 correlator stack (target-side twins) | Same stray-`--help`-as-pairs.json pattern as icf_disambiguate/nearpair_scan → `TypeError: string indices must be integers` (not a real bug) | N-A | N-A |
| tu_locality_invert.py | WORKING | TU-locality inversion resolver: homes unmapped functions via `.pdata` neighbour-window evidence (`tuloc` channel resolves on its own; `confirm` channel = caller-side inversion with family cap lifted, TU-locality only needs to agree) | `--help` OK; very large flag set (`--results --worktree` required) | `--min-family 17` documented as exactly the big-family pool caller-side inversion refuses | N-A |
| unemitted_symbol_scan.py | WORKING | **(special attention, brand-new/untracked)** Finds target functions unclaimable by ANY name the build compiles (zero-emitting object anywhere in the build for that symbol name); classifies FIX-SIG (wrong declaration/signature under correct scope) vs ADD-DECL (name never emitted at all); also flags Ham↔Band variant-swap candidates. Seed case: `ObjRefConcrete<T>::SetObj` vs `Replace` | `--help` OK; `--json --min-size` | Brand new (untracked in git status) | **YES** — `os.walk(SRC_OBJ)` where `SRC_OBJ = BUILD/"src"`, no live_units import |
| unit_scoped_twin_map.py | WORKING | Unit-scoped twin-symbol mapping, 3-pass pipeline (`--pass 1/2/3`) | `--help` OK; positional `worktree` ("apply" flag present → UNRUN-BY-POLICY for `--apply` invocations) | N-A | **YES** — 2 glob hits, no live_units |
| unnamed_parent_verify.py | WORKING | Unit-scoped reloc-masked byte-identity resolution for UNNAMED EH-parent functions (attacks the identity question from the "which of this unit's symbols is this VA" direction rather than whole-binary homing); `--validate` measures held-out precision over already-named parents | `--help` OK; `--worktree` required; `--census --validate --propose --strict-unique --all` | Measured (lane L, 2026-07-26): naming a parent does NOT cascade its EH funclets — funclet pairing is independent of the parent's map entry | N-A |
| vftable_name_contradiction_scan.py | WORKING | Scans for vftable-derived class names that contradict the name already mapped elsewhere for that class | `--help` OK; positional `worktree`, `--json` | N-A | N-A |
| vtable_1anchor.py | WORKING (probe artifact) | Single-anchor vtable identification pass (uses `vtable_global.extract_runs`) | No `--help`; positional project-dir consumed to build `RDATA_OBJ` path — probe's `--help` arg produced `NotADirectoryError` (artifact, not real) | N-A | **YES** — `glob.glob(PROJ+'/build/45410914/src/**/*.obj')`, no live_units |
| vtable_align_diag2.py | WORKING (probe artifact) | Vtable-alignment diagnostic, v2 | Same `PROJ`-positional pattern as `vtable_1anchor.py` → `NotADirectoryError` artifact | Calls `vg.extract_runs` — inherits `vtable_global.py`'s glob transitively | N-A (direct); inherits vtable_global.py's glob transitively |
| vtable_align_diag.py | WORKING (probe artifact) | Vtable-alignment diagnostic (original) | Same `PROJ`-positional pattern → `NotADirectoryError` artifact | N-A | **YES** — direct hit at line 70, plus inherits vtable_global.py's |
| vtable_global.py | WORKING (probe artifact) | **(foundational library)** Shared COFF-read + vtable-run-extraction helpers (`read_coff`, `extract_runs`) imported by `vtable_1anchor.py`/`vtable_align_diag.py`/`vtable_align_diag2.py`/`vtable_multianchor.py` | No `--help`; `PROJ`-positional → same `NotADirectoryError` artifact as its callers | This is the root glob source all four vtable_* tools above inherit | **YES** — `glob.glob(PROJ+'/build/45410914/src/**/*.obj')` at line 323, no live_units |
| vtable_multianchor.py | WORKING | Multi-anchor vtable identification with tiered confidence (`--tier A/B/C`), ICF-tolerant mode, holdout calibration | `--help` OK; positional `proj outdir`, `--min-anchors --tier --holdout --seed --icf-tolerant --min-size` | N-A | **YES** — line 224, no live_units |

## Notes on out-of-scope subdirectory

`scripts/harvest/tu_locate/` (if present as a subdirectory) was not enumerated as
individual files per the task's file-list scope (`ls scripts/harvest/`, top level
only); not audited here.

### 7b. `tools/` and `scripts/` — build, measurement, orchestration

Machine-generated from an AST parse + a real `--help` run per argparse tool.
`purpose` is the tool's own docstring first line, truncated — not a re-summary.
`.sh` and `.js` entries were syntax-checked/read only, never executed.

#### `scripts/`

| tool | status | purpose | inv | stale-glob |
|---|---|---|---|---|
| `_census_r3.py` | WORKING(no argparse) | Variant of tu_wiring_census.py that reports the COMPILED-NOT-PINNED bucket (map entries whose mangled name IS a defined symbol in a compiled obj, but  | positional args | n/a |
| `_fix_split_loop.py` | WORKING(no argparse) | Iteratively fix dtk SPLIT boundary errors in splits.txt by snapping the end address to the authoritative function end reported by dtk's error message, | positional args | n/a |
| `_fix_split_loop_r3.py` | WORKING(no argparse) | Iteratively fix dtk SPLIT boundary errors in splits.txt by snapping the end address to the authoritative function end reported by dtk's error message, | positional args | n/a |
| `atexit_fuzzy_verify.py` | WORKING | Atexit destructor fuzzy verifier. Runs after `obj_atexit_scope_patcher.py` to mark patched `??__F*` symbols | --help | n/a |
| `audit_splits.py` | WORKING(no argparse) | Audit splits.txt .text ranges against the authoritative function-boundary table in symbols.txt (post-truncation-fix, grown sizes). | positional args | n/a |
| `batch_check.py` | WORKING | Batch-check all untracked functions in a unit. Runs objdiff on each, auto-reports 100% matches as COMPLETE. | --help | n/a |
| `batch_rtti_probe.py` | WORKING(no argparse) | Batch-probe a list of candidate addresses (one per line, from find_replace_candidates.py output) via Ghidra RTTI and print address -> resolved T name  | positional args | n/a |
| `check_objects_json.py` | WORKING | Check for mismatches between src/ files and config/45410914/objects.json Reports: | --help | n/a |
| `clean_stale_objects.sh` | SHELL | (shell script — not executed by this audit; read its header) | positional args | n/a |
| `configure_existing_worktree.sh` | SHELL | (shell script — not executed by this audit; read its header) | positional args | n/a |
| `create_data_stubs.py` | WORKING | Create supplement-stub .obj files from split .objs for Matching units. When the build links decomp .obj instead of split .obj for Matching units, | --help | n/a |
| `dc3_compare.py` | WORKING | Compare RB3-xenon and DC3 decomp databases to find porting opportunities. Finds functions in the shared system/ engine that DC3 has matched but RB3 ha | --help | n/a |
| `dump_vtable.py` | WORKING | Dump vtable layout from original COFF .obj files. Reads COFF symbol and relocation tables to reconstruct vtable entries, | --help | **YES** |
| `extract_decomp_symbols.py` | WORKING | Extract data symbol names from decomp .obj files and map to original VAs. For Matching units, the decomp .obj exports data symbols with real C++ names | --help | n/a |
| `find_replace_candidates.py` | WORKING(no argparse) | List candidate unmatched functions (size in {192,236,260}, no/low match) across the set of pinned units that instantiate an unmapped ObjPtrList<T,Obje | positional args | n/a |
| `find_truncated_splits.py` | WORKING(no argparse) | Find split .text ranges that TRUNCATE a function mid-body. jeff-INDEPENDENT: works off dtk's emitted target asm (the instruction stream + | positional args | **YES** |
| `find_underpins.py` | WORKING(no argparse) | Find candidate UNDER-PINNED .text ranges: a pinned TU whose range ends exactly at a function start, where that function (and the contiguous run after  | positional args | n/a |
| `get_progress.py` | WORKING(no argparse) | Get decomp progress summary. Returns total/complete/at_limit counts, percentages, pattern breakdown, | positional args | n/a |
| `idtransfer_harvest.py` | WORKING | the identity-transfer harvest DRIVER (PIPELINE-DESIGN .md S2 architecture / S3 Phases 1-10 / S9 B3 / S10 hard-fail gates). | --help | n/a |
| `ingest_report.py` | WORKING | Ingest report.json into the orchestrator database. Usage: | --help | n/a |
| `mangle_backref_scan.py` | WORKING(no argparse) | Find map entries that denote the SAME function as one of our compiled symbols but are spelled with a different MANGLING (e.g. missing MSVC back-refere | positional args | n/a |
| `map_verify.py` | WORKING | audit `scripts/target_symbol_map.json` entries against static evidence, per pinned unit or tree-wide. | --help | **YES** |
| `measure_progress.sh` | SHELL | (shell script — not executed by this audit; read its header) | positional args | n/a |
| `native_scope_map.py` | WORKING(no argparse) | authoritative NATIVE-SCOPE decomp map for rb3-xenon. Decomp scope is defined by the NATIVE PORT (native/ x86_64 engine; see | positional args | n/a |
| `obj_anon_ns_patcher.py` | WORKING | Post-build patcher for MSVC anonymous namespace hashes in .obj files. MSVC generates anonymous namespace hashes (e.g., ?A0x12345678@@) based on | --help | **YES** |
| `obj_atexit_scope_patcher.py` | WORKING | Post-build patcher: rename ??__F atexit destructor symbols to match target scope counters. | --help | n/a |
| `obj_bool_mangle_patcher.py` | WORKING | Post-build patcher: fix bool parameter back-reference mangling. Our MSVC compiler caches `bool` (_N) in the parameter back-reference table, | --help | **YES** |
| `obj_dynamic_init_patcher.py` | WORKING | Post-build patcher to promote ??__E dynamic initializer symbols from STATIC to EXTERNAL. MSVC emits ??__E symbols (C++ dynamic initializers for global | --help | n/a |
| `obj_guard_patcher.py` | WORKING | Post-build patcher: convert $S guard variables to ??_B format. Compares decomp .obj files against original .obj files and renames | --help | **YES** |
| `obj_regswap_patcher.py` | WORKING | Post-compilation .obj register swap patcher. Patches PowerPC register fields in COFF .obj files to fix register swap | --help | n/a |
| `obj_target_symbol_renamer.py` | WORKING | Post-SPLIT patcher: rename anonymous `fn_<addr>` symbols in dtk-split target .obj files to their MSVC-mangled equivalents. | --help | **YES** |
| `obj_transplant_patcher.py` | WORKING | Post-build .obj transplant patcher. Replaces a function's COFF section data with the original .obj's machine code, | --help | n/a |
| `permuter_targets.py` | WORKING | rank the permuter's work queue from report.json. The source permuter (the `decomp_synth` package, wired via the `permute` skill) mechanizes | --help | n/a |
| `prune_orphan_asm.py` | WORKING | Delete orphaned `build/<title>/asm/*.s` files -- the stale-carve trap. WHY THIS EXISTS | --help | **YES** |
| `recon.py` | WORKING | Unified function reconnaissance — single command for full function intel. Combines: | --help | n/a |
| `reset_false_complete.py` | WORKING(no argparse) | One-time reset of false COMPLETE functions caused by base_size=0 objdiff bug. Functions were falsely marked COMPLETE when objdiff reported 100% match  | positional args | n/a |
| `residue_census.py` | WORKING(no argparse) | Residue census: map entries INSIDE pinned .text ranges of WIRED units that our compiled objs do NOT define. = body-port completion targets. | positional args | n/a |
| `rtti_probe.py` | WORKING(no argparse) | Given a candidate retail address, decompile it in Ghidra and, if it looks like an ObjPtrList<T,ObjectDir>::Replace body (calls the __RTDynamicCast thu | positional args | n/a |
| `scan_objptrlist_replace.py` | WORKING(no argparse) | Scan all compiled .obj files for ObjPtrList-family Replace/dtor/ctor/Unlink COMDAT symbols, and cross-reference against target_symbol_map.json to find | positional args | n/a |
| `scan_replace_sizes.py` | WORKING(no argparse) | For every pinned unit, list ObjPtrList<T,ObjectDir>::Replace COMDAT symbols with their exact section size (raw_size of their own COMDAT .text section) | positional args | n/a |
| `setup_worktree.sh` | SHELL | (shell script — not executed by this audit; read its header) | positional args | n/a |
| `signature_mismatch_scan.py` | WORKING(no argparse) | Whole-tree scan for MANGLED-NAME DIVERGENCE. A retail map entry and one of our compiled symbols can be the SAME function | positional args | n/a |
| `symbols_hygiene.py` | WORKING(no argparse) | audit config/45410914/symbols.txt fn_/except_data_ boundaries against the CURRENT retail xex's authoritative .pdata, scoped to pinned .text splits (th | positional args | n/a |
| `symbols_hygiene_fix.py` | WORKING(no argparse) | rewrite config/45410914/symbols.txt to remove stale TU5-era carving artifacts inside pinned .text ranges. | positional args | n/a |
| `sync_match_percent.py` | WORKING | Sync objdiff results from report.json into decomp.db. Reads the report generated by `ninja build/45410914/report.json` (which runs | --help | n/a |
| `truncation_audit.py` | WORKING(no argparse) | find NAMED near-miss target functions truncated by a stale symbols.txt boundary (the "swallowed tail" artifact), binary-wide. | positional args | **YES** |
| `tu_wiring_byunit.py` | WORKING(no argparse) | Attribute every map entry to its OWNING splits.txt unit (by address range), then report per-unit: total, compiled, uncompiled, wired?, source. This is | positional args | n/a |
| `tu_wiring_census.py` | WORKING(no argparse) | TU-wiring census: find map entries (functions retail HAS) that our build does NOT emit and are NOT pinned. Each orphan = a function whose owning TU is | positional args | n/a |
| `tu_wiring_census_r2.py` | WORKING(no argparse) | Variant of tu_wiring_census.py that reports the COMPILED-NOT-PINNED bucket (map entries whose mangled name IS a defined symbol in a compiled obj, but  | positional args | n/a |
| `tu_wiring_cluster.py` | WORKING(no argparse) | Cluster the census orphans by address (gap > GAP splits a cluster) and summarize dominant class + fn count + span per cluster. | positional args | n/a |
| `tu_wiring_rank.py` | WORKING(no argparse) | Per-owning-module census: map every game/engine map entry (addr<0x82800000) to an owning class/module, count total vs orphan, check wired + source ora | positional args | n/a |
| `validate_symbols.py` | WORKING | Validate symbols.txt addresses against known section ranges. Checks that function symbols in .text fall within the valid .text virtual | --help | n/a |
| `vsig_diff.py` | WORKING | Three-way virtual-signature differ (ours / dc3-decomp / rb3-Wii). Scans headers shared (same relative path) between this tree and the two | --help | n/a |
| `wf_bodyport_tails.js` | JS-WORKFLOW | (agent-workflow driver from the identity-transfer era; not run by this audit) | positional args | n/a |
| `wf_classa_harvest.js` | JS-WORKFLOW | (agent-workflow driver from the identity-transfer era; not run by this audit) | positional args | n/a |
| `wf_classa_ports.js` | JS-WORKFLOW | (agent-workflow driver from the identity-transfer era; not run by this audit) | positional args | n/a |
| `wf_idt_b2.js` | JS-WORKFLOW | # Read first | positional args | n/a |
| `wf_idt_build.js` | JS-WORKFLOW | # TASK B1 (the keystone — build with care) | positional args | n/a |
| `wf_idt_classb.js` | JS-WORKFLOW | (agent-workflow driver from the identity-transfer era; not run by this audit) | positional args | n/a |
| `wf_idt_harvest.js` | JS-WORKFLOW | # Read first | positional args | n/a |
| `wf_idt_research.js` | JS-WORKFLOW | (agent-workflow driver from the identity-transfer era; not run by this audit) | positional args | n/a |
| `wf_levers.js` | JS-WORKFLOW | # Part 1 — pick 4-5 SPECIFIC permuter targets (NOT a bulk sweep) | positional args | n/a |
| `wf_underpins.js` | JS-WORKFLOW | # Your candidates | positional args | n/a |

#### `scripts/analysis/`

| tool | status | purpose | inv | stale-glob |
|---|---|---|---|---|
| `__init__.py` | WORKING(no argparse) | (no docstring) | positional args | n/a |
| `audit_normalized_masking.py` | WORKING | Audit functions the normalized metric counts as "matched" but that are NOT byte-exact, to find whether the masked arg diffs are benign (register/branc | --help | n/a |
| `codeview_locals.py` | WORKING | Extract base-side local-variable → stack-offset mappings from MSVC CodeView. Recompiles a source file with `/Z7` (CodeView embedded in `.debug$S` COFF | --help | n/a |
| `coffx.py` | WORKING(no argparse) | Minimal COFF (PPC/XBOX360 MSVC + dtk-split) reader: sections, symbols, relocations. Mirrors the parts of objdiff's obj::read that funclet_signature de | positional args | n/a |
| `compare_progress.py` | WORKING | Compare decomp progress between two report.json files, or show current snapshot. Usage: | --help | n/a |
| `diff_inspect.py` | WORKING | Inspect objdiff JSON output for specific mismatch types with context. Usage: | --help | n/a |
| `stack_layout.py` | WORKING | Compare stack-frame layouts between target and base compilations (MSVC X360). For a given function, walk the objdiff target+base instruction stream an | --help | n/a |

#### `scripts/grind/`

| tool | status | purpose | inv | stale-glob |
|---|---|---|---|---|
| `__init__.py` | WORKING(no argparse) | rb3-xenon side of the decomp_synth bootstrap grind loop. Three project seams, kept dependency-light (stdlib + decomp_synth only): | positional args | n/a |
| `agent_tools.py` | WORKING(no argparse) | the grind loop's agentic tool belt (Anthropic tool-use spec). Gives the completion model a small, read-only investigation surface while it | positional args | n/a |
| `backfill_gold.py` | WORKING | regenerate lossless-capture records for PRE-CAPTURE grind runs (RFC-21 T4 backfill, ``docs/plans/grind-training-data-capture.md`` §7 item 6). | --help | n/a |
| `bench.sh` | SHELL | (shell script — not executed by this audit; read its header) | positional args | n/a |
| `campaign.py` | WORKING | Campaign driver for the rb3-xenon bootstrap grind loop. Reads a tasks JSON (the recon-A candidate list), builds a per-function | --help | n/a |
| `classify_funclets.py` | WORKING | EH-unwind-funclet detector + decomp.db tagger. Wave-3 lesson: the grind worklist repeatedly picked up MSVC X360 exception | --help | clean |
| `claude_backend.py` | WORKING(no argparse) | claude-swap completion backend (stdlib `urllib` only, no `requests`). A `CompletionBackend` for the grind loop that talks to the local **claude-swap | positional args | n/a |
| `corpus.py` | WORKING | sync the whole rb3-xenon grind training corpus out of B2 into one local mirror + one unified SQLite db that agents query directly. | --help | n/a |
| `enrich.py` | WORKING | Enrichment-packet builder for the rb3-xenon grind loop. The calibration doc (docs/plans/grind-loop-calibration-2026-07-07.md) found the | --help | n/a |
| `eval_report.py` | WORKING | aggregate grind eval run(s) into the metrics table. Reads one or more run dirs' attempts_full.ndjson (the lossless capture sink) and | --help | n/a |
| `export_training_data.py` | WORKING | Gated training-data exporter: rb3-xenon grind capture -> decomp-synth corpus. Ported from godzilla-decomp's ``grind/tools/export_training_data.py`` (s | --help | n/a |
| `harvest_landed_gold.py` | WORKING | mine main's LANDED matched functions into verified- correct GOLD SFT training rows (review §3 opportunity #3, the biggest untapped positive-signal sou | --help | n/a |
| `load_attempts_db.py` | WORKING | Fan-in loader: grind `attempts_full.ndjson` + `blobs/` -> `rb3_grind_attempts.db`. Collapses many per-run capture dirs (each an `attempts_full.ndjson` | --help | n/a |
| `merge_runs.py` | WORKING | Merge N grind campaign runs into a best-of-N result dir. The calibration experiments (docs/plans/grind-loop-calibration-2026-07-07.md) | --help | n/a |
| `push_corpus.sh` | SHELL | (shell script — not executed by this audit; read its header) | positional args | n/a |
| `recipe.py` | WORKING(no argparse) | Rb3XenonRecipe — the PowerPC / MSVC X360 bootstrap+refine prompt recipe. Adapted from godzilla-decomp's proven section structure, retargeted from | positional args | n/a |
| `run_eval_lanes.py` | WORKING | parallel sharded eval runner for the grind campaign. Shards an eval tasks file across N pinned worktree lanes and launches one | --help | n/a |
| `splice_scorer.py` | WORKING(no argparse) | Splice-and-score seam for the rb3-xenon bootstrap loop. The bootstrap loop hands the scorer a *single C++ function* (the LLM's | positional args | n/a |
| `synth_traces.py` | WORKING | enrich the training corpus with *synthetic* reasoning traces. The capture pipeline recorded WHAT happened (mismatch asm, the patch that landed, | --help | n/a |
| `teacher_critique.py` | WORKING | v0 teacher-critique training-row generator (RLHF, SFT-shaped). Design: docs/plans/grind-teacher-critique-rlhf.md §5 (v0-scoped: CRITIQUE ONLY — | --help | n/a |
| `validate_offline.py` | WORKING(no argparse) | Offline, zero-spend end-to-end validation of the rb3-xenon grind loop. Run from the project/worktree root with the shared venv: | positional args | n/a |
| `worklist.py` | WORKING | the canonical vetted-pool generator for the grind loop. Wave 3 wasted 4 of 8 matcher groups because its worklist was built straight from | --help | n/a |

#### `scripts/orchestrator/`

| tool | status | purpose | inv | stale-glob |
|---|---|---|---|---|
| `__init__.py` | WORKING(no argparse) | RB3-Xenon Decomp Orchestrator - Multi-agent decompilation pipeline. | positional args | n/a |
| `database.py` | WORKING(no argparse) | Database module for RB3-Xenon Decomp Orchestrator. Handles SQLite database for persistent state tracking of functions, | positional args | n/a |
| `mcp_server.py` | WORKING | MCP Server for RB3-Xenon Decomp Orchestrator. Provides tools for sub-agents to: | --help | n/a |
| `worktree_pool.py` | WORKING(no argparse) | Worktree pool for agent isolation. Each agent works in its own git worktree to prevent file conflicts. | positional args | n/a |

#### `scripts/recarve/`

| tool | status | purpose | inv | stale-glob |
|---|---|---|---|---|
| `climb.py` | WORKING | Recarve Stage B: empirical boundary hill-climb for one EXTEND candidate. Policy: extend-all, then trim-to-last-good, confirm (2-3 builds total, not | --help | n/a |
| `funclets.py` | WORKING(no argparse) | Shared EH-funclet detector for the recarve pipeline. MSVC X360 EH unwind funclets establish the PARENT frame via r12: | positional args | **YES** |
| `scan.py` | WORKING | Recarve Stage A: scan pinned TUs for attribution-repair candidates. Merges three deterministic signals per pinned TU (see | --help | n/a |

#### `scripts/triage/`

| tool | status | purpose | inv | stale-glob |
|---|---|---|---|---|
| `divergence_triage.py` | WORKING | Batch divergence-triage classifier for the rb3-xenon decomp pool. Buckets named divergent functions into a fixed taxonomy so the project can | --help | n/a |
| `reprice_router.py` | WORKING | measured per-bucket flip-rates for the triage router. Joins real grind-attempt outcomes from decomp.db against the triage bucket | --help | n/a |

#### `tools/`

| tool | status | purpose | inv | stale-glob |
|---|---|---|---|---|
| `__init__.py` | WORKING(no argparse) | (no docstring) | positional args | n/a |
| `ab_measure.py` | WORKING | REWRITTEN 2026-08-01 (lane AB-TOOL): protocol-enforcing whole-binary A/B harness. Measures BOTH legs in-run (no --baseline by design), settles to zero-work, wipes report cache, strict-key reads, forces re-split for map/splits patches, REFUSES broken runs (exit 2, no numbers). See CLAUDE.md "Whole-binary A/B measurement" + /ab-measure skill. | --help, --selftest | n/a |
| `band3_worklist_pin.py` | WORKING | Deterministic band3-worklist micro-pin + name (NO broad-oracle scatter). The safe replacement for the Sonnet `identity_transfer --tu X --apply` step t | --help | n/a |
| `build_dc3_oracle.py` | WORKING | DC3-VA <-> RB3Xenon-VA engine oracle. Joins a BinDiff DC3-vs-RB3Xenon result with DC3's leaked ham_xbox_r.map to emit | --help | n/a |
| `changes_fmt.py` | WORKING | (no docstring) | --help | n/a |
| `classify_nearmiss.py` | WORKING | Classify the mismatch CAUSE of near-miss functions via objdiff per-fn JSON. For each function in a match band, run objdiff diff -f json and bucket eac | --help | n/a |
| `classify_nearmiss_codegen.py` | WORKING | Classify the 90.0-99.99% near-miss pool by CODEGEN root cause. Body-divergence wall #2 diagnostic. For every function report.json scores in | --help | n/a |
| `dc3_content_match.py` | WORKING | Cross-binary COFF content-matcher: identify RB3 functions by DC3 name. Why | --help | **YES** |
| `dc3_map.py` | WORKING(no argparse) | parse the leaked Microsoft linker map for ham_xbox_r.exe and the dc3 objects.json, then expose `mangled_name -> source .cpp` lookups. | positional args | n/a |
| `dc3_name_eligible.py` | WORKING | the DC3-oracle FUZZY-drain tool. Emit ADD-ONLY {rb3_va -> dc3_name} entries for ANONYMOUS engine targets that the | --help | n/a |
| `dc3_obj_source.py` | WORKING(no argparse) | Single source of truth for *which* DC3 object tree the cross-binary function-identification tools read. | positional args | n/a |
| `dc3_residual_rank.py` | WORKING | Rank unwired DC3 engine TUs by expected RB3 byte-match yield. For each residual DC3 system TU (compiled in dc3 but not wired in our | --help | **YES** |
| `decompctx.py` | WORKING | (no docstring) | --help | n/a |
| `defines_common.py` | WORKING(no argparse) | (no docstring) | positional args | n/a |
| `demangle_cw.py` | WORKING(no argparse) | Best-effort MetroWerks/CodeWarrior symbol demangler for evidence packs. Produces a human-readable "class::method(args)" rendering from a CW-mangled | positional args | n/a |
| `download_tool.py` | WORKING | (no docstring) | --help | n/a |
| `enrich_unattributed.py` | WORKING | Sub-classify the UNATTRIBUTED near-miss bucket by instruction-level signature. The fork's pattern detector finds no recognized pattern for ~560 real-b | --help | n/a |
| `field_offset_gate.py` | WORKING | partial-port POISONED-TAIL static analyzer (B1). THE PROBLEM (PIPELINE-DESIGN.md S6 / research/04-sourceport-bottleneck.md S2A) | --help | n/a |
| `field_offset_gate_validate_rockcentral.py` | WORKING(no argparse) | Validate field_offset_gate against the proven RockCentral.cpp +17 win (B1). PIPELINE-DESIGN.md S9 B1 / S10 gate 5 require: the field_offset_gate must  | positional args | n/a |
| `find_struct_gaps.py` | WORKING(no argparse) | Scan headers for struct layout mismatches by comparing offset comments to expected sizeof. | positional args | n/a |
| `fingerprint_match.py` | WORKING | identify anonymous RB3 functions by cross-referencing the shared Milo engine against dc3-decomp's (near-fully-named) symbol set. | --help | **YES** |
| `fingerprint_pipeline.py` | WORKING | same-compiler GAME-code fingerprint pipeline. The premise (verified by spike, 2026-05-30): rb3-Wii game source compiled under | --help | n/a |
| `fn_resolver.py` | WORKING | resolve anonymous fn_8XXXXXXX addresses to identities. RB3's retail XEX has 66k anonymous functions named fn_8XXXXXXX. This tool | --help | n/a |
| `fresh_report.sh` | SHELL | (shell script — not executed by this audit; read its header) | positional args | n/a |
| `fuzzy_content_match.py` | WORKING | Similarity-based cross-binary function id (DC3->RB3): recover matches the EXACT masked-hash matcher (tools/dc3_content_match.py) drops. | --help | n/a |
| `fuzzy_progress.py` | WORKING | tiered FUZZY progress reporter for the rb3-xenon decomp. Operationalizes the rank-1 recommendation of | --help | n/a |
| `game_content_match.py` | WORKING | Cross-binary COFF content-matcher for GAME code: identify RB3 functions by the name of OUR COMPILED GAME BASE obj's defining section symbol. | --help | **YES** |
| `game_oracle_triage.py` | WORKING | Whole-game identification triage from the RB3-Wii BinDiff oracle. Consumes `unified_id_rb3wii.json` (RB3-360 addr -> RB3-Wii source TU, produced by | --help | n/a |
| `game_splits.py` | WORKING | derive TARGET-ONLY .text splits for GAME TUs from the rb3-Wii cross-binary bindiff oracle (unified_id_rb3wii.json). | --help | n/a |
| `gap_atlas.py` | WORKING | the definitive 5-bucket breakdown of the unmatched binary. Regenerates the "gap composition atlas" (docs/plans/paths-to-100/ | --help | n/a |
| `gen_band3_port_worklist.py` | WORKING | Generate the band3 porting worklist from the Wii->Xenon ghidriff identities. CONSUMES (no ghidriff run): the 978 ACCEPT identities ingested into rb3-x | --help | n/a |
| `gen_game_target_map.py` | WORKING | Generate target_symbol_map.json entries for RB3 *game* TUs. Pipeline role | --help | n/a |
| `gen_symbol_alias_map.py` | WORKING | Generate an MSVC-linker-map-format file declaring PROVEN ICF symbol aliases. Why this exists (the ICF-merged-symbol aliasing gap) | --help | n/a |
| `gen_sysnet_port_worklist.py` | WORKING | Generate the system/network porting worklist from the Wii->Xenon ghidriff identities. Sibling of `gen_band3_port_worklist.py`, generalized to `categor | --help | n/a |
| `global_fuzzy_index.py` | WORKING(no argparse) | PROTOTYPE: global cross-binary fuzzy index (DC3 named -> RB3 anon) via banded MinHash LSH over reloc-masked opcode shingles. Finds high-similarity pai | positional args | **YES** |
| `icf_alias_check.py` | WORKING | honesty audit for "ICF-alias inflation" in pinned spans. THE LESSON (codified here) | --help | n/a |
| `icf_alias_finder.py` | WORKING | Find / validate PROVEN ICF-merged-symbol alias pairs (the PoolAlloc gap). The problem | --help | clean |
| `identity_transfer.py` | WORKING | Per-function MICRO-PIN identity transfer for ICF-SCATTERED TUs. THE PROBLEM | --help | n/a |
| `inline_policy_finder.py` | WORKING | inline_policy_finder — detect the INLINE-POLICY force-multiplier near-miss class. THE PATTERN (proven win: String::operator==/!= — commit ce16bfa, +6) | --help | n/a |
| `layout_family.py` | WORKING | Enumerate a base class's full layout family + each member's current match state. A coupled-base edit shifts every class that INHERITS or EMBEDS the ba | --help | n/a |
| `layout_fix_rank.py` | WORKING | Rank candidate struct/base-class LAYOUT fixes by empirical fan-out. The engine matching wall is offset-class: ~97% of near-miss functions differ from | --help | n/a |
| `locator.py` | WORKING | per-method VA-placement classifier for ICF-scattered TUs. THE PROBLEM (hard-frontier #2) | --help | n/a |
| `map_lint.py` | WORKING | target_symbol_map consistency linter. Cross-checks ``scripts/target_symbol_map.json`` (the address->MSVC-mangled-name | --help | **YES** |
| `member_delta_finder.py` | WORKING | detect the DC3 DROPPED/ADDED-MEMBER force-multiplier. THE PATTERN (proven wins: CharSleeve/CharIKSliderMidi mMe -0xC; Gem Tail -0x14; | --help | n/a |
| `member_delta_finder2.py` | WORKING | CLASSIFY member-offset delta candidates. v2 adds a CLASSIFY stage on top of v1's detection core. Candidates are tagged: | --help | **YES** |
| `ninja_syntax.py` | WORKING(no argparse) | Python module for generating .ninja files. Note that this is emphatically not a required piece of Ninja; it's | positional args | n/a |
| `objdiff_to_m2c.py` | WORKING | Convert objdiff JSON output to m2c-compatible assembly format. This script parses the JSON output from objdiff-cli (with --include-instructions) | --help | n/a |
| `oracle_contiguity_scan.py` | WORKING | stub-filtered contiguity scanner for option-C port-then-pin target selection. | --help | n/a |
| `oracle_quality.py` | WORKING | Oracle-quality pre-screen for identity-transfer harvest target selection. B2 warm-up (2026-06-21) found the dominant wall is NOT the source port — it  | --help | n/a |
| `permuter_targets.py` | WORKING | Produce a RANKED permuter-target list from the near-miss cause-class JSONs (tools/classify_nearmiss.py output) — bucketed by true permuter-viability. | --help | n/a |
| `pin_audit.py` | WORKING | sliver / over-pin / displaced-pin detector (READ-ONLY). Systematizes the sliver-pin vein from | --help | **YES** |
| `pin_candidates.py` | WORKING | the unified oracle -> pin ranker. Designed by docs/plans/execution-schedule.md S2. We have FIVE oracle sources | --help | n/a |
| `pin_identified.py` | WORKING | Pin helper for byte-IDENTIFIED but UNPINNED functions. WHY | --help | n/a |
| `project.py` | WORKING(no argparse) | (no docstring) | positional args | n/a |
| `rbtree_blast.py` | WORKING | Census of the R-B tree +4 coupled-base blast radius. Retail STLport `_Rb_tree` is 0x1c; ours is 0x18 (missing one 4-byte member after | --help | n/a |
| `refill_loop.sh` | SHELL | (shell script — not executed by this audit; read its header) | positional args | n/a |
| `refresh_permuter_db.py` | WORKING | Refresh decomp.db for the permuter: re-ingest report.json AND populate the per-function objdiff metadata (current_percent / best_percent / verdict) th | --help | n/a |
| `relocate_engine_splits.py` | WORKING | Relocate under-pinned engine TU splits onto their real bodies, from content-match identifications (tools/dc3_content_match.py). | --help | n/a |
| `relocate_game_splits.py` | WORKING | Relocate / pin GAME-code TU splits onto their real bodies, from cross-binary COFF content-match identifications (tools/game_content_match.py -> game_c | --help | n/a |
| `reveal_sweep.py` | WORKING | Symbol-map "reveal" sweep. Find unmapped target ``fn_<addr>`` whose normalized bytes equal a not-yet-matched | --help | n/a |
| `safe_name_merge.py` | WORKING | Collision-safe naming GATE for target_symbol_map.json — the load-bearing invariant every bulk naming/pinning sweep MUST pass through before merge. | --help | n/a |
| `scope_map.py` | WORKING | classify every function in the RB3-360 XEX into a decomp scope bucket and compute a *meaningful* progress denominator. | --help | n/a |
| `span_confirm.py` | WORKING | oracle-plurality span-identity confirmer (ws7 R3). Cheap identity check for "this .text span is TU X" using the committed | --help | n/a |
| `split_imm_offset.py` | WORKING | Split the IMM_OFFSET near-miss sub-bucket into STACK vs STRUCT vs CONST. For each IMM_OFFSET fn, pull the instruction diff and look at the differing | --help | n/a |
| `static_symbol_finder.py` | WORKING | static_symbol_finder — worklist generator for two GAME-layer near-miss levers. Motivation (wave-2 batch-2, Gem +8 / GuitarController +4, commit e4180d | --help | n/a |
| `struct_db.py` | WORKING | Struct offset resolution tool for DC3 decomp. Parses annotated headers to build a struct offset lookup database. | --help | n/a |
| `symbol_sweep_scan.py` | WORKING | READ-ONLY scanner for the RFC-14 systematic symbol sweep. Implements the scanner proposed in | --help | n/a |
| `topo_locate.py` | WORKING | Callee-Set Topological Locator (hard-frontier identifier). Locates scattered-TU game methods in the RB3-360 retail binary purely by | --help | n/a |
| `transform_dep.py` | WORKING(no argparse) | Normalise MSVC /showIncludes output so Ninja can consume it on non-Windows platforms. Reads from stdin, writes the transformed lines to stdout. | positional args | n/a |
| `true_progress.py` | WORKING | Honest "true progress" classifier for the near-miss pool. Motivation (LTO/ICF investigation, 2026-06-06): the project owner suspected the | --help | n/a |
| `tu5_map_build.py` | WORKING(no argparse) | Build the base(TU0) -> TU5 function VA map for every NAMED base function. Two-stage method (proven on the 7-address spike + 25-fn sample): | positional args | n/a |
| `tu5_skel_recover.py` | WORKING(no argparse) | Recover correct TU5 VAs for a set of TU0 functions via section-mapped, relocation-normalized opcode-skeleton matching. | positional args | n/a |
| `tu5_va.py` | WORKING(no argparse) | Section-mapped VA reader/disassembler for the TU5 (v0.0.5.1) RB3 PE. TU5's XEX is "basic"-format: the loaded image is SECTION-MAPPED, so the flat | positional args | n/a |
| `unknown_triage.py` | WORKING | honestly characterize the "unknown" scope bucket. scope_map.py sums per-fn `size` fields to size the unknown bucket, but those | --help | n/a |
| `update_readme_progress.py` | WORKING | Regenerate the README progress table from build/45410914/report.json. Rewrites the block between the `progress-table:begin` / `progress-table:end` | --help | n/a |
| `va_disasm.py` | WORKING(no argparse) | Disassemble a VA range from band.exe (decompressed RB3 PE) using capstone. Ghidra-independent body inspection for the STEP-1 recon-gate. Reads the PE | positional args | n/a |
| `va_disasm_tu5.py` | WORKING(no argparse) | Section-mapped VA reader/disassembler for the TU5 (v0.0.5.1) RB3 PE. TU5's XEX is "basic"-format: the loaded image is SECTION-MAPPED, so the flat | positional args | n/a |
| `va_size.py` | WORKING(no argparse) | Find the true size of a function at a VA from band.exe .pdata. The Xbox360 PE has a .pdata section: RUNTIME_FUNCTION array sorted by BeginAddress. | positional args | n/a |
| `vector_arity.py` | WORKING | Classify std::vector template-parameter arity in retail RB3. MSVC mangles vector<T, Alloc> as: | --help | n/a |
| `wall_classify.py` | WORKING | Auto-tag HAS_REAL near-miss functions with playbook wall classes. Implements the 8 wall detectors from docs/decomp/playbooks/hasreal-grind.md §3 plus | --help | n/a |
| `xdbg.py` | WORKING(no argparse) | xdbg — RB3DX (Xbox 360) live-crash + static-disasm helper. One tool for the SI-hardware debug loop: capture a live crash over XBDM, map the | positional args | n/a |
| `xex_binpatch.py` | WORKING | XEX2 flat-image binary patcher for the RB3 "Same Instrument" static code-cave patch. The target default.xex (title 45410914) is an UNCOMPRESSED, UNENC | --help | n/a |
| `xex_binpatch_tu5.py` | WORKING | XEX2 SECTION-MAPPED binary patcher for the TU5 (retail v0.0.5.1) Same-Instrument code-cave patch. | --help | n/a |
| `xex_string_at.py` | WORKING | read a C string (or raw bytes) at a retail virtual address. The retail RB3 XEX (title 45410914) decodes to an ordinary big-endian PowerPC | --help | n/a |

#### `tools/ghidra/`

| tool | status | purpose | inv | stale-glob |
|---|---|---|---|---|
| `__init__.py` | WORKING(no argparse) | (no docstring) | positional args | n/a |
| `apply_symbols.py` | WORKING | import our known mangled symbols into the RB3 Ghidra project by renaming the anonymous fn_<addr>/FUN_<addr> functions, so the decomp_synth permuter's  | --help | n/a |
| `batch_export.py` | WORKING | Batch export Ghidra decompilations and cross-references to SQLite cache. Pre-caches all function decompilations so an orchestrator can serve them | --help | n/a |
| `batch_export_types.py` | WORKING | Batch export Ghidra-inferred structure types into struct_db.sqlite. Note: rb3-xenon has no leaked .map file (unlike DC3). The seed pipeline here | --help | n/a |
| `bsim_seedprop_measure.py` | WORKING(no argparse) | Measurement: cross-tool precision + per-stem densification for the seed-prop experiment. Inputs: seedprop_matches.json, baseline_matches.json, seeds.j | positional args | n/a |
| `build_full_symbol_map.py` | WORKING | produce the FULL base-program naming map for Ghidra. apply_symbols.py consumes {"0x82...": {"symbol": <mangled>, "demangled"?: ...}}. | --help | n/a |
| `build_symbol_map.py` | WORKING | Extract a high-confidence fn_<addr> -> mangled_symbol map for the RB3 retail XEX, so the names can be imported into the Ghidra project (see apply_symb | --help | n/a |
| `code_search.py` | WORKING | Semantic search over Ghidra decompiled code via pyghidra-mcp's ChromaDB vector index. Usage: | --help | n/a |
| `direct_client.py` | WORKING | DirectGhidraClient: Direct Python→Java→Ghidra bridge for decompilation context. Provides direct access to Ghidra without HTTP overhead, optimized for  | --help | n/a |
| `export_types.py` | WORKING | Export Ghidra-discovered types as C headers for m2c context. Since the Ghidra MCP doesn't expose direct type manager APIs, this script | --help | n/a |
| `ghidra-callgraph.py` | WORKING | Generate call graph for a function in the Ghidra project. Usage: | --help | n/a |
| `ghidra-decompile.py` | WORKING | Decompile a function from the Ghidra project. Usage: | --help | n/a |
| `ghidra-search.py` | WORKING | Search for symbols, strings, or code in the Ghidra project. Usage: | --help | n/a |
| `ghidra-status.py` | WORKING | Check Ghidra MCP server status and list available binaries. Usage: | --help | n/a |
| `ghidra-xrefs.py` | WORKING | List cross-references for a symbol or address in the Ghidra project. Usage: | --help | n/a |
| `import-xex.sh` | SHELL | (shell script — not executed by this audit; read its header) | positional args | n/a |
| `mcp_client.py` | WORKING(no argparse) | MCP client for Ghidra pyghidra-mcp server. Handles session initialization, JSON-RPC formatting, and response parsing. | positional args | n/a |
| `pcode_inspect.py` | WORKING | Inspect Ghidra decompilation for switch statements and cast operations. Since pyghidra-mcp does not expose raw pcode, this script analyzes: | --help | n/a |
| `pyghidra-service.sh` | SHELL | (shell script — not executed by this audit; read its header) | positional args | n/a |
| `run_apply_symbols.sh` | SHELL | (shell script — not executed by this audit; read its header) | positional args | n/a |
| `search-string.sh` | SHELL | (shell script — not executed by this audit; read its header) | positional args | n/a |
| `struct_check.py` | WORKING | Compare C++ header struct layouts against Ghidra's inferred layouts. Uses the local struct_db.sqlite (built from annotated headers) as our source | --help | n/a |
| `test-hardening.sh` | SHELL | (shell script — not executed by this audit; read its header) | positional args | n/a |
