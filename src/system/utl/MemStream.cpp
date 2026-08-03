#include "utl/MemStream.h"

void MemStream::Flush() {}

bool MemStream::Fail() { return mFail; }

// Retail RB3-360 guards a negative byte count before doing anything else --
// `cmpwi cr6,r5,0 / bge / li r11,1 / stb r11,0xc(r3) / b <epilogue>` at the very
// top of the retail body (target 148 B vs our 124 B).  NEITHER oracle has this:
// rb3-Wii's MemStream.cpp and dc3's both start straight at the overflow test, so
// this is a genuine RB3-360-only guard recovered from retail asm.
//
// The overflow arm calls mBuffer.size() TWICE (retail computes `subf r7,r9,r10`
// for the compare and then re-computes `subf r10,r9,r10` inside the taken
// branch); caching it in a local `size`, as we did, folds those into one.  Same
// for mTell -- no `tell` local.  This is dc3's exact shape.
void MemStream::ReadImpl(void *data, int bytes) {
    if (bytes < 0) {
        mFail = true;
        return;
    }
    // Statement order matters: the `mFail` store may alias the vector's
    // begin/end pointers, so writing it BEFORE the second size() forces MSVC to
    // reload both from memory (an extra `addi r11,r31,0x14` + two lwz).  Retail
    // recomputes the size from the still-live registers and stores mFail after,
    // which is also dc3's order.
    if (mTell + bytes > mBuffer.size()) {
        bytes = mBuffer.size() - mTell;
        mFail = true;
    }
    memcpy(data, &mBuffer[mTell], bytes);
    mTell += bytes;
}

void MemStream::SeekImpl(int offset, SeekType t) {
    int pos;

    switch (t) {
    case kSeekBegin:
        pos = offset;
        break;
    case kSeekCur:
        pos = mTell + offset;
        break;
    case kSeekEnd:
        pos = mBuffer.size() + offset;
        break;
    default:
        return;
    }

    if (pos < 0 || pos > mBuffer.size()) {
        mFail = true;
    } else {
        mTell = pos;
    }

    // case 0: validate offset, mFail = true or mTell = offset
    // case 1: offset += mTell, then case 0's logic
    // case 2: offset += mSize, then case 0's logic
}

void MemStream::Compact() {
    mBuffer.erase(mBuffer.begin(), mBuffer.begin() + mTell);
    mTell = 0;
}

MemStream::MemStream(bool b) : BinStream(b) {
    mBuffer.reserve(0x1000);

    // Initializer list wasn't used here for some reason
    mFail = false;
    mTell = 0;
}

void MemStream::WriteImpl(const void *data, int bytes) {
    int toReserve = mBuffer.capacity();
    while (mTell + bytes > toReserve)
        toReserve += toReserve;
    mBuffer.reserve(toReserve);
    if (mTell + bytes > mBuffer.size()) {
        mBuffer.resize(mTell + bytes);
    }
    memcpy(&mBuffer[mTell], data, bytes);
    mTell += bytes;
}

void MemStream::WriteStream(BinStream &bs, int bytes) {
    int toReserve = mBuffer.capacity();
    while (mTell + bytes > toReserve)
        toReserve += toReserve;
    mBuffer.reserve(toReserve);
    if (mTell + bytes > mBuffer.size()) {
        mBuffer.resize(mTell + bytes);
    }
    bs.Read(&mBuffer[mTell], bytes);
    mTell += bytes;
}
