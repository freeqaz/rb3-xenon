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

class StorePanel : public UIPanel {
    friend class StoreEnumJob;
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
    virtual StoreOffer *MakeNewOffer(DataArray *) = 0;
    virtual StoreOffer *FindOffer(Symbol) const;
    virtual bool EnumerateSubsetOfOfferIDs() const { return false; }
    virtual void GetOfferIDsToEnumerate(std::vector<u64> &, bool) const {}
    virtual void LoadArt(char const *, UIPanel *);

    StorePanel();
    void CheckOut(StorePurchaseable *);
    void SetSource(Symbol src, bool backup);
    void ExitError(StoreError);
    void HandleNetCacheMgrFailure();
    void HandleNetCacheLoaderFailure(int);
    void MultipleItemsCheckout(std::list<StoreOffer *> *);

    std::vector<StoreOffer *> mOffers;
    std::vector<StoreOffer *> mPendingOffers;
    bool mNeedsCacheLoad;
    bool mLoadOk; // 0x51
    bool mShowTestOffers;
    std::list<NetCacheLoader *> mNetCacheLoaders;
    int mArtLoader;
    RndTex *mAlbumTex;
    UIPanel *mPendingArtCallback; // 0x64
    int mEnumJobID;
    StorePreviewMgr *mStorePreviewMgr; // 0x6c
    bool mNeedsReEnum;
    StorePurchaser *mPurchaser; // 0x74
    StorePurchaseable *mCheckoutItem;
    int mCheckoutProfile;
    std::vector<std::pair<StorePurchaseable *, const Profile *>> mCartOffers;
    Symbol mPurchaseSource;
    Symbol mBackupPurchaseSource;
    int unk94;
    Job *mPostPurchaseJob;

protected:
    // UIPanel
    virtual void PopulateOffers(DataArray *, bool);
    virtual void EnumerateOffers(bool);
    virtual void FinishEnum(std::list<EnumProduct> const &, bool);
    virtual StoreError UpdateOffers(std::list<EnumProduct> const &, bool);
    virtual void UpdateFromEnumProduct(StorePurchaseable *, EnumProduct const *);
    virtual void StoreUserProfileSwappedToUser(LocalUser *);

    void StartReEnum();
    DataNode OnMsg(SigninChangedMsg const &);
    DataNode OnMsg(ProfileSwappedMsg const &);
    DataNode OnMsg(SingleItemEnumCompleteMsg const &);
    DataNode OnMsg(MultipleItemsEnumCompleteMsg const &);
    void ValidateOffers(std::vector<StoreOffer *> &);
};

class StoreEnumJob : public Job {
public:
    StoreEnumJob(StorePanel *, int, std::vector<UINT64> *);
    virtual ~StoreEnumJob();
    virtual void Start();
    virtual void Cancel(Hmx::Object *);
    virtual bool IsFinished();
    virtual void OnCompletion(Hmx::Object *);

protected:
    XboxEnumeration *mEnumeration; // 0x8
    StorePanel *mStorePanel; // 0xc
};
