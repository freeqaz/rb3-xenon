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

---

## §3 — MEASURED. The pre-registration held exactly.

`python3 tools/ab_measure.py --worktree /home/free/tmp/wt-w16-fa --from-dirty`,
map-kind patch so both legs forced a re-split and iterated to a `symbols.txt`
fixed point. Ruler: shipped graded (`functionRelocDiffs=name_check`), objdiff
4.2.9, `tool_commit a5f0ea903ec1`, read out of `report.json`'s own
`provenance.diff_config`.

| measure | predicted in §1 | measured |
|---|---:|---:|
| Δ`matched_functions` | +4 | **+4** |
| Δ`matched_code` | +3,516 B | **+3,516 B** |
| Δ`matched_code_percent` | — | +0.034313 pp |
| Δ`masked_equal_functions` | 0 | **+0** |
| Δhonest (`matched − masked_equal`) | +4 | **+4** |
| units at 100% | no change | 189 → 189 |
| leg-B recompiles | 0 (map-only) | **0** |
| `renamer_patched` | >0 | 1,830 |

Row-level: **exactly the four pre-registered rows crossed**, summing to exactly
3,516 B. **Zero unpredicted bonus bytes**, and **zero rows fell off 100**. A
bonus here would have been the alarm, not the prize — §1 named that explicitly.

### The would-be false positives did NOT cross — all five

This is the gate the lane was designed around, and it is the half of the result
that carries the integrity claim. Every one of the five rows that a
*wrongness-forgiving* membership would have wrongly paid improved without
crossing:

| row | fuzzy before | fuzzy after | crossed? |
|---|---:|---:|---|
| `??0PlayerTrackConfigList@@QAA@XZ` | 99.71014 | 99.92754 | **NO** |
| (4 further pre-registered rows) | — | improved | **NO** |

`??0PlayerTrackConfigList` is the sharpest of them: it carries `vector<int>`
charges at idx 53/56/**59**/62/65, of which **59 targets the OTHER twin**. Had
the membership been forgiving wrongness rather than folding, idx 59 would have
been forgiven too and the row would have crossed and banked its bytes. It moved
up and stopped — the ruler declined to pay exactly the site the evidence says is
genuinely a different callee.

---

## §4 — ★★★ TWO CORRECTIONS TO THE BRIEFED RECIPE

### 4.1 The displacement decode is CIRCULAR for a charged site

The brief calls the per-caller displacement decode **"★★★ THE HARD RULE"** and
step 3 of the recipe. **As a proof it is circular, and I can size the error.**

objdiff's `T` (the retail-side name in a `diff_arg` charge) **is** the
`target_symbol_map.json` name at the retail branch destination. So decoding that
destination out of `band.exe` and checking `map[addr] == T` **re-derives
objdiff's own input** and passes *by construction*. It cannot fail on a charged
site, whatever the truth about folding.

Measured, not argued: of **48** pairs that the displacement channel called
PROVEN, **9 are REFUTED by chased T1** — including

- `_snprintf` ↔ `Hx_snprintf`
- `PrefabIsCustomizable` ↔ `GetPrefabMgr`
- `CheckAwesomesCondition` ↔ `CheckHoposPercentCondition`
- `find@FixedString` ↔ `End@Movie`

⇒ **displacement alone would have installed 9 fabricated aliases**, a **17.7%
fabrication rate by bytes** (12,468 B of 70,592 B).

**Its true role is narrower and still real:** once chased T1 has proven the
*body*, and retail holds two or more body-identical twins, the displacement is
the only thing that can pick *which* twin a given caller meant. That is exactly
the job it did in W16-EZ's group (two surviving 188-byte twins). It is also a
strong *screen* — a `REFUTED_split` verdict is meaningful and cheap. It is not a
proof.

### 4.2 ★★★ Chased T1 can return PROVEN on a purely VACUOUS basis

Newly found this lane, and it is not covered by `--chasetest`'s anti-vacuity
arms (the in-family decoy uses a normal-sized body, so the decoy arm cannot
expose this).

`??3BinStream@@SAXPAX@Z` ↔ `OggFree` — **6,112 B / 28 rows, the single largest
pair-set in the entire vein** — reads:

```
retail_size 4    our_size 4
FLAT T1  : UNDECIDABLE  "VACUOUS: body under 4 words ... compares equal to too much"
retail_bodytwins   396
our_bodytwins    2,364
CHASED T1: PROVEN
      VACUOUS-BUT-IDENTICAL  ??3BinStream@@SAXPAX@Z / OggFree
```

The chase's **only** evidence line is `VACUOUS-BUT-IDENTICAL`. A 4-byte body is
one instruction; it compares equal to 396 retail functions and 2,364 of ours.
"PROVEN" here means "two `blr`s are the same `blr`".

⇒ **`CHASED T1: PROVEN` must always be read together with `retail_size` and the
`VACUOUS-BUT-IDENTICAL` marker.** Re-classifying the top-40 pair-sets on that
rule moves the honest supply:

| class | bytes | rows |
|---|---:|---:|
| PROVEN substantive (real slot-fold chain, or ≥16 B body) | **48,304** | 87 |
| PROVEN but VACUOUS-ONLY — **refused** | 9,820 | 36 |
| displacement-PROVEN, chase-REFUTED — **refused** | 12,468 | 26 |

Only **68.4%** of the displacement-PROVEN bytes survive a vacuity-aware chase.

### 4.3 A correction to W16-EZ (mechanism, not decision)

EZ read `??0DataArraySongInfo`'s failure to cross as *"the ruler declined to
pay"* its twin-targeted sites. **That is not what happened.** Its twin sites
(idx 445/505) are **not charged at all**: `0x827a36f0` is a JSON-`null` row in
`target_symbol_map.json`, so the renamer skips it and the target keeps the
placeholder `fn_827A36F0`, which objdiff **forgives by construction**
(`is_placeholder_symbol_name`). The row was held back by `vector<int>` charges
at **survivor**-targeted sites — which is precisely why this lane's membership
crossed it.

EZ's *decision* was right and its *gate* was right. Its account of the mechanism
was not, and a lane inheriting "the ruler declines to pay a twin site" would
build on a rule that does not exist.

---

## §5 — REFUSED MEMBERSHIPS (a refusal with a reason is a full result)

| candidate | bytes | reason refused |
|---|---:|---|
| `??3BinStream@@SAXPAX@Z` ↔ `OggFree` | **6,112** | 4-byte body; 396 retail / 2,364 our body-twins; chase self-labels `VACUOUS-BUT-IDENTICAL`; displacement circular for charged sites ⇒ **no evidence on either channel** |
| `?SetMultiplier@TrackPanelDir@@UAAXH_N@Z` | 3,708 | same class — 8-byte body, vacuous |
| 9 displacement-PROVEN pairs (incl. `_snprintf`↔`Hx_snprintf`) | 12,468 | **chase-REFUTED**; displacement passed by construction |
| `?GenerateMacros@ShaderOptions@@` | 3,084 | chase-REFUTED (a brief top-10 row) |

**Total refused: 25,372 B** — 5.2× the bytes this lane installed. That ratio is
the lane's real output.

---

## §6 — What I did NOT do, and why

- **I did not take a second membership.** The largest remaining candidates are
  all in §5. The 48,304 B "PROVEN substantive" class is real supply, but each
  entry still needs its own per-caller twin disambiguation and its own named
  false positive; I ran out of budget after the first, and installing on the
  class label alone is exactly the failure mode this doc documents.
- **I did not touch `CustomizePanel::Handle`** (5,036 B, the brief's documented
  trap). Its two `diff_arg` aliases were not adjudicated; the three
  instruction-level mismatches are a separate lane's work.
- **I did not bulk-install from `scripts/icf_alias_groups.json`** (~1,407
  ungated groups). Given a 17.7% fabrication rate on *displacement-screened*
  pairs, the fabrication rate on unscreened groups is unknown and certainly
  worse.
- **I did not prune any zero-forgiving group.**
- **I did not commit the four `~/tmp/w16fa/*.py` instruments into `tools/`.**
  The displacement scanner and the complement check are reusable, but the
  displacement one would be landed as a *prover* under a name that §4.1 shows is
  wrong; landing it needs a rename and a docstring that states its scope.
  Recorded here so the next lane can lift them from the transcript.
