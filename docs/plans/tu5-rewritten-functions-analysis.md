# TU0→TU5 rewritten-function analysis (the 81 genuine MISS)

Status: **COMPLETE (read-only analysis).** Date 2026-07-07. Author: investigator agent.
Scope: characterize the 81 genuinely-rewritten functions the base(TU0)→TU5 migration
surfaced, assess same-instrument-patch relevance, and estimate P5 re-decomp cost.

Inputs: `_tu5probe/tu5_migrate/tu5_changed_worklist.json` (478 changed = 81 MISS + 397
AMBIG), `base_to_tu5_map.json`, both PEs (`orig/45410914/band.exe` = TU0,
`band_tu5.exe` = TU5), section-mapped readers `tools/va_disasm.py` / `tools/tu5_va.py`,
and the prior keystone `docs/plans/same-instrument-tu5-retarget.md`.

---

## 0. Method + the load-bearing caveat on the divergence metric

The migration's MISS set has **no recorded TU5 address** (that's *why* they're MISS —
the anchor matcher couldn't place them). I independently re-located each of the 81 in
TU5 by masked-skeleton prologue search (reusing `tools/tu5_skel_recover.py`'s
relocation-normalizing `mask_word`: `b/bl` → keep AA/LK only, `bc` → keep BO/BI/AA/LK,
all D-form imm16 → drop the immediate). **52/81 recovered a unique TU5 entry**; the
other 29 are short leaf/`Exit`/dtor/`ReadImpl` bodies whose ≤6-word masked prologue
collides thousands of times, so my crude anchor can't place them (not evidence they
changed *more*).

For each recovered pair I computed `skel_match_pct` = fraction of masked words that
agree **positionally** over the base length. **This is a lower bound on similarity /
upper bound on change:** it is not alignment-aware, so a single inserted or deleted
instruction near the top shifts the whole tail out of phase and tanks the score even
when the logic is unchanged. Read the buckets accordingly:

- **≥90%** → essentially the same function; the flag is *ripple* (a struct member or
  vtable slot inserted upstream, or a moved callee) not a rewrite. **Trivial re-match.**
- **~60–90%** → moderate edits (a block added/removed) on an otherwise-intact body.
- **<60%** → genuine divergence: real new logic, control-flow changes, or a large
  early insertion. **Real re-decomp.**

Divergence distribution over the 52 recovered: min 8.5%, median 80.5%, max 99.2%.

---

## 1. The 81, grouped by subsystem

`base VA → TU5 VA (recovered) · symbol · size(B, TU0) · skel-match% (change proxy)`.
`—`/`n/a` = not anchorable by prologue skeleton (short leaf); size still from TU0.

### Gameplay (12)
| base VA | TU5 VA | function | size | match% |
|---|---|---|---|---|
| 0x8268d410 | 0x826aabc0 | GemTrainerPanel::Poll | 792 | 17.7% |
| 0x82671c58 | 0x826901c0 | GameMode::SetMode | 832 | 19.2% |
| 0x8258d6e0 | 0x825a5f10 | Campaign::UpdateEndGameInfo | 220 | 23.6% |
| 0x82684130 | 0x826a2828 | Player::CheckCrowdFailure | 228 | 38.6% |
| 0x826721a0 | 0x8268fe80 | GameMode::OnSetMode | 72 | 55.6% |
| 0x82676cb8 | 0x82694f58 | GamePanel::PollForLoading | 328 | 61.0% |
| 0x826853d0 | 0x826a3ad8 | Player::StopDeployingBandEnergy | 432 | 84.3% |
| 0x826893b0 | 0x826a7b38 | Player::Restart | 472 | 97.5% |
| 0x8265c250 | 0x826795a0 | Game::HandleAudioLoad | 352 | 98.9% |
| 0x82672040 | — | GameMode::GameMode (ctor) | 240 | n/a |
| 0x8269ea00 | — | GemPlayer::UpdateGameCymbalLanes | 348 | n/a |
| 0x826d01a0 | — | TrainerGemTab::GetLane | 84 | n/a |

### UI / overshell (16)  ← same subsystem the patch operates in
| base VA | TU5 VA | function | size | match% |
|---|---|---|---|---|
| 0x825c1f80 | 0x825db930 | OvershellSlot::UpdateView | 6852 | 27.1% |
| 0x8254c130 | 0x8255f9d0 | UIStats::MaybePublish | 2532 | 46.6% |
| 0x825c1698 | 0x825dafe0 | OvershellSlot::UpdateState | 2132 | 48.4% |
| 0x825bfb08 | 0x825d93d0 | OvershellSlot::IsQuitToken | 120 | 50.0% |
| **0x8264b5f8** | **0x826684c0** | **OvershellPartSelectProvider::IsActive (Layer A)** | 896 | 56.2% |
| 0x8264bd70 | 0x82668c70 | OvershellPartSelectProvider::Reload | 1224 | 56.5% |
| 0x8259df90 | 0x825b6ad0 | OvershellPanel::ResolveSlotStates | 1448 | 69.3% |
| 0x8264bb18 | 0x82668a20 | OvershellPartSelectProvider::DataSymbol | 40 | 70.0% |
| 0x82608cc0 | 0x82625528 | ManageBandPanel::Exit | 240 | 80.0% |
| 0x82643ae8 | 0x82660818 | OwnedSongSortNode::IsEnabled | 232 | 81.0% |
| 0x82795140 | 0x827ba148 | ConnectionStatusPanel::Exit | 132 | 81.8% |
| 0x82541138 | — | PrefabMgr::AssignPrefabsToSlots | 392 | n/a |
| 0x82542418 | — | PrefabMgr::Handle | 412 | n/a |
| 0x8255a940 | — | MetaPanel::Exit | 64 | n/a |
| 0x82619a88 | — | SigninScreen::Exit | 88 | n/a |
| 0x826719f0 | — | PropertyEventProvider::~ (deleting dtor) | 104 | n/a |

### Online / save / net (11)
| base VA | TU5 VA | function | size | match% |
|---|---|---|---|---|
| 0x82287908 | 0x822d1900 | RockCentral::UpdateChar | 660 | 8.5% |
| 0x8254b1a0 | 0x8255ea58 | AccomplishmentManager::HandleSongCompletedForUser | 292 | 27.4% |
| 0x825464f0 | 0x82559d18 | AccomplishmentManager::HandlePreSongCompletedForUser | 212 | 37.7% |
| 0x82542f20 | 0x825566e8 | AccomplishmentManager::UpdateMiscellaneousSongDataForUser | 320 | 82.5% |
| 0x82787198 | 0x827abfd0 | MemcardMgr::Init | 312 | 84.6% |
| 0x825c7188 | 0x825e0b68 | RockCentral::RecordDataPoint | 1384 | 87.6% |
| 0x82532b48 | 0x825461f0 | ProfileMgr::SaveGlobalOptions | 532 | 91.0% |
| 0x82516688 | 0x82529bc0 | SendRawData (StageKit/USB, free fn) | 216 | 96.3% |
| 0x82519cc8 | — | AsyncFile::ReadAsync | 180 | n/a |
| 0x8251df68 | — | WinSockSocket::Init | 104 | n/a |
| 0x8252c130 | — | MusicLibrary::RebuildProfileData | 176 | n/a |

### Song / audio / MIDI / beatmatch (11)
| base VA | TU5 VA | function | size | match% |
|---|---|---|---|---|
| 0x827c5d30 | 0x827eb3a0 | MidiParser::ParseNote | 224 | 28.6% |
| 0x826f3b50 | 0x82711a00 | MetaMusic::Start | 576 | 32.6% |
| 0x82b87ab8 | 0x82bbab18 | MultiTempoTempoMap::AddTempoInfoPoint | 180 | 51.1% |
| 0x82753fd0 | 0x827788f8 | SongData::Poll | 496 | 58.9% |
| 0x827c2478 | 0x827e7af0 | MidiParser::InsertDataEvent | 240 | 70.0% |
| 0x827c1b18 | 0x827e7198 | MidiParser::PushIdle | 492 | 85.4% |
| 0x826e6d08 | 0x82704c38 | StandardStream::Init | 668 | 92.2% |
| 0x82630a88 | — | SongUpgradeData::~ | 12 | n/a |
| 0x82668d18 | — | SongDB::ClearTrackPhrases | 84 | n/a |
| 0x82669328 | — | SongDB::RebuildPhrases | 68 | n/a |
| 0x826d8a80 | — | Singer::CreateMicClientID | 256 | n/a |

### Content / DLC (4)
| base VA | TU5 VA | function | size | match% |
|---|---|---|---|---|
| 0x8250df08 | 0x825213d0 | ContentMgr::ContentMgr (ctor) | 176 | 84.1% |
| 0x8250c708 | 0x8251fb18 | XboxContent::~XboxContent | 40 | 90.0% |
| 0x8250cd40 | — | XboxContentMgr::Terminate | 24 | n/a |
| 0x82784040 | — | SongMgr::ContentName | 112 | n/a |

### Engine / render / streams (8)
| base VA | TU5 VA | function | size | match% |
|---|---|---|---|---|
| 0x82491df0 | 0x8255d180 | RndShaderProgram::LoadShaderBuffer | 116 | 37.9% |
| 0x824dabc0 | 0x824eda00 | FreeCamera::FreeCamera (ctor) | 136 | 82.4% |
| 0x824da6e0 | 0x824ed500 | FreeCamera::Poll | 1236 | 90.6% |
| 0x823fdd70 | 0x824109c0 | Rnd::DrawString | 44 | 90.9% |
| 0x823a95c0 | — | CharLookAt::Save | 56 | n/a |
| 0x827a4ee0 | — | ChunkStream::ReadImpl | 96 | n/a |
| 0x827a6508 | — | MemStream::ReadImpl | 128 | n/a |
| 0x827a6fc0 | — | BufStream::ReadImpl | 144 | n/a |

### Input / misc (5)
| base VA | TU5 VA | function | size | match% |
|---|---|---|---|---|
| 0x825070d8 | 0x8251a340 | UsbMidiGuitar::Poll | 1252 | 81.5% |
| 0x82262cf8 | 0x82272e68 | main | 76 | 94.7% |
| 0x8250f438 | 0x82522908 | DateTime::ToCode | 96 | 95.8% |
| 0x8250fd18 | — | UserMgr::GetLocalUserFromPadNum | 16 | n/a |
| 0x82513128 | — | OnJoypadStageKitRaw (anon-ns) | 76 | n/a |

### SDK / audio middleware (11)  — replaced wholesale in the native port
| base VA | TU5 VA | function | size | match% |
|---|---|---|---|---|
| 0x82ba4ef0 | 0x82bcc188 | OAPIPELINE::ConvertFromInt8 | 236 | 52.5% |
| 0x82809380 | 0x8282ef30 | __u64tod (CRT) | 48 | 83.3% |
| 0x82bcc9a8 | 0x82bffaf0 | xWMA::prvDecodeSubFrameHeader | 5020 | 95.8% |
| 0x82bd30d8 | 0x82c06220 | xWMA::prvGetNextRunDECVecNonRL | 1412 | 95.8% |
| 0x82bcc3b0 | 0x82bff4f8 | xWMA::prvDecodeFrameHeader | 1528 | 97.1% |
| 0x82bb4810 | 0x82be7cf8 | xWMA::prvDecodeData | 2236 | 98.2% |
| 0x82bc0980 | 0x82bf3c60 | CGraphManager::UpdatePerformanceData (LEAPCORE) | 508 | 99.2% |
| 0x82b83858 | — | OggMap::~OggMap | 92 | n/a |
| 0x82b83910 | — | OggMap::OggMap (ctor) | 128 | n/a |
| 0x82ba1d30 | — | CX2SubmixVoice::SetOutputVoices (XAUDIO2) | 200 | n/a |
| 0x82c08d20 | — | EsConditionalTimersEnabled (dyn-init) | 84 | n/a |

### Other (3, incl. SDK/speech)
| base VA | TU5 VA | function | size | match% |
|---|---|---|---|---|
| 0x82337aa0 | — | BandPatchMesh::WorkVerts::SetMeshVerts | 1280 | n/a |
| 0x82555488 | 0x8256c8d0 | CheckContextModeProperty (anon-ns) | 100 | 64.0% |
| 0x827aabf8 | — | CCfgEngineBase::GetClient (NUISPEECH) | 16 | n/a |

Total TU0 bytes across the 81: **45,704 B**.

---

## 2. The two populations, with representative disassembly

The 81 split cleanly into **ripple-flagged near-identicals** and **genuine rewrites**.
Three worked examples pin the mechanism.

### 2a. Ripple, not rewrite — `Game::HandleAudioLoad` (98.9%)
Instruction-for-instruction identical logic; the ONLY deltas are **struct offsets that
shifted upstream**:

```
BASE 0x8265c250                         TU5 0x826795a0
 lbz  r11, 0x124(r3)                      lbz  r11, 0x12c(r3)   ; member +8
 lwz  r11, 0x64(r3)                       lwz  r11, 0x68(r3)    ; member +4
 bl   0x82756eb0  (SongData::GetGemList)  bl   0x8277b800       ; callee relocated
```
Same frame (`stwu -0x70`), same branch structure, same everything else. The `Game`
object grew a member between TU0 and TU5, so *every* accessor re-encoded — that alone
trips the "changed" flag with zero behavioral edit. `Player::Restart` (97.5%, frame
grew `-0x70→-0x80`), `main` (94.7%), `DrawString`, `ProfileMgr::SaveGlobalOptions`,
`FreeCamera::Poll`, and all four xWMA codecs are the same story: recompile ripple.

### 2b. The upstream cause — a virtual method AND a data member were inserted
`IsActive` (2c below) shows a **vtable slot shift** on the object at `0x3c(this)`:

```
BASE  lwz r11, 0(r3) ; lwz r11, 4(r11) ; mtctr ; bctrl   ; vtable slot +4
TU5   lwz r11, 0(r3) ; lwz r11, 8(r11) ; mtctr ; bctrl   ; vtable slot +8
```
Combined with 2a's data-member insertion, TU5 inserted **both a virtual method and a
data member into a widely-used base class** (a UI/Provider or Hmx::Object-adjacent
base). That single source edit re-lays-out every derived class and re-indexes every
vtable-relative call — which is why **478 functions flag as "changed" while only 81 are
real** and most of those 81 are near-identical. The AMBIG 397 (mostly `<0x80` getters)
are pure layout ripple.

### 2c. Patch-critical — `OvershellPartSelectProvider::IsActive` (Layer A, 56.2%)
Prologue is **byte-identical** and hookable; the shared head computes the same
`(mNumX - mBase)/12 == 0 → return false` guard on the same offsets (0x34/0x30, `li
r10,0xc`, `subf`, `divw.`):

```
BASE 0x8264b5f8            TU5 0x826684c0
 mflr r12                   mflr r12                 ; ← detour overwrites THIS word
 bl   savegprlr (0x82803f2c) bl savegprlr (0x8282924c); CRT block relocated
 lwz  r11, 0x34(r3)         lwz  r11, 0x34(r3)       ; identical guard
 li   r10, 0xc              li   r10, 0xc
 ...
 lwz  r11, 4(r11); bctrl    lwz  r11, 8(r11); bctrl  ; vtable slot +4→+8 (the ripple)
```
The 56% number is dominated by the vtable-slot ripple pushing the body out of phase
plus real edits deeper in — but it is **irrelevant to the patch** (2d/§3).

### 2d. Genuine rewrite — `RockCentral::UpdateChar` (8.5%)
Completely different member offsets and control flow, larger frame:
```
BASE 0x82287908                          TU5 0x822d1900
 stwu r1, -0x100(r1)                      stwu r1, -0x140(r1)   ; frame +0x40
 lwz  r11, 0x40(r3) ; beq …               lwz  r11, 0x1c(r3) ; beq …
 lwz  r11, 0xfc(r3) ; beq …               lwz  r11, 0x28(r3) ; beq …
 (no analog)                              lwz  r11, 0x74(r3); li r22,1; stw r11,0x54(sp)…
```
This is real new logic. `RockCentral` is Harmonix's online stats/leaderboard backend —
exactly the kind of thing a title update touches (server protocol, DLC accounting).

---

## 3. Same-instrument patch relevance — VERDICT

**The patch surface survives TU5 essentially intact.** Of the patch's 7
hooked/called game functions, **6 are clean 1:1 matches (NOT rewritten)** and only
`IsActive` (Layer A) is in the rewritten 81 — and that layer is a whole-function
override, so its body divergence does not matter.

| patch role | function | base VA | in the 81? | verdict |
|---|---|---|---|---|
| Layer A detour | OvershellPartSelectProvider::IsActive | 0x8264b5f8 | **YES (56.2%)** | **holds — whole-fn override** |
| Layer B detour | OvershellPanel::ResolvePartWaitStates | 0x8259d948 | no (clean) | unaffected |
| Layer C detour | PlayerTrackConfigList::ProcessConfig | 0x8274acf8 | no (clean) | unaffected |
| centre detour | TrackWatcherImpl::RecalcGemList | 0x8276fbb0 | no (clean) | unaffected |
| GAME_FN | GameGemDB::Duplicate | 0x8276e590 | no (clean) | unaffected |
| GAME_FN | GameGemList::CopyFrom | 0x82769450 | no (clean) | unaffected |
| GAME_FN | GameGemDB::GetDiffGemList | 0x8276e010 | no (byte-exact) | unaffected |

### Layer-A detour verdict (detailed)
`same-instrument-tu5-retarget.md` already retargeted and byte-verified this; my
disassembly confirms its reasoning independently:
- **Entry is hookable and unchanged:** TU5 `0x826684c0` opens `7D8802A6 mflr r12`
  (the doc's detour overwrites exactly this word with `b 0x82C8A080`). The trampoline
  replicates the `mflr` and re-enters at entry+4.
- **Signature unchanged:** `IsActive(int) const → bool` (r3=this, r4=int, bool in r3).
  Same guard math on the same offsets in the shared head.
- **Body divergence is irrelevant by construction:** the Layer-A hook wraps
  `IsActiveOrig` and forces `true` under the flag — it never depends on the interior of
  the rewritten body. **No rework needed.** ✅

### Other overlaps flagged (neighbors, NOT hooked → no patch impact, but note)
The overshell/part-select subsystem is one of the **most-churned in TU5**: besides
`IsActive`, TU5 rewrote `OvershellPartSelectProvider::Reload` (56.5%) and `DataSymbol`
(70%), `OvershellPanel::ResolveSlotStates` (69.3%), and `OvershellSlot::UpdateView`
(27.1%) / `UpdateState` (48.4%) / `IsQuitToken` (50%). The patch does not hook any of
these, and Layer B *reimplements* its target wholesale (offsets already byte-verified
on TU5 in the retarget doc), so statically there is no conflict. **Caution (runtime,
not static):** because Harmonix reworked overshell part-select behavior in TU5, the
patch's assumptions about part-select *flow* deserve one Xenia smoke-test on the TU5
build — the hook points and struct offsets are confirmed, but the surrounding state
machine moved.

---

## 4. P5 re-decomp cost of the 81

The 81 are the *real* re-decomp delta of a TU5 rebase. Bucketed:

- **Skip entirely — SDK / middleware / CRT (~13):** all of §1's "SDK/audio middleware"
  (xWMA ×4, LEAPCORE, OAPIPELINE, XAUDIO2, OggMap ×2, __u64tod, dyn-init), plus
  NUISPEECH `GetClient`. These are Microsoft XAudio2/xWMA + Harmonix LEAP audio +
  speech SDK — replaced wholesale in the native port. The high-% ones (xWMA 95–99%) are
  reloc-only recompiles anyway. **Zero port value.**
- **Trivial re-match — ripple-flagged in-scope (~18):** `≥90%` game/engine/online
  bodies (`HandleAudioLoad`, `Player::Restart`, `main`, `DrawString`, `FreeCamera::*`,
  `DateTime::ToCode`, `SendRawData`, `SaveGlobalOptions`, `StandardStream::Init`,
  `RecordDataPoint`, `UpdateMiscellaneousSongDataForUser`, `MemcardMgr::Init`,
  `UsbMidiGuitar::Poll`, `ContentMgr` ctor, `XboxContent` dtor, `OwnedSongSortNode::
  IsEnabled`, `ConnectionStatusPanel::Exit`, `ManageBandPanel::Exit`). Existing decomp
  source re-matches after the upstream struct/vtable insertion is modeled; each is an
  offset/one-block tweak, not new logic. **~½ day total.**
- **Real re-decomp — genuine in-scope rewrites (~25):** the `<60%` and unanchored-real
  tail: `RockCentral::UpdateChar` (8.5), `GemTrainerPanel::Poll` (17.7),
  `GameMode::SetMode`/`OnSetMode`/`ctor`, `Campaign::UpdateEndGameInfo` (23.6),
  `Player::CheckCrowdFailure` (38.6), `OvershellSlot::UpdateView`/`UpdateState`/
  `IsQuitToken`, `OvershellPartSelectProvider::IsActive`/`Reload`/`DataSymbol`,
  `OvershellPanel::ResolveSlotStates`, `UIStats::MaybePublish` (46.6),
  `AccomplishmentManager::Handle(Pre)SongCompletedForUser`, `MidiParser::ParseNote`/
  `InsertDataEvent`, `MetaMusic::Start` (32.6), `SongData::Poll` (58.9),
  `MultiTempoTempoMap::AddTempoInfoPoint`, `RndShaderProgram::LoadShaderBuffer` (37.9),
  `GamePanel::PollForLoading`, plus the unanchored small bodies (`SongDB::*Phrases`,
  `GemPlayer::UpdateGameCymbalLanes`, `PrefabMgr::*`, `Singer::CreateMicClientID`,
  `Rnd`/stream `ReadImpl` trio, `MusicLibrary::RebuildProfileData`, `AsyncFile::
  ReadAsync`, `WinSockSocket::Init`). These carry actual behavioral edits and must be
  re-read against the TU5 body. **The bulk of the effort — ~1–2 focused days.**

**Bottom line:** of the 81, ~13 are skip (SDK), ~18 are trivial offset re-matches, and
~25 are genuine in-scope re-decomp — call it **~2–3 engineer-days** for the whole
rewritten delta on top of the mechanical remap of the ~12,800 unchanged functions
(which is scripted per §Phase-3 of `tu5-migration-scope.md`). The 81 rewrites are a
**small, bounded** cost; the migration is dominated by the mechanical re-anchor.

---

## 5. What TU0→TU5 actually is

1. **A textbook bug-fix/stability title update, layout-rippled.** The dominant "change"
   signal (478 changed, 397 AMBIG getters + the ≥90% half of the 81) is **not** edits —
   it is one or two upstream insertions (a data member + a virtual method into a shared
   base class) rippling through struct offsets and vtable indices. Real source edits are
   the ~25-function low-% tail.
2. **Which subsystems Harmonix touched (by rewrite density):**
   - **Overshell / part-select UI** — the single most-churned area (`OvershellSlot`,
     `OvershellPartSelectProvider`, `OvershellPanel`). Player-facing instrument/part
     selection got reworked. *(Directly relevant to us — this is the patch's turf.)*
   - **Online / stats / accomplishments** — `RockCentral::UpdateChar` (near-total
     rewrite), `AccomplishmentManager::Handle(Pre)SongCompleted`, `ProfileMgr`,
     `MusicLibrary::RebuildProfileData`. Consistent with server-protocol/DLC-accounting
     and leaderboard fixes.
   - **Song / MIDI / beatmatch** — `MidiParser::ParseNote/InsertDataEvent/PushIdle`,
     `SongData::Poll`, `SongDB::*Phrases`, `MultiTempoTempoMap`. Likely
     chart-parsing/DLC-content robustness.
   - **Content / DLC / marketplace** — `XboxContent*`, `ContentMgr`, `SongMgr::
     ContentName`. Expected for a marketplace-era update.
   - **Gameplay polish** — `GameMode::SetMode`, `Player::CheckCrowdFailure`,
     `GemTrainer*`, `Campaign::UpdateEndGameInfo`.
3. **Surprising:** the audio-middleware codecs (xWMA/XAUDIO2/LEAP) appear in the changed
   set but are 95–99% reloc-only — they were *recompiled*, not edited; Harmonix didn't
   touch the decoders. And the churn is remarkably *concentrated* — 81 real rewrites out
   of ~13,300 functions (0.6%) — confirming TU5 is a surgical patch, not a re-release.
4. **TU5 is the right decomp target.** It is what RB3Enhanced, the same-instrument
   patch, and every real player runs (per `tu5-migration-scope.md`, the architecture
   decision is a full re-base to TU5 with TU0 frozen as a tag). The rewritten-function
   cost quantified here (~2–3 days, mostly a bounded ~25-function tail, 13 skippable as
   SDK) is small and does not change that recommendation.

---

## Appendix — artifacts produced (read-only, /tmp only; nothing in-repo mutated)
- `/tmp/miss81_rec.json` — the 81 with recovered TU5 VAs + skel_match_pct.
- `/tmp/miss81_tagged.json` — same, with subsystem tags + demangled names.
- Recovery script logic: masked-prologue search via `tools/tu5_skel_recover.py`
  primitives against `band_tu5.exe` .text (documented in §0).
