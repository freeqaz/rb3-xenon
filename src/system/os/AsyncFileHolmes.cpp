#include "HolmesClient.h"
#include "os/AsyncFileHolmes_p.h"
#include "os/Debug.h"

AsyncFileHolmes::AsyncFileHolmes(const char *name, int mode)
    : AsyncFile(name, mode), mFd(-1) {}

AsyncFileHolmes::~AsyncFileHolmes() { Terminate(); }

bool AsyncFileHolmes::Truncate(int x) {
    HolmesClientTruncate(mFd, x);
    return true;
}

void AsyncFileHolmes::_OpenAsync() {
    unsigned int siz;
    mFail = !HolmesClientOpen(mFilename.c_str(), mMode, siz, mFd);
    if (mFail) {
        siz = 0;
    }
    mSize = siz;
}

void AsyncFileHolmes::_WriteAsync(const void *data, int bytes) {
    MILO_ASSERT(mOffset == bytes, 0x26);
    HolmesClientWrite(mFd, mTell - mOffset, bytes, data);
}

void AsyncFileHolmes::_SeekToTell() {
    while (!_ReadDone())
        ;
}

void AsyncFileHolmes::_ReadAsync(void *data, int bytes) {
    HolmesClientRead(mFd, mTell, bytes, data, this);
}

bool AsyncFileHolmes::_ReadDone() { return HolmesClientReadDone(this); }

void AsyncFileHolmes::_Close() {
    if (!mFail) {
        HolmesClientClose(this, mFd);
    }
}
