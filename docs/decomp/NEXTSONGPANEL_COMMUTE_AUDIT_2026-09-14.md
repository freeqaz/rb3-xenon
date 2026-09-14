# NextSongPanel::CountOrCreateExpandedDetails — the "ARITH_COMMUTE proved inert" refusal, audited and CROSSED

**Lane W16-C (Fable escalation), 2026-09-14. Branch `w16-c`, worktree `~/tmp/wt-w16-c`, base `adc98a6f`.**

Row: `?CountOrCreateExpandedDetails@NextSongPanel@@QAAHHAAVDataArrayPtr@@_N@Z` (retail `fn_82645320`,
12,220 B, unit `default/NextSongPanel`). Refused in `GAME_CROSSING_GRIND_2026-09-14.md` §"why declined" as
*"mm=1, `add r3,r11,r28` vs `add r3,r28,r11` — ARITH_COMMUTE, the class `crossing_worklist.py` proved inert
by direct experiment (MSVC canonicalises commutative operand order; the source edit is a no-op)."*

## Verdict

**CROSSED — fuzzy 99.99673 → 100.0, +12,220 B graded, +0 functions, predicted exactly.** Two-line source
change (`src/band3/meta_band/NextSongPanel.cpp`): an explicit `Symbol(*it)` copy at the goals-loop site,
and reverting the prior lane's `ptr->Node` back to `ptr.Node` at the first section-loop site.

The refusal label was **right about its lever and wrong about its conclusion**. `crossing_worklist.py` proved
that swapping `a + b` → `b + a` *in source* is a no-op. It is — the operand order of this `add` is not a
property of the expression at all. It is a property of the *whole function's optimisation history*, and a
codegen-invisible edit **elsewhere** in the function moves it. "MSVC canonicalises" was never the rule; the
rule (characterised below, on retail bytes and on 40 probe compiles) is that the order is **history-dependent,
shape-independent, and per-function**.

## Numbers (provenance: graded `name_check` ruler, `report.json` in the worktree, objdiff 4.2.8 / `032122696555`)

| key | baseline (`adc98a6f`, full build, log `~/tmp/rb3_build_w16c.log`) | pre-registered | measured (full build, `~/tmp/rb3_build_w16c_2.log`, 1 TU recompiled) |
|---|---:|---:|---:|
| row `fuzzy_match_percent` | 99.99673 | 100.0 | **100.0** |
| row `match_percent_normalized` | 100.0 | 100.0 | 100.0 |
| `matched_code` | 3,908,104 B | 3,920,324 (+12,220) | **3,920,324** |
| `matched_functions` | 43,101 | 43,101 (+0) | **43,101** |
| `matched_code_percent` | 38.14289 | 38.26216 | **38.262157** |
| unit `default/NextSongPanel` `matched_code` | 16,088 / 33,456 | 28,308 | **28,308** (334/350 fns, unchanged) |

`total_code` 10,245,956. The unit delta equals the row's size exactly, so no other row moved.
Settled A/B (`tools/ab_measure.py --worktree ~/tmp/wt-w16-c --from-dirty`): see §"Settled A/B" at the end.

## What retail encodes — read on retail bytes (`build/45410914/asm/NextSongPanel.s`, keyed on `.fn fn_82645320`)

The function has **50** register-register `add r3,…` sites. 45 are `add r3,r10,r11` (`&mNodes[count]` with
`count` freshly loaded) and are equal on both sides. Five sit inside two loops where MSVC strength-reduced
`count*8` into a callee-saved induction variable `r28` (`slwi r28,r29,3` in the preheader; `addi r28,r28,8`
after every use; `r29 = count`, `r27 = &ptr`). Those five are the only sites where the operand order can
differ, because they are the only sites where the addend is a *kept* value rather than a scratch temp:

| retail addr | loop / source site (our line) | retail order | ours, before | ours, after |
|---|---|---|---|---|
| `0x82646730` | goals loop, l.558 `ptr.Node(count++) = DataArrayPtr(label, cur)` | `add r3,r11,r28` BASE-first | `r28,r11` IV-first **(charged)** | BASE-first |
| `0x82646C98` | section loop site 1, l.605 (`unk4 < 0`, left_label) | `r11,r28` BASE-first | BASE-first (via `->`) | BASE-first (via `.`) |
| `0x82646D48` | section loop site 2, l.610 | `r28,r11` IV-first | IV-first | IV-first |
| `0x82646E00` | section loop site 3, l.616 | `r28,r11` IV-first | IV-first | IV-first |
| `0x82646EFC` | section loop site 4, l.625 | `r28,r11` IV-first | IV-first | IV-first |

Every one of the five has the *identical* seven-instruction schedule around it
(`lwz r11,0(r27); addi r4,r31,<tmp>; addi r29,r29,1; lwz r11,0(r11); add r3,…; addi r28,r28,8; bl DataNode::operator=`).
Sites 1 and 3 of the section loop are **textually identical source** and retail orders them differently.
So on retail's own bytes the order is not a function of liveness, schedule, register class, or source shape.
That rules out every "what feeds r11" reading the brief asked for: r11 is `mNodes` in all five cases, r28 is
the IV in all five, and the two orders coexist with everything else equal.

## What the order actually is — characterised empirically (40 compiles, same cl.exe 10224, same flags, `/FAs`)

Instrument: compile a TU outside the tree with NextSongPanel's exact command line plus `/FAs`, read the
listing (1.9 s for the real 42k-line TU). Harness lived in `~/tmp/w16c_probe/` (`compile.sh`, `screen.py`);
the command is reproduced at the end of this doc. Each probe reports, for every IV-form add (an
`add r3,X,Y` immediately followed by `addi rN,rN,8`), whether it is IV-first or BASE-first, and whether the
function's instruction stream is otherwise identical to baseline (labels/temps/unwind numbers normalised,
the IV adds operand-canonicalised).

**1. The goals loop in isolation compiles BASE-first — retail's order — with plain `ptr.Node(count++)`.**
So our spelling is not "wrong"; the context of the real function is what flips it.

**2. Unrelated sites before/after the loop move it, non-monotonically** (small TU: goals loop + N copies of
`if (b) count++; else ptr.Node(count++) = DataArrayPtr(sym);`):

| context | goals-loop add |
|---|---|
| alone; 1 or 2 sites before | BASE-first |
| 3 or 4 sites before | IV-first |
| 1 site after | IV-first |
| 2 sites after | BASE-first |
| 1 before + 1 after | IV-first |

Code *after* the loop changes the order *inside* it, and the dependence is not monotone in the amount of code.

**3. Other functions in the TU are inert.** Five kinds of dummy function (leaf int, one `ptr.Node` site, a
`ptr.Node` loop, two loops) placed before or after the probe: all ten compiles BASE-first. The state is
**per-function**, so the 16 unmatched functions elsewhere in NextSongPanel.cpp are not a lever.

**4. On the real TU, codegen-free edits move sites in coupled sets.** Baseline is `[558 IV, 605 BASE, 610 IV,
616 IV, 625 IV]` (605 BASE only because the prior lane put `->` there — `605dot_only` gives all-IV).

| edit at the goals site (l.554–558) | 558 | 605 | 610/616/625 | rest of function |
|---|---|---|---|---|
| `ptr->Node` (prior lane's V2, 1b7a698e) | IV | BASE | IV IV IV | identical — calibration against the prior lane's full builds: agrees |
| `Symbol cur(*it);` direct-init | IV | BASE | IV IV IV | identical |
| `it++` instead of `++it` | IV | BASE | IV IV IV | identical |
| `cur = cur;` self-assign | IV | BASE | IV IV IV | identical |
| `const_iterator` loop (+605dot) | IV | IV | IV IV IV | identical |
| `DataArrayPtr(DataNode(label), cur)` (+605dot) | IV | IV | IV IV IV | identical |
| evaluated `MILO_ASSERT_FMT` (`((void)(cond))`) | IV | BASE | IV IV IV | identical — **refuted** as the mechanism |
| `const Symbol &cur` | IV | BASE | IV IV IV | **differs** (load moves under the branch) |
| `DataArrayPtr(label, *it)` no local | IV | IV | IV IV IV | differs |
| `if (!b) … else count++` | IV | BASE | IV IV IV | differs (branch sense) |
| `Node(count) = …; count++;` | — | — | (4 sites) | differs, one IV site lost |
| `int idx = count++;` | IV | IV | IV IV IV | differs |
| **`Symbol cur2 = cur;` dead copy** | **BASE** | IV | IV IV IV | identical |
| **`DataArrayPtr(label, Symbol(cur))`** | **BASE** | IV | IV IV IV | identical |
| **`Symbol cur = Symbol(*it);`** | **BASE** | IV | IV IV IV | identical |
| **`DataArrayPtr(Symbol(label), cur)`** | **BASE** | IV | IV IV IV | identical |
| **`Symbol lbl = label;` + use** | **BASE** | IV | IV IV IV | identical |
| `DataArrayPtr(Symbol(label), Symbol(cur))` | IV | IV | IV IV **BASE** | identical |
| `->` at l.548 / l.565 / l.610 / l.616 (single) | IV | BASE | IV IV IV | identical (inert) |
| `->` at l.625 (single) | IV | BASE | IV IV **BASE** | identical |
| **any of the five bold rows + `.Node` at 605** | **BASE** | **BASE** | IV IV IV | identical — **retail's pattern** |

The five spellings that flip 558 are all "one extra `Symbol` copy that the optimiser creates and then
eliminates". Each flips **558 and 605 together** (558 → BASE, 605 → IV); the prior lane's `->` at 605 was
only ever compensating for 558 being wrong. Remove that compensation and the pattern is retail's.

**The rule, stated as far as the evidence allows.** MSVC 10224's operand order for a commutative `add`
whose operands are a loaded base and a strength-reduced IV is decided by *internal identity of the two
operand nodes* (their order of creation or their position in a recycled pool), not by their shape. Evidence:
identical-text sites order differently within one retail function; the order depends on code after the
site; it is non-monotone in the amount of surrounding code; it is invariant to other functions in the TU;
and it is shifted by constructs that create temporaries which never reach codegen. I could not reduce that
to a closed-form predicate on source (I did not disassemble the compiler), and I do not think one exists
at source level — the only way to find a matching spelling is to **screen** spellings, which the 2 s
harness makes cheap. "MSVC canonicalises" is false as stated: retail itself carries both orders.

## Hypotheses, pre-registered and refuted

- **H1 (brief's suggestion): what feeds r11 differs.** Refuted on retail bytes — r11 is `lwz r11,0(r11)`
  (`mNodes`) at all five sites, both binaries, identical schedules.
- **H2: `->` at the goals site.** Pre-refuted by 1b7a698e (V2/V4/V5 inert); reproduced inert in the harness.
- **H3: the evaluated-assert difference (`MILO_ASSERT_FMT` is `sizeof`-unevaluated here).** Refuted: it moves
  605, not 558. `os/Debug.h`'s comment records the FMT no-op was itself measured; left untouched.
- **H4: TU-level history (unmatched functions earlier in the TU).** Refuted, 10 compiles.
- **H5: an extra eliminated temporary at the goals site + plain `.Node` at 605.** Pre-registered fuzzy → 100,
  +12,220 B, +0 fns, 1 TU recompiled. **Measured exactly.**

## What I did NOT do

- Did not disassemble cl.exe or derive the optimiser's node-ordering rule in closed form. The characterisation
  above is empirical (40 compiles) and is sufficient to cross the row and to stop the next lane re-hunting it
  as a "canonicalisation" question; it is not a proof of mechanism.
- Did not touch `os/Debug.h` or `obj/Data.h` (both tree-wide; H3 was refuted before any edit).
- Did not test `unsigned count` (whole-function change) — unnecessary once H5 crossed.
- Did not commit the probe harness; the command line is below and rebuilds in a minute.
- Did not run `ab_measure` before the full-build read; the settled A/B ran after it (result below).
- Did not remove the Metrowerks `#pragma opt_usedef_mem_limit 300` (l.366) — MSVC ignores it. The C4068
  warnings in the build log are at l.249/278 (`#pragma pool_data off/reset`), also Metrowerks, also inert.

## Behavioural bugs noticed in NextSongPanel.cpp

None. `CountOrCreateExpandedDetails` is now byte-identical to retail, so its behaviour is retail's by
construction; the body is a uniform count-or-create layout builder and nothing else in it read as a defect.

## Harness (reproduce in ~1 minute)

```sh
# from the worktree root; the TU copy lives in ../w16c_probe/nsp.cpp
WIBO_FS_CACHE=1 /home/free/code/milohax/wibo/build/release/wibo build/compilers/X360/16.00.10224.00/cl.exe \
  /I src/system/stlport /I src/xdk/LIBCMT /I src /I src/system /I src/system/oggvorbis /I src/band3 \
  /I src/network /I src/system/speex/include /nologo /wd4355 /wd4164 /c /GR /O1 /Oi /EHsc /TP \
  /DRB3_HANDLE_LOCAL_STATIC /FAs /Fa../w16c_probe/nsp.asm /Fo../w16c_probe/nsp.obj ../w16c_probe/nsp.cpp
```
Then in the listing find the `?CountOrCreateExpandedDetails@…` `PROC NEAR` … `ENDP` span (the listing is
CRLF, and the PROC line uses a **tab** before `PROC` — both broke a first version of the scanner and made it
report "no adds" for every variant), list every `add r3,rX,rY` followed by `addi rN,rN,8`, and compare the
rest of the instruction stream to baseline after normalising `$LN/$T/$M/__unwind$` numbers. Traps hit:
cl.exe treats an absolute unix path argument as an option (use worktree-relative paths); `echo =====` is a
zsh `=cmd` expansion.

## Settled A/B

`tools/ab_measure.py --worktree ~/tmp/wt-w16-c --from-dirty`, run `20260914-044420-from-dirty-4104133`
(log `~/tmp/ab_w16c.log`), ruler `name_check` from `objdiff.json` options, objdiff-cli `a5c35b15d7d46ac4`
stable across both legs, both legs settled (leg A 0 recompiles; leg B 1 recompile, 6 patch steps, 2 settle
iterations):

```
leg A: matched=43101 masked=22999 honest=20102 code%=38.142890
leg B: matched=43101 masked=22999 honest=20102 code%=38.262157
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.119267pp  Δcode_bytes=+12220
[control none] Δmatched_code=+12220 B  (moves on both rulers ⇒ an instruction fix, not an alias)
units at 100%: 167 -> 167 (mpn), 137 -> 137 (all-rows-fuzzy)
```

## For the next lane

Do not re-open this row, and do not re-open any other `ARITH_COMMUTE`/`diff_arg`-on-`add` row *as a
canonicalisation question*. The operand order of a strength-reduced `add` is history-dependent; the lever is a
codegen-free temporary somewhere in the function, found by screening spellings with the `/FAs` harness above
(2 s per compile on a 12 kB function), never by swapping `a+b` for `b+a`. Expect coupled moves: one edit
shifts a subset of the IV sites, so screen all of them together and require the rest of the instruction
stream to be identical before spending a full build.
