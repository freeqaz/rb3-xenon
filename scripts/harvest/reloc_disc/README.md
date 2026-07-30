# reloc_disc — relocation-content discriminator for reloc-masked byte twins

**Status: VALIDATED (laneBT5, 2026-07-30).** Recovered from branch `laneAS-B`,
where it had been orphaned. Runs, and measures better than it originally claimed.

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
* `emit_reloc_frag.py --funnels` — **still has no producer.** It wants live
  (unmapped) `cls=="EXACT_AMBIG"` rows. Writing that funnel is the remaining work
  to make the emitter usable; `heldout_reloc.py` covers the already-mapped
  population, which is why calibration was runnable but emission was not.

## Entry points vs library

`relocdisc.py` and `reloclib.py` are **libraries** — no `__main__`, no argparse.
Running `relocdisc.py` prints nothing and exits 0. Its docstring used to
advertise `--heldout`/`--emit` modes it never implemented; corrected by laneBT5.
Runnable: `lblindex.py`, `bodyidx.py`, `heldout_reloc.py`, `emit_reloc_frag.py`,
`collision_adjudicate.py`, `collision_control.py`.

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
