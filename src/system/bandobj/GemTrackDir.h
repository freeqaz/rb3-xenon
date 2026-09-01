#pragma once
#include "track/TrackDir.h"
#include "bandobj/BandTrack.h"
#include "bandobj/TrackInstruments.h"
#include "rndobj/Group.h"
#include "rndobj/Tex.h"
#include "rndobj/Mesh.h"
#include "rndobj/Mat.h"
#include "rndobj/Env.h"
#include "rndobj/Cam.h"
#include "rndobj/EventTrigger.h"
#include "rndobj/PropAnim.h"
#include "track/TrackWidget.h"
#include "obj/Task.h"
#include "bandobj/ChordShapeGenerator.h"
#include "beatmatch/RGState.h"
#include "bandobj/FingerShape.h"
#include "bandobj/ArpeggioShape.h"

class GemTrackDir : public TrackDir, public BandTrack {
public:
    GemTrackDir();
    OBJ_CLASSNAME(GemTrackDir)
    OBJ_SET_TYPE(GemTrackDir)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual ~GemTrackDir();
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    virtual void SyncObjects();
    virtual void Poll();
    // Not virtual -- follows TrackDir, where the retail evidence lives.
    void SyncFingerFeedback();
    virtual void SetDisplayRange(float);
    virtual void SetDisplayOffset(float, bool);
    virtual RndDir *SmasherPlate() { return mSmasherPlate; }
    virtual float GetFretPosOffset(int) const;
    virtual int GetNumFretPosOffsets() const { return mFretPosOffsets.size(); }
    virtual float GetCurrentChordLabelPosOffset() const;
    virtual int PrepareChordMesh(unsigned int);
    virtual RndMesh *GetChordMesh(unsigned int, bool);
    virtual void SetUnisonProgress(float);
    virtual void ClearChordMeshRefCounts();
    virtual void DeleteUnusedChordMeshes();
    virtual void AddChordRepImpl(
        RndMesh *,
        TrackWidget *,
        TrackWidget *,
        TrackWidget *,
        float,
        const std::vector<int> &,
        class String
    );
    virtual ArpeggioShapePool *GetArpeggioShapePool() { return mArpShapePool; }
    virtual bool IsBlackKey(int) const;
    virtual void KeyMissLeft();
    virtual void KeyMissRight();
    virtual bool IsActiveInSession() const;
    virtual void PlayIntro();
    virtual void TrackReset();
    virtual void ResetSmashers(bool);
    // ⚠ GameWon IS virtual in retail -- do not "fix" this one.  It was
    // de-virtualized once on the strength of a slot-count argument and the
    // measurement refuted it immediately (-1 matched / -80 B).  Retail's
    // GemTrackDir primary vtable (0x820276c4) holds GameWon in its LAST slot
    // (35) at 0x822e6730, and our body for it scores fuzzy 100.0 against that
    // very address -- so the address is GameWon, the map's `UAAXXZ` spelling
    // is right, and the slot is real.  Removing `virtual` re-mangles the
    // symbol U->Q, our obj stops defining the name the map assigns to
    // 0x822e6730, and objdiff un-pairs the row to 0% forever.
    // The surplus slot was SyncFingerFeedback, one slot earlier.
    virtual void GameWon();
    virtual void Retract(bool);
    virtual void Extend(bool);
    virtual void SetStreak(int, int, int, bool);
    virtual void PeakState(bool, bool);
    virtual void SuperStreak(bool, bool);
    virtual void Deploy();
    virtual void EnterCoda();
    virtual void DisablePlayer(int);
    virtual void SetPlayerLocal(float);
#ifdef HX_NATIVE
    // V3 — clang-LP64 ud2 trap; see VocalTrackDir.h for the same pattern.
    virtual ObjectDir *ThisDir() { return this; }
    virtual ObjectDir *ThisDir() const { return const_cast<GemTrackDir *>(this); }
#else
    // MSVC X360 rejects empty non-void bodies (C4716). A Dir's ThisDir is itself.
    virtual ObjectDir *ThisDir() { return this; }
    virtual ObjectDir *ThisDir() const { return const_cast<GemTrackDir *>(this); }
#endif
    virtual void RefreshStreakMeter(int, int, int);
    virtual void SpotlightPhraseSuccess();
#ifdef HX_NATIVE
    virtual GemTrackDir *AsGemTrackDir() { return this; }
#else
    virtual GemTrackDir *AsGemTrackDir() { return this; }
#endif
    virtual RndDir *AsRndDir() { return AsGemTrackDir(); }
    virtual void SetPerformanceMode(bool);
    virtual void SetInstrument(TrackInstrument);
    virtual void SetupInstrument();
    virtual void ResetEffectSelector();
    virtual void SetupSmasherPlate();
    virtual void ReleaseSmasherPlate();

    void SetPitch(float);
    void SetFade(float, float);
    void SetFOV(float);
    void SetCamPos(float, float, float);
    void SetScreenRectX(float);
    void SetTrackOffset(float);
    void SetSideAngle(float);
    void Mash(int);
    void CrashFill();
    bool ToggleKeyShifting();
    void UpdateSurfaceTexture();
    void OnUpdateFx(int);
    void GemPass(int, int);
    void GemHit(int);
    void SeeKick();
    void KickSwing();
    void FillMash(int);
    void FillHit(int);
    void ResetDrumFill();
    void ResetCoda();
    float GetKeyRange();
    float GetKeyOffset();
    void UpdateFingerFeedback(const RGState &);
    void UpdateLeftyFlip(bool);
    bool KeyShifting();
    void FreeChordMeshes();
    void FreeChordMeshes(std::map<unsigned int, std::pair<int, RndMesh *> > &);
    void ClearChordMeshRefCounts(std::map<unsigned int, std::pair<int, RndMesh *> > &);
    void SetGemTrackID(int id) { mGemTrackDirID = id; }

    DataNode OnDrawSampleChord(DataArray *);

    NEW_OVERLOAD;
    DELETE_OVERLOAD;
    NEW_OBJ(GemTrackDir)
    static void Init() { Register(); }
    REGISTER_OBJ_FACTORY_FUNC(GemTrackDir)

    int mNumTracks; // 0x520
    int unk488; // 0x524
    int mGemTrackDirID; // 0x528
    int mKickPassCounter; // 0x52c
    int unk494; // 0x530
    float mStreakMeterOffset; // 0x534
    float mStreakMeterTilt; // 0x538
    float mTrackPitch; // 0x53c
    ObjPtr<RndDir> mEffectSelector; // 0x540
    ObjPtr<RndGroup> mRotater; // 0x54c
    ObjPtr<RndTex> mSurfaceTexture; // 0x558
    ObjPtr<RndMesh> mSurfaceMesh; // 0x564
    ObjPtr<RndMat> mSurfaceMat; // 0x570
    ObjPtr<RndEnviron> mTrackEnv; // 0x57c
    ObjPtr<RndEnviron> mTrackMissGemsEnv; // 0x588
    ObjPtr<RndCam> mGameCam; // 0x594
    ObjPtr<EventTrigger> mPeakStateOnTrig; // 0x5a0
    ObjPtr<EventTrigger> mPeakStateOffTrig; // 0x5ac
    ObjPtr<EventTrigger> mPeakStopImmediateTrig; // 0x5b8
    ObjPtr<EventTrigger> mBassSuperStreakOnTrig; // 0x5c4
    ObjPtr<EventTrigger> mBassSuperStreakOffTrig; // 0x5d0
    ObjPtr<EventTrigger> mBassSSOffImmediateTrig; // 0x5dc
    ObjPtr<EventTrigger> mKickDrummerTrig; // 0x5e8
    ObjPtr<EventTrigger> mKickDrummerResetTrig; // 0x5f4
    ObjPtr<EventTrigger> mSpotlightPhraseSuccessTrig; // 0x600
    std::vector<ObjPtr<RndPropAnim> > mGemMashAnims; // 0x60c
    std::vector<ObjPtr<RndPropAnim> > mDrumMashAnims; // 0x618
    std::vector<ObjPtr<RndPropAnim> > mFillLaneAnims; // 0x624
    std::vector<ObjPtr<RndPropAnim> > mRealGuitarMashAnims; // 0x630
    std::vector<
        std::pair<ObjPtr<EventTrigger>, ObjPtr<EventTrigger> > >
        mDrumRollTrigs; // 0x63c
    std::vector<
        std::pair<ObjPtr<EventTrigger>, ObjPtr<EventTrigger> > >
        mTrillTrigs; // 0x648
    std::vector<ObjPtr<EventTrigger> > mFillHitTrigs; // 0x654
    ObjPtr<EventTrigger> mDrumFillResetTrig; // 0x660
    ObjPtr<RndPropAnim> mDrumMash2ndPassActivateAnim; // 0x66c
    ObjPtr<RndGroup> mDrumMashHitAnimGrp; // 0x678
    ObjPtr<RndGroup> mFillColorsGrp; // 0x684
    ObjPtr<RndPropAnim> mLodAnim; // 0x690
    ObjPtr<RndDir> mSmasherPlate; // 0x69c
    ObjPtrList<TrackWidget, ObjectDir> mGlowWidgets; // 0x6a8
    ObjPtr<Task> unk600; // 0x6bc
    ObjPtr<Task> unk60c; // 0x6c8
    ObjPtr<Task> unk618; // 0x6d4
    ObjPtr<Task> unk624; // 0x6e0
    ObjPtr<RndMesh> mGemWhiteMesh; // 0x6ec
    ObjPtr<EventTrigger> mMissOutofRangeRightTrig; // 0x6f8
    ObjPtr<EventTrigger> mMissOutofRangeLeftTrig; // 0x704
    ObjPtr<RndAnimatable> unk654; // 0x710
    ObjPtr<RndAnimatable> mKeysShiftAnim; // 0x71c
    ObjPtr<RndPropAnim> mKeysMashAnim; // 0x728
    float mKeyRange; // 0x734
    float mKeyOffset; // 0x738
    std::vector<RndDir *> unk680; // 0x73c
    std::vector<EventTrigger *> unk688; // 0x748
    std::vector<EventTrigger *> unk690; // 0x754
    FingerShape *mFingerShape; // 0x760
    std::vector<float> mFretPosOffsets; // 0x764
    float mChordLabelPosOffset; // 0x770
    ObjPtr<ChordShapeGenerator> mChordShapeGen; // 0x774
    std::map<unsigned int, std::pair<int, RndMesh *> > unk6b4; // 0x780
    std::map<unsigned int, std::pair<int, RndMesh *> > unk6cc; // 0x798
    ArpeggioShapePool *mArpShapePool; // 0x7b0
    bool unk6e8; // 0x7b4
    // Retail RB3-360 (MILO_DEBUG off) ends here: sizeof == 0x6ec. rb3-Wii (dev)
    // gates 4 more members under MILO_DEBUG (mFakeFingerShape,
    // mCycleFakeFingerShapes, mRandomShapeFrameCount, RGState mRGState) — removed
    // outright since macros.h force-defines MILO_DEBUG and GemTrackDir.cpp is
    // now wired (ctor + factory bake sizeof; multiple TUs see this layout).
};

int WhiteKeyToSemitone(int);
int SemitoneToWhiteKey(int);