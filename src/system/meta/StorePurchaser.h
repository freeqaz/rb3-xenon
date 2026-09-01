#pragma once

#include "obj/Data.h"
#include "obj/Object.h"
#include "stl/_vector.h"
#include "ui/UI.h"
#include "utl/Symbol.h"

enum PurchaseState { // just know the val of kPurchaseSuccess
    purchasestate0 = 0,
    purchasestate1 = 1,
    kPurchaseSuccess = 2, // renamed from kSuccess to avoid colliding with the
                          // unscoped JoinResponseError::kSuccess (net/SessionMessages.h)
                          // when both headers land in one TU; value (2) preserved.
    purchasestate3 = 3,
};

class StorePurchaser {
public:
    /* Retail ??_7StorePurchaser@@6B@ @ 0x8211523c is SIX slots: a destructor
     * followed by five _purecall (0x828299b8), and the next word (0x821ec094)
     * is XboxPurchaser's ??_R4 -- the adjacent class's table, so there is no
     * seventh slot.  NeedsEnum() has a body and therefore could never be one
     * of those five purecalls; it is a DC3-era addition that RB3 does not
     * have, and it was deleted here.  See
     * docs/decomp/VTABLE_COUNT_PURCHASER_2026-08-27.md. */
    virtual ~StorePurchaser() {}
    virtual void Initiate() = 0;
    /* Slot ORDER is likewise read out of retail's own machine code, via
     * XboxPurchaser's table @ 0x82115258 (the base's own five slots are all
     * _purecall and carry no body evidence):
     *   [1] 0x827b2928  XShowMarketplaceDownloadItemsUI, tests 0x3E5  -> Initiate
     *   [2] 0x827b2828  reads mState@0xc vs {0,2,3} -> bool          -> IsPurchasing
     *   [3] 0x827b2858  calls slot 2, then mState-2/cntlzw/extract    -> IsSuccess
     *   [4] 0x827ca3a8  lbz r3,0x1c(r3); blr                          -> PurchaseMade
     *   [5] 0x827b2c30  XGetOverlappedResult, tests 0x3E4/0x4C7       -> Poll
     * Slot 5 writes `stb r11,0x1c(r30)` on all four exit paths, i.e. Poll
     * computes exactly the byte slot 4 returns -- which closes 4 and 5 on each
     * other.  `mState - 2 -> cntlzw -> bit extract` is `== kPurchaseSuccess`,
     * so slot 3 is IsSuccess; PurchaseMade cannot produce it.
     * ⛔ The previous comment here claimed 0xc PurchaseMade / 0x10 IsSuccess
     * "from StorePanel::Poll's inlined slot loads".  That instrument is
     * AMBIGUOUS: our own callers disagree on evaluation order -- StorePanel.cpp
     * :202 is `PurchaseMade() && IsSuccess()` while TokenRedemptionPanel.cpp
     * :85 is IsSuccess() first -- so "first slot loaded" answers differently
     * depending on which caller you read.  Bodies have no such ambiguity. */
    virtual bool IsPurchasing() const = 0;
    virtual bool IsSuccess() const = 0;
    virtual bool PurchaseMade() const = 0;
    virtual void Poll() = 0;

    StorePurchaser(Symbol s, unsigned int i) : mSource(s), mUserIndex(i) {}

    Symbol mSource;
    int mUserIndex;
};

class XboxPurchaser : public StorePurchaser, public Hmx::Object {
public:
    // Hmx::Object
    virtual ~XboxPurchaser();
    virtual DataNode Handle(DataArray *, bool);

    // StorePurchaser
    virtual void Initiate();
    virtual bool IsPurchasing() const;
    virtual bool IsSuccess() const;
    virtual bool PurchaseMade() const;
    virtual void Poll() {}

    XboxPurchaser(
        int,
        unsigned long long,
        unsigned long long,
        unsigned long long,
        Symbol,
        unsigned int
    );

    PurchaseState mState; // 0x34
    u32 unk3c;
    unsigned long long mOfferID;
    int mUserIndex;

private:
    DataNode OnMsg(UIChangedMsg const &);
};

class XboxMultipleItemsPurchaser : public StorePurchaser, Hmx::Object {
public:
    // Hmx::Object
    virtual ~XboxMultipleItemsPurchaser();
    virtual DataNode Handle(DataArray *, bool);

    // StorePurchaser
    virtual void Initiate();
    virtual bool IsPurchasing() const;
    virtual bool IsSuccess() const;
    virtual bool PurchaseMade() const;
    virtual void Poll() {}

    XboxMultipleItemsPurchaser(
        int, std::vector<unsigned long long> &, Symbol, unsigned int
    );

    PurchaseState mState;                  // 0x34 - Current purchase state
    std::vector<unsigned long long> mOfferIDs; // Offer IDs to purchase
    int mUserIndex;                             // User index
    DWORD mSelectedCount;                       // Count of items selected by user

private:
    DataNode OnMsg(UIChangedMsg const &);
};
