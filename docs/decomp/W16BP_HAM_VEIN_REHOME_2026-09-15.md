# W16-BP — the Ham vein: adjudicating 18 DC3 labels on retail RB3 bytes

**Branch** `w16-bp` · **base** `3f2fd225` · **date** 2026-09-15 · ruler `name_check`
(`functionRelocDiffs=name_check`, `ppc.calculatePoolRelocations=false`, read from
`report.json`'s own `provenance.diff_config` — not assumed).

## 0. The question

`config/45410914/splits.txt` carried **18 `Ham*.cpp:` headings** — Dance Central 3 labels
applied to Rock Band 3 code. The brief's premise was that retail `band.exe` contains **zero**
`.?AVHam*@@` RTTI.

**Verified literally rather than inherited** (Python, not `grep` — the shell `grep` shim routes
through `ugrep -I` and is binary-blind, yielding false negatives shaped like decisive ones):
**0** `.?AVHam*@@` type-name strings, **44** `.?AVBand*@@`. The premise holds.

⚠ **But state the caveat the premise does not carry:** RTTI absence disproves only a
*polymorphic* class of that name. `/GR` emits `??_R0` for polymorphic types; a non-polymorphic
struct, a template instantiation, or a free function can be named `Ham*` and leave no RTTI.
So "no `.?AVHamX@@`" licenses "no polymorphic class `HamX`", never "no symbol mentioning Ham".

**Why do Ham-named rows match at all, then?** Because the DC3 `hamobj/*.cpp` sources are
genuinely compiled in this tree, and the rows that match are generic engine/STL COMDATs whose
*bodies* depend on layout, not on the name in the label.

## 1. Result

| | matched_functions | matched_code | matched_code_percent | total_functions | total_code |
|---|---:|---:|---:|---:|---:|
| **before** (`3f2fd225`) | 43,523 | 4,039,508 | 39.421112 | 69,240 | 10,247,068 |
| **after** (`212f4c91`) | 43,530 | 4,039,728 | 39.423256 | 69,240 | 10,247,068 |
| **Δ** | **+7** | **+220 B** | **+0.002144** | 0 | **0** |

`total_code` Δ0 across all five commits — every edit was a **re-home**, never a new pin.

⛔ **Do not book the +220 B as source progress.** 6 of the +7 functions are provably
disclosure-class: at commit D `masked_equal_functions` rose 23,046 → 23,052, **+6, exactly
matching Δmatched_functions**, so the honest floor (`matched − masked_equal`) is
**20,478 → 20,478, exactly flat**. This lane bought **attribution accuracy**, not matched code.

Headings: **18 → 14**. Four retired.

## 2. The instrument, and the two legs a re-home needs

objdiff pairs a target row **by name** against the base obj of the unit its address is
**pinned** to. So a re-home is only safe when **both** legs hold:

1. **GEOMETRY** — the block is interior to one destination heading's address run (the **same**
   heading immediately before *and* after it), not merely abutting a boundary.
2. **PAIRING** — that destination's compiled base obj **DEFines** every named row in the block.

### 2.1 Geometry alone is not enough — caught live, twice

CLAUDE.md prices "enclosed by the same heading on both sides ⇒ membership" at **66.24%
precision (33.76% FP)**. This lane hit that rate in practice:

- `HamNavList 0x824EA220-0x824EA298` is a **perfect** hole in `Instance.cpp` (gap 0 both sides)
  carrying **120 matched bytes** — but its spelling is `?Load@WorldInstance@@UAAXAAVBinStream@@@Z`
  and `Instance.obj` defines only `?PreLoad@WorldInstance@@`. Geometry alone would have moved it
  and **destroyed 120 B**.
- `HamMaster 0x8276E328-0x8276E3D8`, a perfect hole in `BeatMaster.cpp` with **176 matched B**,
  whose `??0HamMasterLoader@@QAA@PAVHamMaster@@@Z` `BeatMaster.obj` does not define.

Both FILED.

### 2.2 …and the PAIRING leg is VACUOUS for generic template COMDATs

The DEF test was originally applied per-candidate against a strictly-adjacent neighbour. That is
wrong twice over. Scanning **all 1,047 declared base objs** for every named row in a Ham unit
gives the real distribution:

| class | rows | meaning |
|---|---:|---|
| **HAM_ONLY** | 57 | defined **only** by a Ham obj — **no destination exists** |
| **SPECIFIC** (≤2 non-Ham objs) | 34 | DEF and geometry must agree on the same destination |
| **NARROW** (3–10) | 14 | |
| **GENERIC** (>10) | 19 | up to **456** objs — STL / `Hmx::Object` template COMDATs |

`?_M_increment@?$_Rb_global@_N@stlpmtx_std@@` is defined in **394 objs**. "PropKeys.obj defines
it" therefore carries ≈0.4 bits and is **not evidence**. Several candidates from the first pass
rested on exactly that and were **not** moved.

### 2.3 A third limit: DEF does not apply to anonymous rows at all

Anonymous rows (`fn_*`) pair by **byte signature** against their unit's funclet pool, never by
name. So for an all-anonymous block the DEF leg is inapplicable *by construction*, and the only
honest bound is **how many matched bytes the block currently holds** — if that is 0, the
downside is 0 and the move is a free bet.

### 2.4 The strongest evidence class is neither leg

For EH funclets, retail's **own `.xdata` unwind record names the parent function**. That is not
inference from geometry or from a symbol table — it is the binary stating the answer.
`tools/funclet_homing.py` reads it.

## 3. What landed

| # | sha | what | pre-registered | **measured** |
|---|---|---|---|---|
| A | `66125eb6` | §10.1 known-answer Label fixture: `HamLabel.cpp:` entry deleted, block to `BandLabel.cpp:`, 2 map rows renamed to `BandLabelCountDoneMsg` | Δ0 | **Δ0 exactly** |
| B | `8e1d879b` | `HamSongData` → `SyncStore`, `HamScrollSpeedIndicator` → `ReviewDisplay`; both entries drained + deleted | — | **+1 fn / +40 B** |
| C | `76c0be37` | 8 blocks → `CharEyes` / `BandList` / `CrowdAudio`, both legs non-vacuous | Δcode ∈ [0,+424] | **Δ0**; 13 rows / 1,064 B in == 13 rows / 1,064 B out |
| D | `f608d0c5` | 15 true-hole blocks passing the mechanical test; `HamCamShot` drained | Δcode 0, band [−120,+120] | **+6 fn / +180 B** — *above band* |
| E | `212f4c91` | 5 `.xdata`-named funclets + 2 Gesture factory rows | Δcode [−100,+340], central +100…+200 | **Δ0** — *at the lower bound* |

### 3.1 Commit A — the fixture and why it was safe

`src/system/bandobj/BandLabel.cpp:141-144` carries the **"sw2 scatter-include"** convention
(252 sites tree-wide): `BandLabel.cpp` `#include`s `hamobj/HamLabel.cpp`, so `BandLabel.obj`
emits both COMDATs. Verified on bytes, not assumed: both ctor COMDATs are byte-identical
(sha256 `e4b89581ab3bbc09`, 252 B each), their relocation lists differing **only** in the
Ham/Band spelling of `?Type@…CountDoneMsg@@` and `??_7…CountDoneMsg@@6B@`. That settled the
caller/callee coupling **before** editing, which is why the splits move and both map renames
had to land in **one commit** — a map rename alone turns a 100 into a 0.

`scripts/symbol_aliases.json` was **ruled out** as the bridge and **not edited** (lane bar).

### 3.2 Commit D — a failed prediction, and what it was actually worth

Predicted [−120,+120]; **measured +180**. The set-diff attributes it exactly: the 3 net-new rows
are `fn_822AB62C` (68) + `fn_822AB670` (68) + `fn_822AB6E0` (44) = **exactly 180 B**, all
anonymous funclets in `Gem`. The 1,120 B block `0x822AB3D8` carries 12 anon rows; moving it into
Gem's **larger** pool let three find byte-equal counterparts HamCamTransform's smaller pool did
not contain. I priced the *risk* from changed pools and not the symmetric *gain*.

And `masked_equal_functions` rose by exactly +6 — so **100% of it is disclosure**, honest floor
flat. Same mechanism lane W16-BM measured running the other way.

### 3.3 Commit E — the bound was the answer, not the middle

The two Gesture rows are **still fuzzy 0** in their new homes. Downside bound (0) held exactly;
upside was empty. Their bodies do not match `CrowdAudio`'s or `DataUtl`'s pool any better.

## 4. The negative result that matters most

**Re-homing recovers PAIRING; it never recovers CORRECTNESS.**

Commit C moved 1,488 block bytes of which 1,064 were already matched — **424 B of headroom**.
None of it converted. `0x82387AA0-0x82387C50` is 432 B matched at **40**, and re-pairing it
against `CharEyes.obj` — which defines its spelling — left it at 40. That shortfall is genuine
source divergence and **no pin move can touch it**. Anyone pricing a pin-move lane off
"unmatched bytes inside mislabelled headings" is pricing a number that does not convert.

## 5. Disposition of all 18 headings

### RE-HOMED / DELETED (4)

| heading | disposition | sha |
|---|---|---|
| `HamLabel.cpp` | entry DELETED; `.text` → `BandLabel.cpp` + 2 map rows renamed | `66125eb6` (Δ0) |
| `HamSongData.cpp` | entry DELETED; 2 blocks → `SyncStore.cpp` | `8e1d879b` (+1/+40 B) |
| `HamScrollSpeedIndicator.cpp` | entry DELETED; 4 blocks → `system/bandobj/ReviewDisplay.cpp` | `8e1d879b` |
| `HamCamShot.cpp` | entry DELETED; `0x826521E0-0x82652288` → `Matchmaker.cpp` | `f608d0c5` |

### PARTIALLY RE-HOMED, remainder FILED (6)

| heading | moved | remainder FILED because |
|---|---|---|
| `HamCamTransform.cpp` | 5 blocks → `CharEyes` (C); 6 blocks → `PatchDir`/`Gem`/`CameraShot` (D) | 22 rows are `HAM_ONLY` over `TransformArea`/`TransformCrowd` — **types absent from retail RTTI**; the spatial neighbours (`Gem.cpp` 38 adjacencies, `OutfitConfig.cpp` 14) do not define them |
| `HamNavList.cpp` | 2 blocks → `BandList` (C) | `?Load@WorldInstance@@` (§2.1), `ObjDirPtr<HamListRibbon>` — HAM_ONLY |
| `HamRibbon.cpp` | 1 block → `CrowdAudio` (C) | `?DeleteAll@ObjPtrList<RndTransformable>` undefined in bounding `CharBoneDir.obj`; `Key<Transform>` rows undefined in `SongParser.obj` |
| `HamMove.cpp` | 4 blocks → `VocalTrackDir`/`MetaPanel`/`Performer` (D) | `LocalizedName@HamMove@@` rows HAM_ONLY; the rest are GENERIC COMDATs where DEF is vacuous (§2.2) |
| `HamIKEffector.cpp` | 3 blocks → `BandIKEffector`/`FileMerger`/`CharForeTwist` (D) | `Constraint@HamIKEffector@@` HAM_ONLY |
| `HamListRibbon.cpp` | 1 block → `MeterDisplay` (D) | `HamListRibbonDrawState` HAM_ONLY |

### FILED — NEEDS_SOURCE, untouched (8)

| heading | evidence |
|---|---|
| `HamNavProvider.cpp` | `NavItem@HamNavProvider@@` HAM_ONLY; `0x8230C200` is a perfect hole in `SongSectionController` but that obj does not define it |
| `HamMaster.cpp` | `??0HamMasterLoader@@QAA@PAVHamMaster@@@Z` HAM_ONLY; perfect hole in `BeatMaster`, 176 matched B at risk (§2.1) |
| `HamIKSkeleton.cpp` | `ObjPtr<HamCharacter>` — bounding `BandIKEffector.obj` defines neither it nor `ObjRefConcrete<BandIKEffector>` |
| `HamDirector.cpp` | `??_GOfflineCallback@@UAAPAXI@Z` HAM_ONLY; perfect hole in `BandDirector`, 68 matched B at risk |
| `HamBattleData.cpp` | sole row's only non-Ham definer is `ByteGrinder`, which is **not** a spatial neighbour — DEF and geometry disagree |
| `HamSupereasyData.cpp` | 4 rows, **0 matched**, all anonymous; no named spelling to adjudicate |
| `HamPhotoDisplay.cpp` | 1 row, 0 matched, anonymous |
| `system/hamobj/MiniLeaderboardDisplay.cpp` | ⚠ **see below — do NOT re-home** |

### ⚠ `MiniLeaderboardDisplay` — a correction to this lane's own first reading

`.?AVMiniLeaderboardDisplay@@` **IS present** in retail. It is a **genuine RB3 class**; only the
`system/hamobj/` *directory* is a DC3 artifact. All 24 of its named rows classify `HAM_ONLY`
**because the only obj defining them is our own `system/hamobj/MiniLeaderboardDisplay.obj`** —
i.e. the heading is perfectly self-consistent and already carries 28/31 rows matched (1,912 B).

⇒ **Re-homing it would be destructive.** The `HAM_ONLY` label is a property of the *directory
name*, not of the code. A later lane reading the class label without this note would move it and
lose 1,912 B. Renaming the path is cosmetic and was deliberately **not** done — it would
re-home every one of those rows for zero gain.

## 6. Not done, and why

- **`0x823F4A30`** (`UI.cpp` → `CharTaskMgr.cpp`), the 6th mis-pinned funclet — **lane BN's
  surface**. `funclet_homing.py` skipped it under an explicit bar; reported, untouched.
- **`mutate_assertions.py` registration — SKIPPED** under the brief's own escape clause. It runs
  inside the 2-minute bar but **not cleanly**: **6/9** cases, two hard `IndexError`s, one case
  that "DID NOT TRIP" — and it still exits **rc=0**. Registering it would add a gate that
  **cannot fail**, the exact false-green class this project has repeatedly been burned by. It
  also hardcodes `/home/free/tmp/wt-w16-bm`, another lane's worktree that `prune_worktrees.py`
  is entitled to delete. Fix both before registering.
- **`scripts/symbol_aliases.json`** — lane bar, not edited.

## 7. What would change these verdicts

Every FILED heading is blocked on the **same single fact**: no compiled obj in the tree defines
the spelling, so no destination can pair it. Each would be re-homable if **either**:

1. **The source arrives** — an RB3 `.cpp` (or a `sw2` scatter-include, the mechanism that made
   the `HamLabel` fixture work) causes some obj to emit that COMDAT. Then the DEF leg is
   satisfiable and the geometry is already recorded above.
2. **The map name is wrong and is corrected** — if a row's `target_symbol_map.json` spelling is
   a DC3 fabrication over a body that is really some RB3 function, renaming it makes it pair.
   ⚠ Adjudicate on **retail bytes** (does the callee's signature match the call site?), never on
   a tool's `AT_LIMIT` label, and remember that proving a name wrong does **not** make renaming
   safe — the destination obj must define the new spelling or the row sits at 0% permanently.

For `MiniLeaderboardDisplay` specifically, nothing would change the verdict: it is already
correct. The only open item is the cosmetic directory name, and moving it **costs** bytes.

## 8. Gates

`funclet_homing.py --validate`: **PASS** (`MIS-PINNED 6 → 1`, the survivor being lane BN's).
§7.4 topology census: **PASS — 0 defects** over 14 Ham-labelled units; every matched row in
every remaining Ham unit is defined by its own base obj.
