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
    // Retail vtable slot +0x54, and the LAST slot: retail's `??_7StoreOffer@@6B@`
    // (`0x82112d44`) is 22 words = Hmx::Object's 21 + this one, and the words
    // after it are an EH state map (`0`, `0xffffffff`, then (VA,state) pairs),
    // so the table demonstrably ends here.  Name coverage on it is 22/22.
    // Defined in StoreOffer.cpp (retail fn_827A64B8) -- NOT pure; retail emits
    // a real body here and BandStoreOffer overrides it (`0x8266e4e8`, which
    // `bl`s straight into `0x827a64b8`).
    // ★ Adjudicated ARGUMENT-wise, so it does not rest on the map name: the
    // body at `0x827a64b8` touches ONLY r3 -- it reads the bools at 0x28
    // (= StorePurchaseable::isAvailable, compiler-verified), 0xa8 and 0x68 and
    // returns.  r4/r5 are never read, so this slot cannot hold a two-argument
    // `Cmp(const StoreOffer&, Symbol)`.
    virtual bool IsCompletelyUnavailable() const;
    // ⛔ DO NOT RE-ADD `virtual bool Cmp(StoreOffer const&, Symbol) const = 0;`
    // (nor the `SortCmp` functor that used to sit at the bottom of this
    // header).  Both are DC3-lineage additions that RB3 PREDATES -- the same
    // shape as MoggClip's `PlayableSample` block
    // (VTABLE_SLOT_COUNT_FIXES_2026-08-20.md sec 15b).  Removed by lane VT-SIG
    // on four converging lines:
    //   1. retail's table has no slot for it (22 vs our 23; see above);
    //   2. the rb3-Wii oracle -- the RIGHT oracle for this file, and a DEV
    //      build that retains MORE than retail -- declares neither `Cmp` nor
    //      `SortCmp` on `StoreOffer` OR `BandStoreOffer`.  Checked for vacuity:
    //      that header is 253 lines and does declare `StoreOffer`;
    //   3. dc3-decomp (NEWER than RB3) has both, verbatim what we inherited;
    //   4. `Cmp` was declared pure here, "overridden" by BandStoreOffer, and
    //      NEVER DEFINED anywhere -- invisible only because the match build
    //      compiles to .obj and never links.  Its sole caller was
    //      `SortCmp::operator()`, and `SortCmp` was never instantiated.
    // Retail's map also carries ~40 named StoreOffer methods and no `Cmp`, and
    // instantiates `__find` over `StoreOffer**` but no sort-family algorithm.

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

// ⛔ `class SortCmp` was here and is REMOVED -- see the `Cmp` note inside
// StoreOffer above for the four converging lines of evidence.  It existed only
// to call `StoreOffer::Cmp`, was never instantiated anywhere in this tree, and
// its one appearance outside this header was the never-included
// `stlport/stl/_algo_special.c` -- a file byte-identical to dc3-decomp's,
// self-described as "Specialized implementations for DC3-specific scenarios",
// and dead in BOTH trees.  That file is removed in the same change, since its
// entire content was the `__unguarded_partition<StoreOffer**, StoreOffer*,
// SortCmp>` specialization.
