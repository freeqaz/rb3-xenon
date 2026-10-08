# rb3-xenon — Claude Context

Core rules only. Each section names the doc that carries the reasoning, the verified
failure modes and the measured incidents — read it before deviating, debugging in that
area, or citing a number. The full pre-2026-10-08 text of this file (140 KB, every dated
measurement and inline correction) is frozen at
`docs/decomp/CLAUDE_MD_FULL_RECORD_2026-10-08.md`. `docs/INDEX.md` indexes the core docs
(banner audit 2026-07-06) but not most lane docs written since, so also search `docs/decomp/`
by lane ID or topic. Any number quoted here is a snapshot: re-measure before you build on it.

**Document your work as you go.** Say what you are about to do, what you expect, and what you
measured — predictions before results; a failed prediction is the most useful line in a record.
Quote numbers with how they were measured (which leg, which build, which ruler). Say what you
deliberately did not do. A subagent's final report is the only thing the coordinator sees, so
put the evidence in it. Keep it proportional: a sentence or two per step.

## What this is

Decompilation of **Rock Band 3** for **Xbox 360** (PowerPC): matching retail machine code
from C++ source. Title ID `45410914`, target `orig/45410914/default.xex` (not committed).

- **Target image is CLEAN retail TU5** (since 2026-10-06; sha1 `d56e7f31…` in
  `config/45410914/build.sha1`; the `CHECK TARGET IMAGE` edge fails the build before the split
  if that one file hashes to anything else). The previous target, RB3 Deluxe, is archived at
  `orig/45410914/rb3dx-archive/`. ⚠ `orig/45410914/default_tu5.xex` holds **Deluxe** bytes
  despite its name and is the file the Ghidra program `default_tu5.xex-c5a170` was imported
  from: leave it in place (moving it imports a duplicate program). That program is wrong only
  in 53 words in 10 groups at the same VAs, usable everywhere else. Any raw address in a doc
  dated before 2026-07-15 is a **TU0** address and is invalid here.
  Record: `docs/decomp/W16PT_CLEAN_TU5_RETARGET_2026-10-06.md`.
- **Retail bytes:** `orig/45410914/band.exe` is the decompressed clean-TU5 PE (image base
  `0x82000000`); read any VA with `python3 tools/va_disasm.py <hexva> [nbytes]`. Prefer it
  over the Ghidra program and over any map-derived artifact. RB3 has **no linker map**:
  retail functions are anonymous `fn_8XXXXXXX` until named in
  `scripts/target_symbol_map.json`. `ham_xbox_r.map` is DC3's.
- **Compiler and flags are settled.** cl **10224**; `/GR`, synchronous `/EHs`, no LTCG,
  `/GS` off for Harmonix code, and linker ICF **on** (relocation-restricted COMDAT folding)
  are verified on retail bytes; `/O1 /Oi` and the implicit `/fp:fast` by whole-binary A/B
  (every perturbation scored worse); the `c` in `/EHsc` rests on that metric alone. Per-TU
  overrides live in `objects.json` `extra_cflags` (Quazal `/Od /Oi- /Ob1`, mostly `/EHs-c-`;
  `keygen_xbox` `/Od /Os`; curl `/TC /GS`). Do not re-litigate from match-%: `/GR` and ICF
  cannot be settled by that instrument. `docs/decomp/xdk-11164-compiler.md`,
  `docs/decomp/W16NT_PER_TU_FLAG_AUDIT_2026-10-02.md`.
- **Priority is native-port relevance:** RB3's game layer (`src/band3/`) plus the
  `src/system/` pieces the native runtime consumes, with the VIA-DC3 engine ring funded
  alongside (user, 2026-10-06; engine fixes may land in `../milo-native-engine`). DC3 supplies
  the renderer, materials and mesh as-is (`docs/plans/engine-reuse-and-asset-rendering.md`).
  XDK porting is out of scope (pinning and an allocator/memory-management subset are not).
  All of `src/network/` (Quazal and the `net/` layer over it) is out of scope for the native
  port: landed work stays, fund no new lanes there. Tiers: `scripts/native_scope_map.py`;
  current ranking: newest `docs/decomp/CAMPAIGN_STATE_*.md`.

## Source provenance

- `src/system/` (Milo engine), `src/xdk/`, `src/system/stlport/`, build tooling ⟵
  `../dc3-decomp` (same engine, already 360-ported, same cflags, named functions from its
  leaked map). The compiler is **not** shared — we build with cl 10224, dc3 with 11886 against
  a dev image — so its codegen mechanisms transfer and its per-function numbers do not.
- `src/band3/`, `src/network/` ⟵ `../rb3` (rb3-Wii **dev** decomp: keeps `MILO_ASSERT`
  path strings and function names retail stripped; MWCC, needs Wii→360 porting).
- dc3 is **newer** than RB3; when a file misbehaves, cross-check rb3-Wii and merge intent.
  Retail bytes outrank either oracle, and the oracle has been the defect more than once.
  Write authoritative comments — never "rb3-Wii body".
- `MILO_DEBUG` is force-defined tree-wide (`src/macros.h`) and does **not** gate
  `MILO_ASSERT`; `HX_NATIVE` does, and the match build never defines it, so
  `MILO_ASSERT(c, l)` is `((void)(c))` and a side-effecting condition still emits its call.
  The force-define's only effect is to switch on rb3-Wii dev-build code retail compiled out,
  so every `#ifdef MILO_DEBUG` a port brings in is suspect. Fix per site with
  `#if defined(MILO_DEBUG) && defined(HX_NATIVE)`; never blanket-remove (control −21).
  `docs/decomp/patterns/milo-debug-force-define.md` (its "no `/D` at all" line is stale).
- Per-TU `/D` gates churn (a dozen-odd; count them with a whitespace-delimited
  `' /D[A-Za-z_][A-Za-z0-9_]*'` over `build.ninja` — a bare `/D…` regex matches inside paths,
  which is how the count was wrong twice). None is `HX_NATIVE`. Gates rot (`RB3_MAP_0x1C`
  went live-in-6 to dead-in-all within two months), so probe liveness with
  `tools/gate_liveness.py` rather than inheriting it; a gate tested inside the PCH closure
  does nothing in a PCH-dir TU.

## Git and worktrees (other agents are in the main tree right now)

- **Never `git stash`, `git checkout`/`restore`/`reset --hard` files, or switch branches in
  the main repo.** Do isolated work in a worktree.
- **`scripts/setup_worktree.sh ~/tmp/wt-<name> <branch>`** makes a buildable worktree via
  btrfs reflinks (`--cold-cache` for a clean A/B baseline — ⚠ its prime currently fails with
  a false `THE SPLIT REWROTE ITS OWN INPUT`; re-run `./tools/ninja-locked
  build/45410914/config.json` and commit nothing). A bare `git worktree add` is unbuildable
  here. Worktrees and scratch go under **`~/tmp`, never `/tmp`** (tmpfs, no reflink, quota).
  Every worktree carries an intentional ` M tools/download_tool.py`; never commit it.
- **A fresh worktree's target objs are pre-renamer: every retail mangled name reads "absent"
  until the first build.** Build before any name-keyed analysis and sanity-check the symbol
  count.
- **Land with `git merge --no-ff`** after rebasing onto `main`, with a real merge message
  (goal, findings, what was not done). Never cherry-pick, squash or `--ff-only` — the
  intermediate commits are the point; the one exception is salvaging a single commit from an
  abandoned lane (say so). Never rebase a branch someone is still committing to.
  `scripts/harvest/land.sh <wt|branch>` only **rebases** (auto-unions the map, `objects.json`
  and `splits.txt`, prints READY/DEFER); merge `--no-ff` afterwards despite its header's
  `--ff-only`, and never let it union-resolve a `splits.txt` conflict when your patch deletes
  lines (the union drops deletions). Stage exact file lists; no AI co-author trailers;
  committing is standing-authorized, pushing is not.
- **Remove your worktree after landing with plain `git worktree remove <path>`** (no
  `--force`: plain `remove` refuses while untracked or modified files remain, and untracked
  files are often the deliverable). Or run `python3 tools/prune_worktrees.py` (dry-run), then
  `--execute --protect <wt>` for every live lane — its only other liveness guard is a 2 h mtime
  window. `worktree remove` does not delete the branch; only uncommitted work, or commits on
  a detached HEAD no ref reaches (orchestrator pool trees are detached), can be lost.
- A killed `--agent-tools` grind run leaves the worktree without `.git` (moved to
  `~/tmp/.<name>.gitmeta/gitfile`). Restore with `cp ~/tmp/.<name>.gitmeta/gitfile <wt>/.git
  && git worktree repair <wt>` before anything runs `git worktree prune` (the orchestrator
  pool does); once `.git/worktrees/<name>` is gone the tree is unrecoverable scratch. Never run
  two campaigns against one worktree.

## Building (X360 match build)

```bash
./tools/ninja-locked 2>&1 | tee ~/tmp/rb3_build_{task}.log   # never bare ninja; ALWAYS tee to ~/tmp
python3 configure.py                                         # after objects.json/splits.txt edits
```

- **Never bare `ninja`** (concurrent runs in one build dir corrupt `.ninja_deps`).
  ⛔ **Never `ninja <one>.obj` and never `objdiff-cli --build`** (same thing): the six obj
  patchers hang off the `post-compile` phony downstream of the compile edges, so a targeted
  build leaves unpatched objects that score lower with no timestamp change.
  `scripts/verify_objs_patched.py --check` asserts the tree is a fixed point;
  `scripts/orchestrator/patch_guard.py` is what every build-and-score tool goes through.
- **Split guard:** `config/45410914/symbols.txt` and the `.pdata` lines of `splits.txt` are
  committed fixed points. If a split rewrites either, the SPLIT edge fails with
  `THE SPLIT REWROTE ITS OWN INPUT`; the corrected file is already on disk — inspect it,
  commit it with your change, rebuild. A `.text` move must land together with the re-derived
  `.pdata` lines and the fixed-point `symbols.txt` (`scripts/verify_split_current.py`).
- Tools are **prebuilt local forks** resolved by absolute path, none on PATH: dtk =
  `../jeff/target/release/dtk`; objdiff = **`bin/objdiff-cli`** (the grader's 4.2.9 binary,
  shared by rb3 and dc3-decomp — a bare `objdiff-cli` on PATH is an older install); compiler
  wrapper = `../wibo` (live `build/release/wibo` = `1.2.0-c2rs.1`, built with the
  **`release64-clang`** preset and copied in; never build inside `build/release/`, it is the
  i686 `release` preset's binaryDir and a build there overwrites the live binary); object
  cache = `../objcache` (`~/.cache/rb3-objcache`, shared by every tree; bypass one command
  with `OBJCACHE=off` — `objcache off` flips the **shared** config for every lane).
- **Rebuilding any tool is a fleet deployment.** `cargo build --release` in `../jeff`,
  `../objdiff` or `../objcache` writes straight over the live binary every lane runs. Build
  with `CARGO_TARGET_DIR=~/tmp/...`, back up the live binary, swap deliberately, verify by
  sha256. wibo: build to staging, run the byte gate (≥3 TUs, same `/Fo`, 0 differing bytes
  beyond the COFF timestamp), then swap atomically. objcache does not key on the wibo binary,
  so a bad swap keeps serving old-loader objs: roll back the cache too. Full discipline: full
  record §"Two build tracks".
- **Raw `.obj` byte comparison is a dead instrument** (cache-served objs embed the populating
  worktree's `/Fo` and can carry its unattested `?A0x…` anon-namespace hash; PCH signature
  churn). Compare `report.json` keys or relocation-normalized body hashes, or use
  `tools/gate_liveness.py` (both legs `OBJCACHE=off`, same `/Fo`). Never key a tool on an
  exact unattested `?A0x` name.
- PCH: nine engine dirs compile through `build/45410914/pch/system.pch` from
  `src/system/decomp_pch.h` (Object.h + Debug.h only — **codegen-load-bearing, keep sacred**;
  an edit to anything it includes recompiles ~2,000 objs). `char rndobj world ui` are
  excluded on purpose (they regressed tracked matches), so never add a dir without a fresh A/B.
  Disable via `config.pch_eligible_dirs = set()` in `configure.py` + `python3 configure.py`.
- Expected split noise: the SPLIT step prints ~130 lines including ~36 dtk `WARN`. Tens of
  thousands of lines means an old dtk (`../jeff/target/release/dtk --version`). rb3 (Wii) must
  keep stock dtk 1.3.0.

## Build wiring

- `tools/defines_common.py` — include order: STLport, `src/xdk/LIBCMT`, `src`, `src/system`,
  then `src/band3`, `src/network`. Earlier roots shadow later ones (`net/JsonUtils.h` exists
  in both `system/` and `network/`), so never add stub headers at `src/` root.
- `config/45410914/objects.json` — which `.cpp` compile (new files as `NonMatching`). An entry
  gets a compile edge even with no `splits.txt` pin (compile-only scaffolding: port and
  compile a TU before its span is known; it scores nothing until pinned). A declared file whose
  `src_path` does not exist is dropped **silently** (about 240 units, nearly all `xdk/*`; do
  not stub them into compiling). ~145 TUs `#include` other `.cpp` files, so a `.cpp` absent
  from `objects.json` can still be compiled and one edit can move several units — find
  consumers with `ninja -t deps`, not `objects.json`.
- `config/45410914/splits.txt` — pin **`.text` only**; `.pdata` is re-derived on every split
  (never hand-edit it, but do commit what the split re-derives — see Split guard). Key every
  scan on the **full path**, never `basename()` (bare vs nested headings broke four lanes;
  `Movie.obj` collides). Draining a unit's last `.text` block must delete the whole entry or
  the build fails on a 42-byte obj — so a single-function unit can never be completed by a
  boundary move. Adding a pin over `auto_*` code is Δ0 on the matched keys (`total_code` may
  still move when the span held a phantom label row — a denominator correction);
  **re-homing an already-pinned address is not neutral**
  (`docs/decomp/pin-neutrality-scoped-2026-08-14.md`). `.s` address columns are synthetic for
  multi-block units — key on the `.fn fn_<addr>` symbol.
- **objdiff pairs target and base rows by NAME** (only EH funclets and `??__E`/`??__F` thunks
  fall back to byte-signature pairing), so a pinned function with no map entry, or whose base
  obj cannot define that name, reads 0% however correct the code is. That is why the renamer
  and the map exist and why re-homing is not neutral.
- **Obj patchers** (`scripts/`, wired in `configure.py`): pre-compile
  `obj_target_symbol_renamer` rewrites the target obj's `fn_<addr>` symbols from
  `scripts/target_symbol_map.json` (add rows by textual edit or
  `scripts/harvest/tu5_map_apply_fragment.py`, never `json.dump` the whole map;
  `tools/gen_game_target_map.py` is dead on TU5). **Changing or removing an existing name is
  inert until a re-split** (the renamer only rewrites `fn_` symbols):
  `rm build/45410914/target_symbol_renames.stamp && touch config/45410914/config.yml` first.
  Post-compile **six** passes — `anon_ns`, `dynamic_init`, `guard`, `bool_mangle`,
  `atexit_scope`, `eh_boundary`; by ablation only `anon_ns` and `guard` move the metric, the
  rest are correctness or idle (re-measure before quoting). `regswap` and `transplant` are
  bitrotted (DC3 title ID), not wired.
- Splits bootstrap for a new cluster: `tools/fingerprint_match.py autoid` → pin
  `[min(fn), max(fn)+size)` as `.text` → `touch config/45410914/config.yml &&
  ./tools/ninja-locked`. Details: full record §"Matching phase".

## Measurement — the rules that bite

- **Price any change with `python3 tools/ab_measure.py --worktree <wt> --from-dirty`**
  (or `--patch`/`--pick`/`--revert`; skill `/ab-measure`). It settles both legs to zero work,
  wipes report caches, forces and iterates re-splits for map/splits/symbols patches, hands
  the worktree back as found, and **refuses** (exit 2, no numbers) on a broken run — one
  change per run, no staged index. Never hand-run the checklist; an unsettled reading can flip
  sign. Under `--from-dirty` a dirty `symbols.txt` is restored as drift and not measured, so
  commit a deliberate carve and use `--pick`/`--patch`. If the run prints **LANDING**, commit
  its `legB_symbols_fixed_point.diff` / `legB_splits_rederived.diff` with the patch or main's
  next build fails the split guard. Deltas come only from legs measured in-run — deltas
  compose, absolutes do not.
- **`report.json` is the score.** It self-declares its ruler in `provenance.diff_config` (a
  list of `key=value` strings; `scripts/analysis/ruler.py` parses it). The shipped ruler is
  **`functionRelocDiffs=name_check`** (since 2026-08-12); `none` is opt-in and is **not** a
  control for either headline number (on the same objdiff binary it reads a different, moving
  function count). Any absolute recorded without its ruler is incomparable.
  `docs/decomp/RULER_CHANGE_name_check_2026-08-12.md` (its "ruler-invariant
  `matched_functions`" line is superseded by
  `docs/decomp/W16AR_AO_FILED_ROWS_RULER_INVARIANCE_2026-09-14.md` §3).
- **The two headline numbers use different rulers:** `matched_functions` counts rows at
  `match_percent_normalized == 100` (`mpn`, arg penalties excluded — except that under
  `name_check` a vetted relocation-name diff is charged, objdiff `b14ba45`); `matched_code`
  sums rows at `fuzzy_match_percent == 100`. A fix can move one and not the other. A wrong
  callee stays invisible to `mpn` only when its target is a placeholder, an alias, a regalloc
  save helper, a local-static ordinal, or inside an anonymous funclet — so a correctness fix
  can still read Δ0; land it anyway. About half of `matched_functions` are `masked_equal`
  funclet pairings; the honest floor is `matched − masked_equal`, and `ab_measure` prints
  Δhonest (never chain one from before the 2026-08-02 disclosure flip,
  `docs/decomp/RULER_CHANGE_2026-08-02.md`).
- `report.json` traps: byte and address numerics are JSON **strings** and zero-valued scalars
  are omitted (most units have no `matched_code`), so read `int(x.get(k, 0))`; but
  `match_percent_normalized` is emitted even at 0.0 — count how many rows lack a key before
  testing on its absence. `masked_equal` row flags are a superset of the measure. Read
  `total_code`/`total_functions` from the file — they move.
- **`bin/objdiff-cli diff` and the MCP `run_objdiff` agree with `report.json`** (four config
  keys pinned in `objdiff.json`; `scripts/verify_ruler_agreement.py --check`). But their
  `Match:` headline is **canonical = `mpn`**, the markdown header **rounds** (a row at fuzzy
  99.97 prints `100.0%`), and "N/N instructions equal" or a mismatch **count** excludes
  relocation-name charges. **Price a row from its charged sites in
  `bin/objdiff-cli diff -f json --include-instructions` at the graded ruler, counting every
  `diff_arg`, then confirm bytes with `report.json`'s `fuzzy_match_percent`.** Never read a
  crossing off a printed 100.
  `docs/decomp/patterns/two-objdiff-entry-points-two-rulers.md`.
- `name_check` **forgives** every relocation whose retail target is a splitter placeholder
  (`fn_`/`lbl_`/`jumptable_`/`data_`/`bss_`/`rdata_`…), so a fuzzy-100 row can still call the
  wrong unnamed callee or load a wrong `lbl_` constant (`tools/const_value_audit.py`). A
  charged callee our map cannot place is **unidentified, not proven folded** — a triage
  backlog, never noise or an alias candidate.
- Verdict labels: since objdiff `7a334d4` (2026-08-21) `LINKER_MERGED` fires only on a
  splitter merged name; two different named callees are `WRONG_CALLEE` (fixable). An
  `AT_LIMIT`/ICF label on a relocation-name-only row from an older brief carries no
  information — adjudicate on retail bytes (does the named callee's signature fit the call
  site?). `REGISTER_SWAP` on a sub-100 row is a symptom, not a diagnosis.
- **Aliases** (`scripts/symbol_aliases.json`, ~2,200 groups) forgive a large share of
  `matched_code` by construction — the last two ablations read ~8 pp (09-01) and ~18 pp
  (10-08) of `total_code`; re-ablate rather than quote. Admit an alias only on retail-byte
  proof of the fold (`tools/icf_pair_adjudicate.py` PROVEN), never because a validator prints
  `OK (MAP-CONSISTENT)`. Never merge `scripts/icf_alias_groups.json` into it. A map-only
  patch that moves `name_check` while `none` stays flat is alias-suspect (`ab_measure` fires
  `ALIAS_SUSPECT`). Do not prune classes that forgive 0 today.
- **Naming is a bet.** Naming an anonymous address pays bytes only when the name lets the
  retail row pair with a body its unit obj compiles; otherwise it turns forgiven placeholder
  sites into checked ones, and its payout is bug exposure. Repairing a wrong name pays only if
  the base obj defines the new name, else the row goes permanently 0%. A fabricated alias
  lifts the score and is a hazard.
- **The reachable ceiling is not 100% and it moves both ways.** Re-measure with
  `python3 tools/ceiling_recompute.py build/45410914/report.json objdiff.json . "<label>"`
  (four positional args; keep its `≤6` scaffold rule). Never inherit a prior lane's ceiling or
  share. `matched_code` is all-or-nothing per row, so rank candidates by size-if-it-crosses;
  `report.json` sizes can bill a 12 B body as 8,852 B — price from the asm extent. Accuracy
  beats headline %: a drop from a truer denominator is a win.

## Native build and gate

```bash
cd native && cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build
```

- The shared native runtime is `../milo-native-engine`, consumed as a live working tree (an
  engine edit reaches every consumer on its next configure). `MILO_ENGINE_PIN` in
  `native/CMakeLists.txt` is a warn-only SHA and a CMake cache variable (an existing build dir
  keeps the old value; reconfigure with `-UMILO_ENGINE_PIN` before reading a mismatch as
  drift). Only the coordinator bumps it. Siblings resolve from the real repo, so a cold
  worktree gates 18/18 without seeding. Lessons: memory `hub_native.md`, `docs/plans/x*.md`.
- **Run `tools/native_build_gate.sh` before landing anything under `src/`** as your **last**
  action (a comment-only commit once broke the link). Require `PASS 18/18, 0 SKIPs, rc=0` and
  paste the `NATIVE_GATE_RESULT …` line verbatim. rc: 0 pass · 1 broken · 2 could not run ·
  3 ran but incomplete/partial. It also runs `tools/layout_odr.py` (cross-TU class-layout ODR
  over every X360 TU and native program; a cold run after a wide header change takes ~35 min,
  do not kill it — `docs/decomp/W16QD_LAYOUT_ODR_CHECK_2026-10-06.md`). The gate proves the
  targets link, not that they run: for shared `src/`, `docs/decomp/NATIVE_HEALTH.md` also
  requires `tools/native_health.sh --selftest` (all 18 targets on real data); paste its
  `NATIVE_HEALTH_RESULT` line too. If you seed a configure by hand, pin the compiler or CMake
  wipes the cache.
- **Never put render or gate flags in a shell variable** — zsh does not word-split, the tool
  silently renders defaults and returns 0. Spell the flags out or use an array, and confirm
  the log names the cell or venue you asked for; `rc=0` alone proves nothing.

## Shell and instrument traps

- **The Bash tool runs zsh.** It never word-splits `$VAR`; it applies history modifiers inside
  double quotes, so `"$ref:config/…"` loses the `:c` — write `"${ref}:path"`; an unquoted glob
  with no match aborts the command (behind a pipe that reads as rc 0 or a count of 0), so quote
  globs.
- **The Claude shell's `grep` is a `ugrep -I` function: binary-blind, false negatives only**
  (`cat bin | grep` too). Pass `-a`, use `command grep -a`, or scan in Python, and put `-a` in
  any recipe you write into a doc. Guard: `python3 tools/grep_binary_guard.py`.
- `cmd | tail; echo "rc=$?"` reads tail's exit code. Redirect to a file and test `$?` next.
- A screen that cannot fail returns a clean, decisive-looking result. Before trusting a
  negative, show the instrument can discriminate (a control that fails, a count that moves).
  A reproducing count is not evidence for its mechanism.
- **Class layout: ask the compiler, not the comments.**
  `scripts/harvest/class_layout_report.py <Class>` (`/d1reportSingleClassLayout`; skill
  `/struct-info`) is authoritative; `// 0xHEX` comments and `struct_db.sqlite` are derived and
  have been wrong. `--check-header` audits a header.
- Declaration order controls stack slots; for registers it is inert unless the allocator has
  slack (this corrects `docs/decomp/MSVC_X360_REGALLOC.md`'s "declaration order controls
  allocation", which the file does not flag). Registers follow liveness and scheduling: read
  `docs/decomp/patterns/fixable-liveness.md` and its Triage Split before opening a
  register-swap residual. Permuter is off by directive (deferred until after the ceiling).

## Tooling

- **Orchestrator MCP** (`.mcp.json` key `orchestrator`, tools appear as
  `mcp__orchestrator__*`; `scripts/orchestrator/`, DB `decomp.db`): `query_functions`,
  `run_objdiff`, `run_diff_inspect`, `lookup_dc3`, `lookup_rb3wii`, `lookup_struct_offset`,
  `report_result`, … The build-and-score tools **refuse** without `project_dir=<worktree>`
  and refuse main unless the server was launched with `RB3_MCP_ALLOW_MAIN=1`.
  `lookup_merged_symbol` always fails here (needs a map; for ICF folds read
  `scripts/symbol_aliases.json` / `build/45410914/icf_aliases.map`). No build step refreshes
  `decomp.db`, so `query_functions` percents trail `report.json`
  (`venv/bin/python tools/refresh_permuter_db.py` refreshes them).
- **Ghidra MCP** on port **8002** (`tools/ghidra/pyghidra-service.sh`, project
  `ghidra_projects/RB3Xenon/RB3Xenon/RB3Xenon.gpr` — the top-level `RB3Xenon.gpr` is a stale
  May copy; Xbox 360 Ghidra fork 12.3_DEV since 2026-10-02,
  `docs/plans/ghidra-install-swap-2026-10-02.md`). pyghidra caches decompiles in the repo-root
  `cache.db` keyed by binary hash (`--reinit` does not clear it); move it aside after any
  Ghidra change. BinDiff at `/usr/bin/bindiff`, one BinExport copy per Ghidra version only.
- **Skills** (`.claude/skills/`): `recon`, `compare-asm`, `stack-layout`, `vtable`,
  `data-diff`, `struct-info`, `dc3-pair` (engine oracle), `rb3wii-pair` (game oracle),
  `ab-measure`, `progress`, `batch-check`. Dated campaign workflows (bodyport/grind/permuter
  sweeps) live in `.claude/workflows/`; check them against current directives before running.
- objdiff pattern-doc links resolve against this repo as `DocProject::Dc3`; verify doc renames
  with `python3 ../objdiff/scripts/check_doc_links.py --dc3 . --allow-missing` (baseline is
  rc=1 with 2 known failures on `relocation-names-are-unmetered.md`, which exists only in
  dc3-decomp; a rename must not add failures). A link fix in `../objdiff` changes nothing
  until the binary is rebuilt.
- MSVC pattern docs: `docs/decomp/patterns/`, `docs/decomp/MSVC_X360_REGALLOC.md`,
  `docs/decomp/TECHNICAL_NOTES.md`, `docs/decomp/XBOX360_FLOATING_POINT_CODEGEN.md`. Tool
  inventory: `docs/decomp/TOOLING.md` (counts stale since 07-29).

## Memory and state

Memory lives at `~/.claude/projects/-home-free-code-milohax-rb3-xenon/memory/` (hub-and-spoke;
start at `MEMORY.md`). For current numbers read the newest `docs/decomp/CAMPAIGN_STATE_<date>.md`
(⚠ plain `ls` sorts `10-07.md` after `10-07d.md`); the roadmap memory's top CURRENT STATE block
was measured on the Deluxe image before the 10-06 retarget, so its absolutes do not compose
with today's. Per-lane plans are dated docs under `docs/plans/`.
