# W16-EK — the fold-thunk gate measured its two sides differently

Lane W16-EK, 2026-09-16, worktree `~/tmp/wt-w16-ek`, branch `w16-ek`, off `2045465b`.

Adjudicates the two defects lane W16-EH diagnosed in `tools/fold_thunk_gate.py`
(`docs/decomp/W16EH_BUTTONDOWNMSG_DECODE_AND_SHUTTLE_SETACTIVE_2026-09-16.md` §4.2–4.3)
and, if they hold, fixes them and collects the row they block.

## 0. Pre-registration (written before the fix was measured)

Recorded before the patched gate was run even once.

| # | prediction |
|---|---|
| P1 | the `Shuttle::SetActive` / `Metronome::Enable` pair flips REFUSE -> ADMIT, at tier **FT-EMPTY** (no relocation on either side) |
| P2 | house 36-pair worklist: **zero** verdict changes -- ADMIT 7 / REFUSE 29 before and after |
| P3 | wide 1,048-pair probe: **0** ADMIT->REFUSE flips and **<=2** REFUSE->ADMIT flips |
| P4 | all 5 existing gate/alias tests still PASS |
| P5 | whole-binary A/B: **+260 B**, **+1** matched function |

**Named ways this could fail, also pre-registered:**

- **(a)** a previously-ADMITted pair flips to REFUSE, because an unrelocated
  displacement is now genuinely compared and differs. That would be a real
  defect exposure -- correct, but it would withdraw evidence already relied on,
  and it is reported rather than suppressed.
- **(b)** the wide probe shows MANY REFUSE->ADMIT flips. That would mean I had
  broadly loosened an admission gate rather than corrected an asymmetry --
  the metric-fitting hazard the brief forbids -- and the lane stops there.

## 1. Adjudication: BOTH claimed defects hold, and they are COUPLED

Re-derived from the code, then reproduced by running the gate. Neither was
inherited from the brief.

### 1.1 Defect one — the two sides were derived by different methods

`Retail.canon()` INFERRED retail's relocation set from instruction form
(pre-fix `tools/fold_thunk_gate.py`):

```python
            elif op in IMM16_OPS:
                targets[4 * i] = None          # unresolvable in a linked image
```

`our_canon()` READ ours out of the COFF relocation table:

```python
    for off, nm, ty in cd["fn_relocs"]:
```

and `compare()` refused on the difference:

```python
    if set(rt) != set(ot):
        return False, ("relocated fields at different offsets: retail %s vs ours %s" ...)
```

For `stb r4,8(r3); blr` the `8` is a plain literal, so a LINKED image has no
relocation there at all -- but `stb` is op 38, which is in `IMM16_OPS`, so the
retail side recorded `targets[0] = None` regardless. Our COFF side is correctly
empty. The refusal therefore fired on **pure asymmetry, after the masked words
had already compared equal**. Reproduced verbatim on a one-pair worklist:

```
REFUSE -            1  ?Enable@Metronome@@QAAX_N@Z <- ?SetActive@Shuttle@@QAAX_N@Z
         our COMDAT is not the retail survivor body -- relocated fields at
         different offsets: retail [0] vs ours []
```

This is the ONE-SIDED INSTRUMENT ERROR class. It is invisible to a two-sided
control because the artifact cancels on both legs -- the same shape as the
phantom "+8 B STLport source bug", where `coff_bodies_ext.py` billed the
successor symbol's EH funclet into one side of a COMDAT span.

### 1.2 Defect two — the `+8` the gate claims to check was never checked

`mask_word` zeroed the low 16 bits of every IMM16-form word. Measured directly
against the real function:

| word | raw | masked (pre-fix) |
|---|---|---|
| `stb r4,8(r3)`     | `0x98830008` | `0x98830000` |
| `stb r4,0xc(r3)`   | `0x9883000c` | `0x98830000` |
| `stb r4,0x7ff(r3)` | `0x988307ff` | `0x98830000` |

All three compare EQUAL. The displacement that a fold-thunk pair turns on was
masked away, so the gate passed that check **by construction**. A vacuity is
worse than a missing check because it reads as coverage.

### 1.3 Why they are one change, not two

This is the load-bearing finding of the lane, and it is the reason "just delete
the asymmetric check" would have been wrong. Removing the 1.1 refusal ALONE
would have made the gate ADMIT a pair whose store offsets differ, because 1.2
means the masked words compare equal for ANY displacement. **Fixing the
asymmetry is only safe together with un-masking the literal.** The pair is
tighter after the change than before it.

### 1.4 The prize, priced from charged sites rather than a mismatch count

`?OnSetShuttle@Game@@QAA?AVDataNode@@PAVDataArray@@@Z`, 260 B, `fuzzy`
99.92308. Of its **65 instruction rows exactly ONE is charged**, and it is:

```
arg_type symbol | target ?Enable@Metronome@@QAAX_N@Z
                | base   ?SetActive@Shuttle@@QAAX_N@Z
```

So the row is gated solely by this fold-name charge, and +260 B is the true
size-if-it-crosses. Verified, not inherited.

### 1.5 Evidence the fold is real, established WITHOUT the gate

- Retail `0x826f07b8` is 8 bytes, `988300084e800020`, `symbols.txt` extent 8,
  fan-in 3 (3 branches, 0 address-taken, 0 data pointer words).
- Our compiled `?SetActive@Shuttle@@QAAX_N@Z` is `988300084e800020`, **zero
  relocations** -- byte-identical.
- Our compiled `?Enable@Metronome@@QAAX_N@Z` is ALSO `988300084e800020`, zero
  relocations. Two of OUR OWN COMDATs are byte- and relocation-identical, which
  is exactly the `/OPT:ICF` condition, and retail's survivor body equals both.
- `?SetActive@Shuttle@@QAAX_N@Z` is absent from `target_symbol_map.json`, so
  nothing in the map contradicts the fold (the docstring's first FT1 case).

⚠ A precondition the brief flagged and I re-checked: the worktree's inherited
`Shuttle.obj` was STALE and carried no `SetActive` COMDAT, so the gate would
still have hit refusal #1. It only appears after a build. Any analysis keyed on
our compiled symbols must build first -- the same trap as reflinked pre-renamer
objs.

## 2. The fix

A linked image has no relocation records: which fields WERE patched cannot be
read back, only inferred. Our COFF object states it. **That record is the only
ground truth available, so it now drives both sides**, and a word with no
relocation is compared as a full 32-bit value.

### 2.1 Scope, which I got wrong first and had to measure

My first patch threaded the COFF-record mask through `homonym()` as well. That
flipped `??3BinStream@@SAXPAX@Z <- ??3@YAXPAX@Z` -- **1,180 sites** -- from
ADMIT/FT3 to REFUSE, by destroying its homonym witness.

`homonym()` compares dc3's linked image against retail's linked image. That is
**LINKED vs LINKED**: NEITHER side has relocation records, and the two are
linked at different addresses, so their patched fields legitimately differ.
There, inference by instruction form is the CORRECT instrument, precisely
because applying it to both sides cancels. I had removed a one-sided error in
one place and re-introduced it in another.

⇒ `Retail.canon(va, relocated=None)` keeps form-inference for that path;
the conditional mask applies only where exactly ONE side has records.

### 2.2 FT-EMPTY's rationale corrected (not its verdict)

FT-EMPTY used to mean "no relocation, so the byte comparison is vacuous" --
true only because of defect 1.2. With the mask fixed, a relocation-free body is
compared as full 32-bit literals, i.e. byte identity, which IS the complete
`/OPT:ICF` condition. Leaving the old wording would have written a false
evidence string into `symbol_aliases.json`.

It stays a held-back-by-default tier for a DIFFERENT reason: **low
discriminating power**. A tiny body is shared by many distinct functions -- 133
of our compiled COMDATs are exactly `stb r4,8(r3); blr` -- so byte identity
alone does not say which of them retail's survivor is, and no relocation target
corroborates it. Installing it requires an explicit `--tier FT-EMPTY`.

## 3. Blast radius, measured in both directions

The brief asked me to re-measure EH's "0 of 29" over ALL refusals.

**House worklist** (`wrong-callee-triage-2026-08-12.json`, 36 fold_thunk pairs),
before vs after, same tree, same build:

| | ADMIT | REFUSE | verdict changes |
|---|---|---|---|
| before | 7 / 1507 sites | 29 / 290 sites | -- |
| after  | 7 / 1507 sites | 29 / 290 sites | **0 of 36** |

EH's figure REPRODUCES: 0 of the 29 refusals carry this reason. (The committed
`fold-thunk-alias-gate-2026-08-12.json` records 9/27; that is a stale historical
record, superseded by the W16-CN function-extent fix.)

**Wide probe.** 36 rows is a thin denominator, so I relabelled ALL 1,048 triage
pairs to `fold_thunk_naming` and adjudicated every one, before and after:

| | count |
|---|---|
| pairs | 1,048 |
| flips | **8 (0.76%)** |
| REFUSE -> ADMIT | 8 |
| **ADMIT -> REFUSE** | **0** |
| flips whose BEFORE reason is `relocated fields at different offsets` | **8 of 8** |

⇒ **EH's "0" was correct WITHIN ITS SCOPE and understates the reach.** Across
the full triage population the defect blocks 8 pairs, not 1 -- 7 at FT2, 1 at
FT-EMPTY, mostly STLport `list::insert`/`erase` and `operator new` spellings
whose bodies carry a literal our side never relocates.

**I did NOT install those 8.** They are a measurement of reach; each needs its
own adjudication, and the house worklist they would come from is unchanged.

## 4. Proof the gate can still FAIL

A gate that cannot fail is worthless, and the honest test targets the exact
blind spot the fix opens -- a body that is masked-equal but literally
different. Real COMDATs from our own build, all 8-byte relocation-free
`stb r4,N(r3); blr`, all adjudicated against the same survivor `0x826f07b8`:

| folded spelling | disp | verdict | reason |
|---|---|---|---|
| `?SetActive@Shuttle@@QAAX_N@Z` | +0x8 | **ADMIT** FT-EMPTY | identical: 2 words |
| `?SetShowing@RndDrawable@@QAAX_N@Z` | +0x8 | **ADMIT** FT-EMPTY | identical: 2 words |
| `?SetSuperEasy@MoveParent@@QAAX_N@Z` | +0xc | **REFUSE** | masked words differ at `['0x0']` |
| `?SetPaused@UIPanel@@UAAX_N@Z` | +0x24 | **REFUSE** | masked words differ at `['0x0']` |
| `??0value_compare@?$map@HM...` | +0x0 | **REFUSE** | masked words differ at `['0x0']` |

Both halves matter: the two ADMITs prove the gate is not refuse-everything (a
gate that refuses everything proves nothing), and the three REFUSEs are refused
**by the displacement** -- the check that was vacuous before. Under the pre-fix
`mask_word` all five collapse to the same masked word.

Made permanent as `tools/test_fold_thunk_gate_mask.py`, which guards
anti-vacuity, symmetry and the linked-vs-linked scope, requires the positive
case to ADMIT so it cannot pass vacuously, and requires a +0xc-vs-+8 body to
REFUSE. The 5 pre-existing gate/alias tests were green before the change and
are green after:

```
tools/test_fold_thunk_gate_install.py     PASS
tools/test_fold_gate_function_extent.py   PASS
tools/test_shape_key_reloc.py             PASS
tools/test_icf_alias_survivor_gate.py     PASS
tools/test_icf_alias_withdrawal_guard.py  PASS
```

## 5. The alias, and what it bought

Installed ONLY through `tools/fold_thunk_gate.py --install --tier FT-EMPTY` on
a one-pair worklist: one new group, `0x826f07b8` / `?Enable@Metronome@@QAAX_N@Z`
with folded `?SetActive@Shuttle@@QAAX_N@Z`. 1,657 -> 1,658 groups, +9 lines.

`tools/ab_measure.py --from-dirty`, alias file the ONLY dirty path (the gate fix
and its test were committed first -- `ab_measure` REFUSED the first attempt at
preflight over the untracked test file, correctly):

| | leg A | leg B | delta |
|---|---|---|---|
| `matched_functions` | 43,956 | 43,957 | **+1** |
| `masked_equal` | 23,224 | 23,224 | +0 |
| honest | 20,732 | 20,733 | +1 |
| `matched_code_percent` | 40.242733 | 40.245266 | **+0.002533 pp** |
| `matched_code` bytes | -- | -- | **+260 B** |
| `none` ruler matched | 45,421 | 45,421 | +0 |
| `none` ruler code% | 44.479220 | 44.479220 | +0.000000 |

Both legs settled to zero work, both at a `symbols.txt` split fixed point, the
forced re-split ran on leg B (`renamer_patched=1830`), and `objdiff-cli` was
pinned stable across legs. Unit `default/band3/game/Game` 330 -> 331; 0 units
reached or fell off 100%.

**`ALIAS_SUSPECT` fired**, exactly as the brief said it would for this patch
class: default ruler up +260 B while `none` is flat, on a map-only patch.

⇒ **I adjudicated on RETAIL BYTES, not on that alert, and I say so explicitly.**
The `none` control CANNOT discriminate here -- it ignores relocation names, so
it reads +0 for a sound alias and a fabricated one alike, and that flatness is
the SIGNATURE of the hazard rather than a clearance. The evidence that the fold
is real is §1.5: retail `0x826f07b8` is `988300084e800020`; our `SetActive`
COMDAT is the same 8 bytes with zero relocations; our `Metronome::Enable`
COMDAT is *also* the same 8 bytes with zero relocations (two of our own COMDATs
meeting the `/OPT:ICF` condition); `SetActive` is absent from the map, so
nothing contradicts; and retail `Game::OnSetShuttle` reaches `0x826f07b8` via
`mr r4,r30; lwz r3,0xe0(r31); bl`, which is `mShuttle->SetActive(active)`.

### 5.1 Pre-registration vs measured

| # | predicted | measured | |
|---|---|---|---|
| P1 | pair ADMITs at FT-EMPTY | ADMIT, FT-EMPTY | ✅ |
| P2 | 0 verdict changes on the 36-pair worklist | 0 of 36 | ✅ |
| P3 | 0 ADMIT->REFUSE and **<=2** REFUSE->ADMIT on 1,048 pairs | 0 ADMIT->REFUSE, **8** REFUSE->ADMIT | ❌ **magnitude wrong by 4x** |
| P4 | 5 existing tests PASS | 5 PASS | ✅ |
| P5 | +260 B, +1 matched function | +260 B, +1 | ✅ |

**P3 is recorded as failed, not rounded off.** The direction held (no admitted
pair was withdrawn) but my bound was wrong by 4x, because I had reasoned from
the 36-pair subclass where the answer is 0 and never estimated the rate across
the full triage population. I pre-registered failure mode (b) -- "MANY
REFUSE->ADMIT flips means I loosened the gate broadly, and the lane stops" -- so
the honest question is whether 8 trips it. It does not, and the reason is
specific rather than a judgement call: all 8 of 8 carry the asymmetry reason as
their BEFORE refusal, each then passes a comparison that is STRICTER on the
words previously masked away, and §4 shows the gate still refuses real bodies
that differ only in that displacement. Had any flip been ADMIT->REFUSE, or had
the before-reasons been heterogeneous, the stop condition would have applied.

## 6. What I did NOT do

- **Did not install the 8 pairs the wide probe unblocked** (§3). They are a
  measurement of the defect's reach, not this lane's deliverable. Each is a
  separate question about a separate pair, the house worklist that would
  produce them is unchanged (0 flips of 36), and installing 8 unadjudicated
  alias memberships to bank bytes is exactly the integrity hazard the brief
  forbids -- an unproven alias lifts `name_check` BY CONSTRUCTION.
- **Did not loosen the gate to admit the pair I wanted.** The net change is
  strictly TIGHTER on the previously-masked literal (§1.3, §4): three real
  COMDATs that the pre-fix gate could not distinguish from the survivor are now
  refused by displacement.
- **Did not treat the `none`-ruler control as a clearance.** For an alias
  change `none` reads +0 by construction, and that flatness is the SIGNATURE of
  the fabrication hazard, not a clearance. The adjudication is on retail bytes
  (§1.5): byte identity at `0x826f07b8`, zero relocations on both sides, and
  our own `Metronome::Enable` COMDAT independently identical to our `SetActive`
  COMDAT.
- **Did not hand-edit `scripts/symbol_aliases.json`.** Installed only through
  `tools/fold_thunk_gate.py --install --tier FT-EMPTY`, which appends and never
  re-sorts. A backup was taken at `~/tmp/w16ek_aliases.bak`.
- **Did not promote FT-EMPTY's standing.** Its rationale string was corrected
  because the old one became false (§2.2), but it remains a separate,
  held-back-by-default tier requiring an explicit `--tier`.
- **Did not touch `tools/comdat_fold_gate.py`, `tools/icf_alias_build.py` or
  any other alias producer**, though `comdat_fold_gate.py` masks both sides by
  form for the same historical reason. Whether it carries the same one-sided
  read against OUR COFF objects is UNVERIFIED by this lane and is the obvious
  follow-up.
- **Did not re-run the permuter** (OFF by standing directive).
- **Did not correct `docs/plans/fold-thunk-alias-gate-2026-08-12.json`**, whose
  recorded 9 ADMIT / 27 REFUSE no longer reproduces (the current tree gives
  7/29). It is another lane's dated record; flagged, not rewritten.
