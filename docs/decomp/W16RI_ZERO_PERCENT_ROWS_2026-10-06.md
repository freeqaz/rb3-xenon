# W16-RI: in-scope 0% rows in compiled units (2026-10-06)

Branch `w16-ri`, worktree `~/tmp/wt-w16ri`, off main `dd88a0ee7`.

**Task:** take the largest 0% rows in compiled units, name each one from retail
bytes (callers, vtables, RTTI, strings) before writing it, and skip XDK and
Quazal. The listed rows were DataNode `fn_82C40098`, UI `fn_823F5460`,
Memory_Xbox AllocType / AllocAlign, EventTrigger `fn_8249B200`, and the 0% rows
in WavMgr, BandUser, SyncStore, Voice, TrackPanelDir and CheatProvider.

## Result

| commit | kinds | A/B (ab_measure `--revert`, name_check) |
|---|---|---|
| `6092f80ec` Memory_Xbox source | source | **+7 fns / +2,384 B** |
| `4e1fc108b` map + splits | map, splits | **+20 fns / +3,132 B** (`none` control +2,032 B) |
| `326a566b4` AutoplayAuditionUser | map, source | **+20 fns / +1,176 B** |
| `7dd478cda` TrackPanelDirBase implicit dtor | map, source | row diff on one tree: +140 B −8 B, Δfns 0 (not A/B'd separately) |
| **whole lane** (reverse patch vs `dd88a0ee7`) | all | **+36 fns / +3,316 B** (+0.032360 pp code%, +32 honest; `none` control +3,272 B) |

The per-commit figures overlap and do not add up: Memory_Xbox's four rows need
both the source in `6092f80ec` and the map names in `4e1fc108b`. The whole-lane
row is the number to quote. No unit fell off 100% in any run.

Whole-lane units: BandUser +20, PropSync +6, EventTrigger +4, Memory_Xbox +3,
system/obj/Dir +2, Voice +1. TrackPanelDir is +0 functions: +140 B for the
destructor, −8 B for SetTrackPanel.

Rnd_Xbox, ShaderMgr and XMAReader each read +1 in the `6092f80ec` A/B but
**net 0** over the whole lane. Before the lane, alias group 0x822733b8 folded
the `PhysicalFree` spelling into `PhysicalFreeTracked`, which forgave those call
sites calling the wrong callee. The lane withdraws that unproven membership
(retail has two distinct bodies) and fixes the call sites, so the score is
unchanged but the callees are now correct.

Measured with `tools/ab_measure.py --worktree ~/tmp/wt-w16ri --patch <git diff
HEAD dd88a0ee7>`: leg A = lane applied (54,750 / 58.883920%), leg B = lane
reverted (54,714 / 58.851560%). Leg B recompiled 318 TUs and did one re-split;
both legs read at the split fixed point.

## Row dispositions

### Written / named to 100

- **Memory_Xbox** (4 rows):
  - `?AllocType@?A0x7a439e55@@YAPBDK@Z` (0x82272EE8, 984 B) and
    `?AllocAlign@?A0x7a439e55@@YAHK@Z` (0x822734E0, 208 B), rewritten as a
    `0x80`-based switch with a signed `isPhys` test. Our earlier source compiled
    to 1,164 B and 224 B. A signed `(int)(x & 0x80000000) != 0` gives retail's
    `addic`/`subfe`, and the unsigned form gives `srwi`.
  - `?PhysicalFree@@YAXPAX@Z` (0x82273470, 68 B) and
    `?PhysicalFreeTracked@@YAXPAX@Z` (0x822733B8, 84 B).
    - Retail's tracked free takes one argument (null-checked usage decrement,
      `XPhysicalFree`, `MemTrackFree`). The four-argument spelling stays only
      under `HX_NATIVE`, with a one-argument inline forwarder.
    - `Rnd.h`, `Rnd_Xbox.cpp` and `XMAReader.cpp` call sites now use it, so
      Rnd_Xbox, ShaderMgr and XMAReader each gained a row.
  - Alias group 0x822733b8 was rewritten. `PhysicalFree` was withdrawn as
    `DISTINCT_RETAIL_BODY`, because it has its own retail body at 0x82273470,
    and the four-argument spelling as `SPELLING_RETIRED`.
- **EventTrigger / WorldDir / MsgSource** (12 rows, map and splits only):
  - `?StartAnim@EventTrigger@@UAAXXZ` is 0x8249B200, vtable slot 1. It was
    lifted from `_denylist`, with the rationale appended to
    `_denylist_comment`.
  - EventTrigger also gained `??1HideDelay@EventTrigger@@QAA@XZ`, three
    `??_G?$ObjOwnerPtr<T>` deleting destructors, and the ProxyCall
    `_Copy_Construct` and `list<Anim>::_M_create_node` rows.
  - Two WorldDir `MatOverride`/`BitmapOverride` STL helpers (0x824CD738,
    0x824CD7A8, 0x824CDCE8) were re-pinned from EventTrigger.cpp to
    PropSync.cpp, where our object defines them.
  - `?NewObject@MsgSource@@SAPAVObject@Hmx@@XZ` (0x8274DF40) was re-pinned to
    `system/obj/Dir.cpp`.
- **Voice** `??3EnvelopeGenerator@@SAXPAX@Z` (0x82B64C20, 12 B), which is
  `PoolFree(0x94, p)`.
- **BandUser** `AutoplayAuditionUser` (18 rows to 100). This is a TU5 class
  derived from `NullLocalBandUser`, with a `ControllerType` member at 0x24 and
  eight constant overrides.
  - It was named from its vtable and its retail constructor and destructor.
    The class name and the factory name `NewAutoplayAuditionUser` are ours,
    because no RTTI string survives.
  - 16 rows mapped: the ctor, ~, `??_D`, and 13 `$4`/`$R4` vtordisp thunks.
  - `?IsNullUser@NullLocalBandUser@@UBA_NXZ` was admitted to fold group
    0x82533618 (CHASED T1 PROVEN, plus the slot-28 vtable witness).
  - BandUser unit: 182 → 202 rows, 16,376 → 17,552 B.
- **TrackPanelDir** `??1TrackPanelDirBase@@UAA@XZ` (0x823084F0, 140 B).
  - Retail's destructor tears down members with **no vfptr re-stores**.
    MSVC emits the re-stores for a user-declared `virtual ~X() {}` (ours
    compiled to 288 B) but **not for the implicit destructor**.
  - Removing the user-declared destructor gives a byte match.
  - Two `vector<ObjPtr<T>>` destructor spellings (GemTrackDir, BandTrack) were
    admitted to group 0x822d8cc0 (CHASED T1 PROVEN, 136 B).

### Recorded, not written

- **TrackPanelDir `?SetTrackPanel@TrackPanelDirBase@@` (0x823038A0, 8 B): this
  row went from 100 to 0 as a side effect of the destructor fix.**
  - Once the user-declared destructor is gone, our TrackPanelDir.obj no longer
    references `??_7TrackPanelDirBase`. It therefore stops emitting that
    inline virtual, which now exists only in TrackPanelDirBase.obj.
  - Retail's only copy is referenced once, from TrackPanelDirBase's own
    vtable slot (0x8203C960). That copy sits at the first address of
    TrackPanelDir.cpp's range, right after the VocalTrackDir.cpp block.
  - A scan of retail's TrackPanelDir and VocalTrackDir `.text` found **no**
    `lis`/`addi` pair forming any address in 0x8203C700..0x8203CC00, and
    **no** `bl` to 0x823038A0.
  - So why retail kept TrackPanelDir.obj's copy is **unexplained**.
  - The pin was **not** re-homed to TrackPanelDirBase.cpp, because that would
    misstate retail TU membership to collect 8 B. Net for the commit: +132 B.
- **BandUser** `?UserName@NullLocalBandUser@@$4PPPPPPPM@3BAPBDXZ` stays at
  98.75.
  - Its `b` target folds with `ContentPattern`, and
    `icf_pair_adjudicate --chase` refutes that fold as **VACUOUS**: the bodies
    are 2–3 words with a placeholder string slot.
  - No alias was admitted.
- **BandUser** `AutoplayAuditionUser` factory (0x8268F0D0) stays at 0.
  - The name cannot be established from retail bytes: there is no RTTI and
    no string, and the only caller is anonymous.
  - It was deliberately left unmapped. Naming an anonymous address is a bet.
- **EventTrigger `fn_8249BBB0`** was deliberately left unnamed. It is the
  `ObjPtrList<T>` `operator<<` fold survivor, and our callers spell it 17+
  different ways, so any one name is a fabricated closure.
- **DataNode `fn_82C40098`** (1,372 B): this row is **not DataNode**.
  - It is a `??__E` dynamic initializer that fills a 0x740-byte per-genre
    subgenre table at 0x82C76408 (`[29][16]`; rb3's dev source has a flat
    `gSubGenreStrs`).
  - The table's only other user is `fn_8272C8E8` (15 KB, in an `auto_`
    unit). That function belongs to the UGC song validator TU (assert strings
    `IsValidEnum( strSubGenre, subGenres, kUGCNumSubGenres, idx )`,
    `kUGCRankMin`, etc. at 0x820FF588..).
  - That TU has no source in rb3, rb3-xenon or dc3-decomp.
  - The pin was dropped. The row now reads as `auto_03_82C40098_text`, which
    is neutral: it moves from one place in the denominator to another.
- **UI `fn_823F5460`, and the WavMgr / CheatProvider / UI / TrackPanelDir /
  SyncStore rows at 0x823E9xxx–0x823F5xxx**: **Quazal, skipped.**
  - These addresses populate `_DOC_MessageBroker@Quazal` (vtable 0x820594B4),
    `HarmonixGameDDLDeclarations@Quazal` (0x82059594) and
    `RockBandDDLDeclarations@Quazal` (0x8219D4AC) slots.
  - This is the Xbox net-messenger layer. The pins come from an earlier
    gap-fill commit and should be re-homed by a map lane.
- **Voice 0x82C42958..0x82C42D68**: `/Od`-codegen static initializers (a fresh
  `lis` per access, `atexit` registration). These are Quazal-style, so they
  were skipped.

## What was deliberately not done

- Quazal reverse engineering: none.
- Pins were not re-homed to collect a score; the TrackPanelDirBase SetTrackPanel row above is the case in point.
- No alias was admitted without a CHASED T1 PROVEN adjudication. The
  `UserName`→`ContentPattern` alias was refused.
- No merge, no push.

## Native gate

Run after the last source commit (`7dd478cda`); this doc-only commit comes after it:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```
