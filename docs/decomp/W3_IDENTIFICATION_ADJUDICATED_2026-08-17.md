# W3 (identification at scale) — adjudicated, 2026-08-17 (lane W3-IDENT)

> **STATUS: CURRENT.** Corrects `CAMPAIGN_STATE_2026-08-17.md` §7 W3, which
> describes Ghidra+BinDiff transfer as a *"standing plan, never executed
> (XEXLoaderWV needs a Ghidra 12.1 rebuild)"*. **Both halves are false.**

## 1. ⛔⛔ THE PIPELINE WAS EXECUTED THREE TIMES AND LANDED — AND THIS IS THE SECOND LANE BRIEFED OTHERWISE

| date | SHA | what landed |
|---|---|---|
| 2026-05-29 | `823eee16` / `05bdcc7d` | game target-map + Ghidra symbol-map tooling |
| **2026-07-20** | **`e5293a8c`** | **BinDiff r1 transfer: +286 map entries, +5 strict** |
| **2026-07-24** | **`aa86fb41`** | **BinDiff r2 anchored: +263 transfers, +7 strict** |
| **2026-07-24** | **`5ff856bc`** | **r2 repoints: 36 conflicts, 35 REPOINT / 1 KEEP, +9 strict** |
| 2026-07-24 | `bd0e2e3f` | 293 fragment-mispair entries deleted |
| **2026-08-03** | **`254e80bd`** | **DL-1: the calibrated KILL** |
| 2026-08-14 | `65154fb0` | IDENT-1: frontier priced at **~0.2% of `total_code`** |

≈549 map names landed. Measured precision: r1 **89.8%** @conf≥0.95 (**96.4%**
non-boilerplate); r2 anchored **98.6%** top band; per-library **94.5% engine /
84.4% hamgame**.

★★★ **WHY EVERYONE BELIEVES IT NEVER RAN: `tools/bindiff_match.json` IS
GITIGNORED** (`.gitignore:108`). It has *zero* git history, so a 3.7 MB file
sits on disk looking like un-acted-on scratch. The work it fed landed under
subjects saying `id(bindiff-r2)` / `map(mispair)`, which no search for
"bindiff" in a plan doc finds. **Lane DL-1's own commit message opens
`⛔ MY BRIEF WAS WRONG ON BOTH HALVES` — it was told the channel was
"unexamined" and found the tooling already in tree. Today's lane was given the
same brief.** A gitignored artifact is an *invisible* institutional memory.

## 2. ⛔ DL-1's CALIBRATED KILL — re-running BinDiff is CIRCULAR, not merely weak

Calibrated against 15,714 ground-truth named functions at `mpn==100`:

- top-1 over 56,893 DC3 candidates: **46.8%**
- **scoped to the correct DC3 `.obj`** (median 86 candidates): **80.7%**;
  sabotage leg (random wrong `.obj`): **0.0%** ✅ (the control could fail)
- decoy null (RB3 functions DC3 provably lacks): **p95 = 1.000** ⇒
  **"NO THRESHOLD EXISTS"** — ≥5% of functions DC3 does not contain still
  retrieve a DC3 body at similarity **1.000**.

★★★ **"THE BOTTLENECK IS LOCATION, NOT SCORING POWER (46.8 → 80.7 from scope
alone) — so RE-RUNNING BINDIFF CANNOT RESCUE THE UNPINNED CASE, because
obtaining a location prior IS WHAT PINNING A TU MEANS. The channel is circular
for exactly the population it was proposed for."**

⚠ Counter-intuitive keeper: **identical bodies recover *worse* than divergent
ones (40.7% vs 49.4%)** — identity here means boilerplate identical to dozens of
unrelated functions, so identity destroys uniqueness.
⚠ Strategic wall (r2 §5): DC3-BinDiff names only *shared engine* code —
**27/299 ≈ 9%** game yield. Game discovery belongs to the rb3-Wii oracle.
⛔ `tools/bindiff_match.json` is additionally **TU0-era and address-dead**: only
**3.13%** of its `rb3_addr` values are `.pdata` starts on TU5 (84.89% on the TU0
archive). Naively recalibrated it reads **0/238 = 0.00%** — a fabricated
decisive negative DL-1 caught and retracted.

## 3. Toolchain true state (verified, not inferred)

- **The service does not use `/opt/ghidra` at all.** `pyghidra-service.sh` sets
  `GHIDRA_INSTALL_DIR=../ghidra/build/ghidra` = the **VMX128 SLEIGH fork,
  Ghidra 12.2**, which **already has `XEXLoaderWV` installed** (v12.2) plus
  `ghidra-xbe`. ⇒ **CLAUDE.md's "XEXLoaderWV needs a rebuild for 12.1 /
  installed prebuilt is 12.0.1" is STALE.** `/opt/ghidra` is 12.1.2 with an
  **empty** `Extensions/`; the `ghidra_12.1_PUBLIC_…GhidraXenon.zip` in
  vmx128-research exists but is **not what is deployed**.
- **BinDiff** `/usr/bin/bindiff` works. ⚠ **BinExport is NOT installed into the
  fork Ghidra** (`Extensions/` holds only `ghidra-xbe`, `XEXLoaderWV`); it ships
  unpacked at `/opt/bindiff/extra/ghidra/BinExport`. A fresh export needs that
  wired first — the only real infrastructure gap, and per §2 it is **not worth
  closing**.
- A `pyghidra-mcp` process has held the project lock for **23 days**; the
  service script's own PID file is stale and reports "not running".

## 4. IDENT-1's controls REPRODUCE EXACTLY at HEAD

`tools/ident_body_channel.py --worktree <wt> --holdout`, run at HEAD:

| control | IDENT-1 (08-14) | this lane (08-17) |
|---|---|---|
| leave-one-out FP (true owner removed from supply) | 2.76% | **2.76%** (580/21,006) |
| same-unit removes … of FPs | 64.9% | **65.0%** |
| non-template ≥128 B | — | **0.79%** |
| template <128 B | — | **6.99%** |

**The entire body-identity-reachable surface at HEAD is 234 rows / 21,224 B =
0.206% of `total_code`** (`A_same_unit` 43 / 4,144 B + `B_other_unit` 191 /
17,080 B), reproducing IDENT-1's "~0.2%" to three digits. Against it,
**`no_body` is 36,669 rows / 3,480,708 B** — we hold no byte-identical body, so
this channel cannot reach them at any effort.

⇒ **W3 "identification at scale" is DRAINED. It is not blocked on tooling.**

## 5. The wave — 26 names, measured

Gate: tier `A_same_unit` ∧ non-template (tool's exact test `'?$' in name`) ∧ not
already mapped ∧ VA unoccupied ∧ **VA is a `.pdata` BeginAddress**.
Composition-weighted gated FP **0.94%** (expected **0.24** wrong of 26).
30 of 31 pre-`.pdata` rows were `band3/` game code — the priority layer, and the
residue IDENT-1 explicitly deferred for concurrency.

| measure | pre-registered | **measured** |
|---|---|---|
| Δmatched_functions | +20…+26 (central +24) | **+26** |
| Δmatched_code | +1,900…+2,372 B | **+1,976 B** |
| Δcode% | +0.019…+0.023 pp | **+0.019146 pp** |
| Δmasked_equal | ≈0 | **+0** ⇒ Δhonest **+26** |
| Δreachable ceiling | **exactly 0** | **0** (units already had base objs) |

`none`-ruler control: **REAL_PAIRING** (+2,372 B on `none` = exactly the wave's
byte total; +1,976 graded). Not the ALIAS_SUSPECT shape. `Δunits_at_100` +1.

★★★ **THE 396 B GAP IS BUG EXPOSURE, AND ONE INSTANCE IS PINNED.**
`default/HamBattleData`'s `??$?6VBattleStep@@…` (96 B) went **fuzzy 100.0 →
99.791664 with `mpn` unmoved at 100.0**. Naming an anonymous address converted a
**forgiven** placeholder call target into a **checked** one, and our source
spells a different callee. `mpn` excludes arg-only penalties, hence only fuzzy
moved. ⇒ **the documented "naming is a bet, paying in bug exposure not bytes"
mechanism, caught live** — a defect the metric was previously hiding.

## 6. ⚠ What this lane did NOT do

- **Did not publish a re-measured ceiling.** `PAIRABLE` reproduces GAPMAP-1
  **exactly** (6,488,248 B / **62.867%**, rows and bytes both summing exactly),
  but **two independent attempts at the map-scaffold correction produced
  estimator artifacts** — a regex-on-raw-obj count gave 264,516 B, and a
  mis-adapted `parse_coff` gave **0 B, a vacuous "no scaffolds" that would have
  INFLATED the ceiling.** GAPMAP-1's 180,196 B is inherited **with attribution**
  rather than replaced by a number this lane cannot stand behind.
- **Did not touch `scripts/symbol_aliases.json`** (an alias lifts `matched_code`
  by construction; the `none` control is flat for a fabricated one by
  construction).
- **Did not pin or attribute any `auto_*` row.** That is the only
  ceiling-*raising* route, and AUTOID-1 priced it at **8.9% attributable-and-
  portable**, dominated by 7-line Quazal scaffolds and absent XDK source.
- **Did not land 10 template rows** (FP 4.69–6.99%) or **5 sub-32 B non-`.pdata`
  rows**. ⚠ The `.pdata` exclusion is **conservative, not evidential**: per
  AUDIT-NC an 8-byte leaf touches neither stack nor LR and so *legitimately* has
  no unwind record — absence is expected in that stratum, not damning.
- **Did not re-run BinDiff/BinExport.** §2 rules it out on measurement.

## 7. ⛔ An instrument failure from this lane, worth reusing

The first wave assembly used `fn_(82[0-9a-f]{6})` against target names spelled
with **UPPERCASE** hex (`fn_82B9FBB8`). It silently dropped **29 of 36 rows** and
produced a "the wave is tiny, W3 really is drained" result **that agreed with the
lane's prior** — the hardest kind to catch. Caught only by asking why a
36→7 collapse had no stated cause. ⚠ Address **case** is a live hazard in this
map: the r2 lane recorded a collision on `0X82637888`. **Match VAs
case-insensitively on both sides.**
