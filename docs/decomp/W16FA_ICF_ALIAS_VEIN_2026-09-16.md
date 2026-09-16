# W16-FA — the ICF alias vein: sized whole-binary, and one membership proven on retail bytes

Lane W16-FA, 2026-09-16, worktree `~/tmp/wt-w16-fa`, branch `w16-fa`, based at `f030a74e`.
Ruler: shipped graded `name_check`, read from `build/45410914/report.json`
`provenance.diff_config` (objdiff **4.2.9**, `tool_commit a5f0ea903ec1`) — resolved at
runtime, never hardcoded.

Baseline measured in **this** worktree, settled to zero compile work (the 5 edges that
re-run every pass are `CHECK ICF-ALIAS MAP` / `CHECK SPLIT CURRENT` / `CHECK MAP
NAME-INJECTIVITY` / `CHECK TARGET OBJS RENAMED` / `PROGRESS` — validators and a phony,
not compiles):

```
matched=43981  masked_equal=23224  honest=20757  code=4132660  code%=40.330170
fuzzy=50.014153   total_functions=69240   total_code=10247068
```

That is exactly W16-EZ's post-landing figure (43978 + 3 / 4131656 + 1004), so main moved
by nothing else between the lanes.

---

## 1. PRE-REGISTRATION — written and committed BEFORE any measurement

The change: add **one** membership, `?reserve@?$vector@HV?$StlNodeAlloc@H@stlpmtx_std@@@stlpmtx_std@@QAAXI@Z`
(`std::vector<int>::reserve`), to the existing ICF alias group whose survivor is
`?reserve@?$vector@PAUDep@CharPollableSorter@@…@Z` at **`0x823715e0`** — the same group
W16-EZ added `vector<float>` to.

### Predicted to CROSS — exactly 4 rows, 3,516 B, Δmatched_functions = +4

Derived by applying the new equivalence to every charged pair of every class-E row in
the whole-binary census, not by guessing:

| bytes | fuzzy = mpn | unit | symbol |
|---:|---:|---|---|
| 2496 | 99.98397 | `default/DataArraySongInfo` | `??0DataArraySongInfo@@QAA@PAVDataArray@@0VSymbol@@@Z` |
| 732 | 99.94536 | `default/GuitarController` | `??0GuitarController@@QAA@…@Z` |
| 164 | 99.87805 | `default/SongData` | `?SetUpTrackDifficulties@SongData@@QAAXPAVPlayerTrackConfigList@@@Z` |
| 124 | 99.83871 | `default/Matchmaker` | `??0QuickFinding@@QAA@XZ` |

All four have `mpn == fuzzy` and their ONLY charges are this pair, so **both** headline
measures must move: **Δmatched = +4, Δcode_bytes = +3,516**.

### ★ Pre-registered WOULD-BE FALSE POSITIVES — these must NOT cross

If the membership were forgiving *wrongness* rather than *folding*, these are the rows
that would be wrongly paid. Each keeps ≥1 charge from a **different** fold group, and
each loses 3 of its 4 charges to my membership, so each must **improve and NOT cross**,
banking **0 bytes**:

| bytes | row | residual charge that MUST survive |
|---:|---|---|
| 276 | `??0PlayerTrackConfigList@@QAA@H@Z` | `vector<PracticeSection>::reserve` vs our `vector<PlayerTrackConfig>::reserve` |
| 276 | `?Process@PlayerTrackConfigList@@…@Z` | `vector<Symbol>::operator=` vs our `vector<TrackType>::operator=` |
| 180 | `??0HitTracker@@QAA@XZ` | `_M_fill_insert<vector<Object*>>` vs our `_M_fill_insert<vector<int>>` |
| 384 | `?ClearQuarantinedPhrases@SongDB@@QAAXH@Z` | `vector<char>::reserve` vs our `vector<unsigned char>::reserve` |
| 3388 | `?Poll@VocalPlayer@@UAAXMABVSongPos@@@Z` | 165 instruction-level charges + 8 other name pairs |

**`??0PlayerTrackConfigList@@QAA@H@Z` is the sharpest of these and is the row to watch.**
It is a strictly better control than W16-EZ's because the discrimination happens **inside
one function**: its five `reserve` sites (here `idx == body_offset/4` exactly) are

| idx | our spelling | retail target | state |
|---:|---|---|---|
| 53 | `vector<int>` | `0x823715e0` survivor | charged → my membership forgives |
| 56 | `vector<int>` | `0x823715e0` survivor | charged → my membership forgives |
| **59** | **`vector<TrackType>`** | **`0x827a36f0` TWIN** | uncharged (target name is the placeholder `fn_827A36F0`) |
| 62 | `vector<int>` | `0x823715e0` survivor | charged → my membership forgives |
| 65 | `vector<PlayerTrackConfig>` | — | charged, **different** fold group |

A membership that forgave the family indiscriminately would have to swallow idx 59 too.
It cannot: I am not aliasing `vector<TrackType>`, and idx 59 is not charged in the first
place.

### Named ways this can fail

1. Any of the 4 predicted rows fails to cross ⇒ my pair-set model is wrong.
2. **Any pre-registered false positive crosses** ⇒ STOP, revert, the membership is
   forgiving wrongness. In particular a "bonus" beyond +3,516 B is an **alarm, not a
   prize** — that is W16-EZ's rule and it is inherited here.
3. Δmatched ≠ +4 while Δbytes = +3,516 (or vice versa) ⇒ the mpn/fuzzy split is not what
   the census reports and the pricing must be re-derived.
4. `ALIAS_SUSPECT` will fire (map-only patch). That is **expected and is not evidence
   either way**: the defence is the retail-byte proof in §3, never the `none` control,
   which is flat for a fabricated alias by construction.

---

## 2. The vein, sized whole-binary (the lane's other deliverable)

Measured in this worktree, not briefed.

| population | rows | bytes |
|---|---:|---:|
| named partial rows (0 < fuzzy < 100) | 2,596 | 917,720 |
| **class E — ALL charges are relocation-NAME charges** | **1,423** | **318,740** |
| class F — mixed (a name charge AND something else) | 442 | 262,016 |
| no name charge at all | 731 | 336,964 |

Only class E can **ever** be crossed by alias/map work, because `matched_code` is
all-or-nothing per row. Within class E, 1,422 rows / 318,432 B have real names on both
sides of every pair, forming **1,071 distinct pair-SETS** — the true unit of work, since
a row crosses only when *all* its pairs are forgiven. 1,243 of those rows carry exactly
one pair.

And the alias-provable supply is far smaller than the raw charge count suggests. The
whole-binary charged-pair census (`tools/icf_relocname_census.py`) gives **28,321**
distinct charged pairs, **26,836** after subtracting the 1,485 already aliased — but only
**1,556 pairs / 2,591 sites carry real names on BOTH sides**. The other **25,260** are
retail-side *placeholder*, i.e. the identification backlog, which no alias can touch.
