# The allocation-size immediate: an UNMASKED discriminator for masked-class rows

Lane NEWOBJ-1, 2026-08-13. Instrument: `tools/newobj_size_screen.py`
(registered in `tools/screen_gate.py` as `newobj_size` + `newobj_ancestor`).

## Why this instrument is different

CLAUDE.md records **3,262 named rows / 368,948 B** whose name↔address assignment
is *unconstrained by the metric*, because objdiff masks relocations: a `bl` to
the right constructor and a `bl` to the wrong one score identically. Every audit
of that population so far needed **external** evidence (RTTI strings, vtable
membership, retail bytes read by hand).

`X::NewObject()` is a textbook member of the masked class — every one is
alloc-then-construct with an identical shape. But:

```
li      r3, <sizeof>      <-- A PLAIN IMMEDIATE. *** NOT MASKED. ***
bl      <operator new>
bl      <T::T()>          <-- masked; scores the same against any ctor
```

So the allocated size is a fact about the row that **the metric itself can see**,
and the compiler independently reports `sizeof(T)`.

A second, stronger signal came out of the same bytes. Milo's *tagged* allocator
takes the class `Symbol`, so retail emits `bl <T::StaticClassName>` first — and
that callee holds the class name as a **string literal**. `0x824576f8` loads
`"Text"` at `0x8203bca8`. That names the true class **map-independently**.

## ⚠ The ancestor check — without it the screen is wrong about most of what it says

**An inherited static names the BASE, not the derived class.** `NgMat : public
RndMat` does not override `StaticClassName`, so a *correct* `NgMat::NewObject`
calls `RndMat::StaticClassName` and tags `"Mat"`. Reading that as non-identity is
the "a callee NAME never witnesses a member type" trap in a new costume.

The check is exact rather than heuristic because **the compiler computes it**:
our own obj emits a reloc to `?StaticClassName@RndMat@@`, stating which class the
tag *should* name. A mismatch is a finding only when *our* expected tag differs
from retail's.

| | count |
|---|---|
| naive "retail tag ≠ the row's own class name" flags | 32 |
| suppressed as inherited (`DxCam:RndCam`, `NgSpotlightDrawer`, `FxSendDistortion360`, `NgMat`) | **23 (71.9%)** |
| survive | 9 |

A third class exists and must not be collapsed into "false pairing": when
retail's tag names **the row's own class**, retail is *agreeing* with the map —
the pairing is corroborated and the divergence is that *our* class inherits
`StaticClassName` where retail's declares its own (`OUR_TAG_TOO_GENERIC`).

## Population — the discriminator does NOT generalise past the seed case

Extent-bounded (see below), over every named row in `target_symbol_map.json`:

| | count |
|---|---|
| named rows allocating a compile-time-**constant** size | **632** |
| **CHECKABLE** (allocated type recoverable) | **210 (33.2%)** |
| — of those, the `NewObject` seed case | 207 |
| — from anywhere else | **3** |
| type **UNRECOVERABLE** from the row | 422 (66.8%) |

`li r3, N; bl operator_new` inside `Foo::Bar()` proves N bytes were allocated but
never *of what*, so it is not a checkable fact. **This is a real and useful
negative: the vein is the 209 `NewObject` rows and essentially nothing else.**

⚠ The survey's first number was **inflated and the tool now refuses it**: a fixed
96-instruction window ignored function extents, so tiny adjustor thunks read
their *neighbour's* allocation — `0x82b5ada8` and `0x82b5adb0` are **8 bytes
apart** and both "found" the same 180-byte alloc. Bounding at the next known
symbol drops **26** falsely-credited rows and cuts "checkable beyond the seed
case" from 5 to 3.

### Census verdicts (209 rows; 207 target-decoded, 202 both sides)

`AGREE 189 · FALSE_PAIRING_TAG 9 · SIZE_DISAGREE 3 · OUR_TAG_TOO_GENERIC 1 · UNCHECKABLE 7`
⇒ **13 of 202 (6.4%) disagree.**

## Re-adjudication of lane BODIES-6's four findings

Two confirmed, **one refuted**, one confirmed but **relocated to a different class**.

| BODIES-6 claim | verdict |
|---|---|
| `UIPanel` 264≠104 — false pairing | **CONFIRMED.** Compiler: `sizeof(UIPanel)=104`, `sizeof(PreloadPanel)=164`, and the layout shows `[UIPanel]` base members, so PreloadPanel *derives* from UIPanel. Retail's PreloadPanel row allocates exactly 164 at fuzzy 100.00/mpn 100.00. A 264-byte UIPanel cannot be a sub-object of a 164-byte class ⇒ `0x8268b9a0` is not UIPanel::NewObject. Our layout is **not** implicated. |
| `SkeletonClip` 456≠4732 — false pairing | **CONFIRMED and strengthened.** Three independent strands: the tag string is `"Text"`, the callee is `RndText::StaticClassName`, and **our own `RndText::NewObject` allocates exactly 456**. |
| `PropertyEventProvider` −4 — real defect | **REFUTED.** Not a size defect at all. Retail uses the *plain untagged* `operator new` and constructs `??0MsgSource@@QAA@XZ` (a **public** ctor); we use the *tagged* `MemAlloc` + `StaticClassName` and construct `??0PropertyEventProvider@@` (protected). Compiler: **`sizeof(MsgSource) = 68`** = retail's allocation exactly, with a vbptr/vtordisp matching the row's virtual-base adjust tail. ⇒ `0x8274df40` is **`MsgSource::NewObject`**. |
| `NgMat` −16 — real defect | **CONFIRMED REAL, but the root cause is not in NgMat.** |

## ★ The NgMat −16 has a single root cause with wide reach — HANDOFF

`NgMat::NewObject`'s **only** mismatch across 28 instructions is
`li r3, 0x250` (592) vs our `0x260` (608); the vtable store is `??_7NgMat@@6B@`,
so the pairing is genuine. The 16 bytes are inherited:

- compiler: `sizeof(BaseMaterial) = 396 (0x18c)`, `sizeof(RndMat) = 412 (0x19c)`
- RndMat's **only** own members are `ObjPtr<MetaMaterial> mMetaMaterial` (12 B)
  + `mToggleDisplayAllProps` + `mOwnsMetaMat` + `mUpdatingFromMetaMat` + 1 pad
  = **exactly 16 bytes**
- retail's row tagged `RndMat` allocates **396** ⇒ **retail's RndMat adds nothing
  to BaseMaterial**
- `RefreshState`'s offset histogram: **+16 on 71 of 86 instructions (82.6%)**,
  uniform — our members are shifted, not reordered
- **binary-absence proof, with controls**: `meta_material`, `MetaMaterial`,
  `owns_meta_mat`, `toggle_display_all_props`, `updating_from_meta` all occur
  **0 times** in `band.exe`, while 7 of 8 BaseMaterial DTA props
  (`next_pass`, `emissive_map`, `normal_map`, …) are present. The screen can fire.

⇒ **Our `RndMat` carries a 16-byte MetaMaterial block that RB3 retail does not
have** (DC3 is newer and added it). Blast radius is small — `Mat.h` + `Mat.cpp`,
29 references — but it is a *feature removal*, not a layout edit, and it shifts
every RndMat subclass.

**DELIBERATELY NOT DONE.** Removing the feature needs its own lane and a full
whole-binary A/B; a rushed removal could regress the whole material family. The
target is exact and pre-measured: `sizeof(RndMat)` must become **396**, which
takes `NgMat` to 592 and should collapse `RefreshState`'s uniform +16.

This also re-explains the `BaseMaterial` row: `0x8240f5d0` is retail's
**`RndMat::NewObject`** (396, tagged `RndMat`); our `BaseMaterial::NewObject` is
paired to it because both are 396. **The size matched by coincidence and only the
tag exposed it** — the clearest demonstration that the tag adds power beyond size.

## False pairings — REPORTED, NOT FIXED (`target_symbol_map.json` is out of scope)

A map edit needs its own forced re-split and A/B. Sizes are ours/retail.

| row VA | map name | retail tag (string) | ours | retail | note |
|---|---|---|---|---|---|
| `0x824576f8` | SkeletonClip | RndText (`"Text"`) | 4732 | 456 | **strongest** — size + name + our RndText=456 |
| `0x8240f5d0` | BaseMaterial | RndMat (`"Mat"`) | 396 | 396 | is retail's `RndMat::NewObject`; see above |
| `0x8227b408` | PhotoSpotlightPositioner | BandRetargetVignette | 80 | 80 | unrelated classes (both `RndPollable`) |
| `0x8227b9b8` | HamPhotoDisplay | UnisonIcon | 564 | 564 | unrelated |
| `0x8227bb98` | NavigationSkeletonDir | OverdriveMeter (`"OverdriveMeterDir"`) | 648 | 648 | unrelated |
| `0x82329778` | SkeletonViz | LayerDir | 548 | 548 | unrelated |
| `0x82b5a2e0` | FxSendReverb360 | FxSendChorus | 180 | 180 | **swapped pair** |
| `0x82b5adc0` | FxSendChorus360 | FxSendReverb | 180 | 180 | **swapped pair** |

⚠ Five of these (SkeletonClip, PhotoSpotlightPositioner, HamPhotoDisplay,
NavigationSkeletonDir, SkeletonViz) are `gesture`/`hamobj` — **Dance Central
subsystems with no RB3 counterpart**, i.e. the same class of DC3-only pin that
`a0d03243` deleted 16 of at Δmetric exactly zero.

**NOT false pairings — our source is missing a `StaticClassName` override:**

| row VA | map name | retail tag | note |
|---|---|---|---|
| `0x8231d220` | StarDisplay | `StarDisplay` | retail names the row's OWN class ⇒ pairing corroborated |
| `0x8264c828` | AppMiniLeaderboardDisplay | `MiniLeaderboardDisplay` | its **base** (`AppMiniLeaderboardDisplay : public MiniLeaderboardDisplay`); ours resolves to `UIComponent` because *our* `MiniLeaderboardDisplay` does not declare its own. Classified `FALSE_PAIRING_TAG` by the tool only because the rule tests exact class equality — **treat as `OUR_TAG_TOO_GENERIC`.** |

## Rows at mpn 100 that are demonstrably wrong

`BaseMaterial`, `FxSendChorus360`, `FxSendReverb360` and `StarDisplay` all sit at
**mpn 100.00 — counted in `matched_functions`** — while the tag proves the row is
wrong. Another instance of the at-100% defect class, found from the metric's own
input rather than external evidence.

---

# Lane MAPDEF-2 — adjudication and repair of the 8 reported false pairings

2026-08-13. The section above reported 8 false pairings and deliberately left
`scripts/target_symbol_map.json` untouched. This section adjudicates each one on
retail bytes and repairs the map. The oracle was **re-derived from scratch**
(an independent PE decoder + an index-aligned COFF reloc reader) rather than by
importing `newobj_size_screen.py`, so a decoder bug there could not be
reproduced and mistaken for confirmation. It reproduces this doc's census
exactly: **112 AGREE / 9 survivors / 1 corroborated** over 209 rows.

## ⛔ CORRECTION TO THIS DOC: `StarDisplay` is NOT wrong

The "Rows at mpn 100 that are demonstrably wrong" section above lists
**`StarDisplay`** — contradicting this doc's *own* preceding table, which
correctly exonerates it. The preceding table is right and that list
over-collected. Settled on three strands:

- retail's tag string is `"StarDisplay"` — the row's **own** class
- retail's ctor callee is `??0StarDisplay@@QAA@XZ`
- `cl /d1reportSingleClassLayoutStarDisplay` ⇒ **`sizeof = 408`**, which is
  **exactly** the row's retail allocation (`li r3, 0x198`)

`AppMiniLeaderboardDisplay` is corroborated the same way: `sizeof = 432` ==
retail's allocation, retail's ctor names the class **exactly**, and the layout
prints `[MiniLeaderboardDisplay > UIComponent > …]`, so retail's
`MiniLeaderboardDisplay` tag is an inherited static naming a genuine base.

⇒ **A row can be at mpn 100 *and* correct.** Two of the four rows this doc
flagged as "at mpn 100 and demonstrably wrong" were the corroborated class.

## ★ A SECOND INDEPENDENT STRAND THIS DOC DID NOT USE: the ctor callee

The tagged-allocator body calls `bl <T::T()>` after the allocation. That callee
is a *second* witness to the row's true class, and it is the one that settles
the `FxSend` pair beyond argument. Combined with our own obj's reloc (which
states which tag *our* compiler expects), each verdict rests on up to four
strands.

## Verdicts

| row VA | map name | retail tag (string, map-independent) | retail ctor callee | verdict |
|---|---|---|---|---|
| `0x82b5a2e0` | FxSendReverb360 | `"FxSendChorus"` | `??0FxSendChorus360` | **VERIFIED — swapped** |
| `0x82b5adc0` | FxSendChorus360 | `"FxSendReverb"` | `??0FxSendReverb360` | **VERIFIED — swapped** |
| `0x824576f8` | SkeletonClip | (`RndText::StaticClassName`) | `??0RndText` | **VERIFIED false** |
| `0x8227b408` | PhotoSpotlightPositioner | (`BandRetargetVignette::StaticClassName`) | — | **VERIFIED false** |
| `0x8227b9b8` | HamPhotoDisplay | `"UnisonIcon"` | `??0UnisonIcon` | **VERIFIED false** |
| `0x8227bb98` | NavigationSkeletonDir | `"OverdriveMeterDir"` | — | **VERIFIED false** |
| `0x82329778` | SkeletonViz | `"LayerDir"` | `??0LayerDir` | **VERIFIED false** |
| `0x8240f5d0` | BaseMaterial | `"Mat"` | — | **OUT OF SCOPE** (lane MAT-1 owns `RndMat`/`rndobj`) |

### ★ A fifth strand for the swap, purely structural — no bytes decoded

Every `FxSend*360` class in this TU emits `StaticClassName` and then its
`NewObject` at exactly **+0x80**, in a regular `+0x88 / +0x80` stride —
`FxSendDistortion360`, `FxSendDelay360`, `FxSendCompress360`, `FxSendEQ360`,
`FxSendFlanger360`, `FxSendMeterEffect360`, `FxSendWah360`: **8 of 8**.
`0x82b5a260` is `?StaticClassName@FxSendChorus360@@`, so `0x82b5a2e0` is
`FxSendChorus360::NewObject` by adjacency alone. Under the **old** map that
address was `FxSendReverb360::NewObject` — the one class in the whole TU whose
`NewObject` failed to follow its own `StaticClassName`. The swap restores the
regularity.

All 10 rows are genuine `.pdata` BeginAddresses whose extents (112/84 B) equal
our row sizes exactly — which is *why* they scored ~100 and why size alone could
never discriminate them. `sizeof(FxSendChorus360) == sizeof(FxSendReverb360) ==
180`: **only the tag can separate that pair.**

## ⛔ The "DC3-only pin, delete it like `a0d03243`" framing is the wrong reading

The five `gesture`/`hamobj` rows are DC3-only **in the map name only**. The
*retail* classes their tags name — `RndText`, `BandRetargetVignette`,
`UnisonIcon`, `OverdriveMeter`, `LayerDir` — are all **real RB3 classes**, and
none of them is named at any address in the map. So these are not stray pins
over dead space: they are **live RB3 bodies wearing a Dance Central name**.

⚠ **Re-homing them was rejected, not overlooked.** Each address is pinned to its
DC3-only unit (`Gesture.cpp`, `HamPhotoDisplay.cpp`, …) while its true owner
lives elsewhere, so re-homing the name requires *moving splits pins across
units* — a splits change, not a map change. Nulling is the in-scope repair;
the mis-pinned spans are flagged below for a splits lane.

## ★★ A FALSE MAP NAME TAXES EVERY CALLER OF THAT ADDRESS

The single most useful finding here, and it was **not predicted**. Nulling
`0x82329778` (falsely `SkeletonViz`, truly `LayerDir::NewObject`) moved a row in
a *different unit*:

```
+100 B   default/LayerDir   ?Init@LayerDir@@SAXXZ   fuzzy 99.6 -> 100.0
```

`LayerDir::Init` calls that address. Under `functionRelocDiffs=name_check` the
target's reloc carried the **wrong name** while ours carried the right one, so
the caller was penalised for its callee's bad label. Removing the wrong name
un-taxed it.

⇒ **A false pairing costs more than its own row — it leaks into every caller.**
And this is *fourth-party corroboration* of the tag oracle: an unrelated row
reaching 100% exactly when we stop calling `0x82329778` "SkeletonViz" is what
must happen if the address really is `LayerDir::NewObject`.

## Measured (whole-binary A/B, `ab_measure --from-dirty`, forced re-split both legs)

Ruler: **`functionRelocDiffs=name_check`** (the shipped default), objdiff
`4.2.3` / `6bf7ba700ce5`.

| | leg A | leg B | Δ |
|---|---|---|---|
| `matched_functions` | 44,271 | 44,267 | **−4** |
| `masked_equal_functions` | 22,888 | 22,888 | 0 |
| honest (`matched − masked_equal`) | 21,383 | 21,379 | **−4** |
| `matched_code_percent` | 34.440598 | 34.443195 | **+0.002597pp** |
| `matched_code` bytes | | | **+268 B** |
| units at 100% (mpn) | 253 | 252 | −1 |

`−4` = the four DC3-only rows losing false mpn-100 credit. **`SkeletonClip`
moved nothing** — it sat at mpn 99.9643, i.e. it was drawing *no* credit to
remove; nulling it is pure accuracy. `+268 B` = `+84 +84` (the FxSend swap
reaching fuzzy 100, because the reloc *names* now agree) `+100` (the LayerDir
caller above). One unit fell off 100%:
`default/system/hamobj/PhotoSpotlightPositioner` (2 rows → 1 matched), which was
at 100% **on false credit**.

### ⚠ The `none`-ruler control MOVED, and that is CORRECT here

`ab_measure` warned `[control none] Δmatched_code=-448 B … (MOVED -- a name-only
change should not do this)`. That guard is calibrated for **renames**; a null is
a **removal**. −448 B is exactly **4 × 112 B**, the four rows that stopped
pairing. Under `none`, relocation names are masked entirely, so those four
demonstrably-wrong pairings were scoring **fuzzy 100 and drawing 448 B of false
byte credit**. Removing it is the point of the lane, not a defect in it.

⇒ Net accuracy: **4 false function matches and 448 B of false byte credit
removed; 268 B of honest credit gained.**

## Deliberately NOT done

- **`BaseMaterial` / `0x8240f5d0`** — lane MAT-1 owns `sizeof(RndMat)` and
  `src/system/rndobj/**`. Untouched, not adjudicated further.
- **Re-homing the five nulled addresses** to `RndText` / `BandRetargetVignette` /
  `UnisonIcon` / `OverdriveMeter` / `LayerDir` — needs splits moves (above).
- **The mis-pinned spans themselves.** `SkeletonClip.cpp` is pinned over
  `RndText` code, `Gesture.cpp` over `OverdriveMeter`/`ChordShapeGenerator`,
  `HamPhotoDisplay.cpp` over `UnisonIcon`/`EndingBonus`/`StreakMeter`. Nulling
  the `NewObject` row is a partial repair; **the pins are the root cause** and
  are a splits lane's work.
- **No native gate** — this lane changed `scripts/target_symbol_map.json` and
  this doc only, **zero `src/**` edits**, so the gate has nothing to test.

The five nulled addresses are added to `_denylist` so `gen_target_map` cannot
re-emit them — the map's own comment records a prior null becoming a
"permanent oscillator" for exactly this reason.

---

# Lane SPLITS-1 — the ROOT CAUSE: re-homing the mis-pinned spans

2026-08-13. MAPDEF-2 called nulling a **partial** repair and handed off the
mis-pinned `.text` spans. This section completes them: the five addresses are
re-homed to the units that own them and re-named to their **true** classes.

## The oracle was re-derived a THIRD time, and it gained a strand

An independent PE decoder + PPC `bl`/`lis`/`addi`/`li` decoder, written from the
bytes up. It reproduces both prior censuses — and adds a strand neither used:
**the `StaticClassName` callee's own map name gives the C++ class directly**,
where the string literal gives only the *registered Symbol*. That distinction is
load-bearing exactly once, and it is the row that would otherwise have been
mis-named:

| row VA | `li` alloc | `bl` → StaticClassName | tag STRING | ctor callee | compiler `sizeof` |
|---|---|---|---|---|---|
| `0x8227b408` | 80 | `?StaticClassName@BandRetargetVignette@@` | `"BandRetargetVignette"` | — | **80 ✓** |
| `0x8227b9b8` | 564 | `?StaticClassName@UnisonIcon@@` | `"UnisonIcon"` | `??0UnisonIcon@@QAA@XZ` | **564 ✓** |
| `0x8227bb98` | 648 | `?StaticClassName@OverdriveMeter@@` | **`"OverdriveMeterDir"`** | — | **648 ✓** |
| `0x82329778` | 548 | `?StaticClassName@LayerDir@@` | `"LayerDir"` | `??0LayerDir@@QAA@XZ` | — |
| `0x824576f8` | 456 | `?StaticClassName@RndText@@` | **`"Text"`** | `??0RndText@@IAA@XZ` | — |

⇒ `0x8227bb98`'s correct symbol is `?NewObject@OverdriveMeter@@…`, **not**
`OverdriveMeterDir` — `src/system/bandobj/OverdriveMeter.h:16` declares
`OBJ_CLASSNAME(OverdriveMeterDir)`, so the C++ class registers under a different
Symbol. **A tag string is the REGISTERED name, not the class name.** All five
also call the same tagged allocator `0x827bcd38`.

## ⛔ THE DENYLIST WOULD HAVE MADE THE WHOLE MAP HALF INERT

`obj_target_symbol_renamer.py:156` — *"Denied rows are unclaimed regardless of
the value they carry."* MAPDEF-2 put all five addresses on `_denylist`, so
writing the true names **while leaving them denylisted renames nothing**: a
textbook absent-vs-absent leg that would have measured the splits half alone
while reporting on both. Proven by executing the loader, not by reading it —
denylisted ⇒ `<SKIPPED>`, removed ⇒ name emitted, with a never-denied control
row unaffected throughout. The five are removed from `_denylist`; manual entries
still beat `gen_target_map`'s auto guesses, so the names are durable.

⚠ My *first* probe of this was **vacuous** — it called `load_symbol_map`, which
does not exist (`load_address_map` does), and the `hasattr` guard returned a
clean `N/A` on **both** legs. It would have "confirmed" any hypothesis.

## Structural corroboration independent of the tag: the three holes

`BandCharacter.cpp` runs `0x8227AFC0`–`0x8227C6D0` with **exactly three holes** —
and those holes *are* the three stray blocks (PhotoSpotlightPositioner,
HamPhotoDisplay, Gesture). Independently, all three true classes'
`NewObject` are **defined in our `BandCharacter.obj`** (COFF symbol table,
`SectionNumber > 0`, negative control ABSENT). Likewise `0x82329778` is the
*exact* 152-byte gap between `LayerDir.cpp`'s two blocks. Two strands, no map.

`_M_fill_insert<vector<RecordedFrame>>` at `0x82457680` is **DEFINED in
`SkeletonClip.obj` and ABSENT from `Text.obj`**, so it genuinely stays with
SkeletonClip — the block is split at `0x824576F8`, not moved wholesale.

## Boundary convention — measured, not guessed

Our `NewObject` COMDATs are `[8 B EH prefix][112 B body][28–40 B funclet]`
(`symval=8`), so prefix attribution is a real choice. Census of all 6,486 `.text`
blocks: **762 start at the body (prefix billed to the previous block) vs 198
that include it — ~4:1 prefix-exclusive.** Boundaries therefore stay on function
starts, which also leaves 4 of the 5 moves address-identical.

## The moves

| span | from | to |
|---|---|---|
| `0x8227B408–0x8227B4A8` | `system/hamobj/PhotoSpotlightPositioner.cpp` | `BandCharacter.cpp` |
| `0x8227B9B8–0x8227BA58` | `HamPhotoDisplay.cpp` | `BandCharacter.cpp` |
| `0x8227BB98–0x8227BC38` | `Gesture.cpp` | `BandCharacter.cpp` |
| `0x82329778–0x82329810` | `Gesture.cpp` | `LayerDir.cpp` |
| `0x824576F8–0x82457790` | `SkeletonClip.cpp` (split) | `Text.cpp` |

`system/hamobj/PhotoSpotlightPositioner.cpp` had that as its **only** `.text`
block, so its whole splits entry is deleted in the same edit — the documented
"a single-function unit VANISHES rather than reaching 100%" case. It did
vanish; `report.json` did **not** hard-fail.

Invariant asserted before building: the covered address set is **byte-identical**
before and after (2,182,154 words, 0 lost / 0 gained, 0 overlaps) — every move is
pure re-attribution. `.pdata` re-derived itself onto the new owners, including
moving PhotoSpotlightPositioner's `0x821F2138–0x821F2148` to BandCharacter
unprompted — dtk independently agreeing with the re-homing.

## Measured (`ab_measure --from-dirty`, forced re-split both legs)

Ruler **`name_check`** (shipped default), objdiff `4.2.3`, read from provenance.

| | leg A | leg B | Δ |
|---|---|---|---|
| `matched_functions` | 44,268 | 44,268 | **0** |
| `masked_equal_functions` | 22,888 | 22,887 | **−1** |
| honest | 21,380 | 21,381 | **+1** |
| `matched_code_percent` | 34.447920 | 34.449010 | **+0.001090pp (+112 B)** |
| `none`-ruler control | 42.398396 | 42.399094 | **+72 B** |
| units at 100% (mpn) | 252 | 252 | **0 fell off** |

Per-unit: `BandCharacter` 513→516, `Text` 111→113; `Gesture` 7→5,
`HamPhotoDisplay` 2→1, `SkeletonClip` 22→21, `PhotoSpotlightPositioner` 1→0
(vanished). Net 0 — the rows **moved units**, they were not lost.

## ★ THE PREDICTED CALLER-SIDE GAIN DID NOT HAPPEN — a negative result

MAPDEF-2's headline was that a false name taxes every caller, and the brief for
this lane said to expect caller-side gains. **Per-row set-diff over both reports:
ZERO rows changed in any untouched unit.** The entire +112 B / +1 honest is a
single row — `?NewObject@RndText@@SAPAVObject@Hmx@@XZ`, 112 B, fuzzy 100,
`masked_equal=false`.

The mechanism is asymmetric and that is the lesson: MAPDEF-2 gained by
**removing** a wrong name that was taxing callers under `name_check`. Once the
name is gone the callers are already un-taxed, so **restoring the correct name
pays nothing further caller-side.** The caller-tax lever is spent by the null;
re-homing collects only the row's own credit. Do not budget caller-side yield
for the remaining re-homings.

## Deliberately NOT done

- **The material region.** A peer lane flagged `BaseMaterial`/`Mat`/
  `MetaMaterial` as one retail TU carved into three units. A read-only span dump
  over `0x82435520–0x82439000` shows it is **worse than reported: 7 distinct
  units across 9 alternations** — `BaseMaterial → RockCentral → Mat → PostProc →
  MetaMaterial → Mat → MetaMaterial → StringTable → Utl`, i.e. `RockCentral`
  (60 B), `PostProc` (528 B) and `StringTable` (740 B) are interleaved into the
  material run too. ⚠ **The tag oracle was NOT run over this region, so no true
  ownership is established for any block here** — interleaving alone is
  suggestive, not decisive. Hypothesis for a follow-up lane, no edit attached.
- **`0x8240f5d0` / `BaseMaterial`** — still lane MAT-1's row, still untouched.
- **The other `.text` interleavings** between `Text.cpp` and `SkeletonClip.cpp`
  (they alternate across `0x82456260`–`0x82457F70`); only the one block carrying
  a tag-adjudicated address was split.
- **No native gate** — `config/45410914/splits.txt`, `scripts/target_symbol_map.json`
  and this doc only, **zero `src/**` edits**, so it has nothing to test. Said
  rather than skipped silently.

---

# Lane SPLITS-2 — the rest of `0x82456260`–`0x82457F70`: mostly a refutation, with a real defect underneath

2026-08-13. SPLITS-1 left "the other `Text.cpp`/`SkeletonClip.cpp`
interleavings" unadjudicated and handed them here.

## ★★★ The headline is that the interleaving is CORRECTLY OWNED

The blocks pinned to `SkeletonClip` in this region are **not class member
functions at all** — they are **STL template COMDATs over `RecordedFrame`**:

```
vector<RecordedFrame>::{push_back, resize, reserve, _M_erase, _M_fill_insert,
                        _M_fill_insert_aux}
__uninitialized_copy<RecordedFrame*>   __uninitialized_fill_n<RecordedFrame*>
_Vector_base<RecordedFrame>::~_Vector_base
```

`Faders`' two blocks are `_Rb_tree<FaderGroup*>`; `CharUtl`'s is
`vector<Transform>`. **A template COMDAT is emitted by every TU that instantiates
it and placed by the linker independently of TU grouping**, so interleaving into
the `Text` run is the *expected* shape here — not evidence of a mis-pin. This is
a different phenomenon from SPLITS-1's five spans, which were class member
functions wearing a Dance Central unit's name.

Measured by COFF definition census over all **1,204** compiled objs: **22 of 24**
named rows in the region read `PIN-DEFINES` — the pinned unit's own `.obj`
defines the symbol.

## ⛔ MY PREDICTION WAS REFUTED, AND IT CORRECTED THE INSTRUMENT

I predicted `vector<RecordedFrame>::reserve` at `0x82456be8` was mis-pinned,
because it sits in a **BandCamShot** block while `RecordedFrame` is defined only
in `SkeletonClip.cpp`. **`BandCamShot.obj` defines it.** The defining set of
these COMDATs is **five TUs** — `BandCamShot`, `EventTrigger`, `ByteGrinder`,
`SkeletonClip`, `CharLipSync`.

⇒ **"DEFINED in X" is NOT EXCLUSIVE.** SPLITS-1's single-obj `DEF`/`ABSENT`
probe discriminates only when the defining set is a **singleton**; on a template
COMDAT it confirms whatever unit you point it at. The fix is to measure the
**whole defining set** (`tools`-free scratch census over `build/45410914/src/**`)
and treat `ndef > 1` as *non-decisive* rather than as confirmation. A
single-candidate probe cannot fail — the same disease SPLITS-1 caught twice in
itself.

## The real defect: 2,044 B of RndText code wearing SkeletonClip/TourProgress pins

Adjudicated on retail bytes (a `bl`-target decoder) plus a **whole-`.text`
caller scan**, with both controls passing *before* the unknowns were touched: a
known `RndText` function and a known `SkeletonClip` template COMDAT each decode
as expected, and `_M_fill_insert_aux` is seen calling the two adjacent
`SkeletonClip`-pinned `RecordedFrame` helpers.

| span | from | evidence |
|---|---|---|
| `0x824568C0–0x824569F0` | SkeletonClip | `?GetMeshes@RndText@@` is `DEF` in `Text.obj`, **ABSENT** from `SkeletonClip.obj` (`ndef=2`, singleton among candidates), and **our COMDAT is 88 B == retail's `.pdata` extent 88 B exactly**. `fn_82456918` calls `Object::New<RndMesh>`, `RndTransformable::SetTransParent/SetTransConstraint`, and Text-pinned `fn_824553d0` |
| `0x824575D0–0x82457680` | TourProgress | **six callers, three inside `Text.cpp`'s own pins** (`UpdateMesh@RndText`, `Copy@RndText` ×2), **zero** from TourProgress |
| `0x82457800–0x82457D18` | SkeletonClip | three functions calling `?ParseMarkup@RndText@@`, `?SyncMeshes@RndText@@`, `fn_824553d0`/`fn_82455138`/`fn_824555b0` |
| `0x82457D98–0x82457EA0` | SkeletonClip | **`RndText::Init`** — calls `?StaticClassName@RndText@@` + `RegisterFactory`, loads `"text_superscript_scale"`, `"text_guitar_scale"`, `"text_guitar_z_offset"`; its **only** caller is `Rnd::PreInit` |

★ Fourth-party corroboration for the whole run: `fn_82457BA0` is called twice by
`Lyric::UpdateColor`, matching the unclaimed `?UpdateLineColor@RndText@@`. And
the `nearest named symbol` for every caller inside `0x82457800–0x82457D18` is
`??1RndText@@` — the run is RndText code with SkeletonClip *pins* laid over it.

## Deliberately LEFT — refutations and inert rows

- **`0x824575C8–0x824575D0`, 8 B, stays TourProgress.** It is a **tail-call
  thunk** (`addi r3, r3, 0xd8; b 0x82456be8`) at **mpn 100**, defined
  exclusively by `TourProgress.obj`. It is *not* a `.pdata` BeginAddress — the
  documented sub-`.pdata` stub stratum (an 8-byte leaf touches neither stack nor
  LR, so it gets no unwind record). Splitting `0x824575c8` from `0x824575d0` is
  the whole repair there.
- **`?insert_unique@_Rb_tree<Symbol,Award*>` `0x824566E8` (472 B) — inert.**
  Defined in **no** obj we compile: **retail uses a `_Rb_tree` where we use a
  `hashtable`** (a genuine source divergence, not this lane's). It cannot pair in
  any unit, and there is no evidence for a destination, so it stays.
- `fn_82456650` (calls `__uninitialized_copy<RecordedFrame*>`), `fn_824566bc`
  and `fn_82456bbc` (generic `MemOrPoolFreeSTL` deallocators), `fn_82456308`
  (generic list insert) — no positive evidence for a different home.

## ⚠ The `.pdata` decode, and the extent that would have been wrong

The X360 packed `RUNTIME_FUNCTION` flag word is **`PrologLen` = low 8 bits,
`FuncLen` = bits 8..29 in WORDS**. My first decode (`PrologLen` from the MSB)
produced 25 KB functions and was discarded; the corrected one then validated all
**44** extents in the region against the *existing* splits edges. Every new
boundary lands on a function start with a preceding gap of **0 or 4 B of
padding** — never an orphaned 8-byte EH prefix.

★ **`GetMeshes` is 88 B, not the 304 B** that reading "up to the next named
symbol" would have given: `fn_82456918` sits between it and the next named row.
Sizing a span off the map's naming rather than off `.pdata` would have moved 216
extra bytes on no evidence.

## Measured (`ab_measure --from-dirty`, forced re-split both legs)

Ruler **`name_check`** (shipped default), objdiff `4.2.3`, from provenance.
`renamer_patched=1821`.

| | leg A | leg B | Δ |
|---|---|---|---|
| `matched_functions` | 44,268 | 44,269 | **+1** |
| `masked_equal_functions` | 22,887 | 22,887 | 0 |
| honest | 21,381 | 21,382 | **+1** |
| `matched_code_percent` | 34.449010 | 34.449010 | **+0.000000pp (+0 B)** |
| `fuzzy_match_percent` | 48.333980 | 48.334835 | +0.000855pp |
| `none`-ruler control | | | **+88 B** |
| units at 100% (mpn) | 252 | 252 | **0 fell off, 0 vanished** |

`total_code` **10,320,692** and `total_functions` **69,231** are *equal on both
legs* — the build independently confirming the pre-write invariant (covered
`.text` address set byte-identical, 2,182,154 words, 0 lost / 0 gained /
0 overlaps).

### ★★★ The two rulers disagree here BY CONSTRUCTION, and both are right

**+1 function but +0 bytes on `name_check`, +88 B on `none`.** `GetMeshes`
reaches **`mpn` 100** (arg-penalty-excluded) but **not `fuzzy` 100**, i.e. it
matches **modulo relocation names** — its callees are ICF fold-aliases whose
retail map names (`erase@vector<unsigned>`, `push_back@vector<ChatReceiver*>`)
differ from our `vector<RndMesh*>` spellings. This is a textbook instance of
CLAUDE.md's "a change can move functions with Δbytes = 0".

⚠ `ab_measure` warned `[control none] MOVED -- a name-only change should not do
this`. That guard is calibrated for **renames**; a **splits re-attribution**
genuinely moves `none` bytes when a row that previously could not pair starts
pairing, and **+88 B is `GetMeshes`' size exactly**. Same shape as MAPDEF-2's
`none` warning, opposite sign, same conclusion: read the guard, don't obey it
blindly.

## ★ SPLITS-1's caller-side null REPRODUCES

`unit net over ALL units = +1` vs whole-binary `Δmatched = +1` ⇒ **zero rows
changed in any untouched unit.** The only unit that moved is `default/Text`
(113 → 114). `SkeletonClip`'s matched count is **unchanged** — it shed ~1.7 kB of
dead denominator it could never match. Second independent confirmation that
re-homing collects only the row's own credit; **do not budget caller-side yield**.

## Deliberately NOT done

- **No map edit at all.** The 5-entry `_denylist` was checked and contains none
  of these addresses, so the existing `GetMeshes` name was free to be emitted in
  its new unit. The other moved code is unnamed `fn_*` and therefore metrically
  inert — pure re-attribution.
- **The 36 unclaimed `RndText` members are left unnamed.** `RndText::Init`,
  `UpdateLineColor`, `GetDefiningFont` and friends now sit in the right unit and
  are named nowhere; naming is a separate lever (+1 honest / +0.000000pp) and a
  separate lane.
- **The material region `0x82435520`–`0x82439000`** — lane BASEMAT-2 owns it.
  Untouched, not adjudicated.
- **No native gate** — `config/45410914/splits.txt` and this doc only, **zero
  `src/**` edits**, so it has nothing to test. Said rather than skipped silently.
