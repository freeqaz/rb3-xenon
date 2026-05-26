#pragma once
#include "meta_band/StoreMenuProvider.h"
#include "stl/pointers/_vector.h"
#include "system/ui/UIPanel.h"

class BandList;
class MetadataLoadedMsg;

class StoreMenuPanel : public UIPanel {
public:
    StoreMenuPanel();
    OBJ_CLASSNAME(StoreMenuPanel);
    OBJ_SET_TYPE(StoreMenuPanel);
    NEW_OBJ(StoreMenuPanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~StoreMenuPanel();
    virtual void FinishLoad();
    virtual void Unload();
    virtual void Enter();
    virtual void Exit();
    virtual void Poll();

    void AddMenu(DataArray *, const char *);
    DataNode OnBack(const DataArray *);
    DataNode OnMsg(const MetadataLoadedMsg &);
    const char *GetCrumbText() const;
    void SetPendingMenuIx(int);

    static StoreMenuPanel *sInstance() { return inst; }

    std::vector<StoreMenuProvider *> mMenuStack; // 0x38
    int mCurrentMenuIx; // 0x40
    int mPendingMenuIx; // 0x44
    BandList *mList; // 0x48
    int mStartingHighlightIx; // 0x4c

    static StoreMenuPanel *inst;
};
