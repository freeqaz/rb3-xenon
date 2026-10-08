# rb3-xenon — Claude Context

Core rules only. Each section names the doc that carries the reasoning, the verified
failure modes and the measured incidents — read it before deviating, debugging in that
area, or citing a number. The full pre-2026-10-08 text of this file (140 KB, with every
dated measurement and correction) is frozen at
`docs/decomp/CLAUDE_MD_FULL_RECORD_2026-10-08.md`; `docs/INDEX.md` is the audited index of
everything under `docs/`.

**Document your work as you go.** Say what you are about to do, what you expect, and what you
measured — predictions before results. Quote numbers with how they were measured (which leg,
which build, which ruler). Say what you deliberately did not do. A subagent's final report is
the only thing the coordinator sees, so put the evidence in it.

## What this is

Decompilation of **Rock Band 3** for **Xbox 360** (PowerPC): matching retail machine code
from C++ source. Title ID `45410914`, target `orig/45410914/default.xex` (not committed).

- **Target image is CLEAN retail TU5** (since 2026-10-06; sha1 `d56e7f31…`, recorded in
  `config/45410914/build.sha1`; the `CHECK TARGET IMAGE` edge fails the build if `orig/` holds
  anything else). The previous target, RB3 Deluxe, is archived at `orig/45410914/rb3dx-archive/`
  and the Ghidra program `default_tu5.xex-c5a170` still holds its bytes. Record:
  `docs/decomp/W16PT_CLEAN_TU5_RETARGET_2026-10-06.md`.
- **Compiler and flags are SETTLED, verified on retail bytes:** cl **10224**,
  `/O1 /Oi /GR /EHsc` (+ implicit `/fp:fast`), no LTCG, `/GS` off, linker ICF **on**
  (relocation-restricted COMDAT folding). `/Od` is object-granular (Quazal, `keygen_xbox`).
  Do not re-litigate from match-%: `/GR` and ICF cannot be settled by that instrument.
  Evidence tables: full record §"Optimization level"; `docs/decomp/xdk-11164-compiler.md`.
- **Priority is the GAME layer** (`src/band3/`, `src/network/`), not the Milo engine
  (`src/system/`): DC3 is the same engine on the same platform and renders RB3 assets as-is.
  `docs/plans/engine-reuse-and-asset-rendering.md`. The native port is the real goal; XDK
  porting and Quazal RE are out of scope (pinning is not).

## Source provenance

- `src/system/` (Milo engine), `src/xdk/`, `src/system/stlport/`, toolchain ⟵ `../dc3-decomp`
  (same engine, already 360-ported, same flags, named functions from its leaked map).
- `src/band3/`, `src/network/` ⟵ `../rb3` (rb3-Wii **dev** decomp: richer oracle, MWCC,
  needs Wii→360 porting).
- dc3 is **newer** than RB3; when a file misbehaves, cross-check rb3-Wii and merge intent.
  Retail bytes outrank either oracle. Write authoritative comments — never "rb3-Wii body".
- `MILO_DEBUG` is force-defined tree-wide (`src/macros.h`) and does **not** gate
  `MILO_ASSERT`; `HX_NATIVE` does, and the match build never defines it. Fix per site with
  `#if defined(MILO_DEBUG) && defined(HX_NATIVE)`; never blanket-remove (control −21).
  Cflags carry eleven distinct `/D` flags — census in
  `docs/decomp/patterns/milo-debug-force-define.md`.

## Git and worktrees (other agents are in the main tree right now)

- **Never `git stash`, `git checkout`/`restore`/`reset --hard` files, or switch branches in
  the main repo.** Do isolated work in a worktree.
- **`scripts/setup_worktree.sh ~/tmp/wt-<name> <branch>`** makes a buildable worktree via
  btrfs reflinks (`--cold-cache` for a clean A/B baseline). A bare `git worktree add` is
  unbuildable here. Worktrees and scratch go under **`~/tmp`, never `/tmp`** (tmpfs, no
  reflink, quota).
- **A fresh worktree's target objs are pre-renamer: every retail mangled name reads "absent"
  until the first build.** Build before any name-keyed analysis and sanity-check the symbol
  count.
- **Land with `git merge --no-ff`** after rebasing onto `main`, with a real merge message
  (goal, findings, what was not done). Never cherry-pick, squash or `--ff-only` — the
  intermediate commits are the point. Stage exact file lists; no AI co-author trailers;
  committing is standing-authorized, pushing is not.
- **Remove your worktree after landing** (`git worktree remove --force`), or run
  `python3 tools/prune_worktrees.py [--execute]`. `worktree remove` does not delete the
  branch; only *uncommitted* work can be lost, and untracked files are often the deliverable.
- A killed `--agent-tools` grind run leaves the worktree without `.git` (moved to
  `~/tmp/.<name>.gitmeta/gitfile`); restore it and `git worktree repair` before git prunes
  it. Never run two campaigns against one worktree.

## Building (X360 match build)

```bash
./tools/ninja-locked 2>&1 | tee ~/tmp/rb3_build_{task}.log   # ALWAYS tee to ~/tmp
python3 configure.py                                         # after objects.json/splits.txt edits
```

- ⛔ **Never `ninja <one>.obj` and never `objdiff-cli --build`** (same thing). The six obj
  patchers hang off the `post-compile` phony downstream of the compile edges, so a targeted
  build leaves unpatched objects that score lower with no timestamp change (measured −2 pp
  on a unit). `scripts/verify_objs_patched.py --check` asserts the tree is a fixed point;
  `scripts/orchestrator/patch_guard.py` is what every build-and-score tool goes through.
- Tools are **prebuilt local forks** resolved by absolute path: dtk = `../jeff`, objdiff =
  `../objdiff` (one binary symlinked into rb3, dc3-decomp and here), compiler wrapper =
  `../wibo` (`build/release/wibo`, built with the **`release64`** preset — `release` is i686),
  object cache = `../objcache` (`~/.cache/rb3-objcache`; `objcache off` to bypass).
- **Rebuilding a tool is a fleet deployment.** `cargo build --release` in `../jeff` overwrites
  the live dtk for every lane; build with `CARGO_TARGET_DIR=~/tmp/...`, back up the live
  binary, swap deliberately, verify by sha256. wibo: build to staging, run the byte gate
  (≥3 TUs, same `/Fo`, 0 differing bytes beyond the COFF timestamp), then swap atomically.
  Full discipline: full record §"Two build tracks".
- **Raw `.obj` byte comparison is a dead instrument** (cache-served objs embed the populating
  worktree's `/Fo`; PCH signature churn). Compare `report.json` keys or relocation-normalized
  body hashes, or use `tools/gate_liveness.py` (both legs `OBJCACHE=off`, same `/Fo`).
- PCH: nine engine dirs compile through `build/45410914/pch/system.pch` from
  `src/system/decomp_pch.h` (Object.h + Debug.h only — **codegen-load-bearing, keep sacred**).
  Disable via `config.pch_eligible_dirs = set()` in `configure.py`.
- Expected split noise: ~114 lines on a fresh `ninja`, ~30 dtk `WARN` lines. Tens of thousands
  of lines means an old dtk (`dtk --version`). `config/45410914/symbols.txt` is a committed fixed point; drift
  is a real change. rb3 (Wii) must keep stock dtk 1.3.0.

## Build wiring

- `tools/defines_common.py` — include order: STLport, `src/xdk/LIBCMT`, `src`, `src/system`.
  `/I src` precedes `/I src/system`, so never add stub headers at `src/` root.
- `config/45410914/objects.json` — which `.cpp` compile (new files as `NonMatching`). A
  declared file whose `src_path` does not exist is dropped **silently** (230 such units, 229
  `xdk/*`; do not stub them into compiling).
- `config/45410914/splits.txt` — pin **`.text` only**; `.pdata` is re-derived on every split
  (never hand-edit it). Key every scan on the **full path**, never `basename()` (bare vs
  nested headings broke four lanes; `Movie.obj` collides). Draining a unit's last `.text`
  block must delete the whole entry or the build fails on a 42-byte obj — so a
  single-function unit can never be completed by a boundary move. Adding a pin over `auto_*`
  code is metric-neutral; **re-homing an already-pinned address is not**
  (`docs/decomp/pin-neutrality-scoped-2026-08-14.md`). `.s` address columns are synthetic for
  multi-block units — key on the `.fn fn_<addr>` symbol.
- **Obj patchers** (`scripts/`, wired in `configure.py`): pre-compile
  `obj_target_symbol_renamer` (map `scripts/target_symbol_map.json`, game rows from
  `tools/gen_game_target_map.py`); post-compile **six** passes — `anon_ns`, `dynamic_init`,
  `guard`, `bool_mangle`, `atexit_scope`, `eh_boundary`. Only `anon_ns` moves the metric
  (ablation −82 fns / −14,604 B); the rest are correctness. `regswap` and `transplant` are
  bitrotted (DC3 title ID), not wired.
- Splits bootstrap for a new cluster: `tools/fingerprint_match.py autoid` → pin
  `[min(fn), max(fn)+size)` as `.text` → `touch config/45410914/config.yml && ninja`.
  Details: full record §"Matching phase".

## Measurement — the rules that bite

- **Price any change with `python3 tools/ab_measure.py --worktree <wt> --from-dirty`**
  (or `--patch`/`--pick`/`--revert`; skill `/ab-measure`). It settles both legs to zero work,
  wipes report caches, forces and iterates re-splits for map/splits/symbols patches, hands
  the worktree back as found, and **refuses** (exit 2, no numbers) on a broken run. Never
  hand-run the checklist; an unsettled reading can flip sign. Deltas come only from legs
  measured in-run — deltas compose, absolutes do not.
- **`report.json` is the score.** It self-declares its ruler in `provenance.diff_config`.
  The shipped ruler is **`functionRelocDiffs=name_check`** (since 2026-08-12); `none` is
  opt-in and is **not** a control for either headline number (objdiff 4.2.9: `none` reads
  +1,651 `matched_functions`). Any absolute recorded without its ruler is incomparable.
  `docs/decomp/RULER_CHANGE_2026-08-02.md`,
  `docs/decomp/W16AR_AO_FILED_ROWS_RULER_INVARIANCE_2026-09-14.md`.
- **The two headline numbers use different rulers:** `matched_functions` counts rows at
  `match_percent_normalized == 100` (arg penalties excluded); `matched_code` sums rows at
  `fuzzy_match_percent == 100`. A fix can move one and not the other; a wrong callee or wrong
  container type can read 100 on `mpn` before and after — land correctness fixes at Δ0.
- `report.json` traps: several numerics are JSON **strings** (`int()`-coerce);
  `fuzzy_match_percent` is omitted when 0 but `match_percent_normalized` is emitted even at
  0.0 — count how many rows lack a key before testing on its absence; `masked_equal` row flags
  are a superset of the measure. Read `total_code`/`total_functions` from the file — they move.
- **objdiff's `run_objdiff` and `objdiff-cli diff` now agree with `report.json`** (all four
  divergent config keys are pinned in `objdiff.json`; guard
  `scripts/verify_ruler_agreement.py --check`). But "N/N instructions equal" and a mismatch
  **count** exclude relocation-name charges: **price a row from `report.json`'s charged-site
  list**, not from an instruction count.
  `docs/decomp/patterns/two-objdiff-entry-points-two-rulers.md`.
- **An `AT_LIMIT` / `LINKER_MERGED` verdict on a row whose only charges are relocation names
  carries no information** — it is the definition of a wrong callee restated. Adjudicate on
  retail bytes (does the named callee's signature fit the call site?). Likewise
  `REGISTER_SWAP` on a sub-100 row is a symptom, not a diagnosis.
- **Aliases** (`scripts/symbol_aliases.json`, ~1,500 groups) forgive ~7.9 pp of `matched_code` by
  construction. A map-only patch that moves `name_check` while `none` stays flat is
  alias-suspect; `ab_measure` fires `ALIAS_SUSPECT` for exactly that. Size the mechanism by
  ablation, never by a name-keyed census. Do not prune classes that forgive 0 today.
- **Naming** an anonymous address pays in bug exposure, not bytes; a wrong name is charged.
  Repairing a wrong existing name pays. A fabricated alias lifts the score and is a hazard.
- **The reachable ceiling is not 100% and it moves both ways.** Re-measure with
  `python3 tools/ceiling_recompute.py build/45410914/report.json objdiff.json . "<label>"`
  (four positional args; keep its `≤6` scaffold rule). Latest:
  `docs/decomp/CAMPAIGN_STATE_2026-10-07d.md`. Never inherit a prior lane's ceiling or share.
- Reading disagreements: `matched_code` is all-or-nothing per row, so rank candidates by
  size-if-it-crosses; `report.json` sizes can bill a 12 B body as 8,852 B — price from the
  asm extent. Accuracy beats headline %: a drop from a truer denominator is a win.

## Native build and gate

```bash
cd native && cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build
```

- The shared native runtime is `../milo-native-engine`, pinned by `MILO_ENGINE_PIN` in
  `native/CMakeLists.txt`; only the coordinator bumps the pin. Siblings resolve from the real
  repo, so a cold worktree gates 18/18 without seeding. Lessons: memory `project_native_port.md`,
  `docs/plans/x*.md`.
- **Run `tools/native_build_gate.sh` before landing anything under `src/system/**`,
  `src/band3/**` or a shared header**, as your **last** action (a comment-only commit once
  broke the link). Require `PASS 18/18, 0 SKIPs, rc=0` and paste the
  `NATIVE_GATE_RESULT …` line verbatim. rc: 0 pass · 1 broken · 2 could not run · 3 ran but
  incomplete/partial. If you seed a configure by hand, pin the compiler or CMake wipes the
  cache.
- **Never put render or gate flags in a shell variable** — zsh does not word-split, the tool
  silently renders defaults and returns 0. Spell the flags out or use an array.

## Shell and instrument traps

- **The Claude shell's `grep` is a `ugrep -I` function: binary-blind, false negatives only.**
  Pass `-a`, use `command grep -a`, or scan in Python. Guard:
  `python3 tools/grep_binary_guard.py`.
- `cmd | tail; echo "rc=$?"` reads tail's exit code. Redirect to a file and test `$?` next.
- A screen that cannot fail returns a clean, decisive-looking result. Before trusting a
  negative, show the instrument can discriminate (a control that fails, a count that moves).
  A reproducing count is not evidence for its mechanism.
- **Class layout: ask the compiler, not the comments.**
  `scripts/harvest/class_layout_report.py <Class>` (`/d1reportSingleClassLayout`) is
  authoritative; `// 0xHEX` comments and `struct_db.sqlite` are derived and have been wrong.
  `--check-header` audits a header.
- Declaration order controls stack slots, not registers; read
  `docs/decomp/patterns/fixable-liveness.md` before opening a register-swap residual.
  Permuter is off by directive (deferred until after the ceiling).

## Tooling

- **Orchestrator MCP** (`.mcp.json` → `scripts/orchestrator/`, server `decomp`, DB `decomp.db`
  from `scripts/ingest_report.py`): `query_functions`, `run_objdiff`, `run_diff_inspect`,
  `lookup_dc3`, `lookup_rb3wii`, `lookup_struct_offset`, `lookup_merged_symbol`,
  `report_result`, … Pass `project_dir=<worktree>` or it measures main.
- **Ghidra MCP** on port **8002** (`tools/ghidra/pyghidra-service.sh`, project
  `ghidra_projects/RB3Xenon`, Xbox 360 Ghidra fork 12.3_DEV since 2026-10-02 —
  `docs/plans/ghidra-install-swap-2026-10-02.md`). pyghidra caches decompiles in `cache.db`
  keyed by binary hash; move it aside after any Ghidra change. BinDiff at `/usr/bin/bindiff`,
  one BinExport copy per Ghidra version only.
- **Skills** (`.claude/skills/`): `recon`, `compare-asm`, `stack-layout`, `vtable`,
  `data-diff`, `dc3-pair` (engine oracle), `rb3wii-pair` (game oracle), `ab-measure`,
  `progress`, `batch-check`, and the bodyport/grind campaign skills.
- objdiff pattern-doc links resolve against this repo as `DocProject::Dc3`; verify doc renames
  with `python3 ../objdiff/scripts/check_doc_links.py --dc3 . --allow-missing`.
- MSVC pattern docs: `docs/decomp/patterns/`, `docs/decomp/MSVC_X360_REGALLOC.md`,
  `docs/decomp/TECHNICAL_NOTES.md`, `docs/decomp/TOOLING.md`.

## Memory and state

Memory lives at `~/.claude/projects/-home-free-code-milohax-rb3-xenon/memory/` (hub-and-spoke;
start at `project_rb3_xenon_roadmap.md`'s CURRENT STATE). Campaign state docs:
`docs/decomp/CAMPAIGN_STATE_<date>.md`. Live plan files: `~/.claude/plans/rb3-xenon-*.md`.
