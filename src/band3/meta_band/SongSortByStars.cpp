#include "meta_band/SongSortByStars.h"
#include "SongSortNode.h"
#include "bandobj/StarDisplay.h"
#include "meta/Sorting.h"
#include "meta_band/MusicLibrary.h"
#include "meta_band/SongRecord.h"
#include "meta_band/StoreSongSortNode.h"
#include "os/Debug.h"
#include "ui/UIListCustom.h"
#include "ui/UIListLabel.h"
#include "ui/UILabel.h"
#include "utl/MemMgr.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"

// Retail 360 resolves star-count tokens through a free function (0x82309FA0)
// rather than StarDisplay::GetSymbolForStarCount (the Wii-dev shape).
Symbol GetStarsToken(int);

StarsCmp::StarsCmp(int stars, float rank, const char *name)
    : mStars(stars), mRank(rank), mName(name) {
    mHeaderSym = GetStarsToken(mStars);
    MILO_ASSERT(!mHeaderSym.Null(), 0x1D);
}

int StarsCmp::Compare(const SongSortCmp *s, SongNodeType nodeType) const {
    StarsCmp *cmp = (StarsCmp *)s;
    switch (nodeType) {
    case kNodeShortcut:
    case kNodeHeader:
        if (mStars == cmp->mStars)
            return 0;
        else
            return cmp->mStars - mStars > 0 ? 1 : -1;
    case kNodeSong:
    case kNodeStoreSong:
        if (mStars == cmp->mStars) {
            float mine = mRank;
            float other = cmp->mRank;
            if (mine == other) {
                return AlphaKeyStrCmp(mName, cmp->mName, false);
            } else if (other == 0)
                return -1;
            else if (mine == 0)
                return 1;
            else
                return mine - other > 0 ? 1 : -1;
        }
        return cmp->mStars - mStars > 0 ? 1 : -1;
    default:
        MILO_FAIL("invalid type of node comparison.\n");
        return 0;
    }
}

void SongSortByStars::Init() { unk3c = TheMusicLibrary->DifficultySortPart(); }

OwnedSongSortNode *SongSortByStars::NewSongNode(SongRecord *record) const {
    MemDoTempAllocations m;
    static Symbol guitar("guitar");
    static Symbol bass("bass");
    Symbol part = unk3c;
    float rank = record->Data()->Rank(part);
    if (unk3c == guitar && rank == 0) {
        rank = record->Data()->Rank(bass);
    }
    const char *title = record->Data()->Title();
    StarsCmp *cmp = new StarsCmp(record->GetStars(), rank, title);
    OwnedSongSortNode *node = new OwnedSongSortNode(cmp, record);
    return node;
}

StoreSongSortNode *SongSortByStars::NewSongNode(StoreOffer *offer) const {
    MemDoTempAllocations m;
    static Symbol guitar("guitar");
    static Symbol bass("bass");
    Symbol part = unk3c;
    float rank = offer->PartRank(part);
    if (unk3c == guitar && rank == 0) {
        rank = offer->PartRank(bass);
    }
    const char *name = offer->OfferName();
    StarsCmp *cmp = new StarsCmp(0, rank, name);
    StoreSongSortNode *node = new StoreSongSortNode(cmp, offer);
    return node;
}

ShortcutNode *SongSortByStars::NewShortcutNode(SongSortNode *node) const {
    MemDoTempAllocations m;
    int stars = 0;
    OwnedSongSortNode *owned = dynamic_cast<OwnedSongSortNode *>(node);
    if (owned) {
        stars = owned->GetSongRecord()->GetStars();
    }
    StarsCmp *cmp = new StarsCmp(stars, 0, "");
    ShortcutNode *newNode = new ShortcutNode(cmp, cmp->mHeaderSym, true);
    return newNode;
}

HeaderSortNode *SongSortByStars::NewHeaderNode(SongSortNode *node) const {
    MemDoTempAllocations m;
    int stars = 0;
    OwnedSongSortNode *owned = dynamic_cast<OwnedSongSortNode *>(node);
    if (owned) {
        stars = owned->GetSongRecord()->GetStars();
    }
    StarsCmp *cmp = new StarsCmp(stars, 0, "");
    HeaderSortNode *newNode = new HeaderSortNode(cmp, cmp->mHeaderSym, true);
    return newNode;
}

const char *SongSortByStars::TextForNode(
    ShortcutNode *node, UIListLabel *listLabel, UILabel *label
) const {
    label->SetTextToken(gNullStr);
    return (const char *)1;
}

bool SongSortByStars::CustomForNode(
    ShortcutNode *node, UIListCustom *custom, Hmx::Object *obj
) const {
    if (custom->Matches("stars")) {
        StarDisplay *sd = dynamic_cast<StarDisplay *>(obj);
        MILO_ASSERT(sd, 0x8E);
        sd->SetToToken(node->GetToken());
        sd->SetShowing(true);
        return true;
    }
    return false;
}
