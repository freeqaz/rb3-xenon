#pragma once
#include "game/BandUser.h"
#include "meta/StoreEnumeration.h"
class StorePurchaser; // forward-decl to avoid kSuccess enum collision with net/SessionMessages.h
#include "net_band/DataResults.h"
#include "net_band/RockCentralMsgs.h"
#include "os/JoypadMsgs.h"
#include "ui/UIListProvider.h"
#include "ui/UIPanel.h"

class TokenRedemptionPanel : public UIListProvider, public UIPanel {
public:
    enum RedemptionState {
        kIdle = 0,
        kRequestingOffers = 2,
        kEnumeratingOffers = 3,
        kRequestingPreviousOffers = 5,
        kEnumeratingPreviousOffers = 6,
        kPurchasing = 7,
        kReportingPurchase = 8
    };
    TokenRedemptionPanel();
    virtual ~TokenRedemptionPanel() {}
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual int NumData() const;
    OBJ_CLASSNAME(TokenRedemptionPanel);
    OBJ_SET_TYPE(TokenRedemptionPanel);
    NEW_OBJ(TokenRedemptionPanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual void Enter();
    virtual void Poll();
    virtual void Unload();

    const char *GetListString(int) const;
    void GetOffersForToken(const char *, LocalBandUser *);
    void GetPreviousOffersForUser(LocalBandUser *);
    void EnumerateOffers(LocalBandUser *);
    void ShowPurchaseUIForOffer(int, LocalBandUser *);

    DataNode OnMsg(const ButtonDownMsg &);
    DataNode OnMsg(const RockCentralOpCompleteMsg &);

    int mRedemptionState; // 0x40
    String mActiveToken; // 0x44
    DataResultList mResultList; // 0x50
    std::vector<String> mListData; // 0x68
    // Xbox-retail-only: the offer-ID list handed to XboxEnumeration. Absent from
    // the rb3-Wii oracle (Wii uses the TheStoreMetadata.mRedemptionsTable global).
    // Proven by retail asm: ctor zeroes 6 words at 0x74..0x88; ~TokenRedemptionPanel
    // (0x826414C0) inlines ~_Vector_base over 0x74/0x7c with srawi/slwi 3 =>
    // 8-byte POD element; EnumerateOffers (0x8263FEB0) push_back's an `std`-stored
    // doubleword and passes `this+0x74` as arg2 to XboxEnumeration(int, vector<u64>*).
    std::vector<unsigned long long> mOfferIDs; // 0x74
    int mSelectedOfferIndex; // 0x80
    StoreEnumeration *mEnumeration; // 0x84
    StorePurchaser *mPurchaser; // 0x88
};