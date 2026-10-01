# W16-LB — engine 90–99.99 band: charge-class census and fixes (2026-10-01)

**Branch** `w16-lb`, started off main `9d627e235` (W16-KF landing), rebased onto main `c055eaf8f` (W16-LA) with
`--rebase-merges` (pre-rebase tips kept as `w16-lb-prerebase`, `w16-lb-prerebase2`). **Not merged to main**, per
the brief.
**Ruler** `name_check` (graded; `report.json` `provenance.diff_config`).

**Population.** Every row with `90 ≤ fuzzy < 100` in a unit whose `objdiff.json` `metadata.source_path`
is under `src/system/{rndobj,synth,synth_xbox,ui,utl,obj,meta}/`. `bandobj`, `char` and `band3` were not
touched (other lanes own them). `matched_code` credits a row's full size only at fuzzy 100, so the band
is ranked by size.

## 1. Class census (main `9d627e235`)

The brief's figure reproduces exactly: **936 rows / 204,488 B** (`~/tmp/w16lb/pop.py`).

Each row was diffed with `objdiff-cli diff` under the project config (`~/tmp/w16lb/cls.py`); the diff's
`fuzzy_match_percent` equals `report.json`'s on **936 / 936** rows, so the census is on the graded ruler.
A row takes the first class that applies, in this order:

| class | rule |
|---|---|
| STRUCT_INSDEL | any inserted or deleted instruction |
| OPCODE | any replaced instruction or differing opcode, no ins/del |
| IMMEDIATE | an immediate argument differs (not `r1`-based) |
| STACK_REG | only `r1`-based immediates and registers differ (stack layout follows register residue) |
| NAME_ONLY | only relocation target names differ |
| NAME+REG | relocation names and registers |
| REG_ONLY | only registers differ (register allocation; skipped by brief) |

Bytes by class and directory:

| class | rndobj | synth | synth_xbox | ui | utl | obj | meta | rows | bytes |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| STRUCT_INSDEL | 56,816 | 6,260 | 8,856 | 5,044 | 8,820 | 6,704 | 2,188 | 152 | 94,688 |
| NAME_ONLY | 32,548 | 5,644 | 2,488 | 6,096 | 2,124 | 2,720 | 1,952 | 523 | 53,572 |
| REG_ONLY | 9,396 | 4,448 | 1,184 | 1,508 | 920 | 3,848 | 1,468 | 53 | 22,772 |
| IMMEDIATE | 10,520 | 872 | 2,056 | 1,152 | 1,696 | 200 | 1,780 | 159 | 18,276 |
| OPCODE | 836 | 332 | 168 | 0 | 1,228 | 3,848 | 40 | 22 | 6,452 |
| NAME+REG | 1,952 | 432 | 1,588 | 0 | 920 | 0 | 0 | 14 | 4,892 |
| STACK_REG | 2,036 | 440 | 0 | 0 | 0 | 548 | 0 | 6 | 3,024 |
| other (branch-dest / mixed arg kinds / one `diff_op`) | 276 | 216 | 0 | 0 | 92 | 0 | 228 | 7 | 812 |
| **total** | 114,380 | 18,644 | 16,340 | 13,800 | 15,800 | 17,868 | 7,656 | **936** | **204,488** |

Where the bytes sit inside the two big classes:
- **STRUCT_INSDEL** is dominated by large Load/Draw/Save bodies with 50–220 charged instructions
  (`RndMesh::Load` 3,452 B / 127 charges, `RndTexRenderer::DrawToTexture` 3,320 B / 220). Rows with
  under ~12 charges are the realistic targets.
- **NAME_ONLY**: one pair, retail `??1ObjRef@@QAA@XZ` vs our `??1ObjRefOwner@@UAA@XZ`, was the sole charge
  on **140 rows / 5,608 B** (§3.1). 368 NAME_ONLY rows / 14,968 B are anonymous `fn_` rows, mostly 40-byte EH funclets; most of them
  called the ObjRef survivor and closed with §3.1. After §3.1–3.3, 135 anonymous rows / 5,440 B remain.

Single-pair closability, measured on the census: 511 NAME_ONLY rows / 49,624 B carry exactly one distinct
name pair.

⚠ **NAME_ONLY is an upper bound on "a name fix closes the row".** On an instruction whose relocation
target differs, `diff_breakdown` lists only the `symbol` argument even when a register also differs.
`EventTrigger::Replace` read NAME_ONLY with an unnamed retail target, and showed a register swap once the
target was named (§4.2).

## 2. Results

### 2.1 Whole-branch A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-lb-ab --patch <git diff c055eaf8f..w16-lb, docs excluded>`.
- **Worktree:** fresh, `setup_worktree.sh` at main `c055eaf8f`.
- **Patch:** 42 files; kinds map + source + splits; no `symbols.txt`.
- **objdiff-cli:** sha `c1b7d952`, stable across legs.
- **Both legs** were read at a split fixed point (0 extra re-splits). Leg B made 1,000 recompiles, one
  split, renamer patched 1,856.
- **Run dir:** `~/tmp/wt-w16-lb-ab/.ab_measure_runs/20261001-165148-w16-lb-branch-3593990/`.

```
leg A: matched=49957 masked=24398 honest=25559 code%=51.628090  (recompiles: 0, settled)
leg B: matched=50055 masked=24403 honest=25652 code%=52.278416  (recompiles: 1000, split=1, patch_steps=7, settle iterations: 2)
Δmatched=+98  Δmasked_equal=+5  Δhonest=+93  Δcode%=+0.650326pp  Δcode_bytes=+66640
Δfuzzy=+0.009720pp   (legA 59.271255 -> legB 59.280975)
units at 100% [mpn ruler]: legA 395 -> legB 401  (Δ+6; 6 reached 100, 0 fell off)
units at 100% [all-rows-fuzzy ruler]: legA 319 -> legB 346  (Δ+27; 27 reached 100, 0 fell off)
[control none] Δmatched_code=+17940 B -- NOT_APPLICABLE (source in patch)
```

**Prediction, written before the run:** +98 fns / +66,640 B / +0.650326 pp, 979 rows up (939 to 100), one
row down. It came from in-tree full builds of the rebased tip and of main `c055eaf8f`. **Measured: exactly
that, on every key.**

**Row-level diff of the archived leg reports**, keyed by retail address (`rowdiff.py`): **979 rows up, 939
to 100; 1 down; 0 rows off 100 on the fuzzy ruler and 0 on the `mpn` ruler.** The one row down is
`0x82687D48` (99.76 → 0, §4). The two unit "regressions" in the tool's list (HamNavList −2, PanelDir −1)
are re-homes moving rows out of those units.

The bytes are far larger than the band's own movement because the `0x82270298` repair (§3.1) and the fold
memberships (§3.2) close rows in every unit that calls those functions, including units outside this lane.

### 2.2 The band itself, on one base

Every leg-A band row (main `c055eaf8f`: **926 rows / 201,444 B**), keyed by retail address, read at the
lane tip:

| class at leg A | rows | bytes | to 100 (rows) | to 100 (bytes) | rose, not to 100 | fell |
|---|---:|---:|---:|---:|---:|---:|
| STRUCT_INSDEL | 151 | 93,304 | 32 | 15,680 | 12 | 0 |
| NAME_ONLY | 518 | 52,360 | 276 | 23,376 | 2 | 0 |
| REG_ONLY | 53 | 22,772 | 0 | 0 | 0 | 0 |
| IMMEDIATE | 155 | 17,828 | 7 | 1,444 | 7 | 1 |
| OPCODE | 22 | 6,452 | 2 | 472 | 0 | 0 |
| NAME+REG | 14 | 4,892 | 0 | 0 | 2 | 0 |
| STACK_REG | 6 | 3,024 | 0 | 0 | 0 | 0 |
| other | 7 | 812 | 0 | 0 | 0 | 0 |
| **total** | 926 | 201,444 | **317** | **40,972** | 23 | 1 |

At the tip the band holds **609 rows / 160,872 B**. Rows that rose from below 90 have entered it, which is
why REG_ONLY grew from 53 to 57.

### 2.3 Step ledger

| step | Δfns | ΔB | rows up (to 100) / down |
|---|---:|---:|---|
| §3.1 `0x82270298` map repair | +1 | +29,704 | 768 (743) / 0 |
| §3.2 67 fold memberships | +40 | +14,808 | 128 (118) / 0 |
| §3.3 ShaderMacro copy ctor + fold | +1 | +3,084 | 1 (1) / 0 |
| fork snd | +9 | +1,172 | 9 (7) / 0 |
| fork oum (band3 re-home reverted) | +11 | +5,036 | 17 (13) / 0 |
| §3.5 MemTrackReport | +1 | +72 | 1 (1) / 0 |
| fork r1 | +21 | +7,516 | 24 (21) / 0 |
| §3.6 Key.h | +1 | +72 | 1 (1) / 0 |
| fork r2 | +16 | +5,768 | 19 (15) / 1 explained |
| §3.8 RndConsole::SetShowing | +2 | +268 | 3 (2) / 0 |
| **sum, base `9d627e235`** | **+103** | **+67,500** | |
| **A/B, rebased onto `c055eaf8f`** | **+98** | **+66,640** | 979 (939) / 1 |

Each step was measured with a full build and a whole-report row diff against the previous step. The
pre-rebase chain brought **922 rows / 67,500 B** to 100. The A/B difference is fully attributed, keyed by
retail address:
- **−1,740 B / 5 rows** were already at 100 on main `c055eaf8f`: `NgRnd::UpdateOverlay` and the four
  `MakeString<int,int>` callers. W16-LD admitted the same `MakeString<int,int>` membership into the same
  survivor, and the installer skipped it on the rebase.
- **+880 B / 22 rows** are new on the new base: 40-byte EH funclets in bandobj/char units that W16-LA
  touched (StreakMeter, CharEyes, OutfitConfig, …). They call the `0x82270298` dtor and close through
  §3.1's map repair. No bandobj/char source was edited.
- 67,500 − 1,740 + 880 = **66,640**, the measured figure.

## 3. Lane work (global map and alias files)

### 3.1 `0x82270298` is `~ObjRefOwner`, not `~ObjRef` — 743 rows to 100, +29,704 B

The largest NAME_ONLY pair was retail `??1ObjRef@@QAA@XZ` against our `??1ObjRefOwner@@UAA@XZ`: 140 rows
/ 5,608 B in the band, each with that as its only charge.

- The retail body at `0x82270298` is `lis r11,0x8200 / addi r11,r11,0x9EC / stw r11,0(r3) / blr`. It
  installs the vtable at `0x820009EC`, whose RTTI is `.?AVObjRef@@` (`tools/retail_rtti.py vtable`).
  Retail has **no** `ObjRefOwner` class (`retail_rtti.py class ObjRefOwner`: no COL).
- Our X360 `ObjRefOwner` **is** retail's `ObjRef`: W17-OPTR re-based `ObjPtr`/`ObjOwnerPtr` on it, and
  `tools/icf_pair_adjudicate.py` carries `CLASS_RENAMES = {ObjRefOwner@@: ObjRef@@}` with a retail-byte
  witness. The map already names the same vtable's slot-0 deleting dtor `0x822702A8`
  `??_GObjRefOwner@@UAAPAXI@Z`.
- Our `ObjRef` has no vtable in the match build (`OBJREF_VIRTUAL` is empty without `HX_NATIVE`), so its
  dtor cannot be a body that stores a vtable.
- As an alias the pair is not provable: `--chase` reads it REFUTED, because the 4-word body with 2 masked
  words is vacuous at top level. The defect is the **map name**, so the fix is a map repair.
- Safety: no relocation in any of our objects targets `??1ObjRef@@QAA@XZ` (only Object.obj and
  Instance.obj define an unreferenced COMDAT), and `??1ObjRefOwner@@UAA@XZ` sat at no other address.
  MatAnim.obj defines it, so the row at the address pairs.

Full build vs main: **+29,704 B, +1 fn; 768 rows up (743 to 100), 0 down.** The prediction (140 rows in the
band, more binary-wide) was right in direction and about 5× low in magnitude: the pair was charged
binary-wide. `matched_functions` moves only +1 because `mpn` already excluded these charges.

### 3.2 67 chase-proven fold memberships — 118 rows to 100, +14,808 B

All 294 function-name pairs left in the band went through `tools/icf_pair_adjudicate.py --pairs --chase`:
**111 CHASED T1 PROVEN, 183 REFUTED**. Admissible = PROVEN with no `CYCLE` or `UNDISCHARGED` text in the
pair's own log block (W16-KD's rule): **89**. `~/tmp/w16lb/install.py` then screened each:

| screen | refused |
|---|---:|
| spelling already a member or survivor of another group | 15 |
| the map places the spelling at an address of its own (not a fold) | 7 |
| installed | **67** |

`tools/alias_placeholder_slot_audit.py`: **67 / 67 CLEAN, cycle 0**. `icf_alias_finder.py --validate`:
PASS, 1,667 map-consistent, 0 contradicted.

Ten admissions override earlier withdrawal records for the same spelling; each admitted record names the
withdrawal and why the per-pair chase answers it:
- 7 `FABRICATED_CLOSURE_NOT_PARTITION`: closure memberships refused for want of a per-pair proof;
- `CF2_WARRANT_WITHDRAWN` on `~list<RndMultiMesh::Instance>`. That record asked for a positive warrant.
  The chase supplies one: the 4-byte body's branch destination is recursively proven to be the same
  function on both sides. The address that used to separate the spellings, `0x8273e038`, was a wrong
  map row and is now `DxEnviron::Select` (W16-JA);
- `WRONG_MAP_NAME_AT_ADDRESS` on `MakeString<int,int>`, withdrawn from a *different* group (`0x82399348`);
- group `0x82440118` `Keys<Quat>::Remove`: its withdrawn member is the `Keys<Color>` spelling (KeyLessEq vs
  KeyGreaterEq). The `Keys<Vector3>` spelling's chase recursed through its own callees and they agree.

Full build: **+14,808 B, +40 fns; 128 rows up (118 to 100), 0 down.** The prediction was 90 band rows /
12,436 B. `matched_functions` moves because objdiff `b14ba45` lets a vetted wrong-callee charge reach
`mpn`.

### 3.3 `ShaderMacro` needs a user-declared copy constructor — GenerateMacros 99.55 → 100, +3,084 B

`ShaderOptions::GenerateMacros` (3,084 B) had one charge: retail's survivor
`push_back<pair<const VocalPhrase*,VocalPart*>>` against our `push_back<ShaderMacro>`. The chase refuted
at one leaf: our `__uninitialized_fill_n<ShaderMacro>` differs in bytes from retail's
`fill_n<pair<int,int>>` at `0x824F9020`. Retail counts with `mtctr`/`bdnz`; ours counted with
`addic.`/`bne`.

Across all 48-byte `fill_n` COMDATs in our build, the `mtctr` form appears for exactly the element types
that have a **user-declared copy constructor**: every `std::pair`, `CharBones::Bone`, `MicClientID`. Every
other type uses the counter form. Our own `fill_n<pair<int,int>>` is at 100 (Rot), so the compiler
reproduces retail's form when the type has one.

- First attempt, **failed prediction**: removing the user `operator=` left `fill_n` unchanged and dropped
  GenerateMacros 99.55 → 86.80. Reverted.
- Adding `ShaderMacro(const ShaderMacro&)` makes `fill_n<ShaderMacro>` byte-identical to `0x824F9020`. The
  `push_back` pair then reads CHASED T1 PROVEN, no CYCLE, and is installed.

Full build: GenerateMacros to 100, **+3,084 B, +1 fn, 0 down.**

### 3.4 Negative result: naming retail RTTI TypeDescriptors and literals

Nine data addresses that rows charge as unnamed `lbl_` against our named data were named from retail
content: six `??_R0` TypeDescriptors (each by its own `.?AV…` string at +8), two string literals (`free`,
`alloc`) and `__real@46fffe00`. All gates passed. Prediction: 4 rows / 1,752 B to 100.

**Measured: 0 rows up, 8 down, −2 fns** (two AmbientOcclusion `GatherObjects*<RndMesh>` rows fall from
`mpn` 100). Two things were wrong with the model:
- The `lbl_82C70AD0` ↔ {Object, RndMesh} pairings are **not wrong casts**. The function loads three
  TypeDescriptors and retail schedules the three `lis`/`addi` pairs in a different order.
- Unnamed `lbl_` targets were partly forgiven. Naming them turned forgiven sites into checked ones and
  exposed ordering and register residue: `EventTrigger::Replace` is name-correct afterwards and shows a
  register swap.

Reverted in full (nothing committed); the tree's measures came back bit-identical. The names are right on
retail bytes, but under the brief's no-row-off-100 rule they are a net loss here.

### 3.5 `MemTrackReport` takes one argument — MemTrackReportDF 93.78 → 100, +72 B

Fork oum re-homed `0x827C4AA0` (`MemTrackReportDF`, from a lead by fork snd), and the row read 93.78:
retail calls `MemTrackReport` (`0x827C4828`) with `r3 = 1000` and leaves `r4` holding the incoming
`DataArray*`. Retail's body never reads `r4`, and `tools/retail_callers.py` finds **one** call site, so
retail's function has one parameter. The match build now declares `MemTrackReport(int)`, and the map row
follows to `?MemTrackReport@@YAXH@Z`; native keeps `(int,bool)` for its `MemTrackInit` tail.
Full build: +1 fn / +72 B, 0 down.

### 3.6 `Key.h`: `operator>>(BinStream&, Weight&)` chains the stream — +72 B

Retail's `Key<Weight>` load (`0x82482F50`, Morph) reads through the stream returned by the `Vector3`
read. Full build: 94.44 → 100, +1 fn / +72 B, 0 down. (Lead from fork r1.)

### 3.7 The `const bool` lever: the stated tell finds 0 rows, yet the lever closed one

A sibling lane reported that a `const bool` local removes bool-materialisation residue (retail
`li rN,0|1` into a callee-saved register where ours has `clrlwi rN,rM,24`), and that a named
`static const float kZero` restores retail's per-compare pool reload with no FPR save.
`~/tmp/w16lb/booltell.py` scanned every band row outside NAME_ONLY at lane tip `fe1cd8ff0`
(383 of the band's 635 rows; NAME_ONLY rows carry no such instructions by construction):

| tell | rows |
|---|---:|
| any `clrlwi …, 0x18` on our side | 23 |
| our `clrlwi …, 0x18` on a charged line | 2 |
| retail `li r14–r31, 0/1` on a charged line | 24 |
| **both on the same row (the tell)** | **0** |
| retail reloads `__real@00000000` more often than we do, with an FPR save on our side | **0** |

The two smallest "retail `li 0/1`" rows (`DataNetLoader::PollLoading`, `FileLoader::SaveData`) were read
by hand. Both are pure register permutations (`li r28,0` vs `li r29,0`), not bool materialisation. The
first version of the scanner keyed on `, 24` and could not fire, because objdiff prints `0x18`. It was
caught by printing the raw counts beside the conjunction.

⚠ **The tell undercounts.** Fork r2 closed `NgPostProc::DoBloom` (1,312 B, 99.57 → 100, `15c82bfa6` on the
rebased branch) by making `doBloom`/`doGlare` `const bool` locals. At the census that row was
STRUCT_INSDEL with one inserted instruction and seven register arguments: the extra materialisation showed
as an **inserted** instruction, not as `clrlwi`-vs-`li` on one line. So "0 rows with the tell" does not
mean "0 rows the lever can close". Small STRUCT_INSDEL rows with one ins/del and a run of register
arguments are the candidates to try it on.

### 3.8 `RndConsole::SetShowing` is empty in retail — +2 fns / +268 B

Retail `Rnd::OnShowConsole` (`0x82412D40`) and `ModalKeyListener::OnMsg` (`0x82411968`) call the shared
`blr` survivor `0x826C3888` (at `0x82412D68` and `0x824119C4`) where our source calls
`RndConsole::SetShowing`. The match build's body is now empty (native keeps it). That alone moved no row:
the in-TU Console.cpp callers are unchanged. The spelling is admitted to group `0x826C3888` (CHASED T1
PROVEN, VACUOUS-BUT-IDENTICAL) on that call-site witness, W16-KD's rule for 4-byte bodies. Both callers
reach 100. (Lead from fork r2.)

### 3.9 Not taken

- 15 chase-proven spellings are already members of a group whose survivor is a *different* retail
  address. Retail keeps two byte-equivalent bodies, and a spelling can fold to only one survivor. Not
  changed.
- 40-byte anonymous EH funclets whose "callee" pairs (`~ObjRefOwner` vs `~String`, …) come from
  byte-signature pairing of unrelated funclets: the `masked_equal` class, not a defect.

## 4. Forks

Four forks ran in their own `setup_worktree.sh` worktrees on disjoint directories, branched off lane tip
`5d55cb864`. Each measured every change with a full `./tools/ninja-locked` build and a whole-report row diff
(`~/tmp/w16jc/rowdiff.py`). Each was rebased onto the lane, rebuilt, row-diffed against the lane's previous
build, and merged with `git merge --no-ff`. The merge-time measurement equals the fork's own report wherever
nothing was reverted:

| fork | scope | merged Δ | rows up (to 100) / down |
|---|---|---|---|
| snd | synth, synth_xbox, utl | +9 fns / +1,172 B | 9 (7) / 0 |
| oum | obj, ui, meta | +11 fns / +5,036 B | 17 (13) / 0 |
| r1 | rndobj units A–M | +21 fns / +7,516 B | 24 (21) / 0 |
| r2 | rndobj units N–Z | +16 fns / +5,768 B | 19 (15) / 1 explained |

**Behaviour bugs fixed** (each on retail bytes):
- `StorePanel::LoadArt` requested `NetLoaderPos` 0; retail passes 1.
- `~WorldDir` leaked `mGlowMat`.
- `RndMatAnim::LoadStage` loaded the stage keys into `mKeysOwner`'s lists instead of its own.
- `RndBitmap`'s DXT3 alpha nibble leaked into the next texel.
- `FloatKeys::FloatAt` used the wrong fourth spline control point (retail `at(idx + 2)`).
- `EventTrigger::TriggerSelf` recorded the task before starting it.

**Map and pin repairs from the forks:**
- `0x827BE188`/`0x827BE190` are `String::find(const char*)`/`String::contains`: they read `mStr` at +8,
  which is String's layout, not FixedString's.
- Three `ObjDirPtr<UILabelDir>` members were named for a `HamListRibbon` class this game does not have.
  Retail `dynamic_cast`s to `.?AVUILabelDir@@`. Re-homed to UIFontImporter.
- `0x827C4AA0` is `MemTrackReportDF`, re-homed from DataFunc to MemTrack.
- `0x827ED550` is `vector<DataEvent>::_M_insert_overflow_aux`, re-homed from PanelDir to DataEventList.
- `0x82474D20` is `RndFont::SetASCIIChars`. It and a second block were holes in Font's `.text` pinned to
  FilterVersion, whose splits entry is removed.
- `ObjectKeys::ObjectAt`/`SymbolKeys::SymbolAt` were swapped; `0x8240F158` is `Rnd::SetProcAndLock`;
  `0x8245F378` is `vector<Key<Vector3>>::_M_allocate_and_copy`.

**The one row that fell:** `0x82687D48` in TransAnim, 99.76 → 0. It allocates 0x64-byte elements and calls
`__uninitialized_copy<SongDB::TrackData>`, so it is `vector<SongDB::TrackData>::_M_allocate_and_copy`, not
the `Key<Vector3>` name it had. TransAnim.obj cannot define it, so it reads 0. It was never at 100.

**Reverted for scope:** oum re-homed `0x82634760` (`fill_n<pair<vector<int>,int>>`, retail strides 0x10 and
sits between SetlistMergePanel's blocks) into SetlistMergePanel, a **band3** unit, and then folded
`fill_n<FilePath>`. That measured +3 fns / +544 B on the fork. It is reverted on the fork branch (commit
message gives the reason) and left as a lead, because band3 belongs to another lane.

## 5. Gates

Final code tip `1d9b86152` (rebased on `c055eaf8f`), after a full build equal to A/B leg B
(50,055 / 5,357,080 B):

```
[map-injectivity] OK: 33157 applied rows, 33156 distinct names, injective (+1 enumerated internal-linkage exception(s))
VALIDATE: PASS -- 1663 map-consistent, 282 tolerated (enumerated above), 0 contradicted, 1946 total
[patch-state] OK: tree is a fixed point of 6 post-compile passes
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

Alias memberships carrying a W16-LB record, re-checked on the final tip: **74 / 74 CHASED T1 PROVEN with no
CYCLE or UNDISCHARGED text in their own log blocks** (`~/tmp/w16lb/chase_final.log`), and
`tools/alias_placeholder_slot_audit.py` reads **74 / 74 CLEAN, cycle 0**. The tree-wide audit summary is
CLEAN 4,672, CLEAN-CYCLE 62, LAX-ALSO-FAILS 228; none of the non-CLEAN rows is a membership this branch
added.

⚠ During the first rebase, a VALIDATE run read `REFUSED (exit 2)` on the half-rebased tree, and a
`| tail -1` in the command hid that exit code from the `&&` chain. It was re-run on the settled tree and
read PASS (rc 0). Every gate above was read with its exit code checked directly.

The native gate ran last, on the final code. Only this docs-only commit follows.

## 7. Not done

- **Not merged to main**, per the brief.
- No `bandobj`, `char` or `band3` source edited. The 22 bandobj/char funclets in §2.3 close through a map
  row, and the band3 re-home was reverted (§4).
- No hand edits to `symbols.txt`.
- The permuter was not run, and register-only rows were not ground.

## 6. Rows left, by blocker

At the tip the band is 609 rows / 160,872 B (§2.2). The blocker for each is as recorded by the lane or
fork that read it.

- **Register allocation / scheduling, skipped by brief** (REG_ONLY 57 rows / 24,008 B, plus STACK_REG,
  plus register-only residue left inside other classes after about three spellings): TexRenderer
  `DrawToTexture`, `Part::InitParticle`, TexBlender `DrawShowing`, Utl `BuildVisit`/`UtilDrawPlane`,
  `Vector3Keys::SetFrame`, `VelocityBuffer::Draw`, `Trans::SetWorldXfm`, `Font::Load`/`Kerning`,
  `KerningTable::SetKerning`, `FillCompressedVertex`, `Line::SetNumPoints`, `MeshDeform::AppendWeights`,
  `SkinVertex`, `RndDir::Enter`, `ObjectDir::PreLoad`/`Save`, `UIListDir::BuildDrawState`, the Synapse /
  PeakDetector / PitchDetector / EQEffect / GranularSynth DSP bodies, `~BufStream`, `~VorbisReader`,
  `LoadMgr::PollUntilLoaded`, `DataNetLoader::PollLoading`, `FileLoader::SaveData`.
- **Stack-slot layout, declaration order and scoping inert:** `ExternalMic::sampleProcessThread`,
  `VorbisReader::CheckHmxHeader`, `NetCacheMgr::AddLoaderRef` (retail frame 0x10 larger), `ASCIItoUTF8`,
  `Mesh::CollideShowing`, `Flare::DrawShowing`.
- **Switch lowering / tail merge:** `StreamReceiver::Poll`, `StandardStream::PollStream`,
  `MemcardMgr::ThreadDone`, `NgMat::RefreshState`, `OnToggleHeap`.
- **FP operand order:** `XfmSort`, `UpdateRelativeXfm`, and the inlined `Multiply(Vector3, Matrix3)` in
  `BurnXfm`/`RotateLineVerts`/`InitParticle` (math headers).
- **CYCLE-blocked fold names**, refused by policy: the `_M_fill_insert<Object*>` family (Synth360::Init,
  StandardStream::Init, Synapse ctor), `sort<CuePoint>`, `list::operator=`, `_Rb_tree::clear`
  (`~RndFont`, `UpdateChars`).
- **Spelling already folded into a different survivor** (15 in §3.2's screen, 5 more in fork snd's scope):
  retail keeps two byte-equivalent bodies, and a spelling folds to one survivor.
- **`.bss` offset `gHeaps+0x254`:** `MemInit`, `MemFindAddrHeap`, `MemAllocSize`.
- **PCH input `obj/Object.h` (not edited):** `~Hmx::Object` (retail frees the ring at +0x20 with
  `list::clear`), `TypeProps::Load(BinStream&, bool)`.
- **ObjPtr inline policy:** a TU-wide `RB3_OBJPTR_FORCEINLINE_CTOR` in Mesh.cpp measured +12 fns / +440 B
  (14 rows to 100, `RndMesh::Load` 95.65 → 98.51), but dropped `??0RndMesh` 100 → 89.14, so it was
  reverted. It needs a lever that inlines only `RndBone`'s `ObjPtr<RndTransformable>`.
- **Not opened:** `RndMesh::Load`/`OnSync`, `RndAnimatable::Load`, `NgEnviron::Select`,
  `NgMat::SetRegularShaderConst`, `MeshDeform::Reskin`, `CalculateAO`, `PatchVerts::Add`. These are large
  STRUCT_INSDEL bodies with 40–220 charges each.
- **Anonymous EH funclets** (about 5 KB): they move only when their parent's frame does.

### 6.1 Leads outside this lane's scope

- **band3:** `0x82634760` is `fill_n<pair<vector<int>,int>>` and belongs in SetlistMergePanel (+3 fns /
  +544 B measured on fork oum, reverted here). `0x82687D48` is `vector<SongDB::TrackData>::
  _M_allocate_and_copy`, pinned in TransAnim; it belongs in SongDB's TU. `ButtonHolder`'s
  `_M_allocate_and_copy<ActionRec>` is in Tour's TU, almost certainly `vector<map<int,float>>`.
  `0x827A4F38` is named `Profile::GetName` but returns a `LocalUser*`.
- **char / bandobj:** map row `0x823DD0E0` `??0Symbol@@QAA@XZ` installs `RndOverlay::Callback`'s vtable
  (`0x82054334`), so it is `??1Callback@RndOverlay@@UAA@XZ` (pinned in Waypoint). `0x822E1238`
  (`push_back<Burst>`) pushes a 4-byte two-halfword element; retail `CheckBursts` calls `0x82788308`, which
  the chase proves equal to our `push_back<Burst>`. `0x82397808` is named `__uninitialized_copy<DataEvent>`
  but copy-constructs `CharIKHand::IKTarget`; fixing it frees `0x827ED198` for its DataEvent name.
  `0x822D8D78` (named for `ActionElement`, pinned to InlineHelp) strides 0xC and sits in StreakMeter's TU.
- **net:** `NetLoaderXbox::NetLoaderXbox` (98.13): retail allocates 0x70 for HttpGet against our 0x78 and
  passes one fewer ctor argument (`src/system/net/HttpGet.h`).
- **Shared macro, needs a whole-binary A/B:** `StorePanel::Handle` and `StorePreviewMgr::Handle` are 0x10
  frame short because retail gives `HANDLE_EXPR` temporaries their own stack slot (`obj/ObjMacros.h`).
- **`rndobj/Rnd.h`:** RB3's occlusion mode is raw `Rnd::Mode` 3, but the tree's enum numbers
  `kDrawOcclusion` 4. Fork r1 used a local cast, as `Shader.cpp` already does.
- **RTTI TypeDescriptor names** (§3.4): correct on retail bytes, but a net loss under the no-row-off-100
  rule until the ordering residue they expose is fixed. Not to be re-proposed as a free win.
