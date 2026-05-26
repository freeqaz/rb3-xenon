#pragma once
#include "utl/MemMgr.h"
#include "utl/Symbol.h"
#include "obj/Data.h"
#include "os/OSFuncs.h"
#ifdef HX_NATIVE
#include <chrono>
// Emulate PPC __mftb() with high-resolution clock (returns microseconds)
inline unsigned int __mftb() {
    using namespace std::chrono;
    return (unsigned int)(duration_cast<microseconds>(
        steady_clock::now().time_since_epoch()).count());
}
// PPC byte swap intrinsics - no-ops on little-endian (data already in native order)
inline unsigned int __loadwordbytereverse(int offset, const void *base) {
    unsigned int val;
    memcpy(&val, (const char *)base + offset, 4);
    return __builtin_bswap32(val);
}
inline unsigned short __loadshortbytereverse(int offset, const void *base) {
    unsigned short val;
    memcpy(&val, (const char *)base + offset, 2);
    return __builtin_bswap16(val);
}
inline void __storewordbytereverse(unsigned int val, int offset, void *base) {
    val = __builtin_bswap32(val);
    memcpy((char *)base + offset, &val, 4);
}
inline void __storeshortbytereverse(unsigned short val, int offset, void *base) {
    val = __builtin_bswap16(val);
    memcpy((char *)base + offset, &val, 2);
}
#else
#include "ppcintrinsics.h"
#endif

class Timer {
    friend class AutoSlowFrame;

private:
    unsigned int mStart; // 0x00
    // padding, 0x04
    unsigned long long mCycles; // 0x08
    float mLastMs; // 0x10
    float mWorstMs; // 0x14
    int mWorstMsFrame; // 0x18
    int mFrame; // 0x1c
    Symbol mName; // 0x20
    int mRunning; // 0x24
    float mBudget; // 0x28
    bool mDraw; // 0x2c

    static float sLowCycles2Ms;
    static float sHighCycles2Ms;
    static double sDoubleCycles2Ms;
    static Timer sSlowFrameTimer;
    static float sSlowFrameWaiver;
    static const char *sSlowFrameReason;

public:
    static void Init();
    static void Sleep(int);

    static float CyclesToMs(long long cycles) {
        unsigned long lowCycles = cycles;
        long highCycles = cycles >> 32;
        return (lowCycles * sLowCycles2Ms) + (highCycles * sHighCycles2Ms);
    }

    static void ClearSlowFrame() {
        sSlowFrameReason = "None";
        sSlowFrameTimer.Reset();
        sSlowFrameWaiver = 0;
    }

    static Timer &SlowFrameTimer() { return sSlowFrameTimer; }
    static float SlowFrameWaiver() { return sSlowFrameWaiver; }
    static void AddToSlowFrameWaiver(float val) { sSlowFrameWaiver += val; }
    static void SetSlowFrameReason(const char *reason) { sSlowFrameReason = reason; }

    Timer();
    Timer(DataArray *);

    void Start() {
        if (mRunning++ != 0)
            return;

        mStart = __mftb();
    }

    // Function addresses from retail, not sure what these do yet
    unsigned long long Stop() {
        unsigned long long cycles = 0;

        if (--mRunning == 0) {
            unsigned int mftb = __mftb();
            cycles = mftb - mStart;
            mCycles += cycles;
        }

        return cycles;
    }

    void Pause() {
        if (mRunning <= 0) {
            return;
        }

        mRunning = -mRunning;

        unsigned int mftb = __mftb();
        mCycles += mftb - mStart;
    }

    void Resume() {
        if (mRunning >= 0) {
            return;
        }

        mRunning = -mRunning;

        mStart = __mftb();
    }

    bool Running() const { return mRunning > 0; }

    void Split() {
        if (mRunning <= 0)
            return;

        unsigned int cycle = __mftb();

        mCycles += cycle - mStart;
        mStart = cycle;
    }

    float SplitMs() {
        Split();
        return Ms();
    }

    MEM_OVERLOAD(Timer, 0x57);

    void Reset();
    void Restart();

    Symbol Name() const { return mName; }
    float Ms() { return CyclesToMs(mCycles); }
    float GetLastMs() { return mLastMs; }
    float GetWorstMs() { return mWorstMs; }
    float Budget() const { return mBudget; }
    bool Draw() const { return mDraw; }
    void SetDraw(bool draw) { mDraw = draw; }
    void SetLastMs(float ms);
};

#define MAX_TOP_VALS 128

class TimerStats {
    friend class Rnd;
private:
    int mCount; // 0x0
    float mAvgMs; // 0x4
    float mStdDevMs; // 0x8
    float mMaxMs; // 0xc
    int mNumOverBudget; // 0x10
    float mBudget; // 0x14
    bool mCritical; // 0x18
    int mNumCritOverBudget; // 0x1c
    float mAvgMsInCrit; // 0x20
    float mTopValues[MAX_TOP_VALS]; // 0x24
public:
    TimerStats(DataArray *);

    bool Critical() const { return mCritical; }

    void CollectStats(float, bool, int);
    void PrintPctile(float);
    void Dump(const char *, int);
    void Clear();
};

typedef void (*AutoTimerCallback)(float elapsed, void *context);

class AutoSlowFrame {
public:
    AutoSlowFrame(const char *reason, float waiver)
        : mStartTime(0), mReason(reason), mWaiver(waiver) {
        if (MainThread()) {
            sDepth++;
            mStartTime = Timer::SlowFrameTimer().Ms();
            Timer::AddToSlowFrameWaiver(waiver);
            Timer::SlowFrameTimer().Start();
        } else {
            mStartTime = 0;
        }
    }

    ~AutoSlowFrame() {
        if (MainThread()) {
            sDepth--;
            Timer::SlowFrameTimer().Stop();
            if (Timer::SlowFrameTimer().Ms() - mStartTime > mWaiver) {
                Timer::SetSlowFrameReason(mReason);
            }
        }
    }

private:
    float mStartTime; // 0x0
    const char *mReason; // 0x4
    float mWaiver; // 0x8

    static int sDepth;
};

class AutoGlitchReport {
public:
    AutoGlitchReport(float f1, const char *func) {
        if (MainThread()) {
            mLimit = f1;
            mFunc = func;
            mContext = 0;
            mCallback = nullptr;
            sDepth++;
            mTimer.Start();
        }
    }
    AutoGlitchReport(float f1, AutoTimerCallback cb, void *v3) {
        if (MainThread()) {
            mLimit = f1;
            mContext = v3;
            mCallback = cb;
            mFunc = nullptr;
            sDepth++;
            mTimer.Start();
        }
    }

    ~AutoGlitchReport() {
        if (MainThread()) {
            sDepth--;
            SendCallback(mTimer.SplitMs(), mLimit, mFunc, mCallback, mContext);
        }
    }
    static void EnableCallback();
    static void EndExternal(float elapsed, float limit, const char *func, AutoTimerCallback cb, void *ctx) {
        if (MainThread()) {
            sDepth--;
            SendCallback(elapsed, limit, func, cb, ctx);
        }
    }
    static void SendCallback(float, float, const char *, AutoTimerCallback, void *);
    static int sDepth;

private:
    Timer mTimer; // 0x0
    const char *mFunc; // 0x30
    AutoTimerCallback mCallback; // 0x34
    void *mContext; // 0x38
    float mLimit; // 0x3c
};

class AutoTimer {
public:
    AutoTimer(Timer *t, float limit, AutoTimerCallback callback, void *context) {
        mTimeLimit = limit;
        mCallback = callback;
        mContext = context;
        mTimer = t;
        if (mTimer) {
            if (MainThread()) {
                AutoGlitchReport::sDepth++;
            }
            mTimer->Start();
        }
    }

    ~AutoTimer() {
        if (mTimer) {
            unsigned long long cycles = mTimer->Stop();
            float elapsed = Timer::CyclesToMs(cycles);
            AutoGlitchReport::EndExternal(
                elapsed,
                mTimeLimit,
                mTimer->Name().Str(),
                mCallback,
                mContext
            );
        }
    }

    static Timer *GetTimer(Symbol);
    static void DumpTimerStats();
    static void SetCollectStats(bool, bool);
    static bool CollectingStats();
    static void ComputeCriticalFrame();
    static void CollectTimerStats();
    static void PrintTimers(bool);
    static void Init();
    static void ResetTimers();

    static std::list<std::pair<Timer, TimerStats> > &Timers() { return sTimers; }

    friend class Rnd;

private:
    Timer *mTimer; // 0x0
    float mTimeLimit; // 0x4
    AutoTimerCallback mCallback; // 0x8
    void *mContext; // 0xc

    static bool sCriticalFrame;
    static bool sCollectingStats;
    static int sCritFrameCount;
    static std::list<std::pair<Timer, TimerStats> > sTimers;
};

#ifdef MILO_DEBUG
#define START_AUTO_TIMER_CALLBACK(name, func, context)                                   \
    static Timer *_t = AutoTimer::GetTimer(name);                                        \
    AutoTimer _at(_t, 50.0f, func, context)
#else
#define START_AUTO_TIMER_CALLBACK(name, func, context) (void)0
#endif

#define START_AUTO_TIMER(name) START_AUTO_TIMER_CALLBACK(name, NULL, NULL)

#define TIMER_ACTION(name, action)                                                       \
    {                                                                                    \
        START_AUTO_TIMER(name);                                                          \
        action;                                                                          \
    }

const char *FormatTime(float);
