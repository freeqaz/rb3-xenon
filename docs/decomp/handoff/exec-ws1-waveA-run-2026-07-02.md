# exec ws1-waveA — sysnet Wave A run plan (2026-07-02)

**Stream:** ws1 (`docs/plans/workstreams-2026-07-02/ws1-sysnet-drain.md`), Wave A ONLY.
**Planner:** Fable. **Base:** main `06938a5`. **Primary worktree:** `/home/free/tmp/wt-exec-ws1-waveA`
(branch `exec/ws1-waveA-0702`). **Baseline snapshot:**
`/home/free/tmp/exec-ws1-waveA-baseline-report.json` (also per-packet `-p2..-p6` baselines).

## Phase 0 results (live re-derivation, main @06938a5)

Doc counts were stale as predicted. Live (commands from the stream doc, reproduced twice):

- `net-new=356 wired=103 unwired-portable=109 no-oracle=144` (doc: 361/101/113/144;
  delta = commit `00c5b19` wired Faders/StandardStream/SongPreview/HamAudio).
- Pin-tool dry-run: **70 nameable (15 need micro-pins), 32 skipped** (19 foreign-pin,
  13 no-size). Ledger: `/home/free/tmp/sysnet_dryrun_1782997691.txt`.
- Confidence of the 70: high 2, bsim>=30 10, bsim20-30 21, **bsim15-20 37**
  (confirm-on-consume).
- Owner-WIP scan: NoteTube has 1 NAME-only id but its lane landed + double-audited
  (3e07239/916780e/dfea01c) — kept, reviewer re-checks. MasterAudio + GemTrackDir
  (untracked/in-flight in main) appear only as no-size SKIPs. No Band/Track/Game/
  Tracker/BandProfile/GemTrainerPanel/ClosetMgr/GemSmasher/BandTrack ids anywhere.
- **Stream is ALIVE — no kill.**

## What the planner already executed (deterministic, committed on `exec/ws1-waveA-0702`)

Commit `57345e0`: `band3_worklist_pin.py --all-wired --apply` in the primary worktree.
- **31 map names added** (incl. all **15 micro-pins** to splits.txt). dtk split
  validation clean — no except_data widening needed.
- Renamer applied: NOTE `rm stamp && ninja config.json` is NOT enough — the rename
  edge is the phony target **`pre-compile`**; sequence that works:
  `touch config/45410914/config.yml && rm -f build/45410914/target_symbol_renames.stamp
  && ./tools/ninja-locked build/45410914/config.json && ./tools/ninja-locked pre-compile`.
- Probe: `?Restart@TrackWatcher@@QAAXXZ` per-symbol objdiff reads **99.8% on 5
  instructions (20 B)** — measurement path proven; also a textbook tiny-forwarder
  confirm-on-consume case for P1.

The remaining 39 dry-run-nameable ids are ALL **BODY_MISSING** (verified per-id
against the compiled objs with ggtm primitives): the wired TU compiles but does not
contain the method/instantiation, so the deterministic namer has nothing to bind.
These are the Wave A port packets (P2–P5). All 27 TUs have an rb3-Wii oracle; most
have a DC3 twin (same compiler). The 4 `Quazal::DuplicatedObject` ids (wired TU,
Wii source exists) are **parked** per stream scope ("Quazal ids stay parked") →
35 actionable port ids.

## Packets (6, disjoint files)

| id | worktree | scope |
|---|---|---|
| p1-verify-applied | primary `/home/free/tmp/wt-exec-ws1-waveA` | verify all 31 applied names (21 are bsim15-20); remove failures |
| p2-port-rndobj | `-p2` | Text(4), MeshAnim(3), MatAnim, TransAnim, Rnd MakeString — 10 ids |
| p3-port-char-world-ui | `-p3` | CharBones, CharClip, CharClipDriver, CharServoBone, CameraManager, LightPreset, UILabel — 7 ids |
| p4-port-os-utl-obj-math | `-p4` | DateTime, Joypad(2), OnlineID, Color, Rot, DirLoader, Task, JobMgr, NetCacheMgr — 10 ids |
| p5-port-synth-bm-track | `-p5` | MidiInstrument, VorbisReader(2), BaseGuitarTrackWatcherImpl, TrackWidget RemoveAt(2), VocalTrackDir(2, incl the HIGH TypeToString) — 8 ids |
| p6-nosize-salvage | `-p6` | 12 no-size skips (timeboxed 15 min/id); DuplicatedObject::ClearFlag parked |

Port-packet lane rules (v2): per-symbol objdiff ONLY (`project_dir=<own worktree>`),
no whole-binary builds, no `fresh_report.sh`; pin exclusively via
`band3_worklist_pin.py --tu <Base>.cpp --apply`; bsim15-20 = per-fn identity check
vs the Wii body before the name counts; commit only own src (+minimal headers) +
`splits.txt` + `target_symbol_map.json` to the packet branch.

## Residue / handoffs

- **19 foreign-pin skips → ws5** (case-B campaign). Full list with enclosing TU in
  the Phase-0 ledger `/home/free/tmp/sysnet_dryrun_1782997691.txt`; notable:
  Anim-in-Tex(2), BandCharDesc-in-BandWardrobe(4), MemMgr `_MemAlloc`-in-MemHeap,
  System `SystemPoll`-in-Debug, DataNode-ctor-in-Task.
- **Parked Quazal:** 0x82a435b8/0x82a45a78/0x82a46180/0x82a474a8 (port ids) +
  0x82a45410 (no-size) in `network/ObjDup/DuplicatedObject.cpp`.
- **Known trap in P6:** `TrackWidget::Init` 0x827bb458 is THE measured worklist miss
  (aliased to sibling `Empty`, 20-byte forwarders differing only in the vtable-slot
  immediate 0x44 vs 0xc) — verify immediates before naming.

## Integration (reviewer)

Land order: p1 (primary, includes the 31-name commit + any removals) first, then
p2–p6 via `scripts/harvest/land.sh <wt>` (map dict-union, splits line-union — all
addresses disjoint by construction). ONE composed A/B on main-staging:
double `tools/fresh_report.sh`, `scripts/harvest/measure_delta.py
/home/free/tmp/exec-ws1-waveA-baseline-report.json build/45410914/report.json`,
`python3 tools/icf_alias_check.py`. Unnamed micro-pins (a few run-1 spans whose
name did not resolve, e.g. NetCacheMgr 0x82741b78 until p4 lands) are harmless:
new spans, previously uncounted, cannot regress the delta. After landing, re-run
`tools/gen_sysnet_port_worklist.py` (rewrites the tracked roster md — commit it).

## Review + integration results (reviewer, 2026-07-02)

**Composed A/B (this worktree, branch `exec/ws1-waveA-0702`): NET +46 strict
(10936 -> 10982), 0 strict regressions, 0 fuzzy regressions.** Reproduced twice
via `tools/fresh_report.sh` (identical NET both runs); log
`~/tmp/rb3_build_exec-ws1-waveA-ab.log`. ICF alias gate: HONEST
(14 real-bodied anchors; 32 tiny guard/getter funclets, the VocalTrackDir run
below is the TypeToString static-Symbol guard cascade).

### Per-packet verdicts

- **p1-verify-applied — ACCEPT.** 21/31 names kept, 10 removed (verdict table:
  `exec-ws1-waveA-p1-verdicts.md`). Spot-reproduced: GetVal 100, DetachBuffer 100,
  UnflipGems 95.1. Kept names contribute 8 of the composed stricts (GetVal,
  DetachBuffer, Jump/NonStrumSwing/Enable, UnflipGems stayed fuzzy, etc.).
- **p2-port-rndobj — ACCEPT (reviewer-harvested).** Worker committed nothing
  (pin-tool namer gap: free-function templates); reviewer added the 6 verified
  {addr: mangled} pairs directly (manglings re-extracted from the compiled objs).
  Composed A/B confirms all 5 op<< Key<T> stricts (MatAnim, TransAnim, MeshAnim x3);
  Rnd MakeString<Symbol,f4> stays 97 (MakeString.h +0x800 frame wall).
  Namer-gap fix largely covered by p5's tool patch (landed here).
- **p3-port-char-world-ui — ACCEPT with 1 repair + 1 drop.** LightPreset::
  GetCurrentPostProc strict 100 reproduced. REPAIR: p3 left a stale non-const
  nullptr stub in BandDirector.cpp:1794 that broke the composed build (C2511)
  once the header went const — deleted (p3 had flagged it). DROP: 0x82ad4578
  CameraManager __unguarded_partition name removed (20.5%, identity UNCONFIRMED
  per p3's own measurement — misname risk). Kept: CharClipDriver ctor 96.7,
  CharBones MakeString 97.3, CharServoBone::RegulateInternal 78.4 named near-misses.
- **p4-port-os-utl-obj-math — ACCEPT.** All 5 stricts reproduced per-symbol
  (HasJob, OnlineID op==, Color op<<, Rot Interp, NetCacheMgr _List_base clear).
  In the composed report ToMiniDateString (lane 98.7) landed TRUE 100;
  NetCacheMgr clear + UserHasGHDrums (99.9) stayed fuzzy (ServerData/JoypadData
  struct walls) — as predicted.
- **p5-port-synth-bm-track — ACCEPT incl. tool patch.** PressNote, DoFileRead,
  Decrypt stricts reproduced. TypeToString 97.2 rewrite additionally revealed the
  24-fn VocalTrackDir guard/getter cascade (all inside the pre-existing pinned
  span). Tool patch (band3_worklist_pin.py + gen_game_target_map.py: overload
  argcount disambig, $4-thunk exclusion, anon-ns free fns, ??$ templates) is
  build-inert (not in any ninja rule) and fixes the namer gaps p2/p3 hit — landed.
- **p6-nosize-salvage — ACCEPT.** All 3 tiny stricts reproduced at 100
  (CaptureAfter 32B, UpdateLeftyFlip 20B, SelectedDisplay 28B); none in
  icf_aliases.map, all name-paired with distinctive bodies — stub-fold guard
  satisfied. SetPartActive 88.0 named near-miss kept. TrackWidget::Init trap
  correctly refused.

### Composed strict gains (46)

22 named: CaptureAfter, Color op<<, ToMiniDateString, FaderGroup::GetVal,
UpdateLeftyFlip, HasJob, GetCurrentPostProc, Key<T> op<< x5 (MatAnim/TransAnim/
MeshAnim), PressNote, DetachBuffer, OnlineID op==, Rot Interp, TrackWatcher
Jump/NonStrumSwing/Enable, SelectedDisplay, Decrypt, DoFileRead.
24 anonymous VocalTrackDir funclets (TypeToString guard cascade).

### Residue

- Rnd MakeString<Symbol,f4> 97.0 / CharBones MakeString<f7> 97.3 /
  BaseGuitarTrackWatcherImpl MakeString<i,f,i> 96.4: shared utl MakeString.h
  buffer (+0x800 frame) — one shared-header fix would close all three.
- JoypadData 8-byte struct wall (UserHasGHDrums 99.9, JoypadGetCalbertValue).
- CharClip::BeatAlignString body correct but unmeasurable (dtk exception-region
  boundary fragmentation at 0x8236A628) — needs symbols surgery.
- VocalTrackDir::Copy named at 0% (retail-360 layout diverges from Wii; deep).
- UILabel highlight-mesh infra absent; DirLoader MakeString<FilePath,f> gated
  by MILO_LOG no-op; Task _S_remove_if needs ThreadTask::Replace restructure.

## Round-2 re-run review (2026-07-02, later same day)

A second execution wave was dispatched against the same packet list after the
original wave had already landed on main (44a00a8, then 89c3038 / ad2daa5 /
c5632f9). Reviewer verdicts:

- **p1-verify-applied — ACCEPT (no-op).** Verdict table already merged
  (`exec-ws1-waveA-p1-verdicts.md`); reviewer cross-checked current main map:
  all 21 kept names PRESENT, all 10 FAIL names ABSENT. Consistent.
- **p2-port-rndobj — ACCEPT (no-op).** Worktree clean vs main. Reviewer
  reproduced MatAnim `op<< Key<Vector3>` 100.0 norm (30 insns, name-paired).
  4 Text ids remain walled behind the RndText flat-layout rewrite (DC3
  ObjVector<Style> model vs retail flat single-Style, class end ~0xc8 vs
  0x158+) — separate structural packet.
- **p3-port-char-world-ui — ACCEPT (no-op).** Worktree gone (post-land
  removal); reviewer reproduced GetCurrentPostProc 100.0/100.0 (21 insns)
  from a clean-at-main worktree.
- **p4-port-os-utl-obj-math — ACCEPT (no-op).** Worktree clean vs main;
  3 residue defers documented with root cause (JoypadGetCalbertValue: no
  oracle body anywhere; DirLoader MakeString<FilePath,f>: gated by no-op
  MILO_LOG in PCH-load-bearing os/Debug.h; Task _S_remove_if: needs
  std::list-vs-ObjPtrList container restructure).
- **p5-port-synth-bm-track — ACCEPT (no-op).** Worktree clean vs main;
  reviewer reproduced PressNote 100.0 norm (44 insns) and Decrypt 100.0 norm
  (71 insns). VocalTrackDir::Copy 0% root-caused to ObjMacros/ObjPtr
  template semantics (retail direct-stw copy + ~90-insn inlined deep-copy
  tail), not a missing body.
- **p6-nosize-salvage (round 2) — ACCEPT with 1 drop.** New edits, composed
  into this branch:
  - 3 symbols.txt lbl->fn conversions + 3 splits.txt micro-pins:
    Str 0x8254F660 (8B, unnamed — target overload not emitted standalone),
    UILabel 0x827CD310 (0x28, unnamed — InqMinMaxFromWidthAndHeight, not
    standalone in our obj), ADSR 0x8270C008 (0x10).
  - ADSR named `?NearestSustainRate@Ps2ADSR@@QBAHM@Z`; reviewer reproduced
    0.0% (target 16B head fragment / base 88B — dtk over-split the true
    [0x8270C008,0x8270C060) body into lbl+fn_8270C018+fn_8270C040). Kept as
    a named near-miss marker; full match needs boundary surgery + body port.
  - **DROPPED: `0x827ccd80 -> ?Poll@UILabel@@UAAXXZ` map re-add.** The
    landed p1 verdict explicitly removed this exact entry at 3.4% full-body
    match; re-adding without new identity evidence contradicts it.
  - Correct SKIPs confirmed: TrackWidget 0x827bb458 trap (slot 0x3c =
    SetDirty, not Init), CharClipGroup inlined no-arg GetClip, NetStream
    forwarder, MasterAudio owner-WIP overlap.

**Round-2 composed A/B (this branch vs main c5632f9): 10995 -> 10995 matched,
0 regressions, +2 total functions (micro-pins registering as target-only).**
Net strict +0 by design — round 2 is pin/name hygiene only; all match gains
were already banked in the original wave land (44a00a8).
