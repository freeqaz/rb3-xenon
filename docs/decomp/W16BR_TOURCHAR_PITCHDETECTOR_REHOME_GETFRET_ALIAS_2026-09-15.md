# W16-BR — TourChar/TourCharRemote re-home, the dsp cluster, and the GetFret alias

**Lane:** W16-BR · **Date:** 2026-09-15 · **Branch:** `w16-br` (off `main` `0a1f3063f833`)
**Worktree:** `~/tmp/wt-w16-br` (all work; main never touched)

---

## 0. Baseline verification — the brief's figures, tested literally

Every §0 figure in the brief was re-measured on this worktree before anything was
edited, because the standing rule is that a briefed number is a claim, not a fact.

| key | brief | measured | verdict |
|---|---:|---:|---|
| `matched_functions` | 43,550 | **43,550** | ✅ |
| `matched_code` | 4,041,980 | **4,041,980** | ✅ |
| `matched_code_percent` | 39.445232 | **39.445232** | ✅ |
| `total_functions` | 69,240 | **69,240** | ✅ |
| `total_code` | 10,247,068 | **10,247,068** | ✅ |
| `fuzzy_match_percent` | 49.70534 | **49.70534** | ✅ |
| `masked_equal_functions` | 23,054 | **23,054** | ✅ |

Rowset identical to `~/tmp/rows_w16bp_main.json` (40,915 rows, 0 diff). Renamer
asserted live: 81,078 mangled names across 3,107 target objs — the check CLAUDE.md
requires before trusting any name-keyed negative in a worktree.

⚠ **One near-miss worth recording, because it nearly produced a false "the brief is
wrong".** My first map lookups all returned `None`, which looked like the brief's map
names being fabricated. The defect was mine: `target_symbol_map.json` keys are
`'0x%08x'` **with** the `0x` prefix, and I had built `'%08x'`. After the fix every
single map claim in the brief verified. A lookup that silently returns nothing is
shaped exactly like a decisive negative — the same family as the `grep`-binary trap.

**Brief figures that did NOT survive** are in §7 (the seam) and §3 (the `??_G` name).

---

## 1. Result

| key | baseline | final | Δ |
|---|---:|---:|---:|
| `matched_functions` | 43,550 | **43,571** | **+21** |
| `matched_code` | 4,041,980 | **4,045,036** | **+3,056** |
| `matched_code_percent` | 39.445232 | **39.475056** | **+0.029824** |
| `fuzzy_match_percent` | 49.70534 | **49.74163** | +0.036290 |
| `masked_equal_functions` | 23,054 | 23,056 | +2 |
| `total_functions` | 69,240 | **69,240** | **0** |
| `total_code` | 10,247,068 | **10,247,068** | **0** |

**`total_functions` and `total_code` are Δ0 across the entire lane.** No pin in this
lane moved the denominator, so every gain is real matching or honest reattribution —
there is no denominator effect to discount.

Honest floor: +21 matched − 2 masked = **+19 honest**.

### Units at 100%

| unit | before | after |
|---|---|---|
| `default/band3/tour/TourChar` | 22/26 fns, 1,804/2,104 B | **26/26, 2,104/2,104 — COMPLETE** |
| `default/band3/tour/TourCharRemote` | (did not exist) | **12/12, 1,128/1,128 — COMPLETE** |

---

## 2. Per-commit ledger — predicted vs measured

Every edit was pre-registered with a band before building. Two predictions failed;
both are recorded, because a failed prediction is the most informative line here.

| # | commit | change | predicted | **measured** | verdict |
|---|---|---|---|---|---|
| 1 | `ec55207f` | TourChar re-home + 2 map names + `??_E` alias | +4 fns / [+260,+300] B | **+4 / +300** | exact |
| 2 | `ce5080db` | `HxGuid::Chunk32` definition, then the GetFret alias | Δ0, then +1 / +784 | **Δ0, then +1 / +784** | exact |
| 3 | `4ee7a8c0` | TourCharRemote port + pin + 9 names + alias | +8..+11 fns / +900..+1,200 B | **+10 / +1,088** | in band |
| 4 | `f04a6939` | IIRFilter port/pin + VibratoDetector re-bound + 3 names | +1..+2 / +124..+216 | **+2 / +216** | in band |
| 5a | `1a21aa10` | PitchDetector port + pin + 4 names | +3..+4 / +628 | **+2 / +352** | ❌ **MISSED** |
| 5b | `1a21aa10` | `IIR4PoleFilter` size 0x60 → 0xE0 | +1 / +276 | **+1 / +276** | exact |
| 6 | `7b0db588` | `Time2IirA` helper | (exploratory) | **Δ0** | inert |
| | | **lane total** | | **+21 / +3,056** | |

**Why 5a missed:** I predicted `??0PitchDetector` would reach 100 because 27 of its
28 differing words were relocations. The 28th was `li r3,0x60` vs retail `li r3,0xE0`
— the `operator new` size for `IIR4PoleFilter`. It landed at 99.985504, one
instruction short, and the 276 B I had banked arrived only after 5b. The lesson is
narrow and reusable: **a lone non-relocation diff in an otherwise relocation-only
body is not noise — it is the whole remaining distance**, and it should be read
before predicting, not after.

**Second failed prediction (commit 3):** I expected
`?Handle@TourCharRemote@@$4PPPPPPPM@A@…` to MISS, because `symbols.txt` gave retail's
extent as `0x10` against our 12 B COMDAT. The re-split re-carved it to 12 B and it
matched. ⇒ **A size mismatch read off `symbols.txt` is not a verdict until the
affected range has been re-split.**

---

## 3. The wrong-name adjudications (LocalePanel, ChooseProfilePanel)

Two map rows named `??_G` bodies for classes that are not theirs. Both were settled on
evidence, and in both cases the deciding argument is the same and is *structural*:

> **A `??_G` calls its own class's `??1`. MSVC `/OPT:ICF` folds only bodies identical
> INCLUDING relocations. Therefore two classes' `??_G` bodies can never fold**, and a
> `??_G` address belongs to exactly one class.

### 3.1 `0x82B801D8` — mapped `??_GLocalePanel@@UAAPAXI@Z`, actually `??_GTourChar`

| leg | evidence |
|---|---|
| vtable | referenced by **no** vtable; its only data-word reference in the image is its own `.pdata` BeginAddress |
| code refs | only two `b`, from TourChar's own map-named `??_E` adjustor thunks |
| objdiff | the single charged site is retail `bl ??1TourChar` vs our `bl ??1LocalePanel` |
| bytes | our `??_GTourChar` COMDAT (84 B) matches at all 21 words but its 3 relocation sites |

### 3.2 `0x82B80768` — mapped `??_GChooseProfilePanel@@UAAPAXI@Z`, actually `??_GTourCharRemote`

| leg | evidence |
|---|---|
| vtable | referenced by **no** vtable; only data-word reference in the whole image is its own `.pdata` BeginAddress at `0x8225C638` |
| code refs | exactly two `b`, from `0x82B80664` and `0x82B80674` — TourCharRemote's `??_E` thunks |
| bytes | our `??_GTourCharRemote` (84 B) matches at all 21 words except 3 relocation sites, whose targets are `??1TourCharRemote` / `??1Object@Hmx@@` / `??3@YAXPAX@Z` |
| blast radius | it was the map's **only** ChooseProfilePanel row, and the class is neither compiled nor pinned — nothing depended on the name |

### 3.3 Rename DIRECTION — the trap that was avoided

Both addresses could be spelled `??_G` or `??_E`. **`??_G` is the right choice and
`??_E` would have been permanently destructive.** `??_G` is the DEFINED COMDAT
objdiff can pair (+84 B each); `??_E` has **no section** (it is a weak external, see
§4), so renaming the target row to it would strand the row at 0% forever — the
W16-BN §7 trap. The alias then captures the `??_E` spelling anyway, so choosing `??_G`
costs nothing and gains the pairing.

---

## 4. The two `??_E` → `??_G` alias groups

Both installed on **two independent instruments that agree**, which is stronger than
the usual T1 byte argument alone.

**Instrument A — compiler-declared.** In our compiled objs,
`??_ETourChar@@UAAPAXI@Z` and `??_ETourCharRemote@@UAAPAXI@Z` are each
`IMAGE_SYM_CLASS_WEAK_EXTERNAL` (105), SectionNumber 0, with an aux record whose
TagIndex resolves to the corresponding `??_G` (Characteristics=2). **MSVC itself
states the two spellings denote one body**, which is strictly stronger than inferring
a fold from byte identity.

**Instrument B — retail bytes, independent of the COFF record.** Retail's own adjustor
thunks branch into the `??_G` body:

| thunk | word | decode | destination |
|---|---|---|---|
| `0x82B80660` | `0x48000104` | `b +0x104` from `0x82B80664` | **`0x82B80768`** |
| `0x82B80668` | `0x480000F4` | `b +0xF4` from `0x82B80674` | **`0x82B80768`** |

In our object those same two thunks relocate against `??_ETourCharRemote@@UAAPAXI@Z`.
So the spelling our thunks name and the address retail's thunks reach are one body.

⚠ This matters because of the standing hazard that an **unproven alias lifts
`name_check` by construction**, and the `none` control is structurally incapable of
catching a fabricated one. Neither group rests on the metric.

---

## 5. The GetFret / Chunk32 alias — T1 record

`HxGuid::Chunk32` was **declared** in `src/system/utl/HxGuid.h:20` and **defined
nowhere**. Defining it (`return mData[i];`) measured **Δ0 on its own** — clean
attribution, so the subsequent +784 B is unambiguously the alias.

Full 7-leg record, as installed in `scripts/symbol_aliases.json` @ `0x8277b3f0`:

- **Charged-site check FIRST.** Retail `0x8277B3F0` is 12 B:
  `548B103A 7C6B182E 4E800020` (`slwi r11,r4,2; lwzx r3,r11,r3; blr`), **no `.pdata`
  BeginAddress**. Our `?Chunk32@HxGuid@@QBAHH@Z` COMDAT is byte-identical.
- **Relocation-free**, so byte identity is the *entire* proof — the usual T1 vacuity
  caveat (masked relocation targets) does not apply, and the body is 3 real
  instructions, not an empty `blr`.
- **Exhaustion.** Exactly ONE function in the whole retail `.text` has this body, so
  an EH-free 12 B getter COMDAT can fold only here.
- **Call-site role from retail's own instruction stream, not our spelling.** The four
  charged sites in `?AddCustomSettings@BandMatchmaker@@` (`0x826524B0`) are preceded
  by `addi r4,r0,{0,1,2,3}` and `addi r3,r31,0x70` — an index-by-constant getter over
  a 4-word object, i.e. `guid.Chunk32(0..3)`.
- **Blast radius.** The folded spelling is the target of exactly 4 relocations
  tree-wide, all in `Matchmaker.obj`, inside that one row.
- **FT1.** `Chunk32` has no row in `target_symbol_map.json`.
- **Safety.** Both bodies are a pure read of the i-th word with no calls, so the alias
  cannot conceal a behavioural divergence.
- **Noted, not hidden:** return types differ (retail `QBAIH@Z` unsigned vs ours
  `QBAHH@Z` int). The emitted `lwzx` is identical for a full word either way.

---

## 6. Per-function census of the re-homed bands

### 6.1 Block 1 — `0x82B801D8`–`0x82B80320` → `band3/tour/TourChar.cpp`

Merged into TourChar's existing heading; UI.cpp lost the `.text` line (and, derived,
its `.pdata`). Unit went 22/26 → **26/26, COMPLETE**.

### 6.2 Block 2 head — `0x82B8032C`–`0x82B807C0` → `band3/tour/TourCharRemote.cpp` (NEW)

All twelve rows at `fuzzy == 100`. Names assigned by byte comparison against retail,
never by position.

| addr | size | disposition | evidence |
|---|---:|---|---|
| `0x82B80330` | 116 | `?SyncLoad@TourCharRemote@@…` | 2 diffs, both relocs, name-equal |
| `0x82B803A8` | 172 | `?GetTexAtPatchIndex@…` | 9 diffs; 8 reloc + **1 RAW** (see §8) |
| `0x82B80460` | 192 | `?Handle@TourCharRemote@@…` | 7 diffs, all reloc (one is `?Handle@TourChar@@`, named in commit 1) |
| `0x82B80520` | 40 | EH funclet (masked) | byte-signature pairing; **moved out of UI.cpp** |
| `0x82B80550` | 204 | `??0TourCharRemote@@QAA@XZ` | 16 diffs, all reloc |
| `0x82B8061C` | 68 | EH funclet (masked) | byte-signature pairing |
| `0x82B80660` | 8 | `??_ETourCharRemote@@WBI@…` | 1 reloc diff → `??_E`, alias-forgiven |
| `0x82B80668` | 16 | `??_ETourCharRemote@@$4…FM@…` | 1 reloc diff → `??_E`, alias-forgiven |
| `0x82B80678` | 12 | `?Handle@TourCharRemote@@$4…A@…` | 1 reloc diff; re-carved 16→12 B by the split |
| `0x82B80690` | 176 | `??1TourCharRemote@@UAA@XZ` | 10 diffs, all reloc |
| `0x82B80740` | 40 | EH funclet (masked) | byte-signature pairing |
| `0x82B80768` | 84 | `??_GTourCharRemote@@UAAPAXI@Z` | §3.2; 3 reloc diffs |

⚠ `??1` vs `??_D` at `0x82B80690` was **proved, not assumed**: `??1TourCharRemote`
is 176 B with all 10 diffs at relocation sites; `??_DTourCharRemote` is 52 B and does
not fit there at all.

### 6.3 Block 2 tail + gap — the three-TU split, derived from call topology

| addr | size | identification | how |
|---|---:|---|---|
| `0x82B807C0` | 72 | `??1PitchDetector` | 3×`MemFree` + `operator delete` on `*(void**)this` |
| `0x82B80810` | 1780 | `?AnalyzeBlock@PitchDetector@@` | `Symbol` ctor ×6, `DataVariable` ×6, `memcpy`, `FilterSlow` ×2, `ShiftedDotProduct`, `FindCCPeak`, `RefinePeriod2`, `Time2IirA`, `log10` |
| `0x82B80F04`..`0x82B80FA4` | 6×32 | the six `DataVariable` static initialisers | count and size match AnalyzeBlock's six statics exactly |
| `0x82B80FC8` | 280 | `?SetSampleRate@PitchDetector@@` | `MemFree` ×3, `MemAlloc` ×3, `memset` ×3 |
| `0x82B810E8` | 276 | `??0PitchDetector@@QAA@H@Z` | calls **both** `fn_82B80FC8` (SetSampleRate) **and** `fn_82B815B8` |
| `0x82B811FC` | 40 | EH funclet (masked) | byte-signature pairing |
| `0x82B81228` | 92 | `??0VibratoDetector@@QAA@HH@Z` | 2 diffs, both reloc |
| `0x82B81288` | 376 | `?Detect@VibratoDetector@@` | already map-named; now **paired**, 20.86% |
| `0x82B81400` | 308 | `?Analyze@VibratoDetector@@` | 100% |
| `0x82B81538` | 124 | `?FilterSlow@IIR4PoleFilter@@` | **ZERO differing words** |
| `0x82B815B8` | 312 | `??0IIR4PoleFilter@@QAA@PAM0@Z` | ours 232 B — 80 B short (§9) |

**The boundary argument.** `??0PitchDetector` calls `fn_82B815B8`, and the source does
`new IIR4PoleFilter(b, a)` — that identifies `fn_82B815B8` as IIR4PoleFilter's ctor.
`0x82B815B8 + 312 = 0x82B816F0`, **exactly** the pre-existing SndAnalysis boundary. So:

```
system/dsp/PitchDetector.cpp    0x82B807C0 - 0x82B81228
system/dsp/VibratoDetector.cpp  0x82B81228 - 0x82B81538   (was 0x82B81400-0x82B816F0)
system/dsp/IIRFilter.cpp        0x82B81538 - 0x82B816F0
```

`IIR4PoleFilter::Begin()`/`End()` are empty and ICF-folded away, which is why the
IIRFilter block holds two bodies and not four.

**The old VibratoDetector pin was wrong at BOTH ends** — it excluded the class's own
ctor and `Detect` (stranded in an `auto_*` unit) while swallowing both IIRFilter
bodies, which it could never define. Re-bounding measured **Δ0** on the whole binary,
correctly: every row that moved was at 0% on both sides. What it bought is
**pairability** — `?Detect@VibratoDetector@@` now scores 20.86% inside its own unit
where it was previously invisible to adjudication at a structural 0. Per this
project's own doctrine that is a *correctness* instrument, not a scoring one.

---

## 7. The seam — the brief's `0x82B80810` is wrong; it is `0x82B807C0`

The brief placed the TourCharRemote/PitchDetector boundary at `0x82B80810`. Measured,
it is **`0x82B807C0`**: `fn_82B807C0` (72 B) frees `this+0x20/0x24/0x28` and then
calls `operator delete` on `*(void**)this`, which is `PitchDetector::~PitchDetector()`
against the Wii `PitchDetector.h` offsets (`mDecimBuf` 0x20, `mCorrBuf` 0x24,
`mPeakBuf` 0x28, `mFilter` 0x00). It is the next TU, not TourCharRemote's.

**dtk corroborated this independently** when it derived `.pdata` for the new unit:
`??_G`'s own `.pdata` entry is at `0x8225C638`, and the derived block ends at
`0x8225C640` = `0x8225C638 + 8` — exactly. `fn_82B807C0`'s entry is the first one left
to UI.cpp. Had the seam been the brief's `0x82B80810`, the derived `.pdata` would have
had to include it.

---

## 8. Source defects exposed (the real payout)

Three, all found by byte comparison against retail rather than by the metric.

1. **`GetTexAtPatchIndex` comparison operand order.** Retail word 30 is `0x7F07F000`
   (`cmpw cr6,r7,r30`); the Wii spelling `i == unk4c[n].index` emits `0x7F1E3800`
   (`cmpw cr6,r30,r7`). Writing the member first reproduces retail. This was the
   **only** non-relocation difference in the entire TourCharRemote port.

2. **`MemAlloc` alignment silently dropped.** `utl/MemMgr.h` defines
   `#define MemAlloc(size, file, line, name, ...) (MemAlloc)((size), 0)`, and MSVC's
   permissive preprocessor expands it **even for a 2-arg call**. So
   `MemAlloc(mFrameSize * 4, 0x10)` compiled to `li r4,0` at all three SetSampleRate
   sites where retail has `li r4,0x10`. Fixed with the parenthesized
   `(MemAlloc)(size, align)` bypass the header itself prescribes. ⚠ This is a
   *silent, compiling, plausible-looking* wrong call — worth knowing about at every
   other `MemAlloc` site that passes a real alignment.

3. **The oracle's allocator spelling is wrong for X360.** The Wii file calls
   `_MemAlloc`/`_MemFree`; those are MWCC phantoms here (zero occurrences across 396
   pinned target objs per `utl/MemMgr.h`), and retail's own bodies call
   `?MemAlloc@@YAPAXHH@Z` / `?MemFree@@YAXPAX@Z`.

---

## 9. PitchDetector / IIR4PoleFilter layout reconciliation

**`PitchDetector` — no reconciliation needed.** Our `src/system/dsp/PitchDetector.h`
and the Wii header are **layout-identical**, offset for offset. Only three field
*names* differed (`unk2C`/`unk30_period`/`unk34` vs `mPitch`/`mPeriod`/`mAveEnergy`).
Adopted the Wii names, which are backed by its Bank 5 DWARF. Safe: the only in-tree
dependent, `src/band3/game/GameMic.h`, holds a `PitchDetector*` and touches no field.
(`src/system/synth_xbox/PitchDetector.h` is a *different* class with its own header —
no collision.)

**`IIR4PoleFilter` — a real correction, measured from retail.** The object is
**0xE0 (224) bytes, not the 0x60 (96)** the Wii header describes.

- Evidence: `??0PitchDetector`'s `new IIR4PoleFilter(b, a)` emits `li r3,0xE0`; our
  0x60 layout emitted `li r3,0x60`. That was the only non-relocation diff in the body.
- **The first 0x60 is confirmed CORRECT and was not disturbed:**
  `?FilterSlow@IIR4PoleFilter@@QAAMM@Z` is byte-identical to retail (124 B, **zero**
  differing words) and reads `mB0`/`mGain`/`mNegA`/`mAccum`, so those offsets are
  pinned by matching code. It still reads 100.0 after the size change — that is the
  control on the edit.
- **What occupies `0x60..0xE0` is NOT identified.** Only the *size* is established by
  retail, so only the size is asserted, as an explicitly-named unknown tail. It is
  128 B = 8×16 (the shape of VMX128 scratch) and the class exposes `FilterSlow`, whose
  name implies a fast path this tail probably serves. Recorded as follow-up, **not**
  as a solved layout.

---

## 10. `Time2IirA` — resolved

`?Time2IirA@?A0xa7b3dd7d@@YAMMM@Z` @ `0x82B6EA08`. The brief noted the Wii file does
not call it. **It is called:** `PitchDetector::AnalyzeBlock` calls it at
`0x82B80D54`, and AnalyzeBlock contains **no `exp` call at all** (its only libm call is
`log10` at `0x82B80E90`). So retail routes `1.0f - exp(-1.0f / (t * rate))` through an
anonymous-namespace helper where the oracle writes the `exp` inline.

`0x82B6EA08` lies far outside this TU's `.text`, so it is an **ICF fold survivor**:
the helper is defined in several TUs and the surviving copy's name belongs to whichever
TU owns that address — note the hash `?A0xa7b3dd7d` is therefore **not**
PitchDetector.cpp's own.

Writing the helper the obvious way measures **Δ0**, with AnalyzeBlock byte-unchanged at
fuzzy 85.723595, because `/O1 /Ob2` inlines it straight back. Kept anyway, with the
inertness stated in the source, so the next lane does not re-derive the call site.
**Not resolved:** why retail does not inline it. ⚠ That should not be attacked by
reaching for `__declspec(noinline)` to chase the metric; the useful question is whether
the retail source reaches a definition in another TU through a shared declaration,
which the foreign anon-namespace hash already hints at.

---

## 11. Exact edits

**`config/45410914/splits.txt`** — TourChar `.text` merged to `0x82B7FAA8–0x82B8032C`;
new `band3/tour/TourCharRemote.cpp:` `0x82B8032C–0x82B807C0`; new
`system/dsp/PitchDetector.cpp:` `0x82B807C0–0x82B81228`; `system/dsp/VibratoDetector.cpp`
re-bound to `0x82B81228–0x82B81538`; new `system/dsp/IIRFilter.cpp:`
`0x82B81538–0x82B816F0`; UI.cpp's two band `.text` lines deleted (heading kept — it
retains 19 other blocks); the bare `PitchDetector.cpp:` heading path-qualified to
`system/synth_xbox/PitchDetector.cpp:`. All `.pdata` re-derived by dtk, never hand-written.

**`config/45410914/objects.json`** — added `band3/tour/TourCharRemote.cpp`,
`system/dsp/IIRFilter.cpp`, `system/dsp/PitchDetector.cpp` (all `NonMatching`).

**`scripts/target_symbol_map.json`** — 29,479 → **29,495** entries. 2 corrected
(`??_GLocalePanel`→`??_GTourChar`, `??_GChooseProfilePanel`→`??_GTourCharRemote`),
14 added.

**`scripts/symbol_aliases.json`** — 1,652 → **1,655** groups (`_GTourChar`,
`GetFret`, `_GTourCharRemote`).

**Source** — new: `src/band3/tour/TourCharRemote.cpp`, `src/system/dsp/IIRFilter.{h,cpp}`,
`src/system/dsp/PitchDetector.cpp`. Modified: `src/system/utl/HxGuid.cpp` (defined
`Chunk32`), `src/system/dsp/PitchDetector.h` (Wii field names),
`src/system/dsp/VibratoDetector.cpp` (stale banner corrected).

### A guard that fired and was right

Adding a second `PitchDetector.cpp` basename made the **pre-existing bare heading**
ambiguous, and `configure.py`'s unresolved-heading guard refused with
`AMBIGUOUS basename, 2 objects.json entries claim it`. This is the same defect class
as the doubled headings `b341d7ab` fixed, caught *before* anything could silently read
0%. Note the guard validates against dtk's generated `build/45410914/config.json`, so
it reports the **previous** split's unit list; the escape hatch was used once to
regenerate, then `configure.py` was re-run **without** it and passes clean — which is
what establishes the state is actually correct rather than merely bypassed.
`verify_objs_patched.py --check` confirms **0 objects declared by >1 unit**.

---

## 12. Instrument defects found in my own tooling

- **Map key format.** `'%08x'` vs the real `'0x%08x'` — see §0. Caught only by testing
  a claim I expected to fail.
- **`provenance.diff_config` is a LIST** of `"k=v"` strings, not a dict.
- **My rowset snapshot looked like a full census and was a `fuzzy==100` filter**
  (40,920 of 69,240 rows; 909 of 3,107 units). Its first set-diff reported
  "28,321 rows appeared", which is the only reason it was not believed. Replaced with
  a **dual-ruler** snapshot (`fuzzy==100` for bytes AND `mpn==100` for functions),
  because the single-ruler one could not attribute a function-count delta at all —
  see the unattributed row in §13.
- **A truncated (`head -40`) COFF listing** nearly produced a false "the brief is
  wrong" about `TourChar.obj` not defining `??_GTourChar`. Re-run untruncated, it does.

---

## 13. NOT done — explicitly

- **`?AnalyzeBlock@PitchDetector@@` — 85.72%, 1,780 B.** The largest single prize left
  in this band. Source work.
- **`??0IIR4PoleFilter@@QAA@PAM0@Z` — 10.19%, 312 B** (ours 232 B). It does not
  initialise the unidentified `0x60..0xE0` tail, which is most of the 80-byte
  shortfall. Blocked on §9's open layout question.
- **`?Detect@VibratoDetector@@` — 20.86%, 376 B.** Newly pairable, not yet worked.
- **`0x60..0xE0` of `IIR4PoleFilter` is unidentified.** Only the size is asserted.
- **Why retail does not inline `Time2IirA`** (§10).
- **Task 6 (SndAnalysis residue), NOT STARTED.** `ShiftedDotProduct` 356 B @ 25.0 was
  the named first target. Dropped for budget; the band is otherwise complete.
- **One function of the commit-3 delta is unattributed.** Bytes reconcile exactly
  (+1,128 crossed − 40 moved out of UI.cpp = +1,088, measured), but `matched_functions`
  moved +10 where the `fuzzy==100` row count moved +11. Exactly one row at fuzzy<100
  lost `mpn == 100` — the expected cost of naming previously-anonymous addresses, since
  `name_check` forgives placeholder targets and naming converts a forgiven site into a
  checked one. I could not name the row because the snapshot I held stored only the
  `fuzzy==100` set; the instrument is fixed (§12) but the specific row is unrecovered
  without a rebuild, and I judged one function not worth a full A/B.
- **`?SyncLoad@RemoteBandUser@@` (740 B, fuzzy 99.97298)** was checked as the likely
  candidate for that row and its relocation to `?SyncLoad@TourCharRemote@@` is
  **name-equal**, so it is probably not the one. Left open rather than asserted.

---

## 14. Gates

All run in the worktree, on a fully built tree, in the brief's order.

```
BUILD                                             rc=0
python3 scripts/verify_ruler_agreement.py --check  rc=0
    OK: both objdiff-cli entry points resolve the same ruler.
python3 scripts/verify_objs_patched.py --verify-manifest  rc=0
    [patch-state] OK: 1219 decomp, 3107 target objects match (tree_sha256=ef37d763ea03b684)
python3 tools/icf_alias_finder.py --validate       rc=0
    VALIDATE: PASS -- 1407 map-consistent, 247 tolerated, 0 contradicted, 1655 total
python3 tools/funclet_homing.py --validate         rc=0
    VALIDATE: PASS
```

⚠ `--verify-manifest` **failed first (rc=6)** on a committed-but-unbuilt comment edit,
naming `src/system/dsp/PitchDetector.cpp` exactly. Rebuilt, re-ran, green. The gate
did precisely its job — `report.json` had been describing source no longer in the tree.

### Native gate

```
tools/native_build_gate.sh                         rc=0
NATIVE GATE: PASS  (rc=0, 0 errors, 0 warnings, 18/18 target(s) verified)
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

`skipped=0`, so this is full coverage — not the `PASS (INCOMPLETE: ...)` form that
reads one space apart from a real pass. Re-run as the lane's final action after the
docs-only commit below, per CLAUDE.md's rule that a comment-only commit is not safe
to skip re-gating.

---

## 15. Branch

`w16-br`, 7 commits off `main` `0a1f3063f833`:

```
ec55207f  TourChar: re-home the UI.cpp hole, fix two map names, fold ??_E onto ??_G
ce5080db  HxGuid: define Chunk32, and fold it onto the GetFret ICF survivor
4ee7a8c0  TourCharRemote: port the unit, re-home Block 2's head, fold ??_E onto ??_G
f04a6939  dsp: port IIRFilter, re-bound VibratoDetector onto its real TU extent
1a21aa10  dsp: port PitchDetector, and correct IIR4PoleFilter's object size from retail
7b0db588  dsp: resolve Time2IirA -- retail calls it where the oracle inlines exp()
<this doc>
```
