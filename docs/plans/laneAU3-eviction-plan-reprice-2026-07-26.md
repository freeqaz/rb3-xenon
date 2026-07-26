# Lane AU-3 — honest re-price of the laneH eviction plan (2026-07-26)

**Verdict: the −24 is SUPERSEDED, not confirmed. The plan is already ~fully
consumed. Its 63 moves are a provable no-op today; its 34 removals now cost −25
strict, and every one of those 25 is collateral damage on *other lanes'*
since-landed identities, not the integrity trade the plan described.**

Branch `laneAU-3`, merge-base `24f508fb` (37,282 strict). Nothing landed on
main — this is a measurement record only. **The apply decision is the user's.**

Subject: `docs/plans/laneH-eviction-plan-2026-07-25.json`
(63 moves / 34 removals / 34 evicted holders), priced at plan time by the span
predictor at **GAIN 5 / LOSS 27 = net −22** and held as a human decision at a
−24 headline ever since — see
`docs/plans/laneH-map-rotation-repair-2026-07-25.md` §6.

Machine-readable record: `docs/plans/laneAU3-eviction-plan-reprice-2026-07-26.json`.

## 1. Reconciliation first — subtract overlap before claiming any delta

Plan compared against the map at its own commit (`d854a39a`) and at `24f508fb`,
raw-line parsed (never `json.load`, which silently keeps the last of a duplicate
key).

| plan half | consumed | live | contradicted |
|---|---|---|---|
| **63 moves** (`set`) | **63** | **0** | **0** |
| **34 removals** (`remove`) | 7 (VA already absent) | **2** | **25** |

| 34 evicted holders | count |
|---|---|
| gone from the map entirely | **29** |
| present but relocated elsewhere | 5 |
| still sitting on the plan's destination VA | **0** |

So the plan is **consumed, not stalled**. Every destination VA already holds the
name the plan wanted there. `?StaticClassName@Object@Hmx@@` is on `0x82271a90`;
`HamMove` — the Dance Central class RB3 does not have, the doc's own poster
child for a false pairing — is gone. Later lanes (`lanePHANTOM`, `laneAO`,
`laneAQ`, `laneAP`, `laneRUNCARVE`, `laneAR`) reached the same answers
independently and landed them.

**The 25 "contradicted" removals are the trap.** Those VAs no longer hold the
name the plan expected to vacate; a different, independently-adjudicated name
landed on them. Removing them today deletes *that* work, not the plan's target.

## 2. Prices — whole-binary `report.json` A/B, every leg built twice

Protocol per leg: `git checkout -- config/45410914/symbols.txt` →
`rm -f build/45410914/{report.cache,target_symbol_renames.stamp}` →
`touch config/45410914/config.yml` → `NINJA_JOBS=8 ./tools/fresh_report.sh`.
Baseline read **37,282 twice**; every leg reproduced its number exactly.

| subset | map lines | strict (unit,name) | strict (**name-only**) | `matched_code` | pct |
|---|---|---|---|---|---|
| **moves** — all 63 `set` | **0** | **0** | **0** | **0** | **0** |
| **evictions, still-live** — 2 | −2 | **−2** | **0** | −176 B | −0.00167pp |
| evictions, contradicted — 25 | −25 | −23 | −19 | −1,976 B | −0.01868pp |
| **evictions, as written** — 27 of 34 applicable | −27 | **−25** | −19 | −2,152 B | −0.02034pp |

Additivity is exact: `−2 + −23 = −25`, `−176 B + −1,976 B = −2,152 B`.

**The moves subset needed no build.** Running the plan's 63 `set` operations
through `map_rotation_repair.py apply` produces a **byte-identical map file**.
That is stronger than a measured 0 — it is a proof.

**The still-live remainder is free in real terms.** Its −2 headline is entirely
**duplicate-name double-counting**: `?StaticClassName@CharEyes@@` and
`?Type@InviteAcceptedMsg@@` are each bound to two VAs and score at both.
Unit-agnostically, **no name loses strict status** — hence −2 by `(unit,name)`
and **0** by name. (4 of the 25 contradicted removals are the same shape:
−23 by `(unit,name)`, −19 by name.)

### Controls
* `symbols.txt` checked out per leg, never committed; renamer stamp deleted per
  leg (a map-only edit is exactly the case where a stale stamp silently no-ops).
* Map invariants held on every leg: **0 duplicate VAs**, `_bijection_arbitrary`
  **1207**, `_icf_arbitrary` **25**, `_denylist` **3**.
* **Stale-obj phantom control: PASS.** 0 unexplained losses, 0 named gains, 0
  common-key improvements in any leg. Every lost symbol maps 1:1 to a map line
  I removed; the only new entries are the anonymous `fn_<VA>` replacements.
  No gain in any unit the edit did not touch.

## 3. ★ `pair_funclets_by_bytes` does NOT recover an evicted ordinary function

The premise this re-price was handed — that objdiff's byte-signature fallback
re-pairs the now-anonymous `fn_8XXXXXXX` and recovers the fuzzy — is **refuted**,
by code and by measurement.

**Code** (`/home/free/code/milohax/objdiff/objdiff-core/src/diff/mod.rs`):
`diff_objs(target, base, …)` ⇒ **left = target, right = base**
(`objdiff-cli/src/cmd/report.rs:1777`). `pair_funclets_by_bytes` (L1410) gates
*both* candidate loops on `is_funclet_like` (L1423 for left, L1438 for right).
`is_funclet_like` (L815) accepts `__unwind$N`, `__catch$N`, `__unwind__merged_*`,
`fn_<8 hex>`, `??__E*`, `??__F*`.

An evicted target reverts to `fn_<8 hex>` and **passes on the left**. But our
compiled base object defines the ordinary mangled name —
`?StaticClassName@FxSendChorus@@SA?AVSymbol@@XZ` — which **fails the predicate on
the right**. It is never collected as a right candidate, so passes 1, 2, 2b and 3
are all unreachable. (The published docstring corrections in
`scripts/harvest/splits_move.py` are accurate — pass 1 is uniqueness-gated, pass
2 zips ambiguous exact-signature groups greedily in name-sorted order — but they
describe passes that this case never reaches.)

**Measurement:** all 27 removals produced a replacement `fn_<VA>` at exactly
**0.000%**, never a partial recovery, and `matched_code` fell by the full body
size. Examples: `?Type@InviteAcceptedMsg@@` → `fn_8240DCB8` @ 0%;
`?StaticClassName@FxSendChorus@@` → `fn_826FC268` @ 0%.

**Therefore lane AR's "+0 strict, −0.0007pp fuzzy for 5 retirements" was not
recovery — those 5 bindings were already scoring ~nothing.** The correct
doctrine is narrower than the one in circulation:

> Retiring a **non-scoring** wrong binding is free. Retiring a **scoring**
> binding costs its entire contribution, with no fallback. Check what the entry
> currently scores before pricing its retirement.

## 4. Other things checked and refuted

* **ICF-twin ordering.** 0 swap pairs among the 25 contradictions — none is the
  "two lanes assigned twins in swapped order, both gained" shape. They are
  genuine third-party assignments.
* **Lane AE artifact overlap** (`docs/plans/lane-ae-unemitted-symbols.md`,
  read-only from main): **essentially none.** 0 of the 34 evicted holders are
  `lbl_*` / `merged_*` / `__MERGED_*` / foreign-namespace; exactly 1 is
  DC3-lineage `Ham*` (`HamMove`), and it is **already gone**. That handoff
  supplies no independent corroboration here.
* **My own reading, corrected.** The still-live remainder's −2 headline is not a
  real identity loss; the name-only A/B shows 0. Had I stopped at the
  `(unit,name)` number I would have over-priced it.

## 5. Recommendation — here is the price; the decision is the user's

* **Do not run the documented apply command.** As written it removes 27 entries
  for **−25 strict / −2,152 code bytes**, buying **zero** integrity: the false
  holders it targets are already retired, so the whole cost falls on other
  lanes' since-landed identities.
* **The moves half is free and already done.** No action; no further tracking.
* **The 2 still-live removals are genuinely cheap** — −2 headline, **0 real
  identities**, and they remove real duplicate-name double-counting. This is the
  only honest integrity gain left in the plan, and it is a hygiene call, not a
  score call.
* **Retire the plan file** as consumed, and retire the −24 with it. The number
  did not move because the plan got cheaper; it moved because **other lanes paid
  the gain half and dissolved the loss half.**

Corollary for the map channel: an eviction plan has a **shelf life**. Its
`remove` list is stated as bare VAs, which silently re-targets whatever lands on
those VAs later. Future plans should record the *expected holder* alongside each
VA so a stale entry fails loudly instead of deleting a stranger's work.
