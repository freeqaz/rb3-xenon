#include "synth/StreamReceiver.h"

#define kStreamRcvrSendSize 0xC000
#include "os/Debug.h"
#ifdef HX_NATIVE
#include "platform/StreamReceiver_Native.h"
#else
extern "C" void XMemCpy(void *, const void *, int);
#endif

StreamReceiver::StreamReceiver(int numBuffers, bool slip)
    : mSlipEnabled(slip), mBuffer(), mNumBuffers(numBuffers), mRingFreeSpace(0),
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
    unsigned long long buffersSent = (unsigned long long)mBuffersSent;
    unsigned long long numBuffers = (unsigned long long)mNumBuffers;
    unsigned long long bufferOffset = buffersSent * 0xc000;
    unsigned long long totalPlayed = (unsigned long long)mLastPlayCursor + (buffersSent / numBuffers) * numBuffers * 0xc000;

    for (; totalPlayed >= bufferOffset; totalPlayed = totalPlayed - numBuffers * 0xc000)
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
    // Retail RB3-360 (0x8272a5b8): a switch on the state, 0xC000-byte send
    // blocks (hence divw, not a shift) and a 100000 wrap limit.
    switch ((unsigned int)mState) {
    case kInit:
        mWantToSend = true;
        break;
    case kReady:
        break;
    case kPlaying:
    case kStopped: {
        int playCursor = GetPlayCursor();
        int activeBuf = playCursor / kStreamRcvrSendSize;
        mLastPlayCursor = playCursor;
        MILO_ASSERT(activeBuf >= 0 && activeBuf < mNumBuffers, 0xc2);
        if (!mSlipEnabled && activeBuf != mSendTarget) {
            mWantToSend = true;
        }
        int diff = activeBuf - mSendTarget;
        if (diff == mNumBuffers / 2 || diff == -(mNumBuffers / 2)) {
            mWantToSend = true;
        }
        break;
    }
    default:
        MILO_FAIL("bad state logic.\n");
        break;
    }
    if (mWantToSend && mState != kInit && kStreamRcvrBufSize - mRingFreeSpace != 0) {
        mStarving = true;
    }
    if (mWantToSend && mRingFreeSpace >= kStreamRcvrSendSize && !mSending) {
        StartSendImpl(mBuffer, kStreamRcvrSendSize, mSendTarget);
        mBuffersSent++;
        if (mBuffersSent >= 100000) {
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
            int overflow = mRingFreeSpace - kStreamRcvrSendSize;
            MILO_ASSERT(overflow >= 0, 0x134);
            if (overflow != 0) {
                XMemCpy(mBuffer, mBuffer + kStreamRcvrSendSize, overflow);
            }
            int ringFreeSpace = mRingFreeSpace - kStreamRcvrSendSize;
            mRingFreeSpace = ringFreeSpace;
            if (mEndData) {
                memset(&mBuffer[ringFreeSpace], 0, kStreamRcvrBufSize - ringFreeSpace);
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
