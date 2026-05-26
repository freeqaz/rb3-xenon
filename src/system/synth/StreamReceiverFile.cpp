#include "synth/StreamReceiverFile.h"
#include "StreamReceiver.h"
#include "StreamReceiverFile.h"
#include "os/Debug.h"

int StreamReceiverFile::sPlayCursor = 0;

StreamReceiverFile::StreamReceiverFile(int numBuffers, bool slip)
    : StreamReceiver(numBuffers, slip), mTargetBuffer(nullptr), mBufSize(0) {}

float StreamReceiverFile::GetSlipOffset() { return 0.0f; }

int StreamReceiverFile::GetPlayCursor() {
    return (unsigned int)(sPlayCursor * 2) % (unsigned int)(mNumBuffers * kStreamBufSize);
}

void StreamReceiverFile::StartSendImpl(unsigned char *data, int bufSize, int targetIdx) {
    MILO_ASSERT(mTargetBuffer, 0x1D);
    MILO_ASSERT(targetIdx * kStreamBufSize + bufSize <= mBufSize, 0x1F);
    memcpy(targetIdx * kStreamBufSize + mTargetBuffer, data, bufSize);
}
