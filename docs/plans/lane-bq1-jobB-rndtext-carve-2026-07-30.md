# Lane BQ-1 · Job B — carving RndText's three out-of-TU bodies home

Date: 2026-07-30 · branch `laneBQ1` · worktree `~/tmp/wt-bq1`

A-leg for this job is Job A's landed state: **matched 40886 / masked_equal 1509 /
honest 39377**.

| | predicted | measured |
|---|---|---|
| matched | +1 | **+1** (40887) |
| masked_equal | 0 | **0** (1509) |
| honest | +1 | **+1** (39378) |

Cumulative for the lane so far: **+7 matched / +7 honest** vs the 40880 baseline.

---

## 1. What was blocked, and by what

Lane BP-8 (landed `1b7089e2`) measured three RndText bodies that retail hosts
*inside other units' pinned spans*, and parked them in
`add_blocked_on_splits_surgery` because BP-8 was forbidden to touch `splits.txt`:

| VA | identity | hosted by |
|---|---|---|
| `0x82455928` | `?Save@RndText@@` | `UIFontImporter.cpp` span `[0x82455928,0x82455A70)` |
| `0x82457790` | `??1RndText@@` | `SkeletonClip.cpp` span `[0x82457680,0x82457D18)` |
| `0x82457ea0` | `??_GRndText@@` | `Lit_NG.cpp` span `[0x82457EA0,0x82457EF0)` |

## 2. Independent corroboration before carving (I did not take BP-8 on trust)

`0x82457ea0` is 80 bytes and its only two `bl` targets are **`0x82457790`** and
`0x827bc430` (`operator delete`). That is exactly the `??_G` scalar-deleting
destructor shape — call the destructor, then conditionally free. So one
disassembly confirms *both* rows at once: `0x82457ea0` is a `??_G`, and the thing
it destroys through is the `??1` at `0x82457790`. This is independent of BP-8's
method.

Pre-build byte checks against our `Text.obj` (branch-masked) set the prediction:

| VA | retail | ours | verdict |
|---|---|---|---|
| `0x82457ea0` `??_G` | 80 B | 80 B | **masked-equal ⇒ predict 100%** |
| `0x82455928` `Save` | 328 B | 352 B | size-divergent ⇒ predict <100% |
| `0x82457790` `??1` | 108 B | 608 B | badly size-divergent ⇒ predict ~0% |

## 3. The phantom-unit warning, discharged

The brief flagged that `SkeletonClip` is one of BP-7's confirmed phantom classes,
so a splits unit *named* for it is itself suspect. Checked before cutting:
`SkeletonClip.cpp`'s spans in this neighbourhood are **earning 7 matches at
100%**, all `vector<RecordedFrame>` STL instantiations. Whatever the unit's name
is worth, that content is real enough to be worth not disturbing — so I split its
span around the RndText body rather than re-attributing the unit:

```
0x82457680–0x82457D18   ->   0x82457680–0x82457790   (kept by SkeletonClip)
                             0x82457800–0x82457D18   (kept by SkeletonClip)
                             0x82457790–0x82457800   (moved to Text.cpp)
```

All 7 SkeletonClip matches survived the split — verified after the build.

**Filed, not acted on:** those 7 are a strong candidate for the same false-100
class BP-8 drained for `vector<BlacklightPacket>` and `vector<Style>` —
`RecordedFrame` is Kinect gesture-recording state with no plausible RB3 retail
existence, and tiny STL template bodies are the canonical byte-collision
population. Draining them would cost 7 matched. Out of scope here; it needs the
same literal/xref adjudication BP-8 applied, not a name-based guess.

`UIFontImporter.cpp` (7 blocks remain) and `Lit_NG.cpp` (2 blocks remain) each
earned **zero** in the region, so those two moves cost nothing. Neither unit was
drained of its last `.text` block, so no unit entry had to be deleted.

## 4. Result, including the two that did not reach 100%

- `??_GRndText@@` — **100%**, the predicted +1.
- `?Save@RndText@@` — **80.59%**. Predicted only "<100%", so this is better than
  expected, and it is worth recording as evidence rather than as a number: an
  80% body agreement across a 328-byte function is not a byte coincidence, so it
  independently corroborates BP-2b/BP-8's identification of this VA as
  `RndText::Save` (which BP-7 Part F had already used, from the other direction,
  to free `?Save@UIFontImporter@@` for `0x828182A8`).
- `??1RndText@@` — **4.81%**. Paired and correctly attributed, but our
  destructor is 608 B against retail's 108 B: we inline member destructors
  (`vector<Line>`, `String`) that retail out-lines. A body-port target, filed.

Both sub-100 rows are honest partial credit that did not exist before — they were
previously unpinned-or-misattributed and contributed nothing.

`.pdata` was not hand-edited anywhere; it is derived output and was re-derived by
the split run (which is also why `splits.txt` line numbers shift between builds —
re-locate before editing).
