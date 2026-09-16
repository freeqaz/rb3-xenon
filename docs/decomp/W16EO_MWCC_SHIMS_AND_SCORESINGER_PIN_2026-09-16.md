# W16-EO — the three MWCC shims in VocalPart.cpp, and why the census is the result

Lane W16-EO, 2026-09-16, worktree `~/tmp/wt-w16-eo`, branch `w16-eo`, off main `2d366ad2`.
Ruler: shipped graded `name_check`, read from `build/45410914/report.json` (not assumed).
Baseline at leg A: `matched=43958 masked_equal=23224 honest=20734 code%=40.260880`.

## Result in one line

The shim repair is **correct and measures exactly Δ0** — pre-registered as Δ0, measured as
Δ0 — because **both live call sites are inside an UNPAIRED function**. The lane's actual
product is two things the brief did not ask for: a **tree-wide census proving this vein is
empty** (this was the last file), and the **identification of `fn_826F3658` as
`VocalPart::ScoreSinger`**, which is what made the shim fix worthless and is the only thing
that can make it worth anything.

## 1. The census — the vein is EMPTY, and a naive regex says otherwise

The brief warned that a `--include=*.cpp` glob had failed vacuously under zsh. It had; but
the *opposite* error is the one that nearly bit here. A permissive MWCC regex
(`name__<N><Class>`) over `src/` + `native/` returns **239 hits**. 230 of them are Ogg Vorbis
codebook tables — `_vq_quantlist__8u0__p1_0`, `_vq_lengthlist__16c0_s_p1_0`. These are not
killed by length-validating N against the class name, because `__8` really is followed by
8 characters (`u0__p1_0`). They are killed only by recognising them as a known data family.

After filtering, **9 validated hits survive tree-wide, and they are exhaustively:**

| file | line | symbol |
|---|---|---|
| `src/band3/game/VocalPart.cpp` | 629, 817 | `kInvalidPitch__11VocalPlayer` (decl + use) |
| `src/band3/game/VocalPart.cpp` | 630, 818 | `NoteAt__13VocalNoteListCFf` (decl + use) |
| `src/band3/game/VocalPart.cpp` | 631, 689 | `PitchAt__13VocalNoteListCFf` (decl + use) |
| `native/src/m10_support.cpp` | 38, 41, 44 | all three, **definitions** |

⇒ **This is NOT a repeatable vein.** `VocalGuidePitch.cpp` was the only sibling and W16-EJ
already cleaned it. There is no third file. A lane briefed to "find more of these" should be
told the answer is zero and not re-run the sweep.

## 2. The three shims do NOT have the same answer

| shim | real C++ symbol? | compiled in match build? | retail's target symbol | verdict |
|---|---|---|---|---|
| `NoteAt__13VocalNoteListCFf` | **YES** — `VocalNoteList::NoteAt(float) const`, decl `beatmatch/VocalNote.h:115`, def `VocalNoteList.cpp:569` | yes (in `ScoreSinger`) | **named**: `?NoteAt@VocalNoteList@@QBAPBVVocalNote@@M@Z` @ `0x82781078` | FIXED |
| `PitchAt__13VocalNoteListCFf` | **YES** — decl `VocalNote.h:116`, def `VocalNoteList.cpp:581` | **NO** — inside `#ifdef HX_NATIVE` | n/a | FIXED (inert) |
| `kInvalidPitch__11VocalPlayer` | **NO** — nowhere in `src/`, and the rb3-Wii oracle carries the *same shim* (`rb3/src/band3/game/VocalPart.cpp:625,815`) | yes (in `ScoreSinger`) | **placeholder**: `lbl_820F14B4` | **KEPT** |

Two non-obvious findings behind that table:

**(a) `PitchAt` was never in the object at all.** The pre-fix `VocalPart.obj` had
`NoteAt__13VocalNoteListCFf` and `kInvalidPitch__11VocalPlayer` as undefined externals
(`cls=2 sec=0`) but **no `PitchAt__` symbol whatsoever** — MSVC emits nothing for an unused
`extern` declaration. That is a *non-metric* proof that `HX_NATIVE` is dead here, independent
of any percentage. (`command grep -c ' /DHX_NATIVE' build.ninja` = 0.)

**(b) `kInvalidPitch` is metric-free even if the row were paired.** Retail does not fold it
to a literal — `0x447A0000` (1000.0f) appears **nowhere** in `VocalPart.s` or `VocalPlayer.s`.
It loads a real external float global:

```
lis  r10, lbl_820F14B4@ha
lfs  f0,  lbl_820F14B4@l(r10)
stfs f0,  0x0(r27)            ; r27 = o_rPitchDiff, from lwz r27, 0x13c(r1)
```

`lbl_820F14B4` is a **placeholder** name, and objdiff's `name_check` forgives placeholder
target names. So the wrong spelling on our side costs **zero charges**, paired or not.
Inventing a `VocalPlayer::kInvalidPitch` declaration would be a **fabricated name** for no
measurable gain — the alias-fabrication hazard class. It is deliberately kept, with a comment
in the source saying why.

## 3. Why the fix measures Δ0: `ScoreSinger` is UNPAIRED

`VocalPart` has 70 target rows; **34** are map-named. `?ScoreSinger@VocalPart@@` is **not**
one of them, and neither is `?Poll@VocalPart@@`. Our object *does* define
`?ScoreSinger@VocalPart@@QAAXMMMMHPAVTalkyMatcher@@AAVVocalScoreCache@@AAHAAM@Z`
(`cls=2 sec=689`), but objdiff pairs by name, so with no counterpart the row is never
compared. **An unpaired row is invisible: it looks identical to a row with nothing wrong.**

That is the whole explanation of the Δ0, and it is also why W16-EJ's identical fix *did* pay
+572 B: `?Poll@VocalGuidePitch@@QAAXM@Z` **is** map-named (`0x826c91c8`), so its charge was
live.

## 4. `fn_826F3658` IS `VocalPart::ScoreSinger` — identified on retail bytes

Two independent lines of evidence.

**Body.** Reading the retail asm against our source, every argument slot and every store
offset agrees (X360 ABI: `r3`=this, `f1`=ms, `f2..f4`=arg1..3, `r8`=arg4, `r9`=TalkyMatcher,
`r10`=o_rCache, stack `0x134`=o_rNote, `0x13c`=o_rPitchDiff):

| retail | our source |
|---|---|
| `lfs f0,0x38(r3)` / `lfs f13,0x3c(r3)` / `fsubs` / `fsel` / `stfs f0,0x8(r30)` | `o_rCache.unk8 = Min(unk38, mPhraseScoreMax)` |
| `lwz r27,0x13c(r1)` / `lfs f0,lbl_820F14B4` / `stfs f0,0x0(r27)` | `o_rPitchDiff = kInvalidPitch` |
| `fcmpu cr6,f2,f30` (f30 = `lbl_82000D78`) | `arg1 == 0.0f` |
| `lwz r3,0x8(r3)` / `bl fn_82781078` | `mVocalNoteList->NoteAt(ms)` |
| `lfs f0,lbl_820009FC` / `stfs f0,0x0(r30)` / `stw r29,0x0(r10)` | `o_rCache.unk0 = 1.0f; o_rNote = arg4` |

**Caller.** The only retail call site is `VocalPlayer.s:10863 bl fn_826F3658`, inside
`?Poll@VocalPlayer@@UAAXMABVSongPos@@@Z` (`0x826eb030`) — and our source calls
`pPart->ScoreSinger(...)` from `VocalPlayer.cpp:602`. The caller agrees independently of the
body.

⚠ Method note: I keyed on the `.fn fn_<addr>` symbol, **not** the asm address column, which
renders this function's first instruction as `0x826F3524`. That column is synthetic for
multi-block units; trusting it would have mis-identified the function.

## 5. Two instrument failures I caused, recorded so they are not repeated

**(a) A torn read of build artifacts during a concurrent A/B.** I launched
`ab_measure --from-dirty` in the background and then dumped `VocalPart.obj` symbols. The
assertion FAILED (`NoteAt__` still present, `?NoteAt@VocalNoteList@@` absent, 2339 symbols).
It was not a real result: `ab_measure` reverts the tree to HEAD to build leg A, and my read
landed in that window — confirmed directly by `git diff --stat` coming back **empty**
mid-run. Re-asserted on a quiescent tree afterwards: **all five assertions pass, 2346
symbols.** ⇒ **Never read build outputs while an A/B owns the tree**; the failure is shaped
like a real refutation.

**(b) `pgrep -x ninja` is not a build detector here.** It reported "no" while a build was
running, because the process is `/bin/sh tools/ninja-locked`, not a binary named `ninja`.
`ps -eo pid,etime,cmd` found both it and the `ab_measure` parent.

## 6. Measurements

### Change 1 — the shim fix (source-only, commit `30be4c7d`)

Pre-registered: **Δ0 bytes, Δ0 functions**, because both live sites are in an unpaired
function and the `PitchAt` site is `HX_NATIVE`-dead. Measured:

```
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.000000pp   units at 100%: 189 -> 189 (0 reached, 0 fell off)
leg B recompiles: msvc=1  (so NOT absent-vs-absent — the patch really compiled)
```

**Prediction hit.** Landed on merit per the standing accuracy-over-headline rule.

### Change 2 — pinning `0x826f3658` = `ScoreSinger`


Pre-registered: **+0 B / +0–1 fn, with roughly a 1-in-4 chance of the full +472 B.**
Named failure modes registered in advance: the row pairs but scores <100 (all-or-nothing ⇒
0 bytes); `VocalPlayer::Poll`'s call site flips from forgiven-placeholder to *checked* and
becomes a new charge if our spelling differs; the forced re-split perturbs byte-signature
funclet pairings elsewhere. Measured (`kinds=['map']`, forced re-split, leg B
`renamer_patched=1830`, **both legs at a `symbols.txt` split fixed point**):

```
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.004200pp   (legA 50.006645 -> legB 50.010845)
units at 100%: 189 -> 189 (0 reached, 0 fell off)
[control none] FLAT -- consistent with a pure RE-name; no ALIAS_SUSPECT flag
```

**Landed on the 75% branch.** The `Δfuzzy` movement is the proof the pin is live rather than
inert; the bytes are 0 because `matched_code` keys on `fuzzy == 100` per row.

The row itself:

| leg | row | size | fuzzy | mpn |
|---|---|---|---|---|
| A | `fn_826F3658` | 472 | **0.0** (unpaired — never compared) | 0.0 |
| B | `?ScoreSinger@VocalPart@@…` | 472 | **90.881355** | **91.135590** |

`masked_equal=False`; implied `diff_score = (100−fuzzy)·size/4 = 1076`. So **our source
already reproduces ~91% of a 472 B function that was previously invisible at 0%**, and
because `mpn < 100` there is genuine instruction-level divergence left — this is real source
work, not a name charge. That is the entire value of the pin: it converts a blind spot into
a legible target. It is byte-direction **non-negative by construction**, since the row was
already in `total_code` at 0%.

## 7. What I did NOT do

- **I did not invent a `VocalPlayer::kInvalidPitch` declaration.** It would have required
  adding a static member to `VocalPlayer.h` and a definition to `VocalPlayer.cpp` (perturbing
  a different, paired unit) to produce a *guessed* mangled name — `?kInvalidPitch@VocalPlayer@@2MA`
  vs `…@2MB` cannot be discriminated from available evidence, because retail never folds the
  constant and the target symbol is an unnamed `lbl_`. Zero measurable upside, fabrication risk.
- **I did not close the remaining ~9% of `ScoreSinger`.** It is now visible at fuzzy 90.88 with
  `diff_score` 1076; diagnosing those charges is a follow-up lane, not this one.
- **I did not touch `native/src/m10_support.cpp`.** Its `NoteAt__`/`PitchAt__` definitions are
  now unreferenced-but-defined, which is harmless (a definition needs no caller), while
  `kInvalidPitch__11VocalPlayer` is still referenced by `VocalPart.cpp` and must stay. Deleting
  the two dead shims is optional cleanup with a nonzero link risk and no benefit; I left them.
- **I did not re-derive the reachable ceiling or any figure I inherited** beyond the ones I
  measured in-run.

## 8. Handoff

1. **`VocalPart::ScoreSinger` at fuzzy 90.88 / `diff_score` 1076, 472 B** is the best target
   this file now offers, and it did not exist as a target before this lane. Price it from
   `report.json`'s charged-site list, not from a mismatch count.
2. **`?Poll@VocalPart@@` is still unnamed.** I verified only that it is absent from the 34
   named `VocalPart` rows — I did **not** identify which `fn_` address it is, and this doc
   deliberately names no candidate rather than plant an unverified address someone inherits as
   fact. The technique used in §4 (match argument slots and store offsets against the body,
   then confirm independently from the caller) should transfer to the remaining 35 unnamed
   `VocalPart` rows.
3. **Do not re-run the MWCC shim census.** It is empty: 9 hits, all accounted for above.
