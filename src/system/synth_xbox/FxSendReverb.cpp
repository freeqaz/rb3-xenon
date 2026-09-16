#include "FxSendReverb.h"
#include "FxSend.h"
#include "os/Debug.h"
#include "utl/Symbol.h"
#include "xdk/xaudio2/xaudio2.h"
#include "xdk/xaudio2/xaudio2fx.h"

FxSendReverb360::FxSendReverb360() : FxSend360(this) {}

FxSendReverb360::~FxSendReverb360() {}

namespace {
    // One I3DL2 environmental reverb preset paired with the Symbol name it answers to.
    // Retail 0x82B68070 builds this table with a 0x38-byte stride: Symbol at +0x00
    // (constructed by `bl fn_827C0728` with the name literal), params at +0x04..+0x37.
    struct ReverbPreset {
        Symbol name;
        XAUDIO2FX_REVERB_I3DL2_PARAMETERS params;
    };
}

// The I3DL2 preset table is a function-local static, so it is built once behind a
// guard variable — retail tests bit 0 of lbl_82E12860 and skips to the search when
// already set, which is the `??_B`/`$S` static-init guard shape.
//
// ADJUDICATED AGAINST RETAIL BYTES, NOT AGAINST DC3 (CLAUDE.md: DC3 is newer, and
// five rows on 2026-09-16 alone had our source faithfully reproducing an oracle
// defect). Symbolically executing retail 0x82B68070 reconstructs the whole table:
//   * entry count 30 — the search compares a byte cursor against 0x690 = 30 * 0x38,
//     and the last Symbol ctor targets slot 0x658 = entry 29;
//   * all 30 names and their ORDER (read from the .rdata literals the ctor calls
//     pass) are exactly the list below — independently corroborated by the option
//     list in synth/FxSendReverb.h's mEnvironmentPreset doc comment;
//   * all 30 x 13 = 390 constants match, including three (hallway DecayHFRatio,
//     mountains and medium_hall ReflectionsDelay) that retail spills to stack slots
//     r31+0x50/0x54/0x58 rather than keeping in an FPR.
// So on the TABLE the oracle is verified, not assumed. The one place RB3 and DC3
// genuinely differ is sizeof(XAUDIO2FX_REVERB_PARAMETERS) — see xaudio2fx.h.
void FxSendReverb360::SyncEffectParams(IXAudio2SubmixVoice *voice) const {
    static ReverbPreset presets[] = {
        { Symbol("default"),          { 100.0f, -10000,    0, 0.0f,  1.00f, 0.50f, -10000, 0.020f, -10000, 0.040f, 100.0f, 100.0f, 5000.0f } },
        { Symbol("generic"),          { 100.0f,  -1000, -100, 0.0f,  1.49f, 0.83f,  -2602, 0.007f,    200, 0.011f, 100.0f, 100.0f, 5000.0f } },
        { Symbol("padded_cell"),      { 100.0f,  -1000, -6000, 0.0f, 0.17f, 0.10f,  -1204, 0.001f,    207, 0.002f, 100.0f, 100.0f, 5000.0f } },
        { Symbol("room"),             { 100.0f,  -1000, -454, 0.0f,  0.40f, 0.83f,  -1646, 0.002f,     53, 0.003f, 100.0f, 100.0f, 5000.0f } },
        { Symbol("bath_room"),        { 100.0f,  -1000, -1200, 0.0f, 1.49f, 0.54f,   -370, 0.007f,   1030, 0.011f, 100.0f,  60.0f, 5000.0f } },
        { Symbol("living_room"),      { 100.0f,  -1000, -6000, 0.0f, 0.50f, 0.10f,  -1376, 0.003f,  -1104, 0.004f, 100.0f, 100.0f, 5000.0f } },
        { Symbol("stone_room"),       { 100.0f,  -1000, -300, 0.0f,  2.31f, 0.64f,   -711, 0.012f,     83, 0.017f, 100.0f, 100.0f, 5000.0f } },
        { Symbol("auditorium"),       { 100.0f,  -1000, -476, 0.0f,  4.32f, 0.59f,   -789, 0.020f,   -289, 0.030f, 100.0f, 100.0f, 5000.0f } },
        { Symbol("concert_hall"),     { 100.0f,  -1000, -500, 0.0f,  3.92f, 0.70f,  -1230, 0.020f,     -2, 0.029f, 100.0f, 100.0f, 5000.0f } },
        { Symbol("cave"),             { 100.0f,  -1000,    0, 0.0f,  2.91f, 1.30f,   -602, 0.015f,   -302, 0.022f, 100.0f, 100.0f, 5000.0f } },
        { Symbol("arena"),            { 100.0f,  -1000, -698, 0.0f,  7.24f, 0.33f,  -1166, 0.020f,     16, 0.030f, 100.0f, 100.0f, 5000.0f } },
        { Symbol("hangar"),           { 100.0f,  -1000, -1000, 0.0f, 10.05f, 0.23f,  -602, 0.020f,    198, 0.030f, 100.0f, 100.0f, 5000.0f } },
        { Symbol("carpeted_hallway"), { 100.0f,  -1000, -4000, 0.0f, 0.30f, 0.10f,  -1831, 0.002f,  -1630, 0.030f, 100.0f, 100.0f, 5000.0f } },
        { Symbol("hallway"),          { 100.0f,  -1000, -300, 0.0f,  1.49f, 0.59f,  -1219, 0.007f,    441, 0.011f, 100.0f, 100.0f, 5000.0f } },
        { Symbol("stone_corridor"),   { 100.0f,  -1000, -237, 0.0f,  2.70f, 0.79f,  -1214, 0.013f,    395, 0.020f, 100.0f, 100.0f, 5000.0f } },
        { Symbol("alley"),            { 100.0f,  -1000, -270, 0.0f,  1.49f, 0.86f,  -1204, 0.007f,     -4, 0.011f, 100.0f, 100.0f, 5000.0f } },
        { Symbol("forest"),           { 100.0f,  -1000, -3300, 0.0f, 1.49f, 0.54f,  -2560, 0.162f,   -613, 0.088f,  79.0f, 100.0f, 5000.0f } },
        { Symbol("city"),             { 100.0f,  -1000, -800, 0.0f,  1.49f, 0.67f,  -2273, 0.007f,  -2217, 0.011f,  50.0f, 100.0f, 5000.0f } },
        { Symbol("mountains"),        { 100.0f,  -1000, -2500, 0.0f, 1.49f, 0.21f,  -2780, 0.300f,  -2014, 0.100f,  27.0f, 100.0f, 5000.0f } },
        { Symbol("quarry"),           { 100.0f,  -1000, -1000, 0.0f, 1.49f, 0.83f, -10000, 0.061f,    500, 0.025f, 100.0f, 100.0f, 5000.0f } },
        { Symbol("plain"),            { 100.0f,  -1000, -2000, 0.0f, 1.49f, 0.50f,  -2466, 0.179f,  -2514, 0.100f,  21.0f, 100.0f, 5000.0f } },
        { Symbol("parking_lot"),      { 100.0f,  -1000,    0, 0.0f,  1.65f, 1.50f,  -1363, 0.008f,  -1153, 0.012f, 100.0f, 100.0f, 5000.0f } },
        { Symbol("sewer_pipe"),       { 100.0f,  -1000, -1000, 0.0f, 2.81f, 0.14f,    429, 0.014f,    648, 0.021f,  80.0f,  60.0f, 5000.0f } },
        { Symbol("underwater"),       { 100.0f,  -1000, -4000, 0.0f, 1.49f, 0.10f,   -449, 0.007f,   1700, 0.011f, 100.0f, 100.0f, 5000.0f } },
        { Symbol("small_room"),       { 100.0f,  -1000, -600, 0.0f,  1.10f, 0.83f,   -400, 0.005f,    500, 0.010f, 100.0f, 100.0f, 5000.0f } },
        { Symbol("medium_room"),      { 100.0f,  -1000, -600, 0.0f,  1.30f, 0.83f,  -1000, 0.010f,   -200, 0.020f, 100.0f, 100.0f, 5000.0f } },
        { Symbol("large_room"),       { 100.0f,  -1000, -600, 0.0f,  1.50f, 0.83f,  -1600, 0.020f,  -1000, 0.040f, 100.0f, 100.0f, 5000.0f } },
        { Symbol("medium_hall"),      { 100.0f,  -1000, -600, 0.0f,  1.80f, 0.70f,  -1300, 0.015f,   -800, 0.030f, 100.0f, 100.0f, 5000.0f } },
        { Symbol("large_hall"),       { 100.0f,  -1000, -600, 0.0f,  1.80f, 0.70f,  -2000, 0.030f,  -1400, 0.060f, 100.0f, 100.0f, 5000.0f } },
        { Symbol("plate"),            { 100.0f,  -1000, -200, 0.0f,  1.30f, 0.90f,      0, 0.002f,      0, 0.010f, 100.0f,  75.0f, 5000.0f } },
    };

    unsigned int idx;
    for (idx = 0; idx < 30; idx++) {
        if (presets[idx].name == mEnvironmentPreset)
            break;
    }
    // MILO_FAIL is `((void)(__VA_ARGS__))` in the match build (HX_NATIVE undefined),
    // so this collapses to nothing and the `if` is dead — which is why retail's
    // search falls straight through to the index computation with no miss-check.
    if (idx == 30)
        MILO_FAIL("Unexpected environment preset.");

    XAUDIO2FX_REVERB_PARAMETERS native;
    ReverbConvertI3DL2ToNative(&presets[idx].params, &native);
    voice->SetEffectParameters(0, &native, sizeof(native), 0);
}

IUnknown *FxSendReverb360::CreateFx() {
    IUnknown *apo;
    // XDK export (leapfxlib), declared in xdk/xaudio2/xaudio2fx.h; Synth.cpp declares
    // it the same way for the master chain.  Retail 0x82B67BD8:
    // `addi r3,r1,0x50; bl fn_82BBF340; lwz r3,0x50(r1)`.
    CreateAudioReverb(&apo);
    return apo;
}
