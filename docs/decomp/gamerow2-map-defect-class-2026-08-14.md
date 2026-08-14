# Lane GAMEROW-2 — the map-defect class pays twice more, and three veins closed

**Result: +8,780 B, +1 matched_function, 0 units off 100%, 0 rows regressed.**
Two map repairs, both adjudicated on retail bytes and both measured with a
pre-registered, decomposed A/B.

| commit | row | Δbytes | class |
|---|---|---|---|
| `51df7b5e` | `0x82677490` → `Game::OnRemoteTrackerEndStreak` | **+5,236** | map carried a SpeedTree name on an RB3 address |
| `136a139e` | `0x8259fb20` → `BandSongMetadata::Rank` | **+3,544** | map carried a plausible SIBLING name |

This is GAMEROW-1's class (`3a1af7e3`) continuing to pay, but note the
difference: GAMEROW-1's three were **same qualified name, wrong signature**
(`sigscan`'s `SIG_SAME_QUALNAME`). Both of mine are **a different function
entirely**, which `sigscan` files under `UNRELATED` — the bucket that also
contains every genuine ICF fold. So the tool cannot rank these; only
retail-byte adjudication separates them.

---

## 1. The witnesses that actually decide it

Ranked by strength, from the two rows that landed. **Any one of 1–3 is
sufficient; the call site alone is not.**

1. **Return register (categorical).** `fn_8259FB20` returns in `f1`
   (`lfs f1, 0x8(r11)`; `lfs f1, <0.0f>`). A `bool` function cannot return in
   an FPR. This alone killed the mapped `HasPart(Symbol,bool)`.
2. **Callee body vs our source, statement for statement**, including struct
   offsets checked against our own header (`mRanks` @ `0x9c`, `mSongMgr` @
   `0x134`).
3. **Span ownership.** `0x82677490` lies inside `Game.cpp`'s `.text` span
   (`0x82677410`–`0x82678cfc`). A SpeedTree function cannot live inside RB3's
   `Game.obj`. Cheap, structural, and it needs no disassembly.
4. **The sibling family.** `Game.cpp` defines seven 8-byte `OnRemoteTracker*`
   forwarding thunks; **six were already at fuzzy 100** and the odd one out was
   at 0. The missing family member was exactly the name the row wanted.
5. **Our callee is ABSENT from the map.** True in both landed rows. A good
   *prior*, never a proof.
6. **Arity at the call site.** Useful, but see §4 — it is the witness that
   misled me, because it constrains the CALLER, not the callee.

★ **The strong form is the CALLEE PROLOGUE/BODY, not the call site.** A call
site tells you what the caller *believed*; only the callee tells you what it
*is*.

---

## 2. ALIAS_SUSPECT on a map repair is a patch-KIND alarm, not a verdict

`136a139e` measured `none` **flat at +0** with `name_check` **+3,544**, on a
map-only patch — bit-for-bit the fabricated-alias shape, and `ab_measure` said
so. It is also the shape of the MAPDEF-3 "repair a wrong existing name" class,
which is documented to pay with `none` flat. **The two are separable only by
evidence, and the documented discriminator is: pre-register the exact rows.**

A fabricated alias forgives an **arbitrary** set of sites. A real fix moves the
**mechanism's own** set. Set-diff of the two archived reports — 5 crossed, 0
fell off:

```
+2572  ?Handle@BandSongMgr@@                    BandSongMgr.cpp:1130  ->Rank(...)
 +336  ?NewSongNode@SongSortByStars@@
 +248  ??RSongDifficultyCmp@@QBA_NVSymbol@@0@Z  AccomplishmentManager.cpp:109
 +224  ?NewSongNode@SongSortByDiff@@
 +164  ??RSongRankCmp@@QBA_NHH@Z                BandSongMgr.cpp:145
```

Every one is a confirmed `BandSongMetadata::Rank` caller, and all five are
rank/difficulty consumers. Corroborated **before** the edit existed:
`grep -rl fn_8259FB20` over the split asm returned `SongSortByDiff.s`.

⚠ **I predicted +2,572 and measured +3,544 — an honest miss in magnitude.** I
priced only the row I had opened and never enumerated the callee's other
callers. Direction and mechanism were right, the number was low. **When pricing
a map repair, enumerate the callee's call sites first** — the fix pays across
all of them, not just the row that led you there.

★ A corollary worth more than the bytes: the 236 B row
`?HasPart@BandSongMetadata@@UBA_NVSymbol@@_N@Z` sat at **fuzzy 53.2** — and it
was never a hard function. It was a **false pairing**: our `HasPart` body
diffed against retail's `Rank` body. **A mid-50s row is not always a body
problem. Check the pairing before opening the body.**

---

## 3. `sigscan`'s SIG_SAME_QUALNAME vein is DRAINED tree-wide

Ran `tools/sigscan.py` (`87721225`) over the whole binary on a **built**
worktree (target objs must be post-renamer, or every retail name reads absent):

```
CHARGED pairs both real-named : 2124  (3695 sites)
  SIG_SAME_QUALNAME      4 pairs     7 sites      <- GAMEROW-1's class
  TEMPLATE_ARGS        895 pairs  1876 sites
  UNRELATED           1174 pairs  1744 sites
FULLY-REALISABLE rows: 0 B
```

**4 pairs, 0 fully realisable, and the single candidate is engine-tier, 364 B,
and accounts for only 2 of its row's 4 charged sites.** Do not re-fund
SIG_SAME_QUALNAME. Both of my landed rows are in `UNRELATED`.

The game-tier `TEMPLATE_ARGS` stratum is **274 pairs / 305 rows / 91,556 B** —
the STL fold / wrong-container shape. Left alone deliberately: an unproven
alias lifts the score by construction, and CLAUDE.md's standing directive
forbids editing correct source to satisfy a fold.

---

## 4. DEAD END — TourPerformerImpl. Do not reopen on the call site alone.

`?Handle@TourPerformerImpl@@` (2,196 B, 2 charged sites) looked like a third
instance and is **not resolvable as one**. It is the most useful negative here
because it nearly became a wrong edit.

Site 502: retail stages **r3, r4, r5**, with `r4` straight out of
`__RTDynamicCast` — which reads exactly like our
`UpdateTourPlayerContributionLabel(UILabel*, BandUser*)` (this + 2 pointers),
against the map's 1-arg `?SetDancer@AppLabel@@QAAXVSymbol@@@Z`. `SetDancer`
appears **nowhere in our source tree**, and its address `0x82360fb0` sits inside
**TourPerformer's own span** — three independent signals, all pointing the
same way.

⛔ **Then the callee body refuted it.** `fn_82360FB0` never reads incoming `r5`:

```
mr r29, r4            ; captures arg1 only
mr r4, r3 ; addi r3, r31, 0x50 ; bl fn_82360BF8   ; Symbol t = this->G()
...
lwz r5, 0x58(r31)     ; r5 is WRITTEN from a FIELD OF THE SRET BUFFER
```

`0x58(r31)` is 8 bytes into the `0x50` sret temp — **not a parameter.** The
function takes `this` + 1 arg. So the 3-arg name does not fit, even though the
call site stages three registers. That contradiction is unresolved.

⇒ **`addi/lwz rN` near a call proves nothing about parameter N** — check
whether the register is *read before first write*. And the row could not pay
anyway: its other site (188) is `GetCurrentQuestSuccessMessage` vs
`GetCurrentQuestDisplayName`, same class, **same signature**
(`QBA?AVSymbol@@XZ`), so arity cannot discriminate, and `matched_code` is
all-or-nothing per row.

---

## 5. DEAD END — the SongDB thunk ladder is CIRCULAR. Do not edit it.

`?Handle@Game@@` (5,428 B) and `?Handle@GemPlayer@@` (5,612 B) are each 2 sites,
both naming SongDB accessors, and the call-site evidence says the map is wrong:

- **GemPlayer 307**: the return is immediately dereferenced
  (`lwz r10, 0x4(r3)`, then indexed by 0x44-byte elements). The mapped
  `SetFakeHitGemsInFill` returns **void**. Our `GetGemList` returns a pointer.
- **Game 650**: only `r4` is staged (a `subfe` bool) and the result is
  discarded. The mapped `GetGemListByDiff(int,int)` needs r4 **and** r5.

**But the control fires the other way.** SongDB is a thin wrapper: essentially
every accessor is an 8-byte thunk `lwz r3, 0x4(r3); b <impl>`. Their report rows:

| row | fuzzy | mpn |
|---|---|---|
| `?GetGemListByDiff@SongDB@@` | **100** | 100 |
| `?SetFakeHitGemsInFill@SongDB@@` | 97.5 | 100 |
| `?GetGemList@SongDB@@` | 97.5 | 100 |

`fuzzy == 100` means that thunk's `b`-target **name** already agrees with ours.
A ladder shifted by one **plus an impl ladder shifted by one in the same way**
reproduces exactly this pattern — self-consistently. `mpn == 100` on a 2-
instruction thunk is near-vacuous (it is arg-blind, so any sibling matches).

⇒ **The evidence inside the ladder cannot break the tie.** Resolving it needs an
independent anchor — identifying the *inner* class's methods by their own
bodies. That is a lane of its own; 11,040 B sits behind it. **Do not rotate map
names here on call-site evidence alone.**

---

## 6. ★ CORRECTION — `?Handle@CustomizePanel@@` is UN-RETIRED. The prize is now real.

`CAMPAIGN_STATE_2026-08-14.md` retires this row, and the in-source block at
`src/band3/meta_band/CustomizePanel.cpp` (lane RESIDUAL-1) states:

> CLOSING ALL THREE INSTRUCTIONS BUYS `mpn == 100` (+1 matched_function) AND
> ZERO BYTES … The 5,036 B additionally requires BOTH ICF aliases to be proven.

**That rationale is now STALE — the two aliases were proven and landed in the
interim** by FOLDPROVE-1/2 (`ec8481d5`, `7b57c6a7`). Measured today at
`name_check` on a settled tree, the row has **3 charged sites and ZERO
`diff_arg`**:

```
5036 B  fuzzy 99.762  mpn 99.762   {insert: 2, delete: 1}
```

`fuzzy == mpn` ⇒ **closing the three body instructions now buys the full
5,036 B and +1 function.** It is the largest game-tier row behind a
single-digit site count.

⚠ **What is NOT stale is the diagnosis.** Both defects remain ruled out by
compile-and-read experiments, not by argument, and should not be re-run:

- **(a)** base-only `subi r10,r26,0xb8 / stw r10,0x94(r31)` — a dead home for
  the vbase-adjusted `this` of an **inlined member call** in the
  `in_clothing_state` arm. Retail has this home in **3 of our 4** inlined member
  calls in `Handle`, so retail inlined a bool-returning **non-member** there. No
  plausible source shape was found; `const` and in-class definition were both
  tried and changed nothing.
- **(b)** target-only `clrlwi r11,r11,24` closing the `has_license/has_patch`
  bool tail. A scan of the whole TU's `/FAs` listing finds **zero** occurrences
  of `subfe` followed by `clrlwi ,24` — no construct in the file reproduces it.

⇒ **The row now needs a NEW IDEA, not a re-run.** Retire the "buys zero bytes"
sentence, keep every experiment.

★ The general lesson: **a retirement is only valid on the tree it was measured
on.** RESIDUAL-1's verdict was correct when written and became wrong without
anyone touching the row — another lane closed the aliases underneath it. When
inheriting a "documented negative", re-measure the *price* even when you trust
the *diagnosis*.

---

## 7. Ranking note for the next lane

Priced all band3 rows ≥60 fuzzy at `name_check` by **charged-site count**, not
fuzzy%. The two shapes worth opening:

- **1–2 charged sites, all `diff_arg`, large row.** This is where both of my
  fixes came from. Remaining unadjudicated: `?SelectNode@MusicLibrary@@`
  (2,260 B, 1 site — `RemoveLastSongFromSetlist` vs `PushSetlistToScreen`, both
  `void()` members of the same class, a real fold candidate),
  `?RecordScore@RockCentral@@` (1,932 B), `?Configure@TourDesc@@` (1,608 B —
  `push_back<ChatReceiver*>` vs `<TourDescEntry*>`, i.e. TEMPLATE_ARGS, skip).
- **Few sites, all `insert`/`delete`** — real body divergence. `CustomizePanel`
  above is the big one.

⛔ Not worth opening: `?CountOrCreateExpandedDetails@NextSongPanel@@` (12,220 B)
— GAMEROW-1 already abandoned it at exactly 2 sites, `add r3,r11,r28` vs
`add r3,r28,r11`, with ~40 instances of the same source construct emitting the
matching order. Permuter-class; permuter is OFF by directive.
