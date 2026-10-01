#include "meta/StoreEnumeration.h"
#include "os/Debug.h"
#include "utl/MakeString.h"
#include "xdk/XAPILIB.h"
#include <cstring>


XboxEnumeration::XboxEnumeration(int i, std::vector<unsigned long long> *offerIDs)
    : mUserIndex(i), mOfferIDCount(0), mOfferIDsBegin(0), mCurOffers(0), mEnumerating(false), mHandle(0), mBufferSize(0), mEnumBuffer(0) {
    if (offerIDs != 0) {
        mOfferIDCount = (offerIDs->end() - offerIDs->begin());
        MILO_ASSERT(mOfferIDCount, 0x197);
        mOfferIDsBegin = new unsigned long long[mOfferIDCount];
        memcpy(mOfferIDsBegin, &(*offerIDs)[0], mOfferIDCount << 3);
        mCurOffers = mOfferIDsBegin;
    }
}


XboxEnumeration::~XboxEnumeration() {
    delete[] mOfferIDsBegin;
    mOfferIDsBegin = 0;

    if (mHandle != 0 && mOverlapped.InternalLow == 0x3E5U) {
        u32 result = XCancelOverlapped(&mOverlapped);
        if (result != 0) {
            MILO_FAIL("Error cancelling enum %d", result);
        }
    }

    if (mHandle != 0) {
        CloseHandle(mHandle);
        mHandle = 0;
    }

    delete mEnumBuffer;
    mEnumBuffer = 0;
}

bool XboxEnumeration::IsSuccess() const {
#ifdef HX_NATIVE
    // Use proper member access instead of hardcoded struct offsets
    if (mHandle != 0) {
        MILO_ASSERT(false, 0x208);
    }
    return (bool)mOverlapped.InternalHigh;
#else
    if (*((u32*)((u8*)this + 0x3c)) != 0) {
        MILO_ASSERT(false, 0x208);
    }
    return *((bool*)((u8*)this + 0x24));
#endif
}

void XboxEnumeration::Start() {
    mEnumerating = true;
    if (mHandle == 0) {
        unsigned int error;
        mBufferSize = 0;
        if (mCurOffers == mOfferIDsBegin) {
            mContentList.clear();
        }
        if (mOfferIDsBegin == 0) {
            error = XMarketplaceCreateOfferEnumerator(mUserIndex, 2, 0xFFFFFFFFFFFFFFFFULL, 99, &mBufferSize, &mHandle);
        } else {
            int remaining = (int)(mOfferIDCount - (u32)(mCurOffers - mOfferIDsBegin));
            if (remaining >= 99) remaining = 99;
            error = XMarketplaceCreateOfferEnumeratorByOffering(mUserIndex, remaining, mCurOffers, (WORD)remaining, &mBufferSize, &mHandle);
            mCurOffers += remaining;
        }
        MILO_ASSERT(!mEnumBuffer, 0x1EA);
        mEnumBuffer = new char[mBufferSize];
        if (error != 0) {
            goto error_path;
        }
    }
    memset(mEnumBuffer, 0, mBufferSize);
    memset(&mOverlapped, 0, 0x1c);
    {
        DWORD result = XEnumerate(mHandle, mEnumBuffer, mBufferSize, 0, &mOverlapped);
        if (result == 0x3e5) {
            return;
        }
    }
error_path:
    if (mHandle != 0) {
        CloseHandle(mHandle);
        mHandle = 0;
    }
    delete[] (char*)mEnumBuffer;
    mEnumerating = false;
    mEnumBuffer = 0;
}

bool XboxEnumeration::IsEnumerating() const {
    return mEnumerating;
}

void XboxEnumeration::Poll() {
    if (0 == mHandle || mOverlapped.InternalLow == 0x3E5U) {
        return;
    }

    DWORD count = 0;
    DWORD result = XGetOverlappedResult(&mOverlapped, &count, 0);
    DWORD i = 0;
    if (count != 0) {
        std::list<EnumProduct>::iterator it = mContentList.end();
        u32 offset = 0;
        for (; i < count; i++, offset += 0x68) {
            EnumProduct prod;
            char buf[256];
            u8 *entry = offset + (u8 *)mEnumBuffer;
            WideCharToMultiByte(
                0, 0, *(LPCWSTR *)(entry + 0x14), *(int *)(entry + 0x10), buf, 0xFF, 0, 0
            );
            prod.mName = buf;
            prod.mOfferID = *(u64 *)entry;
            prod.mPurchased = *(int *)(entry + 0x48);
            prod.mPrice = *(int *)(entry + 0x64);
            mContentList.insert(it, prod);
        }
    }

    // Without an offer-ID list a successful pass simply enumerates the next
    // page on the open handle; otherwise the handle and buffer are released.
    if (mOfferIDsBegin != 0 || result != 0) {
        if (mHandle != 0) {
            CloseHandle(mHandle);
            mHandle = 0;
        }
        delete mEnumBuffer;
        mEnumBuffer = 0;
        if (result != 0) {
            if (result != 0x12) { // ERROR_NO_MORE_FILES
                if (result != 0x65b) { // ERROR_FUNCTION_FAILED
                    XGetOverlappedExtendedError(&mOverlapped);
                } else {
                    WORD ext = XGetOverlappedExtendedError(&mOverlapped);
                    if (ext < 0x2710 || ext >= 0x2EE0) {
                        // not a winsock error: move on to the next offer, if any
                        if (mOfferIDsBegin == 0) {
                            return;
                        }
                        goto next_offer;
                    }
                }
            } else if (mOfferIDsBegin == 0) {
                return;
            }
            mEnumerating = false;
            return;
        }
    }
    if (mOfferIDsBegin != 0) {
    next_offer:
        if (mCurOffers >= mOfferIDsBegin + mOfferIDCount) {
            return;
        }
    }
    Start();
}

