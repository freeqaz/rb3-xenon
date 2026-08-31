# Vtable slot misattribution — the `withheld` list is real, sized, and **unrepairable by renaming** (lane SLOTMAP, 2026-08-31)

Tree: `05ff76aa`, worktree `~/tmp/wt-slotmap`, branch `slot-map`, freshly built
(the reflinked target objs are pre-renamer until the first build — every retail
mangled name would otherwise read "absent").

**Headline: 0 map rows repaired, deliberately.** The vein the brief pointed at is
real and larger than briefed (269 demonstrably-wrong thunk names, not 110), but
**not one of them can be repaired by renaming a single map row**: 218 (81%) are
pin-gated and 51 (19%) are rotation-locked. Two shipped instruments were found
defective and fixed. This is the "most of this vein is pin-gated and only N are
repairable" outcome the brief named as valid, with N = 0.

---

## 1. The brief's census reproduces — almost exactly

Re-derived from scratch (`tools/vtable_order_sweep.py --json`), not inherited:

| measure | brief | measured here |
|---|---|---|
| withheld total | 314 | **313** |
| `unrelated_owner` | 161 | **161** ✅ |
| `thunk_twin` | 111 | **110** |
| `nonvirtual_name` | 42 | **42** ✅ |
| `unrelated_owner` ∧ fan-in==1 | 110 rows / 80 classes | **110 rows / 80 classes** ✅ |
| thunks / plain bodies in that set | 92 / 18 | **92 / 18** ✅ |

Fan-in, re-measured with an image-wide aligned-word index (built once, then
cross-validated against `RetailRtti.word_refs` on the guard hub):

| population | median | >1 | max |
|---|---|---|---|
| control — 400 random slots from `SAME` vtables | **76** | 88% | **6520** |
| `unrelated_owner` | 1 | 32% | 2 |
| `thunk_twin` | 1 | **0%** | 1 |
| `nonvirtual_name` | 2 | 64% | 2 |

The brief's control median was 77 (different seed); `max = 6520` reproduces
exactly. **Anti-vacuity guard: `len(word_refs(0x823591e8)) = 2770`** (2,760
`.rdata` + 10 `.data`) — reproduces the coordinator's figure exactly. Memory's
"x1235" does **not** reproduce; do not inherit it. Every script in this lane
asserts the guard before doing anything, because the coordinator's first run of
this measurement was vacuous and was caught only by its control.

### 1a. One correction I made and then RETRACTED — record both

I first measured **88 thunks / 22 plain bodies** and reported the brief's 92/18
as a name-vs-bytes conflation. **That was my decoder, not the brief.** The four
disputed rows (`DxCam[8]`, `DxParticleSys[2]`, `PanelDir[0]`, `PanelDir[14]`)
*are* adjustor thunks in the **4-instruction** form
(`lwz; subf; addi rN,rN,-M; b`). With a generalized decoder the cross-tab is
perfectly diagonal — 92/18, name and bytes agreeing on every row. The brief was
right. **This became finding §4.1.**

---

## 2. ⛔ THE MAIN RESULT: the vein is pin-gated and rotation-locked, end to end

### 2.1 A stronger, flag-free detector finds 2.4× the population

The `unrelated_owner` label is not needed. For an adjustor thunk `T` with
image-wide fan-in 1, referenced by exactly one vtable whose `??_R4` COL names
class `C`, and whose **branch target** `B` is map-named a method of `C`:
*a thunk IS its branch target*, so `T`'s correct name is determined.

| stage | count |
|---|---|
| adjudicable thunks (fan-in 1, one vtable, body names a method of the owner) | **1,390** |
| map name already correct | 1,121 (80.6%) |
| **map name DEMONSTRABLY WRONG** | **269 (19.4%)** |

Of the 269: **36** have a class that differs from the RTTI owner (proven from
*retail bytes*), **233** have the right class and the wrong method (proven from
another map row, so weaker).

### 2.2 And **none** of them is safely repairable

The pairing rule is the one THUNK-105 got wrong and paid for: objdiff pairs
target↔base **per unit**, so the **base obj of the unit that owns that address**
must define the new spelling. Checked against `config/45410914/splits.txt`
(name-independent) joined to `objdiff.json`'s declared `base_path`, reading
**defined** COFF symbols (`secnum > 0`):

| outcome over the 269 | count |
|---|---|
| **PIN-GATED** — correct spelling is not defined in that unit's base obj at all | **218 (81%)** |
| **ROTATION-LOCKED** — it is defined, but already assigned to another address | **51 (19%)** |
| ambiguous (>1 candidate spelling) | 2 |
| **FREE RENAME — correct spelling exists and is unused** | **0** |

Renaming a pin-gated row drives it to a **permanent 0%** (the base obj can never
define that name). Renaming a rotation-locked row creates a **duplicate map key**
— the map is essentially injective today (only 2 names of 28,928 appear twice).

### 2.3 The rotation cannot be closed either

A rotation is safe if it is a **closed cycle**, or a chain terminating in a free
name, with every member in one unit. Measured:

- closed permutation cycles: **0**
- per-unit conflict-free batches: **0 of 32 units**
- global 49-row batch: **36 of 49 rows** want a spelling held **outside** the batch

Funnel over the whole binary, showing exactly where provability dies:

| stage | count |
|---|---|
| vtables with ≥2 adjustor thunks | 313 |
| …all thunks fan-in 1 **and** map-named | 98 |
| …**and** every thunk's **body** is map-named | **60** ← the binding constraint |
| …**and** ≥1 thunk mis-named | **53 (88%)** |
| …**and** the correction is a pure rotation of the same name multiset | **0** |

⇒ **The correct spellings live OUTSIDE each affected set.** The mis-naming is not
a local permutation, so no local repair can be injective. The chains terminate at
thunks whose **branch target is unnamed** — 44 of the lane's 92 thunks — which is
where the evidence runs out. Worked example, all four rows in `CrowdMeterIcon.cpp`
with byte-identical adjustments (`8163fffc 7c6b1850`, so *not* different-displacement
variants):

```
0x822bac80  map ?Save@CrowdMeterIcon@@$4…      -> body 0x822b91d0 ?PostLoad@CrowdMeterIcon@@UAA…
0x822bae88  map ?PostLoad@CrowdMeterIcon@@$4…  -> body 0x822b94c0 <UNNAMED>     <- chain dies here
```

`0x822bac80` is provably `PostLoad`'s thunk; the spelling it needs is held by
`0x822bae88`, which cannot be proven wrong because its own body has no name.

⇒ **Repairing this vein requires (a) a pin re-homing lane for the 218, and
(b) more body identifications for the 51 — not a naming pass.** Re-homing is not
metric-neutral (CLAUDE.md) and is explicitly a separate lane.

---

## 3. ⛔ TWO HYPOTHESES OF MINE, BOTH REFUTED BY THEIR OWN CONTROLS

Recorded because a narrated dead end stops the next lane re-hunting a drained vein.

### 3.1 "The map's thunk names are shifted by one along address order" — REFUTED

`RndLine` looked like a smoking gun: every thunk's body named the method it must
be, and the map's name was displaced by one position.

| addr | ref by | body | map says | must be |
|---|---|---|---|---|
| `0x8247b638` | `RndLine[0]` | `??_GRndLine@@` | `?SyncProperty@RndLine@@$4` | `??_GRndLine@@$4` |
| `0x8247b7e0` | `RndLine[7]` | `?SyncProperty@RndLine@@` | `?Copy@RndSpline@@$4` | `?SyncProperty@RndLine@@$4` |
| `0x8247b7f0` | `RndLine[8]` | `?Save@RndLine@@` | `?Load@RndLine@@$4` | `?Save@RndLine@@$4` |
| `0x8247c3e0` | `RndLine[10]` | `?Load@RndLine@@` | `?Load@RndSpline@@$4` | `?Load@RndLine@@$4` |

Tested over all 1,390 adjudicable thunks **against a random-permutation null**:

| hypothesis | rate |
|---|---|
| H1 map name already correct | 80.6% |
| H2 map name is the **next** thunk's correct name | **0.1%** |
| H2′ map name is the **previous** thunk's correct name | **0.1%** |
| **NULL** map name matches a **random** thunk's correct name | **0.1%** |

**H2 is indistinguishable from the null.** There is no global shift; four rows
were pattern-matching. ⇒ *This is why the null was run — the four-row table is
genuinely compelling and completely wrong.*

### 3.2 "The conflicts are multi-virtual-base adjustor variants" — REFUTED

When 36 of 49 wanted spellings turned out to be held by another address **in the
same unit**, the natural reading was that a class with several virtual bases has
several adjustor thunks per method, distinguished by the displacement token
(`$4PPPPPPPM@A@` vs `@BM@` vs `@3A`). Disassembling four conflict pairs killed it:
**all have identical adjustment bytes** `8163fffc 7c6b1850` — same displacement,
same 3-instruction form. They are genuine rotations, not variants.

### 3.3 "`name_owned_by`'s substring bug explains a large share" — REFUTED IN SCALE

The bug is real (§4.2) but is **10 of 161 rows**, not a large share. Measured
with a proper mangled-class extractor rather than asserted. **100 of the 110
target rows are genuinely a different class**, so the brief's framing holds.

---

## 4. Two shipped instruments were defective — both fixed in `b7745582`

### 4.1 `thunk_target_audit.decode_thunk` missed **28.2%** of all adjustor thunks

It handled only `lwz r11,-4(rN); subf rN,r11,rN; b target`. MSVC emits a fourth
instruction, `addi rN,rN,-M`, whenever the adjustment beyond the vtordisp is
nonzero. Measured over `scripts/target_symbol_map.json` on retail `band.exe`:

| decoder | thunks found |
|---|---|
| shipped (3-instruction only) | 1,553 |
| generalized | **2,164** |
| **missed** | **611 (28.2%)** |

The missed rows are exactly those whose mangled name carries a nonzero
displacement token (`$4PPPPPPPM@DM@`, `@CCE@`, `@BHI@`), so the census was biased
toward the zero-displacement primary-base stratum — a silent, one-directional
narrowing. **Every `--validate` figure in that tool's docstring was computed over
72% of the population**; re-measured and recorded there. The instrument survives
the correction: CONSISTENT 1,827/1,835 at `fuzzy==100` (99.6%) vs INCONSISTENT
4/130 (3.1%) — a **32× separation**, and the control could have failed. *Reach
changed; validity did not.* The `addi` is matched by **shape** (opcode 14,
`RT==RA==this` register), never a constant — a constant would rebuild the same
blind spot one displacement narrower. A missing `LK` check was also fixed (the
old decoder accepted a `bl` as a tail branch).

### 4.2 `icf_fold_safe.name_owned_by` could not see the `??<op>` family

It tested `('@' + cls + '@@') in sym`, which requires an `@` immediately before
the class name. True for `?Method@Class@@…`; **false** for `??_GBandScoreboard@@`,
which contains `_GBandScoreboard@@` and never `@BandScoreboard@@`. So a class's
own deleting-destructor thunk was labelled `unrelated_owner` **on its own vtable**.

**Measured effect, which contradicted my prediction.** I expected the 10 affected
rows to become charged mismatches. Instead **all 10 moved to `thunk_twin`** —
`mark_thunk_twins` only marks slots whose `reason is None`, so the wrong label had
been *masking* the right one — and exactly 1 further row (`RandomGroupSeq`) left
`withheld` as a charged agreement.

| sweep measure | before | after |
|---|---|---|
| withheld | 313 | **312** |
| `unrelated_owner` | 161 | **150** |
| `thunk_twin` | 110 | **120** |
| PERMUTED / SET_DIFFER / SAME / UNRESOLVED | 0 / 9 / 967 / 1241 | **unchanged** |

`thunk_twin` is the **stronger** diagnosis: two byte-identical thunks compete for
the slot, so the map's assignment between them is arbitrary and the row is *not
adjudicable by name at all*. ⇒ The corrected target population is **100**, not 110.

---

## 5. Pin evidence (`splits.txt`), for the re-homing lane

The resolver was validated on THUNK-105's documented known answer before use:
`0x82289748 → Line.cpp` while its body `0x822896e0 → BandCharacter.cpp`. It
reproduces exactly — **and that pin defect still exists at `05ff76aa`**.
6,588 `.text` ranges over 1,278 units parse with **0 overlaps**.

Of the 92 thunks in the target set, **44 have the thunk and its own body in
different pinned units**. On the stricter, decisive bar (four independent
evidence lines: RTTI owner + byte-decoded thunk + body named a method of the
owner + our compiled slot), **29 rows qualify and 24 are pin-gated**. In every
one of the 24, **the map name and the pin agree with each other and both
disagree with the vtable** — the map names the thunk after the class of the unit
it is pinned into. A representative contiguous run in `src/system/char/`:

| thunk | belongs to (RTTI + body) | pinned into | map names it |
|---|---|---|---|
| `0x82375d10` | `CharServoBone[4]` | `CharSleeve.cpp` | `?ClassName@CharSleeve@@$4…` |
| `0x823b40d0` | `CharIKFingers[4]` | `CharBlendBone.cpp` | `?ClassName@CharBlendBone@@$4…` |
| `0x823c6178` | `CharForeTwist[4]` | `CharIKMidi.cpp` | `?ClassName@CharIKMidi@@$4…` |
| `0x823c6e60` | `CharUpperTwist[4]` | `CharIKSliderMidi.cpp` | `?ClassName@CharIKSliderMidi@@$4…` |
| `0x823c9570` | `CharIKMidi[4]` | `CharMirror.cpp` | `?ClassName@CharMirror@@$4…` |
| `0x823cb0f8` | `CharIKSliderMidi[4]` | `CharNeckTwist.cpp` | `?ClassName@CharNeckTwist@@$4…` |
| `0x823cd9b0` | `CharMirror[4]` | `CharPollGroup.cpp` | `?ClassName@CharPollGroup@@$4…` |
| `0x823ce698` | `CharNeckTwist[4]` | `CharServoBone.cpp` | `?ClassName@CharServoBone@@$4…` |
| `0x823cf948` | `CharSleeve[4]` | `CharDriver.cpp` | `?ClassName@CharDriver@@$4…` |

Many of these sit in **12-byte `.text` blocks whose start IS the thunk address**
(`0x823c6178..0x823c6184`, `0x823cd9b0..0x823cd9bc`), i.e. a single-thunk block
attributed to the wrong unit — individually re-homable, which is the shape
CLAUDE.md's "59 rows / 2,972 B re-homable" class describes.

⚠ **Do not read the map name as corroborating the pin.** They agree because the
name appears to have been *derived from* the pin; the independent arbiters (RTTI
owner, fan-in-1 vtable membership, the thunk's own branch target) all side
against both.

---

## 6. What I deliberately did NOT do

- **No map edits.** Zero rows renamed. Every candidate failed the pairing test,
  the injectivity test, or both. Renaming any of them would lift `name_check`
  **by construction** — the ALIAS_SUSPECT metric-fitting shape — while making the
  map no truer.
- **No `splits.txt` edits.** Re-homing is not metric-neutral and is a separate
  lane; this lane produced the evidence for it (§5), not the change.
- **No source edits.** §12/§21 of `VTABLE_SLOT_COUNT_FIXES_2026-08-20.md` are
  right: this is a map-defect worklist, and every row here has our source on the
  correct side.
- **No A/B measurement.** With no map, splits or source change there is nothing to
  measure; `ab_measure` would correctly refuse as absent-vs-absent. Reporting a
  number here would be fabricating one. The two tool fixes are analysis-only —
  `tools/` is not a build input — so they are metric-inert by construction.
- **Did not adjudicate the 42 `nonvirtual_name` or 120 `thunk_twin` rows.**
  `thunk_twin` is *structurally* unadjudicable by name (byte-identical twins);
  `nonvirtual_name` was out of the brief's scope and has median fan-in 2.
- **Did not chase the 233 wrong-method / right-class rows** to a per-row verdict.
  They rest on another map row rather than on retail bytes, and they are subject
  to the same pin/rotation gate as the 36 RTTI-proven ones.

## 7. For the next lane

1. **The 218 pin-gated rows are a pin re-homing lane**, not a naming one. Many are
   single-thunk 12-byte blocks; start there and A/B, since re-homing moves bytes.
2. **The 51 rotation-locked rows unlock when the unnamed bodies get names.** The
   blocker is identification, not naming: 44 of 92 thunks branch to an unnamed body.
3. **Re-run both fixed instruments before quoting any earlier figure** — the
   thunk-audit docstring's pre-2026-08-31 numbers are over a 72% population.
