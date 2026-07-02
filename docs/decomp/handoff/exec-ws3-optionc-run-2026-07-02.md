# exec ws3-optionc run — 2026-07-02 (planner: Fable; workers: Opus)

Execution of `docs/plans/workstreams-2026-07-02/ws3-optionc-port-then-pin.md`
(option-C oracle-cluster port-then-pin remainder). Read that doc + CLAUDE.md
for full context; this file records the Phase-0 verification results and the
packet partition actually dispatched.

## Phase-0 verification (all LIVE, re-derived this session on main @00c5b19)

Baseline: main @`00c5b19`, report.json = **10,936 / 65,607 matched**, 8.42%
strict code bytes, 11.58% fuzzy (2 more than the stream doc's 10,934 — doc holds).

Per-target verdicts:

| Target | Verdict | Evidence re-derived |
|---|---|---|
| CharClipGroup ObjVector flip | **GO (banked)** | Flip NOT applied on main: `src/system/char/CharClipGroup.h:34` still `ObjPtrVec<CharClip> mClips`; report.json shows FindClip 91.53%, Save 99.9%. Handoff doc `docs/decomp/handoff/charclipgroup-objvector-flip-READY.md` present and tracked. Old worktree `/home/free/tmp/wt-ov-CharClipGroup` is stale (base d696b52) — apply in the FRESH ws3 worktree instead. |
| MotionBlur + SoftParticles | **GO** | Gap [0x82480660,0x824822d8) verified FREE in current splits.txt (below: AmbientOcclusion, above: CubeTex). 5 non-funclet sim≥0.9 anchors, 3+2 in-gap, all real bodies per symbols.txt (0x78–0xAC). Sources exist in BOTH oracles. |
| MoggClip | **GO** | Free gap [0x826efb60,0x826f1520); contested head [0x826ef868,0x826efb60) overlaps exactly the 3 known BinkClip micro-pins. 11 non-funclet sim≥0.9 anchors, 8 in-region, real bodies (0x3C–0xF4). 7 MoggClip names already in target map. |
| NavListNode | **GO (conservative)** | Gap [0x82643bd0,0x82649c38) verified FREE. Dense cluster 0x826454e8–0x82646220 confirmed: 7 sim≥0.9 non-funclet oracle rows + ~10 map names already content-matched. Real bodies (0x64–0xDC). Rest of gap shows FOREIGN map names (LayerArray/HamDriver, MovieProvider, UIList, BandLabel) — pin the cluster only. |
| SongInfoAudioType | **CONDITIONAL** | Re-confirmed: **0** non-funclet sim≥0.9 oracle rows for SongInfoAudioType.obj. Only attempt if the scan tool corroborates the 0x825d3958 neighborhood. |
| Sound.cpp | **KILL (stands)** | Not re-attempted per stream doc; do not touch. |

New facts discovered in Phase 0 (bake into execution):

- **0x824811E8 name conflict:** target map says `??_G?$ObjRefConcrete@VRndTexBlendController@...` but oracle says `?Copy@RndMotionBlur@...` sim 1.0, size 0x78. Resolve via Ghidra before naming (packet p2).
- The MotionBlur gap also contains map names for `RndTexBlender`/`RndPollAnim` (`ClassName@RndTexBlender` at TWO VAs = likely ICF content-match artifacts), yet **0 oracle rows** for TexBlender.obj/TexBlendController.obj/PollAnim.obj/SoftParticleBuffer.obj land in the gap. The gap may hold more TUs than MotionBlur+SoftParticles — partition on evidence, pin conservatively, extend only with proof.
- **0x826efa28 ownership conflict (MoggClip vs BinkClip KillStream, ICF twins):** map + landed pin say BinkClip; oracle says MoggClip sim 1.0. Do NOT regress BinkClip — carve around unless Ghidra proves re-attribution.
- **0x826461D0 conflict:** map `??_GChooseModeProvider` vs oracle `??_GNavListShortcutNode` sim 1.0 (ICF scalar-deleting-dtor twins).

## Worktrees + baselines (all created off main @00c5b19)

| Packet | Worktree | Branch | Frozen baseline |
|---|---|---|---|
| p1 charclipgroup-flip | /home/free/tmp/wt-exec-ws3-optionc | exec/ws3-optionc-0702 | /home/free/tmp/exec-ws3-optionc-baseline-report.json |
| p2 motionblur-softparticles | /home/free/tmp/wt-exec-ws3-optionc-p2 | exec/ws3-optionc-0702-p2 | /home/free/tmp/exec-ws3-optionc-p2-baseline-report.json |
| p3 moggclip | /home/free/tmp/wt-exec-ws3-optionc-p3 | exec/ws3-optionc-0702-p3 | /home/free/tmp/exec-ws3-optionc-p3-baseline-report.json |
| p4 navlist-scantool | /home/free/tmp/wt-exec-ws3-optionc-p4 | exec/ws3-optionc-0702-p4 | /home/free/tmp/exec-ws3-optionc-p4-baseline-report.json |

One worktree per packet because every port-then-pin packet edits the SAME
config files (objects.json / splits.txt / target_symbol_map.json) and p1 is a
header lever needing whole-binary measurement independence.

## Honesty gates (every packet, from the stream doc — no exceptions)

1. `tools/icf_alias_check.py --worktree <wt> --baseline-report <baseline>` exit 0.
2. Whole-binary A/B (`tools/ab_measure.py --worktree <wt> --baseline <baseline>`,
   add `--resplit` after splits edits) net > 0, **0 regressions**, reproduced
   twice (run1 == run2).
3. Yield counted in REAL bodies only (>44B or oracle-attributed); funclets /
   `??__E` / `??__F` / guard thunks are not wins.
4. No pin overlaps an existing splits.txt range (`scripts/harvest/overlap_check.py`).
5. Names ADD-ONLY via `tools/safe_name_merge.py`; a name that doesn't produce a
   byte-exact match is removed.
6. Commit on the packet branch only. NEVER touch main, NEVER stash/checkout in
   the main repo.

## Expected yield (from stream doc, Phase-0-adjusted)

p1 +2 strict (validated), p2 +8–20, p3 +15–30, p4 +7–15 (+0–4 conditional)
plus the reusable scan tool. Stream total ~+32–70.

---

## REVIEW + INTEGRATION RESULTS (reviewer, 2026-07-02)

All four packets ACCEPTED after independent reproduction (MCP run_objdiff with
project_dir = each packet worktree; stub-fold guard applied — every claimed
match spot-checked is a real body well above 44 bytes):

- **p1 charclipgroup-flip**: FindClip 100.0/100.0, Save 100.0 norm (99.9 raw,
  reloc-name-only). Reviewer confirms the worker's DC3-drift finding (no unk24;
  vtordisp layout) — the READY doc's layout instruction was wrong.
- **p2 motionblur-softparticles**: Copy@RndMotionBlur 100 norm, ctor 100 norm,
  Save 100 norm, Copy@RndSoftParticles 100 norm.
- **p3 moggclip**: ??0MoggClip 100 norm, Save 100 norm, PreLoad 100 norm.
  BinkClip no-regress verified: SetLoop 88.3% == frozen baseline.
- **p4 navlist-scantool**: Insert@ShortcutNode / Renumber@SortNode /
  ??0ShortcutNode all 100 norm. `oracle_contiguity_scan.py --validate` = ALL
  GATES PASS against pre-p4 config (state-dependent self-test: fails
  post-consumption on NavListNode/MoggClip rank gates by design).

**Composition**: p2/p3/p4 diffs applied onto p1's worktree (all four shared
base 06938a5). Two tail-append conflicts in `scripts/target_symbol_map.json`
resolved by keeping both blocks; composed map verified to contain every
packet's exact add/remove set (p2 +12, p3 +14, p4 +17/-11) and to be valid
JSON. overlap_check clean.

**Composed whole-binary A/B** (resplit + full build, teed to
`~/tmp/rb3_build_exec-ws3-optionc-ab.log`, reproduced twice run1==run2):

    baseline 10936 -> candidate 11021   NET +85   (5 units up, 0 down)
      +38  default/system/synth/MoggClip        (0->38)
      +20  default/SongSortNode                 (1->21)
      +14  default/system/rndobj/SoftParticles  (0->14)
      +11  default/system/rndobj/MotionBlur     (0->11)
       +2  default/CharClipGroup                (0->2)

icf_alias_check (composed newly-matched set): VERDICT HONEST, exit 0 —
36 real-bodied, 32 own-unit stub-folds interspersed, longest stub run 6.

Verdict: **LAND**. Stream landed above the expected-yield band (+85 vs ~+32-70).
