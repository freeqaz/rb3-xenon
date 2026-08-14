# `SOURCE_INSDEL` 7–15 charge band — sized, worked, and re-characterised (lane INSDEL-5, 2026-08-14)

Tree `b3e65016` (INSDEL-4 merged) + this lane. Ruler `functionRelocDiffs=name_check`,
read from `report.json` `provenance.diff_config` (`tool_commit 6bf7ba700ce5`,
`tool_binary_hash 69e6f63d2b725e33`) and re-confirmed on every per-row reading.

Baseline (leg A, settled): `matched_functions` **44,422** · `masked_equal` **22,897**
⇒ honest **21,525** · `matched_code` **3,730,660 B** · `matched_code_percent`
**36.147480** · `total_code` **10,320,664** (read from the key, not inherited).

## THE TWO NUMBERS THE BRIEF ASKED FOR

### 1. The 7–15 band is **160 rows / 45,192 B** — 4.1× the 4–6 band

| charge band | rows | bytes | avg | worked by |
|---|---:|---:|---:|---|
| ≤3 | 45 | 12,436 | 276 | INSDEL-1/2/3 |
| 4–6 | 54 | 10,912 | 202 | INSDEL-4 |
| **7–15** | **160** | **45,192** | **282** | **this lane** |
| 16+ | 545 | 378,184 | 693 | untouched |

⚠ **Staleness check, run rather than assumed: 0 of the 160 rows had closed** since
INSTR-1's census, joined against a `report.json` built fresh in this worktree.
0 rows were missing from the report either.

⚠ **A discrepancy with INSDEL-4's table I could not reconcile and am flagging
rather than papering over:** recomputing the ≤3 band from the same census column
(`billed`) gives **12,436 B**, where INSDEL-4's table says **3,652 B**. Its 4–6
figure (54 rows / 10,912 B) reproduces from that column **exactly**, so the method
is right and the ≤3 cell is the odd one out — most likely a post-closure remainder
mislabelled as the band total. It does not affect any conclusion here, but the
≤3 comparison below should be treated as approximate.

### 2. Control availability: **9 of 13 rows triaged (69%)** — it went UP, sharply

| lane | band | control existed for | rate |
|---|---|---|---|
| INSDEL-3 | ≤3 | 4 of 4 opened | 100% |
| INSDEL-4 | 4–6 | 3 of 19 triaged | **16%** |
| **INSDEL-5** | **7–15** | **9 of 13 triaged** | **69%** |

**This reverses INSDEL-4's projection.** It expected "well under 16%" at this
width, reasoning that more charges ⇒ more ways to draw an unclosable one. That
reasoning is sound *per row* and is exactly why the number moved the other way:

★★★ **At 7–15 charges the wins are SHARED-CAUSE FAMILIES, not individual rows —
and a family supplies its own control.** Both closures here were families, and in
each the control was *internal to the population*: a sibling that already matches
at 100% while carrying the rival source shape. Triage cost collapses because one
reading adjudicates 4–7 rows at once.

⇒ **The right unit of work at this width is the UNIT, not the ROW.** Measured on
the band: **75 rows / 18,848 B (41% of band bytes) live in 29 units holding more
than one row.** Rank candidates by multi-row unit, not by row size.

| top multi-row units | bytes | rows |
|---|---:|---:|
| `default/CameraShot` | 3,104 | 4 |
| `default/BandCharacter` | 2,336 | 3 |
| `default/Text` | 1,264 | 2 |
| `default/MemTracker` | **1,188** | **5** ← drained by this lane |
| `default/MoveMgr` | 956 | 3 |
| `default/MeshAnim` | 836 | 6 |

## Result — +11 functions / +1,804 B, both predictions pre-registered

| measure | leg A | leg B | Δ |
|---|---:|---:|---:|
| `matched_functions` | 44,422 | 44,433 | **+11** |
| `masked_equal` | 22,897 | 22,897 | +0 |
| honest | 21,525 | 21,536 | **+11** |
| `matched_code` | 3,730,660 | 3,732,464 | **+1,804 B** |
| `matched_code_percent` | 36.147480 | 36.164960 | **+0.017480 pp** |

**0 regressions. 0 units fell off 100% on EITHER ruler, in both A/Bs.** Native
gate **PASS 18/18, 0 SKIPs, rc=0**.

## Closure 1 — `MemDiffEntry::operator<`: one line, +7 functions / +1,804 B

All five `MemTracker` rows in the band are STL sort/heap templates instantiated on
the **file-local** `MemDiffEntry`, and every charge in every row sits in ONE
contiguous cluster: the inlined comparator.

Our source:

```cpp
if (mHeap != other.mHeap) return mHeap < other.mHeap;
return mSizeDiff < other.mSizeDiff;      // ← wrong
```

Retail emits **two different comparison idioms** for these two returns:

| | idiom | decodes to |
|---|---|---|
| `mHeap` branch | `subfc/eqv/srwi/addze/clrlwi` | strict `<` |
| second branch | `srwi/srawi/subfc/adde` | **non-strict, reversed** |

Because we spell both the same way, MSVC shares one tail between them; retail
duplicates it.

★ **The control is INSIDE the row, and it is the strongest form available: the
`mHeap` half already matched us at 100%.** That pins "our `<` spelling → the eqv
idiom", so a `<` spelling *provably cannot* also produce retail's second idiom.
No oracle involved — and none could have helped, since `src/system` is a verbatim
DC3 copy and DC3 is newer.

★ **Derived twice under opposite operand placement**, which is what makes it
safe rather than a plausible story: `__push_heap` holds `this` in memory and the
comparand in registers, `__unguarded_linear_insert` the reverse. Both give
`mSizeDiff >= other.mSizeDiff`. Retail sorts descending by size within a heap;
`>=` rather than `>` is retail's own sloppiness, and it is what the bytes say.

★ Corroborating **size inequality**: retail is consistently 4–8 B **bigger** in
every affected row (156/148, 212/204, 284/280) — the direction that excludes
"retail inlined something we did not". And `mulli r11,r11,72` matched throughout,
so `sizeof == 72` was never the defect.

★★ **A held-back control population existed by construction and it verified
perfectly:** all **7** `MemDiffEntry` templates that inline `operator<` were
broken and all 7 crossed; the 9 that do **not** inline it (`__introsort_loop`,
`__insertion_sort`, `__final_insertion_sort`, `__pop_heap`, `__pop_heap_aux`,
`__make_heap`, `sort_heap`, `sort`, `__unguarded_insertion_sort_aux`) were
already at 100% and **stayed** there.

### ⚠ The prediction MISSED, and the miss is the most reusable finding here

Pre-registered **+5 / +1,188 B**; measured **+7 / +1,804 B**.

| row | bytes | was | in band? |
|---|---:|---:|---|
| `__median` | 352 | 55.625 | **NO** |
| `__adjust_heap` | 308 | 83.052 | yes |
| `__linear_insert` | 284 | 81.620 | yes |
| `__unguarded_partition` | 264 | 69.167 | **NO** |
| `__partial_sort` | 228 | 78.860 | yes |
| `__push_heap` | 212 | 80.755 | yes |
| `__unguarded_linear_insert` | 156 | 68.718 | yes |

⇒ **616 B — 34% of the yield — landed OUTSIDE the band being worked**, because
`__median` (55.6% fuzzy) and `__unguarded_partition` (69.2%) inline the same
comparator but carry more charges.

★★★ **A charge-band census systematically UNDER-COUNTS shared-cause families:
the band boundary cuts through the family, not around it.** Every per-band ROI
figure in this lane series is therefore a floor, and the 16+ tail is not a
separate population from 7–15 — it holds the *other half* of the same families.

## Closure 2 — 4 `TourProgress` accessors: +4 functions and **exactly +0 bytes**

`GetTourName`, `GetTourWelcome`, `GetTourLeaderboardGoal`, `GetNextCity` each
carried `if (!pTourDesc) return gNullStr;`. **RB3-360 retail has no such guard.**
Our source faithfully ports the rb3-Wii **DEV** oracle, which does have it — the
oracle right about intent, wrong about retail.

Controls, all read before editing:

1. **In-source notes from a prior lane**, which had already removed this identical
   guard from three siblings in the same file and recorded the retail-byte
   verification (`GetNumTotalGigs` L248, `DoesTourHaveLeaderboard` L241,
   `GetTotalStarsForTour` L317). These four are the ones it did not reach.
2. **Size inequality, exact in all four**: retail 72/72/72/88 B vs our
   92/92/92/108 — smaller by exactly 20 B = the 5 guard instructions
   (`bne` / `lis`+`lwz gNullStr` / `bl Symbol` ctor / `b`). Retail **smaller** ⇒
   the guard is **absent**, not inlined.
3. **A 100%-structured sibling carrying the rival shape**: `AreAllTourGigsComplete`
   inlines the already-guard-free `GetNumTotalGigs`, runs straight
   `GetTourDesc → GetNumGigs` with no `cmplwi`/branch, at `target_size ==
   base_size == 84` with zero structural charges.

### ★★★ This row set is the RESIDUAL-1 pattern, and it was priced correctly IN ADVANCE

All four land at **`mpn` 100.0 / fuzzy ≈99.72**, and `matched_code` keys on
`fuzzy == 100`. The pre-registered prediction was **+4 functions, +0 bytes**, and
that is exactly what was measured.

The residual charge in every row is a single **ICF fold-alias**: retail's callee
resolves to `?GetAward@AccomplishmentManager@@` (pinned `0x8235cc80`) where we
spell `Tour::GetTourDesc`, which has no map entry. **Signature adjudication on
retail bytes CONFIRMS our source** — retail passes `TheTour` as `this` and feeds
the result straight into `bl ?GetNumGigs@TourDesc@@`, so it is a `TourDesc*`, not
an `Award*`. The map name is one arbitrary member of a fold group whose body
("hash_map at `this+0x80`; find; return `second` or NULL") is shared across
**52 call sites in 12 unrelated units**.

★ **The `none`-ruler control MEASURES the tax exactly: Δ+304 B on `none` vs +0 on
the shipped `name_check`.** The bytes are real and are withheld *solely* by the
relocation-name charge. Collecting them means installing a `SymbolEquivalences`
alias — forbidden without first proving the fold by relocation-normalized body
hashing, and deliberately **not** bundled here.

⇒ **A row's charge count does not predict its byte collectability.** Budget the
16+ tail knowing that some structurally-perfect closures pay 0 bytes.

## Declined — `CamShot::Copy` (676 B), and the in-source note is why

The charges decode cleanly: retail makes **four** member-assign calls in the
`COPY_MEMBER` run, we make **three**. Frame decoding gives `r30 == this + 416`,
so every source offset maps 1:1 to a dest slot, and retail's order is
`mHideList(88) → mGenHideList(148) → <168> → mShowList(128)` — exactly our source
order. The member at 168 is copied by a `vector`-shaped `operator=`, and our
source has `COPY_MEMBER(mGenHideVector)` **guarded out by `#ifdef HX_NATIVE`** at
precisely that position. Size arithmetic reconciles exactly: retail 676 vs our
672 = missing 3 instrs (12 B) − surplus 2 instrs (8 B).

⛔ **Declined anyway, because the control that exists points AGAINST the edit.**
`CameraShot.h:280`:

> `// NB(rb3-xenon): DC3-only std::vector mirror used by HamCamShot in the native`
> `// build. Retail RB3 has no HamCamShot, so this field is absent in the matching`
> `// layout — guarded out to keep CamShot size correct.`

Three reasons not to overrule it on this lane's budget:

1. **Blast radius is class-wide, not row-local.** `sizeof(std::vector)` is 12, so
   un-guarding shifts `mParentDir` and every member above it by 12 B across every
   function touching `CamShot` — the `volatile` trap at scale (that one closed its
   row at +116 B and knocked a sibling off 100 for **net −60 B**).
2. **Not every charge is accounted for.** A surplus copy of offset **64** remains
   unexplained by the `mGenHideVector` hypothesis, and one unclosable charge
   withholds the whole row.
3. **The prize is probably 0 bytes anyway.** Retail's call is
   `vector<int,StlNodeAlloc<int>>::operator=` where we would emit
   `vector<RndDrawable*,…>` — an ICF fold survivor (identical bodies for any
   4-byte element type). Unless `icf_aliases.map` already carries that
   equivalence, the row stops at `fuzzy < 100` exactly like `TourProgress`.

⇒ **Recorded as a genuine open contradiction, not as noise: the note asserts the
field is absent from retail, and retail's own bytes show a member copy at 168
between `mGenHideList` and `mShowList`.** Settling it needs a layout-wide A/B
with an AT_100 set-diff over all of `CameraShot`/`HamCamShot`, which is a lane of
its own. **Do not "fix" it cheaply.**

## Is 7–15 worth funding further? YES — and it is the best band so far

| | ≤3 | 4–6 | **7–15** |
|---|---:|---:|---:|
| rows closed | 4 | 3 | **11** |
| bytes | +316 | +576 | **+1,804** |
| bytes per closed row | 79 | 192 | **164** (258 counting only byte-paying rows) |
| rows triaged per closure | ~1.0 | 6.3 | **1.2** |
| control availability | 100% | 16% | **69%** |
| band total | 12,436 B | 10,912 B | **45,192 B** |

**7–15 pays 3.1× the 4–6 band's bytes at roughly one-fifth the triage cost per
closure.** The mechanism is not that the rows are easier — individually they are
harder — it is that **at this width rows cluster into families, and a family is
adjudicated once and closed wholesale.**

⚠ Do not over-read the triage-per-closure figure: it is low *because* families
close in bulk. A lane that picks singleton rows here will see 4–6's economics or
worse. **The targeting rule is: multi-row units first.**

### Recommended next, in order

1. **The rest of the multi-row units in this band** — 70 rows / 17,660 B still
   live across 28 units, led by `BandCharacter` (2,336 B / 3), `Text`
   (1,264 B / 2), `MoveMgr` (956 B / 3), `MeshAnim` (836 B / 6).
2. **The 16+ tail is now worth funding, but re-scope it as family work, not row
   work.** It is 545 rows / 378,184 B, and this lane proved the band boundary cuts
   through families — `__median` and `__unguarded_partition` sat outside 7–15 and
   fell to the same one-line edit. **Census by unit across ALL bands before
   opening anything.**
3. `CamShot::Copy` + the `CameraShot` layout question above — as a dedicated
   layout lane with a whole-class AT_100 set-diff.
4. **Not** the fold-alias lever until the folds are proven by relocation-normalized
   body hashing.

## Tooling

`~/tmp/insdel5_show.py` (per-row charged-instruction dump at the shipped ruler,
inherited from INSDEL-4 with `WT` repointed) and `~/tmp/insdel5_join.py`
(census → fresh-`report.json` join, emitting `~/tmp/insdel5_live.tsv`).
Per-row findings are recorded as **in-source notes** at `MemTracker.cpp` and
`TourProgress.cpp`.
