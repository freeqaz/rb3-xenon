#pragma once
#include <list>

class SkeletonRecoverer {
public:
    struct TrackingIDHistory {
        int mTrackingID; // 0x0
        float unk4; // 0x4
        float unk8; // 0x8
        float unkC; // 0xc
        float unk10; // 0x10
        float mUntrackedTime; // 0x14
    };
    SkeletonRecoverer();
    virtual ~SkeletonRecoverer();

    void Poll();
    bool WaitingToRecover();
    int GetTrackingIDWithRecovery(int, int);

protected:
    std::list<TrackingIDHistory> mIDHistory; // 0x4

private:
    bool IsSkeletonTracked(int) const;
};
