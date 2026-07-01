# Verify/A-B reliability findings + Stream-1 struct-candidate status (2026-07-01)

Coordinator verify pass on the pre-staged ready worktrees (Stream-1 struct fixes +
option-C DC3-cluster ports). Net matched delta LANDED this pass: **0** — but the pass
produced high-value tooling findings that matter more than the candidates. main @3585dcb,
10682 matched.

## TOOLING FINDINGS (feedback loop — act on these)

### 1. Warm-cache in-worktree A/B can silently return a FALSE net-zero
A CoW worktree reflinks main's warm `build/`. ninja mtime tracking can decide a `.obj`
is up-to-date even after its source changed (reflink preserved obj mtime > source mtime),
so a `fresh_report.sh` "after" run reuses the OLD obj → the A/B shows 0 change even when
the edit is real. PROVEN on CharEyes: the edit changes codegen (compiled `CharEyes.obj`
md5 differs with vs without the edit) yet the whole-binary report showed **0** fuzzy
changes across all 1972 units — the report never saw the new obj.
- NOT the report cache: `../objdiff` `report.cache` is **content-hashed** on target+base
  obj contents (`report.rs::hash_unit`, xxh3_64) — it self-invalidates correctly.
- `fresh_report.sh`'s warm-cache divergence check only warns on a LARGE spurious delta
  (`abs(delta) > 10`); it CANNOT catch a false-net-**zero** from an obj not rebuilding.
- **RELIABLE FIX:** use `scripts/setup_worktree.sh <path> <branch> --cold-cache` for any
  A/B that must be trusted, OR force-rebuild the changed objs (`touch` the edited sources
  before build, or rm the specific `build/45410914/src/.../<TU>.obj`) and confirm the obj
  mtime advanced past the source. A struct/layout edit that shows EXACTLY 0 whole-binary
  change is SUSPECT — re-verify cold before believing it.
- TOOLING TODO: extend `fresh_report.sh`'s check to flag "edited source has an obj whose
  mtime ≤ source mtime" (the false-negative case), not just large deltas.

### 2. Stamp-removal glob is inconsistent across the SOP
The renamer stamp is at `build/45410914/target_symbol_renames.stamp` (top level, no
subdir). Several workflows/docs use the WRONG glob `build/45410914/*/target_symbol_renames.stamp`
(zsh: "no matches" → stamp NOT removed): `.claude/workflows/verify-stage-wave.js`,
`scripts/wf_{idt_harvest,classa_ports,bodyport_tails,idt_classb,classa_harvest}.js`,
`docs/decomp/plans/{post-codegen-kill-streams,codegen-matcher-investment-prompt}.md`.
Others use the correct top-level path. Standardize on the top-level path everywhere.
(Note: for a struct-only edit the target obj is unchanged so the renamer is irrelevant;
this mainly matters for splits/tsm changes — but fix it for consistency.)

### 3. Concurrent verify has a git-stash RACE
`git stash push/pop` uses the SHARED object store across sibling worktrees; a concurrent
agent's stash push made another's `pop` grab+drop the WRONG stash (recovered via
`git stash store <dangling-sha>`). The verify-stage-wave used stash push/pop → unsafe.
**Verify SERIALLY (coordinator-driven), or use cp-aside** (copy edited files to ~/tmp,
`git checkout` for baseline, restore from copy) — never the shared stash under concurrency.
Matches [[project_shared_index_commit_race]]. `fresh_report.sh` also writes a SHARED log
(`/tmp/rb3_build_fresh_report.log`) → garbled under concurrency; should be per-worktree.

### 4. objdiff-cli batch mode resolves symbols GLOBALLY
`objdiff-cli diff --batch` (symbols on stdin) ignores an explicit `-1/-2` obj pair and
resolves against whatever unit the target_symbol_map points to (catch-all → base_size=0
→ false STUB verdicts). Use single-symbol one-shot `diff -1 <target> -2 <base> <sym>`.

## STREAM-1 STRUCT CANDIDATES — status (all UNVERIFIED; estimates were access-site counts)
Their roadmap "+N" are IMM_OFFSET access-SITE counts, NOT verified function flips. The one
CharEyes near-miss probed (`EyesOnTarget` 97.2%) is **permuter-class** (regswaps/FPR),
which a struct-offset fix won't flip — so the struct-cascade classification may be
over-optimistic. Edits are preserved in the shared stash stack for a cold re-verify:
- CharEyes (stash: `WIP on wt-s1-CharEyes`) — edit changes codegen but net effect UNVERIFIED.
- Character (`@{2}`), CreditsPanel (`@{3}`, worktree still present), GamePanel (`@{0}`).
- **Recommended:** re-verify each in a `--cold-cache` worktree; land only cold-verified net+/0-regr.

## OPTION-C PORTS — verified NOT landable (this pass)
- AccomplishmentProgress: already on main (recycle the worktree).
- CharClipGroup: ported source is NonMatching — 21 fns paired, best `Save` 99.9%, most <30%,
  0 at 100%. Pinning = scaffolding, no byte-exact reveal → 0 matched. Not landable.
- MoggClip: NonMatching — best 99.93%, 7 fns at 0% with base>target size (source out of sync
  with retail — missing debug-string stripping). Not landable.
- LESSON: the "DC3-cluster port-then-pin" GO vein yields NonMatching for these units; pinning
  a NonMatching port grows the denominator but 0 matched — gate on ">=1 fn reaching 100%".

## Next
1. Re-verify the 4 Stream-1 struct edits via `--cold-cache` (reliable); land cold-verified only.
2. Fix the stamp glob + `fresh_report` false-negative check + per-worktree log (tooling).
3. Recycle the spent option-C worktrees (AccomplishmentProgress/CharClipGroup/MoggClip).
