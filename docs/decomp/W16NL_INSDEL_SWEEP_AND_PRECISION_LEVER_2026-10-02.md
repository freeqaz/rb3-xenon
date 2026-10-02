# W16-NL — named insert/delete rows at fuzzy 90–99.99, and the single-precision lever (2026-10-02)

**Branch** `w16-nl`, started off main `7f0f55bda`, rebased with `--rebase-merges` onto `cb7613a70` and
again onto `119c4e270` (pre-rebase tips kept as `w16-nl-prerebase` / `w16-nl-prerebase2`; the lane's net
diff is identical across both rebases apart from hunk offsets in `target_symbol_map.json`; W16-NO and the
MISC fork both edited `synth_xbox/Mic.cpp`, which auto-merged). Five fork branches (`w16-nl-{RND,BOBJ,GAME,CW,MISC}`) are merged
into it with `--no-ff`. **Not merged to main.** Ruler `name_check` (graded). The permuter was not run.
No `fn_` row in a band3/network unit was edited, named or re-pinned (W16-NK's territory).

## 1. Population

W16-NH's census tools (`~/tmp/w16na/pop.py`, `cls.py`) re-run on main `7f0f55bda`: **954 named rows at
`90 ≤ fuzzy < 100` in sourced units (497,692 B)**. STRUCT_INSDEL is 353 rows / 263,384 B; 38 of those are pure
reschedules (insert/delete multisets equal with registers masked; W16-NA/ND territory, not opened), leaving
**315 rows / 241,332 B** — the brief's "about 321 / 245 KB". `CustomizePanel::Handle` was excluded (five lanes
record it as a wall). The 315 were split by directory into five disjoint slices, so no two forks shared a
file: RND (rndobj+rnddx9, 63 rows), BOBJ (bandobj+track, 56), GAME (band3+network, 45), CW (char+world, 44),
MISC (every other system dir, 106). Slices: `~/tmp/w16nl/slice_*.json`.

## 2. Whole-branch A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16nl-ab2 --patch ~/tmp/w16nl/ab_branch2.patch`
(`git diff main w16-nl -- . ':!docs'`, 53 paths, kinds map + source), fresh worktree at main `119c4e270`.
Run dir (worktree since removed; legs archived as `~/tmp/w16nl/abA2.json`/`abB2.json`):
`.ab_measure_runs/20261002-150619-w16nl-whole-branch-r2-870840/`.

```
leg A: matched=51328 masked=24620 honest=26708 code%=54.419518  (recompiles: 0, settled)
leg B: matched=51375 masked=24629 honest=26746 code%=54.558790  (recompiles: 242, split=1, settle iterations: 2)
split fixed point: leg A converged after 0 extra re-split(s), leg B after 0
Δmatched=+47  Δmasked_equal=+9  Δhonest=+38  Δcode%=+0.139272pp  Δcode_bytes=+14272
Δfuzzy=+0.014324pp
unit net (ALL units) = +47   vs whole-binary Δmatched = +47
units at 100% [mpn]: 476 -> 478 (FilterCoeffs, FreeCamera; 0 fell off)
```

The same patch measured against `cb7613a70` (before W16-NO landed) read identically: +47 / +9 / +38 /
+14,272 B. It was re-run because of the `Mic.cpp` overlap.

**Prediction, written before the run:** sum of the five fork row-diffs (each against `7f0f55bda`) plus my
own commits = **+47 fns / +14,272 B**. **Measured: +47 / +14,272 B exactly** — the disjoint-by-file slicing
composed with no overlap loss even though main moved three times underneath.

**Row diff of the archived legs** (`~/tmp/w16nl/ab_rowdiff2.txt`): 69 rows up, **1 row down, 0 rows off 100**,
2 renamed keys:
- `UtilDrawPlane` drops its unused trailing `bool` (retail's only caller never sets r8); same score, 99.344.
- `CharWidgetImp` ctor: the map named a `Color32` parameter where retail stores a 16-byte `Hmx::Color`.
  The wrong name left the 404 B row unpaired; **0 → 97.91**.

**The row that went down:** `CharCollide::Highlight` (656 B), fuzzy 96.506 → 96.317. **Behaviour fix**: the
vertex-sphere loop advanced its cursor before the first read, so it drew elements 1..n instead of 0..n−1;
retail's cursor starts one element back. With the five-argument `UtilDrawPlane` call both call sites now line
up with retail, and the old misaligned tail had been earning partial credit. Kept on accuracy.

Of the 315-row pool, **32 rows / 13,032 B crossed to 100** and 53 moved up. The other crossings are
out-of-pool precision rows (§3) and nine 40-byte EH funclets that went 99.9 → 100 as a side effect (seven in
BandTrack/Group, `fn_8259878C` in SongSortMgr — moved by a named-row source edit, not touched directly).

## 3. The single-precision lever (coordinator)

**The brief's three rows do not call `fabs`.** Tested literally: `BandStarDisplay::SetNumStars`,
`PerfectSectionTracker::HandleExitExtent` and `DSP::LowpassCoefficients` contain no `fabs`/`std::fabs`; their
residue was frsp placement, stack slots and integer ordering. They were worked anyway:

| row | before → after | what retail showed |
|---|---|---|
| `DSP::LowpassCoefficients` | 95.35 → **100** | `coeffs[1]` (1 − cos w0) assigned first; four other orders stay at 95.35. |
| `BandStarDisplay::SetNumStars` | 98.88 → **100** | **Behaviour fix**: retail's `animated == false` branch lands on the `if (b)` test, so the earn-star sfx is gated only by `b` inside `newStar > i`; ours skipped it when no sweep ran. Plus `(int)mNumStars` converted first and `intPart` in its own block (retail `&intPart` = r31+0x68). |
| `PerfectSectionTracker::HandleExitExtent` | 93.77 → 94.77 | End tick in its own local fixes the argument order. Not closed: retail evaluates `unkc - (m0x0c - unk8 - i11c)` literally, numerator first; 13 spellings either reassociate it into adds or compute it first. |

To apply the lever "everywhere it fits", a detector flagged every sub-100 named row in a sourced unit, plus
every `fn_` row outside band3/network (1,239 + 439 rows), where **our side has more `frsp` or more
double-precision `fadd/fsub/fmul/fdiv/fmadd` than retail**. It found **8 rows**, and nothing in the `fn_` set.
Four were real precision defects:

| row | before → after | defect |
|---|---|---|
| `TrackPanelDirBase::GetPulseAnimStartDelay` | 78.11 → **100** | `std::floor` on a float promoted the tail to double; `floorf`. |
| `BandIKEffector::ApplyPosConstraints` | 82.40 → **100** | **Behaviour fix**: the chained effector's weight was added as `bool(weight)` (0 or 1); retail adds the float (`fadds f30,f1,f30`). Also `mWeight * 144` before the divide. |
| `CalibrationPanel::UpdateProgress` | 82.58 → 96.31 | Retail keeps the negative filter coefficients as stored doubles (0x820BFEE0 = −0.78059, 0x820BFED0 = −5.29293) and accumulates with `fmadd` in source order; parenthesising each term is the `/fp:fast` barrier (unbracketed, they fold to two trailing `fnmsub`s — the residue the old comment recorded). |
| `SetBloomBlurWeightsStreak` | 84.50 → 95.42 | **Behaviour fix**: the tap registers started at DC3's `0x9a`; retail loads `0x2f`, the RB3 base `SetBloomBlurWeights` already uses. Plus explicit `(double)` casts removed (retail `fmuls`) and `weights[3]` stored before the `pow` calls. |

The other four are not this lever: `RndFont::SetCharInfo` (tail-merge structure), `PatchLayer::Draw` (field
order, a by-value Color copy, member offsets), and the two `fft_matrix_*_columnwise` (hand-written VMX under
`float_control(precise)`, register residue). The CW fork found the same lever as `pow` → `powf` in
`FreeCamera::Poll` (91.38 → 100). **Generalisation: any `<cmath>` double routine applied to a float where
retail stays single — `fabs`, `floor`, `pow` — is the same defect.** On this tree the detector now reads only
the four non-fits, so the lever is drained for named rows.

## 4. What the forks closed

Counts are each fork's full-build row diff against `7f0f55bda`. Per-row before/after, commit and evidence are
in `~/tmp/w16nl/<FORK>/result.json`.

| fork | Δfns / Δbytes | rows up / down | opened |
|---|---|---|---|
| BOBJ | +21 / +5,296 | 25 / 0 | most of 56 |
| MISC | +7 / +1,992 | 16 / 0 | ~40 of 106 |
| RND | +7 / +1,008 | 11 / 0 | ~25 of 63 |
| GAME | +5 / +1,876 | 7 / 0 | most of 45 |
| CW | +3 / +2,440 | 4 / 1 (behaviour fix) | most of 44 |
| coordinator | +4 / +1,660 | 6 / 0 | 3 briefed + 8 detector rows |

### 4.1 Behaviour bugs found and fixed (each read on retail bytes)

| row | defect |
|---|---|
| `BandStarDisplay::SetNumStars` | Earn-star sfx skipped when no sweep animated (§3). |
| `BandIKEffector::ApplyPosConstraints` | Chained weight added as `bool` (§3). |
| `SetBloomBlurWeightsStreak` | Taps written to DC3's constant registers (§3). |
| `CharCollide::Highlight` | Vertex-sphere loop drew elements 1..n instead of 0..n−1 (§2). |
| `SongParser::HandleRGGemStop` | Channel-4 gems set `kStrumForceOff`; retail sets `kStrumForceOn` (0). |
| `PitchDetector::AnalyzeBlock` | New samples overwrote the retained overlap; retail starts at `ixDecim = overlap`. |
| `DxRnd::SetDefaultRenderStates` | `MaxPointSize` converted to an integer; retail passes the float's bit pattern (the D3D render-state convention). |
| `BandCamShot::FindTarget` | Ours null-tested `Dir()`; retail calls `Dir()->Find` unconditionally. |

### 4.2 Recurring levers worth carrying forward

- **A loop bound written in the loop condition** keeps retail's explicit counter (compare-and-branch) where a
  hoisted bound gave `mtctr`/`bdnz`. Five rows: `TalkyMatcher::Analyze` (→ 100), `ChatReceiver::ProcessChatData`
  (→ 99.81, mpn 100), `MicXbox::ReadChatBuffer` (→ 99.35), `fft_real_forward_scalar` (→ 86.24).
- **Implicit conversion instead of explicit `Symbol(..)`/`DataNode(..)`/`FilePath(..)` temporaries.** Five rows
  in BandTrack and BandHeadShaper. An explicit temporary made MSVC forward the constructor's return value
  instead of the stack address retail passes.
- **A dead frame store of an address marks a named reference local** (`T &x = …`): `TrackPanel::Poll`,
  `RandomizeFace`, `BuildSetlistTree`.
- **Repeated dead stores of one value before each test** can mean retail called an inline accessor at every
  use (`NgEnviron::Select` → 99.27).
- **Placeholder-named constants (`lbl_*`) must be read from retail bytes before acting on them.** RND made and
  reverted a `GetBlendState` curve change read off our side of the diff; retail's 0x82014844 = 3.0 and
  0x8201FEB0 = −2.0 confirm the original smoothstep. (I used the same check on CalibrationPanel and
  SetBloomBlurWeightsStreak above.)

## 5. What is left, and why

- **~120 of the 315 rows were not opened** (MISC ~60, RND ~38, the small tail of the others). They are the
  smallest rows of each slice; each fork worked largest first and stopped at its ~3 h budget.
- **Opened and left** (reasons per row in the fork `result.json`s): scheduling, register and stack-slot
  residue with no source construct behind it — `VocalTrack::UpdateScrolling` (8,948 B), `StoreOfferProvider::BuildList`,
  `ObjectDir::Save`, `UIListDir::BuildDrawState`, `Locale::Init`, `Song::SyncState`, `RndMesh::Load`'s dead
  stores to 0x54; tail-merge/cross-jump layout (`DrawBeatLine`, `MusicLibrary::Text`, `ResolveSlotStates`,
  `D3DFORMAT_BitsPerPixel`); component order inside shared math inlines (`math/Mtx.h` Plane ctor, Color/Quat).
- **Not landed:** `GetCareerLevel`'s table as `const` (fixes `AccomplishmentProvider::Mat`'s constant base but
  drops `GetCareerLevel` 100 → 97.84); `insert(end(), s)` in `BandRetargetVignette::EnterDir` (row up, four
  funclets down, not a behaviour fix). Both reverted.
- **Map misnamings found, not renamed** (a rename would leave the row unpairable without an identification):
  `vector<CharHair::Point>` insert helper (holds `StoreMainPanel::NewReleaseEntry`'s 0x3c-stride body),
  `vector<CharLipSync::Generator::Weight>` insert helper (1-byte elements; Weight is 2),
  `_M_insert_overflow_aux<Key<vector<Vector3>>>` in RndMeshAnim (retail stride 0x50, ours 16).
- **`SongSectionController::Load`** (pinned in the CharIKFingers unit) needs its two rev statics merged into
  one aligned struct; its source is compiled into two units, so it was left.

## 6. Gates

On the rebased tip (`d8a136418`, final code), after a full `./tools/ninja-locked` build:

```
VALIDATE: PASS -- 1798 map-consistent, 291 tolerated (enumerated above), 0 contradicted, 2090 total
[map-injectivity] OK: 33820 applied rows, 33819 distinct names, injective (+1 enumerated internal-linkage exception(s))
[patch-state] OK: 1262 decomp, 3093 target objects match 2026-10-02T15:09:52Z (tree_sha256=7e66ae82696c058c)
```
(`scripts/validate_symbols.py` read 68,742 checked `.text` functions, 0 invalid, on the pre-W16-NO tip.)

**No alias was added anywhere in the lane** — `git diff main w16-nl -- scripts/symbol_aliases.json` is empty —
so there was nothing for `icf_pair_adjudicate.py --chase` to adjudicate. Map edits: the two renames in §2.
Shared headers touched: `bandobj/TrackPanelInterface.h`, `track/TrackWidgetImp.h`, `rndobj/Utl.h`,
`utl/RangedDataCollection.h`, `band3/bandtrack/TrackPanel.h`. No PCH header, `math/Vec.h` or `symbols.txt` edit.
The native gate (§7) ran last on the final code; only the docs-only commit adding this file follows it.

A grep of `main..w16-nl` finds no added source line or commit message citing rb3-Wii or "the oracle", and no
Co-Authored-By line.

## 7. Native gate

Run on `d8a136418` (the final code after the second rebase; log `~/tmp/w16nl/native_gate2.log`), after
every build and gate above. It also passed on the pre-W16-NO tip (`~/tmp/w16nl/native_gate.log`).

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

Scratch: `~/tmp/w16nl/` (population `pop_base.json`, `cls_base.json`, `shape_insdel.json`, slices, fork
`result.json`s, detector inputs `det_all_named.json`/`det_fn_sys.json`, variant specs `v_*.json`, A/B legs
`abA2.json`/`abB2.json`, `ab_rowdiff2.txt`, `ab2.log`; the first run's `abA.json`/`abB.json`/`ab.log`).
