# What does 100% mean — endgame taxonomy and recommended target

> Status: DRAFT-RFC | Date: 2026-07-08 | Author: Claude Opus (paths-to-100 wave) | Theme: strategy

## Summary

"100%" is undefined for rb3-xenon and the ambiguity is costing us: the metric of
record (whole-XEX byte-match) has a denominator that is **94.6% middleware/CRT/XDK
code we have no source for**, so it can never reach 100 and its headline number
(8.69%) badly understates real progress on the parts we care about. This RFC
defines five candidate endgames precisely, maps each to what current tooling
*measures* vs what the endgame *needs*, and recommends a primary: **endgame B
(100% of GAME code, engine credited via DC3) as the north star, with per-scope
metrics promoted to first-class**, plus **endgame C (shiftable/bootable relink) as
the credibility milestone**. Whole-XEX strict (A) stays as an honest denominator
of record but stops being the *goal*.

## Motivation

Every other RFC in this set (`02`–`20`) optimizes *toward* a target. If the target
is "11,074,108 code bytes at 100%," most of those RFCs are rational-but-doomed:
they grind functions that live inside `src/band3` and `src/system`, which together
are **5.5% of the binary** (verified below). The remaining 94.6% is Bink video,
the MSVC CRT/LIBCMT, the Xbox XDK (`d3d`, `xgraphics`, `xam`, net/crypto), and
unidentified `.text` — none of which we have source for and most of which we never
will (`docs/plans/path-to-100.md` §0 P7: "months-years, low ROI, no source
exists"). Declaring the endgame fixes three things at once:

1. It tells the sequencing roadmap (`03-master-sequencing-roadmap.md`) where the
   finish line is, so vein-ROI accounting (`18-metrics-and-dashboard.md`) measures
   against a reachable number rather than an asymptote we can't touch.
2. It resolves the denominator question the brief raises: does whole-binary stay
   the honest metric, or do game%/engine%/middleware-excluded% become
   first-class? (This RFC: **both** — keep whole-binary as the honesty anchor,
   promote per-scope as the steering metrics.)
3. It decides whether "bootable artifact" is in scope at all — which determines
   whether the full-splits/relink work (`19-shiftable-relink-milestone.md`) and
   native-port work (`20-native-port-and-engine-reuse.md`) are core or optional.

## Current state (verified)

All figures from `build/45410914/report.json` and `tools/fuzzy_progress.py`, main
@ a1312de, 2026-07-08. Cross-checked against the brief's SHARED FACTS — all match.

**Whole-binary strict (the metric of record):**
- Functions: **11,240 / 65,619 = 17.13%**
- Code bytes: **962,656 / 11,074,108 = 8.69%**

**Scope decomposition** (bucketed by unit name over all 2,456 report units):

| Scope | total code B | matched B | strict % | share of binary |
|---|---:|---:|---:|---:|
| game (`band3` + `network`) | 481,860 | 140,244 | 29.10% | 4.4% |
| engine (`system`) | 117,440 | 32,004 | 27.25% | 1.1% |
| other (vendor/xdk/crt/bink/**unsplit**) | 10,474,808 | 790,408 | 7.55% | **94.6%** |
| **SUM** | **11,074,108** | 962,656 | 8.69% | 100% |

Two things this table makes undeniable:
- **The parts we actively decomp are ~5.5% of the binary** (game 4.4% + engine
  1.1%). Even game+engine at a *perfect* 100% would move the whole-binary
  metric only from 8.69% to ~11.2%. The whole-XEX number is dominated by
  scope we don't work.
- The "other" 790,408 matched bytes are **not** a real decomp achievement — they
  are mostly small ICF/boilerplate matches and whatever the pinned-but-misfiled
  units caught; the honest game/engine progress is the 29% / 27% columns.

**How much is even identified yet** (the deeper problem behind the denominator):

| category | code B | % of binary |
|---|---:|---:|
| inside **named/split** units | 3,132,020 | 28.3% |
| still in **unidentified `auto_03_*_text` blobs** | 7,942,088 | 71.7% |
| (of the unsplit, BINK-named auto blobs) | 65,292 | 0.6% |

So **72% of the binary has not even been carved into candidate translation units
yet** — it's raw dtk address-range `.text` slabs (`auto_03_8290FAF8_text` alone is
1.69 MB). This is why `04-pinning-at-scale.md` and `02-gap-composition-atlas.md`
exist. The whole-XEX denominator is not just large — it is mostly *unmapped*.

**Fuzzy (progress signal, per `tools/fuzzy_progress.py`):**
- WIRED-set fuzzy **94.602%** over 1,392,316 attempted bytes (n=13,584 fns) — the
  primary fuzzy goal. Staircase ≥95: 12,743 | ≥90: 13,021 | ≥80: 13,175.
- RB3-specific (band3/network): 5,962 fns, 3,364 wired, wired-fuzzy **91.02%**.
- engine (system): 17,531 fns, 10,220 wired, wired-fuzzy **95.72%**.
- other: 42,126 fns, **0 wired** — confirming the middleware bucket is untouched.

**The two confirmed walls** (from the brief, corroborated by
`docs/decomp/research/2026-06-30-topo-locator-design.md` and
`docs/decomp/research/2026-06-24-pivot-bodyport-classb-results.md`):
(1) **identification** — class-B ICF-scattered methods can't be located
(topo_locate precision 0.13, BSim 0.24); (2) **body-divergence** —
correctly-identified fns stall <100% on MWCC(Wii)→MSVC(X360) codegen. Both walls
constrain *game* progress specifically, which is exactly the scope endgame B
targets — so the endgame choice and the wall RFCs are coupled.

**Bootable-relink tooling status (verified, refutes/qualifies a scout claim):**
`docs/plans/oss-build-path.md` **exists** and its claim is real but narrower than
the scout implied. It proves steps 1–3 (compile `.c`→PPC COFF `.obj` machine
`0x1f2`; `link /LIB /DEF`→import lib; link→valid PPC PE DLL) **end-to-end on this
machine, XDK-free**, with disassembly proof of real Xbox-360 PPC `.text`. Step 4
(PE→XEX2 pack) is explicitly **PLAUSIBLE, not proven** — "no open-source PE→XEX2
builder exists," three mitigation paths, ~2–3 days to a Xenia-bootable artifact.
**Critical caveat the scout omitted:** that doc packages the *RB3Enhanced
same-instrument runtime-mod DLL* (a tiny additive hook TU), **not a full relink of
the whole decompiled binary.** It proves the *toolchain seam* is open, not that we
can relink RB3. Do not cite it as "we can build a bootable RB3 XEX from our
sources" — it proves we can build a bootable *mod DLL*.

## Proposal

### The five endgames, defined precisely

**Endgame A — Whole-XEX strict byte-match.**
*Definition:* `matched_code / total_code = 100%` over all 11,074,108 bytes;
every function in `report.json` objdiff-equal to its dtk target.
*Measured today:* exactly this (8.69%). No new tooling needed to *measure* it.
*Needs to reach:* source for Bink (proprietary, no source), the full XDK
(`d3d9`/`xgraphics`/`xam`/net/crypto — `docs/plans/xdk-dependency-audit.md`), the
MSVC CRT, and identification of all 7.94 MB of unsplit `auto_*` code. `path-to-100.md`
§0 rates the XDK shader-compiler RE at "months-years, low ROI." *Feasibility:*
**effectively unreachable.** Honest ceiling per `path-to-100.md`: "~17–25% of
binary functions, ~30–50% of non-XDK code bytes."
*Verdict:* keep as the **denominator of record / honesty anchor**, not the goal.

**Endgame B — 100% of GAME code, engine credited via DC3.**
*Definition:* every function in `src/band3` + `src/network` objdiff-equal, with
`src/system` (engine) treated as *pre-solved by the DC3 Rosetta Stone* rather than
required to hit 100% byte-match here.
*Rationale:* CLAUDE.md "Decomp priority" + `engine-reuse-and-asset-rendering.md`
prove DC3 is the *same engine on the same platform* and its compiled engine
*already loads and renders RB3-360 assets*. The RB3-specific value is the game
layer. This is the endgame the whole project is *already* implicitly pursuing.
*Measured today:* the "RB3-specific (band3/network)" sub-goal in `fuzzy_progress.py`
(5,962 fns, 2,798 at ≥100, wired-fuzzy 91.02%) is *exactly* this metric — but it
is currently a secondary readout, not the headline.
*Needs to reach:* (a) promote game% to a first-class metric of record
(`18-metrics-and-dashboard.md`); (b) break the two walls for game code —
identification (`04`,`05`,`07`,`08`,`09`) and body-divergence (`13`,`14`,`15`);
(c) a defensible "engine = credited" definition so we're not silently excluding
game code that DC3 lacks. *Feasibility:* **reachable in principle** for the
identified portion; blocked on the two walls for the class-B/MWCC-divergent tail.
*Verdict:* **RECOMMENDED PRIMARY north star.**

**Endgame C — Shiftable/relinkable XEX that boots.**
*Definition:* full per-unit splits over the whole `.text`, reloc-normalized
equivalence (à la the "shiftable" milestone in melee/mkw-style decomps), then a
PE→XEX2 repack that boots our compiled objects on Xenia/RGH. This is
`19-shiftable-relink-milestone.md`'s territory.
*Measured today:* **not measured.** We measure per-unit byte-equality at *fixed*
addresses; we do not measure whole-binary link closure or run a bootable repack.
*Needs to reach:* (1) splits for all 7.94 MB of `auto_*` (`04-pinning-at-scale.md`);
(2) source (even stub/asm-passthrough) for every unit so the link closes — the
hard part, since middleware has no source; (3) the PE→XEX2 packer from
`oss-build-path.md` step 4, **generalized from a single mod-DLL to the full image**
(a large unproven leap); (4) a reloc-normalized equivalence metric.
`../jeff/target/release/dtk` (the local jeff fork) already does the XEX→objs
split; the inverse (objs→bootable XEX) is the open work. *Feasibility:*
**partially proven at the seam** (oss-build-path steps 1–3), **unproven at scale.**
The "boots but is a Frankenstein of our-objs + original-objs" hybrid is the
realistic near-term form (like a decomp that link-swaps matched TUs into the
original image). *Verdict:* **credibility milestone, pursue as C-hybrid** (swap
matched units into the original image and boot), *not* full from-scratch relink.

**Endgame D — Fully playable native port.**
*Definition:* RB3 runs as a native x86-64 binary via `native/` + the DC3/milo
engine, playable end-to-end. `20-native-port-and-engine-reuse.md`'s track.
*Measured today:* not a decomp metric at all — measured by "does it boot / render
/ play." `native/` currently boots headless and loads `songs.dta` (CLAUDE.md
"Native engine build"). *Needs to reach:* the entire game layer *ported* (not
byte-matched) + engine reuse from DC3 + input/audio/render glue + the
`milo-native-engine` extraction. Note: native port needs *correct* code, not
*byte-matching* code — it's a **different acceptance criterion** than A/B/C and can
consume matched *or* merely-equivalent (unicorn-EQUIVALENT,
`17-unicorn-equivalence-lane.md`) functions. *Feasibility:* **reachable as a
separate product**, orthogonal to byte-match %. *Verdict:* **parallel track, not
the byte-match endgame** — but the strongest *demonstrable* payoff and the reason
game-layer decomp has value beyond a percentage.

**Endgame E — A + C gold standard.**
*Definition:* whole-XEX strict 100% *and* a bootable relink — the "matching decomp
that also builds the shipping binary" gold standard. *Feasibility:* gated on A,
which is unreachable. *Verdict:* **aspirational only; do not plan against E.**

### Recommended target and metric policy

**Primary north star: Endgame B.** Game code to 100% (identified portion), engine
credited via DC3. **Credibility milestone: Endgame C-hybrid** (matched units
link-swapped into the original image, boots on Xenia). **Parallel product track:
Endgame D** (native port) as the demonstrable payoff. **A stays as the honesty
denominator; E is dropped.**

**Metric policy — promote per-scope to first-class.** Concretely:

1. **Metric of record (headline), three numbers side by side, never one alone:**
   - `whole-binary strict %` (honesty anchor — the 8.69% / 17.13% dc3-comparable
     number; keeps us honest that most of the binary is middleware).
   - `game% = matched_code(band3+network) / total_code(band3+network)` — today
     **29.10%** — the **steering metric** for endgame B.
   - `engine% = matched_code(system) / total_code(system)` — today **27.25%** —
     tracked but *not* a grind target (DC3-credited; only matched where cheap).

2. **A `middleware-excluded%`** = matched / (total − BINK − XDK − CRT − unidentified
   `auto_*`). This is the "of the code we could plausibly ever have source for, how
   much is matched" number. Requires a scope-classifier for the `auto_*` blobs
   (`02-gap-composition-atlas.md` builds it; `10-middleware-and-denominator.md`
   owns the exclusion list). Until that classifier exists, approximate with
   "inside named units" (3.13 MB) as the denominator.

3. **Fuzzy staircase per scope** stays the *progress* signal (it climbs before
   strict does), already emitted by `tools/fuzzy_progress.py`. Guard against
   WIRED-denominator inflation with `tools/icf_alias_check.py` per the honesty
   gates.

**Data flow (all tools already exist):**
```
build/45410914/report.json ──► tools/fuzzy_progress.py  (per-scope buckets: game/engine/other)
                            └─► [NEW ~40-line] tools/scope_metrics.py  (headline triple + middleware-excluded%)
                                    │  uses the same unit-name → {game,engine,middleware,unsplit} classifier
                                    │  as 02-gap-composition-atlas.md
                                    └─► 18-metrics-and-dashboard.md dashboard
tools/icf_alias_check.py ──► honesty gate on WIRED denominator (ICF-fold inflation)
../jeff/target/release/dtk ──► (endgame C) whole-.text splits; objs→XEX repack is the open work
```

### Milestones (per endgame, so the roadmap can sequence them)

- **B-1:** `scope_metrics.py` lands; game%/engine%/whole% become the committed
  headline in `18`. *(days)*
- **B-2:** game identification wall progress — any of `05`/`07`/`08`/`09` moves the
  band3/network *wired* fraction (currently 3,364 / 5,962 fns) upward. *(weeks)*
- **B-3:** game body-divergence wall progress — `13`/`14`/`15` convert
  identified-but-<100% game fns to 100%. Anchor: past grind +22 (a1312de),
  bodyport waves.
- **B-4 (endgame B "done" definition):** every *identified* band3/network fn at
  100%; residual is explicitly the class-B-unidentified + MWCC-unfixable tail,
  documented not hidden.
- **C-1:** whole-`.text` split coverage from 28% → high-90s (`04`).
- **C-2:** link-closure spike — can our objs + original-obj passthrough link into a
  valid PPC PE? (extends `oss-build-path.md` steps 1–3 to N units).
- **C-3:** PE→XEX2 repack of the hybrid image; boots in Xenia. *(the
  `oss-build-path.md` step-4 leap, generalized — highest-risk milestone)*
- **D-track:** owned entirely by `20`; milestone = playable vertical slice.

## Alternatives considered

- **Keep whole-XEX strict as the sole goal (status quo).** Rejected: the number is
  dominated by unreachable middleware; it demotivates and misdirects. A grinder
  who moves game from 29%→40% sees the headline move 8.69%→~9.2% and concludes the
  work was worthless — false.
- **Redefine the denominator to game+engine only and report "45%".** Rejected as
  the *sole* metric: it's denominator-gaming (the exact sin CLAUDE.md's "Known
  issues" warns against — "no denominator gaming"). The fix is *additional*
  first-class per-scope metrics *alongside* the honest whole-binary one, not
  replacing it.
- **Make D (native playable) the primary.** Rejected as *primary* because it
  decouples from byte-matching entirely — a native port can be "done" with zero
  new byte-matches (it needs correct, not identical, code). Kept as a parallel
  track and the demonstrable payoff, but the decomp's *identity* is byte-matching.
- **Make C (bootable relink) the primary.** Rejected: C's hardest milestone (C-3,
  full PE→XEX2 of a hybrid image) is unproven and gated on 72% of the binary being
  split first. Better as a milestone that *validates* B's matched units than as the
  north star.

## Effort & expected value

This RFC itself is **~1 day**: write `tools/scope_metrics.py` (~40 lines over the
same classifier `02` needs), wire it into the dashboard (`18`), and get the
endgame definition adopted so sibling RFCs sequence against it. That is the entire
*direct* cost. The *value* is leverage, not matches: it re-points every other RFC
at a reachable target and stops grind effort being scored against an 11 MB
denominator that is 94.6% unreachable.

Honest EV anchoring, using past results in this repo:
- Promoting game% surfaces that band3/network is already **29.1%** and the
  proven levers (class-A span harvest +403 one session; grind best-of-N +22;
  Waypoint objptr-flip +7; CharClipGroup micro-pin) all move *this* number
  efficiently. The steering-metric change makes those wins legible.
- Endgame B's reachable ceiling is bounded by the two walls: of 5,962
  band3/network fns, 3,364 are wired and 2,798 already at 100%. The realistic B
  target is roughly the *wired-and-not-class-B* set — low thousands of fns, not
  all 5,962. Do **not** promise "100% of game" as if the class-B/MWCC tail were
  free; it is the confirmed wall.
- Endgame C-hybrid EV is a *credibility* payoff (a booting artifact proves the
  matched objects are real), not a %-payoff. Anchor its feasibility to
  `oss-build-path.md`'s proven steps 1–3, and its risk to the unproven step 4.

## Risks & failure modes

- **Metric proliferation → nobody trusts any number.** Mitigation: exactly three
  headline numbers (whole/game/engine) + one derived (middleware-excluded); all
  from one committed `scope_metrics.py`, all cross-checked by `fuzzy_progress.py`.
- **"Engine credited via DC3" becomes a fudge** that silently drops hard game code
  DC3 happens to share. Mitigation: the credit rule keys on *unit path*
  (`src/system` = engine-credited) not on "DC3 has it," and any band3/network fn
  stays in the game denominator regardless of DC3 overlap.
- **Betting on C, then step-4 XEX packing proves infeasible at full-image scale.**
  Mitigation: C is a *milestone*, not the north star; the C-hybrid (swap-into-
  original) form de-risks it, and D (native) is an independent demonstrable payoff.
- **Denominator reclassification hides regressions.** Mitigation: keep whole-binary
  strict reported forever; `icf_alias_check.py` + cold-cache A/B gates unchanged.

## Kill criteria

- **Kills the metric-policy change:** if `scope_metrics.py`'s game%/engine%
  numbers are within noise of the whole-binary number (i.e. game/engine are *not*
  meaningfully denser-matched than the whole) — refuted here: 29% and 27% vs 8.7%,
  a 3× gap, so the split is clearly informative. (Re-check if it ever converges.)
- **Kills endgame B as primary:** if the two walls (identification +
  body-divergence) prove to cap identified-game-fn matching so low that game%
  plateaus far below ~50% *and* no RFC in `05`–`15` moves it — then game-100% is
  as unreachable as A and the honest move is to pivot the north star to D (native
  playable, which tolerates equivalent-not-identical code).
- **Kills endgame C:** if the link-closure spike (C-2) shows our objs cannot link
  against original-obj passthrough into a valid PPC PE, *or* the step-4 packer
  can't produce a Xenia-booting hybrid after the `oss-build-path.md` 4a path is
  attempted — then C is shelved and the project is A-denominator + B-steering + D
  only.

## Open questions

- Where exactly is the game/engine/middleware boundary for the 7.94 MB of
  unsplit `auto_*` code? `02-gap-composition-atlas.md` must answer before
  `middleware-excluded%` is trustworthy; until then it's approximated by
  "inside named units."
- Should `network` (Quazal/RockCentral) be counted as game or middleware? It's
  RB3-specific game logic *but* sits on the Quazal NetZ middleware
  (`10-middleware-and-denominator.md`). This RFC counts `src/network` as game;
  revisit if the Quazal library portion turns out to be vendor code we can't
  source.
- Is the "melee/mkw-style linkable milestone" the right model for C? **[UNVERIFIED]**
  — the scout cited it but no doc in `docs/` references melee/mkw or a "linkable
  milestone" (only `lto-vs-icf-investigation` mentions "relink" tangentially). The
  *concept* (shiftable/reloc-normalized full-splits build) is sound and is what
  `19` should define; there is no existing rb3-xenon precedent doc for it.
- Does `native/` extraction into `../milo-native-engine` (CLAUDE.md architectural
  goal) change D's scope enough to make D cheaper than assumed? Owned by `20`.

## References

- `docs/plans/oss-build-path.md` — compile→PE-link (steps 1–3) PROVEN XDK-free;
  PE→XEX2 pack (step 4) PLAUSIBLE. **Scope: a mod-DLL, not a full relink.**
- `docs/plans/engine-reuse-and-asset-rendering.md` — DC3 engine renders RB3-360
  assets; the "decompile the GAME not the engine" evidence base (endgame B).
- `docs/plans/path-to-100.md` — [HIST] 394-era roadmap; §0 phase ranking + the
  honest "~17–25% fns / ~30–50% non-XDK bytes" ceiling; P7 = XDK RE, months-years.
- `docs/plans/xdk-dependency-audit.md` — the XDK surface (part of the "other" 94.6%).
- `docs/plans/lto-vs-icf-investigation-2026-06-06.md` — ICF-active verdict.
- `docs/decomp/research/2026-06-30-topo-locator-design.md` — identification wall
  (topo_locate precision 0.13).
- `docs/decomp/research/2026-06-24-pivot-bodyport-classb-results.md` —
  body-divergence wall (BandProfile 0/64 despite correct identity).
- `CLAUDE.md` — "Decomp priority: the GAME, not the engine"; "no denominator
  gaming"; native-build + `milo-native-engine` extraction goal.
- `build/45410914/report.json`, `tools/fuzzy_progress.py`,
  `tools/icf_alias_check.py`, `config/45410914/splits.txt` (1,216 pinned `.text`
  ranges), `../jeff/target/release/dtk`, `native/CMakeLists.txt` — verification
  sources for every number above.
- Sibling RFCs: `02-gap-composition-atlas.md` (classifier for the 94.6%),
  `03-master-sequencing-roadmap.md` (sequences against this endgame),
  `10-middleware-and-denominator.md` (the exclusion list),
  `18-metrics-and-dashboard.md` (consumes `scope_metrics.py`),
  `19-shiftable-relink-milestone.md` (endgame C),
  `20-native-port-and-engine-reuse.md` (endgame D).
