#pragma once
#include "bandobj/BandTrack.h"
#include "bandobj/CrowdMeterIcon.h" // CrowdMeterState (was transitively pulled in rb3-Wii)
#include "bandobj/PitchArrow.h"
#include "math/Color.h"
#include "rndobj/Dir.h"
#include "rndobj/Group.h"
#include "rndobj/Text.h"

enum VocalHUDColor;

class VocalTrackDir : public RndDir, public BandTrack {
public:
    enum HarmonyShowingState {
    };

    VocalTrackDir();
    OBJ_CLASSNAME(VocalTrackDir)
    OBJ_SET_TYPE(VocalTrackDir)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual ~VocalTrackDir() {}
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    virtual void SyncObjects();
    virtual void TrackReset();
    virtual void ResetSmashers(bool);
    virtual void PlayIntro();
    virtual void Deploy();
    virtual void SetPlayerLocal(float);
#ifdef HX_NATIVE
    // V3 — clang/clang-LP64 treats empty-body non-void inline virtuals as
    // undefined behavior and emits a ud2 (SIGILL) trap. RB3's matched-fork
    // header uses these as "covariant return placeholders" relying on MWCC's
    // bug-tolerant fallthrough. Provide a real return: VocalTrackDir IS-A
    // RndDir IS-A ObjectDir, so `this` is the right self-pointer.
    virtual ObjectDir *ThisDir() { return this; }
    virtual ObjectDir *ThisDir() const { return const_cast<VocalTrackDir *>(this); }
#else
    // Retail RB3 (MWCC) used empty inline bodies relying on r3==this fallthrough;
    // MSVC X360 rejects empty non-void bodies (C4716). A Dir's ThisDir is itself.
    virtual ObjectDir *ThisDir() { return this; }
    virtual ObjectDir *ThisDir() const { return const_cast<VocalTrackDir *>(this); }
#endif
    virtual void SpotlightPhraseSuccess();
#ifdef HX_NATIVE
    virtual VocalTrackDir *AsVocalTrackDir() { return this; }
#else
    virtual VocalTrackDir *AsVocalTrackDir() { return this; }
#endif
    virtual RndDir *AsRndDir() { return AsVocalTrackDir(); }
    virtual void Reset();
    virtual void Retract(bool);
    virtual void Extend(bool);
    virtual void RefreshCrowdRating(float, CrowdMeterState);
    virtual void SetPerformanceMode(bool);
    virtual void SetTambourine(bool);
    virtual void TutorialReset();

    void SetConfiguration(Hmx::Object *, HarmonyShowingState);
    void UpdateConfiguration();
    void ShowPhraseFeedback(int, int, int, bool);
    void SetStreakPct(float);
    void SetEnableVocalsOptions(bool);
    void ApplyFontStyle(Hmx::Object *);
    void ApplyArrowStyle(Hmx::Object *);
    void SetIsolatedPart(int);
    int NumVocalParts();
    void SetRange(float, float, int, bool);
    void UpdateTubeStyle();
    void ConfigPanels();
    PitchArrow *GetPitchArrow(int);
    Hmx::Color GetLyricColor(int) const;
    float GetLyricAlpha(int) const;
    float PitchToZ(float, bool) const;
    void Tambourine(Symbol);
    void TambourineNote();
    void SetVocalLineColors(VocalHUDColor *);
    void UpdateVocalMeters(bool, bool, bool, bool);
    void ShowMicDisplay(bool);
    void SetMicDisplayLabel(Symbol);
    void SetMissingMicsForDisplay(bool, bool, bool);
    void CanChat(bool);
    void RecalculateLyricZ(bool *, bool *);
    void SetupNetVocals();
    void UpdatePartIsolation();
    void SortArrowFx();

    DataNode DataForEachConfigObj(DataArray *);
    DataNode OnGetDisplayMode(DataArray *);
    DataNode OnSetDisplayMode(DataArray *);
    DataNode OnSetLyricColor(const DataArray *);
    DataNode OnIsolatePart(DataArray *);

    DECLARE_REVS;
    NEW_OVERLOAD;
    DELETE_OVERLOAD;
    NEW_OBJ(VocalTrackDir)
    static void Init() { Register(); }
    REGISTER_OBJ_FACTORY_FUNC(VocalTrackDir)

    float mHiddenPartAlpha; // 0x2f0
    bool mEnableVocalsOptions; // 0x2f4
    bool unk2a5; // 0x2f5
    bool mIsTop; // 0x2f6
    bool unk2a7; // 0x2f7
    int mFeedbackStateLead; // 0x2f8
    int mFeedbackStateHarm1; // 0x2fc
    int mFeedbackStateHarm2; // 0x300
    std::map<int, Hmx::Color> mLyricColorMap; // 0x304
    std::map<int, float> mLyricAlphaMap; // 0x31c
    ObjPtr<RndDir> mVocalMics; // 0x334
    ObjPtr<RndDir> mVocalistVolume; // 0x340
    float mMinPitchRange; // 0x34c
    float mPitchDisplayMargin; // 0x350
    float mArrowSmoothing; // 0x354
    ObjPtrList<RndTransformable> mConfigurableObjects; // 0x358
    ObjPtr<Hmx::Object> mVoxCfg; // 0x36c
    ObjPtr<RndDir> mTambourineSmasher; // 0x378
    ObjPtr<EventTrigger> mTambourineNowShowTrig; // 0x384
    ObjPtr<EventTrigger> mTambourineNowHideTrig; // 0x390
    ObjPtr<BandLabel> mLeadPhraseFeedbackBottomLbl; // 0x39c
    ObjPtr<EventTrigger> mPhraseFeedbackTrig; // 0x3a8
    ObjPtr<EventTrigger> mSpotlightSparklesOnlyTrig; // 0x3b4
    ObjPtr<EventTrigger> mSpotlightPhraseSuccessTrig; // 0x3c0
    ObjPtr<PitchArrow> mPitchArrow1; // 0x3cc
    ObjPtr<PitchArrow> mPitchArrow2; // 0x3d8
    ObjPtr<PitchArrow> mPitchArrow3; // 0x3e4
    bool mPitchWindow; // 0x3f0
    float mPitchWindowHeight; // 0x3f4
    ObjPtr<RndMesh> mPitchWindowMesh; // 0x3f8
    ObjPtr<RndMesh> mPitchWindowOverlay; // 0x404
    bool mLeadLyrics; // 0x410
    float mLeadLyricHeight; // 0x414
    ObjPtr<RndMesh> mLeadLyricMesh; // 0x418
    bool mHarmLyrics; // 0x424
    float mHarmLyricHeight; // 0x428
    ObjPtr<RndMesh> mHarmLyricMesh; // 0x42c
    ObjPtr<RndMesh> mLeftDecoMesh; // 0x438
    ObjPtr<RndMesh> mRightDecoMesh; // 0x444
    float mNowBarWidth; // 0x450
    ObjPtr<RndMesh> mNowBarMesh; // 0x454
    bool mRemoteVocals; // 0x460
    float mTrackLeftX; // 0x464
    float mTrackRightX; // 0x468
    float mTrackBottomZ; // 0x46c
    float mTrackTopZ; // 0x470
    float mPitchBottomZ; // 0x474
    float mPitchTopZ; // 0x478
    float mNowBarX; // 0x47c
    float unk42c; // 0x480
    Symbol mPitchGuides; // 0x484
    ObjPtr<Hmx::Object> mTubeStyle; // 0x488
    ObjPtr<Hmx::Object> mArrowStyle; // 0x494
    ObjPtr<Hmx::Object> mFontStyle; // 0x4a0
    ObjPtr<RndText> mLeadText; // 0x4ac
    ObjPtr<RndText> mHarmText; // 0x4b8
    ObjPtr<RndText> mLeadPhonemeText; // 0x4c4
    ObjPtr<RndText> mHarmPhonemeText; // 0x4d0
    float mLastMin; // 0x4dc
    float mLastMax; // 0x4e0
    float mMiddleCZPos; // 0x4e4
    int mTonic; // 0x4e8
    ObjPtr<RndAnimatable> mRangeScaleAnim; // 0x4ec
    ObjPtr<RndAnimatable> mRangeOffsetAnim; // 0x4f8
    bool unk4b0; // 0x504
    int unk4b4; // 0x508
    RndTransformable *mLeftTrans; // 0x50c
    RndTransformable *mRightTrans; // 0x510
    RndTransformable *mBottomTrans; // 0x514
    RndTransformable *mTopTrans; // 0x518
    RndTransformable *mPitchBottomTrans; // 0x51c
    RndTransformable *mPitchTopTrans; // 0x520
    RndTransformable *mPitchMidTrans; // 0x524
    RndTransformable *mNowTrans; // 0x528
    ObjPtr<RndGroup> mTubeRangeGrp; // 0x52c
    ObjPtr<RndGroup> mTubeSpotlightGrp; // 0x538
    ObjPtr<RndGroup> mTubeBack0Grp; // 0x544
    ObjPtr<RndGroup> mTubeBack1Grp; // 0x550
    ObjPtr<RndGroup> mTubeBack2Grp; // 0x55c
    ObjPtr<RndGroup> mTubeFront0Grp; // 0x568
    ObjPtr<RndGroup> mTubeFront1Grp; // 0x574
    ObjPtr<RndGroup> mTubeFront2Grp; // 0x580
    ObjPtr<RndGroup> mTubeGlow0Grp; // 0x58c
    ObjPtr<RndGroup> mTubeGlow1Grp; // 0x598
    ObjPtr<RndGroup> mTubeGlow2Grp; // 0x5a4
    ObjPtr<RndGroup> mTubePhoneme0Grp; // 0x5b0
    ObjPtr<RndGroup> mTubePhoneme1Grp; // 0x5bc
    ObjPtr<RndGroup> mTubePhoneme2Grp; // 0x5c8
    ObjPtr<RndMat> mSpotlightMat; // 0x5d4
    ObjPtr<RndMat> mLeadBackMat; // 0x5e0
    ObjPtr<RndMat> mHarm1BackMat; // 0x5ec
    ObjPtr<RndMat> mHarm2BackMat; // 0x5f8
    ObjPtr<RndMat> mLeadFrontMat; // 0x604
    ObjPtr<RndMat> mHarm1FrontMat; // 0x610
    ObjPtr<RndMat> mHarm2FrontMat; // 0x61c
    ObjPtr<RndMat> mLeadGlowMat; // 0x628
    ObjPtr<RndMat> mHarm1GlowMat; // 0x634
    ObjPtr<RndMat> mHarm2GlowMat; // 0x640
    ObjPtr<RndMat> mLeadPhonemeMat; // 0x64c
    ObjPtr<RndMat> mHarm1PhonemeMat; // 0x658
    ObjPtr<RndMat> mHarm2PhonemeMat; // 0x664
    ObjPtr<RndGroup> mVocalsGrp; // 0x670
    ObjPtr<RndTransformable> mScroller; // 0x67c
    ObjPtr<RndTransformable> mLeadLyricScroller; // 0x688
    ObjPtr<RndTransformable> mHarmonyLyricScroller; // 0x694
    ObjPtr<RndGroup> mBREGrp; // 0x6a0
    ObjPtr<RndGroup> mLeadBREGrp; // 0x6ac
    ObjPtr<RndGroup> mHarmonyBREGrp; // 0x6b8
    ObjPtr<RndGroup> mPitchScrollGroup; // 0x6c4
    ObjPtr<RndGroup> mLeadLyricScrollGroup; // 0x6d0
    ObjPtr<RndGroup> mHarmonyLyricScrollGroup; // 0x6dc
    float unk694; // 0x6e8
    float unk698; // 0x6ec
    float unk69c; // 0x6f0
    float unk6a0; // 0x6f4
    ObjPtr<RndMat> mLeadDeployMat; // 0x6f8
    ObjPtr<RndMat> mHarmDeployMat; // 0x704
    float mGlowSize; // 0x710
    float mGlowAlpha; // 0x714
    int unk6c4; // 0x718
    bool unk6c8; // 0x71c
    ObjPtr<RndGroup> mArrowFXDrawGrp; // 0x720
    float unk6d8; // 0x72c
    float unk6dc; // 0x730
    bool unk6e0; // 0x734
};