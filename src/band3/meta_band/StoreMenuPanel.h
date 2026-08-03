#pragma once
#include "meta_band/StoreMenuProvider.h"
#include "system/ui/UIPanel.h"
#include <vector>

class BandList;
class MultipleItemsEnumCompleteMsg;

class StoreMenuPanel : public UIPanel {
public:
    StoreMenuPanel();
    OBJ_CLASSNAME(StoreMenuPanel);
    OBJ_SET_TYPE(StoreMenuPanel);
    NEW_OBJ(StoreMenuPanel);
    virtual DataNode Handle(DataArray *, bool);
    // NO user-declared destructor: retail's ??1StoreMenuPanel@@UAA@XZ emits no
    // vfptr/vtordisp re-initialization, which MSVC only generates for a
    // USER-DECLARED dtor. The implicit one still destroys mMenuStack and calls
    // ~UIPanel (matches retail exactly). Cf. MoviePanel/ChooseColorPanel (no
    // declared dtor, no stores) vs DeJitterPanel/TrainingPanel (declared, stores).
    virtual void FinishLoad();
    virtual void Unload();
    virtual void Enter();
    virtual void Exit();
    virtual void Poll();

    // Retail 360: third arg is the explicit menu index (-1 = push after
    // current); OnMsg passes 0 to replace the root menu.
    void AddMenu(DataArray *, const char *, int);
    DataNode OnBack(const DataArray *);
    DataNode OnMsg(const MultipleItemsEnumCompleteMsg &);
    const char *GetCrumbText() const;
    void SetPendingMenuIx(int);

    std::vector<StoreMenuProvider *> mMenuStack; // 0x3c
    int mCurrentMenuIx; // 0x48
    int mPendingMenuIx; // 0x4c
    BandList *mList; // 0x50
    int mStartingHighlightIx; // 0x54
};
