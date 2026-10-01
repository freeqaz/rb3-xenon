# W16-MB — band3 / bandobj / char 90–99.99 band: charge-class census and fixes (2026-10-01)

**Branch** `w16-mb`, started off main `87266c1b6` (W16-LB landing), rebased onto main `1a527b256` (W16-MA)
before the forks were merged. **Not merged to main**, per the brief.
**Ruler** `name_check` (graded; `report.json` `provenance.diff_config`).
**Method** W16-LB's (`docs/decomp/W16LB_ENGINE_BAND_90_100_CLASS_CENSUS_2026-10-01.md`), applied to the
directories LB was barred from.

**Population.** Every row with `90 ≤ fuzzy < 100` in a unit whose `objdiff.json` `metadata.source_path` is
under `src/band3/`, `src/system/bandobj/` or `src/system/char/`.

## 1. Class census (main `87266c1b6`)

**887 rows / 293,124 B** (`~/tmp/w16mb/pop.py`, base full build 50,055 fns / 5,357,080 B, equal to LB's A/B
leg B). The brief estimated about 240 KB; the measured band is 293 KB. The difference was not attributed; one
contributor is LB's landing, which moved 22 bandobj/char funclets without touching their source (LB §2.3).

Each row was diffed with `objdiff-cli diff` under the project config (`~/tmp/w16mb/cls.py`, LB's script
unchanged); the diff's `fuzzy_match_percent` equals `report.json`'s on **887 / 887** rows. A row takes the
first class that applies (LB §1's order: STRUCT_INSDEL, OPCODE, IMMEDIATE, STACK_REG, NAME_ONLY, NAME+REG,
REG_ONLY).

Bytes by class and directory (the five band3 columns are `src/band3/<dir>`):

| class | meta_band | bandtrack | game | tour | net_band | bandobj | char | rows | bytes |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| STRUCT_INSDEL | 25,656 | 17,508 | 8,856 | 0 | 1,488 | 46,860 | 30,280 | 156 | 130,648 |
| REG_ONLY | 15,572 | 6,548 | 19,148 | 284 | 5,112 | 8,720 | 16,280 | 106 | 71,664 |
| NAME_ONLY | 8,644 | 1,052 | 6,428 | 3,792 | 84 | 18,744 | 12,012 | 440 | 50,756 |
| IMMEDIATE | 2,320 | 1,264 | 268 | 232 | 120 | 8,404 | 8,164 | 136 | 20,772 |
| NAME+REG | 7,116 | 0 | 772 | 0 | 0 | 272 | 512 | 16 | 8,672 |
| OPCODE | 1,544 | 0 | 16 | 0 | 0 | 4,100 | 1,068 | 22 | 6,728 |
| STACK_REG | 0 | 0 | 1,156 | 0 | 0 | 200 | 2,176 | 8 | 3,532 |
| other (mixed arg kinds) | 0 | 0 | 0 | 0 | 0 | 248 | 104 | 3 | 352 |
| **total** | 60,852 | 26,372 | 36,644 | 4,308 | 6,804 | 87,548 | 70,596 | **887** | **293,124** |

Compared with LB's engine band, REG_ONLY is three times the share (24% of bytes against 11%): band3/game
alone has 19 KB of it, mostly large Poll/Handle bodies.

- **STRUCT_INSDEL**: median 9 charged instructions per row; 92 rows / 59,016 B carry 12 or fewer.
- **NAME_ONLY**: 427 rows / 51,500 B (NAME_ONLY and NAME+REG together) carry exactly one distinct name
  pair. 254 NAME_ONLY rows / 10,772 B are anonymous `fn_` rows, almost all 40–48 B unwind funclets.
- **Name pairs**: 344 distinct pairs. 319 are function-name pairs (both sides a function spelling); 25
  are data labels (literals, `??_R0` TypeDescriptors, vtables, static-local Symbols, save/restore helpers).

⚠ The first pass of the pair filter treated any spelling containing `@@3` as a global variable. Template
back-references produce `@@3` inside function names (`…@stlpmtx_std@@3@…`), so 47 real function pairs were
filed as data. It was caught by printing the "data" list and seeing `??1?$map…` destructors in it; the
filter now anchors the global-variable form at the start of the name.

## 2. Results

### 2.1 Whole-branch A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-mb-ab --patch <git diff be0cae238..w16-mb, docs excluded>`.
- **Worktree:** fresh, `setup_worktree.sh`, at `be0cae238` = main `1a527b256` plus the branch's one
  `symbols.txt` line (`fn_82356300` size 0x4 → 0x58, fork bo's carve of `ReleaseSmasherPlate`).
  `ab_measure` refuses a patch that touches `symbols.txt`, so leg A carries it (W16-HZ / W16-LA's recipe).
- **Patch:** 37 files; kinds map + source + splits.
- **objdiff-cli:** sha `c1b7d952`, stable across legs.
- **Both legs** were read at a split fixed point (0 extra re-splits). Leg B made 455 recompiles, one split,
  renamer patched 1,854.
- **Run dir:** `~/tmp/w16mb/ab_run_final/` (copy of
  `~/tmp/wt-w16-mb-ab/.ab_measure_runs/20261001-180410-w16-mb-branch-final-68192/`).

```
leg A: matched=50064 masked=24403 honest=25661 code%=52.295630  (recompiles: 0, settled)
leg B: matched=50194 masked=24408 honest=25786 code%=52.587612  (recompiles: 455, split=1, patch_steps=7, settle iterations: 2)
Δmatched=+130  Δmasked_equal=+5  Δhonest=+125  Δcode%=+0.291982pp  Δcode_bytes=+29920
Δfuzzy=+0.023397pp   (legA 59.283173 -> legB 59.306570)
units at 100% [mpn ruler]: legA 401 -> legB 408  (Δ+7; 9 reached 100, 2 fell off)
units at 100% [all-rows-fuzzy ruler]: legA 346 -> legB 352  (Δ+6; 8 reached 100, 2 fell off)
[control none] Δmatched_code=+10224 B -- NOT_APPLICABLE (source in patch)
```

**Prediction, written before the run:** +130 fns / +29,920 B, the sum of the in-tree steps on this base
(5 rebased lane commits read 50,118 / 5,373,132 B against main's 50,064 / 5,358,888 B, then §3.7 +7 / +1,324 and
the four merges). **Measured: exactly that.** Leg A reads exactly main's figures, so the bare carve in leg A
moved nothing.

An earlier run of the same protocol on the tip before §3.8 also measured +130 / +29,920 B; §3.8 is byte-neutral.

**Row-level diff of the archived legs**, keyed by retail address (`rowdiff.py`): **238 rows up, 226 to 100; 4
down; 0 rows off 100 on the fuzzy ruler and 0 on the `mpn` ruler.** The four rows down, none from 100:
- `Character::PostLoad` 99.694 → 99.647 (fork ch, §4);
- `ProfileMgr::GetJoypadExtraLagInits` 99.91 → 97.28 (fork mb's behaviour fix, §4.1);
- funclets `0x82642AA0` / `0x82642AC8`, 99.4 / 99.9 → 99.3 (re-paired after their island moved, §4.1).

The two "units fell off 100" are BandStorePanel and SetlistToStorePanel, which gained re-homed rows (130 → 139
and 25 → 30); no row in them fell. The other unit "regressions" in the tool's list (CharClip −5, NetGameMsgs −2,
OriginalChoreoRemixer −2, StreakMeter −2, …) are re-homes moving rows out of those units.

### 2.2 The band itself

Every census row (main `87266c1b6`, 887 rows / 293,124 B), keyed by retail address, read at leg A (main
`1a527b256`, so W16-MA's effect is separated) and leg B:

| class at census | rows | bytes | at 100 already on main `1a527b256` | to 100 (rows) | to 100 (bytes) | rose, not to 100 | fell |
|---|---:|---:|---:|---:|---:|---:|---:|
| STRUCT_INSDEL | 156 | 130,648 | 0 | 16 | 5,068 | 3 | 1 |
| REG_ONLY | 106 | 71,664 | 0 | 1 | 152 | 0 | 0 |
| NAME_ONLY | 440 | 50,756 | 13 | 160 | 18,608 | 4 | 1 |
| IMMEDIATE | 136 | 20,772 | 0 | 5 | 1,208 | 1 | 2 |
| NAME+REG | 16 | 8,672 | 0 | 1 | 512 | 1 | 0 |
| OPCODE | 22 | 6,728 | 0 | 1 | 672 | 0 | 0 |
| STACK_REG | 8 | 3,532 | 0 | 0 | 0 | 0 | 0 |
| other | 3 | 352 | 0 | 1 | 44 | 0 | 0 |
| **total** | 887 | 293,124 | 13 | **185** | **26,264** | 9 | 4 |

At the tip the band holds **690 rows / 266,672 B** (STRUCT_INSDEL 141 / 126,028 · REG_ONLY 105 / 71,512 ·
NAME_ONLY 268 / 31,608 · IMMEDIATE 130 / 19,468 · NAME+REG 14 / 7,980 · OPCODE 21 / 6,056 · STACK_REG 9 / 3,712 ·
other 2 / 308). Rows that rose from below 90 have entered it. The whole-binary +29,920 B is larger than the
band's own +26,264 B because map and alias repairs close rows outside the band too.

### 2.3 Step ledger

| step | Δfns | ΔB | rows up (to 100) / down | measured against |
|---|---:|---:|---|---|
| §3.1 + §3.2 82 folds, SetlistMergePanel row | +48 | +13,236 | 123 (115) / 0 | main `87266c1b6` |
| §3.3 CharUpperTwist vtables, Callback dtors | +2 | +708 | 12 (12) / 0 | previous step |
| §3.4 SongDB, `0x8235F6D0` re-homes | +4 | +288 | 5 (4) / 0 | previous step |
| §3.5 `0x825971F0` thunk | +0 | +444 | 10 (10) / 0 | previous step |
| *rebase onto main `1a527b256`* | | | | |
| §3.7 Tour `vector<set<Symbol>>` | +7 | +1,324 | 16 (12) / 0 | rebased tip without it |
| merge gt | +9 | +1,432 | 9 (9) / 0 | previous step |
| merge bo | +22 | +5,988 | 31 (29) / 0 | previous step |
| merge ch | +20 | +5,152 | 28 (26) / 1 explained | previous step |
| merge mb | +18 | +1,780 | 19 (19) / 3 explained | previous step |
| §3.8 `_M_clear` membership | +0 | +0 | 1 (0) / 0 | previous step |
| **A/B, base main `1a527b256` + carve** | **+130** | **+29,920** | 238 (226) / 4, 0 off 100 | |

The first four steps sum to +54 / +14,676 B on the old base and measure +54 / +14,244 B on the new one
(50,118 − 50,064; 5,373,132 − 5,358,888). The 432 B gap is W16-MA's overlap: it had already named the two
Callback rows, so some of §3.3's funclets were at 100 on the new base before this lane touched them.

## 3. Main-lane work (global map and alias files)

Each step was measured with a full `./tools/ninja-locked` build and a whole-report row diff keyed by retail
address (`~/tmp/w16mb/rowdiff.py`, LB's) against the previous step.

### 3.1 82 chase-proven fold memberships — 115 rows to 100, +13,236 B (with §3.2)

All 319 function-name pairs went through `tools/icf_pair_adjudicate.py --pairs --chase`
(`~/tmp/w16mb/adj_all.log`): **133 CHASED T1 PROVEN, 186 REFUTED**. Admissible = PROVEN with no `CYCLE` or
`UNDISCHARGED` text in the pair's own log block: **97**. LB's installer (`~/tmp/w16mb/install.py`) then
screened them:

| screen | refused |
|---|---:|
| spelling already a member or survivor of another group | 12 |
| the map places the spelling at an address of its own | 3 |
| installed | **82** |

15 of the 82 override older withdrawal records for the same spelling under a *different* survivor (mostly
`FABRICATED_CLOSURE_NOT_PARTITION`, one `CALLEE_SIZE_MISMATCH`, one `FIXPOINT_ROOT_DIFFERS`); each admitted
record names them. Those withdrawals refused membership without a per-pair proof against the survivor in
question; the chase is one.

`tools/alias_placeholder_slot_audit.py`: 82 / 83 of this branch's memberships CLEAN, 1 NOT-ALIGNED:
`__find<CharKeyHandMidi::KeyboardKey>` into group `0x823E1060`, whose two existing members read NOT-ALIGNED
too. Its chase is PROVEN through the tool's RETAIL-TAIL-PAD rule (retail 176 B including a 4-byte zero tail,
ours 172 B), which `--self-break-tailpad` guards; the size gap is what the slot audit cannot align.

### 3.2 `0x82634760` re-applied: `fill_n<pair<vector<int>,int>>`, homed in SetlistMergePanel

W16-LB's fork oum made this change and measured it (+3 fns / +544 B, 0 down), then reverted it (`8f0fd923e`)
only because SetlistMergePanel is a band3 unit. This branch reverts that revert (`1050a57cb`). Its bytes are
inside §3.1's measurement.

### 3.3 CharUpperTwist's vtables and `RndOverlay::Callback`'s destructors — +2 fns / +708 B, 12 rows to 100

- `0x82050164` / `0x820501BC` were named `FlowIf` vtables. `tools/retail_rtti.py vtable` reads both as
  `.?AVCharUpperTwist@@`; retail has no `FlowIf` COL. `??0CharUpperTwist` (`0x823C6C18`) installs exactly
  these two (`retail_rtti.py installs`).
- `0x823DD0E0` was named `??0Symbol@@QAA@XZ`. Its body is `lis/addi/stw/blr` storing vtable `0x82054334`
  (`.?AVCallback@RndOverlay@@`). Its 10 callers are the unwind funclets and destructors of the five classes
  deriving from `RndOverlay::Callback` (CharacterTest, Rnd, Synth, Song, VocalPlayer), so it is
  `~Callback`. Waypoint.obj, its old home, cannot define that name; the 16-byte block moves to
  CharacterTest's entry, the adjacent TU, whose object does.
- `0x823DD0F0`, named `??_GFitnessFilter`, is slot 0 of the same Callback vtable and installs it before
  `operator delete`: `??_GCallback@RndOverlay@@UAAPAXI@Z`. Retail has no FitnessFilter class. The row was
  already at 100 under the wrong name, because the vtable it loads was an unnamed (forgiven) placeholder.

Prediction: CharUpperTwist's ctor, 10 funclets and the dtor row. Measured: exactly those 12 rows to 100,
0 down.

After the lane was rebased onto main `1a527b256`, W16-MA turned out to have renamed `0x823DD0E0` /
`0x823DD0F0` to the same two Callback names independently (from retail RTTI), and it recorded the Waypoint
row falling to 0 as an open re-home lead (W16-MA §4). On the rebased base this commit's effective change is
the two vtable rows and the CharacterTest re-home, which closes that lead.

⚠ The rest of FitnessFilter's pinned span (`0x823DD140`–`0x823DD550`) is `ObjPtr` load/copy helpers
(one sets an `ObjPtr<BandCharacter>`) sitting immediately before `CharacterTest::Save`. That fits
CharacterTest's TU, not a gesture class, but was not proven. Those rows are anonymous and unpaired either
way; they were not moved.

### 3.4 Two re-homes — +4 fns / +288 B, 4 rows to 100

- `0x82687D48` (W16-LB lead) already carried `vector<SongDB::TrackData>::_M_allocate_and_copy` but was
  pinned in TransAnim and read 0. It sits between SongDB's own blocks, and SongDB.obj defines the name.
- `0x8235F6D0` (W16-LB lead) was named `vector<ActionRec>::_M_allocate_and_copy` in ButtonHolder. Retail
  allocates 0x18-byte elements (`mulli r3,r4,0x18`; ActionRec is 0x14 and every other ActionRec helper
  matches at that size), and its one caller (`0x8235FAA4`) is inside `vector<map<int,float>>::operator=`.
  Renamed and moved, with its catch funclet `0x8235F73C`, into CharClip's entry beside the rest of that
  vector's helpers. LB's lead had guessed this type ("almost certainly `vector<map<int,float>>`").
  ⛔ **That element type was wrong, and §3.7 corrects it.** `set<Symbol>` and `map<int,float>` are both
  0x18 bytes, so the allocation size cannot tell them apart; the row read 100 only because its callees'
  names were forgiven. The prediction rested on the one fact the two candidates share.

The first build of this step stopped at the split guard ("THE SPLIT REWROTE ITS OWN INPUT"): the split
re-derived the moved blocks' `.pdata`. That is the documented one-build recovery; the re-derived lines are
committed with the step.

### 3.5 `0x825971F0` is a folded `~map` thunk — +444 B, 10 funclets to 100

Retail `0x825971F0` is `b 0x827690D0`, and `0x827690D0` is an `_Rb_tree` clear body. It was named
`__ucopy_aux<_Slist_node_base**>`, which would be a copy loop. Its 35 call sites are unwind funclets that
destroy maps of twelve key/value types, all reducing to that one tail call. It is now named for the
most-used spelling, `~map<int,float>` (8 sites), and listed in `_icf_arbitrary`. `0x827690D0` is left
unnamed on purpose: its 29 direct callers are forgiven as a placeholder target today.

Prediction "up to 8 rows"; measured **10** to 100 and 0 down, `matched_functions` +0 (`mpn` already excluded
the charge). The two extra rows are funclets whose byte-signature pairing changed once the name existed.

### 3.7 `0x8235CA80`–`0x823600D0` is Tour's `vector<set<Symbol>>` family — +7 fns / +1,324 B, 12 rows to 100

Lead from fork mb. The helpers in this span were named for `vector<map<int,float>>` (and for `LeaderboardRow`,
`ActionRec`, and DC3's `set<const MoveParent*>`) and pinned to six unrelated units. On retail bytes they are
one `vector<set<Symbol>>` instantiation, the member of `SongSortMgr::SongFilter`:
- the node-creating callee `0x8235C328` does `li r3,0x14` and copies one word to +0x10: a 0x14-byte node with
  a 4-byte key. A `map<int,float>` node is 0x18 and holds a pair;
- the bodies call `_Rb_tree<Symbol,…,_SetTraitsT>::clear` (`0x822DEA78`) and `_Copy_Construct<set<Symbol>>`
  (`0x8235CB88`);
- the blocks interleave Tour's own functions and `SongFilter`'s ctor, dtor and `operator=`.

Tour.obj defines the whole family. 14 rows were renamed to the set<Symbol> spellings taken verbatim from
Tour.obj's symbol table (one, `0x8235F370` `__uninitialized_copy`, had been unnamed) and 16 `.text` blocks moved
to Tour. Five alias groups follow the map; where the set<Symbol> spelling had been folded into a wrong-type
survivor, that membership is withdrawn as `MAP_NAME_WRONG`.

Two rows fell off 100 on the first build and were fixed before committing, one level at a time:
`__uninitialized_move` (100 → 99.75) charged retail's `_Rb_tree<const MoveParent*>` move ctor
`0x8235CA80`, whose four callers are all inside this family, so it was renamed and re-homed too; that move ctor
then read 99.76 against retail's `_Rb_tree_base<pair<const int,float>>` move ctor, an element-type-independent
base ctor. `_Rb_tree_base<Symbol>`'s move ctor folds into group `0x8235C608` (flat T1 PROVEN, CHASED T1 PROVEN,
no CYCLE, 104 B both sides).

Measured against a full build of the same tree without the commit: **+7 fns / +1,324 B; 16 rows up (12 to
100), 0 down.** Four rows remain at 99.79–99.92; their two charges (`_Rb_tree<TrackWidget*>` copy ctor, and
`_Copy_Construct` vs `_Param_Construct<set<Symbol>>`) chase PROVEN only with CYCLE-ASSUMED `_M_copy` leaves, so
they are not admitted.

The CharClip.h scaffold (`template class std::vector<std::map<int,float> >`, which says its donor TU is
unidentified) is left in place. This span was its main target; removing it is a separate change to measure.

### 3.8 A membership invalidated by §3.7

The final chase re-check (§5) read one W16-MB membership REFUTED: `_M_clear<vector<map<int,float>>>`, admitted
in §3.1 against the *old* survivor `_M_clear_after_move<vector<map<int,float>>>`. With the survivor corrected to
the set<Symbol> spelling, the bodies still match but the relocation targets do not. It is withdrawn
(`CHASE_REFUTED_AFTER_SURVIVOR_RENAME`) and `_M_clear<vector<set<Symbol>>>` is admitted instead (flat T1
PROVEN, 116 B both sides). Full build: +0 B; `vector<set<Symbol>>::operator=` 99.81 → 99.88; 0 down.
⚠ A map rename can silently invalidate a membership proven against the old survivor name. Re-chasing every
membership on the final tip is what caught it, not the validator: `--validate` passed both before and after.

### 3.9 Leads examined and not taken

- **Static-local Symbol labels** (`SaveLoadManager::SetState` 4,096 B, `AssetMgr::GetTypeFromName`,
  `SessionMgr::OnMsg`, `ProfileMgr::Check*Status`, `CharacterCreatorPanel::GetHair`): retail's `.bss` label
  is unnamed and ours is the static local. Each row also carries 6–22 register charges. Naming the label
  changes no code, so it cannot bring these rows to 100.
- **Float literals** (`CharBones::StringVal`, `VocalPart::GetNoteSliceWeight`, `UpdateMinMaxPitch`):
  `tools/retail_body.py` shows retail's unnamed `lbl_` addresses hold exactly our constants. Naming them is
  correct but each row has register residue too; LB §3.4 measured a net loss from naming data labels.
- **`0x828043A8`** (`__destroy_aux<LocalePanel::Entry>`): a 4-byte `b list::clear` thunk with 150 callers.
  The 7 band rows calling it are funclets paired by byte signature with unrelated `~DataNode` funclets on our
  side (LB §3.9's class). Renaming it gains nothing and puts 150 sites at risk.
- **`0x827A4F38` `Profile::GetName`**: retail's body tail-calls `UserMgr::GetLocalUserFromPadNum(mPadNum)`,
  and our source already reproduces it with a cast. Its row, `Profile::Handle` and the band3 callers are at
  100; the one caller below 100 (`MemcardMgr::OnMsg`, 99.88) is `system/meta`. No bytes to gain here.
- **`StorePanel::Handle` / `StorePreviewMgr::Handle`** (`HANDLE_EXPR` frame): neither row is in this band.

## 4. Forks

Four forks ran in their own `setup_worktree.sh` worktrees on disjoint directories, branched off lane tip
`f1d718de6` (after §3.1). Each measured every change with a full build and a whole-report row diff, and
committed only changes with no row falling off 100. Each was rebased onto the lane (`git rebase --onto
w16-mb c0d805dcb`, after the lane itself was rebased onto main `1a527b256`), rebuilt, row-diffed against the
lane's previous build, and merged with `git merge --no-ff`. **The merge-time measurement equals each fork's
own report exactly.**

| fork | scope | merged Δ | rows up (to 100) / down |
|---|---|---|---|
| gt | band3 bandtrack, game, tour | +9 fns / +1,432 B | 9 (9) / 0 |
| bo | bandobj | +22 fns / +5,988 B | 31 (29) / 0 |
| ch | char | +20 fns / +5,152 B | 28 (26) / 1 explained |
| mb | band3 meta_band, net_band | +18 fns / +1,780 B (measured by the lane at merge) | 19 (19) / 3 explained |

Census at each fork's base (`f1d718de6`, after §3.1's folds), their own scope:

| fork | rows | bytes | STRUCT_INSDEL | REG_ONLY | NAME_ONLY | IMMEDIATE | other classes |
|---|---:|---:|---:|---:|---:|---:|---:|
| gt | 146 | 65,004 | 26,364 | 25,980 | 8,952 | 1,764 | 1,944 |
| bo | 261 | 82,136 | 46,860 | 8,720 | 13,332 | 8,404 | 4,820 |
| ch | 180 | 66,744 | 30,280 | 16,280 | 8,160 | 8,164 | 3,860 |
| mb (at the lane's census base `87266c1b6`; the fork stopped before reporting its own) | 214 | 67,656 | 27,144 | 20,684 | 8,728 | 2,440 | 8,660 |

**The one row that fell** (fork ch): `Character::PostLoad` 99.694 → 99.647, never at 100. Retail addresses
PostLoad's statics off `gRevs`; after the Lod reader's owner pointer became its own file static (which took
that reader to 100), ours addresses them off the pointer. Splitting all three statics apart made it worse
(the Lod reader and PreLoad fell off 100), so that was not applied.

**Merge conflict** (bo into the lane after gt): `0x822CD8C0` / `0x822CD918`. gt had renamed `0x822CD8C0` to
`BandStarDisplay::SyncObjects` (vtable `0x82021094` slot 3); bo's side still carried the old wrong name
there and had renamed `0x822CD918` to `~ObjPtr<BandStarDisplay>`. Main's W16-MA had named `0x822CD918`
`~ObjRefConcrete<BandStarDisplay,ObjectDir>` from retail RTTI and listed `ObjPtr<BandStarDisplay>`'s dtor as
its true identity, leaving it open only because our build emitted neither spelling (W16-MA §4). bo's source
change makes our build emit `~ObjPtr<BandStarDisplay>`. Resolved as gt's `0x822CD8C0` and bo's `0x822CD918`.

### 4.1 Fork work, by kind

**Behaviour bugs fixed** (each on retail bytes):
- `MicInputArrow` started every arrow marked connected; retail pushes `false` into `mConnectedFlags` (bo).
- `LightPreset::LegacyFadeIn` was a stub returning 0.0f, so the MIDI-preset cleanup fade was always zero;
  it is the inline accessor for the field at +0x70 (bo).
- `CharDriver::PlayGroup(name)` plays `grp->GetClip()` directly; `Starved` keeps its result in a bool (ch).
- `Character::SyncObjects` has no `IsSubDir` test in retail (ch).

**Levers named in the brief:**
- `const bool`: fork gt found a new tell. Retail returns a compare result straight from `adde` into `r3`
  where ours adds `clrlwi r3,rX,24`; a `const bool` local closed `GemTrainerPanel::ShouldMissCauseFail`
  (97.08 → 100). A scan of gt's scope found no other instance. LB §3.7's two tells (`clrlwi`-vs-`li` on one
  line, and an inserted materialisation) were also tried where they appeared: `CheckRemoveChordBracket` was
  inert.
- `static const float`: no fork reported a row with its tell (retail reloading a `__real` constant per
  compare where ours saves it in an FPR), and none applied it. This lane did not run a dedicated scan.
- ObjPtr inline policy (LB §6's open lead): fork ch closed it per TU — `RB3_TU_OBJPTR_FORCEINLINE_CTOR`
  in CharWeightSetter (Load 92.81 → 100, all 60 unit rows scanned, nothing else moved) and the owner-only
  ObjOwnerPtr ctor in CharBonesMeshes (`resize` 33.67 → 100). In CharHair the same lever took the ctor to
  99.92 but made `Point`'s ctor provably nothrow, which elided EH frames in `PropSync<Point>` and
  `ObjVector<Point>::resize` and dropped both off 100; reverted.

**Map and pin repairs from the forks:** `GemTrackResourceManager::ReleaseSmasherPlate` (`0x82356300`, 88 B),
carved and named as a 4-byte allocator copy ctor (bo; the carve is the branch's one `symbols.txt` hunk); BandStarDisplay's `ObjVector` COMDATs (mis-typed, in StreakMeter);
three MeshAO COMDATs (named SongPattern, in SongLayout); `vector<OutfitConfig::Overlay>` helpers (named
SampleMarker); `vector<ChordShapeGenerator::Edge>::push_back` and its overflow (named Burst, in CharLipSync —
main's lead, released to ch); `ObjList<PracticeSectionMapping>` / `<ContentPoolMapping>` readers;
`resize<TrackerPlayerDisplay>` / `resize<BandFaceDeform::DeltaArray>` (names swapped); `TourDescPanel::Load`;
`~BandHeadShaper`; `BandDirector::Terminate`. Every alias group whose survivor was the wrong name follows the
map with a `survivor_renamed` record, and a real spelling that had been folded into it is withdrawn as
`MAP_NAME_WRONG`. Nothing was pruned.

**Fork mb** stopped at its turn limit right after rebasing onto the lane, before its final measurement and
report; the lane measured its merge itself (+18 fns / +1,780 B, 19 rows to 100, 0 off 100). Its commits each
carry their own full-build measurement:
- `AccomplishmentGroupProvider::GetCareerLevel` reads `t[6]` (97.84 → 100, +556 B);
  `OpenGateData::GetWaitingUsers` is a plain indexed loop (93.44 → 100); `MusicLibrary::IsPurchasing` returns
  whether the store holds a purchaser (28.0 → 100); `_Temporary_buffer<Symbol*,Symbol>`'s unwind frees through
  `MemFree` (funclet `0x825595BC` to 100).
- **Behaviour fix, net-negative on its own row:** `ProfileMgr::GetJoypadExtraLagInits` returns 14.0f on the
  PS3/Wii 22-fret video-calibration arm (retail `0x82546020`: `lfs 0x41600000`), not 74.0f. The row was never at
  100 and falls 99.91 → 97.28: retail keeps that arm's `lis/lfs/blr` separate from an identical default-arm
  tail, and our compiler now cross-jumps the two. Kept because the value was wrong.
- map+splits: `ViewSetting::Reset`, `PatchSelectPanel::Draw`, `StoreInfoPanel::Enter`, the
  `CharacterCreatorPanel`/`TexLoadPanel` `FinishLoad` overrides, three deleting destructors named for their own
  classes, `??_DInstarank`, and seven islands homed in the TUs that define them. One island,
  `SetlistToStorePanel::StartMetadataLoaders` (no body in our source yet), moved with two 40-byte funclets that
  re-pair by byte signature in the new unit at 99.3 (were 99.4 / 99.9, never 100). It also makes
  BandStorePanel and SetlistToStorePanel stop being "units at 100": they gained rows, none lost one.
- The lead behind §3.7 (Tour's `vector<set<Symbol>>` family).

## 5. Gates

Final code tip `c07171779`, after a full build equal to A/B leg B (50,194 / 5,388,808 B). Every exit code read
directly, not through a pipe:

```
[map-injectivity] OK: 33164 applied rows, 33163 distinct names, injective (+1 enumerated internal-linkage exception(s))
VALIDATE: PASS -- 1697 map-consistent, 279 tolerated (enumerated above), 0 contradicted, 1977 total
[patch-state] OK: tree is a fixed point of 6 post-compile passes
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

Alias memberships carrying a W16-MB record (lane and forks), re-checked on the final tip:
**81 / 81 CHASED T1 PROVEN with no CYCLE or UNDISCHARGED text in their own log blocks**
(`~/tmp/w16mb/chase_final.log`). `tools/alias_placeholder_slot_audit.py`: 80 CLEAN, 1 NOT-ALIGNED (the
`__find<KeyboardKey>` tail-pad membership, §3.1). Tree-wide: CLEAN 4,753, CLEAN-CYCLE 61, LAX-ALSO-FAILS 228,
NOT-ALIGNED 16; none of the non-CLEAN rows except that one is a membership this branch added.

The native gate ran on the final code tip, in parallel with the A/B. Fork bo replaced
`BandCharacter::Terminate`'s `__asm nop` with an empty `__asm {}`; the native build compiles it.

## 6. Rows left, by blocker

The blocker for each is as recorded by the lane or the fork that read it.

- **Register allocation / scheduling, skipped by brief:** REG_ONLY 105 rows / 71,512 B at the tip, three
  times LB's engine share. The large rows: band3/game Poll and Handle bodies, `TrackPanel::Poll`, `Gem::Poll`,
  `VocalPlayer::Poll`, `CharDriver::Poll` (r29/r30), `CharEyes`, IK `SimulateInternal`, `OnFileLoaded`,
  `VocalTrackDir::PostLoad`, `SetVenueDir`, `GemManager` ctor store order, `ThreadProcessOneFrame`.
- **Measured inert or worse** (fork gt): `Stats::EndMultiplier`, `BuildScrollingDeployZones` (strength-reduction
  base +0x2D4 vs +0x128), `CheckRemoveChordBracket`, `IsEndOfFill`, `BandUser::SetChar`, `HandleExitExtent`.
- **FP operand order** (fork ch): `GetRadius` (5 spellings), `CalcBoundingSphere`, `MeasureLengths`.
- **Static-local placement:** `ReplaceRefs`, `BuildChordMesh`, `BuildContourCap` (bo); `Character::PostLoad`
  (ch, §4); the static-local Symbol rows in §3.9.
- **CYCLE-blocked fold names**, refused by policy: `sort<VocalPart*>` (`HandlePhraseEnd`, 2,332 B), the
  `_M_fill_insert<Object*>` family, `_Rb_tree::clear`, `insert<RndTransformable*>`, `resize<Vert>` /
  `<MirrorOp>`, and the two `_Rb_tree` copy-ctor pairs holding four Tour rows at 99.79–99.92 (§3.7).
- **Spelling already folded into a different survivor:** `_M_erase<ObjOwnerPtr<Waypoint>>` charges on
  StreakMeter and `SetupStars` (bo).
- **`obj/Object.h` (PCH input, not edited):** `SetPreFrame`'s `clrrwi` node cast belongs in `ObjPtrList`.
- **CharHair ctor:** the ObjPtr inline lever elides EH frames in two other rows (§4.1).
- **Anonymous EH funclets:** about 40 B each; byte-signature pairings, not defects (LB §3.9).

### 6.1 Leads outside this lane's scope

- **CharClip.h scaffold:** `template class std::vector<std::map<int,float> >` exists only to donate bodies to
  retail's "vector<map<int,float>>" helpers. §3.7 showed this span's helpers are Tour's `vector<set<Symbol>>`;
  whether any retail address still needs the scaffold should be re-measured before deleting it.
- **SongLayout (hamobj) pin** (bo): more SongPattern-named COMDATs inside bandobj/char ranges
  (`~vector<SongPattern>` 19%, `__destroy_range_aux` 39%, `MoveReplacer` copies 86%), likely mis-typed as the
  MeshAO ones were.
- **OriginalChoreoRemixer's remaining two blocks** (`0x823873F8`, `0x82387EE0`): another 4-byte-key
  `vector<set<…>>` family named for DC3's `MoveParent`, which RB3 does not have.
- **`HamListRibbonDrawState`** (gt): four map rows named for a class with no retail RTTI (`0x8239C508`,
  `0x826F7CC8`, `0x82773920`, `0x827742B0`); two are called from Singer's `_M_fill_insert<SingerResultsData>`.
- **`0x82596580`** (gt): `GetIdentifyingToken@InternalSavedSetlist` is `return *(Symbol*)(this+0x30)`; the
  chase reads the fold as VACUOUS.
- **`SongSectionController.cpp`** (ch): `gRev`/`gAltRev` should be file statics; retail's Load addresses both
  off one base.
- **CharLipSync `0x82349B10` / `0x82348E20`** (ch): a fill_insert/overflow pair for a 1-byte non-POD element,
  not `vector<unsigned char>` or `Generator::Weight`.
- **`FileMerger::Merger::operator=`** (bo): ours copies a byte at +41 that retail's does not.
- **`BandDirectorStubs.cpp`** (bo): `LightPreset::StaticResetEvents` is still a volatile stub.

## 7. Not done

- **Not merged to main**, per the brief.
- No hand edits to `symbols.txt` by the lane; the one carve is fork bo's, measured with its name fix.
- The permuter was not run, and register-only rows were not ground.
- No dedicated scan for the `static const float` tell (§4.1).
- `src/system/obj/Object.h` and `obj/ObjMacros.h` were not edited.
