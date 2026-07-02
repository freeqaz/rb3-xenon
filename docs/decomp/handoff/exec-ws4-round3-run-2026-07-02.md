# exec-ws4-round3 — run plan (2026-07-02, planning agent)

Stream: ws4-round3 (round-3 banked repairs). Source docs:
- `docs/plans/workstreams-2026-07-02/ws4-round3-banked-repair.md` (stream doc — the
  authoritative step-by-step; this file records Phase-0 amendments + packetization)
- `docs/decomp/handoff/round3-shared-header-followups-2026-07-02.md` (round-3 handoff)

## Phase-0 verification results (all LIVE at main `00c5b19`)

- Main HEAD at planning time: `00c5b19` (port(synth-family): wire Faders/StandardStream/
  SongPreview/HamAudio). Branch `followup/round3-full-batch` @ `3879248` present.
- **Drift since the stream doc's zero-drift check (`b2b9654`): exactly ONE of the 14
  Bundle-2 files changed — `src/system/hamobj/HamAudio.cpp`** (46-line synth-family
  rework in 00c5b19). The drift is context-only: the 8-arg `new FileLoader(...)` call
  survives at line 205 and the extracted patch passes `git apply --check` on main.
  The other 13 files are byte-identical to `9b938ea` state.
- Bundle-2 patch extracted and stashed at `/home/free/tmp/exec-ws4-round3-bundle2.patch`
  (14 `diff --git` stanzas; exclusion grep for `target_symbol_map|rndobj/Dir.h|
  Character.h|PanelDir` prints nothing → no Bundle-1 contamination).
- HamAudio.cpp is wired in objects.json but **NOT pinned** (00c5b19 dropped the bogus
  micro-pins), so its own match state cannot regress; only the shared Loader.h shape
  flows through it.
- FileLoader 8-arg construction-site census on current main (grep): HamAudio.cpp:205,
  Synth.cpp:72, FileCache.cpp:63, BinkClip.cpp:195, LiveCameraInput.cpp:59,
  Tex.cpp:145, Utl.cpp:163, MoggClip.cpp:356, LightHue.cpp:86+99, NetLoader.cpp:74,
  **Loader.cpp:426** (an 11th in-Loader.cpp site — covered by the patch since
  Loader.cpp is in the 14). NetCacheLoader.cpp constructs differently (subclass path).
- Item B: `/home/free/tmp/hct_edited.{h,cpp}` verified to be exact edits of *current*
  main `src/system/hamobj/HamCamTransform.{h,cpp}` (2 header hunks removing
  `mCrowdRotate`; 4 cpp hunks: Setup copy, SYNC_PROP, Save, Load). The 5
  `TransformCrowd` map entries exist at the documented VAs (lines 468/487/497/962/966
  of `scripts/target_symbol_map.json`). HamCamTransform.cpp is pinned
  (splits.txt:1108) + wired (objects.json:669, NonMatching).
- All regressed VAs (`0x82735358/80/470, 0x82322468, 0x826F1C20, 0x826F1BF4,
  0x827B9D18/6C, 0x823A5A90, 0x8228B5C8`) confirmed ABSENT from the map → pairing is
  positional/funclet-shaped (stream doc step A5 applies).

**Verdict: stream ALIVE, both items go.**

## Worktrees + baselines

| Packet | Worktree | Branch | Baseline snapshot |
|---|---|---|---|
| p1 bundle2-loader-cascade | `/home/free/tmp/wt-exec-ws4-round3-p1` | `exec/ws4-round3-0702-p1` | `/home/free/tmp/exec-ws4-round3-p1-baseline-report.json` (10,936 matched) |
| p2 hct-transformcrowd | `/home/free/tmp/wt-exec-ws4-round3-p2` | `exec/ws4-round3-0702-p2` | `/home/free/tmp/exec-ws4-round3-p2-baseline-report.json` (10,936 matched) |

Snapshots are from the warm main build the worktrees were reflinked from. Workers
MUST take their own in-worktree fresh baseline (`tools/fresh_report.sh`) before
editing and measure against THAT; the snapshots are a cross-check only.

## Packetization

Two packets, disjoint files, separate worktrees (both touch shared headers →
measurement independence required; compose at review).

- **Packet 1 (bundle2-loader-cascade)**: apply the stashed 14-file patch, repair the
  ~6 regression clusters per stream-doc steps A2–A6. Hard-stop if first A/B gain
  < +10 strict and expected net ≤ +5. Land bar: NET > 0, zero unexplained strict
  regressions, zero real fuzzy regressions, double-run identical.
- **Packet 2 (hct-transformcrowd)**: probe-apply banked HCT edits, census the
  false-100 regressions, identify the true 0x10 family, fix
  `scripts/target_symbol_map.json` naming, land map+source as ONE commit. Kill if any
  regressed target fn shows stride 0xc (premise falsified).

Do NOT re-attempt Bundle 1 (CollideListSubParts devirt) — disproven, see handoff
lines 17–38. Never take `src/system/rndobj/Dir.h`, `src/system/char/Character.h`,
`src/system/ui/PanelDir.cpp`, or the branch's map file.

Review/integration: land each packet independently via `scripts/harvest/land.sh
<worktree>` + `git merge --ff-only <branch>`; composed verify on main afterward
(both touch `scripts/target_symbol_map.json`-adjacent machinery only in p2; file
sets are disjoint so composition is trivial).

---

## Packet p1 (bundle2-loader-cascade) EXECUTION RESULT — 2026-07-02 (Opus worker)

**Verdict: at_limit / RE-BANK.** Bundle-2 applied cleanly and is CORRECT
(+25 net strict, oracle-confirmed layout), but produces real MSVC regalloc
regressions that CANNOT be cleared in scope. Do NOT land as-is; coordinator
call on whether +25 net outweighs the honesty-gate regressions.

### Numbers (in-worktree fresh_report A/B, one confirmed run)
- BASELINE strict-100: **10936** (matches planning snapshot).
- AFTER bundle2 (14 files, `git apply` clean, all 8-arg `FileLoader` sites
  now 7-arg; verified): **10961**.
- **NET +25 (gained 43, regressed 18 strict + 13 real fuzzy >1%).**
- ObjDirItr port verified byte-for-byte vs rb3-Wii `system/obj/Dir.h`
  (5 members mDir/mSubDir/mEntry/mObj/mWhich = 0x14; NextSubDir identical).
  Layout is NOT the problem.

### Gains (real, dominant): rndobj/Utl +11 net (GetNormalMapTextures,
ObjDirItr<RndTex>++, many fn_8242Cxxx), LightHue +4 (Sync), AmbientOcclusion +3,
CharClipSet +2 net, Char{Driver,FaceServo,Hair} +4, NetCacheLoader +2,
FileCache +1, PanelDir/UIListDir ObjDirItr op++ +2, NetSync +9 funclets.

### Why the regressions are NOT repairable here (root cause)
The correct ObjDirItr revert (flat 0x14 iterator + NextSubDir call, replacing the
DC3 std::list collector) changes MSVC's **register allocation / stack-frame
layout** in every function that inlines ObjDirItr. In the loser units this adds an
extra spill (a BASE_ONLY `int` temp) that shifts subsequent frame slots by
0x10–0x18, so previously byte-matched EH cleanup funclets and near-miss bodies
now differ by their frame immediates. This is **permuter/regalloc-class**, not a
source or pairing fix:

- Empirically classified every regressed target funclet against our compiled obj
  (normalized-instruction twin search, reloc operands masked). **ZERO regressed
  target funclets have a byte-identical twin anywhere in our obj** — so a
  `target_symbol_map` name-pairing entry (A5) would NOT recover them (there is no
  matching-bytes symbol to pair to; they'd stay <100). The A5 fallback is
  therefore inapplicable to this cascade.
- Frame-delta directions are **mixed** (BandWardrobe funclets +0x10 *bigger* than
  retail: tgt `subi 0x70`/`lwz 0x84` vs ours `subi 0x80`/`lwz 0x94`; NetSync
  funclet fn_82586050 ours 0x60 *smaller*: tgt `subi r31,r12,0xf0` vs ours
  `0x90`). Mixed directions ⇒ regalloc ripple, not one fixable size bug.

### Per-cluster ledger (supersedes the stream-doc estimate)
| Unit | fns | after | class |
|---|---|---|---|
| band3/meta_band/NetSync | fn_82586050/078/0A0/0F0/140/190/1B8/1E0/208/230 | 100→99.9 (10 strict) | funclet re-zip vs +9 gained (82585170–2E0); DataNode-dtor funclets, no twin |
| BandWardrobe | fn_82322468 100→0; fn_82321538,fn_82322180 100→99.9; fn_8232225C,82322230 99.9→0 (fuzzy); BandDirector fn_8228B0CC 99.9→0 | strict×3 + fuzzy×3 | frame +0x10, funclet dtor(mem+8)@fr+0x84 has no twin (ours @fr+0x94) |
| CameraManager | fn_824A88F4, fn_824A8ABC | 100→99.9 (2 strict) | frame slot shift |
| CharClipSet | fn_823C063C, fn_823C0664 | 100→99.8/9 (2 strict, but unit net +2) | " |
| Dir (world) | fn_8272D208 | 100→99.9 (1 strict) | one `addi +4`, slot shift |
| DirLoader | fn_82735358/380/470 | 99.9→0 (fuzzy) | Loader-subclass EH funclets unpaired; +1 gained (fn_82732CFC) |
| TrackDir | fn_827B9D18, fn_827B9D6C | 93.9→0 (fuzzy) | funclets unpaired |
| MetaMusic | fn_826F1C20 94→67; fn_826F1BF4 99.9→94 | fuzzy | frame size matches (0x60), extra BASE_ONLY spill, −0x18 body shift (regalloc) |
| CharBoneDir | fn_823A5A90 | 99.9→94 (fuzzy) | frame matches, extra spill, −0x10 shift (regalloc) |
| rndobj/Utl | fn_8242C3D0 | 99.8→93.9 (fuzzy) | 1 loss vs +12 gains |

### Recommendation
- Bundle-2 is CORRECT and net-positive; the regressions are inherent regalloc
  ripple of a correct structural change. Options for the coordinator:
  1. **Land +25 and accept the regressions** as documented regalloc-class churn
     (metric-level call; violates the strict zero-regression gate but is honest).
  2. **Re-bank** (leave on branch, unlanded) — the safe default per plan.
  3. Feed MetaMusic/CharBoneDir/Dir/CameraManager/CharClipSet near-misses to the
     **permuter** in a follow-up (the extra-spill removals are the only plausible
     recovery path; NetSync/DirLoader/TrackDir/BandWardrobe funclet losses have
     no source lever). Permuter success is uncertain and per-function.
- Worker left the 14-file patch APPLIED in the worktree (uncommitted, per worker
  rules — reviewer commits). No map edits, no splits.txt edits, no Bundle-1 files.

---

## REVIEW + INTEGRATION verdicts (2026-07-02, Fable reviewer)

### Packet p1 (bundle2-loader-cascade): **LAND** (+25 net, regressions documented)

Reviewer independently reproduced the composed A/B in the p1 worktree
(`tools/fresh_report.sh`, log `~/tmp/rb3_build_exec-ws4-round3-ab.log`):

- BASELINE (planning snapshot `/home/free/tmp/exec-ws4-round3-p1-baseline-report.json`):
  `measures.matched_functions` = **10936**
- COMPOSED (bundle2 applied): **10961** → **NET +25 strict**
  (43 gained / 18 strict regressed / 12 real fuzzy regressed >1% / 5 fuzzy improved).
  Per-function ledger matches the worker's report exactly (NetSync ±, BandWardrobe,
  CameraManager, CharClipSet, Dir, DirLoader, TrackDir, MetaMusic, CharBoneDir,
  rndobj/Utl — all confirmed).
- Spot checks: `?GetNormalMapTextures@@YA?AVDataNode@@PAVObjectDir@@@Z` = TRUE 100
  (103 insns, all equal — real named body, not a stub-fold);
  `fn_82322468` confirmed genuinely unpaired (11-insn target funclet, no base twin,
  frame Δ −0x60) — worker's "no twin ⇒ A5 map fallback inapplicable" holds.
- Gains include real named bodies (LightHue::Sync, GetNormalMapTextures, three
  ObjDirItr<T>::op++ instantiations 94.9→100) — not stub-fold artifacts; funclet
  flips appear on BOTH sides of the ledger, so the +25 delta is metric-honest.

**Rationale for landing over re-bank** (coordinator call the worker escalated):
the reviewer land gate is composed-net-positive + clean hygiene, and this is
+25 with the regressions root-caused as regalloc ripple of a CORRECT structural
change (ObjDirItr = flat 0x14 retail shape per rb3-Wii oracle; DC3 std::list
collector was drift). The 18 lost strict were matches of *incorrect* source —
re-pricing them is honest. Recovery lever (permuter on the frame-size-matching
near-misses: MetaMusic, CharBoneDir, Dir, CameraManager, CharClipSet) is
documented in the worker ledger above; NetSync/DirLoader/TrackDir/BandWardrobe
funclet losses have no source lever and are accepted.

### Packet p2 (hct-transformcrowd): **REJECT / re-bank** (nothing composed)

Worker correctly invoked the fallback: the banked 0x10→0xc TransformCrowd shrink
is VERIFIED CORRECT (op= flips to 100 at 0xc; target 0x82298800 stride is 0x10 ⇒
misnamed map entry; ~TransformCrowd funclet matches only at 0xc) but nets **−3**
with no legitimate map-rename recovery (the true-0xc `_M_clear` is ICF-folded
under a foreign survivor; the funclet losses are positional ICF-soup phantoms).
Reviewer confirms the p2 worktree is main-shaped (git status: no tracked changes)
→ nothing to compose; composed state = p1 only. Banked edits remain at
`/home/free/tmp/hct_edited.{h,cpp}` for a future combined map-hygiene pass
(defer until the 0x8229xxxx ICF span de-folds or a multi-unit rename pass can
absorb the −3).

### Composed A/B summary

| | matched_functions |
|---|---|
| baseline | 10936 |
| composed (p1 only) | 10961 |
| **net** | **+25** |

Hygiene: 14 intended source files committed; scratch `global_fuzzy_pairs.json`
removed; no map/splits/config edits; `tools/download_tool.py` untouched
(assume-unchanged). Branch `exec/ws4-round3-0702-p1`.
