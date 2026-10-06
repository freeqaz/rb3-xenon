# W16-PK: in-scope gap rows ranked by native runtime execution (2026-10-06)

Branch `w16-pk`, worktree `~/tmp/wt-w16-pk`. The branch was cut from main
`b371f188a` and rebased onto `7e5a99800` before measuring. Forks A and B
handled two row groups in their own worktrees (`~/tmp/wt-w16-pk-{a,b}`); their
patches are committed here as separate commits.

**Brief.** Take the remaining in-scope gap rows (report.json
`fuzzy_match_percent < 100`, unit source under `src/band3` or `src/system`;
`src/network` and `src/xdk` are out of scope). Rank them by whether the native
targets actually execute them on real data, measured at runtime rather than
from a static call graph. Then fix the executed rows to retail's behaviour,
most-executed first.

## 0. Headline

| | value |
|---|---|
| ranking tool | `tools/native_runtime_rank.py`, committed |
| in-scope sub-100 rows (main `b371f188a`) | 1,777 rows / 573,924 B (848 of them anonymous `fn_`) |
| rows joined to a native definition | 494 (sig 460 · arity 26 · unique-name 8) |
| **rows the 18 native targets actually entered** | **67 rows / 40,516 B** |
| rows linked into a native target but never entered | 427 |
| executed rows where our behaviour differed from retail | **4, all fixed** |
| executed rows lifted on codegen only | 2 to 100, 2 partial |
| executed rows fixed on main by W16-PI while this lane ran | 5 |
| X360 A/B of the lane's `src/` (`tools/ab_measure.py`) | **+7 fns / +1,648 B / +0.016078 pp code%, 8 rows up, 0 rows down** |
| executed sub-100 rows after the lane (main `7e5a99800` + lane) | 58 rows / 38,996 B, of which 2 are new because W16-PL's driver changes now reach them |

The biggest lesson is where the behaviour bugs were. All four sat in rows that
were either at 0% (pairing never looked like a near-miss) or at 98.66%
(CharPollableSorter::Sort: one load offset, `0x68` vs `0x6c`, turned a FIFO
queue into a LIFO one). None of them was in the high-count rows. The 20 most
executed rows are all register allocation, scheduling, stack layout, ICF fold
aliases, or the out-of-scope `ObjRef` ring that W16-PI fixed concurrently. In
other words, **the native targets execute our code in a state that already
matches retail behaviour wherever they spend their time.**

## 1. The tool

```
tools/native_runtime_rank.py [project_dir] [--no-build] [--no-run] [--out TSV] [--json OUT] [--top N]
```

1. **Build.** Configures `native/build-prof` (gitignored by `native/build-*/`)
   with clang `-fprofile-instr-generate -fcoverage-mapping` and builds every
   native target. This is a separate tree, so the gate's `native/build` is not
   touched. Cold build: about 3,770 edges, several minutes.
2. **Run.** Runs every target listed by `tools/native_health.sh`'s
   `run_target` lines, with the **same argv and completion marker** parsed
   from that file. Each run writes `LLVM_PROFILE_FILE=<target>-%p.profraw`,
   and each target gets its own `llvm-profdata merge`. A target that does not
   exit 0 with its marker is reported.
3. **Count.** Runs `llvm-cov export` per target and keeps a function only when
   its coverage-mapping filename is under this repo's `src/`. A
   `milo-native-engine` class that shares a name with one of ours therefore
   cannot be credited to our row. Counts are **function entry counts** from
   frontend instrumentation, which is taken before inlining, so an inlined
   body still counts. The counts are summed across targets, and the targets
   that entered each function are listed.
4. **Join.** Each native function is joined to report.json rows by demangled
   signature (`llvm-cxxfilt` / report's `demangled_name`). The tiers are:
   `sig` (qualified name + normalized parameter types + const), `arity`
   (qualified name + parameter count, used only when that pair is unique on
   both sides) and `name` (qualified name, again only when unique on both
   sides). Normalization strips `class/struct/enum/union`, maps
   `__int64` and `std::__1::` / `stlpmtx_std::` / `_STL::`, maps both spellings
   of the anonymous namespace to one form, and pairs MSVC `??_G`/`??_E` with
   Itanium `D0`.
5. **Rank.** Rows are sorted by total entry count, then by
   `size × (100 − fuzzy)`. The output is a TSV with
   `count, targets, fuzzy, mpn, size, tier, name, src, demangled`.

**Self-checks (exit 3).** The parsed target list must equal
`native_build_gate.sh`'s `KNOWN_TARGETS`.
`Symbol::Symbol(char const*)` must be counted and must join its report row
`??0Symbol@@QAA@PBD@Z` on the `sig` tier. The executed set must be neither
empty nor every row.

⚠ **The first version hardcoded the target table and only compared names.
That copy went stale the same day:** W16-PL gave rb3-score3 a band chart
(`"PART DRUMS" centerfold.mid`), and the names-only check stayed green. Since
the check could not fail on argv, the tool now parses argv and markers from
`native_health.sh` itself (commit `W16-PK: native_runtime_rank reads …`).

### 1.1 How complete the join is (measured, not assumed)

| question | measured |
|---|---|
| Of the 429 `none`-tier in-scope rows, how many have their **qualified name** defined natively at all (any count)? | **2**, and neither was executed. A join failing on type spellings is therefore not hiding executed rows. The `none` rows are in TUs no native target links. |
| Of the 2,172 executed native functions defined in `src/`, how many hit no report row? | 850 (`sig` 1,231 · qualified-name-only 90 · other dir 1). The top ones are inline header helpers that retail inlined and so have no row: `__mftb`, `Timer::Split`, `VocalNote::GetMs`, `MidiIsStatus`, `DataNode::Type`, `Symbol::operator<`. |
| Anonymous rows | 848 `fn_` rows cannot be joined by name. Natively executed functions that may be among them (for example `MidiReader::ReadNextEventImpl`, entered 378,893 times with no named row) are an **identification** question, not a runtime one. Left alone. |

## 2. The 67 executed rows, most-executed first

Fuzzy is report.json's `fuzzy_match_percent` on the shipped `name_check`
ruler. "pre" is main `b371f188a`; "post" is main `7e5a99800` + this lane.
Classes:
- **BEHAV** means our behaviour differed from retail. All of these are fixed.
- **CG** means the difference is codegen only (registers, scheduling, stack
  layout, operand order) and the behaviour is already equal.
- **FOLD** means the only charges are ICF fold-alias `bl` names: identical
  template bodies, folded in retail.
- **PLACE** means the row sits in a unit our TU does not emit the symbol into
  (inline/COMDAT placement). The behaviour of the body was checked.
- **MAIN** means W16-PI fixed the row on main while this lane ran.
- **DX** means the RB3DX TU5 in-place patch (CLAUDE.md).

| count | row | pre → post | class | evidence / why it stops |
|---:|---|---|---|---|
| 115,319 | Player::PollEnabledState | 99.87 → 99.87 | CG | one `fmuls` operand swap; swapping the source product was inert (measured, reverted) |
| 88,380 | VocalPart::GetNoteSliceWeight | 98.76 | CG | registers + placeholder `lbl_` float constants; mpn 100 |
| 61,390 | GameGemList::AddGameGem | 97.34 | CG | retail reuses `end` (`clrrwi r4,r10,0`) for `lower_bound`, while ours reloads `0x8(this)`; same pointers. `operator<` vs `CompareTimes` is an equal fold |
| 57,619 | BufStream::ReadImpl | 99.76 | CG | 1 register |
| 50,005 | VoiceBeat::Analyze | 86.55 | CG | the delay-line shifts load/store in a different order but give the same final values (checked: `a[0..3]=a[1..4]` both sides); "rebuilt from retail asm" by an earlier lane |
| 50,005 | Singer::ResolveAmbiguity | 99.47 | CG | 4 registers |
| 47,879 | ParseNode | 99.98 | CG | 1 register |
| 38,223 | VocalPart::GetBestHit | 96.97 | CG | `stfs`/`mr` scheduling before `ScoreNote` (fork A: reordering locals gave a stack-slot mismatch, reverted) |
| 37,604 | VocalPlayer::Poll | 94.98 | CG | `fabs` operand order; `srawi.` vs `clrrwi.` for the same non-zero `size()` test; W17 already documents these in source |
| 23,341 | MemAlloc | 97.14 | CG | scheduling of one `cmpwi`, registers |
| 17,261 | MemFree | 97.45 → **100** | MAIN | W16-PI `baf9a231a` laid MemMgr `.bss` out as retail's |
| 16,363 | MidiReader::ReadMetaEvent | 99.93 | CG | frame 0x1c0 vs 0x1b0; same `0x100` buffer bound (the immediates match) |
| 13,118 | MultiTempoTempoMap::GetLoopTick | 76.95 | CG | same `divw`/`twllei`/`andc`/`twi`, scheduled differently; inlining the modulo was inert (fork B) |
| 8,079 | Hmx::Object::AddRef | 69.68 → **100** | MAIN | W16-PI `e57fd4a3f`: `mRefs` is `std::list<ObjRefOwner*>` on X360 |
| 6,736 | Performer::AddPoints | 99.89 | CG | 1 register |
| 6,122 | Hmx::Object::Release | 77.61 → **100** | MAIN | same as AddRef |
| 5,586 | SongParser::StartVocalNote | 98.51 | CG | the same four loads in a different order; `EndMs()`/`EndTick()` dropped it to 97.1 (fork A, reverted) |
| 5,271 | ObjDirItr\<Object\>::operator++ (in CharServoBone) | 0.00 | PLACE | retail body is `FirstFrom(mEntry+1); Advance()`, the same as our `Next(mEntry)`; our CharServoBone.obj never emits it out of line |
| 2,228 | ChunkStream::Eof | 96.91 | CG | store order 0x8ac/0x8b0/0x8a4/0x888 vs 0x888 first; reordering the source was inert (fork B) |
| 2,065 | Hmx::Object::Object() | 72.08 → **100** | MAIN | W16-PI (STLport list ctor) |
| 1,322 | TambourineManager::TambourineSwing | 95.83 | CG | one `mr` hoisted above both branches |
| 705 | VocalPart::IsEmptyPhrase | 96.55 | CG | retail has an extra no-op `clrrwi r10,r10,0` |
| 643 | CommonPhraseCapturer::ExtendPhraseStates | 99.84 | FOLD | `_M_fill_insert<Note>` vs `<PhraseState>` |
| 513 | VibratoDetector::Detect | 96.54 | CG | registers (one extra `mr`) |
| 483 | GemNumSlots | 97.86 | CG | 6 registers; mpn 100 |
| 420 | VocalPart::CalcPhraseScoreMax | 76.75 | CG | same max/min/divide; retail loops on a compare (`cmplw;bne`), ours on `mtctr/bdnz` |
| 365 | pow (MidiReader) | 83.25 → **90.00** | CG, partial | fork A: local copy of `base`, unsigned exponent, ternary reciprocal; the residual is the `cmpwi`/`mr` schedule |
| 258 | SongParser::ParseText | 98.68 | CG | block layout of the dev-only `String`/`TickFormat` warning arms |
| 241 | BufStream::~BufStream | 92.31 | CG | one `addi` scheduled before vs after the zero stores |
| 235 | VocalPart::HandlePhraseEnd | 99.96 | CG | 1 register |
| 225 | Archive::GetFileInfo | 97.92 | CG | `li r3,1` vs `li r29,1; mr r3,r29` across the String dtor |
| 130 | RndMesh::LoadVertices | 94.53 | CG | native runs its own HX_NATIVE compressed-vertex arm; the X360 arm differs only in `slwi` placement and loop-end hoisting |
| 130 | RndMesh::Load | 99.41 | CG | retail spills a vector address to `0x54(r31)` before three calls |
| 97 | NewFile | 84.39 | CG | our extra `gNullFiles` → `NullFile` early-out is kept on purpose (the in-source note measures −4 fns / −88 B when it is removed); `gNullFiles` is never set, so the branch is dead |
| 97 | FileLocalize | 99.95 | CG | 1 register |
| 92 | Normalize(Vector3) | 99.38 | CG | 2 registers |
| 69 | ArkFile::ReadAsync | 98.97 | CG | registers |
| 61 | SongParser::Reset | 99.98 | FOLD | `_M_fill_insert<Note>` vs `<RGTrill>` |
| 57 | Stats::EndMultiplier | 99.75 | CG | commutative `fadds` operand order; an in-source note (CM-1-C) records the source swap as a byte-identical no-op |
| 47 | ObjectDir::NextSubDir | 80.46 → **100** | CG | fork B: retail does `li r3,0` before the loop and returns r3; `ret` hoisted |
| 46 | ObjectDir::ResetViewports | 98.31 | CG | −768 held in f30 with `fmadds` vs +768 with `fmsubs`; numerically identical including ±0 |
| 30 | SymToTrackType | 40.37 → **100** | CG | fork A: retail tests `cmpwi i,10; blt; bne` (`i < kNumTrackTypes \|\| i == kTrackNone`); the range is unchanged at 0..10 |
| 21 | ThreadMemStack | 85.05 → 85.17 | CG | retail's lock is anonymous `.bss` (`lbl_82E06E0C`). **Tried:** making `gMemStackLock` a file static (it has no definition anywhere in X360 `src/`; native gets a weak `.s` stub). That moved the row to **82.6** because the lock co-addressed with the wrong anchor, so it was reverted. W16-PI's `.bss` relayout moved the row +0.12 |
| 21 | ObjectDir::PreLoad | 99.62 → **99.70** | CG, partial | fork B: `size() > 0` gives retail's `srawi.`; the residual is a whole-function r17↔r18 swap |
| 21 | MemHeapTracker::~MemHeapTracker (in BandHeadShaper) | 0.00 | PLACE | a 4-byte `b` thunk retail placed in that unit |
| 21 | DirLoader::Cleanup | 99.96 | CG | frame 0x80 vs 0x90, same locals |
| 20 | Hmx::Object::~Object | 96.62 → **100** | MAIN | W16-PI |
| 19 | DirLoader::~DirLoader | 98.76 | CG | one extra frame spill of `mDir` |
| 6 | VocalNoteList::NotesDone | 98.65 | CG | spill and `addi`/`add` scheduling |
| 6 | MemFindHeap | 99.54 → 99.57 | CG | anchor offset (`0x254` vs `-0x1b4`) of the same `gNumHeaps`; W16-PI moved it |
| 5 | VocalPart::SetDifficultyVariables | 99.90 | CG | 2 registers |
| 4 | **CharBonesSamples::Load** | **0.00 → 100** | **BEHAV** | retail reads `gVer` and goes straight to `LoadHeader`/`LoadData`; ours called `TheDebugFailer << MakeString(...)` on an out-of-range version. Now `MILO_ASSERT` (still live natively, compiled out on X360) |
| 4 | CharClip::Transitions::Resize | 98.00 | CG | retail spills `r3` before `MemRealloc` |
| 4 | VocalPart::UpdateMinMaxPitch | 98.61 | CG | registers + placeholder constant |
| 4 | HDCache::Init | 99.79 | FOLD | `_M_fill_insert<Object*>` vs `<File*>` |
| 4 | BlockMgr::Init | 99.94 | FOLD | `_M_fill_insert<Object*>` vs `<Block*>` |
| 3 | **operator>>(FixedSizeSaveableStream&, FixedSizeSaveable&)** | **0.00 → 100** | **BEHAV** | retail is `LoadFixed(fs, sCurrentMemcardLoadVer); return fs;`. Ours also called `Tell()` twice and `mSaveSizeMethod`, and `MILO_FAIL`ed on a size mismatch. The check now sits under `MILO_DEBUG && HX_NATIVE` (still live natively) |
| 3 | **operator<<(FixedSizeSaveableStream&, const FixedSizeSaveable&)** | **0.00 → 100** | **BEHAV** | same, for `SaveFixed` |
| 3 | SongData::ValidateVocalSPPhrases | 99.56 | CG | registers |
| 2 | Hmx::Object::SyncProperty (in Tour) | 0.00 | PLACE | COMDAT placement in another unit |
| 2 | Debug::Fail | 94.09 | CG | native returns early in its HX_NATIVE arm and never runs the retail body; on X360 it is branch layout only (a guarded do-while was inert, fork B) |
| 2 | **CharPollableSorter::Sort** | **98.66 → 100** | **BEHAV** | retail's `erase(begin())` reads the list head's `_M_next` (`lwz r11,0x68(r31)`); ours read `_M_prev` (`0x6c`). That is `pop_back`, which walks the dependency queue **LIFO where retail walks it FIFO**. DC3 has `front()`/`pop_front()` too |
| 2 | Character::PostLoad | 99.65 | CG | the revision halfword is addressed off a different base (`0x4(r21)` vs `-0x4(r21)`), same slot |
| 2 | TrackWatcherImpl::GetNextRoll | 99.23 | CG | 2 registers |
| 2 | TrackWatcherImpl::TrackWatcherImpl | 99.98 | FOLD | `_M_fill_insert<Note>` vs `<GemInProgress>` |
| 1 | BandSongMgr::AddSongData | 99.63 | DX | retail (the DX image) has `li r3,0` where clean TU5 calls `IsInExclusionList`; structurally unmatchable per CLAUDE.md |
| 1 | RndGroup::ListDrawChildren | 99.85 | FOLD | `_M_splice_insert_dispatch<RndPollable*>` vs `<RndDrawable*>` |

### 2.1 Rows the post-lane ranking added

W16-PL (merged on main during this lane) made rb3-score3 run a band chart
through the unison path, so two rows are now entered for the first time. Both
were opened:

| count | row | fuzzy | class | evidence |
|---:|---|---|---|---|
| 8 | SongData::TrimOverlappingGems | 87.41 | CG | the cur/next tick loads are scheduled around the `it == end` test, with iterator spills to `0x58(r31)`; `next.tick − cur.tick` is the same on both sides |
| 4 | MultiplayerAnalyzer::AddGems | 85.02 | CG | after the `mMaxPts` store, retail reloads `mGemScores.begin()` for `mGemScores[j].unk4`, while ours reuses the cached element pointer; the value is the same. The source already spells the second access as `pData->mGemScores[j]` |

## 3. Measurement

**`tools/ab_measure.py --patch`** of `git diff main -- src/` (7 files), run in a
fresh worktree at main `7e5a99800`, settled on both legs, `name_check` ruler.
Run dir: `~/tmp/wt-w16-pk-ab/.ab_measure_runs/20261006-090024-w16pk-lane-3903967`.

```
leg A: matched=53484 masked=25192 honest=28292 code%=57.825882
leg B: matched=53491 masked=25192 honest=28299 code%=57.841960  (recompiles: 1006)
Δmatched=+7  Δmasked_equal=+0  Δhonest=+7  Δcode%=+0.016078pp  Δcode_bytes=+1648
units at 100% [mpn]: 548 -> 549 (TrackType reached 100, 0 fell off)
```

**Prediction before the run:** about +6 fns / about +1,650 B (the six rows
that reach fuzzy 100 sum to 1,648 B; pow and PreLoad are partial and add no
bytes). Measured: **+1,648 B exactly**, and +7 fns, because PreLoad's mpn
reaches 100 while its fuzzy stays at 99.70.

**Row-by-row diff of the archived leg reports** (`legA_report.json.gz` vs
`legB_report.json.gz`, every `(unit, symbol)` on fuzzy and mpn): **8 rows up,
0 rows down**, 0 rows appearing or vanishing. Those 8 are the 6 rows to 100,
plus pow 83.25 → 90.00 and PreLoad 99.62 → 99.70.

Each fork also ran a full build of its own patch against the lane worktree's
report before this A/B: fork A measured +1 fn / +108 B and fork B +2 fns /
+156 B, and in each case only the rows named in its commit moved.

## 4. Deliberately not done

- **The ObjRef ring → STLport list change** (Hmx::Object ctor/dtor/AddRef/Release)
  was out of this lane's size; fork B stopped on it. W16-PI landed the same
  change on main while this lane ran, so those rows are now at 100 for that
  reason.
- **Register/scheduling grinding** on the high-count CG rows, with no permuter.
  Those rows already behave like retail, and the brief was behaviour. Several
  carry in-source AT_LIMIT notes from earlier lanes.
- **PLACE rows** (ObjDirItr::operator++ in CharServoBone,
  MemHeapTracker::~MemHeapTracker in BandHeadShaper, Hmx::Object::SyncProperty
  in Tour). These rows need our TU to emit the COMDAT where retail placed it;
  the bodies are correct.
- **Anonymous `fn_` rows.** The tool cannot join them by name. Joining natively
  executed functions with no row, for example
  `MidiReader::ReadNextEventImpl`, to anonymous retail addresses is an
  identification lane.
- **`native_runtime_rank.py` is not wired into the health check.** A
  profile-instrumented rebuild is several minutes cold. It is a
  ranking instrument to run before choosing rows, not a gate.

## Reproduce

```bash
python3 tools/native_runtime_rank.py <tree> --out rank.tsv --json rank.json --top 60
#   first run configures + builds <tree>/native/build-prof (several minutes)
python3 tools/native_runtime_rank.py <tree> --no-build --no-run --out rank.tsv   # re-rank only
```
