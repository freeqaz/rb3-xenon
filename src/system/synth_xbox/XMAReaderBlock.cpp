// XMAReaderBlock: the per-channel byte FIFO behind XMAReader.
// Retail .text 0x82BBB2E8-0x82BBB510, written from the retail asm.
#include "synth_xbox/XMAReaderBlock.h"
#include "utl/MemMgr.h"
#include <string.h>

// 0x82BBB2E8
XMAReaderBlock::~XMAReaderBlock() {
    if (mOwnsBuffer)
        delete mBuffer;
    mBuffer = 0;
}

// 0x82BBB330
int XMAReaderBlock::FreeSpace() const {
    if (mRead > mWrite)
        return mRead - mWrite;
    if (mRead < mWrite)
        return mRead - mWrite + mSize;
    return mFull ? 0 : mSize;
}

// 0x82BBB378
int XMAReaderBlock::ReadableBytes() const {
    if (mRead == mWrite && !mFull)
        return 0;
    if (mRead < mWrite)
        return mWrite - mRead;
    return mSize - mRead;
}

// 0x82BBB3B0
void *XMAReaderBlock::ReadPtr(int) const { return mBuffer + mRead; }

// 0x82BBB3C0
void XMAReaderBlock::Consume(int bytes) {
    if (bytes != 0) {
        mRead += bytes;
        if (mRead == mSize)
            mRead = 0;
        mFull = false;
    }
}

// 0x82BBB3F0
XMAReaderBlock::XMAReaderBlock(int size)
    : mSize(size), mRead(0), mWrite(0), mFull(false), mBuffer(0), mOwnsBuffer(true) {
    mBuffer = (char *)MemAlloc(size, 0x20);
}

// 0x82BBB450
bool XMAReaderBlock::Write(const void *data, int bytes, bool) {
    bool noRoom = FreeSpace() < bytes;
    if (!noRoom) {
        if (mWrite + bytes <= mSize) {
            memcpy(mBuffer + mWrite, data, bytes);
        } else {
            int head = mSize - mWrite;
            memcpy(mBuffer + mWrite, data, head);
            memcpy(mBuffer, (const char *)data + head, bytes - head);
        }
        mWrite += bytes;
        if (mWrite >= mSize)
            mWrite -= mSize;
        mFull = mWrite == mRead;
    }
    return !noRoom;
}
