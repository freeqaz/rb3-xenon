#include "meta_band/TokenRedemptionPanel.h"
#include "decomp.h"
#include "game/BandUser.h"
#include "meta/StorePackedMetadata.h"
#include "meta/StoreEnumeration.h"
#include "meta/StorePurchaser.h"
#include "meta_band/AppLabel.h"
#include "meta_band/InputMgr.h"
#include "net/Net.h"
#include "net/Server.h"
#include "net_band/RockCentral.h"
#include "net_band/RockCentralMsgs.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "os/JoypadMsgs.h"
#include "ui/UIListLabel.h"
#include "ui/UIPanel.h"
#include "utl/Std.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols3.h"

TokenRedemptionPanel::TokenRedemptionPanel()
    : mRedemptionState(0), mListData(0, String()), mSelectedOfferIndex(0),
      mEnumeration(0), mPurchaser(0) {}

void TokenRedemptionPanel::Text(int, int data, UIListLabel *slot, UILabel *label) const {
    MILO_ASSERT(mListData.size() > data, 0x26);
    if (slot->Matches("name")) {
        AppLabel *appLabel = dynamic_cast<AppLabel *>(label);
        appLabel->SetTokenRedemptionString(this, data);
    } else
        label->SetTextToken(gNullStr);
}

int TokenRedemptionPanel::NumData() const { return mListData.size(); }
const char *TokenRedemptionPanel::GetListString(int i) const {
    return mListData[i].c_str();
}

void TokenRedemptionPanel::Enter() {
    UIPanel::Enter();
    mRedemptionState = 0;
}

UNPOOL_DATA
void TokenRedemptionPanel::Poll() {
    UIPanel::Poll();
    switch (mRedemptionState) {
    case kEnumeratingOffers:
    case kEnumeratingPreviousOffers:
        MILO_ASSERT(mEnumeration, 0x4A);
        mEnumeration->Poll();
        if (!mEnumeration->IsEnumerating()) {
            bool succ = mEnumeration->IsSuccess();
            if (!succ && mRedemptionState == kEnumeratingPreviousOffers) {
                succ = true;
            }
            if (succ) {
                if (mRedemptionState != kEnumeratingPreviousOffers) {
                    static Message token_msg("token_redemption_msg", gNullStr);
                    token_msg[0] = token_redemption_ready;
                    HandleType(token_msg);
                } else {
                    static Message offersReadyMsg("token_offers_ready");
                    HandleType(offersReadyMsg);
                }
            } else {
                static Message token_msg("token_redemption_msg", gNullStr);
                if (mRedemptionState == kEnumeratingPreviousOffers) {
                    token_msg[0] = token_error_no_previous_offers;
                } else {
                    token_msg[0] = token_redemption_error;
                }
                HandleType(token_msg);
            }
            mRedemptionState = 0;
            RELEASE(mEnumeration);
        }
        break;
    case kPurchasing:
        MILO_ASSERT(mPurchaser, 0xA8);
        mPurchaser->Poll();
        if (!mPurchaser->IsPurchasing()) {
            if (mPurchaser->IsSuccess()) {
                bool result = mPurchaser->PurchaseMade();
                mRedemptionState = 0;
                static Message checkout_msg("checkout_finished", 0);
                checkout_msg[0] = result;
                HandleType(checkout_msg);
            } else {
                mRedemptionState = 0;
                static Message token_msg("token_redemption_msg", gNullStr);
                token_msg[0] = token_redemption_error;
                HandleType(token_msg);
            }
            MILO_ASSERT(mRedemptionState != kPurchasing, 0xCB);
            RELEASE(mPurchaser);
        }
        break;
    default:
        break;
    }
}
END_UNPOOL_DATA

void TokenRedemptionPanel::Unload() {
    TheRockCentral.CancelOutstandingCalls(this);
    mResultList.Clear();
    mListData.clear();
    RELEASE(mEnumeration);
    RELEASE(mPurchaser);
    UIPanel::Unload();
}

// Retail fn_8263FAA0. The rb3-Wii oracle reads `TheServer.GetMasterProfileID()`;
// retail's 360 body instead dispatches Server slot 5 (0x14) IsConnected() and,
// only when connected, slot 7 (0x1c) GetPlayerID(user->GetPadNum()) -- the pad
// number arriving in r4 from a vbtable-adjusted slot-0 vcall on the user:
//     lwz  r3, lbl_82C6EB50@l(r30)   ; TheServer (a reference => pointer load)
//     lwz  r11,0x0(r3); lwz r11,0x14(r11); bctrl        ; IsConnected()
//     clrlwi. r11,r3,24 ; beq .L_8251DC54               ; id stays 0 if offline
//     lwz  r10,0x4(r29); lwz r10,0xc(r10); add r11,r10,r29
//     addi r3,r11,0x4 ; lwz r11,0x4(r11); lwz r11,0x0(r11); bctrl ; GetPadNum()
//     lwz  r11,0x1c(r28) ; mr r4,r3 ; r3 = TheServer ; bctrl       ; GetPlayerID
// Same wrong-callee bug W16-G found in StoreInfoPanel::GetRecommendationIndexPath,
// and the same one in GetPreviousOffersForUser / ShowPurchaseUIForOffer below.
void TokenRedemptionPanel::GetOffersForToken(const char *token, LocalBandUser *user) {
    mActiveToken = token;
    mResultList.Clear();
    int id = 0;
    if (TheServer.IsConnected()) {
        id = TheServer.GetPlayerID(user->GetPadNum());
    }
    mRedemptionState = kRequestingOffers;
    TheRockCentral.RedeemToken(id, mActiveToken.c_str(), mResultList, this);
}

// Retail fn_8263F948 -- identical server shape to GetOffersForToken above.
void TokenRedemptionPanel::GetPreviousOffersForUser(LocalBandUser *user) {
    mResultList.Clear();
    int id = 0;
    if (TheServer.IsConnected()) {
        id = TheServer.GetPlayerID(user->GetPadNum());
    }
    mRedemptionState = kRequestingPreviousOffers;
    TheRockCentral.GetRedeemedTokensByPlayer(id, mResultList, this);
}

void TokenRedemptionPanel::EnumerateOffers(LocalBandUser *user) {
    // Xbox commerce drift: retail uses XboxEnumeration + a private redemptions
    // table rather than the Wii oracle's WiiEnumeration/StoreRedemptionsTable.
    // Compile-only adaptation; this body is not byte-matched (see report notes).
    std::list<DataResult> &dataList = mResultList.mDataResultList;
    DataNode node(0);
    std::vector<unsigned long long> offerIds;
    std::list<DataResult>::iterator end = dataList.end();
    std::list<DataResult>::iterator it = dataList.begin();
    for (; it != end; ++it) {
        it->GetDataResultValue(String("offer"), node);
        offerIds.push_back(0);
    }
    int count = offerIds.size();
    if (count == 0) {
        static Message token_msg("token_redemption_msg", gNullStr);
        if (mRedemptionState == kEnumeratingPreviousOffers) {
            token_msg[0] = token_error_no_previous_offers;
        } else {
            token_msg[0] = token_redemption_error;
        }
        HandleType(token_msg);
        mRedemptionState = 0;
        return;
    }
    MILO_ASSERT(!mEnumeration, 0x14A);
    mEnumeration = new XboxEnumeration(user ? user->GetPadNum() : 0, &offerIds);
    mEnumeration->Start();
}

void TokenRedemptionPanel::ShowPurchaseUIForOffer(int ix, LocalBandUser *user) {
    MILO_ASSERT(mRedemptionState == kIdle, 0x15A);
    MILO_ASSERT(mListData.size() > ix, 0x15B);
    MILO_ASSERT(!mPurchaser, 0x15C);
    MILO_ASSERT(user, 0x15F);
    Server *server = TheNet.GetServer();
    if (server && server->IsConnected()) {
        server->GetPlayerID(user->GetPadNum());
    }
    mPurchaser = NULL;
    mRedemptionState = kPurchasing; // retail fn_8263FB98 stores 5
}

DataNode TokenRedemptionPanel::OnMsg(const ButtonDownMsg &msg) {
    if (mRedemptionState == kIdle) {
        return DataNode(kDataUnhandled, 0);
    }
    return 1;
}

DataNode TokenRedemptionPanel::OnMsg(const RockCentralOpCompleteMsg &msg) {
    static Message token_msg("token_redemption_msg", gNullStr);
    static Symbol token_redemption_ready("token_redemption_ready");
    static Symbol token_redemption_purchased("token_redemption_purchased");
    static Symbol token_redemption_not_found("token_redemption_not_found");
    static Symbol token_redemption_other_player("token_redemption_other_player");
    static Symbol token_redemption_too_late("token_redemption_too_late");
    static Symbol token_redemption_too_early("token_redemption_too_early");
    static Symbol token_redemption_wrong_platform("token_redemption_wrong_platform");
    static Symbol token_redemption_error("token_redemption_error");
    static Symbol token_error_no_previous_offers("token_error_no_previous_offers");
    int state = mRedemptionState;
    if (state != kRequestingOffers && state != kRequestingPreviousOffers) {
        return 1;
    }
    Symbol errSym = (state == kRequestingPreviousOffers)
        ? token_error_no_previous_offers
        : token_redemption_error;
    if (msg.Success()) {
        DataNode statusNode(0);
        mResultList.Update(NULL);
        if (mResultList.mDataResultList.empty()
            && mRedemptionState == kRequestingPreviousOffers) {
            LocalBandUser *u = TheInputMgr->GetUser()
                ? TheInputMgr->GetUser()->GetLocalBandUser()
                : NULL;
            mRedemptionState = kEnumeratingPreviousOffers;
            EnumerateOffers(u);
            return 1;
        }
        mResultList.GetDataResult(0)->GetDataResultValue(String("status"), statusNode);
        int status = statusNode.Int(NULL);
        switch (status) {
        case 0xA0002:
            MILO_ASSERT(mRedemptionState == kRequestingPreviousOffers, 0x1D1);
            mRedemptionState = kEnumeratingPreviousOffers;
            {
                LocalBandUser *u = TheInputMgr->GetUser()
                    ? TheInputMgr->GetUser()->GetLocalBandUser()
                    : NULL;
                EnumerateOffers(u);
            }
            return 1;
        case 0xA0005:
        case 0xA0007:
            MILO_ASSERT(mRedemptionState == kRequestingOffers, 0x1DF);
            mRedemptionState = kEnumeratingOffers;
            {
                LocalBandUser *u = TheInputMgr->GetUser()
                    ? TheInputMgr->GetUser()->GetLocalBandUser()
                    : NULL;
                EnumerateOffers(u);
            }
            return 1;
        case 0xA0006: {
            static Symbol token_redemption_purchased("token_redemption_purchased");
            MILO_ASSERT(mRedemptionState == kReportingPurchase, 0x1EA);
            token_msg[0] = token_redemption_purchased;
            break;
        }
        case 0x800A0003: {
            static Symbol token_redemption_not_found("token_redemption_not_found");
            MILO_ASSERT(mRedemptionState == kRequestingOffers, 0x1F1);
            mResultList.Clear();
            token_msg[0] = token_redemption_not_found;
            break;
        }
        case 0x800A0005: {
            static Symbol token_redemption_other_player("token_redemption_other_player");
            MILO_ASSERT(mRedemptionState == kRequestingOffers, 0x1F7);
            mResultList.Clear();
            token_msg[0] = token_redemption_other_player;
            break;
        }
        case 0x800A0008: {
            static Symbol token_redemption_too_late("token_redemption_too_late");
            MILO_ASSERT(mRedemptionState == kRequestingOffers, 0x1FD);
            mResultList.Clear();
            token_msg[0] = token_redemption_too_late;
            break;
        }
        case 0x800A0009: {
            static Symbol token_redemption_too_early("token_redemption_too_early");
            MILO_ASSERT(mRedemptionState == kRequestingOffers, 0x203);
            mResultList.Clear();
            token_msg[0] = token_redemption_too_early;
            break;
        }
        case 0x800A000B: {
            static Symbol token_redemption_wrong_platform("token_redemption_wrong_platform");
            MILO_ASSERT(mRedemptionState == kRequestingOffers, 0x209);
            mResultList.Clear();
            token_msg[0] = token_redemption_wrong_platform;
            break;
        }
        default:
            token_msg[0] = errSym;
            break;
        }
    } else {
        if (mRedemptionState == kRequestingPreviousOffers) {
            LocalBandUser *u = TheInputMgr->GetUser()
                ? TheInputMgr->GetUser()->GetLocalBandUser()
                : NULL;
            mRedemptionState = kEnumeratingPreviousOffers;
            EnumerateOffers(u);
            return 1;
        }
        token_msg[0] = errSym;
    }
    mRedemptionState = kIdle;
    HandleType(token_msg);
    return 1;
}

BEGIN_HANDLERS(TokenRedemptionPanel)
    HANDLE_ACTION(
        get_offers_for_token, GetOffersForToken(_msg->Str(2), _msg->Obj<LocalBandUser>(3))
    )
    HANDLE_ACTION(
        get_previous_offers, GetPreviousOffersForUser(_msg->Obj<LocalBandUser>(2))
    )
    HANDLE_ACTION(
        show_purchase_ui_for_offer,
        ShowPurchaseUIForOffer(_msg->Int(2), _msg->Obj<LocalBandUser>(3))
    )
    HANDLE_MESSAGE(ButtonDownMsg)
    HANDLE_MESSAGE(RockCentralOpCompleteMsg)
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_CHECK(0x23C)
END_HANDLERS