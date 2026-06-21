# Wave-13 AccomplishmentConditional sliver-evict audit (DISCOVER, read-only main)

**Date:** 2026-06-20
**Lane:** `AccomplishmentConditional-evict` (DISCOVER/PLANNER)
**Main @** c00664c, baseline 9404 matched.
**Scope:** `AccomplishmentPlayerConditional` + `AccomplishmentSongConditional` —
reducer flagged as "dead-sliver squatters needing eviction+relocation to their
real cluster in GAP B".

## VERDICT: **DEFER BOTH** (evidence below). Neither is a clean +N sliver-evict.

- **AccomplishmentSongConditional** — verdict REFUTED. Its pin is NOT a dead
  sliver; it is already a healthy `matched=3/3 = 100%` real contiguous cluster
  with clean blob boundaries. Nothing to evict. The other ~20 own methods are
  COMDAT-scattered (port-then-extend territory, not evict).
- **AccomplishmentPlayerConditional** — the eviction premise is CORRECT (the
  bare-name pin IS a dead `mf=0` misattributed ICF sliver, and the real
  contiguous own sub-cluster IS located at `[0x825CDB40,0x825CDE50)` with clean
  pdata `[0x8221DB80,0x8221DBB8)`). **BUT** the wired source does not reproduce
  retail codegen for any of the 7 functions in that window → a relocate-pin
  registers the target obj but yields **+0 matched functions**. It is a
  **port-then-pin**, not a clean evict. Deferred to a body-port wave with the
  cluster map handed off below.

---

## 1. Structure: two different "Accomplishment*Conditional" split entries

`config/45410914/splits.txt` has TWO families of Accomplishment split entries:

- **Bare-name** (no path prefix), e.g. line 2379 `AccomplishmentPlayerConditional.cpp:`
  → dtk emits target `build/45410914/obj/AccomplishmentPlayerConditional.obj` +
  `asm/AccomplishmentPlayerConditional.s`. There is **no compiled obj** paired with
  this name (objects.json only declares `band3/meta_band/...`), so these bare-name
  pins are **dead `mf=0` slivers**.
- **`band3/meta_band/` prefixed**, e.g. line 2539
  `band3/meta_band/AccomplishmentSongConditional.cpp:` → pairs with the wired
  compiled obj `build/45410914/src/band3/meta_band/AccomplishmentSongConditional.obj`.

`report.json` (current main) confirms:
- `default/AccomplishmentPlayerConditional` (bare sliver): **matched=0, total=5, 0%**.
- `default/band3/meta_band/AccomplishmentSongConditional`: **matched=3, total=3, 100%**.

## 2. AccomplishmentPlayerConditional bare sliver = misattributed ICF flag-setters

The bare pin `[0x8243F178,0x8243F220)` (0xA8, 5 fns). Disassembly (`asm/Accomplishment
PlayerConditional.s`) shows all 5 are trivial **bit-flag setters** on one global:
```
lis r11, lbl_82C90838+0x12B8@ha ; lwz ; rlwinm r11,r11,0,N,M ; stw ; blr
```
These are ICF-folded `rlwinm` flag mutators with NOTHING to do with the TU. The
`scripts/target_symbol_map.json` mis-maps them:
```
0x8243F178 -> ?GetType@AccomplishmentPlayerConditional@@UBA?AW4AccomplishmentType@@XZ
0x8243F198 -> ?IsRelevantForSong@AccomplishmentPlayerConditional@@UBA_NVSymbol@@@Z
```
Both map entries point at the **wrong (sliver) addresses** — the map was seeded
from the stale spatial pin. This is the canonical "the map seeded the wrong
location" misattribution; the sliver reads 0% because the real bytes there are
foreign flag-setters.

## 3. The REAL AccomplishmentPlayerConditional cluster (Ghidra + blob/pdata)

Anchored via the unique Symbol strings `launch_part_difficulty_sym` (`0x820b5ec0`)
and `launch_filter` (`0x820b5eb0`). The only xref to `launch_filter` is from
**`0x825cdd58`** inside `fn_825CDCD8` — decompiling it reproduces the source
`Configure(DataArray*)` line-for-line (FindData(launch_part_difficulty_sym) →
FindArray(launch_filter) → loop → AddFilter). So the real TU lives in the big
**GAP B** region (`0x825Cxxxx`), inside blob `auto_03_825C3A44_text.s`
(`[0x825C3A44,0x825D0380)`).

Identified own methods (decompiled + cross-checked vs source):

| addr | size | method |
|---|---|---|
| `fn_825CCBE0` | 0xB00 | `InqConditionProgress` (the 30+ Symbol if-else chain) — **far away** |
| `fn_825CDB40` | 0x50 | `IsConditionMet` (calls InqConditionProgress) |
| `fn_825CDB90` | 0x58 | `IsFulfilled` (loops m_lConditions) |
| `fn_825CDBE8` | 0xC0 | `InqBestProgressValues` (float div loop) |
| `fn_825CDCA8` | 0x30 | `InqProgressValues` (`{ InqBest(); return 1; }`) |
| `fn_825CDCD8` | 0x130 | `Configure` |
| `fn_825CDE08` | 0x20 | `??__F` guard-clear (DAT_82dce9b4 &= ~1) |
| `fn_825CDE28` | 0x28 | `??__F` guard-clear (DAT_82dce9b4 &= ~2) |

Methods at `[0x825CDB40,0x825CDE50)` are a **contiguous 7-fn own sub-cluster**:
- bounded BELOW by `fn_825CDB20` (foreign `??__F` on DAT_82dce924 — different TU)
- bounded ABOVE by `fn_825CDE50` (foreign static-init on DAT_82dce9f0 — next TU)
- between `InqConditionProgress`(fn_825CCBE0) and this window sit ~58 foreign
  `??__F` guard thunks on DAT_82dce9a8/DAT_82dce924 → the TU is **COMDAT-scattered**.

pdata for the 7 fns is **contiguous** at `[0x8221DB80,0x8221DBB8)` (auto_01_8221CE28
_pdata.s), bounded by fn_825CDB20 below and fn_825CDE50 above. No splits overlap
(.text or .pdata) — a structurally clean relocate target.

## 4. WHY DEFER: the bodies don't match (the decisive measurement)

Byte-level compare of the wired compiled obj
(`build/.../meta_band/AccomplishmentPlayerConditional.obj`) vs target blob bytes,
relocation-normalized (functionRelocDiffs=none equivalent):

| fn | target words | compiled words | non-reloc mismatches |
|---|---|---|---|
| `Configure` (fn_825CDCD8) | 76 (0x130) | 54 (0xD8) | 40 — **size + prologue differ** |
| `InqBestProgressValues` (fn_825CDBE8) | 48 | 49 | 24 |
| `IsFulfilled` (fn_825CDB90) | 22 | 22 | 15 |
| `IsConditionMet` (fn_825CDB40) | 19 | 20 | 5 |
| `InqProgressValues`/IsRelevantForSong (fn_825CDCA8) | 9 | 2 | (different fn) |

Target `Configure` is 0x130 vs compiled 0xD8 with a different frame (`subi r31,r1`
funclet vs `stwu` only) — a genuine codegen/inlining divergence, not a near-miss
nudge. None of the 7 are byte-exact. The two `??__F` guard-clears have no findable
compiled symbol (atexit scope-counter mangling differs).

**Therefore a relocate-pin to `[0x825CDB40,0x825CDE50)` registers the target obj
but matches 0 functions** → +0, not the reducer's optimistic ~+10. It fails the
HONESTY GATE as a pin-only lane (net 0; and the window would show 5+ real foreign-
to-each-other near-miss bodies, not 100%).

## 5. AccomplishmentSongConditional: already healthy, verdict refuted

Pin `[0x8264CEF0,0x8264D018)` (mf=3/3 = 100%). Blob `auto_03_8264C238_text.s` ENDS
exactly at 0x8264CEF0 and `auto_03_8264D018_text.s` STARTS exactly at 0x8264D018 —
the pin is its own cleanly-carved region. Map pairs it to `UpdateIncrementalEntry
Name`, `InqProgressValues`, `IsSymbolEntryFulfilled` (all matching). The dtor is
scattered to `0x825D1240` (separate). The remaining ~20 own methods (CheckStars/
Score/Accuracy/Streak/HoposPercentCondition...) are scattered with gap-to-next
0x6948 — port-then-extend, not evict. **Nothing to evict here.**

## 6. Disposition

- DO NOT land a pin for either TU this lane (no net gain).
- The bare-name `AccomplishmentPlayerConditional.cpp:` sliver is harmless (mf=0);
  removing it is cosmetic (no delta), so not worth a lane on its own.
- **Handoff (discovered_frontier):** the AccomplishmentPlayerConditional cluster
  is now precisely located. A future **body-port wave** should port the bodies
  from rb3-Wii (`../rb3/src/band3/meta_band/AccomplishmentPlayerConditional.cpp`)
  to retail codegen, THEN relocate the pin to `[0x825CDB40,0x825CDE50)` (+ the
  big InqConditionProgress at fn_825CCBE0 separately). Estimated ceiling ~7-9 fns
  once bodies match, but each needs per-fn regalloc/inline work first.
