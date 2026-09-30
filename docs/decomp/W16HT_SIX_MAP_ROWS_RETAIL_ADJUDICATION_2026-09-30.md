# W16-HT — six map rows settled on retail bytes (2026-09-30)

Branch `w16-ht`, worktree `~/tmp/wt-w16-ht`, off main `81542f43e`. The method follows
W16-HJ (`W16HJ_SEVEN_MAP_ROWS_RETAIL_ADJUDICATION_2026-09-30.md`). Every verdict below
comes from retail bytes: RTTI vtable slots, `__RTDynamicCast` TypeDescriptors (TD),
callee TDs, and `bl` callers. The tools were `tools/retail_rtti.py`,
`tools/retail_body.py` and `tools/retail_callers.py`. **None** of the evidence is read
from the map or the scorer.

The branch has **no source edits**. Every source defect these rows expose lives under
`src/system/bandobj` or `src/system/char`, which were out of scope. They are listed in §7.

## Predictions (written before the build)

| VA | before | predicted | Δfns / ΔB |
|---|---|---|---|
| `0x822e40b0` → `ObjPtr<OverdriveMeter>::Replace` | 100 | 0 | −1 / −120 |
| `0x82304000` → `ObjPtr<GemTrackDir>::Replace` | 100 | 0 | −1 / −120 |
| `0x822b0de0` ← `ObjPtr<EventTrigger>::Replace` | 0 | 100 | +1 / +120 |
| `0x822bbc70` ← `ObjPtr<RndGroup>::Replace` | 0 | 100 | +1 / +120 |
| `0x82334528` → `??_GOutfitPiece@BandCharDesc` | 100 | 100 | 0 |
| `0x822b10a0` ← `??_G ObjPtr<ObjectDir>` | 60.8 | 100 | +1 / +76 |
| `0x82390368` → `CharClipGroup::AddClip` | 35.1 | low | 0 |
| `0x822a79a8` → `>>(ObjVector<ObjPtr<RndDir>>)` | 99.8 | 100 | +1 / +100 |
| `0x82390180` ← `>>(ObjVector<ObjOwnerPtr<CharClip>>)` | 0 | 100 | +1 / +100 |
| `0x824cc978` / `0x824cc9d8` (swap) | 99.58 / 99.58 | 100 / 100 | +2 / +192 |

**Predicted: +5 fns / +468 B.** Call-site effects were not modelled.

## 1. `0x822e40b0`: was `ObjPtr<EventTrigger>::Replace`, is `ObjPtr<OverdriveMeter>::Replace`

- The body is the standard `ObjPtr<T>::Replace`. The only per-T datum is the r6
  TypeDescriptor passed to `__RTDynamicCast`: `0x82C6CD2C` = `.?AVOverdriveMeter@@`
  (r5 = `.?AVObject@Hmx@@`).
- It is **slot 2 of the `ObjPtr<OverdriveMeter,ObjectDir>` vtable** (`0x8202542C`). Slot 0 is
  `0x822e4600`, already mapped `??_G?$ObjPtr@VOverdriveMeter@@`.
- The real `ObjPtr<EventTrigger>::Replace` is slot 2 of `0x8201BA34`, at **`0x822b0de0`**
  (cast TD `.?AVEventTrigger@@`). It was anonymous and is pinned in BandCamShot.cpp, whose
  obj defines the spelling. It now reads 100.
- Why the old name read 100: the TD relocation's target is an anonymous data placeholder,
  which `name_check` forgives. So a wrong-T Replace pairs perfectly.
- Home: the address sits inside a GemTrackDir.cpp block, and so does `0x822e4600`. So retail's
  GemTrackDir TU emitted the `ObjPtr<OverdriveMeter>` family. Our `GemTrackDir.obj` defines
  neither spelling; only BandTrack.obj and VocalTrackDir.obj do. So the row now reads 0, as
  `0x822e4600` already did. I did not re-home it to BandTrack.cpp: retail placed it in
  GemTrackDir, and the defect is in our GemTrackDir source (§7).

## 2. `0x82304000`: was `ObjPtr<RndGroup>::Replace`, is `ObjPtr<GemTrackDir>::Replace`

- The evidence has the same shape. The cast TD is `0x82C6CF18` = `.?AVGemTrackDir@@`, and the
  address is slot 2 of the `ObjPtr<GemTrackDir,ObjectDir>` vtable (`0x8202C1E4`), whose
  slot 0 is `0x82304330`.
- The real `ObjPtr<RndGroup>::Replace` is **`0x822bbc70`**: slot 2 of `0x8201DCD4`, with cast
  TD `.?AVRndGroup@@`. It is pinned in Memory_Xbox.cpp, whose obj defines the spelling. It now
  reads 100.
- The row is pinned in TrackPanelDir.cpp. Our `TrackPanelDirBase::mGemTracks` is deliberately
  `ObjVector<ObjPtr<RndDir>>`, so TrackPanelDir.obj never instantiates `ObjPtr<GemTrackDir>`.
  The row reads 0 (as does `0x82304330`). Retail proves the member is `ObjPtr<GemTrackDir>`;
  that is out of scope here (§7).

## 3. `0x82334528`: was `??_G?$ObjPtr@VObjectDir@@`, is `??_GOutfitPiece@BandCharDesc@@UAAPAXI@Z` (ICF-arbitrary)

- The body is `bl 0x827a28d0; if (flags & 1) delete this`. The callee `0x827a28d0` is
  `this->vfptr = 0x82036604`, and RTTI resolves that to `.?AVFixedSizeSaveable@@`. So the
  callee is `~FixedSizeSaveable`.
- Retail RTTI puts `0x82334528` in **slot 0 of four vtables**: `FixedSizeSaveable`
  (`0x82036604`), `BandCharDesc::OutfitPiece` (`0x82036614`), `BandCharDesc::Head`
  (`0x820367EC`) and `SongStatus` (`0x820B31FC`). All four are FixedSizeSaveable-only
  hierarchies with trivial destructors, so it is a 4-way fold.
- It is not `ObjPtr<ObjectDir>`: that class's vtable (`.?AV?$ObjPtr@VObjectDir@@V1@@@`,
  `0x8201B9E8`) has slot 0 = **`0x822b10a0`**, whose body calls
  `~ObjRefConcrete<ObjectDir,ObjectDir>`. That spelling moved there. It is pinned in
  BandCamShot.cpp and goes 60.8 → 100.
- Spelling pick: the body sits in BandCharDesc's TU, just before OutfitPiece's slots 1–2
  (`0x82334578`, `0x823345c8`). So I picked the OutfitPiece spelling. BandCharDesc.obj
  defines it, and it is not mapped elsewhere. `??_GFixedSizeSaveable` is already taken at
  `0x825ea8e8` (see §7). The VA was added to `_icf_arbitrary`. It reads 100.

## 4. `0x82390368`: was `HAQManager::PrintComponentInfo`, is `CharClipGroup::AddClip` (re-homed)

- The body loads `this+8` (end) and `this+4` (begin), then calls `0x8238da40` (`find`). If
  `find` returned end, it builds `ObjOwnerPtr<CharClip>(this, clip)` (`0x8238dd38`) and calls
  `ObjVector<ObjOwnerPtr<CharClip>>::push_back` (`0x82390290`). That is `AddClip` with
  `HasClip` inlined. Retail's out-of-line `HasClip` (`0x8238e3c8`) calls the same `find` on
  the same `this+4/+8` pair.
- There is **one caller**, at `0x82390858` inside `0x82390578` (CharClipGroup's 1,056 B
  handler block). It passes `__RTDynamicCast(DataNode::GetObj(..), .?AVCharClip@@)`.
- The whole span `0x8238DB68–0x82390B38` is CharClipGroup's retail TU. I moved `.text
  0x82390368–0x8239040C` from HAQManager.cpp to CharClipGroup.cpp; HAQManager keeps its
  `0x82BB1260` block. The row reads **22.77**. Our `HasClip` is DC3's/rb3's index loop,
  while retail's is `find` (§7). The row's EH funclet `fn_823903E4` moved with it and reads 100.
- The `PrintComponentInfo` name now maps nowhere. Its real body was not identified; it may be
  inlined into its sole caller.

## 5. `0x822a79a8`: was `>>(ObjVector<ObjOwnerPtr<CharClip>>)`, is `>>(ObjVector<ObjPtr<RndDir>>)` (re-homed)

- The body reads a count, calls resize (`0x822a6db8`), then loops at stride 0xc calling
  `0x8229ecb8(bs, true, 0)`. That callee's own cast TD is `0x82C6B5B4` =
  `.?AVRndDir@@`, i.e. `ObjRefConcrete<RndDir>::Load`.
- Both callers (`0x822acddc`, `0x822ace28`) are in `OutfitConfig::Load`. They read into a
  temporary vector, and our source spells that vector `ObjVector<ObjPtr<RndDir> > dirs(this)`,
  which agrees with retail.
- Home: the address sat in a HamCamTransform.cpp block (a DC3 TU) between Gem.cpp blocks.
  Gem.cpp scatter-includes OutfitConfig.cpp and Gem.obj defines the spelling, so `.text
  0x822A79A4–0x822A7A10` moved to Gem.cpp. It reads 100.
- The real CharClip reader is **`0x82390180`**: a count read, then
  `ObjVector<ObjOwnerPtr<CharClip>>::resize` (`0x82390110`), then per-element loads. It is
  pinned in CharEyes.cpp, whose obj defines the spelling, and goes 0 → 100.
- `symbol_aliases.json`'s group at `0x822a79a8` (`folded: []`) was relabelled to the RndDir
  survivor.
- Also, W16-HQ's prose calls `0x823CA8F8` "the `ObjVector<ObjPtr<RndDir>>` `>>`". That address
  is the `RndTransformable` instantiation (`resize@ObjVector<ObjPtr<RndTransformable>>` +
  `ObjRefConcrete<RndTransformable>::Load`), and the map already spells it that way.

## 6. `0x824cc978` / `0x824cc9d8`: the `WorldDir` reader names were swapped

- `0x824cc978` loads `obj+0` through `0x824cb4c8` (cast TD `.?AVLightPreset@@`) and `obj+0xc`
  through `0x824cb5b0` (`.?AVLightHue@@`). It is `operator>>(BinStream&,
  WorldDir::PresetOverride&)`.
- `0x824cc9d8` loads through `0x822c8f50` (`.?AVRndMesh@@`) and `0x8229e728` (`.?AVRndMat@@`).
  It is the `MatOverride` reader.
- Our `world/Dir.cpp` readers are `bs >> o.preset >> o.hue` and `bs >> o.mesh >> o.mat`. The
  source was right and the map was swapped. The home (MidiSynth.cpp, which holds WorldDir's
  `Poll` at 100) was left alone. Both rows go 99.58 → 100.

## 7. Deliberately not done (follow-ups)

- **Source, `src/system/bandobj/GemTrackDir.cpp`**: retail's GemTrackDir TU instantiates
  `ObjPtr<OverdriveMeter>` (`??_G` `0x822e4600` and `Replace` `0x822e40b0`, both 0 now).
  Which GemTrackDir function constructs it is unidentified.
- **Source, `TrackPanelDirBase::mGemTracks`**: retail is `ObjPtr<GemTrackDir>`
  (`0x82304000`, `0x82304330`); ours is `ObjVector<ObjPtr<RndDir>>` by documented choice.
- **Source, `src/system/char/CharClipGroup.cpp` `HasClip`**: retail calls `find` (DC3 spells it
  `mClips.end() != mClips.find(clip)`). Ours is an index loop, so AddClip is at 22.77 and
  `HasClip` (`0x8238e3c8`) at 0.
- **Map, `0x825ea8e8`** (`??_GFixedSizeSaveable`, 100, pinned FixedSizeSaveable.cpp) is really
  `AccomplishmentGroup`'s `??_G`. It is slot 0 of AccomplishmentGroup's vtable, and it stores
  vtable `0x820B925C` = `.?AVAccomplishmentGroup@@`. It reads 100 only because that vtable
  relocation is a forgiven placeholder. It was not touched, to keep scope.
- **Name, `0x827a28d0`** = `~FixedSizeSaveable` (16 B, anonymous, pinned Achievements.cpp).
  It was left anonymous because callers are forgiven.
- **Pins**: CharEyes.cpp `0x8238FB88–0x82390368` is CharClipGroup template code, and
  HamCamTransform `0x822A7A10–0x822A7D10` (`>>` Overlay etc.) looks like OutfitConfig code.
  Neither was moved.

## 8. Measurement

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-ht --patch <git diff main w16-ht -- src config scripts>`.
The worktree was detached at main `81542f43e` for the run and restored afterwards. The patch
is the whole branch diff, with kinds `map, splits`. Both legs were force-re-split and read at a
`symbols.txt` fixed point. Ruler `name_check`; objdiff-cli sha256 `c1b7d95240a35cd6` on both
legs. Run dir: `.ab_measure_runs/20260930-180943-w16-ht-branch-vs-main-834571/`.

```
leg A: matched=45469 masked=23556 honest=21913 code%=43.926846
leg B: matched=45475 masked=23557 honest=21918 code%=43.931805
Δmatched=+6  Δmasked_equal=+1  Δhonest=+5  Δcode%=+0.004959pp  Δcode_bytes=+508
Δfuzzy=+0.001110pp
units: +2 BandCamShot, +2 MidiSynth, +1 CharClipGroup, +1 CharEyes, +1 Memory_Xbox, +1 Gem,
       -1 GemTrackDir, -1 TrackPanelDir
units at 100%: 219 -> 219 (mpn), 190 -> 190 (all-rows-fuzzy)
`none` control: +216 B (NOT_APPLICABLE: map+splits)
```

**Prediction vs measured**: predicted +5 / +468 B, measured **+6 / +508 B**. Every predicted row
moved as predicted. Two movers were unpredicted:
- `fn_823903E4` (AddClip's EH funclet) moved with the re-home and went 99.9 → 100: +40 B, +1.
- `OutfitConfig::Load` went 99.88 → 99.91: its `bl 0x822a79a8` is no longer charged.

The two −1 units are the correction cost of §1–2 (right names that our objs cannot define yet).

## 9. Gates

The gates ran on the branch after a full `./tools/ninja-locked`. That build needed one
split-guard retry for the re-derived `.pdata`, the same shape W16-HJ recorded.

- `tools/map_name_injectivity.py`: `OK: 30337 applied rows, 30336 distinct names, injective`.
- `tools/icf_alias_finder.py --validate`: `VALIDATE: PASS -- 1475 map-consistent, 264
  tolerated, 0 contradicted, 1740 total`, rc=0.
- `tools/native_build_gate.sh`: run last; the result line is in the lane report.
