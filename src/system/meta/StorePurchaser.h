#pragma once

#include "obj/Data.h"
#include "obj/Object.h"
#include "stl/_vector.h"
#include "ui/UI.h"
#include "utl/Symbol.h"

enum PurchaseState { // just know the val of kSuccess
    purchasestate0 = 0,
    purchasestate1 = 1,
    kSuccess = 2,
    purchasestate3 = 3,
};

class StorePurchaser {
public:
    virtual ~StorePurchaser() {}
    virtual void Initiate() = 0;
    virtual bool IsPurchasing() const = 0;
    virtual bool IsSuccess() const = 0;
    virtual bool PurchaseMade() const = 0;
    virtual bool NeedsEnum() const { return true; }
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
