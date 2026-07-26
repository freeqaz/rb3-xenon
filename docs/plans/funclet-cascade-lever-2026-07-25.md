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

---

# Session 2 (lane N, 2026-07-26) — `PARENT_OFFUNIT` is a **splits** defect, not a codegen one

Base main `d83ca54f` (27,629). Worktree `~/tmp/wt-laneN-frames`, branch
`laneN-frames`. **Result: +34 measured on top of the concurrent main
`fb55bbe7` (27,839 → 27,873), from two splits commits and zero source changes.**

## 9. The fourth frame/funclet defect shape: COMDAT scatter across a splits boundary

§8 above listed "26 parents live outside their funclets' pinned unit" as a
*signal*. It is better than that — it is a **directly harvestable lever with a
mechanical fix and no source edit at all**, and it is the cheapest shape found
so far.

The tell is unmistakable and the opposite of every other shape:

> **The parent's frame size AND `__savegprlr_N` are EXACT — several parents are
> even at 100% body — yet not one of their funclets matches.**

Nothing is wrong with our code. Retail's linker scattered the parent's COMDAT,
so the funclets' VAs land inside a *different* unit's pinned `.text` range. dtk
therefore carves them into that other unit's target obj, while our compiled obj
emits parent + funclets together in one COMDAT. The funclets have nothing to
pair against, in either unit.

`ProfileMgr::Handle` @`0x82549128` is the canonical case: parent in
`band3/meta_band/ProfileMgr.cpp`'s `.text 0x82549128-0x8254B070`, its 14
funclets at `0x8254B990..0x8254BBB8` sitting inside
`band3/meta_band/SaveLoadManager.cpp`'s `.text 0x8254B070-0x82553F28` and
emitted into `SaveLoadManager.obj`.

### The fix

Pure `splits.txt` surgery — split the source unit's range around the hole and
give the hole to the parent's unit, in address order:

```
band3/meta_band/SaveLoadManager.cpp:
-	.text       start:0x8254B070 end:0x82553F28
+	.text       start:0x8254B070 end:0x8254B990
+	.text       start:0x8254BBC0 end:0x82553F28

band3/meta_band/ProfileMgr.cpp:
 	.text       start:0x82549128 end:0x8254B070
+	.text       start:0x8254B990 end:0x8254BBC0
```

`touch config/45410914/config.yml`, `rm -f build/45410914/report.cache`, full
`./tools/ninja-locked`. **+14, LOST 0.**

### Rule: stop at the last funclet — do NOT absorb the trailing adjustor thunk

Retail follows the funclet run with the parent's adjustor/vcall thunk (here a
0xC block at `0x8254BBC0`: `lwz r11,-4(r4) ; subf r4,r11,r4 ; b fn_82549128`).
Including it in the move still gains the same 23 but costs
`?Handle@SaveLoadManager@@$4PPPPPPPM@A@AA...` — **net +22 instead of +23**. The
thunk belongs to the *source* unit's pairing. End the moved range at
`last_funclet_va + last_funclet_size`.

### Computing the range — read the LABEL, never the comment column

The dtk asm address *comment* column is synthetic for scatter-gathered units
(~20% real). Parse `.fn fn_<VA>` / `.obj <name>_<VA>` **labels** from the kid
unit's `.s`; the range is `[min(funclet label), next label after max(funclet
label))`. Reject any candidate whose computed `end <= start` — that means the
next label crossed another scatter boundary and the range needs the PE `.pdata`
size instead.

### Harvest (10 further moves, one build)

| parent | funclets | net | source unit |
|---|--:|--:|---|
| `SyncGameStartPanel::Handle` | 8 | **+8** | `band3/game/Player` |
| `FingerShape::FingerShape` | 3 | **+2** (+4/−2) | `GemTrackResourceManager` |
| `ClosetMgr::Handle` | 3 | **+3** | `band3/meta_band/LessonMgr` |
| `UIComponentScrollMsg` ctor | 2 | **+2** (+3/−1) | `band3/meta_band/NewAwardPanel` |
| `MicClientMapper` ctor | 1 | **+1** (+2/−1) | `MetaMusic` |
| `CharNeckTwist` ctor | 4 | **+2** | `PracticeSection` |
| `TourPerformerImpl::Handle` | 1 | **+1** | `TourProgress` |
| `LocalizeOrdinal` | 1 | **+1** | `Cache` |
| `MetaPerformer::Handle` | 6 | 0 | `band3/meta_band/SessionMgr` |
| `ObjDirItr<ThreeDSound>::operator++` | 1 | 0 | `band3/meta_band/CharCache` |

**+20 for the batch**, every move individually net non-negative.

Two of them stayed at 0 for an understood reason, and this is the useful
corollary: **a scatter move only pays if the parent's frame and sgpr are
already exact.** `MetaPerformer::Handle` still has sgpr 23 vs 24 and
`ObjDirItr<ThreeDSound>::operator++` still has frame 0x80 vs 0x60 — the move is
structurally correct and *pre-positions* them, but they need their own fix to
cash. So run the scatter pass **first**, then the frame/sgpr passes; the scatter
pass converts "invisible" funclets into addressable ones.

### The collateral is bookkeeping, not regression

Three moves cost 4 anonymous funclets in the source unit. Those funclets were
matching **positionally in the wrong unit** and are now counted in the unit that
actually owns them — this is the mirror image of the §5.1 naming trap
(objdiff pairs anonymous funclets positionally, so removing functions from the
middle of a target obj re-shuffles the rest). Every move is still individually
net-positive, but always whole-binary A/B and attribute gains *and* losses per
unit before landing a batch.

### Skips (still open, ~6 funclets)

* 4 parents whose bodies we do not compile at all (`base_frame` is `None`) —
  moving their ranges cannot pay until the body exists.
* 2 ranges where the next label crossed a scatter boundary (`end <= start`):
  `fn_827DC9E8` (JsonUtils ← TrackDir), `fn_827E5D88` (Dir ← MidiParser). Use
  the PE `.pdata` `FunctionLen` for the last funclet instead of the next label.
* `PhraseDB::PhraseDB` ← `BeatMatcher`: the hole is not contained in a single
  source `.text` range, so it needs two range edits rather than one split.

## 10. Re-pricing the caller-side-invert "346 near-miss answer key"

Regenerated end-to-end on a fresh build (`homing_scan_all.sh` → 914 TUs →
`caller_side_invert.py --iterate 6`). The `_nonbyte.json` came back at **249
rows — but that is one row per anchoring caller.** Deduped by `(name, VA)` it is
**83 unique functions**: 1 already strict, **13 mapped and open**, **69 unmapped**
(needing a reveal entry before objdiff can pair them at all). **55 of the 83 are
STL template-instantiation swarm members**, a vein measured at ~0/6 flips.

The real structure of the residue is a handful of template families, not 346
independent targets:

| family | n |
|---|--:|
| STL swarm | 55 |
| singletons | 13 |
| `ObjVector<T>::resize` | 9 |
| `ObjList<T>::operator=` | 3 |
| `NewNetMessage` | 2 |
| `ObjList<T>::resize` | 1 |

`ObjVector<T>::resize` at 9 instantiations from one template body is the only
force multiplier in the list. Triage written to `~/tmp/laneN_346_triage.{md,json}`.

## 11. `span_predictor`'s PAYS tier can be a **mispair-repoint** worklist

The same run proposed 62 RESOLVED homes; `span_predictor --only PAYS` passed 7.
**All 7 were name collisions** — the mangled name was already in
`target_symbol_map.json` but pointed at the *wrong* retail VA, showing as a
false low match (26.6 / 0.5 / 47.6 / 27.5 / 99.9 / 0.0 / 1.0%). Caller-side
derivation pins the true VA and guard 4 (hit-set containment) already proves
byte-identity there, so each repoint is a guaranteed flip: **+9 (7 repointed
names plus 2 bonus, as the freed old VAs reverted to anonymous `fn_` and paired
byte-identically), LOST 0**. A second iteration drained PAYS to 0 — the vein is
one-shot per scan.

**But it did not survive integration.** Main advanced concurrently and
`a380ed69` ("map: string-verified StaticClassName/Type family repair") repaired
the same family a different way; measured on top of `fb55bbe7` the repoints cost
**−15 net**, so they were reverted. Lesson: **map-repair work is
single-owner** — two lanes repairing `target_symbol_map.json` concurrently will
collide, because a repoint that is correct against one map state is wrong
against another. Splits work does not have this problem.

## 12. ★ The FIFTH shape: a **stubbed callee shrinks its caller's frame**

Found independently by two lane-N workers on the same target, which is about as
strong as corroboration gets. It closed `Game::Handle` (+21, the target §6
recorded as walled at −24).

### First, the two corrections to §6

1. **Retail's chain is `RemoteLeaderLeftMsg → ButtonDownMsg → ButtonUpMsg`,
   NOT `JoypadConnectionMsg`.** Verified from the PE: decode every `bl` in
   `Game::Handle` (`0x8267D480`, len `0x1534`) and resolve the targets through
   `scripts/target_symbol_map.json` — `?Type@ButtonDownMsg@@SA?AVSymbol@@XZ`
   (`0x82528878`) and `?Type@ButtonUpMsg@@SA?AVSymbol@@XZ` (`0x825288F8`) are
   both called; `?Type@JoypadConnectionMsg@@SA?AVSymbol@@XZ` (`0x825324A0`) is
   **not**. Inferring the message type from the `OnMsg` callee address is not
   reliable — resolve the `?Type@…Msg@@SA?AVSymbol@@XZ` call instead.
   (`ButtonDownMsg`/`ButtonUpMsg` here are **360-retail-only**: rb3-Wii's dev
   `Game.cpp` stops at `RemoteLeaderLeftMsg`, so the oracle cannot find this
   class of gap — it only comes from reading the target.)

2. **The −24 was never the `Game.h` declarations.** `MusicLibrary.cpp`
   scatter-includes `band3/game/Game.cpp`, so a *second* copy of `Game::Handle`
   is compiled into `MusicLibrary.obj`. Two extra `HANDLE_MESSAGE` blocks give
   that shadow copy two extra EH funclets, which shifts objdiff's **positional**
   pairing of MusicLibrary's anonymous funclets — exactly the 24. A decl-only
   probe measures DELTA 0, which disproves the declaration theory outright.
   Fix = a **primary-TU discriminator** guarding the new `HANDLE_MESSAGE`s, the
   same pattern as the `gRev`/`gAltRev` guard already at that include site, so
   the shadow copy stays at frame `0x120`. (An `#ifdef` *defined at the top of
   `Game.cpp`* does NOT work — the `#define` is inside the file being textually
   included, so the scatter copy sees it too.)

### The shape itself

Both handlers only moved the frame `0x120 → 0x130`; the last `0x10` was a
**caller-side consequence of the callee bodies**.

MSVC `/O1` does intra-TU interprocedural analysis of callees it has already
seen. `HANDLE_MESSAGE(X)` builds an `X(_msg)` temporary on the caller's stack
and passes its address to `OnMsg`. **If `OnMsg` never reads through the
reference, MSVC proves the temporary is dead the instant the call returns**,
pools its 8 bytes into a shared slot, and drops the inlined `~Message` vptr
reset and `mData` reload. Measured bisect on `Game::Handle`'s frame:

| `OnMsg` stub body | resulting frame |
|---|---|
| `return DATA_UNHANDLED;` | `0x130` |
| `if (TheGamePanel->IsGameOver()) return 0; …` (opaque call, no `msg` use) | `0x130` |
| `if (msg.GetUser()) …` (**reads `msg`**) | **`0x140`** ✅ |

Negative controls that did **not** move the frame: the definition's position in
the TU, a named local instead of the macro temporary, a non-`const` reference
parameter, and `volatile char pad[16]` — **MSVC drops unreferenced locals
whether or not they are `volatile`, so there is no frame-filler escape hatch.**

**Generalisation:** any caller whose callees we have stubbed can have a frame
*smaller* than retail's purely because of the stub, making every one of its
funclets unmatchable. Triage a `−8×k` frame delta as "stub-shrunk frame" before
starting body archaeology; the minimum fix is a stub body that dereferences the
parameter. Retail's real `0x8267B808` / `0x82679900` both open with
`msg.GetUser()`, so that placeholder is faithful in shape as well as in effect.

### Corollary: compute the required locals delta before touching source

From `RockCentral::RecordPerformance`: for a `__savegprlr_14` `/EHsc` parent,

> **frame = align16(locals_end + 0x98)**

Retail locals_end `0x5E8` → `0x680` exactly; ours `0x5D0` → align16(`0x668`) =
`0x670`. So an observed Δ`−0x10` can be produced by anything from `+0x08` to
`+0x18` of locals — the align16 quantises it. Derive the locals delta from the
frame delta first; hunting for "a single 16-byte temp" can be looking for
something that does not exist. (`RecordPerformance`'s real gap was 13 vs 10
eight-byte temps — `make_pair` sources and `insert()` sret `pair<iterator,bool>`
slots — not one object.)

### Anti-pattern retired

`SongParser::ParseText` (`0x827840C8`) looks like a §2 local-static job (frame
exact, sgpr 27 vs 28) but is not: its 4 funclets are
`lwz r3, 0x54(r31) ; bl <String dtor>` — retail builds a **`String` temp** from
the parsed `[...]` text, and our source creates no destructible temp at all (we
emit zero funclets at frame `0xF0`). The sgpr delta is plain register pressure.
**A sgpr mismatch is only a local-static signal when the funclet count matches;
if our funclet count is zero, it is a body-port job.**

---

# Session 2 harvest table + the shapes it added (lane N, 2026-07-26)

**Base main `fb55bbe7` (27,839) → `28,085`, net +246 (253 gained, 7 lost),
across 9 subagents in 9 isolated worktrees.**

| stream | net | headline |
|---|--:|---|
| map: purge stale `__unwind$N` entries | **+124** | §13 |
| COMDAT-scatter splits (11 moves) | +35 | §9 |
| negative-delta batch (6 targets) | +33 | §14 — `MILO_WARN` copies its args |
| `Game::Handle` | +21 | §12 — stub-shrunk frame |
| `DataArray` mutators | +12 | §15 — scatter-include inlining collapse |
| positive-delta batch 2 | +11 | §16 |
| positive-delta batch 1 | +6 | §17 |
| `BinStreamRev` base@0 re-drain | +4 | §18 |

## 13. ★★ `__unwind$N` map entries are pure poison — the largest single win

`scripts/target_symbol_map.json` held **235** entries of the form
`"0x…": "__unwind$<N>"`. MSVC's `__unwind$N` counter is **TU-global and shifts
on every source edit**, so these names are stale the moment anything in the TU
changes — and **zero** `__unwind$`-named symbols appear anywhere in
`report.json` (checked across all 69,180 entries). They can never bank.

They are also actively destructive, for exactly the §5.1 reason: objdiff pairs
EH funclets **positionally while anonymous**. Renaming a target funclet to a
name our object does not emit kills that pairing *and* re-shuffles every funclet
after it in the unit.

**Purge = +124 (125 gained, 1 lost), map-only, no source change.** One line:

```python
re.sub(r'\n\s*"0x[0-9a-fA-F]+"\s*:\s*"__unwind\$\d+",', '', src)
```

Generalise the rule: **never map a name whose identity is a compiler-generated
ordinal.** Audit the map for other ordinal-shaped names before the next wave.

## 14. ★ `MILO_WARN` still EVALUATES its arguments in retail (negative-delta mirror)

Our `#define MILO_WARN(...) ((void)(__VA_ARGS__))` evaluates *nothing* — a class
lvalue as a discarded comma operand emits no code. **Retail strips only the
emission.** Every by-value class vararg (a `String`) is still copy-constructed
into its **own 0x10 frame slot** and destroyed, and sometimes the `MakeString`
call itself survives.

This is the exact mirror of the positive-delta "stripped debug branch / phantom
0x800" shape and it hit **2 of 6** targets in one batch (`PrefabMgr` 3 slots,
`SongParser::EndVocalNote` 2 slots + 2 callee-saved regs = −0x30).

Signature in objdiff: a run of pure `delete`s of
`addi rN, r31, <slot>` / `bl ??0String@@QAA@ABV0@@Z` / `bl ??1String@@UAA@XZ`
around a warn site. **Fix per call site** (named local, `String(x)` prvalue, or
`(void)MakeString(...)`) — no `Debug.h` change is needed, which keeps the blast
radius at one TU. A `Debug.h` vararg-sink form (`((void)Sink(__VA_ARGS__))`)
would do it fleet-wide but sits in the PCH and needs its own isolated full A/B.

**This is scannable**: negative-delta parents whose source contains `MILO_WARN`
with a `String`/class argument.

Related negative-delta cause found alongside it: **a surplus arm that costs a
callee-saved register rather than a stack slot** (`PrefabMgr`'s fourth
`strcmp(substrname,"BBE")` branch — retail has 3 strcmp loops and 5 hoisted
literals, we had 4/6, worth `_15` vs `_16`). Counting the target's hoisted string
literals and inline `strcmp` loops is a 30-second census for it.

## 15. ★★ Scatter-include inlining-policy collapse (force multiplier: 234 sites)

`src/system/obj/DataNode.cpp` scatter-includes `obj/DataArray.cpp`. That puts
`DataNode::DataNode(const DataNode&)`'s **body** in scope for DataArray's
`new (&mNodes[i]) DataNode(x)` loops, and two things follow that retail cannot do
(it built `DataArray.cpp` as its own TU, where the ctor is opaque):

1. `/Ob2` **inlines** the ctor (10 instructions replace the `bl`).
2. The now-local ctor is provably nothrow, so MSVC **deletes the
   placement-`operator delete(void*,void*)` EH cleanup funclets entirely** —
   along with the EH-state spills that make up the frame delta.

Retail's funclets here are `f(p, p)` calls into an ICF-folded `blr`; **that empty
function is `operator delete(void*, void*)` and it is the fingerprint of this
shape.**

Fix: host the shared ctor in a TU that copy-constructs none of the type
(`src/system/obj/DataUtl.cpp`). `__declspec(noinline)` alone is NOT enough — it
stops the inlining but not the nothrow deduction, so the funclets stay deleted
(measured: `Insert` reached 61.7%, not 100).

Result: `DataArray::Insert` 31.7→100, `InsertNodes` 35.8→100, `Resize` 71.2→100,
`Remove` 43.6→100, **+9 funclets**; net +12 (the −1 is `gDataVars`'
`map<Symbol,DataNode>::operator[]` in DataNode's own TU, where retail *does*
inline the ctor — the two behaviours cannot coexist in one TU).

Also settled: **`sizeof(DataNode)` and the `DataArray` element stride are
CORRECT** (both sides `slwi r,i,3`). Do not chase the stride hypothesis.

**The mechanism is generic to every scatter-include in the tree — there are 234
(`grep -rn '#include ".*\.cpp"' src/`).** For each host `H.cpp` including guest
`G.cpp`: any non-inline function defined in `H.cpp` that `G.cpp` calls is
silently `/Ob2`-inlined *and* has its callers' EH cleanup funclets deleted.
Diagnostic is cheap: objdiff's Function Call Diff showing a base-only
`??0…`/method symbol that is target-only as an `fn_`, plus a negative frame
delta with retail saving *more* registers. **Highest-yield follow-up from this
lane — build the scanner.**

## 16. Shapes from the positive-delta batch

* **★ A local `static Symbol` declared too HIGH in the function.** Distinct from
  §2 (which is about the *count* of statics) — this is about *position*.
  Declaring it at top-of-function instead of at its use point forces a float
  argument to survive across the `Symbol` ctor calls → an **`f31` save** (+0x10
  frame) *and* one lower `__savegprlr_N`. Moving the declarations down fixed
  frame, sgpr and body in a single edit (`StreakTracker::LocalEndStreak`,
  26% → **100.0%** + 3 funclets). The pattern `static Symbol …;` as the first
  statement of a function is common in our tree — **worth a scanner.**
* **Static-declaration order is recoverable from `.rdata` string spacing.**
  Normalized diff hides which retail string is which (both sides render as
  `lbl_…` vs `??_C@…`), so an unresolvable r27↔r28 swap can look like regalloc.
  Compute `addr + strlen + 1` rounded to 4 to identify which literal is longer,
  and the declaration order falls out.
* **A "surplus" that is a redundant explicit init loop.**
  `Symbol hints[4]; for (i<4) hints[i] = Symbol();` emits 8 stores where retail
  emits 4 — the array default-ctor already did it.
* **Slot-reuse scoping** (`Synth360::PreInit`): retail declares
  `effectDescs[2]`/`chain` in ONE nested block and **reuses both objects** for
  the reverb submix; we gave the reverb its own copies. Frame 0x1B0→0x1A0.
* **A symbol-map MISPAIR masquerading as a frame defect** (`BandSongMgr::Handle`,
  `EaseElasticOutIn` — the latter's retail body is a *constructor*, not an easing
  curve). Cheap pre-filter for the ranker: if the target's call set is
  categorically incompatible with the mapped name (a `Handle` with no
  `BEGIN_HANDLERS` `Symbol` ctors), flag `MISPAIR` instead of ranking it as
  frame work.

## 17. `__savegprlr_N` is NOT causal for funclets — de-fund that bucket

Proved by attribution: `MetaPerformer::Handle`'s 6 funclets flip with the sgpr
source fix **reverted** (they were blocked by §13's stale map names). A funclet
body encodes only the parent's **frame immediate** plus its own dtor target and
slot; it never touches `__savegprlr`. The 77.1% match rate in the
"frame ok, sgpr MISMATCH" bucket is **correlation, not a lever** — treat sgpr as
a *diagnostic hint* about what the source is doing, never as the thing to fix.

Sharper mechanism where local statics *do* move sgpr: it is not "more statics ⇒
lower N", it is that a **duplicated string literal gets its address hoisted into
a callee-saved register** (retail loads `"has_online_scoring"` once into r27 and
does `mr r4, r27` at both ctor sites). Other observed causes: retail burning r31
as the EH frame pointer *because* it has funclets and we do not; and us holding
floats in `f30/f31` that retail rematerialises.

**★ Related new shape — a global pinned in a callee-saved register.** A negative
sgpr delta (we save more) *combined with* a positive frame delta often means we
**inlined an expression that references a global/static across calls** where
retail had it behind an out-of-line accessor. `Player::LocalSetEnabledState`:
our inlined `GetSongMs() / TheSongDB->GetSongDurationMs()` pinned `&TheSongDB`
in r28 for the whole function; adding `Player::GetSongPct()` fixed frame *and*
sgpr together (+6 funclets, 87.3 → 99.8%).

## 18. Walls confirmed — do not re-hunt

* `RndGenerator::RndGenerator` and `RndParticleSys::RndParticleSys`: both are the
  **`ObjPtr` two-ctor inline wall** wearing a positive-delta disguise (retail
  inlines the 1-arg `ObjPtr(owner)` ctor, so `0.0f` lives in volatile `f0`; our
  out-of-line call clobbers it and forces a hoist into `f31`). Already
  **CLOSED at-limit** in `docs/decomp/research/2026-07-10-objptr-two-ctor-inline.md`.
  **Tell:** when a positive frame delta is exactly one FPR/GPR save and the
  register swaps are `f0↔f31`-shaped around constructor calls, it is the
  inline-policy wall, not a stripped debug branch. Check this *before* spending
  attempts. (`RndParticleSys`'s apparent member-offset deltas are index
  misalignment from scheduling inserts — the `addi rX, r30, <off>` sequences are
  identical on both sides. Not struct drift.)
* `rndobj/Utl.cpp` (`TessellateMesh`, `BuildVisit`, `ResetNormals`): no
  scatter-include, no debug arm — genuine large FP+STL body divergence, frame
  delta is a *consequence*. `diff_inspect --stack-layout` is unavailable on this
  unit (the `/Z7` recompile fails). Their target-only/base-only call lists are
  mostly unmapped-`fn_` noise; do not read those as real differences.
* `VocalPlayer::Poll` / `HandlePhraseEnd`: body ports (887 and 447+155
  insert/delete), not frame work. **Not** native-port leakage — commit
  `08c77f11`'s hunk is properly `#ifdef HX_NATIVE`-gated and the X360
  preprocessed output is byte-for-byte unchanged; the divergence predates M10.
* `GemTrainerLoopPanel` ctor: the sole delta is the MSVC **vtordisp** init for
  the `Hmx::Object` virtual base — retail constant-folds `vbtable[1]-0x3c` to a
  literal `0`, we emit `subi r10,r11,0x3c`. Same value; compiler state, not
  source (retail itself emits the non-folded form elsewhere). at_limit.
* `BandDirector::OnFileLoaded`: we save `f30/f31`, retail does not. Its 11 "open"
  funclets are additionally **positionally mispaired against other parents'**
  (they show `subi r31,r12,0x230` against our real 0x240).

## 19. Re-pricing: `caller_side_invert`'s `_nonbyte` list is a **mispair detector**

§10 already cut "346" to 83 unique functions. Working them cut it further, to
**zero**:

* All **69 "UNMAPPED, needs a reveal"** entries are a mirage — our build emits
  **none** of those 69 symbols, so a reveal creates a target-only 0% pairing that
  can never bank. 51 of the 69 VAs are already occupied by a different name, so
  they are repoint questions, not inserts. **Pure-insert count: 0.**
* Of the 13 MAPPED-OPEN: 2 out of scope, and **5 more proven mispairs** by a
  single immediate each — `_List_base<Anim>::clear` (`li r3,0x24` ⇒ element 0x1c
  = `ProxyCall`, not `Anim`'s 0x34); `vector<PropertyFilter>::push_back`
  (`addi 0x30` vs provable 0x14); `list<ProxyCall>::operator=` (target inlines
  two adjacent `ObjOwnerPtr` `SetOwnerObj`s, which `ProxyCall` has not);
  `__inplace_stable_sort` (target takes 5 args and *is* `__stable_sort_adaptive`).
* **`ObjVector<T>::resize` needs nothing** — the template at `src/system/obj/Object.h:1862`
  is already correct and **20+ instantiations are already strict 100%**. Both
  "open" ones are map mispairs. There is no ripple to be had.

**Flip rate on the assigned list: 0/9.** Re-purpose the tool: for each row,
compare the target's element-size immediates and argument counts against our
struct sizes — it pinpoints wrong map names with one immediate. **Volume lives
elsewhere: 661 named non-STL functions sit in the 78–100% band binary-wide.**

## 20. Tool bug

`funclet_cascade_rank.py` reads the parent's **`stwu` immediate** as
`base_frame`. The funclet actually encodes the **`subi r31, r1, N`** immediate.
These are normally equal but need not be — trust the `subi`, and cross-check
against the funclet's own `subi r31, r12, N`.

---

# Session 3 (lane R, 2026-07-26) — the identified-body worklist

Base main `e7662cdb` (28,238). Lane branch `laneR-frames`.

## 21. The `callee_set_join` WORKLIST is a **reveal** lever, and it pays a little

`callee_set_join.py --propose` over a fresh `funclet_cascade_rank --json` emitted
**75 WORKLIST** targets (identified retail VA, body diverges) and **0 BANKABLE**.
The tool's own docstring is right that naming a frame-mismatched parent banks
nothing *causally* — but "BANKABLE" is computed from a conservative
reloc-masked byte-identity test, and inserting all 75 map entries measured
**+4 strict, 0 lost** (28,238 → 28,242). So:

> **Insert the WORKLIST entries anyway.** BANKABLE is a lower bound, not the
> answer. The entries cost one map edit and one build.

The far bigger return is that the reveal **turns 75 unpaired `fn_` targets into
scored near-misses you can actually diff**. Post-reveal distribution:

| band | count |
|---|--:|
| flipped outright (100%) | 4 |
| 90–100% | 49 |
| 70–90% | 16 |
| 40–70% | 6 |

Before the reveal every one of these read as an anonymous `fn_` with no diff at
all. **This is how the identification tooling still pays after byte-identity
homing has drained: not as strict delta, but as worklist generation.**

Re-run it as the map grows — 238 of 1,809 funclet parents still have zero mapped
callees, so the join's candidate pool grows every time a body flips.

### The worklist's shape census (use this to route the next wave)

**18 of 75 (24%) are `SyncProperty` / `PropSync<T>`**, nearly all carrying a
positive size surplus clustered at **+32/+36/+40/+44/+48/+52** bytes, with **+44
recurring across five unrelated classes** (`Waypoint`, `CharFaceServo`,
`WorldCrowd`, `PropSync<ActionElement>`, `PropSync<String>`). A repeated
constant surplus across unrelated TUs is the signature of a **shared macro or
template cause**, not 18 independent body divergences — route the whole cluster
to one worker, not one worker per class.

Next largest shapes: 7 `Handle` methods, 2 `op>>(BinStream&)`.

## 22. ★ The §14 `MILO_WARN` vein is **DRAINED** — do not fund a sweep

§14 called the shape "scannable". It was scanned. **The census is 1.**

Method (reproducible): 437 `MILO_WARN(` call sites tree-wide outside
`src/xdk/` and `src/system/stlport/` → 94 with a plausibly class-typed argument
→ after resolving the *declared type* of every bare-identifier argument, exactly
**one** unfixed site remains (`src/band3/meta_band/AssetMgr.cpp:219`, arg `s38`),
and its enclosing function `AssetMgr::VerifyAssets` **is not in `report.json` at
all**, so fixing it banks nothing.

Two corrections that make the difference, both load-bearing:

1. **`String(x)` at a call site is the FIXED form, not the broken one.**
   Scanning for `MILO_WARN(..., String(...))` finds sites lane N *already
   repaired* — `PrefabMgr.cpp:85` and `SongParser.cpp:1072` are exactly the two
   §14 harvest targets, and they show up because of the fix. The **broken**
   signature is a bare **`String`-typed lvalue** argument, which as a discarded
   comma operand emits no copy at all.
2. **`Symbol` arguments do NOT qualify.** `Symbol` is a single `const char*`
   (`src/system/utl/Symbol.h:11-13`) — 4 bytes, register-passed, no `0x10`
   stack slot, so it cannot produce the shape. The great majority of the 94
   plausible sites are `Symbol` or `.Str()` / `PathName()` (`const char*`)
   arguments and are noise. Filtering by *argument spelling* rather than
   *declared type* over-counts this vein by roughly 90×.

**Corollary: the fleet-wide `Debug.h` vararg-sink form is not worth its isolated
whole-binary A/B.** There is nothing left for it to fix. §14 stays valuable as a
*diagnostic* for an individual negative-delta function; it is not a wave.

## 23. Session 3 harvest — shapes the identified-body worklist produced

**`laneR-frames`: 28,238 → 28,313, net +75, 0 lost (unit-agnostic), 6 subagents.**

| stream | net | headline |
|---|--:|---|
| map: `callee_set_join` WORKLIST reveal | +4 | §21 |
| `SyncProperty` superclass-chain (97 classes) | **+25** | §23.1 — one cause, 100 files |
| `laneR-fc1` local-static `Symbol` ×2 | +24 | §23.2 |
| `laneR-fc1` `BandCamShot::StartAnim` | +9 | §23.3, §23.4 |
| `laneR-fc2` ctor-temp rematerialization | +9 | §23.5 |
| `laneR-tail` `NextSongPanel` pad placement | +1 | §23.6 |

The merged total (+75) **exceeds the sum of the independently-measured parts
(+72)**: the streams reinforce rather than interfere, because several share
parents whose funclets only pair once every arm in the unit is right. Always
re-measure the merge; do not report the sum.

### 23.1 ★★ `SyncProperty` chains: retail stops at the immediate superclass
The single biggest win of the session, and a clean instance of the recurring
**"retail X360 PREDATES the newer oracle"** pattern — this time against **DC3**,
not rb3-Wii. DC3 (newer engine) added a direct `SYNC_SUPERCLASS(Hmx::Object)`
tail to its `SyncProperty` chains; RB3-360 retail has none, and neither does the
rb3-Wii oracle for any of the 97 affected classes. Our `src/system/` tree
inherited DC3's version.

Cost per affected function: an extra **8–13 instruction virtual-base-adjusted
call**, *plus* — because that call needs one more callee-saved GPR — **a
register-allocation cascade through the entire function body**. That is why the
surplus presented as a *constant* (+32…+52 bytes) across unrelated classes while
the match percentages varied wildly (69%–99%).

**Diagnostic:** a base-only `bl ?SyncProperty@Object@Hmx@@` in objdiff's Function
Call Diff plus a **pure-insert tail cluster of exactly the surplus size**.
Guard removals with `#ifdef HX_NATIVE` to preserve native-port behaviour.
A second, smaller wave (`Spotlight`/`UIComponent` → trailing `RndPollable`,
`CharLipSyncDriver` → `CharPollable`, `TexMovie` → mid-chain `Hmx::Object`) was
strict-neutral but moved `Spotlight::SyncProperty` 98.89 → 99.83 fuzzy.

> **Generalise:** a *constant* size surplus repeated across unrelated classes is
> a **shared macro/chain** defect, never N independent body divergences. Route
> the whole cluster to one worker. Audit the remaining `SYNC_SUPERCLASS` and
> `LOAD_SUPERCLASS` chains against rb3-Wii the same way.

### 23.2 The §16 local-static `Symbol` lever generalises to the game layer
§16 found it once. It closed **two more** here, both at 88–90% → 100.0 with all
their funclets (`RGTrainerPanel::UpdateStepText` +11 funclets,
`PlayerGameplayMsg::Dispatch` +10). Retail declares the symbols as
**function-local `static Symbol`** rather than using the `Symbols*.h` globals.

Tells, all readable without guessing:
* an extra `??0Symbol@@QAA@PBD@Z` call per static (3 vs 1);
* the **guard bit index**: `rlwinm. 0,29,29` (bit 4) vs our `clrlwi. 31` (bit 1)
  — the bit number tells you how many statics precede it in the function;
* the token fetched as `lwz r4, 0(rN)` from static storage instead of
  `lis/lwz ?name@@3VSymbol@@A`;
* `.rdata` string spacing to recover declaration order (§16).

Each extra pinned storage pointer plus the hoisted guard base costs one
callee-saved register, so **one edit fixes body, `__savegprlr_N` and frame
together**. Note this is *safe under scatter-includes*: a local static adds no EH
state, so shadow copies' funclet counts are unchanged and positional pairing
holds (`NetGameMsgs.cpp` is scatter-included by `CampaignLevel.cpp` and
`MeshAnim.cpp`; the fix even collected a funclet in a shadow copy).

### 23.3 ★ Dev-only surplus at WHOLE-BLOCK scale — read the target's epilogue
`BandCamShot::StartAnim`: retail's body simply **ends** at
`mCachedTotalDuration = GetTotalDuration()`. The rb3-Wii DEV build additionally
has a `Character *chars[32]` scratch array, a `numChars`/`MILO_ASSERT`, a
`DoHide()` call and an `sHideAllCharactersHack` tail — together the entire
+0xA0 frame delta and a 39-instruction pure-insert tail cluster.

The method that found it is the transferable part: **read the target's epilogue
and ask where retail stops**, rather than trying to align clusters from the top.
Cluster-shape triage finds *small* stripped arms; whole-block strips are easier
to see from the end of the function.

### 23.4 Trailing bitfields can be TWO allocation units, not one
Same target's `Target` struct: `int mForceLod : 3` occupies a word at `0x5c`
(`srawi r,r,29`), then six `bool : 1` flags share a **1-byte** unit at `0x60`
(`lbz` + `clrrwi. 7` / `extrwi 1,26` / `extrwi 1,29`, masks 0x80/0x20/0x04).
We had packed all seven as `int` in one word, which forced bool-normalising
`subic`/`subfe`/`cntlzw` sequences retail never emits.
**Tell: `lbz` on a bitfield word ⇒ a separate `bool :1` allocation unit.**

### 23.5 ★ NEW shape — ctor `this`-in-r3 suppresses temp rematerialization
MSVC reuses a constructor's `this` return value in `r3` as the temporary's
address. So a **prvalue temporary passed as a reference argument emits NO
`addi rX, r31, <slot>` rematerialization** — and the compiler is then free to
hoist the *destination* address computation **above** the ctor call, pinning it
in an extra callee-saved register (+0x10 frame, `__savegprlr_22` vs `_23`).

Retail instead declares a **named local in a nested block**:
```cpp
{ DataNode n(arr2, kDataArray); arr->Node(i5++) = n; }
```
which (a) re-materializes the address for each use and (b) restores
RHS-before-LHS evaluation order. **The nested block is load-bearing** — a plain
named local moves the dtor past the following statement (measured on
`BandCharDesc::Init`: a plain local left 2 replaces; the extra scope closed it).
Worth +1 parent +7 funclets on `EntityUploader::ReturnProfileResults`.

### 23.6 Struct padding: derive the pad's POSITION from target offsets
`NextSongPanel`'s known +4 pad was in the wrong place. The target reads
`mDetailsPageSize` at `0x5c` and `mDetailsHeight[]` at `0x68` while `unk70[]`
stays at `0x78` — so the pad sits **between `mDetailCounts` and
`mDetailsPageSize`**, not after `mDetailsHeight`. A pad of the right size in the
wrong slot still reads as body divergence; always solve for position from two
independent member offsets, not one.

### 23.7 Access-specifier over-restriction forces accessor-shaped codegen (OPEN)
Unmeasured but concrete. `GemManager::GemManager` (99.7%, 4 mismatched
instructions) differs only in
`mTrackDir->Find<RndDir>("chord_shape_outline",true)->LocalXfm().v.y`: retail
folds the whole chain into one displacement (`mr r11,r3` + `lfs …,0xFC(r11)`),
while going through the `LocalXfm()` **reference-returning accessor**
materializes the sub-object address (`addi r11,r3,0xd4` + `lfs …,0x28(r11)`).
`RndTransformable::mLocalXfm` is **`public` in the rb3-Wii oracle**
(`Trans.h:187`) but `protected` in ours (`Trans.h:188`), which forces the
accessor at every external call site.

Splitting the access specifier **in place** (declaration order unchanged, so
layout is untouched) is the candidate fix. It was not landed here — the full
`Trans.h` cascade competes with concurrent workers for build capacity and the
immediate payoff is ~1 function — but the *class* of defect (our headers more
restrictive than the oracle ⇒ accessor-shaped codegen that cannot match) is
worth a scan of its own: grep for reference-returning accessors whose member the
oracle exposes directly.

### 23.8 Final session-3 tally and the number to price the next wave with

**`laneR-frames`: 28,238 → 28,329, net +91, 0 lost (unit-agnostic), 6 subagents.**
Supersedes the +75 table in §23; a second `laneR-fc2` wave added +16.

**Measured flip rate of the 75-target worklist: 33.3% (25/75).** That is the
honest price for a `callee_set_join` WORKLIST wave — roughly one flip per three
targets, on a pool whose identity is already established. Note it was achieved
with only 4 of 6 planned fix streams completing (the engine stream never got a
concurrency slot), so 33% is a floor, not a ceiling.

The second `fc2` wave is worth reading as confirmation that **§23.2's
local-static lever is the densest single shape in this pool** — it closed four
more parents on its own (`TrackerDisplay::SetPercentageProgress` 68.8 → 100 with
3 funclets, `SetTimeProgress` 65.7 → 100, `DialogEvent::OnActivate` 75.1 → 100
with 2 funclets, `MidiParserMgr::OnText` 97.4 → 100), and in every case the
storages, shared guard word, guard-bit order and `.rdata` strings were recovered
directly from the PE. Two refinements worth keeping:

* **Declaration order == ctor address order == guard-bit order.** Once you have
  the guard word you can read the whole static block's source order off the
  binary without guessing.
* **A local `static Message`** (`DialogEvent::OnActivate`:
  `static Message init_msg(Symbol("init"))`) behaves the same way as a local
  `static Symbol` — retail uses it in preference to the `Messages*.h` global.
  Extend the §23.2 scan to `Message` as well as `Symbol`.
* **An existing helper can be the last 4 instructions.** `SetTimeProgress`'s
  residual was retail calling `MsToMinutesSeconds(ms, min, sec)` where we
  open-coded `totalsecs / 60, totalsecs % 60` in the argument list. The helper
  inlines to *identical arithmetic* but a different **store schedule** (both
  divides issue before either `DataNode` store). When a diff is a handful of
  reordered stores around arithmetic, look for a helper retail called rather
  than assuming scheduling noise.

## 24. ★★ Correction to the routing rule: positive delta + whole-function regswap is usually ONE defect

This overturns a triage habit that has been costing the project real matches.

The standing rule — *"regswap-only ⇒ at_limit, stop"* (§ throughout) — is only valid
once the **size delta is already zero**. On `RndCubeTex::SyncProperty` the extra
`SYNC_SUPERCLASS(Hmx::Object)` tail call is virtual-base-adjusted and therefore
needs **one more callee-saved GPR** (`__savegprlr_24` vs retail's `_26`), which
**renumbers the entire function's allocation** — presenting as a full-function
`r24↔r26 / r25↔r27 / r26↔r28` swap. The regswap was a *downstream symptom* of the
surplus, not an independent wall.

> **Rule:** never triage a function as "regswap ⇒ at_limit" while it still has a
> non-zero size delta. Resolve the surplus first, then re-read the swaps. A
> whole-function swap accompanying a positive delta is evidence of **one**
> defect — an extra call that consumed a register — not two.

This also explains why the §23.1 cluster's deltas varied (+32…+52) while all
sharing one cause: different vbase-adjust costs per class.

## 25. The sibling `*_SUPERCLASS` chains are the same vein, and they are UNTOUCHED

§23.1 swept `SYNC_SUPERCLASS` and banked +29. The identical DC3-accretion
question applies to every other superclass-chaining macro, none of which has been
audited against rb3-Wii:

| macro | call sites in `src/system` + `src/band3` | status |
|---|--:|---|
| `SYNC_SUPERCLASS` | 431 | swept (§23.1), +29 |
| `HANDLE_SUPERCLASS` | **611** | **untouched** |
| `COPY_SUPERCLASS` | **385** | **untouched** |
| `SAVE_SUPERCLASS` | **353** | **untouched** |

**1,349 unaudited sites against 431 that yielded +29.** Method is mechanical and
already proven: diff each class's chain list against
`/home/free/code/milohax/rb3/src`, confirm on the target asm (base-only
`bl ?<Method>@Object@Hmx@@` plus a pure-insert tail cluster of exactly the
surplus size), guard removals with `#ifdef HX_NATIVE`. **This is the
highest-leverage known follow-up in the lane.**

Two cautions carried over from the sweep:
* **Expect genuine exceptions and verify per class.** `CharBonesObject`
  demonstrably *does* tail-chain (8 target-only instructions + a bool-normalised
  return); its chain was restored unguarded. A blanket removal would have been a
  regression.
* **`Cam.cpp` and `CameraShot.cpp` see `obj/Object.h`'s older macro set, not
  `ObjMacros.h`** — `SYNC_PROP_MODIFY_ALT` / `SYNC_PROP_BITFIELD_STATIC` do not
  compile there.

### 25.1 Second shared cause found alongside it
DC3 also added an `idx >= vec.size() + (op == kPropInsert)` bounds check to the
`std::vector<T>` / `std::list<T>` `PropSync` templates — an 11-instruction
`divw`/`cntlzw`/`cmplw`/`bge` block (**+44**, the recurring constant from §23.1's
census) emitted in *every* instantiation, absent from both retail and rb3-Wii.
Template-level accretion multiplies across instantiations; audit the other
`PropSync`/`PropSyncPtr` templates the same way.

### 25.2 Walls recorded (do not re-grind)
* `Spotlight::SyncProperty` 99.83 — regswap-only *with the delta already
  resolved*, i.e. a legitimate at_limit under the §24 rule (arg-save order of
  `_val` vs `_i`).
* `PropSync<SpotlightEntry>` 97.72 / `PropSync<CharData>` 98.79 — `i++` in-place
  vs `i+1` fresh-temp regalloc, **template-level**. The obvious source rewrite
  *regressed* it (97.72 → 91.8). at_limit for hand editing.
* `CamShot::SyncProperty` 95.56 — genuine body divergence; our source is DC3's
  (`mHideList`/`mShowList`/`mCrowdStateOverride`) where rb3-Wii has bool-bitfield
  temps + `SYNC_PROP_BITFIELD_STATIC`. Needs a real body port, not a chain fix.
* `RndCam::SyncProperty` 75.11 — **inline-policy divergence**: retail calls
  `RndCam::SetFrustum` out-of-line (`bl` with 4 float args + `this-0x330`);
  `/Ob2` inlines it for us (~17 instructions × 3 arms). The TU already has an
  `_outline_SetFrustum` noinline-wrapper convention to reuse.

### 25.3 Two real source bugs the sweep exposed
Worth noting because both were *behavioural*, not cosmetic — this kind of sweep
doubles as a correctness audit:
1. `RndParticleSys::SetGrowRatio` clamped against `mGrowRatio` instead of
   `mShrinkRatio` (target: load `this-0x60`, store `this-0x64`; rb3-Wii confirms
   `f <= mShrinkRatio`).
2. `CharBonesSamples::SyncProperty` tested `_op == kPropSize` (0x10) where retail
   tests `0x40`, matching the `SYNC_PROP_SET` macro's own early-out.

## 26. FINAL session-3 tally — supersedes §23 and §23.8

**`laneR-frames`: 28,238 → 28,342. Net +104 (106 gained, 2 lost). 6 subagents.**

The 2 losses are honest and understood: `fn_825BC4CC` / `fn_825BC4F4`, two 40-byte
anonymous EH funclets in `default/band3/meta_band/MetaNetMsgs`, which went
100.0 → 99.9 when `BandEventPreviewMsg::Dispatch`'s arm count changed and shifted
objdiff's **positional** pairing of the unit's anonymous funclets (the §5.1 /
§12 mechanism). They are not a code regression — both still compile, and the
parent they hang off flipped 79.6 → 100. Accepted against 106 gained.

**Measured flip rate of the 75-target `callee_set_join` WORKLIST: 48.0%
(36/75).** That is the number to price the next wave with. It is a *floor*: only
4 of 6 planned fix streams got concurrency slots (the `system/` engine stream
never launched), and 46 targets were still open when the lane closed, 12 of them
above 97%.

### Where the +104 came from

| stream | net | dominant shape |
|---|--:|---|
| `SyncProperty` superclass-chain, 97+4 classes | +29 | §23.1 / §25 DC3 chain accretion |
| `laneR-fc1` (3 parents) | +33 | §23.2 local-static `Symbol`, §23.3 whole-block strip |
| `laneR-fc2` (6 parents) | +25 | §23.2 local-static, §23.5 ctor-temp |
| `laneR-tail` (7 near-flips) | +7 | §26.1 below |
| `laneR-game` (5 parents) | +6 | §23.2 local-static, retail-only ctor tails |
| map reveal waves | +4 | §21 |

### 26.1 The lane's dominant shape, stated plainly

**The single most productive shape of the session was the function-local
`static Symbol` / `static Message` (§23.2/§16) — it closed 11 of the 36 flips
across three independent workers, with a reported 3/3 hit rate on negative-delta
game targets.** Retail declares the token as a function-local static; our
rb3-Wii port uses the `Symbols*.h` / `Messages*.h` global.

Consolidated tell-set (all target-derivable, no oracle needed):
* `??0Symbol@@QAA@PBD@Z` **call count target > base**;
* an ~11-instruction delete cluster holding an extra **static-guard bit test**,
  and **all downstream guard `ori` immediates shifted left by one bit**;
* a **−0x10 frame with 1–2 extra `__savegprlr` registers** (each pinned storage
  pointer plus the hoisted guard base costs a callee-saved register);
* **declaration order == ctor address order == guard-bit order**, and storages sit
  descending from the guard word — so the whole block's source order is readable
  off the binary;
* the literal itself is at the `lis`/`addi` pair feeding the `Symbol` ctor.

**Worth a scanner** (§23.2 already asked for one): negative-delta parents where
the `??0Symbol@@QAA@PBD@Z` count differs. With 46 worklist targets still open and
611+385+353 unaudited `*_SUPERCLASS` sites (§25), this and the chain sweep are
the two funded follow-ups.

### 26.2 Further "retail predates the dev tree" confirmations
The pattern now has ~10 independent confirmations and should be the **first**
hypothesis for any negative delta in ported game code:
* `Rnd::CreateDefaultTexture` — retail calls the **3-arg** `RndTex::SetBitmap`
  overload, not the `Type`-taking one.
* `MusicLibraryNetSetlists::Poll` — retail has **no `SwapDxtEndianness()`** step
  between `RndBitmap::Load` and `SetMip`; that is **Wii-only**.
* `RockCentral::UpdateChar` — retail has **no runtime `if (profile)` guard**,
  only the stripped `MILO_ASSERT`. The guard also drove a 3-way callee-saved
  register rotation that cleared with it — another §24 instance.
* `OvershellSlot` ctor — Wii-dev-only `unk28` init and a trailing
  `Find<BandLabel>("user_name.lbl")` + `TheServer.AddSink(UserLoginMsg)` block,
  with **zero target instructions** between the `setupProviders` HandleType and
  the epilogue.
* `MusicLibrary` ctor — the inverse: retail **does** zero `unk19c`/`unk1a0`; our
  header comment claiming otherwise was simply wrong. Verify against the target,
  not against our own annotations.

## 27. Sharpening §24: the regswap stop-rule needs a second condition

§24 established that a *positive size delta* invalidates a "regswap ⇒ at_limit"
call. The `laneR-tail` stream then hit the same wrong verdict **twice more, at
zero size delta**, which forces a stronger statement of the rule.

* `RockCentral::UpdateChar` — `run_diff_inspect` reported a 3-way callee-saved
  rotation (r21/r23/r24) as `REGISTER_SWAP`. The actual defect was a **spurious
  runtime `if (profile)` guard** retail does not have (it has only the stripped
  `MILO_ASSERT`). Deleting the guard dissolved the entire rotation.
* `PatchLayer::Handle` — likewise: `mStickerCategory = Symbol(0)` →
  `Symbol s(0); mStickerCategory = s;`. Retail reads the value back from the
  temp's own stack slot and hoists *that* slot's zero-init rather than the
  `Sym()` NRVO slot's.

In both cases a **single stray `insert` or `diff_op`** sitting among the swaps
was the thread to pull.

> **Revised rule.** `regswap-only ⇒ at_limit` requires **both**:
> (a) the size delta is zero, **and**
> (b) there is **no `insert`, `delete`, or `diff_op` anywhere in the function**.
> If either fails, the swaps are a *symptom*. Register allocation is
> deterministic — it does not drift on its own; something upstream moved it.

Genuine at_limit under the revised rule still exists — e.g. `DisplayEvents`
(95.72): 55 swap instructions over 11 FPR pairs plus a 0x10 frame delta, where
retail keeps the `5.0f` constant's *address* in r26 and reloads it 3× while we
hoist the value into `f29`. That is permuter-class, and the permuter is off.

### 27.1 Structural leads left open, with the missing data point named
Recorded so the next wave does not re-derive them:
* **`MetaPanel`** (99.95) — the non-virtual part is **0x10 too big**: vbase at
  `this+0xdc` retail vs `+0xec` ours. `mMusic@0x60` and the `UIPanel` subobject
  `@0x40` are already correct, so the excess is inside **`SongPreview`** (ours
  0x74, retail 0x64). Three DC3-newer members sit at its tail; removing all
  three gives 0x60, not 0x64 — so **one of them is real** (likely `mPreviewDb`).
  Cheap and high-value with one more data point.
* **`PlatformMgr`** (99.95) — `mRegion` must be at **0x30** (ours 0x28), with
  `mScreenSaver@0x2c` retail-confirmed; needs 8 bytes between `mConnected@0x26`
  and `mRegion`. Wants a Ghidra pass on `SetDiskError`/`GetRegion`.
* **`LocalBandUser`** (98.35) — two independent defects: `HANDLE_EXPR(has_as_friend)`
  is **stubbed to constant 1** (retail calls `BandUser::HasAsFriend(BandUser*)`,
  undeclared in our tree), and the member block is 4 bytes too big (0x2c vs
  0x28) — and note `RB3_RBTREE_0x1C` is **not** set for this TU, so the
  `std::set` is already 0x18 and the extra 4 is elsewhere.
* **`EndingBonus::Handle`** (98.03) — our inlined `ObjectDir` lookup carries an
  extra **`TheLoadMgr.EditMode()`** branch (`lbz r11, 0x5c(TheLoadMgr)`) retail
  lacks. Engine-header-level; **likely a cascade lever far beyond this one
  function**, but needs its own isolated full-rebuild A/B.
* **`RndMesh::VertVector`** (via `GemRepTemplate`, 98.63) — its tail is **one
  32-bit field in retail** (single `stw` at +8), two `unsigned short`s in ours.
  The fix deletes `unkc`, which has 2 live sites in `Mesh.cpp`. Shared engine
  struct — dedicated A/B, not a drive-by.
* **`ViewSettingsProvider::SelectSetting`** should return **`bool`**, not `int`
  (target masks with `clrlwi r11,r3,24`). That changes its mangled name and
  needs a `target_symbol_map` repoint — **map owner's call.**

### 27.2 Map issues reported, not applied (single-owner rule)
* `?UpdateChar@RockCentral@@QAAXPAVTourCharLocal@@AAVDataResultList@@@Z` does not
  exist in our obj; the real symbol is the 6-arg
  `…@AAVDataResultList@@PAVObject@Hmx@@HH@Z` in `default/RockCentral`.
* `RndTex::SetBitmap` renders as the **4-arg** name on the target side in
  `Rnd.cpp` but the **3-arg** name in `MusicLibraryNetSetlists.cpp` — at least
  one is wrong. Harmless under normalized diff, but it misleads readers.
* `RockCentral::UpdateChar`'s target-side `insert_unique` resolves to
  `pair<Symbol,String>` where ours is `pair<Symbol,DataNode>` — evidence retail's
  `DataPoint` stores `map<Symbol,String>`.
* `0x822b6720` is `?resize@?$ObjList@UTarget@HamCamShot@@@@QAAXI@Z` (DC3's
  `HamCamShot`). **Careful — it currently reads 100.0% in `default/BandCamShot`**,
  so it is *not* a simple mispair to repoint; our tree emits that exact name.
  The real question is whether `BandCamShot::mTargets` is `ObjList` (retail
  iterates an embedded-sentinel list: `addi r18,r26,0x19c` = `end()`,
  `lwz r29,0x19c(r26)` = `begin()`) rather than our `ObjVector`, and whether the
  nested type should be renamed. A `laneR-fc1` trial of the `ObjList` swap
  reached `StartAnim` 99.5% and gained 4 sibling functions but lost 5 anonymous
  funclets to positional re-pairing (**net −1**), so it was reverted. With the
  naming settled first it should go clearly net-positive.

---

## 28. CORRECTED FINAL NUMBERS (supersede §26)

§26 was written before the last two `laneR-fc2` / `laneR-tail` merges landed.

**`laneR-frames` @ `f023625a`: 28,238 → 28,351. Net +113 (115 gained, 2 lost).**
**Worklist flip rate: 53.3% (40/75). 35 targets still open, 12 of them ≥97%.**

The 2 losses are the ones documented in §26 (`fn_825BC4CC` / `fn_825BC4F4`,
anonymous EH funclets re-paired positionally in `MetaNetMsgs`; their parent
flipped 79.6 → 100). Unchanged.

**Baseline caveat for whoever lands this:** the lane branched from `e7662cdb`
(28,238). `main` moved to **28,382** during the session (lane Q, +144). A merge
and a fresh whole-binary re-measure are required before landing — the +113 is
measured against the branch point, not against current `main`.

### 28.1 Final per-stream attribution

| stream | net (own baseline) | closed |
|---|--:|--:|
| `laneR-fc1` funclet parents | +33 | 2 full + 1 frame-only |
| `laneR-fc2` mid funclet parents | +33 | 9 |
| `laneR-sync` SyncProperty cluster | +29 | 14 of 18 |
| `laneR-tail` 99-band near-flips | +7 | 7 |
| `laneR-game` game bodies | +6 | 5 |
| map reveal waves (lead) | +4 | 4 |

Sum of independently-measured parts = +112; **merged whole-binary = +113**.
The streams reinforce rather than interfere. Re-measure the merge; never sum.

### 28.2 Ranked follow-ups, in priority order
1. **`HANDLE_`/`COPY_`/`SAVE_SUPERCLASS` chain audit (§25)** — 1,349 unaudited
   sites, identical mechanical method to the `SYNC_SUPERCLASS` sweep that banked
   +29. Highest expected value in the lane.
2. **Local-static `Symbol`/`Message` scanner (§26.1)** — the session's densest
   shape (11 of 40 flips, three independent workers, reported 3/3 on
   negative-delta game targets). Scan key: parents where the
   `??0Symbol@@QAA@PBD@Z` call count differs target-vs-base.
3. **Re-run `callee_set_join` (§21)** — it grows as the map grows, and 40 fresh
   names just landed. 238 of 1,809 funclet parents still have zero mapped callees.
4. **The 35 still-open worklist targets**, 12 of which are ≥97%.
5. **The six structural leads in §27.1**, each with its missing data point named.
6. **`EndingBonus`'s `TheLoadMgr.EditMode()` branch (§27.1)** — engine-header
   level, plausibly a cascade lever well beyond the one function; needs its own
   isolated full-rebuild A/B.

## 29. ★★ The `bool x = A || B` select — and why every other source form fails

The most broadly reusable shape of the session, because the **branchless-mask
divergence it fixes is everywhere** in ported game code.

**Symptom.** Retail materialises a bool through a volatile temp and a shared
`li rN, 1` true-block, with the zero supplied by a *function-wide zero register*
(`li r25, 0x0`). We instead emit MSVC's branchless idiom:
```
subf   r11, rA, rB
subfic r11, r11, 0x0
subfe  r11, r11, r11      ; r11 = (a != b) ? -1 : 0
and    rX, r11, rX
```
`subfic 0` followed by `subfe rX,rX,rX` **is** the tell — whenever you see that
trio, retail almost certainly branched and we did not.

**The only source form that reproduces retail is a single `||` initialiser:**
```cpp
bool finished = (mBand && mBand->GetBand() && mBand->MainPerformer()->mGameOver)
    || unk1e1;
```

**Measured negative controls — every one of these still fails**, which is why
this needs writing down rather than re-deriving:

| source form | result |
|---|---|
| `x = true; if (cond) x = false;` | branchless mask (the form we had) |
| `x = false; if (A) x = true; else {…}` | +1 callee-saved reg, frame +0x10 |
| `bool x; if (A) x = true; else x = (p != q);` | branchless mask |
| explicit inner `if/else` assigning both arms | branchless mask |

The intuition that "writing it branchy makes it branch" is **wrong** — MSVC `/O1`
re-collapses all of the branchy spellings. The short-circuit `||` *initialiser*
is load-bearing: it is what lets the compiler share one true-block and pull the
false value from the function-wide zero register.

This also settles the open diagnosis on `GemPlayer::Pass` recorded at the start
of the lane (source already used the branchy `if (…) isPhraseStart = false;` form
yet still emitted `subf/subfic/subfe/and`) — the branchy form was never going to
work; the fix is to restructure the two phrase-boundary bools as `||`
initialisers feeding the compound condition.

**Scannable:** any function whose diff contains `subfic … , 0x0` immediately
followed by `subfe rX, rX, rX`.

## 30. TRUE FINAL NUMBERS (supersede §26 and §28)

The `laneR-game` stream landed one further commit (§29's `||`-select lever plus
retail-only guards and a `TaskMgr::Init` body port) after §28 was written.

**`laneR-frames` @ `c6f04a6b`: 28,238 → 28,354. Net +116 (118 gained, 2 lost).**
**Worklist flip rate: 57.3% (43 of 75).**

Verified twice by independent full-rebuild A/B. The 2 losses are unchanged and
are the ones documented in §26 (`fn_825BC4CC` / `fn_825BC4F4`, anonymous EH
funclets positionally re-paired in `MetaNetMsgs`; their parent flipped
79.6 → 100).

**A 57.3% flip rate on an identity-established worklist is the headline number
for pricing.** It is still a floor: only 5 of 6 planned fix streams got
concurrency slots (the `system/` engine stream never launched), and 32 targets
remained open at close.

**Landing caveat unchanged from §28:** the lane branched from `e7662cdb`
(28,238); `main` reached **28,382** during the session. Merge and re-measure
whole-binary before landing — do not carry +116 forward as-is.

### 30.1 What this lane actually demonstrated
1. **Identification tooling still pays after byte-identity homing drains** — not
   as strict delta (`BANKABLE` was 0) but as **worklist generation**. The reveal
   converted 75 unpaired `fn_` addresses into scored near-misses; 43 of them are
   now matched.
2. **Constant surpluses repeated across unrelated classes mean one shared cause.**
   The `SyncProperty` census (§23.1) turned 18 apparently independent near-misses
   into a single 100-file edit worth +29.
3. **"retail X360 predates the newer oracle" is now the single most confirmed
   pattern in the project** (~15 independent instances this session alone),
   against *both* rb3-Wii DEV **and** DC3. It should be the first hypothesis for
   any size delta in ported code, in either direction.
4. **Two triage rules were measurably wrong** and are corrected in §24/§27
   (regswap stop-rule) and §29 (branchy source does *not* defeat the branchless
   mask — only a `||` initialiser does).

## 31. Shipped: the in-body local-static `Symbol` scanner (62 candidates, ready to work)

`scripts/harvest/localstatic_symbol_inbody_scan.py` (from `laneR-game`) turns
§26.1's tell-set into a worklist. It compares the target's
`??0Symbol@@QAA@PBD@Z` call count against ours per function.

```bash
python3 scripts/harvest/localstatic_symbol_inbody_scan.py \
    --min-pct 60 --max-pct 99 --exclude-macro-bodies \
    --json ~/tmp/localstatic_candidates.json
# -> 62 candidates
```
`--exclude-macro-bodies` is important: without it, `BEGIN_HANDLERS` expansions
flood the output with false positives. Columns are `pct | unit | symbol |
missing-Symbol-ctor count | .s file`.

Given this shape closed **11 of 43 flips** this session across three independent
workers (reported 3/3 on negative-delta game targets), this list is the single
most concrete piece of next-wave work the lane produced.

### 31.1 ★ A force multiplier hiding inside the scanner output
**Ten `SetType` overrides sit at *exactly* 62.92%** — `RndTexRenderer`,
`TexLoadPanel`, `RndLine`, `RndGroup`, `RndFlare`, `EventTrigger`, `CharIKScale`,
`RndPollable`, `BandCamShot`, `UIComponent` — all in unrelated units, each
missing exactly 1 `Symbol` ctor.

By §23.1's own lesson (*a constant repeated across unrelated classes is one
shared cause, never N independent divergences*), this is almost certainly a
**single defect in the shared `SetType`/`SetTypeDef` path or its macro**, not ten
jobs. **Diagnose one, fix all ten.** Same reasoning that turned 18 `SyncProperty`
near-misses into one 100-file edit worth +29.

### 31.2 Trap recorded: the `// 0x…` header annotations can be stale
`laneR-game` found `Player.h` / `Performer.h` offset comments **stale by ~0x30**,
and — critically — **`lookup_struct_offset` inherits that error**, because the
struct DB is built from those annotations. Resolving `Performer`'s real layout
required anchoring off `mGameOver@0x238` read from a *different* function's asm.

> **Rule:** when a member offset matters, confirm it against target asm from a
> second, independent function. Do not trust `// 0x…` comments or
> `lookup_struct_offset` alone — they share a single point of failure.
