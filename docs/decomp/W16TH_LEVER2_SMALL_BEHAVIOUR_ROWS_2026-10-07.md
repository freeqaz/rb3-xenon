# W16-TH — lever 2: the last small behaviour-class rows (2026-10-07)

Lane W16-TH took lever 2 of `CAMPAIGN_STATE_2026-10-07b.md` §6:
- the 8 in-scope unpaired named rows;
- the 13 VIA-DC3 unpaired named rows;
- `SuperFormatString`'s three-argument ctor (1,608 B, 94.33);
- `OggMap::OpenMogg` (1,004 B, 97.51).

Worktree `~/tmp/wt-w16th`, branch `w16-th`, rebased onto main `d5cbd3cbc` (W16-SY, dtk 1.15.0).

## Result

- **20 of the 21 unpaired named rows are at 100.** The 21st, `SendDataPoint<const char*,int>`, has a stop reason
  below. Two anonymous rows are now named and at 100 as well: `fn_82272E50` = `SetSongMidiChecksumData` and
  `fn_826FE8A8` = `Synth::Play`.
- **Two real callee defects were exposed by the new names and are fixed.** `OvershellSlot` called `PlaySound`
  where retail calls `Synth::Play`, and three classes called `operator delete` out of line.
- **`SuperFormatString`'s ctor: 94.33 → 97.96.** The residual is codegen; stop reason below.
- **`OggMap::OpenMogg` stays at 97.51.** The mechanism is now identified (a compiler temporary shares the
  `new`-expression EH slot), but no source spelling produces it; stop reason below.
- **Whole branch, `tools/ab_measure.py --patch`, rebased onto main `d5cbd3cbc` (dtk 1.15.0), ruler `name_check`:**
  **Δmatched +22, Δcode_bytes +1,416, Δcode% +0.013820 pp, Δfuzzy +0.014370 pp. No row went down.**
- `icf_alias_finder.py --validate`: PASS (2,159 groups, 0 contradicted).
- Native gate, run in `~/tmp/wt-w16th` at the branch tip after the last source commit:
  ```
  NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
  ```

## Method

An unpaired row is a retail name that the pinned unit's base obj does not define. objdiff pairs by name, so the
row reads 0% however good our code is. I resolved each row one of three ways:
- **re-home**: the `.text` block moves in `splits.txt` to the TU whose compiled obj emits the name;
- **respell**: at an ICF fold address the map name is arbitrary, so I renamed it to a spelling the pinned obj
  defines and swapped the alias group's survivor to match;
- **body**: I wrote the missing definition.

For each row:
1. I read retail's body.
2. I found every compiled obj that defines a byte-identical COMDAT, comparing relocation-masked bytes and
   relocation target names (`~/tmp/w16th_cand2.py`).
3. I attributed each retail caller by its enclosing `.fn` symbol, not by the `.s` address column, which is
   synthetic for multi-block units.

## Per-row dispositions

### In scope (8 rows / 524 B)

| row | B | retail | disposition | after |
|---|---:|---|---|---|
| `??$Find@VRndCam@@@ObjectDir@@QAAPAVRndCam@@PBD_N@Z` | 164 | `0x822E4A30` | **re-home** GemTrackDir → TrackPanelDir (`.text 0x822E4A30–0x822E4AD4`; GemTrackDir keeps `0x822E46EC–0x822E4A30`). Every retail caller is in Splash or TrackPanelDir, and TrackPanelDir.obj emits a byte-identical COMDAT. Nothing in GemTrackDir calls it, so retail's GemTrackDir.obj presumably emitted it for code `/OPT:REF` later dropped. **The home is an approximation:** it pairs with an identical body, but it is not provably the obj the linker kept. | 100 |
| `??1?$ObjPtr@VBandStarDisplay@@@@UAA@XZ` | 116 | `0x822CD918` | **re-home** BandStarDisplay → BandScoreboard (`0x822CD90C–0x822CD9B4`; the 40 B funclet `fn_822CD98C` moves with it, at 100 on both sides). **Source:** `RB3_TU_OBJPTR_OUTOFLINE_DTOR` used to leave `~ObjPtr` undefined in its one TU, so BandScoreboard.obj could not emit retail's body. It now defines the same body `__declspec(noinline)` (`obj/ObjPtr_p.h`), which keeps the stack-slot packing that `SetupScore` needs: `SetupScore` stays at 100. Retail's callers are `??_G?$ObjPtr<BandStarDisplay>` and `~BandScoreboard`, both BandScoreboard code | 100 |
| `?SetObjConcrete@?$ObjPtr@VObject@Hmx@@@@QAAXPAVObject@Hmx@@@Z` | 104 | `0x8238B130` | ICF fold address. **re-home** Msg → CharLipSyncDriver (`0x8238B130–0x8238B198`) and **respell** to `?SetObjConcrete@?$ObjPtr@VCharLipSync@@@@…`; alias survivor swapped, old spelling kept as folded | 100 |
| `??$SendDataPoint@PBDH@@YAXPBD0H@Z` | 100 | `0x82563258` | **STOP.** The only retail caller is in the unpinned `auto_03_825632BC_text` TU, which sends `"rbn/audition/fail"` with a `"pid"` field. That is TU5 RBN-audition code with no counterpart in rb3-Wii, DC3 or our tree. MetaPerformer.obj does not instantiate `<const char*,int>`, and no compiled obj does. Closing the row needs that TU identified and written | 0 |
| `?SetFileChecksumData@@YAXXZ` | 16 | `0x82272E40` | **body**, in a new TU `src/ChecksumData_xbox.cpp` (`objects.json` NonMatching; `.text 0x82272E3C–0x82272E68` re-homed out of Object.cpp). It holds retail's two tables, generated from band.exe `.data`: `gFileChecksums[1065]` at `0x82C64A60` and `gSongMidiChecksums[62]` at `0x82C6AE38`, each 0x18 B `FileChecksum`. Each wrapper calls `SetFileChecksumData(table, DIM(table))`. The second wrapper, `SetSongMidiChecksumData` (`fn_82272E50`), is named as well. Native keeps `native/src/platform/ChecksumData_Stub.cpp` and does not glob `src/` root | 100 (+1 named) |
| `?Load@UIPicture@@$4PPPPPPPM@A@AAXAAVBinStream@@@Z` | 12 | `0x82823728` | fold address in the UIProxy pin. **respell** to `?Load@UIProxy@@$4…`, which UIProxy.obj emits; survivor swapped | 100 |
| `?SetTrackPanel@TrackPanelDirBase@@UAAXPAVTrackPanelInterface@@@Z` | 8 | `0x823038A0` | **re-home** TrackPanelDir → TrackPanelDirBase (`0x823038A0–0x823038A8`; `…A8–…B0` stays). Retail's TrackPanelDir.obj carried the TrackPanelDirBase vtable for a reason not visible in source; the inline body is emitted by TrackPanelDirBase.obj. **The home is an approximation** for the same reason as `Find<RndCam>` | 100 |
| `??1?$map@HM…` (`map<int,float>` dtor, SongSortMgr) | 4 | `0x825971F0` | **respell** to `??1?$_Rb_tree<int,…,pair<const int,float>…>`, plus a **new alias group** with `map<int,float>::~map` and `::clear` folded. All three are a 4 B `b _Rb_tree<int,float>::clear` with an identical relocation | 100 |

### VIA-DC3 (13 rows / 804 B)

| row | B | retail | disposition | after |
|---|---:|---|---|---|
| `??_EBandRetargetVignette@@UAAPAXI@Z` | 68 | `0x822C6A40` | **re-home** Instance → BandRetargetVignette (`0x822C6A40–0x822C6A90`) and **respell** `??_E`→`??_G`. MSVC emits `??_E` as a weak external aliasing `??_G`, and the 223 matched `??_E…$4` thunks all map their target as `??_G`. Exposed the inline-delete defect (see below) | 100 |
| `??_ECharIKHead@@UAAPAXI@Z` | 68 | `0x823C1798` | **respell** `??_G`; inline-delete fix | 100 |
| `??_ECharMirror@@UAAPAXI@Z` | 68 | `0x823CDCB0` | **respell** `??_G`; inline-delete fix. CharMirror reaches 100% as a unit | 100 |
| `?SynthPoll@Sfx@@UAAXXZ` | 4 | `0x8271A0B8` | **re-home** StreamNull → Sfx (`0x8271A0B8–0x8271A0BC`; StreamNull now `0x82719CB0–0x8271A0B8`). No neighbouring obj emits it | 100 |
| `?insert@?$list@PAVObject@Hmx@@…` | 100 | `0x823D14C0` | fold address. **respell** `list<CharClip*>::insert`, which CharClipSet.obj emits; survivor swapped | 100 |
| `??3BinStream@@SAXPAX@Z` | 4 | `0x8240DDB0` | fold address. **respell** `??3RndCubeTex@@SAXPAX@Z`, which Rnd.obj emits; survivor swapped | 100 |
| `??$__uninitialized_copy@PBVSampleMarker@@…` | 96 | `0x822A2E10` | **re-home** SampleInst → OutfitConfig (`0x822A2E10–0x822A2E78`) and **respell** to the `const OutfitConfig::Overlay*` instantiation; survivor swapped | 100 |
| `??$__uninitialized_copy@PAUUnlockable@?A0x…` | 96 | `0x827070F0` | fold address. **respell** to the `ObjPtr<Sequence>*` instantiation, which Sequence.obj emits; survivor swapped | 100 |
| `??$Find@VSequence@@@Synth@@QAAPAVSequence@@PBD_N@Z` | 128 | `0x826FE428` | **body**: the missing `Synth::Play(const char*, float, float, float)` (`0x826FE8A8`, 172 B, laid out after `OnPassthrough`). Sfx.cpp scatter-includes Synth.cpp, so Sfx.obj now instantiates `Find<Sequence>`. `Play` itself names `fn_826FE8A8` and is at 100. It exposed the OvershellSlot defect (see below) | 100 (+1 named) |
| `??_GLabelShrinkWrapper@@$4PPPPPPPM@A@AAPAXI@Z` | 12 | `0x82826E28` | **respell** `??_E…$4`, the spelling our thunk emits, as for the 223 matched thunks. LabelShrinkWrapper reaches 100% | 100 |
| `??_GInlineHelp@@$4PPPPPPPM@A@AAPAXI@Z` | 12 | `0x82316428` | **respell** `??_E…$4` | 100 |
| `??$New@VRndMesh@@@Object@Hmx@@SAPAVRndMesh@@XZ` | 72 | `0x822DC830` | **re-home** FlowOnStop → ChordShapeGenerator (`0x822DC828–0x822DC878`). It was FlowOnStop's only block, so the heading is deleted. The 8 B anonymous `fn_822DC828` moves with it and stays at 0 | 100 |
| `??_G?$ObjPtr@VSynthSample@@@@UAAPAXI@Z` | 76 | `0x826FCD78` | **re-home** system/synth/Synth.cpp → Sfx.cpp (`0x826FCD78–0x826FCDC8`; Synth keeps `…CB28–…CD78` and `…CDC8–…CE10`) | 100 |

### Bugs the new names exposed

Naming a previously anonymous address charges any call site that names a different callee. The first A/B lost
3,032 B to two such defects, and both are fixed in `3964c6baa`.

1. **`OvershellSlot` called the wrong Synth method.** Retail's `0x826FE8A8` is `Synth::Play(const char*, float,
   float, float)`:
   - its body is `CheckCommonBank`, then `Find<Sequence>`, then `Sequence::Play`, with no warning path;
   - it was unnamed and had no body in our tree;
   - GamePanel and ClosetMgr already called it by that name.

   All six of OvershellSlot's cue calls (`overshell_up/down/back/select`, `button_error`, `slider`) go there in
   retail, and rb3-Wii's OvershellSlot also calls `TheSynth->Play`. Ours called DC3's `Synth::PlaySound`, which
   plays a `Sound` rather than a `Sequence` and warns when the sound is missing. That is a behaviour difference: in
   our build those six cues looked up the wrong object type. All six now call `Play`, and `UpdateState` (2,240 B)
   and the two `OnMsg` rows return to 100.
2. **The class `operator delete` was not inlined.** Once `??_GCharIKHead`, `??_GCharMirror` and
   `??_GBandRetargetVignette` paired, each read 99.71, because retail calls `MemFree` directly where we called the
   out-of-line `??3Class`. The three classes now use `OBJ_MEM_OVERLOAD_INLINE_DEL`, as `CharIKHand` does
   (`utl/MemMgr.h`, lane W4-G). No retail `??3` row exists for any of the three.

## `SuperFormatString::SuperFormatString(const char*, const DataArray*, bool)` — 94.33 → 97.96 (stop: codegen)

W16-QF had already compared every placeholder path clause by clause and found no behaviour difference. The 5.7 pp
was layout, so this row is tier C in effect.

I searched with scratch compiles: the same cl 10224 flags with `/FI decomp_pch.h` and no PCH, scored by
`objdiff-cli diff` against the target obj under the grader's four pinned options. The control reproduced
**94.33085**, main's report value. I ran a grid over seven spelling axes, then coordinate descent from the best
point. Each step is below.

| change | fuzzy |
|---|---:|
| baseline (main) | 94.33085 |
| `{{` escape: test `p[1] == '{'` first, `p++` then copy `*p`; int/float set `phType` before `'%'`/`state`; ordinal sets `state` first; no separate local for the `"string"` strcmp result | 96.24876 |
| locals declared `tempFmtPos, tempFmtEnd, phInfoPos, paramPos, phType, state` (retail's prologue initialises them in that order; best of all 720 orders) | 97.42288 |
| `LocalizeOrdinal(node.Int(), gender, num, false)` with gender and num as locals | **97.95522** |

Coordinate descent over all seven axes from that point found nothing better.

Residual, in three places:
- **int/float tail.** Retail's int case jumps into float's `stb '%'; li state,2` tail, and float's tail then
  shares case 2's `paramPos++`. Ours merges the other way, with float jumping into int's. Four statement orders
  and four increment placements were tried, and none reverses it.
- **escape path.** We reload `*p` after storing `p`, where retail stores the byte it already loaded. Three
  spellings were tried: `*++p`, `p[1]`, and `p++` followed by `*p`.
- **ordinal.** Two stores are scheduled in a different order.

Stop reason: all three are codegen and the permuter is off by directive. Behaviour is unchanged by every edit,
since each is an equivalent spelling.

## `OggMap::OpenMogg` — stays 97.51; stop reason, with the mechanism now identified

W16-RG's residual:
- retail puts the 64-bit header temp at `0x58`, the slot of the `new FileStream` EH temp, and `hdrSize` at `0x60`;
  we have `0x60` and `0x5c`;
- around each 64-bit read, retail stores `gMagicA`/`gMagicB`/`gKeyIndex` before it reloads `mFile`.

Probes (scratch compiles as above):

| spelling | 64-bit slot | frame | note |
|---|---|---|---|
| main (one function-scope `long long val`) | 0x60 | 0xb0 | |
| `val` declared in the `else if` block | 0x60 | 0xb0 | inert, as RG found |
| `hdrSize` in its own block | 0x60 | 0xb0 | inert |
| three sibling blocks `{ long long v; … }` | 0x60 / 0x68 / 0x70 | **0xc0** | cl 10224 does **not** share slots between sibling-scope named locals here |
| `long long &v = const_cast<long long&>(static_cast<const long long&>(0LL))` | **0x58** | 0xb0 | `hdrSize` moves to **0x60**: retail's exact frame |

The last probe shows the mechanism. Retail's 64-bit value is a **compiler temporary**: only a temporary shares
the slot of the `new` expression's EH temporary, and every named local gets its own slot. The probe is not
shippable, because binding the reference to `0LL` adds a zero-initialising `std` before each read (93.43 overall).
I found no natural spelling that creates an uninitialised `long long` temporary and passes its address to
`BinStream::ReadEndian`. W16-RG had already tried an inline helper that returns by value, and it gets a slot per
inlined copy.

The `stw`-before-`lwz mFile` order sits in the same instructions and may follow from the slot. Stop reason: a
stack-temporary shape with no source spelling found. DC3's `VorbisReader` records the same unsolved shape.

## Measurement

All three runs used `tools/ab_measure.py`, ruler `name_check`, one at a time, `--jobs 12`.

| run | what | base | Δmatched | Δcode_bytes | note |
|---|---|---|---:|---:|---|
| 1 | `f6bf6c9a9`, pre-rebase, now `0c91c2aac` (homes, respellings, bodies) | pre-rebase main `dbe68f5a7` | +16 | **−1,820** | OvershellSlot −3,032 B and three `??_G` rows at 99.71: the exposed defects |
| 2 | `ee0e30c4e`, pre-rebase, now `3964c6baa` (the two exposed-defect fixes) | run 1's leg B | +6 | +3,236 | = 3,032 + 3 × 68 exactly |
| 3 | whole branch as one patch | main `d5cbd3cbc` (after W16-SY, dtk 1.15.0) | **+22** | **+1,416** | the landing figure; SFS ctor 94.331 → 97.955 |

- Runs 1 and 2 finished before the fleet dtk swap at 09:24:51. Run 3 is on the rebased tree, and both of its legs
  were at a `symbols.txt` split fixed point with 0 extra re-splits.
- Run 1's split re-derived 10/−6 `.pdata` lines. They are committed in `da9054805`, so run 3 was already at a
  splits fixed point.
- Row-level diff of run 3, legA vs legB: every changed row is listed in the two tables above, plus the SFS ctor.
  **No row's fuzzy went down.** BandStarDisplay's unit count drops by 1 only because its 40 B funclet moved to
  BandScoreboard, at 100 on both sides.
- Logs: `~/tmp/rb3_build_w16th_ab{1,2,3}.log`; run dirs are under `~/tmp/wt-w16th/.ab_measure_runs/`.

## What this lane did not do

- No permuter, and no register-allocation grinding past the axis searches above.
- `??_EMemStream@@UAAPAXI@Z` in NetSession has the same `??_E`→`??_G` respelling pattern, but it is in the NET
  ring, which is out of ranking by directive. Not touched.
- No alias was installed without an evidence line. The new `map<int,float>` group records its evidence: all three
  spellings are a 4 B `b _Rb_tree<int,float>::clear` with an identical relocation.
- No merge and no push.
