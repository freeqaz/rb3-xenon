# The ContextChecker name that belonged to `Object.cpp`, and two `Save` bodies written from retail bytes

**Lane W16-H, 2026-09-14.** Branch `w16-h` off `main` `83488617`.
Worktree `~/tmp/wt-w16-h`. Ruler `name_check`, read from `report.json`'s
`provenance.diff_config`, never assumed.

Two items, both briefed as **bug exposure** — expected metric Δ ≈ 0 or slightly
negative, and that being the point. One of them returned the largest single row
of the branch instead, and the reason it did is the most transferable finding
here.

> **VERDICT SUMMARY**
>
> | | |
> |---|---|
> | item 1 adjudication | **`0x8275B868` is `?RegisteredFactory@Object@Hmx@@SA_NVSymbol@@@Z`** — not ContextChecker's `IsContextUsed`. Rename + re-home. |
> | item 1 measured | **+1 fn / +864 B**, pre-registered Δ0. The miss *is* the finding (§3). |
> | item 2 bodies written | **2 of 6**, both **byte-exact against retail before being named** |
> | item 2 rows that were never bodies | **2 of 6** — `BandSwatch::Save` and `WorldInstance::PreSave` are **misnamed thunks reading a FALSE 100**, proven on retail bytes (§7) |
> | item 2 measured | **+2 fns / +216 B**, pre-registered outcome A exactly, zero collateral from the pins |
> | branch total | **+3 fns / +1,080 B** |
> | left priced for the next lane | **+2 fns / +560 B** — two decoded bodies, unwritten (§6); plus a tree-wide `$4`-thunk false-100 census (§8) |
> | item 2 left undone | **2 of 6** — `EventAnim::Save`, `BandList::Save`: fully decoded, not written (§8) |
> | ⛔ largest correction to an existing doc | **`OBJECTCPP_TU` §1.2's "byte identity outranks the geometry" is FALSE here** — the bytes that would have discriminated are exactly the forgiven ones (§2.4) |
> | ⛔ correction to the brief itself | the six briefed row sizes are **not those rows** (every one is a 12 B thunk already at 100); **3 of 6 quoted sizes are wrong** (§4) |

---

## 1. Provenance

Full `./tools/ninja-locked` before any name-keyed analysis — a fresh worktree's
reflinked target objects are **pre-renamer**, so every mangled-name lookup would
read "absent" and every negative would be vacuous.
`scripts/verify_objs_patched.py --verify-manifest` → **rc=0** at every
measurement point.

Baseline at `83488617`: **43,129 matched / 3,926,484 B / 38.322280% /
fuzzy 49.250120**, `total_code` 10,245,956, `total_functions` 69,219.

Every measurement in this document is a **set-diff of the `fuzzy == 100` row
set** (`tools/rowset_snapshot.py`), never a rounded gap — so offsetting moves
are visible individually.

---

## 2. Item 1 — `0x8275B868`: reading the 72 B body

### 2.1 What was handed over

W15-A (`CIRCULAR_PIN_CENSUS_2026-09-14.md` §8.5) flagged a **72 B single-function
sliver** named `?IsContextUsed@?A0x1e5d0754@@YA_NVSymbol@@@Z`, pinned to
`ContextChecker.cpp` **1.99 MB** from that unit's nearest other block, sitting
inside DirLoader's territory. Its only caller is
`0x82757108 = ?LoadHeader@DirLoader@@AAAXXZ` — and a DirLoader method cannot call
a ContextChecker file-static. W15-A declared the *name* the suspect, declined to
act without a positive identification, and handed off.

W13 (`OBJECTCPP_TU_2026-09-11.md` §1.2) had independently cited the **same row**
as one of two *refutations* of "this whole span is `Object.cpp`", because it
"reads 100.0 today — our `ContextChecker.obj` byte-matches retail at that
address", concluding **"byte identity outranks the geometry"**.

Both lanes were reasoning about the same 72 bytes and reached opposite
conclusions. Neither read the bytes.

### 2.2 The body

```
8275B868  mflr r12 ; stw r12,-8(r1) ; std r31,-0x10(r1) ; stwu r1,-0x60(r1)
8275B878  stw   r3, 0x74(r1)          ; spill the Symbol argument
8275B87C  lis   r11, 0x82E0
8275B880  addi  r4, r1, 0x74          ; r4 = &sym
8275B884  addi  r31, r11, 0x5C50      ; r31 = 0x82E05C50   <-- a static
8275B888  mr    r3, r31
8275B88C  bl    0x82B992A0            ; _Rb_tree<Symbol,...>::_M_find
8275B890  subf  r11, r3, r31
8275B894  addic r10, r11, -1
8275B898  subfe r3,  r10, r11         ; r3 = (retval != 0x82E05C50)
8275B89C  ... epilogue ... blr
```

`_M_find` (`0x82B992A0`) initialises its "not found" register to `this` and
returns it on a miss, so comparing the result against the container's own
address **is** comparing against `end()`. The body is exactly

```cpp
return theStatic.find(sym) != theStatic.end();
```

⚠ **The callee's mangled name says `map<Symbol, bool>` and that is an ICF
survivor name, not evidence about the container.** `find` never touches the
mapped value, so every `map<Symbol, *>::_M_find` instantiation has identical
bytes *and* identical relocations and they all fold; the surviving spelling is
arbitrary. Reading the value type off a folded callee's name is the
`TEMPLATE_ARGS_DIFFER` trap.

### 2.3 Four channels, all retail-side, name the owner

1. **The static.** A scan of retail `.text` for every `lis`/D-form pair forming
   an address in `[0x82E05C50, +0x40)` (`~/tmp/w16h/xref.py`, attributing each
   hit to its enclosing function via `.pdata` BeginAddresses) returns exactly
   three game functions touching `0x82E05C50`: this one,
   `?NewObject@Object@Hmx@@SAPAV12@VSymbol@@@Z`, and
   `?RegisterFactory@Object@Hmx@@SAXVSymbol@@P6APAV12@XZ@Z`.
   ⇒ `0x82E05C50` is **`Hmx::Object::sFactories`**, an `Object.cpp` static member.
   Our own `Object.obj` symbol table spells it
   `?sFactories@Object@Hmx@@0V?$map@VSymbol@@P6APAVObject@Hmx@@XZ...`.
2. **The caller.** `DirLoader::LoadHeader` calls `Hmx::Object::RegisteredFactory`
   in *all three* sources — ours `src/system/obj/DirLoader.cpp:1193`, rb3-Wii
   `501/510/557`, DC3 `1024/1033/949`.
3. **The oracles' body.** Both spell it identically:
   `return sFactories.find(name) != sFactories.end();` — and so does our own
   `src/system/obj/Object.cpp:1018`, already in the tree.
4. **The geometry.** `Object.cpp`'s `.text` blocks **bracket the sliver
   exactly** — `...end:0x8275B868` immediately followed by
   `start:0x8275B8B0...`. W13 carved *around* it.

⇒ **`?RegisteredFactory@Object@Hmx@@SA_NVSymbol@@@Z`.**

### 2.4 ⛔ Why W13's byte identity was not evidence

Our `ContextChecker.cpp:30`

```cpp
bool IsContextUsed(Symbol ctx) { return gUsedContexts.find(ctx) != gUsedContexts.end(); }
```

over a `std::set<Symbol>` compiles to a **byte-identical instruction stream** to
retail's `RegisteredFactory`. The two differ in exactly two relocations — the
static's address, and the `_M_find` instantiation (which itself folds). **Both
are placeholder-forgiven on the target side**, because `0x82E05C50` has no map
name, so `name_check` charged nothing and the row read a **false 100 across a TU
boundary**.

★ **"Byte identity outranks the geometry" is false whenever the bytes that would
discriminate are the forgiven ones.** A `find(x) != end()` one-liner is *all*
relocation and almost no instruction; its instruction stream carries essentially
no identifying information. This is the placeholder-forgiveness hazard CLAUDE.md
documents for call sites, operating one level up — on the *identity of a whole
function*.

### 2.5 The other two adjudications, excluded on bytes

* **NOT a cross-TU ICF fold.** MSVC folds only COMDATs identical **including
  relocations** (CD-7). A ContextChecker `IsContextUsed` relocates to
  `gUsedContexts`; the same `xref.py` scan puts that at **`0x82DFE098`**, every
  one of whose referrers is a `0x8256Dxxx` ContextChecker function. Retail's body
  here relocates to `0x82E05C50`. Different relocation ⇒ **cannot fold.**
  Moreover **there is no second body to fold with**: retail has no standalone
  `IsContextUsed` at all — `/Ob2` inlined it into its single call site,
  `?PotentiallyCreateAndAddEntry@@...` @`0x8256DA40`, which performs the
  `_M_find` on `0x82DFE098` inline at `0x8256DA6C`.
* **NOT a port-first case.** Our `Object.cpp` already carries the body, and
  `Object.obj` already **defines** `?RegisteredFactory@Object@Hmx@@SA_NVSymbol@@@Z`
  at COFF storage class **2 (EXTERNAL)** — the precondition that makes a rename
  safe rather than a permanent 0%, per the standing "proving a name wrong ≠
  renaming is safe" rule. Checked in the object, after a full build.

---

## 3. Item 1 — the measurement, and why the prediction missed

Pre-registered, before building: **Δ0**. The reasoning was the standard one for
a rename — `matched_functions` counts `mpn == 100`, the row at `0x8275B868` was
already reading **100.0**, and moving a correct body under a correct name should
not move a score that was already full.

Measured, by set-diff of the `fuzzy == 100` row set:

| | matched_functions | matched_code | code% | fuzzy% |
|---|---:|---:|---:|---:|
| baseline `83488617` | 43,129 | 3,926,484 | 38.322280 | 49.250120 |
| after item 1 | 43,130 | 3,927,348 | 38.330710 | 49.250122 |
| **Δ** | **+1** | **+864 B** | +0.008430 | +0.000002 |

**ADDED: 1 row / 864 B — `?LoadHeader@DirLoader@@AAAXXZ`. LOST: 0.**

The row that crossed is **not the row that was renamed**. `0x8275B868` itself
contributed **0 bytes** — it was already at 100 and stayed there. The entire
+864 B is its **caller**.

**Mechanism.** `DirLoader::LoadHeader` contains a `bl` to `0x8275B868`. Under
`name_check`, a relocation whose target carries a **real** name is *checked*:
retail's call resolved to `?IsContextUsed@?A0x1e5d0754@@YA_NVSymbol@@@Z` while
our source emits a call to `?RegisteredFactory@Object@Hmx@@SA_NVSymbol@@@Z`.
Two different real names ⇒ **charged**, exactly as a genuinely wrong callee is
charged. That single `diff_arg` held an otherwise byte-perfect 864 B function
below `fuzzy == 100`, and `matched_code` is all-or-nothing per row, so the whole
864 B was withheld on the strength of one wrong *name* in a table.

This is MAPDEF-3's "repairing a wrong map name pays through its callers" — at
**8× the size of the row repaired**. Recorded as a sizing precedent: the payout
of a map-name repair is not bounded by the renamed function; it is bounded by
the **total size of every caller that was otherwise complete**. Nothing about
the 72 B body could have predicted 864.

⚠ **The pre-registration was still correct practice and I would make it again.**
It was wrong in magnitude and right in kind — it forced the set-diff that
attributed the gain to `LoadHeader` rather than letting a "+864, nice" land
unexplained. A prediction that misses and is *diagnosed* is worth more than one
that hits.

---

## 4. Item 2 — what the six rows actually are

The brief described six `UNIMPLEMENTED_BODY` rows with sizes
388 / 136 / 128 / 96 / 80 / 68 B. **The first thing measured was that this
premise does not survive contact with `report.json`.**

Every one of the six map symbols is a `$4PPPPPPPM@…` **vtordisp adjustor
thunk**, and every one of those rows is **12 bytes** and **already reads
`fuzzy = 100.0`**:

```
?Save@BandList@@$4PPPPPPPM@A@AAXAAVBinStream@@@Z          size=12  fuzzy=100.0
?PreSave@WorldInstance@@$4PPPPPPPM@A@AAXAAVBinStream@@@Z  size=12  fuzzy=100.0
?Save@BandScoreboard@@$4PPPPPPPM@A@AAXAAVBinStream@@@Z    size=12  fuzzy=100.0
?Save@OverdriveMeter@@$4PPPPPPPM@A@AAXAAVBinStream@@@Z    size=12  fuzzy=100.0
?Save@BandSwatch@@$4PPPPPPPM@A@AAXAAVBinStream@@@Z        size=12  fuzzy=100.0
?Save@EventAnim@@$4PPPPPPPM@A@AAXAAVBinStream@@@Z         size=12  fuzzy=100.0
```

So the briefed sizes are not the sizes of the briefed rows. They are *mostly*
the sizes of the **branch destinations**, and **three of the six are wrong**.
Destinations were recovered by decoding each thunk's `b` and reading the retail
`.pdata` `FunctionLength`:

| symbol | thunk | `b` destination | briefed size | **measured body size** |
|---|---|---|---:|---:|
| `BandList::Save` | `0x8233D548` | `0x8233C4E0` | 388 | **388** ✅ |
| `WorldInstance::PreSave` | `0x824EB260` | `0x824EA298` | 136 | **no body — not a `.pdata` BeginAddress** ❌ |
| `BandScoreboard::Save` | `0x822CE828` | `0x822CDC80` | 128 | **128** ✅ |
| `OverdriveMeter::Save` | `0x822DBC88` | `0x822DB0F8` | 96 | **88** ❌ |
| `BandSwatch::Save` | `0x822AE7A0` | `0x822AF178` | 80 | **80** ✅ (but see §7) |
| `EventAnim::Save` | `0x824CA0B0` | `0x824C9540` | 68 | **172** ❌ |

⚠ **The `.pdata` decoder was validated before any of these numbers were
believed.** PowerPC `RUNTIME_FUNCTION` packs `PrologLength:8, FunctionLength:22`
from the LSB; a first pass shifted by 10 instead of 8 and produced sizes around
**4.19 MB** — absurd enough to catch, but the same error with a smaller offset
would have produced *plausible* wrong numbers. The corrected decoder reproduces
three independently known answers exactly (128, 88, and item 1's 72), which is
what licenses the two it discovered (388, 172).

`0x824EA298` having **no unwind record at all** is itself corroborating rather
than anomalous: it is an 8-byte leaf thunk, and per the standing scope bound on
CD-7, a leaf stub that touches neither stack nor LR gets no `.pdata` entry.

---

## 5. Item 2 — per-body results

| body | retail addr | size | ported? | byte-exact? | named? | pinned? | predicted Δ | **measured Δ** |
|---|---|---:|---|---|---|---|---|---|
| `BandScoreboard::Save` | `0x822CDC80` | 128 | ✅ | ✅ 128/128 words | ✅ | ✅ re-home | A: +1 / +128 | **+1 / +128** |
| `OverdriveMeter::Save` | `0x822DB0F8` | 88 | ✅ | ✅ 88/88 words | ✅ | ✅ carve | A: +1 / +88 | **+1 / +88** |
| `BandList::Save` | `0x8233C4E0` | 388 | ❌ decoded only | — | ❌ | ❌ | — | **0** |
| `EventAnim::Save` | `0x824C9540` | 172 | ❌ decoded only | — | ❌ | ❌ | — | **0** |
| `BandSwatch::Save` | *(no body)* | — | n/a | n/a | ❌ **name is wrong** | — | — | **0** (§7) |
| `WorldInstance::PreSave` | *(no body)* | — | n/a | n/a | ❌ **name is wrong** | — | — | **0** (§7) |

**Order of operations was body-first, name-second, exactly as briefed**, and the
two-stage measurement proves the discipline was real rather than asserted:

| stage | matched_functions | matched_code | Δ |
|---|---:|---:|---|
| after item 1 | 43,130 | 3,927,348 | — |
| **bodies written, still unnamed** | 43,130 | 3,927,348 | **0 / 0** — as pre-registered |
| **+ name + pin** | 43,132 | 3,927,564 | **+2 / +216** |

The unnamed stage reading **exactly** Δ0 is the control that makes the +216
attributable to pairing rather than to anything incidental, and it is also the
evidence that byte-exactness was verified **before** the name was added — the
standing "never name a non-matching body" rule, executed in a way that leaves a
trace in the numbers.

Byte-exactness was established **without** a map name, by
`~/tmp/w16h/cmpbody.py`: our compiled COMDAT against retail bytes word-by-word
with relocated fields masked (branch forms `& 0xFC000003`, D-form
`& 0xFFFF0000`). This is the instrument that makes "body first, name second"
*possible*; `run_objdiff` cannot score an unnamed row at all.

**Sources.** `OverdriveMeter::Save` — retail has a **real** `Save` where the
rb3-Wii oracle carries only a `SAVE_OBJ` assert stub, so the oracle was
overridden on retail bytes (`SAVE_REVS(0,0)` + `SAVE_SUPERCLASS(RndDir)`, the
rev word being a literal 0 read out of the instruction stream).
`BandScoreboard::Save` needed `SAVE_REVS(1,0)`, an `if (!IsProxy())` guard
around `bs << mStarDisplay`, then `SAVE_SUPERCLASS(RndDir)`; the `IsProxy`
codegen signature was calibrated against `BandCrowdMeter`, a neighbouring
100%-matching reference, rather than recognised from memory.

**Pin mechanics.** `BandScoreboard::Save`'s block was pinned whole to
`HamPhotoDisplay.cpp` and moved entire. `OverdriveMeter::Save` had to be
**carved out of the middle** of `StreakMeter.cpp`'s 1,960 B block, splitting it
three ways. PINHOME-1 warns that re-homing an already-pinned address is **not**
metric-neutral; the pre-registration therefore carried collateral as a real
risk. **Measured collateral: zero rows, zero bytes** — the set-diff shows 2
added and 0 lost. Recorded as the benign end of PINHOME-1's range: re-homing is
not *guaranteed* neutral, which is not the same as being reliably costly.

---

## 6. Item 2 — the two bodies left unwritten

Both are fully decoded and both are **blocked on the same two mechanical steps**,
not on understanding:

| | `BandList::Save` | `EventAnim::Save` |
|---|---|---|
| retail body | `0x8233C4E0`, **388 B** | `0x824C9540`, **172 B** |
| in `target_symbol_map.json`? | **absent** | **absent** |
| currently pinned to | `CharSignalApplier.cpp` `0x8233C480–0x8233C668` | `EventTrigger.cpp` `0x824C94D0–0x824C97D8` |
| work required | carve the block, add map entry, write body | carve the block, add map entry, write body |
| **value if byte-exact** | **+1 fn / +388 B** | **+1 fn / +172 B** |

Both destinations sit **inside another unit's `.text` block**, so each needs the
same three-way carve `OverdriveMeter::Save` required — which is now demonstrated
to be safe on this tree. Combined outstanding value: **+2 fns / +560 B**, on
bodies whose disassembly is already done. This is the highest-density remaining
item on the branch and it is left deliberately, not overlooked.

---

## 7. ⛔ Two of the six rows are FALSE 100s — the forgiveness mechanism running backwards

`BandSwatch::Save` and `WorldInstance::PreSave` were briefed as missing bodies.
They are not. **They are wrong names on correct-looking thunks, and the metric
cannot see it.**

### 7.1 `?Save@BandSwatch@@$4PPPPPPPM@A@AAXAAVBinStream@@@Z` @ `0x822AE7A0`

Retail's thunk branches to `0x822AF178`, whose 80 B body is:

```
822AF17C  stw   r12, -8(r1)            ; prologue
822AF18C  addi  r31, r3, -0xa4         ; adjust to the base subobject
822AF198  bl    0x822AECB0             ; the real destructor
822AF19C  clrlwi. r11, r30, 0x1f       ; r30 = the incoming r4, tested for BIT 0
822AF1A0  beq   0x822AF1AC
822AF1A8  bl    0x827BC430             ; = ?MemFree@@YAXPAX@Z
822AF1AC  mr    r3, r31                ; return this
```

This is the canonical **scalar deleting destructor**: `if (flag & 1)
MemFree(this); return this;`. A `void Save(BinStream&)` returns nothing and
receives a `BinStream&` in `r4`; here `r4` is an integer flag whose **bit 0**
decides a `MemFree`. A stream reference tested for bit 0 to decide whether to
free the object is not a reading of this code — it is decisive against it.

Two further facts locate the real owner. The thunk at `0x822AE7A0` lies **inside
BandSwatch.cpp's own block** (`0x822ADED8–0x822AF178`), and the destructor it
reaches calls `0x822AECB0`, which is **also inside that block**. So this is
BandSwatch's *scalar deleting destructor* adjustor thunk, and the destructor
body at `0x822AF178` — the first byte past BandSwatch's block — is **mis-pinned
into `CharInterest.cpp`**.

### 7.2 `?PreSave@WorldInstance@@$4PPPPPPPM@A@AAXAAVBinStream@@@Z` @ `0x824EB260`

Chains through a second adjustor thunk:

```
824EA298  addi r3, r3, -0x14
824EA29C  b    0x82402F68   ; = ?Replace@RndDir@@UAAXPAVObjRef@@PAVObject@Hmx@@@Z
```

`RndDir::Replace` takes **two pointer arguments**; `PreSave` takes a
`BinStream&`. This is a **`Replace` thunk**. Independently, our
`WorldInstance::PreSave` is already implemented — `void WorldInstance::PreSave(BinStream&) {}`
at `src/system/world/Instance.cpp:113` — so there was never a missing body here
either.

### 7.3 Why the metric reads 100 anyway — and why this is §2.4 again

Our objects **do** define both names as 12 B thunks, so both rows pair, and both
read `fuzzy = 100.0`. `cmpbody.py` shows why:

```
0000  ours 8163FFFC lwz  r11, -4(r3)   | retail 8163FFFC lwz  r11, -4(r3)
0004  ours 7C6B1850 subf r3, r11, r3   | retail 7C6B1850 subf r3, r11, r3
0008  ours 4BFFFFF8 b <BandSwatch::Save> | retail 480009D0 b 0x822AF178   [reloc]
VERDICT: BYTE-EXACT (reloc-masked)
```

The instruction stream is identical. The **only** discriminating information is
the `b` target — and retail's target `0x822AF178` is **absent from the map**, so
`name_check` treats it as a placeholder and **forgives it**. Our thunk points at
`BandSwatch::Save`; retail's points at a destructor; the ruler charges nothing.

⇒ **This is §2.4's finding arriving a second time, independently, from the
opposite direction.** In item 1, byte identity failed to *distinguish* a wrong
name. Here, byte identity plus reloc forgiveness actively *certifies* one. Same
mechanism, same blind spot: **when the discriminating relocation is the forgiven
one, an identical instruction stream carries zero identifying information** —
and a 100 built on it is a statement about the map's coverage, not about our
code.

### 7.4 Why neither was renamed

Per the standing rule — **proving a name wrong is not the same as proving a
rename safe; objdiff pairs by name, and a base object that cannot define the new
name leaves the row permanently at 0%.**

For `BandSwatch` the replacement is a `??_E` adjustor thunk, and our
`BandSwatch.obj` defines **five** candidate spellings — `WBFE@`, `WBEA@`,
`WBEE@`, `WBFA@` (8 B each) and `$4PPPPPPPM@CFI@` (16 B) — and **not one of them
is 12 bytes**. No candidate matches the retail thunk's size, so no rename is
currently defensible. For `WorldInstance` the correct name is a `Replace`
adjustor thunk whose exact vtordisp spelling was likewise not established.

⚠ **And nulling the map entries is the wrong repair, not merely a timid one.**
Nulling would drop both rows to unpaired 0% (−24 B) and would have to be
*undone* by whichever lane identifies the correct spellings; the map-name
economics record that un-pairing dominates a map edit's delta. The correct
repair is a **rename to the right `??_E`/`Replace` spelling**, which preserves
pairing *and* accuracy at once. Recorded here, priced at −24 B of false credit,
and handed on rather than half-applied.

---

## 8. What was NOT done

* **`BandList::Save` (388 B) and `EventAnim::Save` (172 B) were not written.**
  Both bodies are decoded; both need a splits carve plus a map entry. Worth
  **+2 fns / +560 B**. §6 carries the addresses and current owners.
* **The two false-100 thunks were not renamed** — §7.4. The deliverable there is
  the proof and the priced recommendation, not an edit.
* **`0x822AF178` was not re-pinned** out of `CharInterest.cpp` into
  `BandSwatch.cpp`, even though §7.1 argues it belongs there. Re-homing is not
  metric-neutral and the correct `??_E` name is not yet established; doing the
  pin without the name buys nothing and risks collateral.
* **No alias was installed in `symbol_aliases.json`** for any of this. Item 1's
  third adjudication was excluded on evidence (§2.5), and an alias for the §7
  thunks would be *forgiveness of a divergence proven real* — the integrity
  hazard that mechanism is explicitly flagged for.
* **The `$4` thunk rows were not re-priced tree-wide.** The forgiveness pattern
  in §7.3 is structural, not specific to these two: any `$4` thunk whose retail
  destination is unnamed reads 100 regardless of whether our source names the
  right callee. **How many such rows exist was not measured**, and that census —
  not more bodies — is probably the higher-value follow-up.
