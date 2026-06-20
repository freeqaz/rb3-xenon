#pragma once
#include "obj/Object.h"
#include "obj/Msg.h"
#include "utl/Str.h"
#include <list>
#include <map>
#include <utility>
#include <vector>

// Minimal slice of the rb3-Wii meta/StorePackedMetadata.h, declaring only the
// pieces StoreMainPanel.cpp depends on, with ABI-correct offsets. The full
// header pulls in the complete StoreOffer.h packed-offer type family, which our
// tree only carries in trimmed form; widening it would ripple across the other
// pinned Store TUs (header edits are the #1 cross-TU regression source). Keep
// this minimal and offset-faithful.

class StoreMarqueeTable {
public:
    ~StoreMarqueeTable();
    bool Load(const char *);

    char *mBuffer;     // 0x0
    char *mMarquees;   // 0x4
    int mNumMarquees;  // 0x8
};

class StoreSingleStringTable {
public:
    ~StoreSingleStringTable() {
        if (mBuffer)
            _MemFree(mBuffer);
    }
    bool LoadFile(const char *);
    const char *GetString(int idx) const {
        if (idx < 0 || idx >= mNumStrings)
            return "STRING INDEX OUT OF BOUNDS";
        else
            return mStrings[idx];
    }

    int mNumStrings;  // 0x0
    char *mBuffer;    // 0x4
    char **mStrings;  // 0x8
};

class StoreStringTable {
public:
    ~StoreStringTable() {}
    bool Load(const char *);
    bool IsValid(int);

    StoreSingleStringTable mNonLocalized;  // 0x0
    StoreSingleStringTable mLocalized;     // 0xc
};

class StoreMetadataManager : public Hmx::Object {
public:
    ~StoreMetadataManager();
    virtual DataNode Handle(DataArray *, bool);

    const char *GetString(int idx) const {
        StoreStringTable *table = mStringTable;
        if (idx & 0x8000)
            return table->mLocalized.GetString((idx & 0x7FFF) - 1);
        else
            return table->mNonLocalized.GetString(idx - 1);
    }

    unsigned int mFlags;                // 0x1c
    int mLoadingState;                  // 0x20
    int mContentSize;                   // 0x24
    String mBasePath;                   // 0x28
    void *mVersion;                     // 0x34
    StoreStringTable *mStringTable;     // 0x38
    void *mSongTable;                   // 0x3c
    void *mOfferTable;                  // 0x40
    void *mRbnOfferTable;               // 0x44
    void *mPageTable;                   // 0x48
    void *mCurrentPage;                 // 0x4c
    StoreMarqueeTable *mMarqueeTable;   // 0x50
    void *mRedemptionsTable;            // 0x54
    std::map<unsigned long long, void *> unk58;  // 0x58
    int unk70;
    int unk74;
    int unk78;
    int unk7c;
    int unk80;
    int mErrorMsg;                      // 0x84
    unsigned long long unk88;           // 0x88
    unsigned short unk90;               // 0x90
    int unk94;
    std::list<std::pair<unsigned long long, unsigned short> > unk98;
    int unka0;
};

extern StoreMetadataManager TheStoreMetadata;
