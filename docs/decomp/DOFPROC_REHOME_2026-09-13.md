# The DOFProc re-home: one carve, three owners, and an alias that was right about the bytes and wrong about the world — lane W12-C, 2026-09-13

**Date** 2026-09-13 · **Branch** `w12-dofproc-rehome` off `main@96c3a685` ·
**Worktree** `~/tmp/wt-w12-c` ·
**Ruler** `functionRelocDiffs=name_check` (graded), resolved at runtime from
`build/45410914/report.json` — never hardcoded. `objdiff-cli` pinned across both
legs of every A/B (`sha256:a5c35b15d7d46ac4`).

Lane W11-A (`INIT_FOLDS_2026-09-13.md`, handoff 1) left three things undone
*together*, because each one alone is wrong: `0x82466000` is misnamed,
`MetaPerformer.cpp` mis-pins the 112 B sliver it sits in, and alias group 331 is
a `MAP_DEFECT_INVERTED_CONCLUSION` riding on both. This lane took all three.
It also found that W11-A's picture of the carve was incomplete in one direction
and that my own extension of it was wrong in another.

## 0. Result

| stage | Δmatched_functions | Δmatched_code | predicted | units off 100% |
|---|---:|---:|---|---:|
| 1 — re-home + rename + `ClassName` + withdraw 331 | **+1** | **+48 B** | +1 / +48 — **exact** | **0** |
| 2 — re-home the DOFProc COMDAT run out of `Mat.cpp` | **+3** | **+412 B** | +2 / +160 — **MISSED, see §4.2** | **0** |
| **branch total** | **+4** | **+460 B** | | **0** |

Both stages were pre-registered in full — row by row, with the downside cases
priced — before any build. The pre-registrations are reproduced verbatim in §3
and §4.

## 1. What the carve actually looks like

`0x82466000`–`0x82466298` is 664 bytes of contiguous code that `splits.txt`
divided among **three** units, none of which is where most of it belongs:

| range | size | owner before | nearest sibling in that unit |
|---|---:|---|---|
| `0x82466000`–`0x82466070` | 112 B | `MetaPerformer.cpp` | `0x82563258` — **1,036,264 B away** |
| `0x82466070`–`0x82466080` | 16 B | `DOFProc.cpp` | *(its only block)* |
| `0x82466080`–`0x82466298` | 536 B | `Mat.cpp` | `0x824382D4` — **187,308 B away** |

W11-A saw the first two. The third is new here, and it changes the shape of the
problem: this is not one mis-carve, it is a **COMDAT pile** — the linker grouped
COMDATs from several TUs adjacently, and the splitter attributed each run to
whichever unit happened to claim the surrounding address space.

The eight functions in it, with the identifications this lane established:

| address | size | identification | evidence |
|---|---:|---|---|
| `0x82466000` | 60 | `??0DOFProc@@QAA@XZ` | §2 — four independent proofs |
| `0x82466040` | 48 | `?ClassName@DOFProc@@UBA?AVSymbol@@XZ` | vtable slot 4 both sides; RELOC-IDENTICAL |
| `0x82466070` | 16 | `??1DOFProc@@UAA@XZ` | already named; fuzzy 100 |
| `0x82466080` | 80 | **DISPUTED** — see handoff 1 | not `RndMat::Terminate` (ours is a 4 B `blr`) |
| `0x824660D0` | 72 | `??$New@VDOFProc@@@Object@Hmx@@SAPAVDOFProc@@XZ` | RELOC-IDENTICAL; uses `??_R0?AVDOFProc@@@8` |
| `0x82466120` | 252 | `?SetType@DOFProc@@UAAXVSymbol@@@Z` | vtable slot 5; drives SetType's own static guard |
| `0x8246621C` | 32 | atexit-style reset of SetType's guard — **unnamed** | our TU emits no `??__F` for it |
| `0x82466240` | 88 | `??_GDOFProc@@UAAPAXI@Z` | vtable slot 0; RELOC-IDENTICAL |

## 2. `0x82466000` is DOFProc's constructor — four independent proofs

W11-A proved this one way (retail RTTI). Three more fell out of this lane, and
two of them are cheaper than the RTTI walk:

1. **RTTI** (W11-A): vtable `0x8206AEB4`, `??_R4` at `vt-4` → `0x821daa8c` →
   `pTypeDescriptor` `0x82c707d0` → `.?AVDOFProc@@`.
2. **The dtor, and it needs no RTTI at all.** `0x82466070` is *already* named
   `??1DOFProc@@UAA@XZ` and *already* matches at fuzzy 100 — and it stores the
   **same vtable `lbl_8206AEB4`** that `0x82466000` stores. One constructor and
   one destructor storing one vtable are one class. The map already contained
   its own answer.
3. **The vtable slots.** Dumping our compiled `??_7DOFProc@@6B@` relocations and
   retail's vtable side by side, slots 0/4/5 are `0x82466240` / `0x82466040` /
   `0x82466120` against our `??_E` / `ClassName` / `SetType`. The region *is*
   DOFProc's code.
4. **The subclass.** `DOFProc_NG.obj` — DOFProc's own derived class — relocates
   its constructor to this address. ⚠ W11-A recorded *"exactly one target obj
   relocates to the wrong name — `Rnd.obj`"*; there are **two**, and the second
   is the subclass. That figure is now stale.

### 2.1 Why nobody caught it, stated precisely

Both candidate constructors are **RELOC-IDENTICAL** to retail's 60 bytes:

```
??0DOFProc@@QAA@XZ                 60 B, 3 differing words, 0 non-reloc diffs
??0QuickplayPerformerImpl@@QAA@XZ  60 B, 3 differing words, 0 non-reloc diffs
```

Two *different classes'* empty constructors, and the only thing that
distinguishes them is which vtable they store — a relocation whose retail
operand is spelled `lbl_8206AEB4`, a **placeholder that `name_check` forgives**.
So the row's fuzzy 100 was never evidence for either name, and could never have
become evidence for either name. That is the `MAP_DEFECT_INVERTED_CONCLUSION`
shape in its purest form: **the byte evidence was correct and the conclusion was
inverted.** Group 331's T1 tier was earned honestly and pointed the wrong way.

`??0QuickplayPerformerImpl@@QAA@XZ` is not a fiction — `MetaPerformer.obj`
genuinely defines it, and the class is real. What was fictional was its address.

## 3. Stage 1 — the coupled change, predicted vs measured

Four parts. The first three are W11-A's handoff; the fourth was added on evidence
gathered here.

| | change |
|---|---|
| **P1** | `MetaPerformer.cpp` drops `.text 0x82466000-0x82466070`; `DOFProc.cpp`'s 16 B block grows to `0x82466000-0x82466080` (contiguous) |
| **P2** | map `0x82466000`: `??0QuickplayPerformerImpl@@QAA@XZ` → `??0DOFProc@@QAA@XZ` |
| **P3** | map `0x82466040`: *unnamed* → `?ClassName@DOFProc@@UBA?AVSymbol@@XZ` |
| **P4** | alias group **331** → `folded: []` + a `withdrawn` record (nothing pruned) |

### 3.1 Why it had to be one commit — each half priced before measuring

* **P2 without P1 = −60 B, pure.** `MetaPerformer.obj` cannot define
  `??0DOFProc@@QAA@XZ`, so objdiff — which pairs target↔base **by name, per
  unit** — would leave the row at 0% permanently. This is W11-A's warning, and
  it is the reason the rename is not a rename but a re-home.
* **P4 without P2 = −352 B.** Group 331 forgave exactly two sites, and both are
  fuzzy 100 *today* only because of it: `?NewObject@DOFProc@@SAPAVObject@Hmx@@XZ`
  (72 B, `default/system/rndobj/Rnd`) and `??0NgDOFProc@@QAA@XZ` (280 B,
  `default/DOFProc_NG`). With P2 in place both compare equal **literally** and
  the withdrawal costs nothing. **Measured: it cost nothing.** That is the
  coupling, demonstrated rather than asserted.
* **P1 without P2/P4** would move a correctly-scoring row to a unit whose name it
  no longer matches — the same −60 B by a different route.

### 3.2 P3 was licensed by a caller census, not by hope

The house rule is that naming a previously-anonymous address is a **bet**:
`name_check` *forgives* a placeholder target, so naming converts a forgiven site
into a checked one. So the census ran first, over all **3,083** target objs:

| address | reloc sites | in | verdict |
|---|---:|---|---|
| `0x82466040` (`ClassName`) | 1 | the `.rdata` vtable only | **zero `bl` callers ⇒ safe** |
| `0x8240E940` (`StaticClassName`) | 5 | `Mat`, `MetaPerformer`, `Rnd`, `.rdata` | **a real bet ⇒ NOT named** |

`0x8240E940` is confidently identified (`Init` and `ClassName` both call it, and
`New<DOFProc>` calls it first) and was still left alone. Identification does not
license naming; the caller population does.

Without P3, the re-home would have carried the 48 B `fn_82466040` into
`default/DOFProc` as a 0% row and knocked a 100% unit off for a purely
bookkeeping reason.

### 3.3 Measured

```
Δmatched=+1  Δmasked_equal=+0  Δhonest=+1  Δcode%=+0.000468pp  Δcode_bytes=+48
Δfuzzy=+0.000469pp   (legA 49.149975 -> legB 49.150444)
unit improvements: +2  default/DOFProc      (1->3)
unit REGRESSIONS:  -1  default/MetaPerformer (306->305)
unit net (ALL units) = +1   vs whole-binary Δmatched = +1
units at 100% [mpn]:   163 -> 163  (0 reached, 0 fell off)
units at 100% [fuzzy]: 135 -> 135  (0 reached, 0 fell off)
```

Pre-registered: **+1 fn / +48 B / +0.000468 pp / 0 units off.** Exact on all four.

The `none`-ruler control moved +48 B and was correctly labelled
`NOT_APPLICABLE: reading only (kinds=map,splits)` — a splits patch can move real
code, so `none` movement is expected and adjudicates nothing. ⚠ **The
`ALIAS_SUSPECT` shape is only readable on a map-*only* patch**, so the alias
withdrawal here is vouched for by its *measured cost of zero* against a
pre-registered −352 B exposure, not by the control.

### 3.4 `.pdata` was not touched, and dtk agreed with the re-home

`.pdata` is derived output. The first build after the edit **failed the
split-guard by design** — dtk rewrote `splits.txt`, moving exactly one line,
`.pdata 0x8220ED18-0x8220ED28` (the two unwind records for the two re-homed
functions), from `MetaPerformer.cpp` to `DOFProc.cpp`. The second build is the
fixed point, and that converged file is what is committed.

⚠ Operationally this matters for anyone re-running an A/B over a splits change:
**`ab_measure` REFUSED the first run** (`stage: build`, split-guard) because leg
B's freshly-applied patch was not a split fixed point. The fix is to converge
the file in the worktree first — two builds — and measure the converged file.
Leg A's reading in the refused run was already correct (42,818 / 37.934030),
so nothing was lost but a cycle.

## 4. Stage 2 — the third owner, and a misprediction with a diagnosable cause

Chasing the sliver turned up `Mat.cpp` pinning `0x82466080-0x82466298`. My first
reading was *"the whole thing is DOFProc's, re-home all 536 B"*. **That was wrong
two ways, and both corrections are worth keeping.**

1. ⛔ **`Mat.cpp` is `system/rndobj/Mat.cpp` — RndMat — not `math/Mat.cpp`.** I
   assumed the latter from the basename and had to be corrected by
   `objects.json`. This is the bare-vs-nested heading hazard wearing a different
   hat: `DOFProc.cpp`, `MetaPerformer.cpp`, `Mat.cpp` and `Console.cpp` are all
   **bare** headings, and their build artifacts land at
   `build/45410914/obj/Console.obj` while `Rnd.obj` lands at
   `build/45410914/obj/system/rndobj/Rnd.obj`. Two of my scripts failed on this
   mid-lane. **Never reconstruct these paths; glob or read the pairing.**
2. ⛔ **`0x82466080` is disputed and was NOT moved** (§5.1).

So only the provable contiguous run moved: `0x824660D0-0x82466298` → `DOFProc.cpp`.
`Mat.cpp` keeps `0x82466080-0x824660D0` and still holds 7 `.text` blocks.

### 4.1 Predicted vs measured — I MISSED, and by a lot

| | Δmatched | Δcode | `default/DOFProc` |
|---|---:|---:|---|
| **predicted** | +2 | +160 B | **falls off 100%** (5/7) |
| **measured** | **+3** | **+412 B** | **7/7, stays at 100%** |

```
Δmatched=+3  Δmasked_equal=+0  Δhonest=+3  Δcode%=+0.004022pp  Δcode_bytes=+412
Δfuzzy=+0.004021pp   (legA 49.150444 -> legB 49.154465)
unit improvements: +4  default/DOFProc  (3->7)
unit REGRESSIONS:  -1  default/Mat      (61->60)
unit net (ALL units) = +3   vs whole-binary Δmatched = +3
units at 100% [mpn]:   163 -> 163  (0 reached, 0 fell off)
units at 100% [fuzzy]: 135 -> 135  (0 reached, 0 fell off)
```

### 4.2 ★ Why I missed: I rebuilt a known one-sided instrument error

I priced `?SetType@DOFProc@@UAAXVSymbol@@@Z` at **0 bytes** because my hand-rolled
COFF comparator returned a confident **`BODY DIFFERS` — 17 non-reloc diffs**.

It was reading the COMDAT's `SizeOfRawData` (**292 B**) as the *function extent*.
The section actually holds the **252 B function + a 32 B trailing thunk — which
is literally `fn_8246621C` — + 8 B**. So it compared 292 of our bytes against
292 of retail's, running **40 bytes past the function boundary into the next
function**, and every byte of that overhang counted as a difference.

That is the **same one-sided over-read `STLPORT-1` recorded** for
`tools/coff_bodies_ext.py` (billing the successor symbol's EH funclet into the
COMDAT span). ⚠ And it fails in the most dangerous direction: an over-read can
only ever manufacture a **decisive-looking NEGATIVE**, which is the verdict class
that closes veins. A two-sided size check cannot catch it, because the artifact
is entirely on our side.

★ **What saved the 252 bytes was the accuracy motive, not the instrument.** I
named `SetType` anyway — because it is provably DOFProc's (vtable slot 5, and it
drives DOFProc's own static guard) and the caller census said naming it was free
— while *expecting zero bytes*. Had I let the (wrong) instrument set policy, I
would have left it unnamed and lost 252 B. This is the standing directive
"accuracy outranks the headline" paying a literal dividend.

### 4.3 The accounting is exact

`+412 = 72 (New<DOFProc>) + 252 (SetType) + 88 (??_G)`.

`fn_8246621C` (32 B) was **already fuzzy 100 inside `Mat`, flagged
`masked_equal: true`** — objdiff had paired it by funclet byte-signature, not by
name. It merely changed units, so it nets **0**. That is why `default/DOFProc`
gains 444 B while the binary gains 412, and why `default/Mat` shows
`61 -> 60`. `masked_equal_functions` is +0 across the run, as it must be.

⚠ I had pre-registered `fn_8246621C` as a *0% row that would drag DOFProc off
100%*. It was a 100% row all along. **A name-keyed reading of an unnamed row is
blind to the `masked_equal` pairing channel** — the same blindness ALIAS-2
recorded when a name-keyed census booked its own blind spot as risk.

## 5. Refutations

### 5.1 ⛔ `0x82466080` is NOT `?Terminate@RndMat@@SAXXZ`, and our `Rnd::Terminate` calls the wrong function

This is the most valuable thing the lane found and it is **deliberately not
fixed here**, because it is a `src/` change.

`?Terminate@Rnd@@UAAXXZ` reads **fuzzy 100.0 (180 B)** in
`default/system/rndobj/Rnd`. Its relocations, target vs ours:

```
          target (retail)                    ours
+0x54     ?Terminate@RndOverlay@@SAXXZ       ?Terminate@RndOverlay@@SAXXZ
+0x58     ?Terminate@RndMultiMesh@@SAXXZ     ?Terminate@RndMultiMesh@@SAXXZ
+0x5c     fn_82466080                        ?Terminate@RndMat@@SAXXZ
```

Both sides emit **exactly three** `Terminate` calls at **identical offsets**. And
`src/system/rndobj/Rnd.cpp:509` currently reads:

```cpp
#ifdef HX_NATIVE
    // DC3-era addition (also present in rb3-Wii dev); retail RB3-360 does not
    // call DOFProc::Terminate here — its teardown goes straight to RndMat.
    DOFProc::Terminate();
#endif
    RndMat::Terminate();
```

**That comment is refuted by the retail bytes.** `fn_82466080` is an **80-byte**
function that does `RELEASE(<global at lbl_82CC6368>)` — load the global, if
non-null call vtable slot 0 with flag 1 (the deleting dtor), store null. Our
`?Terminate@RndMat@@SAXXZ` is an **empty 4-byte `blr`** (deliberately emptied by
lane METAMAT-1: *"retail's material Terminate has no sMetaMaterials to
release"*). **An empty function cannot be an 80 B `RELEASE`.**

Since retail emits only three calls and RndMat's is empty in both builds — and
an empty *cross-TU* callee still costs a `bl` under `/O1` (no LTCG, no cross-TU
inlining), which is exactly why *our* empty call survives — the third slot in
retail is a real, non-empty function releasing a singleton. `RELEASE(TheDOFProc)`
is precisely the body of `DOFProc::Terminate`.

⇒ **The `#ifdef HX_NATIVE` is backwards**: retail calls `DOFProc::Terminate()`
there, and it is `RndMat::Terminate()` that is absent. The row scores **100
anyway**, because `fn_82466080` is an unnamed placeholder and `name_check`
**forgives placeholder targets**. This is the canonical shape of the hazard the
project already records: *a wrong callee reads 100.*

⚠ I did **not** name `0x82466080`. Naming it without fixing the source would
convert that forgiven site into a charged one and cost **−180 B** — measured
exposure, pre-registered. The correct order is: fix `Rnd.cpp`, *then* name the
address. Handoff 1.

### 5.2 ⛔ W11-A's "exactly one target obj relocates to the wrong name" is stale

There are **two**: `Rnd.obj` and `DOFProc_NG.obj`. The second is DOFProc's own
subclass, and it is corroborating evidence W11-A's exposure census missed. The
conclusion W11-A drew from the figure (zero cascade risk) still holds — both
sites re-pair correctly — but the figure itself should not be re-quoted.

### 5.3 ⛔ My own two errors, recorded so they are not repeated

* **The COMDAT over-read** (§4.2) — reading `SizeOfRawData` as a function extent
  produced a false `BODY DIFFERS` on a function that scores 100.
* **`Mat.cpp` is RndMat, not math/Mat** — a basename assumption that survived
  three tool calls before `objects.json` corrected it. Related: `default/Mat`,
  `default/Console`, `default/DOFProc` and `default/MetaPerformer` are **bare**
  headings while `default/system/rndobj/Rnd` is nested, and two of my scripts
  crashed or silently found nothing on exactly that split.

## 6. Gates, verbatim

Full `./tools/ninja-locked` (never a targeted `.obj` — the six post-compile
patchers are part of the ruler), log `~/tmp/rb3_build_w12c.log`:

```
EXIT=0
[renamed-check] 25532/29047 map names present in 3083 target objs = 87.9% (floor 40%)
```

(baseline was `25528/29043`; **+4 addresses, +4 present** — all four new map
names resolve in the target objs.)

```
[patch-state] OK: 1205 decomp, 3083 target objects match 2026-09-13T07:12:25Z (tree_sha256=5a9593a3ad6beeb5)
```

```
VALIDATE: PASS -- 1362 map-consistent, 239 tolerated (enumerated above), 0 contradicted, 1603 total
```

`tools/native_build_gate.sh` **not run, and not required**: the branch touches no
`src/` and no `native/`. `git diff --name-only 96c3a685..HEAD` is exactly three
files — `config/45410914/splits.txt`, `scripts/symbol_aliases.json`,
`scripts/target_symbol_map.json` — and a grep for `^(src/|native/)` returns 0.

⚠ Pre-renamer sanity check, run **before** any name-keyed analysis, because a
fresh worktree's reflinked target objs carry the objs but not the renamer's
effect and every retail mangled name would read "absent": 3,083 target objs,
`[renamed-check] 25528/29043 = 87.9%` on the untouched baseline build, and
`??1DOFProc@@UAA@XZ` present in `DOFProc.obj`.

### 6.1 Branch total, read off the final built tree

| | matched_functions | matched_code | matched_code_percent | fuzzy |
|---|---:|---:|---:|---:|
| main `96c3a685` | 42,818 | 3,886,704 | 37.934030 | 49.149975 |
| branch tip `5d3cc86e` | **42,822** | **3,887,164** | **37.938520** | **49.154465** |
| delta | **+4** | **+460 B** | **+0.004490 pp** | +0.004490 pp |

The two stage deltas compose exactly (`+1 +3 = +4`, `+48 +412 = +460`), and
stage 2's leg A equals stage 1's leg B to the last digit (`matched=42819`,
`code%=37.934498`), so they are chainable. `masked_equal_functions` is
**22,994 throughout** — unmoved by either stage. `total_code` is unchanged at
**10,245,956**: every byte here is *reattribution plus pairing*, not denominator
movement.

## 7. Handoffs

1. ⛔⛔ **`src/system/rndobj/Rnd.cpp:509` calls the wrong `Terminate`, and the
   metric cannot see it.** §5.1 has the full evidence. The fix is to drop the
   `#ifdef HX_NATIVE` around `DOFProc::Terminate()` and guard `RndMat::Terminate()`
   instead, then name `0x82466080` → `?Terminate@DOFProc@@SAXXZ` and re-home it
   from `Mat.cpp` to `DOFProc.cpp`. **Order matters**: naming first costs −180 B
   because it charges a site our source spells wrongly. Expected value after the
   source fix is ~**0 B** (the site is forgiven either way today) — this is a
   pure-accuracy, bug-exposure change of exactly the class the project ranks
   above the headline. It touches `src/system/**`, so it **needs
   `tools/native_build_gate.sh`** (require `skipped=0`).
   ⚠ Whoever takes it should also settle what `lbl_82CC6368` is; if it is
   `?TheDOFProc@@3PAVDOFProc@@A` the case is closed outright.

2. **Our `DOFProc::Init` and `DOFProc::Terminate` carry a DC3-era block RB3
   retail does not have.** Retail's `Terminate` is 80 B; ours compiles to a
   256 B COMDAT. Retail has no counterpart to the
   `static DataNode &n = DataVariable("the_dof_proc"); n = NULL_OBJ;` pair in
   either function — and note that `0x824660D0`, which I first guessed was
   `Init`, turned out to be `??$New@VDOFProc@@@Object@Hmx@@SAPAVDOFProc@@XZ`,
   so **retail's `DOFProc::Init` is not in this address range at all** and has
   not been located. Two jobs: find retail's `Init`, and decide whether the
   `DataVariable` block is a DC3 addition to be `#ifdef`-guarded. Same
   "DC3 is newer" family as handoff 1.

3. **`0x8240E940` = `?StaticClassName@DOFProc@@SA?AVSymbol@@XZ`, identified but
   deliberately unnamed.** Three call sites corroborate it (`ClassName`,
   `New<DOFProc>`, and the factory registration all call it, and our COMDAT is
   the right shape). It is left anonymous because the caller census found **5
   reloc sites across 4 objs** (`Mat`, `MetaPerformer`, `Rnd`, `.rdata`) — naming
   it is a real bet, not a freebie. Anyone taking it must price all five sites
   first; the same census script is in the transcript.

4. **`fn_8246621C` (32 B) has no name we can supply.** It resets bit 0 of
   `SetType`'s static guard `??_B?1??SetType@DOFProc@@UAAXVSymbol@@@Z@51`, i.e.
   it is the atexit-style teardown for that function-local static — but our TU
   emits **no `??__F` symbol** for it, so there is nothing to pair. It already
   scores 100 through `masked_equal`, so there is no metric reason to chase it;
   the open question is why our compile does not emit the thunk.

5. **`??0QuickplayPerformerImpl@@QAA@XZ` now has no address.** The class is real
   and `MetaPerformer.obj` defines the constructor; only its *address* was
   fictional. Its true retail address is unidentified. ⚠ It will be hard to find
   by bytes alone: it is a 60 B empty constructor whose only distinguishing word
   is the stored vtable, and retail spells that operand as a forgiven
   placeholder — the very property that made this defect invisible for months.
   Find it via its vtable / RTTI, not via body matching.

6. **The `RecursePatternInternal` second target was NOT attempted.** W11-D
   re-priced H6 to +3,084 B / +11 fns across 12 rows and then refused it,
   concluding the fix is a map repair rather than a `contradiction_exempt`. I
   left it untouched: the DOFProc carve turned out to have a third owner and a
   live source bug in it, which was more work and more value than the brief
   anticipated. H6 is unchanged and still open.

## 8. What this lane deliberately did NOT do

* **No `src/` change of any kind**, therefore no native gate. The `Rnd::Terminate`
  wrong-callee (§5.1) is the one change I most wanted to make and explicitly did
  not: it is a shared-`src/` edit that must be gated, and bundling it into a
  splits/map commit would have made the A/B unattributable.
* **Did not name `0x82466080`** — −180 B without the source fix, and the naming
  is only correct *after* it. Left with `Mat.cpp`, which is now the only part of
  that 664 B run still misattributed, and knowingly so.
* **Did not name `0x8240E940`** despite being confident of its identity —
  5 exposed caller sites (handoff 3). *Identification does not license naming;
  the caller population does.*
* **Did not keep `default/DOFProc` at 100% by holding SetType back.** Leaving
  `SetType` + its guard thunk with `Mat.cpp` would have kept the unit at 5/5 and
  was pre-registered as **REFUSED** — that is denominator manipulation. The whole
  contiguous DOFProc run moved. (In the event the unit stayed at 100% anyway, but
  the decision was made before the measurement and would stand had it not.)
* **Did not prune alias group 331.** `folded: []` plus a `withdrawn` record, per
  house convention — pruning classes that currently forgive 0 has been measured
  at **+94,616 B to reverse**.
* **Did not run a whole-binary alias fixpoint or any bulk naming sweep.** Four
  addresses, each individually adjudicated on retail bytes with a caller census.
