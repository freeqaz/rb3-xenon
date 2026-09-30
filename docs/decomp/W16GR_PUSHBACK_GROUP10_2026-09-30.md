# W16-GR — group 10 `push_back<T*>` batch: predicted vs measured, and the miss

Lane W16-GR. Group 10 survivor `?push_back@?$vector@PAVChatReceiver@@V?$StlNodeAlloc@PAVChatReceiver@@@stlpmtx_std@@@stlpmtx_std@@QAAXABQAVChatReceiver@@@Z`
(`0x82b5f808`), `scripts/symbol_aliases.json`. Predictions were written and committed
*before* measuring (`792c7d4d1`, `docs/decomp/W16GR_PREDICTIONS_2026-09-30.md`):
**+85 matched functions / +27,572 matched-code bytes**. This doc is the after-the-fact
reconciliation the coordinator asked for.

## 1. Controls (from the earlier session, unchanged)

- `tools/icf_alias_finder.py --selftest` / `--chasetest` / `--self-break`: all passed.
- 60 candidate spellings chased; 60/60 admitted with zero red flags, 0 rejected. 3
  non-pointer-arg controls correctly refused (wrong template arg, not this survivor's
  fold).
- Added in 6 commits of ≤10 spellings each: `4c2c26270`, `d73a5b6f2`, `8bbb19aa5`,
  `e9e06566b`, `d3fef3c58`, `b4afc6d9b`.

## 2. A/B measurement, both directions

Both runs via `tools/ab_measure.py --worktree /home/free/tmp/wt-w16-gr`, ruler
`name_check` (the shipped default), `none`-ruler control run alongside each.

**Forward** (`--patch` = the 6-commit diff forward, `07f080af` → `b4afc6d9b`):

```
Δmatched=+83  Δmasked_equal=+0  Δhonest=+83  Δcode%=+0.215560pp  Δcode_bytes=+22088
[control none] Δmatched_code=+0 B Δcode%=+0.000000 (default ruler +22088 B)  -- ALIAS_SUSPECT (expected shape for a map-only patch)
units at 100% [mpn]: 201 -> 206 (+5); [all-rows-fuzzy]: 177 -> 180 (+3)
```

**Reverse** (`--patch` = the same diff applied in reverse, `b4afc6d9b` → `07f080af`):

```
Δmatched=-83  Δmasked_equal=+0  Δhonest=-83  Δcode%=-0.215560pp  Δcode_bytes=-22088
[control none] Δmatched_code=+0 B Δcode%=+0.000000 (default ruler -22088 B) -- FLAT (pure re-name)
units at 100% [mpn]: 206 -> 201 (-5); [all-rows-fuzzy]: 180 -> 177 (-3)
```

Exact mirror, confirmed by direct read of `~/tmp/w16gr_ab_reverse.log` (not taken on
say-so) — same 5 units at 100%, same regression list, same magnitude, opposite sign.
Both legs restored the worktree to its exact pre-run state.

**Measured: +83 / +22,088 B. Predicted: +85 / +27,572 B. Short by 2 functions / 5,484 B.**

## 3. The miss, row by row

Of the 85 predicted-pure rows (`~/tmp/w16gr_pure_detail.json`, verified `len==85`), a
whole-binary Leg-A/Leg-B Δ-diff keyed on `(unit,name)` found **0 rows lost anywhere in
the binary** and **exactly 78 rows gained, summing to exactly 22,088 B, with 0 gains
outside the 85-row list** — i.e. the entire measured delta is fully and only these 85
rows, no side effects. 7 of the 85 did not fully credit:

| # | symbol | unit | size | pre mpn / fuzzy | post mpn / fuzzy | charge remaining (`run_diff_inspect diagnose`) |
|---|---|---|---:|---|---|---|
| 1 | `?Sort@CharPollableSorter@@...` | Character | 1136 | 94.419 / 93.697 | 94.437 / 93.715 | **TRUE MISS.** Real, independent replace/insert-delete instruction cluster beyond the alias charge — not noise. |
| 2 | `?BuildChainVector@FxSend@@...` | system/synth/FxSend | 240 | 94.883 / 92.550 | 94.967 / 92.633 | **TRUE MISS.** Same shape — genuine unrelated defect. |
| 3 | `??$GatherObjectsFromDir@VRndMesh@@...` | AmbientOcclusion | 316 | 99.937 / 99.810 | **100.0** / 99.873 | mpn crossed; fuzzy residual. Sibling `GatherObjectsFromGroup` (row 4) diagnoses as 1 register-swap pair + 1 symbol-reloc arg, 0 unexplained — same family, same shape; this row's target symbol isn't independently indexed by objdiff (resolves through the alias table), so it could not be diagnosed standalone, but the report.json shortfall (0.13pp) matches the diagnosed sibling's magnitude. |
| 4 | `??$GatherObjectsFromGroup@VRndMesh@@...` | AmbientOcclusion | 396 | 99.949 / 99.798 | **100.0** / 99.848 | Same as row 3 — same undiagnosable-standalone symbol; same family/magnitude as row 5. |
| 5 | `?Dispatch@EnterFlowMsg@@...` | WaitingUserGate | 252 | 99.921 / 99.762 | **100.0** / 99.841 | **Diagnosed directly**: 63 instructions, 61 equal, 2 `diff_arg` = 1 register-swap pair (r11↔r3) + 1 symbol reloc. 0 unexplained, 0 actionable insert/delete/replace. Pure pre-existing residual, unrelated to group 10. |
| 6 | `?ParseDataResultsIntoSetlists@MusicLibraryNetSetlists@@...` | band3/meta_band/MusicLibraryNetSetlists | 1968 | 99.990 / 99.522 | **100.0** / 99.533 | **Diagnosed directly**: pure r28↔r29 / r8↔r9 register swap, 0 unexplained. |
| 7 | `?Update@MicInputArrow@@...` | MicInputArrow | 1176 | 99.898 / 99.864 | **100.0** / 99.966 | **Diagnosed directly**: pure r11↔r28 register swap, 0 unexplained. Also one of the §5 four-way multi-spelling rows in the predictions doc. |

Sizes sum exactly: 1136+240+316+396+252+1968+1176 = **5,484 B** = the byte shortfall,
exactly. Rows 1–2 are the only ones that also failed `mpn` (2 functions) = the function
shortfall, exactly. Both counts reconcile to the last byte/function — nothing left
unexplained.

**Mechanism**: `mpn` (what `matched_functions` counts) excludes arg-only penalties
(register swaps, branch-dest, reloc-name); `fuzzy` (what `matched_code`'s all-or-nothing
byte credit keys on) does not. Rows 3–7 had the group-10 alias charge as their *last*
mpn-blocking penalty — resolving it correctly crossed `mpn` to 100 and they are properly
counted in the +83 function delta — but each also carries an independent, pre-existing,
unrelated register-swap/reloc residual that keeps `fuzzy` short of 100, so `matched_code`
withholds their bytes. Rows 1–2 are worse: the alias fix moved their score at all
(94.42→94.44, 94.88→94.97) but a genuine second bug (not alias-related) still blocks
them from `mpn==100` entirely.

## 4. §5 multi-spelling rows (from the predictions doc), resolved

Of the 4 rows credited with >1 group-10 spelling:

- `RebuildKeyCheatsForMode` (468 B) — fuzzy 100, fully credited.
- `PrefabMgr::PrefabMgr` ctor (1,188 B) — fuzzy 100, fully credited.
- `PlayerDiffIcon` row (604 B) — fuzzy 100, fully credited.
- `MicInputArrow::Update` (1,176 B) — **this is miss-row #7 above**: crossed mpn, withheld
  from fuzzy/bytes by its own unrelated register-swap residual.

## 5. The >8-charges rows, re-derived

`~/tmp/w16gr_gt8_full.json` (recomputed this session against the built worktree):

| symbol | pre charges | post charges | class |
|---|---:|---:|---|
| `?Init@ByteGrinder@@QAAXXZ` | 66 | 0 | `mixed` |
| `??0FingerShape@@QAA@PAVRndDir@@@Z` | 20 | 0 | `pure` |

Both fully resolved to 0 remaining charges post-fix; this is the reconciliation of the
predictions doc's "9" brief vs the prior session's "~32" estimate — the true count was
2 rows, 66 and 20 charges respectively, both now closed.

## 6. Validator

`python3 tools/icf_alias_finder.py --validate`, re-run against the fix-applied,
freshly-built tree (`792c7d4d1`, `.ninja-build.lock` empty/stale, no build in flight —
confirmed via `ps aux` before running):

```
COVERAGE: 1660 groups classified (1660/1660 reached, 7231 member spellings looked up)
  OK (MAP-CONSISTENT)          1408
  TOLERATED PLACEHOLDER_SURVIVOR   34
  TOLERATED STALE_SPELLING         89
  TOLERATED SURVIVOR_MISLABELED    27
  TOLERATED UNWITNESSED            101
  CONTRADICTION_EXEMPT              1
  CONTRADICTED (FATAL)              0
VALIDATE: PASS -- 1408 map-consistent, 251 tolerated (enumerated above), 0 contradicted, 1660 total
```

**PASS, 0 contradicted.** The earlier attempt this session (`~/tmp/w16gr_validate_after.log`)
correctly REFUSED — the reverse A/B's build was in flight at that moment
(`.ninja-build.lock` held, 903 objects disagreeing with the manifest). That was the
instrument doing its job, not a defect; this re-run, after the reverse A/B completed and
the tree settled back to a verified fixed point, is the real "after" answer.

## 7. Open questions for the reviewer

1. Rows 1–2 (`CharPollableSorter::Sort`, `FxSend::BuildChainVector`) were classified
   "pure" (alias-only) by the original scout but carry a genuine, independent defect
   unrelated to group 10. Should these be filed as separate follow-up rows now that
   they're isolated, or left for whichever lane next touches `Character`/`FxSend`?
2. Rows 3–7 all carry independent register-swap residuals that block `fuzzy==100`
   despite crossing `mpn`. None are group-10's fault, but they're now the entire
   visible gap between "functions matched" and "bytes credited" for this batch — worth
   a small follow-up sweep given 4 of 5 are fully diagnosed as pure (0 unexplained)?
3. Rows 3–4 (`GatherObjectsFromDir`/`GatherObjectsFromGroup<RndMesh>`) could not be
   diagnosed standalone via `run_diff_inspect` (`Symbol not found in target` — the
   target side resolves only through the alias/ICF table, not as an independent
   entry). Is there a preferred way to diagnose a template instantiation whose target
   symbol is alias-only, for future lanes that hit the same wall?
