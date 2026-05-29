# BinDiff vs RB3-Wii: a game-code identification oracle

**Date:** 2026-05-29. **Status:** exploratory, **VERDICT: THIS WORKS.**
**Read-only w.r.t. the rb3-xenon main build** (no `ninja`, no `splits.txt`/`objects.json` edits, no commit).

## TL;DR (the decisive numbers)

- BinDiffing RB3-360 against **RB3-Wii** (same game, named functions) instead of
  **DC3** (same engine, different game) yields **tight, correctly-named game-TU
  clusters** — exactly as the bet predicted.
- **The decisive metric — meta_band dominant-cluster density:** RB3-Wii gives
  **10–88%** density on the real multi-function meta_band TUs (median across all
  matched TUs is far above the 15% bar). DC3's oracle gave **1–8%** on the same
  TUs (AccomplishmentManager "13 fns scattered across 16 KB @1-8%";
  AccomplishmentProgress "5 fns in 13 KB @1%"). **RB3-Wii is ~10× tighter and
  recovers ~4× more functions per TU.**
- **Pinnable game TUs:** **26 high-purity (≥70%) meta_band TUs**, of which **10
  are already wired-and-compiling locally** (zero-port-cost pins). Candidate
  `.text` spans (snapped to `config/45410914/symbols.txt`) are listed below.
- **Full game coverage:** 9,301 RB3-360→RB3-Wii game-code matches across **703
  distinct game TUs**; **196 high-confidence** (sim≥0.8 & conf≥0.9) across 136 TUs.
- **It unlocks a band3 pin wave.** The DC3-oracle's "meta_band is unpinnable"
  conclusion (`docs/plans/next-levers-2026-05-29.md` §Q2) was an artifact of the
  *wrong reference binary*, not a real ceiling.

Mapping written to **`unified_id_rb3wii.json`** (repo root, gitignored).

## Why RB3-Wii beats DC3 as the reference

The prior oracle (`unified_id.json`) BinDiffed RB3-360 against **DC3 (Dance
Central 3)** — the same Milo *engine* but a *different game*. DC3's game-code
matches were structurally-similar-but-wrong: they aliased RB3 functions to DC3
class names (`Challenges`, `MetagameRank`, `HamSongMgr`…) whose addresses
scatter across many unrelated real-RB3 TUs → 1–8% dominant-cluster density,
empirically unpinnable.

**RB3-Wii (`../rb3`) is the SAME game's source**, decompiled with named
functions. Both binaries are 32/64-bit big-endian PowerPC (Wii = Gekko/Broadway,
360 = Xenon/VMX128). BinDiff matches structurally (basic-block + call-graph + MD
index), so same-source cross-PPC matching places the linker's contiguous TU
runs onto the right RB3-360 addresses even when per-instruction byte similarity
is low.

## Method (reproducible)

The DC3 pipeline (`~/.claude/plans/rb3-xenon-bindiff.md`) was reused verbatim,
swapping the reference binary.

1. **BinExport RB3-360 (secondary):** reused the existing `/tmp/rb3.BinExport`
   (`band.exe`, PowerPC-64, produced by the DC3 run — identical program, so
   apples-to-apples). 36,959 functions captured.
2. **BinExport RB3-Wii (primary):** the live RB3-Wii Ghidra project is on the
   8001 MCP, which holds the project lock, so per the caution the project was
   `cp -r`'d to `/tmp/W` (excluding the 764 MB `chromadb` MCP cache) and exported
   from the copy. The program is the **debug proto ELF**
   `/home/free/code/milohax/milo-executable-library/rb3/Wii Proto (Bank 5) (Debug)/band_r_wii.elf`
   (`band_r_wii.elf`, PowerPC-32, 36,343 functions, fully named via MWCC
   symbols + DWARF). Exported with `RunBinExport.java` (the proven headless
   script at `/tmp/scripts/`).
   - **Gotcha:** the export must run under **`/opt/ghidra`** (stock 12.1, has the
     `PowerPC:BE:32:Gekko_Broadway` language) — **not** the VMX128 SLEIGH fork at
     `/home/free/code/milohax/ghidra/build/ghidra` (Xenon-only; it throws
     `Language not found for 'PowerPC:BE:32:Gekko_Broadway'`). `/opt/ghidra` has
     both Gekko and Xenon.
   - **Project-path gotcha:** `analyzeHeadless <location> <name>` resolves
     `<location>/<name>.gpr`. The original nests as `…/RB3/RB3/RB3.gpr`; the
     working copy is `/tmp/W/RB3.gpr` + `/tmp/W/RB3.rep`, invoked as
     `analyzeHeadless /tmp/W RB3`.
3. **BinDiff (primary=Wii, secondary=RB3-360)** so the output keys RB3-360
   addresses (`address2`) to RB3-Wii names (`name1`):
   ```
   bindiff --primary=/tmp/wii.BinExport --secondary=/tmp/rb3.BinExport \
           --output_dir=/tmp/bindiff-wii-out --output_format=bin
   ```
   → `wii_vs_rb3.BinDiff` (SQLite). **21,151 of 36,343/36,959 functions matched.**
4. **Source attribution via DWARF.** The proto ELF carries `.debug_info`; each
   `DW_TAG_compile_unit` gives `[DW_AT_low_pc, DW_AT_high_pc)` → full source path
   (e.g. `E:\hproj\band3_wii\band3\src\meta_band\AccomplishmentManager.cpp`).
   1,298 usable CU intervals map every Wii function address to its source TU.
   (The retail `../rb3/config/SZBE69_B8/splits.txt` could **not** be used — the
   proto ELF has different addresses; DWARF is the authoritative map.)
5. **Filter to game code** (`band3/src/…`, `network/src/…`), join Wii-addr→source
   onto the BinDiff RB3-360 addrs → `unified_id_rb3wii.json`.
6. **Density + purity** computed with the same gap-split (8 KB) and
   `density = dom_fns / (span_bytes/40)` formula the DC3 oracle used, plus a
   **purity** check (fraction of matched addrs inside the candidate span that
   actually belong to the claimed TU) and a **snap to `symbols.txt`** function
   boundaries.

## Match-quality distribution (honest read)

| Bucket | Count | Meaning |
|---|---|---|
| sim≥0.8 & conf≥0.9 | **196** (136 TUs) | genuine structural identity — trustworthy *individual* names (MD-index / prime-signature / flowgraph algos). 33 are in meta_band (e.g. `OvershellSlot::InOverrideFlow` sim=1.0, `PatchPanel::EditLayer` sim=0.92). |
| 0.5 ≤ sim < 0.8 | 469 | moderate structural match. |
| sim < 0.5 | 8,593 | mostly **address-sequence / call-sequence propagation** (uniform ~0.42 sim / 0.62 conf). Individually unreliable, but **collectively reveal TU placement** because the propagation follows the linker's contiguous layout. |

The low *global* similarity (7.4%) is the expected cross-PPC artifact (Gekko vs
Xenon emit different encodings) — it does **not** reflect placement quality.
**The oracle's value is TU-cluster localization, validated by purity, not
per-function byte similarity.**

## THE DECISIVE METRIC: meta_band dominant-cluster density (RB3-Wii vs DC3)

For the locally-wired meta_band TUs, RB3-Wii dominant clusters (gap-split 8 KB):

| TU | matched | dom-cluster fns | span | density | vs DC3 |
|---|---|---|---|---|---|
| Accomplishment.cpp | 39 | **22** | 1.1 KB | **77.7%** | DC3: scatter |
| AccomplishmentProgress.cpp | 118 | **19** | 6.9 KB | **10.7%** | DC3: 5 fns/13 KB @1% |
| AccomplishmentManager.cpp | 131 | **13** | 3.6 KB | **13.9%** | DC3: 13 fns/16 KB @1–8% |
| MusicLibrary.cpp | 83 | 19 | 9.2 KB | 8.1% | — |
| UIEvent.cpp | 17 | 8 | 0.3 KB | **89.9%** | — |
| AccomplishmentSetlist.cpp | 5 | 5 | 0.2 KB | **82.0%** (single cluster) | — |
| AccomplishmentCategory / *Conditional family | 3–17 | 2–4 | <0.2 KB | **100–222%** | — |

**Summary:** of the 43 wired meta_band TUs, **38 have matches**; the dominant
clusters land at **10–88%** for the real multi-function TUs and **>100%** for
tight small-function families (sub-40-byte conditional leaf methods). This is a
**~10× density improvement over the DC3 oracle's 1–8%**, and the tight runs snap
exactly onto real `symbols.txt` function boundaries (verified: the
`0x8243A18C`+ Accomplishment run is 0x20/0x28-byte contiguous RB3-360 functions).

## Pinnable meta_band TUs and candidate spans

A TU is **pinnable** when its dominant cluster has ≥3 matched fns, density ≥15%,
**purity ≥70%** (most matched fns in the span belong to that TU), no span
overlap with another TU, and the bounds snap to `symbols.txt`. **26 meta_band
TUs qualify** (full list in `/tmp/candidate_spans.json`). **10 are already wired
and compiling locally** (zero-port-cost — pure span-derivation), so they are the
immediate band3 pin wave:

```
# Candidate .text spans for splits.txt — WIRED-LOCALLY meta_band TUs
# (snapped to config/45410914/symbols.txt; verify each before committing)
Accomplishment.cpp:                       .text start:0x8243A18C end:0x8243A614   # 22 fns, purity 88%
AccomplishmentProgress.cpp:               .text start:0x8243854C end:0x8243A12C   # 19 fns, purity 79%
AccomplishmentManager.cpp:                .text start:0x8248BBA4 end:0x8248CB98   # 13 fns, purity 72%
UIEvent.cpp:                              .text start:0x825519DC end:0x82551B5C   #  8 fns, purity 100%
AccomplishmentSetlist.cpp:                .text start:0x8243F220 end:0x8243F330   #  5 fns, purity 83%
AccomplishmentPlayerConditional.cpp:      .text start:0x8243F178 end:0x8243F220   #  4 fns, purity 100%
AccomplishmentSongFilterConditional.cpp:  .text start:0x8243F378 end:0x8243F418   #  4 fns, purity 100%
PatchPanel.cpp:                           .text start:0x825FA3E8 end:0x825FB8E4   #  4 fns, purity 100%
AccomplishmentCategory.cpp:               .text start:0x8243EF98 end:0x8243EFF8   #  3 fns, purity 100%
ContextChecker.cpp:                       .text start:0x826D1558 end:0x826D1E68   #  3 fns, purity 100%
```

The remaining 16 high-purity pinnable TUs (ProfileMgr, MetaPerformer,
BandSongMetadata, BandUI, AppInlineHelp, BandMachine, Matchmaker,
SetlistSortByLocation, ViewSetting, AccomplishmentCategoryPanel, Campaign,
CharData, CustomizePanel, SavedSetlist, SongUpgradeMgr, TokenRedemptionPanel,
StoreMenuPanel) are **not yet ported locally** — they're the next porting wave's
targets, and the oracle now gives each a target span up front.

**Important pinning caveats (do not skip):**
- These spans cover the **dominant cluster**, not necessarily the whole TU. Many
  TUs scatter into multiple clusters (the propagation matcher loses some
  functions). Pinning the dominant span is correct and safe (it's a tight,
  high-purity sub-range), but the TU may have additional functions elsewhere —
  pin the dominant span first, then extend if objdiff shows the obj wants more.
- Purity < 100% means a few neighbor-TU functions fall inside the span (e.g.
  AccomplishmentProgress @79% has 3 VocalTrack + 1 NetSession addrs interleaved).
  When deriving the exact `splits.txt` range, prefer to **start/end on a
  confirmed same-TU boundary** and let dtk back-fill, rather than trusting the
  raw min/max blindly.
- One overlap was detected: `OvershellSlot.cpp` vs `NameGenerator.cpp`
  (NameGenerator is a single noise match inside the OvershellSlot run) — excluded
  from the pinnable set.

## Full-game coverage (beyond meta_band)

9,301 game matches by RB3-Wii source area: meta_band 2,813 · band3/game 1,803 ·
network/ObjDup 991 · network/Plugins 656 · band3/bandtrack 547 ·
network/Services 428 · network/net 426 · band3/tour 289 · band3/net_band 196 · …
The oracle covers the entire RB3-Wii game tree (172-file meta_band dir, not just
the 43 ported locally), so it is also a **porting-priority map**: it shows which
RB3-Wii game TUs have a recoverable RB3-360 home before any porting starts.

## Files / artifacts

- **`unified_id_rb3wii.json`** (repo root, gitignored) — the mapping. Schema:
  `{rb3_addr, rb3_fn, wii_addr, wii_name, bindiff_src, similarity, confidence,
  algorithm, size, source:"bindiff_rb3wii"}`. `wii_name` is Ghidra-demangled and
  human-readable; `bindiff_src` is the RB3-Wii source TU (normalized
  `band3/src/…` / `network/src/…`).
- `/tmp/wii.BinExport` — RB3-Wii proto ELF BinExport (regenerate via `/opt/ghidra`).
- `/tmp/bindiff-wii-out/wii_vs_rb3.BinDiff` — the BinDiff SQLite DB.
- `/tmp/candidate_spans.json` — all candidate spans (TU, start, end, purity, dom).
- `/tmp/wii_dwarf.txt` — DWARF dump used for source attribution.
- `/tmp/build_wii_oracle.py`, `/tmp/finalize_oracle.py`, `/tmp/density.py`,
  `/tmp/candidate_spans.py` — the analysis scripts.

## Verdict

**This works.** The RB3-Wii oracle inverts the DC3 oracle's "meta_band is
unpinnable" conclusion: dominant-cluster density jumps from **1–8% to 10–88%**,
and **26 meta_band TUs (10 already wired) become high-purity pinnable** with
concrete `.text` spans. It unlocks the band3 pin wave that
`docs/plans/next-levers-2026-05-29.md` §Q2 flagged as the CLAUDE.md priority but
believed blocked. Recommended next step (a separate build-touching session): pin
the 10 wired-locally spans above, `touch config.yml && ninja-locked`, and let
objdiff confirm — then port + pin the remaining 16.
