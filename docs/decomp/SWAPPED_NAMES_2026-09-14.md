# Two "swapped" map names: refuted as stated, replaced by a 6-cycle

**Lane W14-C, 2026-09-13.** Branch `w14-swapped-names` off `90be524c`.
Worktree `~/tmp/wt-w14-c`.

W13-B flagged two map rows as *possibly swapped* and deliberately did not act
(`docs/decomp/FLOAT_AND_ARGORDER_2026-09-13.md`, "Side findings"):

- `0x8252f3e8` is named `??0SigninChangedMsg@@QAA@KK@Z` but has the exact 2-arg
  RG-message ctor body, is called from the connected-accessories position in
  `Poll`, and sits on `UsbMidiGuitarMsgs.cpp`'s `.text` **end** boundary.
- `RGPitchBendMsg`'s ctor at `0x8252ee20` calls `0x8252e6b0`, which the map
  names `?Type@RGConnectedAccessoriesMsg@@SA?AVSymbol@@XZ`.

**Verdicts: the SWAP HYPOTHESIS IS REFUTED AS STATED. `0x8252e6b0` is CORRECT
and was not touched.** The real defect is larger, cleaner, and fully repaired:
a **6-CYCLE of constructor names** across two units, coupled to a **mirror-image
swap of two `splits.txt` pin blocks**.

Measured: **+6 matched functions / +984 B / +0.009600 pp / +1 unit at 100% /
0 units fell off.** Every pre-registered figure hit exactly.

---

## The instrument

`~/tmp/w14c/probe.py`. Same family as W13-B's — it parses `band.exe`'s PE
section table, maps VA to file offset, and reads big-endian. It independently
reproduced W13-B's section table (`.rdata` `0x82000400`, `.pdata` `0x821f1600`,
`.text` `0x82270000`), which is a free cross-lane control on both probes.

Two deliberate differences from W13-B's:

- **PPC decode is HAND-WRITTEN, not capstone.** Only the forms needed
  (`lis/addi/lwz/stw/bl/mr`) are decoded, and `walk(lo,hi)` asserts
  `len(out) == (hi-lo)//4` — a gapless decode of the whole extent. This is the
  standing hazard: capstone's PPC decoder **halts at the first undecodable
  word** and a scan then reports zero call sites in a function full of them.
  A hand decoder cannot halt; it can only mislabel, and a mislabelled word is
  visible where a missing one is not.
- **The chain, not a single reading.** The identification does not rest on
  reading one body.

### The chain that settles it

`DECLARE_MESSAGE(C, "s")` (`src/system/obj/Msg.h:176`) expands to
`static Symbol Type() { static Symbol t("s"); return t; }`. So:

1. every `?Type@C@@SA?AVSymbol@@XZ` body references the **unique string
   literal** `"s"` — which names `C` with no inference at all; and
2. every ctor `C::C(...)` is `Message(Type(), ...)`, so it `bl`s **its own
   class's** `Type()`.

⇒ **a ctor's `Type()` callee NAMES the ctor.** The chain is
`ctor -> Type() -> string literal -> class`, and only the last link needs to be
trusted.

**Link 1 verified 11 for 11**, before anything else was looked at:

| VA | map name | literal found in body |
|---|---|---|
| `0x8252e530` | `?Type@StringStrummedMsg@@` | `"string_strummed"` |
| `0x8252e5b0` | `?Type@StringStoppedMsg@@` | `"string_stopped"` |
| `0x8252e630` | `?Type@RGAccelerometerMsg@@` | `"rg_accelerometer"` |
| `0x8252e6b0` | `?Type@RGConnectedAccessoriesMsg@@` | **`"rg_connected_accessories"`** |
| `0x8252e730` | `?Type@RGPitchBendMsg@@` | `"rg_pitch_bend"` |
| `0x8252e7b0` | `?Type@RGMutingMsg@@` | `"rg_muting"` |
| `0x8252e830` | `?Type@RGStompBoxMsg@@` | `"rg_stomp_box"` |
| `0x8252e8b0` | `?Type@RGProgramChangeMsg@@` | `"rg_program_change"` |
| `0x8252e930` | `?Type@RGSwingMsg@@` | `"rg_swing"` |
| `0x8252e9b0` | `?Type@RGFretButtonDownMsg@@` | `"rg_fret_button_down"` |
| `0x8252ea30` | `?Type@RGFretButtonUpMsg@@` | `"rg_fret_button_up"` |

**Every `?Type@` name in the map is right.** ⇒ **`0x8252e6b0` really is
`RGConnectedAccessoriesMsg` — the half of the brief's swap that named it is
refuted outright.** W13-B's observation was not wrong; it was a *symptom* read
from the wrong end. The ctor at `0x8252ee20` calls the ConnectedAccessories
`Type()` because **that ctor is not PitchBend's** — the ctor name was wrong, not
the `Type` name.

---

## The finding: a 6-cycle, not a 2-swap

Applying link 2 to every ctor in the run:

| VA | size | `Type()` callee = **TRUE class** | map name | |
|---|---|---|---|---|
| `0x8251cbe0` | 0x128 | **SigninChanged** (`"signin_changed"`, via `0x823ec538`) | `??0RGConnectedAccessoriesMsg@@QAA@HH@Z` | ❌ |
| `0x8252eab0` | 0x1b8 | StringStrummed | `??0StringStrummedMsg@@QAA@HHHH@Z` | ✅ |
| `0x8252ec68` | 0x1b8 | RGAccelerometer | `??0RGAccelerometerMsg@@QAA@HHHH@Z` | ✅ |
| `0x8252ee20` | 0x128 | **RGConnectedAccessories** | `??0RGPitchBendMsg@@QAA@HH@Z` | ❌ |
| `0x8252ef48` | 0x128 | **RGPitchBend** | `??0RGMutingMsg@@QAA@HH@Z` | ❌ |
| `0x8252f070` | 0x128 | **RGMuting** | `??0RGProgramChangeMsg@@QAA@HH@Z` | ❌ |
| `0x8252f198` | 0x128 | RGStompBox | `??0RGStompBoxMsg@@QAA@_NH@Z` | ✅ |
| `0x8252f2c0` | 0x128 | **RGProgramChange** | `??0RGSwingMsg@@QAA@HH@Z` | ❌ |
| `0x8252f3e8` | 0x128 | **RGSwing** | `??0SigninChangedMsg@@QAA@KK@Z` | ❌ |
| `0x8252f510` | 0x170 | RGFretButtonDown | `??0RGFretButtonDownMsg@@QAA@HH_N@Z` | ✅ |
| `0x8252f680` | 0x170 | RGFretButtonUp | `??0RGFretButtonUpMsg@@QAA@HH_N@Z` | ✅ |

The six wrong rows form a **single 6-cycle**:

```
  0x8251cbe0  RGConnectedAccessories -> SigninChanged
  0x8252ee20  RGPitchBend            -> RGConnectedAccessories
  0x8252ef48  RGMuting               -> RGPitchBend
  0x8252f070  RGProgramChange        -> RGMuting
  0x8252f2c0  RGSwing                -> RGProgramChange
  0x8252f3e8  SigninChanged          -> RGSwing
```

The name **set** is identical before and after, so the map's name-injectivity
invariant is preserved *by construction*. (Checked as a **delta**, not an
absolute: the map already carries 2 licensed duplicates — `?NodeCmp@@YAHPBX0@Z`
is in `_internal_linkage_allow` — so an absolute injectivity assertion fires on
a clean tree and proves nothing. The assertion that matters is
`post_dup - pre_dup == {}`. My first attempt used the absolute form and refused
on a correct edit.)

### Why it never read as a clean off-by-one

`0x8252f198` (`RGStompBox`) sits **inside** the window and was **already
correct**. The true ctor order follows the header declaration order in
`UsbMidiGuitarMsgs.h` exactly (minus `StringStoppedMsg`, which retail emits no
out-of-line ctor for), but the map's list had **StompBox and ProgramChange
transposed** — and that transposition absorbs one step of the shift. So the
run reads as "mostly wrong with one inexplicably right", which is not the shape
anyone scans for.

### The pins are swapped in mirror image

`0x8251cbe0` was pinned to `UsbMidiGuitarMsgs.cpp` and `0x8252f3e8` to
`PlatformMgr.cpp` — each unit holding exactly one 0x128-byte block belonging to
the other. `SigninChangedMsg` is declared in `src/system/os/PlatformMgr.h:267`
with an **inline** ctor at line 268, used at `PlatformMgr.cpp:175`.

This is the circular-pin hazard CLAUDE.md warns about, caught in the wild: the
pin at `0x8251CBE0` exists *because* someone believed the map name there, and
the map name looks plausible *because* it is pinned into a unit that declares
that class. Name and pin corroborate each other and are both wrong.

---

## Why the change is COUPLED, and half is strictly worse than none

Names-only would put `??0RGSwingMsg@@QAA@HH@Z` inside `PlatformMgr`'s pin and
`??0SigninChangedMsg@@QAA@KK@Z` inside `UsbMidiGuitarMsgs`' pin. **objdiff pairs
target↔base BY NAME PER UNIT**, and neither object defines the other's name, so
**both rows would read 0% forever** — two rows destroyed to fix four.

Coupled, every name lands in the unit that **already** defines it. That was
checked, not assumed: both `??0SigninChangedMsg@@QAA@KK@Z` (in
`default/PlatformMgr`) and `??0RGSwingMsg@@QAA@HH@Z` (in
`default/UsbMidiGuitarMsgs`) are **already pairing today**, so there is no
new-name pairing risk anywhere in this change. It is a rare fully-safe map edit,
and it is safe *because* it is coupled.

Pin edit (equal 0x128-byte blocks; neither unit drained of its last `.text`
block, and both blocks land in ascending order):

```
UsbMidiGuitarMsgs.cpp   drop .text 0x8251CBE0-0x8251CD08
                        extend  .text 0x8252E530 end:0x8252F3E8 -> end:0x8252F510
PlatformMgr.cpp         drop .text 0x8252F3E8-0x8252F510
                        add     .text 0x8251CBE0-0x8251CD08
```

⚠ **The bare-vs-nested heading trap was checked explicitly**, keyed on FULL
PATH: exactly one heading has basename `PlatformMgr.cpp` and exactly one has
`UsbMidiGuitarMsgs.cpp`, both bare, no collision. (`system/os/UsbMidiGuitar.cpp`
is a *different*, nested unit and is untouched.)

---

## Three independent corroborations

None of these was used to form the hypothesis; all three were produced after it.

### 1. ★ A natural control that was sitting in `report.json` the whole time

Eight constructors of one family, same compiler, same shape:

| row | before | after |
|---|---|---|
| `??0RGConnectedAccessoriesMsg@@QAA@HH@Z` | **99.87805** | 100.0 |
| `??0RGPitchBendMsg@@QAA@HH@Z` | **99.87805** | 100.0 |
| `??0RGMutingMsg@@QAA@HH@Z` | **99.87805** | 100.0 |
| `??0RGProgramChangeMsg@@QAA@HH@Z` | **99.87805** | 100.0 |
| `??0RGSwingMsg@@QAA@HH@Z` | **99.87805** | 100.0 |
| `??0SigninChangedMsg@@QAA@KK@Z` | **99.87805** | 100.0 |
| `??0StringStrummedMsg@@QAA@HHHH@Z` (name proven RIGHT) | **100.0** | 100.0 |
| `??0RGStompBoxMsg@@QAA@_NH@Z` (name proven RIGHT) | **100.0** | 100.0 |

**The six I proved wrong all read exactly 99.87805; the two I proved right all
read exactly 100.0.** The score partitions the family along precisely the line
the retail-byte probe drew — 8/8, by an instrument that knows nothing about
string literals. The two correct siblings are the control: had they *also* read
99.87805, the number would have been a property of the shape, not of the name.

An arithmetic self-check falls out of it: `UsbMidiGuitarMsgs` read
`matched_code 3904 / total 4724`, and `3904 + 5 x 164 = 4724` exactly — the five
misnamed ctors were the unit's *entire* deficit.

### 2. objdiff charges exactly one instruction, and it is the name

```
## Mismatched Instructions (1 of 41 total)   99.87805% graded
| 14 | diff_arg | bl ?Type@RGConnectedAccessoriesMsg@@SA?AVSymbol@@XZ
                | bl ?Type@RGPitchBendMsg@@SA?AVSymbol@@XZ   [sym]
```

Target calls ConnectedAccessories' `Type()`; we call PitchBend's. That is the
map name being wrong, restated by the differ.

⚠ Worth naming the near-miss here: this is the `diff_arg`-only stratum that
CLAUDE.md warns ships a confident false `AT_LIMIT` ("ICF: cross-function
merge / no source mutation can close them"). It is **bit-for-bit the shape of a
wrong callee**, and here it *was* one. Six rows, 984 B, closed by editing a map.

### 3. dtk re-derived `.pdata` in the same direction, unprompted

The first build after the `.text` edit **failed**, and correctly: dtk re-derives
every `.pdata` split from the `.text` split owning the function each entry
describes. It moved exactly the two `.pdata` entries for the two functions I
swapped, in the same directions:

```
UsbMidiGuitarMsgs.cpp   - .pdata 0x82218918-0x82218938        (-> PlatformMgr)
                        - .pdata 0x82219968-0x82219B18
                        + .pdata 0x82219968-0x82219B38        (absorbed 0x82219B18-38)
PlatformMgr.cpp         - .pdata 0x82219B18-0x82219B38        (-> UsbMidiGuitarMsgs)
                        + .pdata 0x82218918-0x82218938
```

The splitter, using unwind-record ownership and knowing nothing about string
literals, **agrees with the probe**. The `.pdata` lines in the commit are dtk's,
not hand-written — per the standing rule, only `.text` was edited.

⚠ Operationally: a `.text` move of this kind **will** fail its first build with
a `.pdata` drift refusal. That is not an error to debug; the split has already
written the corrected file. Recovery is one more build, then commit what it
wrote. `ab_measure` refuses (exit 2) on that state rather than measuring it, and
correctly restored the lane's dirty work on the way out.

---

## Measurement

Pre-registered **before any edit**, sign included, at `~/tmp/w14c/PREREGISTER.md`.
`python3 tools/ab_measure.py --worktree ~/tmp/wt-w14-c --from-dirty`:

```
  leg A: matched=42840 masked=22997 honest=19843 code%=37.982830  (recompiles: 0, settled)
  leg B: matched=42846 masked=22997 honest=19849 code%=37.992430  (recompiles: 0, split=1, patch_steps=1)
  Δmatched=+6  Δmasked_equal=+0  Δhonest=+6  Δcode%=+0.009600pp  Δcode_bytes=+984
  Δfuzzy=+0.000020pp   (legA 49.154740 -> legB 49.154760)
  unit improvements: 2 unit(s), sum +6
      +5  default/UsbMidiGuitarMsgs  (65->70)
      +1  default/PlatformMgr  (62->63)
  unit net (ALL units) = +6   vs whole-binary Δmatched = +6
  units at 100% [mpn ruler]:            legA 164 -> legB 165  (1 reached 100, 0 fell off)
  units at 100% [all-rows-fuzzy ruler]: legA 136 -> legB 137  (1 reached 100, 0 fell off)
    +100%  default/UsbMidiGuitarMsgs  matched 65->70  MATCHED_ROSE
```

| measure | pre-registered | measured |
|---|---|---|
| Δmatched_functions | **+6** | **+6** |
| Δmatched_code | **+984 B** | **+984 B** |
| Δcode% | **+0.009604 pp** | **+0.009600 pp** |
| units reaching 100% | **+1 (UsbMidiGuitarMsgs)** | **+1 (UsbMidiGuitarMsgs)** |
| units falling off 100% | **0** | **0** |
| `total_code` | unchanged | unchanged (10,245,956) |

**Every figure hit.** `unit net (ALL units) = +6` equals the whole-binary
Δmatched, so nothing regressed anywhere off-target. Leg B reports
`split=1, renamer_patched=1822`, so the map edit genuinely reached the build —
not an absent-vs-absent run. Both legs converged at a `symbols.txt` fixed point
after 0 extra re-splits.

Post-fix, `default/UsbMidiGuitarMsgs` is **70/70 rows, 4724/4724 bytes — a
complete unit**; `default/PlatformMgr` 62/78 -> 63/78.

### ⚠ One correction to the brief's own framing

The brief said to *expect the metric to be silent* — "a swapped argument pair is
a register arg diff, which `mpn` excludes by construction; Δ0 is a pass."

**That is right for Task 2 and wrong for Task 1.** This defect is a relocation
**NAME** charge, which the shipped `name_check` ruler **does** score. A Δ0 here
would not have been a pass — it would have meant the re-split never ran and the
edit never reached the build. Pre-registering the expected sign *as positive*,
against the brief's stated expectation, is what made the run interpretable. The
two defect classes look alike in the source and are opposite in the metric.

The `[control none]` leg reads `Δ+0 B` while the default ruler reads `+984 B`.
On a map-only patch that shape would be the ALIAS_SUSPECT signature; here the
tool correctly labels it `NOT_APPLICABLE` because the patch carries `splits`
and can move real code.

---

## Task 2 — tree-wide sweep for the wave-13 swapped-call defect

**Verdict: 0 CONFIRMED, 0 SUSPECT. The vein is DRAINED.** ~2,300 call sites
examined across six detectors; 33 automated flags raised, **33/33 refuted by
hand**. Read-only — no edit came out of this half of the lane.

The signature being hunted: *the author read the asm correctly and then wrote
the call in DECLARATION order rather than SEMANTIC order.*

| population | decls | call sites | flagged | real |
|---|---:|---:|---:|---:|
| **A** `NormalizeTo` | 1 (`math/Mtx.h:569`, output **second**) | 3 | 0 | **0** |
| **B** `sort`/`stable_sort`/`partial_sort`/`nth_element`/`qsort` | — | 118 non-vendor | 0 | **0** |
| **C1** `(const T&, T&)` same-type, output-second | 20 decls / 12 names | 275 | 2 | **0** |
| **C2** `(const A&, B&)` different type | 44 decls | — | — | inversion **uncompilable** |
| **C3** 3-param output-last (uninit-input detector) | 41 names | 1,273 | 16 | **0** |
| **C4** adjacent-same-output (the literal wave-13 tell) | — | 86 pairs | 86 | **0** |
| **C5** `(dest,src)`: `memcpy/memmove/strcpy/strncpy/strcat/sprintf` | — | 506 | 15 | **0** |
| **C6** `(first,last)`-named ranges outside sort | — | ~60 | — | **0** |

Both briefed bugs are **already fixed and landed** in `135f6a9e`
(`std::sort(priBegin, priEnd)`; `NormalizeTo(prevQuat, ...)` x3). All three
`NormalizeTo` calls now share `prevQuat` as *input* with three **distinct**
outputs, each read below — the "same output twice" tell is absent, and both
oracles agree operand-for-operand.

Sort arguments were extracted by **paren-balancing in Python, not line-grep**:
7 of the 118 calls span multiple lines and are invisible to a line-oriented
scan. That is the kind of gap that turns a real sweep into a vacuous one.

### ★ Why the vein is structurally drained — worth more than the null result

**An argument inversion of an output-parameter function is usually
uncompilable.** For `(const T&, T&)` the input slot is `const`, so an inversion
only compiles when *both* arguments are non-const lvalues of the same type —
60 of 275 sites; the other 215 are `F(x,x)` self-ops, inversion-immune by
construction. For `(const A&, B&)` the types differ and the compiler rejects it
outright.

⇒ **the compiler is already the detector for most of this class.** What wave 13
found were precisely the two shapes that slip past it — same-type non-const
lvalues on both sides (`sort(ptr,ptr)`, `NormalizeTo(Quat,Quat)`) — and both are
enumerated exhaustively above and clean.

**Recommendation: do not re-fund a textual sweep of this class.** If the
coordinator wants to keep hunting the defect *class*, the instrument is not
textual: it is the asm signature both wave-13 bugs shared — **retail setting up
`r4` before `r3` at a 2-argument `bl`**, which is what led the author to
transcribe materialization order as argument order. The 2,518 split `.s` files
under `build/45410914/asm/` make that scan feasible with no build, **keying on
the `.fn fn_<ADDR>` symbol and never the synthetic address column**.

### Swept and clean — do not re-hunt

`NormalizeTo` (3 sites) · the whole sort family (118) · all 12 output-second
math functions (275) · the 41-name output-last 3-param family (1,273) ·
`memcpy/memmove/strcpy/strncpy/strcat/sprintf/snprintf` (506) ·
`(first,last)`-named range params outside sort · `CharIKHand` quaternion block
(oracle-cleared) · `HamAudio` loop/jump ordering (implementation-cleared) ·
`Singer.cpp` `copy(last,first)` (**text is inside comments**; the real
`__copy_ptrs` calls are correctly `(first,last,result)`).

### Two traps found for the next lane (neither is a bug)

- ⚠ **`src/system/rndobj/AmbientOcclusion.cpp:1232-1233`** — the *declarations*
  are still `priEnd` then `priBegin`. That is **deliberate**: retail loads
  `0xb4`(end) before `0xb0`(begin). The **call** is correct
  (`std::sort(priBegin, priEnd)`). The reason lives only in `135f6a9e`'s commit
  message, so **any future textual scan will re-flag this site**. A one-line
  comment there is cheap insurance; not added by this lane (see below).
- ⚠ **`HamAudio::SetLoop` / `SetJump` / `SetCrossfadeJump`** — the naming is a
  genuine trap: `SetCrossfadeJump`'s parameter named `startTime` is the loop
  **end**. Traced end-to-end: `SetLoop(f1,f2)` sets marker `"start"`=f1 /
  `"end"`=f2 then calls `stream->SetLoop(end, start)`, so f1 is jump-**to** and
  f2 jump-**from**; `SetLoop(endTime, startTime)` is therefore **correct**.
  Three independent call sites agree. Headers declare the parameters unnamed, so
  they cannot adjudicate — the implementation had to.

---

## Gates

Full `./tools/ninja-locked` (never a single `.obj`, never `objdiff-cli --build`):
`EXIT=0`.

```
[patch-state] OK: 1205 decomp, 3083 target objects match 2026-09-13T08:31:26Z (tree_sha256=b7d92dc56d474a97)
```
`python3 scripts/verify_objs_patched.py --verify-manifest` -> **rc=0**

```
VALIDATE: PASS -- 1362 map-consistent, 239 tolerated (enumerated above), 0 contradicted, 1603 total
```
`python3 tools/icf_alias_finder.py --validate` -> **rc=0**, **0 CONTRADICTED**.

⚠ `scripts/symbol_aliases.json` was grepped **by ADDRESS** (`8251cbe0`,
`8252e6b0`, `8252ee20`, `8252f3e8`, and the rest of the cycle) **before** any
edit: **0 hits**, so no alias group is anchored at a renamed address and none
could be orphaned by this change. The address grep is the one that matters — a
name-keyed census structurally cannot see an address-anchored group. (By name,
`SigninChangedMsg` appears 5x and `RGConnectedAccessoriesMsg` 1x in that file,
none at these addresses.)

Native gate: **NOT REQUIRED and reported below anyway.** This lane touches no
`src/` and no header — only `config/45410914/splits.txt`,
`scripts/target_symbol_map.json` and this document, none of which is an input to
the native build. It was run regardless because the brief asks for the verbatim
line and a cheap gate is worth more than an argument about applicability.

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

Full coverage, `skipped=0`, on the **first** run — W13-B's first-run-STALE
anomaly did not recur here, so no re-run was needed to earn the PASS.

---

## What this lane deliberately did NOT do

- **Did not touch `0x8252e6b0`** or any other `?Type@` row. All 11 verified
  correct against their string literals; the brief's suspicion of that row is
  refuted, and editing it would have broken four working rows.
- **Did not act on Task 2.** The sweep found nothing that contradicts its own
  declaration, and manufacturing a weak candidate to have something to land
  would be worse than the null. The structural reason it is drained is recorded
  above so the next lane can price a re-sweep at zero.
- **Did not add the explanatory comment at `AmbientOcclusion.cpp:1232`**, though
  I recommend it. It is a `src/` edit, and a `src/` edit would pull the native
  gate, a rebuild and a fresh A/B into a lane whose measured result is already
  clean — trading a documented trap for an undocumented risk on someone else's
  file. Flagged for whoever next edits that TU.
- **Did not investigate the other 229 no-source / mis-pinned units.** The
  circular pin found here (`0x8251CBE0`) is very unlikely to be unique — the
  same mechanism (a plausible-looking map name justifying a pin, and the pin
  then corroborating the name) can produce a self-consistent wrong pair
  anywhere. A census keyed on *"does this function's callee set agree with the
  unit it is pinned to"* is the obvious follow-on and is **not** this lane's.
- **Did not re-verify W13-B's `Poll` argument-order fixes.** They are landed and
  independently confirmed there; nothing in this lane's evidence contradicts
  them, and the 10th ctor W13-B could not name is now named (`RGSwingMsg` at
  `0x8252f3e8` is a red herring for that question — the connected-accessories
  ctor is `0x8252ee20`).
- **Did not fix `tools/ab_measure.py`.** No need: the untracked-deletion defect
  W13-B reported appears **already fixed** — this lane's restore line reads
  `verified by re-reading the diff AND the untracked set`, where W13-B saw only
  the tracked half. Recorded as an observation, not a re-audit.
