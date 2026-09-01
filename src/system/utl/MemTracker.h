#pragma once
#include "MemStats.h"
#include "obj/Data.h"
#include "utl/AllocInfo.h"
#include "utl/KeylessHash.h"
#include "utl/Str.h"
#include "utl/TextFileStream.h"
#include "utl/TextStream.h"

// size 0x1820c
class MemTracker {
public:
    MemTracker(int, int);
    const AllocInfo *GetInfo(void *) const;
    void Alloc(
        int requestedSize,
        int actualSize,
        const char *type,
        void *memory,
        signed char heap,
        bool pooled,
        unsigned char strat,
        const char *file,
        int line
    );
    void Free(void *);
    void CloseReport();
    void SetAllocInfoName(const char *);
    void StartLog(TextStream &);
    void StopLog();
    void Realloc(void *, int, int, void *);
    void HeapReport(TextStream &);
    void DiffDump(TextStream &);
    void ReportMemoryAlloc(const char *);
    void ReportMemoryUsage(const char *);
    void ReportMemoryUsageOverview(const char *);
    void Report(int, TextStream &);
    void SetSpew(bool spew) { mSpew = spew; }
    void SetReport(TextFileStream *s) { mReport = s; }
    signed char Heap() const { return mHeap; }
    bool GetHeapOnly() const { return mHeapOnly; }
    void SetHeapOnly(bool heapOnly) { mHeapOnly = heapOnly; }

#ifdef HX_NATIVE
    static void *operator new(size_t);
#else
    static void *operator new(unsigned int);
#endif
    static void operator delete(void *);
    static int SpitAllocInfo(TextStream *);
    static int SpitAllocInfo(struct _iobuf *);

private:
    void UpdateStats();
    void ColatedPrint(TextStream &, AllocInfo *, const char *);

    static DataNode SpitAllocInfo(DataArray *);

    void *mHashMem; // 0x0
    KeylessHash<void *, AllocInfo *> *mHashTable; // 0x4
    short mTimeSlice; // 0x8
    HeapStats mHeapStats[16]; // 0xc
    BlockStatTable mMemTable[2]; // 0x14c
    BlockStatTable mPoolTable[2]; // 0xc164
    int mCurStatTable; // 0x1817c
    AllocInfoVec mFreedInfos; // 0x18180
    TextStream *mLog; // 0x1818c
    signed char mHeap; // 0x18190
    bool mHeapOnly; // 0x18191
    bool mSpew; // 0x18192
    // ---- Members below do NOT exist in RB3 retail's MemTracker. -------------
    // Retail evidence (MemTracker.s / MemTrack.s target asm):
    //   * MemTracker::operator new is called with r3 = 0x18194, so retail
    //     sizeof(MemTracker) == 0x18194 -- 116 bytes smaller than ours.
    //   * the retail ctor writes mFreedInfos at 0x18180/84/88, a word (mLog)
    //     at 0x1818c and a BYTE (mHeap) at 0x18190, and nothing beyond.
    //   * MemTrackHeapDump reads mHeap with lbzx at 0x18190.
    // They are kept (RB3 code here still references them) but parked AFTER
    // mSpew so they cannot perturb any offset retail actually uses. Deleting
    // them outright -- the fully correct fix -- also needs mReport /
    // mFreeSysMem / mFreePhysMem / the Strings / mAllocInfoName turned into
    // MemTracker.cpp file statics, across 7 including TUs.
    TextFileStream *mReport; // 0x18194 (retail: absent)
    int mFreeSysMem; // 0x18198 (retail: absent)
    int mFreePhysMem; // 0x1819c (retail: absent)
public:
    String unk181a4; // current file name
    String unk181ac; // previous file name (stack push/pop)
    String unk181b4; // current object name
private:
    char mAllocInfoName[64]; // 0x181c4
};

void MemTrackInit(int, int, bool);
bool MemTrackEnable(bool);
void MemTrackSpew(bool);
void MemTrackSetReportName(const char *);
