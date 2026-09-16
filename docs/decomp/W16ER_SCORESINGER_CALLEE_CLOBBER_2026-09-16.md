# W16-ER — ScoreSinger's 19 charges were all its CALLEE's fault

Lane W16-ER, 2026-09-16, worktree `~/tmp/wt-w16-er`, branch `w16-er`, off main `0f93166c`.
Ruler: shipped graded `name_check`, read from `build/45410914/report.json` `provenance.diff_config`
(22 keys) — not assumed. Baseline (leg A, measured in this worktree):
`matched=43958 masked_equal=23224 honest=20734 code%=40.260880 fuzzy=50.010845`.

## Result in one line

`?ScoreSinger@VocalPart@@…` goes **90.881355 → 100.000000, diff_score 1076 → 0, +472 B**,
and **not one byte of the fix is in ScoreSinger**. Every charge was induced by its callee
`GetNoteRange`, which we had implemented as the wrong algorithm.

## 1. The brief verified literally

Confirmed before building anything on it: `size 472`, `fuzzy 90.881355`, `mpn 91.13559`,
unit `default/VocalPart`. Exact. ⚠ `size` comes back as the JSON **string** `"472"` — the
coercion trap is live in this file. (The protobuf omitted-defaults trap is live too: some
units carry **no `functions` key at all**, and an unguarded `u['functions']` raises `KeyError`.
It did.)

## 2. Charge anatomy — 19 sites, zero of them relocation names

`run_diff_inspect mode=diagnose` noise budget: **`Symbol relocs: 0 arg diffs`**. Not one
charge is a wrong callee or an ICF fold survivor. 104/123 equal; 7 `diff_arg`, 5 `insert`,
4 `replace`, 3 `delete`, **0 `diff_op`**. All 19 sit in ONE window (idx 47–72, the argument
setup for a single call) plus one straggler at idx 107.

The call is `GetBestHit`, 11 params, so slots 9–12 go to the outgoing stack area at
`0x54/0x5c/0x64/0x6c` (8-byte slots). Resolving both sides:

| slot | param | retail | ours |
|---|---|---|---|
| 0x54 (9) | `int& noteMatched` | `r1+0x88` | `r1+0x78` ✗ |
| 0x5c (10) | `float& bestPitch` | `r1+0x80` | `r1+0x80` ✓ |
| 0x64 (11) | `float& sloppyArg` | `r1+0x8c` | `r1+0x88` ✗ |
| 0x6c (12) | `bool& talkyHit` | `r1+0x70` | `r1+0x70` ✓ |

As an ordered frame, retail is **ours with one extra local spliced in**:

```
ours   (0x70→0x88): talkyHit endNote beginNote octaves bestPitch pitch             sloppyArg
retail (0x70→0x8c): talkyHit endNote beginNote octaves bestPitch pitch noteMatched sloppyArg
```

MSVC had **coalesced `noteMatched` into `beginNote`'s slot** — legal, since `beginNote`'s
value is loaded to `r5` before the call and the callee only writes `noteMatched` during it.
Retail declined to.

Separately, an exact instruction-multiset difference (discounting label rendering and branch
addresses, which the diff already scores equal) gives our **entire** surplus as two
instructions — `mr r3, r31` and `fmr f1, f29` — accounting for the whole
`base_size 480 − target_size 472 = 8 B`.

⚠ Note what the tool called these: a `-4`/`+16` **offset shift** and **`r11<->r29`,
`r11<->r9` register swaps across 4 pairs**. There is no real register swap and no real
offset bug. Both labels are symptoms of one allocator decision, and both dissolved without
being touched — the 13th+ recorded instance of the standing rule that `REGISTER_SWAP` is a
symptom, not a diagnosis.

## 3. The mechanism: MSVC X360 /O1 does intra-TU callee clobber analysis

Retail calls `GetBestHit` **without reloading `this` or `ms`**, relying on both surviving the
preceding call. That is only sound given interprocedural knowledge. Dumping the callee
(keyed on the `.fn` symbol, **never** the synthetic address column) settles it:
`fn_826F1EC8` is a leaf that reads `r3` and `f1` and **writes neither**.

So the caller's codegen is a function of what the callee clobbers. Our `GetNoteRange` was a
`std::upper_bound` binary search; retail's is a backward linear scan from a cached index.
The heavier body forced *both* the `this`/`ms` reload *and* the different stack packing.

**Decisive probe.** Stubbing `GetNoteRange` to a body that provably touches neither `r3` nor
`f1` (kept out-of-line with `__declspec(noinline)`), a pure diagnostic since reverted:

| | fuzzy | diff_score | charges | base_size | slot 9 / 11 |
|---|---|---|---|---|---|
| baseline | 90.881355 | 1076 | 19 | 480 | 0x78 / 0x88 |
| callee stubbed | 94.805084 | 613 | 9 | **472** | **0x88 / 0x8c** ✓ |
| retail algorithm | **100.000000** | **0** | **0** | **472** | **0x88 / 0x8c** ✓ |

The stub's residual 9 were an identical instruction multiset in a different order — a
scheduling artifact of the stub *not reading `f1`*, where retail's body does
(`fsubs f0, f1, f0`). The faithful body closed them.

## 4. Two negative results — both lore-sanctioned levers are INERT here

Each measured with the recompile **confirmed in the build log**
(`[5/14] MSVC …/VocalPart.obj`), so neither is absent-vs-absent:

| probe | result |
|---|---|
| local **declaration** order (`int noteMatched;` moved) | byte-identical, Δ0 |
| function **definition** order (`ScoreSinger` moved below all three callees) | byte-identical, Δ0 |

⇒ `MSVC_X360_REGALLOC.md`'s "declaration order controls stack slots" does **not** reach this
case. The stack layout here is decided by *callee-induced register pressure*, and the only
lever on it is the callee's body.

⚠ **An inference of mine that collapsed, recorded because it was load-bearing when I made it.**
I justified the definition-order probe by noting all three callees precede `ScoreSinger` in
retail's `.text` (`0x826F1EC8` < `0x826F26A0` < `0x826F2E58` < `0x826F3658`) while all three
follow it in our source. That is **void**: retail `.text` order is essentially uncorrelated
with our source order — `SetDifficultyVariables` is our line 26 at `0x826F1770`, `PostLoad`
is line 48 at `0x826F3990`. The linker reorders COMDATs. **Callee addresses say nothing about
source order**, and nobody should re-derive that argument.

## 5. The retail `GetNoteRange`, and why it must NOT be pinned

Recovered from `fn_826F1EC8` **+ `fn_826F1F30`**, which are one function that dtk
**OVER-CARVED** into two rows: the first is 104 B, ends on a *conditional* `bgelr` with no
return, and runs contiguously into the second (`0xdb4 + 0x68 == 0xe1c`).

⛔ **Do not pin `fn_826F1EC8` as `?GetNoteRange@VocalPart@@QAAXMAAH0@Z`.** It is a **phantom
extent**, not a whole function — the byte-geometry check the campaign notes prescribe is what
caught it, and it is the reason this lane planted no name. Identification is *not* the
blocker here; carve quality is.

Member offsets taken from the compiler (`class_layout_report.py`), not header comments:
`0x8 mVocalNoteList`, `0x5c unk58` (⚠ the header comment spells it `unk58` but the compiler
places it at **0x5c** — referencing it *by name* is safe regardless), `0x68 mSlop`;
`VocalNote::mMs` at `0xc`, `mDurationMs` at `0x14` (so `f13 = mMs + mDurationMs` is `EndMs()`).

```c
startOut = -1; endOut = -1;
int i = unk58;                                    // cached search hint
if (i > 0) { float lower = ms - mSlop;
             while (notes[i].mMs > lower) { if (--i <= 0) break; } }
for (; i < count; i++) {                          // bgelr guard + bottom-tested loop
    if (notes[i].mMs > ms + mSlop) return;        // bgtlr
    if (notes[i].mMs + notes[i].mDurationMs >= ms - mSlop) {
        if (startOut == -1) startOut = i;
        endOut = i + 1;
    }
}
```

This is a genuine **accuracy** fix independent of the metric: `GetNoteRange` is unpaired and
scores nothing either way, and our previous body was a different algorithm (binary search from
scratch vs resume-from-hint). Its 472 B of value is entirely indirect.

## 6. Measurement

Pre-registered **before** measuring: `Δmatched=+1`, `Δcode_bytes=+472`, `Δcode%=+0.004606pp`,
`Δunits at 100%=0`. Named failure modes: one of the other **four** `GetNoteRange` callers
(lines 200/520/669/1018) regresses; settling noise; refusal.

`tools/ab_measure.py --from-dirty`, `kinds=['source']`, both legs settled to zero work:

```
leg A: matched=43958 masked=23224 honest=20734 code%=40.260880  (recompiles 0, settled)
leg B: matched=43959 masked=23224 honest=20735 code%=40.265488  (recompiles 1, settle iters 2)
Δmatched=+1  Δmasked_equal=+0  Δhonest=+1  Δcode%=+0.004608pp  Δcode_bytes=+472
Δfuzzy=+0.000405pp   units at 100%: 189 -> 189 (0 reached, 0 fell off)
unit improvements: +1  default/VocalPart (30->31)
unit net (ALL units) = +1  ==  whole-binary Δmatched = +1
```

**Prediction hit** (code% differs in the 6th decimal: my arithmetic, not the tool's).
`Δmasked_equal=+0` with `Δhonest=+1` ⇒ real honest matching, not a funclet/masked artifact.
`unit net == whole-binary Δ` ⇒ the other four callers netted **exactly zero**; no hidden
regression paid for this.

## 7. An instrument failure I caused, and the rule that caught it

I diffed my worktree's `report.json` against **main's live `report.json`** and got a confident
`-184 B` with a `TrainerGemTab::Draw` regression — in a unit my patch cannot reach. **Main had
moved under me**: it read `43958 / 4125560` at the start of this lane and `43959 / 4126216`
an hour later, because another agent was building in the shared tree.

⇒ **Main's build outputs are not a baseline.** They are a concurrently-mutating artifact, and
the resulting comparison is shaped exactly like a real regression in an unrelated unit. This
is the same disease as W16-EO's torn read during a running `ab_measure`, with a different
cause: the fix is that **both legs must be measured in your own worktree**, which is precisely
what `ab_measure` enforces and why there is deliberately no `--baseline` flag.

⚠ Related self-correction: I briefly believed `CouldScoreAgainstPart` improved 97.08 → 100.
That 97.08 was read off the **probe-3 stub** build, not the baseline. Its true baseline value
is unknown to me and no claim is made about it.

## 8. What I did NOT do

- **I planted no name for `?Poll@VocalPart@@`** (the optional bonus). I found something more
  useful than a guess: this unit demonstrably contains **over-carved dtk rows**, so any pin
  here needs a byte-geometry check first. Naming under `name_check` is a bet that converts a
  forgiven placeholder site into a checked one; I had no retail-byte identification to the
  standard W16-EO used, so I made none.
- **I did not pin `GetNoteRange`**, for the over-carve reason in §5, even though it *is*
  identified on retail bytes. The identification is recorded here for whoever fixes the carve.
- **I did not touch the `kInvalidPitch__11VocalPlayer` shim.** W16-EO kept it deliberately;
  it is a considered refusal, not an oversight.
- **I did not chase the unit's other near-misses** — `GetBestHit` (528 B, 96.97),
  `GetNoteSliceWeight` (484 B, 88.12), `FramePhraseMeterFrac` (136 B, 57.65),
  `IsEmptyPhrase` (116 B, 65.24), `InTambourinePhrase` (44 B, 18.00) are all still open, and
  `GetBestHit` is now the best remaining target in the file.
- **I inherited no figure I did not measure in-run**, and re-derived no ceiling.

## 9. Handoff

1. **The callee-clobber lever is general and nobody has swept for it.** Any caller whose
   diff shows stack-slot shifts plus "register swaps" concentrated in ONE call's argument
   setup, with `base_size > target_size` by a couple of instructions, is a candidate — and
   the fix is in the *callee*, not the row you are looking at. The cheap discriminator is the
   `__declspec(noinline)` non-clobbering stub probe in §3: it separates "our body is heavy"
   from "our body is wrong" in one build.
2. **`fn_826F1EC8` + `fn_826F1F30` = `VocalPart::GetNoteRange`**, identified on retail bytes,
   currently unpinnable due to the over-carve. Fixing that carve would make it pairable.
3. `?GetBestHit@VocalPart@@…` — 528 B at fuzzy 96.9697 — is the largest remaining near-miss
   in this unit and is itself a `GetNoteRange` sibling.
