# Player +4 layout — RESOLVED: it's `utl/SongPos.h` (DC3 `mPhrase`), not Player.h

**Research dossier, 2026-06-11.** Baseline: main @ `154a11a`, 6932/65544 matched.
Status: root cause **identified and multiply proven**; ready for implementation.

## TL;DR verdict

- **The "Player +4" is NOT a Player.h problem and NOT a vbase-MI wall.** It is a
  single DC3-vs-RB3 member delta in **`src/system/utl/SongPos.h`**: our header is
  DC3's (sizeof 0x18, with `int mPhrase; // 0x8`), while **retail RB3-360 SongPos is
  0x14 with NO mPhrase** (== the rb3-Wii header `../rb3/src/system/utl/SongPos.h`).
- Direction: **ours = retail + 4** (we have 4 excess bytes). The excess sits inside
  `Performer::mSongPos` (Performer+0x21c), so every member of Performer from
  `mQuarantined` (retail 0x230 / ours 0x234) upward — and therefore ALL of
  MsgSource-subobject + Player-own + VocalPlayer-own + the Hmx::Object vbase —
  shifts +4 in our build.
- Exact onset (Player-object coordinates): **first shifted byte is Performer+0x224**
  (= mSongPos+0x8, where DC3 inserted `mPhrase`). Offsets ≤ 0x220 agree
  (`mSongPos.mTotalTick` @0x21c, `mTotalBeat` @0x220, `mScore` @0x218, `mBand` @0x210,
  `mPollMs` @0x8, `mStats` @0x10 — all verified equal).
- The fix is a 2-file edit (SongPos.h + one call-site in HamSongData.cpp).
  **Player.h needs no change.** Est. **+8 high-confidence flips** (+up to ~13 with
  refill), with 4 more fns partially improved but gated by small independent residues
  documented below.

## 1. Reconciling the two bracket observations

The two observations never actually disagreed — they were both loose lower bounds
from different member-access footprints:

| observation | claim | what it actually was |
|---|---|---|
| batch-2 dossier (docs/decomp/research/2026-06-10-force-multipliers.md, Lever 4) | "all member reads ≥0x260 shifted; mUser at 0x260/0x264" | `GetBandTrack`'s **lowest** member access is `mUser` (retail 0x260 / ours 0x264). True onset is lower. |
| SetEnergy port (commit `8bb12f0`) | "all fields ≥0x2a0 shifted; unk2a0 prime candidate" | `SetEnergy`'s **lowest** member access is `mBandEnergy` (retail 0x2a0 / ours 0x2a4). `unk2a0` is innocent. |

Both are consistent with onset ≤ 0x260. The real onset (0x224, inside
`Performer::mSongPos`) was found by decompiling the **retail Player and Performer
constructors**, which store every member in declaration order — a complete layout
oracle for the retail side, mirrored by self-diffing our compiled ctor for our side.

The batch-2 refutation note ("+4 sits in the vbase prefix below 0x260", roadmap
`docs/plans/decomp-state-and-roadmap-2026-06-09.md:577-579`) was directionally right
(it IS below Player's own members, inside the Performer base) but wrong to call it a
vbase-MI *wall* — it's a plain DC3-added-member delta, the same family as ADSR
`mPacked` (+2) and the WorldCrowd/RndTex levers.

## 2. Evidence

### 2.1 Retail constructors (Ghidra, port 8002, binary `/default.xex-35adb6`)

- **Retail `Player::Player(BandUser*,Band*,int,BeatMaster*)` = `fn_82688E40`**
  (0x82688E40, size 704 — *exactly* our compiled ctor size). Found via
  `fingerprints.json`: the only fn whose callees include 0x826844E0
  (`??0PlayerParams@@QAA@XZ`, pinned + matched 100%). NOTE: it is OUTSIDE the pinned
  Player.cpp .text range (0x826843F0–0x82686C50, splits.txt:2390) — do not expect it
  to count toward the unit.
  Decompile: `venv/bin/python tools/ghidra/ghidra-decompile.py 0x82688e40`
- **Retail `Performer::Performer(BandUser*,Band*)` = `fn_8267F0F0`** (0x8267F0F0),
  called from the Player ctor. Decompile: `... ghidra-decompile.py 0x8267F0F0`
- Cross-confirmations inside the retail Player ctor: calls `Function_826847B0`
  (= `?SetVocals@PlayerParams@@QAAXXZ`, pinned+matched @0x826847B0) under
  `GetTrackType()==3`; `Function_826844E0` after `new(0x28)`; `SystemConfig
  ("track_graphics")->FindArray("popup_help_intro_duration_ms")` tail — all matching
  our `src/band3/game/Player.cpp:66-90` exactly.

Our side: self-diff our compiled obj to disassemble (no rebuild):

```
bin/objdiff-cli diff -1 build/45410914/src/band3/game/Player.obj \
                     -2 build/45410914/src/band3/game/Player.obj \
  '??0Player@@QAA@PAVBandUser@@PAVBand@@HPAVBeatMaster@@@Z' \
  --full-listing -f json -o /tmp/claude/our_ctor.json
```

### 2.2 Composite layout table (retail ctor vs our compiled ctor)

Word-by-word from `fn_82688E40` (retail, `puVar2[i]` = this+4*i) vs our ctor stores:

| member (Player.h name) | retail | ours | Δ |
|---|---|---|---|
| vptr / vbptr `??_8Player@@7BPerformer@@` | 0x0 / 0x4 | 0x0 / 0x4 | 0 |
| `Performer::mPollMs` | 0x8 | 0x8 | 0 |
| `Performer::mCrowd` | 0xc | 0xc | 0 |
| `Performer::mStats` (Stats ctor `Function_8267AC88`) | 0x10 | 0x10 | 0 |
| `Performer::mBand` | 0x210 | 0x210 | 0 |
| `Performer::mScore` | 0x218 | **0x218** (proven: our `?GetAccumulatedScore@Player@@UBAHXZ` = `lfs f0, 0x218(r3)`) | 0 |
| `Performer::mSongPos` | 0x21c | 0x21c | 0 |
| — retail SongPos zero-inits +0x0,+0x8,+0xc,+0x10 and **skips +0x4** | 0x21c/0x224/0x228/0x22c | (we don't compile Performer.cpp) | — |
| `Performer::mQuarantined` | **0x230** | **0x234** (our `?PostDynamicAdd@Player@@UAAXXZ`: `stb r10, 0x234(r31)`) | **+4** |
| `Performer::mProgressMs` / `mGameOver` / `mMultiplierActive` / `mNumRestarts` | 0x234/0x238/0x239/0x23c | 0x238/0x23c/0x23d/0x240 | +4 |
| **MsgSource subobject** (`??_8Player@@7BMsgSource@@`) | **0x240** | **0x244** | +4 |
| `Player::mParams` | 0x258 | 0x25c | +4 |
| `Player::mBehavior` | 0x25c | 0x260 | +4 |
| `Player::mUser` | 0x260 | 0x264 | +4 |
| `Player::mCommonPhraseCapturer` | 0x264 | 0x268 | +4 |
| `mRemote` / `mPlayerName(String)` / `mTrackNum` / `mTrackType` / `mEnabledState` / `mTimesFailed` | 0x268/0x26c/0x278/0x27c/0x280/0x284 | 0x26c/0x270/0x27c/0x280/0x284/0x288 | +4 |
| `unk260` vector (3 words — retail is ALSO a 12-byte vector; sized-vector refutation re-confirmed) | 0x290–0x298 | 0x294–0x29c | +4 |
| `mIsInCoda` / `mBandEnergy` / `mDeployingBandEnergy` / `unk274` / `unk278` / `mPhraseBonus` / `mBeatMaster` / `unk284` | 0x29c/0x2a0/0x2a4/0x2a8/0x2ac/0x2b0/0x2b4/0x2b8(=5000.0f) | +4 each (0x2a0…0x2bc) | +4 |
| `unk288…unk2a4` block | 0x2bc…0x2d8 | 0x2c0…0x2dc | +4 |
| `mDisconnectedAtStart…mHasBlownCoda` bytes | 0x2dc–0x2e7 | 0x2e0–0x2eb | +4 |
| `unk2b4/unk2b8/unk2bc/unk2c0(=-1)/unk2c4(=1)` | 0x2e8/0x2ec/0x2f0/0x2f4/0x2f8 | 0x2ec/0x2f0/0x2f4/0x2f8/0x2fc | +4 |
| **Hmx::Object vbase** (vbase-adjust constant) | **0x300** (retail stores `vbtable−0x2fc`) | **0x304** (ours `subi r10, r11, 0x300`) | +4 |

Header `// 0xNN` comments in Player.h are stale by −0x34 below the onset and −0x38
above it (Performer/Stats are much bigger on 360 than the Wii comments); ignore them.

### 2.3 Retail Performer ctor detail (`fn_8267F0F0`) — the onset proof

Init stores (word idx ×4): `[2]`=0 → mPollMs 0x8; Stats ctor at +0x10;
`[0x84]`=band → **mBand 0x210**; bytes 0x214/0x215/0x216 (unk1e0-2);
`[0x86]`=0 → mScore 0x218; **SongPos inline default-ctor zeroes 0x21c, 0x224, 0x228,
0x22c and SKIPS 0x220** — exactly the rb3-Wii `SongPos()` ctor
(`SongPos() : mTotalTick(0), mMeasure(0), mBeat(0), mTick(0) {}` — no mTotalBeat,
no mPhrase); byte `[0x8c]`=0 → **mQuarantined 0x230**; bytes 0x231/0x232/0x233=1;
`[0x8d]`=0 → mProgressMs 0x234; byte 0x238=0 mGameOver; byte 0x239=1
mMultiplierActive; `[0x8f]`=0 mNumRestarts 0x23c; vbase adjust `−0x240`
(vbase Object at 0x4+0x240=0x244 in standalone Performer). So retail
**sizeof(SongPos)=0x14, field offsets = Wii exactly** (mTotalTick 0x0, mTotalBeat 0x4,
mMeasure 0x8, mBeat 0xc, mTick 0x10), and retail **sizeof(Stats)=0x200**
(0x10→0x210), identical to ours (mBand agrees at 0x210; Stats-internal layout is
also clean — Stats.cpp unit has 15 matched fns incl. `?SaveSingerStats@Stats@@QBAXAAVBinStream@@@Z` @100%).

### 2.4 Near-miss instruction evidence (target asm vs our objs; no rebuilds)

Generated via (note: NO `--build` flag → diffs existing objs, doesn't touch the tree):

```
bin/objdiff-cli diff -p . -u default/band3/game/Player '<mangled>' \
  --include-instructions -f json -o /tmp/claude/x.json
venv/bin/python scripts/analysis/diff_inspect.py /tmp/claude/x.json --offsets
```

| fn | report % | shifted pairs (target→ours) | non-layout residue |
|---|---|---|---|
| `?GetBandTrack@Player@@QBAPAVBandTrack@@XZ` | 93.90 | 0x260→0x264 (mUser) | `cmplwi`→`cmpwi` (see §6.1) |
| `?Rollback@Player@@UAAXMM@Z` | 97.46 | 0x260→0x264 | same `cmplwi` residue (inlined GetBandTrack) |
| `?GetEnabledStateAt@Player@@QBA?AW4EnabledState@@M@Z` | 95.49 | 0x280→0x284, 0x288→0x28c, 0x290→0x294, 0x294→0x298 | compare-shape: retail `cmpwi ==2; beq; cmpwi ==3; bne` vs ours `subi; cmplwi; bgt` range-check (source-shape, see §6.2) |
| `?SetMultiplierActive@Player@@UAAX_N@Z` | 98.48 | 0x260→0x264 | same `cmplwi` residue |
| `?SetEnergy@Player@@QAAXM@Z` | 99.93 post-`8bb12f0` (on-disk obj is one commit stale) | 0x2a0→0x2a4 ×3 (mBandEnergy), 0x2d8→0x2dc (unk2a4), 0x2e4→0x2e8 (unk2b0) | none — **flips to 100** |
| `?Rollback@VocalPlayer@@UAAXMM@Z` | 89.88 | 0x390→0x394, 0x398, 0x388, 0x38c, 0x3cc→0x3d0 (5×+4) | 4 tail `insert`s (epilogue restore shape) — may not flip |
| `?OnGameOver@VocalPlayer@@QAAXXZ` | 99.91 | 3×+4 (0x390, 0x394, addi 0x3cc) | only bl/reloc-NAME noise (report ignores) — **flips** |
| `?ChangeDifficulty@VocalPlayer@@UAAXW4Difficulty@@@Z` | 99.84 | 5×+4 (0x278, 0x384, 0x388, 0x390, 0x394) | name noise only — **flips** |
| `?GetSpotlightPhraseID@VocalPlayer@@QBAHXZ` | 99.91 | 2×+4 | — **flips** |
| `?GetNextPhraseMarker@VocalPlayer@@QBAPBVVocalPhrase@@ABQBV2@@Z` | 99.67 | 1×+4 | — **flips** |
| `?SetAutoplay@VocalPlayer@@UAAX_N@Z` | 99.80 | 5×+4 | — **flips** |
| `?GetFreestyleDeploymentRequiredMs@VocalPlayer@@QBA_NAAM@Z` | 99.90 | 7×+4 | — **flips** |
| `?GetFrameMatchType@Singer@@QAAHXZ` | 99.91 | single diff: `lwz r10, 0x390(r10)`→`0x394` (VocalPlayer::mVocalParts) | — **flips** |

NOT this lever (checked, do not count): `?SetQuarantined@Player@@UAAX_N@Z` 25.6
(real body divergence + +4s; partial help only), `?ResolveAmbiguity@Singer@@QAAXXZ`
69.4 (ours emits `divw` by 0x14 = ptr-difference/sizeof(AmbiguousData) loop idiom
retail lacks — body-port class), `fn_8268663C` 99.9 (EH-funclet parent-frame-size
0xa0 vs 0xb0 — funclet class), `fn_826C75B4` 92.5 (guard-bit thunk mask mismatch
`rlwinm …,31,29` vs `clrrwi …,1` — pairing artifact).

## 3. Root cause

`src/system/utl/SongPos.h` came from DC3 in the original scaffold commit
(`8b28623` "Scaffold engine + math library from dc3-decomp") and was never
re-based against the rb3-Wii oracle:

- ours/DC3 (`../dc3-decomp/src/system/utl/SongPos.h`, byte-identical): size 0x18 —
  `mTotalTick, mTotalBeat, int mPhrase /*0x8*/, mMeasure, mBeat, mTick`, default ctor
  inits all six, 6-arg value ctor, has `GetPhrase()/AccessPhrase()`.
- rb3-Wii (`../rb3/src/system/utl/SongPos.h`): size 0x14 — NO mPhrase, default ctor
  `: mTotalTick(0), mMeasure(0), mBeat(0), mTick(0)` (**skips mTotalBeat** — and the
  retail Performer ctor reproduces exactly that skip), 5-arg value ctor
  `SongPos(float tt, float tb, int m, int b, int t)`.

Retail RB3-360 == rb3-Wii form, proven independently by (a) the Performer-ctor
zero-store pattern, (b) mQuarantined @0x230 = 0x21c+0x14, (c) every downstream +4.

## 4. Proposed edit

**File 1 — `src/system/utl/SongPos.h`** (the only layout change). Gate the DC3 field
and its accessors; adopt the Wii ctors exactly:

```cpp
#pragma once

// Retail RB3-360 SongPos is 0x14 — NO mPhrase (proven 2026-06-11: retail
// Performer ctor fn_8267F0F0 zero-inits SongPos at +0x0/+0x8/+0xc/+0x10,
// skipping +0x4, and Performer::mQuarantined sits at 0x230 = 0x21c + 0x14).
// DC3 later inserted int mPhrase @ 0x8 (size 0x18). Keep the DC3 form only
// behind SONGPOS_DC3_PHRASE (BeatClock.cpp, currently compiled nowhere).
// See docs/decomp/research/2026-06-11-player-plus4-layout.md.
class SongPos {
private:
    float mTotalTick; // 0x0
    float mTotalBeat; // 0x4
#ifdef SONGPOS_DC3_PHRASE
    int mPhrase;
#endif
    int mMeasure; // 0x8
    int mBeat; // 0xc
    int mTick; // 0x10
public:
    // NOTE: retail/Wii default ctor deliberately does NOT init mTotalBeat.
    SongPos() : mTotalTick(0), mMeasure(0), mBeat(0), mTick(0) {}
    SongPos(float totalTick, float totalBeat, int measure, int beat, int tick)
        : mTotalTick(totalTick), mTotalBeat(totalBeat), mMeasure(measure),
          mBeat(beat), mTick(tick) {}
    float GetTotalTick() const { return mTotalTick; }
    float GetTotalBeat() const { return mTotalBeat; }
    int GetMeasure() const { return mMeasure; }
    int GetBeat() const { return mBeat; }
    int GetTick() const { return mTick; }

    int &AccessMeasure() { return mMeasure; }
    int &AccessBeat() { return mBeat; }
    int &AccessTick() { return mTick; }
    float &AccessTotalTick() { return mTotalTick; }
    float &AccessTotalBeat() { return mTotalBeat; }
#ifdef SONGPOS_DC3_PHRASE
    int GetPhrase() const { return mPhrase; }
    int &AccessPhrase() { return mPhrase; }
#endif
};
```

Three load-bearing details, all byte-visible wherever the ctors inline:
1. drop `mPhrase` (the 4 bytes);
2. default ctor must NOT init `mTotalBeat` (4 zero-store sites: e.g.
   `HamMaster.cpp:106 mPrevSongPos = SongPos();`, TaskMgr ctor);
3. value ctor becomes 5-arg.

**File 2 — `src/system/hamobj/HamSongData.cpp:52`**: the only compiled 6-arg call
site. `return SongPos(tick, beat, 0, m, b, t);` → `return SongPos(tick, beat, m, b, t);`
(the dropped `0` was the phrase arg).

**File 3 (optional, keeps tree consistent) — `src/system/world/BeatClock.cpp`**:
compiled by NOTHING (not in `config/45410914/objects.json`, not in `native/`), but it
uses `AccessPhrase()/GetPhrase()` at lines 31, 147, 153. Either leave untouched (it
already doesn't build) or wrap those three lines in `#ifdef SONGPOS_DC3_PHRASE`.
`native/` has zero references to SongPos/BeatClock — no HX_NATIVE coupling.

**No change to Player.h, Performer.h, Stats.h, Task.h, BeatMaster.h, BeatMatcher.h,
Game.h** — their declarations are correct; only the embedded SongPos size was wrong.

## 5. Blast radius

### 5.1 TUs that embed SongPos (members after it shift −4 on recompile)

| class (member @ ours-coords) | TU | compiled? | pinned? | effect |
|---|---|---|---|---|
| Performer::mSongPos @0x21c | (no Performer.cpp in tree; layout flows into every Player-chain TU) | — | — | the main lever |
| Player/VocalPlayer/Singer chain | band3/game/{Player,VocalPlayer,Singer}.cpp | yes | yes (splits.txt:2390/2398, Singer @0x826D8B98) | **improvers**, §2.4 |
| TaskMgr::mSongPos @0x30 (then mAutoSecondsBeats/unk4c/mTime/mAVOffset/unk84 shift) | system/obj/Task.cpp | yes | yes (`default/Task`) | possible flips in the 0% pool (fn_82724FE8 = TaskMgr::Poll-shaped, 640B); **all 16 currently-matched Task fns verified = guard-bit/EH thunks touching NO TaskMgr members → no regression exposure** |
| BeatMaster::mSongPos @0x30 + mLastSongPos | system/beatmatch/BeatMaster.h | **BeatMaster.cpp NOT compiled** | — | none now; future TU port gets correct layout |
| BeatMatcher::mSongPos @0x64 | system/beatmatch/BeatMatcher.h | NOT compiled | — | none |
| Game::mSongPos @0x40 | band3/game/Game.h | Game.cpp NOT compiled | — | none (but see §6.3) |
| HamMaster::mSongPos @0x60 + mPrevSongPos | system/hamobj/HamMaster.cpp | yes | yes (1 fn, 0 matched) | neutral→up |
| HamSongData (returns SongPos by value, 6-arg ctor) | system/hamobj/HamSongData.cpp | yes | yes (2 fns, 1 matched) | needs File-2 edit; CalcSongPos may improve |

### 5.2 TUs that read SongPos internal fields (mMeasure/mBeat/mTick move 0xc/0x10/0x14 → 0x8/0xc/0x10)

Compiled call sites audited one by one — most "GetTick()" hits are `Gem::GetTick`
etc., NOT SongPos:

- **real SongPos-field readers (will re-compile to retail offsets — upside only):**
  `band3/game/GamePanel.cpp:399-402` (`UpdateNowBar`, MILO_DEBUG-on per macros.h,
  reads tm.GetSongPos().GetMeasure/GetBeat/GetTick), `system/obj/Task.cpp:424`
  (MBT MakeString) + `:366` total_tick handler, `band3/game/TrainerPanel.cpp:124`
  (GetTotalTick — offset 0x0, unaffected), `band3/game/Player.cpp:747-748` /
  `VocalPlayer.cpp:903` (GetTotalTick — unaffected).
- **false positives (verified unrelated, no action):** MasterAudio.cpp:595
  (`Gem::GetTick` — the flagship 34-match unit is SAFE), Song.cpp (Song::GetBeat /
  MeasureMap), TrainerGemTab/Lyric/Gem/SongDB/GemTrack (gem/phrase GetTick),
  MoveDir/MoveAsyncDetector (MoveFrame::GetBeat, TheMaster->GetMeasure),
  TrackPanelDir.cpp:612 IsGameOver (= virtual `TrackPanelInterface::IsGameOver`
  vcall, NOT Performer's inline).
- **no compiled user of `mPhrase`/`GetPhrase()`/`AccessPhrase()` anywhere**
  (only BeatClock.cpp, uncompiled). `SongDB.cpp`/`GemTrack.cpp` "GetPhrase" hits are
  `PhraseAnalyzer::GetPhrase*`/`GetPhraseExtents` — unrelated.

### 5.3 TUs that inline Player/Performer accessors at shifted offsets (recompile-correct automatically; audit set for the A/B)

`GetUser()`(mUser), `GetTrackType()`, `GetEnabledState()`, `GetBandEnergy()`,
`GetQuarantined()`, `IsGameOver()`, `SetBlownCoda()` bake the +4 into:
GemTrack, VocalTrack, GamePanel, SongDB, Singer, OvershellSlot,
OvershellPartSelectProvider, MusicLibrary, Utl(meta_band), RockCentral,
AccomplishmentOneShot, AccomplishmentPlayerConditional, QuestFilterPanel,
TrainerPanelDir-adjacent. (Some hits are on BandUser, not Player — fine either way;
these TUs are the per-unit regression audit list for the A/B.)
`ContextChecker::GetNumRestarts` is NOT header-inline (declared only) — unaffected.

### 5.4 Predicted +N

- **High confidence (+8):** VocalPlayer ×6 (OnGameOver, ChangeDifficulty,
  GetSpotlightPhraseID, GetNextPhraseMarker, SetAutoplay,
  GetFreestyleDeploymentRequiredMs), Singer ×1 (GetFrameMatchType), Player ×1
  (SetEnergy).
- **Medium (needs refill pass / reveal):** Player.cpp + VocalPlayer.cpp anon-0% pools
  contain accessor-shaped target fns now blocked ONLY by the +4 (e.g. fn_82684418
  36B `lwz 0x260(r3); lwz 0x88; cmplwi; …` — though this one also has the §6.1
  compare residue), Task 0% pool, GamePanel UpdateNowBar, HamSongData::CalcSongPos.
  Run `tools/refill_loop.sh` after landing. Realistic total **+8…+13**.
- **Gated partials (do NOT count, see §6):** GetBandTrack 93.9→~99,
  Rollback@Player, SetMultiplierActive 98.5→~99.7, GetEnabledStateAt 95.5→~99,
  Rollback@VocalPlayer 89.9→~97, SetQuarantined.

### 5.5 Regression risk: LOW

Every plausible falsifier was checked against the live objects:
- All 16 currently-matched `default/Task` fns are guard-bit/EH thunks (verified
  instruction-by-instruction from `build/45410914/asm/Task.s`) — none touch TaskMgr.
- All matched Stats fns are Stats-internal (Stats layout identical, 0x200 both).
- MasterAudio/Song/TrackPanelDir hits are false positives (§5.2).
- No compiled mPhrase user; no compiled 6-arg ctor besides HamSongData.cpp:52.
- The classes embedding SongPos that have currently-matched fns reading
  post-SongPos members: none found.

## 6. Independent residues unlocked-adjacent (separate micro-fixes, optional same campaign)

1. **`cmplwi` vs `cmpwi` on `mUser->GetTrack()` null-test** — blocks GetBandTrack /
   Rollback@Player / SetMultiplierActive (+ anon clones like fn_82684418) from
   reaching 100 even after the layout fix. Retail tests the `Track*` unsigned
   (`cmplwi`); ours emits signed `cmpwi` despite `BandUser.h:74
   Track *GetTrack() const { return mTrack; }`. Suspect a type/conversion nuance in
   our port of `Player::GetBandTrack` (`src/band3/game/Player.cpp:976-981`).
   Cheap experiments for the implementer: `Track *t = mUser->GetTrack(); if (t != NULL)`,
   or compare how rb3-Wii's MWCC body is phrased; also check whether `Track` is
   fully-declared at that point in OUR include order. Fixing this one body likely
   converts 3 named fns + several anons (it inlines everywhere).
2. **GetEnabledStateAt compare shape**: retail emits two equality compares
   (`==2 beq; ==3 bne`), ours a subtractive range-check — re-phrase the condition in
   `src/band3/game/Player.cpp` (e.g. `if (s == kPlayerBeingSaved || s == kPlayerDroppingIn)`
   split into explicit branches) and A/B.
3. **Parked side-observations (recorded here so nobody re-derives them):**
   - **Band head +4**: retail Player ctor reads `Band::mCommonPhraseCapturer` at
     band+0x90; our compiled Player ctor reads band+**0x94**. `Band : public
     Hmx::Object` (Band.h:12), no SongPos member — a *different* 4-byte excess in
     Band's head (likely another DC3-vs-RB3 delta in Band.h's first members or in a
     member type). Band.cpp is NOT compiled, but compiled TUs inlining Band accessors
     (Player.cpp:406 `mBand->GetActivePlayers()`, :518 `EnergyMultiplier()`, :554)
     stay −4 vs retail until found. Worth its own mini-recon.
   - **Game head +4**: retail reads `TheGame->mProperties.mEnableOverdrive` at
     Game+0x3d; ours bakes Game+**0x41** (Player ctor `lbz r11, 0x41(r11)`).
     `Game.h:205 mProperties // 0x24` sits BEFORE `mSongPos // 0x40` → NOT explained
     by SongPos; Game's base/head carries its own +4. Game.cpp not compiled, but
     `TheGame->mProperties.*` readers (GemTrack.cpp:386/514/611/639,
     GamePanel.cpp:243/262/332/338, VocalPlayer.cpp:1898 `SongSectionOnly`,
     Player.cpp:73/208/822) keep that residue. Separate recon target — possibly the
     same class of DC3 delta in whatever sits in Game's first 0x24 bytes
     (`Game : public Hmx::Object + MsgSource?` — check `??_8Game@@…` in Ghidra).
   - VocalPlayer-own members confirmed shifted as a block (0x384…0x3d0-ours family);
     Singer reads `VocalPlayer+0x390` (retail) = mVocalParts.

## 7. A/B campaign plan (step-by-step, cold-startable)

1. **Worktree:** `scripts/setup_worktree.sh ~/wt/songpos-fix songpos-fix`
   (auto-reflinks orig/, private warm build/45410914/, copies
   global_fuzzy_pairs.json / unified_id_rb3wii.json / struct_db.sqlite).
2. **Baseline:** in the worktree run
   `rm -f build/45410914/target_symbol_renames.stamp && touch config/45410914/config.yml && NINJA_JOBS=12 tools/fresh_report.sh 2>&1 | tee /tmp/rb3_build_songpos_base.log`
   — expect `measures.matched_functions == 6932`. (Main's on-disk report is one
   commit stale: Player.obj predates `8bb12f0`'s SetEnergy source — the baseline
   build will recompile it; SetEnergy should read ~99.93 pre-edit.)
3. **Edit** the 2 files per §4 (SongPos.h + HamSongData.cpp:52). Nothing else.
4. **A/B:** same fresh_report command, tee to `/tmp/rb3_build_songpos_ab.log`.
   **Judge ONLY `build/45410914/report.json` `measures.matched_functions`** (never
   diff_inspect --diagnose headline %, never bare objdiff-cli strict output — [sym]
   reloc-name mismatches are ignored by the authoritative metric).
5. **Per-unit delta audit** vs baseline for the §5.3 audit list + default/Task +
   default/HamSongData + default/MasterAudio (expected: no unit down; Player/VocalPlayer/Singer up).
   Quick check: dump per-unit `measures.matched_functions` from both report.jsons and diff.
6. **Refill:** `NINJA_JOBS=12 tools/refill_loop.sh --map global_fuzzy_pairs.json`
   (playbook §9) to harvest newly byte-exact anon fns (Player/VocalPlayer/Task pools).
7. **Verify the marquee fns individually** (objdiff, no --build needed post-build):
   SetEnergy, GetFrameMatchType, the six VocalPlayer fns → expect 100.
8. Land via the usual rebase+ff-only flow; commit message should cite this dossier;
   then fresh-verify on main and fold the verdict into the roadmap addendum.
9. **Optional same-campaign micro-fixes:** §6.1 (cmplwi) then §6.2, each as its own
   A/B'd commit. If either nets negative, drop it — they're independent of the lever.
10. **Revert rule:** if the A/B nets < +4 or any audited unit regresses, capture the
    per-unit diff, revert, and record in the roadmap (CameraShot `fe0aaaa` precedent).

## 8. Confidence & falsifiers

**Confidence: HIGH (≈0.9)** that retail SongPos is 0x14/no-mPhrase and the edit
removes the +4 across the Performer/Player/VocalPlayer chain. Five independent proof
legs: (1) retail Performer ctor zero-store pattern incl. the skipped mTotalBeat
quirk matching the Wii ctor exactly; (2) retail mQuarantined @0x230 vs our compiled
0x234 with mScore @0x218 equal on both sides; (3) retail Player ctor: MsgSource
subobject 0x240 vs ours 0x244 and vbase 0x300 vs 0x304; (4) 30+ uniform +4
instruction pairs across 13 named near-misses in 3 units; (5) rb3-Wii header
agreement (and the rb3-Wii-header-wrong-for-retail precedent (OvershellSlot) is
covered by legs 1-4 being retail-machine-code-only).

Falsifiers / what would change the verdict:
- A currently-100% fn anywhere whose body reads a SongPos field at DC3 offsets
  (0xc/0x10/0x14) or a post-SongPos member at DC3 offsets. I searched the audit
  surface and found none; the whole-binary A/B is the final check.
- If after the edit the Player chain still shows ±4 on some members: re-open the
  possibility of a SECOND compensating delta (none observed — every observed Δ in
  these units is exactly +4 with a single onset).
- If `default/HamSongData`/`default/HamMaster` regress: re-examine the 5-arg edit
  (retail might phrase CalcSongPos differently) — those pins were game-splits
  derived, treat with the usual mis-pin suspicion.

## Appendix: artifacts & repro commands

- Diff JSONs used for the tables: `/tmp/claude/player_research/*.json`
  (regenerable with the §2.4 command; symbols/units embedded above).
- Retail fingerprint index: `fingerprints.json` (repo root, gitignored) — the
  Player-ctor hunt: callees ∋ `826844E0`.
- Ghidra MCP: `http://127.0.0.1:8002/mcp`, binary `/default.xex-35adb6`;
  helpers `tools/ghidra/ghidra-decompile.py <addr>`.
- Target asm: `build/45410914/asm/band3/game/{Player,VocalPlayer,Singer}.s`,
  `build/45410914/asm/Task.s`. Our objs: `build/45410914/src/band3/game/*.obj`
  (self-diff trick in §2.1 disassembles without a rebuild).
- Pinned ranges: `config/45410914/splits.txt:2390` (Player 0x826843F0–0x82686C50),
  `:2398` (VocalPlayer 0x826C6328–0x826C8C44), Singer 0x826D8B98–0x826D9F10,
  Stats `:2426`.
- Key retail addresses: Player ctor **0x82688E40**, Performer ctor **0x8267F0F0**,
  Stats ctor 0x8267AC88, Hmx::Object ctor 0x82737FE8, MsgSource ctor 0x827432A0,
  PlayerParams ctor 0x826844E0 (pinned+matched), SetVocals 0x826847B0
  (pinned+matched), CrowdRating ctor 0x826CFF40 (alloc 0x44).
