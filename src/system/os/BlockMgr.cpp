#include "CDReader.h"
#include "obj/DataFunc.h"
#include "os/Archive.h"
#include "os/Block.h"
#include "os/Debug.h"
#include "os/HDCache.h"
#include "os/OSFuncs.h"
#include "os/System.h"
#include "utl/MemMgr.h"

#define kNumBlockBuffers 0x13

static char gBuffers[kNumBlockBuffers * 0x10000];
int gCurrBuffNum;
int Block::sCurrTimestamp;
Timer gReadTime;

BlockMgr TheBlockMgr;

namespace {
    bool gReadHD = false;
    char *gTempBlock;
    static DataNode OnSpinUp(DataArray *) { return TheBlockMgr.SpinUp(); }
    __declspec(noinline) static int ReadError() {
        if (gReadHD) {
            return TheHDCache.ReadFail();
        }
        return 1;
    }
}

static int gReadCount;
static int gSeekCount;
static float gAccumSeekTime;
static int gLastBlockNum = -1;
static int gLastArkFileNum = -1;

int GetFreeBuffer() {
    MILO_ASSERT(gCurrBuffNum < kNumBlockBuffers, 0x46);
    return gCurrBuffNum++;
}

Block::Block()
    : mBuffer(0), mArkfileNum(-1), mBlockNum(-1), mTimestamp(-1), mWritten(true),
      mDebugName("") {
    mBuffer = &gBuffers[GetFreeBuffer() * 0x10000];
    UpdateTimestamp();
}

void Block::UpdateTimestamp() { mTimestamp = ++sCurrTimestamp; }

BlockRequest::BlockRequest(const BlockRequest &other)
    : mArkfileNum(other.mArkfileNum), mBlockNum(other.mBlockNum),
      mStr(other.mStr), mTasks(other.mTasks) {}

BlockRequest::BlockRequest(const AsyncTask &task)
    : mArkfileNum(task.GetArkfileNum()), mBlockNum(task.GetBlockNum()),
      mStr(task.GetStr()) {
    mTasks.push_back(task);
}

void BlockMgr::Init() {
    gTempBlock = (char *)MemAlloc(0x1000, __FILE__, 0xB7, "BlockMgr junk", 4);
    gCurrBuffNum = 0;
    mBlockCache.resize(kNumBlockBuffers);
    mReadingBlock = nullptr;
    for (int i = 0; i < mBlockCache.size(); i++) {
        mBlockCache[i] = new Block();
    }
    TheHDCache.Init();
    DataRegisterFunc("disc_spin_up", OnSpinUp);
}

void BlockMgr::ReadBlock() {
    MILO_ASSERT(mReadingBlock, 0x174);
    bool err;
    char *buf = (char *)mReadingBlock->Buffer();
    int arkNum = mReadingBlock->ArkFileNum();
    int blockNum = mReadingBlock->BlockNum();
    if (TheHDCache.ReadAsync(arkNum, blockNum, buf)) {
        gReadHD = true;
        err = false;
    } else {
        gReadHD = false;
        err = CDRead(arkNum, blockNum * 32, 32, buf);
    }
    if (!err) {
        mReadingBlock->UpdateTimestamp();
    } else {
        MILO_LOG("CD READING ERROR: %x\n", err);
        mReadingBlock = nullptr;
    }
}

void BlockMgr::MarkDiscRead() { mSpinDownTimer.Restart(); }

void BlockMgr::WriteBlock() {
    MILO_ASSERT(!mWritingBlock, 0x161);
    for (Block *block = FindLRUBlock(true); block != nullptr;
         block = FindLRUBlock(true)) {
        block->SetWritten();
        if (TheHDCache.WriteAsync(
                block->ArkFileNum(), block->BlockNum(), block->Buffer()
            )) {
            mWritingBlock = block;
            return;
        }
    }
}

Block *BlockMgr::FindBlock(int arknum, int blocknum) {
    for (int i = 0; i < mBlockCache.size(); i++) {
        if (mBlockCache[i]->CheckMetadata(arknum, blocknum))
            return mBlockCache[i];
    }
    return nullptr;
}

Block *BlockMgr::FindLRUBlock(bool b) {
    Block *ret = 0;
    int time = Block::sCurrTimestamp;
    for (int i = 0; i < mBlockCache.size(); i++) {
        if (mBlockCache[i] != mWritingBlock && mBlockCache[i] != mReadingBlock) {
            if (b) {
                if (mBlockCache[i]->mWritten) continue;
            }
            if (mBlockCache[i]->mTimestamp < time) {
                ret = mBlockCache[i];
                time = mBlockCache[i]->mTimestamp;
            }
        }
    }
    return ret;
}

Block *BlockMgr::FindMRUBlock() {
    int time = -1;
    Block *ret = nullptr;
    for (int i = 0; i < mBlockCache.size(); i++) {
        if (mBlockCache[i]->Timestamp() > time) {
            ret = mBlockCache[i];
            time = mBlockCache[i]->Timestamp();
        }
    }
    return ret;
}

char *BlockMgr::GetBlockData(int ark, int blk) {
    Block *blokc = FindBlock(ark, blk);
    if (blokc != nullptr && blokc != mReadingBlock) {
        blokc->UpdateTimestamp();
        return (char *)blokc->Buffer();
    }
    return nullptr;
}

void BlockMgr::GetAssociatedBlocks(
    unsigned long long offset, int bytes, int &startBlock, int &numBlocks, int &blockSize
) {
    blockSize = 0x10000;
    startBlock = (int)(offset >> 16);
    int remaining = (int)(offset & 0xFFFF) + bytes - 0x10000;
    if (remaining > 0) {
        int extraBlocks = remaining / 0x10000;
        numBlocks = extraBlocks + 1;
        if (remaining % 0x10000 != 0) {
            numBlocks++;
        }
        return;
    }
    numBlocks = 1;
}

void BlockMgr::AddTask(const AsyncTask &task) {
    std::list<BlockRequest>::iterator it;
    int arkNum = task.GetArkfileNum();
    int blockNum = task.GetBlockNum();
    for (it = mRequests.begin(); it != mRequests.end(); ++it) {
        bool match = (arkNum == it->mArkfileNum && blockNum == it->mBlockNum);
        if (match) {
            it->mTasks.push_back(task);
            break;
        }
        int itArk = it->mArkfileNum;
        bool exceeds = (itArk > arkNum || (itArk == arkNum && it->mBlockNum > blockNum));
        if (exceeds) {
            mRequests.insert(it, BlockRequest(task));
            break;
        }
    }
    if (it == mRequests.end()) {
        mRequests.push_back(BlockRequest(task));
    }
}

void BlockMgr::KillBlockRequests(ArkFile *arkFile) {
    std::list<BlockRequest>::iterator end = mRequests.end();
    std::list<BlockRequest>::iterator it = mRequests.begin();
    while (it != end) {
        std::list<AsyncTask>::iterator taskIt = it->mTasks.begin();
        while (taskIt != it->mTasks.end()) {
            if (taskIt->GetOwner() == arkFile) {
                taskIt = it->mTasks.erase(taskIt);
            } else {
                ++taskIt;
            }
        }
        if (it->mTasks.size() == 0) {
            if (mReadingBlock
                && mReadingBlock->CheckMetadata(it->mArkfileNum, it->mBlockNum)) {
                ++it;
            } else {
                it = mRequests.erase(it);
            }
        } else {
            ++it;
        }
    }
}

void BlockMgr::Poll() {
    if (!MainThread())
        return;

    TheHDCache.Poll();
    mSpinDownTimer.Split();

    if (mWritingBlock && TheHDCache.WriteDone()) {
        mWritingBlock = nullptr;
        WriteBlock();
    }

    if (mReadingBlock) {
        gReadCount++;
        int err = ReadError();
        if (err != 0) {
            MILO_LOG(" CD READING ERROR!!!  %x\n", err);
            ReadBlock();
            return;
        }
        bool readDone;
        if (gReadHD) {
            readDone = TheHDCache.ReadDone();
        } else {
            readDone = CDReadDone();
        }
        if (readDone) {
            if (Archive::DebugArkOrder()) {
                gReadTime.Split();
                int seekDist = mReadingBlock->mBlockNum - gLastBlockNum;
                if (mReadingBlock->mArkfileNum != gLastArkFileNum) {
                    seekDist = 99999;
                }
                if (seekDist != 1) {
                    gSeekCount++;
                    gAccumSeekTime += gReadTime.Ms();
                } else {
                    gSeekCount = 0;
                    gAccumSeekTime = 0.0f;
                }
                if (gSeekCount >= 1 || gAccumSeekTime >= 240.0f) {
                    char debugName[100];
                    strncpy(debugName, mReadingBlock->mDebugName, 99);
                    debugName[99] = '\0';
                    MILO_LOG(
                        "BlockMgr Seek: Ark: %2d  Dist: %5d  Seek Time: %3.0f ms  Suspect: %s\n",
                        mReadingBlock->mArkfileNum,
                        seekDist,
                        gAccumSeekTime,
                        debugName
                    );
                }
                gLastBlockNum = mReadingBlock->mBlockNum;
                gLastArkFileNum = mReadingBlock->mArkfileNum;
            }
            if (!gReadHD) {
                mSpinDownTimer.Restart();
            }
            mReadingBlock->UpdateTimestamp();

            std::list<BlockRequest>::iterator request = mRequests.begin();
            while (request != mRequests.end()) {
                if (mReadingBlock->CheckMetadata(request->mArkfileNum, request->mBlockNum))
                    break;
                ++request;
            }
            MILO_ASSERT(request != mRequests.end(), 0x1eb);

            mReadingBlock = nullptr;
            for (std::list<AsyncTask>::iterator taskIt = request->mTasks.begin();
                 taskIt != request->mTasks.end(); ++taskIt) {
                taskIt->FillData();
            }
            mRequests.erase(request);
            if (!mWritingBlock) {
                WriteBlock();
            }
        }
    }

    if (mReadingBlock)
        return;
    if (mRequests.size() == 0)
        return;

    BlockRequest &nextReq = mRequests.front();
    Block *block = FindLRUBlock(false);
    int blocknum = nextReq.mBlockNum;
    int arkfilenum = nextReq.mArkfileNum;
    const char *str = nextReq.mStr;

    MILO_ASSERT(blocknum != -1, 0x20f);

    mReadingBlock = block;
    mReadingBlock->mBlockNum = blocknum;
    mReadingBlock->mArkfileNum = arkfilenum;
    mReadingBlock->mWritten = false;
    mReadingBlock->mDebugName = str;

    gReadTime.Restart();
    gReadCount = 0;

    ReadBlock();
}

bool BlockMgr::SpinUp() {
    TheBlockMgr.Poll();
    if (UsingCD()) {
        if (mSpinDownTimer.Ms() > 120000.0f) {
            if (mReadingBlock == nullptr) {
                MILO_LOG("BlockMgr spinning up...\n");
                Block *blk = FindMRUBlock();
                mReadingBlock = blk;
                AsyncTask at(blk->ArkFileNum(), blk->BlockNum());
                AddTask(at);
                gReadHD = false;
                bool x = CDRead(
                    mReadingBlock->ArkFileNum(),
                    (mReadingBlock->BlockNum() << 5),
                    2,
                    (void *)gTempBlock
                );
                if (!x) {
                    mReadingBlock->UpdateTimestamp();
                } else {
                    MILO_LOG("CD READING ERROR: %x\n", x);
                    mReadingBlock = nullptr;
                }
            }
            return false;
        }
    }
    return true;
}
