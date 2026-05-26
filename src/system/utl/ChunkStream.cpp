#include "utl/ChunkStream.h"

#include "Compress.h"
#include "obj/Object.h"
#include "os/CritSec.h"
#include "os/Endian.h"
#include "os/File.h"
#include "os/SynchronizationEvent.h"
#include "os/System.h"
#include "utl/Std.h"

namespace {
    std::list<DecompressTask> gDecompressionQueue;
    CriticalSection gDecompressionCritSec;
    bool gDecompressionThread = false;
    SynchronizationEvent gDataProcessedEvt;
    SynchronizationEvent gDataReadyEvt;
    void *mThreadHandle;

    unsigned long DecompressionThread(void *v) {
        for (; gDecompressionThread != false;) {
            if (ChunkStream::PollDecompressionWorker()) {
                gDataProcessedEvt.Set();
            } else {
                gDataReadyEvt.Wait(-1);
            }
        }
        return false;
    }

    void StartDecompressionThread() {
        if (!gDecompressionThread) {
            gDecompressionThread = true;
#ifdef HX_NATIVE
            // Skip threaded decompression on native — runs synchronously
#else
            mThreadHandle = CreateThread(nullptr, 0, DecompressionThread, nullptr, 4, nullptr);
            MILO_ASSERT(mThreadHandle, 0x82);
            XSetThreadProcessor(mThreadHandle, 3);
            ResumeThread(mThreadHandle);
#endif
        } else {
            gDataReadyEvt.Set();
        }
    }
}

Hmx::Object *gActiveChunkObject;

void ChunkStream::SetPlatform(Platform plat) {
    if (plat == kPlatformNone) {
        plat = ConsolePlatform();
    }
    mLittleEndian = PlatformLittleEndian(plat);
    mPlatform = plat;
}

void ChunkStream::WriteImpl(const void *data, int bytes) {
    if (mCurBufOffset + bytes > mBufSize) {
        while (mCurBufOffset + bytes > mBufSize)
            mBufSize += mBufSize;
        void *a = _MemAllocTemp(mBufSize, __FILE__, 0x1E4, "ChunkStreamBuf", 0);
        memcpy(a, mBuffers[0], mCurBufOffset);
        MemFree(mBuffers[0]);
        mBuffers[0] = (char *)a;
        MemFree(mBuffers[1]);
        mBuffers[1] =
            (char *)_MemAllocTemp(mBufSize, __FILE__, 0x1EA, "ChunkStreamBuf", 0);
    }
    memcpy(mBuffers[0] + mCurBufOffset, data, bytes);
    mCurBufOffset += bytes;
}

void ChunkStream::ReadChunkAsync() {
    int bufIdx = 1;
    int idx;
    for (; bufIdx < 4; bufIdx++) {
#ifdef HX_NATIVE
        idx = (mCurBufferIdx + bufIdx) % 2;
#else
        idx = (mCurBufferIdx + bufIdx) % 3;
#endif
        if (mBuffersState[idx] == kInvalid)
            break;
    }
    if (mBuffersState[idx] == kInvalid) {
        int *thechunk = &mCurChunk[bufIdx];
        if (thechunk != mChunkEnd) {
            int thechunkval = *thechunk;
            int sizemask = thechunkval & kChunkSizeMask;
            if (mChunkInfo.mID != 0xCABEDEAF && !(thechunkval & 0x01000000)) {
                mFile->ReadAsync(mBuffers[idx] + mBufSize - sizemask, sizemask);
            } else {
                mFile->ReadAsync(mBuffers[idx], sizemask);
            }
            mBuffersOffset[idx] = &mCurChunk[bufIdx];
            mBuffersState[idx] = kReading;
        }
    }
}

void SetActiveChunkObject(Hmx::Object *obj) { gActiveChunkObject = obj; }

BinStream &ReadChunks(BinStream &bs, void *data, int total_len, int max_chunk_size) {
    int curr_size = 0;
    while (curr_size != total_len) {
        int len_left = Min(total_len - curr_size, max_chunk_size);
        char *dataAsChars = (char *)data;
        bs.Read(&dataAsChars[curr_size], len_left);
        curr_size += len_left;
#ifdef HX_NATIVE
        bs.WaitUntilReady();
#else
        while (bs.Eof() == TempEof)
            Timer::Sleep(0);
#endif
    }
    return bs;
}

ChunkStream::ChunkStream(
    const char *file,
    FileType type,
    int chunkSize,
    bool compress,
    Platform plat,
    bool cached
)
    : BinStream(false), mFile(nullptr), mFilename(file), mFail(false), mType(type),
      mChunkInfo(compress), mIsCached(cached), mBufSize(-1), mCurReadBuffer(nullptr),
      mRecommendedChunkSize(chunkSize), mLastWriteMarker(0), mCurBufferIdx(-1),
      mCurBufOffset(0), mChunkInfoPending(false), mCurChunk(nullptr), mChunkEnd(nullptr),
      mTell(0) {
    SetPlatform(plat);
    for (int bufCnt = 0; bufCnt < 3; bufCnt++) {
        mBuffersState[bufCnt] = kInvalid;
        mBuffersOffset[bufCnt] = 0;
        mBuffers[bufCnt] = 0;
    }
    mFile = NewFile(file, type == kRead ? 2 : 0x301);
    mFail = !mFile;
    if (!mFail) {
        if (type == kWrite) {
            mFile->Write(&mChunkInfo, 0x810);
            mBufSize = mRecommendedChunkSize * 2;
            mBuffers[0] =
                (char *)_MemAllocTemp(mBufSize, __FILE__, 0x144, "ChunkStreamBuf", 0);
            mBuffers[1] =
                (char *)_MemAllocTemp(mBufSize, __FILE__, 0x145, "ChunkStreamBuf", 0);
            mCurBufferIdx = 0;
        } else {
            mChunkInfoPending = true;
            mFile->ReadAsync(&mChunkInfo, 0x810);
        }
    }
}

ChunkStream::~ChunkStream() {
    if (mFail == false && mType == kWrite) {
        MaybeWriteChunk(true);
        if (mChunkInfo.mNumChunks == 0x200) {
            MILO_NOTIFY(
                "%s is %d compressed bytes too large",
                mFilename,
                mChunkInfo.mChunks[0x1ff]
            );
        }
        memset(
            &mChunkInfo.mChunks[mChunkInfo.mNumChunks],
            0,
            (0x200 - mChunkInfo.mNumChunks) * 4
        );
#ifndef HX_NATIVE
        for (int i = 0; i < mChunkInfo.mNumChunks; i++) {
            EndianSwapEq((unsigned int &)mChunkInfo.mChunks[i]);
        }
        EndianSwapEq((unsigned int &)mChunkInfo.mID);
        EndianSwapEq((unsigned int &)mChunkInfo.mChunkInfoSize);
        EndianSwapEq((unsigned int &)mChunkInfo.mNumChunks);
        EndianSwapEq((unsigned int &)mChunkInfo.mMaxChunkSize);
#endif
        mFile->Seek(0, 0);
        mFile->Write(&mChunkInfo, 0x810);
    }
    delete mFile;
    for (;;) {
        bool anyDecompressing;
        int i = 2;
        BufferState *statePtr = &mBuffersState[2];
        do {
            if (*statePtr == kDecompressing) {
                anyDecompressing = true;
                goto check_decomp;
            }
            i--;
            statePtr--;
        } while (i >= 0);
        anyDecompressing = false;
check_decomp:
        if (!anyDecompressing) break;
        gDataProcessedEvt.Wait(-1);
    }
    for (int i = 0; i < 3; i++) {
        MILO_ASSERT(mBuffersState[i] != kDecompressing, 0x194);
        MemFree(mBuffers[i]);
    }
}

bool ChunkStream::Cached() const { return mIsCached; }
Platform ChunkStream::GetPlatform() const { return mPlatform; }
bool ChunkStream::Fail() { return mFail; }
const char *ChunkStream::Name() const { return mFilename.c_str(); }

void ChunkStream::ReadImpl(void *data, int bytes) {
    MILO_ASSERT(mCurBufferIdx != -1, 0x1D3);
    MILO_ASSERT(mBuffersState[mCurBufferIdx] == kReady, 0x1D4);
    MILO_ASSERT(mBuffersOffset[mCurBufferIdx] == mCurChunk, 0x1D5);
#ifdef HX_NATIVE
    // Handle cross-chunk reads on native: if the read spans beyond the current
    // chunk, copy what's available, advance to the next chunk, and continue.
    char *dst = (char *)data;
    int remaining = bytes;
    while (remaining > 0) {
        int chunkSize = *mCurChunk & kChunkSizeMask;
        int available = chunkSize - mCurBufOffset;
        if (available <= 0) {
            // Current chunk exhausted, advance to next
            EofType eof = Eof();
            if (eof == RealEof) {
                // Past end of stream — zero remaining, mark failed to prevent
                // infinite caller loops (MILO_FAIL doesn't halt on native)
                memset(dst, 0, remaining);
                mTell += remaining;
                mFail = true;
                return;
            }
            if (eof == TempEof) {
                // On native all I/O is synchronous, TempEof should not happen.
                // Treat as RealEof to prevent infinite spin.
                memset(dst, 0, remaining);
                mTell += remaining;
                mFail = true;
                return;
            }
            continue; // NotEof — chunk advanced, re-check
        }
        int toRead = (remaining < available) ? remaining : available;
        memcpy(dst, mCurReadBuffer + mCurBufOffset, toRead);
        dst += toRead;
        mCurBufOffset += toRead;
        mTell += toRead;
        remaining -= toRead;
    }
#else
    MILO_ASSERT(mCurBufOffset + bytes <= (*mCurChunk & kChunkSizeMask), 0x1D6);
    memcpy(data, (void *)(mCurReadBuffer + mCurBufOffset), bytes);
    mCurBufOffset += bytes;
    mTell += bytes;
#endif
}

void ChunkStream::SeekImpl(int, SeekType) { MILO_FAIL("Can't seek on chunkstream"); }

int ChunkStream::Tell() {
    if (mType == kRead) {
        return mTell;
    } else {
        MILO_FAIL("Can't tell on chunkstream");
        return 0;
    }
}

EofType ChunkStream::Eof() {
    MILO_ASSERT(!mFail && mType == kRead, 0x22c);
    if (mChunkInfoPending) {
        int x;
        if (mFile->ReadDone(x) == 0)
            return TempEof;
        mChunkInfoPending = false;
#ifndef HX_NATIVE
        // On Xbox (BE host), swap chunk header from LE file format to BE host order.
        // On native x86_64 (LE host), chunk header is already in native byte order.
        EndianSwapEq((unsigned int &)mChunkInfo.mID);
        EndianSwapEq((unsigned int &)mChunkInfo.mChunkInfoSize);
        EndianSwapEq((unsigned int &)mChunkInfo.mNumChunks);
        EndianSwapEq((unsigned int &)mChunkInfo.mMaxChunkSize);
        for (int i = 0; i < mChunkInfo.mNumChunks; i++) {
            EndianSwapEq((unsigned int &)mChunkInfo.mChunks[i]);
        }
#endif
        if ((mChunkInfo.mID & 0xf0ffffff) != kChunkIDMask) {
            mChunkInfo.mID = 0xCABEDEAF;
            mChunkInfo.mChunkInfoSize = 0;
            mChunkInfo.mNumChunks = 1;
            mChunkInfo.mMaxChunkSize = mFile->Size();
            MILO_ASSERT((mChunkInfo.mMaxChunkSize & ~kChunkSizeMask) == 0, 0x24b);
            mChunkInfo.mChunks[0] = mChunkInfo.mMaxChunkSize;
        }

        if (strstr(mFilename.c_str(), ".milo_")) {
            mIsCached = true;
            if (strstr(mFilename.c_str(), ".milo_xbox"))
                SetPlatform(kPlatformXBox);
            else if (strstr(mFilename.c_str(), ".milo_ps3"))
                SetPlatform(kPlatformPS3);
            else if (strstr(mFilename.c_str(), ".milo_wii"))
                SetPlatform(kPlatformWii);
            else
                SetPlatform(kPlatformPC);
        } else {
            mIsCached = false;
            SetPlatform(kPlatformPC);
        }

        mBufSize = mChunkInfo.mMaxChunkSize;
        if (mChunkInfo.mID != 0xCABEDEAF)
            mBufSize += 0x800;
        int cap = Min(2, mChunkInfo.mNumChunks);
        for (int i = 0; i < cap; i++) {
            mBuffers[i] = (char *)_MemAllocTemp(mBufSize, __FILE__, 0x104, "ChunkStreamBuf", 0);
        }
        int *chunks = mChunkInfo.mChunks;
        mChunkEnd = chunks + mChunkInfo.mNumChunks;
        mCurChunk = chunks - 1;
        mCurBufOffset = mChunkInfo.mMaxChunkSize & kChunkSizeMask;
        mCurBufferIdx = 1;
        mFile->Seek(mChunkInfo.mChunkInfoSize, 0);
        ReadChunkAsync();
    }

    if (mCurBufOffset < (*mCurChunk & kChunkSizeMask)) {
        return NotEof;
    } else {
        MILO_ASSERT(mCurBufOffset == (*mCurChunk & kChunkSizeMask), 0x28B);
        if (mBuffersOffset[mCurBufferIdx] == mCurChunk) {
            mBuffersState[mCurBufferIdx] = kInvalid;
        }
        if (mCurChunk + 1 == mChunkEnd)
            return RealEof;
        else {
            int x;
            if (mFile->ReadDone(x)) {
                DecompressChunkAsync();
                ReadChunkAsync();
                PollDecompressionWorker();
            }
            int idx = (mCurBufferIdx + 1) % 2;
            if (mBuffersState[idx] != kReady)
                return TempEof;
            else {
                mCurBufferIdx = idx;
                mCurChunk++;
                mCurBufOffset = 0;
                mCurReadBuffer = mBuffers[idx];
#ifdef HX_NATIVE
                int chunkSz = *mCurChunk & kChunkSizeMask;
                for (int dbg = 0; dbg < chunkSz && dbg < 8; dbg++)
#endif
                return NotEof;
            }
        }
    }
}

int ChunkStream::WriteChunk() {
    MILO_ASSERT(mCurBufOffset < kChunkSizeMask, 784);
    int size = mCurBufOffset;
    int flags = 0;
    int *firstbuf = (int *)mBuffers[0];
    if (mChunkInfo.mID == 0xCDBEDEAF) {
        int l38 = mBufSize - 4;
        unsigned int *secondbuf = (unsigned int *)mBuffers[1];
        *secondbuf = size;
#ifndef HX_NATIVE
        EndianSwapEq(*secondbuf);
#endif
        CompressMem(mBuffers[0], size, secondbuf + 1, l38, 0);
        if (((float)mCurBufOffset / (float)l38) > 1.1f && mChunkInfo.mNumChunks != 0) {
            size = l38 + 4;
            firstbuf = (int *)secondbuf;
        } else
            flags |= 0x1000000;
    }
    if (mFile->Write(firstbuf, size) != size) {
        mFail = true;
    }
    MILO_ASSERT((size & ~kChunkSizeMask) == 0, 826);
    MILO_ASSERT((flags & (kChunkSizeMask|kChunkUnusedMask)) == 0, 828);
    int result = size | flags;
    MILO_ASSERT((result & kChunkUnusedMask) == 0, 833);
    return result;
}

BinStream &MarkChunk(BinStream &bs) {
    ChunkStream *cs = dynamic_cast<ChunkStream *>(&bs);
    if (cs)
        cs->PotentiallyWriteChunk(false);
    return bs;
}

void DecompressMemHelper(
    const void *compressedMem, int size, void *dst, int &dstLen, const char *c
) {
    unsigned int rawSize = *(unsigned int *)compressedMem;
    DecompressMem((const char *)compressedMem + 4, size - 4, dst, dstLen, c);
#ifdef HX_NATIVE
    int expectedDstLen = rawSize;
#else
    int expectedDstLen = EndianSwap(rawSize);
#endif
    MILO_ASSERT(dstLen == expectedDstLen, 0x3bb);
}

void ChunkStream::DecompressChunk(DecompressTask &task) {
    MILO_ASSERT(*task.mState == kDecompressing, 0x3c1);
    int data = *task.mChunk;
    int dataMsk = data & kChunkSizeMask;
    MILO_ASSERT((data & ~kChunkSizeMask) == 0, 0x3c5);
    int out_len = task.mDecompressedSize;
    int id = task.mID;
    if (id == 0xCDBEDEAF) {
        char *dataOffset = (char *)task.mBuffer + (out_len - dataMsk);
        DecompressMemHelper(dataOffset, dataMsk, task.mBuffer, out_len, task.mTempBuf);
    } else if (id == 0xCCBEDEAF) {
        char *dataOffset = (char *)task.mBuffer + (out_len - dataMsk) + 10;
        DecompressMem(dataOffset, dataMsk - 0x12, task.mBuffer, out_len, task.mTempBuf);
    } else {
        MILO_ASSERT(task.mID == CHUNKSTREAM_Z_ID, 0x3d7);
        char *dataOffset = (char *)task.mBuffer + (task.mDecompressedSize - dataMsk);
        DecompressMem(dataOffset, dataMsk, task.mBuffer, out_len, task.mTempBuf);
    }
    *task.mChunk = out_len;
    *task.mState = kReady;
}

void ChunkStream::DecompressChunkAsync() {
    int bufIdx = 1;
    int idx;
    for (; bufIdx < 4; bufIdx++) {
#ifdef HX_NATIVE
        idx = (mCurBufferIdx + bufIdx) % 2;
#else
        idx = (mCurBufferIdx + bufIdx) % 3;
#endif
        if (mBuffersState[idx] == kReading)
            break;
    }

    if (mBuffersState[idx] == kReading) {
        unsigned int chunkBits = (unsigned int)mCurChunk[bufIdx];
        bool maskexists = (chunkBits >> 24) & 1;
        if (mChunkInfo.mID != 0xCABEDEAF && !maskexists) {
            mBuffersState[idx] = kDecompressing;
            DecompressTask dtask;
            dtask.mChunk = &mCurChunk[bufIdx];
            dtask.mBuffer = mBuffers[idx];
            dtask.mState = &mBuffersState[idx];
            dtask.mDecompressedSize = mBufSize;
            dtask.mID = mChunkInfo.mID;
            dtask.mTempBuf = (char *)mFilename.c_str();
#ifdef HX_NATIVE
            // Decompress synchronously on native — no background thread needed
            DecompressChunk(dtask);
#else
            {
                CritSecTracker tracker(&gDecompressionCritSec);
                gDecompressionQueue.push_back(dtask);
            }
            StartDecompressionThread();
#endif
        } else {
            mBuffersState[idx] = kReady;
        }
    }
}

bool ChunkStream::PollDecompressionWorker() {
    CritSecTracker tracker(&gDecompressionCritSec);
    unsigned int counter = 0;
    for (std::list<DecompressTask>::iterator it = gDecompressionQueue.begin(); it != gDecompressionQueue.end(); it++) {
        counter++;
    }
    if (counter != 0) {
        DecompressTask task;
        memcpy(&task, &gDecompressionQueue.front(), sizeof(DecompressTask));
        gDecompressionQueue.erase(gDecompressionQueue.begin());
        tracker.mCritSec = 0;
        gDecompressionCritSec.Exit();
        DecompressChunk(task);
        return true;
    }
    return false;
}

BinStream &WriteChunks(BinStream &bs, const void *v, int i1, int i2) {
    for (int i = 0; i != i1;) {
        int temp = i1 - i;
        if (i2 < temp) {
            temp = i2;
        }
        bs.Write((void *)(i + (intptr_t)v), temp);
        i += temp;
        if (bs.GetPlatform() == kPlatformWii) {
            MarkChunk(bs);
        }
    }
    return bs;
}

void ChunkStream::MaybeWriteChunk(bool b) {
    if (mChunkInfo.mNumChunks < 2 && 0x2000 <= mCurBufOffset) {
        b = true;
    }
    if (mCurBufOffset >= mRecommendedChunkSize || b) {
        bool maxchunks = mChunkInfo.mNumChunks == 511;
        if (!b && maxchunks) {
            return;
        }
        if ((mCurBufOffset >= mRecommendedChunkSize + 0x2000)
            && (0x2000 <= mLastWriteMarker) && !maxchunks) {
            int __n = mCurBufOffset - mLastWriteMarker;
            void *__dest = _MemAllocTemp(__n, __FILE__, 0x2e6, "ChunkStreamBuf", 0);
            memcpy(__dest, mBuffers[0] + mLastWriteMarker, __n);
            mCurBufOffset = mLastWriteMarker;
            mLastWriteMarker = 0;
            MaybeWriteChunk(true);
            mCurBufOffset = __n;
            memcpy(mBuffers[0], __dest, __n);
            MemFree(__dest);
            if (!b) {
                return;
            }
        }
        MILO_ASSERT_FMT(
            mChunkInfo.mNumChunks < 512,
            "%s has %d chunks, max is %d",
            mFilename,
            mChunkInfo.mNumChunks,
            512
        );
        unsigned int wrote = WriteChunk();
        int mask = wrote & kChunkSizeMask;
        mChunkInfo.mChunks[mChunkInfo.mNumChunks++] = wrote;
        mChunkInfo.mMaxChunkSize =
            Max<int>(mask, mCurBufOffset, mChunkInfo.mMaxChunkSize);
        mCurBufOffset = 0;
    }
    mLastWriteMarker = mCurBufOffset;
}
