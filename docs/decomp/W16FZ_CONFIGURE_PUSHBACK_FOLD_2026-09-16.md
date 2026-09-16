# W16-FZ — installing the `push_back<Symbol>` ICF fold: +3 functions, +2,672 B, and Δhonest really is +3

Date: 2026-09-16 · branch `w16-fz` · worktree off main `51d4c06d`
Alias: survivor `?push_back@?$vector@VSymbol@@…@Z` @ `0x822d16f8`
       folded  `?push_back@?$vector@W4ControllerType@@…@Z`
Patch class: **map-only** (`scripts/symbol_aliases.json`, 11 insertions / 0 deletions).

## Headline — every pre-registered number hit

| measure | leg A | leg B | Δ | predicted |
|---|---:|---:|---:|---:|
| `matched_functions` | 44,131 | 44,134 | **+3** | +3 ✓ |
| `matched_code` | 4,163,348 | 4,166,020 | **+2,672 B** | +2,672 ✓ |
| `masked_equal_functions` | 23,322 | 23,322 | +0 | +0 ✓ |
| **honest** (`matched − masked_equal`) | 20,809 | 20,812 | **+3** | +3 ✓ |
| `matched_code_percent` | 40.629654 | 40.655727 | +0.026073 pp | 40.655727 ✓ |
| `fuzzy_match_percent` | 50.487220 | 50.487232 | +0.000012 pp | +0.000006 pp (miss +6e−6) |

Rows, all `masked_equal = FALSE`:

| row | size | fuzzy | mpn |
|---|---:|---|---|
| `?Configure@Accomplishment@@QAAXPAVDataArray@@@Z` | 2,584 B | 99.99226 → **100.0** | 99.99226 → **100.0** |
| `?AddValidController@OvershellSlot@@QAAXW4ControllerType@@@Z` | 44 B | 99.545456 → **100.0** | → **100.0** |
| `?AddAutoVocalsValidController@OvershellSlot@@…@Z` | 44 B | 99.545456 → **100.0** | → **100.0** |

Unit improvements `+2 default/OvershellSlot` (406→408), `+1 default/Accomplishment` (71→72);
unit net over ALL units `+3` == whole-binary Δmatched, i.e. **no row anywhere regressed**.
Leg A reproduced this lane's own pre-alias baseline to the last digit.

★ **Δhonest = +3, and that is the point.** Almost everything landed this week moved
`matched` and `masked_equal` together, netting Δhonest 0 — W16-FY's own +22 was
22 EH funclets at 32 B each. Here all three crossing rows are real bodies that
pair by NAME, `masked_equal=False` on every one, so the honest floor moves.

## Two corrections to the brief, both measured

**(1) The prize was three rows, not one.** The brief priced `Configure` alone
(+1 fn / +2,584 B). Censusing OUR objs for relocations naming the folded spelling
found **three** callers. The two `OvershellSlot` rows have **exactly one
relocation slot each, and it IS this pair** (retail `push_back<Symbol>` vs our
`push_back<ControllerType>` at +0x18, 44 B on both sides) — so they were each one
charge away from 100 and nobody had noticed. Price the **block**, not the row.

**(2) The equivalence counter moves +2, not +1.** The brief required
`Loaded N ICF equivalence entries` to grow by one. Measured: **6,102 → 6,104**.
`SymbolEquivalences::len()` counts *names appearing in a multi-symbol group*
(objdiff-core `map_file.rs`), and this is a **NEW** group contributing **two
previously-absent names**. +1 is the correct reading for adding a membership to
an **existing** group — lane FU's case, which is where the brief's figure came
from. The model was validated before predicting: it reproduces the standing
6,102 exactly from the current map.

## What I re-derived vs inherited

**Re-derived in this lane, in my own built worktree:**
* The full `--chase` adjudication (FLAT T1 REFUTED, CHASED T1 PROVEN) — reproduces FY.
* `--chasetest`: all four controls discriminate, **including the in-family decoy**
  (`fn_827B0E78`, same masked body, Symbol-keyed callees → correctly REFUTED).
* Survivor map-residency at `0x822d16f8`, and that our spelling is at **no** address.
* That `Configure` has **196** differing-name relocation slots of which exactly
  **one** is non-placeholder on both sides — the other 195 are `lbl_*`, which
  `name_check` already forgives. That is what 99.99226 is made of.

**Inherited from FY and NOT independently re-derived:** the claim that retail's
`Configure` calls `0x822d16f8` from all three loops (I verified the *charged*
slot at +0xac; I did not disassemble all three loop bodies). Nothing in this
lane's conclusion rests on it — the OvershellSlot witness below is stronger.

**New evidence FY did not carry, both independent of the recursion:**
1. **`--family` pigeonhole.** SEVEN of our `push_back<T>` spellings (Symbol,
   ControllerType, CubeFace, SkeletonJoint, TrackInstrument, TrackType,
   JointAngle) collapse onto **ONE** retail address. The discriminator is not a
   blur: retail keeps a **second** masked-body twin at `0x82b5f808`
   (`push_back<ChatReceiver*>`, itself a separate landed group) and the slot-0
   test **excludes** it. It separates the two retail survivors instead of merging them.
2. **The caller-signature witness.** `OvershellSlot::AddValidController(ControllerType)`
   and `AddAutoVocalsValidController(ControllerType)` — functions whose own
   *mangled signature* takes a `ControllerType` — call the address the map names
   `push_back<Symbol>`. Both cannot be the true template argument, so that map
   name is the **fold-arbitrary survivor spelling**. Same argument shape as
   W16-FU's "decisive corroboration from the map itself", from a different TU
   than Accomplishment.

## The `retail_bodytwins: 2` reading that looks like a uniqueness violation

`tools/incomplete_group_install.py` enforces a **retail-uniqueness** gate: a fold
class is usable only if retail kept ONE address for it. This pair reports
`retail_bodytwins: 2`, which will look like a violation to the next auditor. It
is not, for two reasons, and both should be checked before anyone re-opens it:

* That gate is documented **EXACT for `nrel == 0`** and deliberately
  **CONSERVATIVE for `nrel > 0`** — masked-equal twins that would *not* fold
  because their relocation targets differ are counted as ambiguity. This pair is
  `nrel > 0`, and the two retail twins differ exactly there (slot-0 callee).
* **More decisively, the ambiguity never arises, because the address was not
  searched for — it was READ OFF retail's own relocation.** All three of our
  charged call sites sit opposite a retail `bl` whose relocation *names the
  survivor*. There is no candidate-selection step, so there is no coin flip to
  lose. This is `--chase`'s stated safety property.

## The integrity problem, stated honestly

`ab_measure` fired, exactly as pre-registered:

```
[control none] ALIAS_SUSPECT: default ruler UP (+2672 B) while `none` is FLAT
on a map-only patch — the FABRICATED-ALIAS shape.
```

**That is not a finding against this lane, and its absence would not have cleared
it either.** An alias lifts `name_check` *by construction*, and a fabricated one
produces a bit-identical shape (`none` matched 45,569 / code% 44.788540 on BOTH
legs). The metric and the `none` control are **structurally incapable** of
adjudicating this edit. Neither is offered as evidence. The clearance is the
retail-byte adjudication above.

## Liveness instruments (both required to fire; both did)

| instrument | leg A | leg B |
|---|---|---|
| `icf_alias_finder.py --validate` | rc=0, 1,658 groups, 0 CONTRADICTED | rc=0, **1,659**, **0 CONTRADICTED** |
| `OK (MAP-CONSISTENT)` | 1,407 | **1,408** |
| TOLERATED classes | 34 / 88 / 27 / 101 | **unchanged** 34 / 88 / 27 / 101 |
| `GEN ICF-ALIAS MAP` | 1,607 groups, 6,918 symbol lines | **1,608**, **6,920** |
| `Loaded … ICF equivalence entries` | 6,102 | **6,104** |

The counter reading appears in **`legB_report.log`** — the run that produced the
score — not merely in the gate, which is what makes it the sharper instrument:
it proves the edit reached the **scoring** path.

**Nothing pruned.** The diff is 11 insertions / 0 deletions; `STALE_SPELLING` (88)
and `UNWITNESSED` (101) are untouched. Both forgive 0 bytes today and become live
as porting advances; a prior prune cost **+94,616 B** to reverse.

Gate tests, before and after: `test_icf_alias_{join_guard,no_ourbuild_gate,survivor_gate,withdrawal_guard}`
and `test_alias_group_key` all rc=0. `icf_alias_finder --selftest` PASS (it proves
`UNRENAMED_TARGET_OBJS` **fires**, which is the trap below).

## Traps hit and caught in this lane

1. **The pre-renamer worktree trap, avoided by construction.** A reflinked
   worktree carries the target objs but not the effect of the pre-compile
   renamer, so every retail mangled name reads *absent* and the adjudicator
   returns `UNDECIDABLE: survivor absent` — **a vacuity that would have agreed
   with a "refuted" prior**. I built the worktree fully before adjudicating
   anything; `--selftest` independently confirms that detector can fire.
2. **A membership test that could not fire.** My first map parser keyed the
   address as the *second* field when it is the *last*. It parsed 0 records, so
   `SURV in names` returned a clean, decisive-looking `False` off an **empty
   dict**. Caught only because `distinct names = 0` was implausible. Re-run with
   a control (a name known to be present must return `True`).
3. **FY's case-sensitivity trap, reproduced live.** `target_symbol_map.json`
   keys are lowercase: `m["0x822D16F8"]` → `None`, `m["0x822d16f8"]` → the
   survivor. A case-sensitive lookup is a screen that cannot fire.
4. **`| tail; echo $?`** — not walked into; every rc in this lane was read from
   the command itself or from a redirect tested on the next line.

## Deliberately not done

* **Did not add the other five family spellings** (CubeFace, SkeletonJoint,
  TrackInstrument, TrackType, JointAngle). They are in the same pigeonhole, but
  each is a separate assertion needing its own adjudication, and none is charged
  today — our objs reference the folded spelling from **only** the three
  functions above. Adding them would buy 0 bytes and widen the integrity surface.
* **Did not touch any other group**, withdrawn or live; no prune, no repair.
* **Did not disassemble** all three `Configure` loops (see "inherited", above).
* **Did not fix** the other 17 unmatched rows in `default/Accomplishment`
  (unit now 72/89) or the 406→408 remainder in `default/OvershellSlot`.

## Verdict

An honest gain: +3 real-bodied functions, +2,672 B, Δhonest **+3**, zero
regressions, every liveness instrument moved in the predicted direction and by
the predicted amount, and the two corrections above are both to the brief rather
than to the adjudication.

---

## ⚠ COORDINATOR NOTE (appended at landing, 2026-09-16) — two things the landing measured that the lane could not

Landed as `160cebb7`. **Every pre-registered key hit miss 0** (44,134 / 4,166,020
/ masked_equal 23,322 / honest 20,812 / code% 40.655727 / fuzzy 50.487232), and
the run was surgical: **exactly 3 rows moved in the whole binary**, the 3
predicted, 0 fell out. Both liveness instruments fired: map 6,918 → **6,920**
symbol lines, objdiff `Loaded 6,102` → **`6,104`** equivalence entries — the +2
model confirmed against the +1 that a membership-into-existing-group would give.

Two things surfaced at landing that are worth carrying forward.

### 1. ⛔ My pre-registered falsifier **F5 FAILED**, and the gate was wrong, not the run

F5 required the build to show *"the renamer patched >0 files"*. It read
**`0 files patched`**. Reporting this rather than booking past it, because a
failed falsifier that gets quietly dropped is how a gate stops meaning anything.

**The clause was mis-specified for this patch class.** Checked at landing:

| | |
|---|---|
| `obj_target_symbol_renamer` consumes | `scripts/target_symbol_map.json` — **untouched by this branch (0 hits)** |
| the alias map reaches the ruler via | `objdiff.json` → `map_file: build/45410914/icf_aliases.map`, **read by objdiff directly** |

⇒ **An alias-only edit never passes through the renamer at all.** `0 files
patched` is the *correct* reading here; a non-zero one would have meant something
unrelated moved. The `>0` clause belongs to a **`target_symbol_map.json`** map
edit, and I imported it from a template built for that class without checking
whether the pipeline applied. (The stamps *were* removed — all six
`*_patched.stamp` files carry the build's own timestamp — so this is not a
forced-re-split failure.)

The instruments that actually cover an alias-only patch all fired, and one of
them is decisive because it was read **from the build that produced the score**:
the `Loaded 6,104` line. Plus `verify_objs_patched.py --verify-manifest` **rc=0**,
which asserts every decomp and target object is a verified content fixed point —
the independent check that "renamer did nothing" left nothing stale.

★ **The durable rule: a liveness gate must be keyed to the pipeline the patch
actually travels.** A gate copied from an adjacent patch class can fail while the
run is perfect — and, worse, could pass while a genuinely inert edit sails
through. Lane CF-1 lost an entire A/B leg to the second version of this.

### 2. ★ The validator classified this group as **TOLERATED, not MAP-CONSISTENT**

```
before (main 3fff0fb2): 1,408 map-consistent · 249 tolerated · 0 contradicted · 1,658 total
after  (main 160cebb7): 1,408 map-consistent · 250 tolerated · 0 contradicted · 1,659 total
```

**`map-consistent` did not move.** The new group landed in `tolerated` — i.e. the
validator explicitly does **not** vouch for it.

This is the right outcome and it is worth stating loudly, because a green
`VALIDATE: PASS` is easy to relay as "the fold is verified". It is not.
`OK (MAP-CONSISTENT)` was already renamed from `OK (grounded)` precisely because
it means *map-consistency*, not proof — and this group does not even reach that
bucket. **The only thing standing behind this alias is the retail-byte
adjudication** (`--chase` with `--chasetest` controls discriminating including
the in-family decoy; the `--family` pigeonhole; and retail's
`AddValidController(ControllerType)` calling the address the map names
`push_back<Symbol>`, which cannot both be true of the same template argument).

⇒ **For any future alias lane: `VALIDATE: PASS` with your group in `tolerated` is
the EXPECTED reading, not a clearance.** Do not let a passing validator, a rising
`name_check`, or a flat `none` control be presented as evidence — all three are
produced identically by a fabricated alias. Only retail bytes separate them.
