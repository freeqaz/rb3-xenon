#include "utl/Profiler.h"
#include "math/Utl.h"
#include "os/Debug.h"
#include "os/Timer.h"
#include "xdk/LIBCMT/float.h"

Profiler::Profiler(char const *c, int i)
    : mName(c), mMin(3.4028235e+38), mMax(0.0f), mSum(0.0f), mCount(0), mCountMax(i) {}

void Profiler::Start() { mTimer.Start(); }

void Profiler::Stop() {
    mTimer.Stop();
    float ms = mTimer.Ms();
    if (mMin > (int)ms) {
        mMin = ms;
    }
    if (ms > mMax) {
        mMax = ms;
    }
    mCount++;
    if (mCount == mCountMax) {
        if (mCountMax == 1U) {
            TheDebug << MakeString("%s: %s\n", mName, FormatTime(mMin));
        } else {
            TheDebug << MakeString(
                "%s: min %s max %s mean %s\n",
                mName,
                FormatTime(mMin),
                FormatTime(mMax),
                FormatTime(mSum / (float)mCount)
            );
        }
        mCount = 0;
        mMin = 3.4028235e+38;
        mMax = 0;
        mSum = ms = 0;
    }
    mTimer.Reset();
}
