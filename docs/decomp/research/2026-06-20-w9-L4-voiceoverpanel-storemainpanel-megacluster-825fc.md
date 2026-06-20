# W9-L4 dossier — "voiceoverpanel-storemainpanel-megacluster-825fc"

Date: 2026-06-20. Mode: adversarial discover/planner (Opus L4), READ-ONLY in main.
Baseline: main @ 8314 matched. Region scouted: 0x825FC080..0x82628658.

## Verdict: REAL_ACTIONABLE — but the frontier framing is REFUTED as stated.

The scout described a single "VoiceoverPanel + StoreMainPanel megacluster, 67
Handle-tails, est +30." That framing is **wrong on three counts**:

1. **Not 67 functions — 1631** COFF function symbols live in
   [0x825FC080, 0x82628658). The window is a 0x2C5D8-byte (~181 KB) span, the
   bulk of the 0x23FD0 unpinned gap between CalibrationPanel.cpp (ends
   0x825EF598) and Cam.cpp (0x82613568) / NameGenerator.cpp (the only two pins
   inside, both tiny: Cam 0x64, NameGen 0x280).
2. **Not 2 TUs — ~18-21 distinct meta_band panel TUs** in sequence, COMDAT-
   interleaved. VoiceoverPanel and StoreMainPanel are just two of them, and
   neither sits at the start of the window.
3. **The named owners were mis-placed.** `marquee_path`/`is_waiting_on_enum`
   ARE StoreMainPanel (confirmed), but they sit at 0x8261FAE8 deep in the
   window, not at the head. `marquee_rotation_ms` is **SelectDifficultyPanel**,
   not StoreMainPanel.

The underlying opportunity is REAL: a dense forest of unwired rb3-Wii game
panels (band3/meta_band), almost none wired. This is a genuine multi-TU
game-port vein, but it must be decomposed into per-TU self-contained items
(scout/boundary-derive FIRST, then one port item per confirmed clean TU), NOT
pinned as one block.

## Ground truth (COFF auto_03_82260000_text.obj, authoritative retail symbols)

- Bounding pins: CalibrationPanel.cpp .text ends 0x825EF598; next real pins are
  Cam.cpp 0x82613568-0x826135CC and NameGenerator.cpp 0x82626F80-0x82627200
  (already pinned). The window is otherwise UNPINNED.
- 1631 `fn_` starts + 288 `except_data_` in the window.

## TU map (string-anchor owner ID + COFF fn-start refinement)

Owners identified by resolving rdata string refs (lis/addi disasm via
/tmp/scanstr.py over auto_03 text + auto_00 rdata) and grepping rb3-Wii
src/band3/meta_band. Spans are first-fn-start to next-TU-first-fn-start
(approximate; the messy interior needs per-fn boundary-derive).

| span | ~fns | owner TU (rb3-Wii) | distinctive strings |
|------|------|--------------------|---------------------|
| 0x825FC080-0x825FE478 | 80 | CustomizePanel.cpp | set_current_boutique, preview_asset, set_patch_menu_return_state, take_portrait, bitfield setters |
| 0x825FE478-0x82602DE8 | 169 | EditSetlistPanel.cpp | setlist_default_name, edit_setlist, create_setlist_success |
| 0x82602DE8-0x82603318 | 14 | InterstitialPanel.cpp | transition_camshot_done, vignette_outro |
| 0x82603318-0x82608380 | 181 | MainHubPanel.cpp | MainHubAdvanceMsg, message_motd, update_pool_info |
| 0x82608380-0x8260AA00 | 93 | ManageBandPanel.cpp | vignetteviewer_hidden_title, reward_vignettes, queue_reward_vignette |
| 0x8260AA00-0x8260B8A0 | 33 | MultiSelectListPanel.cpp | full_selection.mesh, sel_section.lst, fake_component_scroll |
| 0x8260B8A0-0x8260C000 | 16 | NewAwardPanel.cpp | handle_continue, get_award, pop_and_show_first_award |
| 0x8260C000-0x8260D7F0 | 58 | NewAwardPanel / name-matcher (male.mat/female.mat/gender) + a message-queue panel (queue_message/show_message — strings not in band3, likely Wii-stripped) | MIXED — needs per-fn boundary |
| 0x8260D7F0-0x8260FFD8 | 78 | CharCache.cpp (editing_patch) + ArtMaker/sticker (art_maker, sticker.mat, patch_layer_fmt) | MIXED — needs per-fn boundary |
| 0x8260FFD8-0x82611840 | 56 | PatchPanel.cpp | create_layer, swap_layers, copy_from_patch (ALREADY in objects.json NonMatching, NOT pinned) |
| 0x82611840-0x826134E8 | 71 | PatchSelectPanel.cpp | patch.lst, has_any_patches, setup_for_band_logo |
| 0x826134E8-0x82614318 | 35 | VoiceoverPanel.cpp | voiceover_fade_duration, set_voiceover_symbol, play_voiceover |
| 0x82614318-0x82616938 | 84 | finding_presence panel (set_presence/join_invite/finding_presence_* -> resolves to MetaPanel.cpp/Messages.cpp, likely a FindPlayers/MetaPanel TU) | NEEDS ID |
| 0x82616938-0x8261C1F0 | 199 | SelectDifficultyPanel.cpp | marquee_rotation_ms, update_tour_setlist_label, party_shuffle |
| 0x8261C1F0-0x8261C918 | 21 | SongSelectPanel.cpp | set_mini_leaderboard_showing |
| 0x8261C918-0x8261E478 | 73 | StoreInfoPanel.cpp | no_recommendations, fetch_recommendations, recommendation_path |
| 0x8261E478-0x8261FF18 | 53 | StoreMainPanel.cpp | cover_art_%02i.mat, album_scroll.anim, marquee_path, is_waiting_on_enum |
| 0x8261FF18-0x826211E8 | 45 | StoreMenuPanel.cpp | menu_list, get_menu_provider, set_menu_waiting |
| 0x826211E8-0x826224B8 | 51 | TrainingPanel.cpp | trainer_drums, refresh_lessons_list, goto_trainer |
| 0x826224B8-0x826265A8 | 159 | TokenRedemptionPanel.cpp | offer_id, token_redemption, get_offers_for_token, demo_upgrade |
| 0x826265A8-0x82626F80 | 17 | NextSongPanel.cpp(part) | setlist_to_store_screen_timeout, details_page_size, detail_component_parent |

Note: fn-counts are inflated by interleaved STL/template/inline thunks
(EditSetlist 169, MainHub 181, SelectDifficulty 199, TokenRedemption 159 are NOT
that many own methods). Per roadmap EV rule, rank by retail-map NAMED-METHOD
count, not fn count. Realistic per-panel own-named yield ~5-20.

## Wired status (objects.json / splits.txt)

ALL UNWIRED except: PatchPanel.cpp (objects.json NonMatching, NOT pinned);
NameGenerator.cpp (wired + pinned, the tail anchor). Only CalibrationPanel +
PatchPanel from this whole panel family are in objects.json at all.

## Port complexity

- Macro infra present: ObjMacros.h has BEGIN_HANDLERS/HANDLE_ACTION/
  HANDLE_SUPERCLASS/END_HANDLERS (the scout's "macro prereq" is ALREADY met).
- VoiceoverPanel.h, StoreMainPanel.h etc. already exist in xenon src/.
- Wii-dep gotcha: VoiceoverPanel.cpp + SongSelectPanel.cpp include
  `os/ContentMgr_Wii.h` (absent in xenon) -> must swap to 360 ContentMgr during
  port. InterstitialPanel.cpp + StoreMenuPanel.cpp have NO Wii deps (cleanest).
- Each TU is a self-contained game port: scaffold .cpp (header exists), pin the
  refined .text span (pdata auto-derives), gen target_symbol_map entries
  (tools/gen_game_target_map.py from rb3-Wii oracle), port MWCC->MSVC, objdiff.

## Why each pin is attribution_risk=true

These are NOT byte-exact-already extensions of a wired TU (the dominant wave-3
sliver vein). They are brand-new wirings of unpinned game TUs that require an
actual source port to register matches. A pin alone reads false-0 without the
map entry; the honesty gate (matched>0, no >=8 foreign fn_@0% run) must hold
per-TU after the port. Boundaries are string-anchor-derived, not COFF-symbol-
exact, so the boundary-derive item must confirm fn-start alignment before any
pin lands.

## Recommended decomposition

1. Boundary-derive scout (one item): walk auto_03 fn-by-fn across
   0x825FC080..0x82628658, resolve per-fn rdata strings, fix the two MIXED
   spans (0x8260C000 name-matcher/message-queue; 0x8260D7F0 CharCache/ArtMaker)
   and the unidentified finding_presence TU (0x82614318), emit exact
   per-TU [min_fn, max_fn+size) spans. Produces the inputs for the port items.
2. Per-TU port items (clean first): InterstitialPanel, StoreMenuPanel (no Wii
   dep, small) -> then VoiceoverPanel, SongSelectPanel, StoreMainPanel,
   PatchSelectPanel, TrainingPanel, NewAwardPanel. Each does scaffold+wire+pin+
   port+map+objdiff in ONE worktree, lands independently vs main@8314.

## Tools used (reusable)
- /tmp/coffsyms.py, /tmp/containing_fn.py — COFF fn enumeration + containing-fn.
- /tmp/scanstr.py — lis/addi -> rdata string resolver over auto_03 + auto_00.
- rb3-Wii oracle: ~/code/milohax/rb3/src/band3/meta_band.
