#include "meta_band/SongSortByReview.h"
#include "SongSortNode.h"
#include "bandobj/ReviewDisplay.h"
#include "meta/Sorting.h"
#include "meta_band/SongRecord.h"
#include "meta_band/StoreSongSortNode.h"
#include "os/Debug.h"
#include "ui/UIListLabel.h"
#include "ui/UIListCustom.h"
#include "ui/UILabel.h"
#include "utl/MemMgr.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols4.h"

ReviewCmp::ReviewCmp(int review, const char *name) : mReview(review), mName(name) {
    mHeaderSym = ReviewDisplay::GetSymbolForReviewScore(mReview);
    MILO_ASSERT(!mHeaderSym.Null(), 0x19);
}

int ReviewCmp::Compare(const SongSortCmp *s, SongNodeType nodeType) const {
    ReviewCmp *cmp = (ReviewCmp *)s;
    switch (nodeType) {
    case kNodeShortcut:
    case kNodeHeader:
        if (mReview == cmp->mReview)
            return 0;
        else if (mReview == 1)
            return 1;
        else if (cmp->mReview == 1)
            return -1;
        else
            return cmp->mReview - mReview > 0 ? 1 : -1;
    case kNodeSong:
    case kNodeStoreSong:
        if (mReview == cmp->mReview) {
            return AlphaKeyStrCmp(mName, cmp->mName, true);
        } else if (mReview == 1)
            return 1;
        else if (cmp->mReview == 1)
            return -1;
        else
            return cmp->mReview - mReview > 0 ? 1 : -1;
    default:
        MILO_FAIL("invalid type of node comparison.\n");
        return 0;
    }
}

OwnedSongSortNode *SongSortByReview::NewSongNode(SongRecord *record) const {
    MemDoTempAllocations m;
    const char *title = record->Data()->Title();
    ReviewCmp *cmp = new ReviewCmp(record->mReview, title);
    OwnedSongSortNode *node = new OwnedSongSortNode(cmp, record);
    return node;
}

StoreSongSortNode *SongSortByReview::NewSongNode(StoreOffer *offer) const {
    MemDoTempAllocations m;
    const char *name = offer->OfferName();
    ReviewCmp *cmp = new ReviewCmp(0, name);
    StoreSongSortNode *node = new StoreSongSortNode(cmp, offer);
    return node;
}

ShortcutNode *SongSortByReview::NewShortcutNode(SongSortNode *node) const {
    MemDoTempAllocations m;
    int review = 0;
    OwnedSongSortNode *owned = dynamic_cast<OwnedSongSortNode *>(node);
    if (owned) {
        review = owned->GetSongRecord()->mReview;
    }
    ReviewCmp *cmp = new ReviewCmp(review, "");
    ShortcutNode *newNode = new ShortcutNode(cmp, cmp->mHeaderSym, true);
    return newNode;
}

HeaderSortNode *SongSortByReview::NewHeaderNode(SongSortNode *node) const {
    MemDoTempAllocations m;
    int review = 0;
    OwnedSongSortNode *owned = dynamic_cast<OwnedSongSortNode *>(node);
    if (owned) {
        review = owned->GetSongRecord()->mReview;
    }
    ReviewCmp *cmp = new ReviewCmp(review, "");
    HeaderSortNode *newNode = new HeaderSortNode(cmp, cmp->mHeaderSym, true);
    return newNode;
}

bool SongSortByReview::TextForNode(
    ShortcutNode *node, UIListLabel *listLabel, UILabel *label
) const {
    label->SetTextToken(gNullStr);
    return true;
}

bool SongSortByReview::CustomForNode(
    ShortcutNode *node, UIListCustom *custom, Hmx::Object *obj
) const {
    if (custom->Matches("review")) {
        ReviewDisplay *rd = dynamic_cast<ReviewDisplay *>(obj);
        MILO_ASSERT(rd, 0x7A);
        rd->SetToToken(node->GetToken());
        rd->SetShowing(true);
        return true;
    }
    return false;
}

// sw2 scatter-include (default/SongSortByReview <- meta_band/SongSortByPlays.cpp)
// RB3 retail scattered SongSortByPlays's NewSongNode/NewShortcutNode/NewHeaderNode
// COMDATs into SongSortByReview's .text span; compile them here so objdiff pairs them.
#define gRev gRev_SongSortByPlays
#define gAltRev gAltRev_SongSortByPlays
#include "meta_band/SongSortByPlays.cpp"
#undef gRev
#undef gAltRev
