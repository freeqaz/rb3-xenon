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
