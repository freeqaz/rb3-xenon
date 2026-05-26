#include "os/HDCache.h"
#include "math/SHA1.h"
#include "math/Utl.h"
#include "os/Archive.h"
#include "os/CritSec.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/OSFuncs.h"
#include "os/System.h"
#include "utl/BinStream.h"
#include "utl/FileStream.h"
#include "utl/HxGuid.h"
#include "utl/MemStream.h"
#include "utl/Option.h"

HDCache TheHDCache;

HDCache::HDCache()
    : mBlockState(0), mWriteFileIdx(0), mWriteBlock(-1), mWritingHeader(false),
      mReadFileIdx(0), mDirtyCache(0), mLastHdrWriteMs(-1), mLastCacheWriteMs(-1),
      mLockId(-1), mLockCount(0), mCritSec(nullptr), mHdrIdx(0), mHdrBuf(nullptr),
      unk64(false) {}

HDCache::~HDCache() {}

bool HDCache::LockCache() {
    CritSecTracker cst(mCritSec);
    if (mLockId == -1 || mLockId == GetCurrentThreadId()) {
        mLockId = GetCurrentThreadId();
        mLockCount++;
        return true;
    } else {
        return false;
    }
}

void HDCache::UnlockCache() {
    CritSecTracker cst(mCritSec);
    MILO_ASSERT(mLockId == CurrentThreadId(), 0xfa);
    mLockCount--;
    if (mLockCount == 0)
        mLockId = -1;
}

int HDCache::HdrSize() {
    int blockStateSize = 32;
    int numArkfiles = TheArchive->NumArkFiles();
    for (int i = 0; i < numArkfiles; i++) {
        if (TheArchive->GetArkfileCachePriority(i) >= 0) {
            int numBlocks = TheArchive->GetArkfileNumBlocks(i) + 0x1F;
            blockStateSize += (numBlocks / 32 + 1) * 4;
        }
    }
    int ret = blockStateSize + 0x100;
    int remainder = ret - ((ret / 4096) << 12);
    if (remainder != 0) {
        ret = ret - remainder + 0x1000;
    }
    return ret;
}

bool HDCache::ReadFail() {
    File *file = mReadArkFiles[mReadFileIdx];
    if (file && file->Fail()) {
        MILO_LOG("HDCache Read %d failed\n", mReadFileIdx);
        return true;
    } else
        return false;
}

bool HDCache::ReadDone() {
    File *file = mReadArkFiles[mReadFileIdx];
    if ((int)file == 0) {
        return true;
    }
    return file->ReadDone();
}

bool HDCache::WriteDone() {
    if (mWriteBlock >= 0) {
        if (mWriteArkFiles[mWriteFileIdx]->WriteDone()) {
            MILO_ASSERT(mReadArkFiles[mWriteFileIdx]->Size() == mWriteArkFiles[mWriteFileIdx]->Size(), 499);
            UnlockCache();
            if (mWriteArkFiles[mWriteFileIdx]->Fail()) {
                MILO_LOG("HDCache Write %d.%d failed\n", mWriteFileIdx, mWriteBlock);
            } else {
                if (++mDirtyCache == 1) {
                    mLastHdrWriteMs = SystemMs();
                }
                mBlockState[mWriteFileIdx][mWriteBlock / 32] |= 1 << mWriteBlock;
            }
            mWriteBlock = -1;
        }
    }
    return mWriteBlock == -1;
}

void HDCache::Poll() {
    if (mWritingHeader) {
        if (mHdr[mHdrIdx]->WriteDone()) {
            UnlockCache();
            if (mHdr[mHdrIdx]->Fail()) {
                MILO_LOG("HDCache Write Header Failed\n");
            }
            Flush();
            mWritingHeader = false;
        }
    }
    if (mDirtyCache && !mWritingHeader
        && (mDirtyCache > 0x400 || SystemMs() - mLastHdrWriteMs > 60000)) {
        WriteHdr();
    }
}

bool HDCache::ReadAsync(int arkfileNum, int blockNum, void *ptr) {
    MILO_ASSERT(ReadDone(), 0x191);
    if (mBlockState[arkfileNum]) {
        MILO_ASSERT(blockNum < TheArchive->GetArkfileNumBlocks(arkfileNum), 0x196);
        if ((mBlockState[arkfileNum][(blockNum / 32)] & (1 << (blockNum % 32)))) {
            MILO_ASSERT(mReadArkFiles[arkfileNum]->Size() >= ((blockNum + 1) * kArkBlockSize), 0x19D);
            mReadFileIdx = arkfileNum;
            mReadArkFiles[arkfileNum]->Seek(blockNum * kArkBlockSize, 0);
            return mReadArkFiles[mReadFileIdx]->ReadAsync(ptr, kArkBlockSize);
        }
    }
    return false;
}

bool HDCache::WriteAsync(int arkfileNum, int blockNum, const void *ptr) {
    MILO_ASSERT(WriteDone(), 0x191);
    if (mBlockState[arkfileNum]) {
        MILO_ASSERT(blockNum < TheArchive->GetArkfileNumBlocks(arkfileNum), 0x196);
        if ((mBlockState[arkfileNum][(blockNum / 32)] & (1 << (blockNum % 32))) > 0) {
            MILO_ASSERT(mWriteArkFiles[arkfileNum]->Size() >= ((blockNum + 1) * kArkBlockSize), 0x19D);
            mWriteFileIdx = arkfileNum;
            mWriteArkFiles[arkfileNum]->Seek(blockNum * kArkBlockSize, 0);
            return mWriteArkFiles[mWriteFileIdx]->WriteAsync(ptr, kArkBlockSize);
        }
    }
    return false;
}

// void HDCache::Init() {}

FileStream *HDCache::OpenHeader() {
    if (mHdrFmt.empty())
        return nullptr;
    else {
        const char *str;
        int i;
        for (i = 0; i < 2; i++) {
            str = MakeString(mHdrFmt.c_str(), 0);
            if (FileExists(str, 0x10000, nullptr))
                break;
        }
        if (!(i != 2)) {
            return nullptr;
        } else {
            return new FileStream(str, FileStream::kReadNoArk, true);
        }
    }
}

void HDCache::WriteHdr() {
    if (!mHdr[mHdrIdx]->Fail() && LockCache()) {
        MILO_ASSERT(mHdr[mHdrIdx]->WriteDone(), 0x144);
        CSHA1 sha;
        mHdrBuf->Seek(0, BinStream::kSeekBegin);
        mHdrBuf->EnableWriteEncryption();
        *mHdrBuf << 2;
        HxGuid guid;
        TheArchive->GetGuid(guid);
        *mHdrBuf << guid;
        int numArkfiles = TheArchive->NumArkFiles();
        *mHdrBuf << numArkfiles;
        for (int i = 0; i < numArkfiles; i++) {
            int blockSize = 0;
            if (mBlockState[i]) {
                int arkBlocks = TheArchive->GetArkfileNumBlocks(i);
                int numBlocks = arkBlocks + 0x1F;
                blockSize = (numBlocks / 32) * 4;
            }
            *mHdrBuf << blockSize;
            if (blockSize > 0) {
                mHdrBuf->Write(mBlockState[i], blockSize);
                sha.Update((const unsigned char *)mBlockState[i], blockSize);
            }
        }
        char buf[256];
        buf[0] = 0;
        memset(&buf[1], 0, 255);
        sha.Final().ReportHash(buf, 0);
        mHdrBuf->Write(buf, 256);
        mHdrBuf->DisableEncryption();
        mDirtyCache = 0;
        int finalSize = HdrSize();
        MILO_ASSERT(mHdrBuf->Size() <= finalSize, 0x176);
        char zeroPad[0x80];
        memset(zeroPad, 0, 0x80);
        while (mHdrBuf->Size() < finalSize) {
            int size = finalSize - mHdrBuf->Size();
            if (size > 0x80U)
                size = 0x80;
            mHdrBuf->Write(zeroPad, size);
        }
        MILO_ASSERT(mHdrBuf->Size() == finalSize, 0x183);
        int oldSize = mHdr[mHdrIdx]->Size();
        int newSize = mHdrBuf->Size();
        MILO_ASSERT(oldSize == newSize, 0x186);
        mWritingHeader = true;
        mHdr[mHdrIdx]->Seek(0, 0);
        mHdr[mHdrIdx]->WriteAsync(mHdrBuf->Buffer(), mHdrBuf->Size());
    }
}

void HDCache::OpenFiles(int numCachedArkfiles) {
    if (mFileFmt.empty())
        return;
    int numArkfiles = TheArchive->NumArkFiles();
    MILO_ASSERT(numCachedArkfiles <= numArkfiles, 0x22F);
    FileMkDir(FileGetPath(mFileFmt.c_str()));
    std::vector<int> pendingArkfiles;
    for (int i = 0; i < numArkfiles; i++) {
        const char *fileFmt = MakeString(mFileFmt.c_str(), i);
        bool exists = FileExists(fileFmt, 0x10000, nullptr);
        int prio = TheArchive->GetArkfileCachePriority(i);
        if (exists && i > numCachedArkfiles) {
            FileDelete(fileFmt);
        }
        if (prio >= 0) {
            pendingArkfiles.push_back(i);
        }
    }
    const char *hdrFmt = MakeString(mHdrFmt.c_str(), 0);
    mHdr[0] = NewFile(hdrFmt, 0x50101);
    bool hdrValid = mHdr[0] && !mHdr[0]->Fail();
    if (hdrValid) {
        int hdrSize = HdrSize();
        mHdr[0]->Truncate(hdrSize);
        RELEASE(mHdr[0]);
        mHdr[0] = NewFile(hdrFmt, 0x50001);
        hdrValid = (hdrSize - mHdr[0]->Size()) == 0;
    }
    if (!hdrValid) {
        RELEASE(mHdr[0]);
    } else {
        while (pendingArkfiles.size() != 0) {
            int i5 = -1;
            auto max = pendingArkfiles.begin();
            MILO_ASSERT(max != pendingArkfiles.end(), 0x26F);
            // there's a pendingArkfiles iteration somewhere here
            const char *fileFmt = MakeString(mFileFmt.c_str(), i5);
            File *file = NewFile(fileFmt, 0x50101);
            pendingArkfiles.erase(max);
        }
        for (int i = 0; i < numArkfiles; i++) {
            const char *fileFmt = MakeString(mFileFmt.c_str(), i);
            File *write = NewFile(fileFmt, 0x50002);
            File *read = NewFile(fileFmt, 0x50001);
            if (write && read && !write->Fail() && !read->Fail()) {
                mReadArkFiles[i] = read;
                mWriteArkFiles[i] = write;
            }
        }
    }
}

void HDCache::Init() {
    mCritSec = new CriticalSection();
    if (TheArchive) {
        if (OptionBool("no_hdcache", true)) {
            Flush();
        }
        int numArkfiles = TheArchive->NumArkFiles();
        mReadArkFiles.resize(numArkfiles);
        mWriteArkFiles.resize(numArkfiles);
        FileStream *header = OpenHeader();
        bool next = header && header->Size() == HdrSize();
        if (next) {
            header->EnableReadEncryption();
            int version;
            *header >> version;
            next = version == 2;
        }
        if (next) {
            HxGuid guid1, guid2;
            *header >> guid1;
            TheArchive->GetGuid(guid2);
            next = guid1 == guid2;
        }
        int numFilesToOpen = 0;
        if (next) {
            *header >> numFilesToOpen;
            if (numFilesToOpen < 0 || numFilesToOpen > numArkfiles) {
                numFilesToOpen = 0;
                next = false;
            }
        }
        OpenFiles(numFilesToOpen);
        mBlockState = new int *[numArkfiles];
        CSHA1 sha;
        unsigned char blockBuf[0x1000];
        for (int i = 0; i < numArkfiles; i++) {
            unsigned int blockSize = 0;
            if (i < numFilesToOpen) {
                *header >> blockSize;
                if (blockSize <= 0x1000 && (blockSize & 3) == 0) {
                    header->Read(blockBuf, blockSize);
                } else {
                    next = false;
                }
                if (header->Fail() || !next) {
                    blockSize = 0;
                    next = false;
                    numFilesToOpen = 0;
                }
                if (next) {
                    sha.Update(blockBuf, blockSize);
                }
            }
            // Check if read/write files are valid
            File **readFiles = &mReadArkFiles[0];
            File **writeFiles = &mWriteArkFiles[0];
            if (readFiles[i] == NULL || readFiles[i]->Fail() ||
                writeFiles[i] == NULL || writeFiles[i]->Fail()) {
                File *rf = readFiles[i];
                if (rf != NULL) { delete rf; }
                readFiles[i] = NULL;
                File *wf = writeFiles[i];
                if (wf != NULL) { delete wf; }
                writeFiles[i] = NULL;
            }
            if (readFiles[i] != NULL) {
                auto _tmp6 = TheArchive->GetArkfileNumBlocks(i);
                int numDwords = (_tmp6 + 0x1F) / 32;
                int *blockMem = new int[numDwords];
                memcpy(blockMem, blockBuf, blockSize);
                memset((char *)blockMem + blockSize, 0, numDwords * 4 - blockSize);
                mBlockState[i] = blockMem;
            } else {
                mBlockState[i] = NULL;
            }
        }
        bool hashValid = false;
        if (next) {
            char hash1[256], hash2[256];
            memset(hash1, 0, 256);
            memset(hash2, 0, 256);
            sha.Final().ReportHash(hash1, 0);
            header->Read(hash2, 0x100);
            if (!header->Fail()) {
                auto _tmp0 = memcmp(hash1, hash2, 256);
                hashValid = _tmp0 == 0;
            }
        }
        bool skipHdcache = OptionBool("skip_hdcache", false);
        if (!skipHdcache & hashValid) {
            unk64 = true;
            TheDebug << MakeString("Using the archive cache\n");
        } else {
            for (int i = 0; i < numArkfiles; i++) {
                if (mBlockState[i] != NULL) {
                    int numDwords = (TheArchive->GetArkfileNumBlocks(i) + 0x1F) / 32;
                    memset(mBlockState[i], 0, numDwords * 4);
                }
            }
        }
        if (header != NULL) {
            delete header;
        }
        mHdrFmt = "";
        mFileFmt = "";
        mHdrBuf = new MemStream(true);
    }
}
