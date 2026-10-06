# W16-QH: the static-initializer region at 0x82C3F000+ (2026-10-06)

Lane W16-QH. Brief: identify the 60 `??__E` rows at `0x82C3F000`+ listed in
category 6 of `W16QB_DC3_REFERENCE_AUDIT_2026-10-06.md` (42 rows / 3,384 B in
core scope), attribute each to the source file that owns it, and make them
pair. Worktree `~/tmp/wt-w16qh`, branch `w16-qh`, based on main `377f357a6`.

## 1. Result

| | rows | B |
|---|---:|---:|
| core (IN-CORE) rows paired at fuzzy 100 | **35 / 42** | **1,624 / 3,384** |
| core rows attributed, not pairable (no source, or Quazal) | 7 | 1,760 |
| VIA-DC3 rows paired | 2 | 64 |
| OUT-360-OTHER (Quazal, `Voice` span) | 16 attributed to Quazal, not funded | 1,060 |
| unlisted rows in the same spans re-homed to their owner (were already 100 by byte signature, now by name) | 7 | 400 |

**Measured** with `tools/ab_measure.py --pick 2a887c028` from its parent
(`name_check` ruler, both legs at a split fixed point, run
`.ab_measure_runs/20261006-122809-2a887c028-2573178`):

| | leg A | leg B | Δ |
|---|---:|---:|---:|
| `matched_functions` | 53,577 | 53,602 | **+25** |
| `matched_code` | 5,951,200 | 5,952,888 | **+1,688 B** |
| `matched_code_percent` | 58.072998 | 58.089470 | +0.016472 pp |
| `fuzzy_match_percent` | 63.734640 | 63.749190 | +0.014550 pp |
| `masked_equal_functions` | 25,201 | 25,182 | −19 |
| units at 100% (`mpn`) | 567 | 569 | +2 (FilePath by denominator, Output new) |

Predicted from a same-worktree build before the A/B: +25 / +1,688 B, every
re-homed row at fuzzy 100, no other row in the region moving. All held.
`masked_equal` falls by 19 because rows that were paired by byte signature
are now paired by name.

**Rows that went down: one.** Row-level diff of the two archived leg reports
(every `(unit, symbol)` present on both sides, fuzzy or `mpn` lower):
`ContextChecker fn_823F4338`, 16 B, 70 → 0. It is not an initializer: it is a
Quazal `_DO_MessageBroker` dtor thunk (stores that vtable, tail-calls the base
dtor) mis-pinned into ContextChecker's `0x823F4268` block, and its 70 was a
byte-signature pairing against ContextChecker's 16 B `gContextRand`
initializer, which now pairs with its own row (`0x82C3FED0`). It was never
matched, and it reads 0 under any correct attribution. Kept; it is the cost
of pairing `gContextRand`. The "unit REGRESSIONS" in the A/B output (DataNode
−7, UsbMidiGuitar −4, BlockMgr −2, FilePath −2, PropKeys −1) are rows leaving
those units, not rows scoring lower: 44 rows vanished from their old units and
the same 44 appeared in their owners.

Native gate (`tools/native_build_gate.sh` in the worktree, on the lane commit):

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

## 2. Method

The region holds every TU's dynamic initializers, emitted in link order, and
each TU's initializers in declaration order. W16-QB found DC3's names here
false (a masked hash matches any same-shaped initializer), so DC3 was not used.
Each row was attributed by two witnesses that share no arithmetic:

1. **Link order.** Our objs' `??__E` symbols, ordered by their unit's largest
   pinned `.text` block, form a predicted sequence; the retail sequence aligns
   with it. Example: File (`gFiles`, `gDirList`) → FileCache (`gCaches`) →
   UsbMidiGuitar (`gCritSection`, `gQueue`, `mTimer`) → ContentMgr_Xbox →
   … → Joypad → VirtualKeyboard → JoypadClient → Joypad_Xbox → Memcard_Xbox →
   BlockMgr → Joypad_Xinput → CDReader ×2 → HDCache → BandUI → ProfileMgr →
   UIStats → (AuditionMgr) → ContextChecker ×3 → BandSongMgr → PresenceMgr.
   The ctor addresses climb monotonically the same way (BandUI 0x82536…,
   ProfileMgr 0x82548…, UIStats 0x8255F…, BandSongMgr 0x8257A…).
2. **Relocation targets.** The global each initializer constructs (its `addi`
   operand, plus the `??__F` stub's), and which unit's main `.text` reads it.
   The ctor, vtable (RTTI type name) and string the initializer reaches
   identify the type; the readers identify the TU.

The name given to each row is the owning obj's own `??__E` symbol, read from
the built obj after the patchers, with its size checked against the retail
body (all 44 equal). Data and `??__F` relocations stay placeholder names, which
`name_check` forgives, so each re-homed row is byte-equal by name.

Blocks: each re-homed `.text` block is `[row, next function)`, so the trailing
padding moves with it; an 8-byte EH prefix of the next function was checked
from retail bytes (none of the moved blocks border one). Rows not re-homed
stay in their old unit, so the old spans become multi-block rather than
moving unrelated rows. `.pdata` was re-derived by the split.

### 2.1 Source added

- `src/system/synth/Faders.cpp`: `static std::vector<Fader *> sFaderList;`
  before `FaderTask::sTasks`. Retail's initializer for it is
  `0x82C3FFC0` (a 12 B `atexit` of a `MemOrPoolFreeSTL` vector dtor),
  directly ahead of `sTasks`' at `0x82C3FFD0`, and no retail code reads it.
  DC3's copy dropped it.
- `src/system/beatmatch/Output.cpp` (new TU, `objects.json` + a new splits
  heading): `LogFile TheBeatMatchOutput("beatmatch-%05d.rec");`, retail
  `0x82C40D68` (string at `0x8210F6D0`). DC3's map calls this address
  `BinkInit`. Its readers are BeatMatcher, TrackWatcherImpl, BeatMaster,
  RealGuitarTrackWatcherImpl and RGGemMatcher. The native build lists its
  sources explicitly and keeps its own `TheBeatMatchOutput` shim
  (`native/src/beatmatch_native_support.cpp`), so it does not compile this file.

## 3. Rows

"Witness" is the unit(s) whose main `.text` reads the initialized global
(function count in parentheses); it is empty where the type alone is unique
(a ctor plus a vtable). Unit names there are the *pinned* units, so they carry
known mis-pins: `0x82C3F900`'s global is read by four functions pinned to
UsbMidiGuitar at `0x8251A138`, which sit below UsbMidiGuitar's own block
(`0x8251A2E0`) at the tail of FileCache.

The column reads any reference within 0x10 bytes of the global, so a
neighbouring global's readers can appear (`0x82C3FC70`'s BlockMgr hits are
`TheHDCache` at +0xC). For manager singletons (`TheBandUI`, `TheProfileMgr`,
`TheVirtualKeyboard`, `TheHDCache`, `TheDxRnd`, `TheTaskMgr`) the readers
are spread across the game, so the column only shows the global is live:
the owner there is the TU whose obj defines that singleton with the same
ctor/dtor, at the matching link position.

| row | B | tier | was (unit, fuzzy) | owner | witness: users of the global (main .text) | now |
|---|---:|---|---|---|---|---|
| `fn_82C3F080` | 12 | VIA-DC3 | PropKeys 98.3 | PropAnim.cpp | PropAnim (3), auto_03_82429C2C_text (1) | `??__EsKeyReplace@@YAXXZ` 100.0 |
| `fn_82C3F8F0` | 12 | IN-CORE | system/os/UsbMidiGuitar 98.3 | File.cpp | File (3) | `??__EgDirList@@YAXXZ` 100.0 |
| `fn_82C3F900` | 84 | IN-CORE | system/os/UsbMidiGuitar 0.0 | FileCache.cpp | FileCache (4), system/os/UsbMidiGuitar (4) | `??__EgCaches@@YAXXZ` 100.0 |
| `fn_82C3F9D8` | 72 | IN-CORE | system/os/UsbMidiGuitar 0.0 | ContentMgr_Xbox.cpp | none outside the init (ctor/vtable unique) | `??__EgContentMgr@@YAXXZ` 100.0 |
| `fn_82C3FA20` | 12 | IN-CORE | system/os/UsbMidiGuitar 98.3 | StageKit TU (unpinned, `auto_03_825219A0`) | Timer used by the `stagekit_set_*` handlers at 0x82521ED0/0x82522608 | no surviving source; left in place |
| `fn_82C3FA30` | 40 | IN-CORE | system/os/UsbMidiGuitar 0.0 | StageKit TU (unpinned, `auto_03_825219A0`) | 64-byte array zeroed; used at 0x82521B30/0x82521ED0 | no source; left in place |
| `fn_82C3FA60` | 52 | IN-CORE | system/os/UsbMidiGuitar 0.0 | Joypad.cpp | registers atexit for a constant-initialized `ObjPtr<Hmx::Object>` at .data 0x82C71AF0, 12 B ahead of `gKeepaliveThresholdMs` (0x82C71AFC) | variable unnamed in any surviving source and unused; left in place |
| `fn_82C3FA94` | 40 | IN-CORE | system/os/UsbMidiGuitar 78.5 | Joypad.cpp | EH unwind funclet of the row above (`~ObjRefOwner` on 0x82C71AF0) | as above |
| `fn_82C3FAC0` | 56 | IN-CORE | system/os/UsbMidiGuitar 0.0 | Joypad.cpp | Joypad (23), OnlineID (2) | `??__EgJoypadData@?A0x47b254f1@@YAXXZ` 100.0 |
| `fn_82C3FAF8` | 52 | IN-CORE | system/os/UsbMidiGuitar 99.6 | VirtualKeyboard.cpp | System (3) | `??__ETheVirtualKeyboard@@YAXXZ` 100.0 |
| `fn_82C3FB30` | 12 | IN-CORE | system/os/UsbMidiGuitar 98.3 | JoypadClient.cpp | JoypadClient (3), Joypad_Xbox (1) | `??__EgClients@?A0xc08169d8@@YAXXZ` 100.0 |
| `fn_82C3FC28` | 52 | IN-CORE | BlockMgr 0.0 | Joypad_Xinput.cpp | Joypad_Xinput (1) | `??__EgCritSection@?A0x2c42976e@@YAXXZ` 100.0 |
| `fn_82C3FC60` | 12 | IN-CORE | BlockMgr 98.3 | CDReader.cpp | CDReader (4) | `??__EgArkFiles@?A0x7f36a62b@@YAXXZ` 100.0 |
| `fn_82C3FC70` | 12 | IN-CORE | BlockMgr 98.3 | CDReader.cpp | BlockMgr (5), CDReader (2) | `??__EgExternalArkFiles@?A0x7f36a62b@@YAXXZ` 100.0 |
| `fn_82C3FC80` | 52 | IN-CORE | BlockMgr 0.0 | HDCache.cpp | BlockMgr (5) | `??__ETheHDCache@@YAXXZ` 100.0 |
| `fn_82C3FCB8` | 56 | IN-CORE | BlockMgr 0.0 | BandUI.cpp | WaitingUserGate (5), band3/meta_band/BandScreen (5) | `??__ETheBandUI@@YAXXZ` 100.0 |
| `fn_82C3FCF0` | 56 | IN-CORE | BlockMgr 0.0 | ProfileMgr.cpp | band3/meta_band/AccomplishmentManager (26), MetaPerformer (14) | `??__ETheProfileMgr@@YAXXZ` 100.0 |
| `fn_82C3FD28` | 52 | IN-CORE | BlockMgr 0.0 | band3/meta_band/UIStats.cpp | none outside the init (ctor/vtable unique) | `??__EgUIStats@@YAXXZ` 100.0 |
| `fn_82C3FD60` | 172 | IN-CORE | BlockMgr 0.0 | AuditionMgr (TU5-only) | `Hmx::Object` ctor + AuditionMgr vtables; position between UIStats and ContextChecker | no surviving source (see `AuditionMgr.h`); left in place |
| `fn_82C3FE10` | 132 | IN-CORE | BlockMgr 0.0 | ContextChecker.cpp | ContextChecker (3) | `??__EgUsedContexts@?A0x1e5d0754@@YAXXZ` 100.0 |
| `fn_82C3FE98` | 52 | IN-CORE | BlockMgr 0.0 | ContextChecker.cpp | none outside the init (ctor/vtable unique) | `??__EgStoredContext@?A0x1e5d0754@@YAXXZ` 100.0 |
| `fn_82C3FED0` | 16 | IN-CORE | BlockMgr 0.0 | ContextChecker.cpp | ContextChecker (3) | `??__EgContextRand@?A0x1e5d0754@@YAXXZ` 100.0 |
| `fn_82C3FEE0` | 56 | IN-CORE | BlockMgr 0.0 | BandSongMgr.cpp | none outside the init (ctor/vtable unique) | `??__EgSongMgr@@YAXXZ` 100.0 |
| `fn_82C3FFB0` | 12 | IN-CORE | DataNode 98.3 | system/synth/Synth.cpp | Sfx (1), system/synth/SynthSample (1) | `??__EgDebugGraphs@?A0x07ff4b01@@YAXXZ` 100.0 |
| `fn_82C3FFC0` | 12 | IN-CORE | DataNode 98.3 | Faders.cpp | system/synth/MoggClip (1) | `??__EsFaderList@@YAXXZ` 100.0 |
| `fn_82C40080` | 20 | IN-CORE | DataNode 0.0 | MicNull.cpp | MicNull (1) | `??__EsRand@@YAXXZ` 100.0 |
| `fn_82C40098` | 1372 | IN-CORE | DataNode 0.0 | UGC song-data validator (unpinned, `auto_03_8272BAC8`) | 1372 B of `Symbol()` default inits (stores of 0x82C71838) into .data 0x82C76408, read only by the `"Validating Song Data (songs.dta)..."` function 0x8272C8E8 | no source; left in place |
| `fn_82C405F8` | 52 | IN-CORE | DataNode 0.0 | system/rnddx9/Rnd.cpp | system/rnddx9/ShaderMgr (24), Rnd_Xbox (18) | `??__ETheDxRnd@@YAXXZ` 100.0 |
| `fn_82C40630` | 72 | IN-CORE | DataNode 0.0 | system/rnddx9/ShaderMgr.cpp | Rnd_Xbox (1) | `??__ETheDxShaderMgr@@YAXXZ` 100.0 |
| `fn_82C40678` | 12 | IN-CORE | DataNode 98.3 | system/movie/Movie.cpp | system/movie/Movie (3), TexMovie (1) | `??__E?sActiveMovies@Impl@Movie@@2V?$vector@PAVIm…` 100.0 |
| `fn_82C40688` | 52 | IN-CORE | DataNode 0.0 | system/movie/Movie.cpp | system/movie/Movie (2) | `??__EgMovieCrit@@YAXXZ` 100.0 |
| `fn_82C40718` | 52 | IN-CORE | DataNode 0.0 | Task.cpp | BandList (14), band3/game/Game (8) | `??__ETheTaskMgr@@YAXXZ` 100.0 |
| `fn_82C40898` | 76 | IN-CORE | DataNode 0.0 | system/obj/Dir.cpp | system/obj/Dir (2) | `??__EgPreloaded@?A0xf8b42a02@@YAXXZ` 100.0 |
| `fn_82C40A20` | 12 | IN-CORE | DataNode 98.3 | Object.cpp | BandLeadMeter (2), Object (1) | `??__EsRevStack@@YAXXZ` 100.0 |
| `fn_82C40BF8` | 52 | IN-CORE | FilePath 0.0 | DataFile.cpp | DataFile (3) | `??__EgDataReadCrit@@YAXXZ` 100.0 |
| `fn_82C40C30` | 20 | IN-CORE | FilePath 0.0 | DataFile.cpp | DataFile (6) | `??__EgFile@@YAXXZ` 100.0 |
| `fn_82C40C48` | 84 | IN-CORE | FilePath 0.0 | DataFile.cpp | DataFile (4) | `??__EgConditional@@YAXXZ` 100.0 |
| `fn_82C40CA0` | 132 | IN-CORE | FilePath 0.0 | DataFile.cpp | DataFile (2) | `??__EgReadFiles@@YAXXZ` 100.0 |
| `fn_82C40D28` | 12 | IN-CORE | FilePath 98.3 | SongParser.cpp | band3/game/Game (1) | `??__EgSongLoadTimer@@YAXXZ` 100.0 |
| `fn_82C40D38` | 44 | IN-CORE | FilePath 0.0 | system/beatmatch/Playback.cpp | BeatMatcher (9), system/beatmatch/BeatMaster (4) | `??__ETheBeatMatchPlayback@@YAXXZ` 100.0 |
| `fn_82C40D68` | 60 | IN-CORE | FilePath 0.0 | system/beatmatch/Output.cpp | BeatMatcher (8), TrackWatcherImpl (4) | `??__ETheBeatMatchOutput@@YAXXZ` 100.0 |
| `fn_82C40DA8` | 12 | IN-CORE | FilePath 98.3 | Achievements.cpp | Achievements (3), StoreEnumeration (1) | `??__E?gThreadAchievements@Achievements@@0V?$vect…` 100.0 |
| `fn_82C42910` | 72 | IN-CORE | System 0.0 | Quazal (users at 0x82B1CD70) | global 0x82E11C90 is read only from the Quazal block | Quazal: not funded |
| `fn_82C42958` | 68 | OUT-360-OTHER | Voice 0.0 | Quazal (Voice span) | read only from 0x82B1–0x82B5 | Quazal: not funded |
| `fn_82C429A0` | 72 | OUT-360-OTHER | Voice 0.0 | Quazal (Voice span) | read only from 0x82B1–0x82B5 | Quazal: not funded |
| `fn_82C429E8` | 52 | OUT-360-OTHER | Voice 99.2 | Quazal (Voice span) | read only from 0x82B1–0x82B5 | Quazal: not funded |
| `fn_82C42A20` | 72 | OUT-360-OTHER | Voice 0.0 | Quazal (Voice span) | read only from 0x82B1–0x82B5 | Quazal: not funded |
| `fn_82C42A68` | 96 | OUT-360-OTHER | Voice 0.0 | Quazal (Voice span) | read only from 0x82B1–0x82B5 | Quazal: not funded |
| `fn_82C42AC8` | 72 | OUT-360-OTHER | Voice 0.0 | Quazal (Voice span) | read only from 0x82B1–0x82B5 | Quazal: not funded |
| `fn_82C42B10` | 52 | OUT-360-OTHER | Voice 99.2 | Quazal (Voice span) | read only from 0x82B1–0x82B5 | Quazal: not funded |
| `fn_82C42B48` | 72 | OUT-360-OTHER | Voice 0.0 | Quazal (Voice span) | read only from 0x82B1–0x82B5 | Quazal: not funded |
| `fn_82C42B90` | 52 | OUT-360-OTHER | Voice 99.2 | Quazal (Voice span) | read only from 0x82B1–0x82B5 | Quazal: not funded |
| `fn_82C42BC8` | 72 | OUT-360-OTHER | Voice 0.0 | Quazal (Voice span) | read only from 0x82B1–0x82B5 | Quazal: not funded |
| `fn_82C42C10` | 60 | OUT-360-OTHER | Voice 0.0 | Quazal (Voice span) | read only from 0x82B1–0x82B5 | Quazal: not funded |
| `fn_82C42C50` | 64 | OUT-360-OTHER | Voice 0.0 | Quazal (Voice span) | read only from 0x82B1–0x82B5 | Quazal: not funded |
| `fn_82C42C98` | 92 | OUT-360-OTHER | Voice 0.0 | Quazal (Voice span) | read only from 0x82B1–0x82B5 | Quazal: not funded |
| `fn_82C42CF4` | 40 | OUT-360-OTHER | Voice 93.9 | Quazal (Voice span) | read only from 0x82B1–0x82B5 | Quazal: not funded |
| `fn_82C42D20` | 72 | OUT-360-OTHER | Voice 0.0 | Quazal (Voice span) | read only from 0x82B1–0x82B5 | Quazal: not funded |
| `fn_82C42D68` | 52 | OUT-360-OTHER | Voice 99.6 | Quazal (Voice span) | read only from 0x82B1–0x82B5 | Quazal: not funded |
| `fn_82C43780` | 52 | VIA-DC3 | SpotlightDrawer 0.0 | VelocityBuffer.cpp | PostProc_NG (4), Rnd_Xbox (2) | `??__E?sSingleton@RndVelocityBuffer@@0V1@A@@YAXXZ` 100.0 |

The seven unlisted rows re-homed alongside (they shared the spans and already
read 100 by byte signature, not by name): `0x82C3F958` UsbMidiGuitar
`gCritSection`, `0x82C3F9C8` UsbMidiGuitar `mTimer`, `0x82C3FB40` Joypad_Xbox
`tCritSection` (it was scoring 100 against UsbMidiGuitar's `gCritSection`),
`0x82C3FC18` BlockMgr `gReadTime`, `0x82C3FFD0` Faders `sTasks`, `0x82C40028`
Pollable `sPollables`, `0x82C406C0` Movie `gOpenMovies`. All read 100 by name
afterwards.

### 3.1 Why the seven core rows stay unpaired

- **StageKit** (`0x82C3FA20` Timer, `0x82C3FA30` 64-byte array): their
  globals are read only by an unpinned block at `0x825219A0` (between
  ContentMgr_Xbox and DateTime) that registers `stagekit_set_fog`,
  `set_stagekit_strobe`, … and calls `JoypadStageKitSetRaw`. No surviving
  source tree has this TU. Pairing them means writing it from nothing.
- **Joypad's ObjPtr** (`0x82C3FA60` + its funclet `0x82C3FA94`): a
  constant-initialized `ObjPtr<Hmx::Object>` at `.data 0x82C71AF0`,
  immediately before `gKeepaliveThresholdMs` (`0x82C71AFC`). The ctor folded
  into static data; the initializer only registers `atexit` inside an EH
  frame. No retail code reads it and no surviving source names it, so adding it
  would mean inventing a variable name; not done.
- **AuditionMgr** (`0x82C3FD60`, 172 B): `Hmx::Object` ctor plus two
  AuditionMgr vtables, positioned between UIStats and ContextChecker.
  TU5-only; no surviving source (`src/band3/meta_band/AuditionMgr.h` says so).
- **UGC song-data validator** (`0x82C40098`, 1,372 B): ~340 `Symbol()`
  default inits (stores of `0x82C71838`) into `.data 0x82C76408`, read only
  by `0x8272C8E8` (`"Validating Song Data (songs.dta)..."`,
  `ValidateRawSymbol(...)`), an unpinned TU after SampleData. No surviving
  source.
- **`0x82C42910`** (tiered IN-CORE under `System`): its global `0x82E11C90` is
  read only from `0x82B1CD70`, inside the Quazal block. Quazal, not funded.

These rows were left in the units they were in, so nothing they currently
score moves.

### 3.2 The `Voice` span (16 OUT-360-OTHER rows)

`0x82C42958`–`0x82C42D68`: every global is read only from `0x82B1…`–`0x82B5…`
(Quazal NetZ, including `JobConnectEndPoint`, `UDPTransport`, `Session`,
`Station`). Attributed to Quazal; not funded per the standing directive.

## 4. Not done

- No `??__F` (atexit dtor stub) rows at `0x82C45000`+ were touched. They
  follow the same link order and can be attributed the same way.
- The rows in DataNode's span that already read 100 by byte signature
  (`0x82C40750`–`0x82C40B20`, DataNode / DataArray / Dir / Utl / Object
  candidates) were not re-homed. They score 100 now, and moving them needs a
  per-row owner check against the stray DC3 copies our objs carry (DataNode.obj
  and DataArray.obj both define `?gFile@DataArray` and
  `gDataArrayConditional`).
- Several of our objs define `??__E` symbols retail does not have (DC3 statics
  pulled in through headers or misattributed sources: `ThreadCall_Win`
  `jobQueueMutex`, `PrefabMgr` `gSongMgr`/`gEntries`, `JoypadMsgs` Synth
  statics, `ContextChecker` `TheCharDebug`, `OnlineID` `gOverride` /
  `gJoypadData`, `Synth` `mPlayHandlers`/`unka8` and the copies of them in
  `Sfx`/`CheatProvider`; retail goes straight from Synth's `gDebugGraphs` to
  Faders). They do not affect
  pairing here; listed for a cleanup lane.
- No merge, no push (coordinator).

Scratch (not committed): `~/tmp/w16qh/` (`allfns.json`, `ours.json`,
`annot.py`, `match.py`, `predict.py`, `xref.py`, `assign.py`,
`edit_splits.py`, `rowdiff.py`).
