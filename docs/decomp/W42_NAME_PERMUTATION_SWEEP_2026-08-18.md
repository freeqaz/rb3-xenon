# W42 — the name-permutation family sweep, and the proof that the vein is nearly drained

**Lane W42-PERMSWEEP, 2026-08-18. Base `grounded2-restoration` @ `05445c10`.**
Commits: `11f44721` (detector), `8299da6e` (GemPlayer/SongDB, **+96 B**),
`f5dc461b` (AccomplishmentSongConditional, **+256 B**).

The lane was funded to **systematise** the campaign's single most productive
lever. Six name-permutation families had been found, **every one of them by
accident**, while a lane was doing something else. Nobody had ever swept for
them. This is the sweep, its two landed fixes, and — the more useful result —
**a measured bound proving the class is nearly exhausted.**

---

## 1. The defect class, and why charged rows cannot find it

★★★★★ **A uniformly wrong name FAMILY is invisible to `name_check`, because
every member is wrong *consistently*, so nothing that references them can
disagree.**

So the sweep does **not** look at charged rows. It looks for **structural
inconsistency between our source's call graph and the map's**:

```
our obj says   OurClass::X  calls  A
the map says   retail's X   calls  B          (A != B)
```

Our side is taken **mechanically** from each compiled obj's outgoing
`IMAGE_REL_PPC_REL24` relocations — those relocations *are* our source's call
graph. No source-text parsing, no heuristics. Retail's side is its raw branch
words resolved through `scripts/target_symbol_map.json`.

★ **THE SOURCE IS THE ARBITER, NOT NAME EQUALITY.** W31's `fwdscan.py` flagged
`Outer::X -> Inner::Y` whenever the method names differed; W34 proved that
screen wrong (`Game::OnPlayerRemoved -> TrackerManager::HandleRemovePlayer` is
exactly what `Game.cpp:1138` says). Legitimate delegation with differing names
is **silent** here, because our source and the map agree.

### ★★★★★ The detector fires only at the BOUNDARY of a wrong region

This is the lane's central insight and it is a **direct corollary** of the
invisibility above. If a whole region is wrong *consistently*, its internal
edges agree and nothing fires. A hit appears **only where a wrong region meets
a correct one.**

Demonstrated, not asserted: `GemPlayer::GetBaseBonusPoints` called SongDB
`0x82684ed8`, which was *itself* misnamed `GetBaseBonusPoints`. Self-consistently
wrong ⇒ **never charged**. Both GemPlayer rows scored **`fuzzy` 100.0 before and
after** carrying the wrong name. Two of that fix's four corrections were worth
**exactly 0 bytes** and were landed anyway.

⇒ **Family size is systematically UNDER-counted by this instrument.** A census
of hits is a census of *boundaries*, not of wrong names.

---

## 2. Fold adjudication — 1,050 raw hits, 96% explained without a defect

Raw structural inconsistency is dominated by ICF. Three stages, in order:

1. **`FOLD_ALIAS`** — an existing `symbol_aliases.json` group already unions the
   two spellings (union-find over the group file).
2. **`TEMPLATE_ARGS_DIFFER`** — same method, same template head, different args.
   ⛔ **This IS what a fold looks like**, and it is the stage that mattered
   most: per-`T` node allocators fold, so folding is **transitive** and a
   one-level byte test cannot see it. 8 distinct callers all branching to one
   address (`0x823d14c0`) survived every other screen.
3. **`FOLD_BYTES`** — our two COMDATs are byte-identical with relocations masked
   **and reloc TARGET NAMES compared**. ⛔ Raw `memcmp` is *silently vacuous*
   here: PC-relative `bl` displacements differ at different addresses, so
   identical functions are not identical bytes.

| verdict | rows | bytes | meaning |
|---|---:|---:|---|
| `FOLD_ALIAS` | 434 | 26,832 | proven fold (existing alias group) |
| `UNKNOWN` | 392 | 9,652 | **our callee has no retail address** — not adjudicable either way |
| `TEMPLATE_ARGS_DIFFER` | 116 | 10,596 | fold shape |
| `OURS_UNMAPPED` | 56 | 1,552 | our symbol has no unique retail address |
| **`DUAL_MAPPED`** | **49** | **992** | **live candidates** |
| `FOLD_BYTES` | 3 | 172 | proven fold (byte identity) |
| **TOTAL** | **1,050** | **49,796** | |

⚠ `UNKNOWN` (392 rows) is an **identification-coverage** class, not a defect
class — the same disease as the refuted "callee absent from map ⇒ fold-alias"
model. It is what the sweep **cannot see**, and it is stated here so the bound
below is read with its blind spot attached.

---

## 3. What landed

### 3a. `GemPlayer` + `SongDB` — `GetBaseMaxPoints` / `GetBaseBonusPoints` transposed (`8299da6e`, **+96 B**)

Four addresses, two classes, one transposition. Adjudicated on four independent
instruments: `MultiplayerAnalyzer` struct offsets (`mMaxPts // 0x1c` vs
`mBonusPts // 0x20`), the tail-call chain with **our source as arbiter**, MSVC
**source-definition order**, and a `VocalPlayer` control **itself proven on
retail bytes**. Two of the four corrections paid 0 bytes (see §1).

### 3b. `AccomplishmentSongConditional` (`f5dc461b`, **+256 B**)

The 12 condition bodies at `0x82669898–0x82669d98` are **byte-shape-identical**
— same prologue, same `GetSongIDFromShortName`, and exactly **one**
distinguishing `bl` to a `SongStatusMgr` accessor. That single call is the whole
information content.

| retail body | accessor | map said | landed |
|---|---|---|---|
| `0x82669898` | `GetBestStars` | **unnamed** | `CheckStarsCondition` |
| `0x82669918` | `GetScore` | `CheckScoreCondition` | ✅ **control** |
| `0x82669998` | `GetBestAccuracy` | `CheckAccuracyCondition` | ✅ **control** |
| `0x82669a18` | `GetBestStreak` | ✗ `CheckStars` | `CheckStreakCondition` |
| `0x82669a98` | `GetBestAwesomes` | ✗ `CheckStreak` | `CheckAwesomesCondition` |
| `0x82669b18` | `GetBestDoubleAwesomes` | **unnamed** | `CheckDoubleAwesomesCondition` |

The accessor is a **bijection** over the 7 distinct-accessor bodies: each is
called by exactly one of our 14 `Check*Condition` methods and appears in exactly
one retail body. **The assignment is forced, not chosen.**

⚠ **I first called this a block shift. It is NOT** — `Score` and `Accuracy` were
already right and are the in-family control, which rules out a systematic
mis-derivation of the block. Counts reconcile exactly: 7 distinct-accessor + 5
`GetBestSongStatusFlag` bodies = 12, and our two extra methods
(`HoposPercent`/`SoloPercent`) call accessors appearing **nowhere** in retail.

Independent size corroboration: our `Stars/Score/Accuracy/Streak` compile to
**128/124/128/128 B** against retail's **128/124/128/128** at the addresses
assigned.

---

## 4. ★★★★ A failed prediction worth more than the bytes

I pre-registered that **`none` would MOVE** on the AccomplishmentSongConditional
patch, because it changes pairing. **It was FLAT**, and `ab_measure` correctly
fired `ALIAS_SUSPECT`.

The mechanism is worth keeping. In leg A our `CheckStarsCondition` body was homed
on retail's **Streak** body and still scored **`fuzzy` 99.8438 / `mpn` 100** —
because the family is uniformly shaped, the *only* difference was the `bl` callee
**name**. `none` ignores relocation names, so those rows were **already 100 under
`none`**; a rename cannot move it. **Flatness is forced by the mechanism, not
evidence of fabrication.**

★ That `99.8438` is the lane's thesis in one number: **a mis-homed member of a
uniform family is 0.16 pp away from invisible.**

`ALIAS_SUSPECT` resolved on three grounds, none of them assertion:
1. **No alias was added** — a map-only edit; the forgiveness mechanism
   (`symbol_aliases.json`) was never touched.
2. The +256 B is two rows reaching **exactly 100.0000**, which a fabricated
   assignment cannot produce — it would leave residual charges.
3. The assignment is independently forced by the accessor bijection.

⚠ **`ALIAS_SUSPECT` fires on every map-only patch by construction.** It is a
prompt to adjudicate, never a verdict.

---

## 5. ⛔ The economics are not a constant — a third ratio

Measured, per channel, by row-level diff of the two archived reports:

| channel | measured |
|---|---|
| **renames** (`a18`, `a98`) | **+256 B** — 2 rows, 99.8438 → **100.0000** |
| **additions** (`898`, `b18`) | **+0 B** — paired for the first time, at **58.75%** |
| **cascade** (`CheckConditionsForSong`, 1,188 B, fan-in 1) | **+0 B** — 99.9495 → 99.9663, **did not cross** |

⇒ this edit splits **100% pairing / 0% cascade** — a third distinct ratio after
the 98.9/0.6 and 19.5/80.5 already on record. **Do not price a map edit off any
prior figure.**

★ The additions behaved **exactly as the standing rule predicts**: objdiff
**forgives placeholder targets**, so naming an anonymous address has zero
call-site upside. It paid in **bug exposure** instead.

### The bug the additions exposed

The two newly checked rows pair at **58.75%**, and the cause is a real source
divergence that was invisible while they were unpaired:

| our body | ours | retail |
|---|---:|---:|
| `CheckAwesomesCondition` | **156 B** | 128 B |
| `CheckDoubleAwesomesCondition` | **156 B** | 128 B |
| `CheckTripleAwesomesCondition` | **156 B** | 128 B |
| `CheckHoposPercentCondition` | 156 B | *no retail body* |
| `CheckSoloPercentCondition` | 156 B | *no retail body* |

The 28 B excess is a guard our port carries and retail does not:

```cpp
if (cond.mScoreType != kScoreVocals && cond.mScoreType != kScoreHarmony) {
    MILO_WARN("awesome condition can only be used with vocals or harmony!");
    return false;
}
```

`MILO_WARN` is a no-op in the match build, but the `if` is **unconditional** in
our source, so the compare/branch/early-return survives. Retail's bodies are the
same 128 B as the unguarded `Stars`/`Streak` ⇒ **retail has no such guard.**
Removing it is a **priced source lever worth up to 3 × 128 B**. Not done here:
it touches `src/**`, needs the native gate, and this lane's change budget was
the map edit.

---

## 6. ★★★★★ The exhaustion bound — the vein is nearly drained

**The entire remaining live candidate class is 49 rows / 992 B**, which is
**0.0096% of `total_code`**. That is the ceiling if *every* remaining candidate
were a real defect **and** every one crossed.

| stratum | rows | bytes | fan-in | adjudicable by fan-in? |
|---|---:|---:|---:|---|
| vtable `this`-adjustor thunks (`$4`/`$R4`) | 22 | 392 | **0** | ⛔ **no** |
| stl/template instantiations | 15 | 280 | 28 | fold-dominated |
| ordinary named methods | 12 | 320 | 20 | yes |

Size histogram: **16 B ×26, 8 B ×15**, 24 B ×3, 84 B ×2, 32/56/128 B ×1.
Median **16 B**. **Only 4 rows are ≥56 B** — i.e. large enough to carry logic:

| addr | size | fan-in | our callee | map's callee | read |
|---|---:|---:|---|---|---|
| `0x822dc9d0` | 128 | 2 | `MemOrPoolFreeSTL` | `Object::New<RndMesh>` | almost certainly a fold artifact (allocator stratum) |
| `0x825c5590` | 84 | 1 | `BandTrack::UserName` | `BandTrack::GetTrackIcon` | **best remaining candidate** — same class, same signature `QBAPBDXZ` |
| `0x8282a258` | 84 | 1 | `exception::_Copy_str` | `exception::operator=` | CRT/STLport, fold-suspect |
| `0x827706e0` | 56 | 0 | `GameGemDB::MergeChordGems` | `GameGemDB::Finalize` | real candidate, but **fan-in 0** |

⚠ **45% of the remaining class (22 rows) has fan-in 0 by CONSTRUCTION** — a
vtable adjustor thunk is reached through the **vtable**, not by `bl`, so the
lane's strongest instrument (caller semantics) is **structurally unavailable**
for every one of them. They are not un-adjudicable, but they need a *different*
instrument: **vtable slot identity** (`scripts/dump_vtable.py`, `/vtable`), not
call-site fan-in. Anyone reopening this should start there and nowhere else.

### The verdict, in numbers

- Landed this lane: **+352 B** from 2 families (8 addresses).
- Remaining ceiling across **all 44 remaining families**: **992 B**, of which
  ~672 B sits in the thunk + stl strata that are fold-dominated or fan-in-0.
- Expected realisable remainder: **~150 B**, concentrated in 1–2 rows.

⇒ **The vein is not empty, but it is drained as a byte lever.** The sweep's
lasting value is the **detector** and the **boundary insight**, not the bytes.
★ And per §1 the hit census under-counts wrong names systematically, so
"992 B of candidates" is a bound on what *this instrument can see*, never a
bound on how many map names are wrong.

---

## 7. Reusing it

```bash
python3 tools/w42_family_sweep.py --fanin --json out.json   # census
python3 tools/w42_verify.py 0x826cda68 0x826cdad8 ...       # adjudicate a family
```

⛔⛔ **BUILD THE WORKTREE FIRST.** A fresh worktree's reflinked target objs are
**pre-renamer**, so every retail mangled name reads "ABSENT" — silently, and
**the failure agrees with your prior**. `w42_verify` asserts a non-zero
DEFINED-symbol count (164,799 here) before trusting any negative, for exactly
this reason.

Before firing any rename, all four checks, every one of which has independently
changed a lane's patch:

1. **Own `.pdata` entry?** A phantom row (dtk mis-carve) is indistinguishable
   from an unidentified one by every name-keyed instrument.
2. **Does the obj DEFINE the name** (not merely reference it)? Proving a name
   wrong does **not** make renaming it safe — a name the base obj cannot define
   strands the row at a permanent 0%.
3. **Injectivity preserved?** Compare against the baseline's pre-existing
   duplicates; require zero **new** ones.
4. **Are the addresses inside the right unit's `.text` pins?**

⚠ And edit `scripts/target_symbol_map.json` **surgically, by line**. It is
1-space-indented and **unsorted**; a `json.dump` re-write touches all ~30k lines.
