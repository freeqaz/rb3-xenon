# BinDiff DC3→RB3 identification — ROUND 2 (anchored second pass), 2026-07-24

**Verdict: anchoring WORKS and is the headline result.** Seeding BinDiff with our
current ground-truth as name-hash anchors lifted top-band precision **95.9% → 98.6%**,
pulled **+2,065** more ground-truth-covered matches into the top band, and — the point
of the exercise — **collapsed the sim/confidence tradeoff so that confidence ≥0.95
alone is now a reliable operating point across every similarity band** (the 0.85–0.9
band went 51.7% → 93.6%; 0.7–0.8 went 84.1% → 96.1%). Net high-confidence,
boilerplate-filtered, native-scope, oracle-gated transfer set over the anonymous pool:
**299 names** (~99% genuinely anonymous in Ghidra), all carrying a recovered full
MSVC-mangled name (directly map-insertable).

Artifacts (all under `~/tmp/`):
- `bindiff_r2_candidates_highconf.json` — the 299-name transfer set (recommended).
- `bindiff_r2_candidates.json` — full 400 in-scope anon set incl. 98 flagged DC3-only.
- `bindiff_r2_stats.json` — summary counts.
- `~/tmp/bindiff_spike/` — anchored BinExports/`.BinDiff`, all scripts (reusable).

---

## 1. Did anchoring work, and how it was done

**BinDiff CLI has no manual-anchor / fixed-match flag** (verified `--helpfull`; only
`--config`/`--print_config`). The *only* CLI-accessible anchoring is BinDiff's
**name-hash matcher**: functions with an identical, unique name on both sides are locked
as fixed points, and the structural propagation matchers (call-graph MD-index,
neighborhood) extend from them.

Round-1 got **zero** name-hash anchoring by accident: the RB3 BinExport stores
**mangled** names (`?Load@EnvLightEntry@LightPreset@@…`) while the DC3 BinExport stores
**bare** names (`Load`) — disjoint strings. Round-1's 92.9% was therefore *purely
structural*. That is exactly the headroom anchoring exploits.

**Mechanism used (no Ghidra re-export needed).** I patched the round-1 BinExport
protobufs directly (`binexport2.proto` → protoc → `patch_anchors.py`):
- Join our map's mangled names to `ham_xbox_r.map`'s mangled names (identical signature =
  the same function in the shared Milo engine → **guaranteed-correct RB3 VA ↔ DC3 VA**).
- **13,921** clean unique-both pairs; **13,598** applied (both vertices present).
- For each pair, overwrite `mangled_name` *and* `demangled_name` on both vertices with a
  unique token `ZANCHOR000123`, then re-run `bindiff` on the patched exports.

Why this had headroom: of the 13,921 clean anchors, BinDiff on its own already matched
9,033 correctly but got **4,260 WRONG** and **missed 628** → anchoring injects **4,888
new correct fixed points** the structural pass lacked. That propagated widely.

**Result of the re-run:** 37,447 matched pairs (vs 36,967 unanchored).

---

## 2. Precision tables (measured against the current 19,308-entry ground-truth map)

### 2a. Operating points, unanchored vs anchored

| operating point | unanchored | **anchored** |
|---|---|---|
| sim ≥0.9 & conf ≥0.95 | n=10,120 @ 95.9% | **n=12,185 @ 98.6%** |
| sim ≥0.85 & conf ≥0.95 | n=10,149 @ 95.8% | **n=12,279 @ 98.5%** |
| sim ≥0.9 & conf ≥0.9 | n=10,350 @ 95.2% | **n=12,327 @ 98.1%** |

### 2b. Anchored precision by (similarity × confidence) — confidence is now the discriminator

| sim band | conf ≥0.95 | conf 0.90–0.95 | conf 0.80–0.85 |
|---|---|---|---|
| ≥0.9 | **98.6%** (n=12185) | 59.2% | 16.2% |
| 0.85–0.9 | **93.6%** (n=94) | 17.5% | — |
| 0.8–0.85 | **97.4%** (n=76) | 71.4% | 44.4% |
| 0.7–0.8 | **96.1%** (n=311) | 84.7% | 35.0% |
| <0.7 | 71.1% (n=807) | 61.1% | 35.9% |

Unanchored the same low-sim/high-conf cells were 51–84%. **Chosen operating point:
conf ≥0.95 AND sim ≥0.70** (excludes the <0.7 band, which stays at 71%).

### 2c. Anchored ground-truth precision by DC3 library group (validates each candidate tier)

Exact-method match, conf ≥0.95 & sim ≥0.70, n=12,666:

| DC3 lib group | n | precision |
|---|---:|---:|
| engine (obj/utl/os/math/rndobj/world/ui/gesture/movie/meta/flow/synth/ST) | 4,809 | **94.5%** |
| char | 959 | **94.2%** |
| hamgame (meta_ham/hamobj/game) | 904 | **84.4%** |
| other/vendor (dropped from candidates) | 5,994 | 99.0% |

(Exact-method is a conservative floor — demangle-parse misses and ICF/sibling
near-misses count as errors here; the true rate is a few points higher, cf. round-1's
96.4% non-boilerplate.) The **hamgame 84.4%** is the key finding driving the oracle gate
below: DC3's Ham/game libraries contain Dance-Central-specific classes that RB3 does not
have, so a structural match there is only ~84% a real RB3 function.

---

## 3. Candidate set over the anonymous pool (native-scope, boilerplate-filtered, oracle-gated)

Anon = RB3 VA **not** in `target_symbol_map`. Boilerplate dropped (`??_*`, vector
ctor/dtor iterators, `_M_*`, STL, RTTI, thunks, funclets, template-instantiation
mangles). Vendor/Quazal/360 libraries dropped entirely (nuispeech, d3dx9, xgraphics,
xaudio2, net/net_ham, rnddx9, xapi/xhv/xonline/xnet, bink, jpeg/ogg/zlib, LIBCMT).
DC3 name recovered via `address2 → ham_xbox_r.map → mangled`.

**Oracle gate (honest RB3-existence check).** For Ham/game/char candidates I checked the
DC3 class against the **rb3-Wii oracle** (`../rb3/src`, the *real* RB3 decomp — the
rb3-xenon tree is polluted with DC3-copied headers and is NOT a valid existence test).
Classes present in rb3-Wii (MetaPerformer, SaveLoadManager, ProfileMgr, Accomplishment,
Campaign, GamePanel…) are genuinely RB3; classes absent (CampaignEra, RhythmBattle,
MoveGraph, DancerSequence, FreestyleMove, HamStorePanel…) are DC3-only → excluded.

| tier | count | disposition |
|---|---:|---|
| **A — engine (shared Milo)** | 229 | transfer (measured 94.5% / 94.2% on char) |
| **B — char, oracle-confirmed** | 48 | transfer |
| B — char, unverified | 16 | flagged (verify) |
| **C — hamgame, oracle-confirmed** | 25 | transfer (genuinely-RB3 game logic) |
| C — hamgame, DC3-only | 82 | **excluded** (RhythmBattle/MoveGraph/etc — false for RB3) |

(A further 3 template ctor/dtor/operator forms were dropped from the transfer set as
ICF-risk boilerplate that slipped the `??N?$` filter: 302 → **299**.)

**HIGH-CONFIDENCE TRANSFER SET = 299** (`bindiff_r2_candidates_highconf.json`), by
native_scope class:

| native_scope class | count | note |
|---|---:|---|
| NATIVE-VIA-DC3 (rndobj/char/world/ui/synth/…) | 193 | engine; redundant for the port but names UNKNOWN-anon → feeds recarve / whole-binary % |
| NATIVE-CORE (obj/utl/os/math) | 79 | the booted DTA/parse runtime path |
| **NATIVE-SOON (genuinely-RB3 game logic)** | **27** | the native-priority names |

The 27 NATIVE-SOON names include: `SaveLoadManager::AutoSave` / `::Start`,
`MetaPerformer::MetaPerformer`, `ProfileMgr::GetProfileFromPad`,
`Accomplishment::Accomplishment`, `AccomplishmentProgress::GiveGamerpic`,
`Campaign::LoadCampaignDanceMoves`, `GamePanel::{SetGameOver,SetGameOver,OnStartLoadSong}`,
`PresenceMgr::SetNotInGame`, `CalibrationPanel`/`EventDialogPanel`/`MetaPanel`/
`TexLoadPanel`::StaticClassName, `MidiVarLenNumber`/`DataEventList` (M2 chart parse).

---

## 4. Mangled-name recovery (map-entry vs carving-hint)

Every one of the 299 carries a **full MSVC-mangled name** recovered via the DC3-map join
(`dc3_mangled` field), not just a bare method name — this is a strict improvement over
round-1's bare-name transfer and makes them directly insertable into
`target_symbol_map.json`.

- **map_entry (engine, 272):** the DC3 mangled signature is identical across the two
  games (same Milo engine), so the recovered mangled name is the RB3 symbol verbatim →
  insert directly, then spot-verify the per-TU cluster.
- **carving-hint / verify-first (char+hamgame oracle-confirmed, 27):** class exists in
  RB3 but at ~84–94% method precision and RB3-vs-DC3 layout may differ — treat as a
  strong hint; confirm the method against the rb3-Wii oracle before pinning. (These are
  exactly the ones a landing pass should spot-check.)

---

## 5. Yield & the strategic conclusion

- **Anchored pass:** +2,065 top-band ground-truth matches and +2.7pts precision for free
  (no re-export; protobuf name-patch + re-run only).
- **New names over the anonymous pool:** **299 high-confidence** (~99% genuinely anon),
  **+98** flagged DC3-only carving hints held back.
- **NATIVE-SOON fraction: 27/299 ≈ 9%.** This is the honest and important finding: **DC3
  BinDiff is an engine-naming tool, not a game-code discovery tool.** BinDiff can only
  name code *shared* between the two titles, which is overwhelmingly Milo engine
  (272/299). RB3-unique game code (band3 / meta_band UI panels, scoring, save) has no DC3
  twin and is structurally invisible to this method — its oracle is **rb3-Wii**, not DC3.
  So: mine the 272 engine names to name UNKNOWN-anon engine functions (recarve →
  whole-binary %), take the 27 game names as a bonus, and route game-code discovery to the
  rb3-Wii correlator, not here.
- The anchoring recipe is **reusable and cheap** (~4 min: patch + re-bindiff); re-run it
  whenever the map grows meaningfully — each +N map entries becomes +N anchors and lifts
  the frontier further.

---

## 6. Repro (round-2 additions to round-1's recipe)

Round-1 BinExports still valid in `~/tmp/bindiff_spike/`. Round-2 added:
1. `protoc --python_out=. binexport2.proto` (proto at `../binexport/binexport2.proto`);
   protobuf in throwaway venv `~/tmp/bindiff_spike/pbvenv`.
2. `patch_anchors.py` — build 13.9k clean anchors (map∩ham mangled), write
   `*.anchored.BinExport`.
3. `bindiff --primary=rb3_tu5.anchored.BinExport --secondary=dc3.anchored.BinExport
   --output_format=bin` (~1.6 min).
4. `final_analysis.py` — per-lib-group gt precision + oracle-gated candidate tiers
   (uses `rb3wii_classes.txt`/`rb3wii_scopes.txt` built from `../rb3/src`).
   `evaluate3.py` — the (sim×conf) precision table for either `.BinDiff`.

**Note for the coordinator:** read-only pass, no map edits made. To land: insert the
`dc3_mangled` values from `bindiff_r2_candidates_highconf.json` at their `rb3_va` keys in
`scripts/target_symbol_map.json`; full-rebuild gate; named-LOST==0; spot-verify per-TU
clusters (engine first — highest precision). Hold the 98 DC3-only until a rb3-Wii
cross-check.
