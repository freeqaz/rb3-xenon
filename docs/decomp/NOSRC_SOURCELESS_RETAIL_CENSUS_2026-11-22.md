# NOSRC: census of in-scope retail code that no function of ours pairs with (2026-11-22)

Lane NOSRC. Read-only census, branch `nosrc` in `~/tmp/wt-nosrc`, based on main `af50f101a`. No source, map,
splits or objects change. The brief asked for every piece of retail RB3 code that is game or engine (not XDK, CRT
or Quazal) and has no body in our tree that pairs with it. That means two populations:

- functions in unpinned `auto_*` runs, and
- retail rows inside pinned units that no function of ours pairs with (fuzzy 0, no base symbol of that name).

For each run the census gives its address range, its size, what it is (from strings, RTTI, vtables and callers),
whether an oracle body exists in `../rb3` or `../dc3-decomp`, and whether boot-to-song can reach it. Ranking is by
reachability first, then size. A run whose body **is** in our source but whose retail row still pairs with nothing
is listed separately (§4), because the work it needs is a pin, a name or a re-home rather than writing code.

## 1. Result

**Prediction, stated before the census.** The truly sourceless in-scope code would be the TU5 audition / RBN
cluster that W16-QP found (about 46 KB), plus a few KB of scattered holes. Most of `auto_*` would be XDK, Quazal
and vendor code.

**Measured.** The prediction held, but the audition cluster is larger than QP's 46 KB once its runs are joined
and its satellite rows in pinned units are counted: **~63 KB**. It is nearly all of the sourceless in-scope code.

| population | runs | rows | bytes |
|---|---:|---:|---:|
| all `auto_*` code (`.text` + `BINK`) | 468 | 7,870 | 1,442,796 |
| ... XDK / CRT (both neighbours `xdk/`, or one neighbour plus MS evidence) | 282 | 2,834 | 708,588 |
| ... Quazal (address band `0x82A6D168`–`0x82B54190`) | 101 | 3,540 | 531,084 |
| ... RAD Bink (section `BINK`) | 1 | 141 | 65,292 |
| ... static-init / atexit thunks and import thunks (`0x82C3E5C0`–`0x82C4CE3C`) | 42 | 583 | 20,712 |
| ... vendor (liboggz, zlib copies) | 5 | 119 | 18,092 |
| ... HMX network layer on Quazal (`network/` neighbours, DDL clients) | 6 | 153 | 12,508 |
| ... EH funclets / a data word carved as code | 3 | 26 | 1,028 |
| **... sourceless HMX game/engine (§3)** | **9** | **397** | **69,736** |
| **... HMX game/engine whose body we hold (§4)** | **19** | **77** | **15,756** |
| in-scope pinned units, rows at fuzzy 0 (§5, appendix) | — | 133 | 7,780 |
| HMX bodies folded by ICF into XDK territory (§4.2) | — | 7 | 56 |

(The XDK line is QP's spatial class `XDK2` 634,392 B plus 13 one-sided runs reviewed by hand, 74,196 B. The nine
sub-rows add up to all `auto_*` code: 1,442,796 B.)

**Sourceless in-scope code, ranked (§3 has the evidence for each row):**

| # | range | size | what | our tree | oracle | boot-to-song |
|---:|---|---:|---|---|---|---|
| 1 | `0x82560B08`–`0x82564408` (2 runs) + 2 pinned slivers | 14,284 + 272 | **AuditionMgr** | `AuditionMgr.h` only; branch `audmgr` in progress | none | **boot-constructed** (ctor/Init from `App`, R1); body runs only in audition mode |
| 2 | `0x82650B78`–`0x82651924` | 3,404 | **BandNetGameData** (+ `NetGameData`, `RemovingRemoteUserMsg`) | header only | rb3: header only; dc3: none | **boot-constructed** (SessionMgr, R1); used by online sessions |
| 3 | `0x8272BAC8`–`0x82732A94` | 28,496 | **MidiValidator** / UGC song validator | none | none | off path (called only by AuditionMgr) |
| 4 | `0x82B8C8C8`–`0x82B8FC88` (2 runs) + pinned slivers | 12,836 + 672 | **UGCNet** (connection, discovery, TCP/UDP sockets) | none | none | off path (AuditionMgr only) |
| 5 | `0x825EAC38`–`0x825EC4F8` | 6,224 | **AuditionSessionBuilder** | declared in `AuditionSessionPanel.h` | none | off path (audition) |
| 6 | 5 pinned rows (BandSongMgr, BandUserMgr, ChunkStream) | 832 | TU5 audition helpers on existing classes | none | none | off path (audition callers only) |
| 7 | `0x82B68D40`–`0x82B6963C` | 2,288 | `FxSendPitchShift360` / `FxSendSynapse360` (360-ONLY) | 7–8-line stubs | dc3: same stubs | effect-dependent; 360-only tier |
| 8 | `0x8266B4C0`–`0x8266BDC8` | 2,204 | `PlayerCampaign{Goal,Career}Leaderboard` virtuals, `AssetOffer` dtor | headers only | **rb3: `CampaignLeaderboards.cpp`, `AssetOffer.cpp`** | off path (leaderboard / store UI) |
| 9 | `0x82B69868` (one row in an FxSend360 run) | 332 | `FxSend360::Cleanup` (360-ONLY) | none (W16-UK §5) | none | on the synth path; 360-only tier |

So the sourceless in-scope code is **~70 KB** (69,736 B in 9 `auto_*` runs, plus 1,776 B of pinned slivers and
`FxSend360::Cleanup` 332 B, which is counted inside its §4 run), **and ~63 KB of it is the RBN audition feature** (rows 1, 3, 4, 5, 6).
Outside audition, the only sourceless code is `BandNetGameData` (3.4 KB, header-only in every tree), the campaign
leaderboard / AssetOffer bodies (2.2 KB, which have an rb3 oracle) and the 360-only FxSend stubs.

Only two sourceless items lie on the boot path, AuditionMgr and BandNetGameData. Both are constructed at boot and do
their real work outside an offline song, so a native boot needs a ctor/Init (or a stub) for each and nothing more.
**No sourceless code lies on the song path itself.** Every on-song-path hole in §4 is a body we already hold.

## 2. Method

All inputs are read-only from main's build, `build/45410914/` (`report.json` 2026-10-07 23:09). **No
`splits.txt`, `objects.json` or `target_symbol_map.json` commit has landed since then** (`git log --since` on those
three is empty), so the unit layout is current. Source edits since then could change the pairing of a handful of rows
in §5.

Scratch scripts are in `~/tmp/nosrc/` and are not committed. Each one is short:

- **`index.py`** reads every `.s` under `build/45410914/asm`. It keys each function on its `.fn` symbol: a
  `fn_<addr>` name, or a named symbol resolved through `config/45410914/symbols.txt`. It never keys on the `# .text`
  address column, which is synthetic for multi-block units. A first version keyed on that column and lost 1,465
  functions, among them every pinned `App` row, so reachability collapsed to 99 functions. Fixing it brought the index
  to 68,839 functions (`report.json` has 68,908 rows). The script also records every code→symbol reference (`bl`,
  `b`, `lis`/`addi` address loads, named callees) and every pointer inside data objects.
- **`gaps.py` / `classify.py`**: one `auto_*` unit is one contiguous unpinned run. Each run is classified by section
  (`BINK`), by the Quazal address band, and by its nearest pinned neighbours on each side (`xdk/` on both sides
  gives `XDK2`; on one side gives `XDK1`; otherwise `OPEN`). The both-sides rule is W16-QP's spatial rule, which its
  control measured right on 383 of 388 pinned blocks (98.7%, 99.91% of bytes). W16-QP's per-row classes
  (`~/tmp/w16qp/final_class.json`) are carried along as a cross-check.
- **`evidence.py` / `vtslot.py`** read the evidence from retail bytes (`orig/45410914/band.exe`):
  - ASCII and UTF-16 strings that the rows load;
  - RTTI for every vtable a row stores (vtable−4 → COL → TypeDescriptor+8);
  - for a row reached only through data, a byte scan of `.rdata` for its address, a walk back to the COL, and the
    slot index with its named neighbour slots. This is how `0x82641670` became `TokenRedemptionPanel` slot 7
    (`Poll`) and `0x82655788` became `PerformanceData` slot 1 (`SaveFixed`, next to `LoadFixed`);
  - callers and callees by unit.

  All 85 `OPEN` and 12 `XDK1` runs were then read by hand. The labels in §3, §4 and §6 are that reading.
- **`runstats.py`** has two parts:
  - an **EH-funclet test**: no code caller, and the only `.rdata` slot holding the row is preceded by a state word
    (`0xFFFFFFFF` or a small int) inside an unwind map, with no COL. `0x8242A508` checks out by hand: the unwind
    map ends in the FuncInfo magic `0x19930522`;
  - the **reach** instrument (below).
- **`pinned.py`** reads every non-`auto_*` `report.json` row with fuzzy 0 and resolves its address (from its
  `fn_` name, or by inverting the map within the unit). It tiers the row with `tools/native_runtime_rank.py`
  `tier_fn`. The in-scope tiers are IN-CORE, IN-SOON, IN-RB3ENG and VIA-DC3. 360-ONLY and the sourceless `xdk/`
  pins are reported only as totals.
- **`xdkhmx.py`** is the second detector. It finds every row in XDK territory (`XDK1`/`XDK2` runs and pinned `xdk/`
  units) that an in-scope pinned HMX unit references. This is how the ICF-folded HMX getters are caught (§4.2).

**Reach.** This is a static call graph from the PE entry point (`0x8283CD20`):

| tier | edges followed |
|---|---|
| **R1** | direct calls and code address loads only |
| **R2** | R1, plus every pointer in any data object a reached function references (vtables, handler tables, the CRT `XCU` table) |
| **XCU** | reachable only through the static-initializer table |
| **none** | none of the above |

R2 over-approximates: it follows every vtable a constructor installs. It also under-approximates: it misses dispatch
through data that only other data points to.

- **Controls.** These are all reached: `App::App` R1, `App::Run` R1, `GameInit` R1, `SongMgr::Init` R2,
  `BandSongMgr::Init` R2, `Game::Game` R2, `Game::Start` R2, `Game::Poll` R2, `BeatMaster::Poll` R2,
  `GemPlayer::Poll` R2, and both `SongData::Load` overloads R2. That is 12 of 12.
- **Discrimination.** Over the 53,852 pinned HMX `.text` rows (non-`xdk/`, non-`network/`), R1 is 4,044, R2 22,143,
  XCU 89 and none 27,576, so about half are not reached. The instrument separates something, but **"none" is not
  proof of dead code**. `GemTrackDir::UpdateFingerFeedback`, which runs every frame on a guitar track, reads "none"
  because its only caller is reached through a path the graph does not model.

The boot-to-song column in the tables is therefore **a judgement that cites the static tier**, not the tier alone:

| judgement | meaning |
|---|---|
| **on path** | runs during boot or in a normal offline song |
| **boot-constructed** | its ctor or Init runs at boot; its feature body does not |
| **off path** | runs only in a menu, a mode or online features off boot→song |

## 3. Sourceless HMX runs (no body anywhere in our tree)

### 3.1 The audition / RBN cluster (~63 KB): everything here is TU5-only

| range | rows / B | evidence | reach |
|---|---|---|---|
| `0x82560B08`–`0x82563250` | 95 / 9,892 (60 rows / 2,252 B of them are its own EH funclets) | RTTI `AuditionMgr`, `ThreadCallback`. Strings `audition_mgr`, `mogg_encryption`, `license_bits`, `on_validation_success`, `An Xbox Without A Signed In Gamer Profile`. Called by `App` (1 site) | R1 (ctor from `App`) |
| `0x825632BC`–`0x82564408` | 35 / 4,392 | AuditionMgr's `Handle` side: `can_enter_audition`, `enter_audition`, `get_download_progress`, `has_validation_failed`, `get_venue_sym`. RTTI `SigninChangedMsg`, `ServerStatusChangedMsg`, `ConnectionStatusChangedMsg` | R2 |
| `0x82563258` (MetaPerformer pin) | 1 / 100 | `SendDataPoint<const char*, int>`, called only from the run above | R2 |
| `0x82C3FD60` (BlockMgr pin) | 1 / 172 | AuditionMgr's static initializer; branch `audmgr` re-homes it (`15f3872df`) | R2 |
| `0x8272BAC8`–`0x82732A94` | 45 / 28,496 | RTTI `MidiValidator`. UGC asserts (`kUGCPanMin`, `kUGCTuningOffsetCentsMin`, `kUGCSongPreviewLengthMs`, `!HasUnnecessaryWhitespace`). Callers: AuditionMgr only. Calls `ChunkStream` pin `0x827CA180` | R2 |
| `0x82B8C8C8`–`0x82B8E788` | 66 / 7,704 | RTTI `UGCNet`, `UGCNet_Server`, `UGCNetConnection[_Server]`. Strings `ProcessSongPartComplete`, `Hash Failure: Part %d`, `Discovery Failed.` Callers: AuditionMgr | R2 |
| `0x82B8E7E8`–`0x82B8FC88` | 47 / 5,132 | RTTI `UGCNetDiscovery[_Server]`, `UGCNetInterface`, `UGCNetResource`, `UGCNet{TCPListen,TCPSession,UDP}Socket` | R2 |
| `0x82B8E790` (DataFunc pin), `0x82B8FC88`–`0x82B8FE30` (6 ProfileMgr pin rows) | 7 / 672 | at fuzzy 0, between and right after the UGCNet runs, called by them. These are **probably UGCNet code under the wrong pins** (DataFunc and ProfileMgr cannot define them). Not adjudicated | R2 |
| `0x825EAC38`–`0x825EC4F8` | 49 / 6,224 | RTTI `AuditionSessionBuilder`, `AddLocalUserResultMsg`, `LocalUserLeftMsg`. Song-path format strings `%s/%s/gen/%s.milo_xbox`, `%s/%s/%s.mogg`, `%s/songs.dta`. Callers: `AuditionSessionPanel`, AuditionMgr | R2 |
| BandSongMgr `0x82577480` 252, `0x82576B70` 120; BandUserMgr `0x82682D40` 192, `0x826833B0` 168; ChunkStream `0x827CA180` 100 | 5 / 832 | pinned rows at fuzzy 0, and every caller is one of the runs above | R2 |

Oracles: `../rb3` and `../dc3-decomp` contain **none** of `AuditionMgr`, `AuditionSessionBuilder`,
`MidiValidator`, `UGCNet`, `can_enter_audition` or `kUGCPanMin` (a `grep -rl` over `*.cpp` and `*.h` in both trees
returned nothing). In our tree, `AuditionMgr.h` and `AuditionSessionPanel.{h,cpp}` declare the classes.

The audition feature is **off the boot-to-song path**. The one exception is AuditionMgr's construction and `Init`,
which `App` reaches directly (R1). The `audmgr` branch (3 commits, a 322-line `AuditionMgr.cpp` with
ctor/Init/Poll/SetState/dtor) covers exactly that part.

### 3.2 Other sourceless runs

| range | rows / B | what | our tree | oracle | boot-to-song |
|---|---|---|---|---|---|
| `0x82650B78`–`0x82651924` | 28 / 3,404 | `BandNetGameData` (vtable slots 0, 2–6 identified), `NetGameData`, `RemovingRemoteUserMsg`. Strings `default_ranked_match`, `h2h`, `ranked`. Two callers in `SessionMgr` | `BandNetGameData.h`, no bodies | rb3: `BandNetGameData.h` only, no bodies; dc3: none | boot-constructed (SessionMgr, R1 on 1 row); online sessions |
| `0x82B68D40`–`0x82B6963C` | 7 / 2,288 | `FxSendPitchShift360` / `FxSendSynapse360` (QP H3). **Also holds `0x82B69618`**, a 36 B ICF-folded `vector<T*>` storage free with **899 callers**, held as STLport source and unnamed | stubs | dc3: the same stubs | 360-ONLY (`synth_xbox`); effect-dependent |
| `0x8266B4C0`–`0x8266BDC8` | 25 / 2,204 | `PlayerCampaignGoalLeaderboard` and `PlayerCampaignCareerLeaderboard` slots 27–29 (`EnumerateFromID`, `EnumerateRankRange`, `GetStats`), `AssetOffer` slot 0 (dtor), plus QP's U1 100 B dtor | `CampaignLeaderboards.h` only, no `AssetOffer.cpp` | **rb3 `band3/meta_band/CampaignLeaderboards.cpp`, `AssetOffer.cpp`** | off path (leaderboard and store UI), R2 |
| `0x82B69868` (in run `0x82B69868`–`0x82B6A090`) | 1 / 332 | `FxSend360::Cleanup`, called by `Synth` and the run's own dtor | none (W16-UK §5) | none | on the synth path; 360-ONLY |

## 4. HMX code whose body we hold but which pairs with nothing

### 4.1 Unpinned holes (`auto_*` runs)

Ranked by judgement, then size. This is pin, name or wire work, not writing code.

| range | B | what | why it does not pair | boot-to-song |
|---|---:|---|---|---|
| `0x826C8470`–`0x826C9094` | 3,108 | `GemPlayer::GemPlayer` 2,704 + 9 EH funclets 404 | just past GemPlayer.cpp's pins (10-07d §6 item 2) | **on path** (every song), R2 |
| `0x825219A0`–`0x825227E8` | 3,616 | 23 StageKit `DataFunc`s (`set_stagekit_fog` …) + `FileEnumerate` 396 | `src/system/os/StageKit.cpp` ported but **not in `objects.json`** (item 2) | **on path**: registered at boot (R1); called by `LightPreset` during songs |
| `0x822D6568`–`0x822D6EA0` (4 runs) | 1,792 | `ObjPtr<T>` dtor / `Load` instances (RndTransAnim, RndPartLauncher, RndText, RndPropAnim …) | spans unpinned inside StreakMeter's holes (10-07d item 6, W16-UM NO-BASE-OBJ) | **on path** (track HUD load), R2 |
| `0x82BAC148` | 316 | `Gem::Gem` | hole in Gem.cpp's span (item 6) | **on path**, R2 |
| `0x82487880` | 168 | `RndEnvAnim::SetFrame` (vtable slot 7) | hole in EnvAnim's span | on path (venue env anims), R2 |
| `0x823399C0` | 112 | `BandCharDesc::NewObject` | sandwiched hole (item 2) | **on path** (boot), R1 |
| `0x82272A60` | 108 | `vector<Object*>::_M_fill_insert` | unpinned (item 6) | **on path**, R1 |
| `0x82402F68` | 56 | `RndDir::Replace` (named in map; 25 callers) | inside Anim's span, unpinned | on path (load), R2 |
| `0x822E36B8` | 16 | `GemTrackDir::UpdateFingerFeedback` | sandwiched hole (item 2) | **on path** (static tier "none"; see §2) |
| `0x82429BE0` | 4 | tail thunk to `~vector<Key<ObjectStage>>` (PropAnim) | hole in PropAnim | on path, none |
| `0x82655788` | 2,552 | `PerformanceData::SaveFixed` (vtable slot 1) | hole between PerformanceData's pins | end of song (profile save), R2 |
| `0x82B69868`–`0x82B6A090` | 1,724 (+332 sourceless, §3.2) | `~FxSend360` 372 + FxSend360 funclets and helpers | between `FxSend.cpp` and `FxSendPitchShift` (item 2) | on the synth path; 360-ONLY |
| `0x826F8F68` | 52 | `vector<ArpeggioPhrase>::erase` wrapper, called by `VocalPlayer::Rollback` | hole in Singer's span | practice rollback, none |
| `0x82641670` | 1,708 | `TokenRedemptionPanel::Poll` (vtable slot 7) | hole after TokenRedemptionPanel's pin | off path, R2 |
| `0x8276D248` | 84 | `TextFile::SetName` (vtable slot 16) | hole between DirUnloader and DataFile | off path, R2 |
| `0x826F0F18` | 8 | `map<Symbol,…>::clear` thunk (`RGTrainerPanel`) | hole in RGTutor | off path, R2 |

`GetAward` (`0x823EA5A8`, 12 B, AccomplishmentGroup, W16-UM) sits inside the network run `0x823EA4E8`, which is
otherwise ObjDup `SelectionIterator` code. It is the only HMX game row in that run.

### 4.2 HMX bodies that ICF folded into XDK territory

These are trivial getters. Retail's ICF placed the single surviving copy at an address inside an XDK unit, so no
pin of ours can ever own them. `xdkhmx.py` finds them as leaf bodies of 12 B or less, with a `blr`, no `bl` and no
Microsoft name, that pinned HMX units reference:

| addr | B | body | in | HMX referencers |
|---|---:|---|---|---:|
| `0x82956038` | 8 | `lwz r3,0x54(r3)` | `xdk/…/ssmdevice` | 14 |
| `0x82956040` | 8 | `lwz r3,0x6c(r3)` | `ssmdevice` | 5 |
| `0x82956048` | 8 | `lwz r3,0x74(r3)` | `ssmdevice` | 1 |
| `0x82956050` | 8 | `lwz r3,0x78(r3)` | `ssmdevice` | 1 |
| `0x82BD4EB0` | 8 | `lwz r3,0x8c(r3)` | `auto_03_82BD4B94` | 2 |
| `0x82BF5398` | 8 | `lwz r3,0x90(r3)` | `auto_03_82BF49F4` | 6 |
| `0x82C1A608` | 8 | `lwz r3,0xcc(r3)` | `auto_03_82C19F4C` | 15 |

The detector also matches `0x8283D580` (12 B, `lwz r11,0x100(r13)`), a TLS accessor, which is XDK. And
`0x82A478D0` (`stw r4,0(r3); blr`, 3 HMX referencers) is mapped as XDK `DirectInstrument::SetVolume`, so it is not
listed.

## 5. Rows in pinned in-scope units that no function of ours pairs with

There are **133 rows / 7,780 B** (VIA-DC3 57, IN-SOON 39, IN-CORE 20, IN-RB3ENG 17) at fuzzy 0 in pinned units
whose base obj exists. This is 10-07d §6 item 5 (U2 residue, 129 / 7,124 there), re-measured. They are listed in
the appendix with retail evidence and reach:

| reach | rows |
|---|---:|
| R1 | 10 |
| R2 | 65 |
| none | 57 |
| address not resolved | 1 |

12 of the rows are EH funclets.

- The rows called only from §3.1's runs belong to the audition cluster and are counted there.
- Most of the rest are anonymous fragments of 4–100 B with a named caller in the same unit. That is identification
  residue: our source very likely holds the body, but no name pairs it.

Two other groups were measured and are out of scope:

- 360-ONLY units add 420 rows / 77,000 B at fuzzy 0, nearly all `network/quazal` and `network/Core`.
- Pinned `xdk/` units without a base obj hold 4,506 rows / 2.1 MB, plus 5 rows in `network/Plugins`.

## 6. Excluded classes (not game or engine), with how each was decided

| class | runs | rows | B | decided by |
|---|---:|---:|---:|---|
| XDK/CRT, both neighbours `xdk/` | 269 | 2,507 | 634,392 | QP spatial rule (98.7% blocks); `xdkhmx.py` checked its HMX-referenced rows (§7) |
| XDK/CRT, one-sided, by hand | 13 | 327 | 74,196 | MS names / strings: CRT block at `0x828290A0`, D3DX deflate, XMP, xaudio2 / LEAPCORE, xhv2 `vt::Identity` DSP (QP X2), xmic, XParty `0x82B53D78` |
| Quazal band | 101 | 3,540 | 531,084 | address band; **1 row / 76 B** in it (`0x82A8D668`) is referenced only by an HMX unit (`RockCentral`, an RMC client stub) |
| RAD Bink | 1 | 141 | 65,292 | section `BINK` |
| static-init / atexit + import thunks | 42 | 583 | 20,712 | `0x82C3E5C0`–`0x82C4CE3C`; reached through the CRT `XCU` table. Some are HMX TU initializers (`0x82C41058` FourCC `Symbol`s, `0x82C40098` a 1,372 B table init). These pair once the owning TU's pins cover them, so they are QI-style re-home work, not missing source |
| vendor | 5 | 119 | 18,092 | liboggz `oggz-validate` (QP V1), zlib `zcalloc` / D3DX `adler32` |
| HMX network on Quazal | 6 | 153 | 12,508 | `network/net` / ObjDup `SelectionIterator`, `QuazalSession` callbacks (H3), `MessageBrokerDDL_Xbox`, `RBDataClient` / `RockBandDDLDeclarations` |
| EH funclets / phantom | 3 | 26 | 1,028 | `0x8242A508` (21 PropAnim unwind funclets), `0x82697484` (4 GamePanel funclets), `0x8273D7C8` (a data word carved as code) |

## 7. Control, and how complete the method is

**Known examples, each by address (`runstats.py` / `pinned0.json` / `xdkhmx.json`): 32 of 32 found.**

| source | examples | found by |
|---|---|---|
| AuditionMgr file `0x82560B08`–`0x82564410` | both runs + `SendDataPoint` pin + static init `0x82C3FD60` | `auto_*` runs + pinned fuzzy-0 |
| 10-07d §6 item 2, five runs | `0x825219A0`, `0x826C8468` (run starts `0x826C8470`), `0x82B6985C` (run `0x82B69868`), `0x823399C0`, `0x822E36B8` | `auto_*` runs |
| item 2 re-home rows | `ObjPtrList::operator=` `0x8230C628`, `JoypadTerminateCommon` `0x82524838`, `PreloadPanel::SetTypeDef` `0x827B36E0` | pinned fuzzy-0 |
| item 6, W16-UM NO-BASE-OBJ (13) | 6× `ObjPtr` `0x822D65E8`…, `GetAward` `0x823EA5A8`, `Gem::Gem` `0x82BAC148`, 4× ssmdevice, `0x82BD4EB0`, `0x82BF5398`, `0x82C1A608` | `auto_*` runs + ICF detector |
| item 6, others | `fill_insert` `0x82272A60`, G846 `0x824DFA50`, MetaPanel `fn_82574D90`, TrackPanelDir `fn_82308470`, `fn_82C3FA94` | runs + pinned fuzzy-0 |

**Expected non-finds**, each outside the definition:

- `RndShaderMgr::InitShaders` pairs at 70.45 (item 2). A function of ours pairs with it, wrongly homed.
- The VorbisReader rows `0x82BB33D8`/`33430`/`3510` pair at 100 under other units (item 6).
- The CRT ctor `0x8282A080` sits in CRT unit `rtti` (item 6), which is excluded as CRT.

**What the method can miss:**

1. **HMX code inside XDK-flanked runs that no HMX unit references.** The spatial rule is right on 98.7% of blocks
   (QP). Every XDK-territory row that an HMX unit references was listed: 163 rows / 25,596 B, of which 29 / 3,352 B
   have only HMX referencers. All of those carry a CRT/XDK name or sit inside a named CRT/xapi/xnet block (`strtok`,
   `rand`, `_beginthreadex`, `OpenThread`, `XMarketplaceCreateOfferEnumerator`, `XeCryptSha` thunks …), apart from
   the 7 ICF getters in §4.2. An unreferenced HMX body inside an XDK gap would still be invisible.
2. **The Quazal band** was classified by address only. Its HMX-only-referenced residue is 1 row / 76 B.
3. **A row that pairs with the wrong body** (fuzzy > 0) is outside the definition and was not counted.
4. **Identification is per run, not per row.** A run labelled by its dominant RTTI or strings could hide a stray
   function of another class. One case was caught that way: the 899-caller `vector<T*>` free inside the FxSend run.
   Small mixed rows in large runs were not each re-read.
5. **The static-init region** is aggregated, not adjudicated row by row.
6. **Reach** is a static over- and under-approximation (§2). The boot-to-song column is a judgement that cites it.

## 8. Not done

- Nothing was pinned, named, compiled or measured: this lane is read-only, so there is no A/B and no native gate.
- The likely mis-homed pins inside the UGCNet span (DataFunc `0x82B8E790`, ProfileMgr `0x82B8FC88`–`0x82B8FE30`)
  are flagged, not adjudicated.
- The appendix rows were not individually identified beyond the automatic evidence.
- The 0x82C3E5C0 static-init region was not split per owning TU (W16-QI covers the method).

## Appendix: in-scope pinned rows at fuzzy 0 (133 rows / 7,780 B)

Sorted by reach (R1, R2, none), then size. The evidence column is automatic: vtable slot, callers and callees by
map name or `unit:addr`.

| addr | B | unit | tier | evidence (retail) | reach |
|---|---:|---|---|---|---|
| `822774F8` | 164 | PatchDir | IN-RB3ENG | called by ??0MoggClipMap@@QAA@ABV0@@Z, ??0PatchLayer@@QAA@ABV0@@Z (+2); calls ??0?$list@PAUDep@CharPollableSorter@@V?$StlNodeA | R1 |
| `82524838` | 104 | OnlineID | IN-CORE | called by ?JoypadTerminate@@YAXXZ | R1 |
| `8250F3D0` | 100 | EntityUploader | IN-SOON | called by EntityUploader:8250F4D0, auto_03_82ABDF78_text:82ABE188 (+2); calls __restgprlr_29, __savegprlr_29 | R1 |
| `82574D90` | 100 | MetaPanel | IN-SOON | called by ?Init@MetaPanel@@SAXXZ; calls .L_82320778, ??0NextSongPanel@@QAA@XZ | R1 |
| `827BFB38` | 88 | Loader | IN-CORE | called by ??0App@@QAA@HPAPAD@Z; calls ?Poll@LoadMgr@@QAAXXZ | R1 |
| `827A28E0` | 20 | FixedSizeSaveable | VIA-DC3 | called by ??0App@@QAA@HPAPAD@Z | R1 |
| `826758E8` | 12 | band3/meta_band/StoreSongSortNode | IN-SOON | vtable StoreSongSortNode[22]; called by ?CheckContextVenue@?A0x1e5d0754@@YA_NPBVDataArra | R1 |
| `8250F4D0` | 8 | EntityUploader | IN-SOON | called by auto_03_8250A510_text:8250AB48; calls EntityUploader:8250F3D0 | R1 |
| `82288420` | 4 | BandCharacter | IN-RB3ENG | called by ??$_S_sort@VString@@V?$StlNodeAlloc@VString@@@st, BandCharacter:82289604 (+22); calls ?clear@?$_List_base@VString@@V?$StlNodeAlloc@VSt | R1 |
| `8276E210` | 4 | DataFlex | IN-CORE | called by ??0App@@QAA@HPAPAD@Z; calls ?Init@MidiParser@@SAXXZ | R1 |
| `82578A90` | 396 | MetaMaterial | VIA-DC3 | called by ?push_back@?$vector@U?$pair@MM@stlpmtx_std@@V?$S; calls .L_82578B74, ??$__uninitialized_copy@PAU?$pair@MM@stlpmtx_std | R2 |
| `823DB8C0` | 252 | Waypoint | VIA-DC3 | called by Waypoint:823DC070; calls ?Replace@RndTransformable@@UAAXPAVObjRef@@PAVObj, ?SetOwnerObj@?$ObjOwnerPtr@VCharInterest@@@@QAAX | R2 |
| `82577480` | 252 | BandSongMgr | IN-SOON | called by auto_03_82560B08_text:825613C8, auto_03_82560B08_text:82562978; calls ??$_M_find@H@?$_Rb_tree@HU?$less@H@stlpmtx_std@@, ??$_M_find@H@?$hashtable@U?$pair@$$CBHPAVSongMet | R2 |
| `82418DC0` | 232 | Mesh | VIA-DC3 | called by ?DrawShowing@DxMesh@@UAAXXZ; calls .L_822DE0C4, ?WorldXfm_Force@RndTransformable@@AAAABVTransfor | R2 |
| `8230C628` | 220 | SongSectionController | IN-RB3ENG | called by ??0Point@CharHair@@QAA@ABU01@@Z, ??4BeamDef@Spotlight@@QAAAAU01@ABU01@@Z (+30); calls ?Link@?$ObjPtrList@VRndMesh@@VObjectDir@@@@AAAXV, ?PoolAlloc@@YAPAXHHPBDH0@Z | R2 |
| `82682D40` | 192 | band3/game/BandUserMgr | IN-SOON | called by auto_03_825EAC38_text:825EB4F0; calls .L_82682DBC, ??$__find@PAVSymbol@@V1@@stlpmtx_std@@YAPAVSymbo | R2 |
| `82C3FD60` | 172 | BlockMgr | IN-CORE | calls ??0Object@Hmx@@QAA@XZ, atexit | R2 |
| `826833B0` | 168 | band3/game/BandUserMgr | IN-SOON | called by auto_03_825EAC38_text:825EB3F8; calls .L_8268341C, ??$__find@PAVSymbol@@V1@@stlpmtx_std@@YAPAVSymbo | R2 |
| `82B8FE30` | 168 | ProfileMgr | IN-SOON | called by DataFunc:82B8E790, auto_03_82B8E7E8_text:82B8ECA0; calls .L_823F2DA8, ?Sleep@Timer@@SAXH@Z | R2 |
| `8250F438` | 148 | EntityUploader | IN-SOON | called by EntityUploader:8250F4D8, auto_03_82ABDF78_text:82ABE740 (+6); calls .L_8250F4C4, __restgprlr_29 | R2 |
| `823199C8` | 136 | system/bandobj/MiniLeaderboardDisplay | IN-RB3ENG | called by ??1AppMiniLeaderboardDisplay@@UAA@XZ, ??_DMiniLeaderboardDisplay@@QAAXXZ (+1); calls ??1UIComponent@@UAA@XZ | R2 |
| `82576B70` | 120 | BandSongMgr | IN-SOON | called by auto_03_82560B08_text:82562978; calls ?MemOrPoolFreeSTL@@YAXHPAX@Z | R2 |
| `824CB080` | 100 | PropSync | IN-CORE | called by ??0PresetOverride@WorldDir@@QAA@PAVObject@Hmx@@@; calls ?AddRef@Object@Hmx@@QAAXPAVObjRefOwner@@@Z | R2 |
| `82563258` | 100 | MetaPerformer | IN-SOON | called by auto_03_825632BC_text:825632F0; calls ??0DataPoint@@QAA@PBD@Z, ?AddPair@DataPoint@@QAAXPBDVDataNode@@@Z | R2 |
| `827CA180` | 100 | ChunkStream | IN-CORE | called by auto_03_8272BAC8_text:827303C8 | R2 |
| `82BB1588` | 96 | HAQManager | VIA-DC3 | called by ??1Tail@@UAA@XZ, ?ReturnTail@GemRepTemplate@@QAAXPAVRndMesh@@@Z; calls ?SetTransParent@RndTransformable@@QAAXPAV1@_N@Z, HAQManager:82BB1540 | R2 |
| `826DA078` | 92 | band3/game/PerfectSectionTracker | IN-SOON | called by ?clear@?$_Rb_tree@W4TrackType@@U?$less@W4TrackTy; calls .L_822D85B4, ?MemOrPoolFreeSTL@@YAXHPAX@Z | R2 |
| `82725AB8` | 92 | ByteGrinder | VIA-DC3 | called by ?Init@ByteGrinder@@QAAXXZ; calls ?Int@DataNode@@QBAHPBVDataArray@@@Z, __restgprlr_29 | R2 |
| `82B8E790` | 88 | DataFunc | IN-CORE | called by auto_03_82B8C8C8_text:82B8C990; calls ProfileMgr:82B8FE30, __restgprlr_27 | R2 |
| `824F6778` | 80 | RockCentral | IN-SOON | vtable ClientProtocol@Quazal[4], RBDataClient@Quazal[4] | R2 |
| `82812418` | 80 | system/ui/UILabelDir | VIA-DC3 | called by ??_EUILabelDir@@$4PPPPPPPM@A@AAPAXI@Z; calls ??_DUILabelDir@@QAAXXZ, ?MemFree@@YAXPAX@Z | R2 |
| `82B8FD58` | 80 | ProfileMgr | IN-SOON | vtable UGCNetResource[4]; called by auto_03_82B8E7E8_text:82B8F970, auto_03_82B8E7E8_text:82B8F980; calls auto_03_82845180_text:8284D830 | R2 |
| `82295948` | 76 | BandDirector | IN-RB3ENG | vtable exception@std[0], __Named_exception@stlpmtx_std[0]; calls ??1exception@std@@UAA@XZ, ??3RndCubeTex@@SAXPAX@Z | R2 |
| `822C0CF8` | 76 | BandSongPref | IN-RB3ENG | vtable BandSongPref[0], UIColor[0]; calls ??1Object@Hmx@@UAA@XZ, ?MemFree@@YAXPAX@Z | R2 |
| `822C23E8` | 76 | BandIKEffector | IN-RB3ENG | vtable ?$ObjOwnerPtr@VCharWeightable@@VObjectDir@@[0]; calls ??1?$ObjOwnerPtr@VCharWeightable@@@@UAA@XZ, ??3RndCubeTex@@SAXPAX@Z | R2 |
| `823208A0` | 76 | CharacterTest | VIA-DC3 | called by ??_EAppScoreDisplay@@$4PPPPPPPM@BGI@AAPAXI@Z, ??_EScoreDisplay@@WBEA@AAPAXI@Z; calls ??_DScoreDisplay@@QAAXXZ, ?MemFree@@YAXPAX@Z | R2 |
| `82654440` | 76 | SessionUsersProviders | IN-SOON | called by ?ViewUserGamercard@OvershellSlot@@QAAXH@Z; calls .L_82654480, ?ShowGamercard@PlatformMgr@@QAA?AW4ShowGamercard | R2 |
| `8268F0D0` | 76 | BandUser | IN-SOON | called by auto_03_825EAC38_text:825EB3F8; calls .L_825282B8, ??0AutoplayAuditionUser@@QAA@XZ | R2 |
| `82709530` | 76 | Sequence | VIA-DC3 | vtable Sequence[0], WaitSeq[0]; called by ??_ESequence@@WCI@AAPAXI@Z; calls ??1Sequence@@UAA@XZ, ??3RndCubeTex@@SAXPAX@Z | R2 |
| `82709E90` | 76 | Sequence | VIA-DC3 | vtable GroupSeq[0], ParallelGroupSeq[0]; called by ??_EGroupSeq@@WCI@AAPAXI@Z; calls ??1GroupSeq@@UAA@XZ, ??3RndCubeTex@@SAXPAX@Z | R2 |
| `827B8630` | 76 | StoreEnumeration | VIA-DC3 | vtable XboxEnumeration[0]; calls ??1XboxEnumeration@@UAA@XZ, ??3RndCubeTex@@SAXPAX@Z | R2 |
| `827BFC48` | 76 | Loader | IN-CORE | vtable MetaMusicLoader[0], Loader[0]; calls ??1Loader@@UAA@XZ, ??3RndCubeTex@@SAXPAX@Z | R2 |
| `827FDA90` | 76 | UIList | VIA-DC3 | called by ??_EUIList@@$4PPPPPPPM@CEA@AAPAXI@Z, ??_EUIList@@WBEA@AAPAXI@Z (+3); calls ??_DUIList@@QAAXXZ, ?MemFree@@YAXPAX@Z | R2 |
| `82B8B808` | 76 | Lit_NG | VIA-DC3 | vtable RndFur[0], NgFur[0]; calls ??1RndFur@@UAA@XZ, ?MemFree@@YAXPAX@Z | R2 |
| `825150C8` | 72 | PlatformMgr | IN-CORE | called by ?Category_HandleButtonDownMsg@AccomplishmentPane, ?Details_HandleButtonDownMsg@AccomplishmentPanel (+38); calls ?GetObj@DataNode@@QBAPAVObject@Hmx@@PBVDataArray, __RTDynamicCast | R2 |
| `825AAFF0` | 72 | SetlistMergePanel | IN-SOON | called by ?Init@LockStepMgr@@SAXXZ; calls .L_825AAD2C, ??0LockResponseMsg@@QAA@XZ | R2 |
| `823793C8` | 68 | CharForeTwist | VIA-DC3 | called by ??_ECharDriver@@$4PPPPPPPM@A@AAPAXI@Z; calls ??1CharDriver@@UAA@XZ, ??1Object@Hmx@@UAA@XZ | R2 |
| `823B08D0` | 68 | CharPollGroup | VIA-DC3 | called by ??_ECharPollGroup@@$4PPPPPPPM@A@AAPAXI@Z; calls ??1CharPollGroup@@UAA@XZ, ??1Object@Hmx@@UAA@XZ | R2 |
| `823C48A0` | 68 | CharBlendBone | VIA-DC3 | called by ??_ECharBlendBone@@$4PPPPPPPM@A@AAPAXI@Z; calls ??1CharBlendBone@@UAA@XZ, ??1Object@Hmx@@UAA@XZ | R2 |
| `823C6880` | 68 | CharForeTwist | VIA-DC3 | called by ??_ECharForeTwist@@$4PPPPPPPM@A@AAPAXI@Z; calls ??1CharForeTwist@@UAA@XZ, ??1Object@Hmx@@UAA@XZ | R2 |
| `823CED00` | 68 | CharDriver | VIA-DC3 | called by ??_ECharNeckTwist@@$4PPPPPPPM@A@AAPAXI@Z; calls ??1CharNeckTwist@@UAA@XZ, ??1Object@Hmx@@UAA@XZ | R2 |
| `82651948` | 68 | Matchmaker | IN-SOON | vtable MatchmakerMode[0], BandFinding[0]; calls ??3RndCubeTex@@SAXPAX@Z | R2 |
| `82BB1540` | 68 | HAQManager | VIA-DC3 | called by HAQManager:82BB1588; calls .L_82BB1560, ?GroupOwner@@YAPAVRndGroup@@PAVObject@Hmx@@@Z | R2 |
| `826927D8` | 60 | NetGameMsgs | IN-SOON | vtable HopoStatMemberTracker[26], HopoPercentStatMemberTracker[26] | R2 |
| `82C3FA60` | 52 | system/os/UsbMidiGuitar | IN-CORE | calls atexit | R2 |
| `824F6738` | 48 | RockCentral | IN-SOON | lis r11, lbl_8207F9B0@ha; addi r11, r11, lbl_8207F9B0@l; cmplw cr6, r11, r4 | R2 |
| `82C3FA30` | 40 | system/os/UsbMidiGuitar | IN-CORE | li r10, 0x20; lis r11, lbl_82CCB1E8@ha; addi r11, r11, lbl_82CCB1E8@l | R2 |
| `82B8FDA8` | 36 | ProfileMgr | IN-SOON | vtable UGCNetResource[1] | R2 |
| `8278C8D8` | 28 | system/beatmatch/GameGemList | IN-SOON | called by ?ClosestMarkerIdxAtOrAfterTick@GameGemList@@QBAH, ?FillAt@FillInfo@@QBA_NHAAUFillExtent@@_N@Z (+2) | R2 |
| `824F6720` | 24 | RockCentral | IN-SOON | lis r11, lbl_8207F9A0@ha; addi r11, r11, lbl_8207F9A0@l; subf r11, r11, r4 | R2 |
| `828027A8` | 24 | UIFontImporter | VIA-DC3 | called by ?Init@UIManager@@UAAXXZ, ?Terminate@UIManager@@UAAXXZ | R2 |
| `8227DB30` | 16 | BandCharacter | IN-RB3ENG | vtable BandCharDescTest[0], BandCharDesc[0]; calls ??_GBandCharDesc@@UAAPAXI@Z | R2 |
| `823163F8` | 16 | InlineHelp | VIA-DC3 | vtable InlineHelp[15]; calls ?SetTypeDef@UIComponent@@UAAXPAVDataArray@@@Z | R2 |
| `8231A930` | 16 | system/bandobj/MeterDisplay | IN-RB3ENG | vtable MeterDisplay[13], ScoreDisplay[13]; calls ?Print@RndTransformable@@UAAXXZ | R2 |
| `82641320` | 16 | band3/meta_band/TokenRedemptionPanel | IN-SOON | vtable TokenRedemptionPanel[10], StorePanel[10]; calls ?Load@UIPanel@@UAAXAAVBinStream@@@Z | R2 |
| `828161E0` | 16 | UIPicture | VIA-DC3 | vtable UIPicture[2], UIProxy[2]; calls ?Replace@RndTransformable@@UAAXPAVObjRef@@PAVObj | R2 |
| `823D9508` | 12 | FileMergerOrganizer | VIA-DC3 | vtable FileMergerOrganizerLoader[1], FileMergerOrganizerLoader[3] | R2 |
| `823DA748` | 12 | FileMergerOrganizer | VIA-DC3 | vtable FileMergerOrganizerLoader[4]; calls ?StartLoad@FileMergerOrganizer@@QAAXXZ | R2 |
| `823DC070` | 12 | Waypoint | VIA-DC3 | vtable Waypoint[2]; calls Waypoint:823DB8C0 | R2 |
| `82519FF8` | 12 | FileCache | IN-CORE | called by ?E3CheatChangeRGThreshold@@YA?AVDataNode@@PAVDat, ?OnMsg@RealGuitarController@@QAAHABVStringStrumm | R2 |
| `82C3FA20` | 12 | system/os/UsbMidiGuitar | IN-CORE | calls ??0Timer@@QAA@XZ | R2 |
| `822EE490` | 8 | GemTrackDir | IN-RB3ENG | vtable GemTrackDir[13], TrackPanelDir[13] | R2 |
| `8250F4D8` | 8 | EntityUploader | IN-SOON | called by auto_03_8250A510_text:8250A928; calls EntityUploader:8250F438 | R2 |
| `827B36E0` | 8 | DeJitterPanel | VIA-DC3 | called by ?SetTypeDef@PreloadPanel@@$4PPPPPPPM@7AAXPAVData, ?SetTypeDef@PreloadPanel@@$4PPPPPPPM@A@AAXPAVDat; calls ?SetTypeDef@UIPanel@@UAAXPAVDataArray@@@Z | R2 |
| `828012D8` | 8 | UIComponent | VIA-DC3 | vtable BandList[2], UIListProvider[2]; calls ?GetTick@BeatMatcher@@UBAHXZ | R2 |
| `8270A8D8` | 112 | Sequence | VIA-DC3 | called by ?resize@?$vector@V?$ObjPtr@VSeqInst@@@@V?$StlNod; calls .L_823D8B28, ?_M_fill_insert_aux@?$vector@V?$ObjPtr@VSeqInst@ | none |
| `82B8FCE8` | 112 | ProfileMgr | IN-SOON | vtable UGCNetResource[3]; calls .L_823F2C34, auto_03_82845180_text:8284D820 | none |
| `82B8FDD0` | 96 | ProfileMgr | IN-SOON | vtable UGCNetResource[2]; calls .L_823F2D10, auto_03_82845180_text:8284D840 | none |
| `82B8FC88` | 92 | ProfileMgr | IN-SOON | vtable UGCNetResource[5]; calls .L_823F2BC4, auto_03_82845180_text:8284D810 | none |
| `826F7C78` | 80 | band3/game/Singer | IN-SOON | called by band3/game/Singer:826F8128, band3/game/Singer:826F8350 (+1); calls ??1?$pair@$$CBVSymbol@@V?$vector@PAVLightPreset@ | none |
| `82434194` | 68 | CharBonesMeshes | VIA-DC3 | calls ??1RndHighlightable@@UAA@XZ; EH funclet | none |
| `824F6CD0` | 68 | LitAnim | VIA-DC3 | calls ??1Object@Hmx@@UAA@XZ; EH funclet | none |
| `824380F0` | 64 | system/rndobj/Mat | VIA-DC3 | calls ??1DataNode@@QAA@XZ; EH funclet | none |
| `8236CA38` | 48 | Char | VIA-DC3 | called by CharBones:823AE428, CharBonesSamples:823DFB94; calls ?MemOrPoolFreeSTL@@YAXHPAX@Z | none |
| `8282486C` | 48 | system/ui/UIProxy | IN-RB3ENG | calls ??1FilePath@@UAA@XZ; EH funclet | none |
| `82578C24` | 44 | MetaMaterial | VIA-DC3 | calls ?MemOrPoolFreeSTL@@YAXHPAX@Z, auto_03_828290A0_text:82829A08; EH funclet | none |
| `825AF2E0` | 44 | Text | VIA-DC3 | calls ??$?0H@?$StlNodeAlloc@V?$_List_node@H@stlpmtx_st; EH funclet | none |
| `822C2488` | 40 | BandIKEffector | IN-RB3ENG | called by BandIKEffector:822C4160, BandPatchMesh:8234BAE8 (+9); calls ?MemOrPoolFreeSTL@@YAXHPAX@Z | none |
| `822DF9F8` | 40 | CharLipSync | VIA-DC3 | called by AmbientOcclusion:82492BFC, ChordShapeGenerator:822DF888 (+6); calls ?MemOrPoolFreeSTL@@YAXHPAX@Z | none |
| `8257A4B4` | 40 | SongMgr | VIA-DC3 | calls auto_03_82B68D40_text:82B69618; EH funclet | none |
| `8257A4DC` | 40 | SongMgr | VIA-DC3 | calls auto_03_82B68D40_text:82B69618; EH funclet | none |
| `8257A504` | 40 | SongMgr | VIA-DC3 | calls auto_03_82B68D40_text:82B69618; EH funclet | none |
| `826556B8` | 40 | PerformanceData | IN-SOON | called by BandDirector:82297380, ButtonHolder:827B8CF8 (+56); calls ?MemOrPoolFreeSTL@@YAXHPAX@Z | none |
| `827BFBF8` | 40 | system/synth/Utl | VIA-DC3 | calls ??1String@@UAA@XZ; EH funclet | none |
| `827BFC20` | 40 | system/synth/Utl | VIA-DC3 | calls ??1FilePath@@UAA@XZ; EH funclet | none |
| `82BBA020` | 40 | aes | VIA-DC3 | calls aes:82BBA058 | none |
| `82C3FA94` | 40 | system/os/UsbMidiGuitar | IN-CORE | calls ??1ObjRefOwner@@UAA@XZ; EH funclet | none |
| `824DFA50` | 36 | Spotlight | VIA-DC3 | called by CameraShot:824C56D4, CharMeshHide:827009BC (+13); calls ?MemOrPoolFreeSTL@@YAXHPAX@Z | none |
| `82713740` | 36 | MidiInstrument | VIA-DC3 | called by MidiInstrument:82714DA0; calls ?MemOrPoolFreeSTL@@YAXHPAX@Z | none |
| `8231F620` | 16 | system/bandobj/ReviewDisplay | IN-RB3ENG | called by PlayerDiffIcon:82325C4C, ScoreDisplay:8231FAAC (+1) | none |
| `82360988` | 16 | Tour | IN-SOON | called by TourPerformer:823610E8, TourPerformer:823611C4; calls ??1Object@Hmx@@UAA@XZ | none |
| `82390AE0` | 16 | CharClipGroup | VIA-DC3 | called by FileMerger:82394698, FileMerger:82394B58 | none |
| `8239C370` | 16 | CharCollide | VIA-DC3 | called by system/char/CharMeshCacheMgr:8239CBFC | none |
| `8251F838` | 16 | ContentMgr | IN-CORE | called by ContentMgr_Xbox:8251FA48, ContentMgr_Xbox:8251FACC (+2) | none |
| `82595F08` | 16 | Accomplishment | IN-SOON | called by SongSortMgr:8259A408, SongSortMgr:8259A4A8 (+1); calls ??1Object@Hmx@@UAA@XZ | none |
| `82651938` | 16 | Matchmaker | IN-SOON | called by Matchmaker:826539BC, Matchmaker:82653A9C | none |
| `82684E30` | 16 | band3/game/BandUserMgr | IN-SOON | called by SongDB:82687EC0, SongDB:826884A8 (+2) | none |
| `8276E218` | 16 | DataFlex | IN-CORE | called by system/beatmatch/BeatMaster:8276F07C, system/beatmatch/BeatMaster:8276F2B0 | none |
| `827D1088` | 16 | MBT | IN-CORE | called by SongInfoCopy:827D18B0, SongInfoCopy:827D1BE0 (+1) | none |
| `82814440` | 16 | UIPanel | VIA-DC3 | called by UIListCustom:8281E6F0, UIListHighlight:8281F620 (+1) | none |
| `82BBA048` | 16 | aes | VIA-DC3 | called by aes:82BBA020 | none |
| `82376D30` | 12 | CharServoBone | VIA-DC3 | called by CharClipDriver:8239F8B4, CharDriver:82377DC0 (+1); calls ?PoolFree@@YAXHPAX@Z | none |
| `82518348` | 12 | File | IN-CORE | called by FileCache:82519AC8, FileCache:82519C14; calls ?PoolFree@@YAXHPAX@Z | none |
| `8256A850` | 12 | band3/meta_band/AssetMgr | IN-SOON | called by BandSongMetadata:8259F950, BandSongMetadata:825A0248 (+3); calls ?_M_erase_after@?$_Slist_base@U?$pair@$$CBVSymbo | none |
| `827130A0` | 12 | system/synth/FxSendPitchShift | VIA-DC3 | called by MidiInstrument:82713714, Sequence:8270B444 (+1); calls ?PoolFree@@YAXHPAX@Z | none |
| `8271A0A8` | 12 | StreamNull | VIA-DC3 | called by Sfx:8271BCE4; calls ?PoolFree@@YAXHPAX@Z | none |
| `82BBA058` | 12 | aes | VIA-DC3 | called by aes:82BBA020, aes:82BBA048 | none |
| `822C7038` | 8 | BandRetargetVignette | IN-RB3ENG | called by system/bandobj/BandFaceDeform:822C7C84, system/bandobj/BandFaceDeform:822C85D0 (+2); calls ?MemFree@@YAXPAX@Z | none |
| `8235FC80` | 8 | Tour | IN-SOON | called by AccomplishmentPanel:825FEB48, CampaignSongInfoPanel:825F69D8 (+3); calls ??1SongFilter@SongSortMgr@@QAA@XZ | none |
| `822EAB90` | 4 | GemTrackDir | IN-RB3ENG | called by Font:824762A4, GemTrackDir:822EE2EC (+2); calls ?clear@?$_Rb_tree@VSymbol@@U?$less@VSymbol@@@stl | none |
| `82308470` | 4 | TrackPanelDir | IN-RB3ENG | called by BandCrowdMeter:822C0A24, BandScoreboard:822CE590 (+33); calls ??1?$vector@V?$ObjPtr@VRndPartLauncher@@@@V?$Stl | none |
| `8230EE88` | 4 | SongSectionController | IN-RB3ENG | called by CharIKFingers:82310894, SongSectionController:8230EF98 (+1); calls ?clear@?$_List_base@VPracticeSectionMapping@Song | none |
| `823A0910` | 4 | CharMeshHide | VIA-DC3 | called by CharMeshHide:823A1610, CharMeshHide:823A1F44 (+1); calls ??1?$ObjPtr@VRndDrawable@@@@UAA@XZ | none |
| `823C0000` | 4 | CharIKHead | VIA-DC3 | called by BandList:8233CEA8, BandList:8233F778 (+16); calls ??1?$ObjPtr@VRndTransformable@@@@UAA@XZ | none |
| `8243DD20` | 4 | system/rndobj/Utl | VIA-DC3 | called by MatAnim:82462AC0, MatAnim:8246351C (+3); calls ??1?$ObjPtr@VRndTex@@@@UAA@XZ | none |
| `824CE8F8` | 4 | PropSync | IN-CORE | called by PanelDir:824CF99C, PropSync:824CEAFC (+1); calls ?clear@?$_List_base@UPresetOverride@WorldDir@@V? | none |
| `824CE900` | 4 | PropSync | IN-CORE | called by PanelDir:824CFA44, PropSync:824CEB2C (+1); calls ?clear@?$_List_base@UBitmapOverride@WorldDir@@V? | none |
| `824DC570` | 4 | Spotlight | VIA-DC3 | called by Spotlight:824DF8F0; calls ??1?$vector@UBeamDef@Spotlight@@V?$StlNodeAlloc@ | none |
| `8250B400` | 4 | band3/net_band/DataResults | IN-SOON | called by band3/net_band/DataResults:8250B4B4, band3/net_band/DataResults:8250B594; calls ?clear@?$_List_base@VDataResult@@V?$StlNodeAlloc | none |
| `82713678` | 4 | MidiInstrument | VIA-DC3 | called by CheatProvider:826FFFC0, MidiInstrument:82714BD0 (+4); calls ??1?$ObjPtr@VSynthSample@@@@UAA@XZ | none |
| `827150A8` | 4 | MidiInstrument | VIA-DC3 | called by MidiInstrument:827151BC, MidiInstrument:82715458; calls ??1?$vector@VSampleZone@@V?$StlNodeAlloc@VSample | none |
| `82774068` | 4 | SongData | IN-SOON | called by system/beatmatch/DrumMap:8278C438; calls ??1FillInfo@@UAA@XZ | none |
| — | 116 | Tour | IN-SOON | named `public: __cdecl stlpmtx_std::vector<class Symbol, class stlpmtx_std::S` (address not resolved from map) | ? |
