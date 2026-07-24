// M4 profile/save native link stubs.
//
// GameplayOptions.cpp is compiled for its real SaveFixed/LoadFixed/SaveSize
// bodies. Its *other* members (SetVocalVolume, the BEGIN_HANDLERS block) pull a
// handful of singletons and interned handler Symbols that the serialization
// round-trip never executes. These honest leaf satisfiers keep the link closed
// without dragging in ProfileMgr.cpp / MsgSource and the UI cascade behind them.
// Native build only — the shared X360 sources are untouched.

#include "utl/Symbol.h"
#include "meta_band/ProfileMgr.h"

// --- ProfileMgr::GetSliderStepCount (off-path: only SetVocalVolume calls it) ---
// Real return is the options-slider step count; the round-trip pokes mVocalVolume
// directly and never routes through SetVocalVolume, so a benign constant closes
// the link.
int ProfileMgr::GetSliderStepCount() const { return 21; }

// --- TheProfileMgr (extern ProfileMgr) ---
// Raw uninitialized storage under the exact (unmangled) linker name. Never
// dereferenced on the save path. Defined via top-level asm so it doesn't clash
// with the header's `extern ProfileMgr TheProfileMgr;` declaration and doesn't
// require running ProfileMgr's ctor.
asm(".globl TheProfileMgr\n"
    ".bss\n"
    ".align 16\n"
    "TheProfileMgr:\n"
    "    .zero 8192\n"
    ".text\n");

// --- GameplayOptions handler Symbols (off-path; BEGIN_HANDLERS-only) ---
// Bare interned-Symbol globals the message-handler block compares against. Same
// convention as m6_symbols.cpp — default-constructed link satisfiers, since
// GameplayOptions::Handle is never dispatched on the serialization path.
Symbol get_lefty;
Symbol set_lefty;
Symbol get_vocal_style;
Symbol set_vocal_style;
Symbol get_vocal_volume;
Symbol set_vocal_volume;
