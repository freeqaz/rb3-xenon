# W16-FB — the "named but scores zero" vein, classified; and one re-home landed

**Tree:** `1d028679`. Ruler: `name_check` (objdiff 4.2.9, `tool_binary_hash
5a51cd51fe0a353f`), read from `report.json`'s own `provenance` block.
Campaign baseline on this commit: 43,991 fns / 4,134,460 B / 40.347736% /
fuzzy 50.031284 / `total_code` 10,247,068.

## 1. The population reproduces exactly

The brief's signature — a **named** row inside a **pairable** unit that
nonetheless scores `fuzzy == 0` — measures **316 rows / 46,000 B**, byte-for-byte
the briefed figure. (CLAUDE.md's older 267 rows / 38,096 B is stale, as the brief
warned. Re-measured, not inherited.)

## 2. The classification instrument, and its validation

The kill-check is the COFF symbol table of **our compiled** object for the unit:
does it DEFINE the symbol, merely reference it (UNDEF), or not mention it at all?

That instrument is **validated, not assumed**: it predicts objdiff's own
behaviour. Every one of the 263 rows whose object does not define the symbol has
**no `match_percent_normalized` key at all** in `report.json` — objdiff never
computed a diff — and 43 of the 53 rows whose object does define it **were**
diffed. The COFF table explains the pairing outcome in 306 of 316 rows.

⚠ Anti-vacuity: a reflinked worktree's target objs are pre-renamer, so retail
mangled names read "absent" until you build. Worktree built first; target
`GemPlayer.obj` carries **1,430 symbols / 398 mangled**, identical to main, and
`??0Player@@…` is present. Negatives are therefore trustworthy.

## 3. Census — and two classes the brief's taxonomy did not anticipate

| class | rows | bytes |
|---|---:|---:|
| **4 — absent by design** (XDK / CRT `rtti`+`osfinfo` / vendor DSP `FFT`,`GranularSynth`,`PeakDetector` / XAudio2 / LEAPCORE) | 47 | **19,360** |
| **1 — missing source** (79 general + 36 STL instantiations our tree never instantiates) | 115 | 13,136 |
| **3 — mis-homed pin** (31 unique-home + 70 multi-home template COMDATs) | 101 | 7,512 |
| **2 — wrong map name** | not separable en masse — see §3.1 | — |
| **C5 — paired, divergent body** (NOT a pairing failure) | 43 | 5,684 |
| **C6 — defined but unpaired** (anomaly) | 10 | 308 |
| total | 316 | 46,000 |

**The headline is class 4: 42% of the bytes are vendor/CRT source we will never
write.** The vein is much smaller than 46,000 B for any actionable purpose.

**C5 is a correction to the brief's framing.** 43 rows / 5,684 B are not pairing
failures at all: they pair fine and simply score ~0 because our body is entirely
different (`?PollRefresh@XboxContentMgr@@` mpn 1.04, `?DrawShowing@Character@@`
mpn 2.55, `?SyncProperty@RndLine@@` mpn 3.55). Their bodies are *larger* than
retail's, so they are not stubs either. That is ordinary decomp work, not
map/pin/wiring work, and it should not be briefed to a map lane.

### 3.1 Why class 2 is not separated

Proving a map name wrong requires per-row retail-byte adjudication; it cannot be
done en masse by any cheap instrument, and a wrong name is **indistinguishable
from missing source** by the COFF check alone (both read "our build defines
nothing under that spelling"). I therefore fold class 2 into class 1 for the
census and adjudicate only the row I act on. Claiming a split I did not measure
would be the same error the schema comment in `target_symbol_map.json` records
three lanes making.

## 4. The fix: `??0Player` is a mis-homed pin, adjudicated on geometry

`??0Player@@QAA@PAVBandUser@@PAVBand@@HPAVBeatMaster@@@Z` (708 B, `0x826a75c8`)
is the largest actionable row in the census. It is **DEFINED in
`build/45410914/src/band3/game/Player.obj`** and merely UNDEF in
`GemPlayer.obj` — the object objdiff consults.

Retail-byte / geometry evidence, **not** the metric:

- `band3/game/Player.cpp` is pinned `0x826a2af8-0x826a7558`, then resumes at
  `0x826a79d0-0x826a7b38`. `GemPlayer.cpp` owns one lone block
  **`0x826a7558-0x826a79d0`, wedged exactly in that hole.**
- Every other GemPlayer block is at `0x826bbd58`+ — the nearest is **0x14388
  away**. A 1,144-byte island that far from its TU is not real TU content, and
  RB3 has no LTCG, so `.text` TU grouping is preserved (CLAUDE.md).
- The block contains **exactly one** mapped symbol (`??0Player`) and **zero**
  anonymous `fn_` rows in either unit's report.
- Our `Player.cpp:66` defines `Player::Player(BandUser*, Band*, int,
  BeatMaster*)` — the exact signature — compiling to a 1,036 B COMDAT with the
  symbol at offset 8 (the 8-byte EH prefix).

Edit: move that one `.text` line from `GemPlayer.cpp` to `band3/game/Player.cpp`
in `config/45410914/splits.txt`. Boundaries unchanged; `.pdata` is derived output
and must not be hand-edited.

## 5. PRE-REGISTERED PREDICTIONS (written and committed before measuring)

- **P1 (pairing, the actual deliverable).** After the move, `??0Player` appears
  under unit `default/band3/game/Player` **with a `match_percent_normalized` key
  present** (objdiff computes a diff) and disappears from `default/GemPlayer`.
- **P2 (metric).** `matched_code` is all-or-nothing per row, so Δ is either
  **+708 B / +1 fn** (row crosses to `fuzzy == 100`) or **exactly 0**. I predict
  **0** as the modal outcome: a 708-byte constructor matching on first pairing is
  unlikely. This lands as a *pairability/correctness* fix, per the standing
  finding that pairability is a correctness instrument and not a scoring one.
  A Δ0 here is a **success**, not a refutation.
- **P3 (falsifiable neutrality).** `total_code` is **exactly unchanged**. No
  address is added to or removed from the pinned set — this is pure
  reattribution. If `total_code` moves, my model of the edit is wrong.
- **P4 (the named would-be FALSE POSITIVE — the ruler must DECLINE these).**
  - `??0BeatMatchController@@QAA@PAVUser@@PBVDataArray@@_N@Z` (260 B, unit
    `RGGemMatcher`, class 1 — defined nowhere in our build) **must stay at
    `fuzzy == 0` and remain unpaired.** No pin edit can bind a symbol our build
    never defines. If it improves, the run is contaminated.
  - `?SetTrack@Player@@UAAXH@Z` (`0x826a79d0`, already inside Player.cpp's
    existing block, untouched by this edit) **must be unchanged.**
- **P5 (row bookkeeping).** `default/GemPlayer` `total_functions` 347 → 346;
  `default/band3/game/Player` 185 → 186.

## 6. Deliberately NOT done

- **Three further re-home candidates are pre-screened but NOT landed** (strict
  test: block flanked on both sides by the defining TU *and* an outlier for its
  current owner): `_M_allocate_and_copy<RndBone>` 100 B (PanelDir→Mesh, 0xb3d00
  away), `ObjDirItr<BandList>::operator++` 84 B (CharClipSet→OvershellDir,
  0xc5380), `list<CheatLog>::~list` 4 B (WaveFile→Cheats, 0x113ec). All three are
  **template COMDATs**, which a TU may legitimately emit itself, so "our build
  happens to emit it in TU X" is weaker evidence than for a class's own
  constructor. Landing them in the same patch would also muddy attribution.
- **Class 4 (19,360 B, 42% of the vein) is closed by standing directive** —
  writing Microsoft/vendor source is out of scope, and stubbing it to "pairable"
  would buy rows at 0% with no content.
- **C6 (10 rows / 308 B)** — defined in their own object yet never diffed
  (`?HasMic@GameMicManager@@`, `?CreateVertexShader@DxShader@@`). Recorded, not
  chased: 308 B does not justify the instrument work.

## 7. MEASURED — the re-home alone (A/B, leg A == campaign baseline)

```
leg A: matched=43991 masked=23228 honest=20763 code%=40.347736
leg B: matched=43998 masked=23235 honest=20763 code%=40.350900
Δmatched=+7  Δmasked_equal=+7  Δhonest=+0  Δcode_bytes=+324  Δcode%=+0.003164pp
unit: default/band3/game/Player 159->166
```

| prediction | outcome |
|---|---|
| **P1** pairing | ✅ **CONFIRMED, and stronger than predicted.** `??0Player` moved `default/GemPlayer` (mpn 0.0) → `default/band3/game/Player` at **mpn 99.43503 / fuzzy 99.43503**. |
| **P2** Δcode ∈ {0, +708} | ❌ **WRONG — measured +324.** |
| **P3** `total_code` unchanged | ✅ **CONFIRMED exactly** (10,247,068 / 69,240 on both legs). |
| **P4a** BeatMatchController declines | ✅ **CONFIRMED** — stays `fuzzy == 0`, unpaired, in RGGemMatcher. |
| **P4b** `?SetTrack@Player@@` unchanged | ✅ **CONFIRMED** — 100.0 → 100.0; only its unit-relative `address` moved 20976 → 22112 (= +1,136, the block size). My first comparator used whole-object equality and called that a move; the comparator was over-strict, not the result. |
| **P5** ±1 row | ❌ **WRONG — measured ±9.** GemPlayer 347→338, Player 185→194; `total_code` 50,868→49,732 and 25,468→26,604, conserved to the byte. |

**P2 and P5 failed for one shared reason, and it is the useful part of this
record: I reasoned about the one NAMED row and forgot the block carries 8 other
rows.** The moved block is 1,136 B, of which the named ctor is 708 B; the rest is
EH funclets that objdiff pairs by byte signature. That is why all 7 gains are
`masked_equal` and Δhonest is exactly 0. **When pricing a pin re-home, price the
BLOCK, not the row** — the row is what you noticed, the block is what moves.

⚠ Corollary for anyone repricing this vein: the census sizes are *row* sizes, so
the 3,208 B of unique-home class-3 rows understates what a re-home actually moves.

## 8. The re-home turned an invisible row into a ONE-INSTRUCTION near-miss

Paired at last, `??0Player` showed **1 mismatched instruction out of 177**:
`[116] delete: stw r29, 0x2f8(r30)` — target 708 B vs our 704 B, exactly one
missing store.

Adjudicated on retail bytes, not the metric: the target zero-fills a run at
`0x2e5`–`0x2f8` and stores `r8` at `0x2f4`, `r29(=0)` at `0x2f8`, `r26` at
`0x2fc`. `scripts/harvest/class_layout_report.py Player` (the compiler, not the
header comments) puts **`mUnkTU5_tail` at exactly 0x2f8**, between `unk2c0`
(0x2f4) and `unk2c4` (0x2fc) — so our layout was already right and the
constructor simply never initialised the member. It is even initialised in
Player.h:87's other ctor. Fix: add `mUnkTU5_tail(0)` between `unk2c0(-1)` and
`unk2c4(1)`; MSVC emits member inits in declaration order, so position follows.

### PRE-REGISTERED (written before the second measurement)

- **P6.** `??0Player` reaches `fuzzy == 100` and the combined patch measures
  **Δmatched = +8** and **Δmatched_code = +1,032 B** (= the re-home's +324 plus
  the ctor's own 708). Both legs A are the identical clean HEAD, so the two runs
  decompose legitimately.
- **P7.** `total_code` remains **10,247,068** — a member init cannot move the
  denominator.
- **P8 (must NOT move).** `??0BeatMatchController@@…` stays `fuzzy == 0` and
  unpaired, again. And `default/GemPlayer`'s `matched_functions` stays **305** —
  the source edit is confined to Player.cpp and must not touch it.
