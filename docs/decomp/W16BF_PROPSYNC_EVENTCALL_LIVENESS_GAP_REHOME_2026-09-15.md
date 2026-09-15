# W16-BF — the 8 B liveness gap in `PropSync<ObjList<EventAnim::EventCall>>`, its re-home, and where `fn_82271620` actually lives (2026-09-15)

Lane W16-BF (Fable). Worktree `~/tmp/wt-w16-bf`, branch `w16-bf`, base = main `5e852e7c1755`
(baseline measures from `153a9aa659fb`: **43,506 fns / 4,036,280 B / 39.3937 %**, `total_code`
10,246,004, `total_functions` 69,216, objdiff 4.2.9 `5a51cd51fe0a353f`, ruler `name_check`).

Commits on `w16-bf`, in order:

| sha | what |
|---|---|
| `5e9aa0d6` | `EventAnim.cpp`: `#define RB3_TU_OBJPTR_FORCEINLINE_CTOR` — closes the 8 B gap (item 1, leg 1) |
| `9e3d634e` | `EventAnim.cpp`: `#define RB3_TU_OBJPTR_DEFER_OWNER` — makes the now-inline ctor byte-equal to retail `fn_824C8DD0` (item 1, leg 2) |
| `54646f45` | `splits.txt` + `target_symbol_map.json`: re-home retail `0x824C95F0–0x824C97D8` from `EventTrigger.cpp:` to `EventAnim.cpp:`; name three byte-proved rows (item 2, ONE commit; includes the split's own `.pdata` re-derivation, see §2.3) |
| (this doc) | write-up + `docs/decomp/W16BF_map_proposals.json` for W16-BD (item 3) |

Lane-internal result: **43,506 / 4,036,280 B → 43,512 / 4,037,096 B (+6 fns / +816 B)**,
`total_code` / `total_functions` **unchanged**, every delta pre-registered and measured by rowset
set-diff (§2.4). Gates (§4) all green; native gate `PASS 18/18 skipped=0`.

---

## 1. Item 1 — why retail keeps `i+1` in r6 and we did not (byte-level diagnosis)

### 1.1 The charged site

Retail `fn_824C95F8` (388 B) and our `??$PropSync@VEventCall@EventAnim@@@@YA_NAAV?$ObjList@VEventCall@EventAnim@@@@AAVDataNode@@PAVDataArray@@HW4PropOp@@@Z`
COMDAT in `EventAnim.obj` (396 B before the fix) share a 12-word prefix and a 6-word suffix. Inside
the `kPropInsert`/`kPropSet` path retail does

```
addi  r6,r28,1          ; i+1 computed ONCE, straight into the outgoing-arg register
cmpw  cr6,r6,r11
...
bl    fn_824C8DD0       ; EventCall::EventCall(Hmx::Object*)  -- 76 B, a LEAF
...                     ; r6 still holds i+1 here
bl    fn_824C9270       ; PropSync(EventCall&, DataNode&, DataArray*, int i, PropOp) -- r6 is the `i` arg
```

and ours did

```
addi  r28,r28,1         ; i+1 into a NON-volatile
cmpw  cr6,r28,r11
...
bl    ??0?$ObjPtr@...   ; two out-of-line ObjPtr member ctors
bl    ??0?$ObjPtr@...
mr    r6,r28            ; +4 B   (o+0x120)
...
mr    r6,r28            ; +4 B   (o+0x170)
```

### 1.2 Mechanism: intra-TU callee register-usage knowledge, not a permuter swap

MSVC X360 at `/O1` tracks, for callees defined **in the same TU**, which volatile registers the
callee actually writes. `T item(owner)` in the template body calls `EventCall::EventCall(Hmx::Object*)`.
In retail that ctor (`fn_824C8DD0`, 76 B) is a **leaf** — both `ObjPtr<>` member ctors are inlined
into it — and it never touches r6. The compiler therefore treats r6 as *preserved* across the `bl`
and can leave `i+1` sitting in the argument register from the compare all the way to the
`PropSync(EventCall&,…)` call. Once the ctor instead calls two out-of-line
`??0?$ObjPtr@VObjectDir@@VObject@Hmx@@@@QAA@PAVObject@Hmx@@PAVObjectDir@@@Z`-style ctors, that
knowledge is gone (the callees are cross-COMDAT and their register usage is opaque), r6 must be
assumed clobbered, `i+1` is parked in r28, and two `mr r6,r28` appear. That is exactly +8 B.

Why the **ProxyCall** instantiation (`EventTrigger.obj`) already read 388 B with the r6 shape and
needed nothing: `ProxyCall`'s ctor is already a leaf in our build, so the same template body gets
the same intra-TU knowledge there. So **T selects the shape through the leaf-ness of `T::T(Hmx::Object*)`
inside the instantiating TU** — not through `sizeof(T)` (BA's refuted struct-size hypothesis, which
this lane did not reopen), not through the element-overload body, and not through anything in
`PropSync_p.h` (untouched).

### 1.3 The fix (scoped to `EventAnim.cpp` by construction)

`src/system/obj/ObjPtr_p.h` / `Object.h` already carry two per-TU levers used by
`BandCharacter.cpp` and `GemTrack.cpp`:

- `RB3_TU_OBJPTR_FORCEINLINE_CTOR` — force-inlines the two-arg `ObjPtr` ctor in this TU. The only
  two-arg `ObjPtr` ctor call sites in `EventAnim.cpp` are inside `EventCall::EventCall`, so the define
  affects exactly that ctor. Leg 1 (`5e9aa0d6`): PropSync COMDAT 396 → **388 B, 97/97 words equal
  reloc-normalized** against retail `fn_824C95F8`.
- `RB3_TU_OBJPTR_DEFER_OWNER` — retail's ctor stores in the order `{lis vptr, stw mOwner, li 0, addi,
  stw mObject, stw vptr}`; the default inline ctor emits `stw mOwner` first. Leg 2 (`9e3d634e`): the
  ctor COMDAT reads **19/19 words** vs `fn_824C8DD0` (76 B).

The element overload `?PropSync@@YA_NAAVEventCall@EventAnim@@AAVDataNode@@PAVDataArray@@HW4PropOp@@@Z`
was already **68/68** vs `fn_824C9270` (272 B) and was only *named* by this lane (§2.2).

### 1.4 Item 1 pre-registration vs measurement

Pre-registered: the body cannot move the metric until item 2 (its target row sat under
`EventTrigger.cpp:`, unpaired), so evidence = COMDAT byte compare + **FELL OUT = 0**.

Measured (rowset set-diff vs the baseline copy `~/tmp/rows_w16bf_base.json`, run inside the tree
after the full build): **FELL OUT 0**. Every other `??$PropSync@` row and all four sibling
`PropSync<…@EventAnim>` rows unmoved. CROSSED IN **+3 fns / +40 B**: the ctor's EH funclets
`fn_824C8E1C` / `fn_824C8E44` / `fn_824C8E6C` re-paired (only `fn_824C8E44`, 40 B, crossed
`fuzzy == 100`; the other two crossed `mpn` only). Lane-internal after item 1: 43,509 / 4,036,320 B.

---

## 2. Item 2 — re-home + name

### 2.1 Retail geometry of the COMDAT (why the 12 B "tail" is not part of it)

Retail `0x824C95F0–0x824C97D8` (488 B) = 8 B EH prefix `{.text ptr, FuncInfo ptr}` + `fn_824C95F8`
(388) + `fn_824C977C` (40 B funclet, calls `??1DataNode@@QAA@XZ`) + `fn_824C97A4` (40 B funclet,
calls `??1EventCall@EventAnim@@QAA@XZ`) + **12 B = 4 B `.4byte 0` alignment pad + the 8 B EH prefix of
the function at `0x824C97D8`** (read off `EventTrigger.s` after `.endfn fn_824C97A4`). So the old
split cut a *different* function's EH prefix off its body, and merging the three blocks reunites it.

### 2.2 The edit (commit `54646f45`)

`config/45410914/splits.txt`:
- `EventTrigger.cpp:` (heading line 2772): deleted `\t.text start:0x824C95F0 end:0x824C97D8`.
  Verified with the awk block printer that the heading keeps its other **20** `.text` blocks — no
  vanishing-unit hazard.
- `EventAnim.cpp:` (heading line 10914): `0x824C9430–0x824C95F0` + `0x824C97D8–0x824C9878` →
  ONE line `\t.text start:0x824C9430 end:0x824C9878`.

`scripts/target_symbol_map.json` — three keys appended (file's hex keys are unsorted; appending
matches convention; round-tripped with `json.dumps(d, indent=1, ensure_ascii=False)+'\n'`, diff
checked = exactly 3 added lines):

```
"0x824c95f8": "??$PropSync@VEventCall@EventAnim@@@@YA_NAAV?$ObjList@VEventCall@EventAnim@@@@AAVDataNode@@PAVDataArray@@HW4PropOp@@@Z"
"0x824c8dd0": "??0EventCall@EventAnim@@QAA@PAVObject@Hmx@@@Z"
"0x824c9270": "?PropSync@@YA_NAAVEventCall@EventAnim@@AAVDataNode@@PAVDataArray@@HW4PropOp@@@Z"
```

All three spellings were read from our `EventAnim.obj` COFF symbol table after a full build (not
typed from memory), and each is byte-proved above (97/97, 19/19, 68/68).

`0x824c977c` / `0x824c97a4` deliberately **left absent** — see §5 (NOT-done) for the mechanism.

### 2.3 Build sequence (what actually happened, including the failure)

- `touch config/45410914/config.yml`; build 4 → **rc=1**: `[1/2] SPLIT` failed at
  `verify_split_current.py --complete` because the split re-derived the `.pdata` lines
  (EventTrigger `0x82213DF0–0x82213E08` deleted; EventAnim `0x82213DD8–0x82213DF0` +
  `0x82213E08–0x82213E18` → `0x82213DD8–0x82213E18`) into the tracked `splits.txt` and refuses an
  uncommitted rewrite. Fix: `git add config/45410914/splits.txt && git commit --amend --no-edit`
  (`7a5f1b57` → `54646f45`), keeping the brief's ONE-commit rule for splits+map.
- Build 5 → **rc=0**; `symbols.txt` and `splits.txt` clean afterwards, i.e. the fixed point was
  reached after one re-split.

### 2.4 Pre-registration vs measurement

Pre-registered (before build 5): **+776 B / +3 fns** — 388 (PropSync) + 272 (overload) + 76 (ctor)
+ 40 (`fn_824C977C`, expected to re-pair under EventAnim) = 776; `fn_824C97A4` expected to stay at
`mpn` 100 / `fuzzy` < 100 as BA recorded, and EventTrigger's copy of `fn_824C977C` to fall out
(unit move, Δ0 net).

Measured, `python3 tools/rowset_snapshot.py diff` inside the tree vs the item-1 snapshot:

| direction | rows | bytes |
|---|---|---|
| CROSSED IN | `??$PropSync@VEventCall…` 388 · `?PropSync@@…EventCall…` 272 · `??0EventCall@…` 76 · `EventAnim::fn_824C977C` 40 · `EventAnim::fn_824C97A4` 40 · (`fn_824C8E44` 40 was item 1's) | +816 (this commit: +776) |
| FELL OUT | `EventTrigger::fn_824C977C` 40 (unit move) | −40 |

Item 2 alone: 43,509 → **43,512 (+3) / 4,036,320 → 4,037,096 (+776 B) — exactly as pre-registered.**
The one deviation from the *prose* prediction: `fn_824C97A4` also crossed `fuzzy == 100` (its charged
callee `??1EventCall@EventAnim@@QAA@XZ` now resolves in the new unit), which the +776 figure had
already priced in via the EventTrigger→EventAnim swap netting to zero, so the byte total is unchanged.

Post-build `report.json`: `default/EventAnim` 57/63 fns, 5,656/6,768 B; `default/EventTrigger` 269/330,
22,456/41,180 B. `total_code` 10,246,004 / `total_functions` 69,216 — **unchanged** (a re-home moves
rows between units; it neither creates nor hides one).

---

## 3. Item 3 — `fn_82271620`'s parent COMDAT LOCATED: it is `App::App`, not a RhythmDetector member

BA §4 recorded the funclet as "reloc-masked-absent from BOTH RhythmDetector.obj and
DirectInstrument.obj, so its parent belongs to a third TU", and listed "parent COMDAT located and
shown to be a RhythmDetector member" as the evidence that would change its conclusion. The evidence
came in — the other way.

Method (Python over `orig/45410914/band.exe`, PE sections parsed by hand, big-endian fields; the
`0x19930522` FuncInfo magic the tree already uses in `tools/eh_state_screen.py`):

1. Scan `.rdata` for FuncInfo records; for each, walk its UnwindMap and look for action
   `0x82271620`. **Exactly one hit**: FuncInfo at `.rdata:0x82000DE0`, `maxState` 8, actions
   `[(-1,0x822715B0),(0,0x822715D8),(0,0x822715F8),(0,0x82271620),(3,0x82271648),(0,0x82271670),(0,0x82271698),(6,0x822716C0)]`
   — i.e. **all eight** funclets `0x822715B0–0x822716E8` belong to one parent.
2. Scan `.text` for the 8 B EH prefix `{.text ptr, 0x82000DE0}`. **Exactly one hit** at
   `0x82270E60` ⇒ the parent is **`fn_82270E68`** (`symbols.txt` size 0x748 = 1,864 B,
   `stwu r1,-0x250(r1)`; all eight funclets open with `subi r31,r12,0x250`, and only three functions
   tree-wide have a 0x250 frame — keyed on `.fn`, never the synthetic address column).
3. Identify the parent by its callees via the map: `?Poll@Splash@@` ×22, the `Splash`
   ctor/dtor/`AddScreen`/`BeginSplasher`/`EndSplasher`/`PrepareRemaining`/`Suspend`, `?GameInit@@YAXXZ`,
   `?BandUserMgrInit@@YAXXZ`, `?Init@GameMicManager@@SAXXZ`, `?SetArchivePermission@Archive@@` ×3,
   `?SystemConfig@@` ×2, `?SplitMs@Timer@@` ×2, `?EnableKeyCheats@@`, `?OptionBool@@`,
   `??0FilePath@@` ×3, `??1String@@` ×3, `??0Symbol@@` ×6, `CriticalSection` new,
   `FixedSizeAlloc` delete. rb3-Wii oracle `~/code/milohax/rb3/src/App.cpp:169`
   `App::App(int argc, char **argv)` has `SetArchivePermission` (320/485/495), `BandUserMgrInit`
   (336), a local **`ObjDirPtr<ObjectDir> oPtr;` (379)** and `GameInit` (416). `fn_82271620` is the
   cleanup for that `ObjDirPtr<ObjectDir>` local at frame +0x68 (it calls `fn_822709D8`, the
   `ObjDirPtr<ObjectDir>` dtor BA identified).

So the `ObjDirPtr<ObjectDir>` BA hunted through RhythmDetector is a **local in `App::App`**. The
retail COMDAT `0x82270E60–0x822716E8` is cut by `splits.txt` at `0x822715B0` between `App.cpp:`
(single-function block `0x82270E68–0x822715B0`) and `RhythmDetector.cpp:`, so RhythmDetector's unit
is carrying 312 B of App's funclets, which pair falsely by byte signature (7 at fuzzy 99.3 / mpn 99.8,
`fn_822715D8` 32 B at 100/100).

Neither heading is mine (W16-BD owns every heading except my two), so the re-home is **filed, not
applied**: `docs/decomp/W16BF_map_proposals.json` (BB-1's shape). Predicted effect of BD applying it:
**−1 fn / −32 B** (the one falsely-100 row), `total_*` unchanged — an accuracy landing of the same
class as `b341d7ab`. `App.cpp` has **no source** in `src/` (objects.json declares it, `project.py`
drops the edge silently — the no-source class), so `default/App` cannot pair any row at any pin; the
row `0x82270e68` is therefore NOT named (§5).

---

## 4. Gates (worktree, in the brief's order)

- Build 5 `./tools/ninja-locked` → `BUILD rc=0` (`~/tmp/rb3_build_w16bf_5.log`).
- `python3 scripts/verify_ruler_agreement.py --check` → rc=0, "OK: both objdiff-cli entry points
  resolve the same ruler".
- `python3 scripts/verify_objs_patched.py --verify-manifest` → rc=0, "[patch-state] OK: 1215 decomp,
  3114 target objects match … tree_sha256=1b17b12c49e89edd"; denylist OK (6 addresses, 495,636
  symbols scanned).
- `tools/native_build_gate.sh` (LAST action, after all `src/` edits), verbatim:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

---

## 5. NOT done, and why

1. **`0x824c977c` / `0x824c97a4` left absent from the map.** Naming a retail funclet `__unwind$NNN`
   (the eh_boundary spelling our obj emits) would REMOVE its row from the report: objdiff-core
   `src/obj/read.rs:150` sets `SymbolFlag::Hidden` on COFF symbols named `except_data_*` / `__unwind*`
   / `__catch*` / `__comdat_gap*`, and objdiff-cli `src/cmd/report.rs:1225` skips Hidden symbols —
   numerator AND denominator. That is why the 21 existing `__unwind$`-named map rows have no report
   rows at all. Both funclets pair by byte signature today and read 100/100 under EventAnim, and a
   placeholder target is forgiven under `name_check`; naming them buys nothing and would silently
   shrink `total_functions` by 2. ⚠ Flagged for a map lane: those **21 `__unwind$` rows are a
   denominator hazard tree-wide**, not a naming convention.
2. **`0x82270e68` not named `??0App@@QAA@HPAPAD@Z`.** The identification (§3) rests on the rb3-Wii
   signature + callee set, but there is no `App.obj` to read the MSVC spelling from, and with no base
   obj naming has zero pairing effect; its one caller is `main`, which CLAUDE.md records as
   structurally unmatchable on this image. Proposed as identification-only in the JSON; BD decides.
3. **App/RhythmDetector boundary move not applied** — not my headings. Filed for W16-BD with the
   exact two-line edit, the `.pdata` re-derivation note (§2.3's failure mode), and the predicted
   −1/−32.
4. **`fn_822716E8` (16 B) / `fn_822716F8` (68 B)** after the funclets under RhythmDetector: a
   different COMDAT (stores/uses vtable `lbl_82000E8C`, scalar-deleting-dtor shape calling
   `fn_8240DDB0`). Not identified; explicitly excluded from the proposal.
5. **`PropSync_p.h` untouched** — the gap was never in the template; changing it would have risked
   every other instantiation for no reason.
6. **No `none` control** run, per the brief (function count is not ruler-invariant on 4.2.9).
7. **BA's refuted ProxyCall struct-size hypothesis not reopened** — the T-selection mechanism in
   §1.2 explains the ProxyCall control without it.

What would change §3's conclusion: an `App.obj` (i.e. `src/**/App.cpp` ported) whose
`??0App@@QAA@HPAPAD@Z` COMDAT is NOT 1,864 B with eight funclets, or a second FuncInfo in `.rdata`
whose UnwindMap references `0x82271620` (the scan found exactly one).
