# W16-HE — game-layer fuzzy sweep: 30 rows in the 1–90% band

**Branch** `w16-he`, rebased onto main `1c9ccf51`. **Ruler** `name_check` (graded,
read from `report.json` `provenance.diff_config`).
**Population:** every *named* row in `src/band3` / `src/network` units with
`1 ≤ fuzzy_match_percent < 90`, excluding StoreOfferProvider, RockCentral,
BandStorePanel, MemMgr and `src/system/` (other lanes). Ranked by
`size × (100 − fuzzy)` off the worktree's own freshly built `report.json`.
**30 rows** at the start; **9** remain, each with a named blocker (§3).

## 1. Whole-binary A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-he-ab --patch <main..w16-he diff>`
in a fresh worktree at main `1c9ccf51`: one run over the whole branch diff
(16 files), objdiff-cli pinned across both legs, leg A settled at 0 recompiles,
leg B 230 recompiles.

```
leg A: matched=44544 masked=23331 honest=21213 code%=42.157425
leg B: matched=44572 masked=23339 honest=21233 code%=42.223200
Δmatched=+28  Δmasked_equal=+8  Δhonest=+20  Δcode%=+0.065775pp  Δcode_bytes=+6740
Δfuzzy=+0.035011pp   (legA 50.816906 -> legB 50.851917)
units at 100% [mpn]: 212 -> 212 (0 in, 0 out)   [all-rows-fuzzy]: 187 -> 187
control none: Δmatched_code=+7296 B (default ruler +6740 B)
```

**Row-level diff of the two archived leg reports:** 33 rows up, **0 down**,
0 disappeared, 0 new. The 33 are the 21 rows in §2 plus
`VocalTrack::Poll` / `PracticePanel::Poll` and 10 anonymous `fn_` rows
(EH funclets / guard thunks in MusicLibraryStore, SongSort,
TokenRedemptionPanel, PracticeSectionProvider and SaveLoadManager) that
re-paired once their parents matched. Those 10 are where the +28 matched in
excess of the named rows comes from.

Prediction before running: net positive fuzzy, no regressed rows, and
Δmatched at least the 17 named rows that reached 100. It held (+28).

## 2. Rows, before → after

Before = worktree build at `4887c2ec` (branch base), after = branch tip. Per-row
values are `fuzzy_match_percent` read from `report.json`.

| row | B | before | after | what fixed it |
|---|---:|---:|---:|---|
| `Game::OnMsg(ButtonDownMsg)` | 1,664 | 8.01 | **99.90** (mpn 100) | wrote W16-EH's decoded tail; then **two stacked switches** (§4.1); `Max(0.0f, x)`; DirectInstrument read before the index store |
| `Game::Game` | 1,504 | 80.80 | **100** | create `mUnkTU5GuidePitch` under movie-sync (**behavioural**: never set, yet deleted/dereferenced); init-list shape; null BandUser to `Band`; local static Symbols |
| `Game::UpdatePausedState` | 972 | 70.00 | **100** | no Wii screen-saver block; 3rd bool gates `SetNoFail` (**behavioural**); local static Messages; `players&` loop |
| `GetUserFontChar` (Utl.cpp) | 472 | 54.38 | **100** | four function-local static Symbols |
| `GigFilter::Init` | 472 | 59.75 | **100** | each static Symbol declared at first use |
| `TokenRedemptionPanel::ShowPurchaseUIForOffer` | 332 | 42.67 | **100** | construct + `Initiate()` the `XboxPurchaser` (**behavioural**: the Wii stub never started a purchase) |
| `SessionMgr::AreInvitesAllowed` | 288 | 64.21 | **100** | second static after the early-out; `&&` return |
| `Game::ResetVoiceChatState` | 296 | 86.47 | **100** | skip `IsNullUser()` users (**behavioural**) |
| `RetryAudioPanel::PollForLoading` | 260 | 51.45 | **100** | local static Message |
| `TokenRedemptionPanel::Unload` | 192 | 65.58 | **100** | no RockCentral cancel; clear `mOfferIDs` |
| `SaveLoadManager::IsReasonToUpload` | 192 | 71.31 | **100** | local static Symbol; `bool` skip flag |
| `SaveLoadManager::StartSaveAction` | 164 | 22.44 | **100** | save `GetProfile()`; no Wii locking/sink; `SaveMemcardAction(BandProfile*)`, 0x14 B |
| `SaveLoadManager::Finish` | 108 | 66.67 | **100** | `TheMemcardMgr.RemoveSink(this)` |
| `TrainerPanel::GetSectionLoopEnd` | 140 | 76.29 | **100** | early return on zero remainder |
| `GemTrainerLoopPanel::GemTrainerLoopPanel` | 140 | 76.03 | **100** | implicit ctor (§4.2) |
| `LicenseMgr` `operator>>(BinStream&, hash_map&)` | 120 | 86.63 | **100** | unsigned count-down loop |
| `PracticeSectionProvider::~PracticeSectionProvider` | 116 | 48.48 | **100** | implicit dtor (§4.2) |
| `CharProvider::GetCharData` | 72 | 88.89 | **100** | early-return shape |
| `Player::InRollback` | 44 | 66.82 | **100** | `Game::InRollback` returns the comparison directly |
| `BandUserMgr::~BandUserMgr` | 448 | 84.24 | **99.96** | statics at use; added the retail-only null-user purge (§4.3) |
| `Quazal::MemoryManager::GetDefaultMemoryManager` | 188 | 81.96 | **99.89** | `/Od` shape: static re-read, if/else chain |
| `Quazal::MD5::transform` | 6,068 | 82.19 | 82.19 | not fixed (§3) |
| `StoreMainPanel` `__destroy_range_aux<…NewReleaseEntry>` | 96 | 17.33 | 17.33 | wrong map name (§3) |
| `AllowedToAccessContent` | 88 | 16.14 | 16.14 | needs the real callee body (§3) |
| `FindFrameWithLeadIn` (Stats) | 68 | 61.76 | 61.76 | wrong map name (§3) |
| `ModifierMgr::IsHidden` / `IsActive` | 28 + 28 | 28.57 | 28.57 | unexplained EH-style frame (§3) |
| `TrackPanel::GetNumPlayers` | 12 | 40.00 | 40.00 | dtk mis-carve (§3) |
| `LicenseMgr` `_Slist_base` ctor | 12 | 80.00 | 80.00 | wrong map name (§3) |
| `Game` `_List_iterator` ctor | 8 | 70.00 | 70.00 | wrong map name (§3) |

Rows outside the band that moved with these changes (full-report row diffs
taken around the `Game::InRollback` edit): `VocalTrack::Poll` 97.62 → **100**,
`PracticePanel::Poll` 98.63 → 99.38. The A/B in §1 lists every unit movement.

## 3. The nine rows left, and why

- **`Quazal::MD5::transform`** — Quazal NetZ `/Od` code. Retail evaluates the
  *right* operand of `|` first when both sides are non-leaf, and keeps `32 - s`
  as a register operand (`li r6,25; srw`). Our `/Od` compile evaluates strictly
  left-to-right and folds to `srwi`. The RFC 1321 macro order
  (`(b&c)|(~b&d)`, `(x<<s)|(x>>(32-s))`) reproduces retail's `or` operand order
  but not its evaluation order, and scored **77.78 (worse)**; reverted. No
  operand order can produce "evaluate right, emit (left, right)" under our
  compiler. That is consistent with a vendor lib built by a different compiler
  build, but it is **not verified**.
- **`AllowedToAccessContent`** — retail calls `MaxAllowedHmxMaturityLevel` out
  of line (`fn_825BE310`, 584 B: a per-country parental-rating table over two
  unidentified XDK calls, `fn_82B54328` / `fn_82B543A8`). Our 3-line stand-in is
  wrong and gets inlined. The honest fix is the real body. ⚠ The split named
  `MusicLibraryStore.cpp` (`0x825BE2F0`–`0x825BE720`) actually holds **Utl.cpp**
  functions (`IsLeaderLocal`, this one, and `GetFontCharFromTrackType`'s
  neighbourhood). Pin issue, not touched.
- **Wrong map names** (body cannot be the named function): `FindFrameWithLeadIn`
  @`0x82605C38` is `MakeString(fmt, sym.Str(), SystemLocale().Str())`, not a
  float; `__destroy_range_aux<…NewReleaseEntry>` @`0x8263B168` is an
  uninitialized-copy of 0x3c-byte entries; `_Slist_base` ctor @`0x8264E920` is
  `stb 0,0x38(r3)`; `_List_iterator` ctor @`0x826758F8` is `return 7`.
- **`TrackPanel::GetNumPlayers`** @`0x82B90798` is the tail half of a
  mis-carved `fn_82B90790` (`return TheGame != 0`).
- **`ModifierMgr::IsHidden`/`IsActive`** — retail returns a constant inside a
  minimal `r31` frame (the EH-frame shape), unique in the binary (scanned: 2
  hits, both these rows). An iterator-based `GetModifierAtListData` did not
  produce it; reverted.

## 4. Findings worth reusing

### 4.1 A unique switch lowering was two stacked switches

Retail `Game::OnMsg(ButtonDownMsg)` maps the sparse button value to a dense
index through a decision tree whose leaves are `li r11,N; b join`, then runs an
`mtctr r11 / cmpwi r11,0 / beq / bdz…` chain. A scan of every `.s` for that
shape found **one** function in the binary: this one. Writing it as
`switch (button) → action = N` followed by `switch (action)` took the row from
**35.5 → 95.7** in one edit. The action numbers come from the tree's `li`
values. The second switch's case order is retail's body-layout order, and that
order is independently confirmed by the static guard-bit numbering
(`camToggle`=1, `deploy`=2, `back`=4, `fwd`=8, `endBuffer`=0x10).
Prediction before measuring: "if the two-switch reading is right, the dispatch
block and the index cascade both collapse to equal". It held: the residue is
4 operand swaps.

### 4.2 Implicit special members leave two byte-level tells

- **Implicit dtor:** no derived-vptr re-store at dtor entry.
  `PracticeSectionProvider` 48.5 → 100 by deleting `virtual ~X() {}`. Every
  100% sibling provider *with* a user-declared `{}` dtor stores both vptrs,
  which makes it a clean discriminator.
- **Implicit ctor (virtual base):** a literal `0` stored to the vtordisp slot.
  A user-declared `{}` ctor computes `vboff - 0x3c` there and costs a saved
  register. `GemTrainerLoopPanel` 76.0 → 100.

### 4.3 One retail-only helper, identified by a shared dispatch

`~BandUserMgr` calls `fn_82682E00(this)` before `DeleteAll(mUsers)`. Its body
erases users whose User-vtable `+0x70` virtual returns true, from `mLocalUsers`
and `mUsers`. The same `+0x70` dispatch in `Game::ResetVoiceChatState` is
`IsNullUser()` (already identified in `User.h`), so the helper is a null-user
purge. It is also a correctness fix: without it `DeleteAll(mUsers)` deletes
`mNullUser` and `RELEASE(mNullUser)` frees it again. Named `RemoveNullUsers`
**provisionally** (no oracle). The map is untouched and the retail row stays
`fn_`.

### 4.4 Static-Symbol placement is the most common defect here

9 of the 21 rows fixed (Game ctor, UpdatePausedState, GetUserFontChar,
GigFilter::Init, ShowPurchaseUIForOffer, AreInvitesAllowed, PollForLoading,
IsReasonToUpload, ~BandUserMgr) had function-local statics that our source
spelled as `Symbols*.h` / `Messages*.h` globals, or declared all at the top
when retail guards each one at its first use. The guard-bit order in retail is a direct
readout of source declaration order.

## 5. What I did not do

- No map, splits, alias or `symbols.txt` edits. The four wrong-map-name rows
  and the mis-carve are recorded, not fixed.
- Did not write `MaxAllowedHmxMaturityLevel`'s real body (unidentified XDK
  callees).
- Did not touch `src/system/` or the excluded files. `TheRockCentral` appears
  in this diff only as a **removed** call in `TokenRedemptionPanel::Unload`.
- Permuter not run (standing directive).

## 6. Native gate

<!-- GATE-RESULT -->
