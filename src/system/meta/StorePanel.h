#pragma once
#include "meta/StorePurchaser.h"
#include "meta/Profile.h"
#include "meta/StoreEnumeration.h"
#include "meta/StoreOffer.h"
#include "meta/StorePreviewMgr.h"
#include "obj/Data.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "os/PlatformMgr.h"
#include "rndobj/Tex.h"
#include "stl/_vector.h"
#include "types.h"
#include "ui/UIPanel.h"
#include "utl/NetCacheLoader.h"
#include "utl/Symbol.h"
#include <list>

DECLARE_MESSAGE(MultipleItemsEnumCompleteMsg, "multiple_items_enum_complete")
MultipleItemsEnumCompleteMsg(bool success, bool purchaseMade, int numOfferIDs, const String &offerID)
    : Message(Type(), success, purchaseMade, numOfferIDs, DataArrayPtr(), DataArrayPtr()) {}
bool Success() const { return mData->Int(2); }
int NumOfferIDs() const { return mData->Int(4); }
unsigned long long OfferID(int index) const;
bool Purchased(int index) const {
    DataArray *arr = mData->Node(6).Array(mData);
    return arr->Node(index).Int(arr);
}
void SetSuccess(bool b) { mData->Node(2) = b; }
void SetPurchaseMade(bool b) { mData->Node(3) = b; }
void SetNumOfferIDs(int count);
void SetOfferID(int index, const String &s);
void SetPurchased(int index, bool b);
END_MESSAGE

// RB3-360 retail StorePanel re-ported from the rb3-Wii oracle (member set +
// inline-mEnum control flow) with Xbox platform objects, verified against the
// retail Ghidra project (default_tu5.xex): ctor fn_827B6A58, Load fn_827B4F30,
// Poll fn_827B6020, EnumerateOffers fn_827B66E0, UpdateOffers fn_827B5E18,
// SyncProperty fn_827B5B68, LoadArt fn_827B6600. The prior DC3 port carried
// ~0x18 bytes of DC3-only members (mCartOffers/mCheckoutItem/mCheckoutProfile/
// mEnumJobID/mPostPurchaseJob/unk94/extra mNeedsCacheLoad) + a StoreEnumJob
// architecture that retail does not use; those are gone here.
class StorePanel : public UIPanel {
public:
    // Hmx::Object
    virtual ~StorePanel();
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Load();

    // UIPanel
    virtual void Enter();
    virtual void Exit();
    virtual bool Exiting() const;
    virtual void Poll();
    virtual bool IsLoaded() const;
    virtual void Unload();
    virtual bool Unloading() const;
    virtual bool IsSongInLibrary(int const &) const { return false; }
    virtual void ExitStore(StoreError) const;
    virtual Profile *StoreProfile() const;
    // RB3 overrides this as MakeNewOffer(const StorePackedOfferBase *, bool) —
    // a different signature. Make the base non-pure so BandStorePanel compiles.
    virtual StoreOffer *MakeNewOffer(DataArray *) { return 0; }
    virtual StoreOffer *FindOffer(Symbol) const;
    virtual bool EnumerateSubsetOfOfferIDs() const { return false; }
    virtual void GetOfferIDsToEnumerate(std::vector<u64> &, bool) const {}
    virtual void LoadArt(char const *, UIPanel *);

    StorePanel();
    void CheckOut(StorePurchaseable *);
    void SetSource(Symbol src, bool backup);
    void ExitError(StoreError);
    void HandleNetCacheMgrFailure();
    void FinishCheckout();
    void HandleNetCacheLoaderFailure(int);
    bool IsEnumerating() const;
    bool InCheckout() const;

    // Retail layout (offsets are of the complete-object; UIPanel non-virtual
    // part ends at 0x3c, Hmx::Object is a shared virtual base at the tail):
    std::vector<StoreOffer *> mOffers;          // 0x3c
    std::vector<StoreOffer *> mPendingOffers;   // 0x48
    bool mLoadOk;                                // 0x54
    bool mShowTestOffers;                        // 0x55
    std::list<NetCacheLoader *> mNetCacheLoaders; // 0x58
    NetCacheLoader *mPendingArtLoader;          // 0x60
    RndTex *mAlbumTex;                           // 0x64
    UIPanel *mPendingArtCallback;                // 0x68
    StorePreviewMgr *mStorePreviewMgr;           // 0x6c
    XboxEnumeration *mEnum;                       // 0x70
    bool mNeedsReEnum;                            // 0x74
    bool mUnk75;                                  // 0x75
    StorePurchaser *mPurchaser;                   // 0x78
    Symbol mPurchaseSource;                       // 0x7c
    Symbol mBackupPurchaseSource;                 // 0x80
    int mPostPurchaseState;                       // 0x84 (Load sets it to 2)

protected:
    // UIPanel
    virtual void PopulateOffers(DataArray *, bool);
    virtual void EnumerateOffers(bool);
    // RB3 returns int (not StoreError) — matches rb3-Wii's StorePanel.h
    virtual int UpdateOffers(std::list<EnumProduct> const &, bool);
    virtual void UpdateFromEnumProduct(StorePurchaseable *, EnumProduct const *);
    virtual void StoreUserProfileSwappedToUser(LocalUser *);

    DataNode OnMsg(SigninChangedMsg const &);
    DataNode OnMsg(ProfileSwappedMsg const &);
    DataNode OnMsg(SingleItemEnumCompleteMsg const &);
    DataNode OnMsg(MultipleItemsEnumCompleteMsg const &);
    void ValidateOffers(std::vector<StoreOffer *> &);
};
