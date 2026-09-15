// Retail's `system/synth_xbox/FxSendPitchShift.cpp` TU is where FxSendPitchShift360's
// two out-of-line virtuals live -- NOT FxSendPitchShift360.cpp, which retail (and DC3)
// leave out of the build entirely. This file was an empty stub, so all three rows of
// unit `default/system/synth_xbox/FxSendPitchShift` (188 B) read fuzzy 0: the unit's
// base obj defined nothing at all.
//
// Ported from dc3-decomp src/system/synth_xbox/FxSendPitchShift.cpp, whose objects.json
// wires exactly this file (and not the 360 one) as Matching -- the same arrangement.
//
// Retail evidence for CreateFx (lane W16-BO, fn_82B6A0E8, 72 B):
//   li r3, 0x70            <- sizeof(PitchShiftEffect) == 112, per
//                             cl /d1reportSingleClassLayout and the header's `// size 0x70`
//   bl 0x827BD2F0          <- operator new
//   cmplwi r3,0 / beq      <- MSVC new-null guard
//   bl 0x82B6D820          <- ??0PitchShiftEffect@@QAA@XZ
//   return r3 unadjusted   <- IUnknown* sits at offset 0, so no this-adjustor tail
// It has ZERO retail `bl` callers, which is the signature of a *virtual* (dispatched
// through the vtable), not of a REGISTER_OBJ_FACTORY reached via a data pointer.
// Its EH cleanup funclet is fn_82B6A130 (40 B): reload the saved allocation from
// 0x50(r31), `bl` operator delete -- it pairs by funclet byte signature, not by name.
#include "synth_xbox/FxSendPitchShift360.h"
#include "synth_xbox/PitchShiftEffect.h"

void FxSendPitchShift360::SyncEffectParams(IXAudio2SubmixVoice *voice) const {
    PitchShiftEffectParams p;
    p.unk0 = mRatio;
    voice->SetEffectParameters(0, &p, sizeof(p), 0);
}

IUnknown *FxSendPitchShift360::CreateFx() {
    return static_cast<CXAPOBase *>(new PitchShiftEffect());
}
