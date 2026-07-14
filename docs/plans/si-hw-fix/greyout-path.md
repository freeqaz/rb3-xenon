# Overshell instrument-select grey-out — ground truth (Layer A / IsActive)

Date: 2026-07-09. Scope: verify whether our Layer-A hook (force
`OvershellPartSelectProvider::IsActive` -> true) is hooked at the right point and
whether it's actually reachable on a real RB3E+RB3Deluxe console, given that
hardware showed **no visible change and no crash**.

## VERDICT (headline)

**IsActive IS necessarily executed** when the overshell renders/polls the
part-select rows, on stock retail TU5 and on RB3 Deluxe alike (RB3DX's Overshell
code is byte-identical to clean TU5 — see §5). It is a genuine C++ `virtual`
method (MSVC mangling `?IsActive@OvershellPartSelectProvider@@UBA_NH@Z`, "U" =
public virtual) invoked through the object's vtable by generic, provider-agnostic
engine code in `system/ui/UIList*.cpp` (`UIListState::CanScrollBack/CanScrollNext`,
`UIList::Refresh/DisableData/StartScroll/CompleteScroll`) — **every** poll of
**every** `UIList`/`UIListState` bound to this provider calls it. It is not
conditioned on scene/DTA content, not something a `.milo`/`.dta` change can skip.

So the absence of any observed behavior change on hardware is **not** explained
by "IsActive is dead code" — it is called. The two live hypotheses that remain
(not resolved by this static ground-truth pass, need runtime confirmation) are:

1. **The detour never actually installed / branch never executed on real
   hardware** — e.g. the `.data` cave at `0x82C8A000` didn't get patched into
   the *booted* image (wrong file staged, hardware boot loader/RB3E didn't
   apply this specific patch.toml, or NX enforcement on console silently no-ops
   writes to that region without crashing — consoles are typically stricter
   than Xenia's `writable_code_segments`, so a non-executing `.data` branch
   target could fault-and-be-caught/ignored by the RB3E loader rather than
   hard-crash, depending on how the patch is delivered).
2. **The whole-function-override design (§4) is working exactly as intended
   at Layer A, but Layers B/C are unmodified stock and re-reject the
   selection before the player ever sees a difference** — i.e., the grey-out
   *looks* stock because a still-active Layer B (`ResolvePartWaitStates`)
   or Layer C (`ProcessConfig`) is bouncing the user back to `ChoosePart`
   fast enough that the UI never lingers on the "not-grey" state, OR those
   layers' detours similarly didn't install. This doc doesn't have hardware
   telemetry to distinguish "IsActive detour didn't fire" from "it fired but a
   downstream layer silently reasserted the grey/rejected state" — recommend
   instrumenting `IsActiveHook` (a debug counter/log line, RB3E already has
   `RB3E_MSG`) and re-testing on hardware, since that's the cheapest way to
   settle which hypothesis is true.

Also newly surfaced (not previously flagged, potentially important): **the
task's IsActive addresses are BASE (TU0) addresses**
(`0x8264B5F8`) but the ACTUALLY-STAGED/tested artifact per
`rb3-xenon/docs/plans/same-instrument-tu5-retarget.md` uses **TU5 VA
`0x826684C0`** (retargeted 2026-07-07, same day). If hardware is running
retail TU5/RB3DX (near-certain — that's what ships), **a patch built against
the BASE VA `0x8264B5F8` would branch into the middle of an unrelated TU5
function** (TU0 and TU5 are NOT the same layout — text delta measured at
`+0x6200`/`.data +0xD400` in TU5's section-mapped XEX, and IsActive's own body
diverged ~56% between TU0 and TU5 builds). **This is the single most likely
explanation for "identical to stock, no crash": if the hardware xex is TU5 and
the patch was built/targeted at the TU0 address 0x8264B5F8, the write either
(a) hit a completely different function (garbling it, which might not
manifest as a crash if that code path is rarely exercised) or (b) if the xex
patcher/loader also does its own TU5 offset translation and got it right, it's
moot — but this must be checked before trusting any other hypothesis.**
**Action: confirm which XEX/patch artifact was actually flashed/loaded on the
test hardware — `default_tu5_patched.xex` (TU5 VA 0x826684C0, byte-verified
2026-07-07) vs an older TU0-targeted build (0x8264B5F8).**

---

## Q1 — What calls IsActive? Virtual (data-driven, robust) or direct C++ call? Greyed vs hidden?

**Virtual, called by generic engine UI-list code — not overshell-specific, not
scene/DTA-driven.** Evidence:

- Header: `src/band3/meta_band/OvershellPartSelectProvider.h:22`:
  `virtual bool IsActive(int) const;` — class inherits
  `public UIListProvider, public Hmx::Object` (line 8).
- `UIListProvider::IsActive` base declaration:
  `src/system/ui/UIListProvider.h:35` (`virtual bool IsActive(int) const { return true; }`,
  default) — this is a generic list-provider interface, implemented by many
  unrelated providers (`CheatProvider::IsActive`, `LocalePanel::IsActive`,
  `UIListDir::IsActive`, `UIGridProvider::IsActive`, etc.)
- Callers, all in the generic engine `system/ui/` layer, called through
  `mProvider`/`mDataProvider`/`state.Provider()` (a `UIListProvider*`), never
  through a concrete `OvershellPartSelectProvider*`:
  - `UIListState::CanScrollBack/CanScrollNext` (`UIListState.cpp:54,66`) —
    used to determine whether a scroll in either direction lands on any active
    row.
  - `UIList::Refresh` (`UIList.cpp:424`) — **every poll**, if the currently
    selected row is no longer active, auto-reselects
    (`SetSelected(nowrap, -1)`).
  - `UIList::DisableData` (`UIList.cpp:683`), `UIList::StartScroll/CompleteScroll`
    (`UIList.cpp:706,725`) — gate scroll-start/scroll-complete UI messages and
    `HandleSelectionUpdated()`.
  - `UIListState.cpp:262,445,481` — internal display/data index walking
    (`skipActive` handling) also queries it.
- On the Ghidra/binary side (base TU0, port 8002, project `default.xex-35adb6`):
  `list_xrefs 0x8264b5f8` shows **zero CODE (branch) xrefs** but **one DATA
  xref**: `from_address 820d4c60 -> to_address 8264b5f8, type DATA`. Reading
  32 bytes around `0x820d4c60` (`read_bytes 0x820d4c50 64`) shows a table of
  consecutive PowerPC code addresses (`8264bb18, 8229be98, 8264b5d8, 8264b5f8,
  82b59210, ...`) — a **vtable slot table**, consistent with IsActive being
  called only through indirect (`(**vtable)(this,...)`) dispatch, which is why
  static xref analysis finds no direct `bl` callers. `search_symbols_by_name`
  confirms the MSVC-mangled name `?IsActive@OvershellPartSelectProvider@@UBA_NH@Z`
  at `0x8264b5f8` — the `U` access-flag in MSVC member mangling = **public
  virtual**, confirming it's compiled as a true vtable method in the retail
  binary too (not devirtualized/inlined).

**Greyed vs hidden — GREYED, always renders.** `IsActive` gates *state*, not
*presence*. The row is always drawn (all `NumData()` entries get a
`UIListSlotElement`); when a row's `mComponentState == UIComponent::kDisabled`,
`UIListSlot::Draw` (`system/ui/UIListSlot.cpp:38-140`) applies
`d10 *= DisabledAlphaScale()` (dims alpha) and a distinct
`kUIListWidgetActive`/`kDisabled` color (`system/ui/UIListWidget.cpp:191-192`
declares the disabled-state color slot). So a taken instrument stays visible in
the list with a dimmed/disabled look — matches the reported "stays greyed out"
behavior on hardware, i.e. that observed behavior is **consistent with stock**
IsActive still returning false for that row (our detour not taking effect),
not with any other code path.

---

## Q2 — What does IsActive check? RepresentSamePart? Which screen?

Source: `src/band3/meta_band/OvershellPartSelectProvider.cpp:86-143`.

```cpp
bool OvershellPartSelectProvider::IsActive(int data) const {
    if (mPartSelections.empty()) return false;
    if (!mUser->IsParticipating()) return true;
    ...
    if (mUser->IsLocal() && entry.unk0 == overshell_drums_pro) {
        if (UserHasGHDrums(mUser->GetLocalUser())) return false;   // GH-drums exclusion
    }
    if (mUser->GetTrackType() != kTrackNone && ... != entry.unk4) return false; // track-type mismatch

    for (int i = 0; i < mOvershell->mSlots.size(); i++) {           // <-- same-part rejection
        OvershellSlot *curslot = mOvershell->GetSlot(i);
        BandUser *curuser = curslot->GetUser();
        if (curuser && curuser != mUser) {
            OvershellSlotState *curstate = curslot->GetState();
            if (!curstate->IsPartUnresolved()) {
                if (RepresentSamePart(entry.unk4, curuser->GetTrackType()))
                    return false;      // <-- THE grey-out our patch targets
            }
        }
    }
    // campaign / lesson-mode required-track/score gates ...
    return true;
}
```

- **Yes**, it calls `RepresentSamePart` (`src/band3/game/Defines.cpp:231`,
  equivalence classes `{Guitar,RealGuitar} {Bass,RealBass} {Keys,RealKeys}
  {Vocals} {Drum}` from `GetTracksRepresentativeOfPart`, `Defines.cpp:202`) for
  every *other* overshell slot that has a resolved user, and greys the row if
  it maps to the same instrument family.
- **Screen**: `OvershellPartSelectProvider` is one of `OvershellSlot`'s 4
  registered UI-list providers (`OvershellSlot.cpp:73,92`:
  `mPartSelectProvider = new OvershellPartSelectProvider(mOvershell);
  setupProviders[3] = mPartSelectProvider;`), reloaded per controller-type via
  `mPartSelectProvider->Reload(ty, pUser)` (`OvershellSlot.cpp:1827`) —
  this is the **instrument-select overshell state/panel** (part of the
  `OvershellPanel`/`OvershellSlot` join flow, state machine values
  `kState_ChoosePart` / `kState_ChoosePartWait` in `OvershellSlotState.h`),
  i.e. exactly the screen the patch targets.

---

## Q3 — Ghidra xrefs/callers on BASE xex; vtable dispatch; is the detour point sound?

Base VA `0x8264B5F8`, Ghidra project = base TU0 `default.xex` (sha1
`35adb6b4...`), served on `http://127.0.0.1:8002/mcp`.

- `list_xrefs("0x8264b5f8")` -> **0 CODE xrefs, 1 DATA xref**
  (`820d4c60 -> 8264b5f8`).
- Bytes at `0x820d4c50` (16 words): a run of valid `0x82xxxxxx` code
  addresses including `8264b5d8` immediately before (likely `NumData()`,
  declared immediately before `IsActive` in the header — small getter, plausible
  32-byte gap) and `8264b5f8` (`IsActive`) at slot index — a vtable.
- `search_symbols_by_name` confirms `?IsActive@OvershellPartSelectProvider@@UBA_NH@Z`
  is a USER_DEFINED `Function` symbol at `8264b5f8` with **public-virtual**
  MSVC mangling (`U` flag), matching the header's `virtual bool IsActive(int) const;`.
  Sibling symbols in the same class also present:
  `??0OvershellPartSelectProvider` (ctor, `8264bb78`),
  `??1OvershellPartSelectProvider` (dtor, `8264bc30`),
  `??_GOvershellPartSelectProvider` (scalar deleting dtor, `8264bd18`),
  `?Text@OvershellPartSelectProvider` (`8264b9a0`).
- No direct `bl 0x8264b5f8` call sites exist anywhere in `.text` — **100% of
  invocations are indirect, through the vtable slot**, consistent with Q1's
  finding (all callers hold a `UIListProvider*`, never a concrete
  `OvershellPartSelectProvider*`).

**Is the detour point sound?** Yes, mechanically: `HookFunction`/the static
detour overwrites the function's **first instruction** at its entry VA
(confirmed prologue `mflr r12` at both `0x8264B5F8` (base) and the
TU5-retargeted `0x826684C0` — see §5), branching to the cave. Since a vtable
slot stores the function's *entry address*, not a copy of its code, **every**
virtual call — regardless of caller, regardless of static/dynamic dispatch —
lands at that same entry VA and hits the detour. A first-instruction entry
hook is exactly the right technique for a virtual method; there is nothing
about vtable dispatch that a first-instruction hook fails to catch (it would
only be a problem for *inlined* virtual calls, which don't apply here since
the callers hold a base-class pointer and can't devirtualize at compile time).
**The hook point itself is sound** — assuming the hooked VA is the one
actually present in the booted binary (see the TU0-vs-TU5 VA caveat in the
headline verdict).

---

## Q4 — Does RB3E's BuildInstrumentSelectionList replacement interact with IsActive?

**No — they are separate functions operating in separate phases, and RB3E's
replacement does not touch/bypass IsActive.**

- `BuildInstrumentSelectionList` (RB3E `source/OvershellHooks.c:24-64`,
  `PORT_BUILDINSTRUMENTSELECTION = 0x82668c70`, retail-TU5) is the
  **list-population** step: given a `ControllerType`, it clears/rebuilds the
  slot's candidate-instrument vector (`AddInstrumentToList` pushes
  `{sym, trackType, icon}` entries) — this is RB3E's reimplementation of an
  MSVC-inlined function (no standalone body existed to hook, so they wrote a
  drop-in replacement, per the doc's "MSVC-inlining caveat").
- The Wii-decomp equivalent of this is
  `OvershellPartSelectProvider::Reload(ControllerType, BandUser*)`
  (`OvershellPartSelectProvider.cpp:23-82`) — same shape: `switch(ty)` +
  `AddPart(...)` per case, populating `mPartSelections`. Called from
  `OvershellSlot::UpdateData`-equivalent path via
  `mPartSelectProvider->Reload(ty, pUser)` (`OvershellSlot.cpp:1827`),
  fired whenever the controller type for a slot changes — **once per
  controller-type event, not per frame**.
- `IsActive` is queried **continuously**, per-row, by the generic
  `UIList`/`UIListState` engine every poll (Q1) against whatever list
  `Reload`/`BuildInstrumentSelectionList` most recently populated. It has no
  knowledge of *how* the list was built (RB3E's generic per-controller lists
  vs. the stock per-controller `switch`) — it only walks `mOvershell->mSlots`
  looking for other resolved users with a colliding `RepresentSamePart`
  class. **The grey/active state is computed independently of, and after,
  list construction**, at render/poll time — so RB3E's list-genericization
  hack changes *which instruments are offered as candidates* (e.g. lets a pad
  controller pick guitar/bass/keys/drums), it does **not** change or
  duplicate the same-part rejection logic that Layer A targets. Address-wise
  they're also nowhere near each other in the binary (`0x82668c70` vs.
  `0x8264b5f8`/`0x826684c0`), confirming they're unrelated compiled units.

---

## Q5 — Deluxe (dx_*.dta) angle: native code or DTA-scriptable grey-out gate?

**Native code only — not DTA-scriptable, and RB3 Deluxe does not modify this
code path at all.**

- No `{user ...}`/`HandleType`/`DataFunc` dispatch anywhere in
  `OvershellPartSelectProvider.cpp` — its only DTA/DataArray touchpoints live
  in the sibling `OvershellPanel.cpp` (`player_panels`, `valid_controllers`,
  `joining_priority` arrays — these configure controller-slot UI layout, not
  per-instrument active/grey state) and `OvershellSlot.cpp` (`HANDLE_ACTION`
  entries like `select_part` which *invoke* selection, but the grey/active
  gate itself, `IsActive`, has zero `DataArray`/`Symbol`-table lookups in its
  body — every check (`RepresentSamePart`, `GetTrackType`, `IsPartUnresolved`,
  `TheGameMode->InMode`) is a native member-function/global call, not a
  DTA-array read). So **a DTA-only mod cannot retarget or disable this gate**
  — it would require a native code patch (exactly what our approach does).
- **RB3DX-specific check (direct binary comparison, done 2026-07-07,
  `docs/plans/clean-tu5-vs-rb3dx-divergence.md`):** RB3DX (`band_tu5.exe`) was
  diffed byte-for-byte against a freshly-produced clean retail TU5
  (`band_clean_tu5.exe`, same version/entry/section-table). Only **170 bytes
  differ in the entire 14.36 MB image** (92 in `.text`, 15 in `.rdata`, 63 in
  `.data` — Deluxe's own DLC-cache/gameplay hooks, e.g. a `bne`->`nop` patch at
  `0x82575f9c`, unrelated to Overshell). **All 13 same-instrument-relevant
  functions were explicitly diffed and are byte-identical between clean TU5
  and RB3DX**, including `IsActive` itself at TU5 VA `0x826684C0`. The
  `0x82C8A000` cave region is also an identical all-zero free run in both.
  **RB3 Deluxe's own patches do not touch the instrument-select/grey-out code
  path in any way** — whatever RB3DX changes (DLC cache, etc.) is orthogonal.
  This directly rules out "Deluxe's own UI replaces/bypasses IsActive" as an
  explanation for the observed no-change behavior.

---

## Q6 — Is a 2nd same-instrument selection gated elsewhere even if IsActive alone were forced true?

**Yes — `SelectPartImpl` itself performs NO same-part check, but two further
independent layers (already identified and separately patched per the design
doc) still gate real assignment.**

- `OvershellSlot::SelectPartImpl` (`OvershellSlot.cpp:379-438`) — the function
  invoked when a player presses to select a row — does **not** call
  `RepresentSamePart` or otherwise re-verify the instrument is free. It checks
  unrelated things (critical-user/campaign set-completion state, battle-mode
  instrument gates) then unconditionally does
  `pUser->SetTrackType(track); ... ShowChoosePartWait();` — i.e. **if the row
  is clickable at all (Layer A not greying it), pressing it succeeds and
  advances the user to `kState_ChoosePartWait`.** So forcing `IsActive` alone
  IS sufficient to make the *selection action* go through the UI without an
  immediate rejection.
- However, per the design doc's own architecture (§3.1, already implemented
  as Layers B/C in `RB3Enhanced/source/SameInstrumentHooks.c`), two more
  independent gates fire downstream:
  - **Layer B**, `OvershellPanel::ResolvePartWaitStates` (base
    `0x8259D948` / TU5 `0x825B6488`) — arbitrates every user sitting in
    `kState_ChoosePartWait`; stock logic erases/bounces a colliding selection
    back to `kState_ChoosePart` (source `OvershellPanel.cpp:906-1026`, cited
    in the design doc). **If this detour also didn't install/execute on
    hardware, the 2nd player would silently get bounced right back out of
    `ChoosePartWait`**, which from a player's perspective looks identical to
    "the instrument stayed greyed out" even though Layer A worked — this is
    exactly hypothesis (2) from the headline verdict.
  - **Layer C**, `PlayerTrackConfigList::ProcessConfig` (base `0x8274ACF8` /
    TU5 `0x8276FA08`) — the actual track-assignment gate; without its detour
    the 2nd same-type claimant would `MILO_FAIL` at song start (a hard
    crash) rather than silently fail at the UI. Since hardware showed **no
    crash**, this is consistent with the player never getting past Layer A/B
    in the first place (never reaching song start with two same-type
    claimants) — i.e. it does NOT prove Layer C's detour installed
    correctly; it's equally consistent with Layer A/B blocking selection
    before Layer C is ever exercised.

**Conclusion for Q6:** all 4 layers (A/B/C/centre) are needed for the full
feature; a working Layer-A-only patch would be necessary-but-not-sufficient
for a visible change (selection would succeed past Layer A but then get
bounced by an un-patched Layer B). Given hardware showed *zero* observable
difference, the simplest read is that the Layer-A detour itself isn't taking
effect (never mind B/C) — reinforcing the VA-mismatch concern in the headline
verdict as the top suspect to rule out first.

---

## §5 — TU0 vs TU5 addresses (why this matters for "did we hook the right function")

From `rb3-xenon/docs/plans/same-instrument-tu5-retarget.md` (2026-07-07,
worktree `tu5-migrate`, status COMPLETE/byte-verified) and
`clean-tu5-vs-rb3dx-divergence.md` (2026-07-07):

| Layer | BASE (TU0) VA | **TU5 VA** (retail/RB3DX) |
|---|---|---|
| A — IsActive | `0x8264B5F8` | **`0x826684C0`** |
| B — ResolvePartWaitStates | `0x8259D948` | **`0x825B6488`** |
| C — ProcessConfig | `0x8274ACF8` | **`0x8276FA08`** |
| centre — RecalcGemList | `0x8276FBB0` | **`0x82794740`** |

- TU5's XEX is section-mapped with **non-uniform per-section file offset
  deltas** (`.text +0x6200`, `.data +0xD400` relative to a naive
  `0x3000+VA` reader) — a flat-VA patcher/reader silently drifts by exactly
  `-0x8000` on TU5, landing mid-function (this bit an earlier "INGEST" attempt
  and was corrected).
- IsActive's **compiled body diverged ~56%** between TU0 and TU5 (different
  register allocation/codegen from a recompile across title updates) — but
  this is **irrelevant to our patch** because the hook overrides the entire
  function via a first-instruction branch, not a body-internal byte patch;
  only the **entry address and calling convention** need to match, both of
  which were independently byte-verified on TU5 (`mflr r12` prologue at
  `0x826684C0`, `IsActive(int) const -> bool` in r3).
- **RB3Enhanced's own `include/ports_xbox360.h`** (`PORT_OVERSHELL_ISACTIVE`)
  currently holds `0x8264B5F8` — **the BASE/TU0 address**, per
  `tu5-execution-status.md`'s "Flagged (not fixed)" note: *"RB3Enhanced
  ports_xbox360.h mixes TU5 general ports with BASE same-instrument
  addresses."* A retail TU5/RB3DX console loading a patch built from that
  header would write its detour at the **wrong VA** for that binary.
- The **correct, byte-verified, TU5-targeted artifact** is
  `orig/45410914/default_tu5_patched.xex` (sha1
  `a9fa9a91863cbe727377420bd6debe2790ffeac1`) built via
  `RB3Enhanced/scripts/objcave_pack_tu5.py` +
  `RB3Enhanced/patches/45410914_same_instrument_full_tu5.patch.toml`, using
  TU5 VA `0x826684C0` for IsActive and cave `0x82C8A000` (file-backed `.data`
  zero run, confirmed unreferenced by `.text`, confirmed all-zero in **both**
  clean TU5 and RB3DX). This patch was **never runtime-tested** as of
  `tu5-execution-status.md` (2026-07-07) — its own §"Remains unverified
  (runtime)" explicitly lists "Xenia boot-spike to confirm the `.data` cave
  executes" and "runtime gameplay confirmation" as still-open.

**This is the single highest-priority thing to check before any further
Layer-A debugging**: which VA/artifact did the hardware test actually run —
the header's stale `0x8264B5F8` (base/TU0, wrong for a TU5 console — would
corrupt or miss an unrelated function) or the byte-verified TU5 retarget
`0x826684C0`? If it was the former, "identical to stock, no crash" is fully
explained without needing any deeper IsActive-callgraph hypothesis: the
detour simply isn't at the entry IsActive actually lives at on the booted
binary, and depending on what *is* at `0x8264B5F8` on a TU5 image (mid-body
of some unrelated function, given the `+0x6200` text-section delta), a
corrupted first instruction there may not even be reachable/exercised enough
to crash during a short test session.

---

## Sources consulted

- `rb3/src/band3/meta_band/OvershellPartSelectProvider.{h,cpp}`
- `rb3/src/band3/meta_band/OvershellSlot.cpp` (SelectPart/SelectPartImpl/Reload
  call sites, lines ~369-454, ~1827)
- `rb3/src/system/ui/UIListProvider.h`, `UIList.cpp`, `UIListState.cpp`,
  `UIListSlot.cpp`, `UIListWidget.cpp`
- `rb3-xenon/docs/plans/rb3enhanced-same-instrument-patch.md` (design doc,
  1134 lines, §3.1 Layers A/B/C, §8 address-derivation methodology)
- `rb3-xenon/docs/plans/same-instrument-tu5-retarget.md` (TU5 retarget,
  byte-verified addresses, cave location correction)
- `rb3-xenon/docs/plans/clean-tu5-vs-rb3dx-divergence.md` (RB3DX vs clean-TU5
  170-byte diff; same-instrument surface byte-identical in both)
- `rb3-xenon/docs/plans/tu5-execution-status.md` (execution status,
  "Flagged (not fixed)" base-vs-TU5 address mixing note)
- `RB3Enhanced/source/OvershellHooks.c`, `SameInstrumentHooks.{c,h}`,
  `include/ports_xbox360.h`, `scripts/objcave_pack_tu5.py`,
  `patches/45410914_same_instrument_full_tu5.patch.toml`
- Ghidra MCP (rb3-xenon, port 8002, base TU0 `default.xex`
  sha1 `35adb6b4...`): `decompile_function 0x8264b5f8`, `list_xrefs
  0x8264b5f8` / `0x820d4c60`, `read_bytes 0x820d4c50 64`,
  `search_symbols_by_name OvershellPartSelectProvider`
