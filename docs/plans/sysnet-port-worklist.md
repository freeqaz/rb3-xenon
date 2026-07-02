# system/network porting worklist — net-new Wii→Xenon identities

**Generated:** `tools/gen_sysnet_port_worklist.py` (regenerable). **Source:** `ghidriff_identities.json` (ACCEPT tier) minus `scripts/target_symbol_map.json`, `category ∈ {system, network}`.
**Data feed:** `sysnet_port_worklist.json` (machine-readable, one row per fn; gitignored/regenerable).

## What this is

290 RB3 **engine + netcode** functions across **192 TUs** (**88 system** + **202 network**), each pinned to a specific Wii (Bank-8, CodeWarrior-mangled) function by the forked-ghidriff/BSim Wii→Xenon identity pipeline. These are **net-new**: their Xenon address is NOT yet in the production pairing set (`target_symbol_map.json`), re-derived against the **live** map on each regen.

These ~530 identities were **human-validated at 0.967 precision** (system 14/15 = 0.933, network 15/15 = 1.000; **HIGH + BSim≥30 core = 11/11 = 1.000**), clearing the ≥0.85 handoff bar — so, like band3, they get a worklist. This is the **second-priority** lever behind band3: much of system/network is shared Milo engine + Quazal netcode where **DC3 BinDiff also helps**, so the marginal value is lower even though precision is higher.

**This is a targeting/porting worklist + per-fn identity oracle, NOT a `target_symbol_map.json` injection.** Many TUs aren't compiled yet (no MSVC symbol to pair), and our `wii_symbol` is CW/MWCC-mangled, not MSVC-mangled — injecting it as a map key would mis-pair objdiff (actively harmful). Use this to pick which engine/netcode TU to port next and to name each function from the Wii body. Both outputs are additive + reversible.

## Safe-first slice + the confirm-on-consume tier

- **Safe-first core = HIGH + BSim≥30: 46 rows** (8 high + 38 bsim≥30). This is the human-judged-1.000 slice — port/name these with the most trust. Table below.
- **BSim 15–20 = confirm-on-consume.** That band holds the **only** measured miss across the 30-pair sample: `TrackWidget::Init` was aliased to its sibling `TrackWidget::Empty` (the two 20-byte `mImp->virtual()` forwarders differ ONLY in the vtable-slot immediate — Init forwards through slot `0x44`, Empty through `0xc`). **Verify each BSim 15–20 name per-fn when a porter actually consumes it** (diff vtable-slot / type-tag / node-size immediates + referenced strings + resolved callees against the Wii body).

## DC3 reachability

Most system/network is shared Milo engine that **DC3 can supply** (DC3 is the same engine on the same Xbox 360 toolchain — `dc3_cannot_provide` defaults **False**). **144 rows are flagged genuinely DC3-unreachable** (`dc3_cannot_provide=true`): their rb3 source file is absent AND no same-named `.cpp` exists in the DC3 tree — overwhelmingly the Quazal/ObjDup netcode proprietary to RB3's Wii build with no DC3 twin. Those rows mark `DC3?=cannot-provide` in the rosters; all others mark `DC3?=shared`.

## Confidence strata (the measured prior)

system/network human-judged precision (n=30) = **0.967**. Totals here: **8 high** · **38 bsim≥30** · **113 bsim20-30** · **131 bsim15-20**.

- **high** — `ExactInstructions`/`SwitchSig`/`Implied`/`SymbolsHash`, or BSim simconf ≥ 30. The safest-first targets.
- **bsim≥30 / bsim20-30 / bsim15-20** — BSim similarity×confidence bands; lower = vet harder. **bsim15-20 = confirm-on-consume.**

## Safe-first subset — HIGH + BSim≥30 (verify these names with most trust)

| Xenon addr | cat | TU | src | confidence | match | Wii signature | wii_symbol | DC3? |
|---|---|---|---|---|---|---|---|---|
| `0x823eda20` | system | Anim.o | `src/system/rndobj/Anim.cpp` | high | ExactInstructionsFunctionHasher | RndAnimatable const::Units(...) | `Units__13RndAnimatableCFv` | shared |
| `0x823871f0` | system | CharIKScale.o | `src/system/char/CharIKScale.cpp` | high | ExactInstructionsFunctionHasher | CharIKScale::CaptureBefore(...) | `CaptureBefore__11CharIKScaleFv` | shared |
| `0x82757000` | system | MasterAudio.o | `src/system/beatmatch/MasterAudio.cpp` | high | Implied Match | MasterAudio::SetVocalCueFader(...) | `SetVocalCueFader__11MasterAudioFf` | shared |
| `0x8276e798` | system | PhraseList.o | `src/system/beatmatch/PhraseList.cpp` | high | ExactInstructionsFunctionHasher | PhraseListCollection::AddPhrase(...) | `AddPhrase__20PhraseListCollectionF19BeatmatchPhraseTypefifi` | shared |
| `0x82837880` | system | RGState.o | `src/system/beatmatch/RGState.cpp` | high | ExactInstructionsFunctionHasher | __as__7RGStateFRC7RGState | `__as__7RGStateFRC7RGState` | shared |
| `0x8254f660` | system | Str.o | `src/system/utl/Str.cpp` | high | ExactInstructionsFunctionHasher | String::insert(...) | `insert__6StringFUiRC6String` | shared |
| `0x827bb458` | system | TrackWidget.o | `src/system/track/TrackWidget.cpp` | high | Implied Match | TrackWidget::Init(...) | `Init__11TrackWidgetFv` | shared |
| `0x82b292b0` | system | deflate.o | `src/system/zlib/deflate.cpp` | high | ExactInstructionsFunctionHasher | deflateInit_ | `deflateInit_` | cannot-provide |
| `0x82a9ee90` | network | Authentication.o | `src/network/ObjDup/Authentication.cpp` | bsim 34 | BSIM | Quazal::ProcessAuthentication::Authenticate(...) | `Authenticate__Q26Quazal21ProcessAuthenticationFRCQ26Quazal21ProcessAuthentication` | cannot-provide |
| `0x82a5a660` | network | BackEndServices.o | `src/network/Services/BackEndServices.cpp` | bsim 31 | BSIM | Quazal::BackEndServices::BackEndServices(...) | `__ct__Q26Quazal15BackEndServicesFv` | cannot-provide |
| `0x82a45a78` | network | DuplicatedObject.o | `src/network/ObjDup/DuplicatedObject.cpp` | bsim 35 | BSIM | Quazal::DuplicatedObject::Publish(...) | `Publish__Q26Quazal16DuplicatedObjectFUi` | shared |
| `0x82af8f88` | network | DuplicationSpace.o | `src/network/Extensions/DuplicationSpace.cpp` | bsim 38 | BSIM | Quazal::DuplicationSpace::OperationEndMatchTrigger(...) | `OperationEndMatchTrigger__Q26Quazal16DuplicationSpaceFPQ26Quazal11DOOperation` | cannot-provide |
| `0x82b18c08` | network | DuplicationSpaceTable.o | `src/network/Extensions/DuplicationSpaceTable.cpp` | bsim 31 | BSIM | Quazal::DuplicationSpaceTable::OperationEndMatchTrigger(...) | `OperationEndMatchTrigger__Q26Quazal21DuplicationSpaceTableFPQ26Quazal11DOOperation` | cannot-provide |
| `0x82af4eb8` | network | EncryptionAlgorithm.o | `src/network/Plugins/EncryptionAlgorithm.cpp` | bsim 36 | BSIM | Quazal::EncryptionAlgorithm::SetKey(...) | `SetKey__Q26Quazal19EncryptionAlgorithmFRCQ26Quazal3Key` | shared |
| `0x82ac8950` | network | EndPoint.o | `src/network/Plugins/EndPoint.cpp` | bsim 34 | BSIM | Quazal::EndPoint::SetConnectionID(...) | `SetConnectionID__Q26Quazal8EndPointFUi` | cannot-provide |
| `0x82ac8a08` | network | EndPoint.o | `src/network/Plugins/EndPoint.cpp` | bsim 34 | BSIM | Quazal::EndPoint::SetPrincipalID(...) | `SetPrincipalID__Q26Quazal8EndPointFUi` | cannot-provide |
| `0x82a91638` | network | FetchContext.o | `src/network/ObjDup/FetchContext.cpp` | bsim 35 | BSIM | Quazal::FetchContext::FetchDuplicaImpl(...) | `FetchDuplicaImpl__Q26Quazal12FetchContextFQ26Quazal8DOHandle` | cannot-provide |
| `0x82b14400` | network | JobConnectSecureEndPoint.o | `src/network/Services/JobConnectSecureEndPoint.cpp` | bsim 35 | BSIM | Quazal::JobConnectSecureEndPoint::RequestConnectionData(...) | `RequestConnectionData__Q26Quazal24JobConnectSecureEndPointFv` | cannot-provide |
| `0x82a88d60` | network | JobConnectStation.o | `src/network/ObjDup/JobConnectStation.cpp` | bsim 33 | BSIM | Quazal::JobConnectStation::WaitForURLs(...) | `WaitForURLs__Q26Quazal17JobConnectStationFv` | cannot-provide |
| `0x82a9c718` | network | JobJoinSession.o | `src/network/ObjDup/JobJoinSession.cpp` | bsim 41 | BSIM | Quazal::JobJoinSession::ProcessPositiveJoinResponse(...) | `ProcessPositiveJoinResponse__Q26Quazal14JobJoinSessionFUcQ26Quazal8DOHandleQ26Quazal8DOHandle` | cannot-provide |
| `0x82ac1098` | network | JobProcessJoinRequest.o | `src/network/ObjDup/JobProcessJoinRequest.cpp` | bsim 42 | BSIM | Quazal::JobProcessJoinRequest::InitiateConnection(...) | `InitiateConnection__Q26Quazal21JobProcessJoinRequestFv` | cannot-provide |
| `0x82b19df8` | network | MatchOperation.o | `src/network/Extensions/MatchOperation.cpp` | bsim 30 | BSIM | Quazal::MatchOperation::ExecuteQueuedOperation(...) | `ExecuteQueuedOperation__Q26Quazal14MatchOperationFi` | cannot-provide |
| `0x82ad62a0` | network | NATTraversalEngine.o | `src/network/Plugins/NATTraversalEngine.cpp` | bsim 54 | BSIM | Quazal::NATTraversalEngine::SendProbe(...) | `SendProbe__Q26Quazal18NATTraversalEngineFQ36Quazal18NATTraversalEngine3MsgRCQ26Quazal10StationURLQ26Quazal4Time` | cannot-provide |
| `0x82a66cc8` | network | ObjDupProtocol.o | `src/network/ObjDup/ObjDupProtocol.cpp` | bsim 33 | BSIM | Quazal::ObjDupProtocol::ProcessJoinResponse(...) | `ProcessJoinResponse__Q26Quazal14ObjDupProtocolFPQ26Quazal7MessageRUc` | cannot-provide |
| `0x82b05480` | network | PRUDPEndPoint.o | `src/network/Plugins/PRUDPEndPoint.cpp` | bsim 48 | BSIM | Quazal::PRUDPEndPoint::Defrag(...) | `Defrag__Q26Quazal13PRUDPEndPointFPQ26Quazal8PacketIn` | cannot-provide |
| `0x82b0dfe8` | network | Packet.o | `src/network/Plugins/Packet.cpp` | bsim 32 | BSIM | Quazal::Packet::Packet(...) | `__ct__Q26Quazal6PacketFv` | cannot-provide |
| `0x82a9a220` | network | PacketQueue.o | `src/network/Plugins/PacketQueue.cpp` | bsim 39 | BSIM | Quazal::PacketQueue::Dequeue(...) | `Dequeue__Q26Quazal11PacketQueueFQ36Quazal74qChain<PQ26Quazal6Packet,Q26Quazal37DefaultChainPolicy<PQ26Quazal6Packet>>8iterator` | cannot-provide |
| `0x82a7be58` | network | ProtocolRequestBroker.o | `src/network/Protocol/ProtocolRequestBroker.cpp` | bsim 35 | BSIM | Quazal::ProtocolRequestBroker::ProcessMessageCore(...) | `ProcessMessageCore__Q26Quazal21ProtocolRequestBrokerFPQ26Quazal27CallProtocolMethodOperationPQ26Quazal8EndPointPQ26Quazal6Buffer` | cannot-provide |
| `0x82a83a18` | network | PseudoGlobalVariableList.o | `src/network/Core/PseudoGlobalVariableList.cpp` | bsim 38 | BSIM | Quazal::PseudoGlobalVariableList::AddVariable(...) | `AddVariable__Q26Quazal24PseudoGlobalVariableListFPQ26Quazal24PseudoGlobalVariableRoot` | shared |
| `0x82b0bcc8` | network | QueuingSocket.o | `src/network/Plugins/QueuingSocket.cpp` | bsim 33 | BSIM | Quazal::QueuingSocket::CompleteSend(...) | `CompleteSend__Q26Quazal13QueuingSocketFv` | cannot-provide |
| `0x82a50480` | network | SessionClock.o | `src/network/Extensions/SessionClock.cpp` | bsim 38 | BSIM | Quazal::SessionClock::SessionClock(...) | `__ct__Q26Quazal12SessionClockFv` | cannot-provide |
| `0x82a9cf78` | network | SessionState.o | `src/network/ObjDup/SessionState.cpp` | bsim 47 | BSIM | Quazal::SessionState::OperationEnd(...) | `OperationEnd__Q26Quazal12SessionStateFPCQ26Quazal11DOOperation` | cannot-provide |
| `0x82a93da0` | network | SharedSessionDescription.o | `src/network/ObjDup/SharedSessionDescription.cpp` | bsim 30 | BSIM | Quazal::SharedSessionDescription::PullSharedSessionDescription(...) | `PullSharedSessionDescription__Q26Quazal24SharedSessionDescriptionFv` | cannot-provide |
| `0x82a4dfd8` | network | Station.o | `src/network/ObjDup/Station.cpp` | bsim 59 | BSIM | Quazal::Station::ValidOperation(...) | `ValidOperation__Q26Quazal7StationFPQ26Quazal11DOOperation` | cannot-provide |
| `0x82ab8230` | network | StringConversion.o | `src/network/Platform/StringConversion.cpp` | bsim 36 | BSIM | @unnamed@StringConversion_cpp@::Utf8ToLatin1(...) | `Utf8ToLatin1__30@unnamed@StringConversion_cpp@FPCcPcUi` | shared |
| `0x82a53440` | network | SystemError.o | `src/network/Platform/SystemError.cpp` | bsim 34 | BSIM | Quazal::SystemError::GetErrorString(...) | `GetErrorString__Q26Quazal11SystemErrorFUiPcUi` | cannot-provide |
| `0x82ae7b58` | network | UDPTransport.o | `src/network/Plugins/UDPTransport.cpp` | bsim 38 | BSIM | Quazal::UDPTransport::StartEventListener(...) | `StartEventListener__Q26Quazal12UDPTransportFv` | cannot-provide |
| `0x8229e690` | system | BandHeadShaper.o | `src/system/bandobj/BandHeadShaper.cpp` | bsim 31 | BSIM | BandHeadShaper::End(...) | `End__14BandHeadShaperFv` | shared |
| `0x8236a5f8` | system | CharClip.o | `src/system/char/CharClip.cpp` | bsim 37 | BSIM | CharClip::BeatAlignString(...) | `BeatAlignString__8CharClipFi` | shared |
| `0x824b8a28` | system | Dir.o | `src/system/world/Dir.cpp` | bsim 48 | BSIM | WorldDir::DrawShowing(...) | `DrawShowing__8WorldDirFv` | shared |
| `0x82775b68` | system | JoypadGuitarController.o | `src/system/beatmatch/JoypadGuitarController.cpp` | bsim 34 | BSIM | JoypadGuitarController::ReconcileFretState(...) | `ReconcileFretState__22JoypadGuitarControllerFv` | shared |
| `0x827a6db0` | system | LogFile.o | `src/system/utl/LogFile.cpp` | bsim 34 | BSIM | LogFile::Print(...) | `Print__7LogFileFPCc` | shared |
| `0x8271fb70` | system | Movie.o | `src/system/movie/Movie.cpp` | bsim 39 | BSIM | Movie::Impl::End(...) | `End__Q25Movie4ImplFv` | shared |
| `0x822fad50` | system | SongSectionController.o | `src/system/bandobj/SongSectionController.cpp` | bsim 30 | BSIM | SongSectionController::DebugActivate(...) | `DebugActivate__21SongSectionControllerFv` | shared |
| `0x82b42308` | system | TDStretch.o | `src/system/synthwii/soundtouch/TDStretch.cpp` | bsim 40 | BSIM | soundtouch::TDStretch::seekBestOverlapPosition(...) | `seekBestOverlapPosition__Q210soundtouch9TDStretchFPCs` | shared |
| `0x82446f08` | system | Text.o | `src/system/rndobj/Text.cpp` | bsim 35 | BSIM | RndText::SetAltSizeAndZOffset(...) | `SetAltSizeAndZOffset__7RndTextFff` | shared |

## TU ranking (port these first — by #high+#bsim≥30 desc, then total desc)

| Rank | cat | TU | src | #ids | high | ≥30 | 20-30 | 15-20 | 10-15 | contra | DC3? |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | network | EndPoint.o | `src/network/Plugins/EndPoint.cpp` | 3 | 0 | 2 | 0 | 1 | 0 | 0 | cannot-provide |
| 2 | network | DuplicationSpace.o | `src/network/Extensions/DuplicationSpace.cpp` | 6 | 0 | 1 | 4 | 1 | 0 | 0 | cannot-provide |
| 3 | network | DuplicatedObject.o | `src/network/ObjDup/DuplicatedObject.cpp` | 5 | 0 | 1 | 0 | 4 | 0 | 0 | shared |
| 4 | network | ObjDupProtocol.o | `src/network/ObjDup/ObjDupProtocol.cpp` | 4 | 0 | 1 | 2 | 1 | 0 | 0 | cannot-provide |
| 5 | network | Station.o | `src/network/ObjDup/Station.cpp` | 4 | 0 | 1 | 3 | 0 | 0 | 0 | cannot-provide |
| 6 | system | Text.o | `src/system/rndobj/Text.cpp` | 4 | 0 | 1 | 1 | 2 | 0 | 0 | shared |
| 7 | network | Authentication.o | `src/network/ObjDup/Authentication.cpp` | 3 | 0 | 1 | 2 | 0 | 0 | 0 | cannot-provide |
| 8 | network | BackEndServices.o | `src/network/Services/BackEndServices.cpp` | 3 | 0 | 1 | 1 | 1 | 0 | 0 | cannot-provide |
| 9 | system | Dir.o | `src/system/rndobj/Dir.cpp` | 3 | 0 | 1 | 0 | 2 | 0 | 0 | shared |
| 10 | network | JobJoinSession.o | `src/network/ObjDup/JobJoinSession.cpp` | 3 | 0 | 1 | 0 | 2 | 0 | 0 | cannot-provide |
| 11 | network | NATTraversalEngine.o | `src/network/Plugins/NATTraversalEngine.cpp` | 3 | 0 | 1 | 1 | 1 | 0 | 0 | cannot-provide |
| 12 | system | Anim.o | `src/system/rndobj/Anim.cpp` | 2 | 1 | 0 | 0 | 1 | 0 | 0 | shared |
| 13 | system | BandHeadShaper.o | `src/system/bandobj/BandHeadShaper.cpp` | 2 | 0 | 1 | 0 | 1 | 0 | 0 | shared |
| 14 | network | DuplicationSpaceTable.o | `src/network/Extensions/DuplicationSpaceTable.cpp` | 2 | 0 | 1 | 1 | 0 | 0 | 0 | cannot-provide |
| 15 | network | JobConnectStation.o | `src/network/ObjDup/JobConnectStation.cpp` | 2 | 0 | 1 | 1 | 0 | 0 | 0 | cannot-provide |
| 16 | network | JobProcessJoinRequest.o | `src/network/ObjDup/JobProcessJoinRequest.cpp` | 2 | 0 | 1 | 1 | 0 | 0 | 0 | cannot-provide |
| 17 | network | MatchOperation.o | `src/network/Extensions/MatchOperation.cpp` | 2 | 0 | 1 | 0 | 1 | 0 | 0 | cannot-provide |
| 18 | network | QueuingSocket.o | `src/network/Plugins/QueuingSocket.cpp` | 2 | 0 | 1 | 1 | 0 | 0 | 0 | cannot-provide |
| 19 | network | SessionClock.o | `src/network/Extensions/SessionClock.cpp` | 2 | 0 | 1 | 0 | 1 | 0 | 0 | cannot-provide |
| 20 | network | SessionState.o | `src/network/ObjDup/SessionState.cpp` | 2 | 0 | 1 | 1 | 0 | 0 | 0 | cannot-provide |
| 21 | network | SharedSessionDescription.o | `src/network/ObjDup/SharedSessionDescription.cpp` | 2 | 0 | 1 | 0 | 1 | 0 | 0 | cannot-provide |
| 22 | network | UDPTransport.o | `src/network/Plugins/UDPTransport.cpp` | 2 | 0 | 1 | 1 | 0 | 0 | 0 | cannot-provide |
| 23 | system | CharClip.o | `src/system/char/CharClip.cpp` | 1 | 0 | 1 | 0 | 0 | 0 | 0 | shared |
| 24 | system | CharIKScale.o | `src/system/char/CharIKScale.cpp` | 1 | 1 | 0 | 0 | 0 | 0 | 0 | shared |
| 25 | network | EncryptionAlgorithm.o | `src/network/Plugins/EncryptionAlgorithm.cpp` | 1 | 0 | 1 | 0 | 0 | 0 | 0 | shared |
| 26 | network | FetchContext.o | `src/network/ObjDup/FetchContext.cpp` | 1 | 0 | 1 | 0 | 0 | 0 | 0 | cannot-provide |
| 27 | network | JobConnectSecureEndPoint.o | `src/network/Services/JobConnectSecureEndPoint.cpp` | 1 | 0 | 1 | 0 | 0 | 0 | 0 | cannot-provide |
| 28 | system | JoypadGuitarController.o | `src/system/beatmatch/JoypadGuitarController.cpp` | 1 | 0 | 1 | 0 | 0 | 0 | 0 | shared |
| 29 | system | LogFile.o | `src/system/utl/LogFile.cpp` | 1 | 0 | 1 | 0 | 0 | 0 | 0 | shared |
| 30 | system | MasterAudio.o | `src/system/beatmatch/MasterAudio.cpp` | 1 | 1 | 0 | 0 | 0 | 0 | 0 | shared |
| 31 | system | Movie.o | `src/system/movie/Movie.cpp` | 1 | 0 | 1 | 0 | 0 | 0 | 0 | shared |
| 32 | network | PRUDPEndPoint.o | `src/network/Plugins/PRUDPEndPoint.cpp` | 1 | 0 | 1 | 0 | 0 | 0 | 0 | cannot-provide |
| 33 | network | Packet.o | `src/network/Plugins/Packet.cpp` | 1 | 0 | 1 | 0 | 0 | 0 | 0 | cannot-provide |
| 34 | network | PacketQueue.o | `src/network/Plugins/PacketQueue.cpp` | 1 | 0 | 1 | 0 | 0 | 0 | 0 | cannot-provide |
| 35 | system | PhraseList.o | `src/system/beatmatch/PhraseList.cpp` | 1 | 1 | 0 | 0 | 0 | 0 | 0 | shared |
| 36 | network | ProtocolRequestBroker.o | `src/network/Protocol/ProtocolRequestBroker.cpp` | 1 | 0 | 1 | 0 | 0 | 0 | 0 | cannot-provide |
| 37 | network | PseudoGlobalVariableList.o | `src/network/Core/PseudoGlobalVariableList.cpp` | 1 | 0 | 1 | 0 | 0 | 0 | 0 | shared |
| 38 | system | RGState.o | `src/system/beatmatch/RGState.cpp` | 1 | 1 | 0 | 0 | 0 | 0 | 0 | shared |
| 39 | system | SongSectionController.o | `src/system/bandobj/SongSectionController.cpp` | 1 | 0 | 1 | 0 | 0 | 0 | 0 | shared |
| 40 | system | Str.o | `src/system/utl/Str.cpp` | 1 | 1 | 0 | 0 | 0 | 0 | 0 | shared |
| 41 | network | StringConversion.o | `src/network/Platform/StringConversion.cpp` | 1 | 0 | 1 | 0 | 0 | 0 | 0 | shared |
| 42 | network | SystemError.o | `src/network/Platform/SystemError.cpp` | 1 | 0 | 1 | 0 | 0 | 0 | 0 | cannot-provide |
| 43 | system | TDStretch.o | `src/system/synthwii/soundtouch/TDStretch.cpp` | 1 | 0 | 1 | 0 | 0 | 0 | 0 | shared |
| 44 | system | TrackWidget.o | `src/system/track/TrackWidget.cpp` | 1 | 1 | 0 | 0 | 0 | 0 | 0 | shared |
| 45 | system | deflate.o | `src/system/zlib/deflate.cpp` | 1 | 1 | 0 | 0 | 0 | 0 | 0 | cannot-provide |
| 46 | network | Session.o | `src/network/ObjDup/Session.cpp` | 5 | 0 | 0 | 2 | 3 | 0 | 0 | cannot-provide |
| 47 | system | BandCharDesc.o | `src/system/bandobj/BandCharDesc.cpp` | 4 | 0 | 0 | 3 | 1 | 0 | 0 | shared |
| 48 | network | CallContext.o | `src/network/Core/CallContext.cpp` | 4 | 0 | 0 | 2 | 2 | 0 | 0 | shared |
| 49 | network | WKHandle.o | `src/network/ObjDup/WKHandle.cpp` | 4 | 0 | 0 | 3 | 1 | 0 | 0 | cannot-provide |
| 50 | network | DOCoreTypes.o | `src/network/ObjDup/DOCoreTypes.cpp` | 3 | 0 | 0 | 3 | 0 | 0 | 0 | cannot-provide |
| 51 | network | PromotionRefereeDDL.o | `src/network/ObjDup/PromotionRefereeDDL.cpp` | 3 | 0 | 0 | 1 | 2 | 0 | 0 | cannot-provide |
| 52 | network | Protocol.o | `src/network/Protocol/Protocol.cpp` | 3 | 0 | 0 | 1 | 2 | 0 | 0 | shared |
| 53 | network | SessionDDL.o | `src/network/ObjDup/SessionDDL.cpp` | 3 | 0 | 0 | 3 | 0 | 0 | 0 | cannot-provide |
| 54 | system | UILabel.o | `src/system/ui/UILabel.cpp` | 3 | 0 | 0 | 1 | 2 | 0 | 0 | shared |
| 55 | network | AuthenticationClient.o | `src/network/Services/AuthenticationClient.cpp` | 2 | 0 | 0 | 1 | 1 | 0 | 0 | cannot-provide |
| 56 | system | BandFaceDeform.o | `src/system/bandobj/BandFaceDeform.cpp` | 2 | 0 | 0 | 1 | 1 | 0 | 0 | shared |
| 57 | system | BeatMatchUtl.o | `src/system/beatmatch/BeatMatchUtl.cpp` | 2 | 0 | 0 | 1 | 1 | 0 | 0 | shared |
| 58 | network | Buffer.o | `src/network/Plugins/Buffer.cpp` | 2 | 0 | 0 | 1 | 1 | 0 | 0 | shared |
| 59 | network | ByteStream.o | `src/network/Plugins/ByteStream.cpp` | 2 | 0 | 0 | 2 | 0 | 0 | 0 | shared |
| 60 | system | CameraManager.o | `src/system/world/CameraManager.cpp` | 2 | 0 | 0 | 1 | 1 | 0 | 0 | shared |
| 61 | system | ChordShapeGenerator.o | `src/system/bandobj/ChordShapeGenerator.cpp` | 2 | 0 | 0 | 0 | 2 | 0 | 0 | shared |
| 62 | network | ClientProtocol.o | `src/network/Protocol/ClientProtocol.cpp` | 2 | 0 | 0 | 2 | 0 | 0 | 0 | cannot-provide |
| 63 | network | DOCallContext.o | `src/network/ObjDup/DOCallContext.cpp` | 2 | 0 | 0 | 0 | 2 | 0 | 0 | cannot-provide |
| 64 | system | DrumTrackWatcherImpl.o | `src/system/beatmatch/DrumTrackWatcherImpl.cpp` | 2 | 0 | 0 | 1 | 1 | 0 | 0 | shared |
| 65 | network | InstanceControl.o | `src/network/Core/InstanceControl.cpp` | 2 | 0 | 0 | 0 | 2 | 0 | 0 | shared |
| 66 | network | InstantiationContext.o | `src/network/Core/InstantiationContext.cpp` | 2 | 0 | 0 | 1 | 1 | 0 | 0 | shared |
| 67 | network | IteratorOverDOs.o | `src/network/ObjDup/IteratorOverDOs.cpp` | 2 | 0 | 0 | 1 | 1 | 0 | 0 | cannot-provide |
| 68 | network | Job.o | `src/network/Core/Job.cpp` | 2 | 0 | 0 | 1 | 1 | 0 | 0 | shared |
| 69 | network | JobBackEndServicesLogin.o | `src/network/Services/JobBackEndServicesLogin.cpp` | 2 | 0 | 0 | 0 | 2 | 0 | 0 | cannot-provide |
| 70 | network | JobBackEndServicesLogout.o | `src/network/Services/JobBackEndServicesLogout.cpp` | 2 | 0 | 0 | 2 | 0 | 0 | 0 | cannot-provide |
| 71 | network | JobChangeConnection.o | `src/network/ObjDup/JobChangeConnection.cpp` | 2 | 0 | 0 | 0 | 2 | 0 | 0 | cannot-provide |
| 72 | network | JobTerminateFacade.o | `src/network/Products/JobTerminateFacade.cpp` | 2 | 0 | 0 | 1 | 1 | 0 | 0 | cannot-provide |
| 73 | network | Jobs_Wii.o | `src/network/net/Jobs_Wii.cpp` | 2 | 0 | 0 | 2 | 0 | 0 | 0 | shared |
| 74 | system | Joypad.o | `src/system/os/Joypad.cpp` | 2 | 0 | 0 | 1 | 1 | 0 | 0 | shared |
| 75 | network | KerberosAuthentication.o | `src/network/Services/KerberosAuthentication.cpp` | 2 | 0 | 0 | 1 | 1 | 0 | 0 | cannot-provide |
| 76 | network | MasterStationRef.o | `src/network/ObjDup/MasterStationRef.cpp` | 2 | 0 | 0 | 0 | 2 | 0 | 0 | cannot-provide |
| 77 | system | MidiInstrumentMgr.o | `src/system/synth/MidiInstrumentMgr.cpp` | 2 | 0 | 0 | 1 | 1 | 0 | 0 | shared |
| 78 | network | PRUDPStream.o | `src/network/Plugins/PRUDPStream.cpp` | 2 | 0 | 0 | 1 | 1 | 0 | 0 | cannot-provide |
| 79 | system | RealGuitarTrackWatcherImpl.o | `src/system/beatmatch/RealGuitarTrackWatcherImpl.cpp` | 2 | 0 | 0 | 1 | 1 | 0 | 0 | shared |
| 80 | network | Scheduler.o | `src/network/Core/Scheduler.cpp` | 2 | 0 | 0 | 2 | 0 | 0 | 0 | shared |
| 81 | network | SecureEndPoint.o | `src/network/Services/SecureEndPoint.cpp` | 2 | 0 | 0 | 0 | 2 | 0 | 0 | cannot-provide |
| 82 | network | SessionDiscoveryTable.o | `src/network/Plugins/SessionDiscoveryTable.cpp` | 2 | 0 | 0 | 0 | 2 | 0 | 0 | cannot-provide |
| 83 | network | SessionSearcher_RV.o | `src/network/net/SessionSearcher_RV.cpp` | 2 | 0 | 0 | 2 | 0 | 0 | 0 | shared |
| 84 | network | StationProbeList.o | `src/network/Plugins/StationProbeList.cpp` | 2 | 0 | 0 | 2 | 0 | 0 | 0 | cannot-provide |
| 85 | network | StationState.o | `src/network/ObjDup/StationState.cpp` | 2 | 0 | 0 | 2 | 0 | 0 | 0 | cannot-provide |
| 86 | network | StreamSettings.o | `src/network/Plugins/StreamSettings.cpp` | 2 | 0 | 0 | 1 | 1 | 0 | 0 | shared |
| 87 | network | SystemComponent.o | `src/network/Core/SystemComponent.cpp` | 2 | 0 | 0 | 1 | 1 | 0 | 0 | shared |
| 88 | network | TransportSignatureGenerator.o | `src/network/Plugins/TransportSignatureGenerator.cpp` | 2 | 0 | 0 | 1 | 1 | 0 | 0 | cannot-provide |
| 89 | network | AccountManagementClient.o | `src/network/Services/AccountManagementClient.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | cannot-provide |
| 90 | system | ArpeggioShape.o | `src/system/bandobj/ArpeggioShape.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 91 | system | BandScoreboard.o | `src/system/bandobj/BandScoreboard.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 92 | network | BandwidthCounter.o | `src/network/Platform/BandwidthCounter.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 93 | system | BeatMaster.o | `src/system/beatmatch/BeatMaster.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | shared |
| 94 | network | BitStream.o | `src/network/Plugins/BitStream.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 95 | system | BlockMgr.o | `src/system/os/BlockMgr.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | shared |
| 96 | system | CacheMgr_Wii.o | `src/system/utl/CacheMgr_Wii.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 97 | network | CallProtocolMethodOperation.o | `src/network/Protocol/CallProtocolMethodOperation.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | cannot-provide |
| 98 | network | CallRegister.o | `src/network/ObjDup/CallRegister.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | cannot-provide |
| 99 | system | CharBone.o | `src/system/char/CharBone.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 100 | system | CharClipGroup.o | `src/system/char/CharClipGroup.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 101 | system | CharDriver.o | `src/system/char/CharDriver.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 102 | system | CharHair.o | `src/system/char/CharHair.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 103 | system | CharKeyHandMidi.o | `src/system/bandobj/CharKeyHandMidi.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | shared |
| 104 | system | ClipCompressor.o | `src/system/char/ClipCompressor.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 105 | network | CompressionAlgorithm.o | `src/network/Plugins/CompressionAlgorithm.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 106 | network | ConnectionInfo.o | `src/network/ObjDup/ConnectionInfo.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | cannot-provide |
| 107 | network | ConnectionInfoDDL.o | `src/network/ObjDup/ConnectionInfoDDL.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 108 | network | ConnectionManager.o | `src/network/Plugins/ConnectionManager.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | cannot-provide |
| 109 | network | Core.o | `src/network/Core/Core.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | shared |
| 110 | system | CreditsPanel.o | `src/system/meta/CreditsPanel.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 111 | network | DOCore.o | `src/network/ObjDup/DOCore.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | cannot-provide |
| 112 | network | DOHandle.o | `src/network/ObjDup/DOHandle.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | cannot-provide |
| 113 | network | DOOperation.o | `src/network/ObjDup/DOOperation.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | cannot-provide |
| 114 | system | DataNode.o | `src/system/obj/DataNode.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | shared |
| 115 | system | DateTime.o | `src/system/os/DateTime.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 116 | system | DirLoader.o | `src/system/obj/DirLoader.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 117 | system | EnvAnim.o | `src/system/rndobj/EnvAnim.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | shared |
| 118 | network | EventHandler.o | `src/network/Platform/EventHandler.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | shared |
| 119 | system | FIRFilter.o | `src/system/synthwii/soundtouch/FIRFilter.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 120 | system | Faders.o | `src/system/synth/Faders.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | shared |
| 121 | network | FaultProcessingContext.o | `src/network/ObjDup/FaultProcessingContext.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | cannot-provide |
| 122 | system | FillInfo.o | `src/system/beatmatch/FillInfo.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | shared |
| 123 | system | GameGem.o | `src/system/beatmatch/GameGem.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 124 | system | HxGuid.o | `src/system/utl/HxGuid.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 125 | network | IDGenerator.o | `src/network/ObjDup/IDGenerator.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | cannot-provide |
| 126 | system | IIRFilter.o | `src/system/dsp/IIRFilter.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | shared |
| 127 | network | JobDisconnectStation.o | `src/network/ObjDup/JobDisconnectStation.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | cannot-provide |
| 128 | network | JobListenOnWellKnown.o | `src/network/ObjDup/JobListenOnWellKnown.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | cannot-provide |
| 129 | network | JobManageAccount.o | `src/network/Services/JobManageAccount.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | cannot-provide |
| 130 | network | JobProcessFault.o | `src/network/ObjDup/JobProcessFault.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | cannot-provide |
| 131 | network | JobProcessMessage.o | `src/network/ObjDup/JobProcessMessage.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | cannot-provide |
| 132 | network | JobTerminateDOCore.o | `src/network/ObjDup/JobTerminateDOCore.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | cannot-provide |
| 133 | network | JobTicketManagerAcquireTicket.o | `src/network/Services/JobTicketManagerAcquireTicket.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | cannot-provide |
| 134 | network | Jobs_RV.o | `src/network/net/Jobs_RV.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | shared |
| 135 | network | JsonUtils.o | `src/network/net/JsonUtils.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 136 | network | KerberosEncryption.o | `src/network/Services/KerberosEncryption.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | cannot-provide |
| 137 | network | MD5Checksum.o | `src/network/Plugins/MD5Checksum.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | shared |
| 138 | network | MatchMakingClient.o | `src/network/Services/MatchMakingClient.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | cannot-provide |
| 139 | network | MatchmakingSettings.o | `src/network/net/MatchmakingSettings.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 140 | system | MemMgr.o | `src/system/utl/MemMgr.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 141 | network | MessageBroker.o | `src/network/net/MessageBroker.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | cannot-provide |
| 142 | system | MeterDisplay.o | `src/system/bandobj/MeterDisplay.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 143 | network | MigrationContext.o | `src/network/ObjDup/MigrationContext.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | cannot-provide |
| 144 | system | NetCacheMgr.o | `src/system/utl/NetCacheMgr.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | shared |
| 145 | network | NetSearchResult.o | `src/network/net/NetSearchResult.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | shared |
| 146 | network | NetSession.o | `src/network/net/NetSession.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | shared |
| 147 | system | NetStream.o | `src/system/os/NetStream.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 148 | network | Operation.o | `src/network/Core/Operation.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | shared |
| 149 | network | OutputFormat.o | `src/network/Platform/OutputFormat.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 150 | system | PatchDir.o | `src/system/bandobj/PatchDir.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 151 | system | PatchRenderer.o | `src/system/bandobj/PatchRenderer.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 152 | system | ProfilePicture.o | `src/system/os/ProfilePicture.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 153 | network | PromotionReferee.o | `src/network/ObjDup/PromotionReferee.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | cannot-provide |
| 154 | network | QuazalSession.o | `src/network/net/QuazalSession.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | shared |
| 155 | system | RGUtl.o | `src/system/beatmatch/RGUtl.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | shared |
| 156 | network | RMCContext.o | `src/network/ObjDup/RMCContext.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | cannot-provide |
| 157 | network | RandomNumberGenerator.o | `src/network/Platform/RandomNumberGenerator.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 158 | network | RemoteLogDeviceServer.o | `src/network/Extensions/RemoteLogDeviceServer.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | cannot-provide |
| 159 | system | Rnd.o | `src/system/rndobj/Rnd.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 160 | network | RootDODDL.o | `src/network/ObjDup/RootDODDL.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | cannot-provide |
| 161 | network | RootTransport.o | `src/network/Plugins/RootTransport.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | cannot-provide |
| 162 | network | SecureConnectionClient.o | `src/network/Services/SecureConnectionClient.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | cannot-provide |
| 163 | network | SessionDescription.o | `src/network/Plugins/SessionDescription.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | cannot-provide |
| 164 | network | SessionInfo.o | `src/network/ObjDup/SessionInfo.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 165 | network | SessionMessages.o | `src/network/net/SessionMessages.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 166 | network | SessionSpace.o | `src/network/Extensions/SessionSpace.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | cannot-provide |
| 167 | network | SlidingWindow.o | `src/network/Plugins/SlidingWindow.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | cannot-provide |
| 168 | network | Socket.o | `src/network/Platform/Socket.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | shared |
| 169 | system | SongData.o | `src/system/beatmatch/SongData.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 170 | system | SpotlightDrawer.o | `src/system/world/SpotlightDrawer.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 171 | network | StationContactInfo.o | `src/network/Plugins/StationContactInfo.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | cannot-provide |
| 172 | network | StationDDL.o | `src/network/ObjDup/StationDDL.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | cannot-provide |
| 173 | network | StationIdentificationDDL.o | `src/network/ObjDup/StationIdentificationDDL.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 174 | network | StationManager.o | `src/network/ObjDup/StationManager.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | cannot-provide |
| 175 | network | StationProbe.o | `src/network/Plugins/StationProbe.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | cannot-provide |
| 176 | system | StoreArtLoaderPanel.o | `src/system/meta/StoreArtLoaderPanel.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 177 | network | Stream.o | `src/network/Plugins/Stream.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | shared |
| 178 | network | StreamManager.o | `src/network/Services/StreamManager.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | cannot-provide |
| 179 | system | SynthSample.o | `src/system/synth/SynthSample.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 180 | system | System.o | `src/system/os/System.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | shared |
| 181 | network | SystemComponentGroup.o | `src/network/Core/SystemComponentGroup.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 182 | network | SystemComponents.o | `src/network/Core/SystemComponents.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | shared |
| 183 | system | Task.o | `src/system/obj/Task.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 184 | network | ThreadVariable.o | `src/network/Platform/ThreadVariable.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | shared |
| 185 | network | Ticket.o | `src/network/Services/Ticket.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | cannot-provide |
| 186 | network | TicketManager.o | `src/network/Services/TicketManager.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | cannot-provide |
| 187 | system | TimeConversion.o | `src/system/utl/TimeConversion.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 188 | network | TimeoutManager.o | `src/network/Plugins/TimeoutManager.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | cannot-provide |
| 189 | system | UIResource.o | `src/system/ui/UIResource.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 190 | system | UISlider.o | `src/system/ui/UISlider.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | shared |
| 191 | system | UITransitionHandler.o | `src/system/ui/UITransitionHandler.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | shared |
| 192 | network | UpdatePolicy.o | `src/network/ObjDup/UpdatePolicy.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 0 | cannot-provide |

## Per-TU function rosters

Each TU's identities, confidence-ranked. `wii_symbol` is the CW/MWCC ground-truth name (`bin/analyze-function <wii_symbol>` in the rb3 repo for the real body). Rows in the **bsim15-20** tier are confirm-on-consume.

### EndPoint.o — network, 3 ids (high 0, ≥30 2, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Plugins/EndPoint.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82ac8950` | `0x800457c0` | bsim 34.3 | - | BSIM | Quazal::EndPoint::SetConnectionID(...) | `SetConnectionID__Q26Quazal8EndPointFUi` |
| `0x82ac8a08` | `0x80045850` | bsim 34.3 | - | BSIM | Quazal::EndPoint::SetPrincipalID(...) | `SetPrincipalID__Q26Quazal8EndPointFUi` |
| `0x82ac8520` | `0x800455b0` | bsim 19.9 | - | BSIM | Quazal::EndPoint::EndPoint(...) | `__ct__Q26Quazal8EndPointFPQ26Quazal24ConnectionOrientedStreamPCQ26Quazal10StationURL` |

### DuplicationSpace.o — network, 6 ids (high 0, ≥30 1, 20-30 4, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Extensions/DuplicationSpace.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82af8f88` | `0x800be7a0` | bsim 37.7 | - | BSIM | Quazal::DuplicationSpace::OperationEndMatchTrigger(...) | `OperationEndMatchTrigger__Q26Quazal16DuplicationSpaceFPQ26Quazal11DOOperation` |
| `0x82a51c18` | `0x800c3140` | bsim 22.2 | - | BSIM | Quazal::PseudoGlobalVariable<Q26Quazal22MatchOperationTriggers>::FreeExtraContexts(...) | `FreeExtraContexts__Q26Quazal55PseudoGlobalVariable<Q26Quazal22MatchOperationTriggers>Fv` |
| `0x82af8c48` | `0x800be590` | bsim 20.9 | - | BSIM | Quazal::DuplicationSpace::DuplicationSpace(...) | `__ct__Q26Quazal16DuplicationSpaceFv` |
| `0x82af8e48` | `0x800be720` | bsim 21.4 | - | BSIM | Quazal::DuplicationSpace::GetGlobalTriggers(...) | `GetGlobalTriggers__Q26Quazal16DuplicationSpaceFv` |
| `0x82afd7d8` | `0x800c31e0` | bsim 25.8 | - | BSIM | Quazal::PseudoGlobalVariable<Q26Quazal22MatchOperationTriggers>::ResetContext(...) | `ResetContext__Q26Quazal55PseudoGlobalVariable<Q26Quazal22MatchOperationTriggers>FUi` |
| `0x82afb1a8` | `0x800c1c70` | bsim 15.2 | - | BSIM | Quazal::DuplicationSpace::AddDOClassToFilter(...) | `AddDOClassToFilter__Q26Quazal16DuplicationSpaceFPPQ26Quazal8DOFilterUib` |

### DuplicatedObject.o — network, 5 ids (high 0, ≥30 1, 20-30 0, 15-20 4, 10-15 0, contra 0)  ·  `src/network/ObjDup/DuplicatedObject.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a45a78` | `0x80087900` | bsim 34.6 | - | BSIM | Quazal::DuplicatedObject::Publish(...) | `Publish__Q26Quazal16DuplicatedObjectFUi` |
| `0x82a435b8` | `0x80084820` | bsim 18.1 | - | BSIM | Quazal::DuplicatedObject::ExecAddToStore(...) | `ExecAddToStore__Q26Quazal16DuplicatedObjectFRCQ26Quazal19AddToStoreOperation` |
| `0x82a45410` | `0x80086d20` | bsim 15.8 | - | BSIM | Quazal::DuplicatedObject::ClearFlag(...) | `ClearFlag__Q26Quazal16DuplicatedObjectFUs` |
| `0x82a46180` | `0x80087eb0` | bsim 16.0 | - | BSIM | Quazal::DuplicatedObject::Create(...) | `Create__Q26Quazal16DuplicatedObjectFUiUi` |
| `0x82a474a8` | `0x800892a0` | bsim 16.8 | - | BSIM | Quazal::DuplicatedObject::UnidentifiedMasterState(...) | `UnidentifiedMasterState__Q26Quazal16DuplicatedObjectFRCQ36Quazal12StateMachine6QEvent` |

### ObjDupProtocol.o — network, 4 ids (high 0, ≥30 1, 20-30 2, 15-20 1, 10-15 0, contra 0)  ·  `src/network/ObjDup/ObjDupProtocol.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a66cc8` | `0x8009ffc0` | bsim 32.8 | - | BSIM | Quazal::ObjDupProtocol::ProcessJoinResponse(...) | `ProcessJoinResponse__Q26Quazal14ObjDupProtocolFPQ26Quazal7MessageRUc` |
| `0x82a65670` | `0x8009ef10` | bsim 28.8 | - | BSIM | Quazal::ObjDupProtocol::ObjDupProtocol(...) | `__ct__Q26Quazal14ObjDupProtocolFv` |
| `0x82a67a88` | `0x800a37d0` | bsim 22.3 | - | BSIM | JobExecuteDelayedRMC::Execute(...) | `Execute__20JobExecuteDelayedRMCFv` |
| `0x82a69020` | `0x800a22c0` | bsim 15.3 | - | BSIM | Quazal::ObjDupProtocol::StopToListen(...) | `StopToListen__Q26Quazal14ObjDupProtocolFv` |

### Station.o — network, 4 ids (high 0, ≥30 1, 20-30 3, 15-20 0, 10-15 0, contra 0)  ·  `src/network/ObjDup/Station.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a4dfd8` | `0x800b0d70` | bsim 58.6 | - | BSIM | Quazal::Station::ValidOperation(...) | `ValidOperation__Q26Quazal7StationFPQ26Quazal11DOOperation` |
| `0x82a4d258` | `0x800affa0` | bsim 25.9 | - | BSIM | Quazal::Station::Station(...) | `__ct__Q26Quazal7StationFv` |
| `0x82a4e4c0` | `0x800b1300` | bsim 24.8 | - | BSIM | Quazal::Station::SetLocalStation(...) | `SetLocalStation__Q26Quazal7StationFQ26Quazal8DOHandle` |
| `0x82a4e548` | `0x800b1360` | bsim 20.6 | - | BSIM | Quazal::Station::GetLocalStation(...) | `GetLocalStation__Q26Quazal7StationFv` |

### Text.o — system, 4 ids (high 0, ≥30 1, 20-30 1, 15-20 2, 10-15 0, contra 0)  ·  `src/system/rndobj/Text.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82446f08` | `0x80946bd0` | bsim 34.5 | - | BSIM | RndText::SetAltSizeAndZOffset(...) | `SetAltSizeAndZOffset__7RndTextFff` |
| `0x82446dd0` | `0x809466f0` | bsim 23.3 | - | BSIM | RndText::SetSize(...) | `SetSize__7RndTextFf` |
| `0x82442120` | `0x80946db0` | bsim 19.3 | - | BSIM | RndText::Print(...) | `Print__7RndTextFv` |
| `0x82443440` | `0x8094be10` | bsim 16.7 | - | BSIM | RndText::SyncMeshes(...) | `SyncMeshes__7RndTextFv` |

### Authentication.o — network, 3 ids (high 0, ≥30 1, 20-30 2, 15-20 0, 10-15 0, contra 0)  ·  `src/network/ObjDup/Authentication.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a9ee90` | `0x80072500` | bsim 34.2 | - | BSIM | Quazal::ProcessAuthentication::Authenticate(...) | `Authenticate__Q26Quazal21ProcessAuthenticationFRCQ26Quazal21ProcessAuthentication` |
| `0x82a9eb80` | `0x80072200` | bsim 20.0 | - | BSIM | Quazal::ProcessAuthentication::ProcessAuthentication(...) | `__ct__Q26Quazal21ProcessAuthenticationFPQ26Quazal11ProductInfo` |
| `0x82a9ed70` | `0x800723f0` | bsim 25.5 | - | BSIM | Quazal::ProcessAuthentication::ExtractFrom(...) | `ExtractFrom__Q26Quazal21ProcessAuthenticationFPQ26Quazal7MessagebPQ26Quazal6String` |

### BackEndServices.o — network, 3 ids (high 0, ≥30 1, 20-30 1, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Services/BackEndServices.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a5a660` | `0x800e0a40` | bsim 31.0 | - | BSIM | Quazal::BackEndServices::BackEndServices(...) | `__ct__Q26Quazal15BackEndServicesFv` |
| `0x82a5b718` | `0x800e1cf0` | bsim 21.1 | - | BSIM | Quazal::BackEndServices::LogoutImpl(...) | `LogoutImpl__Q26Quazal15BackEndServicesFPQ26Quazal11CallContextPQ26Quazal11Credentials` |
| `0x82a5b280` | `0x800e17e0` | bsim 17.7 | - | BSIM | Quazal::BackEndServices::RegisterProtocols(...) | `RegisterProtocols__Q26Quazal15BackEndServicesFv` |

### Dir.o — system, 3 ids (high 0, ≥30 1, 20-30 0, 15-20 2, 10-15 0, contra 0)  ·  `src/system/rndobj/Dir.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x824b8a28` | `0x8082cc40` | bsim 48.2 | - | BSIM | WorldDir::DrawShowing(...) | `DrawShowing__8WorldDirFv` |
| `0x823f1e18` | `0x8088bd70` | bsim 18.9 | - | BSIM | RndDir::RemovingObject(...) | `RemovingObject__6RndDirFPQ23Hmx6Object` |
| `0x82729098` | `0x80463500` | bsim 15.9 | - | BSIM | ObjectDir::Load(...) | `Load__9ObjectDirFR9BinStream` |

### JobJoinSession.o — network, 3 ids (high 0, ≥30 1, 20-30 0, 15-20 2, 10-15 0, contra 0)  ·  `src/network/ObjDup/JobJoinSession.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a9c718` | `0x80095440` | bsim 41.5 | - | BSIM | Quazal::JobJoinSession::ProcessPositiveJoinResponse(...) | `ProcessPositiveJoinResponse__Q26Quazal14JobJoinSessionFUcQ26Quazal8DOHandleQ26Quazal8DOHandle` |
| `0x82a9c2f8` | `0x80095040` | bsim 18.3 | - | BSIM | Quazal::JobJoinSession::JoinSuccess(...) | `JoinSuccess__Q26Quazal14JobJoinSessionFv` |
| `0x82a9c510` | `0x800952a0` | bsim 17.7 | - | BSIM | Quazal::JobJoinSession::TestConnection(...) | `TestConnection__Q26Quazal14JobJoinSessionFv` |

### NATTraversalEngine.o — network, 3 ids (high 0, ≥30 1, 20-30 1, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Plugins/NATTraversalEngine.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82ad62a0` | `0x800490f0` | bsim 53.9 | - | BSIM | Quazal::NATTraversalEngine::SendProbe(...) | `SendProbe__Q26Quazal18NATTraversalEngineFQ36Quazal18NATTraversalEngine3MsgRCQ26Quazal10StationURLQ26Quazal4Time` |
| `0x82ad67b8` | `0x800492b0` | bsim 27.9 | - | BSIM | Quazal::NATTraversalEngine::ReceiveMessage(...) | `ReceiveMessage__Q26Quazal18NATTraversalEngineFRCQ26Quazal10StationURLPCUcUi` |
| `0x82ad4df8` | `0x80048480` | bsim 18.4 | - | BSIM | Quazal::NATTraversalEngine::NATTraversalEngine(...) | `__ct__Q26Quazal18NATTraversalEngineFv` |

### Anim.o — system, 2 ids (high 1, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/system/rndobj/Anim.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x823eda20` | `0x8087a120` | high | - | ExactInstructionsFunctionHasher | RndAnimatable const::Units(...) | `Units__13RndAnimatableCFv` |
| `0x823eda38` | `0x8087a140` | bsim 15.5 | - | BSIM | RndAnimatable::FramesPerUnit(...) | `FramesPerUnit__13RndAnimatableFv` |

### BandHeadShaper.o — system, 2 ids (high 0, ≥30 1, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/system/bandobj/BandHeadShaper.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8229e690` | `0x8055abc0` | bsim 30.9 | - | BSIM | BandHeadShaper::End(...) | `End__14BandHeadShaperFv` |
| `0x8229e400` | `0x8055a9c0` | bsim 17.0 | - | BSIM | BandHeadShaper::Reskin(...) | `Reskin__14BandHeadShaperFv` |

### DuplicationSpaceTable.o — network, 2 ids (high 0, ≥30 1, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Extensions/DuplicationSpaceTable.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b18c08` | `0x800c3970` | bsim 30.6 | - | BSIM | Quazal::DuplicationSpaceTable::OperationEndMatchTrigger(...) | `OperationEndMatchTrigger__Q26Quazal21DuplicationSpaceTableFPQ26Quazal11DOOperation` |
| `0x82b18888` | `0x800c3660` | bsim 20.2 | - | BSIM | Quazal::DuplicationSpaceTable::StartPeriodicMatch(...) | `StartPeriodicMatch__Q26Quazal21DuplicationSpaceTableFv` |

### JobConnectStation.o — network, 2 ids (high 0, ≥30 1, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/ObjDup/JobConnectStation.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a88d60` | `0x80090d10` | bsim 33.3 | - | BSIM | Quazal::JobConnectStation::WaitForURLs(...) | `WaitForURLs__Q26Quazal17JobConnectStationFv` |
| `0x82a88e90` | `0x80090dd0` | bsim 24.7 | - | BSIM | Quazal::JobConnectStation::PrepareURLs(...) | `PrepareURLs__Q26Quazal17JobConnectStationFv` |

### JobProcessJoinRequest.o — network, 2 ids (high 0, ≥30 1, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/ObjDup/JobProcessJoinRequest.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82ac1098` | `0x80098830` | bsim 42.0 | - | BSIM | Quazal::JobProcessJoinRequest::InitiateConnection(...) | `InitiateConnection__Q26Quazal21JobProcessJoinRequestFv` |
| `0x82ac0d30` | `0x800984d0` | bsim 25.9 | - | BSIM | Quazal::JobProcessJoinRequest::ApproveJoinOperation(...) | `ApproveJoinOperation__Q26Quazal21JobProcessJoinRequestFv` |

### MatchOperation.o — network, 2 ids (high 0, ≥30 1, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Extensions/MatchOperation.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b19df8` | `0x800c4d40` | bsim 30.1 | - | BSIM | Quazal::MatchOperation::ExecuteQueuedOperation(...) | `ExecuteQueuedOperation__Q26Quazal14MatchOperationFi` |
| `0x82b19ea0` | `0x800c4db0` | bsim 17.7 | - | BSIM | Quazal::MatchOperation::ExecuteOperation(...) | `ExecuteOperation__Q26Quazal14MatchOperationFv` |

### QueuingSocket.o — network, 2 ids (high 0, ≥30 1, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Plugins/QueuingSocket.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b0bcc8` | `0x80068ec0` | bsim 32.6 | - | BSIM | Quazal::QueuingSocket::CompleteSend(...) | `CompleteSend__Q26Quazal13QueuingSocketFv` |
| `0x82b0b6b0` | `0x80068ae0` | bsim 24.2 | - | BSIM | Quazal::QueuingSocket::CreateBufferFromPacketQueue(...) | `CreateBufferFromPacketQueue__Q26Quazal13QueuingSocketFPQ26Quazal11PacketQueueUi` |

### SessionClock.o — network, 2 ids (high 0, ≥30 1, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Extensions/SessionClock.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a50480` | `0x800c7860` | bsim 37.7 | - | BSIM | Quazal::SessionClock::SessionClock(...) | `__ct__Q26Quazal12SessionClockFv` |
| `0x82a40630` | `0x800c7c00` | bsim 15.4 | - | BSIM | Quazal::SessionClock::GetInstance(...) | `GetInstance__Q26Quazal12SessionClockFv` |

### SessionState.o — network, 2 ids (high 0, ≥30 1, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/ObjDup/SessionState.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a9cf78` | `0x800afac0` | bsim 46.6 | - | BSIM | Quazal::SessionState::OperationEnd(...) | `OperationEnd__Q26Quazal12SessionStateFPCQ26Quazal11DOOperation` |
| `0x82a9d030` | `0x800afb60` | bsim 23.0 | - | BSIM | Quazal::SessionState::GetPersistantState(...) | `GetPersistantState__Q26Quazal12SessionStateFv` |

### SharedSessionDescription.o — network, 2 ids (high 0, ≥30 1, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/ObjDup/SharedSessionDescription.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a93da0` | `0x800afdb0` | bsim 30.3 | - | BSIM | Quazal::SharedSessionDescription::PullSharedSessionDescription(...) | `PullSharedSessionDescription__Q26Quazal24SharedSessionDescriptionFv` |
| `0x82a93ea8` | `0x800afe80` | bsim 18.6 | - | BSIM | Quazal::SharedSessionDescription::OperationEnd(...) | `OperationEnd__Q26Quazal24SharedSessionDescriptionFPCQ26Quazal11DOOperation` |

### UDPTransport.o — network, 2 ids (high 0, ≥30 1, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Plugins/UDPTransport.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82ae7b58` | `0x80069de0` | bsim 38.0 | - | BSIM | Quazal::UDPTransport::StartEventListener(...) | `StartEventListener__Q26Quazal12UDPTransportFv` |
| `0x82ae9828` | `0x8006bc60` | bsim 23.2 | - | BSIM | Quazal::UDPTransport::TransportThread(...) | `TransportThread__Q26Quazal12UDPTransportFPv` |

### CharClip.o — system, 1 ids (high 0, ≥30 1, 20-30 0, 15-20 0, 10-15 0, contra 0)  ·  `src/system/char/CharClip.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8236a5f8` | `0x80695a20` | bsim 36.9 | - | BSIM | CharClip::BeatAlignString(...) | `BeatAlignString__8CharClipFi` |

### CharIKScale.o — system, 1 ids (high 1, ≥30 0, 20-30 0, 15-20 0, 10-15 0, contra 0)  ·  `src/system/char/CharIKScale.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x823871f0` | `0x806fa350` | high | - | ExactInstructionsFunctionHasher | CharIKScale::CaptureBefore(...) | `CaptureBefore__11CharIKScaleFv` |

### EncryptionAlgorithm.o — network, 1 ids (high 0, ≥30 1, 20-30 0, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Plugins/EncryptionAlgorithm.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82af4eb8` | `0x80040010` | bsim 35.6 | - | BSIM | Quazal::EncryptionAlgorithm::SetKey(...) | `SetKey__Q26Quazal19EncryptionAlgorithmFRCQ26Quazal3Key` |

### FetchContext.o — network, 1 ids (high 0, ≥30 1, 20-30 0, 15-20 0, 10-15 0, contra 0)  ·  `src/network/ObjDup/FetchContext.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a91638` | `0x8008c2f0` | bsim 34.9 | - | BSIM | Quazal::FetchContext::FetchDuplicaImpl(...) | `FetchDuplicaImpl__Q26Quazal12FetchContextFQ26Quazal8DOHandle` |

### JobConnectSecureEndPoint.o — network, 1 ids (high 0, ≥30 1, 20-30 0, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Services/JobConnectSecureEndPoint.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b14400` | `0x800f5ba0` | bsim 35.4 | - | BSIM | Quazal::JobConnectSecureEndPoint::RequestConnectionData(...) | `RequestConnectionData__Q26Quazal24JobConnectSecureEndPointFv` |

### JoypadGuitarController.o — system, 1 ids (high 0, ≥30 1, 20-30 0, 15-20 0, 10-15 0, contra 0)  ·  `src/system/beatmatch/JoypadGuitarController.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82775b68` | `0x80630190` | bsim 33.6 | - | BSIM | JoypadGuitarController::ReconcileFretState(...) | `ReconcileFretState__22JoypadGuitarControllerFv` |

### LogFile.o — system, 1 ids (high 0, ≥30 1, 20-30 0, 15-20 0, 10-15 0, contra 0)  ·  `src/system/utl/LogFile.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827a6db0` | `0x8049c650` | bsim 34.3 | - | BSIM | LogFile::Print(...) | `Print__7LogFileFPCc` |

### MasterAudio.o — system, 1 ids (high 1, ≥30 0, 20-30 0, 15-20 0, 10-15 0, contra 0)  ·  `src/system/beatmatch/MasterAudio.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82757000` | `0x80637e20` | high | - | Implied Match | MasterAudio::SetVocalCueFader(...) | `SetVocalCueFader__11MasterAudioFf` |

### Movie.o — system, 1 ids (high 0, ≥30 1, 20-30 0, 15-20 0, 10-15 0, contra 0)  ·  `src/system/movie/Movie.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8271fb70` | `0x807883b0` | bsim 38.8 | - | BSIM | Movie::Impl::End(...) | `End__Q25Movie4ImplFv` |

### PRUDPEndPoint.o — network, 1 ids (high 0, ≥30 1, 20-30 0, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Plugins/PRUDPEndPoint.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b05480` | `0x80058b70` | bsim 48.5 | - | BSIM | Quazal::PRUDPEndPoint::Defrag(...) | `Defrag__Q26Quazal13PRUDPEndPointFPQ26Quazal8PacketIn` |

### Packet.o — network, 1 ids (high 0, ≥30 1, 20-30 0, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Plugins/Packet.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b0dfe8` | `0x80066e00` | bsim 31.8 | - | BSIM | Quazal::Packet::Packet(...) | `__ct__Q26Quazal6PacketFv` |

### PacketQueue.o — network, 1 ids (high 0, ≥30 1, 20-30 0, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Plugins/PacketQueue.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a9a220` | `0x800681e0` | bsim 38.9 | - | BSIM | Quazal::PacketQueue::Dequeue(...) | `Dequeue__Q26Quazal11PacketQueueFQ36Quazal74qChain<PQ26Quazal6Packet,Q26Quazal37DefaultChainPolicy<PQ26Quazal6Packet>>8iterator` |

### PhraseList.o — system, 1 ids (high 1, ≥30 0, 20-30 0, 15-20 0, 10-15 0, contra 0)  ·  `src/system/beatmatch/PhraseList.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8276e798` | `0x8063eb90` | high | - | ExactInstructionsFunctionHasher | PhraseListCollection::AddPhrase(...) | `AddPhrase__20PhraseListCollectionF19BeatmatchPhraseTypefifi` |

### ProtocolRequestBroker.o — network, 1 ids (high 0, ≥30 1, 20-30 0, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Protocol/ProtocolRequestBroker.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a7be58` | `0x8006eaf0` | bsim 35.3 | - | BSIM | Quazal::ProtocolRequestBroker::ProcessMessageCore(...) | `ProcessMessageCore__Q26Quazal21ProtocolRequestBrokerFPQ26Quazal27CallProtocolMethodOperationPQ26Quazal8EndPointPQ26Quazal6Buffer` |

### PseudoGlobalVariableList.o — network, 1 ids (high 0, ≥30 1, 20-30 0, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Core/PseudoGlobalVariableList.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a83a18` | `0x8002f5c0` | bsim 37.5 | - | BSIM | Quazal::PseudoGlobalVariableList::AddVariable(...) | `AddVariable__Q26Quazal24PseudoGlobalVariableListFPQ26Quazal24PseudoGlobalVariableRoot` |

### RGState.o — system, 1 ids (high 1, ≥30 0, 20-30 0, 15-20 0, 10-15 0, contra 0)  ·  `src/system/beatmatch/RGState.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82837880` | `0x80644ad0` | high | - | ExactInstructionsFunctionHasher | __as__7RGStateFRC7RGState | `__as__7RGStateFRC7RGState` |

### SongSectionController.o — system, 1 ids (high 0, ≥30 1, 20-30 0, 15-20 0, 10-15 0, contra 0)  ·  `src/system/bandobj/SongSectionController.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x822fad50` | `0x80613480` | bsim 30.3 | - | BSIM | SongSectionController::DebugActivate(...) | `DebugActivate__21SongSectionControllerFv` |

### Str.o — system, 1 ids (high 1, ≥30 0, 20-30 0, 15-20 0, 10-15 0, contra 0)  ·  `src/system/utl/Str.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8254f660` | `0x804bbe20` | high | - | ExactInstructionsFunctionHasher | String::insert(...) | `insert__6StringFUiRC6String` |

### StringConversion.o — network, 1 ids (high 0, ≥30 1, 20-30 0, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Platform/StringConversion.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82ab8230` | `0x80023f40` | bsim 36.0 | - | BSIM | @unnamed@StringConversion_cpp@::Utf8ToLatin1(...) | `Utf8ToLatin1__30@unnamed@StringConversion_cpp@FPCcPcUi` |

### SystemError.o — network, 1 ids (high 0, ≥30 1, 20-30 0, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Platform/SystemError.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a53440` | `0x80025120` | bsim 33.7 | - | BSIM | Quazal::SystemError::GetErrorString(...) | `GetErrorString__Q26Quazal11SystemErrorFUiPcUi` |

### TDStretch.o — system, 1 ids (high 0, ≥30 1, 20-30 0, 15-20 0, 10-15 0, contra 0)  ·  `src/system/synthwii/soundtouch/TDStretch.cpp`  ·  DC3 shared  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b42308` | `0x8097c600` | bsim 40.2 | - | BSIM | soundtouch::TDStretch::seekBestOverlapPosition(...) | `seekBestOverlapPosition__Q210soundtouch9TDStretchFPCs` |

### TrackWidget.o — system, 1 ids (high 1, ≥30 0, 20-30 0, 15-20 0, 10-15 0, contra 0)  ·  `src/system/track/TrackWidget.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827bb458` | `0x807995c0` | high | - | Implied Match | TrackWidget::Init(...) | `Init__11TrackWidgetFv` |

### deflate.o — system, 1 ids (high 1, ≥30 0, 20-30 0, 15-20 0, 10-15 0, contra 0)  ·  `src/system/zlib/deflate.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b292b0` | `0x804d85e0` | high | - | ExactInstructionsFunctionHasher | deflateInit_ | `deflateInit_` |

### Session.o — network, 5 ids (high 0, ≥30 0, 20-30 2, 15-20 3, 10-15 0, contra 0)  ·  `src/network/ObjDup/Session.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a48ec0` | `0x800a9d20` | bsim 23.8 | - | BSIM | Quazal::Session::CreateSession(...) | `CreateSession__Q26Quazal7SessionFPCcb` |
| `0x82a49918` | `0x800aada0` | bsim 25.7 | - | BSIM | Quazal::Session::JoinSessionImpl(...) | `JoinSessionImpl__Q26Quazal7SessionFPQ26Quazal11CallContextRCQ26Quazal28qList<Q26Quazal10StationURL>` |
| `0x82a48940` | `0x800a9530` | bsim 15.5 | - | BSIM | Quazal::Session::OperationBegin(...) | `OperationBegin__Q26Quazal7SessionFPQ26Quazal11DOOperation` |
| `0x82a4a098` | `0x800ab320` | bsim 19.1 | - | BSIM | Quazal::Session::CallApproveJoinSessionCallback(...) | `CallApproveJoinSessionCallback__Q26Quazal7SessionFPQ26Quazal20JoinSessionOperation` |
| `0x82a4a390` | `0x800ab630` | bsim 17.7 | - | BSIM | Quazal::Session::UnregisterWellKnownDOsFactory(...) | `UnregisterWellKnownDOsFactory__Q26Quazal7SessionFPFv_v` |

### BandCharDesc.o — system, 4 ids (high 0, ≥30 0, 20-30 3, 15-20 1, 10-15 0, contra 0)  ·  `src/system/bandobj/BandCharDesc.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82321f88` | `0x80550cc0` | bsim 21.5 | - | BSIM | BandCharDesc::OutfitPiece::OutfitPiece(...) | `__ct__Q212BandCharDesc11OutfitPieceFv` |
| `0x823220a0` | `0x80550ef0` | bsim 23.3 | - | BSIM | BandCharDesc::Outfit::Outfit(...) | `__ct__Q212BandCharDesc6OutfitFv` |
| `0x823223c0` | `0x80551240` | bsim 20.2 | - | BSIM | BandCharDesc::InstrumentOutfit::InstrumentOutfit(...) | `__ct__Q212BandCharDesc16InstrumentOutfitFv` |
| `0x82321938` | `0x805519e0` | bsim 18.3 | - | BSIM | BandCharDesc::Head::SetShape(...) | `SetShape__Q212BandCharDesc4HeadFR14BandHeadShaper` |

### CallContext.o — network, 4 ids (high 0, ≥30 0, 20-30 2, 15-20 2, 10-15 0, contra 0)  ·  `src/network/Core/CallContext.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a5cc30` | `0x8002a400` | bsim 26.0 | - | BSIM | Quazal::InvokeCallbackOnSuccess(...) | `InvokeCallbackOnSuccess__6QuazalFPQ26Quazal11CallContextPCQ26Quazal11UserContext` |
| `0x82a5d150` | `0x8002a9d0` | bsim 21.7 | - | BSIM | Quazal::CallContext::TransitionIsValid(...) | `TransitionIsValid__Q26Quazal11CallContextFQ36Quazal11CallContext6_StateQ36Quazal11CallContext6_State` |
| `0x82a5df10` | `0x8002ba90` | bsim 16.2 | - | BSIM | Quazal::CallContext const::Wait(...) | `Wait__Q26Quazal11CallContextCFUi` |
| `0x82a5e1f8` | `0x8002bd20` | bsim 15.4 | - | BSIM | Quazal::CallContext::ClearFlag(...) | `ClearFlag__Q26Quazal11CallContextFUi` |

### WKHandle.o — network, 4 ids (high 0, ≥30 0, 20-30 3, 15-20 1, 10-15 0, contra 0)  ·  `src/network/ObjDup/WKHandle.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a70b00` | `0x800ba0d0` | bsim 26.6 | - | BSIM | Quazal::WKHandle::Cleanup(...) | `Cleanup__Q26Quazal8WKHandleFv` |
| `0x82a70b60` | `0x800ba110` | bsim 22.0 | - | BSIM | Quazal::WKHandle::AtLeastOneCreated(...) | `AtLeastOneCreated__Q26Quazal8WKHandleFv` |
| `0x82a70c10` | `0x800ba190` | bsim 28.2 | - | BSIM | Quazal::WKHandle::AllInDOS(...) | `AllInDOS__Q26Quazal8WKHandleFv` |
| `0x82a70d18` | `0x800ba240` | bsim 18.5 | - | BSIM | Quazal::WKHandle::IsAWKHandle(...) | `IsAWKHandle__Q26Quazal8WKHandleFQ26Quazal8DOHandle` |

### DOCoreTypes.o — network, 3 ids (high 0, ≥30 0, 20-30 3, 15-20 0, 10-15 0, contra 0)  ·  `src/network/ObjDup/DOCoreTypes.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a6e380` | `0x8007af10` | bsim 20.9 | - | BSIM | Quazal::_Type_qBuffer::Add(...) | `Add__Q26Quazal13_Type_qBufferFPQ26Quazal7MessageRCQ26Quazal7qBuffer` |
| `0x82a6e480` | `0x8007af90` | bsim 25.7 | - | BSIM | Quazal::_Type_qBuffer::Extract(...) | `Extract__Q26Quazal13_Type_qBufferFPQ26Quazal7MessagePQ26Quazal7qBuffer` |
| `0x82a6e548` | `0x8007b040` | bsim 24.4 | - | BSIM | Quazal::_Type_buffertail::Add(...) | `Add__Q26Quazal16_Type_buffertailFPQ26Quazal7MessageRCQ26Quazal6Buffer` |

### PromotionRefereeDDL.o — network, 3 ids (high 0, ≥30 0, 20-30 1, 15-20 2, 10-15 0, contra 0)  ·  `src/network/ObjDup/PromotionRefereeDDL.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a973b8` | `0x800a5b00` | bsim 23.1 | - | BSIM | Quazal::_DO_PromotionReferee::ElectNewMasterWrapper(...) | `ElectNewMasterWrapper__Q26Quazal20_DO_PromotionRefereeFRCQ26Quazal19CallMethodOperation` |
| `0x82a96d58` | `0x800a5840` | bsim 15.3 | - | BSIM | Quazal::_DO_PromotionReferee::CallDeclinePromotion(...) | `CallDeclinePromotion__Q26Quazal20_DO_PromotionRefereeFPQ26Quazal10RMCContextRCQ26Quazal8DOHandleRCQ26Quazal8DOHandle` |
| `0x82a96ed0` | `0x800a5780` | bsim 20.0 | - | BSIM | Quazal::_DO_PromotionReferee::ConfirmElectionWrapper(...) | `ConfirmElectionWrapper__Q26Quazal20_DO_PromotionRefereeFRCQ26Quazal19CallMethodOperation` |

### Protocol.o — network, 3 ids (high 0, ≥30 0, 20-30 1, 15-20 2, 10-15 0, contra 0)  ·  `src/network/Protocol/Protocol.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a77db8` | `0x8006d4c0` | bsim 21.2 | - | BSIM | Quazal::Protocol::Protocol(...) | `__ct__Q26Quazal8ProtocolFUi` |
| `0x82a780b0` | `0x8006d690` | bsim 16.9 | - | BSIM | Quazal::Protocol::ExtractCallOutcome(...) | `ExtractCallOutcome__Q26Quazal8ProtocolFPQ26Quazal7MessagePQ26Quazal7qResult` |
| `0x82a78408` | `0x8006da80` | bsim 16.6 | - | BSIM | Quazal::Protocol::UseLocalLoopback(...) | `UseLocalLoopback__Q26Quazal8ProtocolFUiUi` |

### SessionDDL.o — network, 3 ids (high 0, ≥30 0, 20-30 3, 15-20 0, 10-15 0, contra 0)  ·  `src/network/ObjDup/SessionDDL.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a64148` | `0x800adf90` | bsim 21.6 | - | BSIM | Quazal::_DO_Session::CallOperationOnDatasets(...) | `CallOperationOnDatasets__Q26Quazal11_DO_SessionFPQ26Quazal11DOOperationQ36Quazal9Operation6_Event` |
| `0x82a64a68` | `0x800ae5c0` | bsim 21.1 | - | BSIM | Quazal::_DO_Session::CallRetrieveURLs(...) | `CallRetrieveURLs__Q26Quazal11_DO_SessionFPQ26Quazal10RMCContextRCQ26Quazal8DOHandlePQ26Quazal28qList<Q26Quazal10StationURL>` |
| `0x82a64dc0` | `0x800ae950` | bsim 21.1 | - | BSIM | Quazal::_DO_Session::CallSynchronizeTermination(...) | `CallSynchronizeTermination__Q26Quazal11_DO_SessionFPQ26Quazal10RMCContextPbRCQ26Quazal8DOHandle` |

### UILabel.o — system, 3 ids (high 0, ≥30 0, 20-30 1, 15-20 2, 10-15 0, contra 0)  ·  `src/system/ui/UILabel.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827cdce8` | `0x807c05f0` | bsim 25.5 | - | BSIM | UILabel::UpdateAndDrawHighlightMesh(...) | `UpdateAndDrawHighlightMesh__7UILabelFv` |
| `0x827ccd80` | `0x807c0220` | bsim 16.3 | - | BSIM | UILabel::Poll(...) | `Poll__7UILabelFv` |
| `0x827cd310` | `0x807c0890` | bsim 18.5 | - | BSIM | UILabel::InqMinMaxFromWidthAndHeight(...) | `InqMinMaxFromWidthAndHeight__7UILabelFffQ27RndText9AlignmentR7Vector3R7Vector3` |

### AuthenticationClient.o — network, 2 ids (high 0, ≥30 0, 20-30 1, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Services/AuthenticationClient.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aee0a8` | `0x800d7760` | bsim 20.2 | - | BSIM | Quazal::AuthenticationClient::AuthenticationClient(...) | `__ct__Q26Quazal20AuthenticationClientFv` |
| `0x82aee370` | `0x800d7940` | bsim 17.9 | - | BSIM | Quazal::AuthenticationClient::Rebind(...) | `Rebind__Q26Quazal20AuthenticationClientFPQ26Quazal11Credentials` |

### BandFaceDeform.o — system, 2 ids (high 0, ≥30 0, 20-30 1, 15-20 1, 10-15 0, contra 0)  ·  `src/system/bandobj/BandFaceDeform.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x823f8418` | `0x8050e540` | bsim 25.6 | - | BSIM | BandFaceDeform::DeltaArray::SetSize(...) | `SetSize__Q214BandFaceDeform10DeltaArrayFi` |
| `0x822b5870` | `0x8050ea60` | bsim 15.8 | - | BSIM | BandFaceDeform::DeltaArray::Load(...) | `Load__Q214BandFaceDeform10DeltaArrayFR9BinStream` |

### BeatMatchUtl.o — system, 2 ids (high 0, ≥30 0, 20-30 1, 15-20 1, 10-15 0, contra 0)  ·  `src/system/beatmatch/BeatMatchUtl.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8276b4a8` | `0x8061e8b0` | bsim 25.1 | - | BSIM | GemNumSlots(...)   [free function] | `GemNumSlots__Fi` |
| `0x8276b500` | `0x8061e960` | bsim 15.8 | - | BSIM | ConsumeNumber(...)   [free function] | `ConsumeNumber__FRPCc` |

### Buffer.o — network, 2 ids (high 0, ≥30 0, 20-30 1, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Plugins/Buffer.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a79b88` | `0x8003e430` | bsim 22.4 | - | BSIM | __eq__Q26Quazal6BufferCFRCQ26Quazal6Buffer | `__eq__Q26Quazal6BufferCFRCQ26Quazal6Buffer` |
| `0x82a79c08` | `0x8003e490` | bsim 17.5 | - | BSIM | Quazal::Buffer const::GetContentSize(...) | `GetContentSize__Q26Quazal6BufferCFv` |

### ByteStream.o — network, 2 ids (high 0, ≥30 0, 20-30 2, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Plugins/ByteStream.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a69cd0` | `0x8003ebb0` | bsim 21.3 | - | BSIM | Quazal::ByteStream::SetLength(...) | `SetLength__Q26Quazal10ByteStreamFUi` |
| `0x82a6a350` | `0x8003f400` | bsim 20.4 | - | BSIM | Quazal::ByteStream::SetPosition(...) | `SetPosition__Q26Quazal10ByteStreamFUi` |

### CameraManager.o — system, 2 ids (high 0, ≥30 0, 20-30 1, 15-20 1, 10-15 0, contra 0)  ·  `src/system/world/CameraManager.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82ad4b10` | `0x807fe8a0` | bsim 22.9 | - | BSIM | __adjust_heap<PQ213CameraManager8Category,l,Q213CameraManager8Category,Q211stlpmtx_std32less<Q213CameraManager8Category>>__11stlpmtx_stdFPQ213CameraManager8CategoryllQ213CameraManager8CategoryQ211stlpmtx_std32less<Q213CameraManager8Category>_v | `__adjust_heap<PQ213CameraManager8Category,l,Q213CameraManager8Category,Q211stlpmtx_std32less<Q213CameraManager8Category>>__11stlpmtx_stdFPQ213CameraManager8CategoryllQ213CameraManager8CategoryQ211stlpmtx_std32less<Q213CameraManager8Category>_v` |
| `0x82ad4578` | `0x807fe630` | bsim 15.8 | - | BSIM | __unguarded_partition<PQ213CameraManager8Category,Q213CameraManager8Category,Q211stlpmtx_std32less<Q213CameraManager8Category>>__11stlpmtx_stdFPQ213CameraManager8CategoryPQ213CameraManager8CategoryQ213CameraManager8CategoryQ211stlpmtx_std32less<Q213CameraManager8Category>_PQ213CameraManager8Category | `__unguarded_partition<PQ213CameraManager8Category,Q213CameraManager8Category,Q211stlpmtx_std32less<Q213CameraManager8Category>>__11stlpmtx_stdFPQ213CameraManager8CategoryPQ213CameraManager8CategoryQ213CameraManager8CategoryQ211stlpmtx_std32less<Q213CameraManager8Category>_PQ213CameraManager8Category` |

### ChordShapeGenerator.o — system, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 2, 10-15 0, contra 0)  ·  `src/system/bandobj/ChordShapeGenerator.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x822cb0f0` | `0x8057b9b0` | bsim 16.3 | - | BSIM | ChordShapeGenerator::TransformVert(...) | `TransformVert__19ChordShapeGeneratorFRQ27RndMesh4VertfffRC9TransformQ23Hmx7Color32` |
| `0x822d0be8` | `0x80579160` | bsim 17.3 | - | BSIM | ChordShapeGenerator::BuildChordMesh(...) | `BuildChordMesh__19ChordShapeGeneratorFv` |

### ClientProtocol.o — network, 2 ids (high 0, ≥30 0, 20-30 2, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Protocol/ClientProtocol.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a7a9e0` | `0x8006c910` | bsim 20.2 | - | BSIM | Quazal::ClientProtocol::SendOverLocalLoopback(...) | `SendOverLocalLoopback__Q26Quazal14ClientProtocolFPQ26Quazal19ProtocolCallContextPQ26Quazal7Message` |
| `0x82a7ab38` | `0x8006ca60` | bsim 24.6 | - | BSIM | Quazal::ClientProtocol::SendRMCMessage(...) | `SendRMCMessage__Q26Quazal14ClientProtocolFPQ26Quazal19ProtocolCallContextPQ26Quazal7Message` |

### DOCallContext.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 2, 10-15 0, contra 0)  ·  `src/network/ObjDup/DOCallContext.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a72a40` | `0x80077af0` | bsim 18.3 | - | BSIM | Quazal::DOCallContext::SignalResponse(...) | `SignalResponse__Q26Quazal13DOCallContextFQ26Quazal11UserContext` |
| `0x82a72c10` | `0x80077ca0` | bsim 17.3 | - | BSIM | Quazal::DOCallContext::InternalCancel(...) | `InternalCancel__Q26Quazal13DOCallContextFQ36Quazal11CallContext6_StateQ36Quazal13DOCallContext8_Outcome` |

### DrumTrackWatcherImpl.o — system, 2 ids (high 0, ≥30 0, 20-30 1, 15-20 1, 10-15 0, contra 0)  ·  `src/system/beatmatch/DrumTrackWatcherImpl.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8275bb78` | `0x80625970` | bsim 20.9 | - | BSIM | DrumTrackWatcherImpl::CheckForKickAutoplay(...) | `CheckForKickAutoplay__20DrumTrackWatcherImplFf` |
| `0x8275b878` | `0x80625920` | bsim 15.7 | - | BSIM | DrumTrackWatcherImpl::JumpHook(...) | `JumpHook__20DrumTrackWatcherImplFf` |

### InstanceControl.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 2, 10-15 0, contra 0)  ·  `src/network/Core/InstanceControl.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a57fc8` | `0x8002d9d0` | bsim 16.2 | - | BSIM | Quazal::InstanceControl::InstanceControl(...) | `__ct__Q26Quazal15InstanceControlFUiUi` |
| `0x82a58388` | `0x8002dce0` | bsim 16.8 | - | BSIM | Quazal::InstanceControl::ContextIsValid(...) | `ContextIsValid__Q26Quazal15InstanceControlFUi` |

### InstantiationContext.o — network, 2 ids (high 0, ≥30 0, 20-30 1, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Core/InstantiationContext.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a57e50` | `0x8002e050` | bsim 27.2 | - | BSIM | Quazal::InstantiationContext::AddInstance(...) | `AddInstance__Q26Quazal20InstantiationContextFPQ26Quazal15InstanceControlUi` |
| `0x82a57ef8` | `0x8002e0d0` | bsim 18.4 | - | BSIM | Quazal::InstantiationContext::DelInstance(...) | `DelInstance__Q26Quazal20InstantiationContextFPQ26Quazal15InstanceControlUi` |

### IteratorOverDOs.o — network, 2 ids (high 0, ≥30 0, 20-30 1, 15-20 1, 10-15 0, contra 0)  ·  `src/network/ObjDup/IteratorOverDOs.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a530e8` | `0x8008eba0` | bsim 28.3 | - | BSIM | Quazal::IteratorOverDOs const::CurrentItemIsValid(...) | `CurrentItemIsValid__Q26Quazal15IteratorOverDOsCFv` |
| `0x82a52ab8` | `0x8008e370` | bsim 16.6 | - | BSIM | Quazal::IteratorOverDOs::IteratorOverDOs(...) | `__ct__Q26Quazal15IteratorOverDOsFbb` |

### Job.o — network, 2 ids (high 0, ≥30 0, 20-30 1, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Core/Job.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aa1578` | `0x8002e250` | bsim 22.6 | - | BSIM | Quazal::Job::PerformExecution(...) | `PerformExecution__Q26Quazal3JobFRCQ26Quazal4Time` |
| `0x82aa1700` | `0x8002e420` | bsim 16.1 | - | BSIM | Quazal::Job const::WaitForCompletion(...) | `WaitForCompletion__Q26Quazal3JobCFUi` |

### JobBackEndServicesLogin.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 2, 10-15 0, contra 0)  ·  `src/network/Services/JobBackEndServicesLogin.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aae1c8` | `0x800e33b0` | bsim 16.5 | - | BSIM | Quazal::JobBackEndServicesLogin::ProcessAuthenticationResult(...) | `ProcessAuthenticationResult__Q26Quazal23JobBackEndServicesLoginFv` |
| `0x82aaeec0` | `0x800e4060` | bsim 16.2 | - | BSIM | Quazal::JobBackEndServicesLogin::CompleteJob(...) | `CompleteJob__Q26Quazal23JobBackEndServicesLoginFQ26Quazal7qResult` |

### JobBackEndServicesLogout.o — network, 2 ids (high 0, ≥30 0, 20-30 2, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Services/JobBackEndServicesLogout.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82ab0b58` | `0x800e42a0` | bsim 22.8 | - | BSIM | Quazal::JobBackEndServicesLogout::JobBackEndServicesLogout(...) | `__ct__Q26Quazal24JobBackEndServicesLogoutFUiPQ26Quazal15BackEndServicesPQ26Quazal11Credentials` |
| `0x82ab1320` | `0x800e49a0` | bsim 20.4 | - | BSIM | Quazal::JobBackEndServicesLogout::ProcessSecConnDisconnectionResult(...) | `ProcessSecConnDisconnectionResult__Q26Quazal24JobBackEndServicesLogoutFv` |

### JobChangeConnection.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 2, 10-15 0, contra 0)  ·  `src/network/ObjDup/JobChangeConnection.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aca9c8` | `0x8008ee70` | bsim 18.9 | - | BSIM | Quazal::JobChangeConnection::JobChangeConnection(...) | `__ct__Q26Quazal19JobChangeConnectionFRCQ26Quazal6StringQ26Quazal8DOHandle` |
| `0x82acab38` | `0x8008ef80` | bsim 15.1 | - | BSIM | Quazal::JobChangeConnection::ReportJobCompletion(...) | `ReportJobCompletion__Q26Quazal19JobChangeConnectionFv` |

### JobTerminateFacade.o — network, 2 ids (high 0, ≥30 0, 20-30 1, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Products/JobTerminateFacade.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82abb458` | `0x800f9060` | bsim 25.2 | - | BSIM | Quazal::JobTerminateFacade::JobTerminateFacade(...) | `__ct__Q26Quazal18JobTerminateFacadeFPQ26Quazal13ProductFacadeUi` |
| `0x82abb858` | `0x800f96a0` | bsim 16.0 | - | BSIM | Quazal::JobTerminateFacade::ClearTheStore(...) | `ClearTheStore__Q26Quazal18JobTerminateFacadeFv` |

### Jobs_Wii.o — network, 2 ids (high 0, ≥30 0, 20-30 2, 15-20 0, 10-15 0, contra 0)  ·  `src/network/net/Jobs_Wii.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a5e520` | `0x80125e50` | bsim 25.1 | - | BSIM | stlpmtx_std::_List_base<Ui,Q26Quazal16MemAllocator<Ui>>::clear(...) | `clear__Q211stlpmtx_std42_List_base<Ui,Q26Quazal16MemAllocator<Ui>>Fv` |
| `0x82b0d350` | `0x80125dd0` | bsim 23.5 | - | BSIM | stlpmtx_std::_List_base<Q26Quazal6String,Q26Quazal30MemAllocator<Q26Quazal6String>>::clear(...) | `clear__Q211stlpmtx_std70_List_base<Q26Quazal6String,Q26Quazal30MemAllocator<Q26Quazal6String>>Fv` |

### Joypad.o — system, 2 ids (high 0, ≥30 0, 20-30 1, 15-20 1, 10-15 0, contra 0)  ·  `src/system/os/Joypad.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x825113c0` | `0x8042f5d0` | bsim 29.4 | - | BSIM | JoypadKeepAlive(...)   [free function] | `JoypadKeepAlive__Fib` |
| `0x82511900` | `0x80430710` | bsim 19.1 | - | BSIM | JoypadGetCalbertValue(...)   [free function] | `JoypadGetCalbertValue__Fib` |

### KerberosAuthentication.o — network, 2 ids (high 0, ≥30 0, 20-30 1, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Services/KerberosAuthentication.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82af1638` | `0x800d9520` | bsim 21.9 | - | BSIM | Quazal::KerberosAuthentication::ValidateConnectionResponse(...) | `ValidateConnectionResponse__Q26Quazal22KerberosAuthenticationFPQ26Quazal9BitStreamUi` |
| `0x82af1338` | `0x800d92b0` | bsim 15.1 | - | BSIM | Quazal::KerberosAuthentication::ValidateConnectionRequest(...) | `ValidateConnectionRequest__Q26Quazal22KerberosAuthenticationFPQ26Quazal9BitStreamPQ26Quazal9BitStreamPQ26Quazal20AuthenticationClientPUiPPQ26Quazal6Ticket` |

### MasterStationRef.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 2, 10-15 0, contra 0)  ·  `src/network/ObjDup/MasterStationRef.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a857f8` | `0x8009cd50` | bsim 16.1 | - | BSIM | Quazal::MasterStationRef::MasterStationRef(...) | `__ct__Q26Quazal16MasterStationRefFv` |
| `0x82a858b8` | `0x8009cdf0` | bsim 17.0 | - | BSIM | Quazal::MasterStationRef::MasterStationRef(...) | `__ct__Q26Quazal16MasterStationRefFQ26Quazal8DOHandleQ26Quazal20LogicalClockTmpl<Uc>` |

### MidiInstrumentMgr.o — system, 2 ids (high 0, ≥30 0, 20-30 1, 15-20 1, 10-15 0, contra 0)  ·  `src/system/synth/MidiInstrumentMgr.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826f5368` | `0x8099b9d0` | bsim 21.7 | - | BSIM | MidiInstrumentMgr::UnloadInstrument(...) | `UnloadInstrument__17MidiInstrumentMgrFv` |
| `0x826f53c0` | `0x8099ba40` | bsim 16.1 | - | BSIM | MidiInstrumentMgr::Poll(...) | `Poll__17MidiInstrumentMgrFv` |

### PRUDPStream.o — network, 2 ids (high 0, ≥30 0, 20-30 1, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Plugins/PRUDPStream.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82accd68` | `0x8005a2f0` | bsim 22.4 | - | BSIM | Quazal::PRUDPStream::PRUDPStream(...) | `__ct__Q26Quazal11PRUDPStreamFQ36Quazal6Stream4TypePQ26Quazal13RootTransport` |
| `0x82acdd28` | `0x8005b1a0` | bsim 19.6 | - | BSIM | Quazal::PRUDPStream::OpenEndPoint(...) | `OpenEndPoint__Q26Quazal11PRUDPStreamFUi` |

### RealGuitarTrackWatcherImpl.o — system, 2 ids (high 0, ≥30 0, 20-30 1, 15-20 1, 10-15 0, contra 0)  ·  `src/system/beatmatch/RealGuitarTrackWatcherImpl.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8277a250` | `0x80642de0` | bsim 29.4 | - | BSIM | RealGuitarTrackWatcherImpl::PollHook(...) | `PollHook__26RealGuitarTrackWatcherImplFf` |
| `0x82779b88` | `0x80643320` | bsim 17.7 | - | BSIM | RealGuitarTrackWatcherImpl::FretButtonUp(...) | `FretButtonUp__26RealGuitarTrackWatcherImplFi` |

### Scheduler.o — network, 2 ids (high 0, ≥30 0, 20-30 2, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Core/Scheduler.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a99878` | `0x80031570` | bsim 29.9 | - | BSIM | Quazal::Scheduler::SingleThreadDispatch(...) | `SingleThreadDispatch__Q26Quazal9SchedulerFUi` |
| `0x82a99b90` | `0x80031760` | bsim 20.6 | - | BSIM | Quazal::Scheduler::GlobalSingleThreadDispatch(...) | `GlobalSingleThreadDispatch__Q26Quazal9SchedulerFUi` |

### SecureEndPoint.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 2, 10-15 0, contra 0)  ·  `src/network/Services/SecureEndPoint.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aef1a8` | `0x800f6290` | bsim 17.4 | - | BSIM | Quazal::SecureEndPoint::SetAssociatedEndPoint(...) | `SetAssociatedEndPoint__Q26Quazal14SecureEndPointFPQ26Quazal8EndPoint` |
| `0x82aef7f8` | `0x800f6b80` | bsim 16.9 | - | BSIM | Quazal::SecureEndPoint::CompleteClose(...) | `CompleteClose__Q26Quazal14SecureEndPointFv` |

### SessionDiscoveryTable.o — network, 2 ids (high 0, ≥30 0, 20-30 0, 15-20 2, 10-15 0, contra 0)  ·  `src/network/Plugins/SessionDiscoveryTable.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a95648` | `0x80042470` | bsim 15.8 | - | BSIM | Quazal::SessionDiscoveryTable::Create(...) | `Create__Q26Quazal21SessionDiscoveryTableFv` |
| `0x82a957d0` | `0x80042520` | bsim 15.8 | - | BSIM | Quazal::SessionDiscoveryTable::Delete(...) | `Delete__Q26Quazal21SessionDiscoveryTableFv` |

### SessionSearcher_RV.o — network, 2 ids (high 0, ≥30 0, 20-30 2, 15-20 0, 10-15 0, contra 0)  ·  `src/network/net/SessionSearcher_RV.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82ab7420` | `0x80113320` | bsim 27.9 | - | BSIM | stlpmtx_std::_List_base<Q26Quazal53AnyObjectHolder<Q26Quazal9Gathering,Q26Quazal6String>,Q26Quazal78MemAllocator<Q26Quazal53AnyObjectHolder<Q26Quazal9Gathering,Q26Quazal6String>>>::clear(...) | `clear__Q211stlpmtx_std166_List_base<Q26Quazal53AnyObjectHolder<Q26Quazal9Gathering,Q26Quazal6String>,Q26Quazal78MemAllocator<Q26Quazal53AnyObjectHolder<Q26Quazal9Gathering,Q26Quazal6String>>>Fv` |
| `0x82ad0de8` | `0x801132b0` | bsim 25.1 | - | BSIM | stlpmtx_std::_List_base<i,Q26Quazal15MemAllocator<i>>::clear(...) | `clear__Q211stlpmtx_std40_List_base<i,Q26Quazal15MemAllocator<i>>Fv` |

### StationProbeList.o — network, 2 ids (high 0, ≥30 0, 20-30 2, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Plugins/StationProbeList.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x825e9688` | `0x8004cff0` | bsim 21.2 | - | BSIM | __median<Ui,Q230@unnamed@StationProbeList_cpp@17AscendingPingSort>__11stlpmtx_stdFRCUiRCUiRCUiQ230@unnamed@StationProbeList_cpp@17AscendingPingSort_RCUi | `__median<Ui,Q230@unnamed@StationProbeList_cpp@17AscendingPingSort>__11stlpmtx_stdFRCUiRCUiRCUiQ230@unnamed@StationProbeList_cpp@17AscendingPingSort_RCUi` |
| `0x82ad3c48` | `0x8004d4e0` | bsim 22.2 | - | BSIM | Quazal::StationProbeList::Trace(...) | `Trace__Q26Quazal16StationProbeListFUi` |

### StationState.o — network, 2 ids (high 0, ≥30 0, 20-30 2, 15-20 0, 10-15 0, contra 0)  ·  `src/network/ObjDup/StationState.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a97860` | `0x800b8f60` | bsim 22.0 | - | BSIM | Quazal::StationState::OperationBegin(...) | `OperationBegin__Q26Quazal12StationStateFPQ26Quazal11DOOperation` |
| `0x82a978d8` | `0x800b8fd0` | bsim 22.2 | - | BSIM | Quazal::StationState::OperationEnd(...) | `OperationEnd__Q26Quazal12StationStateFPQ26Quazal11DOOperation` |

### StreamSettings.o — network, 2 ids (high 0, ≥30 0, 20-30 1, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Plugins/StreamSettings.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a3fba0` | `0x80053490` | bsim 25.4 | - | BSIM | Quazal::StreamSettings::SetKey(...) | `SetKey__Q26Quazal14StreamSettingsFRCQ26Quazal6String` |
| `0x82a3fa20` | `0x80053390` | bsim 19.6 | - | BSIM | Quazal::StreamSettings::StreamSettings(...) | `__ct__Q26Quazal14StreamSettingsFv` |

### SystemComponent.o — network, 2 ids (high 0, ≥30 0, 20-30 1, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Core/SystemComponent.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a791d8` | `0x80036960` | bsim 23.4 | - | BSIM | Quazal::SystemComponent::WaitForTerminatedState(...) | `WaitForTerminatedState__Q26Quazal15SystemComponentFUi` |
| `0x82a78870` | `0x80036050` | bsim 15.0 | - | BSIM | Quazal::SystemComponent::SetState(...) | `SetState__Q26Quazal15SystemComponentFQ36Quazal15SystemComponent6_Stateb` |

### TransportSignatureGenerator.o — network, 2 ids (high 0, ≥30 0, 20-30 1, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Plugins/TransportSignatureGenerator.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b01ae0` | `0x800542d0` | bsim 21.5 | - | BSIM | Quazal::TransportSignatureGenerator::ComputeSourceSignature(...) | `ComputeSourceSignature__Q26Quazal27TransportSignatureGeneratorFUiUs` |
| `0x82b016c8` | `0x800540f0` | bsim 18.0 | - | BSIM | Quazal::TransportSignatureGenerator::TransportSignatureGenerator(...) | `__ct__Q26Quazal27TransportSignatureGeneratorFv` |

### AccountManagementClient.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Services/AccountManagementClient.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aa94d8` | `0x800cdb40` | bsim 15.9 | - | BSIM | Quazal::AccountManagementClient::AccountManagementClient(...) | `__ct__Q26Quazal23AccountManagementClientFv` |

### ArpeggioShape.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/system/bandobj/ArpeggioShape.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82343188` | `0x804e0610` | bsim 15.7 | - | BSIM | ArpeggioShape const::GetYPos(...) | `GetYPos__13ArpeggioShapeCFv` |

### BandScoreboard.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/system/bandobj/BandScoreboard.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8258c6f8` | `0x80533dc0` | bsim 16.1 | - | BSIM | BandScoreboard::SetNumStars(...) | `SetNumStars__14BandScoreboardFfb` |

### BandwidthCounter.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Platform/BandwidthCounter.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82ac8fc8` | `0x8001a8b0` | bsim 19.6 | - | BSIM | __apl__Q26Quazal16BandwidthCounterFUi | `__apl__Q26Quazal16BandwidthCounterFUi` |

### BeatMaster.o — system, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/system/beatmatch/BeatMaster.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82749778` | `0x8061d400` | bsim 29.4 | - | BSIM | BeatMaster::LoaderPoll(...) | `LoaderPoll__10BeatMasterFv` |

### BitStream.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Plugins/BitStream.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82af0160` | `0x8003d660` | bsim 16.6 | - | BSIM | Quazal::BitStream::AdjustLength(...) | `AdjustLength__Q26Quazal9BitStreamFv` |

### BlockMgr.o — system, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/system/os/BlockMgr.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827a88a8` | `0x8040de10` | bsim 20.8 | - | BSIM | stlpmtx_std::_List_base<9AsyncTask,Q211stlpmtx_std24StlNodeAlloc<9AsyncTask>>::clear(...) | `clear__Q211stlpmtx_std64_List_base<9AsyncTask,Q211stlpmtx_std24StlNodeAlloc<9AsyncTask>>Fv` |

### CacheMgr_Wii.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/system/utl/CacheMgr_Wii.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827b31a0` | `0x8048d680` | bsim 16.4 | - | BSIM | CacheMgrWii::Poll(...) | `Poll__11CacheMgrWiiFv` |

### CallProtocolMethodOperation.o — network, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Protocol/CallProtocolMethodOperation.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82abfe18` | `0x8006c7b0` | bsim 21.6 | - | BSIM | Quazal::CallProtocolMethodOperation::CallProtocolMethodOperation(...) | `__ct__Q26Quazal27CallProtocolMethodOperationFv` |

### CallRegister.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/ObjDup/CallRegister.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82ab2178` | `0x800733c0` | bsim 18.1 | - | BSIM | Quazal::CallRegister::Start(...) | `Start__Q26Quazal12CallRegisterFv` |

### CharBone.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/system/char/CharBone.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x823fa368` | `0x806805e0` | bsim 16.2 | - | BSIM | CharBone::CharBone(...) | `__ct__8CharBoneFv` |

### CharClipGroup.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/system/char/CharClipGroup.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8237b5c0` | `0x806a1560` | bsim 15.4 | - | BSIM | CharClipGroup::GetClip(...) | `GetClip__13CharClipGroupFv` |

### CharDriver.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/system/char/CharDriver.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8244e668` | `0x806b27f0` | bsim 18.0 | - | BSIM | CharDriver::SetBones(...) | `SetBones__10CharDriverFP15CharBonesObject` |

### CharHair.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/system/char/CharHair.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x823f85d0` | `0x806d02a0` | bsim 15.6 | - | BSIM | __amu__7Vector3Ff | `__amu__7Vector3Ff` |

### CharKeyHandMidi.o — system, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/system/bandobj/CharKeyHandMidi.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x822beb78` | `0x80573c10` | bsim 20.5 | - | BSIM | CharKeyHandMidi::DefaultSelectFinger(...) | `DefaultSelectFinger__15CharKeyHandMidiFQ215CharKeyHandMidi11KeyboardKey` |

### ClipCompressor.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/system/char/ClipCompressor.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82262298` | `0x8072d450` | bsim 18.5 | - | BSIM | MakeString<PCc,f,f>(...)   [free function] | `MakeString<PCc,f,f>__FPCcPCcff_PCc` |

### CompressionAlgorithm.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Plugins/CompressionAlgorithm.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b0fdb0` | `0x8003cc20` | bsim 16.5 | - | BSIM | Quazal::CompressionAlgorithm::CompressionAlgorithm(...) | `__ct__Q26Quazal20CompressionAlgorithmFv` |

### ConnectionInfo.o — network, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/ObjDup/ConnectionInfo.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aa0c28` | `0x80076860` | bsim 21.7 | - | BSIM | Quazal::ConnectionInfo const::GetURL(...) | `GetURL__Q26Quazal14ConnectionInfoCFi` |

### ConnectionInfoDDL.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/ObjDup/ConnectionInfoDDL.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aa7728` | `0x80076c90` | bsim 17.4 | - | BSIM | Quazal::_DS_ConnectionInfo const::FormatVariableValue(...) | `FormatVariableValue__Q26Quazal18_DS_ConnectionInfoCFPQ26Quazal8VariablePQ26Quazal6String` |

### ConnectionManager.o — network, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Plugins/ConnectionManager.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82abe2d8` | `0x800440e0` | bsim 20.0 | - | BSIM | Quazal::ConnectionManager::ConfigureEndPointForRouting(...) | `ConfigureEndPointForRouting__Q26Quazal17ConnectionManagerFPQ26Quazal8EndPoint` |

### Core.o — network, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Core/Core.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aac338` | `0x8002d1c0` | bsim 23.5 | - | BSIM | Quazal::Core::Core(...) | `__ct__Q26Quazal4CoreFv` |

### CreditsPanel.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/system/meta/CreditsPanel.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8278a2b8` | `0x8074c100` | bsim 15.0 | - | BSIM | CreditsPanel::OnMsg(...) | `OnMsg__12CreditsPanelFRC13ButtonDownMsg` |

### DOCore.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/ObjDup/DOCore.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a929d8` | `0x80079a90` | bsim 19.3 | - | BSIM | Quazal::DOCore::DOCore(...) | `__ct__Q26Quazal6DOCoreFv` |

### DOHandle.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/ObjDup/DOHandle.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a6a848` | `0x8007c6c0` | bsim 15.5 | - | BSIM | Quazal::DOHandle::SetDOClassID(...) | `SetDOClassID__Q26Quazal8DOHandleFUi` |

### DOOperation.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/ObjDup/DOOperation.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82ac4230` | `0x8007cbe0` | bsim 19.2 | - | BSIM | Quazal::DOOperation::DOOperation(...) | `__ct__Q26Quazal11DOOperationFQ26Quazal8DOHandlePQ26Quazal16DuplicatedObject` |

### DataNode.o — system, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/system/obj/DataNode.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82725950` | `0x8045ef10` | bsim 27.5 | - | BSIM | DataNode::DataNode(...) | `__ct__8DataNodeFRC8DataNode` |

### DateTime.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/system/os/DateTime.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8250fc50` | `0x8041dc20` | bsim 19.3 | - | BSIM | DateTime const::ToDateString(...) | `ToDateString__8DateTimeCFR6String` |

### DirLoader.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/system/obj/DirLoader.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8276a698` | `0x80471950` | bsim 16.9 | - | BSIM | MakeString<8FilePath,f>(...)   [free function] | `MakeString<8FilePath,f>__FPCc8FilePathf_PCc` |

### EnvAnim.o — system, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/system/rndobj/EnvAnim.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8244e6f0` | `0x8089ada0` | bsim 24.5 | - | BSIM | __ls<Q23Hmx5Color>__FR10TextStreamRC17Key<Q23Hmx5Color>_R10TextStream | `__ls<Q23Hmx5Color>__FR10TextStreamRC17Key<Q23Hmx5Color>_R10TextStream` |

### EventHandler.o — network, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Platform/EventHandler.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82ac49a8` | `0x8001c440` | bsim 24.6 | - | BSIM | Quazal::EventHandler::ResetEvent(...) | `ResetEvent__Q26Quazal12EventHandlerFPQ26Quazal5Event` |

### FIRFilter.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/system/synthwii/soundtouch/FIRFilter.cpp`  ·  DC3 shared  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b46bd0` | `0x8097aca0` | bsim 16.8 | - | BSIM | soundtouch::FIRFilter const::evaluate(...) | `evaluate__Q210soundtouch9FIRFilterCFPsPCsUiUi` |

### Faders.o — system, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/system/synth/Faders.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826edab0` | `0x80987050` | bsim 23.7 | - | BSIM | Fader const::GetTargetDb(...) | `GetTargetDb__5FaderCFv` |

### FaultProcessingContext.o — network, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/ObjDup/FaultProcessingContext.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82adb638` | `0x8008b460` | bsim 22.4 | - | BSIM | Quazal::FaultProcessingContext::FaultProcessingContext(...) | `__ct__Q26Quazal22FaultProcessingContextFv` |

### FillInfo.o — system, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/system/beatmatch/FillInfo.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8276d430` | `0x80626880` | bsim 23.1 | - | BSIM | FillInfo const::NextFillExtents(...) | `NextFillExtents__8FillInfoCFiR10FillExtent` |

### GameGem.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/system/beatmatch/GameGem.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8276a088` | `0x80626d50` | bsim 18.7 | - | BSIM | GameGem::GameGem(...) | `__ct__7GameGemFRC12MultiGemInfo` |

### HxGuid.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/system/utl/HxGuid.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827a6220` | `0x80497e40` | bsim 15.5 | - | BSIM | HxGuid const::IsNull(...) | `IsNull__6HxGuidCFv` |

### IDGenerator.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/ObjDup/IDGenerator.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82ac3080` | `0x8008c740` | bsim 16.0 | - | BSIM | Quazal::IDGenerator::IDGenerator(...) | `__ct__Q26Quazal11IDGeneratorFv` |

### IIRFilter.o — system, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/system/dsp/IIRFilter.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b52b40` | `0x80745d60` | bsim 23.4 | - | BSIM | IIR4PoleFilter::FilterSlow(...) | `FilterSlow__14IIR4PoleFilterFf` |

### JobDisconnectStation.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/ObjDup/JobDisconnectStation.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82acaf20` | `0x80092580` | bsim 19.7 | - | BSIM | Quazal::JobDisconnectStation::CheckExceptions(...) | `CheckExceptions__Q26Quazal20JobDisconnectStationFv` |

### JobListenOnWellKnown.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/ObjDup/JobListenOnWellKnown.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aa0a38` | `0x80095b40` | bsim 18.9 | - | BSIM | Quazal::JobListenOnWellKnown::SetDefaultPostExecutionState(...) | `SetDefaultPostExecutionState__Q26Quazal20JobListenOnWellKnownFv` |

### JobManageAccount.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Services/JobManageAccount.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82ae3a08` | `0x800d1af0` | bsim 18.9 | - | BSIM | Quazal::JobManageAccount::JobManageAccount(...) | `__ct__Q26Quazal16JobManageAccountFUiPQ26Quazal31AccountManagementProtocolClientPQ26Quazal24AccountManagementCommand` |

### JobProcessFault.o — network, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/ObjDup/JobProcessFault.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a47dc0` | `0x800968a0` | bsim 23.7 | - | BSIM | Quazal::RefTemplate<Q26Quazal16PromotionReferee> const::IsValid(...) | `IsValid__Q26Quazal40RefTemplate<Q26Quazal16PromotionReferee>CFv` |

### JobProcessMessage.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/ObjDup/JobProcessMessage.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82abfc88` | `0x8009a270` | bsim 18.2 | - | BSIM | Quazal::JobProcessMessage::JobProcessMessage(...) | `__ct__Q26Quazal17JobProcessMessageFPQ26Quazal14ObjDupProtocolPQ26Quazal7Message` |

### JobTerminateDOCore.o — network, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/ObjDup/JobTerminateDOCore.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82af63d8` | `0x8009a850` | bsim 24.5 | - | BSIM | Quazal::JobTerminateDOCore::SyncTermination(...) | `SyncTermination__Q26Quazal18JobTerminateDOCoreFv` |

### JobTicketManagerAcquireTicket.o — network, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Services/JobTicketManagerAcquireTicket.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b25178` | `0x800d7c90` | bsim 25.3 | - | BSIM | Quazal::JobTicketManagerAcquireTicket::PrepareCall(...) | `PrepareCall__Q26Quazal29JobTicketManagerAcquireTicketFv` |

### Jobs_RV.o — network, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/net/Jobs_RV.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a4b790` | `0x800fc9d0` | bsim 23.5 | - | BSIM | stlpmtx_std::_List_base<Q26Quazal10StationURL,Q26Quazal35MemAllocator<Q26Quazal10StationURL>>::clear(...) | `clear__Q211stlpmtx_std80_List_base<Q26Quazal10StationURL,Q26Quazal35MemAllocator<Q26Quazal10StationURL>>Fv` |

### JsonUtils.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/net/JsonUtils.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b534a8` | `0x800fccb0` | bsim 15.6 | - | BSIM | JsonArray::AddMember(...) | `AddMember__9JsonArrayFP10JsonObject` |

### KerberosEncryption.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Services/KerberosEncryption.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b09570` | `0x800dfdf0` | bsim 18.9 | - | BSIM | Quazal::KerberosEncryption::InitializeKey(...) | `InitializeKey__Q26Quazal18KerberosEncryptionFPQ26Quazal11CallContextPCcPQ26Quazal3Key` |

### MD5Checksum.o — network, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Plugins/MD5Checksum.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82af3cc0` | `0x8003cb80` | bsim 20.9 | - | BSIM | Quazal::MD5Checksum::ComputeChecksum(...) | `ComputeChecksum__Q26Quazal11MD5ChecksumFRCQ26Quazal6BufferPQ26Quazal6Buffer` |

### MatchMakingClient.o — network, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Services/MatchMakingClient.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a5fb78` | `0x800e61a0` | bsim 21.1 | - | BSIM | Quazal::MatchMakingClient::MatchMakingClient(...) | `__ct__Q26Quazal17MatchMakingClientFv` |

### MatchmakingSettings.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/net/MatchmakingSettings.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8227caa0` | `0x800fda60` | bsim 15.2 | - | BSIM | MakeString<i>(...)   [free function] | `MakeString<i>__FPCci_PCc` |

### MemMgr.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/system/utl/MemMgr.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827977d0` | `0x804a0a70` | bsim 18.1 | - | BSIM | _MemAlloc(...)   [free function] | `_MemAlloc__Fii` |

### MessageBroker.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/net/MessageBroker.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82796720` | `0x800ff820` | bsim 18.4 | - | BSIM | MakeString<i,i>(...)   [free function] | `MakeString<i,i>__FPCcii_PCc` |

### MeterDisplay.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/system/bandobj/MeterDisplay.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82309e88` | `0x805c5920` | bsim 16.3 | - | BSIM | MeterDisplay::Copy(...) | `Copy__12MeterDisplayFPCQ23Hmx6ObjectQ33Hmx6Object8CopyType` |

### MigrationContext.o — network, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/ObjDup/MigrationContext.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a91ef0` | `0x8009e450` | bsim 24.0 | - | BSIM | Quazal::MigrationContext::MigrateObjectImpl(...) | `MigrateObjectImpl__Q26Quazal16MigrationContextFQ26Quazal8DOHandleQ26Quazal8DOHandle` |

### NetCacheMgr.o — system, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/system/utl/NetCacheMgr.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827a8980` | `0x804b2020` | bsim 24.6 | - | BSIM | NetCacheMgr::EnterUnloadState(...) | `EnterUnloadState__11NetCacheMgrFv` |

### NetSearchResult.o — network, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/net/NetSearchResult.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x823e2d98` | `0x801026c0` | bsim 25.4 | - | BSIM | NetSearchResult::Load(...) | `Load__15NetSearchResultFR9BinStream` |

### NetSession.o — network, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/net/NetSession.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x823d2f18` | `0x8010a800` | bsim 23.8 | - | BSIM | NetSession::SendMsgToAll(...) | `SendMsgToAll__10NetSessionFR10NetMessage10PacketType` |

### NetStream.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/system/os/NetStream.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x822c4880` | `0x80439900` | bsim 15.1 | - | BSIM | NetStream::ReadAsync(...) | `ReadAsync__9NetStreamFPvi` |

### Operation.o — network, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Core/Operation.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a870e0` | `0x8002eca0` | bsim 22.8 | - | BSIM | Quazal::Operation const::Trace(...) | `Trace__Q26Quazal9OperationCFQ36Quazal9Operation6_Event` |

### OutputFormat.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Platform/OutputFormat.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82abbf28` | `0x8001fad0` | bsim 18.6 | - | BSIM | Quazal::OutputFormat::OutputFormat(...) | `__ct__Q26Quazal12OutputFormatFv` |

### PatchDir.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/system/bandobj/PatchDir.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82266500` | `0x805ad930` | bsim 18.9 | - | BSIM | PatchDir::SaveRemote(...) | `SaveRemote__8PatchDirFR9BinStream` |

### PatchRenderer.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/system/bandobj/PatchRenderer.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8229c790` | `0x805b2060` | bsim 17.9 | - | BSIM | PatchRenderer::Terminate(...) | `Terminate__13PatchRendererFv` |

### ProfilePicture.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/system/os/ProfilePicture.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b5e520` | `0x80441510` | bsim 15.5 | - | BSIM | ProfilePicture::Poll(...) | `Poll__14ProfilePictureFv` |

### PromotionReferee.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/ObjDup/PromotionReferee.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a9f840` | `0x800a4990` | bsim 19.6 | - | BSIM | Quazal::PromotionReferee::ComputeAffinity(...) | `ComputeAffinity__Q26Quazal16PromotionRefereeFQ26Quazal8DOHandlePUcPi` |

### QuazalSession.o — network, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/net/QuazalSession.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x823e0048` | `0x80117850` | bsim 27.7 | - | BSIM | QuazalSession::Poll(...) | `Poll__13QuazalSessionFv` |

### RGUtl.o — system, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/system/beatmatch/RGUtl.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827554c8` | `0x80645140` | bsim 27.2 | - | BSIM | HandleInterval(...)   [free function] | `HandleInterval__FPciRC7GameGemiRi` |

### RMCContext.o — network, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/ObjDup/RMCContext.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a6e960` | `0x800a61e0` | bsim 24.4 | - | BSIM | Quazal::RMCContext::ProcessResponse(...) | `ProcessResponse__Q26Quazal10RMCContextFQ26Quazal11UserContextPQ36Quazal11CallContext6_StatePQ36Quazal13DOCallContext8_Outcome` |

### RandomNumberGenerator.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Platform/RandomNumberGenerator.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aca0a8` | `0x80021d10` | bsim 18.5 | - | BSIM | Quazal::RandomNumberGenerator::GetRandomNumber(...) | `GetRandomNumber__Q26Quazal21RandomNumberGeneratorFUi` |

### RemoteLogDeviceServer.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Extensions/RemoteLogDeviceServer.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aaf110` | `0x800c68f0` | bsim 18.9 | - | BSIM | Quazal::RemoteLogDeviceServer::RemoteLogDeviceServer(...) | `__ct__Q26Quazal21RemoteLogDeviceServerFPQ26Quazal21ProtocolRequestBroker` |

### Rnd.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/system/rndobj/Rnd.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8247d378` | `0x8092f880` | bsim 19.4 | - | BSIM | Rnd::Rnd(...) | `__ct__3RndFv` |

### RootDODDL.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/ObjDup/RootDODDL.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a6b528` | `0x800a72f0` | bsim 18.9 | - | BSIM | Quazal::_DO_RootDO::CalleeAddDuplicaLocationStub(...) | `CalleeAddDuplicaLocationStub__Q26Quazal10_DO_RootDOFPQ26Quazal7Message` |

### RootTransport.o — network, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Plugins/RootTransport.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a40030` | `0x8004ac60` | bsim 20.2 | - | BSIM | Quazal::RootTransport::RootTransport(...) | `__ct__Q26Quazal13RootTransportFv` |

### SecureConnectionClient.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Services/SecureConnectionClient.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a5eb00` | `0x800f3c90` | bsim 15.9 | - | BSIM | Quazal::SecureConnectionClient::SecureConnectionClient(...) | `__ct__Q26Quazal22SecureConnectionClientFv` |

### SessionDescription.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Plugins/SessionDescription.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a9d298` | `0x800409e0` | bsim 17.6 | - | BSIM | Quazal::SessionDescription::SessionDescription(...) | `__ct__Q26Quazal18SessionDescriptionFv` |

### SessionInfo.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/ObjDup/SessionInfo.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a93f88` | `0x800af670` | bsim 18.6 | - | BSIM | Quazal::SessionInfo::SetSessionName(...) | `SetSessionName__Q26Quazal11SessionInfoFPCc` |

### SessionMessages.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/net/SessionMessages.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x823de5a0` | `0x801141d0` | bsim 19.9 | - | BSIM | UserLeftMsg::UserLeftMsg(...) | `__ct__11UserLeftMsgFP4User` |

### SessionSpace.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Extensions/SessionSpace.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b19898` | `0x800c53b0` | bsim 19.9 | - | BSIM | Quazal::SessionSpace::InitializeSpecialRelations(...) | `InitializeSpecialRelations__Q26Quazal12SessionSpaceFv` |

### SlidingWindow.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Plugins/SlidingWindow.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b1b4b0` | `0x8005df70` | bsim 18.3 | - | BSIM | Quazal::SlidingWindow::SlidingWindow(...) | `__ct__Q26Quazal13SlidingWindowFUs` |

### Socket.o — network, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Platform/Socket.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aec130` | `0x80029c00` | bsim 22.7 | - | BSIM | Quazal::Socket::Socket(...) | `__ct__Q26Quazal6SocketFUi` |

### SongData.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/system/beatmatch/SongData.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8274c518` | `0x8064f360` | bsim 15.3 | - | BSIM | SongData::AddMultiGem(...) | `AddMultiGem__8SongDataFiRC12MultiGemInfo` |

### SpotlightDrawer.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/system/world/SpotlightDrawer.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x824c1a60` | `0x8086b360` | bsim 16.4 | - | BSIM | SpotlightDrawer::DeSelect(...) | `DeSelect__15SpotlightDrawerFv` |

### StationContactInfo.o — network, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Plugins/StationContactInfo.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b01368` | `0x8004bba0` | bsim 25.7 | - | BSIM | Quazal::StationContactInfo::Trace(...) | `Trace__Q26Quazal18StationContactInfoFUi` |

### StationDDL.o — network, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/ObjDup/StationDDL.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a554e0` | `0x800b3ea0` | bsim 21.6 | - | BSIM | Quazal::_DO_Station::CallOperationOnDatasets(...) | `CallOperationOnDatasets__Q26Quazal11_DO_StationFPQ26Quazal11DOOperationQ36Quazal9Operation6_Event` |

### StationIdentificationDDL.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/ObjDup/StationIdentificationDDL.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aa7510` | `0x800b51c0` | bsim 15.9 | - | BSIM | Quazal::_DS_StationIdentification const::FormatVariableValue(...) | `FormatVariableValue__Q26Quazal25_DS_StationIdentificationCFPQ26Quazal8VariablePQ26Quazal6String` |

### StationManager.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/ObjDup/StationManager.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a8b720` | `0x800b7820` | bsim 15.3 | - | BSIM | Quazal::StationManager::ConnectStation(...) | `ConnectStation__Q26Quazal14StationManagerFQ26Quazal8DOHandle` |

### StationProbe.o — network, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Plugins/StationProbe.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82ad9540` | `0x8004bfb0` | bsim 21.6 | - | BSIM | Quazal::StationProbe::Trace(...) | `Trace__Q26Quazal12StationProbeFUi` |

### StoreArtLoaderPanel.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/system/meta/StoreArtLoaderPanel.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82792300` | `0x807610b0` | bsim 17.0 | - | BSIM | StoreArtLoaderPanel::IsAllArtLoadedOrFailed(...) | `IsAllArtLoadedOrFailed__19StoreArtLoaderPanelFv` |

### Stream.o — network, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Plugins/Stream.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a4fb50` | `0x800528f0` | bsim 22.2 | - | BSIM | Quazal::PseudoGlobalVariable<Q26Quazal14StreamSettings>::FreeExtraContexts(...) | `FreeExtraContexts__Q26Quazal47PseudoGlobalVariable<Q26Quazal14StreamSettings>Fv` |

### StreamManager.o — network, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Services/StreamManager.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aad118` | `0x800e0660` | bsim 27.0 | - | BSIM | Quazal::StreamManager::Initialize(...) | `Initialize__Q26Quazal13StreamManagerFUsUcUi` |

### SynthSample.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/system/synth/SynthSample.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82709ef0` | `0x809c0c70` | bsim 16.9 | - | BSIM | SynthSample::SynthSample(...) | `__ct__11SynthSampleFv` |

### System.o — system, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/system/os/System.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x824fd150` | `0x80443080` | bsim 24.3 | - | BSIM | SystemPoll(...)   [free function] | `SystemPoll__Fb` |

### SystemComponentGroup.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Core/SystemComponentGroup.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82abb000` | `0x800372e0` | bsim 15.2 | - | BSIM | Quazal::SystemComponentGroup::DoWork(...) | `DoWork__Q26Quazal20SystemComponentGroupFv` |

### SystemComponents.o — network, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Core/SystemComponents.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aedf08` | `0x800376b0` | bsim 23.2 | - | BSIM | Quazal::SystemComponents::SystemComponents(...) | `__ct__Q26Quazal16SystemComponentsFv` |

### Task.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/system/obj/Task.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82ad3180` | `0x8047eae0` | bsim 18.8 | - | BSIM | stlpmtx_std::_S_remove_if<Pv,Q211stlpmtx_std16StlNodeAlloc<Pv>,Q211stlpmtx_std48(...) | `_S_remove_if<Pv,Q211stlpmtx_std16StlNodeAlloc<Pv>,Q211stlpmtx_std48__unary_pred_wrapper<Q23Hmx6Object,10ObjMatchPr>>__11stlpmtx_stdFRQ211stlpmtx_std48_List_impl<Pv,Q211stlpmtx_std16StlNodeAlloc<Pv>>Q211stlpmtx_std48__unary_pred_wrapper<Q23Hmx6Object,10ObjMatchPr>_v` |

### ThreadVariable.o — network, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/Platform/ThreadVariable.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aa5b78` | `0x80026c30` | bsim 22.1 | - | BSIM | Quazal::ThreadVariableList::ClearValue(...) | `ClearValue__Q26Quazal18ThreadVariableListFv` |

### Ticket.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Services/Ticket.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b14ff8` | `0x800da850` | bsim 16.2 | - | BSIM | Quazal::Ticket::Decrypt(...) | `Decrypt__Q26Quazal6TicketFPQ26Quazal18KerberosEncryption` |

### TicketManager.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Services/TicketManager.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82adf7b8` | `0x800d9c30` | bsim 17.4 | - | BSIM | Quazal::TicketManager::FindTicket(...) | `FindTicket__Q26Quazal13TicketManagerFUi` |

### TimeConversion.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/system/utl/TimeConversion.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827a3e50` | `0x804d4a40` | bsim 16.8 | - | BSIM | MsToBeat(...)   [free function] | `MsToBeat__Ff` |

### TimeoutManager.o — network, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/network/Plugins/TimeoutManager.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b01d08` | `0x80061ec0` | bsim 18.7 | - | BSIM | Quazal::TimeoutManager::TimeoutManager(...) | `__ct__Q26Quazal14TimeoutManagerFv` |

### UIResource.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/system/ui/UIResource.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827fde10` | `0x807f30f0` | bsim 16.9 | - | BSIM | UIResource::UIResource(...) | `__ct__10UIResourceFRC8FilePath` |

### UISlider.o — system, 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1, 10-15 0, contra 0)  ·  `src/system/ui/UISlider.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827e4c98` | `0x807f6ff0` | bsim 15.6 | - | BSIM | UISlider::DrawShowing(...) | `DrawShowing__8UISliderFv` |

### UITransitionHandler.o — system, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/system/ui/UITransitionHandler.cpp`  ·  DC3 shared

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827dc698` | `0x807f9400` | bsim 20.5 | - | BSIM | UITransitionHandler const::HasTransitions(...) | `HasTransitions__19UITransitionHandlerCFv` |

### UpdatePolicy.o — network, 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0, 10-15 0, contra 0)  ·  `src/network/ObjDup/UpdatePolicy.cpp`  ·  DC3 cannot-provide  *(rb3 src absent)*

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82aa6d68` | `0x800b9c20` | bsim 21.4 | - | BSIM | Quazal::UpdatePolicy::AddToDiscoveryMessage(...) | `AddToDiscoveryMessage__Q26Quazal12UpdatePolicyFPQ26Quazal16DuplicatedObjectPvUcPQ26Quazal7StationPQ26Quazal7Message` |

---

## For the next agent

- **Regenerate:** `python3 tools/gen_sysnet_port_worklist.py` (cwd-independent; reads `ghidriff_identities.json` + `scripts/target_symbol_map.json` + the rb3 CW map; the script VERIFIES every `wii_symbol` resolves to its claimed Bank-8 addr and that 0 entries are already in the production map, exiting non-zero on any failure).
- **Data feed:** `sysnet_port_worklist.json` — per-fn rows + `tu_summary` + `ranked_tus` for machine ingestion.
- **Port the safe-first core first** (HIGH + BSim≥30, human-judged 1.000), then the BSim 20–30 tier, then **confirm-on-consume each BSim 15–20 row** before trusting its name.
- **Do NOT inject these into `target_symbol_map.json`** — CW≠MSVC mangling, TUs uncompiled; wrong key mis-pairs objdiff. Confirm each name when the TU is actually ported (`gen_game_target_map.py --tu <TU>`).
- **DC3 first for shared rows.** For `DC3?=shared` engine TUs, DC3's already-decomp'd body (`/dc3-pair`, BinDiff) is the faster base; this worklist's value is highest on the `DC3?=cannot-provide` Quazal/ObjDup netcode rows.
- **Watch the dominant failure mode:** same-TU sibling aliasing (the lone miss was `TrackWidget::Init` vs `::Empty`, vtable slot 0x44 vs 0xc). It bites hardest in **bsim15-20**.
