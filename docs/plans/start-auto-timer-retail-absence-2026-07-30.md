# START_AUTO_TIMER is absent from retail — proven, gated, metric-neutral

**Lane BS-4, 2026-07-30.** Base: main `51e61cf7`. Worktree `~/tmp/laneBS4/wt`,
branch `laneBS4`. Change: `src/system/os/Timer.h` (one hunk).

**One-line verdict: the hypothesis is TRUE and now proven from the retail
binary — but the measured yield is `axis-1 −2`, i.e. ZERO within the ±2
split-churn floor. Land it for correctness, not for points.**

---

## 1. What was claimed, and what was actually true

A previous lane observed second-hand that `START_AUTO_TIMER` is live in our
build, absent from retail, and that 11 paired functions touching it sit at
≤76%. All three parts reproduce. The prior docs were split on this:

- `docs/decomp/playbooks/nearmiss-harvest.md:81-87` correctly said **our**
  matching build does expand the macro (`src/macros.h:3` force-defines
  `MILO_DEBUG` unconditionally, for the `MILO_ASSERT` family). It also says
  *"Any doc claiming START_AUTO_TIMER_CALLBACK is (void)0 in the matching build
  is wrong."* That statement was about **our** build and remains correct; it is
  **not** a claim about retail, and should not be read as one.
- `docs/plans/decomp-state-2026-07-19.md:279` noted `BandCharacter::Poll` has no
  `START_AUTO_TIMER` in retail — a single-function retail observation, never
  generalized.
- `src/band3/bandtrack/TrackPanel.cpp:55-65` already carried a hand-rolled
  per-TU `#undef START_AUTO_TIMER` with the same reasoning, for one TU.

So the vein was half-dug in three places and never joined up.

## 2. Retail-side proof (from `orig/45410914/band.exe`, not from the oracles)

`band.exe` is the decompressed retail PE. Section table confirms the memory
note: `.text` VA `0x82270000`, raw `0x00264E00` → the `.text` raw delta is
`0x8200B200`, **not** `0x82000000`.

**(a) String-literal absence.** The macro's argument is a symbol-name literal.
27 of 28 distinctive names are absent from retail's `.rdata`: `char_poll`,
`char_draw`, `crowd_set3d`, `crowd_iter`, `crowd_set`, `crowd_draw`, `psysmove`,
`updateworldxfm`, `lipsyncdriver`, `char_hair`, `world_reflect`,
`sound_mgr_poll`, `hud_track_poll`, `hud_track_draw`, `game_poll`, `cc_poll`,
`spotlight_xfm`, `world_draw`, `world_poll`, `phys_mgr_poll`, `cam_switch`,
`bink_audio`, `voice_beat`, `draw_natal_buffer`, `ui_poll_raw`,
`draw_light_approx`, `light_approx_poll`. The lone `faceservo` hit is the milo
object path `face.faceservo`, not the timer symbol.
*Positive controls fire* (`CharHair` 3, `Crowd` 15, `spotlight` 22,
`BandCharacter` 3, `TrackPanel` 6), so the absence is real, not a broken grep.
Also `AutoTimer` = 0 and `Character.cpp` = 0 — retail stripped assert paths.

**(b) Call-site census — the decisive test.** Decoded all 197,239 `bl`
instructions in retail `.text` and resolved their targets. `AutoTimer::GetTimer`
(`0x82511A28`) has exactly **THREE** callers binary-wide:

| caller | what it is |
|---|---|
| `0x82410BE0`, `0x82410C1C` | inside `Rnd::UpdateRate` — `Rnd.cpp` writes `AutoTimer::GetTimer("cpu"/"draw"/"overlays")` **by hand**, not via the macro (see `src/system/rndobj/Rnd.cpp:529-552`) |
| `0x8273CF30` | inside `fn_8273CEF0`, an unpinned Rnd_Xbox tail — **also a plain direct call** |

This tree has ~45 macro sites in 28 TUs. Had the macro expanded, retail would
show ~45+ callers. It shows zero.

**(c) The third site, settled three ways.** `0x8273CF30` was the one candidate
for "retail kept the macro somewhere". It is **not** a macro expansion:
- the string literal is `"cpu"` (`0x820010B0`) — the *same* literal the Rnd.cpp
  site uses — not `"draw"`, which the Rnd_Xbox macro site would need;
- no `50.0f` (`0x42480000`) anywhere in the function, no 4-word AutoTimer
  object, no `MainThread()`/`sDepth++`, no `Timer::Start()`;
- retail's own EH data is conclusive: `except_record_8273CEF0` →
  `FuncInfo @ 0x821026C8` has `maxState == 1`, and that single unwind action
  (`0x8273D07C`) is the **static-guard reset**. A macro expansion needs a
  *second* state for the `~AutoTimer` funclet calling
  `Timer::Stop`/`CyclesToMs`/`AutoGlitchReport::EndExternal`. There is none.
  (The preceding function has two genuine dtor funclets, so this binary does
  emit them when they exist.)

Curiosity worth recording: that static `Timer*` at `0x82E04FF8` is **written and
never read** anywhere in the binary — a dead leftover initializer.

⇒ **Retail contains zero `START_AUTO_TIMER` expansions.** The correct scope is
global; no per-TU exclusion is warranted.

## 3. Blast radius — the honest, post-filter numbers

Site count is not defect count. Measured:

| | count |
|---|---|
| raw grep sites | 45 |
| distinct enclosing functions | 37 |
| **paired in `report.json`** | **14** |
| …of which <100% | 11 (matches the second-hand "11 at ≤76%") |
| …already at 100% | 1 — `TrackPanel::Draw`, the TU that *already* had the `#undef` |
| **not paired at all → cannot move the metric** | **23** |

`TrackPanel::Draw` sitting at a strict 100% under the pre-existing per-TU
override was the tell before any build was run.

★ **The ceiling here is gated by map coverage, not by the fix.** 23 of 37
enclosing functions (`Character::Poll`, `Character::DrawShowing`, `Movie::Poll`,
`TaskMgr::Poll`, `RndDrawable::Collide`, `VorbisReader::TryDecode`, the four
other `WorldCrowd` methods, …) are simply absent from `report.json`. They carry
the same defect and get the same fix for free — but only once they are named.

## 4. The change

`src/system/os/Timer.h`: `#ifdef MILO_DEBUG` → `#if defined(MILO_DEBUG) &&
defined(HX_NATIVE)` on `START_AUTO_TIMER_CALLBACK`, keeping the real macro for
the native build. Chosen over 28 per-TU `#undef`s because the evidence is global
and scattered pragmas rot. Note `os/Timer.h` is inside the PCH closure
(`Object.h → MessageTimer.h → os/Timer.h`), so this cascades to the ~281
PCH-eligible TUs plus 78 direct includers — **measured on full rebuilds, both
legs, same split, `symbols.txt` restored and `report.cache` removed each leg.**

## 5. Measurement — bank the number, not the prediction

Predicted +2..+6. **Got −2.**

```
BASE    matched 40898  masked_equal 1509  axis1 39389
TREAT   matched 40897  masked_equal 1510  axis1 39387
DELTA   matched -1     masked_equal +1    AXIS1 -2      (= 0 within ±2 churn)
```

Yet every timer-bearing paired function improved, several enormously:

| function | before → after |
|---|---|
| `WorldDir::Poll` (unit `default/MidiSynth`) | 64.20 → **100.00** |
| `CharLipSyncDriver::Poll` | 45.66 → 77.07 |
| `Spotlight::UpdateTransforms` | 71.37 → 95.35 |
| `CamShot::SetFrame` | 66.09 → 88.28 |
| `LightPreset::SetFrameEx` | 70.21 → 83.57 |
| `UIManager::Poll` | 43.44 → 56.18 |
| `Spotlight::DrawShowing` | 68.26 → 79.34 |
| `WorldCrowd::DrawShowing` | 70.86 → 76.50 |

**Why the metric didn't move:** only 3 functions crossed into 100%
(`WorldDir::Poll`, `fn_82803ADC`, `fn_824E57B4`) while 4 anonymous near-100
neighbours in the same TUs slipped out (`fn_828058F0`, `fn_8236EC98`,
`fn_82805758`, `fn_824E59DC`, each −0.1/−0.2). Those are EH-funclet re-pairing
churn: deleting the `~AutoTimer` funclets changes which funclets objdiff's fuzzy
byte-fallback pairs to. Net at-100%: −1.

**The one apparent regression is a map mispair, not counter-evidence.**
`DxRnd::DoWorldEnd` reads 55.73 → 19.55, but retail `0x822AED20` does `mr r31, r4`
— a *parameter* register in a `void` function — with 26 target-side deletes.
Arg-only + wrong arity = mispair. Its percentage is meaningless either way, and
it is not grounds to exclude Rnd_Xbox.cpp. (It is also a live example of the
documented pct inversion: shrinking our N with S fixed tanks the score.)

## 6. What I did NOT do, and what's left

- **Did not narrow the gate to only the paying TUs.** Restricting it to the ~8
  units with paired sites would likely dodge some of the 4 funclet drops, but
  the evidence says retail expanded the macro *nowhere*; narrowing to chase
  metric artifacts would be gaming the metric against known-correct source.
  Flagged as a coordinator option, deliberately not taken.
- **Did not chase the 8 improved functions to 100%** — that is per-function body
  work for a body-port lane. They are now much better near-miss candidates than
  they were (`Spotlight::UpdateTransforms` at 95.35 and `CamShot::SetFrame` at
  88.28 are genuinely close).
- **Did not investigate the 4 funclet drops individually.**
- **Did not verify the identity of `fn_8273CEF0`** — not needed for the verdict.

## 7. For the next lane

- ⛔ **Do not re-hunt "is START_AUTO_TIMER in retail".** It is not. Settled by
  string absence + a 197k-`bl` call census + retail EH `FuncInfo`. Re-deriving
  this costs a lane and yields the same answer.
- ★ This is a **zero-cost correctness fix that pre-pays**: as identification
  lands names on the 23 unpaired enclosing functions, they arrive already free
  of timer divergence instead of each needing rediscovery.
- ★ Sibling lever, still open: `MESSAGE_TIMER` in `BEGIN_HANDLERS` — same
  root cause (`MILO_DEBUG` force-defined tree-wide switching on instrumentation
  retail compiled out). See `tools/static_symbol_finder.py` "LEVER B". A global
  `MILO_DEBUG` flip is layout-coupled and needs its own dedicated wave.
- ⚠ Method note that generalizes: **retail's `.pdata`/`FuncInfo` unwind-state
  count is a cheap, strong discriminator** for "was this RAII object really
  constructed here?" — `maxState` counts destructors in scope. Faster and more
  reliable than reading the code.
