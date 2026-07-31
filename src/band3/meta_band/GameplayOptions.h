#pragma once
#include "obj/Object.h"
#include "meta/FixedSizeSaveable.h"
#include "bandtrack/VocalStyle.h"
#include "types.h"

#ifdef RB3_METAPERFORMER_NOVTORDISP
// MetaPerformer.cpp is built with /vd0 to strip the vtordisp that MSVC would
// otherwise put ahead of MetaPerformer's Hmx::Object virtual base (retail puts
// that vbase at 0x384 -- commit ab6fbd15).  That suppression has to be TU-wide,
// because MetaPerformer inherits the vtordisp from its *base* (MsgSource), and a
// #pragma at MetaPerformer's own definition cannot remove a base-supplied one
// (measured: a derived class defined under vtordisp(off) still carries its base's
// vtordisp).  But /vd0 is indiscriminate -- it also strips the legitimate
// vtordisp from GameplayOptions (0x48 -> 0x44), which drags every BandUser member
// past mGameplayOptions down by 4 (mPlayer 0x8c -> 0x88) and made the
// UpdateSoloInstarank*Label family load mTrack's slot instead of mPlayer's.
// #pragma vtordisp(on) overrides the command-line /vd0 for this one class.
#pragma vtordisp(on)
#endif
class GameplayOptions : public virtual Hmx::Object, public FixedSizeSaveable {
public:
    GameplayOptions();
    virtual ~GameplayOptions() {}
    virtual void SaveFixed(FixedSizeSaveableStream &) const;
    virtual void LoadFixed(FixedSizeSaveableStream &, int);
    virtual DataNode Handle(DataArray *, bool);
    virtual void SetLefty(bool);
    virtual bool GetLefty() const { return mLefty; }
    virtual void SetVocalStyle(VocalStyle);
    virtual VocalStyle GetVocalStyle() const { return mVocalStyle; }
    virtual void SetVocalVolume(int, int);
    virtual int GetVocalVolume(int) const;

    static int SaveSize(int);

    int mVocalVolume;
    bool mLefty;
    VocalStyle mVocalStyle;
    mutable bool mDirty;
};
#ifdef RB3_METAPERFORMER_NOVTORDISP
#pragma vtordisp(off) // restore the TU-wide /vd0 state for classes defined later
#endif
