# PENDING ruler change — objdiff equal-length fast-path fix (lane DR-1)

> **STATUS 2026-08-03: BUILT, GATED, STAGED — *NOT* SWAPPED.**
> Held deliberately: lanes DR-2 and DR-3 are mid-flight and will run
> `ab_measure`, which pins `objdiff-cli`'s sha256 across both legs and **REFUSES
> on a mid-run swap**. Swapping now would abort their runs (safely, but
> wastefully). **Swap when no A/B is in flight.**

## The bug being fixed

`objdiff-core/src/diff/code.rs` took a fast path pairing instructions **1:1
whenever the two instruction counts were EQUAL**, justified in-comment by
*"same-length sequences have no insertions/deletions"* — **false: N insertions +
N deletions preserve length.**

⚠ **Provenance:** the fast path is a *fork addition*, commit `3d511814`, whose
message is literally **"random changes"**. There is **no documented performance
justification** for it.

**Independent reproduction** (DR-1 could not reproduce DQ-1's literal table — that
tree state no longer exists after `2a48b057` — so it reproduced the *mechanism*
on a different function, with the culprit instructions named):

`?SetNumPoints@RndLine@@QAAXH@Z`, **976 B on both sides = 244 instructions**:

| binary | score | instructions |
|---|---|---|
| live (buggy) | **69.4%** | 103 equal, 18 diff_arg, **123 replace** |
| fixed | **99.1%** | 232 equal, 11 diff_arg, **1 insert, 1 delete** |

Our base has exactly **one extra** `mr r4, r28` and is **missing one**
`slwi r11, r30, 1`. One insert + one delete → equal length → the fast path fired
and rendered the entire **133-instruction span between them** as `replace`.

## The fix

The guard becomes **element-wise opcode equality** instead of length equality.
That is **provably equivalent to the general path** (`capture_diff_slices` on
equal slices returns a single `Equal` op covering both ranges = the 1:1 pairing),
so the fast path now only ever *saves* work.

- **Perf cost nil**: median whole-binary `report generate` **4.426 s → 4.420 s**
  (−0.13%, inside noise), 6 interleaved runs per leg, cache wiped each run. The
  retained guard still fires for **38,500 of the 39,008** pairs the old one covered.
- **Regression test shown able to fail**: against the unfixed tree
  `test_diff_instructions_equal_length_still_aligns` fails with *"no gap rows
  emitted ⇒ fast path was taken"*; three controls (identical / unequal-length /
  empty) pass on both trees.

## ★ Headline neutrality — four keys EXACT, one MOVES

Control first: a scratch-built pristine baseline is identical to the live fleet
binary on **all 11 headline keys and all 69,351 rows**, so the build environment
is not a confounder.

| key | base → fix | |
|---|---|---|
| `matched_functions` | 43,668 → 43,668 | **Δ0** |
| `matched_code` | 4,212,332 → 4,212,332 | **Δ0** |
| `matched_code_percent` | 39.40931 → 39.40931 | **Δ0** |
| `masked_equal_functions` | 22,707 → 22,707 | **Δ0** |
| `fuzzy_match_percent` | 46.10219 → **46.08841** | **−0.013780 pp** |

**0 rows cross to `mpn=100`, 0 fall off it, 0 units change `matched_code` or
`matched_functions`.**

⇒ This is a **far narrower ruler change than the 2026-08-02 flip**:
★ **Δmatched and Δcode from in-flight lanes REMAIN VALID. Only Δfuzzy is on a new
ruler** — do not chain a pre-swap Δfuzzy with a post-swap one.
The structural reason the decision-grade rulers cannot move: reaching 100
requires *every* row equal, which requires element-wise equal opcodes, which is
exactly the retained condition.

## Blast radius

| population | count |
|---|---|
| symbol pairs reaching the full diff pipeline | 40,114 |
| equal instruction count (old fast path fired) | 39,008 (97.2%) |
| — opcodes identical (old ≡ new, provably) | 38,500 |
| — **opcodes differ ⇒ EXPOSED** | **508** (1.27%, 84,412 B) |
| **actually changed score** | **257** (50.6% of exposed) |

Across all 3,266 sub-100 diffed rows, **257 changed, 97 by ≥10 pp** — those are
the rows that could have misled a lane.

★ **Adjudicated NON-METRICALLY, because 194 rows went *down*.** Over all 508
exposed rows the real alignment recovers **more** opcode-aligned pairs in **255**,
the **same** in **253**, and **fewer in ZERO**. (Patience is a heuristic, so this
was measured, not assumed.) The drops are pure repricing:
`PENALTY_INSERT_DELETE=100` charges 200 for a genuine ins+del pair where
`PENALTY_REPLACE=60` charged 60 for the blind substitution. Reconciles exactly.

## ★ The cache-version hazard — PROVEN, not assumed

`ReportCache::CACHE_LOGIC_VERSION` is bumped **3 → 4**. DR-1 proved it necessary:
running the fixed logic *without* the bump against a cache written by the old
binary reproduced the old number **to the digit** (46.10219).
⛔ **Without the bump the swap silently does nothing wherever a warm
`report.cache` exists**, then serves a mix as objects change.

⚠ Two adjacent traps: the cache path is `set_extension("cache")`
(`report_x.json` → `report_x.cache`, **not** `.json.cache`) — this invalidated
DR-1's first perf run; and `--concise` prints one decimal, which made rounding
look like ruler disagreement in its first gap instrument. Both caught and redone.

## ★ EQUAL LENGTH IS NECESSARY BUT NOT SUFFICIENT — a failed prediction, reconciled

Lane DR-2 predicted `CrowdAudio::SetTypeDef` would trip the bug (removing 3
instructions made our count *exactly* equal the target's) and **it did not** —
`report.json` read a clean 100.0. That is not a counterexample; it is the model
working:

- The old fast path fired on **39,008** equal-length pairs, but **38,500** of
  those had **element-wise identical opcodes**, where old ≡ new *provably*. Only
  the **508** with differing opcodes were EXPOSED.
- ⇒ **You need equal length AND a differing opcode at a paired index.**
- ★★★ **A row that CROSSES TO 100 can never show the bug**, because 100 requires
  every instruction equal — which is exactly the retained condition. This is the
  same structural fact that makes the fix headline-neutral.

⇒ **Do not expect the bug on a crossing. Expect it on a SUB-100 row whose length
just became equal** — the signature is a wall of `replace` with no insert/delete.

## DQ-3 interaction — INDEPENDENT

The equal-length bug **contributes ZERO** to the `diff` vs `report generate` gap.
`diff_instructions` lives in `diff_code()`, shared verbatim by both subcommands,
so it cannot produce an asymmetry; measured on 250 named sub-100 rows the
disagreeing set is 221 (base) vs 227 (fix), symmetric difference **6 rows, all 6
in the realigned set**.

The gap is **partially config**: `diff` uses `FunctionRelocDiffs::DataValue` +
schema defaults; `report generate` sets `none` / `combineDataSections=true` /
`combineTextSections=true` / `ppc.calculatePoolRelocations=false`. Replicating all
four cuts disagreement **221 → 160 rows** and max gap **7.27 → 4.81 pp**.
DQ-3's direction reproduced exactly (**report > diff in 221/221, 0 the other
way**), but ⛔ **64% of rows still disagree at identical config — that residue is
NOT settled.**

## Swap procedure

Staged binary: `/home/free/code/milohax/objdiff/target/release/objdiff-cli.laneDR1-staged`
sha256 `0bcc7dd0d5af9281f8bd0dce3c344dfec3a81542791cc6864c3333d1d41aa338`
(same filesystem as the live binary ⇒ `mv` is an atomic rename).
Backup copy: `/home/free/tmp/laneDR1/objdiff-cli.laneDR1-staged`.
Patch: `/home/free/tmp/laneDR1/laneDR1-eqlen-fix.patch` (applies to objdiff `6ee1098`).
Branch: `refs/heads/laneDR1-eqlen-fix` = `4e932e6` in `../objdiff`, a fast-forward
from HEAD, created via plumbing with a temp index (the shared checkout is
untouched, still on `oversub-disclosure`).

```sh
cd /home/free/code/milohax/objdiff/target/release
cp -a objdiff-cli objdiff-cli.pre-laneDR1-ff7fcf52
mv objdiff-cli.laneDR1-staged objdiff-cli
git -C /home/free/code/milohax/objdiff merge --ff-only laneDR1-eqlen-fix
```

⛔ **The `git merge` is NOT optional** — without it the next
`cargo build --release` in `../objdiff` **silently reverts the fix.**

**Verify:** `sha256sum objdiff-cli` → `0bcc7dd0…`; then the first
`report generate` should take **~4.4 s, not ~0.1 s** — proving the cache version
bump invalidated the old entries.

**Rollback:**
```sh
cp -a /home/free/code/milohax/objdiff/target/release/objdiff-cli.pre-laneDR1-ff7fcf52 \
      /home/free/code/milohax/objdiff/target/release/objdiff-cli.rb && \
mv /home/free/code/milohax/objdiff/target/release/objdiff-cli.rb \
   /home/free/code/milohax/objdiff/target/release/objdiff-cli
git -C /home/free/code/milohax/objdiff branch -D laneDR1-eqlen-fix
# if already merged: reset oversub-disclosure to 6ee1098
```

## ⛔ NOT settled

- The **64% residual `diff`-vs-`report` disagreement at identical config**.
  Suspected report-side symbol pairing (funclet byte-signature pairing /
  `symbol_equivalences`) wired differently from `diff` — **untested**.
- ~~**Whether past lane conclusions should be re-examined.**~~ **SETTLED by lane
  DS-2, 2026-08-03 — see below. No lane was misled. Do not re-fund.**
- DR-1's absolute figures are from a worktree off an earlier main (main has since
  advanced). The comparison is binary-vs-binary on a **fixed** tree, so it is
  common-mode and valid — but the absolutes are of that tree, not current main.
  ✅ **Re-derived on current main by DS-2 and IDENTICAL**: 508 exposed / 257
  changed / 97 ≥10 pp, `matched_functions` 43,678 and `matched_code` 4,215,804
  Δ0 across the binary swap. The exposed set is stable across the tree drift.

## ✅ RESOLVED — lane DS-2: no lane conclusion was corrupted

Reproduction first: DR-1's blast-radius figures reproduce **exactly**, both from
its own artifacts and re-derived on current main (40,114 reached / 39,008 eqlen /
508 exposed / 257 changed / 97 ≥10 pp).

### ★ The 257/97 figures were never the harm-capable population — SPLIT BY DIRECTION

DR-1 reported the movers without splitting them. The split is decisive:

| | count | ≥10 pp |
|---|---|---|
| **UNDER**-reported (`fix > base`) — bug DEFLATED ⇒ abandonment-capable | **63** | **15** |
| **OVER**-reported (`fix < base`) — bug INFLATED ⇒ cannot cause abandonment | 194 | 82 |

An inflated score never makes anyone abandon anything. **82 of the 97 large
movers are DOWN**, so the population that could have made a lane see a collapse
is **15 rows, not 97** — an 6.5× smaller worry than the open question implied.

### ★★★ THE INSTRUMENT INVERTS ON EXACTLY THE POPULATION IT WAS AIMED AT

Exposure requires **today's** source to produce an equal instruction count. A
lane that **reverted** its fix put the source *back* into the unequal-length
state ⇒ **reverted work is BY CONSTRUCTION largely ABSENT from the exposed set.**
Verified, not argued: on the reverted `BoxMap` state both binaries agree exactly
(85.214 / 73.783 / 65.722), i.e. the row is not exposed at all.

⇒ Scanning the exposed set for reverts is structurally incapable of finding them.
It finds a *different* population — rows filed as walls **at the landed state**.
Hunting reverted work requires **re-applying the patch and scoring a 2×2** (both
binaries × both source states). Both populations were worked separately.

### CONTROL — the harm-capable stratum is NOT enriched for lane attention

"Touched" = has a `decomp.db.attempts` row or a terminal `functions.verdict`.

| stratum | n | touched | enrichment |
|---|---|---|---|
| CONTROL: all sub-100 rows | 25,667 | 9.9% | 1.00× |
| EXPOSED (508) | 506 | 64.8% | 6.6× |
| UNDER-reported (63) | 63 | 34.9% | 3.5× |
| **UNDER-reported ≥10 pp (15)** | 15 | **13.3% (2/15)** | **1.35×** |

⚠ **The 6.6× on EXPOSED is REVERSE CAUSATION, not harm.** Equal instruction count
is a *consequence* of a lane having driven our size to the target's, so exposure
is enriched for lane attention by construction. The reading that matters is the
gradient *within*: harm-capability and lane attention move in **opposite**
directions, bottoming out at the base rate (2 of 15, vs 1.5 expected) exactly
where the deflation is largest. **13 of the 15 biggest deflations were never
worked by anyone** — nobody was there to be misled.

### The four adjudicated re-open candidates — ALL CLEAR

Every revert found in `git log` decided on a **whole-binary** Δmatched/Δcode is
immune by construction (those keys are Δ0). Only per-row sub-100 decisions could
be corrupted; there were four, and all four survive re-adjudication:

| candidate | decided on | verdict |
|---|---|---|
| `914d6a99` BoxMap `ApplyLight(Point/Spot)` | per-row mpn 73.783→65.663 / 65.722→63.989 | **CLEARED — 2×2 run.** Re-applied; both binaries report the *identical* numbers. Regression is real, revert correct. |
| `54e8341b` AX-7 `NextSongPanel::FinishLoad` | per-row mpn 52.11→40.6 | **CLEARED AND ALREADY RECOVERED** — laneAY-B supplied the missing `highscore_1.lbl` statement; the row is **mpn 100** today. "Body first, then statics" was right. |
| `c37e3012` laneBF-8 SYNC_PROP local-static | — | **CLEARED — never lost**: reapplied at `79a507b4`, lever later extended by CT-4/CU-2. |
| `2c023f0e` CO-2 DisplayChord hoist | per-row mpn 97.2→96.7 | Not exposed; regression re-measured stable across two independent readings by CO-2 itself. |

And every `AT_LIMIT` verdict sitting on an under-reported row is justified by a
**named mechanism**, never by the number — the four laneBF/W6 rows (+12.2/+8.5/
+6.6/+4.2) carry a *class* verdict (family-stride map-mispair, routed to the
splits channel) with `start_percent == end_percent`, i.e. **no work was ever
attempted, so no number could have deterred it**; `PropAnim::Handle` (+4.5) was
proven a wall with five instruments; `CustomizePanel::PreviewMakeup` (+5.7) is
`COMPLETE` at 100%.

⚠ Limitation, stated because it bounds the claim: map/splits/body-port lanes
often never write `decomp.db`, so the touch rates are **floors**. They undercount
numerator and denominator alike, so the enrichment *ratio* holds.

### What the bug DID cost — a corrected diagnosis, not lost work

`?SetNumPoints@RndLine@@QAAXH@Z` reads **69.693 → 99.156**. Under the old ruler
its residual looked like a 123-instruction `replace` wall; the truth is **one
insert + one delete**. Its DB note calls the residue a "remat-vs-spill band;
permuter long-shot". The real, now-visible mechanism is sharper: target calls
`__savegprlr_28`, we call `__savegprlr_27` — **we burn one extra callee-saved
register** because MSVC CSEs `num*2` into `r28` and holds it across the loop,
while retail puts it in volatile `r4` (dead at the call) and **rematerializes**
`slwi r11,r30,1` for `(num-1)*2`. That one choice produces the extra `mr r4,r28`,
the missing `slwi`, the r27↔r28 swaps and the whole +0x10 frame delta.
⛔ Not source-addressable: both oracles (rb3-Wii, DC3) already spell these
expressions as we do — the previous lane deliberately adopted rb3-Wii's
mutate-`num`-in-place shape to get 90.1→97.0. A 5th spelling (`num*2-2`) was
tried by DS-2 and moved it **0.000 pp** (MSVC canonicalises). Genuinely
permuter-class, and the permuter is banned. **Parked, with the mechanism named.**

⇒ **The lasting value here is triage, not recovery**: 13 rows now known to be far
closer to matching than the campaign's records say (e.g. `CrowdAudio::SetBank`
74.06 not 49.01; `PatchPanel::SyncProperty` 97.19 not 87.04). Those are forward
candidates, not salvage.

Reproduce: `scripts/harvest/eqlen_lane_xref.py <rep_oldbin.json> <rep_newbin.json>`.
