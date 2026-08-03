# Levers that pay — field guide from waves DC..DG

> **STATUS (2026-08-02):** current as of `362217af`. Records the **eight
> matching levers** exercised in waves **DC..DG** (2026-08-02) — each with its
> precondition, the failure mode it has already inflicted on somebody, a worked
> example with real numbers, and a LIVE / DRAINED / REFUTED verdict — plus
> **§9 targeting**, **§10 measurement**, and a drained/refuted ledger.
> **This document is prose — it cannot and did not move the metric.** No A/B
> was run for it; every number below is quoted from the landing commit that
> measured it.
>
> ⚠ **Wave headline, measured — state the range or the numbers look
> contradictory.** Over **`4c1ae369..1cbcabc8`**: `43,527 → 43,590` matched
> (**+63**, sum of the per-lane deltas 13+7+12+9+8+4+2+1+1+1+2+3), **units at
> 100% 149 → 176** of 1,024, ≈ **+15.2 KB** matched code. **§8 (DG-3,
> `362217af`) lands *after* that range** and adds **+8** ⇒ **current tree
> 43,598 matched / 176 units at 100% / one-away 110** (Font gained 8 rows
> without completing, so the unit counts do not move).
> If you have seen "≈+1,600 matched" for this wave, it is not supported by the
> commit log — the value of DC..DG is **27 whole units** and six refutations,
> not the function count.

## How to use this

**Pick a lever by symptom, not by hope.** Each section leads with the symptom
you would actually observe (a diff shape, a scanner hit, a census bucket). Check
the **precondition** — these levers are narrow, and the ones that drained did so
because someone generalised past it. Read the **trap** before editing; every
trap here is a loss somebody already ate. Then **price it with
`tools/ab_measure.py`** (`/ab-measure`), never an in-worktree incremental read
(§10). **Before targeting anything, read §9** — two thirds of the tree cannot be
finished by a source lane at all.

Control discipline — anti-vacuity guards, positive controls, null models — is
**not** duplicated here: see `docs/decomp/INSTRUMENT_DESIGN.md` (sibling lane,
same wave). This file is the *what to try*; that one is the *how to know you
measured it*.

**State tags:** **LIVE** = fund it. **DRAINED** = the mechanism is real and the
pool is empty, though the *reads* may still be reusable per-site. **REFUTED** =
the mechanism was never there; do not re-fund.

---

## 1. Storage-class divergence — **LIVE per-site**, `/D`-gate half **EXHAUSTED**

**Symptom.** A function comparing/using a `Symbol` or `Message` diffs with an
extra address materialisation, a frame delta, a prologue change — and your
source is **byte-identical to the rb3-Wii oracle**, so a source diff shows
nothing at all.

**Precondition.** rb3-Wii keeps file-scope `Symbol`/`Message` **globals**
(`Symbols*.h`, `Messages*.h`) where retail declares **function-local statics**.
Detect with **MSVC's per-function guard word** — a free source-order oracle:
one bit per local static, allocated in **declaration order**, so the target's
guard word tells you *how many* statics retail had and *in what order*. It
discriminates spellings (BandTrack's `reset` spends one bit on Symbol+Message;
`disable` spends two).
★ **Check the `/D` gate before writing any source:** `ObjMacros.h` carries two
competing definitions of the `SYNC_PROP` and `HANDLE` families, switched per-TU
by `/DRB3_SYNCPROP_LOCAL_STATIC` / `/DRB3_HANDLE_LOCAL_STATIC` in
`objects.json` `extra_cflags`.

**Trap — placement is codegen-load-bearing, not stylistic.** Inserting the
static immediately before its first use is **wrong when that use is inside a
loop**: it puts the guard test inside the loop where retail's is outside
(measured **87.04 → 75.79**). Hoist above the loop. The general rule from DE-2
and DF-3: **the guard test must precede the size()/first read.**

**Worked.**
- DE-2 `d7a9775a` — `EyebrowsProvider::none_eyebrows` is a function-local
  static in retail, a `Symbols4.h` global in ours. The REGISTER_SWAP, the frame
  delta and the prologue change were **all downstream** and dissolved with the
  one-line fix ⇒ **band3/meta_band/EyebrowsProvider 13/13**.
- DE-2 `TourGameRules` — *placement* alone collapsed 30 `diff_args` to 11.
- DF-3 `dbab6082` part B — `InqIncrementalSymbols` **77.27 → 100.0** ⇒
  `AccomplishmentTrainerCategoryConditional` 13/13.
- ★ **Used as a screen-out too:** `Asset::Asset` (49.36%) was **ruled out** —
  the target's guard word shows exactly 6 static slots and we already have 6.
  The residue is structural (frame 0xf0 vs 0xc0). A negative guard-word read
  saves a whole lane.

**State.** Per-site **LIVE**, grindy: residue **1,042 guard-bit deficit across
137 units** (`tools/guard_funclet_census.py --deficit`); **235** reachable
per-site at ≈1 match each, 75 in source-less `auto_03_*`.
⛔ The **`/D`-gate sweep is EXHAUSTED, measured not assumed**: 14 further TUs
with macro sites and no gate compiled to **Δmatched −2 / Δcode% −0.008610** —
two losses, zero gains. **Deficit is a good SIZER, not a predictor of `/D` yield.**
★ Sign surprise: MSVC emits a 32-byte `??__F` funclet per local static
**regardless of destructibility**, and those are target functions we did not
previously emit — so an added static often brings a **free extra match** (38 of
CX-1's first 48 gains were funclets). Price per site; assume neither sign.

---

## 2. `LOAD_REVS` macro dialect — ⛔ **DRAINED** (one family, not a vein)

**Symptom / precondition.** A `Load`/`PreLoad` materialises a `BinStreamRev` on
the stack where retail **stores** to rev words (`sth r10,0x0(r29)` /
`sth r11,0x4(r29)`) and reads them with `lhz`+`cmplwi`. Two incompatible
definitions resolved **per-TU by include order**: `ObjMacros.h:605-618`
(rb3-Wii) file-scope **mutable** rev words ⟵ *retail*; `Object.h:1606-1614`
(DC3) static **const** + a `BinStreamRev` local. A `static const` cannot be
stored to, so retail's stores settle the dialect.

**Worked (the exception).** DE-1 `c14bba5c` — the FxSend family, **+8 matched /
+2,212 B**, 8/8 Loads to 100%, **five units complete** (Chorus 18/18, Compress
19/19, EQ 21/21, Delay 18/18, MeterEffect 42/42). ★ Δbytes equalled the sum of
the 8 target sizes **to the byte**. Three **coupled** sub-fixes, each
independently necessary: (1) dialect swap (Chorus 51.4 → 88.7); (2) single-base
`+0/+4` addressing — retail folds both rev words onto one base, which happens
**only** for internal-linkage `align(4)` file-scope statics, not class statics
(→96.5); (3) chaining **per-site, not family-wide** — Chorus and Delay needed
`bs >> mTempoSync; bs >> mSyncType;` split in two, Wah did not (→100).

**Trap — and this is the whole story of the lever.** ⛔⛔ **DF-2 swept it and it
is empty:** full 33-TU sweep **−16 matched / −0.012573pp / −1,344 B**; reduced
to the best 9 settled improvers, **exactly +0**. Scanning by *source call-site*
(not by rows named `Load`, which misses every `PreLoad`) gives **272 TUs and 55
real DC3-dialect sub-100 bodies / 29,528 B** — 2.75× the original estimate, with
yield ≈ 0. **Pool size was never the constraint.**
★ **The byte-sum check caught it:** predicted +4,236 B from 17 bodies reaching
100, actual −1,344; DE-1's Δbytes matched its target-size sum exactly. **Use
that check in both directions.**
★ **The separating gate:** `operator>>` **template instantiations in the TARGET
unit** are positive evidence retail used the `Object.h` dialect — deleting the
`BinStreamRev d` local stops our obj emitting them and they fall 100 → 0. Splits
the population **13 UNSAFE / 25 SAFE**. ⚠ **Necessary but not sufficient**;
several SAFE units regressed anyway, which is why the reduced patch paid +0.
⚠ **Never copy field order between targets.** Every DF-2 target is
**altRev-first**, the *opposite* of DE-1's struct.

**State.** ⛔ **DRAINED** — FxSend was the exception, not the head of a vein.
The three sub-fixes stay reusable *individually* on a target already proven
DC3-dialect by retail stores.

---

## 3. `SAVE_OBJ` stub Saves — ⛔ **DRAINED**, accounting closed; the **chaining lever is LIVE**

**Symptom.** Our body is a single `blr` (base size 4) against a real-bodied
retail target: `SAVE_OBJ(T, line)` expands to
`void T::Save(BinStream&){ MILO_ASSERT(0, line); }`, and `MILO_ASSERT` is
`((void)(cond))` in the match build — **but retail implements these.**

**Precondition + three mechanical reads (reusable on any Save/Load port):**
1. **The rev constant is free.** The `li rN,K` feeding `WriteEndian` is
   **relocation-free** and hands you the revision directly — held **11/12**.
   ⚠ Exception: `RndEnvAnim` emits no immediate at all (`lwz r11, lbl_82C70A34`,
   an address in **writable `.data`** holding big-endian 4) ⇒ the rev is 4 but
   lives in a **mutable global** — §1's family inside this one. Progression:
   literal `4` = 68.7% → `static int gSaveRev` = 83.6% → +chaining = **100.0**.
2. **The implemented `Load`/`PreLoad` is the EXACT MIRROR** of the missing
   `Save`, **including field order** — held **12/12**, verified independently by
   offset spacing (BandCharacter −596/−544/−528/−520 vs header +0/+52/+68/+76).
3. ★★ **THE CHAINING LEVER.** Whether retail wrote `bs << a << b << c` as one
   full-expression or as separate statements is **decidable from stack-temp
   usage**, since temporaries live to the end of the full-expression:
   **distinct consecutive temps ⇒ ONE chained expression; reused temps ⇒
   SEPARATE statements.** For object writes (no temps) the tell is
   **precomputed addresses in registers**.

**Trap.** ⛔⛔ **A callee NAME can never witness a member-type mismatch.** DC-3
deferred `LayerDir` because "retail streams `list<Symbol>` but our member is
`list<Layer>`". Misdiagnosis: the callee name is a **relocation argument**, and
`functionRelocDiffs=none` masks it — the read is *structurally incapable* of
witnessing a type mismatch. Retail's writer had simply ICF-folded with
`list<Symbol>`'s. No member-type change was needed; **LayerDir landed at 100%.**
Same artifact: `BandIKEffector` calls `?Save@FlowValueCase@@` for
`CharWeightable` and matched **100% untouched**. Generalise: *"the callee it
calls proves our type is wrong"* is **void under the default ruler**.

**Worked.** DC-3 `ebca36e8` **+7 / +668 B** (TrackPanelDir, OvershellDir,
BandButton, UnisonIcon 4.55→100, Fader 3.57→100, CheckboxDisplay 1.74→100,
MicInputArrow 3.57→100) then DD-2 `81d23046` **+12 / +2,760 B**, both predicted
exactly before running, zero regressions, unit-net == whole-binary Δ.
★ **StarDisplay 83.8 → 100.0 on the chaining read alone**; BandLeadMeter's
precompute groups of {3},{2},{4} reproduced its Load's chaining and matched
first try; RndEnvAnim closed on a 4+1 split.

**State.** ⛔ **DRAINED, accounting closed.** DC-3's census was 23 rows:
**20 at 100%**, **1 tractable-but-expensive** (`OutfitConfig`, rev 27 — the one
known-recoverable +1 / +248 B), **2 that were never Saves** (`Save@BandHighlight`
is a 1,140-byte *Load* ⇒ map mispairing, unexamined; `PostLoad@UIComponent` is a
genuine PostLoad). ⚠ Scoped to that enumeration, **not re-derived at HEAD**:
18 `SAVE_OBJ` strings survive, none carrying the census signature (*paired,
real-bodied retail target vs a tiny stub*).

---

## 4. Coupled halves & boundary **MOVES** — **LIVE**

### 4a. Decompose every time — a negative half is not a wrong half

**Precondition.** Any fix whose correctness spans two artefacts: source +
`scripts/target_symbol_map.json`, or `splits.txt` + map.

**Worked.** DF-1 `369273db` (RndFontBase collapse) — **the halves multiply, an
+8 swing:**

| leg | Δmatched | Δcode% | Δbytes |
|---|---:|---:|---:|
| source-only | **−3** | −0.008831pp | −944 |
| map-only | **−4** | −0.009243pp | −988 |
| **combined** | **+1** | +0.000408pp | **+44** |

Sum of halves −7 vs combined +1. Cause: 7 map entries hard-code `RndFontBase`
**inside their mangled names** and four were at 100% (988 B), so collapsing the
class renames them and they stop pairing.
⇒ ★★★ **A +0 or negative half is NOT evidence that half is wrong.** ⚠ Nor is
the converse a law — CW-4's halves summed **exactly** (47+1 = 48). **Measure
source-only, map-only, splits-only AND combined, every time.**

### 4b. Boundary MOVES, not trims

**Precondition.** A pin over-reaches into the neighbouring unit.
★ **A MOVE re-attributes bytes; a TRIM deletes them from the denominator.** Move
the boundary and the bytes stay pinned, they just change owner ⇒ **Δtotal_code
0**, a correct attribution rather than a denominator trick. A trim shrinks
`total_code` and can "complete" a unit at Δmatched 0 by making the problem
un-measured — that is metric-fitting, and it is why DE-3's and DE-4's trim
proposals were converted to moves or refused. **Assert byte conservation per
pair BEFORE measuring** (DF-4: 220+860 == 176+904; 1644+3940 == 1552+4032).

**Trap.** ⛔ **"The class is foreign to this unit" is NOT sufficient grounds for
a trim.** `HitSink` is a **header-only base**, so its inline ctor is a COMDAT
**any** TU may emit — DE-3 read the evidence correctly and recommended the wrong
remedy; a trim would have deleted bytes belonging to the neighbour. Ownership is
settled by the *neighbour already owning the sibling symbol*. And it had to land
**coupled**: `47907c6f` moved the boundary *and* added the map row, because the
splits half alone hands HitTracker a 13th unmapped-anon row and **drops
HitTracker off 100** — unit-count neutral, one unit strictly worse.
⛔ **Three "candidates" were structurally impossible**: single-function units
with matched=0, where the move **drains the unit, which must then be deleted —
the unit VANISHES** rather than reaching 100%. Test for this first.

**Discriminators, ranked by what actually fired:**
- ★ **vtable membership via retail RTTI COL** — decisive where it fires
  (`fn_826E34D0` installs vtable `0x820F1400` → COL `0x821E82F0` →
  TypeDescriptor `.?AVHitSink@@`). ★ **xref-caller-to-pinned-unit** likewise.
- **Pin-truncation detection** — high-precision / low-recall: **1 of 21 fired
  and it was correct** (SfxMap→ADSR: the pin physically cut the function 28 B
  short of its terminator).
- ⚠ **"Donor block is an island inside the receiver's blocks" is suggestive,
  not proof** — `/Gy` COMDATs let the linker interleave TUs. Use only with
  vtable or xref evidence.
- ⛔⛔ **"Blocker sits at a pin tail" carries ~zero information**: 53.0% treated
  vs **42.3% by chance = 1.25×**, and 12 of 66 are single-function units where
  "tail" is guaranteed. Consistent with over-reach *and* with a unit's own last
  function merely lacking a map row — it cannot separate them.

**Worked.** DF-4 `5f7bf5b4` — Biquad, Server; Δmatched 0, Δtotal_code 0, units
**161 → 163**. DG-2 `1cbcabc8` — **8 moves, units 168 → 176**, matched/
total_code/total_functions all **unchanged**. The units that complete are the
**donors**, which shed a foreign body.
★ **Refusing to hand over a name is a result.** DG-2 declined map rows for three
donor-owned cases because `StoreSongSortNode`'s vtable is riddled with ICF
fold-aliases (slot 21 names a `_List_iterator` *constructor*, impossible for a
virtual slot) — deriving a name from that **launders a fold-alias into the map.**

**State.** **LIVE.** ~23 further candidates remain
(`boundary_move_candidates.json`), each needing its own RTTI adjudication.
DG-2's hit rate on DF-4's shortlist was **8 of 23 (~35%)**.

---

## 5. Per-instantiation inlining traits — **LIVE (thin)**; regswap is a **SYMPTOM**

**Symptom.** N instantiations of one template share an **identical** surplus at
an **identical mpn** — the tell of a single shared cause.

**Precondition.** MSVC `/O1 /Ob2` decides inlining **per instantiation**.
Retail's `ObjRefConcrete<T1,ObjectDir>::Load` no-dir path is one unconditional
`SetObjConcrete(0)`; MSVC inlines it for **non-virtual-base `T1`** and **not**
for **virtual-base `T1`** — inlining would have to emit the vbase displacement
lookup to form the `Hmx::Object` subobject for the virtual `Release`. **Our
compiler inlines neither**, so *no single source form serves both*:
`SetObjConcrete(0)` fixed 9 rows and drove the other 18 from 100% → ~92%;
open-coding did the reverse. Encode as a **compile-time trait** — the house
pattern (`ObjPtr_p.h` already carries an explicit `RndParticleSys`
specialization for the same reason). Both arms are the same operation ⇒
semantics-preserving.

**Trap.** ★ The predicate needs a **TRANSITIVE virtual-base closure** (1,457
classes, separates the population **27/27**). The direct `class X : … virtual`
grep gives **3/9** — a false negative that reads as *"the vbase rule is wrong."*

**Worked.** DD-3 `5d8fc966` — **+9 matched / +2,088 B** = 9 × 232, the full
target size of all nine rows; zero regressions; unit-net == whole-binary Δ.

**★★ The counter-finding is worth more than the +9.** objdiff labelled the row
**`REGISTER_SWAP (MaybeFixable)`** with a **13-instruction r30↔r31 swap** —
under the standing permuter ban that reads as *"dead, drop it."* **The swap
dissolved on its own when the real source defect was fixed.** It was downstream
register pressure.
⇒ ★★★ **Treat a regswap label as a SYMPTOM, not a diagnosis.** Look for a
source cause first; classify as regalloc only after one is ruled out.
⚠ **It is a count, not a rate.** The wave produced **four** instances (DD-3's
13-instruction swap; DE-1's 24-instruction Chorus swap; DE-2's EyebrowsProvider
prologue/frame/swap bundle; DC-3's F-cluster, graded REGISTER_SWAP where the
cause was inline policy) — but **nobody has measured what fraction of
regswap-labelled rows have a dissolvable source cause.** The 184
`register(permuter)` rows in the mpn/fuzzy-gap census were all dropped on this
label, unexamined. That measurement has not been run.

**State.** **LIVE but thin.** No second multi-row lever of this shape is
visible: of 29 split templates (8,141 B), `ObjRefConcrete::Load` was the **only**
one whose failing side had a **single distinct mpn** — every other shows
scattered mpn, i.e. per-instantiation causes. ★ **"Single distinct mpn across
the failing side" is the discriminator for a shared-cause lever.** Class E
overall: **553 rows / 34,513 B = 0.3229pp** if every row converted, and not
concentrated (top 25 = 33%).

---

## 6. `ObjPtr` ctor ordering — the one-line dead store — ⛔ **DRAINED at the shared site**

**Symptom.** A 3-instruction rotation around the vtable store, **diff score
exactly 126**, identical registers across unrelated units:

```
retail: stw mOwner ; lis VT ; stw mObject ; addi VT ; stw vptr
ours:   stw mObject ; stw mOwner ; lis VT ; addi VT ; stw vptr
```

**Precondition.** Our `mObject` store is a **free constant store** that floats up
into the `addi`→`stw` **load-use stall**, filling the wrong gap. Fix: a redundant
`this->mObject = ptr;` **in the ctor body** kills the base mem-init's store, so
the surviving one lands after the vtable.
★ **Found structurally, not by spellings** — a shape scan over all non-stale
target asm found **124 sites in retail's order and ZERO in ours**, which is what
made it a **defect rather than a per-TU policy** and justified going ungated.
The 2 rows already at fuzzy 100 were the control group.

**Traps.**
- ★ **The body is written out THREE TIMES in the tree.** Commit 1 reached one,
  which is why `ScrollbarDisplay` and `TrackPanelDirBase` did not move until
  commit 2.
- ★ **There is a THIRD ordering.** `??0RndEnviron@@IAA@XZ` regressed
  **94.740 → 94.221**, **kept and recorded, not absorbed**: Env's owner is
  `this` directly, so there is no vbase `addi` and **no load-use stall exists** —
  retail hoists the `lis` a slot earlier. The fix moved it between two wrong
  orders.
- ⚠ **The first shape scanner returned ZERO HITS and was silently vacuous** (its
  regex expected one space where the asm has two). *Shaped exactly like a
  decisive negative.* See INSTRUMENT_DESIGN.md.

**Worked.** DC-2 `1ed4b1e8` — **+13 matched / +4,816 B / +0.045060pp**;
whole-binary per-function control vs pristine: **13 gained mpn-100, 0 lost.**

**State.** ⛔ **DRAINED at the shared site** — the 124-site shape scan closed
the population. The remaining ~10 multi-site class-C ctors sit at **72–87% with
heavy unrelated residue**, so converting one `ObjPtr` site **cannot cross 100**
there. The Env third-ordering variant is unworked.

---

## 7. Container member types (`std::map` vs `hash_map`) — ⛔ **DRAINED with a control**

**Symptom.** A member declared `std::map<Symbol,V>` where retail's calls resolve
to `hash_map` (or vice versa). **The metric will never tell you** (below).

**Precondition + governing fact.** **`sizeof(map) = 0x18`, `sizeof(hash_map) =
0x1c`** (re-verified with a `/d1reportSingleClassLayout` probe struct; also
`set` 0x18, `vector` 0x0c, `list` 0x08, `deque` 0x28) ⇒ **a container swap is
layout-safe only for TRAILING members**; a mid-class swap shifts everything
after it by 4.

**Traps — three, all expensive.**
- ⛔⛔ **`mpn` can NEVER witness a member-type bug.** It masks relocation
  arguments, so a function calling the *wrong* container's `operator[]` scores a
  clean **100**. `SetPropertyValue` read **100% before AND after** the fix.
  ⇒ **Expect correct fixes to be Δmatched 0 and land them anyway** — a metric
  that hides a real bug is worse than a lower metric. (Fifth independent
  confirmation of this blind spot in five waves.)
- ★★★ **An improvement can be a false positive.** DF-3 called `Campaign` a
  defect first, on a "confounder-immune counting argument" that **was itself a
  confounder**, and the swap **improved** the ctor **73.30 → 89.00**. Only
  retail bytes refuted it: its 472-B
  `_Rb_tree<Symbol,pair<const Symbol,Symbol>>::insert_unique` **matched retail at
  100%**. `BandProfile` looked positive too and measured **−73**.
- ★★★ **Naive value-type matching is UNSOUND UNDER ICF.**
  `clear@_Rb_tree<…CharLipSync*>` and `_M_find@_Rb_tree<…bool>` appear in **nine
  different units** — a folded symbol's value type is **arbitrary**. Only
  **non-folding, instantiation-specific** symbols carry usable evidence:
  `_M_create_node`, `insert_unique`, `_M_insert`.

**Worked.** DE-2 `d7a9775a` — `TourWeightManager::unk4`, adjudicated on retail
bytes (the target's own `ConfigureQuestWeightData` calls
`??A?$hash_map@VSymbol@@M…::operator[]`). DF-3 `dbab6082` — **+2 matched /
+1,044 B** over 3 fixes: `TourPropertyCollection`, `LessonMgr` (ctor
**55.52 → 100.0**), `NextSongPanel`. ★ `NextSongPanel.h` carried
`int unk58_retailpad` commented *"likely a std::map node header being 0x1c"* —
**that pad WAS the hash_map**. Someone had patched the *symptom* with filler, so
the class laid out correctly **by accident** while the type stayed wrong.

**State.** ⛔ **DRAINED, with an exhaustive control.** DG-4 scanned 1,635
headers: **TIGHT compensating-pad signature count 0** against a base rate of
**1,948 pad-like members**; widening past pads, **46 Symbol-keyed 0x18-container
members across 29 units** adjudicated on retail `bl` evidence ⇒ **zero defects**.
★ ~20 `std::set<Symbol>` members closed as a class — retail has **zero
`hash_set` symbols (0 / 27,952 named)**. ★★ Untreated-population control:
**20.0% of container-calling units (43/215) are hashtable-only**, so "this unit
calls hash_map" fires on 1 in 5 units by default, independently confirming the
measured **~75% false-positive rate**.

---

## 8. The retail-shape LAYOUT PORT — **LIVE**

**Symptom.** A whole class's members sit at the wrong offsets because our source
inherited the **DC3-newer shape** where retail is the **rb3-Wii shape**. Several
large named rows in one unit stall together in the 30–90% band and no per-row
edit crosses 100.

**Precondition.** Adjudicate the shape on **retail evidence** — an RTTI/COL
decode plus a `Save` disassembly read against
`scripts/harvest/class_layout_report.py` (`/d1reportSingleClassLayout`).
⛔ **Never the `// 0xHEX` header comments** — they are a documented lie class.

**Worked.** DG-3 `362217af` (RndFont, the vein DF-1 named but could not work) —
**+8 matched / +1,272 B / +0.011901pp**, zero regressions, all in `default/Font`.
Layout compiler-verified **12/12** against a retail decode the lane performed
**itself rather than trusting its brief**: `sizeof(RndFont) == 0x94`, `mMat@0x28`
… `mNextFont@0x88`. Per function: `Save` (548 B) 66.9 → 100 · `Print` (352 B)
46.2 → 100 · `CharDefined` (152 B) 86.1 → 100.
★ **Byte-sum check EXACT:** 548 + 352 + 152 + 5×44 = **1,272 = Δcode_bytes**.
★ **The lane corrected its own brief on retail bytes:** `CharInfo` is **20 B, not
16** — `_M_erase` deallocates `li r3,0x28` (a 40-byte node) ⇒ the pair is 24 ⇒
`CharInfo` is 20, and the fifth word is a *trailing* unknown, **not** DC3's
leading serialized `mPage`. Adding it restored `_M_erase` to 100 while
`Save`/`Print`/`CharDefined` stayed at 100 — **four retail bodies satisfied at
once**, which is the real confirmation.
★ **It predicted a member the brief never listed** — `mTexCellSize@0x7c`,
confirmed by retail reusing the same `Vector2` helper as `mCellSize`.

★★★ **A layout change perturbs WHICH anon rows pass pairing — in BOTH
directions.** The fork's pass 3 requires **identical size AND ≥50% masked byte
equality**, so moving members moves rows across that threshold. The same commit
**newly paired 5 anon rows** (32 → 37) *and* drove `BitmapLocker::ctor`
**32.6 → 0**. Read the rulers together: Δmatched **+8** but Δhonest only **+3**,
because `masked_equal` went **+5** — **those five are the newly-paired anon
rows.** The pairing change and the disclosure delta are the same event, so
**quoting +8 alone double-counts against honest.**
⇒ ★★★ **A ceiling is a SNAPSHOT of pairing state, not a bound on it.**

⛔ **What this is NOT: "DG-3 beat the census."** That claim circulated and is
**false against the tool.** DF-1's ceiling of 70 came from the **superseded
two-way anon model** (reachable = 64 matched + 6 named sub-100; anon-paired
sub-100 rows counted as unreachable). The corrected **three-way** model
(named / anon-paired / anon-unpaired — the one DF-4 established when it found
**1,696 anon rows strictly between 0 and 100**) puts `default/Font` at **81**,
and **72 lands comfortably inside it.** "Beat the ceiling" was true of the
*brief* and false of the *tool*.
★★ Keep the general form: **a ceiling quoted from a superseded model is
indistinguishable from a measured one.**

**Traps.**
- ⛔ **A half-done layout change is worse than none** — tree-wide blast radius.
  DF-1 correctly declined to start one it could not finish.
- ★ **Do not "fix" the vtable to match.** Retail's `CharDefined`/`Print` mangle
  `U..B..` (public **virtual** const) where rb3-Wii has them non-virtual: RB3-360
  is **Wii-era members under a DC3-era vtable**. Changing the signatures unpairs
  the map rows outright.
- ★ **Devirtualising to remove a hack can add a call retail does not make.**
  `Text.cpp`'s `+0x30` hack was *not* removed by calling `Mat()` — `Mat()` is
  virtual, and retail does a plain `lwz 0x30`. The fix was rb3-Wii's
  **non-virtual `GetMat()`**. (That half measured **exactly Δ0** and was kept
  anyway — §4a.)
- ⚠ **Price the partial credit you destroy.** `BitmapLocker::ctor` went
  **32.6 → 0**: removing the DC3 multi-page structure cost its partial fuzzy,
  and the lane said so plainly rather than netting it away. It is scored against
  an unrelated function and could never have matched.
- ⛔⛔ **THE MATCH BUILD NEVER LINKS**, so it is *structurally blind* to link
  breakage — an undefined template instantiation compiles clean and ships.
  **Twice this session a landed decomp change broke the native build:** DD-2's
  `RndEnvAnim::Save` needed an `ObjOwnerPtr` save instantiation (`dce343a1`), and
  DG-3's `RndFont::Save` is expected to need plain `ObjPtr<T>` `operator<<`
  instantiations in `native_link_glue.cpp`. **A new `bs <<` over a smart-pointer
  member is the recurrence signature** — check it before landing.

**State.** **LIVE.** The instrument (retail decode → compiler layout report →
byte-sum check) is general; the pool is however-many classes still carry DC3
shape, which nobody has censused.

---

## 9. TARGETING DISCIPLINE — where to point a lane

### ★★★ 9.0 DO NOT RANK BY "PENALTY". Rank by SIZE-IF-IT-CROSSES.
Lane DN-4 (`2a851aec`, 2026-08-03) — this corrects an instruction that was in
every source brief for ~8 waves, including in this file's own framing.

**`matched_code` is ALL-OR-NOTHING PER ROW.** Verified at HEAD rather than taken
from a doc: `Σ size where fuzzy == 100` **equals `matched_code` exactly**
(4,185,280, Δ0), and `count where mpn == 100` **equals `matched_functions`
exactly** (43,668, Δ0).

⇒ **A partial improvement on a row pays LITERALLY ZERO.** A 7.5 KB row sitting at
fuzzy 99.8 carries only ~14 B of "penalty" but is worth **7,504 B the moment it
crosses**. So *"sort by penalty (bytes at stake)"* points a lane at 3 KB altivec
bodies stuck at 0% — the rows with the most penalty and the least chance of
crossing — and **buries the actual vein.**

| band | rows | value if crossed | share of value | share of penalty |
|---|---:|---:|---:|---:|
| all source-addressable | — | 843,012 B / 7.89pp | 100% | 100% |
| **mpn<100 at fuzzy ≥ 95** | **412** | **206,240 B / 1.930pp** | **24.5%** | **1.9%** |

⛔ **CORRECTED 2026-08-03 by lane DO-1 (`1457aa52`) — the first version of this
table said "46% of the value at 4% of the penalty" and that DOES NOT FOLLOW FROM
ITS OWN ADJACENT NUMBERS.** 213,744 / 850,620 = **25.1%**, not 46%; measured
fresh at HEAD it is **24.5%**, and the penalty share is **1.9%**, not 4%. The
figure came from a lane report and I propagated it into this file without
checking it against the total in the very same row — **the fabricated-baseline
hazard, committed in the document written to prevent it.** Rule 15 applies to the
author too.

★ **The conclusion is unchanged and still strong: ~13× value-per-penalty makes
this the best ROI band in the tree.** Rank by the row's SIZE, gated on it being
close enough to cross. Two rows drawn from it paid **+0.070205pp** and
**+0.107776pp** — each larger than most entire waves.
⚠ Band size itself is stable: 412 rows / 206,240 B measured against 413 /
213,744 briefed, within 3.5%. It is the *derived share* that was wrong, not the
band.
⚠ Corollary for a *unit*-completion lane: this is a **byte/function** ranking. A
7.5 KB row crossing may move zero units (DN-4's did). Pick the ranking that
matches what you are being scored on, and say which you used.

### 9.1 The reachable-ceiling partition

⏱ **Live as of 2026-08-02 — check the tree before re-funding any of this.**
Wave **DI** is working these follow-ons right now: **DI-1** the mislabelled
MAP_ONLY source-workable pool (the 31 below), **DI-2** the COMPLETABLE bucket,
**DI-3** map/splits cleanup plus the native `ObjPtr<T>` unblock from §8.

★★★ **Read this before selecting targets.** The reachable-ceiling census
(DF-4 `5f7bf5b4`, all 1,024 pairable units; `TARGETING.json`,
`reachable_ceiling_census.json`):

| bucket | units | meaning |
|---|---:|---|
| AT_100 | 161 | done |
| **COMPLETABLE** | **85** | zero unmapped-anon rows — **pure source work reaches 100%** |
| MAP_ONLY | 196 | *only* unmapped-anon rows left |
| MIXED | 572 | needs both source work and map rows |
| OD_REGION | 10 | pin overlaps the `/Od` region |

⇒ **The source-only ceiling on "units at 100%" is 246 / 1,024 (24.0%).** Of the
127 one-away units only **61** are source-reachable. **Any wave targeting a unit
outside {AT_100, COMPLETABLE} spends source budget on something that cannot
finish.**
⚠ **Open, and not re-derived: this partition is a two-way anon treatment too.**
A unit lands in MAP_ONLY if its leftovers are unmapped-anon — but the
**anon-paired sub-100** rows among them are exactly the class DG-3 showed a
layout fix *can* move (§8). If that applies at unit scale, **246 is a floor, not
a ceiling.** Nobody has recomputed the partition under the three-way model;
treat the number as a targeting heuristic, not a proof of unreachability.

**The anon gate, ground-truth and non-metric:** a sub-100 anon row is precisely
**"a retail address absent from `scripts/target_symbol_map.json`."** Mechanism
(fork `diff/mod.rs:815`/`1410`): a target `fn_<8hex>` can only pair with a base
symbol satisfying `is_funclet_like`, and a retail method compiles to a mangled
name ⇒ no pair forms. Demonstration: `Pool.s` has four `fn_` functions; the
three in the map match at 100%, the absent one reads 0.

**Corrections that must travel with the census — it is not a licence:**
- ⛔⛔ **The "65 units one map row from 100%" headline is 21× optimistic.** DG-1
  `bce10a25` worked the set and shipped **3 of 65 (4.6%)**.
- ⛔⛔ **31 of the 65 are NOT map-only at all** — our source *does* contain the
  method, only the body diverges. They are the **most valuable follow-on** and
  are parked in a bucket telling source lanes to skip them.
  ⇒ **Read MAP_ONLY as "the leftover is anon", NOT as "unreachable by source."**
- ⛔ **"Anon bodies can never pair" is too strong** — **104 of 161 complete units
  contain an anon `fn_` row at 100%** via funclet byte-signature pairing. The
  true statement is **"anon residue is not reachable BY SOURCE EDITS."**
- ⚠ **The census cannot detect a bogus pin.** `HamDriver`, `FilterQueue` and
  `SkeletonDir` sit in COMPLETABLE and are exactly the bogus-pin / metric-fitting
  cases already rejected on evidence.
- ⚠ **A census ages the moment anything lands** — 3 of the 65 were already at
  100%, fixed by DF-4's own boundary moves after the JSON was written.
- ★★★ **A ceiling is a SNAPSHOT of pairing state, not a bound on it.** A layout
  fix moves anon rows across pass 3's threshold **in both directions** — DG-3
  newly paired 5 and destroyed 1 (§8). ⛔ It did **not** "beat the census": the
  70 it was measured against came from the **superseded two-way anon model**;
  the corrected three-way model puts Font at **81** and 72 is inside it. ★★ **A
  ceiling quoted from a superseded model is indistinguishable from a measured
  one** — check which model produced any ceiling before you trust or exceed it.
- ⚠ **Two rulers at unit scale:** "all rows `mpn`==100" gives **161**, "all rows
  `fuzzy`==100" gives **149**. Say which you mean.

★★ **An arg-only anon row is worth naming even though it pays +0 bytes.** DG-1
reproduced the two-rulers split deliberately: the byte-exact `ClosetPanel` row
paid **all 560 B**, while `SIVideo` and `ColorXfm` paid **+1 matched each with
+0 B** (`mpn` excludes arg-only penalties). Predicted +3 / +560 B before
measuring; confirmed to the byte.
⛔ **Retire, do not re-fund, the 6 "structurally completable" rows** — 8–16 B
with 1–3 real non-relocated words, so each **fails the anti-vacuity guard** while
having 1–25 same-size candidates. No evidence at that body size can change it.

---

## 10. Measurement, in one paragraph

★★★ **An unsettled in-worktree `report.json` read is not a weak measurement, it
is a WRONG one — and it is shaped exactly like success.** DF-2 read **+23
matched / 17 bodies at 100%** from apply-revert cycles inside its worktree; the
settled whole-binary A/B showed **zero bodies at 100** and most **worse**
(`CharPosConstraint` 82.25 → 34.48, `RndLight` 80.44 → 37.10). Cause: dirty-obj
contamination. **Only `tools/ab_measure.py` produces a number worth quoting** —
settle-to-zero-work, report+cache wiped per read, ruler pinned. Sharper than
"deltas compose, absolutes do not": here the **sign** was wrong.
⛔ And remember what a *clean* A/B still cannot see: **the match build never
links**, so link breakage is invisible to every number in this file (§8).

---

## Drained / refuted ledger

| lever | verdict | sizing |
|---|---|---|
| `LOAD_REVS` dialect sweep | ⛔ DRAINED | 33-TU sweep **−16 / −1,344 B**; best-9 **+0**. 55 real DC3-dialect sub-100 bodies / 29,528 B exist and yield ≈0. FxSend (`c14bba5c`, +8) was the exception |
| `SAVE_OBJ` stub Saves | ⛔ DRAINED | accounting closes: 23 rows = 20 at 100% + 1 expensive (`OutfitConfig`, +1/+248 B) + 2 never Saves. `ebca36e8` +7, `81d23046` +12 |
| Container member types | ⛔ DRAINED (with control) | 46 Symbol-keyed 0x18 members / 29 units ⇒ **0** further defects; TIGHT pad signature **0** vs base rate **1,948**; ~75% FP rate on the naive scan |
| Shift-amount "struct-size oracle" | ⛔ **REFUTED TWICE** | BZ-3 (2026-07-30), re-refuted DD-1 `78e19b99`: **0 struct-size defects in 14 rows** (7 anon byte-fallback, 6 map mispair, 1 not-a-defect); **five physically impossible** (e.g. `~vector<pair<T*,float>>` retail 64 vs ours 8 — 4+4 by the language) |
| Boundary-move **pin-tail filter** | ⛔ near-uninformative | 53.0% treated vs 42.3% chance = **1.25×**; 12 of 66 are single-function units where "tail" is free. Use RTTI/xref/pin-truncation instead |
| `Ease*` map rows | ⛔ proven false, deleted | DC-1 `eda76311`: **39 rows deleted** (11 `Ease*`; `EaseLinear` retained — a bare `blr`, not disprovable). Δ0 on **both** headline rulers, **−0.014467pp fuzzy** — credit never earned |
| ICF alias **withdrawal** | ⛔ REFUTED — **RETAIN 7 of 8** | DD-4 `b206d005`: *"cannot demonstrate the fold"* is **not** grounds to withdraw. The condemning test **would reject the survivor as an alias of itself** (vacuous on that stratum). Only `??1?$ObjRefConcrete@VRndFont@@` withdrawn |
| Sibling-stride **repoints** | ⛔ declined | DD-1's 6 candidates measured **−2 with zero gain**. ★ **Byte identity proves which retail body you EQUAL, not which is your HOME** — the home-unit gate killed 3 that passed a strong byte test |
| Pin over anonymous functions | ⛔ cannot pay (and that is fine) | `SessionJobs_Xbox.cpp` 31-function span: **Δ0 exactly, as predicted**, `total_code` unmoved ⇒ free attribution, zero dilution. Real yield needs a naming pass |
| Plain `??_E` (vector deleting dtor) rows | ⛔ **UNPAIRABLE BY CONSTRUCTION** — sized, closed | DJ-4: **50 rows / 3,768 B / 0.035252pp**, and the "17× vs `??_G`" disparity is a **symbol-form artifact, not codegen**. MSVC emits plain `??_E<Foo>` as a **bodiless COFF WEAK-EXTERNAL ALIAS to `??_G<Foo>`** — our tree has **10,161 alias records vs 16 real `??_E` bodies** — so a target row spelled `??_E<Foo>@@UAAPAXI@Z` has nothing on our side to pair with. Respelling `??_E`→`??_G` pays **0**: 4 candidates blocked (`??_G` already in the map **and paired at 100** ⇒ our single body is spent), the other 9 have **no `??_G` body in that unit's obj** |

**Also standing, from earlier waves:** the `/D`-gate half of §1 (14 TUs, **−2**);
AT_LIMIT re-triage (yield 0, never re-fund); the `base_path` vein (**+0**).

★ **The `??_E` case is also a clean rule-4 specimen** (one-label classifier): the
disparity is only visible once `??_E` is split by **mangled form**. Plain `??_E`
(weak aliases, no body) passes **1/51 = 2.0%**; `??_E` **thunks** (`W…`/`$4…`,
which ARE real definitions) pass **379/386 = 98.2%**. Same mangling family,
opposite storage class, opposite outcome — the thunks are the known-opposite case
that turns "plain `??_E` is sick" from a bare rate into a finding. Against a
size-matched null (named plain rows 40–130 B, **17.0%** sub-100) plain `??_E` is
**5.8× the null** while `??_G` is **0.36×**, i.e. *healthier* than average.
⚠ And do not adjudicate these on masked bytes: deleting destructors are
shape-identical, so a masked comparator returns **7 exact candidates** for one
76 B body. The **resolved (unmasked) `bl` target** — which names the class whose
`??1`/`??_D` the body calls — is what discriminates, and it shows **18 of 51 rows
name a class the callee contradicts**.
