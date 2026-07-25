# SaveLoadManager::SetState — dedicated reconstruction project

**Worktree:** `~/tmp/wt-laneC-setstate`, branch `laneC-setstate` (base main `7222d88f`).
**Target:** `fn_82550880` @ `0x82550880`, size `0x1000` (1024 instrs), 106-case switch.
**Our symbol:** `?SetState@SaveLoadManager@@IAAXW4State@1@@Z` (protected, `IAA`).
**Reveal entry:** `"0x82550880": "?SetState@SaveLoadManager@@IAAXW4State@1@@Z"` is committed
in `scripts/target_symbol_map.json` — keep it; it is what lets objdiff diff the giant by
name. (Strict-neutral until the body matches.)

Prior context: `docs/plans/saveloadmanager-port-log-2026-07-20.md` (class-layout
reconstruction, wave-3 body ports).

## 0. Session ledger — session 1 (2026-07-24)

Baseline main `7222d88f` = **25,133** strict / SetState **45.0%** / frame **+0x840**.

| commit | change | SetState | frame Δ | whole-binary | lost |
|---|---|---|---|---|---|
| `4114919f` | `TheDebug.Fail/Notify(MakeString)` → `MILO_FAIL/MILO_NOTIFY` | 56.9 | **+0x20** | 25,135 | 0 |
| `eee776e4` | shared dialog block + function-local static Symbol + `mState` re-read | 56.8 | +0x20 | 25,135 | 0 |
| `190a53ab` | `BandProfile *mProfile` moved BEFORE `mUploadProfiles` (0x30) | — | +0x20 | 25,138 | 0 |
| `d7a570f3` | port cases 3,4,b,43/44,45,54,5a,5e,64 from Ghidra | 70.9 | **0 (EXACT)** | 25,140 | 0 |
| `52b95da8` | **5 function-local static Symbols** + SelectDevice arg order + GetProfile-by-pad | 84.9 | +0x10 | **25,170** | 0 |
| `a254b6a0` | renumber tail states 0x6d/6e/6f → 0x69/6a/6b; ManualDeleteStart → 0x4b | 87.0 | +0x10 | 25,170 | 0 |
| `43c85c18` | hoist static Symbol decls above `GetGlobalOptionsSize` | **88.6** | +0x10 | 25,170 | 0 |

**Net session: +37 whole-binary strict (25,133 → 25,170), 0 lost. SetState 45.0% → 88.6%.
SaveLoadManager unit 37/405 → 74/405 at strict 100.**

### The two force-multipliers found

**(1) The `MakeString` 0x800 phantom frame.** `FormatString` carries
`char mFmtBuf[0x800]` (`src/system/utl/MakeString.h:20`). Our Wii-derived source called
`TheDebug.Fail(MakeString<int>(...))` **directly** (30 sites in this TU), inlining a 2 KB
buffer into every such function's frame. Retail stripped debug-output emission, so the
`MILO_FAIL` / `MILO_NOTIFY` macros (`((void)(__VA_ARGS__))`, `src/system/os/Debug.h:112+`)
are the correct source form. This alone took the frame from +0x840 to +0x20.
> **Generalizable — worth a fleet-wide scan.** Any ported TU containing
> `TheDebug.Fail(MakeString`, `TheDebug.Notify(MakeString`, `TheDebug.Warn(MakeString`
> or `TheDebug << MakeString` carries the same phantom 0x800 in every affected frame,
> which blocks that function AND its EH funclets. `grep -rn "TheDebug\.\(Fail\|Notify\|Warn\)(MakeString\|TheDebug << MakeString" src/`

**(2) Local-static count drives the callee-saved register range → the funclet cascade.**
Retail's SetState has **five** function-scope `static Symbol`s sharing ONE guard word at
`0x82DFDA28` (bits 1/2/4/8/0x10; storages `0x82DFDA24/20/1C/18/14`). With more than one
local static, MSVC pins the shared guard word's address in a **callee-saved** register
(r24) → `bl __savegprlr_24`. With only one, it uses a volatile → `__savegprlr_25`.
**The EH funclets encode the parent's saved-register range**, so this single-register
difference held ~30 funclets at 99.9% even with an exact frame size. Adding the other
four statics flipped **+32 functions in one build**.
> **Phase-1 lesson, corrected:** exact frame size is necessary but NOT sufficient. The
> funclet trigger is (frame immediate) **AND** (`__savegprlr_N` range). Check instruction
> [1] of the compare-asm, not just the `subi/stwu` immediate.

The five statics, in source order:

| where | string | guard bit | storage |
|---|---|---|---|
| top of fn (after the early return) | `saveload_dialog_event` | 1 | `0x82DFDA24` |
| case 0x19 | `song_info_cache_name` | 2 | `0x82DFDA20` |
| case 0x2b | `global_options_cache_name` | 4 | `0x82DFDA1C` |
| case 0x2c | `global_options_cache_name` | 8 | `0x82DFDA18` |
| case 0x3b | `global_options_cache_name` | 0x10 | `0x82DFDA14` |

(Three *separate* statics with the same string — one per case block.)

## 1. Frame accounting

Retail prologue (`build/45410914/asm/band3/meta_band/SaveLoadManager.s:9830`):
`mflr r12 ; bl __savegprlr_24 ; subi r31,r1,0x170 ; stwu r1,-0x170(r1)`

**Retail frame = 0x170 (368 B). Ours = 0x180. Δ = +0x10 (16 B) — REGRESSED from exact.**
It was exact at `d7a570f3`; the four added local statics reintroduced +0x10 (4 slots).
Suspect: the `Localize(sym, NULL)` call sites spill a Symbol temp per static that retail
loads directly (`lwz r10, 0x0(r27)` straight into the arg register). See §4 Q3.

Retail locals, from the Ghidra decompile (`~/tmp/laneC_setstate_ghidra.txt`):

| slot | size | what | case |
|---|---|---|---|
| `auStack_c0` | 192 | `FixedSizeSaveableStream` | 0x33 / 0x3e |
| `auStack_f0` | 48 | `BufStream` | 0x21 |
| `local_100` / `local_f8` | 12 | `vector<BandProfile*>` temp | 0x38 |
| `local_110` / `local_108` | 12 | `vector<BandProfile*>` temp | 0x56 |
| `local_120` | 8 | `_M_erase` scratch | 0x57 |
| `auStack_118` | 8 | `Symbol` temp | 0x53 |
| `auStack_104` | 4 | `Symbol` temp | 0x13 |
| — | ~80 | outgoing-arg + linkage (0x120..0x170) | — |

Measure with (the MCP `run_diff_inspect mode=stack-layout` wrapper **times out** here):
```bash
venv/bin/python scripts/analysis/diff_inspect.py --stack-layout \
  --symbol '?SetState@SaveLoadManager@@IAAXW4State@1@@Z' --project-dir ~/tmp/wt-laneC-setstate
```

## 2. Retail global / helper identifications (all confirmed this session)

| retail symbol | is | evidence |
|---|---|---|
| `0x82E066AC` | **`TheMemcardMgr`** | `_OnCheckForSaveContainer_MemcardMgr__QAAXPAVProfile___Z`, `_OnDeleteSaves_…`, `_OnLoadGame_…` all take it as `this` |
| `fn_8254C0B0` | **`SaveLoadManager::GetProfile()`** = `TheProfileMgr.GetProfileForPad(mUser->GetPadNum())` | decompiled: vbase-adjust mUser(0x28), virtual slot 0, then `fn_82545E90(TheProfileMgr, pad)` |
| `fn_825504D8` | `GetNewSigninProfile()` | case 3 |
| `fn_82550598` | `GetAutosavableProfile()` | case 0x54 |
| `fn_82550148` | `UpdateStatus()` | — |
| `fn_82550460` | `SaveLoadManager::Finish()` | case 0x6a |
| `fn_82550658` | `StartSaveAction(bool)` | cases 0x46/0x47 |
| `fn_8258AEF0` | **`BandProfile::GetLocalBandUser()`** = `TheBandUserMgr->GetUserFromPad(GetPadNum())` | decompiled; matches the rb3-Wii oracle body verbatim |
| `fn_827AB9A8` | `MemcardMgr::SelectDevice(Profile*, **bool**, Hmx::Object*, int)` | r4=profile r5=bool r6=sink r7=pad — the bool is **second**, not last |
| `fn_827ABB60` | `MemcardMgr::IsStorageDeviceValid(Profile*)` (best fit) | case 0x54 branch predicate |
| `fn_82545838` | `ProfileMgr::GetGlobalOptionsSize()` | cases 0x2b/0x32/0x33/0x3b |
| `0x82DFD818` | `TheProfileMgr` | `_GlobalOptionsNeedsSave_ProfileMgr__QAA_NXZ` |
| `0x82E071F8` | `TheCacheMgr` (pointer) | `_GetCacheID_CacheMgr__QAAPAVCacheID__VSymbol___Z` |
| `0x82C72BA8` | `TheSongMgr` | `_GetCachedSongInfoSize_SongMgr__QBAHXZ` |
| `0x82DFED4C` | `TheUIEventMgr` (pointer) | `fn_825933E8(TheUIEventMgr, sym, 0)` = `TriggerEvent` |

> **Unblocks `SaveLoadManager::Start`.** The "unnameable external global @0x82E066AC"
> recorded as a blocker in `saveloadmanager-port-log-2026-07-20.md` **is
> `TheMemcardMgr`** (its MsgSource subobject is at +4). Start is now a tractable +1 and
> should be picked up next session.

## 3. Structural facts about the retail switch

* **ONE shared dialog block.** 29 case labels
  (`6 7 c e f 10 11 17 18 1c 29 2a 2f 3a 48 49 4a 4c 4e 4f 50 5c 5f 60 62 63 65 66 67`)
  reach `TheUIEventMgr->TriggerEvent(saveload_dialog_event, NULL)` at `0x82551488`, and
  **case 0x42 falls through into it**. `0x4d` belongs with `0xa`/`0xd`, NOT the dialog set.
* **Shared `SetState` tail.** ~30 cases put a next-state in a register and reach a single
  `bl SetState` at `.L_82550A60`. MSVC tail-merges plain `SetState((State)X); break;` —
  no source restructuring needed.
* **`mState` is RE-READ** for the exit switch (`lwz r11,0x20(r30)` after the static init).
  Caching `State oldState = mState;` pins a callee-saved reg and cascades regalloc.
* **State space is 0..0x6b** (jump-table bound `cmplwi r11, 0x6a` @ `0x825509CC`, 2-byte
  `lhzx` table at `jumptable_82093B10`). Renumbering landed in `a254b6a0`:
  our `0x6d/0x6e/0x6f` → retail `0x69/0x6a/0x6b`; our extra `0x69/0x6a/0x6b/0x6c` deleted;
  ManualDeleteStart moved to retail's **`0x4b`**.

## 4. Open questions / next steps (priority order)

1. **Case 0x14 (~9-instruction cluster, the largest remaining).** Ours calls
   `BandSongMgr::CreateSongCacheID` + `Symbol` ctor + `CacheMgr::AddCacheID`; retail is:
   ```
   unk4c /*String @0x48*/ = fn_827A8968(TheSongMgr);   // String::operator=(const char*)
   if (mCacheID) { TheCacheMgr->RemoveCacheID(mCacheID); delete mCacheID; mCacheID = 0; }
   if (!TheCacheMgr->vf8(m0x50, &mCacheID)) TheCacheMgr->GetLastResult();
   SetState(0x19);
   ```
2. **The +0x10 frame.** See §1. Likely the `Localize(static, NULL)` argument spills.
   Compare index-by-index around the case-0x2b/0x2c/0x3b blocks in `~/tmp/laneC_cmpasm.txt`.
3. **`LoadMemcardAction` sizeof.** Retail `operator new(0x14)`; ours emits `li r3, 0x1c`.
   Its base `MemcardAction` (or our added `unk24`/`mProfiles`) is 8 bytes too big.
   The ctor now takes `BandProfile*` (retail form) — that part is done.
4. **Post-manual-delete states are a GUESS.** `a254b6a0` remapped the two targets that
   pointed at our old `0x6b`/`0x6c` to `0x4c`/`0x4a` (both retail dialog states).
   Not independently confirmed — verify from `HandleEventResponse` / `OnMsg(MCResultMsg)`
   in Ghidra before relying on it.
5. **Residual regswaps.** `r25↔r26` × 28 is the dominant remaining class and is downstream
   of the above; do not permute (permuter is OFF per project directive).
6. **The other giants.** `GetDialogMsg` (`fn_8254CC98`, 97-case, 0%) and `GetDialogOpt1`
   (`fn_82553490`, 106-case) own the remaining ~140 funclets at 99.x. Apply the same two
   force-multipliers there FIRST (MakeString strip + local-static count) before body work.

## 5. Reproducible commands

```bash
WT=~/tmp/wt-laneC-setstate

# whole-binary A/B (ALWAYS rm the cache first; baseline pickle = 25,133 from main 7222d88f)
cd $WT && rm -f build/45410914/report.cache && ./tools/ninja-locked
python3 -c "
import json,pickle
r=json.load(open('build/45410914/report.json')); s=set()
for u in r['units']:
    for f in u.get('functions',[]):
        if f.get('match_percent_normalized')==100.0: s.add((u['name'],f['name'],f.get('virtual_address')))
b=pickle.load(open('/home/free/tmp/laneC_base_strict.pkl','rb'))
print(len(s), len(s)-len(b), 'lost', len(b-s))"

# Ghidra oracle (needs dangerouslyDisableSandbox for the network call)
venv/bin/python tools/ghidra/ghidra-decompile.py 0x82550880 --binary default_tu5.xex-c5a170

# instruction-level diff (full ~1100 lines, or --range A-B for a window)
venv/bin/python scripts/analysis/diff_inspect.py --compare-asm \
  --symbol '?SetState@SaveLoadManager@@IAAXW4State@1@@Z' --project-dir $WT

# retail this-relative member-offset histogram (ground truth for layout)
sed -n '9828,10964p' $WT/build/45410914/asm/band3/meta_band/SaveLoadManager.s \
  | grep -oE "(lwz|stw|lbz|stb|addi) +r[0-9]+, 0x[0-9a-f]+[,(]r30" \
  | grep -oE "0x[0-9a-f]+" | sort | uniq -c | sort -k2 -V
```

Note: `/d1reportSingleClassLayoutSaveLoadManager` produces **no output** with this
cl.exe (16.00.11886.00) — use the offset histogram above instead.

---

# Session 2 (2026-07-24/25) — the dialog giants + the HANDLE fleet sweep

**Worktree:** `~/tmp/wt-laneF-dialog`, branch `laneF-dialog` (base main `85af0a80`).
**Baseline: 25,231 strict.  Final: 25,885 (+654, 0 lost).**

## 6. Session ledger — session 2

| commit | change | fn % | frame | whole-binary | lost |
|---|---|---|---|---|---|
| `ebf4176f` | GetDialogMsg: drop case-0xE null branch + merge 0x3A into 0x2A | 0 → 0 | **0x4f0 → 0x4c0 (EXACT)** | 25,376 | 0 |
| `1837a106` | `SaveLoadManager.cpp` `/DRB3_HANDLE_LOCAL_STATIC` | Handle 66.7 → 91.8 | 0x110 → **0x120 (EXACT)** | 25,453 | 0 |
| `749cf23d` | GetDialogMsg: 32 function-local `static Symbol` + case 0x4A | **100.0** (1648/1648) | exact | 25,496 | 0 |
| `5212ce13` | GetDialogOpt1/2/3 local statics + retail case sets | **100/100/100** | exact | 25,499 | 0 |
| `0607b7c3` | HANDLE gate sweep, game round 1 (6 TUs) | — | — | 25,608 | 0 |
| `2bf1c8d4` | HANDLE gate sweep, game round 2 (6 TUs) | — | — | 25,689 | 0 |
| `984f49ac` | HANDLE gate sweep, rounds 3–4 (4 TUs) | — | — | 25,736 | 0 |
| `2288e2f6` | HANDLE gate, `BandCharacter.cpp` | — | — | 25,774 | 0 |
| `bf7d8639` | HANDLE gate, `VocalTrackDir.cpp` | — | — | 25,793 | 0 |
| `e2bfd154` | HANDLE gate, bandobj batch 1 (4 TUs) | — | — | 25,850 | 0 |
| `07cfe6f8` | HANDLE gate, bandobj batch 2 (4 TUs) | — | — | 25,877 | 0 |
| `dc7abace` | HANDLE gate, bandobj batch 3 (3 TUs) | — | — | 25,885 | 0 |

`SaveLoadManager` unit: 74/405 → **342/405** at strict 100.

## 7. Force-multiplier (2) confirmed and generalised — the funclet trigger

Session 1's corrected model held exactly. GetDialogMsg already had the right
`__savegprlr_27`; the **only** thing holding its 144 EH funclets at 99.9% was a
+0x30 frame. Fixing the frame alone flipped **145 functions in one build**, with
the body still at 0%.

**The frame-accounting rule that produced it: MSVC gives every `case` body its
own stack slots — it does NOT reuse them across arms of a switch.** So the frame
of a big switch is (roughly) the *sum* of every arm's temporaries, and the frame
delta is a direct census of extra/missing case bodies. Ours was exactly two
extra 2-node `DataArrayPtr` temp sets (24 B each):

* **case 0xE** — retail has *no* `if (!pProfile) return DataArrayPtr(mc_manual_load_corrupt, …)`
  and no `MILO_ASSERT`; it is one expression,
  `DataArrayPtr(s, DataArrayPtr(), GetProfile()->GetName())` (`fn_8254C0B0` then
  `fn_827A5018` then `??0DataNode@@QAA@PBD@Z`).
* **case 0x3A** — retail's jump table points 0x2A **and** 0x3A at the *same*
  block, so it is `case 0x2A: case 0x3A:`, not two bodies.

> **Reusable:** when a switch-heavy function has an exact `__savegprlr_N` but a
> frame that is a small multiple of one arm's temp footprint, look for
> extra/duplicated `case` bodies before touching anything else. It is worth far
> more than body work — 145 functions for a 6-line diff here.

## 8. Force-multiplier (3) — NEW: the retail dialog/handler `static Symbol` form

Retail does **not** reference the centralized `Symbols2/3/4.h` globals inside
these methods. It declares **function-local `static Symbol`s built from string
literals**, all sharing one guard word:

| function | retail addr | guard | storages | count | frame / savegpr |
|---|---|---|---|---|---|
| `GetDialogMsg` | `fn_8254CC98` | `0x82DFDA04` | `0x82DFDA00`↓`0x82DFD984` | **32** | 0x4c0 / `_27` |
| `GetDialogOpt1` | `fn_8254C108` | `0x82DFD950` | `0x82DFD94C`↓`0x82DFD91C` | **13** | 0xd0 / `_18` |
| `GetDialogOpt2` | `fn_8254C608` | `0x82DFD978` | `0x82DFD974`↓`0x82DFD954` | **9** | 0xb0 / `_21` |
| `GetDialogOpt3` | `fn_8254C9C8` | `0x82DFD980` | `0x82DFD97C` | **1** | 0x80 / `_28` |
| `Handle` | `fn_82552660` | `0x82DFDA74` | `0x82DFDA70`↓ | 16 | 0x120 / `_24` |

Two placement rules, both load-bearing:

* **`GetDialogMsg`** declares each static **inside its case arm** — the guarded
  init sits at the top of that arm's block, in jump-table block order.
* **`GetDialogOpt1/2/3`** declare **all** statics at the **top of the function**,
  *before* `Symbol sym(gNullStr)`; every guarded init runs before the switch and
  the live ones are pinned in callee-saved registers (that is why Opt1 saves
  from **r18**). Opt1 declares 3 statics the switch never reads
  (`upload_button_view_first` / `_return` / `_view_prev`) and Opt2 declares 2
  (`upload_button_return`, `upload_button_view_next`) — the ctor calls are still
  emitted, so they must be present in source.

For `Handle`/`OnMsg` the same form is already available as
`/DRB3_HANDLE_LOCAL_STATIC` (`src/system/obj/ObjMacros.h`). Turning it on for
`SaveLoadManager.cpp` alone was **+77**.

## 9. Retail-vs-Wii divergences confirmed this session

* `GetDialogMsg` state space is **6..0x67** (`subi r11,r11,6 ; cmplwi r11,0x61`,
  2-byte offset table `jumptable_82093A48` @ `0x82093A48`, base `fn_8254CC98+0xD0`).
  Decode it with the `band.exe` reader recipe in §11.
* **Case 0x4A exists** and we were missing it. Retail 0x49 is a bare
  `DataArrayPtr(s, DataArrayPtr())`; 0x4A is
  `DataArrayPtr(s, DataArrayPtr(), -TheMemcardMgr.GetSizeNeeded())` (the
  `neg r11,r3` at `0x8254DBEC`). **Both use the same string
  `"mc_save_not_enough_space"`** — retail has no `_fmt` variant and no
  `TheCacheMgr` size fixup. Adding that 30th case label is also what tips MSVC's
  size-based switch lowering from a binary-search chain to the jump table.
* `fn_823591E8` (called for 0x4A) is an ICF-folded `li r3,0 ; blr` stub — another
  instance of the "retail stubbed trivial game accessors" pattern.
* `Opt1` gains `case 0x4A -> "mc_button_delete_saves"` and **drops** the Wii
  `0x49/0x4E -> global_options_button_cancel` arm (that symbol belongs to Opt2).
* `fn_82552660` is the real `SaveLoadManager::Handle` body; `0x8254bbc0` and
  `0x82553478` are its two 12-byte adjustor thunks (both already 100%).

## 10. The HANDLE-gate sweep — measured, and where it stops

Per-TU protocol: set `/DRB3_HANDLE_LOCAL_STATIC`, full `./tools/ninja-locked`,
whole-binary A/B, **keep only if delta>0 AND LOST==0**, else revert.
Harness: `~/tmp/laneF_sweep.py` (rewritable in ~30 lines; see §11).

**33 TUs tried, 22 kept, +381 total.** Notable: SessionMgr +73, BandCharacter +38,
CampaignGoalsLeaderboardPanel +23, PitchArrow +23, TrackPanelDirBase +22,
StoreOfferProvider +20, GemTrainerPanel +20, VocalTrackDir +19, BandTrack +17.

**Rejected by the gate (net-positive but with regressions — worth a diagnose):**
`PresenceMgr.cpp` +19/LOST 1, `TrackPanelDir.cpp` +11/LOST 1,
`BandHighlight.cpp` +4/LOST 1, `BandWardrobe.cpp` **-10/LOST 11** (retail clearly
uses the global-Symbol form there).

**Where the vein stops.** The local-static handler form is an **RB3 game-layer**
property, not a shared-engine one. Zero-delta on every one of:
`rndobj/{Rnd,Trans,PropAnim,Part,Mesh,MultiMesh}`, `ui/{UI,UIList,UIPanel}`,
`obj/{Object,Dir,Task}`, `os/{PlatformMgr,ContentMgr,User}`,
`meta/{SongMgr,StorePanel,StorePreviewMgr,MoviePanel,CreditsPanel,StoreOffer}`,
`synth/Synth`, `synth_xbox/Synth`, `midi/MidiParser`, `track/TrackDir`,
`utl/{Song,Cheats}`, plus game TUs `NetworkEmulator`, `TourDescPanel`,
`TambourineManager`, `TexLoadPanel`, `SelectDifficultyPanel`,
`ContentLoadingPanel`, `GuitarFx`, `InterstitialMgr`.
`band3/`, `network/`, and `system/bandobj/` are now **drained**. ~200 ungated
engine TUs remain but the prior is now measured at ~0 for `system/` outside
`bandobj` — **do not blanket-sweep them**; `system/hamobj` (34 TUs) is DC3-only
and should be skipped outright.

## 11. Open questions / next steps (priority order)

1. **`SetState` frame is still +0x10 (0x180 vs 0x170) — blocks 5 EH funclets**
   (`fn_825518A0/18E8/1970/1998/19C0`, all `subi r31, r12, 0x170`). Apply §7's
   census rule: retail's slot ladder below `BufStream` is
   `0x50:8, 0x58:8, 0x60:12, 0x6c:4, 0x70:16`; ours is
   `0x50:4, 0x54:12, 0x60:16, 0x70:12, 0x7c:4, 0x80:16`. Matching from the top,
   `{16,4,12}` line up and then retail has **two 8-byte temps** where we have
   **{16,12,4}** — i.e. one extra object, 16 B. Look at the case-0x38/0x56/0x57
   `vector<BandProfile*>` / `_M_erase` arms. SetState is 89.37%.
2. **`Poll` = `fn_82553490`** (2628 B, frame **0x130**, `__savegprlr_29`, 0%,
   not yet in `target_symbol_map.json`). Owns the last 2 funclets
   (`fn_82553ED4/EFC`). Reveal it and apply the frame census.
3. **`Handle` 91.8% → 100.** Remaining diff is one *extra* handler we emit
   (guard bit 0x4, `"delete_saves"`) and an autoload arm that writes a bitfield
   at `this-0x14` where retail does `li r11,1 ; stb r11, 0x75(r30)`.
4. **12 funclets `fn_8254B990..fn_8254BB98` have parent frame 0xf0** — and **no
   function in the pinned SaveLoadManager span has a 0xf0 frame**. Their parent
   is outside this unit's split; either a scatter from another TU or an
   unpinned span. Do not hunt them from inside SaveLoadManager.cpp.
5. `GetDialogFocusOption` is still unlocated (not among Handle's `bl` targets —
   likely inlined into Handle).
6. Diagnose the 4 rejected HANDLE-gate TUs in §10; three are net-positive.

## 12. Reproducible commands (session 2)

```bash
WT=~/tmp/wt-laneF-dialog

# read retail .rdata strings / jump tables straight out of the extracted PE
# (orig/45410914/band.exe — no Ghidra needed).  Parse the section table once,
# then map VA -> file offset.  Used to recover all 55 static-Symbol strings and
# to decode jumptable_82093A48 (2-byte offsets, base = fn_8254CC98+0xD0).

# annotate a retail function's asm with the strings it references
python3 ~/tmp/laneF_dump.py fn_8254C108        # see docs; ~15 lines

# per-TU HANDLE-gate A/B sweep (keep iff delta>0 and LOST==0)
python3 ~/tmp/laneF_sweep.py <cur.pkl> band3/meta_band/Foo.cpp system/bandobj/Bar.cpp

# whole-binary A/B — ALWAYS rm report.cache first
cd $WT && rm -f build/45410914/report.cache && ./tools/ninja-locked
```
