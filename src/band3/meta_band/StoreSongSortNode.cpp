#include "StoreSongSortNode.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/SongSortNode.h"
#include "obj/Data.h"
#include "obj/ObjMacros.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"

StoreSongSortNode::StoreSongSortNode(SongSortCmp *cmp, StoreOffer *off)
    : SongSortNode(cmp) {
    mOffer = off;
    mToken = off->ShortName();
}

StoreSongSortNode::~StoreSongSortNode() {}

bool StoreSongSortNode::IsEnabled() const { return IsActive(); }

const char *StoreSongSortNode::GetAlbumArtPath() {
    return "ui/image/song_select_random_keep.png";
}

const char *StoreSongSortNode::GetTitle() const { return mOffer->OfferName(); }

const char *StoreSongSortNode::GetArtist() const {
    static Symbol artist("artist");
    return mOffer->GetData(DataArrayPtr(artist), false).Str();
}

bool StoreSongSortNode::GetIsCover() const {
    static Symbol cover("cover");
    bool ret = mOffer->HasData(cover) && mOffer->GetData(DataArrayPtr(cover), false).Int();
    return ret;
}

const char *StoreSongSortNode::GetAlbum() const {
    static Symbol album_name("album_name");
    return mOffer->GetData(DataArrayPtr(album_name), false).Str();
}

int StoreSongSortNode::GetTotalMs() const { return 0; }

int StoreSongSortNode::GetTier(Symbol sym) const {
    static Symbol rank("rank");
    float f1 = mOffer->PartRank(sym);
    return TheSongMgr.RankTier(f1, sym);
}

SongNodeType StoreSongSortNode::GetType() const { return kNodeStoreSong; }

Symbol StoreSongSortNode::GetToken() const { return mToken; }

BEGIN_HANDLERS(StoreSongSortNode)
    HANDLE_EXPR(id, (int)mOffer->GetSingleSongID())
    HANDLE_EXPR(get_offer, mOffer)
    HANDLE_MEMBER_PTR(mOffer)
    HANDLE_SUPERCLASS(SongSortNode)
    HANDLE_CHECK(76)
END_HANDLERS
