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

/* Retail's dtor @0x827b28a0 cancels the in-flight marketplace call --
 *   if (IsPurchasing() && sOverlapped.InternalLow == ERROR_IO_PENDING)
 *       XCancelOverlapped(&sOverlapped);
 * -- via a class-static XOVERLAPPED at 0x82e0684c that this port does not
 * model yet (Initiate below is likewise not the retail body).  NOT ported here
 * deliberately: that is a body port, not a layout fix, and it would widen this
 * change past what the retail layout evidence supports.  What retail's dtor
 * demonstrably does NOT do is remove a message sink -- there is no Hmx::Object
 * subobject to be one. */
XboxPurchaser::~XboxPurchaser() {}

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
        MILO_NOTIFY("Error starting checkout UI: %d", ret);
        mState = purchasestate3;
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
    return mState == purchasestate1;
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
