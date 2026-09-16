#pragma once
#include "xdk/win_types.h"
#include "xdk/unknwn.h"

// XAudio2FX built-in audio effects (reverb / volume meter) — public XAPOFX C API.
// Only the reverb declarations the engine references are mirrored here; the
// implementations live in the XDK (xdk/xaudio2/reverb).
//
// Ported from ../dc3-decomp/src/xdk/xaudio2/xaudio2fx.h with ONE deliberate
// departure, adjudicated on retail bytes — see XAUDIO2FX_REVERB_PARAMETERS.

#pragma pack(push, 1)

// I3DL2 environmental reverb parameters (size 0x34). Matches the DirectX SDK layout.
// Verified against retail 0x82B68070: the preset table there has a 0x38-byte stride
// carrying a 4-byte Symbol at +0x00 and these 13 fields at +0x04..+0x37, and every
// one of the 30x13 = 390 stored values decodes to the standard I3DL2 preset set.
struct XAUDIO2FX_REVERB_I3DL2_PARAMETERS { /* Size=0x34 */
    /* 0x0000 */ float WetDryMix;
    /* 0x0004 */ INT32 Room;
    /* 0x0008 */ INT32 RoomHF;
    /* 0x000c */ float RoomRolloffFactor;
    /* 0x0010 */ float DecayTime;
    /* 0x0014 */ float DecayHFRatio;
    /* 0x0018 */ INT32 Reflections;
    /* 0x001c */ float ReflectionsDelay;
    /* 0x0020 */ INT32 Reverb;
    /* 0x0024 */ float ReverbDelay;
    /* 0x0028 */ float Diffusion;
    /* 0x002c */ float Density;
    /* 0x0030 */ float HFReference;
};

// Native reverb parameters consumed by SetEffectParameters.
//
// ⚠ SIZE IS 0x34 HERE, NOT DC3's 0x38 — DC3 IS NEWER AND ITS STRUCT IS RIGHT FOR
// DC3, NOT FOR RB3. Adjudicated on BOTH retail binaries, which disagree:
//   RB3 0x82B68D14  `li r6, 0x34`   (ParametersByteSize = sizeof(native))
//   DC3 0x82E32334  `li r6, 0x38`   (dc3-decomp build/373307D9 .../FxSendReverb.s:1399)
// DC3's trailing `UINT32 WetDryMixPct` at 0x34 is an XDK-revision addition that
// postdates RB3. Carrying it here would make the one `li r6` immediate a charged
// instruction and cost the whole 3,280-byte row, since matched_code is
// all-or-nothing. Do not "restore" it from the DC3 header.
struct XAUDIO2FX_REVERB_PARAMETERS { /* Size=0x34 */
    /* 0x0000 */ float WetDryMix;
    /* 0x0004 */ UINT32 ReflectionsDelay;
    /* 0x0008 */ BYTE ReverbDelay;
    /* 0x0009 */ BYTE RearDelay;
    /* 0x000a */ BYTE PositionLeft;
    /* 0x000b */ BYTE PositionRight;
    /* 0x000c */ BYTE PositionMatrixLeft;
    /* 0x000d */ BYTE PositionMatrixRight;
    /* 0x000e */ BYTE EarlyDiffusion;
    /* 0x000f */ BYTE LateDiffusion;
    /* 0x0010 */ BYTE LowEQGain;
    /* 0x0011 */ BYTE LowEQCutoff;
    /* 0x0012 */ BYTE HighEQGain;
    /* 0x0013 */ BYTE HighEQCutoff;
    /* 0x0014 */ float RoomFilterFreq;
    /* 0x0018 */ float RoomFilterMain;
    /* 0x001c */ float RoomFilterHF;
    /* 0x0020 */ float ReflectionsGain;
    /* 0x0024 */ float ReverbGain;
    /* 0x0028 */ float DecayTime;
    /* 0x002c */ float Density;
    /* 0x0030 */ float RoomSize;
};

#pragma pack(pop)

// Creates the XAudio2 reverb XAPO effect (returns its IUnknown*). XDK-provided.
// Undecorated (extern "C") symbol — matches the XDK leapfxlib export.
extern "C" HRESULT CreateAudioReverb(IUnknown **ppApo);

// Converts I3DL2 environmental parameters into native reverb parameters.
// XDK-provided (retail fn_82B67D30); its body is Microsoft vendor implementation
// and is deliberately NOT decompiled here — see docs/decomp/W16FG_*.
void ReverbConvertI3DL2ToNative(
    const XAUDIO2FX_REVERB_I3DL2_PARAMETERS *pI3DL2,
    XAUDIO2FX_REVERB_PARAMETERS *pNative
);
