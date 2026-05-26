#include "os/CritSec.h"
#include "xdk/XBOXKRNL.h"

CriticalSection::CriticalSection() : mEntryCount(0) {
    RtlInitializeCriticalSection(&mCritSec);
}

void CriticalSection::Enter() {
    RtlEnterCriticalSection(&mCritSec);
    mEntryCount++;
}

void CriticalSection::Exit() {
    mEntryCount--;
    RtlLeaveCriticalSection(&mCritSec);
}

bool CriticalSection::TryEnter() {
    if (RtlTryEnterCriticalSection(&mCritSec) != 0U) {
        mEntryCount++;
        return true;
    } else
        return false;
}

CriticalSection::~CriticalSection() { RtlDeleteCriticalSection(&mCritSec); }

void CriticalSection::Abandon() {
    while (mEntryCount-- > 1) {
        RtlLeaveCriticalSection(&mCritSec);
    }
    RtlLeaveCriticalSection(&mCritSec);
}
