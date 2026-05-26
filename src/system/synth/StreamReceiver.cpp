#include "synth/StreamReceiver.h"
#include "os/Debug.h"
#ifdef HX_NATIVE
#include "platform/StreamReceiver_Native.h"
#else
extern "C" void XMemCpy(void *, const void *, int);
#endif

StreamReceiver::StreamReceiver(int numBuffers, bool slip)
    : mSlipEnabled(slip), mNumBuffers(numBuffers), mBuffer(), mRingFreeSpace(0),
      mState(kInit), mSendTarget(0), mWantToSend(false), mSending(false), mBuffersSent(0),
      mStarving(false), mEndData(false), mDoneBufferCounter(0), mLastPlayCursor(0) {
    MILO_ASSERT(numBuffers > 0, 0x33);
}

StreamReceiver::~StreamReceiver() {}

int StreamReceiver::BytesWriteable() { return kStreamRcvrBufSize - mRingFreeSpace; }
bool StreamReceiver::Ready() { return mState != kInit; }

void StreamReceiver::EndData() {
    if (!mEndData) {
        if (mRingFreeSpace < kStreamRcvrBufSize) {
            memset(&mBuffer[mRingFreeSpace], 0, kStreamRcvrBufSize - mRingFreeSpace);
            mRingFreeSpace = kStreamRcvrBufSize;
        }
        mEndData = true;
    }
}

void StreamReceiver::Play() {
    MILO_ASSERT(Ready(), 0x91);
    if (mState != kPlaying) {
        if (mState == kStopped) {
            PauseImpl(false);
        } else {
            PlayImpl();
        }
        mState = kPlaying;
    }
}

void StreamReceiver::Stop() {
    MILO_ASSERT(mState == kPlaying || mState == kStopped, 0xA6);
    if (mState == kPlaying) {
        PauseImpl(true);
        mState = kStopped;
    }
}

u64 StreamReceiver::GetBytesPlayed() {
    if (mState == kInit) {
        return 0;
    }
#ifdef HX_NATIVE
    // Native: GetPlayCursor() updates mLastPlayCursor with total bytes consumed
    GetPlayCursor();
    return (u64)mLastPlayCursor;
#else
    unsigned long long numBuffers = (unsigned long long)mNumBuffers;
    unsigned long long buffersSent = (unsigned long long)mBuffersSent;
    unsigned long long bufferOffset = buffersSent << 0xe;
    unsigned long long totalPlayed = (unsigned long long)mLastPlayCursor + (buffersSent / numBuffers) * numBuffers * 0x4000;

    for (; totalPlayed >= bufferOffset; totalPlayed = totalPlayed - numBuffers * 0x4000)
        ;
    return totalPlayed;
#endif
}

void StreamReceiver::WriteData(const void *data, int size) {
#ifdef HX_NATIVE
    // On native, forward data directly to the platform receiver's ring buffer
    // via StartSendImpl. The base class mBuffer is not used — audio output
    // reads from StreamReceiverNative::mPCMBuf instead.
    StartSendImpl((unsigned char *)data, size, 0);
    mSending = true;
    mWantToSend = false;
#else
    MILO_ASSERT(size > 0 && size <= BytesWriteable(), 0x51);
    memcpy(mBuffer + mRingFreeSpace, data, size);
    mRingFreeSpace += size;
#endif
}

void StreamReceiver::Poll() {
#ifdef HX_NATIVE
    if (mSending && SendDoneImpl()) {
        mSending = false;
        mBuffersSent++;
    }
    // On Xbox, mDoneBufferCounter increments via the ring buffer send cycle
    // when mEndData is true. Native skips ring buffer management — data goes
    // directly to the platform audio thread. Increment here once the audio
    // output has drained, so StandardStream can transition to kFinished.
    if (mEndData && IsOutputDrained()) {
        mDoneBufferCounter++;
    }
#else
    if (kInit != (unsigned int)mState) {
        if (mState == kReady) {
            goto ready;
        }
        if (mState > kStopped) {
            MILO_FAIL("bad state logic.\n");
            goto ready;
        }
        int playCursor = GetPlayCursor();
        int activeBuf = playCursor / 0x4000;
        mLastPlayCursor = playCursor;
        MILO_ASSERT(activeBuf >= 0 && activeBuf < mNumBuffers, 0xc2);
        if (!mSlipEnabled && activeBuf != mSendTarget) {
            mWantToSend = true;
        }
        int halfBufs = mNumBuffers / 2;
        int diff = activeBuf - mSendTarget;
        if (diff != halfBufs && diff != -halfBufs) {
            goto ready;
        }
    }
    mWantToSend = true;
ready:
    if (mWantToSend && mState != kInit && mRingFreeSpace != kStreamRcvrBufSize) {
        mStarving = true;
    }
    if (mWantToSend && mRingFreeSpace >= 0x4000 && !mSending) {
        StartSendImpl(mBuffer, 0x4000, mSendTarget);
        mBuffersSent++;
        if (mBuffersSent >= 700000) {
            mBuffersSent -= mNumBuffers;
        }
        int sendTarget = mSendTarget;
        mWantToSend = false;
        mSending = true;
        mSendTarget = sendTarget + 1;
        if (sendTarget + 1 == mNumBuffers) {
            mSendTarget = 0;
        }
    }
    if (mSending) {
        if (SendDoneImpl()) {
            mSending = false;
            mStarving = false;
            if (mSendTarget == 0 && mState == kInit) {
                mState = kReady;
                mWantToSend = false;
            }
            int overflow = mRingFreeSpace - 0x4000;
            MILO_ASSERT(overflow >= 0, 0x134);
            if (overflow != 0) {
                XMemCpy(mBuffer, mBuffer + 0x4000, overflow);
            }
            int ringFreeSpace = mRingFreeSpace;
            mRingFreeSpace = ringFreeSpace - 0x4000;
            if (mEndData) {
                memset(&mBuffer[ringFreeSpace - 0x4000], 0, kStreamRcvrBufSize - (ringFreeSpace - 0x4000));
                mRingFreeSpace = kStreamRcvrBufSize;
                mDoneBufferCounter++;
            }
        }
    }
#endif
}

#ifndef HX_NATIVE
StreamReceiver *StreamReceiver::New(int i1, int i2, bool b3, int i4) {
    MILO_ASSERT(sFactory, 0x1C);
    return sFactory(i1, i2, b3, i4);
}
#endif
