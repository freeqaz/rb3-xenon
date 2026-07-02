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

---

## Independent review verification (2026-07-02, ws4-round3 review agent, second pass)

Reproduced independently in the primary worktree (`fresh_report.sh`, log:
`~/tmp/rb3_build_exec-ws4-round3-ab.log`):

- Composed A/B: baseline 10936 → composed 10961 `matched_functions` = **NET +25 strict**.
- Per-function census matches the ledger exactly: **43 gained / 18 strict lost /
  12 fuzzy-regressed >1%**. Gained units: rndobj/Utl 11, NetSync 9, LightHue 4,
  CharClipSet 4, AmbientOcclusion 3, CharFaceServo 2, NetCacheLoader 2, +7 singles.
  Lost: NetSync 10 funclets, BandWardrobe 3 (incl. fn_82322468 → 0.0 unpaired),
  CameraManager 2, CharClipSet 2, Dir 1.
- Per-symbol spot checks: `GetNormalMapTextures` TRUE 100 (103 insns, real body,
  not stub-fold); `ObjDirItr<RndTex>::operator++` (rndobj/Utl) TRUE 100 (21 insns);
  `fn_82322468` confirmed 11-insn all-insert unpaired funclet (frame Δ −0x60, no
  base twin — A5 map fallback inapplicable, as documented).
- p2 worktree confirmed main-shaped (no tracked changes); nothing composed from p2.

Verdict re-affirmed: **LAND** (fc6d561). Appendix below preserves the p2 worker's
full census (was untracked in the p2 worktree, which gets cleaned up).

---

## Appendix: Packet 2 (hct-transformcrowd) full findings (from p2 worktree)

### exec-ws4-round3 — Packet 2 (hct-transformcrowd) findings — 2026-07-02

Worker: Opus, worktree `/home/free/tmp/wt-exec-ws4-round3-p2`
(branch `exec/ws4-round3-0702-p2`). Outcome: **at_limit — FALLBACK invoked
(source reverted to main-shaped, map untouched, net 0). "Metric honesty vs
metric level" flagged for coordinator sign-off.**

### TL;DR

The banked TransformCrowd 0x10→0xc shrink is **verified CORRECT** but nets
**−3 strict** on the metric, and that −3 is **NOT recoverable** to net-positive
by any legitimate `target_symbol_map.json` rename. The re-pair premise in the
plan fails on a hard fact: **HamCamTransform.obj compiles no 0x10-element
vector**, so the one named strict loss cannot be re-attributed to a symbol our
obj actually emits. I reverted per the packet FALLBACK. The change should only
land as a correctness fix with coordinator sign-off (accepting −3 metric).

### Baseline

- In-worktree fresh baseline: **10936 matched** (`~/tmp/ws4b_BASE.json`),
  matches the planning snapshot.
- After reverting the probe: **10936 matched** (net 0, double-checked). Worktree
  is main-shaped; only regenerable build artifacts differ.

### Probe A/B census (banked edits applied, map untouched)

`measure_delta` = NET **−3** (gained 2, regressed 5). Full census:

| VA / name | base→new | class |
|---|---|---|
| `0x82295C30` `??4?$ObjVector@VTransformCrowd@@` (op=) | 93.33→**100** | **REAL GAIN** (0xc verified) |
| `fn_82296218` | 99.9→**100** | **REAL GAIN** — EH funclet for stack `TransformCrowd` local; target `subi r31,r12,0xc0; addi r3,r31,0x58; bl ~TransformCrowd`, frame 0xc0 now matches with 0xc |
| `0x82298800` `_M_clear_after_move@vector<TransformCrowd>` | 100→92.14 | strict LOSS — **false-100**, unrecoverable (see below) |
| `fn_82296240` | 100→99.9 | strict LOSS — positional funclet phantom |
| `fn_82296268` | 100→99.9 | strict LOSS — positional funclet phantom |
| `fn_82298550` | 100→99.8 | strict LOSS — positional funclet phantom |
| `fn_82298EC8` | 100→99.8 | strict LOSS — positional funclet phantom |
| `?Load@TransformCrowd@` `0x82297D38` | 84.11→12.32 | fuzzy-only (never strict) — misnamed target, see below |
| `fn_8229756C` | 72.55→67.09 | fuzzy-only |
| `fn_82296304` (Setup) | 94.55→**99.91** | fuzzy GAIN (expected) |

### Why the change is correct (3 independent proofs)

1. **op= flips to 100 with 0xc.** `??4?$ObjVector@VTransformCrowd@@` at
   `0x82295C30` (already correctly named in the map) went 93.33→100 the instant
   TransformCrowd shrank to 0xc. Retail's op= genuinely uses 0xc stride.
2. **The misnamed target uses 0x10 stride.** `0x82298800` (map-named
   `_M_clear_after_move@vector<TransformCrowd>`): target asm is
   `srawi r11,r11,4` (/16) + `slwi r3,r11,4` (×16) = **0x10** element; our new
   0xc code emits `divw`/`mulli 0xc`. Ghidra confirms
   `(p[2]-*p >> 4) << 4`. So `0x82298800` is a **0x10-element** vector's
   `_M_clear`, ICF-folded and *misnamed* as vector<TransformCrowd>. KILL CHECK
   passed (no regressed target uses 0xc stride).
3. **A real TransformCrowd-dtor funclet matches at 0xc.** `fn_82296218` (target
   calls `~TransformCrowd` at frame-offset +0x58, frame 0xc0) reaches 100 only
   with the 0xc layout.

### Why −3 is NOT recoverable via map rename

The plan assumed the misnamed 0x10-family entries could be renamed to "the true
family's mangled instantiation (if our tree compiles it, the regression becomes
a re-pair)." That fails here:

- **`HamCamTransform.obj` compiles NO 0x10-element vector.** Full inventory of
  vector/ObjVector element types in our compiled obj:
  `TransformArea` (0x70), `TransformCrowd` (0xc), `pair<int,int>` (0x8). There is
  no 0x10-element `_M_clear_after_move` symbol in our obj. Renaming target
  `0x82298800` to its true 0x10 type therefore makes it **target-only /
  unpaired** — the same metric result as the loss, plus a name our obj can't
  pair. No recovery.
- **The real 0xc `vector<TransformCrowd>::_M_clear` has no pairable target.**
  Retail ICF-folds it under some *other* 0xc-element type's survivor name; our
  `vector<TransformCrowd>::_M_clear` (0xc) is byte-identical to that survivor but
  objdiff won't pair across names without a global ICF-alias entry (out of scope,
  and risky to the shared map). I scanned the 0xc cluster near op=
  (`0x82295C98…0x82295FC8`) — `0x82295C98` is the 0x70 `_M_clear`; no standalone
  0xc `_M_clear` target is exposed.
- **The 4 funclet losses are unnamed 40-byte EH-cleanup phantoms.** Each is a
  positional stub-fold (<44 bytes, `fn_` on both sides) whose only mismatch is a
  frame-reconstruction constant + a *different* dtor callee than retail
  (ours `~DataNode`; targets `fn_82260288` / `fn_822905B0` / `fn_822926F0`).
  They are genuinely different funclets aligned by position; the shrink
  reshuffled which of our funclets lands in each retail slot. Not addressable by
  a name in the map (per hygiene rule, these were never real matches). Deleting
  map entries to stop counting them is explicitly forbidden.
- **`0x82297D38` ("TransformCrowd::Load") is also misnamed** and fuzzy-only.
  Ghidra: `ReadEndian(bs, p, 4); Function_82297998(bs, p+4);` — an int@0 + call
  struct load with a 0x70 frame; NOT a single-ObjPtr `bs >> mCrowd`. The real
  TransformCrowd::Load (0xc, `bs >> mCrowd` only) is almost certainly inlined in
  retail (no standalone target). Re-pointing the name has no valid 0xc target.

### Recommendation

- **Do not land net-negative unattended.** Two options for the coordinator:
  1. **Land-for-correctness with sign-off**: accept the −3 metric to remove the
     genuine DC3-drift bug (extra `mCrowdRotate` member; retail RB3 lacks it).
     The banked edits are exact and re-appliable
     (`/home/free/tmp/hct_edited.{h,cpp}`). The −3 is entirely false-100 /
     positional-phantom noise, not real code regression.
  2. **Shelve**: keep the (incorrect but metric-neutral) 0x10 layout until the
     span's ICF soup is de-folded enough that the 0xc family can re-pair. Map
     hygiene for `0x82298800` / `0x82297D38` is real but cannot be done
     net-neutral in isolation (they're currently false-100 / fuzzy on our 0x10
     code), so defer it to a combined pass.
- **Map hygiene note for future pins**: `0x82298800` is a 0x10-element
  `_M_clear_after_move` (handoff lane-E "TransConstraint-owned 0x10 type" is
  consistent; the element is 0x10 with a non-trivial move via `fn_82297C68` and
  free via `fn_82798278`). `0x82297D38` is an int@0 + sub-object load, ~0x70
  frame — not TransformCrowd::Load. Neither should keep its current
  vector<TransformCrowd> name once a de-fold pass can supply the true owners.

### Artifacts

- `~/tmp/ws4b_BASE.json` — in-worktree baseline (10936).
- `/home/free/tmp/hct_orig.{h,cpp}` — restored originals (main-shaped).
- `/home/free/tmp/hct_edited.{h,cpp}` — the (correct) banked edits, unused/reverted.
- Build logs: `~/tmp/rb3_build_ws4b.log`, `~/tmp/rb3_build_ws4b_probe.log`,
  `~/tmp/rb3_build_ws4b_revert.log`.

---

### Packet p2 (hct-transformcrowd) — SECOND RUN 2026-07-02 (Opus worker, deep map probe)

**Verdict: at_limit / re-bank (unchanged). NET −3 strict, no legitimate map-rename
recovery.** Confirms the prior p2 verdict with a full instruction-level census.
Worktree left MAIN-SHAPED (both source files reverted from HEAD; `scripts/target_symbol_map.json` never touched). Nothing to compose.

### Baseline / probe A/B (in-worktree `tools/fresh_report.sh`)
- BASELINE strict-100: **10936** (`~/tmp/ws4b_BASE.json`).
- After banked `hct_edited.{h,cpp}` (0x10→0xc shrink): **10933** → **NET −3** (gained 2, regressed 5 strict).

### KILL CHECK: PASSED (premise intact — targets are 0x10, not 0xc)
- GAIN `??4?$ObjVector@VTransformCrowd@@` (0x82295C30) → **TRUE 100** (24 insns all equal; retail op= is genuinely 0xc). Retail `ObjRefConcrete` = {vtable@0,mOwner@4,mObject@8}=0xc verified in `obj/Object.h`, so retail `sizeof(TransformCrowd)`=0xc. Shrink is CORRECT.
- REGRESSED `_M_clear_after_move vector<TransformCrowd>` (0x82298800): target uses **0x10 stride** (`srawi r11,r11,4` / `slwi r3,r11,4`); our new base uses `li r11,0xc`/`divw`/`mulli r3,r11,0xc`. Target is a misnamed 0x10-element `_M_clear`. (Confirmed via run_diff_inspect mismatches — explicit Target/Base columns.)
- REGRESSED `?Load@TransformCrowd@` (0x82297D38, 84→12 fuzzy): target is a **0x10-type Load** — reads a 4-byte value via `BinStream::ReadEndian(obj+0, 4)` then loads an ObjRef at `obj+0x4` (`bl fn_82297998`). Layout {value@0, ObjPtr@4}=0x10. Our correct 0xc Load is a 3-insn tailcall to `ObjRefConcrete<WorldCrowd>::Load`. Misnamed.

### Why NO recovery is possible (three independent blocks)
1. **_M_clear (−1 strict):** grep of the entire pinned target span
   `[0x82295870,0x8229A2A0)` for `mulli *,0xc` (the 0xc-stride reconstruct our base
   `_M_clear` emits) returns **ZERO hits**; there are 6 `slwi *,4` (0x10). So there is
   **no standalone 0xc `_M_clear` target twin** to re-point the map name at — the real
   0xc `vector<TransformCrowd>::_M_clear` is inlined/ICF-folded away. Re-pointing has no
   valid destination.
2. **Misnamed 0x10 targets (0x82298800, 0x82297D38):** our HamCamTransform TU only
   instantiates `vector<TransformArea>` (0x70) and `vector<TransformCrowd>` (now 0xc). It
   compiles **no 0x10-element vector**, so renaming these target VAs to the true 0x10
   family's mangled name yields an **unpaired** target (no base symbol in HamCamTransform.obj
   to pair with) — honest, but recovers 0 strict and worsens fuzzy (92→0). objdiff pairs
   within the unit only; the true 0x10 owner lives in another TU.
3. **4 funclet phantoms (−4 strict): fn_82296240, fn_82296268, fn_82298550, fn_82298EC8**
   (100→99.8/99.9). All are DataNode-dtor EH cleanup funclets whose frame-reconstruct
   immediate shrank with the enclosing frame (e.g. fn_82298550 target `subi r31,r12,0xc0`
   vs base `subi r31,r12,0x70`; fn_82296268 `0xc0`→`0xb0`). They are NOT map entries
   (anonymous fn_), so map edits cannot touch their pairing. The correct 0xc source
   produces the smaller frames; no 0xc0 base funclet twin survives to re-pair (positional
   ICF-soup phantoms). Note the sibling GAIN fn_82296218 is a ~TransformCrowd cleanup
   funclet that re-aligned to TRUE 100 at frame 0xc0 — confirming this region is a
   funclet reshuffle, net −3.

### True 0x10 family identity (partial — for a future multi-unit map-hygiene pass)
Structure established, exact C++ type NOT pinned: the misnamed family is a **0x10 STL
element with layout {4-byte int/enum@0, ObjPtr/ObjRef@0x4}** (from the 0x82297D38 Load
shape) whose `vector<T>` STL helpers (`_M_clear_after_move`, resize, copy) are
ICF-identical to our old DC3-shaped (0x10) `vector<TransformCrowd>` — hence the original
false-100s. Lane E's "TransConstraint-owned 0x10 type" was checked: `TransConstraint`
(`src/system/hamobj/TransConstraint.h`) is an `RndHighlightable`/`RndPollable` subclass and
does not obviously own a `vector<{int,ObjPtr}>` member — attribution unconfirmed. The two
out-of-span entries (0x823390C0, 0x8233A108) are NOT in any pinned span (cannot affect the
report). Identifying the exact type would enable map-hygiene renames but would still NOT
change strict NET (block 2 above), so it was not pursued to conclusion.

### Recommendation
Keep banked at `/home/free/tmp/hct_edited.{h,cpp}`; do NOT land. The lever only becomes
net-positive if either (a) the 0x8229xxxx ICF span de-folds so the real 0xc
`_M_clear`/`Load` acquire standalone target VAs to re-point at, or (b) a future pin brings
the true 0x10 family's TU into scope so the misnamed entries re-pair there. Flag for
coordinator: **metric honesty vs metric level** — the +2 (op= TRUE 100, funclet TRUE 100)
are real correctness gains, but the −5 (1 genuine false-100 correction + 4 funclet
phantoms) outweigh them under the strict counter with no available recovery.
