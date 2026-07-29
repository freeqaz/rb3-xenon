# laneBD — locating the 141 Wii-oracle TUs with no 360 position (2026-07-29)

Mission, from laneBB's `docs/plans/rb3-360-vs-wii-coverage-2026-07-29.md` §9: the
actionable frontier is **141 Wii TUs / 2,505 Wii fns / 0.722 MB that have a full
rb3-Wii oracle and whose 360 `.text` position has never been located**. Find them,
pin the best, wire them.

Constraints from laneBA's `docs/plans/attribution-frontier-census-2026-07-29.md`
are respected throughout: never map-name an unpinned auto VA; never glob
`build/45410914/{obj,asm}` for evidence (4,618 stale `auto_03_*` artifacts, 2,439
pre-TU5); never use `unified_id*.json` / `ghidriff_identities.json` (TU0-keyed, VA-dead).

---

## 0. TL;DR

**The 141 TUs are not missing. They are mis-attributed.** Their 360 `.text` is
already inside `splits.txt` pins that belong to *neighbouring* TUs, and it is a
principal cause of the sub-25 % match rate on several large units.

Three things were established:

1. **The premise "we have not located them" is now false for 58 of them.** Two
   independently-calibrated instruments (RTTI class-owned-vtable-slot + ctor-site
   chase; string-literal cross-reference) produce concrete 360 `.text` spans, with
   positive-control precisions of **0.775 / 0.836** and **0.95** against chance
   baselines of **0.0013** and **~0.0035**. §3, §4.
2. **There is nowhere to put them that is empty.** The game/engine `.text` zone
   `0x82270000–0x8282A000` (6.00 MB) is **93.2 % claimed**; only 407 KB is
   unclaimed and it is shattered into 2,173 fragments whose largest is 10 KB.
   Locating a TU therefore *never* means finding free space — it means finding the
   over-broad pin that swallowed it. §2.
3. **The census figure needs two corrections and one large caveat.** At least
   **12 TUs / 109 fns are outright false positives** (the whole `system/speex`
   codec, plus `bufstreamnand` and `rso_utl`, which are Wii-platform TUs the
   `WII_PLATFORM` regex does not spell). A further 39 TUs / 376 fns have **no
   evidence in either direction** — mostly non-polymorphic `system/math`,
   `system/utl` helpers where absence of RTTI proves nothing. §5.

**Two instruments were refuted and should not be re-funded:** Wii→360 link-order
interpolation, and directory-locality. §6.

---

## 1. The worklist, derived

`scripts/harvest/oracle_coverage_matrix.py --reverse` (laneBB, commit `9886b66b`),
`ABSENT` rows, non-`network`, minus the `WII_PLATFORM` regex — reproduces
**exactly 141 TUs / 2,505 Wii fns / 721,568 B**. By directory (Wii fns):

| dir | fns | | dir | fns |
|---|--:|---|---|--:|
| `band3/meta_band` | 707 | | `system/utl` | 78 |
| `system/bandobj` | 490 | | `system/speex` | 72 |
| `band3/game` | 339 | | `system/track` | 53 |
| `band3/tour` | 228 | | `system/math` | 44 |
| `system/meta` | 142 | | `band3/net_band` | 40 |
| `system/beatmatch` | 129 | | `system/char` | 34 |
| `system/ui` | 80 | | `system/os`, `bandtrack`, `dsp`, `rndobj`, `synth` | 59 |

**125 of the 141 have a real `.cpp` in `../rb3/src`** (the other 16 are `speex`
third-party plus 6 headers-only). So the oracle is genuinely in hand; only the
360 position was missing.

Full ranked list: `~/tmp/laneBD/real_unlocated.json` (regenerable, not committed).

---

## 2. ★ There is no free space — the "unlocated" framing is wrong

Derived from `config/45410914/splits.txt` (authoritative geometry, **not**
`report.json` auto-unit boundaries — laneBA's retracted trap):

| | |
|---|--:|
| `.text` total | `0x82270000–0x82C4CE3C`, 10.342 MB |
| pinned `.text` ranges | 5,813 |
| unclaimed, whole binary | 4.246 MB / 2,350 gaps |
| **unclaimed in the game/engine zone `0x82270000–0x8282A000`** | **407,364 B (6.8 %) / 2,173 gaps** |
| largest game/engine gap | 10,048 B (`Server.cpp` → `ContextChecker.cpp`) |
| game/engine gaps ≥ 1 KB | 109, totalling 236 KB |

Cross-check from the other side: 47,878 resolved functions below `0x8282A000`
account for 5,497,176 B = **91.5 %** of the zone; the remaining ~1.7 % is
inter-function padding. The two measurements agree.

The 90 evidenced-present unlocated TUs are **526 KB of Wii code**. That does not
fit in 407 KB of shattered holes, and the largest hole (10 KB) is smaller than
several single TUs. **Conclusion: their 360 code must already lie inside pins
owned by other units** — which is exactly what §4 then measures directly.

*(Practical consequence for the pipeline: the CLAUDE.md splits-bootstrap recipe's
step 3 — "add a new `.text` range" — is the wrong primitive here. The right
primitive is `scripts/harvest/splits_move.py`: carve the foreign span **out** of
the over-broad pin.)*

---

## 3. Instruments that work — and their calibration

### 3.1 RTTI → COL → vtable, filtered to class-owned slots (primary)

RB3-360 ships `/GR`. 1,415 type descriptors → 2,220 `??_R4` Complete Object
Locators → **1,354 classes with a fully resolved vtable**.

laneBB tried this for the RBN classes and concluded the instrument "measures the
wrong thing" because a derived class's vtable is dominated by inherited base
slots. **That is fixed, and the fix is the whole lever:** a slot is *class-owned*
iff `Derived.vt[i] != PrimaryBase.vt[i]` (or `i >= len(base.vt)`), with the
primary base read from the RTTI base-class array — computed **in-binary**, so it
needs no header parsing and absorbs intermediate-class overrides automatically.
A second, independent channel finds each class's ctor/dtor by scanning `.text`
for the `lis`/`addi` materialisation of `??_7<Class>@@6B@`.

Positive control — 298 classes whose own `<Class>.cpp` *is* pinned; precision =
fraction of evidence functions whose `splits.txt` owner is that unit:

| filter | mean | median |
|---|--:|--:|
| ALL vtable slots (laneBB's method) | 29.8 % | 20.7 % |
| **class-owned slots only** | **67.5 %** | **82.8 %** |
| **vtable-materialisation (ctor/dtor) sites** | **80.2 %** | **100.0 %** |

Restricted to the 193 classes where the filter is informative (has a base *and*
owns ≤ 60 % of slots): 21.3 % → **77.5 %** (median 91.7 %); ctor sites **83.6 %**
(median 100 %). Top-cluster majority owner == true unit in **92.2 %** of cases.
**≈3.6× lift over all-slots — laneBB's objection is answered, not restated.**

Negative controls: label permutation (score evidence against a *random* pinned
unit) gives mean **0.13 %**, n = 329. And the instrument declines rather than
inventing: `DrumMap` has 0 owned slots and 0 ctor sites and emits nothing; 148
more classes fall back to ctor sites only.

Artifacts: `~/tmp/laneBD-vt/` (`rtti_scan.py`, `hier.py`, `constidx.py`,
`spans.py`, `vtables.json`, `final.json`, `final.txt`).

### 3.2 String-literal cross-reference (independent corroboration)

A hand-rolled PPC decoder walks all 2.58 M `.text` instructions tracking `lis`
high-halves through `addi`/`ori`/loads/stores with clobber and `bl`-volatile
invalidation (0.7 s, no capstone): **20,141 code→string edges over 12,693 distinct
strings**. Functions are attributed via the real `.pdata` table (PE data-directory
entry 3 at `0x1F1600`, 461,864 B = 57,733 × 8-byte `RUNTIME_FUNCTION`,
`function_length = ((word1 >> 8) & 0x3FFFFF) * 4`) — independent of our decomp.

Positive control: 20 already-pinned units selected by the *same* ≥3-selective-
literal gate as the candidates, no peeking. Cluster centroid inside one of that
unit's individual pin ranges: **19/20 = 0.95, median absolute error 0**. Those
units' pins average 0.35 % of `.text`, so chance expectation is **0.07 of 20** —
the result is ~270× chance. Stable across cluster-gap settings (0.95/0.90/0.95 at
0x400/0x1000/0x2000).

Negative controls: Wii-only trees `rndwii` (111 literals) → top corroboration 2;
`synthwii` → 1; `usbwii` → 0. 300 synthetic random 9-char words → **0 hits, 0
clusters**. Character-shuffled `BandWardrobe` literals → 1. The ≥3 gate separates
signal from noise cleanly.

Artifacts: `~/tmp/laneBD-str/` (`xref.py`, `xref.json`, `final.json`),
`~/tmp/laneBD/xref_edges.json`.

### 3.3 ★ The two instruments agree where they overlap

They were built by different agents from different signals and were not shown each
other's output. Where both fire they name the same claiming unit: `UnisonIcon` ⊂
`MoveMgr.cpp`, `TourDesc` ⊂ `DataFunc.cpp`, `UIProxy` ⊂ `SongDifficultyDisplay.cpp`,
`PatchRenderer` ⊂ `BandSwatch.cpp`, `CharProvider` ⊂ `PropKeys.cpp`,
`CrowdRating` + `RealGuitarGemPlayer` ⊂ `VocalPlayer.cpp`, `BandPerformer` ⊂
`FlowEventListener.cpp`, `CampaignCareerLeaderboardPanel` ⊂
`CampaignGoalsLeaderboardPanel.cpp`, `PracticeSectionProvider` ⊂ the
`GemTrack`/`Tracker`/`PlayerTrackConfigList` cluster.

A worked example (`CrowdRating`, the single strongest string cluster, corr = 9),
decoded straight out of `.text` at `0x826EE598–0x826EE8B4` — one contiguous 0 %
function inside `VocalPlayer.cpp`'s pin, reading exactly the Wii
`CrowdRating.cpp` property set in order:

```
0x826EE648 -> 'lose_level'      0x826EE7A0 -> 'bad_level'
0x826EE6F4 -> 'note_weight'     0x826EE7D8 -> 'warning_level'
0x826EE72C -> 'great_level'     0x826EE810 -> 'free_ride'
0x826EE768 -> 'okay_level'      0x826EE848 -> 'phrase_weight'
                                0x826EE87C -> 'initial_display_level'
```

---

## 4. ★★ The result: 58 located TUs, and the seam they expose

Columns: `wf` = Wii fns, `ev` = evidence functions (owned slots + ctor sites),
`uncl` = % of the span not claimed by any pin, `str` = Wii literals referenced
from the span / literals testable. Full table: `~/tmp/laneBD-vt/final.txt`.

### 4a. Spans that are entirely inside ONE foreign pin (the seam)

These are decisive: the string channel independently confirms the code is that
class's, yet the range is claimed by an unrelated unit — and the claiming units
have conspicuously depressed match rates.

| TU (wf) | 360 span | size | str | claimed by (unit fn-match %) |
|---|---|--:|---|---|
| `TrainingMgr` (21) | `82565140..82565D74` | 0xC34 | **23/23** | `band3/meta_band/UIStats.cpp` (**22.9 %**) |
| `UnisonIcon` (66) | `822D2D58..822D3530` | 0x7D8 | **14/14** | `MoveMgr.cpp` (**25.3 %**) |
| `TourDesc` (43) | `823684E8..82368F88` | 0xAA0 | **12/12** | `system/obj/DataFunc.cpp` |
| `TourPerformerLocal` (26) | `823663B8..82367CF4` | 0x193C | **10/10** | `system/obj/DataFunc.cpp` |
| `UIProxy` (47) | `828233A0..8282457C` | 0x11DC | 8/11 | `system/hamobj/SongDifficultyDisplay.cpp` (**14.0 %**) |
| `TourProperty` (9) | `82368FC8..82369348` | 0x380 | 6/6 | `system/obj/DataFunc.cpp` |
| `CharProvider` (35) | `826668E8..82667B3C` | 0x1254 | 5/5 | `system/rndobj/PropKeys.cpp` |
| `BandPerformer` (34) | `826ED658..826EDFE4` | 0x98C | 5/5 | `system/flow/FlowEventListener.cpp` |
| `CharKeyHandMidi` (82) | `822D0EF8..822D2908` | 0x1A10 | 4/6 | `MoveMgr.cpp` |
| `CampaignCareerLeaderboardPanel` (33) | `825F11F8..825F2000` | 0xE08 | 13/16 | `CampaignGoalsLeaderboardPanel.cpp` |
| `CharSync` (57) | `82564410..82564FC4` | 0xBB4 | 2/2 | `band3/meta_band/UIStats.cpp` |
| `PatchRenderer` (46) | `822AE298..822AE63C` | 0x3A4 | 2/2 | `BandSwatch.cpp` |
| `TourWeightManager` (6) | `823693E8..8236955C` | 0x174 | 1/1 | `system/obj/DataFunc.cpp` |
| `BandButton` (55) | `82343950..82344B64` | 0x1214 | 1/4 | `BandHighlight.cpp` |
| `AppScoreDisplay` (37) | `825720E8..82572208` | 0x120 | – | `MetaPanel.cpp` |
| `InterstitialPanel` (32) | `8261EFC0..8261F208` | 0x248 | – | `EventDialogPanel.cpp` |
| `GameTimePanel` (14) | `8261EA48..8261EF4C` | 0x504 | 0/3 | `EventDialogPanel.cpp` |
| `UIGridProvider` (27) | `82817260..82817A64` | 0x804 | – | `UIPicture.cpp` |
| `CrowdRating` (19) | `826EE970..826EEB04` (+ `826EE598..826EE8B4`) | | 9 sel. strings | `VocalPlayer.cpp` |
| `RealGuitarGemPlayer` (40) | `826EBE10..826EC7F0` | 0x9E0 | 3/3 | `VocalPlayer.cpp` |
| `ParentalControlPanel` (11) | `8262EE18..8262F2E4` | 0x4CC | 1/4 | `PatchSelectPanel.cpp` |

Four further clusters where one over-broad pin swallowed **several** unlocated TUs:
`DataFunc.cpp`'s `82366274..8236955C` (0x32E8) holds the entire `band3/tour`
`TourDesc`/`TourPerformerLocal`/`TourProperty`/`TourWeightManager` group;
`Leaderboard.cpp`'s `8266C464..8266F730` holds `EyebrowsProvider`,
`FaceHairProvider`, `FaceTypeProvider`, `InstrumentFinishProvider`,
`BandStoreOffer`; `TrackWatcherImpl.cpp` + `GuitarController.cpp` hold the
`Button`/`RealGuitar`/`Keyboard`/`JoypadGuitar`/`JoypadMidi` controller family;
`StorePanel.cpp` holds `BandPreloadPanel` + `StoreArtLoaderPanel`.

### 4b. Spans that are genuinely UNCLAIMED (pin-ready, no move needed)

| TU (wf) | span | size | str |
|---|---|--:|---|
| `LockStepMgr` (51) | `825ABDF8..825AC9AC` | 0xBB4 | 2/2 |
| `SlotChannelMapping` (23) | `82793FB8..827946B4` | 0x6FC | – |
| `UGCPurchasePanel` (18) | `8263E6A0..8263F240` | 0xBA0 | 3/7 |
| `TourChar` (24) | `82B7FAB0..82B800E0` | 0x630 | – |
| `TourCharLocal` (17) | `82B79A40..82B79FAC` | 0x56C | – |
| `TourCharRemote` (11) | `82B803A8..82B80740` | 0x398 | – |
| `TourGameRules` (9) | `82365E68..82366098` | 0x230 | 3/3 |
| `HitTracker` (8) | `826E3528..826E3804` | 0x2DC | – |
| `Asset` (8) | `825EF708..825EFA68` | 0x360 | 6/6 |
| `LogFile` (6) | `827CBEF8..827CC144` | 0x24C | – |
| `TourGameModifier` (4) | `823699CC..82369B1C` | 0x150 | – |
| `AssetOffer` (1) | `8266B520..8266B7CC` | 0x2AC | – |
| `DrumTrackWatcherImpl` (11) | `827800B0..827808C0` | 0x810 (98 % free) | – |
| `ChordShapeGenerator` (73) | `822DD290..822E325C` (21 % free) | 0x5FCC | **43/44** |

`ChordShapeGenerator` deserves its own note: 43 of 44 Wii literals are referenced
from that span, it is the one genuinely-missing TU laneBA also flagged
(`autoid` proposal `0x822DD480`, score 6/6), and its span is heavily interleaved
with `Mesh.cpp` / `CharLipSync.cpp` / `Font.cpp` pins.

### 4b-bis. Self-consistency check on the claimers

If the instrument were simply drifting toward big pins, the *claiming* units'
own classes would land in the wrong place too. They do not. Running the same
class-owned-slot + ctor-site procedure on the claimers puts each one's evidence
overwhelmingly inside its **own** pin: `VocalPlayer` 39/44, `StorePanel` 16/18,
`Leaderboard` 12/16, `PropKeys` 7/11, `BandHighlight` 6/6, `UIStats` 2/3,
`EventDialogPanel` 3/3, `UIPicture` 2/2, `BandSwatch` 2/2. So the foreign spans
in §4a are *additional* code inside those ranges, not a relocation of the owner.
Three claimers (`SongDifficultyDisplay`, `MoveMgr`, `FlowEventListener`) produce
**no evidence at all** — 0 class-owned slots and 0 ctor sites — so for those the
instrument declines to place the owner rather than guessing.

### 4c. ★ Headroom of the whole seam: 528 functions

Measured, not estimated — every located span intersected with the current
per-function match state (VAs resolved from `fn_<VA>` names + the reverse of
`scripts/target_symbol_map.json`, never from `report.json`'s `address` field,
which is a within-unit offset, not an address):

| | fns |
|---|--:|
| currently **unmatched** inside the 35 in-pin located spans | **402** |
| already matched inside those spans (at risk during a move; most should re-pair) | 171 |
| functions in the 13 fully-unclaimed located spans (counted from `.pdata`) | **126** |
| **total currently-unscoreable, now with a name and a span** | **528** |

The full machine-readable worklist — 76 rows, every located span with its
evidence, claimers, size, current match state and a confidence label — is
committed at **`scripts/harvest/tu_locate/located_spans.json`**. Confidence is
`HIGH` when the Wii-literal hit ratio ≥ 0.8 with ≥3 hits, or when a single
claimer is corroborated by ≥3 evidence functions in a ≤8 KB span; `LOW` when the
span exceeds 8 KB with ≥3 claimers (the controller family — `KeyboardController`,
`ButtonGuitarController`, `RealGuitarController`, `JoypadGuitarController` — all
land on the same overlapping `TrackWatcherImpl`/`GuitarController` block and
cannot be separated by these instruments).

| confidence | spans | currently-unscoreable fns |
|---|--:|--:|
| **HIGH** | **42** | **504** |
| MED | 12 | 165 |
| LOW (do not act without a third channel) | 22 | 532 |

**All 42 HIGH spans have a ready rb3-Wii `.cpp` in `../rb3/src`, and they are
small: 46–548 lines, median ≈ 130.** Top of the queue by unmatched functions
(fns / Wii .cpp lines): `UIProxy` 36/189, `RealGuitarGemPlayer` 29/145,
`TourPerformerLocal` 27/542, `LockStepMgr` 25/267, `BandUserMgr` 24/548,
`UIGridProvider` 24/87, `BandPerformer` 23/223, `BandButton` 23/270,
`TourDesc` 22/248, `SlotChannelMapping` 18/117, `UGCPurchasePanel` 17/131,
`PracticeSectionProvider` 17/132, `CharProvider` 16/243, `CharSync` 16/248,
`TourChar` 15/88, `Quest` 15/99.

Biggest single blocks of unmatched code: `ChordShapeGenerator` 77,
`UIProxy` 36, `CharKeyHandMidi` 32, `RealGuitarGemPlayer` 29,
`TourPerformerLocal` 27, `UIGridProvider` 24, `BandButton` 23, `BandPerformer` 23,
`TourDesc` 22, `CharProvider` 16, `CharSync` 16.

For contrast, laneBA priced the *attribution* channel on the auto-carve pool at
**+25 to +85**. This is a different and larger channel because it is a **decomp**
channel: the functions have an oracle and a location, and what they need is a body
port. It is correspondingly more expensive per function.

### 4d. Why this is not free money

Pinning alone scores **0**. objdiff pairs Code symbols by **name**; a freshly
carved target obj holds anonymous `fn_<VA>` symbols while our compiled obj holds
MSVC-mangled ones, and `is_funclet_like` forbids anonymous↔mangled byte-pairing
(laneBA §5, `objdiff-core/src/diff/mod.rs`). So each TU needs the full chain:
**carve the span out of the foreign pin → port the Wii source so it compiles →
recover `fn_<VA>` ↔ mangled pairings (`scripts/harvest/size_order_automap.py`) →
apply the map fragment → build → A/B.** That is hours per TU, and the yield is
bounded by how well an MWCC→MSVC body port reproduces retail codegen.

---

## 5. Corrections to the census's 141 / 2,505 figure

Existence was tested against the binary itself, three ways: exact RTTI type
descriptor `.?AV<Stem>@@`; template/nested RTTI (`.?AV?$<Stem>@…`); the class
name as a verbatim Symbol string (Milo's `OBJ_CLASSNAME` interns it).

Controls: positive — 15 units known pinned in `splits.txt` → **15/15** detected.
Negative — 10 Wii-platform class names (`MicWii`, `ContentMgrWii`, `SynthWii`, …)
plus 2 nonsense strings → **0/12** false positives.

| verdict | TUs | Wii fns | Wii bytes |
|---|--:|--:|--:|
| evidence of presence in the 360 binary | **90** | **2,020** | 525,636 |
| no evidence either way | 51 | 485 | 195,932 |

Within the no-evidence bucket, three sub-claims are strong enough to act on:

* **★ `system/speex` — 10 TUs / 72 fns — FALSE POSITIVE.** 320 literals were
  extracted from the Wii speex tree; the only 11 present in `band.exe` are generic
  English words (`append`, `author`, `delete`, `frame`, `title`, `version`, …).
  `speex`/`nb_celp`/`celp`/`quant_lsp` appear **0 times**. The 360 SKU uses XHV/XMA
  voice; speex is the Wii voice codec.
* **★ `system/utl/bufstreamnand` (22 fns) and `system/utl/rso_utl` (15 fns) —
  FALSE POSITIVES.** NAND is Wii flash storage and RSO is the Wii/GC relocatable
  module format; `NAND` and `RSO_` occur 0 times in `band.exe`. These are Wii
  platform TUs that laneBB's `WII_PLATFORM` regex simply does not spell — a
  recall gap in that filter, not a join error.
* `system/utl/symbols{,2,3,4}` + `messages{,2,3,4}` (8 TUs / 18 fns / 133 KB) are
  the generated Symbol/Message tables. `Symbols*.cpp` is the documented systematic
  identification FP; treat them as data TUs, not work items.

**Net: at least 12 TUs / 109 fns are outright false positives; the honest headline
becomes ~129 TUs / ~2,396 fns.** The remaining 39 no-evidence TUs are
non-polymorphic helpers (`system/math/striper`, `customarray`, `adjacency`,
`revisitedradix`; `system/utl/intpacker`, `wav`; `system/beatmatch/submix`;
`system/dsp/iirfilter`) where absence of RTTI proves nothing — **indeterminate,
not refuted**.

### 5.1 One thing that cannot be concluded

The string channel **cannot** be used to declare a TU absent. Calibrated on
known-present pinned units, the fraction of Wii literals (len ≥ 8) present in the
360 pool has median 0.43 but a long tail to **0.00** (`CharBone.cpp` 0/16,
`Tour.cpp` 5/51, `Rot.cpp` 1/7), because retail stripped the `MILO_ASSERT` path
strings that dominate many Wii literal sets. A 0-of-N score is therefore
indistinguishable from "this TU's literals were all asserts". Units where that
bites: `BandUserMgr` (53 fns), `TrackWidgetImp` (49), `CharKeyHandMidi` (82),
`StoreOfferContentsProvider` (31), `StoreRootPanel`, `RockCentralJobs`.

*(`TrackWidgetImp` is in fact present — as `.?AV?$TrackWidgetImp@…` template
instantiations plus `TrackWidgetImpBase`. A stem-exact RTTI test misses templates.)*

### 5.2 ★ A control that passed while the instrument was dead

Worth recording because it is precisely the failure mode laneBA warned about, and
it fired here too. When `make_audit.py` was first run from a *worktree*,
`dirname(REPO)` resolved to the worktree's parent, so `../rb3/src` did not exist;
every TU then fell back to its **lowercase** Wii path component as the class name,
which matches no RTTI descriptor. The audit came out completely empty (0 RTTI
hits, 0 literal hits across all 141) — **and both controls still passed**, because
they were written with hardcoded correctly-cased class names and therefore never
exercised the broken code path. Fixed in `812bdfea` (`_paths.py` derives
`MAIN_REPO` from `git worktree list`). Lesson: a control must travel the *same*
code path as the data, not merely the same function.

---

## 6. Instruments REFUTED — do not re-fund

### 6.1 Wii → 360 link-order interpolation

The hypothesis: no LTCG on either SKU, both built from the same source tree, so
the Wii map's TU order should predict the 360's, and an unlocated TU between two
located Wii neighbours would be tightly constrained.

**Measured on 679 TU pairs (Wii `.text` first address vs 360 pin start):
global Spearman ρ = −0.02.** Per directory, the best is `band3/game` 0.37 and
`band3/meta_band` 0.33; several are *negative* (`system/synth` −0.24,
`system/beatmatch` −0.18, `system/utl` −0.15). Nowhere near usable.

### 6.2 Directory locality in `.text`

Also refuted as a locator. Per-directory 360 pin-start spread (IQR of the
lowest pin address per TU): `band3/meta_band` 948 KB over 129 TUs,
`system/rndobj` 1.29 MB, `system/ui` 3.26 MB, `system/math` 6.94 MB. A TU's
directory tells you essentially nothing about where it lands.

### 6.3 The (class, method) join against `target_symbol_map.json`

Joining each unlocated TU's Wii symbol census to the 25,675 already-transferred
MSVC names, restricted to classes the TU owns, yields **1–6 VAs per TU and they
are almost all `?StaticClassName@<Class>@@`** — header-macro COMDATs that the
linker groups into a scatter block at `0x8227A000–0x8227B000`, already finely
pinned and **already 100 % matched** inside whichever neighbour's `.cpp` includes
the header. Real signal, zero headroom. (It does confirm the mechanism: retail
`BandCharacter.obj` legitimately defines `?StaticClassName@CharKeyHandMidi@@`.)

---

## 7. Pin + wire attempts

*(filled in below)*

---

## 8. Reproduction

```bash
cd /home/free/code/milohax/rb3-xenon        # main, read-only

# 1. the 141-TU worklist
venv/bin/python scripts/harvest/oracle_coverage_matrix.py --reverse
#    -> ~/tmp/laneBB/wii_reverse.json ; ABSENT & non-network & !WII_PLATFORM = 141

# 2. splits-derived gap geometry (authoritative; NOT report.json auto boundaries)
python3 ~/tmp/laneBD/adjacency.py       # link-order refutation, 679 pairs
python3 ~/tmp/laneBD/fnva2.py           # every report fn -> real VA
python3 ~/tmp/laneBD/xref.py            # 98k lis/addi constant edges over .text

# 3. the two locating instruments
python3 ~/tmp/laneBD-vt/rtti_scan.py    # 1,415 typedescs -> 1,354 vtables
python3 ~/tmp/laneBD-vt/spans.py        # class-owned slots + ctor sites -> spans
python3 ~/tmp/laneBD-str/xref.py        # 20,141 code->string edges
python3 ~/tmp/laneBD-str/final.py       # selective-literal clusters -> spans

# 4. existence audit (RTTI + class-string), with both controls
python3 ~/tmp/laneBD/… (audit3.json)
```

Scratch (regenerable, not committed): `~/tmp/laneBD/`, `~/tmp/laneBD-vt/`,
`~/tmp/laneBD-str/`. Worktrees `~/tmp/wt-laneBD-{1,P1,P2}`.

### Re-run triggers
Re-run §2's gap census whenever `splits.txt` changes in bulk; the RTTI and string
instruments read only the retail binary and are stable until a TU-target flip.
