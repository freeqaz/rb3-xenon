# W16-RC: dtk mis-carves in in-scope units (2026-10-06)

Lane W16-RC. Brief: repair the known dtk mis-carves (`SemitoneToWhiteKey`,
W16-QZ; the six `Message` atexit stubs, W16-QI §4.2), then look for more of the
same shapes in in-scope units, checking each against retail bytes and `.pdata`.
Worktree `~/tmp/wt-w16rc`, branch `w16-rc`, based on main `1b62177bd`.
Target image: clean TU5 (`band.exe` sha1 `5f3f667a…`, `default.xex` `d56e7f31…`).

## 1. Result

| commit | change | measured by | Δ`matched_functions` | Δ`matched_code` |
|---|---|---|---:|---:|
| `c016075d0` | 18 heads re-carved, 24 fragments deleted, 4 splits boundaries moved, 4 false map names removed | in-tree, hand-run (§6.2; `ab_measure` refuses symbols.txt) | **+1** | **+228 B** |
| `0d8008523` | Achievements_Xbox span re-homed; 3 carve-freed rows named | `ab_measure --revert` (§6.1) | **+3** | **+72 B** |
| **lane** | | | **+4** | **+300 B** |

Predictions were stated before each measurement: +1 / +228 B for the carve
commit (made up of +5 fns / +256 B for the rows reaching 100 and −4 fns / −28 B
for the false matches it removes), and +3 / +72 B for the naming commit. Both
reproduced exactly in-tree.

The headline is small because most repaired heads are anonymous (`fn_…`) and
stay at 0% whether carved right or wrong. The carve commit's value is accuracy:
24 phantom rows leave the denominator, 4 byte-coincidence matches are withdrawn,
and every repaired function is now one row at its retail extent, so it can pair
once it is named.

## 2. Method

Three independent instruments, all reading retail `band.exe` directly:

- **Extent scan** (`tools/carve_extent_scan.py`, W16-KE): walks each function's
  control flow, including byte/half jump tables behind `bctr`, and reports a
  head whose reached extent exceeds its carve. `--base` adds our compiled size
  for named heads.
- **Reference finder** (scratch `~/tmp/w16rc/refs.py`): for an address, every
  `.pdata` BeginAddress, every `b`/`bl`/`bc` in `.text`, every `lis`+`addi`/`ori`
  pair within 8 instructions, and every 4-byte word in every non-`.text` section
  that equals it. Control: every true head has external callers (`bl`), an
  `atexit` registration (`lis/addi`) or vtable words; every fragment has none,
  or only a reference from inside its own parent (a `bc`, or the jump table's
  case-base `lis/addi`).
- **Hole screen** (scratch `~/tmp/w16rc/holes.py`): non-zero `.text` words in
  in-scope split blocks that no symbols.txt symbol covers. Control: on main
  (`1b62177bd`) it finds 48 words in 12 units, including every hole this lane
  closes; after the lane, 33 words in 5 units, all accounted for in §5.

A fragment is deleted only when it is not a `.pdata` BeginAddress and has no
reference from outside its parent. `.pdata` is read with `tools/pdata_extent.py`
(the corrected `>>8` decode); none of the 18 heads has its own `.pdata` entry
(all are frameless leaves or thunks), so none was ever bounded by one.

## 3. The repairs (`c016075d0`)

| head | unit | carve before → after | fragments deleted | shape |
|---|---|---|---|---|
| `0x822E35D0` `SemitoneToWhiteKey` | GemTrackDir | 0x60 + 0x30 → **0x90** | `822E3630` | byte jump table: the case-body base (`lis/addi r12,0x822E3630`) taken for a function start |
| `0x82272EE8` (`AllocType`) | Memory_Xbox | 0x44 + 0x338 + 0x5C → **0x3D8** | `82272F2C`, `82273264` | byte jump table (`lbzx; slwi 2; bctr`) plus its `bgt` default arm |
| `0x822734E0` (`AllocAlign`) | Memory_Xbox | 0x3C + 0x64 + 0x8 + 0x28 → **0xD0** | `8227351C`, `82273580`, `82273588` | byte jump table plus two branch targets |
| `0x82C460A8` | BandSongMgr | 0xC + hole + 0x10 → **0x1C** | `82C460B8` | `Message` atexit stub (W16-QI §4.2) |
| `0x82C46568` | AppLabel | 0xC + 0xC → **0x1C** | `82C46578` | same |
| `0x82C46DC8` | CampaignSongInfoPanel | 0xC + 0xC → **0x1C** | `82C46DD8` | same |
| `0x82C47948` | MainHubPanel | 0x14 + 0x4 → **0x1C** | `82C47960` | same |
| `0x82C46DA8` | CampaignSongInfoPanel | 0x20 → **0x1C** | — | same stub, size ran over its trailing pad word (cosmetic, §5.2) |
| `0x82C49500` | TrackerDisplay | 0x20 → **0x1C** | — | same |
| `0x823199C8` | MiniLeaderboardDisplay | 0x8 + hole + 0x78 → **0x88** | `823199D8` | vbase-adjust thunk ending in a tail call, cut by fall-through |
| `0x823474F8` | BandPatchMesh | 0x30 + 0x8 → **0x38** | `82347528` | byte-copy leaf; its `beq` exit tail carved |
| `0x82376D30` | CharServoBone | 0x8 + 0x4 → **0xC** | `82376D38` | `mr; li; b` tail-call leaf |
| `0x823EA988` | WavMgr | 0x1C + 0x14 + 0x28 → **0x58** | `823EA9A4`, `823EA9B8` | tree-search leaf; loop targets carved |
| `0x823F3F20` | WavMgr | 0x18 + 0x4 → **0x1C** | `823F3F38` | leaf; `bne` target (the tail `b`) carved |
| `0x8249B200` | EventTrigger (was VorbisMem) | 0x4 + hole + 0xC → **0x2C** | `8249B220` | virtual (`EventTrigger` vtable slot 1); 0x2C includes the dead `blr` after its `bctr` |
| `0x8278CD40` | GameGemList | 0x5C + 4 + 0xC + 0xC + 0x10 + 0x8 → **0x98** | `8278CDA0`, `8278CDA4`, `8278CDB0`, `8278CDC0`, `8278CDD0` | float median-of-three leaf; branch targets carved |
| `0x827B8690` (`SubmitAchievementsFunc`) | Achievements_Xbox (was StoreEnumeration) | 0xC + 0x14 → **0x20** | `827B869C` | tail-call leaf cut by fall-through |
| `0x827D2858` (`BeatInfoCmp`) | BeatMap (was Profiler) | 0x10 + 0xC → **0x1C** | `827D2868` | leaf cut by fall-through |

After the first forced re-split dtk recognised the three byte tables as jump
tables (`lbl_8200E610`, `lbl_8200E630`, `lbl_820253C8` became `jumptable_*`) and
re-created none of the deleted fragments. The second split reproduced
symbols.txt and splits.txt byte-for-byte (fixed point); so did the split after
the naming commit.

### 3.1 Four splits.txt boundaries sat inside a true function

Each was drawn at a fragment, so the head could not grow without moving it.
Ownership was settled on retail references, not on the old pins:

- **`0x823474F8`**: all four callers (`0x82347AB4`, `…7B3C`, `…8EB0`, `…8F20`) are
  in BandPatchMesh. File.cpp's 8 B block `0x82347528..30` held only the
  `NullFile::Write` fragment; it goes to BandPatchMesh.
- **`0x8249B200`**: retail RTTI (`tools/retail_rtti.py`) puts it at slot 1 of
  `.?AVEventTrigger@@`'s vtable (`0x82074874`) and of `.?AVUITrigger@@`'s
  (`0x8212A6A0`, inherited). VorbisMem's 4 B block on it is a mis-pin; it goes to
  EventTrigger, whose blocks surround it.
- **`0x827D2858`**: loaded as a function pointer only at `0x827D2AA4` and
  `0x827D2BC4`, both in BeatMap. Our `src/system/utl/BeatMap.cpp:15` is
  `bool BeatInfoCmp(const BeatInfo &info, int tick) { return info.mTick < tick; }`,
  passed to `std::lower_bound` at lines 76 and 98. Profiler's block
  `0x827D2854..68` goes to BeatMap.
- **`0x827B8690`**: passed to `ThreadCall` from `Achievements::Poll`
  (`0x827A27B8`) alongside `SubmitAchievementsCallback`, and reads the global at
  `0x82E064F4` that Achievements code reads. The carve commit first gave it an
  `Achievements.cpp` block; the next commit corrects that (§4).

### 3.2 Four map names on fragments withdrawn

Each fragment's bytes happened to equal a trivial inline that the unit's base
object defines, so the row read **100** while no function starts at the
address (no `.pdata`, no data word, no external branch):

| address | name removed | bytes | unit |
|---|---|---|---|
| `0x82273580` | `?CamOverride@RndDrawable@@UAAPAVRndCam@@XZ` | `li r3,0; blr` (a case arm of `AllocAlign`) | Memory_Xbox |
| `0x82347528` | `?Write@NullFile@@UAAHPBXH@Z` | `mr r3,r5; blr` (exit tail of the copy loop) | File |
| `0x8278CDA0` | `?_Destroy_Range@stlpmtx_std@@YAXPAD0@Z` | `blr` | GameGemList |
| `0x8278CDD0` | `??2GameGem@@SAPAXIPAX@Z` | `mr r3,r4; blr` | GameGemList |

None appears in `scripts/symbol_aliases.json`. These are the −4 fns / −28 B in
the carve commit's delta.

## 4. Naming (`0d8008523`)

Our `Achievements_Xbox.obj` defines `?SubmitAchievementsFunc@Achievements@@CAHXZ`
as 32 B with retail `0x827B8690`'s instruction sequence (only relocated fields
differ), and `?GetAchievementData@Achievements@@AAA?AUXUSER_ACHIEVEMENT@@HH@Z`
(`stw r6,4(r3); stw r5,0(r3); blr`), byte-identical to retail `0x827B8680`,
which `Achievements::Submit` calls at `0x827A28A0`. So retail's
Achievements_Xbox TU is `0x827B8680..0x827B86B0`. It gets a path-qualified
heading (`system/meta/Achievements_Xbox.cpp`, already declared in objects.json
with no pin), and StoreEnumeration ends at `0x827B8680`.
`?BeatInfoCmp@@YA_NABUBeatInfo@@H@Z` is byte-identical to retail `0x827D2858`.
All three read fuzzy 100.

## 5. Recorded, not changed

### 5.1 In-scope scan hits left alone

| head | unit | what the scan sees | why not changed |
|---|---|---|---|
| `0x822C74E0` `BandFaceDeform::TotalSize` | BandFaceDeform | carve 0x34, code to 0x44 | the carve stops at a stray `type:label` (`lbl_822B5C68`); objdiff extends the row to the next symbol and it reads **100** at 68 B |
| `0x8243F394` | rndobj/Utl | 0x2C, code to 0x4C | carve stops after `bl 0x82829A08`; row reads 100 |
| `0x82735690` `DxTex::PreDeviceReset` | rnddx9/ShaderMgr | 0x10, code to 0x20 | stray `type:label` at `0x827356A4`; row reads 100 |
| `0x82772310` | SongData | 0x28, code to 0x68 | stops after `bl 0x82829A08`; row reads 100 |
| `0x82713740` | MidiInstrument | 0x24, 0x28 with the dead `blr` after its tail call | a different shape (W16-KE's dead epilogue); anonymous, so no base size to corroborate it |

The four rows at 100 are cosmetic: the remaining words sit in no symbol (they
are the hole screen's remaining 32 words) but are already inside each row's
inferred extent.

### 5.2 Carves that run over a trailing pad word: benign, 336 in scope

A screen for symbols whose size ends in zero word(s) after a terminator
(scratch `padscan.py`) finds **336** in in-scope units. 320 of the 328 named
ones read fuzzy 100, and the report's row size already excludes the pad
(`BandRetargetVignette::Enter`: `size:0x8` in symbols.txt, a 4 B row); the 8
below 100 have ordinary body differences. The two `Message` stubs the brief
named (`0x82C46DA8`, `0x82C49500`) were trimmed to 0x1C anyway (Δ0, both rows
were already 28 B at 100). The other 334 were not touched.

### 5.3 Outside scope

After the lane the extent scan reports 127 hits: 6 in named in-scope units (the
five in §5.1 plus `aes` `0x82BBA020`, vendor crypto), 17 in `xdk/` units
(`isalpha`, `isxdigit`, `_decomp`, D3DX/xgraphics/xaudio2/xhv2), and 104 in
unattributed `auto_*` units: 58 below `0x82A00000`, 45 above it, 1 in the
Quazal `/Od` block. Seven `auto_*` hits are byte jump tables, the
SemitoneToWhiteKey shape (`0x828B2360`, `0x828B2378`, `0x8295A680`, and four at
`0x82A55800..0x82A55A60`). Fixing an `auto_*` carve moves no metric (the rows
are unpairable) and needs an owner first, so none was changed.

### 5.4 Source follow-ups

- `AllocType` (`0x82272EE8`, 984 B) and `AllocAlign` (`0x822734E0`, 208 B)
  are now one row each, but our anonymous-namespace bodies in
  `src/Memory_Xbox.cpp` compile to 1,164 B and 224 B, so they were not named.
  `AllocType`'s `switch` in particular does not match retail's 27-entry byte
  table.
- `0x827D283C..40` is a padding-only Profiler block between two BeatMap blocks;
  harmless, left.

## 6. Measurement

### 6.1 `ab_measure` on the naming commit

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16rc --revert 0d8008523`
(`name_check` ruler; leg A carries the commit, leg B removes it). Run dir
`~/tmp/wt-w16rc/.ab_measure_runs/20261006-192245-w16rc-names-2572720/`.

```
leg A: matched=54700 masked=25213 honest=29487 code%=58.736020  (recompiles: 0, settled)
leg B: matched=54697 masked=25213 honest=29484 code%=58.735320  (recompiles: 0, split=1, patch_steps=7, settle iterations: 2)
split fixed point: leg A after 0 extra re-splits, leg B after 0
Δmatched=-3  Δmasked_equal=+0  Δhonest=-3  Δcode%=-0.000700pp  Δcode_bytes=-72
Δfuzzy=-0.000694pp   (legA 64.174110 -> legB 64.173416)
units at 100% [mpn]: 590 -> 588 (BeatMap 12/12 -> 11/12; Achievements_Xbox 2/2 -> absent)
```

So the commit is worth **+3 fns / +3 honest / +72 B / +0.000700 pp**, +2 units at
100% (BeatMap completes; Achievements_Xbox is a new unit at 2/2), as predicted.
The `none` control moved by the same −72 B; the tool marks it not applicable for
a splits+map patch.

### 6.2 The carve commit, hand-run

`ab_measure` refuses this commit: `--revert c016075d0` was run and stopped at
`classify` ("patch touches config/45410914/symbols.txt"), before any build. The
brief assumed `--revert` would accept it; it does not, because `--revert`
builds its patch with `git diff REF REF^`, which includes symbols.txt, and every
mode goes through the same classifier. W16-MC and W16-NF hit the same wall and
put their carve hunks in leg A's base, recording the carve value in-tree. This
lane does the same, with the protocol's own safeguards applied by hand:

1. Check out the leg's `symbols.txt`, `splits.txt` and `target_symbol_map.json`
   in the worktree (main `1b62177bd`, or `c016075d0`).
2. Remove `build/45410914/target_symbol_renames.stamp`, touch `config.yml`.
3. Build with `tools/ninja-locked` until a build has no SPLIT/MSVC/PATCH lines
   **and** symbols.txt is unchanged by it (fixed point).
4. Delete `report.json` and `report.cache`, build `report.json`, read by key.

Script: `~/tmp/w16rc/handleg.sh`. **Control:** the procedure run on
`c016075d0`'s files must reproduce `ab_measure`'s leg B above, which is that
same state measured by the tool. It does, on every key: matched 54,697,
masked 25,213, code% 58.73532, fuzzy 64.173416. Run on HEAD it reproduces leg A
(54,700 / 58.73602 / 64.17411).

| leg | matched | masked_equal | total_functions | matched_code | total_code | code% | fuzzy |
|---|---:|---:|---:|---:|---:|---:|---:|
| main `1b62177bd` | 54,696 | 25,213 | 68,908 | 6,018,876 | 10,247,792 | 58.73339 | 64.1721 |
| `c016075d0` | 54,697 | 25,213 | 68,884 | 6,019,104 | 10,247,844 | 58.73532 | 64.173416 |
| Δ | **+1** | 0 | −24 | **+228** | +52 | +0.00193 pp | +0.001316 pp |

Both legs reached the fixed point after one forced split (main also reproduced
this worktree's first, unforced build exactly). Row diff of the two reports, all
`(unit, symbol)` keys: 27 rows gone, 3 new, 13 changed, nothing else.

- 5 rows reach 100: `SemitoneToWhiteKey` (96 B @ 50 → 144 B @ 100) and the four
  split `Message` stubs (12–20 B @ 0/60 → 28 B @ 100): +5 / +256 B.
- 4 rows leave at 100: the withdrawn names of §3.2: −4 / −28 B. These are the
  only rows that lose `mpn` 100.
- The other 23 gone rows are the anonymous fragments; 3 "new" rows are heads
  that changed unit (`fn_827D2858` Profiler → BeatMap, `fn_8249B200`
  VorbisMem → EventTrigger, `fn_827B8690` StoreEnumeration → Achievements); 8
  anonymous heads grew to their true size. All at 0 before and after.

## 7. Native gate

`tools/native_build_gate.sh` in the worktree at `0d8008523` (the lane touches no
source, so this is a confirmation, not a risk check):

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

## 8. Not done

- No source edits. `AllocType`/`AllocAlign` bodies (§5.4) not written.
- No `auto_*` or XDK carve changed (§5.3); no pad-only size trimmed beyond the
  two stubs (§5.2).
- No merge, no push (coordinator).

Scratch: `~/tmp/w16rc/` (`rdis.py` word-by-word retail disassembly, `refs.py`,
`holes.py`, `padscan.py`, `fixes.py` + `apply_symbols.py`, scan outputs, A/B logs).
