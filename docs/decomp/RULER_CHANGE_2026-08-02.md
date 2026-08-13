# ⚠ RULER CHANGE — 2026-08-02: `masked_equal_functions` now discloses ALL funclet pairings

**If you are reading an "honest" figure from before 2026-08-02, it is stale by ~21,500.**

> **Update 2026-08-13 — ruler identification, only.** This is a dated result doc
> and its body is frozen provenance; every figure and every verdict below stands
> as written. One piece of *advice* in it has been overtaken: §"What changed"
> says version strings cannot identify the ruler and to pin by sha256. Since the
> objdiff fork landing, **`--version` prints `objdiff-cli 4.2.3 (<commit12>,
> xxh3 <hash16>)`** — the fork commit and a hash of the executable — and **every
> generated report self-identifies** via a `provenance` block carrying
> `tool_version` / `tool_commit` / `tool_binary_hash` / `diff_config` /
> `map_file_hash` / `cache_hits`. Two builds that both said `4.2.3` no longer
> look alike, and a *report* can be attributed to its ruler after the fact
> without having kept the binary.
>
> sha256 pinning is not wrong and `tools/ab_measure.py`'s same-ruler guard is
> unchanged — it is simply no longer the *only* way to tell two rulers apart.
> Also folded into the cache key at that landing: the resolved config, the map
> file's content, and the binary hash — which **removed** the
> `CACHE_LOGIC_VERSION` counter described under §"The hazard this created"
> rather than adding to it. It is not a floor and it is not still there: the
> constant is gone from the source, and hashing the binary does automatically
> what the counter needed a human to remember. See the §"The hazard this
> created" note.

## What changed

The local `../objdiff` fork was flipped (source `f74dce1` + `6ee1098` on top of
`3a024eb`). `masked_pairing` had always set `masked_equal_symbol` on **every**
funclet byte-signature pairing (`objdiff-core/src/diff/mod.rs:312-314`), but
`objdiff-cli/src/cmd/report.rs:740-754` derived the reported
`masked_equal_functions` from **pass-2b over-subscription only**.

| measure **@none** | before | after |
|---|---|---|
| `masked_equal_functions` | 1,096 | **22,640** |
| honest (`matched − masked_equal`) | 42,358 | **20,814** |
| disclosure share of `matched_functions` | 2.52% | **52.10%** |

⚠ **Every absolute in this document is `@none`** (the report edge hard-codes
`functionRelocDiffs=None`) **except where it says `@name_check`**, and every one
of them is a snapshot of one tree state. Absolutes here are **tree-sensitive
independently of the ruler**: re-measuring `matched_code_percent` at the pinned
ruler four days later read `40.755680` against a same-day `41.0118`, a ~0.26 pp
drift caused by a single object being rebuilt in between, while every
*relational* claim held exactly. Quote a **ruler** *and* a **tree state** next to
any absolute, and pin the ruler by **sha256** — `~/.local/bin/objdiff-cli` and
the fork build both report `objdiff-cli 4.2.3` and only the fork supports
`name_check`. The fork build in use is sha256 `ca2be75232767f53…`.
*(Update 2026-08-13: "both report `objdiff-cli 4.2.3`" was true when written and
is not any more — `--version` now carries the commit + executable xxh3, and
reports self-identify. See the banner at the top of this file.)*

## What did NOT change — verified, 11/11 keys identical @none

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

*(Update 2026-08-13: `CACHE_LOGIC_VERSION` no longer exists — the fork
**removed** the counter, it was not merely supplemented. The cache key now folds
in the objdiff-cli binary's own xxh3, the resolved diff config, and the map
file's content hash, so a binary swap invalidates the cache by construction and
there is nothing left for a human to remember to bump; that is precisely why the
counter went. Do not go looking for the constant, and do not treat a bump as a
step you still owe. Check `provenance.cache_hits` on the report instead — with
`.get("cache_hits", 0)`, because proto3 omits zero-valued scalars and a fully
cold run has no such key.)*

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
default ruler** (all 16 keys and the disclosure identical **@none**);
**@name_check** 174 functions improve to RAW 100 for exactly 1,904 B, zero
regressions.
Known bound, not eliminated: the gate is per-object while weak-external
resolution is link-wide — measured exposure is **one** name
(`??_EString@@UAAPAXI@Z`), affecting 0 of the 174.

Validated by lane CZ-4; flipped by the coordinator once the fleet was idle.

## How much does the `name_check` ruler actually withdraw? **[0.23, 4.00] pp**

Added 2026-08-06 (lane WS-4), citing decomp-synth's independent audit
(`decomp-synth/docs/reloc-name-blindness.md`; classification archived at
`decomp-bench/archive/harvest/relocname-audit-2026-08-06/`, decomp-bench
`6cc3caa6`). Full record and the A/B:
[`RELOCNAME_AUDIT_ALIGNMENT_2026-08-06.md`](RELOCNAME_AUDIT_ALIGNMENT_2026-08-06.md).

Flipping the ruler withdraws **11.89 pp** of `matched_code` (`40.755680 @none` →
`28.866774 @name_check`, tree `a236686e`, ruler `ca2be75232767f53…`). Almost none
of that is defect. All 12,679 charged (T, B) pairs were adjudicated by comparing
our compiled body for B against retail bytes at `addr(T)`:

| body verdict | pairs | share |
|---|---:|---:|
| FOLD + FOLD via thunk | 7,882 | **62.2%** |
| GENUINE (different size, or same size different code) | 3,356 | **26.5%** |
| non-call / one-sided, or unresolved | 1,441 | 11.4% |

> **`matched_code` is overstated by between 0.23 pp and 4.00 pp `@none`.** The
> upper bound is the whole GENUINE stratum rolled up to functions (2,863 fns /
> 422,804 B). The lower bound is this repo's own **adjudicated** figure for the
> at-100 charged population — **298 fns / 25,920 B / 0.2425 pp** (lane CW-2,
> `34017f74`), superseding CV-4's estimate of 353 fns / 24,836 B / 0.23 pp
> (`34b44dd6`).

The band is wide because a body difference has two causes and no body test
separates them: our source calls the wrong function, **or** the retail map
mis-attributes the name at the destination VA — and the `map(...)` lane series
exists to repair the latter. ⇒ **GENUINE is an upper bound on defects, not a
defect count.**

⚠ Do **not** quote the NameCheck commit's `35.51% → 20.96%`. The relative drop
does not reproduce; the alias map has grown since it was measured.
