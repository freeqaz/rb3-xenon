# WAVE-5 lane handoff — BaseGuitarTrackWatcherImpl (rb3-xenon)

- **Branch:** `w5-bgtwi`  **Worktree:** `/home/free/tmp/wt-w5-bgtwi`
- **Worktree base:** main `5cb96d4` (STALE — see GameGem note). Current main HEAD at
  work time: `aa58cee`.
- **TU:** `src/system/beatmatch/BaseGuitarTrackWatcherImpl.{cpp,h}` (ported from
  rb3-Wii oracle, cpp 281 lines / h 59 lines). Header was MISSING in xenon — ported too.
- **objects.json:** `system/beatmatch/BaseGuitarTrackWatcherImpl.cpp` = `NonMatching`
  (module `engine`, mirrors TrackWatcherImpl.cpp), inserted right after TrackWatcherImpl.
- **Compiles:** YES (clean; only benign xdk C4391/C4392 intrinsic warnings).

## RESULT: 4 STRICT pins (all size-exact, all PROVEN byte-exact / score 0)

| addr | fn (MSVC mangled) | size | clean-map cli | proof |
|---|---|---|---|---|
| 0x8277D278 | `?Slop@BaseGuitarTrackWatcherImpl@@UAAMH@Z` | 80 (0x50) | **100.0 / score 0** | direct 100 (CanHopo pinned) |
| 0x8277CFE0 | `?CanHopo@BaseGuitarTrackWatcherImpl@@QBA_NH@Z` | 240 (0xF0) | 99.833 / score 10 | score 0 w/ IsRealGuitar+RightHandTap named |
| 0x8277D2C8 | `?TryToHopo@BaseGuitarTrackWatcherImpl@@QAAXMH_N0@Z` | 580 (0x244) | 99.862 / score 20 | score 0 w/ TimeAt+TimeAtNext+Playable named |
| 0x8277D510 | `?NonStrumSwing@BaseGuitarTrackWatcherImpl@@UAAXH_N0@Z` | 204 (0xCC) | 99.804 / score 10 | score 0 w/ IsTrillActive named |

Slop is a clean 100 in objdiff-cli (its only callee, CanHopo, IS pinned). The other 3
are size-exact with tiny scores whose residual is EXCLUSIVELY source-immune
call-target naming (`bl fn_XXXX` vs `bl ?Method@Class@...` for real functions in
OTHER units that this lane does not pin) — the standard report-normalized-100 pattern.
Each was PROVEN byte-identical (diff_score 0/N) by temporarily naming its callees;
those temp map entries were then REMOVED (not this lane's to pin). Mangled names were
extracted from the built base COFF via `scripts/obj_target_symbol_renamer.parse_coff_symbols`,
not hand-guessed.

## ⚠ GameGem 0x44 DEPENDENCY (load-bearing — coordinator must know)
TryToHopo/NonStrumSwing/CanHopo index the gem vector; the stride is `sizeof(GameGem)`.
The rb3-Wii dev `GameGem` is 0x2c; retail X360 is **0x44**. main HEAD `aa58cee`
already carries the fix (`int unk2c[6]; // 0x2c` tail in `src/system/beatmatch/GameGem.h`),
but this worktree branched from `5cb96d4` which PRE-DATES it — so the CoW copy was 0x2c
and the first build showed `mulli ...,0x2c` vs target `mulli ...,0x44` (a REAL diff).
I synced GameGem.h to HEAD's committed 0x44 version (**committed on this branch** so it
builds/verifies standalone; the rebase onto current main will dedupe the identical hunk).
**These 4 pins are ONLY valid on a GameGem-0x44 build.** Rebase onto current main
(which has 0x44) before landing — as land.sh already does.

## Splits carve (GuitarController.cpp container-unit)
GuitarController.cpp's `.text [0x82777E90,0x8277D790)` lump physically contains the
BGTWI tail. Carved 2 disjoint BGTWI ranges (both function-boundary-exact, verified vs
GuitarController.s `.fn`/`.endfn`):
```
BaseGuitarTrackWatcherImpl.cpp:
	.pdata  0x82237628-0x82237630   (CanHopo unwind)
	.pdata  0x82237648-0x82237660   (Slop+TryToHopo+NonStrumSwing unwind, 3 entries)
	.text   0x8277CFE0-0x8277D0D0   (CanHopo)
	.text   0x8277D278-0x8277D5E0   (Slop, TryToHopo, NonStrumSwing — contiguous)
```
GuitarController.cpp reduced to: `.text` [0x82777E90,0x8277CFE0)+[0x8277D0D0,0x8277D278)+[0x8277D5E0,0x8277D790);
`.pdata` [0x82237270,0x82237628)+[0x82237630,0x82237648)+[0x82237660,0x82237680).
Splits overlap self-check: **0 pdata / 0 text overlaps.** GuitarController.obj still
builds; carved fns had NO base counterpart in GuitarController.cpp (BGTWI code) so its
matched count cannot regress.

### ⚠ DISJOINTNESS with the w5 TrackWatcher lane (SAME container unit)
The TrackWatcher lane carves GuitarController near 0x82778738-0x82778838 (far below my
lowest carve at 0x8277CFE0) — DISJOINT. Both lanes rewrite the GuitarController.cpp
splits block, so a merge/union is required. Run the SOP overlap self-check after the
union and re-confirm GuitarController's pre-wave matches.

## Map (`scripts/target_symbol_map.json`) — ADD-ONLY, +4 entries
Added the 4 pins above (13635 -> 13639 keys). No other-unit / guessed entries. The
0X-uppercase key format was matched.

## MakeString<i,f,i> worklist id 0x8276be50 — NOT pinned (deferred, correct)
Worklist listed `MakeString<i,f,i>__FPCcifi_PCc` @0x8276be50 (a template instantiation
emitted from CheckForFretTimeout's `MakeString("...MISS_FRET_TIMEOUT...",0,f,gem)`),
sitting in a BeatMatcher.cpp splits gap [0x8276BA48,0x8276BF98). Deferred per the plan's
own caution ("template-instantiation ownership under ICF is shaky; pin only if byte-exact
AND icf_alias_check passes"): MakeString<int,float,int> is ICF-folded across many TUs, so
the representative at 0x8276be50 is not safely ownable by this TU. The source that emits
it rides in the ported cpp; leave the address to the global fuzzy pairer / a dedicated
MakeString keystone.

## Fuzzy-paired source (the bulk of the value)
The full 281-line cpp compiles and rides as NonMatching. Besides the 4 pinned fns it
carries the whole class (ctor/dtor, Swing, FretButtonDown/Up, PollHook, JumpHook,
HitGemHook, GemCanBePassed, AutoCaptureHook, ResetGemNotFretted, TryToFinishSwing,
SustainedGemToKill, CheckForFretTimeout, CheckForHopoTimeout, SetLastNoStrumGem,
HandleDifficultyChange) for the global fuzzy pairer to credit against their scattered
target addresses.

## MWCC->MSVC adaptations
- Dropped the rb3-Wii unprefixed `#include "BeatMatchControllerSink.h"` (needs
  `/I src/system/beatmatch` which the compile flags lack); kept the `beatmatch/`-prefixed one.
- Added `#include "utl/MakeString.h"` + `#include "os/Debug.h"` (MakeString / MILO_ASSERT).
- `HitGemHook`: rb3-Wii source falls off the end of this non-void fn (MWCC returns
  garbage, caller discards). MSVC rejects that (C4716 = error), so added `return 0.0f;`
  (the base-class default). HitGemHook is NOT in any carved range — this does not affect
  the 4 pins.
- All member/gem accessors, GemHitFlags enum, TheBeatMatchOutput(LogFile) resolved
  against existing xenon headers unchanged.

## What a lander must know
1. Rebase onto current main (GameGem 0x44) — mandatory for the pins to verify.
2. Union the GuitarController.cpp splits block with the TrackWatcher lane; re-run the
   overlap self-check; confirm each pinned fn reports in the BaseGuitarTrackWatcherImpl
   unit (not GuitarController) after the union (the 7951cb5 unit-attribution incident).
3. Verify recipe (from worktree): `python3 configure.py && touch config/45410914/config.yml`
   -> `rm -f build/45410914/config.json && tools/ninja-locked build/45410914/config.json`
   (fresh split) -> `python3 scripts/obj_target_symbol_renamer.py --batch --apply` ->
   `tools/ninja-locked build/45410914/src/system/beatmatch/BaseGuitarTrackWatcherImpl.obj`
   -> `objdiff-cli diff -u default/BaseGuitarTrackWatcherImpl '<mangled>' -f json -o FILE`.

## Files changed (path-limited)
- NEW `src/system/beatmatch/BaseGuitarTrackWatcherImpl.cpp`, `.h`
- `src/system/beatmatch/GameGem.h` (re-sync of main's committed 0x44 fix; rebase dedupes)
- `config/45410914/objects.json` (+1 NonMatching wire)
- `config/45410914/splits.txt` (GuitarController carve + BGTWI block)
- `scripts/target_symbol_map.json` (ADD-ONLY, +4 pins)
- `docs/decomp/handoff/w5-baseguitartrackwatcherimpl-handoff.md` (this doc)

---

## AUDIT VERDICT (wave-5 auditor, 2026-07-02) — **CLEAR**

Independently re-verified in this worktree (objdiff-cli-direct, JSON→file; direct
cl.exe compile-gate; instruction-level residual inspection). Every lane claim
reproduces. Landable as-is through the normal land.sh flow — **with the two
coordinator MUST-DOs below (already documented above; restated as land gates).**

### 1. Pins re-verified (all 4 reproduce, size-exact)
| fn | tgt/base size | normalized | score | note |
|---|---|---|---|---|
| Slop | 80/80 | **100.0000** | 0/2000 | true 100 (callee CanHopo pinned) |
| CanHopo | 240/240 | 99.8333 | 10/6000 | report-normalized-100 |
| TryToHopo | 580/580 | 99.8621 | 20/14500 | report-normalized-100 |
| NonStrumSwing | 204/204 | 99.8039 | 10/5100 | report-normalized-100 |

### 2. Honesty gate — PASS (residuals are 100% source-immune)
Instruction-by-instruction diff of every non-`equal` row confirms the ENTIRE
residual is `bl`-target *naming*, not code:
- CanHopo: idx10 `bl fn_82769D48` / idx14 `bl fn_82769DFC` ↔ base names
  `GameGem::IsRealGuitar` / `GameGem::RightHandTap` (other unit, unnamed in map).
- TryToHopo: idx29/33/50 `bl fn_82767E08/fn_82767E18/fn_82771208` ↔
  `GameGemList::TimeAt` / `TimeAtNext` / `TrackWatcherImpl::Playable`; idx24 is an
  intra-unit `bl CanHopo` (same name BOTH sides).
- NonStrumSwing: idx39 `bl fn_8276FC98` ↔ `TrackWatcherImpl::IsTrillActive`;
  idx47 intra-unit `bl TryToHopo` (same name BOTH sides).
Branch **addresses are identical** on both sides — only the symbol objdiff prints
differs (unnamed cross-unit callees). Standard report-normalized-100.

**No ICF/sibling-aliasing.** These are large real-bodied fns (240/580/204/80B)
with distinct call graphs, not ≤44B stub-folds → structurally cannot be
stub-fold inflation. Ownership proven: the ported source's call graph
(CanHopo→IsRealGuitar/RightHandTap; TryToHopo→CanHopo/TimeAt/TimeAtNext/Playable;
NonStrumSwing→IsTrillActive/TryToHopo) matches the TARGET disasm's `bl` targets
exactly → the pinned addresses house genuine BGTWI code (own, not foreign).
(icf_alias_check --range/--worktree needs a fresh whole-binary report, which is
owner-contended and off-limits in-lane; the direct evidence above is conclusive.)

### 3. Map + splits — clean
- Map diff vs 5cb96d4: **ADD-ONLY +4** (3 sorted-head + CanHopo at tail). No edits/removals.
- Splits overlap self-check: **0 pdata / 0 text overlaps.** Carve is complementary
  to GuitarController (function-boundary-exact, full original range covered, no gaps).
- objects.json: +1 NonMatching wire (BGTWI after TrackWatcherImpl). GameGem.h
  byte-identical to main HEAD `aa58cee` → clean rebase dedup.
- Commit `ba763ce` touches only the 7 declared paths; **no forbidden/owner file**
  (TourSavable/global_fuzzy_pairs/auto_*.obj/etc.) touched; global_fuzzy_pairs.json
  correctly left untracked.

### 4. Compile-gate — PASS (direct cl.exe, no audit-time drift). MILO_DEBUG: N/A
(no MILO_DEBUG conditionals in this TU; the only sizeof lever is GameGem 0x44, handled).

### ⛳ COORDINATOR LAND GATES (must-do, do not shallow-merge)
1. **GuitarController.cpp splits union with the sibling w5 TrackWatcher lane.**
   BOTH lanes rewrite the SAME unit's existing splits block (this lane carves
   0x8277CFE0-0x8277D0D0 + 0x8277D278-0x8277D5E0; sibling carves ~0x82778738-…).
   The carves are DISJOINT, but this is TWO independent MODIFICATIONS of the same
   block — the additive "theirs-added" resolver may NOT merge it cleanly. Union by
   hand if needed, **re-run the 0-overlap self-check**, and (per the 7951cb5
   unit-attribution incident) **confirm each of the 4 pins reports in
   `default/BaseGuitarTrackWatcherImpl`, not `default/GuitarController`,** on the
   composed build.
2. **Rebase onto current main for GameGem 0x44** (trivial — worktree GameGem.h ==
   main HEAD; hunk dedupes). Pins are only valid on a 0x44 build.

---

## INDEPENDENT RE-AUDIT (second wave-5 auditor, 2026-07-02) — **CLEAR**

Fresh reproduction in this worktree (no reliance on the prior audit section above).
Every lane claim reproduces byte-for-byte.

- **Pins (objdiff-cli-direct, JSON→file):** Slop 100.0000 / 80B·80B; CanHopo
  99.8333 / 240B·240B; TryToHopo 99.8621 / 580B·580B; NonStrumSwing 99.8039 /
  204B·204B. All size-exact, all report in unit `default/BaseGuitarTrackWatcherImpl`.
- **Honesty (instruction-level, `--include-instructions`):** every non-`equal` row
  is a `bl`-target *name* only, with the target's `fn_XXXX` matching the ported
  source's call graph exactly — CanHopo→IsRealGuitar/RightHandTap;
  TryToHopo→CanHopo(intra)/TimeAt/TimeAtNext/Playable;
  NonStrumSwing→IsTrillActive/TryToHopo(intra). No opcode/register/immediate diffs.
  ICF: **0 of the 4 symbols appear in `icf_aliases.map`** (nothing folded) — refutes
  any ICF-alias inflation; own-vs-foreign PASS. Real 80–580B bodies, not stub-folds.
- **Map:** ADD-ONLY +4 vs base (3 sorted-head + CanHopo at tail); no edits/removals.
- **Splits:** hand-verified full complementary coverage of the original
  GuitarController `.text [0x82777E90,0x8277D790)` + `.pdata [0x82237270,0x82237680)`
  with the 2 BGTWI ranges — **0 gaps, 0 overlaps**.
- **GameGem.h:** byte-identical to current main HEAD (`0473e17`) → clean rebase dedup.
- **Compile-gate:** direct `wibo cl.exe` PASS (only benign C4391/C4392 xdk intrinsic
  warnings); obj emitted. No audit-time drift.
- **MILO_DEBUG:** N/A — no conditionals in this TU; the only sizeof lever is
  GameGem 0x44, handled in GameGem.h.

**Coordinator land gates (unchanged, both real):** (1) current main still carries the
un-carved GuitarController block, so this carve applies cleanly *if landed against
main as-is*; if the sibling w5 TrackWatcher lane lands its GuitarController carve
first, hand-union the two carves, re-run the 0-overlap self-check, and confirm the 4
pins still report in `default/BaseGuitarTrackWatcherImpl` (7951cb5 attribution
incident). (2) rebase for GameGem 0x44 (already dedup-clean). Neither is a lane defect.

**Verdict: CLEAR — landable as-is through land.sh with the two coordinator gates carried forward.**
