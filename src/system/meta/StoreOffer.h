#pragma once
#include "SongMgr.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/DateTime.h"
#include "stl/_vector.h"
#include "types.h"
#include "utl/Str.h"
#include "utl/Symbol.h"
#include "xdk/win_types.h"

class StorePurchaseable : public Hmx::Object {
public:
    StorePurchaseable();
    bool Exists() const;
    static unsigned long long OfferStringToID(char const *);
    // Inverse of OfferStringToID (retail fn_827A6500, "%016llX").
    static void IDToOfferString(unsigned long long, String &);
    char const *CostStr() const;
    bool IsAvailable() const { return isAvailable; }
    bool IsPurchased() const { return isPurchased; }
    unsigned long long SongID() const { return songID; }

    // Offsets verified with cl.exe /d1reportSingleClassLayoutStorePurchaseable
    // (lane BU-2); the previous 0x2c/0x2d comments were stale by 4 bytes.
    bool isAvailable; // 0x28
    bool isPurchased; // 0x29
    unsigned long long songID; // 0x30
    int cost; // 0x38
};

class StoreOffer : public StorePurchaseable {
public:
    // Hmx::Object
    virtual ~StoreOffer();
    virtual DataNode Handle(DataArray *, bool);
    // Retail vtable slot +0x54 (called by the is_completely_unavailable
    // handler, fn_827827B0) — declared before Cmp. Overridden by
    // BandStoreOffer.
    // Defined in StoreOffer.cpp (retail fn_827A64B8) -- NOT pure; retail emits
    // a real body here and BandStoreOffer overrides it.
    virtual bool IsCompletelyUnavailable() const;
    virtual bool Cmp(StoreOffer const &, Symbol) const = 0;

    Symbol OfferType() const {
        static Symbol type("type");
        Symbol s = type;
        return mStoreOfferData->FindArray(s)->Sym(1);
    }
    bool HasData(Symbol) const;
    Symbol FirstChar(Symbol, bool) const;
    Symbol PackFirstLetter() const;
    char const *OfferName() const;
    char const *ArtistName() const;
    char const *AlbumName() const;
    char const *Description() const;
    bool IsNewRelease() const;
    bool IsTest() const;
    int NumSongs() const;
    int Song(int) const;
    bool ValidTitle() const;
    bool InLibrary() const;
    bool PartiallyInLibrary() const;
    int GetSingleSongID() const;
    // RB3-specific methods (missing from DC3 base)
    char const *Artist() const;
    // Inline (retail evidence: ??0StoreSongSortNode inlines this as
    // mStoreOfferData->Sym(0) — DataNode::Sym called on node[0] directly).
    Symbol ShortName() const { return mStoreOfferData->Sym(0); }
    bool IsCover() const;
    bool HasArtist() const;
    float PartRank(Symbol) const;
    DataNode GetData(DataArray const *, bool) const;
    bool HasSong(StoreOffer const *) const;
    DataNode OnGetData(DataArray *);
    StoreOffer(DataArray *, SongMgr *);
    // RB3 song-filter accessors (used by SongSortMgr::DoesOfferMatchFilter).
    // Defined in StoreOffer.cpp — retail keeps them in this TU (0x82781860,
    // 0x82781D08, 0x82781D78, 0x827822E0, 0x82782448, 0x827825C0, 0x82782130).
    int YearReleased() const;
    Symbol Genre() const;
    Symbol Decade() const;
    Symbol LengthSym() const;
    Symbol RatingSym() const;
    Symbol VocalPartsSym() const;
    bool HasSolo() const;
    int Rating() const;
    bool IsRbn() const;

    DataArray *StoreOfferData() const { return mStoreOfferData; }

protected:
    // RB3-360 retail keeps the nested album/pack purchaseables (DC3 dropped
    // them). Each StorePurchaseable is 0x40 bytes, so they push mStoreOfferData
    // from 0x40 to 0xc0 — verified against the retail binary (?HasData@,
    // ?PackFirstLetter@ load mStoreOfferData from this+0xc0).
    StorePurchaseable mAlbum; // 0x40
    StorePurchaseable mPack; // 0x80
    DataArray *mStoreOfferData; // 0xc0
    // Retail evidence: the ctor (fn_82783368) default-constructs a String at
    // this+0xc4 and assigns it Localize(store_release_date_format) formatted
    // through DateTime::Format; the release_date_str handler (fn_827827B0)
    // returns the char* at this+0xcc (= String::mStr, vptr+mCap+mStr layout);
    // the dtor (fn_827831F8) runs ~String on this+0xc4. The Wii dev branch's
    // `DateTime date` member is a ctor local in retail.
    String mReleaseDateStr; // 0xc4
    SongMgr *mSongMgr; // 0xd0
    std::vector<int> mSongsInOffer; // 0xd4
};

// Retail fn_827A6548 (= rb3-Wii StoreOffer.cpp:182).
bool operator==(const StoreOffer *, Symbol);

class SortCmp {
public:
    SortCmp(Symbol sortBy) : mSortBy(sortBy) {}
    bool operator()(const StoreOffer *offer1, const StoreOffer *offer2) const {
        return offer1->Cmp(*offer2, mSortBy);
    }

    Symbol mSortBy;
};
