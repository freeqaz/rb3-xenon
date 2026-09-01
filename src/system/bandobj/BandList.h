#pragma once
#include "ui/UIList.h"
#include "rndobj/TransAnim.h"
#include "synth/Sequence.h"

class HighlightObject {
public:
    HighlightObject(Hmx::Object *);
    void Load(BinStream &);

    static int sRev;

    ObjPtr<RndTransformable> mTargetObj; // 0x0
    float mXOffset; // 0xc
    float mYOffset; // 0x10
    float mZOffset; // 0x14
};

class BandList : public UIList {
public:
    enum AnimState {
        kOut = 0,
        kGoingIn = 1,
        kIn = 2,
        kGoingOut = 3
    };

    enum RevealState {
        kConcealed = 0,
        kRevealing = 1,
        kConcealing = 2,
        kRevealed = 3
    };

    enum BandListState {
        kReveal = 0,
        kConceal = 1,
        kImmediateConceal = 2
    };

    BandList();
    OBJ_CLASSNAME(BandList)
    OBJ_SET_TYPE(BandList)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, CopyType);
    virtual void Load(BinStream &);
    virtual void DrawShowing();
    virtual ~BandList();
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    virtual void Enter();
    virtual void Exit();
    virtual void AdjustTrans(Transform &, const UIListElementDrawState &);
    virtual void AdjustTransSelected(Transform &);

    void StartPulseAnim(int);
    void StartFocusAnim(int, AnimState);
    void StartRevealAnim(int, Transform &);
    void RevealAnimPoll(int, Transform &);
    void UpdateFocusAndPulseAnims(int, Transform &);
    void UpdateRevealState(int, Transform &);
    void UpdateConcealState(int, Transform &);
    void UpdateRevealConcealState(int, Transform &);
    void UpdatePulseAnim(int, Transform &);
    void ForceConcealedStateOnAllEntries();
    void StartConcealAnim(int, Transform &);
    void ForceConcealed(int, Transform &);
    void ForceRevealed(int, Transform &);
    void ConcealAnimPoll(int, Transform &);
    void ConcealNow();
    void Conceal();
    void Reveal();
    void UpdateShowingState();
    bool RevealTimedOut();
    bool ConcealTimedOut();
    float GetRevealNumFrames() const;
    float GetRevealFramesPerSecond() const;
    float GetRevealStartFrame() const;
    float GetRevealEndFrame() const;
    float GetCurrentRevealFrame(float) const;
    float GetConcealNumFrames() const;
    float GetConcealFramesPerSecond() const;
    float GetConcealStartFrame() const;
    float GetConcealEndFrame() const;
    float GetCurrentConcealFrame(float) const;
    bool IsAnimating();
    bool IsRevealAnimFinished(float) const;
    bool IsConcealAnimFinished(float) const;
    void MakeRevealTransform(float, Transform &);
    void MakeConcealTransform(float, Transform &);
    bool SupportsRevealConcealAnim() const { return mRevealAnim || mConcealAnim; }
    bool IsRevealed() { return mBandListState == kReveal && !IsAnimating(); }

    static void Init();
    static void Register() { REGISTER_OBJ_FACTORY(BandList); }
    NEW_OBJ(BandList);
    // DECLARE_REVS removed: gRev/gAltRev are file-scope statics in BandList.cpp
    // so retail's single-base addressing reproduces. Re-adding it here would
    // silently make that inert (class scope beats namespace scope in PreLoad).
    NEW_OVERLOAD;
    DELETE_OVERLOAD;

    int mBandListRev; // 0x23c
    std::map<int, AnimState> mAnimStates; // 0x240
    std::map<int, float> mStartTimes; // 0x258
    std::map<int, float> mFrames; // 0x270
    std::map<int, RevealState> mRevealStates; // 0x288
    std::map<int, float> mRevealStartTimes; // 0x2a0
    std::map<int, bool> unk264; // 0x2b8
    ObjPtr<RndTransAnim> mFocusAnim; // 0x2d0
    ObjPtr<RndTransAnim> mPulseAnim; // 0x2dc
    ObjPtr<RndTransAnim> mRevealAnim; // 0x2e8
    ObjPtr<RndTransAnim> mConcealAnim; // 0x2f4
    ObjPtr<Sequence> mRevealSound; // 0x300
    ObjPtr<Sequence> mConcealSound; // 0x30c
    float mRevealSoundDelay; // 0x318
    float mConcealSoundDelay; // 0x31c
    float mRevealStartDelay; // 0x320
    float mRevealEntryDelay; // 0x324
    float mRevealScale; // 0x328
    float mConcealStartDelay; // 0x32c
    float mConcealEntryDelay; // 0x330
    float mConcealScale; // 0x334
    bool mAutoReveal; // 0x338
    BandListState mBandListState; // 0x33c
    float mShouldbeRevealedTimeStamp; // 0x340
    ObjVector<HighlightObject> mHighlightObjects; // 0x344
};