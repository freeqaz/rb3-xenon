#include "gesture/FreestyleMotionFilter.h"
#include "Skeleton.h"
#include "hamobj/HamGameData.h"
#include "hamobj/HamPlayerData.h"
#include "obj/Object.h"
#include "os/Debug.h"

FreestyleMotionFilter::FreestyleMotionFilter() : mIsActive(false) { Clear(); }

FreestyleMotionFilter::~FreestyleMotionFilter() {}

BEGIN_PROPSYNCS(FreestyleMotionFilter)
    SYNC_PROP(velocity_threshold, mVelocityThreshold)
    SYNC_PROP(move_time, mMoveTime)
    SYNC_PROP(movement_amount, mMovementAmount)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

void FreestyleMotionFilter::Clear() {
    mVelocityThreshold = 0.0f;
    mMoveTime = 0.0f;
    mMovementAmount = 0.0f;
}

void FreestyleMotionFilter::Activate() {
    mIsActive = true;
    mMoveTime = 0.0f;
    mMovementAmount = 0.0f;
}

void FreestyleMotionFilter::Deactivate() { mIsActive = false; }

bool FreestyleMotionFilter::IsActive() const { return mIsActive; }

bool FreestyleMotionFilter::Detected() { return 0 < mMoveTime; }

void FreestyleMotionFilter::UpdateFilters(SkeletonUpdateData const &skeletonData) {
    HamPlayerData *player = TheGameData->Player(0);
    MILO_ASSERT(player, 0x44);
    if (player->IsPlaying() && skeletonData.mSkeletonsLeft) {
        Skeleton *skeleton = *skeletonData.mSkeletonsLeft;
        mMoveTime = 0.0f;
        for (int i = 0; i < 20; i++) {
            Vector3 velocity;
            int elapsed;
            if (skeleton->Velocity(*skeletonData.mHistory, kCoordCamera, (SkeletonJoint)i, skeleton->ElapsedMs(), velocity, elapsed) &&
                skeleton->JointConf((SkeletonJoint)i) == kConfidenceTracked) {
                float vx = velocity.x;
                float vy = velocity.y;
                float vz = velocity.z;
                float x2 = vx * vx;
                float y2 = vy * vy;
                float z2 = vz * vz;
                float sum = x2 + z2;
                if (sum + y2 > mVelocityThreshold) {
                    mMovementAmount = sqrtf(x2 + z2 + y2);
                    mMoveTime = (float)skeleton->ElapsedMs();
                    return;
                }
            }
        }
    }
}

BEGIN_HANDLERS(FreestyleMotionFilter)
    HANDLE_ACTION(activate, Activate())
    HANDLE_ACTION(deactivate, Deactivate())
    HANDLE_EXPR(detected, Detected())
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS
