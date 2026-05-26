#include "gesture/SkeletonRecoverer.h"
#include "gesture/GestureMgr.h"
#include "gesture/Skeleton.h"
#include "obj/Task.h"
#include "utl/Std.h"
#include <cfloat>

SkeletonRecoverer::SkeletonRecoverer() {}

SkeletonRecoverer::~SkeletonRecoverer() {}

bool SkeletonRecoverer::IsSkeletonTracked(int id) const {
    for (int i = 0; i < 6; i++) {
        if (TheGestureMgr->GetSkeleton(i).TrackingID() == id) {
            if (TheGestureMgr->GetSkeleton(i).IsTracked())
                return true;
        }
    }
    return false;
}

int SkeletonRecoverer::GetTrackingIDWithRecovery(int id, int exclude) {
    Skeleton *skel = TheGestureMgr->GetSkeletonByTrackingID(id);
    if (skel && skel->IsTracked()) {
        return id;
    }

    TrackingIDHistory *found = nullptr;
    for (std::list<TrackingIDHistory>::iterator it = mIDHistory.begin(); it != mIDHistory.end();
         ++it) {
        if (it->mTrackingID == id) {
            found = &(*it);
            break;
        }
    }
    if (!found) {
        return 0;
    }

    int bestSkeleton = -1;
    int i = 0;
    float bestDist = FLT_MAX;
    do {
        Skeleton &candidate = TheGestureMgr->GetSkeleton(i);
        if (candidate.TrackingState() != kSkeletonNotTracked
            && candidate.TrackingID() != exclude) {
            float dy = candidate.GetUnkab0().y - found->unk8;
            float dz = candidate.GetUnkab0().z - found->unkC;
            float dx = candidate.GetUnkab0().x - found->unk4;
            float dist = dx * dx + (dz * dz + dy * dy);
            if (dist < bestDist) {
                bestDist = dist;
                bestSkeleton = i;
            }
        }
        i++;
    } while (i < 6);

    float maxRecoveryDistance = GestureMgr::MaxRecoveryDistance();
    if (bestSkeleton == -1 || bestDist > maxRecoveryDistance * maxRecoveryDistance
        || found->mUntrackedTime <= GestureMgr::MinRecoveryTime()) {
        return found->mTrackingID;
    }
    return TheGestureMgr->GetSkeleton(bestSkeleton).TrackingID();
}

bool SkeletonRecoverer::WaitingToRecover() {
    FOREACH (it, mIDHistory) {
        if (it->mUntrackedTime > 0.0f) {
            return true;
        }
    }
    return false;
}

void SkeletonRecoverer::Poll() {
    float deltaSeconds = TheTaskMgr.DeltaUISeconds();

    for (int i = 0; i < 6; i++) {
        Skeleton &skel = TheGestureMgr->GetSkeleton(i);
        if (!skel.IsTracked()) {
            continue;
        }

        int trackingID = skel.TrackingID();
        TrackingIDHistory *found;
        for (std::list<TrackingIDHistory>::iterator it = mIDHistory.begin();
             it != mIDHistory.end(); ++it) {
            if (it->mTrackingID == trackingID) {
                found = &(*it);
                goto check_found;
            }
        }
        found = nullptr;
        check_found:
        if (found) {
            found->mUntrackedTime = 0.0f;
            const int *src = reinterpret_cast<const int *>(&skel.GetUnkab0());
            int *dst = reinterpret_cast<int *>(&found->unk4);
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
        } else {
            TrackingIDHistory history;
            const int *src = reinterpret_cast<const int *>(&skel.GetUnkab0());
            int w0 = src[0];
            int w1 = src[1];
            int w2 = src[2];
            int w3 = src[3];
            history.mUntrackedTime = 0.0f;
            history.mTrackingID = trackingID;
            int *dst = reinterpret_cast<int *>(&history.unk4);
            dst[0] = w0;
            dst[1] = w1;
            dst[2] = w2;
            dst[3] = w3;
            mIDHistory.insert(mIDHistory.begin(), history);
        }
    }

    for (std::list<TrackingIDHistory>::iterator it = mIDHistory.begin();
         it != mIDHistory.end();) {
        TrackingIDHistory *data = &(*it);
        if (data->mUntrackedTime > GestureMgr::MaxRecoveryTime()) {
            it = mIDHistory.erase(it);
            continue;
        }
        if (!IsSkeletonTracked(data->mTrackingID)) {
            data->mUntrackedTime = deltaSeconds + data->mUntrackedTime;
        }
        ++it;
    }
}
