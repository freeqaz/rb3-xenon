#include "meta_band/SongSortByRecent.h"
#include "SongSortByRecent.h"
#include "SongSortNode.h"
#include "meta/Sorting.h"
#include "meta_band/BandSongMetadata.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/StoreSongSortNode.h"
#include "os/Debug.h"
#include "utl/MemMgr.h"
#include "utl/Symbol.h"
#include "utl/Symbols4.h"

RecentCmp::RecentCmp(int pos, const char *name, Symbol s, bool b)
    : mRecentPos(pos), mSongName(name) {
    RecentType ty = OriginToRecentType(s);
    if (ty != 9)
        mType = ty;
    else if (b)
        mType = (RecentType)8;
    else if (mRecentPos < 0)
        mType = (RecentType)1;
    else
        mType = (RecentType)0;
}

RecentCmp::RecentType RecentCmp::OriginToRecentType(Symbol origin) {
    static Symbol rb3("rb3");
    static Symbol pearljam("pearljam");
    static Symbol greenday("greenday");
    static Symbol lego("lego");
    static Symbol rb2("rb2");
    static Symbol rb1("rb1");
    if (origin == rb3)
        return kDisc;
    else if (origin == pearljam)
        return kPearljam;
    else if (origin == greenday)
        return kGreenday;
    else if (origin == lego)
        return kLego;
    else if (origin == rb2)
        return kRB2;
    else if (origin == rb1)
        return kRB1;
    else
        return kDontHave;
}

Symbol RecentCmp::RecentTypeToOrigin(RecentCmp::RecentType ty) {
    static Symbol rb3("rb3");
    static Symbol pearljam("pearljam");
    static Symbol greenday("greenday");
    static Symbol lego("lego");
    static Symbol rb2("rb2");
    static Symbol rb1("rb1");
    switch (ty) {
    case kDisc:
        return rb3;
    case kPearljam:
        return pearljam;
    case kGreenday:
        return greenday;
    case kLego:
        return lego;
    case kRB2:
        return rb2;
    case kRB1:
        return rb1;
    default:
        return gNullStr;
    }
}

int RecentCmp::Compare(const SongSortCmp *s, SongNodeType nodeType) const {
    RecentCmp *cmp = (RecentCmp *)s;
    switch (nodeType) {
    case kNodeShortcut:
    case kNodeHeader:
        return mType - cmp->mType;
    case kNodeSong:
    case kNodeStoreSong:
        if (mType == cmp->mType) {
            if (mType == 0) {
                if (cmp->mRecentPos != mRecentPos) {
                    return cmp->mRecentPos - mRecentPos;
                }
            }
            return AlphaKeyStrCmp(mSongName, cmp->mSongName, true);
        }
        return mType - cmp->mType;
    default:
        MILO_FAIL("invalid type of node comparison.\n");
        return 0;
    }
}

OwnedSongSortNode *SongSortByRecent::NewSongNode(SongRecord *record) const {
    MemDoTempAllocations m;
    const BandSongMetadata *data = record->Data();
    int id = data->ID();
    int pos = TheSongMgr.GetPosInRecentList(id);
    const char *title = data->Title();
    RecentCmp *cmp = new RecentCmp(pos, title, data->GameOrigin(), false);
    OwnedSongSortNode *node = new OwnedSongSortNode(cmp, record);
    return node;
}

StoreSongSortNode *SongSortByRecent::NewSongNode(StoreOffer *offer) const {
    MemDoTempAllocations m;
    const char *name = offer->OfferName();
    RecentCmp *cmp = new RecentCmp(-1, name, gNullStr, true);
    StoreSongSortNode *node = new StoreSongSortNode(cmp, offer);
    return node;
}

ShortcutNode *SongSortByRecent::NewShortcutNode(SongSortNode *node) const {
    MemDoTempAllocations m;
    RecentCmp *other = (RecentCmp *)node->Cmp();
    RecentCmp::RecentType ty = other->mType;
    int pos = ty ? -1 : 0;
    bool tyIs8 = ty == 8;
    Symbol token = other->RecentTypeToOrigin(ty);
    RecentCmp *cmp = new RecentCmp(pos, nullptr, token, tyIs8);
    static Symbol recently_acquired("recently_acquired");
    static Symbol previously_acquired("previously_acquired");
    static Symbol acquired_from_discs("acquired_from_discs");
    static Symbol not_yet_acquired("not_yet_acquired");
    Symbol tok;
    switch (ty) {
    case RecentCmp::kRecent:
        tok = recently_acquired;
        break;
    case RecentCmp::kPrevious:
        tok = previously_acquired;
        break;
    case RecentCmp::kNotYet:
        tok = not_yet_acquired;
        break;
    default:
        tok = token;
        break;
    }
    ShortcutNode *newNode = new ShortcutNode(cmp, tok, true);
    return newNode;
}

HeaderSortNode *SongSortByRecent::NewHeaderNode(SongSortNode *node) const {
    MemDoTempAllocations m;
    RecentCmp *other = (RecentCmp *)node->Cmp();
    RecentCmp::RecentType ty = other->mType;
    int pos = ty ? -1 : 0;
    bool tyIs8 = ty == 8;
    Symbol token = other->RecentTypeToOrigin(ty);
    RecentCmp *cmp = new RecentCmp(pos, nullptr, token, tyIs8);
    static Symbol recently_acquired("recently_acquired");
    static Symbol previously_acquired("previously_acquired");
    static Symbol acquired_from_discs("acquired_from_discs");
    static Symbol not_yet_acquired("not_yet_acquired");
    Symbol tok;
    switch (ty) {
    case RecentCmp::kRecent:
        tok = recently_acquired;
        break;
    case RecentCmp::kPrevious:
        tok = previously_acquired;
        break;
    case RecentCmp::kNotYet:
        tok = not_yet_acquired;
        break;
    default:
        tok = token;
        break;
    }
    HeaderSortNode *newNode = new HeaderSortNode(cmp, tok, true);
    return newNode;
}