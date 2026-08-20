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
- ★ **`RndFont` (+13) is the biggest remaining candidate and is VERIFIED as a
  real disagreement — deliberately deferred to its own lane.** Ours 34, retail
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
