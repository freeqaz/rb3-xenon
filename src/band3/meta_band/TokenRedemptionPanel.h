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
    // Retail-360 state values, read off retail bytes -- the rb3-Wii oracle's
    // gapped 0,2,3,5,6,7,8 is WRONG for this binary. Witnessed stores/compares
    // at mRedemptionState (this+0x40) in band3/meta_band/TokenRedemptionPanel.s:
    //   fn_8263FAA0 GetOffersForToken        stw 1
    //   fn_82640288 OnMsg(RockCentralOpComplete) stw 2, stw 4, cmpwi 3
    //   fn_8263F948 GetPreviousOffersForUser stw 3
    //   fn_8263FEB0 EnumerateOffers          cmpwi 4
    //   fn_8263FB98 ShowPurchaseUIForOffer   stw 5   (creates mPurchaser)
    // i.e. retail's enum is CONTIGUOUS. kReportingPurchase is unwitnessed --
    // only its VALUE is observable from bytes and no site emits it, so 6 is the
    // contiguous continuation, not a measurement.
    enum RedemptionState {
        kIdle = 0,
        kRequestingOffers = 1,
        kEnumeratingOffers = 2,
        kRequestingPreviousOffers = 3,
        kEnumeratingPreviousOffers = 4,
        kPurchasing = 5,
        kReportingPurchase = 6
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