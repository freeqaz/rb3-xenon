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
#include "utl/Locale.h"
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

// Retail fn_82605720 (80 B, primary vtable slot 17 / disp 0x44).  The rb3-Wii
// dev oracle calls TheInputMgr->GetUser() TWICE with a null test between them;
// retail 360 calls it ONCE and does not test it.  Read off the retail body:
// there is exactly one `bl` to InputMgr::GetUser and no compare after it -- the
// `cmplwi r3,0 / beq` that IS there sits AFTER the GetLocalBandUser vcall and
// guards the compiler-generated LocalBandUser* -> LocalUser* virtual-base
// adjust (`lwz r11,4(r3)` vbptr, `lwz r11,0xc(r11)` vbtable idx 3 == LocalUser,
// the second of `class LocalBandUser : public virtual BandUser, public virtual
// LocalUser`).  So the null check is the conversion's, not the source's.
LocalUser *BandStorePanel::StoreUser() const {
    return TheInputMgr->GetUser()->GetLocalBandUser();
}

// Retail fn_82605878 (104 B, primary vtable slot 27 / disp 0x6c).  EMPTY in BOTH
// oracles (rb3-Wii BandStorePanel.h:46 and our header carried `{}`) and NOT empty
// in retail 360 -- a real divergence, adjudicated on bytes:
//   * the body never reads its `this` (r3 is overwritten by `mr r3,r4` before
//     first use), which is why an override that ignores `this` fits;
//   * `bl __RTDynamicCast` (fn_8282A0C8) with the two RTTI type descriptors
//     .rdata 0x82C6E6B0 ".?AVLocalUser@@" (source) and 0x82C72528
//     ".?AVLocalBandUser@@" (target), VfDelta 0 and isReference 0;
//   * the result is then put through the vbtable idx-2 adjust (`lwz r11,4(r3)`,
//     `lwz r11,8(r11)`) == BandUser, the FIRST virtual base of LocalBandUser;
//   * and handed to fn_825B1598 == ?SetUser@InputMgr@@QAAXPAVBandUser@@@Z on
//     the global at 0x82DFF3BC, whose parameter type is exactly what that
//     adjust produces.
// The null-guard around the adjust is the compiler's, as in StoreUser above.
void BandStorePanel::StoreUserProfileSwappedToUser(LocalUser *u) {
    TheInputMgr->SetUser(dynamic_cast<LocalBandUser *>(u));
}

StoreOffer *BandStorePanel::MakeNewOffer(DataArray *da) {
    return new BandStoreOffer(da, &TheSongMgr);
}

// Retail fn_82605B48 (100 B, primary vtable slot 19 / disp 0x4c) scans ONE
// vector, not two: the loop bounds are `lwz r31,0x3c(r3)` and `lwz r11,0x40(r30)`
// == mOffers.begin()/end() (StorePanel.h puts mOffers at 0x3c and
// mPendingOffers at 0x48), and there is no second loop in the body at all.
// The rb3-Wii dev oracle's trailing unk48 pass is not in the retail 360 build.
StoreOffer *BandStorePanel::FindOffer(Symbol s) const {
    for (std::vector<StoreOffer *>::const_iterator it = unk38.begin();
         it != unk38.end(); ++it) {
        StoreOffer *o = *it;
        if (o->ShortName() == s)
            return o;
    }
    return 0;
}

// Retail fn_82608B70 (452 B).  Reconstructed from the retail body, every step
// adjudicated on bytes rather than inferred:
//   * the five probed sub-objects are StorePurchaseable instances at +0x0
//     (the StoreOffer's own StorePurchaseable base), +0x80 mPack, +0x40 mAlbum,
//     +0xe0 mDemo, +0x120 mUpgrade -- and retail probes them in exactly that
//     order, which is NOT address order.  Each `ld` is from sub-object +0x30,
//     i.e. StorePurchaseable::songID.
//   * the value pushed is a PRVALUE: retail `ld`s into a stack temp and passes
//     its address to push_back(const u64 &).  Binding the reference straight to
//     the member would need no temp, so the source calls the by-value accessor
//     SongID(), not the member.
//   * the tail is `sort` then `resize(unique(..) - begin())`, not the more usual
//     `erase(unique(..), end())`: retail has BOTH arms of STLport's resize (an
//     erase arm and an insert-at-end arm) and materialises the zero u64 default
//     argument UNCONDITIONALLY before the branch, which is what a defaulted
//     `const _Tp & = _Tp()` parameter does.
//   * `lbz r5, 0x50(r1)` before adjacent_find is its empty `equal_to<u64>`
//     functor passed by value out of an uninitialised slot -- STLport's 2-arg
//     `unique` forwards to the 3-arg `adjacent_find`.
void BandStorePanel::GetOfferIDsToEnumerate(std::vector<u64> &ids, bool pending)
    const {
    const std::vector<StoreOffer *> &offers = pending ? mPendingOffers : mOffers;
    for (int i = 0; i < offers.size(); i++) {
        BandStoreOffer *offer = dynamic_cast<BandStoreOffer *>(offers[i]);
        if (offer->Exists())
            ids.push_back(offer->SongID());
        if (offer->mPack.Exists())
            ids.push_back(offer->mPack.SongID());
        if (offer->mAlbum.Exists())
            ids.push_back(offer->mAlbum.SongID());
        if (offer->mDemo.Exists())
            ids.push_back(offer->mDemo.SongID());
        if (offer->mUpgrade.Exists())
            ids.push_back(offer->mUpgrade.SongID());
    }
    std::sort(ids.begin(), ids.end());
    u64 *newEnd = std::unique(ids.begin(), ids.end());
    ids.resize(newEnd - ids.begin());
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

// ⚠ HANDOFF (W16-CI): this body is a STUB and retail's is not, but the row is
// UNREACHABLE from this unit and no source work here can collect it.
//
// Retail's OnMsg(LocalUserLeftMsg&) is at 0x826067C0, 232 B, and it is PROVEN
// by the same dispatch-arm test used for OnMsg(MetadataLoadedMsg&): inside
// Handle, 0x826084EC calls ?Type@LocalUserLeftMsg@@SA?AVSymbol@@XZ, compares,
// branches away on mismatch, and falls through to `bl 0x826067C0` at
// 0x82608528. Its body is NOT `return DataNode(1)` -- it calls
// DataNode::GetObj and __RTDynamicCast, so ours is a real divergence.
//
// ⛔ BUT 0x826067C0 IS MIS-PINNED TO Mat.cpp. This unit's splits block ends
// exactly at `.text start:0x82606260 end:0x826067C0` and resumes at
// 0x826068A8; the 232 B between them are pinned to Mat.cpp, which is why the
// symbol appears in THIS unit's target obj as an undefined external (sec=0).
// So the row lives in Mat.cpp's target obj, whose base obj can never define
// ?OnMsg@BandStorePanel@@...LocalUserLeftMsg... -- adding a map entry for it
// would make the row read 0 % PERMANENTLY, the exact trap that "proving a name
// wrong does not make renaming safe" describes. The repair is a splits.txt
// re-home of [0x826067C0, 0x826068A8) into this unit, which W16-CI was banned
// from and which is NOT metric-neutral (PINHOME-1: +3 fns / +428 B), so it must
// be measured by whoever does it.
//
// PORTED ANYWAY (lane W16-GI, 2026-09-16) -- not for this row's own score, which
// the mis-pin makes uncollectable here, but because the EMPTY STUB it replaced
// was a codegen assertion that propagated into Handle.  rb3-Wii's dev body is
// `return DataNode(1);`; MSVC saw that body earlier in this TU, proved the
// callee nothrow, dropped the EH region that protects Handle's stack
// LocalUserLeftMsg temporary across the call, and its scheduler then hoisted
// the three OnMsg argument set-ups into the Message ctor's stores.  Retail
// keeps them contiguous before the bl -- exactly as BOTH compilers do for the
// MetadataLoadedMsg arm three instructions later, whose callee has a real
// body.  Same mechanism as the QuazalSession(bool) {} two-defect stub
// (project_oracle_fidelity_has_four_modes: "an empty function body is not
// neutral -- it is a codegen assertion").
//
// Body read off retail 0x826067C0 (r3 = sret, r4 = this, r5 = msg):
//   lwz r4,4(r5); lwz r11,0(r4); addi r3,r11,0x10; bl ?GetObj@DataNode@@
//        -> msg.mData->Node(2).GetObj(mData)            == msg.GetUser()
//   bl __RTDynamicCast(.., ??_R0 Hmx::Object, ??_R0 LocalUser, 0)
//   lwz r11,0(r30); lwz r11,0x44(r11); bctrl           -> this->StoreUser()
//   bne -> skip; guard bit @0x82E00810 / static Symbol @0x82E0080C built from
//   "critical_user_drop_out" (0x820B42A0) via ??0Symbol@@QAA@PBD@Z: a
//   FUNCTION-LOCAL static, not the Symbols2.h global CriticalUserListener uses
//   lwz r3,?TheUIEventMgr@@; li r5,0; bl ?TriggerEvent@UIEventMgr@@QAAXVSymbol@@PAVDataArray@@@Z
//   li r11,1; stw r28(=0),4(sret); stw r11,0(sret)     -> return DataNode(1)
DataNode BandStorePanel::OnMsg(const LocalUserLeftMsg &msg) {
    LocalUser *user = msg.GetUser();
    if (user == StoreUser()) {
        static Symbol critical_user_drop_out("critical_user_drop_out");
        TheUIEventMgr->TriggerEvent(critical_user_drop_out, 0);
    }
    return DataNode(1);
}

// Retail fn_82606280 (908 B).  The store index-.dta parser, reconstructed
// instruction-by-instruction off retail bytes -- the rb3-Wii dev oracle is the
// packed-StoreMetadata arm and carries only a skeleton of this, so every claim
// below is read from band.exe, not from the oracle.
//
// Node indices: Message::operator[](i) is mData->Node(i + 2), so retail's
// node[2]/node[4]/node[6] are exactly the msg[0]/msg[2]/msg[4] that Poll fills
// in -- the metadata DataArray, mLastRequest's path, and (int)!mLastRequestExtra.
//
// ⚠ `fn_8274B0F8` is mapped ?Int@DataNode@@ yet node[2]'s result is passed to
// FindArray as `this`.  That is not a contradiction and NOT a wrong map name:
// DataNode::Int and DataNode::Array are both `return mValue.<word>` once the
// MILO_ASSERTs compile out, so they are byte-identical COMDATs and ICF folded
// them onto one arbitrary survivor.  Array(2)/Int(6) is the correct spelling.
//
// Strings, read out of band.exe (VA - 0x82000000 in this region):
//   0x820BF638 "index_info"  0x820BF628 "previous_chunk"  0x820BF61C "next_chunk"
//   0x820BF614 "sorted"      0x8205EBF8 "title"           0x820AE0C0 "offers"
//   0x820116D8 "/"
// and lbl_82C71838 is a .data `const char *` whose value 0x82000C55 is the empty
// string == gNullStr, which is what the four resets assign.
// ⚠ The final lookup is "offers", NOT the "metadata" this function used to
// spell.  That was invisible to the metric (the target's argument is a
// placeholder lbl_, which name_check forgives) and wrong all the same.
DataNode BandStorePanel::OnMsg(const MetadataLoadedMsg &msg) {
    DataArray *data = msg->Array(2);
    String path(msg->Str(4));
    if (!msg->Int(6)) {
        mPrevChunkPath = gNullStr;
        mNextChunkPath = gNullStr;
        // NEGATIVE RESULT (W16-CK), measured -- do not re-try the named local.
        // Retail reads this Symbol back from its FRAME SLOT (`lwz r11, 0x50(r31)`)
        // where we read it through the ctor's returned `this` (`mr r11,r3` then
        // `lwz r11,0(r11)`); those are charges [40]/[43].  Spelling it as a named
        // local `Symbol nullSym(gNullStr); mSort = nullSym;` DOES close that pair
        // and is still wrong: the named local claims a dedicated 16-byte-aligned
        // slot, our frame grows 0xf0 -> 0x100, the 8 charges become 29 OFFSET
        // charges, and the four 40-byte EH funclets -- which objdiff pairs by BYTE
        // SIGNATURE, and whose signature encodes the parent frame size -- fall back
        // off 100.  Whole binary measured -96 B (+64 B from fn_826066D4 accidentally
        // re-pairing onto the WRONG frame, -160 B from the four funclets).
        // The hypothesis is refuted independently of the metric: retail
        // re-CONSTRUCTS into 0x50 TWICE (once for gNullStr, once for "index_info"),
        // which a named local cannot do.  So 0x50 is a reused TEMP slot and the
        // residue is MSVC temporary-slot rotation, not a missing declaration.
        mSort = Symbol(gNullStr);
        mMenuTitle = gNullStr;
        DataArray *info = data->FindArray(Symbol("index_info"), false);
        if (info) {
            // The directory part of the request path: retail calls
            // find_last_of("/") and feeds pos+1 to substr(0, n) as the count.
            String dir(path.substr(0, path.find_last_of("/") + 1));
            DataArray *prev = info->FindArray(Symbol("previous_chunk"), false);
            if (prev) {
                const char *s = prev->Str(1);
                // `lbz r11,0(r3) / cmplwi cr6,r11,0x2f` -- an absolute path is
                // taken as-is, a relative one is hung off the request's dir.
                if (*s == '/') {
                    mPrevChunkPath = s;
                } else {
                    mPrevChunkPath = dir + s;
                }
            }
            DataArray *next = info->FindArray(Symbol("next_chunk"), false);
            if (next) {
                const char *s = next->Str(1);
                if (*s == '/') {
                    mNextChunkPath = s;
                } else {
                    mNextChunkPath = dir + s;
                }
            }
            DataArray *sorted = info->FindArray(Symbol("sorted"), false);
            if (sorted) {
                mSort = sorted->Sym(1);
            }
            DataArray *title = info->FindArray(Symbol("title"), false);
            if (title) {
                // One call taking (sret, &node) -- the out-of-line DataNode copy
                // ctor, not Evaluate() (which would be a call returning a
                // reference plus a second call to copy it).  The trailing
                // `rlwinm. r11,r11,0,27,27` + DataArray::Release is ~DataNode
                // inlined from Data.h, which is why the local is spelled out.
                DataNode n(title->Node(1));
                if (n.Type() == kDataString) {
                    mMenuTitle = n.Str(0);
                } else {
                    mMenuTitle = Localize(n.Sym(0), 0);
                }
            }
        }
    }
    DataArray *found = data->FindArray(Symbol("offers"), false);
    if (found) {
        PopulateOffers(found, msg->Int(6) != 0);
        EnumerateOffers(msg->Int(6) != 0);
    }
    return DataNode(1);
}

// ⚠ HANDOFF (W16-CI): fn_82608D38 (88 B, fuzzy 0) is the last anonymous row in
// this unit and it is NOT a BandStorePanel method -- do not hunt for its source
// here. Scanning all of .text for callers gives exactly two, both in a
// different class: ?InitializeVisuals@CalibrationPanel@@ + 0x218 and
// ?OnInitializeContent@CalibrationPanel@@ + 0x2e0. Its body loads a global,
// calls one function, returns 5 when the result is null, otherwise makes two
// chained virtual calls -- a shared-header inline emitted once and laid out
// inside this unit's pin range, just below CalibrationPanel's own functions at
// 0x82608D90. Because BandStorePanel.cpp never uses that inline, our obj will
// never define it, so naming it buys a permanent 0 % row. Left anonymous ON
// PURPOSE; its 88 B are not collectable by this unit at any source quality.
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
    // HAND-EXPANDED HANDLE_ACTION (local-static dialect -- this TU compiles with
    // /DRB3_HANDLE_LOCAL_STATIC).  Retail's arm is TWO STATEMENTS, not one
    // comma expression, and the difference is visible in the byte order:
    //     retail:  bl Request ; addi r3,r31,0x58 ; bl ~String ; li r11,1 ; stb
    //     comma :  bl Request ; li r11,1 ; stb ; addi r3,r31,0x58 ; bl ~String
    // A comma operator keeps both operands inside ONE full-expression, so the
    // String temporary must outlive the assignment; retail destroys it first.
    // ObjMacros.h spells the macro body `(action);` -- a STYLE rule ("require
    // side-actions via comma operator"), not a codegen requirement -- so the
    // macro cannot express retail's shape.  The general repair is `action;` in
    // ObjMacros.h, which cascades to every HANDLE_ACTION in the tree and is out
    // of this lane's scope; this is that repair applied to one arm, and it is
    // byte-for-byte the macro's own expansion with the parens removed.
    {
        static Symbol _hs("request_prev_chunk");
        if (sym == _hs) {
            Request(mPrevChunkPath.c_str(), true);
            mStartBrowserAtBottom = true;
            return 0;
        }
    }
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
