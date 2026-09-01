# Vtable COUNT wave — `ClientProtocol`, `OggMap`, `MCResultMsg` (lane VTCOUNT2, 2026-09-01)

Tree: branch `vt-count2` off `dc605388`, worktree `~/tmp/wt-vtcount2`, **built
before any name-keyed analysis** (a reflinked worktree's target objs are
pre-renamer, so every retail mangled name would otherwise read "absent" —
CLAUDE.md FOLDPROVE-2).

> **Deliberately a NEW file.** Companion to
> `VTABLE_SLOT_COUNT_FIXES_2026-08-20.md`, `VTABLE_COUNT_MESH_2026-08-27.md`
> and `VTABLE_COUNT_PURCHASER_2026-08-27.md`. Five concurrent lanes have now
> collided on section numbers in the first of those. Nothing here renumbers
> anything there.

Brief: three `ours == retail − 1` COUNT rows — retail has exactly one more
virtual than we declare. All three reproduce. **One is FIXED, two are reported
UNDERDETERMINED with the evidence that would settle them.**

---

## 0. Headline — and four things in the brief that are wrong

| claim | outcome |
|---|---|
| main is at `26576070` | ⛔ main was at **`dc605388`** |
| `OggMap` "declares `class OggMap {` with **no base class**" — and that is correct | ⛔ **retail's `OggMap` HAS a base class**, `OggValidatorFileSource`, and retail's object is **0x40 bytes vs our 0x14** |
| `MCResultMsg` "has no class definition under `src/` that I could find" | ⛔ it **is** defined — `DECLARE_MESSAGE(MCResultMsg, "memcard_result")`, `src/system/os/Memcard.h:133`. Macro-generated, which is why a class-definition search missed it |
| `ClientProtocol` — "name-based reasoning will be weak here and **may be impossible**"; concluding underdetermined is acceptable | ⛔ **it is the one that got FIXED.** Name-based reasoning *was* impossible; the fix came from **fan-out across sibling subclasses**, which needs no name at all |
| the brief's difficulty ordering (OggMap/MCResultMsg tractable, ClientProtocol hopeless) | **INVERTED**, exactly as in lane MESHCOUNT |

★ The reason the ordering inverted is the same one MESHCOUNT recorded and the
brief did not carry: what makes a COUNT class tractable is **not** how clean its
names are, it is **whether a sibling exists that resolves the same slot
differently**. `ClientProtocol` has three matching siblings. `OggMap` and
`MCResultMsg` have none — `OggValidatorFileSource` has no COL in the image at
all, and `MCResultMsg` is 1-of-173 in its family.

**Instruments validated before use** (a PASS is worthless until the gate is
shown to be able to fail):

- `tools/retail_rtti.py --selftest` → **8/8 controls pass**;
  `--sabotage naive-va` → **rc=1**; `--sabotage overscan` → **rc=1**.
- Anti-vacuity guard asserted at the top of every scratch script in this lane:
  `len(word_refs(0x823591e8)) == 2770`. Reproduces the published figure exactly.
- `scripts/verify_objs_patched.py --check` → *"OK: tree is a fixed point of 6
  post-compile passes"*, 1045/1045 declared objects paired.

---

## 1. `ClientProtocol@Quazal` — 23 retail vs 22 ours. **FIXED**

### 1a. Every name in this vtable is garbage, and that is the point

`??_7ClientProtocol@Quazal@@6B@` @ **`0x8207f944`**, `folded_slots = 23 of 23`.
The map spellings are ICF fold-survivors and are actively misleading:

```
[ 5] 0x826c3888  EMPTY_STUB  fan=6520  "StlNodeAlloc<_List_node<int>>::ctor"
[ 8] 0x82b5ac08  fan=80      "PreLoad@Object@Hmx"
[11] 0x82533618  fan=422     "IsDirPtr@ObjDirPtr<ObjectDir>"
[18] 0x823591e8  fan=2770    "GetCrowdMeter@TrackPanelDirBase"
[21] 0x823591e8  fan=2770    "GetCrowdMeter@TrackPanelDirBase"   <- same address
[22] 0x828299b8  _purecall   fan=849
```

`GetCrowdMeter@TrackPanelDirBase` appearing twice inside a Quazal network
protocol vtable is not a defect to repair — it is what `li r3,0; blr` looks like
after `/OPT:ICF` collapses every `return 0` in the image into one body and the
map keeps one arbitrary spelling. **Do not open these as map defects.**

### 1b. The instrument that worked: FAN-OUT across siblings

Of 2,220 retail vtables, **13** are in the `Quazal` namespace. Three of them are
`ClientProtocol` subclasses and are **already matching at 23/23**:

| class | vt VA | retail | ours | slot 22 (retail) |
|---|---|---:|---:|---|
| `ClientProtocol` | `0x8207f944` | 23 | **22** | `0x828299b8` **`_purecall`** |
| `RBDataClient` | `0x8207f9d4` | 23 | 23 | `0x8250a510` (real body) |
| `RBBinaryDataClient` | `0x8207fa3c` | 23 | 23 | `0x8250aaa8` (real body) |
| `RBTestClient` | `0x8207faa4` | 23 | 23 | `0x8250af08` (real body) |

**Three DISTINCT real bodies at one slot across three siblings, over
`_purecall` on their common base, is the signature of a pure virtual declared on
the base and overridden per subclass.** No name is consulted anywhere in that
inference.

Our side already had the name, on the wrong class: all three subclasses declare
`?ExtractCallSpecificResults@…@Quazal@@UAAXPAVMessage@2@PAVProtocolCallContext@2@@Z`
at their own slot 22. We declared it on **five** subclasses and **nowhere on the
base**, leaving the base one slot short.

### 1c. The alignment is corroborated WITHOUT names

Our 22 slots map 1:1 onto retail 0–21, checked on body *shape* rather than
spelling:

| slot | retail shape | ours |
|---|---|---|
| 19, 20 | `EMPTY_STUB` `0x826c3888` | `EndPointDisconnected`, `FaultDetected` — both empty |
| 18, 21 | the `return 0` fold `0x823591e8` | `GetProtocolType`, `Clone` — both `return 0` |
| 5 | `EMPTY_STUB` | `EnforceDeclareSysComponentMacro` — empty |

So the extra slot is unambiguously **appended at 22**, not inserted mid-table.

### 1d. The repair, and why it is safe

```cpp
virtual void ExtractCallSpecificResults(Message *, ProtocolCallContext *) = 0;
```

appended after `GetProtocolType()` in `src/network/Protocol/ClientProtocol.h`.

`ClientProtocol` is **never instantiated directly** — all five uses in the tree
are base-class initialisers (`RBDataClient() : ClientProtocol(1) {}`) — so
making it abstract cannot break a construction site.

**Verified on the rebuilt tree, not predicted:**

```
??_7ClientProtocol@Quazal@@6B@ : 23 slots   [21] Clone@Protocol  [22] _purecall
??_7RBDataClient@Quazal@@6B@   : 23 slots   [22] ExtractCallSpecificResults@RBDataClient
??_7RBTestClient@Quazal@@6B@   : 23 slots   [22] ExtractCallSpecificResults@RBTestClient
```

Base now matches retail's shape exactly; **subclasses unchanged**, as predicted.

### 1e. Measured

`tools/ab_measure.py --worktree ~/tmp/wt-vtcount2 --from-dirty`, graded
(`name_check`) ruler resolved from `objdiff.json`, **256 leg-B recompiles** so
the change is live and not absent-vs-absent:

| measure | leg A | leg B | Δ |
|---|---|---|---|
| `matched_functions` | 42,276 | 42,276 | **+0** |
| `masked_equal` | 22,911 | 22,911 | +0 |
| honest (`matched − masked`) | 19,365 | 19,365 | +0 |
| `matched_code_percent` | 36.822760 | 36.822760 | **+0.000000 pp** |
| `matched_code` bytes | — | — | **+0 B** |
| `fuzzy` | 48.934082 | 48.934082 | +0.000000 pp |
| **`none`-ruler control** | 44,496 / 43.171413 | 44,496 / 43.171413 | **+0** |
| units at 100% (mpn) | 149 | 149 | 0 reached, **0 fell off** |
| units at 100% (all-rows-fuzzy) | 121 | 121 | 0 reached, **0 fell off** |

**Δ0 was predicted before the run and is the safety check, not the payoff.**
Vtables live in `.rdata`, which is not a scored row, and `mpn` is arg-blind. The
payoff is that a call through slot 22 now dispatches correctly in the native
runtime, and that the base's abstractness now matches retail. Landed on merit
per the standing directive that vtable/struct work is valuable and accuracy
beats headline %.

⚠ Note the `none`-control line the tool printed: `NOT_APPLICABLE — reading only
(kinds=source)`. For a patch containing `source`, a flat `none` is **not** an
alias clearance; the shape is only adjudicable on a map-only patch. Nothing here
touches the map, so no alias question arises.

---

## 2. `OggMap` — 2 retail vs 1 ours. **UNDERDETERMINED (repair), but fully characterised**

### 2a. The brief and BOTH oracles are wrong about this class

`src/system/synth/OggMap.h`, `../rb3/src/system/synth/OggMap.h:5` and
`../dc3-decomp/src/system/synth/OggMap.h:6` **all** declare `class OggMap {`
with no base and one virtual. Retail's own RTTI disagrees:

```
COL @ 0x821f0c2c  numBaseClasses=2
   [0] .?AVOggMap@@                    ncb=1  PMD(m=0,p=-1,v=0)
   [1] .?AVOggValidatorFileSource@@    ncb=0  PMD(m=0,p=-1,v=0)
   vtable @ 0x821a16a4
```

⇒ **`class OggMap : public OggValidatorFileSource`**, a direct non-virtual base
at offset 0. This is the "retail bytes outrank the oracle" mode, with *both*
oracles on the losing side.

`.?AVOggValidatorFileSource@@` is a real type descriptor, sitting in the string
blob **between `.?AVVorbisReader@@` and `.?AVOggMap@@`** — i.e. genuinely part of
the synth/Vorbis cluster, not a coincidental name.

### 2b. Class identity is NOT in doubt

Exactly **one** `OggMap` string exists in the whole image (file offset
`0xc98148`, inside `.?AVOggMap@@`). So retail's `OggMap` and ours are the same
class, and the divergence is real rather than a name collision.

The destructor settles it independently. `0x82bb1b90`:

```
lis  r11, -0x7de6 ; addi r11, r11, 0x16a4   ; r11 = 0x821a16a4 = OggMap's OWN vtable
stw  r11, 0(r3)                             ; => this IS ~OggMap
addi r29, r3, 0x34 ; bl 0x824f1960          ; clear() on the vector at +0x34
lwz  r4, 0x34(r30) ; ... srawi r11,r11,3 ; slwi r3,r11,3 ; bl MemOrPoolFreeSTL
addi r3, r30, 8    ; bl ??1MemStream@@UAA@XZ
```

- `0x824f1960` is map-named `vector<Vector2>::clear` — **`Vector2` is 8 bytes,
  the same as `pair<int,int>`, so that is an ICF fold-survivor spelling of
  `mLookup.clear()`**. The `>>3 <<3` byte-count arithmetic confirms an 8-byte
  element type. This is our `~OggMap() { mLookup.clear(); }`, exactly.
- but the vector's data pointer is at **`this+0x34`**, where our header puts
  `mLookup` at **`0x8`**.

### 2c. Retail's layout

| offset | retail | ours |
|---|---|---|
| 0x00 | vptr | vptr |
| 0x04 | — | `mGran` |
| 0x08 | a `MemStream` (destroyed by `??1MemStream@@UAA@XZ`) | `mLookup` (vector) |
| 0x28 | a polymorphic pointer; slot 0 calls its vtable at `+0x8/+0xc/+0x10/+0x2c` | — |
| 0x30 | `mGran` (inferred) | — |
| 0x34 | `mLookup`, 8-byte elements | — |
| size | **≈0x40** | **0x14** |

The ~0x30 bytes preceding our first member is precisely what a base class at
offset 0 contributes. `OggValidatorFileSource` is therefore ≈0x30 bytes with a
vptr, a contained `MemStream` at +0x8 and a stream pointer at +0x28 — consistent
with the name "FileSource".

### 2d. The missing virtual is PREPENDED, not appended

Retail slot **1** is `??_GOggMap@@UAAPAXI@Z`; slot **0** is `0x82bb1980`. So the
extra virtual comes *before* the destructor in declaration order — i.e. it is
declared by the **base**, not added to `OggMap`. Slot 2 reads `0x00000000`, a
hard stop, so the table is exactly 2.

`0x82bb1980` has image-wide fan-in 2, and **one of those two is its own `.pdata`
entry** (`0x8225e7b0`; `.pdata` is 8-byte `(BeginAddress, unwind)` pairs, which
is why the "second reference" is 0x18 away rather than adjacent). **True fan-in
is 1** — referenced only by OggMap's vtable, so it is not a shared base body
installed in many sibling tables.

Its body: a **0x4b70 (19,312-byte) stack frame**, signature `(this, int)`,
driving the polymorphic stream at `this+0x28` through slots `0x10`, `0xc`,
`0x2c`, `0x8`.

### 2e. Why NOT repaired

Repair requires **inventing an entire class** — `OggValidatorFileSource`, ≈0x30
bytes, member types known only by their destructors, plus one virtual whose name
appears nowhere in the image, in either oracle, or in our tree. It would also
move `mGran`/`mLookup` from `0x4`/`0x8` to `0x30`/`0x34` and change `OggMap`'s
size 0x14 → 0x40, cascading into `VorbisReader` (which embeds an `OggMap` by
value). **A fabricated layout is worse than a documented gap**, and nothing in
the evidence names the members. Deferred deliberately, not overlooked.

**What would settle it:** a name for `0x82bb1980` (it is unnamed in
`target_symbol_map.json`), or identification of the TU around
`0x82bb1600–0x82bb1d68` — a contiguous, entirely unnamed cluster that is almost
certainly `OggMap.cpp` + its base's TU, and which no split unit currently covers
(no `.s` in the tree contains `fn_82bb1980`).

---

## 3. `MCResultMsg` — 2 retail vs 1 ours. **UNDERDETERMINED (repair), body fully decoded**

### 3a. It IS defined; the brief's search missed a macro

`src/system/os/Memcard.h:133`:

```cpp
DECLARE_MESSAGE(MCResultMsg, "memcard_result")
MCResultMsg(MCResult res) : Message(Type(), res) {}
MCResult Result() const { return (MCResult)mData->Int(2); }
END_MESSAGE
```

(rb3-Wii uses the type string `"mc_result"`; the 360 string is
`"memcard_result"`. That divergence is already handled in our tree.)

### 3b. The fan-out control — MCResultMsg is 1 of 173

Every class in the sweep whose name contains `Msg`:

| retail slots | ours | count |
|---:|---:|---:|
| 1 | 1 | **107** |
| 1 | 0 | 3 |
| **2** | **1** | **1 — MCResultMsg, the sole outlier** |
| 8 | 8 | 42 |
| 8 | 0 | 5 |
| 9 | 9 | 7 |
| 9 | 0 | 1 |
| 21 | 21 | 7 |

The plain `Message`-subclass shape is **1 slot** — the folded
`??_GMessage@@UAAPAXI@Z` (`0x82342f58`, fan-in 112). The 8/9/21-slot families are
`NetMessage` subclasses with their own `ByteCode()`/`Name()` virtuals, and they
all match. **MCResultMsg is the only `Message` subclass in the entire game with
an extra virtual.**

### 3c. The body decodes exactly

Slot 1 = `0x8254ca60`, true fan-in 1:

```
lwz  r4, 4(r3)          ; this->mData        (Message: vptr@0, mData@4)
lwz  r11, 0(r4)         ; mData->mNodes
addi r3, r11, 0x10      ; &mNodes[2]         (DataNode is 8 bytes)
bl   ?Int@DataNode@@QBAHPBVDataArray@@@Z     ; == mData->Int(2) == Result()
addi r4, r11, 0x3d5c    ; r4 = 0x82093d5c -> the literal "res:"
bl   ??6TextStream@@QAAAAV0@VSymbol@@@Z
bl   ??6TextStream@@QAAAAV0@H@Z
```

⇒ **`ts << "res:" << Result();`** — a debug-print virtual. The literal at
`0x82093d5c` sits immediately after the 2-slot vtable, which is also the
independent confirmation that the table is exactly 2 slots long.

(`operator<<(Symbol)` and `operator<<(const char*)` fold if `Symbol` wraps a
`char*`, so that spelling is very likely another fold survivor and the argument
is the plain literal.)

The emitting TU is `SaveLoadManager.cpp` — `?Type@MCResultMsg@@SA?AVSymbol@@XZ`
(`0x8254be10`) and `?GetDialogOpt3@SaveLoadManager@@` (`0x8254c9c8`) bracket it —
so the virtual is an **inline member of `MCResultMsg` whose COMDAT
`SaveLoadManager.cpp` won**, i.e. it is declared in the header, as expected for
a `DECLARE_MESSAGE` class.

### 3d. Why NOT repaired

The **name and signature are unrecoverable from the image**: the map names only
`??0MCResultMsg@@QAA@W4MCResult@@@Z` and `?Type@MCResultMsg@@SA?AVSymbol@@XZ` for
this class, `0x8254ca60` is unnamed, and no oracle has a print virtual anywhere
on `Message`, on `MCResultMsg`, or in `Memcard.h`. There is no sibling to fan out
against — it is the only such class in 173.

Adding an invented method name to make a count match is fabrication and would
also mint an unpairable symbol. **The shape is known; the identity is not.**

**What would settle it:** a name for `0x8254ca60`, or any RB3-360 artifact
naming a `Message` print/dump virtual.

---

## 4. What this lane did NOT do

- **Did not touch `scripts/target_symbol_map.json` or
  `config/45410914/splits.txt`** — lane THUNK3 was live on both.
- **Did not work `SynapseAPO@DSP` or `?$CSampleXAPOBase@…`** (d=−14 and −4) —
  XDK-derived, out of scope by standing directive. They reproduce unchanged.
- **Did not re-open `Server`** (d=+1) — MESHCOUNT proved it underdetermined; it
  reproduces at retail 19 / ours 20.
- **Did not name `0x82bb1980` or `0x8254ca60`**, and deliberately did not invent
  names for them.
- **Did not attempt the `OggValidatorFileSource` reconstruction.** It is a whole
  missing class, and a wrong layout would cascade into `VorbisReader`.

## 5. COUNT rows after the fix

Re-derived by re-running `tools/vtable_order_sweep.py --json` on the rebuilt
tree — 8 COUNT mismatches before, **7 after**; `ClientProtocol@Quazal` is gone
and nothing else moved.

| class | before | after |
|---|---|---|
| `ClientProtocol@Quazal` | ours 22 / retail 23 | **RESOLVED — 23/23** |
| `OggMap` | 1 / 2 | 1 / 2 (underdetermined) |
| `MCResultMsg` | 1 / 2 | 1 / 2 (underdetermined) |
| `Server` | 20 / 19 | unchanged (MESHCOUNT: underdetermined) |
| `SynapseAPO@DSP` ×2 | 4/18, 1/5 | unchanged (XDK, out of scope) |
| `?$CSampleXAPOBase@…` ×2 | 4/18, 1/5 | unchanged (XDK, out of scope) |

## 6. Reusable lessons

1. **A COUNT class is tractable iff a sibling resolves the same slot
   differently.** Not iff its names are clean. This inverted the brief's
   difficulty ordering for the second lane running (cf. MESHCOUNT §0).
2. **`.pdata` inflates fan-in by exactly 1.** `.pdata` is 8-byte
   `(BeginAddress, unwind)` pairs, so a function's own unwind record is an
   image-wide word reference to it. A raw fan-in of 2 on a vtable slot means
   **true fan-in 1** — "referenced by one vtable" — not "shared by two". Reading
   it as 2 would have wrongly demoted OggMap's slot 0 to a shared base body.
3. **A macro-generated class is invisible to a class-definition search.**
   `DECLARE_MESSAGE(X, …)` defines `class X` with no greppable `class X`.
4. **A fold-survivor name inside an obviously unrelated subsystem is expected
   output, not a defect.** `GetCrowdMeter@TrackPanelDirBase` twice in a Quazal
   protocol vtable is what `return 0` looks like after `/OPT:ICF`.
5. **"Underdetermined" is cheap to say and expensive to say *usefully*.** Both
   deferrals here carry the exact artifact that would settle them: a name for
   one specific address.
