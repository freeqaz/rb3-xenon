# Wave-12 MainHubPanel.cpp — port-then-pin DISCOVER (DEFER, no contiguous TU)

**Date:** 2026-06-20
**Mode:** DISCOVER/PLANNER (Opus), read-only on main @ `d2d3e53` (baseline 9301 matched).
**TU:** `band3/meta_band/MainHubPanel.cpp` — the OvershellPanel-derived "main hub"
online/matchmaking panel. Oracle: rb3-Wii `../rb3/src/band3/meta_band/MainHubPanel.{h,cpp}`
(authoritative game oracle; ~590 source lines, ~45 emitted functions incl. namespace
`MainHubAdvanceMsg`, ~14 `OnMsg` handlers, the `BEGIN_HANDLERS` dispatch, accessors).

## VERDICT: **DEFER — MainHubPanel has NO contiguous `.text` TU in retail RB3-360.**

The task premise ("~589 bytes, compact contiguous gap in the meta_band belt") does not hold.
MainHubPanel is one of the LARGEST meta_band panels (the `589` is **source lines**, not
bytes). More importantly, its 45 functions are **fully scattered across a ~9 MB span**
(`0x822CE6D0` … `0x82BC38F8`) of `.text`, not packed into one TU. There is no span to pin.
**This is a COMDAT/ICF-scatter wall, the exact class the Waypoint-audit + MEMORY.md flag.**
Honest deferral with ground-truth evidence (per the mode's "deferral with evidence is valid").

---

## 1. In-tree wiring state (partial — headers already landed)

- `src/band3/meta_band/MainHubPanel.h` — **present, byte-identical to the Wii oracle**
  (`diff` → IDENTICAL).
- `src/band3/meta_band/MainHubMessageProvider.h` — present, identical to Wii oracle.
- `MainHubPanel.cpp` — **NOT in tree**; `MainHubMessageProvider.cpp` — NOT in tree (Wii has it).
- `config/45410914/objects.json` — **no** MainHubPanel entry.
- `config/45410914/splits.txt` — **no** MainHubPanel pin.
- `scripts/target_symbol_map.json` — **no** MainHub entries.
- `build/45410914/report.json` — **0** named MainHub functions (unwired, as expected).

So the source-side scaffolding is half-done (headers), but there is nothing to pin against.

## 2. The scatter — ground truth from the cross-binary bindiff oracle

`unified_id_rb3wii.json` carries **45** bindiff hits whose `bindiff_src` is
`band3/src/meta_band/MainHubPanel.cpp`. Mapping each `rb3_addr` against the pinned `.text`
ranges in `splits.txt`:

- **41 of 45 fall in UNPINNED gaps**, spread from `0x822CE6D0` to `0x82BC38F8`.
- **4 fall INSIDE other already-pinned TUs**: `0x824EA05C` → RockCentral.cpp,
  `0x8252CD38` → MusicLibrary.cpp, `0x825DA908` → PhysicsManager.cpp,
  `0x8272BF2C` → Dir.cpp. (= ICF address-aliases sitting in foreign owners.)

Representative spread (Wii-named, rb3 VA):

| rb3 VA | Wii method | region |
|---|---|---|
| 0x823D1600 | `MainHubPanel::MainHubPanel()` (ctor) | far below the belt |
| 0x82605638 | `OnMsg(RockCentralOpCompleteMsg)` (autoid seed) | meta_band gap |
| 0x82627F58 | `MainHubPanel::Handle(DataArray*,bool)` | meta_band gap edge |
| 0x82642EB0 | `CheckStartWaitingLock()` | meta_band gap edge |
| 0x82808180 | `AppLabel::SetMotd(MainHubPanel*)` | +2 MB |
| 0x82913298 | `OnMsg(RockCentralOpCompleteMsg)` (dup VA) | +3 MB |
| 0x82971618 | `RefreshData()` | +3.6 MB |
| 0x829B8A00 | `Exit()` | +3.9 MB |
| 0x82A65F48 | `Poll()` | +4.6 MB |
| 0x82A670B8 | `ReloadMessages(LocalBandUser*)` | +4.6 MB |
| 0x82B8FCC0 | `Enter()` | +5.9 MB |
| 0x82BC38F8 | `PartyMicCallback::OnDisconnected()` | +6.0 MB |

**No two consecutive source methods are spatially adjacent.** A `[min,max)` span would be
`[0x822CE6D0, 0x82BC38FC)` ≈ **9.0 MB**, overlapping dozens of unrelated pinned TUs. Pinning
it is structurally impossible and would corrupt every TU in between (over-pin / honesty-gate
fail by construction).

## 3. The "gap" the autoid seed lives in is NOT MainHubPanel's

`autoid.json` independently flags `fn_82605638` (size 1100) as MainHubPanel via the strings
`role_is_global`,`role_rank` (the `OnMsg(RockCentralOpCompleteMsg)` ticker-result reader).
It sits in the 66744-byte gap between `EditSetlistPanel.cpp` (`…end:0x82603030`) and
`VoiceoverPanel.cpp` (`start:0x826134E8`). But autoid attributes that **same gap** to FIVE
different panels — it is fragmented COMDAT placement, not a clean owner:

```
autoid src tags in gap [0x82603030,0x826134E8):
  2  PatchPanel.cpp
  1  MainHubPanel.cpp   <- only the single ticker-reader fn
  1  CustomizePanel.cpp
  1  NewAwardPanel.cpp
  1  PassiveMessagesPanel.cpp
```

Only **1 of 45** MainHub functions is in this gap. The gap is shared scatter, with no
dominant TU — there is no MainHubPanel cluster here to bound.

## 4. The scattered VAs are ICF aliases, not transplantable MainHub bodies

Spot-check of the bindiff-claimed ctor `0x823D1600` (Ghidra decompile): the body is a
**MemStream / serialization** routine (`__0MemStream__QAA…`, `FUN_823ded68`, stream ctor +
virtual dispatch), **not** the MainHubPanel ctor (which must call
`LockStepMgr("main_hub_waiting", this)`, read `TheSessionMgr->mMachineMgr`, and
`MainHubAdvanceMsg::Register()`). This is an **ICF address-alias**: the linker folded
MainHubPanel's ctor with an identical-shape COMDAT and bindiff matched the Wii ctor onto the
primary's VA. The "owner" body at that VA is foreign. This is precisely the Waypoint-audit §3
phenomenon (ICF aliases mis-read as TU members) — except here it works AGAINST us: there is
no real contiguous Waypoint-style cluster underneath; the methods are genuinely dispersed.

## 5. Why a port-then-pin cannot work here (the structural blocker)

The wave-12 recipe is: port MWCC→MSVC → wire objects.json → **pin a bounded `.text` span**
→ gen map → objdiff. Steps 4–6 require a contiguous retail span. MainHubPanel has none:

- A compiled `MainHubPanel.obj` would emit ~45 functions in **source order**, but retail's
  copies are scattered (ICF + fragmented COMDAT placement) — objdiff pairs target↔base by
  the pinned VA range, so there is no range that brackets our compiled obj against retail's
  bodies. Even the 4 functions that share VAs with foreign pinned TUs cannot be claimed
  (they'd collide with RockCentral/MusicLibrary/PhysicsManager/Dir pins).
- This matches the playbook's explicit negative for big scattered TUs (bodyport-wave §2:
  "Big player TUs scatter across the whole binary — per-FUNCTION work only, never
  span-pinning; verified negative"). MainHubPanel is in that scatter class.

## 6. Honesty-gate disposition

- Contiguous TU exists? **NO** (45 fns over ~9 MB; 4 inside foreign pins).
- Bounded pin vs both neighbours possible? **NO** (no single owner gap).
- Coords = N/A (no real span; the degenerate `[0x822CE6D0,0x82BC38FC)` is a 9 MB over-pin).
- → **DEFER.** Recorded so no future wave re-attempts a MainHubPanel span-pin.

## 7. If revisited later (the only viable path)

Per-FUNCTION transfer, NOT TU-pinning:
1. The single in-gap real body `fn_82605638` (`OnMsg(RockCentralOpCompleteMsg)`) could
   in principle be matched if/when its surrounding gap is claimed by a future *neighboring*
   panel pin (PatchPanel / PassiveMessagesPanel / CustomizePanel / NewAwardPanel) that
   brackets it — i.e. it rides in on someone else's TU, not its own.
2. The 44 ICF-aliased VAs are only addressable via the DC3/Wii byte-identical fingerprint
   transfer lever (`pin_identified`/`reveal_sweep` over `unified_id`), where an anon fn that
   is ALREADY byte-exact just needs a symbol-map reveal — **but** those VAs are inside
   foreign TUs/gaps and would have to be revealed under the *owning* pin, not a MainHubPanel
   pin. That is reveal-sweep territory, not a port-then-pin task. Out of scope for this lane.
3. Do NOT scaffold `MainHubPanel.cpp` + objects.json without a pin — it compiles to dead
   weight (no compile→match edge) and pollutes the denominator.

## Evidence index
- Oracle: `../rb3/src/band3/meta_band/MainHubPanel.{h,cpp}`, `MainHubMessageProvider.{h,cpp}`.
- Scatter table: `unified_id_rb3wii.json` (45 `bindiff_src=…/MainHubPanel.cpp` hits).
- Autoid seed: `autoid.json` `fn_82605638` (strings `role_is_global`,`role_rank`).
- Gap fragmentation: `autoid.json` 5-panel mix in `[0x82603030,0x826134E8)`.
- ICF alias proof: Ghidra decompile `0x823D1600` = MemStream serializer, not the ctor.
- Belt context: `splits.txt` EditSetlistPanel `end:0x82603030`, VoiceoverPanel
  `start:0x826134E8` (66744-byte shared gap).
