# W16-PF: the IR-temporary-count lever applied to the register-only bucket (2026-10-06)

**Branch** `w16-pf`, off main `6f1d1d794`, rebased onto main `e095c7ab7` before measuring. **Ruler** `name_check`
(graded, from `report.json` `provenance.diff_config`). **The permuter was not run.** No map, splits, alias or
`symbols.txt` edits, and nothing under `src/network/` or `src/xdk/`.

This lane follows W16-OY (`W16OY_HOISTED_ADDRESS_NUMBERING_2026-10-03.md`). OY found that hoisted-address register
numbering follows the function's count of IR temporaries, and that the count can be moved by spellings which do not
change the code otherwise. OY's §6 left open whether the same lever reaches the other register-numbering rows. This
lane applied it to the in-scope register-only bucket from `CAMPAIGN_STATE_2026-10-03b.md`: rings `IN-*`, key class
P1/P2, rows earlier lanes had opened by hand. That is **114 rows / 78,372 B**, taken largest first.

## 1. Result

**13 rows to fuzzy 100, 3 partials, 98 recorded as stopped. Measured +0 functions / +12,260 B, exactly as predicted, with 0 rows down** (§4).

The rows that moved fall into three of the kinds of temporary OY described, plus a fourth OY had listed as counting
but never used as a fix:

| kind | change | fixed | partial |
|---|---|---:|---:|
| bind a value to a named local (adds) | `bool inSong = …`, `const char *str = …`, `AccomplishmentProgress &progress = …`, `TrackData *data = …`, `float bonus = …`, `float x = q1.x`; partials `bool shown`, `CacheID *id`, `int users`, `float v1_y` | 6 | 3 |
| read through an existing inline accessor instead of the field (adds) | `gem.GetMs()`, `mGameGem.GetMs()`, `InstrumentType()` | 3 | |
| drop a named local (removes) | `strtablesize`, `keystr`, RockCentral's fourth iterator `lbit` | 3 | |
| post-increment instead of pre-increment (an iterator copy, adds) | `++it` → `it++` | 1 | |

Two things are new relative to OY.

1. **The lever reaches commutative operand order, not only hoisted-`lis` numbering.** Nearly every fixed row's
   charge was a commutative operand pair that read swapped:
   - `add`/`lwzx`/`stwx` base,index, mostly DataArray `Node(i)` address adds, where retail has (offset, base);
   - `fadds`/`fmuls`/`fmadds` operands.

   W16-NA had found a bare source swap of the operands inert. That still holds here: re-tested on 4 rows
   (`Performer::AddPoints`, `VocalTrack::RebuildHUD`, `Game::OnMsg`, `FastInterp`), inert on every one. What moves
   the order is the function-wide temporary count, sometimes from a site far from the swapped instruction. In
   `MetaPerformer::SelectRandomVenue`, an identity probe at the early `curArr->Sym(0)` moves an add in the later
   `validVenues` loop (instruction 438).
2. **Member reads count, not only call results.** OY's probe wrapped tested call results. Wrapping a plain member
   read (`gem.mMs`, `mVibratoFrameBonus`, `q1.x`) in the identity probe found 5 of the 13 fixes and all 3 partials.
   On 4 of those 5 rows (`GemPlayer::Hit`, `Singer::SetAssignedPart`, `FastInterp`, `Gem::PartialHit`), neither the
   call-result pass nor the drop pass had found any site that moved.

## 2. Method

**Probe.** `~/tmp/w16pf/probe.py` (scratch, not committed) does the following:
- writes a temporary copy of the TU, never the real file;
- compiles it with the TU's exact `ninja -t commands` command line, with the objcache prefix stripped and a scratch
  `/Fo`;
- diffs the result against the target object with `objdiff-cli diff`. It passes the four `objdiff.json` option pins
  and `--map-file build/45410914/icf_aliases.map`, so that ICF alias forgiveness matches the grader.

**Control.** Before any edit, 113 of 114 rows reproduced `report.json`'s `fuzzy_match_percent` exactly. The exception
is `VocalTrackDir::PostLoad`, which reads 99.322 against 99.338 and was treated as unreliable. The first version of
the probe left out the ICF map and read `SaveLoadManager::SetState` at 99.829 against the report's 99.893. The
control is what caught that.

**Identity probe.** The probe wraps one expression in `template <class T> inline T _pfid(T x) { return x; }`. This
produces no code of its own and adds one IR temporary. It is only a probe and is never landed. A site that moves a
row tells you which direction the row needs and roughly where. The landed fix is then a natural spelling of the same
kind at or near that site.

**Sweeps.** Four automatic passes ran, each one site at a time, over every row the locator could place. A site was
flagged BETTER only if the instruction count was the same and there were fewer mismatches.

| pass | what it varies | rows | sites | BETTER sites |
|---|---|---:|---:|---:|
| `autoid` | wrap each tested call result and each assigned call result | 103 | 753 | 17 |
| `autodrop` | drop each single-use named local, inlining its initialiser | 102 | 165 | 2 |
| `autoid_mem` | wrap each member / subscript read | 103 | 1,214 | 14 |
| `autopost` | flip each statement-level `++x` / `x++` | 103 | 156 | 1 (rejected, below) |

Nine rows could not be located by name for the sweeps (§3.3 lists them).

Each BETTER site was then followed up by hand with natural spellings, using `batch.py` (8 variants per compile
wave). Rows with no BETTER site in any sweep were probed by hand where the residue was 1–4 instructions. In total
about 30 rows were hand-probed with 5–14 spellings each.

⚠ **One sweep result was a false positive and was rejected.** `autopost` flagged `MidiParser::PushIdle` (6 → 1
mismatches) for `arr->Node(idx++)` → `Node(++idx)`. That edit writes node 2 instead of node 1, which is a semantics
change, not a spelling. From that row on, the pass was restricted to statement-level and for-step increments.
Before that row, no other BETTER site had been flagged.

## 3. Rows

### 3.1 Fixed (probe fuzzy 100)

| row | B | before | lever (landed spelling) |
|---|---:|---:|---|
| `RockCentral::RecordAccomplishmentData` | 4,676 | 99.957 | reuse the existing iterator `it` instead of a fourth one (`lbit`) |
| `GemPlayer::Hit` | 2,724 | 99.971 | `gem.GetMs()` instead of `gem.mMs` (2 `fadds` orders) |
| `CharSync::UpdateCharCache` | 1,352 | 99.911 | `bool inSong = overshell->InSong();` |
| `BandCharacter::ListAnimGroups` | 688 | 99.826 | `GetInstrumentFromSym(InstrumentType())`; by-value `Symbol` accessor |
| `NextSongPanel::FinishLoad` | 528 | 99.508 | drop single-use `strtablesize`; fixes an 11-instruction callee-saved rotation |
| `AccomplishmentSongFilterConditional::Configure` | 384 | 99.792 | `const char *str = pEntry->Str(1); Symbol sym = str;` |
| `TypeProps::SetKeyValue` | 380 | 99.895 | drop the `keystr` local (match-build arm only; native arm untouched) |
| `VignetteViewerProvider::RefreshVignettes` | 332 | 99.880 | `++it` → `it++` |
| `Singer::SetAssignedPart` | 332 | 99.880 | `float bonus = mVibratoFrameBonus;` inside the `if` |
| `FastInterp` (`math/Rot.cpp`) | 312 | 99.744 | `float x = q1.x;` before the dot product |
| `Gem::PartialHit` | 284 | 99.577 | `mGameGem.GetMs()` |
| `MasterAudio::Ignore` | 136 | 99.118 | `TrackData *data = mTrackData[num];` |
| `TourProgress::UpdateMostStars` | 132 | 99.697 | bind the discarded `AccessAccomplishmentProgress()` to a named reference |

The local's placement matters. In `Singer::SetAssignedPart`, `float bonus` declared inside the `if` reaches 100,
while the same local declared above the `!= 0` test is inert. In `CharSync`, a `ClosetMgr*` local reaches 100 at
either single use but reads 98.5 when one local is shared by both uses. Sharing it is a code change.

### 3.2 Partial (landed: a natural spelling that lowers the count and changes nothing else)

| row | B | mismatches | landed | why it stops |
|---|---:|---|---|---|
| `SaveLoadManager::SetState` | 4,096 | 22 → 10 | `bool shown = …ShowUserSelectUIAsync(…)` at all three sites (22 → 13); `CacheID *id = mCacheID;` before the 0x31/0x3d mount (13 → 10) | the prologue rotation {`newState`, `&static`, `wasIdle`} moved under none of 8 identity probes or 32 call-result sites; `bool shown` also introduces a vptr r10/r11 swap at case 0x58 (7 probes there inert); a second member-read pass over the whole body (66 sites) found nothing further |
| `SetlistMergePanel::OnMsg(ReleasingLockStepMsg)` | 708 | 4 → 3 | `int users = mSetlists[i].second;` | the three left (else-branch `mSetlists[j]` base/index) did not move under 12 spellings and identity probes; `std::vector<int> &` and `pair &` references change code |
| `Intersect(Vector3, Vector3, Triangle, float&)` | 296 | 9 → 8 | `float v1_y = v1.y;` (the style its `e2_x`/`v2_x` locals already use) | a second member-read pass on top of it (30 sites) moved nothing; `v2_z` / `e2_z` locals change code |

### 3.3 Stopped

Hand-probed rows, with the reason each stops. The full per-row log is in the lane notes, summarised here:

| row | B | residue | tried | why it stops |
|---|---:|---|---|---|
| `ParseNode` | 2,432 | 1 macro-loop add order | 23 identity sites, 9 local spellings | nothing moved it |
| `VocalTrack::RebuildHUD` | 2,188 | 2 `fadds` (`maxFrom + diffFrom`) | bare swap, 6 identity sites, `(*it).`, `RangeShift &`, `half` local | all inert; `+=`, `it++`, dropped locals change code |
| `MusicLibraryNetSetlists::ParseDataResultsIntoSetlists` | 1,968 | register order | 6 spellings | all worse or inert |
| `MetaPerformer::SelectRandomVenue` | 1,924 | 2 `Node(i)` adds | `_pfid(curArr->Sym(0))` fixes one of the two | 6 natural spellings of that site inert or worse |
| `Game::OnMsg(ButtonDownMsg)` | 1,664 | 3 `lwzx`/`stwx` on `mUnkCounts[pad]` + 1 `fadds` | 7 spellings, 4 identity sites | inert; `int &count` reads 12 mismatches |
| `EQEffect::SetParameter` | 1,644 | `fadds` B1+B2, `fmuls` | all four sweeps | no BETTER site |
| `PerfectOverdriveTracker::Poll_` | 1,248 | 2 add orders | `_pfid(find/end)` fixes one | no natural spelling; the other never moved |
| `VocalPart::HandlePhraseEnd` | 1,120 | 1 `mullw total*indMult` | every added temporary is *worse*; removals | retail is on the low side and nothing removable is left (`indMult` is used again) |
| `PatchPanel::Poll` | 980 | 2 `fmuls` recip×`unk64` | 10 spellings / identity sites | inert, or a code change |
| `VocalPart::SetDifficultyVariables` | 768 | 2 `Float(diff+1)` adds | 9 of 13 identity sites make it worse; 4 removals | retail on the low side; removals change code |
| `FocusTracker::Poll_` | 760 | stack slot (`TrackerPlayerID` @112 vs shared @96) | 10 identity sites, 5 spellings | a stack-slot sharing decision, not a temporary count |
| `Tour::OnMsg(PrimaryProfileChangedMsg)` | 596 | `lis` r10/r11 | 14 identity sites, 7 structural spellings | inert, or a code change |
| `DirLoader::Cleanup` | 548 | stack slot (`FilePath` copy vs `Timer::Stop` scratch) | 6 spellings | slot sharing |
| `XboxEntityUploader::ApplyStringVerifyResults` | 436 | 3 `Array(i)` adds (retail alternates) | 7 identity combinations | best is 2 mismatches, no natural spelling |
| `Performer::AddPoints` | 364 | 1 `fadds` (`mScore += add_points`) | 3 bare swaps, 10 identity sites, early declaration | all inert |
| `TrackDir::SetSlotXfm` | 336 | `this+608`/`this+620` r29/r30 | drop `prev`, drop `slot`, both, add `slots` | all worse (11 / 12 / 12 / 21) |
| `StoreMainPanel::Poll` | 664 | 1 `lwzx mCoverArtTexs[idx]` | 7 spellings | a named `tex` folds the second load (code change); the rest are inert |

**The other 81 rows (41,372 B).** 72 of them went through the four sweeps (§2) with no BETTER site, and the sweep
outputs are their record (per-site results in `~/tmp/w16pf/{auto,drop,mem,post}/NNN.txt`). All 15 `SCHED`-class rows
in the bucket are among them (instruction order, e.g. `Tail::UpdateVerts` 96.8, `VocalPart::GetBestHit` 97.0), and
none had a BETTER site in any pass. That is what the mechanism predicts: a temporary-count change that leaves the code
otherwise unchanged does not reorder instructions.

The other 9 could not be placed by the locator. Four (`Piercing::Deform`, `WorkVerts::SetSameVerts`,
`MeshVert::Normalize`, `MusicLibraryTask::GetSongFilterAsString`) were located by hand and given identity probes, with
no hits. **Five were not opened:** `BandCamShot::Load`, `CharWidgetImp::AddTextInstance`, `DeformTri::Contains`,
`PlayerDiffIcon::Save`, and `Normalize` (header body in `Vec.h`).

## 4. Whole-binary A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-pf-ab --patch <git diff main...w16-pf -- src>`
- **Worktree:** fresh, `scripts/setup_worktree.sh` at main `e095c7ab7`.
- **Patch:** 16 source files, sha256/16 `d14ac27f8fbfc2c1`. objdiff-cli sha `c1b7d952`, stable across legs.
- **Run dir:** `~/tmp/wt-w16-pf-ab/.ab_measure_runs/20261006-080855-w16-pf-3391315/`.

**Prediction, written before the run:** +0 functions (all 16 rows already at `mpn` 100), +12,260 B. That is the sum
of the 13 rows in §3.1. The three partials move fuzzy but no bytes.

```
[control none] Δmatched_code=+12260 B Δcode%=+0.119636 (default ruler +12260 B)
leg A: matched=53469 masked=25191 honest=28278 code%=57.679703  (recompiles: 0, settled)
leg B: matched=53469 masked=25191 honest=28278 code%=57.799340  (recompiles: 39, split=0, patch_steps=6, settle iterations: 2)
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.119637pp  Δcode_bytes=+12260
units at 100% [mpn ruler]: 547 -> 547   [all-rows-fuzzy ruler]: 483 -> 488 (+5, 0 fell off)
```

**Measured: +0 functions / +12,260 B, exactly as predicted.**

**Row-level diff of the archived legs:**
- 68,909 rows on both legs; none appeared or vanished.
- **16 rows up**: the 13 rows of §3.1 to fuzzy 100, plus the 3 partials. SetState went 99.893 → 99.951,
  SetlistMergePanel 99.774 → 99.831, Intersect 98.784 → 98.919.
- **0 rows down.**

The five unit completions on the fuzzy ruler are `AccomplishmentSongFilterConditional`, `TourProgress`,
`TypeProps`, `CharSync` and `ManageBandPanel`. The tool labels each one `UNEXPLAINED`, meaning the row count and
`mpn`-matched count are unchanged. That is the expected shape when bytes move on rows already at `mpn` 100.

## 5. Not done

- **The permuter was not run** (standing directive).
- No row was fixed with the identity probe itself. On two rows the probe closes one charge of two, and no natural
  spelling of that site was found (`PerfectOverdriveTracker::Poll_`, `MetaPerformer::SelectRandomVenue`). Both are
  recorded as stopped. `BandCharacter::ListAnimGroups` was in the same state after the call-result pass (only
  `_pfid(Symbol())` reached 100). The member-read pass then found the `InstrumentType()` spelling.
- Combinations of two or more sites were not swept automatically, only by hand on a few rows. A pairwise sweep is
  quadratic in sites per row (about 1,200 member-read sites in total).
- The scratch harness (`~/tmp/w16pf/`: `probe.py`, `batch.py`, `autoid*.py`, `autodrop.py`, `autopost.py`, the
  drivers and their outputs) was not committed.

## 6. Gates

`tools/native_build_gate.sh` is run last, on the commit that adds this document (no source changes after it). The
`NATIVE_GATE_RESULT` line is reported with the lane's result rather than written here, because committing it would
make the gate no longer the last action.
