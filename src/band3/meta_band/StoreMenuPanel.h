#pragma once
#include "meta_band/StoreMenuProvider.h"
#include "system/ui/UIPanel.h"
#include <vector>

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

    // Retail 360: third arg is the explicit menu index (-1 = push after
    // current); OnMsg passes 0 to replace the root menu.
    void AddMenu(DataArray *, const char *, int);
    DataNode OnBack(const DataArray *);
    DataNode OnMsg(const MetadataLoadedMsg &);
    const char *GetCrumbText() const;
    void SetPendingMenuIx(int);

    std::vector<StoreMenuProvider *> mMenuStack; // 0x3c
    int mCurrentMenuIx; // 0x48
    int mPendingMenuIx; // 0x4c
    BandList *mList; // 0x50
    int mStartingHighlightIx; // 0x54
};
