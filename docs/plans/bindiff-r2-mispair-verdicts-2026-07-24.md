# BinDiff round-2 mispair adjudication — 36 conflicts

Method: `band.exe` (= current TU5 target, byte-verified vs Ghidra `default_tu5.xex-c5a170`) gives direct target bytes by VA. DC3 `ham_xbox_r.exe` (leaked-map named) is the ground-truth oracle. For each conflict I compared the DC3 function at `dc3_va` against BOTH RB3 VAs via (a) opcode-stream similarity, (b) `.pdata` function-entry authority + function size, (c) referenced-string decode, and (d) Ghidra decompile. All four signals agreed in every case.

## Verdict tally

| verdict | count |
|---|---|
| REPOINT-TO-CANDIDATE | 35 |
| KEEP-EXISTING | 1 |

Confidence: {'high': 32, 'medium': 4}

## Key finding

The existing map entries in these 36 conflicts are overwhelmingly **mispairs** (older-pass errors): 33 point at non-`.pdata` leaf/fragment addresses or tiny real-but-different functions; the BinDiff-round-2 candidates are clean `.pdata` function entries whose structure matches the DC3 oracle. **35 REPOINT-TO-CANDIDATE, 1 KEEP-EXISTING.** No existing entry in this set is a strict-100 match in report.json (all are unpinned or 0%), so no repoint loses a live match. All candidate VAs are currently unmapped (clean adds).

## KEEP-EXISTING (BinDiff was wrong)

- **CharDriverMidi::SyncProperty** — keep existing `0x823cc9e8`. It is a 388-byte `.pdata` entry (DC3 = 380B), opcode-sim 0.979, and references `parser`/`flag_parser`/`blend_override_pct` = genuine CharDriverMidi. The candidate `0x823cbb20` references `target`/`first_spot`/`second_spot` = a different class. This is the lone BinDiff round-2 mispair in the set (its BinDiff sim was the low 0.788).

## ADDR collision 0x82637888 — adjudication

**REPOINT to the candidate** `?SyncProperty@FlowPickOne@@$4...@Z` (vtordisp adjustor thunk). The three instructions at 0x82637888 (`lwz r11,-4(r3); subf r3,r11,r3; b -0x210`) are **byte-identical** to DC3 FlowPickOne's vtordisp thunk at `82406468`; it is a 12-byte adjustor thunk followed by padding, not a real function. The existing label `KickPlayer@SessionUsersProvider(BandUser*)` is a mispair — the real KickPlayer(BandUser*) already exists at `0x82654350` (a duplicate map entry), so removing the 0x82637888 mislabel loses nothing.

## REPOINT decisive-evidence table

| symbol | existing→ | cand→ | DC3 sz | cand sz | sim cand | sim exist | BinDiff | conf |
|---|---|---|---|---|---|---|---|---|
| UpdateDebugParsers::Song | 0X827A0AD8 | 0x827c5a60 | 124 | 124 | 0.968 | 0.211 | 0.888 | high |
| Draw::RndOverlay | 0X824042A0 | 0x82416f38 | 636 | 564 | 0.907 | 0.086 | 0.87 | high |
| InitParticle::RndParticleSys | 0X82435588 | 0x82447f00 | 2732 | 2492 | 0.815 | 0.036 | 0.953 | high |
| SynthPoll::MidiInstrument | 0X826F5FB0 | 0x82713b40 | 228 | 208 | 0.954 | 0.095 | 0.988 | high |
| FixClassName::DirLoader | 0X82730090 | 0x82754df8 | 2260 | 2260 | 1.0 | 0.018 | 0.99 | high |
| BuildScroll::UIListState | 0X827E93C8 | 0x8280e838 | 588 | 592 | 0.936 | 0.012 | 0.905 | high |
| DrawShowing::WorldInstance | 0X824D72D8 | 0x824ea0f8 | 96 | 116 | 0.906 | 0.071 | 0.983 | high |
| ?DataACos@@YA?AVDataNode@@PAVD | 0X824E2108 | 0x824f50c8 | 148 | 136 | 0.93 | 0.1 | 0.982 | high |
| ?DataATan@@YA?AVDataNode@@PAVD | 0X824E2190 | 0x824f5150 | 148 | 136 | 0.93 | 0.091 | 0.982 | high |
| IsValid_AOReceive::RndAmbientOcclusion | 0X8247BC80 | 0x8248e670 | 212 | 192 | 0.95 | 0.274 | 0.946 | high |
| ?DataASin@@YA?AVDataNode@@PAVD | 0X824E2080 | 0x824f5040 | 148 | 136 | 0.93 | 0.14 | 0.981 | high |
| MakeWorldSphere::RndMesh | 0X824054A8 | 0x82417fe8 | 444 | 432 | 0.959 | 0.034 | 0.912 | high |
| ?JoypadPoll@@YAXXZ | 0X825165A8 | 0x82529ae0 | 16 | 24 | 0.8 | 0.034 | 1.0 | high |
| Init::MemcardMgr | 0X82787198 | 0x827abfd0 | 296 | 316 | 0.902 | 0.0 | 0.972 | high |
| SortBySize::BlockStatTable | 0X827B3DD8 | 0x827d93a8 | 88 | 88 | 1.0 | 0.375 | 0.967 | high |
| Process::FlangerEffect | 0X82B82558 | 0x82bb5650 | 824 | 768 | 0.668 | 0.082 | 0.795 | medium |
| UpdateTransforms::Spotlight | 0X824C5E18 | 0x824d8fc8 | 1456 | 1340 | 0.518 | 0.022 | 0.909 | medium |
| Save::BaseMaterial | 0X824233C0 | 0x82435dc0 | 1192 | 988 | 0.352 | 0.026 | 0.963 | medium |
| Copy::UIFontImporter | 0X826B28F8 | 0x826d1e90 | 420 | 112 | 0.346 | 0.088 | 0.918 | medium |
| HideHint::MoviePanel | 0X8278AAC8 | 0x827afa70 | 132 | 116 | 0.935 | 0.14 | 0.966 | high |
| Save::UIList | 0X827D3878 | 0x827f8dd0 | 484 | 448 | 0.953 | 0.129 | 0.967 | high |
| ??0ChunkStream@@QAA@PBDW4FileT | 0X827A51A8 | 0x827ca488 | 440 | 380 | 0.849 | 0.188 | 0.956 | high |
| PreInit::Symbol | 0X8279B558 | 0x827c04f8 | 172 | 100 | 0.706 | 0.202 | 0.94 | high |
| DoVelocity::NgPostProc | 0X82B5ADF8 | 0x82b89878 | 224 | 180 | 0.891 | 0.159 | 0.734 | high |
| IsTranslucent::RndBitmap | 0X823EB5A0 | 0x823fe230 | 156 | 148 | 0.921 | 0.302 | 0.896 | high |
| Init::BlockMgr | 0X82519920 | 0x8252ce70 | 352 | 344 | 0.77 | 0.299 | 0.938 | high |
| 5::YAAAVBinStream | 0X82405EA8 | 0x824189e8 | 124 | 124 | 0.839 | 0.278 | 0.941 | high |
| ?SendRawData@@YAXHEEEEEEE@Z | 0X82516688 | 0x82529bc0 | 308 | 216 | 0.824 | 0.257 | 0.822 | high |
| ShowHint::MoviePanel | 0X8278AB40 | 0x827afae8 | 160 | 144 | 0.921 | 0.269 | 0.947 | high |
| EnforceMinimumTargetDistance::CharEyes | 0X823711F8 | 0x82383680 | 216 | 216 | 1.0 | 0.032 | 1.0 | high |
| SyncProperty::CharSleeve | 0X823BDAD8 | 0x823d0030 | 812 | 776 | 0.957 | 0.074 | 0.935 | high |
| Print::CharBonesSamples | 0X823CC7E8 | 0x823df2d0 | 340 | 308 | 0.926 | 0.114 | 0.971 | high |
| ??0CharDriver@@IAA@XZ | 0X82366A88 | 0x82378f38 | 644 | 488 | 0.707 | 0.168 | 0.943 | high |
| StaticClassName::GamePanel | 0x82675a80,0x8264bce8 | 0x82675ad0 | 88 | 88 | 1.0 | 0.513 | 0.967 | high |
| FlowPickOne::SyncProperty (vtordisp thunk) | 0X82637888 | 0x82637888 | thunk | 12 | byte-identical | — | — | high |

## Medium-confidence repoints (low opcode-sim from RB3↔DC3 revision drift, but existing is an impossible stub)

- **?Process@FlangerEffect@@QAAXPAMHH@Z** → 0x82bb5650: DC3-oracle opcode-sim: cand 0.668 vs existing 0.082 (BinDiff 0.795). Candidate is a .pdata function entry sz768 (DC3 sz824); existing 0X82B82558 sz56 is a non-pdata leaf/fragment = mispair.
- **?UpdateTransforms@Spotlight@@IAAXXZ** → 0x824d8fc8: DC3-oracle opcode-sim: cand 0.518 vs existing 0.022 (BinDiff 0.909). Candidate is a .pdata function entry sz1340 (DC3 sz1456); existing 0X824C5E18 sz24 is a non-pdata leaf/fragment = mispair.
- **?Save@BaseMaterial@@UAAXAAVBinStream@@@Z** → 0x82435dc0: DC3-oracle opcode-sim: cand 0.352 vs existing 0.026 (BinDiff 0.963). Candidate is a .pdata function entry sz988 (DC3 sz1192); existing 0X824233C0 sz32 is a non-pdata leaf/fragment = mispair.
- **?Copy@UIFontImporter@@UAAXPBVObject@Hmx@@W4CopyType@23@@Z** → 0x826d1e90: DC3-oracle opcode-sim: cand 0.346 vs existing 0.088 (BinDiff 0.918). Candidate is a .pdata function entry sz112 (DC3 sz420); existing 0X826B28F8 sz32 is a non-pdata leaf/fragment = mispair.

Note: Copy@UIFontImporter and Spotlight::UpdateTransforms were additionally Ghidra-verified — the existing VAs decompile to a `trapWord` stub and a trivial 1-call vector-dtor fragment respectively, while the candidates decompile to genuine member-copy / transform-update bodies.
