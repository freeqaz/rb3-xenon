# W16-QU — App::Run, its exception filter and the frame loop (2026-10-06)

Branch `w16-qu` off main `014662e57`. Worktree `~/tmp/wt-w16qu`.
Brief: write the five `App.cpp` rows that W16-QR re-homed into `App` but left at 0%
(W16-QR §3.2–§3.3, `docs/decomp/W16QR_NOTETUBE_BODIES_AND_SUSPECT_PINS_2026-10-06.md`).

| row | addr | size | before | after |
|---|---|---|---|---|
| `??1App@@QAA@XZ` | `0x82270000` | 20 | 0 | **100** |
| `?DrawRegular@App@@IAAXXZ` | `0x82270018` | 104 | 0 | **100** |
| `?RunWithoutDebugging@App@@QAAXXZ` (was `fn_82270080`) | `0x82270080` | 264 | 0 | **100** |
| `?AppExceptionFilter@@YAJPAU_EXCEPTION_POINTERS@@@Z` (was `fn_822703A8`) | `0x822703A8` | 36 | 0 | **100** |
| `?Run@App@@QAAXXZ` | `0x822703D0` | 68 | 0 | **100** |

All five read 100 on both `fuzzy_match_percent` and `match_percent_normalized` (graded
`name_check` ruler). Whole-binary A/B: **+5 fns / +492 B, as predicted**, all in
`default/App` (§4).

Commits: `ed0d5a02d` (source, header, two map names, Main.cpp comment), `4b09a0490` (one
alias membership).

## 1. Identifying the globals

The bodies are short. The work was naming the pointer globals the frame loop polls. Four had
map names. The rest were identified from their other references and from the RTTI next to
their initialisers in `.data`:

| label | identity | evidence |
|---|---|---|
| `lbl_82C76B68` | `TheRnd` (`Rnd &`) | 116 references, e.g. `RndShadowMap::PrepShadow`, `RndConsole::Break` |
| `lbl_82C721F0` | `TheUI` (`UIManager *`) | 75 references, e.g. `UIScreen::InComponentSelect`, `MusicLibrary::OnEnter` |
| `lbl_82E036FC` | `TheSynth` | 41 references, e.g. `MasterAudio::Load`, `Sfx::Sfx`, `StandardStream::Init` |
| `lbl_82C72910` | `TheUIStats` | set and cleared by `BandUI::Init` / `BandUI::Terminate` |
| `lbl_82C716F0` | `TheEntityUploader` (`EntityUploader &`) | initialised to `&lbl_82CC97A4`; the next `.data` object is the `.?AVXboxEntityUploader@@` type descriptor |
| `lbl_82C7292C` | `TheAuditionMgr` | initialised to `&lbl_82DFDDBC`; the next `.data` object is the `.?AVAuditionMgr@@` type descriptor. The call goes through vtable slot 21 (`0x54`), which `AuditionMgr.h` already records as the one virtual AuditionMgr introduces (`Poll`) |

The frame loop reads, in order:

```
SystemPoll(false); TheUIStats->Poll(); TheAchievements->Poll();
TheAccomplishmentMgr->Poll(); PrefabMgr::GetPrefabMgr()->Poll(); TheSaveLoadMgr->Poll();
TheProfileMgr.Poll(); TheMusicLibrary->Poll(); TheSynth->Poll(); TheNet.Poll();
TheRockCentral.Poll(); TheEntityUploader.Poll(); TheAuditionMgr->Poll(); TheUI->Poll();
TheTaskMgr.Poll(); DrawRegular();
```

This order matches the Wii retail loop's order once its timers and its Wii-only calls are
removed (`CheckForPassivePlatformErrors`, `UpdateStoreOverlay`, the hang detector and
`PollTriFrame`). `TheAuditionMgr->Poll()` has no counterpart there; it is 360-only. The loop
calls `DrawRegular` directly, not `Draw()`. `DrawRegular` is
`TheRnd.BeginDrawing(); TheUI->Draw(); TheRnd.EndDrawing();` (slots `0x80`, `0xC`, `0x84`):
the DC3 shape, with no Wii home-menu branch.

## 2. Three source details that retail bytes decided

1. **`gApp` and `gOldExceptionFilter` are external globals, not file statics.** With both
   `static`, MSVC co-addressed them off one base register (`addi r31, r11, …` then
   `lwz r3, 0x8(r31)`). Retail gives each its own `lis`/`@l` pair. Removing `static` took the
   filter 65.56 → 100 and `Run` 73.24 → 98.53 in one build. This is the co-addressing
   signature in memory `project_msvc_global_coaddressing_2026-08-17.md`, here showing that
   the two are separate externals.
2. **`RunWithoutDebugging` is `__declspec(noreturn)`.** The filter has no epilogue after its
   `bl`. MSVC drops the epilogue only when the callee is declared no-return. The attribute is
   on the declaration in `src/App.h`, under `#ifdef _MSC_VER`. With the attribute, the filter
   had no epilogue on the first build.
3. **`Run` faults before it stores the previous filter.** Retail is
   `li r10,1; stw r10,0(r0); stw r3,gOldExceptionFilter`. Writing
   `gOldExceptionFilter = SetUnhandledExceptionFilter(...); *(int *)0 = 1;` scheduled the two
   stores the other way round (98.53 fuzzy, mpn 100). Holding the result in a local and
   storing it after the fault gives 100. That is also what retail does at run time: the
   fault fires first, so the filter reinstalls whatever `gOldExceptionFilter` already held
   (null).

`fn_8283C6B0` is the callee these call sites use for `SetUnhandledExceptionFilter`. It was
not given that name in the map. Its body fits (it swaps `lbl_82E5A2A8` and returns the old
value). But the call sites are placeholder targets, which are already uncharged, and no base
object defines the function, so a name would pair no row.

## 3. One fold membership: `?Poll@UIStats@@QAAXXZ` → `0x826c3888`

After §2, the frame loop's only charge (99.92) was at the `TheUIStats->Poll()` call. Retail
branches to `0x826C3888`, the shared empty-body survivor
(`??$?0H@?$StlNodeAlloc@V?$_List_node@H@…`). Our `UIStats::Poll` is `{}`, a bare `blr`.

- `tools/icf_pair_adjudicate.py --chase`: **CHASED T1 PROVEN (VACUOUS-BUT-IDENTICAL)**.
  Flat T1 is undecidable on a one-word body, as it is for every member of this FT-EMPTY group.
- Call-site witness: the loop loads `TheUIStats` into r3 and branches to the survivor right
  after `SystemPoll` and before `Achievements::Poll`. That is the Wii retail position of
  `TheUIStats->Poll()`.
- `UIStats::Poll` has no map row anywhere else.

It was appended to the existing `0x826c3888` group's `folded` list, with an `admitted` record
(lane W16-QU). That took the loop from 99.92 to 100.

## 4. Measurements

`tools/ab_measure.py --from-dirty` on the worktree, with all five changed paths applied as
one patch (`f9ec6066fe6eff9d`, kinds `map` + `source`). The graded `name_check` ruler was
used. Both legs were settled, force-re-split, at a `symbols.txt` fixed point (0 extra
re-splits each) and measured in-run.

| | predicted | measured |
|---|---|---|
| Δmatched (mpn) | +5 | **+5** (Δhonest +5, Δmasked_equal +0) |
| Δmatched_code | +492 B (20+104+264+36+68) | **+492 B**, Δcode% +0.004805 pp |
| Δfuzzy | — | +0.006750 pp |
| units | only `default/App` | `default/App` 9 → 14 matched; no other unit moved |
| units at 100 | +0 (`App::App` stays 0) | +0 on both rulers |

The `none` control read +492 B too. The tool marks that reading NOT_APPLICABLE, because a
patch containing source is expected to move `none`.
Leg A absolutes were 54,632 matched / code% 58.500015 / fuzzy 64.126660. They are for
orientation only: deltas compose, absolutes do not.

⚠ The worktree's starting `report.json` showed `fn_822715B0` and five of its 40-byte
neighbours at 0. After this lane's first build they read 99.3. Leg A shows the same values,
so the 0s were a stale reflinked report, not an effect of this change.

Native gate on the final `src/` (`ed0d5a02d` + `4b09a0490`):

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

The native build does not compile `src/App.cpp` or `src/Main.cpp`. It globs only
`src/system/*` dirs. So this gate run confirms that nothing shared broke. It does not test
these bodies.

## 5. Other edits

- `src/Main.cpp`: the comment said `main` was capped at 96.84 by a `bcl 20,0,X` call and that
  `0x82270080` was `App::Run`. Both claims are false on the clean TU5 target. `main` reads
  100, and W16-PT lists that `bcl` as RB3 Deluxe's patch group 2 ("debugger check"). That
  patch sent the call straight to `RunWithoutDebugging`, which skipped `Run`'s filter entry.
  Only the comment changed.

## 6. Deliberately not done

- **`App::App` (`fn_82270E68`, 1,864 B) was not written.** It is outside this brief, and
  `default/App` stays below 100 because of it and the 40-byte neighbours.
- **`fn_8283C6B0` was not named.** See §2.
- **No permuter.** None was needed.
- **Process note:** a first draft of this doc, written into the worktree while
  `ab_measure --from-dirty` was running, was deleted by the tool's restore. The tool reported
  the deletion, and it is documented behaviour. Write run-time deliverables to `~/tmp` until
  the run ends.
