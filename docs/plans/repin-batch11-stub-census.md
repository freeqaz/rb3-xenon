# Batch-11 — Stub / full-file-port TU census

Author: batch-11 lane-2 (SongParser + census). Baseline main = **24,834** strict
(`build/45410914/report.json` `measures.matched_functions`).
Generator: `scripts/harvest/stub_census.py` (committed). Raw data:
`~/tmp/census_rows.json`, `~/tmp/census_unwired.json`.

Scope filter: `scripts/native_scope_map.py` classes **NATIVE-CORE + NATIVE-SOON
only** (VIA-DC3 renderer + 360-ONLY excluded — out of native-port scope / DC3
supplies wholesale). **412** CORE+SOON wired TUs scanned.

Oracle line counts: **dc3** (`../dc3-decomp/src`) for `system/*` engine, **rb3-Wii**
(`../rb3/src`) for `band3/*`, `meta_ham/*`, `network` game code; exact-relpath
preferred, basename fallback. `ratio = oracle_lines / xenon_lines`.

---

## Headline: the full-file-port stub vein is ~98% drained

| bucket | count | read |
|---|--:|---|
| **done** (0 unmatched fns / ≥99% strict) | 69 | fully matched |
| **ported** (ratio < 1.30, source ~= oracle in-tree) | 225 | source complete; residue = body-divergence walls |
| **partial-body** (ratio ~1.0, strict% low, ≥4 unmatched fns) | 110 | source present, per-fn bodyport residue — **batch-10 confirmed walled** |
| **HIGH/MED/LOW full-file-port stub** (oracle materially fuller) | **6** | the literal deliverable-2 target — only 6 remain, top one a known wall |
| no-oracle / other | 2 | — |

**The batch-10 primary vein ("full-file-port stubbed-but-pinned TUs") is nearly
exhausted.** 294 of 412 CORE+SOON TUs are already ported or done; only **6** TUs
have an oracle materially fuller (ratio ≥ 1.30) than the in-tree `.cpp`, and the
fullest (BeatMatchController) is the **no-pairing wall batch-10 already reverted**.

### Validated negative — the anon-`fn_` "automap-recovery" idea does NOT generalize

I initially ranked 279 already-pinned units by their count of unmapped anon
`fn_` target symbols (13,228 total), hypothesising the SongParser map-recovery
lever would harvest them. **Dry-run falsified this:** `size_order_automap` emits
**0** pairings on all 5 top-anon units tested (DataFunc, UIStats, SessionMgr,
MoveMgr, NextSongPanel). The anon residue in *already-pinned* units is EH
funclets + mixed-owner foreign COMDATs + genuinely-divergent bodies — there is
no byte-matching compiled counterpart left to pair. **Do not price a wave on
anon-`fn_` counts.**

SongParser's **+16 this session came from pinning a NEW byte-matching
continuation span** (0x827848CC..0x82788288), not from recovering existing anon.
That is the real adjacent vein (below).

---

## Deliverable-1 result (for the record)

SongParser 3rd span **0x827848CC..0x82788288** (dtkBIG 53, mixed-owner):
carved out the two interleaved SongCollision COMDATs
(0x82787670..0x827876C4, 0x82787718..0x82787778) → three sub-spans pinned under
`SongParser.cpp`. Source already complete (batch-10 full-file port);
`size_order_automap` recovered **12 EXACT/STRONG** pairings → **+16 strict, 0
regressions** (24,834 → 24,850). Committed. **This is the template the batch-11
seeds below should follow.**

---

## Table 1 — Genuine full-file-port stub candidates (oracle materially fuller)

Only 6. `ratio ≥ 1.30`. **Screen each against the batch-10 wall log.**

| # | TU | class | milestone | xenon_ln | oracle_ln | ratio | strict fn% | rem | note |
|--:|---|---|---|--:|--:|--:|--:|--:|---|
| 1 | BeatMatchController | SOON | M3 gameplay | 48 | 109 (wii) | 2.27 | 57.1 | 3 | **KNOWN WALL** — batch-10 reverted (0 pairings). Skip. |
| 2 | AccomplishmentCategory | SOON | M4 save | 27 | 36 (wii) | 1.33 | 40.0 | 3 | small; try full-file port + automap |
| 3 | SongSelectPanel | SOON | M4 UI | 219 | 556 (wii) | 2.54 | 82.9 | 12 | wii inflated by asserts; already 82.9% — likely mostly ported, wii ratio misleading |
| 4 | HamIKSkeleton | SOON | M3 Ham | 109 | 206 (dc3) | 1.89 | 0.0 | 1 | dc3 oracle; only 1 fn pinned — extend span first |
| 5 | HamIKEffector | SOON | M3 Ham | 721 | 1261 (dc3) | 1.75 | 0.0 | 1 | dc3 oracle; only 1 fn pinned — extend span first |
| 6 | HamDriver | SOON | M3 Ham | 329 | 532 (dc3) | 1.62 | 0.0 | 2 | dc3 oracle; span barely pinned |

Caveat: rb3-Wii keeps `MILO_ASSERT` path strings + comments, so `wii` line counts
over-state real code — a high wii-ratio on an already-partly-matched TU (e.g.
SongSelectPanel 82.9%) is usually assert inflation, not a stub. dc3 counts are
stripped-flags-honest. The Ham* trio (dc3 oracle, 0% strict, 1-2 fns pinned) are
**span-extension** candidates, not pure ports — pin more of their `.text` first.

---

## Table 2 — The real adjacent vein: continuation-span pins (SongParser shape)

The +16 lever = a **ported** TU whose linker-scattered COMDATs land in `.text`
spans the current pin doesn't cover. Pin the extra byte-matching span → automap
pairs the whole cluster, **zero source edits**. Detected by
`scripts/harvest/comdat_scatter_scan.py` (separates scattered-but-compiled from
genuinely unwired). This is where next-wave yield actually lives — run the
scatter scanner to rank, then pin+automap each like SongParser.

High-`rem`, source-present (`ratio ≈ 1.0`) ported TUs = the scatter-scan input
pool (co-signal; scatter-scan disambiguates scattered vs divergent). Top rows:

| TU | class | milestone | strict fn% | unmatched fns | note |
|---|---|---|--:|--:|---|
| SaveLoadManager | SOON | M4 save | 7.4 | 375 | huge headroom; memory: prior "+28 wall-mapped" — verify scatter vs divergent |
| RockCentral | SOON | M4 UI | 51.5 | 366 | Quazal-adjacent; check /Od split |
| NextSongPanel | SOON | M4 UI | 16.4 | 295 | |
| UIStats | SOON | M4 save | 26.6 | 163 | |
| VocalPlayer | SOON | M3 gameplay | 48.7 | 162 | |
| SessionMgr | SOON | M4 UI | 28.9 | 133 | |
| TrackWatcherImpl | SOON | M3 gameplay | 40.7 | 124 | |
| MoveMgr | SOON | M3 Ham | 19.4 | 116 | dc3 oracle |
| DirLoader | CORE | M0 boot | 50.9 | 114 | CORE — high native value |
| Anim | CORE | M0 boot | 35.8 | 113 | CORE |
| HamCamTransform | SOON | M3 Ham | 37.6 | 113 | dc3 oracle |
| DataFunc | CORE | M0 boot | 43.6 | 110 | CORE |

**Do not assume these convert.** `size_order_automap` on existing pins of
DataFunc/UIStats/SessionMgr/MoveMgr/NextSongPanel emits 0 — their *current-span*
residue is walls. Yield requires finding *new* scattered spans (scatter-scan),
which these high-rem TUs are the likeliest to have.

---

## Table 3 — UNWIRED in-scope oracle TUs (gameport / wire lever)

**208** `.cpp` present in an oracle tree, CORE/SOON scope, **no `objects.json`
entry, no in-tree `src/` file** (`.permuter_work`/`symbols*`/`dataflex`/platform
scratch filtered). Wiring these needs a located `.text` span (BinDiff/gameid) —
harder than a repin, lower confidence. Top by oracle size:

| # | rel path | oracle | ln | # | rel path | oracle | ln |
|--:|---|---|--:|--:|---|---|--:|
| 1 | band3/game/bandusermgr.cpp | wii | 548 | 8 | system/beatmatch/gamegem.cpp | wii | 337 |
| 2 | band3/tour/tourperformerlocal.cpp | wii | 542 | 9 | band3/meta_band/inputmgr.cpp | wii | 310 |
| 3 | system/hamobj/clipplayer.cpp | dc3 | 572 | 10 | system/beatmatch/gamegemlist.cpp | wii | 280 |
| 4 | system/hamobj/hamscrollbehavior.cpp | dc3 | 389 | 11 | band3/meta_band/lockstepmgr.cpp | wii | 267 |
| 5 | system/hamobj/hamvisdir.cpp | dc3 | 377 | 12 | system/beatmatch/playertrackconfiglist.cpp | wii | 251 |
| 6 | band3/net_band/entityuploader.cpp | wii | 365 | 13 | band3/meta_band/charsync.cpp | wii | 248 |
| 7 | system/track/trackwidgetimp.cpp | wii | 360 | 14 | system/hamobj/supereasyremixer.cpp | dc3 | 324 |

(Many `system/hamobj/*` + `system/math/strips/*` are dc3/Wii-only and may not
exist in RB3-360 retail — verify span existence in Ghidra `:8002` before wiring.
`band3/*` game TUs are the higher-confidence subset.)

---

## Census summary & batch-11 recommendation

- **Stub TUs remaining (full-file-port, oracle-fuller): 6** — 1 known wall
  (BeatMatchController), 3 span-extension (Ham* trio), 2 tractable ports
  (AccomplishmentCategory, SongSelectPanel-if-not-assert-inflated). **Est yield
  ≈ +3 to +12** — the classic full-file-port vein is drained.
- **Partial-body residue: 110 TUs / 4,544 unmatched fns** — body-divergence
  walls (batch-10 confirmed exhausted-residue). **Not full-file-portable.** Do
  not fund per-fn bodyport here.
- **Real remaining yield = continuation-span pins** (SongParser shape): run
  `comdat_scatter_scan.py` over the Table-2 pool; each scattered cluster pins +
  automaps at 0-regression like SongParser's +16. Yield unknown until scanned;
  this is the honest primary batch-11 lever, **not** anon-count farming
  (validated 0-yield on existing pins).
- **Unwired pool: 208 in-scope files** — gameport lever (needs span location);
  band3 subset highest-confidence, hamobj/strips likely DC3-only.

### Top ~15 seeds for the next wave (ranked, honest)

| # | seed | lever | expected | risk |
|--:|---|---|---|---|
| 1 | `comdat_scatter_scan.py` over SaveLoadManager | continuation-span pin+automap | MED-HIGH (375 rem) | verify scatter vs wall |
| 2 | scatter-scan RockCentral | continuation-span | MED (366 rem) | Quazal /Od |
| 3 | scatter-scan NextSongPanel | continuation-span | MED (295 rem) | |
| 4 | scatter-scan DirLoader (CORE) | continuation-span | MED (114 rem) | high native value |
| 5 | scatter-scan Anim / DataFunc (CORE) | continuation-span | LOW-MED | CORE boot path |
| 6 | scatter-scan UIStats | continuation-span | LOW-MED (163 rem) | |
| 7 | scatter-scan VocalPlayer | continuation-span | LOW-MED (162 rem) | |
| 8 | scatter-scan SessionMgr | continuation-span | LOW-MED (133 rem) | |
| 9 | scatter-scan MoveMgr / HamCamTransform | continuation-span | LOW-MED | dc3 oracle |
| 10 | AccomplishmentCategory full-file port | stub port + automap | LOW (+~3) | small |
| 11 | HamIKEffector span-extend + dc3 port | span-extend | LOW | 0% now |
| 12 | HamDriver span-extend + dc3 port | span-extend | LOW | 0% now |
| 13 | wire band3/game/bandusermgr.cpp | gameport | LOW | needs span loc |
| 14 | wire band3/meta_band/inputmgr.cpp | gameport | LOW | needs span loc |
| 15 | wire system/beatmatch/gamegem.cpp + gamegemlist.cpp | gameport | LOW | needs span loc |

**Bottom line:** batch-10's "full-file-port stubbed TUs" primary is drained
(6 candidates, ~+3-12). The live successor lever is **continuation-span pins via
`comdat_scatter_scan.py`** on the high-`rem` ported TUs — the exact mechanism
that paid SongParser +16 — not the anon-`fn_` residue (validated 0-yield).
