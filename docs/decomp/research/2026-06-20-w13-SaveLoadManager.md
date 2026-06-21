# W13 DISCOVER — SaveLoadManager (band3/meta_band)

**Date:** 2026-06-20  **Mode:** DISCOVER/PLANNER (read-only main @ c00664c, baseline 9404)
**TU:** `band3/meta_band/SaveLoadManager.cpp`
**Verdict:** IDENTIFIED + LOCATED + BOUNDED, but **DEFER** — this is a ~55 KB / 519-function
*major TU*, not a clean self-contained ~+12 lane. Heavy unwired dependency graph + a real
Wii→Xbox platform-layer argument-order divergence + virtual-base MI. Phased plan below; the
first landable slice is real but must be gated behind dependency wiring.

---

## 1. Identification (ground-truth, not a guess)

The TU is unambiguously present in the retail XEX, in the MusicLibrary→PrefabMgr gap.

Anchors (all from `auto_03_82260000_text.obj` COFF relocations + Ghidra decompile, port 8002):

- **Constructor** `fn_8253EAF8` — references the C-string `saveload_mgr` (`SetName("saveload_mgr",…)`),
  installs vtable via the **virtual-base adjust** `*(int*)(*this+4)` pattern. This matches
  rb3-Wii `class SaveLoadManager : public MsgSource`, and `MsgSource : public virtual Hmx::Object`
  (`../rb3/src/system/obj/Msg.h:195`) → SaveLoadManager has a **virtual base**, exactly as the
  retail ctor shows.
- **`SetState`** `fn_8253D198` (4096 bytes) — Ghidra reports a **~106-case switch** at `0x8253D310`;
  matches the Wii `SetState(State)` enum (states up to `kS_Finish=0x6F`). References the Symbol
  literal `global_options_cache_name`.
- **`Poll`** `fn_8253FDA8` (2628 bytes) — a **second ~106-case switch** at `0x8253FE88`; the Wii
  `Poll()` state machine.
- **`HandleEventResponse`-class** `fn_825395B8` (6592 bytes) — **~97-case switch**, called by the
  dialog handler `fn_8253EF78` and self-recursive.
- **Dialog accessors** `fn_8253EF78` (2576 B, 15 saveload-Symbol refs) and `fn_825338E8`
  (1268 B, 7 refs) = `GetDialogMsg` / `GetDialogOpt1/2/3`.
- **Init** `fn_82531F80` writes the `TheSaveLoadMgr` global (`lbl_82DCBF70`), allocating `0x1A8` (424 B).

**Oracle = rb3-Wii (NOT DC3).** Decisive: retail's global-options cache name is
`globaloptions` (present in `auto_00…rdata.obj` @ off 557936 and Ghidra string @ `0x82094550`
region). rb3-Wii uses `"globaloptions"`; **DC3 uses `"global"`** and `class SaveLoadManager :
public Hmx::Object` (no MsgSource, no virtual base) — DC3 is the false friend here, confirming
the CLAUDE.md rule that game code follows rb3-Wii. Source: `../rb3/src/band3/meta_band/SaveLoadManager.cpp`
(2266 lines, 39 named methods) vs `../dc3-decomp/src/lazer/meta_ham/SaveLoadManager.cpp`.

The header is already in-tree (Wii-flavored, `// 0xNN` annotated):
`src/band3/meta_band/SaveLoadManager.h`.

---

## 2. Bounds (vs BOTH neighbours)

Gap between pinned neighbours:
- `MusicLibrary.cpp .text` ends **`0x8252E608`** (splits.txt:47)
- `band3/meta_band/PrefabMgr.cpp .text` starts **`0x82540840`** (splits.txt:2469)

The gap `[0x8252E608, 0x82540840)` (~73 KB) holds **TWO** TUs:

- `[0x8252E6B0, 0x82532068)` — a *different* class: vtable `0x8209002C`, ctor `fn_82531B48`,
  an "Init" `fn_82531F80` that also writes `TheSaveLoadMgr`. Likely a saveload **factory /
  status-panel** sibling (NOT SaveLoadManager proper — its ctor does not reference `saveload_mgr`).
  **Out of scope for this lane.**
- **`[0x82532068, 0x82540814)` = SaveLoadManager** — proven by SetState/Poll calling helpers as
  low as `fn_82532188`/`fn_82532198`/`fn_825321B8` and the dialog/Symbol-accessor block.

**Proposed SaveLoadManager `.text` span (if pinned as one TU):**
```
band3/meta_band/SaveLoadManager.cpp:
    .text       start:0x82532068 end:0x82540814
```
- Lower bound `0x82532068` is a guard-clear COMDAT thunk (natural TU start), strictly **>**
  MusicLibrary end `0x8252E608` ✔, and **>** the sibling cluster that ends at `~0x82532068` ✔.
- Upper bound `0x82540814` is the last in-span function start (`fn_82540814`, sz 40); its end
  `0x8253C…`→`0x8254083C` is strictly **<** PrefabMgr start `0x82540840` ✔.
- **Splits overlap self-check:** no existing pin covers `0x82532000–0x82540840` (grep clean).
- `.pdata` is auto-derived by dtk on first split; do NOT hand-pin it.

Scale: **519 functions, 56,784 bytes (~55.5 KB)**. 27 are ≥200 B (real method bodies);
**404 are ≤44 B** COMDAT Symbol-literal accessors + `Message::Type()` accessors + `vector<BandProfile*>`
helpers + `??_E`/`??__F` — legitimate emissions of a dialog-state-heavy TU. Biggest:
`0x82535A48` (8008), `0x825395B8` (6592, OnMsg/HandleEventResponse), `0x8253D198` (SetState, 4096),
`0x8253FDA8` (Poll, 2628), `0x8253EF78` (dialog, 2576).

---

## 3. Why DEFER (not a one-worktree +12)

1. **Scale.** ~55 KB / 39 named methods incl. FOUR 100±-case state machines. A full match is
   realistically +30..+50 functions but is a multi-session grind, not a single lane. The brief's
   "~+12" under-scoped this TU by ~4×.

2. **Platform-layer argument divergence (hard blocker for the core).** The Wii source calls e.g.
   `TheMemcardMgr.SelectDevice(pProfile, false, this, devId)` (Wii arg order), but the in-tree
   Xbox interface `src/system/meta/MemcardMgr.h:55` declares
   `SelectDevice(Profile*, Hmx::Object*, int, bool)` — **different parameter order/types**. The
   engine wires `MemcardMgr.cpp` + **`MemcardMgr_Xbox.cpp`** (objects.json:264-265), NOT `_Wii`.
   So the Wii `Poll`/`SetState` bodies will not compile against the retail platform layer without
   per-call adaptation. The retail RB3-Xbox state machine is structurally the Wii one (state
   values match) but its MemcardMgr/WiiProfileMgr call sites are the Xbox convention.

3. **Unwired / NonMatching dependency graph.** SaveLoadManager `#include`s and calls into:
   `ProfileMgr` (TheProfileMgr.* — many calls; **not in objects.json**), `BandProfile`
   (**not wired**), `BandSongMgr` (wired NonMatching), `MemcardMgr`/`MemcardMgr_Xbox`
   (wired NonMatching), `EntityUploader` (header in-tree, **.cpp not wired**),
   `WiiProfileMgr`/`MemcardMgr_Wii` (header `MemcardMgr_Wii.h` **absent** from src),
   `FixedSizeSaveableStream`, `CacheMgr`. To *compile* the TU you must satisfy all these symbols;
   to *match* it you depend on their layouts being correct first. This is the ProfileMgr-depends-on-
   SaveLoadManager coupling the brief noted — but the dependency points the other way for the
   build: SaveLoadManager needs ProfileMgr's interface/layout pinned to match its many `TheProfileMgr.*`
   calls.

4. **Virtual-base MI.** Ctor + dtor go through the MsgSource virtual-base vtable. Matchable (the
   project has working vbase support, cf. ObjectDir-vbase work in MEMORY) but adds per-funclet risk.

The HONESTY GATE (no ≥8-contiguous foreign fn_@0% run; headline net == intended) cannot be met by
a naive "pin the whole 55 KB span + drop in Wii source": the source won't compile against the Xbox
MemcardMgr, and a pin without a matching obj reads 0% across 519 functions (a huge dishonest-0
surface), or worse drags fuzzy.

---

## 4. Phased plan (for a future multi-lane campaign, NOT this single worktree)

Each phase is independently landable vs its own fresh baseline; do them in order.

1. **Keystone (separate lane): pin + port ProfileMgr.cpp.** SaveLoadManager's match ceiling is
   gated by `TheProfileMgr.*` call resolution and layout. Locate ProfileMgr's TU (it's elsewhere
   in `.text`; `TheProfileMgr` global xrefs), wire `band3/meta_band/ProfileMgr.cpp` NonMatching,
   pin, port from rb3-Wii. **Flag as the prerequisite.**

2. **Resolve the MemcardMgr Wii↔Xbox call convention.** Decide per-call-site mapping from Wii
   `MemcardMgr` calls to the in-tree Xbox `MemcardMgr.h` interface (use DC3
   `../dc3-decomp/src/lazer/meta_ham/SaveLoadManager.cpp` ONLY as the *Xbox-platform* reference
   for which MemcardMgr entry points retail calls — NOT for the RB3 logic, which is Wii's). Produce
   an adaptation table (Wii arg order → Xbox arg order) as a shared note.

3. **Wire + scaffold SaveLoadManager.cpp** (objects.json NonMatching) with the in-tree header.
   Port the **platform-independent leaf helpers first** — the ones with NO MemcardMgr/ProfileMgr
   deps: `IsIdle`, `IsInitialLoadDone`, `GetDialogFocusOption`, `Activate`,
   `HandleEventResponseStart`, `UpdateStatus` (`fn_82532188` is the trivial +0x34 setter),
   `GetDialogOpt1/2/3` / `GetDialogMsg` (Symbol switches — pure, just need the dialog Symbol
   literals), `GetNewSigninProfile`/`GetAutosavableProfile` (only need ProfileMgr accessors from
   phase 1). Pin a **tight sub-range** covering only the ported, byte-matching functions; extend
   the pin as more functions land (the split-cluster EXTENSION vein — never shrink-bound).

4. **Port the state machines** (`Poll`, `SetState`, `OnMsg`/`HandleEventResponse`) only after
   phases 1-2 land, since they are dominated by MemcardMgr/ProfileMgr/EntityUploader calls.

5. **Genmap + reveal.** Generate target_symbol_map entries via `tools/gen_game_target_map.py`
   from the rb3-Wii oracle for the SaveLoadManager methods (VA-keyed; ADD-only, never regenerate
   wholesale). Reveal-sweep any fn that is already byte-exact once the pin lands.

A/B every phase: `rm -f build/45410914/target_symbol_renames.stamp && touch
config/45410914/config.yml && NINJA_JOBS=8 tools/fresh_report.sh`; re-run for the splits-only FP.

---

## 5. Evidence index

- splits neighbours: `config/45410914/splits.txt:47` (MusicLibrary), `:2469` (PrefabMgr).
- vtable/method anchors: `auto_03_82260000_text.obj` relocs (ctor `0x8253EAF8`→`saveload_mgr`;
  SetState `0x8253D198`→`global_options_cache_name`; Init `0x82531F80`→`TheSaveLoadMgr` `0x82DCBF70`).
- Ghidra (port 8002): `0x8253D198` ~106-case, `0x8253FDA8` ~106-case, `0x825395B8` ~97-case.
- oracle: `../rb3/src/band3/meta_band/SaveLoadManager.cpp` (`"globaloptions"`, MsgSource vbase);
  divergent twin `../dc3-decomp/src/lazer/meta_ham/SaveLoadManager.cpp` (`"global"`, Hmx::Object).
- platform divergence: `src/system/meta/MemcardMgr.h:55` (Xbox SelectDevice sig) vs Wii call sites;
  engine wires `MemcardMgr_Xbox.cpp` (objects.json:264-265), `MemcardMgr_Wii.h` absent from src.
- header present: `src/band3/meta_band/SaveLoadManager.h`.
