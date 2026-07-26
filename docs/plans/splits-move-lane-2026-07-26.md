# Splits MOVE lane (laneQ) — wrong-unit `.text` spans, 2026-07-26

Lane goal: land the **deferred COMDAT-scatter splits moves** from lane N, then
generalise the shape into a repeatable scanner.

Baseline at lane start: main `e7662cdb`, whole-binary strict **28,238**.

---

## 1. What a MOVE is, and why it needed a new primitive

`config/45410914/splits.txt` pins per-object `.text` (and `.pdata`) address
ranges. dtk carves the retail XEX into one target `.obj` per pinned unit, and
objdiff compares that target obj against the obj we compile from source.

When a range is drawn too wide it swallows machine code belonging to a
*different* TU. Two consequences:

1. The real owner's unit never receives that code, so its functions can never
   match — the count is depressed.
2. objdiff pairs anonymous (`fn_8XXXXXXX`) target functions **positionally**.
   Swallowed foreign code therefore manufactures **fake 100% matches**: a target
   function that is not the function we compiled, scoring 100 because the two
   happened to line up.

The repair is a **MOVE**: shrink donor unit A's range and hand the freed span to
claimant unit B. It is only ever correct as **both halves at once**. Half a move
either leaves an overlap — dtk's `validate_splits` hard-fails, two units may not
own one address — or a hole.

`scripts/harvest/homing_apply4.py`, the existing splits splicer, can only *add*
ranges and refuses on overlap. That is exactly why lane N could not land its
moves and why 33 of its ranges were skipped at landing.

**Consequence for measurement:** removing a fake match *lowers* the strict count
while *raising* honesty. Every A/B in this lane is therefore checked
**unit-agnostically** — by name as well as by (unit, name) — because a move that
relocates a function between units shows up as a false loss in a naive
by-(unit, name) diff.

## 2. `scripts/harvest/splits_move.py`

Three subcommands.

### `scan`

Joins two ground truths:

* `scripts/target_symbol_map.json` — retail VA → MSVC mangled name (~21.7k
  functions).
* the COFF symbol tables of every obj we compile — name → {units that define
  it}. Our objs are COMDAT-per-function, so an inline/template symbol
  legitimately has many definers; all of them are valid owners.

Classification of each mapped VA that falls in a pinned range:

| class | meaning |
|---|---|
| `OK` | the range's owner is (one of) the units defining the name — pin is right |
| `WRONG-UNIT` | the name is defined by our objs, but by **none** of them as the owning unit — pin is suspect, the definers are the claimants |
| `UNPORTED` | no obj of ours defines the name — no opinion |
| `UNPINNED` | VA in no range at all — a `homing_gen4`/`homing_apply4` job, not ours |

Contiguous `WRONG-UNIT` functions sharing one (donor, claimant) pair are
clustered into a single span. A cluster is **refused** if any correctly-owned
(`OK`) donor function falls inside it — that would be a blind span across a gap,
which may contain a third TU.

Whole-binary result: `OK 12,945 · WRONG-UNIT 765 · UNPORTED 304 · UNPINNED 592`
→ **544 raw proposals** (427 MIDDLE, 49 WHOLE, 40 TAIL, 28 HEAD).

### `apply`

Atomic shrink-donor + give-to-claimant, spliced **textually** so every unrelated
line of splits.txt stays byte-identical. Handles the MIDDLE case (donor range
splits into two remnants). Appends to an existing claimant block in sorted
position; a brand-new claimant gets exactly **one** fresh block — never a
duplicate, because a unit emitted twice makes dtk carve two objs for one source.
After splicing it re-runs the full audit and **refuses to write** on any finding.

### `audit`

Whole-file hygiene: cross-unit `.text` overlaps, inverted ranges (`end <= start`),
duplicate unit blocks. Run after every splits change. Main was clean before this
lane and is clean after.

## 3. The scanner's confirmed false-positive classes

Measurement is the arbiter; the scanner proposes, it does not decide.

* **Extension assumption (FIXED).** `header_matches` assumed `.cpp`, so
  `system/obj/DataFlex.c` — retail built some C libraries as C++ — could never
  own its own range, and the whole, correctly-pinned, already-matching DataFlex
  block surfaced as a bogus WRONG-UNIT proposal. Now tries `.cpp/.c/.cc/.cxx`.
* **COMDAT ubiquity (FIXED).** Definition alone is a weak claimant signal: an
  STL or inline symbol is defined in *every* obj that instantiates it
  (`DataArray::FindSym` in 12 of ours), so taking the first definer picks
  arbitrarily. `HamMove`'s span was routed to `GemPlayer`, whose nearest pin is
  4.4 MB away, when `MessageTimer` — also a definer — begins 4 bytes past the
  span end. **Spatial adjacency is the discriminator**: retail lays a TU's
  COMDATs out contiguously, so the true claimant's existing pin is almost always
  immediately adjacent. `scan` now picks the definer whose nearest pinned range
  is closest to the span.
* **Cluster overrun (FIXED).** A cluster could run past the end of the donor
  range containing its start, across a gap, and into a *different* range —
  possibly one the claimant already owns. `apply` correctly refused such spans
  ("not fully inside one donor .text range"), but `scan` should not emit them;
  the span is now clamped to the containing range.
* **Map mispair.** The map entry itself is wrong, so the VA's name is misleading.
  Map repair is single-owner and out of scope for this lane — these are reported,
  never applied.
* **ICF ambiguity.** Retail folded two identical COMDATs; the surviving copy's
  VA is genuinely attributable to either TU.
* **Claimant is not a real TU.** Sanity-check the claimant against
  `config/45410914/objects.json` and the source tree before trusting a proposal.

The decisive tell for a bad move is a **real name-paired loss for zero gain**:
mangled names pair by name, not position, so losing one is a genuine regression,
not an honesty trade.

## 4. Corroborating signal: `PARENT_OFFUNIT`

`scripts/harvest/funclet_cascade_rank.py` flags a parent function
`PARENT_OFFUNIT` when its `__unwind$` EH funclets resolve to a different unit
than the parent. That is a splits defect, not a codegen defect, and it is an
*independent* channel from the symbol-map join.

The two channels agree on boundary after boundary — the EH channel
independently re-derived 13 of lane N's 16 spans. Where they differ, the EH
channel proposes a *wider* span (from the donor's range start); this lane
preferred lane N's tighter spans, per the hygiene rule "pin tight sub-ranges,
never one span across a gap".

Census: 42 `PARENT_OFFUNIT` parents whole-binary, 41 in scope. 17 of them are
single funclets **already reading 100%** — those are existing fake matches, i.e.
honesty debt rather than count upside.

## 5. Results

### 5.1 The deferred lane-N moves — 12 of 16 landed, **+41**

Lane N's `laneN-frames` branch was diffed against `e7662cdb`. Its splits are an
*older* base (main has since been union-merged), so the diff was analysed for
spans laneN attributes to unit B that main currently has inside a range owned by
a different unit A. Result: **16 MOVE candidates, 0 ADD-ONLY, 0 multi-conflict**
— every one of laneN's `.text` edits is a genuine reassignment, all fully
contained in exactly one main range, split 8 START / 8 MIDDLE.

Four independent Opus batches (A–D) verified all 16 individually in isolated
worktrees. Their per-move numbers reproduce the aggregate exactly.

| donor → claimant | Δ | verdict |
|---|---:|---|
| SaveLoadManager → ProfileMgr | +14 | KEEP |
| Player → SyncGameStartPanel | +8 | KEEP |
| SessionMgr → MetaPerformer | +6 | KEEP |
| LessonMgr → ClosetMgr | +3 | KEEP |
| GemTrackResourceManager → FingerShape | +2 | KEEP |
| PracticeSection → CharNeckTwist | +2 | KEEP |
| NewAwardPanel → UIList | +2 | KEEP |
| MetaMusic → MicClientMapper | +1 | KEEP |
| BeatMatcher → PhraseDB | +1 | KEEP |
| TourProgress → TourPerformer | +1 | KEEP |
| Cache → LocaleOrdinal | +1 | KEEP |
| CharCache → ThreeDSoundManager | 0 | KEEP (honesty; 94.0% → 99.9%) |
| DrumMixDB → BeatMatcher | −1 | **DROP** |
| InlineHelp → FilterQueue | −1 | **DROP** |
| DeployCountTracker → FileMergerOrganizer | −1 | **DROP** |
| SongData → SongCollision | −1 | **DROP** |

**28,238 → 28,279 (+41).** 42 by-name gains, 0 real losses, 4 relocations,
**1 fake match removed** (`fn_82356208`).

The four DROPs share one signature: the span holds a **mapped, name-paired**
symbol that was a genuine byte-identical match under the donor, and the claimant
obj does not define it at all. Mangled names pair by *name*, not position, so
losing one is a real regression — not an honesty trade. In each case the symbol
also names its true owner outright: `~DrumMixDB`,
`vector<InlineHelp::ActionElement>::_M_erase`, `_Rb_tree<TrackerPlayerID>…
DeployCountTracker`, `TickedInfo<String>`. Project memory had independently
flagged both SongCollision ("stays gated") and FileMerger ("isolated-flip
net −7"); the measurements agree.

`BeatMatcher.cpp` @0x82793050 vs `GameGemDB.cpp` was already resolved in main's
favour and was not re-litigated.

**Honesty caveat on the largest gains.** The 28 functions gained by the three
biggest moves are 40-byte EH unwind funclets. They score 100% *normalized* (the
metric the 28,238 baseline uses) but ~99.5% *raw* — only relocation
normalization makes the base-side `bl` equal the target's. They are real gains
under the headline metric, and the runs being contiguous and complete (14/14,
8/8, 6/6) is the signature of a correct TU boundary rather than lucky overlap —
but they are low-information matches. Note also that the `.pdata` sub-ranges
appearing in the diff are **dtk back-fill derived from our own `.text` pin**, so
they are *not* independent corroboration of a move.

### 5.2 Generalised scanner harvest — 9 landed, **+17**

First wave from `splits_move.py scan` beyond laneN's hand-derived set: the top
10 high-confidence proposals, verified individually (batch E1).

| donor → claimant | Δ |
|---|---:|
| GuitarController → BaseGuitarTrackWatcherImpl | +3 |
| FreestylePanel → GemPlayer | +2 |
| CharCuff → CharClipDriver | +2 |
| GemTrainerPanel → RGTrainerPanel | +2 |
| SelectDifficultyPanel → SetlistMergePanel | +2 |
| Band → Performer | +2 |
| AccTrainerCategoryCond → AccTrainerListCond | +2 |
| CharIKSliderMidi → CharDriverMidi | +2 |
| Game → StoreSongSortNode | 0 (honesty-only) |
| DataFlex.c → DataFlex.cpp | **DROP** — scanner bug, see §3 |

This wave is **purely additive**: every donor's count was unchanged, so none of
these over-wide ranges was manufacturing a fake positional 100% — they were only
starving the true owner.

**Running lane total: 28,238 → 28,296 (+58).** 59 by-name gains, 0 real losses,
1 fake match removed.

### 5.3 MIDDLE splits — 6 landed, **+30**, and the class verdict reverses

The dense-MIDDLE tier (≥4 wrong records, density ≥0.5): the claimed span sits in
the **interior** of the donor's range, so applying it splits the donor into two
sub-ranges.

| donor → claimant | Δ |
|---|---:|
| RealGuitarTrackWatcherImpl → KeyboardTrackWatcherImpl | +9 |
| CharDriverMidi → CharMirror | +5 |
| PropAnim → Rot | +4 |
| SongStatusMgr → ViewSetting | +4 |
| AccomplishmentDiscSongCond → AccomplishmentSongListCond | +4 |
| BandLabel → BandHighlight | +4 |

6/6 applied, 6/6 paid, perfectly additive, 0 real losses. **28 of the 30 gains
are NAMED symbols** — whole coherent method runs (`KeyboardTrackWatcherImpl::
Poll/OnHit/OnPass/Jump/…`, `BandHighlight::Copy/Load/PostLoad/StaticClassName`).
Named symbols pair by name, not position, so these cannot be positional
artifacts.

**MIDDLE splits are the SAFEST class, not the riskiest.** This lane initially
deprioritised them for fear of donor-range inversion, overlap, or a third TU
hiding in the gap; none materialised. The structural reason: a MIDDLE span is
bracketed by donor code on **both** sides, which constrains the boundary far more
than an EDGE trim, where it is pinned on one side only and can drift. All six
donors also scored 0 strict inside the ceded span, so there was no fake-match
credit to give back — score-positive and honesty-neutral at once.

The residual pool is **425 MIDDLE proposals**. On this evidence it should be
funded at the same or higher priority than the EDGE tiers — which inverts the
ranking this lane started with.

Caveat: payoff is capped by whether the claimant's source is already ported —
the RealGuitar span held 17 carved functions but only 9 flipped, the rest being
unported/divergent bodies. Expect a MIDDLE move to bank the ported fraction now
and leave the remainder as newly-**visible** near-miss work.

### 5.4 EDGE tail — 10 landed, **+36**

The remaining high-confidence EDGE proposals. Best payer was the **loosest**
span in the whole pool: `PatchSelectPanel → VoiceoverPanel`, 2060 B / 23 carved
/ 8 flagged, **+15** — it is simply PatchSelectPanel over-running into
VoiceoverPanel's contiguous body, whose pin starts 4 bytes past the span end.
**Looseness is not a proxy for risk.** Also `FlowValueCase → CharWeightSetter`
+6, `CameraManager → WaitingUserGate` +5, `AccDiscSongCond →
AccSongFilterConditional` +3, `Player → FadePanel` +2 (as two sub-moves),
`HamMove → MessageTimer` +2, `ContentMgr_Xbox → File_Win` +1, `MoveAsyncDetector
→ FileCache` +1, `PracticeSection → CharMeshHide` +1.

### 5.5 Funclet + content channels — 18 landed, **+20**

Six moves from `PARENT_OFFUNIT`, twelve from the round-5 content worklist.
`DepthBuffer3D` was carved into **six tight sub-spans across three claimants**
(Singer ×4, FilterQueue, ColorPalette) rather than one blind span — the exact
third-TU-in-the-gap hazard the hygiene rule exists for.

Two worklist items remain residual, both structurally out of this lane's reach:

* `IdentityInfo.cpp` 0x825C2300-0x825C23AC — **zero** entries in
  `target_symbol_map.json`, so the scanner has no opinion. Needs BinDiff or the
  rb3-Wii oracle.
* `ColorPalette.cpp` 0x826AAF78-0x826AB314 — 2 mapped symbols, one legitimately
  ColorPalette's and **already 100%** (the worklist's "emits ~0" note is stale
  for this unit), the other a bare STL template instantiation whose "definer" is
  an ODR/ICF coincidence.

**`.pdata`-only moves are NOT a lever — do not generalise them.** This wave's
brief posited a `.pdata`-only defect (PartLauncher's funclets `.pdata`-pinned to
AccomplishmentSetlist). Both halves of the premise are false: the cited `.pdata`
entries point at AccomplishmentSetlist's *own* `.text`, and the real defect is a
plain `.text` move. `splits_move.py` contains zero references to `pdata`, yet
every move's diff shows `.pdata` sub-ranges rewritten — that is dtk back-filling
`.pdata` from `.text` ownership. Fix the `.text` and the `.pdata` follows.

### 5.6 Lane total

**28,238 → 28,382 (+144).** **55 moves landed, 8 refused.** 148 by-name gains,
**0 real losses**, **4 fake matches removed**, 22 relocations.

| wave | moves | Δ |
|---|---:|---:|
| laneN deferred | 12 | +41 |
| scanner EDGE (first) | 9 | +17 |
| scanner MIDDLE | 6 | +30 |
| scanner EDGE (tail) | 10 | +36 |
| funclet + content | 18 | +20 |
| **total** | **55** | **+144** |

Every wave was perfectly additive, and every splits state passed the whole-file
audit with 0 overlaps, 0 inversions, 0 duplicate blocks.

The 4 removed fake matches are all anonymous `fn_` positional artifacts:
`fn_82670B0C` and `fn_82670B34` were EH funclets of a `HamSupereasyData` parent
scoring 100% inside `AssetProvider` (a funclet of a HamSupereasyData function
cannot be an AssetProvider function — the 100% came from coincidental byte
identity of boilerplate funclets under positional pairing; they now read
94.0 / 67.2 in their true owner, honest and improvable). `fn_82356208` and
`fn_826FB2D8` are re-pairing artifacts in a donor's *remaining* range.

## 6. Residual pool and how to work it

Re-scanned after all 55 moves landed, with the three scanner fixes in:
`OK 13,027 · WRONG-UNIT 665 · UNPORTED 322 · UNPINNED 592` → **497 proposals
remaining**, overwhelmingly MIDDLE. Run `splits_move.py scan` to regenerate; it
is cheap and idempotent, and worth re-running after every wave because landed
moves change the claimant-adjacency ranking.

Ranked by what the measurements actually showed:

1. **The MIDDLE bulk — highest priority.** §5.3 measured this class at 6/6,
   +30, zero losses. Filter by evidence density (`n_wrong / n_carved_in_span`)
   and work downward; the tier used in §5.3 was ≥4 wrong, density ≥0.5. The next
   ranks are visible in the current scan: `ContextChecker → MetaPanel` (14
   flagged), `MeshDeform → Rnd` (14), `VocalPlayer → CommonPhraseCapturer` (11),
   `RealGuitar/Keyboard`-shaped watcher pairs, `ByteGrinder → SynthSample`.
2. **17 `PARENT_OFFUNIT` rows already reading 100%.** Honesty-only repairs: they
   will cost count while removing fakes. Fund for correctness, not the number.
3. **Ranges with no map coverage at all** (e.g. `IdentityInfo.cpp`) are outside
   this lane's reach by construction — route them to BinDiff or the rb3-Wii
   oracle, not to a splits move.

Two things that turned out **not** to be selection criteria:

* **Looseness is not risk.** The loosest span in the pool (2060 B, 23 carved, 8
  flagged) was the single best payer at +15. Do tighten a cluster to its flagged
  run when a *correctly-owned donor function* sits inside the span — but a low
  flagged/carved ratio on its own is just unported claimant source, not danger.
* **MIDDLE is not risk** — see §5.3. It is the safest class.

The real refusal criterion, learned from all 8 refusals: **a mapped, name-paired
symbol inside the span that the claimant's obj does not define.** That is a
guaranteed regression and the tell is usually in the symbol's own name.

## 7. Rules this lane operated under

* Audit cross-unit overlaps and inverted ranges across the **whole file** after
  every change — a move that overlaps silently mis-carves another unit.
* Pin **tight sub-ranges**; never one span across a gap.
* Appending to an existing unit header is gap-fill; emitting a **duplicate
  block** for a unit is never correct.
* `.pdata` sub-ranges are **dtk back-fill** derived from the `.text` pin — they
  appear in the diff after a rebuild but are *not* independent corroboration of
  a move.
* Map repair is single-owner and was not touched.
* `auto_03_*` units and 0x828–0x82C (XDK vendor + Quazal) are out of scope.

---

# laneU — draining the WRONG-UNIT residual (2026-07-26)

Follow-on lane. Start: main `88a92ad5`, strict **28,382**, residual **497
proposals** (overwhelmingly MIDDLE) left by laneQ. Branch `laneU-moves2`.

Method change vs laneQ: laneQ verified **one move per worktree**. That was the
right call while the primitive was unproven, but a splits-only change rebuilds
in **39 s** (dtk re-split + report; our compiled objs are untouched), so laneU
verified **whole evidence tiers as single batches** with bisect held in reserve.
It was never needed: every batch was perfectly additive.

## Tiering

The laneQ selection filter (`n_wrong >= 4`, density >= 0.5) matches exactly
**one** proposal in the residual — laneQ drained it. laneU re-tiered by raw
evidence count instead:

| tier | filter | n |
|---|---|--:|
| A | `n_wrong >= 3` | 31 |
| B | `n_wrong == 2` | 40 |
| C | `n_wrong == 1` | 427 |

## Waves

| wave | what | moves | Δ | running |
|---|---|--:|--:|--:|
| A | `n_wrong >= 3` | 31 | **+227** | 28,609 |
| E | rescan-after-A, newly visible | 4 | +4 | 28,613 |
| F | EH-funclet channel (`PARENT_OFFUNIT`, unmatched > 0) | 9 | +17 | 28,630 |
| G | EH-funclet honesty repair (already-100% in wrong unit) | 21 | **−8** | 28,622 |
| B | `n_wrong == 2` | 40 | +92 | 28,714 |

### Wave A — the evidence count is the whole ranking

31/31 applied, 0 refused, **+227**: 142 NAMED by-name gains against 0 NAMED
losses. The gains are coherent whole-method runs of the claimant class
(`CommonPhraseCapturer::OneTrackCompletedPhrase/Reset/HasPlayedWholePhrase/…`,
`TrackerManager` ctor/dtor/`SetTracker`/`ConfigureQuestGoal`,
`CharGuitarString::Poll/Save/Copy/Handle`, `RndEnviron::AddLight/RemoveLight/
ReclassifyLights`, `GemTrainerPanel::Draw/ShouldLoop/AddBeatMask/…`). Named
symbols pair by name, so none of this can be positional artifact.

22 anonymous positional matches were removed in the process — fake 100%s, an
honesty gain, and already netted out of the +227.

### Waves F and G — the EH channel is independent and cheap

`funclet_cascade_rank.py` flags `PARENT_OFFUNIT` when a parent's `__unwind$`
funclets are pinned to a different unit than the parent. When the **parent is a
NAMED mangled symbol**, its unit is ground truth and the funclets must follow.
laneU mechanised that into moves: take each contiguous funclet run pinned to the
kid unit, clamp it to the containing donor range, and move it to the parent's
unit. Every generated span contains **zero mapped symbols**, so the refusal
criterion (a name-paired symbol the claimant does not define) cannot fire — this
class is structurally safe.

* **F** (9 runs whose funclets are currently *unmatched*): +17, all 40–88 B EH
  funclets. Real under the normalized headline metric, ~99.5% raw — priced
  separately as low-information.
* **G** (21 runs whose funclets already read **100% in the wrong unit**): 27
  funclets moved, 19 keep matching honestly in their true owner, **8 do not** —
  those 8 were fake positional matches. **Cost −8, funded for correctness.**

Residual `PARENT_OFFUNIT` after F+G: only anonymous-parent rows, where neither
side's unit is corroborated by a name. Not actionable from this channel.

### Wave B — two-evidence tier, real bodies not funclets

40/40 KEEP, no bisect. +92 measured in isolation and **+92 again** when replayed
on top of A/E/F/G — perfectly additive. 96 by-name gains (60 NAMED), of which
only **9** are 40-byte EH funclets: this tier is real-body-dominated. 4 more
anonymous fakes removed.

### Wave C — the single-evidence bulk, and it is the cleanest tier of all

427 proposals with `n_wrong == 1`, split three ways and verified concurrently in
three isolated worktrees. **420 KEEP / 7 DROP, +201.**

| batch | KEEP | Δ isolated | Δ replayed on top of A/B/E/F/G |
|---|--:|--:|--:|
| C1 | 141/143 | +65 | +65 |
| C2 | 141/143 | +75 | +75 |
| C3 | 138/141 | +63 | +61 (2 spans already taken by wave E) |

**All 201 gains are NAMED mangled symbols — zero anonymous.** Single-evidence
was expected to be the weakest tier; it is instead the *purest*. Nothing in it
is positional, nothing was a fake match being repaid, and it lost nothing.

The intuition that low evidence density means low confidence is wrong for the
same structural reason looseness was: `n_wrong` counts how many symbols in the
span our objs happen to *define*, which tracks how ported the claimant's source
is — not how sure the boundary is.

## laneU results

**28,382 → 28,915 (+533).** 471 moves landed, 7 refused, over 6 waves.

| wave | moves | Δ |
|---|--:|--:|
| A — `n_wrong >= 3` | 31 | +227 |
| B — `n_wrong == 2` | 40 | +92 |
| C1/C2/C3 — `n_wrong == 1` | 420 | +201 |
| E — rescan cascade | 4 | +4 |
| F — EH funclets, unmatched | 9 | +17 |
| G — EH funclets, honesty repair | 21 | −8 |
| **total** | **525 attempted / 471 landed** | **+533** |

Composition of the 565 by-name gains:

* **407 NAMED** (378 of them >48 B — real bodies, coherent whole-method runs).
* **158 anonymous**, of which **148 are <=48 B EH-funclet-class** — real under
  the normalized headline metric, ~99.5% raw, low information. Price them
  separately: **the substantive gain is ~417, the funclet-class gain ~148.**
* **0 NAMED losses** across every wave.
* **33 anonymous fake positional matches removed** (honesty gain, already netted
  out of the +533; 8 of them deliberately bought in wave G).

## The vein is drained

Re-scan after the last wave:
`OK 13,685 · WRONG-UNIT 7 · UNPORTED 322 · UNPINNED 592` → **7 proposals**,
and those 7 *are* the 7 refusals. The WRONG-UNIT class went **665 -> 7**.
Anything further must come from a different channel (`UNPINNED 592` is
`homing_gen4`/`homing_apply4`'s pool; `UNPORTED 322` needs source, not splits).

## Findings that revise the priors

1. **`n_wrong` (evidence count) is a payoff predictor, not a safety predictor.**
   Δ per move: A 7.3, B 2.3, C 0.48. Refusal rate is flat-to-inverse: A 0/31,
   B 0/40, C 7/420 (1.7%). Rank by `n_wrong` to bank fastest, but do not treat
   the low tier as risky — it was the cleanest.
2. **MIDDLE-is-safest HELD at 20x the N.** laneQ measured it at 6/6. laneU
   landed ~400 MIDDLE moves; of 7 refusals all 7 are MIDDLE, which is 1.9% of
   the MIDDLE population — no worse than any other class, and every wave was
   additive. The finding stands; keep MIDDLE at top priority.
3. **A NEW refusal class** (found twice, independently, by C1 and C2): *the
   claimant already owns a byte-identical 100% symbol of the same mangled name*.
   Ceding a second same-named COMDAT gives the target obj two functions with one
   name; objdiff's name pairing splits them and the pre-existing 100% dies. This
   is COMDAT ubiquity (§3) surfacing on the *claimant* side rather than as a
   wrong claimant pick. Pre-filter: refuse any proposal whose evidence name
   already reads 100% in the claimant unit.
4. **`n_carved_in_span == 0` is a reliable pre-screen** for spans that slice the
   interior of a carved function; dtk refuses them outright ("ends within
   symbol"). Filter at scan time.
5. **`.pdata` remains a derived view**, confirmed again: 15 donor blocks were
   left holding *only* dtk-back-filled `.pdata` after a WHOLE move, and that
   `.pdata` was meaningless — see the tool fix below.

### Tool fix landed in `splits_move.py`

A WHOLE move can consume a donor's **only** `.text` range. The block survives as
a bare header plus its back-filled `.pdata`; dtk emits a sectionless ~86-byte
stub obj and **`objdiff-cli report generate` hard-fails** with `Invalid COFF/PE
section headers` — no `report.json` at all, so it is a build stop, not noise.
Batches C1 and C2 hit it independently on 17 donors.

* `apply` now drops any block left with no real section (`.pdata` does not
  count, being derived). 15 blocks were auto-dropped landing wave C.
* `audit` reports **empty blocks** as a third finding class beside cross-unit
  overlaps and duplicate blocks.

### Still out of reach

`IdentityInfo.cpp` and `ColorPalette.cpp` are unchanged from laneQ §5.6 — no map
coverage, so the symbol-map channel has no opinion. Route to BinDiff or the
rb3-Wii oracle. The residual `PARENT_OFFUNIT` rows after F+G are all
anonymous-parent, where neither side's unit is corroborated by a name.
