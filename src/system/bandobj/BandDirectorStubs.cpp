// Scaffolding TU for BandDirector.cpp's LightPreset stubs.
//
// Why these live in a SEPARATE translation unit (load-bearing for matching):
// retail implements SymToPstKeyframe out-of-line in the LightPreset TU
// (0x824AACB8), so from BandDirector.cpp's point of view it is an opaque
// extern whose register clobber set is UNKNOWN. When the stub body was
// defined inside BandDirector.cpp, MSVC could see it clobbers almost nothing
// and therefore kept the LightPresetMgr()-adjusted `this` pointer in a
// VOLATILE register (r10) across the `bl` in OnLightPresetKeyframeInterp.
// Retail is forced to a callee-saved register (r30) plus a distinct spill
// slot. Moving the definition out of the TU restores the unknown-clobber
// assumption and takes OnLightPresetKeyframeInterp 99.9% -> 100.0%.
//
// Keep the definition OUT of BandDirector.cpp. Also keep it out of the pinned
// LightPreset.cpp / LightPresetManager.cpp units, whose .text spans cover the
// real retail implementations and would pair against this stub.
//
// Behavior matches "no music-video presets"; port the real implementation as
// part of the world/LightPreset Wii->360 wave, at which point this file goes
// away.

#include "utl/Symbol.h"
#include "world/LightPreset.h"
#include "world/LightPresetManager.h"

// volatile local: without it MSVC /O1 constant-folds the return into callers
// (even across __declspec(noinline)) and deletes retail call sites.
__declspec(noinline) int SymToPstKeyframe(Symbol) {
    volatile int n = LightPreset::kPresetKeyframeNum;
    return n;
}

__declspec(noinline) float LightPreset::LegacyFadeIn() const { volatile float f = 0.0f; return f; }
__declspec(noinline) void LightPreset::StaticResetEvents() { volatile int n = 0; (void)n; }
__declspec(noinline) void LightPresetManager::GetPresets(LightPreset *&a, LightPreset *&b) {
    volatile int n = 0; (void)n;
    a = nullptr; b = nullptr;
}
__declspec(noinline) void LightPresetManager::Interp(Symbol, Symbol, float) { volatile int n = 0; (void)n; }
__declspec(noinline) void LightPresetManager::SchedulePstKey(int) { volatile int n = 0; (void)n; }
__declspec(noinline) void LightPresetManager::StompPresets(LightPreset *, LightPreset *) { volatile int n = 0; (void)n; }
__declspec(noinline) LightPreset *LightPresetManager::PickRandomPreset(Symbol) {
    volatile int n = 0;
    return (LightPreset *)(n & 0);
}
