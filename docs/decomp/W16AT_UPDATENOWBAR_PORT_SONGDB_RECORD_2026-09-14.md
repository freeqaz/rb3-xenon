# W16-AT — `GamePanel::UpdateNowBar` ported (628 B crosses); the `SongDB` record is `PracticeSection`; `lbl_82C78F5C` is `TheTempoMap`

**Lane:** W16-AT (Fable) · **Branch:** `w16-at` off main `6c35e385` (= `11506937` + docs) ·
**Worktree:** `~/tmp/wt-w16-at` · **Date:** 2026-09-14 · **Ruler:** `name_check`, objdiff 4.2.9.

Two Opus lanes (W16-AP, W16-AS) deferred `?UpdateNowBar@GamePanel@@QAAXXZ`
(`0x82695178`, 628 B / 157 instr, unit `default/GamePanel`, fuzzy 8.19 at dispatch).
This lane was the escalation. **The row crosses at `fuzzy == 100` in `report.json`
(+1 fn / +628 B, predicted exactly, nothing fell out), the "genuine unknown" record
vector is the already-declared `std::vector<PracticeSection>` at compiler-verified
`SongDB+0x20` (no header change needed), and the thunk's singleton is `TheTempoMap`,
proved by its writer and named in the map at measured Δ0.**

| commit | what |
|---|---|
| `c0fd0e3f` | port the body (`GamePanel.cpp`, `TimeConversion.{h,cpp}`) — reads 100 graded |
| `c0eeb581` | map: `0x82c78f5c` = `?TheTempoMap@@3PAVTempoMap@@A` — Δ0 measured |

## 0. Briefed figures, tested literally

| briefed | measured | verdict |
|---|---|---|
| 628 B / 157 instr at `0x82695178` | `.fn fn_82695178` in `build/45410914/asm/GamePanel.s`: 157 instructions; report row `size 628` | holds |
| fuzzy ~8.19 / mpn ~8.48 | baseline snapshot `~/tmp/rows_w16aq_main.json` (row absent from the `fuzzy==100` set); GamePanel unit 94/97, 8,460 B | holds |
| callees list (MakeString, GetMaxValue@TourProperty, Seconds@TaskMgr, Symbol ctor, `fn_827C91A0`, fmod, GetTrackPanelDir) + `TheSongDB` | all present in the `.s`; **plus** `TheTaskMgr` (`lbl_82E051A0`, unnamed) and `gNullStr`'s pointer (`lbl_82C71838`) | holds, incomplete |
| `lbl_820E27B8 = 4.47656`, `lbl_820E27C0 = 7.41553` | they are **`lfd` doubles**: `1000.0` and `60000.0` (the briefed values are the high 32 bits read as a float) | wrong as stated, harmless |
| `fn_827C91A0` = `r3=*lbl_82C78F5C; f1*=1000.0f; tailcall vtbl[8](f1)` | bytes `3D6082C8 3D408200 806B8F5C C00A10B4 EC210032 81630000 816B0008 7D6903A6 4E800420` — exactly that | holds |
| baseline 43,470 / 4,028,008 B / 39.3130 % | set-diff tool prints 43,470 / 4,028,008 / 39.312965 from the baseline snapshot | holds |

## 1. The `SongDB` record — modelled with retail evidence per field

**Intent → expectation → measured.** I expected the 0x24-byte element vector at
`SongDB+0x20` to be something not yet declared (the brief called it "the one genuine
unknown"). It is not: `scripts/harvest/class_layout_report.py SongDB --tu
src/band3/game/SongDB.cpp --exact` reports

```
=== SongDB   sizeof = 52 (0x34) ===
  0x20  ?$vector@VPracticeSection@@...  mPracticeSections
=== PracticeSection   sizeof = 36 (0x24) ===
  0x0  Symbol unk0   0x4 unk4   0x8 unk8   0xc unkc   0x10 unk10   0x14 unk14 (bool, +3 pad)
  0x18 unk18   0x1c unk1c   0x20 unk20
```

⚠ Trap: a bare `class_layout_report.py PracticeSection` picks the **hamobj**
`PracticeSection` (0x60 B, an `RndAnimatable` from DC3) — same class name, different
class. `--tu src/band3/game/SongDB.cpp --exact` is required to see the game one.

Retail's body (`fn_82695178`, quoted from the `.s`, keyed on the `.fn` symbol) reads
exactly three fields of the element and two of `SongDB`:

| retail instruction(s) | field | meaning |
|---|---|---|
| `lwz r31, lbl_82E023F8@l(r31)` then `lwz r11, 0x20(r31)` / `lwz r8, 0x24(r31)` | `SongDB+0x20` = `mPracticeSections._M_start`, `+0x24` = `_M_finish` | vector begin/end |
| `li r10, 0x24` · `divw. r10, r8, r10` (after `subf`) | element stride 0x24 | `size()` = (finish−start)/36, signed divide then `cmplw` → `i < size()` compared unsigned |
| `lwz r6, 0x4(r8)` · `cmpw cr6, r21, r6` · `blt` | `PracticeSection::unk4` (start tick) | skip if `curTick < unk4` |
| `lwz r6, 0x8(r8)` · `cmpw cr6, r21, r6` · `bge` | `PracticeSection::unk8` (end tick) | skip if `curTick >= unk8` |
| `lwz r4, 0x0(r8)` (found path) | `PracticeSection::unk0` (`Symbol` name) | stored into the local `Symbol`; first hit wins, `b` out |
| `addi r7, r7, 0x24` | stride | next element |

So the predicate is `unk4 <= curTick < unk8` on the tick returned by the thunk, and
`unk0` is the section name passed to slot `0xd4`. Fields `unkc…unk20` are never read
here. **No header fix and no cascade** — the layout was already right; the header's
comments (`// tick`, `// some other tick`) were merely under-specific. `r21` is
`fctiwz`'d from the thunk's `f1` return, i.e. `int curTick = SecondsToTick(t1)`.

## 2. The singleton at `lbl_82C78F5C` — `TheTempoMap`, proved by its writer

Readers alone would be circumstantial. The proof is the one **writer**:
`fn_827D2510` = `?ResetTheTempoMap@@YAXXZ` (unit `default/TempoMap`, 6/6 rows at 100
before this lane touched anything):

```
lis r11, lbl_82C78F60@ha ; lis r10, lbl_82C78F5C@ha
addi r11, r11, lbl_82C78F60@l ; stw r11, lbl_82C78F5C@l(r10) ; blr
```

and our matching source is `void ResetTheTempoMap() { TheTempoMap = &gDefaultTempoMap; }`
(`src/system/utl/TempoMap.cpp:7`). Hence `lbl_82C78F5C = TheTempoMap` and
`lbl_82C78F60 = gDefaultTempoMap` (0x8 B: vptr + the `SimpleTempoMap` payload). The
`TempoMap` vtable is `[0] dtor, [1] TickToTime(float) @0x4, [2] TimeToTick(float) @0x8`
(`src/system/utl/TempoMap.h`), so the thunk's `vtbl[0x8](sec*1000)` is
`TheTempoMap->TimeToTick(ms)` — a seconds→tick conversion, the twin of the already
named `SecondsToBeat` at `0x827C91C8`.

Reader census (every retail `.s` referencing the label, keyed on `.fn`):

| unit | fn | map name | our source spells |
|---|---|---|---|
| GemPlayer | `fn_826C6D00` | `?Poll@GemPlayer@@…` | `TheTempoMap` (GemPlayer.cpp:1147) |
| VocalPlayer | `fn_826EB030` | `?Poll@VocalPlayer@@…` | `TheTempoMap` (VocalPlayer.cpp:929) |
| Task | `fn_82747858` | `?SetSeconds@TaskMgr@@…` | `TheTempoMap` (Task.cpp:414) |
| TempoMap | `fn_827D2510` | `?ResetTheTempoMap@@YAXXZ` | writer |
| StringTable (pin) | `fn_827C90B8/D0, 9110, 9128, 91A0, 91C8, 9218, 9370` | `MsToTick, MsToBeat, TickToMs, BeatToMs, (anon), SecondsToBeat, TickToSeconds, OnMsToTick` | `TheTempoMap` (TimeConversion.cpp) |
| **SongInfoCopy** | `fn_827D2500` | **`?_M_throw_length_error@?$_String_base@…`** | — a string-throw helper does not read a tempo map: **map row is wrong** (§6) |

Every one of our reader objs carries `?TheTempoMap@@3PAVTempoMap@@A` (checked in the
COFF string tables after a build — a fresh worktree's objs are pre-renamer). Naming
therefore converts forgiven placeholder sites into checked-and-equal sites: predicted
Δ0, measured Δ0 (§4). The one row that moved is the wrong one — the bug-exposure
signature, exactly as the map-economics record says naming pays.

**`fn_827C91A0` deliberately left anonymous.** Its body is defined in
`TimeConversion.cpp` as `float SecondsToTick(float sec) { return MsToTick(sec * 1000); }`
(the `/Ob2` inline of the 24 B `MsToTick` collapses to the 9-instruction tail-call
thunk; our compiled `?SecondsToTick@@YAMM@Z` in `StringTable.obj` is shape-identical to
retail's bytes modulo relocation fields). But the **name** `SecondsToTick` is
convention-derived from its twin `SecondsToBeat`, not proved by any retail string or
oracle; under `name_check` the call site in `UpdateNowBar` is already forgiven, so naming
it could only lose. The old in-tree note that the map once said `??__ETheLocale` for it
is recorded in the source comment.

## 3. The port

`src/band3/game/GamePanel.cpp` (`__declspec(noinline) void GamePanel::UpdateNowBar()`),
plus `SecondsToTick` in `src/system/utl/TimeConversion.{h,cpp}`. What retail computes:

- `t1 = TheTaskMgr.Seconds(kRealTime)`; `curTick = (int)SecondsToTick(t1)` (`fctiwz`).
- measure/beat/tick from `TheTaskMgr.GetSongPos()` (+1 on measure and beat), gated by
  `t1 >= 0` (`fcmpu f30, f26(0.0)`; `blt` skips → zeros).
- `t2 = TheSongDB->GetSongDurationMs() * 0.001f` (callee resolves to the ICF survivor
  `?GetMaxValue@TourProperty@@QBAMXZ`, forgiven via `symbol_aliases.json`).
- `elapsedMs = Max(t1, 0) * 1000`; `remaining = Max(0, t2 − t1)`; `remainingMs = Min(remaining, t2) * 1000`.
- min/sec/hundredths: `ms * (1/60000)`, `(float)fmod(ms, 60000.0) * 0.001f`,
  `(float)fmod(ms, 1000.0) * 0.1f` — twice.
- section lookup (§1) into a `Symbol` default-constructed from `gNullStr`.
- `GetTrackPanelDir()->Unkd4(MakeString("%d.%d.%03d", m, b, t), MakeString("%d.%02d.%02d", …elapsed), MakeString("%d.%02d.%02d", …remaining), section)` — the proved slot-`0xd4` shape kept.

Constants (read from `orig/45410914/band.exe` in Python, image base `0x82000000`):

| label | bytes | value |
|---|---|---|
| `lbl_82000D78` | `00000000` | 0.0f |
| `lbl_820010EC` | `3a83126f` | 0.001f |
| `lbl_820010B4` | `447a0000` | 1000.0f |
| `lbl_82070E14` | `378bcf65` | 1.6666667e-5f = exact float 1/60000 |
| `lbl_820010CC` | `3dcccccd` | 0.1f |
| `lbl_820E27C0` | double | 60000.0 (the "7.41553" in the brief) |
| `lbl_820E27B8` | double | 1000.0 (the "4.47656") |
| `lbl_820E27A4` / `lbl_820E2798` | strings | `"%d.%02d.%02d"` / `"%d.%d.%03d"` |

Codegen levers that mattered (all measured on the graded ruler through
`run_diff_inspect`/`run_objdiff` with `project_dir=` the worktree, then confirmed on a full
build + `report.json`):

1. **`fsel` shapes come from `math/Utl.h`'s float specialisations** (`Max(x,y) = (x−y<0)?y:x`,
   `Min(x,y) = (x−y<0)?x:y`): `Max(t1,0)` → `fsel t1,t1,0`; `Max(0,rem)` → `fneg`+`fsel`;
   `Min(rem,t2)` → `fsel rem−t2, t2, rem`. `std::max`/hand-written ternaries do not produce them.
2. **`fmod` is `double(double,double)` only** (`src/xdk/LIBCMT/math.h:90-93`) — `(float)fmod(x, 60000.0)`
   yields the `lfd` double constants + `frsp`; an `fmodf` spelling would not.
3. **Declaration order set the callee-saved layout**: 94.1 % → 100 came from declaring
   `std::vector<PracticeSection> &sections = TheSongDB->mPracticeSections;` *before*
   `Symbol section(gNullStr);` (keeps `TheSongDB` in `r31` across the ctor → prologue
   `__savegprlr_19`, frame `0x110`, no re-materialisation) and binding
   `const PracticeSection &ps = sections[i]` (kills the `mulli r11,r9,0x24` in the found path).
4. `TheTaskMgr` (`lbl_82E051A0`) is unnamed in the map and outside this lane's rows; the
   site is forgiven, and its two data reads match anyway.

## 4. Predicted vs measured, per item

All measurements: full `./tools/ninja-locked` in the worktree (rc=0, logs
`~/tmp/rb3_build_w16at_{1,2}.log`), `report.json` read with `int(x.get(k,0))`, set-diff via
`tools/rowset_snapshot.py` against the copied baseline `~/tmp/rows_w16at_base.json`.

| item | predicted | measured |
|---|---|---|
| 1. record model | header fix + cascade possible | **no header change**; layout already compiler-correct — Δ0 by construction |
| 3. port (`c0fd0e3f`) | +1 fn / +628 B, `default/GamePanel` only | **CROSSED IN 1 row / 628 B** `default/GamePanel::?UpdateNowBar@GamePanel@@QAAXXZ`, **FELL OUT 0**; 43,470 → 43,471 fns, 4,028,008 → 4,028,636 B, 39.312965 → 39.319096 %; GamePanel 94/97 8,460 B → 95/97 9,088 B; StringTable 28/35 2,016 B and SongDB 110/143 8,032 B unchanged |
| 2. name `0x82c78f5c` (`c0eeb581`) | Δ0 rows / Δ0 B; only the mis-named `fn_827D2500` may move | **Δ0 / Δ0** vs the post-port snapshot (43,471 / 4,028,636 both sides); `ResetTheTempoMap`, `MsToTick`, `SecondsToBeat`, `TaskMgr::SetSeconds`, `GemPlayer::Poll` all still 100; `VocalPlayer::Poll` 93.83235 → 93.83235; **`SongInfoCopy` `0x827d2500` 60.0 → 58.33** (12 B) |
| row `fn_827C91A0` (36 B) | unchanged (unnamed ⇒ unpaired) | fuzzy 0 before and after |
| 4. charged-site list | not needed if it crosses | not needed — row at 100.0 / 100.0 |

Whole-tree, lane-internal: **43,470 / 4,028,008 B → 43,471 / 4,028,636 B** (`total_code`
10,246,004, `total_functions` 69,217 unchanged).

## 5. Gates (in the brief's order, all in the worktree)

1. full build rc=0 — `~/tmp/rb3_build_w16at_2.log` (SPLIT + renamer 1,837 files patched + REPORT).
2. `python3 scripts/verify_ruler_agreement.py --check` → rc=0, "both objdiff-cli entry points resolve the same ruler".
3. `python3 scripts/verify_objs_patched.py --verify-manifest` → rc=0, `tree_sha256=9c9387e3c18fcc39`, 1215 decomp + 3115 target objects verified.
4. `tools/native_build_gate.sh` — run LAST; its `NATIVE_GATE_RESULT` line is appended in §7 by a docs-only follow-up commit (the gate must post-date every source change, and the doc must carry the line).

## 6. NOT done, and why (report-only findings for other lanes)

- **`SongInfoCopy` map row `0x827d2500` is wrong** — retail's body reads `TheTempoMap`; a
  `_M_throw_length_error` cannot. Outside this lane's unit; its fuzzy 60 → 58.33 after the
  naming is the exposure, not a regression to hide. Re-identify it in a SongInfoCopy lane.
- **The TimeConversion cluster (`0x827C90B8`–`0x827C9370`) sits inside `StringTable`'s
  `.text` pin**, so `TimeConversion.cpp`'s functions are scored against `StringTable.obj`
  (they still match because both TUs end up in that base obj through the pairing). A
  splits re-home would be cleaner; `splits.txt` is W16-AU's — not touched.
- **`fn_827C91A0` not named** (§2) — the name is convention-derived.
- **`TheTaskMgr` `lbl_82E051A0` not named** — outside the allowed rows; forgiven anyway.
- **`lbl_82C78F60` (`gDefaultTempoMap`) not named** — proved by the same writer, but outside
  the two addresses the brief allowed.
- **hamobj/game `PracticeSection` name collision** not resolved — a rename cascades into
  DC3-derived engine code; noted as a tooling trap only.
- The header comments on `PracticeSection::unk4/unk8` were left as-is (semantics now
  recorded here and in the `GamePanel.cpp` comment block) — a comment-only churn in a
  shared header is not free of risk (see the CLAUDE.md `ScatterIncludes` incident).
- No `Co-Authored-By`/AI trailer on any commit, regardless of the harness reminder.

## 7. Native gate line (appended after the run)
