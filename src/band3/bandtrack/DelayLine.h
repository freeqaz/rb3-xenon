#pragma once
#include "os/Debug.h"
#include <string.h>

template <class T, int N>
class DelayLine {
public:
    static const int size = N;
    DelayLine() : mCur(0) { memset(mData, 0, sizeof(mData)); }
    void Clear() {
        memset(mData, 0, sizeof(mData));
        mCur = 0;
    }
    void Set(const T &data) {
        mCur++;
        if (mCur >= N) {
            mCur = 0;
        }
        mData[mCur] = data;
    }
    const T &operator[](int idx) const {
        MILO_ASSERT(idx >=0 && idx < size, 0x39);
        return mData[((mCur + N) - idx) % N];
    }
    T &operator[](int idx) {
        MILO_ASSERT(idx >=0 && idx < size, 0x39);
        return mData[((mCur + N) - idx) % N];
    }

    T mData[N];
    int mCur;
};