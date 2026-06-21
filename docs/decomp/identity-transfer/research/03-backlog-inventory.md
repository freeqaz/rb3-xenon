# Identity-Transfer Backlog Inventory + EV

**Lane:** backlog-inventory. **Date:** 2026-06-21. **Read-only analysis.**

Companion to `01-tooling-audit.md` (the *tool* spec). This doc enumerates the
**ICF-scattered game-TU backlog**, classifies each TU's methods case-A / case-B,
estimates real-bodied yield, checks rb3-Wii source availability, and ranks by EV.

All numbers derived from:
- Oracle: `unified_id_rb3wii.json` (9,301 entries, 703 distinct `bindiff_src` TUs;
  schema: `rb3_addr, rb3_fn, wii_name, bindiff_src, similarity, confidence, size`).
- Pins: `config/45410914/splits.txt` (732 `.text` spans, 615 TU headers).
- Wired set: `config/45410914/objects.json` (677 wired basenames).
- The tool's own gate constants: `tools/identity_transfer.py:107 SPAN_PIN_MIN = 0x800`,
  case discriminator `:334-405`, HARD GATE `:526-575`.

---

## How "eligible" was computed (matches the tool's gate exactly)

A method is **case-A** if its `rb3_addr` falls in NO pinned `.text` span (an
unowned `auto_*` blob — micro-pinnable with stock tools), **case-B** if it falls
INSIDE a *foreign* TU's pinned span (needs the objdiff fork), **owned/SELF** if it
falls inside its own TU's pin.

A TU is **ELIGIBLE for identity-transfer** only if it has **no own span pin ≥ 0x800
bytes** — this is the tool's HARD GATE (`identity_transfer.py:354-357,526-529`,
the wave-16 −14 collision root: appending micro-pins to a span-pinned TU steals
pairing). 52 game TUs are already span-pinned and thus **excluded** (they count
via their span). 21 game TUs are *micro-only* pinned (identity-transfer already
applied — e.g. `RockCentral.cpp`, `VoiceoverPanel.cpp`, several `Accomplishment*`).

**Methodology note / pitfall fixed:** splits.txt headers use **full paths**
(`network/ObjDup/DuplicatedObject.cpp:`), not basenames. Keying the span-owner map
by full path silently marks every multi-dir TU as un-pinned and inflates the
inventory. The tool normalizes via `tu_base()` (`:116`); this inventory keys by
**basename** to agree with it. (A first pass that didn't do this falsely listed
DuplicatedObject/GemTrack/VocalPlayer/AccomplishmentManager as eligible — they are
all in fact span-pinned and the tool correctly refuses them.)

**`realA` = case-A methods with oracle `size > 44B`** — the not-an-ICF-stub proxy.
The ≤44B class is the `??_E` dtor-thunk / getter / `_Vector_base` template fold that
byte-matches across unrelated TUs (the wave-14 +57 refutation). `realA` is the EV
driver; raw `caseA` is the noise-inflated count.

---

## Calibration: RockCentral (the one proven win) anchors the yield

Reconstructing RockCentral's **pre-transfer** state (owner map with RockCentral's
own micro-pins removed):

| metric | value |
|---|---|
| raw case-A | 108 |
| **realA (>44B)** | **20** |
| **landed** | **+17** |
| **realA→match yield** | **0.85** |

RockCentral realA size distribution: `[48,48,56,56,56,56,64,68,68,72,72,72,80,84,84,88,104,120,180]` —
mostly small-but-real bodies. **0.85 is the BEST CASE** (its source was already a
clean port). The **worst case is wave-16 BandProfile: 0/64 reached 100%** after a
full MWCC→MSVC port — ported bodies *diverged from retail* (the gating risk; see
`objdiff-caseb-fork-banked.md:50-53`). So I model a **blended 0.4–0.6 expected
yield** for un-ported TUs, reserving 0.85 only for TUs whose source ports cleanly.

---

## The eligible inventory (case-A, the cheap path)

**Top eligible game TUs, ranked by `realA`** (`-` src = no rb3-Wii/dc3 source found):

| TU | total | realA | caseA | caseB | src | lines |
|---|---:|---:|---:|---:|---|---:|
| GemPlayer.cpp | 169 | 29 | 101 | 68 | wii | 2891 |
| BandProfile.cpp | 104 | 15 | 73 | 31 | wii | 1013 |
| TrackPanel.cpp | 67 | 15 | 49 | 18 | wii | 862 |
| MetaPerformer.cpp | 70 | 15 | 56 | 14 | wii | 1824 |
| DuplicationSpace.cpp | 40 | 13 | 23 | 17 | **NONE** | - |
| Game.cpp | 103 | 12 | 76 | 27 | wii | 1786 |
| AccomplishmentCategoryPanel.cpp | 28 | 11 | 25 | 3 | **NONE** | - |
| PRUDPEndPoint.cpp | 29 | 11 | 26 | 3 | **NONE** | - |
| ObjDupProtocol.cpp | 36 | 11 | 23 | 13 | **NONE** | - |
| MainHubPanel.cpp | 42 | 11 | 39 | 3 | wii | 590 |
| NetSession.cpp | 111 | 10 | 77 | 34 | wii | 1090 |
| OvershellPanel.cpp | 77 | 10 | 44 | 33 | wii | 1667 |
| Station.cpp | 51 | 10 | 39 | 12 | **NONE** | - |
| PRUDPStream.cpp | 31 | 9 | 18 | 13 | **NONE** | - |
| CharacterCreatorPanel.cpp | 39 | 8 | 23 | 16 | wii | 883 |
| PatchPanel.cpp | 38 | 8 | 27 | 11 | wii | 724 |
| SongSortNode.cpp | 53 | 7 | 30 | 23 | wii | 454 |
| VocalPart.cpp | 47 | 7 | 41 | 6 | wii | 1019 |
| GemManager.cpp | 99 | 7 | 48 | 51 | wii | 1669 |
| AccomplishmentPanel.cpp | 37 | 7 | 19 | 18 | wii | 1606 |
| TourPerformerLocal.cpp | 35 | 7 | 26 | 9 | wii | 534 |
| SessionMgr.cpp | 34 | 7 | 26 | 8 | wii | 524 |
| TourProgress.cpp | 48 | 6 | 35 | 13 | wii | 508 |
| BudgetScreen.cpp | 56 | 6 | 37 | 19 | wii | 611 |
| ChordbookPanel.cpp | 41 | 6 | 26 | 15 | wii | 541 |
| BandSongMetadata.cpp | 60 | 6 | 34 | 26 | wii | 511 |
| PerfectSectionTracker.cpp | 18 | 6 | 15 | 3 | wii | 395 |

(`VoiceChannelDDL.cpp` reports realA=7 but its rb3-Wii file is a 5-line forward — a
mis-attribution; do NOT count it. `SaveLoadManager.cpp` src is 2267L, low realA=6 —
poor ratio.)

### Aggregate totals (eligible = game, not span-pinned, realA>0)

| population | TUs | realA | caseA(raw) | caseB(raw) |
|---|---:|---:|---:|---:|
| **all eligible** | 375 | **975** | 4092 | 1964 |
| eligible **with rb3-Wii/dc3 src** | 220 | **590** | — | — |
| eligible **NO source** (mostly Quazal/PRUDP/Station/DDL net stack) | 155 | 385 | — | — |
| already-wired but unpinned (no port, just wire+transfer) | 23 | 34 | — | — |

The **155 NO-SRC TUs (realA=385)** are dominated by the **Quazal/RakNet network
stack** (`PRUDPEndPoint`, `PRUDPStream`, `Station`, `StationURL`, `ObjDupProtocol`,
`*DDL`, `Job*`, `Session*`) — third-party middleware rb3-Wii either named
differently or didn't decompile. These are **effectively unreachable** without
fresh source archaeology; treat realA=385 as a hard discount off the ceiling.

### Cheap near-term subset (rb3-Wii src < 700 lines, realA ≥ 4)

28 TUs, **realA = 146**. These are the lowest-port-cost targets:

```
MainHubPanel 11/590L   TourPerformerLocal 7/534L   SessionMgr 7/524L
SongSortNode 7/454L    TourProgress 6/508L         BudgetScreen 6/611L
ChordbookPanel 6/541L  BandSongMetadata 6/511L     PerfectSectionTracker 6/395L
SessionMessages 5/284L GameConfig 5/435L           MetaPanel 5/564L
RGTrainerPanel 5/636L  Scoring 5/366L              ChordPreview 4/88L
PassiveMessenger 4/465L PracticePanel 4/478L       Matchmaker 4/428L
VocalTrainerPanel 4/335L  (+ ~9 more realA=4)
```

### Mid tier (700–1500L, realA ≥ 6): 6 TUs, realA = 63

`BandProfile 15/1013L · TrackPanel 15/862L · NetSession 10/1090L ·
CharacterCreatorPanel 8/883L · PatchPanel 8/724L · VocalPart 7/1019L`
⚠ BandProfile already PORTED in wave-16 → **0/64 at 100%** (branch
`w16-bandprofile @ec65595`); its 15 realA is the *theoretical* count, not achieved.
Treat BandProfile as the **divergence-risk poster child**, not a quick win.

---

## EV model (honest)

Applying the yield band to `realA` (a method counts only if it byte-matches its own
real body — `identity-transfer.md:68-74`):

| subset | realA | EV @0.85 (best) | EV @0.5 (blended) | EV @0.4 (conservative) |
|---|---:|---:|---:|---:|
| cheap (28 TU, <700L) | 146 | 124 | **73** | 58 |
| cheap + mid (34 TU) | 209 | 178 | **104** | 84 |
| all with-src (220 TU) | 590 | 502 | **295** | 236 |
| grand total (375 TU) | 975 | 829 | 488 | 390 |

**Honest case-A ceiling ≈ +236–295** (all 220 with-src eligible TUs, blended
yield), of which the **cheap near-term subset is ≈ +58–73 (28 TUs, all <700L
rb3-Wii source)**. The grand-total +390–829 figures are *not* realistic — they
fold in the 155 NO-SRC network TUs.

### Case-B (gated on the objdiff fork)

Eligible game TUs carry **realB (>44B) = 432** case-B methods (raw caseB = 2357,
across 224 TUs). At the same yield band the **case-B ceiling ≈ +216–367** —
consistent with the fork doc's claimed "+150–220". But case-B requires BOTH (a) the
objdiff `caseb-global-byteeq` fork integrated AND (b) the same source-port-then-
byte-exact gate as case-A. **Recommendation: do not build case-B plumbing yet** —
it's strictly downstream of proving case-A yields real wins on freshly-ported TUs.

---

## Ranked recommendation (EV-per-port-hour)

1. **Cheap case-A subset first** (28 TUs, <700L, realA=146 → EV ≈ +58–73). Start
   with the smallest/highest-realA: **MainHubPanel (11/590L)**, **SongSortNode
   (7/454L)**, **PerfectSectionTracker (6/395L)**, **Scoring (5/366L)**,
   **ChordPreview (4/88L)**. These are the canonical scattered TUs the doc already
   names — pure mechanism validation at low port cost.
2. Per TU: port rb3-Wii MWCC→MSVC source → wire `objects.json` NonMatching →
   `identity_transfer.py --tu X --apply` → overlap self-check → **per-unit A/B +
   `tools/icf_alias_check.py`** (the ≤44B-stub honesty gate) → composed verify.
3. **Skip** the 155 NO-SRC network TUs (realA=385 unreachable without new source)
   and **defer** BandProfile/Game/GemPlayer (large + body-divergence risk —
   GemPlayer 2891L, Game 1786L; high realA but the wave-16 lesson applies).
4. Defer **all case-B** (+216–367 fork ceiling) until ≥3 case-A ports land real
   wins — only then is the objdiff fork integration worth its do-no-harm validation.

---

## GAPS / what to build

1. **A `realA`-vs-actually-matched calibration harness.** Today the only data point
   is RockCentral (0.85). The tool's "truthful EV" (`identity_transfer.py:185-247`)
   returns 0 for unwired TUs (no compiled obj → no nameable symbols → no estimate),
   so it cannot pre-rank the backlog — every eligible TU above reads EV=0 from the
   tool. **Build:** after each port, record `realA_predicted` vs `landed` into a
   small CSV so the yield band tightens from N=1 to N≥5. Without this the +236–295
   ceiling is a single-sample extrapolation.

2. **A source-divergence pre-screen.** The dominant risk is *ported body ≠ retail
   body* (BandProfile 0/64). Nothing today predicts this before a multi-hour port.
   **Build:** a cheap per-method screen that, for a candidate TU's realA methods,
   compares the rb3-Wii oracle `similarity` (already in the JSON) against a
   threshold — sort the cheap subset by *mean realA `similarity`*, port the highest
   first. (RockCentral's realA were high-sim; that's likely why it ported cleanly.)

3. **A NO-SRC resolver for the network stack.** 155 TUs / realA=385 are blocked
   purely on missing rb3-Wii source (Quazal/RakNet/PRUDP/Station/DDL/Job). **Build/
   investigate:** whether dc3-decomp, the rb3-Wii *retail* tree, or a Quazal SDK
   gives these names. Until then, exclude them from every ceiling figure.

4. **`realA` size source is the oracle, not the binary.** `size` comes from BinDiff,
   not `symbols.txt`; the >44B cut is a proxy. **Build:** cross-check realA against
   `symbols.txt` sizes for the cheap subset before trusting the EV (the tool already
   loads `symbols.txt` sizes at `:360` — reuse that path to validate the proxy).

5. **Case-B realB is computed but unverified.** The 432 realB / +216–367 figure has
   never been measured end-to-end (the fork rejected all 4 demo promotions as un-
   oracle'd STL folds). Do not bank it; it is a *ceiling*, gated on (2) and (3)
   above plus fork integration (`objdiff-caseb-fork-banked.md:59-67` checklist).
