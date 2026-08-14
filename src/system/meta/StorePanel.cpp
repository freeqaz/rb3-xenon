#include "meta/StorePanel.h"
#include "macros.h"
#include "meta/Profile.h"
#include "meta/StoreEnumeration.h"
#include "meta/StoreOffer.h"
#include "obj/Data.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "obj/PropSync.h"
#include "os/ContentMgr.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"
#include "rndobj/Bitmap.h"
#include "rndobj/Tex.h"
#include "ui/UI.h"
#include "ui/UIPanel.h"
#include "utl/BufStream.h"
#include "utl/MakeString.h"
#include "utl/NetCacheMgr.h"
#include "utl/Std.h"
#include "utl/Symbol.h"

// Retail RB3-360 StorePanel — re-ported from the rb3-Wii oracle (member set +
// inline-mEnum control flow), Xbox platform objects, verified against retail
// Ghidra (default_tu5.xex). See StorePanel.h for the ctor/Load/Poll/… address
// anchors. The prior DC3 port used a StoreEnumJob architecture + ~0x18 bytes of
// DC3-only members that retail does not have.

StorePanel::StorePanel()
    : mLoadOk(false), mShowTestOffers(false), mPendingArtLoader(0),
      mAlbumTex(Hmx::Object::New<RndTex>()), mPendingArtCallback(0),
      mStorePreviewMgr(0), mEnum(0), mNeedsReEnum(false), mUnk75(false),
      mPurchaser(0), mPurchaseSource(gNullStr), mBackupPurchaseSource(gNullStr),
      mPostPurchaseState(0) {}

StorePanel::~StorePanel() {
    DeleteAll(mOffers);
    DeleteAll(mPendingOffers);
    delete mAlbumTex;
}

unsigned long long MultipleItemsEnumCompleteMsg::OfferID(int index) const {
    DataArray *arr = mData->Node(5).Array(mData);
    int lo = arr->Node(index * 2).Int(arr);
    int hi = arr->Node(index * 2 + 1).Int(arr);
    return ((unsigned long long)hi << 32) | (unsigned int)lo;
}

BEGIN_PROPSYNCS(StorePanel)
    SYNC_PROP(load_ok, mLoadOk)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

void StorePanel::Load() {
    UIPanel::Load();
    mLoadOk = true;
    ThePlatformMgr.AddSink(this);
    // NOTE (laneFAMILY-1, measured): retail dispatches vtable slot 0x44 (=17,
    // StoreUser) here, not 0x70 (=28, StoreProfile), and then makes a VIRTUAL
    // call on the result through the virtual-base adjust — i.e.
    // LocalUser::GetPadNum(), not the non-virtual Profile::GetPadNum(). This is
    // the shape StorePanel.h:71's note already predicted from band.exe.
    LocalUser *user = StoreUser();
    int padNum = user->GetPadNum();
    if (padNum == 0 || ThePlatformMgr.IsSignedIntoLive(padNum) == 0) {
        if (mState == 0)
            mLoadOk = false;
        ExitStore(kStoreErrorLiveServer);
    }
    TheContentMgr.StartRefresh();
#ifdef HX_NATIVE
    if (TheNetCacheMgr)
#endif
    TheNetCacheMgr->Load((NetCacheMgr::CacheSize)1);
    MILO_ASSERT(!mStorePreviewMgr, 0x84);
    mStorePreviewMgr = new StorePreviewMgr();
    mStorePreviewMgr->AddSink(this);
    MILO_ASSERT(!mPurchaser, 0x88);
    mPostPurchaseState = 2;
}

void StorePanel::Enter() {
    UIPanel::Enter();
    // NOTE (laneFAMILY-1, measured): same defect as Load() — retail dispatches
    // slot 0x44 (=17, StoreUser) and threads the LocalUser* straight into the
    // LocalUser-typed PlatformMgr overloads. Decisive: retail's callee here is
    // the NAMED symbol ?IsUserSignedIntoLive@PlatformMgr@@QBA_NPBVLocalUser@@@Z,
    // whose signature takes const LocalUser* — there is no GetPadNum in retail's
    // body at all. Branch targets decode to a single 3-term disjunction guarding
    // `if (mState == 0) mLoadOk = false;` + an UNCONDITIONAL ExitStore(4), which
    // is the identical shape Load() already matches at 100%.
    LocalUser *user = StoreUser();
    if (user == 0 || ThePlatformMgr.IsUserSignedIntoLive(user) == 0
        || ThePlatformMgr.IsUserAGuest(user) != 0) {
        if (mState == 0)
            mLoadOk = false;
        ExitStore(kStoreErrorLiveServer);
    }
    mShowing = (bool)mLoadOk;
    XBackgroundDownloadSetMode(XBACKGROUND_DOWNLOAD_MODE_ALWAYS_ALLOW);
    mNeedsReEnum = false;
}

void StorePanel::Exit() {
    XBackgroundDownloadSetMode(XBACKGROUND_DOWNLOAD_MODE_AUTO);
    ThePlatformMgr.RemoveSink(this);
    UIPanel::Exit();
}

bool StorePanel::Exiting() const {
    if (mPurchaser && mPurchaser->IsPurchasing()) {
        return true;
    }
    if (mEnum && mEnum->IsEnumerating()) {
        return true;
    }
    return UIPanel::Exiting();
}

void StorePanel::Poll() {
    if (!mLoadOk)
        return;
    UIPanel::Poll();
    // NOTE: retail runs the TheNetCacheMgr->GetHasFailed() check at the END of
    // Poll (see the tail below) and has no IsReady() gate at all — matching the
    // rb3-Wii oracle. The leading early-outs are a DC3-era restructuring.
    mStorePreviewMgr->Poll();
    // Retail StorePreviewMgr (TU5) has no mHasFailure/mLastFailType members —
    // its layout is fixed at 0x60 (see StorePreviewMgr.h), so GetLastFailure()
    // is a DC3-only addition and cannot exist here.

    // Iterate NetCacheLoaders
    std::list<NetCacheLoader *>::iterator cur = mNetCacheLoaders.begin();
    while (cur != mNetCacheLoaders.end()) {
        NetCacheLoader *loader = *cur;
        if (loader->IsLoaded()) {
            if (loader == mPendingArtLoader) {
                MILO_ASSERT(mPendingArtCallback, 0x167);
                int size = loader->GetSize();
                char *buffer = loader->GetBuffer();
                MILO_ASSERT(buffer, 0x16d);
                RndBitmap bmap;
                BufStream stream(buffer, size, true);
                bmap.Load(stream);
                bmap.SetMip(0);
                TheNetCacheMgr->DeleteNetCacheLoader(loader);
                mAlbumTex->SetBitmap(bmap, 0, false);
                if (mPendingArtCallback->GetState() == UIPanel::kUp) {
                    static Message msg("art_loaded");
                    mPendingArtCallback->Handle(msg.mData, false);
                }
                mPendingArtLoader = 0;
                mPendingArtCallback = 0;
            } else {
                TheNetCacheMgr->DeleteNetCacheLoader(loader);
            }
            cur = mNetCacheLoaders.erase(cur);
        } else if (loader->HasFailed()) {
            NetCacheMgrFailType ft = loader->GetFailType();
            TheNetCacheMgr->DeleteNetCacheLoader(loader);
            cur = mNetCacheLoaders.erase(cur);
            HandleNetCacheLoaderFailure((int)ft);
        } else {
            ++cur;
        }
    }

    // Drive the inline enumeration
    if (mEnum && mEnum->IsEnumerating()) {
        mEnum->Poll();
        if (!mEnum->IsEnumerating()) {
            if (mEnum->IsSuccess()) {
                int err = UpdateOffers(mEnum->mContentList, false);
                if ((err == 0 || err == 1) && !mPendingOffers.empty()) {
                    err = UpdateOffers(mEnum->mContentList, true);
                }
                if (err != 0 && err != 1) {
                    ExitError((StoreError)err);
                    return;
                }
                static Message msg("enum_finished");
                HandleType(msg.mData);
            } else {
                MILO_NOTIFY("An enumeration failed!");
                static Message msg("enum_finished");
                HandleType(msg.mData);
                return;
            }
        }
    }

    // Re-enumerate if requested and idle
    if (!mPurchaser && mNeedsReEnum && !IsEnumerating()) {
        mNeedsReEnum = false;
        EnumerateOffers(!mPendingOffers.empty());
    }

    // Drive the purchaser
    if (mPurchaser) {
        mPurchaser->Poll();
        if (!mPurchaser->IsPurchasing()) {
            if (mPurchaser->PurchaseMade() && mPurchaser->IsSuccess()) {
                mNeedsReEnum = true;
                mUnk75 = true;
            } else {
                mNeedsReEnum = false;
                mUnk75 = false;
            }
            FinishCheckout();
        }
    }

    if (TheNetCacheMgr->GetHasFailed()) {
        HandleNetCacheMgrFailure();
    }
}

// Retail (fn_827B5C78) keeps the checkout-finished broadcast out of line; it is
// called from Poll() once the purchaser stops purchasing.
void StorePanel::FinishCheckout() {
    static Message msg("checkout_finished", DataNode(0));
    msg[0] = mUnk75;
    HandleType(msg.mData);
    TheUI->Handle(msg.mData, false);
    RELEASE(mPurchaser);
}

bool StorePanel::Unloading() const {
    if (mState != kUp && !TheNetCacheMgr->IsUnloaded())
        return true;
    return UIPanel::Unloading();
}

StorePanel *StorePanel::Instance() {
    return ObjectDir::Main()->Find<StorePanel>("store_panel", true);
}

void StorePanel::ExitStore(StoreError) const {}
LocalUser *StorePanel::StoreUser() const { return nullptr; }
Profile *StorePanel::StoreProfile() const { return nullptr; }

// NOTE (laneFAMILY-1, measured against retail bytes): retail carries a THIRD
// clause our port dropped. Retail dereferences `lwz r11,0x84(r31)` / `cmpwi 2`
// and `lbz r11,0x54(r31)` — and /d1reportSingleClassLayout puts mPostPurchaseState
// at 0x84 and mLoadOk at 0x54, so the offsets identify the members outright.
// The rb3-Wii oracle spells the same expression (`mSessionStatus ==
// kSessionCreated || !mLoadOK`), and Enter() already writes the literal 2.
// The `!mLoadOk` polarity looks backwards but is what both retail and the
// oracle say — do not "correct" it.
bool StorePanel::IsLoaded() const {
    return UIPanel::IsLoaded() && TheContentMgr.RefreshDone()
        && (mPostPurchaseState == 2 || !mLoadOk);
}

void StorePanel::Unload() {
    RELEASE(mPurchaser);
    RELEASE(mEnum);
    mStorePreviewMgr->RemoveSink(this);
    RELEASE(mStorePreviewMgr);
    FOREACH (it, mNetCacheLoaders) {
        TheNetCacheMgr->DeleteNetCacheLoader(*it);
    }
    mNetCacheLoaders.clear();
    DeleteAll(mOffers);
    DeleteAll(mPendingOffers);
    TheNetCacheMgr->Unload();
    UIPanel::Unload();
}

void StorePanel::LoadArt(const char *cc, UIPanel *panel) {
    String str(cc);
    std::list<NetCacheLoader *>::iterator it = std::find(mNetCacheLoaders.begin(), mNetCacheLoaders.end(), str);
    if (it == mNetCacheLoaders.end()) {
        NetCacheLoader *loader = TheNetCacheMgr->AddNetCacheLoader(cc, (NetLoaderPos)0);
        mPendingArtLoader = loader;
        if (loader) {
            mNetCacheLoaders.insert(it, mPendingArtLoader);
        }
    } else {
        mPendingArtLoader = *it;
    }
    mPendingArtCallback = panel;
}

void StorePanel::CheckOut(StorePurchaseable *p) {
    StorePurchaser *purchaser;

    MILO_ASSERT(p->IsAvailable(), 0x2c0);
    MILO_ASSERT(!mPurchaser, 0x2c1);
    Profile *profile = StoreProfile();
    MILO_ASSERT(profile, 0x2c4);

    // Allocate and construct XboxPurchaser
    void *mem = operator new(sizeof(XboxPurchaser));
    if (mem) {
        purchaser = new (mem) XboxPurchaser(
            profile->GetPadNum(),
            p->songID,
            0,
            0,
            mPurchaseSource,
            0
        );
    } else {
        purchaser = 0;
    }
    mPurchaser = purchaser;
    mPurchaser->Initiate();
}

void StorePanel::ExitError(StoreError e) {
    MILO_ASSERT(e != kStoreErrorSuccess, 0x405);
    // Retail (inlined into Poll): the mLoadOk clear is gated on the panel being
    // unloaded, and ExitStore runs unconditionally.
    if (GetState() == kUnloaded) {
        mLoadOk = false;
    }
    ExitStore(e);
}

void StorePanel::HandleNetCacheMgrFailure() {
    StoreError err;
    NetCacheMgrFailType failTy;

    err = kStoreErrorSuccess;
    failTy = TheNetCacheMgr->GetFailType();
    switch (failTy) {
    case kNCMFT_StoreServer:
    case kNCMFT_NoSpace:
        MILO_WARN("Failure %d in NetCacheMgr.\n", failTy);
        break;
    case kNCMFT_StorageDeviceMissing:
        err = kStoreErrorNoMetadata;
        break;
    default:
        MILO_WARN("Unknown failure %d in NetCacheMgr.\n", failTy);
        break;
    }
    if (err != kStoreErrorNoMetadata && !ThePlatformMgr.IsEthernetCableConnected()) {
        err = kStoreErrorNoMetadata;
    }
    if (err != kStoreErrorSuccess)
        ExitError(err);
}

void StorePanel::HandleNetCacheLoaderFailure(int failType) {
    MILO_ASSERT((0) <= (failType) && (failType) < (kNCMFT_Max), 0xe5);

    if (failType == kNCMFT_Unknown) {
        failType = TheNetCacheMgr->GetFailType();
    }

    StoreError err;
    switch (failType) {
    case kNCMFT_StoreServer: {
        Profile *profile = StoreProfile();
        bool signedIn = ThePlatformMgr.IsSignedIntoLive(profile->GetPadNum());
        err = (StoreError)((!signedIn ^ 1) + kStoreErrorCacheNoSpace);
        break;
    }
    case kNCMFT_NoSpace:
        return;
    case kNCMFT_StorageDeviceMissing:
        goto no_metadata;
    default:
        MILO_NOTIFY("Unknown failure %d in a net cache loader!", failType);
        err = kStoreErrorCacheRemoved;
        break;
    }

    if (!ThePlatformMgr.IsEthernetCableConnected()) {
no_metadata:
        err = kStoreErrorNoMetadata;
    }

    ExitError(err);
}

void StorePanel::PopulateOffers(DataArray *arr, bool b) {
    if (mLoadOk) {
        DeleteAll(mPendingOffers);
        if (!b) {
            DeleteAll(mOffers);
        }

        std::vector<StoreOffer *> *offerVec = &mPendingOffers;
        if (!b) {
            offerVec = &mOffers;
        }

        if (arr != NULL) {
            arr->AddRef();
            int i = 1;

            if (arr->Size() > 1) {
                do {
                    DataArray *child_arr = arr->Array(i);
                    StoreOffer *offer = MakeNewOffer(child_arr);

                    if ((mShowTestOffers == 0) && offer->IsTest()) {
                        delete offer;
                    } else if (!offer->ValidTitle()) {
                        delete offer;
                    } else {
                        offerVec->push_back(offer);
                    }

                    i++;
                } while (i < arr->Size());
            }

            ValidateOffers(*offerVec);
            arr->Release();
        }
    }
}

void StorePanel::EnumerateOffers(bool b) {
    RELEASE(mEnum);
    Profile *profile = StoreProfile();
    MILO_ASSERT(profile, 0x356);
    if (EnumerateSubsetOfOfferIDs()) {
        std::vector<UINT64> offerIDs;
        GetOfferIDsToEnumerate(offerIDs, b);
        if (offerIDs.empty()) {
            if (mLoadOk) {
                mLoadOk = false;
                ExitStore(kStoreErrorSignedOut);
            }
            // NOTE (laneAX-W7, measured): retail's EnumerateOffers does carry a
            // second `static Message("enum_finished")` + HandleType + TheUI->Handle
            // pair (target 0x827B66E0, guard at +0xfc, mirrored again at +0x25c),
            // but its body is otherwise materially different from ours (two
            // duplicated branches calling fn_827BD2F0/fn_827B84A0/fn_827BCA50).
            // Adding the static here alone cost 4 strict matches in this unit
            // (fn_82605040, fn_827B6994, fn_827B69DC, fn_827B6A04 -- EH funclets
            // un-pairing on the changed frame) for 0 gain, so it is deliberately
            // NOT applied until the body is ported. Do not re-add in isolation.
            return;
        }
        mEnum = new XboxEnumeration(profile->GetPadNum(), &offerIDs);
    } else {
        mEnum = new XboxEnumeration(profile->GetPadNum(), 0);
    }
    mEnum->Start();
    static Message msg("enum_start");
    HandleType(msg);
    TheUI->Handle(msg, false);
}

// Retail fn_827B4AE0: `ld r11,0x10(r3); ld r10,0x30(r4); cmpld` -- a 64-bit
// compare of EnumProduct's offer id against StorePurchaseable::songID (0x30).
// NOTE 0x10, not the `// 0x08` StoreEnumeration.h annotates mOfferID with: a
// String is 0xc bytes, so the u64 aligns up to 0x10. The declaration order
// already produces that; only the comment is stale.
bool operator==(const EnumProduct &product, const StorePurchaseable &purchaseable) {
    return product.mOfferID == purchaseable.songID;
}

int StorePanel::UpdateOffers(std::list<EnumProduct> const &enumList, bool arg) {
    std::vector<StoreOffer *> *offers = arg ? &mPendingOffers : &mOffers;

    int result;
    if (mShowTestOffers) {
        result = kStoreErrorSuccess;
    } else {
        result = offers->empty() ? kStoreErrorSignedOut : kStoreErrorNoContent;
    }

    std::vector<StoreOffer *>::iterator it;
    for (it = offers->begin(); it != offers->end(); ++it) {
        StoreOffer *offer = *it;

        // Primary purchaseable (the offer itself).
        std::list<EnumProduct>::const_iterator e;
        if (offer->Exists()) {
            for (e = enumList.begin(); e != enumList.end(); ++e) {
                if (e->mOfferID == offer->songID)
                    break;
            }
        } else {
            e = enumList.end();
        }
        if (e != enumList.end()) {
            result = kStoreErrorSuccess;
            UpdateFromEnumProduct(offer, &*e);
        } else if (offer->IsTest()) {
            offer->isAvailable = false;
            offer->isPurchased = false;
            offer->cost = 0x270f;
        }

        // Album purchaseable (offer + 0x40).
        StorePurchaseable *album = (StorePurchaseable *)((char *)offer + 0x40);
        if (album->Exists()) {
            for (e = enumList.begin(); e != enumList.end(); ++e) {
                if (e->mOfferID == album->songID)
                    break;
            }
            if (e != enumList.end()) {
                result = kStoreErrorSuccess;
                UpdateFromEnumProduct(album, &*e);
            }
        }

        // Pack purchaseable (offer + 0x80).
        StorePurchaseable *pack = (StorePurchaseable *)((char *)offer + 0x80);
        if (pack->Exists()) {
            for (e = enumList.begin(); e != enumList.end(); ++e) {
                if (e->mOfferID == pack->songID)
                    break;
            }
            if (e != enumList.end()) {
                result = kStoreErrorSuccess;
                UpdateFromEnumProduct(pack, &*e);
            }
        }
    }

    return result;
}

void StorePanel::UpdateFromEnumProduct(StorePurchaseable *sp, EnumProduct const *ep) {
    MILO_ASSERT(sp, 0x3f0);
    MILO_ASSERT(ep, 0x3f1);
    sp->isPurchased = (ep->mPurchased != 0);
    sp->cost = ep->mPrice;
    sp->isAvailable = true;
}

DataNode StorePanel::OnMsg(SigninChangedMsg const &msg) {
    Profile *profile = StoreProfile();
    if (profile != 0) {
        // Check if this profile's pad number is in the signin change mask
        int changedMask;
        int padNum;
        changedMask = bool(msg.mData->Node(3).Int(msg.mData));
        padNum = profile->GetPadNum();
        // If this pad's bit is not set in the change mask, ignore the message
        if (((1 << padNum) & changedMask) == 0) {
            return 0;
        }
    }
    // Signin changed for this profile - exit the store
    if (mLoadOk) {
        mLoadOk = false;
        ExitStore(kStoreErrorLiveServer);
    }
    return 0;
}

DataNode StorePanel::OnMsg(ProfileSwappedMsg const &) { return 0; }

DataNode StorePanel::OnMsg(SingleItemEnumCompleteMsg const &msg) {
    bool hasOffer = false;
    if (msg.Success()) {
        if (msg.HasOfferID()) {
            hasOffer = true;
        }
    }

    if (hasOffer) {
        u64 offerId = msg.OfferID();
        for (std::vector<StoreOffer *>::iterator it = mOffers.begin(); it != mOffers.end();
             ++it) {
            StoreOffer *offer = *it;
            if (offer->songID == offerId) {
                offer->isPurchased = true;
                static Message enumMsg("enum_finished");
                HandleType(enumMsg);
                TheUI->Handle(enumMsg, false);
                break;
            }
        }
    }

    static Message doneMsg("reenum_finished", DataNode(0));
    doneMsg->Node(2) = DataNode((int)hasOffer);
    TheUI->Handle(doneMsg, false);
    return DataNode(0);
}

void StorePanel::ValidateOffers(std::vector<StoreOffer *> &offers) {
    std::vector<StoreOffer *> song_offers;
    std::vector<Symbol> song_names;

    static Symbol pack_sym("pack");
    static Symbol album_sym("album");

    std::vector<StoreOffer *>::iterator it;
    auto _tmp4 = offers.end();
    for (it = offers.begin(); it != _tmp4; ++it) {
        StoreOffer *offer = *it;
        Symbol offer_type = offer->OfferType();

        if (offer_type != ("dummy_upsell_offer")) {
            Symbol short_name = offer->StoreOfferData()->Sym(0);

            std::vector<Symbol>::iterator sit =
                std::find(song_names.begin(), song_names.end(), short_name);

            if (sit != song_names.end()) {
                MILO_NOTIFY("Duplicate offer short name: %s", short_name);
            } else {
                song_names.push_back(short_name);
            }

            if (offer_type == ("song")) {
                song_offers.push_back(offer);
            }
        }
    }

    Symbol offer_types[2];
    offer_types[0] = album_sym;
    offer_types[1] = pack_sym;

    for (int i = 0; i < 2; i++) {
        Symbol cur_type = offer_types[i];
        std::vector<StoreOffer *>::iterator nit;
        for (nit = song_offers.begin(); nit != song_offers.end(); ++nit) {
            StoreOffer *song_offer = *nit;
            int count = 0;
            std::vector<StoreOffer *>::iterator oit;
            for (oit = offers.begin(); oit != offers.end(); ++oit) {
                StoreOffer *offer_ptr = *oit;
                if (offer_ptr->OfferType() == cur_type && offer_ptr->HasSong(offer_ptr)) {
                    count++;
                }
            }
            if (count > 1) {
                Symbol song_name = song_offer->StoreOfferData()->Sym(0);
                auto _tmp0 = MakeString("Song %s is in more than one %s", song_name, cur_type);
                TheDebug.Notify(_tmp0);
            }
        }
    }
}

DataNode StorePanel::OnMsg(MultipleItemsEnumCompleteMsg const &msg) {
    bool success = msg.Success();
    if (success) {
        int numOffers = msg.NumOfferIDs();
        for (int i = 0; i < numOffers; i++) {
            u64 offerId = msg.OfferID(i);
            for (std::vector<StoreOffer *>::iterator it = mOffers.begin(); it != mOffers.end();
                 ++it) {
                StoreOffer *offer = *it;
                if (offer->songID == offerId) {
                    if (!offer->isPurchased) {
                        bool purchased = msg.Purchased(i);
                        if (purchased) {
                            offer->isPurchased = true;
                        }
                    }
                    break;
                }
            }
        }
        static Message enumMsg("enum_finished");
        HandleType(enumMsg);
        TheUI->Handle(enumMsg, false);
    }

    static Message doneMsg("reenum_finished", DataNode(0));
    doneMsg->Node(2) = DataNode((int)success);
    TheUI->Handle(doneMsg, false);
    return DataNode(0);
}

void StorePanel::SetSource(Symbol src, bool backup) {
    mPurchaseSource = src;
    if (backup)
        mBackupPurchaseSource = src;
}

StoreOffer *StorePanel::FindOffer(Symbol) const { return nullptr; }
void StorePanel::StoreUserProfileSwappedToUser(LocalUser *) {}

BEGIN_HANDLERS(StorePanel)
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
    HANDLE_EXPR(toggle_test_offers, mShowTestOffers = !mShowTestOffers)
    HANDLE_EXPR(test_offers, mShowTestOffers)
#endif
    HANDLE_ACTION(load_art, LoadArt(_msg->Str(2), _msg->Obj<UIPanel>(3)))
    HANDLE_EXPR(album_tex, mAlbumTex)
    HANDLE_ACTION(cancel_art, (mPendingArtLoader = 0, mPendingArtCallback = 0))
    HANDLE_ACTION(check_out, CheckOut(_msg->Obj<StorePurchaseable>(2)))
    HANDLE_ACTION(re_download, CheckOut(_msg->Obj<StorePurchaseable>(2)))
    HANDLE_ACTION(set_source, SetSource(_msg->Sym(2), _msg->Int(3)))
    HANDLE_ACTION(set_source_to_backup, mPurchaseSource = mBackupPurchaseSource)
    HANDLE_MESSAGE(SigninChangedMsg)
    HANDLE_MESSAGE(ProfileSwappedMsg)
    HANDLE_MESSAGE(SingleItemEnumCompleteMsg)
    HANDLE_MESSAGE(MultipleItemsEnumCompleteMsg)
    HANDLE_SUPERCLASS(UIPanel)
END_HANDLERS
// sw2 scatter-include (default/StorePanel <- hamobj/DancerSequence.cpp)
#define gRev gRev_DancerSequence
#define gAltRev gAltRev_DancerSequence
#include "hamobj/DancerSequence.cpp"
#undef gRev
#undef gAltRev
