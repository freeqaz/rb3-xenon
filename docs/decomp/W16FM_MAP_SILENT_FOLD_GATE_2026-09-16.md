# W16-FM — the fold gate had no representable input for its own strongest case, and `StartRefresh` is a genuine fold

Base `bd80df5c` (= `origin/main`). Ruler `name_check` (graded), read from `report.json`.
Authoritative measurement: `tools/ab_measure.py --from-dirty`.

Three deliverables: the gate fix (with the design answer for what stage 2 *means*
when the folded name has no retail address), proof by mutation that the fixed gate
can fail, and the adjudication of `?StartRefresh@XboxContentMgr@@UAAXXZ`.

---

## 1. The adjudication: GENUINE ICF FOLD, not a wrong container element type

**The brief's first question was the right one — "is OUR container element type
wrong?" — and the answer is no. Our source is correct.** I adjudicated this on
retail bytes *before* touching the gate, because an alias would have buried a real
bug if the answer had gone the other way.

Retail's call site charges `list<Hmx::Object*>::insert` where we spell
`list<Content*>::insert`. Six independent lines of evidence say those are one body:

### 1.1 `Content` is not an `Hmx::Object`, so the retail spelling is impossible for us

`src/system/os/ContentMgr.h:13` — `class Content` has **no base class**.
`RootContent : public Content` (`:50`). A `list<Hmx::Object*>` therefore *cannot*
hold the `RootContent*` that `StartRefresh` pushes. The unit's own neighbours agree:
`NotifyMounted`/`NotifyFailed` take `Content*` and are **100%-matching rows**. DC3's
independent header spells the member `std::list<Content *> mContents` as well.

⇒ changing our element type to match the retail *name* would be a behavioural
regression adopted purely to satisfy a relocation name.

### 1.2 The map-independent structural anchor (the decisive one)

Retail's body at `0x823d14c0` — the one the map calls `insert<Hmx::Object*>` —
contains at **+0x24** a branch to `0x82520150`, which the map calls
`_M_create_node<Dep<CharPollableSorter>*>`:

```
=== 0x823d14c0  size=100  map: ?insert@?$list@PAVObject@Hmx@@...
   +0x24  branch -> 0x82520150   map: ?_M_create_node@?$list@PAUDep@CharPollableSorter@@...
```

Under **any** consistent typed reading this is impossible: `list<Hmx::Object*>::insert`
does not call `list<Dep<CharPollableSorter>*>::_M_create_node`. Under ICF it is exactly
what you expect — both are fold survivors whose single surviving name was chosen
independently by the linker. **This argument uses no alias, no map identification of
ours, and no claim about our source; it is a property of retail's own bytes.**

### 1.3 The pointer-vs-value natural experiment

Same template, same binary, same compiler; the only variable is whether the element
type is a pointer:

| family | pointer-element | value-element |
|---|---|---|
| `insert@?$list@` | 1 spelling -> **1 address** (`0x823d14c0`) | 39 spellings -> **39 distinct addresses** |
| `_M_create_node@?$list@` | 1 spelling -> **1 address** (`0x82520150`) | 31 spellings -> **31 distinct addresses** |

Value element types fold **not at all** (39/39, 31/31 distinct); pointer element types
fold **totally**. That is precisely what STLport's code predicts: every `list<T*>` node
is 12 bytes and copied with one word store, so all pointer instantiations compile to
byte-identical bodies, while value types differ in `sizeof` and in the copy.
**A "one address per spelling" model predicts 40 and 32 addresses. The image shows 1.**

### 1.4 Image uniqueness

Both survivors have **0 relocation-normalised rivals** — `0x823d14c0` among **998**
same-size extents, `0x82520150` among **645**. So the linker had exactly one body our
COMDAT could have folded onto; the fold target is not ambiguous.

### 1.5 `icf_pair_adjudicate.py --chase` returns `CHASED T1: PROVEN` for both pairs

and reports `our_bodytwins: 53` vs `retail_bodytwins: 1` for `_M_create_node`.
**53 -> 1 is ICF measured on both sides of the same template.**

### 1.6 W10-B's independent population control, with a negative control

Already recorded in the `0x823d14c0` group's evidence (and preserved — see §4):
235 insert-shaped bodies / 149 distinct `_M_create_node` targets / **exactly one**
pointer-element. Its negative control is the load-bearing half: `list<bool>` allocates
the same `0xc` but copies with a **byte** op and keeps its **own separate**
`_M_create_node` at `0x8274cf60` — so the scan can distinguish "folded" from
"looks similar", which is what makes the positive result mean anything.

### ⚠ 1.7 The MPNGAP-1 signature test is INAPPLICABLE here, not passed

The brief directs: *"does the named callee's signature match the call site?"* On this
row that test **cannot run**, and the reason is itself the evidence.
`insert(iterator, T* const&)` is machine-identical for **every** pointer `T`: the
element type appears in no opcode, no immediate, no offset. **The element type is not
observable in the machine code at all**, so no source change could alter these bytes.

That asymmetry is the finding. MPNGAP-1 killed `Handle@GemPlayer` because retail's
callee *demonstrably disagreed with its call site* (returned `void` where the site
dereferenced `r3`). Here there is no such disagreement available in either direction.
**Where a signature difference would be invisible in code, a relocation-name charge
can only be about naming.** I am recording this as "inapplicable" rather than quietly
counting it as a sixth pass, because reporting an untestable test as passed is how a
confident wrong verdict gets built.

### 1.8 What would have changed my mind

If `Content` had derived from `Hmx::Object`; if the pointer families had occupied
many addresses like the value families do; if either survivor had a relocation-
normalised rival; or if retail's `insert` body had **not** called the differently-
spelled `_M_create_node`. Each was checked and each came out the other way.

---

## 2. The gate fix, and what stage 2 *means* for a map-silent spelling

### 2.1 The defect

`tools/comdat_fold_gate.py` coerced `base_addr` unconditionally:

```python
sa, fa = int(r["target_addr"], 16), int(r["base_addr"], 16)
```

A folded spelling **absent from `target_symbol_map.json` has no address to put
there** — and that is not a corner case, it is the *strongest* fold case: a name can
be absent precisely **because ICF folded it away**. The gate was structurally unable
to adjudicate the population it exists for. Forced to supply a placeholder, the
natural choice `base_addr = survivor` trips the tool's own documented
`same_function(A, A)` vacuum (43.8% of readable bodies), producing a REFUSE that is
an artifact of the fabricated input rather than evidence.

### 2.2 The design answer: stage 2 is a VETO, never a WARRANT

This is the question the brief asked, and it is the whole of the fix.

Stage 2 exists to let **retail's map contradict** a fold: the map places the folded
spelling on a *different* body, so the two cannot be one. When the map is **silent**,
there is nothing to contradict — and silence must contribute **no affirmative
evidence whatsoever**, because "absent from map ⇒ folded" is a **refuted model**.
The map names ~41.7% of functions; a null shows 36.8% of *all* call sites have a
callee absent from the map against 71.6% in the charged stratum — an enrichment of
**~1.95x**, which was previously used as if it were a deterministic classifier.

⇒ **CF5 admits on stage 1 alone**, plus one image-based guard. Map-silence only
removes the veto; it never supplies the warrant. Concretely:

1. **laundering guard** — if `target_symbol_map.json` *does* name the spelling
   anywhere, refuse: that pair has a real `base_addr` and belongs to CF1/CF2/CF3.
   Without this, any pair could be laundered into CF5 by dropping its address.
2. **uniqueness guard** — the survivor body must have **0 relocation-normalised
   rivals**. Stage 1 proves our COMDAT *is* the body at the survivor address; if the
   image held a second body of identical shape, our COMDAT would be equally identical
   to it and *which* body a `bl` denotes would be undetermined.
3. **stage 1 unchanged** — full 32-bit word comparison, branch destinations resolved
   through the map and name-compared, a 16-bit immediate relocation refusing rather
   than being masked.

CF5 is **labelled separately from CF1** and is weaker than it by construction,
because **CF4 was deleted for exactly the mistake of dressing confidence up as
evidence** ("admitted on our confidence, not on retail's bytes"). A tier that cannot
be told apart in the record is a tier that will be re-litigated.

### 2.3 Honest selectivity of the uniqueness guard — it is LOW on the population that matters

Measured over the whole image (hash-once-and-group, not a sampled scan):

```
extents read       78,075     (0 unreadable)
in a rivalled set  11,627  = 14.89% of all extents      surplus copies 9,601
NAMED extents      28,622 ; with >=1 rival: 46 = 0.161%
```

**So the guard fires on 14.89% of everything but only 0.161% of the named
population CF5 can actually be asked about.** The 14.89% is dominated by the
sub-`.pdata` funclet/stub stratum that CD-7 excluded by construction — the same
scope bound CLAUDE.md records. ⇒ **the uniqueness guard is necessary but is not what
makes CF5 safe; stage 1 is.** I am writing the weak number down rather than the
flattering one, because a guard advertised as selective that is not is worse than no
guard.

---

## 3. Proof the fixed gate CAN FAIL (mutation)

`tools/test_comdat_fold_gate_map_silent.py` — 13 checks, 8 unit (synthetic `Retail`)
and 5 integration (real image, real compiled objs). Green at rc=0.

Each mutation **asserts the file actually changed before running**, because the first
attempt targeted variable names that do not exist (`live`/`rv` instead of
`placed`/`riv`) and reported a **silent, vacuous pass** — rc=0, 0 FAILs, mutation
never applied. That is the documented "did the ablation take?" vacuity and it nearly
produced a gate that "survived" mutations never made.

| mutation | rc | checks red |
|---|---|---|
| laundering guard removed (`if placed:` -> `if False:`) | **1** | 3 |
| uniqueness guard removed (`if riv:` -> `if False:`) | **1** | 2 |
| stage 1 removed (`if not ok:` -> `if False:`) | **1** | 2 |
| *(restored)* | **0** | 0 — gate sha256 byte-identical to pristine |

⚠ **Defence in depth, stated honestly:** with the laundering guard removed the
end-to-end laundering case was still refused — by the downstream conflict sweep — but
its tier was mislabelled CF5. The guard is the primary defence and the sweep a
backstop; a test asserting only "was refused" would have passed and hidden the
regression. The check asserts the **tier**, which is why it goes red.

### 3.1 A test fixture whose verdict was an artifact — and the fix

The original stage-1-mismatch fixture used `insert<Content*>`, whose stage 1 failed
*at the time the test was written*. Installing the inner `_M_create_node<Content*>`
fold closed the fixpoint, so its inner `bl` resolved name-equal, stage 1 **passed**,
and the test went red **for a correct gate change**.

Replaced with a **cross-pairing control**: the same spelling that legitimately admits
at `0x82520150` is resubmitted against the **wrong** survivor `0x823d14c0`, where our
64 B COMDAT cannot be the 100 B body. Alias closure only affects relocation-**name**
comparison, so a **size** mismatch can never be rescued by it — the fixture is
state-independent *by construction*. And because the only thing differing between the
two submissions is the survivor, an ADMIT there would prove the gate keys on the
**name** rather than the **body**, which is the precise failure CF5 must not have.

**Lesson worth carrying:** a gate test whose fixture's verdict depends on the alias
file's contents will flip as the fixpoint legitimately advances. Pick fixtures that
fail on a dimension the mechanism under test cannot touch.

---

## 4. Two silent data-destruction defects in `install()` — both found by being bitten

Neither was in the brief. Both would have destroyed evidence quietly.

1. **Folded spellings dropped.** For a group whose evidence `startswith(OWNED)`,
   `g["folded"] = sorted(folded)` **replaces** rather than unions. Installing the
   outer group would have dropped 4 already-proven spellings. Fixed with a
   **fail-closed drop guard** + an explicit `--allow-drop` for a deliberate
   withdrawal. **The guard then fired for real**, catching that `insert<char*>` had
   not re-admitted — which is how the fixpoint incoherence in §5 was found.
2. **Evidence text replaced.** The same path overwrote the `evidence` **string**,
   and on the first run it **erased lane W10-B's whole-binary population control and
   its `list<bool>` negative control** from `0x823d14c0` — the load-bearing evidence
   for the very fold being installed. `hand_annotations()` now preserves every
   segment this tool did not write (other provenances joined with `" | "`, hand
   adjudications appended with `" ++ "`), order-preserved and de-duplicated so a
   re-install neither loses them nor grows without bound. Verified after re-install:
   W10-B note present, `0x8274d128` negative control present, CF5 evidence appended.

**These are the same disease one field apart, and the second was invisible until I
looked for it.** A tool that owns a record must not be able to delete an earlier
lane's adjudication as a side effect of being re-run.

---

## 5. A pre-existing fixpoint incoherence, now closed

Group `0x823d14c0` asserted `insert<char*>` folds, while group `0x82520150` did
**not** assert `_M_create_node<char*>` folds — although the former necessarily calls
the latter. The alias set was not a fixpoint. Closed by admitting
`_M_create_node<char*>` at CF5 on the same evidence.

---

## 6. Regression: strictly additive

Patched gate vs the original over the shipped 1,048-pair worklist, same alias file:

```
rows orig=1048 new=1048   keys-only-orig=0  keys-only-new=0
rows differing on verdict/tier/reason/discredit: 0
orig: ADMIT 15 pairs, 1523 sites
new : ADMIT 15 pairs, 1523 sites
```

`icf_alias_finder.py --validate` output is **byte-identical to base** except
`7137 -> 7140` member spellings looked up — exactly the +3 installed. The 1
CONTRADICTED group (`StandardEffect<CompressionEffect>` @ `0x82b8c8c8`) is
**pre-existing at base `bd80df5c`** and untouched by this lane.

---

## 7. Whole-binary A/B

### 7.1 Pre-registered BEFORE the run

Mechanism-based, not a guess: the three newly-forgiven spellings are referenced by
exactly **three** compiled objects in the whole tree (`insert<Content*>` and
`_M_create_node<Content*>` -> `ContentMgr_Xbox.obj`; `_M_create_node<char*>` ->
`File.obj`), and of those only `StartRefresh`'s two charged `bl` sites sit on a
scored sub-100 row.

| measure | predicted | **measured** |
|---|---|---|
| `Δmatched_code` | +932 B | **+932 B** |
| `Δmatched_functions` | +1 | **+1** |
| `Δmasked_equal` | 0 (=> Δhonest +1) | **+0 (Δhonest +1)** |
| `Δcode%` | +0.009095 pp | **+0.009093 pp** |
| units reaching / falling off 100% | 0 / 0 | **0 / 0** |
| unit attribution | `ContentMgr_Xbox` only; `File` Δ0 | **1 unit, `default/ContentMgr_Xbox` 69->70** |

Falsifiers, all five held:

| # | falsifier | outcome |
|---|---|---|
| F1 | any unit other than `ContentMgr_Xbox` moves | **held** — `unit net (ALL units) = +1`, one unit |
| F2 | `Δmatched_functions` > +1 | **held** — exactly +1 |
| F3 | `control_none_shape()` != `ALIAS_SUSPECT` at `kinds={"map"}` | **held** — fired, `kinds: ['map']` |
| F4 | `none`-ruler `Δmatched_code` != 0 | **held** — `+0 B / +0.000000` |
| F5 | `Δmatched_code` != +932 exactly | **held** |

```
leg A: matched=44028 masked=23245 honest=20783 code%=40.462930  (recompiles: 0, settled)
leg B: matched=44029 masked=23245 honest=20784 code%=40.472023  (settle iterations: 2)
Δmatched=+1  Δmasked_equal=+0  Δhonest=+1  Δcode%=+0.009093pp  Δcode_bytes=+932
unit improvements: +1  default/ContentMgr_Xbox (69->70)
[control none] Δmatched_code=+0 B Δcode%=+0.000000 (default ruler +932 B)
[control none] ALIAS_SUSPECT: ⚠ SHAPE ALERT ... Adjudicate on retail bytes before landing.
```

Leg A reproduced main's `report.json` measures (`matched_code` 4,146,264 /
40.462930%), so the baseline is main's and not a worktree artifact.

### 7.2 ALIAS_SUSPECT fired, and that is the EXPECTED reading — not a clearance and not a conviction

The patch is alias-only, so `kinds == {"map"}` and the guard is *applicable*; the
shape is `name_check` UP / `none` FLAT, which is the fabricated-alias signature. **The
brief predicted this and it is exactly why the `none` control cannot settle anything
here:** `none` ignores relocation names, so it reads `+0` **by construction**. Its
flatness is the *signature of the hazard*, not evidence against it — and the identical
shape is also what a genuine wrong-callee fix produces.

⇒ **The defence of this install is §1, and only §1: six independent retail-byte
lines, of which §1.2 and §1.3 use no alias, no map name of ours, and no claim about
our source.** The tool told me to adjudicate on retail bytes before landing; that
adjudication was done first, and would have blocked the install had it gone the other
way.

⚠ Note also `[leg A none ruler] matched=45469` against the default ruler's `44028`
— Δ+1,441 from the **ruler alone**, on an untouched tree. That is the documented
`vetted_reloc_name_diff` effect (objdiff-core `b14ba45`): **`matched_functions` is no
longer ruler-invariant, so a `none` leg is not a function-count control either.**


---

## 8. Pre-existing issues found and NOT fixed

* **`icf_alias_finder.py --validate` FAILs at base** (rc=1, 1 CONTRADICTED):
  group `0x82b8c8c8` lists `??0XboxContentMgr@@QAA@XZ` as folded, contradicted by
  W16-FI naming `0x825213d0`. Confirmed inherited — reproduced on a tree whose only
  modification was `tools/comdat_fold_gate.py`. **Not fixed**: adjudicating it needs
  a retail-byte census of a different group and is its own lane; fixing it blind
  would be the CF4 mistake again.
* **`scripts/namecheck_triage.py` does not exist.** It is referenced by six files as
  the worklist pipeline's first stage, so the 2026-08-12 worklist is a **frozen,
  non-regenerable artifact**. Anything that needs a *fresh* charged-site population
  cannot currently get one. **Not fixed** — writing it is a lane, and guessing at its
  output format would silently produce a different population.

## 9. What I did NOT do, and why

* **Did not change the container element type.** §1 says our source is right; the
  change would have been a behavioural regression adopted to satisfy a relocation
  name.
* **Did not prune `STALE_SPELLING` (143) or `UNWITNESSED` (11).** House rule — a
  prior prune cost +94,616 B to reverse, and a Δ0 today licenses a change that
  degrades later.
* **Did not touch the sibling lanes' areas** (`src/system/synth_xbox/`, and
  Campaign/MetaMaterial/CheatProvider/VocalTrack/Morph/JoypadMsgs/Character).
* **Did not touch `PollRefresh` (824 B, fuzzy 0.0)** — the largest remaining row in
  the unit. W16-FI already recorded that its DC3 oracle is itself unsolved (95.17);
  it needs its own lane and is not a fold question.
* **Did not widen CF5 beyond map-silent pairs.** It would have been easy to let the
  uniqueness guard admit map-*present* spellings whose map entry looks wrong. That is
  CF4's grave.
* **Did not re-verify §1.5's `--chase` output in this session** — it is quoted from
  this lane's earlier run. §1.2/1.3/1.4 were all re-measured fresh and are
  independently sufficient.
