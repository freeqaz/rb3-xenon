# Unicorn DIVERGENT `logic` rows in native scope, adjudicated on retail bytes (2026-11-22)

Lane **UNIDIV**, branch `unidiv`, worktree `~/tmp/wt-unidiv` (from main `4ff9de1ae`).
Brief: take the unicorn rows marked DIVERGENT with divergence class `logic` in native
scope (src/band3 + the src/system dirs native uses; not src/network, not xdk), decide on
retail bytes whether our source behaves differently, and fix the real ones.

## 1. The briefed population is empty

Tested literally before building on it:

| source | rows with class `logic` |
|---|---:|
| `decomp.db` `functions.unicorn_class` (tested 2026-07-16, the pre-correction fixture) | **0** of 3,854 DIVERGENT |
| 09-10 corrected-fixture corpus (`unicorn_corpus_2026-09-10.csv.gz`, 22,718 fns) | **0** |

`scripts/unicorn_runner/comparator.py` split the old `logic` bucket into fine classes
(`call_count`, `call_arg`, `return_value`, `object_memory`, `error`, …) and kept `logic`
only as a "should be minimal" fallthrough; `UNICORN_DEEP_SCHEDULE_2026-09-10.md` §5a
already recorded it as 0. `scripts/unicorn/query.py`, named by the `/unicorn-query`
skill, does not exist.

**Substitute population (my choice, stated):** the two fine classes the comparator
calls real bugs that an inline-policy difference cannot produce, `return_value` and
`error`, from the 09-10 screened worklist (`unicorn_matched_but_wrong_2026-09-10.csv`,
`SURVIVES_BOTH_SCREENS`), restricted to native scope. I also checked the 5
`unmapped_access_mismatch` survivors. I did **not** take `call_count` (218 survivors,
confounded by inline policy, per the 09-10 caveat) or `call_arg`/`object_memory`
(mostly harness mock-region artifacts, per `final_worklist.py`).

Scores below are from a fresh full build of this worktree (ruler `name_check`,
55,265 matched before the change).

## 2. Verdicts

| row (09-10 unit) | 09-10 class / fuzzy | now | verdict | evidence |
|---|---|---|---|---|
| `_String_base::_M_Start` (Accomplishment) | return_value 22.17 | VA renamed | **WRONG MAP NAME, fixed since** | 0x82275050 is `PatchDir::NumLayers` (`53aea21a4`, W16-IA); now 100 |
| `ObjectDir::Main` (Matchmaker) | return_value 80.0 | VA nulled | **WRONG MAP NAME, fixed since** | 0x823f3eb8 nulled in `53aea21a4` |
| `??_GPracticeSection` | return_value 99.59 | renamed | **WRONG MAP NAME, fixed since** | that pin is CharTransCopy (`698096d57`) |
| `RndCam::Current` (PropKeys) | return_value 80.0 | renamed | **WRONG MAP NAME, fixed since** | 0x82421758 is a store, `SetPropKeysRev` (`b60279993`); now 100 |
| `LocalUser::GetLocalUser` (User) | return_value 99.50 | renamed | **WRONG MAP NAME, fixed since** | 0x825238F0 is `RemoteUser::GetRemoteUser` (`subi r3,r3,0x18`; `f8c75534c`); now 100 |
| `list<FileCache*>::erase` | return_value 0.00 | renamed | **WRONG MAP NAME, fixed since** | 0x82276828 is `_Stl_prime<bool>::_S_next_size` (`7addc8565`); now 100 |
| `__uninitialized_fill_n<SongCollisionOutput>` | return_value 56.29 | renamed | **WRONG MAP NAME, fixed since** | 0x82787718 re-homed in `8181ed8d2`; now `vector<RGRollChord>::_M_erase` |
| `_List_iterator<void(*)()>` ctor (Game) | return_value 70.0 | renamed | **WRONG MAP NAME, fixed since** | 0x826758f8 is `StoreSongSortNode::GetType` (`li r3,7`; `f1276c36f`); now 100 |
| `Handle@BandCharDesc$4` thunk | error 93.33 | 100 / 100 | **NO DIVERGENCE NOW** | fuzzy 100 at `name_check` |
| `RndBitmap::Load` | error 100 | 100 / 100 | **ARTIFACT** (09-10 doc §1b: byte-identical outside relocs) | — |
| `RndBitmap::LoadHeader` | error 49.98 | 100 / 100 | **NO DIVERGENCE NOW** | fuzzy 100 |
| `CamShot::EndAnim` | error 57.29 | 100 / 100 | **NO DIVERGENCE NOW** | fuzzy 100 |
| `CamShot::StartAnim` (0x824C1620) | error 8.41 | 99.93 / 100 | **MATCHES RETAIL** | one charged site, idx 108: `add r10,r11,r28` vs ours `add r10,r28,r11` (the `mCrowds[i]` address). Integer add commutes |
| `GemPlayer` ctor (0x826C8470) | error 0.00 | 99.99 / 99.99 → **100 / 100** | **MATCHES RETAIL; alias gap closed** | one charged site, idx 668 (0x826C8EE0): retail `bl 0x82453970` = `reserve<Vector2>`, the ICF-fold survivor for 8-byte elements (`srawi 3`), where we call `reserve<UpcomingFretRelease>` (`{int,float}`, 8 B). `icf_pair_adjudicate --chase`: flat T1 REFUTED, **CHASED T1 PROVEN**, 0 cycle-assumed → alias member admitted (`967f69d29`) |
| `RndGenerator::SetFrame` | error 99.91 | 99.91 / 100 | **MATCHES RETAIL** | idx 88: `fadds f0,f0,f31` vs `f31,f0`. IEEE add commutes (09-10 §1b already) |
| `StreakMeter::SetType` | error 0.00 | 100 / 100 | **WRONG MAP NAME, fixed since** | vtordisp thunk-name permutation repaired 09-14 (`3ffdbbf79`), renamed 10-01 (`c49803204`) |
| `UIList::Copy` | error 64.87 | 100 / 100 | **NO DIVERGENCE NOW** | fuzzy 100 |
| `InterstitialMgr` ctor | error 99.73 | 100 / 100 | **NO DIVERGENCE NOW** | fuzzy 100 |
| `Handle@CharWeightable$4`, `Handle@MultiSelectListPanel$4` | error 95 | — | **WRONG MAP NAME, fixed 09-10** | `UNICORN_FIX_2026-09-10.md` rows 2/3 |
| `Normalize(Vector3)` (mtx) | unmapped_access 0.00 | 99.375 / 100 | **MATCHES RETAIL** | idx 25/27: `fmuls f12,f10,f0` vs `f0,f10` (and f11). IEEE mul commutes |
| `list<MidiParser*>` copy ctor (Song), `list<RndMesh*>` ctor (DefaultPhysicsManager), `ObjPtrList<RndTransformable>::operator=` (TrackPanelDirBase), `RndGroup::Draw` (ViewSetting) | unmapped_access | renamed | **WRONG MAP NAME, fixed since** | `c83fa7001`, `78c7339a4`, `8c8c4722e` (0x82357640 is `ReapplyConfiguration`), `2c442ddbe` |

Out of scope and skipped: the two `RockCentral` `reverse_iterator` rows (`net_band`;
also dtk over-carves merged in `c949bc698`), and `aes`/keygen (crypto, byte-identical).

**Summary: 0 real behavioural divergences in native scope.** Of 25 rows checked:
15 were wrong map names, so the emulator diffed our body against a different retail
function (all corrected by other lanes between 09-14 and 10-07; 2 were corrected on
09-10); 5 now score fuzzy 100; 1 is a byte-identical artifact; 3 are commutative
operand swaps with no behavioural content; and 1 (GemPlayer ctor) was an ICF alias
gap, now closed.

## 3. Measurement

Pre-registered: the GemPlayer ctor's single charged site disappears, so the row crosses at both
rulers: Δmatched_functions +1, Δmatched_code ≈ +2,704 B (its report size), no other row moves.

`tools/ab_measure.py --patch` on the combined change, measured as a **revert** (`git diff HEAD
4ff9de1ae`; leg A = with the change, leg B = without). `--pick` applies on top of HEAD, which
already held the commit, so the first attempt was not a baseline and was refused anyway by the
`CHECK ALIAS CALLEE NAMES VS MAP` guard (a new member needs `alias_callee_name_drift.py --reprove
--write`, now commit `340a5bbf0`: group re-proved, 8/8 memberships PROVEN). Ruler `name_check`,
objdiff `sha256:ec3badc0f33323e9`, both legs at a split fixed point, 0 recompiles.

| | leg A (with) | leg B (without) | change (A − B) |
|---|---:|---:|---:|
| matched_functions | 55,266 | 55,265 | **+1** |
| honest (matched − masked_equal) | 29,906 | 29,905 | **+1** |
| code% | 59.648490 | 59.622105 | **+0.026385 pp** (+2,704 B) |
| fuzzy | 64.704510 | 64.704510 | 0 |
| `none` control | 55,321 / 60.015200 | 55,321 / 60.015200 | 0 (expected: alias forgiveness lives on the `name_check` ruler) |

The only unit that moved is `default/GemPlayer`, 382 → 383 of 383 rows, which now reaches 100% on
the mpn ruler. Log: `~/tmp/unidiv_ab2.log`; result
`~/tmp/wt-unidiv/.ab_measure_runs/20261122-182530-unidiv_revert-1304380/result.json`. No
LANDING diffs were emitted (symbols/splits already at their fixed point).

This is an **accuracy and bookkeeping** gain, not a behaviour fix: the ctor already called a body
identical to retail's.

## 4. What I did not do

- Did not re-run the unicorn harness: every verdict above rests on retail bytes, git
  history and the current objdiff diff. `decomp.db`'s unicorn columns (07-16,
  pre-correction fixture) are stale and were not rewritten.
- Did not adjudicate the 218 `call_count` and 42 `call_arg` survivors. They are the
  next population if anyone wants one, and need the inline-policy screen first.
- Did not touch the three commutative-operand rows (permuter-class; permuter off by
  directive).
- No `src/` change, so no native gate was required. I ran no native binaries.
