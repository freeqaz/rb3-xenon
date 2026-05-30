#pragma once
#include "SongMgr.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/DateTime.h"
#include "stl/_vector.h"
#include "types.h"
#include "utl/Symbol.h"
#include "xdk/win_types.h"

class StorePurchaseable : public Hmx::Object {
public:
    StorePurchaseable();
    bool Exists() const;
    static unsigned long long OfferStringToID(char const *);
    char const *CostStr() const;
    bool IsAvailable() const { return isAvailable; }
    bool IsPurchased() const { return isPurchased; }
    unsigned long long SongID() const { return songID; }

    bool isAvailable; // 0x2c
    bool isPurchased; // 0x2d
    unsigned long long songID; // 0x30
    int cost; // 0x38
};

class StoreOffer : public StorePurchaseable {
public:
    // Hmx::Object
    virtual ~StoreOffer();
    virtual DataNode Handle(DataArray *, bool);
    virtual bool Cmp(StoreOffer const &, Symbol) const = 0;

    Symbol OfferType() const {
        static Symbol type("type");
        Symbol s = type;
        return mStoreOfferData->FindArray(s)->Sym(1);
    }
    bool HasData(Symbol) const;
    DateTime const &ReleaseDate() const;
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
    Symbol ShortName() const;
    bool IsCover() const;
    float PartRank(Symbol) const;
    DataNode GetData(DataArray const *, bool) const;
    bool HasSong(StoreOffer const *) const;
    DataNode OnGetData(DataArray *);
    StoreOffer(DataArray *, SongMgr *);

    DataArray *StoreOfferData() const { return mStoreOfferData; }

protected:
    // RB3-360 retail keeps the nested album/pack purchaseables (DC3 dropped
    // them). Each StorePurchaseable is 0x40 bytes, so they push mStoreOfferData
    // from 0x40 to 0xc0 — verified against the retail binary (?HasData@,
    // ?PackFirstLetter@ load mStoreOfferData from this+0xc0).
    StorePurchaseable mAlbum; // 0x40
    StorePurchaseable mPack; // 0x80
    DataArray *mStoreOfferData; // 0xc0
    DateTime date; // 0xc4
    SongMgr *mSongMgr; // 0xcc
    std::vector<int> mSongsInOffer; // 0xd0
};

class SortCmp {
public:
    SortCmp(Symbol sortBy) : mSortBy(sortBy) {}
    bool operator()(const StoreOffer *offer1, const StoreOffer *offer2) const {
        return offer1->Cmp(*offer2, mSortBy);
    }

    Symbol mSortBy;
};
