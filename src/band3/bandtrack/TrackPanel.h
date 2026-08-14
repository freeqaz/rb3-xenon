#pragma once
#include "bandobj/BandScoreboard.h"
#include "bandobj/TrackInstruments.h"
#include "bandobj/TrackPanelDirBase.h"
#include "bandobj/TrackPanelInterface.h"
#include "bandtrack/Track.h"
#include "game/BandUser.h"
#include "game/Player.h"
#include "obj/Data.h"

class DepChecker;

class TrackPanel : public TrackPanelInterface {
public:
    class TrackSlot {
    public:
        TrackSlot() : mTrack(0), mInstrument(kInstNone) {}

        Track *mTrack; // 0x0
        TrackInstrument mInstrument; // 0x4
    };

    enum TourGoalConfig {
        kConfigScoreStars = 0,
        kConfigScoreStarsGoal = 1,
        kConfigScoreGoal = 2,
        kConfigStarsGoal = 3,
        kConfigGoal = 4,
        kConfigInvalid = 5
    };

    TrackPanel();
    OBJ_CLASSNAME(TrackPanel);
    OBJ_SET_TYPE(TrackPanel);
    static Hmx::Object *NewObject();
    virtual DataNode Handle(DataArray *, bool);
    virtual ~TrackPanel();
    virtual void Draw();
    virtual void Enter();
    virtual void Exit();
    virtual void Poll();
    virtual void Unload();
    virtual void FinishLoad();
    virtual void GetTrackOrder(std::vector<TrackInstrument> *, bool) const;
    virtual int GetTrackCount() const;
    virtual int GetNumPlayers() const;
    virtual bool InGame() const;
    virtual bool IsGameOver() const;
    virtual int GetNoCrowdMeter() const; // fix ret type
    virtual int GetGameExcitement() const; // fix ret type
    virtual void PushCrowdReaction(bool);
    virtual bool ShowApplauseMeter() const { return false; }
    virtual float CrowdRatingDefaultVal(Symbol) const;
    virtual bool ShouldUpdateScrollSpeed() const;
    virtual bool SlotReservedForVocals(int) const;
    virtual bool GameResumedNoScore() const;
    virtual bool AutoVocals() const { return mAutoVocals; }

    void CleanUpReloadChecks();
    Track *GetTrack();
    Track *GetTrack(Player *, bool);
    Track *GetTrack(BandUser *, bool);
    const BandUser *GetUserFromTrackNum(int);
    void Reload();
    void CleanUpTracks();
    void UpdateReservedVocalSlot();
    void CreateTracks();
    void Reset();
    void AssignAndInitTracks();
    void SetMainGoalConfiguration(TourGoalConfig);
    void MainGoalReset();
    void SetSuppressTambourineDisplay(bool);
    void AssignTrack(int);
    void HandleAddUser(BandUser *);
    void HandleAddPlayer(Player *);
    void DoHandleAddPlayer(BandUser *);
    void PostHandleAddPlayer(Player *);
    void DoPostHandleAddPlayer(BandUser *);
    void HandleRemoveUser(BandUser *);
    void PostHandleRemoveUser(BandUser *);
    void PlaySequence(const char *, float, float, float);
    void StopSequence(const char *, bool);
    void ShowMainGoalInfo(bool);
    void SendTrackerDisplayMessage(const Message &) const;
    void SendTrackerBroadcastDisplayMessage(const Message &) const;
    void TrackerDisplayReset() const;
    void StartPulseAnims();
    void SetSmasherGlowing(int, bool);
    void PopSmasher(int);
    int GetTrackSlot(Player *);
    void UpdateJoinInProgress(bool, bool);
    void FailedJoinInProgress();
    void SetSuppressUnisonDisplay(bool);
    void UnisonStart(int);
    void UnisonPlayerSuccess(Player *);
    void UnisonPlayerFailure(Player *);
    void SetSuppressPlayerFeedback(bool);

    DataNode ForEachTrack(const DataArray *);

    // Layout re-rooted from retail ctor fn_82B61AD0 (vbase K=0xa8). Retail has a
    // leading bool at 0x3c (before mConfig) and 6 bools total, distributed at
    // 0x3c/0x6c/0x94/0x95/0xa0/0xa1 (all stb 0 in the ctor). Our old 8-bool block
    // packed at 0x68 was 2 phantoms over (unk5e: unused; unk61: our Poll-only
    // alternation, absent from retail Poll fn_82B60080). mReservedVocalSlot is the
    // "@0x5c = 2" member. Bool identities pinned from accessors: unk5c@0x3c
    // (Reset/Draw/Poll gate, fn_82B603B8/fn_82B5E978), unk5f@0x94 (StartPulseAnims
    // fn_82B5ED48), unk60@0x95 (MainGoalReset fn_82B5F8C8), unk62@0xa0
    // (SendTrackerBroadcast/Poll fn_82B5FA48/fn_82B60080), mAutoVocals@0xa1
    // (AutoVocals fn_82B60D50); unk5d@0x6c by elimination.
    bool unk5c; // 0x3c
    DataArray *mConfig; // 0x40
    std::vector<Track *> mTracks; // 0x44
    std::vector<TrackSlot> mTrackSlots; // 0x50
    int mReservedVocalSlot; // 0x5c
    ObjPtr<BandScoreboard> mScoreboard; // 0x60
    bool unk5d; // 0x6c
    std::map<Symbol, DepChecker *> mReloadChecks; // 0x70
    float mNextReloadTime; // 0x88
    TrackPanelDirBase *mTrackPanelDir; // 0x8c
    int unk84; // 0x90
    bool unk5f; // 0x94
    bool unk60; // 0x95
    TourGoalConfig mTourGoalConfig; // 0x98
    float mLastCrowdRating; // 0x9c
    bool unk62; // 0xa0
    bool mAutoVocals; // 0xa1
};

TrackPanel *GetTrackPanel();
TrackPanelDirBase *GetTrackPanelDir();

extern TrackPanel *TheTrackPanel;
