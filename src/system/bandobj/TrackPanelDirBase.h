#pragma once
// Ported from rb3-Wii src/system/bandobj/TrackPanelDirBase.h.
// ObjPtr<T,ObjectDir> -> ObjPtr<T>; GemTrackDir & friends forward-declared
// (used only via pointer/ObjPtr/ObjVector) to avoid pulling GemTrackDir.h
// (not present in src/system) and its transitive tail.
#include "ui/PanelDir.h"
#include "bandobj/TrackInstruments.h"
#include "rndobj/Group.h" // RndGroup used inline in Showing()
#include "obj/ObjMacros.h" // DECLARE_REVS / NEW_OVERLOAD / DELETE_OVERLOAD

class BandTrack;
class TrackPanelInterface;
class GemTrackResourceManager;
class EndingBonus;
class BandCrowdMeter;
class GemTrackDir;

class TrackPanelDirBase : public PanelDir {
public:
    TrackPanelDirBase();
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual ~TrackPanelDirBase() {}
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &bs);
    virtual void Enter();
    virtual void ConfigureTracks(bool) = 0;
    virtual void ConfigureTrack(int) = 0;
    virtual void AssignTracks() = 0;
    virtual void AssignTrack(int, TrackInstrument, bool) = 0;
    virtual void RemoveTrack(int) = 0;
    virtual void SetConfiguration(Hmx::Object *, bool);
    virtual void ReapplyConfiguration(bool);
    virtual void Reset() = 0;
    virtual void ResetAll() {}
    virtual void PlayIntro();
    virtual bool TracksExtended() const { return false; }
    virtual void GameOver();
    virtual void HideScore();
    virtual void Coda() = 0;
    virtual void CodaEnd() = 0;
    virtual void SetCodaScore(int) {}
    virtual void SoloEnd(BandTrack *, int, Symbol) = 0;
    virtual void SetTrackPanel(TrackPanelInterface *panel) { mTrackPanel = panel; }
    virtual void ResetPlayers() {}
    virtual void StartFinale() {}
    virtual void SetMultiplier(int, bool) {}
    virtual void SetCrowdRating(float) {}
    virtual void CodaSuccess();
    virtual void UnisonStart(int) {}
    virtual void UnisonEnd() {}
    virtual void UnisonSucceed() {}
    virtual EndingBonus *GetEndingBonus() { return nullptr; }
    virtual BandCrowdMeter *GetCrowdMeter() { return 0; }
    virtual void SetupApplauseMeter(
        int, const char *, const char *, RndDir *, RndDir *, bool, Symbol
    ) {}
    virtual void DisablePlayer(int, bool) {}
    virtual void EnablePlayer(int) {}
    virtual void FadeBotbBandNames(bool) {}
    virtual void CleanUpChordMeshes() {}
    virtual void SetApplauseMeterScale(int, int) {}
    virtual void StartPulseAnims(float) {}
    virtual float GetPulseAnimStartDelay(bool) const;
    virtual GemTrackResourceManager *GetGemTrackResourceManager() const { return 0; }
    // Retail TrackPanelDir vtable (file 0x2d464) has two more slots past
    // GetGemTrackResourceManager (0xd0) that the rb3-Wii dev oracle lacks:
    //   0xd4 -> 0x82303bb8  (reads this+0x374, shows 3-4 sub-objects)
    //   0xd8 -> 0x82309b60  (inline: this->0x378 = false)
    // TrackPanel::Reset() calls slot 0xd8 right before ConfigureTracks(false).
    virtual void Unkd4() {}
    virtual void Unkd8() {}

    bool Showing() {
        const char *name = "draw_order.grp";
        return Find<RndGroup>(name, true)->Showing();
    }

    void SetShowing(bool);
    void UpdateTrackSpeed();
    void UpdateJoinInProgress(bool, bool);
    void FailedJoinInProgress();
    bool ModifierActive(Symbol);
    void ToggleSurface();
    void ToggleNowbar();
    void SetPlayerLocal(BandTrack *);
    bool ReservedVocalPlayerSlot(int);
    BandTrack *GetBandTrackInSlot(int);

    DataNode DataForEachConfigObj(DataArray *);

    DECLARE_REVS;
    NEW_OVERLOAD;
    DELETE_OVERLOAD;

    float mViewTimeEasy; // 0x1d8
    float mViewTimeExpert; // 0x1dc
    float mNetTrackAlpha; // 0x1e0
    float mPulseOffset; // 0x1e4
    ObjPtr<Hmx::Object> mConfiguration; // 0x1e8
    ObjPtrList<RndTransformable> mConfigurableObjects; // 0x1f4
    std::vector<TrackInstrument> mInstruments; // 0x204
    ObjVector<ObjPtr<BandTrack> > mTracks; // 0x20c
    ObjVector<ObjPtr<RndDir> > mGemTracks; // 0x218  (GemTrackDir; RndDir keeps layout, avoids header tail)
    bool unk224; // 0x224
    TrackPanelInterface *mTrackPanel; // 0x228
    ObjPtr<RndDir> mApplauseMeter; // 0x22c
    RndDir *mBandLogoRival; // 0x238
    RndDir *mBandLogo; // 0x23c
    bool mPerformanceMode; // 0x240
    bool mDoubleSpeedActive; // 0x241
    bool mIndependentTrackSpeeds; // 0x242
};
