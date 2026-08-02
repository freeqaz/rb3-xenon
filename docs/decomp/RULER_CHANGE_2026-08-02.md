# ⚠ RULER CHANGE — 2026-08-02: `masked_equal_functions` now discloses ALL funclet pairings

**If you are reading an "honest" figure from before 2026-08-02, it is stale by ~21,500.**

## What changed

The local `../objdiff` fork was flipped (source `f74dce1` + `6ee1098` on top of
`3a024eb`). `masked_pairing` had always set `masked_equal_symbol` on **every**
funclet byte-signature pairing (`objdiff-core/src/diff/mod.rs:312-314`), but
`objdiff-cli/src/cmd/report.rs:740-754` derived the reported
`masked_equal_functions` from **pass-2b over-subscription only**.

| measure | before | after |
|---|---|---|
| `masked_equal_functions` | 1,096 | **22,640** |
| honest (`matched − masked_equal`) | 42,358 | **20,814** |
| disclosure share of `matched_functions` | 2.52% | **52.10%** |

## What did NOT change — verified, 11/11 keys identical

`total_code`, `matched_code`, `total_functions`, `matched_functions`,
`matched_code_percent`, `matched_functions_percent`, `fuzzy_match_percent`,
`total_data`, `matched_data`, `total_units`, `matched_units`.

**This is a disclosure change, not a scoring change.** Any movement in a score
key would have been a bug in the patch, and the flip gate checks exactly that.

## Why

The credit is **supply-backed** — our compiler really did emit those bytes — but
**per-row attribution is arbitrary within a signature group**, and relocation
targets are masked *both* in the signature and in the score. Standing project
rule: *a metric that hides real bugs is worse than a lower metric.*

Corroborated independently twice before the change: lane CX-4 read it out of the
objdiff source; lane CY-2 measured a 732-funclet phantom credit in the
storage-class family by a completely unrelated route.

## The hazard this created, and the guard

The objdiff binary is **not** an input to compile edges, so a swap landing
between a lane's leg A and leg B would fabricate Δhonest ≈ −21,500 **from an
untouched tree**. `tools/ab_measure.py` gained a **same-ruler guard**
(`373d17c6`, landed *before* the flip): it pins objdiff-cli's sha256 + size
before leg A, re-checks after leg B, and REFUSES on change — reporting
UNVERIFIED rather than passing quietly if it cannot resolve the binary.

Also bumped: `CACHE_LOGIC_VERSION` 2 → 3. Without it, `report.cache` re-serves
units diffed by the *old* binary — which would have made the whole validation
vacuous by producing a convincing "no change".

## Rollback

```
mv /home/free/code/milohax/objdiff/target/release/objdiff-cli.pre-CZ4 \
   /home/free/code/milohax/objdiff/target/release/objdiff-cli
git -C /home/free/code/milohax/objdiff reset --hard 3a024eb
```

## Second change in the same flip

NameCheck now forgives COFF **weak-external aliases** (`??_E<C>` → `??_G<C>`):
1,744/1,744 aux defaults resolve to `??_G` of the same class with zero
conflicts, so `bl ??_E<C>` and retail's call reach the same code. **Inert on the
default ruler** (all 16 keys and the disclosure identical); on the name_check
ruler 174 functions improve to RAW 100 for exactly 1,904 B, zero regressions.
Known bound, not eliminated: the gate is per-object while weak-external
resolution is link-wide — measured exposure is **one** name
(`??_EString@@UAAPAXI@Z`), affecting 0 of the 174.

Validated by lane CZ-4; flipped by the coordinator once the fleet was idle.
