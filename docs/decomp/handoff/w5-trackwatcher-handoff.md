# WAVE-5 lane handoff — system/beatmatch/TrackWatcher

Branch `w5-trackwatcher`, worktree `/home/free/tmp/wt-w5-trackwatcher`.
Base: main `5cb96d4`. Checkpoint commit: `679232f` (clean-compiling port).

## What landed (committed 679232f)

- **Ported `src/system/beatmatch/TrackWatcher.cpp`** (full TU) Wii->MSVC:
  ControllerTypeToTrackWatcherType, NewTrackWatcherImpl, TrackWatcher ctor/dtor,
  SetImpl, ReplaceImpl, AddSink, RecalcGemList + all mImpl delegators. Adaptations:
  includes rewritten to `beatmatch/`-prefixed paths; added `os/Debug.h` (MILO_FAIL)
  + `macros.h` (RELEASE); `nullptr`->`NULL` (sibling convention). Compiles clean
  under cl.exe 16.00.11886.00 (only the expected va_list/frsqrte intrinsic warnings).
- **7 supporting impl-subclass headers** (needed by NewTrackWatcherImpl's `new`
  expressions; all verbatim ports whose deps already exist in xenon):
  BaseGuitarTrackWatcherImpl.h, DrumTrackWatcherImpl.h, GuitarTrackWatcherImpl.h,
  JoypadTrackWatcherImpl.h, RealGuitarTrackWatcherImpl.h, DrumFillTrackWatcherImpl.h,
  KeyboardTrackWatcherImpl.h.
- **objects.json**: `system/beatmatch/TrackWatcher.cpp: NonMatching` in module engine
  (next to TrackWatcherImpl.cpp).
- **splits.txt carve**: GuitarController.cpp's `.text 0x82777E90..0x8277D790` split into
  `0x82777E90..0x82778700` + `0x82778860..0x8277D790`; new `TrackWatcher.cpp` block owns
  `.text 0x82778700..0x82778860`. Overlap self-check: 0 pdata / 0 text overlaps.

## The target cluster is entirely ICF-shaped delegator thunks

`[0x82778700, 0x82778860)` holds TrackWatcher's non-virtual delegators. Each vtable
thunk is a 20-byte (0x14) leaf tail-call `lwz r3,0(r3); lwz r11,0(r3); lwz r11,SLOT(r11);
mtctr; bctr` on an 0x18 stride (20B body + 4B pad). Identity is 100% determined by the
TrackWatcherImpl vtable SLOT immediate. **The worklist's Wii-name->address pairings were
misattributed for 3 of 5** (bsim15-20 cluster-ambiguous). Corrected via slot:

| addr | slot | TRUE identity | worklist said |
|------|------|---------------|---------------|
| 0x82778708 | 0x0c | SetIsCurrentTrack | (not listed) |
| 0x82778720 | 0x1c | **Poll** | (not listed) — already map-claimed by FacePipelineDetect::Detect (ICF) |
| 0x82778738 | 0x14 | Jump | Jump  ✓ |
| 0x82778750 | 0x18 | Restart | (worklist put Restart at 0x82778810 — WRONG) |
| 0x82778768 | 0x20 | Swing | (not listed) |
| 0x82778780 | 0x24 | NonStrumSwing | NonStrumSwing ✓ |
| 0x82778798 | 0x28 | FretButtonDown | (not listed) |
| 0x827787B0 | 0x30 | RGFretButtonDown | (worklist put RGFretButtonDown at 0x827787c8 — WRONG) |
| 0x827787C8 | 0x2c | FretButtonUp | (worklist called this RGFretButtonDown — WRONG) |
| 0x827787E0 | 0x3c | Enable | Enable ✓ |
| 0x827787F8 | 0x44 | SetCheating | (not listed) |
| 0x82778810 | 0x48 | SetAutoplayError | (worklist called this Restart — WRONG) |
| 0x82778838 | 0x4c | SetSyncOffset | (not listed) |

Direct (non-virtual) delegator thunks at 0x82778700 / 0x82778828 / 0x82778830 /
0x82778850 / 0x82778858 (`lwz r3,0(r3); b <impl-method>`, 8B each) map to
RecalcGemList / SetAutoplayCoda / CycleAutoplayAccuracy / SetAutoplayAccuracy /
E3CheatIncSlop / E3CheatDecSlop — left unpinned (even more ICF-prone).

The big non-thunk functions (ctor, dtor, SetImpl, NewTrackWatcherImpl,
ControllerTypeToTrackWatcherType) are NOT in this cluster — they live elsewhere in the
binary and are currently unmapped. Not pinnable without full-binary fuzzy pairing;
covered by the fuzzy-paired source only.

## dtk boundary fix (uncommitted, see decision below)

dtk auto-split merged thunk PAIRS into 0x2C "functions" (Jump showed target_size=44 vs
base 20) with boundaries offset by the 4B pad. Fixed `config/45410914/symbols.txt` lines
206096.. to clean per-thunk `fn_<addr>` entries (size 0x14 vtable / 0x8 direct). After the
fix, all 12 pinned vtable thunks measure **target_size=20, base_size=20, fuzzy=100.0%**
(size-exact) via objdiff-cli-direct.

## Map pins (uncommitted)

12 ADD-ONLY entries appended to `scripts/target_symbol_map.json` (surgical tail insert —
original formatting preserved, 1 comma + 12 lines). Poll@0x82778720 deliberately NOT
pinned (address already owned by FacePipelineDetect::Detect via a pre-existing ICF fold —
a live example of the cluster's ICF hazard).

## DECISION: map pins DROPPED (ICF-alias inflation)

`tools/icf_alias_check.py --range 0x82778700-0x82778860 --list`:
```
matched (100%): 12   REAL-BODIED: 0   STUB-FOLD: 12 (100%)   FOREIGN-TU: 0
longest contiguous stub run: 12   VERDICT: ICF-ALIAS INFLATION
```
All 12 candidates are 20-byte stub-folds with zero real-bodied anchors. Per the lane
spec ("if only <=44B stub-folds go 100, land the SOURCE PORT and skip the map pins") and
the honesty gate, **the 12 map pins were reverted** — `scripts/target_symbol_map.json`
is byte-identical to HEAD. Do NOT pin these unless the project decides to accept
stub-folds, or until a real-bodied anchor appears in the cluster.

Each candidate DID verify true-100 size-exact (target_size=20=base_size=20, fuzzy=100.0)
after the symbols.txt boundary fix. **Recorded for a future lander** — if stub-folds
become acceptable, append these ADD-ONLY to target_symbol_map.json (they need the
symbols.txt boundary fix, which IS committed on this branch):
```
"0x82778708": "?SetIsCurrentTrack@TrackWatcher@@QAAX_N@Z"
"0x82778738": "?Jump@TrackWatcher@@QAAXM@Z"
"0x82778750": "?Restart@TrackWatcher@@QAAXXZ"
"0x82778768": "?Swing@TrackWatcher@@QAA_NH_N0W4GemHitFlags@@@Z"
"0x82778780": "?NonStrumSwing@TrackWatcher@@QAAXH_N0@Z"
"0x82778798": "?FretButtonDown@TrackWatcher@@QAAXH@Z"
"0x827787b0": "?RGFretButtonDown@TrackWatcher@@QAAXH@Z"
"0x827787c8": "?FretButtonUp@TrackWatcher@@QAAXH@Z"
"0x827787e0": "?Enable@TrackWatcher@@QAAX_N@Z"
"0x827787f8": "?SetCheating@TrackWatcher@@QAAX_N@Z"
"0x82778810": "?SetAutoplayError@TrackWatcher@@QAAXH@Z"
"0x82778838": "?SetSyncOffset@TrackWatcher@@QAAXM@Z"
```
(Poll@0x82778720 is unpinnable — already owned by FacePipelineDetect::Detect.)

## What a lander must know

- **Landable state = strict 0, fuzzy-paired TU source.** Committed on this branch:
  source port + 7 headers + objects.json wiring + splits carve + symbols.txt boundary
  fix. `scripts/target_symbol_map.json` untouched (== HEAD).
- **GuitarController carve is shared with lane 2** (BaseGuitarTrackWatcherImpl, near
  0x8277D278). My carve is disjoint: `[0x82778700,0x82778860)`. After union-merging both
  carves, re-run the SOP overlap self-check and confirm GuitarController still reports
  **17/158** (its matched fns are all < 0x82778700 — unaffected; verified this branch).
- The **symbols.txt boundary fix** (lines ~206096..) corrects genuinely-wrong dtk
  auto-analysis (it had merged thunk pairs into 0x2C "functions"). It is correct/honest
  regardless of pins and enables clean future pinning. Localized diff: 16 ins / 9 del.
- Overlap self-check on this branch: 0 pdata / 0 text overlaps.
