#pragma once
#include "obj/Object.h"
#include "synth/MoggClip.h"
#include "utl/BinStream.h"

// RB3-360 retail (and rb3-Wii) MoggClipMap derives from Hmx::Object — the leading
// Object subobject (0x28 bytes on X360, 0x1c on Wii) pushes mMoggClip to 0x28.
// DC3's newer refactor dropped the base (mMoggClip at 0x4); porting that verbatim
// caused a uniform -0x24 member-offset drift in myLoad + the ObjVector helpers.
class MoggClipMap : public Hmx::Object {
    friend bool PropSync(MoggClipMap &, DataNode &, DataArray *, int, PropOp);

public:
    MoggClipMap(Hmx::Object *);
    MoggClipMap(const MoggClipMap &);
    virtual ~MoggClipMap();

    void mySave(BinStream &) const;
    void myLoad(BinStream &);
    // RB3-360 retail reads the parent Sfx's rev via a TU-static (rb3-Wii idiom)
    // rather than threading a BinStreamRev wrapper. Sfx::Load stashes its rev
    // here right before `bs >> mMoggClipMaps`.
    static int sRev;
    MoggClipMap &operator=(const MoggClipMap &);
    MoggClip *GetMoggClip() const { return mMoggClip; }
    float Pan() const { return mPan; }
    float PanWidth() const { return mPanWidth; }
    float Volume() const { return mVolume; }
    bool Stereo() const { return mIsStereo; }

protected:
    /** "Which moggclip to play" */
    ObjPtr<MoggClip> mMoggClip; // 0x28 (after Object 0x28)
    /** "Surround pan, between -4 and 4" */
    float mPan; // 0x3c
    /** "Surround pan width, between 0 and 4" */
    float mPanWidth; // 0x40
    /** "Volume in dB (0 is full volume, -96 is silence)" */
    float mVolume; // 0x44
    /** "Is the mogg clip stereo?" */
    bool mIsStereo; // 0x48
};

BinStream &operator<<(BinStream &, const MoggClipMap &);
BinStream &operator>>(BinStream &, MoggClipMap &);
