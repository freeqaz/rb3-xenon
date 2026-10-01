#pragma once
#include "utl/MemMgr.h"
#include "xdk/XBOXKRNL.h"

// size 0x20
class CriticalSection {
private:
    int mEntryCount; // 0x0
    RTL_CRITICAL_SECTION mCritSec; // 0x4

public:
    CriticalSection();
    ~CriticalSection();
    void Enter();
    void Exit();
    bool TryEnter();
    void Abandon();

#ifdef HX_NATIVE
    MEM_OVERLOAD(CriticalSection, 0x20);
#endif
    // No class allocator in retail: MemInit (0x827BD300), compiled in the TU
    // that defines the global operator new, inlines it into MemAlloc(0x20, 0)
    // for both locks, while every other `new CriticalSection` site branches to
    // the global operator new's folded body at 0x827BD2F0.
};

class CritSecTracker {
public:
    CriticalSection *mCritSec;

    CritSecTracker(CriticalSection *section) {
        mCritSec = section;
        if (section != 0) {
            section->Enter();
        }
    }

    ~CritSecTracker() {
        if (mCritSec != 0) {
            mCritSec->Exit();
        }
    }
};
