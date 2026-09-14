# Vtable slot divergences, thunk shapes, DrawRect residual, XMPStateChangedMsg — lane W16-G (2026-09-14)

Branch `w16-g` (worktree `~/tmp/wt-w16-g`), merge-base with `main` = `62943770`.
Ruler: shipped `name_check` (`report.json` `provenance.diff_config`). All Δs are
whole-binary `report.json` reads on full `./tools/ninja-locked` builds;
`matched_functions` counts rows at `mpn == 100`, `matched_code` sums rows at
`fuzzy == 100`. Set-diffs are keyed on `(unit, symbol)`.

## 0. Result line

**Baseline (merge-base, settled):** 43,126 fns / 3,926,136 B (`total_code` 10,245,956).
**HEAD `c113f523`:** 43,153 fns / 3,928,008 B. **Net +27 fns / +1,872 B.**

| item | verdict | Δ fns | Δ B |
|---|---|---:|---:|
| 1 TourSavable / PracticePanel / DxTexRenderer | **NO DIVERGENCE** — retail RTTI vtables and the compiler agree slot-for-slot; briefed numbers were W15-D instrument artefacts | 0 | 0 |
| 1 Server (20 vs 19) | retail has ONE fewer slot than ours in the middle (slot [8] is a bare `blr` in retail, our header lacked it); fixed as `GetPlayerIDs` (name unattested); retail's two TAIL slots beyond our count remain unprovable | 0 | 0 |
| 1 PlatformMgr::Callback (14 vs 3) | header carried DC3's `ContentMgr::Callback` shape; retail `0x82088a4c` is a 3-slot `ThreadCallback` (`??_G`, `ThreadStart`, `ThreadDone`) — replaced | (in item 4 build) | |
| 2a SynthEmitter::Handle / `??1?$ObjPtr` thunks | mis-carved 8+4 → 12 B; two FALSE-100 map rows dropped | +1 | +20 |
| 2b UILabelDir::PreLoad `$4` | re-homed `0x828123A4–0x82812500` HamCharacter.cpp → UILabelDir.cpp; PreLoad matches | +0 | +148 |
| 3 `?DrawRect@DxRnd@@` | CLOSED NEGATIVE, 0/5 spellings; anchor recorded | 0 | 0 |
| 4 `0x8251CED0` = `??0XMPStateChangedMsg@@QAA@H@Z` + PlatformMgr re-home | 13 map inserts / 7 renames / 2 drops / 1 null | +26 | +1,704 |

Commits: `123454b7` (2a), `80797088` (2b), `bd2f668e` (Server), `c113f523` (PlatformMgr + item 4).

## 1. Vtable slot divergences

### 1.0 Why four of the six "divergences" were never real — the W15-D instrument

Two systematic errors in how the briefed numbers were produced, both reproduced
here rather than inferred:

1. **`class_layout_report.py` counts vbtable rows inside its "slots" figure.**
   For a class with a virtual base (`Object` is a virtual base of everything
   Hmx), the `<Class>@<Class>@` listing contains `[0] -4` and `[1] <vbase off>`
   rows *before* the virtual function slots. `TourSavable@TourSavable@ (7 slots)`
   is 5 virtuals + 2 vbtable rows; `PracticePanel@UIPanel@ (17 slots)` is 15 + 2;
   `Server@Server@ (22 slots)` is 20 + 2. Compare **n − 2** to retail's `n`.
2. **The retail RTTI dump was keyed by class name only.** A class with several
   vtables (DxTexRenderer has six) yields several `.?AVDxTexRenderer@@` COL hits
   at different `off`; W15-D matched the 14-slot primary against secondary tables
   ("14 vs 9", "14 vs 4"). Key on `(class, off)`.

Retail evidence below is the RTTI Complete-Object-Locator walk: each vtable is
listed as `addr off N n M` = vtable address, `this`-displacement, slot count;
slots are resolved through `scripts/target_symbol_map.json` where named. Three
ICF survivors recur: `0x826c3888` (every bare `blr`), `0x823591e8`
(`li r3,0; blr`), `0x828299b8` (a third fold, unnamed in the map).

### 1.1 TourSavable — briefed 20 vs 21 — NO EDIT

| | retail | ours (compiler) |
|---|---|---|
| primary | `0x82040504 off 0 n 5` | `TourSavable@TourSavable@` 7 rows = **5 virtuals** + 2 vbtable |
| Object | `0x820404ac off 24 n 21` | `TourSavable@Object@` 21 slots, Object subobject at `0x18` = 24 |
| sizeof | — | 64 |

Retail primary slots: `[0] IsDirtySave 0x8252d3f0`, `[1] IsUploadNeeded 0x823696e8`,
`[2] 0x828299b8` (SecBetweenUploads — folded survivor), `[3] SetDirty 0x82369768`,
`[4] UploadComplete 0x823697c8`. Our header declares exactly those five in that
order. The "20" is not reproducible from either the compiler or the binary.

### 1.2 PracticePanel — briefed 20 vs 21 — NO EDIT

| | retail | ours |
|---|---|---|
| primary | `0x820e816c off 0 n 15` | `PracticePanel@UIPanel@` 17 rows = **15 virtuals** + 2 vbtable |
| Object | `0x820e8114 off 108 n 21` | `PracticePanel@Object@` 21, Object at `0x6c` = 108 |
| sizeof | — | 148 |

Retail primary: `[0] Load 0x826b1ee0`, `[1] UIPanel::Draw 0x82812868`, `[2] Enter 0x826b2e00`,
`[3] Entering 0x828127f8`, `[4] Exit 0x826b2960`, `[5] Exiting 0x82812830`,
`[6] Unloading 0x82812a78`, `[7] Poll 0x826b2fe0`, `[8] SetPaused 0x82573468`,
`[9] blr`, `[10] blr`, `[11] Unload 0x826b1fa0`, `[12] IsLoaded 0x826b1f18`,
`[13] PollForLoading 0x82812680`, `[14] FinishLoad 0x826b1f68`. Matches the
UIPanel virtual order in `ui/UIPanel.h` (Draw/Enter/Entering/Exit/Exiting/Unloading/
Poll/SetPaused/FocusComponent/… /Unload/IsLoaded/PollForLoading/FinishLoad) with
PracticePanel overriding Load/Enter/Exit/Poll/Unload/IsLoaded/FinishLoad.

### 1.3 DxTexRenderer — briefed 14 vs 9 / 14 vs 4 — NO EDIT (six vtables)

| retail vtable | off | n | ours (compiler, sizeof 184) |
|---|---:|---:|---|
| `0x8210228c` | 0 | 14 | RndTexRenderer primary @0x00, 14 |
| `0x821022c8` | 36 | 9 | RndAnimatable @0x24, 9 |
| `0x821022f0` | 52 | 4 | RndPollable @0x34, 4 |
| `0x82102224` | 124 | 2 | DxObject @0x7c, 2 real slots |
| `0x82102234` | 132 | 21 | Object @0x84, 21 |
| `0x8210221c` | 176 | 1 | RndHighlightable @0xb0, 1 real slot |

Retail primary (14): UpdateSphere `0x82297128`, GetDistanceToPlane `0x82704640`,
`li0-blr` ×2, `blr`, DrawShowing `0x82446130`, ListDrawChildren `0x82446058`,
`li0-blr`, CollidePlane `0x82406988`, CollideList `0x824078c0`, DrawPreClear
`0x822ae748`, UpdatePreClearState `0x82443788`, `blr`, `blr`.
RndAnimatable (9): …, SetFrame `0x824441c0`, StartFrame `0x82444100`, EndFrame
`0x82444160`, AnimTarget `0x822737d8`, `blr`, ListAnimChildren `0x82445fe8`.
RndPollable (4): `blr`, Enter `0x824215e0`, Exit `0x824216a0`, ListPollChildren
`0x824460a8`. DxObject (2): `blr`, PostDeviceReset `0x8273ab50`.
RndHighlightable (1): Highlight `$4` thunk `0x8248abc0`.
Every table, count and `this`-offset agrees. "14 vs 9" and "14 vs 4" are the
primary table compared against the RndAnimatable and RndPollable secondaries.

### 1.4 Server — briefed 20 vs 19 — EDITED (`bd2f668e`), tail unsettled

| | retail | ours before | ours after |
|---|---|---|---|
| primary | `0x820577bc off 0 n 19` | `Server@Server@` 22 rows = 20 virtuals | 21 virtuals |
| Object | `0x82057764 off 112 n 21` | Object at `0x70` = 112 | unchanged |

Retail primary: `[0] Init 0x823f5dd8`, `[1] blr`, `[2..4] 0x828299b8` ×3,
`[5] IsConnected 0x823ec500`, `[6] IsLoggingIn 0x823ec518`, `[7] li0-blr`,
**`[8] blr`**, `[9..18] li0-blr` ×10.

The divergence is NOT "we have one too many at the end": slots 0–7 line up with
our `Init / Poll(blr) / Connect… / IsConnected / IsLoggingIn / …`, then retail
has a **void-returning `blr` at [8]** that our header did not have, pushing every
later slot by one. Proof independent of the header: `RockCentral::OnMsg(ServerStatusChangedMsg)`
(`fn_824FA350`) vcalls `0x38` and `0x34` on the server object; those are slots
14 and 13 = `GetCompetitionClient` / `GetPersistentStoreClient` **only** with
the extra slot at [8] present. `XboxServer` overrides slot [8] at `0x823edd88`
(120 B, iterates users → pushes IDs into a `vector<unsigned>&`), so the slot was
added as `virtual void GetPlayerIDs(std::vector<unsigned>&) {}` — the **name
is unattested** (no rb3-Wii/DC3 equivalent; chosen from the override body).

Pre-registered Δ0 / Δ0 (no Server row at 100 either side), OnMsg +0.78pp;
measured **Δ0 / Δ0 exact**, OnMsg 94.015625 → 94.02344 (immediate-offset diffs
are weighted fractionally — magnitude prediction was wrong, sign right).

**Unsettled:** after the [8] insert we have 21 virtuals vs retail's 19, i.e. two
TAIL slots in our header (inherited from the rb3-Wii dev decomp) that retail
never emits. Every retail tail slot [9..18] is the `li r3,0; blr` fold, so retail
bytes cannot say which two of our tail names are absent — dropping the wrong pair
would silently shift `XboxServer` override slots. Left as is; a lane with the
`XboxServer` vtable (`0x8205....` walk of its 19+ slots against the override
addresses) can settle it.

**Side-finding (not fixed, out of scope):** `StoreInfoPanel::GetRecommendationIndexPath`
calls `server->GetMasterProfileID()`, retail `fn_82638D58` calls
`server->GetPlayerID(user->GetPadNum())`.

### 1.5 PlatformMgr::Callback — briefed 14 vs 3 — EDITED (`c113f523`)

| | retail | ours before | ours after |
|---|---|---|---|
| Callback vtable | `0x82088a4c off 0 n 3` | `PlatformMgr::Callback` = DC3 `ContentMgr::Callback` shape, 14 slots; `class_layout_report.py`: "could not find a compiled TU in which 'PlatformMgr::Callback' is complete" | `ThreadCallback` 3 slots |
| PlatformMgr Object | `0x820889f4 off 76 n 21` | Object at `0x4c` = 76 | unchanged |

Retail slots: `[0] ??_GPlatformMgr 0x8251d4d8` (scalar deleting dtor — proves the
vtable belongs to a base of `PlatformMgr` whose first virtual is the dtor),
`[1] ThreadStart 0x8251c8a8`, `[2] ThreadDone 0x8251c928`. The 14-slot class in
our header was a copy of DC3's content-enumeration callback and was never
instantiated by RB3 code; the retail vtable is the `ThreadCallback` base that
`PlatformMgr` derives from (`int ThreadStart()` / `void ThreadDone(int)`), which
is why `??_GPlatformMgr` sits in it. Header change: `src/system/os/PlatformMgr.h`
(ThreadCallback base with those three virtuals; `XMPStateChangedMsg` moved here
from `band3/meta_band/MetaPanel.h`), `PlatformMgr_Xbox.cpp` bodies,
`PlatformMgr.cpp` statics (`mHomeMenuWii`, `unkce6b`), native shim
`native/src/platform/PlatformMgr_Native.cpp` (`ThreadStart` → -1, `ThreadDone` no-op).
Δ is bundled with item 4 (one build) — see §4.

## 2. r3/r4 thunk-SHAPE mismatches at 50%

Both were **carving** defects, not source defects: dtk had cut a 12-byte
`mr r3/r4` adjustor thunk as 8 + 4, so the map named the 8-byte fragment with a
full symbol and the 4-byte tail was an orphan. Each read 50% because half the
instructions paired.

### 2a `0x8271EEE8` / `0x82812500` (`123454b7`)

| addr | before | after |
|---|---|---|
| `0x8271eee8` | 8 B `fn_` + `0x8271eef0` 4 B named `??1?$ObjPtr@VRndTransformable@@@@UAA@XZ` (FALSE 100) | one 12 B thunk; map row for `0x8271eef0` **dropped** |
| `0x82812500` | 8 B `fn_` + `0x82812508` 4 B named `??1FilePath@@UAA@XZ` (FALSE 100) | one 12 B thunk; row **dropped**; `0x82812468` = `?PreLoad@UILabelDir@@UAAXAAVBinStream@@@Z` **added** |

`symbols.txt`: `fn_8271EEE8` / `fn_82812500` resized to `0xC`. `splits.txt`
(`.text` only): SynthEmitter (`synth/Emitter.cpp`) end `0x8271EEF0 → 0x8271EEF4`,
next unit start `→ 0x8271EEF8`; UILabelDir end `0x82812508 → 0x8281250C`;
UIPanel start `→ 0x82812510`. Pre-registered +1 / +20 B / `total_functions` −2;
measured **exactly** that. The two dropped rows were 4-byte `blr`-tail fragments
that had been scoring 100 against a same-named 4-byte fragment in our obj —
removing them is a denominator correction, not a loss.

### 2b `UILabelDir::PreLoad` `$4` (`80797088`)

`0x828123A4–0x82812500` was pinned to `HamCharacter.cpp` (wrong TU — the
retail cluster is UILabelDir's PreLoad + its `$4` adjustor + `??1FilePath`
thunk). Re-homed to `system/ui/UILabelDir.cpp`; `UILabelDir.cpp` got the
`push_macro/undef rev` bracket around `PreLoad` **only** — retail pushes it
*before* `RndDir::PreLoad`, so the bracket must not cover the ctor.
Pre-registered +148 B (PreLoad 148 B crosses); measured **+148 B exact**,
fns **+0** because `fn_82810170` (40 B funclet, `masked_equal`) moved
99.5 → 99.4 as a pairing artefact of the same re-home. A control build with the
re-home only (no source bracket) measured Δ0 / Δ0, isolating the source effect.

**Pin wish for W16-F (NOT applied — W16-F owns `0x82812018–0x82812068`):** the
six 12-byte `$4` adjustor thunks `fn_82812018 … fn_82812068` (sizes `0xC`,
`0x10`, `0xC` ×4 in `symbols.txt`) sit in the **unpinned gap** between
UILabelDir.cpp's block end `0x82812014` and its next block start `0x82812080`.
They are `UILabelDir` `$4` thunks (same class as PreLoad's). Recommended pin:
extend `system/ui/UILabelDir.cpp` `.text` to `start:0x828101a0 end:0x828121e8`
(absorbing the gap). Expected Δ: `auto_*` → named reattribution only, +0 / +0 on
the matching keys unless our obj emits the same `$4` set, in which case up to
+6 fns / +76 B. The `0x82812028` 16-byte one is the odd size — verify it is a
`$4` with a stack adjust rather than two mis-carved 8-byte thunks before pinning.

## 3. `?DrawRect@DxRnd@@` six-parameter register residual — CLOSED NEGATIVE

Exact symbol: `?DrawRect@DxRnd@@UAAXABVRect@Hmx@@PAVRndMat@@W4ShaderType@@ABVColor@3@PBV63@4@Z`
(1,512 B, `fuzzy 99.64286`, `mpn 100` — so it already counts as a function,
only bytes withheld). Retail body in `.fn fn_82733538`.

**Anchor instruction:** retail `lwz r24, 0x0(r30)` (the `this->vptr` load for the
`ShaderMgr` vcall is coloured into r24 **first**, before the six argument moves);
ours colours the same temp **last**, and the resulting r24/r25 shift propagates
through ~30 instructions of the arg-marshal block. Everything else is identical.

Spellings tried (each a full build, each pre-registered "crosses if the vptr temp
is coloured before the arg temps"):

| # | spelling | result |
|---|---|---|
| 1 | hoist `ShaderMgr&` reference into a local before the call | regression: prologue grew to `__savegprlr_24` |
| 2 | direct `TheShaderMgr.Method(...)` call (no ref) | `__savegprlr_25`, global reloaded per use |
| 3 | pointer local `ShaderMgr* sm = &TheShaderMgr;` at use site | byte-identical to baseline |
| 4 | same pointer declared at function top | byte-identical to baseline |
| 5 | `(&TheShaderMgr)->Method(...)` | identical to #2 |

0/5 crossed. **Corroboration:** DC3's `DxRnd::DrawRect` carries the identical
residual (`99.62687`, `mpn 100`) with the vptr in r22 — the same compiler
(cl 10224 vs 11886, same `/O1`) makes the same colouring choice from the
same-shaped source, so this is a coloring-order property not reachable from
source (`docs/decomp/patterns/fixable-liveness.md` Triage Split: register-only,
liveness identical ⇒ permuter-class, and the permuter is OFF by directive).
Do not re-fund without a new lever.

## 4. `0x8251CED0` is `??0XMPStateChangedMsg@@QAA@H@Z` — W16-E escalation (`c113f523`)

### 4.1 Evidence

The PlatformMgr message-ctor cluster `0x8251bd98–0x8251d4d8` was mapped by
W15/W16-E from name order, and several rows carried the **wrong message name at
99.85–99.88** (every instruction pairs; only the `?Type@X@@` callee / RTTI string
relocation differs — exactly the `name_check`-visible, `mpn`-blind class). Each
ctor was re-identified from its **own** callee `?Type@<Msg>@@SA?AVSymbol@@XZ` or
its `.?AV<Msg>@@` RTTI string (`~/tmp/w16g/ctor_rtti_index.txt`, rec 149 onward):

| addr | size | was | now | evidence |
|---|---:|---|---|---|
| `0x8251bd98` | | (none) | `??0Friend@@QAA@XZ` | body = `XONLINE_FRIEND` zero-init, called from EnumerateFriends |
| `0x8251c320` | | (none) | `??0PlatformMgr@@QAA@XZ` | vtable store `0x82088a4c` |
| `0x8251c5a0` | | (none) | `??1PlatformMgr@@UAA@XZ` | |
| `0x8251c620` | | (none) | `?UpdateSigninState@PlatformMgr@@QAAXXZ` | |
| `0x8251c8a8` | | (none) | `?ThreadStart@PlatformMgr@@UAAHXZ` | vtable slot [1] |
| `0x8251c928` | | (none) | `?ThreadDone@PlatformMgr@@UAAXH@Z` | vtable slot [2] |
| `0x8251cbe0` | 164 | | (unchanged) `SigninChangedMsg` | RTTI |
| `0x8251cd08` | 164 | `??0UITransitionCompleteMsg@@` | `??0ProfileSwappedMsg@@QAA@PAVLocalUser@@0@Z` | RTTI `ProfileSwappedMsg` |
| `0x8251ce28` | | (none) | `??0StorageChangedMsg@@QAA@XZ` | Type callee |
| `0x8251ce78` | | (none) | `??0ContentInstalledMsg@@QAA@XZ` | Type callee |
| **`0x8251ced0`** | 132 | `??0ServerStatusChangedMsg@@QAA@W4ServerStatusResult@@@Z` | **`??0XMPStateChangedMsg@@QAA@H@Z`** | Type callee `?Type@XMPStateChangedMsg@@`; one `int` arg |
| `0x8251cfa8` | | (none) | `??0PartyMembersChangedMsg@@QAA@XZ` | Type callee |
| `0x8251d000` | 136 | `??0LeftHandListEngagementMsg@@` | `??0UIChangedMsg@@QAA@_N@Z` | Type callee |
| `0x8251d0e0` | 132 | `??0MCResultMsg@@` | `??0FriendsListChangedMsg@@QAA@H@Z` | RTTI |
| `0x8251d1c0` | | `??0PlatformMgrOpCompleteMsg@@` | `??0ConnectionStatusChangedMsg@@QAA@_N@Z` | RTTI |
| `0x8251d2a0` | | `??0UIChangedMsg@@` | `??0PlatformMgrOpCompleteMsg@@QAA@_N@Z` | RTTI |
| `0x8251d378` | 224 | (none) | **not written** (`PlatformMgr::Init`) | no name, no RTTI; two callees unidentified |
| `0x8251d4d8` | | `??_GGameMicManager@@` | `??_GPlatformMgr@@UAAPAXI@Z` | vtable `0x82088a4c` slot [0] — the old name was a FALSE 100 |
| `0x8251d538` | | (none) | `?EnumerateFriends@PlatformMgr@@…` | `XFriendsCreateEnumerator` import |
| `0x8251dab8` | | (none) | `?Poll@PlatformMgr@@QAAXXZ` | `XNotifyGetNext` switch on `XN_*` ids |
| `0x823ecd70` | | (none) | `??0ServerStatusChangedMsg@@QAA@_N@Z` | Type callee — the REAL ServerStatusChangedMsg ctor, in `DingoSvr`'s cluster, takes `bool` not `W4ServerStatusResult` |
| `0x82b900a0` | | `??0ConnectionStatusChangedMsg@@QAA@_N@Z` | **null** | body calls `?Type@ProfilePictureFetchedMsg@@` — neither name is right; left anonymous rather than guess |

So `??0ServerStatusChangedMsg@@` is settled on retail bytes: it lives at
`0x823ecd70`, takes `bool`, and the W16-E row at `0x8251ced0` was a different
message entirely.

### 4.2 The three-part change, priced as ONE

(a) map rewrite above (13 inserts / 7 renames / 2 drops / 1 null; full diff vs
merge-base in `scripts/target_symbol_map.json`), (b) `PlatformMgr.h`
`ThreadCallback` base + `XMPStateChangedMsg` moved from `MetaPanel.h`, (c)
`PlatformMgr_Xbox.cpp` bodies for ctor/dtor/Poll/EnumerateFriends/ThreadStart/
ThreadDone/UpdateSigninState + the xdk header additions they need
(`xdk/xapilibi/xbox.h`: `XN_SYS_UI 0x9`, `XN_SYS_SIGNINCHANGED 0xA`,
`XN_SYS_STORAGEDEVICESCHANGED 0xB`, `XN_LIVE_CONNECTIONCHANGED 0x02000001`,
`XN_LIVE_INVITE_ACCEPTED 0x02000002`, `XN_LIVE_CONTENT_INSTALLED 0x02000007`,
`XN_FRIENDS_FRIEND_ADDED/REMOVED 0x04000002/3`, `XN_XMP_STATECHANGED 0x0A000001`,
`XN_PARTY_MEMBERS_CHANGED 0x0E040002`, `XONLINE_S_LOGON_CONNECTION_ESTABLISHED
0x001510F0`; `xdk/xonline/xonline.h`: `XONLINE_GAMERTAG_SIZE`, friend-state
flags, `XONLINE_FRIEND` (0xc4, pack 4), `XFriendsCreateEnumerator`).

**Pre-registered:** [+8, +16] fns / [+1,000, +3,500] B.
**Measured:** 43,127 → 43,153 = **+26 fns**, 3,926,304 → 3,928,008 = **+1,704 B**.
Bytes inside the interval; functions **above** it by 10. Attribution: the map
rewrite put a base obj under 12 EH funclets (472 B) that were previously
unpaired `auto_*` rows — pairing them counts on `mpn` at once. This is the
"adding a pin over `auto_*` is NOT neutral for EH-funclet rows" observation:
pin-neutrality (`docs/decomp/pin-neutrality-scoped-2026-08-14.md`) is scoped to
reattribution of *function* rows; funclet rows pair by byte signature the moment
a base obj exists.

Rows still sub-100 in the cluster: `Poll` 99.32, `EnumerateFriends` 97.75,
`ThreadDone` 95.7, `ThreadStart` 86.5, `??0PlatformMgr` 69.5,
`UpdateSigninState` 79.2, `??0ProfileSwappedMsg` 0 (body not written).
Cheap next steps priced but not taken (budget): `ThreadDone` unsigned compare
(+1 / +56 B), `ThreadStart` `result` init order (+1 / +124 B),
`EnumerateFriends` `li r26,0` placement (+1 / +356 B).

Other findings recorded on the way: `vector<Friend>::push_back` in
EnumerateFriends instantiates `??2Friend` via the templated `_Construct` path
(retail shows the same); `InviteAcceptedMsg` ctor needs `(int)sessionID`
(C2664 otherwise — the retail arg is a 32-bit truncation of the 64-bit XUID).

## 5. NOT done

- `PlatformMgr::Init` (`0x8251d378`, 224 B) — not written; two `bl` targets have
  no map name and no string anchor.
- PlatformMgr cluster rows below 100 (§4.2 list) — priced, not taken.
- TourSavable / PracticePanel / DxTexRenderer — **no edit, by evidence**; if
  W15-D's numbers are re-derived, key on `(class, off)` and subtract vbtable rows.
- Server's two tail virtuals vs retail's 19 — unprovable from the folded
  `li0-blr` tail; needs the `XboxServer` vtable walk.
- `?DrawRect@DxRnd@@` residual — 0/5, closed; permuter-class.
- UILabelDir `$4` block `0x82812018–0x82812068` — NOT pinned (W16-F owns it);
  pin wish in §2b.
- `0x82b900a0` left null (calls `?Type@ProfilePictureFetchedMsg@@` — a
  `ProfilePictureFetchedMsg` ctor exists in neither oracle header).
- `StoreInfoPanel::GetRecommendationIndexPath` callee mismatch (§1.4) — not fixed.
- Branch is on merge-base `62943770`; `main` has advanced (to `4edcd1b8` at
  time of writing) — **rebase before `git merge --no-ff`**. Do not diff
  `main..w16-g` to audit this lane; diff `62943770..w16-g`.

## 6. Branch net Δ

| | fns | `matched_code` B | `total_code` |
|---|---:|---:|---:|
| merge-base `62943770` (settled, `~/tmp/rb3_build_w16g_baseline.log`) | 43,126 | 3,926,136 | 10,245,956 |
| HEAD `c113f523` (re-confirmed full build, rc=0) | 43,153 | 3,928,008 | 10,245,956 |
| **net** | **+27** | **+1,872** | 0 |

Per-commit: `123454b7` +1 / +20 · `80797088` +0 / +148 · `bd2f668e` 0 / 0 ·
`c113f523` +26 / +1,704. Sum +27 / +1,872 — composes exactly with the
end-to-end read. `total_functions` 69,217 (−2 from the two dropped fragment rows
in 2a). Files changed vs merge-base (13): `config/45410914/splits.txt`,
`config/45410914/symbols.txt`, `scripts/target_symbol_map.json`,
`src/system/os/PlatformMgr.{h,cpp}`, `src/system/os/PlatformMgr_Xbox.cpp`,
`src/system/ui/UILabelDir.cpp`, `src/network/net/{Server,NetSession}.h`,
`src/band3/meta_band/MetaPanel.h`, `src/xdk/xapilibi/xbox.h`,
`src/xdk/xonline/xonline.h`, `native/src/platform/PlatformMgr_Native.cpp`.
