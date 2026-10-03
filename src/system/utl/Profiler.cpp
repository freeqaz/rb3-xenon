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
    // Ms() is re-evaluated per use; MinEq/MaxEq store through a reference, which
    // is why retail re-reads the timer after each update.
    MinEq(mMin, mTimer.Ms());
    MaxEq(mMax, mTimer.Ms());
    mSum += mTimer.Ms();
    mCount++;
    if (mCount == mCountMax) {
#ifdef HX_NATIVE
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
#else
        // RB3-360 retail: the print is stripped; only the FormatTime argument
        // calls survive, in argument (right-to-left) evaluation order.
        if (mCountMax == 1U) {
            FormatTime(mMin);
        } else {
            FormatTime(mSum / (float)mCount);
            FormatTime(mMax);
            FormatTime(mMin);
        }
#endif
        mCount = 0;
        mMin = 3.4028235e+38;
        mMax = 0;
        mSum = 0;
    }
    mTimer.Reset();
}
