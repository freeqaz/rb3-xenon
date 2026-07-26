# laneAC — splits "holes" and the refilled WRONG-UNIT pool (2026-07-26)

Fourth lane in the splits family (laneQ → laneU → laneV → **laneAC**). Base:
main `585b77dc`, whole-binary strict **29,784**. Branch `laneAC-holes`,
worktree `~/tmp/wt-laneAC-holes`.

**Result: 29,784 → 29,998 = +214, with 0 by-name losses and 0 fake matches
removed** across five waves and three independent channels. **195 of the 214
gains are NAMED mangled symbols** (median 108 B); the other 19 are funclet-class
(100% normalized, ~99.5% raw — real under the headline metric, low information)
and come entirely from the zero-evidence EH channel of §7. Every splits state passed the whole-file audit (0 cross-unit
overlaps, 0 inverted ranges, 0 duplicate blocks, 0 sectionless blocks).

---

## 1. The finding this lane was handed

laneAB's verifier found that **repointing a symbol correctly is necessary but
not sufficient**. `0x824dbeb8` was repointed `??_GTexProc` → `??_GSpotlight` and
still read **0.0%**, because `splits.txt` had carved `0x824DBEB8..0x824DBF08`
out of the *middle* of `Spotlight.cpp`'s span and handed it to `TexProc.cpp`.
objdiff pairs by name, and our `??_GSpotlight` lives in `Spotlight.obj`, so it
could never pair. Merging the hole back read 100.0%.

A **hole** is therefore: a small `.text` range assigned to unit A that sits
*bracketed* inside unit B's territory — B pins a range immediately before it and
immediately after it in address order.

The free per-hole test, needing no build:

> Does the unit the hole is assigned to actually **define** the symbols in it?
> If not, and the enclosing unit does, the hole belongs to the enclosing unit.

`scripts/target_symbol_map.json` gives retail VA → mangled name; our objs are
COMDAT-per-function, so their COFF symbol tables give name → {units that define
it}. The join is the whole test.

## 2. ★ The funnel, measured before anything was applied

| stage | n | why it drops |
|---|--:|---|
| raw bracketed holes, any size | **873** | |
| raw bracketed holes < 1 KB | **819** | the population as scanned at lane start |
| − out of scope (`auto_03_*`, 0x828–0x82C XDK/Quazal) | −8 | hard-skipped |
| **in scope** | **811** | |
| − no mapped symbol in the hole at all | −147 | the symbol-map channel has no opinion |
| **has mapped evidence** | **664** | |
| − **assignee DOES define the symbol** | **−579** | ★ **genuine COMDAT scatter — correct as pinned** |
| − no definer anywhere (UNPORTED) | −3 | needs source, not splits |
| − a *third* unit defines it, not the enclosing one | −6 | an ordinary WRONG-UNIT move, not a hole merge |
| **assignee does NOT define it, enclosing one DOES** | **76** | DEFECT |
| − refused by criterion (1) name-paired symbol claimant lacks | −0 | |
| − refused by criterion (2) claimant already 100% on that name | −0 | |
| − refused by criterion (3) `n_carved_in_span == 0` | −0 | |
| **genuinely workable** | **76** | |

**The funnel does not collapse — but the durable number is the other column.**
Of the 664 evidence-bearing holes, **579 (87.2%) are real COMDAT scatter**;
only **76 (11.4%)** are carving defects. Retail genuinely interleaves TUs, so
"a small range inside another unit's territory" is *usually correct*. A lane
funded off the raw 819 would have been funded ~11x over.

### 2.1 Reconciling the handed-over "475"

The handoff counted **475**. That figure additionally requires the hole to be
**contiguous on both sides** (`prev.end == hole.start` and `hole.end ==
next.start`). Dropping that requirement — a hole separated from its enclosing
unit by a small unpinned gap is just as much a defect — grows the pool 475 → 819
and the workable set **34 → 76**. **The contiguity requirement was
over-restrictive and cost more than half the vein.** (Other framings measured
for the record: bracketed and contiguous on *one* side = 600; any sub-1KB range
whose two neighbours are both foreign, without requiring them to be the *same*
foreign unit = 1831, which is just the general WRONG-UNIT population.)

## 3. Wave 1 — the 76 holes

Applied as one batch with `splits_move.py apply` (atomic donor-shrink +
claimant-give; `homing_apply4.py` cannot express this, it only adds and refuses
on overlap). 76 applied, 0 refused, audit clean.

**29,784 → 29,860 = +76. 76 by-name gains, 0 by-name losses, and every single
gain is a NAMED mangled symbol** — no anonymous `fn_`, nothing ≤48 B. That is a
**1.00 substantive-gain-per-hole hit rate, the cleanest ratio any wave in the
splits family has measured** (laneV's best salvage wave was also 1.00 but
mixed-size; laneU's tiers ran 7.3 / 2.3 / 0.48 with 158 of 565 gains being
funclet-class).

The gains are exactly the predicted shape: `??_G` vector-deleting destructors
landing in their real owner (`??_GStreakMeter` → `StreakMeter`, `??_GCharacter`
→ `Character`, `??_GCharClipSet` → `CharClipSet`, `??_GCharFaceServo` →
`CharFaceServo`), the `CharClip` sort/heap family → `CharClipGroup`, STLport
instantiations, and real ctors (`??0Lod@Character@@`, `??0RndDir@@`).

An independent Opus verifier reproduced the headline exactly from its own cold
baseline in its own worktree (29,784 → 29,860, 76 gained, **LOST set empty**),
and agreed with `report.json`'s `measures.matched_functions` scalar at both legs.

**Five donors held the hole as their ONLY `.text` range** and were auto-dropped
by `apply`'s empty-block guard (`HamRegulate`, `MoggClip` — a bare-basename
duplicate of the real `system/synth/MoggClip`, `MoveParent`,
`SkeletonRecoverer`, `StreamRecorder`). Each carried **0** strict matches, so
nothing was repaid. Without that guard dtk would have emitted sectionless ~86 B
stub objs and `objdiff-cli report generate` would have hard-failed.

**Cascade: none.** Re-running the funnel on the landed state gives **0**
workable holes, and widening the size cap from 1 KB to 64 KB adds **0** more at
any stage. The hole channel is drained in one wave.

## 4. ★ The WRONG-UNIT pool REFILLED — 7 → 186

laneU signed off with `WRONG-UNIT 7`, and those 7 *were* its 7 refusals. A fresh
`splits_move.py scan` at this lane's base returns **186 proposals**.

**The cause is laneAB's symbol-map repair round.** Its +118 came from
displacements, evictions, ICF tie-breaks and 25 newly-named ICF-folded VAs — and
**every VA that newly carries a name is a fresh test of the unit it is pinned
to.** The Spotlight case that opened this lane is the same mechanism seen from
the splits side.

> **The WRONG-UNIT channel is not a one-shot vein: it regenerates whenever the
> symbol map moves.** Re-run `splits_move.py scan` after every map-repair round,
> not just after body-port waves.

68 of the 186 were the hole wave's own spans. The other 118 (115 after dropping
`n_carved_in_span == 0`) were split three ways and verified concurrently by
three Opus workers, each from its own cold 29,784 baseline in its own worktree.

| batch | moves | pre-refusals | Δ isolated |
|---|--:|--:|--:|
| B1 | 39/39 | 0 | **+40** |
| B2 | 38/38 | 0 | **+38** |
| B3 | 38/38 | 0 | **+39** |

**0 pre-refusals across all 115**, and the checks were exercised, not vacuous:
B3 substituted the *donor* as claimant as a negative control and got
`donorOK == 0` on all 38 spans while `claimOK == span_mapped`. Every batch
reported 0 by-name losses and needed no bisection.

Replayed onto the hole wave: **112 of 115 applied**; 3 refused as *"span not
fully inside one donor `.text` range"* (`ContextChecker → MetaPanel`,
`CharNeckTwist → TransAnim`, `RealGuitarTrackWatcherImpl → KeyboardTrackWatcher
Impl`) because wave 1 had already redrawn those donor ranges. Plus 1 proposal
the post-wave-1 rescan newly exposed.

**29,860 → 29,975 = +115.** Isolated sum was 40+38+39 = 117; the 2-count
shortfall is precisely the 3 superseded spans, so the waves are additive.

Three more donors were auto-dropped after losing their only `.text` range
(`FlowOutPort`, `OggMap`, `SkeletonHistory`), all with 0 strict matches.

## 5. Wave 3 — cascade

A rescan on the landed state exposed 5 newly-visible proposals (landed moves
change the scanner's claimant-adjacency ranking, so clustering re-derives).
5/5 applied, 0 refused: **29,975 → 29,981 = +6**.

## 6. Lane totals

| wave | what | moves | Δ |
|---|---|--:|--:|
| 1 | 76 bracketed holes merged back | 76 | **+76** |
| 2 | refilled WRONG-UNIT pool (B1+B2+B3, replayed) | 112 (+1) | **+115** |
| 3 | cascade rescan | 5 | **+6** |
| 4a | zero-evidence holes, EH channel, no prior 100% | 3 | **+5** |
| 4b | zero-evidence holes, EH channel, prior 100% under wrong unit | 6 | **+8** |
| 5 | EH-channel cascade | 4 | **+4** |
| | **total** | **207 landed / 3 superseded** | **+214** |

**29,784 → 29,998.**

* ★ **The symbol-map channel (waves 1–3) paid 197/197 NAMED** — zero anonymous
  `fn_`, zero funclet-class, nothing to price down. That is the best mix in the
  family (laneU 407/565 = 72% NAMED with 148 funclet-class; laneV 236/252 = 94%).
  Median gain size 108 B, largest 908 B.
* The EH channel (waves 4–5) paid **19, all funclet-class** — price separately.
* Lane total **214 = 195 substantive + 19 funclet-class**.
* **0 by-name losses.** The 10 by-(unit, name) losses are all anonymous `fn_`
  that migrated to their true owner and still match there
  (`fn_822A89F8`/`fn_822AABF8`/… HamCamTransform → Gem, `fn_8238A520`/`fn_8238A548`
  Morph → CharEyes, `fn_8256EAB0`/`fn_8256EB3C` ContextChecker → MetaPanel).
* **0 fake positional matches removed** — unlike laneQ/laneU, which repaid 4 and
  33. Every span this lane ceded scored 0 strict under its donor, so the whole
  +197 is new coverage rather than an honesty trade.

## 6b. ★ A third channel: EH funclets over the ZERO-EVIDENCE holes

178 of the 864 holes carry **no mapped symbol at all**, so the symbol-map
channel has no opinion. `funclet_cascade_rank.py` does: it flags
`PARENT_OFFUNIT` when a function's `__unwind$` EH funclets are pinned to a
different unit than the parent, and **when the parent is a NAMED mangled symbol
its unit is ground truth**.

Intersecting the 91–92 named `PARENT_OFFUNIT` runs against the zero-evidence
holes gave **9 holes**, and the two channels agreed **9/9**: in every case the
parent's own pinned unit is exactly the hole's **enclosing** unit, never the
assignee (`??1CharSleeve` under a `CharDriver` hole enclosed by `CharSleeve`;
`??1TambourineManager` under `DepthBuffer3D`; `??1KeyboardTrackWatcherImpl`
under `RealGuitarTrackWatcherImpl`; `??0MusicLibraryStore` under
`BandStorePanel`; …). **The EH channel never once said "keep with the
assignee".** These spans contain zero mapped symbols, so refusal criterion (1)
cannot structurally fire — the class is safe by construction, as laneU also
found.

Split by prior exposure and measured as two sub-waves:

| wave | holes | already reading 100% under the wrong assignee | Δ |
|---|--:|--:|--:|
| 4a | 3 | 0 | **+5** |
| 4b | 6 | 6 | **+8** |
| 5 | 4 (cascade) | — | **+4** |

### ★ 4b is the wave laneU priced at a LOSS

laneU's wave G moved 27 funclets that already read 100% in the wrong unit; 8
failed to re-match in their true owner and it **cost −8**, funded for
correctness. The identical shape here **paid +8 with 0 losses** — all six
re-matched honestly under the true owner.

> **"Already 100% in the wrong unit" is not automatically a fake match.** It is
> only a fake if the true owner's obj does not also emit that code. Measure the
> sub-wave; do not price it as honesty debt up front. This is the fourth time in
> the splits family that a heuristic presumed to indicate risk turned out to
> indicate nothing of the sort.

## 7. Drain state

Final scans on the landed state:

| | start | end |
|---|--:|--:|
| raw bracketed holes (any size) | 873 | 873 |
| holes with mapped evidence | 664 | 686 |
| — of which genuine COMDAT scatter | 579 (87.2%) | **681 (99.3%)** |
| — **workable DEFECT** | **76** | **0** |
| — third-party-definer holes | 6 | **0** |
| `splits_move.py scan` WRONG-UNIT proposals | 186 | **8** |
| — of which `n_carved_in_span == 0` (nothing to score) | | 3 |
| zero-evidence holes contradicted by a named `PARENT_OFFUNIT` run | 13 | **0** |

**All three channels are drained.** The hole population *grows* slightly as moves
create new bracketed sub-ranges, but its defect fraction goes to zero: after the
lane, **99.3% of evidence-bearing holes are genuine COMDAT scatter**. The 178
zero-evidence holes and 5 UNPORTED holes are not splits problems — they need map
coverage or source (BinDiff / rb3-Wii / DC3 oracle), exactly as laneQ §5.6 and
laneV §6 concluded for their residuals.

## 8. Findings that revise the priors

1. **★ The WRONG-UNIT channel regenerates from map repair.** 7 → 186 with no
   splits change at all, purely because laneAB renamed VAs. Schedule a
   `splits_move.py scan` after every map round; it is ~5 min and needs no build.
2. **★ Only ~11% of "holes" are defects; ~87% are genuine COMDAT scatter.**
   This is the durable ratio and it is the reason the free definer test matters:
   without it, a lane funded off the site count would have applied ~700 wrong
   moves. Site counts are not opportunity counts.
3. **The contiguity requirement in the handed-over 475 was over-restrictive** —
   relaxing it more than doubled the workable set (34 → 76) at identical
   quality (0 losses either way).
4. **MIDDLE-is-safest holds again**, now at a fourth independent N. Every hole is
   MIDDLE by definition and 76/76 landed clean.
5. **A fourth "risk" heuristic would have been wrong too.** laneQ found
   looseness, laneU found `n_wrong`, laneV found claimant distance — all payoff
   orderings, never safety orderings. This lane measured the refusal rate
   directly instead of inventing one: **0 refusals in 194 landed moves**
   (criteria (1) and (2) never fired once across the whole lane).
6. **Symbol-map holes pay in NAMED symbols, not funclets.** 197/197 NAMED is the
   best mix in the family; the hole shape structurally selects for whole COMDATs of a class
   whose other COMDATs are already pinned next door.

7. **★ The EH-funclet channel is the right tool for zero-evidence holes**, and it
   agreed with the enclosing-unit hypothesis 13/13 (9 + a 4-hole cascade) while
   never once contradicting it. It is narrow (13 of 178) but 100% precise.
8. **★ "Already 100% in the wrong unit" is not automatically a fake match** —
   see §6b. laneU measured this class at −8; here it measured **+8**.

## 9. Subagents

**Five.** One Opus independent verifier for wave 1 (reproduced +76 exactly from
its own cold baseline), three Opus batch workers for the refilled WRONG-UNIT
pool (B1/B2/B3, own worktrees at `~/tmp/wt-laneAC-b{1,2,3}`, own cold
baselines), and one Sonnet read-only recon over the residual hole classes — **it
is the recon that opened the EH channel of §6b**, worth +17 on its own, and it
also independently confirmed all 6 THIRD-PARTY calls the batch waves had made
and sampled the SCATTER verdict at 7/8 clean (the one flag being `MetaPanel`
factory boilerplate, the known REVS-macro ambiguity, not a proven error). Every
subagent claim was re-verified by the lane lead against its own baseline pickle
before landing — the replay measured +191 against the workers' isolated +117
sum plus wave 1's +76, and the 2-count difference is fully explained by the 3
superseded spans.

## 10. Reusable procedure

```bash
# funnel (no build needed) -- the decisive test is the definer join
python3 scripts/harvest/splits_move.py scan --worktree $PWD --out ~/tmp/scan.json
# holes: bracketed sub-ranges whose assignee does not define their symbols
#   -> moves of the form {donor: assignee, claimant: enclosing, start, end}
python3 scripts/harvest/splits_move.py apply --worktree $PWD --moves ~/tmp/moves.json
python3 scripts/harvest/splits_move.py audit --worktree $PWD   # MUST be clean
touch config/45410914/config.yml && rm -f build/45410914/report.cache
./tools/ninja-locked
# A/B BOTH ways -- by (unit,name) AND by NAME; only by-NAME losses are real
```
