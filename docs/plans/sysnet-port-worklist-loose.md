# system/network porting worklist — net-new Wii→Xenon identities

**Generated:** `tools/gen_sysnet_port_worklist.py` (regenerable). **Source:** `ghidriff_identities_loose.json` (ACCEPT tier) minus `scripts/target_symbol_map.json`, `category ∈ {system, network}`.
**Data feed:** `sysnet_port_worklist_loose.json` (machine-readable, one row per fn; gitignored/regenerable).

> **LOOSE BAND (ws2, BSim simconf 10–15).** CANDIDATES from the run-3 archive re-vetted at the looser ≥10 operating point (sibling-check REJECT applied). Measured band precision ≈ **0.85** (ws2 20-pair judging incl. system+network: 18 confirmed / 2 plausible / 0 wrong on non-contradicted rows). **Confirm-on-consume every id**; **skip `rb3wii=contradicted` rows** unless separately judged. Future-round candidate pool — no strict matches minted directly.

## What this is

474 RB3 **engine + netcode** functions across **269 TUs** (**270 system** + **204 network**), each pinned to a specific Wii (Bank-8, CodeWarrior-mangled) function by the forked-ghidriff/BSim Wii→Xenon identity pipeline. These are **net-new**: their Xenon address is NOT yet in the production pairing set (`target_symbol_map.json`), re-derived against the **live** map on each regen.

This LOOSE tranche (BSim 10–15) is the **ws2 candidate extension** below the strict 0.967-precision 15+ band. Measured band precision ≈ **0.85** (ws2 judging). Much of system/network is shared Milo engine + Quazal netcode where **DC3 BinDiff also helps**, so the marginal value is lower than band3's — but the Quazal netcode slice is still DC3-cannot-provide. Confirm-on-consume each id.

**This is a targeting/porting worklist + per-fn identity oracle, NOT a `target_symbol_map.json` injection.** Many TUs aren't compiled yet (no MSVC symbol to pair), and our `wii_symbol` is CW/MWCC-mangled, not MSVC-mangled — injecting it as a map key would mis-pair objdiff (actively harmful). Use this to pick which engine/netcode TU to port next and to name each function from the Wii body. Both outputs are additive + reversible.

## Safe-first slice + the confirm-on-consume tier

- **Safe-first core = HIGH + BSim≥30: 0 rows** (0 high + 0 bsim≥30). This is the human-judged-1.000 slice — port/name these with the most trust. Table below.
- **BSim 15–20 = confirm-on-consume.** That band holds the **only** measured miss across the 30-pair sample: `TrackWidget::Init` was aliased to its sibling `TrackWidget::Empty` (the two 20-byte `mImp->virtual()` forwarders differ ONLY in the vtable-slot immediate — Init forwards through slot `0x44`, Empty through `0xc`). **Verify each BSim 15–20 name per-fn when a porter actually consumes it** (diff vtable-slot / type-tag / node-size immediates + referenced strings + resolved callees against the Wii body).

## DC3 reachability

Most system/network is shared Milo engine that **DC3 can supply** (DC3 is the same engine on the same Xbox 360 toolchain — `dc3_cannot_provide` defaults **False**). **127 rows are flagged genuinely DC3-unreachable** (`dc3_cannot_provide=true`): their rb3 source file is absent AND no same-named `.cpp` exists in the DC3 tree — overwhelmingly the Quazal/ObjDup netcode proprietary to RB3's Wii build with no DC3 twin. Those rows mark `DC3?=cannot-provide` in the rosters; all others mark `DC3?=shared`.

## Confidence strata (the measured prior)

system/network human-judged precision (n=30) = **0.967**. Totals here: **0 high** · **0 bsim≥30** · **0 bsim20-30** · **0 bsim15-20**.

- **high** — `ExactInstructions`/`SwitchSig`/`Implied`/`SymbolsHash`, or BSim simconf ≥ 30. The safest-first targets.
- **bsim≥30 / bsim20-30 / bsim15-20** — BSim similarity×confidence bands; lower = vet harder. **bsim15-20 = confirm-on-consume.**

## Safe-first subset — HIGH + BSim≥30 (verify these names with most trust)

| Xenon addr | cat | TU | src | confidence | match | Wii signature | wii_symbol | DC3? |
|---|---|---|---|---|---|---|---|---|

## TU ranking (port these first — by #high+#bsim≥30 desc, then total desc)

| Rank | cat | TU | src | #ids | high | ≥30 | 20-30 | 15-20 | 10-15 | contra | DC3? |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | system | PlayerTrackConfigList.o | `src/system/beatmatch/PlayerTrackConfigList.cpp` | 10 | 0 | 0 | 0 | 0 | 10 | 0 | shared |
| 2 | system | Movie.o | `src/system/rndobj/Movie.cpp` | 10 | 0 | 0 | 0 | 0 | 10 | 1 | shared |
| 3 | system | BeatMatcher.o | `src/system/beatmatch/BeatMatcher.cpp` | 9 | 0 | 0 | 0 | 0 | 9 | 2 | shared |
| 4 | network | DuplicatedObject.o | `src/network/ObjDup/DuplicatedObject.cpp` | 10 | 0 | 0 | 0 | 0 | 10 | 4 | shared |
| 5 | system | Rnd.o | `src/system/rndobj/Rnd.cpp` | 7 | 0 | 0 | 0 | 0 | 7 | 1 | shared |
| 6 | network | Buffer.o | `src/network/Plugins/Buffer.cpp` | 6 | 0 | 0 | 0 | 0 | 6 | 1 | shared |
| 7 | network | Station.o | `src/network/ObjDup/Station.cpp` | 6 | 0 | 0 | 0 | 0 | 6 | 1 | cannot-provide |
| 8 | system | SongData.o | `src/system/beatmatch/SongData.cpp` | 5 | 0 | 0 | 0 | 0 | 5 | 0 | shared |
| 9 | system | UIListState.o | `src/system/ui/UIListState.cpp` | 5 | 0 | 0 | 0 | 0 | 5 | 0 | shared |
| 10 | system | BandCharDesc.o | `src/system/bandobj/BandCharDesc.cpp` | 4 | 0 | 0 | 0 | 0 | 4 | 0 | shared |
| 11 | network | CallMethodOperation.o | `src/network/ObjDup/CallMethodOperation.cpp` | 4 | 0 | 0 | 0 | 0 | 4 | 0 | cannot-provide |
| 12 | network | DOCallContext.o | `src/network/ObjDup/DOCallContext.cpp` | 4 | 0 | 0 | 0 | 0 | 4 | 0 | cannot-provide |
| 13 | system | GameGem.o | `src/system/beatmatch/GameGem.cpp` | 4 | 0 | 0 | 0 | 0 | 4 | 0 | shared |
| 14 | system | SongParser.o | `src/system/beatmatch/SongParser.cpp` | 4 | 0 | 0 | 0 | 0 | 4 | 0 | shared |
| 15 | system | CameraManager.o | `src/system/world/CameraManager.cpp` | 4 | 0 | 0 | 0 | 0 | 4 | 1 | shared |
| 16 | system | MasterAudio.o | `src/system/beatmatch/MasterAudio.cpp` | 4 | 0 | 0 | 0 | 0 | 4 | 1 | shared |
| 17 | system | MoggClip.o | `src/system/synth/MoggClip.cpp` | 4 | 0 | 0 | 0 | 0 | 4 | 1 | shared |
| 18 | network | ObjDupProtocol.o | `src/network/ObjDup/ObjDupProtocol.cpp` | 4 | 0 | 0 | 0 | 0 | 4 | 1 | cannot-provide |
| 19 | system | UIList.o | `src/system/ui/UIList.cpp` | 4 | 0 | 0 | 0 | 0 | 4 | 1 | shared |
| 20 | system | BandCrowdMeter.o | `src/system/bandobj/BandCrowdMeter.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 0 | shared |
| 21 | network | BerkeleySocketDriver.o | `src/network/Platform/BerkeleySocketDriver.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 0 | shared |
| 22 | network | CallContext.o | `src/network/Core/CallContext.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 0 | shared |
| 23 | network | Credentials.o | `src/network/Services/Credentials.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 0 | cannot-provide |
| 24 | network | DOClass.o | `src/network/ObjDup/DOClass.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 0 | cannot-provide |
| 25 | network | DOHandle.o | `src/network/ObjDup/DOHandle.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 0 | cannot-provide |
| 26 | network | EndPoint.o | `src/network/Plugins/EndPoint.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 0 | cannot-provide |
| 27 | network | HMACChecksum.o | `src/network/Plugins/HMACChecksum.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 0 | shared |
| 28 | network | JsonUtils.o | `src/network/net/JsonUtils.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 0 | shared |
| 29 | system | KeyboardTrackWatcherImpl.o | `src/system/beatmatch/KeyboardTrackWatcherImpl.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 0 | shared |
| 30 | network | MessageBrokerDDL_Wii.o | `src/network/net/MessageBrokerDDL_Wii.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 0 | shared |
| 31 | system | NetLoader.o | `src/system/utl/NetLoader.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 0 | shared |
| 32 | network | Protocol.o | `src/network/Protocol/Protocol.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 0 | shared |
| 33 | network | SecureStream.o | `src/network/Services/SecureStream.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 0 | cannot-provide |
| 34 | system | TrackDir.o | `src/system/track/TrackDir.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 0 | shared |
| 35 | system | UI.o | `src/system/ui/UI.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 0 | shared |
| 36 | system | UILabelDir.o | `src/system/ui/UILabelDir.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 0 | shared |
| 37 | system | MidiInstrument.o | `src/system/synth/MidiInstrument.cpp` | 4 | 0 | 0 | 0 | 0 | 4 | 2 | shared |
| 38 | system | BandTrack.o | `src/system/bandobj/BandTrack.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 1 | shared |
| 39 | system | DirLoader.o | `src/system/obj/DirLoader.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 1 | shared |
| 40 | network | DuplicationSpace.o | `src/network/Extensions/DuplicationSpace.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 1 | cannot-provide |
| 41 | system | StoreArtLoaderPanel.o | `src/system/meta/StoreArtLoaderPanel.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 1 | shared |
| 42 | system | Anim.o | `src/system/rndobj/Anim.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | shared |
| 43 | network | BundlingPolicy.o | `src/network/ObjDup/BundlingPolicy.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | cannot-provide |
| 44 | network | ChangeDupSetOperation.o | `src/network/ObjDup/ChangeDupSetOperation.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | cannot-provide |
| 45 | system | CharMeshHide.o | `src/system/char/CharMeshHide.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | shared |
| 46 | system | Character.o | `src/system/char/Character.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | shared |
| 47 | network | ConnectionOrientedStream.o | `src/network/Plugins/ConnectionOrientedStream.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | cannot-provide |
| 48 | network | FaultProcessingContext.o | `src/network/ObjDup/FaultProcessingContext.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | cannot-provide |
| 49 | network | FetchContext.o | `src/network/ObjDup/FetchContext.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | cannot-provide |
| 50 | network | IOCompletionNotifier.o | `src/network/Platform/IOCompletionNotifier.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | cannot-provide |
| 51 | network | JobBackEndServicesLogin.o | `src/network/Services/JobBackEndServicesLogin.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | cannot-provide |
| 52 | network | JobCreateAccount.o | `src/network/Services/JobCreateAccount.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | cannot-provide |
| 53 | network | JobDisconnectStation.o | `src/network/ObjDup/JobDisconnectStation.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | cannot-provide |
| 54 | network | JobTerminateDOCore.o | `src/network/ObjDup/JobTerminateDOCore.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | cannot-provide |
| 55 | network | JobTerminateFacade.o | `src/network/Products/JobTerminateFacade.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | cannot-provide |
| 56 | network | KerberosEncryption.o | `src/network/Services/KerberosEncryption.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | cannot-provide |
| 57 | system | Loader.o | `src/system/utl/Loader.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | shared |
| 58 | network | Log.o | `src/network/Platform/Log.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | shared |
| 59 | system | Mesh.o | `src/system/rndobj/Mesh.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | shared |
| 60 | system | MeterDisplay.o | `src/system/bandobj/MeterDisplay.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | shared |
| 61 | system | PitchArrow.o | `src/system/bandobj/PitchArrow.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | shared |
| 62 | system | RGState.o | `src/system/beatmatch/RGState.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | shared |
| 63 | system | RGUtl.o | `src/system/beatmatch/RGUtl.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | shared |
| 64 | network | RefCountedObject.o | `src/network/Platform/RefCountedObject.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | shared |
| 65 | system | Rot.o | `src/system/math/Rot.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | shared |
| 66 | system | ScoreDisplay.o | `src/system/bandobj/ScoreDisplay.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | shared |
| 67 | network | SpinTest.o | `src/network/Platform/SpinTest.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | cannot-provide |
| 68 | system | StarDisplay.o | `src/system/bandobj/StarDisplay.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | shared |
| 69 | network | StationManager.o | `src/network/ObjDup/StationManager.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | cannot-provide |
| 70 | system | StreakMeter.o | `src/system/bandobj/StreakMeter.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | shared |
| 71 | network | StreamSettings.o | `src/network/Plugins/StreamSettings.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | shared |
| 72 | system | Synth.o | `src/system/synth/Synth.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | shared |
| 73 | system | System.o | `src/system/os/System.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | shared |
| 74 | network | SystemComponent.o | `src/network/Core/SystemComponent.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | shared |
| 75 | system | Task.o | `src/system/obj/Task.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | shared |
| 76 | system | TexMovie.o | `src/system/movie/TexMovie.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | shared |
| 77 | system | Text.o | `src/system/rndobj/Text.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | shared |
| 78 | network | Ticket.o | `src/network/Services/Ticket.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | cannot-provide |
| 79 | system | TrackWatcher.o | `src/system/beatmatch/TrackWatcher.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | shared |
| 80 | system | TrackWatcherImpl.o | `src/system/beatmatch/TrackWatcherImpl.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | shared |
| 81 | system | UILabel.o | `src/system/ui/UILabel.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | shared |
| 82 | system | UIListWidget.o | `src/system/ui/UIListWidget.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | shared |
| 83 | system | PatchDir.o | `src/system/bandobj/PatchDir.cpp` | 5 | 0 | 0 | 0 | 0 | 5 | 4 | shared |
| 84 | system | BaseGuitarTrackWatcherImpl.o | `src/system/beatmatch/BaseGuitarTrackWatcherImpl.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 2 | shared |
| 85 | system | DataArraySongInfo.o | `src/system/meta/DataArraySongInfo.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 2 | shared |
| 86 | system | Faders.o | `src/system/synth/Faders.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 2 | shared |
| 87 | network | Result.o | `src/network/Platform/Result.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 2 | shared |
| 88 | system | BandCharacter.o | `src/system/bandobj/BandCharacter.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 1 | shared |
| 89 | system | BandDirector.o | `src/system/bandobj/BandDirector.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 1 | shared |
| 90 | system | BandPatchMesh.o | `src/system/bandobj/BandPatchMesh.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 1 | shared |
| 91 | network | Job.o | `src/network/Core/Job.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 1 | shared |
| 92 | system | LightPresetManager.o | `src/system/world/LightPresetManager.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 1 | shared |
| 93 | network | Router.o | `src/network/Plugins/Router.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 1 | cannot-provide |
| 94 | network | Selection.o | `src/network/ObjDup/Selection.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 1 | cannot-provide |
| 95 | network | SingleThreadCallPolicy.o | `src/network/Core/SingleThreadCallPolicy.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 1 | shared |
| 96 | network | StationURL.o | `src/network/Plugins/StationURL.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 1 | cannot-provide |
| 97 | system | StoreOffer.o | `src/system/meta/StoreOffer.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 1 | shared |
| 98 | system | StorePanel.o | `src/system/meta/StorePanel.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 1 | shared |
| 99 | network | UDPTransport.o | `src/network/Plugins/UDPTransport.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 1 | cannot-provide |
| 100 | system | Utl.o | `src/system/rndobj/Utl.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 1 | shared |
| 101 | system | VocalTrackDir.o | `src/system/bandobj/VocalTrackDir.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 1 | shared |
| 102 | network | WiiFriendMgr.o | `src/network/net/WiiFriendMgr.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 1 | shared |
| 103 | system | ADSR.o | `src/system/synth/ADSR.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 104 | network | AccountManagementProtocolDDL.o | `src/network/Services/AccountManagementProtocolDDL.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 105 | system | Archive.o | `src/system/os/Archive.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 106 | system | ArpeggioShape.o | `src/system/bandobj/ArpeggioShape.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 107 | network | BackEndServices.o | `src/network/Services/BackEndServices.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 108 | system | BandCamShot.o | `src/system/bandobj/BandCamShot.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 109 | system | BandSwatch.o | `src/system/bandobj/BandSwatch.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 110 | system | BeatMatchController.o | `src/system/beatmatch/BeatMatchController.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 111 | system | BeatMatchUtl.o | `src/system/beatmatch/BeatMatchUtl.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 112 | system | BinkClip.o | `src/system/synth/BinkClip.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 113 | system | BufStreamNAND.o | `src/system/utl/BufStreamNAND.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 114 | network | CallRegister.o | `src/network/ObjDup/CallRegister.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 115 | system | CameraShot.o | `src/system/world/CameraShot.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 116 | network | ChangeMasterStationOperation.o | `src/network/ObjDup/ChangeMasterStationOperation.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 117 | system | CharBones.o | `src/system/char/CharBones.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 118 | system | CharDriver.o | `src/system/char/CharDriver.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 119 | system | CharEyeDartRuleset.o | `src/system/char/CharEyeDartRuleset.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 120 | system | CharFaceServo.o | `src/system/char/CharFaceServo.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 121 | system | CharIKHand.o | `src/system/char/CharIKHand.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 122 | network | ChecksumAlgorithm.o | `src/network/Plugins/ChecksumAlgorithm.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 123 | network | ClientProtocol.o | `src/network/Protocol/ClientProtocol.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 124 | network | CompetitionClient.o | `src/network/Services/CompetitionClient.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 125 | network | ConnectionManager.o | `src/network/Plugins/ConnectionManager.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 126 | system | ContentMgr.o | `src/system/os/ContentMgr.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 127 | system | ContentMgr_Wii.o | `src/system/os/ContentMgr_Wii.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 128 | network | DDLDeclarations.o | `src/network/ObjDup/DDLDeclarations.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 129 | network | DOClassesTable.o | `src/network/ObjDup/DOClassesTable.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 130 | network | DOCoreTypes.o | `src/network/ObjDup/DOCoreTypes.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 131 | network | DOFilters.o | `src/network/ObjDup/DOFilters.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 132 | network | DOOperation.o | `src/network/ObjDup/DOOperation.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 133 | network | DOSelections.o | `src/network/ObjDup/DOSelections.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 134 | system | DataFile.o | `src/system/obj/DataFile.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 135 | system | DataNode.o | `src/system/obj/DataNode.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 136 | system | DateTime.o | `src/system/os/DateTime.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 137 | network | DuplicationSpaceTable.o | `src/network/Extensions/DuplicationSpaceTable.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 138 | network | DynamicRunTimeInterface.o | `src/network/ObjDup/DynamicRunTimeInterface.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 139 | network | EmulationDevice.o | `src/network/Plugins/EmulationDevice.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 140 | system | EncryptXTEA.o | `src/system/utl/EncryptXTEA.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 141 | network | EncryptionAlgorithm.o | `src/network/Plugins/EncryptionAlgorithm.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 142 | network | Event.o | `src/network/Platform/Event.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 143 | system | FakeSongMgr.o | `src/system/utl/FakeSongMgr.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 144 | system | FingerShape.o | `src/system/bandobj/FingerShape.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 145 | system | FixedSizeSaveableStream.o | `src/system/meta/FixedSizeSaveableStream.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 146 | system | Flare.o | `src/system/rndobj/Flare.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 147 | system | Font.o | `src/system/rndobj/Font.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 148 | system | Fur.o | `src/system/rndobj/Fur.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 149 | system | FxSendFlanger.o | `src/system/synth/FxSendFlanger.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 150 | system | FxSendWah.o | `src/system/synth/FxSendWah.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 151 | system | GemTrackDir.o | `src/system/bandobj/GemTrackDir.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 152 | network | IOCompletionContext.o | `src/network/Platform/IOCompletionContext.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 153 | system | IntPacker.o | `src/system/utl/IntPacker.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 154 | network | InterfaceInfo.o | `src/network/Platform/InterfaceInfo.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 155 | network | JobBackEndServicesLogout.o | `src/network/Services/JobBackEndServicesLogout.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 156 | network | JobConnectStation.o | `src/network/ObjDup/JobConnectStation.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 157 | network | JobExecuteDelayedOperation.o | `src/network/ObjDup/JobExecuteDelayedOperation.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 158 | network | JobJoinSession.o | `src/network/ObjDup/JobJoinSession.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 159 | network | JobNintendoTerminate.o | `src/network/RVPackages/JobNintendoTerminate.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 160 | network | JobProcessFault.o | `src/network/ObjDup/JobProcessFault.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 161 | network | JobProcessJoinRequest.o | `src/network/ObjDup/JobProcessJoinRequest.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 162 | network | JobTicketManagerAcquireTicket.o | `src/network/Services/JobTicketManagerAcquireTicket.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 163 | network | JoinSessionOperation.o | `src/network/ObjDup/JoinSessionOperation.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 164 | system | LayerDir.o | `src/system/bandobj/LayerDir.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 165 | system | LightPreset.o | `src/system/world/LightPreset.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 166 | system | Lit.o | `src/system/rndobj/Lit.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 167 | network | LockChecker.o | `src/network/Platform/LockChecker.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 168 | system | LogFile.o | `src/system/utl/LogFile.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 169 | system | Mat.o | `src/system/rndobj/Mat.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 170 | network | MatchOperationTriggers.o | `src/network/Extensions/MatchOperationTriggers.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 171 | network | MatchmakingSettings.o | `src/network/net/MatchmakingSettings.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 172 | system | MemMgr.o | `src/system/utl/MemMgr.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 173 | system | MemPoint.o | `src/system/utl/MemPoint.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 174 | system | Mem_Wii.o | `src/system/utl/Mem_Wii.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 175 | network | Message.o | `src/network/Plugins/Message.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 176 | system | MetaMusic.o | `src/system/synth/MetaMusic.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 177 | system | MicClientMapper.o | `src/system/synth/MicClientMapper.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 178 | network | MigrationContext.o | `src/network/ObjDup/MigrationContext.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 179 | system | MoggClipMap.o | `src/system/synth/MoggClipMap.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 180 | network | NATTraversalStream.o | `src/network/Plugins/NATTraversalStream.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 181 | system | NetCacheMgr.o | `src/system/utl/NetCacheMgr.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 182 | network | NetZProductInfo.o | `src/network/Products/NetZProductInfo.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 183 | network | Network.o | `src/network/Plugins/Network.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 184 | network | NintendoClient.o | `src/network/RVPackages/NintendoClient.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 185 | system | Object.o | `src/system/obj/Object.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 186 | system | OnlineID.o | `src/system/os/OnlineID.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 187 | network | Operation.o | `src/network/Core/Operation.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 188 | system | OutfitConfig.o | `src/system/bandobj/OutfitConfig.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 189 | network | OutputFormat.o | `src/network/Platform/OutputFormat.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 190 | system | OverdriveMeter.o | `src/system/bandobj/OverdriveMeter.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 191 | network | PRUDPEndPoint.o | `src/network/Plugins/PRUDPEndPoint.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 192 | network | PacketIn.o | `src/network/Plugins/PacketIn.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 193 | system | Part.o | `src/system/rndwii/Part.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 194 | system | PatchRenderer.o | `src/system/bandobj/PatchRenderer.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 195 | system | PhraseAnalyzer.o | `src/system/beatmatch/PhraseAnalyzer.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 196 | system | PostProc.o | `src/system/rndwii/PostProc.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 197 | network | ProductInfo.o | `src/network/ProductInfo/ProductInfo.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 198 | system | ProfilePicture.o | `src/system/os/ProfilePicture.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 199 | network | ProtocolRequestBroker.o | `src/network/Protocol/ProtocolRequestBroker.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 200 | network | RC4Encryption.o | `src/network/Plugins/RC4Encryption.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 201 | network | RTT.o | `src/network/Plugins/RTT.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 202 | network | RangeDDL.o | `src/network/ObjDup/RangeDDL.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 203 | network | RootDODDL.o | `src/network/ObjDup/RootDODDL.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 204 | network | RootTransport.o | `src/network/Plugins/RootTransport.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 205 | network | RoutingAddressResolver.o | `src/network/Plugins/RoutingAddressResolver.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 206 | network | Scheduler.o | `src/network/Core/Scheduler.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 207 | system | ScrollSelect.o | `src/system/ui/ScrollSelect.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 208 | network | SessionDiscoveryTable.o | `src/network/Plugins/SessionDiscoveryTable.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 209 | network | SessionInfo.o | `src/network/ObjDup/SessionInfo.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 210 | network | SessionMessages.o | `src/network/net/SessionMessages.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 211 | network | SessionSearcher.o | `src/network/net/SessionSearcher.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 212 | network | SessionState.o | `src/network/ObjDup/SessionState.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 213 | system | SfxMap.o | `src/system/synth/SfxMap.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 214 | system | SlipTrack.o | `src/system/synth/SlipTrack.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 215 | system | SlotChannelMapping.o | `src/system/beatmatch/SlotChannelMapping.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 216 | system | SongSectionController.o | `src/system/bandobj/SongSectionController.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 217 | system | Splash.o | `src/system/movie/Splash.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 218 | system | SpotlightDrawer.o | `src/system/world/SpotlightDrawer.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 219 | network | StateMachine.o | `src/network/Core/StateMachine.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 220 | network | StationConnectionManager.o | `src/network/ObjDup/StationConnectionManager.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 221 | network | StepSequenceJob.o | `src/network/Core/StepSequenceJob.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 222 | system | Str.o | `src/system/utl/Str.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 223 | network | StreamManager.o | `src/network/Services/StreamManager.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 224 | system | Symbols.o | `src/system/utl/Symbols.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 225 | system | SynthSample.o | `src/system/synth/SynthSample.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 226 | system | TDStretch.o | `src/system/synthwii/soundtouch/TDStretch.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 227 | system | Tex.o | `src/system/rndobj/Tex.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 228 | system | TexRenderer.o | `src/system/rndobj/TexRenderer.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 229 | system | ThreadCall_Wii.o | `src/system/os/ThreadCall_Wii.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 230 | network | TicketManager.o | `src/network/Services/TicketManager.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 231 | system | TimeConversion.o | `src/system/utl/TimeConversion.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 232 | network | UDPNetworkEmulator.o | `src/network/Plugins/UDPNetworkEmulator.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 233 | system | UIListDir.o | `src/system/ui/UIListDir.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 234 | system | UIScreen.o | `src/system/ui/UIScreen.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 235 | system | UISlider.o | `src/system/ui/UISlider.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 236 | network | UpdateDataSetOperation.o | `src/network/ObjDup/UpdateDataSetOperation.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 237 | network | UpdatePolicy.o | `src/network/ObjDup/UpdatePolicy.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 238 | system | UserMgr.o | `src/system/os/UserMgr.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 239 | system | VibratoDetector.o | `src/system/dsp/VibratoDetector.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 240 | network | VoiceChannelDDL.o | `src/network/Extensions/VoiceChannelDDL.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 241 | network | VoiceChannelMemberDDL.o | `src/network/Extensions/VoiceChannelMemberDDL.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 242 | system | VorbisMem.o | `src/system/oggvorbis/VorbisMem.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 243 | network | WiiMessenger.o | `src/network/net/WiiMessenger.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 244 | system | WiiProfileMgr.o | `src/system/meta/WiiProfileMgr.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 245 | system | Wind.o | `src/system/rndobj/Wind.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 246 | network | ZLibCompression.o | `src/network/Plugins/ZLibCompression.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | shared |
| 247 | system | UIFontImporter.o | `src/system/ui/UIFontImporter.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 3 | shared |
| 248 | network | NetSession.o | `src/network/net/NetSession.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 2 | shared |
| 249 | system | BandFaceDeform.o | `src/system/bandobj/BandFaceDeform.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | shared |
| 250 | system | BandHeadShaper.o | `src/system/bandobj/BandHeadShaper.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | shared |
| 251 | system | BandWardrobe.o | `src/system/bandobj/BandWardrobe.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | shared |
| 252 | system | Bitmap.o | `src/system/rndobj/Bitmap.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | shared |
| 253 | network | ByteStream.o | `src/network/Plugins/ByteStream.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | shared |
| 254 | system | CacheMgr.o | `src/system/utl/CacheMgr.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | shared |
| 255 | system | ChordShapeGenerator.o | `src/system/bandobj/ChordShapeGenerator.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | shared |
| 256 | system | Console.o | `src/system/rndobj/Console.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | shared |
| 257 | system | CrowdMeterIcon.o | `src/system/bandobj/CrowdMeterIcon.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | shared |
| 258 | system | Dir.o | `src/system/obj/Dir.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | shared |
| 259 | system | FxSendDelay.o | `src/system/synth/FxSendDelay.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | shared |
| 260 | network | IDGenerator.o | `src/network/ObjDup/IDGenerator.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | cannot-provide |
| 261 | network | IDGeneratorDDL.o | `src/network/ObjDup/IDGeneratorDDL.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | cannot-provide |
| 262 | network | LANSessionDiscovery.o | `src/network/Plugins/LANSessionDiscovery.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | cannot-provide |
| 263 | network | ProductFacade.o | `src/network/Products/ProductFacade.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | cannot-provide |
| 264 | system | PropKeys.o | `src/system/rndobj/PropKeys.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | shared |
| 265 | network | QueuingSocket.o | `src/network/Plugins/QueuingSocket.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | cannot-provide |
| 266 | network | ServerProtocol.o | `src/network/Protocol/ServerProtocol.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | cannot-provide |
| 267 | network | SessionClockDDL.o | `src/network/Extensions/SessionClockDDL.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | cannot-provide |
| 268 | system | SongPreview.o | `src/system/meta/SongPreview.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | shared |
| 269 | system | VoiceBeat.o | `src/system/synth/VoiceBeat.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | shared |

## Per-TU function rosters

Each TU's identities, confidence-ranked. `wii_symbol` is the CW/MWCC ground-truth name (`bin/analyze-function <wii_symbol>` in the rb3 repo for the real body). Rows in the **bsim15-20** tier are confirm-on-consume.

### PlayerTrackConfigList.o — system, 10 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 10, contra 0)  ·  `src/system/beatmatch/PlayerTrackConfigList.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8230e6c0` | `0x806409a0` | bsim 10.8 | absent | BSIM | PlayerTrackConfigList::SetUseRealDrums(...) | `SetUseRealDrums__21PlayerTrackConfigListFb` |
| `0x8274a9f8` | `0x80640960` | bsim 10.2 | absent | BSIM | PlayerTrackConfigList const::GetAutoVocals(...) | `GetAutoVocals__21PlayerTrackConfigListCFv` |
| `0x8274aa00` | `0x80640970` | bsim 10.8 | absent | BSIM | PlayerTrackConfigList::SetUseVocalHarmony(...) | `SetUseVocalHarmony__21PlayerTrackConfigListFb` |
| `0x8274aa08` | `0x80640980` | bsim 10.2 | absent | BSIM | PlayerTrackConfigList const::UseVocalHarmony(...) | `UseVocalHarmony__21PlayerTrackConfigListCFv` |
| `0x8274aa18` | `0x806409b0` | bsim 10.2 | absent | BSIM | PlayerTrackConfigList const::UseRealDrums(...) | `UseRealDrums__21PlayerTrackConfigListCFv` |
| `0x8274aa20` | `0x806409c0` | bsim 10.8 | absent | BSIM | PlayerTrackConfigList::SetGameCymbalLanes(...) | `SetGameCymbalLanes__21PlayerTrackConfigListFUi` |
| `0x8274ac50` | `0x80640d30` | bsim 12.3 | absent | BSIM | PlayerTrackConfigList::TrackNumOfExactType(...) | `TrackNumOfExactType__21PlayerTrackConfigListF9TrackType` |
| `0x8274ae30` | `0x806401b0` | bsim 12.9 | absent | BSIM | PlayerTrackConfigList::ProcessConfig(...) | `ProcessConfig__21PlayerTrackConfigListFRC8UserGuid` |
| `0x8274b778` | `0x806400b0` | bsim 10.1 | absent | BSIM | PlayerTrackConfigList::AddPlaceholderConfig(...) | `AddPlaceholderConfig__21PlayerTrackConfigListFRC8UserGuidib` |
| `0x82b66f78` | `0x80640950` | bsim 10.8 | absent | BSIM | PlayerTrackConfigList::SetAutoVocals(...) | `SetAutoVocals__21PlayerTrackConfigListFb` |

### Movie.o — system, 10 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 10, contra 1)  ·  `src/system/rndobj/Movie.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82465780` | `0x808fada0` | bsim 12.3 | absent | BSIM | RndMovie::Replace(...) | `Replace__8RndMovieFPQ23Hmx6ObjectPQ23Hmx6Object` |
| `0x82514568` | `0x80788fa0` | bsim 11.0 | absent | BSIM | Movie::LockThread(...) | `LockThread__5MovieFv` |
| `0x8271e948` | `0x80788f50` | bsim 11.0 | contradicted | BSIM | Movie const::IsLoading(...) | `IsLoading__5MovieCFv` |
| `0x8271e9c0` | `0x80788fc0` | bsim 11.0 | absent | BSIM | Movie const::MsPerFrame(...) | `MsPerFrame__5MovieCFv` |
| `0x8271e9c8` | `0x80788fd0` | bsim 11.0 | absent | BSIM | Movie const::NumFrames(...) | `NumFrames__5MovieCFv` |
| `0x8271e9d0` | `0x807891f0` | bsim 11.0 | absent | BSIM | Movie::SetAspect(...) | `SetAspect__5MovieFf` |
| `0x8271f418` | `0x80788f70` | bsim 11.0 | absent | BSIM | Movie::SetPaused(...) | `SetPaused__5MovieFb` |
| `0x8271fdb8` | `0x807891c0` | bsim 11.0 | absent | BSIM | Movie::End(...) | `End__5MovieFv` |
| `0x82720020` | `0x80786080` | bsim 13.6 | absent | BSIM | Movie::Impl::Impl(...) | `__ct__Q25Movie4ImplFv` |
| `0x82721108` | `0x80788f40` | bsim 11.0 | absent | BSIM | Movie const::IsOpen(...) | `IsOpen__5MovieCFv` |

### BeatMatcher.o — system, 9 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 9, contra 2)  ·  `src/system/beatmatch/BeatMatcher.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8265a3b0` | `0x806207c0` | bsim 11.6 | absent | BSIM | BeatMatcher::SetAutoplayError(...) | `SetAutoplayError__11BeatMatcherFi` |
| `0x8276b7c0` | `0x8061ef60` | bsim 13.0 | absent | BSIM | BeatMatcher::Leave(...) | `Leave__11BeatMatcherFv` |
| `0x8276b7d0` | `0x8061f120` | bsim 10.1 | contradicted | BSIM | BeatMatcher::IsReady(...) | `IsReady__11BeatMatcherFv` |
| `0x8276b850` | `0x8061f4a0` | bsim 14.7 | absent | BSIM | BeatMatcher::ResetGemStates(...) | `ResetGemStates__11BeatMatcherFf` |
| `0x8276b9e0` | `0x8061f490` | bsim 11.6 | absent | BSIM | BeatMatcher::Restart(...) | `Restart__11BeatMatcherFv` |
| `0x8276c600` | `0x8061fcc0` | bsim 14.2 | absent | BSIM | BeatMatcher::MercurySwitch(...) | `MercurySwitch__11BeatMatcherFf` |
| `0x8276cc50` | `0x80620780` | bsim 14.3 | contradicted | BSIM | BeatMatcher const::GetTrackType(...) | `GetTrackType__11BeatMatcherCFi` |
| `0x8276ce58` | `0x80620bb0` | bsim 10.1 | absent | BSIM | BeatMatcher::EnterCoda(...) | `EnterCoda__11BeatMatcherFv` |
| `0x8276ce60` | `0x80620bc0` | bsim 10.8 | absent | BSIM | BeatMatcher::SetButtonMashingMode(...) | `SetButtonMashingMode__11BeatMatcherFb` |

### DuplicatedObject.o — network, 10 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 10, contra 4)  ·  `src/network/ObjDup/DuplicatedObject.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a418a8` | `0x80082160` | bsim 11.3 | contradicted | BSIM | Quazal::DuplicatedObject::UpdateImpl(...) | `UpdateImpl__Q26Quazal16DuplicatedObjectFPQ26Quazal7DataSetRCQ26Quazal4Time` |
| `0x82a41c58` | `0x80082560` | bsim 14.9 | absent | BSIM | Quazal::DuplicatedObject::SpecificUpdate(...) | `SpecificUpdate__Q26Quazal16DuplicatedObjectFPQ26Quazal7DataSetQ26Quazal4Time` |
| `0x82a41cb8` | `0x80082510` | bsim 14.9 | absent | BSIM | Quazal::DuplicatedObject::SpecificRefresh(...) | `SpecificRefresh__Q26Quazal16DuplicatedObjectFPQ26Quazal7DataSetRCQ26Quazal4Time` |
| `0x82a41d28` | `0x800825b0` | bsim 12.0 | absent | BSIM | Quazal::DuplicatedObject::CallApproveFaultRecovery(...) | `CallApproveFaultRecovery__Q26Quazal16DuplicatedObjectFv` |
| `0x82a42da0` | `0x800839b0` | bsim 12.8 | contradicted | BSIM | Quazal::DuplicatedObject::ExecuteOperation(...) | `ExecuteOperation__Q26Quazal16DuplicatedObjectFRQ26Quazal11DOOperation` |
| `0x82a44748` | `0x80085a50` | bsim 12.9 | contradicted | BSIM | Quazal::DuplicatedObject::PerformFaultRecovery(...) | `PerformFaultRecovery__Q26Quazal16DuplicatedObjectFQ26Quazal8DOHandleQ26Quazal20LogicalClockTmpl<Uc>` |
| `0x82a44b50` | `0x80085e90` | bsim 10.9 | absent | BSIM | Quazal::DuplicatedObject const::MigrationInProgress(...) | `MigrationInProgress__Q26Quazal16DuplicatedObjectCFv` |
| `0x82a45168` | `0x80086b30` | bsim 12.0 | absent | BSIM | Quazal::DuplicatedObject const::GetMasterID(...) | `GetMasterID__Q26Quazal16DuplicatedObjectCFv` |
| `0x82a46638` | `0x800882a0` | bsim 13.3 | contradicted | BSIM | Quazal::DuplicatedObject::ValidOperation(...) | `ValidOperation__Q26Quazal16DuplicatedObjectFPQ26Quazal11DOOperation` |
| `0x82a47740` | `0x80089570` | bsim 12.2 | absent | BSIM | Quazal::DuplicatedObject::DeletedMasterState(...) | `DeletedMasterState__Q26Quazal16DuplicatedObjectFRCQ36Quazal12StateMachine6QEvent` |

### Rnd.o — system, 7 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 7, contra 1)  ·  `src/system/rndobj/Rnd.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x824589d8` | `0x80935c70` | bsim 10.2 | absent | BSIM | Rnd const::InGame(...) | `InGame__3RndCFv` |
| `0x82508f90` | `0x809f3530` | bsim 10.1 | absent | BSIM | WiiModal(...)   [free function] | `WiiModal__FRbPcb` |
| `0x825864f8` | `0x80936090` | bsim 10.2 | absent | BSIM | Rnd const::GetEvenOddDisabled(...) | `GetEvenOddDisabled__3RndCFv` |
| `0x825bed28` | `0x80936050` | bsim 10.2 | contradicted | BSIM | Rnd const::ProcAndLock(...) | `ProcAndLock__3RndCFv` |
| `0x826d1f20` | `0x80935c30` | bsim 10.2 | absent | BSIM | RndDrawable::GetForceSubpartSelection(...) | `GetForceSubpartSelection__11RndDrawableFv` |
| `0x8276bc20` | `0x80935c40` | bsim 10.8 | absent | BSIM | RndDrawable::SetForceSubpartSelection(...) | `SetForceSubpartSelection__11RndDrawableFb` |
| `0x82b533e0` | `0x8092f350` | bsim 11.6 | absent | BSIM | Rnd::ShowConsole(...) | `ShowConsole__3RndFb` |

### Buffer.o — network, 6 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 6, contra 1)  ·  `src/network/Plugins/Buffer.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a798f8` | `0x8003e1b0` | bsim 12.7 | absent | BSIM | Quazal::Buffer const::CopyContent(...) | `CopyContent__Q26Quazal6BufferCFPvUiUi` |
| `0x82a799a0` | `0x8003e210` | bsim 10.5 | absent | BSIM | Quazal::Buffer::Clear(...) | `Clear__Q26Quazal6BufferFv` |
| `0x82a79af8` | `0x8003e3f0` | bsim 10.5 | confirmed | BSIM | __apl__Q26Quazal6BufferFRCQ26Quazal6Buffer | `__apl__Q26Quazal6BufferFRCQ26Quazal6Buffer` |
| `0x82a79bf8` | `0x8003e480` | bsim 10.0 | contradicted | BSIM | Quazal::Buffer const::GetContentPtr(...) | `GetContentPtr__Q26Quazal6BufferCFv` |
| `0x82a79c40` | `0x8003e4b0` | bsim 10.8 | absent | BSIM | Quazal::Buffer::SetContentSize(...) | `SetContentSize__Q26Quazal6BufferFUi` |
| `0x82a79c58` | `0x8003e4c0` | bsim 10.0 | absent | BSIM | Quazal::Buffer const::GetSize(...) | `GetSize__Q26Quazal6BufferCFv` |

### Station.o — network, 6 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 6, contra 1)  ·  `src/network/ObjDup/Station.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a4df60` | `0x800b0ce0` | bsim 14.2 | absent | BSIM | Quazal::Station::ReleaseConnection(...) | `ReleaseConnection__Q26Quazal7StationFv` |
| `0x82a4e108` | `0x800b0e70` | bsim 12.1 | contradicted | BSIM | Quazal::Station::OperationEnd(...) | `OperationEnd__Q26Quazal7StationFPQ26Quazal11DOOperation` |
| `0x82a4e698` | `0x800b14d0` | bsim 10.6 | absent | BSIM | Quazal::Station::InitIdentification(...) | `InitIdentification__Q26Quazal7StationFPQ26Quazal21StationIdentification` |
| `0x82a4e820` | `0x800b15e0` | bsim 10.9 | absent | BSIM | Quazal::Station::ConvertDOHandleToID(...) | `ConvertDOHandleToID__Q26Quazal7StationFQ26Quazal8DOHandle` |
| `0x82a4ea18` | `0x800b16e0` | bsim 10.0 | absent | BSIM | Quazal::Station const::GetProcessType(...) | `GetProcessType__Q26Quazal7StationCFv` |
| `0x82a4f2e0` | `0x800b22b0` | bsim 12.1 | absent | BSIM | Quazal::Station::SignalFault(...) | `SignalFault__Q26Quazal7StationFb` |

### SongData.o — system, 5 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 5, contra 0)  ·  `src/system/beatmatch/SongData.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8274b8b0` | `0x806528c0` | bsim 11.6 | absent | BSIM | SongData const::GetUsingRealDrums(...) | `GetUsingRealDrums__8SongDataCFv` |
| `0x8274ca00` | `0x806524f0` | bsim 10.3 | absent | BSIM | SongData::RecalculateGemTimes(...) | `RecalculateGemTimes__8SongDataFi` |
| `0x8274cbc0` | `0x806528a0` | bsim 14.3 | absent | BSIM | SongData::GetDrumFillInfo(...) | `GetDrumFillInfo__8SongDataFi` |
| `0x8274cd38` | `0x80652cf0` | bsim 10.6 | absent | BSIM | SongData const::GetAudioTrackNum(...) | `GetAudioTrackNum__8SongDataCFi` |
| `0x82750730` | `0x8064b850` | bsim 11.3 | absent | BSIM | SongData::UpdatePlayerTrackConfigList(...) | `UpdatePlayerTrackConfigList__8SongDataFP21PlayerTrackConfigList` |

### UIListState.o — system, 5 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 5, contra 0)  ·  `src/system/ui/UIListState.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x825864f0` | `0x807e6820` | bsim 10.2 | absent | BSIM | UIListState const::ScrollPastMinDisplay(...) | `ScrollPastMinDisplay__11UIListStateCFv` |
| `0x82785700` | `0x807e6830` | bsim 10.2 | absent | BSIM | UIListState const::ScrollPastMaxDisplay(...) | `ScrollPastMaxDisplay__11UIListStateCFv` |
| `0x827e8a48` | `0x807e7570` | bsim 10.8 | absent | BSIM | UIListState::SetScrollPastMaxDisplay(...) | `SetScrollPastMaxDisplay__11UIListStateFb` |
| `0x827e91d0` | `0x807e74a0` | bsim 10.8 | absent | BSIM | UIListState::SetCircular(...) | `SetCircular__11UIListStateFbb` |
| `0x82b5d158` | `0x807e69b0` | bsim 11.6 | absent | BSIM | UIListState const::StepPercent(...) | `StepPercent__11UIListStateCFv` |

### BandCharDesc.o — system, 4 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 4, contra 0)  ·  `src/system/bandobj/BandCharDesc.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82321cc0` | `0x80552950` | bsim 13.8 | absent | BSIM | BandCharDesc::SetChanged(...) | `SetChanged__12BandCharDescFi` |
| `0x82329df8` | `0x80553930` | bsim 11.7 | absent | BSIM | BandCharDesc::Save(...) | `Save__12BandCharDescFR9BinStream` |
| `0x8249d810` | `0x805514c0` | bsim 12.8 | absent | BSIM | BandCharDesc::Patch const::SaveFixed(...) | `SaveFixed__Q212BandCharDesc5PatchCFR23FixedSizeSaveableStream` |
| `0x82705f18` | `0x80552f80` | bsim 12.0 | absent | BSIM | __ls__FR9BinStreamRCQ212BandCharDesc5Patch | `__ls__FR9BinStreamRCQ212BandCharDesc5Patch` |

### CallMethodOperation.o — network, 4 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 4, contra 0)  ·  `src/network/ObjDup/CallMethodOperation.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a6fde8` | `0x80072ad0` | bsim 12.0 | absent | BSIM | Quazal::CallMethodOperation::DispatchCall(...) | `DispatchCall__Q26Quazal19CallMethodOperationFv` |
| `0x82a70418` | `0x80073060` | bsim 10.2 | absent | BSIM | Quazal::CallMethodOperation const::OperationIsPostponed(...) | `OperationIsPostponed__Q26Quazal19CallMethodOperationCFv` |
| `0x82a70428` | `0x80073070` | bsim 10.0 | absent | BSIM | Quazal::CallMethodOperation::GetAttemptCount(...) | `GetAttemptCount__Q26Quazal19CallMethodOperationFv` |
| `0x82a78458` | `0x80072d90` | bsim 10.0 | absent | BSIM | Quazal::CallMethodOperation const::GetCallMessage(...) | `GetCallMessage__Q26Quazal19CallMethodOperationCFv` |

### DOCallContext.o — network, 4 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 4, contra 0)  ·  `src/network/ObjDup/DOCallContext.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a6c3e0` | `0x80077f00` | bsim 11.9 | absent | BSIM | Quazal::DOCallContext::SignalFailure(...) | `SignalFailure__Q26Quazal13DOCallContextFQ36Quazal13DOCallContext8_Outcome` |
| `0x82a725c0` | `0x800776b0` | bsim 10.3 | absent | BSIM | Quazal::DOCallContext::SetTargetStation(...) | `SetTargetStation__Q26Quazal13DOCallContextFQ26Quazal8DOHandle` |
| `0x82a72880` | `0x800778f0` | bsim 10.1 | absent | BSIM | Quazal::DOCallContext const::Wait(...) | `Wait__Q26Quazal13DOCallContextCFUi` |
| `0x82a73020` | `0x80078090` | bsim 10.0 | absent | BSIM | Quazal::DOCallContext::GetOutcome(...) | `GetOutcome__Q26Quazal13DOCallContextFv` |

### GameGem.o — system, 4 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 4, contra 0)  ·  `src/system/beatmatch/GameGem.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82769c50` | `0x80627560` | bsim 11.8 | absent | BSIM | GameGem const::PlayableBy(...) | `PlayableBy__7GameGemCFi` |
| `0x82769c58` | `0x80627570` | bsim 10.4 | absent | BSIM | GameGem::Flip(...) | `Flip__7GameGemFRC7GameGem` |
| `0x82769e88` | `0x80627d70` | bsim 10.2 | absent | BSIM | GameGem const::GetRootNote(...) | `GetRootNote__7GameGemCFv` |
| `0x82b51590` | `0x80627c10` | bsim 10.2 | absent | BSIM | GameGem const::GetImportantStrings(...) | `GetImportantStrings__7GameGemCFv` |

### SongParser.o — system, 4 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 4, contra 0)  ·  `src/system/beatmatch/SongParser.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8275df78` | `0x8065e1c0` | bsim 12.3 | absent | BSIM | SongParser::AudioTrackUsed(...) | `AudioTrackUsed__10SongParserF17SongInfoAudioType` |
| `0x8275dfc0` | `0x8065eae0` | bsim 13.0 | absent | BSIM | SongParser::SetSectionBounds(...) | `SetSectionBounds__10SongParserFii` |
| `0x8275e028` | `0x8065ec50` | bsim 14.9 | absent | BSIM | SongParser::HandleRGAreaStrumStart(...) | `HandleRGAreaStrumStart__10SongParserFiRQ210SongParser14DifficultyInfoUcUc` |
| `0x8275fcc8` | `0x80660f30` | bsim 12.5 | absent | BSIM | FillTrackList(...)   [free function] | `FillTrackList__FRQ211stlpmtx_std57vector<6Symbol,Us,Q211stlpmtx_std21StlNodeAlloc<6Symbol>>R9BinStream` |

### CameraManager.o — system, 4 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 4, contra 1)  ·  `src/system/world/CameraManager.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x823d7b98` | `0x808034e0` | bsim 11.1 | contradicted | BSIM | stlpmtx_std::_Vector_base<Q213CameraManager14PropertyFilter,Us,Q211stlpmtx_std47StlNodeAlloc<Q213CameraManager14PropertyFilter>> const::_M_throw_length_error(...) | `_M_throw_length_error__Q211stlpmtx_std115_Vector_base<Q213CameraManager14PropertyFilter,Us,Q211stlpmtx_std47StlNodeAlloc<Q213CameraManager14PropertyFilter>>CFv` |
| `0x824a6ec8` | `0x80801b80` | bsim 11.6 | absent | BSIM | CameraManager::Enter(...) | `Enter__13CameraManagerFv` |
| `0x82ad40b8` | `0x807fe120` | bsim 10.6 | absent | BSIM | stlpmtx_std::sort<PQ213CameraManager8Category>(...) | `sort<PQ213CameraManager8Category>__11stlpmtx_stdFPQ213CameraManager8CategoryPQ213CameraManager8Category_v` |
| `0x82ad4970` | `0x807fe260` | bsim 11.5 | absent | BSIM | __unguarded_insertion_sort_aux<PQ213CameraManager8Category,Q213CameraManager8Category,Q211stlpmtx_std32less<Q213CameraManager8Category>>__11stlpmtx_stdFPQ213CameraManager8CategoryPQ213CameraManager8CategoryPQ213CameraManager8CategoryQ211stlpmtx_std32less<Q213CameraManager8Category>_v | `__unguarded_insertion_sort_aux<PQ213CameraManager8Category,Q213CameraManager8Category,Q211stlpmtx_std32less<Q213CameraManager8Category>>__11stlpmtx_stdFPQ213CameraManager8CategoryPQ213CameraManager8CategoryPQ213CameraManager8CategoryQ211stlpmtx_std32less<Q213CameraManager8Category>_v` |

### MasterAudio.o — system, 4 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 4, contra 1)  ·  `src/system/beatmatch/MasterAudio.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82756fd8` | `0x80639be0` | bsim 11.6 | absent | BSIM | ChannelData::SetFaderVal(...) | `SetFaderVal__11ChannelDataFf` |
| `0x82756fe0` | `0x80637c00` | bsim 11.6 | absent | BSIM | MasterAudio::SetForegroundVolume(...) | `SetForegroundVolume__11MasterAudioFf` |
| `0x82756fe8` | `0x80637e00` | bsim 10.2 | absent | BSIM | MasterAudio::SetVocalDuckFader(...) | `SetVocalDuckFader__11MasterAudioFf` |
| `0x827570a0` | `0x80638170` | bsim 12.4 | contradicted | BSIM | MasterAudio::SetTimeOffset(...) | `SetTimeOffset__11MasterAudioFf` |

### MoggClip.o — system, 4 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 4, contra 1)  ·  `src/system/synth/MoggClip.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x823b76a8` | `0x809a2ae0` | bsim 10.7 | contradicted | BSIM | MoggClip::Copy(...) | `Copy__8MoggClipFPCQ23Hmx6ObjectQ33Hmx6Object8CopyType` |
| `0x82572868` | `0x809a34e0` | bsim 10.7 | absent | BSIM | MoggClip::KillStream(...) | `KillStream__8MoggClipFv` |
| `0x826d3888` | `0x809a3070` | bsim 10.8 | absent | BSIM | MoggClip::SetLoopEnd(...) | `SetLoopEnd__8MoggClipFi` |
| `0x8276bcb0` | `0x809a3060` | bsim 10.8 | absent | BSIM | MoggClip::SetLoopStart(...) | `SetLoopStart__8MoggClipFi` |

### ObjDupProtocol.o — network, 4 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 4, contra 1)  ·  `src/network/ObjDup/ObjDupProtocol.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a65820` | `0x8009f030` | bsim 11.6 | contradicted | BSIM | Quazal::ObjDupProtocol::FaultDetection(...) | `FaultDetection__Q26Quazal14ObjDupProtocolFPQ26Quazal8EndPointUi` |
| `0x82a65cf8` | `0x8009f480` | bsim 13.6 | absent | BSIM | Quazal::ObjDupProtocol::QueueMessageFromLocalStation(...) | `QueueMessageFromLocalStation__Q26Quazal14ObjDupProtocolFPQ26Quazal7Message` |
| `0x82a67838` | `0x800a0720` | bsim 13.1 | absent | BSIM | Quazal::ObjDupProtocol::ProcessRMCCallMessage(...) | `ProcessRMCCallMessage__Q26Quazal14ObjDupProtocolFPQ26Quazal7MessageRUsRQ26Quazal8DOHandleRUiRQ26Quazal8DOHandleRUs` |
| `0x82a691e0` | `0x800a2570` | bsim 10.2 | absent | BSIM | Quazal::ObjDupProtocol const::IsListeningOnWellKnown(...) | `IsListeningOnWellKnown__Q26Quazal14ObjDupProtocolCFv` |

### UIList.o — system, 4 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 4, contra 1)  ·  `src/system/ui/UIList.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82389bf0` | `0x807d96c0` | bsim 10.8 | contradicted | BSIM | UIList::SetDrawManuallyControlledWidgets(...) | `SetDrawManuallyControlledWidgets__6UIListFb` |
| `0x827d2988` | `0x807d4ec0` | bsim 10.0 | absent | BSIM | UIList const::NumDisplay(...) | `NumDisplay__6UIListCFv` |
| `0x827d2990` | `0x807d4ee0` | bsim 10.0 | absent | BSIM | UIList const::FirstShowing(...) | `FirstShowing__6UIListCFv` |
| `0x827d2a10` | `0x807d5080` | bsim 10.0 | absent | BSIM | UIList const::GetUIListDir(...) | `GetUIListDir__6UIListCFv` |

### BandCrowdMeter.o — system, 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 0)  ·  `src/system/bandobj/BandCrowdMeter.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x822a9e18` | `0x80505de0` | bsim 11.4 | absent | BSIM | BandCrowdMeter const::Disabled(...) | `Disabled__14BandCrowdMeterCFv` |
| `0x822aac18` | `0x80505d10` | bsim 14.8 | absent | BSIM | BandCrowdMeter const::Draining(...) | `Draining__14BandCrowdMeterCFv` |
| `0x822aac80` | `0x80505d70` | bsim 14.8 | absent | BSIM | BandCrowdMeter const::Deploying(...) | `Deploying__14BandCrowdMeterCFv` |

### BerkeleySocketDriver.o — network, 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 0)  ·  `src/network/Platform/BerkeleySocketDriver.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b0eb80` | `0x80028030` | bsim 14.5 | absent | BSIM | Quazal::BerkeleySocketDriver::BerkeleySocket::Bind(...) | `Bind__Q36Quazal20BerkeleySocketDriver14BerkeleySocketFUs` |
| `0x82b0f3e0` | `0x800285f0` | bsim 11.7 | absent | BSIM | Quazal::BerkeleySocketDriver::BerkeleySocket::Send(...) | `Send__Q36Quazal20BerkeleySocketDriver14BerkeleySocketFPUcUiPUi` |
| `0x82b0f568` | `0x80028700` | bsim 13.2 | absent | BSIM | Quazal::BerkeleySocketDriver::BerkeleySocket::Close(...) | `Close__Q36Quazal20BerkeleySocketDriver14BerkeleySocketFv` |

### CallContext.o — network, 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 0)  ·  `src/network/Core/CallContext.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a5d098` | `0x8002a920` | bsim 13.0 | absent | BSIM | Quazal::CallContext::SetDependentConnection(...) | `SetDependentConnection__Q26Quazal11CallContextFPvUi` |
| `0x82a5e170` | `0x8002bce0` | bsim 13.8 | absent | BSIM | Quazal::CallContext::SetFlag(...) | `SetFlag__Q26Quazal11CallContextFUi` |
| `0x82a5e220` | `0x8002bd30` | bsim 13.1 | absent | BSIM | Quazal::CallContext const::FlagIsSet(...) | `FlagIsSet__Q26Quazal11CallContextCFUi` |

### Credentials.o — network, 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 0)  ·  `src/network/Services/Credentials.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aafe10` | `0x800dcfd0` | bsim 14.2 | absent | BSIM | Quazal::Credentials::Credentials(...) | `__ct__Q26Quazal11CredentialsFUiPQ26Quazal13StreamManager` |
| `0x82ab0080` | `0x800dd1c0` | bsim 10.8 | absent | BSIM | Quazal::Credentials::SetAuthenticationConnection(...) | `SetAuthenticationConnection__Q26Quazal11CredentialsFPQ26Quazal8EndPoint` |
| `0x82ab0098` | `0x800dd1e0` | bsim 10.8 | absent | BSIM | Quazal::Credentials::SetSpecialSecureConnection(...) | `SetSpecialSecureConnection__Q26Quazal11CredentialsFPQ26Quazal8EndPoint` |

### DOClass.o — network, 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 0)  ·  `src/network/ObjDup/DOClass.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a6bb00` | `0x800783b0` | bsim 10.9 | absent | BSIM | Quazal::DOClass::CompleteInitialisation(...) | `CompleteInitialisation__Q26Quazal7DOClassFv` |
| `0x82a6bb58` | `0x800783f0` | bsim 10.9 | absent | BSIM | Quazal::DOClass::PrepareToLeave(...) | `PrepareToLeave__Q26Quazal7DOClassFv` |
| `0x82a6c0b0` | `0x80078a60` | bsim 12.3 | absent | BSIM | Quazal::DOClass::CreateIDGenerator(...) | `CreateIDGenerator__Q26Quazal7DOClassFv` |

### DOHandle.o — network, 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 0)  ·  `src/network/ObjDup/DOHandle.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a6a808` | `0x8007c6a0` | bsim 14.9 | absent | BSIM | Quazal::DOHandle::SetDOID(...) | `SetDOID__Q26Quazal8DOHandleFQ26Quazal4DOID` |
| `0x82a6a8c8` | `0x8007c700` | bsim 11.5 | absent | BSIM | Quazal::DOHandle const::IsAWKHandle(...) | `IsAWKHandle__Q26Quazal8DOHandleCFv` |
| `0x82a6ab10` | `0x8007c990` | bsim 12.6 | absent | BSIM | Quazal::DOHandle::IsA(...) | `IsA__Q26Quazal8DOHandleFUi` |

### EndPoint.o — network, 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 0)  ·  `src/network/Plugins/EndPoint.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82ac8710` | `0x80045720` | bsim 12.9 | absent | BSIM | Quazal::EndPoint::Open(...) | `Open__Q26Quazal8EndPointFv` |
| `0x82ac8760` | `0x80045740` | bsim 13.1 | absent | BSIM | Quazal::EndPoint::_Open(...) | `_Open__Q26Quazal8EndPointFv` |
| `0x82ac8780` | `0x80045750` | bsim 12.9 | absent | BSIM | Quazal::EndPoint::Close(...) | `Close__Q26Quazal8EndPointFv` |

### HMACChecksum.o — network, 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 0)  ·  `src/network/Plugins/HMACChecksum.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b1b1b8` | `0x80039660` | bsim 11.7 | confirmed | BSIM | Quazal::HMACChecksum::HMACChecksum(...) | `__ct__Q26Quazal12HMACChecksumFv` |
| `0x82b1b238` | `0x80039710` | bsim 10.5 | confirmed | BSIM | Quazal::HMACChecksum::KeyHasChanged(...) | `KeyHasChanged__Q26Quazal12HMACChecksumFv` |
| `0x82b1b380` | `0x800398b0` | bsim 14.4 | confirmed | BSIM | Quazal::HMACChecksum::ComputeChecksum(...) | `ComputeChecksum__Q26Quazal12HMACChecksumFRCQ26Quazal6BufferPQ26Quazal6Buffer` |

### JsonUtils.o — network, 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 0)  ·  `src/network/net/JsonUtils.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8254df30` | `0x800fd0d0` | bsim 11.6 | absent | BSIM | JsonConverter::NewArray(...) | `NewArray__13JsonConverterFv` |
| `0x82b53548` | `0x800fcba0` | bsim 11.6 | absent | BSIM | JsonObject const::GetObjectAsString(...) | `GetObjectAsString__10JsonObjectCFv` |
| `0x82b535f0` | `0x800fcf70` | bsim 11.6 | absent | BSIM | JsonDouble const::GetValue(...) | `GetValue__10JsonDoubleCFv` |

### KeyboardTrackWatcherImpl.o — system, 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 0)  ·  `src/system/beatmatch/KeyboardTrackWatcherImpl.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8277ae18` | `0x80634910` | bsim 11.4 | absent | BSIM | KeyboardTrackWatcherImpl::FindFatFingerDataForSlot(...) | `FindFatFingerDataForSlot__24KeyboardTrackWatcherImplFi` |
| `0x8277aec8` | `0x806349f0` | bsim 10.1 | absent | BSIM | KeyboardTrackWatcherImpl::GetFatFingerGem(...) | `GetFatFingerGem__24KeyboardTrackWatcherImplFf` |
| `0x8277b2e0` | `0x80634540` | bsim 13.3 | absent | BSIM | KeyboardTrackWatcherImpl::OnPass(...) | `OnPass__24KeyboardTrackWatcherImplFfi` |

### MessageBrokerDDL_Wii.o — network, 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 0)  ·  `src/network/net/MessageBrokerDDL_Wii.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x823e1420` | `0x8011a4c0` | bsim 10.5 | absent | BSIM | Quazal::_DO_MessageBroker::CreateWellKnown(...) | `CreateWellKnown__Q26Quazal17_DO_MessageBrokerFRQ26Quazal8WKHandle` |
| `0x823e1698` | `0x8011a330` | bsim 11.2 | absent | BSIM | Quazal::_DOC_MessageBroker::DispatchRMCCall(...) | `DispatchRMCCall__Q26Quazal18_DOC_MessageBrokerFRCQ26Quazal19CallMethodOperation` |
| `0x823e1768` | `0x8011a410` | bsim 11.2 | absent | BSIM | Quazal::_DO_MessageBroker::_DO_MessageBroker(...) | `__ct__Q26Quazal17_DO_MessageBrokerFv` |

### NetLoader.o — system, 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 0)  ·  `src/system/utl/NetLoader.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827aa8f0` | `0x804af410` | bsim 10.2 | absent | BSIM | NetLoader::IsLoaded(...) | `IsLoaded__9NetLoaderFv` |
| `0x827aadd8` | `0x804af890` | bsim 10.4 | absent | BSIM | DataNetLoader::DataNetLoader(...) | `__ct__13DataNetLoaderFRC6String` |
| `0x827b51b8` | `0x804af430` | bsim 10.0 | absent | BSIM | NetLoader::GetSize(...) | `GetSize__9NetLoaderFv` |

### Protocol.o — network, 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 0)  ·  `src/network/Protocol/Protocol.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a77f08` | `0x8006d5b0` | bsim 10.8 | absent | BSIM | Quazal::Protocol::SetProtocolID(...) | `SetProtocolID__Q26Quazal8ProtocolFUc` |
| `0x82a77f20` | `0x8006d5e0` | bsim 13.1 | absent | BSIM | Quazal::Protocol const::FlagIsSet(...) | `FlagIsSet__Q26Quazal8ProtocolCFUi` |
| `0x82a77f50` | `0x8006d600` | bsim 13.8 | absent | BSIM | Quazal::Protocol::SetFlag(...) | `SetFlag__Q26Quazal8ProtocolFUi` |

### SecureStream.o — network, 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 0)  ·  `src/network/Services/SecureStream.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aaf2d0` | `0x800f6c10` | bsim 11.7 | absent | BSIM | Quazal::SecureStream::SecureStream(...) | `__ct__Q26Quazal12SecureStreamFv` |
| `0x82aaf570` | `0x800f6d60` | bsim 10.8 | absent | BSIM | Quazal::SecureStream::SetAssociatedStream(...) | `SetAssociatedStream__Q26Quazal12SecureStreamFPQ26Quazal24ConnectionOrientedStream` |
| `0x82aafa70` | `0x800f7040` | bsim 13.1 | absent | BSIM | Quazal::SecureStream::FilterIncomingConnection(...) | `FilterIncomingConnection__Q26Quazal12SecureStreamFPQ26Quazal6BufferPQ26Quazal6BufferPQ26Quazal8EndPoint` |

### TrackDir.o — system, 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 0)  ·  `src/system/track/TrackDir.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827b7cb0` | `0x80794010` | bsim 14.5 | absent | BSIM | TrackDir const::TopSeconds(...) | `TopSeconds__8TrackDirCFv` |
| `0x827b7cc0` | `0x80794020` | bsim 14.5 | absent | BSIM | TrackDir const::BottomSeconds(...) | `BottomSeconds__8TrackDirCFv` |
| `0x827b7cf0` | `0x80794580` | bsim 14.7 | absent | BSIM | TrackDir::SetScrollSpeed(...) | `SetScrollSpeed__8TrackDirFf` |

### UI.o — system, 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 0)  ·  `src/system/ui/UI.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8257c800` | `0x807b7bf0` | bsim 10.2 | absent | BSIM | UIManager const::WentBack(...) | `WentBack__9UIManagerCFv` |
| `0x8274aa10` | `0x807b6d90` | bsim 10.8 | absent | BSIM | UIManager::SetRequireFixedText(...) | `SetRequireFixedText__9UIManagerFb` |
| `0x827ddbd8` | `0x807b6da0` | bsim 11.2 | absent | BSIM | UIManager::BottomScreen(...) | `BottomScreen__9UIManagerFv` |

### UILabelDir.o — system, 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 0)  ·  `src/system/ui/UILabelDir.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827ea8e0` | `0x807d00c0` | bsim 12.1 | absent | BSIM | UILabelDir::SyncObjects(...) | `SyncObjects__10UILabelDirFv` |
| `0x827ea950` | `0x807cff60` | bsim 10.0 | absent | BSIM | UILabelDir const::HighlighMeshGroup(...) | `HighlighMeshGroup__10UILabelDirCFv` |
| `0x827eaa10` | `0x807cfdf0` | bsim 13.9 | absent | BSIM | UILabelDir const::GetStateColor(...) | `GetStateColor__10UILabelDirCFQ211UIComponent5StateRQ23Hmx5Color` |

### MidiInstrument.o — system, 4 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 4, contra 2)  ·  `src/system/synth/MidiInstrument.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x822b0e38` | `0x8099c060` | bsim 11.6 | absent | BSIM | NoteVoiceInst::SetSend(...) | `SetSend__13NoteVoiceInstFP6FxSend` |
| `0x826f5650` | `0x8099bde0` | bsim 11.1 | absent | BSIM | NoteVoiceInst::SetFineTune(...) | `SetFineTune__13NoteVoiceInstFf` |
| `0x826f5898` | `0x8099bb10` | bsim 11.7 | contradicted | BSIM | NoteVoiceInst::NoteVoiceInst(...) | `__ct__13NoteVoiceInstFP14MidiInstrumentP10SampleZoneUcUciif` |
| `0x826f8158` | `0x8099db80` | bsim 12.4 | contradicted | BSIM | MidiInstrument::Load(...) | `Load__14MidiInstrumentFR9BinStream` |

### BandTrack.o — system, 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 1)  ·  `src/system/bandobj/BandTrack.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8233a950` | `0x805f59a0` | bsim 10.8 | absent | BSIM | BandTrack::SetSuppressSoloDisplay(...) | `SetSuppressSoloDisplay__9BandTrackFb` |
| `0x8233a9c8` | `0x805f1350` | bsim 12.5 | contradicted | BSIM | BandTrack::ResetStreakMeter(...) | `ResetStreakMeter__9BandTrackFv` |
| `0x8233c5c8` | `0x805f2550` | bsim 12.2 | absent | BSIM | BandTrack::SpotlightFail(...) | `SpotlightFail__9BandTrackFb` |

### DirLoader.o — system, 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 1)  ·  `src/system/obj/DirLoader.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827324c8` | `0x804701f0` | bsim 10.3 | absent | BSIM | DirLoader::LoadHeader(...) | `LoadHeader__9DirLoaderFv` |
| `0x82732ec0` | `0x8046f1e0` | bsim 11.0 | contradicted | BSIM | DirLoader::LoadObjects(...) | `LoadObjects__9DirLoaderFRC8FilePathPQ26Loader8CallbackP9BinStream` |
| `0x82a75768` | `0x8046f780` | bsim 12.6 | absent | BSIM | DirLoader const::StateName(...) | `StateName__9DirLoaderCFv` |

### DuplicationSpace.o — network, 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 1)  ·  `src/network/Extensions/DuplicationSpace.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82af8e08` | `0x800be6f0` | bsim 11.9 | absent | BSIM | Quazal::DuplicationSpace::AssignID(...) | `AssignID__Q26Quazal16DuplicationSpaceFv` |
| `0x82af8f40` | `0x800be770` | bsim 14.3 | contradicted | BSIM | Quazal::DuplicationSpace::NoCellRequired(...) | `NoCellRequired__Q26Quazal16DuplicationSpaceFv` |
| `0x82afb470` | `0x800c1ec0` | bsim 12.4 | absent | BSIM | Quazal::DuplicationSpace::ClearAllDOClassRoles(...) | `ClearAllDOClassRoles__Q26Quazal16DuplicationSpaceFv` |

### StoreArtLoaderPanel.o — system, 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 1)  ·  `src/system/meta/StoreArtLoaderPanel.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826260f8` | `0x80760e60` | bsim 13.0 | absent | BSIM | StoreArtLoaderPanel::Unload(...) | `Unload__19StoreArtLoaderPanelFv` |
| `0x82792128` | `0x80760c90` | bsim 11.1 | contradicted | BSIM | StoreArtLoaderPanel::Poll(...) | `Poll__19StoreArtLoaderPanelFv` |
| `0x82a4fd08` | `0x80760e20` | bsim 11.6 | absent | BSIM | StoreArtLoaderPanel::Load(...) | `Load__19StoreArtLoaderPanelFv` |

### Anim.o — system, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/system/rndobj/Anim.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x823eda50` | `0x8087a160` | bsim 10.2 | absent | BSIM | RndAnimatable::ConvertFrames(...) | `ConvertFrames__13RndAnimatableFRf` |
| `0x823ee668` | `0x8087c2c0` | bsim 14.4 | absent | BSIM | AnimTask::AnimTask(...) | `__ct__8AnimTaskFP13RndAnimatablefffbf` |

### BundlingPolicy.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/network/ObjDup/BundlingPolicy.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aa11d0` | `0x80072670` | bsim 11.1 | absent | BSIM | Quazal::BundlingPolicy::BundlingPolicy(...) | `__ct__Q26Quazal14BundlingPolicyFv` |
| `0x82aa1210` | `0x800726e0` | bsim 12.6 | absent | BSIM | Quazal::BundlingPolicy const::FlagIsSet(...) | `FlagIsSet__Q26Quazal14BundlingPolicyCFUi` |

### ChangeDupSetOperation.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/network/ObjDup/ChangeDupSetOperation.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a725a8` | `0x80076330` | bsim 10.8 | absent | BSIM | Quazal::ChangeDupSetOperation::AttachMigrationContext(...) | `AttachMigrationContext__Q26Quazal21ChangeDupSetOperationFUs` |
| `0x82a90310` | `0x80076220` | bsim 15.0 | absent | BSIM | Quazal::ChangeDupSetOperation::ChangeDupSetOperation(...) | `__ct__Q26Quazal21ChangeDupSetOperationFQ26Quazal8DOHandlePQ26Quazal16DuplicatedObjectQ26Quazal8DOHandlebQ36Quazal21ChangeDupSetOperation7Context` |

### CharMeshHide.o — system, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/system/char/CharMeshHide.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8237b970` | `0x80707ba0` | bsim 10.2 | absent | BSIM | CharMeshHide::Hide::Hide(...) | `__ct__Q212CharMeshHide4HideFRCQ212CharMeshHide4Hide` |
| `0x8238e3c8` | `0x80707df0` | bsim 11.5 | absent | BSIM | CharMeshHide::HideDraws(...) | `HideDraws__12CharMeshHideFi` |

### Character.o — system, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/system/char/Character.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8238fd88` | `0x80674940` | bsim 11.6 | absent | BSIM | Sphere const::GetRadius(...) | `GetRadius__6SphereCFv` |
| `0x824589d0` | `0x8067ad00` | bsim 10.8 | absent | BSIM | Character::SetDebugDrawInterestObjects(...) | `SetDebugDrawInterestObjects__9CharacterFb` |

### ConnectionOrientedStream.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/network/Plugins/ConnectionOrientedStream.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aeeb38` | `0x80044d30` | bsim 11.0 | absent | BSIM | Quazal::ConnectionOrientedStream::ConnectionOrientedStream(...) | `__ct__Q26Quazal24ConnectionOrientedStreamFQ36Quazal6Stream4TypePQ26Quazal13RootTransport` |
| `0x82aeecd8` | `0x80044e20` | bsim 12.4 | absent | BSIM | Quazal::ConnectionOrientedStream::StartListen(...) | `StartListen__Q26Quazal24ConnectionOrientedStreamFUcPUc` |

### FaultProcessingContext.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/network/ObjDup/FaultProcessingContext.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82adba80` | `0x8008b900` | bsim 13.1 | absent | BSIM | Quazal::FaultProcessingContext::PollElectionResult(...) | `PollElectionResult__Q26Quazal22FaultProcessingContextFv` |
| `0x82adbf10` | `0x8008be10` | bsim 10.9 | absent | BSIM | Quazal::FaultProcessingContext const::IsComplete(...) | `IsComplete__Q26Quazal22FaultProcessingContextCFv` |

### FetchContext.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/network/ObjDup/FetchContext.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a913a8` | `0x8008c0a0` | bsim 10.8 | absent | BSIM | Quazal::FetchContext::~FetchContext(...) | `__dt__Q26Quazal12FetchContextFv` |
| `0x82a91598` | `0x8008c260` | bsim 10.8 | absent | BSIM | Quazal::FetchContext::DeleteOrphanOnFailure(...) | `DeleteOrphanOnFailure__Q26Quazal12FetchContextFv` |

### IOCompletionNotifier.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/network/Platform/IOCompletionNotifier.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b0a110` | `0x800295d0` | bsim 13.7 | absent | BSIM | Quazal::IOCompletionNotifier::CreateIOCompletionContext(...) | `CreateIOCompletionContext__Q26Quazal20IOCompletionNotifierFv` |
| `0x82b0a600` | `0x800297a0` | bsim 10.7 | absent | BSIM | Quazal::IOCompletionNotifier::WaitForPollIOCompletion(...) | `WaitForPollIOCompletion__Q26Quazal20IOCompletionNotifierFUi` |

### JobBackEndServicesLogin.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/network/Services/JobBackEndServicesLogin.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aae5d8` | `0x800e3b00` | bsim 14.3 | absent | BSIM | Quazal::JobBackEndServicesLogin::ProcessSpecialConnResult(...) | `ProcessSpecialConnResult__Q26Quazal23JobBackEndServicesLoginFv` |
| `0x82aaee08` | `0x800e3fc0` | bsim 12.0 | absent | BSIM | Quazal::JobBackEndServicesLogin::ProcessAuthDisconnectionResult(...) | `ProcessAuthDisconnectionResult__Q26Quazal23JobBackEndServicesLoginFv` |

### JobCreateAccount.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/network/Services/JobCreateAccount.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82ae5d88` | `0x800d0420` | bsim 12.6 | absent | BSIM | Quazal::JobCreateAccount::ChangePasswordByGuest(...) | `ChangePasswordByGuest__Q26Quazal16JobCreateAccountFv` |
| `0x82ae60e0` | `0x800d0180` | bsim 10.8 | absent | BSIM | Quazal::JobCreateAccount::CreateAccountWithCustomData(...) | `CreateAccountWithCustomData__Q26Quazal16JobCreateAccountFv` |

### JobDisconnectStation.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/network/ObjDup/JobDisconnectStation.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82acad98` | `0x80092490` | bsim 13.1 | absent | BSIM | Quazal::JobDisconnectStation::JobDisconnectStation(...) | `__ct__Q26Quazal20JobDisconnectStationFPQ26Quazal7Station` |
| `0x82acb850` | `0x80093010` | bsim 12.9 | absent | BSIM | Quazal::JobDisconnectStation::WaitForCancellationComplete(...) | `WaitForCancellationComplete__Q26Quazal20JobDisconnectStationFv` |

### JobTerminateDOCore.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/network/ObjDup/JobTerminateDOCore.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82af6158` | `0x8009a430` | bsim 13.1 | absent | BSIM | Quazal::JobTerminateDOCore::JobTerminateDOCore(...) | `__ct__Q26Quazal18JobTerminateDOCoreFv` |
| `0x82af6af0` | `0x8009b4b0` | bsim 10.3 | absent | BSIM | Quazal::JobTerminateDOCore::ClearNonWellKnownCoreDOs(...) | `ClearNonWellKnownCoreDOs__Q26Quazal18JobTerminateDOCoreFv` |

### JobTerminateFacade.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/network/Products/JobTerminateFacade.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82abb9d8` | `0x800f9820` | bsim 11.1 | absent | BSIM | Quazal::JobTerminateFacade::StopToListen(...) | `StopToListen__Q26Quazal18JobTerminateFacadeFv` |
| `0x82abbd10` | `0x800f9e00` | bsim 12.5 | absent | BSIM | Quazal::JobTerminateFacade::TerminateStationConnectionManager(...) | `TerminateStationConnectionManager__Q26Quazal18JobTerminateFacadeFv` |

### KerberosEncryption.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/network/Services/KerberosEncryption.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b09220` | `0x800dfab0` | bsim 12.9 | confirmed | BSIM | Quazal::KerberosEncryption::Decrypt(...) | `Decrypt__Q26Quazal18KerberosEncryptionFRCQ26Quazal6BufferPQ26Quazal6Buffer` |
| `0x82b093d8` | `0x800dfc70` | bsim 11.1 | confirmed | BSIM | Quazal::KerberosEncryption::Decrypt(...) | `Decrypt__Q26Quazal18KerberosEncryptionFRCQ26Quazal6BufferPQ26Quazal6BufferRCQ26Quazal3Key` |

### Loader.o — system, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/system/utl/Loader.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82781610` | `0x8049a3c0` | bsim 11.6 | absent | BSIM | FileLoader::DebugText(...) | `DebugText__10FileLoaderFv` |
| `0x82799a60` | `0x804998c0` | bsim 13.1 | absent | BSIM | LoadMgr::StartAsyncUnload(...) | `StartAsyncUnload__7LoadMgrFv` |

### Log.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/network/Platform/Log.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82abca40` | `0x8001cfd0` | bsim 11.3 | absent | BSIM | Quazal::Log::Log(...) | `__ct__Q26Quazal3LogFv` |
| `0x82abd290` | `0x8001d460` | bsim 10.0 | absent | BSIM | Quazal::Log::GetOutputFormat(...) | `GetOutputFormat__Q26Quazal3LogFv` |

### Mesh.o — system, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/system/rndobj/Mesh.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x825864b0` | `0x808db620` | bsim 10.4 | absent | BSIM | RndMesh const::NumBones(...) | `NumBones__7RndMeshCFv` |
| `0x82643d50` | `0x808d9860` | bsim 10.6 | absent | BSIM | RndMesh::CacheStrips(...) | `CacheStrips__7RndMeshFR9BinStream` |

### MeterDisplay.o — system, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/system/bandobj/MeterDisplay.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82309850` | `0x805c6480` | bsim 11.8 | absent | BSIM | MeterDisplay::SetPercentageText(...) | `SetPercentageText__12MeterDisplayFb` |
| `0x8230b4e0` | `0x805c6470` | bsim 11.8 | absent | BSIM | MeterDisplay::SetShowText(...) | `SetShowText__12MeterDisplayFb` |

### PitchArrow.o — system, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/system/bandobj/PitchArrow.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x822e0418` | `0x805b4b60` | bsim 14.1 | absent | BSIM | PitchArrow::SetColor(...) | `SetColor__10PitchArrowF13VocalHUDColor` |
| `0x822e0de0` | `0x805b5310` | bsim 11.7 | absent | BSIM | PitchArrow::Poll(...) | `Poll__10PitchArrowFv` |

### RGState.o — system, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/system/beatmatch/RGState.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82756a80` | `0x80644880` | bsim 10.1 | absent | BSIM | RGState::RGState(...) | `__ct__7RGStateFv` |
| `0x82756b58` | `0x80644b20` | bsim 14.4 | absent | BSIM | UnpackRGData(...)   [free function] | `UnpackRGData__FUiRiRi` |

### RGUtl.o — system, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/system/beatmatch/RGUtl.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82755438` | `0x80645080` | bsim 13.9 | absent | BSIM | HandleSlashChords(...)   [free function] | `HandleSlashChords__FPciRC7GameGemiRi` |
| `0x82756850` | `0x80647350` | bsim 10.8 | absent | BSIM | RGFretNumberToString(...)   [free function] | `RGFretNumberToString__Fi` |

### RefCountedObject.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/network/Platform/RefCountedObject.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a784e8` | `0x80022280` | bsim 12.9 | absent | BSIM | Quazal::RefCountedObject::AcquireRef(...) | `AcquireRef__Q26Quazal16RefCountedObjectFv` |
| `0x82a786c0` | `0x800223e0` | bsim 10.4 | absent | BSIM | Quazal::RefCountedObject const::GetRefCount(...) | `GetRefCount__Q26Quazal16RefCountedObjectCFv` |

### Rot.o — system, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/system/math/Rot.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x823aa9f0` | `0x803ff770` | bsim 12.5 | absent | BSIM | __rs__FR9BinStreamR16TransformNoScale | `__rs__FR9BinStreamR16TransformNoScale` |
| `0x824dc730` | `0x80401560` | bsim 10.0 | absent | BSIM | FastInvert(...)   [free function] | `FastInvert__FRCQ23Hmx7Matrix3RQ23Hmx7Matrix3` |

### ScoreDisplay.o — system, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/system/bandobj/ScoreDisplay.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8230d328` | `0x805c0b20` | bsim 13.5 | absent | BSIM | ScoreDisplay::SetValues(...) | `SetValues__12ScoreDisplayFsiib` |
| `0x8230d9c0` | `0x805c03e0` | bsim 10.8 | absent | BSIM | ScoreDisplay::CopyMembers(...) | `CopyMembers__12ScoreDisplayFPC11UIComponentQ33Hmx6Object8CopyType` |

### SpinTest.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/network/Platform/SpinTest.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aa5470` | `0x80022710` | bsim 14.6 | absent | BSIM | Quazal::SpinTest::SpinTest(...) | `__ct__Q26Quazal8SpinTestFUiUi` |
| `0x82aa56b8` | `0x80022880` | bsim 10.8 | absent | BSIM | Quazal::SpinTest::LeaveOnTimeout(...) | `LeaveOnTimeout__Q26Quazal8SpinTestFv` |

### StarDisplay.o — system, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/system/bandobj/StarDisplay.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8230b4d8` | `0x805c4190` | bsim 11.8 | absent | BSIM | StarDisplay::SetShowDenominator(...) | `SetShowDenominator__11StarDisplayFb` |
| `0x82598a40` | `0x805c4170` | bsim 11.8 | absent | BSIM | StarDisplay::SetAlignment(...) | `SetAlignment__11StarDisplayFQ27RndText9Alignment` |

### StationManager.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/network/ObjDup/StationManager.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a8aae0` | `0x800b6870` | bsim 14.0 | confirmed | BSIM | Quazal::StationManager::SetInitialConnectionPoint(...) | `SetInitialConnectionPoint__Q26Quazal14StationManagerFQ26Quazal8DOHandlePQ26Quazal8EndPoint` |
| `0x82a8ab18` | `0x800b6880` | bsim 10.5 | absent | BSIM | Quazal::StationManager::ClearInitialEndPoint(...) | `ClearInitialEndPoint__Q26Quazal14StationManagerFv` |

### StreakMeter.o — system, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/system/bandobj/StreakMeter.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x822c4a78` | `0x805cea00` | bsim 11.4 | absent | BSIM | StreakMeter::ForceFadeInactiveParts(...) | `ForceFadeInactiveParts__11StreakMeterFv` |
| `0x822c6158` | `0x805cc930` | bsim 11.9 | absent | BSIM | StreakMeter::SetPartColor(...) | `SetPartColor__11StreakMeterFi13VocalHUDColor` |

### StreamSettings.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/network/Plugins/StreamSettings.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a3fdb0` | `0x80053590` | bsim 10.0 | absent | BSIM | Quazal::StreamSettings::GetInitialRTT(...) | `GetInitialRTT__Q26Quazal14StreamSettingsFv` |
| `0x82a3fe10` | `0x80053530` | bsim 10.0 | absent | BSIM | Quazal::StreamSettings const::GetKeepAliveTimeout(...) | `GetKeepAliveTimeout__Q26Quazal14StreamSettingsCFv` |

### Synth.o — system, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/system/synth/Synth.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826de8a0` | `0x809bd8c0` | bsim 11.6 | absent | BSIM | Synth::SetMasterVolume(...) | `SetMasterVolume__5SynthFf` |
| `0x826de8a8` | `0x809bd8d0` | bsim 13.3 | absent | BSIM | Synth::GetMasterVolume(...) | `GetMasterVolume__5SynthFv` |

### System.o — system, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/system/os/System.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x824fdda8` | `0x804428b0` | bsim 10.9 | absent | BSIM | SystemPreInit(...)   [free function] | `SystemPreInit__FPCc` |
| `0x824fe1c8` | `0x80442e20` | bsim 14.3 | absent | BSIM | SystemInit(...)   [free function] | `SystemInit__FPCc` |

### SystemComponent.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/network/Core/SystemComponent.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a78718` | `0x80035f40` | bsim 11.9 | absent | BSIM | Quazal::SystemComponent::SystemComponent(...) | `__ct__Q26Quazal15SystemComponentFRCQ26Quazal6String` |
| `0x82a78cf0` | `0x80036280` | bsim 11.8 | absent | BSIM | Quazal::SystemComponent const::Trace(...) | `Trace__Q26Quazal15SystemComponentCFUib` |

### Task.o — system, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/system/obj/Task.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82722790` | `0x8047fe40` | bsim 13.3 | absent | BSIM | TaskMgr const::UISeconds(...) | `UISeconds__7TaskMgrCFv` |
| `0x82722928` | `0x8047fff0` | bsim 12.4 | absent | BSIM | TaskMgr::SetAVOffset(...) | `SetAVOffset__7TaskMgrFf` |

### TexMovie.o — system, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/system/movie/TexMovie.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82721410` | `0x8078cb70` | bsim 12.6 | absent | BSIM | TexMovie::Enter(...) | `Enter__8TexMovieFv` |
| `0x82721500` | `0x8078ccf0` | bsim 14.8 | absent | BSIM | TexMovie::SetFile(...) | `SetFile__8TexMovieFRC8FilePath` |

### Text.o — system, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/system/rndobj/Text.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82441ef8` | `0x8094c3f0` | bsim 14.5 | absent | BSIM | RndText::GetCurrentStringDimensions(...) | `GetCurrentStringDimensions__7RndTextFRfRf` |
| `0x82442980` | `0x8094cba0` | bsim 11.6 | absent | BSIM | RndText const::GetDefiningFont(...) | `GetDefiningFont__7RndTextCFRUsP7RndFont` |

### Ticket.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/network/Services/Ticket.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b14ea0` | `0x800da6d0` | bsim 12.9 | absent | BSIM | Quazal::Ticket::Ticket(...) | `__ct__Q26Quazal6TicketFRCQ26Quazal6BufferUi` |
| `0x82b150b8` | `0x800da900` | bsim 10.2 | absent | BSIM | Quazal::Ticket const::IsValid(...) | `IsValid__Q26Quazal6TicketCFv` |

### TrackWatcher.o — system, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/system/beatmatch/TrackWatcher.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82778700` | `0x80664690` | bsim 11.0 | absent | BSIM | TrackWatcher::RecalcGemList(...) | `RecalcGemList__12TrackWatcherFv` |
| `0x82778830` | `0x80664910` | bsim 11.0 | absent | BSIM | TrackWatcher::SetAutoplayAccuracy(...) | `SetAutoplayAccuracy__12TrackWatcherFf` |

### TrackWatcherImpl.o — system, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/system/beatmatch/TrackWatcherImpl.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82770f38` | `0x80666eb0` | bsim 10.1 | absent | BSIM | TrackWatcherImpl::CheckForPitchBend(...) | `CheckForPitchBend__16TrackWatcherImplFf` |
| `0x82771b00` | `0x80666350` | bsim 12.0 | absent | BSIM | TrackWatcherImpl::CheckForPasses(...) | `CheckForPasses__16TrackWatcherImplFf` |

### UILabel.o — system, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/system/ui/UILabel.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827ccf10` | `0x807c0b70` | bsim 10.0 | absent | BSIM | UILabel::TextObj(...) | `TextObj__7UILabelFv` |
| `0x827cfff8` | `0x807c23e0` | bsim 11.5 | absent | BSIM | UILabel::FitText(...) | `FitText__7UILabelFv` |

### UIListWidget.o — system, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/system/ui/UIListWidget.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827abe78` | `0x807e99e0` | bsim 11.6 | absent | BSIM | UIListWidget const::DisabledAlphaScale(...) | `DisabledAlphaScale__12UIListWidgetCFv` |
| `0x82b4b3f8` | `0x807e99f0` | bsim 10.0 | absent | BSIM | UIListWidget const::WidgetDrawType(...) | `WidgetDrawType__12UIListWidgetCFv` |

### PatchDir.o — system, 5 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 5, contra 4)  ·  `src/system/bandobj/PatchDir.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82264328` | `0x805a93d0` | bsim 14.8 | contradicted | BSIM | __rs__FR9BinStreamR15PatchDescriptor | `__rs__FR9BinStreamR15PatchDescriptor` |
| `0x822647e0` | `0x805aa420` | bsim 14.5 | contradicted | BSIM | PatchLayer::ClearSticker(...) | `ClearSticker__10PatchLayerFv` |
| `0x82268510` | `0x805ae1a0` | bsim 14.9 | contradicted | BSIM | PatchDir::CollapseEmptyLayers(...) | `CollapseEmptyLayers__8PatchDirFv` |
| `0x82292640` | `0x805a9430` | bsim 10.5 | contradicted | BSIM | PatchSticker::PatchSticker(...) | `__ct__12PatchStickerFv` |
| `0x822d9d18` | `0x805aa210` | bsim 10.3 | absent | BSIM | PatchLayer::Copy(...) | `Copy__10PatchLayerFPCQ23Hmx6ObjectQ33Hmx6Object8CopyType` |

### BaseGuitarTrackWatcherImpl.o — system, 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 2)  ·  `src/system/beatmatch/BaseGuitarTrackWatcherImpl.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8277c7d0` | `0x8061c560` | bsim 13.8 | absent | BSIM | BaseGuitarTrackWatcherImpl::SetLastNoStrumGem(...) | `SetLastNoStrumGem__26BaseGuitarTrackWatcherImplFfi` |
| `0x8277ce40` | `0x8061c020` | bsim 10.9 | contradicted | BSIM | BaseGuitarTrackWatcherImpl::CheckForFretTimeout(...) | `CheckForFretTimeout__26BaseGuitarTrackWatcherImplFf` |
| `0x8277d0d0` | `0x8061b080` | bsim 11.6 | contradicted | BSIM | BaseGuitarTrackWatcherImpl::PollHook(...) | `PollHook__26BaseGuitarTrackWatcherImplFf` |

### DataArraySongInfo.o — system, 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 2)  ·  `src/system/meta/DataArraySongInfo.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8277e3a8` | `0x8074dfe0` | bsim 11.4 | contradicted | BSIM | DataArraySongInfo::DataArraySongInfo(...) | `__ct__17DataArraySongInfoFv` |
| `0x8277e448` | `0x8074e710` | bsim 13.5 | contradicted | BSIM | __rs__FR9BinStreamR17DataArraySongInfo | `__rs__FR9BinStreamR17DataArraySongInfo` |
| `0x8277e4e8` | `0x8074e760` | bsim 14.2 | absent | BSIM | __ls__FR9BinStreamRC13TrackChannels | `__ls__FR9BinStreamRC13TrackChannels` |

### Faders.o — system, 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 2)  ·  `src/system/synth/Faders.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x822c5cb8` | `0x80989dd0` | bsim 10.8 | contradicted | BSIM | Fader::Check(...) | `Check__5FaderFv` |
| `0x8251eb08` | `0x809885b0` | bsim 10.8 | contradicted | BSIM | FaderGroup::SetDirty(...) | `SetDirty__10FaderGroupFv` |
| `0x826edae0` | `0x809885c0` | bsim 10.8 | absent | BSIM | FaderGroup::ClearDirty(...) | `ClearDirty__10FaderGroupFv` |

### Result.o — network, 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 2)  ·  `src/network/Platform/Result.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a5bec8` | `0x80022470` | bsim 10.9 | contradicted | BSIM | Quazal::qResult::qResult(...) | `__ct__Q26Quazal7qResultFRCi` |
| `0x82a5c100` | `0x800224e0` | bsim 10.9 | contradicted | BSIM | __as__Q26Quazal7qResultFRCi | `__as__Q26Quazal7qResultFRCi` |
| `0x82a5c140` | `0x80022500` | bsim 13.4 | absent | BSIM | __as__Q26Quazal7qResultFRCQ26Quazal7qResult | `__as__Q26Quazal7qResultFRCQ26Quazal7qResult` |

### BandCharacter.o — system, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 1)  ·  `src/system/bandobj/BandCharacter.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8226c820` | `0x80542220` | bsim 13.6 | absent | BSIM | BandCharacter::SetClipTypes(...) | `SetClipTypes__13BandCharacterF6Symbol6Symbol` |
| `0x82270370` | `0x80540800` | bsim 12.4 | contradicted | BSIM | BandCharacter::PlayFaceClip(...) | `PlayFaceClip__13BandCharacterFv` |

### BandDirector.o — system, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 1)  ·  `src/system/bandobj/BandDirector.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8227ff68` | `0x804f3c10` | bsim 15.0 | absent | BSIM | BandDirector::PlayNextShot(...) | `PlayNextShot__12BandDirectorFv` |
| `0x82288308` | `0x804f5990` | bsim 14.1 | contradicted | BSIM | BandDirector::HarvestDircuts(...) | `HarvestDircuts__12BandDirectorFv` |

### BandPatchMesh.o — system, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 1)  ·  `src/system/bandobj/BandPatchMesh.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82337fc8` | `0x80528110` | bsim 11.3 | contradicted | BSIM | BandPatchMesh::WorkVerts::SetSameVerts(...) | `SetSameVerts__Q213BandPatchMesh9WorkVertsFPQ213BandPatchMesh9WorkVerts` |
| `0x82339300` | `0x8052aad0` | bsim 11.2 | absent | BSIM | BandPatchMesh::BandPatchMesh(...) | `__ct__13BandPatchMeshFPQ23Hmx6Object` |

### Job.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 1)  ·  `src/network/Core/Job.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aa1418` | `0x8002e180` | bsim 14.5 | absent | BSIM | Quazal::Job::Job(...) | `__ct__Q26Quazal3JobFRCQ26Quazal11DebugString` |
| `0x82aa1608` | `0x8002e330` | bsim 11.7 | contradicted | BSIM | Quazal::Job::AddActivity(...) | `AddActivity__Q26Quazal3JobFPCc` |

### LightPresetManager.o — system, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 1)  ·  `src/system/world/LightPresetManager.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x824a5a28` | `0x80855540` | bsim 13.6 | absent | BSIM | LightPresetManager::SetPresetsEquivalent(...) | `SetPresetsEquivalent__18LightPresetManagerFb` |
| `0x824a5b88` | `0x80856580` | bsim 14.5 | contradicted | BSIM | LightPresetManager::GetPresets(...) | `GetPresets__18LightPresetManagerFRP11LightPresetRP11LightPreset` |

### Router.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 1)  ·  `src/network/Plugins/Router.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aa2f48` | `0x80063990` | bsim 11.8 | contradicted | BSIM | Quazal::Router::GetMTU(...) | `GetMTU__Q26Quazal6RouterFv` |
| `0x82aa2f78` | `0x800639a0` | bsim 14.0 | absent | BSIM | Quazal::Router::ShouldRoute(...) | `ShouldRoute__Q26Quazal6RouterFPCQ26Quazal11InetAddress` |

### Selection.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 1)  ·  `src/network/ObjDup/Selection.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a84848` | `0x800a7da0` | bsim 10.8 | absent | BSIM | Quazal::Selection::SetFlags(...) | `SetFlags__Q26Quazal9SelectionFUc` |
| `0x82a84d80` | `0x800a8720` | bsim 12.7 | contradicted | BSIM | stlpmtx_std::_Rb_tree<Q26Quazal8DOHandle,Q211stlpmtx_std24less<Q26Quazal8DOHandle>,Q211stlpmtx_std54pair<CQ26Quazal8DOHandle,PQ26Quazal16DuplicatedObject>,Q211stlpmtx_std83_Select1st<Q211stlpmtx_std54pair<CQ26Quazal8DOHandle,PQ26Quazal16DuplicatedObject>>,Q29stlp_priv84_MapTraitsT<Q211stlpmtx_std54pair<CQ26Quazal8DOHandle,PQ26Quazal16DuplicatedObject>>,Q26Quazal85MemAllocator<Q211stlpmtx_std54pair<CQ26Quazal8DOHandle,PQ26Quazal16DuplicatedObject>>>::insert_unique(...) | `insert_unique__Q211stlpmtx_std439_Rb_tree<Q26Quazal8DOHandle,Q211stlpmtx_std24less<Q26Quazal8DOHandle>,Q211stlpmtx_std54pair<CQ26Quazal8DOHandle,PQ26Quazal16DuplicatedObject>,Q211stlpmtx_std83_Select1st<Q211stlpmtx_std54pair<CQ26Quazal8DOHandle,PQ26Quazal16DuplicatedObject>>,Q29stlp_priv84_MapTraitsT<Q211stlpmtx_std54pair<CQ26Quazal8DOHandle,PQ26Quazal16DuplicatedObject>>,Q26Quazal85MemAllocator<Q211stlpmtx_std54pair<CQ26Quazal8DOHandle,PQ26Quazal16DuplicatedObject>>>FQ211stlpmtx_std189_Rb_tree_iterator<Q211stlpmtx_std54pair<CQ26Quazal8DOHandle,PQ26Quazal16DuplicatedObject>,Q29stlp_priv84_MapTraitsT<Q211stlpmtx_std54pair<CQ26Quazal8DOHandle,PQ26Quazal16DuplicatedObject>>>RCQ211stlpmtx_std54pair<CQ26Quazal8DOHandle,PQ26Quazal16DuplicatedObject>` |

### SingleThreadCallPolicy.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 1)  ·  `src/network/Core/SingleThreadCallPolicy.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a870c8` | `0x80034ab0` | bsim 10.8 | contradicted | BSIM | Quazal::SingleThreadCallPolicy::SetMaximumRecursionLevel(...) | `SetMaximumRecursionLevel__Q26Quazal22SingleThreadCallPolicyFUi` |
| `0x82aa58c0` | `0x80034b40` | bsim 11.7 | absent | BSIM | Quazal::SingleThreadCallPolicy const::CurrentThreadIsDispatchingJobs(...) | `CurrentThreadIsDispatchingJobs__Q26Quazal22SingleThreadCallPolicyCFv` |

### StationURL.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 1)  ·  `src/network/Plugins/StationURL.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a73060` | `0x8004d540` | bsim 10.1 | contradicted | BSIM | Quazal::StationURL::StationURL(...) | `__ct__Q26Quazal10StationURLFv` |
| `0x82a74f38` | `0x80050e10` | bsim 10.3 | absent | BSIM | Quazal::StationURL::SetURL(...) | `SetURL__Q26Quazal10StationURLFPCc` |

### StoreOffer.o — system, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 1)  ·  `src/system/meta/StoreOffer.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8255e040` | `0x80762fd0` | bsim 11.6 | contradicted | BSIM | StoreOffer const::Artist(...) | `Artist__10StoreOfferCFv` |
| `0x8274b8c0` | `0x80762fe0` | bsim 11.6 | absent | BSIM | StoreOffer const::AlbumName(...) | `AlbumName__10StoreOfferCFv` |

### StorePanel.o — system, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 1)  ·  `src/system/meta/StorePanel.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8278fc20` | `0x80773020` | bsim 13.5 | absent | BSIM | StorePanel const::Unloading(...) | `Unloading__10StorePanelCFv` |
| `0x82790fd8` | `0x80772840` | bsim 14.5 | contradicted | BSIM | StorePanel::Poll(...) | `Poll__10StorePanelFv` |

### UDPTransport.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 1)  ·  `src/network/Plugins/UDPTransport.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82ae76d0` | `0x80069960` | bsim 13.9 | contradicted | BSIM | Quazal::UDPTransport::~UDPTransport(...) | `__dt__Q26Quazal12UDPTransportFv` |
| `0x82ae9520` | `0x8006b970` | bsim 12.5 | absent | BSIM | Quazal::UDPTransport::ServiceIOCompletions(...) | `ServiceIOCompletions__Q26Quazal12UDPTransportFv` |

### Utl.o — system, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 1)  ·  `src/system/rndobj/Utl.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82545b48` | `0x80969320` | bsim 10.1 | absent | BSIM | GetRenderTextures(...)   [free function] | `GetRenderTextures__FP9ObjectDir` |
| `0x82733380` | `0x80485520` | bsim 13.2 | contradicted | BSIM | StringMatchesFilter(...)   [free function] | `StringMatchesFilter__FPCcPCc` |

### VocalTrackDir.o — system, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 1)  ·  `src/system/bandobj/VocalTrackDir.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x822e4598` | `0x805e7220` | bsim 13.0 | contradicted | BSIM | VocalTrackDir::ShowPhraseFeedback(...) | `ShowPhraseFeedback__13VocalTrackDirFiiib` |
| `0x822e9450` | `0x805e6510` | bsim 15.0 | absent | BSIM | VocalTrackDir::UpdateConfiguration(...) | `UpdateConfiguration__13VocalTrackDirFv` |

### WiiFriendMgr.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 1)  ·  `src/network/net/WiiFriendMgr.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82527748` | `0x80120a20` | bsim 10.5 | contradicted | BSIM | WiiFriendMgr::Poll(...) | `Poll__12WiiFriendMgrFv` |
| `0x82533358` | `0x8011e9d0` | bsim 14.0 | absent | BSIM | WiiFriend const::GetProfileByIdx(...) | `GetProfileByIdx__9WiiFriendCFi` |

### ADSR.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/synth/ADSR.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8270c2c8` | `0x8097dd80` | bsim 10.1 | absent | BSIM | __rs__FR9BinStreamR4ADSR | `__rs__FR9BinStreamR4ADSR` |

### AccountManagementProtocolDDL.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Services/AccountManagementProtocolDDL.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82ae1db0` | `0x800d5c00` | bsim 11.0 | absent | BSIM | Quazal::AccountManagementProtocolClient::ExtractCallSpecificResults(...) | `ExtractCallSpecificResults__Q26Quazal31AccountManagementProtocolClientFPQ26Quazal7MessagePQ26Quazal19ProtocolCallContext` |

### Archive.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/os/Archive.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x824ff158` | `0x80408bf0` | bsim 13.0 | absent | BSIM | Archive::SetArchivePermission(...) | `SetArchivePermission__7ArchiveFiPCi` |

### ArpeggioShape.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/bandobj/ArpeggioShape.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x823435c0` | `0x804e05a0` | bsim 10.5 | absent | BSIM | ArpeggioShape::SetYPos(...) | `SetYPos__13ArpeggioShapeFf` |

### BackEndServices.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Services/BackEndServices.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a5a960` | `0x800e0ea0` | bsim 13.0 | absent | BSIM | Quazal::BackEndServices::IgnoreRemoteTraces(...) | `IgnoreRemoteTraces__Q26Quazal15BackEndServicesFv` |

### BandCamShot.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/bandobj/BandCamShot.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x822a2340` | `0x804e8450` | bsim 13.0 | absent | BSIM | BandCamShot::EndAnim(...) | `EndAnim__11BandCamShotFv` |

### BandSwatch.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/bandobj/BandSwatch.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8229b780` | `0x8053b370` | bsim 13.8 | absent | BSIM | BandSwatch::Terminate(...) | `Terminate__10BandSwatchFv` |

### BeatMatchController.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/beatmatch/BeatMatchController.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8233aaa8` | `0x8061e750` | bsim 12.9 | absent | BSIM | BeatMatchController const::RegisterKey(...) | `RegisterKey__19BeatMatchControllerCFi` |

### BeatMatchUtl.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/beatmatch/BeatMatchUtl.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8276b480` | `0x8061e880` | bsim 10.1 | absent | BSIM | GemPlayableBy(...)   [free function] | `GemPlayableBy__Fii` |

### BinkClip.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/synth/BinkClip.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8276ba00` | `0x8097ecc0` | bsim 10.8 | absent | BSIM | BinkClip::UnloadWhenFinishedPlaying(...) | `UnloadWhenFinishedPlaying__8BinkClipFb` |

### BufStreamNAND.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/utl/BufStreamNAND.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8274fd48` | `0x8048a260` | bsim 10.7 | absent | BSIM | BufStreamNAND::DeleteChecksum(...) | `DeleteChecksum__13BufStreamNANDFv` |

### CallRegister.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/ObjDup/CallRegister.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a8dbc8` | `0x80074bc0` | bsim 11.8 | absent | BSIM | Quazal::CallRegister::CancelPeriodicJobs(...) | `CancelPeriodicJobs__Q26Quazal12CallRegisterFv` |

### CameraShot.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/world/CameraShot.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x825864b8` | `0x80806b80` | bsim 11.6 | absent | BSIM | RndCam const::YFov(...) | `YFov__6RndCamCFv` |

### ChangeMasterStationOperation.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/ObjDup/ChangeMasterStationOperation.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a86c30` | `0x800765a0` | bsim 11.0 | confirmed | BSIM | Quazal::ChangeMasterStationOperation const::GetImplicitStationConnection(...) | `GetImplicitStationConnection__Q26Quazal28ChangeMasterStationOperationCFv` |

### CharBones.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/char/CharBones.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82399768` | `0x8068b990` | bsim 10.6 | absent | BSIM | MakeString<f,s>(...)   [free function] | `MakeString<f,s>__FPCcfs_PCc` |

### CharDriver.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/char/CharDriver.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82364890` | `0x806b2600` | bsim 13.4 | absent | BSIM | CharDriver::Clear(...) | `Clear__10CharDriverFv` |

### CharEyeDartRuleset.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/char/CharEyeDartRuleset.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8243d960` | `0x806c89c0` | bsim 14.2 | absent | BSIM | CharEyeDartRuleset::Copy(...) | `Copy__18CharEyeDartRulesetFPCQ23Hmx6ObjectQ33Hmx6Object8CopyType` |

### CharFaceServo.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/char/CharFaceServo.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8226a708` | `0x806ca440` | bsim 11.6 | absent | BSIM | CharFaceServo const::BlinkWeightLeft(...) | `BlinkWeightLeft__13CharFaceServoCFv` |

### CharIKHand.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/char/CharIKHand.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826790b8` | `0x806ee390` | bsim 11.6 | absent | BSIM | CharCollide const::Radius(...) | `Radius__11CharCollideCFv` |

### ChecksumAlgorithm.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Plugins/ChecksumAlgorithm.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82af4518` | `0x80039e90` | bsim 12.4 | absent | BSIM | Quazal::ChecksumAlgorithm::DeriveKey(...) | `DeriveKey__Q26Quazal17ChecksumAlgorithmFPCcUi` |

### ClientProtocol.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Protocol/ClientProtocol.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a7ae40` | `0x8006cd30` | bsim 14.2 | confirmed | BSIM | Quazal::ClientProtocol::ProcessResponse(...) | `ProcessResponse__Q26Quazal14ClientProtocolFPQ26Quazal7MessagePQ26Quazal8EndPoint` |

### CompetitionClient.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Services/CompetitionClient.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a5e790` | `0x800db960` | bsim 14.0 | absent | BSIM | Quazal::CompetitionClient::CompetitionClient(...) | `__ct__Q26Quazal17CompetitionClientFPQ26Quazal17MatchMakingClient` |

### ConnectionManager.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Plugins/ConnectionManager.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82abe408` | `0x80044250` | bsim 10.9 | absent | BSIM | Quazal::ConnectionManager const::IsTerminated(...) | `IsTerminated__Q26Quazal17ConnectionManagerCFv` |

### ContentMgr.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/os/ContentMgr.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8250b100` | `0x80416320` | bsim 12.2 | absent | BSIM | ContentMgr::SetReadFailureHandler(...) | `SetReadFailureHandler__10ContentMgrFPQ23Hmx6Object` |

### ContentMgr_Wii.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/os/ContentMgr_Wii.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8226e7b8` | `0x80417a40` | bsim 10.1 | absent | BSIM | DebugPrintContents(...)   [free function] | `DebugPrintContents__FP9CNTHandle` |

### DDLDeclarations.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/ObjDup/DDLDeclarations.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a70938` | `0x800772c0` | bsim 10.5 | absent | BSIM | Quazal::DDLDeclarations::UnloadAll(...) | `UnloadAll__Q26Quazal15DDLDeclarationsFv` |

### DOClassesTable.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/ObjDup/DOClassesTable.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a86330` | `0x80079640` | bsim 11.6 | absent | BSIM | Quazal::DOClassesTable::~DOClassesTable(...) | `__dt__Q26Quazal14DOClassesTableFv` |

### DOCoreTypes.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/ObjDup/DOCoreTypes.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a6e260` | `0x8007ade0` | bsim 14.8 | absent | BSIM | Quazal::_Type_qresult::Extract(...) | `Extract__Q26Quazal13_Type_qresultFPQ26Quazal7MessagePQ26Quazal7qResult` |

### DOFilters.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/ObjDup/DOFilters.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a57b10` | `0x8007c280` | bsim 12.6 | absent | BSIM | Quazal::IsAWellKnownDOFilter::Filter(...) | `Filter__Q26Quazal20IsAWellKnownDOFilterFPQ26Quazal16DuplicatedObject` |

### DOOperation.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/ObjDup/DOOperation.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82ac4368` | `0x8007cc80` | bsim 13.5 | absent | BSIM | Quazal::DOOperation::DOOperation(...) | `__ct__Q26Quazal11DOOperationFQ26Quazal8DOHandleQ26Quazal8DOHandle` |

### DOSelections.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/ObjDup/DOSelections.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a90028` | `0x800813c0` | bsim 10.3 | absent | BSIM | Quazal::DOSelections::RemoveFromAllSelections(...) | `RemoveFromAllSelections__Q26Quazal12DOSelectionsFPQ26Quazal16DuplicatedObject` |

### DataFile.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/obj/DataFile.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82747268` | `0x80453270` | bsim 10.4 | absent | BSIM | DataReadStream(...)   [free function] | `DataReadStream__FP9BinStream` |

### DataNode.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/obj/DataNode.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82725e18` | `0x8045e030` | bsim 10.7 | absent | BSIM | DataNode const::Evaluate(...) | `Evaluate__8DataNodeCFv` |

### DateTime.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/os/DateTime.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8250f480` | `0x8041dd30` | bsim 13.1 | absent | BSIM | DateTime const::Year(...) | `Year__8DateTimeCFv` |

### DuplicationSpaceTable.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Extensions/DuplicationSpaceTable.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b18a30` | `0x800c3820` | bsim 11.0 | absent | BSIM | Quazal::DuplicationSpaceTable::StopPeriodicMatch(...) | `StopPeriodicMatch__Q26Quazal21DuplicationSpaceTableFv` |

### DynamicRunTimeInterface.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/ObjDup/DynamicRunTimeInterface.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a41160` | `0x80089f00` | bsim 14.4 | absent | BSIM | Quazal::PseudoGlobalVariable<PQ26Quazal23DynamicRunTimeInterface>::AllocateExtraContexts(...) | `AllocateExtraContexts__Q26Quazal57PseudoGlobalVariable<PQ26Quazal23DynamicRunTimeInterface>Fv` |

### EmulationDevice.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Plugins/EmulationDevice.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a58b80` | `0x800454a0` | bsim 12.7 | absent | BSIM | Quazal::EmulationDevice::EmulationDevice(...) | `__ct__Q26Quazal15EmulationDeviceFv` |

### EncryptXTEA.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/utl/EncryptXTEA.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b620e0` | `0x80494b40` | bsim 10.8 | absent | BSIM | XTEABlockEncrypter::SetKey(...) | `SetKey__18XTEABlockEncrypterFPCUc` |

### EncryptionAlgorithm.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Plugins/EncryptionAlgorithm.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82af4d80` | `0x8003ff40` | bsim 12.6 | absent | BSIM | Quazal::EncryptionAlgorithm::EncryptionAlgorithm(...) | `__ct__Q26Quazal19EncryptionAlgorithmFUiUi` |

### Event.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Platform/Event.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82ac3da0` | `0x8001bcf0` | bsim 10.1 | absent | BSIM | Quazal::Event::Event(...) | `__ct__Q26Quazal5EventFPQ26Quazal12EventHandlerUiUi` |

### FakeSongMgr.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/utl/FakeSongMgr.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827aa848` | `0x80494ea0` | bsim 10.1 | absent | BSIM | FakeSongMgr::MidiFile(...) | `MidiFile__11FakeSongMgrFPC8SongInfo` |

### FingerShape.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/bandobj/FingerShape.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82342688` | `0x80587120` | bsim 13.9 | absent | BSIM | FingerShape::Reset(...) | `Reset__11FingerShapeFb` |

### FixedSizeSaveableStream.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/meta/FixedSizeSaveableStream.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8270f850` | `0x80750b70` | bsim 12.0 | absent | BSIM | FixedSizeSaveableStream::FixedSizeSaveableStream(...) | `__ct__23FixedSizeSaveableStreamFPvib` |

### Flare.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/rndobj/Flare.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x824640f0` | `0x808afa40` | bsim 10.1 | absent | BSIM | RndFlare::SetSteps(...) | `SetSteps__8RndFlareFi` |

### Font.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/rndobj/Font.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826d3880` | `0x808b1020` | bsim 12.4 | absent | BSIM | RndFont::SetBaseKerning(...) | `SetBaseKerning__7RndFontFf` |

### Fur.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/rndobj/Fur.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826c0db0` | `0x8096e100` | bsim 11.4 | absent | BSIM | RndFur::RndFur(...) | `__ct__6RndFurFv` |

### FxSendFlanger.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/synth/FxSendFlanger.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82308210` | `0x8098d6b0` | bsim 13.1 | absent | BSIM | FxSendFlanger::Copy(...) | `Copy__13FxSendFlangerFPCQ23Hmx6ObjectQ33Hmx6Object8CopyType` |

### FxSendWah.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/synth/FxSendWah.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x822ee118` | `0x80995810` | bsim 13.4 | absent | BSIM | FxSendWah::Copy(...) | `Copy__9FxSendWahFPCQ23Hmx6ObjectQ33Hmx6Object8CopyType` |

### GemTrackDir.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/bandobj/GemTrackDir.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x822d1a40` | `0x805db7b0` | bsim 11.6 | absent | BSIM | GemTrackDir::GetKeyOffset(...) | `GetKeyOffset__11GemTrackDirFv` |

### IOCompletionContext.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Platform/IOCompletionContext.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b23b08` | `0x800294e0` | bsim 12.1 | absent | BSIM | Quazal::IOCompletionContext::Reset(...) | `Reset__Q26Quazal19IOCompletionContextFv` |

### IntPacker.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/utl/IntPacker.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827a05d0` | `0x80498910` | bsim 10.8 | absent | BSIM | IntPacker::SetPos(...) | `SetPos__9IntPackerFUi` |

### InterfaceInfo.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Platform/InterfaceInfo.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82af18c0` | `0x80029130` | bsim 10.4 | absent | BSIM | Quazal::InterfaceInfo::InterfaceInfo(...) | `__ct__Q26Quazal13InterfaceInfoFv` |

### JobBackEndServicesLogout.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Services/JobBackEndServicesLogout.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82ab13b8` | `0x800e4c60` | bsim 11.5 | absent | BSIM | Quazal::JobBackEndServicesLogout::TerminateStreamManager(...) | `TerminateStreamManager__Q26Quazal24JobBackEndServicesLogoutFv` |

### JobConnectStation.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/ObjDup/JobConnectStation.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a884f8` | `0x80090490` | bsim 10.1 | absent | BSIM | Quazal::JobConnectStation::TryConnectViaIncomingEndPoint(...) | `TryConnectViaIncomingEndPoint__Q26Quazal17JobConnectStationFv` |

### JobExecuteDelayedOperation.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/ObjDup/JobExecuteDelayedOperation.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82acacb8` | `0x800933b0` | bsim 13.8 | absent | BSIM | Quazal::JobExecuteDelayedOperation::Execute(...) | `Execute__Q26Quazal26JobExecuteDelayedOperationFv` |

### JobJoinSession.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/ObjDup/JobJoinSession.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a9c988` | `0x80095910` | bsim 10.7 | absent | BSIM | Quazal::JobJoinSession::ProcessNegativeJoinResponse(...) | `ProcessNegativeJoinResponse__Q26Quazal14JobJoinSessionFUci` |

### JobNintendoTerminate.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/RVPackages/JobNintendoTerminate.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82ab1910` | `0x800f7c30` | bsim 10.1 | absent | BSIM | Quazal::JobNintendoTerminate::JobNintendoTerminate(...) | `__ct__Q26Quazal20JobNintendoTerminateFUiPQ26Quazal14NintendoClient` |

### JobProcessFault.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/ObjDup/JobProcessFault.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8234fa48` | `0x80096820` | bsim 11.6 | absent | BSIM | Quazal::DORef const::GetHandle(...) | `GetHandle__Q26Quazal5DORefCFv` |

### JobProcessJoinRequest.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/ObjDup/JobProcessJoinRequest.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82ac2298` | `0x8009a230` | bsim 13.0 | absent | BSIM | Quazal::JobProcessJoinRequest::JoinSuccess(...) | `JoinSuccess__Q26Quazal21JobProcessJoinRequestFv` |

### JobTicketManagerAcquireTicket.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Services/JobTicketManagerAcquireTicket.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b25430` | `0x800d8000` | bsim 14.3 | absent | BSIM | Quazal::JobTicketManagerAcquireTicket::ProcessResponse(...) | `ProcessResponse__Q26Quazal29JobTicketManagerAcquireTicketFv` |

### JoinSessionOperation.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/ObjDup/JoinSessionOperation.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a9cdb8` | `0x8009ca20` | bsim 10.5 | absent | BSIM | Quazal::JoinSessionOperation::Approve(...) | `Approve__Q26Quazal20JoinSessionOperationFv` |

### LayerDir.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/bandobj/LayerDir.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82313ee8` | `0x80587640` | bsim 11.6 | absent | BSIM | LayerDir::DrawShowing(...) | `DrawShowing__8LayerDirFv` |

### LightPreset.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/world/LightPreset.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b78198` | `0x80838b30` | bsim 10.2 | absent | BSIM | RndEnviron const::GetAnimateFromPreset(...) | `GetAnimateFromPreset__10RndEnvironCFv` |

### Lit.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/rndobj/Lit.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827e8a40` | `0x808c92e0` | bsim 12.4 | absent | BSIM | RndLight::SetBotRadius(...) | `SetBotRadius__8RndLightFf` |

### LockChecker.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Platform/LockChecker.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aa53c0` | `0x8001cf70` | bsim 12.0 | absent | BSIM | Quazal::LockChecker::LockChecker(...) | `__ct__Q26Quazal11LockCheckerFUi` |

### LogFile.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/utl/LogFile.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827a6c70` | `0x8049c6d0` | bsim 10.6 | absent | BSIM | LogFile::AdvanceFile(...) | `AdvanceFile__7LogFileFv` |

### Mat.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/rndobj/Mat.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x825323c8` | `0x808ce4c0` | bsim 11.6 | absent | BSIM | RndMat::GetRefractStrength(...) | `GetRefractStrength__6RndMatFv` |

### MatchOperationTriggers.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Extensions/MatchOperationTriggers.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b199a8` | `0x800c5190` | bsim 10.8 | absent | BSIM | Quazal::MatchOperationTriggers::DisablePeriodicMatch(...) | `DisablePeriodicMatch__Q26Quazal22MatchOperationTriggersFv` |

### MatchmakingSettings.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/net/MatchmakingSettings.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x823e0e88` | `0x800fe4a0` | bsim 12.0 | confirmed | BSIM | SearchSettings::SearchSettings(...) | `__ct__14SearchSettingsFibi` |

### MemMgr.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/utl/MemMgr.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82797500` | `0x804a0300` | bsim 11.0 | absent | BSIM | MemDoTempAllocations::MemDoTempAllocations(...) | `__ct__20MemDoTempAllocationsFbb` |

### MemPoint.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/utl/MemPoint.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82ae0b30` | `0x804a25a0` | bsim 10.1 | absent | BSIM | MemPointDelta::MemPointDelta(...) | `__ct__13MemPointDeltaFv` |

### Mem_Wii.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/utl/Mem_Wii.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x822cad58` | `0x804a2300` | bsim 10.1 | absent | BSIM | __sys_alloc | `__sys_alloc` |

### Message.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Plugins/Message.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a7b5e8` | `0x80048130` | bsim 14.1 | confirmed | BSIM | Quazal::Message::GetLastError(...) | `GetLastError__Q26Quazal7MessageFv` |

### MetaMusic.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/synth/MetaMusic.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826f1d20` | `0x80998a40` | bsim 11.6 | absent | BSIM | MetaMusic const::IsFading(...) | `IsFading__9MetaMusicCFv` |

### MicClientMapper.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/synth/MicClientMapper.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826f1750` | `0x8099a770` | bsim 11.1 | absent | BSIM | MicClientMapper::SetMicManager(...) | `SetMicManager__15MicClientMapperFP19MicManagerInterface` |

### MigrationContext.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/ObjDup/MigrationContext.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a92698` | `0x8009eb70` | bsim 10.7 | absent | BSIM | Quazal::MigrationContext::ProcessOutcome(...) | `ProcessOutcome__Q26Quazal16MigrationContextFQ26Quazal8DOHandleQ36Quazal13DOCallContext8_Outcome` |

### MoggClipMap.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/synth/MoggClipMap.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826d61d0` | `0x809a42a0` | bsim 11.6 | absent | BSIM | MoggClipMap::MoggClipMap(...) | `__ct__11MoggClipMapFPQ23Hmx6Object` |

### NATTraversalStream.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Plugins/NATTraversalStream.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b07da0` | `0x80049e60` | bsim 13.8 | absent | BSIM | Quazal::NATTraversalStream::NATTraversalStream(...) | `__ct__Q26Quazal18NATTraversalStreamFPQ26Quazal18NATTraversalEnginePQ26Quazal13RootTransport` |

### NetCacheMgr.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/utl/NetCacheMgr.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82441ee8` | `0x804b2710` | bsim 13.1 | absent | BSIM | NetLoaderRef::AddRef(...) | `AddRef__12NetLoaderRefFv` |

### NetZProductInfo.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Products/NetZProductInfo.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82ab1d48` | `0x800fb720` | bsim 13.4 | absent | BSIM | Quazal::NetZProductInfo::NetZProductInfo(...) | `__ct__Q26Quazal15NetZProductInfoFv` |

### Network.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Plugins/Network.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aab358` | `0x8004a6d0` | bsim 10.0 | absent | BSIM | Quazal::Network::GetNATTraversalEngine(...) | `GetNATTraversalEngine__Q26Quazal7NetworkFv` |

### NintendoClient.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/RVPackages/NintendoClient.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a5a4b8` | `0x800f8850` | bsim 11.6 | absent | BSIM | Quazal::NintendoClient::OnLoginCompletion(...) | `OnLoginCompletion__Q26Quazal14NintendoClientFPQ26Quazal11CallContextPCQ26Quazal11UserContext` |

### Object.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/obj/Object.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82735f20` | `0x80477e20` | bsim 12.8 | absent | BSIM | Hmx::Object::SetTypeDef(...) | `SetTypeDef__Q23Hmx6ObjectFP9DataArray` |

### OnlineID.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/os/OnlineID.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82511020` | `0x8043a710` | bsim 10.8 | absent | BSIM | OnlineID::OnlineID(...) | `__ct__8OnlineIDFv` |

### Operation.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Core/Operation.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a87190` | `0x8002ed30` | bsim 10.8 | absent | BSIM | Quazal::Operation const::Trace(...) | `Trace__Q26Quazal9OperationCFUi` |

### OutfitConfig.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/bandobj/OutfitConfig.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x823726e0` | `0x8058e3b0` | bsim 11.5 | absent | BSIM | OutfitConfig::MatSwap::MatSwap(...) | `__ct__Q212OutfitConfig7MatSwapFPQ23Hmx6Object` |

### OutputFormat.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Platform/OutputFormat.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82abc908` | `0x80020730` | bsim 13.8 | absent | BSIM | Quazal::OutputFormat::IncreaseIndent(...) | `IncreaseIndent__Q26Quazal12OutputFormatFUi` |

### OverdriveMeter.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/bandobj/OverdriveMeter.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82675170` | `0x805d1560` | bsim 11.4 | absent | BSIM | OverdriveMeter::SetNoOverdrive(...) | `SetNoOverdrive__14OverdriveMeterFv` |

### PRUDPEndPoint.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Plugins/PRUDPEndPoint.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b03878` | `0x80057950` | bsim 12.5 | absent | BSIM | Quazal::PRUDPEndPoint::PRUDPEndPoint(...) | `__ct__Q26Quazal13PRUDPEndPointFPQ26Quazal24ConnectionOrientedStreamPCQ26Quazal10StationURL` |

### PacketIn.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Plugins/PacketIn.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b23f98` | `0x800672b0` | bsim 13.1 | absent | BSIM | Quazal::PacketIn::PacketIn(...) | `__ct__Q26Quazal8PacketInFv` |

### Part.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/rndwii/Part.cpp`  ·  DC3 shared  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8266cd28` | `0x809ecf70` | bsim 10.2 | absent | BSIM | WiiRnd::GetShowParticle(...) | `GetShowParticle__6WiiRndFv` |

### PatchRenderer.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/bandobj/PatchRenderer.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8229c880` | `0x805b25c0` | bsim 14.3 | absent | BSIM | PatchRenderer::DrawBefore(...) | `DrawBefore__13PatchRendererFv` |

### PhraseAnalyzer.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/beatmatch/PhraseAnalyzer.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827673a0` | `0x8063c350` | bsim 14.2 | absent | BSIM | PhraseAnalyzer::Analyze(...) | `Analyze__14PhraseAnalyzerFv` |

### PostProc.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/rndwii/PostProc.cpp`  ·  DC3 shared  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b5aa00` | `0x809eeeb0` | bsim 11.5 | absent | BSIM | WiiPostProc::WiiPostProc(...) | `__ct__11WiiPostProcFv` |

### ProductInfo.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/ProductInfo/ProductInfo.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a713a8` | `0x800baf10` | bsim 10.0 | absent | BSIM | Quazal::ProductInfo const::GetBuild(...) | `GetBuild__Q26Quazal11ProductInfoCFv` |

### ProfilePicture.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/os/ProfilePicture.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b5e228` | `0x80441480` | bsim 12.4 | absent | BSIM | ProfilePicture::Update(...) | `Update__14ProfilePictureFv` |

### ProtocolRequestBroker.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Protocol/ProtocolRequestBroker.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a7b818` | `0x8006e280` | bsim 10.9 | absent | BSIM | Quazal::ProtocolRequestBroker::ProtocolRequestBroker(...) | `__ct__Q26Quazal21ProtocolRequestBrokerFPQ26Quazal21ProtocolRequestBroker` |

### RC4Encryption.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Plugins/RC4Encryption.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82ae6a20` | `0x80040240` | bsim 12.7 | confirmed | BSIM | Quazal::RC4Encryption::RC4Encryption(...) | `__ct__Q26Quazal13RC4EncryptionFv` |

### RTT.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Plugins/RTT.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b1eec0` | `0x8005ded0` | bsim 11.8 | absent | BSIM | Quazal::RTT::RTT(...) | `__ct__Q26Quazal3RTTFUi` |

### RangeDDL.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/ObjDup/RangeDDL.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b01438` | `0x800a6700` | bsim 11.5 | absent | BSIM | Quazal::_DS_Range const::FormatVariableValue(...) | `FormatVariableValue__Q26Quazal9_DS_RangeCFPQ26Quazal8VariablePQ26Quazal6String` |

### RootDODDL.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/ObjDup/RootDODDL.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a6b428` | `0x800a71f0` | bsim 10.2 | absent | BSIM | Quazal::_DO_RootDO::AddDuplicaLocation_OnMaster(...) | `AddDuplicaLocation_OnMaster__Q26Quazal10_DO_RootDOFQ26Quazal8DOHandleQ26Quazal8DOHandlebQ26Quazal8DOHandle` |

### RootTransport.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Plugins/RootTransport.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a9d0a0` | `0x8004b360` | bsim 14.4 | absent | BSIM | Quazal::PseudoGlobalVariable<Us>::AllocateExtraContexts(...) | `AllocateExtraContexts__Q26Quazal24PseudoGlobalVariable<Us>Fv` |

### RoutingAddressResolver.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Plugins/RoutingAddressResolver.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82ad1290` | `0x800642f0` | bsim 12.6 | absent | BSIM | Quazal::RoutingAddressResolver const::ResolveToAddress(...) | `ResolveToAddress__Q26Quazal22RoutingAddressResolverCFUsPQ26Quazal11InetAddress` |

### Scheduler.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Core/Scheduler.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a98c68` | `0x80030900` | bsim 14.6 | absent | BSIM | Quazal::Scheduler const::GetTotalQueueSize(...) | `GetTotalQueueSize__Q26Quazal9SchedulerCFv` |

### ScrollSelect.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/ui/ScrollSelect.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827f7d78` | `0x807ae210` | bsim 10.5 | absent | BSIM | ScrollSelect::Reset(...) | `Reset__12ScrollSelectFv` |

### SessionDiscoveryTable.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Plugins/SessionDiscoveryTable.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a940a0` | `0x800420f0` | bsim 13.6 | absent | BSIM | Quazal::SessionDiscoveryTable::SessionDiscoveryTable(...) | `__ct__Q26Quazal21SessionDiscoveryTableFv` |

### SessionInfo.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/ObjDup/SessionInfo.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a94030` | `0x800af710` | bsim 13.5 | absent | BSIM | Quazal::SessionInfo::GenerateSessionID(...) | `GenerateSessionID__Q26Quazal11SessionInfoFv` |

### SessionMessages.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/net/SessionMessages.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82599a88` | `0x80113e00` | bsim 10.9 | absent | BSIM | JoinResponseMsg const::Joined(...) | `Joined__15JoinResponseMsgCFv` |

### SessionSearcher.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/net/SessionSearcher.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x823d8328` | `0x80110d20` | bsim 13.4 | confirmed | BSIM | SessionSearcher::AllocateNetSearchResults(...) | `AllocateNetSearchResults__15SessionSearcherFv` |

### SessionState.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/ObjDup/SessionState.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a9ce60` | `0x800af9d0` | bsim 12.9 | absent | BSIM | Quazal::SessionState::SessionState(...) | `__ct__Q26Quazal12SessionStateFv` |

### SfxMap.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/synth/SfxMap.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8270bd48` | `0x809b5fb0` | bsim 10.1 | absent | BSIM | __rs__FR9BinStreamR6SfxMap | `__rs__FR9BinStreamR6SfxMap` |

### SlipTrack.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/synth/SlipTrack.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b7f718` | `0x809b6270` | bsim 11.4 | absent | BSIM | SlipTrack::ForceOn(...) | `ForceOn__9SlipTrackFv` |

### SlotChannelMapping.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/beatmatch/SlotChannelMapping.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x823de7b0` | `0x806474c0` | bsim 12.0 | absent | BSIM | SingleSlotChannelMapping::SingleSlotChannelMapping(...) | `__ct__24SingleSlotChannelMappingFi` |

### SongSectionController.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/bandobj/SongSectionController.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x822f9f58` | `0x80613070` | bsim 10.6 | absent | BSIM | SongSectionController::FindPoolCategoryForPracSession(...) | `FindPoolCategoryForPracSession__21SongSectionControllerF6Symbol` |

### Splash.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/movie/Splash.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8271d600` | `0x8078a020` | bsim 12.3 | absent | BSIM | Splash::EndSplasher(...) | `EndSplasher__6SplashFv` |

### SpotlightDrawer.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/world/SpotlightDrawer.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82c0b4e8` | `0x808710a0` | bsim 10.6 | absent | BSIM | __sinit_\SpotlightDrawer_cpp | `__sinit_\SpotlightDrawer_cpp` |

### StateMachine.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Core/StateMachine.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a840a8` | `0x80034fe0` | bsim 10.0 | absent | BSIM | Quazal::StateMachine::DispatchEvent(...) | `DispatchEvent__Q26Quazal12StateMachineFRCQ36Quazal12StateMachine6QEvent` |

### StationConnectionManager.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/ObjDup/StationConnectionManager.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aa3fd0` | `0x800b3630` | bsim 10.8 | absent | BSIM | Quazal::StationConnectionManager::DenyIncomingConnectionsFromNewStation(...) | `DenyIncomingConnectionsFromNewStation__Q26Quazal24StationConnectionManagerFv` |

### StepSequenceJob.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Core/StepSequenceJob.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aca5e8` | `0x80035c30` | bsim 10.7 | absent | BSIM | Quazal::StepSequenceJob::ResumeOnCallCompletion(...) | `ResumeOnCallCompletion__Q26Quazal15StepSequenceJobFPQ26Quazal11CallContextPQ36Quazal15StepSequenceJob4Step` |

### Str.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/utl/Str.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827994f0` | `0x804bbe10` | bsim 10.1 | absent | BSIM | String::insert(...) | `insert__6StringFUiPCc` |

### StreamManager.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Services/StreamManager.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aacbe0` | `0x800e0260` | bsim 12.8 | absent | BSIM | Quazal::StreamManager::StreamManager(...) | `__ct__Q26Quazal13StreamManagerFQ36Quazal6Stream4TypePQ26Quazal13RootTransport` |

### Symbols.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/utl/Symbols.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x825a8598` | `0x804be9f0` | bsim 10.8 | absent | BSIM | EndLiteralSymbols::EndLiteralSymbols(...) | `__ct__17EndLiteralSymbolsFv` |

### SynthSample.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/synth/SynthSample.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82532328` | `0x809c0dd0` | bsim 10.2 | absent | BSIM | SynthSample const::GetIsLooped(...) | `GetIsLooped__11SynthSampleCFv` |

### TDStretch.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/synthwii/soundtouch/TDStretch.cpp`  ·  DC3 shared  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b91e30` | `0x8097c4f0` | bsim 11.6 | absent | BSIM | soundtouch::TDStretch::clearMidBuffer(...) | `clearMidBuffer__Q210soundtouch9TDStretchFv` |

### Tex.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/rndobj/Tex.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82322570` | `0x8093af20` | bsim 10.0 | absent | BSIM | RndTex::RndTex(...) | `__ct__6RndTexFv` |

### TexRenderer.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/rndobj/TexRenderer.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82769df8` | `0x80943130` | bsim 13.3 | absent | BSIM | RndMesh const::GetKeepMeshData(...) | `GetKeepMeshData__7RndMeshCFv` |

### ThreadCall_Wii.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/os/ThreadCall_Wii.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82514458` | `0x80443df0` | bsim 11.1 | absent | BSIM | ThreadCallPreInit(...)   [free function] | `ThreadCallPreInit__Fv` |

### TicketManager.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Services/TicketManager.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a564c0` | `0x800d9ca0` | bsim 13.2 | absent | BSIM | Quazal::TicketManager::InsertTicket(...) | `InsertTicket__Q26Quazal13TicketManagerFUiPQ26Quazal6Ticket` |

### TimeConversion.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/utl/TimeConversion.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827a3f00` | `0x804d4ae0` | bsim 10.1 | absent | BSIM | BeatToTick(...)   [free function] | `BeatToTick__Ff` |

### UDPNetworkEmulator.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Plugins/UDPNetworkEmulator.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b0d8f0` | `0x800666d0` | bsim 13.9 | absent | BSIM | Quazal::NetworkEmulator::GetHead(...) | `GetHead__Q26Quazal15NetworkEmulatorFPPQ26Quazal6BufferPQ26Quazal11InetAddress` |

### UIListDir.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/ui/UIListDir.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827e5800` | `0x807dd870` | bsim 11.6 | absent | BSIM | UIListDir const::ElementSpacing(...) | `ElementSpacing__9UIListDirCFv` |

### UIScreen.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/ui/UIScreen.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826d0180` | `0x807f5c60` | bsim 10.8 | absent | BSIM | UIScreen::SetShowing(...) | `SetShowing__8UIScreenFb` |

### UISlider.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/ui/UISlider.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827e4360` | `0x807f7140` | bsim 10.0 | absent | BSIM | UISlider const::Current(...) | `Current__8UISliderCFv` |

### UpdateDataSetOperation.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/ObjDup/UpdateDataSetOperation.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a86f10` | `0x800b9500` | bsim 11.6 | absent | BSIM | Quazal::UpdateDataSetOperation::UpdateDataSetOperation(...) | `__ct__Q26Quazal22UpdateDataSetOperationFQ26Quazal8DOHandlePQ26Quazal16DuplicatedObjectPQ26Quazal7Message` |

### UpdatePolicy.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/ObjDup/UpdatePolicy.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a6d128` | `0x800b9e40` | bsim 10.9 | absent | BSIM | Quazal::UpdatePolicy::AddFilter(...) | `AddFilter__Q26Quazal12UpdatePolicyFPQ26Quazal18GlobalUpdateFilter` |

### UserMgr.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/os/UserMgr.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8250fd08` | `0x80447470` | bsim 10.8 | absent | BSIM | SetTheUserMgr(...)   [free function] | `SetTheUserMgr__FP7UserMgr` |

### VibratoDetector.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/dsp/VibratoDetector.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b52a08` | `0x80747640` | bsim 12.4 | absent | BSIM | VibratoDetector::Analyze(...) | `Analyze__15VibratoDetectorFf` |

### VoiceChannelDDL.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Extensions/VoiceChannelDDL.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a96fd8` | `0x800ccc70` | bsim 12.8 | absent | BSIM | Quazal::_DO_VoiceChannel::CallReclaimStream(...) | `CallReclaimStream__Q26Quazal16_DO_VoiceChannelFPQ26Quazal10RMCContextRCQ26Quazal8DOHandleRCUc` |

### VoiceChannelMemberDDL.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Extensions/VoiceChannelMemberDDL.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a6b770` | `0x800cd860` | bsim 10.6 | absent | BSIM | Quazal::_DDL_VoiceChannelMember::Extract(...) | `Extract__Q26Quazal23_DDL_VoiceChannelMemberFPQ26Quazal7MessagePQ26Quazal23_DDL_VoiceChannelMember` |

### VorbisMem.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/oggvorbis/VorbisMem.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82bfa808` | `0x80a146a0` | bsim 10.1 | absent | BSIM | OggRealloc | `OggRealloc` |

### WiiMessenger.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/net/WiiMessenger.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a8fa38` | `0x80127150` | bsim 10.6 | absent | BSIM | WiiMessenger::DeleteMessage(...) | `DeleteMessage__12WiiMessengerFi` |

### WiiProfileMgr.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/meta/WiiProfileMgr.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8266d3d8` | `0x807773e0` | bsim 10.8 | absent | BSIM | WiiProfileMgr::PostSave(...) | `PostSave__13WiiProfileMgrFv` |

### Wind.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/system/rndobj/Wind.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82364fc8` | `0x8096b100` | bsim 12.8 | absent | BSIM | RndWind::Replace(...) | `Replace__7RndWindFPQ23Hmx6ObjectPQ23Hmx6Object` |

### ZLibCompression.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/network/Plugins/ZLibCompression.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8282f008` | `0x8003cf90` | bsim 10.0 | absent | BSIM | QuazalCZlibAlloc | `QuazalCZlibAlloc` |

### UIFontImporter.o — system, 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 3)  ·  `src/system/ui/UIFontImporter.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827f4e38` | `0x807c7c30` | bsim 12.8 | contradicted | BSIM | UIFontImporter::FontImporterSyncObjects(...) | `FontImporterSyncObjects__14UIFontImporterFv` |
| `0x827f5340` | `0x807c83f0` | bsim 11.1 | contradicted | BSIM | UIFontImporter::OnAttachToImportFont(...) | `OnAttachToImportFont__14UIFontImporterFP9DataArray` |
| `0x827f5c58` | `0x807c8440` | bsim 11.1 | contradicted | BSIM | UIFontImporter::OnImportSettings(...) | `OnImportSettings__14UIFontImporterFP9DataArray` |

### NetSession.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 2)  ·  `src/network/net/NetSession.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x823ce578` | `0x8010bbb0` | bsim 10.9 | contradicted | BSIM | NetSession const::IsStartingGame(...) | `IsStartingGame__10NetSessionCFv` |
| `0x823d3848` | `0x80104710` | bsim 11.5 | contradicted | BSIM | NetSession::Disconnect(...) | `Disconnect__10NetSessionFv` |

### BandFaceDeform.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/system/bandobj/BandFaceDeform.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x828169c0` | `0x8050dd90` | bsim 10.1 | contradicted | BSIM | BandFaceDeform::DeltaArray::Clear(...) | `Clear__Q214BandFaceDeform10DeltaArrayFv` |

### BandHeadShaper.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/system/bandobj/BandHeadShaper.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8229e048` | `0x8055ac40` | bsim 14.2 | contradicted | BSIM | BandHeadShaper::AddFrame(...) | `AddFrame__14BandHeadShaperFPCcif` |

### BandWardrobe.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/system/bandobj/BandWardrobe.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82319c78` | `0x8055e4e0` | bsim 10.1 | contradicted | BSIM | BandWardrobe::GetCoopMode(...) | `GetCoopMode__12BandWardrobeFP11BandCamShot` |

### Bitmap.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/system/rndobj/Bitmap.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x823e97b0` | `0x808811a0` | bsim 12.1 | contradicted | BSIM | RndBitmap::DetachMip(...) | `DetachMip__9RndBitmapFv` |

### ByteStream.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/network/Plugins/ByteStream.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a6a608` | `0x8003f4d0` | bsim 12.0 | contradicted | BSIM | __rs__Q26Quazal10ByteStreamFRQ26Quazal6Buffer | `__rs__Q26Quazal10ByteStreamFRQ26Quazal6Buffer` |

### CacheMgr.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/system/utl/CacheMgr.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827a9b80` | `0x8048d120` | bsim 10.8 | contradicted | BSIM | CacheMgr::SetOp(...) | `SetOp__8CacheMgrFQ28CacheMgr6OpType` |

### ChordShapeGenerator.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/system/bandobj/ChordShapeGenerator.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827ccf00` | `0x8057b7f0` | bsim 11.6 | contradicted | BSIM | stlpmtx_std::map<Us,Us,Q211stlpmtx_std8less<Us>,Q211stlpmtx_std43StlNodeAlloc<Q211stlpmtx_std12pair<CUs,Us>>>::begin(...) | `begin__Q211stlpmtx_std96map<Us,Us,Q211stlpmtx_std8less<Us>,Q211stlpmtx_std43StlNodeAlloc<Q211stlpmtx_std12pair<CUs,Us>>>Fv` |

### Console.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/system/rndobj/Console.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82454360` | `0x80889bb0` | bsim 12.3 | contradicted | BSIM | RndConsole::Break(...) | `Break__10RndConsoleFP9DataArray` |

### CrowdMeterIcon.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/system/bandobj/CrowdMeterIcon.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x822a79c8` | `0x806065a0` | bsim 11.1 | contradicted | BSIM | CrowdMeterIcon::SetQuarantined(...) | `SetQuarantined__14CrowdMeterIconFb` |

### Dir.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/system/obj/Dir.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8272a240` | `0x80461fa0` | bsim 11.1 | contradicted | BSIM | ObjectDir::Reserve(...) | `Reserve__9ObjectDirFii` |

### FxSendDelay.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/system/synth/FxSendDelay.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x823b3188` | `0x8098f7a0` | bsim 13.1 | contradicted | BSIM | FxSendDelay::Copy(...) | `Copy__11FxSendDelayFPCQ23Hmx6ObjectQ33Hmx6Object8CopyType` |

### IDGenerator.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/network/ObjDup/IDGenerator.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82ac3228` | `0x8008c8f0` | bsim 12.1 | contradicted | BSIM | Quazal::IDGenerator::GenerateID(...) | `GenerateID__Q26Quazal11IDGeneratorFPUiUi` |

### IDGeneratorDDL.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/network/ObjDup/IDGeneratorDDL.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x824f7848` | `0x8008da40` | bsim 10.9 | contradicted | BSIM | Quazal::_DO_IDGenerator::RequestIDRangeFromMasterReturnStub(...) | `RequestIDRangeFromMasterReturnStub__Q26Quazal15_DO_IDGeneratorFPQ26Quazal10RMCContext` |

### LANSessionDiscovery.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/network/Plugins/LANSessionDiscovery.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aba560` | `0x80043bc0` | bsim 11.2 | contradicted | BSIM | Quazal::LANSessionDiscovery::ProcessProbeResponse(...) | `ProcessProbeResponse__Q26Quazal19LANSessionDiscoveryFPQ26Quazal10ByteStreamPCQ26Quazal10StationURL` |

### ProductFacade.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/network/Products/ProductFacade.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a62680` | `0x800fb000` | bsim 13.8 | contradicted | BSIM | Quazal::ProductFacade::Terminate(...) | `Terminate__Q26Quazal13ProductFacadeFPQ26Quazal11CallContext` |

### PropKeys.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/system/rndobj/PropKeys.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x824191b0` | `0x80925d90` | bsim 13.6 | contradicted | BSIM | __rs<6Symbol>__FR9BinStreamR12Key<6Symbol>_R9BinStream | `__rs<6Symbol>__FR9BinStreamR12Key<6Symbol>_R9BinStream` |

### QueuingSocket.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/network/Plugins/QueuingSocket.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b0bdc8` | `0x80068f40` | bsim 11.0 | contradicted | BSIM | Quazal::QueuingSocket::Recv(...) | `Recv__Q26Quazal13QueuingSocketFUi` |

### ServerProtocol.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/network/Protocol/ServerProtocol.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a7a538` | `0x80070970` | bsim 10.8 | contradicted | BSIM | Quazal::ServerProtocol::DispatchCallRequest(...) | `DispatchCallRequest__Q26Quazal14ServerProtocolFPQ26Quazal7MessagePQ26Quazal7MessagePbPQ26Quazal8EndPoint` |

### SessionClockDDL.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/network/Extensions/SessionClockDDL.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aa4ef0` | `0x800c7190` | bsim 11.8 | contradicted | BSIM | Quazal::_DO_SessionClock::CallSyncRequest(...) | `CallSyncRequest__Q26Quazal16_DO_SessionClockFPQ26Quazal10RMCContextRCUx` |

### SongPreview.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/system/meta/SongPreview.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82781320` | `0x8075f550` | bsim 14.0 | contradicted | BSIM | SongPreview::Poll(...) | `Poll__11SongPreviewFv` |

### VoiceBeat.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/system/synth/VoiceBeat.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826fa7d8` | `0x809c3e70` | bsim 14.7 | contradicted | BSIM | TalkyMatcher::TalkyMatcher(...) | `__ct__12TalkyMatcherFv` |

---

## For the next agent

- **Regenerate:** `python3 tools/gen_sysnet_port_worklist.py` (cwd-independent; reads `ghidriff_identities.json` + `scripts/target_symbol_map.json` + the rb3 CW map; the script VERIFIES every `wii_symbol` resolves to its claimed Bank-8 addr and that 0 entries are already in the production map, exiting non-zero on any failure).
- **Data feed:** `sysnet_port_worklist.json` — per-fn rows + `tu_summary` + `ranked_tus` for machine ingestion.
- **Port the safe-first core first** (HIGH + BSim≥30, human-judged 1.000), then the BSim 20–30 tier, then **confirm-on-consume each BSim 15–20 row** before trusting its name.
- **Do NOT inject these into `target_symbol_map.json`** — CW≠MSVC mangling, TUs uncompiled; wrong key mis-pairs objdiff. Confirm each name when the TU is actually ported (`gen_game_target_map.py --tu <TU>`).
- **DC3 first for shared rows.** For `DC3?=shared` engine TUs, DC3's already-decomp'd body (`/dc3-pair`, BinDiff) is the faster base; this worklist's value is highest on the `DC3?=cannot-provide` Quazal/ObjDup netcode rows.
- **Watch the dominant failure mode:** same-TU sibling aliasing (the lone miss was `TrackWidget::Init` vs `::Empty`, vtable slot 0x44 vs 0xc). It bites hardest in **bsim15-20**.
