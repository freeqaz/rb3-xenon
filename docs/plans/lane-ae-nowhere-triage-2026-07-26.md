# Lane AE round 2 — NOWHERE-pool triage

Read-only analysis. Input: `/home/free/tmp/laneAE2_funnel.json` → `NOWHERE` key,
302 rows `[unit, symbol, size]`. Scanner: `scripts/harvest/unemitted_symbol_scan.py`
(re-run fresh against current build: `emitted_symbols=747844`, `unemitted=306`,
of which all 302 funnel NOWHERE rows are present — 4 extra are build-state drift
from other lanes). Predecessor doc: `docs/plans/lane-ae-unemitted-symbols.md`.

No source edits, no builds, no commits were made. This file is the only output.

## 1. Reproduced bucket table (pre-baked exclusions applied)

| Bucket | Rows | Bytes | Rule |
|---|---:|---:|---|
| VECDTOR | 25 | 1,960 | vector/ObjPtrVec/container dtor family — exact match to predecessor's prior count on this exclusion, a strong stability check |
| MAP-ARTIFACT | 21 | 420 | `??_E<Class>@@UAAPAXI@Z` (deleting-dtor-thunk map defect), `lbl_*`/`merged_*`/`__MERGED_*`, ≤8-byte fragments, foreign-namespace strings (NUISPEECH/FaceCore/TrueColor/`_NUIP_`/XAUDIO2/ATL/`ST::`/XShaderPDBBuilder) |
| STL-INST | 96 | 14,576 | STL/stlport template instantiation noise (explicit specializations, allocator glue, etc.) |
| VENDOR-UNWIRED | 93 | 28,084 | XDK/Quazal/CRT/vendor-SDK keyword match (sub-split below) |
| **Candidate remainder (before per-row DEAD traps)** | **67** | **12,332** | everything not caught above — individually investigated, see §2–5 |

VENDOR-UNWIRED sub-split (93 rows / 28,084 B): LEAPCORE 28/4052B, XGRAPHICS-Compress
11/3828B, ExternalMic 6/872B, rtti-CRT 4/1624B, CRT-osfinfo 5/1092B,
DxRnd/DxTex/DxMesh 13/3808B, XAPO-vendor 4/500B, fft_/FFT-family 5/6644B,
DSP::Synapse(ATG template) 5/3092B, MicXbox 11/2328B, ctr_encrypt 1/244B.

**Caveat (found during row-level work, not yet re-run programmatically):** at
least 2 rows properly belonging in VENDOR-UNWIRED slipped through the keyword
scan into the 67-row remainder because their keyword wasn't in the matched
list: `XGSurfaceSize` (112B, rnddx9/ShaderMgr — XDK graphics, same family as
XGRAPHICS-Compress but a different symbol prefix) and
`NuipHeadOrientationAllowTitleChangeColorSettings` (44B, DataNode — Kinect NUI
vendor API, same family as NUISPEECH/`_NUIP_` but missed because the keyword
match was substring-based on a narrower list). Also `FIFOSampleBuffer::ptrBegin`
(28B, soundtouch third-party lib) and `XAuthCreateLocalSocket`
(20B, CXNetAdapter — Xbox Live vendor API) and `XShowSocialNetworkImagePostUI`
(68B) and `Gesture::GestureManager_GetFrame` (28B, Kinect NUI gesture vendor
API) are the same class of miss. **True honest VENDOR total is closer to
93+6 = 99 rows / ~28,376+ B**, and the true candidate remainder after
correcting this is **61 rows / ~11,940 B** (not built into the tables below,
which still use the as-scanned 67/12,332 for traceability — treat the 6 rows
above as VENDOR, not ACTIONABLE, in the top-25 table).

## 2. ACTIONABLE remainder by fix shape

Of the 67 candidate rows, after individual investigation:

- **11 rows are DEAD** — refuted by the predecessor's byte-level VA decode or by
  a measured-negative trap (§4).
- **6 rows are VENDOR** (see caveat above, reclassified out of ACTIONABLE).
- **~9 rows are SUSPECT-MISPAIR** (Ham/Band lineage substitution candidates,
  §3) — flagged, not counted as safely actionable without further byte-level
  verification per-row.
- **~5 rows are UNWIRED-TU** (shape 5) — real source in-tree, `.cpp` not in
  `objects.json`.
- **~2 rows are shape-1** (inline-COMDAT force-emit, ~100% reliable).
- **~9 rows are shape-2** (missing definition, oracle exists, same-unit) —
  highest-confidence volume.
- **4 rows are shape-4** (FIX-SIG, MakeString const-ref, systemic — UNVERIFIED,
  no build performed).
- The rest (~19 rows) are individually small, either no-oracle-anywhere
  (needs fresh RE) or not fully resolved — listed with UNVERIFIED status.

### Ranked top-25 (by size)

| Size | Landing unit | Symbol | Shape | Natural owner | Same-unit? | Oracle | Confidence |
|---:|---|---|---|---|---|---|---|
| 2492 | default/Part | `InitParticle@RndParticleSys` | — | — | — | — | **DEAD** — superseded by predecessor's byte decode (was in initial FIX-SIG table, refuted later in same doc) |
| 892 | default/CharEyeDartRuleset | `DoFSM@CharIKFoot` | — | — | — | — | **DEAD** — same, refuted |
| 648 | default/UIPanel | `Load@StoreArtLoaderPanel` | — | — | — | — | **DEAD** — refuted |
| 468 | default/StarDisplay | `GetStarsToken` | — | — | — | — | **DEAD** — refuted |
| 448 | default/TexBlender | `MakeString<PBD,H,PBD,I>` | 4 (FIX-SIG) | `src/system/utl/MakeString.h` | Y (header, per-call-site instantiation) | ours (header, wrong convention) | MEDIUM — mangled bytes decoded to want const-ref params; **UNVERIFIED, no build** |
| 356 | default/system/synth_xbox/FxSendSynapse | `SyncEffectParams@FxSendSynapse360` | 5 (UNWIRED-TU) | `src/system/synth_xbox/FxSendSynapse360.cpp` | file exists, not in objects.json | dc3 (file matches) | HIGH — confirmed via grep, not in `objects.json` |
| 348 | default/FilterCoeffs | `HighpassCoefficients@DSP` | 2 | `src/system/synth_xbox/FilterCoeffs.h/.cpp` | Y | dc3 (complete impl in header, `namespace DSP`) | **HIGHEST** — ours is a 1-line empty stub, dc3's is a complete drop-in |
| 344 | default/BandCamShot | `operator<<(BinStream&, BandCamShot::Target const&)` | — | — | — | — | SUSPECT-MISPAIR (§3) |
| 344 | default/FilterCoeffs | `LowpassCoefficients@DSP` | 2 | same as above | Y | dc3 | **HIGHEST** — paired with Highpass, 692B combined, single best finding in this pool |
| 316 | default/BandUser | `MetaPerformer::MetaPerformer(HamSongMgr&,char const*)` | — | — | — | — | SUSPECT-MISPAIR (§3) |
| 296 | default/PostProc_NG | `AddSink@Object@Hmx` | — | — | — | — | **DEAD** — refuted |
| 244 | default/DirLoader | `InitObject@Object@Hmx` | — | — | — | — | **DEAD** — known trap: `virtual` on InitObject measured **−598** |
| 356→ | (Fx family, cont'd below top-10 for context) | | | | | | |
| 208 | default/MidiInstrument | `SynthPoll@MidiInstrument` | 2 | `src/system/synth/MidiInstrument.cpp` | Y (landing unit = natural owner file) | dc3 (`MidiInstrument.cpp/.h` present) | MEDIUM-HIGH — oracle confirmed present, body diff not individually verified line-by-line |
| 188 | default/band3/meta_band/AccomplishmentProgress | `GiveAvatarAsset@AccomplishmentProgress` | 2 | `src/band3/meta_band/AccomplishmentProgress.cpp` | Y | dc3 `lazer/meta_ham/AccomplishmentProgress.cpp` (Ham lineage) | MEDIUM — oracle exists but is Ham-side, needs Ham→Band port check |
| 184 | default/OSCMessenger | `SetBestBattleScore@AppLabel` | — | — | — | — | SUSPECT-MISPAIR (§3) |
| 176 | default/Joypad_Xbox | `ParseRawData` | 2? | `src/system/os/Joypad_Xbox.cpp` | Y | dc3 `Joypad_Xbox.cpp` present (has permuter-work leftovers, i.e. actively worked on there) | MEDIUM — not individually diffed |
| 176 | default/band3/meta_band/AccomplishmentProgress | `GiveGamerpic@AccomplishmentProgress` | 2 | same file as GiveAvatarAsset | Y | dc3 Ham lineage | MEDIUM |
| 172 | default/StorePreviewMgr | `PreviewDownloadCompleteMsg::PreviewDownloadCompleteMsg(bool,bool)` | — | — | — | — | **DEAD** — refuted |
| 168 | default/Joypad_Xbox | `ReadSingleJoypad` | 2? | `Joypad_Xbox.cpp` | Y | dc3 present | MEDIUM — not individually diffed |
| 164 | default/band3/meta_band/ProfileMgr | `ProfileChangedMsg::ProfileChangedMsg(Profile*)` | 4? | class located in-tree | Y (likely) | needs check | **UNVERIFIED** — signature diff vs `(Profile*)` not resolved |
| 164 | default/UI | `EventDialogDismissMsg::EventDialogDismissMsg(Symbol,Symbol)` | — | — | — | — | **DEAD** — refuted |
| 152 | default/BandUser | `HasAsFriend@BandUser` | 2 | `src/band3/game/BandUser.cpp` | Y | dc3 `HamUser::HasAsFriend` at `HamUser.cpp:37`, decl `HamUser.h:17` | **HIGH** — real Ham→Band body-port, call site pattern (`HANDLE_EXPR`) matches |
| 144 | default/system/obj/Dir | `operator<<(BinStream&, ObjDirPtr<HamScrollSpeedIndicator> const&)` | — | — | — | — | SUSPECT-MISPAIR (Ham lineage template ODR, §3) |
| 140 | default/system/rndobj/Rnd | `LocalTalkerIsHeadsetPresent` | — | — | — | — | VENDOR-leaning (Xbox Live voice API name) — not confidently ACTIONABLE |
| 136 | default/band3/bandtrack/TrackPanel | `ConnectionStatusChangedMsg::ConnectionStatusChangedMsg(bool)` | — | — | — | — | UNVERIFIED — not individually resolved |
| 136 | default/BandCamShot | `CheckNoFlashcardsCondition@AccomplishmentSongConditional` | — | — | — | — | **DEAD** — refuted |

(Rows beyond size 136 down to 12 bytes make up the remaining ~40 rows of the
67; the highest-value ones among them are pulled into the shape sections
below. Nothing below ~90 bytes changes the overall picture — see §6 for the
honest total.)

### Shape-1 candidates (inline-COMDAT force-emit, ~100% reliable)

| Size | Symbol | Landing unit | Note |
|---:|---|---|---|
| 88 | `StaticClassName@BandButton` | default/BandHighlight | `BandButton.h` is a real, complete class with `OBJ_CLASSNAME(BandButton)` inline (`DECLARE_MESSAGE`/`OBJ_CLASSNAME` pattern) — MSVC only emits the COMDAT in a TU that ODR-uses it. Mechanically identical to the already-landed OvershellDir/PatchRenderer/ReviewDisplay fixes (+9 landed per predecessor doc). |
| 88 | `Type@ProfilePictureFetchedMsg` | default/band3/bandtrack/TrackPanel | `DECLARE_MESSAGE(ProfilePictureFetchedMsg, "profile_picture_fetched_msg")` at `src/system/os/ProfilePicture.h:35` defines `Type()` inline in-class-body (see `Msg.h:176-183`) — same mechanical shape. |
| 88 | `StaticClassName@ScrollbarDisplay` | default/CameraTilt | **BLOCKED, not a clean shape-1**: `src/system/bandobj/Band.cpp:63` shows `ScrollbarDisplay` is still a fake 1-line stub class (`class ScrollbarDisplay { public: static void Init(); };`), not the real class. The real class exists at `rb3-Wii`'s `src/system/bandobj/ScrollbarDisplay.h/.cpp` (oracle located, not yet ported). A macro/decl fix alone won't work here — needs the real class ported first. |
| 88 | `Type@MicrophonesChangedMsg` | default/Mic | Same DECLARE_MESSAGE family as ProfilePictureFetchedMsg, **but the class itself does not exist anywhere** — grepped `MicrophonesChangedMsg` across our tree, dc3, and rb3-Wii: zero hits in all three. Not a simple force-emit; the message class needs to be authored from scratch (likely from the retail PE/Ghidra), no oracle. |

### Shape-2 candidates (missing definition, oracle exists, same-unit)

| Size | Symbol | Oracle | Confidence |
|---:|---|---|---|
| 348+344=692 | `HighpassCoefficients`/`LowpassCoefficients@DSP` | dc3 `synth_xbox/FilterCoeffs.h` — complete inline impl, ours is empty | **Single strongest finding in the whole pool.** Near-mechanical copy. |
| 152 | `HasAsFriend@BandUser` | dc3 `HamUser::HasAsFriend` (`HamUser.cpp:37`) | HIGH |
| 88 | `Unloading@StorePanel` | dc3 `StorePanel::Unloading` (`lazer/meta_ham/MainMenuPanel.cpp:95-110`) — `if (mState != 1 && !TheNetCacheMgr->IsUnloaded()) return true; return UIPanel::Unloading();` | HIGH |
| 40 | `CreateFx@FxSendReverb360` | dc3 `synth_xbox/FxSendReverb.cpp:77-81` — `IUnknown *FxSendReverb360::CreateFx() { IUnknown *apo; CreateAudioReverb(&apo); return apo; }`. Confirmed: our `FxSendReverb.h` **does** declare the real `FxSendReverb360` class (unlike PitchShift/Synapse it's not a separate `360.cpp`), but our `FxSendReverb.cpp` has **no** `CreateFx` body at all. | HIGH — clean, isolated missing method |
| 208 | `SynthPoll@MidiInstrument` | dc3 `synth/MidiInstrument.cpp/.h` present, same-unit | MEDIUM-HIGH (not line-diffed) |
| 188+176=364 | `GiveAvatarAsset`/`GiveGamerpic@AccomplishmentProgress` | dc3 Ham-lineage `lazer/meta_ham/AccomplishmentProgress.cpp`, same-unit (band3 file exists) | MEDIUM (Ham→Band port needed, not copy-paste) |
| 176+168 | `ParseRawData`/`ReadSingleJoypad` (Joypad_Xbox) | dc3 `os/Joypad_Xbox.cpp` present (has permuter-work scratch files, i.e. actively being worked elsewhere) | MEDIUM (not individually diffed; risk of collision with another lane) |

### Shape-4 (FIX-SIG, systemic, UNVERIFIED — no build performed)

**MakeString by-value vs const-ref.** `src/system/utl/MakeString.h`'s multi-arg
template overloads (`MakeString(const char*, T1, T2, ...)`) take params **by
value**; dc3's equivalent takes them **by const reference**
(`const T1&, const T2&, ...`). Manually decoded one target mangled name from
the pool, `??$MakeString@HHH@@YAPBDPBDABH11@Z` (80B, OutfitConfig row): the
`ABH` codes (`const int&`) confirm retail's actual wanted signature for the
3-int-arg instantiation is const-ref, matching DC3's convention — **not**
matching our current by-value convention. All 4 MakeString rows in the pool
decode the same way (`ABQBD`/`ABVSymbol@@`/`ABH`/`ABG` = const-ref
throughout):

| Size | Landing unit | Template args |
|---:|---|---|
| 448 | TexBlender | `<const char*, int, const char*, unsigned int>` |
| 80 | OutfitConfig | `<int,int,int>` |
| 36 | DepthBuffer3D | `<Symbol, const char*, const char*>` |
| 28 | VocalTrackDir | `<const char*, array, const char*, int, short>` |

**Caveat — this is NOT a blind "copy DC3" fix.** Our `MakeString.h` carries a
load-bearing comment explaining a *prior, deliberate* deviation from DC3: the
buffer `mFmtBuf[0x800]` (vs DC3's `[0x1000]`) is retail-verified via a stack
frame proof, and the comment states "retail inlines the **single-arg**
overload... by value" — i.e. a previous investigation already confirmed
by-value is *correct* for the 1-arg overload specifically. The multi-arg
overloads may be a **different case** that was swept into the same by-value
change without individual verification. This finding is: (a) internally
consistent (all 4 pool rows' mangled bytes agree), (b) **not build-verified**
— I made no edits, ran no build. Recommend a scoped A/B in a worktree before
trusting it as a lever: flip only the multi-arg overloads to const-ref,
leave the single-arg one alone, rebuild, and confirm no regression on
whatever call sites currently match. Flagged **UNVERIFIED**.

### Shape-5 — UNWIRED-TU (full list, the priority deliverable)

Real source in-tree (or with a straightforward oracle), simply not compiled —
`.cpp` missing from `config/45410914/objects.json`. Confirmed by direct grep
of `objects.json` plus (for DrawUtl) direct COFF-symbol parsing of every
`.obj` under `build/45410914/src/system/gesture/`:

| File | Status | Pool rows it would help pair (needs a scatter-include into the landing TU too — wiring alone doesn't move THESE specific rows unless the new TU's span covers them) |
|---|---|---|
| `src/system/synth_xbox/FxSendPitchShift360.cpp` (+`.h`) | Confirmed: exists, matches dc3, **not** in objects.json (only non-360 `FxSendPitchShift.cpp` is wired) | `SyncEffectParams@FxSendPitchShift360` (76B, landing unit default/system/synth_xbox/FxSendPitchShift) |
| `src/system/synth_xbox/FxSendSynapse360.cpp` (+`.h`) | Same pattern | `SyncEffectParams@FxSendSynapse360` (356B), `CreateFx@FxSendSynapse360` (72B) — both landing in default/system/synth_xbox/FxSendSynapse |
| `src/system/gesture/DrawUtl.cpp` (+`.h`) | Confirmed via COFF parse of all gesture/*.obj: `TerminateDrawUtl` defined nowhere. Real, already-ported source exists, byte-matches dc3, called from `GestureMgr.cpp:194` | `TerminateDrawUtl` (112B, landing unit default/Voice — a **different** TU, so wiring DrawUtl.cpp alone won't flip this row without an additional scatter-include) |
| `src/system/os/PlatformMgr_Xbox.cpp` | File-level gap confirmed real and unwired (matches dc3, e.g. `InviteParty(int)` at line 242) — **but** the specific pool row attributed to it (`InviteParty@PlatformMgr`, 116B) is a **map mislabel** per the predecessor's byte-decode, not proof of this specific row's fix. Reporting the file-level gap only; do not price this row as fixed by wiring. |
| `src/system/bandobj/ScrollbarDisplay` | Not a wiring gap — the **class itself** is a 1-line fake stub in `Band.cpp:63`. rb3-Wii oracle (`src/system/bandobj/ScrollbarDisplay.h/.cpp`) has the real class, not yet ported. This is a **real port**, not a splits/objects.json wiring fix. |

Total confirmed UNWIRED-TU byte impact in this specific pool: **~616B** across
4 rows (76+356+72+112), plus the PlatformMgr_Xbox and ScrollbarDisplay
file-level gaps that don't directly price any pool row here.

## 3. SUSPECT-MISPAIR — Ham/Band lineage cluster (flagged, not priced as safe)

Per the known trap (retail contains BOTH `Band*`/`meta_band` and
`Ham*`/`meta_ham` families; DC3 is *newer* than retail — do not assume DC3's
version is simply "more correct"):

| Size | Symbol | Note |
|---:|---|---|
| 344 | `operator<<(BinStream&, BandCamShot::Target const&)` | Same `BandCamShot::Target` class/struct + `ObjList`/`ObjVector` container mismatch cluster the predecessor doc already flagged (6-row cluster there) |
| 316 | `MetaPerformer::MetaPerformer(HamSongMgr&, char const*)` | `MetaPerformer` ctor taking a Ham type — predecessor doc's cluster |
| 184 | `SetBestBattleScore@AppLabel(HamProfile*,int)` | Takes `HamProfile*` — same lineage-substitution question as `GetProfileFromPad` below |
| 144 | `operator<<(BinStream&, ObjDirPtr<HamScrollSpeedIndicator> const&)` | Ham-typed template instantiation |
| 108 | `ObjList<BandCamShot::Target>::operator=` | Same BandCamShot::Target cluster, container-type question |
| 84 | `GetProfileFromPad@ProfileMgr` returning `HamProfile*` | Predecessor doc's exact "GetProfileFromPad const/HamProfile" FIX-SIG row |

These are **not** re-priced up or down here — the predecessor's doc already
treats this cluster as requiring careful per-row verification of which
variant retail actually wants (wrong-variant-selected vs DC3-newer-than-retail
both being live possibilities), and I did not do fresh byte-level VA
verification on any of these 6 in this pass. Treat as **UNVERIFIED,
higher-effort** than the shape-2/shape-1 rows above.

## 4. DEAD rows (known-trap or byte-decode refuted)

| Symbol | Trap |
|---|---|
| `InitParticle@RndParticleSys` (2492B) | Predecessor's deeper byte-level VA decode refuted the initial FIX-SIG hypothesis (superseded within the same doc) |
| `DoFSM@CharIKFoot` (892B) | Same — refuted |
| `Load@StoreArtLoaderPanel` (648B) | Same — refuted |
| `GetStarsToken` (468B) | Same — refuted |
| `AddSink@Object@Hmx` (296B) | Same — refuted |
| `InitObject@Object@Hmx` (244B) | **Measured trap**: adding `virtual` to InitObject measured **−598** strict. Do NOT re-fund. |
| `PreviewDownloadCompleteMsg` ctor (172B) | Refuted |
| `EventDialogDismissMsg` ctor (164B) | Refuted |
| `CheckNoFlashcardsCondition@AccomplishmentSongConditional` (136B) | Refuted |
| `InviteParty@PlatformMgr` (116B) | Map mislabel per predecessor's byte-decode (7 verified handoff VAs) — real file-level gap exists (PlatformMgr_Xbox.cpp unwired) but doesn't fix *this* row |
| `FocusComponent@UIPanel` (40B) | **Measured trap**: adding `virtual` to FocusComponent measured **−14** strict. Do NOT re-fund. |
| `SetChallengerGamertag@AppLabel` (88B) | Confirmed no oracle anywhere (ours/dc3/rb3-Wii) |

## 5. Map/splits defects (report only — NOT edited)

- The `??_E<Class>@@UAAPAXI@Z` deleting-destructor-thunk pattern is a
  confirmed **map defect** (predecessor doc has full evidence + 7 verified
  handoff VAs) — `scripts/target_symbol_map.json` was **not** touched here.
- 3 `??_G<Class>@@UAAPAXI@Z` rows in this pool (`CustomPlaylist` 76B,
  `Playlist` 76B, `MetagameStats` 76B) are the primary deleting-destructor
  form (not the `??_E` thunk defect) — their classes were located
  (`src/meta_ham/Playlist.h`/`MetagameStats.h`) but their virtual-dtor status
  was **not** individually confirmed in this pass. **UNVERIFIED**, do not
  assume these are the same map-defect class as the `??_E` cluster.
- `_icf_arbitrary` VAs (25 in the map) are bytes-true-identity-unresolved
  artifacts per the known trap — none of this pool's rows were re-attributed
  to that set in this pass; noting per instructions that this is not evidence
  either way.

## 6. Honest sizing — the true actionable class after all exclusions

- Raw NOWHERE pool: **302 rows / [sum of all bytes]**.
- After VECDTOR(25/1960) + MAP-ARTIFACT(21/420) + STL-INST(96/14576) +
  VENDOR-UNWIRED(93/28084) exclusions: **67 rows / 12,332 B** remain
  (matches predecessor's exclusion methodology, reproduced cleanly — VECDTOR
  count is an exact match to the predecessor's prior snapshot, a good
  stability signal).
- Of those 67: **11 DEAD** (2,872B — refuted/measured-negative traps),
  **~6 more VENDOR** missed by the keyword scan (see §1 caveat, ~272B: XGSurfaceSize
  112 + Nuip 44 + FIFOSampleBuffer 28 + XAuthCreateLocalSocket 20 + XShowSocial 68
  + GestureManager_GetFrame 28 = 300B), leaving **~50 rows / ~9,160B** genuinely
  open.
- Of that ~50: **~9 SUSPECT-MISPAIR** (Ham/Band, unpriced, ~1,180B),
  **~5 UNWIRED-TU confirmed** (~616B, highest-confidence *class* of fix but
  small in this pool since most rows needing their TUs are elsewhere),
  **2 shape-1** (176B, ~100% reliable once ScrollbarDisplay's real class or the
  BandButton/ProfilePictureFetchedMsg pair is force-emitted — the latter two are
  ready now, ScrollbarDisplay needs a prior real-class port), **~9 shape-2**
  (1,748B, oracle-backed, same-unit, medium-high confidence — **FilterCoeffs'
  692B is the single strongest, cleanest, highest-value fix in the entire
  pool**), **4 shape-4** (892B, MakeString systemic, **UNVERIFIED — no build
  performed, flagged as a lever not a guarantee**), and the remainder (~21
  rows, ~4,500B) are individually small and either have no oracle anywhere
  (needs fresh RE from the PE/Ghidra) or were not fully resolved in this pass
  (marked UNVERIFIED row-by-row above; do not treat as pre-cleared).

**Bottom line: the honest, currently-actionable-with-reasonable-confidence
slice of this specific 302-row pool is small — roughly 2,600B across ~16 rows
(2 shape-1 + 9 shape-2 + ~5 shape-5-TU), with FilterCoeffs
(Lowpass/HighpassCoefficients, 692B) as the standout highest-confidence single
finding.** Everything else (MakeString systemic 892B, Ham/Band cluster
~1,180B, ~21 unresolved small rows ~4,500B) is real work but carries
materially lower confidence and is flagged UNVERIFIED rather than costed in.
