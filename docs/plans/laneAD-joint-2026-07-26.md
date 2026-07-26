# laneAD — the symbol map and splits.txt run to a JOINT fixpoint (2026-07-26)

First lane to own **both** `scripts/target_symbol_map.json` and
`config/45410914/splits.txt`. Base: main `905bceff`, whole-binary strict
**29,998**. Branch `laneAD-joint`, worktree `~/tmp/wt-laneAD-joint`.

**Result: 29,998 → 30,093 = +95, with 0 by-name losses, 0 funclet-class gains
and 0 fake matches removed.** 92 are NAMED mangled symbols and the other 3 are
plain C symbols (`JsonCalloc`, `ctr_decrypt`, `longest_match`) — every gain is a
real, name-paired function. Every splits state passed the whole-file audit
(0 cross-unit overlaps, 0 inversions, 0 duplicate blocks, 0 sectionless blocks).

**The loop converges after one cycle.** That is the strategic answer, and §5
explains why the previously-observed regeneration was an artifact of separate
ownership rather than a property of the channels.

---

## 1. ★ The finding: both lanes were throwing away the other's input

`map_displace_round.py` proves, by reloc-masked byte identity over the whole
11.8 MB image, that a mangled name lives at a retail VA. It then **discards the
proof** whenever `span_predictor.py` says the VA does not PAY:

```
displace round: {..., 'refuse-span-UNPINNED': 23, 'refuse-span-WRONG-UNIT': 7}
```

Those 30 are not wrong. They are **correct map repairs that `splits.txt`
forbids from scoring**, because objdiff can only pair a target symbol against
our obj when the VA's pinned unit is a unit whose obj defines that name.

Symmetrically, `splits_move.py` refuses to move a span it has no name evidence
for. Run alone, **both tools print FIXPOINT with the joint pool untouched** —
and both did, at this lane's base state.

Productionised as **`scripts/harvest/joint_unblock.py`**:

| class | meaning | joint move |
|---|---|---|
| `UNPINNED` | the VA is in no pinned `.text` range at all | **ADD** a range to a unit whose obj defines the name |
| `WRONG-UNIT` | the VA is pinned to a unit that does not define the name, another unit does | **MOVE** the extent from the pinned owner to the definer |

Extents are **exact, never guessed**: `config/45410914/symbols.txt` is dtk's own
carve table for the whole retail image, so a pin is emitted as exactly
`[fn.start, fn.end)` of the retail function containing the VA and can never
bisect a neighbour. A VA that is not a retail function start is refused rather
than snapped blindly.

Claimant choice is tiered so the two can be priced separately:

* **`T_SOLE`** — exactly one of our units defines the name. No choice to make.
* **`T_SPATIAL`** — several definers (a COMDAT is defined by every obj that
  instantiates it; STL templates reach 172). Keep only when **exactly one**
  definer owns a pinned span within `--spatial-window` of the VA. Retail is not
  LTCG-built so `.text` preserves per-TU grouping; this is the same positive
  spatial fact that measured +21/−0 as `map_repoint_round.py`'s discriminator 2.
  Ties are **refused**, never coin-flipped.

## 2. The per-iteration yield curve

| # | channel | action | edits | Δ | running |
|--:|---|---|--:|--:|--:|
| 1 | MAP | displacement + argreg eviction | 2 | **+1** | 29,999 |
| 1 | **JOINT** | 22 ADD + 4 MOVE + 26 map assertions | 26 | **+26** | 30,025 |
| 2 | MAP | 1 (re-insert; see §4 oscillation) | 1 | 0 | 30,025 |
| 2 | SPLITS | scan → 3 proposals, all `n_carved == 0` | 0 | 0 | 30,025 |
| 3 | **JOINT** | UNPINNED pool, tier `T_SOLE` | 59 | **+47** | 30,072 |
| 3 | **JOINT** | UNPINNED pool, tier `T_SPATIAL` | 17 | **+16** | 30,088 |
| 4 | **JOINT** | cascade rescan, widened window | 3 | **+2** | 30,090 |
| 4 | MAP | displace 0, repoint **FIXPOINT** | 0 | 0 | 30,090 |
| 4 | SPLITS | scan → same 3, all `n_carved == 0` | 0 | 0 | 30,090 |
| 5 | MAP | evict 1 argreg-proven mispair | 1 | 0 | 30,090 |
| 5 | **JOINT** | 5 brand-new unit blocks (`refuse-definer-has-no-span`) | 5 | **+3** | 30,093 |
| | | **total** | **116** | **+95** | |

**Read the curve this way: the two single-owner channels contributed +1 of the
+95. The joint channel contributed +94.** Both single-owner lanes were honestly
at fixpoint; the entire yield lived in the seam between them.

Split of the +95: **95 real / 0 funclet-only / 0 fake-removed.** No anonymous
`fn_` or `__unwind$` gains at all — this is the cleanest mix any lane in the
splits/map family has measured, and it follows from the shape: every gain is a
whole COMDAT pinned to the unit that provably defines its mangled name.

## 3. ★ Refusal rates, measured directly (not inferred from a heuristic)

Rather than invent a fifth "risk" ordering, the refusal criteria were measured:

| wave | offered | refused | why |
|---|--:|--:|---|
| joint wave 1 | 30 | 4 | 3 × criterion (1) *(a mapped symbol in the span the claimant's obj does not define)*, 1 spatial-far |
| UNPINNED pool | 96 | 20 | 10 spatial-far, 5 definer-has-no-span, 5 VA-not-a-retail-fn-start |
| cascade | 20 | 17 | same three classes, at the widened window |
| new unit blocks | 5 | 0 | all 5 applied; 3 paid, 2 did not (no loss either way) |

Criterion (1) fired 3 times; criterion (2) *(claimant already strict-100 on the
same mangled name)* and criterion (3) *(nothing carved)* never fired. The four
refusals from wave 1 were re-offered at every later iteration and refused
identically — they are a permanent floor, not a backlog.

### 3.1 ★ T_SPATIAL — the *ambiguous* tier paid BETTER

| tier | pins | gains | hit rate |
|---|--:|--:|--:|
| `T_SOLE` (one definer, no choice) | 59 | 47 | 0.80 |
| `T_SPATIAL` (several definers, spatial winner) | 17 | 16 | **0.94** |

> **This is the fifth consecutive time a presumed risk ordering in this family
> has measured as a payoff ordering only** (laneQ: looseness; laneU: `n_wrong`;
> laneV: claimant distance; laneAC: "already 100% in the wrong unit"; here:
> definer ambiguity). Do not invent a sixth. Measure the refusal rate.

## 4. ★ The loop has an OSCILLATOR — and it is score-neutral

`?Terminate@RndMat@@SAXXZ` @ `0x82553fc8`: `argreg_mispair_scan.py` proves the
body reads **r3** while the signature is `static void(void)` and declares no
argument registers, so it evicted the entry. The very next
`map_displace_round.py` proposed **re-inserting it**, because the eviction freed
the VA and reloc-masked byte identity places our compiled
`?Terminate@RndMat@@SAXXZ` there uniquely in the whole image.

Evict → re-insert → evict is a genuine non-terminating cycle in the alternating
loop. Both legs were measured at **exactly 0** strict either way (the entry is
95%, and its pinned owner `Mat.cpp` does not define the name, so it never paid).

> **Guard for the next run: never let one iteration re-insert a name the same
> iteration's argreg pass evicted.** When the two conflict, byte identity is
> direct machine evidence and the argreg verdict is an inference from a decompiled
> signature — but the argreg FP control (0/12,183) was run over the *strict-100*
> population only, so it says nothing about a 95% entry like this one. Resolve by
> leaving the entry in place and recording the conflict; do not spend a wave on it.

The other forward mispair, `?LoadSongData@MoveMgr@@QAAXXZ` @ `0x82c122d8`
(reads r4+r5, declares only r3, *high* confidence, 26.7%), has no such
contradiction and was evicted — measured at 0, kept for the standing reason that
**leaving proven-wrong entries mapped is what costs other lanes real work**.

## 5. ★★ The regeneration was an ARTIFACT OF SEPARATE OWNERSHIP

laneAC's headline finding was that the splits WRONG-UNIT pool went **7 → 186**
purely because laneAB's map round renamed VAs — "every VA that newly carries a
name is a fresh test of the unit it is pinned to" — and concluded the channel
regenerates whenever the map moves.

**It did not regenerate here.** `splits_move.py scan` returned **3 proposals
before the map round and the same 3 after it** (all three with
`n_carved_in_span == 0`, so dtk refuses them — a structural floor, not work).
This lane made 27 map assertions and 79 splits pins and refilled nothing.

The mechanism is now clear:

> A map rename refills the splits pool **only when the rename lands on a VA
> whose pinned unit is left wrong**. `joint_unblock.py` moves the pin in the
> *same edit* as the rename, so the newly-named VA is already in a unit that
> defines it. **There is no residue to regenerate.**

laneAC's 186 was therefore not a steady-state refill rate — it was the
**accumulated debt of a map lane that had been running for several rounds with
no splits lane behind it.** Once that debt is paid, joint ownership keeps the
balance at zero.

**Practical consequence: this loop is worth running ONCE after any period of
split ownership, and is NOT worth re-running session over session** unless the
map or the compiled objs move for some other reason (a body-port wave creates
new byte-identity claimants and genuinely refills the homing input).

## 6. Drain state at the joint fixpoint

| | base | end |
|---|--:|--:|
| `map_displace_round.py` plan | 1 | **0** |
| `map_repoint_round.py` | FIXPOINT | **FIXPOINT** |
| `splits_move.py scan` WRONG-UNIT | 3 | **3** (all `n_carved == 0`) |
| `joint_unblock.py` (displace-fed) | 30 | **0** (4 permanent refusals) |
| UNPINNED map entries with a definer | 96 | **14** (10 spatial-far, 2 no-span definer that did not pay, 5 not a fn start, less overlap) |
| argreg forward mispairs | 1 | **1** (the §4 oscillator, byte-identity-contradicted) |
| bracketed-hole DEFECTs | 0 | 0 |
| zero-evidence holes contradicted by a named `PARENT_OFFUNIT` run | 0 | 0 |

**Two full alternating cycles.** Cycle 2 yielded 0 from both single-owner
channels and 0 from the joint channel after its own cascade. The loop converges.

## 7. Negative results worth not re-deriving

* **The "191 byte-contradicted map entries" residue is ICF-blocked, not
  splits-blocked.** A worker enumerated all 121 of them that are blocked purely
  by `splits.txt`; filtering to the ones with a *single* byte-identity hit
  leaves 26, of which **24 are ICF-tied** and the last 2 are ICF twins of each
  other at one VA. **0 actionable.** laneAB §5's framing ("cannot pay until the
  owning span is pinned") is true but misleading — the honest statement is
  *cannot pay* **and** *cannot be identified*. Do not fund a wave off the 191.
* **A real tool gap was found and is worth fixing if that pool is ever
  revisited:** `map_repoint_round.py`'s discriminator 2 calls
  `span_predictor.py --only PAYS`, so a name whose *every* byte-identical hit is
  UNPINNED or WRONG-UNIT is **silently dropped** — it appears in no printed
  refusal counter at all. 95 of the 191 vanish this way.
* **The EH-funclet channel over zero-evidence holes did not refill** (laneAC
  drained it to 0; re-measured at 0 here, by both the parent-VA and the
  funclet-VA join). The current 88 named `PARENT_OFFUNIT` runs land inside
  ordinary multi-KB `.text` ranges, not bracketed holes — structurally unrelated
  to the hole shape.
* **The bracketed-hole DEFECT pool is still 0** (841 raw holes in scope, 676
  evidence-bearing, 671 genuine COMDAT scatter, 5 UNPORTED). Independently
  re-derived, not copied from laneAC.

## 8. Verification

**Four subagents.** Two Sonnet read-only scanners (the byte-contradicted joint
pool of §7; the hole/EH re-derivation, which independently reproduced laneAC's
drain state rather than trusting it) and two Opus verifiers, each in its own
worktree from its own cold baseline — verifier 1 also ran the negative control. Every subagent claim was re-checked against the lane lead's own
baseline pickle before landing.

* Verifier 1 reproduced joint wave 1 exactly: 29,998 → 30,025, **+27 gained,
  by-NAME LOST set empty**, audit clean, and all 26 evidence records landed in
  their *predicted* claimant unit (0 landed elsewhere).
* **Negative control (the check is not vacuous).** `claimOK = 26/26`. The
  prescribed donor/neighbour substitution is *degenerate* for 16 of 26 (the ADD
  fills a hole inside the claimant's own region, so the previous span is the
  claimant itself) — reported honestly, with two stronger controls: restricted
  to the 10 genuinely-different substitutions, `wrongOK = 0/10`; exhaustive over
  all 26 names × 891 other units, **202/23,166 = 0.87%**.
* Hand spot-audits against `symbols.txt` confirmed every pin is exactly one
  retail function's extent starting at the mapped VA — swept over all 26 of
  wave 1 with 0 findings.
* Verifier 2 reproduced the pin wave leg-by-leg from its own cold baseline:
  29,998 → 30,025 → **30,088**, scalar and strict-row count agreeing at every
  leg, **63 gained / 0 lost**, all 63 NAMED. It confirmed the tier rates
  (T_SOLE 47/59 = 0.80, T_SPATIAL 16/17 = 0.94) and showed the attribution is
  airtight — 47 + 16 = 63 exactly equals the measured delta, with 0
  unattributable gains — and validated **76/76** pins mechanically against
  `symbols.txt` (0 bisections, 0 off-by-one).

### 8.1 ★ The risk this wave actually carried, and it did not fire

Adding target code to a unit can shift objdiff's **positional** pairing of
anonymous `fn_8XXXXXXX` functions and silently break an already-matched
funclet — a sibling lane measured −13 that way. It was checked directly:

> whole-binary anonymous `fn_*` strict matches **15,456 → 15,456 (+0)**;
> NAMED 14,351 → 14,414 (+63). Every pin-receiving unit is `ANON +0`.

The check is not vacuous: anonymous functions are **15,456 of the 29,998
baseline strict matches (52%)**, so a positional break would have been plainly
visible. The reason it cannot fire here is structural — every pin is exactly one
retail function's extent dropped into a gap, so no *existing* carve boundary
moves and no already-paired sequence is renumbered.

### 8.2 Bookkeeping corrections from verification

* The pins land in **33 distinct units** (the 44 / 15 figures are per-tier block
  counts and overlap).
* Commit `e0cb0b2b` is not pure-splits: it also carries the §4 oscillator
  re-insert (`0x82553fc8` → `?Terminate@RndMat@@SAXXZ`), measured **net 0** in
  isolation. 30,088 is reached by `splits.txt` alone.
* dtk auto-backfilled **71 `.pdata` ranges** alongside the 76 `.text` ADDs, so
  the splits diff is 147 added lines and **0 removed** — the wave is purely
  additive, which is why even the by-`(unit,name)` view shows 0 losses.

## 9. Reusable procedure

```bash
# homing input only needs re-running when the OBJS change (a body-port wave).
scripts/harvest/homing_scan_all.sh $PWD ~/tmp/homing 16 20        # ~2 min

# map leg -- the sidecar is the joint input
python3 scripts/harvest/map_displace_round.py --worktree $PWD \
    --results ~/tmp/homing/merged.json --out ~/tmp/plan.json \
    --pays-only --include-free --break-ties --strict-guard build/45410914/report.json
python3 scripts/harvest/map_rotation_repair.py apply --plan ~/tmp/plan.json \
    --map scripts/target_symbol_map.json

# JOINT leg -- `~/tmp/plan_detail.json.span` is the refused-by-span sidecar
python3 scripts/harvest/joint_unblock.py plan --worktree $PWD \
    --span-detail ~/tmp/plan_detail.json.span \
    --out-moves ~/tmp/mv.json --out-blocks ~/tmp/bl.json --out-frag ~/tmp/fr.json
python3 scripts/harvest/splits_move.py apply  --worktree $PWD --moves ~/tmp/mv.json
python3 scripts/harvest/homing_apply4.py --blocks <bl in h4 form> ...
python3 scripts/harvest/splits_move.py audit  --worktree $PWD    # MUST be clean

touch config/45410914/config.yml && rm -f build/45410914/report.cache
./tools/ninja-locked
# A/B BOTH ways -- by (unit,name) AND by NAME; only by-NAME losses are real
```

The whole-map UNPINNED pool of §2 iteration 3 is generated straight from the map
(every entry outside all pinned ranges whose name some obj defines) and fed to
the same `joint_unblock.py plan`; no homing scan is needed for that channel at
all, because the map entry *is* the assertion.
