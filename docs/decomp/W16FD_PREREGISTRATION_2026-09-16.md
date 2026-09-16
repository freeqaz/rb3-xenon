# W16-FD pre-registration (written BEFORE the final measuring build)

Baseline, measured by me at main `d8aa682f` in this worktree after a
settle-to-zero-work build (ruler `functionRelocDiffs=name_check`, objdiff 4.2.9,
read from `build/45410914/report.json`):

    matched_functions       43995
    matched_code            4137976
    matched_code_percent    40.38205
    masked_equal_functions  23228
    total_functions         69240
    total_code              10247068

## Predicted final measures

    matched_functions       43997          (+2)
    matched_code            4138284        (+308)
    matched_code_percent    40.385056
    masked_equal_functions  23228          (unchanged)

+308 B is exactly 128 + 180, the two row sizes; `matched_code` is all-or-nothing
per row, so any other total falsifies the claim that exactly two rows crossed.

## Rows predicted to CROSS (fuzzy -> 100.0, mpn -> 100.0)

1. `?IsFinished@MakeQuazalSessionJob@@UAA_NXZ`   128 B, was 65.65625
2. `?AssignLocalOwner@NetSession@@QAAXXZ`        180 B, was 37.37778

## Required NEGATIVES (falsifiers — if any of these fails, my reasoning is wrong)

N1. `?OnMsg@NetSession@@QAA_NABVAddUserRequestMsg@@@Z` (664 B) MUST remain at
    EXACTLY 99.93976 and MUST NOT cross.  I adjudicated its two charges and
    abandoned it: one (`push_back`) is a T1-provable ICF fold, the other
    (`vector` copy ctor) chains to a 4-byte `blr` thunk that the anti-vacuity
    guard refuses by construction.  I made NO edit to `scripts/symbol_aliases.json`.
    If this row moves at all, something other than my source work touched the
    relocation-name stratum and the abandon reasoning must be re-opened.

N2. `?GetNoteSliceWeight@VocalPart@@QBAMMMH@Z` (484 B) MUST return to EXACTLY
    94.92562 / mpn 96.661156.  Both source variants I tried regressed it
    (f22 constant-fold -> 82.983475; oracle `frameMs` hoist -> 88.12397) and
    both were reverted.  Any other value means the revert was incomplete.

N3. `?OnMsg@NetSession@@QAA_NABVJoinResponseMsg@@@Z` — the sibling overload
    W16-EW already took to 100 — MUST still be 100.  AssignLocalOwner is in the
    same TU; if my body port perturbed the unit, this is where it shows.

N4. No row in `default/network/net/NetSession`, `default/network/net/QuazalSession`
    or `default/VocalPart` may DECREASE in fuzzy.  The QuazalSession change adds
    a pragma and an out-of-line ctor body to a shared TU.

## Note on the brief's "row that improves without reaching 100"

In the FINAL state no retained change leaves a row partially improved — both
retained fixes cross outright, and the VocalPart experiments were reverted.  The
requirement was satisfied in the intermediate state instead, and is recorded in
commit 35bae693: `auto_inline(off)` ALONE moved IsFinished 65.65625 -> 86.43750,
i.e. it improved without crossing, which is what proved the second (nothrow/EH)
defect existed and was distinct from the first (inlining) defect.  I am stating
this rather than inventing a partial row to satisfy the form of the rule.

---

# RESULT (measured after the final settle-to-zero-work build)

| measure | baseline | predicted | measured | verdict |
|---|---|---|---|---|
| matched_functions | 43995 | 43997 | **43997** | hit |
| matched_code | 4137976 | 4138284 | **4138284** | hit |
| matched_code_percent | 40.38205 | 40.385056 | **40.385056** | hit |
| masked_equal_functions | 23228 | 23228 | **23228** | hit |

Rows crossed: exactly the two named (IsFinished 65.65625 -> 100, AssignLocalOwner
37.377777 -> 100).  N1/N2/N3 all held exactly.  N4: over 394 rows in the three
touched units, **0 decreased, 0 appeared, 0 vanished, 2 increased**.

⚠ My own checker printed a false MISS on `matched_code` because `report.json`
stores it as a JSON **string** — `4138284 == "4138284"` is False while the printed
values are identical.  That is the `int()`-coercion trap CLAUDE.md documents,
reproducing live on the lane that had just read the warning.  Coerce every
numeric out of report.json.

---

# ABANDONED ROW 1 — `?OnMsg@NetSession@@QAA_NABVAddUserRequestMsg@@@Z` (664 B, 99.93976)

**It is NOT a source defect.**  Our `std::vector<UserGuid>` is semantically right
and compiles to bytes identical to retail's.  The row carries exactly TWO charges,
both `diff_arg` relocation-NAME charges (the brief said "one charge from
crossing" — that is wrong, and it matters, because `matched_code` is
all-or-nothing):

| charge | target spelling | our spelling |
|---|---|---|
| push_back | `vector<GemInProgress>::push_back` | `vector<UserGuid>::push_back` |
| copy ctor | `vector<Vector3>::vector(const vector&)` | `vector<UserGuid>::vector(const vector&)` |

`Vector3` is **16 bytes** (x,y,z + 4 PAD, per `class_layout_report.py`) and
`UserGuid : HxGuid` is `int mData[4]` = **16 bytes**, so these are same-size
element twins and the fold hypothesis is live, not idle speculation.

Adjudicated with the project's OWN T1 comparator (`tools/icf_alias_build.py`:
`collect` + `canon_relocs` + `relocs_agree` + `vacuous`), through the shipped
equivalence closure (6583 entries), with a NEGATIVE CONTROL that refuted
(`vector<LocalUser*>::push_back`, a 4-byte element: 112 B vs 136 B, bodies
differ) — so the instrument can fail:

* **push_back charge: PROVEN.**  2-link chain.
  `_M_insert_overflow_aux<vector<Vector3>>` == `<vector<UserGuid>>` (412 B,
  masked bodies identical, relocs agree, non-vacuous), which then makes
  `push_back<GemInProgress>` == `<UserGuid>` (136 B) prove.
* **copy-ctor charge: UNPROVABLE BY CONSTRUCTION.**  Three links:
  1. `__uninitialized_copy<Burst*>` == `<const UserGuid*>` — ALREADY in the
     shipped `symbol_aliases.json` (independent corroboration of the 16-byte
     fold family).
  2. `_Vector_base<Hmx::Color>` == `_Vector_base<UserGuid>` — **provable**
     (96 B, bodies identical, relocs agree, non-vacuous).
  3. retail `StlNodeAlloc<_List_node<int>>::ctor<int>` vs our
     `vector<UserGuid>::get_allocator` — **both 4-byte bodies**, i.e. a bare
     `blr`.  `vacuous()` refuses (MIN_WORDS=4 words=16 B).  This is correct, not
     a tooling gap: *every* `blr` in the binary is byte-identical to every other,
     so byte-identity carries zero information here.

⇒ Since `matched_code` keys on `fuzzy == 100`, closing only the provable charge
buys **ZERO bytes**.  I therefore did NOT edit `scripts/symbol_aliases.json`:
adding forgiveness that pays nothing on this row, in the class the project flags
as an integrity hazard (an alias lifts the score BY CONSTRUCTION, and the `none`
control cannot detect a fabricated one), is all risk and no yield.

**WHAT WOULD HAVE TO BE TRUE for this row to be reachable:** link 3 must be
adjudicated by **ADDRESS, not by BYTES** — e.g. establishing that retail's
`??$?0H@?$StlNodeAlloc@V?$_List_node@H@stlpmtx_std@@@stlpmtx_std@@QAA@ABV?$StlNodeAlloc@H@1@@Z`
and our `vector<UserGuid>::get_allocator` resolve to the SAME retail address.
A relocation-normalized body hash can never settle it.  If that is established,
all three copy-ctor links plus the push_back chain are in hand and the row is
worth 664 B.

Secondary finding for whoever owns the alias generator: `tools/icf_alias_build.py`
already has group #36 (survivor `push_back<vector<GemInProgress>>` @ 0x82788308,
folded: `push_back<vector<Vector3>>`, evidence T1) but `vector<UserGuid>` is
absent from it, even though it is T1-provable by the generator's own comparator.
The enumeration gate (census-observed sites only) is why — a T1-provable pair the
ruler is actively charging never got proposed.

---

# ABANDONED ROW 2 — `?GetNoteSliceWeight@VocalPart@@QBAMMMH@Z` (484 B, 94.92562)

39 charged sites.  The diagnosis is ONE cause, not 39: we use one MORE
callee-saved FP register than retail (`__savefpr_20` = f20..f31 = 12 regs, vs
retail's `__savefpr_21` = 11), and that single extra register renumbers f20..f28
across the entire function.  In the setup block we materialise THREE constants
(`4/7`, `2.0f`, `0.5f`) where retail materialises two, and retail instead
*reloads* a pooled constant late (idx 101, `lfs f0, lbl_820F3A40@l(r30)`).

Two principled source levers were tried and **both regressed** (each measured on
a full `tools/ninja-locked` build, not a single-obj build):

| variant | fuzzy | delta |
|---|---|---|
| baseline (current source) | **94.92562** | — |
| `f22 = 1.0f - (4.0f/7.0f)*(4.0f/7.0f)` (constant-fold, kill the 4/7 hoist) | 82.983475 | **-11.94** |
| oracle form: hoist `float frameMs = kFrameTimeMs;`, `min(spC, frameMs)` | 88.12397 | **-6.80** |

Both reverted.  Two things this rules out for the next lane:

1. **The `4/7` hoist is NOT spurious.**  Folding it away makes the row *worse*,
   so retail materialises that constant too; the extra FP register does not come
   from the `f22` expression form.
2. **The rb3-Wii oracle is WORSE than our current source here.**  Our source
   deviates from the oracle at exactly one line and the deviation is correct —
   a prior lane tuned it deliberately.  Do not "restore the oracle".

A hypothesis I refuted before spending a build: I suspected MSVC global
co-addressing (`lbl_820F3A40` / `lbl_820F3A44` are 4 bytes apart, one base
register).  It is dead twice over — `lbl_820F3A40` is referenced from
AmbientOcclusion, AnimFilter and Archive, i.e. a linker-folded *shared* constant,
not VocalPart's `kFrameTimeMs`; and our `kFrameTimeMs` is ALREADY
`static const float` (internal), so there is no extern-vs-static lever.

**WHAT WOULD HAVE TO BE TRUE:** the row needs our FP register pressure reduced by
exactly one live value WITHOUT removing the `4/7` materialisation — i.e. a
scheduling/liveness change, which is the permuter's job.  Permuter is OFF by
standing user directive (deferred until after the ceiling), so this row is
correctly parked, not drained.  Note also that all 39 charges must close for any
bytes at all; a partial collapse of the renumbering is worth 0 B.
