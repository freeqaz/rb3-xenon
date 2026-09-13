# The cigar's swapped ring and the DOFProc name that finally paid — lane W13-C, 2026-09-13

**Date** 2026-09-13 · **Branch** `w13-cigar-and-dofname` off `main@49b5a79f` ·
**Worktree** `~/tmp/wt-w13-c` · **Branch tip** `a412b115`
**Ruler** `functionRelocDiffs=name_check` (graded), read from `report.json`;
`objdiff-cli` pinned across both legs of both A/Bs (`sha256:a5c35b15d7d46ac4`).

Two jobs. Task 1 took dc3's four-part `UtilDrawCigar` patch and re-derived every
part against **RB3 retail bytes** instead of inheriting dc3's rationale — which
cites DC3's own target listing. Task 2 took W12-C's handoff 1: name
`0x82466080`, which W12-C had measured at **−180 B** before the source fix that
has since landed.

## 0. Result

| stage | change | mpn | Δ whole-binary | verdict |
|---|---|---:|---|---|
| A | `v2` y/z sine phases un-swapped | 82.63761 → **82.63761** | +0 fn / +0 B | **real bug, metric-blind** |
| B | `r0`/`r1` + lon sines `double` → `float` | → **89.23395** | +0 fn / +0 B | dc3 claim 1 **SUPPORTED** |
| C | split `h1` into two statements | → **89.23395** | — | dc3 claim 2 **REJECTED** |
| D | drop the `iJcur` induction temp | → **90.61009** | +0 fn / +0 B | dc3 claim 3 **SUPPORTED** |
| — | **Task 1 aggregate, settled A/B** | **82.63761 → 90.61009** | **+0 / +0**, Δfuzzy +0.000675pp | 0 units off 100% |
| 2 | name + re-home `0x82466080` + src | `Terminate` **fuzzy 100** | **+1 fn / +80 B** | predicted exactly |

Both A/Bs pre-registered in full before building. Neither moved a unit off 100%.

## 1. Which of dc3's three shaping changes RB3's bytes support

Stated plainly, because that was the question.

**Claim 1 — `double r0/r1` → `float`: SUPPORTED, and it is the whole payload
(+6.60 pp).** `fn_8243A868`'s FP census is 12 `fmuls`, 4 `fadds`, 2 `fsubs`,
2 `fmadds`, 2 `frsp`, 2 `fcfid`. dc3's phrasing — *"retail uses `fmuls`, no
`frsp`"* — is **not literally true**: there are two `frsp`. But both sit
immediately after an `fcfid` fed by `lfd 0x50(r1)` / `lfd 0x60(r1)`, i.e. they
are the two int→float casts `(float)iIdx` and `(float)iLon`. **Zero `frsp` are
attributable to double arithmetic**, so the substance holds: RB3 carries r0, r1
and both longitude sines single-precision, and our double temps were forcing an
`fmul`+`frsp` at every use.

**Claim 2 — split `h1` to block an `fmadds`: REJECTED as a lever, though its
diagnosis is right.** Two separate findings:

* RB3 *does* contract, twice — `fmadds f0,f13,f13,f0` and `fmadds f0,f12,f12,f0`
  in the `sqrtf(...)` scale expression at the top. dc3's blanket *"which retail
  does not do"* is wrong as stated. What is true is narrower: **at the `h1`
  site** RB3 emits `fmuls f0,f1,f0` then `fadds f27,f0,f24`, uncontracted.
* We **do** contract there — and **the split does not stop it.** Ours is
  `fmadds fr27,fr0,fr1,fr24`, with the same register assignment as retail
  (`fr27`=h1, `fr24`=sLen1). Probed with a standalone `/FAs` compile over four
  spellings — single expression, dc3's two-statement split, and both with the
  add operands reversed — **all four emit `fmadds=3` against retail's 2.** So
  the Δ0 measured on the metric is not a scoring artifact: the change is inert
  **at the codegen level**. Reverted.

  ⇒ The residual is real and remains: **ours has 3 `fmadds` / 11 `fmuls` /
  3 `fadds` where retail has 2 / 12 / 4** — exactly one contracted pair. It is
  not reachable by rearranging that expression. Handoff 1.

**Claim 3 — drop the `iJcur` temp: SUPPORTED (+1.38 pp).** RB3's ring-draw tail
is `mr r11,r27` / `addi r27,r27,0x1` / `cmpwi cr6,r27,0x6`, with the preheader
`li r11,5` / `li r27,0`: `iK` takes the old `iJ` with no staged temp and the
compare is on the **incremented** value. Our `iJcur` spelling kept a third value
live and compared `iJcur + 1`.

## 2. The phase swap is a real bug the metric cannot see

Confirmed on RB3's own bytes, not on dc3's say-so. In the inner loop
`f21 = FastSin(lonVal)` (plain) and `f22 = FastSin(lonVal + π/2)` (cos-phase),
and the stores are

```
v1:  0x80 <- h0b     0x84 <- f22*r0     0x88 <- f21*r0
v2:  0x70 <- h1      0x74 <- f22*r1     0x78 <- f21*r1
```

**Both rings put the cos-phase sine in y and the plain one in z.** Ours had `v2`
the other way round, so the second ring was rotated 90° in that plane.

Fixing it moved **nothing**: mpn `82.63761 → 82.63761` and fuzzy
`81.30734 → 81.30734`, identical to the last digit, on a build where `Utl.obj`
demonstrably recompiled (edge 5/18) — so not absent-vs-absent. The swap only
exchanges which of two `fmul`+`frsp` results reaches which store, and with the
doubles still in place the score cannot distinguish them. **A genuine behavioural
defect at Δ0** — the project's own thesis in miniature, and the reason it was
landed on correctness rather than on the number.

## 3. Task 2 — why the name pays now, and why W12-C's own forecast was low

W12-C measured naming `0x82466080` at **−180 B** *before* `afdd72f7`, and
forecast **~0 B** after it. The brief for this lane said it "should now pay".
**Both were partly wrong, in opposite directions, and the reason is instructive.**

W12-C was right that the *call site* is forgiven either way — `name_check`
forgives a placeholder target, so `?Terminate@Rnd@@UAAXXZ` read 100 before and
reads 100 after (measured both legs). The payment does not come from there. It
comes through the **pairing** channel, and only because of a third change
neither prediction included: our `DOFProc::Terminate` carried a **DC3-era
`DataVariable("the_dof_proc")` block** that retail has not got, which cost a
static guard, a `DataNode` assignment, an EH funclet and `bl __savegprlr_29`
where retail has an inline prologue saving only r31 — **256 B against retail's
80**. Pinned and named without removing it, the row pairs and still cannot
match.

With it removed (`#ifdef HX_NATIVE`, house pattern) our body is
**instruction-for-instruction retail's**, same 20 instructions and same
registers. Three coupled parts, none of which pays alone:

1. **src** — remove the DC3-era block (else the body can never reach 100).
2. **splits** — re-home `0x82466080–0x824660D0` out of `Mat.cpp` into
   `DOFProc.cpp`, whose two blocks merge to `0x82466000–0x82466298` (else the
   row lives in Mat's target obj, which our `Mat.obj` can never define).
3. **map** — `0x82466080` → `?Terminate@DOFProc@@SAXXZ`.

### 3.1 `lbl_82CC6368` is `TheDOFProc` — W12-C's open question, settled

Two independent ways. **(a) Referencer sets.** Retail touches that global from
`FreeCamera` (8 refs), `CameraShot` (4) and `CameraManager` (2); our source's
`TheDOFProc` users are `FreeCamera.cpp`, `CameraShot.cpp`, `CameraManager.cpp`.
A `lis`+`lwz` pair is 2 refs per access, so CameraManager's 2 sit against our
single `if (TheDOFProc) TheDOFProc->UnSet();` and CameraShot's 4 against our two
sites — an exact per-site correspondence, not just a set overlap.
**(b) Codegen.** After part 1 our body puts `?TheDOFProc@@3PAVDOFProc@@A` in
precisely retail's `lis`/`lwz` slots.

### 3.2 dtk corroborated the re-home by itself

The `.text` edit made the split rewrite its own input once (the documented
split-guard; the retry is the fixed point). What it wrote is evidence:
DOFProc's three `.pdata` ranges merged to `0x8220ED18–0x8220ED50` and `Mat.cpp`
lost `0x8220ED28–ED30`. **The unwind record followed the function** — derived
independently of the map row, from `.pdata` alone.

### 3.3 The alias screen, done by address

Per the standing rule (a lane last wave predicted +3,852 and measured +836 on an
unscreened alias orphaning): `82466080` occurs **0 times** in
`scripts/symbol_aliases.json`, so no group is anchored there and none is
orphaned. `Mat.cpp` keeps 6 `.text` blocks, so it cannot vanish.

## 4. Measurements, verbatim

Task 1 aggregate (`--from-dirty`, both legs settled):

```
  Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
  Δfuzzy=+0.000675pp   (legA 49.153210 -> legB 49.153885)
  units at 100% [mpn ruler]: legA 164 -> legB 164  (Δ+0; 0 reached 100, 0 fell off)
  units at 100% [all-rows-fuzzy ruler]: legA 136 -> legB 136  (Δ+0; 0 reached 100, 0 fell off)
```

Task 2 (`--from-dirty`, both legs settled **and** at a `symbols.txt` split fixed
point, `renamer_patched=1822` on leg B):

```
  Δmatched=+1  Δmasked_equal=+0  Δhonest=+1  Δcode%=+0.000782pp  Δcode_bytes=+80
  unit improvements: 1 unit(s), sum +1
      +1  default/DOFProc  (7->8)
  units at 100% [mpn ruler]: legA 164 -> legB 164  (Δ+0; 0 reached 100, 0 fell off)
  units at 100% [all-rows-fuzzy ruler]: legA 136 -> legB 136  (Δ+0; 0 reached 100, 0 fell off)
```

Whole binary at the branch tip: **42,840 matched / 3,891,704 B / 37.982830%**
(from 42,839 / 3,891,624 / 37.982048% at `49b5a79f`).

## 5. What this lane deliberately did NOT do

* **Did not chase the surviving `fmadds`.** Four spellings probed, all inert; it
  needs a different instrument than source rearrangement. Handoff 1.
* **Did not touch `DOFProc::Init`.** It carries the same DC3-era
  `DataVariable` block, but retail's `Init` has **not been located** (W12-C
  showed `0x824660D0` is `??$New@VDOFProc@@…`, not `Init`), so there are no
  retail bytes to adjudicate against and removing it would be a guess. Handoff 2.
* **Did not re-home `~GranularSynth`/`Synapse_dsp.obj`** — out of scope, a
  splits lane, and not metric-neutral.
* **Did not take `UtilDrawCigar` past 90.61.** The remaining ~9.4 pp was not
  diagnosed; it is not claimed to be permuter-bound or at-limit, only unexamined.
* **Did not price stages A–D individually with `ab_measure`.** `matched_code` is
  all-or-nothing per row and this row never reaches fuzzy 100, so every stage is
  structurally +0 B whole-binary; the discriminating measurement is per-function
  mpn from `report.json`, which is the same graded ruler. The aggregate *was*
  priced with a settled A/B, which is what proves no regression.

## 6. Handoffs

1. **One `fmuls`+`fadds` pair is contracted in our `UtilDrawCigar` and is not in
   retail's** (ours 3/11/3 `fmadds`/`fmuls`/`fadds`, retail 2/12/4), at the `h1`
   site. Four source spellings are measured inert. Whoever takes it should treat
   the expression rearrangement vein as **drained** and look elsewhere (the
   `/fp:fast` paren-barrier note, or accept it as at-limit) — but note it is a
   single instruction on an 872 B row, so price it against the other ~9.4 pp
   first, which nobody has diagnosed.
2. **`DOFProc::Init` still carries the DC3-era `DataVariable` block**, and
   retail's `Init` is still unlocated. Finding it would settle whether the same
   removal applies there. Note the shape that worked here: the block's removal
   was adjudicated by *comparing our compiled COMDAT to retail's bytes*, which
   requires having retail's bytes — so locating `Init` is the prerequisite, not
   an optional extra.
3. **A standalone `/FAs` probe is a much cheaper loop than a full build** for
   codegen questions (~20 s vs ~46 s, and it cannot perturb the build tree or
   the six obj patchers). Recipe: lift the edge's `cflags` out of `build.ninja`,
   invoke wibo's `cl.exe` directly with `/FAs /Fa<scratch> /Fo<scratch>`, and
   cross a `sh -c` boundary so the flags actually word-split (zsh will not).
   It is what turned "the split measured Δ0" into "the split is provably inert
   and the divergence survives it".
