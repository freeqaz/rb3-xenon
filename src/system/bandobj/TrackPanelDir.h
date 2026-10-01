#pragma once
// TrackPanelDir (bandobj/TrackPanelDir.h).
// Uses single-arg ObjPtr<T> (rb3-xenon convention).
#include "bandobj/TrackPanelDirBase.h"
#include "bandobj/VocalTrackDir.h"
#include "bandobj/BandCrowdMeter.h"
#include "bandobj/EndingBonus.h"
#include "bandobj/GemTrackResourceManager.h"
#include "rndobj/EventTrigger.h"

class BandLabel;

class TrackPanelDir : public TrackPanelDirBase {
public:
    TrackPanelDir();
    OBJ_CLASSNAME(TrackPanelDir)
    OBJ_SET_TYPE(TrackPanelDir)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    // No user-declared dtor: retail's ~TrackPanelDir (0x82309DE0) makes no
    // vtable/vtordisp stores and does not free mGemTrackRsrcMgr.
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    virtual void SyncObjects();
    virtual void ConfigureTracks(bool);
    virtual void ConfigureTrack(int);
    virtual void AssignTracks();
    virtual void AssignTrack(int, TrackInstrument, bool);
    virtual void RemoveTrack(int);
    virtual void SetConfiguration(Hmx::Object *, bool);
    virtual void ReapplyConfiguration(bool);
    virtual void Reset();
    virtual void ResetAll();
    virtual void PlayIntro();
    virtual bool TracksExtended() const { return mTracksExtended; }
    virtual void GameOver();
    virtual void HideScore();
    virtual void Coda();
    virtual void CodaEnd();
    virtual void SetCodaScore(int);
    virtual void SoloEnd(BandTrack *, int, Symbol);
    virtual void SetTrackPanel(TrackPanelInterface *);
    virtual void ResetPlayers();
    virtual void StartFinale();
    virtual void SetMultiplier(int, bool);
    virtual void SetCrowdRating(float);
    virtual void CodaSuccess();
    virtual void UnisonStart(int);
    virtual void UnisonEnd();
    virtual void UnisonSucceed();
    virtual EndingBonus *GetEndingBonus() { return mEndingBonus; }
    virtual BandCrowdMeter *GetCrowdMeter() { return mCrowdMeter; }
    virtual void
    SetupApplauseMeter(int, const char *, const char *, RndDir *, RndDir *, bool, Symbol);
    virtual void DisablePlayer(int, bool);
    virtual void EnablePlayer(int);
    virtual void FadeBotbBandNames(bool);
    virtual void CleanUpChordMeshes();
    virtual void SetApplauseMeterScale(int, int);
    virtual void StartPulseAnims(float);
    virtual GemTrackResourceManager *GetGemTrackResourceManager() const {
        return mGemTrackRsrcMgr;
    }
    // The two RB3-360-only slots (0xd4/0xd8, see TrackPanelDirBase.h):
    // write the audition time readout, and re-enable it after GameOver.
    virtual void Unkd4(const char *, const char *, const char *, Symbol);
    virtual void Unkd8() { unk378 = false; }

    void GameWon();
    void GameLost();
    void ConfigureCrowdMeter();
    void UpdateTimeInfo();
    void ApplyVocalTrackShowingStatus();
    TrackInstrument GetInstrument(int) const;
    void SetBotbBandIcon(ObjectDir *, RndDir *, bool);

    NEW_OVERLOAD;
    DELETE_OVERLOAD_INLINE;
    NEW_OBJ(TrackPanelDir)
    static void Init() { Register(); }
    REGISTER_OBJ_FACTORY_FUNC(TrackPanelDir)

    int unk244; // 0x2b4
    int mTestMultiplier; // 0x2b8
    int unk24c; // 0x2bc
    int unk250; // 0x2c0
    int unk254; // 0x2c4
    ObjPtr<VocalTrackDir> mVocalTrack; // 0x2c8
    ObjPtr<BandCrowdMeter> mCrowdMeter; // 0x2d4
    ObjPtr<RndDir> mBandScoreMultiplier; // 0x2e0
    ObjPtr<EventTrigger> mBandScoreMultiplierTrig; // 0x2ec
    ObjPtr<EndingBonus> mEndingBonus; // 0x2f8
    ObjPtr<RndDir> mScoreboard; // 0x304
    ObjPtr<RndGroup> mPulseAnimGrp; // 0x310
    bool unk2ac; // 0x31c
    bool unk2ad; // 0x31d
    bool mTracksExtended; // 0x31e
    // Retail X360 stores this as a RAW owning pointer (4 bytes at this+0x320),
    // not an ObjPtr: the ctor emits a single `stw r0, 0x320(this)` with no
    // ObjRef construction. ~TrackPanelDir (0x82309DE0) does not free it; the
    // only release is SyncObjects' RELEASE before re-creating it. (An ObjPtr
    // here is wrong; the retail bytes decide.)
    GemTrackResourceManager *mGemTrackRsrcMgr; // 0x320
    bool mVocals; // 0x324
    bool mVocalsNet; // 0x325
    int mGemInst[4]; // 0x328
    bool mGemNet[4]; // 0x338
    // The audition-mode time readout (UpdateTimeInfo): the four labels and the
    // group of ui/track/time_info.milo. Retail's ctor stores the
    // ObjPtr<BandLabel> vtable (RTTI) for the four labels. Sizing them exactly
    // re-seats the virtual bases at 0x380 (Hmx::Object) / 0x3b4
    // (RndHighlightable), matching retail.
    ObjPtr<BandLabel> mTimeMbt; // 0x33c
    ObjPtr<BandLabel> mTimeElapsed; // 0x348
    ObjPtr<BandLabel> mTimeRemaining; // 0x354
    ObjPtr<BandLabel> mTimeSection; // 0x360
    ObjPtr<RndGroup> mTimeGrp; // 0x36c
    // Set by GameOver, cleared by Unkd8: UpdateTimeInfo does nothing at all.
    bool unk378; // 0x378
};
