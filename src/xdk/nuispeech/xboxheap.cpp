#include "xboxheap.h"

NUISPEECH::CXboxHeap::CXboxHeap(unsigned int initSize, unsigned int size) {
    mSize = size;
    mFreeHead = this;
    mCount = 0;
    mUsedHead = this;
    auto& listHead = mListHead;
    listHead.mNext = &listHead;
    listHead.mPrev = &listHead;
    AllocatePageBlock(initSize);
}