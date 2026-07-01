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

## ⭐ POLICY CHANGE (owner, 2026-07-01): PARTIAL MATCHES ARE LANDABLE
The strict "true-100 byte-equal only; NEVER commit a partial" gate in
`post-codegen-kill-streams-2026-06-30.md` is SUPERSEDED. Partial progress counts:
- **New land gate = FUZZY-positive AND 0 regressions** (not strict-100). A struct/layout
  edit that raises fuzzy% (moves near-misses closer) without flipping to 100 is a WIN worth
  landing. A NonMatching TU PORT that compiles + pins real source at high fuzzy (CharClipGroup
  99.9%, MoggClip 99.93%) is LANDABLE — it adds the real source to the tree + grows fuzzy/pin
  coverage. Still NEVER land a regression (don't break an existing strict-100).
- Re-evaluate the option-C ports as LANDABLE under this gate (resolve MoggClip's tsm
  case-collision at 0x82482288 first; verify 0 regressions).
- Measure BOTH: strict-100 net (`measure_delta.py`) AND fuzzy delta
  (`tools/fuzzy_progress.py` / sum of fuzzy_match_percent). Gate on fuzzy>0 && 0 strict-regr.

## TOOLING FIXED (owner, 2026-07-01)
- **setup_worktree.sh now validates the warm object cache** (`d66dae0`) → the mtime-trap
  false-net-zero (finding #1) is FIXED at the tool level; fresh worktrees build incrementally
  and A/B reliably. The manual `touch <sources>` workaround is no longer needed.
- **setup_worktree neutralizes the build/compilers download edge** (`ef0fad0`, offline
  worktree build) → the per-worktree `tools/download_tool.py` hack agents were carrying is
  now redundant (don't stage it).

## Next
1. Re-verify the 4 Stream-1 struct edits (CharEyes/Character/CreditsPanel/GamePanel — edits
   preserved in the shared stash stack) via the FIXED setup_worktree, land on the FUZZY gate.
2. Land the option-C ports (CharClipGroup/MoggClip) under the partials-landable policy.
3. Fix the stamp glob + `fresh_report` false-negative check + per-worktree log.
4. Recycle spent worktrees. Run the verify+land wave when the owner's build storm subsides
   (load was 266 — concurrent heavy builds starve each other; coordinate cadence).
