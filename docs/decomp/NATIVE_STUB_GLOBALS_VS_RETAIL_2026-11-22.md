# Native hand-defined globals vs retail initial values

Lane STUBGLOBALS, 2026-11-22, worktree `~/tmp/wt-b53`, branch `stubglobals` (from `b53`, rebased by the
coordinator onto main `997e35516`). Follow-up named in `B53_VIA_DC3_RETAIL_READ_2026-11-22.md` §3: B53 found that
`int lbl_82F14008;` (native 0) stands for retail `.data` 0x82C6FB90 = 0xFFFFFFFF. Question: which other global
that `native/src/` defines by hand (because no compiled X360 TU does) starts with a different value, type or size
than retail?

## 1. Result

- **62 globals checked plus 100 `Symbol` globals as one class.** Three size defects fixed. One pointer and the
  `Symbol` class differ and are reported, not fixed.
- **Fixed (size: retail has an object, native reserved too few bytes):**
  - `TheDebugNotifyOncePrinter` (`dta_link_stubs.s`): the weak blob was 128 bytes, but the type is
    `class DebugNotifyOncePrinter { char msg[0x100]; }`. `operator<<` does `strncpy(msg, cc, 0xFF)`, which always
    writes 255 bytes. So the first `MILO_LOG_ONCE` (`BinStream::Write` on a failed stream, linked into every target
    that takes this weak copy: 17 of 18) overwrote the next 127 bytes of `.bss`. In `rb3-dta` that is
    `TheGameData`'s blob, at +0x80 (`nm -n`). The blob is now 256 bytes. Zero fill is still right: it means an
    empty `msg`.
  - `TheProfileMgr`, `TheRockCentral` (`m1_link_stubs.s`, used only by `rb3-song`): these were 8-byte "null manager
    pointer" blobs, but both are declared **by value** (`extern ProfileMgr TheProfileMgr`,
    `extern RockCentral TheRockCentral`). Retail has a `.bss` object at 0x82DFD818 and 0x82CC8F60. The consumer is
    `BandSongMgr.cpp`. Each blob is now 0x400 bytes, above native sizeof 0x1b0 / 0x1a8. As before, the storage is
    zero-filled and never constructed.
- **Reported, not fixed:**
  - **`TheAuditionMgr`** (`rb3_app_link_stubs.cpp:63`, owned by another session): native is null. Retail
    0x82C7292C is initialised `.data` holding 0x82DFDDBC, a pointer to the AuditionMgr object. Main now has
    `AuditionMgr *TheAuditionMgr = &gAuditionMgr;` (`src/band3/meta_band/AuditionMgr.cpp:85`, from audmgr). So once
    rb3-app compiles that TU, the stub is a duplicate definition and should simply go.
  - **The 100 `Symbol` globals** (`milo_link_stubs.cpp` 68, `m10_leaf_stubs.cpp` 18, `m6_symbols.cpp` 9,
    `m1_symbols.cpp` 3, `bandtrack_link_stubs.cpp` 1, `w16ts_link_support.cpp` 1) are default-constructed, so they
    hold an empty `Symbol`. Retail never had a global here. Each handler builds a **function-local static** on first
    use, with the name string. Two samples from `band.exe`:
    - `release_configuration` (string 0x8201D288): used at 0x822B8FE0 as `ori r11,r11,2; stw guard` then
      `bl 0x827C0728` (Symbol ctor) on the local static.
    - `enter_venue` (string 0x820361A8): the same pattern at 0x82332914.

    That retail form is `RB3_HANDLE_LOCAL_STATIC`. Native applies it only to the TUs listed in
    `native/CMakeLists.txt`. In every other TU, a `HANDLE(sym, …)` or `_msg->Sym(…) == sym` test against one of
    these globals compares with an empty symbol and never matches, where retail would. Not fixed in place:
    `Symbol foo("foo")` at static-init time would call `gStringTable->Add` before `Symbol::PreInit`, and
    `gHashTable` in another TU has no defined construction order. The fix is per-TU `RB3_HANDLE_LOCAL_STATIC`, or
    interning these after `Symbol::PreInit`. That is a CMake/boot change for the coordinator.
- Everything else either matches retail or differs by design (§3). One more by-design case: the weak
  `MidiParser::sParsers` blob is an **unconstructed** `std::list` in ark/dta/save/song. Retail constructs it in a
  `??__E` dynamic initializer (found among the functions the resolver read). A zeroed libstdc++ list crashes on
  iteration, so if `MidiParserMgr::Poll` ever runs in those four targets, that is the place to look.

## 2. Method

1. **Population.** I ran `nm` over every native object built from `native/src/` (absent2's build tree
   `~/tmp/wt-absent2/native/build`; its `native/src` differs from this branch only in `milo_link_stubs.cpp` and
   `rb3_app_link_stubs.s`). That gave each defined data symbol (D/B/R/V). I kept those that some object compiled from
   `src/{system,band3,network,xdk}` leaves undefined, i.e. globals the X360 sources declare `extern` and native
   supplies: **162 symbols**. Of these, 100 are `Symbol`s, 6 are typeinfo/vtable stubs (no retail value; out of
   scope), and 56 are values. I added `TheAuditionMgr` and `TheWiiFriendsProvider` from `rb3_app_link_stubs.cpp`,
   and four `Symbol` samples.
2. **Retail address.** `~/tmp/stubglobals/t/scan.py` parses the COFF relocations of every X360 base obj in
   `objdiff.json` and finds the functions that reference the global. For up to three of them (fuzzy-100 first),
   `resolve.py` runs `bin/objdiff-cli diff -f json --include-instructions`. It takes the retail operand paired with
   ours wherever the opcode is equal, so a `lbl_XXXXXXXX` gives the address directly. A named retail symbol is
   resolved through `scripts/target_symbol_map.json`; the map also carries data names such as
   `?TheGame@@3PAVGame@@A` = 0x82E02128. Every resolved global had a single agreeing vote target (4–20 votes).
   Globals with no paired reader were tried by string xref in `band.exe` (`xref.py`).
3. **Retail value.** `pe.py` reads `band.exe`'s section table. An address inside a section's raw data gives its
   initial bytes. An address past `SizeOfRawData` is zero. `.data` has 0x58200 raw bytes of 0x1F5EAC, and all the
   singletons live in the zero tail.
4. **Native value.** `elfval.py` reads the symbol's section, type, size and initial bytes from each of the 18
   linked native executables. So the native column is measured on the binaries, not read off the source.

**Control (the known case).** In the absent2 build (pre-b53 fix), `lbl_82F14008` reads `.bss B sz=4 00000000` in
rb3-app and rb3-render. The resolver maps it to `lbl_82C6FB90` (8 votes over `UpdateHeap` and `OnToggleHeap`), and
`band.exe` reads `.data init ffffffff`. That is **DIFFERS** (0 vs −1). On this branch the source is
`int lbl_82F14008 = -1;` (b53 `7c433ad81`), which makes it EQUAL. The same pipeline read `gMainThreadID` as
retail −1 = native −1, so the instrument can report both outcomes.

## 3. Table

Retail addresses are clean TU5. "zero" means `.bss` or `.data` past its raw bytes. "app/render" names the native
targets where a real definition replaces the stub.

### 3.1 Scalars with a chosen initial value

| global | native file | native | retail | verdict |
|---|---|---|---|---|
| `lbl_82F14008` (heap cursor) | milo_link_stubs.cpp | −1 (b53; was 0) | 0x82C6FB90 init 0xFFFFFFFF | EQUAL after b53 (control: DIFFERS before) |
| `gMainThreadID` | platform/ThreadCall_Native.cpp | 0xFFFFFFFF | 0x82C71B08 init 0xFFFFFFFF | EQUAL |
| `MemHeapStack::sDefaultHeap` | dta_link_stubs.s (weak, 128 zero) | strong src def −1 wins in all 18 targets | 0x82C78E24 init 0xFFFFFFFF | EQUAL (weak copy dead) |
| `gCharHighlightY` | milo_link_stubs.cpp | −1.0f | no reader in retail: no `char_debug`, `frame %d` or `%s %.4f` string in band.exe | UNLOCATED (debug overlay absent from retail) |
| `BandCamShot::sHideAllCharactersHack` | w16ts_link_support.cpp, rb3_app_link_stubs.cpp | 0 | retail `StartAnim` lacks the tail (BandCamShot.cpp:353); no `set_character_hide_hack_enabled` string | UNLOCATED |
| `gShowAssetName` | bandtrack_link_stubs.cpp, rb3_app_link_stubs.cpp | false | read only under `HX_NATIVE`; retail `UpdateExtendedText` is fuzzy 100 without it | UNLOCATED (native-only) |
| `gRB3OutfitComposeActive` | x20_bandpatchmesh_link.cpp | false | native-only flag (OutfitConfig.cpp `ComposeScope`) | N/A |

### 3.2 Null pointers (retail zero)

All EQUAL: native null, retail zero.

| global | retail | global | retail |
|---|---|---|---|
| `TheBandDirector` | 0x82CBC998 | `TheRGTrainerPanel` | 0x82E02A94 |
| `TheBandUserMgr` | 0x82E023B8 | `TheSaveLoadMgr` | 0x82DFD908 |
| `TheCharSync` | 0x82DFDE20 | `TheSessionMgr` | 0x82DFEB88 |
| `TheGame` | 0x82E02128 | `TheSongDB` | 0x82E023F8 |
| `TheGameConfig` | 0x82E02418 | `TheSyncStore` (rb3_app) | 0x82CBFC4C |
| `TheGameMicManager` | 0x82E02394 | `TheSynth` | 0x82E036FC |
| `TheGameMode` | 0x82E02530 | `TheTour` | 0x82CBE8F0 |
| `TheGamePanel` | 0x82E02604 | `TheUIEventMgr` | 0x82DFED4C |
| `TheMidiParserMgr` | 0x82E074AC | `gMemStackLock` | 0x82E06E0C |
| `TheNameGenerator` (rb3_app) | 0x82E01838 | `TheNetSession` | 0x82CBFAF8 |

### 3.3 By-value objects

Retail has zero bytes in `.bss`, and its dynamic initializer constructs the object at boot.

| global | retail | native | verdict |
|---|---|---|---|
| `TheProfileMgr` | 0x82DFD818 | m1_link_stubs.s: **8 bytes** (song); real in app/render | **DIFFERS (size), fixed → 0x400** |
| `TheRockCentral` | 0x82CC8F60 | m1_link_stubs.s: **8 bytes** (song); 0x1a8 storage elsewhere | **DIFFERS (size), fixed → 0x400** |
| `TheDebugNotifyOncePrinter` | no retail reader (`MILO_PRINT_ONCE` compiles out) | dta_link_stubs.s: **128 bytes** for a 256-byte type (17 targets); 256 in app | **DIFFERS (size, overflow), fixed → 256** |
| `MidiParser::sParsers` | 0x82E07410, constructed by `??__E` | real list in 14 targets; weak 128 zero (unconstructed) in ark/dta/save/song | DIFFERS (construction), by design; reported |
| `TheBeatMatchOutput` | 0x82E06488 | constructed `LogFile("beatmatch")` | EQUAL |
| `TheMC` | 0x82CCB9A4 | constructed `MemcardXbox` | EQUAL |
| `TheNetMessageFactory` | 0x82CBFF58 | constructed | EQUAL |
| `TheRenderState` | 0x82E04F68 | constructed (empty class) | EQUAL |
| `TheBandUI` | 0x82DFD2B0 | zero storage, unconstructed (render); real in app | EQUAL bytes; unconstructed by design |
| `TheMemcardMgr` | 0x82E066AC | zero storage, unconstructed | EQUAL bytes; unconstructed by design |
| `TheNet` | 0x82CBFAB8 | zero storage (bandtrack, rb3_app); `Net TheNet;` (m6) | EQUAL bytes; unconstructed by design |

### 3.4 References and pointers retail initialises to an object

| global | retail slot → value | native | verdict |
|---|---|---|---|
| `TheRnd` | 0x82C76B68 → 0x82E04B38 | engine object in render/app; 128-byte zero (null ref) elsewhere | DIFFERS by design (no renderer) |
| `TheNgRnd` | 0x82C76B6C → 0x82E04B38 | render real; `*nullptr` in milo | DIFFERS by design |
| `TheShaderMgr` | 0x82C76CE0 → 0x82E04EF8 | render real; `*nullptr` in milo | DIFFERS by design |
| `TheContentMgr` | 0x82C71A14 → 0x82CCA984 | glue object in app/render; null ref elsewhere | DIFFERS by design |
| `TheServer` | 0x82C6EB50 → 0x82CBFD14 | null ref | DIFFERS by design (no Quazal) |
| `TheEntityUploader` | 0x82C716F0 → 0x82CC97A4 | null ref | DIFFERS by design |
| `TheUI` | 0x82C721F0 → 0x82DFD2B0 (= `TheBandUI`) | app: real `= &TheBandUI` (BandUI.cpp:45); null in milo/render (no constructed BandUI) | EQUAL in app; DIFFERS by design elsewhere |
| `TheAuditionMgr` | 0x82C7292C → 0x82DFDDBC | null (rb3_app_link_stubs.cpp:63) | **DIFFERS, reported (rb3_app owned)** |

"By design" means the file documents a loud null standing in for an object the target does not link. Pointing it
at unconstructed storage would be worse.

### 3.5 Not located

No X360 TU that is compiled and paired reads these, and no string anchor found them:

- `TheGameData`, `TheGestureMgr`, `TheHamDirector`: DC3 gesture/hamobj; RB3 retail has no such code.
- `TheWebSvcMgr`, `TheHAQMgr`: HAQManager rows are unpaired.
- `gBinkMovieSys`.
- `TheWiiFriendMgr`, `TheWiiProfileMgr`, `TheWiiFriendsProvider`: Wii residue.
- `play_msg`, `show_invitation_notification_msg`, `hide_invitation_notification_msg`.

Native holds all of them zero or null.

## 4. Not done

- No native build and no health selftest (the coordinator's). Both `.s` files assemble with `clang -c`, with and
  without the preprocessor, and `nm -S` shows the new sizes (256, 0x400, 0x400).
- No X360 A/B: no `objects.json` TU changed, so the delta is 0 by construction.
- The `Symbol` class and `sParsers` construction (§1) are left for the coordinator. `rb3_app_link_stubs.*` is
  untouched.
- typeinfo/vtable stubs (`dta_link_stubs.s` RndPollable/WorldDir/RndGroup/RndAnimFilter; `m8_link_stubs.cpp`
  BandUser typeinfo, PlayerStatsMsg/PlayerGameplayMsg vtables) carry no retail initial value and were not compared.
- Tools: `~/tmp/stubglobals/t/{coff,scan,resolve,pe,elfval,xref}.py`. Data: `refs.json`, `resolved.json`,
  `native_vals.json`.
