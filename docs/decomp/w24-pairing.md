# W24-PAIRING — the dominant channel stops being a bound

**2026-08-17.** Baseline verified in-worktree before any work, reproducing the
brief on the shipped `name_check` ruler: **44,503 fns / 3,756,568 B /
36.398514%**, honest **21,593**, `total_code` **10,320,664**, `total_functions`
**69,226**. (The brief said 36.398511%; measured 36.398514%. Last-digit
transcription only — every load-bearing key is identical.)

**Shipped: `tools/pairing_model.py` — a two-ruler pairing model validated to
bracket width 0 against three independently measured numbers, plus two priced
refusals. Δmetric 0 by design; no map or splits edit was landed.**

> ★ **SUPERSEDES LEAD 1 of `docs/decomp/open-leads-2026-08-17-round-nine.md`.**
> That file records this lane's mid-outage hypothesis as UNVERIFIED with its
> stated test. **The test was run and it HOLDS** — see "The ruler split" below.
> The lead can be struck.

---

## The mechanism, in one sentence

objdiff pairs target↔base **by name** within a unit, so if the base obj behind
the pin cannot **define** the name the target obj carries, the row reads 0%
however correct our source is. W9 measured that as a −180 B edge case; W20
measured it as **80.5% of a map edit's delta** and could only *bound* it.

## The model

For an edited address A (old name O → new N), pinned in unit U with base obj B,
current row (size S, fuzzy F, mpn M):

| rule | condition | effect |
|---|---|---|
| **R1** | B cannot define N (or N is a placeholder) | row **un-pairs**. `bytes −= S` **only if F == 100**; `fns −= 1` **only if M == 100** |
| **R2** | B can define N | row **re-pairs** against a different COMDAT |
| **R2a** | adjudicate R2 by relocation-normalized COMDAT comparison | see below |
| R3 | callers | a *separate* mechanism — `tools/cascade_price.py` |

★ **The refinement W20's raw bound missed is the credit test in R1.** Un-pairing
a row that is not currently credited costs **zero bytes** — it costs a
*function*. Because `mpn` excludes relocation-name penalties, "loses bytes" and
"loses a function" come apart routinely, so the two must be modelled on their
own rulers.

### R2a — the body is available statically

W20 wrote that whether a re-pairing row reaches `fuzzy 100` "depends on the
BODY, which cannot be diffed until the edit lands and the tree re-splits."
**That is false.** Compare the two COMDATs in *our own* obj with every
relocation-patched word **masked** (raw `memcmp` is vacuous — PC-relative
displacements differ at different addresses):

| bodies | reloc targets | verdict |
|---|---|---|
| differ | — | genuinely different code → stays **BOUNDED** |
| equal | equal | free on every ruler |
| equal | **differ** | free under `none`; **charged** under `name_check` ⇒ row drops off `fuzzy` 100 (cost = size) but **keeps `mpn` 100** |

Measured on the fixture's one re-pairing row: SongSortMgr's two `clear` COMDATs
are **80 B, 1 relocation, reloc-normalized IDENTICAL**, differing only in
`_M_erase<…int>` vs `_M_erase<…SetlistRecord>`. That third cell *is* the entire
80 B by which the two rulers' answers differ.

---

## ★★★ The ruler split — W20 read two different quantities off one number

W20 reported `none` **−2,520** *and* quoted **2,520** as its bound on the
pairing channel. Those are different quantities that happened to coincide.

Under `none`, relocation-name charges vanish ⇒ the cascade (−580, **all**
reloc-name) contributes **exactly 0**, so **`none` measures the pairing channel
alone.** Two things decide an un-pairing row's cost and **only one is
ruler-dependent**:

* the **un-pairing verdict** — does the obj define the name? A COFF fact,
  **ruler-invariant**. `none` forgives relocation *names*; it does **not**
  forgive an **absent base symbol**.
* the **credit test** — is the row credited *today*? **Ruler-dependent.**

**The stated test, run:** `0x82456190` reads graded **99.70588** and `none`
**100.0**. So under `none` its 204 B **is** credited and un-pairing costs them;
under graded it costs a **function and no bytes**.

⚠ **This is a PAIRING change, not name forgiveness**, and the distinction is
load-bearing. The standing caution that "`none` flat is what a fabricated alias
looks like" is correct and **does not apply here**: `none` is *not* flat, it
moves **−2,520**. A fabricated alias only ever moves relocation-name
*arguments*, so it leaves `none` at **0** by construction. Flatness is the alias
signature; **movement** is the pairing signature.

## The known-answer test — three measured numbers, zero free parameters

The tree at this branch's base is in the post-W17 map state, so W20's round trip
is the *static* inverse of `7e9c2d01`'s 11 frozen edits — **no build required**.

| | predicted | W20 measured | bracket |
|---|---:|---:|---:|
| ruler `graded` (`name_check`) | **−2,396** | **−2,396** | **0** |
| ruler `none` | **−2,520** | **−2,520** | **0** |
| Δ`matched_functions` (ruler-invariant) | **−10** | **−10** | exact |

It also **explains W20's residual** rather than merely beating it.
`validate --self-break` drops the credit test and reproduces **−2,520** on the
graded leg — W20's own bound, to the byte. So W20 was loose by `204 − 80 = 124 B`:
it over-counted one uncredited row (`0x82456190`) and under-counted one
re-pairing row.

★ **The two-ruler fixture is strictly stronger than either leg.** Under
`--self-break` the graded leg FAILS but the `none` leg still **PASSES** at
−2,520, because the row the sabotage mis-prices is credited under `none` anyway.
**A `none`-only fixture would have green-lit a broken model.**

⚠ **PROVENANCE, and it moved under this lane within hours.** The validation is
bound to base `cc38cc43`. On `main` (28 commits later) **W17's map edit has been
reverted at 10 of 11 fixture addresses**, so the direction detector reads
`at_new = 0/11` and **REFUSES (exit 2)** rather than scoring a state it cannot
interpret. That is the guard working, not a defect — but re-validating on `main`
requires regenerating the fixture for the now-live direction. Same lesson as the
ceiling: **state-bound results must be re-measured, never inherited.**

---

## The inverse question — measured, and REFUSED

*If rows un-pair when the obj cannot define a name, are there rows already at 0%
that would **pair** if re-homed?* The risk profile looked attractive: such a row
scores zero today, so re-homing cannot lose bytes on that row.

**Re-measured, not inherited** (CLAUDE.md carried 267 rows / 38,096 B, of which
59 / 2,972 B re-homable):

| class | rows | bytes | share |
|---|---:|---:|---:|
| ORPHAN PINS (obj cannot define the pinned name) | **280** | **39,372** | — |
| — no destination anywhere → needs **SOURCE** | 216 | 35,900 | **91.2%** |
| — re-homable, any destination | **64** | **3,472** | 8.8% |
| — — unique destination | 15 | 812 | |
| — — ≥1 **owned** definition | 8 | 160 | |
| — — unique **and** owned | 3 | 64 | |

⚠ **This is a CENSUS, an upper bound on the candidate population. It was never
A/B'd, and no re-home was measured — so it licenses no claim about yield.** The
raw headline roughly reproduces (64 / 3,472 vs the inherited 59 / 2,972); the
ceiling is **0.034% of `total_code`**. The refusal rests on structure, not on a
measured delta:

* ⛔ **Every re-homable row above 64 B is a shared template instantiation or a
  `??_G` deleting-destructor thunk, `owned` in ZERO objs** —
  `ObjectDir::Find<RndGroup>` has **39** candidate homes. This is structural,
  not incidental: *a symbol only one TU can define is defined by the obj that
  owns it, so if that obj does not define it, no obj does.* Choosing one home
  among N identical `COMDAT ANY` copies is picking an arbitrary attribution to
  raise a score — the ICF-survivor-name trap one level up.
* ⛔ **The 8 rows with an owned destination are source-emission failures, and
  their "destination" is a FALSE ATTRIBUTION**: `Movie::LockThread` /
  `NumFrames` / `MsPerFrame` are pinned to `Splash` and definable only by
  `StringTable.obj`. Re-homing asserts that retail's `Movie::LockThread` lives
  in the StringTable TU. Out of bounds under the standing accuracy directive.

The 91.2% no-destination class is **real unimplemented code**, not a pinning
problem — 27,840 B of it ordinary member/free functions: `fft_altivec` (3,044),
`fft_recursive` (2,128), `DxRnd::DrawRect` (1,512), `PeakDetector::Detect`,
`GranularSynth::*`, `_alloc_osfhnd`.

## ★ The census self-validation FAILED first, and fixing it found a mechanism

One orphan read `fuzzy 100.0` — impossible if pairing were strictly exact-name.
The shipped `tools/orphan_pin_census.py` reproduces **the same single
violation** (both readers agree exactly: 282 rows / 39,412 B), so it was never
my reader.

**Cause: anonymous-namespace hashes (`?A0x<hash>`, which MSVC derives from
machine name + source path) are NORMALIZED AWAY before pairing.** Measured
scope: **21 of 282** orphans carry such a hash, **2** normalize onto a base
definition, and **exactly 1 of those 2 pairs** — 2 target symbols against 1 base
definition, so the other loses an **over-subscription**. `masked_equal` is *not*
set on it, so this is a **third pairing channel**, distinct from byte-signature
pairing.

⇒ `can_define` is exact for ordinary names and can return a **false BLOCKED**
for `?A0x` names. Booked as a characterized class; self-validation now passes
**0 violations** rather than shipping a known FAIL.

---

## Shipped, and the gate it implies

`pairing_model.py whereis <name>` answers the question that decides a rename:
*which objs can define this?* If a name is definable only by unit V while the
row is pinned to U, an in-place rename sends the row to **0% permanently** and
the remedy is a **re-home**.

It immediately resolves **W9's Tier B refusal**, which was blocked purely on
pairing: the three `ByteCode` names it could not ship **are** definable —
`LockResponseMsg` and `EndLockMsg` **uniquely in `LockStepMgr.obj`**,
`SyncAllMsg` in `SyncStore` + `NetSession`. That is a fully specified next lane:
re-home the three addresses, then rename. Predicted ≈ **Δ0 bytes** (180 B of
false credit moves to the right unit) with **no cascade** — W9 measured zero
direct `bl` sites, dispatch being virtual through vtable slot 6. It is an
**accuracy** play, not a byte play, and it needs three three-way carves.

## Deliberately NOT done

* **No map or splits edit landed, and no A/B run.** Nothing shipped changes the
  metric, so there was nothing to measure. Both open levers priced to a refusal
  or to ≈Δ0, and W20's precedent — ship the tool, refuse the bet — applies.
* **The re-home lever was NOT measured**, only censused. The refusal is argued
  from structure and attribution, and is stated as such above.
* **W9's Tier B was not executed.** The three addresses were never located (that
  needs W9's `ByteCode` RTTI/string-literal census re-run); only the
  *destinations* are established here.
* **The fixture was not regenerated for `main`'s reverted state**, so `validate`
  refuses there. A follow-up should regenerate it for the live direction.
* **The DATA reference graph is still uncovered**, inherited unchanged from W20:
  a vtable slot or function-pointer table is not a `bl` and is not enumerated.
* **No `src/**` touched**, so no native gate run was required.
