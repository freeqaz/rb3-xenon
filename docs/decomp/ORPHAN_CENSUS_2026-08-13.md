# Orphan census — every branch in the repo, 2026-08-13 (lane ORPHAN-CENSUS)

## Headline

**663 branches. 440 carry commits `git cherry` calls unlanded. 164 hold a single
line of content main does not already have. After per-branch content
verification, the number that should be landed is ZERO.**

| measure | count |
|---|---|
| branches (excl. `main`) | **663** |
| carry `+` commits (`git cherry` says unlanded) | **440** |
| …hold ≥1 genuinely novel line | **164** |
| …survive the blob-in-main-history test | **74** |
| **classified LAND** | **0** |
| classified NEEDS-LANE | **0** |
| could not classify confidently | **0** (`laneDY1-ab` resolved 2026-08-13 — §7.1) |

**The gap between 440 and 0 is the finding.** Patch-id equivalence is a candidate
generator, not a verdict; every retirement below is backed by a content check
against main, and the method is named per branch in the sidecar.

There is **no ranked LAND list**, because after verification there is nothing to
rank. That is the deliverable: the dangling work has already landed, been
superseded, or was deliberately abandoned — and a large part of it is
**actively dangerous to land** (§5).

This lane was **read-only with respect to source**. It merged, rebased, deleted
and pushed nothing.

| artifact | path |
|---|---|
| machine-readable, all 663 rows | `docs/decomp/orphan-census-2026-08-13.json` |
| same as TSV | `docs/decomp/orphan-census-2026-08-13.tsv` |

---

## 1. Snapshot provenance — read before quoting any number

- **main pinned at `32f4fdb1`.** Branch tips pinned **by sha** from one
  `git for-each-ref` at the start of the run (664 refs = 663 branches + `main`).
  Every measurement keys on those shas, never on branch names.
- ⚠ **Both ends moved during the run.** Lane ORPHAN-SAVE banked uncommitted
  worktree dirt onto these branches: re-reading refs at the end showed
  **116 tips moved, 1 branch created**, 0 deleted. Those 117 are censused
  **separately** (§6) so two snapshots are never silently mixed. Main itself
  advanced `32f4fdb1` → `96356aac` → `30ffcaa8` while this ran.
- ★ **Main's drift only strengthens the conclusion.** Main only ever *gains*
  content, so every "already on main" verdict stays true and the LAND set can
  only shrink. The census is conservative in the direction that matters.
- **`local/main` (`37883122`, 2026-07-29) is the pre-scrub main tip**, used as
  the old-era trunk reference throughout.

---

## 2. Three instruments are invalid here — including two that look authoritative

### 2.1 `git branch --merged` — invalid (already known)

Lanes historically landed by **patch** (`cherry-pick` / `git apply` /
`format-patch`), which leaves a branch permanently reading "unmerged" though its
content is fully on main.

### 2.2 ★ Half the repo has **no merge-base with main at all**

**316 of 663 branches (47.7%) return no merge-base** — disjoint histories, caused
by the **2026-07-30 history scrub** that rewrote and force-pushed main. For those,
`git cherry` cannot run; it errors. A census that simply iterated `git cherry`
would have silently lost half the population. Substituted:

```
own  = git rev-list <branch> --not local/main     # local/main = 37883122
plus = [c for c in own if patch_id(c) not in patch_ids(main)]
```

### 2.3 ★★★ Patch-id is **near-vacuous on the population that matters** — my own control failed

I ran what I believed was a control: **74.0%** of all disjoint-branch commits have
a patch-id on main, which looked like proof that patch-id survives the rewrite.

**That control measured the wrong population.** ~78% of those commits are shared
pre-scrub *trunk*. Restricted to **branch-own** commits — the only ones the census
is about — every sample I drew returned **0 landed**; patch-id calls essentially
all branch-own work `+`.

Corroborated independently: lane HARVEST-TOOLS measured **all 8 `laneCK3` commits
reading `+` while their content was verifiably on main** (patches reverse-apply;
map rows already carry the proposed names). Work reaches main **rewritten,
refactored, or re-implemented by a later lane**, and patch-id sees none of it.

⇒ **A `+` from `git cherry` is a CANDIDATE, never a verdict.**

### 2.4 ⛔ "Differs from main's TIP" is also wrong — and it manufactured false candidates

Comparing a branch file against `main:<path>` **at the tip** marks as novel
anything that landed and was *later refactored*. The correct test is whether the
blob exists anywhere in **main's history** (`git rev-list --objects main`).

**Measured: this demoted 90 of my 164 candidates** — every novel file's exact
bytes had been on main at some point. Control: `verifyCD9`'s ICF tools differ from
main's tip yet are byte-identical to landed commit `92e3951a`, an ancestor of
main.

### 2.5 ⛔ File-level "differs" is vacuous for large aggregates — diff at ROW level

`target_symbol_map.json`, `config/45410914/splits.txt` and
`config/45410914/objects.json` are large aggregates that main has since extended,
so **every stale copy "differs" while containing nothing new**. Measured at row /
entry level:

| aggregate | result |
|---|---|
| `objects.json` | **26 of 27 branches add NOTHING** (`NEW = 0`). Only `salvage/laneBO4` adds 2 entries. The file-level signal was **96% false**. |
| `target_symbol_map.json` | across the WIP copies: **13,005 new rows vs 28,591 CONTRADICTED** — main carries a *different* name for those addresses. Main's map (28,948 rows) is ahead of every branch copy. |
| `splits.txt` | ~30% of "new" lines are `.pdata`, which is **derived output regenerated on every split run** (CLAUDE.md), i.e. pure noise. |

★ **And the reverse-direction control kills the rest of the splits signal.**
`.text` pins the branch has and main lacks are *outnumbered by pins main has and
the branch lacks* in every case: `ce11` 1,664 branch-only vs **2,174 main-only**;
`verifyCD4` 1,733 vs **2,314**; `laneDG2` 80 vs **548**. These are **superseded,
larger** pin sets — not missing work. Without the reverse check I would have
reported ~1,700 "novel pins to land".

### 2.6 The instrument that does work — with its positive control and its limit

**Line containment against main's current file**, combined with **blob-in-history**.
For each branch, take its own patch (`base..tip`) restricted to the files its `+`
commits touch; for each line it *adds*, ask whether that line already appears in
main's copy.

★ **Positive control against external ground truth:** on `laneCK3B1/B2/B3` — where
patch-id produced false `+` and HARVEST-TOOLS proved the content is on main — line
containment reports **`novel = 0` for all three**. It catches what patch-id missed.

⚠ **Its honest limit:** for `laneCK3` *itself* the instrument is **silent, not
correct** — its only file is the excluded map, so it scores nothing. And
containment over a handful of lines is weak evidence. The sidecar therefore
carries `added_lines_scored` beside every verdict; §4 breaks retirements down by
evidence strength.

**Excluded from scoring**, as meaningless or unwanted:

| excluded | why |
|---|---|
| `splits.txt`, `symbols.txt`, `target_symbol_map.json`, `build/**`, `report.json` | regenerated aggregates (handled at row level instead, §2.5) |
| `scripts/grind/**` | deliberately removed from main; **must never return** (§5) |
| `docs/plans/decomp-training-harvest-2026-07-29.md` + reasoning-corpus docs | removed by main commit `28002540` (2026-07-30) |
| root-level scratch `*.json` | lane scratch (`eval_roster.json`, `bp1_map_fragment.json`, …) |

---

## 3. Classification breakdown (all 663)

| classification | count | meaning |
|---|---|---|
| LANDED-REWRITTEN | **308** | content on main in a different form (content-verified) |
| LANDED | **216** | ancestor of main, or every own patch-id on main |
| NO-CONTENT | **90** | touches only regenerated / deliberately-excluded files |
| SUPERSEDED | **24** | a later, better solution won on main |
| EXPERIMENT | **18** | deliberately abandoned; **history, must NOT be landed** |
| REFUTED | **6** | premise since disproven |
| SCRATCH | **1** | regenerable pipeline output (`laneCK3`, 221,744 lines) |
| UNRESOLVED | **0** | `laneDY1-ab` resolved 2026-08-13 → LANDED-REWRITTEN (§7.1) |
| **LAND** | **0** | — |

★ **The orphan problem is bounded to before 2026-08-04.** The newest branch
holding novel content dates to **2026-08-03**, and 122 branches tipped in August
are plain ancestors of main. That is the `git merge --no-ff` landing rule (in
force since 2026-08-04) working exactly as intended: lanes now land as merges and
nothing dangles. **Dangling is a legacy of the patch-landing era and is already
fixed going forward.**

---

## 4. How strong is "retire"?

Of the 276 `+` branches whose novel-line count is **0**:

| evidence | count |
|---|---|
| touches only excluded/regenerated files (nothing landable by construction) | 91 |
| 100+ added lines, all present on main — **STRONG** | 87 |
| 10–99 added lines, all present on main — moderate | 80 |
| 1–9 added lines — **weak; flagged in the sidecar** | 18 |

The 18 weak rows are this census's honest soft spot: almost certainly landed, but
containment over <10 lines is not proof. They carry `(weak evidence)` in the
sidecar `reason` field.

**Three prior audits agree.** `docs/plans/branch-audit-2026-07-29.md` (302
branches) and `docs/plans/branch-audit-slice3-2026-07-30.md` (the 47 oldest) both
concluded **0 landings**; 29 of my candidates are named individually there and I
inherit those verdicts rather than re-litigating. Lane ORPHAN-SAVE's concurrent
adjudication of 940 banked files found **74 of 116 trees contributed zero
never-on-main content**. This census adds what none of them covered: the
**disjoint-history half** created by the 07-30 scrub.

---

## 5. ⛔ Why most of this cannot be landed by merge even if you wanted to

**291 of the 316 disjoint branches would re-add deliberately-scrubbed files.**

Main keeps **3** benign files under `scripts/grind/` (`__init__.py`,
`classify_funclets.py`, `worklist.py`). Every pre-scrub branch carries **27** at
its base — including `push_corpus.sh`, `claude_backend.py`,
`export_training_data.py`, `corpus.py`, `synth_traces.py`: the training-corpus
machinery that commit `28002540` removed from this public repo.

⇒ **`git merge` of any pre-scrub branch drags 24 scrubbed files back in**,
independently of whether its content has any value. Any landing from that era
must be **selective file extraction**, never a merge. Since content verification
found nothing worth extracting, the correct action is none.

### Two branches are actively dangerous to apply

- **`laneDW1-ab`** — its `src/system/obj/Object.h` hunk re-introduces a claim main
  **explicitly marks REFUTED** (main's `Object.h` names lane DY-2a: the "retail's
  `New<T>` HAS the `MILO_FAIL` guard" inference does not hold).
- **`wave4-land`** — its `Crowd.cpp` **would break the build**: main moved
  `ColorPalette::GetColor` to an in-class definition, so the branch's out-of-line
  copy is a redefinition.

### Three branches are refuted by main's own source comments, which name the lane

`laneCU2` (by DI-2/C), `laneBV1` (by CF-7/CF-7c), `salvage/laneBO4` (by BT-1).
**Landing `laneCU2` would revert a 100% match** — main proves both arms `_ALT`
reach 100%, while the branch carries a comment calling it an unfixable wall. A
naive "unlanded, therefore land it" pass would have regressed main.

### Several branches carry their own DO-NOT-LAND verdict

`laneCO1` (*"DO NOT LAND — measured −4 matched / −104 B"*), `laneM-str` (*"NULL
RESULT… −57 strict, 0 GAINED… Reverted"*), `laneAV-C` (*"SITE COUNT 2,045. DEFECT
COUNT 0. STRICT YIELD +0"*), `laneAV-B2` (*"SUPERSEDED… UNMEASURED… DO NOT LAND"*),
`laneCP3B-b3` (*"NOT landable yet"*), `gbC` (*"net 0 strict"*). These are failed
A/B legs and refuted probes — **valuable as history, and the branch already
preserves them.**

---

## 6. The banked-WIP delta (ORPHAN-SAVE's concurrent work)

117 branches changed under me, each gaining one
`wip(...): orphaned WIP recovered by ORPHAN-SAVE` commit banking a lane's
previously-uncommitted worktree dirt.

| novel lines in the banked WIP | branches |
|---|---|
| 0 | 74 |
| 1–19 | 26 |
| 20–99 | 5 |
| 100+ | 12 |

Dominated by **regenerable artifacts**: `target_symbol_map.json` (45 branches),
`splits.txt` (20), and `function_analysis/*.md` objdiff scratch. The most
promising residue — **9 `patches/*.patch` files** on `laneCK3B1/B2/B3` holding
UIStats / PresenceMgr / BandStorePanel ports — verified **all 9 present on main,
novel 0, contradicted 0**; each WIP tip is only a `patches/` re-export of that
branch's own earlier commits.

⚠ **Dirtiness is not evidence of anything.** ORPHAN-SAVE measured that **94.2%
(817/867) of dirty *tracked* content already existed in main's history** — because
a lane that landed by patch leaves its worktree dirty forever.

---

## 7. What this lane did NOT do

- **Landed nothing.** No merge, rebase, cherry-pick, branch deletion, or push.
  This document and its sidecar are the entire output.
- **No A/B and no native gate.** This change is **docs + sidecar only**, so
  neither applies. A deliberate skip, stated rather than silently taken.
- **Did not re-adjudicate** the 29 branches already covered individually by the
  two prior audits, nor re-derive ORPHAN-SAVE's 940-file adjudication.
- **Did not verify the 18 weak-evidence retirements** beyond line containment.
- ✅ **`laneDY1-ab` was UNRESOLVED — now RESOLVED, see §7.1.** As written, this
  lane could not classify it: 20 rev-dialect files, delegated verification never
  returned a measured verdict, and there was only a *prior* that its files match
  the rev-dialect family `laneDI1` carried. Refusing to classify on that prior
  was the right call — **the prior named the wrong family.** Measured verdict:
  **LANDED-REWRITTEN**, and it must not be landed.
- **Did not chase the moving target past one delta pass.** Tips will keep moving;
  the census is pinned to the recorded shas.

---

## 7.1 `laneDY1-ab` — RESOLVED 2026-08-13 (lane TOOLFIX-1)

**Verdict: LANDED-REWRITTEN. ⛔ DO NOT LAND — merging it would REVERT main.**

The branch's whole committed history is an **ancestor of main**
(`git merge-base --is-ancestor 725bb9ed7539 main` ⇒ true; `plus_commits` 0). The
only object off main is a single ORPHAN-SAVE WIP commit **`2547c060`** banking 20
uncommitted rev-dialect files (607 insertions / 268 deletions).

### The measurement

| check | result |
|---|---|
| WIP blobs **identical to main's TIP** | **9 / 20** |
| WIP blobs **elsewhere in main's history** (landed-then-refactored) | **11 / 20** |
| WIP blobs **never in main's history** | **0 / 20** |
| reverse-direction control | branch-only **181** lines vs main-only **305** |

Every one of the 20 blobs attributes to a commit **on main** — and all three are
named `fix(DY-1)`, i.e. **this lane's own landed work**:

- `8d5927c3` *fix(DY-1): rev dialect batch 1 — 4 more Load bodies to 100%*
- `009f7490` *fix(DY-1): rev dialect batch 2 — 6 more Loads to 100%, 2 TUs REVERTED as lossy*
- `40abb376` *fix(DY-1): rev STORAGE CLASS — TrackWidget 87.7% → 92.1%*

⇒ **The census's prior named the wrong family.** It guessed `laneDI1`'s
rev-dialect override family; the content is DY-1's own. Declining to classify on
that prior was correct — the prior was wrong, and only the content test found it.

### ⛔ Why it must not be landed

The WIP is a **stale snapshot**, not new work. Its files predate later main
commits, so a merge would revert them:

- `f278d4d7` *wave4(NCCC-0803-b2bb): crack the [70,90) band — +350 matched, +0.900pp code*
- `044ffc1a` *rev(Env, UIFontImporter): same no-static dialect — UIFontImporter::Load → 100%*
- `7f54a1e6` *match(UIFontImporter): SyncProperty 92.02 → 100%*

### ⚠ Two files read "branch ahead" on line counts and are NOT

`rndobj/AmbientOcclusion.cpp` (52 branch-only vs 47 main-only) and
`track/TrackWidget.cpp` (50 vs 23) invert the aggregate control. Both are **false
signals**, and the reason generalises: the differing hunks are **not rev-dialect
at all** — they are `IsValid_AOReceive` and `kdTree<Triangle>::Intersect` bodies
that **main gained later** in `f278d4d7`. A per-file line-count aggregate cannot
tell "branch has content main lacks" from "branch predates main's improvement";
only **blob-in-history plus per-blob attribution** separates them, and both files'
blobs sit verbatim in main's history. This is §2's lesson firing on a live case:
had the count been read as a verdict, the recommendation would have been to land
a patch that reverts `+350 matched`.

---

## 8. Reproducing this

```bash
git for-each-ref --format='%(refname:short)|%(objectname)|%(committerdate:iso8601-strict)' refs/heads/
git rev-list main | git diff-tree --stdin -p | git patch-id --stable   # main patch-ids
git rev-list <branch> --not local/main                                  # disjoint-era own commits
git rev-list --objects main | awk '{print $1}'                          # blob-in-history set
git diff -U0 <base> <branch> -- <files>                                 # then containment vs main:<file>
```

The four decisive checks, in the order that matters: **row-level** for aggregates,
**blob-in-history** rather than blob-vs-tip, **reverse-direction** control on any
"branch has N that main lacks" claim, and a **positive control** proving the
instrument can fire before believing a clean negative.
