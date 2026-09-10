# Unicorn behavioural harness — the deeper schedule (2026-09-10)

Lane **U1-DEEPSCHED**, following S4-UNICORN (`83eba972`). `decomp.db` is
gitignored, so a verdict set that lives only there is an **invisible
artifact**. This file and the committed CSVs are the record.

## Headline

S4 closed with: *"the `logic` class is still 0 … THE OUTSTANDING WORK IS A
DEEPER SCHEDULE, NOT A BROADER RUN."*

**The binding constraint was not fixture richness. It was fixture
CORRECTNESS.** The deepest available schedule change turned out to be a
*truer* fixture rather than a wilder one — and once it was applied, the
broader run became cheap as a side effect: the same 524 functions that
**wedged the harness entirely** before now complete in **12 seconds**.

**The `logic` class is STILL 0.** That is now a much better-founded zero than
S4's: it is measured on a fixture where the incoming `this` pointer is no
longer destroyed at the second instruction of every function.

## 1. Was the harness DEMONSTRATED able to emit DIVERGENT?

**Yes, twice, and one control was designed so it could fail.**

| control | result |
|---|---|
| mutation sensitivity, `?ClearBones@CharBones@@QAAXXZ` | **3/4** deletions caught, null leg EQUIVALENT |
| same fixture, re-run AFTER the port | **3/4**, unchanged — sensitivity preserved |
| `arg_registers` positive control (`mr r3,r4; blr`) | `r3=0x00000000` without → **`r3=0x20002000`** with |

The third is the one that matters methodologically. The out-parameter leg
measured **Δ0**, and a Δ0 has two readings — "the schedule changed nothing" and
"the knob was never connected" — which are *not* the same finding. The
hand-assembled control distinguishes them: the knob is live, so the Δ0 is a
genuine negative.

⚠ The C trampoline hook must be rebuilt against the **venv's Python 3.10**
(`make PY_INCLUDE=… EXT_SUFFIX=…`); the Makefile defaults to system 3.14 and
produces a silently unimportable `.so`. This is not cosmetic: the pure-Python
fallback had a **signedness bug** that made every call-target lookup miss, so
the wrong-callee warning was **silently lost entirely**. The `.so` is
gitignored and `setup_worktree.sh` does not build it, so *every fresh worktree
takes the degraded path by default*.

## 2. What the deeper schedule actually is

Eight defects, ported from dc3-decomp and verified **in this binary**. The
decisive one:

> **MSVC on Xenon does not open-code callee-save spills.** It calls out-of-line
> helpers (`__savegprlr_N` / `__restgprlr_N` / `__savefpr_N` / `__restfpr_N` /
> `__savevmx` / `__restvmx`), which are external REL24 targets — and the
> harness gave every external REL24 target the same `li r3,0; blr` stub.

Two things broke at once:

1. `bl __savegprlr_29` executed `li r3,0`, **destroying the incoming
   `this`/arg0 at the SECOND INSTRUCTION of the function**, on both sides. The
   whole body then ran against a null object.
2. `b __restgprlr_29` is a **tail** branch, and the real helper is what reloads
   LR from `-0x8(r1)`. The stub does not, so its `blr` returned to a stale LR:
   the function re-entered its own tail and spun to the instruction cap.

dc3 measured **87.5%** of swept functions using a helper on both sides. That is
one mechanism behind **both** of S4's headline pathologies — the shared-crash
EQUIVALENT (both sides null-object, same PC) and the large `cap_exhausted`
population (symmetric tail-spin). Those verdicts described the harness.

**Verified here, not inherited:** `test_save_helpers.py` is **29 passed / 0
skipped**, including reading RB3 retail's own bytes at RB3 retail's own
addresses — all **72** GPR/FPR helper bodies match `band.exe` byte-for-byte,
and the VMX `r11` word matches the word the image ends each bank with. A *skip*
there would have meant the bodies were unverified here however well dc3
verified them, which is why the test was retitled rather than left to skip.

The other seven, each removing a class of manufactured divergence:

| fix | what it stops |
|---|---|
| `__real@…` float synthesis | the ORIGINAL side loaded **0.0f** where we loaded 1.0f (they are UNDEFINED externals there) — dc3's single largest DIVERGENT source |
| shipped-image global seeding | cross-unit globals are undefined on the split side, so it divided by 0 where we divided by 48000 — and the runner blamed **us** |
| signedness (`_u32`) | **everything above 0x80000000** (all of CODE/TRAMPOLINE/RDATA) silently failed every region check |
| RDATA in the region check | every pointer-to-string-literal argument — the commonest shape here — was filed as a real `call_arg` bug |
| ICF fold under a real name | defeated `has_merged`, manufacturing `call_count` divergences between two 100% matches |
| co-loader ROOT slot sizing | sized from the DECOMP side only, overwriting the original's tail — concentrated on the sub-100% frontier |
| `cap_exhausted` PC gate | fires for PC anywhere in CODE ∪ TRAMPOLINE ∪ HELPER |
| emulator leak (`close()`) | **this is why my own BEFORE baseline wedged** at 0.0% CPU — S4's documented failure mode. The port repairs the measuring rig. |

**Deliberately NOT ported: `extractor.py` — ours is ahead.** It carries the
`$EH` extent-boundary fix (`777f5641`) that dc3 lacks; copying dc3's would have
silently reverted *which bytes get emulated*.

New schedule legs are implemented in `scripts/unicorn/schedule_legs.py`
(`zero`/`cd`/`outparam`/`sentinel`/`typed`), as a separate file — editing the
audit module while a multiprocessing run imports it would swap the tool
underneath a measurement in progress.

## 3. The A/B — 524 functions, 51 units, identical on both legs

Sample pinned to the units the BEFORE leg completed, so both legs see exactly
the same functions. Deltas compose; absolutes do not.

| evidence | BEFORE | AFTER | Δ |
|---|---:|---:|---:|
| `equiv_real` | 201 (38.4%) | **307 (58.6%)** | **+52.7%** |
| `divergent` | 220 (42.0%) | **129 (24.6%)** | **−41.4%** |
| `equiv_matching_err` | 103 (19.7%) | **88 (16.8%)** | −14.6% |
| shared-error share of EQUIVALENT | 33.9% | **22.3%** | −11.6 pp |

| class | BEFORE | AFTER | |
|---|---:|---:|---|
| `call_arg` | 25 | **0** | S4's mock-region artifact… |
| `object_memory` | 42 | **1** | …killed **at source** |
| `data_layout` (new) | 0 | **90** | …and correctly *named* |
| `cap_exhausted*` | 144 | **26** | −82% |
| **`logic`** | **0** | **0** | the question this lane was asked |

### Pre-registered, and one prediction was WRONG

Registered before running: (1) `equiv_matching_err` falls below 50;
(2) `equiv_real` rises; (3) `divergent` falls; (4) a new `data_layout` class
absorbs `call_arg`/`object_memory`; (5) `logic` stays small.

**2, 3, 4 and 5 hit. (1) MISSED** — 103 → 88 only. The shared-error class is
far more stubborn than the helper-stub mechanism explains, and that is the
single most useful thing this lane learned about the remaining wall.

## 4. The shared-error EQUIVALENT laundering — decision and why

`comparator.py` returns EQUIVALENT when both sides hit the same error at the
same PC. S4 measured 30.9% of EQUIVALENT verdicts in that state; this lane
measured **33.9%** independently on a different sample, then **22.3%** after
the port.

**Decision: fix the CAUSES, label the residue, and do NOT flip the verdict.**

Flipping shared errors to DIVERGENT would have converted 88 harness failures
into 88 fake bug reports — the precise disease this lane exists to cure, since
S4's worklist was already 99.2% artifact. The residue is instead *labelled*
(`equiv_real` vs `equiv_matching_err`, recorded per row in every CSV here) so
it can never be counted as a positive, and it is **excluded by construction**
from the adjudicated worklist.

Fixing causes is what actually moved it: 13 rows flipped
`equiv_matching_err → equiv_real` outright. The remaining 88 are dominated by
genuine **emulation gaps**, not fixture shallowness — the canonical shape is
`?ToQuat@ByteQuat@@`, which dies at instruction 12 with `UC_ERR_EXCEPTION` and
**zero calls logged**, i.e. an illegal instruction Unicorn will not execute. No
schedule can reach past that; only emulator work can.

## 5. Screens, and what survived

Both S4 screens applied; `cap_exhausted*` excluded as **INDETERMINATE**, never
counted as a bug class.

```
129 DIVERGENT (after)
 → 121 artifact class / cap_exhausted
 =   8 real-class candidates
 →   1 byte-identical body (PROOF: identical code cannot emulate differently)
 =   7 survive both screens
 →   0 that score 100%   ⇐ NO MATCHED-BUT-WRONG ROW IN THIS SAMPLE
```

The byte-identity screen earned its keep again: the sample's **only**
100%-scoring divergence, `?processGain@ExternalMic@@QAAJK@Z` (140 B,
fuzzy 100.00 / mpn 100.00, `call_count`), has **byte-identical bodies**.
Without the screen it would have been this lane's headline bug.

All 7 survivors are functions the metric **already** calls broken (fuzzy 0.00 →
97.24). The oracle is agreeing with objdiff, not contradicting it. Full rows:
`docs/decomp/unicorn_deep_schedule_2026-09-10.csv`.

## 5a. The full corpus — the deeper schedule made the broader run CHEAP

The harness that **wedged on a 51-unit sample** before the port audits **all
1,045 units / 22,718 functions in 256 seconds**. That is **2.9×** S4's coverage
(7,816 functions over 650 units, which needed a 72-minute window and stalled
twice).

| evidence | corpus | S4 |
|---|---:|---:|
| `equiv_real` | 14,169 (62.4%) | 3,292 (42.1%) |
| `divergent` | 5,465 (24.1%) | 3,038 (38.9%) |
| `equiv_matching_err` | 3,048 (13.4%) | 1,471 (18.8%) |
| shared-error share of EQUIVALENT | **17.7%** | 30.9% |

```
5,465 DIVERGENT
 → 5,032 artifact class / cap_exhausted     (data_layout alone: 3,755)
 =   433 real-class candidates
 →    84 byte-identical bodies (PROOF)
 =   349 survive both screens
 →    40 SCORE 100%  ⇐ the matched-but-wrong worklist
```

**`logic` does not appear in the corpus distribution at all — 0 of 22,718.**

## 5b. ⛔ THE 40 IS AN UPPER BOUND, NOT A BUG COUNT

The largest coherent cluster in it — **15 `object_memory` rows on engine and
panel constructors, every one at 100.00/100.00** — looked exactly like the
dropped-initializer class dc3 found seven of. **It dissolves on inspection**,
and it reveals a screen gap rather than a bug:

| sub-population | rows | what the differing words actually are |
|---|---:|---|
| constant **0x2C** delta at object `+0x0` | 6 | `RndFlare/RndLine/RndLight/RndScreenMask/RndText/SpotlightEnder`. The delta is **identical (44) across six unrelated classes** — the signature of a systematic harness offset, not six independent source bugs. |
| decomp stores a real float, orig stores **exactly 0** | 4 | 754.0f, −100000.0f, −8.0f, 0.007f vs `0x00000000`. This is the *literal* signature dc3 documents for float-literal asymmetry (the original side's `__real@…` is an undefined external ⇒ loads 0.0f). Residual coverage gap in `_synthesize_float_constants`, not a decomp defect. |
| decomp negative scalar vs orig **GLOBAL** pointer | 5 | one side has the symbol as data, the other as an extern — placement, not value. |

⇒ **Three artifact mechanisms, none of which the `data_layout` screen catches**,
because that screen requires *every* differing word to be a harness-assigned
address and these all carry one word that is a **scalar**. The decisive case:
**arithmetic on two harness-assigned addresses produces a scalar**, which
escapes a region test by construction.

This is S4's lesson recurring one level up, and it is the single most
transferable finding here: **a divergence class label is the detector restating
its own input.** The 40 must be adjudicated on retail bytes before any of it is
called a bug — and the first 15 examined did not survive.

## 5c. The new schedules — one win, one measured negative

Over the same pinned 51 units / 524 functions (`schedule_legs.py`):

| leg | `equiv_real` | `divergent` | `equiv_matching_err` | verdict |
|---|---:|---:|---:|---|
| `zero` (default) | 307 | 129 | 88 | baseline |
| **`outparam`** | 285 | **160 (+24%)** | **79 (−10%)** | **WIN** |
| `sentinel` | 289 | 127 (flat) | **108 (+23%)** | **negative** |
| `typed` (1 unit) | = zero | = zero | = zero | null, and *applied* |

* **`outparam` is vindicated at scale** — more divergence exposed *and* fewer
  shared crashes — and it independently surfaced **one matched-but-wrong row
  the default schedule never sees** (`__unguarded_partition<MoveDetector*>`,
  80 B, 100/100). ⚠ This **overturns the negative I first drew from
  `?ToQuat@ByteQuat@@`**, which was simply an unrepresentative fixture: it dies
  at instruction 12 with zero calls logged and never reaches its out-param
  store. Choose an out-param fixture on "does it actually execute", not on
  inheritance from a prior lane.
* **`sentinel` is a measured negative.** It paid exactly the price registered in
  advance (crashes +23%, because every word is non-zero so pointer members hold
  garbage instead of NULL) and bought **no** additional divergence. Non-uniform
  fill is therefore *not* the missing ingredient S4 hoped for. It looked
  promising on a single unit (+2 divergences on `CharBones`) and flattened at
  51 units — a reminder to size a schedule on the corpus, not on a fixture.
* **`typed` is a genuine null, not a vacuity**: `fixture_note` reads
  `typed:CharBones`, proving the fixture applied rather than silently falling
  back to zero-fill.

## 6. decomp.db was NOT written

Same call as S4's, for the same reasons plus one more: the DB is shared with
concurrently running lanes, and this session was told mid-run that main's build
tree had drifted. Writing verdicts into a shared, gitignored store during that
is strictly worse than leaving it stale-but-consistent. The CSVs and this file
are the artifact.

## 7. Reproduce

```bash
# in a worktree, AFTER a FULL build (never `ninja <one>.obj`)
cd scripts/unicorn_runner && make \
  PY_INCLUDE=$(../../venv/bin/python -c "import sysconfig;print(sysconfig.get_path('include'))") \
  EXT_SUFFIX=$(../../venv/bin/python -c "import sysconfig;print(sysconfig.get_config_var('EXT_SUFFIX'))")

venv/bin/python scripts/unicorn/image_selfcheck.py            # MUST pass first
venv/bin/python scripts/unicorn/mutation_sensitivity.py \
    --unit default/CharBones --symbol '?ClearBones@CharBones@@QAAXXZ'
venv/bin/python scripts/unicorn/equivalence_audit.py -j 4 --sample-every 10 -o ~/tmp/leg.csv
venv/bin/python scripts/unicorn/schedule_legs.py --schedule sentinel \
    --units-file ~/tmp/units.txt -o ~/tmp/sentinel.csv
venv/bin/python scripts/unicorn/deep_schedule_report.py \
    --before ~/tmp/before.csv --after ~/tmp/after.csv \
    --report build/45410914/report.json --out docs/decomp/worklist.csv
```

⚠ Cap workers at **4** (standing user constraint).
⚠ Run `image_selfcheck.py` **before** believing any seeding result: the image
loader degrades quietly, so "image never opened" and "seeding changed nothing"
produce the identical null and the null agrees with the comfortable prior.

## 8. NOT verified — read this before building on any of the above

* **25 of the 40 matched-but-wrong rows were never adjudicated.** I examined
  the 15-row `object_memory` cluster and all 15 dissolved (§5b). The **16
  `call_count`** rows carry the standing inline-policy caveat — retail inlining
  a helper we call out of line produces identical behaviour and a different
  call count — and `has_icf_folded_callsites` catches only the folding it can
  see. **Do not brief any of them as a bug**; two of them
  (`?finalize@MD5@Quazal@@`, `??1AppMiniLeaderboardDisplay@@`) were already
  S4's TIER1 and are still unadjudicated after two lanes.
* **The `outparam` leg was run on 51 units, not the corpus.** It is the one
  schedule that measurably wins, and it found a matched-but-wrong row the
  default misses, so a full-corpus `outparam` run is the highest-value next
  measurement. It was not run here.
* **`sentinel` and `typed` are under-measured** — 51 units and 1 unit
  respectively. `typed`'s null is real but narrow.
* **No source was changed and no A/B was run.** The brief permitted 1–2
  demonstration fixes; I made none, because **nothing reached the bar**. The
  only cluster I adjudicated turned out to be artifact, and fixing an
  unadjudicated `call_count` row would have been exactly the mistake this lane
  exists to prevent. `src/` is untouched, as instructed.
* **The three screen gaps in §5b are diagnosed, not fixed.** In particular
  `_synthesize_float_constants` has a residual coverage gap (four rows where
  the original stores exactly `0x00000000` against our real float).
* **The 3,048 corpus `equiv_matching_err` rows are not characterised.** They
  are the remaining wall and they are an **emulator** problem, not a schedule
  problem — the canonical shape dies on an instruction Unicorn will not
  execute. Nobody has counted which instructions.
* **`decomp.db` was not written** (§6), and **`test_prober`'s single failure
  predates this lane** and is untouched.
* **Main's build tree was never built or diffed by this lane.** Every build was
  a full `./tools/ninja-locked` in `~/tmp/wt-u1sched`; `objdiff-cli` and
  `run_objdiff` were never invoked at all. The `fuzzy`/`mpn` half of the
  worklist was read from this worktree's `report.json`, and the worktree
  verifies as a patched fixed point (`tree_sha256=d568d3736ef0757f`) both
  before and after the run.
