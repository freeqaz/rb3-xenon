# Lane BP-6 — MULTI / UNIQUE-ICF content-join refill (2026-07-29)

**Result: matched 40,870 → 40,885, masked_equal 1,509 → 1,509.
Honest Δ(matched − masked_equal) = +15, 0 regressions.** Branch `laneBP6`,
worktree `/home/free/tmp/wt-bp6`.

Sanctioned refill of lane G's channel
(`docs/plans/laneG-multi-content-join-2026-07-24.md`), which closed with:

> Wave C is dry, so the MULTI content-join vein is **drained at the current obj
> set**. … it only refills when a body-port / source-wiring wave changes what we
> compile — re-run it after every such wave.

## What was measured

Full-tree `homing_scan_all.sh` over **all 1,094 objs** (never a subset —
`project_homing_scan_2026-07-24`) at the 40,870 tree state:

```
NOMATCH 103,883 · ALL-MAPPED 44,250 · MULTI 27,120 · UNIQUE-ICF 4,823 · UNIQUE 972
```

**Surprise #1 — the pools did not refill.** Lane BP-1's gen4 sweep reported
MULTI 27,098 / UNIQUE-ICF 4,820; five landings and +140 matched functions later
they read 27,120 / 4,823 (**+22 / +3**). The intervening work changed *which*
functions match, not the reloc-masked shape of the residue. So the pool sizes in
BP-1's report are not a measure of available yield — they are ~stationary, and
quoting them as "undrained residue" over-states the vein by ~3 orders of
magnitude. The actual refill was **65 resolved occurrences = 25 distinct
(name, VA) pairs**.

**Surprise #2 — occurrences ≠ entries.** The 65 resolutions collapse to 25
distinct pairs; the rest are the *same* COMDAT emitted into many objs
(`StaticClassName@RndEnviron` recurs in 17 TUs, `RndFur` 13, `RndParticleSys` 9).
Counting occurrences would have inflated this wave 2.6×.

## Acceptance rule + rejections

Map-free evidence only (`--no-sym`: `str`/`vfstr`/`f32`/`f64`), the tool's
honesty clause (rivals *positively excluded at a confirmed slot*), a hard veto on
the 13 phantom classes from
`docs/plans/lane-bp4-map-contradiction-adjudication-2026-07-29.md` §6 (**0
proposals tripped it**), and no repoints.

**Rejected: 1,422** (729 `NO-WINNER` + 679 `TIE` + 14 contested ICF VAs) + 1
repoint + the whole `sym` class. Full table and per-entry evidence in
`~/tmp/bp6_map_fragment_justification.md`.

### ★ Refutation — `sym` evidence must now be OFF, not merely trust-gated

Lane G's negative-results table says "always `--trust-file`". Held-out
`--validate` at this tree state says that is no longer sufficient:

| leg | RESOLVED-STRONG | RESOLVED-SYM | demonstrated errors |
|---|---|---|---|
| `--trust-file` (lane G recipe) | 2697/2759 = 97.75% | 779/1083 = **71.93%** | **7** `MISS/TRUTH-AGREE` |
| `--no-sym` (this lane) | 2327/2382 = 97.69% | — | **0** (all 55 misses `MISS/TRUTH-CONFLICT`) |

`MISS/TRUTH-AGREE` = the map's label is itself content-corroborated and the
resolver still picked elsewhere — a demonstrated resolver error, of which lane G
measured exactly zero. Mechanism: `sym` slots can place a CONFLICT on the *true*
candidate and change which rival gets excluded, so the weak class **corrupts the
strong class** rather than only adding its own errors. Cause: intervening map
repairs grew the trust set 2,191 → 3,359, so `sym` fires 4.4× more often
(247 → 1,083 decisions).

Also worth recording: the same audit shows the map is much healthier than lane G
found it — **14,074 names checked → 3,359 corroborated, 53 CONTRADICTED**, down
from lane G's 423 contradicted.

## Applied

5 new `.text` pins (all gap-fills into existing units, 0 new units) +
19 reveal-only names = **24 map entries**.

| unit | pin | symbol |
|---|---|---|
| `CharIKFingers.cpp` | `0x823B40F8..0x823B4234` | `?SetType@CharIKFingers@@` |
| `InstrumentDifficultyDisplay.cpp` | `0x82324BF0..0x82324C48` | `?Init@InstrumentDifficultyDisplay@@` |
| `ScoreDisplay.cpp` | `0x82320848..0x823208A0` | `?Init@ScoreDisplay@@` |
| `band3/meta_band/LockStepMgr.cpp` | `0x825AB060..0x825AB094` | `?Name@LockResponseMsg@@` |
| `system/bandobj/DialogDisplay.cpp` | `0x8232ABF8..0x8232AC50` | `?Init@DialogDisplay@@` |

Fragment (map owner applies): `docs/plans/lane-bp6-multi-content-fragment-2026-07-29.json`.
**This lane does not touch `scripts/target_symbol_map.json`** — BP-7 owns it.
Checked at report time against main's map (last changed `fb83d49d`): 0 VA
collisions, 0 name collisions, all 24 pure additions.

## Measurement provenance

Both legs in the same worktree, same split state, each after
`git checkout -- config/45410914/symbols.txt` + `rm -f report.cache` +
`touch config.yml` (so both legs are same-split; the split-churn floor is ~2 fns
per `project_bandexe_read_traps_2026-07-29`).

| leg | matched | masked_equal | honest |
|---|---|---|---|
| A baseline (= main) | 40,870 | 1,509 | 39,361 |
| B pins + fragment | 40,885 | 1,509 | 39,376 |
| B rebuild (confirm) | 40,885 | 1,509 | 39,376 |
| C pins ONLY (= this commit, map reverted) | 40,870 | 1,509 | 39,361 |

### ★ The delta is carried entirely by the fragment, not the pins

Leg C decomposes it: **the 5 `.text` pins alone are worth +0.** All +15 comes from
the 24 map names. This is the documented mechanism — a pinned range whose dtk
target symbol has no mangled name in `scripts/target_symbol_map.json` stays an
anonymous `fn_<addr>` that objdiff cannot pair (CLAUDE.md: "without a map entry a
pinned game TU reads a false 0%"). **Consequence for landing: committing this
branch on its own yields +0.** The pins and the fragment must land together, and
the +15 is only realised once the map owner applies
`lane-bp6-multi-content-fragment-2026-07-29.json`.

`measure_delta.py`: **NET +15 (gained 15, regressed 0)**, zero fuzzy
regressions. **masked_equal did not move**, so none of the +15 is masked-twin
credit — the MULTI-pool risk the pricing rule warns about did not materialise.
All 15 gained functions are fragment entries (no cascade), so attribution is
exact. Flip rate 15/24 = 62.5% (lane G: 73.6%) — a correct name only flips a
function when the rest of the unit's carve also lines up.

## Remaining pool — what would actually crack it

The residue is **not** 27k of workable candidates:

- **17,927 `ALREADY-HOMED`** — our name is already on a hit. Zero available yield.
- **12,455 `NO-EVIDENCE`** — the function references no string and no FP constant
  at any masked slot. This is the honest structural floor of *callee-side*
  content joining and it is the bulk of the pool. Needs a categorically different
  discriminator: `.pdata` prolog/epilog shape, or **caller-side** identity (who
  calls this VA — `caller_side_invert.py` exists and is unexercised here).
- **679 `TIE` / 729 `NO-WINNER`** — the only genuinely *interesting* remainder.
  `TIE` is where the documented-but-**unimplemented** `op` evidence class would
  pay: masking zeroes whole 4-byte instructions including the opcode, so two
  "byte-identical" retail functions can execute different opcodes at a masked
  slot. That is a free, map-free exclusion signal and it is exactly what
  distinguishes tied candidates. Implementing `op` is the highest-value next
  step on this channel. `NO-WINNER` mostly means our source diverges so the true
  home is not in the hit set — a body-port target, not an identification target.
- **14 contested VAs** — genuine ICF folds; the map holds one name per VA and we
  decline to pick.

Do **not** re-run this channel until a body-port / source-wiring wave changes the
obj set: the +22/+3 pool movement above shows five landings were not enough to
refill it materially.
