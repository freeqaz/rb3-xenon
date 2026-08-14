# The identification frontier, censused — the wall is NOT identification tooling (lane IDENT-1, 2026-08-14)

Five lanes bottomed out on what looked like one wall, from five directions:

| lane | its number |
|---|---|
| SPLITBLOCK-1 | **244 of 246** epilogue-overcarve blocks gated behind the map, not the splitter; 159 anon-head runs at `fuzzy 0.000` |
| PINSRC-1 | **3,218 of 3,310** owned functions have no retail row bearing their name |
| INCOMPLETE-1 | **87.5%** of charged relocation-name pairs have placeholder targets |
| earlier | map names ~**41.7%** of functions; 24,555 of 26,873 backlog pairs have a retail-side placeholder |

All four are **identification coverage** seen from different angles. This lane
censused it directly and the headline is a **negative**: identification tooling
is *not* the binding constraint. It is worth **~0.2% of `total_code`**, and this
lane took most of that.

Baseline `6cb87d90`, ruler `name_check`, `matched 44,374 / total_code
10,320,664 / code% 35.934666`. Tools: `tools/ident_frontier_census.py`,
`tools/ident_body_channel.py`. Queue: `docs/decomp/ident-body-channel-queue-IDENT1.json`.

## 1. Coverage, and the subtraction that matters

`tools/ident_frontier_census.py` self-validates against `report.json`'s own
totals (rows == `total_functions`, bytes == `total_code`) and refuses otherwise.

| | rows | bytes | % `total_code` |
|---|---|---|---|
| NAMED | 28,302 | 6,555,116 | 63.51% |
| PLACEHOLDER | 40,925 | 3,765,548 | 36.49% |

Coverage is **40.88% by rows**, 63.51% by bytes — named rows are 2.5× larger on
average, so the row figure that gets quoted badly understates the *byte*
position.

⛔ **But 40,925 is NOT the backlog, and quoting it is the mistake this lane was
built to stop.** Split by pairing state:

| | rows | bytes | % |
|---|---|---|---|
| `mpn == 100` — **already funclet-paired** | 22,897 | 881,264 | 8.54% |
| `0 < mpn < 100` | 1,492 | 61,488 | 0.60% |
| `mpn == 0` — **the actual frontier** | **16,536** | **2,822,796** | **27.35%** |

★ **The 22,897 is a self-validating control, not an assumption**: it equals
`masked_equal_functions` (22,897) **exactly**. `pair_funclets_by_bytes` is the
only path by which an anonymous target row can pair, so "anonymous **and**
paired" is precisely the funclet byte-signature population. **More than half the
placeholder rows need no name at all.**

## 2. The frontier by channel — where the 16,536 actually go

Classified by whether we hold a reloc-normalized byte-identical body:

| class | rows | bytes | % `total_code` |
|---|---|---|---|
| **0a. RESOLVED bijectively, same unit** (nameable now) | 53 | 7,376 | 0.07% |
| **0b. RESOLVED bijectively, other unit** (needs a pin move) | 156 | 13,788 | 0.13% |
| 1. we hold *a* body, class is **ICF-folded** — identity not establishable by bytes | 1,731 | 140,552 | 1.36% |
| 2. **no base obj** (`auto_*` / no-source) — needs pin+wire first | 9,211 | 1,459,672 | 14.14% |
| 3. **WE DO NOT HOLD THE BODY** | 5,363 | 1,194,012 | 11.57% |

(Post-landing figures; 0a/0b were 73/190 before this lane took 20.)

★★★ **THE REFRAMING. Class 3 is 11.57% of `total_code` and it is ORDINARY
MATCHING WORK WEARING AN IDENTIFICATION HAT.** These rows sit in units that are
already pinned and already compile a base obj. If we compiled a byte-identical
body it would match. Naming them first buys a pairable row at 0% with no
content — `ForceEmit_*`-class metric fitting, explicitly out of bounds. **There
is no identification lever here; there is a body.**

Class 3 by category: engine 2,880 rows / 662,540 B (6.42%) · game 1,421 /
307,964 B (2.98%) · network 1,040 / 218,264 B (2.11%) · thirdparty 20 · sdk 2.

Class 2 is AUTOID-1's territory, already measured **~8.9% attributable-and-portable**
(two-thirds flanked by XDK source we lack or 7-line Quazal map scaffolds). Class 1
is the `_bijection_arbitrary` degeneracy: /OPT:ICF folded N of our names onto one
survivor, so **no amount of byte evidence can say which name belongs there.**

⇒ **Identification is reachable for ~209 functions / ~21,164 B by bijective body
identity, and the rest is unreachable because the identity is ICF-degenerate
(1,731 rows), the unit is not wired (9,211 rows), or we do not have a
byte-identical body (5,363 rows / 11.57%) — the last of which is decomp work,
not identification work.**

## 3. The channel, and why it is not the vein B2 closed

`docs/decomp/identity-transfer/B2-FINDINGS-oracle-wall.md` closed identity
transfer at **0 of 10 fresh TUs**. That channel is **PORT-THEN-LOCATE**: trust an
oracle VA, pin it, hope the body matches. It died on body divergence.

`tools/ident_body_channel.py` is **LOCATE-BY-BODY** and inverts the dependency: a
row is identified *only because* its body already matches. **Body divergence is
therefore the filter, not the wall** — a diverging function yields no candidate
and costs nothing. That is why this channel returns a small, clean answer where
B2 returned a large, empty one.

## 4. The control — three of them, and the first one lied

An identification instrument whose FP rate is unmeasured is the
"instrument that cannot fail" pattern. `autocarve_global_identity.py` shipped
this channel's ancestor with the note *"Its PRECISION IS UNVALIDATED — laneBT5
did not run the `--holdout` mode."* This lane ran it, and then ran the one that
matters.

| control | population | FP |
|---|---|---|
| naive holdout — recover names the map already knows | 12,568 decided | **0.52%** |
| **leave-one-out** — true owner DELETED from supply | 20,936 applicable | **2.76%** |
| **exact landing gate** — same-unit ∧ non-template ∧ ≥128 B | 6,319 applicable | **0.33%** |

⛔ **The 5× gap is the finding: the naive holdout is in the WRONG STRATUM and is
optimistic.** Its population is rows the map already names — enriched for *"the
true owner is among the symbols we compile."* The population we want to name is
enriched for the **opposite**. Every disagreement has one shape:
`?NewObject@A@@` vs `?NewObject@B@@`, `??_E` vs `??_G`, sibling STL
instantiations over pointer-sized `T`. Bijectivity held **only because our supply
lacked the true owner.**

⛔⛔ **And the retail-side multiplicity guard is STRUCTURALLY BLIND to this.**
/OPT:ICF has already folded such a group to **one** surviving address, so
`retail_mult == 1` is exactly what a folded group looks like. The guard that
appears to protect against fold-ambiguity cannot see the dominant error mode.

Gate components, each measured rather than assumed:

| stratum | FP |
|---|---|
| non-template ≥128 B | 0.81% |
| non-template <128 B | 2.66% |
| template ≥128 B | 4.71% |
| template <128 B | **6.99%** |

**Same-unit** is the only constraint independent of bytes (retail has no
whole-program optimization, so TU spatial grouping survives). Measured, it
removes **64.9%** of the false positives — not the assumed all of them.

★ All three controls **can fail and do** (0.33% is 21 wrong of 6,319, not 0).

## 5. Landed: 20 engine names — +19 matched / +5,000 B, fully honest

Pre-registered **+12..+20 matched / +3,000..+6,396 B**; measured:

```
Δmatched     +19  (44374 -> 44393)     Δmasked_equal +0  ⇒ Δhonest +19
Δcode_bytes  +5000 (Δcode% +0.048446pp) Δtotal_code    0
units at 100%  251 -> 251 (0 fell off)  Δfuzzy +0.060754pp
```

**19 of 20 landed at exactly `mpn` 100.000.** `Δmasked_equal = +0`, so every one
is a real named pairing — none of it funclet-signature disclosure. The set is
entirely `src/system/` engine code, **disjoint from `src/band3/`** (lane
GAMEROW-2 concurrent).

⚠ **The metric cannot validate this work and was not used to.** Selection is *by*
byte identity, so a 100% result is circular — exactly as the map's own
`_bijection_arbitrary_comment` warns. The evidence is bijectivity + the 0.33%
control + the same-unit spatial constraint.

⚠ **This deliberately refuses the `_bijection_arbitrary` class** (1,731 anonymous
rows): N of our names onto N retail VAs, where any assignment scores 100 and
establishes nothing. laneAK already banked 1,026 such rows; under `name_check`
each is a **liability**, not a free choice.

### ★ The one near-miss is a finding, not a bad name

`??0CamShotFrame@@QAA@PAVObject@Hmx@@@Z` sits at **70.415**, and the name is
**proven**: the retail body (424 B) is byte-exact against the copy
**`CharMeshHide.obj`** emits, while the other **eleven** TUs emitting this
COMDAT — `CameraShot.obj` among them — compile a **380 B** variant calling
`__savegprlr_28` where retail **inlines** the register saves. **The linker kept
the odd one out.** objdiff pairs within `default/CameraShot`, so it scores our
380 B variant against retail's 424 B body.

⇒ **The same COMDAT does NOT compile identically in every TU, and a per-unit
differ cannot see that.** This is a COMDAT-**variant** divergence, not a
misidentification — and it is a general hazard for any tool that assumes "our
symbol `S`" denotes one body.

## 6. Handed over, not taken

- **10 tier-A rows / 3,232 B in `src/band3/`** pass the same 0.33% gate and were
  left for **GAMEROW-2**: `?Handle@PresenceMgr@@` (908 B),
  `?Poll@TambourineManager@@` (472 B), `??0LocalBandUser@@` (340 B),
  `??0NullLocalBandUser@@` (312 B), `?Handle@ContentLoadingPanel@@` (300 B),
  `??1LessonMgr@@` (292 B), `?CheckLessonCompleteCondition@…` (172 B),
  `?GetTotalCountFromTrainer@LessonMgr@@` (152 B), `??_DLocalBandUser@@` (148 B),
  `??1LocalBandUser@@` (136 B).
- **Deferred sub-gate, NOT landed**: 36 small rows (<128 B, FP 2.66%) / 2,824 B
  and 6 template rows (FP 4.71%) / 1,124 B. Per house doctrine — *an unproven map
  repair is worse than no repair* — these are recorded, not applied.
- **156 tier-0b rows / 13,788 B** are body-proven but live in another unit's
  span; they need a pin move (identity-transfer transport), not a name.

## 7. Known limitation of the tool

Tier A asks "is the claimed name defined in *this unit's* base obj", but the
matching *copy* may come from a different obj (the CamShotFrame case). The name
was still correct there, but the two questions are not the same and a future
run should record the **providing** obj alongside the name.

`src/` did not move, so `tools/native_build_gate.sh` was correctly not run.
