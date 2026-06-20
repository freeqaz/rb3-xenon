# W11 Discovery — MiniLeaderboardDisplay (ENGINE/bandobj)

**Date:** 2026-06-20 · **Mode:** DISCOVER/PLANNER (read-only in main @053d2d0, baseline 9159)
**TU:** MiniLeaderboardDisplay · **Prompt coords:** `.text [0x8262E974, 0x8262F530)`
**Verdict: REFUTED (sliver/misattribution).** The prompt's range is **not** the
MiniLeaderboardDisplay engine TU. It is the static-init / Symbol-intern / base-classname
COMDAT preamble of the **already-pinned AppMiniLeaderboardDisplay.cpp** object. The real
MiniLeaderboardDisplay base-class TU lives at **~0x82307508–0x823083C0** (unsplit blob),
~0xD9000 bytes away. Pinning the prompt range as MiniLeaderboardDisplay would fail the
honesty gate (foreign AppMini static-init, zero MiniLeaderboardDisplay method bodies).

A real, actionable lead is emitted for the true cluster (see discovered_frontier).

---

## What is actually in [0x8262E974, 0x8262F530)

32 functions, dominated by static-init machinery — verified via dtk blob asm
`build/45410914/asm/auto_03_82627200_text.s` + Ghidra MCP decompile:

| addr | size | identity |
|------|------|----------|
| 0x8262E974 … 0x8262EABC | 0x20 ea | `??__F` atexit / guard-clear thunks — each does `lwz; clrrwi/rlwinm (clear guard bit); stw` on global guard-mask `lbl_82DA0017+0x30211`. Pure static-teardown. |
| **0x8262EAF0** | **0x668** | `??__E` dynamic initializer: interns a **batch** of Symbols (`bl Symbol::Symbol` = fn_8279B788) and builds `DataArray`s (fn_82727520/82725FC0/…) for property names at `lbl_820CE3xx`/`lbl_820CCAEC`. These are AppMini's PROPSYNCS/HANDLERS symbols (leaderboard, title_label, icons_label, reset/fade triggers, pending_group — exactly what `AppMiniLeaderboardDisplay::Update()` / handlers reference). |
| 0x8262F158 … 0x8262F3A8 | 0x20–0x108 | further per-Symbol guard thunks + one 0x108 init body (0x8262F3A8 iterates `DAT_82dd0c58` band-list globals — AppMini registration scaffolding). |
| **0x8262F4B0** | 0x58 | `MiniLeaderboardDisplay` **classname accessor** — lazily interns string `MiniLeaderboardDisplay` (0x8202FFE8) into a static Symbol under guard `lbl_82DA0017+0x3024D`, returns it. Has an MSVC EH record (`except_record_8262F4B0`, magic 0x19930522). |
| 0x8262F508 | 0x28 | guard-clear thunk for 0x8262F4B0's static. |

### Why this is AppMini's object, not a separate TU
- **Caller proof:** the only callers of the classname accessor `fn_8262F4B0` are at
  **0x8262F6BC, 0x8262F98C, 0x82630890** — all *inside* the pinned
  `AppMiniLeaderboardDisplay.cpp` .text `[0x8262F530, 0x82630988)`
  (`grep fn_8262F4B0 build/.../band3/meta_band/AppMiniLeaderboardDisplay.s`). The
  accessor exists in this object because `AppMiniLeaderboardDisplay`'s
  `HANDLE_SUPERCLASS(MiniLeaderboardDisplay)` + RTTI need the **base** classname Symbol,
  which MSVC emits as a COMDAT into the derived class's compiled object.
- **Contiguity:** the range abuts AppMini's pinned .text exactly at 0x8262F530 (upper
  neighbour). It is the static-init head of the *same* object, placed just before the
  method bodies — the classic "extend the pinned TU downward" shape, not a new TU.
- **AppMini source** (`src/band3/meta_band/AppMiniLeaderboardDisplay.cpp`) `#include`s
  `Symbols.h/Symbols2/3/4.h` and registers exactly these property symbols.

## Where the REAL MiniLeaderboardDisplay engine TU is

Anchored by the **RB3-only** property string `allow_solo_scores` (0x82030230) and the
classname string (0x8202FFE8):

- `MiniLeaderboardDisplay::SyncProperty` is at **0x82307ea8** (0x108). Ghidra decompile
  shows it interning `allow_solo_scores` (guard `DAT_82c8c920`, str 0x82030230), comparing
  the prop name, reading/writing `mAllowSoloScores`, then calling vtable slot **+0x4c**
  (= `Update()` from `SYNC_PROP_MODIFY(allow_solo_scores, mAllowSoloScores, Update())`).
  This is **exactly** the rb3-Wii `BEGIN_PROPSYNCS(MiniLeaderboardDisplay)`.
- ctor body at **0x82307b30** installs vtable `PTR_Function_82804698` at `this+0x174`
  (UIComponent vtable region), base-ctor at `this+0x148`.
- classname `MiniLeaderboardDisplay` (0x8202FFE8) referenced at **0x8230753c** and
  **0x8230801c**.
- Cluster spans **~0x82307508 → 0x823083C0+** (UNSPLIT; bounded below by
  `FilterQueue.cpp` end 0x82303594, above by `HamListRibbon.cpp` start 0x82308ea0).
  NOTE: two classnames in the region (0x8230753c and 0x8230801c) plus a 2nd Symbol guard
  `DAT_82c8c928`/str 0x82030290 at 0x82308050 imply the real
  `MiniLeaderboardDisplay` TU is **adjacent to a sibling TU** in this blob — boundary
  derivation must split them precisely (do NOT pin the whole 0x82307500–0x82308400 span
  blind).

## Oracle situation (correction to the prompt + CLAUDE.md)
- The prompt says "body-port from DC3 ../dc3-decomp/src/system/bandobj" — **wrong on two
  counts**: (1) `../dc3-decomp/src/system/bandobj/` does not exist; DC3's file is
  `../dc3-decomp/src/system/hamobj/MiniLeaderboardDisplay.cpp`; (2) DC3's version is a
  **different class** — it has `mResourceDir` (ResourceDirPtr), an `OldResourcePreload`
  virtual, `SAVE_REVS(0,0)`, an `Update()` body, and **no `mAllowSoloScores`**. Retail RB3
  implements the **rb3-Wii** layout (`mAllowSoloScores` bool @0x10c, `SAVE_OBJ(...,54)`,
  `ASSERT_REVS(1,0)`, `DrawShowing` via `mResource->Dir()`). The retail binary carries
  both `allow_solo_scores` (0x82030230) and the classname string — confirming RB3-Wii is
  the correct oracle here, not DC3.
- The existing header `src/system/bandobj/MiniLeaderboardDisplay.h` (created by the wave-10
  AppMini land) **already documents** this RB3-vs-DC3 divergence — it is correct as-is.
- Correct oracle: `../rb3/src/system/bandobj/MiniLeaderboardDisplay.{cpp,h}` (full source,
  ~10 small methods: ctor/dtor, Copy, Save(SAVE_OBJ 54), Load, PreLoad/ASSERT_REVS(1,0),
  PostLoad, DrawShowing, Handle, SyncProperty, Init).

## Pin-safety / overlap self-check
- Prompt range `[0x8262E974, 0x8262F530)`: **zero** overlap with any splits.txt `.text`
  range; abuts AppMini at 0x8262F530 (upper), gap above NameGenerator (ends 0x82627200).
  pdata slot would auto-derive into the gap `[0x82223a50, 0x82224548)`. Mechanically
  pinnable — but semantically WRONG (sliver), so do not.
- Real cluster ~0x82307508–0x823083C0: UNSPLIT, bounded by FilterQueue.cpp (0x82303594)
  and HamListRibbon.cpp (0x82308ea0). Needs sibling-TU split derivation before pinning.

## Recommendation
**DEFER** the prompt TU (refuted as a sliver). Re-target a future wave at the real
cluster ~0x82307508 (see discovered_frontier) — that is the genuine, RB3-Wii-portable
MiniLeaderboardDisplay engine TU, but it is NOT independently landable this wave: its
.text boundary is interleaved with a sibling TU in the auto blob and requires a careful
two-TU split derivation + a target_symbol_map entry generation pass first.

## Method index (for re-derivation)
- blob asm (prompt range): `build/45410914/asm/auto_03_82627200_text.s`
- blob asm (real cluster): `build/45410914/asm/auto_03_82305528_text.s`
- AppMini pinned asm: `build/45410914/asm/band3/meta_band/AppMiniLeaderboardDisplay.s`
- classname str `MiniLeaderboardDisplay` @0x8202FFE8; RTTI `.?AVMiniLeaderboardDisplay@@`
  @0x82c3cd14; prop str `allow_solo_scores` @0x82030230.
