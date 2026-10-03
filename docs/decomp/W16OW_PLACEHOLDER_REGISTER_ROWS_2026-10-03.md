# W16-OW — register-only rows hidden as name rows: classifier fix and hand sweep (2026-10-03)

**Branch** `w16-ow`, off main `2147968b1`, rebased onto `66103e262` (W16-OT landed). **Ruler** `name_check`
(graded, from `report.json` `provenance.diff_config`). **The permuter was not run.** No map, splits, alias or
`symbols.txt` edits. Lever 1 of `CAMPAIGN_STATE_2026-10-03.md` §6.

## 1. The classifier fix

W16-NA's charge classifier (`~/tmp/w16na/cls.py`, copied unchanged into W16-NP's `diffall.py`) was never committed.
It counted every differing symbol argument as a relocation-name charge. `name_check` does not charge a symbol
argument whose **retail (left) target** is a splitter placeholder (`fn_`/`lbl_`/`jumptable_`/`code_`/`data_`/
`bss_`/`rdata_`/`vftable_` + hex) or an MSVC `$`-label: objdiff-core `reloc_eq` returns true for it
(`diff/code.rs`, `is_placeholder_symbol_name`, `is_compiler_local_label`). So a register-only row whose only symbol
differences were placeholders was filed `NAME+REG` (N2), and no register sweep saw it.

`tools/charge_classify.py` (commit `d7b803c32`) is that classifier, committed, with the correction: such arguments are
counted as `symbol_forgiven` and ignored by `classify()`. `legacy=True` reproduces the old filing for comparison.

| check | result |
|---|---|
| refiled on main `2147968b1`, all sourced rows 0 < fuzzy < 100 | **33 rows / 17,256 B** (32 → `REG_ONLY`, 1 → `STACK_REG`), exactly W16-OV's figure |
| `--control`: moved rows read the same fuzzy at `functionRelocDiffs=none` | **33 / 33** |
| negative control: real-name `NAME+REG`/`NAME_ONLY` rows read differently at `none` | **60 / 60**, so the control can fail |
| `--selftest` | prefix rule incl. `fn_helper`/`fn_`/`$` edge cases, and a refile/no-refile pair: PASSED |
| re-run on the final rebased tip | **30 rows / 14,248 B** refiled (= 17,256 − the 3,008 B fixed below), control 30/30, negative 60/60 |

No row fell into the new `FORGIVEN_ONLY` class (every row with a forgiven symbol arg also has a register charge).

## 2. Whole-branch A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-ow-ab --patch <git diff main w16-ow -- . ':!docs' ':!tools'>`
- **Worktree:** fresh, `scripts/setup_worktree.sh` at main `66103e262`.
- **Patch:** 3 source files, +4/−4. objdiff-cli sha `c1b7d952`, stable across legs.
- **Run dir:** `~/tmp/wt-w16-ow-ab/.ab_measure_runs/20261003-061712-ab_branch-3425419/`.

```
leg A: matched=53280 masked=25144 honest=28136 code%=57.398586  (recompiles: 0, settled)
leg B: matched=53280 masked=25144 honest=28136 code%=57.427944  (recompiles: 5, split=0, patch_steps=6, settle iterations: 2)
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.029358pp  Δcode_bytes=+3008
units at 100% [mpn ruler]: 527 -> 527   [all-rows-fuzzy ruler]: 465 -> 465
```

**Prediction, written before the run:** +0 functions (all three rows were already at `mpn` 100) and +3,008 B
(104 + 1,652 + 1,252). **Measured: +0 / +3,008 B.**

**Row-level diff of the archived legs:** 3 rows up (the three below, each to fuzzy 100), **0 rows down**, 68,913 rows
on both legs, none appeared or vanished.

## 3. The 16 in-scope rows

Ring = W16-OV's (`IN-CORE`/`IN-SOON`/`IN-RB3ENG`). All 16 were at `mpn` 100. Each charge was first checked as a
possible behaviour defect: every placeholder target was paired with our symbol by position and, where it was data,
its retail bytes were read (`tools/retail_body.py` `Img`) or its `.data` order checked. W16-MB had already confirmed
that the float-literal placeholders in the VocalPart rows hold exactly our constants.

### 3.1 Fixed (3 rows / 3,008 B)

| row | B | before → after | lever |
|---|---:|---|---|
| `Det(const Matrix3&)` (`math/mtx.cpp`) | 104 | 95.769 → **100** | **Behaviour fix.** Retail returns the loaded `0.0f` constant when `det == 0`; ours returned `det`, which is `-0.0f` when the cofactor sum cancels to negative zero. `return 0.0f;` |
| `RGGetChordName` (`beatmatch/RGUtl.cpp`) | 1,652 | 99.964 → **100** | **Statement order.** The slash-chord stores already went to the right globals (`.data` order: `gSlashNote` `0x82C78594`, `gSlashString` `0x82C78598`, `gSlashFret` `0x82C7859C`; retail stores the note, the string index and the fret to exactly those). The callee-saved address registers follow the order the statements are written: note, string, fret. |
| `TrackDir::DrawShowing` (`track/TrackDir.cpp`) | 1,252 | 99.696 → **100** | **Drop a named temporary.** `cur = RndCam::Current(); i6->Select(); i7 = cur;` → `i7 = RndCam::Current(); i6->Select();`. Same values and call order; the temporary rotated the colouring of {`this-0xa0`, `i7`, `b2`}. |

Yield: 3 / 16 rows (18.8%), 3,008 / 11,864 B (25.4%), in the 18.6–28.5% band W16-NA/ND/NS measured.

### 3.2 Permuter-only (13 rows / 8,856 B), with what was tried

Every row below is a pure register colouring difference: same instructions, same constants, same callees, same
values reaching the same stores. "Inert" = byte-identical score.

| row | B | fuzzy | charge | tried |
|---|---:|---:|---|---|
| `SaveLoadManager::SetState` | 4,096 | 99.893 | prologue rotation {`newState`, `&saveload_dialog_event`, `wasIdle`} r25/r29/r27 vs r27/r25/r29; r26/r27 swap {value, vptr} at the three `TheCacheMgr` call sites | `wasIdle` before the static: 99.232; `unk4c.c_str()` as the argument instead of the `cacheName` local: inert. Prior lanes (W16-ID, W16-LD) record the same residue after ~3 spellings each. |
| `AssetMgr::GetTypeFromName` | 692 | 99.566 | `name` ↔ `&none_hat` (r26/r25), every other static in declaration order | no build: the ten statics' order is pinned by their guard bits (bandana bit 0 … wrists bit 9) and the compare order by the return values, so there is no definition-order lever left |
| `Tour::OnMsg(PrimaryProfileChangedMsg)` | 596 | 99.765 | r10/r11 between `isPostScreen` and the `TheUI` base | `isPostScreen` before `pScreen`: inert; `isPostScreen` hoisted above `InMode`: 98.456 |
| `MemTracker::DiffDump` | 500 | 99.04 | whole-function callee-saved rotation (`ts`, `allocBegin`, `allocVec`, end pointer, both strings) | dropped the `allocEnd` alias: inert. Behaviour checked: retail `0x8211B2E0` = `"alloc"`, `0x8211B2E8` = `"free"`, printed for `*allocIt` / `*freedIt` as ours |
| `MidiParser::PushIdle` | 484 | 99.752 | r27/r28 between `&gNullStr` (live 32–61) and the constant `5` (live 62–79): non-overlapping ranges coloured in the opposite order | `idx` defined before `arr`: 97.967 |
| `VocalPart::GetNoteSliceWeight` | 484 | 98.760 | whole-function callee-saved FPR rotation (`noteDurationMs` f23/f26, `fEndRel` f27/f29, `fDurationCap` f29/f28, …) | `noteDurationMs` local dropped: 89.033; `fBeginRel` defined before `fEndRel`: inert |
| `SessionMgr::OnMsg(RemoteUserLeftMsg)` | 440 | 99.045 | colour classes {0, `msg`} and {`user`, `&mWaitingUsers`/`&leaderLeftMsg`} swapped | `leader` read before the static: 94.727 |
| `HeaderSortNode::FinishSort` | 380 | 99.684 | hoisted-triple numbering (§3.3) | `case kNodeSubheader` first: 99.474 (reverses the `lis` order, r11 still on the first-appearing type) |
| `ProfileMgr::CheckProfileWebLinkStatus` | 292 | 99.589 | hoisted-triple numbering (§3.3) | `prog` reference inlined into the call: 96.301 |
| `ProfileMgr::CheckProfileWebSetlistStatus` | 292 | 99.589 | same body shape as its twin, same charge | not built separately: the twin's spelling lost 3.3 pp |
| `VocalPart::UpdateMinMaxPitch` | 288 | 98.611 | volatile colouring in the scan loop (`FLT_MAX` base r10/r9, phrase cursor r9/r10) | `foundPitchedNote` after the `FLT_MAX` stores: inert; named `const VocalNote&` for `mNotes[i]`: inert |
| `StoreMenuPanel::GetCrumbText` | 176 | 99.318 | r26/r27 between `result` and the hoisted format string | `result` defined after the limit computation: 84.955 |
| `CharacterCreatorPanel::GetHair` | 136 | 98.235 | callee-saved rotation {return slot, `&none_hair`, `this`/`hairName`}; its twin `GetFaceHair` has identical source and matches 100 with the other rotation | ternary return: 87.941; `!=` with swapped arms: 92.059; `Outfit&` local: inert; `BandCharDesc *desc` local (the `GetGlasses` shape): inert |

### 3.3 One signature worth a targeted study

`FinishSort` and both `ProfileMgr::Check*Status` rows share one charge: three loop-invariant addresses hoisted into
the loop preheader, `lis`-ed in the same order on both sides, which retail numbers r11/r10/r9 in emission order while
ours gives r11 to the **last** `lis` and r10/r9 to the first two. In `FinishSort`, swapping the case order reverses the
emission order but r11 stays on the first-appearing type, so source order alone does not reproduce retail's
pairing. Three rows / 964 B here; the same shape may sit in other register-only rows. Not pursued further.

## 4. Gates

On the final rebased tip, after a full build: the classifier re-run in §1, and the native gate, run last on the final
code:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

Only this docs-only commit follows the gate. The branch touches no map, splits, `symbols.txt`, `obj/Object.h` or
`os/Debug.h`, and none of the files W16-OT edited.

## 5. Not done

- **The permuter was not run** (standing directive).
- The 17 whole-binary rows outside the in-scope rings (Quazal, `synth_xbox`, `Cache_Xbox`, VIA-DC3) were refiled by
  the classifier but not opened. Quazal and `src/network` are out of scope by directive.
- `~/tmp/w16ov/` scripts were not edited; `tools/charge_classify.py` is the committed replacement for the classifier
  step. W16-NP's SCHED rule and the unpaired/insert-delete sub-classifiers were not ported.
- §3.3's hoisted-triple numbering was characterised, not solved.
