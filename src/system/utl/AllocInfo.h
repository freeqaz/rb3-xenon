#pragma once
#include "MemTrack.h"
#include "os/Debug.h"
#include "utl/Str.h"
#include "utl/trie.h"
#include "utl/TextStream.h"

// Retail RB3 AllocInfo is 0x18 bytes (Pool(0x18) in SetPoolMemory, new(0x18)
// in MemTracker::Alloc @0x827d57a8) with a seven-argument ctor (0x827d71e8) and
// no destructor; the file/line/trie-string/stack-trace members are DC3-era and
// native-only.
#ifdef HX_NATIVE
// size 0x65
#pragma pack(push, 1)
#endif
class AllocInfo {
public:
#ifndef HX_NATIVE
    AllocInfo(
        int requestedSize,
        int actualSize,
        const char *type,
        void *mem,
        signed char heap,
        bool pooled,
        unsigned char strat
    );
#else
    AllocInfo(
        int requestedSize,
        int actualSize,
        const char *type,
        void *mem,
        signed char heap,
        bool pooled,
        unsigned char strat,
        const char *file,
        int line,
        String &,
        String &
    );
    ~AllocInfo();
    void FillStackTrace();
    void PrintCsv(TextStream &) const;
    void PrintForReport(TextStream &) const;
    void PrintForReport(struct _iobuf *) const;
#endif

    int Compare(const AllocInfo &) const;
    void Validate() const;
    void Print(TextStream &) const;
    int StackCompare(const AllocInfo &) const;

    static bool bPrintCsv;
    static void SetPoolMemory(void *, int);
#ifdef HX_NATIVE
    static void *operator new(size_t);
#else
    static void *operator new(unsigned int);
#endif
    static void operator delete(void *);

    int mReqSize; // 0x0
    int mActSize; // 0x4
    const char *mType; // 0x8
    void *mMem; // 0xc
    signed char mHeap; // 0x10
    bool mPooled; // 0x11
    short mTimeSlice; // 0x12
    unsigned char mStrat; // 0x14
#ifdef HX_NATIVE
    const char *mFile; // 0x15
    int mLine; // 0x19
    unsigned int unk1d; // 0x1d
    unsigned int unk21; // 0x21
    int mStackTrace[0x10]; // 0x25
#endif
};
#ifdef HX_NATIVE
#pragma pack(pop)
#endif

TextStream &operator<<(TextStream &, const AllocInfo &);

class AllocInfoVec {
public:
    AllocInfoVec() : mStart(0), mEnd(0), mEndOfStorage(0) {}
    __forceinline AllocInfoVec(int size)
        : mStart((AllocInfo **)DebugHeapAlloc(size * sizeof(AllocInfo *))), mEnd(mStart),
          mEndOfStorage(mStart + size) {}
    ~AllocInfoVec() { DebugHeapFree(mStart); }

    AllocInfo **begin() { return mStart; }
    AllocInfo **end() { return mEnd; }

    void push_back(AllocInfo *info) {
        MILO_ASSERT(mEnd < mEndOfStorage, 0x61);
        *mEnd++ = info;
    }

    AllocInfo **erase(AllocInfo **first, AllocInfo **last);
    void clear() { erase(begin(), end()); }

    void delete_and_clear() {
        AllocInfo **e = mEnd;
        for (AllocInfo **it = mStart; it != e; ++it) {
            AllocInfo *info = *it;
            if (info) {
                delete info;
            }
        }
        clear();
    }

private:
    AllocInfo **mStart; // 0x0
    AllocInfo **mEnd; // 0x4
    AllocInfo **mEndOfStorage; // 0x8
};

void AllocInfoInit();