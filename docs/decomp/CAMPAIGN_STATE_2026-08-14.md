# Campaign state — 2026-08-14

A consolidation written after a run of ~20 lanes closed several veins with
evidence. **Its purpose is to stop the next lane re-funding a drained vein.**
Every closure below is a *measurement*, not fatigue, and each names the number
that closed it.

⚠ **Do not read an absolute score out of this file.** `total_code`,
`total_functions` and every percentage move when pins change, and the two
headline keys are computed on different rulers. **Read `report.json`**, and read
its `provenance` block for the ruler — it self-declares.

---

## 1. The ceiling: price headroom against ~61%, not 100%

`matched_code_percent` **cannot reach 100**. Measured (lane NOOBJ-1,
`13de1453`; census self-validates — rows sum to `total_functions`, bytes to
`total_code`, zero dropped):

| class | rows | bytes | % `total_code` |
|---|---|---|---|
| PAIRABLE | 54,691 | 6,512,524 | **63.10%** |
| UNPAIRABLE — no source | 4,454 | 2,106,356 | 20.41% |
| UNPAIRABLE — `auto_*` | 10,083 | 1,701,784 | 16.49% |

Corrected the same day (lane AUTOID-1, `eb7fd2b1`): **63.10% is ~1.75pp
optimistic** — 105 units counted PAIRABLE have base objs defining only 1–2
symbols (914 rows / 180,196 B). **True reachable ceiling ≈ 61.35%.**

⇒ a measured 34.6% is **~56% of the reachable surface**, not 34.6% of it.

**Both unpairable classes are closed:**

- **No-source (20.41%)** — the 230 units are **already declared** in
  `objects.json` with a `src_path` that does not exist; `tools/project.py` drops
  the compile edge **silently** (1,434 declared − 1,204 compiled = 230). **229
  of 230 are `xdk/*` Microsoft vendor source.** ⛔ **Do not stub them into
  compiling** — that buys pairable rows at 0% with no content.
- **`auto_*` (16.49%)** — only **8.9%** (1,766 rows / 151,024 B = 1.46% of
  `total_code`) is attributable **and portable**, at 0.64% FP. Two-thirds is
  flanked by XDK source we lack or by **7-line Quazal map scaffolds**
  (`namespace Quazal {}`; 103 of 117 `network/` sources are <20 lines, median 7).
  Upper bound if every MIXED row proved ours: **5.51%**.

---

## 2. Veins closed with evidence — do not re-fund

| vein | verdict | closing number |
|---|---|---|
| `mpn==100 / fuzzy<100` stratum | **depleted, not enriched** | real-defect class **10.7% vs 12.1% control = 0.88×**; ~91% irreducible |
| `auto_*` attribution | mostly unportable | **8.9%** attributable-and-portable |
| no-source XDK | needs Microsoft source | 229/230 units |
| map naming (argument types) | **broadly sound** | **12 hard mispairs** across 28,940 rows; 0.19% vs a ~18.9% permutation null |
| body-port, structural half | **not reachable** | **0 of 47,820 B**, predicted; residue is pure-`reg` permuter-class |
| `MILO_*` emissions retail kept | **drained** | **4** in the whole binary; 4 now correct |
| unsuffixed double literals | real but nearly empty | **3 of 73** witnessed; **0 crossable** |
| REFUTED relocation-name pairs — **BYTES** | **71% ICF-fold-gated** | 1,573 rows / 464,564 B of 654,564 B; trustworthy worklist **~2% / ~13,000 B** |
| factory-registration lists | **seam, now worked out** | **17** retail multi-lists (base bounded by the *binary* — two independent enumerations both return 17); 5 differed, **all membership, 0 pure order**; **12 identical** after repair; remainder = 4 unported classes + 1 missing function |

⛔ **CORRECTION to this file's own first edition (2026-08-14).** It recorded the
REFUTED queue as *"map/fold queue, not source — 1 map · 0 source defects."*
**That is REFUTED.** It was measured on a hand-adjudicated handful; a full census
of all 2,294 real-name pairs (lane WRONGCALL-2, `abdbfd6b`) found **448
SOURCE_CAND, of which 202 have BOTH names retail-verified AND bodies genuinely
different — the one combination that can only be a wrong callee** — and landed
**three** source defects (`Synth360::Init`, `Rnd::PreInit`, and a third recorded
at `InlineHelp::Init`). ★ **What survives is the BYTE claim, not the defect
claim**: the queue's *bytes* really are 71% fold-gated, so it is not worth
funding **for bytes** — but it is **not source-clean**, and the 202 remain
unworked. The original row above has been replaced accordingly.
| `json_tokener` version skew | refuted | our 12 error strings incl. the `"nesting to deep"` typo ⇒ json-c 0.9, correct for 2010 |

**Retired rows** (documented negatives, do not re-open without new evidence):
`?Handle@CustomizePanel@@` — with both its ICF folds settled its **fuzzy
converged exactly to its mpn (99.76172)** and objdiff reports **zero `diff_arg`
rows**, so **no relocation-name work can move it by one byte**; three lanes
circled it. Also `?Handle@BandStorePanel@@` (diagnosed; only lever is
metric-fitting), `?Interp@CameraShot@@` (association lives in shared math-header
inlines ⇒ engine-wide fan-out), `?SyncProperty@BandWardrobe@@` (tried, inert).

---

## 3. The mechanism catalogue — eight causes, every one invisible to a source diff

Found by adjudicating retail bytes on rows that looked like scheduling noise.

1. **Code retail never had** — post-ship Harmonix fixes imported from
   **dc3-decomp (newer than RB3)**, or **rb3-Wii dev-build** code retail
   compiled out. Instances: an 8-slot recursion-safe prop-path pool; three
   `(angle != angle)` NaN guards; a dev-only `sDisableEyeClamping` read; a
   `str.empty()` early-out; a `LoadSafely+bool` test; an `sRandomOverride` hook.
2. **Declaration-point displacement** — MSVC emits a local static's guarded
   initializer **at its declaration point**; 17 hoisted `static Symbol`s pushed
   retail's opening block from instructions 4–18 to 196–208. ⛔ Censused as a
   one-off (2 further candidates, both ineligible) — *check*, don't sweep.
3. **Same value, different expression** — retail read the **parameter**
   `&color`; we read **the copy we had just stored**. Fixing it collapsed **all
   11 mismatches to 68/68 equal**.
4. **Flag polarity** — retail holds `dot > 0.0f` positive; **two branch-opcode
   mismatches dissolved from a rename**.
5. **Storage-class divergence** — retail uses function-local statics where our
   `Symbols.h` has globals.
6. **`/fp:fast` barriers** — parens are one; **a block-local named temp is
   another** (`sel * -eyeRot` reassociates to `-(sel * eyeRot)`).
7. **We inherit defects from the ORACLE** — five `if (mPreviewDesc)` guards that
   **rb3-Wii carries verbatim** while retail dereferences straight through.
   ⇒ ★ **oracle agreement is NOT clearance; only retail bytes catch this class.**
8. **The dead `this` home is a source-shape oracle** — MSVC `/O1` homes the
   vbase-adjusted `this` of an inlined **member** call into a dead stack slot, so
   its presence/absence witnesses **member vs non-member at an inlined call
   site**. Companion: a trailing `clrlwi rX,rX,24` says the callee returned
   `bool`.

★ **When retail shipped a real bug, reproduce it in the match build and keep the
fix under `#ifdef HX_NATIVE`.** Do not delete it — the native runtime is the
stated real goal. Same for dev-only paths: gate, don't remove.

---

## 4. Two failure patterns that recurred all run

### 4a. Instruments that cannot fail (≥9 instances)

Every one produced a **clean, decisive-looking number**, and every one was
caught only by a control that could fire:

- controls at `0/57,696`, `0/15,005`, `0/86`, `checkable=0`
- a census keyed on the wrong JSON field returning `{}` for **all 36 rows**
- an `arity_screen` subcommand that was **dead, not degraded** — it crashed
  before analysing anything
- a scanner fooled by a **prose comment containing `#if`**
- a retail reader anchored `^\s+` against dtk lines that begin with `/* addr */`
- a **non-recursive `obj/*.obj` glob** missing 569 of 3,084 live objs
- a `collect()` that is **last-wins across duplicate COMDATs** (`?PreInit@Rnd@@`
  is defined in **nine** objs), reporting a **real source edit as inert** on a
  partially-built tree — **an obj newer than its source was still stale**

⛔⛔ **AND A WORSE DIRECTION, found 2026-08-14: instruments that fail toward
*MANUFACTURING* work.** Everything above fails toward *finding nothing*, which
wastes a lane. Lane REGORDER-1 had **four** fail the other way, and acting on
any of them would have **damaged correct code**:

- a **mis-transcribed `RegisterFactory` VA** (`2188758400` read as `0x82756B40`
  instead of `0x8275CD80`) made the scanner answer **"NOT REGISTERED ANYWHERE IN
  RETAIL" for ALL 19 queries — including FOUR PROVEN POSITIVE CONTROLS.** That
  is precisely what a lane about to delete registrations wants to hear, and
  **only the controls caught it.**
- **keying retail slots on SYMBOL NAMES** dropped every unnamed slot and
  reported surplus registrations that do not exist ⇒ **would have deleted
  correct code**.
- comparing retail literals to **class names** invented ~20 phantom defects in
  one function (retail spells `RndFur` as **"Fur"**).
- **best-overlap matching** paired retail's `Synth360::Init` against our
  `Synth::Init`, inventing a 5-registration defect.

⇒ **Run positive controls BEFORE acting on an absence, not after.** And note the
inverse failure in the same lane: treating an unnamed slot as a **wildcard**
*hid* a real defect a predecessor therefore could not see.
- a census that **inflated itself by construction** by not subtracting shipped
  aliases (80/20/50 → 64/49/37)

⇒ **Before believing any clean number, demonstrate the check CAN fire.** A
shrinking census looks identical to good news; a passing gate looks identical to
a healthy tree.

### 4b. Relayed inferences (5 instances, all caught)

A predecessor's site note is a **hypothesis**, not a finding. Five were relayed
into briefs unverified and refuted by the receiving lane: the `lwzx`
dereference witness; "retail shipped the `DrawToTexture` notify"; a `none`-ruler
**mismatch count** that manufactured a phantom 5,036 B prize for three lanes;
"gated entirely on those two folds"; and "the 49 REFUTED are retail calling
something our source doesn't."

⇒ **All five were caught because each lane was briefed to TEST the premise
rather than execute it.** Keep briefing that way.

---

## 5. What remains

- **Permuter-class residue** — the dominant remaining body-port class.
  **The permuter is OFF by user directive.**
- **46 unadjudicated REFUTED pairs**, plus 37 UNDECIDABLE — now known to be a
  **map/fold** queue, so budget accordingly.
- **24,555 of 26,873 charged pairs (91.4%) have a retail-side placeholder** —
  that is **identification coverage**, untouchable by any fold tool.
- **141 `STALE_SPELLING` alias groups** — deliberately **not** pruned; pruning
  is measured harmful (`a745039e` restored 14 such at **+94,616 B**).
- Named residuals with diagnoses recorded at their sites: `PracticePanel::Poll`
  remainder, `CharIKHand::IKElbow` (`fsubs` operand order + a structural −0x10
  frame delta), `VocalTrackDir::PostLoad`, the `RndFlare::CalcScale`
  CSE/scheduling divergence now *exposed* rather than masked.

---

## 6. Standing operational rules earned this run

- **Run `tools/native_build_gate.sh` as your LAST action** — a **comment-only
  `docs(src)` commit broke the native link** (prose containing `#if` desynced
  `ScatterIncludes.cmake`'s frame counter, silently unguarding an
  `#include "math/mtx.cpp"` 212 lines away ⇒ 17 duplicate definitions).
- **Price a candidate from `report.json`'s charged-site list, never from a
  `run_objdiff` mismatch count** — that count is `none`-ruler and undercounts.
- **A map repair pays on TWO channels** — the call sites *and* the row's own
  re-pairing.
- **`none`-flat is the alias-suspect SIGNATURE, not a clearance.** Answer
  `ALIAS_SUSPECT` by **pre-registering the exact rows and bytes**: a fabricated
  alias forgives an arbitrary set of sites; a real fix moves the specific set
  the model predicts, to the byte.
- **A charged pair names two symbols and the defect need not be either.**
- **Bodies equal under the same name ⇒ the map name is right** (the sound
  converse of a screen already proven vacuous in the other direction).
