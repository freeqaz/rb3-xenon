#include "meta/StorePurchaser.h"
#include "meta/StoreOffer.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"
#include "ui/UI.h"
#include "utl/Symbol.h"
#include "xdk/xapilibi/xbox.h"

extern "C" DWORD XShowMarketplaceDownloadItemsUI(
    DWORD, DWORD, ULONGLONG *, DWORD, DWORD *, XOVERLAPPED *
);

#pragma region XboxPurchaser

XboxPurchaser::XboxPurchaser(
    int param1,
    unsigned long long param2,
    unsigned long long param3,
    unsigned long long param4,
    Symbol s,
    unsigned int ui
)
    : StorePurchaser(s, ui), mState(purchasestate0), mOfferID(param2), mUserIndex(param1) {}

XboxPurchaser::~XboxPurchaser() {
    static Symbol ui_changed("ui_changed");
    ThePlatformMgr.RemoveSink(this, ui_changed);
}

void XboxPurchaser::Initiate() {
    MILO_ASSERT(!IsPurchasing(), 0x39a);
    mState = purchasestate1;

    unsigned long trackingID;
    unsigned long ret;
    if (PlatformMgr::sXShowCallback(trackingID)) {
        ret = XShowNuiMarketplaceUI(
            trackingID, mUserIndex, XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTITEM_BACKGROUND, mOfferID, -1
        );
    } else {
        ret = XShowMarketplaceUI(
            mUserIndex, XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTITEM_BACKGROUND, mOfferID, -1
        );
    }

    if (ret != ERROR_SUCCESS) {
        TheDebug.Notify(MakeString("Error starting checkout UI: %d", ret));
        mState = purchasestate3;
    }

    // Register for UI changed notifications to detect when purchase UI closes
    static Symbol ui_changed("ui_changed");
    ThePlatformMgr.AddSink(this, ui_changed);
}

bool XboxPurchaser::IsSuccess() const {
    MILO_ASSERT(!IsPurchasing(), 0x3c3);
    return mState == kSuccess;
}

bool XboxPurchaser::PurchaseMade() const {
    MILO_ASSERT(mState == kSuccess, 0x3c9);
    return false;
}

bool XboxPurchaser::IsPurchasing() const {
    return mState == purchasestate1;
}

DataNode XboxPurchaser::OnMsg(UIChangedMsg const &msg) {
    if (mState == purchasestate1) {
        if (!msg.Showing()) {
            // UI closed - unregister from notifications and mark as successful
            static Symbol ui_changed("ui_changed");
            ThePlatformMgr.RemoveSink(this, ui_changed);
            mState = kSuccess;
        }
    }
    return DataNode();
}

BEGIN_HANDLERS(XboxPurchaser)
    HANDLE_MESSAGE(UIChangedMsg)
END_HANDLERS

#pragma endregion XboxPurchaser
#pragma region XboxMultipleItemsPurchaser

bool XboxMultipleItemsPurchaser::IsSuccess() const {
    MILO_ASSERT(!IsPurchasing(), 0x365);
    return mState == kSuccess;
}

bool XboxMultipleItemsPurchaser::PurchaseMade() const {
    MILO_ASSERT(mState == kSuccess, 0x36b);
    return false;
}

bool XboxMultipleItemsPurchaser::IsPurchasing() const {
    return mState != purchasestate0 && mState != kSuccess && mState != purchasestate3;
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
        TheDebug.Notify(MakeString("Error starting checkout UI: %d", result));
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
            mState = kSuccess;
        }
    }
    return DataNode();
}

BEGIN_HANDLERS(XboxMultipleItemsPurchaser)
    HANDLE_MESSAGE(UIChangedMsg)
END_HANDLERS

#pragma endregion XboxMultipleItemsPurchaser
