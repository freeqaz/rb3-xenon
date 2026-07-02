# W2-A — Wiring window #2: land the verified PCH patch on main

Model: **opus**. Wave 2. The ONLY task in wave 2 allowed to mutate rb3-xenon main's build
wiring. Prerequisite: W1-C completed with all gates green and delivered a patch; W1-A's
window #1 is committed on main (verify: `git log --oneline -10 | grep -i "fork wibo\|transform_dep"`
and the msvc rule in build.ninja has no pipe).

## Hard rules

- NEVER `git stash` / `git checkout <file>` / `git restore` / `git reset --hard` in main.
- Build ONLY via `./tools/ninja-locked`, tee to `~/tmp/rb3_build_pchland_*.log`.
- Commits pathspec-only (`git commit -o tools/project.py configure.py CLAUDE.md`). Never
  sweep other agents' WIP (untracked `global_fuzzy_pairs.json`, `auto_*.obj`,
  `scripts/orchestrator/decomp.db`, etc.).
- Commit trailer: `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`.
- ABORT DISCIPLINE: any gate failure → restore main to pre-window state via `git revert` of
  your own commit (or, if not yet committed, re-edit the two files back by applying the
  reverse patch — never `git checkout` the files) → reconfigure → settle → report. Do not
  improvise fixes on main; failures go back through a worktree.

## Context

W1-C produced and fully verified (3 gates: section-byte, objdiff, whole-binary
matched_functions equality) a patch adding dc3-style PCH to the build: 4 edits in
tools/project.py (config fields, msvc_pch_create/msvc_pch rules built by `.replace()` on
msvc_cmd with anchor `"$cflags /showIncludes /Fo$out $in"`, the PCH build edge emitting
build/45410914/pch/system.pch, the per-object eligibility switch) + 1 edit in configure.py
(pch_header/pch_source/pch_eligible_dirs = the 13 engine dir basenames dc3 byte-verified).
Full details + rationale: docs/plans/buildspeed/03-pch-verify.md and W1-C's report. The PCH
boundary files src/system/decomp_pch.h/.cpp already exist and are git-tracked; objects.json
is untouched.

What landing costs: the ~471 eligible TUs switch rule msvc→msvc_pch (their command hashes
change) + the one PCH-create edge → ninja recompiles those once. The base msvc rule string is
UNCHANGED by this patch, so the other ~274 TUs do not recompile. With FS_CACHE active
(window #1) this settle is fast.

## Steps

1. Preflight:
   ```bash
   cd /home/free/code/milohax/rb3-xenon
   git log --oneline -3 && git status --short          # record HEAD; W1-A commit must be present
   python3 -c "import json;print(json.load(open('build/45410914/report.json'))['measures']['matched_functions'])"   # M0
   ```
   Confirm the working tree has NO modifications to tools/project.py or configure.py (another
   agent mid-edit = STOP and report).
2. Apply W1-C's patch: `git apply --check <patch> && git apply <patch>` (or `git am` if it is
   a format-patch and you want its message — prefer your own commit message below). If
   `--check` fails, rebase the patch by hand using 03-pch-verify.md's exact edits (they are
   anchor-based) and note the drift in your report.
3. Reconfigure + inspect BEFORE building:
   ```bash
   python3 configure.py 2>&1 | tee ~/tmp/rb3_configure_pchland.log
   grep -n '/Yc"decomp_pch.h"' build.ninja | head -2     # create rule present
   grep -n '/Yu"decomp_pch.h"' build.ninja | head -2     # use rule present
   grep -c ': msvc_pch ' build.ninja                      # ≈471 eligible TU edges
   grep -n 'build build/45410914/pch/decomp_pch.obj' build.ninja
   ```
4. Settle: `./tools/ninja-locked 2>&1 | tee ~/tmp/rb3_build_pchland_settle.log`. Confirm
   build/45410914/pch/system.pch produced; eligible TUs recompiled; second
   `./tools/ninja-locked` = 0 recompiles.
5. Gates on main (yes, again — main is a different tree state than W1-C's worktree):
   - **Report gate (EQUALITY):** matched_functions == M0. If other agents landed matches
     mid-window (HEAD moved), re-baseline against the exact pre-apply report and reason
     explicitly in the report. ANY unexplained decrease → ABORT (see discipline above).
   - **Spot objdiff gate:** run_objdiff (MCP, project_dir=/home/free/code/milohax/rb3-xenon)
     on ≥3 functions in eligible units W1-C used — match% unchanged vs W1-C's recorded values.
   - **Deps spot-check:** `ninja -t deps build/45410914/pch/decomp_pch.obj | head` is VALID and
     lists Object.h/Debug.h transitive headers; touch `src/system/os/Debug.h` → next build
     rebuilds the PCH AND eligible objs (then let it settle; this proves the staleness chain).
     Prefer doing this touch-test LAST and letting the settle complete before committing.
6. CLAUDE.md: add a short "PCH" note under Build wiring — eligible engine dirs compile through
   build/45410914/pch/system.pch; instant disable = `config.pch_eligible_dirs = set()` +
   reconfigure; decomp_pch.h is codegen-load-bearing (only Object.h+Debug.h, keep it sacred;
   native-only edits must be `#ifdef HX_NATIVE` per dc3 HEADER_REGRESSION_ANALYSIS.md §6.4).
7. Commit:
   ```bash
   git commit -o tools/project.py configure.py CLAUDE.md -m "build: PCH for 13 engine dirs (~471 TUs) — dc3 port, 3-gate verified

   msvc_pch_create//Yc + msvc_pch//Yu rules; system.pch edge; per-dir eligibility.
   Gates: .text section bytes identical, objdiff match% identical, whole-binary
   matched_functions equal (worktree W1-C + re-verified on main). Measured
   ~<W1-C number> s/TU on eligible units. Instant disable: empty
   config.pch_eligible_dirs + reconfigure.

   Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
   ```

## Acceptance criteria

1. build.ninja has both PCH rules + edge; ~471 msvc_pch edges; settle clean; second build 0
   recompiles.
2. matched_functions equality gate green on main.
3. PCH staleness chain proven (Debug.h touch test).
4. Committed pathspec-only; other agents' WIP untouched; CLAUDE.md updated.
5. Report includes: measured settle time, per-TU savings confirmation on main, final eligible
   dir set.

## Rollback

Tier 1 (instant, no revert): edit configure.py → `config.pch_eligible_dirs = set()` →
`python3 configure.py && ./tools/ninja-locked` (eligible TUs recompile back through plain
msvc — byte-identical to pre-PCH). Tier 2: `git revert <landing commit>` + reconfigure +
settle. decomp_pch.h/.cpp stay either way (pre-existing, unreferenced when disabled).
