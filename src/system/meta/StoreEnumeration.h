#pragma once
#include "stl/_vector.h"
#include "types.h"
#include "utl/Str.h"
#include "xdk/xapilibi/xbase.h"
#include "xdk/xapilibi/stringapiset.h"
#include <list>

enum StoreError {
    kStoreErrorSuccess = 0,
    kStoreErrorNoContent = 1,
    kStoreErrorCacheNoSpace = 2,
    kStoreErrorCacheRemoved = 3,
    kStoreErrorLiveServer = 4,
    kStoreErrorStoreServer = 5,
    kStoreErrorSignedOut = 6,
    kStoreErrorNoMetadata = 7,
    kStoreErrorEcommerce = 8,
    kStoreErrorNoEula = 9
};

struct EnumProduct {
    String mName;       // 0x00
    u64 mOfferID;       // 0x08
    int mPurchased;     // 0x10
    int mPrice;         // 0x14
};

class StoreEnumeration {
public:
    enum State {
        kEnumWaiting = 0,
        kEnumProcessing = 1,
        kPreSuccess = 2,
        kPreFail = 3,
        kSuccess = 4,
        kFail = 5,
    };
    StoreEnumeration() {}
    virtual ~StoreEnumeration() {}
    virtual void Start() = 0;
    virtual bool IsEnumerating() const = 0;
    virtual bool IsSuccess() const = 0;
    virtual void Poll() = 0;

    std::list<EnumProduct> mContentList;
};

class XboxEnumeration : public StoreEnumeration {
public:
    // StoreEnumeration
    virtual ~XboxEnumeration();
    virtual void Start();
    virtual bool IsEnumerating() const;
    virtual bool IsSuccess() const;
    virtual void Poll();

    XboxEnumeration(int, std::vector<unsigned long long> *);

protected:
    u32 mOfferIDCount;                      // 0xC - total count of offer IDs
    unsigned long long *mOfferIDsBegin;              // 0x10 - begin pointer of offer IDs array
    unsigned long long *mCurOffers;         // 0x14 - current position pointer in offer IDs array
    int mUserIndex;                              // 0x18 - user index
    bool mEnumerating;                             // 0x1c - enumerating flag
    XOVERLAPPED mOverlapped;                // 0x20 - Xbox overlapped I/O structure (28 bytes)
    HANDLE mHandle;                         // 0x3C - enumeration handle
    u32 mBufferSize;                              // 0x40 - buffer size
    void *mEnumBuffer;                      // 0x44 - buffer for enumeration results
};
