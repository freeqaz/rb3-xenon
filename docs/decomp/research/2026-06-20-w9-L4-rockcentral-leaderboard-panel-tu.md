# W9 L4 — "rockcentral-leaderboard-panel-tu" frontier drill (2026-06-20)

Baseline: main @ 8314 matched. READ-ONLY adversarial drill. Verdict: **REAL_ACTIONABLE**.

## TL;DR
The frontier mis-named the TU and mis-bounded the span. The unpinned panel TU below
SongUpgradeMgr is **`AppMiniLeaderboardDisplay.cpp`** (game, `band3/meta_band/`), NOT a
"RockCentral leaderboard panel". The frontier's claimed span (0x826301E0–0x826308C0,
18 fns) is only the UPPER HALF; the real TU spans **0x8262F530 – 0x82630990 (46 fns)** and
is interleaved/adjacent with its base class TU **`MiniLeaderboardDisplay.cpp`** (engine,
should live `src/system/bandobj/`), which is ALSO unwired and sits just below at
~0x8262E3xx–0x8262F508.

## Ground-truth evidence (COFF auto_03 text obj + auto_00 rdata + relocations)

Owner TU = AppMiniLeaderboardDisplay.cpp, confirmed by string/reloc tells:
- **fn_826301E0 = `AppMiniLeaderboardDisplay::Update()`** — references rdata strings
  `leaderboard`(0x820A211C), `title_label`(0x820CE908), `icons_label`(0x820CE8FC) — exactly
  the `t->FindArray(X,true)->Str(1)` lookups in Update (oracle lines 182–200).
- **fn_826305E0 = `AppMiniLeaderboardDisplay::Handle`** — references `fade_in`(0x820CEA04),
  `fade_out`(0x8205C234), `update_leaderboard`(0x820CE9F0) = the BEGIN_HANDLERS block
  (oracle lines 219–225); calls base via fn_8262FEB8.
- **fn_826308C0 = `PlayerMiniLeaderboard::EnumerateFromID()`** — calls **fn_824E88E8** which
  is INSIDE the pinned `RockCentral.cpp` range (0x824E8468–0x824EA844) =
  `RockCentral::GetLeaderboardByPlayer` (oracle lines 231–239).
- **fn_8262FEB8 = `AppMiniLeaderboardDisplay::UpdateLeaderboard`** — references
  `mini_leaderboards_title*`(0x820CE818) = `mini_leaderboards_title_friends` (oracle line 127).

Span / boundary tells:
- AppMini primary vtable at rdata head **lbl_820CE5BC** (0x820CE5DC..0x820CE610), last
  virtual slot = Update(0x826301E0), terminated by `00000000 / FFFFFFFF`. Multiple-inheritance
  second vtable (`Leaderboard::Callback` sub-object) holds **fn_8262F64C / fn_8262F678**
  (= ResultSuccess/ResultFailure) — proving the TU reaches DOWN into 0x8262Fxxx.
- The AppMini vtable head `lbl_820CE5BC` is loaded by **fn_8262F530** and **fn_8262FAC0**
  (the ctor / dtor / NewObject) — so the TU's lower edge is ≈ **0x8262F530** (COMDAT marker
  `except_data_8262F530` at 0x8262F528 confirms a fresh COMDAT cluster start there).
- Just below: **fn_8262F4B0 = `MiniLeaderboardDisplay::ClassName`** references the BASE
  classname string `MiniLeaderboardDisplay`(0x8202FFE8) — i.e. the base-class TU.
- Full AppMini fn list (46 top-level fns, incl. ~14 trivial Symbol-accessor / vcall-thunk
  fns at 0x826304F8–0x826305B8 and 0x826307E4–0x82630880):
  0x8262F530,F64C,F678,F6A8,F6D8,F770,F788,F7D8,F838,F848,F8B0,F940,FA7C,FAA0,FAC0,FC20,
  FC64,FCA8,FD08,FD70,FE88,FEB8, 0x82630014,38,50,C0,E8,138,1E0,4F8,518,538,558,578,598,5B8,
  5E0,7E4,804,824,844,870,880,8C0,95C,988.

## Oracle (game code → rb3-Wii is authoritative; DC3 is a FALSE FRIEND here)
- **`../rb3/src/band3/meta_band/AppMiniLeaderboardDisplay.cpp`** (239 lines) — clean, exact
  RB3 source. Classes in TU: AppMiniLeaderboardDisplay, PlayerMiniLeaderboard::EnumerateFromID,
  Leaderboard::IsEnumComplete/ShowsDifficultyAndPct.
- DC3 `../dc3-decomp/src/lazer/meta_ham/AppMiniLeaderboardDisplay.cpp` (344 lines) is a
  DIFFERENT (job-based: GetMiniLeaderboardJob/mLBRows/OnMsg/UpdateData/Text) rewrite — **do
  NOT use it**, it will not match RB3.
- Base **`../rb3/src/band3/.../MiniLeaderboardDisplay.cpp`** lives at
  `../rb3/src/system/bandobj/MiniLeaderboardDisplay.{h,cpp}` (62-line .cpp; clean RB3 layout:
  `bool mAllowSoloScores`, virtuals SyncProperty/Save/Copy/Load/PreLoad/PostLoad/DrawShowing).

## ⚠ Layout trap (the matching risk)
Our tree already has `src/band3/meta_band/AppMiniLeaderboardDisplay.h` (IDENTICAL to rb3-Wii,
member offsets 0x114–0x140 annotated). But the base header in our tree is **DC3's newer
`src/system/hamobj/MiniLeaderboardDisplay.h`** with extra `ResourceDirPtr<RndDir> mResourceDir
// 0x44`, `UICOMP_DC3_VIRTUAL OldResourcePreload`, `OBJ_MEM_OVERLOAD(0x11)` — a DC3-added-member
layout that will SHIFT AppMini member offsets and break matching. AppMini's `.h` includes
`bandobj/MiniLeaderboardDisplay.h` (rb3-Wii path), which does NOT exist in our tree (only the
hamobj DC3 one does). The work-item MUST port the rb3-Wii **bandobj** base header+cpp, not lean
on the DC3 hamobj header. UIComponent (base-base) is already wired+pinned (layout established).

## Wired/pin status
- `RockCentral.cpp` pinned 0x824E8468–0x824EA844 (the GetLeaderboardByPlayer callee lives here).
- AppMiniLeaderboardDisplay.cpp: header in tree, **.cpp NOT wired, NOT pinned**.
- MiniLeaderboardDisplay.cpp (base): NOT wired, NOT pinned, NO bandobj header in tree.
- Deps present: Leaderboard.h, PlayerLeaderboards.h, AppLabel.h, Profile.h, EventTrigger/Group/UIList.

## Yield estimate
Frontier said +14. Real span = 46 fns; ~14 are trivial Symbol-accessor/vcall thunks that match
near-free once layout is right, plus Update/Handle/UpdateLeaderboard/ctor/dtor/EnumerateFromID
real bodies. Realistic **+18..+30** for AppMini alone (some funclets/permuter-class will defer);
+base MiniLeaderboardDisplay (Save/Load/Copy small class) another **+6..+12**. Pin must use the
auto-derived pdata back-fill; attribution_risk=true (new pin + relocation into the 0x8262Fxxx blob).
