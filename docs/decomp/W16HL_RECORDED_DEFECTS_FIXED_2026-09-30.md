# W16-HL — the defects W16-HI §6 and W16-HJ §4 recorded, fixed on retail bytes (2026-09-30)

Branch `w16-hl`, worktree `~/tmp/wt-w16-hl`, rebased onto main `90bbad025`. The brief:
fix the defects in `W16HI_SYSTEM_ANON_NAMING_2026-09-30.md` §6 and
`W16HJ_SEVEN_MAP_ROWS_RETAIL_ADJUDICATION_2026-09-30.md` §4, from retail bytes, staying out of
`src/system/{bandobj,rndobj,char,rnddx9}`. Every claim below comes from retail asm
(`build/45410914/asm/**`, keyed on the `.fn fn_<addr>` symbol, never the address column).

**Result:** one of the three headline defects was **refuted** (`RecurseInfo`'s size). Its three
rows were mis-named, and they are renamed and re-homed. The other two, plus five of the §4
follow-ups, are fixed. A/B over the whole branch diff: see §6.

## 1. Splash thunks (W16-HI §6)

| VA | was | now | retail evidence |
|---|---|---|---|
| `0x82741c28` | (anon) | `?SuspendFunc@@YAXXZ` | `lis/lwz TheSplasher; b 0x82741a00` (`Suspend@Splash`); `BeginSplasher` loads it into `r4` of `SetRndSplasherCallback(Poll, Suspend, Resume)` |
| `0x82741c38` | `SuspendFunc` | `?ResumeFunc@@YAXXZ` | `b 0x82741b18` (`Resume@Splash`); loaded into `r5` |
| `0x82742c98` | `ResumeFunc` | `?TaskMgrDeltaSeconds@@YAMXZ` | `addi r3,TheTaskMgr; b DeltaSeconds`. It is referenced once, from `fn_827450A0`, which stores it at `+0xa8` inside `if (<config "movie" "is_timed_movie">)`. That is rb3-Wii `Movie.cpp`'s `Movie::Impl::Impl()`, `mTimeCallback = TaskMgrDeltaSeconds;` (strings `lbl_82072E38 = "movie"`, `lbl_821038C8 = "is_timed_movie"`) |

All three VAs were dropped from `_bijection_arbitrary`. That list means "which name sits on which
VA is not established", and the branch destinations, the argument order and the store now
establish it. `SuspendFunc` and `ResumeFunc` both read **100**, where `SuspendFunc@0x82741c38` had
fallen to 98.33 once W16-HI named `Resume@Splash`. `TaskMgrDeltaSeconds` reads 0 (it was 76.7
under the wrong name). No obj of ours defines it, and its home, the Movie-impl code sitting in
the Splash unit's second block `0x82742C08`–`0x82743A38`, is not identified here.

## 2. HolmesClient `RecurseInfo` — the size defect is REFUTED; three map rows were wrong

W16-HI inferred "`RecurseInfo` is 24 B in ours, 16 B in retail" from the `0x10` stride of the row
mapped `push_back<RecurseInfo>`. Retail's callers show that row, and its two siblings, were never
`RecurseInfo`:

| VA | was | retail callers / body | now |
|---|---|---|---|
| `0x826a1808` | `__uninitialized_copy<RecurseInfo>` | only caller `band3/game/Scoring.s`; calls `_Copy_Construct<Scoring::StreakList>` (`0x826a1228`), stride `0x10` | `__uninitialized_copy<Scoring::StreakList>` |
| `0x826a1960` | `__uninitialized_fill_n<RecurseInfo>` | same | `__uninitialized_fill_n<Scoring::StreakList>` |
| `0x827a3fa0` | `push_back<RecurseInfo>` | only caller DataArraySongInfo (`bl` at `0x827A462C`); fast path copies a word at `+0` then runs `vector<int>`'s copy ctor (`0x827c1378`) on `+4`, which is `TrackChannels {SongInfoAudioType; vector<int>}` | `push_back<TrackChannels>` |

A scan of every retail function for a `{String, String}` copy (two `bl ??0String@@QAA@ABV0@@Z`,
`0x827BE608`) at a `0x18` stride finds none. **No retail evidence says `RecurseInfo` is 16 B, so it
stays `{String mDir; String mFile}` as in both oracles.** Its only "evidence" was these rows.

The HolmesClient unit was a set of single-function islands carved out of other TUs to pair these
wrong names. The four StreakList islands (`0x826A1808`, `0x826A1870`, `0x826A1960` and
`0x826A19C8`, i.e. two bodies and their two EH funclets) move to `band3/game/Scoring.cpp`, and
`0x827A3FA0` moves to `DataArraySongInfo.cpp`. HolmesClient keeps its two 100% rows
(`reserve<char>`, `PoolFree`). Alias group `0x827a3fa0`: the survivor is relabelled to
`push_back<TrackChannels>`, TrackChannels leaves `folded`, and the RecurseInfo spelling is withdrawn
with a `WRONG_MAP_NAME_AT_ADDRESS` record. The other 7 memberships are untouched.

Rows: `push_back<TrackChannels>` 99.79 → **100**. The StreakList `uninit_copy` / `fill_n` read
99.79 (one charge each: `_Param_Construct` vs retail's `_Copy_Construct<StreakList>`, a fold
name). The funclets read 64.7 → **100**.

## 3. `BandSongMgr::ClearSongCacheNeedsWrite` (W16-HJ §4)

Retail `0x825756d8` makes three out-of-line calls: `bl 0x827A8978`
(`SongMgr::ClearSongCacheNeedsWrite`, `stb 0 → 0xcd`), then `mUpgradeMgr` (`+0x158`) →
`0x8264D258`, then `mLicenseMgr` (`+0x15c`) → `0x8264E920` (`li r11,0; stb r11,0x38(r3); blr`, i.e.
`mCacheNeedsWrite = false`). `0x8264E920` sits between `LicenseMgr::ContentDir` and the next
LicenseMgr function.

- Added `void LicenseMgr::ClearLicenseCacheNeedsWrite()` after `ContentDir`, and the call. **The
  name is ours**: neither oracle has the method. It mirrors the existing `LicenseCacheNeedsWrite`,
  the same pair SongUpgradeMgr uses. `0x8264e920` (null since W16-HJ) is mapped to it.
- Retail also calls `SongMgr::SongCacheNeedsWrite` and `ClearSongCacheNeedsWrite` out of line from
  both BandSongMgr methods, but our DC3-copied `SongMgr.h` defined them in-class, so MSVC inlined
  the qualified calls. They are now out of line in `SongMgr.cpp`, as in rb3-Wii, placed in retail
  order (`0x827a8938` after `SongAudioData`, `0x827a8978` after `AlternateSongDir`). The bodies
  stay DC3's plain member read/clear, which already scored 100.

Rows: `ClearSongCacheNeedsWrite@BandSongMgr` **20.6 → 100**, `ClearLicenseCacheNeedsWrite` **100**,
and `SongCacheNeedsWrite@BandSongMgr` 88.75 → 99.79. Its one remaining charge is the
`LicenseCacheNeedsWrite` callee, which retail folds into `0x82660610` under the arbitrary name
`LocalizeToken@ShortcutNode`. That is an alias decision and is not made here.

## 4. W16-HJ §4 follow-ups

**`vector<hash_map<Symbol,String>>` family (`0x825A3650`–`0x825A3F58`).** This range was pinned to
`OriginalChoreoRemixer.cpp` / `SongCollision.cpp` under DC3 names (`set<MoveParent>`,
`BeatCollisionData`), and those rows paired only by structural coincidence (~99.7). Each retail
body was matched to our `BandSongMetadata.obj` spelling by size and call structure:

| VA | B | name | score |
|---|---|---|---|
| `0x825a35e8` | 96 | `__destroy_range_aux<reverse_iterator<hash_map*>>` (reverse loop, hashtable dtor; reached from both `~vector` and `_M_clear_after_move`) | 99.79 |
| `0x825a3650` | 60 | `_Copy_Construct<hash_map>` | 99.67 |
| `0x825a3708` | 96 | `__uninitialized_move<hash_map*>` (calls the move ctor `0x825a0708`, stride `0x1c`) | **100** |
| `0x825a3770` | 96 | `__uninitialized_fill_n<hash_map>` | 99.79 |
| `0x825a3800` | 220 | `_M_erase(__true_type)` | 99.73 |
| `0x825a38e8` | 256 | `_M_fill_insert_aux(__true_type)` | 99.77 |
| `0x825a3a10` | 116 | `_M_clear_after_move` | **100** |
| `0x825a3a90` | 136 | `~vector<hash_map>` | **100** |
| `0x825a3b48` | 336 | `_M_insert_overflow_aux(__false_type)` | **100** |
| `0x825a3ce8` | 176 | `_M_insert_overflow_aux(__true_type)`: STLport's self-reference guard, which copies `x` and calls the `__false_type` aux twice | 99.77 |
| `0x825a3dc0` | 112 | `_M_fill_insert` (calls `0x825a38e8` and `0x825a3ce8`) | **100** |
| `0x825a3e30` | 128 | `resize` (calls `0x825a3800` and `0x825a3dc0`) | **100** |
| `0x825a3eb8` | 112 | `operator>>(BinStream&, vector<hash_map<Symbol,String>>&)` | 99.82 |

6 rows were renamed and 7 anonymous ones named (the 7 through `gated_map_write.py`, P1–P7 pass).
All four ranges are re-homed to `BandSongMetadata.cpp`. Every sub-100 row's only charge is a
hashtable ctor/dtor callee name, i.e. a fold. Measured between builds: **+9 fns / +1,032 B**.

**`0x826758f8` `GetType@StoreSongSortNode`:** the 8 B pin moves from `Game.cpp` to
`StoreSongSortNode.cpp` (it was the first word of Game's mis-homed range, directly after
StoreSongSortNode's own run). 0 → **100**.

**ImmediateWidgetImp / MatWidgetImp:**

- *Allocator.* Retail `~ImmediateWidgetImp` clears through `0x8243dbf0`, which frees through
  `ReclaimableAlloc::CustFree` on the global pool (`gTransListAlloc`). Retail `RemoveInstances`
  and `DoPushInstance` call `list<Instance,TransformListAlloc>::erase` / `insert`, and retail
  `_S_sort` (`0x827e3c38`) ends in the same clear.
- *Source fix.* A `TrackWidgetList<T>` trait (`std::list<T>`, specialised to
  `RndMultiMesh::InstanceList`) keeps `TrackWidgetImp<T>`'s mangling unchanged.
  `MultiMeshWidgetImp::Instances()` loses its `reinterpret_cast`.
- *Map.* The four rows whose spellings carry the list type (`DoRemoveUntil`, `RemoveInstances`,
  `DoPushInstance`, `_S_sort`) are renamed. The relocation-free `DoRemoveUntil` alias group only
  relabels its survivor spelling.
- *Destructors.* Retail's two destructors have no own-vtable store at entry, which is the
  implicit-destructor shape (`patterns/fixable-declarations.md`). The user-declared `{}`
  destructors were removed.
- *Rows.* `~ImmediateWidgetImp` 83.37 → **100**, `~MatWidgetImp` 83.37 → 99.74 (the fold-named
  `list<MeshInstance>::clear` W16-HJ recorded). `Clear`, `DoPushInstance`, `RemoveInstances` and
  `Sort` of `TrackWidgetImp<Instance>` read **100**, `MultiMeshWidgetImp::Sort` 99.83 → **100**, and
  `_S_sort` 99.76 → 99.81.

**`0x8235fba8`: element type identified, pin not moved.** It is
`vector<set<Symbol>>::_M_insert_overflow_aux(__true_type)`. Two independent callees support that:
the element clear is `_Rb_tree<Symbol,…,_Identity>::clear`, and the element copy-ctor folds with
`set<TrackWidget*>`'s, which requires a 4-byte value (a `pair<int,float>` node is 8 bytes of
value). `SongSortMgr::SongFilter::filters` is `vector<set<Symbol>>` in our source and rb3-Wii.
Only the name was fixed. The `vector<set<Symbol>>` helpers around it are scattered across five
mis-pinned units (Stats, CharClip, Tour, NetGameMsgs, BandSongMetadata), several under
`map<int,float>` names, and 16 of our objs define the spelling. So finding its real TU is an
identification job, and the row stays at 0.

## 5. Deliberately not done

- **Hashtable alias** (`~hashtable<pair<const Symbol,String>>` ≡ `0x8256ac50`), the
  `LicenseCacheNeedsWrite` ≡ `0x82660610` fold, and the `_Param_Construct` / `_M_clear` fold names
  above. All are forgiveness decisions, and W16-HJ §4 itself defers those to their own lane.
- **`0x8263b0b0` re-home** out of `CharHair.cpp`. That edits a `char/` unit's pins, which another
  session owns.
- **`TaskMgrDeltaSeconds`'s home.** The Movie-impl block in the Splash unit and `fn_827450A0`
  (pinned in `DataNode.cpp`) need a Movie TU identification.
- **`vector<set<Symbol>>` cluster** (§4). **The HolmesClient `reserve<char>` / `PoolFree` islands**
  are left as they are (both 100).

## 6. Gates and measurement

- Both gates re-run after rebasing onto `90bbad025` (full build first), same counts.
- `python3 tools/map_name_injectivity.py`: `OK: 29677 applied rows, 29676 distinct names,
  injective (+1 enumerated internal-linkage exception)`, rc=0.
- `python3 tools/icf_alias_finder.py --validate` (full build first): `VALIDATE: PASS -- 1476
  map-consistent, 253 tolerated, 0 contradicted, 1730 total`, rc=0. The lane started at
  1477 / 252. The +1 tolerated is a STALE_SPELLING: one folded `list<Instance,StlNodeAlloc>`
  spelling no longer has a compiled referent. Pruning on that screen is measured harmful, so it
  was not pruned.
- one run, `python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-hl --patch <git diff main w16-hl -- src config scripts>`
  against main `90bbad025` (kinds map+source+splits; both legs at a split fixed point, 0 extra
  re-splits; leg B 198 MSVC recompiles, renamer patched 1834; objdiff-cli `c1b7d95240a35cd6` on
  both legs; run dir `.ab_measure_runs/20260930-121436-w16-hl-branch-vs-main-2010462/`).

  ```
  leg A: matched=44643 masked=23354 honest=21289 code%=42.380280
  leg B: matched=44666 masked=23358 honest=21308 code%=42.403310
  Δmatched=+23  Δmasked_equal=+4  Δhonest=+19  Δcode%=+0.023030pp  Δcode_bytes=+2360
  Δfuzzy=+0.011410pp   (51.216910 -> 51.228320)
  units: +13 BandSongMetadata, +4 TrackWidget, +3 Splash, +3 Scoring, +1 each BandSongMgr,
         DataArraySongInfo, TrackWidgetImp, LicenseMgr, StoreSongSortNode; -4 OriginalChoreoRemixer,
         -1 SongCollision (the re-homed family's rows, now counted in BandSongMetadata)
  units at 100%: 213 -> 214 (HolmesClient, DENOMINATOR_SHRANK: its 5 mis-homed rows left)
  `none` control: +2716 B (NOT_APPLICABLE: source in the patch)
  ```

  **Prediction vs measured.** Predicted about +21 fns / +1,790 B, summed from the in-worktree row
  diffs of each step: step 1 was hand-summed from rows at +7 / ~+300 B, then +9 / +1,032 B and
  +5 / +456 B measured between builds. Measured **+23 / +2,360 B**. The shortfall is in step 1,
  whose funclet re-pairings (e.g. the two 44 B HolmesClient funclets now paired in Scoring) were
  not all counted by hand. No row moved against its prediction's sign.
- `tools/native_build_gate.sh`: NATIVE_PLACEHOLDER
