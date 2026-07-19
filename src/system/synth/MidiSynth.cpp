#include "synth/MidiSynth.h"
#include "synth/Mic.h"
#include "utl/MemTracker.h"
#include <string.h>

MidiSynth::MidiSynth() { mChannels.resize(16); }

// RB3 retail scattered these RingBuffer COMDATs (from synth/Mic.cpp) into
// MidiSynth.cpp's .text span; compile them here so objdiff can pair them.
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

// RB3 retail scattered this MemTracker COMDAT (from utl/MemTracker.cpp) into
// MidiSynth.cpp's .text span; compile it here so objdiff can pair it.
void MemTracker::StopLog() {
    if (mLog) {
        *mLog << ")";
        mLog = nullptr;
    }
}
