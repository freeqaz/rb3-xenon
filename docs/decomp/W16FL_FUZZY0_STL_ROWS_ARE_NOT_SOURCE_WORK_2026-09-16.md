# W16-FL — six fuzzy-0 STL rows + `Character::DrawShowing` (2026-09-16)

Base: `main` @ `e164426d`. Worktree `~/tmp/wt-w16-fl`, branch `w16-fl`.
Leg A asserted byte-identical to main on every `report.json` key:
`matched_code 4146264 / matched_functions 44028 / 40.46293% / fuzzy 50.33477 /
total_code 10247068 / total_functions 69240`, ruler `name_check`.

## Result in one line

**+416 B / +1 matched function / +0.004060 pp**, all of it from
`Character::DrawShowing`. The six STL rows (2,016 B) are **NOT source work** and
must not be funded as such.

## 1. The population was verified literally, and it held

All seven briefed rows exist at `fuzzy == 0.0` with exactly the briefed sizes
(2,016 B STL + 416 B = 2,432 B), and the two `auto_03_82743BD0_text` rows
(472 + 224 B) exist and are correctly excluded.

The same scan produced something the brief did not have — **a control
population**. The very same template families are at `fuzzy == 100.0` elsewhere:
`insert_unique@_Rb_tree<Symbol,…>` matches in `DataNode` (x2), `SongSortMgr`
(x2), `FileMergerOrganizer`, `TrainerPanel` (x2), `QuestManager`. So the
machinery is not broken; specific *instantiations* are missing. Without that
control, "STL row at 0" invites a codegen hunt that cannot terminate.

## 2. The mechanical classification (the deliverable)

Read from the COFF symbol table of the compiled object after a **full** build.
Anti-vacuity assertion first: the *target* object defines the retail mangled name
(`DEFINED sec=8`), proving the pre-compile renamer had run — a fresh worktree's
reflinked objects are pre-renamer and would have made every negative vacuous.

| row | size | unit | our .obj defines it? | mechanism |
|---|---:|---|---|---|
| `insert_unique@_Rb_tree<Symbol,…,pair<const Symbol,V1>>` | 472 | Campaign | **ABSENT** | 4 |
| `_M_insert_overflow_aux@vector<MatPropEditAction>` | 396 | MetaMaterial | **ABSENT** | 4 |
| `_M_fill_insert_aux@vector<StepMoves>` | 392 | CheatProvider | **ABSENT** | 4 |
| `_M_insert_overflow_aux@vector<vector<Unlockable*>>` | 336 | VocalTrack | **ABSENT** | 4 |
| `_M_insert_overflow_aux@vector<Unlockable>` | 328 | Morph | **ABSENT** | 4 |
| `_M_insert_overflow_aux@vector<pair<const MoveVariant*,…>>` | 92 | JoypadMsgs | DEFINED+paired | 4 (wrong name, proven on bytes) |
| `?DrawShowing@Character@@UAAXXZ` | 416 | Character | DEFINED+paired | **real source work — FIXED** |

The split is corroborated independently by `mpn`: all five ABSENT rows are
exactly `0.0`, both DEFINED rows are non-zero (2.55, 3.48).

Mechanisms 1/2/3 from the brief were all **ruled out**, not assumed:
- **not 2 (defined in another unit)** — scanned all **1,220** compiled objects;
  none defines any of the five names.
- **not 3 (anon-ns hash)** — `Unlockable@?A0x` returns **zero** symbols under
  *any* hash anywhere in the tree, as do `MatPropEditAction` and `StepMoves`.
  A differing hash would look identical to "never instantiated", so this had to
  be tested separately.
- **not 1 (declare the member and it appears)** — see below.

## 3. Mechanism 4: names transferred from DC3's leaked map

All four missing types are **Dance Central 3** types with **zero** occurrences in
the rb3-Wii RB3 oracle: `StepMoves` (DC3 `lazer/meta_ham/PracticeChoosePanel.h`),
`MatPropEditAction` (DC3 `rndobj/MetaMaterial.h`), `Unlockable` (DC3
`lazer/meta_ham/MetagameRank.cpp`, `std::vector<Unlockable> gUnlockables` **in an
anonymous namespace** — the origin of `?A0xf8e4b4b5`), `MoveVariant` (DC3 hamobj).

Proven, not inferred: the exact Morph-row mangled name appears **verbatim** in
DC3's leaked `orig/373307D9/ham_xbox_r.map` at `0005:0058ae20`, and
`?A0xf8e4b4b5` appears there **62** times.

The direct proof is `JoypadMsgs`. Retail's body at `0x825bd1f8` is **23
instructions**: load a global pointer, **one** virtual call via vtable+0x60,
store `0` to `this+4` and `1` to `this+0`, return. A `vector<T>` growth helper
must allocate, copy and free (our 396 B base does exactly that). The name is
simply wrong, and no source can fix it.

## 4. Why they can never pair: template COMDATs are scattered far from their TU

For the other four the callee names are *internally consistent* with the claimed
family, so the bodies really are STL growth code. Two of them can be
**re-identified from retail bytes**:

- **CheatProvider** `0x826ffe38` calls `??0SfxMap@@QAA@ABV0@@Z`. A
  `_M_fill_insert_aux` copy-constructs its element type ⇒ the true element type
  is **`SfxMap`** (a real Milo type present in all three repos,
  `src/system/synth/Sfx.h`), not DC3's `StepMoves`.
- **Campaign** `0x822ea818` calls `_M_insert@_Rb_tree<Symbol,…,pair<const
  Symbol,**CatData**>>` — inconsistent with its own claimed value type `Symbol`.

★ **And our `Sfx.obj` ALREADY DEFINES
`?_M_fill_insert_aux@?$vector@VSfxMap@@…` — the correctly-named counterpart.**
But `Sfx.cpp`'s other code sits near `0x82237520` while this row is at
`0x826ffe38`, **~4.8 MB away**.

⇒ **The linker places a template COMDAT far from its owning TU's main
contribution, so a `.text`-span pin attributes it to whichever unit's span
happens to contain it.** That single fact explains the whole population,
including the otherwise-impossible scatter of one anonymous namespace's
`vector<>` helpers across **8 pinned units** spanning ~9.5 MB
(Gem, Character, PracticeSection, MoveMgr, Morph, Tracker, Sequence, VocalTrack)
— `_Copy_Construct@Unlockable` (`0x82346e50`, PracticeSection span) is *called
by* `_M_insert_overflow_aux@Unlockable` (`0x823495e0`, Morph span): one coherent
instantiation cut in half by a pin boundary.

⇒ **This vein is identification / pin work, not source work.** Making these rows
pair by writing DC3's `StepMoves` / `Unlockable` / `MatPropEditAction` into RB3
source would fabricate game content that does not exist in Rock Band 3 — textbook
metric fitting, and wrong code. **Do not fund it.**

## 5. `Character::DrawShowing` — the one real fix (+416 B, measured)

Retail's body (`.fn fn_8236ECD0`, read **by symbol** — the `.s` address column is
synthetic) is **104 instructions** and returns immediately after `EndShadow()`.
Ours was ~249. Removed:

1. the `TheLoadMgr.EditMode() -> mTest->Draw()` block;
2. the entire `character.show_name` name-drawing block;
3. the DC3-only `GetGfxMode() == kNewGfx` test — `GetGfxMode()` is out-of-line
   (`System.cpp: return gGfxMode;`) so it must emit a `bl`, and retail's clause
   contains exactly one call, the virtual `TheNgRnd.Offscreen()` via vtable+0x104.
   Spelled as DC3's `&&` chain minus that test, which is the shape producing
   retail's select-into-`r11` sequence.

Corroborated **independently of both oracles** by a binary string probe of
`orig/45410914/default.xex`: `character.show_name`, `char_draw`, `char_poll`,
`list_interest_objects`, `debug_draw_interest_objects` are all **ABSENT** —
matching the already-stripped debug handlers. (`bone_head` is present, but it is
used by other char code, so it neither confirms nor refutes; said here so the
probe is not read as cleaner than it is.)

The `PROLOGUE_MISMATCH` (r24-r31 vs r27-r31) and `REGISTER_SWAP` (33 instrs)
patterns objdiff reported **dissolved on their own** — they were symptoms of the
oversized body, not independent defects. Same lesson as the standing note that a
`REGISTER_SWAP` label is a symptom, not a diagnosis.

### Pre-registration vs outcome

Predicted **+416 B / +1 fn / +0.004060 pp**; measured by `ab_measure`
(`--from-dirty`, graded ruler) **+416 B / +1 fn / +0.004060 pp**. Exact on all
three. Only `default/Character` moved (209->210), and unit-net over **all** units
equals the whole-binary delta, so nothing regressed elsewhere. None of the four
falsifiers (F1 net<predicted, F2 fuzzy-moves-but-pays-0, F3 no movement,
F4 excess) fired. Units at 100% unchanged on both rulers (191, 171).

## 6. What I did NOT do, and why

- **Did not re-home `0x826ffe38` to `Sfx.cpp`**, despite our `Sfx.obj` already
  defining the right symbol (~392 B in play). It needs splitting CheatProvider's
  span and adding a distant second `.text` block to `Sfx.cpp`, which forces a
  full re-split A/B; CLAUDE.md records that re-homing is **not** metric-neutral.
  That is a pin lane's job and my remaining budget had to cover the mandatory
  native gate. **This is the best-evidenced hand-off from this lane.**
- **Did not re-identify** the Morph / VocalTrack / MetaMaterial / Campaign
  element types beyond the two callee-derived leads above.
- **Did not touch** the two `auto_*` rows (unpairable by construction).
- **Did not run the permuter** (standing directive: off).
