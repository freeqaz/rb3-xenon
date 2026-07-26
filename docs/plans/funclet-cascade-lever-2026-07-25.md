> **CORRECTION (lane L, 2026-07-26) — read §4 and §8 with this in mind.**
> The calibration table in §4 is **correlational, not causal**, and the §8 plan
> to reach the ~2,300 funclets behind unnamed parents *by naming the parents*
> does not work. Two whole-binary A/B experiments on the 27,629 baseline:
>
> * Removed the `target_symbol_map.json` entries for **116 named parents with a
>   frame MISMATCH** (473 funclets, 235 unmatched) → 27,629. **GAINED 0, LOST 0.**
> * Removed the entries for **60 named parents with frame OK + savegprlr OK**,
>   chosen for having the most currently-*matched* funclets (1,446 between them)
>   → 27,582. **LOST 47 — and all 47 were the parent symbols themselves. Zero
>   funclets moved.**
>
> A parent's map entry has **no causal effect** on whether its EH funclets match;
> objdiff pairs funclets independently of it. Parents whose compiled frame is
> exact are simply parents we ported well, and well-ported functions have
> well-matching funclets. §5's +264 *was* causal — but it came from fixing the
> **source** so the compiled frame changed, which is body work, not naming.
>
> What naming actually buys: the parent itself flips iff our body is already
> reloc-masked byte-identical at that VA, plus **observability** — naming makes
> our frame readable, which turns the unnamed pool into a frame-defect worklist
> for exactly the §7 recipe. Lane L's confirmation on live data: a wave that
> named 11 unnamed parents owning 21 funclets gained the 11 parents and **0**
> funclets.
>
> Tooling from lane L: `scripts/harvest/unnamed_parent_verify.py` (unit-scoped
> byte-identity naming, `--validate` held-out) and
> `scripts/harvest/eh_signature_match.py`. Also measured and closed there: the
> RTTI-catch-name content channel that
> `docs/plans/identification-discriminators-2026-07-25.md` calls "the one
> unexplored content channel left" is **provably empty** — all 1,074 retail
> `HandlerType` entries have `pType == NULL` (RB3 uses `catch (...)` throughout).

# The EH-funclet cascade lever — tooling, honest pool size, and first harvest

**Lane I, 2026-07-25.** Worktree `~/tmp/wt-laneI-funclet`, branch `laneI-funclet`,
base `ca711730` (27,223 strict).

Tool: **`scripts/harvest/funclet_cascade_rank.py`**
Prior art: `docs/plans/slm-setstate-reconstruction.md` §7/§8 (lane F, +145 funclets),
lane C (+32).

---

## 1. What the lever is

MSVC X360 emits a separate **EH funclet** (unwind action / catch handler) per
cleanup state and catch block of a `/EHsc` function. A funclet's first
instruction is

```
subi rX, r12, <PARENT FRAME SIZE>
```

— `r12` holds the parent's *pre-`stwu`* stack pointer, so **every funclet
literally encodes its parent's frame size in its own machine code**. Lanes C
and F established empirically that a funclet flips to strict 100% as soon as the
parent's (a) frame size and (b) `bl __savegprlr_N` range are exact — *regardless
of whether the parent's body matches*. Lane F flipped 145 funclets from a 6-line
source diff with the parent still at 0%.

This session that number was beaten: **one source edit flipped 230 funclets**
(+264 whole-binary).

## 2. How the tool resolves parentage — exactly, not heuristically

Ground truth is the extracted retail PE `orig/45410914/band.exe` (which *is* the
TU5 image — byte-identical to `dtk xex extract orig/45410914/default.xex`).

1. `.pdata` (VA `0x821F1600`, raw `0x1F1600`) — 8 bytes per function:
   `{ BeginAddress, PrologLen:8 | FunctionLen:22 | ThirtyTwoBit:1 | ExceptionFlag:1 }`,
   **big-endian** (the PE *headers* are little-endian; the payload is not).
   Length in bytes = `((w1 >> 8) & 0x3FFFFF) * 4`. 57,733 entries; 9,145 carry
   the exception flag.
2. For an exception-flagged function the two DWORDs immediately **before** its
   entry point are `{ handlerAddress, handlerData }` (classic MIPS/PPC PE
   convention). Every handler here is `0x82829530` (`__CxxFrameHandler`).
3. `handlerData` points at an MSVC `_s_FuncInfo` (magic `0x19930522`, 9 words):
   `magic, maxState, pUnwindMap, nTryBlocks, pTryBlockMap, nIPMapEntries,
   pIPtoStateMap, pESTypeList, EHFlags`.
   * `UnwindMapEntry { int toState; void (*action)(); }` (8 B) → unwind funclets
   * `TryBlockMapEntry { tryLow, tryHigh, catchHigh, nCatches, pHandlerArray }`
     (20 B) → `HandlerType { adjectives, pType, dispCatchObj, addressOfHandler }`
     (16 B) → catch funclets
4. Catch funclets can themselves carry EH data; the tool collapses to the **root**
   parent (r12 is always the root frame pointer).
5. An independent **prologue screen** (`subi rX, r12, imm` as instruction 0) over
   all 57,733 `.pdata` entries provides the census and cross-checks (2).

**Coverage result: every one of the 16,999 prologue-screened funclets has an
exact EH-derived parent. Zero screen-only, zero mis-attribution.** Lane F's
"12 funclets with parent frame 0xF0 and no matching function in the pinned span"
class is now answered by name rather than by frame-size search — the tool flags
it as `PARENT_OFFUNIT` / `PARENT_UNPINNED`.

### ⚠ Trap: do NOT screen the dtk asm address column

`build/45410914/asm/*.s` labels the *second and later* `.text` ranges of a
scatter-gathered unit with synthetic `unit_base + obj_offset` addresses. Only
~20% of the asm-listed addresses agree with the PE. Example — `MetaPanel.s`
prints `.fn fn_82570688` under a comment address of `0x8251BA58`; the **label
name** is the real VA, the comment column is not. Anything VA-based must read
the PE (or the `.fn fn_<VA>` labels), never the comment column.

Other joins: VA → unit from `splits.txt`'s pinned **`.pdata`** ranges (each
8-byte entry names exactly one function VA, so it survives COMDAT scatter);
VA → report symbol from `scripts/target_symbol_map.json` else `fn_%08X`; our own
frame + `__savegprlr_N` read directly out of the compiled COFF in
`build/45410914/src/...` (so a full ranking costs no objdiff run —
`__savegprlr_N` is recovered from the `bl`'s relocation symbol name).

`__savegprlr_14` = `0x82829220`, `__restgprlr_14` = `0x82829270` (asserted at
startup so a wrong binary fails loudly).

## 3. Honest pool size — the 16,821 figure is not the opportunity

```
pdata_functions             57,733
eh_flagged_functions         9,145
funclets (prologue screen)  16,999      <- matches the ~16,821 figure in memory
funclets (EH-derived)       26,321      <- includes 9,322 ordinary out-of-line
                                           cleanup fns (own stwu, no r12 dep):
                                           normal decomp targets, NOT cascade
funclets in pinned units    11,229
  ...already matched         8,465      (75%)
  ...ADDRESSABLE              2,764
```

**The strategic number is ~2,800, not 16,821.** Three quarters of the pinned
funclet population already matches, and ~5,800 funclets live in unpinned spans.

## 4. Measured calibration (the lever is real)

Bucketed over all 4,298 pinned-funclet parents:

| bucket | parents | funclets | matched | rate |
|---|--:|--:|--:|--:|
| parent has no base symbol | 2032 | 5376 | 3069 | **57.1%** |
| frame ok + savegprlr ok | 2150 | 5318 | 5146 | **96.8%** |
| frame ok, savegprlr MISMATCH | 20 | 118 | 90 | 76.3% |
| frame MISMATCH | 96 | 417 | 160 | **38.4%** |

Getting frame **and** `__savegprlr_N` exact takes a parent's funclets from ~38%
to ~97% matched. That is the whole lever in one table, and it is *measured*, not
priced.

Implied remaining yield: ~250 funclets from the 96 frame-mismatch parents, ~30
from the savegprlr bucket, and ~2,300 behind the "no base symbol" wall (those
need a `target_symbol_map.json` reveal entry before the frame can even be read —
a separate, larger lane).

## 5. Harvest — `NextSongPanel::CountOrCreateExpandedDetails` (+264, 0 lost)

Ranked #1: `0x82645320`, **230 funclets, all unmatched**, frame `0x860` vs retail
`0x880`, `__savegprlr_14` on both sides, parent body at 82.0%.

Decisive pre-check: our object had **230 funclet sites at frame 0x860 and retail
has 230 at 0x880** — identical unwind-state structure, so the entire gate was 32
bytes of stack. (This "count the funclets on both sides first" check is worth
doing before any body work: equal counts ⇒ pure frame problem.)

Cause was lane F's force-multiplier (3): retail declares **38 function-local
`static Symbol`s** (one shared guard word at `0x82E0193C`, storages `0x82E01938`
downward); our port used the `Symbols*.h` globals. Porting the blocks to their
retail declaration points — recovered by walking `bl ??0Symbol@@QAA@PBD@Z` sites
in the retail function and reading the `.rdata` string operands — moved the frame
`0x860 → 0x870 → 0x890`, and relocating `vocals_grid` from the top block down to
its use site inside the harmony branch landed it exactly on `0x880`.

**27,223 → 27,487 strict, 0 lost.** The parent body is still ~82%.

## 6. Targets worked and walled

| target | funclets | state | verdict |
|---|--:|---|---|
| `NextSongPanel::CountOrCreateExpandedDetails` | 230 | frame fixed | **CLOSED, +264** |
| `RockCentral::RecordPerformance` | 79 | Δ −0x10, 79≡79 states | **open** — converting all 56 `ADD_DATA_PAIR` keys to retail's function-local `static Symbol` form did **not** move the frame at all. The 0x10 is elsewhere; needs a slot-ladder diff. Experiment reverted. |
| `Game::Handle` | 21 | Δ −0x20 | **walled (net-negative)**. Root cause found and it is correct: retail's handler chain continues `RemoteLeaderLeftMsg → ButtonDownMsg → JoypadConnectionMsg`; we stop at `RemoteLeaderLeftMsg`. Adding both `HANDLE_MESSAGE`es + `OnMsg` stubs moved the frame `0x120 → 0x130` and cut Handle's insert/delete clusters 69 → 21 — but the two `OnMsg` declarations in `Game.h` perturbed **`MusicLibrary`** (a downstream includer) and cost 24 funclets there, for a whole-binary **−24**. Reverted. Worth retrying with the declarations hidden from downstream TUs. |
| `BandCharacter::OnSetFileMerger` | 17 | Δ +0x10 | **CLOSED, +14**. Ours was 0x10 *over*: objdiff clusters were pure inserts for a `LOADMGR_EDITMODE` arm retail stripped (edit mode is dev-only), whose extra `FilePath` temp was the surplus 0x10. |
| `BandDirector::OnFileLoaded` | 11 | frame exact, sgpr 15 vs 16 | **deferred** — we save *one more* callee-saved register than retail; that is regalloc, not a source-shaped lever. Body is also only 41% matched. |

**Session total: 27,223 → 27,501 strict (+278), 0 lost, across 2 landed targets.**
The addressable funclet pool went 2,994 → 2,750.

A third shape of frame defect showed up alongside the two lane-F ones, and it is
the cheapest of the three to spot: **a dev-only branch retail stripped**
(`LOADMGR_EDITMODE`, `MILO_ASSERT`, `TheDebug.Fail(MakeString(...))`) leaves a
surplus temporary in our frame. When the delta is *positive* (we are bigger),
look for a stripped debug/edit-mode arm before anything else; the objdiff signal
is a run of pure `insert`s with no matching target instructions.

## 7. Reusable recipe

```bash
WT=~/tmp/wt-laneI-funclet
python3 scripts/harvest/funclet_cascade_rank.py --census      # pool sizing
python3 scripts/harvest/funclet_cascade_rank.py --calibrate   # measured rates
python3 scripts/harvest/funclet_cascade_rank.py --top 40 --json ranked.json
python3 scripts/harvest/funclet_cascade_rank.py --dump 0x82645320
```

Per target:

1. **Count funclets on both sides at their respective frames.** Equal counts ⇒
   pure frame problem, body work not required. Unequal ⇒ the source is missing
   or has surplus destructible temporaries; fix those first.
2. Recover retail's `static Symbol` block by walking `bl ??0Symbol@@QAA@PBD@Z`
   inside the parent and reading the string operand out of `.rdata` (see
   `--dump`, or the 30-line extractor in §5). Declaration order = ctor address
   order = guard-bit order.
3. Frame census rule (lane F): MSVC gives every `case`/branch body its own stack
   slots and never reuses them across arms, so the delta names the
   missing/surplus bodies. Corollary found here: a missing entry in a
   `BEGIN_HANDLERS` chain costs exactly one message temp of frame.
4. `__savegprlr_N`: more function-local statics ⇒ the shared guard word gets
   pinned in a callee-saved register ⇒ lower `N`.
5. **Always A/B whole-binary.** Two of the four targets above produced a correct
   local fix with a negative or zero global result. `rm -f
   build/45410914/report.cache` before every `report.json` read.

## 8. Where the remaining value is

* **~250 funclets** across the 96 named frame-mismatch parents — direct, this
  lever, per-target body-free work. Top of the ranked list after this session:
  `RockCentral::RecordPerformance` (79), `Game::Handle` (21),
  `BandCharacter::OnSetFileMerger` (17, Δ +0x10), `ProfileMgr::Handle` (14,
  frame *and* sgpr already exact — `PARENT_OFFUNIT`, so a different cause).
* **~2,300 funclets** behind unnamed parents (`fn_822ECC48` in VocalTrackDir
  owns 82; `fn_825DB930` in OvershellSlot owns 54; `fn_822EE768` 49;
  `fn_82501A08` 42). These need a `target_symbol_map.json` reveal entry before
  the frame is even readable — this is the single biggest funclet-side lane and
  it is an *identification* problem, not a codegen one.
* **26 parents live outside their funclets' pinned unit** (87 unmatched
  funclets) — a splits/pinning signal, surfaced explicitly rather than
  mis-attributed.
