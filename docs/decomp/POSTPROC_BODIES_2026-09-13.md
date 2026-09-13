# Lane W9-C — `RndPostProc::Save`, `NewFile`, `FileLocalize`

**Branch** `w9-postproc-bodies` · worktree `~/tmp/wt-w9-c` · base `1d560c2b`
(`git merge-base --is-ancestor 1d560c2b HEAD` asserted before the first edit).
Ruler: **`name_check` (graded)**, read from `report.json`'s
`provenance.diff_config`, not assumed.

Baseline after this lane's mandatory first full build (a reflinked worktree's
target objs are pre-renamer, so any name-keyed negative taken before that build
is vacuous — FOLDPROVE-2):

```
matched_functions   42,739      masked_equal        22,980
matched_code     3,865,716 B    total_code      10,245,956
matched_code_percent 37.729187  total_functions     69,219
fuzzy_match_percent  49.127808
```

That reproduces W8-C's post-lane figures to the digit (+1 fn / +500 B on its own
baseline), so the record I was handed is confirmed before anything was built on
it.

---

## 0. Headline

| # | change | predicted | measured |
|---|---|---|---|
| 1 | `RndPostProc::Save` streams `mLuminanceMap` | row ≥99.3, Δ**0/0** | row **99.878784** (was 96.363640), Δfns **+0**, Δbytes **+0**, Δfuzzy +0.000354pp |
| 2 | `NewFile`: 4 constructs ported off retail bytes | Δ**0/0** | Δfns **+0**, Δbytes **+0**, Δfuzzy +0.000000pp |
| 2x | *(sub-change 5, `gNullFiles`/`NullFile` removal)* | Δ**0/0** | ⛔ **−4 fns / −88 B — PREDICTION FAILED, REVERTED** |
| 3 | `FileLocalize` body port | Δ**0/0** | Δfns **+0**, Δbytes **+0**, Δfuzzy +0.000000pp |

Every run: `python3 tools/ab_measure.py --worktree ~/tmp/wt-w9-c --from-dirty`,
one change per run, committed immediately. **0 units fell off on either ruler on
every landed run** — the per-unit guard the brief required, and it matters:
change 1 is in `rndobj/`, the perturbation-prone directory.

The most valuable result in this lane is the one that failed (§3.3).

---

## 1. `RndPostProc::Save` — the missing field is the `0x54` AGGREGATE ITSELF

### 1.1 What W8-C left, and the correction

W8-C's handoff said *"retail stages one extra scalar through `WriteEndian`
before the `fn_8238B5B8` aggregate at `0x54`"*. That is not what retail does.
The `0x54` **aggregate is the missing field**: `ObjPtr<RndTex> mLuminanceMap`,
written via `fn_8238B5B8` between `mBloomThreshold` (`0x40`) and `mColorXfm`
(`0x64`). There is no extra scalar.

Retail's 49-field schema was read out of `fn_824302B0` in full and checked
field-for-field against our `BEGIN_SAVES` block and against
`scripts/harvest/class_layout_report.py RndPostProc` (compiler-authoritative,
`sizeof = 524 (0x20c)`). **Every other field, and their order, already matched.**
`fn_8238B5B8` is the `ObjPtr<RndTex>` instantiation — the same callee retail uses
for `mNoiseMap` (`0x144`), `mGradientMap` (`0x1a8`) and `mRefractMap` (`0x1c4`).

### 1.2 The corroboration that makes it safe, from the OTHER side of the schema

Reading `Save` alone would have been a one-sided instrument. It is not needed:

* retail's `Save` makes **four** `fn_8238B5B8` (`ObjPtr<RndTex>`) writes —
  `0x54`, `0x144`, `0x1a8`, `0x1c4`;
* retail's `Load` (`fn_82430FD0`, identified in §2) makes **four**
  `?Load@?$ObjRefConcrete@VRndTex@@VObjectDir@@@@` reads;
* our source wrote **three**.

Save and Load agree with each other and disagree with us. A field the writer
emits and the reader consumes is in the schema.

### 1.3 The `REGISTER_SWAP` label was, again, a symptom

`run_objdiff` reported `REGISTER_SWAP (RarelyHandFixable): 4 instructions, 1
pair (r3↔r4)` at charged indices [29]–[37]. That is exactly the standing rule
about this label. `RndColorXfm::Save` is a member (`r3 = this+0x64`, `r4 = bs`)
while `operator<<(BinStream&, const ObjPtr&)` is `(r3 = bs, r4 = &ptr)`, so the
"swap" was the **alignment slip from the omitted field**. It dissolved entirely
on the fix, together with all 3 inserts and 6 deletes.

### 1.4 The residual, and a pricing-screen calibration datum

After the fix the row has **32 charges and every single one is a
`0x54`/`0x58`/`0x5c` stack-scratch-slot immediate** — zero instruction
differences, zero register differences. Retail rotates three scratch slots; we
use `0x58` throughout.

★ **The brief's `5/N` screen over-predicts this row exactly 5×, and the correct
rate for a differing IMMEDIATE is `1/N`.** With `N = 1056/4 = 264`:

| charge kind | rate | 32 charges predict | measured |
|---|---|---|---|
| register / symbol arg (`5/N`) | 0.018939 pp | 99.393940 | — |
| **immediate arg (`1/N`)** | **0.003788 pp** | **99.878788** | **99.878784** |

Closes to five decimals. This does not contradict the brief — the brief scoped
`5/N` to register and name args, and W7-C had already measured "~5× over-
prediction on immediates" without pinning the rate. **It is now pinned: the
screen prices COUNT not KIND *within* a kind, and there are three kinds, with
immediates one-fifth the price of the other two.** `mpn == fuzzy` here, which is
consistent — an offset immediate is charged by both rulers.

⚠ **`run_objdiff` printed "96.5% canonical (96.3% raw)" for a `report.json`
value of 96.363640, and later "Match: 99.9%".** Every figure in this document is
read unrounded from `report.json`, per the brief's warning.

### 1.5 What is left, and why I did not take it

1,056 B + 1 function sits behind those 32 slot immediates. I did **not** attempt
it. It is a stack-slot-allocator difference with no instruction or register
component — textbook permuter-class, and the standing user directive defers the
permuter. The one source-level lever with a mechanism (W8-C's change 3: chaining
`bs << a << b << c` gives one full-expression's temporaries distinct slots) is
already how our source spells the affected groups, so the obvious move is
already made. Handed off in §6 with the full slot map so the next lane does not
have to re-derive it.

---

## 2. `?Load@RndPostProc@@` — IDENTIFIED, and NOT NAMEABLE TODAY

The brief asked for this and the spatial reasoning worked exactly as W8-C
predicted it would. **`fn_82430FD0` (1,728 B) is `RndPostProc`'s revision-gated
load body.** Four independent instruments:

| instrument | evidence |
|---|---|
| callee inventory | `?ReadEndian@BinStream@@` ×38, `?Load@RndColorXfm@@` ×1, `Color` ×9, `bool` ×8, `Key<float>` ×6, `Vector3` ×4 |
| the RndTex count | `?Load@?$ObjRefConcrete@VRndTex@@VObjectDir@@@@` **×4** — matching retail `Save`'s four `ObjPtr<RndTex>` writes |
| relocation count | ours `?LoadRev@RndPostProc@@` has **94**; retail's has **94** |
| relocation multiset | near-identical; the disagreements are enumerated below |

⛔ **But it cannot be named, and the reason is structural, not evidential.** The
call site in `fn_82433268` is decisive:

```
mr   r4, r31
lhz  r5, 0x4(r30)      <- unsigned HALFWORD: a revision
mr   r3, r29
bl   fn_82430FD0
```

**Retail's function takes THREE arguments — `(this, BinStream&, ushort rev)`.**
Ours takes two: `LoadRev(BinStreamRev&)`, where `BinStreamRev` is a
`{BinStream&, int}` pair passed by reference. The relocation diff says the same
thing from the other direction: retail reads through `??5BinStream@@` (bool ×8,
`Key<float>` ×6) where we read through **`??5BinStreamRev@@`**, and retail saves
**r24**–r31 against our r28–r31.

Our object defines **no symbol whose signature describes retail's function.**
Naming `0x82430FD0` today would mean either inventing a spelling our object does
not emit (the row would then be permanently 0% — the base obj cannot define the
name), or pairing retail's 1,728 B body against our 224 B `?Load@RndPostProc@@`
wrapper. That is the *"ASK IF THE THING NEEDING A NAME EXISTS"* rule applied
honestly: **the thing needing a name is a function whose signature our source
does not have.**

★ The decomposition itself matches, which is what makes the handoff concrete:
retail has `fn_82433268` (192 B, reads the rev, gates on `cmplwi 0x10`,
tail-calls) + `fn_82430FD0` (1,728 B); we have `?Load@RndPostProc@@` (224 B) +
`?LoadRev@RndPostProc@@` (1,916 B). Two functions each side, same roles. The
port is a signature change, not a rewrite — see §6.

⚠ Method note: `.4byte fn_82430FD0` in `PostProc.s` is a **`.pdata`** entry, not
a vtable slot. I misread it as a vtable on first pass and the direct-caller scan
is what corrected it. `fn_82430FD0` is in no vtable.

---

## 3. `NewFile` — four constructs ported, and one MEASURED NEGATIVE

### 3.1 What retail actually does

`fn_825173E0`, 424 B, 27 relocations. Four differences from our body, each
adjudicated on retail bytes:

1. **`MainThread()`.** Retail calls it at +24 and **discards `r3`** — no test
   follows. That is `MILO_ASSERT(cond,line)` compiling to `((void)(cond))` in
   this build: the extern call still evaluates, the `TheDebug.Notify` branch
   does not exist. W8-D reached the same conclusion, but note its negative
   ("no `?Notify@Debug@@`, no `?TheDebug@@`, no format literal") was drawn from
   **named**-relocation lookups over a region whose relocations are *all
   anonymous labels* — right answer, weaker warrant than it appeared.
2. **`UsingCD()`** — retail's ArkFile guard tests only mode bits.
3. **The null check around `Fail()`** — retail calls `result->Fail()` through the
   vtable unconditionally, *including on the `mem == 0` path* (`li r3,0; mr r27,r3`
   then `lwz r11, 0(r27)`). It would fault there. That is retail's behaviour and
   we now reproduce it.
4. **The capture-log tail.** Two real corrections here, and the second is a bug:
   * retail's format literal is `lbl_82087CB0`, read out of `band.exe`'s
     `.rdata` as **`"'%s'\n"`**. Our `"./%s"` is not in the binary at all.
   * retail's strlen loop increments **before** the test
     (`lbz r9,0(r11); addi r11,r11,1; cmplwi r9,0; bne`), so the pointer ends at
     `buf+strlen+1` and the trailing `-1` yields **`strlen`**. Ours was
     `while (*ptr != '\0') ptr++;` — test before increment — so the same `-1`
     yielded **`strlen-1`**. We wrote **one byte too few** on every capture-log
     line. `while (*ptr++) ;` produces retail's instruction shape *and* the
     correct length.
   * retail guards on `(gOpenCaptureFile && (mode & 2))` only — no `mode & 0x20000`.

⚠ `_MemAllocTemp` is **not** a divergence: our 5-arg debug call already reduces
to the retail 2-arg `(size, align)` form through `MemMgr.h`'s macro, and
`sizeof(ArkFile) == 0x40` matches retail's `li r3, 0x40`. I checked rather than
assumed, because W8-D listed it adjacent to real divergences.

### 3.2 The instrument, since the row is unpaired

`?NewFile@@` has no map name, so the score is unavailable (fuzzy 0.000000).
COMDAT bytes instead:

| | size | relocs |
|---|---|---|
| ours, before | 524 B | 46 |
| ours, all five sub-changes | **416 B** | **27** |
| ours, as landed (sub-change 5 reverted) | 480 B | 33 |
| **retail `fn_825173E0`** | **424 B** | **27** |

At 416/27 the relocation multiset corresponded **1:1 by name** with retail's:
`lbl_82CCA090`↔`gOpenCaptureFile` ×4, `lbl_82087CB8`↔`"."` ×2,
`lbl_82087CB0`↔`"'%s'\n"` ×2, `fn_82516E28`↔`?FileLocalize@@`,
`fn_8252DEE8`↔`?New@AsyncFile@@`.

★ **That positionally re-confirms both of W8-D's "free identifications" from a
completely independent direction** — a 27-relocation alignment that places
`fn_82516E28` exactly where our `FileLocalize` sits and `fn_8252DEE8` exactly
where `AsyncFile::New` sits. W8-D derived them from argument position inside one
function; this derives them from a whole-body correspondence.

The 8 B residual is `subi r31, r1, 0x290` + `__savegprlr_27` — retail keeps a
frame pointer for its two 256-byte buffers where we address off `r1`.

### 3.3 ⛔ The measured negative — and it is the most transferable thing here

Sub-change 5 deleted the `if (gNullFiles) return new NullFile();` early-out,
which retail's `NewFile` provably does not have (424 B, 27 relocations, no
`operator new`, no `NullFile` vtable relocation). It took our body to a
near-exact 416/27.

**Pre-registered Δ0/Δ0. Measured −4 matched functions / −88 B.**

| row lost from 100 | size |
|---|---|
| `?Write@NullFile@@UAAHPBXH@Z` | 8 |
| `??1File@@UAA@XZ` | 16 |
| `?Filename@File@@UBA?AVString@@XZ` | 48 |
| `?ReadDone@NullFile@@UAA_NAAH@Z` | 16 |

88 B exact, `unit net (ALL units) = -4` equal to the whole-binary Δ. That one
branch is the **only** thing in the TU forcing `NullFile`'s vtable — and with it
those four virtuals — to be emitted into `File.obj`. Retail's `File.obj`
**defines all four** (they are pinned, named, and were scoring 100), so retail's
`File.cpp` instantiates `NullFile` somewhere I did not locate.

⇒ Deleting it made our object define **fewer** symbols than retail's: strictly
less accurate. This is **not** the "code% drop from a truer denominator is a
win" case — the denominator did not move; we lost four genuinely-matching
functions. Reverted; all four back at 100.

★★★ **The rule: "retail's function F does not contain X" does NOT license
"delete X from the TU."** The two claims are coupled through **COMDAT
emission**, not through F's own code. A body port that is byte-correct for its
own function can still be wrong for its translation unit. The cheap screen is to
ask, before deleting any construct that constructs an object: *is this the only
instantiation of that type in the TU, and does the target obj define its
members?*

⚠ And note the shape of the trap — deleting **dead-looking code deleted four
matching functions**. It looked like pure cleanup.

---

## 4. `FileLocalize` — ported; DC3-newer logic removed

`fn_82516E28` = `?FileLocalize@@YAPBDPBDPAD@Z`, 408 B, confirmed a third time by
§3.2's relocation alignment. Its entire callee inventory is `?SystemLocale@@`
**×3 and nothing else**; its only data relocations are one hi/lo pair to
`gNullStr` (`lbl_82C71838`) and one to a single static buffer (`lbl_82CCA5B0`).

Our 548 B body carried four things retail does not have:

| ours | retail | verdict |
|---|---|---|
| `GetGfxMode()`/`kNewGfx` gate on the `/og/` scan | scan is **unconditional** | removed |
| `HongKongExceptionMet()` + `strstr("locale")` + `strstr("ui/eng")` + an `"eng"` fallback write | absent | removed |
| **two** function-local statics (`?N@`, `?CC@` scopes) | **one**, hoisted into `r28` at entry | merged |
| `return buffer` immediately after the `/eng/` splice | **falls through** with `result = buffer` into the `/og/` scan | corrected |

★ The fall-through is a real behavioural difference, not a shape one: a path
containing **both** `/eng/` and `/og/` gets **both** edits in retail and only the
first in ours.

⚠ I explicitly tested and rejected the tidier hypothesis. Retail's shape is what
our source would produce if `isOg` were compile-time **true** and
`HongKongExceptionMet()` compile-time **false** — but `GetGfxMode()` returns the
runtime global `gGfxMode` and `HongKongExceptionMet()` is an extern, so neither
could fold. These are **DC3-newer source constructs RB3-360 retail never had**,
consistent with the standing "dc3-decomp is newer than RB3" caveat — not
optimisation artefacts.

**Result on the COMDAT instrument: 548 B / 31 relocs → 408 B / 13 relocs against
retail's 408 B / 13 relocs — exact on size, exact on relocation count, and the
only name differences are our real symbols against retail's placeholder labels
for the same two objects** (`?gNullStr@@3PBDB`↔`lbl_82C71838`,
`mybuffer`↔`lbl_82CCA5B0`).

The masked bodies are **not** identical: 56 of 102 words differ, and the
difference is a consistent register renaming (ours r31/r30/r29 where retail uses
r30/r29/r28). One iteration on temp naming was tried and moved it not at all
(same 408/13/56). That is permuter-class and deferred by standing directive. I
am **not** claiming this body is byte-exact; §3.2's `NewFile` and W8-D's
`FileRelativePath` were, and this one is not.

Retail emits two back-to-back `SystemLocale()` calls into the same sret slot and
tests only the second. I reproduced that literally (`SystemLocale();` then
`if (!SystemLocale().Null())`) rather than guessing at the original condition,
and said so in the source comment.

**Measured:** pre-registered Δ0/Δ0 (row unpaired); measured **Δmatched +0,
Δcode_bytes +0, Δfuzzy +0.000000pp**, 1 leg-B recompile (so not an
absent-vs-absent vacuity), **0 units fell off on either ruler** — including the
two string-literal COMDATs and the second static this change removes from
`File.obj`, which after §3.3 was the failure mode I was watching for.

---

## 5. Native behaviour — what changed, stated narrowly

Three of this lane's edits are on paths the native runtime executes. I am
stating what the runtime did wrong and nothing more, because the wave before
W8-C overstated exactly this and W8-C had to withdraw it.

* **`RndPostProc::Save` (write-side only).** Our `Save` omitted
  `mLuminanceMap` from the stream. Any `.milo` we authored carried a
  `RndPostProc` block one `ObjPtr` short of the format the retail game emits —
  and, because our own `LoadRev` reads the field it did not write, our own
  read-back of our own file **would desynchronise from that point on**. Assets
  produced by the real game are unaffected: they contain the field, and our
  reader always expected it. So this is a **write-side fidelity** fix with a
  self-round-trip consequence, not a corruption of shipped data.
* **`NewFile` capture log.** The off-by-one meant every line written to the
  file-access capture log was truncated by one byte. Behavioural, small, real.
* **`FileLocalize`.** Our version consulted a Hong Kong locale exception and
  gated the `/og/` → `/ng/` rewrite on `GetGfxMode() == kNewGfx`; retail does
  neither, and retail applies the `/og/` rewrite on top of an already-localised
  path. Native asset-path localisation now follows retail.

### 5.1 Driver coverage — measured, not implied

⛔ **No checked-in native driver exercises any of these three paths, and I did
not build a fixture for them.** Concretely: no `native/src/main_*.cpp` names
`FileLocalize` or `SystemLocale`; `RndPostProc::Save` is a `.milo` object-save
path and the only save driver is `main_save.cpp`, which is the
`FixedSizeSaveable` profile/memcard round-trip and never performs a `BinStream`
object save (W8-C established this for `RndEnviron::Save` and it is the same
gap); and the capture log is off unless `gOpenCaptureFile` is opened. The
`FileLocalize` change *is* reachable from any `NewFile(..., mode & 2)`, so a
venue-load run would execute it — but I did not run one and therefore claim no
before/after evidence. **The native gate below is a build/link check, not a
behavioural one.**

A `.milo` object-save round-trip driver remains the right fixture and still does
not exist — the same handoff W8-C left, unchanged by this lane.

---

## 6. Handoffs, in the order I would fund them

| # | handoff | evidence in hand |
|---|---|---|
| H1 | **`?Load@RndPostProc@@`: change `LoadRev(BinStreamRev&)` to `(BinStream&, ushort)`, then name `0x82430FD0`.** 1,728 B + a 192 B wrapper | §2. Signature proven from the call site (`lhz r5, 0x4(r30)`) and from 14 `??5BinStreamRev@@`→`??5BinStream@@` relocations. Also close the 188 B: drop our extra `ObjPtr<RndDrawable>` load path, `PathName`, and the `__real@` constants retail lacks; retail additionally references `fn_823A0918`/`fn_823A07A8` and four `.rdata` labels we do not |
| H2 | **Where does retail's `File.cpp` instantiate `NullFile`?** Unblocks the last 56 B of the `NewFile` port and hence naming `0x825173E0` | §3.3. Retail's `File.obj` defines `NullFile::Write` and `NullFile::ReadDone` but retail's `NewFile` does not construct one. Find the constructing function and the port completes at 424/27 |
| H3 | **`RndPostProc::Save`'s last 1,056 B + 1 fn** — 32 scratch-slot immediates | §1.4/§1.5. Full retail slot map is in the lane transcript; base uses `0x58` uniformly, retail rotates `0x54`/`0x58`/`0x5c`. Permuter-class; do not open it as a source lever without a new mechanism |
| H4 | **`FileLocalize`'s register renaming** (r31/r30/r29 → r30/r29/r28) | §4. 408/13 exact, 56 masked words, one temp-naming iteration already refuted |
| H5 | **A `.milo` object-save round-trip native driver** | §5.1. Blocks behavioural verification of every `Save` port, this lane's and W8-C's |
| H6 | **`?transform@MD5@Quazal@@`** — NOT STARTED, see §7 | — |

---

## 7. What this lane did NOT do

* ⛔ **Did not attempt `?transform@MD5@Quazal@@`** (task item 4, "if time
  remains"). Time did not remain; it is untouched and un-investigated, so
  nothing here should be read as evidence about it either way.
* ⛔ **Did not name `0x82430FD0`** — refused on structure, not on caution (§2).
* ⛔ **Did not name `0x825173E0`** — W8-D's preconditions were re-verified
  literally on this tree and all hold (`0x825173e0` free; the name placed
  nowhere else, so injective; **21 of 1,205** objects reference
  `?NewFile@@YAPAVFile@@PBDH@Z`, all out-of-line at a 480 B COMDAT). The naming
  is still **non-negative**. I did not do it because H2 leaves the body 56 B and
  6 relocations away from retail's, and W8-D's ordering rule — port, *then*
  name — is what made its own naming free. Naming now would pair a body I know
  is still wrong.
* ⛔ **Did not attempt the `RndPostProc::Save` slot rotation** (§1.5) or the
  `FileLocalize` register renaming (§4) — both permuter-class, standing
  directive defers them.
* ⛔ **Did not touch `splits.txt`, `symbols.txt`, `symbol_aliases.json`,
  `.pdata`, or `scripts/target_symbol_map.json`.** No alias group was added or
  withdrawn; I checked first that no group references `0x82430fd0`,
  `0x825173e0` or `0x82516e28`. This branch changes exactly **two** source files.
* ⛔ **Did not run any native behavioural fixture** — §5.1 states exactly why
  that would be implying coverage that does not exist.
* ⚠ **Did not re-derive W8-C's `RndEnviron` work.** Its figures were re-verified
  as this lane's baseline (§0) and its conclusions were used, not re-litigated.
* ⚠ **Did not locate retail's `NullFile` construction site** (H2) — the single
  thing blocking two of this lane's three ports from completing.

---

## 8. Reusable lessons

* ★★★ **"Retail's function F does not contain X" does not license "delete X from
  the TU."** Coupled through COMDAT emission, not through F's code. Cost here:
  −4 matched functions / −88 B on a change whose own function got *closer* to
  retail. Deleting dead-looking code deleted four matching functions (§3.3).
* ★★ **Corroborate a schema field from BOTH sides of the stream.** "Retail's
  `Save` writes a field we don't" is a one-sided reading; "`Save` writes four
  `ObjPtr<RndTex>` and `Load` reads four, and we write three" cannot be an
  artefact of how I parsed one function (§1.2).
* ★★ **A `REGISTER_SWAP` on a sub-100 row is a symptom.** Twelve prior instances
  are on record; this is the thirteenth. It dissolved on the field fix (§1.3).
* ★ **The pricing screen has THREE charge kinds, not two.** Register and symbol
  args cost `5/N`; a **differing immediate costs `1/N`** — measured exact to
  five decimals over 32 charges (§1.4).
* ★ **When the row is unpaired the score is not an instrument — use the COMDAT
  bytes, and report size AND relocation count AND the name-level multiset.**
  All three moved together here, and the multiset is what re-confirmed two
  independent identifications (§3.2).
* ★ **Ask whether the thing needing a name EXISTS as a signature your object
  emits.** `fn_82430FD0` is identified beyond reasonable doubt and is still not
  nameable, because naming is a claim about *our* symbol table too (§2).
* ⚠ **`.4byte fn_X` in a split `.s` is usually `.pdata`, not a vtable.** Check
  the enclosing `.obj` name before concluding a function is virtual.
