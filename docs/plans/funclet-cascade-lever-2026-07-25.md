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
