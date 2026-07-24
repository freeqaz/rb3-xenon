# fpcarve BATCH-5 — honest ranked target list & channel refresh

Recon date 2026-07-24, main @703f81a9 (25,112 strict). Map 19,769 entries,
report.json current (14:21). READ-ONLY pass; artifacts under `~/tmp`.

## TL;DR — the carve channel is genuinely draining; pivot the method

The easy carve frontier (clean-pin new TUs + BinDiff engine-name transfer) is
**down to a handful**. Today's ~+180 landings consumed almost everything:
- **band3 is fully wired** (208/208 minus 2 tiny TUs). The 428-pool is drained.
- **BinDiff r2's 299 names are 298/299 already landed** into the map.
- The remaining volume lives in **already-pinned game units as anonymous `fn_`
  symbols** — a symbol-map-reveal problem, but after de-mirage only **~210 real**
  candidates remain (1,091 of ~1,300 are ICF/EH-funclet mirage). Batch-4's
  "optimistic seed list" warning is confirmed structurally.

**Recommendation:** fund **one** short clean-harvest wave (deterministic wins
below, ~50-90 strict), then **stop chasing new carve targets** and route effort to
(a) BinDiff-name→recarve engine %, and (b) genuine body-port waves on the
low-fuzzy game units — not the carve channel. Details in §5.

---

## 1. BinDiff r3 (re-anchored on grown map) — DELIVERED, low yield as predicted

Re-ran the anchored pass with the current map (14,045 unique-both anchor pairs vs
r2's 13,921; 13,837 applied). Ground-truth precision held at **96.0%** (conf≥0.95 &
sim≥0.70). New high-confidence names over the still-anonymous pool:

| tier | count | disposition |
|---|---:|---|
| A_engine_shared (map-insertable) | **15** | insert directly, spot-verify cluster |
| B_char_oracle (verify-first) | 12 | strong hint, confirm vs rb3-Wii |
| C_hamgame_oracle | 1 | verify |
| **total new** | **28** | (27 truly new vs r2) |

Zero NATIVE-SOON/game names — **all engine/char**. This re-confirms the r2
conclusion: **DC3 BinDiff is an engine namer and it is now saturated** (299 → +28).
Not worth re-running again until the map grows by thousands.
Artifact: `~/tmp/bindiff_r3_highconf.json` (each carries full `dc3_mangled`).
Sample engine names: `BufStream::ReadImpl`, `FloatKeys::Load`,
`FlowCommand::ClassName`, `ObjVector<FlowMathOp>::operator=`,
`ST::ExtractBodyPart`. **COST CLASS: map-insert (cheapest). Expected +~15 strict**
(engine tier; char tier verify-first, uncertain).

---

## 2. Unwired-with-bodies census — band3 done, engine residue mostly DC3-only

Full census `~/tmp/unwired_census.json`.

- **band3: 2 unwired** (both tiny, rb3-Wii oracle present):
  - `band3/game/ScoreTracker.cpp` (35 lines, 6 methods) — known.
  - `band3/meta_band/BandStoreUIPanel.cpp` (28 lines, 1 handler + BEGIN_HANDLERS).
  - COST CLASS **wire+home**, but tiny (each ≤6 fns). Need to locate their retail
    COMDATs first (0 mapped VAs each) — a homing step. Expected +2..6 each.
- **network: 0 unwired.**
- **system: 115 unwired**, but the majority have **no retail COMDAT** and are dead
  weight in the tree:
  - `gesture/*` (14) + most `hamobj/*` (21: DancerSkeleton, FreestyleMove, HamSong,
    MoveGraph, RhythmBattle…) = **Dance-Central-only**, absent from RB3 retail.
  - `synth_xbox/{3dnow_win,mmx_optimized,sse_optimized,cpu_detect_x86_*}` = **x86
    SoundTouch**, never in a PPC binary.
  - Genuinely-RB3 engine leftovers worth homing: `world/BeatClock.cpp`,
    `os/PlatformMgr_Xbox.cpp`, `char/CharClipDisplay.cpp`, `synth/*` audio,
    `utl/{Crc,JsonEncode,UrlEncode,EncryptXTEA}.cpp`, `rndobj/{Enter,PostProcMgr}.cpp`.
    All **engine (lower priority per user directive)** and need homing (span not
    mapped). COST CLASS **wire+home**, small.

---

## 3. Wired-but-unpinned census — the clean-pin lane (36 TUs, mostly needs-ID)

`~/tmp/wired_unpinned.json`. These compile today but have **no `splits.txt` pin**
→ invisible to objdiff. Correct match keys required both full-path and basename
(splits.txt mixes them). 36 total: 7 game, 2 network, 27 engine (many engine are
DC3-only hamobj/gesture with no retail COMDAT → correctly unpinnable).

**The gate: can we locate the span?** Probed the map for each class's functions:

| TU | cat | mapped VAs | cost class |
|---|---|---:|---|
| **MemcardMgr** (meta) | eng | **14** @827abbc8..827acb80 | **CLEAN-CARVE** — pin the span, gen map, done |
| DirectInstrument | game | 1 | needs-ID then carve |
| TexProc / OggMap / UsbMidiGuitar | eng | 1 each | needs-ID |
| DrumMap, GameGemDB, PhraseList, TrackType (beatmatch **core**) | eng | **0** | needs-ID (correlator/oracle) then clean-carve — genuinely RB3 |
| VocalGuidePitch, QuestJournal, 2×AccomplishmentConditional | game | 0 | needs-ID |
| FontBase, MidiVarLen, FlowSlider, WebSvcReq, UserGuid, Easing, DoubleExpSmoother | eng | 0 | needs-ID |
| Main.cpp | game | (catch-all mass) | not a single-TU carve |

Only **MemcardMgr is a true clean-pin** (span already fully named). The rest are
"needs-identification first" — real but medium-cost (run the tu5 reloc correlator
or oracle span-finding before a pin can register). The **beatmatch four**
(DrumMap/GameGemDB/PhraseList/TrackType) are the highest-value of these: RB3-core
note-track code, definitely present in retail, just not yet located.

---

## 4. fp2 unpinned-run census — thin after noise/region filter

`~/tmp/fp2_runs.json`. Of 1,365 maximal unpinned runs, after dropping the
`App.cpp`/`BandOffline.cpp` catch-all FPs and `.permuter_work_*` scratch, only **21
named runs** survive and **14 are in-scope** (7 fell in the **Quazal HARD-SKIP**
range 0x82A4–0x82B4: ConnectionInfoDDL, Platform, InetAddress, SystemComponent,
DynamicGathering — all excluded per directive). In-scope real ones:

| run | ch | span | fns | note (strings confirm reality) |
|---|---|---|---:|---|
| **SessionMessages.h** | game | 82650b78 (58fn) | 58 | online: h2h/ranked/matchmaker — real; **header-attributed**, owner=`network/net/SessionMessages.cpp` (wired) → likely inline-handler code; verify owner |
| **UsbMidiGuitarMsgs.h** | game | 8252e2d4 (26fn) | 26 | Pro Guitar MIDI (rg_fret/pitch_bend) — real; owner `UsbMidiGuitar.cpp` is **wired-but-unpinned** (see §3) |
| **MidiSectionLister.h** | game | 826cfca0 (30fn) | 30 | trainer/overdrive tracker — real; find owner .cpp |
| BeatClock.cpp | eng | 82748bc8 (16fn) | 16 | real (beat/mbt/seconds); wire+home |
| PlatformMgr_Xbox.cpp | eng | 827b1fc8 (45fn) | 45 | real (xbox store/purchase); wire+home |
| ChordShapeGenerator.cpp | eng | 822dd290 (6fn) | 6 | real (chord/mesh); small |
| Challenges/JointUtl/DataMinerJobs | eng | — | 6-13 | weak attribution, low conf |

The MIDI cluster (UsbMidiGuitarMsgs/MidiSectionLister) + SessionMessages are the
only **game**-channel finds, and all are **header-attributed** (owner .cpp is wired
but the .text run is inline code) → COST CLASS **header-port / owner-span-pin**,
not clean-carve.

---

## 5. Body-match / reveal reservoir on pinned game units — big raw, small real

The real volume is anonymous `fn_XXXX` symbols inside already-pinned game units at
high fuzzy (unmatched only because unnamed): raw **1,170 @≥99% + 121 @90-99%**.
**But de-mirage kills most of it:** 1,091 are clustered identical-size ICF/EH-funclet
echoes (e.g. NextSongPanel's 240 are ALL size-40 → mirage). **Only ~210 are real**
(varied-size), spread ≤10 per unit:

| unit | real reveal cands | | unit | real |
|---|---:|---|---|---:|
| MetaPerformer | 10 | | RockCentral | 7 |
| LessonMgr | 9 | | MetaPanel | 7 |
| TrainerPanel | 8 | | InterstitialMgr/NewAwardPanel/SaveLoadManager | 6 |
| CharData/SongStatusMgr/GuitarFx/CharacterCreatorPanel | 5 | | (long tail of 4s) | 4 |

COST CLASS **reveal (size_order_automap) + near-miss body-match**. The
size_order_automap tool (batch-4 CampaignGoalsLeaderboardPanel +34 @100% precision)
still applies to units where the fn_ are varied-size and un-automapped, but the
per-unit yield is now single-digits. The low-fuzzy pool (84 game units <60% fuzzy:
SaveLoadManager 375-unmatched@25%, RockCentral 366@50%, VocalPlayer 179@44%,
UIStats 163@13%) is **genuine body-divergence / layout** = body-port waves, not carve.

---

## 6. RECOMMENDATION — what the next 2 waves should fund

**Wave A (clean deterministic harvest, ~1 session, honest +50..90 strict):**
1. Insert the **15 engine BinDiff-r3 names** into the map (map-insert; +~15). Hold
   the 12 char names for a rb3-Wii spot-check.
2. **Pin MemcardMgr** (span 827abbc8..827acb80, 14 named VAs) — the one true
   clean-carve (+~10).
3. **Wire+home the 2 tiny band3 TUs** (ScoreTracker, BandStoreUIPanel) — small but
   game-priority (+~5).
4. **size_order_automap the ~6 richest real-reveal units** (MetaPerformer,
   LessonMgr, TrainerPanel, RockCentral, MetaPanel, SaveLoadManager) — de-miraged,
   ~single-digit each (+~30). Skip NextSongPanel/OvershellSlot (pure mirage).

**Wave B (identification-then-carve, higher value, medium cost):**
5. **Locate + pin the beatmatch-core four** (DrumMap, GameGemDB, PhraseList,
   TrackType) via the tu5 reloc correlator / rb3-Wii oracle span-finding. RB3-core,
   definitely present, currently 0 mapped VAs. This is the best *new-TU* value left.
6. **Resolve the MIDI/session header owners** (UsbMidiGuitar.cpp unpinned span;
   MidiSectionLister/SessionMessages owner .cpp) and pin — game-priority Pro-Guitar
   + online code.

**Do NOT fund:** another BinDiff re-anchor (saturated), the 1,091-mirage reveal
pool, the DC3-only unwired system TUs (gesture/hamobj/x86 soundtouch — no retail
COMDAT), or anything in the Quazal/vendor HARD-SKIP ranges.

**Beyond wave B the carve channel is effectively closed** — remaining match% is in
body-port territory (the low-fuzzy game units), which is the body-port batch
skills' job, not fpcarve.

### Artifacts
- `~/tmp/batch5_ranked.md` (this file)
- `~/tmp/bindiff_r3_highconf.json` — 28 r3 names (15 insertable)
- `~/tmp/wired_unpinned.json` — 36 wired-but-unpinned TUs
- `~/tmp/unwired_census.json` — full unwired census
- `~/tmp/fp2_runs.json` — unpinned-run census
- `~/tmp/bindiff_spike/` — r3 anchored exports + `.BinDiff` (reusable)
