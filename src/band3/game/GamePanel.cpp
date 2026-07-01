#include "macros.h"
#undef MILO_DEBUG
#include "game/GamePanel.h"
#include "GamePanel.h"
#include "bandobj/BandDirector.h"
#include "bandobj/BandWardrobe.h"
#include "bandobj/CrowdAudio.h"
#include "bandtrack/TrackPanel.h"
#include "beatmatch/TrackType.h"
#include "game/BandUserMgr.h"
#include "game/DirectInstrument.h"
#include "game/Game.h"
#include "game/GameMode.h"
#include "game/HitTracker.h"
#include "game/NetGameMsgs.h"
#include "game/Scoring.h"
#include "game/SongDB.h"
#include "meta/HAQManager.h"
#include "meta/PreloadPanel.h"
#include "meta_band/BandSongMetadata.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/MetaPerformer.h"
#include "net/NetSession.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/DirLoader.h"
#include "obj/Msg.h"
#include "obj/ObjMacros.h"
#include "obj/PropSync_p.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "os/Joypad.h"
#include "os/PlatformMgr.h"
#include "os/System.h"
#include "os/Timer.h"
#include "rndobj/Overlay.h"
#include "rndobj/PostProc.h"
#include "rndobj/Rnd.h"
#include "synth/Synth.h"
#include "ui/UI.h"
#include "ui/UIPanel.h"
#include "utl/Loader.h"
#include "utl/MBT.h"
#include "utl/Messages.h"
#include "utl/Messages2.h"
#include "utl/Messages3.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"
#include <algorithm>

GamePanel *TheGamePanel;
LatencyCallback gGamePanelCallback;

GamePanel::GamePanel()
    : mGame(0),
#ifdef MILO_DEBUG
      mTime(RndOverlay::Find("time", true)),
      mLatency(RndOverlay::Find("latency", true)),
      mDeltaTime(RndOverlay::Find("delta_time", true)), unk64(1), unk68(0), unk6c(0),
      unk70(0),
#endif
      mStartPaused(0), mGameState(kGameNeedIntro), mMultiEvent(0), mScoring(0),
      mLoadProf("game_panel_load", 1), mExcitement(kNumExcitements),
      mLastExcitement(kNumExcitements), mReplay(0), unk150(1), unk151(0), unk154(0),
      mLoadingState(kLoadingState_NotReady), mHitTracker(new HitTracker()),
      mDirectInstrument(new DirectInstrument()) {
    MILO_ASSERT(!TheGamePanel, 0x6B);
    TheGamePanel = this;
    mScoring = new Scoring();
    mConfig.SetName("gamecfg", ObjectDir::Main());
}

GamePanel::~GamePanel() {
    TheGamePanel = nullptr;
    RELEASE(mHitTracker);
    RELEASE(mDirectInstrument);
    RELEASE(mScoring);
}

void GamePanel::Reset() {
    mGame->Reset();
    mGameState = kGameNeedIntro;
    mExcitement = kNumExcitements;
    mLastExcitement = kNumExcitements;
    Export(reset_msg, true);
    mGame->Restart(true);
    unk151 = false;
}

void GamePanel::Load() {
    mReplay = false;
    mLoadProf.Start();
    BandSongMetadata *data = (BandSongMetadata *)TheSongMgr.Data(
        TheSongMgr.GetSongIDFromShortName(MetaPerformer::Current()->Song(), true)
    );
    mVocalPercussionBank.LoadFile(
        FilePath(".", data->VocalPercussionBank().Str()), true, true, kLoadBack, false
    );

    const char *drumbankStr = data->DrumKitBank().Str();
    if (!drumbankStr || *drumbankStr == '\0') {
        drumbankStr = SystemConfig("sound", "banks", "kit")->Str(1);
    }
    mDrumKitBank.LoadFile(FilePath(".", drumbankStr), true, true, kLoadBack, false);
    UIPanel::Load();
    Symbol playModeSym;
    std::vector<BandUser *> users;
    TheBandUserMgr->GetParticipatingBandUsers(users);
    int i6 = data->HasBass();
    int i7 = data->HasGuitar();
    int i8 = data->HasKeys();
    FOREACH (it, users) {
        switch ((*it)->GetTrackType()) {
        case kTrackKeys:
        case kTrackRealKeys:
            i8 = 2;
            break;
        case kTrackBass:
        case kTrackRealBass:
            i6 = 2;
            break;
        case kTrackGuitar:
        case kTrackRealGuitar:
            i7 = 2;
            break;
        default:
            break;
        }
    }
    if (!data->HasKeys())
        i8 = 0;
    if (!data->HasBass())
        i6 = 0;
    if (!data->HasGuitar())
        i7 = 0;
    int i50 = -1;
    if (MaxEq(i50, i7 + i6)) {
        playModeSym = coop_bg;
    }
    if (MaxEq(i50, i7 + i8)) {
        playModeSym = coop_gk;
    }
    if (MaxEq(i50, i6 + i8)) {
        playModeSym = coop_bk;
    }
    TheBandWardrobe->SetPlayMode(playModeSym, nullptr);
}

void GamePanel::CreateGame() {
#ifdef HX_NATIVE
    if (getenv("GAME_DBG"))
        MILO_LOG("GAME_DBG: *** GamePanel::CreateGame() — about to new Game() ***\n");
#endif
    RELEASE(mGame);
    mGame = new Game();
    mGame->mDisablePauseMs = Property(disable_pause_ms, true)->Float();
}

void GamePanel::PollForLoading() {
    mLoadingState = kLoadingState_NotReady;
    UIPanel::PollForLoading();
#ifdef HX_NATIVE
    if (getenv("GAME_DBG")) {
        static int gp_spam = 0;
        if ((gp_spam++ % 60) == 0)
            MILO_LOG("GAME_DBG: GamePanel::PollForLoading uiLoaded=%d transScreen=%p "
                     "bandDir=%p readyMidi=%d loadChars=%d allCharsLoaded=%d\n",
                     UIPanel::IsLoaded(), (void *)TheUI.TransitionScreen(),
                     (void *)TheBandDirector,
                     TheBandDirector ? TheBandDirector->ReadyForMidiParsers() : -1,
                     TheGameMode->Property("load_chars", true)->Int(),
                     (TheBandWardrobe ? TheBandWardrobe->AllCharsLoaded() : -1));
    }
#endif
    if (UIPanel::IsLoaded()) {
        mLoadingState = kLoadingState_UILoaded;
        if (TheUI->TransitionScreen()) {
            if (TheUI->TransitionScreen()->HasPanel(
                    ObjectDir::Main()->Find<UIPanel>("world_panel", true)
                )) {
                if (!TheBandDirector || !TheBandDirector->ReadyForMidiParsers())
                    return;
            }
        }
        mLoadingState = kLoadingState_WorldLoaded;
        if (TheGameMode->Property("load_chars", true)->Int()
            && !(TheBandWardrobe && TheBandWardrobe->AllCharsLoaded()))
            return;
        else {
            mLoadingState = kLoadingState_CharsLoaded;
            if (!mGame)
                CreateGame();
            if (mGame->IsReady()) {
                mLoadingState = kLoadingState_Ready;
            }
        }
    }
}

bool GamePanel::IsLoaded() const {
#ifdef HX_NATIVE
    // K6 diagnostic — only fires when at least one gate changes.
    if (getenv("GAME_DBG")) {
        static int sLastReportedState = -1;
        int uiL = UIPanel::IsLoaded() ? 1 : 0;
        int vpb = mVocalPercussionBank.IsLoaded() ? 1 : 0;
        int dkb = mDrumKitBank.IsLoaded() ? 1 : 0;
        int dir = (mDirectInstrument && mDirectInstrument->IsLoaded()) ? 1 : 0;
        int compositeState = (uiL << 3) | (vpb << 2) | (dkb << 1) | dir | (mLoadingState << 4);
        if (compositeState != sLastReportedState) {
            MILO_LOG("GAME_DBG: GamePanel::IsLoaded uiL=%d vpb=%d dkb=%d dir=%d state=%d\n",
                     uiL, vpb, dkb, dir, (int)mLoadingState);
            sLastReportedState = compositeState;
        }
    }
#endif
    if (!UIPanel::IsLoaded())
        return false;
    if (!mVocalPercussionBank.IsLoaded())
        return false;
    if (!mDrumKitBank.IsLoaded())
        return false;
    if (!mDirectInstrument->IsLoaded())
        return false;
    return mLoadingState == kLoadingState_Ready;
}

void GamePanel::Unload() {
    UIPanel::Unload();
    RELEASE(mVocalPercussionBank);
    RELEASE(mDrumKitBank);
    RELEASE(mGame);
    mLoadingState = kLoadingState_NotReady;
}

void GamePanel::Enter() {
    TheTaskMgr.ClearTasks();
    UIPanel::Enter();
    mLoadProf.Stop();
    Reset();
    mGame->SetPaused(false, true, true);
    ThePlatformMgr.SetNotifyUILocation((NotifyLocation)0);
    if (TheRnd.GetAspect() != Rnd::kWidescreen && !TheGame->mProperties.mLetterbox) {
        TheRnd.SetAspect(Rnd::kRegular);
        TrackPanelDirBase *trackPanelDir = GetTrackPanelDir();
        MILO_ASSERT(trackPanelDir, 0x155);
        RndCam *cam = trackPanelDir->Cam();
        MILO_ASSERT(cam, 0x158);
        cam->UpdateLocal();
    }
}

void GamePanel::Exit() {
    if (!mMultiEvent) {
        TheTaskMgr.ClearTimelineTasks(kTaskSeconds);
        TheTaskMgr.ClearTimelineTasks(kTaskBeats);
        TheTaskMgr.ClearTimelineTasks(kTaskTutorialSeconds);
    }
    UIPanel::Exit();
    mReplay = true;
    ThePlatformMgr.SetNotifyUILocation((NotifyLocation)1);
    if (TheRnd.GetAspect() != Rnd::kWidescreen && !TheGame->mProperties.mLetterbox) {
        TheRnd.SetAspect(Rnd::kLetterbox);
    }
}

void GamePanel::StartGame() {
    AutoTimer::SetCollectStats(true, TheRnd.VerboseTimers());
    Export(on_extend_msg, true);
    if (mGame->HasIntro())
        mGame->Start();
    mGameState = kGamePlaying;
#ifdef HX_NATIVE
    // V25 (salvage V33 re-apply): the HUD master group `draw_order.grp` is
    // hidden during the cinematic intro by `TrackPanelDir::Reset() ->
    // SetShowing(false)`; retail then re-shows it via the milo `play_intro`
    // message -> `PlayIntro() -> SetShowing(!mPerformanceMode)` at the end of
    // the intro. Natively that play_intro fires inconsistently / late (its
    // intro camshot/anim timeline is the same proxy/anim machinery the venue
    // work is still bringing up), so the score plate + star meter can stay
    // hidden for the first seconds of play and in some runs never come on.
    //
    // Force-show the HUD here at the song-start point: StartGame() is invoked
    // from GamePanel::Poll the moment the song clock crosses 0, exactly the
    // retail "song begins -> HUD appears" moment. Idempotent vs play_intro.
    if (TrackPanelDirBase *tpd = GetTrackPanelDir())
        tpd->SetShowing(true);
#endif
    if (DataVariable("vocal_test").Int()) {
        RunVocalTest();
    }
}

void GamePanel::RunVocalTest() {
    float f1 = 0;
    float durms = TheSongDB->GetSongDurationMs();
    unk150 = false;
    while (f1 < durms) {
        TheTaskMgr.SetSeconds(f1 / 1000.0f, false);
        f1 += 16.0f;
        mGame->Poll();
    }
    TheDebug.Exit(0, true);
}

void GamePanel::Poll() {
    START_AUTO_TIMER("game_poll");
#ifdef HX_NATIVE
    // K6 diagnostic — fires only on mGameState change (no per-frame spam). The
    // K6 audio-load chain now reaches Game::mLoadState=kReady, but the screen
    // transition (tv3_b/tv3_c vignette) keeps GamePanel from going Active in
    // headless, so this Poll() never runs in the V1 reproducer. Tracking the
    // gate is what got us to K6 ack; next stop is the InterstitialPanel /
    // BandScreen camshot-exit gate (see InterstitialPanel.cpp HX_NATIVE block).
    if (getenv("GAME_DBG")) {
        static int sLastState = -1;
        int istate = mGameState;
        if (istate != sLastState) {
            MILO_LOG("GAME_DBG: GamePanel::Poll state=%d IsLoaded=%d audio_fail=%d\n",
                     istate, (int)IsLoaded(),
                     mGame ? (int)mGame->mMaster->GetAudio()->Fail() : -1);
            sLastState = istate;
        }
    }
#endif
    if (!IsLoaded()) return;
    UIPanel::Poll();
    if (mGame->mMaster->GetAudio()->Fail()) return;
    if (mGameState == kGameNeedIntro) {
        StartIntro();
    }
    if (TheGame->mProperties.mCrowdReacts) {
        SetExcitementLevel(mGame->GetCrowdExcitement());
    }
    mGame->Poll();
    if (mGameState == kGameNeedStart && !mGame->IsWaiting()
        && (TheTaskMgr.Seconds(TaskMgr::kRealTime) > -0.025f
            || TheGame->mProperties.mIsPractice)) {
        StartGame();
    }
    if (unk151 && 1000.0f * TheTaskMgr.Seconds(TaskMgr::kRealTime) > unk154) {
        unk151 = false;
    }
#ifdef MILO_DEBUG
    if (mTime->Showing()) {
        UpdateNowBar();
    }
    if (mDeltaTime->Showing()) {
        UpdateDeltaTimeOverlay();
    }
    UpdateLatency();
#endif
}

void GamePanel::SetPaused(bool b1) {
    MILO_WARN(
        "Should not set game panel paused! Use SetGamePaused(bool) or {game set_game_paused <bool> instead!"
    );
    mPaused = b1;
}

void GamePanel::SetPlayingTrackIntroUntil(float f1) {
    if (!unk151) {
        unk151 = true;
        unk154 = f1;
    } else if (unk154 < f1) {
        unk154 = f1;
    }
}

void GamePanel::StartIntro() {
    mGameState = kGameNeedStart;
    HandleType(pick_intro_msg);
    if (mStartPaused) {
        mGame->SetPaused(true, true, true);
    }
    SetExcitementLevel(mGame->GetCrowdExcitement());
    mGame->StartIntro();
}

#ifdef MILO_DEBUG
void GamePanel::UpdateNowBar() {
    MILO_ASSERT(mGame, 0x212);
    TaskMgr& tm = TheTaskMgr;
    float t1 = tm.Seconds(TaskMgr::kRealTime);
    float t2 = TheSongDB->GetSongDurationMs();
    float time = t2 / 1000 - t1;
    char surprise_tool = '-';
    if (time < 0) {
        time = -time;
        surprise_tool = '+';
    }
    float totalTick = tm.GetSongPos().GetTotalTick();
    float val = 0.0f;
    if (t2 > 0.0f) {
        val = std::min(100.0f * t1 / (0.001f * t2), 100.0f);
    }
    *mTime << MakeString(
        "MBT %d:%d:%03d [%s %c%s %4.1f%%] (%.2fsec %dtk)\n",
        tm.GetSongPos().GetMeasure() + 1,
        tm.GetSongPos().GetBeat() + 1,
        tm.GetSongPos().GetTick(),
        FormatTimeMSH(t1 * 1000),
        surprise_tool,
        FormatTimeMSH(time * 1000),
        val,
        t1,
        (int)totalTick
    );
}

#pragma push
#pragma pool_data off
void GamePanel::UpdateLatency() {
    static DataNode &latency_test = DataVariable("latency_test");
    static DataNode &pad_button = DataVariable("pad_button");
    if (latency_test.Int() == 0) {
        mLatency->SetShowing(false);
        return;
    }
    bool bFlash = 0;
    bool bJustPressed = 0;
    int joyNum = latency_test.Int() - 2;
    if (joyNum == -1) {
        static int sFlashCnt = 0;
        float delta = TheTaskMgr.DeltaBeat();
        float prevFloor = std::floor((TheTaskMgr.Beat() - delta) * 2.0f);
        float curFloor = std::floor(TheTaskMgr.Beat() * 2.0f);
        if (curFloor != prevFloor) {
            sFlashCnt = 3;
        }
        if (sFlashCnt > 0) {
            bFlash = 1;
            sFlashCnt--;
        }
    } else {
        static bool sLastBtn = false;
        JoypadData *pad = JoypadGetPadData(joyNum);
        if (pad != nullptr) {
            int bit = pad_button.Int();
            bool pressed = (pad->mButtons & (1 << bit)) != 0;
            bFlash = pressed;
            bJustPressed = pressed && !sLastBtn;
            sLastBtn = pressed;
        }
    }
    gGamePanelCallback.unk4 = bFlash;
    if (bJustPressed) {
        static Hmx::Object *sBeep = nullptr;
        if (sBeep == nullptr) {
            ObjectDir *dir;
            {
                FilePath path("test/latency.milo");
                dir = DirLoader::LoadObjects(path, nullptr, nullptr);
            }
            sBeep = dir->Find<Hmx::Object>("beep.cue", true);
        }
        sBeep->Handle(play_msg, true);
    }
    static int sToggle = 0;
    static float sMs[2] = {0, 0};
    sMs[sToggle] = TheRnd.DrawMs();
    sToggle = 1 - sToggle;
    mLatency->SetShowing(true);
    mLatency->Clear();
    *mLatency << MakeString("Joy %d Beat %.3f\nms %.2f last %.2f", joyNum, TheTaskMgr.Beat(), sMs[0], sMs[1]);
    mLatency->SetCallback(&gGamePanelCallback);
}
#pragma pop

void GamePanel::SetDejitteredTime(float f1) {
    unk6c = unk70;
    unk70 = f1 - unk68;
    unk68 = f1;
}

void GamePanel::UpdateDeltaTimeOverlay() {
    float t = unk70;
    *mDeltaTime << MakeString("dt: %5.1f, ddt: %5.1f\n", t, t - unk6c);
}
#endif // MILO_DEBUG

void GamePanel::FinishLoad() {
    UIPanel::FinishLoad();
    mVocalPercussionBank.PostLoad(nullptr);
    mGame->SetVocalPercussionBank(mVocalPercussionBank);
    mDrumKitBank.PostLoad(nullptr);
    mGame->SetDrumKitBank(mDrumKitBank);
    mDirectInstrument->PostLoad();
    PreloadPanel::sCache->Clear();
    HAQManager::PrintSongInfo(
        MetaPerformer::Current()->Song(), TheSongDB->GetSongDurationMs()
    );
#ifdef MILO_DEBUG
    if (unk64)
        unk64 = false;
#endif
}

void GamePanel::PlayBandDiedCue() {
    String cue;
    TheSongDB->GetBandFailCue(cue);
    if (cue == "") {
        TheSynth->Play("band_fail_rock.cue", 0, 0, 0);
    } else {
        TheSynth->Play(cue.c_str(), 0, 0, 0);
    }
}

void GamePanel::ClearDrawGlitch() {
    TheGameMode->Property(game_screen, true)->Obj<UIScreen>()->SetShowing(false);
    RndPostProc::Reset();
    TheRnd.ForceColorClear();
    // X360 retail uses the dx9 renderer's buffered-frame count here; rndwii's
    // TheWiiRnd.mFramesBuffered does not exist on 360. ClearDrawGlitch() is out
    // of the pinned matching span, so a plausible double-buffer count compiles.
    for (int i = 0; i < 2; i++) {
        TheRnd.BeginDrawing();
        TheRnd.EndWorld();
        TheUI->Draw();
        TheRnd.EndDrawing();
    }
}

void GamePanel::SendRestartGameNetMsg(bool b1) {
    RestartGameMsg msg(b1);
    TheNetSession->SendMsgToAll(msg, kReliable);
}

void GamePanel::SendResumeNoScoreGameNetMsg(float f1) {
    ResumeNoScoreGameMsg msg(f1);
    TheNetSession->SendMsgToAll(msg, kReliable);
}

void GamePanel::SetExcitementLevel(ExcitementLevel e) {
    if (mExcitement != e) {
        mLastExcitement = mExcitement;
        mExcitement = e;
        static Message msg("excitement", 0);
        msg[0] = e;
        Export(msg, true);
    }
}

void GamePanel::ToggleInstrumentSynth() {
    if (mDirectInstrument->Enabled()) {
        mDirectInstrument->Disable();
    } else
        mDirectInstrument->Enable();
}

DataNode GamePanel::OnStartLoadSong(DataArray *a) {
    Symbol s = a->ForceSym(2);
    std::vector<Symbol> syms;
    syms.push_back(s);
    MetaPerformer::Current()->SetSongs(syms);
    CreateGame();
    return 0;
}

BEGIN_HANDLERS(GamePanel)
    HANDLE_ACTION(set_start_paused, mStartPaused = _msg->Int(2))
    HANDLE_EXPR(lost, mGameState == kGameOver && mGame->mResult == kLost)
    HANDLE_EXPR(in_intro, mGameState == kGameNeedStart)
    HANDLE_EXPR(is_game_over, mGameState == kGameOver)
    HANDLE_EXPR(is_playing, mGameState == kGamePlaying)
    HANDLE_ACTION(start_game, StartGame())
    HANDLE(start_load_song, OnStartLoadSong)
    HANDLE_ACTION(send_restart_game_net_msg, SendRestartGameNetMsg(_msg->Int(2)))
    HANDLE_ACTION(
        send_resume_no_score_game_net_msg, SendResumeNoScoreGameNetMsg(_msg->Float(2))
    )
    HANDLE_ACTION(clear_draw_glitch, ClearDrawGlitch())
    HANDLE_ACTION(play_band_died_cue, PlayBandDiedCue())
    HANDLE_ACTION(print_hit_stats, mHitTracker->PrintStats())
    HANDLE_ACTION(clear_hit_stats, mHitTracker->Reset())
    HANDLE_ACTION(toggle_instrument_synth, ToggleInstrumentSynth())
    HANDLE_EXPR(is_instrument_synth_on, mDirectInstrument->Enabled())
    HANDLE_ACTION(set_instrument_synth_volume, mDirectInstrument->SetVolume(_msg->Int(2)))
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_MEMBER_PTR((&mConfig))
    HANDLE_MEMBER_PTR(mGame)
    HANDLE_SUPERCLASS(MsgSource)
    HANDLE_CHECK(0x352)
END_HANDLERS

#pragma push
#pragma pool_data off
BEGIN_PROPSYNCS(GamePanel)
    SYNC_PROP(multi_event, mMultiEvent)

    {
        static Symbol _s("excitement");
        if (sym == _s && _op & kPropGet)
            return PropSync((int &)mExcitement, _val, _prop, _i + 1, _op);
    }
    {
        static Symbol _s("last_excitement");
        if (sym == _s && _op & kPropGet)
            return PropSync((int &)mLastExcitement, _val, _prop, _i + 1, _op);
    }
    {
        static Symbol _s("replay");
        if (sym == _s && _op & kPropGet)
            return PropSync(mReplay, _val, _prop, _i + 1, _op);
    }

    if (&mConfig && mConfig.SyncProperty(_val, _prop, _i, _op)) {
        return true;
    } else
        return false;
END_PROPSYNCS
#pragma pop

float LatencyCallback::UpdateOverlay(RndOverlay *, float f) {
    Hmx::Color color = unk4 ? Hmx::Color(1, 1, 1) : Hmx::Color(0, 0, 0);
    TheRnd.DrawRectScreen(
        Hmx::Rect(0, 0.875f, 0.125f, 0.125f), color, nullptr, nullptr, nullptr
    );
    TheRnd.DrawRectScreen(
        Hmx::Rect(0, 0.125f, 0.125f, 0.125f), color, nullptr, nullptr, nullptr
    );
    return f;
}