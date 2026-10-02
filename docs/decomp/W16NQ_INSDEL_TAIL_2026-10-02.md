# W16-NQ — the insert/delete tail W16-NL left, plus keygen `getMasher` (2026-10-02)

**Branch** `w16-nq`, on main `78d7d8f02` (started at `2b066acc8`, moved to `a01ba367e` after W16-NR and to
`78d7d8f02` after W16-NS; the fork branches were rebased onto `78d7d8f02` before merging). Four fork branches
`w16-nq-{RND,CW,SYS,AUD}` are merged into it with `--no-ff`. Ruler `name_check` (graded). Permuter not run.
No `fn_` row in a band3/network unit was edited. **Not merged to main.**

## 1. Population

W16-NL's 315-row STRUCT_INSDEL pool (`~/tmp/w16nl/slice_*.json`), re-read against main `2b066acc8`: 314 rows
still keyed, **282 still below 100 (223,264 B)**. Of those, 141 rows / 72,636 B were not mentioned in any W16-NL
fork `result.json` (detector: `~/tmp/w16nq/opened.py`, name match on `Class::Method`; W16-NL's own estimate was
"about 120"). The 282 were re-sliced by directory into four disjoint file sets (`~/tmp/w16nq/slice_*.json`):
RND (rndobj+rnddx9, 60 rows), CW (char+world+bandobj+track, 81), SYS (os/utl/obj/math/meta/ui/net + the band3/network
rows, 99), AUD (synth/synth_xbox/dsp/beatmatch/midi/movie, 42). Each fork worked never-opened rows first, largest
first, then W16-NL's opened-not-closed rows only where a new construct was visible.

## 2. Whole-branch A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16nq-base --patch ~/tmp/w16nq/ab_branch2.patch`
(`git diff 78d7d8f02 w16-nq -- . ':!docs'`, 39 paths, kinds source + map + aliases), worktree at main `78d7d8f02`.
Run dir `~/tmp/wt-w16nq-base/.ab_measure_runs/20261002-175442-ab_branch2-1819732/`; legs archived as
`~/tmp/w16nq/legA_report.json.gz` / `legB_report.json.gz`.

```
leg A: matched=51450 masked=24631 honest=26819 code%=54.769770  (recompiles: 0, settled)
leg B: matched=51490 masked=24640 honest=26850 code%=54.927860  (recompiles: 378, split=1, patch_steps=7, settle iterations: 2)
split fixed point: leg A converged after 0 extra re-split(s), leg B after 0
Δmatched=+40  Δmasked_equal=+9  Δhonest=+31  Δcode%=+0.158090pp  Δcode_bytes=+16200
Δfuzzy=+0.007870pp
unit net (ALL units) = +40   vs whole-binary Δmatched = +40
units at 100% [mpn]: 483 -> 485 (LightPresetManager, system/obj/Utl; 0 fell off)
[control none] Δmatched_code=+16752 B
```

**Prediction, written before the merge:** sum of the four fork row diffs against `2b066acc8` (+40 fns /
+15,920 B), less AUD's `AddChordLevel` (+1 fn, 0 B — W16-NS landed the same fix to the same score, so AUD's
commit was dropped in the rebase), plus `getMasher` (+1 fn, +280 B) = **+40 / +16,200 B**. **Measured: +40 /
+16,200 B exactly**, and identical before and after the alias withdrawal in §5 (that change alone is Δ0).

**Row diff of the legs** (`~/tmp/w16nq/ab_rowdiff.txt`): 51 rows up, **0 rows down**, 38 rows crossed to 100
(15,876 B). Three keys renamed (same addresses): the two `Compress` rows (`_N` → `W4AlphaCompress@2@`) and
`fn_827B5470` → `?OnMsg@StorePanel@@IAA?AVDataNode@@ABVProfileSwappedMsg@@@Z`. Of W16-NL's pool, 27 rows crossed
and 37 moved up; 24 of the crossings were rows W16-NL had not opened.

## 3. What closed, by slice

Per-row before/after, commit and evidence: `~/tmp/w16nq/<FORK>/result.json`. Fork counts are each fork's
full-build row diff against `2b066acc8`.

| fork | Δfns / Δbytes | rows up / down |
|---|---|---|
| RND | +8 / +3,780 | 16 / 0 |
| CW | +8 / +4,112 | 9 / 0 |
| SYS | +12 / +4,128 | 13 / 0 |
| AUD | +12 / +3,900 (−1 fn on rebase) | 15 / 0 |
| coordinator (`getMasher`) | +1 / +280 | 1 / 0 |

Crossed to 100 (by size): `BandCharacter::OnSetFileMerger` 2156, `CharDriver::Poll` 1372, `StorePanel::Handle` 1192
(+ its `OnMsg` 152), `RockCentral::UpdateSetlist` 1268, `RndAnimatable::OnAnimate` 1220, `RndLine::SetNumPoints` 976,
`SongParser::PrepareTrack` 956, `GranularSynth` ctor 916, `RndMesh::CollideShowing` 696, `RndShaderParticles::CalcShaderOpts`
544, `MasterAudio::SetupChannels` 532, `Movie::Impl::SharedFinishOpen` 388, `GameGemList::WillBeNoStrum` 368,
`SystemPoll` 280, `KeyChain::getMasher` 280, `PeakDetector::gaussianWindow` 268, `LoadMgr::PollUntilLoaded` 248,
`ASCIItoUTF8` 232, `DxRnd::Present` 224, `ReplaceObject` 216, `MidiParser::ParseNote` 216, `MergeObject` 192,
`Object::SetNote` 188, `FileMerger::Merger::operator=` 160, `BandCharacter::Compress` 144, `SongData::GetVocalNoteList`
136, `LightPresetManager::SetLighting` 96, `BandWardrobe::SyncVignetteInterest`/`SyncEnableBlinks` 80/76,
`BandCharDesc::Compress` 28, plus seven 40 B EH funclets moved as side effects.

### 3.1 Behaviour bugs found and fixed (each read on retail bytes)

| row | defect |
|---|---|
| `StorePanel::OnMsg(ProfileSwappedMsg)` | Was a `return 0` stub. Retail 0x827b5470 compares `StoreUser()` with `GetUser1`/`GetUser2`, calls `StoreUserProfileSwappedToUser` (vtable +0x6c) with the other user and returns 1. The stub also let MSVC treat the call as nothrow and elide `Handle`'s message-dtor store (the 12-byte size gap). New map row `0x827b5470`. |
| `SystemPoll` | Retail ends with `bl fn_82521ED0`, a stage-kit poll that drains a 32-entry ring into `JoypadStageKitSetRaw`; ours never called it. It is declared as `StageKitPoll()` in the match build only — the body is not in source and its retail name is unknown, so the name is a placeholder (the call site's relocation target is a forgiven `fn_` placeholder). `HX_NATIVE` is unchanged. |
| `FileMerger::Merger::operator=` | Copied `mForceReload` (0x29); retail does not. |

(`BandCharacter::OnSetFileMerger` is a codegen fix, not a behaviour fix: the flag was already cleared just above.
Retail stores `ty == kMic && mGenre != "banger"` unconditionally — a non-mic instrument jumps straight to the store
of the zero register — where ours only stored inside `if (ty == kMic)`. An earlier lane (BF-3) had recorded the
branch form as a wall.)

### 3.2 Levers worth carrying forward

- **`/Od` frame slots are keyed on local NAMES, not declaration order** (`getMasher`; the same mechanism the
  `mash` note in `keygen_xbox.cpp` records). When a `/Od` row's only residue is slot permutation, compile one
  scratch TU holding every candidate name tuple with `/Fa` and read the slots off the listing:
  `~/tmp/w16nq/kg/{gen.py,run.sh,parse.py}` compiled 4,006 tuples in 5 s; 84 reproduce retail's order. The
  instrument was validated first by reproducing our own current layout exactly. The chosen names are
  marked in source as non-original.
- **A retail `clrrwi rX,rY,0` right after a store** means the source reads back the member it just stored
  (`LightPresetManager::SetLighting`).
- **A signed `cmpwi` on a pointer** means an `ObjPtr`'s conversion operator tested in place, not via a local
  (`CharEyes::Poll`).
- **A global that cannot alias lets MSVC hoist vtable loads above its store** — making `gLoadCount` file-local
  closed `LoadMgr::PollUntilLoaded`.
- **An empty or `return 0` stub is a two-defect bug** (body, and the caller's EH frame elided) — `StorePanel::Handle`
  crossed only once `OnMsg` had a real body.
- **Read the branch targets of rows recorded as walls**: `OnSetFileMerger` (written as one conditional `&&` store) and `CharDriver::Poll` had been written
  off; both closed once the target showed which block retail falls into.
- W16-NL's levers held again: loop bound written in the loop condition (`SharedFinishOpen`), implicit conversion
  instead of explicit temporaries (`Movie::Impl::Begin`, `CalculateAO`, `Tessellate`).

## 4. keygen (handed over by W16-NS)

- **`KeyChain::getMasher` 84.83 → 100.** Retail stores the endianness test as a byte (`clrlwi`/`stb`, i.e. a
  `bool`), keeps `0xEB` in its own frame slot that the ternary reloads, and swaps through one byte temp via a
  pointer copied into a local (0x64/0x68) — ours used two temps directly on `masher_p`. After that only slot
  order differed; renamed per §3.2.
- **`random` left at 83.33.** Retail materialises the seed's full address (`lis`/`addi`, then `0(r)`) at every
  access; ours folds `@l` into the access. The TU's other global access (`fn_82725298`, an unpaired 32 B
  increment of `lbl_82E03E1C`) has the same form, so it is a property of the TU, not of one variable. None of
  ~25 spellings (function static, file static, global, array, struct member, volatile, unsigned, extern,
  extern array, incomplete type, `extern "C"`, `*const` pointer, reference, `*&`, cast, `__declspec(align)`
  8/16/64, `#pragma data_seg`, `__declspec(allocate)`, `selectany`, `/TC`) nor 15 `/Od` flag sets (`/Os`,
  `/Ot`, `/Oi-`, `/Gy-`, `/Ob1`, `/Zi`, `/Z7`, `/RTCs`, `/RTC1`, `/GS`, `/Oy`, `/Og`) produce it in a scratch
  compile. (An earlier flag sweep in this lane was vacuous — zsh passed each flag string as one argv element —
  and was re-run under bash; the numbers above are from the bash run.)

## 5. Alias change: one membership withdrawn, none added

`icf_alias_finder.py --validate` went **FATAL** on the merged tip: the `0DataNode` group (0x8228d358) listed
`?OnMsg@StorePanel@@IAA?AVDataNode@@ABVProfileSwappedMsg@@@Z` as folded, while the map now places that name at
0x827b5470. The membership dates from the bulk re-derivation `adc8fcc70` and rested on our old stub compiling
byte-identical to `DataNode::DataNode()`. Retail bytes refute it: the only `bl` into 0x827b5470 is at 0x827b58f0
inside `StorePanel::Handle`, and none of 0x8228d358's 9 retail callers is in StorePanel.
`icf_pair_adjudicate.py --chase --survivor '??0DataNode@@QAA@XZ' --ours <that spelling>`: **FLAT T1 REFUTED,
CHASED T1 REFUTED** (retail 16 B vs ours 152 B, bytes differ). Withdrawn under `withdrawn[]` with that evidence
(commit `be11c7e6d`); nothing pruned. Measured Δ0.

**No alias was added**, so there was nothing new for `--chase` to prove.

## 6. What is left, and why

Reasons per row are in the fork `result.json`s. Summary:

- **Scheduling / register / stack-slot residue with no source construct found**: `PlatformMgr::Poll` (1844),
  `CharIKHand::Poll`, `CharBonesSamples::Relativize`, `CharCollide::Deform`, `NgMat::RefreshState`, `SpliceKeys`,
  `PatchVerts::Add` and `RndFont::Kerning` (ours keeps a value in a volatile register across a same-TU call),
  `FindCCPeak`, `ExtractGranules`, `FaderGroup` dtor, `VorbisReader` dtor, `StreamReceiver::Poll` /
  `StandardStream::PollStream` (retail's pivot-compare switch lowering), `MemInit` (99.995, `.bss` order).
- **Outside a fork's file bounds**: `kdTree` rows (`math/kdTree.h`), `BuildVisit` (shared math inline),
  `~Object` (retail `mRefs` is a `std::list`; `obj/Object.h` is a PCH header), `InlineHelp` rows (retail deletes
  `UILabel` through a fixed +0x214 subobject with no vtordisp — a `UILabel` layout question), `BandCamShot::SetPreFrame`
  (needs an `ObjPtrList` iterator `operator--` in a PCH header).
- **Trade-offs not taken**: `WorldCrowd::CharDef::Load` / `Spotlight::BeamDef::Load` reach 100 with separate rev
  statics but drop each TU's main `Load` 100 → ~99.95 (`.bss` order puts another static between them).
- **Missing behaviour that needs identification first**: `LightPreset::ApplyState` calls an unidentified
  `fn_824AAE10(keyframe)` that pushes StageKit LED/strobe fields to four further unidentified callees.
- **Map misnamings found, not renamed** (map edits on template rows were held back while W16-NR was reworking
  them): MetaMusic `__introsort_loop<ObjEntry*,ObjSort>` holds the `pair<int,float>` /
  `SingerStats::PartPercentageSorter` instantiation (8-byte stride); `RemoveInvalidFreestyle`'s callee takes a fourth
  iterator-category argument, so its mangled name is wrong; CheatProvider `vector<Cheat>` rows hold a 0x20-byte
  element type that is not `Cheat`.
- **Not reopened**: W16-NL's recorded walls (`VocalTrack::UpdateScrolling`, `json_tokener_parse_ex`, `RndMesh::Load`, …).
  W16-NL's reasons stand.

## 7. Gates

On the final code (`be11c7e6d`), after a full `./tools/ninja-locked`:

```
VALIDATE: PASS -- 1791 map-consistent, 308 tolerated (enumerated above), 0 contradicted, 2100 total
[map-injectivity] OK: 33821 applied rows, 33820 distinct names, injective (+1 enumerated internal-linkage exception(s))
[patch-state] OK: tree is a fixed point of 6 post-compile passes (1262 decomp + 3099 target objects)
scripts/validate_symbols.py: 68,740 checked .text functions, 0 invalid
```

Map edits: one new row (`0x827b5470`) and two same-address renames (`0x82287e70`, `0x823342e0`). Shared headers
touched: `char/FileMerger.h`, `bandobj/BandCharacter.h`, `bandobj/BandCharDesc.h`. No PCH header, `math/` header,
`symbols.txt` or `splits.txt` edit. A grep of `78d7d8f02..w16-nq` finds no added line citing rb3-Wii or the
oracle, and no Co-Authored-By line.

## 8. Native gate

Run last on the final code (`be11c7e6d`; log `~/tmp/w16nq/native_gate.log`); only this docs-only commit follows it.

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

Scratch: `~/tmp/w16nq/` (pool `open_rows.json`, slices, fork `result.json`s, keygen scratch compiler `kg/`,
A/B legs and `ab2.log`, `ab_rowdiff.txt`, gate logs).
