# W16-KE — dtk mis-carves fixed in `symbols.txt` (2026-10-01)

**Branch `w16-ke`, off main `e762a9298`. Not merged.**

Several lanes parked rows whose source was right but whose retail function dtk had cut short. This lane
collected them, derived each true extent from retail bytes, and fixed `symbols.txt` by hand. It is the
first hand edit of `symbols.txt` in this repo; earlier lanes only committed the split's own rewrites.

**Whole branch, main → tip:** **+28 functions / +28 honest / +4,924 B / +0.047760 pp code /
+0.023418 pp fuzzy**, units at 100 **338 → 346** (`mpn`) and **284 → 292** (all-rows fuzzy). No surviving
row went down, and no unit fell off 100. Six rows at 100 left the denominator because no retail function
starts at their address (§5.3).

| commit | what |
|---|---|
| `4121c3e56` | `splits.txt`: four tails re-homed to their function's TU, and the blocks they cut merged |
| `76e9f030f` | `symbols.txt`: 34 heads grown to their retail extent, 60 fragments removed |
| `18bbfe799` | map: 14 keys inside the merged extents dropped, `0x82422C38` renamed `KeyGreaterEq<Color>` |
| `e6059aaa9` | `DeJitter::NewMs` written in retail's shape |
| `2953bf72c` | `KeylessHash<void*,AllocInfo*>::Remove` written in retail's shape |
| (this doc) | `tools/carve_extent_scan.py` and this doc |

## 1. What was collected

| doc | row | outcome |
|---|---|---|
| W16-JE §7 | `Keys<T>` 0x82422C38 (KeyLessEq/KeyGreaterEq) | fixed; renamed `KeyGreaterEq<Color>`, 62.8 → 100 (192 B) |
| W16-JC §3.4/§5 | `ProfileMgr::GetSongToTaskMgrMs` switch tail | fixed; tail re-homed from CharServoBone, 0 → 100 |
| W16-JC §5 | `PerformanceData::Prune` tail `fn_82657570` | fixed, 82.2 → 100 |
| W16-JC §5 | `CrowdRating::GetThreshold` return stubs | fixed, 71.4 → 100 |
| W16-JC §5 | `GetConfigNameFromAssetType` / `GetAssetTypeFromCurrentState` jump-table bodies | fixed, 0 → 100 each |
| W16-ID §4, JC §5 | `??0TrainerGemTab` (tail `fn_826EFC98`) | fixed, 8.3 → 100 |
| W16-ID §4, JC §5 | `BandUser::DeletePlayer` | fixed (a 4-B dead `blr` sat in a gap), 90 → 100 |
| W16-ID §4 | `??0PerformerStatsInfo` (tail `fn_8257C238`) | fixed, 90.3 → 100 (0x88 B, not the 0x84 ID quoted) |
| W16-ID §4 | `GemSmasher::CodaHit` | fixed, 93.8 → 100 |
| W16-ID §4 | `GemSmasher::CodaHitChord` | already 100 on main; its dead `blr` is inside its symbol |
| W16-ID §4 | `ClosetMgr::PlayFinalizedSound` | fixed, 99.0 → 100 |
| W16-JA §3.4 | "a dtk mis-carved Matrix4 product" | **not located.** JA gives no address, and the defect it names is our shared `Hmx::operator*` operand order, a header-wide source change |
| W16-JD §8 (not in the brief's list, same family) | `GetMaxSlots@BeatMatcher`, `??0ADSRImpl`, `DeJitter::NewMs`, `KeylessHash::Remove`, `??1CharTransDraw` | fixed, all → 100. NewMs and Remove also needed body fixes (§4.3) |
| W16-JD §8 | `GameGem::operator=`, `__median<float>` | **not located.** No map row carries either name, and the scan (§2) flags neither |

Eighteen more came from the scan (§2), each corroborated by our compiled size. The full table is §3.

## 2. How each extent was derived

Retail `.pdata` bounds a function but does not delimit it here. Every row in this lane is a **leaf**
(no stack frame, so no unwind record), and the fragments dtk carved are pdata-less. So `.pdata` gives an
upper bound only: the next BeginAddress. The extent itself comes from control flow:

- Walk from the head, following fall-through and every non-call branch whose target lies before that
  bound. Byte and halfword jump tables behind a `bctr` are decoded from the `lis/addi; lbzx|lhzx;
  [rlwinm]; lis/addi; add; mtctr; bctr` sequence, with the entry count taken from the preceding `cmplwi`.
- Stop at the next EH prefix (`except_data_*`), and never absorb a symbol that is a `.pdata` start or has
  any code or data reference from outside the walked span.
- Add one trailing unreferenced `blr` directly after the last terminator. MSVC leaves a dead epilogue
  `blr` after a tail call (`b`/`bctr`), and dtk carves it as its own 4-B function. Six of the map names
  removed in §4.2 sat on exactly such a `blr`.

The instrument is `tools/carve_extent_scan.py` (`--base` adds our compiled size for the head's map name).
On main it reports 329 heads whose reached extent is larger than their carve.

**Which ones were fixed.** A head was taken when it is named in the map, its row is below 100, and our
independently compiled function is exactly the reached extent (34 rows). The 16 listed rows were taken
regardless. 13 of them meet that test. `Keys` meets it only under its correct name (§4.2), and `NewMs`
and `KeylessHash::Remove` had bodies that differ from retail (§4.3). One candidate (`??__Fdlc_motd`) was dropped because its row sits in an
unpinned `auto_` unit. The result is **34 heads**.

**Controls, each of which could have failed:**
- **Hand derivations.** The 16 listed rows were derived by hand from the disassembly before the scan
  existed. The scan reproduced all 16 extents exactly.
- **A second implementation.** jeff's own over-carve merge (`merge_branch_reached_overcarve_tails`)
  cannot fire across a split-unit boundary. When the first A/B leg merged the re-homed blocks without the
  hand edit (§5.1), jeff grew four heads on its own: `GetSongToTaskMgrMs` 0x6C, `Prune` 0x124,
  `LiteralFloat` 0x2C and `DecodeUTF8` 0xB0. All four equal the hand/scan extents
  (`~/tmp/w16ke-ab/jeff_own_merges_main_plus_splits.diff`).
- **Our compiled size.** For 32 of 34 heads the reached extent equals our compiled function byte for
  byte.
- **Precision on a population that is already right.** Of about 48,900 rows at fuzzy 100 on main, the
  scan flags 4. None of the four absorbs another symbol; they only take in gap bytes that objdiff's size
  inference already counts.
- **Fixed point.** After one recovery build (the split renamed the two jump tables `jumptable_*` once
  their functions owned them, and merged one `.pdata` pair), the split reproduced `symbols.txt` and
  `splits.txt` byte for byte on every later build. That covers both A/B legs (sha chain
  `1b92b3bd -> 1b92b3bd`, 0 extra re-splits).

## 3. The 34 functions

`old → new` is the carved size. *frag* is the number of fragments absorbed. Fuzzy is main → tip,
measured on the A/B reports.

| retail | function | unit | size | frag | fuzzy | from |
|---|---|---|---|---:|---|---|
| `0x822d4258` | `EndingBonus::SetProgress` | EndingBonus | 0x20 → 0x24 | 1 | 87.5 → 100 | scan |
| `0x8232afb0` | `BandWardrobe::MostImportantHuman` | BandWardrobe | 0x74 → 0x84 | 1 | 90.0 → 100 | scan |
| `0x823c8a88` | `CharTransDraw::~CharTransDraw` | CharTransDraw | 0x14 → 0x8c | 3 | 0 → 100 | JD |
| `0x823e1110` | `__find<User**, LocalUser*>` | NetSession | 0x13c → 0x18c | 1 | 79.3 → 100 | scan |
| `0x82422c38` | `Keys<Color,Color>::KeyGreaterEq` (was `KeyLessEq`) | PropKeys | 0xb0 → 0xc0 | 2 | 62.8 → 100 | JE |
| `0x8243d840` | `ScaleXfms` | rndobj/Utl | 0xc → 0xa0 | 2 | 0 → 100 | scan |
| `0x8247c7d8` | `RndGenerator::DrawParticleSys` | Gen | 0x34 → 0x44 | 1 | 69.2 → 100 | scan |
| `0x825459f8` | `ProfileMgr::GetSongToTaskMgrMs` | ProfileMgr | 0x30 → 0x6c | 3 | 0 → 100 | JC |
| `0x82566b38` | `ClosetMgr::PlayFinalizedSound` | ClosetMgr | 0x198 → 0x19c | 1 | 99.0 → 100 | ID |
| `0x8257c1b8` | `PerformerStatsInfo` copy ctor | MetaPerformer | 0x7c → 0x88 | 1 | 90.3 → 100 | ID |
| `0x825eeb70` | `GetConfigNameFromAssetType` | AssetTypes | 0x3c → 0x108 | 6 | 0 → 100 | JC |
| `0x82615188` | `CustomizePanel::GetAssetTypeFromCurrentState` | CustomizePanel | 0x34 → 0xd4 | 2 | 0 → 100 | JC |
| `0x8264bf10` | `AppMiniLeaderboardDisplay::SetLeaderboardStatus` | AppMiniLeaderboardDisplay | 0x8c → 0x98 | 1 | 91.4 → 100 | scan |
| `0x82657478` | `PerformanceData::Prune` | PerformanceData | 0xf8 → 0x124 | 1 | 82.2 → 100 | JC |
| `0x8268b1f0` | `BandUser::DeletePlayer` | BandUser | 0x28 → 0x2c | 0 | 90.0 → 100 | ID/JC |
| `0x8268b888` | `RemoteUser::UserName` (`$R4` thunk) | BandUser | 0x18 → 0x1c | 1 | 83.3 → 100 | scan |
| `0x8268e3f8` | `BandUser::IsNullUser` (`$R4` thunk) | BandUser | 0xc → 0x20 | 1 | 0 → 100 | scan |
| `0x826cfa88` | `PracticeSectionProvider::DataSymbol` | PracticeSectionProvider | 0x14 → 0x4c | 4 | 0 → 100 | scan |
| `0x826ee4e0` | `CrowdRating::GetThreshold` | CrowdRating | 0x38 → 0x48 | 2 | 71.4 → 100 | JC |
| `0x826efc38` | `TrainerGemTab::TrainerGemTab` | TrainerGemTab | 0x60 → 0xb8 | 1 | 8.3 → 100 | ID/JC |
| `0x8270f490` | `MicClientMapper::GetMicIDForClientID` | MicClientMapper | 0x60 → 0x64 | 1 | 95.8 → 100 | scan |
| `0x82729cc0` | `ADSRImpl::ADSRImpl` | ADSR | 0x44 → 0x60 | 1 | 58.8 → 100 | JD |
| `0x8274a978` | `DataNode::LiteralFloat` | DataNode | 0x24 → 0x2c | 1 | 77.8 → 100 | scan |
| `0x8274aba0` | `DataNode::operator==` | DataNode | 0x1cc → 0x1d8 | 1 | 98.2 → 100 | scan |
| `0x8278eb28` | `GameGem::RightHandTap` | GameGem | 0x24 → 0x44 | 2 | 58.3 → 100 | scan |
| `0x8278ecb8` | `GameGem::GetNumStrings` | GameGem | 0x30 → 0x34 | 1 | 91.7 → 100 | scan |
| `0x82790a90` | `BeatMatcher::SetSyncOffset` | BeatMatcher | 0x18 → 0x1c | 1 | 83.3 → 100 | scan |
| `0x827919d8` | `BeatMatcher::GetMaxSlots` | BeatMatcher | 0x2c → 0x4c | 1 | 53.8 → 100 | JD |
| `0x827cc6c8` | `DecodeUTF8` | UTF8 | 0xa0 → 0xb0 | 2 | 89.4 → 100 | scan |
| `0x827d0a10` | `DeJitter::NewMs` | DeJitter | 0x54 → 0x128 | 5 | 0 → 100 | JD |
| `0x827d4c20` | `KeylessHash<void*,AllocInfo*>::Remove` | MemTracker | 0x4c → 0x94 | 3 | 0 → 100 | JD |
| `0x827dc090` | `GetNextLine` (HttpGet anon ns) | HttpGet | 0x1c → 0x98 | 4 | 0 → 100 | scan |
| `0x828175c0` | `UIGridSubProvider::UpdateExtendedText` | UIGridProvider | 0x28 → 0x2c | 1 | 90.0 → 100 | scan |
| `0x82bafb58` | `GemSmasher::CodaHit` | GemSmasher | 0x40 → 0x44 | 1 | 93.8 → 100 | ID |

Shapes seen:
- a head ending in a conditional return, with the rest carved off (TrainerGemTab, Prune, GetThreshold,
  GetMaxSlots, KeylessHash::Remove);
- a dead `blr` after a tail call (CodaHit, PlayFinalizedSound, UpdateExtendedText, SetSyncOffset,
  GetMicIDForClientID, the `$R4` thunks);
- a byte jump table whose arms dtk made into functions (the two AssetType lookups);
- an instruction left in a gap between two symbols (DeletePlayer, PerformerStatsInfo, CharTransDraw).

## 4. The other edits

### 4.1 `splits.txt`

Four functions crossed a `.text` block boundary into another TU. dtk refuses a split that ends inside a
symbol, so each block moved to the function's TU:

| block | was | now |
|---|---|---|
| `0x82545A28–0x82545A64` (GetSongToTaskMgrMs switch tail) | CharServoBone.cpp | ProfileMgr.cpp |
| `0x827CC768–0x827CC770` (DecodeUTF8 tail) | BufStream.cpp | UTF8.cpp |
| `0x827D0A64–0x827D0B40` (NewMs body, plus the DeJitter ctor's EH prefix) | MeasureMap.cpp | system/utl/DeJitter.cpp |
| `0x828175E8–0x828175EC` (UpdateExtendedText dead `blr`) | UIComponent.cpp | system/ui/UIGridProvider.cpp |

Adjacent blocks of one TU were merged wherever a grown function would otherwise straddle them:
ProfileMgr, UTF8, DeJitter, UIGridProvider, PerformanceData, MicClientMapper and DataNode. Every donor
TU keeps other blocks, so no unit vanishes. The UIGridProvider `.pdata` pair merge is the split's own
re-derivation.

### 4.2 Map (`target_symbol_map.json`)

- **14 keys removed (9 named, 5 null).** Each one sits inside a grown function, and nothing outside that
  function references its address in code or data:
  - Six named keys sat on a dead `blr`: `_Destroy_Range` ×3, `_Destroy_Moved_Range`, the
    `_Rb_tree<TrackType>` dtor, and `BeatMatchControllerSink::ReleaseSwing`.
  - `0x82422CE8` (`KeyGreaterEq<Color>`) was the 8-B loop tail of `0x82422C38`; the name moved to the
    head.
  - `0x82615254` was named `ContentAltDirs`. It is the `li r3,0; blr` default arm of
    `GetAssetTypeFromCurrentState`'s table, reached only from `0x82615190`.
  - `0x82422CF0` was named `AsBoolKeys@PropKeys`. It is the return-0 arm of `0x82422C38`, reached only
    from `0x82422C44` and `0x82422C50`. Retail's `PropKeys` vtable sends every `AsBoolKeys` slot to the
    return-null survivor `0x823591E8`.
  - The remaining five were null keys. (The map commit's message says six; it is five.)
- **`0x82422C38` renamed `KeyLessEq<Color>` → `KeyGreaterEq<Color>`.** It is one 192-B function: a
  binary search for the first key with frame >= f, then a walk back over equal frames, stride 0x14. Its
  callers are `Keys<Quat>::FindBounds` and `Remove` and the `Add` mapped `Keys<Vector3>`. `0x82422CF8`
  (`KeyLessEq<Quat>`, 100) is the other half of the pair. Our `KeyGreaterEq<Color>` is 192 B, and the row
  reads 100.
- The injectivity count drops 32,776 → 32,767 applied rows, and the validator is unchanged
  (1,609 / 268 / 0) between main and tip.

### 4.3 Two bodies

With the carve fixed, two rows paired against their whole retail body for the first time, and both
bodies differed:
- **`DeJitter::NewMs`** (0 → 100, 296 B). Retail TU5 has no time-scale path and no `dejitter_disable`
  `DataVariable` check. It clamps the prediction to ±16 ms of the sample (`0x82048870` = 16.0f), where
  ours used ±33 ms. Nothing in the tree writes `sTimeScale`, so the removed branch was dead.
- **`KeylessHash<void*,AllocInfo*>::Remove`** (0 → 100, 148 B). Retail stores `mRemoved` first, wraps the
  next index with a mask, and walks back with a test-first loop that empties the slot it is on before
  stepping back. Ours stepped back first and emptied the slot before. Retail's index is the element index
  shifted right by 2 more (`srawi 2; srwi 2`). That shift is kept as written: it is what retail computes.

## 5. Measurement

### 5.1 Why three runs

`ab_measure` refuses a patch that touches `symbols.txt`, so the hand edit cannot be a patch. Both legs
read the committed file. The W16-IC/JC setup puts it in leg A instead. Here that would put the main
effect inside leg A, so the main baseline was measured separately in a sister worktree:

1. **Refused, correctly.** In `~/tmp/w16ke-ab/wt0` at main, with the `splits.txt` commit as the patch,
   leg B's split rewrote `symbols.txt`. With the blocks merged, jeff's own over-carve merge grew four
   heads (§2), and the split guard failed the build. This is the same refusal W16-JC hit. It is the reason
   the splits commit cannot be measured apart from the symbols commit.
2. **Main control**, `wt0` at main with a comment-only patch to `CrowdRating.cpp`. Leg B recompiled
   exactly 1 TU, and Δ is **0 on every key**. Its leg A is the main baseline.
3. **Branch**, `~/tmp/w16ke-ab/wt` at `76e9f030f` (main + splits + symbols), patch `76e9f030f..2953bf72c`
   (map + source). The kinds are map and source. Leg B recompiled 994 TUs, made one split, and the
   renamer patched 1,861 files.

Both worktrees were made with `scripts/setup_worktree.sh` next to `jeff`/`objdiff` symlinks to the live
forks. objdiff-cli was `c1b7d95240a35cd6` on both runs, ruler `name_check`. Run dirs:
`wt0/.ab_measure_runs/20261001-121352-w16ke-main-control-1296441` and
`wt/.ab_measure_runs/20261001-122413-w16ke-final-1375528`.

### 5.2 Numbers

| | matched | honest | matched_code | code % | fuzzy % | units@100 mpn / fuzzy |
|---|---:|---:|---:|---:|---:|---|
| main (run 2 leg A) | 48,897 | 24,674 | 5,083,956 | 49.613705 | 58.313725 | 338 / 284 |
| + splits + symbols (run 3 leg A) | 48,922 | 24,699 | 5,088,244 | 49.655260 | 58.333470 | 345 / 291 |
| + map + source (run 3 leg B) | 48,925 | 24,702 | 5,088,880 | 49.661465 | 58.337143 | 346 / 292 |

- `masked_equal` is 24,223 on all three.
- `total_functions` is 69,071 → 69,011 (−60 fragments).
- `total_code` is 10,247,080 → 10,247,140 (+60 B of gap instructions now inside a function).
- Run 3's in-run Δ is **+3 / +636 B**, and DeJitter is the unit that reaches 100. Run 3 leg B equals the
  tip's in-tree build on every key.
- The main → tip step crosses runs. It is trustworthy here because the row diff below finds **zero row
  changes outside the edited addresses** across all ~69,000 rows.

### 5.3 Rows (main → tip, keyed by retail address, both rulers)

Script: `~/tmp/w16jc/rowdiff.py` and `rowdiff_mpn.py`. Output: `~/tmp/w16ke-ab/rowdiff_{fuzzy,mpn}.txt`.

- **34 rows up, all 34 now at 100**, on both rulers. They are exactly the 34 heads.
- **8 rows with a nonzero score vanished** (9 on `mpn`, which adds the 2.5 `KeyGreaterEq<Color>` tail
  row). All are absorbed fragments; the other absorbed fragments read 0 and vanish without showing. Six of them read 100: ContentAltDirs 8 B, AsBoolKeys 8 B, `_Destroy_Range` ×2,
  `_Destroy_Moved_Range` and ReleaseSwing at 4 B each, 32 B in all. Those 100s were a `blr` or
  `li r3,0; blr` fragment pairing with an empty function of the same name. They leave the denominator
  because no retail function starts at their address.
- **No surviving row went down. No unit fell off 100.**
- A script checked every UP and DOWN row against the edit set: each is a grown head or an absorbed
  fragment, with **0 unexplained**.
- **Units reaching 100 (both rulers):** AssetTypes, BeatMatcher, GemSmasher, CrowdRating,
  PracticeSectionProvider, AppMiniLeaderboardDisplay, ClosetMgr and DeJitter.

**Byte reconciliation, done after the run (not a prediction).** The new sizes of the 34 heads sum to
4,956 B; every head was below 100 on main, so none of that was already counted. Subtracting the 32 B of
vanished 100-rows gives **4,924 B, the measured Δ exactly**. Functions: +34 − 6 = **+28**, also exact.

## 6. Gates (tip `2953bf72c`, after its full build)

```
[map-injectivity] OK: 32767 applied rows, 32766 distinct names, injective (+1 enumerated internal-linkage exception(s))
VALIDATE: PASS -- 1609 map-consistent, 268 tolerated (enumerated above), 0 contradicted, 1878 total
[patch-state] OK: 1249 decomp, 3123 target objects match 2026-10-01T12:23:31Z (tree_sha256=fb6da946f44491d3)
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

The native gate ran again after the docs/tool commit (§9).

## 7. Left, with what is known

- **The rest of the scan.** At the tip, 285 heads still reach past their carve (329 on main; some
  of the 44 difference were fragments the fixed heads absorbed). 32 are named, and 30 of
  those are below 100. Most are XDK (D3DX shader compiler, xgraphics, XAudio2, CRT `memcpy`/`atan`/
  `setjmp`), with no base object. The named game/engine ones whose compiled size does **not** equal the
  reached extent were left alone, because the corroboration is missing:
  - `list<CharBones::Bone>` ctor 0x822AFD68;
  - `Normalize(Vector3)` 0x822C1280;
  - `StlNodeAlloc<SmasherPlateInfo>` ctor 0x82356300;
  - `_outline_SetFrustum` 0x82433B38 (0x54 → 0x1E0);
  - `__uninitialized_fill_n<WeightedEntry>` 0x8270F4F8;
  - `Latin1ToUtf8` 0x82AE5FA8.
  The other ~250 heads are anonymous; carving them pays only once they are identified.
- **`Keys<Quat>::FindBounds` / `Remove`** (99.90 / 99.55). Retail calls `0x82422C38`, now named
  `KeyGreaterEq<Color>`, and ours calls `KeyGreaterEq<Quat>`. The two are byte-identical 20-B-key bodies,
  so this looks like a genuine ICF fold. It needs an alias with retail-byte proof, which was not added
  here.
- **Alias group 117's withdrawal note** (`Remove@Keys<Vector3>`) quotes the old name `KeyLessEq<Color>`
  for `0x82422C38`. The withdrawal itself still holds, since the stride differs from `Keys<Vector3>`; only
  the quoted name is stale.
- **`0x825459E0`** (`lbz r3,0x6c(r3); blr`, next to `SetSecondPedalHiHat`) is pinned to CharServoBone.
  It looks like ProfileMgr's getter and was not touched.
- **W16-JD's `GameGem::operator=` and `__median<float>`**, and **W16-JA's Matrix4 product**: not located
  (§1).

## 8. Tooling notes

- **A hand edit to `symbols.txt` survives the split** when the edit is a real function extent: 34 of 34
  held, with no re-carve. The split only added `jumptable_*` names.
- **A `splits.txt` block merge alone is not a fixed point** when it puts a head and its tail in one TU.
  jeff then merges the tail itself, and the split guard fails. Commit the merge and the symbols together.
- **Re-homing a tail block needs the same-TU blocks on either side merged too.** Otherwise dtk fails
  with "split ends within symbol".

## 9. Not done

- Not merged to main.
- No ICF aliases were added.
- No XDK carves.
- The permuter was not run.
- `scripts/symbol_aliases.json` was not touched.
