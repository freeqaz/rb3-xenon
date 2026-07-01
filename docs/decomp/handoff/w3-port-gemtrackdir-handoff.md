# Port handoff — GemTrackDir.cpp (rb3-xenon)

Lane: `port-GemTrackDir`. Branch: `w3-gemtrackdir` (base main `52ceb11`).
Worktree: `/home/free/code/milohax/rb3-xenon/.claude/worktrees/wt-w3-gemtrackdir`.
TU: `src/system/bandobj/GemTrackDir.cpp` (1446-line MWCC oracle → MSVC PPC-Xenon),
wired NonMatching under the `engine` module in `config/45410914/objects.json`.

## Result
- **Compiles clean** (NonMatching). Full TU ported; **0 functions dropped**.
- **1 true-100 byte-equal pin: `GemHit`** (the ExactInstr HIGH anchor — validates
  the whole TU placement).
- 3 worklist near-misses kept as NonMatching source (high fuzzy%), **not pinned**
  per policy (bsim15-30 identity below 100 = sibling-aliasing poison risk).

## PINNED (true-100, `normalized_match_percent == 100.0`, byte-equal)
| VA | size | norm% | fn | notes |
|---|---|---|---|---|
| 0x822D17F8 | 36 (0x24) | 100.0 | `GemHit(int)` | leaf (no .pdata); ExactInstr HIGH identity |

Mangled name `?GemHit@GemTrackDir@@QAAXH@Z` extracted from the built COFF symtab
(machine 0x01f2 = PPC BE), not hand-guessed. Target obj/GemTrackDir.obj holds
exactly this one symbol → no fold. ICF verdict: **HONEST** (real 36-byte body:
`unk494=0`; if drum && (i&1) then `mKickPassCounter=0`; not in icf_aliases.map).

## KEPT as NonMatching source (fuzzy, NOT pinned)
| VA | size | norm% | fn | identity | why not pinned |
|---|---|---|---|---|---|
| 0x822D1A58 | 20 (0x14) | 99.0 | `UpdateLeftyFlip(bool)` | bsim15-20 | <100 + low-conf id |
| 0x822D1FF8 | 240 (0xF0) | 98.33 | `GemPass(int,int)` | bsim20-30 | <100 + <bsim30 |
| 0x822D4598 | 296 (0x128) | 99.46 | `TrackReset()` | bsim15-20 | <100 + low-conf id |

These are the highest-ROI residual leads. TrackReset (99.46%) and UpdateLeftyFlip
(99.0%) are likely a single reg/branch/bool-materialization delta each — run
`/permute` or objdiff `run_diff_inspect` before hand-editing. Their base sizes
already equal the target sizes (good sign the identity is correct).

## dtk boundary gotcha (important for re-verify)
dtk auto-analysis **merged** GemHit and UpdateLeftyFlip with the preceding tiny
leaf getters (no separating call-target symbol):
- `fn_822D17F0` (0x30) = 8-byte bool getter `lbz r3,0x429(r3);blr` **+ GemHit
  (0x24) + 4B pad**. Fixed in `symbols.txt`: shrink `fn_822D17F0`→0x8, add
  `fn_822D17F8`→0x24. This is the ONLY symbols.txt change (required for the pin).
- `fn_822D1A38` (0x34) = 3 leaf getters + UpdateLeftyFlip. Left merged (not
  pinned) — no symbols.txt change kept for it.
GemPass (0x822D1FF8) and TrackReset (0x822D4598) were already clean `fn_` symbols
in symbols.txt (real call targets), so they split without a symbols.txt edit.

## MWCC → MSVC adaptations
- Drop HX_NATIVE native-only includes + `GEM_DBG` block; drop the `rndwii/Mesh.h`
  Wii display-list clear in `PrepareChordMesh` (X360 `DxMesh` has no `mDisplays`).
- `SetupSmasherPlate`: keep the retail `afterhide->SetShowing(false)` path (drop
  the HX_NATIVE A1 hit-flame keep-shown hack).
- Two-arg `ObjPtr<T, ObjectDir>` → single-arg `ObjPtr<T>` (xenon ObjPtr).
- `PushRev/PopRev` are `BinStream` methods here: `bs.PushRev(...)/bs.PopRev(...)`.
- `GetCam()` → `PanelDir::Cam()`; protected cam planes via `NearPlane()/FarPlane()/
  YFov()`; transform writes via `DirtyLocalXfm()`; cam rect via `Get/SetScreenRect()`.
- Include `obj/ObjMacros.h` FIRST (DECLARE_REVS lives there, not Object.h — else
  `TrackDir.h:91` fails C4430); include `bandobj/TrackPanelDirBase.h` for
  `MyTrackPanelDir()`'s return type; include `bandobj/UnisonIcon.h` +
  cast `mUnisonIcon` (typed `ObjPtr<RndDir>` here) for `SetProgress`.
- New header `src/system/bandobj/BandButton.h` (UIButton subclass, from oracle).
- `Env.h`: add `RndEnviron::SetFadeRange` inline (additive, no layout change).
- `GemTrackDir.h` `#else` (non-native) `ThisDir`/`AsGemTrackDir` now `return this`
  (MSVC rejects empty non-void bodies — C4716; mirrors VocalTrackDir.h).

## Verify method (per-symbol, avoids forbidden whole-binary fresh_report)
1. Add tentative splits (`.text`) + `target_symbol_map.json` + symbols.txt
   boundary for all candidates; `touch config.yml`; `ninja-locked
   build/45410914/config.json` (re-split → obj/GemTrackDir.obj); `ninja-locked
   objdiff.json pre-compile <base.obj>`.
2. `bin/objdiff-cli diff -u default/GemTrackDir '<mangled>' --format json -o /tmp/x.json`
   → read `normalized_match_percent`.
3. Trim splits+map+symbols to true-100/high-conf only; re-split; re-verify.

## Files
- New: `src/system/bandobj/GemTrackDir.cpp`, `src/system/bandobj/BandButton.h`,
  this handoff.
- Modified: `config/45410914/objects.json` (wire), `config/45410914/splits.txt`
  (GemHit .text), `config/45410914/symbols.txt` (GemHit boundary),
  `scripts/target_symbol_map.json` (+GemHit), `src/system/bandobj/GemTrackDir.h`
  (#else return this), `src/system/rndobj/Env.h` (+SetFadeRange).
