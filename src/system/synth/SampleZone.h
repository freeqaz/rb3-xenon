#pragma once
#include "obj/Object.h"
#include "synth/ADSR.h"
#include "synth/Stream.h"
#include "synth/SynthSample.h"
#include "utl/BinStream.h"

class SampleZone {
    friend bool __cdecl PropSync(SampleZone &, DataNode &, DataArray *, int, PropOp);

public:
    SampleZone(Hmx::Object *);
    void Save(BinStream &) const;
    void Load(BinStreamRev &);
    bool Includes(unsigned char, unsigned char);

    static int gRev;

    int CenterNote() const { return mCenterNote; }
    float Volume() const { return mVolume; }
    float Pan() const { return mPan; }
    FXCore GetFXCore() const { return mFXCore; }
    const ADSRImpl &ADSR() const { return mADSR; }
    SynthSample *Sample() const { return mSample; }

private:
    /** "Which sample to play" */
    ObjPtr<SynthSample> mSample; // 0x0
    /** "Volume in dB (0 is full volume, -96 is silence)" */
    float mVolume; // 0xc
    /** "Surround pan, between -4 and 4" */
    float mPan; // 0x10
    /** "note at which sample pays without pitch change" */
    int mCenterNote; // 0x14
    /** "Lowest zone note" */
    int mMinNote; // 0x18
    /** "Highest zone note" */
    int mMaxNote; // 0x1c
    /** "Lowest zone velocity" */
    int mMinVel; // 0x20
    /** "Highest zone velocity" */
    int mMaxVel; // 0x24
    /** "Which core's digital FX should be used in playing this sample" */
    FXCore mFXCore; // 0x28
    ADSRImpl mADSR; // 0x34
};

BinStream &operator<<(BinStream &, const SampleZone &);
BinStreamRev &operator>>(BinStreamRev &, SampleZone &);
