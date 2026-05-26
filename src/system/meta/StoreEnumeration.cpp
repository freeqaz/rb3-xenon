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
        u32 allocSize = mOfferIDCount << 3;
        if (mOfferIDCount > 0x1FFFFFFFU) {
            allocSize = 0xFFFFFFFF;
        }
        mOfferIDsBegin = (unsigned long long *)new char[allocSize];
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
            error = XMarketplaceCreateOfferEnumerator(mUserIndex, 0x100002, 0xFFFFFFFFFFFFFFFFULL, 99, &mBufferSize, &mHandle);
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

    DWORD bytesReceived = 0;
    DWORD overlappedResult = XGetOverlappedResult(&mOverlapped, &bytesReceived, 0);

    DWORD productCount = 0;
    if (bytesReceived > 0) {
        std::list<EnumProduct>::iterator it = mContentList.end();
        u32 offset = 0;
        while (productCount < bytesReceived) {
            char buf[256];
            String str;
            u8 *entryPtr = (u8 *)mEnumBuffer + offset;
            WideCharToMultiByte(0, 0, (LPCWSTR)(entryPtr + 0x14), *(int *)(entryPtr + 0x10), buf, 0xFF, 0, 0);
            str = buf;

            EnumProduct prod;
            prod.mName = str;
            prod.mOfferID = *(u64 *)entryPtr;
            prod.mPurchased = *(int *)(entryPtr + 0x48);
            mContentList.insert(it, prod);
            prod.mPrice = *(int *)(entryPtr + 0x64);

            offset += 0x68;
            productCount++;
        }
    }

    if (mOfferIDsBegin == 0 && overlappedResult == 0 && bytesReceived >= 99) {
        goto continue_enum;
    }

    if (mHandle != 0) {
        CloseHandle(mHandle);
        mHandle = 0;
    }

    delete mEnumBuffer;
    mEnumBuffer = 0;

    if (overlappedResult == 0) {
        goto done;
    }

    if (overlappedResult == 0x12) {
        goto handle_12;
    }

    if (overlappedResult == 0x65b) {
        goto handle_65b;
    }

    XGetOverlappedExtendedError(&mOverlapped);
    goto check_more_offers;

handle_65b:
    {
        DWORD extError = XGetOverlappedExtendedError(&mOverlapped);
        TheDebug << MakeString(" store enum: overlapped failed with: %d, extended: %d (0x%X)", (unsigned long)overlappedResult, (unsigned long)extError, (unsigned long)extError);
    }
    goto check_more_offers;

handle_12:
    {
        DWORD extError = XGetOverlappedExtendedError(&mOverlapped);
        if ((WORD)extError == 0x12) {
            goto done;
        }
        TheDebug << MakeString(" store enum: funciton failed with: %d (0x%X)", (unsigned long)extError, (unsigned long)extError);
        if ((WORD)extError >= 0x2710 && (WORD)extError < 0x2EE0) {
            TheDebug << MakeString(" which is a winsock error, so fail.");
        }
    }

check_more_offers:
    if (mOfferIDsBegin != 0) {
        if (mCurOffers < mOfferIDsBegin + mOfferIDCount) {
            goto continue_enum;
        }
    }
    goto done;

error_no_more:
    if (mOfferIDsBegin != 0) {
        TheDebug << MakeString(" store enum: error no more files (%d)", (unsigned long)overlappedResult);
        mEnumerating = false;
        return;
    }
    goto done;

continue_enum:
    if (mOfferIDsBegin != 0) {
        if (mCurOffers < mOfferIDsBegin + mOfferIDCount) {
            Start();
            return;
        }
    } else {
        if (bytesReceived >= 99) {
            Start();
            return;
        }
    }

done:
    mEnumerating = false;
}

