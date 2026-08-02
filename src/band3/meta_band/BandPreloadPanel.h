#pragma once
#include "meta/PreloadPanel.h"
#include "meta_band/LockStepMgr.h"

class BandPreloadPanel : public PreloadPanel {
public:
    BandPreloadPanel();
    OBJ_CLASSNAME(BandPreloadPanel);
    OBJ_SET_TYPE(BandPreloadPanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~BandPreloadPanel();
    virtual void Load();
    virtual bool IsLoaded() const;
    virtual void PollForLoading();

    DataNode OnMsg(const LockStepStartMsg &);
    DataNode OnMsg(const LockStepCompleteMsg &);
    NEW_OBJ(BandPreloadPanel);
    static void Init() { REGISTER_OBJ_FACTORY(BandPreloadPanel); }

    bool mLockInProgress; // 0x78 (compiler-verified, not the stale 0x69 comment)
    LockStepMgr *mPreloadLock; // 0x7c (compiler-verified, not the stale 0x6c comment)
    // NOTE(laneCL2-0802): a prior lane (laneNCCC-0730-8ce6/f30s) added an `int mUnknown_0x80`
    // placeholder here on the premise that retail sizeof(BandPreloadPanel) == 0xb0 (176), read
    // from BandPreloadPanel::NewObject's `li r3, 0xb0`. At HEAD that is BACKWARDS: objdiff's
    // full instruction listing for ?NewObject@BandPreloadPanel@@SAPAVObject@Hmx@@XZ reads
    //     [5] diff_arg: `li r3, 0xac`  (TARGET)  vs  `li r3, 0xb0`  (BASE)
    // i.e. 0xb0 was OUR side and retail is 0xac (172) -- exactly PreloadPanel + the two members
    // above, with no extra field. The placeholder was the thing making us 4 bytes too big, so it
    // is removed and the function now matches all-equal.
    // Sibling panels are NOT the same defect and must not be "fixed" the same way: under the
    // same diff CharacterCreatorPanel and TourDescPanel report `off:-4` (ours too SMALL) while
    // this one reported `off:+4`. Opposite signs => independent per-class members, not a shared
    // base-class deficit.
};