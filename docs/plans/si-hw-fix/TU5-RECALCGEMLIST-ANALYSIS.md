# TU5 RecalcGemList Non-Execution Analysis + Real Song-Load mGemList Path

**Date:** 2026-07-15
**Question:** Why is the function at VA `0x82794740` (TrackWatcherImpl::RecalcGemList) never
executed during gameplay, and what is the REAL code path that (re)assigns
`TrackWatcherImpl::mGemList` at song load?

**Verdict (Hypothesis A confirmed):** `0x82794740` is genuinely
`TrackWatcherImpl::RecalcGemList`, but its only caller is
`BeatMatcher::ResetGemStates`, which fires on difficulty change / practice-section
restarts / PostDynamicAdd — **never at song load**. At song load, `mGemList@+0x1c`
is assigned exactly once by the **TrackWatcherImpl base constructor (`0x82797298`)**
from a gemlist parameter that **`NewTrackWatcherImpl` (`0x8279d990`)** fetched via a
single `bl SongData::GetGemList`. Hypotheses B (dead copy) and C (patched caller
sites) are killed.

All analysis performed by direct byte analysis of the clean TU5 basefile:
`/home/free/code/milohax/rb3-xenon/_tu5probe/clean/band_clean_tu5.exe`
cross-referenced against named decomp source in
`/home/free/code/milohax/rb3/src/system/beatmatch/` (TrackWatcher.cpp,
TrackWatcherImpl.cpp, BeatMatcher.cpp).

---

## 1. Calibrated VA <-> file-offset mapping

The clean TU5 basefile is a PE32 XBOX image (`MZ`, 12 sections), imagebase
`0x82000000`. Mapping is **per-section linear** (verified from the PE section
table, not assumed):

| Section  | VA start     | vsize        | raw file ptr | delta (VA − file_off) |
|----------|--------------|--------------|--------------|-----------------------|
| .rdata   | `0x82000400` | `0x001f1184` | `0x00000400` | `0x82000000`          |
| .pdata   | `0x821f1600` | `0x00070c28` | `0x001f1600` | `0x82000000`          |
| **.text**| `0x82270000` | `0x009dce3c` | `0x00264e00` | **`0x8200b200`**      |
| .data    | `0x82c64400` | `0x001f5eac` | `0x00c52000` | `0x82012400`          |
| .reloc   | `0x82e70200` | `0x00104098` | `0x00caea00` | `0x821c1800`          |

For all code work: `file_off = VA − 0x8200b200`. Do **not** reuse the .text delta
outside .text.

Sanity checks (passed):
- `0x82794740` → foff `0x789540` → `7d 88 02 a6` (`mflr r12`), body:
  `80830068` (`lwz r4,0x68(r3)` mTrack), `80630050` (`lwz r3,0x50(r3)` mSongData),
  `4bfdbfd5` (`bl 0x82770730` GetGemList), `907f001c` (`stw r3,0x1c(r31)` mGemList).
- `0x82770730` (SongData::GetGemList) → foff `0x765530` → `81630050` (`lwz r11,0x50(r3)`),
  `812300b0` (`lwz r9,0xb0(r3)`) — matches documented GetGemList body.

**IMPORTANT caveat for future work:** `rb3-xenon/config/45410914/symbols.txt`,
`rb3-xenon/unified_id.json`, and the `RB3Xenon` Ghidra project are all **TU0
base**, not TU5 — their addresses do NOT line up with the TU5 VAs in this doc
(TU5 has .text reordering). Every VA below is TU5, byte-verified.

## 2. Callers of RecalcGemList impl `0x82794740`

Full 4-byte-aligned I-form (opcode 18) branch scan of the entire `.text` section:

- **Exactly ONE caller: `0x8279d6a4`** — and it is a plain **`b` (tail branch,
  not `bl`)**: instruction bytes `4bff709c`.
- Containing function: **`0x8279d6a0` = `TrackWatcher::RecalcGemList`** — a
  2-instruction thunk:
  ```
  0x8279d6a0: 80630000   lwz r3,0x0(r3)     ; r3 = this->mImpl
  0x8279d6a4: 4bff709c   b   0x82794740     ; tail-branch to impl
  ```
  This is source-level `void TrackWatcher::RecalcGemList() { mImpl->RecalcGemList(); }`
  (`TrackWatcher.cpp:117`).
- **Data/vtable refs to `0x82794740`:** exactly one whole-file match for the
  big-endian bytes `82 79 47 40`, at foff `0x23f600` — which is inside
  **`.pdata`** (function-bounds/exception record), NOT a vtable. **No vtable
  slot, no virtual dispatch, no `bl` callers anywhere.** The thunk `0x8279d6a0`
  has zero data refs.

The thunk `0x8279d6a0` itself has **exactly ONE caller: `0x82790590`**, inside
**`0x82790570` = `BeatMatcher::ResetGemStates`**:

```
0x82790570: 7d8802a6   mflr r12                 ; func start
...
0x82790588: 80630044   lwz r3,0x44(r3)          ; r3 = this->mWatcher
0x82790590: 4800d111   bl  0x8279d6a0           ; mWatcher->RecalcGemList()
0x82790594: 807f0044   lwz r3,0x44(r31)
0x8279059c: 4800d13d   bl  0x8279d6d8           ; next mImpl-> thunk (vtable +0xc)
...
0x827905b4: 4e800020   blr
```

Exact match to `BeatMatcher.cpp:144-146`.

`ResetGemStates (0x82790570)` is called from only two sites: `0x826be558` and
`0x82790dd4` — BeatMatcher difficulty-change / section-restart / PostDynamicAdd
paths. **NOT the song-load construction path.**

### Complete reachability of 0x82794740

```
BeatMatcher::ResetGemStates (0x82790570)
  └─ bl 0x8279d6a0  TrackWatcher::RecalcGemList thunk   [sole caller: 0x82790590]
       └─ b  0x82794740  TrackWatcherImpl::RecalcGemList [sole caller: 0x8279d6a4]
```

ResetGemStates only fires on difficulty change / practice loops — never during a
normal song load or steady gameplay. That is exactly why the detour's
`gSISetupSeen @0x84023B60` stays 0 during gameplay. Note both the impl **and**
the thunk land on the detour (tail-branch dispatch lands at the function), so
the detour is correctly placed for the *reset* path; it simply never runs at
song load.

## 3. The REAL song-load mGemList assignment path

`mGemList (this+0x1c)` is written at song load **only in the TrackWatcherImpl
base constructor**, from a `gemlist` parameter that `NewTrackWatcherImpl`
fetched once. Source model (`TrackWatcher.cpp`):

```
BeatMatcher::SetTrack → new TrackWatcher(...) → ctor → SetImpl()
  → mImpl = NewTrackWatcherImpl(track, u, slot, cntType, data, parent, cfg)
      → gemList = data->GetGemList(track)      // ONCE
      → new {Guitar,Joypad,DrumFill,Keyboard,RealGuitar}TrackWatcherImpl(..., gemList, ...)
          → ... → TrackWatcherImpl::TrackWatcherImpl(...)  // mGemList(gemlist) in init-list
```

TU5 pins (all byte-verified):

| Function | TU5 VA | Evidence |
|---|---|---|
| `NewTrackWatcherImpl` | **`0x8279d990`** | prologue `7d8802a6 4808b89d` (`mflr r12; bl 0x82829230` savegpr). Calls `ControllerTypeToTrackWatcherType (0x8279d808)` at `0x8279da78`; **single `bl 0x82770730` GetGemList at `0x8279da94`** (`4bfd2c9d`), result kept in r30; 5-way type switch with `operator new (0x827bd2f0)` of sizes `0xf8/0xdc/0xe0/0x104` + subclass ctor calls. Sole caller: `0x8279e038`. |
| `TrackWatcher::SetImpl` | `0x8279df28` | contains the `bl 0x8279d990` at `0x8279e038`. Its two callers (`0x827915dc`, `0x827915f8`) both live in func `0x82791490` (TrackWatcher ctor / ReplaceImpl region). |
| `ControllerTypeToTrackWatcherType` | `0x8279d808` | SystemConfig lookup + joypad_guitar→guitar remap; sole caller `0x8279da78`. |
| Guitar subclass ctor | `0x827a0fc8` | called from `0x8279dadc` and `0x8279dc70` (guitar + real_guitar-with-tracktype≤1 branches); size 0xf8 `operator new`. Chains `bl 0x827a14b0`. |
| Joypad intermediate ctor | `0x827a14b0` | chains `bl 0x82797298` at `0x827a14cc`. |
| Other subclass ctors | `0x827a0c70`, `0x827a0668`, (drumfill/keyboard/realguitar per new-sizes `0xdc/0xe0/0x104`) | funnel to base ctor via direct callers below. |
| **`TrackWatcherImpl` base ctor** | **`0x82797298`** | prologue `7d8802a6 48091fa1`. `0x827972ac: 7c7e1b78 mr r30,r3` (this→r30); **`0x8279730c: 911e001c stw r8,0x1c(r30)` = `mGemList = gemlist` (r8 = 5th int param)**. Adjacent: `90de0018` (r6→+0x18 mPlayerSlot), `913e0020` (r9→+0x20 mParent), `92de0028` (r22=-1→+0x28 mLastGemHit) — exact init-list match. |

Base ctor direct callers (the universal funnel — all 5 concrete watcher types
pass through here):

- `0x827800d4` in subclass ctor starting `0x8277fbe0`
- `0x827a0684` in subclass ctor starting `0x827a0668`
- `0x827a14cc` in subclass ctor starting `0x827a14b0`

### No inlined RecalcGemList clone exists (exhaustive)

- All **21** branch sites targeting `GetGemList 0x82770730` were dumped and
  classified: `0x82684f6c(b) 0x82684f88 0x82685378 0x826853b0 0x82685620
  0x8268592c 0x82685b10 0x82686b40 0x82686d08 0x82686f60 0x826cddd0 0x8277c260
  0x8277eef0 0x8277f008 0x82790ecc 0x82790f88 0x82791028 0x827910f4 0x82791348
  0x8279475c 0x8279da94`.
- All **1223** `stw *,0x1c(...)` instructions in `.text` were scanned for a
  preceding (≤7 insns) gem-list call. **The only GetGemList→`stw *,0x1c` pair
  in the entire image is `0x8279475c`/`0x82794760`** — inside the impl at
  `0x82794740` itself.
- The ctor writes `+0x1c` from a *register parameter* (decoupled from any
  GetGemList call) — that is why no second clone shows up.
- The `0x82790ecc–0x82791348` GetGemList cluster uses different offsets
  (mTrack@+0x60, mSongData@+0x24 — a different class, likely BeatMatcher/
  TrackWatcher level) and *iterates* the returned list; the `0x82684x–0x82686x`
  cluster loads `+0x4` off the result. None touch `+0x1c`.
- `GameGemDB::GetDiffGemList (0x827931c8)` callers (17 sites, mostly inside
  SongData/GameGemDB internals at `0x82770xxx/0x82771xxx/0x827753xx/0x827754xx`)
  — none store to a watcher `+0x1c` either. `GameGemDB::Duplicate (0x827932c8)`
  callers: `0x82772520`, `0x8277257c`. `GameGemList::CopyFrom (0x8278e168)`
  caller: `0x8279333c` (inside GameGemDB, per Duplicate implementation).

## 4. vtable / data references

None of `0x82794740`, `0x8279d6a0`, `0x8279d990` appear in any vtable. Whole-file
byte searches (aligned + unaligned) for their big-endian addresses hit only
`.pdata` records (`0x23f600` for the impl, `0x23fce0` for NewTrackWatcherImpl).
Dispatch to RecalcGemList is 100% direct (thunk tail-branch), never virtual.
(The virtual thunks in the `0x8279d6a8–0x8279d7fc` block dispatch *other*
TrackWatcherImpl virtuals via vtable offsets `+0xc..+0x4c`; RecalcGemList is not
among them.)

## 5. Hypothesis verdict

**A — CONFIRMED.** Real function, real (single) caller, but the caller only runs
on difficulty-change/section-restart. Song-load assignment happens in the base
ctor from NewTrackWatcherImpl's one-time GetGemList result.
**B killed** — not a dead/cold copy; there is no other RecalcGemList body.
**C killed** — the sole caller chain in the TU5 file is intact and normal; no
foreign patches on the caller sites (RB3DX byte-diff is confined to unnamed
regions per prior verification).

Optional live spot-check commands (not yet run; expected values from the file):

```
python3 /home/free/code/milohax/xex-patcher/tools/xbdm_cmd.py 192.168.8.180 'getmem addr=0x82790590 length=4'   # expect 4800d111
python3 /home/free/code/milohax/xex-patcher/tools/xbdm_cmd.py 192.168.8.180 'getmem addr=0x8279da94 length=4'   # expect 4bfd2c9d
python3 /home/free/code/milohax/xex-patcher/tools/xbdm_cmd.py 192.168.8.180 'getmem addr=0x8279d6a4 length=4'   # expect 4bff709c
```

## 6. Recommended re-pin (hook plan)

Keep the existing detour at `0x82794740` — it is the correct choke-point for the
**reset path** (every ResetGemStates re-borrows `mGemList = shared`, so a private
clone must be re-asserted there). But it never fires at song load, so add a
**second hook on the construction path**:

1. **PRIMARY: hook `NewTrackWatcherImpl` at `0x8279d990` (wrap / post-return).**
   - Clean 8-byte-safe prologue: `7d8802a6 4808b89d` (`mflr r12; bl savegpr` —
     note the `bl` in slot 2 is PC-relative; a trampoline must relocate/re-encode
     it, or detour with `b` and replay `mflr r12; bl 0x82829230` equivalently).
   - Exactly one caller (`0x8279e038`), no vtable refs.
   - On return, r3 = fully-constructed impl with `mGemList@+0x1c` set; entry
     params give track (r3) and SongData (r7-ish per ABI order
     `(int track, const UserGuid& u, int slot, Symbol cntType, SongData*, parent, cfg)`).
   - Post-original: if this watcher is a 2nd+ claimant of its track, overwrite
     `impl+0x1c` with the private per-difficulty clone list. This is the
     song-load install the current scheme is missing.
2. **EQUIVALENT ALT: hook base ctor `0x82797298`** (prologue `7d8802a6 48091fa1`,
   universal funnel for all 5 subclasses). At entry: r3=this, r4=track,
   r8=gemlist, r7=SongData (per init-list stores: r6→+0x18 slot, r8→+0x1c
   gemlist, r9→+0x20 parent). Run original, then re-point `this+0x1c`.
   Caveat: object not fully initialized at base-ctor time (derived vtable/fields
   set afterwards), so the NewTrackWatcherImpl post-return wrap is cleaner.
3. **Do NOT hook the thunk `0x8279d6a0`** — only 8 bytes (`lwz` + tail-`b`), no
   save prologue; not safely detourable.
4. Coarser fallbacks if a single site is wanted: `TrackWatcher::SetImpl
   0x8279df28`, or one level up in func `0x82791490` (TrackWatcher ctor /
   ReplaceImpl), or `BeatMatcher::SetTrack` (BeatMatcher.cpp:314 → `new
   TrackWatcher` at :320).

**Net:** `0x82794740` (reset re-borrow) **+ `0x8279d990` (song-load
construction, return-wrap)** together cover every write to
`TrackWatcherImpl::mGemList`.

---

## Appendix: quick-reference TU5 VA table

| Symbol | TU5 VA | file offset | first bytes |
|---|---|---|---|
| TrackWatcherImpl::RecalcGemList | `0x82794740` | `0x789540` | `7d8802a6 9181fff8` |
| TrackWatcher::RecalcGemList (thunk) | `0x8279d6a0` | `0x7924a0` | `80630000 4bff709c` |
| BeatMatcher::ResetGemStates | `0x82790570` | `0x785370` | `7d8802a6 9181fff8` |
| ResetGemStates callers | `0x826be558`, `0x82790dd4` | — | — |
| NewTrackWatcherImpl | `0x8279d990` | `0x792790` | `7d8802a6 4808b89d` |
| ControllerTypeToTrackWatcherType | `0x8279d808` | `0x792608` | `7d8802a6 4808ba41` |
| TrackWatcher::SetImpl | `0x8279df28` | `0x792d28` | — (bl NewTWImpl @ `0x8279e038`) |
| SetImpl callers' func (TW ctor/ReplaceImpl) | `0x82791490` | `0x786290` | — |
| TrackWatcherImpl base ctor | `0x82797298` | `0x78c098` | `7d8802a6 48091fa1` |
| base-ctor `stw r8,0x1c(r30)` | `0x8279730c` | `0x78c10c` | `911e001c` |
| Guitar TWImpl ctor | `0x827a0fc8` | `0x795dc8` | `7d8802a6 9181fff8` |
| intermediate ctors (call base) | `0x8277fbe0`, `0x827a0668`, `0x827a14b0` | — | — |
| SongData::GetGemList | `0x82770730` | `0x765530` | `81630050 548a103a` |
| GameGemDB::GetDiffGemList | `0x827931c8` | `0x787fc8` | `81630000 548a103a` |
| GameGemDB::Duplicate | `0x827932c8` | `0x7880c8` | `7d8802a6 48095f89` |
| GameGemList::CopyFrom | `0x8278e168` | `0x782f68` | `7d8802a6 9181fff8` |
| operator new | `0x827bd2f0` | — | — |
| __savegprlr_27 | `0x82829254` | `0x81e054` | — |

TrackWatcherImpl field offsets: vtable `+0x0`, mUserGuid `+0x4..`, mSongData
`+0x50` (note: RecalcGemList impl reads mSongData@+0x50 and mTrack@+0x68;
ctor stores slot@+0x18, gemlist@+0x1c, parent@+0x20). BeatMatcher: mWatcher
`+0x44`.
