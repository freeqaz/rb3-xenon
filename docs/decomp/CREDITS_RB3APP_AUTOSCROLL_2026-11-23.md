# rb3-app credits_screen: the list scrolls and ends; the 40,000-frame walk was too short (lane credits, 2026-11-23)

Branch `credits` (from `menuwalk` 524c3ed7c). Follows finding 2 of
`docs/decomp/MENUWALK_RB3APP_MENU_WALK_2026-11-23.md` ("credits_screen never ends or exits"),
which this record refutes.

## Brief and prediction

The brief: credits_screen is reached but its list "never scrolls or ends, even over 40,000
frames"; suspect the native list auto-scroll in `src/system/ui`. CreditsPanel::Poll fires
`credits_done` (credits.dta: `{ui pop_screen}`) once `!mList->IsScrolling()`, so I predicted one
of two native defects: the list never arms or steps (IsScrolling false from the start, which
would pop at once) or it never reports an end (IsScrolling stuck true).

Both predictions failed. Neither defect exists.

## What was measured

1. **Instrumented run** (temporary env-gated printf in `UIList::Poll` for `credits.lst`; not
   committed), det walk `w_credits` cut after the confirm press, 2,500 frames, through
   `no610m.sh`. On credits_screen entry (frame 1117, UI time 36.233 s): `auto=1`, NumData 1777,
   NumDisplay 30, speed 0.850 s per row, auto_scroll_pause 0, MaxFirstShowing 1747. From frame
   1118 the list scrolls: FirstShowing 1 at step 40, 3 at 80, ... 54 at step 1385, one row per
   25.5 frames (0.85 s at 30 fps), `IsScrolling` true throughout.
2. **Expected end:** 1747 steps x 0.85 s = 1485 s after entry, about frame 1117 + 44,550 =
   45,660.
3. **Long run** (same walk, 47,000 frames, uninstrumented): credits_screen transitions to
   meta_loading_main_credits_screen at **frame 45,674** (`credits_screen Exit (to '<null>')`,
   the pop), and main_hub_screen has focus again at frame 45,759. Prediction 45,660, measured
   45,674.
4. The menuwalk `credlong` run's own log ends at frame 40,000 total, UI time 1332.1 s, that is
   1296 s into a 1485 s scroll: it stopped about 5,700 frames before the credits end.
5. **Why the focus log showed `sel=29` throughout:** `ListSel` prints `UIList::SelectedPos()`,
   which is `mListState.SelectedDisplay()`, the highlight's display row, not the data index.
   The first 29 auto-scroll steps only move the highlight down the 30 visible rows (each one
   completes inside `UIListState::Scroll` and `UIList::CompleteScroll` chains the next), so
   the highlight sits on row 29 from frame 1117 while the rows scroll under it. The log was
   accurate; it does not show scrolling.

## Retail check (band.exe TU5, `tools/va_disasm.py`)

- `fn_827AF3D0` CreditsPanel::Poll: `+0x150` tests mAutoScroll (`lbz 0x4c`), `+0x15C..+0x16C` TheUI
  InTransition, `+0x174 bl fn_827F7F08` (UIList::IsScrolling), `+0x17C bne` skips, then the
  static `credits_done` message and `+0x218 bl fn_827F8028` (UIList::StopAutoScroll) stops the list's auto-scroll.
- `fn_827F8868` UIList::AutoScroll: `+0x50 cmpw` NumData against NumDisplay, `+0x68 stb 1`
  to mAutoScrolling (+0x230), `+0x6C` direction 1, then the pause timer.
- `fn_827F8078` UIList::Poll: `+0x24..+0x54` the timer test and the first Scroll.
- `fn_827F9540` UIList::CompleteScroll (UIListStateCallback thunk): `+0x24` tests
  mAutoScrolling, `+0x4C bl fn_8280E0E0` MaxFirstShowing, `+0x58` compare, `+0x68 neg` reverses
  the direction and `+0x80` re-arms the timer at the end, otherwise `+0xA0 bl fn_8280ED90`
  (UIListState::Scroll) chains the next step.
- All of these and UIListState::Poll/IsScrolling/MaxFirstShowing/SetSelected read fuzzy 100 in
  main's `report.json` (UIListState::Scroll 99.97 fuzzy, mpn 100).
- The data are retail too: `ui/main/gen/credits.milo_xbox` (zlib blocks, decompressed) holds
  `00 00 00 1e`, `00 00 00 01`, `3f 59 99 9a` (num_display 30, grid span 1, speed 0.85 BE) at
  decompressed offset 27758. The 1777 rows come from `ui/locale/locale_credits_keep.dta`
  (1822 top-level arrays before its `#ifdef HX_WII`/`HX_PS3`/`HX_XBOX` blocks; my rough
  preprocessor count did not land on 1777 for any define set, so the row count is native's
  reading, not independently confirmed).

So retail runs the same 1485 s scroll; the native credits are retail-faithful.

## What landed

- **No `src/` change**, so no ab_measure run (nothing to price).
- **Gate A39** in `native/src/rb3_app_gates.cpp` (`rb3_app_check_credits_auto_scroll`, called
  from `main_app.cpp`'s construct-only block): a 60-row UIList with credits' settings
  (30 rows, 0.85 s, pause 0), a real UIListDir with no widgets, AutoScroll, one real
  UIList::Poll, then UIListState::Poll on a 30 fps synthetic clock. It must scroll every frame
  to row 30 (MaxFirstShowing), stop with the direction reversed, and take 760..770 frames
  (30 x 0.85 x 30 = 765; measured 766). It guards the mechanism the brief suspected without
  a 45,000-frame walk.
- `tools/rb3_app_walks/menuwalk/w_credits_end.txt`: the credits walk without the later
  presses, with a header saying it needs at least 46,000 frames.

Gate measurements (construct-only, `RB3_APP_CONSTRUCT_ONLY=1`, via `no610m.sh`):

| binary | A39 line | rc | `[PASS]` lines |
|---|---|---|---|
| base (524c3ed7c) | absent | 0 | 79 |
| branch | PASS, 766 frames, first 30 of 30, dir -1 | 0 | 80 |
| sabotage S1: CompleteScroll's `Scroll(mAutoScrollDir)` chain removed | FAIL, started 0, first 0 | 7 | |
| sabotage S2: AutoScroll leaves mAutoScrolling false | FAIL, armed 0 | 7 | |

## Walks: base against branch

Binaries: base = `native/build/rb3-app` built at 524c3ed7c (sha256 `3363f1a4…`), branch =
the same tree plus the A39 commit (sha256 `b380f1c9…`). Compared with
`tools/rb3_app_det_cmp.py` (all det fields, exit 0 = identical).

| walk | recipe | base end | branch end | det cmp |
|---|---|---|---|---|
| band (det+autoplay, pad 0 walk1 + guitar/drums/bass extra pads) | `rb3-scout/a38/walks.sh` copied to `~/tmp/credits-runs/w`, 90000-frame cap, SECS=540 | 47,380 frames, coop_endgame_screen, rc 4 | 47,385, rc 4 | IDENTICAL over 47,380 common frames |
| solo (det+autoplay, pad 0 only) | same | 47,063, coop_endgame_screen, rc 4 | 47,022, rc 4 | IDENTICAL over 47,022 common frames |
| credits (det, `w_credits_end`) | `runb.sh`, 47,000 frames | main_hub_screen, rc 0 | main_hub_screen, rc 0 | IDENTICAL over 47,000 frames; both enter credits_screen at 1117 and pop it at 45,674 |

Neither the band nor the solo walk ever enters credits_screen (0 `screen=credits_screen`
lines; the 10 `credits_screen` matches are the boot-time panel dump), so "identical up to the
first credits frame" is the whole run. All four game walks stopped on the 540 s wall-clock cap
(rc 4) at about 47,000 frames, not the 90,000-frame cap, because five runs shared the GPU
lock at once. Identity is expected by construction: A39 runs only in the construct-only block
(`RB3_APP_CONSTRUCT_ONLY`), which an `RB3_APP_RUN` walk never enters.

## Not done

- The full native selftest (`tools/native_health.sh --selftest`) was left to the coordinator,
  as briefed.
- No retail run (Xenia) timed the credits; the retail claim rests on the matched code and the
  retail asset bytes above.
- The focus log still prints the highlight row for lists; adding FirstShowing to `ListSel`
  would make an auto-scroll visible, but it adds a log line per scroll step on every list, so
  I left it.
- Start → skip on the credits pause menu was not pressed (still unwalked).
