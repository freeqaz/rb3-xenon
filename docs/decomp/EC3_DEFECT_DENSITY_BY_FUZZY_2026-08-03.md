# Lane EC-3 — testing EB-3's tractability inversion: defect density by fuzzy stratum

Tree `5555db76`, settled worktree. Companion data: `defect-signature-census-EC3.tsv`,
tool `scripts/harvest/defect_signature_census.py`.

**Leg A baseline (settled, 0 compiles):** matched 43,852 / matched_code 4,234,020 /
code% 39.611004 / masked_equal 22,724 / honest 21,128 / **254 units at 100%** of 948.
(Briefed `2e589b9b` figures were 43,848 / 4,233,200 / 39.603523 / 21,124; the two carve
commits since account for +4 / +820. `total_code` read **10,689,000** — it moved again.)

## The claim under test

EB-3 Finding 3: *"the census's cheapness ranking is ANTI-correlated with tractability …
the rows with real, source-shaped defects were the LOW-% ones. Rank by defect signature,
not by fuzzy%."* This was an inference from where EB-3 **failed** (it closed zero units),
not a measured yield — so it needed a population measurement.

## Instrument, and the calibration that makes it valid

`objdiff-cli diff -c functionRelocDiffs=none`. **Without that flag the census is
garbage**: on `MemcardMgr::SaveLoadAllComplete` the cli reads 80.93 and shows 12
`diff_arg` rows, of which **10 are masked symbol relocs — pure naming noise**. With the
flag it reads **82.6, exactly report.json**, and the real residue is 2 rows.

- Stratification is taken from **report.json** (authoritative); the cli is used only for
  the **instruction stream**, never for the percentage (they diverge up to 14.75pp).
- Health check: 1,644 rows, **0 errors**, 94.8% agree with report fuzzy within 0.5pp
  (3 rows >5pp).
- Population = **all 1,644 named charged rows** in real-source units — not a sample, so
  the density figures carry no sampling error. Excluded by construction: 22,365
  unpaired-anon rows (no `fuzzy` key at all) and the 1,174 40-byte `fn_8…` EH funclets
  (EB-3 Finding 4: derived work).

Two classifier bugs were found and fixed **before** reading any result, both of which
would have inverted the answer:
1. `real_arg = arg_type != 'symbol'` counted **register** diffs as source defects, which
   drove `CODEGEN_REGALLOC` to **zero in every stratum** — a decisive-looking artefact.
2. `SOURCE_INSDEL` was contaminated by **pure reordering** (objdiff emits insert+delete
   pairs for moved instructions). Split off as `CODEGEN_ORDERING` where the opcode
   multisets are identical and sizes equal.

## RESULT — share of stratum

| class | `<40` (169) | `40-70` (230) | `70-90` (515) | `90-99` (342) | `>=99` (388) |
|---|---|---|---|---|---|
| MAP/FOREIGN | **20.7%** | 2.2% | 0.6% | 0.6% | **0.0%** |
| STUB (unwritten) | **16.6%** | 0.0% | 0.0% | 0.0% | 0.0% |
| SOURCE-SHAPED | 62.1% | **90.9%** | **90.9%** | 71.1% | 61.1% |
| CODEGEN (permuter) | **0.6%** | 2.2% | 5.2% | 22.2% | **36.1%** |
| other/mixed | 0.0% | 4.8% | 3.3% | 6.1% | 2.8% |

**Work-to-cross — median mismatching instructions per row: 40 / 30 / 25 / 11 / 2.**

## Verdict: EB-3 is HALF RIGHT, and the half that is wrong is the actionable half

- ✅ **CONFIRMED (directionally):** codegen share rises monotonically with fuzzy,
  0.6% → 2.2% → 5.2% → 22.2% → **36.1%** (~60× enrichment). High-fuzzy residue really is
  enriched in permuter-class work.
- ⛔ **REFUTED — "the real source defects were in the LOW-% rows".** `<40` has a *lower*
  source-shaped share (62.1%) than the middle bands (90.9%), because **37.3% of it is
  map/foreign + unwritten stubs** — precisely the work EB-3 itself declined as
  metric-fitting, and which pays ~0 on the metric (a straight map repoint is a masked
  reloc). Peak fixable-source density is the **MIDDLE (40–90%)**, not the bottom.
- ⛔ **REFUTED — "cheapness ranking is anti-correlated with tractability".** Tractability
  is density ÷ work. `>=99` is still **61.1% source-shaped** and needs a median of **2**
  instruction fixes; `<40` needs **40** (20×). Ranking *ascending* by fuzzy is worse than
  ranking descending, not better.
- ✅ **EB-3's own headline — "rank by defect SIGNATURE, not by fuzzy%" — is RIGHT.** Its
  stated corollary about low-% rows is what fails. EB-3 worked a list ranked by fuzzy and
  *unfiltered by signature*, so it drew the 36.1% codegen rows and the single-row carve
  slivers (the map/foreign class). The fix is the filter, not a reversed sort.

### Which question does this answer?

This measures **where the defects are** (density). It does **not** measure where a fix
crosses a row to 100 — that is the `matched_code` all-or-nothing question, answered here
by the work-to-cross row. The two prior findings that appeared to conflict with EB-3
(crossing probability falls with fuzzy; rank by size-if-it-crosses at fuzzy ≥95) are
**consistent** with this: defect density is mildly higher in the middle, but work per row
is 12–20× higher there, so expected crossings per unit of effort still favour the high
band **once the codegen 36.1% is filtered out**. All three findings collapse into one
rule: *work the high-fuzzy band, filtered by signature.*

### Direct counter-example to "high fuzzy ⇒ codegen, not source"

`?SetType@CharBlendBone@@` sits at **fuzzy 99.85** and all 12 of its mismatches are one
uniform constant (retail `0x38`/`0x3c` vs ours `0x30`/`0x34`) — the **Object vbase
displacement**, i.e. the exact family lane BQ-2 fixed for **+8 verified**. A real,
source-shaped, previously-proven-tractable defect living at 99.85%.

### Actionable lever extracted (for the next lane)

47 rows / **9,268 B** have a *single uniform immediate delta* (45 of them at `>=99`).
16 are `vector<T>::_M_insert_overflow_aux` where the delta is **`sizeof(T)`**, each
naming a concrete class (DynamicProp +128, ArchiveSkel −388, CamShotFrame −228,
LocalizedEntry +48, …). ⚠ Adjudicate per row: memory refutes the *shift-amount* variant
of this as a struct-size oracle (0/14).

⚠⚠ **A uniform delta is NOT enough — check the base register.** `?Save@PlayerDiffIcon@@`
(99.94, uniform +4) resolves to `addi r4, r1, 0x54` vs `0x50` — **r1 is the stack**, so
it is stack-slot placement, i.e. codegen. The census stores the r1-vs-object split
(`immctx`) precisely for this; across `>=99`, 26% of immediate diffs are stack-relative.

## Row dossiers (part B)

### `EQEffect::Process` — CLOSED as codegen, with a witness that discriminates
EB-3 left this open because the uniform `off:±24` "could be a real layout defect that
would dissolve the 46-instruction regswap". **It is not.** Retail anchors `r9 = this+0xbc`
with offsets `-0x20..+0x8`; we anchor `r9 = this+0xa4` with `-0x8..+0x20`. **Every
effective address is identical** (`0xbc−0x20 == 0xa4−0x8 == 0x9c`, `0xbc+0 == 0xa4+0x18`,
`0xbc+8 == 0xa4+0x20`). The layout is provably right; the residue is MSVC's base-anchor
choice (retail anchors on `mDelayE`, we on `mDelayB`) plus a 2-register renumbering
(`samples` ptr vs `this+0xcc`) and commutative `fmadds` operand order — all three
previously-refuted or permuter-class levers.

### `MemcardMgr::SaveLoadAllComplete` — DX-3's reasoning refuted; row still unresolved
DX-3 declined this as a map mispair because `Init` (316 B) and `OnSaveGame` (180 B) match
retail at 100% "carrying the same vbase sequence". **That witness is weaker than claimed:
the vbase chain in *code* is offset-agnostic** (it loads the displacement from the
vbtable at runtime), so a 100% match proves the *access pattern*, not where `Object`
lands. And the row is almost certainly the **right** function — the target has the
local-static guard, `SaveLoadAllCompleteMsg` ctor, `atexit`, and the trailing
`DataArray::Release` DataNode teardown, matching our source shape (27/34 instrs equal).

Live contradiction, both sides on retail bytes: retail passes `r4 = this+0x20` to a
`Handle`-shaped callee, but `SetProfileSaveBuffer` matches retail 100% as
`stw r4,0x20,r3 / stw r5,0x24,r3`, i.e. `0x20` is `mSaveDataBuffer`. ⚠ That 12-byte body
has **zero relocations**, making it a prime ICF fold — so its 100% may prove only *which
body we equal, not our home*. Authoritative layout puts `Object` at 0x88/0x8c, not 0x20.
**Declined** a speculative vbase change (wide blast radius, no established target layout).

### `CharBlendBone::SetType` — sized and located precisely, NOT named
Needs **+8 bytes** before the `Object` vbase (ours vtordisp 0x30, retail 0x38).
- Own members are **proven correct** by 1,300 B of byte-exact retail code
  (`SyncProperty` 696 B, `Poll` 372 B, `Save` 232 B, all 100%).
- `mSetLocal` is **proven absent** — re-ran the prior lane's probe with the *correct*
  instrument (Python, not the binary-blind grep shim): `set_local\0` = **0** occurrences
  while siblings `src_one\0` and `trans_x\0` = 1 each. The prior lane's evidence stands.
- DC3 names no trailing member beyond `mSetLocal`; `ObjVector`→`ObjList` is already right.
⇒ retail has **8 bytes of unnamed, non-property, non-saved, non-polled trailing state**.
**Declined** to add anonymous padding — that is metric-fitting, and BQ-2 had to undo
exactly such a pad (`0149637d`, fitted to a mispaired `??_G`). Worth +316 B / +1 fn when
named.

### Bonus: a fresh wrong-callee-at-100%
`?ClassName@CharBlendBone@@UBA?AVSymbol@@XZ` scores **100%** while the target calls
`?StaticClassName@CharIKFingers@@` and we call `?StaticClassName@CharBlendBone@@`.
`ClassName`/`SetType` bodies are byte-twins modulo the one discriminating relocation, so
they fold — another instance of "byte identity proves which body you EQUAL, not your HOME".
(`CharIKFingers` is 424 B with a far later vbase, so `SetType` is *not* mispaired to it.)

## What I did NOT do

- **Landed no source change.** Every row I opened was either codegen (EQEffect,
  PlayerDiffIcon), or a real defect I could size but not *name* (CharBlendBone), or
  contradictory on retail bytes (MemcardMgr). I will not pad or repoint to move a number.
- Did not run `tools/native_build_gate.sh` — no `src/` change survives, so it does not apply.
- Did not use the permuter (standing directive).
- Ran the **full population automatically** rather than hand-adjudicating a fixed N per
  stratum; the automated labels come from real retail-vs-ours instruction bytes, and 6
  rows were additionally adjudicated by hand (EQEffect, MemcardMgr ×3, CharBlendBone ×2).
  The hand pass found **no auto-label wrong**, but N=6 is too small to publish a
  confusion rate — treat the class labels as *signatures*, not adjudicated defects.
