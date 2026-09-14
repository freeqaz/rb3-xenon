# The "invisible source" sweep — TUs wired into no build, and externals declared but defined nowhere

**Lane W16-D, 2026-09-14.** Worktree `~/tmp/wt-w16-d`, branch `w16-d`, off `main` at `a427ff73`.
Every number below is from `build/45410914/report.json` on a **full** `./tools/ninja-locked`,
ruler **`name_check`** (the shipped default; objdiff 4.2.8, `tool_binary_hash 14ac591a0814e6c9`),
read with `int(x.get(k, 0))` because several fields are JSON strings.

## Why this class exists at all

The match build **compiles but never links**. Nothing in it can notice an undefined symbol, so three
different things are structurally invisible to the metric:

1. a definition inside `#ifdef HX_NATIVE` — **drained** by W16-A (one payable function tree-wide, +64 B);
2. a TU absent from `objects.json` and `#include`d by nobody — `tools/project.py` drops the missing
   compile edge **silently** (`warn_missing_source=False`). Part 1 below;
3. a symbol declared in a header, called from compiled code, and defined in **no** TU. Part 2 below.

## Ledger

| item | pre-registered | measured | ruler / build |
|---|---|---|---|
| `?DoCompress@DxTex@@QAAXPAX@Z` | A / B / C | **A: +1 fn / +284 B** | name_check, full build `e186baa9` |
| `?UserHasController@@YA_NPAVLocalUser@@@Z` | A / B / C | **A: +1 fn / +44 B** | name_check, full build `a661fc48` |
| `?Unlock@MemHandle@@QAAXXZ` | A / C (B impossible) | **A: +1 fn / +20 B** | name_check, full build `aef03c1c` |
| **lane total** | | **+3 fns / +348 B** | |

Baseline 43,108 fns / 3,920,752 B / 38.26634% → final **43,111 / 3,921,100 / 38.269733%**
(fuzzy 49.23215 → 49.235542). Every step was reconciled by **set-diff of the `fuzzy == 100` row
set** (`tools/rowset_snapshot.py`), never by a rounded gap: **3 rows crossed in, 0 fell out.**

### A mechanism correction worth keeping

The brief warned that defining a symbol converts each caller's **forgiven** placeholder relocation
into a **checked** one (W16-A measured −40 B inside a +404 B change). **That did not happen here, and
the reason is general:** an *undefined external still emits a correctly-NAMED relocation*. Placeholder
forgiveness applies to `fn_`/`lbl_`/`jumptable_`/`data_`/`bss_`/`rdata_` targets — names our compiler
never emits for a declared symbol. So supplying a **body** behind a name that was already spelled
correctly costs callers nothing; the W16-A economics bite when the **name changes**, not when a body
appears. All three landings show FELL OUT = 0, which is the prediction.

Corollary for pre-registration: **outcome B (pairs-but-arg-only, +1 fn / +0 B) is structurally
impossible for a relocation-free body.** `name_check` charges relocation *names*; a leaf with no
relocations gives it nothing to charge, so such a row is an **A-or-C bet**. `Unlock` was called that
way in advance and landed A.

## Part 1 — unwired TUs

Re-derived independently of W16-A. Compiled TUs come from `build.ninja`'s compile edges (**not** from
`objects.json`, which declares 1,434 objects of which 230 have no source on disk); the `#include`
closure comes from `ninja -t deps` in the **built** worktree.

> ⚠ **The trap that cost the first derivation.** `deps = msvc` records only `/showIncludes`
> **headers**. A compiled `.cpp` is an *explicit ninja input* and never appears as a discovered dep,
> so reading compiled TUs out of the deps DB reported **1,298** unwired files — including
> `band3/game/Player.cpp`. A second defect followed: the compile-edge regex missed
> `src/system/decomp_pch.cpp`, whose edge declares an implicit output
> (`build … pch/decomp_pch.obj | … system.pch: msvc_pch_create …`); fixing it moved the count
> 1,206 → **1,207**.

**1,210 `.cpp` under `src/` · 1,207 compiled · 193 reached by `#include` · 23 unwired**, of which 9 are
`src/xdk/**` (out of scope) and 5 are soundtouch vendor. That leaves **9 first-party files**.
This reconciles exactly with W16-A's 16: their count also carried two json-c `.c` files, which a
`.cpp`-only derivation excludes. **The disagreement was never about the graph, only about where the
vendor line is drawn.**

*Positive control:* `rnddx9/Tex.cpp` now appears in the **included** set, proving the instrument sees
W16-A's scatter fix rather than reporting a stale graph.

### Per-file verdicts

| file | verdict | channel |
|---|---|---|
| `system/world/BeatClock.cpp` | **retail-absent — do not wire** | W16-A; corroborated here: absent from rb3-Wii too |
| `system/synth/AudioDucker.cpp` | **retail-absent — do not wire** | W16-A; same corroboration |
| `system/synth/Sound.cpp` | **retail-absent — do not wire** | DC3-only: present in `../dc3-decomp`, **absent from `../rb3`** (RB3's own game decomp), **0 retail map rows** for class `Sound` |
| `system/synth/ThreeDSound.cpp` | **retail-absent — do not wire** | same two channels, same result |
| `system/synth_xbox/StreamReceiver.cpp` | **correctly unwired — wiring would DOUBLE-DEFINE** | a 2-line shim, `#include "synth_xbox/StreamReceiver360.cpp"`; `StreamReceiver360.cpp` is already compiled directly (`objects.json:405`) |
| `system/rnddx9/Env.cpp` | **redundant — do not wire** | its only body is `DxEnviron::Select`, which has no retail row; DxEnviron's three real rows (`StaticClassName` / `ClassName` / `NewObject`) are **already at fuzzy 100.0 in `default/Rnd_Xbox`** |
| `system/rnddx9/Utl.cpp` | **no retail evidence — leave** | `MakeVertexBuffer` / `MakeIndexBuffer` have **no map rows**; file also carries a spurious `#pragma once` at the top of a `.cpp` (harmless, but it is a defect) |
| `system/synth_xbox/FxSendSynapse360.cpp` | **not actionable** | empty stub vs real retail rows (356 B `SyncEffectParams`, 72 B `CreateFx`); **DC3's file is byte-identical to our empty stub**, so the brief's "port the bodies from dc3" has no source to port |
| `system/synth_xbox/FxSendPitchShift360.cpp` | **not actionable** | same shape, 76 B retail row, same byte-identical DC3 stub |

⚠ **Channel caveat, stated because a verdict here closes a vein.** "0 map rows" is *weaker* than it
looks — the map names only ~41.7% of functions, so absence alone is an identification gap, not proof.
That is why each retail-absent verdict above rests on **two converging channels** (map rows **and**
oracle provenance across two sibling repos), and why W16-A's lesson applies: the RTTI/string channel
is blind to non-polymorphic classes, so it was not used as the deciding evidence for any of them.

## Part 2 — declared-but-undefined externals

`tools/undefined_externals_census.py`. Over every compiled `.obj` under `build/45410914/src/`, it
collects symbols that are **undefined externals** (COFF section 0, storage class 2 `EXTERNAL`, value 0),
subtracts everything defined in any compiled obj, and intersects the remainder with retail's named
rows. COFF discrimination that matters:

- section 0 + class 2 + value **≠ 0** is a **COMMON** symbol — storage *is* defined; not undefined.
- storage class **105** is `WEAK_EXTERNAL` — it has a default resolution; tracked separately.

`--selftest` plants a fake undefined symbol and requires it to be found. The fixture objects are
**hand-built COFF** (`_make_fixture_obj`) so the selftest can never silently SKIP. It was then
**proved able to fail**: sabotaging the parser to drop the storage-class test turns it red. An earlier
version of one assertion was a chained comparison whose first clause was always False — i.e. vacuous —
and was rewritten before it could pass for the wrong reason.

**Census, this tree:** 1,206 objects scanned · 117,411 defined symbols · 13,126 undefined references ·
2,339 unresolved in scope · **211 intersect a named retail row (56,340 B)**.

### The triage that governs the whole vein

| class | rows | bytes | meaning |
|---|---:|---:|---|
| **first-party, pairable** | 37 | 19,296 | a body here can pair and can pay |
| XDK/CRT units | 48 | 10,668 | out of scope except the memory-management subset |
| **UNPAIRABLE** (`auto_*` unit, or no unit) | 126 | 26,376 | **needs a PIN, not a body** |

The unpairable half is the important structural fact: **`auto_*` units have no base obj, so nothing we
write can pair there** — objdiff pairs target↔base by name *within a unit*. Writing bodies for those
126 rows would buy exactly 0 B. They belong to a pinning lane. `?ReleaseAutoRelease@DxRnd@@QAAXXZ`
(652 B) is the clearest example: **DC3 has the body** (`rnddx9/Rnd_Xbox.cpp:930`) and it still cannot
pay, because the row lives in `default/auto_03_8273CBFC_text`.

### Real bugs found on the way

1. **A struct field width the DC3 oracle gets wrong.** DC3 declares `CompressDesc::alpha` as
   `RndTex::AlphaCompress` — an enum, 4 bytes, compiling to `lwz` + signed `cmpwi`. **Retail loads a
   byte:** `lbz r11,0x4(r30); cmplwi r11,0x0` at `0x82734148+29`. Byte-sized and compared **unsigned**,
   and it still holds three values (`StartCompress` tests `== 2`), so it is a `u8`, not a `bool`.
   First build landed fuzzy **97.46479** (outcome C); the one-field correction took it to **100.0**.
   *Retail bytes outrank the oracle* — the oracle was simply wrong about the width.
2. **A function our port silently dropped.** `UserHasController` is declared at `Joypad.h:348` and was
   defined in no TU: our port of `Joypad.cpp` dropped exactly this one function while keeping its
   neighbour. Restored from `../rb3/src/system/os/Joypad.cpp:594`, in the oracle's own source position.
3. **A stale pre-TU5 address in a comment.** `MemHandle::Lock` carried `fn_827966E8`, which names
   nothing on the current target (main has targeted TU5 since 2026-07-15). Real row is `0x827BB6F8`.
4. **`RB3_RNDTEX_DC3_CRC` had an ungated second use site.** `rndobj/Tex.h:156` claims the DC3-only CRC
   member's "only use is the COPY_MEMBER in Tex.cpp". That claim was **wrong** — `DxTex::ResetSurfaces`
   used it too, and nothing caught it precisely because `rnddx9/Tex.cpp` was wired into no build at all.

### Blocked, with the retail-byte reason

These are the escalation candidates. Each is blocked for a stated, measured reason — not for lack of
effort — and none of them is "try harder at the source".

| target | bytes | why it is blocked |
|---|---:|---|
| `?SyncEffectParams@FxSendReverb360@@` | 3,280 | **`FxSendReverb360.cpp` does not exist in DC3.** This corrects the brief, which says to write it from dc3. There is no oracle in either sibling repo. |
| `fft_altivec` · `fft_recursive` · `fft_real_forward_altivec` · `SquareComplexTransposeVector` | 6,432 | **Declared and not defined in DC3 either**, and absent from rb3-Wii. Hand-written VMX128 DSP with no oracle anywhere. |
| `JoypadPollCommon` | 2,468 | Six EEPROM members exist only as `unkXX` in our `JoypadData`, and `ReadSingleJoypad` is missing entirely. Needs struct-layout renaming on a widely-used class first; a first-pass port would land at outcome C and pay 0. |
| `?MemFreeH@@` + `?_MemAllocH@@` | 192 | **Blocked on an `inline`.** Both open with a bare `bl ?MainThread@@YA_NXZ` whose result is discarded (retail's `MILO_ASSERT` evaluates-and-discards, so only the call survives). Our `MainThread` is `inline` (`os/OSFuncs.h:8`), so at `/O1 /Ob2` we cannot emit that call — we would expand `gMainThreadID`/`GetCurrentThreadId` inline instead. De-inlining a header on the PCH path to buy 192 B is a tree-wide codegen cascade that needs its own A/B. |
| `DataResultList` family | 388 | `src/band3/net_band/DataResults.h` exists but **`DataResults.cpp` was never written** (rb3-Wii has the full file). Blocked upstream of source: 3 rows live in `default/DataFile`, whose splits heading is an over-broad **19-block grab-bag spanning 0x822E4F70–0x8276D808**, and the 4th (`Clear`) lives in `default/RockCentral`. Belongs to a splits lane. |
| `?ReleaseAutoRelease@DxRnd@@` | 652 | DC3 **has** the body; the retail row is in `default/auto_03_8273CBFC_text`, an `auto_*` unit with no base obj. Cannot pair without a pin. |
| `??0FriendsProvider@@` + `?Reload@FriendsProvider@@` | 140 | **No oracle in either sibling repo.** |

### Decoded for the next lane (so nobody re-derives it)

`_MemAllocH` (0x827BD190, 120 B) and `MemFreeH` (0x827BCA08, 72 B) are fully decoded, and the
**rb3-Wii oracle is wrong twice** where retail is unambiguous:

- oracle calls `_MemFree`; retail calls `?MemFree@@YAXPAX@Z` (0x827BC430) — corroborating
  `utl/MemMgr.h`'s own phantom note that `_MemAlloc`/`_MemFree` are MWCC spellings retail lacks;
- oracle calls `_PoolFree(size, MainPool, h)` with **three** args; retail's 0x827BADB0
  `?PoolFree@@YAXHPAX@Z` takes **two** (`li r3,0x4; mr r4,r31`).
- `_MemAllocH` **inlines the `MemHandle` ctor** as three stores plus an aliasing reload of `mAlloc`;
  `MILO_ASSERT(heap && heap->mUseHeapAlign)` survives only as a bare `bl` to the unnamed 0x827BBA68
  (`GetCurrentHeapNum`), because the `gHeaps` loads are pure and die.
- The 2-arg `?PoolAlloc@@YAPAXHH@Z` is **already proven** to fold onto the 5-arg survivor in
  `scripts/symbol_aliases.json`, so a 2-arg call there is forgiven, not charged.

### Confirmed-oracle leftovers (small, unblocked, not attempted)

`?Release@VertexBufferData@DxMesh@@` 68 B (DC3 `rnddx9/Mesh.cpp:112`, unit `default/Rnd_Xbox`) and
`??0Shuttle@@QAA@XZ` 32 B (Wii `band3/game/Shuttle.cpp:5`, unit `default/FreestylePanel`). Both have a
named oracle and a pairable unit; neither was attempted for budget reasons, not for a technical one.

## What this lane did NOT do

- Did **not** wire any TU. All nine first-party unwired files adjudicated to leave-alone, and two of
  them (`StreamReceiver.cpp`, `Env.cpp`) would have been actively harmful to wire.
- Did **not** touch `splits.txt`, `objects.json`, `symbols.txt`, or any map/alias file — so no pin,
  denominator or forgiveness effect is mixed into the +348 B.
- Did **not** run the permuter (off by standing directive).
- Did **not** port `StartCompress` / `FinishCompress` alongside `DoCompress`: they are undefined the
  same way but carry **no named retail row**, so they cannot pair, and leaving them out kept the
  `DoCompress` measurement a clean one-variable change.
