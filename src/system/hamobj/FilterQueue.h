#pragma once
#include "gesture/Skeleton.h"
#include "hamobj/DetectFrame.h"
#include "hamobj/ErrorNode.h"
#include "hamobj/FilterVersion.h"
#include "hamobj/HamMove.h"
#include <vector>

// size 0x34
class FilterQueue {
public:
    FilterQueue();

    bool GetResults(float &outValue, DetectFrame **frames, float unused);
    void EnqueueNewJob(float outValue, float duration, MoveMode mode);
    void EnqueueFrame(int frameNumber, float f2, float f3, DetectFrame *df, const FilterVersion *fv);
    bool IsJobFinished() const;
    float LastPollMs() const;
    bool HasJob() const;
    void CancelJob();
    void StartJob();
    void Poll(const SkeletonUpdateData &);

private:
    // size 0x14
    class FilterInputFrame {
    public:
        int mSlot;
        float mSongBeats;
        float mSongSpeed;
        DetectFrame *mDetectFrame;
        const FilterVersion *mFilterVersion;
    };

    // size 0x214
    class FilterOutputFrame {
    public:
        FilterInputFrame *mInputFrame;
        Vector3 mErrors[kMaxNumErrorNodes]; // 0x4
    };

    struct QueuedJob {
        float songSeconds; // 0x0 - song seconds minus latency seconds
        MoveMode moveMode; // 0x4 - current move mode
        float songSpeed; // 0x8 - song speed
        std::vector<FilterInputFrame> frames; // 0xc
    };

    struct Output {
        float songSpeed; // 0x0
        MoveMode moveMode; // 0x4
        std::vector<FilterOutputFrame> frames; // 0x20
    };

    QueuedJob mQueuedJob; // 0x0
    Output mOutput; // 0x18
    bool mJobFinished; // 0x2c
    float mLastPollMs; // 0x30
};
