# Unicorn behavioural-equivalence harness — refresh and audit (2026-09-01)

Lane **S4-UNICORN**. `decomp.db` is gitignored, so a refreshed verdict set is
an **invisible artifact** unless it is also written down. This file is that
record.

## Headline

The harness **runs today** and was **demonstrated able to emit DIVERGENT**
before any EQUIVALENT verdict was believed. But the naive deliverable —
"functions the metric scores 100% that behave differently from retail" — is
**1,017 rows, of which 1,009 (99.2%) are instrument artifacts, not bugs.**
Two independent screens establish this, and one of them is a proof rather
than a heuristic. **8 rows survive; ~6 are worth adjudicating.**

The single most useful sentence for a downstream lane: **a DIVERGENT verdict
from this harness is not a bug report until it has been screened**, and the
two most attractive-looking candidates in the whole set turned out to have
byte-identical bodies.

⚠ **Nothing here licenses "the harness found no bugs."** It licenses
"the raw DIVERGENT-at-100% count is not a bug count." The residue after
screening is small and is listed below.

## 1. Does it run, and can it fail?

Yes to both, and the second question is the load-bearing one: an equivalence
instrument that cannot emit DIVERGENT returns a clean, confident "no
divergences" that agrees with the comfortable prior.

`scripts/unicorn/mutation_sensitivity.py` establishes sensitivity directly.
For a function the harness calls EQUIVALENT it deletes one instruction at a
time (replacing it with `nop`) and requires the verdict to flip, with a null
leg requiring the unmutated run to stay EQUIVALENT.

| fixture | evidence class | sensitivity |
|---|---|---|
| `?RecomputeSizes@CharBones@@IAAXXZ` | `equiv_real` | **3/3** |
| `?ClearBones@CharBones@@QAAXXZ` | `equiv_real` | **3/4** |
| `?TypeSize@CharBones@@QBAHH@Z` | `equiv_real` | 3/23 |
| `?ToQuat@ByteQuat@@QBAXAAVQuat@Hmx@@@Z` | `equiv_matching_err` | **1/35** |

⇒ The harness discriminates well **when it actually executes the function**,
and is near-blind when it does not.

### ⚠ The first mutator produced a FALSE "VACUOUS" verdict

The initial version XOR'd the low 16 bits of each instruction and detected
**0/24**, which reads exactly like a dead instrument. It was the *mutator*
that was defective, not the harness:

- on X/A-form instructions bit 31 is the **Rc bit**, so the "corruption"
  only set CR0 — semantically near-identical;
- on D-form loads it shifted a **memory displacement**, which is invisible
  under a uniform fill because every offset holds the same byte.

Suspect your own newest instrument first. A vacuity that *confirms* a
finding is the hardest kind to catch.

## 2. Not every EQUIVALENT is evidence

`comparator.py:186` returns **EQUIVALENT when both sides hit the same error
at the same PC**. Such a row is a shared *emulation failure* laundered into a
positive verdict — the function's real work never executed, so nothing it
computes was ever compared. `?ToQuat@ByteQuat@@` "is EQUIVALENT" because both
sides throw `UC_ERR_EXCEPTION` at instruction 12 of 35.

Verdict counts cannot see this; the EQUIVALENT bucket looks identical either
way. `scripts/unicorn/equivalence_audit.py` records the distinction
(`equiv_real` vs `equiv_matching_err`), and the mutation table above
validates it as a sensitivity proxy rather than a cosmetic label.

A second, separate hole: **the verdict comes from the zero-fill run alone**;
the 0xCD run only sets a confidence label. So a stored row reading
`EQUIVALENT` + `input_sensitive` means *the second fixture diverged and the
verdict was recorded EQUIVALENT anyway* — **385 stored rows** are in that
state.

## 3. The mock-region artifact (measured)

The emulator mocks each relocation target into a region chosen by the
target's **section** (`memory_map.py`). When the same conceptual symbol is
`.rdata` in our object and `.data`/`.bss` in the dtk-split target object, the
mocked pointer values differ and the comparator reports `call_arg` or
`object_memory` — with no behavioural difference at all.

| class | cross-region (artifact-suspect) | candidate-real |
|---|---:|---:|
| `call_arg` | **371 / 375 (98.9%)** | 4 |
| `object_memory` | **491 / 493 (99.6%)** | 2 |

367 of the `call_arg` cases are the single pair **(rdata → globals)**.
Witness: `?Handle@UIScreen@@` has **identical call counts (2633 both sides)**
and diverges at call #2 passing `0x800200B0` (rdata region) versus
`0x30000008` (globals region).

## 4. The byte-identity screen — a proof, not a heuristic

Of the DIVERGENT rows scoring `fuzzy == 100`, **596 of 942 (63.3%) have
BYTE-IDENTICAL function bodies** with identical relocation counts.
Deterministic emulation over identical code cannot diverge, so the divergence
is attributable **entirely** to how relocation targets were mocked — never to
our instructions.

This screen earns its keep: the two most attractive "real bug" candidates
both fall to it.

| symbol | size | fuzzy/mpn | reported | bytes identical? |
|---|---:|---|---|---|
| `?GetEnabledKeyCheats@@YA_NXZ` | 12 | 100/100 | returns **1** vs retail **0** | **YES** |
| `?FloatToBucket@JoypadData@@ABAHM@Z` | 148 | 100/100 | returns **0** vs retail **1** | **YES** |

A 12-byte function reading a global returns `1` on one side and `0` on the
other because the global is mocked from **real `.rdata` bytes** on one side
and **zero-filled `.data`** on the other. Had the screen not existed, these
two would have been briefed as the headline matched-but-wrong bugs.

⚠ **Honest caveat:** byte-identity proves the divergence comes from
relocation targets, *not* that it is harmless. A genuinely **wrong callee**
also lives here — a `bl` encodes identically and only the reloc target
differs. Such a case surfaces as `call_count` or as differing trampoline
targets, not as a data-pointer region flip.

## 4a. The screened worklist: 1,017 → 8

Applying both screens to the refreshed run:

| stage | rows |
|---|---:|
| DIVERGENT ∧ scores 100% (fuzzy or mpn) ∧ class not artifact | **1,017** |
| TIER3 — cross-region pointer and/or byte-identical body | **1,009 (99.2%)** |
| **TIER2_CANDIDATE** — differs *within* a mock region | **3** |
| **TIER1_STRONG** — artifact cannot explain it | **5** |

`cap_exhausted*` contributed a further **1,742 rows** corpus-wide that are
excluded upstream as indeterminate.

### The 8 survivors

| tier | symbol | unit | size | why |
|---|---|---|---:|---|
| 1 | `??0SoundTouch@soundtouch@@QAA@XZ` | SoundTouch | 132 | one side errors, the other does not |
| 1 | `?finalize@MD5@Quazal@@QAAXXZ` | network/…/MD5 | 228 | differing **number** of calls |
| 1 | `??1AppMiniLeaderboardDisplay@@UAA@XZ` | meta_band | 284 | differing **number** of calls |
| 1 | `?Handle@CharWeightable@@$4PPPPPPPM@A@…` | CharWeightable | 12 | our side errors (⚠ see below) |
| 1 | `?Handle@MultiSelectListPanel@@$4PPPPPPPM@A@…` | meta_band | 12 | our side errors (⚠ see below) |
| 2 | `?getKeyImpl@@YAXPAEPAD0@Z` | keygen_xbox | 68 | arg **r6 scalar** `0x8` vs `0x0` |
| 2 | `??0InvExpInterpolator@@QAA@MMMMM@Z` | Interp | 92 | 2 object words differ **within** region |
| 2 | `?InterpTangent@@YAXABVVector3@@000MAAV1@@Z` | Color | 280 | return value differs |

⚠ **Discount the two 12-byte `$4PPPPPPPM@A@` rows**: those are **vcall
adjustor thunks**, and the error string is empty. A 12-byte thunk is the
shape most likely to be mis-extracted rather than mis-implemented — treat
them as extraction artifacts until shown otherwise. That leaves **~6**
genuinely worth adjudicating.

⚠ **`call_count` is the strongest class but is not proof**: an
**inline-policy** difference (retail inlined a helper we call out of line)
produces identical behaviour and a different call count. Adjudicate on retail
bytes.

## 5. Staleness of the stored verdicts

Stored: **7,960** verdicts (4,016 EQUIVALENT / 3,838 DIVERGENT / 106
SKIPPED), **all written in one 72-minute window on 2026-07-16**, covering
**7,687 live rows of 69,341 = 11.09%**.

Two staleness axes, and the DB's own keys catch **neither**:

1. **Tooling.** Every stored row carries `unicorn_signal_version = 3` and one
   schedule hash, so the built-in keys report "current". But the harness was
   patched twice on 2026-08-17 — `777f5641` (`extractor.py`: *stop DELETING
   the build's own EH extent boundary*, i.e. **which bytes get emulated**)
   and `ae56d085` (engine/comparator). Those are verdict-producing changes
   that did not bump the version. **The staleness key under-reports by
   construction.**
2. **Source churn.** **818 of 1,045 probeable units (78.3%)** had their own
   `.cpp` edited since 2026-07-16, before counting 806 changed headers that
   cascade far wider via the PCH. That is staleness by *opportunity*.

Staleness by *effect*, from re-measurement (the only honest test) over the
**2,544** symbols measured on both sides:

| | rows |
|---|---:|
| verdict AGREES | 2,374 (93.3%) |
| verdict **FLIPPED** | **170 (6.7%)** |
| — DIVERGENT → EQUIVALENT | 102 |
| — **EQUIVALENT → DIVERGENT** | **37** |
| — SKIPPED → EQUIVALENT / DIVERGENT | 20 / 11 |

⇒ **37 rows carry a stored EQUIVALENT that re-measurement contradicts**, and
**5,414 stored verdicts got no fresh measurement at all** in this partial
run. A stored verdict is a claim about a function body that has since been
edited in 78.3% of units; it is not evidence.

## 6. Coverage — and what this run did NOT cover

**This refresh is PARTIAL and that is deliberate.** It covers **7,816
functions across 650 of 1,045 probeable units (62.2%)** — comparable in size
to the entire stored set (7,960). It is not complete because Unicorn
repeatedly wedged (workers alive at 0% CPU) while the box ran at load
131–215 under two other lanes; the run was resumed twice and stalled again.
Per the brief, a smaller verified result beats a larger unverified one.

Fresh evidence-quality distribution over those 7,816:

| evidence | rows | share |
|---|---:|---:|
| `equiv_real` | 3,292 | 42.1% |
| `divergent` | 3,038 | 38.9% |
| `equiv_matching_err` | 1,471 | 18.8% |
| `skipped` | 15 | 0.2% |

⇒ **Of 4,763 EQUIVALENT verdicts, 1,471 (30.9%) are a shared emulation
error** carrying no equivalence evidence. Nearly a third of the harness's
positive verdicts are not evidence of anything.

The probeable set has **grown**: 1,045 units now have both a target and a
base object, versus **815** at the 2026-07-16 run. Functions outside the
compiled+pinned set have no base object and are inherently unprobeable, so
this is the reachable surface, not a shortfall.

## 7. Reproduce

```bash
# in a worktree, AFTER a full build (never `ninja <one>.obj`)
venv/bin/python scripts/unicorn/equivalence_audit.py -j 4 --resume \
    -o ~/tmp/unicorn_audit.csv
venv/bin/python scripts/unicorn/analyze_refresh.py \
    --csv ~/tmp/unicorn_audit.csv --baseline-db <snapshot>.db \
    --report build/45410914/report.json
venv/bin/python scripts/unicorn/final_worklist.py \
    --csv ~/tmp/unicorn_audit.csv --report build/45410914/report.json \
    --out docs/decomp/unicorn_worklist.md
# sensitivity control -- run this before believing any EQUIVALENT
venv/bin/python scripts/unicorn/mutation_sensitivity.py \
    --unit default/CharBones --symbol '?ClearBones@CharBones@@QAAXXZ'
```

⚠ **Cap workers at 4** (standing user constraint). Under box contention
Unicorn fails to mmap its translator buffer; `batch_to_db.py` and
`equivalence_audit.py` now recycle pool workers (`maxtasksperchild`) because
a `ProcessPoolExecutor` turns that into a `BrokenProcessPool` that destroys
the whole run — measured at 600/1045 units with zero output. The audit now
checkpoints per unit and supports `--resume`.

## 8. Not verified

- **The 23 permuter defects were NOT used as positive controls.** They were
  already reverted in `main` (`843c7e98`) before this tree was built, so they
  would read EQUIVALENT and prove nothing. Whether this harness *would* have
  caught them is untested.
- **`decomp.db` was NOT written.** Verdicts were computed against a private
  copy. Main's DB is shared with a concurrently grinding agent and this run
  is a partial refresh; writing partial verdicts into it would have been
  worse than leaving it stale-but-consistent.
- **`cap_exhausted*` rows are INDETERMINATE and were not adjudicated** — both
  sides hit the emulator instruction cap. (The S4 brief listed
  `cap_exhausted` as a real-bug class; the in-tree record disagrees and wins.
  Treating it as real inflated the worklist by 542 of 870 rows.)
- **The `logic` class remains 0**, as in 2026-07-16: the schedule is
  zero-fill + 0xCD-fill only. DC3's richer typed/hostile-mock schedules are
  ported but still unrun. A deeper *schedule* run — not a broader one — is
  the outstanding work.
- **Out-parameter coverage is untested.** `r4/r5/r6` are 0 (NULL) in the
  batch schedule, so a function whose observable effect is writing through a
  pointer argument faults identically on both sides.
