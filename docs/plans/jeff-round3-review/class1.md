# DOC CLASS 1 review — jeff-pdata-boundary-round3.md (Fable read-only confirm+plan)

Date: 2026-07-30. Doc under review: `docs/plans/jeff-pdata-boundary-round3.md`
(STATUS 2026-07-12, "design — not yet implemented"). This file re-verifies the
doc's **Class 1** ("Truncated-fragment pins — symbol span has no terminator")
against CURRENT jeff (`../jeff` HEAD `b50881e`) and CURRENT rb3-xenon build.

All work here was read-only wrt main: census run in throwaway worktree
`~/tmp/wt-jeff-r3-census` (removed after). Census output:
`~/tmp/jeff-r3-review/class1_census.txt`.

---

## A. Doc-class → landed-commit mapping (DISAMBIGUATION resolved)

jeff has its OWN "Class 1/2/4" taxonomy in `src/cmd/xex.rs`. Mapping to the
DOC's Class 1/2/3:

| DOC class | jeff taxonomy | landed commit(s) | status |
|---|---|---|---|
| **DOC Class 1** (terminatorless pin, extend/merge-fwd/demote repair) | jeff **Class-1** | `fc5d2af` = **CENSUS ONLY** (env-gated `JEFF_CLASS1_CENSUS`, read-only diagnostic) | **repair pass NOT implemented; census-settled NO-GO** |
| **DOC Class 2** (AddRoll pdata over-split merge) | jeff **Class-2** | `7e49a38` `merge_fallthrough_leaf_fragments` (+7f69b9e doc note) | **LANDED, +67..+77, AddRoll closed** — but reimplemented on the SYMBOL layer, not raw `.pdata` (doc's raw-`.pdata` predicate finds 0) |
| **DOC Class 3** (stray func_type==3 except_data suppression) | jeff **Class-3** | none | **NO-GO: population 0, already solved by `b1bc97c` write-gate** |
| — (not in doc) | jeff **Class-4** | `b50881e` `merge_branch_reached_overcarve_tails` | **LANDED, +35** — NEW class; complement of Class-2 (head ends in blr/b, branch-proven anon tail past it). **Swept the "cleanest 18" of DOC-Class-1's genuine repairable fragments as a side effect.** |

Key mapping fact: **the doc's Class-1 repair pass (`repair_terminatorless_functions`
extend/merge-forward/demote) was never built.** Only a read-only census
(`census_terminatorless_functions`, fc5d2af L22-249) was landed, run once on
2026-07-17, and the verdict recorded in memory
`project_jeff_class4_merge_2026-07-17` was **NO-GO**. The passes that DID land
(Class-2/Class-4) attack the ADJACENT-fragment problem from the merge side and
already absorbed the repairable subset.

Invocation correction: the task brief called the census "a subcommand/flag of
the xex tool." It is NOT a standalone subcommand — it runs inside the normal
`dtk xex split` when `JEFF_CLASS1_CENSUS=1` is set (writes to
`JEFF_CLASS1_CENSUS_OUT`, else stderr; L36, L96-124, L619-621). Running it
therefore forces a re-split (mutates symbols.txt) → must be done in a worktree.

---

## B. Per-premise verdict (CONFIRMED / REFUTED / CHANGED, with measured numbers)

### B1. "77/279 = 28% of leaves are TRUNCATED_FRAGMENT" — CHANGED (denominator moved)
- Doc figure was a wave-36-corpus rate for net-new game/engine leaves.
- **Current whole-binary census: `total=959` terminatorless function symbols**
  (guard_a=202 guard_b=17 guard_c=522 guard_d=20 guard_e=198 **anomaly=0**;
  pinned=754 gap=205; selfpdata=522; last_bl=522 last_bcond=39 last_other=398).
- The `522` guard-c figure **reproduces the 2026-07-17 memory census EXACTLY**
  → census is stable, not drifting.
- The 28% "bogus split boundary" framing is **REFUTED as a characterization**:
  **522/959 (54%) are self-pdata-anchored functions that end in a noreturn
  `bl`** (`lastw=0x4bff…` call, `selfpdata=1`) — these are COMPLETE real
  functions with no `blr` because the last call never returns (throw/fatal),
  NOT fragments. Another 202 (guard-a) end at a next-symbol that has its own
  independent entry proof (nonzero xref) → also not this function's tail.
  `anomaly=0` proves the Class-2 merge left nothing mergeable on the table.

### B2. "4 fixtures + 2 label≠body pairs, all currently at_limit / terminatorless" — REFUTED
Re-checked all 6 in current `symbols.txt` + `report.json` + census:

| doc fixture | doc claim | CURRENT state |
|---|---|---|
| `DrivenPropertyEntry` @0x8275CAB0 | at_limit, terminatorless | **address gone** as a symbol start; absorbed into a run of 0x20 leaves; `fn_8275CA9C`/`fn_8275CABC` = **strict 100.0** |
| `Metronome` @0x826D1F88 | at_limit | **address gone**; inside grown `fn_826D1F08` (size 0x84) |
| `AccomplishmentProgress` @0x82565EE8 | at_limit | **address gone**; inside `fn_82565ED4` (TrainingMgr) = **strict 100.0** |
| `MeasureMap` @0x827AB6F0 | at_limit, terminatorless | `fn_827AB6F0` (size 0x28) still exists but **now strict 100.0** and **NOT in census (terminator-complete)** |
| `BufStream` @0x827A7104 (true 0x827A70C0) | label≠body | **both addresses gone**; absorbed into grown `fn_827A7010` (size 0xE8) |
| `CheatProvider` @0x823E1AF0 (true 0x823E1AC0) | label≠body | **both addresses gone**; re-carved to `fn_823E1ABC`/`fn_823E1AE4` |
- **NONE of the 6 appear in the current census** → all are terminator-complete.
  The doc's concrete acceptance fixtures no longer exist as truncated pins;
  grow/clamp + Class-2/Class-4 already handled them. MeasureMap flipped from
  at_limit → 100%.

### B3. "dtk 'ends within symbol' error is the authoritative boundary-fix signal" — CHANGED
- Still a real signal, but it is now largely PRE-DRAINED: the split runs clean
  (only tolerated BINK/idata/jumptable WARNs, `INFO Done!`), and the residual
  terminatorless set is dominated by legitimate noreturn-bl / independent-next
  cases where "ends within symbol" does NOT fire. Not a useful primary oracle
  for the residue.

### B4. "Class 1 near-term +2..+4, +10-30 unlocked surface" — REFUTED under honest-floor
See section D pricing. Honest Δ ≈ 0.

---

## C. What is ACTUALLY still open for DOC Class 1

**Almost nothing that is worth a pass.** Breakdown of the 959 terminatorless:

- **522 guard-c (self-pdata noreturn-bl):** NOT fragments — complete functions
  ending in a non-returning call. **Extending them would CORRUPT** (memory's
  explicit warning; confirmed by lastw=`bl` + selfpdata=1). HARD no-touch.
- **202 guard-a:** next symbol has independent entry proof (xref>0). Real short
  functions / correct boundaries. No-touch.
- **20 guard-d:** next in a different split unit — P5 (same-split) correctly
  blocks; merging would be the "split ends within symbol" over-fire. No-touch.
- **17 guard-b:** next is named/protected — must not absorb a real function.
- **198 guard-e:** next is unclaimed gap/object. This is the ONLY theoretically
  extendable bucket, but samples are tiny leaves (0xc–0x64) ending in
  bcond/`OTHER`; memory's hand-review found **genuine ~33, and the cleanest 18
  already swept by Class-4**. Residual actionable ≈ **a dozen or two tiny
  leaves**, each a funclet-class body.

Open items that are NOT a repair pass:
1. **Demote-only (optional, cosmetic):** rename terminatorless survivors
   `__FRAGMENT_*`/Unknown so wave classifiers stop re-deriving the predicate.
   Zero match value; only de-noises identification. Low priority.
2. Nothing else. The extend/merge-forward repair is refuted by the guard
   distribution (54% would corrupt, most of the rest are correct boundaries).

---

## D. Honest-floor pricing (load-bearing) — the doc's payoff is ≈0

Pricing rule (memory `project_honest_floor_2026-07-29` §5): **price every
landing by Δ(`matched_functions` − `masked_equal_functions`); if Δ≈0, DISCARD.**

Current measures (main `report.json`): `matched_functions=40882`,
`masked_equal_functions=1509`, honest floor ≈ **39,373**.

Why Class-1 repair prices to ≈0:
- The terminatorless residue is tiny leaves (census size hist peaks at
  0x8/0x28/0x2c/0x3c). Per `project_funclet_pool_closed_2026-07-29`, leaves in
  this size band are **funclet-class**: 21,314 funclets = 52.9% of
  matched_functions but only 22.6% of matched *bytes*, and **99.9% are paired
  only by objdiff's PASS-3 `pair_funclets_by_bytes` byte fallback** → they
  register as matched AND as masked_equal → **Δ(matched−masked_equal)=0**.
- Empirical confirmation already visible WITHOUT any Class-1 pass: the doc's
  fixtures (MeasureMap etc.) already flipped to 100% via grow/clamp+Class-2/4.
  Those flips are exactly the funclet-fallback kind — they raised the headline
  but not the honest floor.
- So the doc's **"+2..+4 near-term" ≈ 0 honest**, and **"+10-30 unlocked
  surface"** is mostly funclet-noise surface = also ≈ 0 honest. Any real body
  hiding among them (a named RB3 class whose source we compile) would surface
  via the normal port pipeline regardless of a Class-1 pass; a boundary-repair
  pass is not the lever that closes it.

Net: the permanent-at_limit framing that justified the doc ("only jeff can fix
these") is FALSE for Class 1 — they are already terminator-complete, and the
few genuinely-open ones are honest-floor-worthless.

---

## E. Revised, honest Class-1 plan section (fold into the doc)

> ### Class 1 — RESOLVED / NO-GO (supersedes the 2026-07-12 repair design)
>
> The proposed `repair_terminatorless_functions` (extend / merge-forward /
> demote) was **not built** and is **not worth building**. Only the read-only
> census (`census_terminatorless_functions`, jeff `fc5d2af`,
> `JEFF_CLASS1_CENSUS=1`) landed; running it settled the class.
>
> **Evidence (2026-07-17 census, reproduced 2026-07-30):** 959 terminatorless
> function symbols, of which **522 (54%) are self-pdata noreturn-`bl` complete
> functions** (extending them corrupts), **202 have an independently-proven
> next entry**, **40 are cross-split / named-next** (correctly blocked), leaving
> **198 gap-tail candidates**; hand-review found **~33 genuine, cleanest 18
> already absorbed by the Class-4 pass** (`b50881e`). `anomaly=0` confirms the
> Class-2 merge left nothing mergeable.
>
> **Fixtures:** all 6 original acceptance fixtures (DrivenPropertyEntry,
> Metronome, AccomplishmentProgress, MeasureMap, BufStream, CheatProvider) are
> **already terminator-complete** in the current tree (grow/clamp + Class-2/4);
> MeasureMap `fn_827AB6F0` is now strict-100. None remain terminatorless.
>
> **Honest-floor payoff: ≈0.** The residue is funclet-class leaves paired only
> by objdiff's byte fallback → Δ(`matched_functions`−`masked_equal_functions`)≈0
> (pricing rule, `project_honest_floor_2026-07-29`). The original "+2..+4 /
> +10-30" estimate predates the funclet/honest-floor accounting and is retracted.
>
> **Only optionally-open item:** a demote-only relabel (`__FRAGMENT_*`) to
> de-noise the identification surface — zero match value, low priority.

### E1. Amended validation (if any demote-only work is ever done)
The doc's validation plan needs two amendments from
`project_bandexe_read_traps_2026-07-29`:
- **`.pdata`-absence is NOT a not-a-function test.** The doc leans on pdata to
  decide "no pdata entry by construction." Frameless leaves are systematically
  absent from `.pdata`; a terminatorless symbol lacking a pdata entry is NOT
  therefore a fragment. Prove entry via an incoming `bl` (the census already
  computes the post-`tracker.apply` xref set — use guard-a's `next_xref` and an
  incoming-`bl` scan, never pdata-absence).
- **Split-churn / symbols.txt-drift floor (~2-5 fns).** The doc's "matched-SET
  diff" + "'ends within symbol' error count" gate is directionally right but
  must control for drift: `symbols.txt` is both dtk input and output; feeding a
  drifted copy moves ~5 functions silently. Protocol: `git checkout --
  config/45410914/symbols.txt` before EVERY split-forcing build on BOTH legs,
  and take both A/B legs in the same (second-split) state. Quote **deltas within
  one worktree only**; absolutes are not portable across worktrees.
- **Count on the honest floor**, not raw strict-100: gate on
  Δ(`matched_functions` − `masked_equal_functions`), not
  `match_percent_normalized==100` set size (which counts funclet fallbacks).

---

## F. Recommendation

**DROP the Class-1 repair pass. Keep the census as a diagnostic only.**
- The pass is refuted three ways: guard distribution (54% would corrupt), the
  fixtures already self-resolved, and honest-floor payoff ≈0.
- Optional tiny follow-up: a demote-only `__FRAGMENT_*` relabel purely for
  identification hygiene — defer unless a wave is actively tripping over the
  959 survivors.
- This matches the standing memory verdict `project_jeff_class4_merge_2026-07-17`
  ("Class 1 & 3 = NO-GO, do NOT re-hunt"); this review confirms it holds on the
  2026-07-30 tree.
