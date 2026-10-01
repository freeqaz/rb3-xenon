# W16-JI — `ab_measure` could not restore a worktree when the patch added a file (2026-10-01)

Lane W16-JI, branch `w16-ji`, worktree `~/tmp/wt-w16ji` off main `17aaa4298`. Not merged.

## 1. The report

Two lanes saw `tools/ab_measure.py` finish **rc=0** and print
`COULD NOT RESTORE THE WORKTREE ... action: restored`, with tracked files still modified:

- W16-JG (`W16JG_PLACEHOLDER_SLOT_AUDIT_2026-10-01.md` §5): two patched files still applied.
  The patch's one **new** file, `tools/alias_placeholder_slot_audit.py`, was gone.
- W16-JC (`W16JC_GAME_LAYER_WEIGHTED_SWEEP_2026-10-01.md` §7): 89, then 101 files still
  modified, after a patch that "adds TUs".

The common factor I predicted was a patch that **adds a file**. The reproduction confirms it.

## 2. Two defects in `TreeGuard.restore()`

**(1) One unknown pathspec aborts the whole checkout.** `plan_restore` widens the checkout set
to every path the measured patch names. That is right for a modified file, but a file the
patch *adds* has no index entry. `git checkout -- a b new` then refuses the **entire** command
(`error: pathspec 'new' did not match`, rc=1) and restores nothing. The call used
`check=False` and ignored rc. The untracked-file removal still ran, so a new file in an
existing directory was deleted while every modified file stayed applied. That is W16-JG's
state exactly. Checked on a scratch repo: rc=1, and the modified file was unchanged.

**(2) A new directory is one porcelain entry.** `_untracked()` read `git status --porcelain`,
which collapses an untracked directory to `?? dir/`. `restore()` only unlinks entries where
`is_file()` is true, so the directory and its file survived. The outcome still listed the
directory under `removed`, which was a false claim in the result record.

The banner then closed with "(Expected for splits.txt: …)". W16-JC read that line as the
explanation ("the banner names only splits.txt as expected"), but splits.txt had nothing to do
with it.

## 3. Reproduction on the real tool (same worktree, warm cache)

Patch `~/tmp/w16ji_repro.diff`: a comment appended to `src/system/math/Rand.cpp` (one compiled
TU, so the run reaches MEASURED) plus a new file `src/w16ji_probe/probe_new.h` in a new
directory.

| run | rc | verdict | `[tree]` | tree afterwards |
|---|---|---|---|---|
| **old** code (`17aaa4298`) | 0 | MEASURED, Δ0 | `COULD NOT RESTORE`, `action: restored` | ` M src/system/math/Rand.cpp`, `?? src/w16ji_probe/probe_new.h` |
| **fixed** (`w16-ji`) | 0 | MEASURED, Δ0 | `restored ... verified`; deleted-untracked banner lists `src/w16ji_probe/probe_new.h` | only the caller's own pre-run `?? docs/w16ji_caller_untracked.txt` |
| **fixed**, new-file-only patch `src/w16ji_probe2/only_new.h` (refusal-after-apply path) | 2 | REFUSED at `apply` (absent-vs-absent) | `restored (0 path(s)) ... verified`; deleted-untracked banner lists `src/w16ji_probe2/only_new.h` | only `?? docs/w16ji_caller_untracked.txt`; `src/w16ji_probe2/` gone |

The old run's `result.json` names the mechanism. `checked_out` contains the never-tracked
`src/w16ji_probe/probe_new.h`, `removed` claims `src/w16ji_probe/`, and the note reads
`post-restore diff still differs ...; untracked set differs ...: still-present ['src/w16ji_probe/']`.
Each prediction was written before its run. One setup prediction failed: a fixed run with
a caller edit to `docs/INDEX.md` was **REFUSED at preflight**, correctly, because without
`--from-dirty` any modified tracked file is refused. That run never armed the guard and left
the tree untouched, so the caller-state check uses an untracked file instead.

## 4. Fix (`tools/ab_measure.py`)

- `restore()` checks out only the candidate paths the **index knows**
  (`git --literal-pathspecs ls-files -z -- …`). A patch-added file is untracked, so the
  removal step undoes it. A non-zero checkout rc is now reported as
  `action: checkout_failed` with git's output, not ignored.
- `_untracked()` is `git ls-files --others --exclude-standard -z`. It lists one entry per
  file, honours `.gitignore`, and is NUL-safe.
- `note_patch()` records, **before** the patch is applied, every directory the patch will
  create. `restore()` rmdirs those if they end up empty, and never touches a directory that
  existed before.
- The failure banner prints the reason and the residual paths, using `diff_residual_paths`,
  which also catches a file present in both diffs with different content. The splits.txt
  line is gone.

## 5. Selftest, and proof it fails without the fix

The existing TOOL-AB restore cases are pure planner logic, and they could not see this bug:
the *plan* for a new-file patch was right, but the *execution* was not. Four new behavioural
fixtures drive the real `TreeGuard` against a scratch git repo with no build, then read the
tree: `git diff`, every untracked file, and any empty directory. The fixtures:

1. modified file plus a new file in an existing directory
2. modified file plus a new file in a new nested directory
3. the W16-JC shape: two modified, two new (one in a new directory), the split rewriting
   `splits.txt`, a caller dirty doc and a caller untracked file
4. a **control**, modify-only, which the old code already handled

The fixture uses `note_patch` when present and falls back to setting `patch_diff`, so the same
test body runs unchanged against the old class.

| variant | fixture 1 | 2 | 3 | 4 (control) |
|---|---|---|---|---|
| fixed | PASS | PASS | PASS | PASS |
| **old `TreeGuard`** spliced back in | **FAIL** | **FAIL** | **FAIL** | PASS |
| fix minus the index filter (defect 1 only) | FAIL | FAIL | FAIL | PASS |
| fix minus `ls-files` untracked (defect 2 only) | PASS | FAIL | FAIL | PASS |
| fix minus the empty-dir rmdir | PASS | FAIL (`src/newdir/sub`) | FAIL (`src/newdir`) | PASS |

Every row matched its prediction. Each part of the fix is caught on its own, and the control
passes everywhere, so the fixtures are not failing on every input. `--selftest`: ALL PASS on
the branch.

## 6. Not done

- Not merged. No CLAUDE.md edit: its "hands the worktree back on every exit path" section
  becomes true again with this fix, and the coordinator can add a line when landing.
- Paths that git *quotes* in a diff header (spaces, non-ASCII) are still missed by
  `DIFF_PATH_RE`. Of 6,197 tracked paths, 0 need quoting (`git ls-files` checked with `core.quotepath=true`), so I left it.
- I did not drive the Ctrl-C and `--pick` exit paths end to end. They reach the same
  `restore()`, and `--pick` builds its patch with the same `git diff`.
