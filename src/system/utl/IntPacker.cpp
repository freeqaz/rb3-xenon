#include "utl/IntPacker.h"
#include <string.h>
#include "os/Debug.h"

// Retail lays this TU out directly after BinStream.cpp, in the definition order
// below. The range checks in AddS/AddU/Add/ExtractU are MILO_ASSERTs, which the
// retail build compiles out: AddS and AddU then have identical bodies and the
// linker keeps one 4-byte `b Add` for both.

IntPacker::IntPacker(void *buf, unsigned int size) {
    mBuffer = (unsigned char *)buf;
    mPos = 0;
    mSize = size;
    memset(buf, 0, size);
}

void IntPacker::Add(unsigned int num, unsigned int bits) {
    for (unsigned int u = 0; u < bits; u++) {
        mBuffer[mPos >> 3] |= ((num >> u) & 1) << (mPos & 7);
        mPos++;
    }
    MILO_ASSERT(mPos <= mSize * 8, 0x36);
}

unsigned int IntPacker::ExtractU(unsigned int bits) {
    unsigned int ret = 0;
    for (unsigned int cnt = 0; cnt < bits; cnt++) {
        ret |= ((mBuffer[mPos >> 3] >> (mPos & 7)) & 1) << cnt;
        mPos++;
    }
    MILO_ASSERT(mPos <= mSize * 8, 0x58);
    return ret;
}

void IntPacker::AddBool(bool b) { Add(b, 1); }

void IntPacker::AddS(int num, unsigned int bits) {
    int max = 1 << (bits - 1);
    MILO_ASSERT(num >= -max && num < max, 0x21);
    Add(num, bits);
}

void IntPacker::AddU(unsigned int num, unsigned int bits) {
    MILO_ASSERT(num < (unsigned int)(1 << bits), 0x28);
    Add(num, bits);
}

bool IntPacker::ExtractBool() { return ExtractU(1) != 0; }

int IntPacker::ExtractS(unsigned int bits) {
    int half = 1 << (bits - 1);
    int ex = ExtractU(bits);
    if (ex >= half) {
        ex -= half * 2;
    }
    return ex;
}

void IntPacker::SetPos(unsigned int pos) { mPos = pos; }
