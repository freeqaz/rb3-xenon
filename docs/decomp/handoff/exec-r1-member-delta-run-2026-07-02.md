# exec-r1-member-delta — run plan (2026-07-02)

Stream: ws7 reopen **R1 — member-delta apply mini-wave** (stream doc:
`docs/plans/workstreams-2026-07-02/ws7-dead-lever-reaudit.md`, Part 1 R1).
Planner: Fable. Workers: Opus, one packet each. Reviewer composes at the end.

## Phase 0 (done by planner, all PASS)

- Audit artifact `~/tmp/ws7-audit/mdf2_2026-07-02.json` **exists** (no regen
  needed) with exactly 4 MEMBER_DELTA candidates: GemPlayer −0x10, BinkClip
  +0x4, OvershellSlot −0x8, CameraManager −0x30.
- All 4 signals **re-fire live on today's main** (00c5b19, baseline
  10,936 matched fns), verified by per-symbol objdiff:
  - `?PlayMissSound@GemPlayer@@QAAXH@Z` → 1 diff_arg `lbz [off:+16]`
  - `?Stop@BinkClip@@QAAXXZ` → 1 diff_arg `lbz [off:-4]`
  - `?IsValidUser@OvershellSlot@@QBA_NPAVBandUser@@@Z` → 4× `lwz [off:+8]`
  - `?DeleteFreeCam@CameraManager@@QAAXXZ` → `lwz/stw [off:+48]`
- Stream proceeds. No skip.

## Sign convention (memorize)

objdiff `off:+N` = OUR compiled offset is N bytes HIGHER than retail →
our header has EXTRA bytes below the boundary → **remove/shrink N bytes**.
`off:-N` = ours LOWER → retail has a member we lack → **add N bytes**.

## Worktrees + baselines (already created, warm-cache, branch names shown)

| packet | worktree | branch | baseline snapshot |
|---|---|---|---|
| primary/doc | `/home/free/tmp/wt-exec-r1-member-delta` | `exec/r1-member-delta-0702` | `~/tmp/exec-r1-member-delta-baseline-report.json` |
| p1 GemPlayer | `/home/free/tmp/wt-exec-r1-member-delta-p1` | `exec/r1-member-delta-0702-p1` | `~/tmp/exec-r1-member-delta-p1-baseline-report.json` |
| p2 BinkClip | `/home/free/tmp/wt-exec-r1-member-delta-p2` | `exec/r1-member-delta-0702-p2` | `~/tmp/exec-r1-member-delta-p2-baseline-report.json` |
| p3 OvershellSlot | `/home/free/tmp/wt-exec-r1-member-delta-p3` | `exec/r1-member-delta-0702-p3` | `~/tmp/exec-r1-member-delta-p3-baseline-report.json` |
| p4 CameraManager | `/home/free/tmp/wt-exec-r1-member-delta-p4` | `exec/r1-member-delta-0702-p4` | `~/tmp/exec-r1-member-delta-p4-baseline-report.json` |

All baselines = 10,936 matched_functions. Each packet builds and measures ONLY
in its own worktree (header levers cascade; measurement independence required).

## Packets (details in the machine-read packet JSON; summary here)

- **p1 GemPlayer −0x10 @ ~0x400 (HIGH conf, run first).**
  `src/band3/game/GemPlayer.h`. Remove 16 bytes somewhere in (0x27c, 0x400).
  Prime suspect: the `unk390/unk394/unk398/unk39c` block (exactly 16 bytes).
  Oracle: rb3-Wii (`lookup_rb3wii GemPlayer`).
- **p2 BinkClip +0x4 @ 0x4c (MEDIUM conf).** `src/system/synth/BinkClip.h`.
  Add a 4-byte member before `mLoader // 0x4c` (i.e. after `mSize // 0x48`).
  Oracle: DC3 first (`lookup_dc3 BinkClip`), engine class.
- **p3 OvershellSlot −0x8 @ 0x34 (LOW conf, coupled-base warn — recon first).**
  `src/band3/meta_band/OvershellSlot.h`. Remove 8 bytes below 0x34
  (candidates in `mStateMgr 0x1c .. mOvershell 0x30` region, e.g. `unk28` + one
  pointer). Oracle: rb3-Wii.
- **p4 CameraManager −0x30 @ ~0x28 (LOW conf, coupled-base warn — recon first).**
  `src/system/world/CameraManager.h`. Mixed deltas (−48 @ 0x28, −36 @ 0x8)
  suggest cumulative multi-member oversize (DC3-fat `ObjPtr`/`ObjPtrList`?) or a
  base wall. Oracle: DC3 first, cross-check rb3-Wii.

## Per-packet protocol (identical for all)

1. Recon in own worktree: `run_objdiff` (project_dir=<worktree>!) per listed
   method, `full_listing=true` to bracket the boundary with MATCHED accesses;
   `run_diff_inspect mode=mismatches` for the full shifted-offset table.
2. Oracle cross-check the member identity BEFORE editing.
3. Minimal header edit; update trailing `// 0xNN` offset comments.
4. Per-symbol gate: listed methods must flip to strict 100 (normalized 100 with
   only anon-reloc naming residue is acceptable, note it).
5. Whole-binary A/B in own worktree:
   `./tools/ninja-locked 2>&1 | tee ~/tmp/rb3_build_r1pN.log`, then diff
   `build/45410914/report.json` matched_functions + per-unit regression list
   against the packet baseline snapshot.
6. Report: net strict delta, regressed units (must be 0), files touched.

## Bars

- Packet success: net ≥ +2 strict, 0 regressions. Packet kill: net ≤ +1,
  any regression, or recon shows vbase/coupled-base wall (document evidence).
- Stream success (composed at review): **≥ +6 strict net** across the 4.
- Stream kill (reviewer): net ≤ +1 after all 4 verified A/Bs, OR ≥2 of 4 recon
  as coupled-base walls → close member-delta permanently per ws7 R1.
- Regardless of outcome: institutional fix = add finder re-runs to the
  post-refill checklist (ws7 R1 note).

## Forbidden

Owner-WIP TUs: Band, Track, Game, Tracker, BandProfile, GemTrainerPanel,
ClosetMgr, GemSmasher, BandTrack (.cpp/.h). GemPlayer.{cpp,h} is explicitly OK.
Never touch the main repo working tree; never stash/checkout files there.

## REVIEW VERDICTS (reviewer, 2026-07-02)

All per-symbol claims independently reproduced via MCP run_objdiff with
project_dir = each packet's own worktree. Stub-fold guard applied: every gated
function is 16-54 instructions (64-216 bytes), well above the <=44-byte ICF
stub-fold threshold — all real matches.

| packet | verdict | net | evidence |
|---|---|---|---|
| p1 GemPlayer −0x10 | **ACCEPT** | +3 strict | PlayMissSound 100.0 norm (98.0 raw), LocalSoloStart 100.0 norm (99.5 raw), HandleFirstGemAfterRollback TRUE 100 (raw+norm). Root cause verified by worker via retail disasm: phantom is the *used* Wii guitar-FX-core block (unk39c/unk3a0/unk3a4-a8, 16 B) — Xbox routes FX via mPitchShift. Deviation from "pick an unused block" justified; .cpp deletions are Wii-only code in UNPINNED fns, A/B shows 0 regressions. |
| p2 BinkClip +0x4 | **ACCEPT** | +2 strict | Stop 100.0 norm (99.4 raw, anon-reloc residue), KillStream TRUE 100, SetLoop 88.3→88.4 with both member diff_args resolved (residual = unrelated SetJump(kStreamEndMs) codegen, out of scope). New `int mUnk3c // 0x3c`; member is retail-360-only (absent in both rb3-Wii and DC3 — DC3 has no BinkClip at all). |
| p3 OvershellSlot −0x8 | **KILL (coupled-code)** | 0 | Recon reproduced (+8 uniform above 0x30, mStateMgr/mState boundary MATCH). All 3 drop candidates (mOverrideFlowReturnState, unk28, mUserNameLabel) are referenced by the compiled OvershellSlot.cpp; dropping any 2 breaks the TU. Requires a Wii→retail-360 body port with no retail oracle. No edit made. |
| p4 CameraManager −0x30 | **KILL (coupled-base)** | 0 | Staircase +36/+48 = DC3 Object-base promotion (+0x24) + blend-block insert (+12). Retail CameraManager is a non-Object class (rb3-Wii header offsets match retail exactly). De-Object-ifying = architectural rewrite rippling into WorldDir by-value embed. No edit made. |

### Composed A/B (primary worktree, full ninja build, log ~/tmp/rb3_build_exec-r1-member-delta-ab.log)

- matched_functions **10,936 → 10,941 (net +5 strict)**, **0 regressions**.
- Per-unit deltas: GAIN default/GemPlayer 10→13, GAIN default/BinkClip 0→2. Nothing else moved.
- Composed = exact sum of p1+p2 packet A/Bs (no interaction).

### Stream-bar assessment

- Stream success bar was ≥ +6 strict; composed result is **+5** — just under
  the bar, but net-positive with zero regressions, so the surviving packets
  LAND. The shortfall is entirely the two coupled walls, not measurement
  optimism.
- ws7 R1 close-out: 2 of 4 candidates reconned as walls (coupled-code +
  coupled-base). Per the stream-kill rule, **member-delta as a standing lever
  is now CLOSED** — the mdf2 finder's four candidates are fully dispositioned
  (2 landed, 2 killed with reproducible evidence). Re-run the finder only
  after the next ≥ +500 strict refill per the institutional rule.

## RE-RUN VERIFICATION (second dispatch, reviewer 2026-07-02 late)

A second worker wave was dispatched against this stream after the first-run
result had already landed on main as **ad2daa5** ("lever(member-delta R1):
GemPlayer −0x10 + BinkClip +0x4 (+5 strict, 0 regressions)"). All four
workers correctly identified the situation and made **zero edits**; this
reviewer independently re-reproduced every claim (MCP run_objdiff,
project_dir = each packet worktree at main head c5632f9; stub-fold guard
applied — every gated fn is 16-54 instrs, 64-216 bytes, named symbol pairs):

| packet | re-run verdict | reproduced |
|---|---|---|
| gemplayer-m10 | **DUPLICATE / already landed (ad2daa5)** | PlayMissSound 100.0 norm (98.0 raw, 54 eq), LocalSoloStart 100.0 norm (99.5 raw, 33 eq), HandleFirstGemAfterRollback TRUE 100 (25 eq) |
| binkclip-p4 | **DUPLICATE / already landed (ad2daa5)** | Stop 100.0 norm (99.4 raw, 16 eq), KillStream TRUE 100 (23 eq); measured vs main (p2 worktree pruned post-first-review) |
| overshellslot-m8 | **KILL re-confirmed (coupled-code)** | IsValidUser 99.9 (4× lwz off:+8), GenerateCurrentState 99.9 (5× off:+8), LookupUserInJoinList 99.9 (2× off:+8) — unchanged, no compilable drop exists |
| cameramanager-m30 | **KILL re-confirmed (coupled-base)** | DeleteFreeCam 99.9 (2× off:+48), CalcFrame 99.9 (3× off:+48), Randomize 99.9 (2× off:+36) — +36/+48 staircase intact |

Re-run composed A/B (primary worktree = clean main head c5632f9, full build,
log `~/tmp/rb3_build_exec-r1-member-delta-ab.log`): baseline snapshot 10,936 →
**10,995 matched, 0 regressions**. The +59 is entirely commits already landed
on main between the baseline snapshot and c5632f9 (ws1-waveA +46, ad2daa5 +5
— GemPlayer 10→13 + BinkClip 0→2 — MetaPanel +5, misc). **Net strict
attributable to this re-run: 0.** Stream remains CLOSED; nothing left to land.
