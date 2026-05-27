#pragma once
// Stub header: only declares the enum + class forward-decl needed to compile
// BandDirector / BandWardrobe headers. Full Wii->360 port deferred (BandCharacter
// cluster needs Wii-only engine deps per docs/plans/bandobj-port.md Stage 4).
#include "obj/Object.h"

class BandCharDesc : public virtual Hmx::Object {
public:
    enum CharInstrumentType {
        kGuitar,
        kBass,
        kDrum,
        kMic,
        kKeyboard,
        kNumInstruments
    };

    BandCharDesc();
    virtual ~BandCharDesc() {}
};
