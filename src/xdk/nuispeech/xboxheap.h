#pragma once

namespace NUISPEECH {
    class CXboxHeap {
        struct _BLOCK_ENTRY {
            _BLOCK_ENTRY *mNext; // 0x0
            _BLOCK_ENTRY *mPrev; // 0x4
        };

    public:
        CXboxHeap(unsigned int, unsigned int);
        ~CXboxHeap();

        void *Alloc(unsigned int, bool);
        bool Free(void *);
        void *Realloc(void *, unsigned int, bool);

    private:
        _BLOCK_ENTRY *AllocatePageBlock(unsigned int);
        void InsertFreeBLockList(_BLOCK_ENTRY *);

    protected:
        CXboxHeap *mFreeHead; // 0x0
        CXboxHeap *mUsedHead; // 0x4
        _BLOCK_ENTRY mListHead; // 0x8 (mNext at 0x8, mPrev at 0xC)
        unsigned int mSize; // 0x10
        unsigned int mCount; // 0x14
    };
}