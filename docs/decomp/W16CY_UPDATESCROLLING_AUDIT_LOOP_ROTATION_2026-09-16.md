# W16-CY — audit of W16-CW's "permuter-bound" verdict on `?UpdateScrolling@VocalTrack@@QAAXM@Z`

**Date:** 2026-09-16 · **Branch:** `w16-cy` (off main `c095500e`) · **Result: Δ0, no source edits
retained.** · **Row unchanged at fuzzy 72.27805 / mpn 73.985695 over 8,948 B.**

## Verdict

**W16-CW's verdict is CONFIRMED — independently, and with a sharper mechanism than CW had.**
The row is a codegen-shape wall. But this lane does not confirm it by agreeing with CW's
argument; CW's *load-bearing argument was incomplete*, and the correct mechanism is one CW
never named. The conclusion survives the correction; the reasoning does not.

⚠ This is an opus lane auditing an opus lane (fable was unavailable — infra 429). Two opus
lanes agreeing is worth less than genuine independence, so everything below is measured on
**this lane's own build**, not inherited.

## What was independently reproduced (not inherited)

Own worktree, own full `./tools/ninja-locked`, graded `name_check` ruler resolved from
`report.json` `provenance.diff_config`:

| measure | value |
|---|---|
| fuzzy / mpn | **72.27805 / 73.985695** |
| target_size / base_size | **8,948 / 8,968** (we emit +5 instructions) |
| rows / equal / charges | 2,511 / 1,139 / **1,372** |
| charge split | `diff_arg 754 · insert 274 · delete 269 · replace 58 · diff_op 17` |

Identical to CW's figures in every digit. The row is real and CW measured it correctly.

## ★ The load-bearing claim, and why it was the wrong frame

CW's verdict rested on this:

> The reordered tail block **cannot** be a source reordering, because it stores `itT - begin`,
> which depends on `itT` *after* the phrase loop. Retail falls through into it, we branch to it.

The **dependency half is true but does not carry the weight CW put on it.** Only the
`*itPPtr = itT - begin` store depends on `itT`; the twelve `GetLyricColor`/`GetLyricAlpha`
calls depend on `colorBase`, which is `(staticLyrics ? 8 : 0) | (lead ? 4 : 0)` — **loop-invariant
and known long before the loop**. So "it cannot move earlier" was argued from the one
instruction in the block that cannot move, about a block that is mostly instructions that can.

I checked the obvious consequence — that retail might hoist the colours above the loop — and
**it does not**: retail's colour block sits at `0x8164`–`0x8228`, *after* the phrase loop's head
`TickToMs` pair at `0x8080`/`0x80a8`. Source-order hoisting is refuted on retail bytes. CW's
conclusion is right; its stated reason is not the reason.

## ★★ The real mechanism: our loop is ROTATED, retail's is NOT

This is the finding CW missed, and it explains the mirrored placement *and* the +5 instructions.

Retail's phrase loop, read off the target bytes:

```
0x8054                  loop top  (test lives here)
0x8100-0x8118           staticLyrics window tests
  0x8110 beq 0x828c       <- !tooWide          -> window_ok
  0x8118 beq 0x828c       <- !highlightStarted -> window_ok
                          ... FALLS THROUGH into the break tail ...
0x811c-0x827c           EXIT TAIL: *itPPtr store, colorBase, 8x GetLyricColor,
                          4x GetLyricAlpha, GetLastBakedLyric
0x8280  b 0x915c        -> forward to the bake-loop test
0x8284-0x845c           rest of the loop tests; two of them `bgt cr6, 0x811c` (break)
0x8460-0x8908           loop BODY (GetNextLyricPlate ... SetChunkEnd)
0x891c  b 0x8054        <- UNCONDITIONAL back edge
```

Ours is the mirror image:

```
0x1dbfc bne 0x1e270     <- branches AWAY to the break tail; falls through to window_ok
0x1dc0c                 window_ok
0x1dd08-0x1e238         loop BODY
0x1e268 cmplw cr6, r8, r10
0x1e26c blt  cr6, 0x1db3c   <- CONDITIONAL back edge (bottom test)
0x1e270                 EXIT TAIL
```

Two facts settle it:

1. **The back edge differs in kind.** Retail's is `b 0x8054`, unconditional, with the test at
   the top — an **unrotated** loop. Ours is `cmplw` + `blt`, a **bottom test** — MSVC rotated it.
2. **The +5 instruction surplus is mostly that rotation.** Rows 1647, 1648, 1652, 1653 are
   `lwzx *curPhPtr` / `lwz begin` / `cmplw` / `blt` — **four instructions retail does not have**,
   present on our side only, i.e. the duplicated bottom test.

Block placement is *downstream* of rotation: once MSVC rotates, the exit block naturally follows
the bottom test (our layout); unrotated, the exit block lands inline at the first `break`'s
fall-through (retail's layout). So the 183-instruction "reordered block" is not an independent
defect — it is a **symptom of the rotation decision**, exactly as the repo's standing warning
about `REGISTER_SWAP`-as-symptom would predict.

## ★★★ The lever that governs rotation is MEASURED INERT — this is the falsification test

Rotation is the one axis a source spelling plausibly controls, and the **rb3-Wii oracle differs
from us on exactly that axis**:

- oracle: `while (*curPhPtr < lyricPhrases.size()) { ... }`
- ours:   `for (;;) { if (!(*curPhPtr < lyricPhrases.size())) break; ... }`

**Pre-registered** before measuring: P1 the bottom-test back edge survives (65%); P2 the exit-tail
placement does not flip (70%); P3 fuzzy within ±3 pp, not 100 (95%); P4 charges within ±80 of
1,372 (80%).

**Probe A — rewrote the loop to the oracle's `while` form. Result: BIT-IDENTICAL OUTPUT.**

| | baseline | Probe A |
|---|---|---|
| fuzzy | 72.27805 | **72.27805** |
| base_size | 8,968 | **8,968** |
| charge split | 754/274/269/58/17 | **754/274/269/58/17** |
| our `0x1e268 cmplw` + `0x1e26c blt` | present | **present** |

All four predictions held. **Not vacuous:** the build log shows
`[5/15] MSVC build/45410914/src/band3/bandtrack/VocalTrack.obj` — the TU really recompiled, and
every match-type count is equal rather than absent. MSVC canonicalises `while (cond) {...}` and
`for(;;){ if(!cond) break; ...}` to the same machine code here.

⇒ **The rotation is not reachable through the loop's control-flow spelling.** Since the CFG is
otherwise identical (below), what remains as the cause is register pressure / frame shape, which
is the permuter's class.

## The CFG is identical — verified instruction-by-instruction, not by call census

CW proved call-graph parity by census. I verified something stronger in the decisive region: the
**test chain matches retail one instruction at a time**, including the construct CW warned about.

Our `goto window_ok` spelling is **byte-correct**: retail emits `beq 0x828c` twice then falls
through to the break, with `0x828c` being `window_ok` and carrying
`if (sectionOnly && phStartMs > sectionEnd - 100.0f) break;`. Ours emits the same two tests in the
same order against the same operands. **CW's "do NOT retarget the goto at the oracle's `||` chain"
is independently confirmed** — and for a better reason than CW gave: not that ours "matches
better", but that ours matches *exactly*.

Five `break` sites, same order, same tests, both sides. Confirmed the exit block is a 5-predecessor
join on both sides (retail: fall-through from `0x8118` plus `bgt cr6, 0x811c` from `0x8288` and
`0x82a4`).

## Why there is no path to 100, and therefore no bytes

`matched_code` is all-or-nothing per row, so only crossing to 100 pays. Accounting the residue:

- **The reordered block is 229 of 543 insert/delete** (89-instruction delete cluster idx 1097-1185;
  43+43 insert clusters idx 1652-1694 / 1707-1749). Fixing it perfectly leaves **314 diffuse
  insert/delete**, spread across *every* index band (7/27/42/55/108/66/121/26/33/58 per 250 rows) —
  not concentrated anywhere.
- **754 `diff_arg` decompose as 398 register-only, 197 immediate/offset-only, 158 other, 1 symbol.**
  Of the 197, **162 are `r1`-relative stack slots**. So ~560 of 754 are pure allocation/frame noise.
- Stack layout reports 8 SWAPPED, 8 SHIFTED, 37 DIFFER, 12 PERMUTED, 4/5 TGT/BASE-only.

Even granting the brief's hoped-for 13.5:1 dissolution ratio from W16-CR, there is no arithmetic
that takes 1,372 charges over 2,511 instructions to zero without reproducing retail's register
allocation wholesale. **Both probes left whole-binary `matched_code` at exactly 4,118,696** — the
all-or-nothing rule demonstrating itself.

## ⛔ Prediction that MISSED — recorded so nobody repeats it

Adjudicating the lone `SIGNEDNESS_MISMATCH` found a genuine source/retail divergence at idx 2319:
retail `cmplw cr6, r9, r11` vs ours `cmpw cr6, r11, r9`, at
`if (staticLyrics && (int)lyricPhrases.size() == *curPhPtr && ...)`. Our source carries an `(int)`
cast that forces a **signed** compare where retail emits **unsigned**; the cast looked spurious.

**Probe B — dropped the cast (`*curPhPtr == lyricPhrases.size()`). Predicted the row would close.
Measured fuzzy 72.225746 / mpn 73.92445 — WORSE by 0.052 pp. Prediction missed; reverted.**
Retail's operand order is not recovered by the rewrite and an adjacent row regressed. The
divergence is real but is **not** closed by removing the cast; do not re-try this exact edit.

## What this lane did NOT do

- **No body port.** CW is right that there is no body to port, and the unit holds 179/220 rows.
- **No permuter** (off by standing directive), and I do not propose it as the answer.
- **Did not chase** the 37 DIFFER stack slots or the 4/5 TGT/BASE-only locals. That is a
  whole-function frame reconstruction, which is the thing the brief forbids funding here.
- **Did not re-run CW's four refuted priors** (MILO_WARN/TheDebug block, `float &lastLyricX`,
  wrong-constant, oracle `||` retarget). I did independently re-confirm the goto/`window_ok` one
  as a by-product of reading the test chain.
- **Did not investigate** the 3 fuzzy-0 `Unlockable` vector-template rows (568 B) CW flagged as a
  pin/attribution question. Still open, still looks like attribution rather than source.

## Routing recommendation

**Leave the row closed.** Two independent lanes now reach the same verdict by different routes:
CW by call-graph and opcode-multiset census, this lane by CFG identity plus a *measured* refutation
of the only source lever. The residue is loop rotation + register allocation, and the rotation is
demonstrably not spelling-reachable.

If the permuter directive is ever revisited this remains the canonical candidate. Until then, the
correct reason to decline it is **not** "72% means reconstruction" (CM's prior, which CW correctly
corrected) and **not** "the tail block cannot move" (CW's stated reason, which is incomplete) — it
is **"our loop is rotated and retail's is not, and `while` ≡ `for(;;)+break` under this compiler."**

## Reproduction

```bash
scripts/setup_worktree.sh ~/tmp/wt-w16-cy w16-cy
cd ~/tmp/wt-w16-cy && ./tools/ninja-locked            # full build; never `ninja <one>.obj`
./bin/objdiff-cli diff -p . -u default/VocalTrack \
    '?UpdateScrolling@VocalTrack@@QAAXM@Z' --include-instructions -f json -o /tmp/d.json
```
Then sort rows by `target.address` and by `base.address` separately to recover each side's
**physical** order from the aligned diff; the back-edge kinds at retail `0x891c` and ours `0x1e26c`
are the whole finding. ⚠ Use the graded ruler (the default; `objdiff.json` pins it) — a `none`
reading of this row is blind to the relocation-name class.
