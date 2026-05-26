#include "RhythmDetector.h"
#include "gesture/BaseSkeleton.h"
#include "gesture/GestureMgr.h"
#include "gesture/Skeleton.h"
#include "math/Vec.h"
#include "ui/UIPanel.h"
#include "obj/DataFunc.h"
#include "obj/Dir.h"
#include "gesture/SkeletonUpdate.h"
#include <cstring>

namespace {
    int kAnalyzeJoints[] = { 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
                             10, 11, 12, 13, 14, 15, 16, 17, 18, 19 };
    int gDebugBone = -1;
    float gAdjust = 1;
    int gLog = -1;
    bool gClamp = true;
    unsigned char gInitCheatDone = 0;
    const char *kConv[] = { "------00++++++00----",
                            "---0+++0-----0+++0--",
                            "-++0--++0--++0--++0-",
                            "-0++--0++--0++--0++-" };
    int kConvCount = 4;
    int kConvLen = strlen(kConv[0]);

    float Mean(const std::vector<float> &, int, int);
    float Variance(const std::vector<float> &, float, int, int);
    const std::vector<float> &jointWeight();

    __declspec(noinline) void AnalyzeData(
        const std::vector<RhythmDetector::Frame> &frames,
        float &outScore,
        float &outEnergy,
        float &decay,
        float toleranceFactor,
        bool b1,
        Symbol sym,
        bool b2,
        DebugGraph *dbg,
        int logIdx,
        TextStream *stream
    ) {
        unsigned int numFrames = frames.size();
        float totalWeightedScore = 0.0f;
        float timeScale = (float)numFrames * 0.025f;

        if (numFrames >= 10) {
            float totalRaw = 0.0f;

            for (int jointIdx = 0; jointIdx < 20; jointIdx++) {
                const std::vector<float> &weights = jointWeight();
                float w = weights[jointIdx];

                if (w != 0.0f) {
                    if (stream) {
                        *stream << "<br>";
                    }

                    float rawFill = 0.0f;
                    float normFill = 0.0f;

                    for (int comp = 0; comp < 3; comp++) {
                        float rawAbsSum = 0.0f;
                        float bestConv = 0.0f;

                        static std::vector<float> raw;
                        static std::vector<float> normalized;

                        // Resize raw to min(numFrames, 40)
                        unsigned int count = frames.size();
                        if (count >= 40) count = 40;

                        unsigned int rawSize = raw.size();
                        if (count < rawSize) {
                            raw.erase(raw.begin() + count, raw.end());
                        } else {
                            raw.insert(raw.end(), count - rawSize, rawFill);
                        }

                        // Resize normalized to min(numFrames, 40)
                        count = frames.size();
                        if (count >= 40) count = 40;

                        unsigned int normSize = normalized.size();
                        if (count < normSize) {
                            normalized.erase(normalized.begin() + count, normalized.end());
                        } else {
                            normalized.insert(normalized.end(), count - normSize, normFill);
                        }

                        // Fill raw with absolute joint velocities
                        if (raw.size() != 0) {
                            for (unsigned int f = 0; f < raw.size(); f++) {
                                float val = frames[f].mJointVelocities[jointIdx][comp];
                                float absVal = fabs(val);
                                raw[f] = absVal;
                                rawAbsSum += absVal;
                            }
                        }

                        // Z-score first 6 entries using window [0, 10]
                        float mean = Mean(raw, 0, 10);
                        float var = Variance(raw, mean, 0, 10);
                        for (int i = 0; i < 6; i++) {
                            normalized[i] = (raw[i] - mean) * (1.0f / var);
                        }

                        // Compute midpoint
                        int rawSz = raw.size();
                        int midEnd = rawSz - 6;
                        if ((unsigned)(rawSz - 6) <= 6) midEnd = 6;

                        // Z-score middle section with sliding window
                        if (midEnd > 6) {
                            int windowStart = 1;
                            int rawOffset = 6;
                            int remaining = midEnd - 6;
                            do {
                                mean = Mean(raw, windowStart, windowStart + 10);
                                float diff = raw[rawOffset] - mean;
                                var = Variance(raw, mean, windowStart, windowStart + 10);
                                remaining--;
                                windowStart++;
                                normalized[rawOffset] = diff / var;
                                rawOffset++;
                            } while (remaining != 0);
                        }

                        // Z-score tail section
                        int tailStart = midEnd - 5;
                        mean = Mean(raw, tailStart, midEnd + 5);
                        var = Variance(raw, mean, tailStart, midEnd + 5);

                        if ((unsigned)midEnd < raw.size() - 1) {
                            int idx = midEnd;
                            do {
                                normalized[idx] = (raw[idx] - mean) * (1.0f / var);
                                idx++;
                            } while ((unsigned)idx < raw.size() - 1);
                        }

                        // Sum of absolute normalized values
                        float absNormSum = 0.0f;
                        unsigned int normCount = normalized.size();
                        if (normCount != 0) {
                            unsigned int i = 0;
                            float *normIter = &normalized[0] - 1;
                            do {
                                float nv = *++normIter;
                                absNormSum += fabs(nv);
                                i++;
                            } while (i < normCount);
                        }

                        // Convolution with kConv patterns
                        if (kConvCount > 0) {
                            int c = kConvCount;
                            const char **convPtr = kConv;
                            do {
                                const char *conv = *convPtr;
                                for (int offset = 0; offset < kConvLen; offset++) {
                                    float convSum = 0.0f;
                                    if (normCount != 0) {
                                        float *normPtr = &normalized[0];
                                        unsigned int i = 0;
                                        do {
                                            float val = *normPtr;
                                            int idx = (int)(i + offset) % kConvLen;
                                            if (conv[idx] == '-') {
                                                val = val * -1.0f;
                                            } else if (conv[idx] == '0') {
                                                val = fabs(val);
                                            }
                                            convSum += val;
                                            normPtr++;
                                            i++;
                                        } while (i < normCount);
                                    }
                                    if (convSum > bestConv) {
                                        bestConv = convSum;
                                    }
                                }
                                convPtr++;
                                c--;
                            } while (c != 0);
                        }

                        // Compute rhythm ratio and score
                        float rhythmRatio = bestConv / absNormSum;
                        float score = rhythmRatio * rawAbsSum;

                        // Debug output
                        if (stream) {
                            *stream << " ";
                            *stream << MakeString("%0.2f", rhythmRatio);
                            *stream << "&middot;";
                            int iScore = (int)(rawAbsSum * w);
                            *stream << iScore;
                            if (iScore < 10) {
                                *stream << "&nbsp;&nbsp;";
                            } else if (iScore < 100) {
                                *stream << "&nbsp;";
                            }
                        }

                        totalRaw += rawAbsSum * w;
                        totalWeightedScore += score * w;
                    }
                }
            }

            // Clamp totalRaw
            float threshold = timeScale * 200.0f;
            float diff = totalRaw - threshold;
            float clampedRaw = diff >= 0.0f ? totalRaw : threshold;

            outScore = (totalWeightedScore / clampedRaw) * timeScale;

            outEnergy = totalRaw >= 200.0f ? totalRaw : 0.0f;

            // Debug summary
            if (stream) {
                *stream << "<br>";
                *stream << (int)totalWeightedScore;
                *stream << "/";
                *stream << (int)totalRaw;
                *stream << "=";
                *stream << outScore;
            }
        }
    }

    DataNode TightenDebugBone(DataArray *da) {
        gAdjust *= 1.01f;
        MILO_LOG("scalar %f\n", gAdjust);
        return 0;
    }

    DataNode LoosenDebugBone(DataArray *da) {
        gAdjust *= 0.990099f;
        MILO_LOG("scalar %f\n", gAdjust);
        return 0;
    }

    DataNode DataSpaceCheat(DataArray *da) {
        gLog += 1;
        if (60 <= gLog) {
            gLog = -1;
        }
        return 0;
    }

    DataNode CycleDebugBone(DataArray *da) {
        gDebugBone += 1;
        gAdjust = 1.0;

        if (gDebugBone == 20) {
            gDebugBone = -1;
        }
        MILO_LOG("debug bone %d\n", gDebugBone);
        return 0;
    }

    void initCheat() {
        if (!gInitCheatDone) {
            gInitCheatDone = 1;
            DataRegisterFunc(Symbol("cycle_movement_bone"), CycleDebugBone);
            DataRegisterFunc(Symbol("tighten_current_bone"), TightenDebugBone);
            DataRegisterFunc(Symbol("loosen_current_bone"), LoosenDebugBone);
            DataRegisterFunc(Symbol("ktb_debug_cheat"), DataSpaceCheat);
        }
    }

    float Mean(const std::vector<float> &vec, int start, int end) {
        // some fruity branchless stuff going on in here
        int size = (vec.size());
        end = size < end ? size : end;
        start = start < 0 ? 0 : start;
        float sum = 0.0f;
        for (int i = start; i < end; i++) {
            sum += vec[i];
        }
        int count = end - start;
        if (count == 0) {
            return 0.0f;
        }
        return sum / count;
    }

    float Variance(const std::vector<float> &vec, float mean, int start, int end) {
        int size = (vec.size());
        end = size < end ? size : end;
        start = start > 0 ? start : 0;
        float sum = 0.0f;
        for (int i = start; i < end; i++) {
            sum += (vec[i] - mean) * (vec[i] - mean);
        }
        int count = end - start;
        if (count != 0) {
            return sum / count;
        }
        return 0.0f;
    }

    const std::vector<float> &jointWeight() {
        static std::vector<float> data;
        static UIPanel *panel =
            ObjectDir::Main()->Find<UIPanel>("rhythm_detector_panel", false);
        DataArray *typeDef = panel->TypeDef();
        if (data.empty()) {
            static Symbol jointWeightSym("joint_weight");
            DataArray *minJoints = typeDef->FindArray(jointWeightSym, true);
            MILO_ASSERT(minJoints->Size() == kNumJoints + 1, 0x4b4);
            for (int i = 1; i < minJoints->Size(); i++) {
                float val = minJoints->Node(i).Float(minJoints);
                data.push_back(val);
            }
            MILO_ASSERT(data.size() == kNumJoints, 0x4bc);
        }
        return data;
    }

    const std::vector<float> &minJointSpeedVector() {
        static std::vector<float> data;
        static UIPanel *panel =
            ObjectDir::Main()->Find<UIPanel>("rhythm_detector_panel", false);
        DataArray *typeDef = panel->TypeDef();
        if (data.empty()) {
            static Symbol minJointSpeedSym("min_joint_speed");
            DataArray *minJoints = typeDef->FindArray(minJointSpeedSym, true);
            MILO_ASSERT(minJoints->Size() == kNumJoints + 1, 0x4c3);
            for (int i = 1; i < minJoints->Size(); i++) {
                float val = minJoints->Node(i).Float(minJoints);
                data.push_back(val);
            }
            MILO_ASSERT(data.size() == kNumJoints, 0x4cb);
        }
        return data;
    }

}

void SetupFrame(
    RhythmDetector::Frame &frame,
    float frameCount,
    float beatDiff,
    const Vector3 *prevJoints,
    const Vector3 *curJoints,
    float deltaTime
) {
    MILO_ASSERT(frameCount >= 0, 0x4d7);
    MILO_ASSERT(beatDiff >= 0, 0x4d8);
    MILO_ASSERT(prevJoints, 0x4d9);
    MILO_ASSERT(curJoints, 0x4da);

    static UIPanel *panel =
        ObjectDir::Main()->Find<UIPanel>("rhythm_detector_panel", false);
    minJointSpeedVector();

    frame.mJointVelocities.resize(20);
    float invDelta = 1.0f / deltaTime;
    for (int i = 0; i < 20; i++) {
        int joint = kAnalyzeJoints[i];
        Vector3 vel;
        Subtract(curJoints[joint], prevJoints[joint], vel);
        Scale(vel, invDelta, vel);
        frame.mJointVelocities[i] = vel;
    }
    frame.mTime = frameCount + beatDiff;
}

RhythmDetector::Frame BlendFrameDataToBeat(
    const RhythmDetector::Frame &a,
    const RhythmDetector::Frame &b,
    float beatTime
) {
    if (beatTime < a.mTime || b.mTime < beatTime) {
        MILO_NOTIFY(
            "bad rhythm detector floating point precision at %f %f %f\n",
            a.mTime, b.mTime, beatTime
        );
    }

    int kSize = a.mJointVelocities.size();
    MILO_ASSERT(kSize == b.mJointVelocities.size(), 0x5a9);

    float timeA = a.mTime;
    float timeB = b.mTime;
    float blend = (beatTime - timeA) / (timeB - timeA);
    blend = -blend >= 0.0f ? 0.0f : blend;
    blend = blend - 1.0f >= 0.0f ? 1.0f : blend;

    RhythmDetector::Frame result;
    result.mTime = beatTime;
    result.mJointVelocities.resize(kSize);

    for (int i = 0; i < kSize; i++) {
        for (int j = 0; j < 3; j++) {
            float valB = b.mJointVelocities[i][j];
            float valA = a.mJointVelocities[i][j];
            result.mJointVelocities[i][j] = (valB - valA) * blend + valA;
        }
    }
    return result;
}

void EraseNewerData(std::vector<RhythmDetector::Frame> &vec, float time) {
    std::vector<RhythmDetector::Frame>::iterator found = vec.end();
    FOREACH (it, vec) {
        if (it->mTime >= time) {
            found = it;
            break;
        }
    }
    if (found != vec.end()) {
        vec.erase(found, vec.end());
    }
}

void CameraToScreenUnit(Vector3 &vec, const Skeleton &skeleton, SkeletonJoint joint) {
    Vector2 skelPos;
    skeleton.ScreenPos(joint, skelPos);
    float y = -skeleton.TrackedJoints()[joint].mSmoothedPos.z;
    vec.Set((skelPos.x - 0.5f) * 2.0f, y * 0.22977939f, (0.5f - skelPos.y) * 2.0f);
}

RhythmDetector::RhythmDetector()
    : mTracked(false), mRecording(0), mSkeletonID(-1), mBeats(8), mGroove(0),
      mRhythmDecay(0), mFold(2), mToleranceFactor(1.5f), mDirection(0, 0, 0), mFrameCount(0),
      mDebugGraphA(0), mDebugGraphB(0), mDebugGraphC(0), mDebugGraphD(0), mDebugGraphE(0),
      mDivergenceCounter(0), mBufferIndex(0) {
    mCurrentFrame.mJointVelocities.clear();
    for (int i = 0; i < 8; i++) {
        mTimestamps[i] = -1;
    }
    initCheat();
}

RhythmDetector::~RhythmDetector() {
    delete mDebugGraphA;
    delete mDebugGraphB;
    if (SkeletonUpdate::InstanceHandle().HasCallback(this)) {
        SkeletonUpdate::InstanceHandle().RemoveCallback(this);
    }
}

BEGIN_HANDLERS(RhythmDetector)
    HANDLE_ACTION(start_recording, StartRecording())
    HANDLE_ACTION(stop_recording, StopRecording())
    HANDLE_EXPR(is_recording, IsRecording())
    HANDLE_SUPERCLASS(RndPollable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(RhythmDetector)
    SYNC_SUPERCLASS(Hmx::Object)
    SYNC_SUPERCLASS(RndPollable)
    SYNC_PROP(rhythm_rating, mGroove);
    SYNC_PROP(rhythm_decay, mRhythmDecay)
    SYNC_PROP(num_beats_to_cover, mBeats)
    SYNC_PROP(beat_fold, mFold)
    SYNC_PROP(tolerance_factor, mToleranceFactor)
    SYNC_PROP(dir_x, mDirection.x)
    SYNC_PROP(dir_y, mDirection.y)
    SYNC_PROP(dir_z, mDirection.z)
END_PROPSYNCS

BEGIN_SAVES(RhythmDetector)
    SAVE_REVS(2, 0)
    SAVE_SUPERCLASS(RndPollable)
    bs << mBeats;
    bs << mFold;
    bs << mToleranceFactor;
    bs << mDirection.x;
    bs << mDirection.y;
    bs << mDirection.z;
END_SAVES

BEGIN_COPYS(RhythmDetector)
    COPY_SUPERCLASS(RndPollable)
    CREATE_COPY(RhythmDetector)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mBeats)
        COPY_MEMBER(mFold)
        COPY_MEMBER(mToleranceFactor)
        COPY_MEMBER(mDirection)
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(2, 0)

BEGIN_LOADS(RhythmDetector)
    LOAD_REVS(bs)
    if (d.rev > 2) {
        TheDebug.Fail(
            MakeString(
                "%s can't load new %s version %d > %d",
                PathName(this),
                ClassName(),
                d.rev,
                *(&gAltRev - 2)
            ),
            0
        );
    }
    if (d.altRev > 0) {
        TheDebug.Fail(
            MakeString(
                "%s can't load new %s alt version %d > %d",
                PathName(this),
                ClassName(),
                d.altRev,
                gAltRev
            ),
            0
        );
    }
    LOAD_SUPERCLASS(RndPollable)
    if (d.rev >= 1) {
        d >> mBeats;
    }
    if (d.rev >= 2) {
        d >> mFold;
        d >> mToleranceFactor;
        d >> mDirection.x;
        d >> mDirection.y;
        d >> mDirection.z;
        Normalize(mDirection, mDirection);
    }
END_LOADS

void RhythmDetector::Poll() {
    if (mRecording) {
        if (TheGestureMgr->GetSkeleton(mSkeletonID).IsTracked()) {
            if (mDebugGraphA)
                mDebugGraphA->Draw();
            if (mDebugGraphB)
                mDebugGraphB->Draw();
            if (mDebugGraphC)
                mDebugGraphC->Draw();
            if (mDebugGraphD)
                mDebugGraphD->Draw();
            if (mDebugGraphE)
                mDebugGraphE->Draw();
        }
    }
}

void RhythmDetector::Enter() {
    RndPollable::Enter();
    if (SkeletonUpdate::HasInstance()
        && !SkeletonUpdate::InstanceHandle().HasCallback(this)) {
        SkeletonUpdate::InstanceHandle().AddCallback(this);
    }
}

void RhythmDetector::PostUpdate(const SkeletonUpdateData *data) {
    if (mSkeletonID >= 0) {
        mTracked = TheGestureMgr->GetSkeleton(mSkeletonID).IsTracked();
    } else {
        mTracked = false;
    }
    if (mRecording == 0 || !mTracked) {
        mCurrentFrame.mJointVelocities.clear();
        mAnalysisFrames2.clear();
        mAnalysisFrames1.clear();
        mRecordData.frames.clear();
    } else if (data) {
        Skeleton &skeleton = TheGestureMgr->GetSkeleton(mSkeletonID);
        if (skeleton.ElapsedMs() != data->mFrame->mElapsedMs) {
            MILO_WARN("current skeleton doesn't match update data");
        }
        AddFrame(skeleton);
        for (int i = 0; i < kNumJoints; i++) {
            CameraToScreenUnit(unkaac[i], skeleton, (SkeletonJoint)i);
        }
        ProcessFrames();
    }
}

float RhythmDetector::Groove() const {
    if (mTracked)
        return mGroove;
    else
        return 0;
}

float RhythmDetector::Freshness() const {
    if (mTracked)
        return 1 - mRhythmDecay;
    else
        return 0;
}
Vector4 RhythmDetector::Data1(int idx) const {
    const Vector3 &v = unkaac[idx];
    return Vector4(v.x, v.y, v.z, 0);
}

Vector4 RhythmDetector::Data2(int) const { return Vector4(0, 1, 1, 1); }

void RhythmDetector::RemoveDebugGraphs() {
    RELEASE(mDebugGraphA);
    RELEASE(mDebugGraphB);
    RELEASE(mDebugGraphC);
    RELEASE(mDebugGraphD);
    RELEASE(mDebugGraphE);
}

void RhythmDetector::AddDebugGraph(
    float f1, float f2, float f3, float f4, Hmx::Color color
) {
    delete mDebugGraphE;
    mDebugGraphE = new DebugGraph(
        f1,
        f2,
        f3,
        f4,
        color,
        Hmx::Color(0.4f, 0.4f, 0.4f, 0.8f),
        120,
        0,
        2,
        MakeString(
            "beats %d  fold %d  dir %.1f %.1f %.1f",
            (int)mBeats,
            mFold,
            mDirection.x,
            mDirection.y,
            mDirection.z
        )
    );
    mDebugGraphE->SetThresholdValue(1);
}

void RhythmDetector::AddFullDebugGraphs() {
    if (gLog != -1) {
        Hmx::Color red(1, 0, 0, 1);
        delete mDebugGraphA;
        mDebugGraphA = new DebugGraph(
            0.1f, 0.0f, 0.8f, 0.06f, red, Hmx::Color(0, 0, 0, 0), 120, -1.1f, 1.1f, ""
        );
        mDebugGraphA->SetIsVisible(false);
    }
}

void RhythmDetector::StartRecording() {
    if (++mRecording == 1) {
        AddFullDebugGraphs();
        mLastBeatTime = TheTaskMgr.Beat();
        ClearData();
    }
    MILO_ASSERT(mRecording >= 1, 0x3cc);
    MILO_ASSERT(mRecording <= 2, 0x3cd);
}

void RhythmDetector::StopRecording() {
    if (--mRecording == 0) {
        mLastBeatTime = TheTaskMgr.Beat();
        ClearData();
    }
    MILO_ASSERT(mRecording >= 0, 0x3da);
    MILO_ASSERT(mRecording <= 1, 0x3db);
}

void RhythmDetector::ClearData() {
    mFrameCount = 0;
    mAnalysisFrames1.clear();
    mAnalysisFrames2.clear();
    mRecordData.frames.clear();
    mGroove = 0;
    mRhythmDecay = 0;
    mRecordData.mFinalized = true;
    mRecordData.unk8 = -1;
    mRecordData.unkc = -1;
    mRecordData.mWindowStart = -1;
    mRecordData.mWindowEnd = -1;
    mRecordData.unk10 = -1;
    mRecordData.unk14 = -1;
    mFrameHistory.clear();
    AddFullDebugGraphs();
}

void RhythmDetector::AddFrame(BaseSkeleton const &skel) {
    static UIPanel *panel = ObjectDir::Main()->Find<UIPanel>("rhythm_detector_panel", false);
    if (!panel)
        return;

    PaddedJointPos localJoints[kNumJoints];
    for (int j = 0; j < kNumJoints; j++) {
        skel.JointPos(kCoordCamera, (SkeletonJoint)j, localJoints[j]);
    }

    float beat = TheTaskMgr.Beat();
    float seconds = TheTaskMgr.Seconds(TaskMgr::kRealTime);
    float beatDiff = beat - mLastBeatTime;
    if (beatDiff < 0.0f) {
        beatDiff = 0.0f;
        ClearData();
    }

    int bestIdx = -1;
    int idx = 0;
    float bestDist = 0.0f;
    for (int i = 0; i < 8; i++) {
        if (mTimestamps[i] >= 0.0f) {
            float dist = fabs((seconds - mTimestamps[i]) - 0.1f);
            if (dist < bestDist || bestIdx == -1) {
                bestIdx = idx;
                bestDist = dist;
            }
        }
        idx++;
    }

    if (bestIdx != -1) {
        Frame newFrame;
        mFrameHistory.insert(mFrameHistory.end(), newFrame);

        // Trim history to 3 entries
        std::list<Frame>::iterator it = mFrameHistory.begin();
        unsigned int count = 0;
        while (it != mFrameHistory.end()) {
            it++;
            count++;
        }
        if (count > 3) {
            mFrameHistory.erase(mFrameHistory.begin());
        }

        SetupFrame(
            mFrameHistory.back(),
            mFrameCount,
            beatDiff,
            (Vector3 *)mJointBuffer[bestIdx],
            (Vector3 *)localJoints,
            seconds - mTimestamps[bestIdx]
        );

        mLastBeatTime = beat;

        // Copy local joints into circular buffer
        for (int k = 0; k < kNumJoints; k++) {
            mJointBuffer[mBufferIndex][k] = localJoints[k];
        }

        mTimestamps[mBufferIndex] = seconds;
        mFrameCount += beatDiff;
        mBufferIndex = (mBufferIndex + 1) % 8;
    }
}

// const RhythmDetector::RecordData &
// RhythmDetector::GetRecord(float f1, float f2, bool b, Symbol sym, TextStream *stream) {
//     RecordData ret = mRecordData;
//     if (mRecordData.unkbec == f1 && mRecordData.unkbf8 == f2) {
//         if (stream) {
//             AnalyzeData(
//                 mAnalysisFrames2,
//                 mRecordData.unkbfc,
//                 mRecordData.mTracked00,
//                 mRhythmDecay,
//                 mToleranceFactor,
//                 mDebugGraphA->GetmFold(),
//                 0,
//                 b,
//                 0,
//                 mDebugGraphA->GetMaxSamples(),
//                 stream
//             );
//             mTracked04 = true;
//         }
//     } else {
//         if (mTracked04 == false) {
//             MILO_NOTIFY(
//                 "new rhythm detector window w/o finalization [%.1f,%.1f] to [%.1f,
//                 %.1f]", mRecordData.unkbec, mRecordData.unkbf0, f1, f2
//             );
//         }
//         ClearData();
//         mRecordData.unkbf0 = f2;
//         mTracked04 = false;
//         mRecordData.unkbf4 = -1.0;
//         mRecordData.unkbf8 = -1.0;
//         mTracked08.clear();
//     }
//     return ret;
// }

const RhythmDetector::RecordData &
RhythmDetector::GetRecord(float windowStart, float windowEnd, bool finalize, Symbol sym, TextStream *stream) {
    if (mRecordData.mWindowStart == windowStart && mRecordData.mWindowEnd == windowEnd) {
        if (finalize) {
            AnalyzeData(
                mAnalysisFrames2,
                mRecordData.unk10,
                mRecordData.unk14,
                mRhythmDecay,
                mToleranceFactor,
                true,
                sym,
                false,
                mDebugGraphA,
                gLog,
                stream
            );
            mRecordData.mFinalized = true;
        }
    } else {
        if (!mRecordData.mFinalized) {
            MILO_NOTIFY(
                "new rhythm detector window w/o finalization [%.1f,%.1f] to [%.1f, %.1f]",
                mRecordData.mWindowStart,
                mRecordData.mWindowEnd,
                windowStart,
                windowEnd
            );
        }
        ClearData();
        mRecordData.mWindowStart = windowStart;
        mRecordData.mWindowEnd = windowEnd;
        mRecordData.mFinalized = false;
        mRecordData.unkc = -1.0f;
        mRecordData.unk8 = -1.0f;
        mRecordData.frames.clear();
    }
    return mRecordData;
}

void RhythmDetector::ProcessFrames() {
    std::list<Frame> localHistory;
    localHistory.swap(mFrameHistory);

    bool hadBlendedFrames = false;
    if (!localHistory.empty()) {
        float lastTime = localHistory.back().mTime;
        EraseNewerData(mRecordData.frames, lastTime);
        EraseNewerData(mAnalysisFrames1, lastTime);

        for (std::list<Frame>::iterator it = localHistory.begin(); it != localHistory.end(); ++it) {
            mAnalysisFrames1.push_back(*it);

            int prevTick = (int)(mCurrentFrame.mTime * 10.0f);
            int curTick = (int)(it->mTime * 10.0f);
            int tickDiff = (curTick - prevTick) % 40;

            if (!mCurrentFrame.mJointVelocities.empty() && tickDiff > 0) {
                hadBlendedFrames = true;
                for (int j = 0; j < tickDiff; j++) {
                    float beatTime = (float)(prevTick + j + 1) * 0.1f;
                    Frame blended = BlendFrameDataToBeat(mCurrentFrame, *it, beatTime);
                    mAnalysisFrames2.push_back(blended);
                }
            }

            mCurrentFrame.mTime = it->mTime;
            mCurrentFrame.mJointVelocities = it->mJointVelocities;
        }

        // Trim local list to keep only the last entry
        unsigned int count = 0;
        for (std::list<Frame>::iterator it = localHistory.begin(); it != localHistory.end(); ++it) {
            count++;
        }
        if (count > 1) {
            localHistory.erase(localHistory.begin());
        }

        // Trim again (same logic - ensures only 1 entry)
        count = 0;
        for (std::list<Frame>::iterator it = localHistory.begin(); it != localHistory.end(); ++it) {
            count++;
        }
        if (count > 1) {
            localHistory.erase(localHistory.begin());
        }

        mCurrentFrame.mTime = localHistory.back().mTime;
        mCurrentFrame.mJointVelocities = localHistory.back().mJointVelocities;
    }

    if (!mAnalysisFrames1.empty()) {
        static UIPanel *panel = ObjectDir::Main()->Find<UIPanel>("rhythm_detector_panel", false);

        float windowSize;
        if (panel == nullptr) {
            windowSize = 0.0f;
        } else {
            DataArray *typeDef = panel->TypeDef();
            static Symbol analyzeBeatFrequency("analyze_beat_frequency");
            DataArray *cfg = typeDef->FindArray(analyzeBeatFrequency, true);
            static Symbol analyzePeriodCount("analyze_period_count");
            DataArray *periodCfg = typeDef->FindArray(analyzePeriodCount, true);
            int periodCount = periodCfg->Node(1).Int();
            cfg->Node(cfg->Size() - 1).Int();
            int beatFreq = cfg->Node(cfg->Size() - 1).Int();
            windowSize = (float)beatFreq * (float)(periodCount - 1) * 2.0f;
        }

        // Trim old frames outside the analysis window
        std::vector<Frame>::iterator trimEnd = mAnalysisFrames1.end();
        float lastFrameTime = (mAnalysisFrames1.end() - 1)->mTime;
        for (std::vector<Frame>::iterator it = mAnalysisFrames1.begin(); it != mAnalysisFrames1.end(); ++it) {
            if (it->mTime >= lastFrameTime - windowSize) {
                break;
            }
            trimEnd = it;
        }
        if (trimEnd != mAnalysisFrames1.end() && mAnalysisFrames1.begin() != trimEnd + 1) {
            mAnalysisFrames1.erase(mAnalysisFrames1.begin(), trimEnd + 1);
        }

        if (hadBlendedFrames) {
            static Symbol emptySym("");
            AnalyzeData(
                mAnalysisFrames2,
                mRecordData.unk10,
                mRecordData.unk14,
                mRhythmDecay,
                mToleranceFactor,
                false,
                emptySym,
                false,
                nullptr,
                gLog,
                nullptr
            );
        }
    }
}
