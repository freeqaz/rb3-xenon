#include "MemPoint.h"
#include "Memory.h"
#include "os/Debug.h"
#include "utl/MakeString.h"
#include "utl/MemMgr.h"
#include "string.h"
#include "Str.h"

MemPoint::MemPoint(eInitType t) {
    if (t == kInitType1) {
        for (int i = 0; i < MemNumHeaps(); i++) {
            int a, b, c, d;
            MemFreeBlockStats(i, a, b, mHeapFreeBlocks[i], c, d);
        }
        mPhysicalFree = PhysicalUsage();
    } else {
        memset(mHeapFreeBlocks, 0, 0x44);
    }
}

MemPointDelta MemPoint::operator-(const MemPoint &mp) {
    MemPointDelta mpd;
    for (int i = 0; i < MemNumHeaps(); i++) {
        mpd.mHeapFreeBlocks[i] = mp.mHeapFreeBlocks[i] - mHeapFreeBlocks[i];
    }
    mpd.mPhysicalFree = mPhysicalFree - mp.mPhysicalFree;
    return mpd;
}

MemPointDelta::MemPointDelta() { memset(mHeapFreeBlocks, 0, 0x44); }

MemPointDelta &MemPointDelta::operator+=(const MemPointDelta &mpd) {
    for (int i = 0; i < MemNumHeaps(); i++) {
        mHeapFreeBlocks[i] += mpd.mHeapFreeBlocks[i];
    }
    mPhysicalFree += mpd.mPhysicalFree;
    return *this;
}

bool MemPointDelta::AnyGreaterThan(int i1) const {
    for (int i = 0; i < MemNumHeaps(); i++) {
        if (mHeapFreeBlocks[i] > i1) {
            return true;
        }
    }
    return (i1 < mPhysicalFree) & 1;
}

const char *MemPointDelta::HeaderString(const char *s) {
    String st;
    for (int i = 0; i < MemNumHeaps(); i++) {
        if (i != 0) {
            st << ',';
        }
        st << MemHeapName(i);
        if (s != 0) {
            st << s;
        }
    }
    st << ",physical";
    if (s != 0) {
        st << s;
    }
    const char *c = MakeString("%s", st);
    return c;
}

const char *MemPointDelta::ToString(int divideBy) const {
    MILO_ASSERT(divideBy > 0, 0x5b);
    String st;
    for (int i = 0; i < MemNumHeaps(); i++) {
        if (i != 0) {
            st << ',';
        }
        st << mHeapFreeBlocks[i] / divideBy;
    }
    st << ',';
    st << mPhysicalFree / divideBy;
    const char *c = MakeString("%s", st);
    return c;
}
