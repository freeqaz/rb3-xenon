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
    virtual ~StorePurchaser() {}
    virtual void Initiate() = 0;
    // Retail vtable order (from StorePanel::Poll's inlined slot loads):
    // 0x8 IsPurchasing, 0xc PurchaseMade, 0x10 IsSuccess, 0x14 Poll.
    // NeedsEnum is a DC3-era addition with no RB3 call sites, so it lands
    // after Poll where it cannot shift the retail slots.
    virtual bool IsPurchasing() const = 0;
    virtual bool PurchaseMade() const = 0;
    virtual bool IsSuccess() const = 0;
    virtual void Poll() = 0;
    virtual bool NeedsEnum() const { return true; }

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
    virtual bool NeedsEnum() const { return false; }
    virtual void Poll() {}

    XboxPurchaser(
        int,
        unsigned long long,
        unsigned long long,
        unsigned long long,
        Symbol,
        unsigned int
    );

    PurchaseState mState; // 0x38
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
    virtual bool NeedsEnum() const { return false; }
    virtual void Poll() {}

    XboxMultipleItemsPurchaser(
        int, std::vector<unsigned long long> &, Symbol, unsigned int
    );

    PurchaseState mState;                  // 0x38 - Current purchase state
    std::vector<unsigned long long> mOfferIDs; // Offer IDs to purchase
    int mUserIndex;                             // User index
    DWORD mSelectedCount;                       // Count of items selected by user

private:
    DataNode OnMsg(UIChangedMsg const &);
};
