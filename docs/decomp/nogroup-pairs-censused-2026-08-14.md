# Charged pairs that touch NO alias group — censused and classified

**Lane NOGROUP-1, 2026-08-14, on `9544fa4c`.** Lane INCOMPLETE-1 (`5f44f30f`)
censused every charged relocation-name pair tree-wide, harvested the
*incomplete-group* slice for **+60,700 B**, and explicitly flagged the remainder as
a vein it did not work: **1,393 pairs touching no group at all**. This lane
classifies them.

Reproduced on `9544fa4c` at **1,392 pairs / 2,122 sites** (one fewer than
INCOMPLETE-1's reading — expected, since `9544fa4c` installed 40 memberships and
the DataNode fix).

## ⛔ First correction: the 302,348 B was never a prize, and it is not even the
## number the census tool prints

INCOMPLETE-1's doc reports the FRESH row as *1,314 closable rows / 302,348 B*. Its
own tool, run here, prints **1,505 rows / 397,832 B** for that row, because
`incomplete_group_census.price()` computes **"bytes on sub-100 victim rows"** — the
figure the same doc warns "is ~30% larger and must not be quoted". Neither number
is a prize:

* `matched_code` is **ALL-OR-NOTHING PER ROW**, so a row pays only when *every*
  charge on it closes. Priced that way — `fuzzy < 100`, `mpn >= 100`, **and every
  surviving charge on the row in-class** — the whole population jointly closes
  **316,964 B**, and that requires closing all 1,392 pairs *including 313 genuine
  source defects and 380 pairs refuted on size*.
* The per-class figures are far smaller than the joint one precisely because most
  rows carry charges from **more than one class**.

⚠ Even the strict figure is an upper bound: `mpn == 100` also tolerates REGISTER
and BRANCH-DEST arg penalties, which a name fix does not close either.

## The census — the deliverable

| verdict | pairs | sites | rows | closable rows | closable B | gross B |
|---|---:|---:|---:|---:|---:|---:|
| **FOLD_PROVEN** | **4** | 5 | 5 | 3 | **920** | 2,464 |
| FOLD_unproven_vacuous | 50 | 72 | 65 | 57 | 18,252 | 21,300 |
| FOLD_unproven_tolerance | 9 | 13 | 7 | 5 | 484 | 3,088 |
| FOLD_blocked_residency | 1 | 1 | 1 | 1 | 292 | 292 |
| MAP_CONTRADICTION | 5 | 6 | 4 | 3 | 480 | 5,908 |
| MAP_REPAIR_CANDIDATE | 54 | 99 | 71 | 49 | 14,444 | 34,108 |
| **SOURCE_DEFECT** | **311** | 432 | 370 | 291 | **44,076** | 82,000 |
| SOURCE_DEFECT_weak | 2 | 2 | 2 | 2 | 24 | 24 |
| UNDECIDABLE_size | 380 | 572 | 498 | 400 | 87,264 | 123,300 |
| UNDECIDABLE_relocs | 318 | 569 | 436 | 328 | 88,244 | 145,608 |
| UNDECIDABLE_absent | 223 | 301 | 288 | 87 | 11,636 | 24,228 |
| UNDECIDABLE_bytes | 35 | 50 | 41 | 27 | 6,452 | 8,888 |
| **ALL FRESH (jointly)** | **1,392** | 2,122 | 1,692 | 1,330 | **316,964** | 397,832 |

**⇒ 956 of 1,392 pairs (68.7%) are UNDECIDABLE, and only 920 B is aliasable.**
The aliasing vein in this population is, to a first approximation, **empty**.

### The discriminator

Every FRESH pair is `(S, F)` where **S is necessarily a MAP name** — dtk spells a
target-side callee with a mangled name only where `obj_target_symbol_renamer`
rewrote it from `target_symbol_map.json` — so every pair is an auditable map claim
about a specific address A. Two tests decide it, both relocation-**masked** with
relocation TARGET NAMES compared (never a raw body compare, which is silently
vacuous):

    T_F   retail bytes at addr(S)  ==  our compiled body for F
    T_S   retail bytes at addr(S)  ==  our compiled body for S

| | meaning | action |
|---|---|---|
| `T_F ∧ T_S` | two of OUR spellings both compile to retail's single body at A | alias, if it survives the gates |
| `T_F ∧ ¬T_S` | A looks like our F, our own S does not corroborate | map-repair **candidate** |
| `¬T_F ∧ T_S` | the map's name at A is corroborated by our OWN body for S, and we call F instead | **fix our source** |
| neither | — | undecidable, split by reason |

## ⛔ The gate this lane had to add: LITERAL relocations

`relocs_agree`'s placeholder tolerance is correct for its own question and **far too
permissive for asserting a NOVEL fold**. Retail spells a string literal
`lbl_82XXXXXX` (that address is absent from the map) while our side spells it
`??_C@...`, so the slot is **tolerated rather than compared** — and **two `Type()`
statics interning DIFFERENT string literals compare EQUAL**:

> `?Type@AddUserResultMsg@@SA?AVSymbol@@XZ` vs `?Type@SpeechEnableMsg@@SA?AVSymbol@@XZ`
> — 88 B, **nrel=17**, passed `relocs_agree`. They cannot be one body; each interns
> its own literal.

Measured contamination of the raw fold class: **17 of 23 pairs rested on a tolerated
slot, and 19 of 23 were VACUOUS** by `icf_alias_build`'s own floor (≥4 words, ≥50%
unmasked) — the documented "a 4-byte `blr` compares equal to everything" hazard,
which this lane's first cut simply did not apply. With vacuity + literal relocations
+ residency + retail uniqueness enforced, **23 → 4**.

★ `nrel == 0` is a **strength** (FATAL-1's scoping): with no relocation nothing is
masked, so byte identity is `/OPT:ICF`'s **complete** criterion. Three of the four
keepers have `nrel == 0`.

This is the same disease INCOMPLETE-1 recorded one turn earlier — *a tool's
tolerance is scoped to the question it was written for* — arriving from a different
direction. There it was retail-vs-retail; here it is asserting a fold where no group
exists yet.

### The keepers (4 pairs / +920 B measured)

All four are STL instantiations over **same-sized trivially-copyable types**, which
is precisely the class `/OPT:ICF` folds:

| survivor (retail) | folded (ours) | addr | B | nrel |
|---|---|---|---:|---:|
| `__uninitialized_fill_n<CubeFace*>` | `<Symbol*>` | `0x82495838` | 40 | 0 |
| `vector<Symbol>::erase` | `vector<TrackType>::erase` | `0x825b4a50` | 80 | 0 |
| `__unguarded_partition<GroupDrawDist>` | `<Filter@FilterViewSetting>` | `0x825d6020` | 136 | 2 literal |
| `__uninitialized_fill_n<CharacterEntry>` | `<Key<Quat>>` | `0x82667220` | 60 | 0 |

## Controls — both able to fail

| leg | decidable | `T_F` | fold-proven shape |
|---|---:|---:|---:|
| treatment (real FRESH pairs) | 1,169 | 123 (10.52%) | 26 (2.22%) |
| **decoy** (re-paired same charged sites; shape-matched) | 4,643 | 6 (**0.13%**) | **0 (0.00%)** |
| **positive** (1,519 installed groups) | 14,491 | 14,195 (**97.96%**) | 2,823 (19.48%) |

`T_F` is **81× enriched** over the decoy; the strict fold gate is **0 / 4,643** on
decoys and fires on **97.96%** of known folds. ★ The positive leg also measures the
strict gate's **recall against known folds at only 19.5%**, so **4 is an honest
FLOOR, not a ceiling** — a later lane with a stronger comparator may find more.

## ★★★ The real finding: 313 GENUINE WRONG-CALLEE SOURCE DEFECTS

The largest classified class is not a fold vein at all. **311 pairs
(+2 weak) are cases where the map's name at A is corroborated by OUR OWN body for S,
and our call site reaches F instead** — i.e. retail really calls S there and our
source calls the wrong function.

**Corroborated independently, and not by construction: 311 of 313 have our S graded
`fuzzy == 100` by objdiff.** That is a second code path (the grader) agreeing with
this lane's COFF comparator on the same claim, 311/313.

Sharpened by the strength of the *other* side:

| strength | pairs | meaning |
|---|---:|---|
| **AIRTIGHT** | **121** | BOTH S and F graded `fuzzy == 100`, bodies differ ⇒ definitively two different functions |
| F ungraded | 159 | F is a template instantiation with no separate row; differing template args are still different functions |
| F sub-100 | 33 | our F is unfinished, so `¬T_F` is partly explained by that — weakest |

⚠ The hole that made the AIRTIGHT split necessary: `¬T_F` can mean *"we call the
wrong function"* **or** *"we call the right function but our F is unfinished"*.
Requiring `fuzzy(F) == 100` closes it.

★ **This is exactly the bug class CLAUDE.md says the metric hides** — *"a caller that
indexes the wrong container type, or calls the wrong callee, scores a clean 100
before AND after the fix"*. The queue is dense with precisely that shape:

* `map<CRC,float>::operator[]` where we index `map<int,float>` — **8,212 B / 6 rows**
* `vector<int>::operator=` where we use `vector<float>::operator=` — 1,988 B / 7 rows
* `Target@BandCamShot::UpdateTarget` where we call `Target@HamCamShot::UpdateTarget`
  (DC3's class name survived the port) — 1,692 B
* `AccomplishmentCmp` where we use `AccomplishmentCategoryCmp` — 1,248 B / 7 rows
* `GameGem::ShowChordNums` where we call `GameGem::Loose`; `GameGem::Loose` where we
  call `GameGem::IsRealGuitar` — 1,200 + 1,068 B (same-signature bool getters)
* `hash_map<Symbol,int>` where we use `hash_map<int,UIComponent*>` — incl.
  `CustomizePanel`, the row RESIDUAL-1 flagged independently
* `GetParticipatingBandUsersInSession` where we call `GetParticipatingBandUsers`

**Queue: `docs/decomp/nogroup-wrong-callee-queue-NOGROUP1.tsv`** — 313 rows ranked by
**solo-closable bytes** (what closes if that pair *alone* is fixed): **40,608 B over
278 rows**, 224 pairs closing ≥1 row on their own.

⛔ **No alias may ever be installed against this class** — it is the wrong-callee case
an alias is most dangerous against, and forgiving it would convert a real bug into a
fabricated gain.

## Two classes that are weaker than their names suggest — corrected here

* **`MAP_REPAIR_CANDIDATE` (54) is NOT "wrong map name".** The lane's first cut called
  it that; measured, **46 of 51 have our S at `fuzzy < 100`**, so `¬T_S` is explained
  by *"our S is simply unfinished"* and carries **no information about the map**. It
  is a candidate needing per-row retail-byte adjudication, not a proven map error.
* **`MAP_CONTRADICTION` (5) is mostly comparator strictness, not map defects.** Two
  artifact hypotheses were tested and **both refuted** — the names occur exactly once
  in the target objs (not a `collect()` `setdefault` first-wins artifact), and only 1
  name tree-wide appears in >1 report unit with no disagreement (not a `report.json`
  key collision). The actual cause is that `relocs_agree(strict=True)` is
  **deliberately stricter than objdiff's `name_check`**, which *forgives* placeholder
  targets — so `T_S == False ∧ fuzzy == 100` is expected, not contradictory.
  ★ One member survives on independent evidence: `__make_heap<EventEntry,MaxSort>`
  and `sort_heap<EventEntry,MaxSort>` are pinned **inside `PrefabMgr.obj`**, where an
  `EventEntry` heap sort has no business being, and our spelling is
  `<PrefabChar, SortPrefabByPortraitFileName>` — a genuine wrong map name on spatial
  evidence.

## Measured

Predicted **+920 B / Δmatched 0**, pre-registered before the run; measured exactly.

```
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.008915pp  Δcode_bytes=+920
```

`ALIAS_SUSPECT` fired — expected and documented for a map-only patch (`none` flat,
`name_check` up); answered per group on retail bytes in the `evidence` field and by
the decoy control, **never** by the `none` control, which cannot clear an alias.

## Reusable findings

1. **A vein flagged by size is not a vein sized by prize.** 397,832 B of gross
   population contained **920 B** of aliasable value. The classification is what made
   that visible; the enumerating census could not distinguish the three actions.
2. **`relocs_agree`'s placeholder tolerance must not be used to assert a NOVEL fold.**
   Two `Type()` statics interning different literals compare equal through it. Require
   literal relocation agreement (or `nrel == 0`) when creating a group from scratch.
3. **Apply the anti-vacuity floor the tree already has.** 19 of 23 raw fold candidates
   were below it; the retail-uniqueness gate does **not** subsume vacuity, because a
   4-byte `b <target>` has a distinct signature per target and passes uniqueness
   trivially.
4. **The biggest class in a fold census can be "not folds at all".** 313 wrong-callee
   source defects were sitting in a population enumerated to look for aliases — and
   they are invisible to `matched_functions` by construction.
5. **Name a class for what it PROVES, not what it suggests.** `MAPNAME_WRONG` was
   renamed to `MAP_REPAIR_CANDIDATE` after measuring that 90% of its `¬T_S` evidence
   was just "our S is unfinished".

---

## ⛔ CORRECTION (lane WRONGCALL-3, 2026-08-14, on `e17ad55d`)

**The "313 GENUINE WRONG-CALLEE SOURCE DEFECTS" headline above does not survive
re-testing, and 54% of the queue's advertised bytes are NOT source defects.**
The census reproduces exactly (311 SOURCE_DEFECT + 2 weak on this tree), so this
is a re-reading of the same evidence, not a different population.

### The mechanism: `!T_F` is TRUE BY CONSTRUCTION for instantiation pairs

Both tests run through `relocs_agree`, which masks relocated fields but **compares
relocation TARGET NAMES**. Two template instantiations over layout-compatible types
emit identical code while calling *per-instantiation* helpers (`_Rb_tree<CRC,...>`
vs `_Rb_tree<int,...>`), so `T_F` is false **whether or not `/OPT:ICF` folded them**
— and a false `T_F` is precisely what routes a pair into `SOURCE_DEFECT`.

Re-tested on the **masked body alone** (`tools/wrongcall3_requalify.py`, which also
writes three columns into the queue TSV):

| re-test | pairs | queue `solo_closable_B` | share of bytes |
|---|---:|---:|---:|
| size differs ⇒ genuinely different functions | 173 | 13,000 | 32.0% |
| same size, masked body genuinely differs | 40 | 5,696 | 14.0% |
| **masked body BYTE-IDENTICAL** (differs only in reloc *names*) | **100** | **21,912** | **54.0%** |

The 100 belong in the census's own **`UNDECIDABLE_relocs`** category — *"bodies agree
modulo relocation but relocation TARGET NAMES disagree"* — which the census simply
never evaluated on the `T_S ∧ ¬T_F` branch.

**Proof case — the queue's #1 row, 8,212 B.** `Hmx::CRC` (`utl/CRC.h`) is a lone
`int mCRC` whose `operator<` is `mCRC < c.mCRC`, i.e. bit-identical to `less<int>`.
`map<CRC,float>` and `map<int,float>` **cannot differ in a single instruction**, and
measured they do not. `BandCamShot::Target::UpdateTarget` (1,692 B) and the
`BeatMatcher` pair (892 B) are fold-shaped too — three of the doc's own calibration
examples.

⛔⛔ **The actionable hazard is a "fix", not an alias.** The no-alias rule was already
stated; the mirror image was not. `SongData::mRangeShifts` is **correctly**
`map<int,float>` (`AddRangeShift(int,float)` indexes by `int`). Editing it to
`map<CRC,float>` to collect 8,212 B would **break working code to satisfy a fold**.

### The anti-vacuity floor was never applied to this branch

`nogroup_census.py` computes `vac = vacuous(rt) or vacuous(ob)` for every record but
consults it **only in the fold branch**, where it took that class 23 → 4. **45 of 313
(14.4%)** SOURCE_DEFECT rows have a retail body below the same floor (<16 B, or >50%
relocated), so their map-corroboration leg is worthless — `stb r4,0x40(r3); blr`
matches **any** class with a byte at `0x40`.
⚠ **This is a weakening, not a refutation**: vacuity manufactures false *equality*,
and the load-bearing evidence here is a *difference*.
Instances: the `GameGem` bool getters (12 B — a doc calibration example) and
`Interp`/`Nlerp` (**4 B**, the literal "`blr` compares equal to everything" case).

### Oracle screen: in the decidable subset the class runs ~6.5:1 the OTHER way

Asking whether the **oracle** sides with retail or with us at each call site
(21 of 311 decidable; the rest are templates / same-shortname / absent):

| | pairs |
|---|---:|
| ORACLE_AGREES_RETAIL ⇒ our source wrong | **2** |
| ORACLE_AGREES_US ⇒ the **map** is wrong | **13** |

⚠ Small and biased toward non-template pairs — **do not extrapolate to all 313**.
⛔ The screen's first cut was **vacuous in one direction**: its window began at the
victim's *definition line*, so any pair whose `ours` name equalled the victim's own
method name "agreed with us" for free (63 rows). Fixed with a self-match guard.

Worked instances of the ORACLE_AGREES_US direction, all **map** defects:
* `AppLabel::SetUserName` calls `UserName()`, not `GetTrackIcon()`.
* `AccomplishmentSongConditional::CheckStarsCondition` calls `GetBestStars`; our
  source and the oracle are identical, so the address the map calls `GetBestStreak`
  **is** `GetBestStars`.
* `0x8278eb78` is `GameGem::SetImportantStrings`, **not**
  `RndShaderMgr::SetAllowPerPixel` (an inline one-liner in `ShaderMgr.h`): it is a
  `stb r4,0x40(r3)` setter sitting inside a contiguous run of GameGem accessors,
  paired with the getter twin `fn_8278EB70` on the same byte, and its only caller is
  `GemManager::SetupRealGuitarImportantStrings`. **Not repaired here** — left charged
  rather than guessed at.

### Two claims of mine that FAILED, recorded so nobody re-runs them

* **`collect()`'s first-wins `setdefault` over a sorted glob** does pick a copy the
  grader never compares (`Rot.obj`'s `SpotlightEntry::Animate` inlines
  `Interp`→`Nlerp` because `Rot.cpp:147` defines the forwarder in-TU; the other six
  objs emit `bl Interp`). I predicted this was systematic. **Measured: 8 pairs,
  2.6%.** Small.
* **`Interp`/`Nlerp` looked like the best oracle-confirmed defect** (3 sites) and is
  worth **zero**: the queue prices it `solo_closable_B=0, rows=0`, and
  `world/LightPreset.obj` — the obj the grader actually reads — already spells
  `Interp`. Nothing to fix.

### Landed

One row, the only pair that survived every screen: `AccomplishmentPanel::
HasCorrectPlayerCount`'s unison block (`cfec935d`). **PREDICTED Δmatched +0,
Δcode_bytes +520, `none` FLAT — MEASURED exactly that**, units 250 → 250, 0 fell off.

★ **The honest re-statement of the class:** of 313, **173 (32.0% of bytes) keep the
wrong-callee reading on size grounds alone**, and those are where a future lane
should spend — but each still needs its **map** leg adjudicated on retail bytes,
because the decidable-subset screen says the map is the wrong side ~6.5:1.

---

## ⛔ CORRECTION 2 (lane WRONGCALL-4, 2026-08-14, on `1ad913dc`)

**Worked the 213 rows WRONGCALL-3 requalified (18,696 B). Landed +9,560 B —
51.1% of the whole requalified queue — from FIVE map rows and two source fixes.**
Both A/Bs hit their pre-registered numbers (one exactly), Δmatched +0 as
predicted, 0 units fell off, native gate PASS 18/18 / 0 SKIPs.

| row | B | which side was wrong | witness |
|---|---:|---|---|
| `Terminate@Movie` → **`MsToTick`** @ `0x827C90B8` | **+5,976** | MAP (+ a source co-defect) | TempoMap.s STORES `&gSimpleTempoMap` into `lbl_82C78F5C` ⇒ it is `TheTempoMap`; vtable `+0x8` is `TimeToTick` because `+0x4` is `SimpleTempoMap::TickToTime` (`f * mTempo`) 24 B later |
| GameGem 0x12 ladder, 5 rows shifted | **+2,676** | **BOTH — mirror images** | `IsRealGuitarChord` opens `lbz r11,0x12(r3); clrrwi. r11,r11,7` (keeps only 0x80) and our source opens it `if (!mRealGuitar)` |
| `SetAllowPerPixel@RndShaderMgr` → **`SetImportantStrings@GameGem`** @ `0x8278EB78` | **+904** | MAP | all 13 other `RndShaderMgr` methods cluster at `0x8246B5F0`-`0x8246BD70`, 3.3 MB away; `0x8278EB78` is `stb r4,0x40(r3)` right after a `lbz r3,0x40(r3)` getter twin |

### ★★★ The "map vs source" framing is itself wrong — the paying class is BOTH

WRONGCALL-3's decidable-subset screen ran 13 "map wrong" : 2 "source wrong" and
declined to extrapolate. On this larger, byte-adjudicated sample the *direction*
held (the map was implicated in 4 of 5 decidable rows) **but the binary framing
did not**: the two largest results were **COMPENSATING PAIRS in which BOTH sides
were wrong in mirror-image ways**, a third category the screen cannot express.

That category is where the bytes are, and it is **only collectable by fixing both
sides in ONE commit**:

* **GameGem**: the map named the 0x12 bit-getters one position EARLY; our header
  ended the 0x10 group with `mRealGuitar` instead of starting the 0x12 group with
  it. The errors cancelled, so all five accessor BODIES read fuzzy 100 while four
  CALL SITES were charged. Fixing either side alone REGRESSES.
* **`BandTrack::GetTrackIcon` / `UserName`** (84 B, `0x8234D6C8`/`0x8234D720`):
  the map has them **TRANSPOSED — proven on retail bytes**, not inferred.
  `lbl_82000C55` is a lone `0x00` (= `""`) and `lbl_82039E74` is `.string "G"`;
  our `UserName()` falls back to `MakeString("")` and `GetTrackIcon()` to
  `MakeString("G")`, so `0x8234D6C8` is `UserName`. **NOT REPAIRED, and do not
  repair it map-only**: both bodies are *already* fuzzy 100 because `name_check`
  FORGIVES the placeholder `lbl_` literal, and they match on the *vtable slot*
  (`0x84` vs `0x7c`) — i.e. our own vtable order mirrors the transposition. A
  map-only swap is **+84 on AppLabel, −168 on the two bodies = net −84**. The
  honest repair needs a vtable reorder in the parent, which no 84 B row justifies.

⇒ **Before repairing any map row, ask what our own side does with it.** The
compensating pair is not rare here; it is the modal shape of the big rows.

### The mechanism behind both big map defects is the same, and it is FIXABLE UPSTREAM

An unpinned or mis-pinned TU's functions fall inside a neighbour's over-broad
`.text` pin and inherit that neighbour's symbol names:

* **`system/utl/TimeConversion.cpp` is compiled but has NO `splits.txt` entry**, so
  its functions sit inside `StringTable.cpp`'s pin `0x827C8C88`-`0x827C9180`.
  `StringTable.cpp` COMDAT-scatter-includes `{GlitchFinder, Locale, Movie,
  TimeConversion}` — which is exactly why TimeConversion's addresses were handed
  **Movie's** symbols. `0x827C9110` is still mis-named `?Init@Movie@@SAXXZ`; it
  uses vtable `+0x4` (`TickToTime`) so it is **`TickToMs`**. Not repaired here:
  we do not define `TickToMs`, so renaming it alone LOSES the 24 B its false
  pairing currently earns.
* **`DepthBuffer3D.cpp`'s pin `0x8278EB78`-`0x8278EBA8` is WRONG** — that 48-byte
  range is a hole carved out of GameGem.cpp's accessor run and every function in
  it (`stb 0x40`, `lbz 0x30`, `lwz 0x38`, `lbz 0x31`) is a GameGem accessor.

⇒ **a pinning lane can drain this vein at the source rather than row by row.**

### Rows adjudicated and DELIBERATELY not taken

* **`SetVolume@MoggClip`** (148 B, `default/Sfx`) — **the map is RIGHT**, proven:
  `fn_8270D720` stores `0x40` = `mControllerVolume`, `fn_8270D748` stores `0x48`
  = `mVolume`. So **our source is the wrong side**, and so is the oracle — rb3-Wii's
  `SfxInst::UpdateVolume` also calls `SetControllerVolume`. This is a genuine
  retail-X360-vs-Wii-dev divergence. Left alone because `SetVolume` is *virtual*
  in our header, so a direct `bl` implies a call form (explicit qualification, or
  a non-virtual retail declaration) that I could not witness.
* **`GetType@AccomplishmentOneShot`** (452 B, the largest row left) — `fn_82654AB8`
  is `li r3,0x9; blr`, **8 bytes, below the anti-vacuity floor**. `li r3,N; blr`
  compares equal to every function returning N. Undecidable; fold-suspect ⇒
  FOLDPROVE-1's territory, not a source row.
* The **9 reciprocal pairs** in the queue were left untouched on principle — a
  reciprocal charge is the transposed-map signature, most of them are template
  instantiations (which cannot be transposed at a call site, because the type
  picks them), and a family permutation is what cost WRONGCALL-1 −1,248 B.
* `AccomplishmentCmp` (1,248 B) and `GetParticipatingBandUsersInSession` (520 B)
  are **already settled** — reverted by WRONGCALL-1 and landed by WRONGCALL-3
  respectively. The queue TSV is static and still lists both.

### Two side-findings, verified, for whoever owns GameGem layout

* **`mFrets` is at `0x32`, not `0x19`.** `GameGem.h`'s comment asserting "mFrets
  stays at 0x19" is WRONG. objdiff on `IsRealGuitarChord` shows
  `addi r8,r3,0x32` vs our `0x19`, and `GetFret` (`fn_8278EAC8`) is
  `add r11,r4,r3; lbz r3,0x32(r11)`. After this lane's bitfield fix that offset is
  the function's **only** remaining mismatch — closing it finishes a 92 B row.
* **Retail's `MsToBeat` (`fn_827C90D0`) has no `if (TheBeatMap && TheTempoMap)`
  guard either** — 17 straight-line instructions, no conditional branch. Same
  DC3-is-newer divergence as `MsToTick`, still unfixed.
