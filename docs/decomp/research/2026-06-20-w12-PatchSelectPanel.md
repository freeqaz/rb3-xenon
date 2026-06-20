# W12 dossier — PatchSelectPanel (band3/meta_band)

Date: 2026-06-20. Mode: DISCOVER/PLANNER (read-only main). Baseline: main @ d2d3e53,
9301 matched. Lane owns exactly ONE TU: **PatchSelectPanel.cpp**.

## Verdict: ACTIONABLE — port + wire + pin (self-contained). expected_delta ≈ +14.

PatchSelectPanel is a small art-editing panel TU, currently **unported and unpinned**.
The `.h` already lives in `src/band3/meta_band/PatchSelectPanel.h` (byte-identical to
the rb3-Wii oracle). No `.cpp`, no objects.json entry, no splits pin → contributes 0
matched today. Porting it from rb3-Wii, wiring it, pinning a cleanly-bounded `.text`
span, and supplying a hand-built target_symbol_map keyed to the CORRECT cluster
addresses captures the byte-exact compiled cluster.

## Coords (bounded vs BOTH splits.txt neighbours — clean, no overlap)

- **PatchSelectPanel.cpp `.text`: start=0x82611248 end=0x82611F28** (size 0xCE0, 29 fns).
- pdata auto-derives on next ninja (do NOT hand-pin pdata).
- Nearest pinned below: `EditSetlistPanel.cpp` 0x825FE180–0x82603030.
- Nearest pinned above: `VoiceoverPanel.cpp` 0x826134E8–0x82613B70.
- Overlap self-check: NONE. The whole gap [0x82603030, 0x826134E8) is one unpinned
  auto-blob chunk (`auto_03_82603030_text.s`, "0x82603030..0x826134E8 | size 0x104B8")
  holding the network/standin panel cluster, then **PatchPanel**, then
  **PatchSelectPanel**, then the next panel TU.

## Boundary evidence (ground truth, not oracle)

The cluster was located by handler-token strings in `fingerprints.json` and confirmed
by Ghidra decompiles + the auto `.s` listing. The oracle is UNRELIABLE here (see below);
these addresses are the ground truth.

- **PatchSelectPanel::Handle @ 0x82611898** (sz=1316) references the exact BEGIN_HANDLERS
  tokens from PatchSelectPanel.cpp: `has_any_patches`, `patch_dir`,
  `setup_for_setlist_art`. Decompile = Symbol-cached message dispatch. UNAMBIGUOUS.
- **Lower edge @ 0x82611248/0x82611250.** Asm shows `fn_82611238` is a vbase adjustor
  thunk `lwz r11,-4(r4); subf r4,r11,r4; b fn_826103D0` → branches into **PatchPanel::Handle**
  (0x826103D0), so it belongs to the PatchPanel TU above. Then `except_data_82611250`
  (.4byte 0x82804210 / 0x820C6788) at 0x82611248 = EH funcinfo for PatchSelectPanel's
  ctor `fn_82611250`. The PatchPanel-family vtable thunks just above store to vtable
  disp `2FC15` (PatchPanel). PatchSelectPanel begins at 0x82611248.
- **PatchSelectPanel::ctor @ 0x82611250** (sz=176): zeroes this[0x3c..0x4c], sets
  this[0x50]=-1, byte this[0x54]=0, vtable=PTR @0x820C6744. Matches
  `PatchSelectPanel() : mPatchProvider(0)…mStartingPatchIx(-1), mStartWithMenu(0)`.
- **Upper edge @ 0x82611F28.** `fn_82611EDC` is the last PatchSelectPanel vtable thunk
  (stores disp `2FC49` = PatchSelectPanel vtable). `fn_82611EFC` = vbase deleting-dtor
  adjustor thunk (last PatchSelectPanel fn). Then `except_data_82611F30`
  (.4byte 0x82804210 / 0x820C6A40) at 0x82611F28 = funcinfo for `fn_82611F30`, the
  **next TU's** Handle dispatcher (different vtable 0x820C6A40, disp `2FC55`/`2FC61`).

## ⚠ Oracle trap (Waypoint-class): unified_id_rb3wii.json is GARBAGE for this TU

The BinDiff oracle maps PatchSelectPanel/PatchProvider functions to scattered, wrong
rb3_addr values with near-zero similarity (MWCC-Wii vs MSVC-X360 structural divergence):
`PatchSelectPanel::Handle → 0x82685ed0 (sim 0.08)`, `PatchProvider::NumData → 0x825cadf4
(0.42)`, `PatchProvider::InitData → 0x82865c48 (0.11)`, `SyncProperty → 0x8227e728 (0.49)`.
NONE land in the real cluster [0x82611248,0x82611F28).

Consequence: **do NOT trust `gen_game_target_map.py` auto-output for this TU** — it keys
on these bad addresses. The map must be hand-built from the CORRECT cluster addresses
(below) cross-checked to the compiled obj's MSVC-mangled symbols. (PatchPanel-family is
equally poisoned — same warning if the deepen-PatchPanel option is later attempted.)

## Confirmed function→address assignments (from decompile)

| addr | size | identity |
|------|------|----------|
| 0x82611250 | 176 | PatchSelectPanel::PatchSelectPanel (ctor; sets [0x50]=-1) |
| 0x82611300 | 68 | ??_E/atexit-style (Hmx::Object subobject dtor at this+0x14+0x5c) |
| 0x82611348 | 48 | NEW_OBJ helper / static New (calls 0x82558CB0) |
| 0x82611380 | 136 | PatchSelectPanel::Unload (release [0x40],[0x3c]; UIPanel::Unload) |
| 0x82611408 | 132 | PatchSelectPanel::DuplicatePatch (GetFirstEmptyPatch/Copy/GetPatchIndex) |
| 0x82611490 | 108 | PatchSelectPanel::Confirm (TourBand logo vs descriptor write) |
| 0x82611528 | 316 | PatchSelectPanel::Load (Symbol-cached; mDescriptor/mSourceProfile checks + new) |
| 0x82611688 | 112 | PatchSelectPanel::SetupForSetlistArt / SetDescriptor (writes [0x48..0x54]) |
| 0x826116F8 | 96 | PatchSelectPanel::SetupForCharacterPatch (ClosetMgr unk50/GetProfile) |
| 0x82611768 | 124 | PatchProvider::HasAnyPatches (iterate mPatches, HasLayers) |
| 0x826117E8 | 88 | PatchProvider scalar deleting dtor (~PatchProvider) |
| 0x82611840 | 80 | PatchSelectPanel::FinishLoad (Find UIList "patch.lst", SetProvider) |
| 0x82611898 | 1316 | PatchSelectPanel::Handle |
| 0x82611F-thunks | 32×10 | PatchSelectPanel vtable adjustor/vcall thunks (auto-pair) |

Remaining small fns (0x82611378, 0x82611500/0x82611510/0x82611758/0x82611664) +
the 0x82611DBC..0x82611EFC thunk run = vcall/vbase thunks; most auto-pair via CRT/thunk
naming once the named anchors are mapped. Source methods Enter/Draw/SetupForBandLogo are
tiny one-liners (UIPanel::Enter/Draw passthroughs) likely ICF-folded or merged — do not
expect 1:1 for all 18 source methods; the 13 real-bodied anchors + thunks drive the delta.

## ⚠ Layout note for the porter (resolve via objdiff, not a blocker)

Retail PatchSelectPanel members are shifted **+4** vs the rb3-Wii header in
`PatchSelectPanel.h`:
- Wii header: mPatchProvider 0x38, mGridProvider 0x3c, mPatchList 0x40, mDescriptor 0x44,
  mSourceProfile 0x48, mStartingPatchIx 0x4c, mStartWithMenu 0x50.
- Retail (from ctor + SetDescriptor/SetupForCharacterPatch decompiles): mPatchProvider
  0x3c, mGridProvider 0x40, mPatchList 0x44, mDescriptor 0x48, mSourceProfile 0x4c,
  mStartingPatchIx 0x50, mStartWithMenu 0x54.

EditSetlistPanel (a MATCHING pinned UIPanel sibling) has its first derived member at 0x38,
so this is NOT a UIPanel base-size delta — it is a vbtr/vtordisp placement specific to
PatchSelectPanel's MI of UIPanel(virtual Hmx::Object). The porter inserts a 4-byte head
pad / fixes the first-member offset to 0x3c and verifies each offset against the pinned
target via objdiff. Iterate; do not pre-commit a guess. (Adjust ONLY this TU's header view
or use a local struct; do not perturb the shared PatchSelectPanel.h offsets for other TUs —
PatchSelectPanel.h is currently consumed by nothing else compiled, so editing it is safe,
but verify with a whole-binary A/B.)

## Dependencies — all resolve (verified)

`src/band3/meta_band/PatchSelectPanel.h` (present, identical to Wii), `ClosetMgr.h`
(GetClosetMgr/unk50/GetProfile), `EditSetlistPanel.h` (mSetlistArt @0x5c), `BandProfile.h`
(mPatches @0x1c, GetTourBand, GetFirstEmptyPatch, GetPatchIndex), `bandobj/PatchDir.h`
(PatchDescriptor patchType/patchIndex, HasLayers, GetTex, Copy), `tour/TourBand.h`
(GetLogo, ChooseBandLogo), `ui/UIGridProvider.h`, `ui/UIListProvider.h`,
`ui/UIListMesh.h`, `ui/UIListLabel.h`. Include roots `/I src/band3` + `/I src/system`
resolve every include in the rb3-Wii .cpp. dc3 has NO PatchSelectPanel (game-only →
rb3-Wii is the sole, authoritative oracle; FALSE-FRIEND caveat N/A).

## Self-contained landing plan (ONE worktree)

1. `scripts/setup_worktree.sh /tmp/wt-patchselect w12-patchselect`.
2. Copy `../rb3/src/band3/meta_band/PatchSelectPanel.cpp` →
   `src/band3/meta_band/PatchSelectPanel.cpp`. Port MWCC→MSVC X360: keep
   BEGIN_HANDLERS/PROPSYNCS macros; `MILO_FAIL`/`MILO_ASSERT` resolve via Debug.h.
   Confirm `RELEASE`, `DeleteAll`, `Hmx::Object::New<RndMat>`, `kCopyDeep/kCopyShallow`,
   `SetTextToken`, `SetAlphaCut/Threshold/SetDiffuseTex` resolve (they do in src).
3. objects.json: add `"band3/meta_band/PatchSelectPanel.cpp": "NonMatching"` (alphabetical
   block near the existing PatchPanel entry @line 715). ADD-only.
4. splits.txt: add
   ```
   band3/meta_band/PatchSelectPanel.cpp:
       .text       start:0x82611248 end:0x82611F28
   ```
   Pin **.text only**; let dtk auto-derive .pdata. `touch config/45410914/config.yml`.
5. Fix the +4 member layout in PatchSelectPanel.h (this-TU header view): first member
   mPatchProvider 0x38→0x3c etc. Drive each offset from objdiff against the pinned target.
6. Generate map entries: after a first compile produces
   `build/45410914/src/band3/meta_band/PatchSelectPanel.obj`, build the explicit
   `0xADDR → mangled` entries for the named anchors (table above) and ADD them to
   `scripts/target_symbol_map.json` (ADD-only; never regen wholesale). Use the
   VoiceoverPanel block (7 entries) as the format template — e.g.
   `"0x82611898": "?Handle@PatchSelectPanel@@UAA?AVDataNode@@PAVDataArray@@_N@Z"`,
   `"0x82611250": "??0PatchSelectPanel@@QAA@XZ"` (mangled exactly as the obj defines).
   Pair each address to the obj's mangled symbol by function-body correspondence
   (sizes/strings in the table), NOT via the poisoned unified_id oracle.
7. VERIFY: `rm -f build/45410914/target_symbol_renames.stamp && touch
   config/45410914/config.yml && NINJA_JOBS=8 tools/fresh_report.sh`; read
   measures.matched_functions; **re-run** (splits-only divergence warning is a known FP).
8. HONESTY GATE: landable iff net ≥ +1, headline net == sum of intended PatchSelectPanel
   gains, zero unexplained per-unit regressions in other units, no ≥8-contiguous FOREIGN
   fn_@0% run inside the span (own thunks bracketed by own named = OK).

## Optional same-worktree deepen: PatchPanel (NOT required; flag risk)

PatchPanel.cpp is listed NonMatching in objects.json but the SOURCE FILE IS ABSENT from
src/ and there is no splits pin → it is a dangling scaffold contributing 0. "Deepen" here
would mean a full first-time PORT+PIN of PatchPanel (a LARGE TU: PatchPanel + LayerProvider
+ CategoryProvider + StickerProvider, ~0x826039xx span up to 0x82611248, multiple vtables
2FB2D..2FC15, the 2696-byte Handle @0x826103D0, pragma push/pop merge_float_consts /
pool_data / dont_inline blocks). That is a separate lane-sized effort and the oracle is
equally poisoned for it. **Recommendation: keep this lane PatchSelectPanel-only.** Do NOT
bundle PatchPanel; surface it as a discovered_frontier item for its own lane. If
attempted opportunistically, its lower bound is the network-panel cluster end (the 2FB2D
vtable group start ≈ 0x8260BED8/0x8260BF18) and its upper bound is 0x82611248 — but verify
the lower edge independently before pinning.

## Expected delta

29 fns in span; ~13 real-bodied anchors + ~13 thunks that auto-pair once the named
anchors are mapped and the vtable is correct; the 1316-byte Handle is the hardest single
unit (may land <100% on first pass — Symbol-cache + DataNode codegen). Conservative
**+14** (range +10…+18). The big Handle and the +4 layout are the two risk items.
