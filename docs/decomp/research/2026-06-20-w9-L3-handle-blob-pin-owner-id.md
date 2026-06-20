# W9 L3 — handle-blob-pin-owner-id (ADVERSARIAL DISCOVER/PLANNER)

**Date:** 2026-06-20  **Baseline:** main @812e1df (8314 matched)
**Mode:** read-only in main; COFF/Ghidra/oracle analysis only.
**Verdict: REAL_ACTIONABLE** (with corrections to the frontier's specific anchors).

## TL;DR

The vein is REAL: **658 unique unpinned functions** binary-wide carry the
`END_HANDLERS` PathName tail (`bl fn_82732F68` → `li r11,6` set-`kDataUnhandled`)
and live in UNPINNED `auto_03_*` blobs. These are pin-gated: the macro fix alone
cannot match them until their owning TU is pinned + wired.

**Two frontier corrections:**
1. The frontier's named anchors `auto_03_82272EB4_text` / `auto_03_8227EF2C_text`
   have **ZERO unpinned Handle-tail owners within ±0x4000** — they are stale/wrong
   labels. Do not chase them.
2. The mechanism is **sliver-pin owner-ID + wire**, identical to the wave-3
   "sliver/over/displaced pin" dominant vein, NOT a fresh blob discovery. Several
   target owner TUs already have a **~0x58–0x160 ICF-displaced sliver pin** in
   `splits.txt` but are **NOT in `objects.json`** (uncompiled), while their REAL
   multi-Handle cluster sits 20–25 MB away in the auto-blob.

## HARD PREREQUISITE (gating dependency, NOT on main)

Every actionable item below is **blocked** until the global BEGIN_HANDLERS
MessageTimer drop lands. On main @812e1df, `src/system/obj/Object.h:928-937`
still emits `MessageTimer timer(...)` **unconditionally**, and `END_HANDLERS`
(line 1032) wraps `PathName(this)` inside `MILO_NOTIFY` (sizeof-stripped at
retail-release → PathName dropped). The fix already exists UNMERGED:
- branch `w9-land-global-begin-handlers-timer-drop`
- commit **`b259212`** "obj: gate per-handler MessageTimer behind
  MILO_MESSAGE_TIMERS (retail off) (+130 @100%)" — the **cleaner gated form**, prefer this.
- commit `e57d204`/`66be8b2` "BEGIN_HANDLERS: drop debug-only MessageTimer +
  UIComponent resource handlers (+139)".

Plus the per-TU PathName tail re-add (Strategy A from
`2026-06-20-w9-L2-handle-check-pathname-systemic.md`): wrap the TU's
`BEGIN_HANDLERS…END_HANDLERS` in `#pragma push_macro("MILO_NOTIFY") / #undef /
#define MILO_NOTIFY(...) (void)(__VA_ARGS__) / … / pop_macro`.

A **self-contained** work-item must therefore do ALL of: rebase-on/include the
macro prereq (b259212) + wire source in objects.json + pin `.text` + port from
oracle + per-TU PathName wrapper + map entries + whole-binary A/B, in ONE worktree.

## Ground-truth method (reproducible)

- COFF symbol-by-VA from `auto_03_82260000_text.obj` (74,500 external `fn_<VA>`
  text syms; record = name[8] value[4] secnum[2] type[2] sclass[1] naux[1]; VA =
  0x82260000 + value). Script `/tmp/coffsym2.py`.
- END_HANDLERS scan: PPC `bl` (op 18, LK=1, AA=0) to 0x82732F68 followed by
  `li r11,6` (0x39600006). Script `/tmp/handle_scan.py` → 1335 PathName calls,
  410 with the strict `li r11,6`. Owner = `bisect` into sorted fn-VA table.
- Pin status: parse `.text start:/end:` ranges in `config/45410914/splits.txt`.
  Result: **535 callsites in PINNED TUs (398 fns), 800 in UNPINNED blobs (658 fns)**.
- Owner class hint: nearest `scripts/target_symbol_map.json` mangled entries
  (`/tmp/owner_id.py`); confirm via Ghidra MCP @8002 decompile + rdata string
  resolve (`/tmp/rdstr.py`, VA→`auto_00_82000400_rdata.obj` section offset).

## OWNER-ID RESULTS (ground-truth confirmed)

### A. Sequence.cpp — ENGINE, DC3 Matching, sliver-pinned (HIGHEST CONFIDENCE)

- **Cluster:** ~`0x826E8048 … ~0x826EDA00` (≈140 fns; 8 Handle-tails inside).
- **Current pin:** `Sequence.cpp .text 0x82532110-0x82532168` (size **0x58**) — an
  ICF-displaced sliver that decompiles to a `FxSend` string ref (mis-attributed).
  **NOT in objects.json** (uncompiled).
- **Owner proof (strings via Ghidra):** `volume/pan/transpose` (Sequence base
  props), `play/stop/add_fader` (Sequence handlers), `avg_volume/volume_spread/
  avg_transpose` (RandomIntervalGroupSeq props), `num_simul/allow_repeats/
  force_choose_index/force_next_play_index/get_next_play_index` (RandomGroupSeq).
  fn_826EB598 (a Handle body) interns Symbol("types")/("objects") then
  `bl fn_82732f68` + vtable[0x3c] — the canonical END_HANDLERS shape.
- **Oracle (DECISIVE):** `dc3-decomp/src/system/synth/Sequence.cpp` =
  **"Matching"** in DC3 `objects.json`; DC3 splits give it a large `.pdata`
  0x822E2E30-0x822E3588 (0x758 = ~90 unwind entries). Same engine, same flags →
  byte-for-byte Rosetta. Source `src/system/synth/Sequence.cpp` ALREADY in our tree.
- **Classes:** Sequence, RandomGroupSeq, RandomIntervalGroupSeq (+ *Inst). The
  `FaderGroup (size %d)` string at ~0x826EDEC0 marks the **Faders.cpp boundary**
  (separate TU below) — Sequence upper bound is just before it.

### B. Faders.cpp — ENGINE, DC3 has it, sliver-pinned (adjacent to Sequence)

- **Cluster:** ~`0x826EDA00 … ~0x826F16E0` (next pin MicClientMapper @0x826F16E0;
  6 Handle-tails inside). Anchor string `FaderGroup (size %d)` @0x826EDEC0.
- **Current pin:** `Faders.cpp .text 0x826C1730-0x826C1794` (size **0x64**) sliver,
  NOT in objects.json.
- **Oracle:** `dc3-decomp/src/system/synth/Faders.cpp` exists; `src/system/synth/
  Faders.cpp` in our tree. (FaderGroup, Faders.) Engine → DC3 oracle.
- **Note:** A/B Sequence FIRST (it is the cleaner span). Faders is the upper
  neighbour; pin only after Sequence's upper bound is nailed so the two ranges abut.

### C. CharWeightSetter.cpp / CharBoneTwist.cpp — ENGINE char, sliver-pinned

- Window `0x823A62F0..0x823A7040` (4 Handle-tails). CharWeightSetter pinned
  `.text 0x823A63D8-0x823A6538` (size 0x160), CharBoneTwist pinned
  `0x8269FBC8-0x8269FC3C` (size 0x74) — both slivers; the Handle bodies fall
  AROUND/just-below the pins. Both wired in objects.json+splits already, so this is
  an EXTEND/RE-PIN (not a fresh wire). Lower confidence: Char setter classes are
  string-poor (could not string-confirm every fn). Oracle: rb3-Wii
  `src/system/char/CharWeightSetter.cpp` (engine; cross-check DC3). DEFER to a
  recon-first item — verify the cluster is contiguous own-TU before extending.

### D. Game panels (band3/meta_band) in the 0x825FC080..0x82628658 mega-cluster

The single largest unpinned Handle cluster (67 Handle-tails over 0x2c5d8) is a
**multi-TU game neighbourhood**, NOT one TU. Confirmed owner strings:
- `set_voiceover_symbol/play_voiceover` → **VoiceoverPanel.cpp** (rb3-Wii game).
- `is_waiting_on_enum/marquee_path` → **StoreMainPanel.cpp** (rb3-Wii game).
- bulk `types/objects` = generic Object Handle tails (TU-internal anon).

This needs per-TU string-anchor boundary derivation (like the L2
ViewSetting/AppLabel cluster) → emitted as discovered_frontier, not a clean
single work-item.

## Honesty / attribution cautions

- **attribution_risk=TRUE on every pin/relocation here.** Spans must be bounded by
  per-function ground-truth (string anchors + Handle-tail anchors), NOT oracle fn
  count (oracle over-counts vs retail COMDAT interleave). Honesty gate per blob:
  net≥+1, no ≥8-contiguous FOREIGN fn_@0% run, gains==intended.
- Pin **conservative core first, extend to next pin** (Sequence: stop before the
  FaderGroup string; Faders: stop before MicClientMapper @0x826F16E0). Let dtk
  back-fill `.pdata`; never gap-shrink.
- The sliver evictions (Sequence 0x58 @0x82532110, Faders 0x64 @0x826C1730) are
  dead `mf=0` mis-pins — when this-unit-IS-the-sliver, re-point the TU's `.text`
  to its real cluster (the wave-3 `requires_sliver_eviction` pattern). The 0x82532110
  sliver's FxSend string means it's ICF-folded FxSend code; dropping it loses
  nothing (it reads 0% now).

## Why est should be DISCOUNTED from the frontier's +40

The frontier's +40 conflates the 658-fn binary-wide Handle mass with one blob.
Realistic per-item: Sequence ≈ +8–15 (8 Handle bodies + accessors + ctors/dtors;
some Save/Load/Inst funclets are permuter/saverev-class), Faders ≈ +6–10. The
mega-cluster game panels are separate multi-TU ports (VoiceoverPanel, StoreMainPanel).
