#include "utl/MemTracker.h"
#include "AllocInfo.h"
#include "MemMgr.h"
#include "MemTrack.h"
#include "Memory.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "os/Debug.h"
#include "os/System.h"
#include "utl/KeylessHash.h"
#include "math/Sort.h"
#include "utl/MakeString.h"
#include "utl/MemMgr.h"
#include "utl/MemStats.h"
#include "utl/Symbol.h"
#include "utl/TextFileStream.h"
#include "utl/TextStream.h"

extern bool gMemTrackerTracking;
String gMemLogType;

struct MemDiffEntry {
    char mName[59]; // 0x00
    char _pad;      // 0x3B
    int mNumDiff;   // 0x3C
    int mSizeDiff;  // 0x40
    int mHeap; // 0x44
    // total: 0x48 = 72 bytes

    bool operator<(const MemDiffEntry &other) const {
        return mHeap < other.mHeap;
    }
};

extern MemTracker *gMemTracker;

bool StackLess(AllocInfo *const &a1, AllocInfo *const &a2) {
    return a1->StackCompare(*a2) < 0;
}

int HashKey(void *ptr, int size) {
    MILO_ASSERT((uint(ptr) & 7) == 0, 0x25);
    return (uint(ptr) / 8) % size;
}

void DiffTblReport(const char *name, BlockStatTable &curTable, BlockStatTable &prevTable, TextStream &ts) {
    curTable.SortByName();
    prevTable.SortByName();

    std::vector<MemDiffEntry> diffs;
    int curNum = curTable.GetNumStats();
    int prevNum = prevTable.GetNumStats();
    diffs.reserve(curNum + prevNum);

    int curIdx = 0;
    int prevIdx = 0;

    while (curIdx < curNum) {
        if (prevIdx >= prevNum)
            break;

        BlockStat &curStat = curTable.GetBlockStat(curIdx);
        BlockStat &prevStat = prevTable.GetBlockStat(prevIdx);

        int cmp = strcmp(curStat.mName, prevStat.mName);

        int numAllocs1, numAllocs2;
        int size1, size2;
        unsigned char heap;
        const char *entryName;

        if (cmp < 0) {
            numAllocs1 = curStat.mNumAllocs;
            size1 = curStat.mSizeReq;
            numAllocs2 = 0;
            size2 = 0;
            heap = curStat.mHeap;
            curIdx++;
            entryName = curStat.mName;
        } else if (cmp > 0) {
            numAllocs1 = 0;
            size1 = 0;
            numAllocs2 = prevStat.mNumAllocs;
            size2 = prevStat.mSizeReq;
            heap = prevStat.mHeap;
            prevIdx++;
            entryName = prevStat.mName;
        } else {
            numAllocs1 = curStat.mNumAllocs;
            size1 = curStat.mSizeReq;
            numAllocs2 = prevStat.mNumAllocs;
            size2 = prevStat.mSizeReq;
            heap = prevStat.mHeap;
            curIdx++;
            prevIdx++;
            entryName = curStat.mName;
        }

        int numDiff = numAllocs1 - numAllocs2;
        int sizeDiff = size1 - size2;

        if (numDiff != 0 || sizeDiff != 0) {
            MemDiffEntry entry;
            strncpy(entry.mName, entryName, 0x3a);
            entry.mName[0x3a] = '\0';
            entry.mNumDiff = numDiff;
            entry.mSizeDiff = sizeDiff;
            entry.mHeap = heap;
            diffs.push_back(entry);
        }
    }

    int totalBytes = 0;
    int totalNum = 0;

    std::sort(diffs.begin(), diffs.end());

    ts << MakeString("%-62s %8s %8s\n", name, "Num", "Bytes");

    int lastHeap = -2;
    for (std::vector<MemDiffEntry>::iterator it = diffs.begin(); it != diffs.end(); ++it) {
        if (it->mHeap != lastHeap) {
            ts << MakeString(" HEAP %d ------------------\n", it->mHeap);
            lastHeap = it->mHeap;
        }
        totalBytes += it->mSizeDiff;
        totalNum += it->mNumDiff;
        ts << MakeString("  %-60s %8d %8d\n", it->mName, it->mNumDiff, it->mSizeDiff);
    }

    ts << MakeString(" %-61s %8d %8d\n\n", "TOTAL ------", totalNum, totalBytes);
}

// Explicit template instantiation for MakeString with 4 array reference arguments
template const char *MakeString<const char(&)[9], const char(&)[3], const char(&)[9], const char(&)[8]>(
    const char *,
    const char(&)[9],
    const char(&)[3],
    const char(&)[9],
    const char(&)[8]
);

MemTracker::MemTracker(int x, int y)
    : mHashMem(nullptr), mHashTable(nullptr), mTimeSlice(0), mCurStatTable(0),
      mFreedInfos(y), mLog(0), mReport(0), mHeap(x) {
    int hashSize = y * 2;
    mHashMem = DebugHeapAlloc(y * 8);
    MILO_ASSERT(mHashMem, 0x4E);
    mHashTable = new KeylessHash<void *, AllocInfo *>(
        hashSize, (AllocInfo *)0, (AllocInfo *)-1, (AllocInfo **)mHashMem
    );
    mFreeSysMem = _GetFreeSystemMemory();
    mFreePhysMem = _GetFreePhysicalMemory();
    DataRegisterFunc("spit_alloc_info", SpitAllocInfo);
    DataRegisterFunc("sai", SpitAllocInfo);
}

#ifdef HX_NATIVE
void *MemTracker::operator new(size_t size) { return DebugHeapAlloc(size); }
#else
void *MemTracker::operator new(unsigned int size) { return DebugHeapAlloc(size); }
#endif
void MemTracker::operator delete(void *mem) { DebugHeapFree(mem); }

const AllocInfo *MemTracker::GetInfo(void *info) const {
    AllocInfo **found = mHashTable->Find(info);
    if (found) {
        return *found;
    } else
        return nullptr;
}

void MemTracker::Alloc(
    int requestedSize,
    int actualSize,
    const char *type,
    void *memory,
    signed char heap,
    bool pooled,
    unsigned char strat,
    const char *file,
    int line
) {
    if (!gMemTrackerTracking)
        return;
    MILO_ASSERT(type, 0x6D);
    if (mHeap != -1 && heap != mHeap) {
        return;
    }
    gMemTrackerTracking = false;
    AllocInfo::bPrintCsv = true;
    if (!mHeapOnly) {
        String str1;
        String str2;
        AllocInfo *info = new AllocInfo(
            requestedSize,
            actualSize,
            type,
            memory,
            heap,
            pooled,
            strat,
            file,
            line,
            str1,
            str2
        );
        mHashTable->Insert(info);
        if (pooled || gMemLogType != gNullStr || gMemLogType == type) {
            if (pooled || mHeap != -1 && heap != mHeap) {
                if (mLog) {
                    *mLog << " ((com new) " << "(mem " << memory << ") " << info << ")\n";
                }
                if (mSpew) {
                    TheDebug << "::Alloc::" << info->mType << " Allocated "
                             << info->mActSize << " Requested " << info->mReqSize
                             << " Address " << info->mMem << " Heap " << info->mHeap
                             << str1.c_str() << ":" << str2.c_str() << "\n";
                }
            }
        } else {
            // if !mLog goto above
            *mLog << " new, ";
            info->PrintCsv(*mLog);
            *mLog << "\n";
        }
    }
    if (!pooled) {
        mHeapStats[heap].Alloc(actualSize, requestedSize);
    }
    gMemTrackerTracking = true;
}

void MemTracker::Free(void *mem) {
    AllocInfo **found = mHashTable->Find(mem);
    if (found) {
        AllocInfo *info = *found;
        info->Validate();
        if (mLog && !info->mPooled && (mHeap == -1 || info->mHeap == mHeap)
            && info->mStrat == 0) {
            *mLog << " ((com free) " << "(" << mem << ") " << *info << ")\n";
        }
        if (!info->mPooled) {
            mHeapStats[info->mHeap].Free(info->mActSize, info->mReqSize);
        }
        mHashTable->Remove(found);
        if (info->mTimeSlice == mTimeSlice) {
            delete info;
        } else {
            mFreedInfos.push_back(info);
        }
    }
}

void MemTracker::ColatedPrint(TextStream &ts, AllocInfo *info, const char *com) {
    ts << "  ((com " << com << ") (rep " << 1 << " ) " << *info << ")\n";
}

void MemTracker::CloseReport() {
    if (mReport) {
        MemNumHeaps();
        TextStream &ts = *mReport;
        ts << "\n";
        ts << "\n";
        ts << "Category,CategoryName,Column,Budget,BudgetType,AlwaysShow,Tooltip\n";
        ts << "column_info,overview,Mode,0,0,1,notes\n";
        ts << "column_info,overview,MainPeak,0,0,1,notes\n";
        ts << "column_info,overview,MainAlloc,0,0,1,notes\n";
        ts << "column_info,overview,MainLargest,0,1,0,notes\n";
        ts << "column_info,overview,CharPeak,0,0,1,notes\n";
        ts << "column_info,overview,CharAlloc,0,0,1,notes\n";
        ts << "column_info,overview,CharLargest,0,1,0,notes\n";
        ts << "column_info,overview,PhysPeak,0,0,1,notes\n";
        ts << "column_info,overview,PhysAlloc,0,0,1,notes\n";
        ts << "column_info,overview,PhysLargest,0,0,1,notes\n";
        ts << "column_info,base,heap,-1.0,-1,1,heap name\n";
        ts << "column_info,base,free,0,0,0,bytes free in heap\n";
        ts << "column_info,base,biggest,0,0,1,size of largest free block\n";
        ts << "column_info,base,lfrags,0,0,0,fragmentation count at low end of memory\n";
        ts << "column_info,base,requested,0,0,0,amount of memory actually requested\n";
        ts << "column_info,base,allocated,0,0,1,amount of memory actually allocated\n";
        ts << "column_info,base,peak,0,0,1,memory high water mark\n";
        ts << "column_info,game,heap,-1.0,-1,1,heap name\n";
        ts << "column_info,game,free,0,0,0,bytes free in heap\n";
        ts << "column_info,game,biggest,0,0,1,size of largest free block\n";
        ts << "column_info,game,lfrags,0,0,0,fragmentation count at low end of memory\n";
        ts << "column_info,game,requested,0,0,0,amount of memory actually requested\n";
        ts << "column_info,game,allocated,0,0,1,amount of memory actually allocated\n";
        ts << "column_info,game,peak,0,0,1,memory high water mark\n";
        ts << "\n";
        ts << "Category,CategoryName\n";
        ts << "category_info,game\n";
        ts << "category_info,base\n";
        ts << "\nDone\n";
        mReport->File().Flush();
        RELEASE(mReport);
    }
}

void MemTracker::SetAllocInfoName(const char *name) {
    Hx_snprintf(mAllocInfoName, 64, "%s", name);
}

void MemTracker::StartLog(TextStream &ts) {
    if (mLog) {
        StopLog();
    }
    MILO_ASSERT(!mLog, 0x113);
    *mLog = ts;
    *mLog << "(elf " << TheSystemArgs.front() << ")\n";
    *mLog << "(data\n";
}

void MemTracker::StopLog() {
    if (mLog) {
        *mLog << ")";
        mLog = nullptr;
    }
}

void MemTracker::Realloc(void *key, int reqSize, int actualSize, void *mem) {
    AllocInfo **found = mHashTable->Find(key);
    if (found) {
        AllocInfo *info = *found;
        info->Validate();
        bool validHeap = mHeap == -1 || info->mHeap == mHeap;
        MILO_ASSERT(validHeap, 0xF6);
        if (reqSize == -1) {
            reqSize = info->mReqSize;
        }
        if (actualSize == -1) {
            actualSize = info->mActSize;
        }
        signed char heap = info->mHeap;
        unsigned char strat = info->mStrat;
        const char *type = info->mType;
        MILO_ASSERT(info->mPooled == 0, 0x100);
        Free(key);
        Alloc(reqSize, actualSize, type, mem, heap, false, strat, __FILE__, 0x102);
    }
}

void MemTracker::HeapReport(TextStream &ts) {
    int max = MemNumHeaps() + 1;
    for (int i = 0; i < max; i++) {
        HeapStats &curStats = mHeapStats[i];
        ts << MakeString("\n*** FREE LIST for heap #%d ***\n", i);
        if (i == MemNumHeaps()) {
            ts << MakeString("  Heap name          = %14s\n", "physical");
            ts << MakeString("  Heap size          = %14d\n", mFreePhysMem);
            ts << MakeString(
                "  Num Free Bytes     = %14d\n", mFreePhysMem - PhysicalUsage()
            );
            ts << MakeString("  Biggest Free Block = %14d\n", _GetFreePhysicalMemory());
            ts << MakeString("  Num Free Blocks    = %14s\n", "N/A");
        } else {
            int i1, i2, i3, i4, i5;
            MemFreeBlockStats(i, i1, i2, i3, i4, i5);
            ts << MakeString("  Heap name          = %14s\n", MemHeapName(i));
            ts << MakeString("  Heap size          = %14d\n", MemHeapSize(i));
            ts << MakeString("  Num Free Bytes     = %14d\n", i3);
            ts << MakeString("  Biggest Free Block = %14d\n", i5);
            ts << MakeString("  lFrags             = %14d\n", i1);
        }
        ts << MakeString("  Num Allocs         = %14d\n", curStats.mTotalNumAllocs);
        ts << MakeString("  Bytes Requested    = %14d\n", curStats.mTotalReqSize);
        ts << MakeString("  Bytes Allocated    = %14d\n", curStats.mTotalActSize);
        ts << MakeString("  Peak Num Allocs    = %14d\n", curStats.mMaxNumAllocs);
        ts << MakeString("  Peak Bytes Alloc'd = %14d\n", curStats.mMaxActSize);
    }
}

void MemTracker::UpdateStats() {
    mPoolTable[mCurStatTable].Clear();
    mMemTable[mCurStatTable].Clear();
    for (auto it = mHashTable->Begin(); it != nullptr; it = mHashTable->Next(it)) {
        AllocInfo *info = *it;
        if (info->mPooled) {
            mPoolTable[mCurStatTable].Update(
                info->mType, info->mHeap, info->mReqSize, info->mActSize
            );
        } else {
            mMemTable[mCurStatTable].Update(
                info->mType, info->mHeap, info->mReqSize, info->mActSize
            );
        }
    }
}

DataNode MemTracker::SpitAllocInfo(DataArray *a) {
    int ret = 1;
    if (a && a->Size() > 1) {
        TextFileStream stream(a->Str(1), false);
        ret = SpitAllocInfo(&stream);
    }
    return ret;
}

void MemTracker::Report(int threshold, TextStream &ts) {
    int numMemBlocks, numPoolBlocks;

    HeapReport(ts);
    UpdateStats();

    mMemTable[mCurStatTable].SortBySize();
    numMemBlocks = mMemTable[mCurStatTable].GetNumStats();
    ts << MakeString("\n  %-30s %2s %5s %10s %10s\n", "TYPE", "Hp", "Num", "SzRequest", "SzActual");

    for (int i = 0; i < numMemBlocks; i++) {
        BlockStat &stat = mMemTable[mCurStatTable].GetBlockStat(i);
        if (stat.mSizeAct >= threshold) {
            ts << MakeString(
                "  %-30s %2d %5d %10d %10d\n",
                stat.mName, stat.mHeap, stat.mNumAllocs, stat.mSizeReq, stat.mSizeAct
            );
        }
    }

    mPoolTable[mCurStatTable].SortBySize();
    numPoolBlocks = mPoolTable[mCurStatTable].GetNumStats();
    ts << MakeString("\n  %-30s %5s %10s %10s\n", "POOL TYPE", "Num", "SzRequest", "SzActual");

    for (int i = 0; i < numPoolBlocks; i++) {
        BlockStat &stat = mPoolTable[mCurStatTable].GetBlockStat(i);
        if (stat.mSizeAct >= threshold) {
            ts << MakeString("  %-30s %5d %10d %10d\n", stat.mName, stat.mNumAllocs, stat.mSizeReq, stat.mSizeAct);
        }
    }

    ts << "Diff from last report:\n";
    DiffTblReport("MALLOC DIFF TYPES", mMemTable[mCurStatTable], mMemTable[1 - mCurStatTable], ts);

    DiffTblReport("POOL DIFF TYPES", mPoolTable[mCurStatTable], mPoolTable[1 - mCurStatTable], ts);

    mCurStatTable = 1 - mCurStatTable;
}

int MemTracker::SpitAllocInfo(TextStream *ts) {
    int ret = 1;
    if (gMemTracker != nullptr && gMemTracker->mHashTable != nullptr) {
        FormatString begin_fmt("----------------------------------------BEGIN MemTracker");
        *ts << begin_fmt.Str() << "\n";
        for (auto it = gMemTracker->mHashTable->Begin(); it != nullptr; it = gMemTracker->mHashTable->Next(it)) {
            AllocInfo *info = *it;
            info->PrintForReport(*ts);
        }
        FormatString end_fmt("----------------------------------------END MemTracker:::");
        *ts << end_fmt.Str() << "\n";
        ret = 0;
    }
    return ret;
}

int MemTracker::SpitAllocInfo(struct _iobuf *file) {
    int ret = 1;
    if (gMemTracker != nullptr && gMemTracker->mHashTable != nullptr) {
        {
            FormatString fmt("----------------BEGIN MemTracker::SpitAllocInfo\n");
            TheDebug << fmt.Str();
        }
        for (auto it = gMemTracker->mHashTable->Begin(); it != nullptr; it = gMemTracker->mHashTable->Next(it)) {
            AllocInfo *info = *it;
            info->PrintForReport(file);
        }
        {
            FormatString fmt("----------------END MemTracker::SpitAllocInfo\n");
            TheDebug << fmt.Str();
        }
        ret = 0;
    }
    return ret;
}

void MemTracker::DiffDump(TextStream &ts) {
    if (mTimeSlice) {
        ts << "(executable " << TheSystemArgs.front() << ")\n";
        ts << "(data\n";
        short curTimeSlice = mTimeSlice;
        int count = 0;
        for (AllocInfo **it = mHashTable->Begin(); it; it = mHashTable->Next(it)) {
            if (curTimeSlice == (*it)->mTimeSlice) {
                count++;
            }
        }
        AllocInfo **allocVec = (AllocInfo **)DebugHeapAlloc(count * sizeof(AllocInfo *));
        AllocInfo **allocEnd = allocVec + count;
        AllocInfo **allocBegin = allocVec;
        for (AllocInfo **it = mHashTable->Begin(); it; it = mHashTable->Next(it)) {
            if (curTimeSlice == (*it)->mTimeSlice) {
                *allocVec = *it;
                allocVec++;
            }
        }
        std::sort(allocBegin, allocEnd, StackLess);
        std::sort(mFreedInfos.begin(), mFreedInfos.end(), StackLess);

        AllocInfo **freedIt = mFreedInfos.begin();
        AllocInfo **allocIt = allocBegin;

        for (; allocIt != allocEnd || freedIt != mFreedInfos.end();) {
            if (allocIt == allocEnd) {
                ColatedPrint(ts, *freedIt, "free");
                freedIt++;
            } else if (freedIt == mFreedInfos.end()) {
                ColatedPrint(ts, *allocIt, "alloc");
                allocIt++;
            } else {
                int cmp = (*allocIt)->StackCompare(**freedIt);
                if (cmp < 0) {
                    ColatedPrint(ts, *allocIt, "alloc");
                    allocIt++;
                } else if (cmp > 0) {
                    ColatedPrint(ts, *freedIt, "free");
                    freedIt++;
                } else {
                    allocIt++;
                    freedIt++;
                }
            }
        }
        DebugHeapFree(allocBegin);
        ts << ")\n";
    }
    mFreedInfos.delete_and_clear();
    mTimeSlice++;
}

#include "hamobj/HamGameData.h"
#include "hamobj/HamPlayerData.h"

void MemTracker::ReportMemoryAlloc(const char *name) {
    Symbol venue = TheGameData->Venue();
    const char *char0 = 0;
    const char *char1 = 0;
    Symbol song = TheGameData->GetSong();
    const char *venueStr = venue.Str();
    HamPlayerData *p0 = TheGameData->Player(0);
    if (p0) {
        char0 = p0->Char().Str();
    }
    HamPlayerData *p1 = TheGameData->Player(1);
    if (p1) {
        char1 = p1->Char().Str();
    }
    char buf[128];
    Hx_snprintf(buf, 0x80, "%s_%s_%s_%s_%s_%s_alloc_info.csv",
                mAllocInfoName, name, venueStr, char0, char1, song.Str());
    TextFileStream stream(buf, false);
    SpitAllocInfo(&stream);
    stream.File().Flush();
}

static bool sReportHeaderWritten = false;

void MemTracker::ReportMemoryUsage(const char *name) {
    TextStream *ts = &TheDebug;
    if (mReport) {
        ts = mReport;
    }
    if (!sReportHeaderWritten) {
        FormatString hdr("Category,heap,free,biggest,lfrags,requested,allocated,peak\n");
        *ts << hdr.Str();
        sReportHeaderWritten = true;
    }
    int numHeaps = MemNumHeaps();
    for (int i = 0; i < numHeaps + 1; i++) {
        {
            FormatString nameStr(name);
            *ts << nameStr.Str();
        }
        if (i == MemNumHeaps()) {
            int freeMem = _GetFreePhysicalMemory();
            int used = mFreePhysMem - PhysicalUsage();
            if (used < freeMem) {
                used = freeMem;
            }
            {
                FormatString physHeap(",physicalHeap");
                *ts << physHeap.Str();
            }
            *ts << MakeString(",%d", used);
            *ts << MakeString(",%d", freeMem);
            {
                FormatString zero(",0");
                *ts << zero.Str();
            }
        } else {
            int lfrags, i2, free, i4, biggest;
            MemFreeBlockStats(i, lfrags, i2, free, i4, biggest);
            const char *heapName = MemHeapName(i);
            *ts << MakeString(",%sHeap", heapName);
            *ts << MakeString(",%d", free);
            *ts << MakeString(",%d", biggest);
            *ts << MakeString(",%d", lfrags);
        }
        HeapStats &stats = mHeapStats[i];
        *ts << MakeString(",%d", stats.mTotalReqSize);
        *ts << MakeString(",%d", stats.mTotalActSize);
        *ts << MakeString(",%d\n", stats.mMaxActSize);
    }
}

void MemTracker::ReportMemoryUsageOverview(const char *name) {
    TextStream *ts = &TheDebug;
    if (mReport) {
        ts = mReport;
    }
    FormatString hdr(
        "\nCategory,Mode,MainPeak,MainAlloc,MainLargest,CharPeak,CharAlloc,CharLargest,"
        "PhysPeak,PhysAlloc,PhysLargest\n"
    );
    *ts << hdr.Str();
    int numHeaps = MemNumHeaps();
    *ts << "overview,";
    *ts << name;
    int loopMax = numHeaps + 1;
    for (int i = 0; i < loopMax; i++) {
        int biggest;
        if (i == MemNumHeaps()) {
            int freeMem = _GetFreePhysicalMemory();
            int used = mFreePhysMem - PhysicalUsage();
            if (used < freeMem) {
                used = freeMem;
            }
            biggest = 0;
        } else {
            int lfrags, i2, free, i4;
            MemFreeBlockStats(i, lfrags, i2, free, i4, biggest);
        }
        HeapStats &stats = mHeapStats[i];
        *ts << MakeString(",%d", stats.mMaxActSize);
        *ts << MakeString(",%d", stats.mTotalActSize);
        *ts << MakeString(",%d", biggest);
    }
}

#ifndef HX_NATIVE
// Forward declaration for __pop_heap_aux template specialization
namespace stlpmtx_std {
    extern void __pop_heap_aux(MemDiffEntry*, MemDiffEntry*, int, less<MemDiffEntry>);
}

// Template specialization for sort_heap<MemDiffEntry*, less<MemDiffEntry>>
namespace stlpmtx_std {
    template <>
    void sort_heap<MemDiffEntry* __restrict, less<MemDiffEntry>>(MemDiffEntry* __restrict __first, MemDiffEntry* __restrict __last, less<MemDiffEntry> __comp) {
        while ((__last - __first) / 72 > 1) {
            __pop_heap_aux(__first, __last, 0, __comp);
            __last -= 72;
        }
    }
}
#endif
