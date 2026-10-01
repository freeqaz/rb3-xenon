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
        unsigned char strat
#ifdef HX_NATIVE
        ,
        const char *file,
        int line
#endif
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
    signed char Heap() const { return mHeap; }
#ifdef HX_NATIVE
    void SetSpew(bool spew) { mSpew = spew; }
    void SetReport(TextFileStream *s) { mReport = s; }
    bool GetHeapOnly() const { return mHeapOnly; }
    void SetHeapOnly(bool heapOnly) { mHeapOnly = heapOnly; }
#endif

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
    // Retail sizeof(MemTracker) is 0x18194 (operator new is called with that
    // size) and the ctor (0x827d4a28) writes nothing past mHeap, so everything
    // below is DC3-era and native-only.
#ifdef HX_NATIVE
    bool mHeapOnly; // 0x18191
    bool mSpew; // 0x18192
    TextFileStream *mReport;
    int mFreeSysMem;
    int mFreePhysMem;
public:
    String unk181a4; // current file name
    String unk181ac; // previous file name (stack push/pop)
    String unk181b4; // current object name
private:
    char mAllocInfoName[64];
#endif
};

#ifdef HX_NATIVE
void MemTrackInit(int, int, bool);
#else
// Retail 0x827c4bb8 takes two arguments: it never reads r5, and its only
// caller (MemInit, 0x827bd300) never sets it.
void MemTrackInit(int, int);
#endif
bool MemTrackEnable(bool);
void MemTrackSpew(bool);
void MemTrackSetReportName(const char *);
