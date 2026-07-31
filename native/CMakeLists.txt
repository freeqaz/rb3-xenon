cmake_minimum_required(VERSION 3.16)
project(rb3-xenon-native CXX C ASM)

# Requires Clang — the Milo engine is decompiled MSVC code and needs MSVC
# compatibility flags (-fms-extensions, -fms-compatibility) to compile.
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(WARNING "rb3-xenon native build expects Clang; got ${CMAKE_CXX_COMPILER_ID}")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

set(REPO_ROOT ${CMAKE_SOURCE_DIR}/..)

# --- MSVC-compat + decomp flags (mirrors dc3-decomp native build) ---
set(MS_COMPAT_FLAGS
    -fms-extensions
    -fms-compatibility
    -fms-compatibility-version=19.29
    -fdelayed-template-parsing
    -D__GNUC_STDC_INLINE__
    -D__GCC_ATOMIC_TEST_AND_SET_TRUEVAL=1
)
set(ATOMIC_COMPAT_FLAGS
    -D__GCC_ATOMIC_BOOL_LOCK_FREE=2
    -D__GCC_ATOMIC_CHAR_LOCK_FREE=2
    -D__GCC_ATOMIC_WCHAR_T_LOCK_FREE=2
    -D__GCC_ATOMIC_CHAR16_T_LOCK_FREE=2
    -D__GCC_ATOMIC_CHAR32_T_LOCK_FREE=2
    -D__GCC_ATOMIC_SHORT_LOCK_FREE=2
    -D__GCC_ATOMIC_INT_LOCK_FREE=2
    -D__GCC_ATOMIC_LONG_LOCK_FREE=2
    -D__GCC_ATOMIC_LLONG_LOCK_FREE=2
    -D__GCC_ATOMIC_POINTER_LOCK_FREE=2
)
set(DECOMP_FLAGS
    -ferror-limit=0
    -fno-omit-frame-pointer
    -w
    "SHELL:-include ${CMAKE_SOURCE_DIR}/src/msvc_compat.h"
)

# --- Diagnostic: which link STUBS actually execute? --------------------------
# The native build satisfies off-path symbols with hand-written stubs whose
# comments ASSERT they are never reached ("NONE is reached on the scoring
# path"). A stub that IS reached silently replaces real engine behaviour with a
# no-op, making a demo's output partly fictional -- and the X360 match build
# cannot see it, because it never links or runs. This option compiles ONLY the
# stub TUs with -finstrument-functions and links a recorder that dumps the
# distinct entered functions; resolve the addresses offline (see
# src/cc5_stub_probe.c). Off by default; a normal build is unaffected.
option(RB3_STUB_PROBE "Instrument link-stub TUs to record which stubs execute" OFF)
set(RB3_STUB_TUS
    ${CMAKE_SOURCE_DIR}/src/m6_link_stubs.cpp
    ${CMAKE_SOURCE_DIR}/src/m8_link_stubs.cpp
    ${CMAKE_SOURCE_DIR}/src/m10_link_stubs.cpp
    ${CMAKE_SOURCE_DIR}/src/m10_leaf_stubs.cpp
    ${CMAKE_SOURCE_DIR}/src/m4_save_link_stubs.cpp
    ${CMAKE_SOURCE_DIR}/src/native_undecomp_stubs.cpp
    ${CMAKE_SOURCE_DIR}/src/native_job_stubs.cpp
)
if(RB3_STUB_PROBE)
    set_source_files_properties(${RB3_STUB_TUS} PROPERTIES
        COMPILE_OPTIONS "-finstrument-functions")
    message(STATUS "[stub-probe] ENABLED on ${CMAKE_SOURCE_DIR}/src/*_link_stubs.cpp")
endif()

# Scatter-include handling: the decomp has ~250 .cpp-includes-.cpp edges to
# reproduce retail COMDAT placement. rb3_add_executable() derives, per target,
# which of those files must NOT also be compiled standalone. See the module.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ScatterIncludes.cmake)

# --- Engine core: the subset needed for DTA parsing ---
# obj (DataNode/DataArray/Object), utl (MemMgr/Str/Symbol/streams),
# os (File/Debug/System), math (already known-good).
# CONFIGURE_DEPENDS: a bare file(GLOB) expands once and never re-runs, so a
# newly added engine .cpp would be silently missing until someone deleted the
# build directory.
file(GLOB ENGINE_OBJ  CONFIGURE_DEPENDS ${REPO_ROOT}/src/system/obj/*.cpp)
file(GLOB ENGINE_UTL  CONFIGURE_DEPENDS ${REPO_ROOT}/src/system/utl/*.cpp)
file(GLOB ENGINE_OS   CONFIGURE_DEPENDS ${REPO_ROOT}/src/system/os/*.cpp)
file(GLOB ENGINE_MATH CONFIGURE_DEPENDS ${REPO_ROOT}/src/system/math/*.cpp)
set(ENGINE_SOURCES ${ENGINE_OBJ} ${ENGINE_UTL} ${ENGINE_OS} ${ENGINE_MATH})

# Drop platform-specific TUs — native shims replace them.
list(FILTER ENGINE_SOURCES EXCLUDE REGEX "_(Xbox|Win|Wii|X360)\\.cpp$")
list(FILTER ENGINE_SOURCES EXCLUDE REGEX "(Dx9|DX9|Wgpu|Gpu|D3D|Xenon)")
# Case-variant duplicates (dc3 ships both Vec.cpp/vec.cpp etc.) — keep lowercase.
list(FILTER ENGINE_SOURCES EXCLUDE REGEX "/(Vec|Mtx)\\.cpp$")

# Flex-generated DTA lexer (C source).
set(DTA_LEXER ${REPO_ROOT}/src/system/obj/DataFlex.c)

# rndobj/Anim.cpp: not part of the obj/utl/os/math engine globs, but utl/Song.cpp
# derives Song from RndAnimatable and emits its vtable, which references the
# virtual thunks to RndAnimatable::Handle / ::SyncProperty. Those thunks (and the
# real method bodies) live in Anim.cpp's BEGIN_HANDLERS/BEGIN_PROPSYNCS blocks, so
# the native link needs this one TU. Compiles clean against the engine subset.
set(RNDOBJ_SOURCES ${REPO_ROOT}/src/system/rndobj/Anim.cpp)

# --- Native platform shims (from dc3-decomp) ---
file(GLOB PLATFORM_SHIMS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/platform/*.cpp)
set(NATIVE_SHIMS
    ${CMAKE_SOURCE_DIR}/src/xdk_shims.cpp
    ${CMAKE_SOURCE_DIR}/src/native_link_glue.cpp
    ${CMAKE_SOURCE_DIR}/src/thunk_stubs.cpp
    ${CMAKE_SOURCE_DIR}/src/native_job_stubs.cpp
    ${CMAKE_SOURCE_DIR}/src/native_undecomp_stubs.cpp
    ${PLATFORM_SHIMS}
)
if(RB3_STUB_PROBE)
    list(APPEND NATIVE_SHIMS ${CMAKE_SOURCE_DIR}/src/cc5_stub_probe.c)
endif()

rb3_add_executable(rb3-dta
    ${CMAKE_SOURCE_DIR}/src/main_dta.cpp
    ${ENGINE_SOURCES}
    ${RNDOBJ_SOURCES}
    ${DTA_LEXER}
    ${NATIVE_SHIMS}
    ${CMAKE_SOURCE_DIR}/src/dta_link_stubs.s
)
target_compile_options(rb3-dta PRIVATE ${MS_COMPAT_FLAGS} ${ATOMIC_COMPAT_FLAGS} ${DECOMP_FLAGS})
target_compile_definitions(rb3-dta PRIVATE HX_NATIVE=1 MILO_DEBUG=1 _DEBUG=1)
# native/include shadows libc++ __wrap_iter (MSVC STL iterator->pointer compat).
target_include_directories(rb3-dta SYSTEM BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(rb3-dta PRIVATE
    ${CMAKE_SOURCE_DIR}/src          # native shims + stl/ STLport-style shim headers
    ${REPO_ROOT}/src
    ${REPO_ROOT}/src/system
)
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
target_link_libraries(rb3-dta PRIVATE Threads::Threads ZLIB::ZLIB)

# Rendering/MIDI/synth/Win32 symbols referenced by compiled-but-unused engine
# vtables are satisfied by weak no-op stubs in src/dta_link_stubs.s (none are
# on the DTA-parse path).

# --- M1: SongMgr / BandSongMgr cluster ---------------------------------------
# The engine base (meta/) is NOT in the obj/utl/os/math globs, so list the three
# TUs the song-manager path needs explicitly. The rest of meta/ is UI/net code
# that doesn't compile natively and isn't on the load path.
set(META_SONG_SOURCES
    ${REPO_ROOT}/src/system/meta/SongMgr.cpp
    ${REPO_ROOT}/src/system/meta/SongMetadata.cpp
    ${REPO_ROOT}/src/system/meta/DataArraySongInfo.cpp
)
# RB3 game-layer song manager + metadata (src/band3/meta_band). MetaPerformer /
# MusicLibrary are NOT wired: they aren't on the load path (off-path symbols are
# link-stubbed in dta_link_stubs.s) and don't yet compile natively (math/Key.h
# Symbol call-operator + UILabel protected-member issues).
set(M1_SOURCES
    ${REPO_ROOT}/src/band3/meta_band/BandSongMgr.cpp
    ${REPO_ROOT}/src/band3/meta_band/BandSongMetadata.cpp
)

rb3_add_executable(rb3-song
    ${CMAKE_SOURCE_DIR}/src/main_song.cpp
    ${CMAKE_SOURCE_DIR}/src/m1_symbols.cpp
    ${ENGINE_SOURCES}
    ${RNDOBJ_SOURCES}
    ${META_SONG_SOURCES}
    ${M1_SOURCES}
    ${DTA_LEXER}
    ${NATIVE_SHIMS}
    ${CMAKE_SOURCE_DIR}/src/dta_link_stubs.s
    ${CMAKE_SOURCE_DIR}/src/m1_link_stubs.s
)
target_compile_options(rb3-song PRIVATE ${MS_COMPAT_FLAGS} ${ATOMIC_COMPAT_FLAGS} ${DECOMP_FLAGS})
target_compile_definitions(rb3-song PRIVATE HX_NATIVE=1 MILO_DEBUG=1 _DEBUG=1)
target_include_directories(rb3-song SYSTEM BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(rb3-song PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${REPO_ROOT}/src
    ${REPO_ROOT}/src/system
    ${REPO_ROOT}/src/band3     # M1: game-layer meta_band/ headers
    ${REPO_ROOT}/src/network   # M1: BandSongMgr pulls network/ headers transitively
)
target_link_libraries(rb3-song PRIVATE Threads::Threads ZLIB::ZLIB)

# --- M2: MIDI parser cluster -------------------------------------------------
# The midi/ TUs are not in the obj/utl/os/math globs; list them explicitly. They
# compile clean against the engine subset (MidiReader/MidiParser/MidiParserMgr/
# DataEventList/MidiReceiver/MidiVarLen). TempoMap/MeasureMap/BeatMap/
# TimeConversion come from the utl glob. gems=null on the load path, so no
# beatmatch/ TU is needed (GemListInterface is a header-only pure interface).
set(M2_SOURCES
    ${REPO_ROOT}/src/system/midi/MidiReader.cpp
    ${REPO_ROOT}/src/system/midi/MidiReceiver.cpp
    ${REPO_ROOT}/src/system/midi/MidiParser.cpp
    ${REPO_ROOT}/src/system/midi/MidiParserMgr.cpp
    ${REPO_ROOT}/src/system/midi/DataEventList.cpp
    ${REPO_ROOT}/src/system/midi/MidiVarLen.cpp
)

rb3_add_executable(rb3-midi
    ${CMAKE_SOURCE_DIR}/src/main_midi.cpp
    ${ENGINE_SOURCES}
    ${RNDOBJ_SOURCES}
    ${M2_SOURCES}
    ${DTA_LEXER}
    ${NATIVE_SHIMS}
    ${CMAKE_SOURCE_DIR}/src/dta_link_stubs.s
    ${CMAKE_SOURCE_DIR}/src/m2_link_stubs.s
)
target_compile_options(rb3-midi PRIVATE ${MS_COMPAT_FLAGS} ${ATOMIC_COMPAT_FLAGS} ${DECOMP_FLAGS})
target_compile_definitions(rb3-midi PRIVATE HX_NATIVE=1 MILO_DEBUG=1 _DEBUG=1)
target_include_directories(rb3-midi SYSTEM BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(rb3-midi PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${REPO_ROOT}/src
    ${REPO_ROOT}/src/system
)
target_link_libraries(rb3-midi PRIVATE Threads::Threads ZLIB::ZLIB)

# --- M3a: beatmatch gem pipeline ---------------------------------------------
# SongParser (the real gem parser, a MidiReceiver) + the concrete gem containers
# (GameGem / GameGemList / GameGemDB, ported from the rb3-Wii oracle). Reuses the
# M2 MIDI cluster (MidiReader drives SongParser; MidiParserMgr fires the gem
# branch). BeatMatchUtl / SongInfoCopy come in via M3_SOURCES; the rest of
# beatmatch/ (BeatMatcher, TrackWatcher*, controllers) is NOT on the parse path
# and is satisfied by weak stubs in m3_link_stubs.s.
set(M3_SOURCES
    ${REPO_ROOT}/src/system/beatmatch/SongParser.cpp
    ${REPO_ROOT}/src/system/beatmatch/GameGem.cpp
    ${REPO_ROOT}/src/system/beatmatch/GameGemList.cpp
    ${REPO_ROOT}/src/system/beatmatch/GameGemDB.cpp
    ${REPO_ROOT}/src/system/beatmatch/BeatMatchUtl.cpp
    ${REPO_ROOT}/src/system/beatmatch/TrackType.cpp
    ${REPO_ROOT}/src/system/utl/SongInfoCopy.cpp
)

rb3_add_executable(rb3-gem
    ${CMAKE_SOURCE_DIR}/src/main_gem.cpp
    ${CMAKE_SOURCE_DIR}/src/m3_symbols.cpp
    ${ENGINE_SOURCES}
    ${RNDOBJ_SOURCES}
    ${M2_SOURCES}
    ${M3_SOURCES}
    ${DTA_LEXER}
    ${NATIVE_SHIMS}
    ${CMAKE_SOURCE_DIR}/src/dta_link_stubs.s
    ${CMAKE_SOURCE_DIR}/src/m2_link_stubs.s
    ${CMAKE_SOURCE_DIR}/src/m3_link_stubs.s
)
target_compile_options(rb3-gem PRIVATE ${MS_COMPAT_FLAGS} ${ATOMIC_COMPAT_FLAGS} ${DECOMP_FLAGS})
target_compile_definitions(rb3-gem PRIVATE HX_NATIVE=1 MILO_DEBUG=1 _DEBUG=1)
target_include_directories(rb3-gem SYSTEM BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(rb3-gem PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${REPO_ROOT}/src
    ${REPO_ROOT}/src/system
)
target_link_libraries(rb3-gem PRIVATE Threads::Threads ZLIB::ZLIB)

# --- M3b: beatmatch hit detection --------------------------------------------
# Extends the M3a gem pipeline with the real scoring engine: TrackWatcher +
# TrackWatcherImpl + the guitar/bass 5-fret impl (BaseGuitar/Guitar).
# NewTrackWatcherImpl is HX_NATIVE-guarded to the guitar impl, so no other
# per-instrument impl is compiled.
#
# M4: the REAL SongData.cpp is now wired (was a minimal stand-in). It brings the
# full chart surface — phrases, fills, roll/trill lanes, drum-mix, disco-flip,
# backup tracks, PlayerTrackConfigList-driven per-player difficulty. The six
# dependency TUs it needs (DrumMap/DrumMixDB/PhraseDB/PhraseAnalyzer/
# PlayerTrackConfigList/FillInfo) were ported from the rb3-Wii oracle; PhraseList
# + VocalNoteList + TimeSpanVector are also pulled in (SongData's ctor news a
# VocalNoteList; PostLoadVocals + IntersectTimeSpans need the rest).
# beatmatch_native_support.cpp is now only the LogFile shim + TheBeatMatchOutput.
set(M3B_SOURCES
    ${REPO_ROOT}/src/system/beatmatch/TrackWatcher.cpp
    ${REPO_ROOT}/src/system/beatmatch/TrackWatcherImpl.cpp
    ${REPO_ROOT}/src/system/beatmatch/BaseGuitarTrackWatcherImpl.cpp
    ${REPO_ROOT}/src/system/beatmatch/GuitarTrackWatcherImpl.cpp
    ${REPO_ROOT}/src/system/beatmatch/SongData.cpp
    ${REPO_ROOT}/src/system/beatmatch/DrumMap.cpp
    ${REPO_ROOT}/src/system/beatmatch/DrumMixDB.cpp
    ${REPO_ROOT}/src/system/beatmatch/PhraseDB.cpp
    ${REPO_ROOT}/src/system/beatmatch/PhraseList.cpp
    ${REPO_ROOT}/src/system/beatmatch/PhraseAnalyzer.cpp
    ${REPO_ROOT}/src/system/beatmatch/PlayerTrackConfigList.cpp
    ${REPO_ROOT}/src/system/beatmatch/FillInfo.cpp
    ${REPO_ROOT}/src/system/beatmatch/TimeSpanVector.cpp
    ${REPO_ROOT}/src/system/beatmatch/VocalNoteList.cpp
    ${CMAKE_SOURCE_DIR}/src/beatmatch_native_support.cpp
)

rb3_add_executable(rb3-hit
    ${CMAKE_SOURCE_DIR}/src/main_hit.cpp
    ${CMAKE_SOURCE_DIR}/src/m3_symbols.cpp
    ${ENGINE_SOURCES}
    ${RNDOBJ_SOURCES}
    ${M2_SOURCES}
    ${M3_SOURCES}
    ${M3B_SOURCES}
    ${DTA_LEXER}
    ${NATIVE_SHIMS}
    ${CMAKE_SOURCE_DIR}/src/dta_link_stubs.s
    ${CMAKE_SOURCE_DIR}/src/m2_link_stubs.s
    ${CMAKE_SOURCE_DIR}/src/m3_link_stubs.s
    ${CMAKE_SOURCE_DIR}/src/m3b_link_stubs.s
)
target_compile_options(rb3-hit PRIVATE ${MS_COMPAT_FLAGS} ${ATOMIC_COMPAT_FLAGS} ${DECOMP_FLAGS})
target_compile_definitions(rb3-hit PRIVATE HX_NATIVE=1 MILO_DEBUG=1 _DEBUG=1)
target_include_directories(rb3-hit SYSTEM BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(rb3-hit PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${REPO_ROOT}/src
    ${REPO_ROOT}/src/system
    ${REPO_ROOT}/src/band3   # M4: real SongData->BeatMatcher.h pulls game/Player.h
)
target_link_libraries(rb3-hit PRIVATE Threads::Threads ZLIB::ZLIB)

# --- M5: game-layer scorer ---------------------------------------------------
# Extends the M4 hit pipeline (identical sources) with the RB3 game-layer
# scorer: real TrackWatcher Hit/Miss/Pass verdicts drive a faithful native
# ScoreState (src/score_engine.*) implementing the real Performer/Player/Scoring/
# Stats formulas over the real config/scoring.dta values, producing genuine
# score / streak multiplier / overdrive meter / solo star rating. The full
# GemPlayer (Player -> Performer + Band/BandUser/BeatMaster/CrowdMeter/meta_band/
# net/tour object graph) is NOT constructed here — that is the M6 frontier; both
# it and the real Scoring/Stats TUs compile natively but drag that cascade at
# link time. See src/score_engine.h for the per-formula oracle provenance.
rb3_add_executable(rb3-score
    ${CMAKE_SOURCE_DIR}/src/main_score.cpp
    ${CMAKE_SOURCE_DIR}/src/score_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/m3_symbols.cpp
    ${ENGINE_SOURCES}
    ${RNDOBJ_SOURCES}
    ${M2_SOURCES}
    ${M3_SOURCES}
    ${M3B_SOURCES}
    ${DTA_LEXER}
    ${NATIVE_SHIMS}
    ${CMAKE_SOURCE_DIR}/src/dta_link_stubs.s
    ${CMAKE_SOURCE_DIR}/src/m2_link_stubs.s
    ${CMAKE_SOURCE_DIR}/src/m3_link_stubs.s
    ${CMAKE_SOURCE_DIR}/src/m3b_link_stubs.s
)
target_compile_options(rb3-score PRIVATE ${MS_COMPAT_FLAGS} ${ATOMIC_COMPAT_FLAGS} ${DECOMP_FLAGS})
target_compile_definitions(rb3-score PRIVATE HX_NATIVE=1 MILO_DEBUG=1 _DEBUG=1)
target_include_directories(rb3-score SYSTEM BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(rb3-score PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${REPO_ROOT}/src
    ${REPO_ROOT}/src/system
    ${REPO_ROOT}/src/band3
)
target_link_libraries(rb3-score PRIVATE Threads::Threads ZLIB::ZLIB)

# --- M6: REAL scoring object graph -------------------------------------------
# Replaces M5's interim score_engine transcription with the genuine band3
# scoring TUs — Performer / Player / Scoring / Stats / Band — driven through
# their real methods (see src/main_score2.cpp for the graph census). Those TUs
# reference a cascade of game/net/UI singletons the scoring path never executes;
# m6_link_stubs.cpp / .s satisfy them (honest leaf stubs). score_engine.cpp is
# linked only for the side-by-side cross-check.
set(M6_GAME_SOURCES
    ${REPO_ROOT}/src/band3/game/Performer.cpp
    ${REPO_ROOT}/src/band3/game/Player.cpp
    ${REPO_ROOT}/src/band3/game/Scoring.cpp
    ${REPO_ROOT}/src/band3/game/Stats.cpp
    ${REPO_ROOT}/src/band3/game/Band.cpp
)
rb3_add_executable(rb3-score2
    ${CMAKE_SOURCE_DIR}/src/main_score2.cpp
    ${CMAKE_SOURCE_DIR}/src/score_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/m3_symbols.cpp
    ${M6_GAME_SOURCES}
    ${ENGINE_SOURCES}
    ${RNDOBJ_SOURCES}
    ${M2_SOURCES}
    ${M3_SOURCES}
    ${M3B_SOURCES}
    ${DTA_LEXER}
    ${NATIVE_SHIMS}
    ${CMAKE_SOURCE_DIR}/src/m6_link_stubs.cpp
    ${CMAKE_SOURCE_DIR}/src/m6_symbols.cpp
    ${CMAKE_SOURCE_DIR}/src/dta_link_stubs.s
    ${CMAKE_SOURCE_DIR}/src/m2_link_stubs.s
    ${CMAKE_SOURCE_DIR}/src/m3_link_stubs.s
    ${CMAKE_SOURCE_DIR}/src/m3b_link_stubs.s
)
target_compile_options(rb3-score2 PRIVATE ${MS_COMPAT_FLAGS} ${ATOMIC_COMPAT_FLAGS} ${DECOMP_FLAGS}
    -ffunction-sections -fdata-sections)
# Drop unreferenced methods of the real scoring TUs (and their off-path link
# refs) so the stub surface is only what the scoring chain actually reaches.
target_link_options(rb3-score2 PRIVATE -Wl,--gc-sections)
target_compile_definitions(rb3-score2 PRIVATE HX_NATIVE=1 MILO_DEBUG=1 _DEBUG=1)
target_include_directories(rb3-score2 SYSTEM BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(rb3-score2 PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${REPO_ROOT}/src
    ${REPO_ROOT}/src/system
    ${REPO_ROOT}/src/band3
    ${REPO_ROOT}/src/network
)
target_link_libraries(rb3-score2 PRIVATE Threads::Threads ZLIB::ZLIB)

# --- M7: the REAL overdrive / band-energy cycle ------------------------------
# Same object graph as rb3-score2 (real Player/Band/Scoring/Stats + the M2/M3/M3b
# beatmatch parse pipeline) but the driver closes the overdrive loop: note-116
# OD phrases parse into the real SongData/PhraseList, phrase completion credits
# energy via the real Player::CompleteCommonPhrase/AddEnergy, and deploy runs the
# real Band::UpdateBonusLevel/EnergyMultiplier multiplier machinery (not the shim).
rb3_add_executable(rb3-score3
    ${CMAKE_SOURCE_DIR}/src/main_score3.cpp
    ${CMAKE_SOURCE_DIR}/src/score_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/m3_symbols.cpp
    ${M6_GAME_SOURCES}
    ${ENGINE_SOURCES}
    ${RNDOBJ_SOURCES}
    ${M2_SOURCES}
    ${M3_SOURCES}
    ${M3B_SOURCES}
    ${DTA_LEXER}
    ${NATIVE_SHIMS}
    ${CMAKE_SOURCE_DIR}/src/m6_link_stubs.cpp
    ${CMAKE_SOURCE_DIR}/src/m6_symbols.cpp
    ${CMAKE_SOURCE_DIR}/src/dta_link_stubs.s
    ${CMAKE_SOURCE_DIR}/src/m2_link_stubs.s
    ${CMAKE_SOURCE_DIR}/src/m3_link_stubs.s
    ${CMAKE_SOURCE_DIR}/src/m3b_link_stubs.s
)
target_compile_options(rb3-score3 PRIVATE ${MS_COMPAT_FLAGS} ${ATOMIC_COMPAT_FLAGS} ${DECOMP_FLAGS}
    -ffunction-sections -fdata-sections)
target_link_options(rb3-score3 PRIVATE -Wl,--gc-sections)
target_compile_definitions(rb3-score3 PRIVATE HX_NATIVE=1 MILO_DEBUG=1 _DEBUG=1)
target_include_directories(rb3-score3 SYSTEM BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(rb3-score3 PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${REPO_ROOT}/src
    ${REPO_ROOT}/src/system
    ${REPO_ROOT}/src/band3
    ${REPO_ROOT}/src/network
)
target_link_libraries(rb3-score3 PRIVATE Threads::Threads ZLIB::ZLIB)

# --- M8: the full-song run-through (clock-driven) ----------------------------
# Same real object graph as rb3-score3 (Player/Band/Scoring/Stats/Performer + the
# M2/M3/M3b beatmatch parse pipeline) but the driver runs an advancing clock: the
# REAL SongData::CalcSongPos turns the headless-audio clock into a SongPos and the
# REAL Player::Poll re-evaluates scoring/energy every frame, start-to-finish. Adds
# the PORTED CommonPhraseCapturer (X360-inert; drives real overdrive credit) plus
# the M8 support/stub layer (minimal real SongDB backed by SongData, Game hooks,
# headless clock). m8_link_stubs replaces m6_link_stubs (four symbols promoted to
# real behavior); m6_symbols still supplies the singleton globals.
rb3_add_executable(rb3-score4
    ${CMAKE_SOURCE_DIR}/src/main_score4.cpp
    ${CMAKE_SOURCE_DIR}/src/m3_symbols.cpp
    ${REPO_ROOT}/src/band3/game/CommonPhraseCapturer.cpp
    ${M6_GAME_SOURCES}
    ${ENGINE_SOURCES}
    ${RNDOBJ_SOURCES}
    ${M2_SOURCES}
    ${M3_SOURCES}
    ${M3B_SOURCES}
    ${DTA_LEXER}
    ${NATIVE_SHIMS}
    ${CMAKE_SOURCE_DIR}/src/m8_support.cpp
    ${CMAKE_SOURCE_DIR}/src/m8_link_stubs.cpp
    ${CMAKE_SOURCE_DIR}/src/m6_symbols.cpp
    ${CMAKE_SOURCE_DIR}/src/dta_link_stubs.s
    ${CMAKE_SOURCE_DIR}/src/m2_link_stubs.s
    ${CMAKE_SOURCE_DIR}/src/m3_link_stubs.s
    ${CMAKE_SOURCE_DIR}/src/m3b_link_stubs.s
)
target_compile_options(rb3-score4 PRIVATE ${MS_COMPAT_FLAGS} ${ATOMIC_COMPAT_FLAGS} ${DECOMP_FLAGS}
    -ffunction-sections -fdata-sections)
target_link_options(rb3-score4 PRIVATE -Wl,--gc-sections)
target_compile_definitions(rb3-score4 PRIVATE HX_NATIVE=1 MILO_DEBUG=1 _DEBUG=1)
target_include_directories(rb3-score4 SYSTEM BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(rb3-score4 PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${REPO_ROOT}/src
    ${REPO_ROOT}/src/system
    ${REPO_ROOT}/src/band3
    ${REPO_ROOT}/src/network
)
target_link_libraries(rb3-score4 PRIVATE Threads::Threads ZLIB::ZLIB)

# --- M9: vocal pitch-track run-through ----------------------------------------
# Parses a real chart's PART VOCALS through the REAL SongParser -> SongData ->
# VocalNoteList path (same Stage-1 as M8), prints the vocal note/phrase/lyric
# table, then drives a synthetic singer through the REAL VocalPart pitch-matching
# engine (ScoreNote / GetBestHit / CalcNoteWeights / CalcPhraseScoreMax). The
# mic input is synthetic; all matching/scoring is real engine code. VocalPart.cpp
# is added on top of the M8 object graph; unused Poll/Singer/VocalPlayer code is
# dropped by --gc-sections. See native/src/main_vocal.cpp for the census.
rb3_add_executable(rb3-vocal
    ${CMAKE_SOURCE_DIR}/src/main_vocal.cpp
    ${CMAKE_SOURCE_DIR}/src/m3_symbols.cpp
    ${REPO_ROOT}/src/band3/game/VocalPart.cpp
    ${REPO_ROOT}/src/band3/game/CommonPhraseCapturer.cpp
    ${M6_GAME_SOURCES}
    ${ENGINE_SOURCES}
    ${RNDOBJ_SOURCES}
    ${M2_SOURCES}
    ${M3_SOURCES}
    ${M3B_SOURCES}
    ${DTA_LEXER}
    ${NATIVE_SHIMS}
    ${CMAKE_SOURCE_DIR}/src/m8_support.cpp
    ${CMAKE_SOURCE_DIR}/src/m8_link_stubs.cpp
    ${CMAKE_SOURCE_DIR}/src/m6_symbols.cpp
    ${CMAKE_SOURCE_DIR}/src/dta_link_stubs.s
    ${CMAKE_SOURCE_DIR}/src/m2_link_stubs.s
    ${CMAKE_SOURCE_DIR}/src/m3_link_stubs.s
    ${CMAKE_SOURCE_DIR}/src/m3b_link_stubs.s
)
target_compile_options(rb3-vocal PRIVATE ${MS_COMPAT_FLAGS} ${ATOMIC_COMPAT_FLAGS} ${DECOMP_FLAGS}
    -ffunction-sections -fdata-sections)
target_link_options(rb3-vocal PRIVATE -Wl,--gc-sections)
target_compile_definitions(rb3-vocal PRIVATE HX_NATIVE=1 MILO_DEBUG=1 _DEBUG=1)
target_include_directories(rb3-vocal SYSTEM BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(rb3-vocal PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${REPO_ROOT}/src
    ${REPO_ROOT}/src/system
    ${REPO_ROOT}/src/band3
    ${REPO_ROOT}/src/network
)
target_link_libraries(rb3-vocal PRIVATE Threads::Threads ZLIB::ZLIB)

# --- M10: full vocal-gameplay orchestration ----------------------------------
# The REAL VocalPlayer::Poll each frame: real Singers, real VocalParts, greedy
# singer<->part assignment, real ScoreSinger / phrase accumulation / phrase-end
# rating. Adds the three real vocal game TUs (VocalPlayer/Singer/VocalPart +
# TambourineManager) on top of the M8 object graph, plus faithful rb3-Wii-oracle
# ports of the leaf helpers (VibratoDetector, TambourineDetector, VocalScoreHistory,
# VoiceBeat/TalkyMatcher) and m10_support (synthetic GameMic/GameMicManager +
# vocal SongDB queries). m10_link_stubs replaces m8_link_stubs (drops the
# GetVocalNoteListCount->0 stub, adds NetSession::IsLocal). Only the mic is
# synthetic; all scoring is real. See native/src/main_vocal2.cpp for the census.
rb3_add_executable(rb3-vocal2
    ${CMAKE_SOURCE_DIR}/src/main_vocal2.cpp
    ${CMAKE_SOURCE_DIR}/src/m3_symbols.cpp
    ${REPO_ROOT}/src/band3/game/VocalPlayer.cpp
    ${REPO_ROOT}/src/band3/game/Singer.cpp
    ${REPO_ROOT}/src/band3/game/VocalPart.cpp
    ${REPO_ROOT}/src/band3/game/TambourineManager.cpp
    ${REPO_ROOT}/src/band3/game/VocalScoreHistory.cpp
    ${REPO_ROOT}/src/band3/game/TambourineDetector.cpp
    ${REPO_ROOT}/src/system/dsp/VibratoDetector.cpp
    ${REPO_ROOT}/src/system/synth/VoiceBeat.cpp
    ${REPO_ROOT}/src/band3/game/CommonPhraseCapturer.cpp
    ${M6_GAME_SOURCES}
    ${ENGINE_SOURCES}
    ${RNDOBJ_SOURCES}
    ${M2_SOURCES}
    ${M3_SOURCES}
    ${M3B_SOURCES}
    ${DTA_LEXER}
    ${NATIVE_SHIMS}
    ${CMAKE_SOURCE_DIR}/src/m8_support.cpp
    ${CMAKE_SOURCE_DIR}/src/m10_support.cpp
    ${CMAKE_SOURCE_DIR}/src/m10_leaf_stubs.cpp
    ${CMAKE_SOURCE_DIR}/src/m10_link_stubs.cpp
    ${CMAKE_SOURCE_DIR}/src/m6_symbols.cpp
    ${CMAKE_SOURCE_DIR}/src/dta_link_stubs.s
    ${CMAKE_SOURCE_DIR}/src/m2_link_stubs.s
    ${CMAKE_SOURCE_DIR}/src/m3_link_stubs.s
    ${CMAKE_SOURCE_DIR}/src/m3b_link_stubs.s
)
target_compile_options(rb3-vocal2 PRIVATE ${MS_COMPAT_FLAGS} ${ATOMIC_COMPAT_FLAGS} ${DECOMP_FLAGS}
    -ffunction-sections -fdata-sections)
target_link_options(rb3-vocal2 PRIVATE -Wl,--gc-sections)
target_compile_definitions(rb3-vocal2 PRIVATE HX_NATIVE=1 MILO_DEBUG=1 _DEBUG=1)
target_include_directories(rb3-vocal2 SYSTEM BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(rb3-vocal2 PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${REPO_ROOT}/src
    ${REPO_ROOT}/src/system
    ${REPO_ROOT}/src/band3
    ${REPO_ROOT}/src/network
)
target_link_libraries(rb3-vocal2 PRIVATE Threads::Threads ZLIB::ZLIB)

# --- M11: multi-part HARMONY scoring topology --------------------------------
# Same real object graph as rb3-vocal2 (M10), but drives the multi-part path:
# a real RB3 harmony chart (PART VOCALS + HARM1/HARM2/HARM3) parsed with
# UseVocalHarmony() -> multiple VocalNoteLists -> multiple VocalParts, scored by
# TWO synthetic Singers through the REAL greedy singer<->part assignment
# (FindBestPart / AddSingerCandidate) and ambiguity machinery (AddAmbiguousPart /
# AllScoresAreIn / ResolveAmbiguity). Only main_harmony.cpp differs from vocal2.
# See native/src/main_harmony.cpp for the census.
rb3_add_executable(rb3-harmony
    ${CMAKE_SOURCE_DIR}/src/main_harmony.cpp
    ${CMAKE_SOURCE_DIR}/src/m3_symbols.cpp
    ${REPO_ROOT}/src/band3/game/VocalPlayer.cpp
    ${REPO_ROOT}/src/band3/game/Singer.cpp
    ${REPO_ROOT}/src/band3/game/VocalPart.cpp
    ${REPO_ROOT}/src/band3/game/TambourineManager.cpp
    ${REPO_ROOT}/src/band3/game/VocalScoreHistory.cpp
    ${REPO_ROOT}/src/band3/game/TambourineDetector.cpp
    ${REPO_ROOT}/src/system/dsp/VibratoDetector.cpp
    ${REPO_ROOT}/src/system/synth/VoiceBeat.cpp
    ${REPO_ROOT}/src/band3/game/CommonPhraseCapturer.cpp
    ${M6_GAME_SOURCES}
    ${ENGINE_SOURCES}
    ${RNDOBJ_SOURCES}
    ${M2_SOURCES}
    ${M3_SOURCES}
    ${M3B_SOURCES}
    ${DTA_LEXER}
    ${NATIVE_SHIMS}
    ${CMAKE_SOURCE_DIR}/src/m8_support.cpp
    ${CMAKE_SOURCE_DIR}/src/m10_support.cpp
    ${CMAKE_SOURCE_DIR}/src/m10_leaf_stubs.cpp
    ${CMAKE_SOURCE_DIR}/src/m10_link_stubs.cpp
    ${CMAKE_SOURCE_DIR}/src/m6_symbols.cpp
    ${CMAKE_SOURCE_DIR}/src/dta_link_stubs.s
    ${CMAKE_SOURCE_DIR}/src/m2_link_stubs.s
    ${CMAKE_SOURCE_DIR}/src/m3_link_stubs.s
    ${CMAKE_SOURCE_DIR}/src/m3b_link_stubs.s
)
target_compile_options(rb3-harmony PRIVATE ${MS_COMPAT_FLAGS} ${ATOMIC_COMPAT_FLAGS} ${DECOMP_FLAGS}
    -ffunction-sections -fdata-sections)
target_link_options(rb3-harmony PRIVATE -Wl,--gc-sections)
target_compile_definitions(rb3-harmony PRIVATE HX_NATIVE=1 MILO_DEBUG=1 _DEBUG=1)
target_include_directories(rb3-harmony SYSTEM BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(rb3-harmony PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${REPO_ROOT}/src
    ${REPO_ROOT}/src/system
    ${REPO_ROOT}/src/band3
    ${REPO_ROOT}/src/network
)
target_link_libraries(rb3-harmony PRIVATE Threads::Threads ZLIB::ZLIB)

# --- M12: the REAL crowd-rating / band-failure meter --------------------------
# Same object graph as rb3-score4 (M8), but links the REAL
# src/band3/game/CrowdRating.cpp instead of the no-op stubs, and feeds it RB3's
# genuine shipped config/scoring.dta (crowd ...) block.
#
# WHY: the stub-execution probe (RB3_STUB_PROBE, src/cc5_stub_probe.c) measured
# CrowdRating::Poll running 25,905 times in ONE rb3-score4 run -- the crowd meter
# was a no-op on the hot path while the demo reported results as if it were real.
# The implementation had been decompiled in-tree the whole time; only the native
# build stubbed it. m12_link_stubs.cpp is m8_link_stubs.cpp with the 10
# CrowdRating stubs removed so the real definitions win.
rb3_add_executable(rb3-crowd
    ${CMAKE_SOURCE_DIR}/src/main_crowd.cpp
    ${CMAKE_SOURCE_DIR}/src/m3_symbols.cpp
    ${REPO_ROOT}/src/band3/game/CrowdRating.cpp
    ${REPO_ROOT}/src/band3/game/CommonPhraseCapturer.cpp
    ${M6_GAME_SOURCES}
    ${ENGINE_SOURCES}
    ${RNDOBJ_SOURCES}
    ${M2_SOURCES}
    ${M3_SOURCES}
    ${M3B_SOURCES}
    ${DTA_LEXER}
    ${NATIVE_SHIMS}
    ${CMAKE_SOURCE_DIR}/src/m8_support.cpp
    ${CMAKE_SOURCE_DIR}/src/m12_link_stubs.cpp
    ${CMAKE_SOURCE_DIR}/src/m6_symbols.cpp
    ${CMAKE_SOURCE_DIR}/src/dta_link_stubs.s
    ${CMAKE_SOURCE_DIR}/src/m2_link_stubs.s
    ${CMAKE_SOURCE_DIR}/src/m3_link_stubs.s
    ${CMAKE_SOURCE_DIR}/src/m3b_link_stubs.s
)
target_compile_options(rb3-crowd PRIVATE ${MS_COMPAT_FLAGS} ${ATOMIC_COMPAT_FLAGS} ${DECOMP_FLAGS}
    -ffunction-sections -fdata-sections)
target_link_options(rb3-crowd PRIVATE -Wl,--gc-sections)
target_compile_definitions(rb3-crowd PRIVATE HX_NATIVE=1 MILO_DEBUG=1 _DEBUG=1 RB3_REAL_CROWD=1)
target_include_directories(rb3-crowd SYSTEM BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(rb3-crowd PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${REPO_ROOT}/src
    ${REPO_ROOT}/src/system
    ${REPO_ROOT}/src/band3
    ${REPO_ROOT}/src/network
)
target_link_libraries(rb3-crowd PRIVATE Threads::Threads ZLIB::ZLIB)

# --- M4: profile / save serialization round-trip -----------------------------
# Runs RB3's real FixedSizeSaveable machinery natively (src/system/meta) and
# round-trips two real FixedSizeSaveable subclasses (GameplayOptions, StandIn)
# plus the full versioned + symbol-table + encrypted memcard envelope
# (SaveLoadManager / HamMemcardAction flow) to a plain heap buffer. No XContent/
# STFS packaging. Off-path singletons the two saveable TUs reference but the
# serialization path never executes (TheProfileMgr, ThePlatformMgr, …) are
# satisfied by weak stubs in m4_save_link_stubs.cpp.
set(M4_SAVE_SOURCES
    ${REPO_ROOT}/src/system/meta/FixedSizeSaveable.cpp
    ${REPO_ROOT}/src/system/meta/FixedSizeSaveableStream.cpp
    ${REPO_ROOT}/src/system/meta/Profile.cpp
    ${REPO_ROOT}/src/band3/meta_band/GameplayOptions.cpp
    ${REPO_ROOT}/src/band3/meta_band/StandIn.cpp
)
rb3_add_executable(rb3-save
    ${CMAKE_SOURCE_DIR}/src/main_save.cpp
    ${M4_SAVE_SOURCES}
    ${ENGINE_SOURCES}
    ${RNDOBJ_SOURCES}
    ${DTA_LEXER}
    ${NATIVE_SHIMS}
    ${CMAKE_SOURCE_DIR}/src/m4_save_link_stubs.cpp
    ${CMAKE_SOURCE_DIR}/src/dta_link_stubs.s
)
target_compile_options(rb3-save PRIVATE ${MS_COMPAT_FLAGS} ${ATOMIC_COMPAT_FLAGS} ${DECOMP_FLAGS})
target_compile_definitions(rb3-save PRIVATE HX_NATIVE=1 MILO_DEBUG=1 _DEBUG=1)
target_include_directories(rb3-save SYSTEM BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(rb3-save PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${REPO_ROOT}/src
    ${REPO_ROOT}/src/system
    ${REPO_ROOT}/src/band3
    ${REPO_ROOT}/src/network
)
target_link_libraries(rb3-save PRIVATE Threads::Threads ZLIB::ZLIB)
