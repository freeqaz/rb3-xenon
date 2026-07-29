# rb3-xenon decomp — state & live veins (2026-07-20)

**Current: 40,730 strict-matched functions** (`build/45410914/report.json`,
`match_percent_normalized == 100.0` exactly). Denominator is the whole TU5 XEX
(~69k functions).

> ⚠ **OPEN INTEGRITY QUESTION (2026-07-29) — read before quoting this number.**
> laneBE closed a +560 different-unit gap-absorption channel after finding the
> flips were 32-byte static-init **guard-clear cleanups**: our symbol clears
> `RndAnimatable`'s guard, the retail function clears an **unrelated** one. Same
> instruction shape, different object — and `functionRelocDiffs=none` **masks the
> differing relocation**, so it scores 100.0%. Same class as the known
> `StaticClassName`/`Type()` family (453 members, one 22-instruction body,
> distinguishable only by the string operand).
> **RESOLVED 2026-07-29 by laneBH** (`docs/plans/reloc-correspondence-audit-2026-07-29.md`,
> tool `scripts/harvest/reloc_correspondence.py`). Whole-binary census of all
> functions at 100.0, reported as a band between the permissive and conservative
> reading of the weakest oracle:
>
> | | permissive | conservative |
> |---|--:|--:|
> | evidenced (corresponding + no-relocs) | **65.5%** | **43.8%** |
> | **DIVERGENT** | **5.2%** | **2.7%** |
> | UNDECIDABLE | 27.7% | 51.8% |
> | unpaired / shape | 1.6% | 1.6% |
>
> ⇒ **The count is substantially sound.** The headline means: ~17,000–26,000
> functions we can *prove* we reproduced, ~1,060–2,040 we can prove we did not,
> and the rest the binaries **cannot adjudicate** — that residue is `.bss`
> statics and externs with no bytes in either image, i.e. **unobservable, not
> suspect**. Divergence is NOT concentrated in funclets (4.6% vs 5.8% for named
> bodies); it concentrates in **≤16 B adjustor/forwarder thunks (10.2%, 2.2×
> tree)** and the `??__E`/`??__F` static-lifecycle family. Game tier (4.6%) is
> cleaner than engine (5.5%).
>
> **laneTIGHTGAP's +109: STANDS, not reverted.** It measures 5.7×–7.3× the tree
> divergence rate (verdict invariant to strictness) and its 105 pairable credits
> rest on only 64 distinct base symbols — laneBE's guard-clear mechanism
> reproduced symbolically. But reverting would delete 31 evidenced + 42
> undecidable to remove 32 unevidenced (0.08% of the count), and **the defect is
> in the scoring rule (many-to-one masked pairing), not the splits geometry** —
> reverting removes credit without correcting anything. Reclassified in the
> ledger as ≈31 evidenced / ≈42 undecidable / ≈32 unevidenced.
>
> ★**STANDING GATE: price every future gap-absorption channel with
> `reloc_correspondence.py` before landing.** A channel materially above the
> tree's 5.2% divergence rate is buying metric, not program. laneBE's unlanded
> +560 is 100% this class and prices worse; its CLOSE recommendation stands.
> Keep **interior** (same-unit-both-sides) sweeps; close **different-unit
> absorption**.
>
> ★**By-product worth more than the audit: `docs/plans/laneBH_realbugs.json`** —
> **109 named bodies ≥128 B at 100% carry content-proven WRONG constants/strings**,
> invisible to every near-miss scanner because scanners only look *below* 100%.

> **VERIFIED 2026-07-29 at main `5e9996fc` (lane docfix).** Independently
> reproduced, not copied from a lane report: fresh `scripts/setup_worktree.sh`
> worktree at that commit, `rm -f build/45410914/report.cache`, full
> `./tools/ninja-locked` (1,045 edges, exit 0), then read
> `build/45410914/report.json`. **`measures.matched_functions = 39382`**, and an
> independent recount of `match_percent_normalized == 100.0` over every function
> in every unit gives **39,382** as well. Other measures at that build:
> `total_functions 69378 · total_units 3967 · matched_code 3431832 ·
> total_code 10579936 · matched_functions_percent 56.764393 ·
> fuzzy_match_percent 39.03488 · complete_units 1`.
> ⚠ **Do not recount with `fuzzy_match_percent`** — it reads **222 LOW** here
> (39,160), consistent with the error bar recorded further down this doc. Only
> `match_percent_normalized` sums to `matched_functions`.

## ★★★ QUOTE THE HONEST FLOOR, NOT `matched_functions` (2026-07-29)

> **SUPERSEDED IN SCOPE 2026-07-29 by lane BO-8**
> (`lane-bo8-icf-funclet-audit-2026-07-29.md`). Everything below is correct **for
> the supply axis alone** and still measures 3.7% (re-verified by a fresh pass-2b
> compile-out A/B at 40,540: reported 1,517, real 1,466). Two amendments:
> 1. **There is a second, disjoint over-count axis** — relocation-target divergence,
>    measured against the retail bytes. **Full honest band: 37,490 – 38,098, i.e.
>    the headline over-states by 6.0% – 7.5%**, not ~4%.
> 2. **"No real decompilation is affected" is false as a general claim.** It is true
>    of pass-2b surplus (which never touches named symbols). But identity divergence
>    is *worse* among named bodies (5.5% / 4.3%) than among supply-backed funclets
>    (2.4% / 0.56%). The clean statement is: *pass-2b surplus never touches named
>    functions.*
>
> Also settled there: **populating the ICF alias map cannot change any measure** —
> `report generate` hardcodes `functionRelocDiffs=None`, under which `reloc_eq`
> never consults `symbol_equivalences`. Measured: 3 groups → 1,408 groups, Δ = 0.

**`matched_functions` over-counts by ~4%.** The cause is objdiff's own funclet
pass (`pair_funclets_by_bytes`): it pairs each leftover funclet-like target onto
a base partner **without marking that partner used**, so **N targets can all
score 100% against 1 base function**. It is a property of the heuristic, not of
any lane — an independent replay attributed the inflation across **137 distinct
commits** spanning the project's whole history, ~70% of it predating the recent
sweeps.

**Every report now discloses this itself.** Our objdiff fork (branch
`oversub-disclosure`, installed 2026-07-29) populates
`measures.masked_equal_functions`:

```
HONEST FLOOR = matched_functions − masked_equal_functions
```

At install: 39,743 − 1,582 = **38,161 floor**; true honest **38,210** (the floor
sits 0.13% low because 49 of the flagged symbols re-pair onto a genuinely unused
partner when the pass is removed — they were *mis-attributed* credit, not
*unsupported* credit). `configure.py` now **hard-fails** if the fork cannot be
resolved, because the downloaded release omits the field and would silently
restore the inflated headline.

Three things that are settled and should not be re-litigated:
- **All fake credit is on anonymous `fn_` symbols.** Named-function matches are
  structurally unreachable by this pass. **No real decompilation is affected.**
- **Do NOT revert any landing over this.** The blocks that are entirely
  over-subscribed also hold genuine named matches; deleting them measures
  **raw −228 / honest −46**, i.e. the honest metric goes *down*. The fix is
  pricing, not deletion.
- **The naive screen does not work.** "unit `matched_functions` > its base obj's
  function-symbol count" trips **0 of 3,881** units (base objs carry thousands of
  inline/template COMDATs). The correct rule is per-signature:
  `excess = Σ_S max(0, demand_target(S) − supply_base(S))` —
  `scripts/harvest/oversub_guard.py`, wired as a gate into
  `diffunit_gap_apply.py`.

⚠ **The inflation grows with new landings** — re-run the census or read the field;
never quote a past figure.

## ⚠ Corrections to landed commit messages (2026-07-29, lane docfix)

Commits are immutable; these are the corrections. Each was re-derived from the
repository, not from another lane's write-up.

- **`01a0e9fa`'s "24 TUs get their FIRST pinned range ever" is WRONG for 23 of the
  24.** The commit added 24 *path-qualified* unit headers to `splits.txt`
  (`system/world/Crowd.cpp:`, …), but 23 of those basenames **already had pins under
  a bare-basename spelling**. Measured by counting `.text start:` lines per bare
  basename in `git show 01a0e9fa^:config/45410914/splits.txt`: Crowd 14, LightPreset
  17, LightHue 2, Instance 6, CameraShot 59, CharBoneOffset 8, CharIKRod 12,
  CharLipSync 40, CharLipSyncDriver 22, FileMerger 16, Anim 22, AmbientOcclusion 16,
  EventTrigger 46, MeshDeform 16, CrowdAudio 14, EndingBonus 25, LayerDir 10,
  Faders 37, Sequence 44, HeldButtonPanel 6, BandMachineMgr 45, MainHubPanel 24,
  MusicLibrary 12 — **only `system/world/Reflection.cpp` was genuinely new (0)**, and
  it is the only one of the 24 whose qualified spelling still exists on main today
  (the rest were later consolidated back onto the bare spelling). The commit's
  *score* claim (−2) and its map/split mechanism finding are unaffected. **No doc
  repeated this claim** — a tree-wide grep found the phrase only in the commit
  message — so nothing else needed editing; it is recorded here because a lane
  reading `git log` would otherwise inherit it.
  ⇒ ★**A new `splits.txt` unit HEADER is not a newly-pinned TU.** `splits.txt` keys
  on the spelling, not the source file, so the same `.cpp` can hold pins under two
  keys. Diff by **basename**, not by header line, before claiming first-ever coverage.
- **Several recent commit messages quote strict counts that do not reproduce on
  main** (one claimed 37,599 where a clean full rebuild at that commit measured
  37,282). The pattern is lanes quoting a **worktree** measurement as if it were
  main's. The headline above is the one number in this doc that has been rebuilt
  and re-read at HEAD; every other count in this file is dated and belongs to its
  own section. ⇒ **Quote a count only with the commit it was measured at and how
  (`report.cache` cleared? full rebuild? which tree?).**

## ★★ laneAY 2026-07-27 — the census-honesty lane (+28), and the FOURTH census bug

**Landed +28** across four measured legs (all with the full re-split recipe and
both ninja legs; every leg A/B'd unit-agnostically, by name AND by unit+name):
map/UIManager **+1**, laneAY-C **+18**, laneAY-A **+7**, laneAY-B **+2**.

### ★★★ THE LESSON: a census tool's UNIVERSE is as load-bearing as its resolver

`scripts/harvest/localstatic_census_wide.py` had a **sound resolver** (18/18
exact string resolution against reference commit `7d5c413e`) and a **broken
universe**: it enumerated by globbing `build/45410914/obj/**/*.obj`, a directory
that is **never cleaned**. Measured at 39,266:

| class | TUs | excess statics |
|---|---:|---:|
| live compiled units (target + base + report) | 506 | 3,423 |
| target-only pins (no compiled source — nothing to edit) | 127 | 571 |
| **orphans: 8,891 `auto_*` carves + 112 STALE objs** | 957 | **9,911** |

**71% of the reported population was in objs no live unit owns.** The 112 stale
objs are orphans of dead `splits.txt` generations, and because the tool ranked
by `max(variants)` over same-basename objs it **actively preferred them**: its
#1 "actionable" row was `band3/game/VocalPlayer` (46 statics) — a dead carve,
mis-attributed to `Poll`, whose live counterpart `default/VocalPlayer ?Handle@`
is **46/46 = zero excess**, i.e. already done.

★ **THE FIX — enumerate from `objdiff.json`, never from the filesystem.** It is
generated by `configure.py` from the live `objects.json` + `splits.txt` and
carries the authoritative triple per unit: `name` (byte-identical to
`report.json`'s unit name — no `split('/', 1)[-1]` guessing), `target_path`,
`base_path`. Only the **894** units with a `base_path` are editable at all.
`NO_REPORT_PAIRING` went **65 → 0**. Tool: `localstatic_census_v2.py`.

★ **This was the FOURTH bug in this tool family** (after the COFF
`SymbolTableIndex`-as-list-position bug and the `tu + '.cpp'` hardcode). The
brief said "assume a fourth exists until you have checked" — it did. **Assume a
fifth.**

### ★★ SPOT-CHECKS ARE NOT OPTIONAL — the corrected census STILL over-fired

The corrected join gave **84** named-editable excess statics. A converting
worker then found the discriminator at the bench: `localstatic_patch_gen.py`
sets `form=LOCAL_STATIC` on **`guard_va` alone**, and its target-side guard test
is "some `.data` VA both loaded and stored in the window" — which any ordinary
**file-scope static** satisfies (`sResources`, `sCharClipTypes`, …). The
base-side scanner uses the exact `??_B`/`$S` symbol-name test, so the two
disagree on byte-identical code and the row reads as pure excess.

★ **A real function-local static resolves BOTH a guard BIT (sequential across
the group) and a `static_va` (marching down by 4).** Requiring both is now the
`WEAK_GUARD` filter. It drops 207 rows / 623 statics and removes **exactly** the
rows hand audits condemned — `CharBoneDir::Init`, `UIEventMgr::Init`,
`Part InitParticleSystem`, `GemManager` ctor, `BeatMaster::CheckBeat` (all sat
at 100%/99.99%, i.e. nothing to fix), plus `BlockMgr`'s `disc_spin_up` (adding
it measured **82.8 → 67.5**), `BandWardrobe`'s `female`, and both of
`Rnd::DrawTimers`' "statics". **Every row a worker actually converted survives
the filter.**

★ **FINAL HONEST POPULATION: 63 named-editable excess statics / 31 functions /
26 units** — down from 13,932 reported and from the "90 actionable" headline,
41 of whose 90 were stale orphans. A well-measured small number.

### Residue verdicts (two confirmed, one refuted)

- **`?SetType@Object@Hmx@@` @ `0x82804588` — CONFIRMED.** Its body calls
  `??0Object@Hmx@@` + `??0Timer@@` and stores `vftable_82123B0C`, whose `??_R4`
  COL type descriptor reads `.?AVUIManager@@`. It is `??0UIManager@@QAA@XZ`.
  Repointed: 53.61% → 98.23%, then **100%** by dropping `mCurrentScreen(0)`
  (retail's ctor is 228 bytes to our 232, the extra insn being
  `stw r29, 0x2c(r30)`). ⚠ **The generalised scanner
  (`vftable_name_contradiction_scan.py`) finds exactly ONE other candidate
  tree-wide and it is a false positive — the vein is a SINGLETON.** Do not
  re-fund it; find these opportunistically instead (laneAY-A found a second by
  hand: `fn_82351E70` is `BandTrack::SetNetTalking`, not
  `HamDirector::PickIntroShot`).
- **`ContentMgr` / `UILabelDir::SyncProperty` — CONFIRMED, stated reason WRONG.**
  `UILabelDir.cpp` *does* have a pin. The defect is only that `ContentMgr.cpp`'s
  final `.text` line `0x828101A0–0x828109EC` is **exactly 0x84C bytes** =
  precisely `UILabelDir::SyncProperty`. Moving it took the row 0.00 → 75.69 with
  `ContentMgr` holding at 23 matched; the rest came from `UILabelDir.{h,cpp}`
  being a **verbatim DC3 copy that dropped 6 members RB3 keeps**. Target `Save`'s
  asm then yielded retail `rev = 9` (not DC3's 11) and member offsets
  `this+0xF8..0x164` that **independently confirm rb3-Wii's declaration order is
  RB3-360's layout**. Both functions → 100%.
- **`TrackerDisplay` map off-by-one — REFUTED.** 50 of 52 named target symbols
  have byte-exact size agreement with our compiled obj; an off-by-one would
  shift sizes wholesale. The two that differ are body divergences, and the 0.00%
  anonymous rows are a map-COVERAGE gap, not a mis-pairing.

### Other transferable findings

- ★ **The WRONG-UNIT splits channel was NOT drained.** Memory said 665 → 7;
  a re-scan found 30, of which **16 moves landed for +17 with zero real losses**
  (WRONG-UNIT 30 → 11, proposals 24 → 8). "Already drained" was too pessimistic.
- ⚠ **`splits_move.py scan` emits UNSAFE proposals.** The remaining 8 all have
  `n_carved_in_span == 0` and a span START strictly inside an already-carved
  function (+0x4 to +0x400), so applying any splits inside a symbol and
  hard-fails dtk with "ends within symbol" — leaving no `report.json`. They pass
  `apply --dry`'s "audit clean" because the audit checks overlap/inversion/empty
  blocks but **not symbol-boundary alignment**. These are
  `target_symbol_map.json` entries bound to non-function-start addresses
  (ICF/inline artifacts), not splits defects. **Worth adding a boundary-alignment
  refusal to the tool.**
- ★ **The lever generalises past statics into a HANDLER-LIST CENSUS.** Diffing
  target-vs-base guarded-`Symbol` sequences in a `BEGIN_HANDLERS` block localises
  *missing/extra handlers* to the instruction. It produced both of laneAY-B's
  100%s: `MusicLibrary` has no `fake_win` / `FriendsListChangedMsg` /
  `UserLoginMsg` and three extra store handlers; `MetaPerformer` has
  `has_online_scoring` **twice** (bits 14 and 17), both wrongly `#ifdef HX_NATIVE`.
  Same method proved `BandDirector::Handle` has **36** arms where an in-source
  comment claimed 34 (wrong count AND wrong address).
- ★ **A repeated rb3-Wii DEV-only pattern, worth a scanner:** retail lacks the
  guard-`if` + `MILO_WARN` wrappers the Wii dev build added, and several dev-only
  bodies are simply **empty** in retail. Five `Instarank` label updaters,
  `MetaPerformer::UploadDebugStats()`, `Set/ClearCreditsPending`, `IsWinning()`.
  Same family: `BandWardrobe::LoadMainCharacters`' whole `LOADMGR_EDITMODE`
  prefab path is dev-only, and its `char buf[256]` was the *entire* 0x260-vs-
  0x140 frame delta; `BandCharacter::Poll` has **no** `START_AUTO_TIMER` in
  retail; `BlockMgr::Init` does **no** `MemAlloc`.
- ★ **`#pragma auto_inline(off)` is a working MSVC-X360 inline-policy lever and a
  FORCE MULTIPLIER.** Applying it to `MetaPerformer::IsBandNoFailSet` (retail
  calls it out-of-line; `/Ob2` inlined ours) collapsed a **105-instruction
  r25↔r26 callee-saved regswap cascade in one step**. ⇒ Register cascades in big
  `Handle` functions can be downstream of a single inline-policy mismatch — try
  this **before** declaring a function "permuter-class".
- ⚠ **`report.json`'s `match_percent_normalized` ignores register-ARGUMENT
  differences, unlike `run_objdiff`'s headline.** `NextSongPanel::FinishLoad`
  reads 99.5 in objdiff but is a strict **100** in the report. Don't keep tuning
  regswaps thinking you're short of the gate — and don't read a report-100 as
  register-exact.

### What remains here
The named channel is nearly spent (63 statics, top row `ContentMgr` 17). The
real residual is **982 ANONYMOUS rows / 3,349 statics in compiled units** —
`fn_<VA>` target symbols inside pinned spans we compile, whose local statics
prove they are Handle/SyncProperty-shaped bodies. That is an **identification**
problem (map coverage), not a source-edit one. Most concentrated:
`VocalTrackDir` 150, `BandSongMetadata` 95, `CustomizePanel` 75, `BandCharDesc`
67, `PostProc` 61, `UIStats` 61. Also open: two map mispairs found but not
repaired (`0x822a68e0` is an `OutfitConfig::MeshAO` SyncProperty, not
`WorldDir::Save` — the identical 0x168 size is coincidence; and
`TriggerCalibration`'s target takes three pointer args where ours takes
`(this,int)`), and `UILabelDir`'s other 10 functions are unidentified.

## ★★ 2026-07-26 LATE ARC — 36,069 → 38,305 (lanes AN…AV), and what it taught

Ten lanes (AN…AV), each an Opus lead fanning out to its own Opus/Sonnet workers
in isolated worktrees. **+2,236 verified strict, every landing A/B'd
unit-agnostically against a pickled baseline.** Largest single landing:
laneAT **+640**.

### The channel that reopened
★**"Byte identity is drained" was REFUTED — every prior drain was measured
GLOBALLY.** Per-unit, the ICF-prone shapes that defeat global uniqueness are
still unique *inside their own unit*. Mechanism: **`is_funclet_like()` gates BOTH
sides of `pair_funclets_by_bytes`** (objdiff `diff/mod.rs:1423,1438`), so an
anonymous target can never byte-pair with a mangled base name — objdiff
structurally cannot reach these; only a map entry can. laneAQ +276, laneAS +321,
laneAT +520 off this one correction.
★**The size "window" (17–68 B) is a property of the ANONYMOUS POPULATION, not of
the functions:** named >84 B functions are **91.9% strict**, anonymous ones 0.2%;
356 functions at exactly 0.0% → **305 flipped once named**. >84 B is
**supply-limited, not gate-limited**.
★**New channel — existing map entries can be PROVABLY WRONG** (laneAT): 155
named targets below 100% with a free exact in-unit byte twin under a different
name; repairing 120 = **+99 at 82.5%**. Mutually recursive with the twin scan
(a repair frees a name the scan then claims) ⇒ **run alternately to fixpoint.**

### Honest error bars discovered this arc — quote these, not intuitions
- ★**113 target symbols are reloc-masked byte-EQUAL to their mapped base and
  STILL score <100%.** objdiff's normalized diff is **stricter** than masked byte
  equality. That is the error bar on every byte-twin claim.
- ★**Only `match_percent_normalized` sums to `matched_functions`;
  `fuzzy_match_percent` reads 222 LOW.** A ceiling computed off the fuzzy field
  had to be retracted.
- ★**Reloc-masked byte equality is near-vacuous below ~32 B** — the 12-byte
  adjustor body is shared by **1,673 distinct symbols**.
- ★**Fuzzy % is NOT identity evidence** — 249 provably-wrong names measured mean
  **53.7%**, max 89.95%; MSVC PPC prologue boilerplate alone reads ~55%.
- ★**The 99.8/99.9% band is NOT a near-miss queue** — 1,643 forty-byte funclets
  there are objdiff pass-3 **fuzzy pairings of unrelated funclets**.

### Pools that shrank when the tool was un-gated (both were "source problems")
- laneAM's "~4,300 unreachable, we compile no matching body" → its predictor was
  **funclet-shape-gated on the base side**, so ordinary bodies had an empty
  candidate set *by construction*. Un-gated: **1,502 of 3,290 have an exact
  reloc-masked twin we already compile** (not 8). Honest source residue
  **1,445**.
- laneAT's member-defect census was **~64% wrong** (`addi r3,r31,off` is the
  funclet's own FRAME pointer, not a `this` slot). Corrected: 1,732 of 2,659
  frame-related, **390** true member. Then an over-carve scan ate 306 of those
  ⇒ **the header-reconciliation channel is 84 functions, not 1,098**, and the
  splits-attribution channel is correspondingly larger.
⇒ ★**Before concluding "we lack the source", re-run the measurement with the
tool's own gates removed.** Twice in one day that was the whole answer.

### Mechanisms (reusable)
- ★**A funclet flips on the parent's frame SIZE alone — the parent need not
  match.** Controlled: parent held at 99.4%, all 6 funclets at 100.0%. One parent
  fix = 4× multiplier. Tool `scripts/harvest/frame_delta_scan.py`.
  ★**But re-priced by its own worker: realistic yield from the 409-row pool is
  LOW TENS, not hundreds** — 1 of 14 landed, 1 was an ICF trap that would have
  LOST matches, 4 unfixable in source, 8 entangled with body/regalloc. **Sort by
  "frame is the only diff" (≤6 mismatches, all offset-shifts), NOT by pct band;
  below ~97% the frame delta is a SYMPTOM.**
- ★**Sibling-scope overlay:** most +0x10 deltas are one local failing to
  *overlay* onto another's slot. MSVC /O1 stack-colours two disjoint-lifetime
  objects onto the same offset **only when both are block-scoped in the same
  parent scope**. Bare `{ }` around the trailing object fixed `ReplaceSubdir`
  (+4 from 3 lines). **Named objects only** — unnamed temporaries resisted every
  variant and naming them made one case *worse*.
- ★**ICF folds masquerade as sizeof defects.** `ObjVector<DynamicPropertyEntry>::
  resize` looked like a perfect +0x30 sizeof bug; "fixing" it hit 100% but broke
  3 stride-dependent STL functions in the same unit (+5/−3, reverted; 0x80 was
  correct). **Tell: objdiff's Function Call Diff shows a target-only callee
  naming a different class than the base at the same call slot.**
- ★**A large single-parent funclet cluster is as likely to be a WRONG-UNIT carve
  as a layout defect** — 20 funclets that looked like a layout bug were
  `StreakMeter::StreakMeter()` over-carved into Waypoint's span; one splits move
  fixed all 20. **Check whether the class's other members are already pinned
  elsewhere before reconciling a header.**

### Measurement contamination — FIVE live sources, control for ALL
1. ★**`setup_worktree.sh` reflinks main's `.obj` files and main's build dir is
   DIRTY** ⇒ any obj-derived scan before the worktree's own first full build
   reads another lane's uncommitted source. Measured **73 pre-build vs 32
   post-build**; **dirty objs MANUFACTURE evidence**, so it reads as a rich vein.
   ★Second axis: the **map** drifts faster than `src/` (923/549 lines in one
   interval) — check **both** diffs against *current* main, not the branch point.
2. ★**`config/45410914/symbols.txt` is both a dtk INPUT and a regenerated
   OUTPUT** ⇒ a control leg silently retains its own treatment. `git checkout --`
   it on **BOTH** legs. Never commit it.
3. **`dynamic_init` patcher unstable on a first build** ⇒ same number of builds
   per leg; a ±2 drift is documented.
4. ★**The map is NOT a ninja input to the renamer** ⇒ map-only edits need
   `rm -f build/45410914/target_symbol_renames.stamp`, or the edit silently does
   nothing **and reads as a refutation**.
5. **Stale `.s`:** `build/*/asm/*.s` for units no longer in `splits.txt` are
   never regenerated and are silently wrong.

### Tool defects found (fix or route around)
- ★**`span_predictor.py`: `matches()` hardcodes `tu + '.cpp'` ⇒ EVERY `.c` unit
  is mis-flagged WRONG-UNIT** (raw 532 vs corrected **290** — 242 false positives
  in json-c/vorbis/zlib/tomcrypt). It also **self-confirms if used naively** (it
  unions the record's own `tu` into the candidate set) — **pass a sentinel `tu`**.
- ★**A WRONG-UNIT pool is usually NOT map work:** **276 of 290 have no definer
  anywhere**; only 3 unique-definer. **Check `n_definers` first.**
- **`map_rotation_repair.py apply` corrupted the map** — `startswith('"0x')` also
  matched bare array elements of `_bijection_arbitrary`, writing a key/value pair
  *inside* a JSON array; asserts were blind (they filter `isinstance(v,str)`).
  Fixed.
- **`land.sh`** reflows all ~25k map lines (breaks the byte-splice invariant) and
  its **union-merge corrupts splits when two lanes each add `.text` pins**
  (unioned `.pdata` back-fills → hard split failure). **`READY:` is not a
  verify** — run `scripts/harvest/overlap_check.py`.

### Fleet rules adopted this arc
- ★**Never re-serialise `scripts/target_symbol_map.json`** — a `sort_keys`
  rewrite churns ~25k lines and makes the branch unmergeable against every
  concurrent map lane. Appends + one line per repair.
- ★**After composing a splits change, re-run the map tools to fixpoint** — a pin
  move strands every map entry whose VA crossed a unit boundary.
- ★**Re-check cross-lane collisions LATE.** Two correctly-measured fragments
  (+26, +8) rebased to exactly **0** because a concurrent lane had taken every VA.
- ★**Commit every worker worktree early.** Of six orphaned worktrees, the four
  that had committed contributed +32; the two that had not contributed **0**, and
  their buffers did not compile.
- ★**Check that a confirmation test COULD fail.** An inherited 48-entry set was
  refuted **48/48** because the prescribed test was a byte-diff on a class
  *constructed* by byte equality. A leave-one-out that restores the truth to the
  candidate supply likewise reports 100% **vacuously** (same tier: 95.5% wrong
  under abstention).
- ★**Draft findings only AFTER the measurement lands.** Six lanes retracted
  claims written ahead of a real `report.json` A/B — several of those retractions
  were the lane's most valuable output.

### ★★ SANDWICH OVER-CARVE: measured NET NEGATIVE — necessary but NOT sufficient

A "sandwiched" `.text` block (both immediate gap-0 neighbours are the same other
unit) plus **100% definer corroboration** (every anonymous target function in the
block reloc-masked byte-matches a symbol *defined* in the proposed owner's obj)
**still loses matches.** Measured on the 19 strongest blocks / 268 functions
(incl. `SaveLoadManager`→`ProfileMgr` 73 fns, `SessionMgr`→`MetaPerformer` 72):
**−23 net, 49 gained / 72 lost, 18% conversion.** Run twice, identical. Reverted.

★**Why: the definer test asks whether the destination obj DEFINES a byte-identical
symbol — not whether that symbol is ALREADY CLAIMED.** Moving code in gives the
base symbol a second claimant and greedy pairing displaces the incumbent. **The
losses are named symbols in the RECEIVING units, not the donors**
(`??_GBaseSkeleton`, `??1ObjVector<TransformCrowd>`, `?NewObject@BackdropPanel`,
`??0Callback@Loader`). This is **laneAP's leg-B failure returning through a
different door**: moving code between units on byte evidence is not free even
when the evidence is strong *and the destination is right*.

★**The two moves that DID pay (StreakMeter +13, Waypoint→VocalTrackDir +67)
satisfied a third condition incidentally** — both were **pure anonymous funclet
tails whose parent was already pinned to the destination**, so nothing in the
destination competed for a pairing.

★★**THE THIRD PREDICATE WAS BUILT AND DOES NOT SAVE IT — CHANNEL CLOSED.**
Predicate 3 as implemented (supply-vs-demand per reloc-masked signature:
admissible only when `supply − already_claimed ≥ incoming` for **every** signature
in the block) gave 600 sandwiched → 182 scorable → 107 definer-corroborated →
**87 capacity-safe (312 fns)**. Applied all 87: **−10 by (unit,name); by name
+73 / −101 = −28.**
⇒ **Measured NEGATIVE TWICE on DISJOINT candidate sets under TWO different
admission rules** (19 blocks/268 fns → −23; 87 blocks/312 fns → −10).
Per-signature counting is insufficient because destination incumbents are also
displaced by **transitive re-pairing**. ⛔**DO NOT FUND the 716-function pool.**

★**And the two "wins" were probably not a channel at all.** StreakMeter (+13) and
Waypoint→VocalTrackDir (+67) were each a **pure anonymous funclet tail whose
parent is already pinned to the destination, with NO parent function of its own in
the block** — and both were found by *reading* the block, not by any scanner.
Narrower than any computable predicate. ⇒ **Record as a hand-verified special
case, not a queue. Two data points that pay do not make a channel when the
population they were drawn from measures negative twice.**

Tool: `scripts/harvest/sandwich_overcarve.py`. Rows:
`/home/free/tmp/laneAT/sandwich_scored.json`, `sandwich_applied.json`.

★★**TWO OPERATIONAL TRAPS FOR ANY BULK SPLITS MOVER:**
1. **A move that empties a unit's LAST `.text` block HARD-FAILS the build** —
   `Failed to open <unit>.obj: Invalid COFF/PE section headers`. Guard for it.
2. ⛔~~**`.text` and `.pdata` must be moved and restored TOGETHER.**~~
   **FALSIFIED 2026-07-27 — see below. Move only `.text`; `.pdata` follows.**

★★**⛔RETRACTED 2026-07-27 — THE `.pdata` "DELETION LOSES RANGES" CLAIM DID NOT
REPRODUCE.** It was recorded here as fact and propagated; it is **false**.
**Verified rule (jeff `src/util/split.rs:1035-1095`, `update_splits` at
`split.rs:1146`, xex path `src/cmd/xex.rs:2476-2483`):** `split_pdata()`
**clears the ENTIRE `.pdata` split set and re-derives one range per `.text` code
split on EVERY run.** ⇒ **`.pdata` lines in `splits.txt` are DERIVED OUTPUT, not
input. Derivation is never gated on absence.** Confirmed on the deployed fleet
binary (`dtk 1.9.2`, sha `57b52d64`; the `laneAF-va-fragments` diff touches only
`va_fragments`, pdata logic identical).
Empirical: deleting **54** `.pdata` lines (MasterAudio 30 + BandCharacter 24) and
re-splitting **regenerated all 54**, sorted diff empty, 5,254 ranges stable; a
hand-introduced overlapping `.pdata` line was **silently healed** back to
baseline.
⇒ **The observed 5,172 → 4,694 can only have come from the lane's accompanying
`.text` edits (derived count is a function of `.text` splits) or from a split run
that FAILED and left the hand-edited file in place** — e.g. symbols.txt drift.
★**If `.pdata` lines don't regenerate, the SPLIT RUN failed — that is the bug to
chase, not the `.pdata`.** Never hand-edit or hand-carry `.pdata`.

★**The empty-unit trap IS real, but fires at a different stage than reported:**
draining a unit's last `.text` block, the **split SUCCEEDS** and emits a 42-byte
`obj/<unit>.obj`; the build then hard-fails at the **report.json** step with
`Failed to open ./build/45410914/obj/<unit>.obj: Invalid COFF/PE section headers`.
**Remediation (verified): delete the unit's whole `splits.txt` entry.**
`CLAUDE.md` amended accordingly at `6ab38692`.

### ★ Two inference rules for diagnosing a "wrong" unit or a "missing" class

★**Before calling any pin a PHANTOM, `grep '#include "[^"]*\.cpp"'` in the owning
`.cpp`.** Scatter-wiring is deliberate and widespread — a sweep of 49 suspect
units found **20 of 49 scatter-wired** (e.g. `HamCamTransform.cpp` carries **nine**
`#define gRev`-guarded `#include "….cpp"` lines; `Character` 4, `EventTrigger` 5,
`LightPreset` 5, `MeshAnim` 6). **All 49 resolved to a real source file**, so
foreign-class symbols in a unit are usually *intended*, not a mis-pin. A lane
retracted a whole "DC3-only files pinned onto spans that never held them" finding
to this rule.
★Corollary on scope: a tool reading the **compiled `.obj`** already sees
post-include content, so existing scatter wiring does **not** loosen a bucket it
measured. *Adding* a scatter include is a different and much larger change.

★★**ABSENCE FROM `../rb3` DOES NOT PROVE ABSENCE FROM RB3-360 RETAIL.** `../rb3`
is the **Wii dev** decomp, and Wii is the **cut-down SKU**. A class missing there
may still exist in the 360 retail binary. ⇒ Downgrade any "this class does not
exist in RB3" row from **permanently unfixable** to **"no RB3 oracle evidence —
unreachable pending a second source."** (Dance-Central-lineage names like
`HamMove::LocalizedName`, `DancerFrame`, `DetectFrame` remain very unlikely, but
that is a prior, not proof.)

### ★ Two build-monitoring traps (multi-lane box)

★**A bare `pgrep -f "ninja-locked"` COUNTS OTHER LANES' BUILDS.** With 3+ lanes
building concurrently in separate worktrees, a lane's own "progress" readings were
partly other lanes' work — it reported edge counts that were not its own. **Match
processes by `/proc/<pid>/cwd` against your worktree path**, not by a shared tool
name. (The builds are *not* deadlocked when this happens — each worktree has its
own build dir; they merely contend for CPU.)

★**Long uncached builds get truncated by harness task reaping** —
`ninja: build stopped: interrupted by user`, twice on one leg. Detach with
`setsid nohup … & disown` and monitor `build/45410914/report.json` **mtime** as
the completion signal, with a ninja-exit fallback.

### ★ `OBJ_MEM_OVERLOAD` opt-out backlog: measured **ZERO** — a phantom count

The classifier finds **156 OUTLINED-only** classes and only **10** opt-outs were
applied, which reads like ~146 left behind an already-proven +111/+5 lever. **It
is not there.** Of the 139 non-vendor OUTLINED classes (17 are
`soundtouch`/`D3DXShader`/`NUISPEECH`, out of scope):

| | n |
|---|--:|
| no in-tree header — class lives in an **unwired TU** | 62 |
| header exists, **no allocation macro** — unaffected by the change | 75 |
| already declared `MEM_OVERLOAD` — already correct | 2 |
| **declared `OBJ_MEM_OVERLOAD` — needs an opt-out** | **0** |

★**So `scripts/harvest/newobj_inline_classify.py` is a per-class ORACLE FOR FUTURE
PORTS, not a work queue** — when a TU gets wired, look its classes up and declare
each with the macro the bytes say retail used. The **62 with no in-tree header are
exactly the ones that will need it.** Ranking it as a backlog would have sent a
lane after 146 phantom candidates — the same failure mode as the sandwich pool:
**a plausible count that dissolves on contact.**

★**METHOD NOTE (two confidently-wrong answers before the right one):** matching
`OBJ_MEM_OVERLOAD\s*\(\s*(\w+)` returns **nothing** — the macro takes a **line
number**, not a class name; and parsing class bodies mislabels **138 of 139**
(these headers don't survive a naive brace scanner). **File-level presence keyed
on header basename** is what answers it. Both wrong answers were plausible (0
actionable / 138 "no declaration") — the lane only caught them by spot-checking a
header it had written itself. ⇒ **Spot-check any census against a case you know
by hand before believing it.**

### Refusals worth not re-funding
`_bijection_arbitrary` ceiling **+2** (1,205 of 1,207 already at 100) ·
`.pdata` parentage decides **2.8%** of the unreachable pool · the 84-byte
pairing cap **do-not-lift** (p2 36.3% / p2b 43.3% precision; the lift mostly
duplicates the name-side waves silently) · `DECOMP_FORCEBLOCK` is a **silent
no-op under MSVC** (`src/decomp.h:38` gates on `__MWERKS__`), ceiling ≤5 ·
"overlapping data carves" = stale TU0 artifact, **0 defects** · 187 dangling map
entries: dropping 179 measured **net 0** and **destroys identification info**
(a dangling entry becomes valid the moment its TU is wired) · **retiring
proven-wrong bindings is CHEAP** (5 evictions = +0 strict, −0.0007pp fuzzy).

## ★ TRANSFERABLE LEVERS from the 2026-07-25/26 coordinator session (27,223 → 27,816 in-lane)

These are mechanism findings, not one-off fixes. Landed + measured; reusable fleet-wide.

### 1. ⚠ SUPERSEDED — "BULK-CONVERSION LAW" (098f84a8, +177) → the predicate is THE PARENT AT 100
> **CORRECTED 2026-07-29 (lane docfix) by `be2b574c`.** The measurements below are
> reproduced verbatim and still stand. The *rule inferred from them* was wrong, and
> acting on the wrong rule is expensive: it tells a lane to keep converting a whole
> TU while the score falls, when what actually clears the churn is driving **one
> parent function to 100%**.

Original measurement (unchanged): converting ONE function to retail's
`DP_KEYS`/function-local-`static Symbol` form measured **−7** (its 3 new statics
collaterally un-paired 9 already-matching EH funclets — objdiff funclet
over-subscription re-pairing). Converting **all 21 stragglers in the TU
simultaneously = +48** (53 gained / 5 lost).

★★**The operative predicate is THE PARENT FUNCTION REACHING 100%, not "the TU is
fully converted."** Measured in `be2b574c`:
- `NextSongPanel`: moving a **single** static read **−230** mid-flight (230 funclets
  100 → 99.9). The same edit read **+1 / 0 losses** once the parent was driven to 100.
  Whole-TU conversion was never what changed; the parent hitting 100 was.
- `AccomplishmentPanel::LaunchSelectedEntry`: improved **95.6 → 97.5** and still
  measured **−16** (16 EH funclets 100 → 99.9). **Reverted.** Partial credit does NOT
  cancel the funclet re-pairing churn — so "keep going, it'll stabilise" is false for
  any function that stops short of 100.
- ⇒ In `098f84a8` the whole-TU sweep paid because it happened to take its parents
  *to 100*, not because it was whole-TU. A bulk conversion that leaves parents at 97%
  is a **net-negative** operation.

★★**DECLARATION POSITION IS LOAD-BEARING, AND IT IS USUALLY NOT THE FUNCTION TOP.**
The guard-check position in the target body names the declaration point; retail
declares these at the USE SITE.
- `PanelDir::PanelNav` **96.9 → 16.4** when its 3 statics were hoisted to the top. Reverted.
- `TrackPanelDir` **90.6 → 79.6** at the top, **→ 100** with the same statics moved
  inside `if (mScoreboard)`.
- 3 of 4 `NextSongPanel` fixes were **pure placement moves** (no new statics):
  80.7 → 100 on their own.

★**FAKE-100s exist in this vein.** `MusicLibrary::ClientSetPartyShuffleMode` read 100
while its target body held a local static ours lacked (only 12/34 instructions actually
equal). Converting a sibling exposed it at 52.9; adding the static made it a REAL 100.

**⇒ A one-at-a-time trial that reads net-negative is still NOT evidence the lever is dead
— but the retest is "drive THIS parent to 100", not "convert the rest of the TU".**
Tools: `scripts/harvest/ls_guard_timeline.py` (guard-bit order = declaration order, plus
the string literal and storage address — makes each conversion a transcription) and
`scripts/harvest/localstatic_tu_census.py` (per-unit done-vs-straggler; ⚠ counting only
`static Symbol` massively over-reports — most apparent gaps are `static Message` /
`static DataArrayPtr` already present).
Follow with `homing_scan` on the converted obj (bytes changed) — yielded 7 more
plain-UNIQUE homes, +7/−0. ⚠ **`be2b574c`'s full-tree homing sweep (1024 TUs) after the
conversions found 0 new homes** — that follow-up is swept out tree-wide; do not re-run
it per-TU.

### 2. GUARD-BIT TIMELINE = a transcript of the source's static-declaration structure
Bit ORDER gives declaration order; the GAPS between guard-check runs give grouping and placement.
On `RecordPerformance` it named the 7 declaration groups (at their USE SITES — 27/1/1/1/1/23/2, NOT a
switch), the missing key (`hopo_gems_strummed`), and a DEAD key retail declares but never inserts
(`high_gems_hit_high`). 67 → 99.27, frame 0x780→0x680, flipping ALL 79 dependent funclets.
Note: big recorders declare **DataPoint FIRST then keys** — the OPPOSITE of the small getters.

### 3. Container identity from CALL SHAPE
`std::hash_map<Symbol,int>` vs `std::map`: retail called an OUT-OF-LINE ctor/dtor and walked a
null-terminated chain (node+0 next, +4 key, +8 value); `std::map` INLINES its ctor and iterates via
`_M_increment` against `end()`. Correcting it closed a 0x4d0→0x4c0 frame gap (+42 with 41 funclets).

### 4. ⚠ IDENTICAL-BODY FAMILY = a systematic FAKE-100% generator (a380ed69)
Every `Foo::StaticClassName()` (OBJ_CLASSNAME) and `FooMsg::Type()` (DECLARE_MESSAGE) compiles to ONE
identical 22-instruction body; **453 exist in the TU5 image**, differing only in three RELOCATIONS.
objdiff normalized mode ignores relocation targets ⇒ **ANY member pairs against ANY other at a fake
100%**, self-confirming. **The STRING OPERAND is the only sound discriminator.**
Measured: **153 of 405 mapped entries were WRONG**, incl. a contiguous off-by-one shift across 25
consecutive `Char*` slots. Repair landed +10; one VA GAINED a match once UNMAPPED ⇒ **unmapped beats
wrongly-mapped**. Tool: `scripts/harvest/localstatic_symbol_audit.py --json` (re-derives in ~40 s,
flags each repair's harmfulness).
~~**OPEN DEBT: ~51 entries are correct-by-string but currently satisfy a fake 100%**~~
> ✅ **PAID 2026-07-26/27 — this debt is CLOSED.** Do not re-open it as a backlog item.
> - `01a0e9fa` (lanePHANTOM) retired **31 string-proven-wrong** map entries. Cost was
>   **−2, not −31**: repairing the map ALONE costs full price (−30), but repairing
>   **map + SPLIT together** is nearly free — 28 of the 31 were isolated 0x58-byte
>   scatter ranges that existed ONLY because of the wrong map entry, so re-pinning
>   `.text`+`.pdata` to the string-proven owner recovers them 1:1. ★**The blast radius
>   was 2 functions per phantom, not 1** — 26 of the ranges had been grown 0x58 → 0x78
>   by `.pdata`-parentage pins, and that extra 0x20 is the phantom's own `??__F` atexit
>   dtor (26/26 verified by decoding the guard word out of both bodies).
> - `560dffb3` then dropped **528 non-injective duplicate names** (user-approved),
>   measured **−105**. ★**The naive count overstates that debt by ~3x**: 322 of the 528
>   were scoring 100%, but a VA that loses its name reverts to anonymous `fn_<VA>` and a
>   large share **re-pair positionally** as unnamed funclets. Duplicate credit 158 → 1
>   (the legitimate `?NodeCmp@@YAHPBX0@Z`, a file-static qsort comparator with genuinely
>   different bodies in `DataArray.cpp` and `BandWardrobe.cpp` — statics are not COMDATs,
>   so the linker does not dedup them).
>
> **LIVE RESIDUE, re-measured 2026-07-29 on main `5e9996fc`** (`venv/bin/python
> scripts/harvest/localstatic_symbol_audit.py --json`, run in a clean worktree after a
> full `./tools/ninja-locked`): `family members in .text 453 · distinct strings 418 ·
> ambiguous strings 32` → **OK=405 MISMATCH=25 UNMAPPED=20 FOREIGN=2 NO_TOKEN=1**,
> **repairable=3** (of which harmful-to-apply: 0). The three uniquely repairable are
> `0x8227a1a8` Flow→`BandCamShot`, `0x82369ba8` ClipCollide→`CharBone`, `0x8236ac28`
> RndMesh→`CharPollGroup`; the other **22 are the AMBIGUOUS-string** rows
> (`Rnd*`/`Dx*`/`Ng*` triplets, `FxSend*` pairs) — still string-proven wrong, still
> fake-100, but **not uniquely repairable without a second oracle**.
> ⚠ At `01a0e9fa` this residue read **33 MISMATCH / repairable=0**; intervening map lanes
> moved it. The number drifts — **re-run the audit, never quote it from a doc.**

### 5. NOT all "class absent from src/" map entries are contamination
LEAPCORE / XAUDIO2 / NUISPEECH / XGRAPHICS / TrueColor / FaceCore are REAL Xbox360-SDK + Kinect
middleware statically linked into both games (0x82BE0000–0x82BE6000 is a coherent XAUDIO2/LEAPCORE
region). Their defect is a **SPLIT PIN inside XDK library territory** — `System.cpp` pinned
0x82BE28C8–0x82BE4428 (tot=33, comp=1), also `Compress.cpp` 0x82A68050, `GemTrack.cpp` 0x82B93C78.
⇒ splits lane, NOT map lane. **RESOLVED 2026-07-26 (laneXDKPIN).** Content-verified and re-carved:
`System.cpp` .text 0x82BE28C8–0x82BE4428 + its .pdata 0x8225F688–0x8225F7A8 **removed** (all 55 fns are
`CLeapSystem@LEAPCORE`; the span's ctors store 7 vtables and reference the GUID {8bcf1f58-…} @0x821A7D9C,
L"Xbox 360 audio device" @0x821A7DAC, L"Audio" @0x821A7DD8, "SimpList: non-growable list…" @0x821A68BC —
and have NO RTTI COL, i.e. built /GR-, unlike Milo). `Compress.cpp` .text 0x82A68050–0x82A68F38 + .pdata
0x822506A8–0x822506F0 **removed** (XGRAPHICS suffix-tree shader-microcode compressor; loads
"Compression : creates %d subroutines" @0x8217D21C). Our real spans are unaffected: Compress.cpp keeps
0x827CF920–0x827CFA40, whose zlib version string "1.2.1" @0x8211A4A0 confirms it. The GemTrack.cpp
0x82B93C78 item was a FALSE ALARM — 0x82B93xxx is band3/bandtrack territory, no XDK symbol within 0x100000,
and a later lane already re-carved it (TrackPanel.cpp 0x82B93C78–0x82B93CE4, GemTrack from 0x82B93CE4).
A tree-wide re-scan (map-named XDK/middleware symbols inside each pinned .text) found NO other straddler:
next worst is Synth.cpp at 5/96. Measured: +2 matched, 0 lost.

### 6. Source bug, still open
Retail's `FxSend*360` classes register under the **base** token (`FxSendReverb`, not `FxSendReverb360`);
`RndMultiMeshProxy` loads `"RndMultiMeshProxy"` where our `OBJ_CLASSNAME` says `MultiMeshProxy`.
**FIXED 2026-07-26 (laneXDKPIN).** Ground truth read out of the retail StaticClassName bodies:
`FxSendReverb360::StaticClassName` @0x82B59FD0 loads "FxSendReverb" @0x820F4EC0; likewise Wah@0x82B5A888→
"FxSendWah", MeterEffect@0x82B5A680, Synapse@0x82B5A808, Delay@0x82B5A158, PitchShift@0x82B5A788 —
all base tokens (EQ/Chorus/Distortion/Compress/Flanger already were). `RndMultiMeshProxy::StaticClassName`
@0x8240E3C0 loads "RndMultiMeshProxy" @0x8205DD00. No "*360" token exists anywhere in .rdata; the only
360-suffixed strings are .data RTTI type descriptors (`.?AVFxSendReverb360@@`). SynthSample360 fixed too.
⚠ The "disambiguates 32" prediction is **REFUTED**: `ambiguous` in localstatic_symbol_audit.py is an
IMAGE property (≥2 bodies load the same string) and is invariant to our source tokens — 32 distinct
ambiguous strings before AND after. The real, measured payoff is 69→62 MISMATCH / 358→365 OK (7 map
entries vindicated). Score impact is nil by construction (normalized diff ignores the reloc).

### 7. Process hazards that cost real time this session
- **`git apply` aborts ATOMICALLY while still printing per-file "applied cleanly".** Always verify with
  `git status` after any apply that reports an error. Hit twice.
- **`git add <file>` on shared main also stages OTHER lanes' uncommitted edits to that same file** —
  swept 122 foreign map entries into one commit. The `does not match index` error from `git apply` is
  the tell; re-apply with `--exclude=<file>`, hand-add your own lines, and read `git diff --cached`
  (line count is the giveaway) before committing.
- **`build/45410914/report.json` on disk can be weeks stale** — another lane rebuilds it. Check mtime or
  regenerate before quoting ANY number.
- **TU0 → TU5 flip (2026-07-15) invalidated every pre-flip ADDRESS.** Use Ghidra bank
  `default_tu5.xex-c5a170`; never the live default.xex. Cross-check any address against the map.


## 2026-07-20 flywheel session — 18,819 → 18,874 (+55 this session; +185 across the arc)

### Wave close-out addendum (later 2026-07-20): 18,874 → 18,924 (+50)
Final grind-tail wave +28 (LaunchGoal local-static cascade +19; struct-stride
SongPattern/LocalizedName; levers: NOTIFY_ONCE_EVAL flag, qualified-base call,
DrawMode=DC3-minus-1, if-guard vs mask-fold) + correlator r6 +1 + BinDiff r1
+5 (286 map entries, 563 carving hints) + partial-recovery/W-E landings.
**Measured-fundable near-miss pool EXHAUSTED (92/92 attempted, 9.8% tail
rate) — Phase-1 grind CLOSED; pivot to identification (Phase 2) is live:**
see docs/plans/remaining-bytes-decomposition-2026-07-20.md +
docs/plans/bindiff-transfer-spike-2026-07-20.md. Struct-stride RE campaign
in flight (ICF-fold-stride trap documented in memory).

Ran the **body-flip → correlator-harvest → reprice** flywheel to a clean milestone:
- **Grind wave 1** +11 (LEVER-STRING/SYMBOL + BODY-LEVER 70-90; missing-`virtual`
  GameMode cascade +9 discovered).
- **Lane A wave 1** +22, **Lane A wave 2** +14 (retail-absent deletions, HttpGet
  layout, HasPart virtual, dead-stub body restores). Combined Lane A +36.
- **Correlator re-scan** +8 (6 byte-identity additions + 2 invcorr repoints,
  full-rebuild A/B gained 8 / LOST 0, `ce41a0a4`). Near fixed-point.
- **Tooling landed:** `scripts/triage/reprice_router.py` (`48a8ce51`) — grind-outcome
  feedback loop, router self-sharpens each wave; `scripts/harvest/missing_virtual_scan.py`
  (`6726d4ee`) — cascade detector (vein now drained).

**Measured priors (reprice_router, N-gated):** BODY-LEVER yield is 70-90 stratum ONLY
and **thinning: 24.1% → 20.4%** as the band is skimmed; ≥90 dead (4.8%), <70 dead (0%);
certify-skip RELOC-COLOC/STRUCT-ARTIFACT/NEEDS-REVIEW confirmed. **Forecast: ~80 flips +
correlator dividend ≈ ~150 strict remaining in the flywheel → lands near ~19k/69k in
~4-6 more waves. Next order-of-magnitude requires a PIVOT (native/OSS-build/HW) — the
USER's call, to be made while the flywheel still produces.**

---
### [HIST] 2026-07-19 automation build-out — 18,689 → 18,819 (+130)
triage classifier built + 4-round calibrated, MISPAIR heuristic fixed, inverse
correlator built, grind waves drained productive buckets (ZS-inst +9, VocalPlayer
+7, foreman +5, MECH-LEVER +8, STRUCT-cal +4, calibration +7, correlator +2,
BODY-LEVER drain +17). Zero named regressions across ~16 landings.

## ⛔ PIVOT POINT (2026-07-19 pm) — cheap wire-and-flip / near-miss veins EXHAUSTED

Every coordinator-hand-wave vein was probed to exhaustion this session, each
gated cheaply with zero regressions:
- scatter expose-and-fix ≥88 band = **MIRAGE** (mispair / reloc-coloc / struct-artifact)
- struct-recon = **DEAD** (5/5 leads ICF/foreign-offset)
- near-misses = **AT_LIMIT** (regalloc / RB3<DC3 vtable)
- TrackWatcher = **NO-WAVE mirage** (own methods done, span = foreign scatter)
- grouped-globals = **1 fix** (SystemMs), rest banked
- unwired scatter-include = **+52, DRAINED** (7 cands, 5 flipped)
- unwired own-span wire-and-flip = **DRAINED ≈0** (body-port, not wire)

**What remains is DEEP GRIND: body-porting the ~103 unwired engine TUs + the
~5,300 divergent-body long tail (partial→100 via DC3/rb3-Wii oracle).** Per the
user mandate ("avoid deep grind unless high cascade") and Fable review #3, this is
a **work-kind pivot for the USER to decide**, not a unilateral coordinator grind.
Recommendation to bring the user: route the divergent tail to the AUTOMATED
machinery (crack-farm / grind-loop / the training-corpus model) — that pool is
exactly what it's built for — while coordinator attention moves to whichever the
user ranks of native-port / OSS-build / HW streams. Two explicit asks: (a) re-open
permuter or keep banned; (b) fund a divergence-triage pipeline as batch infra.
The id round-5 gate (+~1,000 names) is NOT reachable at the ~+70/session naming
pace, so the flywheel needs a bigger name-feed (body-port waves) to re-open.

## ▶ AUTOMATION BUILD-OUT (2026-07-19 pm, user-directed)

User decisions: **permuter stays BANNED**; **build the divergence-triage
classifier first** (price the automatable yield before funding any fleet), and
concurrently run Opus-foreman/Sonnet-worker grind waves whose outcomes serve as
ground truth to refine the classifier.

Fresh pool (report.json regenerated at 18,689 baseline, cache cleared;
`~/tmp/triage_pool.csv`): 7,723 named divergent fns / 2.97 MB. 6,341 at exactly
0% (unwired/scatter/unmapped mass); the divergent-body pool = 1,382 fns / 440 KB:
0–50: 292 · 50–75: 138 · 75–90: 260 · 90–98: 289 · 98–99.8: 145 · 99.8+: 258
(the 99.8+ band is mostly reloc-coloc residue — skip bucket).

In flight: (a) Fable tooling lead + Opus implementers building
`scripts/triage/divergence_triage.py` in wt-triage — buckets = mispair /
reloc-coloc / struct-artifact / form-divergence / body-port / zero-unwired,
features via batched `objdiff-cli diff -f json` + `scripts/analysis/
diff_inspect.py` analyzers; output `~/tmp/triage_{results.json,buckets.md}`.
(b) Opus grind foreman running 2–3 waves × 4–5 Sonnet workers on the 90–99.8
band (walls excluded via get_attempts), producing verified diffs for
coordinator landing + ranked tooling-gap feedback.

### Results (same day): classifier LANDED + calibrated, campaign +21, main 18,710

`scripts/triage/divergence_triage.py` on main (full pool 36s warm). Landed
gains: missing-instantiation vein +9 (`ba690393` + harvest), VocalPlayer grind
+7, foreman package +5 → **18,710**, zero regressions. Grind campaign ground
truth (24 assignments: 13 flips/3 improves): **route by diff shape, not %**
(screened 12/15 vs unscreened 1/9); I/D-cluster≥3 ≈ flip; regswap-only = skip;
97.5–99.8 = survivor-bias wall band, 78–96 = flip band. Full rules in memory
`project_grind_foreman_groundtruth_2026-07-19.md`.

**FINAL bucket table** (4 calibration rounds; snapshot committed at
`docs/plans/triage-buckets-2026-07-19.md`, regen with
`python3 scripts/triage/divergence_triage.py --jobs 12`): BODY-LEVER 240
(MEASURED per-stratum: 70-90 non-STL 25%, else ≤5%) · LEVER-STRING 41 +
LEVER-SYMBOL 9 (validated off 1 flip each — calibrate in first wave) ·
ZS-INST 17 (probe 2/2) · BODY-PORT 172 · STRUCT-ARTIFACT 175 + FORM-DIVERGENCE
146 (**UNMEASURED estimates — calibrate before funding**) · certified-skip 318
(RELOC-COLOC 160, WALL-VTORDISP 60, WALL-DEADARG 7, ZS-STL 84, STL-CONTAM 7) ·
MISPAIR 191 (map fix first) · UNRELIABLE-EVIDENCE 226 (stale live-diff, re-verify
before routing) · NEEDS-REVIEW 221 · ZERO-UNMAPPED 5,766.

**Honest fleet economics: bankable ≈96 expected flips** (BODY-LEVER ~26-35 +
LEVER-STRING ~36 + ZS-INST ~15 + LEVER-SYMBOL ~8 + BODY-PORT 78-96 ~3);
**estimate-only upside ≈149** (STRUCT 105, FORM 44) pending 20-30-fn calibration
waves. The original 530 was ~2.2× overpriced (BODY-LEVER measured 6.7% vs 80%
priced — calibration wave 30 fns: only 70-90 non-STL flips at 25%, STL 0/6,
mispairs 9/30). Calibration wave itself landed +7 incl. the **codec.h
`__forceinline` alloca lever** (6 vorbis fns / 1 line; intrinsic-wrapper class
swept — UNIQUE instance, closed). decomp.db drift: ~3k strict fns have renamed
symbol keys; treat get_attempts "not found" as unknown, not pass.

**Session arc (2026-07-19 pm, automation build-out): 18,689 → 18,717 (+28)**
— ZS-instantiation vein +9, VocalPlayer grind +7, foreman package +5, calibration
wave +7. Zero named regressions across all landings.

### MISPAIR bucket fixed (2026-07-19 late) — heuristic bug, 68 reclaimed

The MISPAIR prefilter was OVER-FIRING: it flagged `class-name ≠ attributed-unit-name`
as a wrong-pairing, but `CamShot`::Shake in `CameraShot.cpp` (and BSPFace/Geo,
kdTree/AmbientOcclusion, KerningTable/Font) are the SAME thing — the class just
doesn't string-equal the filename. Fixed (landed): Rule-1 delta made relative
(ratio>1.5 not bare delta>64), a2 class-vs-unit now uses a normalized subsequence
+ a cached class→defining-file index (resolves CamShot→CameraShot.cpp), a3 gated
to skip ICF/anon callee noise. **MISPAIR 191→123**; 68 reclassified —
44→BODY-LEVER, 11→WALL-VTORDISP, 4→LEVER-SYMBOL, 3→SCATTER-OWNER, rest.
3/3 hand-probed reclaims were REAL near-misses (CamShot::Shake 95.6%,
BSPFace::Update 94.8%, KerningTable::SetKerning 93.2%). 51 of 66 reclaimed are
≥70%-live grindable; net-new (not already in a running worklist) = 21 at
`~/tmp/grind_bodylever_reclaimed.json`.

### Priors re-measured (2026-07-19 late) — the "bankable 96" deflates further

Every bucket grinded this round came in BELOW its estimate — the pattern holds
(unmeasured priors ≈ multiples over). Landed +12 (MECH +8 → 18,725, STRUCT +4 →
18,729):
| bucket | priced | MEASURED | note |
|---|---|---|---|
| LEVER-SYMBOL | ~90% | **44%** | only real mech vein; named-Symbol evidence ≠ Symbol is the sole mismatch |
| LEVER-STRING | ~85% | **~5%** | heuristic near-NOISE — flags ObjPtr-2ctor(at-limit)/regalloc/struct as "string-reloc"; needs real string-lit-vs-`li 0` check |
| ZS-INST | high | **0% drained** | MakeString.h already all by-value → no const-ref producible; rest = middleware no-source |
| STRUCT-ARTIFACT | 50-70% | **~12-23%** | 68% mislabeled (mostly STL-template mispairs); bimodal (low-band layout bugs + 99.9% STL-stride; mid-band 0/9); most "deltas" are stack-frame-size not members |

STRUCT-ARTIFACT classifier refinements (recommended, not yet coded): quarantine
STL-template symbols unless a same-`T` sibling is already 100%; discard deltas
equal to `target_frame-base_frame`; require `this`-relative displacement; add a
"genuine-but-blocked" sub-label (foundational-MI-base / ICF-fold / multi-site RB3
divergence) so real-but-unflippable drift doesn't count as yield. The
`Hmx::Object+RndOverlay::Callback` MI base is +4 short across ~13 Rnd/Synth
classes — real foundational drift needing a coordinated cross-class fix.
**SongCollision +2 is gated out** (resize+_M_fill_insert flip but sibling
_M_fill_insert_aux regresses 100→99.87 on a contradictory 56B stride = its own
mispair) — becomes a clean +2 once the inverse-correlator repairs that sibling
pairing.

### Inverse correlator LANDED (2026-07-19 late, 18,744) — tool > its +2

`scripts/harvest/invcorr_mispair_repoint.py` (+ additive `relocs_full` in
`tu5_reloc_masked_correlate.py`) repairs `target_symbol_map.json` for true-mispairs,
applying ONLY unique-byte-identical repoints (guaranteed strict flip), reloc-verified
(position-wise (offset,type); PAIR/0x12 excluded; anon `fn_/lbl_/vftable_/…` =
unconfirmable-not-contradiction; contradicted candidates dropped BEFORE uniqueness
so they can't launder through the hamming/fuzzy fallback). Apply recipe: `--class
UNIQUE-IDENTICAL --apply` → `touch config/45410914/config.yml` (renamer never
un-names) → full rebuild → named-set diff both ways. **Of 122 true-mispairs: only
2 UNIQUE-IDENTICAL** (`__final_insertion_sort<MemDiffEntry>`,
`_List_base<OldMMInst>::clear`) → +2 landed, 0 regressions; 6 reloc-contradicted,
82 nomatch, 18 MULTI. **The vein is thin but the TOOL is the asset** — it's the
machinery to generalize over ZERO-UNMAPPED 5,766 (captain's primary post-drain lane).

Two follow-ups it surfaced:
- **SongCollision aux is NOT a map bug** — its true home `fn_825A38E8` exists but
  is 256B vs our 396B (retail out-of-lines fill/uninit_fill_n our /Ob2 inlines);
  repointing won't flip without fixing the inlining. **The gated SongCollision +2
  stays gated** (correction to the earlier "correlator unlocks it" assumption).
- **GemManager is a rotated-neighborhood mispair cluster** — reloc verification is
  self-referential there (PollHelper's target reloc resolves to PollHelper itself;
  the map names it trusts are themselves wrong). Needs a neighborhood re-derivation
  pass over PollHelper/UpdateArpeggios/MsToTick/Poll@NowBar together, not 1-by-1.

**Held (not landed, follow-ups):**
- **True-MISPAIR (120 remaining):** need an inverse-correlator mode that auto-repoints
  target_symbol_map ONLY on a unique byte-identical unmapped `fn_` (guaranteed
  strict flip), Ghidra-confirms low-hamming singles, hard-excludes ??_G/??_E/ICF/
  over-carve. Proven on `GameMode::SetMode` (mapped VA held an unrelated 84B fn;
  repoint to true 0x826901c0 + `touch config.yml` → 0→97% fuzzy, 0 named
  regressions) — but +0 strict alone and map edits are fleet-wide, so NOT landed;
  worth it as batch tooling. Map-fix recipe: repoint + `touch config/45410914/
  config.yml` (renamer never un-names — stale symbol persists without re-SPLIT) +
  full-rebuild + named-set diff both directions.
- **triage_pool.csv is now regenerated from the 18,717 report** (was stale at
  18,689 — flipped fns like VocalPlayer::UpdateMicDisplay were lingering in
  worklists). Regenerate the pool from current report.json before any extraction.

## Captain review (2026-07-19, at 18,742) — "Drain and repair, then re-measure"

Verdict: triage-and-calibrate is a FINISHING tool, not a growth engine — it did
its job (killed 2-12× overpriced priors before fleets burned, 0 regressions) but
realistic remaining strict from THIS machinery is **~+80-120** (~2 sessions):
BODY-LEVER untapped 59@59%≈+35, reclaimed 21≈+10-12, LEVER-SYMBOL≈+4, correlator
gated, STRUCT/FORM residue ~+20-30 deflated. Durable value = the certified-SKIP
fence (~400 fns) + honest routing. The real MASS is ZERO-UNMAPPED 5,766 + the
~5,300 divergent tail — post-drain, the cascade-shaped next vein is **generalizing
the inverse-correlator's reloc-masked byte-identity correlation over ZERO-UNMAPPED
at fleet scale** (identification, ~0.157 flips/name, feeds the round-5 +1,000-name
gate) + automation fleets on the divergent tail. Native/OSS/HW is the USER's call,
present only after the drain.

**Directive (executing):** Wave A = BODY-LEVER drain on untapped 74 (≥50% bar,
STOP if <40%); Wave B = reclaimed-21 (folded into A) + LEVER-SYMBOL 4 + held
one-offs; Wave C = land correlator repairs under guaranteed-flip gate, then
regen pool + re-run comdat_scatter_scan + id-stack stage-1 for the name-flywheel
dividend. After C, if bankable remainder <+30 → bring user the pivot (scale
correlator over ZERO-UNMAPPED as primary lane).

**Red flags fixed (now wave-preflight checklist):** (1) re-ingest decomp.db from
live report before every foreman wave (done at 18,742); (2) regenerate
triage_pool.csv from live report.json + `rm -f report.cache` before every
extraction/A-B leg (done); (3) cap first-touch calibration at 10-fn probes,
escalate to 30 only if the probe clears ~20%.

## Recent arc

| date | strict | delta | driver |
|---|---|---|---|
| 2026-07-17/18 mega-run | 17,445 | +2,081 | identification stack (+1,871 names), lane-B near-pair, naming wave, BandSwatch, struct leads |
| 2026-07-18 review | 17,445 | — | 3 Opus scouts ranked pools; `docs/plans/review-2026-07-18-next-focus.md` |
| 2026-07-19 body-port/recarve/scatter/id-flywheel | **18,621** | **+1,176** | the "mapped-but-0%" pool cracked open (see below) |

The +1,176 came from **one discovery and its flywheel**: the "mapped-but-0%" pool (functions with
real mangled names stuck at 0%) is overwhelmingly **COMDAT-scatter / TU-composition
drift**, NOT missing source. Retail MSVC/X360 (`/O1`, no LTCG) emits each function
into its own COMDAT and the linker scatters them across `.text`; dtk carves the
retail binary into per-source-file target objs by address range, so a function
whose COMDAT landed in unit X's span is attributed to X even though its source
lives in unit Y — and *our* obj for Y is the one that emits the matching bytes,
under a name objdiff never pairs into X.

### The three fix shapes (all landed, all regression-clean)

1. **Owner-TU whole-file include** — append `#include "<owner>.cpp"` to the
   span-owning `.cpp` so its obj emits the scattered COMDATs. INIT_REVS `gRev`
   collisions on double-include → byte-neutral `#define gRev gRev_<Owner>`.
   Landed: bp3 (+26), bp2r (+84), scatter-sweep w1 (+174). Idiom at HEAD in
   `TDStretch.cpp`, `MeshAnim.cpp`, `Console.cpp`.
2. **Retail-arity body duplication under `#ifndef HX_NATIVE`** — when whole-file
   include collides (statics/anon-ns/PROPSYNC barewords), copy just the needed
   bodies into the span owner with extern decls; native keeps canonical defs.
   Landed: bp1 (+36). Idiom in `Debug.cpp`, `DirLoader.cpp`, `MemHeap.cpp`.
3. **Splits gap-fill recarve** — when the auto blob is the missing *middle* of an
   already-pinned TU, add one gap `.text` range + reloc-masked byte-identity map
   entries (`tu5_reloc_masked_correlate.py`); ICF-twin MULTI groups resolve by
   order-preserving assignment; funclets cascade free. Landed: rc1 (+130,
   AccomplishmentPanel), rc3 (+14, TrackWatcherImpl).

**Instrument:** `scripts/harvest/comdat_scatter_scan.py` (~0.9s, re-runnable)
scans the COFF symbol tables of all ~836 compiled objs and splits every named-0%
function into **SCATTER** (emitted by another wired obj → owner-include/dup
fixable) vs **UNWIRED** (no wired obj emits it → gameport pool).

**Kill test before recarving any auto blob:** if the span's pre-mapped names are
emitted by *no* wired obj, the blob is a COMDAT catch-all from unwired classes —
a gameport target, not an attribution gap. (rc2/SongSort `0x826DD570` was
correctly skipped this way: SkillsAwardList / CampaignEra* / a NavListSortMgr
SongSortMgr redesign that matches DC3, not our older port.)

## Captain's plan (2026-07-19, Fable strategic review) — ACTIVE

**Key reframe (overturns the "scatter drained" verdict below):** the ~218
"net-0" scatter residue is NOT dead — it is a **near-miss discovery engine**.
Applying an owner-include PAIRS the scattered body in objdiff, turning an opaque
0% stub into a *diagnosed* fuzzy near-miss with a known owner source file + DC3/
rb3-Wii oracle. This is exactly how UpdateOverlay / UpdateCache / enableAAFilter
were found and then fixed to strict. Net-0 ≠ rejected; it means "here's a paired
body and its diff." **Frame for every wave: judged by strict flips + names fed to
the identification flywheel** (round-5 gate ~+1,000 names; body-ports buy it).

  **sw2-parent-leak guard (F1 discovery, load-bearing):** several sw3 consumers
  are themselves scatter-*owners* included by sw2-era parents (Morph←HamMove,
  DepthBuffer3D←UIList, Gem←OutfitConfig, …). Those parents bracket the include
  with `#define gRev gRev_<Child>` but do NOT set `SW_SCATTER_OWNER_INCLUDE`, so a
  naive owner-append leaks the new body into the parent TU and breaks it. Fix:
  guard the append to fire only in the consumer's PRIMARY TU. `gRev` is a static
  member *variable* (never a macro) in a primary compile, so `#ifndef gRev` is a
  reliable primary-vs-owner discriminator; where an internal block `#undef gRev`s
  before the tail (UIList's BandDirector block), use a stronger top-of-file
  `<UNIT>_SW3_PRIMARY_TU` sentinel instead.

- **Wave 1 — Expose-and-fix:** RAN 2026-07-19. Harvest → `~/tmp/expose_harvest.md`
  (9 freebies / 71 ≥88% / 118 compile-fail). **Actual yield: +10 total (F1
  freebies only; F2=0, F3=0, F4=0).** BIG EV MISS vs the +80–150 estimate — the
  ≥88 band is systematically blocked (recalibration below).
  **NEW CASCADE-SHAPED VEIN — DC3-oversized struct recon (F2 leads).** F2 proved
  the clean-building 99.9x targets miss on a single **struct-size immediate**: our
  DC3-sourced headers declare several structs LARGER than retail. Shrinking each to
  retail size flips its near-miss AND (cascade) every function that touches that
  struct — a shared-struct fix is wide-ripple by nature. Exact leads (each needs
  its own whole-binary A/B; gate DC3-newer fields behind `#ifndef HX_NATIVE`):
  **SongSection 0x18→0xc, RecurseInfo 0x18→0x10, BandIKEffector::Constraint
  0x1c→0xc, StoreMainPanel member −0x18, CharPollGroup base subobject −0x28.**
  This is the "B_STRUCT_OFFSET is the real vein" call (see A_TOOLING ICF memory),
  now with concrete targets. HIGHER EV than the mispair band.
  **PROBE RESULTS (2026-07-19) — REFINED PREDICATE, both wide leads DEAD:**
  S2 CharPollGroup = **misread** (the −0x28 was a member offset 0x50 vs an
  ICF-folded `??_G` dtor's full-object adjust 0x78; layout already matches retail;
  ground-truth against target-asm MEMBER offsets, NOT Ghidra `??_G` adjusts —
  ICF-contaminated). S1 SongSection = size mismatch is **real** (0x18 vs 0xc, DC3
  added mPatternRange+mSongPattern) but **cascade REFUTED** — its only
  `vector<SongSection>` consumers are 2 unimplemented stubs; **zero near-misses
  index it** → 0 flips. **THE RULE: a struct resize flips a near-miss only when a
  near-miss (90–99.99%) actually indexes that struct. Size-mismatch is necessary
  but NOT sufficient.** So the scanner predicate is NOT "struct size ≠ retail" —
  it's "struct size ≠ retail STL-stride AND indexed by ≥1 fn in the 90–99.99%
  band" (join size-deltas against the near-miss pool). The 3 narrow S3 leads
  (RecurseInfo/Constraint/StoreMainPanel) were each derived FROM a near-miss
  (99.9x), so they satisfy the predicate — S3 is the live test of the vein.
  **S3 RESULT — VEIN DEAD (all 5 struct leads mirages, 2026-07-19).** RecurseInfo
  0x10 is real but holds two 0xC Strings (=0x18; can't shrink without global
  String change). Constraint copy-ctor matches 100% at 0x1c (F2's `li 0xc` = a
  mis-paired ICF body). StoreMainPanel ctor matches 100% (F2's `addi 0x88` = a
  BandStorePanel singleton's return+0x88, foreign object). **Conclusion: F2's
  "struct-size" immediates were real numbers but SYSTEMATICALLY ICF-fold or
  foreign-offset artifacts, not oversized fields — the Movie::IsLoading mispair
  lesson generalized to the whole exposed sub-100 band. Do NOT fund a struct-size
  scanner sweep; do NOT re-hunt these. The ≥88 exposed band is a mirage across ALL
  three sub-taxonomies (mispair / reloc-co-location / struct-artifact).** Net from
  the entire struct-recon probe lane: 0, but 0 regressions (verify-before-edit
  gate held on all 5).
  **Mechanism rule (F2, durable):** an owner `.cpp` with its OWN nested
  scatter-includes is UNSAFE via the dialect shim — the push forces Object.h
  dialect and breaks the owner's nested ObjMacros-dialect includes, cascading to
  every TU that includes the consumer. Nested-scatter counts: HamCamTransform=9,
  BandCamShot=3, ViewSetting=2, HamNavList/Spotlight/HolmesClient=1; SAFE (0):
  SongLayout, CharEyes, ClipDistMap, CharPollGroup, TransAnim, FlowSetProperty,
  StoreMainPanel, BandIKEffector.
  **⚠ RECALIBRATION — the ≥88%-but-<100% exposed band is a MISPAIR MIRAGE.** The
  target-symbol renamer labels a physically-adjacent, ICF-shaped-but-semantically-
  DIFFERENT function with the exposed name, so "closing" the near-miss matches our
  code to the WRONG target. F4 proved every tiny "one-liner" was a mispair:
  Movie::IsLoading ("fixing" Movie 4→8B broke 10 MoviePanel funcs, net −9; our
  4-byte Movie is CORRECT, DC3's 8-byte doesn't apply to RB3), NetLoader::
  PostDownload (ours already stores 0x10 correctly), PlatformMgr::QueueEnumJob
  (target tail-calls a DIFFERENT function), OnSeedRandomContext (already 100 in its
  home unit). F3 proved the 99.8x `??_G`/STL residue is gapped by a reloc-arg
  (vtable/callee at a different scattered address) report.json won't forgive. **So
  only the exact-100.00%-on-include freebies flip; the sub-100 band is
  mispairs + struct-divergence + pairing artifacts. Do NOT re-hunt it as cheap
  near-misses.** UniqueFilename is the lone real crack — see vein #3.
  Still-untried Wave-1 items (separate from the mirage band): 3 body-dup cases
  (CameraShot←Flow, PropAnim←PropKeys, CharBonesMeshes←GemManager as `#ifndef
  HX_NATIVE` dup), MidiSynth WorldDir::PropSync trio (splits re-attribution —
  Dir.obj already emits), MemTracker::StopLog (map/splits).
- **Wave 2 — UNWIRED-OWNER SCATTER-WIRING = THE TOP LIVE VEIN (probe P2 GO,
  +9 @3917a0e4).** The winning shape: **117 `.cpp` files exist in-tree with full
  bodies but were never wired** (not in objects.json → no obj emits them; list
  `~/tmp/unwired_cpp_list.txt`). Retail scattered their COMDATs into an
  already-wired unit's `.text` span → a near-free `#include "<owner>.cpp"` append
  to that consumer emits + pairs them. P2: CubeTex.cpp += 4 includes
  (rnddx9/{MultiMesh,Cam,Lit,Part}.cpp) → +9 in ~5 min, 0 regr. Sweep running
  (`~/tmp/uwire_worklist.md`). **~60–65% clean flip rate**; FILTER OUT
  multiple-inheritance dtors (`??1`/`??_D`/`??_G` of 2+-base classes) — they ride
  a shared-base layout delta, only reach 99.x, route to a separate struct stream.
  Prioritize engine files (rnddx9/rndobj/synth/movie/os/net/midi) over gesture/*
  + Dance-Central hamobj/* (mostly Kinect, likely no RB3 target). EV: unknown
  addressable pool, but each hit is ~free. This SUPERSEDES the old "per-symbol
  owner-driven port" framing below — the bodies already exist; only the wiring
  was missing.
  **OWN-SPAN WIRE-AND-FLIP = DRAINED (2026-07-19, gated out ≈0).** The captain's
  "dark own-span pool, engine/lib-heavy, good byte-match prior" thesis was based
  on DC3's tree, not ours: the big C-lib pools don't exist in `src/` (jpeg=1 file
  not 73, zlib=1, oggvorbis=1, net=14/3-unwired not 107). Real unwired pool =
  **~103 engine files**. Best case (26 with pre-carved target objs, 20 compiled)
  → **5 byte-identity hits, ALL noise** (vtable-adjustor thunks + unwind funclets),
  0 real flips, no ≥3-hit clusters. Root cause: DC3-lineage bodies DIVERGE from
  retail RB3, and TU5 map-anchoring already carved every span that byte-matches an
  anonymous region — so the ~77 files with no target obj are precisely the ones
  whose bodies don't match. **These are BODY-PORT targets (partial→port to 100%
  via DC3/rb3-Wii oracle, the `bodyport-batch` skills), NOT wire-and-flip. Do NOT
  build a whole-binary own-span correlator.** With this, ALL cheap wire-and-flip
  and near-miss veins are exhausted → pivot territory (see PIVOT below).
- ~~**Wave 2 (old) — Oracle-backed UNWIRED wiring** (superseded by the above; the
  "port the bodies" premise was wrong — bodies pre-exist, just unwired).~~
  Original target census (for reference): rnddx9 CubeTex 8 Dx* + Rnd_Xbox(3),
  Anim(7), Sequence(8), MemTracker(8), DataPointMgr(5), WaveFile(4), Cam(2); game
  DataArraySongInfo(11), TrainerPanel(5), VocalTrack(3), VocalPlayer(3). SKIP
  oracle-poor (System/LEAPCORE, Mic, FFT, Compress, DSP, rtti/osfinfo).
- **Wave 3 — TrackWatcherImpl beatmatch gameport:** 121 flat-0% NAMED bodies,
  direct oracle `../rb3/src/system/beatmatch/TrackWatcherImpl.cpp`, splits
  already gap-filled (rc3). NOT banned grind — highest-cascade single target
  (biggest name-feed to round-5; RealGuitarTrackWatcherImpl.obj already owns
  scattered spans → landing beatmatch types unblocks chained proposals). Split
  4–6 agents by method cluster, 4488B monster last, accept partial. EV +80–140.
- **Micro-lane (no wave slot):** the 4 named near-miss probes (PreInit,
  InitParams, FindShader, SetTransform) + DxRnd::UpdateScalerParams / UpdateCache
  99.8 / enableAAFilter 99.5 / RingBuffer::Write 91.4 singles; grouped-globals
  **RECON ONLY** (count 80–97 fns citing shared-anchor `lbl_*` base+offset
  addressing — ≥30 → build a source-level global-aggregation mechanism, <10 →
  drop). → `~/tmp/grouped_globals_recon.md`.
- **Between waves:** re-run `comdat_scatter_scan.py` (chained proposals) + id
  stack stage-1 even below the +1,000 gate (~0.15 flips/name).
- **Pivot decision deferred ~3 waves:** after, the long tail is the ~5,300
  nomatch divergent-body pool — choose (a) scale Wave-1 expose-and-fix into a
  systematic divergence-triage pipeline, (b) grouped-globals mechanism if recon
  supports, or (c) pivot work-kind (native/tooling).

## Live veins (ranked by EV)

### 1. COMDAT-scatter sweep — reframed as EXPOSE-AND-FIX (see Captain's plan)
After 3 sweep waves (+661) the scanner reports **275 SCATTER candidates /
218 proposals** still open. Previously called "nearly drained / body-port-grade";
the captain's reframe (above) makes these the **cheapest diagnosed near-miss
fodder on the board** — apply the include to pair the body, harvest the exposed
%, fix the ≥88% ones. Cross-dialect walls unlocked by the wave-3 byte-neutral
shim `obj/dialect_object_{push,pop}.h`. Method is mechanical + gated (per-unit
whole-binary A/B, auto-revert on loss); **re-run the scanner between waves** —
fixing one owner unblocks chained proposals (w1's MidiSynth←PropSync only
appeared after PropSync←Dir landed).

### 2. UNWIRED gameport pool — 327 fns / 138 units
Functions no wired obj emits. Two sub-classes:
- **Engine, oracle-backed (portable):** rnddx9/CubeTex (8 Dx* ctors, DC3 oracle),
  Anim (7), Lit_NG, rnddx9/Rnd, Sequence — DC3 near-verbatim. These are true
  body-ports / TU wirings, ~medium cost.
- **Oracle-poor (defer, hard):** FFT (10 fns, VMX128 hand-asm — DC3's FFT unit is
  only 23%), System/LEAPCORE (32, no oracle), Mic + ExternalMic (25, Xbox voice),
  Compress/XGRAPHICS (10, shader-microcode), GranularSynth/SpectralAnalysis/
  PeakDetector (DSP hand code), rtti/osfinfo (CRT). Lowest ROI — leave for last.
- **Game (band3):** 16 units incl. TrainerPanel (5), DataArraySongInfo (11) —
  rb3-Wii oracle, gameport cost.

### 3. Exposed near-misses (fuzzy → strict fodder) — partly worked (nm +3, sm +3)
Pairing the scattered bodies revealed genuine near-misses hidden as 0% stubs.
DONE: NgRnd::UpdateOverlay/Terminate + MakeWorldSphere (nm, NgStats mSpotlights
strip + Geo.h fix); RndShaderMgr::Terminate/Invalidate + InitShaderOptions (sm,
ShaderType enum 38→26).
**AT_LIMIT (do NOT re-hunt, 2026-07-19):** RndShaderMgr::FindShader 80.3 and
SetTransform 81.7 — our source is byte-identical to the DC3 oracle; both are
pure callee-save-vs-volatile regalloc divergence (permuter-band, banned).
FindShader additionally has a HARD structural blocker: retail RB3 (2010)'s
`RndShaderMgr` vtable has **one fewer virtual than DC3 (2012)** — NewShaderProgram
sits at slot `0x5c` retail vs our `0x60`. DC3 is not an oracle for the vtable
shape; removing a virtual is a wide-ripple header change (re-lays every
ShaderMgr-subclass vtable) with no ground truth for *which* virtual RB3 lacks.
Prerequisite for any revisit: dump a concrete retail ShaderMgr-subclass vtable to
identify the missing virtual — a standalone structural task, not near-miss polish.
REMAINING leads: UpdateCache 99.8; enableAAFilter 99.5 (RateTransposer +16B
member — pad-probe); RingBuffer::Write 91.4; DxRnd::UpdateScalerParams 0%.
MemTracker::StopLog 77 = MISPAIRING (target is a MemFree/dtor, not StopLog —
map/splits fix, not source).
**UniqueFilename — CRACKED (F4 2026-07-19), needs an independent splits pin to
land.** The 2-line fix in `src/system/os/File.cpp` reaches 100.0% normalized
(Ghidra-verified vs `default_tu5.xex`): (a) declare `int i=0` BEFORE `String ret`;
(b) format string is hardcoded `"%s_%06d.bmp"` (drops the `c2` param — retail
ignores it and emits `.bmp` for both callers: Rnd.cpp:499 wants `.bmp`,
LiveCameraInput.cpp:1185 passes `"data"` but retail still emits `.bmp`). Can't land
now: UniqueFilename's COMDAT lives in Rnd's `.text` span, so the only measurement
path (`Rnd ← os/File.cpp` include) reshuffles objdiff pairing and drops
`GetNormalMapTextures` (rndobj/Utl) 100→94.5% — a pairing artifact, not a real
regression (`matched_functions` stays put, Utl.obj byte-identical). Give
UniqueFilename its own `splits.txt` `.text` range (carve out of Rnd's span, like
rc1/rc4 gap-fills) → then the File.cpp fix is a clean +1. Exact patch in F4's
report / this session's transcript.

### 4. Remaining recarve gap-fills
**0x82560660** UI-message run DONE (rc4 +48, UIStats gap-fill). **0x8234FCEC**
DataArray/ObjectDir SKIPPED by kill test (unwired gesture catch-all —
SkeletonFrame from gesture/Skeleton.cpp; recovery = wire that TU first). The
Accomplishment/TrackWatcher blobs are done; SongSort is UNWIRED (vein #2). The
easy gap-fill recarve targets are now exhausted; new ones require wiring an
unwired owner TU first (converges with vein #2).

### 5. Deep grinds (banked, lower EV)
- **TrackWatcher family — CORRECTED CHARACTERIZATION (2026-07-19).** The "121
  flat-0% NAMED bodies" framing is WRONG per the live report: `TrackWatcherImpl`
  is 159 fns / 45 matched / **78 at-0% but ALL anonymous `fn_` (0 named-0)** + 36
  named partials; `RealGuitarTrackWatcherImpl` 40/16/21-anon; family total ~104
  unnamed-0% + ~40 partials. Our source (872 lines, ≈ oracle 859) is largely
  ported. So the 0% pool is an **IDENTIFICATION gap (unmapped targets), not a
  body-port gap** — Wave-2 approach is **correlator-FIRST**: run
  `scripts/harvest/tu5_reloc_masked_correlate.py` on the TrackWatcher-family objs
  to pair our compiled named methods to the target's unnamed `fn_` by
  reloc-masked byte identity → add map entries → the byte-matching bodies flip
  (+ feed the id flywheel). ONLY the residual (unnamed, bodies diverge) + the ~40
  named partials are the actual body-port grind (oracle
  `../rb3/src/system/beatmatch/TrackWatcherImpl.cpp`, largest 4488B). Do NOT fan
  out a 4–6-agent body-port wave before the correlator run scopes the real
  residual.
  **CORRELATOR RUN DONE (2026-07-19) — it's a real body-port grind, NOT a cheap
  id win.** `tu5_reloc_masked_correlate.py TrackWatcherImpl.obj (target) vs our
  compiled obj` → only **14 UNIQUE byte-matches, ALL boilerplate** (`__unwind$`
  funclets + `bad_alloc` dtor); **0 real named methods match.** The 78 unmapped
  bodies are genuinely DIVERGENT (NOMATCH) — our source is a rough Wii port that
  doesn't byte-match 360 retail. So each flip needs a real body-port THEN
  correlator-pairing (unnamed target). EV per Fable (+80-140) is optimistic;
  recommend a SMALL probe (1 agent, ~8 representative bodies, measure port
  hit-rate) before committing 4–6 agents. If hit-rate is low, TrackWatcher is a
  low-ROI grind → pivot to oracle-backed unwired wiring (vein #2) or the
  round-5-prep / user pivot conversation.
  **PROBE VERDICT (2026-07-19): NO-WAVE — TrackWatcher is a MIRAGE.** The premise
  is wrong: TrackWatcherImpl has only 23 named methods, **22 already at 100%**
  (own methods effectively DONE); the "78 anon-0%" are FOREIGN functions
  scatter-interleaved into its 20KB pinned span (BandCrowdMeter, PartAnim,
  HamSupereasyData, Object, DataArray, STL templates — our source already
  `#include`s PartAnim.cpp + BandCrowdMeter.cpp). Correlator confirmed 0 real
  matches. Only residual = `CheckForAutoplay` 92.9% (permuter-class, deferred).
  The real (separate) opportunity buried here is BandCrowdMeter/PartAnim as
  first-class units (~20% near-misses, cross-TU layout problem, NOT clean
  porting). **Do NOT commit a TrackWatcher body-port wave.**
- **Grouped-globals wall** — RECON DONE (2026-07-19): verdict **NARROW, no
  mechanism wave**. Of 441 named 80–97 fns, only **17** are genuinely fold-walled
  and just **2 pure-fold** (the known MemFindAddrHeap/SystemMs). MSVC only shares
  a base register when the globals are *defined in the same TU as the accessor* —
  so cross-TU manager singletons (`TheBandDirector`/`TheLoadMgr`, `TheTaskMgr`/
  `TheUI`, `TheSessionMgr`/`TheSynth`, `ThePlatformMgr`/`region`) are UNFOLDABLE
  by any source change. Only **3 intra-TU clusters are source-fixable** (cheap
  micro-fixes, ~+2–3, fold into scatter campaign not a wave): MemHeap
  `gHeaps`+`gNumHeaps` (extern in MemHeap.cpp), Debug/System `gSystemMs`+
  `gSystemFrac` (extern in Debug.cpp, defined System.cpp), Voice
  `gCommitSyncVoices`+`gCommitTag` (in-TU, declaration-adjacency fix). Detail:
  `~/tmp/grouped_globals_recon.md`. Not a new mechanism — a facet of TU-drift.
- **DxRnd::UpdateScalerParams** (0x82739948) — paired at 0% since the vtable fix,
  genuine body-port lead.
- **BandCharacter −4 container compaction** (cr6), **BandCamShot vbase-MI
  reconstruction** (documented wall, pad-probe-killed the tempting +0x80 tail).

### 6. Identification round-4 — DONE +170 (`39038c09`), FLYWHEEL CONFIRMED
The scatter/recarve campaign's +250 names & 3 new pinned clusters cleared the
round-3 fixed-point gate. Re-running `scripts/harvest/TU5_SCANNER_STACK.md`
yielded **+170 strict** at ~0.157 flips/name (5x the collapsed 0.031 rate),
fixed point in 3 rounds, 6/6 Ghidra spot-checks. **Key insight: the scatter
vein FEEDS the identifier** — every owner-TU-include body flip creates a fresh
clean byte-identity pair the scanner then cracks. Round-5 not warranted until
+~1,000 more names. This coupling means future body-port waves should be
followed by an identification re-run.

## Dead / banned (do NOT re-hunt)
Permuter (user directive — low yield, grinds the box); ≥99 fixwave round-2
(rejected — 80% funclet mirage, ~20-30 fixable, no cascade); lane-B near-pair
residue (drained); A_TOOLING ICF fold mirage; pad-probe deferred struct walls
(drained); local-static mechanical wave; the 3 scatter-sweep w1 lossy candidates
(CameraShot←Flow, PropAnim←PropKeys/AmbientOcclusion, CharBonesMeshes←GemManager
— need body-dup, not whole-file include).

## Method (stable)
Fable coordinator delegates to Opus agents in `scripts/setup_worktree.sh`
worktrees under `~/tmp`; coordinator independently re-verifies every diff with a
fresh clean-worktree whole-binary A/B (strict set keyed `(unit, name)`, LOST must
be empty) before a path-limited commit on main; `touch config/45410914/config.yml`
before any A/B leg that changed splits/map (renamer re-split trap). Scoreboard:
`docs/plans/tu5-p5-progress.md`. Memory: `project_comdat_scatter_lever_2026-07-19`.
