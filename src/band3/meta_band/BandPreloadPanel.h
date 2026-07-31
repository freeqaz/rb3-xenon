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
    // NOTE(laneNCCC-0730-8ce6/f30s): retail sizeof(BandPreloadPanel) == 0xb0 (176) per
    // BandPreloadPanel::NewObject's `li r3, 0xb0`, but PreloadPanel + the two members above
    // compute to only 0xac (172) -- 4 bytes short. PreloadPanel's OWN layout is independently
    // verified correct (PreloadPanel::NewObject matches 100% all-equal at sizeof==0xa4/164, same
    // relative vtordisp-for-Object offset used here), so the deficit is provably inside
    // BandPreloadPanel's own member region, not a shared base. Exact identity of the missing
    // 4-byte member is UNKNOWN: BandPreloadPanel.cpp is not yet a compiled/split unit in this
    // repo, so its ctor/Load/PollForLoading/IsLoaded/OnMsg bodies can't be cross-checked against
    // retail offsets to name it. This placeholder restores the correct total size (fixes
    // NewObject's operator-new size immediate); replace with the real member once
    // BandPreloadPanel.cpp is split and its methods can be inspected.
    int mUnknown_0x80; // 0x80 -- placeholder for retail's missing 4 bytes, see note above
};