# W16-AN — MainHubPanel (ticker cast, Poll, eight anonymous rows) + one mis-pin re-home

Lane W16-AN, 2026-09-14. Worktree `~/tmp/wt-w16-an`, branch `w16-an`, based on main `8d4fb23a`.
Baseline at main: **43,411 fns / 4,004,168 B / 39.080288 %**, `total_code` 10,246,004,
objdiff 4.2.9, ruler `functionRelocDiffs=name_check` (read from `report.json`'s
`provenance.diff_config`, not assumed). Every number below is a set-diff of `report.json`
row membership against `~/tmp/rows_w16ak_main.json`, taken after a **full** `./tools/ninja-locked`
with rc=0 — never from `run_objdiff`, which does a one-obj incremental build that skips the six
obj patchers.

**Lane result: +12 functions / +2,736 B. `default/MainHubPanel` 166/176 → 176/176 (100 %).
`default/CampaignSongInfoPanel` 50/50 → 53/53 (100 %). Zero rows fell out at any step.**

| | measure | before | after |
|---|---|---|---|
| | `matched_functions` | 43,411 | **43,423** |
| | `matched_code` | 4,004,168 | **4,006,904** |
| | `matched_code_percent` | 39.080288 | 39.106990 |
| | `fuzzy_match_percent` | 49.600136 | 49.624172 |

---

## Item 1 — `CheckProfileForTicker` 98.38 → 100 — **DONE**, `80c341ad`

**Verdict: confirmed; fixed at the call site, NOT by widening the header.**

Retail `fn_8261FF10` closes with `cmplwi r3, 0x0`; we emitted `cmpwi`. The fix is a cast at
the one call site in `src/band3/meta_band/MainHubPanel.cpp`:

```cpp
if ((unsigned int)TheServer.GetPlayerID(profile->GetPadNum()) != 0)
    return true;
```

W16-AJ §7 proposed changing `virtual int GetPlayerID(int)` to unsigned in
`src/network/net/Server.h:26`. **That was not done and must not be.** The in-tree record
(lane W16-B, `src/band3/net_band/RockCentral.cpp:279-286`) measured that retail tests the same
virtual slot with `cmpwi` at four sites that are already at 100 with `int`, and with `cmplwi`
at `fn_824F7D48`. Widening the header would break the four to fix the one.

**Predicted +1 fn / +148 B — measured +1 fn / +148 B, exact.**

### ⚠ The brief's premise about `ReloadMessages` was FALSE — corrected here

The brief said `ReloadMessages` (line 175, same call in an `if`) "is at 100 today, so it must be
`cmpwi` in retail; leave it alone and say so."

**Measured: `ReloadMessages` had no row anywhere in `report.json` and no map entry.** It was
unidentified and *unpaired* — reading 0 %, not 100 %. An unpaired row is invisible to callee
adjudication and looks identical to a row with nothing wrong. The brief's conclusion (do not
touch `Server.h`) survives, but it survives on RockCentral's evidence alone, not on this.

And when `ReloadMessages` *was* paired (Item 3), retail turned out to make **no
`Server::GetPlayerID` test at all** — see Item 3.

## Item 2 — `?Poll@MainHubPanel@@UAAXXZ` 51.80 → 100 — **DONE**, `b247af7d`

**Verdict: not a register-swap or permuter case at all. One wrong call spelling.**

Retail emits a single `bl ?SplitMs@Timer@@`. We expanded `Split()` and `Ms()` separately,
because both are inline in `src/system/os/Timer.h` while `SplitMs()` is the out-of-line
composite `float SplitMs() { Split(); return Ms(); }`. One line:

```cpp
if (mMessageTimer.SplitMs() > mMessageRotationMs) {
```

replacing `mMessageTimer.Split(); if (mMessageTimer.Ms() > mMessageRotationMs) {`.

A 51.80 % row behind one call is what a wrong callee spelling looks like when the callee is
inline on one side — the whole inline expansion is charged. **The rb3-Wii oracle spells this
the expanded way and is wrong for retail** (the oracle was wrong twice in this lane; retail
bytes outrank it in every mode).

Neither briefed family applied: there is no `MILO_DEBUG` block in this body, and the
`REGISTER_SWAP` label was a symptom that dissolved with the real fix.

**Predicted +1 fn / +244 B — measured +1 fn / +244 B, exact.**

## Item 3 — the eight anonymous MainHubPanel rows — **DONE**, `fd6405ff` + `9f69275a` + `716e478a`

**Verdict: all eight identified and proved, all eight now at 100 %. Unit is 176/176.**

### 3a — identification and map rows (`fd6405ff`)

Eight rows added to `scripts/target_symbol_map.json` (lowercase keys, round-trip asserted
byte-stable before editing):

| address | symbol |
|---|---|
| `0x82620540` | `?GetMotd@MainHubPanel@@QAAPBDXZ` |
| `0x82620f38` | `?OnMsg@MainHubPanel@@QAA?AVDataNode@@ABVOvershellOverrideEndedMsg@@@Z` |
| `0x826213c0` | `?OnMsg@MainHubPanel@@QAA?AVDataNode@@ABVSessionMgrUpdatedMsg@@@Z` |
| `0x82621ac0` | `?ReloadMessages@MainHubPanel@@QAAXXZ` |
| `0x82621c70` | `?PrepareProfilesAndMessages@MainHubPanel@@QAAXXZ` |
| `0x82622748` | `?StartFinding@MainHubPanel@@QAAXXZ` |
| `0x82622d00` | `?OnMsg@MainHubPanel@@QAA?AVDataNode@@ABVNewRemoteMachineMsg@@@Z` |
| `0x82622de8` | `?OnMsg@MainHubPanel@@QAA?AVDataNode@@ABVRemoteMachineLeftMsg@@@Z` |

**Predicted +0 fns / +0 B — measured +0 / +0, exact.** Naming pays in pairing and bug exposure,
not bytes: the rows went 0 % → 23.5–71.9 % and the unit went 166 → 168/176.

#### The hard case, and how it was separated

`fn_82622D00` and `fn_82622DE8` (`NewRemoteMachineMsg` / `RemoteMachineLeftMsg`) are
**character-identical source bodies**: identical callees, identical data refs, identical 188 B
size, identical post-pairing score (56.085 both). The brief's 2-candidate test says report a
class rather than pick, unless relocation targets plus `.xdata` identity separate them. They
do not.

Ordering was ruled out as an instrument, not assumed away: retail `.text` COMDAT order is
uncorrelated with oracle source order (475, 301, 329, 422, 470, 547, 589, 208 …), and `.data`
local-static order is perfectly monotonic with `.text`, so it carries no *independent*
information.

They were separated by the **`HANDLE_MESSAGE` dispatch table in `Handle`**, which calls each
message class's `Type()` and then its handler — so the dispatch order fixes which address is
which. Validated against a 10-row control before being trusted. **Without that instrument
these two would have been reported as a class, not named.**

### 3b — the source defect behind all eight (`9f69275a`, `716e478a`)

A systematic porting artifact, and the census is clean in both directions: **every function in
this file that used a function-local static was already matched; every function that used a
shared `extern` was one of the eight unmatched rows.** Retail materialises a function-local
`static` (guard word with bit flags 0x1/0x2/0x4, instance, and a `??__F` atexit thunk, emitted
at the declaration point); the rb3-Wii dev source uses file-scope externs from `Messages4.h` /
`Symbols*.h`.

Eight ports, each placed at retail's declaration point:

```cpp
static Symbol  messages_per_session("messages_per_session");                 // ReloadMessages, first stmt
static Message refresh_message_provider("refresh_message_provider");         // PrepareProfilesAndMessages
static Message cancel_find_override("cancel_find_override");                 // OnMsg(OvershellOverrideEndedMsg)
static Symbol  mod_auto_vocals("mod_auto_vocals");                           // StartFinding, top
static Symbol  error_find_players_with_auto_vocals("error_find_players_with_auto_vocals");
static Message update_finding_help("update_finding_help");                   // x3: New/Left/SessionMgrUpdated
static Symbol  message_motd("message_motd");                                 // GetMotd (bits 0x1/0x2/0x4)
static Symbol  message_motd_signin("message_motd_signin");
static Symbol  message_motd_noconnection("message_motd_noconnection");
```

Note `update_finding_help` — `src/system/utl/Messages4.h:53` declares
`extern Message update_finding_help_msg;`, the wrong-for-retail spelling.

**Predicted +8 fns / +2,084 B — measured +6 fns / +1,428 B.** The prediction failed on two rows,
both of which had a *second*, independent defect:

**`GetMotd`** — I removed `|| IsOnlineRestricted()` (correct: `IsOnlineRestricted()` is
out-of-line at `src/system/os/PlatformMgr.h:252` and cannot inline, so retail would have to show
a `bl`; it shows none) **and also swapped the remaining branch arms (incorrect)**. Measured
result: `beq` where retail has `bne`, plus two swapped loads r28↔r26, 3 charged instructions,
99.776. Self-caught by measurement, not by review. Restored in `716e478a`:

```cpp
if (!ThePlatformMgr.IsEthernetCableConnected()) {          // out-of-line -> bl
    return Localize(message_motd_noconnection, nullptr);
} else if (!ThePlatformMgr.IsConnected()) {                 // inline -> lbz r11, 0x26
    return Localize(message_motd_signin, nullptr);
} else {
    return Localize(message_motd, nullptr);
}
```

**`ReloadMessages`** — two further divergences, both adjudicated on retail bytes:

1. **Retail makes NO `Server::GetPlayerID` test.** Proved two independent ways: a unit-wide scan
   for the virtual slot at 0x1c finds it only in `fn_8261FF10` and `fn_82622648`, and
   `lbl_82C6EB50` (`TheServer`) is absent from `fn_82621AC0`'s data refs entirely. The wrapper
   `if (TheServer.GetPlayerID(profile->GetPadNum()))` was removed outright. This is the row the
   brief believed was already at 100 and evidence for the `cmpwi` reading — it was neither.
2. Retail uses three explicit compares, not an unsigned range trick:
   ```cpp
   if (ty == kTrackNone || ty == kTrackPending || ty == kTrackPendingVocals) {
   ```
   replacing `if (ty - 10U <= 2)`. Retail emits `cmpwi 0xa` / `cmpwi 0xb` / `cmpwi 0xc`
   (`src/system/beatmatch/TrackType.h`: `kTrackNone = 10`, `kTrackPending = 11`,
   `kTrackPendingVocals = 12`).

Per-step set-diffs: GetMotd + ReloadMessages(1) predicted +2/+656, measured +1/+268;
ReloadMessages track-type predicted +1/+388, measured +1/+388 exact.

## Item 4 (optional) — PhysicsManager.cpp mis-pin re-home — **DONE**, `692e28e2` + `e9d32d55`

**Verdict: W16-AJ's claim CONFIRMED on retail bytes, with five independent witnesses and no
ordering argument. It undercounted — there are THREE rows, not two.**

`config/45410914/splits.txt` pinned `.text 0x825F5BF4–0x825F5D28` to `PhysicsManager.cpp`, an
engine file name sitting in the middle of the band3 meta-panel band. `src/system/world/PhysicsManager.cpp`
does exist, so this was a genuine mis-pin of a real file, not a phantom row — byte geometry
checked first, per the standing rule that a dtk mis-carve is indistinguishable from an
unidentified row.

`build/45410914/asm/PhysicsManager.s` holds exactly three functions:

| address | size | identity | evidence |
|---|---|---|---|
| `fn_825F5BF8` | 92 B | `?SelectedScoreType@CampaignSongInfoPanel@@QBA?AW4ScoreType@@XZ` | `li r3, 0xa` on the `!kUp` path == `kScoreBand` (`src/band3/game/Defines.h:40`); string at `lbl_820BB9B4` is literally `"instruments.lst"`; `bl fn_8268F490` is map-named `?SymToScoreType@@YA?AW4ScoreType@@VSymbol@@@Z`; `bl fn_827F8720` is `?SelectedSym@UIList@@QBA?AVSymbol@@_N@Z` |
| `fn_825F5C60` | 168 B | `?GetCareerScore@CampaignSongInfoPanel@@QBAHXZ` | calls `fn_825F5B88`, **already map-named** `?SelectedSource@CampaignSongInfoPanel@@` and itself the start of CampaignSongInfoPanel's adjacent `.text` block; then `fn_825A4F28` = `Campaign::GetProfile`, `fn_825D1ED8` = `SongStatusMgr::CalculateTotalScore`; builds a function-local `static Symbol all` from `lbl_82014840` (= `"all"`) with guard `0x82E003D0` / storage `0x82E003CC` — the exact addresses our own source comment already cited |
| `fn_825F5D08` | 32 B | the `??__F` atexit thunk clearing that guard bit | `clrrwi r11, r11, 1` on `lbl_82E003D0` |

Nothing in any of the three touches physics.

**Structural corroboration from two sections independently**: the `.text` block sat exactly in
the gap between CampaignSongInfoPanel's `0x825F5B88–0x825F5BF4` and `0x825F5D28–0x825F6B00`, and
its `.pdata 0x82227110–0x82227128` sat exactly in the gap between that unit's
`0x82227108–0x82227110` and `0x82227128–0x82227230`. After the move both section lists are
contiguous.

Map names were read out of **our own compiled COFF symbol table**, not hand-mangled — a wrong
map name pins a row at 0 % permanently because the base obj cannot define the name.

The `PhysicsManager.cpp` splits entry was deleted outright (that `.text` block was its only one;
an empty unit hard-fails `report.json` with `Invalid COFF/PE section headers`). The file stays
declared in `objects.json` as compile-only scaffolding. Its `.pdata` line is derived output:
the first build failed the split-guard because dtk re-derived it under the new owner and
rewrote `splits.txt`; the retry is the fixed point, exactly as the guard's message says.

**Re-homing an already-pinned address is not metric-neutral.** Measured, whatever the sign:

- **Predicted +2 fns / +260 B — measured +1 fn / +168 B.**
- CROSSED IN `?GetCareerScore@…` 168 B; CROSSED IN `default/CampaignSongInfoPanel::fn_825F5D08`
  32 B; FELL OUT `default/PhysicsManager::fn_825F5D08` 32 B (the same row, re-homed — net 0).
- **Denominator exactly neutral**: PhysicsManager's 292 B moved wholesale into
  CampaignSongInfoPanel (4,860 → 5,152 `total_code`), whole-binary `total_code` unchanged at
  10,246,004.

### The prediction failed, and the failure was the most valuable part

`?SelectedScoreType@…` did **not** cross: the unit landed at 52/53, exactly 92 B short. The row
was at 99.73913 fuzzy / 99.95652 mpn with **one** charged site of 23 instructions:

```
idx 17   target  lwz r3, 0x50(r1)        base  lwz r3, 0x0(r3)
```

Retail reads the returned `Symbol` back out of its **own stack slot**; we dereferenced the sret
pointer `UIList::SelectedSym` returns. Chaining the call straight into `SymToScoreType()`
produces the pointer form; binding it to a named local produces the slot form. Fixed in
`e9d32d55` by naming the temporary. The precedent needed no oracle and is two functions away in
the same TU: `GetCareerScore` does `Symbol src = SelectedSource();` and retail reads
`lwz r5, 0x50(r31)` — and it is at 100 %.

**Predicted +1 fn / +92 B — measured +1 fn / +92 B, exact.** `default/CampaignSongInfoPanel` is
now 53/53, 5,152/5,152, code % 100.0.

**This divergence existed before the lane and was structurally invisible.** The address was
pinned to `PhysicsManager`, so our compiled body was never compared against anything and the
unit read a clean 50/50. Pairability is a correctness instrument, not a scoring one.

---

## Measurement ledger

Every row: full `./tools/ninja-locked` rc=0, then a `report.json` row-membership set-diff against
`~/tmp/rows_w16ak_main.json` (main `8d4fb23a`). **Zero rows fell out at any step** except the
one re-homed row in Item 4, which crossed back in under its new unit in the same measurement.

| step | predicted | measured | commit |
|---|---|---|---|
| null control | 0 / 0 | 0 rows / 0 B | — |
| 1 `CheckProfileForTicker` | +1 / +148 | **+1 / +148** exact | `80c341ad` |
| 2 `Poll` | +1 / +244 | **+1 / +244** exact | `b247af7d` |
| 3a map rows | +0 / +0 | **+0 / +0** exact | `fd6405ff` |
| 3b source ports | +8 / +2,084 | **+6 / +1,428** | `9f69275a` |
| 3b `GetMotd` + `ReloadMessages`(1) | +2 / +656 | **+1 / +268** | `716e478a` |
| 3b `ReloadMessages` track-type | +1 / +388 | **+1 / +388** exact | `716e478a` |
| 4 re-home | +2 / +260 | **+1 / +168** | `692e28e2` |
| 4 `SelectedScoreType` | +1 / +92 | **+1 / +92** exact | `e9d32d55` |
| **lane total** | | **+12 fns / +2,736 B** | |

## NOT done, and why

- **`src/network/net/Server.h:26` `GetPlayerID` was NOT widened to unsigned.** Refuted on retail
  bytes before the lane started (W16-B, `RockCentral.cpp:279-286`): four sites already at 100 %
  use `cmpwi` with `int`. The call-site cast is the correct shape. The brief's supporting
  argument from `ReloadMessages` was itself false (that row was unpaired, not matched) — the
  conclusion stands on RockCentral alone.
- **The `?Size@ArkFile@@UAAHXZ` and `?GetCacheName@CacheXbox@@UAAPBDXZ` "target only" callees
  under `MainHubPanel` were NOT chased and NO alias was added.** They are ICF fold aliases — a
  folded callee resolves to an arbitrary survivor name. `scripts/symbol_aliases.json` is owned by
  lane W16-AL and was not touched. The rows reached 100 % without them, so no alias was needed;
  adding an unproven one lifts `name_check` by construction and is an integrity hazard, not a win.
- **Bars respected, nothing read or written**: `0x825F58C8–0x825F591C`
  (CampaignGoalsLeaderboardPanel) and `0x825F5920–0x825F5B88` (SetlistToStorePanel) — W16-AL owns
  the folds there. Note `fn_825F5AD0` (the `Find<UIList>` instantiation that `SelectedScoreType`
  calls) lives inside the second bar; it has no map row, so `name_check` **forgives** it as a
  placeholder target and no charge arises. It was left alone. AM-owned addresses (`0x823d3918`,
  `0x823f0b50`, `0x8248f1c0`, `0x82787718`, `0x827d5bb0`, `0x822b6538`, `fn_824CE130`,
  `_bijection_arbitrary`) untouched. OvershellPanel and the Server vtable not opened.
- **`default/PhysicsManager` no longer exists as a unit**, and `src/system/world/PhysicsManager.cpp`
  is now compiled-but-unpinned scaffolding. Its real `.text` in retail has **not** been located —
  that is a separate identification job for a splits lane, not a defect introduced here. The unit
  it used to name held no physics code at all.
- **`default/DefaultPhysicsManager` (1/3, 224 B) was NOT touched** — a different unit, out of
  scope, and no evidence was gathered about it.

---

## Gates

Run in the worktree, in the brief's order, native gate last.

```
BUILD rc=0
```

```
$ python3 scripts/verify_ruler_agreement.py --check
  OK  functionRelocDiffs = name_check
  OK  combineDataSections = true
  OK  combineTextSections = true
  OK  ppc.calculatePoolRelocations = false

OK: both objdiff-cli entry points resolve the same ruler.
rc=0
```

```
$ python3 scripts/verify_objs_patched.py --verify-manifest
[denylist] OK: 6 denylisted address(es), 3 with a live map string, none named in 3115 target objects (495648 symbols scanned)
[patch-state] OK: 1215 decomp, 3115 target objects match 2026-09-14T21:01:14Z (tree_sha256=4773672d9023c522)
rc=0
```

```
$ python3 tools/icf_alias_finder.py --validate
FAIL [$__lower_bound @ 0x825f3480]: target objs name 2 members: ['??$__lower_bound@PAVSymbol@@V1@VAccomplishmentCmp@@H@stlpmtx_std@@YAPAVSymbol@@PAV1@0ABV1@VAccomplishmentCmp@@PAH@Z', '??$__lower_bound@PAVSymbol@@V1@VGoalCmp@@H@stlpmtx_std@@YAPAVSymbol@@PAV1@0ABV1@VGoalCmp@@PAH@Z']
FAIL [$__merge_sort_loop @ 0x825f3c58]: target objs name 2 members: [...AccomplishmentCategoryCmp..., ...GoalCmp...]
FAIL [$__linear_insert @ 0x825f3db8]: target objs name 2 members: [...AccomplishmentCmp..., ...GoalCmp...]
FAIL [$__merge_backward @ 0x825f3e38]: target objs name 2 members: [...AccomplishmentCmp..., ...GoalCmp...]
FAIL [SetTypeDef @ 0x82808f88]: target objs name 2 members: ['?SetTypeDef@UIPanel@@$4PPPPPPPM@BM@AAXPAVDataArray@@@Z', '?Highlight@RndDir@@$4PPPPPPPM@FM@AAXXZ']
VALIDATE: FAIL -- 1387 map-consistent, 245 tolerated, 5 contradicted, 1638 total
rc=1
```

### ⛔ The ICF gate FAILS, and it is PRE-EXISTING on main — NOT caused by this lane

This is reported rather than fixed, because `scripts/symbol_aliases.json` is owned by lane
W16-AL and the two vcall-thunk spellings sit on map rows this lane is barred from touching.
**Non-causation is proved on four independent grounds, not asserted:**

1. **This lane never touched `scripts/symbol_aliases.json`.** `git diff 8d4fb23a..HEAD` over that
   path is empty; the lane's whole diff is 4 files (`config/45410914/splits.txt`,
   `scripts/target_symbol_map.json`, `src/band3/meta_band/CampaignSongInfoPanel.cpp`,
   `src/band3/meta_band/MainHubPanel.cpp`).
2. **All 5 groups already exist at the base commit** `8d4fb23a`, with those exact survivors.
3. **All 10 contradicting spellings resolve to identical addresses at base and now.** The map
   edit is purely additive: 10 rows added, **0 changed, 0 removed**, and none of the 10 added
   addresses is any of the contradicting ones.
4. **None of the 10 involved addresses lies inside the re-homed range** `0x825F5BF4–0x825F5D28`
   (five below it, four above it, one at `0x82808f88`), so the splits change cannot have altered
   their target-obj liveness either.

The mechanism, for whoever picks this up: the group at `0x825f3480` claims
`__lower_bound@…GoalCmp…` folds with `__lower_bound@…AccomplishmentCmp…`, but the map names the
`AccomplishmentCmp` instantiation at a **different** address, `0x825f7638`. Two spellings named
at two distinct addresses contradict the fold claim. The other three STLport pairs are the same
shape (`0x825f3c58`/`0x825f9e18`, `0x825f3db8`/`0x825faba0`, `0x825f3e38`/`0x825fac20`); the
fifth is the vcall-thunk pair `0x82808f88`/`0x825f4268`. This is the map/alias coupling class
main has repaired three times already (`209b007d`, `a467ce21`, `18214665`) — all three are
ancestors of this lane's base, so these five are a live remainder, not stale state.

```
NATIVE_GATE_RESULT  (pasted verbatim below, run as the lane's last action)
```
