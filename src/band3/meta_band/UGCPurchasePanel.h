#pragma once
#include "game/BandUser.h"
#include "meta/StoreEnumeration.h"
#include "meta/StorePurchaser.h"
#include "net_band/DataResults.h"
#include "net_band/RockCentralMsgs.h"
#include "ui/UIPanel.h"

class UGCPurchasePanel : public UIPanel {
public:
    enum PurchaseState {
        kUninitialized = 0
    };
    UGCPurchasePanel();
    OBJ_CLASSNAME(UGCPurchasePanel);
    OBJ_SET_TYPE(UGCPurchasePanel);
    NEW_OBJ(UGCPurchasePanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~UGCPurchasePanel() {}
    virtual void Enter();
    virtual void Exit();
    virtual void Poll();
    virtual void Unload();
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);

    DataNode OnMsg(const SigninChangedMsg &);
    DataNode OnMsg(const RockCentralOpCompleteMsg &);

    int mPurchaseState; // 0x3c
    LocalBandUser *mUser; // 0x40
    Symbol mSong; // 0x40
    const char *mOfferID; // 0x48
    StorePurchaser *mPurchaser; // 0x4c
    bool unk4c; // 0x50
    DataResultList mResultList; // 0x50
};