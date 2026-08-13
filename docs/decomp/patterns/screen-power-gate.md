# Verify a screen fires on a known positive before trusting its negative

**Lane TOOL-SCREEN, 2026-08-13.** Tool: `tools/screen_gate.py`.

A **screen** is any detector a lane writes to find defect candidates — in retail
bytes, in our `.obj`s, in the map, or in the source. On 2026-08-13, **six of them
in a single session produced clean, decisive-looking output that was wrong** —
and in every case the wrong output was a **NEGATIVE**, which is the verdict class
that closes veins and cancels work.

That asymmetry is the whole problem. A screen that returns a *false positive*
costs one lane an hour of triage and then gets caught, because somebody reads the
retail body and it does not say what the screen claimed. A screen that returns a
*false negative* is never caught at all: it looks exactly like a clean
population, it is shaped like a decisive result, and the correct response to a
decisive result is to stop working the vein. **Nobody audits an empty list.**

## The six defects

| # | defect | what it printed |
|---|---|---|
| 1 | regex anchored at `^subi` while every dtk `.s` line carries a `/* ADDR OFF BYTES */\t` prefix | `0/21,349` **and** `0/6,680` — fired nowhere, **including on two rows already confirmed by hand** |
| 2 | `lwz` decoder with the **rD and rA fields swapped** | **0 hits across 14 MB**; corrected, the same scan fires **1,972** times |
| 3 | handler parser missing one macro variant (`HANDLE_EXPR_STATIC`) | two classes parsed as **zero** handlers ⇒ all **102** and all **18** of retail's arms reported MISSING |
| 4 | never-compiled `#if defined(MILO_DEBUG) && defined(HX_NATIVE)` arms not stripped | **invented** a handler (`debug_toggle_autoscroll`) that does not exist |
| 5 | thunk decoder tracking only `r3`, missing the `r4` struct-return form | manufactured **false disagreements** |
| 6 | splits.txt parser written for lowercase hex, run against an **uppercase** file | read as a decisive *"none of these addresses are pinned"* |

This is not a new disease, it is a recurring one. Two instances are already
recorded in `CLAUDE.md`: the **`grep` shim that is binary-blind** (a shell
*function* routing through `ugrep -I`, so a scan of a binary yields only false
negatives — it has already cost real yield), and the **first `/GS` cookie
detector**, which scored **0 hits on a known-`/GS` object** because it assumed
`lis`/`lwz` adjacency and the compiler schedules two instructions between them.
That one was caught *only just* before it could publish a false "retail has no
`/GS`".

## The rule

> ### VERIFY A SCREEN FIRES ON A KNOWN POSITIVE BEFORE TRUSTING ITS NEGATIVE.

A screen that **cannot** fire is indistinguishable from a clean population. If
you cannot name an input your screen *must* flag, and demonstrate that it does,
you have not measured the population — you have measured nothing, and you cannot
tell the two apart from the output.

## Three assertion classes, and why one is not enough

`tools/screen_gate.py` makes a screen assert its own power before its results are
trusted. It registers three kinds of evidence, and the count is **empirical, not
aesthetic**:

1. **`must_fire` — known positives ⇒ POWER.** Inputs the screen *must* flag. This
   catches defects **#1, #2, #3, #5, #6**.
2. **`must_not_fire` — known negatives ⇒ SPECIFICITY.** Inputs it must stay
   silent on. This catches defect **#4**, and **nothing else does** — #4 is the
   one defect that fires too *much*. A harness with positive controls only would
   have shipped it. When the gate's own `--self-break` runs, #4 is the sole
   `INDISCRIMINATE` verdict among six; every other injected defect lands as
   `POWERLESS`.
3. **Enrichment vs an untreated population ⇒ DISCRIMINATION.** A screen can fire
   correctly on both fixtures and still carry no information, because it fires at
   the same rate on rows that are *fine*.

Class 3 is the one lanes skip, and it is the one `CLAUDE.md`'s control-group
discipline is most explicit about: a raw signal "fires on any large function
under register pressure, i.e. **it confirms whatever you point it at**". The `/Od`
detector only became trustworthy once the untreated-population control was run
(≈**413×** enrichment, exactly one false positive).

**Report enrichment, never a raw count.** `tools/callsite_screen.py` is the
worked example and is committed as a **drained vein** for exactly this reason:

```
CONTROL  named non-virtual mpn==100 : 959/10959 unreachable = 8.7508%
TREATED  named non-virtual sub-100  : 314/ 4695 unreachable = 6.6880%
ENRICHMENT: 0.76x
```

The suspect rows are **less** flagged than the correctly-paired ones. An
enrichment at or below 1.0 is a **drained vein, not a candidate list** — and a
raw count of "314 candidates!" would have read as a rich seam.

## Phase 0 — the gate must itself be capable of failing

The obvious failure mode of a harness like this is that it becomes the bug it
polices. This repo has shipped guards that could not fail: a **single-candidate
gate CANNOT FAIL**, and a fixture-free selftest that skipped silently is not a
test. So before running any fixture, `screen_gate` refuses a gate that is
structurally incapable of catching anything:

- **no `must_fire` fixture registered** ⇒ `VACUOUS-GATE`. It cannot detect a
  screen that cannot fire.
- **positive and negative payloads identical** ⇒ `VACUOUS-GATE`. It cannot
  discriminate.
- **no `must_not_fire` fixture** ⇒ a `WARN`: power is proved, specificity is not.

Two further properties are held deliberately:

- **It reads the returned value, never a screen's claim about itself.** No stdout
  parsing, no exit-code trust. A lane this session sabotaged a `restore()` to
  merely *claim* it had worked, and the tool printed *"verified by re-reading the
  diff"* over a mutated tree — **a test that trusted that printed line would have
  passed a completely broken restore.**
- **It never skips.** Every fixture is an inline literal lifted verbatim from a
  real artifact, so the gate runs with no build tree, no `orig/45410914/band.exe`
  and no `report.json`. A guard that skips when its data is missing is a guard
  that cannot fail — which is why `tools/grep_binary_guard.py` builds its own
  binary fixture. A screen whose *module* cannot be imported is reported
  `UNTESTABLE` and exits non-zero: **an untestable screen is not a passing
  screen.**

## Fixtures must be real artifact excerpts

An invented fixture proves the screen matches your idea of the input, which is
precisely the belief that was wrong in defects #1 and #6. Every fixture in
`screen_gate.py` was lifted verbatim:

- **`.s` lines with the prefix intact** — `/* 82657630 0064C430  3B EC FF 80 */\tsubi r31, r12, 0x80`. There are **13,866** such lines, so defect #1's `0/21,349` was not a clean population; it was a scan that could not see its own input.
- **The uppercase pin** — `.text start:0x8277B52C end:0x8277B6E8`. `config/45410914/splits.txt` is uppercase throughout, so a `[0-9a-f]+` regex reads the file as containing no pins at all.
- **Real retail instruction words** — `0x8163FFFC` = `lwz r11, -4(r3)` at `0x82275560`, and `0x8164FFFC` = `lwz r11, -4(r4)` at `0x82273F28`. See the next section: these two words are defects #2 and #5.
- **Real handler blocks** — `src/system/meta/CreditsPanel.cpp` (whose true retail set is exactly `[pause_panel, is_cheat_on]`) and `src/band3/meta_band/ClosetMgr.cpp` (all `*_STATIC` arms).

`--provenance` re-checks that each literal still occurs in the live artifact, so
a fixture cannot silently rot away from the thing it was copied out of.

## The `this`-register trap (defects #2 and #5, one instruction apart)

Worth stating on its own because it will recur in any thunk or prologue decoder.
The MSVC X360 virtual-base adjustor thunk opens by loading the vbase displacement
off `this`:

```
?SetType@RndAnimatable@@$4...  @0x82275560     returns void
    8163FFFC   lwz  r11, -4(r3)      <- `this` is in r3
    7C6B1850   subf r3,  r11, r3

?Handle@RndAnimatable@@$4...   @0x82273F28     returns ?AVDataNode@@ BY VALUE
    8164FFFC   lwz  r11, -4(r4)      <- `this` is in r4 !
    7C8B2050   subf r4,  r11, r4
```

**A method that returns a struct by value gives r3 to the hidden return pointer,
so `this` moves to r4.** A decoder hardcoded to `rA == 3` misses *every*
struct-returning virtual — which is defect #5, and it manufactures disagreements
rather than reporting nothing, because the affected rows silently drop out of the
comparison.

And `0x8163FFFC` decodes as rD=`11`, rA=`3`. **Transpose those two field
extractions and a "loads r11 from r3" filter matches nowhere in 14 MB** — defect
#2. Both bugs are a single wrong constant, both produce confident output, and
neither is visible without a fixture whose correct answer you already know.

## Prove it can fail — `--self-break`

Every one of the six defects is registered as an **injectable broken predicate**,
so the harness's coverage is *executed*, not asserted:

```bash
python3 tools/screen_gate.py --self-break        # 6/6 CAUGHT
```

Each historical defect is injected in turn and the gate must **REFUSE**. If an
injected defect ever slips through, that is reported as a failure *of the
harness*, because a harness that cannot fail is exactly the bug it exists to
prevent. (The idiom is borrowed from `tools/grep_binary_guard.py --self-break`.)

Measured: **6/6 caught** — D1, D2, D3, D5, D6 as `POWERLESS`; D4 as
`INDISCRIMINATE`, the sole catch attributable to the negative-control class.

## The harness caught itself, on its first run

`check_provenance()` initially reported `DRIFT` for an `.s` line that occurs
**13,866** times in the tree. It had concatenated only `files[:400]` of an
**unsorted** 2,515-file glob — so it read an arbitrary ~16% subset and then
reported absence over it.

That is the exact defect class this page documents, shipped **inside the tool
that polices it**, by the lane that had just spent the session studying it. It is
recorded rather than quietly fixed because it is the strongest available argument
that the discipline is not optional: *writing the harness does not make you
immune to the bug.* The fix reads every file, stops at the first hit, and prints
how many files it actually read before concluding (`after 5/2515`) so a reader
can see the scan's reach instead of trusting it.

## Using it

```python
from screen_gate import Screen, gate

s = Screen("my-screen", detect=my_predicate,
           why="fires when the retail extent is an EH funclet")
s.must_fire("known funclet", REAL_LINE_FROM_A_CONFIRMED_ROW)
s.must_not_fire("ordinary prologue", ANOTHER_REAL_LINE)
s.populations("sub-100 rows", treated, "mpn==100 rows", control)

res = gate([s])
if not res.armed:      # POWERLESS / INDISCRIMINATE / DRAINED / VACUOUS-GATE
    sys.exit(2)        # ... and print NO candidates
```

Wrap or instrument an existing screen — **do not rewrite its logic**. The
retrofits in `screen_gate.py` call the landed code directly:
`false_pairing_screen.STRIP`/`FUNCLET` are used as-is, and the handler retrofit
drives the real `handler_sweep.our_blocks()` over a temp tree holding the
fixture. `callsite_screen`'s retrofit reproduces its published measurement
exactly (959/10,959 control, 314/4,695 treated, 0.76×), which is how you know the
wrapper is measuring the same thing the tool measures.

| verdict | meaning |
|---|---|
| `ARMED` | proved power and specificity — **its negatives may be believed** |
| `POWERLESS` | a known positive did not fire — **results are VOID** |
| `INDISCRIMINATE` | a known negative fired — the screen invents defects |
| `DRAINED` | sound, but enrichment below threshold — report a drained vein |
| `VACUOUS-GATE` | the gate itself cannot fail |
| `UNTESTABLE` | the screen could not be exercised — **not a pass** |

## What this does NOT do

- It does not make a screen **correct**, only **non-vacuous**. Passing fixtures
  bound the failure modes you thought of; a screen can be armed and still wrong
  in a way no registered fixture probes. `false_pairing_screen`'s own docstring
  makes the matching point: an empty result there means *the funclet half* of the
  list is clean, not the list.
- It cannot catch a defect in a screen **nobody registered**. Three of the six
  above (#2, #5, #6) came from in-transcript one-offs that were never landed as
  tools, and they are carried here as **reference** screens reproducing the
  *class* with real bytes — not as instrumentation of the original code, which no
  longer exists. A throwaway script in a transcript is exactly where this bug
  lives, and a gate in `tools/` cannot reach it.
- **A hit is still only a reason to read the retail body, not a verdict.**

## See Also

- [gate-liveness-probe.md](gate-liveness-probe.md) — the other non-metric instrument; same lesson that a blunt comparator produces confident wrong answers
- [verifiable-icf.md](verifiable-icf.md) — instruments structurally incapable of settling a question (match-% and raw `memcmp` for ICF)
- [false-layout-drift.md](false-layout-drift.md) — offset diffs that are not layout bugs; rule out before editing
- `tools/grep_binary_guard.py` — the `--self-break` idiom, and the binary-blind `grep` shim
- `tools/callsite_screen.py` — a screen committed as a **drained vein** at 0.76×
