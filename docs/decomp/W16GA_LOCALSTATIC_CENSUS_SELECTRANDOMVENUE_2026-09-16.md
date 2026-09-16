# W16-GA — generalising the packed-guard local-static census, and `MetaPerformer::SelectRandomVenue`

Date: 2026-09-16 · branch `w16-ga` · worktree off main `51d4c06d`
Generalises W16-FY (`docs/decomp/W16FY_ACCOMPLISHMENT_CONFIGURE_2026-09-16.md`).

## Headline

| measure | leg A | leg B | delta |
|---|---:|---:|---:|
| row `?SelectRandomVenue@MetaPerformer@@QAAXXZ` `fuzzy` | 58.328484 | **99.958420** | **+41.63 pp** |
| row `match_percent_normalized` | 59.544697 | **100.0** | +40.46 pp |
| our body size | 1,264 B | **1,924 B** | target is 1,924 B — **exact** |
| whole `matched_functions` | 44,131 | 44,133 | **+2** |
| whole `matched_code` | — | — | **+40 B** |
| whole `masked_equal_functions` | 23,322 | 23,323 | +1 |
| whole **honest** (`matched − masked_equal`) | 20,809 | 20,810 | **+1** |
| whole `matched_code_percent` | 40.629654 | 40.630043 | +0.000389 pp |
| whole `fuzzy_match_percent` | 50.487220 | 50.494823 | +0.007603 pp |

`tools/ab_measure.py --patch`, settled on both legs, report cache wiped, parsed
by exact key, graded `name_check` ruler. Leg A reproduced main exactly.

## Part 1 — the census, and how I proved it is not vacuous

`tools/localstatic_census.py` (committed). MSVC packs up to 32 function-local
statics into ONE guard int; the per-static shape is
`lwz / clrlwi.|rlwinm. / bne / ori|oris BIT / stw`, then the ctor.

**It keys on the CLAIM (`ori`/`oris`), never the TEST.** The brief warned a
census keyed on `andi.`/`andis.` reads zero; measured, that is not merely true
but quantified: of the 2,086 functions that really carry a packed guard,
**100% test with `clrlwi.`/`rlwinm.` and only 3.7% contain `andi.` at all.**

Everything is keyed on the `.fn fn_<addr>` symbol, never the address column —
`fn_82594EF8`'s body renders at column `822754D4`, so an address-keyed scan is
wrong by construction here.

### Self-validation (a scan that cannot find the known case is worthless)

| control | result |
|---|---|
| **positive** — `fn_82594EF8` (W16-FY, proven 32) | `nstatics=32, claims=32, union=0xFFFFFFFF, popcount=32` — reproduces FY's independently-derived 16 `ori` + 16 `oris` exactly, by a different code path |
| **untreated population** — all 52,224 split functions | 22.1% load some `lbl@l()`; **18.3% both load AND store one**; only **4.0% (2,086)** pass the full claim sequence |
| **sabotage** — require the `ori` to write a *different* register than the guard was loaded into | **2,086 → 1.** The detector can return a negative |

The untreated-population control is the one that matters: "touches a data label"
fires on 9,545 functions, so it is the register-matched `ori` window that
discriminates, not proximity. Without it the 2,086 would have been a number that
confirmed whatever it was pointed at — the `/Od`-detector failure mode.

### Census result

2,118 guard words over **2,086 functions**. **1,635 of 1,750 bit-claims are
followed by `??0Symbol@@QAA@PBD@Z`** ⇒ `static Symbol` is overwhelmingly the
dominant packed-guard idiom in this binary. 1,717 of the 2,086 have a mangled
name; **160 are named rows below `fuzzy` 100**.

### Cross-reference to our source — the actual defect class

"Retail has local statics" is not a defect. **"Retail has them and we don't" is.**
Both sides' instructions were dumped with `objdiff-cli diff` (no `--build`, so the
six patchers are untouched) and `??0Symbol` calls counted per side. **14 rows**
have retail constructing ≥2 more Symbols in-body than we do:

| dSym | guard bits | target B | our B | gap B | fuzzy | unit | symbol |
|---:|---:|---:|---:|---:|---:|---|---|
| 15 | 15 | 1924 | 1264 | 660 | 58.328 | `MetaPerformer` | `?SelectRandomVenue@MetaPerformer@@QAAXXZ` |
| 3 | 3 | 436 | 120 | 316 | 23.073 | `PlayerLeaderboards` | `?OnSelectRow@PlayerLeaderboard@@UAA?AVSymbol@@HPAVBandUser@@@Z` |
| 5 | 1 | 712 | 492 | 220 | 67.955 | `MicInputArrow` | `?Handle@MicInputArrow@@UAA?AVDataNode@@PAVDataArray@@_N@Z` |
| 5 | 5 | 1164 | 956 | 208 | 74.323 | `VocalTrackDir` | `?ApplyFontStyle@VocalTrackDir@@QAAXPAVObject@Hmx@@@Z` |
| 3 | 3 | 604 | 408 | 196 | 61.430 | `BandDirector` | `?EnterVenue@BandDirector@@QAAXXZ` |
| 3 | 1 | 360 | 224 | 136 | 59.922 | `OutfitConfig` | `?PropSync@@YA_NAAVMeshAO@OutfitConfig@@AAVDataNode@@PAVDataArray@@HW4PropOp@@@Z` |
| 3 | 3 | 332 | 204 | 128 | 53.301 | `BandDirector` | `?GetModeInst@BandDirector@@QAA?AVSymbol@@V2@@Z` |
| 3 | 3 | 224 | 120 | 104 | 24.964 | `BandSongMgr` | `?RankTierToken@BandSongMgr@@QBA?AVSymbol@@H@Z` |
| 2 | 2 | 404 | 316 | 88 | 70.624 | `GameConfig` | `?GetController@GameConfig@@QBA?AVSymbol@@PAVBandUser@@@Z` |
| 2 | 2 | 316 | 228 | 88 | 57.696 | `MetaPerformer` | `?PartPlaysInSet@MetaPerformer@@QBA_NVSymbol@@@Z` |
| 2 | 2 | 292 | 204 | 88 | 53.260 | `MetaPerformer` | `?GetSetlistMaxVocalParts@MetaPerformer@@QBAHXZ` |
| 2 | 2 | 364 | 276 | 88 | 62.670 | `MetaPerformer` | `?GetHighestDifficultyForPart@MetaPerformer@@QBAHVSymbol@@@Z` |
| 2 | 2 | 188 | 124 | 64 | 38.574 | `AccomplishmentCategory` | `?Configure@AccomplishmentCategory@@QAAXPBVDataArray@@@Z` |
| 2 | 2 | 236 | 172 | 64 | 48.153 | `BandSongMetadata` | `?Rank@BandSongMetadata@@QBAMVSymbol@@@Z` |

All 14 are game layer or bandobj. **One is fixed here; the other 13 are the
deliverable for the next lane** — they are pre-adjudicated, and the fix recipe is
below. Note `MicInputArrow` and `OutfitConfig` show more ctors than guard bits
(5 vs 1, 3 vs 1): those mix local statics with genuine temporaries, so they need
per-site reading rather than a blanket conversion.

## Part 2 — the fix

`?SelectRandomVenue@MetaPerformer@@QAAXXZ`, `src/band3/meta_band/MetaPerformer.cpp`,
retail body at `0x82581360`. Chosen over the engine rows per the standing
game-first directive; it is also the largest single gap **and** MetaPerformer
holds 4 of the 14 rows, so the TU is a force multiplier.

### Defect 1 — 15 `Symbols*.h` externs → 15 function-local statics (660 B)

Three independent signals had to agree before this was more than a guess:

1. the census says **15 guard bits** (`lbl_82DFEA44`, union `0x7FFF`, popcount 15,
   15 Symbol slots at `lbl_82DFEA08..EA40`);
2. our source used **exactly 15** distinct `Symbols*.h` externs, and that set is
   **identical** to the 15 string literals resolved out of `orig/45410914/band.exe`
   by parsing PE section headers (a presence control — an absence would have
   been meaningful);
3. 15 × ~44 B = **660 B**, which is *exactly* the measured target-vs-base gap.

**Declaration order is load-bearing and was read off retail, not guessed.** MSVC
emits the guard at the point of *declaration*, so retail's layout says where the
declarations were: 12 contiguous claims (bits `0x1`..`0x800`) complete **before
the function's first real call**, then 3 more (`0x1000`..`0x4000`,
`campaignlevel_van/bus/jet`) immediately after the `if (profile)` test. Reproduced
as a 12-declaration block at the top and a 3-declaration block inside `if (profile)`.

⚠ **The bit order is NOT source-execution order.** `venues` is bit 5 while
`key_video_venues` — used *earlier* in the flow — is bit 11. Deriving the order
from execution flow gives the wrong assignment; derive it from the emitted claim
order.

Result: `fuzzy` 58.32848 → **99.48025**, body 1,264 → 1,916 B.

### Defect 2 — the nested `TheSongMgr` call (8 B, and 6 of the 8 residual rows)

Retail re-reads `TheSongMgrPtr` **and reloads its vtable** for the outer virtual
call; nested, MSVC hoists the outer object's vtable load above the inner `bctrl`
and caches it in a callee-save register. `TheSongMgr` is `(*TheSongMgrPtr)` and
both callees are virtual. Computing the argument into a named local first forces
retail's order.

Result: **99.48025 → 99.95842**, body **1,924 B == target 1,924 B exactly**.

## The honest-vs-disclosure split, and where my prediction missed

Pre-registered: `matched_functions` **+8..+18**, `matched_code` **+256..+576 B**,
`masked_equal` ≈ Δmatched, **Δhonest = 0**.

Measured: **+2 / +40 B / +1 / Δhonest +1.**

| | rows | bytes | mechanism |
|---|---:|---:|---|
| **honest** | **+1** | **+0 B** | the target row itself — real reconstructed body |
| **disclosure** | +1 | +40 B | `fn_82581CC4`, a 40 B EH funclet, `masked_equal=true`, 99.8 → 100 |

Two things to carry forward, both of which correct the brief's implicit model:

**(a) The row banks ZERO bytes but DOES count as a matched function.** Its `mpn`
reached **100.0** while `fuzzy` stopped at 99.95842, because both residual charges
are `diff_arg` and `mpn` excludes arg-only penalties. So the 1,924 B of correctly
reconstructed body pays **+1 function and 0 bytes**. I pre-registered fuzzy *and*
mpn at 99.958 and was **wrong about mpn** — it crossed.

**(b) FY's +704 B funclet windfall does NOT generalise.** I predicted ~14 new
funclets by analogy with FY's 22. **Zero new rows appeared** (`legB only: []`) and
exactly one existing funclet moved 99.8 → 100. FY's windfall came from its rows
sitting at `fuzzy` 0 and starting to pair; MetaPerformer's funclets already
existed and already nearly matched. ⇒ **do not price a local-statics fix off FY's
byte figure** — the side-effect payout is a property of the *pre-state*, not of
the lever.

The inverted gate passed: aggregate `fuzzy` moved **+0.007603 pp**, so the leg
genuinely built. That was the only thing separating this result from "never built,
read a stale report.json", because the honest prediction was ~0.

## Dead ends (narrated so the next lane does not re-hunt them)

The 2 surviving charges are commutative `add` operand order —
`add r3, r18, r11` vs `add r3, r11, r18` — **with identical registers on both
sides**, inside the inlined `DataArray::Node(i)` (`return mNodes[i]`).

* **"It is a header lever" — REFUTED.** Retail uses **both** operand orders at
  four structurally identical sites in this one function, and we already match
  two of them. No uniform spelling of `Node()` can emit two different orders at
  two identical sites, and changing `Data.h` is a wide-ripple engine edit anyway.
* **"No source change can move it" — also REFUTED, by my own probe.** An
  artificial `int idx = i;` before the *first* site moved the row to **99.97921**
  by fixing the *second* site — a non-local register-allocation ripple. So the
  order **is** source-sensitive, just not through any principled lever.
* The same temp at the second site alone: **no change** (99.95842). Both temps
  together: **99.97921**, same as one. Hoisting `curArr`'s declaration out of the
  loop (matching the `artistArr` style): **no change** — consistent with
  CLAUDE.md's correction that declaration order controls *stack slots*, not
  registers.
* **Not landed.** 99.97921 still banks **zero** bytes (`matched_code` needs
  `fuzzy == 100`), the temp has no mechanism, and it is not plausible retail
  source. Landing it would be pure metric-fitting for +0 B. Classified
  permuter/regalloc-class and deferred per the standing directive.

**The oracle was the defect again, twice.** `../rb3`'s `SelectRandomVenue` is
*verbatim* our source: it uses the `Symbols*.h` globals and it writes the nested
`TheSongMgr` call. Both are wrong against retail bytes. That is now a thoroughly
repeated pattern — treat oracle text as a hypothesis, retail bytes as the ruler.

## Traps hit

1. `objdiff-cli`'s JSON omits `fuzzy_match_percent` when it is 0 — a bare
   `d['fuzzy_match_percent']` raises `KeyError`. Same protobuf-omission rule
   CLAUDE.md documents for `report.json`; it applies to the `diff` JSON too.
2. Batch dumps keyed on `echo "$sym" | md5sum` (which appends `\n`) did not match
   Python's `md5(sym)`. Every lookup missed and the first analysis reported a
   confident **"0 rows"** — a screen that cannot fire, caught only because 0 was
   implausible, not because anything errored.
