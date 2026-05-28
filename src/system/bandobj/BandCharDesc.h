#pragma once
// Stub header: only declares the enum + class forward-decl needed to compile
// BandDirector / BandWardrobe headers. Full Wii->360 port deferred (BandCharacter
// cluster needs Wii-only engine deps per docs/plans/bandobj-port.md Stage 4).
#include "obj/Object.h"
#include "meta/FixedSizeSaveable.h"

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

    // Nested Patch class — needed by AssetTypes.h for Patch::Category enum.
    class Patch : public FixedSizeSaveable {
    public:
        enum Category {
            kPatchNone = 0x0,
            kPatchTorso = 0x1,
            kPatchLeg = 0x2,
            kPatchFeet = 0x4,
            kPatchHair = 0x8,
            kPatchTattoo = 0x10,
            kPatchMakeup = 0x20,
            kPatchFacepaint = 0x40,
            kPatchTorsoOverlay = 0x80,
            kPatchLegOverlay = 0x100,
            kPatchGuitar = 0x200,
            kPatchBass = 0x400,
            kPatchDrum = 0x800,
            kPatchMic = 0x1000,
            kPatchKeyboard = 0x2000
        };
        Patch() {}
        virtual ~Patch() {}
        virtual void SaveFixed(FixedSizeSaveableStream &) const {}
        virtual void LoadFixed(FixedSizeSaveableStream &, int) {}
        static int SaveSize(int) { return 0; }
    };

    BandCharDesc();
    virtual ~BandCharDesc() {}
};
