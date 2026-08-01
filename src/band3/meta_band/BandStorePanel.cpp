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

int BandStorePanel::StoreUser() const {
    LocalBandUser *l = TheInputMgr->GetUser()
        ? TheInputMgr->GetUser()->GetLocalBandUser()
        : 0;
    return (int)(LocalUser *)l;
}

StoreOffer *BandStorePanel::MakeNewOffer(const StorePackedOfferBase *base, bool isRbn) {
    return new BandStoreOffer(base, &TheSongMgr, isRbn);
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
    return StorePanel::IsLoaded() || !mLoadOk;
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
    LocalBandUser *u = dynamic_cast<LocalBandUser *>((LocalUser *)StoreUser());
    if (u && !u->IsJoypadConnected()) {
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
            String url(sRequestPrefix);
            Symbol region = PlatformRegionToSymbol(ThePlatformMgr.GetRegion());
            url += MakeString("/%s%s", region, path);
            Server *server = TheNet.GetServer();
            if (server && server->IsConnected()) {
                url += MakeString("?pid=%u", server->GetMasterProfileID());
            }
            mMetadataLoader = new DataNetLoader(url);
            mStartBrowserAtBottom = false;
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

BEGIN_HANDLERS(BandStorePanel)
    HANDLE_EXPR(get_request_prefix, "dlc_store")
    HANDLE_ACTION(request, Request(String(_msg->Str(2)), _msg->Int(3)))
    if (sym == request_prev_chunk) {
        Request(String(mPrevChunkPath.c_str()), true);
        mStartBrowserAtBottom = true;
        return 0;
    }
    HANDLE_ACTION(request_next_chunk, Request(String(mNextChunkPath.c_str()), true))
    HANDLE_EXPR(should_start_browser_at_bottom, mStartBrowserAtBottom)
    HANDLE_EXPR(request_in_progress, mMetadataLoader != 0 || !(TheStoreMetadata.mFlags & 8))
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
    HANDLE_EXPR(sort_name, SortName())
    // rb3-Wii's user_can_do_input tail checked TheWiiCommerceMgr async op state;
    // there is no CommerceMgr on 360 (Xbox uses XboxEnumeration), so the
    // Wii-only commerce clause is dropped. (Handle is a deferred funclet wall.)
    HANDLE_EXPR(
        user_can_do_input,
        mUserCanDoInput == 0 && IsLoaded() && mLastRequest.empty()
    )
    HANDLE_ACTION(set_shortcut_data, SetShortcutData(_msg->Array(2)))
    HANDLE_ACTION(apply_shortcut_provider, ApplyShortcutProvider(_msg->Obj<UIList>(2)))
    HANDLE_MESSAGE(LocalUserLeftMsg)
    HANDLE_MESSAGE(MetadataLoadedMsg)
    HANDLE_SUPERCLASS(StorePanel)
    HANDLE_CHECK(0x2B0)
END_HANDLERS

BEGIN_PROPSYNCS(BandStorePanel)
    SYNC_PROP(waiting, mUserCanDoInput)
    SYNC_SUPERCLASS(StorePanel)
END_PROPSYNCS

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
                static Message msg(
                    MetadataLoadedMsg::Type(),
                    DataNode(metadata, kDataArray),
                    DataNode(1),
                    DataNode(nullStr),
                    DataNode(0),
                    DataNode(0)
                );
                msg[0] = DataNode(metadata, kDataArray);
                msg[2] = DataNode(mLastRequest.c_str());
                msg[3] = DataNode(
                    (int)(mLastRequest == MakeString("%d", StoreBuildNum()))
                );
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
            DataArray *empty = new DataArray(0);
            {
                Message msg(
                    MetadataLoadedMsg::Type(),
                    DataNode(empty, kDataArray),
                    DataNode(0),
                    DataNode(gNullStr),
                    DataNode(0),
                    DataNode(0)
                );
                msg[2] = DataNode(mLastRequest.c_str());
                msg[3] = DataNode(
                    (int)(mLastRequest == MakeString("%d", StoreBuildNum()))
                );
                msg[4] = DataNode((int)!mLastRequestExtra);
                mLastRequest.erase();
                if (mMetadataLoader) {
                    delete mMetadataLoader;
                    mMetadataLoader = 0;
                }
                Export(msg.mData, true);
            }
            empty->Release();
        }
    }
}

int BandStorePanel::UpdateOffers(const std::list<EnumProduct> &list, bool b) {
    return StorePanel::UpdateOffers(list, b);
}
