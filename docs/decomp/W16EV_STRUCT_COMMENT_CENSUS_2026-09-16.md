# W16-EV — the `// 0xHEX` struct-comment surface, swept and measured

**Lane W16-EV, 2026-09-16, worktree `~/tmp/wt-w16-ev`, branch `w16-ev` off `9e97cfdf`.**

Sweeps the game-layer header comment surface against `cl.exe
/d1reportSingleClassLayout`, which is authoritative about **our** layout. The
governing distinction, kept separate throughout:

- **Class 1 — comment wrong, layout right.** A documentation defect. Metric-
  neutral *by construction* (`--fix-header` writes comments, never code). Its
  value is that it stops misleading the next lane and un-poisons
  `struct_db.sqlite` / `lookup_struct_offset`. Real value, zero bytes.
- **Class 2 — our layout disagrees with RETAIL.** A real bug with metric
  consequences, requiring retail-byte evidence rather than a comment diff.

**Headline: the class-1 surface is essentially DRAINED, and there is no class-2
bug here.**

- **Remit (`band3`+`network`):** 3,325 comment rows over 369 headers; 3,012
  examined; **35 disagreements in 6 headers = 1.16% of examined rows**. 30 fixed
  at a measured Δ0; 5 deliberately left alone.
- **Tree-wide (§8), run after the instrument was validated:** 11,823 rows over
  1,042 headers; 10,681 examined; **113 disagreements in 24 headers = 1.06%**.
  A further 39 fixed at a measured Δ0; 74 deliberately left alone.

One class-2 *candidate* was found, adjudicated on retail bytes, measured, and
came back **Δ0** — reported in §5 as a negative with a handoff. A **tool defect**
producing a confident false clean is reported in §6.

**Both fix waves are metric-neutral by construction and measured so: Δmatched +0,
Δcode_bytes +0, Δcode% +0.000000pp, on cascades of 226 and 535 recompiled TUs.**

## 1. Method

Cost is per **TU**, not per class, so the CLI's one-compile-per-class path
(~14 s each) was not used. Instead `/d1reportAllClassLayout` was run **once per
TU** over all 385 compiled `band3`/`network` TUs (measured **3.1 s** and 3,070
classes for one TU), and `audit_header()` was driven as a library against the
resulting layouts.

**All 385 TUs returned `status=OK`** — zero `TIMEOUT`, zero `COMPILE_FAILED`,
zero unparseable. That matters: the tool's three-label contract exists because a
failed compile parses to an *empty* class list and is otherwise indistinguishable
from "class legitimately not here". No TU in this census contributed a false zero.

### 1.1 Coverage is INSTRUMENTED, not assumed

`audit_header()` returns `[]` both when a header is clean **and** when it audited
nothing (class body span not locatable, class not reported, member names not
matching). Those are not the same answer, and conflating them is exactly how a
census reports a confident false "all agree".

So every `(header, class)` pair is audited **twice**: once as-is, and once
against a copy of the header whose every `// 0xHEX` comment is rewritten to
`// 0xdeadbe`. Rows flagged on the perturbed copy are *precisely* the rows the
auditor actually examines. This makes coverage a measurement, and it doubles as
a **per-header discrimination control** — a header whose perturbed probe flags 0
rows is reported `UNAUDITED`, never "clean".

| | rows | headers |
|---|---:|---:|
| `// 0xHEX` comment rows in scope | 3,325 | 369 |
| **examined** (perturbed probe flags them) | **3,012 (90.6%)** | 317 |
| unexamined — class `NOT_REPORTED` (57) or `NO_ROWS` (11) | 186 | 52 |
| unexamined — inside a partially-covered header | 127 | 45 |

The 52 unaudited headers are uninstantiated templates (`qChain`, `iterator`),
DDL scaffolds (`_DDL_AccountData`, `_DS_ConnectionInfo`) and Wii-only classes
(`WiiEntityUploader`) that the compiler never completes. **They are unaudited,
not clean**, and this lane makes no claim about them.

## 2. Self-validation — the census reproduces known answers

A census that has not reproduced a known figure is not yet an instrument.

| known answer | result |
|---|---|
| `src/band3/meta_band/SaveLoadManager.h` (docs claimed "uniformly +4 stale") | **`AUDITED`, coverage 26/26 rows, 0 disagreements — CLEAN** |
| `src/system/char/CharEyes.h` (docs claimed "20 wrong offsets") | **"all `// 0xHEX` comments agree with the compiler"** |

Both are clean, confirming the brief: they were repaired and the docs were never
updated. Note the coverage figure on SaveLoadManager — 26 of 26 rows examined —
is what makes "clean" a real answer rather than a vacuous one.

### 2.1 The instrument can FAIL (proved, not assumed)

A gate that cannot fail is worse than no gate. Line 143 of `SaveLoadManager.h`
was corrupted `// 0x1c` → `// 0x44` and the tool was re-run:

```
WRONG line 143: mMode commented 0x44 but is really 0x1c
```

Exactly the sabotaged row, correctly named, with no collateral false positives.
The header was restored (`git status` clean) before any further work.

## 3. CLASS 1 — comment wrong, layout right (35 rows, 6 headers)

Every disagreement is a **positive, monotonically non-decreasing shift**. None is
a random transcription error; the shape is "bytes were inserted ahead of these
members and the comments were never renumbered".

The discriminator used to separate class 1 from class 2 is cheap and principled:
**a unit with many functions already at 100% has a retail-correct layout**,
because wrong member offsets would break those matches.

| header | rows | unit match state | verdict |
|---|---:|---|---|
| `meta_band/MetaPerformer.h` | 16 | 306/351 fns, 74.7% | class 1 — fixed |
| `game/BandUserMgr.h` | 5 | 80/89 fns, 78.1% | class 1 — fixed |
| `tour/TourProgress.h` | 5 | 86/95 fns, 58.1% | class 1 — fixed |
| `game/GemPlayer.h` (`GemStatus`) | 2 | 305/347 fns, 74.2% | class 1 — fixed |
| `net_band/DataResults.h` (`DataResult`) | 2 | 46/47 fns, **99.9%** | class 1 — fixed |
| `network/Core/Scheduler.h` | 5 | **0/26 fns, 0.0%** | ⚠ UNRESOLVED — **not** fixed |

`DataResults` settles the principle: the class declares `virtual ~DataResult()`,
so a vfptr owns 0x0 and `mUrl` cannot be at 0x0 as commented — the classic
"numbered the members from zero and forgot the vfptr" error. At 46/47 functions
and 99.9% of code matched, if `mUrl` really were at 0x0 in retail then
essentially every accessor in the unit would mismatch. The comment is wrong; the
layout is right.

### 3.1 ⛔ Scheduler.h was deliberately NOT fixed — a new guard rule

`--fix-header` rewrites comments to describe **our** layout. Where our layout is
validated by nothing, that **destroys RE-derived retail documentation and
replaces it with our guess.**

`src/network/Core/Scheduler.h` is that case: the unit is **0/26 functions and
0.0% of code matched** (`Scheduler.cpp` is a 31-line scaffold), so no match
evidence constrains our layout — while the comments *and the member names*
(`unk4`, `unk8`, `unk34`) independently encode the same RE-derived retail
offsets. Two witnesses agree against our compiler.

The cause of our +4 is visible in the raw report: `{vfptr}` at 0, then an
**empty `RootObject` base occupying 0x4–0x8** which MSVC did not elide, pushing
`unk4` to 0x8.

Rewriting these would leave `int unk4; // 0x8` — internally incoherent — and
would erase the only retail record we hold for this class.

> **Guard rule this sweep discovered: `--fix-header` is only safe where our
> layout is independently validated (a substantially-matched unit). On a 0%-
> matched unit it is evidence destruction, not documentation repair.**

## 4. Measured: the class-1 fix is Δ0, and the Δ0 is EARNED

Pre-registered in `docs/decomp/W16EV_PREREGISTRATION.md` (committed before any
edit): Δ exactly 0 on every measure; leg B recompiles > 0.

30 rows across 5 headers, verified comments-only (30 `-` lines, 30 `+` lines,
**0 lines whose non-comment content changed**). `tools/ab_measure.py
--from-dirty`, both legs settled:

```
leg A: matched=43970 masked=23224 honest=20746 code%=40.305344  (recompiles 0, settled)
leg B: matched=43970 masked=23224 honest=20746 code%=40.305344  (recompiles 226)
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.000000pp     units at 100% [mpn]: 189 -> 189    [fuzzy]: 169 -> 169
```

**Leg B recompiled 226 TUs**, so this is a real measurement and not
absent-vs-absent. Prediction met exactly on all three counts. Re-running the
census after the fix: **35 → 5 disagreements**, the 5 being Scheduler's, with
coverage unchanged at 3,012 rows.

## 5. CLASS 2 — one candidate, adjudicated and measured: NEGATIVE

`MetaPerformer` has **two different layouts in one program**. `/DRB3_NO_WII_META_MEMBERS`
is applied to **exactly one TU** (`config/45410914/objects.json`); its own TU
sees 50 members / size 940 with the `Object` vbase at 0x380, while **86 other
TUs** see 53 members / size 952 (`mWiiPending`, `mLastVenue`, `mVenueOverride`)
with the vbase at 0x38c.

The retail evidence is already recorded in the header (lane CO-1/METAPERF,
2026-08-02) and is of exactly the standard this lane demands: retail's ctor
stores nothing at 0x380; the target `.s` has 3 accesses to 0x37c and **zero** to
0x380; `band.exe` contains 0 occurrences of `venue_override` strings against
live positive controls. **Retail-correct therefore means the members are absent
in every TU**, which our build does not do.

No file in `src/` references any of the three members, so making the guards
unconditionally false is safe. Measured:

```
leg B recompiles 104 — Δmatched=+0  Δcode_bytes=+0  Δcode%=+0.000000pp
Δfuzzy=+0.000000pp   units at 100%: 189 -> 189
```

**Δ0.** The extra members sit at the *tail*, so ordinary member accesses in the
other 86 TUs are unaffected; only `Object`-vbase adjustors and vtordisp constants
shift, and no currently-matched code in those TUs does that.

**The probe was reverted.** It remains a genuine ODR inconsistency and a latent
accuracy defect, but there is no evidence to justify churning a deliberate,
documented, retail-adjudicated configuration on a Δ0.

> **Handoff:** if a future lane matches code in those 86 TUs that dispatches
> virtually on `MetaPerformer` or takes its `Object` sub-object, re-measure this.
> The fix is a 2-line guard change in `MetaPerformer.h` and costs 104 recompiles.

## 6. ⛔ Tool defect found: `--check-header` can audit the WRONG header and report a FALSE CLEAN

Running `--fix-header MetaPerformer` printed:

```
=== header comment audit: src/meta_ham/MetaPerformer.h ===
  all // 0xHEX comments agree with the compiler
```

It audited **`src/meta_ham/MetaPerformer.h`** — not the live
`src/band3/meta_band/MetaPerformer.h` whose 16 rows were wrong. `main()` consults
`find_header(...)[0]` only; **two headers declare `class MetaPerformer`**, both
stems match the class name, so the sort falls through to shortest path and the
near-dead `meta_ham` copy (absent from `objects.json`, included only by
`src/meta_ham/MetagameStats.h`) wins.

This is the same family as the cross-class and shadowed-base-member bugs the tool
already carries regression pins for, in **cross-file** form, and it is the
dangerous direction: a **confident false clean**. Had the foreign header's
comments happened to disagree with the audited class's layout, `--fix-header`
would have rewritten *that* file's correct comments to another class's offsets.

The 16 MetaPerformer rows in this lane were therefore applied against the
explicitly-named correct header, not via `find_header`.

**Not fixed by this lane** (it is shared tooling and a fix wants its own
regression pin). Recommended: have `--check-header`/`--fix-header` refuse — or at
minimum warn — when `find_header` returns more than one candidate, and prefer the
header the resolved TU actually `#include`s.

## 7. Cross-TU layout conflicts — 39 keys, mostly artifacts

The census flagged 39 classes whose layout differs between TUs. Do **not** read
that as 39 ODR bugs; it conflates three things, and MSVC's layout report prints
class names **without namespace qualification**, which is the main source:

- **name collisions** — `Node`, `iterator`, `Data`, `Message`, `Buffer`, `Job`,
  `Stream` are different classes in different namespaces sharing a bare name.
- **parser artifacts** — `TourProgress` reports 2 "layouts" with identical sizes
  (296) and **zero** common-name offset differences; the only delta is a phantom
  member named `pragma`, i.e. a line the parser misread as a member.
- **genuinely per-TU-variable layouts** — `MetaPerformer` (§5), and `DataResult`,
  whose size is 40 vs 44 with *identical* member offsets: the tail is 0x18 vs
  **0x1c**, i.e. the live `/DRB3_MAP_0x1C` define.

Because `DataResult`'s member offsets are identical under both, its 2 flagged
rows are robust; likewise MetaPerformer's 16 rows all sit below 0x380 and were
verified to flag identically against **both** of its layouts before fixing.

## 8. Extension — the same sweep, run TREE-WIDE (beyond the remit)

The remit was `band3`+`network`. Once the instrument was validated it was cheap
to finish the job, and the engine headers are what feed `struct_db.sqlite` and
`lookup_struct_offset` that every lane reads. All **1,184** compiled TUs were
reported (`status=OK`, zero failures, 12 min wall).

| | rows | headers |
|---|---:|---:|
| `// 0xHEX` rows tree-wide | 11,823 | 1,042 |
| **examined** | **10,681 (90.3%)** | 953 |
| unexamined | 1,142 | 89 unaudited |
| **disagreeing** | **113 (1.06% of examined)** | 24 |

**The class-1 surface is drained tree-wide, not just in the game layer.** Of the
113, only 5 are in the remit (Scheduler); 108 are engine headers.

39 rows across 10 headers were fixed — all in units whose matches validate our
layout (`HiResScreen` 8, `FileMerger` 9, `rnddx9/Tex` 6, `BandCamShot` 4,
`HttpReq` 3, `BandPatchMesh` 3, `DataFile` 2, `DataPointMgr` 2, `Interp` 1,
`MiniLeaderboardDisplay` 1). Measured A/B: **Δmatched +0, Δcode_bytes +0,
Δcode% +0.000000pp, units 189 → 189, leg B recompiled 535 TUs** — the largest
cascade of the three legs, still exactly zero.

**74 rows deliberately left disagreeing:**

- **52 rows / 11 headers** where the declaring unit is **<45% code-matched or
  absent from `report.json`** — `TexProc` (0/1 fns), `SpeechMgr` (10.6%),
  `Shockwave` (14.0%), `HamCamTransform` (17.3%), `CheatProvider` (20.4%),
  `HAQManager` (2.0%), `Scheduler` (0/26), plus `WebSvcReq`,
  `HamScrollSpeedIndicator`, `HamCamShot`, `Stats_NG` with no unit at all. This
  is §3.1's guard rule applied at scale.
- **22 rows / 3 headers** — `MeterDisplay` (11), `CharHair`'s `Point` (10),
  `Msg`'s `Message` (1) — where the class has **more than one layout across
  TUs**, so any fix bakes in one arbitrary view. These were **refused
  automatically** by a robustness check that re-audits each row against *every*
  TU's view of the class and declines unless all agree. `Point` and `Message`
  are almost certainly bare-name collisions across namespaces (§7).

⚠ Limitation, stated: unit match% is a **proxy**. A header's members may be
accessed from units other than its same-stem one, so a well-matched declaring
unit is evidence, not proof, that every commented offset is retail-correct. The
threshold is deliberately conservative in the safe direction.

## 9. What I did NOT do

- **Did not fix `Scheduler.h`** (§3.1) — 5 rows left disagreeing, deliberately.
- **Did not fix the tool defect in §6** — shared tooling; wants its own pin.
- **Did not land the MetaPerformer layout change** (§5) — measured Δ0, reverted.
- **Did not audit 313 rows / 52 headers** in the remit (§1.1), or 1,142 rows /
  89 headers tree-wide (§8) — uninstantiated templates, DDL scaffolds, Wii-only
  classes. Unaudited, not clean.
- **Did not fix 74 tree-wide rows** (§8) — unvalidated layouts and
  multiple-layout classes.
- **Did not adjudicate the 39 conflicts individually** — only the 3 that bore on
  a flagged row.
- **Did not touch `main`**, and did not push.
- The brief's population figure of **416 headers did not reproduce**: the
  measured count of `band3`+`network` headers carrying `// 0xHEX` is **369**
  (512 headers exist; 384 contain any `0x`). 421 is the count of files of *any*
  extension carrying such a comment, which is the likeliest origin of 416.
