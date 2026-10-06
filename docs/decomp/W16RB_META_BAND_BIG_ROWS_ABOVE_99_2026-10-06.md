# W16-RB — large meta_band rows above 99: one to 100, two moved, eight recorded

**Tree:** worktree `~/tmp/wt-w16rb`, branch `w16-rb`, off main `7410935ea`.
**Ruler:** graded `name_check` (`objdiff.json` options = `report.json` `provenance.diff_config`),
read with `bin/objdiff-cli diff` after a full `./tools/ninja-locked`.

The brief: bring the large band3 meta rows that sit above 99 to 100 by fixing source
against retail bytes. The named rows were SetState, MaybePublish, BuildList and
BandWardrobe::SyncProperty. After that came every other `src/band3/meta_band` row
≥ 99 and ≥ 1 KB, found from `report.json` keyed on the unit's `source_path` in
`objdiff.json`.

## 1. Results

| row | size | before | after | what did it |
|---|---:|---:|---:|---|
| `?SyncProperty@BandWardrobe@@` | 2,416 | 99.743 | **100.0** | `ForceBlink` copies the name to a local `Symbol` (§2.1) |
| `?MaybePublish@UIStats@@` | 2,604 | 99.594 | 99.919 | implicit `DataArray*`→`DataNode`; `base == write`; local arm first (§2.3) |
| `?BuildList@StoreOfferProvider@@` | 2,536 | 99.235 | 99.685 | named `DataNode` for the non-localized shortcut (§2.2) |
| `?SetState@SaveLoadManager@@` | 4,096 | 99.951 | 99.951 | not moved: register coloring only (§3.1) |
| `?Handle@CustomizePanel@@` | 5,036 | 99.921 | 99.921 | not attempted: one `clrlwi`, closed by W16-CG (§3.4) |
| `?ParseDataResultsIntoSetlists@MusicLibraryNetSetlists@@` | 1,968 | 99.533 | 99.533 | not attempted: r28/r29 only, W16-PF swept it today (§3.4) |
| `?SelectRandomVenue@MetaPerformer@@` | 1,924 | 99.958 | 99.958 | not attempted: 2 `Node(i)` add orders, W16-PF swept it today (§3.4) |
| `?FillExpandedDetails@NextSongPanel@@` | 1,640 | 99.756 | 99.756 | not attempted: one dead `stfs`, W16-PR measured it today (§3.4) |
| `?DoesOfferMatchFilter@SongSortMgr@@` | 1,416 | 99.421 | 99.421 | 6 spellings inert (§3.2) |
| `?OnMsg@OvershellSlot@@` (ButtonDownMsg) | 1,396 | 99.427 | 99.427 | 3 spellings, none better (§3.3) |
| `?ShowData@Leaderboard@@` | 1,348 | 99.955 | 99.955 | not attempted: `mulli`/`extsw` temp registers (§3.4) |

Whole-binary A/B: **+5 functions (+1 honest) / +2,664 B / +0.025990 pp**. See §4.

## 2. What moved

### 2.1 `BandWardrobe::SyncProperty` → 100

All 12 charges were one shape, 4 times: the four `playerN_force_blink` handlers
inline `ForceBlink(n)`, and in each one retail loads `mCurNames` straight into the
argument register (`lwz r5,-0x58(r25)`, then `lwz r4,N(r5)`), where we loaded it
into r11 and copied it (`mr r5,r11`).

The in-source record called this "permuter-class, do not re-fund". It said the
shape was shared with `SyncVignetteInterest` and `SyncEnableBlinks` and that
binding a reference was inert. But **both of those helpers are now at 100**. They
use `Symbol name = mCurNames->names[i]; FindTarget(name, *mCurNames);`, and
`ForceBlink` still passed `mCurNames->names[i]` directly as the argument. Giving
`ForceBlink` the same spelling closed every charge: 99.74338 → 100.0, and the size
went from 2420 to 2416. The comment now records the working spelling.

### 2.2 `StoreOfferProvider::BuildList` 99.24 → 99.68 (25 charges → 2)

Retail passes the non-localized shortcut's `DataNode(const char*)` to
`DataArray::Insert` by its recomputed frame address (`addi r5,r31,0xa0`). The
explicit temporary passed the ctor's returned `this` instead (`mr r5,r3`). Naming
the node fixed that site. It also moved every later temp in the loop to retail's
slot, and the scheduling of the inlined `Element` ctor's `stb` stores at idx
507–511 resolved with it.

The two charges left are the stray `stw r11,0x50(r31)` / `stw r11,0x58(r31)` temps
at the prev/next chunk-path tests, which W17-W78 recorded. Nine condition spellings
were measured again on the 2-charge tree; the results are in the source next to
the test:

| spelling | fuzzy |
|---|---:|
| `const String &` bound first | 98.96 |
| `*c_str() && c_str()`, `c_str()[0] != 0 && …` | 99.53 |
| `!= NULL`, nested ifs | 99.68 (identical to the current form) |
| hoisted `char *p`: `*p != 0 && p`, or `if (*p) if (p)` | 99.51 (the pointer test is deleted) |
| `strcmp(p, "") != 0 && p`, `strlen(p) != 0 && p` | 96.5 / 96.8 (inline loops) |

### 2.3 `UIStats::MaybePublish` 99.59 → 99.92 (63 charges → 53)

* **The row is no longer alias-gated.** W16-EI closed it because of two charged
  call sites, idx 246 (`GetBandUsers` vs survivor `GetContainerName@MemcardXbox`)
  and idx 249 (the `vector<BandUser*>` vs `vector<int>` copy ctor). Both read
  *equal* on today's tree because those memberships are now in
  `symbol_aliases.json`. The row can be collected by source work.
* **Implicit conversion for the static's argument.**
  `static Message msg("exit_stats", new DataArray(0));` goes through
  `DataNode(DataArray*, DataType = kDataArray)` as a converting ctor. Retail
  passes that temp by frame address (`addi r5,r31,0x78`). The explicit
  `DataNode(…, kDataArray)` passed the returned `this` and cost an extra `mr`.
  Size is now 2604, the same as retail. This is the same mechanism as §2.2, at a
  site where a named local is impossible.
* **Pad-log compare.** Retail loads `mPadLogWritePtr` before `mPadLogBuffer` and
  compares `cmplw base, write`. Declaring `write` first and writing `base == write`
  cleared 3 charges.
* **The "6-register rotation" was not a register difference.** W16-EI recorded
  idx 263–268 as a rotation `r22→r20→r18→r22, r21→r19→r25→r21`. The target operands
  are `lbl_<hex>` placeholders, so we read the six strings out of `band.exe` at
  those VAs (`%s:%s`, `remote_user_%d`, `null`, `local_user_%d`, `.data`, `pad_%d`).
  With the strings known, the **assignment is identical** on both sides (r22 `%s:%s`,
  r21 `remote_user`, r20 `null`, r19 `local_user`, r18 `ThePlatformMgr`, r25 `pad`).
  Only the *order* of the six hoisted `lis/addi` pairs differed, and ours was the
  exact reverse of our source's first-use order. The prediction was that writing
  the local-user arm first (`if (IsLocal() && !IsNullUser()) {local} else {if
  (!IsLocal()) remote}`) would emit retail's order. It did: all six charges
  cleared, with no other code change.
  ⇒ **A placeholder target operand forces objdiff to pair by position.** Resolve the
  labels before calling a lis/addi block a register rotation.
* **Left: 53 frame-slot offsets.** No instruction differs. Retail, ascending:
  static-init DataNode temp 0x78 · `users` / 2nd reset `OnlineID()` temp 0x80 ·
  `screenExit` 0x90 · `HandleType` result 0xb0 · local `id` 0xc0 · `key` 0xd0 ·
  `padUser` 0xe0 · `val` / 1st reset temp 0x100 · remote `id` 0x110.
  Ours: `users` 0x78 · temp 0x88 · local `id` / 2nd temp 0x90 · `screenExit` 0xa0 ·
  `HandleType` 0xc0 · remote `id` / 1st temp 0xd0 · `val` 0xe0 · `key` 0xf0 ·
  `padUser` 0x100.
  Within the remote arm, retail's ascending order (`key < val < id`) is exactly the
  reverse of ours. No slot experiment was run beyond that observation. The map is
  in the source comment for the next lane.

## 3. What did not move

### 3.1 `SaveLoadManager::SetState` — a coloring choice

The 10 charges are three registers, r25/r27/r29, with no instruction differences.
Tracing every use shows the **same interference graph with a different color
choice**:

* Retail gives `newState` a register of its own (r25), puts the static
  `saveload_dialog_event`'s address in a register it shares with later case-local
  values (r29), and does the same for `wasIdle` (r27).
* Ours gives the static's address its own register (r25), and `wasIdle` / `newState`
  take the two shared ones.

This agrees with W16-FW / W16-CF / W17 X4 ("scheduling, bank it"). No spelling was
tried. The permuter is off by directive.

### 3.2 `SongSortMgr::DoesOfferMatchFilter` — double bool normalization (6 spellings inert)

All 11 switch arms tail-merge into one `_M_find` + `!= end` block. Retail normalizes
the result twice in that block (`subic/subfe` ×2). The branch-source data shows no
join between the two pairs, so it is one block. These were applied to all 11 arms
and each was **byte-identical** to the current form:

* `set::count(x)`, which is `find == end ? 0 : 1` in our STLport
* `count(x) != 0`
* `(… != end) != 0`
* `… ? true : false`
* `(int)(…) != 0`
* `(bool)(int)count(x)`

The second normalization is not reachable by any spelling tried. The 568 B sibling
`DoesSongMatchFilter` has the same residue (W16-PQ).

### 3.3 `OvershellSlot::OnMsg(ButtonDownMsg)` — one load scheduled differently

After the implicit `const char*`→`Symbol` temp for `btnMsg.SetType(pulseType)`,
retail loads `lwz r4,0(r3)` before its `mr r11,r3`, and we load from r11 after it.

* An explicit `Symbol(pulseType)` is identical.
* A named `Symbol` (`Symbol s(p)` or `Symbol s = p`) is worse: 99.01, 50 charges.
  It also deletes retail's dead `mr r11,r3`, which shows retail used an unnamed
  temporary.

### 3.4 Not attempted, and why

Today's lanes already measured these rows, and nothing in this tree has changed
since:

* `MetaPerformer::SelectRandomVenue`: W16-PF (identity probe fixes one of the two adds; no natural spelling).
* `MusicLibraryNetSetlists::ParseDataResultsIntoSetlists`: W16-PF (6 spellings, worse or inert).
* `NextSongPanel::FillExpandedDetails`: W16-PR (EQUAL, a dead float home).
* `CustomizePanel::Handle`: W16-CG closed the codegen channel, with more than 18 spellings in the source.
* `Leaderboard::ShowData`: the 3 charges are scratch-register choices inside one `mulli`/`extsw`/`add`
  sequence, which is scheduling.

## 4. Measurement

`tools/ab_measure.py --patch` was run with the combined source diff `7410935ea..5e62f5f7b`
(3 files, `src/` only) in a fresh `setup_worktree.sh` worktree off `7410935ea`. Both legs were settled,
leg B recompiled 13 objects, and the run is labelled `w16-rb`.

**Pre-registered prediction:** only `SyncProperty@BandWardrobe` crosses, so +1 function / +2,416 B.

| measure | leg A | leg B | Δ |
|---|---:|---:|---:|
| `matched_functions` | 54,688 | 54,693 | **+5** |
| `masked_equal_functions` | 25,209 | 25,213 | +4 |
| honest (matched − masked) | 29,479 | 29,480 | **+1** |
| `matched_code` | | | **+2,664 B** |
| `matched_code_percent` | 58.663370 | 58.689360 | +0.025990 pp |
| units at 100 (mpn / all-rows-fuzzy) | 583 / 514 | 583 / 514 | 0 / 0 |

**The prediction missed by +4 functions / +248 B, and the miss is fully attributed** from a
row-level diff of the two archived reports:

| row | before (size, fuzzy, mpn) | after |
|---|---|---|
| `?SyncProperty@BandWardrobe@@` | 2,416 · 99.743 · 99.834 | 100 · 100 |
| `?BuildList@StoreOfferProvider@@` | 2,536 · 99.235 · 99.243 | 99.685 · 99.685 |
| `?MaybePublish@UIStats@@` | 2,604 · 99.594 · 99.671 | 99.919 · 99.919 |
| `fn_82665338` | 40 · 99.5 · 100 | 100 · 100 |
| `fn_82665360` | 40 · 99.9 · 99.9 | 100 · 100 |
| `fn_82665388` | 64 · 99.94 · 99.94 | 100 · 100 |
| `fn_826653F0` | 64 · 99.94 · 99.94 | 100 · 100 |
| `fn_82665458` | 40 · 99.4 · 99.9 | 100 · 100 |

The five `fn_` rows are BuildList's EH funclets. All of them are `masked_equal`, meaning they are
paired by funclet byte signature. They reference the parent's frame offsets, so when the named
`DataNode` moved BuildList's temps onto retail's slots, the funclets followed. That accounts for the
extra bytes (40+40+64+64+40 = **248 B**), the extra functions (4, because one was already at mpn 100),
and why Δhonest is +1 while Δmatched is +5. No row got worse.

The `none` control read +2,624 B. The tool reports it as NOT_APPLICABLE for a source patch.

## 5. What this lane deliberately did not do

* It did not run the permuter (standing directive).
* It did not run frame-slot experiments on MaybePublish beyond mapping them.
* It did not try spellings on SetState.
* It did not edit `symbol_aliases.json` or the map. Everything here is a source edit.
* It did not merge or push. That is left to the coordinator.
