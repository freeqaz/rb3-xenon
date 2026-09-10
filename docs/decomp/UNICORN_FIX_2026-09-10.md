# Unicorn matched-but-wrong: the survivors adjudicated on RETAIL BYTES (2026-09-10)

Lane **L1-UNICORNFIX**, branch `l1-unicornfix`, worktree `~/tmp/wt-l1unicorn`,
rebased onto main `37dc1ebe` (post U1 landing). Input: S4-UNICORN's screened
worklist (`docs/decomp/unicorn_matched_but_wrong_2026-09-01.md`, 5 TIER1 +
3 TIER2), then — per the coordinator's mid-lane correction — U1-DEEPSCHED's
replacement (`docs/decomp/unicorn_matched_but_wrong_2026-09-10.csv`, the 40
rows that score 100%).

> ⚠ **Mid-lane correction (lane U1-DEEPSCHED):** S4's unicorn fixture stubbed
> MSVC's `__savegprlr_N`/`__restgprlr_N` helpers with `li r3,0; blr`, so every
> function using them clobbered `this` at instruction 2 on BOTH sides and
> re-entered its own tail. **Every emulator verdict S4 produced is void as
> evidence.** The verdicts below were reached on retail bytes *before* that
> correction arrived and **none of them rests on the emulator** — the correction
> changes no row, and it independently explains the SoundTouch row (both sides
> error right after `bl __savegprlr_29`). My own re-run of the unicorn core
> (§3) used the same degraded fixture and is recorded as *informational only*.

## 1. Verdicts on S4's 8 rows

| # | tier | symbol | unit | size | fuzzy/mpn (in) | verdict | evidence (retail bytes) | action |
|---|---|---|---|---:|---|---|---|---|
| 1 | T1 | `??0SoundTouch@soundtouch@@QAA@XZ` | SoundTouch | 132 | 100/100 | **BYTE_IDENTICAL_ARTIFACT** | body identical outside reloc words, 17/17 relocs aligned, every callee same-named; emulator: *both* sides error (`fetch from unmapped` vs `UC_ERR_EXCEPTION`) — a laundered failure, not a divergence (and exactly U1's `__savegprlr` mechanism) | none |
| 2 | T1 | `?Handle@CharWeightable@@$4PPPPPPPM@A@…` | CharWeightable | 12 | 95/100 | **WRONG MAP NAME** — *not* a layout bug | retail thunk at `0x823aec78` is **r3-form**; a `Handle` thunk is r4-form (`DataNode` returns via hidden pointer in r3, `this` is r4) — **214/214** matched `Handle$4` rows are r4-form. The vtordisp displacement `-0x4` is identical on both sides. Callee `fn_823AF2B8` is **word-identical to our `??_GCharWeightable@@UAAPAXI@Z`** (this−0x1c; `~ObjOwnerPtr` at +0xc; `~Object` at +0x1c; `flags&1 → operator delete`; compiler layout: vtordisp 0x18, Object 0x1c). The map carries no `Handle` body for the class at all (folded elsewhere); the orphan thunk name landed on the neighbouring `??_E$4` thunk | map: renamed to `??_ECharWeightable@@$4PPPPPPPM@A@AAPAXI@Z` (`f2d075c4`) → 100/100 |
| 3 | T1 | `?Handle@MultiSelectListPanel@@$4PPPPPPPM@A@…` | meta_band/MultiSelectListPanel | 12 | 95/100 | **WRONG MAP NAME** — *not* a layout bug | same shape at `0x82626be8` → `fn_82627118`: this−0x64 = the class's compiler-reported `-100` adjustor; `~UIPanel` at +0x40 (UIPanel's own vbase convention, 0x3c + vtordisp); `~Object` at +0x64; `flags&1 → delete` — MSLP's deleting dtor with `??_D` inlined (ours calls `??_D` out of line). `ClassName$4` (r4-form) sits at `0x82626bb8`; the only unplaced r3-form `$4` thunk of the class is `??_E`. Retail MSLP ctor confirms the layout (`Object` ctor at +0x64, plain pointer zeroed at +0x40) | map: renamed to `??_EMultiSelectListPanel@@$4PPPPPPPM@A@AAPAXI@Z` (`f2d075c4`) → 100/100; **unit now 15/15 at fuzzy 100** |
| 4 | T1 | `??1AppMiniLeaderboardDisplay@@UAA@XZ` | meta_band/AppMiniLeaderboardDisplay | 284 | 99.93/100 | **REAL_BUG** | one charged site (idx 47, branch dest): retail's `beq` on `mLeaderboard == NULL` jumps **past** the second `SetProvider`; ours made the call unconditionally. Our source was the rb3-Wii DEV oracle verbatim; 360 retail differs. Behavioural: with `mLeaderboard` NULL we re-pointed the list (on a possibly-NULL `mLeaderboardList`) where retail did nothing | source: the re-point moved inside `if (mLeaderboard)` (`ef9cc519`) → 100/100 |
| 5 | T1 | `?finalize@MD5@Quazal@@QAAXXZ` | network/…/MD5 | 228 | 100/100 | **BYTE_IDENTICAL_ARTIFACT** (+ map gap) | body identical outside reloc words, 9/9 relocs aligned. Retail `bl fn_82B45480` where we `bl ?encode@MD5@…` — a **forgiven placeholder**; `fn_82B45480` is visibly `encode` (the byte-unpack loop) but compiled **unoptimized** (args spilled to the home area, loop counters on the stack), 4 B past MD5's pin end in `auto_03_82B4547C_text`. The callee is right; a /Od body cannot pair with our /O1 build | none; handoff §4 |
| 6 | T2 | `?InterpTangent@@YAXABVVector3@@000MAAV1@@Z` | Color (body: `math/Key.cpp`) | 280 | 99.57/100 | **NOT A BUG — permuter-class residual** | 3 charged sites, all commutative `fadds` operand swaps (idx 38/53/67: retail `tmp+vout` on z, `vout+tmp` on x/y). IEEE-identical. The "return value differs" is **r3**, clobbered at idx 7 with the mocked address of the `2.0f` constant, in a `void` function | tried `vout += vtmp` and `Add(vtmp, vout, vout)`: **both byte-identical to the original** — source-inert; reverted |
| 7 | T2 | `??0InvExpInterpolator@@QAA@MMMMM@Z` | Interp | 92 | 100/100 | **BYTE_IDENTICAL_ARTIFACT** | body identical outside reloc words, 14/14 relocs aligned, **0 calls**. The two differing words: the vtable pointer (a mock address) and `+0x1c` = `1.0f` ours vs `NaN` retail — `1.0f` loads through `lbl_820009FC` (zero-filled on the retail leg), so `mInvRun = 0/0` there | none |
| 8 | T2 | `?getKeyImpl@@YAXPAEPAD0@Z` | keygen_xbox | 68 | 100/100 | **BYTE_IDENTICAL_ARTIFACT** | body identical outside reloc words, 2/2 relocs aligned. The charged call is `memcpy_cs(c, uc1, 0x20)` = the recorded `r3=0, r4=0x20000000, r5=0x20` exactly; **r6 is a dead non-argument register** never written in the body (left over from the co-loaded `revealKey`, executed from our obj on one leg and retail on the other). The comparator compares r3–r6 regardless of arity | none |

**Summary of the 8:** 1 REAL_BUG (row 4, fixed) · 2 WRONG MAP NAME (rows 2/3, fixed; they are *not* the virtual-base layout bug the brief hypothesised) · 4 BYTE_IDENTICAL_ARTIFACT (1/5/7/8) · 1 permuter-class byte residual with no behavioural content (6). **Zero layout changes, zero `src/system` changes. All 8 verdicts survive the U1 correction.**

### What contradicts S4's (and U1's) doc

- **The byte-identity screen under-detects, in both lanes.** It compares raw bytes
  (`bytes(db) == bytes(ob)`) including relocation fields and unequal extraction lengths
  (232/228, 92/96, 72/68), so rows 1/5/7/8 read `bytes_identical: 0` although they are
  identical outside reloc words with relocs aligned 1:1. Re-run with reloc words masked
  (§1b), **34 of U1's 40 score-100 rows are byte-identical** — U1's CSV marks **1**.
- **The two `$4` rows are neither "extraction artifacts" (S4) nor a layout bug (brief)**
  — they are map misattributions of the `??_E` deleting-dtor thunk, and the names were
  never on `_bijection_arbitrary`, where such picks are supposed to be declared.
- `return_value` fires on **void** functions (row 6): the comparator reads r3 at return.

## 1b. U1's 40 score-100 rows, screened with the reloc-masked instrument

| bucket | rows | which |
|---|---:|---|
| byte-identical outside reloc words, relocs aligned 1:1 | **33** | all 15 `object_memory` ctors, 14 of 16 `call_count` (incl. MD5::finalize), `??0CharWeightable`, `getKeyImpl`, `?Load@RndBitmap@@`, `?RemoteVocalState@VocalPlayer@@` |
| REAL_BUG, fixed | 1 | `??1AppMiniLeaderboardDisplay` (row 4) |
| wrong map name, fixed | 2 | the two `$4` thunks (rows 2/3) |
| permuter-class register/operand residual — no behavioural content | 4 | `?SustainedGemToKill@TrackWatcherImpl@@` 99.68 and `?KillSustainForSlot@TrackWatcherImpl@@` 99.29 (`mr r6` retail vs `mr r5` ours across a call, both functions — same shape, possibly one systematic cause) · `?IsAvailable@AccomplishmentManager@@` 99.86 (`lwzx r4, r11, r31` vs `r31, r11`) · `?SetFrame@RndGenerator@@` 99.91 (`fadds f0, f0, f31` vs `f31, f0`) |

⇒ U1's "25 never adjudicated" are now adjudicated: **no further REAL_BUG in the 40.**
Two of the byte-identical rows carry forgiven placeholder callees that are the only
place a wrong callee could still hide (`?ThreadGetDir@CacheXbox@@` → `fn_8283D2D8`
twice where we say `CloseHandle`; `?Handle@AccomplishmentManager@@` → three unnamed
retail callees) — an identification backlog item, not a divergence finding.

## 2. Measurements (pre-registered, then measured)

Baseline in the worktree at lane start: `42,305 / 3,774,924 B / 36.843063%`
(objdiff 4.2.8 binary `14ac591a0814e6c9`, ruler `name_check`).

| change | predicted Δfns / Δbytes | in-worktree full build | `ab_measure --pick` (settled) |
|---|---|---|---|
| map (`f2d075c4`): CW +12, MSLP +12, Waypoint −12 (refuted arbitrary pick evicted) | **−1 / +12 B** | −1 / +12 B | **Δmatched −1 · Δhonest −1 · Δcode_bytes +12 · Δcode% +0.000117 pp · Δfuzzy −0.000116 pp**; `none` control +12 B (REAL_PAIRING, expected for a nulling + rename); re-split ran both legs (`renamer_patched=1823`, symbols.txt fixed point); **MultiSelectListPanel reached 100% on the all-rows-fuzzy ruler** (121→122) |
| dtor (`ef9cc519`) | **0 / +284 B** | 0 / +284 B | **Δmatched +0 · Δcode_bytes +284 · Δcode% +0.002774 pp · Δfuzzy 0**; `none` control +284 (source patch, expected); 1 recompile in leg B |
| both | **−1 / +296 B** | `42,304 / 3,775,220 B / 36.845950%` — exact | (sum) |

The map change is **deliberately net −1 function**: the evicted Waypoint row (`0x823dc070`)
was a bijection-arbitrary name whose callee is not a destructor — accuracy over headline.

Raw `ab_measure` logs: `~/tmp/l1_ab_map.log`, `~/tmp/l1_ab_dtor.log`.

## 3. Instrument notes (for the next unicorn lane)

- Screen byte identity with **relocation words masked** and require reloc offsets to
  align; raw `bytes()==bytes()` misses 33 of 40 here.
- `call_arg` compares r3–r6 regardless of the callee's arity — a stale r6 on a 2/3-arg
  callee is not evidence (row 8). Read the callee's arity first.
- `return_value` on a `void` function is r3 garbage (row 6).
- Any `$4` thunk row: the **register** (r3 vs r4) identifies aggregate-returning virtuals
  (`Handle`/`ClassName`) vs the rest; a wrong-register name is a map defect, not a layout
  one. The 12-byte body is reloc-identical across every class, so bijection picks are
  blind to it.

## 4. Handoffs (evidence gathered, not acted on)

1. **`0x823AF2B8` = `??_GCharWeightable@@UAAPAXI@Z` (proven word-identical)** sits inside
   **CharPollGroup's** pin (`0x823AF210–`), 0xA8 in, between two CharPollGroup thunks.
   Naming it needs re-homing `0x823AF2B8–0x823AF310` into CharWeightable (splits edit;
   +88 B if paired; re-homing is NOT metric-neutral — A/B it).
2. **`0x82627118` (MSLP's deleting dtor, `??_D` inlined)** is pinned as a deliberate
   0x58-byte NewAwardPanel block (`0x82627118–0x82627170`) directly before
   `FinishLoad@MultiSelectListPanel` at `0x82627170`. Identity is MSLP's by layout; our
   `??_G` calls `??_D` out of line, so it would not pair at 100 without an inline-policy
   change.
3. **`0x823dc070` (Waypoint unit) — nulled.** Was `??_ECharWeightable@@$4` on
   `_bijection_arbitrary`; every Waypoint-own `$4` thunk is already placed, so its true
   owner is a COMDAT thunk first-defined by Waypoint.obj (our Waypoint.obj carries
   `ShadowBone`/`WorldDir`/`Character`/`RndAnimatable` thunks). Callee `fn_823DB8C0` takes
   ≥3 GPR args and loops over 0xc-byte elements — not a dtor.
4. **MD5's pin ends 4 B before `encode`/`decode`, which are /Od-shaped inside a TU whose
   other functions match at /O1** (update/finalize/init 100%). That is *function*-granular
   optimization heterogeneity (a `#pragma optimize` candidate) — flagged, unverified, and a
   counter-example to "`/Od` is object-granular" if it holds. Naming `0x82B45480`
   `?encode@MD5@Quazal@@CAXPAEPBII@Z` is right on bytes but buys a 0% row; it would convert
   finalize's forgiven callee into a checked one.
5. `?Handle@CharWeightable@@…` / `?Handle@MultiSelectListPanel@@…` bodies and thunks are
   absent from retail's TU regions (folded elsewhere); their addresses are unidentified.
   Our `Handle$4` thunks are now base-only (unscored).
6. The two `TrackWatcherImpl` residuals share one shape (`this` kept in r6 vs r5 across
   `GetGemInProgressWithSlot`) — worth one look for a systematic cause before being filed
   as permuter-only.

## 5. Native gate (last action; `src/band3` was touched)

Run in `~/tmp/wt-l1unicorn` after the rebase and the final full build, as the last build action of the lane (log `~/tmp/l1_native_gate.log`):

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

## 6. What I did NOT do

- Did not run the corrected (U1) unicorn fixture and did not rebuild the C hook; every
  verdict here is on retail bytes.
- Did not touch any class layout or header (the brief's hint was refuted on bytes).
- Did not name `fn_82B45480`, `0x823AF2B8`, `0x82627118`, or re-name `0x823dc070`
  (handoffs above; each needs a splits move or is not recoverable from bytes).
- Did not run the permuter on the five operand/register residuals (OFF by directive).
- Did not adjudicate the forgiven placeholder callees inside byte-identical rows
  (identification backlog, §1b).
