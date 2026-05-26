#pragma once
#include "FreestyleMove.h"
#include "gesture/BaseSkeleton.h"
#include "gesture/Skeleton.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "rndobj/Tex.h"
#include "utl/MemMgr.h"
#include "utl/Str.h"
#include "utl/Symbol.h"

#define MAX_FREESTYLE_MOVES 4

// size 0x110
class FreestyleMoveRecorder : public SkeletonCallback {
    friend class BustAMovePanel;
public:
    struct JointAngle {
        SkeletonJoint mJoint;
    };
    struct JointPos {
        int mJoint; // SkeletonJoint?
        int unk4;
    };
    FreestyleMoveRecorder();
    virtual ~FreestyleMoveRecorder();
    virtual void Clear() {}
    virtual void Update(const struct SkeletonUpdateData &) {}
    virtual void PostUpdate(const struct SkeletonUpdateData *) {}
    virtual void Draw(const BaseSkeleton &, class SkeletonViz &) {}

    void Poll();
    void Free();
    void StartRecording();
    void StartRecordingDancerTake();
    void StopRecording();
    void ClearRecording();
    void StartPlayback(bool);
    void StopPlayback();
    void ClearDancerTake();
    BaseSkeleton *GetLiveSkeleton();
    float
    CompareSkeletonPositions(const BaseSkeleton *, const BaseSkeleton *, float) const;
    void AssignStaticInstance();
    void DrawDebug();
    void PlaybackComplete();
    void ClearFrameScores();
    void ReadFreestyleMoveClip(String, int &, FreestyleMoveFrame *);
    float GetScore(const BaseSkeleton *, int, float, bool);
    float GetScore(int, int, float, bool);
    void CalcFrameScore(
        FreestyleFrameScores &, const FreestyleMoveFrame *, int, const BaseSkeleton *, float
    ) const;
    RndTex *GetPlayerPalette() const { return mPlayerPalette; }
    int GetDancerTakeFrameCount() const { return mDancerTakeFrameCount; }
    int GetCurrentMoveNumFrames() const { return mTakes[mCurrentTakeIndex].mNumFrames; }

    void SetVal44(int i) { mSkeletonIndex = i; } // change once context found

    void SetFreestyleMove(int index) {
        MILO_ASSERT(index >= 0 && index < MAX_FREESTYLE_MOVES, 0x50);
        mCurrentTakeIndex = index;
    }

    MEM_OVERLOAD(FreestyleMoveRecorder, 0x2E);
    static FreestyleMoveRecorder *sInstance;

private:
    void UpdateRecordingAttempt(const BaseSkeleton *, float);
    void UpdateFakeSkeleton();
    void RecordMoveAttempt(String);
    void WriteRecordedMoveAttempt();
    void CompareDisplacementVectors(
        const Vector3 &, int, const Vector3 &, int, float &, float &
    ) const;
    float CompareSkeletonJointDisplacement(
        const FreestyleMoveFrame *, int, const BaseSkeleton *, float &
    ) const;
    void WriteFreestyleMoveClip(String, int, FreestyleMoveFrame *);
    void ClearFreestyleMoveClip();

    static DataNode OnRecordAttempt(DataArray *);
    static DataNode OnWriteCreated(DataArray *);
    static DataNode OnReadCreated(DataArray *);
    static DataNode OnReadAttempt(DataArray *);
    static DataNode OnClearAttempt(DataArray *);

    float mPlaybackSpeed; // 0x4
    FreestyleMoveFrame *mClipFrames; // 0x8 - frames
    int mClipFrameCount; // 0xc - frame count for mClipFrames
    String mClipName;
    FreestyleMoveFrame *mRecordingFrames;
    int mRecordingFrameCount;
    int mLastFrameIndex; // 0x20 - initialized to -1
    int mMaxFrames;
    float mRecordPos;
    float mPlaybackPos;
    float mDefaultTimeout; // 0x30 - initialized to 15
    int mPlaybackIndex; // 0x34 - initialized to -1, playback position
    bool mRecording;
    bool mPlaybackActive; // 0x39
    Symbol mRecordingTarget; // 0x3c
    int unk40;
    int mSkeletonIndex;
    FreestyleMove mTakes[4]; // 0x48
    int mCurrentTakeIndex;
    RndTex *mPlayerPalette;
    FreestyleMoveFrame *mFrameBuffer;
    int mDancerTakeFrameCount;
    int mFrameIndex;
    std::vector<JointAngle> mAngleLimits;
    std::vector<SkeletonJoint> mTrackedJoints;
    FreestyleFrameScores unke4[2];
    std::vector<JointPos> mPositions;
};
