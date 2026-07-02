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

---

## WAVE-5 AUDIT (2026-07-02) — VERDICT: CLEAR

Independently re-verified in this worktree. All five audit tasks pass.

1. **Strict claims (0) reproduce.** `strictClaimed=0` — nothing to reproduce.
   `git diff 5cb96d4..HEAD -- scripts/target_symbol_map.json` = EMPTY: the map is
   byte-identical to base. No false strict claim, no guessed sub-100 identity pinned.
2. **Honesty gate PASS (no ICF-alias inflation).** `tools/icf_alias_check.py
   --range 0x82778700-0x82778860 --list` → "no 100%-matched functions in the
   selected set … VERDICT: HONEST (empty set)". Because zero map pins were added
   and TrackWatcher.cpp is NonMatching, no function in the cluster reports 100% —
   so there is nothing to inflate. The 12 stub-folds were correctly identified and
   DROPPED (recorded above for a future lander only). Body-port lane, own-TU only.
3. **Map ADD-ONLY / splits respect both neighbours.** Map unchanged. SOP overlap
   self-check on the full splits.txt = **0 pdata / 0 text overlaps**. TrackWatcher's
   carve `[0x82778700,0x82778860)` fits exactly in the gap between GuitarController's
   two remaining ranges (`…end:0x82778700` and `start:0x82778860…`) — disjoint.
4. **Compile gate PASS.** Direct `cl.exe 16.00.11886.00` on TrackWatcher.cpp →
   only the expected va_list(C4392)/frsqrte(C4391) intrinsic warnings; 31,435-byte
   .obj produced. Port matches the Wii source structurally (include-path prefixing +
   `nullptr`→`NULL` + added macros.h/os/Debug.h only).
5. **MILO_DEBUG landmine N/A.** No dev-only members or debug-only blocks in the
   ported .cpp or the 7 impl headers. strict=0 (NonMatching) means no sizeof-driven
   false-100 risk anyway.

**Extra checks:** symbols.txt boundary fix is an honest correction of dtk's
misalignment (it had made 0x2C "functions" spanning 1.5 thunks off a 4B pad; the fix
lays down clean 0x14-stride thunk entries covering the cluster exactly). None of the
8 removed dtk labels (lbl_82778708, fn_8277870C/764/790/7BC/7E8/814/840) are
referenced anywhere in config/ or scripts/. objects.json wiring is a single-line add
in the correct engine module beside TrackWatcherImpl.cpp. No forbidden/owner files
in either commit; global_fuzzy_pairs.json correctly left untracked.

**FOR THE LANDER:** the GuitarController `.text` split IS shared with lane 2
(BaseGuitarTrackWatcherImpl near 0x8277D278, inside `[0x82778860,0x8277D790)`).
This lane's carve is disjoint from lane 2's, so union-merge is safe — but after
merging, re-run the SOP overlap self-check and grep GuitarController still reports
17/158. The `.pdata` split (0x82237270 → …0x822372D8 + 0x822372D8…) keeps BOTH
halves under GuitarController — content-neutral (contiguous, same owner); harmless
to keep or collapse.

Verdict: **CLEAR** — landable as-is (strict-0, fuzzy-paired TU source per owner policy).

## WAVE-5 AUDIT (independent re-run, 2026-07-02) — VERDICT: CLEAR

Second, independent auditor. All five gates re-run from scratch in this worktree;
all pass. Adds one check the first audit did not: byte-level ground-truth.

- **Map is byte-identical to base** (`git diff 5cb96d4..HEAD scripts/target_symbol_map.json` = empty). strict=0, nothing to downgrade.
- **Honesty gate:** `icf_alias_check.py --range 0x82778700-0x82778860` = HONEST (empty set); zero pins, TrackWatcher.cpp NonMatching. The 12 stub-folds were correctly dropped.
- **Splits:** full-file overlap self-check = 0 text / 0 pdata overlaps; carve `[0x82778700,0x82778860)` is disjoint and fits exactly between GuitarController's two remaining ranges.
- **Compile gate:** direct cl.exe 16.00.11886.00 → 31,435-byte .obj, only the expected C4392/C4391 intrinsic warnings.
- **MILO_DEBUG:** N/A (no dev-only members in the .cpp or 7 headers).
- **No forbidden/owner files** in either commit; 8 removed dtk labels have 0 remaining references in config/scripts/src.
- **Ground-truth byte verification (new):** disassembled the cluster from `orig/45410914/band.exe` (imgbase 0x82000000). 0x82778708→`lwz r11,0xc(r11)` slot 0x0c; 0x82778720→slot 0x1c (Poll); 0x82778838→slot 0x4c; 0x82778700/0x82778850→`lwz r3,0(r3); b` 8-byte direct thunks. The committed symbols.txt (0x14 body / 0x18 stride vtable thunks + 0x8 direct) matches the real bytes exactly; dtk's prior 0x2C-merged, +4-offset labels were genuinely wrong. The boundary fix is honest and correct.

Verdict: **CLEAR** — landable as-is. Lander: union-merge the GuitarController carve with lane 2, re-run the overlap self-check, confirm GuitarController stays 17/158.
