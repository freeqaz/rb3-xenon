#pragma once
#include "gesture/Skeleton.h"
#include "gesture/ArchiveSkeleton.h"
#include <vector>

class SkeletonHistoryArchive {
public:
    SkeletonHistoryArchive();
    ~SkeletonHistoryArchive() {}

    const std::vector<ArchiveSkeleton> &GetArchive(int) const;
    void AddToHistory(int, const Skeleton &);
    void ClearHistory(int);

protected:
    std::vector<ArchiveSkeleton> mHistories[6]; // 0x0
};

class SkeletonHistory {
public:
    virtual bool PrevSkeleton(const Skeleton &, int, ArchiveSkeleton &, int &) const = 0;

protected:
    bool
    PrevFromArchive(const SkeletonHistoryArchive &, const Skeleton &, int, ArchiveSkeleton &, int &)
        const;
};
