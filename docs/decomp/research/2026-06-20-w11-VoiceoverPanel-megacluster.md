# W11 — VoiceoverPanel megacluster scout (meta_band panel belt)

Date: 2026-06-20  •  Mode: DISCOVER/PLANNER (read-only in main @ 9159)
TU owned: **VoiceoverPanel-megacluster** `[0x825FC080, 0x8261AAF0)`

## TL;DR

The given window is **NOT a single TU** and its start (`0x825FC080`) is **mid-stream**
inside the giant `CustomizePanel.cpp` family. It is a sprawling meta_band UI-panel belt
of ~10 interleaved TUs. As instructed, this is a SCOUT-THEN-PORT: I boundary-derived the
**first clean single-TU sub-cluster = `EditSetlistPanel.cpp`** at
`.text [0x825FE180, 0x82603030)` and produced a self-contained port-then-pin plan for it.
The rest of the belt is emitted as `discovered_frontier`.

Verdict: **REAL_ACTIONABLE** (EditSetlistPanel sub-cluster). expected_delta ~+15 (first batch).

## How the megacluster was mapped (string-content + Ghidra ground-truth)

Source signals: `fingerprints.json` (VA→referenced-strings/callees), rb3-Wii oracle
(`../rb3/src/band3/meta_band/*`), Ghidra MCP (port 8002) for boundary decompiles.
**No mangled names in COFF auto_03** — it is a raw dtk dump (`fn_<VA>` only), so TU
ownership comes from string cross-ref, not COFF symbol names.

Address-ordered TU attribution across the window (string anchors → rb3-Wii owner):

| .text region (approx) | TU (rb3-Wii owner) | anchor strings |
|---|---|---|
| ≤ 0x825FE180 (window start cuts mid-TU) | **CustomizePanel.cpp** + CharacterCreatorPanel / ContentDeletePanel / ContentLoadingPanel / AssetProvider (all interleaved) | `%s.mesh`, `asset_provider`, `in_clothing_state`, `get_face_options_selected`, `key_unlocked_face_paint`, `bandana/hat`, `drum/feet/rings`, `refresh_started`, `finding_additional_content` |
| **0x825FE180 – 0x82603030** | **EditSetlistPanel.cpp** ← PORT TARGET | `battle_default_desc`, `setlist_save_share`, `weeks`, `goto_create_dialog`, `leave_setlist`, `battle_maxrank`, `create_battle/create_setlist/done_editing/editing_setlist` |
| 0x82603030 – ~0x82603FA0 | **InterstitialPanel.cpp** | `vignette_outro_done` |
| ~0x82603FA0 – ~0x826080A0 | **MainHubPanel.cpp** | `advance`, `update_message_counter`, `update_state_view`, `role_is_global/role_rank`, `get_motd`, `has_role_info`, `message_rotation_ms` |
| ~0x826080A0 – ~0x8260B858 | **ManageBandPanel.cpp** | `hide_character`, `reward_vignettes`, `check_for_kickout_condition`, `get_char_provider/get_history_provider/get_selected_standin` |
| ~0x8260B858 – ~0x8260C090 | **NewAwardPanel.cpp** | `handle_continue`, `update_provider`, `get_num_assets`, `get_user` |
| ~0x8260C090 – ~0x826134E8 | **PatchPanel.cpp / PatchSelectPanel.cpp** | `get_mat_for_data`, `patch`, `stickers`, `copy_from_patch/copy_to_patch/edit_layer`, `has_any_patches/patch_dir/setup_for_setlist_art` |
| 0x826134E8 – 0x82613B70 | **VoiceoverPanel.cpp** (ALREADY PINNED, WAVE-10 +9) | `play_voiceover` |
| ~0x826138F8 – < 0x8261AAF0 | network join panels (join_result / finding_presence / join_invite / must_not_be_a_guest) | — |

`ng/startup_autosave_esrb_keep.milo` recurs binary-wide (App.cpp const) → **ownership noise**, ignored.

## Chosen sub-cluster: EditSetlistPanel.cpp

### Exact coords (bounded vs BOTH splits.txt neighbours)
`.text  start:0x825FE180  end:0x82603030`  (0x2EB0 = 11952 bytes)

- **Below** nearest pinned .text: `CalibrationPanel.cpp` ends `0x825EF598` (large gap; CustomizePanel family fills it, unpinned).
- **Above** nearest pinned .text: `band3/meta_band/VoiceoverPanel.cpp` starts `0x826134E8` (large gap; InterstitialPanel/MainHubPanel/ManageBandPanel/Patch* fill it, unpinned).
- **Overlap self-check: PASSED.** Parsed all 638 `.text` ranges in splits.txt + the proposed pin → **0 overlaps**. Proposed pin sits in a clean gap; no adjacency to any existing pin.

### Boundary evidence (Ghidra, port 8002)
- **Start 0x825FE180** = `EditSetlistPanel::EditSetlistPanel` ctor: installs vtable `PTR_..._820c198c`, UIPanel vbase `PTR_LAB_820c1934`, two `String` ctors at +0x3c/+0x48 (= `mSetlistName 0x38` after String header / `mSetlistDescription 0x44`), defaults `unk54=10, unk58=...`. `except_data_825FE180 = .text:0x825FE178` and `except_record_825FE180 = .rdata:0x820C19F0` are this function's own EH data immediately preceding it. (The 12b `fn_825FE168` + 8b except_data at 0x825FE178 are the boundary marker; CustomizePanel's giant 5012b `fn_825FC6E0` and its ~119-byte funclet tail at 0x825FDA74+ end just below.)
- **0x825FE5A0** (first string fn, `battle_default_desc`) decompiles to a setter writing fields 0x3c/0x48/0x54/0x58/0x60/0x88/0xa0 — matches EditSetlistPanel layout (`mProfile 0x88`, `mEditingSetlist 0x8c`, String members) → confirmed `CreateBattle`-class method.
- **End 0x82603030** = `InterstitialPanel::Handle`-class dispatcher (`vignette_outro_done` Symbol via `DAT_82dcf8e8`, string @ 0x820c2d20). The last EditSetlist fn is `fn_82602FD0` (88b, vector-deleting-dtor: operates at `param-0x9c`, the EditSetlistPanel object size), ending at 0x82603028; 8b EH gap to 0x82603030.
- **All 6 string-bearing fns strictly inside the span are EditSetlistPanel strings — ZERO foreign strings** (honesty-gate green for string content).

### Span composition
190 functions in span: ~71 substantial (>44b or >1 callee), ~119 tiny (funclets / vtable thunks / MakeString templates / OnMsg adapter thunks). rb3-Wii `EditSetlistPanel.cpp` has 27 named methods (Enter/Poll/Exiting/Unload/CreateSetlist/EditSetlist/CreateBattle/GetMessageToken/GetTitleToken/GetArtTex/DoneEditing/MessageOK/3×OnMsg/VerifyStrings*/SetEditState/SetUIState/FailWithReason/SymToDayCount/SymToTimeUnits/DayCountToSym/CleanupStringVerify + Handle/SyncProperty/ctor/dtor/factory). The 71 substantial > 27 named is expected (dtor variants, Handle/OnMsg trampolines, MakeString instantiations, inlined-helper expansions).

### Port feasibility — GREEN
- rb3-Wii source present: `../rb3/src/band3/meta_band/EditSetlistPanel.cpp` (16972 B, 27 real-bodied methods, no stubs).
- Header already in tree: `src/band3/meta_band/EditSetlistPanel.h`.
- All `#include` deps present in rb3-xenon: `meta_band/BandProfile.h`, `net_band/DataResults.h` (`DataResultList`), `bandobj/PatchDir.h` (`PatchDescriptor`), `net_band/RockCentralMsgs.h`, `ui/UIPanel.h`, `os/PlatformMgr.h`.
- Base = `UIPanel` + MessageReceiver mixins (3× `OnMsg` → `Handle` dispatch) — the SAME shape as the WAVE-10 sibling `VoiceoverPanel.cpp` that just landed +9 in this exact belt → proven template.
- NOT yet in `objects.json` (fresh wire). PatchPanel/VoiceoverPanel show the `NonMatching` wiring pattern.

## Self-contained port-then-pin plan (one worktree, independently landable vs main@9159)

1. `scripts/setup_worktree.sh /tmp/w11-editsetlist editsetlist-port` (CoW buildable worktree).
2. Copy `../rb3/src/band3/meta_band/EditSetlistPanel.cpp` → `src/band3/meta_band/EditSetlistPanel.cpp`. Port MWCC→MSVC X360 using `VoiceoverPanel.cpp` as the in-tree style template (it shares UIPanel + OnMsg/Handle shape). Keep RB3 game semantics from rb3-Wii (DC3 is a FALSE FRIEND for game code — do not consult DC3 for these panels).
3. Wire `objects.json`: add `"band3/meta_band/EditSetlistPanel.cpp": "NonMatching"`.
4. Pin in `splits.txt` under `band3/meta_band/EditSetlistPanel.cpp:` with `.text start:0x825FE180 end:0x82603030`. (dtk auto-back-fills the matching `.pdata` on next `ninja` — do NOT hand-pin pdata; never gap-shrink.)
5. `tools/gen_game_target_map.py` to emit the EditSetlistPanel renamer entries; **ADD** them to `scripts/target_symbol_map.json` (never regenerate wholesale — poison rule).
6. `rm -f build/45410914/target_symbol_renames.stamp && touch config/45410914/config.yml && NINJA_JOBS=8 tools/fresh_report.sh`. dtk emits `asm/EditSetlistPanel.s` + `obj/.../EditSetlistPanel.obj`.
7. Re-derive precise fn addresses from `EditSetlistPanel.s` (`^.fn fn_<hex>`); batch-check via `/batch-check`. Convert trivial accessors / Symbol getters (`SymToDayCount`/`DayCountToSym`/`GetMessageToken`/`GetTitleToken`) + MakeString thunks first (cheap matches); then `Enter/Poll/Exiting/Unload/CreateSetlist/CreateBattle`; defer regalloc/funclet-class near-misses.
8. Add `reveal`-style map entries for any fn already byte-exact-but-0% (no body change).
9. **Honesty gate before declaring landable:** re-run overlap self-check (parse all .text+.pdata, assert 0 overlaps); `report.json measures.matched_functions` net ≥ +1; no ≥8-contiguous FOREIGN fn@0% run (own STL/MakeString/thunks bracketed by own named = OK); headline net == sum of intended EditSetlistPanel gains. Re-run fresh_report.sh once for the splits-only FP warning.

### Expected delta
~+15 first batch (conservative; sibling VoiceoverPanel landed +9 and is smaller). Upside to +20–25 as OnMsg/SetEditState/SetUIState bodies land in a follow-up deepen pass.

## Pitfalls / honesty notes
- DO NOT pin the whole window — multi-class = guaranteed honesty-gate fail (InterstitialPanel scatter at 0x82603030 is the proof).
- The `vignette_outro_done` (InterstitialPanel) fn at 0x82603030 is the hard upper boundary; never extend the EditSetlist pin past it.
- No shared-header lever needed; the WAVE-9/10 Handle/MILO_MESSAGE_TIMERS keystone already landed — do NOT touch Object.h/ObjMacros.h/UIComponent.h for this.
- DC3 is a FALSE FRIEND for these RB3-specific game panels; rb3-Wii is authoritative.

## flag_foundational
**false** — self-contained game-code port; no binary-wide/shared-header change.
