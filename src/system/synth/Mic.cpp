#include "synth/Mic.h"
#include "math/Utl.h"
#include "os/Debug.h"
#include "obj/Data.h"
#include "utl/MemMgr.h"
#include <cstring>

void Mic::Set(const DataArray *data) {
    MILO_ASSERT(data, 0x12);
    SetGain(data->FindFloat("gain"));
    SetDMA(data->FindInt("dma"));
    DataArray *compressorArr = data->FindArray("compressor");
    SetCompressor(compressorArr->Int(1));
    SetCompressorParam(compressorArr->Float(2));
}

RingBuffer::~RingBuffer() {
    if (mBuffer) {
        MemFree(mBuffer, __FILE__, 0x23);
        mBuffer = nullptr;
    }
}

void RingBuffer::Reset() {
    memset(mBuffer, 0, mSize);
    mWriteIx = 0;
    mReadIx = 0;
    mTotal = 0;
}

void RingBuffer::Init(int size) {
    mSize = size;
    if (mBuffer) {
        MemFree(mBuffer, __FILE__, 0x2B);
        mBuffer = nullptr;
    }
    mBuffer = MemAlloc(size, __FILE__, 0x2C, "VirtualMic RingBuffer", 0x80);
    MILO_ASSERT(mBuffer, 0x2D);
    Reset();
}

int RingBuffer::Peek(void *data, int len) {
    MILO_ASSERT(len <= mSize, 0x62);
    int i2 = ((mWriteIx - len) + mSize) % mSize;
    int i30 = mSize - i2;
    i30 = Min(len, i30);
    memcpy(data, (char *)mBuffer + i2, i30);
    if (i30 != len) {
        memcpy((char *)data + i30, mBuffer, len - i30);
    }
    return len;
}

int RingBuffer::Write(void *data, int len) {
    char *src = (char *)data;
    int writeLen = len;

    if (writeLen > mSize) {
        src = src + len - mSize;
        writeLen = mSize;
    }

    int available = mSize - mWriteIx;
    int returnVal = (mTotal - mSize) + writeLen;
    int *pChunk;
    int chunkSize = writeLen;
    if (writeLen < available) {
        pChunk = &chunkSize;
    } else {
        pChunk = &available;
    }
    int chunk1 = *pChunk;

    memcpy((char *)mBuffer + mWriteIx, src, chunk1);

    if (chunk1 != writeLen) {
        memcpy(mBuffer, src + chunk1, writeLen - chunk1);
    }

    int *pTotal;
    int tempTotal = mTotal + writeLen;
    pTotal = &tempTotal;
    mWriteIx = (mWriteIx + writeLen) % mSize;
    if (tempTotal >= mSize) {
        pTotal = &mSize;
    }
    int newTotal = *pTotal;
    mTotal = newTotal;

    if (newTotal == mSize) {
        mReadIx = mWriteIx;
    }

    return returnVal;
}

int RingBuffer::Read(void *data, int len) {
    int readLen;
    if (mTotal >= len) {
        readLen = len;
    } else {
        readLen = mTotal;
    }
    int stk[2];
    stk[0] = readLen;

    if (readLen == 0) {
        return 0;
    }

    int available = mSize - mReadIx;
    int *pChunk = &stk[0];
    if (readLen < available) {
        pChunk = &readLen;
    } else {
        pChunk = &available;
    }
    int chunk1 = *pChunk;

    memcpy(data, mReadIx + (char *)mBuffer, chunk1);

    if (chunk1 != readLen) {
        memcpy((char *)data + chunk1, mBuffer, readLen - chunk1);
    }

    mTotal -= readLen;
    mReadIx = (mReadIx + readLen) % mSize;

    return readLen;
}
