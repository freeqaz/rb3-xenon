#include "meta/StorePurchaser.h"
#include "meta/StoreOffer.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"
#include "ui/UI.h"
#include "utl/DataPointMgr.h"
#include "utl/Str.h"
#include "utl/Symbol.h"
#include "xdk/xapilibi/xbox.h"

extern "C" DWORD XShowMarketplaceDownloadItemsUI(
    DWORD, DWORD, ULONGLONG *, DWORD, HRESULT *, XOVERLAPPED *
);

#pragma region XboxPurchaser

/* Retail ctor @0x827b2800, in its own store order:
 *   stw r8,0x4   mSource      stw r9,0x8   mUserIndex  (StorePurchaser)
 *   std r5,0x10  mOfferID     stw r10,0xc  mState = 0
 *   stw r11,0x0  vptr         stw r4,0x18  mUserIndex  (XboxPurchaser's own)
 * Note it leaves mPurchaseMade and mResult UNINITIALISED -- do not add them to
 * the initialiser list. */
XboxPurchaser::XboxPurchaser(
    int param1,
    unsigned long long param2,
    unsigned long long param3,
    unsigned long long param4,
    Symbol s,
    unsigned int ui
)
    : StorePurchaser(s, ui), mState(purchasestate0), mOfferID(param2), mUserIndex(param1) {}

// The one in-flight marketplace call. Retail keeps a single file-scope
// XOVERLAPPED (0x82e0684c) shared by Initiate, Poll and the dtor.
static XOVERLAPPED sOverlapped;

XboxPurchaser::~XboxPurchaser() {
    if (IsPurchasing() && sOverlapped.InternalLow == ERROR_IO_PENDING) {
        XCancelOverlapped(&sOverlapped);
    }
}

void XboxPurchaser::Initiate() {
    MILO_ASSERT(!IsPurchasing(), 0x39a);
    mState = purchasestate1;
    memset(&sOverlapped, 0, sizeof(XOVERLAPPED));
    mResult = 0;
    if (XShowMarketplaceDownloadItemsUI(
            mUserIndex, 0x3E9, &mOfferID, 1, &mResult, &sOverlapped
        )
        != ERROR_IO_PENDING) {
        mState = purchasestate3;
    }
}

void StorePurchaser::RecordPurchase(const char *offer) {
    static Symbol source("source");
    static Symbol offerSym("offer");
    static Symbol purchaser("purchaser");
    SendDataPoint("store/purchase", source, mSource, offerSym, offer, purchaser, mUserIndex);
}

void XboxPurchaser::Poll() {
    static Symbol xbox("xbox");
    if (mState == purchasestate1) {
        DWORD res;
        DWORD err = XGetOverlappedResult(&sOverlapped, &res, false);
        if (err == ERROR_IO_INCOMPLETE)
            return;
        if (err != ERROR_SUCCESS && err != ERROR_CANCELLED) {
            mState = purchasestate3;
            return;
        }
        mState = kPurchaseSuccess;
        switch (mResult) {
        case 0: { // S_OK
            mPurchaseMade = true;
            String offer;
            StorePurchaseable::IDToOfferString(mOfferID, offer);
            RecordPurchase(offer.c_str());
            break;
        }
        case (HRESULT)0x8057F001:
            mPurchaseMade = true;
            break;
        case (HRESULT)0x8057F002:
            mPurchaseMade = false;
            break;
        case (HRESULT)0x8057F003:
            mPurchaseMade = false;
            break;
        default:
            mPurchaseMade = false;
            break;
        }
    }
}

bool XboxPurchaser::IsSuccess() const {
    MILO_ASSERT(!IsPurchasing(), 0x3c3);
    return mState == kPurchaseSuccess;
}

/* `lbz r3,0x1c(r3); blr` -- vtable slot 4 of ??_7XboxPurchaser (0x82115268).
 * This used to `return false` unconditionally, which is a live behavioural bug:
 * Poll computes the byte at 0x1c on every one of its four exit paths and this
 * is the only reader of it. */
bool XboxPurchaser::PurchaseMade() const {
    MILO_ASSERT(mState == kPurchaseSuccess, 0x3c9);
    return mPurchaseMade;
}

bool XboxPurchaser::IsPurchasing() const {
    return mState != purchasestate0 && mState != kPurchaseSuccess && mState != purchasestate3;
}

#pragma endregion XboxPurchaser
#pragma region XboxMultipleItemsPurchaser

bool XboxMultipleItemsPurchaser::IsSuccess() const {
    MILO_ASSERT(!IsPurchasing(), 0x365);
    return mState == kPurchaseSuccess;
}

bool XboxMultipleItemsPurchaser::PurchaseMade() const {
    MILO_ASSERT(mState == kPurchaseSuccess, 0x36b);
    return false;
}

bool XboxMultipleItemsPurchaser::IsPurchasing() const {
    return mState != purchasestate0 && mState != kPurchaseSuccess && mState != purchasestate3;
}

void XboxMultipleItemsPurchaser::Initiate() {
    MILO_ASSERT(!IsPurchasing(), 0x343);
    mState = purchasestate1;

    // Initialize overlapped structure for async Xbox marketplace operation
    static XOVERLAPPED sOverlapped;
    memset(&sOverlapped, 0, sizeof(XOVERLAPPED));

    mSelectedCount = 0;
    // Show Xbox marketplace UI for purchasing multiple items
    // Returns 0x3E5 (ERROR_IO_PENDING) on success
    unsigned int result = XShowMarketplaceDownloadItemsUI(
        mUserIndex,         // User index
        0x3E9,         // Expected success code
        &mOfferIDs[0],     // Array of offer IDs to purchase
        mOfferIDs.size(),  // Number of offers
        &mSelectedCount,        // [out] Count of items selected by user
        &sOverlapped   // Overlapped I/O structure
    );

    if (result != 0x3E5) {
        MILO_NOTIFY("Error starting checkout UI: %d", result);
        mState = purchasestate3; // Error state
    }

    // Register for UI changed notifications to detect when marketplace closes
    static Symbol ui_changed("ui_changed");
    ThePlatformMgr.AddSink(this, ui_changed);
}

XboxMultipleItemsPurchaser::~XboxMultipleItemsPurchaser() {
    static Symbol ui_changed("ui_changed");
    ThePlatformMgr.RemoveSink(this, ui_changed);
}

XboxMultipleItemsPurchaser::XboxMultipleItemsPurchaser(
    int i, std::vector<unsigned long long> &offerIDs, Symbol s, unsigned int ui
)
    : StorePurchaser(s, ui), mState(purchasestate0), mUserIndex(i) {
    MILO_ASSERT(offerIDs.size() >= 1 && offerIDs.size() <= 6, 0x337);
    mOfferIDs = offerIDs;
}

DataNode XboxMultipleItemsPurchaser::OnMsg(UIChangedMsg const &msg) {
    if (mState == purchasestate1) {
        if (!msg.Showing()) {
            // UI closed successfully - unregister from notifications
            static Symbol ui_changed("ui_changed");
            ThePlatformMgr.RemoveSink(this, ui_changed);
            mState = kPurchaseSuccess;
        }
    }
    return DataNode();
}

BEGIN_HANDLERS(XboxMultipleItemsPurchaser)
    HANDLE_MESSAGE(UIChangedMsg)
END_HANDLERS

#pragma endregion XboxMultipleItemsPurchaser
