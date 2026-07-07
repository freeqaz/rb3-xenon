# Plan: RB3Enhanced-style "same-instrument" runtime patch (multiple players on one instrument)

**Status (2026-07-07):** *Design + implementation guide, not yet built.* This is the
actionable engineering doc for a bootable Xbox 360 runtime mod that lets **two or more
players play the same instrument** (e.g. two lead guitars, four drummers) in retail
Rock Band 3 (title **45410914**, **TU5**). It is built as a fork of **RB3Enhanced**
(XEX-DLL at base `0x84000000`, injected pre-`main` by RB3ELoader / the Xenia patch),
adding a handful of new hooks whose addresses come from the **rb3-xenon** decomp
(address oracle) with the **rb3-Wii** decomp as the behavioral oracle.

**One-paragraph verdict.** The feature is *feasible and small*. All per-player hit
state (score, streak, sustains, rolls) is **already per-player**; the only
cross-player shared-mutable state is the `GameGem::mPlayed` bit on the one
`GameGemList` that each track's watchers borrow. Retail enforces "one player per
instrument" in three independent layers (UI grey-out, arbitration, and an
assignment gate that `MILO_FAIL`s on the 2nd claimant). The mod therefore needs:
(1) disable the three enforcement layers so a 2nd same-type player can be *assigned*,
and (2) give each player's `TrackWatcher` a **private clone** of the gem list (via the
already-present `GameGemList::CopyFrom` / `GameGemDB::Duplicate`) so hits don't steal
notes. **The one subtlety that makes-or-breaks it:** `mGemList` is re-borrowed from the
shared list on every `ResetGemStates` (via `TrackWatcherImpl::RecalcGemList`), so the
clone must be (re)installed at that choke-point — not once at watcher construction, which
gets silently reverted (§3.2, §6.4). Everything else already generalises. Upstream tracks this as
**[RB3Enhanced issue #11](https://github.com/RBEnhanced/RB3Enhanced/issues/11)**
("Support arbitrary instrument combos (4 drummers, etc.)") — the UI-side checklist
items are done there, the shared-gem-list blocker is exactly what this doc solves.

> **Repos referenced (all read-only oracles except this doc):**
> - `/tmp/RB3Enhanced` — RB3Enhanced source (fork base). Build system, hook primitives, port tables.
> - `/home/free/code/milohax/rb3` — RB3 **Wii** decomp (named C++ source = behavioral oracle).
> - `/home/free/code/milohax/rb3-xenon` — RB3 **Xbox 360** retail decomp (address oracle): `tools/fn_resolver.py`, `decomp.db`, Ghidra MCP on port 8002, `orig/45410914/default.xex` (base `0x82000000`).

---

## 1. Goal & status

**Build:** a fork of `RB3Enhanced.dll` for Xbox 360 TU5 that, gated behind a new
`rb3.ini` flag (`AllowSameInstrument=true`), permits N players to join the **same**
instrument and play it **independently** (independent hits, streaks, scores; no
note-stealing; clean song end; no leak across songs). Local play only in v1; online
session-mask handling is deferred to Phase 3.

**Why a runtime patch and not the port.** RB3Enhanced already boots on both Xenia and
RGH/JTAG hardware, injects before `main`, and has a mature hook toolkit. We reuse it
wholesale and add ~5 new hooks. No engine recompilation is required — every game
function we touch is called at its fixed retail address.

**Feature checklist (from issue #11, annotated with our scope):**
- ✅ (upstream) Change track-assignment logic so the game won't crash on duplicate instrument — **Layer C**, this doc §6.3.
- ✅ (upstream) Part-select screen no longer greys out taken instruments — **Layer A**, §6.1.
- ✅ (upstream) Genericize overshell slots — already done in `OvershellHooks.c` (`BuildInstrumentSelectionList`).
- ❌ Remove the "instrument slot open in the gathering" check before joining a session — **Phase 3 / online**, §7.
- ❌ (the real blocker, not in issue #11's list) **Per-player gem-list clone** so hits are independent — **the centerpiece**, §6.4.

---

## 2. Background

### 2.1 RB3Enhanced mechanics

**Loading.** RB3Enhanced is compiled as an **XEX-DLL based at `0x84000000`**
(`xex.xml`):

```xml
<xex>
    <format><unencrypted/><compressed/></format>
    <mediatypes><allpackages/></mediatypes>
    <gameregion><all/></gameregion>
    <baseaddr addr = "0x84000000"/>
</xex>
```

On 360 the DLL's `DllMain` runs pre-`main` (loaded by RB3ELoader or the Xenia patch)
and installs a call-site detour on the `App` constructor call inside `main` so RB3E's
`StartupHook` runs before the game constructs `App` (`source/xbox360.c`):

```c
BOOL APIENTRY DllMain(HANDLE hInstDLL, DWORD reason, LPVOID lpReserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        POKE_32(PORT_MAINSEH, (DWORD)RB3E_ExceptionHandler);
        POKE_BL(PORT_APP_RUN, PORT_APP_RUNNODEBUG);   // App::Run -> App::RunWithoutDebugging
        POKE_BL(PORT_APP_CALL, &CTHook);              // detour the App::_ct call-site in main()
        EnableSockpatch();
    }
    return TRUE;
}
```

`CTHook` initialises the crypto/input/content hooks then calls `StartupHook`, which is
the master init sequence (`source/rb3enhanced.c`):

```c
void StartupHook(void *ThisApp, int argc, char **argv)
{
    RB3E_MSG("Loaded! Version " RB3E_BUILDTAG " (" RB3E_BUILDCOMMIT ")", NULL);
    InitialiseFunctions();   // POKE_B every RB3E_STUB to its real game address
    ApplyPatches();          // unconditional POKE_32/POKE_B patches
    ApplyHooks();            // HookFunction() detours + POKE_B/POKE_BL hooks
    InitDefaultConfig();
    if (HasLauncherConfig()) LoadConfig();
    RB3E_MountFileSystems();
    LoadConfig();            // parse rb3.ini
    ApplyConfigurablePatches();  // patches gated on config flags
    RB3E_MSG("Starting Rock Band 3...", NULL);
    AppConstructor(ThisApp, argc, argv);   // hand control back to the game
}
```

**Hooking primitives** (`include/ppcasm.h`) are hand-assembled PowerPC:

```c
#define POKE_32(addr, val)  do { *(uint32_t *)(addr) = (uint32_t)(val); } while (0)
#define B(dest, src)  (0x48000000 + (((uint32_t)(dest) - (uint32_t)(src)) & 0x3ffffff))
#define BL(dest, src) B(dest, src) + 1
#define POKE_B(addr, dest)  POKE_32(addr, B(dest, addr))    // overwrite with unconditional branch
#define POKE_BL(addr, dest) POKE_32(addr, BL(dest, addr))   // overwrite with branch-and-link
#define LI(dest, val)  ADDI(dest, 0, val)                   // li rD, val
#define NOP  ORI(0, 0, 0)
#define BLR  0x4e800020
```

- **`POKE_32`** — overwrite one instruction (e.g. NOP out a check, `li r3,1` to force a return).
- **`POKE_B` / `POKE_BL`** — redirect a branch / call-site to our C function.
- **`HookFunction`** (`source/utilities.c`) — a first-instruction-relocating detour. It
  copies the target's first instruction into an 8-byte stub, appends a branch back to
  instruction #2, then overwrites the target's first instruction with a branch to our
  hook. Our hook can still call the original by calling the stub:

```c
void HookFunction(unsigned int OriginalAddress, void *StubFunction, void *NewFunction)
{
    unsigned int *orig = (unsigned int *)OriginalAddress;
    unsigned int *stub = (unsigned int *)StubFunction;
    stub[0] = orig[0];                          // copy original 1st instruction
    stub[1] = B(&orig[1], &stub[1]);            // branch back to orig+4
    orig[0] = B((unsigned int)NewFunction, OriginalAddress);  // detour orig -> hook
}
```

**Calling game functions from C — the `RB3E_STUB` pattern.** RB3E can't link against
the game, so it declares tiny naked "stub" placeholders (`source/_functions.c`) and
pokes each one with an unconditional branch to the real game address at startup:

```c
// _functions.c: naked placeholder, uses __LINE__ so MSVC can't fold/optimise it away
#define RB3E_STUB(x) __declspec(naked) void x() { __asm { li r3, __LINE__ } }
RB3E_STUB(AddGameGem)
RB3E_STUB(WillBeNoStrum)
// ... one per game function RB3E calls
```

```c
// rb3enhanced.c InitialiseFunctions(): wire each stub to its retail address
POKE_B(&ModifierActive, PORT_MODIFIERMGR_ACTIVE);
POKE_B(&RandomInt,      PORT_RANDOMINT);
// ...
```

After that, C code can `#include` a prototype and just call `AddGameGem(...)` — it
branches into the game. Addresses live in per-version tables `include/ports_*.h`; the
360 table is `include/ports_xbox360.h` (guarded by `#ifdef RB3E_XBOX`, "for 360 TU5").

**C++ objects are modeled as C structs** with manual `unkN` padding (MSVC 360 ABI).
Example (`include/OvershellHooks.h`, `include/rb3/BandUser.h`):

```c
typedef struct _OvershellSlot {
    char unk1[0x2c];
    ControllerType controllerType;
    int list_vector_maybe;
    int unk_var_2;
    char unk2[0x4];
    BandUser *bandUser;
} OvershellSlot;

typedef enum _TrackType { DRUMS=0, GUITAR=1, BASS=2, VOCALS=3, KEYS=4,
                          PRO_KEYS=5, PRO_GUITAR=6, HARMONIES=7, PRO_BASS=8 } TrackType;
```

**The C-reimplementation pattern.** When MSVC *inlined* a function (so there's no
call-site to detour), RB3E reimplements it in C and pokes a `B` over the original
body. `OvershellHooks.c` does exactly this for `BuildInstrumentSelectionList`
("Monkey patching the function will be hard, so make a realistic, dark and gritty
reboot.") — and note it's **already genericized** so a mic controller can pick
guitar/bass/keys/drums. This is the template for our Layer-A reimplementation.

**DTA extension point.** `POKE_B(PORT_DATAINITFUNCS_TAIL, &AddDTAFunctions)` registers
new DTA script functions (`source/DTAFunctions.c`), how RB3E exposes `rb3e_*` script
hooks. We can add an `rb3_enhanced_same_instrument` DTA getter if the UI needs it.

**Config.** `rb3.ini` is parsed by inih into a `RB3E_Config config` struct
(`include/config.h`, `source/config.c`). Adding a flag = one struct field + one
`strcmp` in `INIHandler` + a line in the shipped `assets/xbox_default_rb3.ini`.

### 2.2 Why rb3-xenon makes the new hooks possible

RB3E's `ports_xbox360.h` only contains the ~150 addresses upstream happened to need.
Our feature needs several **not** in that table (`ProcessConfig`, `NewTrackWatcherImpl`,
`GameGemList` ctor/`CopyFrom`, `OvershellPanel::ResolvePartWaitStates`, …). The
**rb3-xenon** decomp is our address oracle for deriving them against **the exact same
binary** (TU5 confirmed — see §2.3):

- **`decomp.db`** (sqlite, ~12k named functions) — `functions` table maps mangled
  symbol → unit, size, match%. Query by demangled name.
- **`tools/fn_resolver.py resolve <addr>`** — aggregates 7 identity tiers for any
  `fn_82XXXXXX` address (decomp.db named, target_symbol_map, DC3/game byte-match,
  fuzzy pairs, vtable/RTTI, cross-arch Wii BinDiff, string-fingerprint).
- **Ghidra MCP on port 8002** — the retail XEX loaded with symbols; decompile/xref any
  address for hand-derivation of unpinned callees.
- **`orig/45410914/default.xex`** — base `0x82000000`; byte-verify a derived address's
  prologue.

### 2.3 TU5 equivalence (confirmed)

rb3-xenon **is** RB3 TU5 (title 45410914). Evidence: RB3E's own TU5 port addresses land
in the live `.text` of our binary. `PORT_ADDGAMEGEM = 0x8278E530` and
`PORT_WILLBENOSTRUM = 0x8278CBB0` both fall inside real functions in `splits.txt`.
Independently, several class methods this feature needs are already **pinned + verified**
in rb3-xenon (§6 table). Spot-check performed while writing this doc:

```
$ python3 tools/fn_resolver.py resolve 0x8264BCE4
  ★ [decomp_db_named] conf=0.99  OvershellPartSelectProvider::Reload(ControllerType, BandUser*)
      match_pct: 100.0   size: 1224   unit: default/OvershellPartSelectProvider
```

> Because RB3E's port table and rb3-xenon's `decomp.db` are **independent** derivations
> of the same binary, agreement between them is a strong cross-check. `0x8278E530`
> resolves to "no identity" in `decomp.db` (that function isn't a *named* decomp symbol
> yet) but RB3E already ships it as `AddGameGem` — a reminder to trust RB3E's table for
> addresses it already has, and use decomp.db/Ghidra only for the ones it lacks.

---

## 3. Verified findings

All refs below are **rb3-Wii decomp** paths (`/home/free/code/milohax/rb3/...`), which
has named source and is the behavioral oracle. The 360 build is the same engine; class
layouts differ only in ABI details (see §8 for translating an oracle line to a 360
address).

### 3.1 Uniqueness enforcement — three independent layers

**Layer A — UI grey-out.** `OvershellPartSelectProvider::IsActive(int)`
(`src/band3/meta_band/OvershellPartSelectProvider.cpp:86-143`). For each *other* slot
that already has a resolved user, it greys out any instrument in the same equivalence
class (verified against the source this session):

```cpp
for (int i = 0; i < (int)mOvershell->mSlots.size(); i++) {
    OvershellSlot *curslot = mOvershell->GetSlot(i);
    BandUser *curuser = curslot->GetUser();
    if (curuser && curuser != mUser) {
        OvershellSlotState *curstate = curslot->GetState();
        if (!curstate->IsPartUnresolved()) {
            if (RepresentSamePart(entry.unk4, curuser->GetTrackType()))
                return false;   // <-- greys the entry out
        }
    }
}
```

`RepresentSamePart` (`src/band3/game/Defines.cpp:231`) uses equivalence classes
`{Guitar,RealGuitar} {Bass,RealBass} {Keys,RealKeys} {Vocals} {Drum}` from
`GetTracksRepresentativeOfPart` (`Defines.cpp:202`).

**Layer B — arbitration.** `OvershellPanel::ResolvePartWaitStates()`
(`src/band3/meta_band/OvershellPanel.cpp:906-1026`; **derived retail address
`0x8259D948`**, see §6.2). For each local user in `kState_ChoosePartWait` it computes
a `needsResolve` flag (set when the user's part matches an *already-resolved* user's,
`:925-929`, or a higher-priority waiter's, `:939-947`), then does one of two things —
**both** of which enforce uniqueness:
  - **`needsResolve` path (`:953-979`):** builds the controller's `playableTracks`,
    then the erase loop (`:960-969`) strips every track that `RepresentSamePart`s an
    already-resolved user; if exactly one survives it is force-assigned, else the user
    is bounced back to `kState_ChoosePart` (`:976-977`).
  - **`allWaiting` path (`:980-1022`):** runs `mPartResolver` over the contending
    waiters and forces **one** winner to `kState_ChoosePartWarn`, i.e. turn-taking.

The function's *other* job — advancing an **uncontested** waiter out of
`ChoosePartWait` — is load-bearing, so it must **not** be no-oped wholesale (§6.2).

**Layer C — assignment gate (the crash).** One chart `TrackInfo` per instrument
(`SongData::FixUpTrackConfig`, `src/system/beatmatch/SongData.cpp:267`).
`PlayerTrackConfigList::ProcessConfig` (`src/system/beatmatch/PlayerTrackConfigList.cpp:227-243`)
assigns the first free slot of the requested type; the **second** same-type user gets
`num == -1` and `MILO_FAIL`s (verified verbatim this session):

```cpp
void PlayerTrackConfigList::ProcessConfig(PlayerTrackConfig &cfg) {
    TrackType ty = cfg.mTrackType;
    if (ty != kTrackNone) {
        int diff = cfg.mDifficulty;
        int num = TrackNumOfType(ty);
        if (num != -1) {
            cfg.mTrackNum = num;
            mTrackDiffs[num] = diff;
            mTrackOccupied[num] = 1;
        } else {
            MILO_FAIL("Couldn't create track of type %s. Either songs.dta claims it "
                      "exists but it doesn't, or you're trying to play head-to-head, "
                      "which is obsolete.", TrackTypeToSym(ty));
        }
    }
}
```

`TrackNumOfType` → `TrackNumOfExactType` (`:194-200`) returns the first `i` with
`mTrackTypes[i]==ty && mTrackOccupied[i]==0`; occupancy is what blocks the 2nd claimant.

**Assignment flow (join → gameplay):**
`OvershellPartSelectProvider::Reload` (`:23-82`) →
`OvershellSlot::SelectPartImpl` (`OvershellSlot.cpp:377`) →
`BandUser::SetTrackType` →
`GameConfig::AssignTracks`/`AssignTrack` (`src/band3/game/GameConfig.cpp:111-206`) →
`PlayerTrackConfig::mTrackNum` →
`TrackPanel::CreateTracks` (`src/band3/bandtrack/TrackPanel.cpp:155`, per-USER `NewTrack`) →
`Band::NewPlayer` (`src/band3/game/Band.cpp:472-486`) → one `GemPlayer` per user.

**Capacity headroom** (nothing needs raising for 2–4 players):
`kMaxPlayers = 4` (`src/band3/game/GameConfig.h:14`), `TrackPanel kTrackNumSlots = 5`,
`PlayerTrackConfigList kMorePlayersThanWeWillEverNeed = 8`. `overshell.dta` slots 0 AND 3
already both accept `guitar real_guitar keys`.

**Vocals do NOT generalise.** Harmonies use 1 `BandUser`/`VocalPlayer` + 3 `Singer` mic
objects via `GameMicManager`; `NumSingers()` is hardcoded 3
(`PlayerTrackConfigList.cpp:153`). Keep same-instrument to non-vocal tracks in v1.

### 3.2 The decisive finding — `mPlayed` is the only shared mutable state

Every per-player hit accumulator is **already per-player**:
`GemPlayer` (own `GemStatus`, `src/band3/game/GemPlayer.h:436`) →
`BeatMatcher` (one per player, `mWatcher`, `BeatMatcher.h:119`) →
`TrackWatcherImpl` (`mLastGemHit`/`mGemsInProgress`/sustains/rolls,
`TrackWatcherImpl.h:170-213`).

The **only** cross-player shared-mutable state is the `GameGem::mPlayed` bit (plus a
sustain helper `unk10b1`) on the **shared per-track `GameGemList`**. Each watcher
*borrows* that list:

- `NewTrackWatcherImpl` reads a **local** `GameGemList *gemList = data->GetGemList(track)`
  (`TrackWatcher.cpp:36`) and passes it to the impl constructor, which stores it in
  `TrackWatcherImpl::mGemList` (ctor init-list, `TrackWatcherImpl.cpp:28`).
- **Correction to the earlier draft:** `SongData::GetGemList(track)` does **not**
  return "the one `GameGemDB` per track." It returns a **single per-difficulty
  `GameGemList*`**: `GetGemListByDiff(track, mTrackDifficulties[track])` →
  `mGemDBs[track]->GetDiffGemList(mTrackDifficulties[track])` (`SongData.cpp:1195-1201`).
  The per-track *container* is `mGemDBs[track]`, a `GameGemDB*` holding one
  `GameGemList` **per difficulty** (`SongData.h`; `GameGemDB.h`). This matters for the
  clone: the watcher borrows only the *current-difficulty* list, so per-player
  difficulty (Phase 2) needs the whole `GameGemDB` cloned, not one list (§6.4, §7).
- **Critical (missed in the earlier draft): the borrow is re-established on every
  reset.** `TrackWatcherImpl::RecalcGemList()` re-assigns
  `mGemList = mSongData->GetGemList(mTrack)` (`TrackWatcherImpl.cpp:66-68`), and
  `BeatMatcher::ResetGemStates` calls it (`BeatMatcher.cpp:144-147`). So a clone
  written into `mGemList` *once* (at watcher construction) is **silently reverted to
  the shared list** by the first `ResetGemStates` — which fires at song start,
  restart, section-jump, and difficulty change, *before* any gameplay read. The clone
  must therefore be (re)installed at the `RecalcGemList` choke-point, not once at
  construction (§6.4).

**Writers of the shared bit:**
- `TrackWatcherImpl::HitGem` → `gem.SetPlayed(true)` (`TrackWatcherImpl.cpp:314-316`)
- `SetGemsPlayedUntil` (`:303`)
- `SetUnk10B1` (`:624`, `:705`) + `JoypadTrackWatcherImpl.cpp:64`
- `GameGemList::Reset` (`src/system/beatmatch/GameGemList.cpp:275-280`, clears
  `mPlayed`+`unk10b1` on every gem) reached via
  `TrackWatcherImpl::Jump → SetAllGemsUnplayed → mGemList->Reset()`
  (`TrackWatcherImpl.cpp:148-149`, `:307`); `Jump` is itself called by
  `BeatMatcher::ResetGemStates` (`BeatMatcher.cpp:146-147`) and on section jumps
  (`GemTrainerPanel`). **This is a latent cross-player bug on its own:** because the
  list is shared, *any* player's section-jump/reset wipes *every* co-track player's
  played bits. The per-player clone (§3.3) fixes this too — provided the clone
  actually sticks past `RecalcGemList` (see the re-borrow note above).

**Readers that cause note-stealing:** `ClosestUnplayedGem` (`:284-290`) skips gems
whose `mPlayed` bit is set → the first player to hit a note marks it played, the second
player sees it "gone" and over-strums. This is exactly the blocker the issue-#11 author
described.

Everything else (all watcher subclasses, `GemManager` rendering, `NowBar`) is
**const/read-only** on the gem list. `GameGem::unk18` ("`mPlayers`?" mask,
`GemPlayableBy`, `BeatMatchUtl.cpp:12-17`) is *who-MAY-play* (old head-to-head infra),
**not** *who-HAS-played*; there is no per-player played mask inside the struct.

### 3.3 The fix — per-watcher private gem-list clone

Give each player's `TrackWatcher` its **own** `GameGemList` clone so `mPlayed`
mutations are private. The engine already has the clone helpers (verified verbatim this
session, `src/system/beatmatch/GameGemList.cpp:106-110`):

```cpp
void GameGemList::CopyFrom(const GameGemList *gList) {
    mGems.clear();
    mGems.reserve(gList->mGems.size());
    mGems.insert(mGems.begin(), gList->mGems.begin(), gList->mGems.end());
}
```

```cpp
// src/system/beatmatch/GameGemDB.cpp:57-63
GameGemDB *GameGemDB::Duplicate() const {
    GameGemDB *duped = new GameGemDB(mGameGemLists.size(), mHopoThreshold);
    for (int i = 0; i < mGameGemLists.size(); i++)
        duped->mGameGemLists[i]->CopyFrom(mGameGemLists[i]);
    return duped;
}
```

**Lifetime:** the `TrackWatcher` destructor does **not** free `mGemList` (it's
borrowed), so the mod owns the clone's lifetime and must free clones on song
teardown (`Game::__dt` / `TrackPanel` teardown). See §6.4 for the refcount-map design.

### 3.4 Downstream collisions with a duplicated `mTrackNum` (Task-2 audit)

Two same-type players share the same `mTrackNum` (both point at the one chart track).
Consequences, and whether v1 tolerates them:

| Site | File:line (Wii) | Effect of duplicate `mTrackNum` | v1 disposition |
|---|---|---|---|
| `Game::GetPlayerFromTrack` | `src/band3/game/Game.cpp:646-658` | returns **first** matching player | Tolerated; affects unison credit + a couple lookups |
| CommonPhraseCapturer (unison) | `CommonPhraseCapturer.cpp:171,182` | unison credit attributed to first player only | Phase 2 polish |
| SongDB lookups | `SongDB.cpp:73,299` | first-match | Tolerated |
| `Game::GetScoringTracks` | (bitmask) | collapses duplicates in a track bitmask | Cosmetic only |
| `MasterAudio::GetExtraTrackInfo` | `MasterAudio.cpp:238` — `InstrumentPlayer(ty,0)` | one audio stem per instrument | **Inherent** (there is only one stem); acceptable |
| `mTrackDifficulties[track]` | keyed by track | duplicate players **share** difficulty | v1: last-writer-wins (`ProcessConfig` `mTrackDiffs`); Phase 2 = per-player diff clone |
| `GameConfig::AssignTrack` mode flags | `GameConfig.cpp` (`SetUseRealDrums`/`SetUseVocalHarmony`/`unk2c`) | last-writer-wins | Phase 2 = OR / policy |

**Per-player scores survive** regardless — they live on `GemPlayer`/`GemStatus`, never
on the track. So v1 (two guitars, independent hits and scores) is correct even with the
shared `mTrackNum`; the collisions above are cosmetic or Phase-2 polish.

---

## 4. Build & boot pipeline

### 4.1 Fork & clone

```bash
# Fork github.com/RBEnhanced/RB3Enhanced, then:
git clone https://github.com/<you>/RB3Enhanced
cd RB3Enhanced
git checkout -b same-instrument
```

The local `/tmp/RB3Enhanced` is already an up-to-date clone to read from.

### 4.2 Toolchain (Xbox 360) — verified from `Makefile` / `BUILDING.md`

Required for the 360 target:
- **Xbox 360 SDK (XEDK)** with build tools installed. `XEDK` env var must point at the
  install dir. **Where to get XEDK is out of scope / user-supplied** (it is Microsoft's
  proprietary SDK). The Makefile uses `$(XEDK)/bin/win32/{cl.exe,link.exe,imagexex.exe}`
  and `$(XEDK)/include/xbox`, `$(XEDK)/lib/xbox`.
- **Make** (on Linux/macOS just use system make).
- **Wine** for running the MSVC toolchain off-Windows. Set `WINDOWS_SHIM` to the wine
  binary; `XEDK` must be a wine-accessible path, e.g. `Z:\Users\you\xedk\`.
  (`BUILDING.md`: "some Wine versions may not work correctly with MSVC.")

The Makefile wraps each 360 tool with `$(WINDOWS_SHIM)`:
```make
COMPILER_X := "$(TOOLPATH_X)/cl.exe"      # TOOLPATH_X = $(XEDK)/bin/win32
LINKER_X   := "$(TOOLPATH_X)/link.exe"
IMAGEXEX_X := "$(TOOLPATH_X)/imagexex.exe"
$(BUILD_X)/%.obj: $(SRC_DIR)/%.c | scripts
	@INCLUDE=$(INCLUDES_X) $(WINDOWS_SHIM) $(COMPILER_X) $(CFLAGS_X) -Fo"$@" -TC $<
```
360 compile flags (note **`-OPT:ICF`** at link — relevant to §8's ICF check):
```make
CFLAGS_X := -c -Zi -nologo -W3 -WX- -Ox -Os -D _XBOX -D RB3E_XBOX ... -GR- -openmp- ...
LFLAGS_X := ... -OPT:REF -OPT:ICF -TLBID:1 -RELEASE -dll -entry:"_DllMainCRTStartup" -XEX:NO
XEXFLAGS := -nologo -config:"xex.xml"
```

### 4.3 `make xbox` flow & artifacts

```bash
export XEDK="Z:\\Users\\you\\xedk\\"           # wine-visible path
export WINDOWS_SHIM="/usr/bin/wine"
make xbox            # or: make -jN xbox
# DEBUG=1 make xbox   -> extra RB3E_DEBUG logging (recommended for bring-up)
# EMULATOR=1 make xbox -> links xbdm.lib (for Xenia debug-monitor logging)
```

Artifact chain (`Makefile`):
1. `cl.exe` compiles `source/*.c` → `build_xbox/*.obj`
2. `link.exe` links → `build_xbox/RB3Enhanced.exe` (a 360 PE DLL)
3. `imagexex.exe -config:xex.xml` wraps it → **`out/RB3Enhanced.dll`** (an XEX-DLL based at `0x84000000`)

`scripts/version.sh` (auto-run) writes `source/version.h` with
`RB3E_BUILDTAG`/`RB3E_BUILDCOMMIT` from git → surfaced in the boot log line
`"Loaded! Version <tag> (<commit>)"` (see smoke test §4.6).

### 4.4 Boot path A — Xenia (emulator, fastest iteration)

The `out/RB3Enhanced.dll` is loaded by **the Xenia Canary game-patch** (`BUILDING.md`:
"loaded with the RB3ELoader Dashlaunch plugin, or the Xenia patch").

- **RESOLVED (mechanism, verified from the patch file this session).** Xenia Canary
  reads per-title `.patch.toml` files from its `patches/` directory. The RB3E patch is
  **`45410914 - Rock Band 3.patch.toml`** (InvoxiPlayGames'
  [gist](https://gist.github.com/InvoxiPlayGames/b7ee7483876efe606f8ed4a92b7c780f)). It
  accepts two module hashes — `464451C1022FFF32` ("EA disc default.xex + TU5 applied")
  and `02B607A811A4C291` ("RB3DX modded pre-patched TU5 XEX"); the first is an
  **independent confirmation that our target is the TU5 XEX**. Its single `[[patch]]`
  ("Load RB3Enhanced.dll") "replaces a check in `_start` with a branch to LoadLibrary":
  it pokes three instructions at `0x8283CD64/68/6C` (`lis r3, 0x82C64350@hi` /
  `ori r3, r3, @lo` / `bl LoadLibrary`) and plants the string `"RB3Enhanced.dll"` at
  `0x82C64350`. So the **game's own `LoadLibrary` import** loads the DLL at startup;
  its `DllMain` (§2.1) then runs the pre-`main` bootstrap. **This is why our feature
  needs no Xenia-specific work** — once the DLL is in, the hook path is identical to
  hardware.
- **Requirement:** the patch header notes it needs a Xenia Canary build with
  **`writable_code_segments`** enabled (RB3E POKEs game `.text` at runtime; canary
  PR #100, "Not merged as of 18-Dec-2022" — verify your canary build supports it) and
  `apply_patches = true` in `xenia-canary.config.toml`. Place the `.patch.toml` in
  `patches/` and `RB3Enhanced.dll` + `rb3.ini` where the loader expects them (alongside
  the mounted game). The patch also recommends "Commit 28f3eb6 or later of RB3Enhanced"
  (removes the DEMO check).
- `RB3E_IsEmulator()` (`source/xbox360.c`) auto-detects Xenia (XAM export address / `sc`
  first-instruction) and RB3E already applies two emulator-only patches
  (`OutfitConfig::CompressTextures` → `BLR`, `SongMgr::IsDemo` check → NOP). Our feature
  is emulator-agnostic; no special-casing expected.
- **Iteration loop:** rebuild `out/RB3Enhanced.dll`, drop into Xenia's expected path,
  relaunch RB3. `DEBUG=1` + `EMULATOR=1` routes `RB3E_MSG`/`RB3E_DEBUG` to the Xenia
  log; watch for the version line then our new `RB3E_MSG` markers.

### 4.5 Boot path B — real hardware (RGH/JTAG via DashLaunch + RB3ELoader)

On console the DLL is loaded by the **RB3ELoader** DashLaunch plugin:
- Requires an **RGH'd/JTAG** 360 (not stock), an RB3 disc dump (GoD-format rips do NOT
  work), Aurora dashboard, and **DashLaunch** (configures `launch.ini`, installs plugins).
- Install `RB3ELoader.xex` as a DashLaunch **plugin** and place `RB3Enhanced.dll` +
  `rb3.ini` where the loader expects them (alongside the game / in the RB3E folder).
  (Upgrade note from RB3E docs: replace `RB3Enhanced.xex` with `RB3ELoader.xex` in
  DashLaunch.) See the community install guide
  ([gist](https://gist.github.com/leftyfl1p/16eccae6c3ec4ac84a87578de1fac16e)) and the
  [official download page](https://rb3e.rbenhanced.rocks/download.html).
- **RESOLVED (partial).** `RB3ELoader.xex` **is** the DashLaunch plugin: the official
  upgrade guidance is "replace both `RB3Enhanced.dll` and `RB3ELoader.xex`" (from
  RB3Enhanced 0.5.1). DashLaunch loads plugins from `launch.ini` `pluginN=` slots
  (`plugin1=`, `plugin2=`, …), each pointing at a loadable `.xex` on the console FS —
  so `RB3ELoader.xex` goes in a free `pluginN=` slot, and it in turn loads the
  companion `RB3Enhanced.dll` + `rb3.ini`. **Still open:** the exact console paths the
  current loader release expects for the DLL/ini (the loader ships *inside* the RB3E
  release zip, not a separately-browsable repo — `github.com/RBEnhanced/RB3ELoader`
  404s). Pull the paths from the release zip's README before a hardware run; this does
  not block Xenia bring-up (§4.4), which is the recommended iteration path.

### 4.6 Smoke test — vanilla RB3E boots BEFORE any of our changes

Build and boot **unmodified** RB3E first and confirm it initialises. Acceptance = the
startup log shows, in order (strings from `rb3enhanced.c`):

```
DLL has been loaded
Loaded! Version <tag> (<commit>)
Functions initialized!
Patches applied!
Hooks applied!
Starting Rock Band 3...
```

If those don't appear (Xenia log / xbdm), fix the loader/toolchain before touching the
feature. This isolates "my patch broke it" from "the harness was never working."

---

## 5. Phase 0 — Xenia spikes (de-risk before writing the real hooks)

Four cheap pokes, each with a precise expected observable. Do them in Xenia with
`DEBUG=1 EMULATOR=1`. These validate the §3 model empirically before committing to the
full implementation.

| # | Spike | How | Expected observable |
|---|---|---|---|
| 0.1 | **Reproduce the crash** | Vanilla RB3E; get two controllers to both select Guitar (may require Layer-A poke 0.2 first to make the 2nd selectable) | `MILO_FAIL("Couldn't create track of type guitar … head-to-head, which is obsolete.")` at song start — confirms Layer C is the gate |
| 0.2 | **Layer A off** | `POKE_32` the same-part `return false` branch in `IsActive` (the `RepresentSamePart(...) return false` at the tail of the loop) to fall through | The 2nd player's already-taken instrument is **no longer greyed out** / is selectable in the part-select screen |
| 0.3 | **Occupancy guard off** | Force `ProcessConfig` to accept the 2nd claimant (see §6.3 — e.g. hook so the 2nd same-type user reuses the same `mTrackNum` instead of `-1`) | No `MILO_FAIL`; song **starts** with two guitarists — **but expect note-stealing** (first to hit marks `mPlayed`, 2nd over-strums). This *positive* bug proves §3.2 |
| 0.4 | **Gem-clone spike** | Additionally give the 2nd watcher a private gem-list clone **at the `RecalcGemList` choke-point** (§6.4) — installing at watcher construction will be reverted by the first `ResetGemStates`, so verify the clone actually sticks | Both players hit the **same** notes **independently**; no stealing; two independent streaks/scores. Feature proven end-to-end. Also spot-check a **mid-song section restart** to confirm clone reset semantics (§6.4) |

Spike 0.3 producing note-stealing and 0.4 removing it is the go/no-go gate for the
whole plan.

---

## 6. Phase 1 — implementation

New files in the fork: `source/SameInstrumentHooks.c` (+ `include/SameInstrumentHooks.h`),
plus additions to `include/ports_xbox360.h`, `source/_functions.c`,
`source/rb3enhanced.c` (wire-up), `include/config.h`, `source/config.c`, and
`assets/xbox_default_rb3.ini`.

**Config flag wiring (do this first).**
```c
// include/config.h — add to RB3E_Config (inside/near [General])
char AllowSameInstrument;   // NEW: permit multiple players on one instrument
```
```c
// source/config.c INIHandler(), inside if (strcmp(section,"General")==0) { ... }
if (strcmp(name, "AllowSameInstrument") == 0)
    config.AllowSameInstrument = RB3E_CONFIG_BOOL(value);
```
```ini
# assets/xbox_default_rb3.ini, [General]
AllowSameInstrument=false
```
Every hook below early-returns to the original behaviour when
`config.AllowSameInstrument == 0`, so the mod is inert unless explicitly enabled.

**New port addresses** (`include/ports_xbox360.h`; values marked `?` need §8 derivation):
```c
// same-instrument feature (TU5).
// DERIVED this session (byte-verified prologue = `mflr r12` 0x7d8802a6, safe to hook):
#define PORT_OVERSHELL_ISACTIVE      0x8264B5F8  // OvershellPartSelectProvider::IsActive(int) const
                                                 //   Ghidra symbol ?IsActive@OvershellPartSelectProvider@@UBA_NH@Z;
                                                 //   cross-confirmed: calls GameMode::InMode x4 + RepresentSamePart x2.
#define PORT_OVERSHELLPANEL_RESOLVE  0x8259D948  // OvershellPanel::ResolvePartWaitStates()
                                                 //   fingerprint: calls RepresentSamePart(0x82671818) exactly 3x (Wii :926/:940/:964);
                                                 //   frame 0x130; in OvershellPanel .text neighborhood. HIGH confidence, not yet runtime-verified.
// STILL UNPINNED — units unsplit + retail strips the anchor MILO_FAIL strings, so the §8
// string-xref shortcut is UNAVAILABLE; derive structurally in Ghidra (recipe rewritten in §8):
#define PORT_PTCL_PROCESSCONFIG      0x????????  // PlayerTrackConfigList::ProcessConfig(PlayerTrackConfig&)
#define PORT_PTCL_TRACKNUMOFTYPE     0x????????  // PlayerTrackConfigList::TrackNumOfType(TrackType)
#define PORT_NEWTRACKWATCHERIMPL     0x????????  // NewTrackWatcherImpl(...) — NOT in the tiny TrackWatcher.cpp split block; find via the mGemList-assign fingerprint
#define PORT_TWI_RECALCGEMLIST       0x????????  // TrackWatcherImpl::RecalcGemList() — the mGemList re-assign choke-point (§6.4 hook target)
#define PORT_SONGDATA_GETGEMLIST     0x????????  // SongData::GetGemList(int track) -> per-DIFFICULTY GameGemList*
#define PORT_GAMEGEMDB_DUPLICATE     0x????????  // GameGemDB::Duplicate() const
#define PORT_GAMEGEMDB_GETDIFFLIST   0x????????  // GameGemDB::GetDiffGemList(int diff) -> GameGemList*
#define PORT_GAMEGEMDB_DTOR          0x????????  // GameGemDB::~GameGemDB() — symmetric free for a Duplicate() clone (§6.4)
#define PORT_GAMEGEMLIST_COPYFROM    0x????????  // GameGemList::CopyFrom(const GameGemList*)
// Bonus anchors derived this session (Defines.cpp, useful for fingerprinting neighbors):
//   RepresentSamePart = 0x82671818 ; GameMode::InMode = 0x82671A60
```
**New stubs** (`source/_functions.c`) for anything we *call* (not just detour):
```c
RB3E_STUB(SongDataGetGemDB)      // SongData::mGemDBs[track] accessor (or GetGemList; see §6.4)
RB3E_STUB(GameGemDBDuplicate)
RB3E_STUB(GameGemDBGetDiffList)  // GameGemDB::GetDiffGemList(diff)
RB3E_STUB(GameGemDBDtor)         // GameGemDB::~GameGemDB
RB3E_STUB(GameGemListCopyFrom)
RB3E_STUB(TrackNumOfType)
```
wired in `InitialiseFunctions()`:
```c
POKE_B(&SongDataGetGemDB,     PORT_SONGDATA_GETGEMLIST);   // or a dedicated GetGemDB port
POKE_B(&GameGemDBDuplicate,   PORT_GAMEGEMDB_DUPLICATE);
POKE_B(&GameGemDBGetDiffList, PORT_GAMEGEMDB_GETDIFFLIST);
POKE_B(&GameGemDBDtor,        PORT_GAMEGEMDB_DTOR);
POKE_B(&GameGemListCopyFrom,  PORT_GAMEGEMLIST_COPYFROM);
POKE_B(&TrackNumOfType,       PORT_PTCL_TRACKNUMOFTYPE);
```

### 6.1 Patch — Layer A (UI grey-out off)

- **Target:** `OvershellPartSelectProvider::IsActive(int)`
- **Wii oracle:** `OvershellPartSelectProvider.cpp:86-143` (the `RepresentSamePart` loop).
- **Retail address (DERIVED this session):** `PORT_OVERSHELL_ISACTIVE = 0x8264B5F8`.
  Ghidra has the symbol `?IsActive@OvershellPartSelectProvider@@UBA_NH@Z` at exactly the
  `OvershellPartSelectProvider.cpp` unit start (`splits.txt` `.text 0x8264B5F8–0x8264C238`,
  decomp.db 99.96%). Cross-confirmed structurally: the retail body calls
  `GameMode::InMode` (`0x82671A60`) **4×** — matching the four `TheGameMode->InMode(...)`
  calls at Wii `:115-118` — and `RepresentSamePart` (`0x82671818`) **2×** (Wii `:109`,
  `:139`). Prologue `mflr r12` → safe for `HookFunction`. (`Reload` sibling is at
  `0x8264BCE4`.)
- **Hook type:** `HookFunction` detour, or a narrow `POKE_32` over the same-part branch.
  Prefer the detour (safer, config-gated): call the original; if it returned `false`
  *only because of the same-part loop*, override to `true` when
  `config.AllowSameInstrument`.
- **Sketch:**
```c
// SameInstrumentHooks.c
char IsActiveStub[8];
int IsActiveHook(void *thisProvider, int data)  // returns bool in r3
{
    int orig = ((int(*)(void*,int))IsActiveStub)(thisProvider, data);
    if (!config.AllowSameInstrument)
        return orig;
    // When same-instrument is on, don't let the "another slot took this part"
    // rule grey out the entry. Simplest correct-enough v1: if the *user's own*
    // eligibility is fine, allow it. (A precise version re-walks the slots and
    // ignores only the RepresentSamePart rejection — see §6.1 note.)
    return 1;
}
```
> **Note.** Returning `1` unconditionally under the flag is the blunt v1. It is safe
> because Layers B/C still gate real assignment; the worst case is showing an entry that
> arbitration later resolves. A precise version reimplements `IsActive` in C (à la
> `BuildInstrumentSelectionList`) and drops **only** the `RepresentSamePart(...) return
> false` rejection, preserving the drums-pro / campaign / difficulty gates. Do the blunt
> version for Phase 1, refine in Phase 2 if UX demands.

### 6.2 Patch — Layer B (arbitration off)

- **Target:** `OvershellPanel::ResolvePartWaitStates()`
- **Wii oracle:** `OvershellPanel.cpp:906-1026` (full body read this session; see §3.1
  Layer B for the two enforcement paths).
- **Retail address (DERIVED this session):** `PORT_OVERSHELLPANEL_RESOLVE = 0x8259D948`
  (fingerprinted: calls `RepresentSamePart` (`0x82671818`) exactly 3×, matching Wii
  `:926/:940/:964`; frame 0x130; prologue `mflr r12`). HIGH confidence — byte-verify at
  runtime before shipping.
- **RESOLVED — do NOT no-op wholesale.** The earlier draft's "no-op the whole pass" is
  **wrong**: this function is also the *only* thing that advances an **uncontested**
  waiter out of `kState_ChoosePartWait`. Nulling it out strands every local player in
  `ChoosePartWait` forever (they never reach choose-difficulty). The two things to
  neutralize are precisely (a) the `needsResolve` erase/bounce and (b) the `mPartResolver`
  single-winner contention — but the state *advancement* must survive.
- **Precise minimal change.** Under the flag, since duplicates are now legal there is no
  contention to resolve, so treat **every** `ChoosePartWait` user as uncontested and push
  it straight to `kState_ChooseDiff` — exactly what the existing `HX_NATIVE` lone-user
  shortcut already does at `OvershellPanel.cpp:996-998` ("the part stands; the disc shows
  no warn when no one else wants it"). This keeps the part the user picked, removes both
  enforcement paths, and preserves the advance:
- **Hook type:** `HookFunction` detour, reimplementing the per-user loop.
- **Sketch:**
```c
char ResolveWaitStub[8];
void ResolveWaitStatesHook(void *thisPanel)
{
    if (!config.AllowSameInstrument) {
        ((void(*)(void*))ResolveWaitStub)(thisPanel);  // stock arbitration
        return;
    }
    // Same-instrument ON: no contention exists. Advance every local ChoosePartWait
    // user directly to ChooseDiff (mirrors the HX_NATIVE lone-user path). Requires
    // GetLocalParticipants + per-user GetOvershellState/SetOvershellSlotState +
    // UpdateAll — port those thin accessors (or reuse GetBandUsers/GetSlot already in
    // ports_xbox360.h). Field/vtable offsets from Ghidra on the 360 body.
    for (each local user U) {
        if (GetOvershellState(U) == kState_ChoosePartWait)
            SetOvershellSlotState(U, kState_ChooseDiff);
    }
    UpdateAll(thisPanel);
}
```
> **Why not just skip the erase loop?** Skipping only `:960-969` leaves `needsResolve`
> true with a possibly-emptied `playableTracks` → the `MILO_ASSERT(!playableTracks.empty())`
> at `:972` can fire, or the user is reset to `kState_ChoosePart`. And it does nothing
> about the `allWaiting`/`mPartResolver` path, which independently forces turn-taking on
> two simultaneous same-part waiters. Advancing to `ChooseDiff` sidesteps both.

### 6.3 Patch — Layer C (assignment gate: accept the 2nd claimant)

- **Target:** `PlayerTrackConfigList::ProcessConfig(PlayerTrackConfig&)`
- **Wii oracle:** `PlayerTrackConfigList.cpp:227-243` (the `MILO_FAIL` gate).
- **Retail address:** `PORT_PTCL_PROCESSCONFIG` — **unpinned**, derive via §8.
- **Hook type:** `HookFunction` detour that reimplements the accept path so the 2nd
  same-type user reuses the existing track's `mTrackNum` instead of hitting `-1`.
- **Sketch:**
```c
// C model of the relevant PlayerTrackConfig / list fields — offsets TO-VERIFY via Ghidra
typedef struct { int mTrackType; int mDifficulty; int mTrackNum; /*...*/ } PlayerTrackConfig;

char ProcessConfigStub[8];
void ProcessConfigHook(void *thisList, PlayerTrackConfig *cfg)
{
    if (!config.AllowSameInstrument) {
        ((void(*)(void*,PlayerTrackConfig*))ProcessConfigStub)(thisList, cfg);
        return;
    }
    int ty = cfg->mTrackType;
    if (ty == /*kTrackNone*/ -1 || ty == 0/*see enum*/) { /* handle none */ }
    int num = TrackNumOfType(thisList, ty);   // first FREE slot of this type, or -1
    if (num == -1) {
        // Second same-type claimant: reuse the first slot of this exact type,
        // WITHOUT re-marking occupancy (so a 3rd could also share).
        num = FirstSlotOfExactType(thisList, ty);   // scan mTrackTypes[] ignoring occupancy
    }
    if (num != -1) {
        cfg->mTrackNum = num;
        // v1: last-writer-wins on mTrackDiffs[num] (Phase 2 clones per-player diff)
        SetTrackDiff(thisList, num, cfg->mDifficulty);
        SetTrackOccupied(thisList, num, 1);
    }
    // never MILO_FAIL under the flag
}
```
> `FirstSlotOfExactType` is a tiny C scan over `mTrackTypes[]` (same loop as
> `TrackNumOfExactType` but ignoring `mTrackOccupied`). Offsets of `mTrackTypes`/
> `mTrackOccupied`/`mTrackDiffs` (STL vectors) come from Ghidra (§8). Alternatively, if
> the retail `ProcessConfig` is small and not inlined, a surgical `POKE_32` that
> replaces the `MILO_FAIL` branch with "reuse first exact-type slot" is possible — but
> the detour+reimplement is clearer and testable.

### 6.4 The centerpiece — per-watcher gem-list clone

- **Target (REVISED): `TrackWatcherImpl::RecalcGemList()`**, *not* `NewTrackWatcherImpl`.
  Per §3.2, `mGemList` is (re)assigned from the shared list on every reset via
  `RecalcGemList` (`TrackWatcherImpl.cpp:66-68`), so a one-time install at construction
  is clobbered by the first `ResetGemStates`. `RecalcGemList` is the single choke-point
  for `mGemList` and the correct place to inject the clone.
- **Wii oracle:** `TrackWatcherImpl::RecalcGemList` = `{ mGemList = mSongData->GetGemList(mTrack);
  HandleDifficultyChange(); }`. Clone helpers `GameGemList::CopyFrom`
  (`GameGemList.cpp:106-110`), `GameGemDB::Duplicate` (`GameGemDB.cpp:57-63`).
- **Retail address:** `PORT_TWI_RECALCGEMLIST` — **unpinned**, derive via §8 (find the
  function whose body is exactly `mGemList = GetGemList(track); HandleDifficultyChange()`).
  Fallback if it is inlined: hook one level up at `TrackWatcher::RecalcGemList`
  (`TrackWatcher.cpp:117`, `{ mImpl->RecalcGemList(); }`) or at
  `BeatMatcher::ResetGemStates` and re-assert `mGemList` after the original runs.
- **Clone timing — RESOLVED (item 3c).** The MIDI parse runs to completion **before**
  gameplay: `SongData::LoadData` blocks on `while (!Poll());` (`SongData.cpp:227-229`),
  and `Poll` only returns `true` once every gem has been added and `PostLoad` has run
  (`:256-258`). Gameplay objects (`Band::NewPlayer` → `GemPlayer` → `BeatMatcher`) are
  created afterward, and `BeatMatcher::SetTrack` builds the `TrackWatcher`
  (`BeatMatcher.cpp:314-320`); `ResetGemStates` (→ `RecalcGemList`) then fires at song
  start (`GemPlayer::ResetGemStates`) **before any hit is read**. So installing the clone
  at the `RecalcGemList` choke-point is guaranteed to copy a **fully-populated** list —
  which is exactly why the construction-time install (whose ordering vs. parse was the
  open worry) is the *wrong* trigger and the `RecalcGemList` install is the *right* one.
- **Hook type:** `HookFunction` detour on `RecalcGemList`: run the original (which sets
  `mGemList = shared` + `HandleDifficultyChange`), then, if this watcher is a 2nd+
  claimant of its track, overwrite `mGemList` with its private clone's current-difficulty
  list.
- **Design — a per-song track→refcount map** keyed on the *impl pointer*: the first impl
  to `RecalcGemList` a given track keeps the shared list (zero overhead in the common
  case); each additional impl on that track is handed its own `GameGemDB` clone. Clones
  are tracked for teardown.

```c
// SameInstrumentHooks.c
#define MAX_CLONES 8
typedef struct { int track; int claims; } TrackClaim;
typedef struct { void *impl; void *clonedDB; } ImplClone;   // clonedDB = GameGemDB* we own
static TrackClaim gClaims[MAX_CLONES];
static ImplClone  gImplClones[MAX_CLONES];
static int gCloneCount = 0;

char RecalcGemListStub[8];   // verify prologue size HookFunction copies
void RecalcGemListHook(void *impl)   // impl = TrackWatcherImpl*
{
    ((void(*)(void*))RecalcGemListStub)(impl);   // original: mGemList=shared; HandleDifficultyChange()
    if (!config.AllowSameInstrument) return;

    int track = ImplTrack(impl);                 // read TrackWatcherImpl::mTrack (offset via Ghidra)
    ImplClone *ic = FindImplClone(impl);
    if (!ic) {                                    // first time we see this impl on this track
        TrackClaim *c = FindOrAddClaim(track);
        if (++c->claims == 1) return;             // 1st watcher of the track: keep shared list
        void *db = GameGemDBDuplicate(SongDataGetGemDB(impl_songdata(impl), track)); // clone WHOLE DB
        if (gCloneCount < MAX_CLONES) { ic = &gImplClones[gCloneCount++]; ic->impl=impl; ic->clonedDB=db; }
        RB3E_MSG("same-instrument: cloned gem DB for track %i (claim %i)", track, c->claims);
    }
    if (ic) {
        int diff = ImplDifficulty(impl, track);   // = SongData::mTrackDifficulties[track] for v1
        SetImplGemList(impl, GameGemDBGetDiffList(ic->clonedDB, diff));  // re-point mGemList -> clone
    }
}
```
- **Why clone the whole `GameGemDB`, not one `GameGemList`.** `GetGemList` returns only
  the *current-difficulty* list (§3.2); cloning the whole DB (`GameGemDB::Duplicate`,
  which internally `CopyFrom`s every difficulty) lets `RecalcGemList` re-index the right
  difficulty on each call and makes Phase-2 per-player difficulty a one-line change
  (index by the *player's* diff instead of the shared `mTrackDifficulties[track]`, §7).
- **Reset semantics (verify in spike 0.4).** `ResetGemStates`/section-jump resets the
  *shared* list's played bits (via `Jump → SetAllGemsUnplayed → Reset`, §3.2) but not
  the clone. If a section restart or difficulty change must re-zero the clone, have the
  detour `GameGemListCopyFrom(cloneDiffList, sharedDiffList)` right after re-pointing
  (the shared list is in its canonical post-reset state at that moment). Confirm whether
  this is needed by testing a mid-song section restart with two co-track players.
- **`SetImplGemList` / `ImplTrack` / `ImplDifficulty`** — write/read the `mGemList` and
  `mTrack` fields at their `TrackWatcherImpl` offsets (from Ghidra on the 360 body; Wii
  header `TrackWatcherImpl.h`, `mGemList` is a ctor init-list member).
- **Allocator symmetry — RESOLVED (item 3d).** RB3E already ports and *uses*
  `MemAlloc(size, 0)` / `MemFree(ptr)` (the Milo heap; `PORT_MEMALLOC 0x827bcd38`,
  `PORT_MEMFREE 0x827bc430`) throughout `FileSD.c` / `xbox360_files.c`.
  `GameGemDB::Duplicate` allocates via `new GameGemDB(...)` → Milo's global
  `operator new` → `MemAlloc`, and its inner `GameGemList`s via `new` too
  (`GameGemDB.cpp:7`). The **symmetric free** is therefore: call `~GameGemDB(db)`
  (`GameGemDB.cpp:12-16`, which `delete`s each inner `GameGemList` = their operator
  delete = `MemFree`), then `MemFree(db)` for the outer block. So port
  `PORT_GAMEGEMDB_DTOR` and free with `{ GameGemDBDtor(db); MemFree(db); }` — do **not**
  bare-`MemFree` a DB (leaks the inner lists).
- **Teardown / free.** The watcher dtor does **not** free `mGemList` (§3.3), so we own
  the clones. Hook `Game::__dt` (`PORT_GAME_DT = 0x8267b1f0`) — RB3E already detours it
  as `GameDestructHook`; add our sweep there:
```c
void FreeSameInstrumentClones(void)   // call from GameDestructHook
{
    for (int i = 0; i < gCloneCount; i++)
        if (gImplClones[i].clonedDB) {
            GameGemDBDtor(gImplClones[i].clonedDB);   // frees inner GameGemLists
            MemFree(gImplClones[i].clonedDB);         // frees the DB block (Milo global delete)
        }
    gCloneCount = 0;
    memset(gClaims, 0, sizeof(gClaims));
    memset(gImplClones, 0, sizeof(gImplClones));
}
```
> **No double-free:** clones are installed only on 2nd+ impls and freed exactly once at
> `Game::__dt`; the shared `mGemDBs[track]` is owned by `SongData` and never touched.

### 6.5 Wire-up

In `rb3enhanced.c ApplyHooks()` (360 branch), after the existing hooks:
```c
#ifdef RB3E_XBOX
    // same-instrument feature (all inert unless config.AllowSameInstrument)
    HookFunction(PORT_OVERSHELL_ISACTIVE,     &IsActiveStub,         &IsActiveHook);
    HookFunction(PORT_OVERSHELLPANEL_RESOLVE, &ResolveWaitStub,      &ResolveWaitStatesHook);
    HookFunction(PORT_PTCL_PROCESSCONFIG,     &ProcessConfigStub,    &ProcessConfigHook);
    HookFunction(PORT_TWI_RECALCGEMLIST,      &RecalcGemListStub,    &RecalcGemListHook);
#endif
```
(and add `FreeSameInstrumentClones()` inside the existing `GameDestructHook`).

---

## 7. Phase 2 (polish) & Phase 3 (online)

**Phase 2 — polish (independent of the v1 acceptance gate):**
- **Per-player difficulty.** v1 shares `mTrackDifficulties[track]` (last-writer-wins).
  Fix: clone from the correct per-difficulty list — `GetGemListByDiff(track,
  playerDiff)` — when installing the clone (§6.4 already leans this way via
  `GameGemDBDuplicate` + index-by-diff). Lets one player play Expert while another plays
  Medium on the same instrument.
- **Mode-flag OR.** `GameConfig::AssignTrack` mode flags
  (`SetUseRealDrums`/`SetUseVocalHarmony`/`unk2c`) are last-writer-wins with duplicate
  `mTrackNum`. OR them (or apply an explicit policy) so one player's real-drums choice
  doesn't silently override another's.
- **Unison credit.** `Game::GetPlayerFromTrack` returns first-match, so
  `CommonPhraseCapturer` unison credit lands on one player. If desired, make unison
  award all same-track players (iterate instead of first-match).
- **Score-screen verify.** Confirm end-of-song results show distinct rows/scores for
  same-instrument players (scores are per-`GemStatus`, so this should already work — just
  verify the results UI doesn't dedupe by track).

**Phase 3 — online (keep v1 LOCAL-ONLY):**
- The pre-join "instrument slot open in the gathering" check (issue #11's open ❌ item)
  and the online **instrument mask** (`PORT_SESSION_MASK_CHECK = 0x82652acc`, already in
  `ports_xbox360.h` — "beq in while loop for instrument mask check") both assume unique
  instruments per session. Handling these is a separate workstream; **gate the feature
  to local play in v1** and do not touch the session mask until Phase 1 is proven.

---

## 8. Address-derivation cookbook (real worked example: `ResolvePartWaitStates` via fingerprint)

For each unpinned `PORT_*`, derive the retail address against **rb3-xenon** / the retail
XEX (base `0x82000000`). The steps below use `ProcessConfig` as the running example for
Steps 1–2, but note up front: **its `MILO_FAIL` string is stripped from retail**, so the
naïve string-xref shortcut fails (Step 3) — the fingerprint method in Step 3 is what
actually landed `IsActive`/`RepresentSamePart`/`ResolvePartWaitStates` this session.

**Step 1 — decomp.db (is it already named?).**
```bash
cd /home/free/code/milohax/rb3-xenon
sqlite3 decomp.db "SELECT symbol, demangled, unit, current_percent
                   FROM functions
                   WHERE demangled LIKE '%ProcessConfig%'
                      OR demangled LIKE '%PlayerTrackConfigList%';"
```
If a row exists with a mangled symbol, its address is recoverable from
`config/45410914/splits.txt` / the unit's obj — or just `fn_resolver` it (Step 2).

**Step 2 — fn_resolver (identity for an address, or find the address).**
```bash
python3 tools/fn_resolver.py resolve 0x82XXXXXX      # identify a candidate address
python3 tools/fn_resolver.py unresolved --unit PlayerTrackConfigList  # bl-target worklist
```
`resolve` reports the best-tier identity + confidence; use it to confirm a candidate is
the function you want (as done for `Reload` → `0x8264BCE4` while writing this doc).
> **fn_resolver T1a gap:** for auto-clustered units (`auto_NN_ADDR_text`), the resolver
> can't map symbol→address directly. Compute manually: `int(ADDR,16) + offset` from the
> unit base. `BandUserMgr::GetUser/GetUsers` fall in these auto-clusters — hand-derive.

**Step 3 — Ghidra MCP (port 8002) for the hard cases / offsets.**
Decompile the candidate and confirm structure. Ghidra also gives the **struct field
offsets** needed by the C sketches (`mTrackNum`, `mTrackTypes`/`mTrackOccupied`/
`mTrackDiffs` vector heads, `TrackWatcherImpl::mGemList`/`mTrack`).

> **The `MILO_FAIL` string-xref shortcut does NOT work on retail — verified this
> session.** Ghidra's `search_strings` (substring-capable — `obsolete` matches
> `mc_auto_load_obsolete_version` etc.) returns **zero** hits for `Couldn't create track
> of type`, `head-to-head`, and `trying to play`. Retail TU5 stripped those `MILO_FAIL`
> format strings (many asserts survive, e.g. `Couldn't cast expression to boolean`, but
> the `PlayerTrackConfigList` ones do **not**). So you cannot land in `ProcessConfig`,
> `TrackNumOfType`, etc. via their assert text. Worse, those units are **not split**
> (`PlayerTrackConfigList` / `GameGemList` / `GameGemDB` have no `splits.txt` block, and
> no Ghidra symbol) — the fn_resolver T1a auto-cluster gap.

**The method that *does* work — call-count fingerprinting from a pinned anchor.** When a
target's assert string is gone, anchor on a *neighbour you can name*, then match by the
**number of call sites** the Wii source dictates. Worked example (done this session):

1. `OvershellPartSelectProvider::IsActive` **is** named in Ghidra
   (`?IsActive@…@@UBA_NH@Z` @ `0x8264B5F8`). Decompile it and list its call targets.
2. It calls `0x82671A60` **4×** and `0x82671818` **2×**, both inside the `Defines.cpp`
   split range (`0x82671A60–0x826725F0`). Decompiling `0x82671A60` yields the Ghidra
   symbol `GameMode::InMode` — the four `TheGameMode->InMode(...)` calls at Wii `:115-118`.
   That leaves `0x82671818` (2 call sites, matching `RepresentSamePart` at Wii `:109`,
   `:139`), and its body has RepresentSamePart's shape (build vector → `std::find` → bool).
3. `list_xrefs 0x82671818` → callers are `IsActive` (2×) **and `Function_8259D948`
   (3×)**. `RepresentSamePart` is called exactly **3×** in `ResolvePartWaitStates`
   (Wii `:926/:940/:964`) — so `0x82671818` = `RepresentSamePart` and
   **`0x8259D948` = `OvershellPanel::ResolvePartWaitStates`** (also inside the
   OvershellPanel address neighborhood). Both then byte-verified (prologue `mflr r12`).

Apply the same recipe to the remaining unpinned targets: `ProcessConfig` is the unique
caller of `TrackNumOfType` **and** the writer of `mTrackOccupied[num]=1`; `TrackNumOfType`
is the only function calling `TrackNumOfExactType` with the `kTrackRealGuitar`/`22Fret`
`switch`; `NewTrackWatcherImpl` is the function with **five `new`-of-`*TrackWatcherImpl`
branches** that reads `GetGemList` — and its lone `GetGemList` call *is*
`SongData::GetGemList`. `GameGemDB::Duplicate` is the small function that `new GameGemDB`
+ loops `CopyFrom`; the `CopyFrom` it calls is `GameGemList::CopyFrom`.

**Step 4 — ICF check (don't hook a folded function).** The retail link uses
`-OPT:ICF`, so identical small functions are folded to one address; hooking a folded
address hits every twin. Check before hooking a small function:
```bash
sqlite3 decomp.db "SELECT f.symbol, m.symbol_name, m.resolved_symbols
                   FROM merged_symbols m JOIN functions f ON f.id=m.function_id
                   WHERE f.demangled LIKE '%ProcessConfig%';"
```
> **TO-VERIFY (tooling):** the task referenced `bin/merged-symbols`; that path does not
> exist in the current tree (`bin/` holds only `objdiff-cli`), and the `merged_symbols`
> table is currently empty. Treat ICF-fold detection as: (a) query `merged_symbols` if
> populated, (b) otherwise inspect in Ghidra whether multiple symbols resolve to one
> address, and (c) consult `docs/plans/lto-vs-icf-investigation-2026-06-06.md` for the
> confirmed ICF behavior. `ProcessConfig` is large/unique enough that folding is
> unlikely; small leaf helpers (`TrackNumOfExactType`) are the real fold risk — hook the
> larger unique caller and reimplement the leaf in C (RB3E's standard pattern).

**Step 5 — byte-verify in `default.xex`.** Disassemble the derived address in the raw
retail image and confirm the prologue matches a function entry (not mid-function), so
`HookFunction`'s first-instruction copy is valid:
```bash
# base 0x82000000; confirm the bytes at the derived VA are a real prologue (mflr/stwu…)
python3 tools/... disasm 0x82XXXXXX   # or Ghidra listing at the VA
```

**MSVC-inlining caveat.** If the target was inlined at all call sites (no standalone
body — like `BuildInstrumentSelectionList` and `AddInstrumentToList` were), there is
nothing to `HookFunction`. Then reimplement in C and `POKE_B` over one call-site, or
hook the **outermost non-inlined caller** and reimplement the leaf — again the
`OvershellHooks.c` pattern.

---

## 9. Test plan

### 9.1 Xenia — Phase 1 acceptance (all with `AllowSameInstrument=true`)

1. **Boot regression:** vanilla flows unaffected with the flag **off** (default). Boot,
   reach song select, play a normal single-instrument song — identical to stock RB3E.
2. **Two guitars select:** two controllers both pick Guitar in part-select; **no
   grey-out** (Layer A), **no kick-back** (Layer B), **no `MILO_FAIL`** (Layer C). Song
   starts.
3. **Independent hits:** both play the guitar chart; hitting/missing on one pad does
   **not** consume the other's gems (no note-stealing). Verify by having P1 miss a run
   while P2 hits it — P2's streak climbs, P1's breaks, independently.
4. **Independent scores/streaks:** end-of-song shows two distinct scores/streaks for the
   two guitarists.
5. **Sustains & rolls:** overlapping sustains/rolls each track per-player
   (`mGemsInProgress`/`unk10b1` on the private clones) — no shared sustain state.
6. **No theft edge cases:** simultaneous hits on the same gem both register for their own
   player.
7. **Clean song end:** song ends normally (`TrulyWinGame`/results) with two
   same-instrument players; no hang, no assert.
8. **No leak across songs:** play song A (2 guitars) → quit to menu → play song B. Clone
   teardown ran (watch for the `FreeSameInstrumentClones` path); memory overview stable
   (`MemPrintOverview` if `DEBUG=1`); no growth per song.
9. **Scale:** repeat with 3–4 players on one instrument, and with mixed
   (2 guitars + 1 drums + 1 keys) to confirm non-same-instrument tracks are untouched.
10. **Difficulty (v1 known-limitation check):** two guitars on the **same** difficulty
    works; different difficulties is a Phase-2 item — confirm it degrades gracefully
    (shares diff) rather than crashing.

### 9.2 Hardware sanity (RGH/JTAG)

Once Xenia passes, smoke-test on console: flag off = stock behavior; flag on = two
guitars start and score independently on one real song. Confirm no NAND/loader
regressions. Keep online **off** (Phase 3).

---

## 10. Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| MSVC inlined a target (no body to `HookFunction`) | Med | Hook outermost non-inlined caller + reimplement leaf in C (`OvershellHooks.c` pattern, §8) |
| ICF folded a small target → hook hits twins | Low–Med | §8 Step 4 ICF check; prefer hooking the large unique caller |
| Wrong struct offset (`mGemList`, `mTrackNum`, vector heads) | Med | Derive every offset from Ghidra on the **360** body, not the Wii header; byte-verify |
| Clone lifetime bug (leak or double-free) | Med | Refcount map installs clones only on 2nd+ watcher; free once in `GameDestructHook`; verify with `MemPrintOverview` across songs (test 9.1.8) |
| Clone reverted by `RecalcGemList` re-borrow | **High if missed** | Install the clone at the `RecalcGemList` choke-point, not at watcher construction (§3.2, §6.4); spike 0.4 confirms it sticks past `ResetGemStates` |
| Clone timing (gems added after watcher creation) | Low (resolved) | Parse blocks to completion before gameplay (`while(!Poll())`, §6.4); cloning at first `RecalcGemList` is guaranteed post-populate |
| Clone not re-zeroed on section restart / diff change | Med | `CopyFrom` shared→clone in the `RecalcGemList` detour (§6.4); verify with a mid-song section restart in spike 0.4 |
| `ResolvePartWaitStates` no-op **strands** waiters in ChoosePartWait | Med | Do NOT no-op wholesale; advance ChoosePartWait users to `kState_ChooseDiff` under the flag (§6.2) |
| Vocals path (harmony `Singer` objects) accidentally hit | Low | Feature gated to non-vocal tracks in v1; `NumSingers()` hardcoded 3 untouched |
| Online session-mask assumes unique instruments | Med | v1 local-only; Phase 3 handles `PORT_SESSION_MASK_CHECK` |
| Xenia load path | Low (resolved) | `.patch.toml` mechanism confirmed (§4.4); needs a Canary build with `writable_code_segments` |
| RB3ELoader hardware paths differ | Med | `RB3ELoader.xex` = DashLaunch plugin confirmed (§4.5); exact DLL/ini console paths still from the release zip — Xenia-first bring-up sidesteps this |
| Address drift (wrong TU) | Low | TU5 confirmed (§2.3); every derived address byte-verified in `default.xex` |

---

## 11. Contribution path

1. **Fork RB3Enhanced**, branch `same-instrument`, land the config flag + new hooks as
   above. Keep the feature fully gated behind `AllowSameInstrument` (inert by default) —
   matches upstream's conservative default posture.
2. **Land against [issue #11](https://github.com/RBEnhanced/RB3Enhanced/issues/11)** —
   it explicitly enumerates the UI-side items (done upstream) and the session item (our
   Phase 3). Our PR closes the *gameplay* gap (Layers A–C + gem-clone) the author flagged
   as the blocker ("first player to hit a note steals it, others overstrum").
3. **Cite the decomps as oracles** in the PR: rb3-Wii (`/home/free/code/milohax/rb3`)
   for behavior (the `file:line` refs in §3), rb3-xenon for the TU5 addresses. This gives
   maintainers a verifiable derivation for every new `PORT_*`.
4. Provide the §9 Xenia checklist as the PR's test evidence; include `DEBUG=1` logs
   showing the clone install + teardown markers.

---

### Appendix — reference addresses already in `ports_xbox360.h` (reuse, don't re-derive)

`PORT_ADDGAMEGEM 0x8278e530`, `PORT_WILLBENOSTRUM 0x8278cbb0`,
`PORT_GAME_CT 0x8267bf30`, `PORT_GAME_DT 0x8267b1f0`,
`PORT_GAMEGETACTIVEPLAYER 0x82678e88`, `PORT_GETBANDUSERS 0x82683b78`,
`PORT_GETBANDUSERFROMSLOT 0x82682b60`, `PORT_THEBANDUSERMGR 0x82e023b8`,
`PORT_BUILDINSTRUMENTSELECTION 0x82668c70`, `PORT_MEMALLOC 0x827bcd38`,
`PORT_MEMFREE 0x827bc430`, `PORT_SESSION_MASK_CHECK 0x82652acc` (Phase 3),
`PORT_MODIFIERMGR_ACTIVE 0x82588d80`, `PORT_RANDOMINT 0x824f2f90`.

rb3-xenon pinned (verify entry with §8 before hooking):
`OvershellPartSelectProvider::Reload 0x8264BCE4` (100%),
`OvershellPartSelectProvider::IsActive` (99.96%, unit `.text` `0x8264B5F8–0x8264C238`),
`OvershellSlot::UpdateState 0x825C4DF8` (75.8%),
`SetUserTrackTypeMsg::Save 0x82672774`, `TrainerGemTab::Init 0x826D0360`.

**Derived this session (prologue-verified; not yet runtime-verified):**
`OvershellPartSelectProvider::IsActive 0x8264B5F8` (= `PORT_OVERSHELL_ISACTIVE`),
`OvershellPanel::ResolvePartWaitStates 0x8259D948` (= `PORT_OVERSHELLPANEL_RESOLVE`),
`RepresentSamePart 0x82671818`, `GameMode::InMode 0x82671A60`. Derivation + method in §8.
