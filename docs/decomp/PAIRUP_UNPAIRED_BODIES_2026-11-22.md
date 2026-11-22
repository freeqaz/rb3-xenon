# PAIRUP: pair the bodies we hold that paired with nothing (2026-11-22)

Lane PAIRUP. Branch `pairup` in `~/tmp/wt-pairup`, based on main `637a73bda`. Input: the census
`NOSRC_SOURCELESS_RETAIL_CENSUS_2026-11-22.md` §4.1 and `CAMPAIGN_STATE_2026-10-07d.md` §6 lever 2.

**Goal.** Each body below sat in an `auto_*` run (or had no map row), so no retail row paired with it and
nothing checked it. Pin the run into the unit whose obj defines the body, add the map row, then read every
newly paired row's diff for behaviour differences from retail and fix them.

**Prediction (before building).** Most rows would pair at or near 100, since W16-UK had already fixed
several of these bodies by reading retail. A few would show codegen residue. Behaviour defects would be rare.

**Measured.** 31 named rows now pair, plus 13 EH funclets. 25 named rows read fuzzy 100 after the fixes.
Four behaviour differences turned up and three were fixed. The prediction held for the pinned rows. It
failed for one big row: `TokenRedemptionPanel::Poll` carries retail logic our body lacks (see below).

## Commits

1. `7c9634e68`: splits, map and `objects.json` (new `system/os/StageKit.cpp` entry), plus the `.pdata` and
   `symbols.txt` fixed point the split re-derived. Every pin extends an existing block of the owning unit
   over its `auto_*` hole, so no already-pinned address is re-homed. StageKit is a new unit at
   `0x82521B30..0x825227E8`.
2. `3e03424c9`: map row `FxSend360::Cleanup`, plus `scripts/alias_callee_names.json` re-recorded.
   Naming `~FxSend360` and `Cleanup` renames callees inside the 11 `FxSend*360` `??_G`/`??_E` fold groups
   and trips the `CHECK ALIAS CALLEE NAMES` edge. Those memberships were recorded UNDECIDABLE before and
   re-prove UNDECIDABLE after (`alias_callee_name_drift.py --reprove --write`), so nothing regressed.
3. `6df22d5d3`: source fixes (behaviour and codegen) for the rows the diffs flagged.

## Table

Fuzzy and mpn are on the shipped ruler (`name_check`), read from the worktree's `report.json` after commit
3. "Before" was unpaired in every row (no retail row matched the name).

| body | retail address | action | fuzzy after | behaviour verdict |
|---|---|---|---:|---|
| `GemPlayer::GemPlayer` (2,704 B) | `0x826C8470` | GemPlayer.cpp block extended to `0x826C9098`; map row | 99.99 | **same**. The one charged site is `vector<Vector2>::reserve` (retail) against our `vector<GemPlayer::UpcomingFretRelease>::reserve`, a template-instantiation pair. Both elements are 8 B, so it is probably an ICF fold. Not adjudicated. |
| 9 GemPlayer ctor EH funclets | `0x826C8F00..0x826C906C` | same pin; pair by bytes | 100 (one 99.55, mpn 100) | same |
| `FileEnumerate` (396 B) | `0x825219A0` | File_Win.cpp block extended to `0x82521B30`; map row | 100 | same (W16-UK's retail fix confirmed) |
| StageKit, 24 functions | `0x82521B30..0x825227E8` | new unit, `objects.json` entry, 24 map rows. The anon-namespace hash `5a6e5d1b` is ours, because retail has no symbols and `anon_ns` matches by template. | 19 rows at 100 | see the StageKit rows below |
| `StageKit` `SendRaw` | `0x82521B30` | codegen fix: the RawCmd is stored as one struct (retail does a `sthx`) | 100 (was 65.0) | same |
| `StageKitSetLedPattern` | `0x82521BF0` | dtk merged the `0x82521C6C` fragment into it (re-derived `symbols.txt`) | 98.29 (mpn 100) | same; the residue is relocation-only (switch table) |
| `StageKitSetLedState` | `0x82521B98` | — | 78.24 | **same**. Branch layout differs, the mapping does not: 0 off, 1 on, 2–7 off, 8–13 on, ≥14 untouched. |
| `StageKitReset` | `0x82521CB0` | — | 72.31 | **same**; store scheduling only |
| `StageKitUpdateLeds` | `0x82521E20` | — | 58.84 | **same**. Retail unrolls the bank loop (constant `rotlwi` per bank), and the result is identical. |
| `StageKitPoll` | `0x82521ED0` | — | 78.12 | **same**. Retail maps bank to command with a switch (0x20/0x40/0x60/0x80, default 0xD0) where ours uses a table. The bank index is always `% 4`, so the default never runs. |
| `~FxSend360` (372 B) | `0x82B699E8` | FxSend.cpp block extended to `0x82B6A090`; map row | 100 | same |
| `??_GFxSend360` | `0x82B69C38` | map row | 100 | same |
| 5 `~FxSend360` funclets | `0x82B69B5C..0x82B69C0C` | same pin | 100 | same |
| `FxSend360::Cleanup` (332 B) | `0x82B69868` | map row, so our dtor's call site is checked by name | 0, no body anywhere (W16-UK §5) | unverifiable; no body |
| FxSend360 `fn_82B69C90` 604, `fn_82B69F18` 176, `fn_82B69FC8` 200, 2 funclets | same run | pinned, unnamed | 0 | unidentified FxSend360 members (they call `__RTDynamicCast`, `Cleanup` and XAudio2 send-vector code); no body in our tree |
| `Gem::Gem` (316 B) | `0x82BAC148` | Gem.cpp block extended to `0x82BAC284`; map row | 100 | same |
| `RndEnvAnim::SetKey` (168 B) | `0x82487880` | EnvAnim.cpp block extended to `0x82487928`; map row. **The census called this `SetFrame`, which is `0x82486EE0` and already mapped. This one takes one float and calls the `Keys<>::Add`s, so it is `SetKey`.** | 100 (was 92.19) | same; codegen fix (test `mEnviron` directly, no reference local) |
| `BandCharDesc::NewObject` (112 B) | `0x823399C0` | BandCharDesc.cpp block extended to `0x82339A30`; map row | 100 | same |
| `GemTrackDir::UpdateFingerFeedback` (16 B) | `0x822E36B8` | GemTrackDir.cpp block extended to `0x822E36C8`; map row | 100 | same |
| `PerformanceData::SaveFixed` (2,552 B) | `0x82655788` | PerformanceData.cpp block extended to `0x82656180`; map row | 100 (was 98.48) | **DIFFERENT, fixed.** Ours narrowed the four streak and multiplier counts through `unsigned short` before writing them and testing against them. Retail writes and compares the full `int` `size()`. Visible only for a count above 65,535. |
| `TokenRedemptionPanel::Poll` (1,708 B) | `0x82641670` | TokenRedemptionPanel.cpp block extended to `0x82641D1C`; map row | 72.40 | **DIFFERENT, not fixed** (see below) |
| `TextFile::SetName` (84 B) | `0x8276D248` | DataFile.cpp block extended to `0x8276D29C`. DataFile.cpp `#include`s TextFile.cpp, and TextFile's other methods already pair in DataFile. Map row. | 100 (was 1.4) | **DIFFERENT, fixed.** Retail calls `Object::SetName`, releases `mFile` and returns. Ours also opened a file through Holmes for `*_append` names. That path is now `#if defined(MILO_DEBUG) && defined(HX_NATIVE)`. |
| `RndDir::Replace` (56 B) | `0x82402F68` (already mapped) | Anim.cpp block extended to `0x82402FA0` (Anim.cpp `#include`s Dir.cpp, and Anim.obj defines it) | 100 (was 12.8) | **DIFFERENT, fixed.** Retail calls `MsgSource::Replace(ref, obj)` before `RndTransformable::Replace`. Ours skipped it, so a RndDir never dropped a replaced or deleted sink from its MsgSource lists. This is on the load path, with 25 callers. |

### `TokenRedemptionPanel::Poll`: a real difference, left open

Retail's Poll (`0x82641670`) has logic our body lacks. It calls a virtual at slot 4 on the object at
`this+0x84`. It rebuilds a `vector<String>` at `this+0x64`: `_M_erase` on the whole vector, then a
`push_back` of every node of the list at `this+0x80`. It also guards a function-local static `Symbol`
(guard word `0x82E017EC`, bit 0x100) and tests whether that vector is empty before it sends a message.
Ours goes straight to a `token_redemption_error` state test. Fixing it means porting about 1.7 KB from
retail bytes. That is a lane, not a step, and it was not attempted inside this budget. The panel is off the
song path (store / token UI).

## Measurement

`total_code` did not move (10,247,844), because the pinned code was already counted in its `auto_*`
units.

**A/B, measured in-run** (`ab_measure.py --patch` with `git diff main pairup` applied on main `637a73bda`, run
`20261122-154753-pairup-all`, objdiff sha256 `ec3badc0…`, ruler `name_check`, both legs at a split fixed point):

| measure | leg A | leg B | Δ |
|---|---:|---:|---:|
| `matched_functions` | 55,221 | 55,265 | **+44** |
| `masked_equal` | 25,346 | 25,360 | +14 |
| honest (`matched − masked_equal`) | 29,875 | 29,905 | **+30** |
| code% | 59.553795 | 59.622105 | +0.068310 pp (**+7,000 B**) |
| fuzzy% | 64.589966 | 64.704510 | +0.114544 pp |

By unit, the +44 is StageKit +20, GemPlayer +9, FxSend +7, and +1 each in Anim, BandCharDesc, DataFile,
EnvAnim, File_Win, GemTrackDir, PerformanceData and Gem. Units at mpn 100 fell from 635 to 633: GemPlayer
(its ctor reads 99.99) and synth_xbox/FxSend (Cleanup plus the unidentified members now sit at 0 inside a
pinned unit). That is the expected accuracy cost of pinning code we cannot yet pair, not a regression of
any row. The `none` control moved +9,748 B; the tool calls that NOT_APPLICABLE for a source-carrying
patch. The run printed no LANDING block. Its `legB_symbols_fixed_point.diff` has the same blob
(`54e9b6e41`) as the `symbols.txt` committed in `7c9634e68`.

## Not done (with reasons)

- `ObjPtrList<T>::operator=` `0x8230C628` (220 B, unnamed row in SongSectionController's span). It is a
  16-spelling ICF fold (UH2 §"carried-over REFUTED pairs"), and naming it for one `T` charges every other
  spelling's callers unless an alias group is proved. Ours is also 204 B against 220 B (W16-UK's stack
  temporary residual). It needs fold adjudication (`tools/icf_pair_adjudicate.py`), not a bare map row.
- `vector<Object*>::_M_fill_insert` `0x82272A60` (108 B): already mapped, but the hole sits between two
  CalibrationPanel.cpp blocks and CalibrationPanel.obj does not define that instantiation. A pin there
  would read 0%. The owning unit is not identified.
- The StreakMeter `ObjPtr<T>` dtor and `Load` runs `0x822D6568..0x822D6EA0` (1,792 B): W16-UM found that no
  base obj defines them (NO-BASE-OBJ). Not touched.
- Small unidentified holes: PropAnim `0x82429BE0` (4 B tail branch to `0x82429958`), RGTutor `0x826F0F18`
  (8 B: `addi r3,r3,4; b map::clear`), Singer-area `0x826F8F68` (52 B: sets four floats at
  `+0x3C..+0x48`, then erases the whole `vector<GemManager::ArpeggioPhrase>` at `+0x280`). Each needs its
  owner named before a pin can pair it.
- `TokenRedemptionPanel::Poll` logic port (above). The StageKit codegen residue in four rows was not ground,
  because each one reads as behaviour-equal.
- The native health selftest was not run; the coordinator runs it before landing (the commits touch `src/`).
