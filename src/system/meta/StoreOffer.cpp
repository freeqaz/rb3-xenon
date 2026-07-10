#include "meta/StoreOffer.h"
#include "SongMgr.h"
#include "macros.h"
#include "meta/Sorting.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/DateTime.h"
#include "os/Debug.h"
#include "os/System.h"
#include "stl/_iterator.h"
#include "stl/_iterator_base.h"
#include "stl/_vector.h"
#include "types.h"
#include "utl/Locale.h"
#include "utl/MakeString.h"
#include "utl/Std.h"
#include "utl/Symbol.h"
#include "xdk/win_types.h"
#include <cstring>
#include <stdlib.h>

StorePurchaseable::StorePurchaseable()
    : isAvailable(0), isPurchased(0), cost(0), songID(0) {}

bool StorePurchaseable::Exists() const { return (songID != 0) ? true : false; }

// Retail 360 (fn_82781568): hand-rolled uppercase-hex parse, not a
// _strtoui64 CRT call.
unsigned long long StorePurchaseable::OfferStringToID(char const *s) {
    unsigned long long id = 0;
    char c = *s;
    while (c != '\0') {
        int digit = c > '9' ? c - 'A' + 10 : c - '0';
        s++;
        id = id * 16 + digit;
        c = *s;
    }
    return id;
}

char const *StorePurchaseable::CostStr() const { return MakeString("%i -", cost); }

// Retail body reconstructed from Ghidra fn_82783368: retail also parses
// album_id/pack_id into the nested purchaseables, formats the localized
// release-date string into mReleaseDateStr (the Wii branch's DateTime member
// is only a ctor local here), and dropped the avatar-offer notify exemption.
StoreOffer::StoreOffer(DataArray *a, SongMgr *mgr) : mStoreOfferData(a), mSongMgr(mgr) {
    static Symbol id("id");
    static Symbol album_id("album_id");
    static Symbol pack_id("pack_id");
    static Symbol release_date("release_date");

    const char *str;
    if (mStoreOfferData->FindData(id, str, false)) {
        songID = OfferStringToID(str);
    }
    if (mStoreOfferData->FindData(album_id, str, false)) {
        mAlbum.songID = OfferStringToID(str);
    }
    if (mStoreOfferData->FindData(pack_id, str, false)) {
        mPack.songID = OfferStringToID(str);
    }

    DataArray *dateArray = mStoreOfferData->FindArray(release_date, false);
    if (dateArray) {
        DateTime dt(dateArray->Int(1), dateArray->Int(2), dateArray->Int(3), 0, 0, 0);
        static Symbol store_release_date_format("store_release_date_format");
        mReleaseDateStr = Localize(store_release_date_format, nullptr);
        dt.Format(mReleaseDateStr);
    }

    static Symbol song_ids("song_ids");
    DataArray *idArray = mStoreOfferData->FindArray(song_ids, false);
    if (idArray) {
        for (int i = 1; i < idArray->Size(); i++) {
            mSongsInOffer.push_back(idArray->Int(i));
        }
    }

    if (mSongsInOffer.empty()) {
        MILO_NOTIFY("%s does not have song_ids", OfferName());
    }
    mStoreOfferData->AddRef();
}

StoreOffer::~StoreOffer() { mStoreOfferData->Release(); }

bool StoreOffer::HasData(Symbol s) const {
    return mStoreOfferData->FindArray(s, false) != nullptr;
}

Symbol StoreOffer::FirstChar(Symbol s, bool b) const {
    return FirstSortChar(mStoreOfferData->FindStr(s), b);
}

Symbol StoreOffer::PackFirstLetter() const {
    static Symbol pack("pack");
    static Symbol name("name");

    if (OfferType() == pack) {
        return FirstSortChar(mStoreOfferData->FindStr(name), 1);
    } else
        return gNullStr;
}

char const *StoreOffer::OfferName() const {
    static Symbol name("name");
    // Retail 360 (fn_827818F8) lazily constructs a second, otherwise-unused
    // static Symbol "song" here (guard bit 2 of the shared guard word).
    static Symbol song("song");
    return mStoreOfferData->FindStr(name);
}

char const *StoreOffer::ArtistName() const {
    static Symbol artist("artist");
    return mStoreOfferData->FindStr(artist);
}
char const *StoreOffer::AlbumName() const {
    static Symbol album_name("album_name");
    return mStoreOfferData->FindStr(album_name);
}
char const *StoreOffer::Description() const {
    static Symbol description("description");
    return mStoreOfferData->FindStr(description);
}

bool StoreOffer::IsNewRelease() const {
    static Symbol new_release("new_release");
    DataArray *r = mStoreOfferData->FindArray(new_release, false);
    if (r != 0) {
        return r->Int(1) != 0;
    }
    return false;
}

bool StoreOffer::IsTest() const {
    static Symbol test("test");
    DataArray *t = mStoreOfferData->FindArray(test, false);
    if (t != nullptr) {
        return t->Int(1);
    }

    return false;
}

// Retail-only accessors below (absent from the rb3-Wii dev snapshot, which
// rewrote StoreOffer around StorePackedOffer; retail 360 kept the DataArray
// form). Bodies reconstructed from the retail binary (Ghidra 0x82781860,
// 0x82781D08, 0x82781D78, 0x827822E0, 0x82782448, 0x827825C0, 0x82782130).

int StoreOffer::YearReleased() const {
    static Symbol year_released("year_released");
    Symbol s = year_released;
    return mStoreOfferData->FindArray(s, true)->Int(1);
}

Symbol StoreOffer::Decade() const {
    int year = YearReleased();
    return Symbol(MakeString("the%is", year - year % 10));
}

Symbol StoreOffer::LengthSym() const {
    static Symbol song_length_short("song_length_short");
    return song_length_short;
}

int StoreOffer::NumSongs() const {
    MILO_ASSERT(OfferType() == "pack" || OfferType() == "album", 0x100);
    return mSongsInOffer.size();
}

int StoreOffer::Song(int i) const {
    MILO_ASSERT(mSongsInOffer.size() > i, 0x106);
    return mSongsInOffer[i];
}

bool StoreOffer::ValidTitle() const {
    static Symbol titles("titles");
    DataArray *titleArray = mStoreOfferData->FindArray(titles, false);
    if (titleArray != nullptr) {
        for (int i = 1; i < titleArray->Size(); i++) {
            if (SystemTitles()->FindArray(titleArray->Sym(0), false)) {
                return true;
            }
        }
        return false;
    }
    return true;
}

bool StoreOffer::InLibrary() const {
    // Retail (fn_82781DF0) computes the "any songs at all" bool up front and
    // funnels the failure path through the common return.
    bool ret = mSongsInOffer.size() > 0;
    for (std::vector<int>::const_iterator it = mSongsInOffer.begin();
         it != mSongsInOffer.end();
         ++it) {
        int i = *it;
        if (mSongMgr == nullptr || !mSongMgr->HasSong(i)) {
            ret = false;
            break;
        }
    }
    return ret;
}

bool StoreOffer::PartiallyInLibrary() const {
    for (std::vector<int>::const_iterator it = mSongsInOffer.begin();
         it != mSongsInOffer.end();
         ++it) {
        int i = *it;
        if (mSongMgr != nullptr && mSongMgr->HasSong(i))
            return true;
    }
    return false;
}

// Retail (fn_82781E68) dropped the Wii dev build's MILO_NOTIFY diagnostics.
int StoreOffer::GetSingleSongID() const {
    const std::vector<int> &v = mSongsInOffer;
    return v.empty() ? 0 : v.front();
}

DataNode StoreOffer::GetData(DataArray const *data, bool b) const {
    DataArray *storeData = mStoreOfferData;
    for (int i = 0; i < data->Size(); i++) {
        storeData = storeData->FindArray(data->Sym(i));
    }
    return b ? storeData : storeData->Node(1);
}

bool StoreOffer::HasSong(StoreOffer const *c) const {
    MILO_ASSERT(OfferType() == "pack" || OfferType() == "album",0x10c);
    MILO_ASSERT(c->OfferType() == "song", 0x10d);
    // Retail (fn_82782068) reads the other offer's song id through a hoisted
    // &c->mSongsInOffer with a ternary, not a GetSingleSongID call.
    for (int i = 0; i < NumSongs(); i++) {
        const std::vector<int> &v = c->mSongsInOffer;
        int songId = Song(i);
        if (songId == (v.empty() ? 0 : v.front())) {
            return true;
        }
    }

    return false;
}

float StoreOffer::PartRank(Symbol part) const {
    static Symbol rank("rank");
    return GetData(DataArrayPtr(rank, part), false).Float();
}

Symbol StoreOffer::Genre() const {
    static Symbol genre("genre");
    return GetData(DataArrayPtr(genre), false).Sym();
}

Symbol StoreOffer::RatingSym() const {
    static Symbol rating("rating");
    return Symbol(MakeString("rating_%i", GetData(DataArrayPtr(rating), false).Int()));
}

Symbol StoreOffer::VocalPartsSym() const {
    static Symbol vocal_parts("vocal_parts");
    return Symbol(
        MakeString("vocal_parts_%i", GetData(DataArrayPtr(vocal_parts), false).Int())
    );
}

DataNode StoreOffer::OnGetData(DataArray *d) {
    DataArray *array = d->Array(2);
    int x;
    if (3 < d->Size()) {
        x = d->Int(3);
    } else {
        x = 0;
    }
    return GetData(array, x != 0);
}

// Retail handler list reconstructed from Ghidra fn_827827B0 (drops the Wii
// branch's artist/album_name/partially_in_library handlers; adds
// year_released, genre, has_available_pack/album, is_completely_unavailable
// and release_date_str; is_test moved to the tail).
BEGIN_HANDLERS(StoreOffer)
    HANDLE_EXPR(short_name, mStoreOfferData->Sym(0))
    HANDLE_EXPR(offer_type, OfferType())
    HANDLE_EXPR(offer_name, OfferName())
    HANDLE_EXPR(description, Description())
    HANDLE_EXPR(is_new_release, IsNewRelease())
    HANDLE_EXPR(year_released, YearReleased())
    HANDLE_EXPR(genre, Genre())
    HANDLE_EXPR(cost_str, MakeString("%i -", cost))
    HANDLE_EXPR(in_library, InLibrary())
    HANDLE_EXPR(is_available, isAvailable)
    HANDLE_EXPR(is_purchased, isPurchased)
    HANDLE_EXPR(has_available_pack, mPack.isAvailable)
    HANDLE_EXPR(has_available_album, mAlbum.isAvailable)
    HANDLE_EXPR(is_completely_unavailable, IsCompletelyUnavailable())
    HANDLE_EXPR(is_test, IsTest())
    HANDLE(get_data, OnGetData)
    HANDLE_EXPR(has_data, HasData(_msg->Sym(2)))
    HANDLE_EXPR(first_char, FirstChar(_msg->Sym(2), _msg->Int(3)))
    HANDLE_EXPR(pack_first_letter, PackFirstLetter())
    HANDLE_EXPR(release_date_str, mReleaseDateStr.c_str())
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS
