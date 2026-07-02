# WAVE-5 lane handoff — BandList (system/bandobj)

Branch: `w5-bandlist`  ·  Worktree: `/home/free/tmp/wt-w5-bandlist`
Baseline: main `5cb96d4`  ·  Commit: `1194919`

## Outcome (one line)
Ported `system/bandobj/BandList.cpp` (668 lines) Wii→MSVC, wired
engine/NonMatching with bounded splits for all 3 worklist targets. Clean
compile, ~99% fuzzy on each target. **0 strict pins** — a real **+8
struct-offset drift** blocks true-100; per OWNER POLICY no map pins were
added. Fuzzy-paired source is the deliverable.

## What landed (commit 1194919, path-limited)
- `src/system/bandobj/BandList.cpp` — new port (full TU, all methods).
- `config/45410914/objects.json` — `"system/bandobj/BandList.cpp": "NonMatching"`
  in module **engine** (inserted after BandCharDesc.cpp).
- `config/45410914/splits.txt` — new `BandList.cpp:` block:
  ```
  .pdata start:0x821F64D0 end:0x821F64E0   (dtk-derived unwind for Conceal/ConcealNow)
  .pdata start:0x821F67F8 end:0x821F6800   (dtk-derived unwind for Reveal)
  .text  start:0x82328EA0 end:0x82328F30   (Conceal + ConcealNow, gap after BandCharDesc)
  .text  start:0x8232C410 end:0x8232C458   (Reveal, gap after HamNavList / before HamLabel)
  ```
  The `.pdata` lines were auto-added by the dtk SPLIT re-serialization (correct
  attribution). Overlap self-check: **0 pdata / 0 text overlaps**.
- `scripts/target_symbol_map.json` — **unchanged** (3 pins were added to measure,
  then reverted; verified byte-identical to base).
- `config/45410914/symbols.txt` — **unchanged** (dtk already split all 3 targets
  as `fn_` with correct sizes: fn_82328EA0=0x4C, fn_82328EF0=0x40, fn_8232C410=0x48).

## Port adaptations (Wii MWCC → xenon MSVC)
1. **Rev-macro dialect.** The header `BandList.h` uses `DECLARE_REVS;`, which
   only exists in `obj/ObjMacros.h` (1-arg `INIT_REVS`/`LOAD_REVS`→`gRev` class
   members). `obj/Object.h` (pulled by `ui/UIList.h`) defines a *competing*
   2-arg dialect (`INIT_REVS(rev,alt)`→file-static, `LOAD_REVS`→`BinStreamRev d`).
   Fix without touching the header: include order
   `ui/UIList.h` → `obj/ObjMacros.h` → `bandobj/BandList.h`, so ObjMacros wins
   *and* `DECLARE_REVS` is defined before line 99 of the header.
2. `TheUI.InitResources` → `TheUI->InitResources` (`TheUI` is `UIManager*`).
3. `UIList::PreLoadWithRev(bs, rev)` (2-arg, Wii) → xenon's `PreLoadWithRev(BinStreamRev&)`;
   construct `BinStreamRev d(bs, mBandListRev); UIList::PreLoadWithRev(d);`.
4. `ObjPtr<RndTransAnim, ObjectDir>` → `ObjPtr<RndTransAnim>` (xenon is 1-param).
5. `SelectedDisplay()` (Wii UIList method) → `mListState.SelectedDisplay()`
   (xenon moved it onto UIListState).
6. Dropped Wii-only matching hacks in `UpdatePulseAnim` (`#pragma fp_contract`,
   `char _slotpad[16]`, dead `mStartTimes[i];`) — NonMatching, harmless.

Compile: clean under `cl.exe X360/16.00.11886.00` (only benign C4005 macro-redef,
C4258 for-loop-var, C4391/2 intrinsic warnings).

## Per-target verification (objdiff-cli direct, `--include-instructions`)
| VA | symbol | tgt/base size | fuzzy = norm % | verdict |
|----|--------|---------------|-----------|---------|
| 0x82328EA0 | `?Conceal@BandList@@QAAXXZ`    | 76 / 76 (exact) | 99.05 | NOT 100 |
| 0x82328EF0 | `?ConcealNow@BandList@@QAAXXZ` | exact           | 98.94 | NOT 100 |
| 0x8232C410 | `?Reveal@BandList@@QAAXXZ`     | exact           | 98.78 | NOT 100 |

Identities are **correct** (logic + size match exactly) — these are genuinely
BandList::Conceal/ConcealNow/Reveal. They are simply not byte-100.

## Why not 100 — two diff classes
Every mismatched instruction is one of:

**(A) Reloc-naming residue** (would normalize IF the referenced target symbols
were named — but they belong to OTHER units, out of this lane):
- `bl fn_82722790`   == `?UISeconds@TaskMgr@@QBAMXZ`
- `lis/addi lbl_82DA0017` == `?TheTaskMgr@@3VTaskMgr@@A` (the TheTaskMgr global)
- `bl fn_8232B280`   == `?UpdateShowingState@BandList@@QAAXXZ` (a BandList method
  in gap-2 @ 0x8232B280 — could be pinned by a follow-up, but see (B)).

**(B) Real +8 struct-offset drift** — THE BLOCKER (will NOT normalize):
```
Conceal:  lwz/stw mBandListState   TGT 0x33c(r3)  vs  BASE 0x344(r3)   (+8)
          stfs    timestamp        TGT 0x340(r31) vs  BASE 0x348(r31)  (+8)
```
My build places `mBandListState` / `mShouldbeRevealedTimeStamp` 8 bytes higher
than retail. Measured facts (COFF probe of my build):
`sizeof(BandList)=916 (0x394)`, `sizeof(UIList)=636 (0x27c)`,
`sizeof(std::map<int,*>)=24`, `sizeof(ObjPtr<T>)=12`, `sizeof(ObjVector<T>)=16`.
Those container/ptr sizes are the *standard* MSVC-stlport sizes (identical to what
retail used — both targets are 32-bit BE PPC), and my computed layout is
self-consistent with `sizeof=916`. So the +8 is **not** container inflation: it is
an extra 8 bytes somewhere in the UIList base chain (or a member) that retail's
non-MILO_DEBUG build did not have. No `#ifdef MILO_DEBUG` member was found in the
UIList/UIComponent/Draw/Object base headers (grep clean), so the root cause is
subtler than the usual CharEyes/GemTrackDir landmine.

## What a lander must know
- **Do not map-pin these three as-is** — they are below true-100 (real +8 diff,
  not reloc residue). The commit intentionally adds NO map entries.
- The port is a clean **fuzzy-paired source** win: wired + split, ~99% on 3 fns.
  Safe to land as-is (mirrors how other NonMatching ports land at strict-0).
- To convert to +3 strict later: resolve the +8 UIList-family layout delta. This
  is a **shared-base** investigation (keystone-territory, NOT a port lane) — a
  fix there would flip all 3 at once. Cross-check whether other UIList-derived
  wired classes silently carry the same +8 (if so it is definitely in UIList /
  its bases; if BandList is unique, suspect a stray member in `BandList.h`).
- Splits already carry dtk-derived `.pdata`; if re-split, keep the unit-aware
  block (do not let a flat EOF union scatter it — SOP incident #2).

## Files for reference
- Port: `/home/free/tmp/wt-w5-bandlist/src/system/bandobj/BandList.cpp`
- Oracle: `cd /home/free/code/milohax/rb3 && bin/analyze-function Conceal__8BandListFv`
- Wii source: `/home/free/code/milohax/rb3/src/system/bandobj/BandList.cpp`
