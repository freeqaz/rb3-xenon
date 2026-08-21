# Spurious `virtual` declarations found by vtable SLOT-COUNT comparison — 2026-08-20

> **STATUS (2026-08-20): CURRENT.** Lane VTGRIND. Extends
> `VTABLE_ORDER_SWEEP_2026-08-19.md`, which compared slot *order* and found 0
> confirmed bugs. This lane reads the same instrument for slot **count** and
> finds 5 real declaration defects across 5 classes.
> ⛔ **Byte value ≈ 0. This is a CORRECTNESS fix, priced as one.**

## 1. Why COUNT reaches where ORDER could not

The 08-19 sweep bounded itself honestly: **1,733 of 2,220 retail vtables
unadjudicated**, because turning a retail slot into a *name* needs
`target_symbol_map.json` (90.2% named) **and** an unfolded address (24.3%) ⇒
only ~20.4% of slots are comparable.

★ **Slot COUNT needs no names at all.** Counting a retail table's extent and
counting our `??_7X@@6B@` relocations are both name-free, so the count
comparison covers **1,190 classes** where order covered 369.

⇒ Same instrument, different read, ~3× the coverage.

★ **And it can AGREE: 1,119 of 1,190 counts match exactly.** That control is
what the order sweep initially lacked — its first run read `SAME = 0`, the
signature of a comparator that *cannot* return agreement. A 94.0% agreement
rate means a disagreement carries information.

## 2. ⛔ 46 of the 71 disagreements are ARTIFACT — and the artifact looks exactly like a finding

| population | mismatch rate |
|---|---:|
| classes with >1 retail vtable | 46 / 464 = **9.9%** |
| classes with exactly 1 | 25 / 726 = **3.4%** |

**Multiple/virtual inheritance is the confound.** Our COFF exposes a single
`??_7X@@6B@`; retail has one table per subobject. Comparing ours against a
*secondary* table compares two different things.

⚠ **The trap is that it clusters, which reads as corroboration.** The raw `+1`
band contained `RndPostProc`, `NgPostProc`, `PostProcessor`, `NgDOFProc`,
`SpotlightDrawer`, `NgSpotlightDrawer`, `RndSoftParticleBuffer` (all 6-vs-5)
and six `*SessionJob` classes — *"25 independent declaration bugs"* that were
one mechanism. `RndPostProc` has **two** retail vtables (5-slot at
`0x82063a34`, 28-slot at `0x82063a4c`); the 5-slot one is the `PostProcessor`
subobject, and the sweep did **not** flag the class ambiguous.

⇒ **Gate every count comparison on "this class has exactly one retail vtable".**

## 3. ⛔ A second instrument defect, caught by hand-checking one case

To separate *interior* mismatches (a missing virtue shifts every later slot ⇒
breaks callers) from *trailing* ones (benign), a longest-common-prefix scan was
written. It reported `XboxContent` as `INTERIOR@3` — but slots 0–13 had already
been read by hand as aligning exactly.

**Cause: the LCP forgave `<unnamed>` retail slots but CHARGED *folded* ones.**
`XboxContent` slot 3 is a named ICF survivor
(`??$Obj@VCharPollable@@@DataNode@@…`) occupying the `Location` slot.

⇒ This is the **same fold-poisoning** that produced `SAME = 0` on 08-19, rebuilt
one day later in a new instrument. Re-using the sweep's own fold-aware
`mismatches` list gives the honest answer: **22 of 25 single-vtable survivors
have ZERO comparable-slot disagreements** — the count differs only in
folded/unnamed/trailing positions.

## 4. The 5 confirmed defects

Every one is a **new** virtual (the base declares no such method), so MSVC
appends it and our table runs one slot long.

| class | our slots | retail | spurious member |
|---|---:|---:|---|
| `NetStream` | 12 | 11 | `virtual int ReadAsync(void*, int)` |
| `BufStream` | 12 | 11 | `virtual int Size()` |
| `IDataChunk` | 12 | 11 | `virtual ChunkHeader *Header()` |
| `WaveFileData` | 12 | 11 | *(inherits `IDataChunk`'s — one cause, two classes)* |
| `FixedSizeSaveableStream` | 14 | 11 | `BufStream::Size` + `FinishWrite` + `FinishStream` |

★ **`FixedSizeSaveableStream` decomposes its own +3 exactly**, which is the
strongest single piece of evidence here: retail gives it **11 slots, the same
count as its `BufStream` base**, so retail adds no new virtuals at all; ours
measured 14; and the three spurious declarations account for 3 of 3.

### Evidence per fix (retail bytes, not the metric)

- **`NetStream`**: retail `??_7NetStream@@6B@` at `0x8208da10` — the word after
  slot 10 is `0x00000000`, followed by `0x19930522` (MSVC `FuncInfo` EH magic).
  The table demonstrably **ends**; it is not a truncated read.
  Corroborating: retail's map carries `ReadAsync` for `FileCacheFile`,
  `HDCache` and `CacheXbox` — **never `NetStream`**.
- **`BinStream` declares neither `Size()` nor `Header()`** ⇒ both are new
  virtuals, confirmed by our slot [11] being literally `?Size@BufStream@@UAAHXZ`
  / `?Header@IDataChunk@@UAAPAVChunkHeader@@XZ`.
- **`FinishWrite` / `FinishStream`** are declared and **never called and never
  overridden** anywhere in `src/`.

### Safety check that gates all of them

Removing `virtual` is only safe if nothing dispatches through the slot:

| class | subclasses | overrides the member? |
|---|---|---|
| `NetStream` | **none** | — |
| `BufStream` | `FixedSizeSaveableStream` | **no** |
| `IDataChunk` | `WaveFileData` | **no** |
| `FixedSizeSaveableStream` | **none** | — |

⇒ Devirtualizing changes **no** dispatch. The methods remain callable directly.

## 5. What this is worth, honestly — MEASURED

**Prediction registered BEFORE the run: ≈ 0 bytes.** A vtable is `.rdata`, and
`total_code` is exactly Σ(listed function sizes), so vtable data is not in the
denominator; and a *trailing* slot shifts no earlier slot, so no caller's vcall
displacement changes.

`tools/ab_measure.py --from-dirty`, both legs settled, tree restored:

```
leg A: matched=44514 masked=22911 honest=21603 code%=36.686474  (recompiles: 0, settled)
leg B: matched=44514 masked=22911 honest=21603 code%=36.686474  (recompiles: 331)
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.000000pp   units at 100%: 255 -> 255 (mpn), 121 -> 121 (fuzzy)
```

★ **The +0 is a CONFIRMED prediction, not a null result** — and it is not
absent-vs-absent: leg B recompiled **331 TUs**, so the change was live and
measured. `ab_measure` refuses an unrecompiled source patch precisely so a
`Δ0` cannot be manufactured.

⇒ The payout is **runtime correctness for the native port** (dispatch through a
slot retail does not have) and structural fidelity to retail — the standing
directive that accuracy outranks headline %. Anyone re-funding this vein for
BYTES should stop here: the number is zero and it is supposed to be.

## 5a. ⛔ The verification that matters is retail's vtable, not the metric

Since Δ = 0 on every scoring key, **the metric cannot confirm or refute these
fixes.** The instrument that can is the slot count itself, re-run after the
build:

| class | before | after | retail |
|---|---:|---:|---:|
| `NetStream` | 12 | **11** | 11 |
| `BufStream` | 12 | **11** | 11 |
| `IDataChunk` | 12 | **11** | 11 |
| `WaveFileData` | 12 | **11** | 11 |
| `FixedSizeSaveableStream` | 14 | **11** | 11 |

★ `FixedSizeSaveableStream`'s 14 → 11 was **predicted before the edit** from the
three-way decomposition and landed exactly.

## 6. Not done

- ⛔ **`XboxContent` is MISSING a virtual (retail slot [14], `0x8251f8f0`) —
  INVESTIGATED AND DELIBERATELY NOT FIXED, because the evidence contradicts
  itself.** The attractive story: dc3-decomp's `Content` declares
  `virtual bool IsCorrupt()`, ours declares `IsCorrupt()` **non-virtual** in
  `ContentMgr_Xbox.h`, and adding `virtual` would supply exactly one slot.
  Three things refuse it:
  1. **Position.** dc3 puts `IsCorrupt` at slot 6 (after `HasValidLicenseBits`);
     retail's slots 1–13 match ours *exactly* and the extra is **trailing**.
  2. **Body.** `0x8251f8f0` is `lwz r11,0xc(r3); addi -1; cntlzw; rlwinm` ⇒
     `return this->field_0xc == 1`. Our `XboxContent::IsCorrupt` reads
     `mState` (**0x160**) and `mCorrupt` (**0x161**). Not the same function.
  3. **Identifiability.** It is a 20-byte leaf absent from `.pdata` (the
     AUDIT-NC tiny-stub stratum) whose shape — `return field == 1` — is a prime
     ICF fold candidate, so its body may not identify the method at all. And the
     map's names may be positionally derived, the same circularity that killed
     the `StreamReceiver360` "finding" on 08-19.
  ⇒ Adding `virtual` here would be a plausible story overriding contradictory
  bytes. Left open.
- `PostProcessor` (6 vs 5) is **not** fixed: it is a base of the
  multiply-inheriting `RndPostProc`, so its table is reached as a secondary
  subobject and the single-vtable gate does not clear it.
> ✅ **`RndFont` RESOLVED in a second wave — see §8 below.** 34 → **21**, exact.

- ★ **`RndFont` (+13) — the original deferral note.** Ours 34, retail
  **21**, and 21 is exactly `Hmx::Object`'s slot count ⇒ retail's `RndFont`
  declares **no new virtuals at all**. Verified against retail bytes rather
  than the count alone: the vtable at `0x8206d344` has slot[21] = `0xffffffff`
  (not a function VA) followed by EH state-table entries, so the table
  demonstrably ends at 21; the next *enumerated* vtable is 471 words away, so
  the bound is not what stopped the read. It has exactly **one** retail vtable
  and **no subclasses** in `src/`, so all ~12 accessors (`CharWidth`,
  `CharAdvance` ×2, `Kerning`, `CharDefined`, `AspectRatio`, `Mat`,
  `DataOwner`, `FontUnit`, `FontUnitInverse`, `Print`, `BitmapFont`) are
  candidates for devirtualization. Plausible mechanism: RB3 has only bitmap
  fonts and keeps 3D fonts in the separate `RndFontBase`/`RndFont3d`
  hierarchy, while DC3 (newer) made these virtual for a shared base.
  ⚠ Not done here because a 13-slot change deserves its own A/B + gate rather
  than riding on five small verified ones.
- `RndFur` (−2), `NoteVoiceInst` (+1), `ClientProtocol@Quazal` (−1),
  `MCResultMsg`/`OggMap` (−1) — remaining single-vtable survivors, not
  adjudicated.

## 7. Verification run for this lane

- whole-binary A/B: `Δ+0` on every key, 331 leg-B recompiles (§5)
- `tools/native_build_gate.sh`:
  `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`
- slot counts re-read against retail after the build: 5/5 now `ours == retail`
- 18 of the 25 survivors have **covered ≤ 1**, i.e. essentially no name-level
  corroboration; they rest on the count alone.

## 8. Wave 2 — `RndFont` 34 → 21, and a HALF-WRONG in-tree claim corrected

### 8.1 The claim that had to be resolved first

`rndobj/Font.h` carried a deliberate decision:

> *"The VTABLE is deliberately NOT changed to rb3-Wii's. Retail's CharDefined
> and Print are mangled `?...@RndFont@@UB...` — public virtual const — whereas
> rb3-Wii declares both non-virtual."*

⛔ **A mangled name in `scripts/target_symbol_map.json` is NOT retail evidence
about virtuality.** The map is populated by our own matching, so a `UB` spelling
is our declaration reflected back — the same circularity that killed the
`StreamReceiver360` "finding" on 08-19.

★ **Vtable MEMBERSHIP is retail bytes, and it adjudicates each member
separately.** That is what makes the prior claim *half* right:

| symbol | body | in retail's vtable? | verdict |
|---|---|---|---|
| `?Print@RndFont@@` | `0x82472C18` | **YES** | virtual — prior claim RIGHT |
| `?CharDefined@RndFont@@` | `0x82473A98` | **no** | not virtual — prior claim WRONG |
| `?CharWidth@RndFont@@` | `0x82474478` | **no** | not virtual |

⇒ The instrument the prior lane used **could not distinguish these two cases**,
which is why it got one of them wrong. The `UB` spelling is identical either way.

### 8.2 The two defects

1. **`Print() const` did not override.** `Hmx::Object::Print()` is non-const, so
   a `const` override is a *different signature*: MSVC keeps `Object::Print` in
   slot 13 **and appends a new slot**. Retail puts a `Print@RndFont` body in slot
   13 ⇒ it overrides ⇒ same signature ⇒ non-const. Corroborated by the oracles:
   **rb3-Wii declares it non-const; dc3 declares it const and we inherited dc3's**
   — the standing "cross-check dc3 against rb3-Wii, dc3 is NEWER" rule paying out.
   Further corroboration: of ~40 retail `Print` overrides in the map, **every
   other one is `UAA` (non-const)**; the lone `UBA` is this symbol.
2. **12 accessors were virtual and retail has no slot for any of them**
   (`CharWidth`, `CharAdvance` ×2, `Kerning`, `CharDefined`, `AspectRatio`,
   `Mat`, `DataOwner`, `FontUnit`, `FontUnitInverse`, `BitmapFont`,
   `SetASCIIChars`). `RndFont` has **no subclasses** in `src/`, so no dispatch
   changes. This generalises what a prior lane had already established
   one-at-a-time for `HasChar` ("retail issues direct `bl` calls with no vtable
   load").

Result: **34 → 21, exactly retail's count**, with slot 13 now `?Print@RndFont@@UAAXXZ`.

### 8.3 ⛔ The first A/B REGRESSED −748 B, and the regression was the MAP

```
Δmatched=-4  Δcode_bytes=-748   unit REGRESSIONS: default/Font (77->73)
```

Four rows went **100.000 → 0.000**: `?Print@RndFont@@UBAXXZ` (352 B),
`?CharDefined@RndFont@@UBA_NG@Z` (152 B), `?CharWidth@RndFont@@UBAMG@Z` (52 B),
`?CharAdvance@RndFont@@UBAMG@Z` (76 B) — every one a **`UBA`** spelling.

★★ **A member function's BODY is identical whether or not it is virtual** — only
the vtable and the call sites change. So those rows were at 100% because our
*bodies* already matched retail, and they fell to 0 purely because our symbols
are now spelled `QBA`/`UAA` while the map still said `UBA`. Pure un-pairing:
*a target row whose base obj cannot define that name reads 0% however correct
our code is.*

⚠ **The row SET was unchanged (108 both legs), so a row-level "renamed away"
check REFUTES the un-pairing hypothesis and is the wrong test.** The un-pairing
is at symbol level *within* a row. Diff `fuzzy` per row, not the row set.

Fixed by renaming the 4 map entries to the spellings **read out of our compiled
COFF** (the CLAUDE.md rule: read the name from COFF, after building — never
guess it), which also guarantees the base obj defines them so the rename cannot
strand a row at permanent 0%.

### 8.4 Re-measured, both legs at a split fixed point

```
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.000875pp   (48.995415 -> 48.996290)
```
native gate: `verdict=PASS expected=18 verified=18 skipped=0 partial=0 rc=0`

⇒ The −748 was entirely the stale map spelling. Net effect: **vtable shape now
exactly retail's, metric-neutral, aggregate fuzzy marginally up.**

---

## 9. Wave 3 — the survivors, and §3's defect finally fixed at the root

### 9a. The instrument was rebuilt properly first

§3 recorded that a longest-common-prefix scan **rebuilt the fold-poisoning
defect one day after it was fixed**, and concluded "any new vtable comparator
must be fold-aware from line one". That conclusion was right and **insufficient**
— it asks the next author to *remember*. The author who wrote the poisoned scan
had personally fixed the same defect the previous day.

So the fold reasoning now lives in **`tools/icf_fold_safe.py`**, where a poisoned
slot is *a value that refuses to answer*: `Slot.__eq__` raises `FoldPoisonError`
and `Slot` is unhashable. A future `if retail != ours:` **crashes** instead of
producing a confident wrong verdict. Gate: `tools/test_icf_fold_safe.py`
(CI, `--self-break`-verified, fixtures are real retail bytes). Full write-up:
`docs/decomp/patterns/icf-fold-poisoning.md`.

Building it surfaced **two criteria this doc did not have**, and one
over-correction caught by measurement:

| | mismatches | SAME | SET_DIFFER | comparable slots |
|---|---|---|---|---|
| fold-blind (what §2/§3 measured) | 141 | 401 | 84 | 2,180 |
| SOFT-as-HARD (over-strict) | 68 | 320 | 23 | **1,652** |
| final (charge rule) | **68** | **451** | **23** | 2,086 |

⛔ **61 of 84 `SET_DIFFER` verdicts were artifacts** — `nonvirtual_name`
(vtable membership proves virtuality, so a `Q`/`A`/`I`/`S` spelling proves the
NAME is wrong) and `unrelated_owner` (a slot of class C can only hold a function
of C or a base of C). `map_audit` already used the first criterion;
`sweep_class` never consulted it, so **the two halves of one file disagreed
about the same slot.**

★ **THE RULE: a suspect name may CONFIRM, but may never ACCUSE.** Treating those
as hard exclusions moved 127 classes `SAME → UNRESOLVED` and destroyed 528
comparable slots — **every one an AGREEING slot**, provably, since a class whose
verdict was `SAME` had zero mismatches by definition. The strictness prevented
**no false defect at all**. A suspect spelling can only manufacture a false
*disagreement*.

### 9b. 13 classes fixed — two causes

| family | classes | cause |
|---|---|---|
| `*SessionJob` | 6 | `CheckError(DWORD, XOVERLAPPED*)` is not virtual in retail |
| `PostProcessor` | 5 (+2) | `GetProcType()` does not exist in retail |

`Job` declares exactly five virtuals and every retail table in that family is
five slots (`StartSessionJob` @`0x82059ddc` slot 3 = `?Cancel@XboxSessionJob@@`).
`MakeSessionJob` does **not** derive from `XboxSessionJob` — the two `CheckError`
declarations are independent, which is why **both roots read +1**.

`PostProcessor` @`0x82063a0c` is five slots and ends there (slot 5 holds
`0x3fadf84d`, a float): `??_G`, then **one folded address for
BeginWorld/EndWorld/DoPost** (all empty `{}`), then `?Priority@PostProcessor@@`.

✅ **This RESOLVES lane BS-3's OPEN note** in `SpotlightDrawer_NG.h`. BS-3
suspected exactly this but deferred because `?GetProcType@` has no
`target_symbol_map` row — **it was waiting on a NAME.** The vtable is a
name-free instrument, so the missing row never blocked it. BS-3's string-pool
reading was independently re-verified and **stands**: both `NgSpotlightDrawer`
hits are RTTI type names, never literals.

### 9c. ⛔ The native gate was RED on main, and it was not this lane

`FAIL 16/18, rc=1` on `Mesh_Wgpu.cpp: cannot refer to type member 'DrawMode' in
'Rnd' with '.'` — reproduced **identically on an unmodified control worktree at
the base commit**, which is the only reason it could be attributed correctly.

Cause: engine `f12d4b9` moved to `TheRnd.DrawMode()` on DC3's shipped-linker-map
proof that retail spells it `DrawMode()` returning `Rnd::Mode`, reasoning that
*"rb3-xenon still uses GetDrawMode() but does not consume this engine"* — **a
false premise**; xenon consumes it via `MILO_ENGINE_PIN`. So **our header was
the wrong one.** Renamed (the enum must move to `Mode` in the same edit, since a
member function cannot share a name with a nested type — that IS the
diagnostic). `Character::DrawMode` is a different enum with real map rows and is
untouched. Gate now `PASS 18/18, rc=0`; A/B Δ0 (the accessor is inline, so no
`?GetDrawMode@Rnd@@` row exists to un-pair).

### 9d. Still open

- **6 count survivors remain unadjudicated**, all `ours < retail` (we are
  MISSING a virtual, which is far harder than deleting a spurious one — you must
  identify *which* function): `RndFur` (−2), `XboxContent` (−1, still refused
  per §6), `ClientProtocol@Quazal` (−1), `MCResultMsg` (−1), `OggMap` (−1).
  `NoteVoiceInst` (+1) and `SampleInst`/`SampleInst360` (+5) are the remaining
  tractable `ours > retail` cases and are the obvious next target.
- **23 `SET_DIFFER` classes survive the new filters** — these are the honest
  order-defect worklist, down from 84.
- **`withheld` pairs are now reported per class** (`vtable_order_sweep --json`).
  They are a byte-adjudication worklist, not noise; nothing about them is
  silently dropped.
- The engine compares `DrawMode() == 8` while our enum tops out at
  `kDrawVelocity = 6`, so both two-sided-cull overrides are dead on every
  consumer. **Whether retail's `Rnd::Mode` has values above 6 is unanswered.**

---

## 10. Wave 4 (2026-08-21) — the tractable `ours > retail` survivors, drained

§9d named `NoteVoiceInst` (+1) and `SampleInst`/`SampleInst360` (+5) as the
obvious next target. Both are now at retail's slot count.

### 10a. `NoteVoiceInst` 31 → 30 — one insertion, not two errors

The sweep charged two mismatches, and they were a single shift:

| slot | retail | ours (before) |
|---|---|---|
| 28 | `?SetPan@NoteVoiceInst@@UAAXM@Z` | `?UpdatePan@NoteVoiceInst@@UAAXXZ` |
| 29 | `?SetVolume@NoteVoiceInst@@UAAXM@Z` | `?SetPan@NoteVoiceInst@@UAAXM@Z` |

Slots 0–27 already agreed in count, so the inserted entry was the **sole**
extra — which is why deleting one declaration closed the count *and* both
order charges at once.

`UpdatePan` is **DC3-only**. Four independent lines agree:

1. retail's vtable has no such slot (30 vs 31, and the shift above);
2. the **rb3-Wii oracle** declares exactly `SetTranspose` / `UpdateVolume` /
   `SetPan` / `SetVolume`, no `UpdatePan`;
3. **dc3-decomp — which is NEWER than RB3** — has it *and* calls it from
   `MidiInstrument::Poll`;
4. nothing in this tree called it: a prior lane had already removed the **call**
   (its note survives in `MidiInstrument.cpp`) but left the **declaration**, so
   the vtable stayed one slot long.

⚠ **The Wii negative was checked for VACUITY before being relied on.** rb3-Wii
does carry `MidiInstrument.h`/`.cpp` and does declare `NoteVoiceInst`, so the
absence of `UpdatePan` is a real absence rather than a missing file. A "zero
hits" that means "the file isn't there" is the standard trap.

Removed declaration **and** definition. The body was `mSample->SetPan(0.0f)` —
hard-centre the voice — i.e. live behaviour, not a stub, and not something to
inherit by accident.

### 10b. `SampleInst` / `SampleInst360` 40 → 35 — the answer was already in the header

Both classes read retail 35 / ours 40. The identical +5 pointed at the shared
base, and **an earlier lane had already pinned the answer from retail bytes**
in `SampleInst.h`: `SetVolumeImpl` slot 29 → `0x74`, `SetPanImpl` 30 → `0x78`,
`SetSpeedImpl` 31 → `0x7c`, so retail's vtable is slots 0–34 = **35** and ENDS
at `SetReverbEnableImpl`. Everything declared after that prefix is over-length.

Exactly five such entries existed: `Play`, `Stop`, `DonePlaying`, `EndLoopImpl`,
`ElapsedTime`. Four of them exist **only to satisfy `PlayableSample`'s pure
virtuals**, and `SampleInst` derives `PlayableSample` **only under `HX_NATIVE`**.
They are now `virtual` in the native build and plain in the matching build
(`SAMPLEINST_NATIVE_VIRTUAL`).

★ **Devirtualization, not deletion, and the distinction is forced by evidence
on both sides.** `Sound.cpp`, `Sfx.cpp` and `Synth.cpp` all call these, so they
must exist; `SampleInst360` is the **only** class deriving from `SampleInst` and
overrides **none** of the five, so virtual and non-virtual dispatch resolve to
the same function and the change is behaviour-preserving. This is the same
finding, and the same treatment, that the non-virtual setters in that header
already carried.

⚠ **The earlier lane's tail-parking was not wrong, it was incomplete.** Trailing
slots cannot perturb slots 21–34, so dispatch was already correct. What survived
was the count.

### 10c. Result, by slot recount

| class | retail | ours | verdict | comparable slots |
|---|---|---|---|---|
| `NoteVoiceInst` | 30 | **30** | SAME | 8 |
| `SampleInst360` | 35 | **35** | SAME | 4 |
| `SampleInst` | 35 | **35** | **UNRESOLVED** | **0** |

★★ **`SampleInst` is `UNRESOLVED` and deliberately NOT `SAME`.** 34 of its 35
slots are ICF fold-poisoned, so the **count is verified and the order is not**.
Reporting SAME there would be precisely the confident-wrong-verdict this
tooling was built to prevent — the guard refusing to answer *is* the feature.

### 10d. Measurement — a falsifiable one, for once

Unlike the pure-vtable waves (where Δ0 is pre-registered because vtables are
`.rdata`), this wave **moves `.text`**: devirtualizing rewrites `lwz`/`mtctr`/
`bctrl` call sites into direct `bl`. That makes it a real test of the reading
that retail calls these directly. **A clear regression would have refuted it.**

```
ab_measure --from-dirty, both legs settled, 106 real leg-B recompiles
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δbytes=+0
Δfuzzy=+0.000237pp   (48.992160 -> 48.992397)
units at 100%: mpn 150->150, all-rows-fuzzy 122->122
```

The sign is right and the magnitude is small. **The +0.000237pp is not claimed
as a win** — it is reported because a *negative* value would have been evidence
the retail reading was wrong. No row crossed `fuzzy == 100`, so `matched_code`
(all-or-nothing per row) is flat.

`NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`

### 10e. Still open after wave 4

All remaining count survivors are `ours < retail` — the hard direction, since
identifying a **missing** virtual requires a name: `RndFur` (−2), `XboxContent`
(−1, still refused per §6), `ClientProtocol@Quazal` (−1), `MCResultMsg` (−1),
`OggMap` (−1). The 23 `SET_DIFFER` order classes and the
410 `AMBIGUOUS_MULTI_VTABLE` / 1,334 `UNRESOLVED` populations are untouched by
this wave.

---

## 11. Wave 5 (2026-08-21) — `PERMUTED` was 100% artifact, and our source was the correct side

### 11a. The case that started it, and inverted

`StreamReceiver360` reported a clean 2-slot swap at 13/14
(`GetPlayCursor` ↔ `PlayImpl`). Because an override takes the slot its **base**
defined, the fix belonged in `StreamReceiver.h` — where a prior lane's comment
already asserted the **opposite** order, citing retail bytes. One of the two
instruments had to be wrong.

Adjudicated on retail bytes, and the answer was neither what I expected nor
what the sweep said:

- retail's vtable `@0x8219754C` really does hold `0x82B6BAE8` at slot 13 and
  `0x82B6BAF8` at slot 14, and `target_symbol_map.json` really does name those
  `GetPlayCursor` and `PlayImpl`. **The sweep read its input correctly.**
- **but both are 16-byte tail-call thunks with byte-identical bodies except the
  branch displacement.** Their addresses are DISTINCT, so `occ == 1` and both
  fold filters passed them as fully comparable — yet nothing in the bytes says
  which name belongs to which.
- **the call site settles it.** `StreamReceiver::Play` dispatches slot `0x30`
  with `li r4,0` (an argument ⇒ `PauseImpl(bool)`, consistent either way) and
  dispatches `0x34` with **no argument and the result DISCARDED**. `Play()`
  calling `GetPlayCursor()` and throwing the `int` away is not a plausible
  reading; the play path calling `PlayImpl()` is.

⇒ **Our header is RIGHT and the MAP has the two thunk names swapped.** Had the
source been "fixed" to match the sweep, correct code would have been broken.
The prior lane's conclusion stands; what its comment lacked was any warning
that the map disagrees — which is exactly why the sweep blindsided a later
reader. That warning is now in the header.

### 11b. ⚠ My own first measurement of the class was WRONG

An initial probe reported **2 of 66** charged mismatches as thunk twins, which
would have made this a one-instance curiosity not worth encoding. The probe
required a **4-instruction** body ending in `b`, so it **missed 3-instruction
adjustor thunks entirely**. Corrected: **7**, and they account for **both
`PERMUTED` classes in full** — `UIFontImporter` 5/5, `StreamReceiver360` 2/2.

★ The lesson is the ordinary one and it nearly cost the finding: **a detector's
threshold is part of its result.** A "small, ignorable" class was an artifact of
where the cutoff sat, not of the population.

### 11c. The criterion

`mark_thunk_twins()` SOFT-marks retail slots that are **shape-identical
tail-call thunks colliding within one vtable**. The shape deliberately
**EXCLUDES the branch instruction**, since the displacement is the only thing
that differs between twins — with a control test for the converse hazard (two
thunks of *different* shape that both merely end in `b` must NOT pair).

SOFT, not HARD, for the same reason as every other suspect class here:
**twins that AGREE need no forgiveness** — our side would have to independently
produce the identical mangled name — so only a DISAGREEMENT is withheld, and
withheld means *returned as a byte-adjudication worklist item*, never dropped.

### 11d. Measured, full sweep

| verdict | before | after |
|---|---:|---:|
| `PERMUTED` | 2 | **0** |
| `SAME` | 452 | **454** |
| `SET_DIFFER` | 22 | **22** |
| `AMBIGUOUS_MULTI_VTABLE` | 410 | 410 |
| `UNRESOLVED` | 1,334 | 1,334 |
| charged mismatches | 66 | 58 |
| withheld | 94 | 102 |
| comparable slots | 2,086 | 2,078 |

★★ **`SET_DIFFER` not moving is the important row.** The criterion removed
exactly the artifact class without weakening the real order-defect worklist by
a single row. And **`PERMUTED` — which by construction means "same name set,
wrong order", the most confident defect the sweep can report — was 100%
artifact.**

⚠ **NOT DONE: fixing the map.** A map edit's delta is mostly *un-pairing* rather
than cascade, so swapping those two names is its own measured lane and not a
free rename. Until then the map stays wrong and the guard withholds rather than
believing it.

Tests: 14 checks, both fixtures anchored on real retail bytes, `--self-break`
still fails as required. Nothing is hardcoded to a verdict — shapes are
recomputed from bytes every run.

---

## §12 — wave 6 (2026-08-21): the sweep was joining the wrong table, and what was left is a MAP worklist, not a source worklist

Two results, one good and one that closes a vein. **Zero source defects were
found, and the two source edits this lane made were both REVERTED after retail
bytes refuted them.** That is the finding, not a preamble to one.

### 12a. The our-side table was chosen by MANGLED NAME, and the rule was false

`sweep_class` picked our vtable as bare `??_7X@@6B@`, treating it as "the
primary (offset-0) table". `cl /d1reportSingleClassLayoutCustomizePanel` says
otherwise for `class CustomizePanel : public UIPanel, public ContentMgr::Callback`:

```
0x0   {vfptr} [UIPanel]                <- retail COL.offset == 0 means THIS
0x3c  {vfptr} [Callback]               <- and THIS is ??_7CustomizePanel@@6B@
0xb8  {vfptr} [Object > ObjRefOwner]
=== vtable CustomizePanel@UIPanel@ (15 slots) ===
```

So the bare name is a **secondary** table. Comparing it against retail's primary
aligns two different tables, and *every* covered slot disagrees — which is
exactly what a `SET_DIFFER` then reports. Our side's mismatching slots were all
`ContentMgr::Callback` methods while retail's were all `UIPanel` methods, and
`??_7CustomizePanel@@6BUIPanel@@@` agrees with retail on all six.

**Fix: join OFFSET to OFFSET.** We build `/GR`, so our objs carry `??_R4` COLs
whose names parallel the `??_7` vftables one-for-one, and a COL states its own
offset (big-endian DWORD at +4). Both sides are then authoritative RTTI and no
mangled-name rule survives in the path. Ambiguity **refuses**
(`AMBIGUOUS_MULTI_VTABLE`) rather than guessing.

| verdict | before | after |
|---|---:|---:|
| `AMBIGUOUS_MULTI_VTABLE` | 410 | **3** |
| `SAME` | 454 | **959** |
| `SET_DIFFER` | 22 | **15** |
| `PERMUTED` | 0 | **1** |
| comparable slots charged | 2,078 | **5,082** |

★★ **The transition table is the evidence, not the totals.** All **17**
multi-vtable `SET_DIFFER` became `SAME`; all **5** single-vtable ones — where no
ambiguity is possible — are unchanged **with identical coverage**; regressions
(was `SAME`, now a defect) = **ZERO**. Resolving 407 previously-refused classes
also surfaced 11 new candidates, so the refusal had been hiding leads, not only
noise.

⚠ **Endianness is load-bearing and offset 0 cannot detect it.** A little-endian
read returns `0x3c` as `1006632960` — not obviously wrong, just a number no
retail offset equals, so every secondary join silently MISSES and degrades to
"ambiguous". The selftest pins `0x3c` and `0xb8` for exactly this reason;
a test using only the primary table would have passed while the join was broken.
Mutation-verified: flipping to `<I` gives `SELFTEST FAIL`.

### 12b. ⛔⛔ THE CALL SITE OUTRANKS THE MAP NAME, THE ORACLE, AND THE BODY SHAPE

Two source edits were made on what looked like strong evidence. **Both were
wrong**, and the same instrument caught both: retail's own compiled **dispatch
offset**, which encodes the slot number as an immediate.

**`TrackWatcherImpl` — reverted.** Moving `RGFretButtonDown` after
`FretButtonUp` measured **−2 matched / −40 B**, all in `default/TrackWatcher`.
The two regressed rows name the cause:

```
?FretButtonUp@TrackWatcher@@QAAXH@Z      100.000 -> 99.800   (20 B)
?RGFretButtonDown@TrackWatcher@@QAAXH@Z  100.000 -> 99.800   (20 B)
  [2]  target `lwz r11, 0x30(r11)`   base `lwz r11, 0x2c(r11)`   diff_arg
```

Retail dispatches `FretButtonUp` at **0x30 = slot 12** — our original order.

**`Cache` — reverted.** Toggle test on `SaveLoadManager::SetState`, one
instruction flips:

| state | instruction 369 |
|---|---|
| `GetFileSizeAsync` moved to slot 6 | target `0x1c` vs base `0x18` → **mismatch** |
| original order | no mismatch (both `0x1c`) |

Retail dispatches it at **0x1c = slot 7** — our original order.

★★★ **Why the "three independent confirmations" were not three.** This is the
reusable lesson:

1. **The map name at the disputed slot IS THE CLAIM UNDER TEST.** Counting it as
   evidence for itself is circular, and it read as one of three agreeing
   sources.
2. **The rb3-Wii oracle is a different build and does not bind retail-360's
   declaration order.** It agreed with the map here and both were wrong —
   cf. `project_oracle_fidelity_has_four_modes_2026-08-17`: retail bytes outrank
   the oracle in every mode.
3. ⛔ **The body-shape argument was CIRCULAR IN ITS INDEX.** Retail's slot 12
   tail-calls virtual slot `0xb8/4 == 46`, and *our* slot 46 is
   `RecordFretButtonDown`, so it "must" be `RGFretButtonDown`. But **retail's
   slot 46 is read through OUR numbering, which is the very thing in dispute** —
   retail's 46 is `RecordFretButtonUp` (our 47), so the thunk actually says
   slot 12 is `FretButtonUp`, agreeing with the call site and refuting the edit.
   *An off-by-one in the numbering you are testing is invisible to a test that
   uses that numbering.*

### 12c. Score card: 6 adjudicated, 6 MAP defects, 0 source defects

Every `SET_DIFFER`/`PERMUTED` row adjudicated on retail bytes turned out to be a
**map** defect. None was a source defect.

| class | map says | retail bytes say | instrument |
|---|---|---|---|
| `StreamReceiver360` (§11) | `GetPlayCursor`/`PlayImpl` swapped | our header right | call-site arity |
| `Synth360::NewBufStream` | 5 args (`_N`) | **6** — prologue keeps `{r4,r5,r6,r8,r9,f1}`, r7 consumed by the float | prologue |
| `MetaMusicLoader` s4 | `IsLoaded` | `PollLoading`; s2 is `IsLoaded` (`mState == 0x826c3888`, the `blr` stub `DoneLoading`) | body |
| `StandardStream` s52 | `SetJump` | `UpdateTimeByFiltering`; real `SetJump` is unnamed s40, which stores `f1`,`f2` and forwards a `const char*` (`MMPBD`) | body/signature |
| `TrackWatcherImpl` s11 | `FretButtonUp` | slot 12 | **call site 0x30** |
| `CacheXbox` s6/s7 | `GetFileSizeAsync`/`GetDirectoryAsync` | swapped | **call site 0x1c** |

⇒ ★★★★★ **The residual `SET_DIFFER`/`PERMUTED` list measures MAP QUALITY, not
source quality.** Do not fund it as a source-defect lever. It is a good
**map-defect worklist**, and each entry needs retail-byte adjudication before
any edit — the sweep cannot distinguish "our order is wrong" from "the map's
name is wrong", because both produce the identical row.

⚠ **`Cache.h:11-12`'s op-kind table (`kOpFileSize == 1`) and Cache_Xbox.cpp:351
("GetDirectoryAsync stores 0x160") are DOWNSTREAM OF THE SAME WRONG MAP** and
are therefore not independent corroboration. An in-tree record is evidence about
what a previous lane concluded, not proof of the conclusion — check what it was
derived FROM.

### 12d. What was NOT done

- **The map is not fixed.** Six proven defects are recorded above; a map edit's
  delta is mostly un-pairing rather than cascade, so it is its own measured lane.
- `DxShaderMgr` s11 was **deferred, not adjudicated** — only one retail slot in
  that whole region is named (s9/s10/s12/s13 are all unnamed), which is too thin
  to decide `SetPConstant` vs `SetVConstant`.
- The remaining 9 `SET_DIFFER` rows are unadjudicated. Given 6/6 above, expect
  map defects, but expect is not measured.
- 1,242 `UNRESOLVED` remain untouched.

---

## 13. Wave 7 (2026-08-21) — the map worklist, adjudicated and repaired

Wave 6 handed over six "proven map defects" as a worklist. Wave 7 re-adjudicated
each one **from retail bytes rather than from the note**, and the outcome is
worth stating plainly: **one of the six was wrong**, four were right, one is
underdetermined, and two defects wave 6 never saw were found alongside them.

Total, three measured A/B runs, each with both legs settled and at a
`symbols.txt` split fixed point: **+5 matched functions / +648 bytes**, unit net
equal to the whole-binary delta in every run (no unexplained cascade).

| # | wave 6 said | wave 7 measured | Δ |
|---|---|---|---|
| 1 | `StreamReceiver360` GetPlayCursor/PlayImpl swapped | ⛔ **REFUTED — the map is correct** | — |
| 2 | `Synth360::NewBufStream` spells 5 args, retail has 6 | ✅ confirmed, repaired | +1 fn |
| 3 | `MetaMusicLoader` s4 is PollLoading, not IsLoaded | ✅ confirmed, repaired **as a family** | +1 fn |
| 4 | `StandardStream` s52 is not SetJump | ✅ confirmed, repaired | +2 fns / +392 B |
| 5 | `CacheXbox` s6/s7 swapped | ⚠ **UNDERDETERMINED — deliberately not edited** | — |
| 6 | `TrackWatcherImpl` s11 | deferred (42 of its slots are ICF-folded) | — |
| + | *(not in wave 6's list)* `MetaMusicLoader::IsLoaded` unnamed | ✅ named | (in #3) |
| + | *(not in wave 6's list)* `MetaMusicLoader::DebugText` | ✅ named + source fixed | +1 fn / +68 B |

### 13a. ⛔ CORRECTION TO §12c: it is 5 of 6, not 6 of 6

**§12c's headline "6 map defects, 0 source defects" overcounts by one.**
`StreamReceiver360` is **correct as mapped**, and re-checking it took one
objdiff run:

- `PlayImpl` @ `0x82B6BAF8` — **100.0%, 4 of 4 instructions equal.**
- `GetPlayCursor` @ `0x82B6BAE8` — 3 of 4 equal; the sole charge is the
  relocation NAME on the tail-call target, which objdiff itself reports as
  `ICF:?GetAddr@Voice@@QAAHXZ (cross-function merge)`.

The return types corroborate independently: `GetPlayCursor` returns `int` and
tail-calls `Voice::GetAddr()` which returns `int`; `PlayImpl` returns `void` and
tail-calls `Voice::Start()` which returns `void`. **A swap would mismatch both.**

⇒ Wave 6 read an **ICF fold-survivor name** (`??2OutfitConfig@@SAPAXI@Z`) as
evidence of a swap. That is the trap CLAUDE.md already names — *`LINKER_MERGED`
is what a fold LOOKS like* — arriving from the opposite direction: §12 was
written to warn against believing an `AT_LIMIT` label, and then believed a
fold-survivor name.

### 13b. The instrument that worked: adjudicate on the ARGUMENTS

Every confirmed row was settled by something no name can poison — what the
retail prologue does with its **incoming registers**:

- **`Synth360::NewBufStream`** preserves `{f1→f31, r4→r30, r5→r28, r6→r27,
  r8→r26, r9→r25}`. **`r7` is conspicuously absent**, because the float in
  parameter slot 4 consumes it. That is **six** parameters, so the 5-arg map
  spelling named nothing and the row was a stub (44 instructions target-side,
  0 base-side, 0%).
- **`StandardStream` slot 40** opens `stfs f1,0x8c(r3); stfs f2,0x90(r3);
  mr r4,r6` — two incoming floats stored into the object plus a third pointer
  argument ⇒ `(float, float, const char*)`, which **is** `SetJump`'s signature.
  Slot 52, the map's "SetJump", saves `f30`/`f31` as **callee-saves** and
  consumes no float argument at all.
- **`MetaMusicLoader` slot 4** is 12 bytes: `lwz r11,0x2c(r3); mtctr r11; bctr`
  — it **invokes** the state member-function pointer. Our `IsLoaded` loads the
  **same** `+0x2c` and **compares** it against `&DoneLoading`. Same field,
  opposite operation.

### 13c. ★ Fix the FAMILY — and let the base class's declaration order check you

`MetaMusicLoader` is the worked example. Renaming only the flagged row would
have left `IsLoaded` homeless. Retail's vtable resolves the whole class, and
`Loader`'s own declaration order (`~Loader`, `DebugText`, `IsLoaded`,
`StateName`, … `PollLoading`) then pins every slot index **without consulting
the map at all**:

| slot | addr | before | after |
|---|---|---|---|
| 0 | `0x827BFC48` | unnamed | (dtor, left alone) |
| 1 | `0x8270FF88` | unnamed | **DebugText** |
| 2 | `0x8270FF68` | unnamed | **IsLoaded** |
| 3 | `0x8270FEC8` | StateName | StateName (was already right) |
| 4 | `0x8270FED8` | **IsLoaded** ✗ | **PollLoading** |

Four independent lines agree on this table — body semantics, retail `.rdata`,
our own compiled slot order, and the base class's declaration order. The sweep
now reads `SAME` for the class with **charged slots 2 → 4**: the verdict
improved *and* coverage rose.

### 13d. Two source divergences the map work exposed

Neither is a naming issue; both are real behavioural differences inherited from
the newer dc3 engine, and both were found only because the map row was fixed
first and the body then failed to match.

1. **`MetaMusicLoader::PollLoading` runs ONE state step, not a loop.** Retail's
   entire body is that 12-byte indirect **tail** call — no compare, no branch,
   `bctr` not `bctrl`. Our `while (!TheLoadMgr.CheckSplit() && … && !IsLoaded())`
   form (176 B) cannot compile to it.
2. **`MetaMusicLoader::DebugText` formats the path.** Retail copy-constructs a
   `FilePath` temp from `this+0xc` and tail-calls
   `MakeString<FilePath>("MetaMusic: %s", temp)`; ours returned the bare
   constant `"MetaMusicLoader"`. The unit **already carried the
   `MakeString<FilePath>` instantiation at 100%** — something in the TU had to
   be calling it, and this was it.
   ⚠ `this+0xc` is `Loader::mFile` (a `FilePath`), **not** `MetaMusicLoader`'s
   own `File *mFile` at `0x18`, which **shadows** it.

### 13e. ⚠ `CacheXbox` is UNDERDETERMINED — and that is the finding

This is the row wave 6 got wrong in the *source* direction, and wave 7
deliberately did **not** edit it in the *map* direction either.

Retail's `.rdata` is unambiguous — slot 6 = `0x827DA730`, slot 7 = `0x827D9F40`
— and the two bodies are genuinely distinguishable (nested dispatch
`lwz r11,4(r11)` vs `8(r11)`, op constant `li r10,2` vs `li r10,1`, field
`352(r31)` vs `360(r31)`), so they are **not** folded. What is missing is a
decisive tie to a *name*:

- **Our 100%/100% scores are NOT independent evidence.** Lane CF-10 pinned our
  `kOpFileSize`/`kOpDirectory` enum **from these very bodies**, so the source
  was fitted to the map's assignment. `Cache.h:11-12` and `Cache_Xbox.cpp:351`
  are downstream of the same premise. (§12c already flagged this; wave 7
  confirms it by re-deriving it.)
- **The call site is ambiguous here, unusually.** `SaveLoadManager::SetState`
  dispatches slot 6 **once** (idx 323) and slot 7 **three times** (369, 413,
  710), while our source contains exactly **one** call to either method — and
  **no instruction in 304–419 currently mismatches**, so both hypotheses fit
  the bytes. Wave 6 anchored on idx 369 and read it as decisive; it is not.
  ★ A slot index only means something once you know the **receiver's class**,
  and not every `lwz r11,d(r11)` in a function is the dispatch you are after.
- The two signatures share an **identical ABI shape**
  (`const char*`, pointer, `Hmx::Object*`), so the call bytes cannot separate
  them even in principle — only the slot can, and the slot is what is in doubt.

⇒ **Left alone.** Both wave 6's source edit and its map verdict rest on the same
under-determined reading. Settling it needs a caller whose receiver's static
type is provable, or a second class overriding the same base.

### 13f. What is still open

- 8 `SET_DIFFER` classes remain unadjudicated: `BandSongMgr`, `GemTrackDir`,
  `StreakMeter`, `BandStorePanel`, `BandCamShot`, `ModifierMgr`, `AppLabel`,
  `GameMicManager`. Given 4-of-6 above, expect a mix — **and expect at least one
  refutation**, which is the actual lesson of this wave.
- ★ A cheap prioritiser fell out of the `SetJump` repair: **a named row scoring
  far below its neighbours is the tell for wrong-address pairing.** `SetJump`
  sat at **23.735%** before the fix. The same scan over the classes above
  surfaces `?SetType@StreakMeter@@UAAXVSymbol@@@Z` (**0.000%**, 104 B),
  `?SetDancer@AppLabel@@QAAXVSymbol@@@Z` (0.000%, 112 B) and
  `?HasMic@GameMicManager@@QBA_NABVMicClientID@@@Z` (0.000%, 56 B) — 55 sub-100
  rows across 312. Start there, not at the top of the alphabet.
- `TrackWatcherImpl` remains deferred: 42 of its slots are ICF-folded, its
  vtable is not in the map, and it is the row a source edit already got wrong.
- `MetaMusicLoader::IsLoaded` rests at **98.571%** (6 of 7). Its one charge is a
  relocation name on the fold hub `0x826C3888` (the empty `DoneLoading`, a
  `StlNodeAlloc` ctor and a `CacheXbox` slot all landed there). **Deliberately
  not aliased** — that evidence is equally consistent with "folded" and with
  "the map is wrong", and 28 B does not justify the ambiguity.

---

## 14. Wave 8 (2026-08-21) — the COUNT reached what the NAMES could not, and my own tool was vacuous for an hour

Wave 7 handed over an 8-class `SET_DIFFER` worklist and a prioritiser ("start at
the low-scoring named rows"). **Neither was what paid.** The sweep's own
`retail_slots` vs `our_slots` columns — which nobody had ranked on — separated
the population immediately, and every fix in this wave came from a **count**
mismatch. Result: `SET_DIFFER` **12 → 10**, `SAME` **962 → 964**, ten surplus
vtable slots removed across six classes, plus two map rows and one source body.

⚠ The worklist was also **stale in a way worth noticing**: a fresh sweep found
**12** SET_DIFFER classes, not 8 — `DxShaderMgr` and the three
`*TrackWatcherImpl` variants were never in wave 7's list. *Re-run the sweep;
do not inherit the class list any more than you inherit a ceiling figure.*

### 14a. The instrument: retail's vtable LENGTH

A count mismatch consults **no map**, so it is immune to the failure that made
wave 6 a map worklist rather than a source worklist. It is also cheap to
falsify, which matters more:

- **End-of-table byte control.** `read_retail_slots` bounds a table by the next
  enumerated vtable's COL slot, so a short read is the obvious way to
  manufacture a finding. Dumping the words *past* the claimed end settles it
  every time: `CamShot` (13) is followed by `0xffffffff` then `(VA, index)`
  pairs — an EH/handler table; `BandCamShot` (14) by `0xfffffffc, 0x1ec, 0,
  0xffffffff` — a **vbtable**, and `0x1ec` is exactly the
  `(vtordisp for vbase Object)` offset `cl /d1reportSingleClassLayout` reports.
  Both ended where the tool said.
- **Population control.** `BandCamShot`'s vbase-`Object` table read 14 against
  our 18 — but **29 other classes have that same table at 18/18**, so 14 is not
  a truncated read of a standard table. §2 of this doc records 46 of 71 earlier
  disagreements being artifact that looked exactly like findings; this is the
  cheapest way to not rejoin them.
- **The family check** is what separates *"the derived class invented a
  virtual"* from *"the base declares it and we don't"* — two hypotheses that
  predict the **same** count on the derived class. Run it on the base:
  `SongMgr` 30/30 exact ⇒ BandSongMgr's three extras are in neither side's
  base. `CamShot` 13 vs 17 ⇒ the defect was never BandCamShot's at all.

### 14b. Fixed

| class | was | now | what |
|---|---|---|---|
| `CamShot` (⇒ `BandCamShot`) | 17 / 18 | 13 / 14 | `ApplyDynamicOffsetPreLookAt`, `…PostLookAt`, `ApplyFinalCamTransform`, `ZoomFovOffset` — DC3-era additions, **0 overrides and 0 call sites** tree-wide; an earlier lane had already deleted their retail call sites (`NB(idx233)` in CameraShot.cpp) but left the declarations |
| `BandSongMgr` | 33 | 30 | `AllowCacheWrite` / `SongName(int)` / `CanAddSong`, slots 30/31/32 — exactly the tail block MSVC reserves for a derived class's NEW virtuals, in declaration order |
| `StorePanel` (⇒ `BandStorePanel`) | 30 | 29 | `StoreProfile` de-virtualized |
| `TrackDir` (⇒ `GemTrackDir`) | 36 / 37 | 35 / 36 | `SyncFingerFeedback` |
| `BandSongMgr` map | — | — | `0x82575558` → `ContentPattern`, `0x82575568` → `ContentDir` |
| `BandSongMgr::ContentPattern` | conditional | `"songs.dta"` | rb3-Wii dev body retail does not have |

★ **`StoreProfile` FINISHES laneSTORE-2 rather than revising it.** That lane
already proved retail has no such virtual and *parked it at the vtable tail* so
it "cannot perturb the dispatch offsets of slots 0..27". The reasoning is
correct and the placement was the right call at the time — but **a tail slot is
still a slot**, and the count instrument is what makes that visible. Same shape
as `CamShot`, where the call sites were removed and the declarations were not.
⇒ **"Neutralised" is not "absent". Grep for the leftover declaration whenever a
prior lane reports killing a method's uses.**

### 14c. ⛔⛔ I DE-VIRTUALIZED THE WRONG METHOD, AND THE ONLY THING THAT CAUGHT IT WAS THE MEASUREMENT

`GemTrackDir`'s surplus looked like `GameWon`: our last slot, our only new
virtual, and `BandTrack::GameWon` / `TrackPanelDir::GameWon` are both
non-virtual, so `virtual` here *hid* a base method rather than overriding it.
Everything fit. Measured: **−1 matched / −80 B**, isolated to a single row —
**`GameWon` itself, fuzzy 100.0 → 0.0.**

Two independent defects, and the first is the one worth carrying forward:

1. ⛔⛔ **The side-by-side tool I wrote for this wave was VACUOUS and agreed with
   me.** It looked the symbol map up with `f'{va:08x}'` while the map's keys
   carry a **`0x` prefix**, so *every* retail name resolved to `None` and every
   slot printed `--`. Nothing errored. The output still looked like a
   side-by-side — our column was real, the positions were real — and I read it
   as one. **A whole table resolving zero names is not a plausible state of the
   map**, which is exactly the anti-vacuity check that was missing; the tool now
   prints name coverage every run and shouts at zero. Same family as the
   `grep`-binary shim and `all([])`: *the instrument returned a decisive-looking
   answer by seeing nothing.*
2. With names resolving, retail slot 35 **is** `GameWon` (`0x822e6730`) — and
   ★ **our body for it already scored fuzzy 100.0 against that address**, which
   is what makes the map's spelling trustworthy *here* rather than assumed. So
   the **−80 B was the un-pairing mechanism**: dropping `virtual` re-mangles the
   symbol **`U` → `Q`** (public virtual → public), our obj stops defining the
   name the map assigns to `0x822e6730`, and objdiff un-pairs the row to 0%
   **permanently**.

⇒ ★★★ **A DE-VIRTUALIZATION IS A RENAME.** `U`/`M`/`I` vs `Q`/`A`/`I` is part of
the mangling, so any `virtual` removal on a **mapped** symbol silently un-pairs
its row unless the map is updated in the same patch. Check the map *before*
editing: of the six methods de-virtualized in this wave, exactly one
(`GameWon`) was mapped — which is why only one regressed, and why the other
five measured clean.

★ **The real surplus was one slot earlier, and `GemTrackDir` is what proves it,
not `TrackDir`.** `TrackDir`'s own tail cannot: from slot 16 on nearly every
retail slot is an **ICF fold hub** (`x1433`, `x1235`, `x195`) because its
virtuals are empty inline bodies, so the map shows one arbitrary survivor name
per hub and **every row reads as a mismatch**. `GemTrackDir` overrides with
**real** bodies, and there is **no retail slot anywhere holding a real
`GemTrackDir::SyncFingerFeedback`**: slots 33/34 are fold hubs (consistent with
the inherited empty `PreDraw`/`PostDraw` it does not override) and 35 is
`GameWon`. **A non-empty override can be neither absent nor folded**, so
`SyncFingerFeedback` is not virtual in retail. ⇒ *When a class's own tail is all
folds, adjudicate on a DERIVED class that overrides with real bodies.*

⚠ This also **supersedes without contradicting** TrackDir.h's "declared LAST to
match retail vtable slot order" note (itself already corrected twice, by
W13-CHARINFO and W16-HEADERTRUTH). Declaring it first *would* have pushed
`SetDisplayRange` one slot too high — the reasoning was sound; the slot it was
moved into does not exist either.

### 14d. ⛔ A 100% ROW IS NOT EVIDENCE THE MAP NAME IS RIGHT — the `ContentDir` pair

The map named **two different addresses** `ContentDir`, and **both scored fuzzy
100.000**, 12 B each. At most one can be ContentDir, so a 100% row was
provably misnamed. The mechanism: a 12-byte `lis / addi / blr` constant return
relocates against a string symbol that is a **placeholder name** in the target
obj, and `name_check` **forgives placeholder targets** — so such a body matches
any other *regardless of which string it returns*.

The string content is the only instrument that discriminates, and it needs no
map and no ICF reasoning:

| retail | slot | returns | ⇒ is |
|---|---|---|---|
| `0x82575558` | 10 | `0x8209DE38` = `"songs.dta"` | `ContentPattern` |
| `0x82575568` | 11 | `0x82000FE4` = `"songs"` | `ContentDir` |

Our `ContentDir` returns `"songs"` ⇒ slot 11 ⇒ **our declaration order was right
all along**; only the map disagreed. And slot 10's body has no load of
`TheArchive`, no compare and no branch, so our
`TheArchive ? "&songs*.dta" : "&songs*.dt?"` — **copied verbatim from the
rb3-Wii DEV oracle, which agrees with our old text exactly** — cannot compile to
it. Retail bytes outrank the oracle.

### 14e. What this measured

Four A/B runs, every leg settled and at a `symbols.txt` split fixed point:

| wave | Δmatched | Δbytes |
|---|---|---|
| `CamShot` ×4 slots | **0** | **0** (129 TUs recompiled) |
| the wrong `GameWon` edit | **−1** | **−80** ⇒ reverted |
| corrected: `SyncFingerFeedback` + BandSongMgr ×3 + StoreProfile | **0** | **0** (173 TUs) |
| `ContentPattern` map + source | **0** | **0** (re-split, `renamer_patched=1826`) |

**Δ0 is the expected and correct outcome for this defect class** — `mpn` is
arg-blind and a vtable is *data*, not a scored function row — so it is the
**safety check**, not the payoff. The `ContentPattern` run was pre-registered as
Δ0 and confirmed row-by-row: `ContentDir@Callback@ContentMgr` (12 B @ 100) is
replaced by `ContentPattern@BandSongMgr` (12 B @ 100), 24 B conserved. Landed on
accuracy, per the standing directive that a metric which hides real bugs is
worse than a lower metric.

### 14f. Deliberately NOT done

- **`BandStorePanel` slot 18 / `MakeNewOffer`** — evidence is strong but the fix
  is not adjudicated. Retail's `StorePanel` slots 16–19 are all `_purecall`
  (`0x828299b8`, x143), so retail declares `MakeNewOffer` **pure virtual**, and
  the map names retail's `BandStorePanel` slot 18 `MakeNewOffer(const
  StorePackedOfferBase *, bool)` — consistent with the bytes, since the body
  uses **r4 only** and reloads r5 from a global (`lwz r5,0x2ba8(r11)`), i.e. an
  **unused** `bool`. Our base spells `(DataArray *)`, so the override becomes a
  NEW slot instead of filling 18. **But changing the base signature requires
  rewriting `StorePanel::PopulateOffers`'s call at StorePanel.cpp:398, and what
  retail's `PopulateOffers` does there is not established.** That is the
  `CacheXbox` situation exactly — left specified, not guessed. `StorePanel`'s
  own table is already **28/28 exact**.
- **`StreakMeter`, `AppLabel`, `GameMicManager`, `ModifierMgr`, `DxShaderMgr`**
  — counts EQUAL, so the count instrument says nothing; their disagreements are
  on **secondary/adjustor-thunk tables** (`$4PPPPPPPM@…`) where retail and our
  spellings differ in owner *and* displacement. That is the thunk-twin/fold
  noise class, not a declaration-order bug. ⚠ Wave 7's prioritiser pointed here
  (`SetType@StreakMeter` at 0.000%) and it did **not** pay this wave — the
  0.000% rows are not the same population as the sweep's charged slots.
- **The three `*TrackWatcherImpl`** — still the ICF-fold wall.
- **`CacheXbox`** — unchanged and still the only `PERMUTED`; see §13e.

---

## 15. Wave 9 (2026-08-21) — the vein wave 8's instrument opened, and a rename has THREE consumers

Wave 8 established that `retail_slots` vs `our_slots` is the instrument. Wave 9
**ranked the whole sweep on it** instead of only reading it inside `SET_DIFFER`,
and that is where the rest of the defects were.

### 15a. ★★★★ THE VERDICT COLUMN HIDES THIS DEFECT CLASS BY CONSTRUCTION

`SET_DIFFER`/`PERMUTED`/`SAME` describe the **order of the covered slots**. They
say nothing about **length**. So a class can have surplus slots and still be
reported `SAME` — and seven do:

| | tables |
|---|---|
| counts differ, raw | 292 |
| …of which an our-side table was actually joined (`our_slots > 0`) | **23** |
| ⇒ reported `SAME` | **7** |
| ⇒ reported `UNRESOLVED` | 15 |
| ⇒ reported `SET_DIFFER` | 1 |

⚠ **The 269 with `our_slots == 0` are NOISE, not a backlog** — no our-side table
was joined, so there is no count claim to make. Filter them before ranking or
the vein looks 13× bigger than it is.

### 15b. `MoggClip` — 21 retail vs **29** ours, the largest in the tree, reported `SAME`

Retail's `Hmx::Object`-subobject table (`0x820f8f34`) holds exactly Object's 21
slots and **ends at `FindPathName`**, so retail's MoggClip introduces **no new
virtual at all**. Ours appended eight in declaration order — `IsPlaying`,
`Play(float)`, `Stop`, `Pause`, `DonePlaying`, `SetVolume`, `SetPan`, `SetSend`
— the header's `// Playable` block, which is **DC3's newer-engine
`PlayableSample`/`SynthPollable` MI refactor that retail's MoggClip predates**.

Three controls, all cheap, all necessary:
- name coverage on that table is **21/21** ⇒ not a coverage artifact;
- the word after slot 20 is `0xffffffff` then `(VA, index)` pairs — a handler
  table ⇒ not a truncated read;
- the **`SynthPollable` subobject table is 3/3 EXACT** ⇒ not a wrong-table join,
  and it is where `GetSoundDisplayName`/`SynthPoll` correctly live.

★ **Third instance this session of "NEUTRALISED IS NOT ABSENT."** An earlier
lane had already *proven* retail's `Play()` is non-virtual and no-arg at
`0x8270DE60` and added it alongside — but left the DC3 `virtual Play(float)`
declared, still burning a slot. Same for `SetPan(float)` beside retail's real
`SetPan(int, float)`. Both DC3 forms are left declared **non-virtual** rather
than deleted: *"not virtual" is what the vtable proves; "does not exist" is a
separate claim.*

### 15c. ⛔⛔ A RENAME HAS THREE CONSUMERS: SOURCE, THE MAP, **AND THE ICF ALIAS FILE**

Wave 8 learned that de-virtualizing re-mangles `U` → `Q` and un-pairs a **mapped**
row. Wave 9 found the second consumer the same way — by measuring a regression
it did not predict: **−3 matched / −1232 B**, in `default/CrowdAudio`, a unit the
patch never touched.

The three rows (`Poll`, `SetBank`, `SetPaused`) all call
`p->MoggClip::Stop()` **qualified**, which suppresses virtual dispatch — so the
instruction was a direct `bl` before *and* after, and only the relocation's
**name** moved. `symbol_aliases.json` **group[1398]** (survivor
`?Stop@BinkClip@@QAAXXZ` at `0x8270d940`, folding `?Stop@MoggClip@@UAAXXZ`) was
forgiving it. The rename dropped our spelling out of the group, so **a REAL ICF
fold began reading as a wrong-callee defect.**

Respelling the group member `U` → `Q` restored all three to 100.0 with one edit.
★ **The group's CLAIM is untouched** — retail `0x8270d940` is still one body
shared by both spellings; only *our* side is respelled. This is the
`STALE_SPELLING` class CLAUDE.md warns must never be pruned, arriving from the
other direction: *a spelling made stale by our own source fix.*

⇒ **Before removing `virtual`, grep all three: the source, `target_symbol_map.json`,
and `symbol_aliases.json`.** In this wave that was 2 map rows (`Pause`
`0x8270D690`, `SetVolume` `0x8270D748` — both mapped with the *virtual* mangling
and both scoring 100%, though neither address is in **either** MoggClip vtable)
and 1 alias group.

### 15d. ⛔ AND MY ALIAS-FILE SEARCH WAS VACUOUS — THE SECOND SELF-INFLICTED VACUITY IN TWO WAVES

`scripts/symbol_aliases.json` is a **dict** `{_comment, groups}`, not a list.
`json.load(...)` then iterating yields the two **key strings**, so
`g.get('folded')` never runs and the scan reports **0 hits** — which I believed,
and which sent me hunting a nonexistent mechanism for three rounds. A raw
`grep -c "Stop@MoggClip"` returned **1** and settled it in one second.

Same shape as wave 8's `0x`-prefix bug and the `grep`-binary shim: **the search
found nothing, and "nothing" was indistinguishable from a real negative.**
⇒ *When a structured search over a file returns zero, cross-check with a dumb
text grep before believing it.* One line, and it would have saved both rounds.

### 15e. `BandUI` — 13 → 12, and an in-tree note NARROWED

Retail's BandUI primary table holds 12 and ends at `IsTimelineResetAllowed`;
`SendTransitionComplete` was our sole new virtual. BandUI.cpp's body carries a
note: *"Retail calls `UIManager::SendTransitionComplete(s1, s2)` here, but the
rb3-xenon DC3-derived UIManager omits that virtual … Introducing it would
perturb UI.cpp."*

The **observation stands**; the **inference does not.** `UIManager`'s own tables
measure **21/21 and 12/12 EXACT**, so our UIManager omits no slot at all —
retail's `UIManager::SendTransitionComplete` is simply *not virtual either*,
which is why there is no base slot to override. ★ The note's proposed-but-
declined fix was therefore **the wrong one**: adding the virtual to UIManager
would have made it 13 against retail's 12. The lane declined it for an unrelated
reason and thereby avoided introducing a defect.

### 15f. Measured

| change | Δmatched | Δbytes |
|---|---|---|
| MoggClip ×8 slots, source only | **−3** | **−1232** ⇒ diagnosed, not reverted |
| + map (×2) + alias respell (×1) | **0** | **0** |
| BandUI ×1 slot | **0** | **0** (22 TUs) |

Final `42204 / 3,764,256 B / 36.738945%`. Δ0 remains the expected outcome for
this class and is the **safety check**, not the payoff.

### 15g. Deliberately NOT done — three UNDERDETERMINED, recorded with what would settle them

- **`StoreOffer` 22→23 / `BandStoreOffer`** — our surplus slot 22 is
  `Cmp(StoreOffer const &, Symbol) const = 0`, and retail's table ends at
  `IsCompletelyUnavailable` (21). **But `SortCmp::operator()`
  (StoreOffer.h:118) calls `offer1->Cmp(*offer2, …)` through a `StoreOffer *`,
  which REQUIRES virtual dispatch** — so retail must sort by some other
  mechanism, and what that is has not been established. Settle retail's sort
  path first.
- **`BandCharacter` 20→21** — our surplus is
  `?Replace@BandCharacter@@UAAXPAVObject@Hmx@@0@Z`, i.e. `(Object*, Object*)`,
  where `Hmx::Object::Replace` takes `(ObjRef*, Object*)`. This is the
  **wrong-base-signature-creates-a-new-slot** shape, the same as
  `BandStorePanel::MakeNewOffer` (§14f) — not a plain surplus. Adjudicate the
  intended signature before editing.
- **`BandUser` 10→11 / `LocalBandUser` / `NullLocalBandUser`** — slots 5–10 are
  `_purecall` on **both** sides, so names cannot localise which of our pure
  virtuals is the extra. Use §14c's move: adjudicate on a derived class that
  overrides with **real** bodies.
- Also untouched: `XboxContent` (**15 retail vs 14 ours** — we are *missing* a
  virtual, the opposite direction and a different fix) and `RndFur` (23 vs 21).
