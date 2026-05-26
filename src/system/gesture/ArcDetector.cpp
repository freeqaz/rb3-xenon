#include "gesture/ArcDetector.h"
#include "gesture/BaseSkeleton.h"
#include "gesture/GestureMgr.h"
#include "gesture/SkeletonViz.h"
#include "os/Debug.h"
#include "rndobj/Rnd.h"
#include "rndobj/Utl.h"
#include "utl/DebugMeter.h"
#include "utl/Std.h"

float ArcDetector::_swipeRetentionFactor = 0.5;
float ArcDetector::_acceptablePathErrorRatio = 0.89999998;
int sDefaultHoverTimer = 600;

ArcDetector::ArcDetector()
    : mArcOffset(0, 0, 0), mSwipeExtentX(0), mSwipeExtentY(0), mSwipeThreshold(0.15f), mInitialized(0), mHadProgress(0),
      mHoverTimer(sDefaultHoverTimer) {
    Clear();
}

ArcDetector::~ArcDetector() {}

void ArcDetector::ResetHoverTimer() { mHoverTimer = sDefaultHoverTimer; }

void ArcDetector::Initialize(
    SkeletonSide side, SkeletonJoint j1, SkeletonJoint j2, float f4
) {
    mSwipeThreshold = f4;
    mSide = side;
    mInitialized = true;
    mPrimaryJoint = j1;
    mSecondaryJoint = j2;
}

Vector3 ArcDetector::GetCurveStart() const {
    MILO_ASSERT(!mJointPath.empty(), 0xE9);
    return Vector3((mSide == kSkeletonLeft ? 1 : -1) * mSwipeExtentX, mSwipeExtentY, 0.0f);
}

void ArcDetector::Clear() {
    mCurrentSwipeAmt = 0;
    mJointPath.clear();
    mSwipeExtentY = 0;
    mSwipeExtentX = 0;
}

void ArcDetector::PrintJointPath() const {
    MILO_LOG("*** Hand path:\n");
    FOREACH (it, mJointPath) {
        MILO_LOG("%f, %f, %f,\n", it->x, it->y, it->z);
    }
    MILO_LOG("GetPathLength() %f\n", GetPathLength());
    MILO_LOG(
        "pow(GetPathLength(), _swipeRetentionFactor + 1) %f\n",
        pow(GetPathLength(), _swipeRetentionFactor + 1)
    );
    MILO_LOG("GetPathError() %f\n", GetPathError());
    MILO_LOG(
        "GetPathError() / _acceptablePathErrorRatio %f\n",
        GetPathError() / _acceptablePathErrorRatio
    );
    MILO_LOG("GetSwipeAmount() %f\n", GetSwipeAmount());
}

void ArcDetector::SwipeFailed(const Skeleton &skeleton) {
    if (mCurrentSwipeAmt > 0.5)
        mHadProgress = true;
    Vector3 vec = mJointPath.front();
    Clear();
    TryToStartSwipe(vec, skeleton);
}

void ArcDetector::CullPath() {
    if (!mJointPath.empty()) {
        std::list<Vector3> other;
        float first = mJointPath.front().x;
        FOREACH (it, mJointPath) {
            const Vector3 &cur = *it;
            if (mSide == kSkeletonLeft && cur.x >= first) {
                other.push_back(cur);
            }
            if (mSide == kSkeletonRight && cur.x <= first) {
                other.push_back(cur);
            }
        }
        mJointPath = other;
    }
}

void ArcDetector::Draw(const Skeleton &skeleton, SkeletonViz &viz) {
    unsigned int count = mJointPath.size();
    if (count != 0U) {
        std::list<Vector3> arcPath;
        float f31 = 0.0f;
        float f29 = 2.0f;
        float f30 = 0.031415927f;
        int i = 0;

        do {
            float f13 = mSwipeExtentX;
            double temp_d = (double)i;
            float f0 = (float)temp_d * f13 * f30;
            float f12 = f13 * f0;
            float comp = f12 * f29 - (f0 * f0);

            if (!(comp > f31)) {
                f13 = f31;
            } else {
                f13 = sqrtf(comp);
            }

            f13 = -f13;
            int sign = ((-(0 - mSide)) & 2) - 1;
            Vector3 vec(f0 * (float)sign, f31, f13);
            arcPath.insert(arcPath.end(), vec);

            i++;
        } while (i < 100);

        const TrackedJoint *joints = skeleton.TrackedJoints();
        const Vector3 &jpos = joints[mPrimaryJoint].mJointPos[0];
        Vector3 pos(
            jpos.x + mArcOffset.x,
            jpos.z + mArcOffset.z,
            jpos.y + mArcOffset.y
        );

        std::list<Vector3> path1(arcPath);
        DrawPath(path1, viz, Hmx::Color(0.0f, 1.0f, 1.0f, 1.0f), pos);
        arcPath.clear();

        const Vector3 &jpos2 = joints[mSecondaryJoint].mJointPos[0];
        std::list<Vector3> path2(mJointPath);
        DrawPath(path2, viz, Hmx::Color(1.0f, 0.0f, 1.0f, 1.0f), jpos2);
    }
}

void ArcDetector::DrawPath(
    const std::list<Vector3> &path, SkeletonViz &viz, Hmx::Color color, const Vector3 &offset
) const {
    for (std::list<Vector3>::const_iterator it = path.begin(); it != path.end(); ++it) {
        viz.DrawPoint3D(Vector3(it->x + offset.x, it->y + offset.y, it->z + offset.z), 1.0f, color, 1.0f);
    }
}

static float sMinSwipeForLocked = 0.5f;
static int sMinNodesForLocked = 3;
static float sZErrorScale = 1.0f;
static float sSlopeRatioThreshold = 2.0f;

float ArcDetector::GetPathLength() const {
    std::list<Vector3>::const_iterator it = mJointPath.begin();
    unsigned int count = 0;
    if (it != mJointPath.end()) {
        do {
            ++it;
            count++;
        } while (it != mJointPath.end());
        if (count > 1) {
            it = mJointPath.begin();
            float length = 0.0f;
            Vector3 prev = *it;
            ++it;
            if (it != mJointPath.end()) {
                do {
                    float dx = mArcOffset.x - it->x;
                    if (mSide == kSkeletonRight) {
                        dx = dx * -1.0f;
                    }
                    float prevDx = mArcOffset.x - prev.x;
                    if (mSide == kSkeletonRight) {
                        prevDx = prevDx * -1.0f;
                    }
                    if (dx > 0.0f && prevDx > 0.0f) {
                        float comp1 = mSwipeExtentX * prevDx * 2.0f - prevDx * prevDx;
                        float arcY1 = 0.0f;
                        if (0.0f < comp1) {
                            arcY1 = sqrtf(comp1);
                        }
                        float comp2 = mSwipeExtentX * dx * 2.0f - dx * dx;
                        float arcY2 = 0.0f;
                        if (0.0f < comp2) {
                            arcY2 = sqrtf(comp2);
                        }
                        float dz = mSwipeExtentY - mSwipeExtentY;
                        length = sqrtf((dx - prevDx) * (dx - prevDx) + (arcY2 - arcY1) * (arcY2 - arcY1) + dz * dz) + length;
                    }
                    prev = *it;
                    ++it;
                } while (it != mJointPath.end());
                return length;
            }
            return 0.0f;
        }
    }
    return 0.0f;
}

float ArcDetector::GetPathError() const {
    std::list<Vector3>::const_iterator it = mJointPath.begin();
    if (it == mJointPath.end()) {
        return 0.0f;
    }
    float error = 0.0f;
    do {
        Vector3 pt = *it;
        float dx = mArcOffset.x - pt.x;
        if (mSide == kSkeletonRight) {
            dx = dx * -1.0f;
        }
        float comp = mSwipeExtentX * dx * 2.0f - dx * dx;
        float arcY = 0.0f;
        if (0.0f < comp) {
            arcY = sqrtf(comp);
        }
        float errY = (mArcOffset.z - pt.z) - arcY;
        float errZ = (1.0f / sZErrorScale) * (pt.y - mSwipeExtentY);
        error = errZ * errZ + errY * errY + 0.0f + error;
        ++it;
    } while (it != mJointPath.end());
    return error;
}

float ArcDetector::GetSwipeAmount() const {
    float threshold = mSwipeThreshold * 0.3f;
    float adjustedThreshold = (float)mHoverTimer / (float)sDefaultHoverTimer * (mSwipeThreshold - threshold) + threshold;
    float pathLen = GetPathLength();
    float powered = (float)pow((double)pathLen, (double)(_swipeRetentionFactor + 1.0f));
    float pathErr = GetPathError();
    float swipeAmt = (powered - (pathErr / _acceptablePathErrorRatio)) / adjustedThreshold;

    std::list<Vector3>::const_iterator it = mJointPath.begin();
    unsigned int count = 0;
    if (it != mJointPath.end()) {
        do {
            ++it;
            count++;
        } while (it != mJointPath.end());
        if (count > 2) goto done_clamp;
    }
    swipeAmt = 0.5f - swipeAmt >= 0.0f ? swipeAmt : 0.5f;
done_clamp:
    if (mJointPath.begin() != mJointPath.end()) {
        Vector3 front = mJointPath.front();
        Vector3 second = mJointPath.back();
        Vector3 dir(front.x - second.x, front.y - second.y, front.z - second.z);
        Normalize(dir, dir);
        Vector3 boneDir(unk40.z, 0.0f, unk40.x);
        Normalize(boneDir, boneDir);
        if (fabsf(boneDir.x * dir.x + boneDir.z * dir.z + boneDir.y * dir.y) < 0.2f) {
            swipeAmt = 0.9f - swipeAmt >= 0.0f ? swipeAmt : 0.9f;
        }
    }
    return swipeAmt;
}

bool ArcDetector::IsLockedIn() const {
    std::list<Vector3>::const_iterator it = mJointPath.begin();
    unsigned int count = 0;
    if (it != mJointPath.end()) {
        do {
            ++it;
            count++;
        } while (it != mJointPath.end());
        if (count > (unsigned int)sMinNodesForLocked) {
            return true;
        }
    }
    float swipe = GetSwipeAmount();
    if (swipe > sMinSwipeForLocked) {
        return true;
    }
    return false;
}

bool ArcDetector::IsPathAcceptable() const {
    std::list<Vector3>::const_iterator it = mJointPath.begin();
    unsigned int count = 0;
    if (it != mJointPath.end()) {
        do {
            ++it;
            count++;
        } while (it != mJointPath.end());
        if (count > 1) {
            if (IsLockedIn()) {
                float swipe = GetSwipeAmount();
                return 0.0f < swipe;
            }
            const Vector3 &front = mJointPath.front();
            const Vector3 &back = mJointPath.back();
            float dy = front.y - back.y;
            float dx = (float)(int)(((-(unsigned int)(mSide != 0)) & 2) - 1) * (front.x - back.x);
            if (dx < 0.0f) {
                return false;
            }
            if (dy != 0.0f) {
                float invDy = 1.0f / dy;
                if (invDy * dx < sSlopeRatioThreshold &&
                    invDy * (front.z - back.z) < sSlopeRatioThreshold) {
                    return false;
                }
            }
        }
    }
    return true;
}

void ArcDetector::TryToStartSwipe(const Vector3 &pos, const Skeleton &skeleton) {
    MILO_ASSERT(mJointPath.empty(), 0x8B);
    bool tracked = true;
    if (skeleton.TrackedJoints()[mPrimaryJoint].mJointConf != kConfidenceTracked ||
        skeleton.TrackedJoints()[mSecondaryJoint].mJointConf != kConfidenceTracked) {
        tracked = false;
    }
    if (tracked) {
        mJointPath.insert(mJointPath.begin(), pos);
        const TrackedJoint *joints = skeleton.TrackedJoints();
        float dz = joints[mPrimaryJoint].mJointPos[0].z - joints[mSecondaryJoint].mJointPos[0].z;
        float dx = joints[mPrimaryJoint].mJointPos[0].x - joints[mSecondaryJoint].mJointPos[0].x;
        mSwipeExtentX = sqrtf(dx * dx + dz * dz);
    }
}

void ArcDetector::Update(const Skeleton &skeleton, int elapsed) {
    MILO_ASSERT(mInitialized, 0x4A);
    if (!skeleton.IsTracked()) {
        Clear();
    } else {
        const TrackedJoint *joints = skeleton.TrackedJoints();
        float dx = joints[mPrimaryJoint].mJointPos[0].x - joints[mSecondaryJoint].mJointPos[0].x;
        float dy = joints[mPrimaryJoint].mJointPos[0].y - joints[mSecondaryJoint].mJointPos[0].y;
        float dz = joints[mPrimaryJoint].mJointPos[0].z - joints[mSecondaryJoint].mJointPos[0].z;
        Vector3 boneVec(dx, dy, dz);
        unk40 = boneVec;

        if (mJointPath.begin() == mJointPath.end()) {
            TryToStartSwipe(boneVec, skeleton);
        } else if (mHadProgress) {
            const Vector3 &frontPt = *mJointPath.begin();
            float frontX = frontPt.x;
            float frontY = frontPt.y;
            float frontZ = frontPt.z;
            Clear();
            mJointPath.insert(mJointPath.begin(), boneVec);
            if (mSide == kSkeletonLeft && !(dx < frontX + 0.01f)) {
                mHadProgress = false;
            }
            if (mSide == kSkeletonRight && !(dx > frontX - 0.01f)) {
                mHadProgress = false;
            }
        } else {
            mArcOffset = GetCurveStart();
            const Vector3 &frontPt = *mJointPath.begin();
            float distX = dx - frontPt.x;
            float distY = dy - frontPt.y;
            float distZ = dz - frontPt.z;
            if (distX * distX + distZ * distZ + distY * distY > 0.0001f) {
                mJointPath.insert(mJointPath.begin(), boneVec);
            }
            float armDz = joints[mPrimaryJoint].mJointPos[0].z - joints[mSecondaryJoint].mJointPos[0].z;
            float armDx = joints[mPrimaryJoint].mJointPos[0].x - joints[mSecondaryJoint].mJointPos[0].x;
            mSwipeExtentX = (sqrtf(armDx * armDx + armDz * armDz) + mSwipeExtentX) * 0.5f;
        }
        mSwipeExtentY = joints[mPrimaryJoint].mJointPos[0].y - joints[mSecondaryJoint].mJointPos[0].y;
        CullPath();
        float swipe = GetSwipeAmount();
        float curAmt = mCurrentSwipeAmt;
        if (curAmt - swipe < 0.0f) {
            curAmt = swipe;
        }
        mCurrentSwipeAmt = curAmt;
        if (!IsPathAcceptable()) {
            SwipeFailed(skeleton);
        }
        float swipe2 = GetSwipeAmount();
        if (swipe2 < 0.1f) {
            int newTimer = mHoverTimer - elapsed;
            if (newTimer < 0) newTimer = 0;
            mHoverTimer = newTimer;
        }
    }
}

static std::list<Vector3> sOverlayPath;
static DebugMeter *sSwipeMeter;
static DebugMeter *sHoverMeter;

float ArcDetector::UpdateOverlay(RndOverlay *overlay, float y) {
    int numPts = mJointPath.size();
    if (numPts > 1) {
        sOverlayPath.clear();
        for (std::list<Vector3>::const_iterator it = mJointPath.begin(); it != mJointPath.end(); ++it) {
            sOverlayPath.insert(sOverlayPath.end(), *it);
        }
    }
    if (sOverlayPath.begin() != sOverlayPath.end()) {
        float aspectRatio = (float)TheRnd.Height() / (float)TheRnd.Width();
        float halfArcScale = aspectRatio / (mSwipeExtentX * 2.0f);
        float drawY = y;

        Vector2 prevScaled;
        for (std::list<Vector3>::const_iterator it = sOverlayPath.begin(); it != sOverlayPath.end(); ++it) {
            Vector3 pt = *it;
            float dx = mArcOffset.x - pt.x;
            if (mSide == kSkeletonRight) {
                dx = dx * -1.0f;
            }
            float scaledX = dx * halfArcScale;
            float errZ = pt.z;
            float comp = mSwipeExtentX * dx * 2.0f - dx * dx;
            float arcY = 0.0f;
            if (0.0f < comp) {
                arcY = sqrtf(comp);
            }

            TheRnd.DrawStringScreen(
                MakeString("%f %f", dx, (mArcOffset.z - pt.z)),
                Vector2(0.6f, drawY),
                Hmx::Color(1.0f, 1.0f, 1.0f, 1.0f),
                true
            );
            UtilDrawCircle2D(Vector2(scaledX, arcY), 0.01f, Hmx::Color(0.0f, 0.0f, 0.0f, 1.0f), 4);
            UtilDrawCircle2D(Vector2(pt.x, pt.y), 0.004f, Hmx::Color(0.0f, 0.0f, 0.0f, 1.0f), 4);

            float yAdj = (mArcOffset.y - pt.y);
            Vector2 errPt(scaledX, yAdj + 0.75f);
            UtilDrawCircle2D(errPt, 0.01f, Hmx::Color(0.0f, 0.0f, 0.0f, 1.0f), 4);

            Vector2 arcPt(scaledX, (mArcOffset.y - pt.y) + 0.75f);
            UtilDrawCircle2D(arcPt, 0.01f, Hmx::Color(0.0f, 0.0f, 0.0f, 1.0f), 4);

            if (pt.x != sOverlayPath.front().x || pt.y != sOverlayPath.front().y || pt.z != sOverlayPath.front().z) {
                UtilDrawLine(prevScaled, Vector2(scaledX, arcY), Hmx::Color(0.0f, 0.0f, 0.0f, 1.0f));
            }

            prevScaled = Vector2(scaledX, arcY);
            float increment = 0.03125f;
            drawY = drawY + increment;
        }

        const Vector3 &front = *mJointPath.begin();
        Skeleton *skel = TheGestureMgr->GetActiveSkeleton();
        float handX, handY, handZ;
        if (skel != NULL) {
            const TrackedJoint *joints = skel->TrackedJoints();
            handX = joints[mPrimaryJoint].mJointPos[0].x - joints[mSecondaryJoint].mJointPos[0].x;
            handY = joints[mPrimaryJoint].mJointPos[0].y - joints[mSecondaryJoint].mJointPos[0].y;
            handZ = joints[mPrimaryJoint].mJointPos[0].z - joints[mSecondaryJoint].mJointPos[0].z;
        } else {
            handX = front.x;
            handY = front.y;
            handZ = front.z;
        }

        float curDx = mArcOffset.x - handX;
        if (mSide == kSkeletonRight) {
            curDx = curDx * -1.0f;
        }
        float curScaledX = curDx * halfArcScale;
        float curScaledY = mArcOffset.z - handZ;
        UtilDrawCircle2D(Vector2(curScaledX, curScaledY), 0.015f, Hmx::Color(1.0f, 1.0f, 0.0f, 1.0f), 4);
        UtilDrawLine(Vector2(curScaledX, curScaledY), Vector2(aspectRatio * 0.5f, 0.0f), Hmx::Color(1.0f, 1.0f, 1.0f, 1.0f));

        Vector2 curErrPt(curScaledX, (mArcOffset.y - handY) + 0.75f);
        UtilDrawCircle2D(curErrPt, 0.015f, Hmx::Color(0.0f, 0.0f, 1.0f, 1.0f), 4);

        float pathErr = GetPathError();
        TheRnd.DrawStringScreen(
            MakeString("Sum of error squares: %f", pathErr),
            Vector2(0.1f, y),
            Hmx::Color(1.0f, 1.0f, 1.0f, 1.0f),
            true
        );

        float pathLen = GetPathLength();
        TheRnd.DrawStringScreen(
            MakeString("Length of path: %f", pathLen),
            Vector2(0.1f, y + 0.03125f),
            Hmx::Color(1.0f, 1.0f, 1.0f, 1.0f),
            true
        );

        if (IsLockedIn()) {
            TheRnd.DrawStringScreen(
                "LOCKED IN",
                Vector2(0.1f, y + 0.0625f),
                Hmx::Color(0.0f, 1.0f, 0.0f, 1.0f),
                true
            );
        } else {
            TheRnd.DrawStringScreen(
                "NOT LOCKED IN",
                Vector2(0.1f, y + 0.0625f),
                Hmx::Color(1.0f, 0.0f, 0.0f, 1.0f),
                true
            );
        }

        TheRnd.DrawStringScreen(
            MakeString("Arc size: %f", mSwipeExtentX),
            Vector2(0.1f, y + 0.09375f),
            Hmx::Color(1.0f, 1.0f, 1.0f, 1.0f),
            true
        );

        if (!sSwipeMeter) {
            sSwipeMeter = new DebugMeter(0.1f, 0.1f, 0.8f, 0.03f, Hmx::Color(0.0f, 0.0f, 0.0f, 1.0f));
        }
        sSwipeMeter->Draw();
        sSwipeMeter->DrawBar(0.0f, GetSwipeAmount(), Hmx::Color(0.0f, 1.0f, 0.0f, 1.0f));

        if (!sHoverMeter) {
            sHoverMeter = new DebugMeter(0.1f, 0.1f, 0.6f, 0.03f, Hmx::Color(0.0f, 0.0f, 0.0f, 1.0f));
        }
        sHoverMeter->Draw();
        sHoverMeter->DrawBar(0.0f, (float)mHoverTimer / (float)sDefaultHoverTimer, Hmx::Color(1.0f, 1.0f, 0.0f, 1.0f));

        y = drawY;
    }
    return y;
}
