# W16-FW — screening the inherited big-row candidate list BEFORE briefing it

**Date:** 2026-09-16 · **Base:** main `9b2486f8` · **Read-only.** No source, map or splits change.

The coordinator's pending list carried nine large partially-matched rows as "measured
candidates". Screening them killed or demoted **five**, and the two instruments that did it are
both cheap. This exists so the next lane is not briefed off the raw list.

## 1. The cheap screen, and its limit

`report.json` alone splits candidates with no build:

- **`mpn == 100` and `fuzzy < 100`** ⇒ every penalty is **argument-level** (relocation-name).
  There are no instruction diffs to close, so source work has nothing to bite on; the prize is
  collectable only by proving relocation names — a **map/alias** question.
- **`mpn < 100`** ⇒ real instruction-level divergence exists.

| row | size | fuzzy | mpn | verdict |
|---|---:|---:|---:|---|
| `?Handle@MusicLibrary@@` | 6,160 | 99.9740 | **100.0** | **ARG-ONLY — do not brief as source** |
| `?RecordAccomplishmentData@RockCentral@@` | 4,676 | 99.9572 | **100.0** | **ARG-ONLY — do not brief as source** |
| `?transform@MD5@Quazal@@` | 6,068 | 82.19 | 89.48 | **KILLED — `/Od`** (§2) |
| `?SetState@SaveLoadManager@@` | 4,096 | 97.32 | 97.79 | **DEMOTED — permuter-shaped** (§3) |
| `?Handle@CustomizePanel@@` | 5,036 | 99.92 | 99.92 | **CLOSED already** (§4) |
| `?Transform@CSHA1@@` | 5,856 | 55.69 | 61.99 | survives — real body divergence |
| `?Tessellate@RndAmbientOcclusion@@` | 4,796 | 64.15 | 66.34 | survives — real body divergence |
| `json_tokener_parse_ex` | 4,872 | 93.20 | 93.57 | survives |
| `?SyncProperty@Spotlight@@` | 4,728 | 99.39 | 99.83 | survives, thin |

⚠ **The screen is a SCREEN, not a verdict — `?Handle@CustomizePanel@@` proves it.** It reads
"real divergence" here and is a *known priced refusal*: closing its instruction diffs buys
`mpn` 100 and **zero bytes**, because `matched_code` is all-or-nothing per row and the 5,036 B
additionally needs two ICF fold-aliases proven (lane RESIDUAL-1). A two-key split cannot see
that structure. **The in-tree closure record is what catches it**, which is why it is read first.

## 2. `?transform@MD5@Quazal@@` — killed by measurement, not by suspicion

The list carried it as "likely `/Od`". Settled: the map places it at **`0x82b43cc8`**, inside the
measured `/Od` Quazal region **`0x82A6D168`–`0x82B54190`** (lane CF-4). Unmatchable at `/O1`.
**6,068 B correctly refused.** (Control: `?SetState@SaveLoadManager@@` `0x82550880` and
`?Transform@CSHA1@@` `0x824f31d0` are both outside it, so the test can come out the other way.)

## 3. `?SetState@SaveLoadManager@@` — the refutation was already in the file

⛔ `src/band3/meta_band/SaveLoadManager.cpp` carries, in-source:

> `// LINKAGE IS *NOT* THE LEVER HERE -- REFUTED, do not re-run (lane W16-CF).`

with the negative result attached: the residual clusters are our `kStrGlobalCacheName.Str()`
load sitting **above** the `bl Localize` at the three sites that pass both in one argument list
(cases `0x2b`, `0x2c`, `0x3b`); retail keeps it **below**. The hypothesis that internal linkage
licensed the hoist was **built and measured** — the TU recompiled, the mangled name really
changed, and codegen came back **bit-identical**, whole-binary Δ0. ⇒ the hoist is **scheduling**,
not alias analysis.

Priced independently here (`objdiff-cli diff`, no `--build`, on a verified fixed-point tree):

| class | count |
|---|---:|
| `diff_arg` | 88 |
| insert / delete | 11 / 11 |
| replace | 1 |
| **total instructions** | 1,035 |

`REGISTER_SWAP` **84 instructions across 9 pairs** (r25↔r26 = 30 of 84) · `OFFSET_SWAP` 2 ·
`ADDRESS_RELOCATION_NOISE` 8 (*RarelyHandFixable*, linker artifact). The insert/delete clusters
sit at idx **479-534 / 545-587 / 600-650** — i.e. inside the 516-648 range W16-CF already
attributed to the refuted `Localize` scheduling difference. Only two 1-insert/1-delete clusters
(idx **959-967**, **1020-1024**) lie outside it.

⇒ **Not a 4,096 B prize.** The collectable-by-source share is small, the large named residual is
refuted, and the dominant class is register swap — permuter-shaped, and the permuter is OFF by
standing directive. Do not brief it as a big-byte candidate. (The row has moved 96.74512 →
97.32227 since W16-CF, so *some* work landed after that note; the note's refutation stands.)

⚠ Worth keeping from the same file: the `LoadMemcardAction` note records **the rb3-Wii oracle
being the defect** — its `unk24`/`mProfiles` pair came from the oracle, was never referenced,
and mis-sized both allocations (retail passes `0x14`, and `MemcardAction` is exactly `0x14`).
Another instance of *oracle text is a hypothesis, not ground truth*.

## 4. What survives, and how to price it

`?Transform@CSHA1@@` (55.69) and `?Tessellate@RndAmbientOcclusion@@` (64.15) are the only two
with a **large** real body divergence — the all-or-nothing "body-divergence job" shape. They are
not screened further here; **price from the charged-site list before briefing either**, never
from the fuzzy% or a mismatch count, and grep **both** oracles first (`../rb3` and
`../dc3-decomp`) — a big row can have no oracle in either.

## 5. What this lane did NOT do

- No source, map or splits change; nothing measured by A/B because nothing was changed.
- Did not price `json_tokener_parse_ex`, `?SyncProperty@Spotlight@@`, CSHA1 or Tessellate.
- Did not adjudicate whether the two ARG-ONLY rows' relocation names are right — that is a map
  question and it was not opened.
