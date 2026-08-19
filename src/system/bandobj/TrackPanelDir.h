#pragma once
// Ported from rb3-Wii src/system/bandobj/TrackPanelDir.h.
// Uses single-arg ObjPtr<T> (rb3-xenon convention); base TrackPanelDirBase
// keeps mGemTracks as ObjVector<ObjPtr<RndDir> > (GemTrackDir IS-A RndDir),
// so GemTrackDir-typed access in the .cpp goes through a cast.
#include "bandobj/TrackPanelDirBase.h"
#include "bandobj/VocalTrackDir.h"
#include "bandobj/BandCrowdMeter.h"
#include "bandobj/EndingBonus.h"
#include "bandobj/GemTrackResourceManager.h"
#include "rndobj/EventTrigger.h"

class TrackPanelDir : public TrackPanelDirBase {
public:
    TrackPanelDir();
    OBJ_CLASSNAME(TrackPanelDir)
    OBJ_SET_TYPE(TrackPanelDir)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual ~TrackPanelDir();
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

    void GameWon();
    void GameLost();
    void ConfigureCrowdMeter();
    void ApplyVocalTrackShowingStatus();
    TrackInstrument GetInstrument(int) const;
    void SetBotbBandIcon(ObjectDir *, RndDir *, bool);

    NEW_OVERLOAD;
    DELETE_OVERLOAD;
    NEW_OBJ(TrackPanelDir)
    static void Init() { Register(); }
    REGISTER_OBJ_FACTORY_FUNC(TrackPanelDir)

    int unk244; // 0x244
    int mTestMultiplier; // 0x248
    int unk24c; // 0x2bc
    int unk250; // 0x250
    int unk254; // 0x254
    ObjPtr<VocalTrackDir> mVocalTrack; // 0x258
    ObjPtr<BandCrowdMeter> mCrowdMeter; // 0x264
    ObjPtr<RndDir> mBandScoreMultiplier; // 0x270
    ObjPtr<EventTrigger> mBandScoreMultiplierTrig; // 0x27c
    ObjPtr<EndingBonus> mEndingBonus; // 0x288
    ObjPtr<RndDir> mScoreboard; // 0x294
    ObjPtr<RndGroup> mPulseAnimGrp; // 0x2a0
    bool unk2ac; // 0x31c
    bool unk2ad; // 0x31d
    bool mTracksExtended; // 0x2ae
    // Retail X360 stores this as a RAW owning pointer (4 bytes at this+0x320),
    // not an ObjPtr: the ctor emits a single `stw r0, 0x320(this)` with no
    // ObjRef construction, and ~TrackPanelDir plain-deletes it. (rb3-Wii's dev
    // header declares an ObjPtr here; the retail bytes disagree, bytes win.)
    GemTrackResourceManager *mGemTrackRsrcMgr; // 0x320
    bool mVocals; // 0x324
    bool mVocalsNet; // 0x325
    int mGemInst[4]; // 0x328
    bool mGemNet[4]; // 0x338
    // Five further ObjPtr members retail constructs after mGemNet and destroys
    // via EH funclets at this+0x33c/0x348/0x354/0x360/0x36c, plus a trailing
    // bool at 0x378. Reconstructed from the target's cleanup census; absent
    // from the rb3-Wii dev header. Sizing them exactly re-seats the virtual
    // bases at 0x380 (Hmx::Object) / 0x3b4 (RndHighlightable), matching retail.
    ObjPtr<EventTrigger> unk33c; // 0x33c
    ObjPtr<EventTrigger> unk348; // 0x348
    ObjPtr<EventTrigger> unk354; // 0x354
    ObjPtr<EventTrigger> unk360; // 0x360
    ObjPtr<RndGroup> unk36c; // 0x36c
    bool unk378; // 0x378
};
