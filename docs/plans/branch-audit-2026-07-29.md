# Branch / worktree audit — 2026-07-29 (laneBC)

**Headline: 0 landings. Main's 39,382 strict already dominates every dangling
branch in the repo.** Of 302 unmerged branches, exactly **one** claims a strict
count above main — `tightgap` (39,491) — and that is a *live lane still running*,
not dangling work. Everything else is superseded, self-refuted, or already in
main in a further-evolved form.

- Baseline (main `166e6268`): **39,382 matched / 69,378 total**, independently
  reproduced by a fresh full build in `~/tmp/wt-laneBC-verify` (report.cache
  removed, full `./tools/ninja-locked`) → **39,382**, exactly matching main's
  `report.json`. No landings were made by this audit, so its contribution to the
  count is **+0** by design.
- While the audit ran, a *different* live lane (laneBODYPORT, `559645e9`) landed
  **+31 → 39,522**. That is not this audit's delta; it is recorded here only
  because it moves the comparison bar further above every dangling branch and so
  strengthens every conclusion below.
- **108 branches banked** (uncommitted lane work preserved, zero lost).
- **29 worktrees + 755 branches cleaned up** (all provably already in main).
- 1,230 → **479 branches**; 388 → **362 worktrees**.

---

## 1. Method

Four filters, cheapest first. Each one is exact, not heuristic:

1. **Ancestry** — `git merge-base --is-ancestor <br> main`. Merged ⇒ content is
   literally in main ⇒ zero-risk to delete (and `git branch -d` self-gates).
2. **Blob identity** — for every dirty worktree file and every branch-changed
   file, compare its hash against main's `HEAD` blob. Equal ⇒ the work landed;
   the dirt/branch is a no-op.
3. **Line containment** — for files whose blob differs, diff the branch against
   its *merge-base* (not against main — that direction is polluted by main having
   moved ahead) and ask what fraction of the branch's **added lines** already
   appear in main's current version of that file. Applied to `src/` only:
   containment on `splits.txt` / `target_symbol_map.json` is meaningless because
   main's copies have been rewritten wholesale many times since.
4. **Claim vs. main** — scrape every unmerged branch's own commit messages for
   the strict count it claims and compare against main's 39,382. This is the
   decisive global filter: a branch claiming 30,317 cannot contain unlanded value
   relative to a 39,382 main.

★ The calibration case in the brief (`laneAW-unitsc` claiming +8 while already
merged at `34b72131`) generalises completely. Lane branches here are landed by
*patch/cherry-pick*, not by merge, so a branch stays permanently "unmerged" long
after its content is in main. **Unmerged is not evidence of unlanded.** Filters 2–4
exist precisely to break that false signal.

## 2. Live lanes — excluded from all mutation

Determined from `/proc/<pid>/cwd` of running processes plus name matching:

| worktree | branch | evidence |
| --- | --- | --- |
| `/home/free/tmp/wt-tightgap` | `tightgap` | 29 live processes (objdiff-cli on PropKeys) |
| `/home/free/tmp/wt-bodyport` | `bodyport` | 4 live processes; HEAD dated 2026-07-29 |
| `/home/free/tmp/wt-laneBA-1` | `laneBA-1` | named live lane (attribution census) |

No `laneBB*` worktree or branch is registered; laneBB is presumably operating in
main or in an unregistered directory. Nothing named `laneBB*` was touched.

`tightgap` is also the **only** branch in the repository claiming a count above
main (39,491, i.e. +109). It is in-flight work, not audit residue — leave it alone
and let the lane land it.

## 3. Classification results

### Worktrees (388 at start)

| category | count | disposition |
| --- | --- | --- |
| A — merged + clean | 25 | 24 removed (worktree + branch); `laneBA-1` skipped as live |
| A — detached, HEAD already in main, clean | 7 | 5 removed; 2 left in place (see below) |
| D — dirty, work banked to own branch | 108 | banked, worktree kept |
| D — dirty, dirt is byte-identical to main (no-op) | 17 | reported, kept, nothing to bank |
| D — dirty with regenerated-artifact dirt only | 153 | reported, kept, deliberately not banked |
| B/C — unmerged, clean | 157→202 | reported (see §5) |

Two detached worktrees were deliberately **not** removed despite being clean and
already in main, because they sit outside the `~/tmp` scratch convention and may
be deliberate long-lived environments:

- `/home/free/code/milohax/rb3-xenon-freecrack-wt` @ `12b1b5d4` (2026-07-15, oss-xbox-build)
- `/home/free/tmp/claude-1000/bake-farm/rb3-xenon` @ `99b3c62d` (2026-07-19, eval farm)

Both are zero-risk to remove whenever the coordinator wants them gone.

### Branches (1,230 at start)

| category | count | disposition |
| --- | --- | --- |
| merged, no worktree | 731 | **deleted** (`git branch -d`, 0 refusals) |
| merged, worktree removed this pass | 24 | **deleted** with their worktree |
| merged, worktree retained | 83 | kept (worktree still dirty or in use) |
| unmerged | 302 → 395 | kept; +108 new from banking, −15 folded into cleanup |

All 755 deletions were ancestors of `main`. `git branch -d` (never `-D`) was used
throughout, so git itself verified every one; **0 refusals** means no branch with
unmerged commits was touched.

## 4. Landing analysis — why nothing landed

### 4.1 Global claim filter

117 of the unmerged branches state an explicit strict count in their own commit
messages. Sorted descending against main's 39,382:

```
39,491  tightgap        <- LIVE LANE, the only one above main
39,382  docfix          <- equal to main
39,267  laneAY-C
39,266  laneAY-1
39,243  laneAX-10
39,168  laneAX-8
39,155  laneAX-6
39,134  laneAX-4
39,128  laneAX-5
39,126  laneAX-3 / laneAX-2
39,054  gapfill2
38,965  missvirt
38,952  thunkedge
```

…and 104 more, all lower. Every branch that measured itself measured itself
*below main*. There is no dangling branch whose own author believed it beat the
current tree.

### 4.2 Source-level survey (33 branches read line by line)

Two read-only survey agents read the actual `src/` diffs of every branch that the
containment filter flagged as having non-trivial unlanded source, and compared
each against main's current file content.

**Result: 31 ALREADY-IN-MAIN, 4 SELF-REFUTED, 1 apparently novel, 0 landable.**

Notable adjudications:

- **`sweep-3`** (Geo.cpp) — the one branch that survived to a build test. It
  replaces the asserting `Vector3::operator[]` in `Intersect(const Plane&, const
  Box&)` with raw `float*` indexing to kill `MILO_ASSERT` codegen; the mechanism
  is genuinely absent from main. **Measured and rejected**: main's existing form
  is already at **99.92%** on that symbol; applying sweep-3's mechanism yields
  **99.9%** — no gain, and sweep-3's own claim was only 93.2%. Superseded in
  effect, not just in form. (Experiment reverted; worktree clean.)
- **`laneP-nothrow`** — genuinely novel mechanism (relocating
  `DataNode::DataNode(const DataNode&)` back into its own TU behind
  `#pragma optimize("g", off)` instead of hosting it in `DataUtl.cpp`), but its
  **own A/B is 28,238 → 28,238, 0 gained, 0 lost**. Zero yield by construction;
  its stated value is reusability across ~233 other scatter-include sites. See §5.
- **`laneN-neg`** — partially in main, and the missing part is *deliberately
  reverted*: main's `docs/decomp/EH_FUNCLET_CASCADE.md` records that the
  PrefabMgr explicit-`String`-copy hunk became a **double** copy once `MILO_WARN`
  itself copied via `MiloStripEval`, inflating the parent frame and un-pairing
  five funclets. Re-applying it would regress.
- **`guardbit-all`** — its `Debug.h` content is already in main, and main has a
  "CORRECTED 2026-07-27" argument-evaluation-order block on top. Re-applying
  would **revert** that correction. This is the only surveyed branch touching the
  PCH/codegen-load-bearing `src/system/os/Debug.h`; leave it alone.
- **`slm-port`** — a 2,276-line SaveLoadManager port superseded by main's own
  2,197-line version (materially different signatures and members). Its own
  `LOG.md` documents a three-layer wall and only 3 byte-exact reveals.
- Self-refuting branches, quoted from their own commits: `cA2-MainHubPanel`
  ("span-pin REFUTED (honest +0) … 92.9% ICF stub-folds"), `pilot-ssn`
  ("REFUTES 3/5 table RECON entries as MISATTRIBUTED"), `w16-bandprofile`
  ("identity-transfer self-refute, net +0"), `cA2-ClosetMgr` ("span REFUTED"),
  `cas-CharacterCreatorPanel` ("0 honest matches … all 35 ICF stub-folds"),
  `lever1` ("DEFER … regresses -6, wrong root cause").

### 4.3 The remaining tail

16 further branches carry 1–4 unlanded `src/` lines each (`bc-c`, `bulkstatic`,
`charfaceservo-fix`, `dxrnd-fix`, `gbA`, `gbE`, `hdcache-fix`, `objectdir-plus4`,
`r2-lane2`, `storeoffer-fix`, `sweep-smoke`, `tail-w4`, `texfam-fix`,
`closeout5-g2-overshell`, `followup/round3-full-batch`, `lever1`). All belong to
branches whose claimed counts sit 1,000–12,000 below main. Not worth a build slot.

### 4.4 Splits/map branches — not merged, by doctrine

96 unmerged branches differ from main **only** in `config/45410914/splits.txt`,
`config/45410914/objects.json`, and/or `scripts/target_symbol_map.json`.

★ These were **not** merged and must not be. Union-merging `splits.txt` has cost
81 real losses before; splits state is a global fixpoint that must be *re-derived*
against the current tree, never textually reconciled. All are reported in §5.
Their claimed counts top out at 39,267 (`laneAY-C`) — below main — so there is no
evidence any of them holds unlanded yield anyway.

## 5. REPORT-ONLY — unlanded work, with value judgment

Nothing below was deleted. Ordered by my estimate of what is worth re-funding.

| item | judgment | note |
| --- | --- | --- |
| `tightgap` (live) | **let it finish** | Only branch above main (39,491, +109). Not audit residue — an in-flight lane. Do not touch. |
| `bodyport` (live) | **let it finish** | HEAD 2026-07-29, save-revision-from-`.data`-int across 7 TUs; 288 unlanded lines, 0% contained. Genuinely new work in progress. |
| `laneP-nothrow` | **worth re-deriving as a technique, not as a patch** | `#pragma optimize("g", off)` defeats scatter-include nothrow deduction *in place*. Self-measured at exactly 0 delta on its one site, so landing it is pointless — but it is the only known way to fix a scatter-include site where relocating the function is impossible. Fund it as a **scanner over the ~233 other sites**, not as a merge. |
| 96 splits/map-only branches | **refuted as branches; re-derive if a lane wants the vein** | Highest claim 39,267 < main. Each represents a harvest that already landed. Never merge; re-run the harvester against current main if the vein is reopened. |
| 108 banked WIP branches (§6) | **unknown — unverified by construction** | These are mid-flight edits frozen at whatever state their agent left them. Most are single-file near-miss experiments. Cheap to re-open individually; none should be trusted without its own A/B. |
| `slm-port` | **refuted** | Superseded by main's SaveLoadManager.cpp. Its LOG.md wall diagnosis is still worth reading before anyone re-attacks that TU. |
| `guardbit-all`, `laneN-neg` | **actively harmful to re-apply** | Both would revert deliberate, documented corrections in main. Flagged so a future audit does not "rediscover" them. |
| `sweep-3` | **refuted by measurement** | 99.92% (main) vs 99.9% (with patch). Recorded so nobody re-tests it. |
| 153 worktrees with regenerated-artifact dirt | **discard-safe** | 89 × `target_symbol_map.json`, 40 × `splits.txt`, 11 × `objects.json`, plus `scripts/grind/*` eval scaffolding. All regenerable outputs whose main versions are far ahead. Deliberately not banked — banking them would add 153 commits of stale state. |
| 2 detached worktrees outside `~/tmp` | **removable anytime** | `rb3-xenon-freecrack-wt`, `bake-farm/rb3-xenon`. Clean, HEADs already in main. |

## 6. Banking commits

108 worktrees had uncommitted tracked edits that genuinely differed from main.
Each was committed **to that worktree's own branch**, path-limited to the exact
files, with message `wip(bank): preserve uncommitted lane work (laneBC audit
2026-07-29)`. **0 failures.** Nothing was committed to `main`.

Exclusions honoured everywhere (never committed, in any worktree):
`config/45410914/symbols.txt` (regenerated dtk output — and a known split-breaker
when drifted), `tools/xex2pack/xex2pack.py`, `src/system/os/MasterAudio.cpp`
(foreign WIP). Files whose working-tree content was already byte-identical to
main were skipped rather than banked.

**Verification: after banking, a full sweep confirmed 0 worktrees anywhere in the
repo still carry uncommitted `src/`, `docs/`, or `native/` content that differs
from main.** No source work was lost.

Largest banks: `replay-pilot` (30 files), `replay-cot` (30), `closeout24-o1` (50),
`laneAX-9`/`laneAY-*` families, and ~60 single-file near-miss worktrees.

### 6.1 Full banked-branch list (108)

  - `binstreamrev` (2 files)
  - `binstreamrev-land` (1 files)
  - `cal-6` (2 files)
  - `cal-verify` (1 files)
  - `campaign-handle` (1 files)
  - `closeout10-g1-fresh10` (2 files)
  - `closeout10-h1-httpget-close` (1 files)
  - `closeout11-g1-fresh11` (1 files)
  - `closeout13-g2-overshell` (1 files)
  - `closeout13-g4-metaband` (1 files)
  - `closeout13-g6-track` (1 files)
  - `closeout13-g7-metasmalls` (2 files)
  - `closeout14-l1-uipanel` (3 files)
  - `closeout14-l2-bandui` (2 files)
  - `closeout14-l3-songmetadata` (2 files)
  - `closeout14-n1-bighandles` (1 files)
  - `closeout14-n2-mid` (1 files)
  - `closeout15-v1-vocal` (4 files)
  - `closeout15-v2-scorelabels` (1 files)
  - `closeout15-v3-musiclib` (2 files)
  - `closeout15-v4-meta` (1 files)
  - `closeout15-v5-storeui` (4 files)
  - `closeout15-v6-banduishell` (4 files)
  - `closeout16-b1-banduishell` (2 files)
  - `closeout16-s1-songsort-layout` (4 files)
  - `closeout17-r1-banddirector` (2 files)
  - `closeout17-r4-bandcamshot` (2 files)
  - `closeout17-r5-vocaltrack` (1 files)
  - `closeout18-d2-banddirector` (2 files)
  - `closeout18-o1-outfitconfig` (1 files)
  - `closeout18-r7-player-context` (2 files)
  - `closeout18-w2-bandwardrobe` (1 files)
  - `closeout19-d3-banddirector-recon` (1 files)
  - `closeout19-o2-outfitconfig` (1 files)
  - `closeout19-r8-editsetlist-songmgr` (2 files)
  - `closeout20-dc1-dircut-recon` (4 files)
  - `closeout20-p1-player` (1 files)
  - `closeout24-o1` (50 files)
  - `closeout24-v1` (1 files)
  - `closeout25-a1` (3 files)
  - `closeout26-v4` (1 files)
  - `closeout27-v4` (1 files)
  - `closeout28-f1` (1 files)
  - `closeout28-n1` (1 files)
  - `closeout29-r1` (2 files)
  - `closeout29-t1` (2 files)
  - `closeout30-p2` (1 files)
  - `closeout30-r2` (2 files)
  - `closeout30-v5` (1 files)
  - `closeout31-s1` (1 files)
  - `closeout31-v6` (1 files)
  - `closeout32-h1` (1 files)
  - `closeout33-a3` (1 files)
  - `closeout33-t3` (1 files)
  - `closeout34-a4` (3 files)
  - `closeout36-c3` (1 files)
  - `closeout37-i2` (1 files)
  - `closeout5-g1-uploader-profile` (1 files)
  - `closeout5-g3-gem` (2 files)
  - `closeout5-g6-engine-a` (1 files)
  - `closeout6-g2-game-b` (1 files)
  - `closeout6-w1-mic-vtable` (1 files)
  - `closeout6-w2-bandprofile` (1 files)
  - `closeout7-g1-handles` (1 files)
  - `closeout7-g2-tracks` (3 files)
  - `closeout7-gm1-game-boolblock` (2 files)
  - `closeout7-q1-quazal-bases` (1 files)
  - `closeout8-g1-fresh-engine` (1 files)
  - `closeout9-c3-cameramanager-msgs` (2 files)
  - `closeout9-g1-fresh9` (2 files)
  - `closeout9-n2-serverdata` (1 files)
  - `closeout9-sq1-synth-bases` (2 files)
  - `eval-lane-4` (2 files)
  - `eval-lane-5` (2 files)
  - `eval-laneB-0` (2 files)
  - `eval-laneB-1` (2 files)
  - `eval-laneB-2` (2 files)
  - `eval-laneB-3` (2 files)
  - `eval-laneB-4` (2 files)
  - `eval-laneB-5` (2 files)
  - `eval-wt-glm-0` (2 files)
  - `eval-wt-glm-1` (2 files)
  - `eval-wt-glm-2` (2 files)
  - `eval-wt-glm-3` (2 files)
  - `eval-wt-glm-4` (2 files)
  - `eval-wt-glm-5` (2 files)
  - `m1-bsmeta` (1 files)
  - `nm-charclipdriver-ctor` (1 files)
  - `nm-crowd-billboard` (1 files)
  - `nm-memstream` (1 files)
  - `nm-vocalplayer` (1 files)
  - `oreval` (3 files)
  - `ov-CharClipGroup` (1 files)
  - `p5w5-perm` (1 files)
  - `p5w5-perm-rev` (1 files)
  - `p5w5-sd1` (1 files)
  - `p5w5-sd2` (3 files)
  - `p5w6-cbodies` (1 files)
  - `p5w6-cbodies-rev` (1 files)
  - `p5w7-gameplay` (1 files)
  - `p5w7-gameplay-rev` (1 files)
  - `replay-cot` (30 files)
  - `replay-pilot` (30 files)
  - `sizedvec-experiment` (3 files)
  - `wire/fspanel` (2 files)
  - `wire/rgtrainer` (1 files)
  - `wt-s1-CreditsPanel` (1 files)
  - `wt-s1-CreditsPanel-verify` (1 files)

## 7. Cleanup log

### 7.1 Worktrees removed (29)

  - wt+branch closeout11-t4-trunc-bodyport  /home/free/tmp/closeout11/wt-t4-trunc-bodyport
  - wt+branch closeout36-j2  /home/free/tmp/closeout36/wt-j2
  - wt+branch laneAA-axis  /home/free/tmp/wt-laneAA-axis
  - wt+branch laneAA-triA  /home/free/tmp/wt-laneAA-triA
  - wt+branch laneAA-triB  /home/free/tmp/wt-laneAA-triB
  - wt+branch laneAA-triC  /home/free/tmp/wt-laneAA-triC
  - wt+branch laneAE-vecdtor  /home/free/tmp/wt-laneAE-vecdtor
  - wt+branch laneAH-triage  /home/free/tmp/wt-laneAH-triage
  - wt+branch laneAP-1  /home/free/tmp/wt-laneAP-1
  - wt+branch laneAO-maphand  /home/free/tmp/wt-laneAO-maphand
  - wt+branch laneAS-1  /home/free/tmp/wt-laneAS-1
  - wt+branch laneAT-f2b  /home/free/tmp/wt-laneAT-f2b
  - wt+branch laneAT-p1base2  /home/free/tmp/wt-laneAT-p1base2
  - wt+branch laneAT-p2base  /home/free/tmp/wt-laneAT-p2base
  - wt+branch laneAU-2  /home/free/tmp/wt-laneAU-2
  - wt+branch laneAW-land  /home/free/tmp/wt-laneAW-land
  - wt+branch laneAW-1  /home/free/tmp/wt-laneAW-1
  - wt+branch laneAX-ctl  /home/free/tmp/wt-laneAX-CTL
  - wt+branch laneY-accomp  /home/free/tmp/wt-laneY-accomp
  - wt+branch laneY-dtor  /home/free/tmp/wt-laneY-dtor
  - wt+branch laneY-objref  /home/free/tmp/wt-laneY-objref
  - wt+branch tu5-flip  /home/free/tmp/wt-tu5-flip
  - wt+branch laneAI-verify-base  /home/free/tmp/wt-verify-base
  - wt+branch w8-setsolo-bounds  /home/free/tmp/wt-w8s
  - detached /home/free/tmp/w8t_mine
  - detached /home/free/tmp/wt-gbALL
  - detached /home/free/tmp/wt-laneAO-a1
  - detached /home/free/tmp/wt-laneAR-1
  - detached /home/free/tmp/wt-w4track

### 7.2 Merged branches deleted (731, no worktree)

All were ancestors of `main`; `git branch -d` accepted every one (0 refusals),
which is git's own proof that no unmerged commit was destroyed.

```
  720a798 9abd8e5 abuts-1 abuts-2 abuts-3 abuts-4
  accprog-map0x1c agent-objdir-relabel appinlinehelp-port bandfix-investigate bandgame-head4 baseline-check
  baseline-pristine bld-w1-1 bld-w1-2 bld-w1-3 bld-w1-4 bld-w1-5
  bld-w2-1 bld-w2-2 bld-wb bl-screen bl-verify bl-w1-bandchar
  bl-w1-bm bl-w1-meta bl-w1-ui bl-w1-wardrobe bl-w2-anim bl-w2-charclip
  bl-w2-fm bl-w2-line bl-w3-inst bl-w3-tex bodyport-base bpB
  bp-TrainerGemTab bpw-geo bpw-utl breadth-sweep buggy-owners bw-Band.o
  bw-BandProfile.o bw-ClosetMgr.o bw-ContextWrapper.o bw-Game.o bw-GemManager.o bw-Gem.o
  bw-GemPlayer.o bw-GemSmasher.o bw-GemTrainerPanel.o bw-MusicLibraryNetSetlists.o bw-MusicLibrary.o bw-OvershellSlot.o
  bw-Player.o bw-PracticePanel.o bw-ProfileMgr.o bw-SongDB.o bw-SongRecord.o bw-Stats.o
  bw-Tracker.o bw-Track.o bw-TrainerPanel.o bw-VocalGuidePitch.o bw-VocalPart.o bw-VocalPlayer.o
  c4after c522ba1 cA2-BandCrowdMeter cA2-BandScoreboard cA2-Defines cA2-GameMicManager
  cA2-PatchDir cA2-TrackPanel cA2-VocalTrainerPanel cal-1 cal-2 cal-3
  cal-4 cal-5 camshot-baseline camshot-lever cap-verify cas-AssetTypes
  cas-BandDirector-range2 caseb-validate-lane cbuild chaincensus-xenon charbones-diamond charclip-fix
  charfaceservo2 chartail classA-AppLabel classA-GemManager closeout10-b1-basematerial-exec closeout12-e2-vector-band
  closeout12-g1-fresh12 closeout12-s5-shrink-structs closeout13-g1-metaperformer closeout13-g3-charcreator-tour closeout13-g5-tourpractice closeout19-r9-accomp-gem
  closeout20-rc1-rockcentral closeout20-t1-trackpanel closeout20-v3-vocalplayer closeout27-p1 closeout34-merge-jefftest closeout37-cls
  closeout3-g1_vocalplayer closeout3-g2_player_campaign closeout3-g3_netsync_editsetlist closeout3-g4_bandsongmgr_musiclib closeout3-g5_netgamemsgs_accmgr_prefab closeout3-g6_songdb_gemtrack
  closeout3-g7_scattered_named closeout3-g8_engine_hi closeout3-pins closeout4-m1_game closeout4-m2_engine_a closeout4-m3_engine_b
  closeout4-p1-interp-sort closeout4-p2-repairs closeout4-w1-rbtree closeout4-w2-trackdir closeout4-w3-stream closeout4-w4-paneldir
  closeout5-g5-tour-upgrade closeout5-g7-engine-b closeout6-w3-cameramanager closeout7-c1-cameramanager closeout7-st1-struct-shrink closeout8-bp2-synth-triage
  closeout8-n1-netcache-serverdata closeout9-r1-render-base-64 clustext corr2 correlator-sizing corr-r10
  corr-r11 corr-r6 corr-r7 corr-r8 corr-r9 corrscale
  cpush-0 cpush-1 crackrec cr-bitmap cr-campaign cr-charclip
  cr-faders cr-filemerger cr-haqmgr cr-midireader cr-moggclip cr-muslibstore
  cr-postproc cr-prefabmgr cr-rndmat cr-rockcentral cr-sfx cr-songinfo
  cr-timer cr-typeprops d4140b8 dancerseq-fix datasym-renamer dc3-engine-pin
  dc3-fuzzy-drain debug-spike depthbuffer dxrnd ee-Anim ee-CharClipDisplay
  ee-CubeTex ee-Dir ee-DirLoader ee-EventTrigger ee-Geo ee-Group
  ee-LightHue ee-PropertyEventListener ee-Rnd ee-Rnd_Xbox ee-SpotlightDrawer_NG ee-Utl
  engext engine-reloc evalset eval-wt-frontier-0 eval-wt-frontier-1 eval-wt-frontier-2
  eval-wt-frontier-3 expose-harvest f88d1bb-baseline fa-GameMode fa-math-inline fa-NgPostProc
  fa-Player fa-RndTexRenderer fa-VocalPlayer fixcfg fixwave-0 fixwave-1
  fixwave-10 fixwave-11 fixwave-12 fixwave-13 fixwave-14 fixwave-15
  fixwave-16 fixwave-17 fixwave-18 fixwave-19 fixwave-2 fixwave-20
  fixwave-21 fixwave-22 fixwave-23 fixwave-24 fixwave-25 fixwave-26
  fixwave-27 fixwave-28 fixwave-29 fixwave-3 fixwave-30 fixwave-31
  fixwave-32 fixwave-33 fixwave-34 fixwave-35 fixwave-36 fixwave-37
  fixwave-38 fixwave-39 fixwave-4 fixwave-40 fixwave-41 fixwave-42
  fixwave-43 fixwave-44 fixwave-45 fixwave-46 fixwave-47 fixwave-48
  fixwave-5 fixwave-6 fixwave-7 fixwave-8 fixwave-9 fixwave-r-1
  fixwave-r-10 fixwave-r-11 fixwave-r-18 fixwave-r-19 fixwave-r-2 fixwave-r-20
  fixwave-r-21 fixwave-r-22 fixwave-r-24 fixwave-r-26 fixwave-r-28 fixwave-r-3
  fixwave-r-31 fixwave-r-36 fixwave-r-41 fixwave-r-45 fixwave-r-47 fixwave-r-48
  fixwave-r-6 fixwave-r-8 fixwave-r-9 flare-fix flowback flowif-fix
  flownode-fix flownode-mnode fm-inline fm-member fpc4-brv fpc4-prov
  fpc4-tpm fpspike fuzzy-strategy fuzzy-test fz-charclip g2-hamcamtransform
  g2-lightpreset g2-mesh g2-midiinstrument g2-postproc g2-vocalplayer gameport-consolidate
  game-reloc gemtrack_base gfi-residuals gp-BandUserMgr gp-CharData gp-Matchmaker
  gp-RealGuitarGemPlayer gp-TourDesc gp-TrainerPanel gp-ViewSetting gp-VocalPart grind-verify
  grind-w1-1 grind-w1-2 grind-w1-3 grind-w1-4 grind-w2-3 grind-w2-4
  group-base guardfix-base harvest-grind hmxobj intrin-sweep inverse-correlator
  jeffc13-c1 jeffc4 jeffleaf-census jeffverify laneA2-band laneA2-char
  laneA2-collide laneA2-hasvirt laneA2-misc laneA2r2-bmp laneA2r2-game laneA2r2-outfit
  laneA2r2-small laneA2r3-ctor laneA2r3-form laneA2r3-misc laneA2r3-sym laneA2r4-eng
  laneA2r4-game laneA2r4-net laneA2r4-tiny laneA2r5-eng laneA2r5-form laneA2r5-game
  laneA2-vocal laneAN-pdata laneAR-3 laneAR-4 laneAR-final laneAT-3
  laneAU-3 laneAU-4 laneAU-5 laneAV-1 laneAV-land laneAW-char
  laneAW-cheap laneAW-hamcam laneAW-mesh laneAW-oneinstr laneAW-srcmissing laneAW-sweep
  laneAW-unitsb laneAW-unitsc laneAX-7 laneM-pin laneQ-a laneQ-b
  laneQ-c laneQ-d laneQ-e1 laneQ-e2 laneQ-f laneQ-g
  laneT-binstream laneT-mkstr laneU-B laneU-C1 laneU-C2 laneU-C3
  leadb-a leadb-b leadb-c leadb-d lever3 lever4
  ls-v-0 ls-v-1 ls-v-10 ls-v-11 ls-v-12 ls-v-13
  ls-v-14 ls-v-15 ls-v-16 ls-v-17 ls-v-18 ls-v-19
  ls-v-2 ls-v-20 ls-v-21 ls-v-22 ls-v-23 ls-v-24
  ls-v-25 ls-v-26 ls-v-27 ls-v-28 ls-v-29 ls-v-3
  ls-v-30 ls-v-31 ls-v-32 ls-v-33 ls-v-34 ls-v-35
  ls-v-36 ls-v-37 ls-v-38 ls-v-39 ls-v-4 ls-v-5
  ls-v-6 ls-v-7 ls-v-8 ls-v-9 m1-bsmgr m1-metaperf
  m1-musiclib m4-base map0x1c-sweep matng-lever mdf2-tool mdgrind-wave
  mech-verify mech-w1-sym mech-w1-zs mech-w2-strA mech-w2-strB mech-w3-bp
  memcard-fix memfree memmgr meshanim-stride metapanel-structlever mf-A
  mf-B mf-C mf-D mf-E mf-F mf-G
  mf-H mf-I mf-J mf-K midiinstrument-fix midiparser
  misnest-fix moviepanel-fix nbfverify nm-bandhighlight nm-campaign nm-pitchcorrectedvoice
  nm-rndline nm-splicekeys nm-tourprogress objcache-dev objcache-probe-1 objcache-probe-2
  objdir-boundary objdirptr-base objdirverify objectdir-fix objptr-migration objptr-relayout
  objsrc-unify oc-AccomplishmentProgress oc-CharClipGroup oc-MoggClip oc-NavListNode oc-SongInfoAudioType
  op-async op-keygen op-mic op-streamnull op-uiguide ownspan-wire
  p5w1-banddirector p5w1-banddirector-rev p5w1-game-keystone p5w1-game-keystone-rev p5w1-laneA p5w1-lightpreset-bandmachine
  p5w1-lightpreset-bandmachine-rev p5w2-banduser p5w2-banduser-rev p5w2-game p5w2-game-rev p5w2-songsort
  p5w2-songsort-rev p5w2-unicorn-nearmiss p5w2-unicorn-nearmiss-rev p5w3-gamemode p5w3-gamemode-rev p5w3-lights
  p5w3-lights-rev p5w3-loaders p5w3-loaders-rev p5w3-user p5w3-user-rev p5w4-objectdir
  p5w4-objectdir-rev p5w4-sweep p5w4-sweep-rev p5w5-sd1-rev p5w5-sd2-rev p5w6-anchors
  p5w6-bresidual p5w7-bodyport p5w7-matchmaker p5w7-permuter part pch-test-1
  pch-test-2 permute-1 permuter-scaled permute-run pf-CharUtl pf-GameMode
  pf-Gem pf-Geo pf-LightPreset pf-Locale pf-PostProcer pf-Rnd_Xbox
  pf-Utl pf-Wind pinext2-game pinext2-misc pinext2-mp1 pinext2-mp2
  pinext2-multi pinext2-tour pmsg3-base port-checksum port-dupobj port-gamemode2
  port-gemplayer port-metaband port-netsync port-rockcentral port-smalltus port-songdb
  port-tourpanels port-vocal port-vocal2 port-vocaltrack ppcamshot ppgp
  ppmat probe-cubetex probe-tw r2synth-recon r3 r3review-scratch
  rcshrink recarve/waypoint-cpp recover-h refill-sweep refilltool rnd-class
  rndenviron-reconstruct rndmat-structlever safegate scal-1 scal-2 scal-3
  scal-4 scal-5 scal-verify screen-bld shader-nm showinc-test
  sliver-relocs songpos-plus4 songsort splitext split-mispin-fixes sr-CameraShot
  sr-CharSleeve sr-SampleData sr-UIListDir ssrecon-standardstream stlmap-ab store-tu-base
  strip-mp strip-tour structdb-ifdef structural-readiness sv-blind sv-ext
  sv-eyedart sv-groups sv-mic sv-r2-0-bitmap sv-r2-1-postproc sv-r2-2-propanim
  sv-r2-3-renderstate sv-r2-4-calibrationpanel sv-r2-5-charikhead sv-r2-7-soundtouch sv-r2-8-charbonedir sv-relocs2
  sv-songdb sv-songmgr sv-spotlight sv-tail sv-texblender sv-tool
  sv-trims sv-uiguide sv-waypoint sweep-2 sweep-4 sweep-asrt
  sweep-cm sweep-ui synth-nm tail-w3a tail-w3c tail-w7
  tail-w8 test-infra-throwaway test-infra-throwaway2 test-infra-throwaway3 tmpmain topo-locator
  tourperf-fix trainer-baseline uc-BandSongMgr uc-BandUser uc-BandUserMgr uc-baseclass
  uc-GemManager uc-GemPlayer uc-GemRepTemplate uc-gemtail uc-Performer uc-ProfileMgr
  uc-TourProgress uc-VocalPart uicomp2-fix uicomponent-fix uicomp-repin uicomp-virtuals
  uilistdir-region uilistprovider uilist-repin uislider-fix uwire-sweep verify-d1
  verify-f1 verify-p2 verify-synth verify-uw verify-w1 verify-w2
  verify-w3 vf3 vnl-port vtable-lever w13-A-contpin w13-B-bandsong
  w13-C-scatter w13-D-contpin w2-datareadstream w2-drumfill w2-gemwidget w2-httpsend
  w2-joypad w2-nodevmsg w2-savepre w2-streamrecv w36-merge-j2val w3-calcxfm
  w3-groupowner w3-nextbuf w3-patchload w3-poolalloc w3-trackpoll w6d-nbf
  w6verify w8-aih-range w8t-demo w8t-mine wave1-d1 wave1-d2
  wave1-f1 wave1-f2 wave1-f3 wave1-f4 wave1-s1 wave1-s2
  wave1-s3 wave2-clamp-test wf-perm wibo-verify wire-missing wl-wired-drain
  worktree-agent-a280f3789c8ea2134 worktree-agent-a351d8a14c20f0199 worktree-agent-a8b556a2739cecead worktree-wf_69e2da3d-bb1-1 worktree-wf_69e2da3d-bb1-2 worktree-wf_69e2da3d-bb1-4
  worktree-wf_69e2da3d-bb1-5 wt-bc-land wt-diag wt-instr-patcher wt-pair-experiment wt-pair-pipeline
  wt-pin-fix wt-verify wt-w6d-aih wt-w6d-pr wt-worktree-1779849700 wt-worktree-1779849705
  wt-worktree-1779849747 wt-wt-ce wt-wt-sn xfer-loop zs-instantiation 
```
