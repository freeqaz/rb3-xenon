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

// Retail fn_827B4AE0, inside StorePanel.s's pinned range -- so it belongs to
// StorePanel.cpp, not to its caller. It exists because retail's
// BandStorePanel::UpdateOffers reaches the enum list through
// std::find(list.begin(), list.end(), <a StorePurchaseable>), which needs this
// heterogeneous comparison. Declared here because this header is the only one
// that already sees BOTH EnumProduct (StoreEnumeration.h) and StorePurchaseable
// (StoreOffer.h).
bool operator==(const EnumProduct &, const StorePurchaseable &);

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
    // Retail 360 has StoreUser() — NOT StoreProfile() — in this vtable position
    // (slot 17 / dispatch offset 0x44). Proven from band.exe: BandStorePanel's
    // primary vtable slot 0x44 = 0x82605720, whose body is
    // TheInputMgr->GetUser()->GetLocalBandUser() followed by the compiler's
    // LocalBandUser* -> LocalUser* virtual-base adjust (vbptr@4, vbtable idx 3 —
    // and LocalBandUser is the only class with >=3 virtual bases here:
    // `class LocalBandUser : public virtual BandUser, public virtual LocalUser`).
    // BandStorePanel::Request then calls slot 0 on the result, which is
    // LocalUser::GetPadNum() (LocalUser's FIRST new virtual, since its other
    // members all override User). The rb3-Wii oracle agrees exactly:
    // StorePanel.h:33 `virtual int StoreUser() const = 0; // fix ret type`,
    // in this same position, and declares no StoreProfile at all.
    virtual LocalUser *StoreUser() const;
    // PURE in retail (lane STOREPANEL, 2026-08-22).  StorePanel's own primary
    // vtable (0x82115fac, located via RTTI, not via the map) has slot 18 =
    // 0x828299b8 -- the binary's `_purecall`, referenced 849 times.  The base
    // was only made non-pure as a workaround for BandStorePanel's override
    // carrying the WRONG signature (const StorePackedOfferBase *, bool); with
    // that corrected to (DataArray *) the override binds here and the base can
    // be pure again, so our slot 18 emits `_purecall` exactly as retail's does.
    // BandStorePanel is the only subclass in the tree.
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
    void FinishCheckout();
    void HandleNetCacheLoaderFailure(int);
    bool IsEnumerating() const;
    bool InCheckout() const;

    // Singleton lookup by object name. Retail's StorePanel.obj emits the
    // ObjectDir::Find<StorePanel> COMDAT (0x827B5300, RTTI-confirmed) and this
    // is its only possible source; rb3-Wii StorePanel.h:60 / .cpp:239 agrees.
    static StorePanel *Instance();

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
    // Retail has no StoreProfile() virtual at all (see the StoreUser() note
    // above). Six StorePanel.cpp bodies still call it, so it is kept — but it
    // is NON-VIRTUAL, so it occupies no slot at all.
    //
    // ⚠ This finishes laneSTORE-2's work rather than revising it.  That lane
    // proved the same thing and parked it at the TAIL of the vtable so it
    // "cannot perturb the dispatch offsets of slots 0..27".  That reasoning is
    // correct as far as it goes, but a tail slot is still a slot: the vtable
    // sweep measures retail's BandStorePanel (0x820bf28c) at 28 slots against
    // our 30, and this was one of the two.  Dropping `virtual` keeps slots
    // 0..27 exactly as laneSTORE-2 left them AND removes the surplus.
    // Nothing overrides it (BandStorePanel is the only class deriving from
    // StorePanel and declares no StoreProfile), so every existing call site
    // binds to this same body and behaviour is unchanged.
    Profile *StoreProfile() const;

    DataNode OnMsg(SigninChangedMsg const &);
    DataNode OnMsg(ProfileSwappedMsg const &);
    DataNode OnMsg(SingleItemEnumCompleteMsg const &);
    DataNode OnMsg(MultipleItemsEnumCompleteMsg const &);
    void ValidateOffers(std::vector<StoreOffer *> &);
};
