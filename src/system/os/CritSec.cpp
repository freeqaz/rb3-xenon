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

#ifdef HX_NATIVE
CriticalSection::~CriticalSection() { RtlDeleteCriticalSection(&mCritSec); }
#else
// Empty in retail: ~Synth360 and ~MicManagerXbox destroy their embedded locks
// by calling the shared empty body at 0x826C3888, and nothing in retail
// references RtlDeleteCriticalSection.
CriticalSection::~CriticalSection() {}
#endif

void CriticalSection::Abandon() {
    while (mEntryCount-- > 1) {
        RtlLeaveCriticalSection(&mCritSec);
    }
    RtlLeaveCriticalSection(&mCritSec);
}
