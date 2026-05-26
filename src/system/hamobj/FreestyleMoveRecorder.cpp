#include "hamobj/FreestyleMoveRecorder.h"
#include "gesture/BaseSkeleton.h"
#include "gesture/GestureMgr.h"
#include "gesture/SkeletonUpdate.h"
#include "gesture/SkeletonViz.h"
#include "hamobj/DancerSkeleton.h"
#include "hamobj/FreestyleMove.h"
#include "math/Color.h"
#include "math/Geo.h"
#include "math/Vec.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "os/DateTime.h"
#include "rndobj/Rnd.h"
#include "rndobj/Tex.h"
#include "utl/FileStream.h"
#include "utl/Symbol.h"
#include "utl/OSCMessenger.h"
#ifdef HX_NATIVE
inline double __fsel(double a, double b, double c) { return a >= 0.0 ? b : c; }
#else
#include "xdk/LIBCMT/ppcintrinsics.h"
#endif
#include <cfloat>

#ifndef HX_NATIVE
namespace {
    struct DebugGraph {
        DebugGraph(const Hmx::Color &c) {
            unk0.resize(200);
            unk8 = 0;
            unkc = c;
        }

        std::vector<float> unk0;
        int unk8;
        Hmx::Color unkc;
    };

    std::vector<DebugGraph> gDebugGraphs;
}
#endif // !HX_NATIVE

static const float kE = 2.71828182845905f;

DancerSkeleton sLastComparedDancerSkel;
static int sLastBeatMod;
#ifndef HX_NATIVE
static SkeletonViz *sVizRecorded = nullptr;
static SkeletonViz *sVizLive = nullptr;
static float sDebugRectX = 0.1f;
static float sDebugRectY = 0.3f;
static float sDebugRectW = 0.3f;
#endif // !HX_NATIVE

FreestyleMoveRecorder *FreestyleMoveRecorder::sInstance = nullptr;

FreestyleMoveRecorder::FreestyleMoveRecorder()
    : mPlaybackSpeed(0), mClipFrames(0), mClipFrameCount(0), mRecordingFrames(0), mLastFrameIndex(-1), mMaxFrames(60), mRecordPos(-1), mPlaybackPos(-1),
      mDefaultTimeout(15), mPlaybackIndex(-1), mRecording(0), mPlaybackActive(0), mSkeletonIndex(-1), mCurrentTakeIndex(0) {
#ifdef HX_NATIVE
    // No depth-buffer palette on native (only used by Poll's depth rendering)
    mPlayerPalette = nullptr;
#else
    mPlayerPalette = Hmx::Object::New<RndTex>();
    mPlayerPalette->SetBitmap(320, 240, 16, RndTex::kRegularLinear, false, nullptr);
#endif

    JointAngle angle;
    angle.mJoint = kJointHandRight;
    mAngleLimits.push_back(angle);
    angle.mJoint = kJointHandLeft;
    mAngleLimits.push_back(angle);
    angle.mJoint = kJointAnkleRight;
    mAngleLimits.push_back(angle);
    angle.mJoint = kJointAnkleLeft;
    mAngleLimits.push_back(angle);
    angle.mJoint = kJointKneeRight;
    mAngleLimits.push_back(angle);
    angle.mJoint = kJointKneeLeft;
    mAngleLimits.push_back(angle);
    mTrackedJoints.push_back(kJointHandRight); // 11
    mTrackedJoints.push_back(kJointHandLeft); // 7
    mTrackedJoints.push_back(kJointAnkleRight); // 17
    mTrackedJoints.push_back(kJointAnkleLeft); // 14
    mTrackedJoints.push_back(kJointHead); // 3
    mTrackedJoints.push_back(kJointHipCenter); // 0
    JointPos pos;
    pos.mJoint = 11;
    pos.unk4 = 2;
    mPositions.push_back(pos);
    pos.mJoint = 7;
    pos.unk4 = 1;
    mPositions.push_back(pos);
    pos.mJoint = 9;
    pos.unk4 = 2;
    mPositions.push_back(pos);
    pos.mJoint = 5;
    pos.unk4 = 1;
    mPositions.push_back(pos);
    pos.mJoint = 17;
    pos.unk4 = 4;
    mPositions.push_back(pos);
    pos.mJoint = 14;
    pos.unk4 = 3;
    mPositions.push_back(pos);
    pos.mJoint = 16;
    pos.unk4 = 4;
    mPositions.push_back(pos);
    pos.mJoint = 13;
    pos.unk4 = 3;
    mPositions.push_back(pos);
    mFrameBuffer = new FreestyleMoveFrame[mMaxFrames];
#ifndef HX_NATIVE
    // Debug recording functions -- use devkit:\ paths, Xbox-only
    DataRegisterFunc("bam_record_attempt", OnRecordAttempt);
    DataRegisterFunc("bam_write_created", OnWriteCreated);
    DataRegisterFunc("bam_read_created", OnReadCreated);
    DataRegisterFunc("bam_read_attempt", OnReadAttempt);
    DataRegisterFunc("bam_clear", OnClearAttempt);
#endif
}

FreestyleMoveRecorder::~FreestyleMoveRecorder() {
    delete mPlayerPalette;
    delete[] mFrameBuffer;
    delete[] mRecordingFrames;
    delete[] mClipFrames;
}

void FreestyleMoveRecorder::Free() {
    mRecordPos = -1;
    mPlaybackPos = -1;
    for (int i = 4; i != 0; i--) {
        mTakes[mCurrentTakeIndex].Free();
    }
}

void FreestyleMoveRecorder::UpdateFakeSkeleton() {
    mPlaybackSpeed += TheTaskMgr.DeltaUISeconds();
    int beatMod = (int)TheTaskMgr.Beat() % 4;
    if (beatMod == 0 && sLastBeatMod != 0) {
        mPlaybackSpeed = 0;
    }
    sLastBeatMod = beatMod;
}

// Poll() accesses LiveCameraInput depth buffers -- Xbox-only.
#ifdef HX_NATIVE
void FreestyleMoveRecorder::Poll() {}
#else
void FreestyleMoveRecorder::Poll() {
    int recordFrame;
    if (mRecordPos >= 0.0f) {
        recordFrame = (int)(mDefaultTimeout * mRecordPos) - 2;
    } else {
        recordFrame = -1;
    }

    int playbackFrame;
    if (mPlaybackPos >= 0.0f) {
        playbackFrame = (int)(mDefaultTimeout * mPlaybackPos);
    } else {
        playbackFrame = -1;
    }

    int maxFrame = mMaxFrames - 1;
    if (maxFrame < recordFrame) {
        recordFrame = maxFrame;
    }

    if (recordFrame >= 0 && mLastFrameIndex != mCurrentTakeIndex) {
        LiveCameraInput *camInput = TheGestureMgr->GetLiveCameraInput();
        if (!camInput->mDepthPolled) {
            camInput->PollNewStream(LiveCameraInput::kBufferDepth);
        }
        RndTex *streamTex = camInput->GetStreamTex(LiveCameraInput::kBufferDepth);
        if (streamTex) {
            void *texels = nullptr;
            char *depthDst =
                (char *)mTakes[mCurrentTakeIndex].mDepthFrames + recordFrame * 0x12c0;
            streamTex->TexelsLock(texels);
            if (texels) {
                int playerIdx = mSkeletonIndex;
                mTakes[mCurrentTakeIndex].unkc = playerIdx;
                unsigned short *src = (unsigned short *)texels;
                char *dst = depthDst - 0x50;
                int col = 0;
                do {
                    for (int row = 0x3c; row != 0; row--) {
                        int pixelPlayer = (*src & 7) - 1;
                        unsigned long depth;
                        if (pixelPlayer == playerIdx) {
                            depth = (*src >> 7) & 0xFF;
                        } else if (playerIdx >= 0) {
                            depth = (*src >> 7) & 0xFF;
                        } else {
                            depth = 0;
                        }
                        src += 0x600;
                        *(unsigned char *)(dst += 0x50) = (unsigned char)depth;
                    }
                    col++;
                    texels = (char *)texels + 8;
                    src = (unsigned short *)texels;
                } while (col < 0x50);
            }
            streamTex->TexelsUnlock();
        }

        if (recordFrame == 0) {
            mTakes[mCurrentTakeIndex].CalcCentering(0);
        }

        int nextFrame = recordFrame + 1;
        if (nextFrame < mPlaybackIndex || mPlaybackIndex == -1) {
            if (!mRecording) {
                mTakes[mCurrentTakeIndex].mNumFrames = nextFrame;
                float recordPosVal = mRecordPos;
                BaseSkeleton *skel = GetLiveSkeleton();
                float beat = recordPosVal * 1000.0f;
                mTakes[mCurrentTakeIndex].RecordSkeletonFrame(skel, recordFrame, beat);
            } else {
                BaseSkeleton *skel = GetLiveSkeleton();
                DancerSkeleton tempSkel;
                tempSkel.Init();
                float beat = mRecordPos * 1000.0f;
                if (skel && skel->IsTracked()) {
                    tempSkel.Set(*skel);
                }
                FreestyleMoveFrame *frame = &mFrameBuffer[recordFrame];
                frame->skeleton = tempSkel;
                frame->mBeat = beat;
                mDancerTakeFrameCount = nextFrame;
            }

            int frameIdx = mFrameIndex;
            if (mFrameIndex < nextFrame) {
                frameIdx = nextFrame;
            }
            mFrameIndex = frameIdx;
        } else {
            mRecording = false;
            mPlaybackIndex = -1;
            mRecordPos = -1.0f;
        }
    }

    if (playbackFrame >= 0
        && (playbackFrame < mTakes[mCurrentTakeIndex].mNumFrames || mPlaybackActive)) {
        void *texels = nullptr;
        mPlayerPalette->TexelsLock(texels);

        int prevFrame = playbackFrame - 1;
        int lastFrame = mFrameIndex - 1;
        if (prevFrame <= lastFrame) {
            lastFrame = prevFrame;
            if (prevFrame < 0) {
                lastFrame = 0;
            }
        }

        int takeIdx = mCurrentTakeIndex;
        char *depthBase = (char *)mTakes[takeIdx].mDepthFrames + lastFrame * 0x12c0;
        int centerX = mTakes[takeIdx].unk10 << 2;
        int minDepth = mTakes[takeIdx].unk14 - 0x7a;

        if (mPlaybackActive) {
            centerX = 0;
            minDepth = 0;
        }

        int unkColor = mTakes[takeIdx].unkc;
        int colorMask = unkColor + 1;
        int pixelX = 0;
        char *texelPtr = (char *)texels;
        for (int col = 0; col < 0x140; col++) {
            int x = pixelX + centerX;
            unsigned int row = 0;
            unsigned short *ptr = (unsigned short *)(texelPtr - 0x300);
            for (int r = 0xf0; r != 0; r--) {
                unsigned int depthVal = 0;
                if ((int)x >= 0 && (int)x < 0x140) {
                    int depthY = (int)row / 4;
                    int depthX = (int)x / 4;
                    depthVal = (unsigned int)*(unsigned char *)(
                        depthBase + depthY * 0x50 + depthX
                    );
                }
                row++;
                ptr += 0x180;
                unsigned short depthU16 = (unsigned short)depthVal;
                unsigned int diff = depthVal - minDepth;
                unsigned int shifted = diff << 7;
                unsigned int depthMask = (depthU16 > 0) ? 0xFFFFFFFF : 0;
                *ptr = (unsigned short)(shifted | (depthMask & colorMask));
            }
            pixelX++;
            texelPtr += 2;
        }

        mPlayerPalette->TexelsUnlock();
    }

    if (0.0f <= mRecordPos) {
        mRecordPos += TheTaskMgr.DeltaUISeconds();
    }
    if (0.0f <= mPlaybackPos) {
        mPlaybackPos += TheTaskMgr.DeltaUISeconds();
    }
    UpdateFakeSkeleton();
}
#endif // Poll

// DrawDebug uses SkeletonViz with CameraInput -- Xbox-only.
#ifdef HX_NATIVE
void FreestyleMoveRecorder::DrawDebug() {}
#else
void FreestyleMoveRecorder::DrawDebug() {
    if (DataVariable("bam_debug").Int()) {
        if (sVizRecorded == nullptr) {
            sVizRecorded = Hmx::Object::New<SkeletonViz>();
            sVizRecorded->Init();
            sVizLive = Hmx::Object::New<SkeletonViz>();
            sVizLive->Init();
        }

        SkeletonUpdateHandle handle = SkeletonUpdate::InstanceHandle();

        std::vector<SkeletonCallback *> callbacks;
        callbacks.push_back(this);

        float screenScale = sDebugRectW / TheRnd.YRatio();

        Hmx::Rect rect1(sDebugRectX, sDebugRectY, sDebugRectW, screenScale);
        Hmx::Color bgColor1(0, 0, 0, 0.4f);
        TheRnd.DrawRectScreen(rect1, bgColor1, nullptr, nullptr, nullptr);

        sVizRecorded->SetUsePhysicalCam(true);
        sVizRecorded->SetPhysicalCamScreenRect(rect1);
        sVizRecorded->Visualize(
            *handle.GetCameraInput(), sLastComparedDancerSkel, &callbacks, false
        );

        float screenScale2 = sDebugRectW / TheRnd.YRatio();
        Hmx::Rect rect2(sDebugRectX + sDebugRectW + 0.1f, sDebugRectY, sDebugRectW, screenScale2);
        Hmx::Color bgColor2(0, 0, 0, 0.4f);
        TheRnd.DrawRectScreen(rect2, bgColor2, nullptr, nullptr, nullptr);

        sVizLive->SetUsePhysicalCam(true);
        sVizLive->SetPhysicalCamScreenRect(rect2);
        BaseSkeleton *liveSkel = GetLiveSkeleton();
        if (liveSkel) {
            sVizLive->Visualize(
                *handle.GetCameraInput(), *liveSkel, &callbacks, false
            );
        }
    }
}
#endif // DrawDebug

// Recording functions -- touch depth frame allocation (Xbox-only).
#ifdef HX_NATIVE
void FreestyleMoveRecorder::StartRecording() {}
void FreestyleMoveRecorder::StartRecordingDancerTake() {}
void FreestyleMoveRecorder::StopRecording() {}
#else
void FreestyleMoveRecorder::StartRecording() {
    mPlaybackIndex = 0xffffffff;
    mRecording = false;
    mRecordPos = 0;
    mPlaybackPos = -1;
    if (mLastFrameIndex != mCurrentTakeIndex) {
        mTakes[mCurrentTakeIndex].Init(mMaxFrames);
    }
}

void FreestyleMoveRecorder::StartRecordingDancerTake() {
    StartRecording();
    mRecording = true;
}

void FreestyleMoveRecorder::StopRecording() {
    mPlaybackIndex = mTakes[mCurrentTakeIndex].mNumFrames + 2;
}
#endif // Recording

void FreestyleMoveRecorder::ClearRecording() {
    if (mLastFrameIndex != mCurrentTakeIndex) {
        mTakes[mCurrentTakeIndex].Clear();
    }
    mFrameIndex = 0;
}

void FreestyleMoveRecorder::StartPlayback(bool param_1) {
    mPlaybackActive = param_1;
    mPlaybackPos = 0;
}

void FreestyleMoveRecorder::StopPlayback() { mPlaybackPos = -1; }

void FreestyleMoveRecorder::ClearDancerTake() { mDancerTakeFrameCount = 0; }

void FreestyleMoveRecorder::AssignStaticInstance() { sInstance = this; }

BaseSkeleton *FreestyleMoveRecorder::GetLiveSkeleton() {
    int numFrames = mClipFrameCount;
    if (numFrames > 0) {
        int count = 0;
        int idx = 0;
        int byteOff = 0;
        do {
            if (count == unk40)
                break;
            float *base = (float *)((char *)mClipFrames + byteOff);
            if (base[0x2d8 / 4] > base[0x5b4 / 4]) {
                count++;
            }
            idx++;
            byteOff += 0x2dc;
        } while (idx < numFrames);

        if (idx < numFrames) {
            int off = idx * 0x2dc;
            do {
                if (*(float *)((char *)mClipFrames + off + 0x2d8) > mPlaybackSpeed * 1000.0f)
                    break;
                idx++;
                off += 0x2dc;
            } while (idx < numFrames);
        }

        return (BaseSkeleton *)((char *)mClipFrames + idx * 0x2dc);
    }
    if (mSkeletonIndex >= 0) {
        return &TheGestureMgr->GetSkeleton(mSkeletonIndex);
    }
    return NULL;
}

void FreestyleMoveRecorder::UpdateRecordingAttempt(
    const BaseSkeleton *skeleton, float f2
) {
    if (mClipName != gNullStr) {
        mRecordingFrames[mRecordingFrameCount].skeleton.Set(*skeleton);
        mRecordingFrames[mRecordingFrameCount].mBeat = f2;
        mRecordingFrameCount++;
    }
}

void FreestyleMoveRecorder::PlaybackComplete() {
#ifndef HX_NATIVE
    if (mClipName != gNullStr) {
        WriteRecordedMoveAttempt();
    }
#endif
}

void FreestyleMoveRecorder::ClearFrameScores() {
    for (int i = 0; i < 2; i++) {
        unke4[i].Clear();
    }
}

// Devkit file I/O and debug recording -- uses devkit:\ paths (Xbox-only).
// ReadFreestyleMoveClip is declared in the header but never called on native.
#ifdef HX_NATIVE
void FreestyleMoveRecorder::ReadFreestyleMoveClip(String, int &, FreestyleMoveFrame *) {}
#else
void FreestyleMoveRecorder::RecordMoveAttempt(String str) {
    mClipName = str;
    delete[] mRecordingFrames;
    mRecordingFrames = new FreestyleMoveFrame[480];
    mRecordingFrameCount = 0;
}

void FreestyleMoveRecorder::WriteRecordedMoveAttempt() {
    WriteFreestyleMoveClip(mClipName, mRecordingFrameCount, mRecordingFrames);
    mClipName = gNullStr;
    delete[] mRecordingFrames;
    mRecordingFrames = nullptr;
    mRecordingFrameCount = 0;
}

void FreestyleMoveRecorder::ClearFreestyleMoveClip() {
    delete[] mClipFrames;
    mClipFrames = nullptr;
    mClipFrameCount = 0;
}

void FreestyleMoveRecorder::WriteFreestyleMoveClip(
    String str, int framecount, FreestyleMoveFrame *frames
) {
    if (str.length() > 0x26) {
        str.resize(0x26);
    }
    str += ".bamclp";
    const char *path = MakeString("devkit:\\%s", str);
    FileStream stream(path, FileStream::kWrite, true);
    stream << mRecordingTarget;
    stream << framecount;
    for (int i = 0; i < framecount; i++) {
        frames[i].skeleton.Write(stream);
        stream << frames[i].mBeat;
    }
    MILO_LOG("Saved clip to %s, framecount: %d\n", path, framecount);
}

void FreestyleMoveRecorder::ReadFreestyleMoveClip(
    String str, int &framecount, FreestyleMoveFrame *frames
) {
    if (str.length() > 0x26) {
        str.resize(0x26);
    }
    str += ".bamclp";
    const char *path = MakeString("devkit:\\%s", str);
    FileStream stream(path, FileStream::kRead, true);
    Symbol s;
    stream >> s;
    stream >> framecount;
    for (int i = 0; i < framecount; i++) {
        frames[i].skeleton.Read(stream);
        stream >> frames[i].mBeat;
    }
    MILO_LOG("Loaded clip that was recorded with %s, framecount: %d\n", s, framecount);
}

DataNode FreestyleMoveRecorder::OnRecordAttempt(DataArray *a) {
    String str;
    if (a->Size() >= 2) {
        str = a->Str(1);
    } else {
        str = sInstance->mRecordingTarget.Str();
        str += "_attempt_";
        DateTime dt;
        GetDateAndTime(dt);
        str += MakeString("%02d%02d_%02d%02d", dt.Month(), dt.mDay, dt.mHour, dt.mMin);
    }
    sInstance->RecordMoveAttempt(str);
    return 0;
}

DataNode FreestyleMoveRecorder::OnWriteCreated(DataArray *a) {
    String str;
    if (a->Size() >= 2) {
        str = a->Str(1);
    } else {
        str = sInstance->mRecordingTarget.Str();
        str += "_created_";
        DateTime dt;
        GetDateAndTime(dt);
        str += MakeString("%02d%02d_%02d%02d", dt.Month(), dt.mDay, dt.mHour, dt.mMin);
    }
    sInstance->WriteFreestyleMoveClip(
        str,
        sInstance->mTakes[sInstance->mCurrentTakeIndex].mNumFrames,
        sInstance->mTakes[sInstance->mCurrentTakeIndex].mFrames
    );
    return 0;
}

DataNode FreestyleMoveRecorder::OnReadCreated(DataArray *a) {
    int framecount;
    sInstance->ReadFreestyleMoveClip(
        a->Str(1), framecount, sInstance->mTakes[sInstance->mCurrentTakeIndex].mFrames
    );
    sInstance->mTakes[sInstance->mCurrentTakeIndex].Init(sInstance->mMaxFrames);
    sInstance->mTakes[sInstance->mCurrentTakeIndex].mNumFrames = framecount;
    sInstance->mLastFrameIndex = sInstance->mCurrentTakeIndex;
    return 0;
}

DataNode FreestyleMoveRecorder::OnReadAttempt(DataArray *a) {
    delete[] sInstance->mClipFrames;
    sInstance->mClipFrames = new FreestyleMoveFrame[480];
    sInstance->ReadFreestyleMoveClip(a->Str(1), sInstance->mClipFrameCount, sInstance->mClipFrames);
    return 0;
}

DataNode FreestyleMoveRecorder::OnClearAttempt(DataArray *a) {
    sInstance->ClearFreestyleMoveClip();
    return 0;
}
#endif // devkit I/O

void FreestyleMoveRecorder::CompareDisplacementVectors(
    const Vector3 &v1, int count1, const Vector3 &v2, int count2, float &outSimilarity, float &outMaxDisp
) const {
    float zero = 0.0f;
    float len1 = Length(v1);
    float len2 = Length(v2);

    float avgDisp1;
    if (count1 != 0) {
        avgDisp1 = len1 / (float)count1;
    } else {
        avgDisp1 = zero;
    }

    float avgDisp2;
    if (count2 != 0) {
        avgDisp2 = len2 / (float)count2;
    } else {
        avgDisp2 = zero;
    }

    float maxDisp = (float)__fsel(avgDisp1 - avgDisp2, avgDisp1, avgDisp2);
    outMaxDisp = maxDisp + 1e-5f;

    float invLen1;
    if (0.0f < len1) {
        invLen1 = 1.0f / len1;
    } else {
        invLen1 = zero;
    }

    float n1x = v1.x * invLen1;
    float n1y = v1.y * invLen1;
    float n1z = invLen1 * v1.z;

    float invLen2;
    if (0.0f < len2) {
        invLen2 = 1.0f / len2;
    } else {
        invLen2 = zero;
    }

    float n2z = invLen2 * v2.z;
    float n2x = v2.x * invLen2;
    float n2y = v2.y * invLen2;

    float dot = (n2x * n1x + n2y * n1y + n2z * n1z) * 0.87f;
    float angleDiff = -(dot - 1.0f);

    float clamped = (float)__fsel(-angleDiff, zero, angleDiff);
    float clamped1 = (float)__fsel(clamped - 1.0f, 1.0f, clamped);
    float score = clamped1 * clamped1 * 20.0f;
    float finalScore = (float)__fsel(-score, zero, score);
    float finalClamped = (float)__fsel(finalScore - 1.0f, 1.0f, finalScore);
    outSimilarity = 1.0f - finalClamped;
}

float FreestyleMoveRecorder::CompareSkeletonPositions(
    const BaseSkeleton *skel1, const BaseSkeleton *skel2, float scale
) const {
    if (skel1 && skel2) {
        if (skel1->IsTracked()) {
            if (skel2->IsTracked()) {
                int count = 0;
                float totalDist = 0.0f;
                float zero = 0.0f;
                if (mPositions.size() > 0) {
                    int idx = 0;
                    do {
                        Vector3 pos1, pos2;
                        skel1->NormPos(
                            (SkeletonCoordSys)mPositions[count].unk4,
                            (SkeletonJoint)mPositions[count].mJoint, pos1
                        );
                        skel2->NormPos(
                            (SkeletonCoordSys)mPositions[count].unk4,
                            (SkeletonJoint)mPositions[count].mJoint, pos2
                        );
                        count++;
                        idx += 8;
                        totalDist = (pos1.z - pos2.z) * (pos1.z - pos2.z)
                            + (pos1.y - pos2.y) * (pos1.y - pos2.y)
                            + (pos1.x - pos2.x) * (pos1.x - pos2.x) + totalDist;
                    } while ((unsigned int)count < (unsigned int)mPositions.size());
                }
                float avg = totalDist / (float)(unsigned int)mAngleLimits.size();
                float result = avg * scale;
                float clamped = (float)__fsel(-result, zero, result);
                float clamped1 = (float)__fsel(clamped - 1.0f, 1.0f, clamped);
                return 1.0f - clamped1;
            }
        }
    }
    return 0.0f;
}

float FreestyleMoveRecorder::CompareSkeletonJointDisplacement(
    const FreestyleMoveFrame *frames, int frameIdx, const BaseSkeleton *liveSkel, float &outTotalWeight
) const {
    // Compute trackedCount from vector pointers: (end - begin) >> 2 (4 bytes per element)
    auto _tmp0 = mTrackedJoints.end();
    auto _tmp1 = mTrackedJoints.begin();
    int trackedCount = (int)(_tmp0 - _tmp1) >> 2;
    // Compute clamped prev-frame index: max(0, frameIdx-1)
    unsigned int clampedPrev = (~((int)(unsigned int)(frameIdx - 1) >> 31)) & (unsigned int)(frameIdx - 1);
    float totalScore = 0.0f;
    float totalWeight = 0.0f;
    if (trackedCount != 0) {
        const FreestyleMoveFrame *prevFrame = &frames[clampedPrev];
        const FreestyleMoveFrame *curFrame = &frames[frameIdx];
        unsigned int i = 0;
        do {
            SkeletonJoint joint = mTrackedJoints[i];
            Vector3 curJointPos, prevJointPos;
            curFrame->skeleton.JointPos(kCoordCamera, joint, curJointPos);
            prevFrame->skeleton.JointPos(kCoordCamera, joint, prevJointPos);
            int beatDiff = (int)(curFrame->mBeat - prevFrame->mBeat);
            // Build displacement vector (y=0.0 zeroed — ignore depth component)
            Vector3 dispOffset;
            dispOffset.x = curJointPos.x - prevJointPos.x;
            dispOffset.y = 0.0f;
            dispOffset.z = curJointPos.z - prevJointPos.z;
            float similarity = 0.0f;
            float maxDisp = 0.0f;
            {
                SkeletonUpdateHandle handle = SkeletonUpdate::InstanceHandle();
                const SkeletonHistory *history = handle.History();
#ifdef HX_NATIVE
                // History may be null before GestureMgr init or after terminate
                if (history) {
#endif
                Vector3 liveDisp;
                int liveCount = 0;
                bool hasDisp = liveSkel->Displacement(history, kCoordCamera, joint, beatDiff, liveDisp, liveCount);
                if (hasDisp) {
                    CompareDisplacementVectors(dispOffset, beatDiff, liveDisp, liveCount, similarity, maxDisp);
                }
#ifdef HX_NATIVE
                }
#endif
            }
            i++;
            totalScore += similarity * maxDisp;
            totalWeight += similarity;
        } while (i < (unsigned int)trackedCount);
        if (0.0f < totalWeight) {
            totalScore /= totalWeight;
        }
    }
    outTotalWeight = totalWeight;
    return totalScore;
}

void FreestyleMoveRecorder::CalcFrameScore(
    FreestyleFrameScores &scores, const FreestyleMoveFrame *frames, int numFrames,
    const BaseSkeleton *liveSkel, float beatOffset
) const {
    struct TemporalWindow {
        int frameIdx;
        float weight;
    };
    std::vector<TemporalWindow> windows;
    float oscTimeout = TheOSCMessenger.GetFloat(String("/temporalwindow"), 250.0f);
    float bestDist = FLT_MAX;
    int bestIdx = 0;
    int i = 0;
    if (numFrames > 0) {
        do {
            float dist = frames[i].mBeat - beatOffset;
            float absDist = (float)fabs((double)dist);
            if (absDist < bestDist) {
                bestDist = absDist;
                bestIdx = i;
            }
            if (absDist < oscTimeout) {
                TemporalWindow tw;
                tw.frameIdx = i;
                // Gaussian weight: exp(-dist^2 / (oscTimeout*0.5)^2)
                float distSq = (float)pow((double)dist, 2.0);
                float halfSq = (float)pow((double)(oscTimeout * 0.5f), 2.0);
                float ratio = distSq / halfSq;
                tw.weight = (float)pow(2.718281828459045, (double)(-ratio));
                windows.push_back(tw);
            }
            i++;
        } while (i < numFrames);
    }
    // Fallback: push best frame with weight 1.0 if no windows found
    if (windows.begin() == windows.end()) {
        TemporalWindow tw;
        tw.frameIdx = bestIdx;
        tw.weight = 1.0f;
        windows.push_back(tw);
    }
    // Accumulate score across windows if liveSkel is valid
    float totalScore = 0.0f;
    if (liveSkel != nullptr) {
        int windowCount = (int)(windows.end() - windows.begin());
        if (windowCount != 0) {
            const TemporalWindow *winPtr = windows.begin();
            int i = 0;
            do {
                int frameIdx = winPtr->frameIdx;
                float twWeight = winPtr->weight;
                // Best frame always gets weight 1.0
                if (frameIdx == bestIdx) {
                    twWeight = 1.0f;
                }
                // Displacement score
                float outTotalWeight;
                float dispScore = CompareSkeletonJointDisplacement(frames, frameIdx, liveSkel, outTotalWeight);
                float dispContrib = outTotalWeight * (dispScore * twWeight);
                // Position score
                float poseWeight = TheOSCMessenger.GetFloat(String("/poserrorweight"), 3.5f);
                float posScore = CompareSkeletonPositions(
                    (const BaseSkeleton *)&frames[frameIdx].skeleton, liveSkel, poseWeight
                );
                float posContrib = posScore * twWeight;
                // Select contribution: max(dispContrib, posContrib)
                float maxContrib = (float)__fsel(dispContrib - posContrib, dispContrib, posContrib);
                if (maxContrib == 0.0f) {
                    // nothing
                } else if (maxContrib == dispContrib) {
                    // dispContrib wins — normalize by outTotalWeight
                    totalScore += dispContrib / outTotalWeight;
                } else if (maxContrib == posContrib) {
                    // posContrib wins
                    totalScore += posContrib;
                }
                i++;
                winPtr++;
            } while ((unsigned int)i < (unsigned int)windowCount);
        }
    }
    // Clamp totalScore to [0, 1]
    float clampedScore = (float)__fsel(-totalScore, 0.0f, totalScore);
    clampedScore = (float)__fsel(clampedScore - 1.0f, 1.0f, clampedScore);
    // Debug OSC send
    TheOSCMessenger.SendOSCFloat(String("/framescore"), clampedScore);
    // Store max(clampedScore, existing) at bestIdx in scores
    float *scoreData = scores.unk0.begin();
    float prev = scoreData[bestIdx];
    scoreData[bestIdx] = (float)__fsel(clampedScore - prev, clampedScore, prev);
    // Update unkc = max(unkc, bestIdx + 1)
    int newCount = bestIdx + 1;
    int maxCount = newCount;
    if (scores.unkc >= newCount)
        maxCount = scores.unkc;
    scores.unkc = maxCount;
}

float FreestyleMoveRecorder::GetScore(const BaseSkeleton *liveSkel, int playerIdx, float beatParam, bool useDancerTake) {
    float initScore = 0.0f;
    if (mLastFrameIndex == mCurrentTakeIndex && beatParam > 0.0f) {
        return 1.0f;
    }
    // Use mPlaybackPos if beatParam == -1.0f, else use beatParam
    // Evaluates beatParam != -1.0f BEFORE loading mPlaybackPos for correct FPR ordering
    bool useInputBeat = (beatParam != -1.0f);
    float beat = mPlaybackPos;
    if (useInputBeat) {
        beat = beatParam;
    }
    float beatMillis = beat * 1000.0f;
    UpdateRecordingAttempt(liveSkel, beatMillis);
    const FreestyleMoveFrame *frames;
    int numFrames;
    if (useDancerTake) {
        frames = mFrameBuffer;
        numFrames = mDancerTakeFrameCount;
    } else {
        frames = mTakes[mCurrentTakeIndex].mFrames;
        numFrames = mTakes[mCurrentTakeIndex].mNumFrames;
    }
    // Compute maxIdx = max(0, numFrames-1)
    unsigned int maxIdx = (unsigned int)(numFrames - 1);
    maxIdx = maxIdx & (unsigned int)(~((int)maxIdx >> 31));
    // Compute raw frame index: int(mDefaultTimeout * beat) - 2
    int frameIdxRaw = (int)(mDefaultTimeout * beat) - 2;
    // frameIdx defaults to maxIdx (used when frameIdxRaw > maxIdx)
    unsigned int frameIdx = maxIdx;
    if (frameIdxRaw <= (int)maxIdx) {
        // clamp negative to 0: srwi/subi/and pattern
        unsigned int r10 = (unsigned int)frameIdxRaw >> 31;
        r10 = r10 - 1;
        frameIdx = r10 & (unsigned int)frameIdxRaw;
    }
    // Copy reference frame skeleton into debug global
    sLastComparedDancerSkel.Set(frames[frameIdx].skeleton);
    // Compute pointer to this player's FreestyleFrameScores
    FreestyleFrameScores &frameScores = *(FreestyleFrameScores *)((char *)unke4 + (playerIdx << 4));
    if (liveSkel != nullptr && liveSkel->IsTracked()) {
        CalcFrameScore(frameScores, frames, numFrames, liveSkel, beatMillis - 100.0f);
    }
    // Accumulate scores: sum(scores[i] / numFrames) for i in 0..unkc
    int scoreCount = frameScores.unkc;
    float total = initScore;
    if (scoreCount > 0) {
        float invNumFrames = 1.0f / (float)(long long)(int)numFrames;
        float *scoresData = frameScores.unk0.begin();
        int byteIdx = 0;
        int j = 0;
        do {
            j++;
            total = *(float *)((char *)scoresData + byteIdx) * invNumFrames + total;
            byteIdx += 4;
        } while (j < scoreCount);
    }
    return total;
}

float FreestyleMoveRecorder::GetScore(int i1, int i2, float f, bool b) {
    // Default: use skeleton from gesture manager if player index is valid
    BaseSkeleton *skeletonToScore = nullptr;
    if (i1 >= 0) {
        skeletonToScore = &TheGestureMgr->GetSkeleton(i1);
    }

    // Check if there's a live skeleton that should override the default
    BaseSkeleton *liveSkeleton = GetLiveSkeleton();
    if (liveSkeleton) {
        // Get the reference skeleton (if mSkeletonIndex is set)
        BaseSkeleton *referenceSkeleton;
        if (mSkeletonIndex >= 0) {
            referenceSkeleton = &TheGestureMgr->GetSkeleton(mSkeletonIndex);
        } else {
            referenceSkeleton = nullptr;
        }

        // Use live skeleton only if it differs from reference
        if (liveSkeleton != referenceSkeleton) {
            skeletonToScore = liveSkeleton;
        }
    }

    return GetScore(skeletonToScore, i2, f, b);
}
