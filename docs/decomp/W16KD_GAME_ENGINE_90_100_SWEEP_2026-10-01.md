# W16-KD — fuzzy 90–99.99 sweep: band3 game layer + world/os/beatmatch/obj/ui/utl/synth/synth_xbox (2026-10-01)

**Branch** `w16-kd`, rebased onto main `1b8903f35` (W16-KE). Not merged to main.
**Ruler** `name_check` (graded, from `report.json` `provenance.diff_config`).

**Population.** Every row with `90 ≤ fuzzy < 100` in a unit whose `objdiff.json` `source_path` is under
`src/band3/{meta_band,game,bandtrack,net_band}/` or `src/system/{world,os,beatmatch,obj,ui,utl,synth,synth_xbox}/`.
Rows were ranked by size, because `matched_code` credits a row's full size only at 100. They were
classified by their charged instructions in `objdiff-cli diff` (`~/tmp/w16kd/classify` = W16-JC's `classify.py`).

| measured on | rows | bytes | named rows | named bytes |
|---|---:|---:|---:|---:|
| main `e762a9298` (start) | 1,377 | 367,020 | 796 | 343,044 |
| A/B leg A (main `1b8903f35`) | 1,361 | 364,120 | 780 | 340,144 |
| A/B leg B (this branch) | 1,128 | 318,028 | 606 | 296,508 |

At the start, the named rows split into these classes:
- 264 instruction-level rows (172 KB);
- 346 relocation-name-only rows (74.5 KB);
- 111 register-only rows (skipped);
- 24 register + relocation rows;
- 51 immediate / branch-destination mixes.

`src/system/{bandobj,rndobj,char}` were not touched; other lanes own them.

## 1. Whole-branch A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16kd-ab --patch <git diff 1b8903f35..w16-kd, minus docs and symbols.txt>`
- **Worktree:** made with `scripts/setup_worktree.sh`.
- **Leg A:** main `1b8903f35` plus `8ac607497`, which carries only the split-written `symbols.txt` merge
  (0x828127F0 folded into 0x828127D0, the W16-JC §7 recipe). With 0x828127D0 unnamed on leg A, that
  merge changes no pairing.
- **Patch:** 72 files; kinds map + source + splits.
- **objdiff-cli:** sha `c1b7d952`, stable across legs.
- **Both legs** were read at a split fixed point (0 extra re-splits).
- **Leg B:** 2,262 recompiles, renamer patched 1,858.
- **Run dir:** `~/tmp/wt-w16kd-ab/.ab_measure_runs/20261001-133511-w16-kd-branch-1989989/`.

```
leg A: matched=48934 masked=24223 honest=24711 code%=49.680088  (recompiles: 0, settled)
leg B: matched=49163 masked=24234 honest=24929 code%=50.211240  (recompiles: 2262, split=1, patch_steps=13, settle iterations: 2)
Δmatched=+229  Δmasked_equal=+11  Δhonest=+218  Δcode%=+0.531152pp  Δcode_bytes=+54428
Δfuzzy=+0.026968pp   (legA 58.335632 -> legB 58.362600)
units at 100% [mpn ruler]: legA 347 -> legB 365  (Δ+18; 20 reached 100, 2 fell off)
units at 100% [all-rows-fuzzy ruler]: legA 293 -> legB 305  (Δ+12; 14 reached 100, 2 fell off)
[control none] Δmatched_code=+13916 B -- NOT_APPLICABLE (source in patch)
```

**Prediction, written before the run:** +229 fns / +54,428 B. It came from the in-tree full build of the
rebased tip against a full build of main `1b8903f35` (49,163 / 5,145,216 vs 48,934 / 5,090,788).
**Measured: +229 / +54,428 B exactly.** Leg B equals the tip's in-tree build on every key.

**Row-level diff of the archived leg reports**, keyed by retail address on both rulers (`rowdiff.py`
from W16-JC):
- fuzzy: **362 rows up, 302 of them to 100; 6 down.**
- `mpn`: 281 up, 230 to 100; the same 6 down.

The 6 rows down:

| address | B | before → after | why |
|---|---:|---:|---|
| 0x823F4698 | 4 | **100 → 0** | **False 100, dropped on purpose.** The body is `b fn_82A808D8`, a tail call into the Quazal `/Od` block, so it is the `operator delete` of some Quazal-derived class in the voice-chat code. Our `??3Loader` is `b MemFree`, and retail's `??_GDataLoader` / `??_GDirUnloader` branch to the `b MemFree` survivor 0x8240DDB0 instead. The old 100 read equal only because `name_check` forgives the placeholder target `fn_82A808D8`. The name is nulled and `??3Loader` joins the 0x8240DDB0 group (chase PROVEN). No compiled object can define this function's real name: there is no source for the voice-chat block. |
| 0x822EAB90, 0x82638B50 | 4 + 4 | 95 → 0 | `??3Task` and `??3TypeProps` misnamed the same way. Both spellings now fold into 0x8240DDB0. |
| 0x82726EE8 `op59` | 108 | 99.93 → 98.81 | Behaviour fix: the two xor constants were swapped. What is left is instruction order only. |
| 0x825238F0 | 8 | 99.5 → 0 | The real name is `RemoteUser::GetRemoteUser`, not `LocalUser::GetLocalUser const` (the old row read 99.5 against the wrong body). User.obj does not emit RemoteUser's inline virtuals, so it is unpaired. |
| 0x82BF63C8 | 76 | 99.74 → 0 | An XDK body, `??_GCBaseSkin@LEAPCORE`, misnamed as a `ResourceDirPtr<UILabelDir>` `??_G`. Accuracy only. |

**The only row that leaves 100 is 0x823F4698 (4 B).** Its 100 was a false pairing, and I did not restore
the wrong name to keep it.

**Units that "fell off" 100:**
- HamMaster no longer exists. Its only block was BeatMasterLoader's ctor, which is re-homed into BeatMaster (§3.4).
- StoreArtLoaderPanel's denominator grew by re-homed thunk rows: 21 → 26 rows, 24 at 100.

## 2. Method

1. **Six forks**, one per scope, each in its own `setup_worktree.sh` worktree off `e762a9298`:

   | fork | scope |
   |---|---|
   | meta | meta_band, net_band |
   | game | game, bandtrack |
   | world | world |
   | objui | obj, ui |
   | osutl | os, utl |
   | audio | beatmatch, synth, synth_xbox |

   Each fork owned its directories' source plus the map rows and `.text` lines of its units.
2. **Per change**, every fork ran a full `./tools/ninja-locked` and a whole-report snapshot diff
   (`~/tmp/w16kd/snap.py`, from W16-JD). A change was kept only with no row off 100 and every drop
   explained. Never `run_objdiff` alone.
3. **Integration.** Each fork landed on `w16-kd` with `git merge --no-ff`, followed by a full build.
   - Map conflicts went through W16-ID's key-level resolver. A structural check asserted that the result
     equals upstream plus the fork's own changes.
   - Alias conflicts went through W16-JD's field-merge resolver, extended (`~/tmp/w16kd/resolve_alias.py`)
     to concatenate a group-level string that both sides only appended to.
   - `check_alias_merge.py` asserted the merged membership set is (ours ∪ theirs) minus removals:
     **missing 0, extra 0** on every merge.
   - After each merge, an **address-keyed check** compared each fork's rows at 100 against the merged
     build. That check found the interactions in §4.
4. **Rebase** onto main `1b8903f35` with `--rebase-merges` (§6).

## 3. What was fixed, by fork

Fork-tip figures were measured against main `e762a9298`. They overlap a little (§5).

| fork | Δfns | ΔB | rows up | down | off 100 |
|---|---:|---:|---:|---:|---:|
| meta | +58 | +15,400 | 68 | 0 | 0 |
| audio | +45 | +10,792 | 48 | 1 | 0 |
| objui | +44 | +7,496 | 71 | 0 | 0 |
| world | +39 | +8,076 | 37 | 0 | 0 |
| osutl | +33 | +9,104 | 73 | 0 | 0 |
| game | +21 | +6,508 | 68 | 0 | 0 |

The objui fork counts the 0x823F4698 drop as a GONE row in its name-keyed diff.

### 3.1 meta (meta_band, net_band)

**Rows to 100 (56 in all).** The larger ones:
- `??0BandProfile`;
- `CheckConditionsForSong`, `GetRandomSongs`, `SetupDetailLine`;
- the LessonMgr, AccomplishmentProgress, ModifierMgr, AccomplishmentManager, BandSongMgr, CustomizePanel
  and SongMgr ctors;
- the hash_map ctor family.

**Behaviour bugs fixed:**
- **BandProfile's ctor** built 8 PatchDirs. Retail loops 19 times (`li r26,0x13`), and every save/load path expects 19.
- **CheckHoposPercent / CheckSoloPercent** had a vocals/harmony early-out that retail lacks. Retail's
  hopos and solo arms branch to the Awesomes / DoubleAwesomes bodies, because the fields share a union
  byte; two new fold groups record that.
- **ContentLoadingPanel::ShowIfPossible** called `ShowCurRefreshProgress`, which is declared and never
  defined. Retail calls `RefreshInProgress`.
- **StoreArtLoaderPanel** had a `Load` override that retail's vtable does not.
- **AppLabel** had an empty user dtor that retail does not.
- Two `_outline_` wrappers that retail calls through directly.
- **GetRandomSongs** now searches through const iterators.

**Map and pin repairs:**
- 0x825EC4F8 `~StandIn`.
- 0x82669888 / 0x8266AF90 / 0x8266B3F0 / 0x8266B470: the conditional dtors and the Trainer `??_G`.
- 0x825E8AD0 `GetFilteredPartSym`.
- 0x82545988 `ProfileMgr::SetMusicLibraryUpsell` (was `CharServoBone::SetMoveSelf`, pinned inside
  ProfileMgr's range; one `.text` line moved out of CharServoBone's heading).
- 0x82654260 `SessionUsersProvidersInit`.
- A rotated set of `$4` thunk names and pins across ProfileMgr / SaveLoadManager / MemcardMgr_Xbox /
  StoreArtLoaderPanel.
- 0x82276828 `_Stl_prime::_S_next_size`. Main landed the same rename independently (W16-JH `7addc8565`).

### 3.2 game (game, bandtrack)

**Rows to 100 (55):** the VocalTrack ctor, `GemPlayer::CheckSolo`, `GetFxSwitchPosition`,
`OnPlayTambourine`, `PerfectOverdriveTracker::FirstFrame_`, `VocalPlayer::Start`,
`MultiplayerAnalyzer::AddGem`, `HookUpFxForMicId` and `Game::PopulatePlayerLists`, plus the deque helpers.

**Behaviour bugs fixed:**
- `CheckSolo` cancelled a solo whose next gem sat on the end tick. Retail keeps it while tick ≤ endTick.
- `HookUpFxForMicId` skipped the pitch-correction reset when there was no mic.
- `VocalScoreHistory::AddScore` wraps on an unsigned compare against `size()`.

**Map:** the VocalTrack `_Deque_base` ctors and `_M_initialize_map` names did not match their bodies.
They are re-keyed by each body's buffer constant (32×4, 16×8, 5×24, 10×12).

### 3.3 world

**Behaviour bugs fixed:** `CamShot::Load` and `CamShot::Copy` read and copied a crowd-state override
that RB3 data does not carry (Copy 97.63 → 100, Load 96.97 → 99.88).

**Other source fixes:**
- `LoadSubPart`'s not-found log evaluates like retail (93.49 → 100).
- EventAnim's revs are one file-static aggregate (`Load` 92.38 → 100).
- `RenderCone` converts the beam ObjPtr once.
- LightPresetManager's preset hash_map uses the stock `hash<Symbol>`, so `_M_find` is now retail's 128 B body.

**Map and pin repairs:**
- 0x824E17B0 is the `list<RndMultiMesh::Instance>` copy ctor, re-homed to Crowd.
- 0x824BBE38 is `vector<CameraManager::PropertyFilter>::_M_allocate_and_copy` (`mulli 0x14`), re-homed to CameraManager.
- 0x824B24F0 is `vector<LightPreset::SpotlightEntry>::_M_insert_overflow_aux` (0x58-byte stride).
- 0x823808B0 / 0x82381088 are `vector<CharBones::Bone>` helpers.

**Withdrawals:** two W16-JE memberships that sat under the wrong survivor names are withdrawn with
`MAP_NAME_WRONG` records. `WorldInstance::SyncDir` (1,840 B) reached 100 through fold names.

### 3.4 audio (beatmatch, synth, synth_xbox)

**Behaviour bugs fixed:**
- `StandardStream` keeps two flags: +0xE8 is the format requested from the decoder and +0xE9 is the format
  the decoder reported back. `Init` used to clear the requested flag before creating the decoder.
- `ByteGrinder` op59's two xor constants were swapped.
- MoggClip's `play` handler called `Play(0)`; retail calls `Play()`.

**ByteGrinder:** op58 is written as op40 and folds into it, so `ByteGrinder::Init` (2,084 B) reaches 100.

**Pin repairs:**
- 0x8276E328 was a DC3 HamMaster pin named `HamMasterLoader`. It is BeatMasterLoader's ctor; re-homed and
  the heading removed.
- **Retail has no BinkClip class** (no RTTI). Its three blocks are MoggClip `KillStream` / `Stop` / `Handle`.
- The SfxMap / MoggClipMap helper names were swapped at four address pairs. Two DC3 `StepMoves` /
  `MoveVariant` names are `vector<SfxMap>` and `vector<pair<int,int>>`.
- The `ObjPtr<SeqInst>` vector helpers moved into Sequence.

**Missing bodies:** `~PeakDetector` and `~PitchCorrectedVoice` were declared and never defined. They are
now empty out-of-line bodies folded into the blr survivor.

**Virtual signature:** `SynthSample::NewInst()` and `SampleInst360(SynthSample360*)` take no extra
arguments in the match build, as in retail (NewInst 50.25 → 100). Native keeps `(bool,int,int)`, because
milo-native-engine defines that form.

### 3.5 osutl (os, utl)

**Behaviour bugs fixed:**
- `MemTrackAlloc` no longer tests the tracking flag.
- `Song::SyncState` restores the task clock with `SetSeconds(t, false)`.
- `PrintDiscFile` uses the constant Xbox suffix.
- The `using_cd` handler returns `DataNode(1)`.
- The Xbox keyboard entry points are empty, and `gSource` is a MsgSource.

**Retail shapes:**
- `~CriticalSection` is an empty body folded into the blr survivor, which takes the Synth360 dtor and
  `~MicManagerXbox` to 100.
- There is no `User::SyncProperty`; `BandUser::SyncProperty` calls the right base now (316 B to 100).
- CriticalSection has no class allocator.
- MakeString's buffer state is one aggregate.
- `PointForTime` searches on its parameter.
- `PrintSymbolTable` and `SystemPreInit` evaluate their stripped logs right to left.

**Renames:**
- 0x827CEDD0 is `CacheMgrTerminate`, re-homed to CacheMgr.
- 0x827BE570 is `String::operator=(Symbol)`.
- 0x827BD2F0 is the global `operator new`.

The larger rows that reached 100 are `ContentMgr::PollRefresh` (1,504 B), `PrintDiscFile`,
`SystemPreInit`, `SystemInit` and `MemTracker::Free`.

### 3.6 objui (obj, ui)

**Rows to 100:**
- `yylex` (1,336 B) and `yy_get_previous_state`: DataFlex now carries the shipped scanner tables.
  Retail has no `\r` rule, and flex rebuilds the identical DFA from the edited `.l`.
- `Object::LoadType`, identified and re-homed (36.2 → 100).
- The `PanelDir`, `UITransitionHandler` and `LabelShrinkWrapper` ctors.
- `UIListDir::CreateElements` and the ten WidgetDrawSort rows. These were named LabelSort; LocalePanel is
  not in retail.
- The `MsgSource::EventSink` list helpers. Their `MsgSinks` names were a DC3-only class.

**Behaviour bugs fixed:**
- `ReadEmbeddedFile` restores `gNode` in the match build. It was re-homed out of UsbMidiGuitar's pin.
- `UIList::SelectedPos` returns the display row.
- PanelDir's ctor no longer reads `EditMode`.

## 4. Integration fixes (on the merged branch)

1. **Two `$4` thunks, +2 fns / +24 B.** The meta fork nulled 0x827B79A0 / 0x827B7B28, which had read 100
   under ProfileMgr names only because their targets were placeholders. Retail vtable 0x82116334 lines up
   slot for slot with our `??_7StoreArtLoaderPanel@@6BObject@Hmx@@@`: slot 4 is 0x827B79A0 and slot 5 is
   0x827B7B28. They are named `ClassName` / `SetType` `$4`.
2. **One cross-fork fold, +1 fn / +108 B.** The audio fork renamed 0x824C3D18 to
   `vector<pair<int,int>>::_M_fill_insert`, while the world fork had admitted only the old spelling of its
   overflow callee. The `pair<int,int>` overflow spelling joins group 0x826e6fb0 (chase PROVEN, 0 CYCLE).
   Fixes 1 and 2 were predicted and measured together: **+3 / +132 B exactly.**
3. **Respelled membership withdrawn (Δ0).** The world fork's switch to stock `hash<Symbol>` respelled the
   LightPreset hashtable-ctor membership of group 0x825a07e0 in place. Re-chased as a new membership, it
   reads CHASED T1 REFUTED (the survivor is outside every pinned span), so it is withdrawn with a
   `RESPELLED_NOT_PROVEN` record.
4. **Native gate (Δ0 on the match build).** The first native gate on the merged tip failed 0/18:
   - LightPresetManager.h's stlport `hash<Symbol>` specialization does not exist under the native STL. It
     broke every TU that includes `world/Dir.h`.
   - CameraShot used the match-only `MiloStripEval`.

   The specialization is now `!HX_NATIVE`, native keeps a private hasher, and the CameraShot site is
   dropped natively. A second run caught the CameraShot site after the first fix. The third run passed 18/18.
5. **Two duplicate admission records** (`GetBandUsers` from meta+game; `??3Task` from game+objui) were
   combined into one record each.

## 5. How the deltas compose

| step | Δfns | ΔB |
|---|---:|---:|
| meta + world + game + audio, merged (fork sum +163 / +40,776) | +161 | +40,504 |
| §4 fixes 1–2 | +3 | +132 |
| osutl merged (fork figure exactly) | +33 | +9,104 |
| objui merged (fork +44 / +7,496, less 38 funclet/`??_G` rows / 1,796 B that the other forks' folds had already raised) | +37 | +5,700 |
| **vs `e762a9298`** | **+234** | **+55,440** |
| rebase onto `1b8903f35` (W16-JH's `_S_next_size` rename already raises 5 hashtable rows / 1,012 B) | −5 | −1,012 |
| **vs `1b8903f35` = the A/B** | **+229** | **+54,428** |

The two shortfalls are fully attributed: the first is the thunk/fold interaction in §4, and the second is
overlap with main. **"Gained on the old base, not on the new" lists exactly 5 rows**, and all 5 are at
100 on both main and the tip.

## 6. Rebase onto main `1b8903f35`

- **106 steps.** Every map/alias conflict went through the resolvers above, with the structural checks.
- **`MakeString.cpp`:** main's `49297faad` and the osutl fork's `ddd9e2009` reshaped the same word-sized
  `FormatString::operator<<` overloads to retail's single body. Main's text was kept (it carries fuller
  notes), and the commit keeps only its fold.
- **`splits.txt` Spotlight block:** world and audio each moved one line out. Resolved the same way as
  in the original merge.
- **No re-added membership collides with a W16-JH withdrawal.** Three osutl memberships override older
  ALIAS-CONSOLIDATION `FABRICATED_CLOSURE_NOT_PARTITION` records: `list<NetLoaderRef>`, `list<Loader*>`
  and `list<FileCache*>::insert`. Those records refused closure membership without a per-pair proof. The
  admissions now say so and cite the per-pair chase (`2598b492c`), and the withdrawal records are kept.

## 7. Alias memberships

**150 net new memberships over main `1b8903f35`, 12 removed or re-keyed** (each with a record).
`~/tmp/w16kd/new_alias_pairs.py` extracted them, and they were checked independently on the rebased tip
with main's post-W16-JH tools:
- `tools/icf_pair_adjudicate.py --chase --pairs`: **150 / 150 CHASED T1 PROVEN, 0 lines mentioning
  CYCLE** (`~/tmp/w16kd/chase_rb.log`).
- `tools/alias_placeholder_slot_audit.py`: **150 / 150 CLEAN, cycle 0.** The tree-wide summary is
  CLEAN 4,589, CLEAN-CYCLE 63, LAX-ALSO-FAILS 231. None of the non-CLEAN rows is a membership this branch
  added.

Every fork refused pairs whose chase had a CYCLE-ASSUMED leaf. That covers the `_M_fill_insert` /
`resize` families, `_Rb_tree::clear`, `sort<RndPollable*>` and `sort<CuePoint>`.

## 8. Rows left, by blocker

- **Register allocation / scheduling (about three spellings each).** This is the largest class:
  - game: VocalTrack `UpdateScrolling` (8,948 B, not opened), `SetupGems`, `DrawTrackMasks`,
    `VocalPlayer::Poll` / `HandlePhraseEnd`, `HandleExitExtent`;
  - meta: `SaveLoadManager::SetState`, `MaybePublish`, `BuildList`;
  - world: `Spotlight::SyncProperty`;
  - obj: `ObjectDir::PreLoad` / `Save`;
  - ui: `BuildDrawState`;
  - audio: `HandleRGGemStop`, Synapse / GranularSynth.
  - os/utl and audio also have a long tail of small rows.
- **Stack / `.bss` layout:**
  - `CamShot::Load` (frame 0x10 larger);
  - `LightPreset::Load` (`.bss` hole);
  - MemMgr's `gNumHeaps` at `gHeaps+0x254` (0x14 B of unreferenced statics);
  - `gSystemConfig` vs `gUsingCD` anchoring;
  - `ExternalMic::sampleProcessThread`.
- **PCH input `obj/Object.h` (not edited):** `Object::LoadRest`, `TypeProps::Load(BinStream&, bool)`, and
  the WorldDir ctor / PostLoad / Save in the PropSync TU.
- **Chase not provable (CYCLE leaf or refuted):**
  - the `_M_fill_insert<Object*>` family (LightPreset Copy / Load, `RandomizeCategory`);
  - `_Copy_Construct<ObjPtr<SeqInst|Sequence>>` (656 B; its groups carry survivors that disagree with the map);
  - 11 pointer `push_back`s in `SongData::AddTrack`;
  - `__find<const int*>` (176 vs 172 B);
  - `MakeString<HH>` at 0x82399348, whose body is an ObjVector `operator=`. This costs 480 B in the
    OverdriveTracker units; FLAT PROVEN, CHASED REFUTED.
- **dtk mis-carves (`symbols.txt`, not edited here):** `DataNode::operator==`,
  `UIGridSubProvider::UpdateExtendedText`, `CodaHit`, `DeletePlayer`, `TrainerGemTab`,
  `PlayFinalizedSound`, `SetLeaderboardStatus`, and the PerformerStatsInfo ctor.
- **No source in any tree:**
  - `LightPreset::ApplyState`'s StageKit LED calls (0x82521B98–0x82522028);
  - `SystemPoll`'s trailing stage-kit poll `fn_82521ED0`;
  - the voice-chat block (including 0x823F4698).
- **Owned elsewhere, recorded:**
  - `RndMultiMesh::InvalidateProxies` is empty in retail (rndobj).
  - Two Waypoint rows (char) carry SeqInst helper names.
  - 0x82466298 is `DOFProc::Init` (Console pin).
  - `HttpGet` needs a 0x70-byte layout for the NetLoaderXbox ctor.
  - `ObjectDir::Terminate` is `b DeleteShared`, misnamed `_Destroy<InlinedDir>`.
  - `hamobj/HamMaster.cpp`'s comment now describes an address that belongs to BeatMaster.
- **Engine change request (for the coordinator, who owns the pin):** milo-native-engine's
  `SampleInst_Native.cpp` should define RB3's no-argument `SynthSample::NewInst()`. The `HX_NATIVE`
  `(bool,int,int)` declaration in `synth/SynthSample.h` can then go.

## 9. Gates (rebased tip `2598b492c`, after a full build)

```
[map-injectivity] OK: 32777 applied rows, 32776 distinct names, injective (+1 enumerated internal-linkage exception(s))
VALIDATE: PASS -- 1646 map-consistent, 269 tolerated (enumerated above), 0 contradicted, 1916 total
[patch-state] OK: tree is a fixed point of 6 post-compile passes
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

The native gate ran last, after the A/B, on the final code commit. Only this docs-only commit follows it.

## 10. Not done

- **Not merged to main**, per the brief.
- No hand edits to `symbols.txt`. The one `symbols.txt` change is the split's own merge after naming
  `UIPanel::FocusComponent` (`82ba61a4b`).
- The permuter was not run.
- `VocalTrack::UpdateScrolling` was not opened.
- No added source comment cites rb3-Wii or "the oracle" (grepped over the branch diff under `src/` and `native/`).
- No edits to `src/system/{bandobj,rndobj,char}` or to the PCH inputs. Some rows in those units moved
  through map or alias names only (for example, the CharServoBone `.text` line in §3.1 and the
  `ListDrawChildren` rows).
