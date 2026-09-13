# Lane W12-D — VB-1: the vendor band is not a defect zone, and "vendor band" is not a band

**Branch** `w12-vendor-band-screen` · worktree `~/tmp/wt-w12-d` · base `96c3a685`.
Ruler: **`name_check` (graded)**, read from `report.json`'s `provenance.diff_config`.

Baseline after this lane's mandatory first full build (a reflinked worktree's
target objs are pre-renamer, so every mangled-name lookup reads "absent" until
the renamer's pre-compile step has run — FOLDPROVE-2). Sanity check asserted
**before** any name-keyed analysis: **3,083 target objs / 566,787 symbols /
178,521 distinct**, and three known mangled probes
(`?UpdateVocalStyle@VocalTrack@@QAAXXZ`, `?Terminate@NgRnd@@UAAXXZ`,
`??0?$ObjPtr@VObjectDir@@@@QAA@ABV0@@Z`) all resolve. 3,083 objs is W11-D's
figure exactly.

```
matched_functions   42,818      masked_equal        22,994
matched_code     3,886,704 B    matched_code_percent 37.934030
total_code      10,245,956      total_functions      69,219
                                fuzzy_match_percent  49.149975
```

---

## 0. The headline

> ★★★★★ **VB-1 is a bounded negative, and the reason is that its premise is
> false. `>= 0x82A00000` is not "the vendor band" — it is three regions, and
> the one all four of W11-D's blockers live in is majority RB3/Milo GAME code.**
> Screened properly, vendor-band STLport template rows are byte-corroborated at
> **76.2% (163/214)** against **71.5% (2,276/3,185)** for non-vendor ones. The
> band is *better* than baseline, not worse. Proven map defects in the class:
> **2 of 214 = 0.93%.**

| # | question | verdict |
|---|---|---|
| **VB-1** | are vendor-band map rows naming Milo STLport templates suspect? | **NO — bounded negative, §1–§3. Denominator 214; proven defects 2 (0.93%); corroboration rate ABOVE the non-vendor baseline.** |
| **VB-1a** | what is `>= 0x82A00000` actually made of? | **Three regions, §1. Region C (>= `0x82B54190`, 2,661 rows) is owned by VocalTrack, Synth, GemManager, TrackPanel, Mic, GemTrack, PostProc_NG, Lit_NG — game/engine code, 54.5% at fuzzy 100.** |
| **VB-1b** | the residual — is any of it a real map defect? | **2 rows. `0x82b9b590` (W11-D's, unmoved) and `0x82b74600` (newly PROVEN wrong here, §4) — and the second one unblocks W11-D's largest handoff.** |
| **H5** | `__destroy_aux<LevelData>` duplicate | **ALREADY CLOSED ON MAIN — both addresses are in the map's `_denylist` with a written rationale. §3.1.** |
| **H6′** | `~vector<String>` is at `0x822d8cc0`, worth +3,084 B | **CONFIRMED and enlarged to 29 rows / 3,852 B; INSTALLED and measured, §4–§5.** |

---

## 1. "Vendor band" is three regions, and the label is load-bearing wrong

CLAUDE.md already warns that **XDK is interleaved throughout `.text`, not
confined above `0x82A00000`** — an address-band pass once read 42.7% of rows as
GAME and all 3,941 were Quazal. This lane is the mirror-image instance of the
same trap: a band pass reading region C as VENDOR when it is mostly game code.

`.text` is `0x82270000`–`0x82c4ce3c` (PE section headers of retail `band.exe`).
Of 29,149 named address rows in `target_symbol_map.json`, **310 sit above
`.text`** and are `.data`/`.bss` symbols, not functions — they must be excluded
before any "function row" claim. Of the rest, `>= 0x82A00000` decomposes:

| region | span | named rows | in report | fuzzy 100 |
|---|---|---:|---:|---:|
| **A** pre-Quazal | `0x82A00000`–`0x82A6D168` | 1,076 | 1,076 | **0** |
| **B** Quazal NetZ `/Od` | `0x82A6D168`–`0x82B54190` | 37 | 37 | 18 |
| **C** post-Quazal | `0x82B54190`–`0x82c4ce3c` | 2,661 | 2,649 | **1,445 (54.5%)** |
| D | above `.text` | 310 | 0 | 0 |

* **Region A is real vendor code** — XDK `XGRAPHICS`, the D3DX shader compiler,
  `XTitleServer*`. Zero at 100 because we have no source for it, by directive.
* **Region B is the Quazal `/Od` block**, and it is nearly *unnamed* (37 rows
  across 933 kB) — which is why a rows-only histogram makes it look like a gap.
* **Region C is not vendor.** Its top owning units are `VocalTrack` (123),
  `system/synth_xbox/Synth` (96), `GemManager` (72), `band3/bandtrack/TrackPanel`
  (68), `Mic` (51), `GemTrack` (48), `TDStretch` (43), `Synapse_dsp` (40),
  `Voice` (39), `band3/bandtrack/Track` (35), `Gem` (35), `PostProc_NG` (34),
  `Env_NG` (29), `Lit_NG` (27), `GemSmasher` (23) — interleaved with genuine
  vendor (`xdk/xaudio2/*`, `xWMA`, `soundtouch`). It is the **audio / bandtrack /
  NG-renderer tranche**, i.e. a link-order artifact, not a vendor/game boundary.

⇒ The reasoning "a Milo engine template instantiated into the vendor band is a
map naming defect" has no premise in region C: **Milo code legitimately lives
there**, so Milo template instantiations legitimately live there too.

---

## 2. The screen, with its denominator

**Population:** map rows at an address inside `.text` whose name contains
`stlpmtx_std` (STLport is Milo's STL; this is the "Milo STLport template" class
VB-1 names). **3,399 rows**, of which **214 (6.30%)** are `>= 0x82A00000`.

**Instrument:** a row is *byte-corroborated* when its `report.json`
`fuzzy_match_percent` is 100 on the graded `name_check` ruler — that means our
independently-compiled body agrees with retail's bytes at that address
**including every relocation NAME**. A wrong map name cannot produce it.
Corroboration is a positive test only: its absence is not a defect, because a
row can be sub-100 for ordinary decomp reasons.

| population | n | fuzzy 100 | sub-100 | 0% | absent |
|---|---:|---:|---:|---:|---:|
| **vendor-band** (`>= 0x82A00000`) | **214** | **163 (76.2%)** | 39 (18.2%) | 10 (4.7%) | 2 (0.9%) |
| **non-vendor** (control) | **3,185** | **2,276 (71.5%)** | 832 (26.1%) | 72 (2.3%) | 5 (0.2%) |

> ★★★★ **The control is the result.** Without the non-vendor column, "76.2%
> corroborated" is just a number; against it, the vendor band is **+4.7 pp
> BETTER** than the rest of the tree. A screen whose target population is
> healthier than its control is not a backlog.

W11-D's own sub-figure reproduces exactly: **4 of 118** `_Param_Construct` /
`_Copy_Construct` rows are vendor-band (3.39%). Adjudicated individually:
`0x82b5bda0` `_Param_Construct<LevelData>` **fuzzy 100 = correct**;
`0x82ba3298` `_Copy_Construct<deque<TubePlate*>>` **fuzzy 100 = correct**;
`0x82b6f100` `_Copy_Construct<vector<float>>` 99.67 = ordinary residue;
`0x82b9b590` `_Param_Construct<Char3D>` = **the one real defect**. So even the
sub-class W11-D pointed at is **1 defect in 4**, not a vein.

### 2.1 Why W11-D saw a defect-rich band and this screen does not

Not a contradiction — a **selection effect**, and it is the hazard my brief
names first. W11-D adjudicated rows *it had already been blocked by*. A sample
selected for being a blocker has a high defect rate by construction. Screening
the class itself, rather than the artifact one happens to hold, moves the rate
from "most of what it touched" to **0.93%**.

---

## 3. The residual, decomposed (the honest 51)

Of the 214, 51 are not corroborated. **None of them is a map defect except the
one in §4.**

* **39 sub-100** are ordinary decomp residue in real, pinned Milo units, mostly
  at 99.7–99.9: `Synapse_dsp` (8 rows, `PitchCorrectedVoice` / `vector<float>`
  containers), `VocalTrack` (12), `GemManager` (5), `Mesh`, `TrackPanel`,
  `ContextChecker`, `Lit_NG`, `MultiTempoTempoMap`, `SpectralAnalysis`. These
  are *our source not being finished*, not the map being wrong.
* **10 at 0%** — 4 are in unattributed `auto_*` units (0% by construction, no
  source), the other 6 are real unfinished rows in `Msg`, `GemManager`,
  `VocalTrack`.
* **2 absent from report** — see §3.1.

### 3.1 H5 is already closed on `main`, and my screen is what noticed

The two `ABSENT` rows are both `??$__destroy_aux@ULevelData@@…` at `0x82b5b1d0`
and `0x82b63ec8` — W11-D's H5. They are **already in the map's `_denylist`**,
with a rationale recorded in `_denylist_comment` that settles it on retail
bytes: they are *not* byte-twins (`addi r3,r3,0x78; b 0x82b69220` vs
`addi r3,r3,0x64; b 0x82b69220`), so at most one can be real, and a single
struct-offset edit fixes either. ⇒ **H5 needs no schema change and no
"vendor-band duplicate survivor" category.** W11-D's §5 was written against a
tree where this had not yet landed.

---

## 4. `0x82b74600` — the one row VB-1 found, PROVEN wrong, and it unblocks H6′

This is the single genuine map defect the screen surfaced that was not already
known, and it is worth more than the screen it came from.

The map called it `??1?$vector@VString@@…`. **It is not a `~vector<T>` of any
kind.** Read off the aligned target stream (`objdiff-cli diff -f json
--include-instructions`, graded ruler, so this is the ruler's own view of retail):

```
addi r3, r3, 0x38 ; bl ??1?$vector@V?$vector@M…    <- destroy member at +0x38 (vector<vector<float>>)
lwz  r4, 0x2c(r30) … li r10,0x18 … bl MemOrPoolFreeSTL   <- free buffer at +0x2c, element stride 0x18
lwz  r4, 0x20(r30) … srawi r11,r11,6; slwi r3,r11,6; bl MemOrPoolFreeSTL
                                                    <- free buffer at +0x20, element stride 0x40
```

**A `~vector<T>` frees exactly one buffer.** This one frees two and delegates a
third member — it is a *class* destructor with three container members, sitting
inside `GranularSynth.cpp`'s pinned range (`0x82b73cd0`–`0x82b74b28`) directly
after `?Flush@GranularSynth@Synapse@DSP@@QAAXXZ`. Almost certainly a Synapse DSP
class dtor; **the correct name is not established, so the address is left
anonymous rather than guessed**, and added to the map's `_denylist` (with the
above rationale written into `_denylist_comment`) so `gen_target_map` cannot
re-emit the wrong name.

### 4.1 Testing W11-D's H6′ literally, and the objection that dissolved

Evicting that row frees the name `~vector<String>` for `0x822d8cc0`, which is
W11-D's H6′. My brief says to test a briefed figure rather than inherit it, so:

| leg | finding |
|---|---|
| **element stride** | retail `0x822d8cc0` computes `li r10,0xc` / `mulli r3,r11,0xc` ⇒ **12-byte** elements. Our `~vector<ObjPtrVec<Object,ObjectDir>::Node>` uses `srawi 4`/`slwi 4` ⇒ **16-byte**. **Different-size COMDATs cannot fold** ⇒ the map name is wrong, on bytes, with no map dependency. |
| **size** | target extent 136 B; our `ObjPtrVec::Node` body 132 B. Confirms W11-D. ⚠ Its "132 B" is easy to misread — `report.json`'s `size` is the **target** extent, and the 132 B row in `default/Character` is the *`RndGroup`* instantiation, a different type. |
| **caller population** | over **every sub-100 row of all 22 units** whose base obj defines `~vector<String>` (261 rows scanned): **30 rows** call `~vector<String>` where retail calls `0x822d8cc0`. Over **all 1,442 rows** of the 10 units defining `~vector<ObjPtrVec::Node>`: **0** rows call it where retail calls this address. |
| **existing T1 evidence** | the alias group already anchored at `0x822d8cc0` records **T1 = retail bytes at this address are byte-identical to our compiled `~vector<ObjDirPtr<ObjectDir>>`** — an independent, pre-existing byte adjudication of the same address. |

> ⛔ **The objection that nearly produced a false refusal.** Retail's body at
> `0x822d8cc0` calls `__destroy_range_aux<reverse_iterator<ObjDirPtr<ObjectDir>*>>`,
> **not** `<…String*>` — which reads as a flat contradiction of "this is
> `~vector<String>`", and `sizeof(ObjDirPtr<ObjectDir>)` is also 12, so the
> stride test does not separate them. I had written the refusal before applying
> CLAUDE.md's rule *"grep `symbol_aliases.json` BEFORE believing a reloc-name
> find"*. That callee address, `0x82710808`, is an existing **T1-proven ICF fold
> group of 19 spellings whose survivor is the `ObjDirPtr<ObjectDir>` spelling and
> which contains `<reverse_iterator<String*>>` as a folded member.** The callee
> name was an arbitrary fold-survivor pick. ⇒ **A relocation-name "contradiction"
> is not evidence until it has been checked against the alias file** — the rule
> earned its place here, and it flipped a refusal into the lane's whole yield.

⇒ Repair: `0x82b74600` → `_denylist`; `0x822d8cc0` → `??1?$vector@VString@@…`.
Name injectivity re-verified over all 29,030 remaining address rows: the only
duplicate names are the two pre-existing allowlisted/denylisted ones.

---

## 5. Measured — and the negative run is the informative one

Both runs are `tools/ab_measure.py --worktree ~/tmp/wt-w12-d --from-dirty`,
map-kind so both legs force a re-split and iterate to a `symbols.txt` fixed
point (0 extra re-splits needed on either leg; `renamer_patched=1822`).

| run | change | predicted | measured | |
|---|---|---|---|---|
| 1 | map only (denylist `0x82b74600`, re-home `0x822d8cc0`) | **+29 fns / +3,852 B** | **+9 fns / +836 B** | ✗ **3.4× short** |
| 2 | map **+ alias re-anchor** | **+12 fns / +3,916 B, 0 fall off** | **+12 fns / +3,916 B, 0 fall off**, Δcode% **+0.038220pp** | ✓ **exact** |

Run 2, in full: `matched_functions` 42,818 → **42,830**, `matched_code`
3,886,704 → **3,890,620 B**, `matched_code_percent` 37.934030 → **37.972250**,
`masked_equal` 22,994 → 22,994 (unchanged). Attribution:
**`unit net (ALL units) = +12` == whole-binary Δmatched = +12**; units at 100%
163 → 164 on `mpn` and 135 → 136 on all-rows-fuzzy, **1 reached, 0 fell off** on
both rulers. `Δfuzzy = −0.001825pp` (a re-split artifact of the denominator, not
a regression: `total_code` is unchanged at 10,245,956).

### 5.1 ⛔ Why run 1 missed, and it was NOT the crossing prediction

Diffing the two archived leg reports row-by-row settles it:

```
rows that CROSSED to 100 : 30   +3,916 B     <- all 29 predicted, plus one bonus
rows that FELL OFF 100   : 10   -3,080 B     <- unscreened
                                   3,916 - 3,080 = +836   exactly the measured Δ
mpn: +12 crossings, -3 fall-offs = +9        exactly the measured Δmatched
```

**The gain prediction was right to within one bonus row** (`??1?$scoped_ptr@
VGranularSynth@Synapse@DSP@@`, 64 B). What I failed to screen was a *loss*:
`?Init@UIManager@@` (1,916 B), `??1ObjectDir@@` (504 B), `??1MetaMusic@@`
(360 B) and 7 small rows all fell off 100.

Cause: the alias group anchored at `0x822d8cc0` had **survivor =
`~vector<ObjPtrVec::Node>`** — solely because that was the map name there — with
`~vector<ObjDirPtr<ObjectDir>>` folded into it. Renaming the map row **orphaned
the group**, so every call site where our source spells `ObjDirPtr<ObjectDir>`
lost its forgiveness and became charged.

> ★★★★★ **A reverse-risk census keyed on "which units DEFINE the old name" cannot
> see risk that arrives through an ICF alias to a THIRD spelling.** My census was
> thorough on its own terms — 1,442 rows over all 10 units defining
> `~vector<ObjPtrVec::Node>`, and its answer (**0 sites at risk**) was *correct*.
> The 10 rows that broke do not call that name at all. This is the "screen
> defined over the artifact you happen to hold" hazard in a new shape: I
> enumerated *name users* when the unit of risk was *group members*.
> **Before re-homing any map row, grep `symbol_aliases.json` for its address —
> not just for its name.**

### 5.2 The alias edit adds no fold claim, and the two runs prove it

`ALIAS_SUSPECT` fired on both runs (`none` flat, default ruler up). It fires on
**every** map-only patch by construction — `none` ignores relocation names, so
it is flat whatever the truth is, and CLAUDE.md is explicit that this shape
licenses nothing in either direction.

The license here is (a) the five-leg retail-byte adjudication in §4.1, and (b) a
structural fact that is *measured, not asserted*: **the alias edit restores
exactly the forgiveness that existed in leg A and creates none.** The group's
`folded` list is byte-identical before and after; only the `survivor` label
follows the map. Run 1 lost 3,080 B by orphaning it; run 2 recovers **exactly**
3,080 B (3,916 − 836). A fabricated alias would have produced a *surplus* over
leg A's baseline, and there is none — every one of the 10 rows returns to 100
and not one new row joins them.

`??1?$vector@UNode@?$ObjPtrVec@…` is deliberately **not** added as a folded
spelling: it cannot fold (132 B vs 136 B), and the census found 0 sites wanting
it. Its own row in `default/Flow` (93.38, contributing 0 bytes) simply unpairs —
priced at Δ0 in advance and confirmed by the 0-fall-off attribution.

`tools/icf_alias_finder.py --validate` after the edit:
**PASS — 1363 map-consistent, 238 tolerated, 0 contradicted, 1603 total.**

---

## 6. The `??0` side — `FileMerger::Merger` (W11-C handoff #2)

W11-C screened this row clean (216 B, `??0Merger@FileMerger@@QAA@PAVObject@Hmx@@@Z`,
fuzzy 76.09, exactly one ours-only callee: the `ObjPtr<ObjectDir>` owner ctor)
and deferred it because "the ctor definition is not in the unit's own `.cpp` and
locating the right TU needed more time".

**Located.** The ctor is declared inline in `src/system/char/FileMerger.h:33`
(`Merger(Hmx::Object *o) : mProxy(0), mPreClear(0), mSubdirs(…), mDir(o),
mLoadedObjects(o), mLoadedSubdirs(o) {}`) and the out-of-line copy that retail
pins lands in **`src/system/bandobj/BandWardrobe.cpp`** — which is why grepping
the report unit (`default/BandWardrobe`) rather than the header's own directory
is what finds it. Only `mDir` is an `ObjPtr<T>`; `mLoadedObjects` /
`mLoadedSubdirs` are `ObjPtrList<T>`, a different template the gate does not
touch — consistent with W11-C's screen finding exactly **one** ours-only callee.

⚠ **`Merger` has no base class and no vptr**, so W11-C's rule ("members are
sensitive to `DEFER_OWNER`, locals are not") does not decide the shape here: the
store order that matters is the one inside `ObjPtr`'s own ctor, not `Merger`'s.
The shape therefore has to be picked by measurement, exactly as W11-C picked
`RndPartLauncher`'s.

⚠ **The risk is TU-wide, and it is large.** `BandWardrobe.cpp` is a 270-row unit
with **217 rows already at fuzzy 100**. The gate is per-TU, so it applies to
every `ObjPtr` construction the TU inlines, and a 216 B prize is not worth
knocking a 2,416 B or 1,944 B row off 100. This is priced by whole-binary A/B,
not by the row.

### 6.1 The shape, picked by measurement — and the plain gate is a trap

Three full builds, one variable each, whole-binary `report.json` read every time
(never a single-`.obj` build — the six post-compile patchers are part of the
ruler):

| gate on `BandWardrobe.cpp` | `??0Merger@FileMerger@@` (216 B) | BandWardrobe rows at fuzzy 100 | whole-binary |
|---|---|---|---|
| none (baseline) | 76.0926 | 217 / 270 | 42,830 / 3,890,620 |
| `RB3_OBJPTR_INLINE_OWNER_CTOR` | **67.96 — WORSE** | 217 / 270 | 42,830 / 3,890,620 (Δ0) |
| `… + RB3_TU_OBJPTR_DEFER_OWNER` | **100.0** | **218 / 270** | 42,831 / 3,890,836 |

★ **The plain gate scores BELOW not inlining at all** — `obj/Object.h` documents
exactly this ("inlining in the WRONG STORE SHAPE scores worse than not inlining")
and W11-C hit it on `RndPartLauncher` (DEFER_OWNER 96.29 vs plain 77.07). A lane
that tried only the plain gate here would have read 76 → 68, concluded "retail
does not inline this after all", and dropped a collectable row. **The screen says
*whether* retail inlines; it does not say in which of the three store orders, and
that must be measured.**

⚠ **And the TU-wide risk was real but did not fire**: 217 rows at 100 held under
both shapes, and no other `BandWardrobe` row moved in either direction. That is
worth stating as a measured fact rather than an assumption, because it is the
reason this row was deferrable in the first place.

---

## 7. `0x82b74600` IDENTIFIED — `??1GranularSynth@Synapse@DSP@@QAA@XZ`

The lane first denylisted it as "proven wrong, correct name not established".
It was then identified outright, by the cheapest possible test:

```
??1?$scoped_ptr@VGranularSynth@Synapse@DSP@@@@QAA@XZ   fuzzy 100.0, 64 B
    target  bl  fn_82B74600
    base    bl  ??1GranularSynth@Synapse@DSP@@QAA@XZ
```

A row already at **fuzzy 100** whose retail side calls `fn_82B74600` exactly
where our source calls `~GranularSynth`. It corroborates the body read in §4
independently: `GranularSynth.cpp` instantiates `vector<Voice>` and
`vector<Granule>` (the 0x18- and 0x40-stride buffers) and `Synapse_dsp.h`
carries the `vector<vector<float>>` (the delegated member at +0x38).

⇒ the address is **named, not denylisted**. Δ **exactly 0**, pre-registered and
measured: naming a previously-anonymous address has no call-site byte upside
because `name_check` already forgives placeholder targets — it converts a
*forgiven* site into a *checked* one, and here the check passes.

★ **A splits observation falls out of this, and it is a handoff.** Our
`~GranularSynth` compiles into **`Synapse_dsp.obj`**, but `0x82b74600` is pinned
inside **`GranularSynth.cpp`**'s `.text` range (`0x82b73cd0`–`0x82b74b28`). So
the row reads 0% and **cannot pair at any source quality** until the pin is
re-homed. That is a `splits.txt` question (re-homing is *not* metric-neutral —
lane PINHOME-1 measured +3 fns / +428 B), not a map one.

---

## 8. Lane total

Three measured changes, **four pre-registered predictions, three exact and one
instructive miss that was diagnosed to the byte**:

| # | change | predicted | measured |
|---|---|---|---|
| 1 | map only | +29 fns / +3,852 B | **+9 / +836** ✗ (§5.1) |
| 2 | map + alias re-anchor | +12 fns / +3,916 B, 0 fall off | **+12 / +3,916, 0 fall off** ✓ |
| 3 | `BandWardrobe.cpp` DEFER_OWNER gate | +1 fn / +216 B | **+1 / +216** ✓ |
| 4 | name `0x82b74600` | **exactly 0** | **0 on every key** ✓ |

**Net landed: +13 functions / +4,132 B / +0.040328 pp**, `matched_functions`
42,818 → 42,831, `matched_code` 3,886,704 → 3,890,836 B, `matched_code_percent`
37.934030 → **37.974358**. On every run `unit net (ALL units) == whole-binary
Δmatched`, and **0 units fell off 100%** on either ruler in any run.

---

## 9. What this lane did NOT do

* ⛔ **Did not open the other 13 mixed positive-delta `??0` rows** W11-C left —
  `BandDirector` (1,088 B, +5), `BandCharacter` (2,180 B), `VocalTrack`,
  `CharDriver`, `CharFaceServo`, `CrowdAudio`, `PatchPair`, `RndParticleSys`,
  `AnimTask`. Only `FileMerger::Merger` was taken, because it was the one W11-C
  had already screened to a *single* ours-only callee; the rest are genuinely
  mixed and each needs its own callee breakdown first.
* ⛔ **Did not re-run W11-C's `bl`-count screen.** Its handoff #3 asks for a
  re-run after any inline-gate wave, and this lane just landed one
  (`BandWardrobe.cpp`). The ranked list is now one TU stale.
* ⛔ **Did not touch `??0RndMat@@` or `??0AnimTask@@`** — different defect
  classes by direction, and out of scope by the brief.
* ⛔ **Did not re-home the `0x82b74600` pin** (§7) — a `splits.txt` change, not
  metric-neutral, and it must be measured on its own.
* ⛔ **Did not attempt W11-D's H4′** (Char3D / `0x82b9b590`, +192 B / +2 fns).
  It is the *second* real defect the screen confirms, and it is blocked on the
  same shape of question — two candidate homes for `LocalePanel::Entry`'s
  construct helper. Left as the one open row of the VB-1 residue.
* ⛔ **Did not adjudicate the 39 sub-100 vendor-band rows individually.** They
  are ordinary decomp residue in real units (§3) and the screen's job was to
  size the class, not to finish `Synapse_dsp`.
* ⛔ **Did not run the permuter** (off by directive), and **did not prune any
  alias spelling** — the group at `0x822d8cc0` keeps exactly the membership it
  had.
* ⚠ **Did not re-measure the reachable ceiling or the `auto_*` class.** Nothing
  here depends on them, and the standing rule is to re-measure rather than
  inherit — so no figure for them is quoted.

---

## 10. Handoffs

| # | handoff | evidence in hand |
|---|---|---|
| **VB-1: CLOSED** | **Do not re-screen the vendor band for Milo STLport map defects.** 214 rows, 2 proven defects (0.93%), corroboration rate 76.2% vs a 71.5% control. Both defects are now fixed or documented. | §2, §3 |
| **SPLIT-GS** | Re-home the `0x82b74600` pin: `~GranularSynth` compiles into `Synapse_dsp.obj` but the address is pinned in `GranularSynth.cpp`'s range, so a 136 B row is unpairable at any source quality. | §7 |
| **H4′** (unchanged from W11-D) | Identify `0x82b9b590` (constructs `LocalePanel::Entry`; competing home `fn_824ADBD0`), then install the Char3D fold. +192 B / +2 fns. | W11-D §6.1 |
| **CTOR-13** | W11-C's 13 remaining mixed positive-delta `??0` rows, **plus** the rule this lane adds: the `bl` screen says *whether* retail inlines, never *which* of the three store orders — budget a build per shape, and treat a plain-gate regression as evidence of the wrong shape, not of "retail does not inline". | §6.1 |
| **ALIAS-ADDR** | Before re-homing **any** map row, grep `symbol_aliases.json` for its **address**, not its name — an alias group anchored there silently forgives spellings that a name-keyed reverse-risk census cannot see. This cost 3,080 B in run 1 and is a generic pre-flight check, not a one-off. | §5.1 |

---

## 11. Reusable lessons

* ★★★★★ **An address band is not a provenance classifier, and this is the
  second measured instance.** CLAUDE.md already records XDK being interleaved
  throughout `.text` (a band pass read 42.7% of rows as GAME and all 3,941 were
  Quazal). This is the mirror image: `>= 0x82A00000` reads as VENDOR and its
  largest region is owned by `VocalTrack`, `Synth`, `GemManager`, `TrackPanel`,
  `Mic`, `GemTrack`, `PostProc_NG`, `Lit_NG`. **Before inferring anything from a
  band, list the units that own it.**
* ★★★★★ **A screen needs a control population or its rate means nothing.**
  "76.2% of vendor-band STLport rows are corroborated" sounds like a backlog
  until the non-vendor column reads 71.5%. The control is what turns a number
  into a verdict, and here it turns the verdict *negative*.
* ★★★★★ **A relocation-name "contradiction" is not evidence until you grep
  `symbol_aliases.json`.** Retail's callee at `0x822d8cc0` names
  `ObjDirPtr<ObjectDir>` where the hypothesis needs `String`; I had the refusal
  drafted. That callee address is a T1-proven 19-member ICF fold group **whose
  folded list contains the `String` spelling** — the name was an arbitrary
  survivor pick. The rule was already written down; applying it converted a
  false refusal into the lane's entire yield.
* ★★★★ **Ask what unit your reverse-risk census is enumerating.** Mine was
  correct and thorough over "units that DEFINE the old name" (1,442 rows, answer
  0) and still missed 3,080 B, because the risk arrived through an ICF alias to
  a **third spelling**. Name-users and group-members are different populations.
* ★★★★ **A prediction that misses is worth more than one that hits, if you
  diff the leg reports.** Run 1 was 3.4× short; row-wise diffing showed the
  *gain* prediction was right to one bonus row and the whole gap was an
  unscreened loss (3,916 − 3,080 = 836, and mpn +12 − 3 = +9). That decomposition
  is what produced the correct second change; a lane that had only the headline
  would have concluded the fold analysis was wrong.
* ★★★★ **`report.json`'s `size` is the TARGET extent, not ours.** W11-D's
  "our `~vector<ObjPtrVec::Node>` is 132 B" is right, but the 132 B *row* in the
  report belongs to a different instantiation (`RndGroup`, in `Character`) than
  the 136 B one (`Object`, in `Flow`) — near-identical mangled names one
  template argument apart. I briefly mis-read this as refuting W11-D. Diff the
  symbol; do not template-match the name.
* ★★★ **An inline-policy screen is a two-stage instrument.** The `bl` count says
  retail inlines; it says nothing about which of the three store orders, and the
  plain gate here scored **below not inlining at all** (76.09 → 67.96) before
  `DEFER_OWNER` reached 100.0. A single-shape trial produces a confident false
  negative.
* ★★★ **`ALIAS_SUSPECT` on a map-only patch is uninformative by construction**
  — `none` ignores relocation names, so it is flat whether the alias is real or
  fabricated. What discharged it here was not the control but a *structural*
  argument the two runs measure between them: the alias edit changed no `folded`
  list, and recovered exactly the 3,080 B that orphaning it had cost — a
  fabricated alias would have shown a surplus over leg A, and there was none.
