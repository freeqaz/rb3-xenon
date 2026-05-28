#include "meta_band/SongSortBySong.h"
#include "SongSortNode.h"
#include "StoreSongSortNode.h"
#include "meta/Sorting.h"
#include "meta/StoreOffer.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "os/System.h"
#include "utl/MemMgr.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"

int SongCmp::Compare(const SongSortCmp *s, SongNodeType nodeType) const {
    SongCmp *cmp = (SongCmp *)s;
    switch (nodeType) {
    case kNodeShortcut:
    case kNodeHeader:
        return mHeaderSym == cmp->mHeaderSym
            ? 0
            : strcmp(mHeaderSym.Str(), cmp->mHeaderSym.Str());
    case kNodeSong:
    case kNodeStoreSong:
        return AlphaKeyStrCmp(mName, cmp->mName, true);
    default:
        MILO_FAIL("invalid type of node comparison.\n");
        return 0;
    }
}

void SongSortBySong::Init() {
    DataArray *cfg = SystemConfig(song_select);
    DataArray *alphas = cfg->FindArray(alpha_shortcuts);
    for (int i = 1; i < alphas->Size(); i++) {
        MemDoTempAllocations m(true, false);
        Symbol curSym = alphas->Sym(i);
        SongCmp *cmp = new SongCmp(gNullStr, curSym);
        mTree.push_back(new ShortcutNode(cmp, curSym, false));
    }
}

OwnedSongSortNode *SongSortBySong::NewSongNode(SongRecord *record) const {
    MemDoTempAllocations m(true, false);
    const char *title = record->Data()->Title();
    Symbol firstChar = NodeSort::FirstChar(title, true);
    SongCmp *cmp = new SongCmp(title, firstChar);
    OwnedSongSortNode *node = new OwnedSongSortNode(cmp, record);
    return node;
}

StoreSongSortNode *SongSortBySong::NewSongNode(StoreOffer *offer) const {
    MemDoTempAllocations m(true, false);
    const char *name = offer->OfferName();
    Symbol firstChar = NodeSort::FirstChar(name, true);
    SongCmp *cmp = new SongCmp(name, firstChar);
    StoreSongSortNode *node = new StoreSongSortNode(cmp, offer);
    return node;
}

ShortcutNode *SongSortBySong::NewShortcutNode(SongSortNode *node) const {
    MemDoTempAllocations m(true, false);
    const char *title = node->GetTitle();
    Symbol firstChar = NodeSort::FirstChar(title, true);
    SongCmp *cmp = new SongCmp(gNullStr, firstChar);
    ShortcutNode *newNode = new ShortcutNode(cmp, firstChar, false);
    return newNode;
}

HeaderSortNode *SongSortBySong::NewHeaderNode(SongSortNode *node) const {
    MemDoTempAllocations m(true, false);
    const char *title = node->GetTitle();
    Symbol firstChar = NodeSort::FirstChar(title, true);
    SongCmp *cmp = new SongCmp(gNullStr, firstChar);
    HeaderSortNode *newNode = new HeaderSortNode(cmp, firstChar, false);
    return newNode;
}