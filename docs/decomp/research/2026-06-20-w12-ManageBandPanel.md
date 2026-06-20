# W12 dossier — ManageBandPanel (band3/meta_band)

**Date:** 2026-06-20 · **Mode:** DISCOVER/PLANNER (read-only main) · **Baseline:** main @ d2d3e53, 9301 matched.
**Verdict:** ACTIONABLE — oracle-HAVE, header already present + identical to oracle, all deps resolved,
TU located + bounded cleanly in the unpinned auto_03 blob. Self-contained port-then-pin. Expected ~+18.

---

## 1. What it is

`ManageBandPanel` is the band-management UI panel (set band logo / standins / character preview /
reward-vignette history). The TU defines **two classes**:

- `VignetteViewerProvider` (a `UIListProvider` + `Hmx::Object`, MI) — the reward-vignette list source.
- `ManageBandPanel : public UIPanel` — the panel itself.

Oracle: `../rb3/src/band3/meta_band/ManageBandPanel.{cpp,h}` (rb3-Wii DEV decomp, authoritative for game code).
Our tree already has `src/band3/meta_band/ManageBandPanel.h` — **byte-identical to the oracle header**
(`diff -q` clean). No `.cpp` yet, not in `objects.json`, not in `splits.txt`. Pure unwired gameport.

## 2. Location (retail XEX, title 45410914)

Located by COFF-relocation string-fingerprinting of `auto_03_82260000_text.obj` (the `.text` blob,
base VA 0x82260000) cross-referenced against `auto_00_82000400_rdata.obj` (rdata strings). Method:
parse the COFF symbol table (65k `fn_<addr>` symbols) + the 411,183-entry relocation table; for each
HI16/LO16 reloc into the rdata string region, resolve the owning function by VA.

Key anchors (all in the auto_03 blob, unpinned):
- `fn_826097B0` (size 2092) = **`BEGIN_HANDLERS(ManageBandPanel)` dispatcher** — references every unique
  handler command string: `refresh_to_main_state` (0x820c48f4), `refresh_to_standins_state`,
  `update_character_from_standin_list`, `update_character_from_char_list`, `queue_reward_vignette`,
  `get_history_provider`, `get_standin_provider`, `get_band_logo_tex`, `set_standin`,
  `set_selected_standin`, `refresh_standin_list`, `check_for_kickout_condition`, `get_state`/`set_state`,
  `get_profile`/`set_profile`. **Decisive single-TU fingerprint.**
- `fn_826084D8` = `VignetteViewerProvider::Handle` (refs `is_locked` 0x820c42bc, `get_screen` 0x820c42b0).
- `fn_82608380` = `VignetteViewerProvider::IsLocked` (refs `vignetteviewer_hidden_title` 0x820c4258).
- `fn_82609298` = `RefreshAll` body (refs `vignetteviewer_hidden_title`).
- `fn_82609410` = RefreshAll/logo path (refs `reward_vignettes` 0x820c46d8, `acc_bandlogo` 0x820a1d74).
- `fn_826080A0` = `SetStandIn` (refs `acc_standins` 0x820a1d64).
- RTTI type descriptor `.?AVManageBandPanel@@` @ 0x82c43bc4; classname string @ 0x8209a2e8.

### TU span (bounded)

**`.text  start:0x82607EE0  end:0x8260A2F8`**  (0x2418 bytes, 85 functions incl. funclets/static-init thunks/MI adjustors).

- **Lower bound 0x82607EE0**: the prior TU is **MainHubPanel.cpp** (unpinned; identified by its strings
  `message_rotation_ms`/`override_ended`/`has_unlinked_motd`/`cancel_waiting_override` →
  `../rb3/src/band3/meta_band/MainHubPanel.cpp`). MainHubPanel's last real function `fn_82607D38`
  ('override_ended', size 424) ends exactly at 0x82607EE0. The function at 0x82607EE0 references
  **`lbl_820C414C` + `lbl_820C40F4`** — the ManageBandPanel C++ EH state tables (those tables target
  0x826081f0/0x82608cc0/0x82609618/0x82609240/0x82608fc0 — all MBP bodies). So 0x82607EE0 is
  unambiguously the first ManageBandPanel function.
- **Upper bound 0x8260A2F8**: the dispatcher ends at 0x82609FDC; the run 0x82609FDC–0x8260A21C are MBP
  static-init/atexit/guard thunks (`??__E`/`??__F`-class, accessing the atexit chain at 0x82dcfaf0 —
  the `static Message init`/`static Message msgUpdateState` ctors), then EH cleanup funclets
  0x8260A21C/0x8260A2BC, then the final catch funclet **0x8260A2E8** (size 16, `b -0xb40` back into
  `fn_826097B0`, referenced by the MBP EH table). The MBP EH/jump tables (0x820c40f4/414c/44fc/4554)
  reference code VAs up to a **max of 0x8260A2E8**. The next function 0x8260A2F8 belongs to the next TU
  (a selection-list panel: strings `sel_section.lst`, `full_selection.mesh` at fn 0x8260aa28). Clean cut.

### Pin safety (bound vs BOTH splits.txt neighbours)

Nearest pinned `.text` ranges in `config/45410914/splits.txt`:
- below: `EditSetlistPanel.cpp  .text 0x825FE180–0x82603030`
- above: `VoiceoverPanel.cpp    .text 0x826134E8–0x82613B70`

The span [0x82607EE0, 0x8260A2F8) sits wholly inside the unpinned gap [0x82603030, 0x826134E8),
**no overlap** with either neighbour (and MainHubPanel / the next selection-panel TU in that gap are
both still unpinned, so no conflict there either). pdata auto-derives on first ninja (dtk back-fills).

## 3. Dependencies — ALL RESOLVED (no header surgery needed)

Every include + member + method the oracle `.cpp` uses exists in our tree with matching signatures:

| Symbol used | Where it lives (ours) | Status |
|---|---|---|
| `BandProfile::AccessAccomplishmentProgress/GetTourBand/GetBandLogoTex/GetStandIn/AccessStandIn/GetNumStandins/GetCharFromGuid/GetAssociatedLocalBandUser` | `meta_band/BandProfile.h` | OK |
| `BandProfile::GetPadNum()` | inherited from `Profile` base — `system/meta/Profile.h:37` | OK |
| `AccomplishmentProgress::mNewRewardVignettes` (0xa8) `unkb0` (0xb0 std::set) `AddNewRewardVignette` | `meta_band/AccomplishmentProgress.h:228/229/101` | OK |
| `ClosetMgr::unk3c` (0x3c) `PreviewCharacter(bool,bool)` `GetClosetMgr()` | `meta_band/ClosetMgr.h:89/22/79` | OK |
| `CharProvider::unk30/Reload/GetCharData/IsIndexNone/IsIndexPrefab/IsIndexCustomChar/DataSymbol` | `meta_band/CharProvider.h` | OK |
| `StandIn::IsNone/IsPrefabCharacter/IsCustomCharacter/SetNone/SetName/SetGuid/mName/mGuid` | `meta_band/StandIn.h:11-23` | OK |
| `AccomplishmentManager::EarnAccomplishment(LocalBandUser*,Symbol)` + `(BandProfile*,Symbol)`, `TheAccomplishmentMgr` | `meta_band/AccomplishmentManager.h:125/126/215` | OK |
| `Hmx::Object::Property(DataArray*,bool)` | `system/obj/Object.h:1722` | OK |
| `PrefabMgr::GetPrefabMgr/GetPrefab`, `StandInProvider`, `UIPanel::Enter/Exit/Unload`, `TheProfileMgr`/`ThePlatformMgr`/`TheUIEventMgr`, `TheCharSync`, `TheWiiProfileMgr.IsPadAGuest`, `meta/WiiProfileMgr.h` (`system/meta/WiiProfileMgr.h`) | respective headers | OK |

No DC3 false-friend risk: this is game code, ported straight from the rb3-Wii oracle. No struct-layout
reconstruction, no gated members, no shared-header edit → **not foundational; fully independent.**

## 4. Self-contained port-then-pin plan (one worktree)

1. `scripts/setup_worktree.sh /home/free/code/milohax/wt-managebandpanel w12-managebandpanel` ; cd in.
   Record baseline: `python3 -c "import json;print(json.load(open('build/45410914/report.json'))['measures']['matched_functions'])"`.
2. Copy oracle source: `cp ../rb3/src/band3/meta_band/ManageBandPanel.cpp src/band3/meta_band/ManageBandPanel.cpp`
   (header already present + identical — leave it). Port MWCC→MSVC as needed (this TU is plain C++:
   `nullptr`→`NULL` if the toolchain complains; keep the `#pragma push/dont_inline on/pop` around
   `BEGIN_HANDLERS(ManageBandPanel)` exactly as the oracle has it — it forces the dispatcher out-of-line,
   matching the retail 2092-byte `fn_826097B0`). The two file-scope `inline` VignetteViewerProvider
   methods (`Text`/`IsLocked`/`GetScreen`) and its `BEGIN_HANDLERS(VignetteViewerProvider)` stay in the .cpp.
3. Add to `config/45410914/objects.json` (alphabetic neighbourhood of the other meta_band panels):
   `"band3/meta_band/ManageBandPanel.cpp": "NonMatching",`
4. Add to `config/45410914/splits.txt`:
   ```
   band3/meta_band/ManageBandPanel.cpp:
       .text       start:0x82607EE0 end:0x8260A2F8
   ```
   `touch config/45410914/config.yml` (let dtk auto-derive .pdata). If configure.py must re-run, pass the
   explicit forked-tool flags (worktree-dtk trap).
5. Generate the target symbol map so objdiff can pair anonymous `fn_<addr>` ↔ our MSVC-mangled names:
   `python3 tools/gen_game_target_map.py --tu band3/meta_band/ManageBandPanel.cpp --apply`
   (uses the rb3-Wii BinDiff oracle `unified_id_rb3wii.json`; ADD-only, never regenerate wholesale).
   If the oracle lacks entries for this span, fall back to `tools/reveal_sweep.py` after the bytes match.
6. `./tools/ninja-locked 2>&1 | tee /tmp/managebandpanel_build.log`. Iterate per-function via
   `run_objdiff(full_listing=True, project_dir=<worktree>)` on the named MBP/VignetteViewerProvider
   symbols; fix MWCC→MSVC body divergences (expected: trivial accessors + the handler dispatchers go
   byte-exact first; RefreshAll's std::list/std::set iteration + the standin/char update paths next).
7. Reveal pass for byte-exact-but-anonymous fns:
   `rm build/45410914/target_symbol_renames.stamp && touch config/45410914/config.yml`, rebuild, then
   `tools/reveal_sweep.py` (+ gate + merge into `scripts/target_symbol_map.json`, ADD-only).
8. **Whole-binary A/B**: fresh full report (`tools/fresh_report.sh`), re-run once for the splits-only FP.
   Landable iff `measures.matched_functions` net ≥ +1, zero unexplained per-unit regressions elsewhere,
   no ≥8-contiguous FOREIGN `fn_@0%` run inside the new unit (own STL/thunks bracketed by own named = OK).
9. Deliver patch (`git diff HEAD > ~/tmp/w12/managebandpanel.patch`); never commit to main.

## 5. Expected delta

85 functions in the span, but those split into ~33 oracle-named methods (two classes) + EH funclets +
static-init/guard thunks + MI adjustor thunks. Realistic byte-exact named matches: the trivial accessors
(`SetProfile`/`ClearProfile`/`SetSelectedStandIn`/`GetProfile`/`GetSelectedStandIn`/`GetManageBandState`/
the four `Get*Provider`/`GetBandLogoTex`), the two `OnMsg`, `ShowCharacter`/`HideCharacter`,
`RefreshToMainState`/`RefreshToStandinsState`, `SetManageBandState`, `CheckForKickoutCondition`,
`SetStandIn`, `RefreshStandinList`, `QueueRewardVignette`, the two `UpdateCharacterFrom*`, the ctor/dtor,
both `BEGIN_HANDLERS` dispatchers, and the VignetteViewerProvider `IsLocked`/`GetScreen`/`Text`. Some
funclets/thunks reveal byte-exact for free. **Estimate +18** (consistent with the prompt). RefreshAll
(std::list scan + std::set::find + MetaPanel::sUnlockAll) is the one body most likely to need a regalloc
permuter pass — defer it if it sticks; it does not block the rest.

## 6. Risks / notes

- **`#pragma dont_inline` parity**: MSVC X360 honours `#pragma dont_inline on/off` (the oracle wraps only
  the ManageBandPanel `BEGIN_HANDLERS`). Keep it; dropping it would let MSVC inline the dispatcher and
  break the 2092-byte match. (If the exact pragma spelling differs under MSVC, the established analog is
  per-method `__declspec(noinline)` or the project's existing handler-dispatcher convention — check a
  sibling pinned panel that already matched its dispatcher.)
- **MI / adjustor thunks**: VignetteViewerProvider is `UIListProvider, Hmx::Object` (multiple inheritance).
  The small 32-byte thunks in the tail are its vtable adjustor thunks; they pair by the renamer/reveal,
  not by source. Not a wall — just don't expect to "write" them.
- This is **not foundational** (no shared-header / binary-wide lever). It is independently landable vs
  main @ d2d3e53 with zero coupling to any other in-flight lane.

## 7. Frontier discovered (adjacent, same gap, same method)

While bounding, **MainHubPanel.cpp** was positively identified as the immediately-preceding unwired TU
in the same auto_03 gap: `.text` roughly **[0x82603030, 0x82607EE0)** (lower edge = EditSetlistPanel's
pinned end 0x82603030; upper edge = 0x82607EE0 = ManageBandPanel start). Oracle present at
`../rb3/src/band3/meta_band/MainHubPanel.cpp`, header `src/band3/meta_band/MainHubPanel.h` likely needs
checking. Same gameport recipe applies; bound the start precisely against EditSetlistPanel and the
intervening funclets before pinning. Larger TU (the MOTD/battles/override hub) → bigger expected delta.
