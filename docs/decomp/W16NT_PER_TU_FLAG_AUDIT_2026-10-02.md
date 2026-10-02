# W16-NT — per-TU compile-flag audit (2026-10-02)

**Branch** `w16-nt`, rebased onto main `1023c0358`. **Ruler** `name_check` (graded). The permuter was not run.

W16-NS found that `keygen_xbox.cpp` needed `/Od /Os`, not bare `/Od`, and that one flag closed 13 rows an earlier lane had
filed as a register wall. This lane asked whether any other TU is compiled with the wrong per-TU flags. It audited every
`extra_cflags` entry in `config/45410914/objects.json` (201 entries), and it screened every unit for the two signatures a
wrong flag would leave: a unit-wide register/scheduling residue, and retail code shaped like `/Od` in a unit we build `/O1`.

**Result: one change.** `network/Core/Scheduler.cpp` is `/Od` in retail and we built it `/O1`. It now carries
`/Od /Oi- /EHs-c- /Ob1`. Whole-binary Δ is 0 functions / 0 B, as predicted. No other entry is wrong. 22 `/D` gate entries
are dead (byte-identical with and without the gate). They are listed in §4 and were left in place.

## 1. Whole-binary A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-nt-ab --patch <git diff main w16-nt -- . ':!docs'>`, run in a fresh
`setup_worktree.sh` worktree at main `1023c0358`. objdiff-cli `sha256:c1b7d952`, stable across legs. Run dir
`~/tmp/wt-w16-nt-ab/.ab_measure_runs/20261002-181815-w16nt_branch-1958508/`.

```
leg A: matched=51491 masked=24640 honest=26851 code%=54.929226  (recompiles: 0, settled)
leg B: matched=51491 masked=24640 honest=26851 code%=54.929226  (recompiles: 1, split=0, patch_steps=6, settle iterations: 2)
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.003798pp   (legA 60.402040 -> legB 60.405838)
units at 100% [mpn ruler]: legA 485 -> legB 485  (Δ+0)
```

**Prediction, written before the run:** Δ0 functions and Δ0 B, because no Scheduler row is at fuzzy 100 under either flag set
(the unit is 0 of 8,500 B at 100 both ways). **Measured: Δ0 / Δ0.** Leg B recompiled exactly one TU (Scheduler).

**Control: the same flag set on a TU retail compiled `/O1`.** The identical four flags were put on `system/math/Interp.cpp`
(HMX math, `/O1` in retail) and run through the same tool (run dir `…/20261002-182229-w16nt_control_interp-1983980/`):

```
Δmatched=-15  Δhonest=-15  Δcode%=-0.011786pp  Δcode_bytes=-1208   (recompiles: 1)
```

That is every matched byte in Interp (1,208 → 0). So the instrument sees a per-TU flag change at full strength, and the flag
set does not raise scores wherever it is applied. On Scheduler it is right because retail's bytes say so (§2), not because a
number went up.

**Row-level diff of the archived legs** (`~/tmp/w16nt/rowdiff.py`): **1 row up, 3 rows down, all in Scheduler.** No row
appeared or vanished, and nothing outside Scheduler moved.

| row | B | fuzzy A → B | reason |
|---|---:|---|---|
| `??0Scheduler@Quazal@@QAA@EPAVSchedulerWorkerThread@01@@Z` | 948 | 35.384 → **84.519** | the flag fix |
| `fn_82AC5BB0` | 44 | 62.636 → 0 (now unpaired) | **false pairing removed.** In leg A it paired by byte signature with our ctor's EH unwind funclet (`subi r31,r12,0x70; lwz r11,0x84(r31); addi r3,r11,0x14; bl ~EventHandler`). Retail's is an ordinary `/Od` member function: `stw r3,0x74(r1)`, then a call through `this->0x2c`. Under retail's EH-off mode our object has no funclets, so the row has no partner. |
| `fn_82AC5F10` | 44 | 62.636 → 0 (now unpaired) | same: leg A's partner was the funclet that calls `~ProfilingUnit`; retail calls through `this->0x168` |
| `fn_82AC7560` | 56 | 76.786 → 62.500 | **false pairing in both legs.** Both legs pair it by byte signature with our `s_csGlobalSystemLock` dynamic initializer (`CriticalSection(2)` + `atexit`). Retail's is an `/Od` member function on `this+0x160`. The drop is the initializer's instruction order changing under `/Od`. |

None of the three is a real function getting worse. Each is an anonymous retail row that had been byte-matched to an unrelated
function of ours.

## 2. Scheduler: retail is `/Od`, EH off, `/Ob1`

Retail `fn_82AC57F0` (the ctor) opens `stw r3,0xd4(r1); stb r4,0xdf(r1); stw r5,0xe4(r1)`, which spills every incoming
argument to the caller's home area. It then reloads `this` from `0xd4(r1)` before every member store, with a descending
r11→r3 volatile chain. That is bare `/Od`; keygen's `/Od /Os` reuses r11 instead. 22 of the unit's 25 retail functions
carry this shape (detector in §3).

**EH is off.** Under `/Od` with EH on, MSVC gives a function with EH state an r31 frame pointer. Our EH-on build of this ctor
opens `std r31,-0x10(r1); subi r31,r1,0xa0` and addresses everything off r31. Retail has no r31 frame in any of the unit's
26 functions and addresses off r1. The same retail count separates the existing Quazal TUs exactly as their per-TU flags
already do: KeyedChecksumAlgorithm 2, DuplicatedObject 2 and ChecksumAlgorithm 1 r31-frame functions (EH on), and
MemoryManager, BandwidthCounter, Scheduler and keygen 0 (EH off).

Unit sweep (one TU recompiled per variant, full `ninja-locked`, `report.json` wiped; ctor fuzzy):

| flags | ctor | note |
|---|---:|---|
| (`/O1`, before) | 35.384 | |
| `/Od` | 53.342 | |
| `/Od /Oi-` | 53.342 | `/Oi-` inert |
| `/Od /Os` | 52.878 | |
| `/Od /Ob1` | 81.544 | `/Ob1` load-bearing |
| `/Od /EHs-c-` | 55.515 | |
| `/Od /EHs-c- /Ob1` | **84.519** | |
| `/Od /Oi- /EHs-c- /Ob1` | **84.519** | chosen: same set as MemoryManager/BandwidthCounter; `/Oi-` inert here |
| `/Od /Os /EHs-c- /Ob1` | 82.156 | `/Os` is not keygen's lever here |

What is left in the ctor is source, not flags. Retail's frame is 0xc0 against our 0xa0, and retail stores three zero
fields through an inlined helper that takes its value as a stacked parameter (`li r7,0; stw r7,0x54(r1); lwz r6,0x54(r1);
stw r6,0(r5)`), where ours stores directly. That needs the helper's inline shape, so it was not attempted. The other 21 rows
are anonymous and unpaired, which is identification work.

## 3. Screens that found nothing else

**`/Od`-shaped retail code in units we build `/O1`.** For each retail function in every `build/45410914/asm/*.s` (keyed on
`.fn`, never the address column), the screen flags an incoming argument register r3–r10 stored at or above `frame+0x14`
off r1 in the first 10 instructions **and** no use of r14–r30 or `__savegpr*` anywhere in the body. The second clause is what
makes it work. Without it, `/O1` functions that spill an address-taken by-value argument (`stb r5,0x97(r1)` for a predicate)
or varargs (`std r4…r10`) flag too: AccomplishmentPanel read 37/387 and MemTracker 14/58.

| population | functions | flagged |
|---|---:|---:|
| `auto_*` units (mostly the Quazal block) | 9,508 | 3,127 |
| named units | 39,847 | 217 |

Named units in order: **DuplicatedObject 76/95** (`/Od` already), **Scheduler 22/25** (fixed), **keygen 18/19**
(`/Od /Os` already), trie 4/6, StringConversion 1/1, then every other unit ≤6 functions and ≤8% of the unit. That tail is
small leaf functions. The two non-flag hits:

- `trie.cpp` has a 900 B `.text` pin at `0x82AB0F70`, inside the Quazal block. Its 6 rows are anonymous at 0. That is the
  bad-pin class CLAUDE.md already flags (engine units claiming functions in the Quazal block), not a flag question.
- `StringConversion.cpp`'s pin is 64 B (`0x82AE5FA8`–`0x82AE5FE8`) of an `/Od` function that branches to `0x82AE60AC`. dtk
  has carved one `/Od` leaf into five slivers (0x40/0x4C/0x30/0x44/0x10), presumably because a red-zone `/Od` leaf has no
  `.pdata`. Under `/Od` and `/Od /Oi- /EHs-c- /Ob1` the row stays at fuzzy 0, so no flag can score until the carve is fixed.
  The flag was not changed.

**Unit-wide register/scheduling residue.** W16-NP's charge classifier (`diffall.py`, W16-NA's rules) was run fresh over all
3,395 sub-100 rows in pairable units (890,020 B). Register/stack/schedule-only rows total 211 (REG_ONLY 156 / 92,804 B,
STACK_REG 13 / 6,964 B, SCHED 42 / 24,776 B), and **no unit holds more than 4 of them**. The few units where they are the
whole residue are almost fully matched (CharBones 3 gap rows of 47, Interp 3 of 15). A wrong per-TU flag would hit most of
a unit's rows, so none of these looks like one. W16-NS's keygen signature (uniform descending chain against retail's single
reused r11) also appears in no other unit's gap rows.

The units whose few gap rows are all of one shape were swept against the plausible alternatives anyway:

| unit | `/O1` (current) B @100 | `/Ot` | `/Ob1` | `/Oi-` | `/fp:precise` |
|---|---:|---:|---:|---:|---:|
| Interp | **1,208** | 16 | 676 | 552 | 1,208 |
| FFT | **488** | 16 | 488 | 488 | 488 |
| BoxMap | **740** | 48 | 712 | 740 | 740 |
| IPP_basicmath_xbox | **88** | 0 | 88 | 88 | 88 |
| CharBones | **6,976** | 612 | 5,912 | 5,704 | 6,976 |

The default is best or tied everywhere. `/fp:precise` is inert in these five TUs; the −47 in CLAUDE.md's whole-binary
`/fp:precise` leg is elsewhere.

## 4. Every `extra_cflags` entry

### Codegen flags

The six existing Quazal TUs were re-swept **individually**. W16-NS had tested `/Os` on all six together only. Each variant
went onto all six in one build and each unit was read separately (they are independent TUs). Each cell is B at fuzzy 100,
with weighted fuzzy where B does not separate.

| TU | current | `/Od` | `/Od /Oi-` | `/Od /Ob1` | `/Od /Oi- /Ob1` | `/Od /EHs-c-` | `/Od /EHs-c- /Ob1` | `/Od /Oi- /EHs-c- /Ob1` | verdict |
|---|---|---:|---:|---:|---:|---:|---:|---:|---|
| MD5 | `/Od /Oi- /Ob1` | 496 | 724 | 496 | **6,792** | 496 | 496 | **6,792** | right (EH inert; `/Oi-` and `/Ob1` both load-bearing) |
| KeyedChecksumAlgorithm | `/Od /Oi-` | **532** | **532** | **532** | **532** | 256 | 256 | 256 | right (EH on; retail has 2 r31-frame fns) |
| DuplicatedObject | `/Od` | **312** | **312** | 204 | 204 | 272 | 164 | 164 | right (EH on, no `/Ob1`; retail has 2 r31-frame fns) |
| MemoryManager | `/Od /Oi- /EHs-c- /Ob1` | 24 | 24 | 24 | 24 | 24 | 24 | 24 | **metric-inert under all 7.** Retail has 0 r31-frame fns, consistent with EH off. Kept. |
| BandwidthCounter | `/Od /Oi- /EHs-c- /Ob1` | 0 | 0 | 136 | 136 | 124 | **332** | **332** | right (EH off, `/Ob1`) |
| ChecksumAlgorithm | `/Od /Oi- /Ob1` | wf 99.53 | 99.53 | 99.53 | 99.53 | 85.07 | 85.07 | 85.07 | right on EH (on; retail has 1 r31-frame fn). `/Oi-`/`/Ob1` inert. |

⇒ The Quazal flags really differ per TU. They are not one library-wide set. The EH split is confirmed independently by retail's
r31 frames (§2), so it is not a fitted pattern. `keygen_xbox` (`/Od /Os`) was not re-swept; W16-NS's evidence is retail's
reused r11 and is unit-wide (18/19 functions `/Od`-shaped).

| entry | carriers | test | verdict |
|---|---:|---|---|
| `/TP` | 6 (5 json-c, tomcrypt `ctr.c`) | build with the flag removed, and with `/TC` (identical) | **load-bearing** on json_object (2,696 vs 620 B), json_tokener (456 vs 252), ctr (676 vs 352); inert on linkhash/printbuf/arraylist. −23 fns whole-binary without it. Kept. |
| `/Y-` | 1 (MoviePanel) | removed | metric-inert. It is a PCH opt-out added alongside `MAP_0x1C`, which is dead in MoviePanel (below). Kept: no evidence either way. |
| `/I src/system/net/curl/include` | 4 | — | include path, needed to compile; no codegen meaning |
| `[]` (empty) | 6 | — | nothing to test |

### `/D` source gates

One build per gate with the gate removed from every carrier, each carrier unit read against a saved baseline report
(`~/tmp/w16nt/gateablate.py`, run on `78d7d8f02` before the rebase). Carriers that **do not compile** without their gate
were found with `ninja -k 0` and kept gated, since they need it by construction. Every metric-inert carrier was then run
through `tools/gate_liveness.py` (both legs `OBJCACHE=off`, same `/Fo`, compares TU-owned `.text` symbols) to separate
**dead** (byte-identical) from **live but unmeasured**. A run with the wrong spelling (`--flag /D…`, which the tool prefixes
again) returned INERT on every unit, which served as a null.

| gate | carriers | NEEDED (removal costs) | compile-required | live, fuzzy-only (keep) | **dead** | **WRONG** (removal gains) | whole-binary Δ without it |
|---|---:|---:|---:|---:|---:|---:|---|
| `RB3_HANDLE_LOCAL_STATIC` | 159 | 149 | 6 | 1 (SongSectionController) | **3** | 0 | −1,628 fns / −273,780 B (153 ablated) |
| `RB3_SYNCPROP_LOCAL_STATIC` | 37 | 33 | 2 (Label3d, TrackWidget) | 0 | **2** | 0 | −173 / −50,344 B (35 ablated) |
| `RB3_STRIP_CHEAT_HANDLERS` | 12 | 6 | 0 | 0 | **6** | 0 | −6 / −7,840 B |
| `RB3_MAP_0x1C` | 11 | 0 | 0 | 0 | **11** | 0 | 0 / 0 |
| `RB3_NOTIFY_ONCE_EVAL` | 11 | 8 | 0 | 3 (ShadowMap, CharCollide, TexRenderer; liveness LIVE) | 0 | 0 | −10 / −4,556 B |
| `RB3_LOG_NO_EVAL` | 2 | 2 | 0 | 0 | 0 | 0 | −24 / −6,892 B |
| `RB3_NO_WII_META_MEMBERS` | 1 | 1 | 0 | 0 | 0 | 0 | −13 / −9,620 B |

**No gate entry is wrong**, and no ablation moved any unit outside its carriers.

**Dead gate entries (22), byte-identical `.text` with and without the gate:**
- `MAP_0x1C` on **all 11**: TourProgress, CharacterCreatorPanel, Campaign, ChooseColorPanel, Tour, MoviePanel, LessonMgr,
  InterstitialMgr, LightPresetManager, RockCentral, TourPerformer. The carriers do include the padded `stl/_map.h`, so the pad is
  applied but reaches no TU-owned code. DJ-3 measured six of these as NEEDED (TourProgress −20, CharacterCreatorPanel −16, …),
  so the maps they padded have since been restructured. `SongMgr.h`, for one, records converting its maps to `hash_map` and
  dropping the gate.
- `STRIP_CHEAT_HANDLERS`: AccomplishmentPanel, Campaign, CustomizePanel, ProfileMgr, RockCentral, QuestFilterPanel.
- `HANDLE_LOCAL_STATIC`: Label3d, SessionSearcher_Xbox, NetSession_Xbox.
- `SYNCPROP_LOCAL_STATIC`: rndobj/Cam, bandobj/ReviewDisplay.

They were **not removed**. A dead gate is not a wrong flag: removing it is Δ0 by construction and changes no codegen, and
each one records an ODR split that retail made and that may matter again if those TUs' sources change. A cleanup lane
can drop them on this evidence alone.

## 5. Notes for the next lane

- **To screen for `/Od`, require "no non-volatile register" as well as the home-area spill.** The spill alone fires on any
  `/O1` function with an address-taken by-value argument or varargs. With both clauses, HMX units sit at ≤6 functions each.
- **The EH setting of an `/Od` TU can be read off retail directly.** An r31 frame pointer in any function means EH on; none
  in a unit that our EH-on build gives r31 frames means EH off. It agreed with the metric on all seven `/Od` TUs.
- **Anonymous rows in an `/Od` unit pair with our EH funclets by byte signature.** `mflr; stw r12; stwu r1,-0x60; …; bl`
  matches a 44-byte `/Od` accessor as easily as a funclet. Expect such rows to "go down" when the EH flag becomes correct.
- **`MAP_0x1C` is dead everywhere it is applied.** Re-measure a gate with `gate_liveness.py` before citing it as needed.

## 6. Gates

Branch tip after the full `./tools/ninja-locked`: `[patch-state] OK: tree is a fixed point of 6 post-compile passes`;
`1055/1055 declared compiled objects pair with a target`. Native gate: see §7 (run last).

## 7. Native gate

(filled in after the run)

## 8. Not done

- Not merged to main.
- Scheduler's ctor (84.5) and its 21 anonymous rows were not worked. The ctor needs the inline helper's shape, and the rest is
  identification.
- The dead gate entries, the `trie.cpp` pin and the StringConversion carve were recorded, not changed.
- `keygen_xbox` was not re-swept. The `/D` gates were not tested for *missing* carriers (adding a gate to TUs that lack it),
  because the header gates need `/Y-` on PCH dirs, which is its own perturbation.

Scratch: `~/tmp/w16nt/` (`sweep.py`, `sweep2.py`, `gateablate.py`, `oddetect.py`, `unitagg.py`, `cls.json`, `gl_*.txt`,
`ablate*.txt`, the A/B patches and logs, `legA.json`/`legB.json`).
