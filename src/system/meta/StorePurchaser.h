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
    virtual bool IsPurchasing() const = 0;
    virtual bool PurchaseMade() const = 0;
    virtual bool IsSuccess() const = 0;
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
