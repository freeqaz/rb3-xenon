#include "os/AsyncTask.h"
#include "os/ArkFile_p.h"
#include "os/Block.h"
#include "os/Debug.h"
#include <cstring>

AsyncTask::AsyncTask(
    ArkFile *owner, void *buf, int arknum, int bnum, int start, int end, const char *str
)
    : mArkfileNum(arknum), mBlockNum(bnum), mOffsetStart(start), mOffsetEnd(end),
      mBuffer(buf), mStr(str), mOwner(owner) {
    MILO_ASSERT(mOwner, 0x1B);
}

AsyncTask::AsyncTask(int arknum, int bnum)
    : mArkfileNum(arknum), mBlockNum(bnum), mOffsetStart(-1), mOffsetEnd(-1), mBuffer(0),
      mStr(gNullStr), mOwner(0) {}

bool AsyncTask::FillData() {
    char *data = TheBlockMgr.GetBlockData(mArkfileNum, mBlockNum);
    if (data && mOwner) {
        memcpy(mBuffer, &data[mOffsetStart], mOffsetEnd - mOffsetStart);
        mOwner->TaskDone(mOffsetEnd - mOffsetStart);
        return true;
    }
    return false;
}
