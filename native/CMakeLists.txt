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
# --- Diagnostics policy ------------------------------------------------------
# This build used to pass a blanket `-w`. The native build is the ONLY instrument
# in this project that LINKS AND RUNS the engine (the X360 match build compiles
# TUs and byte-compares them, so ODR faults, uninitialized reads and dead
# branches are structurally invisible to it) -- and `-w` had its diagnostics
# muted since the port was created. See lane CC-5 / commit dc911261.
#
# ★ `-w` CANNOT be layered on. Measured, not assumed: with clang,
#   `-w -Wreturn-type` is SILENT, and even `-w -Werror=return-type` is SILENT,
#   in either order. `-w` wins absolutely. The mechanism that does work is
#   `-Wno-everything`, which mutes just as completely but permits selective
#   re-enable (verified: `-Wno-everything -Werror=return-type` errors).
#   So `-w` -> `-Wno-everything` is a NO-OP on its own, and every flag listed
#   below is an explicit, deliberate opt-in.
#
# ★ Each flag below was MEASURED to produce ZERO diagnostics across all 251
#   unique TUs (-fsyntax-only over compile_commands.json; raw counts are a
#   fan-out artifact, so counts are of unique file:line:col:message). They are
#   promoted to -Werror so this defect class CANNOT silently return -- a warning
#   scrolls past in a 171-step build, an error does not.
#
# ⚠ DELIBERATELY NOT ENABLED (measured non-zero; each needs the
#   transcription-error-vs-faithful-shipped-bug triage against the dc3 /
#   rb3-Wii oracles BEFORE being touched -- fixing a faithful bug breaks the
#   match). Unique-site counts as of this measurement:
#     -Wshorten-64-to-32          455   (incl. the LP64 landmine)
#     -Woverloaded-virtual         36   (CC-5: 4 real, all faithful)
#     -Wpointer-to-int-cast        22 + -Wvoid-pointer-to-int-cast 6  (LP64)
#     -Wparentheses                13   (via -Wlogical-op-parentheses 7,
#                                        -Wbitwise-op-parentheses 6)
#     -Wformat                      5   (all "%s" fed a Symbol)
#     -Wint-to-pointer-cast         4 + -Wint-to-void-pointer-cast 3  (LP64:
#                                        MemTrack/PoolAlloc/MemHeap store
#                                        pointers in 32-bit words)
#     -Wreturn-type                 2   (MidiConstants.h:34, Debug.cpp:373 --
#                                        the latter deferred by CC-5, no oracle)
#     -Warray-bounds                1   (GlitchFinder.cpp:361, index 1024 into
#                                        char[1024] -- looks like a real defect)
#     -Wswitch                      1   (NetCacheLoader.cpp:57)
#     -Wenum-compare                1   (PlayerTrackConfigList.cpp:164 --
#                                        VERIFIED FAITHFUL, values align 1:1)
#     -Wunused-result               1   (VocalScoreHistory.cpp:38)
#     -Wbitwise-instead-of-logical  1   (Geo.cpp:137 -- VERIFIED FAITHFUL, the
#                                        `|` MUST NOT short-circuit, ClampEq
#                                        mutates. DO NOT "FIX".)
set(DECOMP_WARNINGS
    -Wno-everything
    -Werror=uninitialized
    -Werror=sometimes-uninitialized
    -Werror=self-assign
    -Werror=self-move
    -Werror=delete-non-virtual-dtor
    -Werror=mismatched-new-delete
    -Werror=return-stack-address
    -Werror=dangling
    -Werror=sizeof-pointer-memaccess
    -Werror=memset-transposed-args
    -Werror=infinite-recursion
    -Werror=tautological-compare
    -Werror=tautological-pointer-compare
    -Werror=nonnull
    -Werror=logical-not-parentheses
    -Werror=bitwise-conditional-parentheses
    -Werror=constant-logical-operand
    -Werror=shift-count-overflow
    -Werror=string-compare
    -Werror=unevaluated-expression
    -Werror=unsequenced
    -Werror=varargs
    -Werror=user-defined-warnings
)
set(DECOMP_FLAGS
    -ferror-limit=0
    -fno-omit-frame-pointer
    ${DECOMP_WARNINGS}
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

# --- M13: mount the REAL .ark archive ----------------------------------------
# Every other target reads loose, pre-extracted files. This one mounts RB3's
# shipped ten-part main_xbox archive through the engine's own Archive/ArkFile
# stack and proves it byte-exactly.
#
# The only new engine-facing code is src/platform/ArkRead_Native.cpp, which
# defines NativeArkRead — the single weak no-op in dta_link_stubs.s that the
# HX_NATIVE branch of src/system/os/ArkFile.cpp delegates to. It is picked up by
# the PLATFORM_SHIMS glob, so all targets link the strong definition; the other
# 13 never call it (they do not SetUsingCD(true), so NewFile never builds an
# ArkFile), leaving their behaviour unchanged.
rb3_add_executable(rb3-ark
    ${CMAKE_SOURCE_DIR}/src/main_ark.cpp
    ${ENGINE_SOURCES}
    ${RNDOBJ_SOURCES}
    ${DTA_LEXER}
    ${NATIVE_SHIMS}
    ${CMAKE_SOURCE_DIR}/src/dta_link_stubs.s
)
target_compile_options(rb3-ark PRIVATE ${MS_COMPAT_FLAGS} ${ATOMIC_COMPAT_FLAGS} ${DECOMP_FLAGS})
target_compile_definitions(rb3-ark PRIVATE HX_NATIVE=1 MILO_DEBUG=1 _DEBUG=1)
target_include_directories(rb3-ark SYSTEM BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(rb3-ark PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${REPO_ROOT}/src
    ${REPO_ROOT}/src/system
)
target_link_libraries(rb3-ark PRIVATE Threads::Threads ZLIB::ZLIB)

# =============================================================================
# X1 — milo-native-engine (shared LP64 engine), dc3 GPU flavor
# =============================================================================
# SPIKE-X0 (docs/plans/spike-x0-engine-dc3-flavor-2026-08-01.md) proved the
# engine's `dc3` GPU backend flavor COMPOSES against rb3-xenon's headers:
# 14/14 flavor-critical TUs compile, libmilo-engine.a links, ZERO source edits
# on either side, with a 5-entry MILO_ENGINE_DECOMP_PLATFORM_EXCLUDE. This block
# promotes that out-of-tree harness into the real build.
#
# WHY `dc3` AND NOT `rb3`: rb3-xenon's src/system/rndobj/ is DC3-lineage, not
# RB3-Wii-lineage — it supplies NgRnd (rndobj/Rnd_NG.h:14), RndCam::
# GetViewProjectXfms (Cam.h:46), RndParticleSys::NumTilesAcross/Down (Part.h:
# 284), and the fat RndMesh::Vert (Mesh.h:74). Every structural reason the
# engine's `rb3` BandRnd flavor exists applies to rb3-WII, not to us. So xenon
# takes the engine's most-mature backend and needs 5 platform exclusions where
# rb3-Wii needs 18.
#
# ⚠ COMPILE-COMPAT IS NOT BEHAVIOURAL COMPAT. src/system/rndobj/Rnd.h:354-360
# documents an Rnd/NgRnd member-layout shift vs the shape the DC3 headers
# assume. Nothing below instantiates WgpuRnd/NgRnd — rb3-frame deliberately
# exercises ONLY the decomp-agnostic gfx core (GpuDevice + Screenshot), so a
# silent member-offset bug cannot masquerade as a working frame. Standing up
# WgpuRnd is X3's job, on top of this known-good clear-frame baseline.
# =============================================================================
option(RB3X_BUILD_ENGINE "Build + link milo-native-engine (needs Dawn + glfw3)" ON)

set(MILO_ENGINE_PATH "${CMAKE_SOURCE_DIR}/../../milo-native-engine"
    CACHE PATH "Path to milo-native-engine checkout")
# Soft SHA pin (warn, never fail) — mirrors rb3/native/CMakeLists.txt:88-102.
# Seeded from the engine HEAD SPIKE-X0 measured against.
set(MILO_ENGINE_PIN "138e1606a202f2b3226e38a8f28010b096f3d441"
    CACHE STRING "Engine commit SHA verified against")

# Dawn (WebGPU) lives in the shared dc3 deps tree; glfw3 is system-installed.
# Defaulted (not required) so a bare `cmake -B build` finds it; override with
# -DDawn_DIR=... . If it is absent we DISABLE the engine rather than hard-fail:
# find_package(Dawn REQUIRED) inside the engine would abort configure and take
# all 15 pre-existing targets down with it on a machine without the deps tree.
if(NOT DEFINED Dawn_DIR)
    set(Dawn_DIR "${REPO_ROOT}/../dc3-decomp-deps/dawn/lib/cmake/Dawn"
        CACHE PATH "Path to Dawn's CMake package config")
endif()
if(RB3X_BUILD_ENGINE AND NOT EXISTS "${Dawn_DIR}/DawnConfig.cmake")
    message(WARNING
        "RB3X_BUILD_ENGINE=ON but no Dawn at ${Dawn_DIR} — disabling the engine "
        "(rb3-frame will not be built). Point -DDawn_DIR at a Dawn install to "
        "enable it. The other targets are unaffected.")
    set(RB3X_BUILD_ENGINE OFF)
endif()

if(RB3X_BUILD_ENGINE)
    if(EXISTS "${MILO_ENGINE_PATH}/.git")
        execute_process(COMMAND git rev-parse HEAD
            WORKING_DIRECTORY ${MILO_ENGINE_PATH}
            OUTPUT_VARIABLE _milo_engine_head OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(NOT _milo_engine_head MATCHES "^${MILO_ENGINE_PIN}")
            message(WARNING
                "milo-native-engine HEAD is ${_milo_engine_head}\n"
                "  but rb3-xenon-native pins ${MILO_ENGINE_PIN}\n"
                "  (update MILO_ENGINE_PIN once the new engine state is canonical)")
        endif()
    endif()

    # --- Context injection (verbatim from the SPIKE-X0 harness) --------------
    # Include ORDER IS LOAD-BEARING: `src` MUST precede `src/system`, or a stub
    # at src/<dir>/Foo.h shadows the real src/system/<dir>/Foo.h (the shadowing
    # hazard documented in CLAUDE.md "Build wiring"). MEASURED in X0: adding
    # src/band3, src/network or src/system/oggvorbis changes nothing — the build
    # is byte-for-byte as clean without them, so they are deliberately absent.
    # The engine injects its own include/ STL shim BEFORE PUBLIC, and it is
    # byte-identical to native/include/bits/stl_iterator.h, so no consumer-side
    # STL shim injection is needed either.
    set(MILO_ENGINE_DECOMP_INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/src        # native shims + stl/ STLport-shape shims
        ${REPO_ROOT}/src
        ${REPO_ROOT}/src/system)

    # ⚠ DECOMP_WARNINGS (the -Werror= opt-in list) is DELIBERATELY NOT PASSED to
    # the engine. That policy was measured against xenon's own 251 TUs, never
    # against the engine's. X0 measured what happens if you do pass it: exactly
    # one engine TU fails, src/platform/Mesh_Wgpu.cpp:206,:299, on
    # -Wtautological-constant-out-of-range-compare — `TheRnd.GetDrawMode() == 8`
    # against a Rnd::DrawMode enum topping out at kDrawVelocity = 6. That is a
    # REAL latent engine defect (two two-sided-cull overrides dead on every
    # consumer, incl. DC3, whose blanket `-w` hides it) and it is on the engine
    # backlog — not something to block xenon's build on. Re-enable this once the
    # engine is clean under the full policy; it is a net asset to the engine.
    set(MILO_ENGINE_DECOMP_COMPAT_FLAGS
        ${MS_COMPAT_FLAGS} ${ATOMIC_COMPAT_FLAGS}
        -ferror-limit=0
        -fno-omit-frame-pointer
        -Wno-everything
        "SHELL:-include ${CMAKE_SOURCE_DIR}/src/msvc_compat.h")
    # MILO_ENGINE_DECOMP_PCH deliberately unset: xenon's PCH (system.pch, built
    # from src/system/decomp_pch.h) is a MATCH-BUILD device whose contents are
    # codegen-load-bearing. It has nothing to do with the native build.

    # Platform TUs whose xenon header twins differ. ALL five are off the gfx
    # path and ALL have a xenon-side replacement — this is exactly what the
    # exclude mechanism exists for. Category (b) in the X0 taxonomy:
    #
    #  DataParser_Native.cpp  engine declares `extern int gDataLine`; xenon's
    #                         obj/DataFile_Flex.h:9 declares `extern DataType
    #                         gDataLine`. xenon parses DTA via DataFlex.c
    #                         compiled directly (DTA_LEXER above), so the TU is
    #                         redundant here. One-WORD type disagreement, not a
    #                         shape gap — cheap to close later.
    #  FxSendNative.cpp       xenon's EQEffect::Params lacks mBand4Q/mBand5Freq
    #                         (DC3 synth-FxSend shape). Off the render path.
    #  Joypad_Native.cpp      xenon's JoypadData lacks mNumAnalogSticks.
    #  Synth_Stub.cpp         xenon synth/VorbisReader.h ctor takes 5 args (engine
    #                         passes 4), StandardStream.h takes 6 (engine passes
    #                         7); NewStreamDecoder doesn't override.
    #  FFmpegMovieImpl.cpp    engine's FFmpegMovieImpl.h:37 `void SetPaused(bool)`
    #                         vs xenon movie/MovieImpl.h:24 `virtual bool
    #                         SetPaused(bool)`. A NEAR-twin, not a shape gap —
    #                         xenon HAS movie/MovieImpl.h (rb3-Wii does not).
    #                         See the X1 doc for why this was NOT reconciled.
    #
    # NOT excluded, and each is a tell that xenon is DC3-lineage (every one is
    # on rb3-Wii's 18-entry exclusion list): CDReader_Native, Keygen_Stub,
    # SampleInst_Native, StreamReceiver_Native, BoneSetup, CharTwistSolver,
    # FFmpegAudioReader.
    #
    # === X3: the 13 basename collisions, PAID ===============================
    # X1 §7.5 deferred them ("the first target that links the engine AND
    # NATIVE_SHIMS needs an exclude-or-delete decision on each"); rb3-render is
    # that target. The decision is UNIFORM and it is exclude-the-ENGINE's-twin,
    # for one reason: xenon's native/src/platform/ shims are what the other 16
    # targets already link and what rb3-ark's byte-exact archive gate certifies.
    # Swapping in the engine's copies would re-open a proven path for no gain.
    #
    # ⚠ The pair is not always same-named, so a basename diff UNDERCOUNTS this.
    # Keyboard_Native.cpp (engine) and Keyboard_Stub.cpp (xenon) are different
    # FILE names defining the SAME symbols; ditto Joypad_Native/Joypad_Stub
    # (already excluded above for an unrelated header gap, which happens to
    # cover it). Excluded by symbol overlap, not by name.
    #
    # ⚠ AND THE MEASURED COLLISION SET IS 10, NOT 13. X1's list of 13 was a
    # basename diff of the two DIRECTORIES; three of those engine files are not
    # in the engine's own source list at all (CMakeLists.txt:361-370 defers
    # PlatformMgr_Native.cpp, RenderState_Native.cpp and Skeleton_Native.cpp as
    # DC3 glue), so they never enter libmilo-engine.a and cannot collide. That
    # also means the engine does NOT supply TheRenderState / the 17
    # RndRenderState::Set* bodies — X2's stub-section-(2) mapping claimed it
    # does, and it is wrong on exactly that row. See milo_link_stubs.cpp.
    set(MILO_ENGINE_DECOMP_PLATFORM_EXCLUDE
        DataParser_Native.cpp
        FxSendNative.cpp
        Joypad_Native.cpp
        Synth_Stub.cpp
        FFmpegMovieImpl.cpp
        # --- X3: xenon's native/src/platform/ owns these ---
        AsyncFile_Native.cpp
        Cache_Stub.cpp
        ChecksumData_Stub.cpp
        File_Native.cpp
        Keyboard_Native.cpp      # twin of xenon's Keyboard_Stub.cpp
        MapFile_Stub.cpp
        Memory_Native.cpp
        Net_Stub.cpp
        NetworkSocket_Stub.cpp
        ThreadCall_Native.cpp
        VirtualKeyboard_Stub.cpp)

    # Sticky CACHE + FORCE so a stale value from an earlier configure can't linger.
    set(MILO_ENGINE_BUILD_GFX   ON  CACHE BOOL   "xenon: build the WebGPU gfx core" FORCE)
    set(MILO_ENGINE_GPU_BACKEND dc3 CACHE STRING "xenon: WgpuRnd : NgRnd GPU backend" FORCE)
    set(MILO_ENGINE_BUILD_TESTS OFF CACHE BOOL   "" FORCE)

    add_subdirectory(${MILO_ENGINE_PATH} milo-engine)

    # -------------------------------------------------------------------------
    # rb3-frame — X1 smoke target: engine GPU device + CLEAR + headless
    # screenshot. Deliberately the SMALLEST thing that proves the engine link
    # surface works end to end (configure -> compile -> link -> run -> pixels).
    #
    # It links libmilo-engine.a and NOTHING ELSE from either tree: no
    # ENGINE_SOURCES, no NATIVE_SHIMS, no dta_link_stubs.s. That is a RESULT,
    # not a shortcut — the engine's gfx core (gfx/GpuDevice.cpp, gfx/Screenshot.cpp)
    # is decomp-agnostic and pulls no Milo symbols, so the archive satisfies
    # itself. It also means rb3-frame touches ZERO xenon rndobj members, which is
    # the whole point: the Rnd/NgRnd layout caveat above cannot contaminate this
    # baseline. The moment a target does instantiate WgpuRnd (X3), it will need
    # the shim set AND a resolution for the 13 basename collisions between
    # native/src/platform/ and the engine's src/platform/.
    # -------------------------------------------------------------------------
    add_executable(rb3-frame ${CMAKE_SOURCE_DIR}/src/main_frame.cpp)
    target_compile_options(rb3-frame PRIVATE
        ${MS_COMPAT_FLAGS} ${ATOMIC_COMPAT_FLAGS} ${DECOMP_FLAGS})
    target_compile_definitions(rb3-frame PRIVATE HX_NATIVE=1 MILO_DEBUG=1 _DEBUG=1)
    target_include_directories(rb3-frame SYSTEM BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_include_directories(rb3-frame PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${REPO_ROOT}/src
        ${REPO_ROOT}/src/system)
    target_link_libraries(rb3-frame PRIVATE milo-engine Threads::Threads)
endif()

# =============================================================================
# X2 — rb3-milo: load a real .milo_xbox from the mounted ark as a live object
#      graph. LOAD ONLY; nothing here draws.
# =============================================================================
# X1 proved the engine links and clears a frame; it deliberately touched ZERO
# xenon rndobj members so the Rnd/NgRnd member-layout caveat
# (src/system/rndobj/Rnd.h:354-360) stayed unmeasured. X2 keeps that caveat
# intact from the other direction: it stands up the xenon OBJECT graph without
# a renderer at all.
#
# ⚠ THIS TARGET DOES NOT LINK libmilo-engine.a, AND THAT IS DELIBERATE.
# Loading a .milo needs DirLoader + the object factories, which are 100% xenon
# fork code; the engine supplies the GPU. Linking it here would (a) drag the
# 13 native/src/platform <-> engine src/platform basename collisions into a
# milestone that gains nothing from paying them, and (b) put WgpuRnd/NgRnd
# member offsets on the critical path of a result that must be interpretable
# without them. The ONE thing X2 takes from the engine tree is the header-only
# platform/NativeSettings.h (no .cpp exists; NativeSettings::Get() is inline),
# which rndobj/Cam.cpp includes under #ifdef HX_NATIVE.
#
# X3 is where the engine link and the collision table come due together.
# =============================================================================
file(GLOB ENGINE_RNDOBJ CONFIGURE_DEPENDS ${REPO_ROOT}/src/system/rndobj/*.cpp)
file(GLOB ENGINE_CHAR   CONFIGURE_DEPENDS ${REPO_ROOT}/src/system/char/*.cpp)
file(GLOB ENGINE_WORLD  CONFIGURE_DEPENDS ${REPO_ROOT}/src/system/world/*.cpp)
# ui/ is 38/38 clang-LP64-clean as measured -- not one exclusion. It is here
# because the world/ and rndobj/ object graphs reach PanelDir, UIComponent and
# TheUI, and PanelDir is an ObjectDir SUBCLASS: stubbing it would be the exact
# desync hazard main_milo.cpp gates against, not a cosmetic gap.
file(GLOB ENGINE_UI     CONFIGURE_DEPENDS ${REPO_ROOT}/src/system/ui/*.cpp)
set(MILO_FORK_SOURCES ${ENGINE_RNDOBJ} ${ENGINE_CHAR} ${ENGINE_WORLD} ${ENGINE_UI})

# ---- X4b STEP-0: the classes every RB3 venue root needs -------------------
# X4a measured 684 factory misses over 14 classes on a venue root and called
# them "band3", concluding that a venue needs a compiled src/band3/. THAT
# ATTRIBUTION IS WRONG, and the correction is most of the cost:
#
#   NONE of the 14 is in src/band3/. 5 are src/system/bandobj/, 7 are
#   src/system/synth/, 1 is src/system/world/ and 1 is src/system/ui/ --
#   and the last two (WorldCrowd in world/Crowd.cpp, UIColor in ui/UIColor.cpp)
#   are ALREADY COMPILED here via ENGINE_WORLD/ENGINE_UI. They were missing
#   only a line in the hand-rolled factory list, not any code at all.
#
# Measured with -fsyntax-only under this target's exact flags: 12 of the 13
# defining TUs compile with ZERO errors. The single exception is
# bandobj/BandCharacter.cpp (18 errors ~ 4 root defects, all stale
# rb3-Wii-lineage code inside its own HX_NATIVE arms -- protected-member
# access on mDraws/mLods/mStoredFile, a reference to RndMesh::
# mNativeBonesRebound that xenon's RndMesh does not have, Refs() used as a
# std::vector<ObjRef*>, and a MergeFilter::Action/SubdirAction mixup).
# Counting TUs would have said "1 of 14 blocked"; counting DEFECTS says four,
# in one file -- the same lesson X2 recorded when 13 rndobj failures turned
# out to be 6 root causes.
#
# ⛔ SO WHY ARE THEY STILL NOT COMPILED HERE? A DIFFERENT BLOCKER, AND IT IS
# THE BUILD SYSTEM, NOT THE CODE. X4b ATTEMPTED the wiring and backed it out.
# Adding the 10 clean TUs above to MILO_FORK_SOURCES produces **807 duplicate
# definition errors at link**, from exactly three colliding emitters:
#
#     EventTrigger.cpp  313    Font.cpp  244    CharIKScale.cpp  162
#
# None is a code defect. cmake/ScatterIncludes.cmake drops an `#include "*.cpp"`
# includee only when that includee is ITSELF a target source. Here two DIFFERENT
# target sources scatter-include the same NON-source file --
#     bandobj/BandCamShot.cpp -> math/Geo.cpp   <- char/CharIKScale.cpp
#     bandobj/BandCamShot.cpp -> flow/*.cpp     <- rndobj/EventTrigger.cpp
#     bandobj/BandLabel.cpp   -> char/CharClip.cpp, bandobj/BandDirector.cpp
#                                                  <- rndobj/Font.cpp
# -- so the module has no reason to drop either copy and both get emitted.
#
# The fix is a dedupe pass in ScatterIncludes.cmake: compute the transitive
# includee set per target and assign each includee to exactly ONE emitter. That
# is a change to a module all 18 targets share, so it needs its own lane with an
# A/B on every target -- the same reason X2 declined to make that module
# transitive and listed _MILO_SCATTER_TRANSITIVE_PRUNE by hand instead.
#
# ⇒ Corrected cost of "a venue root loads": ONE build-system change (scatter
# dedupe) + FOUR source defects in bandobj/BandCharacter.cpp. NOT "compile
# src/band3/", which is 260 TUs this does not need at all.

# Same platform/GFX filters the engine globs use. rndobj/ has no *_Xbox.cpp, but
# keep the filters symmetric so a future one is dropped automatically. NOTE the
# *_NG.cpp files (Rnd_NG, Env_NG, Lit_NG, Mat_NG, Fur_NG, PostProc_NG,
# DOFProc_NG, SpotlightDrawer_NG) are KEPT: NgRnd/NgEnviron/... are the
# DC3-lineage classes the assets actually name, not a platform backend.
list(FILTER MILO_FORK_SOURCES EXCLUDE REGEX "_(Xbox|Win|Wii|X360)\\.cpp$")
list(FILTER MILO_FORK_SOURCES EXCLUDE REGEX "(Dx9|DX9|Wgpu|D3D|Xenon)")
# file(GLOB) matches dotfiles; the permuter writes hidden .permuter_work_*.cpp.
list(FILTER MILO_FORK_SOURCES EXCLUDE REGEX "/\\.[^/]*\\.cpp$")

# TUs that still do not compile under clang/LP64, each with the reason and who
# owns it. Measured with -fsyntax-only under this target's exact flags AFTER the
# shared-src repairs in bf03982c (which took rndobj 73->83 / 86, char 55->59 / 60,
# world 19->22 / 24). Everything named here loses only its own object classes;
# DirLoader degrades gracefully on an unregistered class (obj/DirLoader.cpp:927
# "Can't make %s" + ReadDead skip), and main_milo.cpp REPORTS every such class
# so a gap can never pass silently as a clean load.
#
#   BeatClock    xenon's SongPos has no Phrase field, and BeatClock.cpp reads
#                AccessPhrase()/GetPhrase() unconditionally (:31, :147, :153).
#                A genuine header-shape divergence against the beatmatch
#                SongPos, NOT a native-only break -- it wants a SongPos audit,
#                not a guard, so it is left alone.
#
# ⚠ THIS LIST SHRANK FROM 8 TO 3 DURING X2, and the reason is worth recording:
# five of the eight were NOT independent failures. Console, Font and Crowd all
# died on ONE defect -- BandCharacter's TextureCompressed(int) failing to
# override an intptr_t pure virtual under LP64 (fixed in bf03982c) -- reached
# through three different scatter chains. CubeTex and CharBonesMeshes fell out
# with the band3-include-root and GemManager fixes. Counting TUs before finding
# root causes badly overstates the work: 13 rndobj failures were 6 defects.
#
#
# ★ X3: 3 -> 1. X2 predicted MeshAnim/MeshDeform would fall out cheaply once
# `7ca1ef8a` closed the Symbols*.h x POSIX collision. RE-MEASURED, and the
# prediction was HALF right: the Symbols collision is indeed gone, but it was
# never the only cause. MeshAnim still failed inside synth_xbox/Voice.cpp
# (XAUDIO2 send descriptors + CreateThread, 2 errors) and MeshDeform inside
# band3/meta_band/BandUI.cpp (InterstitialMgr::mRandomOverride, 1 error) -- all
# three sites in SCATTER-INCLUDED tails, none in the TU's own body. So the fix
# is not "the blocker went away", it is an #ifndef HX_NATIVE on the two tails
# (see the note at the top of each). X3 needs both classes because Rnd::PreInit
# registers their factories inline, and PreInit is now on the live path.
#
# Anchored on /Name.cpp$ so e.g. Font.cpp does not also drop FontBase.cpp.
set(_MILO_FORK_EXCLUDE
    BeatClock)                                   # world   (1 of 24)
                                                 # rndobj  (0 of 86)
                                                 # char    (0 of 60)
                                                 # ui      (0 of 38)
foreach(_tu ${_MILO_FORK_EXCLUDE})
    list(FILTER MILO_FORK_SOURCES EXCLUDE REGEX "/${_tu}\\.cpp$")
endforeach()

# Transitive scatter-include prune -- a gap in cmake/ScatterIncludes.cmake, not
# a compile failure.
#
# That module scans the TARGET'S OWN sources for `#include "*.cpp"` edges and
# drops any includee that is also a source. It does NOT follow edges out of a
# file that is not itself a target source. Both TUs below are reached through
# exactly such a chain --
#     rndobj/Font.cpp -> bandobj/BandDirector.cpp -> char/CharClip.cpp
#     rndobj/Font.cpp -> bandobj/BandDirector.cpp -> ... -> char/Character.cpp
# -- and bandobj/BandDirector.cpp is NOT compiled here, so the module never sees
# the second hop and leaves CharClip/Character compiling standalone as well.
# Result: ~30 duplicate definitions at link.
#
# Listing them here rather than "fixing" the module is deliberate: making the
# module transitive changes pruning for all 16 pre-existing targets, which is a
# behaviour change X2 has no way to validate. Recorded as an X3 item.
set(_MILO_SCATTER_TRANSITIVE_PRUNE CharClip Character)
foreach(_tu ${_MILO_SCATTER_TRANSITIVE_PRUNE})
    list(FILTER MILO_FORK_SOURCES EXCLUDE REGEX "/${_tu}\\.cpp$")
endforeach()

list(LENGTH MILO_FORK_SOURCES _milo_fork_count)
message(STATUS "[X2] rb3-milo fork surface: ${_milo_fork_count} rndobj/char/world/ui TUs")

# platform/NativeSettings.h is header-only (no .cpp anywhere in the engine), so
# this is an include path and NOT a link dependency -- rb3-milo needs no Dawn,
# no glfw, and builds on a machine with no dc3-decomp-deps tree. If the engine
# checkout is missing entirely, skip the target rather than break configure for
# the 15 targets that predate it (same defensive stance as the Dawn check above).
if(EXISTS "${MILO_ENGINE_PATH}/src/platform/NativeSettings.h")

# The fork+shim source set shared by rb3-milo (X2, load-only) and rb3-render
# (X3, load-and-draw). Factored out rather than copied so the two targets cannot
# drift: the ONLY differences between them are the driver TU, the engine link,
# and RB3_ENGINE_RENDER (which retires the stub bodies the engine supplies).
set(MILO_TARGET_COMMON_SOURCES
    ${CMAKE_SOURCE_DIR}/src/milo_object_factories.cpp
    ${ENGINE_SOURCES}
    ${MILO_FORK_SOURCES}
    # char/CharCollide.cpp unconditionally scatter-includes midi/MidiParserMgr.cpp,
    # which pulls the whole MidiParser/MidiReader/MidiReceiver surface. These are
    # the exact six TUs rb3-midi already links (M2_SOURCES), so this is reusing a
    # proven set rather than stubbing a live parser -- and CharClip's midi-driven
    # bone channels are real content, not a detour.
    ${M2_SOURCES}
    # Two synth leaves, and only two. char/Waypoint.cpp scatter-includes
    # synth/Sequence.cpp (so `typeinfo for Sequence` needs its SynthPollable
    # base) and char/CharMeshHide.cpp instantiates ObjRefConcrete<FxSend>, whose
    # dynamic_cast needs `typeinfo for FxSend`. Both TUs are clang-clean, so
    # compiling them is strictly better than hand-forging two type_info objects
    # -- and it does NOT open synth/ generally (no tomcrypt, no audio device).
    ${REPO_ROOT}/src/system/synth/Pollable.cpp
    ${REPO_ROOT}/src/system/synth/FxSend.cpp
    # ---- X7: the band-slot placement TU ---------------------------------
    # X6 ported bandobj/BandConfiguration.cpp and wired it into objects.json
    # (the X360 objdiff build) but NOT into this list, so the native driver
    # still answered "Can't make BandConfiguration" and the venue's four
    # band-slot transforms were still never read. Adding it here is what
    # actually closes that.
    #
    # It is a clean STANDALONE add and is NOT gated on the ScatterIncludes
    # dedupe: measured with `grep -rn 'include "bandobj/BandConfiguration.cpp"'
    # src/` (no includer) and by reading the TU (it scatter-includes nothing).
    # It therefore participates in ZERO scatter edges in either direction and
    # cannot collide. The 807-duplicate figure in the X4b note above is the
    # cost of adding all TEN bandobj TUs at once; it is not the cost of this
    # one, and re-deriving that per-TU is what let this land without the
    # dedupe lane.
    ${REPO_ROOT}/src/system/bandobj/BandConfiguration.cpp
    # ---- X7: the band MEMBERS ------------------------------------------
    ${REPO_ROOT}/src/system/bandobj/BandCharacter.cpp
    ${REPO_ROOT}/src/system/bandobj/BandCharDesc.cpp
    # BandCharacter's own dependency layer, measured from the link, not guessed:
    # OutfitConfig (outfit recompose / texture patching), BandHeadShaper (face
    # visemes), BandPatchMesh + BandRetargetVignette (leaf drawables), and
    # meta/FixedSizeSaveable (BandCharDesc's serialization base).
    ${REPO_ROOT}/src/system/bandobj/BandHeadShaper.cpp
    ${REPO_ROOT}/src/system/bandobj/OutfitConfig.cpp
    ${REPO_ROOT}/src/system/bandobj/BandPatchMesh.cpp
    ${REPO_ROOT}/src/system/bandobj/BandRetargetVignette.cpp
    ${REPO_ROOT}/src/system/bandobj/BandFaceDeform.cpp
    ${REPO_ROOT}/src/system/bandobj/BandIKEffector.cpp
    ${REPO_ROOT}/src/system/meta/FixedSizeSaveable.cpp
    ${REPO_ROOT}/src/system/meta/FixedSizeSaveableStream.cpp
    ${DTA_LEXER}
    ${NATIVE_SHIMS}
    ${CMAKE_SOURCE_DIR}/src/milo_link_stubs.cpp
    ${CMAKE_SOURCE_DIR}/src/dta_link_stubs.s
)

rb3_add_executable(rb3-milo
    ${CMAKE_SOURCE_DIR}/src/main_milo.cpp
    ${MILO_TARGET_COMMON_SOURCES}
)
set(MILO_TARGETS rb3-milo)

# =============================================================================
# X3 — rb3-render: the same object graph, DRAWN, through the engine's dc3
#      WebGPU backend, headless, to a PNG.
# =============================================================================
# rb3-milo's charter was "load, and prove the load without a renderer". X3 keeps
# every one of its gates and adds the renderer on top: SAME fork surface, SAME
# shims, SAME factories, SAME archive. The delta is exactly three things --
# main_render.cpp, `milo-engine` on the link line, and RB3_ENGINE_RENDER.
#
# ⚠ THIS IS THE FIRST TARGET IN THE TREE THAT INSTANTIATES WgpuRnd, i.e. the
# first one on which src/system/rndobj/Rnd.h:354-360's member-layout caveat can
# actually bite. It cannot bite as an ABI problem -- the engine is compiled with
# MILO_ENGINE_DECOMP_INCLUDE_DIRS pointing at xenon's OWN headers, so both sides
# see one rndobj/Rnd.h and offsets agree by construction. The live risk is
# SEMANTIC: the dc3 backend reading a xenon member that exists at the right
# offset and means something else. main_render.cpp --dump-rnd prints every
# Rnd/NgRnd member the backend actually reads, so that class of bug is
# measurable rather than inferred.
if(RB3X_BUILD_ENGINE)
    rb3_add_executable(rb3-render
        ${CMAKE_SOURCE_DIR}/src/main_render.cpp
        # The engine's two CONSUMER SEAMS (ShouldSkipMesh, DebugPanel::*) —
        # header-declared, deliberately undefined in libmilo-engine.a. See the
        # file for why RB3's answers differ from DC3's.
        ${CMAKE_SOURCE_DIR}/src/rb3_render_glue.cpp
        ${MILO_TARGET_COMMON_SOURCES}
    )
    list(APPEND MILO_TARGETS rb3-render)

    # RB3_ENGINE_RENDER retires the milo_link_stubs.cpp bodies that
    # libmilo-engine.a defines for real (TheNgRnd/TheShaderMgr, the RndTex GPU
    # residency four, RndMesh::DrawShowing/OnSync + CleanupGpuMesh,
    # DrawParticlesBillboard, the two draw-order flushes, TheUI). It does NOT
    # retire TheRenderState / RndRenderState::Set* -- see the exclude-list note
    # above for why the engine does not supply those.
    target_compile_definitions(rb3-render PRIVATE RB3_ENGINE_RENDER=1)
    target_link_libraries(rb3-render PRIVATE milo-engine)
endif()

foreach(_t IN LISTS MILO_TARGETS)
target_compile_options(${_t} PRIVATE ${MS_COMPAT_FLAGS} ${ATOMIC_COMPAT_FLAGS} ${DECOMP_FLAGS}
    -ffunction-sections -fdata-sections)
# --gc-sections, for the same reason rb3-score2/3/4 use it: 162 newly-compiled
# TUs reference a great deal of code that the LOAD path never reaches (render
# methods, editor/EditMode paths, gesture/dance/flow/synth neighbours pulled in
# through scatter-includes). Without GC every one of those references has to be
# satisfied by a stub, and every stub is a place where real behaviour can be
# silently replaced by a no-op. Letting the linker drop the unreachable code
# instead means the stub list below is only what the load path ACTUALLY touches
# -- which is the difference between a stub list you can audit and one you
# cannot.
target_link_options(${_t} PRIVATE -Wl,--gc-sections)

# --- -Werror opt-in exemptions, with the triage that earned each one ---------
# The DECOMP_WARNINGS policy at the top of this file was MEASURED to produce
# zero diagnostics -- across the 251 TUs that were in the build AT THE TIME.
# rb3-milo adds 162 TUs that measurement never covered, and four of them trip an
# opt-in. That is the policy working, not the policy being wrong: each site
# below is a real finding. But three of the four are inside MATCHED bodies,
# where "fixing" it is a match A/B with its own objdiff evidence -- not
# something X2 gets to do on the way past. So they are exempted PER TU, with the
# verdict recorded, and listed in docs/plans/x2-object-graph-load-2026-08-01.md
# as owed work. The policy stays fully armed everywhere else, including on the
# other 158 new TUs.
#
# The ONE that was fixed instead of exempted is char/Character.cpp's
# `drawMode == 4`: see char/Character.h's kCharDrawShadow -- a missing
# enumerator meant clang folded the shadow dispatch to false, so character
# shadows were dead natively. Adding the enumerator is provably codegen-neutral
# for X360, so it did not need an exemption.
# NB the exemption goes on rndobj/Console.cpp, not on MultiMesh.cpp: Console.cpp
# unconditionally scatter-includes rndobj/MultiMesh.cpp, so ScatterIncludes
# prunes MultiMesh from standalone compilation and the diagnostic is emitted
# from Console's TU. (It was on MultiMesh.cpp until Console.cpp came off the
# exclude list -- a nice illustration that scatter-includes move diagnostics as
# well as symbols.)
set_source_files_properties(${REPO_ROOT}/src/system/rndobj/Console.cpp
    TARGET_DIRECTORY ${_t} PROPERTIES COMPILE_OPTIONS "-Wno-self-assign")
#   MultiMesh.cpp:363  `proxy = proxy;` -- a no-op store, almost certainly a
#   decomp device to pin the variable's storage. Harmless (proxy is initialized
#   to nullptr at :354). ⚠ BUT the loop above it assigns `proxy = sit->first`
#   on EVERY iteration and only breaks on a match, so after a full non-matching
#   scan `proxy` holds the LAST pool entry rather than null -- and the
#   `if (!proxy)` at :364 then does not fire. That is a separate, real logic
#   defect; it is off the load path (EditMode-only collision picking) and is
#   recorded for triage, not patched here.

set_source_files_properties(${REPO_ROOT}/src/system/rndobj/Mat_NG.cpp
    TARGET_DIRECTORY ${_t} PROPERTIES
    COMPILE_OPTIONS "-Wno-tautological-constant-out-of-range-compare")
#   Mat_NG.cpp:145  `cur == 6` where cur is RndRenderState::ClampMode -- and
#   rnddx9/RenderState.h:16 declares `enum ClampMode {};`, i.e. EMPTY. It is a
#   placeholder in the DX9 backend header, so EVERY comparison and every
#   `(ClampMode)N` cast in the tree is trivially out of range. NOT a real dead
#   branch (unlike the Character one) -- an artifact of a stub enum in a
#   directory the port replaces wholesale. Fix belongs with rnddx9, or vanishes
#   when X3 swaps in the engine's render state.

set_source_files_properties(
    ${REPO_ROOT}/src/system/rndobj/Text.cpp
    ${REPO_ROOT}/src/system/char/CharClipDisplay.cpp
    TARGET_DIRECTORY ${_t} PROPERTIES
    COMPILE_OPTIONS "-Wno-sometimes-uninitialized")
set_source_files_properties(${REPO_ROOT}/src/system/rndobj/Font.cpp
    TARGET_DIRECTORY ${_t} PROPERTIES COMPILE_OPTIONS "-Wno-uninitialized")
#   Font.cpp:341-355  RndFont::Load's rev<4 branch declares `float w, h;` and
#   then SHADOWS them with `int w, h;` inside the nested `if (d.rev < 2)`. Only
#   the shadowing pair is ever written, so on the rev==2/3 path the OUTER w/h
#   are read uninitialized at :354-355 to divide the bitmap dimensions. Reads
#   like the same transcription artifact as CharClipDisplay: retail almost
#   certainly had one pair of variables. rev<4 fonts do not appear in these
#   assets, so it is reported rather than patched -- fixing it is a match A/B.
#   Text.cpp:245  `int fixedLength;` is only assigned when rev > 0xB, but is
#   read unconditionally three lines later (two MILO_ASSERTs and
#   `mFixedLength = fixedLength`). For a rev in (8, 0xB] RndText::Load stores
#   garbage. Plausibly FAITHFUL -- retail's own Load has the same shape -- so it
#   wants the oracle triage before anyone touches it. Not hit by these assets
#   (rev is far newer).
#   CharClipDisplay.cpp:201  worse, and probably NOT faithful: the function
#   declares `RndTransformable *data` TWICE, once at :213 and once at :219, and
#   `goto drawIKData` at :214 jumps into the second one's scope PAST its
#   initializer. On the goto path the `data` read at :222 is uninitialized.
#   That reads like a transcription artifact of a single outer-scope variable in
#   retail. Off the load path (an editor/debug display draw), but it is a live
#   uninitialized pointer read the moment anything calls it.
target_compile_definitions(${_t} PRIVATE HX_NATIVE=1 MILO_DEBUG=1 _DEBUG=1)
target_include_directories(${_t} SYSTEM BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(${_t} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${REPO_ROOT}/src
    ${REPO_ROOT}/src/system
    # NOT because band3/ or network/ code is compiled here -- none is. Two fork
    # TUs scatter-include a band3 TU (char/CharBonesMeshes.cpp:213 <-
    # bandtrack/GemManager.cpp, ui/UIList.cpp:1084 <- bandtrack/GemTrack.cpp),
    # and those includees resolve their own headers through these roots.
    ${REPO_ROOT}/src/band3
    ${REPO_ROOT}/src/network
    ${MILO_ENGINE_PATH}/src)   # header-only: platform/NativeSettings.h
target_link_libraries(${_t} PRIVATE Threads::Threads ZLIB::ZLIB)
endforeach()

else()
    message(WARNING
        "[X2] no milo-native-engine at ${MILO_ENGINE_PATH} — skipping rb3-milo. "
        "rndobj/Cam.cpp includes its header-only platform/NativeSettings.h under "
        "#ifdef HX_NATIVE. Other targets are unaffected.")
endif()
