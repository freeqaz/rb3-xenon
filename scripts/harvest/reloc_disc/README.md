# reloc_disc — relocation-content discriminator for reloc-masked byte twins

**Status: VALIDATED (laneBT5) + EMITTER RUNNABLE AND PAID (laneBU4), 2026-07-30.**
Recovered from branch `laneAS-B`, where it had been orphaned. Runs, and measures
better than it originally claimed. laneBU4 built the last missing input
(`livefunnel.py`), gave the live channel its own negative control
(`livecontrol.py`), added the cut that control demanded (`--scope-unique`), and
landed a measured +3 honest. See §laneBU4 below — including the finding that the
live pool is now essentially drained.

## Why it exists

`objdiff-cli report generate` hardcodes `functionRelocDiffs=None`, so relocation
differences are **masked**. Two functions whose only difference is which symbol a
`bl`/`lis`+`addi` points at are therefore byte-identical to the report. This is
the mechanism behind the *at-100% defect class* (bodies with wrong constants
scoring a clean 100.0%), and it is why byte-locating certain bodies is
unreliable **by construction** — notably the `OBJ_SET_TYPE` / `StaticClassName`
family, where the one discriminating callee is itself a relocation.

This tool recovers what was zeroed and checks consistency:

```
base reloc symbol NAME --resolve--> VA    ==?  target operand VA
                       --decode---> bytes ==?  bytes living at the target VA
```

Channels: **MAP** (name→VA via `target_symbol_map.json` + `symbols.txt`),
**CONTENT** (`??_C@_0…` string literals, `__real@`/`__xmm@` constants, `??_R0`
RTTI type descriptors decoded to bytes and compared against `lbl_<VA>` content),
**NAME** (direct compare for helpers).

## Measured precision (laneBT5, current map, whole tree)

Leave-one-out over real map entries, with the **alphabetical tie-break as the
control** on the identical population:

| population | n | precision | baseline (alphabetical) |
|---|---|---|---|
| EXACT_AMBIG pool | 3733 | — | 36.99% |
| DECISIVE tier | 519 | **87.48%** | 44.12% |
| ship gate `DECISIVE ∧ (content ∨ (scope_ok ∧ unk==0)) ∧ 33≤size≤68` | 169 | **99.41%** | 44.97% |
| same gate, excluding ARBITRARY-truth rows | 165 | **99.39%** | 45.45% |
| same gate at size ≤32 | 84 | 84.52% | — |
| same gate at size >68 | 53 | 84.91% | — |

laneAS-B originally calibrated the gate at 95.62% and **refused** the ≤32 B and
>68 B bands (it measured 83.93% / 85.37%). Both refusals reproduce almost
exactly. **The size band is load-bearing — do not widen it.**

## Pipeline (this is what was missing)

The tool set died on a branch largely because two required inputs had **no
producer anywhere**, so nothing could be run as shipped. Order matters:

```bash
WT=/path/to/worktree ; S=$WT/scripts/harvest/reloc_disc ; O=~/tmp/relocdisc

# 0. FULL BUILD FIRST. Every step below reads compiled base objs and dtk target
#    asm. A fresh worktree reflinks main's possibly-DIRTY objs, so an unbuilt
#    tree silently scans stale bytes.
cd $WT && ./tools/ninja-locked 2>&1 | tee ~/tmp/rb3_build.log

python3 $S/lblindex.py $WT $O/lblidx.json      # ~2 s   -> 63,704 lbl VA->bytes
python3 $S/bodyidx.py  $WT $O/bodyidx.pkl      # ~8 s   -> 152,718 names (ICF index)

python3 $S/heldout_reloc.py --worktree $WT --lblidx $O/lblidx.json \
        --bodyidx $O/bodyidx.pkl --out $O/rows.json          # ~13 s
```

* `bodyidx.pkl` — **had no producer**; `bodyidx.py` (laneBT5) reconstructs it.
  Contract inferred from `relocdisc.decide()`'s R2 rule: stripped name → set of
  masked-body hashes, so two competing callee names sharing a hash are ICF twins
  and their offset cannot discriminate. It is optional (R2 only).
* `emit_reloc_frag.py --funnels` — **producer built by laneBU4**
  (`livefunnel.py`). The emitter is now runnable end to end; see the laneBU4
  section below for the live-pool census, the channel's own negative control,
  the extra cut it required, and the measured result.

```bash
python3 $S/livefunnel.py --worktree $WT --out $O/funnel_live.json      # ~40 s
python3 $S/livecontrol.py --worktree $WT --lblidx $O/lblidx.json \
        --bodyidx $O/bodyidx.pkl --out $O/control.json                 # NEG arm
python3 $S/livecontrol.py --worktree $WT --lblidx $O/lblidx.json \
        --bodyidx $O/bodyidx.pkl --no-ablate --out $O/positive.json     # POS arm
python3 $S/emit_reloc_frag.py --worktree $WT --lblidx $O/lblidx.json \
        --bodyidx $O/bodyidx.pkl --funnels $O/funnel_live.json \
        --scope-unique --out $O/frag.json --census $O/census.json
```

## Entry points vs library

`relocdisc.py` and `reloclib.py` are **libraries** — no `__main__`, no argparse.
Running `relocdisc.py` prints nothing and exits 0. Its docstring used to
advertise `--heldout`/`--emit` modes it never implemented; corrected by laneBT5.
Runnable: `lblindex.py`, `bodyidx.py`, `heldout_reloc.py`, `emit_reloc_frag.py`,
`collision_adjudicate.py`, `collision_control.py`, and (laneBU4)
`livefunnel.py`, `livecontrol.py`.

`livecontrol.py` doubles as a library: `ship_gate()` is the emitter's exact gate
factored into a reusable predicate, and `scope_unique()` is the laneBU4 cut —
`emit_reloc_frag.py` imports the latter rather than re-describing it, so the
control can never drift from the thing it certifies.

## Bug fixed by laneBT5

`Disc.score()` did `v = self.cmp_one(...)` (which returns a **tuple**
`(verdict, channel)`) and then compared `if v == "AGREE"`. A tuple never equals a
string, so `score()` structurally always returned `agree=0, contra=0`. The
shipped entry points were unaffected — they use `decide()`, which unpacks
correctly — so the 99.41% figure above is untainted. But any *new* caller of
`score()` silently got all-zero evidence, which is exactly what happened when
laneBT5 first built the collision channel on top of it.

## Collision channel (laneBT5 addition)

`collision_adjudicate.py` answers a different question: *a branch places mangled
name N at address B, main places it at A — which is right?* One side is a map
mispair, and byte compare cannot separate them (that is why they collided).

Funnel over 332 branches / 13,859 collision names / 14,243 rows:

```
no_base_comdat            5238   (name has no compiled COMDAT — the availability gate)
under_2_targets_available 7898   (rival VA not in a pinned unit with asm)
DECISIVE                   310   -> 278 confirm main, 32 say main is wrong
ELIM_ONLY                   76
TIE                         85
ALL_CONTRA                 252
```

`collision_control.py` is the **negative control** for this channel: pair a
confirmed, uncontested main entry against a byte-twin decoy, where truth is A by
construction, and count false flips.

| gate | n | precision | false flips |
|---|---|---|---|
| winner agree ≥1 | 2197 | 98.73% | 28 |
| winner agree ≥2 | 813 | 98.28% | 14 |
| **winner agree ≥3** | 536 | **99.63%** | 2 |

agree==2 is the weak band (≈95.7%); **agree≥3 is the defensible cut.**

`collision_verdicts_decisive.json` holds all 310 DECISIVE rows. Six of them are
`agree≥3` branch-wins asserting a main-map defect — including two `SetType`
bodies (`RndPropAnim`, `PracticeSection`), which independently corroborates the
known `OBJ_SET_TYPE` mispair cluster, and a coherent quartet of `PAPAVCamShot`
STL sort instantiations (`__insertion_sort`/`__make_heap`/`sort_heap`/`sort`)
that main splits across `MoveMgr` but which must be co-located — the branch puts
all four in `CameraManager`.

**Those 6 were applied and priced, and came out metric-neutral: Δmatched 0,
Δmasked_equal 0, Δhonest 0, Δcode% 0.000000** (same-split A/B, re-split both
legs, worktree baseline 40925 / 1517 / 39408 / 34.479504). Per the standing
pricing rule (discard when *both* axes are ≈0) the map edit was **not landed**;
the verdicts are kept here as evidence so the adjudication is not lost again.

### ⚠ Trap: a map DELETE cannot be measured without a re-split

`obj_target_symbol_renamer` rewrites the target obj's `fn_<addr>` symbol to the
mapped mangled name. Once an obj carries that name, **removing the map row cannot
revert it** — there is no `fn_` symbol left to rename. A fragment containing
deletions therefore reads a false Δ0 unless you `touch config/45410914/config.yml`
to regenerate virgin target objs. Both A/B legs must be re-split. laneBT5 hit
this: the first measurement showed 0 because the deletions were silent no-ops,
and the re-split A/B independently confirmed 0 for the real reason.

## laneBU4 — the live-pool funnel, its own control, and the emitted fragment

### 1. The missing producer (`livefunnel.py`)

The emitter reads exactly four fields per funnel row — `cls` / `unit` / `va` /
`size` — and **re-derives the candidate class itself**. So the producer is a
*nomination* pass: "at which target VAs is identification currently blocked by a
reloc-masked byte tie?" Its grouping is deliberately byte-identical to the
emitter's (same supply filter, same dedup-by-stripped-name), so a nominated row
is never silently dropped downstream for an unrelated reason.

`LIVE` = the VA has no entry in `target_symbol_map.json`. Unit universe is
`D.unit_iter` (which excludes `auto_*` carves) **to match the population
`heldout_reloc.py` calibrates on** — emitting over a wider universe than was
calibrated would void the measured precision.

### 2. The live pool is small, and its size profile is inverted

```
target fns scanned        51,349
  already mapped          21,751   (skipped: not live)
  no byte class at all    29,106
  unique byte match          204
  LIVE EXACT_AMBIG           288   <-- the whole pool
```

Size bands of the live pool: **151 at ≤32 B, 121 at >68 B, only 16 in the
33–68 B ship band** (5.6%, versus 14.4% in the 3,724-row calibration pool). The
ship band therefore covers almost none of the live pool, and ≤16 rows is the
hard ceiling before any tier or collision filtering. Anyone budgeting this vein
should price it as a handful of functions, not a wave.

Census of the 288: `ALL_CONTRA` 96, `TIE_WITH_EVIDENCE` 84, `ELIM_ONLY` 37,
`DECISIVE` 36, `NO_EVIDENCE` 27, `NO_DISCRIM_RELOC` 8.

### 3. Why the 99.41% could NOT be inherited (`livecontrol.py`)

`heldout_reloc.py` only admits a VA whose truth name **exists as a code symbol
in that unit's base obj**, so its population guarantees by construction that the
correct answer is among the candidates. Its precision is conditional on
**truth-present**. The live pool guarantees no such thing — those VAs are
unmapped precisely because earlier passes could not name them, so the true owner
may not be in the supply at all. When truth is absent every candidate is wrong
and the only correct behaviour is to REFUSE.

**Truth-ablation control:** take the calibration population and delete the true
candidate, requiring ≥2 to remain. Truth is now absent by construction. Any row
that still passes the ship gate is a **FALSE PLANT**. `--no-ablate` runs the
positive arm through identical gate code, so the two arms are strictly
comparable.

Result at the inherited gate, in the 33–68 B ship band:

| arm | n | result |
|---|---|---|
| POS (truth present) | 170 | 99.41% precision (reproduces BT-5's 169/99.41%) |
| **NEG (truth absent)** | 338 | **48 false plants = 14.20%** |

The gate does *not* reliably refuse when truth is absent. ⚠ The "100% refusal"
seen out of band is an artifact, not evidence: `shipA`/`shipB` both require
`inband`, so out-of-band rows can never pass the gate by construction.

### 4. The false plants are family-structured — and tier A is the worst

| truth family | n (in-band) | plants | rate |
|---|---|---|---|
| `StaticByteCode` | 22 | 20 | **90.91%** |
| `ByteCode` | 21 | 16 | **76.19%** |
| `_Copy_Construct` | 45 | 8 | 17.78% |
| `ClassName` | 153 | 3 | 1.96% |
| `_Param_Construct` | 17 | 0 | 0.00% |
| other | 80 | 1 | 1.25% |

Two tiny sibling families supply 36 of the 48 plants. Mechanism:
`?ByteCode@C@@UBAEXZ` and `?StaticByteCode@C@@SAEXZ` are **same class, different
method**, so the token that fires `scope_ok` is the *class name the rival also
carries* — evidence-shaped non-evidence. Contrast `?ClassName@A@@` vs
`?ClassName@B@@` (same method, different class): scopes genuinely differ, 1.96%.

★ **Tier A — the "content" channel — is the worst offender under truth-absence
(36 of 48 plants) despite scoring 100.00% on the truth-present arm.** That
failure mode is completely invisible to the inherited calibration, which is the
whole argument for building a per-channel control.

### 5. The laneBU4 cut: `--scope-unique`

First formulation (**FAILED, recorded so nobody retries it**): require the
pick's scope-token *set* to differ from every rival's. Removed **0 of 48**
plants — `_scope()` includes the method token, so `{ByteCode,C}` never equals
`{StaticByteCode,C}` and same-class siblings never compare equal.

Second formulation (adopted): require that some **AGREE-producing overlap token
is absent from every rival's scope**.

| cut | NEG plants (of 338) | POS n / correct / precision |
|---|---|---|
| inherited ship gate | 48 (14.20%) | 170 / 169 / 99.41% |
| **+ scope-UNIQUE** | **9 (2.66%)** | **133 / 133 / 100.00%** |
| + tier A only | 36 | 34 / 34 / 100.00% |
| + tier B only | 12 | 136 / 135 / 99.26% |

It **strictly dominates**: 39 of 48 false plants removed *while* truth-present
precision rises to 100.00% and 78% of recall is kept. Exposed as an opt-in
`--scope-unique` flag so the landed, validated default is unchanged.

### 6. The size band survives a third independent challenge

Since the new cut improves both arms, the fair question is whether it
rehabilitates the refused bands. Measured (**not acted on**):

| band | POS precision, gate | POS precision, gate+UNIQ |
|---|---|---|
| ≤32 B | 87.12% | 88.80% |
| 33–68 B | 99.41% | **100.00%** |
| >68 B | 84.91% | 89.19% |

**It does not.** Out of band stays ~89% under the better cut — a third
independent confirmation (laneAS-B, laneBT5, laneBU4) that **the band is
load-bearing; do not widen it.** The two bands also fail for *different*
reasons: in-band the danger is truth-absent false plants (fixed by UNIQ);
out-of-band it is ordinary misidentification under truth-presence (not fixed).

### 7. Measured result — this one PAYS

4 rows shipped, 3 after the name/VA collision guard, all `?ClassName@…@@UBA?AV\
Symbol@@XZ` — the `OBJ_SET_TYPE`/`StaticClassName` family this tool was built
for. All 4 survive scope-UNIQUE.

```
0x8263e7d0  ?ClassName@UGCPurchasePanel@@UBA?AVSymbol@@XZ
0x826cb170  ?ClassName@UIPanel@@UBA?AVSymbol@@XZ
0x828236d8  ?ClassName@UIProxy@@UBA?AVSymbol@@XZ
```

Same-split A/B in a private worktree, `symbols.txt` restored and
`config.yml` touched on **both** legs, `report.cache` removed on both:

| leg | matched | masked_equal | honest | code% |
|---|---|---|---|---|
| A (baseline) | 40936 | 1517 | 39419 | 34.497240 |
| B (fragment) | 40939 | 1517 | 39422 | 34.498596 |
| **Δ** | **+3** | **0** | **+3** | **+0.001356** |

Both axes moved, so it is not discarded under the standing rule. +3 is only just
above the ~2-function split-churn floor, so it is banked on **attribution, not
magnitude**: all three symbols are individually present in `report.json` at
`fuzzy=100.0`, 48 B each, in `UGCPurchasePanel` / `TrainerPanel` / `UIProxy` —
an exact 1:1 with the three inserted rows.

A prediction that failed, worth recording: these are byte-twins *under masking*,
so it looked likely they would land in `masked_equal_functions` and net Δhonest
0. `masked_equal` did not move at all.

### 8. State of the vein

The emitter is now runnable and the live pool is **essentially drained**: 288
rows in, 3 functions out. The bottleneck is not the discriminator's precision
(100.00% in-band under the new cut) but **pool supply** — only 16 live rows fall
in the shippable size band. Future yield must come from enlarging the live pool
(more pinned units with base objs), not from loosening the gate.
