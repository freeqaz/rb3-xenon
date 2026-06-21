# W13 — GAP A bisection + first-TU port (CriticalUserListener ↔ OvershellSlot)

**Date:** 2026-06-20  **Mode:** DISCOVER/PLANNER (read-only main @ c00664c, 9404 matched)
**TU/lane:** `gapA-bisect-port`
**Gap:** `[0x825BDF28, 0x825C10D8)` — 110 functions / 0x31B0 bytes, fully accounted for.
Neighbours: `CriticalUserListener.cpp` `.text` ends `0x825BDF28`; `OvershellSlot.cpp` `.text` starts `0x825C10D8` (both pinned & wired).

## Method

- Parsed `auto_03_82260000_text.obj` (big-endian XCOFF, magic 0x01F2, single `.text`
  section, base 0x82260000, 65170 `fn_` symbols). Symbol value = offset within
  `.text`; VA = 0x82260000 + val. Sizes = next-fn-start − this-start.
- Relocations: the section sets `IMAGE_SCN_LNK_NRELOC_OVFL` (0x01000000); the real
  reloc count (411183) lives in the first reloc record's `rva`, payload starts at
  `relptr+10`. (The 16-bit `nrel` field is a saturated 65535 — do NOT use it.)
- String fingerprints: gap functions reference rdata via `lbl_<VA>` relocations.
  Resolved each `lbl_` VA to raw bytes in `auto_00_82000400_rdata.obj`
  (base 0x82000400) / `auto_06_82C34400_data.obj` (base 0x82C34400) and ASCII-decoded.
- Owner ID: grepped each unique string against `../rb3/src` (rb3-Wii oracle).
- Boundary proof: masked-content match of every gap fn against our compiled
  `build/45410914/src/band3/meta_band/OvershellSlot.obj`.

## Bisection map (gap fully partitioned into 2 sub-TUs)

| sub-TU | span | #fns | bytes | owner | oracle | status |
|---|---|---|---|---|---|---|
| **CharData.cpp** | `[0x825BDF28, 0x825BEBD8)` | 36 | 0xCB0 | `band3/meta_band/CharData.cpp` | `../rb3/.../CharData.cpp` | **UNWIRED → PORT THIS** |
| OvershellSlot.cpp head | `[0x825BEBD8, 0x825C10D8)` | 74 | 0x2500 | `band3/meta_band/OvershellSlot.cpp` | `../rb3/.../OvershellSlot.cpp` | WIRED, under-pinned (extension lever) |

### Evidence for the boundary `0x825BEBD8`
- Strings in `[0x825BDF28, 0x825BEBD8)`: only `prefab_mgr`, `prefab_portrait_path_prefix`,
  `prefab_portrait_path_suffix` (at fn_825BE7A8) — these three live **ONLY** in
  `CharData.cpp` (`GetPrefabPortraitPath`, line 22-34). Plus a virtual-inheritance EH/vbase
  cluster (vtable_820B206C carries the MSVC `19930522` `_s_FuncInfo` magic — funclet table,
  not a class vtable) at 0x825BE120/1C4/208/250/618 + thunks, consistent with
  `class CharData : public virtual Hmx::Object`.
- First string at `0x825BEBD8` = `msg_duration`,`state_handlers` → used **only** in
  `OvershellSlot::SetTypeDef` (oracle line 123-128). So `0x825BEBD8` = SetTypeDef = first
  OvershellSlot fn. Everything `[0x825BEBD8, 0x825C10D8)` is OvershellSlot (msg_duration,
  mod_auto_vocals, forced_part, overshell_*_cym, auto_vocals_confirm, pause_menu_quit_token,
  kick_user, difficulty, in_track_mode, slot_view — all in OvershellSlot.cpp).
- Exactness: fn_825BEBB0 (size 0x28) ends at 0x825BEBB0+0x28 = **0x825BEBD8** — clean fn split.
- Content-match: of the 74 OvershellSlot-head fns, `?IsQuitToken@OvershellSlot@@QBA_NVSymbol@@@Z`
  @0x825BFB08 already byte-matches our compiled OvershellSlot.obj exactly (proves OvershellSlot
  ownership of the head; rest are unported/divergent — see Frontier).
- None of the 36 CharData-TU fns matched the compiled OvershellSlot.obj → not OvershellSlot.

## ACTIONABLE: wire + pin + port CharData.cpp `[0x825BDF28, 0x825BEBD8)`

CharData.cpp is a clean, self-contained 98-line TU (`../rb3/src/band3/meta_band/CharData.cpp`,
14 named methods + virtual-inheritance thunks/funclets = 36 emitted fns). Header
`src/band3/meta_band/CharData.h` is ALREADY present in rb3-xenon with full member offsets
(mBandCharDesc 0x18, mTexPortrait 0x1c, mLoader 0x20, unk24 0x24). All compile deps resolve
in rb3-xenon: PrefabMgr.h, BandCharDesc.h (system/bandobj), Loader.h, Locale.h, MakeString.h,
System.h, Symbols4.h. No existing pin/map collision (the 45 existing CharData/PrefabChar map
entries are `WorldCrowd::CharData::Char3D` + PrefabMgr STL helpers at 0x824C/0x8254 — a
different region, verified).

### Self-contained worktree steps
1. `scripts/setup_worktree.sh` a buildable worktree.
2. Copy `../rb3/src/band3/meta_band/CharData.cpp` → `src/band3/meta_band/CharData.cpp`.
   Port MWCC→MSVC X360 (Wii oracle): keep MILO_ASSERT line numbers (0x29/0x3F/0x5A/0x88/0x9A),
   `#pragma push/force_active on/pop` around inline GetPrefabName, RELEASE macro,
   `Hmx::Object::New<RndTex>()`, `TheLoadMgr.AddLoader(...,kLoadBack)`. Debug.h gates MILO_WARN.
3. `config/45410914/objects.json`: add `"band3/meta_band/CharData.cpp": "NonMatching"`.
4. `config/45410914/splits.txt`, add (bounded vs BOTH neighbours — see Pin safety):
   ```
   CharData.cpp:
       .text       start:0x825BDF28 end:0x825BEBD8
   ```
   (dtk auto-derives the `.pdata` range; room exists in [0x8221CA80, 0x8221CD90) between the
   two neighbours' pdata.)
5. `python3 tools/gen_game_target_map.py --tu CharData.cpp --apply` (ADD-only; generates the
   fn_→mangled map entries from the rb3-Wii oracle so objdiff pairs by name — without these a
   pinned game TU reads a false 0%). Never regenerate the whole map.
6. `python3 configure.py` then VERIFY:
   `rm -f build/45410914/target_symbol_renames.stamp && touch config/45410914/config.yml &&
    NINJA_JOBS=8 tools/fresh_report.sh` → read `measures.matched_functions`; **re-run** (splits-only
    divergence warning is a known FP).
7. reveal_sweep / batch-check the new unit; report only TRUE-100% per honesty gate.

### Expected delta
The 36 fns include many trivial byte-matchers: accessors (GetBandCharDesc/GetPortraitTex/
GetCharacterName/IsFinalized/IsPortraitLoaded/GetTexAtPatchIndex), simple bodies
(CachePortraitTex/UnloadPortrait/LoadPortrait/PollLoadingPortrait), the Handle handler, and
the vbase/`??_E`/`??_G`/thunk boilerplate that the renamer pairs. Conservative **+12**;
upside higher if the virtual-inheritance thunk cluster + GetPrefabPortraitPath pair cleanly.
(Tail: the OvershellSlot-head extension is a separate frontier item.)

## Pin safety
- Lower bound `0x825BDF28` == CriticalUserListener.cpp `.text end` (exact abut, no overlap).
- Upper bound `0x825BEBD8` == OvershellSlot::SetTypeDef start (proven), < OvershellSlot pin
  start `0x825C10D8` (no overlap).
- splits.txt overlap self-check: zero overlapping `.text` ranges in the span (verified).
- `.pdata`: dtk auto-derives; gap [0x8221CA80, 0x8221CD90) between neighbours' pdata is free.

## Independence / flags
Fully INDEPENDENT vs main @ c00664c: new file + new objects.json line + new splits.txt block +
ADD-only map entries. No shared-header / binary-wide edit. `flag_foundational=false`.

## Frontier (the rest of the bisection — emit as discovered_frontier)
- **OvershellSlot-head extension** `[0x825BEBD8, 0x825C10D8)` (74 fns): OvershellSlot.cpp is
  WIRED+pinned to its tail `[0x825C10D8, 0x825C3A44)` (currently 13/19 fns @100%). The head is
  the under-pinned sliver. `IsQuitToken`@0x825BFB08 already byte-matches → at least +1 free;
  more after porting the head's divergent fns (struct-layout / SAVE_REVS). Extend the existing
  OvershellSlot pin DOWN to 0x825BEBD8 (CharData must be wired FIRST so the two pins abut, or
  the head is squatting in CharData's auto-blob). Net depends on OvershellSlot port depth —
  body-port lane, expected +6..+20.
