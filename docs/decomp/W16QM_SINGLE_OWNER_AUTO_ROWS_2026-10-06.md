# W16-QM: single-owner attribution of the remaining in-scope `auto_*` rows (2026-10-06)

Lane W16-QM. Brief: generalise W16-QI (`W16QI_STATIC_DTOR_REGION_2026-10-06.md`) to every
remaining in-scope `auto_*` row (Quazal and XDK skipped). Attribute a row when every retail
reference to it comes from one pinned unit and that unit's own obj defines a body matching
retail, pin it there with its name, and measure the method's precision on rows whose owner
is already known before trusting it. Worktree `~/tmp/wt-w16qm`, branch `w16-qm`, based on
main `2d3582fa3`.

## 1. Result

**14 rows pinned and named, +12 functions / +952 B.** The method reaches very little of the
`auto_*` population. §4 shows why: almost all of it has no reference from a pinned unit at all.

| row | B | unit | tier (§3) | after |
|---|---:|---|---|---|
| `0x822750A8` `PatchDir::NumLoadingStickers` | 20 | PatchDir | T3 | 100 |
| `0x822D6690` `ObjPtr<RndPartLauncher>::ObjPtr` | 112 | StreakMeter | T1+T3 | 100 |
| `0x822D6A58` `ObjectDir::Find<RndPartLauncher>` | 164 | StreakMeter | T1+T3 | 100 |
| `0x822D6BE8` `ObjPtr<BandLabel>::Load` | 232 | StreakMeter | T2+T3 | 100 |
| `0x822D6EA0` `ObjPtr<RndMatAnim>::Load` | 232 | StreakMeter | T2+T3 | 100 |
| `0x82459FC8` `RndText::OnSetFont` | 100 | Text | T1+T3m | 100 |
| `0x824F6EA8` `RockCentral::CancelOutstandingCalls` | 12 | RockCentral | T3 | 100 |
| `0x824F6EB8` `RockCentral::FailAllOutstandingCalls` | 12 | RockCentral | T3 | 100 |
| `0x8258A190` `BandProfile::GetUploadFriendsToken` | 8 | band3/meta_band/BandProfile | T3 | 100 |
| `0x827A4FB0` `Profile::MakeDirty` | 12 | Profile | T3 | 100 |
| `0x82BD4EB8` `RemoteBandMachine::GetMachineID` | 8 | BandMachine | T3 | 100 |
| `0x82C295A0` `TubePlate::SetShowing` | 40 | NoteTube | T3 | 100 |
| `0x822EBCB0` `GemTrackDir::Handle` | 2,236 | GemTrackDir | T2 | 95.74 (ours 2,200 B) |
| `0x82429C38` `RndPropAnim::ForeachKeyframe` | 2,256 | PropAnim | T2 | 93.27 (ours 2,192 B) |

The last two pair for the first time and are partial matches. They are now visible to body
work, and before this lane they were not.

**Measured** with `tools/ab_measure.py --revert HEAD` (leg A has the pins, leg B removes
them; `name_check` ruler; both legs at a split fixed point, 0 recompiles), run
`20261006-134553-w16qm-pins-3708720`:

| | leg B (without) | leg A (with) | Δ |
|---|---:|---:|---:|
| `matched_functions` | 54,581 | 54,593 | **+12** |
| `matched_code` | 5,982,780 | 5,983,732 | **+952 B** |
| `matched_code_percent` | 58.381165 | 58.390450 | +0.009285 pp |
| `fuzzy_match_percent` | 64.039400 | 64.090430 | +0.051030 pp |
| `masked_equal_functions` | 25,162 | 25,162 | 0 |

`total_functions` and `total_code` do not move (68,909 / 10,247,792): the lane reattributes
rows, it adds and removes none. Units at 100% do not change (575 `mpn`, 509 all-rows-fuzzy).

**Prediction**, stated before the build that produced the committed set: +13 / +988 B if
`0x82BCD0E0` paired, +12 / +952 B if it did not. It did not (§5), and the A/B reproduced
+12 / +952 B exactly.

**Rows that went down: none.** That is a row diff of the two archived leg reports over every
`(unit, symbol)` present on both sides. Of the 72 rows that vanish on one side and appear on
the other, 58 are the same symbol at the same score in an `auto_*` unit renamed because a pin
split it, and 14 are the rows above. Matched by address, each goes from 0 in its old
`auto_*` unit to the score shown.

**A first attempt did push rows down: 872 of them, −35,016 B** (§3.5). That attempt was not
committed.

The native gate was not run: the lane touches only `splits.txt` and
`target_symbol_map.json`, no source.

## 2. The control, and what it measured

The method was run, unchanged, on every row whose owner is already known: rows pinned in a
named, in-scope unit with a map name, Quazal band (`0x82A6D168`–`0x82B54190`) excluded. Its
`(unit, name)` was then compared with the existing `(unit, name)`. The numbers are for the
final rule as committed (all tiers plus the coverage rule of §3.5), computed by the same code
path that produced the attribution (`final.py control` / `final.py auto`):

| tier | attributed | name wrong |
|---|---:|---:|
| T1 (the brief's rule), alone or with T3 | 10,896 | 1 |
| T2 alone | 401 | 6 (1.5%) |
| T2 with T3 | 6 | 0 |
| T3 / T3m alone | 4,381 | 0 |
| **all** | **15,684** | **7 (0.045%)** |

All seven errors are STL template siblings with byte-identical bodies:
`list<Target>::_M_splice_insert_dispatch` against `list<RndDrawable*>`,
`vector<Key<Weight>>::_M_fill_insert_aux` against `vector<IKTarget>`,
`_List_base<Symbol>::clear` against `_List_base<DecompressTask>`, and similar. That is the
shape an ICF fold takes, so some of them may be the map's choice of spelling rather than
the method's error; they are counted as errors anyway. Neither T2 row in §1 has this shape.

The unit differed from the existing pin on 394 rows (2.51%):

- **329:** the name is defined in both objs, the pinned one and the referencing one (a
  shared COMDAT or inline). References cannot decide the unit, and the row pairs in either.
- **58:** the method picked an alias-equivalent spelling (`scripts/symbol_aliases.json`)
  defined in the referencer's obj, while the pinned obj defines the survivor spelling.
- **5:** the wrong-name rows above. **2:** other.

An earlier reading of the 58 as "existing pins whose obj lacks the name" was wrong. 45 of
them read 100 where they are pinned. They are spelling differences across a fold, not
mis-pins.

Two instrument defects were caught by the control before anything was pinned:

- **The single-referencer rule alone is wrong for ordinary external functions.** Without
  the body requirement, the referencing unit disagreed with the pinned unit on **4,038 of
  16,369** known rows that had a single referencing unit: a function defined in A and called only from B is normal. The body
  requirement is what makes the rule work. With it, the first T1-only control read
  11,710 / 7 wrong.
- **PPC `PAIR` relocations (type `0x12`) carry a displacement in the symbol-index field.**
  The first COFF reader kept the last relocation at each offset, so every `lis`/`addi`
  reference read as `@comp.id` (symbol 0). The caller witness then rejected **1,842**
  correct control rows as "callers contradict". After skipping `PAIR` that fell to 264.
  W16-QI's `coff.py` has the same overwrite. Its byte check masked only the opcode, so the
  defect did not affect its result.

## 3. Method

Retail references were indexed from the dtk `.s` files, keyed on `.fn fn_<addr>` and
`.obj` names, never on the address column (CLAUDE.md: synthetic for multi-block units). A
row's references are its code references from other functions (`bl`/`b`/`@ha`/`@l`) plus
exact data references (`.4byte fn_X`). `.pdata` entries and `fn_X+off` (the row's own EH
IP-to-state entries) are excluded.

### 3.1 T1, the brief's rule

The row has only code references, all from functions in one named, in-scope unit. That
unit's base obj defines exactly one function with:

- equal size;
- equal bytes except relocated fields, masked by opcode (26-bit for `b`, 14-bit for `bc`,
  16-bit otherwise);
- branch callees whose retail map name equals ours or is in its alias group (an anonymous
  retail callee is accepted).

Candidate names already mapped to another address are excluded. Witness: the referencing
function, looked up by name in the same obj, must reference the candidate. When several
byte-identical candidates remain, the one the referencer references is taken if it is
unique. Bodies of 4 B or less are refused, because a `blr` carries no identity.

### 3.2 T2: the brief's owner, name from the aligned call site

The owner is chosen as in T1. If our copy of a referencing function has the same size as
retail's, the relocation at the instruction that references the row in retail names our
callee. That name must be defined in the owner's obj and free. The body is **not** required
to be byte-equal, which is how the two partial matches in §1 were found.

This goes past the brief's "defines a body matching retail". It is a separate tier with its
own measured precision (1.5% errors, all template siblings).

### 3.3 T3: call-site name, owner = the obj that defines it byte-equal

T3 uses the call-site name from any aligned named referencer, in any unit. The owner is the
in-scope unit whose obj defines that name with a byte-equal body (§3.1 comparison). With
several defining objs (**T3m**), the owner is the one defining unit that is also a
referencing unit.

This was added because 135 of the single-unit rows had our aligned caller naming a callee
that the caller's own obj does not define: an external function, so the referencing unit is
not its owner.

### 3.4 Combining

A row takes a decision only if every tier that fires agrees on unit and name. A row that
already carries a different map name is refused, unless the two names are alias-equivalent.
The process iterates until no new row is added, since a newly named row can witness its
callees. It converged in one round.

### 3.5 Coverage rule: every reference must resolve to the name (added after a failure)

The first application attributed 19 rows. The same-worktree build read
**+16 functions / −35,016 B, 872 rows down**. All 872 down rows call one of the 19:
`0x82B69618`, named `vector<QuickJoyCheat*>::~vector`. Retail references that address
**903 times from 245 units**: it is the ICF survivor for pointer-vector destructors. One
Cheats caller was aligned and named it, and the other 899 references were EH funclets,
whose call targets had been forgiven as placeholders and were now charged.

The control could not see this, because it measures whether the identification is right,
and it was right. What was unsafe was naming a folded address.

The fix, applied to every tier and to the control alike: **every** code referencer of the
row must be a named function whose aligned copy in our objs names this callee. Four rows
fail it: `0x82B69618` (899 refs, 2 aligned), `0x822D6730` (7/3), `0x826F0F18` `~RGTutor`
(3/1) and `0x8292F980` zlib `gen_bitlen` (2/1). The control went from 17,028 / 9 wrong to
15,684 / 7 wrong.

### 3.6 Pins

Each block runs `[row, next function)`. It starts at the 8-byte EH prefix when the row has
one (337 existing `.text` blocks start at a prefix). It ends before the next function's
prefix and never past the next existing block. One row's padding was already pinned to
Accomplishment as a 4-byte block (`0x822750BC`), and the PatchDir block stops there. dtk
back-filled the `.pdata` lines.

## 4. Why the rest of `auto_*` is out of reach

In-scope `auto_*` rows (from the `.s` index: 4,153 rows / 904,252 B, with the Quazal band
excluded):

| class | rows | B |
|---|---:|---:|
| referenced only from `auto_*` code | 1,568 | 440,300 |
| has a data reference (`.rdata`/`.data`) | 1,209 | 177,688 |
| referenced from more than one unit, and no tier fires | 653 | 134,236 |
| referenced only from XDK / Quazal units | 335 | 93,156 |
| one named in-scope referencing unit, no tier fires | 180 | 37,524 |
| no reference at all | 189 | 15,132 |
| coverage rule refuses (§3.5) | 4 | 736 |
| attributed (§1, plus `0x82BCD0E0`, §5) | 15 | 5,480 |

A further 3,555 rows / 532,604 B lie in the Quazal `/Od` band and were not examined. The
report counts 7,922 `auto_*` rows (1,438,700 B) against the index's 7,708. The difference,
214 rows / 1,844 B, is report rows with no `.fn` symbol in the `.s` files.

- **Single-unit rows without a tier (180).** Mostly CRT/XDK leaf code reached through a
  game-unit wrapper (`XSessionCreate`, `XSessionDelete`, …, `wcsncpy`, `_open`,
  `vsprintf`), code our objs inline where retail calls it, and code with no definition in
  any of our objs. 3,681 `auto_*` rows (823 KB, Quazal band excluded) have **no aligned
  named caller at all**, so T2/T3 cannot reach them.
- **Data references.** `.rdata` and `.data` are each one unpinned `auto_*` unit, so a
  vtable or table entry is never "a reference from a pinned unit". Resolving a table
  through the code that references it (`vftable_X@l`) was sized: it adds **4 rows**, none
  with a byte-equal body. It was not built further.
- **Chains through `auto_*` code.** Iterating does not open them. The referencers are
  themselves unattributable, and the fixed point is reached in one round.

## 5. Not done

- **`0x82BCD0E0`.** T3 named it `BeatMatchController::RegisterHit`. The map already holds
  `CX2SourceVoice::OnPacketLoopEnd` there, and `symbol_aliases.json` records the fold
  (survivor `OnPacketLoopEnd`, folded `RegisterHit`). Pinned in BeatMatchController with
  the survivor name it read **0.0**: objdiff does not pair a survivor name with an obj that
  defines only the folded spelling. The pin was removed and the map left as it was.
- **The four rows refused by the coverage rule (§3.5).** Unpinned and unnamed. In
  particular `0x82B69618` must not be named after any one of its 245 callers' types.
- **No `symbols.txt` edits, no source edits, no alias additions.**
- **The 7 control errors (§2).** They are existing map names that disagree with our
  callers' spelling across a likely fold. Not adjudicated.
- No merge, no push (coordinator).

Scratch (not committed): `~/tmp/w16qm/`. `index.py` builds the reference index;
`engine.py` holds the tiers, the COFF reader and the byte comparison; `final.py` runs
`control` / `auto`; `apply.py` writes splits and the map; `rowcheck.py` diffs rows.
Also there: `control*.py` / `probe_*.py` (the intermediate controls), `assigned.json`,
`final_control_disagree.json`, `ab1.log` and `ab1_rows.txt`. The A/B run directory is
`~/tmp/wt-w16qm/.ab_measure_runs/20261006-134553-w16qm-pins-3708720/`.
