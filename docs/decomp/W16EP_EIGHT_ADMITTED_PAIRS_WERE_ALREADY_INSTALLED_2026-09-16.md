# W16-EP — the 8 pairs W16-EK's fix unblocked: 6 were already installed, 2 are refused

Lane W16-EP, 2026-09-16, worktree `~/tmp/wt-w16-ep`, branch `w16-ep`, off `2d366ad2`.
Ruler: shipped graded `name_check` (read from `build/45410914/report.json`, not assumed).

## Result in one line

**Nothing installed. Measured delta 0, and no A/B was run because the tree is
byte-identical to HEAD** — the two pairs that clear the evidence bar were
**already in `scripts/symbol_aliases.json`**, and the only two that are *not*
installed are the two the evidence refuses. The lane's real finding is that
**W16-EK's "8 blocked pairs" were overwhelmingly memberships the file already
shipped**: the defect was blocking the gate's ability to *re-derive* its own
aliases, not blocking new value.

## 0. Pre-registration (written before any gate was run)

| # | prediction | measured | |
|---|---|---|---|
| P1 | flip count = **8**, 0 ADMIT->REFUSE | 8 flips, 0 ADMIT->REFUSE, 8/8 carrying the asymmetry as before-reason | ✅ |
| P2 | tier split **7 FT2 + 1 FT-EMPTY** | 7 FT2 + 1 FT-EMPTY | ✅ |
| P3 | **I install 0 of 8** (allowing <=1) | installed 0 — but for a reason I did not predict | ⚠ **right answer, wrong reason** |
| P4 | **0 of 8 have differing sizes**; `compare()` refuses on `body length N vs M` before admission | 0 of 8; word counts equal by construction | ✅ |

**P3 is recorded as a near-miss, not a hit.** I predicted 0 installs because I
expected the 8 to be template twins refuted on their relocation targets (the
W16-EJ shape). That reasoning was **wrong**: the relocation targets are
NAME-EQUAL on both sides in every pair that has one, and two pairs (2 and 4)
do clear the bar on retail bytes. I would have installed them. They were
already installed, which is why the count is 0. Predicting the right number
from the wrong model is a miss.

## 1. Re-derivation — the count reproduces on this tree, not on EK's word

Method, deliberately EK's so the numbers are comparable: relabel **all 1,048**
pairs of `docs/plans/wrong-callee-triage-2026-08-12.json` to
`fold_thunk_naming`, run the pre-EK gate (`git show 97f76e42^:tools/fold_thunk_gate.py`)
and the current gate over the same worklist, same build, and diff the verdicts.

| | ADMIT | REFUSE |
|---|---|---|
| pre-EK gate | 7 / 1,507 sites | 1,041 |
| current gate | **15** / 1,523 sites | 1,033 |

⇒ **8 flips, all REFUSE->ADMIT, 0 ADMIT->REFUSE, and 8 of 8 carry
`relocated fields at different offsets` as their before-reason.** EK's figure
reproduces exactly.

**Build first, then trust a negative.** The reflinked worktree was built to a
fixed point before any of this (`verify_objs_patched.py --check`: *"tree is a
fixed point of 6 post-compile passes"*), and the EK trap was checked explicitly:
680,543 COMDATs indexed, with `?SetActive@Shuttle@@QAAX_N@Z` and
`?Enable@Metronome@@QAAX_N@Z` both present — the exact symbols that were
silently missing from EK's stale tree.

## 2. The 8 pairs

`our_bodytwins` / `retail_bodytwins` and the family pigeonhole come from
`tools/icf_pair_adjudicate.py --pairs ... --family`, whose `--selftest` was run
first and **discriminates** (positive control PROVEN, negative REFUTED).
`retail copies` is my own whole-image census (§3).

| # | sites | survivor <- folded | T1 | our_bodytwins | retail copies (incl. branch targets) | verdict |
|---|---:|---|---|---:|---:|---|
| 1 | 8 | `??2CriticalSection` <- `??2Task` | UNDECIDABLE (vacuous) | 214 | **2** | **REFUSE** |
| 2 | 2 | `list<AccomplishmentCondition>::insert` <- `list<Plane>::insert` | PROVEN | 137 | 1 | clears bar — **already installed** |
| 3 | 1 | `_Vector_base<RndTransformable*>::ctor` <- `GetSongSpecificEntriesForCategory` | PROVEN | **1031** | **2** | **REFUSE** + source defect |
| 4 | 1 | `_Param_Construct<EyeDesc>` <- `_Copy_Construct<EyeDesc>` | PROVEN | 271 | 1 | clears bar — **already installed** |
| 5 | 1 | `list<Voice*>::erase` <- `list<const char*>::erase` | PROVEN | 41 | **11** | **REFUSE (refuted)** |
| 6 | 1 | `list<CharClip*>::insert` <- `list<Dep*>::insert` | UNDECIDABLE | 137 | n/a | **UNDECIDABLE** |
| 7 | 1 | `list<MsgSource::Sink>::erase` <- `list<pair<Symbol,Symbol>>::erase` | PROVEN | 11 | **11** | **REFUSE (refuted)** |
| 8 | 1 | `pair<Symbol,SongRecord>::dtor` <- `pair<const Symbol,SongRecord>::dtor` | UNDECIDABLE (vacuous) | 75 | 1 | **REFUSE** |

**No pair has differing sizes on the two compared sides** — structurally
impossible among ADMITs, since `compare()` refuses on
`body length N words vs M` before any admission. The brief's different-size
defect class therefore cannot appear here; it would have to be hunted in the
*refusals*, which is a different lane.

### 2.1 FT2's discredit is sound, and here is the retail-byte reason

All 8 share one shape: the map parks the folded spelling on an 8-byte,
zero-reference body. That is not a coincidence and not a gate artifact —
**7 of 8 of those addresses are 8-byte EH prefixes**, and `addr+8` is a real
function start (`7d8802a6` = `mflr r12`) with its own map name and extent:
`?Poll@MoviePanel@@` (660 B), `??0SongSortBySong@@` (120 B),
`?RefreshSetlists@MusicLibraryNetSetlists@@` (132 B),
`??1MatSwap@OutfitConfig@@` (120 B). The 8th is a bare `blr` + padding. So the
map is parking fold losers on the EH prefix of an unrelated function, exactly
as the gate's docstring describes. **That clears a contradiction; it proves no
fold.**

## 3. The decisive instrument: a WHOLE-IMAGE body census, not a pinned one

`retail_bodytwins` counts only **pinned** target objs, and W16-EL already
measured what that hides: retail keeps byte-identical bodies (including their
`bl` target) **unfolded at distinct addresses**. So I counted retail's copies
over every `symbols.txt` extent in the image: mask by instruction form
(linked-vs-linked, symmetric — the gate's own doctrine), then additionally
require the resolved branch-destination set to be identical.

**Pairs 5 and 7 are REFUTED on retail bytes.** Retail keeps **eleven**
byte-identical 84-byte `list<T*>::erase` bodies — identical *including* branch
targets, the complete `/OPT:ICF` condition — at eleven distinct addresses:
`822b1e60`, `823d9570`, `82447458`, `8247ceb8`, `824e0350`, `824e03a8`,
`8252c2d8`, `82766828`, … Retail did **not** fold them.

⇒ The family pigeonhole that read *"41 of ours -> 1 retail address"* was an
**artifact of pinning coverage**, and it pointed the wrong way. This is W16-EL's
finding reproduced at scale, and it is the reason a pigeonhole over pinned objs
must never be the basis for an alias.

Pairs 1 and 3 likewise show **2** unfolded same-branch-target copies each.

## 4. Why pairs 2 and 4 clear the bar — a CALL-SITE argument, not a byte argument

"The bytes match" is not an argument; 1031 of our symbols share pair 3's body.
The bar is: why *this* spelling and not a body twin?

**Pair 2.** Retail's callers of the survivor are
`??0list<Plane>::list(const list<Plane>&)`, `?resize@list<Plane>@`, and
`?Update@BSPFace@@` — functions whose own mangled names are `list<Plane>`. A
`list<Plane>::resize` cannot legitimately call
`list<AccomplishmentCondition>::insert`; the only explanation is that the two
inserts folded and the map kept one arbitrary name. Corroborating: retail's
survivor relocates to `_M_create_node<list<Plane>>` — the **Plane** helper,
NAME-EQUAL with ours — retail defines no separate `F`, and the whole-image
census finds exactly **one** body with that branch-target set.

**Pair 4.** Both spellings are STL helpers for the **same T**. Retail's callers
are `__uninitialized_copy<EyeDesc>`, `__uninitialized_fill_n<EyeDesc>` and
`vector<EyeDesc>::_M_insert_overflow_aux`; ours is that same last function, and
the single relocation names `??0EyeDesc@CharEyes@@QAA@ABU01@@Z` on **both**
sides. A body twin for another `T` would call that other `T`'s constructor.

★ Both arguments survive the W16-EJ test that killed 20 of 23 `--chase` pairs:
the discriminator (the relocation target) **agrees by name**, rather than being
tolerated, masked, or recursed around.

## 5. The finding that makes the lane's output zero

`tools/fold_thunk_gate.py --install --tier FT2` on a 2-pair worklist:

```
ADMIT 2 pairs / 3 sites in 2 groups; REFUSE 0 pairs / 0 sites
--tier FT2: installing 2 of 2 admitted pair(s) / 3 of 3 sites
installed: 0 new group(s), 0 updated; 1658 total
```

`git diff --stat scripts/symbol_aliases.json` — **empty**. Checked against the
file directly:

| # | survivor group exists | folded spelling already present |
|---|---|---|
| 1 | yes | **no** |
| 2,3,4,5,7,8 | yes | **yes** |
| 6 | **no** | — |

⇒ **6 of 8 are already installed**, including both pairs that clear the bar.
The two that are NOT installed are pair 1 and pair 6 — the two the evidence
refuses. There is nothing left to install.

⇒ **W16-EK's "8 blocked pairs" measured the defect's reach correctly and its
VALUE not at all.** The unblocked admissions are, six times out of eight, the
gate finally agreeing with memberships already shipped. That is worth having —
*"a generator you cannot re-run is a number you cannot re-derive"* — but it is a
**reproducibility** result, not a byte result, and it should not be briefed as
uncollected headroom.

## 6. The worklist's charged sites are STALE — do not price from them

Priced the candidate rows with `objdiff-cli diff` (no `--build`; the tree was
already built and patched) and **none of the three rows is charged by the pair I
adjudicated**:

| row | size | fuzzy | what is actually charged |
|---|---:|---:|---|
| `?Update@BSPFace@@QAAXXZ` | 612 B | 97.79085 | 36 charges, **all** register/FPR/stack-slot regalloc + 1 `delete`. **Zero** fold-name charges |
| `_M_insert_overflow_aux<EyeDesc>` | 328 B | 99.87805 | 2 charges, both `__uninitialized_copy<PBU EyeDesc>` (const) vs ours `<PAU EyeDesc>` — a **different** pair |
| `?resize@list<Plane>@` | 144 B | 99.86111 | 1 charge, retail `__uninitialized_copy<TimeSigChange>` vs our `list<Plane>::erase` — a **different** pair |

The worklist is dated **2026-08-12**; these aliases were installed after it. So
its `sites` counts describe charges that no longer exist. **Any lane pricing a
candidate off that file's `sites` column is pricing a stale number** — price
from `report.json`'s charged-site list on the current tree, per the standing
rule.

## 7. A real source defect, reported rather than aliased away

Pair 3's folded spelling is **our own stub** (`src/band3/meta_band/ContextChecker.cpp:262`):

```cpp
std::vector<const char *> GetSongSpecificEntriesForCategory(Symbol, bool) {
    return std::vector<const char *>();
}
```

Unnamed parameters, returns an empty vector, compiles to 20 bytes — which is
why it is byte-identical to a `_Vector_base` ctor. Its caller
`GetSongSpecificEntries` uses the result (`entries.size()`, `FOREACH`), so the
empty return is a behavioural gap, not a harmless placeholder.

⚠ **I am NOT claiming an alias was built to hide it.** The group at
`0x826b8b28` (installed by `1b26376b`) is a **blanket COMDAT-identity group with
1030 folded members** — every symbol sharing that trivial 20-byte body — and our
stub's membership is incidental. The honest statement is: **the stub is a real
defect on its own merits, and it is currently invisible to the metric.** Fixing
it is a `ContextChecker` source task, not an alias task.

## 8. What I did NOT do, and why

- **Did not install anything.** Two pairs clear the bar and were already
  installed; the two not installed are refused on evidence. Installing pair 1 to
  bank its 8 sites would be exactly the fabrication hazard the brief forbids: an
  8-byte body whose single relocation is the **shared** `?MemAlloc@@YAPAXHH@Z`
  (so the discriminator does no work), shared by **214** of our own symbols, with
  retail keeping **2** unfolded copies. EK's install was a relocation-**free**
  leaf with a map-silent spelling; pair 1 has neither property.
- **Did not run `ab_measure`.** The tree is byte-identical to HEAD
  (`git status --porcelain` empty), so there is no change to price. Running an
  A/B on an unmodified tree measures settling noise, not a change, and reporting
  it as a delta would be fabrication. **The delta is 0 because nothing was
  installed** — stated, not measured, and the two are not the same thing.
- **Did not withdraw pair 3's membership.** I cannot: the gate has no
  `--withdraw` path and hand-editing `scripts/symbol_aliases.json` is forbidden.
  It is also a 1030-member blanket group, so withdrawing one member is a
  separate, priceable decision, not a by-product of this lane.
- **Did not treat the `none` ruler as a control.** No alias change was made, so
  there was nothing for it to be blind to — but the standing point holds: for a
  map-only patch `none` reads +0 by construction, and that flatness is the
  signature of the hazard, never a clearance.
- **Did not re-litigate `tools/comdat_fold_gate.py`** (it imports `mask_word`;
  the coordinator audited the defaulted-parameter compatibility). I changed no
  shared helper, so no importer blast radius exists to grep.
- **Did not re-run the permuter** (OFF by standing directive).

## 9. Guard state

All six pre-existing gate/alias tests PASS on this tree, unchanged:

```
test_fold_thunk_gate_mask.py            PASS
test_fold_thunk_gate_install.py         PASS
test_fold_gate_function_extent.py       PASS
test_shape_key_reloc.py                 PASS
test_icf_alias_survivor_gate.py         PASS
test_icf_alias_withdrawal_guard.py      PASS
```

`scripts/symbol_aliases.json` is unchanged at **1,658 groups**; a backup was
taken at `~/tmp/w16ep/symbol_aliases.bak.json` before the install attempt.

## 10. For the next lane

1. **The 84-byte `list<T*>::erase` family is an 11-copy unfolded population.**
   Any alias proposed against it is refuted before it is written. The same
   whole-image census should be run before *any* future fold claim — it is cheap
   and it inverted two verdicts here.
2. **`docs/plans/wrong-callee-triage-2026-08-12.json` is stale in its `sites`
   column** (§6). It is still a fine *enumeration* of pairs; it is not a pricing
   instrument.
3. **`GetSongSpecificEntriesForCategory` is unimplemented** (§7).
