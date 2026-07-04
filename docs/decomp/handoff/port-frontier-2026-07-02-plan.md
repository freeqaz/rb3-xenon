# Port-Frontier Wave Plan — 2026-07-02

Planner output for a Wii→Xenon decomp source-porting wave. Consumes the two
gitignored worklists (`band3_port_worklist.json`, `sysnet_port_worklist.json`)
deduped against the live `scripts/target_symbol_map.json` (13,635 keys, uppercase
`0X…` hex). Rank = yield × tractability. Porter model = Opus, one per assignment,
in an isolated `scripts/setup_worktree.sh ~/tmp/wt-<tu> <branch>` CoW worktree.

## Dedupe / worklist state (measured this session)
- band3: 232 rows / 93 TUs → **37 undone targets across 27 TUs** (worklist ~84% consumed).
- sysnet: 516 rows / 276 TUs → **384 undone targets across 232 TUs**, but the
  bulk is un-sourced Quazal/ObjDup/Plugins/Services netcode (rb3-Wii has NO
  source; DC3 cannot provide) → **pin-only yields nothing, EXCLUDED**.
- The port frontier that pays = **wired+split system/band3 TUs with rb3-Wii source
  and undone targets INSIDE the existing split** (pure add-map-entry pairing), plus
  a few small ports with a salvageable Wii source and ≥1 ExactInstr anchor.

## The decisive filter used
For each undone target I checked whether its `rb3_addr` falls INSIDE the TU's
current `splits.txt` `.text` ranges:
- **INSIDE** → the differ already emits a target `.obj` covering it; adding the
  `target_symbol_map.json` VA→mangled entry PAIRS it immediately → measure. Cheapest.
- **OUTSIDE** → needs a micro-pin (`.text start:VA end:VA+size` under the TU's
  EXACT split header + a `symbols.txt fn_<ADDR> size:0xNN` line derived via
  `tools/va_disasm.py`), watching for the **pdata-boundary wall** (a candidate whose
  true byte-range overlaps a pdata-anchored `fn_XXXX` fails dtk with
  "ends within symbol" — REJECT, do not pin).

## Split-header inconsistency (load-bearing — get this exact per TU)
`band3_worklist_pin.py --apply`'s split-insert regex is `^{basename}:` and will
append a DUPLICATE bare header for full-path TUs → hand-edit under the exact header:
- FULL-PATH headers: `band3/bandtrack/Gem.cpp:`, `band3/game/VocalPart.cpp:`,
  `network/ObjDup/DuplicatedObject.cpp:` (also DUPLICATED as a bare header — sibling hazard).
- BARE headers: `TrackWatcherImpl.cpp:`, `Text.cpp:`, `MemMgr.cpp:`, `Sfx.cpp:`,
  `Faders.cpp:`, `ADSR.cpp:`, `BandCharDesc.cpp:`, `VocalTrackDir.cpp:`.

## Const-method demangle gotcha (still live)
`gen_game_target_map.py::parse_wii_name` was fixed 2026-07-01 to strip ` const`
from the class token; but enum-arg mangling still chokes the tool. For any
`… const::Method` or enum-arg target, grab the mangled name from the compiled COFF
obj (`strings X.obj | grep Method`) and add the map entry by hand.

## Sibling-aliasing tier (BSim 15–20 = confirm-on-consume)
The one recurring failure mode across all ghidriff rounds is same-TU sibling
aliasing in the BSim 15–20 tier (near-identical bodies differing only in a
type-tag immediate / STL node-size literal / vtable slot). Treat every bsim15-20
target as "confirm-on-consume": after wiring, if the differ pairs it but scores
implausibly, diff immediates before trusting; the BUILD is the precision gate
(a wrong addr finds no `fn_<addr>` to pair or caps well below plausible).

---

## RANKED ASSIGNMENTS (best first)

### 1. VocalTrackDir.cpp — PIN (easy) — 5 targets, 3 INSIDE split
- Header (BARE): `VocalTrackDir.cpp:` — split `.text 0x822E4180–0x822E95FC`.
- Wired: YES. Xenon source present (1360 lines); all methods confirmed in source
  (RecalculateLyricZ L668, ConfigPanels L823, SetRange L966, TypeToString L156).
- INSIDE split (pure map-entry pins, all **bsim>=30 safe tier**):
  - `0x822e4a00` RecalculateLyricZ  (bsim 47.9; memory notes ~91.9% fuzzy — pairs as workable partial)
  - `0x822e4eb0` SetRange           (bsim 32.3; memory ~80% — workable partial)
  - `0x822e8480` ConfigPanels       (bsim 30.3; confirmed NOT mapped — the old misattribution
                                      was 0x822E4B40→0x822E8480, this addr is clean)
- OUTSIDE (optional micro-pin, defer): `0x822e3788` TypeToString (free fn, ExactInstr),
  `0x827cf2e0` Copy (bsim 17.8, far from TU body — sibling risk, SKIP).
- Notes: highest-confidence immediate pairing this wave. Even the partials count
  (per partials-count policy) and corroborate the identity. Map pins ONLY at
  true-100 or confirmed-identity near-miss.

### 2. Text.cpp (RndText) — PIN (easy) — 4 targets, ALL 4 INSIDE split
- Header (BARE): `Text.cpp:` — split covers 0x82442120, 0x82443440, 0x82446dd0, 0x82446f08.
- Wired: YES. Xenon source present (2728 lines).
- INSIDE (pure map-entry pins):
  - `0x82446f08` SetAltSizeAndZOffset (bsim 34.5)
  - `0x82446dd0` SetSize              (bsim 23.3)
  - `0x82442120` Print                (bsim 19.3, confirm-on-consume)
  - `0x82443440` SyncMeshes           (bsim 16.7, confirm-on-consume)
- Notes: engine rndobj, high value. 2 are bsim15-20 → confirm scores are plausible.

### 3. TrackWatcherImpl.cpp — MIXED (medium) — 8 targets, all OUTSIDE split (micro-pin)
- Header (BARE): `TrackWatcherImpl.cpp:` (existing split has 3 tiny `.text` slivers).
- Wired: YES. Xenon source present (857 lines).
- Targets (need `va_disasm.py` size + micro-pin each, pdata-boundary-check each):
  - `0x82771cb8` CheckForAutoplay (bsim 36.8)  · `0x82771328` OnHit (bsim 35.2)  ← safest first
  - `0x82770428` SendHit (24.5) · `0x827714f8` OnMiss (23.3) · `0x8276fbb0` RecalcGemList (21.5)
  - `0x827720d8` KillSustainForSlot (19.7) · `0x827704e8` SendMiss (17.0) · `0x8276fd78` EndSustainedNote (16.6)
- Notes: highest target COUNT this wave. Do the two bsim>=30 first, then work down;
  each is an independent micro-pin so partial completion still lands value. Watch
  the pdata wall on each. Prior wave already harvested this TU's easy exacts, so the
  residual may include permuter-class near-misses (land as named pairings anyway).

### 4. TambourineManager.cpp — PORT (medium) — 3 targets, 1 ExactInstr — SALVAGE stale scaffold
- Header: NONE yet (no split block) — new cluster.
- NOT wired, NO xenon source. **BUT** a 412-line MSVC port scaffold exists on the
  9-day-stale UNMERGED branch `bw-TambourineManager.o` (commit `cbf7cf1`).
- **Do NOT merge/checkout that branch** (its stale objects.json would revert ~12
  later entries — stale-lane audit lesson). Extract just the source:
  `git show cbf7cf1:src/band3/game/TambourineManager.cpp > src/band3/game/TambourineManager.cpp`,
  then re-verify it compiles against current headers, wire it (objects.json band3
  NonMatching), configure, pin the `.text` cluster, add map entries.
- Targets:
  - `0x826dbaa8` TambourineGems() const  (**ExactInstr** — safe anchor; source L78)
  - `0x826dd580` TambourineSwing (bsim 22.2)  · `0x826dd6f0` HandleButtonDown (bsim 23.5)
- Wii source: `~/code/milohax/rb3/src/band3/game/TambourineManager.cpp` (412 lines).

### 5. ADSR.cpp — PIN (easy) — 3 targets, 2 INSIDE split
- Header (BARE): `ADSR.cpp:` — split has `.text 0x8270c188–0x8270c1d0` and `0x8270c1d0–0x8270c2c8`.
- Wired: YES. Xenon source present (95 lines — small synth TU).
- INSIDE (pure map-entry pins): `0x8270c188` ADSR::SyncPacked (bsim 19.2, confirm),
  `0x8270c1d0` ADSR::Load (bsim 17.7, confirm).
- OUTSIDE (optional micro-pin): `0x8270c008` Ps2ADSR::NearestSustainRate (bsim 30.5).
- Notes: small, self-contained. Two INSIDE targets are bsim15-20 → confirm-on-consume.

### 6. MemMgr.cpp — MIXED (medium) — 4 targets, 1 INSIDE + 1 ExactInstr
- Header (BARE): `MemMgr.cpp:` — split has `.text 0x827963d8–0x82796440`.
- Wired: YES. Xenon source present (824 lines).
- INSIDE (pin): `0x827963d8` Heap::FreeBlockStats (bsim 30.0).
- OUTSIDE (micro-pin, pdata-check): `0x827966e8` MemHandle::Lock (**ExactInstr** — safe),
  `0x827977d0` _MemAlloc (bsim 18.1, free fn), `0x82798278` _MemOrPoolFreeSTL (bsim 15.4, STL-heavy — LOW priority/skip).
- Notes: FreeBlockStats + MemHandle::Lock are the two solid ones; _MemOrPoolFreeSTL
  is STL-template + low-conf, skip unless trivial.

### 7. TrackConfig.cpp — PORT (easy) — 2 targets, tiny (80-line TU)
- Header: NONE yet. NOT wired, NO xenon source.
- Wii source: `~/code/milohax/rb3/src/band3/bandtrack/TrackConfig.cpp` (80 lines).
- Targets (both `const` getters — port cleanly; const-demangle gotcha applies):
  - `0x82b78200` AllowsOverlappingGems() const (bsim 27.6; source L33)
  - `0x82b78288` IsRealGuitarTrack() const      (bsim 23.5; source L44)
- Notes: smallest port on the board. Port whole 80-line TU, wire, pin the cluster.
  Grab mangled const names from the COFF obj for the map entries.

### 8. Sfx.cpp — MIXED (medium) — 3 targets incl 1 bsim>=30
- Header (BARE): `Sfx.cpp:` — split has one sliver `.text 0x824b3818–0x824b3874`; all
  3 targets OUTSIDE (micro-pin).
- **NOT wired** despite having a split (basename false-positive corrected) — needs
  objects.json wiring too → MIXED. Xenon source present (277 lines).
- Targets: `0x826ffb28` Sfx::Load (bsim 41.5 — safest), `0x826fcc90` Sfx::Pause (22.7),
  `0x826fcbf8` SfxInst::UpdateVolume (17.6, confirm).
- Wii source: `~/code/milohax/rb3/src/system/synth/Sfx.cpp`.

### 9. NoteTube.cpp — PORT (medium) — 3 targets, 1 ExactInstr
- Header: NONE yet. NOT wired, NO xenon source. Wii source 517 lines.
- Targets: `0x82bf6580` TubePlate::CurrentEndX() const (**ExactInstr**; source L473 —
  `return mBeginX + mWidthX + f;`), `0x82bf6570` TubePlate::CurrentStartX() const
  (bsim 16.2; source L472), `0x82bf68f8` NoteTube::BakePlates (bsim 19.0).
- Notes: CurrentEndX ExactInstr is the anchor; the two getters are adjacent tiny
  bodies (0x82bf6570/0x82bf6580, 16 bytes apart) → ICF/sibling-alias risk, confirm
  the CurrentStartX vs CurrentEndX immediate (`+f` vs `+mWidthX+f`) after pairing.
- Wii source: `~/code/milohax/rb3/src/system/bandobj/NoteTube.cpp`.

---

## COLLISION WARNINGS
- **CharClipGroup / CharClip family** — ACTIVE (worktrees `~/tmp/wt-fz-charclip`,
  `~/tmp/wt-ov-CharClipGroup`, both committed 7h ago "fuzzy-salvage pin
  CharClipGroup Save+FindClip"). CharClip*/CharServoBone/CharDriver/CharBones/
  CharBone/CharHair/Character.o targets EXCLUDED this wave.
- **CreditsPanel.cpp** — worktree `~/tmp/wt-s1-CreditsPanel` (+ `-verify`) open, 30h.
  EXCLUDED.
- **bw-TambourineManager.o** — 9-day stale UNMERGED branch with an unlanded 412-line
  scaffold. Assignment #4 SALVAGES the source via `git show` only — do NOT
  merge/checkout the branch (stale objects.json reverts later entries).
- **bw-TrackerManager.o / bw-RockCentral.o** — 9-day stale UNMERGED (breadth-sweep
  leftovers). Both TUs' undone targets are all bsim15-20 sibling-alias risk → not
  assigned; if consumed later, audit the stale branch by parent-diff, don't merge.
- `memmgr` branch = 5 weeks old, MERGED, no worktree → NOT a collision (MemMgr.o
  assignment #6 is safe).
- `bp-round3*`, `topo-locator`, `vf3`, `fixcfg`, `sizedvec-experiment` worktrees =
  unrelated to any assigned TU.

## EXCLUSIONS (deliberately skipped)
- **All Quazal/ObjDup/Plugins/Services/Protocol/net netcode** (DuplicatedObject,
  Session, Station, WKHandle, CallContext, DuplicationSpace, EndPoint, Job*,
  Authentication, PRUDP*, etc.) — no rb3-Wii source (`wii=N`), pin-only yields
  nothing, and DC3 cannot provide (Quazal). ~200 sysnet rows. Excluded per roadmap.
- **VocalPart::CalcPhraseScoreMax** (`0x826d4058`) — REJECTED: overlaps pdata anchor
  `fn_826D4108` (pdata-boundary wall, per 2026-07-02 memory). UpdateSongMinMaxPitch
  (`0x826d3cc0`) is OUTSIDE split + isolated; low ROI → skip VocalPart this wave.
- **Gem.cpp** — 3 undone all OUTSIDE the current split (0x8229f730/0x82b79348/
  0x82b79d18 vs split 0x82b7a2a0–), 2 are bsim15-20; the easy Gem exacts were
  already harvested (`bw-Gem.o` MERGED). Low yield → skip.
- **TrackWatcher.cpp** — 5 targets are all trivial one-line `mImpl->X()` forwarders
  at 0x18-spacing, ALL bsim15-20 → textbook ICF-fold + sibling-aliasing (the
  TrackWidget Init-vs-Empty failure mode). High risk, low reward → skip.
- **VocalTrack.cpp / _M_pop_front_aux, _M_push_back_aux_v** — STL `_Deque_impl`
  template instantiations, bsim15-20 → permuter/sibling class, skip.
- **BandCharDesc.cpp** — 4 targets all OUTSIDE split (0x8232xxxx vs split
  0x82322da0–) and all ctor/SetShape bsim15-30; would need 4 micro-pins with
  pdata-check each; deferred (medium yield, higher effort than the INSIDE pins above).
- **Faders.cpp** — 2 targets OUTSIDE split + not wired; both getters, one bsim15-20.
  Lower yield than Sfx (which shares the not-wired-but-split shape); deferred.
- **EndingBonus / BandList / BandPerformer / FadePanel** and other 1–3-target
  band3/system TUs with only bsim15-20/20-30 and no INSIDE-split targets — viable
  next wave, below this wave's cut.
