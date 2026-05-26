#include "meta/DeJitterPanel.h"
#include "ui/UIPanel.h"
#include "utl/DeJitter.h"

DeJitterPanel::DeJitterPanel() : mFirstFrame(true) {}

DeJitterPanel::~DeJitterPanel() {}

void DeJitterPanel::Enter() {
    mDeJitter.Reset();
    mFirstFrame = true;
    DeJitterSetter setter(mDeJitter, 0);
    UIPanel::Enter();
}

void DeJitterPanel::Poll() {
    // First frame only: prime the jitter state
    if (mFirstFrame) {
        mTimer.Restart();
        float f;
        mDeJitter.NewMs(0, f);
    }
    {
        // Use scoped time correction: pass timer on subsequent frames, nullptr on first
        DeJitterSetter setter(mDeJitter, mFirstFrame ? nullptr : &mTimer);
        UIPanel::Poll();
    }
    mFirstFrame = false;
}

DeJitterSetter::DeJitterSetter(DeJitter &dj, Timer *t) {
    // Save current time state for restoration in destructor
    secs = TheTaskMgr.Seconds(TaskMgr::kRealTime);
    delta_secs = TheTaskMgr.DeltaSeconds();
    float f1 = 0.0f;
    float f18 = 0.0f;
    if (t) {
        // Apply jitter correction via DeJitter, convert ms to seconds
        f1 = dj.NewMs(t->SplitMs(), f18) * 0.001f;
        f18 *= 0.001f;
    }
    // Set corrected time for the duration of this scope
    TheTaskMgr.SetTimeAndDelta(kTaskSeconds, f1, f18);
}

DeJitterSetter::~DeJitterSetter() {
    TheTaskMgr.SetTimeAndDelta(kTaskSeconds, secs, delta_secs);
}
