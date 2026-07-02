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
