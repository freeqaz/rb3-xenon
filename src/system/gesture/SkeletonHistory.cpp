#include "gesture/SkeletonHistory.h"
#include "ArchiveSkeleton.h"
#include "os/Debug.h"

SkeletonHistoryArchive::SkeletonHistoryArchive() {
    for (int i = 0; i < 6; i++) {
        mHistories[i].reserve(0xA0);
    }
}

const std::vector<ArchiveSkeleton> &SkeletonHistoryArchive::GetArchive(int skel_idx) const {
    MILO_ASSERT((0) <= (skel_idx) && (skel_idx) < (6), 0x36);
    return mHistories[skel_idx];
}

bool SkeletonHistory::PrevFromArchive(
    const SkeletonHistoryArchive &archives,
    const Skeleton &skeleton,
    int targetMs,
    ArchiveSkeleton &archiveSkeleton,
    int &elapsedMs
) const {
    int skel_idx = skeleton.SkeletonIndex();
    MILO_ASSERT((0) <= (skel_idx) && (skel_idx) < (6), 0x13);
    const std::vector<ArchiveSkeleton> &archive = archives.GetArchive(skel_idx);
    std::vector<ArchiveSkeleton>::const_iterator it = archive.begin();
    std::vector<ArchiveSkeleton>::const_iterator itEnd;
    elapsedMs = skeleton.ElapsedMs();
    while ((itEnd = archive.end(), it != itEnd) && elapsedMs < targetMs) {
        elapsedMs += it->ElapsedMs();
        ++it;
    }
    if (it != itEnd) {
        archiveSkeleton = *it;
        return true;
    } else
        return false;
}

void SkeletonHistoryArchive::ClearHistory(int skel_idx) {
    MILO_ASSERT((0) <= (skel_idx) && (skel_idx) < (6), 0x4C);
    mHistories[skel_idx].clear();
}

void SkeletonHistoryArchive::AddToHistory(int skel_idx, const Skeleton &skeleton) {
    MILO_ASSERT((0) <= (skel_idx) && (skel_idx) < (6), 0x3F);

    while (mHistories[skel_idx].size() >= 0xA0) {
        mHistories[skel_idx].pop_back();
    }

    ArchiveSkeleton archiveSkeleton;
    archiveSkeleton.Set(skeleton);
    mHistories[skel_idx].insert(mHistories[skel_idx].begin(), archiveSkeleton);
}
