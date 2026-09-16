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

/* ⛔ XboxPurchaser does NOT derive from Hmx::Object in RB3 retail.  DC3's
 * header (src/system/meta/StorePurchaser.h:40) declares
 * `: public StorePurchaser, public Hmx::Object` and this file inherited that
 * verbatim -- but DC3 is NEWER than RB3, and the Object base is one of its
 * additions.  FOUR independent retail instruments agree (lane W16-EM,
 * 2026-09-16; see docs/decomp/W16EM_XBOXPURCHASER_NO_OBJECT_BASE_2026-09-16.md):
 *
 *  1. RTTI.  ??_R4 @0x821ec094 -> ??_R3 has numBaseClasses = 2 and
 *     attributes = 0x0 (the 0x1 multiple-inheritance bit is CLEAR); the ??_R2
 *     base-class array is exactly {XboxPurchaser mdisp=0, StorePurchaser
 *     mdisp=0}.  With an Object base it would list FOUR entries -- Object@Hmx
 *     and its own base ObjRef -- and set attributes=1.
 *  2. Exactly ONE ??_R4 in all of .rdata references XboxPurchaser's type
 *     descriptor.  A class with two vptrs emits one COL per vtable.
 *  3. The ctor @0x827b2800 stores ONE vptr, `stw r11,0x0(r3)`, and calls no
 *     base ctor.
 *  4. The dtor @0x827b28a0 writes that same single slot twice (derived table
 *     0x82115258, then base table 0x8211523c) and calls no Object dtor.
 *
 * Consequence: sizeof is 0x28 (40), not 0x50 (80).  THREE call sites allocate
 * it and all three say 40: UGCPurchasePanel::Poll (0x8263edf0),
 * TokenRedemptionPanel::ShowPurchaseUIForOffer (0x8263fb98) and StandIn
 * (0x825ecb54) each do `li r3,0x28` -> `bl fn_827bd2f0` -> `bl fn_827b2800`.
 *
 * The member offsets below are READ OFF retail instructions, not inferred, and
 * no member is invented to reach the size -- each one is load-bearing at a
 * named address (cf. the SetlistArtRecord precedent against padding to size):
 *   0xc  mState        ctor `stw r10,0xc(r3)` (=0); IsPurchasing @0x827b2828
 *                      tests it against {0,2,3}; Poll @0x827b2c30 vs 1.
 *   0x10 mOfferID      ctor `std r5,0x10(r3)` -- an EIGHT-byte store; Initiate
 *                      @0x827b2928 passes `addi r5,r31,0x10` as the
 *                      XShowMarketplaceDownloadItemsUI pOfferIDs argument.
 *   0x18 mUserIndex    ctor `stw r4,0x18(r3)`; Initiate loads it with
 *                      `lwz r3,0x18(r31)` as that API's dwUserIndex.
 *   0x1c mPurchaseMade Poll writes `stb r11,0x1c(r30)` on all four exit paths;
 *                      vtable slot 4 is `lbz r3,0x1c(r3); blr`.
 *   0x20 mResult       Initiate passes `addi r7,r31,0x20` as phrResult and
 *                      zeroes it; Poll reads `lwz r10,0x20(r30)` and compares
 *                      it against 0x8057F001/2/3 and S_OK.
 * 0x20 + 4 = 0x24, rounded to the 8-byte alignment forced by mOfferID = 0x28.
 *
 * There is therefore no Handle()/OnMsg()/BEGIN_HANDLERS and no
 * AddSink/RemoveSink: retail's Initiate registers no sink at all, and Poll
 * drives the purchase by XGetOverlappedResult instead. */
class XboxPurchaser : public StorePurchaser {
public:
    // StorePurchaser
    virtual ~XboxPurchaser();
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

    PurchaseState mState;        // 0xc
    unsigned long long mOfferID; // 0x10
    int mUserIndex;              // 0x18
    bool mPurchaseMade;          // 0x1c
    DWORD mResult;               // 0x20
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
