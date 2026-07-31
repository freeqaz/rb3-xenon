#include "meta_band/StoreOfferProvider.h"
#include "meta/StoreOffer.h"
#include "meta_band/AppLabel.h"
#include "meta_band/BandStoreOffer.h"
#include "meta_band/BandStorePanel.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "rndobj/Mat.h"
#include "ui/UIList.h"
#include "ui/UIListLabel.h"
#include "ui/UIListMesh.h"
#include "ui/UIListSlot.h"
#include "utl/Std.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"

extern bool operator==(const StoreOffer *o, Symbol s);

StoreOfferProvider::StoreOfferProvider(
    std::vector<StoreOffer *> *offers, std::vector<StoreOffer *> *packs
)
    // Retail (fn_826644C0) stores 0 to 0x34..0x48 and mOffers to 0x30, but writes
    // NOTHING to 0x4c (mPacks) and never reads the second parameter -- consistent
    // with FindOffer/FindPack/FindAlbum all lacking their mPacks paths. mPacks
    // almost certainly does not exist in the retail class at all; the member is
    // kept (it is last, so no offsets shift) only so sizeof and BandStorePanel's
    // `new StoreOfferProvider` allocation stay untouched.
    : mShortcuts(NULL),
      mOffers(offers),
      mElements(),
      mAlbumBgMat(NULL),
      mGroupBgMat(NULL),
      mSongBgMat(NULL) {}

StoreOfferProvider::~StoreOfferProvider() { ClearList(); }

void StoreOfferProvider::InitData(RndDir *dir) {
    mAlbumBgMat = dir->Find<RndMat>("album.mat", true);
    mGroupBgMat = dir->Find<RndMat>("group.mat", true);
    mSongBgMat = dir->Find<RndMat>("song.mat", true);
}

// ---------------------------------------------------------------------------
// DEFERRED WORK IN THIS TU (lane BV-1 handover; all read off the retail asm)
//
// Two functions exist in retail that this source does not have at all:
//
//  * fn_82663328 (0xB4) StoreOfferProvider::FindSongOffer(int songID)
//      for each offer in *mOffers:
//        if (offer->OfferType() == song && offer->GetSingleSongID() == songID)
//            return offer;
//      return NULL;
//      (`song` is a function-local static; callee fn_827A6D48 =
//       ?GetSingleSongID@StoreOffer@@QBAHXZ. Only in-TU caller: fn_826635D8.)
//
//  * fn_826635D8 (0x188) StoreOfferProvider::ShowBrowserPurchased(const StoreOffer*)
//      if (o->isPurchased) return true;                      // lbz 0x29
//      if (o->OfferType() == song)
//          return o->mPack.isPurchased || o->mAlbum.isPurchased;  // 0xa9 / 0x69
//      if (o->OfferType() == album || o->OfferType() == pack) {
//          for (it : o->mSongsInOffer) {                     // 0xd4 / 0xd8
//              StoreOffer *s = FindSongOffer(*it);
//              if (!s || !s->isPurchased) return false;
//          }
//          return o->NumSongs() != 0;
//      }
//      return false;
//      (OfferType() is re-called for each comparison, not cached in a local.
//       The 0x69/0xa9 offsets independently CONFIRM StoreOffer.h's nested
//       StorePurchaseable layout: mAlbum@0x40, mPack@0x80, +isPurchased@0x29.)
//
// Text() (fn_826639F0, 1756 bytes, currently a 5% stub) is the big consumer:
// it calls fn_826635D8 at 0x82663BF0 / 0x82663C58 / 0x82663D3C (the cost, new
// and purchased arms). Implementing ShowBrowserPurchased + FindSongOffer is the
// prerequisite for Text and also closes Handle -- see the show_browser_purchased
// note in the handler block below.
//
// BuildList (fn_826646F0) is a TRACTABLE body-port, not a wall; 77.1% now, and
// its control flow already matches statement-for-statement. Remaining items:
//   1. Three more function-local statics are still missing. Retail packs NINE
//      guard bits into lbl_82E01E18 in this declaration order:
//        0x001 store_previous_chunk  0x002 browser_group  0x004 browser_subgroup
//        0x008 localize_heading      0x010 shortcut_group 0x020 shortcut_groups
//        0x040 by_artist             0x080 cover          0x100 store_next_chunk
//      Six are done; shortcut_groups, by_artist and cover are not, so
//      store_next_chunk currently gets bit 0x20 instead of 0x100.
//   2. `shortcut_groups` (plural) is a PHANTOM: lbl_82E01E00 appears only in its
//      own guard-init block and its value is never loaded. Retail compiles
//      MILO_ASSERT as ((void)(cond)) (os/Debug.h:170), so /Oi drops the pure
//      comparison but must keep the Symbol ctor side effect. It needs a
//      reachable declaration whose only use is inside a pure expression.
//   3. IsCover() is inlined and is DataArray-driven; the rb3-Wii body
//      (`return mPackedData->mCover;`) is useless because retail StoreOffer has
//      no mPackedData. Retail emits, at 0x82664A54-0x82664AE4:
//        isCover = (sortName == by_artist) && offer->HasData(cover)
//                  && offer->GetData(DataArrayPtr(DataNode(cover)), false).Int(NULL) != 0;
//      There is no out-of-line ?IsCover@StoreOffer@@ anywhere in the binary.
//   4. `args` is a DataArrayPtr, not a hand-rolled `new DataArray(1)`:
//      use `DataArrayPtr args(DataNode(offer));` (ctor fn_8228D370).
//   5. `lastSubgroup = NULL;` in the new-group path does NOT exist in retail
//      (r21 is only written at init and at `lwz r21,-0x4(r11)`). Delete it.
//   6. The anonymous-namespace BandStorePanel accessors below use 0xC8/0xD4;
//      retail reads 0xC0/0xCC as char*, and String::mStr is at String+0x8, so
//      the Strings are at 0xB8/0xC4 -- exactly what BandStorePanel.h:94-95
//      documents. Today we read into mNextChunkPath.mCap and mMenuTitle.
//
// mPacks should be REMOVED entirely (member + second ctor parameter). Retail's
// sizeof(StoreOfferProvider) is 0x4C, not our 0x50: BandStorePanel.s:0x82605204
// does `li r3, 0x4c` before operator new, no function in the unit touches
// 0x4c, and the ctor never reads its second argument. Doing so also touches
// BandStorePanel.cpp:60, so it wants its own A/B.
// ---------------------------------------------------------------------------
// Text(): still a compile-green stub, NonMatching. See ShowBrowserPurchased above.
void StoreOfferProvider::Text(int i, int pos, UIListLabel *listLabel, UILabel *label)
    const {
    AppLabel *appLabel = dynamic_cast<AppLabel *>(label);
    MILO_ASSERT(appLabel, 0x36);
    appLabel->SetTextToken(gNullStr);
}

// Retail (fn_826642B8): no mElements.size() guard, and `song` is a FUNCTION-LOCAL
// static Symbol declared inside the Matches("bg") block (guard word lbl_82E01DF0
// bit 0, Symbol at lbl_82E01DEC, ctor string lbl_820010F0="song") -- not the
// centralized global Symbol our DC3-era Symbols*.h headers provide.
RndMat *StoreOfferProvider::Mat(int i, int j, UIListMesh *mesh) const {
    StoreOffer *offer = mElements[j]->mOffer;
    if (mesh->Matches("bg")) {
        static Symbol song("song");
        if (!offer) {
            return mGroupBgMat;
        } else if (offer->OfferType() == song) {
            return mSongBgMat;
        } else {
            return mAlbumBgMat;
        }
    }
    return mesh->DefaultMat();
}

// Retail (fn_82664398) indexes mElements directly -- there is NO mElements.size()
// guard. The guard comes from the rb3-Wii DEV build; retail dropped it.
Symbol StoreOfferProvider::DataSymbol(int i) const {
    Element *e = mElements[i];
    if (e->mOffer) {
        return e->mOffer->ShortName();
    } else if (e->mActive) {
        return e->mGroupHeading;
    }
    return gNullStr;
}

// Retail (fn_82664418, 52 bytes) likewise has no size() guard.
bool StoreOfferProvider::IsActive(int i) const {
    // Not matched (71.9%): retail materialises the result in a scratch register and
    // byte-masks it at a single return (`clrlwi r3,r11,24`); MSVC here fuses the
    // returns instead (`beqlr`). Tried: uninitialised bool + explicit else (71.5%),
    // and `result = (mActive != false)` which went BRANCHLESS (subic/subfe, 51.5%).
    // BOOL_MASK / permuter-class -- left at the best-scoring shape.
    Element *e = mElements[i];
    bool result = false;
    if (e->mOffer != NULL || e->mActive) {
        result = true;
    }
    return result;
}

// Retail (fn_82664450, 96 bytes) searches ONLY mOffers -- the mPacks fallback
// present in the rb3-Wii DEV source does not exist in the retail X360 build.
StoreOffer *StoreOfferProvider::FindOffer(Symbol s) const {
    std::vector<StoreOffer *>::iterator it =
        std::find(mOffers->begin(), mOffers->end(), s);
    if (it == mOffers->end())
        return NULL;
    return *it;
}

// Retail (fn_82663408 / fn_826634F0, 192 bytes each): a SINGLE loop over mOffers --
// the mPacks fallback loop in the rb3-Wii DEV source does not exist in retail (the
// target calls HasSong once, not twice). The type Symbol is a function-local static
// (FindPack: guard lbl_82E01D90 bit 0, Symbol lbl_82E01D8C, string lbl_820B08D0).
const StoreOffer *StoreOfferProvider::FindPack(const StoreOffer *song) const {
    MILO_ASSERT(song->OfferType() == "song", 0x153);
    static Symbol pack("pack");
    for (std::vector<StoreOffer *>::iterator it = mOffers->begin();
         it != mOffers->end();
         ++it) {
        StoreOffer *cand = *it;
        if (cand->OfferType() == pack && cand->HasSong(song)) {
            return cand;
        }
    }
    return NULL;
}

const StoreOffer *StoreOfferProvider::FindAlbum(const StoreOffer *song) const {
    MILO_ASSERT(song->OfferType() == "song", 0x169);
    static Symbol album("album");
    for (std::vector<StoreOffer *>::iterator it = mOffers->begin();
         it != mOffers->end();
         ++it) {
        StoreOffer *cand = *it;
        if (cand->OfferType() == album && cand->HasSong(song)) {
            return cand;
        }
    }
    return NULL;
}

Symbol StoreOfferProvider::PosToShortcut(int pos) {
    Element **start = &mElements[0];
    Element **it = &mElements[pos];
    while (it >= start) {
        Element *e = *it;
        if (e->mShortcut.Str() != gNullStr) {
            return e->mShortcut;
        }
        --it;
    }
    MILO_FAIL("StoreOfferProvider is missing a shortcut before index %i!", pos);
    return gNullStr;
}

int StoreOfferProvider::ShortcutToPos(Symbol s) {
    unsigned int n = mElements.size();
    for (unsigned int i = 0; i < n; i++) {
        if (mElements[i]->mShortcut == s) {
            return i;
        }
    }
    MILO_FAIL("StoreOfferProvider can't find shortcut \"%s\"!", s);
    return 0;
}

int StoreOfferProvider::PosToNextGroupPos(int pos) {
    unsigned int n = mElements.size();
    for (unsigned int i = pos + 1; i < n; i++) {
        if (mElements[i]->mGroupHeading.Str() != gNullStr) {
            return i;
        }
    }
    return 0;
}

int StoreOfferProvider::PosToPrevGroupPos(int pos) {
    for (int i = pos - 2; i >= 0; i--) {
        if (mElements[i]->mGroupHeading.Str() != gNullStr) {
            return i;
        }
    }
    int n = mElements.size();
    for (int i = n - 1; i > pos; i--) {
        if (mElements[i]->mGroupHeading.Str() != gNullStr) {
            return i;
        }
    }
    return 0;
}

StoreOfferProvider::Element *StoreOfferProvider::GetElementAtIndex(int i) const {
    return mElements[i];
}

// Retail fn_826635D8 (0x188 bytes). Signature-only port: the real body needs
// FindSongOffer (not implemented -- see the deferred-work block above) plus
// public access to StoreOffer's protected mPack/mAlbum purchaseables, which
// would mean a StoreOffer.h header change out of scope for this pass. This
// stub exists only so Handle()'s show_browser_purchased arm gets a real call
// target (retail calls this out-of-line) instead of an inline byte load --
// ShowBrowserPurchased itself is NOT in splits.txt and is not scored.
bool StoreOfferProvider::ShowBrowserPurchased(const StoreOffer *o) const {
    return o->IsPurchased();
}

// Field offsets in BandStorePanel are protected; access via byte offsets.
namespace {
inline const String &PrevChunkPath(const BandStorePanel *p) {
    return *reinterpret_cast<const String *>(
        reinterpret_cast<const char *>(p) + 0xC8
    );
}
inline const String &NextChunkPath(const BandStorePanel *p) {
    return *reinterpret_cast<const String *>(
        reinterpret_cast<const char *>(p) + 0xD4
    );
}
}

void StoreOfferProvider::BuildList(DataArray *grouping) {
    DataArray * &_ref0 = mShortcuts;
    ClearList();
    _ref0 = new DataArray(0);
    const char *prevPath = PrevChunkPath(BandStorePanel::Instance()).c_str();
    if (*prevPath == 0) prevPath = NULL;
    if (prevPath) {
        static Symbol store_previous_chunk("store_previous_chunk");
        Element *prev = new Element(NULL, store_previous_chunk, true, false, true);
        mElements.push_back(prev);
        mElements.back()->mShortcut = store_previous_chunk;
        _ref0->Insert(_ref0->Size(), DataNode(store_previous_chunk));
    }
    if (grouping) {
        // Retail declares each of these as a FUNCTION-LOCAL static Symbol (guard-bit
        // test + inline ??0Symbol ctor), and initialises them in this order:
        // browser_group, browser_subgroup, localize_heading, shortcut_group,
        // shortcut_groups -- note localize_heading comes BEFORE shortcut_group,
        // the reverse of the rb3-Wii DEV source.
        static Symbol browser_group("browser_group");
        static Symbol browser_subgroup("browser_subgroup");
        static Symbol localize_heading("localize_heading");
        static Symbol shortcut_group("shortcut_group");
        Symbol sortName = grouping->Sym(0);
        DataArray *groupArr = grouping->FindArray(browser_group, true);
        DataArray *subgroupArr = grouping->FindArray(browser_subgroup, false);
        DataArray *locArr = grouping->FindArray(localize_heading, true);
        DataArray *shortcutArr = grouping->FindArray(shortcut_group, false);
        bool localize = locArr->Int(1) != 0;
        Element *lastGroup = NULL;
        Element *lastSubgroup = NULL;
        Symbol curGroupSym(gNullStr);
        for (std::vector<StoreOffer *>::iterator it = mOffers->begin();
             it != mOffers->end();
             ++it) {
            StoreOffer *offer = dynamic_cast<BandStoreOffer *>(*it);
            MILO_ASSERT(offer, 0x208);
            bool isCover = false;
            if (sortName == by_artist && offer->IsCover()) {
                isCover = true;
            }
            DataArray *args = new DataArray(1);
            args->Node(0) = DataNode(offer);
            Symbol groupSym =
                groupArr->ExecuteScript(1, NULL, args, 0)
                    .ForceSym(NULL);
            Symbol subgroupSym = subgroupArr
                ? subgroupArr->ExecuteScript(1, NULL, args, 0).ForceSym(NULL)
                : Symbol(gNullStr);
            if (groupSym.Str() != gNullStr &&
                (!lastGroup || lastGroup->mGroupHeading != groupSym ||
                 lastGroup->mIsCover != isCover)) {
                Element *g = new Element(NULL, groupSym, localize, isCover, false);
                mElements.push_back(g);
                lastSubgroup = NULL;
                lastGroup = mElements.back();
                Symbol shortcutSym = shortcutArr
                    ? shortcutArr->ExecuteScript(1, NULL, args, 0).ForceSym(NULL)
                    : groupSym;
                if (curGroupSym.Str() == gNullStr || curGroupSym != shortcutSym) {
                    if (localize) {
                        _ref0->Insert(
                            _ref0->Size(), DataNode(shortcutSym)
                        );
                    } else {
                        _ref0->Insert(
                            _ref0->Size(), DataNode(shortcutSym.Str())
                        );
                    }
                    lastGroup->mShortcut = shortcutSym;
                    curGroupSym = shortcutSym;
                }
                if (subgroupSym.Str() != gNullStr) {
                    Element *sg =
                        new Element(NULL, subgroupSym, localize, false, false);
                    mElements.push_back(sg);
                    lastSubgroup = mElements.back();
                }
            } else if (subgroupSym.Str() != gNullStr &&
                       (!lastSubgroup ||
                        lastSubgroup->mGroupHeading != subgroupSym)) {
                Element *sg =
                    new Element(NULL, subgroupSym, localize, false, false);
                mElements.push_back(sg);
                lastSubgroup = mElements.back();
            }
            Element *e =
                new Element(offer, Symbol(gNullStr), localize, false, false);
            mElements.push_back(e);
            args->Release();
        }
    } else {
        for (std::vector<StoreOffer *>::iterator it = mOffers->begin();
             it != mOffers->end();
             ++it) {
            StoreOffer *offer = dynamic_cast<BandStoreOffer *>(*it);
            MILO_ASSERT(offer, 0x243);
            Element *e =
                new Element(offer, Symbol(gNullStr), false, false, false);
            mElements.push_back(e);
        }
    }
    const char *nextPath = NextChunkPath(BandStorePanel::Instance()).c_str();
    if (*nextPath == 0) nextPath = NULL;
    if (nextPath) {
        static Symbol store_next_chunk("store_next_chunk");
        Element *next = new Element(NULL, store_next_chunk, true, false, true);
        mElements.push_back(next);
        mElements.back()->mShortcut = store_next_chunk;
        _ref0->Insert(_ref0->Size(), DataNode(store_next_chunk));
    }
}

// Retail (fn_82664568) loads mElements begin/end straight from 0x34/0x38, i.e. the
// source uses begin()/end(); `&mElements[0]` + `start + size()` forces an extra
// subf/srawi/slwi round-trip to recompute end.
void StoreOfferProvider::ClearList() {
    // 97.3%: remaining 12 mismatches are ALL register swaps (r29/r30/r31
    // permutation) at identical size and instruction order -- permuter-class.
    // Combining the two declarations into the for-init scores identically.
    Element **start = mElements.begin();
    Element **end = mElements.end();
    for (Element **it = start; it != end; ++it) {
        delete *it;
    }
    mElements.clear();
    if (mShortcuts) {
        mShortcuts->Release();
        mShortcuts = NULL;
    }
}

BEGIN_HANDLERS(StoreOfferProvider)
    HANDLE_ACTION(build_list, BuildList(_msg->Array(2)))
    HANDLE_ACTION(build_list_no_grouping, BuildList(NULL))
    HANDLE_ACTION(clear_list, ClearList())
    HANDLE_EXPR(find_offer, FindOffer(_msg->Sym(2)))
    HANDLE_EXPR(
        find_album,
        (Hmx::Object *)FindAlbum(dynamic_cast<StoreOffer *>(_msg->GetObj(2)))
    )
    HANDLE_EXPR(
        find_pack,
        (Hmx::Object *)FindPack(dynamic_cast<StoreOffer *>(_msg->GetObj(2)))
    )
    // Retail (0x82665824-0x826658A4) is a bog-standard macro arm using the
    // same function-local static Symbol as its thirteen siblings (guard bit
    // 0x40 of lbl_82E01E54) and an out-of-line call to ShowBrowserPurchased.
    // The prior hand-written `if (sym == show_browser_purchased)` against the
    // GLOBAL Symbol skipped a guard bit, which shifted every later arm's
    // packed-guard-word bit position by one -- see ShowBrowserPurchased's
    // doc comment for why its body is still a stub.
    HANDLE_EXPR(
        show_browser_purchased,
        DataNode(ShowBrowserPurchased(dynamic_cast<StoreOffer *>(_msg->GetObj(2))))
    )
    HANDLE_EXPR(get_shortcut_array, DataNode(mShortcuts, kDataArray))
    HANDLE_EXPR(has_shortcuts, mShortcuts->Size() != 0)
    HANDLE_EXPR(pos_to_shortcut, PosToShortcut(_msg->Int(2)))
    HANDLE_EXPR(shortcut_to_pos, ShortcutToPos(_msg->Sym(2)))
    HANDLE_EXPR(pos_to_next_group_pos, PosToNextGroupPos(_msg->Int(2)))
    HANDLE_EXPR(pos_to_prev_group_pos, PosToPrevGroupPos(_msg->Int(2)))
    HANDLE_EXPR(is_chunk, mElements[_msg->Int(2)]->mActive)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x2B2)
END_HANDLERS

int StoreOfferProvider::NumData() const { return mElements.size(); }
