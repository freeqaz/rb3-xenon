# W16-FR — splitting a byte-identical twin pair on evidence, and closing QuestFilterPanel

**Date** 2026-09-16 · **Base** `main` `20e2614e` · **Branch** `w16-fr` ·
**Ruler** `name_check` (graded), read from `report.json`'s `provenance.diff_config`.

Brief: the **four anonymous rows / 880 B** W16-FN
(`docs/decomp/W16FN_QUESTFILTER_OUTLINED_HELPERS_AND_LOCAL_STATICS_2026-09-16.md`)
identified and then deliberately left open rather than land half-adjudicated map
edits. Its stated success condition was that **splitting the 192 B twin pair
correctly on string-literal evidence is worth more than closing all four rows on
a guess**, and that a non-discriminating result should be reported as a real
negative.

**Delivered +880 B / +4 functions** — the full deferred prize — across two
pre-registered whole-binary A/Bs. Every predicted delta hit exactly and **no
falsifier fired**.

| # | change | Δcode_bytes | Δmatched | Δcode% |
|---|---|---:|---:|---|
| 1 | split + name + fix the twin pair (`GetSongSelectScreen` / `GetDiffSelectScreen`) | **+384** | +2 | +0.003746 pp |
| 2 | name + fix `GetSelectedFilter`, `Refresh` | **+496** | +2 | +0.004841 pp |
| | **lane total** | **+880** | **+4** | **+0.008587 pp** |

Whole-binary: `matched_code` 4,148,932 → 4,149,812; `matched_functions`
44,034 → 44,038; `matched_code_percent` 40.488968 → 40.497555.
`total_code` **10,247,068** and `total_functions` **69,240** identical on all
four legs, so the percentages are comparable across both runs.

Unit `default/QuestFilterPanel`: `matched_code` 6,084 → **6,964** of 7,004,
rows at 100 **67/71 → 71/71**. The unit **reaches 100% on the `mpn` ruler**
(units at 100%: 191 → 192, mechanism `MATCHED_ROSE`).

---

## 1. The trap, and why it was a real one

`fn_82B7A7D0` and `fn_82B7A8E0` are **192 B each and instruction-for-instruction
identical**. Diffed as text with the address/encoding columns stripped, the two
bodies differ in exactly **four relocation targets** and the local label names:

| | `fn_82B7A7D0` | `fn_82B7A8E0` |
|---|---|---|
| guard word | `lbl_82E128CC` | `lbl_82E128D8` |
| static `Message` | `lbl_82E128C4` | `lbl_82E128D0` |
| `atexit` thunk | `fn_82C4BA18` | `fn_82C4BA38` |
| **string literal** | `lbl_82199420` | `lbl_82199498` |

Three of those four are addresses of objects created *by* the function, so they
carry no independent meaning. **Only the string literal has semantic content**,
and the call sequence (`Symbol` ctor → `Message` ctor → `atexit` → virtual
`Handle` → `DataNode::Sym` → release) is byte-identical between them. So a
callee-based identification is **structurally incapable** of separating
`GetSongSelectScreen` from `GetDiffSelectScreen` — W16-FN's refusal to guess was
correct, not conservative.

## 2. Reading the literals — and validating the instrument BEFORE trusting it

The literals live at `0x82199420` / `0x82199498`, inside `.rdata`
(`0x82000400`–`0x821F1584`) — far below this unit's pinned `.text`
(`0x82B7A1A8`+), so **they appear in no `.s` file anywhere in the tree**. They
were read straight out of `orig/45410914/band.exe` with a section-aware VA→file
offset walk in Python (the house rule: the shell's `grep` is a ugrep shim with
`-I` and returns false negatives shaped exactly like decisive ones).

| VA | string |
|---|---|
| `0x82199420` | `get_songselect_screen` |
| `0x82199498` | `get_diffselect_screen` |

Each occurs **exactly once in the whole 14 MB image**, so neither is ambiguous.

★ **The instrument was validated on a positive control that could have failed.**
`GetBackScreen` (`fn_82B7A6C0`) in this same TU is **already at fuzzy 100** with
in-tree source that reads `static Message get_backscreen_msg("get_backscreen")`.
Pointing the same reader at its literal (`0x821993B0`) returns exactly
`get_backscreen`. A reader with a wrong image base, a wrong section walk or an
off-by-one would have returned garbage here. Running that control first is what
separates "my script printed a plausible string" from evidence.

## 3. A second, fully independent instrument agreed — argument evaluation order

The literals alone would have been enough, but the identification does not rest
on a single method. Retail's `HandleFilterSelected` (`fn_82B7A9E8`, **already
named and already at fuzzy 100**) issues, in order:

```
bl ?GetBackScreen@QuestFilterPanel@@QAA?AVSymbol@@XZ   (fn_82B7A6C0, already mapped)
bl fn_82B7A8E0
bl fn_82B7A7D0
bl ?LaunchQuestFilter@Tour@@QAAXHVSymbol@@00W4TourSetlistType@@000@Z   (already mapped)
```

Our source passes `LaunchQuestFilter(..., GetSongSelectScreen(),
GetDiffSelectScreen(), GetBackScreen())` and **MSVC evaluates arguments
right-to-left**, so the emission order must be `GetBackScreen`,
`GetDiffSelectScreen`, `GetSongSelectScreen`. That pins
`fn_82B7A8E0 = GetDiffSelectScreen` and `fn_82B7A7D0 = GetSongSelectScreen` —
**the same answer the literals gave**, derived from a disjoint fact (call
ordering anchored by two neighbouring names that were already adjudicated).

Two independent instruments agreeing is what licensed the map edit. Either one
alone would have been a single point of failure on a 50/50 question.

## 4. A third check was built into the A/B, as a pre-registered falsifier

Because `HandleFilterSelected` calls **both** members of the pair and is
**currently at fuzzy 100**, naming the pair converts its two `bl` targets from
placeholder-**forgiven** to name-**checked**. So a swapped pair could not hide:

- both twins would score ~0 (a wrong name scores ~0, not 100), **and**
- the 244 B `HandleFilterSelected` would **fall off 100**, turning a predicted
  +384 into at best +140.

That was pre-registered as falsifier B before the build. Measured: both twins hit
**fuzzy 100.0 on the first build** and `HandleFilterSelected` **stayed at 100.0**.
Simultaneous 100s on two byte-identical bodies are only consistent with the
correct assignment.

⇒ **The pair is split on three independent lines of evidence**, one of which is
a control that could have failed and one of which is a falsifier that could have
fired.

## 5. The other two rows, adjudicated on retail bytes before the map was touched

Both are the same **function-local-static** class W16-FN documented (retail
builds the `Symbol`/`Message` token inside the body under a guard bit; our
inherited source referenced the file-scope `extern` from
`utl/Messages.h` / `utl/Messages2.h`).

| row | size | literal | corroboration | ⇒ |
|---|---:|---|---|---|
| `fn_82B7A3D8` | 300 B | `0x821992C0` = `get_selected_filter_index` | two refs to `""` at `0x82000C55` = the two `return ""` paths; `lwz 0x20(this)` vs `1` = the state test; provider at `0x58`, vcall slot `0x28` = `NumData`, `0x20` = `DataSymbol` | `GetSelectedFilter` |
| `fn_82B7BCB0` | 196 B | `0x820ACC1C` = `update_all` | calls `fn_82B7BB18`, **already mapped** as `UpdateFilters`; `TheTour` → `GetTourProgress` → `lwz 0x80` → `stw 0x54(this)` = `m_symQuest = pProgress->mCurrentQuest` | `Refresh` |

★ **Guard position is source position, and it is readable off the asm.**
`GetSelectedFilter`'s guard sits at `.L_82B7A40C` — *inside* the `else` arm, past
the state test — so the `static` is declared there, not at the top of the
function. `Refresh`'s guard sits *after* the `bl UpdateFilters`, so its `static`
is declared after that call. This is the same reasoning W16-FN used to conclude
that `setlist_song_fmt` gets two statics because it is declared once per `if`
arm; the guard's placement in the instruction stream is a direct readout of the
declaration's placement in the source.

## 6. The source fix was a template match, not a rewrite

`GetBackScreen` (`fn_82B7A6C0`, 192 B, already at fuzzy 100) is
**instruction-for-instruction identical to `fn_82B7A7D0`** apart from the four
relocations in §1. So its in-tree source shape *is* the answer, and the fix for
both twins was to mirror it:

```cpp
static Message get_songselect_screen_msg("get_songselect_screen");
DataNode dn(Handle(get_songselect_screen_msg, true));
return dn.Sym();
```

replacing `return Handle(<file-scope extern>, true).Sym();`. The local-static
idiom was **already present twice in this same TU** (`GetBackScreen`, and
`get_selected_filter_index_msg` at line 170 in another method), so this was
applying an established in-tree pattern TU-wide, not inventing one.

⇒ **When a target row is byte-identical to a neighbouring row you already match,
stop deriving and copy the neighbour's source shape.** That is a cheaper and
more reliable lever than reading the asm term by term.

## 7. Blast radius was bounded before each map edit

Naming an address converts every call site to it from placeholder-forgiven to
name-checked, and un-pairing is ~80.5% of a map edit's delta. Scanned tree-wide
on the `.fn`/`bl` symbol (never the `.s` address column — synthetic in this
6-block unit):

- each of the four addresses has **exactly one `bl` in the whole tree**, and all
  four are inside `QuestFilterPanel.s` itself (callers `HandleFilterSelected` and
  `Enter`, both already named and both at fuzzy 100);
- every other reference is a `.4byte fn_...+0xNN` **EH IP-to-state entry** inside
  the unpinned `auto_00_82000400_rdata` unit, which is unpairable in both legs
  and therefore cannot move.

"No unit other than `default/QuestFilterPanel` moves" was pre-registered as
falsifier C both times. Measured: `unit net (ALL units)` equalled whole-binary
Δmatched (+2, +2) in **both** runs, and **0 units fell off 100** in either.

---

## What I did NOT do, and why

**`GetSetlistType` (`fn_82B7A078`) is still anonymous — deliberately, for the
second lane running.** It sits **below** this unit's pinned `.text` (which starts
`0x82B7A1A8`), inside the span attributed to `GemManager.s`. Naming it requires a
**pin re-home**, and re-homing an already-pinned address is **not
metric-neutral** (adding a pin over `auto_*` code is; re-homing is not). It costs
nothing to leave alone: placeholder forgiveness already scores all three of its
call sites equal. W16-FO's measurement is the relevant precedent — a re-home buys
**legibility, not bytes** (one moved pin took a row 0.00 → 80.714 **and
stopped**). That is a separate lever with a separate risk profile and it should
be priced and pre-registered on its own, not smuggled into a naming lane.

**`fn_82B7A260` (40 B, fuzzy 99.50, mpn 100.0) is left alone**, same call as
W16-FN. It is a `??1NetMessage@@UAA@XZ` destructor thunk, not a member of either
class in this TU, and it is already pairing by funclet byte signature
(`masked_equal`) — disclosure class, not source work. It is the **only** reason
the unit reads 100% on the `mpn` ruler but not on the all-rows-`fuzzy` ruler,
and chasing it would be chasing a disclosure artifact.

**I did not re-measure the "naming alone pays 0 bytes" split.** W16-FN measured
it inside one lane (+0 B for the map name, +0 B for the source flip, +428 B
jointly) and the brief explicitly said not to re-derive it. Both increments here
therefore bundle the name and the source fix and are priced as one change each.
The consequence worth stating: **these +880 B are joint products** — neither half
of either increment would have paid anything alone, and reporting the fuzzy
movement from naming as if it were bytes would have been wrong.

**No permuter run.** Every residual in this unit was a source-level structural
defect (a file-scope extern where retail has a function-local static); nothing
here was regalloc- or scheduling-bound.

**No `.pdata` was hand-edited** — it is derived output, re-derived from the
`.text` splits on every split run. No `splits.txt` change was needed at all.

## The reusable result

The general shape, which is what to carry forward:

★ **When two candidate identities produce byte-identical code, the discriminator
is the DATA the code points at, not the code.** Here that data was in unpinned
`.rdata` and therefore invisible to every tool in the tree that reads `.s` files;
it had to be read out of the retail PE directly. **A "these are indistinguishable"
verdict from a callee- or shape-based tool is a statement about the tool's
inputs, not about the functions** — the same disease as an `AT_LIMIT` label on a
relocation-name-only row.

★ **Validate a bespoke binary reader on a row you already match before you
believe it on a row you do not.** `GetBackScreen` cost one extra lookup and
converted the whole instrument from plausible to proven.

★ **Prefer an identification that a pre-registered falsifier can kill.** The
caller being at fuzzy 100 and calling *both* twins meant a swap was
self-announcing. Choosing that framing was worth more than adding a fourth
argument for the answer I already had.
