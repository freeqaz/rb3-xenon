#include "meta_band/BandStorePanel.h"
#include "meta_band/BandSongMetadata.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/BandStoreOffer.h"
#include "meta_band/InputMgr.h"
#include "meta_band/SessionMgr.h"
#include "meta_band/StoreOfferProvider.h"
#include "meta_band/UIEventMgr.h"
#include "meta_band/AppLabel.h"
#include "game/BandUser.h"
#include "meta/StorePackedMetadata.h"
#include "net/Net.h"
#include "obj/Dir.h"
#include "obj/Msg.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"
#include "ui/UI.h"
#include "ui/UIList.h"
#include "ui/UIListLabel.h"
#include "ui/UIListProvider.h"
#include "utl/MakeString.h"
#include "utl/Messages.h"
#include "utl/Messages4.h"
#include "utl/NetCacheMgr.h"
#include "utl/NetLoader.h"
#include "utl/Std.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"

// Retail keeps the store request prefix in a statically-initialized file-scope
// `const char *` (target .data 0x82C73FD8 -> .rdata "dlc_store"), not as an
// inline literal: LoadArt/Request/GetRequestPrefix all `lwz` the pointer out of
// .data rather than `addi`-ing the literal's address. GetRequestPrefix
// (fn_82605868) is literally `{ lwz r3, sRequestPrefix; blr }`.
static const char *sRequestPrefix = "dlc_store";

// Retail 360 base (meta/StorePanel.h) has two StoreOffer* vectors
// (mOffers, mPendingOffers) where the rb3-Wii dev oracle used three
// (unk38/unk40/unk48). Map the oracle names onto the retail base:
//   unk38 -> mOffers   (primary offers, provider "offers" arg)
//   unk48 -> mPendingOffers (provider "packs" arg)
//   unk40 -> mPendingOffers (extra offers; only touched by deferred handlers)
#define unk38 mOffers
#define unk40 mPendingOffers
#define unk48 mPendingOffers

// StoreMetadataManager::mVersion is void* in the in-tree (trimmed) header.
// The packed StoreVersionHeader has mBuildNumber (u16) at byte offset 1.
// Read it via a cast rather than widening the shared header (ripple risk).
static inline unsigned short StoreBuildNum() {
    return *(unsigned short *)((char *)TheStoreMetadata.mVersion + 1);
}

// Retail (fn_82605128) never stores to mUserCanDoInput (0xE1) in the ctor --
// only mStartBrowserAtBottom (0xE0) and mShortcutProvider (0xE4) are
// zero-initialized here (verified: target is exactly one 4-byte instruction
// shorter than a version with the `stb ...,0xe1` present). mUserCanDoInput
// starts uninitialized on retail; SYNC_PROP(waiting, mUserCanDoInput) is the
// first place it gets a real value. Match retail exactly rather than "fixing"
// the apparent bug.
BandStorePanel::BandStorePanel()
    : mMetadataLoader(0), mLastRequestExtra(0), mSort(gNullStr),
      mStartBrowserAtBottom(0), mShortcutProvider(0) {
    mOfferProvider = new StoreOfferProvider(&unk38);
}

BandStorePanel::~BandStorePanel() { delete mOfferProvider; }

BandStorePanel *BandStorePanel::Instance() {
    return ObjectDir::Main()->Find<BandStorePanel>("store_panel", true);
}

bool BandStorePanel::IsSongInLibrary(const int &id) const {
    return TheSongMgr.HasSong(id);
}

const char *BandStorePanel::GetIndexFile() const {
    return MakeString("%d", StoreBuildNum());
}

const char *BandStorePanel::GetRequestPrefix() const { return sRequestPrefix; }

LocalUser *BandStorePanel::StoreUser() const {
    LocalBandUser *l = TheInputMgr->GetUser()
        ? TheInputMgr->GetUser()->GetLocalBandUser()
        : 0;
    return l;
}

StoreOffer *BandStorePanel::MakeNewOffer(DataArray *da) {
    return new BandStoreOffer(da, &TheSongMgr);
}

StoreOffer *BandStorePanel::FindOffer(Symbol s) const {
    for (std::vector<StoreOffer *>::const_iterator it = unk38.begin();
         it != unk38.end(); ++it) {
        StoreOffer *o = *it;
        if (o->ShortName() == s)
            return o;
    }
    for (std::vector<StoreOffer *>::const_iterator it = unk48.begin();
         it != unk48.end(); ++it) {
        StoreOffer *o = *it;
        if (o->ShortName() == s)
            return o;
    }
    return 0;
}

StoreOffer *BandStorePanel::GetLoneOffer(bool extras) const {
    if (!extras) {
        MILO_ASSERT(unk38.size() == 1, 0xAA);
        return unk38[0];
    }
    MILO_ASSERT(!unk40.empty(), 0xAF);
    return unk40[0];
}

bool BandStorePanel::IsLoaded() const {
    return StorePanel::IsLoaded() && (TheNetCacheMgr->IsReady() || !mLoadOk);
}

void BandStorePanel::Unload() {
    mLastRequest.erase();
    delete mMetadataLoader;
    mMetadataLoader = 0;
    delete mShortcutProvider;
    mShortcutProvider = 0;
    StorePanel::Unload();
}

void BandStorePanel::Enter() {
    StorePanel::Enter();
    LocalBandUser *u = dynamic_cast<LocalBandUser *>(StoreUser());
    if (u && !u->IsParticipating()) {
        ExitError(kStoreErrorStoreServer);
    }
    TheSessionMgr->AddSink(this, LocalUserLeftMsg::Type());
}

void BandStorePanel::Exit() {
    TheSessionMgr->RemoveSink(this, LocalUserLeftMsg::Type());
    StorePanel::Exit();
}

DataNode BandStorePanel::OnMsg(const LocalUserLeftMsg &) {
    return DataNode(1);
}

DataNode BandStorePanel::OnMsg(const MetadataLoadedMsg &msg) {
    DataArray *data = msg->Array(2);
    String path(msg->Str(4));
    DataArray *found = data->FindArray(Symbol("metadata"), false);
    if (found) {
        PopulateOffers(found, msg->Int(6) != 0);
        EnumerateOffers(msg->Int(6) != 0);
    }
    return DataNode(1);
}

Symbol BandStorePanel::SortName() {
    if (mSort == gNullStr) {
        return Symbol("by_song_first_letter");
    }
    return mSort;
}

inline const char *BandStoreShortcutProvider::RawTextAtData(int i) const {
    DataNode &n = mData->Node(i + mOffset);
    MILO_ASSERT(n.Type() == kDataString, 0x3A);
    return n.Str(0);
}

const char *BandStorePanel::ShortcutTextAtData(int i) {
    MILO_ASSERT(mShortcutProvider, 0x24A);
    return mShortcutProvider->RawTextAtData(i);
}

void BandStorePanel::SetShortcutData(DataArray *arr) {
    if (mShortcutProvider) {
        mShortcutProvider->SetData(arr);
    } else {
        mShortcutProvider = new BandStoreShortcutProvider(arr);
    }
}

void BandStorePanel::ApplyShortcutProvider(UIList *list) {
    MILO_ASSERT(mShortcutProvider, 0x258);
    list->SetProvider(mShortcutProvider);
}

void BandStoreShortcutProvider::Text(int i, int j, UIListLabel *listlabel, UILabel *label) const {
    int _tmp0 = mData->Node(j + mOffset).Evaluate().Type();
    if (_tmp0 == kDataString) {
        AppLabel *al = dynamic_cast<AppLabel *>(label);
        MILO_ASSERT(al, 0x30);
        al->SetRawStoreShortcut(j);
    } else {
        DataProvider::Text(i, j, listlabel, label);
    }
}

void BandStorePanel::LoadArt(const char *path, UIPanel *callback) {
    ObjectDir::Main()->Find<BandStorePanel>("store_panel", true);
    String full(sRequestPrefix);
    full += path;
    StorePanel::LoadArt(full.c_str(), callback);
}

// Retail 360 Request (fn_826071B8) is the path-based (album art / config)
// download flow only. The rb3-Wii dev oracle's atoi()/id-branch (LoadPage /
// DefaultSort / chunk-path derivation) is not part of the retail function.
void BandStorePanel::Request(const String &path, bool extra) {
    if (mLoadOk) {
        if (TheNetCacheMgr->GetHasFailed()) {
            HandleNetCacheMgrFailure();
        } else {
            MILO_ASSERT(mLastRequest.empty(), 0x1B4);
            MILO_ASSERT(TheNetCacheMgr->IsReady(), 0x1B5);
            MILO_ASSERT(!mMetadataLoader, 0x1B6);
            mLastRequest = path;
            mLastRequestExtra = extra;
            mStartBrowserAtBottom = false;
            String url(sRequestPrefix);
            Symbol region = PlatformRegionToSymbol(ThePlatformMgr.GetRegion());
            url += MakeString("/%s%s", region, path);
            Server *server = TheNet.GetServer();
            if (server && server->IsConnected()) {
                url += MakeString("?pid=%u", server->GetPlayerID(StoreUser()->GetPadNum()));
            }
            mMetadataLoader = new DataNetLoader(url);
            static Message updateMsg("update_loading_status");
            TheUI->Handle(updateMsg.mData, false);
        }
    }
}

void BandStorePanel::ExitStore(StoreError err) const {
    static Symbol sEvent("store_load_failed");
    if (!TheUIEventMgr->HasActiveTransitionEvent()) {
        static Message msg("init", DataNode(-1));
        msg[0] = DataNode((int)err);
        TheUIEventMgr->TriggerEvent(sEvent, msg.mData);
    }
}

// Residual status after lane BODYPORT-3 (2026-08-13): Handle is 1928 B at
// 93.12%, and ~all of its 41 charged mismatches are ONE repeated shape across
// the three Request() arms below -- how the String temporary reaches argument 2:
//     retail:  bl String::String ; li r5,1 ; addi r4,r31,0x58 ; bl Request
//              (rematerialises &temp from its known stack slot)
//     ours:    bl String::String ; mr r4,r3 ; li r5,1        ; bl Request
//              (carries the ctor's return, and in the `request` arm spills it to
//               r30 across the intervening _msg->Int(3) call)
// Semantically identical -- the ctor returns `this` -- so this is an argument
// materialisation choice, not a behavioural divergence.  NOT attempted here: the
// obvious lever (construct the String as a named local, which would give MSVC a
// fixed addressable slot) does not fit inside HANDLE_ACTION's single-expression
// form, and inventing a different macro for the arm would be metric-fitting
// rather than reconstructing retail.  Left open deliberately, with the shape
// recorded so the next lane starts from the diagnosis and not the symptom.
BEGIN_HANDLERS(BandStorePanel)
    HANDLE_EXPR(get_request_prefix, GetRequestPrefix())
    HANDLE_ACTION(request, Request(_msg->Str(2), _msg->Int(3)))
    HANDLE_ACTION(
        request_prev_chunk,
        (Request(mPrevChunkPath.c_str(), true), mStartBrowserAtBottom = true)
    )
    HANDLE_ACTION(request_next_chunk, Request(mNextChunkPath.c_str(), true))
    HANDLE_EXPR(should_start_browser_at_bottom, mStartBrowserAtBottom)
    // Retail's request_in_progress arm is a bare bool materialization
    // (subic/subfe) with NO TheStoreMetadata.mFlags test: our version emitted
    // six extra instructions here (lis/addi TheStoreMetadata, lwz +0x28,
    // rlwinm. r11,r11,0,28,28 == the "& 8", plus two branches) against a
    // retail arm whose surrounding instructions match exactly.
    HANDLE_EXPR(request_in_progress, mMetadataLoader != 0)
    HANDLE_EXPR(num_offers, (int)unk38.size())
    HANDLE_EXPR(lone_offer, GetLoneOffer(false))
    HANDLE_EXPR(num_extra_offers, (int)unk40.size())
    HANDLE_EXPR(first_extra_offer, GetLoneOffer(true))
    // Retail's handler set for this unit is readable directly from its .rdata
    // literal pool at 0x820bfb90..0x820bfc90, which bottom-up reads:
    //   get_request_prefix, request, request_prev_chunk, request_next_chunk,
    //   should_start_browser_at_bottom, request_in_progress, num_offers,
    //   lone_offer, num_extra_offers, first_extra_offer, offer_provider,
    //   sort_name, user_can_do_input, set_shortcut_data, apply_shortcut_provider
    // i.e. this list exactly, with two corrections: there is NO
    // offer_contents_provider (that token occurs nowhere in band.exe), and
    // sort_name sits here rather than down by the message handlers. 14 of the
    // 15 tokens already agreed with our order, so the pool ordering is
    // meaningful rather than arbitrary. (Moving sort_name is metric-neutral
    // today -- Handle is unmapped and this unit's .rdata is not pinned.)
    HANDLE_EXPR(offer_provider, mOfferProvider)
    // NOT SortName().  Retail reads the Symbol member straight out of the object
    // here -- `lwz r11, -0x1c(r26)` == this+0xd0 == mSort, then builds the
    // DataNode with `li r10, 0x5` (kDataSymbol) -- with no call at all.  Our
    // SortName() returns Symbol by value, so it gets an sret call that retail
    // does not make.  SortName() itself stays (it is used elsewhere); it is only
    // the wrong expression FOR THIS HANDLER.
    HANDLE_EXPR(sort_name, mSort)
    // CORRECTED ON RETAIL BYTES (lane W16-CA).  The previous note here read:
    //   "rb3-Wii's user_can_do_input tail checked TheWiiCommerceMgr async op
    //    state; there is no CommerceMgr on 360 ... so the Wii-only commerce
    //    clause is dropped."
    // The clause is NOT dropped -- it is PORTED, and it was our extra leading
    // `mUserCanDoInput == 0` that retail does not have.  Retail's guard is
    // four terms in this order (fn at Handle idx 322-342):
    //   lwz/lwz 0x30(vptr); bctrl; clrlwi.; beq   -> IsLoaded()      (slot 12)
    //   bl fn_827B4CC0; clrlwi.; bne              -> !IsEnumerating()
    //   bl fn_827B4D10; clrlwi.; bne              -> !InCheckout()
    //   lwz -0x40(r26); lbz 0(r11); cmplwi 0      -> mLastRequest.empty()
    // The two callees are identified from StorePanel.h's OFFSETS, not from
    // their vtable slot names: fn_827B4CC0 reads this->0x70 (= XboxEnumeration
    // *mEnum), null-checks it and vcalls slot 2 == IsEnumerating(); fn_827B4D10
    // is `return this->0x78 != 0` and 0x78 is StorePurchaser *mPurchaser ==
    // InCheckout().  (I first read these the other way round off the vtable
    // slot names and the header offsets corrected it.)  They are the 360
    // equivalents of the Wii commerce check, so the Wii clause did survive the
    // port -- it was translated, not deleted.
    HANDLE_EXPR(
        user_can_do_input,
        IsLoaded() && !IsEnumerating() && !InCheckout() && mLastRequest.empty()
    )
    HANDLE_ACTION(set_shortcut_data, SetShortcutData(_msg->Array(2)))
    HANDLE_ACTION(apply_shortcut_provider, ApplyShortcutProvider(_msg->Obj<UIList>(2)))
    HANDLE_MESSAGE(LocalUserLeftMsg)
    HANDLE_MESSAGE(MetadataLoadedMsg)
    HANDLE_SUPERCLASS(StorePanel)
    HANDLE_CHECK(0x2B0)
END_HANDLERS

// NO SYNC_PROP here.  Retail's SyncProperty is the bare superclass chain: the
// `waiting` branch we used to emit is ENTIRELY ours-only in the diff -- nine
// target-absent instructions (lis/lwz ?waiting@@3VSymbol@@A@h/@l, the cmplw
// against the incoming Symbol, and the PropSync call on this+0xe1) with no
// counterpart anywhere in retail's 30-instruction body.  The property does not
// exist on retail's BandStorePanel.
BEGIN_PROPSYNCS(BandStorePanel)
    SYNC_SUPERCLASS(StorePanel)
END_PROPSYNCS

// Retail fn_82606020 (264 B), emitted in this translation unit.  Declared in
// BandSongMetadata.h; defined HERE and not in the header so that MSVC emits one
// out-of-line body in BandStorePanel.obj exactly as retail does, rather than a
// COMDAT in every TU that names the type.  Both of Poll's message sites become a
// single `bl` to this, which is why retail's Poll frame is 0xf0 and ours was
// 0x140: the five DataNode temporaries live on THIS function's frame, not on
// Poll's.
MetadataLoadedMsg::MetadataLoadedMsg(
    DataArray *arr, bool loaded, const char *name, bool b2, bool b3
)
    // The four scalar arguments are passed RAW and converted IMPLICITLY -- not
    // wrapped in explicit DataNode(...) temporaries.  Message's ctor takes
    // `const DataNode &`, and DataNode(int) / DataNode(const char *) are not
    // `explicit`, so both spellings compile and are semantically identical --
    // but they do not generate the same code.  For an explicit functional-cast
    // temporary MSVC threads the DataNode ctor's returned `this` through a
    // callee-saved register; for an implicit conversion it re-forms
    // `addi rN, r31, <slot>` from the temporary's known stack slot, which is
    // retail's codegen.  Visible as one FEWER callee-saved register (retail
    // `bl __savegprlr_27` vs our `__savegprlr_26`) and a 16-byte smaller frame
    // (0xb0 vs 0xc0).  Same lever as the three Request() arms in Handle.
    // DataNode(arr, kDataArray) stays explicit -- two-argument ctor, no
    // implicit form.
    : Message(MetadataLoadedMsg::Type(), DataNode(arr, kDataArray), loaded, name, b2, b3) {}

void BandStorePanel::Poll() {
    StorePanel::Poll();
    if (mMetadataLoader && !mLastRequest.empty()) {
        mMetadataLoader->PollLoading();
        if (mMetadataLoader->IsLoaded()) {
            DataArray *metadata = mMetadataLoader->GetUnk4();
            if (metadata->Size()) {
                metadata->AddRef();
                MILO_ASSERT(metadata, 0x11C);
                const char *nullStr = gNullStr;
                static MetadataLoadedMsg msg(metadata, true, nullStr, false, false);
                msg[0] = DataNode(metadata, kDataArray);
                msg[2] = DataNode(mLastRequest.c_str());
                msg[3] = DataNode((int)(mLastRequest == GetIndexFile()));
                msg[4] = DataNode((int)!mLastRequestExtra);
                String path(mLastRequest);
                mLastRequest.erase();
                delete mMetadataLoader;
                mMetadataLoader = 0;
                Export(msg.mData, true);
                Handle(msg.mData, true);
                metadata->Release();
                return;
            }
        }
        if (mMetadataLoader->HasFailed()) {
            MILO_NOTIFY("Request for %s failed.\n", mLastRequest.c_str());
            // objdiff resolves retail's callee here to ??0DataArrayPtr@@QAA@XZ,
            // i.e. the default DataArrayPtr ctor (`mData = new DataArray(0)`),
            // NOT a bare `new DataArray(0)`. Its inlined dtor supplies the
            // trailing Release, which is why the explicit one below is gone.
            DataArrayPtr empty;
            {
                MetadataLoadedMsg msg(empty, false, gNullStr, false, false);
                msg[2] = DataNode(mLastRequest.c_str());
                msg[3] = DataNode((int)(mLastRequest == GetIndexFile()));
                msg[4] = DataNode((int)!mLastRequestExtra);
                mLastRequest.erase();
                if (mMetadataLoader) {
                    delete mMetadataLoader;
                    mMetadataLoader = 0;
                }
                Export(msg.mData, true);
            }
        }
    }
}

// Retail implements this on top of the base call; rb3-Wii has it as the bare
// `return StorePanel::UpdateOffers(list, b);` we used to carry, character for
// character, so the oracle could only ever confirm the stub. Read off retail
// bytes at fn_82607438. Every structural claim below is checked against the
// compiler's own layout, not against header comments:
//   * this+0x3c = mOffers, this+0x48 = mPendingOffers, both vector<StoreOffer*>,
//     selected by `b` with the same polarity StorePanel::UpdateOffers uses;
//   * BandStoreOffer::mDemo @0xe0 and ::mUpgrade @0x120 -- retail's two
//     `addi rN, r29, 0xe0 / 0x120`;
//   * fn_827A6430 = StorePurchaseable::Exists() const;
//   * fn_8282A0C8 = __RTDynamicCast, with the two RTTI type descriptors read
//     out of .data as ".?AVStoreOffer@@" -> ".?AVBandStoreOffer@@";
//   * the `bctrl` through vtable byte offset 0x68 is slot 26, which the layout
//     report names StorePanel::UpdateFromEnumProduct -- whose (StorePurchaseable*,
//     const EnumProduct*) signature is exactly the argument pair retail sets up.
// The dynamic_cast result is deliberately NOT null-checked: retail does
// `addi r31, r29, 0xe0` straight off the return value.
int BandStorePanel::UpdateOffers(const std::list<EnumProduct> &list, bool b) {
    int result = StorePanel::UpdateOffers(list, b);
    if (result == kStoreErrorCacheNoSpace)
        return result;
    std::vector<StoreOffer *> &offers = b ? mPendingOffers : mOffers;
    for (std::vector<StoreOffer *>::iterator it = offers.begin(); it != offers.end();
         ++it) {
        BandStoreOffer *offer = dynamic_cast<BandStoreOffer *>(*it);
        std::list<EnumProduct>::const_iterator e;
        if (offer->mDemo.Exists()) {
            e = std::find(list.begin(), list.end(), offer->mDemo);
            if (e != list.end()) {
                result = kStoreErrorSuccess;
                UpdateFromEnumProduct(&offer->mDemo, &*e);
            }
        }
        if (offer->mUpgrade.Exists()) {
            e = std::find(list.begin(), list.end(), offer->mUpgrade);
            if (e != list.end()) {
                result = kStoreErrorSuccess;
                UpdateFromEnumProduct(&offer->mUpgrade, &*e);
            }
        }
    }
    return result;
}
