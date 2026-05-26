#include "rndobj/SIVideo.h"
#include "os/Debug.h"
#include "utl/MemMgr.h"

void SIVideo::Reset() {
    mWidth = mHeight = mNumFrames = mOrder = 0;
    if (mData) {
        MemFree(mData, __FILE__, 22);
        mData = nullptr;
    }
}

int SIVideo::Bpp() const { return mOrder == 8 ? 4 : 8; }
int SIVideo::FrameSize() const { return (mWidth * mHeight * Bpp()) >> 3; }

char *SIVideo::Frame(int i) {
    if (mData) {
        return &mData[FrameSize() * i];
    } else
        return nullptr;
}

void SIVideo::Load(BinStream &bs, bool load_data) {
    int magic, dump, unused;
    bs >> magic;
    if (magic != 0x5349565FU) { // "SIV_"
        mWidth = magic;
        bs >> mHeight;
        bs >> mNumFrames;
        bs >> dump;
        bs >> unused;
        bs >> unused;
        bs >> unused;
        bs >> unused;
        mOrder = 8;
    } else {
        uint x;
        bs >> x;
        if (x > 1)
            MILO_FAIL("Can't load new SIVideo.\n");
        bs >> mWidth;
        bs >> mHeight;
        bs >> mNumFrames;
        bs >> mOrder;
    }
    if (mData) {
        MemFree(mData, __FILE__, 0x41);
        mData = nullptr;
    }
    if (!load_data) {
        mData = (char *)MemAlloc(mNumFrames * FrameSize(), __FILE__, 70, "SIVideo_buf");
        bs.Read(mData, mNumFrames * FrameSize());
    }
}
