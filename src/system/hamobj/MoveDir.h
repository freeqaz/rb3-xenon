#pragma once
#include "DetectFrame.h"
#include "FilterQueue.h"
#include "gesture/Skeleton.h"
#include "gesture/SkeletonClip.h"
#include "gesture/SkeletonDir.h"
#include "gesture/SkeletonViz.h"
#include "hamobj/CharFeedback.h"
#include "hamobj/DancerSequence.h"
#include "hamobj/DancerSkeleton.h"
#include "hamobj/DetectFrame.h"
#include "hamobj/Difficulty.h"
#include "hamobj/FilterVersion.h"
#include "hamobj/HamMove.h"
#include "hamobj/HamPhraseMeter.h"
#include "hamobj/MoveDetector.h"
#include "hamobj/PracticeSection.h"
#include "math/DoubleExponentialSmoother.h"
#include "obj/Data.h"
#include "obj/DirLoader.h"
#include "obj/Object.h"
#include "rndobj/Draw.h"
#include "rndobj/Overlay.h"
#include "ui/UILabelDir.h"
#include "utl/MemMgr.h"
#include "utl/Str.h"
#include <set>

/** "Dir for HamMoves, contains debugging functionality" */
class MoveDir : public SkeletonDir, public RndOverlay::Callback {
public:
    // size 0x3c
    class MovePlayerData {
    public:
        MovePlayerData() : mCurMove(nullptr) {}
        void Reset() {
            mCurMove = nullptr;
            mPhraseMeter = nullptr;
            mFeedback = nullptr;
            mTextFeedback = nullptr;
            mFeedbackMode = 0;
        }
        ObjPtr<HamMove> mCurMove; // 0x0
        std::vector<DetectFrame> mDetectFrames; // 0xc
        std::vector<HamMoveKey> mMoveKeys; // 0x18
        int mFeedbackMode; // 0x24
        HamPhraseMeter *mPhraseMeter; // 0x28
        CharFeedback *mFeedback; // 0x2c
        RndDrawable *mTextFeedback; // 0x30
    };
    // Hmx::Object
    virtual ~MoveDir();
    OBJ_CLASSNAME(MoveDir)
    OBJ_SET_TYPE(MoveDir)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, CopyType);
    virtual void Load(BinStream &);
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    // RndDrawable
    virtual void DrawShowing();
    // RndPollable
    virtual void Poll();
    virtual void Enter();
    virtual void Exit();
    // SkeletonCallback
    virtual void Update(const struct SkeletonUpdateData &);
    virtual void PostUpdate(const struct SkeletonUpdateData *);
    virtual void Draw(const BaseSkeleton &, class SkeletonViz &);
    // RndOverlay::Callback
    virtual float UpdateOverlay(RndOverlay *, float f2);

    OBJ_MEM_OVERLOAD(0x2F)
    NEW_OBJ(MoveDir)

    void ClearLimbFeedback(int);
    void SetFiltersEnabled(bool);
    void SetMoveOverlay(bool);
    void SetSongPlayClip(SkeletonClip *);
    HamMove *CurrentMove(int) const;
    int MoveIdx() const;
    int MoveBeat() const;
    void StopSongRecord();
    void FlushMoveRecord();
    void SwapMoveRecord();
    HamMove *GetMoveAtMeasure(int, int);
    DancerSequence *PerformanceSequence(Difficulty);
    void FinishGameRecord();
    void SetupSongRecordClip();
    void SetDancerSequence(DancerSequence *);
    void ResetDetection();
    void ResetDetectFrames(int, Difficulty);
    void SetCurrentMove(int, HamMove *);
    float
    DetectFrac(int, const HamMove *, const std::pair<DetectFrame *, DetectFrame *> &);
    void
    EnqueueDetectFrames(float, int, std::vector<DetectFrame> &, const FilterVersion *);
    void SimulateSong(int, int) {}
    void OnBeat();
    void FinalPoseStateMachine();
    void SetDebugLoop(bool);
    PracticeSection *GetPracticeSection(Difficulty);
    DancerSequence *SkillsSequence(Difficulty, Symbol, Symbol);
    float DetectFrac(int, int);
    bool InGracePeriod(int);

    MoveAsyncDetector *GetAsyncDetector() const { return mAsyncDetector; }

    static void Init();
    static void LoadScoring(const DataArray *);
    static const FilterVersion *FindFilterVersion(FilterVersionType);
    static float SongSeconds();
    static bool sGameRecord;
    static bool sGameRecord2Player;

private:
    void SetFilterVersion(Symbol);
    SkeletonClip *ImportClip(bool);
    void ReloadScoring();
    void DetectRange(
        std::vector<DetectFrame> &, std::pair<DetectFrame *, DetectFrame *> &, int, int
    );
    void PostUpdateFilters();
    float SongSpeed() const;
    MoveFrame *ClosestMoveFrame();

    DataNode OnStreamJump(const DataArray *);
    float DetectRangePSNR(
        const std::pair<const DetectFrame *, const DetectFrame *> &, const FilterVersion *
    ) const;
    float DetectRangeFrac(
        const std::pair<DetectFrame *, DetectFrame *> &, const FilterVersion *
    ) const;

    static std::vector<FilterVersion *> sFilterVersions;
    static float sLatencySeconds;
    static float sPLFMinTimeError;

protected:
    MoveDir();

    virtual void MiloUpdate();

    FilterVersion *mFilterVer; // 0x250
    /** "Show debugging overlay for the current HamMove" */
    bool mShowMoveOverlay; // 0x254
    /** "Types of error nodes to show" */
    /** "Specific joints to display debug viz for" */
    /** A bitmask of ErrorNodeType enums */
    int mErrorNodeInfo; // 0x258
    /** "Clip to play back in sync with the song" */
    ObjPtr<SkeletonClip> mPlayClip; // 0x25c
    /** "Clip to use for song recording" */
    ObjPtr<SkeletonClip> mRecordClip; // 0x268
    ObjPtr<SkeletonClip> mAlternateRecordClip; // 0x274
    ObjPtr<SkeletonClip> mSkeletonRecordClip; // 0x280
    int unk2e4; // 0x28c
    /** "If set, report will be limited to this move" */
    ObjPtr<HamMove> mReportMove; // 0x290
    /** "The pre-recorded .clp file to import" */
    String mImportClipPath; // 0x29c
    bool mFiltersEnabled; // 0x2a8
    Hmx::Object *mGamePanel; // 0x2ac
    float unk30c; // 0x2b0
    float mDebugLoopMarker; // 0x2b4
    FilterQueue *mFilterQueue; // 0x2b8
    MovePlayerData mMovePlayerData[2]; // 0x2bc
    MoveAsyncDetector *mAsyncDetector; // 0x324
    DirLoader *mUpdateLoader; // 0x328
    std::list<ObjDirPtr<UILabelDir> > mUpdateFonts; // 0x32c
    /** Smoothed normalized results of the current move. */
    DoubleExponentialSmoother mCurMoveSmoothers[2]; // 0x334

    // current move stuffs vs last move stuffs?
    HamMove *filler[2]; // 0x35c
    HamMove *mCurMove[2]; // 0x364
    float mCurMoveNormalizedResult[2]; // 0x36c
    float mPrevMoveNormalizedResult[2]; // 0x374
    MoveRating mCurMoveRating[2]; // 0x37c
    int mPrevMoveRating[2]; // 0x384

    int mFinishingMoveMeasure; // 0x38c
    RndOverlay *mMoveOverlay; // 0x390
    ObjPtr<DancerSequence> mDancerSeq; // 0x394
    DancerSkeleton *unk414; // 0x3a0
    SkeletonViz *mSkeletonViz; // 0x3a4
    const DetectFrame *mShowErrorFrames; // 0x3a8
    /** "Offset debug skeleton by latency offset" */
    bool mDebugLatencyOffset; // 0x3ac
    Skeleton mDebugSkeleton; // 0x3b0
    bool mDebugLoop; // 0xe84
    float mLastPollMs; // 0xe88
    /** "Show collision debug" */
    bool mDebugCollision; // 0xe8c
    Transform unkf04[2]; // 0xe90
    int unkf84; // 0xf10
    std::set<DetectFrame *> unkf88; // 0xf14
};
