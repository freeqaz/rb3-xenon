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

    int mNumTracks; // 0x484
    int unk488; // 0x488
    int mGemTrackDirID; // 0x48c
    int mKickPassCounter; // 0x490
    int unk494; // 0x494
    float mStreakMeterOffset; // 0x498
    float mStreakMeterTilt; // 0x49c
    float mTrackPitch; // 0x4a0
    ObjPtr<RndDir> mEffectSelector; // 0x4a4
    ObjPtr<RndGroup> mRotater; // 0x4b0
    ObjPtr<RndTex> mSurfaceTexture; // 0x4bc
    ObjPtr<RndMesh> mSurfaceMesh; // 0x4c8
    ObjPtr<RndMat> mSurfaceMat; // 0x4d4
    ObjPtr<RndEnviron> mTrackEnv; // 0x4e0
    ObjPtr<RndEnviron> mTrackMissGemsEnv; // 0x4ec
    ObjPtr<RndCam> mGameCam; // 0x4f8
    ObjPtr<EventTrigger> mPeakStateOnTrig; // 0x504
    ObjPtr<EventTrigger> mPeakStateOffTrig; // 0x510
    ObjPtr<EventTrigger> mPeakStopImmediateTrig; // 0x51c
    ObjPtr<EventTrigger> mBassSuperStreakOnTrig; // 0x528
    ObjPtr<EventTrigger> mBassSuperStreakOffTrig; // 0x534
    ObjPtr<EventTrigger> mBassSSOffImmediateTrig; // 0x540
    ObjPtr<EventTrigger> mKickDrummerTrig; // 0x54c
    ObjPtr<EventTrigger> mKickDrummerResetTrig; // 0x558
    ObjPtr<EventTrigger> mSpotlightPhraseSuccessTrig; // 0x564
    std::vector<ObjPtr<RndPropAnim> > mGemMashAnims; // 0x570
    std::vector<ObjPtr<RndPropAnim> > mDrumMashAnims; // 0x578
    std::vector<ObjPtr<RndPropAnim> > mFillLaneAnims; // 0x580
    std::vector<ObjPtr<RndPropAnim> > mRealGuitarMashAnims; // 0x588
    std::vector<
        std::pair<ObjPtr<EventTrigger>, ObjPtr<EventTrigger> > >
        mDrumRollTrigs; // 0x63c
    std::vector<
        std::pair<ObjPtr<EventTrigger>, ObjPtr<EventTrigger> > >
        mTrillTrigs; // 0x648
    std::vector<ObjPtr<EventTrigger> > mFillHitTrigs; // 0x5a0
    ObjPtr<EventTrigger> mDrumFillResetTrig; // 0x5a8
    ObjPtr<RndPropAnim> mDrumMash2ndPassActivateAnim; // 0x5b4
    ObjPtr<RndGroup> mDrumMashHitAnimGrp; // 0x5c0
    ObjPtr<RndGroup> mFillColorsGrp; // 0x5cc
    ObjPtr<RndPropAnim> mLodAnim; // 0x5d8
    ObjPtr<RndDir> mSmasherPlate; // 0x5e4
    ObjPtrList<TrackWidget, ObjectDir> mGlowWidgets; // 0x5f0
    ObjPtr<Task> unk600; // 0x600
    ObjPtr<Task> unk60c; // 0x60c
    ObjPtr<Task> unk618; // 0x618
    ObjPtr<Task> unk624; // 0x624
    ObjPtr<RndMesh> mGemWhiteMesh; // 0x630
    ObjPtr<EventTrigger> mMissOutofRangeRightTrig; // 0x63c
    ObjPtr<EventTrigger> mMissOutofRangeLeftTrig; // 0x648
    ObjPtr<RndAnimatable> unk654; // 0x654
    ObjPtr<RndAnimatable> mKeysShiftAnim; // 0x660
    ObjPtr<RndPropAnim> mKeysMashAnim; // 0x66c
    float mKeyRange; // 0x734
    float mKeyOffset; // 0x738
    std::vector<RndDir *> unk680; // 0x73c
    std::vector<EventTrigger *> unk688; // 0x748
    std::vector<EventTrigger *> unk690; // 0x754
    FingerShape *mFingerShape; // 0x760
    std::vector<float> mFretPosOffsets; // 0x69c
    float mChordLabelPosOffset; // 0x6a4
    ObjPtr<ChordShapeGenerator> mChordShapeGen; // 0x6a8
    std::map<unsigned int, std::pair<int, RndMesh *> > unk6b4; // 0x6b4
    std::map<unsigned int, std::pair<int, RndMesh *> > unk6cc; // 0x6cc
    ArpeggioShapePool *mArpShapePool; // 0x7b0
    bool unk6e8; // 0x6e8
    // Retail RB3-360 (MILO_DEBUG off) ends here: sizeof == 0x6ec. rb3-Wii (dev)
    // gates 4 more members under MILO_DEBUG (mFakeFingerShape,
    // mCycleFakeFingerShapes, mRandomShapeFrameCount, RGState mRGState) — removed
    // outright since macros.h force-defines MILO_DEBUG and GemTrackDir.cpp is
    // now wired (ctor + factory bake sizeof; multiple TUs see this layout).
};

int WhiteKeyToSemitone(int);
int SemitoneToWhiteKey(int);