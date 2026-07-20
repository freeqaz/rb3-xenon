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
    : mShortcuts(NULL),
      mOffers(offers),
      mPacks(packs),
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

// DEFERRED (retail struct/API drift): the retail Text() (fn_826639F0) reads
// offer purchase/pack state through helper-function calls against a retail
// StorePurchaseable layout (isPurchased/isAvailable/cost) that differs
// fundamentally from rb3-Wii's mOfferState/mPackedData pointer model. Matching
// it requires reconstructing retail's StoreOfferState/StorePackedOfferBase
// equivalents from asm and adding members to the 10-includer meta/StoreOffer.h
// (regression risk). Left as a compile-green stub; NonMatching. The clean,
// in-span match delivered by this TU is InitData (fn_82663968).
void StoreOfferProvider::Text(int i, int pos, UIListLabel *listLabel, UILabel *label)
    const {
    AppLabel *appLabel = dynamic_cast<AppLabel *>(label);
    MILO_ASSERT(appLabel, 0x36);
    appLabel->SetTextToken(gNullStr);
}

RndMat *StoreOfferProvider::Mat(int i, int j, UIListMesh *mesh) const {
    if (mElements.size() != 0) {
        StoreOffer *offer = mElements[j]->mOffer;
        if (mesh->Matches("bg")) {
            if (!offer) {
                return mGroupBgMat;
            } else if (offer->OfferType() == song) {
                return mSongBgMat;
            } else {
                return mAlbumBgMat;
            }
        }
    }
    return mesh->DefaultMat();
}

Symbol StoreOfferProvider::DataSymbol(int i) const {
    if (mElements.size() != 0) {
        Element *e = mElements[i];
        if (e->mOffer) {
            return e->mOffer->ShortName();
        } else if (e->mActive) {
            return e->mGroupHeading;
        }
    }
    return gNullStr;
}

bool StoreOfferProvider::IsActive(int i) const {
    if (mElements.size() < 1)
        return false;
    Element *e = mElements[i];
    bool result = false;
    if (e->mOffer != NULL || e->mActive) {
        result = true;
    }
    return result;
}

StoreOffer *StoreOfferProvider::FindOffer(Symbol s) const {
    std::vector<StoreOffer *>::iterator it =
        std::find(mOffers->begin(), mOffers->end(), s);
    if (it == mOffers->end()) {
        if (mPacks == 0)
            return NULL;
        it = std::find(mPacks->begin(), mPacks->end(), s);
        if (it == mPacks->end())
            return NULL;
        return *it;
    }
    return *it;
}

const StoreOffer *StoreOfferProvider::FindPack(const StoreOffer *song) const {
    MILO_ASSERT(song->OfferType() == "song", 0x153);
    for (std::vector<StoreOffer *>::iterator it = mOffers->begin();
         it != mOffers->end();
         ++it) {
        StoreOffer *cand = *it;
        if (cand->OfferType() == pack && cand->HasSong(song)) {
            return cand;
        }
    }
    if (mPacks) {
        for (std::vector<StoreOffer *>::iterator it = mPacks->begin();
             it != mPacks->end();
             ++it) {
            StoreOffer *cand = *it;
            if (cand->OfferType() == pack && cand->HasSong(song)) {
                return cand;
            }
        }
    }
    return NULL;
}

const StoreOffer *StoreOfferProvider::FindAlbum(const StoreOffer *song) const {
    MILO_ASSERT(song->OfferType() == "song", 0x169);
    for (std::vector<StoreOffer *>::iterator it = mOffers->begin();
         it != mOffers->end();
         ++it) {
        StoreOffer *cand = *it;
        if (cand->OfferType() == album && cand->HasSong(song)) {
            return cand;
        }
    }
    if (mPacks) {
        for (std::vector<StoreOffer *>::iterator it = mPacks->begin();
             it != mPacks->end();
             ++it) {
            StoreOffer *cand = *it;
            if (cand->OfferType() == album && cand->HasSong(song)) {
                return cand;
            }
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
        Element *prev = new Element(NULL, store_previous_chunk, true, false, true);
        mElements.push_back(prev);
        mElements.back()->mShortcut = store_previous_chunk;
        _ref0->Insert(_ref0->Size(), DataNode(store_previous_chunk));
    }
    if (grouping) {
        Symbol sortName = grouping->Sym(0);
        DataArray *groupArr = grouping->FindArray(browser_group, true);
        DataArray *subgroupArr = grouping->FindArray(browser_subgroup, false);
        DataArray *shortcutArr = grouping->FindArray(shortcut_group, false);
        DataArray *locArr = grouping->FindArray(localize_heading, true);
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
        Element *next = new Element(NULL, store_next_chunk, true, false, true);
        mElements.push_back(next);
        mElements.back()->mShortcut = store_next_chunk;
        _ref0->Insert(_ref0->Size(), DataNode(store_next_chunk));
    }
}

void StoreOfferProvider::ClearList() {
    Element **start = &mElements[0];
    Element **end = start + mElements.size();
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
    if (sym == show_browser_purchased) {
        // DEFERRED: retail purchase-state access (see Text()); stub keeps green.
        StoreOffer *offer = dynamic_cast<StoreOffer *>(_msg->GetObj(2));
        return DataNode(offer->IsPurchased());
    }
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
