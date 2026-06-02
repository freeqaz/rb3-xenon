#pragma once
#include "bandobj/CrowdAudio.h"
#include "game/DirectInstrument.h"
#include "game/Game.h"
#include "game/GameConfig.h"
#include "game/HitTracker.h"
#include "game/Scoring.h"
#include "rndobj/Overlay.h"
#include "ui/UIPanel.h"
#include "obj/Msg.h"
#include "utl/DeJitter.h"
#include "utl/Profiler.h"

enum LoadingState {
    kLoadingState_NotReady = 0,
    kLoadingState_UILoaded = 1,
    kLoadingState_WorldLoaded = 2,
    kLoadingState_CharsLoaded = 3,
    kLoadingState_Ready = 4
};

class GamePanel : public UIPanel, public MsgSource {
public:
    GamePanel();
    OBJ_CLASSNAME(GamePanel);
    OBJ_SET_TYPE(GamePanel);
    static Hmx::Object *NewObject();
    virtual DataNode Handle(DataArray *, bool);
    virtual ~GamePanel();
    virtual void Enter();
    virtual void Exit();
    virtual void Poll();
    virtual void SetPaused(bool);
    virtual void Load();
    virtual void Unload();
    virtual bool IsLoaded() const;
    virtual void PollForLoading();
    virtual void FinishLoad();
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);

    void Reset();
    void CreateGame();
    void StartGame();
    void RunVocalTest();
    void SetPlayingTrackIntroUntil(float);
    void StartIntro();
    void SetExcitementLevel(ExcitementLevel);
    void UpdateNowBar();
    void SetDejitteredTime(float);
    void UpdateDeltaTimeOverlay();
    void UpdateLatency();
    void PlayBandDiedCue();
    void SendRestartGameNetMsg(bool);
    void SendResumeNoScoreGameNetMsg(float);
    void ToggleInstrumentSynth();
    void ClearDrawGlitch();
    GameState GetGameState() const { return mGameState; }
    bool IsGameOver() const { return mGameState == kGameOver; }
    void SetGameOver() { mGameState = kGameOver; }
    DirectInstrument *GetDirectInstrument() const { return mDirectInstrument; }
    HitTracker *GetHitTracker() const { return mHitTracker; }

    DataNode OnStartLoadSong(DataArray *);

    // RB3-360 retail layout, reverse-engineered from the ctor (target
    // fn_82677FE8): UIPanel base = 0x40, MsgSource MI base ctor @0x3c, shared
    // Hmx::Object vbase @0x15c. The retail ctor does NOT construct the
    // time/latency/delta_time RndOverlays nor the unk64..unk70 debug scratch
    // floats — those are debug-only HUD instrumentation the shipping build
    // stripped (cf. the globally no-op'd MILO_WARN/LOG family). Keeping them as
    // real members pushed every field past mGame +0x1c (mConfig read 0x98 vs
    // target 0x7c) and, combined with DeJitter's dc3-newer inline 0x80-byte
    // history array, drove mDirectInstrument to 0x1dc vs the target's 0x150.
    // Each offset below is verified against a ctor store.
    Game *mGame; // 0x54   (ctor puVar1[0x15])
    ObjDirPtr<ObjectDir> mVocalPercussionBank; // 0x58 (ObjDirPtr vtable @0x58)
    ObjDirPtr<ObjectDir> mDrumKitBank;         // 0x64 (ObjDirPtr vtable @0x64)
    bool mStartPaused; // 0x70 (ctor byte @0x70)
    GameState mGameState; // 0x74 (Reset/ctor word @0x74 = kGameNeedIntro)
    bool mMultiEvent; // 0x78 (ctor byte @0x78)
    GameConfig mConfig; // 0x7c (ctor GameConfig ctor @0x7c, SetName("gamecfg"))
    Scoring *mScoring; // 0xc0 (ctor new Scoring(0xd0) -> puVar1[0x30])
    Profiler mLoadProf; // 0xc8 (ctor Profiler("game_panel_load") @0x32)
    ExcitementLevel mExcitement; // 0x118 (ctor puVar1[0x46]=kNumExcitements)
    ExcitementLevel mLastExcitement; // 0x11c (ctor puVar1[0x47])
    bool unk130; // 0x120
    bool mReplay; // 0x121 (ctor byte @0x121)
    DeJitter mDeJitter; // 0x124 (ctor DeJitter ctor @0x49 = 0x124, size 0x1c)
    bool unk150; // 0x140 (ctor byte @0x50 = 0x140)
    bool unk151; // 0x141 (ctor byte @0x141)
    float unk154; // 0x144 (ctor puVar1[0x51])
    LoadingState mLoadingState; // 0x148 (ctor puVar1[0x52])
    HitTracker *mHitTracker; // 0x14c (ctor new HitTracker(0x408) -> puVar1[0x53])
    DirectInstrument *mDirectInstrument; // 0x150 (ctor new DirectInstrument(0x18) -> puVar1[0x54])

#ifdef MILO_DEBUG
    // Debug HUD overlays + dejitter scratch — present only in debug builds.
    RndOverlay *mTime;
    RndOverlay *mLatency;
    RndOverlay *mDeltaTime;
    bool unk64;
    float unk68;
    float unk6c;
    float unk70;
#endif
};

class LatencyCallback : public RndOverlay::Callback {
public:
    LatencyCallback() {}
    virtual ~LatencyCallback() {}
    virtual float UpdateOverlay(RndOverlay *, float);
    bool unk4;
};

extern GamePanel *TheGamePanel;