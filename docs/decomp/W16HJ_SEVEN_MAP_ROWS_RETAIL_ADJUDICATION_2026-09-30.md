# W16-HJ — seven map rows settled on retail bytes (2026-09-30)

Branch `w16-hj`, worktree `~/tmp/wt-w16-hj`. Three rows were briefed
(`0x825a36b8`, and the two TrackWidget dtors W16-HB left unnamed). The
coordinator later added four W16-HE "wrong map name" rows (`FindFrameWithLeadIn`,
`__destroy_range_aux<NewReleaseEntry>`, the `_Slist_base` ctor, and the
`_List_iterator` ctor). Settling `0x825a36b8` turned up a source defect, and
fixing that touched two more rows (`0x825a0760`, `0x825a03c0`).

## Predictions (written before the A/B)

Row scores from main's `report.json`, compared with the branch build's rows.

| VA | before | after | Δmatched | Δcode | Δfuzzy B |
|---|---|---|---|---|---|
| `0x825a03c0` hash_map<Symbol,float> reader | 79.16 (mpn 79.81) | 100 | +1 | +124 | +25.8 |
| `0x8263b168` | 17.33 (mpn 18.79) | 100 | +1 | +96 | +79.4 |
| `0x825a36b8` | 99.70 (Leaderboard) | 99.75 (BandSongMetadata) | 0 | 0 | +0.04 |
| `0x825a0760` | 100 | 100 (renamed) | 0 | 0 | 0 |
| `0x8235fba8` (side effect, §1.4) | 99.77 | 0 (orphaned) | 0 | 0 | −175.6 |
| `0x827e2b30`, `0x827e2c98` | 0 / 0 | 83.37 / 83.37 | 0 | 0 | +126.7 |
| `0x82605c38` | 61.76 | 0 (nulled) | 0 | 0 | −42.0 |
| `0x8264e920` | 80.00 | 0 (nulled) | 0 | 0 | −9.6 |
| `0x826758f8` | 70.00 | 0 (unpaired in Game) | 0 | 0 | −5.6 |
| `0x8263b0b0` | 0 | 0 (unpaired in CharHair) | 0 | 0 | 0 |

**Predicted whole-binary: Δmatched_functions +2, Δmatched_code +220 B, Δfuzzy ≈ −1 B
(≈ 0 pp).** Call sites of renamed or nulled addresses are not modelled. The biggest
unknowns are the store-panel callers of `0x82605c38`, which lose a charged name and
gain a forgiven placeholder.

## 1. `0x825a36b8` — was `_Destroy_Range<LeaderboardRow*>`, is `_Destroy_Range<hash_map<Symbol,String>*>`

### 1.1 Retail evidence (none of it from the scorer)

- **The body** is an 80 B loop: `bl 0x8256ac50` on each element, stride `addi 0x1c`.
  Our `LeaderboardRow` instantiation strides **0x40** and calls `??1String@@UAA@XZ`.
  The name was wrong on the element size alone.
- **The callee `0x8256ac50`** (map: `~hashtable<pair<const int,String>>`, scoring 100 in
  AssetMgr) is a STLport hashtable destructor: `clear()`, then it frees the bucket
  vector at `+8..+0x10`, then `_M_erase_after` on the slist at `+4`.
- **The element type is `hash_map`, not `map`.** The retail reader `fn_825A3EB8`
  (`operator>>(BinStream&, vector<X>&)`) does four things:
  - default-constructs its temporary through `0x8256b6e8`, which passes
    `li r4,0x64` (100 buckets). That is STLport's `hash_map()`.
  - calls `resize`.
  - destroys the temporary with `0x8256ac50`.
  - reads each element with `0x825a0760`.

  The move-constructor `0x825a0708` copies the slist head at `+4`, the bucket vector at
  `+8..+0x10`, the element count at `+0x14`, and a **float at `+0x18`**
  (`max_load_factor`). An `_Rb_tree` has none of that.
- **Only one caller**: `fn_825A3EB8` is called only from `BandSongMetadata::Load`
  (`0x825a3f58`). It sits in the `rev >= 2 && rev < 0xE` block that our source spells
  `std::vector<std::map<Symbol, String> > gross; bs >> gross;`. The rb3-Wii oracle
  spells it the same way. **The oracle is wrong for this image.** The key and value
  types come from the element reader, which calls `>>Symbol`, then hashtable
  `operator[]` (`0x825a04a0`), then `>>String`.

### 1.2 Source fix (the lane's only source edits)

- `BandSongMetadata::Load`: `std::vector<std::map<Symbol,String> >` →
  `std::vector<std::hash_map<Symbol,String> >`.
- `BandSongMetadata.cpp`'s local `operator>>(BinStream&, hash_map<T1,T2>&)` template:
  removed `map.clear()` and changed the loop to `for (; size != 0; size--)`. Neither
  retail instance clears before reading: `0x825a03c0` (`<Symbol,float>`, i.e. `mRanks`)
  and `0x825a0760` (`<Symbol,String>`). Their loop is the shape of `BinStream.h`'s
  `std::map` reader. This alone takes `0x825a03c0` from 79.16 to **100**.

### 1.3 Map, splits and aliases

- `0x825a36b8` → `??$_Destroy_Range@PAV?$hash_map@VSymbol@@VString@@…@0@0@Z`. It scores
  **99.75**. The one charge left is the callee name: ours is
  `~hashtable<pair<const Symbol,String>>`, retail's map name is the `<const int,String>`
  spelling. Both our spellings reach retail `0x8256ac50`: this row's `bl`, and
  AssetMgr's 100% `hash_map<int,String>` users. So this is a genuine fold. **No alias
  was added.** That is a separate, forgiveness-shaped decision (see follow-ups).
- `0x825a0760`: `operator>>(BinStream&, map<Symbol,String>&)` →
  `operator>>(BinStream&, hash_map<Symbol,String>&)`. It scores 100 under both names;
  the old spelling no longer exists in our build.
- `splits.txt`: the `.text 0x825A36B8–0x825A3770` range moves from `Leaderboard.cpp`
  to `BandSongMetadata.cpp`. That range holds this row and `fn_825A3708`, the same
  vector's `__ucopy`. dtk re-derived `.pdata 0x82221708–0x82221718` to follow it. That
  re-derivation shows up as the split-guard's one-time "rewrote its own input" retry.
  Without the re-home the correct name could not pair, because `Leaderboard.obj`
  cannot define it.
- `symbol_aliases.json`: the group at `0x825a36b8` claimed a fold of three spellings:
  `_Destroy_Range<LeaderboardRow*>` as survivor, plus `_Destroy_Range<CheatProvider::Cheat*>`
  and `__destroy_range<Cheat*,Cheat>`. It was a T1 masked-bytes install, and masking
  hides exactly the callee. I **withdrew all three per membership, with records**
  (group kept, `folded: []`, survivor relabelled to the retail name):
  - The LeaderboardRow spelling's real body is the stride-0x40 `~String` loop at
    `0x8266d2e0`, the only such loop in retail. There our
    `__destroy_range<LeaderboardRow*,LeaderboardRow>` scores 100.
  - The Cheat spellings call `??1Cheat@CheatProvider@@QAA@XZ` (two `~String`), which
    cannot fold with a hashtable destructor.

  The group recorded "0 census sites", so the predicted cost is Δ0.

### 1.4 Side effect, recorded rather than chased: `0x8235fba8`

`?_M_insert_overflow_aux@?$vector@V?$map@VSymbol@@VString@@…` at `0x8235fba8`
(BandSongMetadata unit, 176 B) scored 99.77 on main. It paired only because our old
source instantiated `vector<map<Symbol,String>>`. Retail's body is an
`_M_insert_overflow` dispatcher whose element copy and destroy are `_Rb_tree` functions
(`0x82b9adf8` copy-ctor, `0x822dea78` `clear`). Its only caller is in `band3/game/Stats`
(`0x82360034`). So its element is some other `_Rb_tree` container. With the source
fixed, the name is an orphan and the row reads 0. It needs its own identification and
re-home. **Not done here**, to keep the scope. This row is the −175.6 B line in the
predictions.

## 2. TrackWidget derived destructors — named

| VA | name | evidence |
|---|---|---|
| `0x827e2b30` | `??1ImmediateWidgetImp@@UAA@XZ` | retail vtable `0x8211D55C` has RTTI `.?AVImmediateWidgetImp@@`; slot 0 = `??_G` `0x827e3ff0`, which does `bl 0x827e2b30` |
| `0x827e2c98` | `??1MatWidgetImp@@UAA@XZ` | retail vtable `0x8211D5EC` has RTTI `.?AVMatWidgetImp@@`; slot 0 = `??_G` `0x827e4040`, which does `bl 0x827e2c98` |

`MultiMeshWidgetImp` is refuted by its body: its destructor deletes the elements of a
`vector<RndMultiMesh*>` at `+4`, not a `list` clear. `tools/anon_proposal_adjudicate.py
--independent`, with each name as the other's runner-up, reads
**CONTRADICTED on both, and on the runner-ups too**. The contradictions are the same
two items on every leg, and neither one discriminates:

- `OURS_ONLY vt:` — our destructor stores its own vtable first; retail's does not.
  This costs the whole 83.37 → 100 gap, identically on both names.
- `RETAIL_ONLY callee` on `0x827e2b30`. Retail clears a
  `list<RndMultiMesh::Instance, TransformListAlloc<…>>`; ours clears a
  `list<…, StlNodeAlloc<…>>`. The element type agrees with ImmediateWidgetImp; the
  **allocator** disagrees.

Only the callee's element type separates the two names, and it separates them the way
the RTTI chain does: `0x827e2c98`'s callee folds with our `list<MeshInstance>::clear`
(adjudicator `FOLD`). I named both on the RTTI → `??_G` → `bl` chain. Both rows read
**83.37** (they were unpaired 0 before). The two source defects are recorded under
follow-ups and not touched here.

## 3. The four W16-HE rows

| VA | was | now | evidence |
|---|---|---|---|
| `0x82605c38` | `?FindFrameWithLeadIn@@YAMXZ` | **null** | Retail computes `MakeString(*g_fmt, PlatformSymbol(2), SystemLocale())` and returns it in r3, with no FPR (`0x8250fdf8` is a static-Symbol-table `PlatformSymbol` in Debug.s). It is called from StoreMainPanel, StoreMenuPanel and BandStorePanel. Ours is a Stats.obj *static* float that calls `TaskMgr::Beat` and `BeatToSeconds`. A scan of every compiled obj, statics included, found **no** function referencing both `MakeString<const char*,const char*>` and `SystemLocale`. So there is no name to give it. |
| `0x8263b168` | `__destroy_range_aux<reverse_iterator<NewReleaseEntry*>>` | `??$__uninitialized_copy@PAVNewReleaseEntry@StoreMainPanel@@PAV12@@…ABU__false_type@0@@Z` | The body is a forward copy loop at stride 0x3c calling `_Copy_Construct<NewReleaseEntry>`; our `__uninitialized_copy` has the same 96 B layout. Adjudicator (independent): proposal **SUPPORTED, fuzzy 100.0** (callee `FOLD` through the existing `_Param_Construct`≡`_Copy_Construct` alias). Runner-up (the old name) is contradicted: wrong callee (dtor vs copy-ctor), and **NAME_BOUND_ELSEWHERE `0x8263b0b0` ×2**. |
| `0x8263b0b0` | (unnamed) | `__destroy_range_aux<reverse_iterator<NewReleaseEntry*>>` | The old name moves to its witnessed home: a reverse loop at stride 0x3c calling `~NewReleaseEntry` (`0x8263a830`), called from StoreMainPanel. It is pinned in `CharHair.cpp`, which cannot define the name, so it reads 0 (it was already 0). |
| `0x8264e920` | `_Slist_base<pair<Symbol,vector<Symbol>>>` ctor | **null** | The body is `li r11,0; stb r11,0x38(r3); blr`, i.e. `LicenseMgr::mCacheNeedsWrite = false`. Its only caller is retail `BandSongMgr::ClearSongCacheNeedsWrite` (`0x825756d8`), which calls `SongMgr::ClearSongCacheNeedsWrite`, then `[this+0x158]` (mUpgradeMgr), then `[this+0x15c]` (**mLicenseMgr**) → here. Neither our source nor rb3-Wii has that LicenseMgr method, so the name can't be recovered from bytes. |
| `0x826758f8` | `_List_iterator<void(*)()>` ctor | `?GetType@StoreSongSortNode@@UBA?AW4SongNodeType@@XZ` (+ `_icf_arbitrary`) | The body is `li r3,7; blr`: it ignores `this` and returns a constant, so it cannot be a constructor. Retail RTTI puts it in **slot 21 of `StoreSongSortNode`'s vtable** and **slot 1 of `AccomplishmentTrainerListConditional`'s**. Our slot 21 / slot 1 are those classes' `GetType`, and both compile to `38600007 4e800020`. It is an ICF fold. I picked the StoreSongSortNode spelling by placement (immediately after `??0StoreSongSortNode` at `0x82675850`) and listed the VA in `_icf_arbitrary`. It is pinned in `Game.cpp`, which defines neither name, so it reads 0. |

## 4. Deliberately not done (follow-ups)

- **Alias** `~hashtable<pair<const Symbol,String>>` ≡ `0x8256ac50`. The fold is shown by
  two independent retail call sites, but an alias is forgiveness; that needs its own lane.
- **Source**: `BandSongMgr::ClearSongCacheNeedsWrite` is missing the
  `mLicenseMgr->…()` call and the out-of-line LicenseMgr clear method (`0x8264e920`).
  Retail also calls `SongMgr::ClearSongCacheNeedsWrite` out of line. The row is at 20.6.
- **Source**: ImmediateWidgetImp's list is `list<RndMultiMesh::Instance, TransformListAlloc>`
  in retail. Also, retail's `~ImmediateWidgetImp` and `~MatWidgetImp` do not store their
  own vtable.
- **Pins**: `0x826758f8` (Game.cpp → StoreSongSortNode) and `0x8263b0b0`
  (CharHair.cpp → StoreMainPanel) need re-homing to pair.
- **`0x8235fba8`**: identify its `_Rb_tree` element type (caller in Stats) and re-home (§1.4).
- **The rest of the `vector<hash_map<Symbol,String>>` family** at `0x825a3650–0x825a3f58`
  is still pinned to `OriginalChoreoRemixer.cpp` / `SongCollision.cpp` (DC3 TUs) under
  DC3 names (`set<MoveParent>`, `BeatCollisionData`). All of it is BandSongMetadata's
  template code.

## 5. Measurement

The command was
`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-hj --patch <git diff main w16-hj -- src config scripts>`,
run against main `c02c86962` after the second rebase. The patch is the **whole branch
diff**: 4 files, classified `map, source, splits`. Both legs were force-re-split and read
at a `symbols.txt` fixed point (0 extra re-splits). Leg B made 2 MSVC recompiles
(`BandSongMetadata.obj`, and `DataNode.obj`, which scatter-includes it) and
`renamer_patched=1834`. Ruler `name_check`; objdiff-cli sha256 `c1b7d95240a35cd6` on
both legs. Run dir: `.ab_measure_runs/20260930-103928-w16-hj-branch-vs-main-995160/`.

```
leg A: matched=44621 masked=23349 honest=21272 code%=42.350220
leg B: matched=44625 masked=23351 honest=21274 code%=42.353420
Δmatched=+4  Δmasked_equal=+2  Δhonest=+2  Δcode%=+0.003200pp  Δcode_bytes=+328
Δfuzzy=-0.000700pp   (51.136610 -> 51.135910)
units: +4 BandSongMetadata, +1 StoreMainPanel, +1 StoreMenuPanel, -2 DataNode
units at 100%: 212 -> 212 (mpn), 187 -> 187 (all-rows-fuzzy)
`none` control: +56 B (NOT_APPLICABLE: source in the patch)
```

**Prediction vs measured.** Predicted +2 fns / +220 B / ≈0 pp fuzzy; measured
**+4 / +328 B / −0.0007 pp**. The row-level diff of the two archived reports sums to
−68.6 B of fuzzy, which matches the headline. Every mover is listed below; the
unpredicted ones are in bold.

| row | A → B | Δcode | note |
|---|---|---|---|
| `0x825a03c0` hash_map<Symbol,float> reader | 79.16 → 100 | +124 | predicted |
| `0x8263b168` `__uninitialized_copy<NRE>` | 17.33 → 100 | +96 | predicted |
| **`fn_825A0554`, `fn_825A0594`** (BandSongMetadata, 64 B each) | 0 → 100 | +128 | the readers' EH funclets now pair |
| **`fn_8259F738`** | 99.9 → 100 | +40 | |
| **`StoreMenuPanel::OnBack`** (216 B) | 99.91 → 100 | +216 | its `bl 0x82605c38` was charged against the wrong `FindFrameWithLeadIn` name; nulled, the site is a forgiven placeholder |
| **`map<Symbol,String>::operator[]` `0x82744e48`** (DataNode, 196 B) | 100 → 0 | −196 | see below |
| **`fn_82744F0C`, `fn_82744F34`** (its funclets) | 100 → 99.5 / 99.4 | −80 | |
| `0x8235fba8` | 99.77 → 0 | 0 | predicted (§1.4) |
| renamed/nulled rows (§2, §3) | as predicted | 0 | |

**The DataNode loss is a correction cost, not a regression.** `DataNode.cpp`
scatter-includes `BandSongMetadata.cpp` so that `map<Symbol,String>::operator[]` can
pair at `0x82744e48`. The only thing in our build that instantiated that COMDAT was the
wrong `vector<map<Symbol,String>> gross`. Retail's user of `0x82744e48` is a different
function: `fn_82744F60`, called from WavMgr, which indexes a member map at `+0xb8`.
That code is not in our source. The row's name is right; our build just no longer emits
the body. I **deliberately did not** add an explicit instantiation to keep it pairing.
It would restore 196 B, but it is a pairing contrivance with no retail user in our
source, and it should be a separate, recorded decision (follow-up).

Also: `TourProgress.cpp` noted that the tree carried two different local
`hash_map` `operator>>` templates (SongMgr's without `clear()`, BandSongMetadata's with
it), "a live ODR hazard". The §1.2 fix makes the copies agree, and retail bytes show
the no-`clear()` form is right.

## 6. Gates

- `python3 tools/icf_alias_finder.py --validate` on the final branch build (full
  `./tools/ninja-locked` first): `VALIDATE: PASS -- 1477 map-consistent, 252 tolerated
  (enumerated above), 0 contradicted, 1730 total`, rc=0. Before the lane: 1479 / 250.
  - The `0x825a36b8` group moved OK → UNWITNESSED → OK: relabelled to a map-resident
    survivor with an empty `folded`.
  - STALE_SPELLING +2: huge groups whose folded lists include `vector<map<Symbol,String>>`
    spellings we no longer compile. Kept; pruning on that screen is measured harmful.
- `tools/native_build_gate.sh` is run last; its `NATIVE_GATE_RESULT` line is in the lane
  report. `BandSongMetadata.cpp` is under `src/band3`, so the gate is required.
