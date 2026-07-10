#include "game/Game.h"
#include "Game.h"
#include "bandobj/BandDirector.h"
#include "bandobj/CrowdAudio.h"
#include "bandobj/TrackPanelDirBase.h"
#include "bandtrack/TrackPanel.h"
#include "beatmatch/BeatMaster.h"
#include "beatmatch/PlayerTrackConfig.h"
#include "beatmatch/SongData.h"
#include "beatmatch/TrackType.h"
#include "beatmatch/VocalNote.h"
#include "decomp.h"
#include "game/Band.h"
#include "game/BandUser.h"
#include "game/BandUserMgr.h"
#include "game/FadePanel.h"
#include "game/GameConfig.h"
#include "game/GameMode.h"
#include "game/GamePanel.h"
#include "game/SyncGameStartPanel.h"
#include "game/GemPlayer.h"
#include "game/VocalPlayer.h"
#include "game/RealGuitarGemPlayer.h"
#include "game/NetGameMsgs.h"
#include "game/Player.h"
#include "game/Scoring.h"
#include "game/Shuttle.h"
#include "game/SongDB.h"
#include "game/TrackerManager.h"
#include "game/GemTrainerPanel.h"
#include "game/PracticePanel.h"
#include "game/ChordbookPanel.h"
#include "game/VocalTrainerPanel.h"
#include "game/FreestylePanel.h"
#include "game/RGTrainerPanel.h"
#include "game/RKTrainerPanel.h"
#include "game/TrainerPanel.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/BandUI.h"
#include "meta_band/MetaPerformer.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/MusicLibrary.h"
#include "meta_band/ModifierMgr.h"
#include "meta_band/SessionMgr.h"
#include "meta_band/OvershellPanel.h"
#include "midi/MidiParser.h"
#include "midi/MidiParserMgr.h"
#include "net/NetMessage.h"
#include "net/NetSession.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"
#include "os/System.h"
#include "os/File.h"
#include "rndobj/Rnd.h"
#include "synth/Synth.h"
#include "ui/UI.h"
#include "ui/UIPanel.h"
#include "world/Dir.h"
#include "ui/UIScreen.h"
#include "utl/MBT.h"
#include "utl/Messages.h"
#include "utl/SongInfoCopy.h"
#include "utl/SongPos.h"
#include "utl/Symbols.h"
#include "utl/TempoMap.h"
#include "utl/TimeConversion.h"

class TrainerPanel;
extern TrainerPanel *TheTrainerPanel;

Game *TheGame;
bool gDebugFullQuota;
bool gKickAutoplay;

NetMessage *PlayerGameplayMsg::NewNetMessage() { return new PlayerGameplayMsg(); }
NetMessage *RestartGameMsg::NewNetMessage() { return new RestartGameMsg(); }
NetMessage *ResumeNoScoreGameMsg::NewNetMessage() { return new ResumeNoScoreGameMsg(); }
NetMessage *PlayerStatsMsg::NewNetMessage() { return new PlayerStatsMsg(); }
NetMessage *SetUserTrackTypeMsg::NewNetMessage() { return new SetUserTrackTypeMsg(); }
NetMessage *SetUserDifficultyMsg::NewNetMessage() { return new SetUserDifficultyMsg(); }
NetMessage *SetlistSubmissionMsg::NewNetMessage() { return new SetlistSubmissionMsg(); }
NetMessage *TourMostStarsMsg::NewNetMessage() { return new TourMostStarsMsg(); }
NetMessage *TourPlayedMsg::NewNetMessage() { return new TourPlayedMsg(); }
NetMessage *AccomplishmentMsg::NewNetMessage() { return new AccomplishmentMsg(); }
NetMessage *AccomplishmentEarnedMsg::NewNetMessage() {
    return new AccomplishmentEarnedMsg();
}
NetMessage *SetPartyShuffleModeMsg::NewNetMessage() {
    return new SetPartyShuffleModeMsg();
}
NetMessage *TourHideShowFiltersMsg::NewNetMessage() {
    return new TourHideShowFiltersMsg();
}
NetMessage *SongResultsScrollMsg::NewNetMessage() { return new SongResultsScrollMsg(); }
NetMessage *SetUpMicsMsg::NewNetMessage() { return new SetUpMicsMsg(); }

class MusicLibraryTaskMsg : public NetMessage {
public:
    MusicLibraryTaskMsg() {}
    virtual ~MusicLibraryTaskMsg() {}
    virtual void Save(BinStream &) const;
    virtual void Load(BinStream &);
    virtual void Dispatch();
    NETMSG_BYTECODE(MusicLibraryTaskMsg);
    NETMSG_NAME(MusicLibraryTaskMsg);
    NETMSG_NEWNETMSG(MusicLibraryTaskMsg);
    MusicLibrary::MusicLibraryTask mTask; // 0x4
};

NetMessage *MusicLibraryTaskMsg::NewNetMessage() { return new MusicLibraryTaskMsg(); }

Hmx::Object *GamePanel::NewObject() { return new GamePanel(); }
Hmx::Object *SyncGameStartPanel::NewObject() { return new SyncGameStartPanel(); }
Hmx::Object *TrackPanel::NewObject() { return new TrackPanel(); }
Hmx::Object *GemTrainerPanel::NewObject() { return new GemTrainerPanel(); }
Hmx::Object *GemTrainerLoopPanel::NewObject() { return new GemTrainerLoopPanel(); }
Hmx::Object *RGTrainerPanel::NewObject() { return new RGTrainerPanel(); }
Hmx::Object *RKTrainerPanel::NewObject() { return new RKTrainerPanel(); }
Hmx::Object *PracticePanel::NewObject() { return new PracticePanel(); }
Hmx::Object *ChordbookPanel::NewObject() { return new ChordbookPanel(); }
Hmx::Object *VocalTrainerPanel::NewObject() { return new VocalTrainerPanel(); }
Hmx::Object *FreestylePanel::NewObject() { return new FreestylePanel(); }

void GameInit() {
    FadePanel::Init();
    REGISTER_OBJ_FACTORY(GamePanel)
    REGISTER_OBJ_FACTORY(SyncGameStartPanel)
    REGISTER_OBJ_FACTORY(TrackPanel)
    PlayerGameplayMsg::Register();
    RestartGameMsg::Register();
    ResumeNoScoreGameMsg::Register();
    PlayerStatsMsg::Register();
    SetUserTrackTypeMsg::Register();
    SetUserDifficultyMsg::Register();
    SetlistSubmissionMsg::Register();
    TourMostStarsMsg::Register();
    TourPlayedMsg::Register();
    AccomplishmentMsg::Register();
    AccomplishmentEarnedMsg::Register();
    SetPartyShuffleModeMsg::Register();
    MusicLibraryTaskMsg::Register();
    SetUpMicsMsg::Register();
    TourHideShowFiltersMsg::Register();
    SongResultsScrollMsg::Register();
    REGISTER_OBJ_FACTORY(GemTrainerPanel)
    REGISTER_OBJ_FACTORY(GemTrainerLoopPanel)
    REGISTER_OBJ_FACTORY(RGTrainerPanel)
    REGISTER_OBJ_FACTORY(RKTrainerPanel)
    REGISTER_OBJ_FACTORY(PracticePanel)
    REGISTER_OBJ_FACTORY(ChordbookPanel)
    TrainerChallenge::Init();
    REGISTER_OBJ_FACTORY(VocalTrainerPanel)
    REGISTER_OBJ_FACTORY(FreestylePanel)
    TheDebug.AddExitCallback(GameTerminate);
}

void GameTerminate() { TheSongMgr.Terminate(); }

Game::Game()
    : mSongDB(new SongDB()), mSongInfo(0), mIsPaused(0), mGameWantsPause(0),
      mOvershellWantsPause(0), unk6b(0), unk6c(0), mPauseTime(0), mRealtime(0), unk6f(0),
      mTimeOffset(0), mLastPollMs(0), mMuckWithPitch(0), mMusicSpeed(1.0f),
      mNeverAllowInput(0), unkb9(1), mDemoMaxPctComplete(0), mDemoMaxMs(0), unkc4(0),
      mLoadState(kLoadingSong), mResult(kRestart), mBand(0), mShuttle(new Shuttle()),
      unkdc(-1), unk11c(-1), unk120(0), mSkippedSong(0), unk124(0), mResumeTime(0),
      mInvalidScore(0), unk130(0), unk134(0), unk138(0), mDrumFillsMod(1), unk13c(-1),
      unk140(-1), mTrackerManager(0), unk148(0), mDisablePauseMs(-1), unk150(1) {
    MILO_ASSERT(!TheSongDB, 0xCE);
    SongDB * &_ref0 = mSongDB;
    TheSongDB = _ref0;
    TheGame = this;
    SetName("beatmatch", ObjectDir::Main());
    mMaster = new BeatMaster(_ref0->GetData(), TheBandUserMgr->GetNumParticipants());
    mMaster->RegisterSink(*this);
    TheNetSession->AddSink(this, GameEndedMsg::Type());
    TheSessionMgr->AddSink(this, LocalUserLeftMsg::Type());
    TheSessionMgr->AddSink(this, RemoteUserLeftMsg::Type());
    TheSessionMgr->AddSink(this, RemoteLeaderLeftMsg::Type());
    OvershellPanel *overshell = TheBandUI.mOvershell;
    overshell->AddSink(this, "required_song_options_chosen");
    TheBandUI.mOvershell->AddSink(this, NewOvershellLocalUserMsg::Type());
    TheBandUI.AddSink(this, UIScreenChangeMsg::Type());

    SetBackgroundVolume(TheProfileMgr.GetBackgroundVolumeDb());
    SetForegroundVolume(TheProfileMgr.GetForegroundVolumeDb());
    SetStereo(true);
    TheGameConfig->AssignTracks();
    for (int i = 0; i != (uint)TheSynth->GetNumMics(); i++) {
        TheSynth->GetMic(i)->Stop();
        TheSynth->GetMic(i)->Start();
    }
    mBand = new Band(false, 0, BandUserMgr::GetBandUser(nullptr), mMaster);
    PopulatePlayerLists();
    mTrackerManager = new TrackerManager(mBand);
    auto _tmp1 = SystemConfig(demo)->FindInt(max_pct_complete);
    mDemoMaxPctComplete = _tmp1;
    mDemoMaxMs = SystemConfig(demo)->FindFloat(max_ms);
    LoadSong();
}

Game::~Game() {
    MetaPerformer::Current();
    TheGameConfig->ChangeRandomSeed();
    RELEASE(mShuttle);
    TheGame = nullptr;
    TheSongDB = nullptr;
    RELEASE(mBand);
    RELEASE(mMaster);
    RELEASE(mSongDB);
    RELEASE(mSongInfo);
    RELEASE(mTrackerManager);
    TrackPanelDirBase *dir = GetTrackPanelDir();
    if (dir)
        dir->CleanUpChordMeshes();

    TheSynth->RequirePushToTalk(false, -1);
    TheBandUI.RemoveSink(this, UIScreenChangeMsg::Type());
    TheBandUI.GetOvershell()->RemoveSink(this, "required_song_options_chosen");
    TheNetSession->RemoveSink(this, GameEndedMsg::Type());
    TheSessionMgr->RemoveSink(this, LocalUserLeftMsg::Type());
    TheSessionMgr->RemoveSink(this, RemoteUserLeftMsg::Type());
    TheSessionMgr->RemoveSink(this, RemoteLeaderLeftMsg::Type());
    if (TheNetSession->IsInGame()) {
        TheNetSession->EndGame(5, false, 0);
    }
    MetaPerformer::Current()->UnlockBandOrSolo();
}

void Game::LoadSong() {
    gSongLoadTimer.Restart();
    Symbol songSym = MetaPerformer::Current()->Song();
    PlayerTrackConfigList *cfgList = TheGameConfig->GetConfigList();
    auto _tmp0 = TheSongMgr.GetSongIDFromShortName(songSym, true);
    const SongDataValidate& i2 = TheSongMgr.Data(_tmp0)->IsOnDisc()
        ? kSongData_Validate
        : kSongData_NoValidation;
    Fader *fader = TheSynth->Find<Fader>("per_song_sfx_level.fade", false);
    if (fader)
        fader->SetVal(0);
    BeatMaster * &_ref0 = mMaster;
    _ref0->GetAudio()->SetPracticeMode(TheGameMode->InMode("practice"));
    RELEASE(mSongInfo);
        _ref0->Load(mSongInfo = new SongInfoCopy(TheSongMgr.SongAudioData(songSym)), 4, cfgList, false, i2, nullptr);
}

bool Game::IsLoaded() {
    if (mLoadState == kReady) {
        return true;
    }
    else if (mMaster && mMaster->GetAudio()->GetSongStream()
             && mMaster->GetAudio()->Fail()) {
        return true;
    } else if (mMaster && !mMaster->IsLoaded()) {
        return false;
    } else {
        if (mLoadState == kLoadingSong) {
            if (!mMaster->IsLoaded())
                return false;
            TheSongDB->PostLoad(GetBeatMaster()->GetMidiParserMgr()->GetEventsList());
            PostLoad();
            mLoadState = kWaitingForAudio;
        }
        if (mLoadState == kWaitingForAudio) {
            if (mMaster->GetAudio()->Fail())
                return true;
            if (!mMaster->GetAudio()->IsReady()) {
                TheSynth->Poll();
                return false;
            }
            mLoadState = kReady;
            TheProfileMgr.PushAllOptions();
            mTrackerManager->ConfigureGoals();
        }
        return mLoadState == kReady;
    }
}

void Game::PostLoad() {
    int i24 = -1;
    std::vector<Player *> &players = GetActivePlayers();
    FOREACH (it, players) {
        (*it)->SetTrack(TheGameConfig->GetTrackNum((*it)->GetUserGuid()));
        (*it)->PostLoad(false);
        Extent ext(0, 0);
        if ((*it)->GetCodaFreestyleExtents(ext) != 0) {
            MaxEq(i24, ext.unk4);
        }
    }

    float ms;
    if (i24 != -1)
        ms = TickToMs(i24);
    else
        ms = mSongDB->GetSongDurationMs();

    FOREACH (it, players) {
        if ((*it)->NeedsToSetCodaEnd()) {
            (*it)->SetCodaEndMs(ms);
        }
    }
    if (DataVariable("print_base_points").Int()) {
        PrintBasePoints();
    }
    ResetVoiceChatState();
}

bool Game::IsReady() {
    if (!IsLoaded())
        return false;
    else {
        std::vector<Player *> &players = GetActivePlayers();
        FOREACH (it, players) {
            if (!(*it)->IsReady())
                return false;
        }
        return true;
    }
}

void Game::Start() {
    for (int i = 0; i < unk154.size(); i++) {
        AddPlayer(unk154[i]);
    }
    unk154.clear();
    mHasIntro = false;
    Go();
}

#ifndef HX_NATIVE
// X360-only synth output-mixer singleton: constructed in the Synth unit
// (ctor at retail 0x82B2E8E8 stores this -> 0x82DDF9A8; builds the 5.1+drum
// channel-name table "FL/FR/C/LFE/SL/SR/DML/DMR" and "limiter"/"ratio"
// strings). Game::Go pokes its +0x110 flag when a song actually starts.
// Name is provisional -- no Wii/dc3 analog exists to confirm it.
class XOutputMixer {
public:
    unsigned char pad[0x110]; // 0x00..0x110 unexplored
    bool unk110; // 0x110: set true on Game::Go
};
extern XOutputMixer *TheXOutputMixer;
#endif

void Game::Go() {
    if (!mMaster->GetAudio()->Fail()) {
        if (unk150) {
            GetTrackPanelDir()->UpdateTrackSpeed();
            unk150 = false;
        }
        mMaster->GetAudio()->Play();
        std::vector<Player *> &players = GetActivePlayers();
        FOREACH (it, players) {
            (*it)->Start();
        }
        mIsPaused = false;
        unk148 = true;
        SetRealtime(false);
#ifndef HX_NATIVE
        TheXOutputMixer->unk110 = true;
#endif
    }
}

void Game::StartIntro() {
    std::vector<Player *> &players = GetActivePlayers();
    FOREACH (it, players) {
        (*it)->StartIntro();
    }
    mTrackerManager->StartIntro();
}

void Game::Reset() {
    TheTaskMgr.SetAVOffset(GetSongToTaskMgrMs() / 1000.0f);
    TheTaskMgr.SetSeconds(0, true);
    mRealtime = false;
    mTimeOffset = 0;
    mPauseTime = 0;
    mSongPos.AccessTotalTick() = 0;
    mSongPos.AccessTotalBeat() = 0;
    mSongPos.AccessMeasure() = 0;
    mSongPos.AccessBeat() = 0;
    mSongPos.AccessTick() = 0;
    mResult = kRestart;
    unk124 = 0;
    mTime.Restart();
    mHasIntro = false;
    unk11c = -1.0f;
    unkdc = -1.0f;
    unk120 = false;
    unk138 = false;
    unk6f = false;
    TheGamePanel->mDeJitter.Reset();
    if (DataVarExists("beatmatch_start_mbt")) {
        int m, b, t;
        ParseMBT(DataVariable("beatmatch_start_mbt").Str(), m, b, t);
        unk134 =
            TickToMs(TheSongDB->GetData()->GetMeasureMap()->MeasureBeatTickToTick(m, b, t)
            );
    }
}

bool Game::HasIntro() { return mHasIntro; }

float Game::GetSongToTaskMgrMs() {
    // Retail constructs (and never reads) two function-local static Symbols
    // here — guard bits 1/2 of the same $S word (evidence: fn 0x82659CE8
    // inits 82dd0a58 from "practice", 82dd0a54 from "trainer").
    LagContext ctx = kGame;
    static Symbol practice("practice");
    static Symbol trainer("trainer");
    float f1 = 1;
    if (mProperties.mInPracticeMode || mProperties.mInTrainer)
        f1 = mMusicSpeed;
    if (0.85 < f1 && f1 <= 0.95) {
        ctx = kPractice90;
    } else if (0.75 < f1 && f1 <= 0.85) {
        ctx = kPractice80;
    } else if (0.65 < f1 && f1 <= 0.75) {
        ctx = kPractice70;
    } else if (0.55 < f1 && f1 <= 0.65) {
        ctx = kPractice60;
    }
    return TheProfileMgr.GetSongToTaskMgrMs(ctx);
}

float Game::GetSongMs() const { return mMaster->GetAudio()->GetTime(); }

Symbol Game::GetSectionAtMs(float ms) const {
    int tick = (int)MsToTick(ms);
    SongDB *songDB = TheSongDB;
    const PracticeSection *begin = songDB->mPracticeSections.begin();
    const PracticeSection *end = begin + songDB->mPracticeSections.size();
    for (const PracticeSection *it = begin; it != end; it++) {
        if (tick < it->unk8) {
            return it->unk0;
        }
    }
    if (songDB->mPracticeSections.size() == 0) {
        MILO_WARN("No practice sections!");
        return Symbol();
    }
    return (end - 1)->unk0;
}

void Game::RemovePlayer(Player *p) {
    mAllActivePlayers.erase(
        std::remove(mAllActivePlayers.begin(), mAllActivePlayers.end(), p),
        mAllActivePlayers.end()
    );
}

void Game::SetPaused(bool b1, bool b2, bool b3) {
    mGameWantsPause = b1;
    UpdatePausedState(b2, b3);
}

bool Game::CanUserPause() const {
    return !mProperties.mEndWithSong
        || mLastPollMs < TheSongDB->GetSongDurationMs() - mDisablePauseMs;
}


void Game::SetMusicSpeed(float speed) {
    gDebugFullQuota = speed != 1;
    mMusicSpeed = speed;
    std::vector<Player *> &players = GetActivePlayers();
    FOREACH (it, players) {
        (*it)->SetMusicSpeed(speed);
    }
}

void Game::SetPitchMucker(bool b1) {
    mMuckWithPitch = true;
    mMaster->GetAudio()->SetMuckWithPitch(b1);
}

float Game::GetMusicSpeed() const { return mMusicSpeed; }

void Game::SetMusicVolume(float vol) {
    MILO_ASSERT(mMaster->GetAudio(), 0x446);
    mMaster->GetAudio()->SetMasterVolume(vol);
}

void Game::SetIntroRealTime(float f1) {
    TheTaskMgr.SetSeconds(f1, true);
    mHasIntro = f1 < 0;
    SetRealtime(true);
    unkd8 = f1 * 1000.0f;
}

Band *Game::GetBand() { return mBand; }

bool Game::IsActiveUser(BandUser *u) const {
    for (int i = 0; i < mAllActivePlayers.size(); i++) {
        if (mAllActivePlayers[i]->GetUser() == u)
            return true;
    }
    return false;
}

Player *Game::GetPlayerFromTrack(int i1, bool b2) const {
    for (int i = 0; i < mAllActivePlayers.size(); i++) {
        if (i1 == mAllActivePlayers[i]->GetTrackNum()) {
            return mAllActivePlayers[i];
        }
    }
    if (b2) {
        MILO_FAIL("There is no player with Track %i", i1);
    }
    return nullptr;
}

int Game::NumActivePlayers() const { return mAllActivePlayers.size(); }

int Game::GetScoringTracks() const {
    int tracks = 0;
    FOREACH (it, mAllActivePlayers) {
        if (!(*it)->GetQuarantined()) {
            tracks |= 1 << ((*it)->GetTrackNum());
        }
    }
    return tracks;
}

EndGameResult Game::GetResult(bool won) {
    EndGameResult result;
    if (won) {
        if (MetaPerformer::Current()->SongEndsWithEndgameSequence()) {
            result = kWonFinale;
        } else {
            result = kWon;
        }
    } else {
        result = kLost;
    }
    return result;
}

EndGameResult Game::GetResultForUser(BandUser *) {
    MILO_FAIL("Game::GetResultForUser shouldn't be called anymore");
    return kLost;
}

Player *Game::GetActivePlayer(int index) const {
    MILO_ASSERT_RANGE(index, 0, mAllActivePlayers.size(), 0x4B2);
    return mAllActivePlayers[index];
}

void Game::Jump(float f1, bool b2) {
    if (f1 < 0)
        f1 = 0;
    if (b2) {
        mMaster->Jump(f1);
        unk120 = true;
    }
    mBand->Restart(false);
    std::vector<Player *> &players = GetActivePlayers();
    FOREACH (it, players) {
        (*it)->Jump(f1, b2);
    }
    unkd8 = f1;
    CheckRollbackEnd(unkdc);
}

void Game::Replay() { unk120 = true; }

DataNode Game::OnJump(const DataArray *a) {
    const DataNode &node = a->Evaluate(2);
    DataType ty = node.Type();
    if (ty == kDataFloat || ty == kDataInt) {
        Jump(node.Float(), true);
    } else if (ty == kDataSymbol || ty == kDataString) {
        int m, b, t;
        ParseMBT(node.Str(), m, b, t);
        int tick = TheSongDB->GetData()->GetMeasureMap()->MeasureBeatTickToTick(m, b, t);
        Jump(TickToMs(tick), true);
    }
    return 1;
}

void Game::Rollback(float f1, float toMs) {
    MILO_ASSERT(toMs >= 0.0f, 0x4F1);
    GetTrackPanelDir()->UpdateTrackSpeed();
    EnableWorldPolling(false);
    mMaster->Jump(toMs);
    std::vector<Player *> &players = GetActivePlayers();
    FOREACH (it, players) {
        (*it)->Rollback(f1, toMs);
    }
    unkdc = f1;
    unk11c = 0;
    mInterpolator.Reset(mLastPollMs / 1000.0f, toMs / 1000.0f, 0, 60.0f, 3.5f);
    unkd8 = toMs;
    unk120 = true;
    if (mProperties.mInPracticeMode) {
        ObjectDir::Main()->Find<MidiParser>("practice_metronome", true)
            ->Reset(MsToBeat(toMs));
    }
}

bool Game::HandleRollbackAnimation() {
    if (unk11c < 0.0f)
        return true;
    if (unk11c <= mInterpolator.X1()) {
        float evalResult = mInterpolator.Eval(unk11c);
        TheTaskMgr.SetSeconds(evalResult, false);
        mLastPollMs = 1000.0f * evalResult;
        unk11c += 1.0f;
        return false;
    }
    unk11c = -1.0f;
    return true;
}

void Game::CheckRollbackEnd(float f1) {
    float rollbackEnd = unkdc;
    if (rollbackEnd != -1.0f && f1 >= rollbackEnd) {
        EnableWorldPolling(true);
        unkdc = -1.0f;
    }
}

void Game::EnableWorldPolling(bool b1) {
    if (TheBandDirector) {
        TheBandDirector->unke5 = b1;
    }
    UIPanel *panel = ObjectDir::Main()->Find<UIPanel>("world_panel", true);
    if (panel) {
        WorldDir *worldDir = dynamic_cast<WorldDir *>(panel->LoadedDir());
        if (worldDir) {
            // Retail pokes WorldDir+0x381 = mPollCamera (layout now retail-exact).
            worldDir->SetPollCamera(b1);
        }
    }
    MidiParserMgr *midiParserMgr = mMaster->GetMidiParserMgr();
    if (midiParserMgr && !mProperties.mInPracticeMode && b1) {
        // Retail: bool at MidiParserMgr+0x69 (Wii unk59 shifted +0x10 by the
        // MI vptrs); not yet a named member in our MidiParserMgr.h.
        *(bool *)((char *)midiParserMgr + 0x69) = b1;
    }
}

void Game::ResetAudio() { mMaster->ResetAudio(); }

void Game::RebuildData() {
    std::vector<Player *> &players = GetActivePlayers();
    FOREACH (it, players) {
        (*it)->RebuildPhrases();
    }
    mSongDB->RebuildData();
}

void Game::Restart(bool doSave) {
    if (!mMaster->GetAudio()->Fail()) {
        TheBandUI.mOvershell->RemoveUsersRequiringSongOptions();
        TheGamePanel->mDeJitter.Reset();
        TheSynth->StopAllSfx(false);
        if (doSave) {
            if (0.0f == mResumeTime) {
                mMaster->Reset();
            } else {
                mMaster->Jump(std::max(0.0f, std::max(0.0f, mResumeTime - 2000.0f) - 2000.0f));
            }
            unk120 = true;
            mMaster->GetAudio()->SetMuckWithPitch(true);
            mLastPollMs = mResumeTime;
        }
        EnableWorldPolling(true);
        ReconcilePlayers();
        RebuildData();
        mDrumFillsMod = TheModifierMgr->IsModifierActive("mod_drum_fills");
        std::vector<Player *> &players = GetActivePlayers();
        for (int i = 0; i < mAllActivePlayers.size(); i++) {
            Player *p = players[i];
            if (p) {
                p->SetFillLogic(GetFillLogic());
            }
        }
        mBand->Restart(mResumeTime != 0.0f);
        if (MetaPerformer::Current()->IsFirstSong()) {
            mBand->SetAccumulatedScore(0);
        }
        if (mInvalidScore) {
            FOREACH (it, players) {
                (*it)->SetQuarantined(true);
            }
        }
        mTrackerManager->Restart();
        unk13c = -1;
        unk140 = -1.0f;
        SetMusicSpeed(1.0f);
        unk148 = false;
        ResetVoiceChatState();
        MetaPerformer::Current()->LockBandOrSolo();
    }
}

FORCE_LOCAL_INLINE
std::vector<Player *> &Game::GetActivePlayers() { return mAllActivePlayers; }
END_FORCE_LOCAL_INLINE

void Game::SetBackgroundVolume(float vol) {
    mMaster->GetAudio()->SetBackgroundVolume(vol);
}

void Game::SetForegroundVolume(float vol) {
    mMaster->GetAudio()->SetForegroundVolume(vol);
}

void Game::SetVocalCueVolume(float vol) { mMaster->GetAudio()->SetVocalCueFader(vol); }
void Game::SetStereo(bool stereo) { mMaster->GetAudio()->SetStereo(stereo); }

void Game::AddMusicFader(Fader *fader) {
    MILO_ASSERT(mMaster->GetAudio() && mMaster->GetAudio()->GetSongStream(), 0x645);
    mMaster->GetAudio()->GetSongStream()->Faders()->Add(fader);
}

void Game::PopulatePlayerLists() {
    std::vector<BandUser *> &users = TheBandUserMgr->GetBandUsers();
    FOREACH (it, users) {
        BandUser *pUser = *it;
        MILO_ASSERT(pUser, 0x651);
        Player *p = pUser->GetPlayer();
        if (p)
            mAllActivePlayers.push_back(p);
    }
    NullLocalBandUser *nullUser = TheBandUserMgr->GetNullUser();
    if (nullUser) {
        Player *p = nullUser->GetPlayer();
        if (p)
            mAllActivePlayers.push_back(p);
    }
}

DECOMP_FORCEACTIVE(Game, "pPlayer", "player")

void Game::AddBonusPoints(BandUser *pUser, int i2, int) {
    MILO_ASSERT(pUser, 0x666);
    pUser->GetPlayer()->AddPoints(i2, 0, 0);
}

float Performer::GetTotalStars() const { return GetNumStarsFloat(); }

FORCE_LOCAL_INLINE
Performer *Game::GetMainPerformer() { return mBand->MainPerformer(); }
END_FORCE_LOCAL_INLINE

ExcitementLevel Game::GetCrowdExcitement() { return GetMainPerformer()->GetExcitement(); }

bool Game::AllowInput() const { return !mPauseTime && !mRealtime && !mNeverAllowInput; }
void Game::SetKickAutoplay(bool autokick) { gKickAutoplay = autokick; }

void Game::SetVocalPercussionBank(Player *p, ObjectDir *dir) {
    VocalPlayer *vp = dynamic_cast<VocalPlayer *>(p);
    if (vp) {
        vp->mTambourineManager.SetBank(dir);
    }
}

void Game::SetVocalPercussionBank(ObjectDir *dir) {
    FOREACH (it, mAllActivePlayers) {
        SetVocalPercussionBank(*it, dir);
    }
}

void Game::SetDrumKitBank(Player *p, ObjectDir *dir) {
    if (p->GetTrackType() == kTrackDrum) {
        GemPlayer *gp = dynamic_cast<GemPlayer *>(p);
        if (gp)
            gp->SetDrumKitBank(dir);
    }
}

void Game::SetDrumKitBank(ObjectDir *dir) {
    FOREACH (it, mAllActivePlayers) {
        SetDrumKitBank(*it, dir);
    }
}

void Game::PrintBasePoints() {
    int pts = TheSongDB->TotalBasePoints();
    MILO_LOG("%s: (base_points %d)\n", MetaPerformer::Current()->Song(), pts);
}

void Game::SetGameOver(bool over) {
    if (!TheGamePanel->IsGameOver()) {
        if (!over) {
            unk124 = mLastPollMs;
        }
        AutoTimer::SetCollectStats(false, TheRnd.VerboseTimers());
        EndGameResult res = GetResult(over);
        TheNetSession->EndGame(res, false, unk124);
    }
}

DataNode Game::OnMsg(const GameEndedMsg &msg) {
    OvershellPanel *panel = TheBandUI.GetOvershell();
    if (panel->InSong()) {
        panel->SetActiveStatus(kOvershellInactive);
    }
    ClearState();
    TheSynth->RequirePushToTalk(false, -1);
    if (msg.GetResult() == 0) {
        static DataArrayPtr restart("game_restart");
        restart->Execute();
        return 1;
    } else {
        TheGamePanel->SetGameOver();
        mResult = msg.GetResult();
        unk124 = msg->Float(3);
        switch (mResult) {
        case kLost:
            TheGamePanel->Export(Message("game_lost"), true);
            break;
        case kWon:
            TheGamePanel->Export(Message("game_won"), true);
            break;
        case kWonFinale:
            TheGamePanel->Export(Message("game_won_finale"), true);
            break;
        case kSkip:
            SetInvalidScore(true);
            SetSkippedSong(true);
            unk148 = true;
            unk120 = false;
            mBand->SetGameOver();
            break;
        case kQuit:
            break;
        default:
            MILO_WARN("bad game over state");
            break;
        }
        TheGamePanel->Export(Message("game_over"), true);
        if (mResult == kSkip) {
            TheGamePanel->Export(Message("game_skip"), true);
        }
        return 1;
    }
}

DataNode Game::OnLocalUserReadyToPlay(const DataArray *a) {
    AddPlayer(a->Obj<BandUser>(2));
    return 1;
}

DataNode Game::OnMsg(const LocalUserLeftMsg &msg) {
    DropUser(dynamic_cast<BandUser *>(msg.GetUser()));
    return 1;
}

DataNode Game::OnMsg(const RemoteUserLeftMsg &msg) {
    DropUser(dynamic_cast<BandUser *>(msg.GetUser()));
    return 1;
}

DataNode Game::OnMsg(const RemoteLeaderLeftMsg &msg) {
    if (unkc4) {
        if (!TheUI->InTransition()) {
            TheGamePanel->Handle(game_outro_msg, true);
        }
    }
    return 1;
}

DataNode Game::OnMsg(const UIScreenChangeMsg &) {
    unkc4 = false;
    return 1;
}

void Game::DropUser(BandUser *user) {
    std::vector<BandUser *>::iterator it;
    bool found = false;
    for (int i = 0; i < mAllActivePlayers.size(); i++) {
        if (mAllActivePlayers[i]->GetUser() == user) {
            found = true;
            break;
        }
    }
    if (!found) {
        it = std::find(unk154.begin(), unk154.end(), user);
        if (it != unk154.end()) {
            unk154.erase(std::remove(unk154.begin(), unk154.end(), user), unk154.end());
        }
    }
    MILO_ASSERT(user, 0x7D3);
    MILO_ASSERT(mBand, 0x7D5);
    mBand->RemoveUser(user);
    TheGameConfig->RemoveUser(user);
}

DataNode Game::OnMsg(const NewOvershellLocalUserMsg &msg) {
    if (TheGamePanel->IsGameOver())
        return 0;
    else {
        BandUser *band_user = msg.GetBandUser();
        MILO_ASSERT(band_user, 0x7E7);
        AddUser(band_user);
        return 1;
    }
}

void Game::AddUser(BandUser *user) {
    PlayerTrackConfigList *plist = TheGameConfig->GetConfigList();
    plist->AddPlaceholderConfig(user->GetUserGuid(), user->GetSlot(), !user->IsLocal());
    MILO_ASSERT(mBand, 0x7F6);
    mBand->AddUserDynamically(user);
}

void Game::ResetVoiceChatState() {
    TheSynth->RequirePushToTalk(false, -1);
    std::vector<LocalBandUser *> users;
    TheBandUserMgr->GetLocalBandUsersInSession(users);
    LocalBandUser **it = users.begin();
    for (; it != users.end(); it++) {
        LocalBandUser *user = *it;
        if (user->GetTrackType() == kTrackVocals) {
            TheSynth->RequirePushToTalk(true, user->GetPadNum());
            break;
        }
    }
}

void Game::ClearState() {
    for (int i = 0; i < mAllActivePlayers.size(); i++) {
        Player *p = mAllActivePlayers[i];
        if (p) {
            p->unk2a9 = false;
        }
    }
}

static const unsigned char gPlayerStates[16][4] = {
    {0, 0, 0, 0},
    {1, 1, 1, 1},
    {1, 1, 1, 0},
    {1, 1, 0, 1},
    {1, 0, 1, 1},
    {0, 1, 1, 1},
    {1, 1, 0, 0},
    {1, 0, 1, 0},
    {1, 0, 0, 1},
    {0, 1, 1, 0},
    {0, 1, 0, 1},
    {0, 0, 1, 1},
    {1, 0, 0, 0},
    {0, 1, 0, 0},
    {0, 0, 1, 0},
    {0, 0, 0, 1},
};

static const char *gStrPlayerStates[16] = {
    "off",
    "(1, 2, 3, 4) all",
    "(1, 2, 3) left guitar, drums, vocals",
    "(1, 2, 4) left guitar, drums, right guitar",
    "(1, 3, 4) left guitar, vocals, right guitar",
    "(2, 3, 4) drums, vocals, right guitar",
    "(1, 2) left guitar and drums",
    "(1, 3) left guitar and vocals",
    "(1, 4) left guitar and right guitar",
    "(2, 3) drums and vocals",
    "(2, 4) drums and right guitar",
    "(3, 4) vocals and right guitar",
    "(1) left guitar",
    "(2) drums",
    "(3) vocals",
    "(4) right guitar",
};

void Game::E3CheatAutoplayAccuracy() {
    for (int slot = 0; slot < 4; slot++) {
        BandUser *user = TheBandUserMgr->GetUserFromSlot(slot);
        if (user && (int)user->mPlayer) {
            if (user->IsLocal()) {
                Player *player = user->mPlayer;
                RealGuitarGemPlayer *rggp = dynamic_cast<RealGuitarGemPlayer *>(player);
                if (rggp) {
                    rggp->SetAutoplay(true);
                    rggp->SetAutoplayAccuracy(0.95f);
                }
            }
        }
    }
}

const char *Game::DebugCycleAutoplay() {
    // Phase 1: find the current state index (the entry in gPlayerStates that
    // matches the current per-slot autoplay settings).
    bool anyAutoplay;
    int statePrev = 0;
    bool mismatch;
    do {
        mismatch = false;
        const unsigned char *stateRow = gPlayerStates[statePrev];
        for (int slot = 0; slot < 4; slot++) {
            BandUser *user = TheBandUserMgr->GetUserFromSlot(slot);
            if (user && user->IsLocal() && user->mPlayer) {
                if (stateRow[slot] != user->mPlayer->IsAutoplay()) {
                    mismatch = true;
                } else {
                    continue;
                }
            } else if (stateRow[slot] != 0) {
                mismatch = true;
            } else {
                continue;
            }
            break;
        }
        if (mismatch) {
            statePrev = (statePrev + 1) & 0xF;
        }
    } while (mismatch);

    // Phase 2: advance to the next applicable state.  A state is applicable
    // when every slot that it wants to autoplay has a valid local player.
    int stateCur = statePrev;
    do {
        int nextIdx = (stateCur + 1) & 0xF;
        mismatch = false;
        const unsigned char *stateRow = gPlayerStates[nextIdx];
        stateCur = nextIdx;
        for (int slot = 0; slot < 4; slot++) {
            if (stateRow[slot] != 0) {
                BandUser *user = TheBandUserMgr->GetUserFromSlot(slot);
                if (!user || !user->IsLocal() || !user->mPlayer) {
                    MILO_ASSERT(statePrev != stateCur, 0x8e7);
                    mismatch = true;
                    break;
                }
            }
        }
    } while (mismatch);

    // Phase 3: apply the new state and compute whether any player is on autoplay.
    anyAutoplay = false;
    const unsigned char *stateRow = gPlayerStates[stateCur];
    for (int slot = 0; slot < 4; slot++) {
        BandUser *user = TheBandUserMgr->GetUserFromSlot(slot);
        if (user && user->mPlayer) {
            if (user->IsLocal()) {
                int val = stateRow[slot];
                user->mPlayer->SetAutoplay(val);
                anyAutoplay = (bool)(anyAutoplay | val);
            }
        }
    }

    // Also apply to the null user's player if present.
    NullLocalBandUser *nullUser = TheBandUserMgr->GetNullUser();
    if (nullUser && nullUser->mPlayer) {
        nullUser->mPlayer->SetAutoplay(anyAutoplay);
    }

    return gStrPlayerStates[stateCur];
}

const char *Game::DebugCycleAutoplayAccuracy() {
    float accuracy = -1.0f;
    for (int slot = 0; slot < 4; slot++) {
        BandUser *user = TheBandUserMgr->GetUserFromSlot(slot);
        if (user && user->mPlayer && user->IsLocal()) {
            GemPlayer *gp = dynamic_cast<GemPlayer *>(user->mPlayer);
            if (gp) {
                accuracy = gp->CycleAutoplayAccuracy();
            }
        }
    }
    if (accuracy >= 0.0f) {
        return MakeString("%0.1f%%", 100.0f * accuracy);
    }
    return MakeString("NA");
}

void Game::SetInvalidScore(bool score) { mInvalidScore = score; }

void Game::SetSkippedSong(bool skipped) {
    mSkippedSong = skipped;
    unk124 = mLastPollMs;
}

void Game::SetResumeFraction(float f1) {
    mResumeTime = f1 * mSongDB->GetSongDurationMs();
}

bool Game::IsInvalidScore() const { return mInvalidScore; }
bool Game::SkippedSong() const { return mSkippedSong; }
bool Game::ResumedNoScore() const { return mResumeTime != 0; }

void Game::ForceTrackerStars(int i) { mTrackerManager->ForceStars(i); }
void Game::OnPlayerAddEnergy(Player *p, float f) {
    mTrackerManager->OnPlayerAddEnergy(p, f);
}
void Game::OnPlayerSaved(Player *p) { mTrackerManager->OnPlayerSaved(p); }
void Game::OnPlayerRemoved(Player *p) { mTrackerManager->HandleRemovePlayer(p); }
void Game::OnPlayerQuarantined(Player *p) { mTrackerManager->OnPlayerQuarantined(p); }
void Game::OnRemoteTrackerFocus(Player *p, int x, int y, int z) {
    mTrackerManager->OnRemoteTrackerFocus(p, x, y, z);
}
void Game::OnRemoteTrackerPlayerProgress(Player *p, float f) {
    mTrackerManager->OnRemoteTrackerPlayerProgress(p, f);
}
void Game::OnRemoteTrackerSectionComplete(Player *p, int x, int y, int z) {
    mTrackerManager->OnRemoteTrackerSectionComplete(p, x, y, z);
}
void Game::OnRemoteTrackerPlayerDisplay(Player *p, int x, int y, int z) {
    mTrackerManager->OnRemoteTrackerPlayerDisplay(p, x, y, z);
}
void Game::OnRemoteTrackerDeploy(Player *p) { mTrackerManager->OnRemoteTrackerDeploy(p); }
void Game::OnRemoteTrackerEndDeployStreak(Player *p, int i) {
    mTrackerManager->OnRemoteTrackerEndDeployStreak(p, i);
}
void Game::OnRemoteTrackerEndStreak(Player *p, int i, int j) {
    mTrackerManager->OnRemoteTrackerEndStreak(p, i, j);
}

#pragma push
#pragma dont_inline on
BEGIN_HANDLERS(Game)
    HANDLE_ACTION(start, Start())
    HANDLE_ACTION(
        set_paused,
        SetPaused(
            _msg->Int(2),
            _msg->Size() > 3 ? _msg->Int(3) : true,
            _msg->Size() > 4 ? _msg->Int(4) : true
        )
    )
    HANDLE_ACTION(set_overshell_pause, OvershellSetPaused(_msg->Int(2)))
    HANDLE_EXPR(get_paused, mIsPaused)
    HANDLE_EXPR(can_user_pause, CanUserPause())
    HANDLE_ACTION(set_music_speed, SetMusicSpeed(_msg->Float(2)))
    // retail stripped set_pitch_mucker (dev-only) from the dispatch table
    HANDLE_EXPR(music_speed, mMusicSpeed)
    HANDLE_EXPR(get_song_ms, GetSongMs())
    HANDLE_EXPR(get_section_at_ms, GetSectionAtMs(_msg->Float(2)))
    HANDLE_ACTION(set_music_volume, SetMusicVolume(_msg->Float(2)))
    HANDLE_ACTION(never_allow_input, NeverAllowInput(_msg->Int(2)))
    HANDLE(set_shuttle, OnSetShuttle)
    HANDLE_EXPR(shuttle_active, mShuttle->IsActive())
    HANDLE(jump, OnJump)
    HANDLE_ACTION(set_intro_real_time, SetIntroRealTime(_msg->Float(2)))
    HANDLE_ACTION(set_realtime, SetRealtime(_msg->Int(2)))
    HANDLE_EXPR(num_active_players, NumActivePlayers())
    HANDLE_EXPR(is_active_user, IsActiveUser(_msg->Obj<BandUser>(2)))
    HANDLE_EXPR(active_player, GetActivePlayer(_msg->Int(2)))
    HANDLE(foreach_active_player, ForEachActivePlayer)
    HANDLE_EXPR(ms_per_beat, TheTempoMap->GetTempo(TheTaskMgr.GetSongPos().GetTick()))
    HANDLE_EXPR(get_result, GetResult(false))
    HANDLE_EXPR(get_result_for_user, GetResultForUser(_msg->Obj<BandUser>(2)))
    HANDLE_ACTION(set_fake_hit_gems_in_fill, mSongDB->SetFakeHitGemsInFill(_msg->Int(2)))
    HANDLE_EXPR(main_performer, GetMainPerformer())
    HANDLE_ACTION(set_kick_autoplay, SetKickAutoplay(_msg->Int(2)))
    HANDLE_EXPR(is_waiting, IsWaiting())
    HANDLE_ACTION(print_base_points, (PrintBasePoints(), MetaPerformer::Current()->Song()))
    HANDLE_ACTION(reset_audio, ResetAudio())
    HANDLE(adjust_for_vocal_phrases, OnAdjustForVocalPhrases)
    HANDLE_EXPR(get_fraction_completed, GetFractionCompleted())
    HANDLE_ACTION(set_resume_fraction, SetResumeFraction(_msg->Float(2)))
    HANDLE_EXPR(resumed_no_score, ResumedNoScore())
    HANDLE_EXPR(skipped_song, SkippedSong())
    HANDLE_EXPR(is_invalid_score, IsInvalidScore())
    HANDLE_ACTION(stats_synced, OnStatsSynced())
    HANDLE_ACTION(replay, Replay())
    HANDLE_ACTION(print_star_thresholds, TheScoring->ComputeStarThresholds(true))
    HANDLE_EXPR(num_active_players, (int)mAllActivePlayers.size())
    HANDLE_ACTION(set_invalid_score, SetInvalidScore(_msg->Int(2)))
    HANDLE_ACTION(set_ready_for_exit, unkc4 = _msg->Int(2))
    HANDLE(required_song_options_chosen, OnLocalUserReadyToPlay)
    HANDLE_ACTION(e3_cheat_autoplay, E3CheatAutoplayAccuracy())
    HANDLE_MESSAGE(GameEndedMsg)
    HANDLE_MESSAGE(NewOvershellLocalUserMsg)
    HANDLE_MESSAGE(UIScreenChangeMsg)
    HANDLE_MESSAGE(LocalUserLeftMsg)
    HANDLE_MESSAGE(RemoteUserLeftMsg)
    HANDLE_MESSAGE(RemoteLeaderLeftMsg)
    // retail stripped debug_cycle_autoplay[_accuracy] (dev-only) from the dispatch table
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0xA10)
END_HANDLERS
#pragma pop

DataNode Game::OnSetShuttle(DataArray *arr) {
    if (arr->Size() > 3) {
        mShuttle->mPadNum = arr->Int(3);
    }
    bool active = arr->Int(2);
    if (active) {
        mShuttle->mMs = mMaster->GetAudio()->GetTime();
        mShuttle->mEndMs = mSongDB->GetSongDurationMs();
    } else {
        Jump(mShuttle->mMs, true);
        while (!IsReady())
            TheSynth->Poll();
    }
    mShuttle->SetActive(active);
    return 0;
}

DataNode Game::ForEachActivePlayer(const DataArray *a) {
    DataNode *var = a->Var(2);
    DataNode tmp = *var;
    for (int i = 0; i < mAllActivePlayers.size(); i++) {
        Player *p = mAllActivePlayers[i];
        *var = p;
        for (int j = 3; j < a->Size(); j++) {
            a->Command(j)->Execute();
        }
    }
    *var = tmp;
    return 0;
}

DataNode Game::OnAdjustForVocalPhrases(DataArray *a) {
    float f24 = a->Float(2);
    float f28 = a->Float(3);
    AdjustForVocalPhrases(f24, f28);
    *a->Var(2) = f24;
    *a->Var(3) = f28;
    return 0;
}

void Game::OnStatsSynced() { mTrackerManager->OnStatsSynced(); }

void Game::SetTimeOffset() {
    mTime.Split();
    float cyclesToMs = mTime.Ms();
    mTimeOffset = (1000.0f * TheTaskMgr.Seconds(TaskMgr::kRealTime)) - cyclesToMs
        - TheProfileMgr.GetSongToTaskMgrMs(kGame);
}

void Game::SetRealtime(bool realtime) {
    if (mProperties.mInDrumTrainer) {
        realtime = true;
    } else {
        FOREACH (it, mAllActivePlayers) {
            (*it)->SetRealtime(realtime);
        }
    }
    mRealtime = realtime;
    if (mRealtime) {
        SetTimeOffset();
    }
}

void Game::SetNoFail(bool doSave) {
    if (doSave && TheGamePanel->mGameState != kGameOver) {
        for (int i = 0; i < mAllActivePlayers.size(); i++) {
            Player *p = mAllActivePlayers[i];
            if (p->mEnabledState == kPlayerDisabled) {
                p->Save(p->GetUser(), false);
            }
        }
    }
}

void Game::OvershellSetPaused(bool paused) {
    if (TheNetSession->IsInGame()) {
        bool canPause = false;
        if (!mProperties.mEndWithSong
            || mLastPollMs < TheSongDB->GetSongDurationMs() - mDisablePauseMs) {
            canPause = true;
        }
        if (canPause && ThePlatformMgr.GetDiskError() == kNoDiskError) {
            mOvershellWantsPause = paused;
            UpdatePausedState(true, true);
        }
    }
}

void Game::UpdatePausedState(bool allowSfx, bool doRollback) {
    bool wantPause = mGameWantsPause | mOvershellWantsPause;
    if ((bool)wantPause != mIsPaused) {
        if (wantPause) {
            unk6c = ThePlatformMgr.ScreenSaver();
            ThePlatformMgr.SetScreenSaver(true);
        } else if (TheGamePanel) {
            ThePlatformMgr.SetScreenSaver(unk6c);
        }
        if (!wantPause || allowSfx) {
            TheSynth->PauseAllSfx(wantPause);
        }
        if (!wantPause) {
            TheTaskMgr.SetAVOffset(GetSongToTaskMgrMs() / 1000.0f);
        }
        FOREACH (it, mAllActivePlayers) {
            (*it)->SetPaused(wantPause);
        }
        if (!wantPause && mProperties.mInTrainer) {
            GetTrackPanelDir()->UpdateTrackSpeed();
        }
        if (!wantPause && MetaPerformer::Current()->IsNoFailActive()) {
            SetNoFail(true);
        }
        if (unk148) {
            if (wantPause) {
                mMaster->GetAudio()->SetPaused(true);
            } else if (TheGamePanel->mGameState != kGameOver) {
                if (doRollback) {
                    float ms = (unkdc != -1.0f) ? unkdc : mLastPollMs;
                    float rollbackTarget;
                    if (mProperties.mInTrainer && TheTrainerPanel) {
                        rollbackTarget = std::max(0.0f, ms - 1000.0f);
                        mLastPollMs = 1000.0f * TheTaskMgr.Seconds(TaskMgr::kRealTime);
                    } else {
                        rollbackTarget = std::max(0.0f, ms - 2000.0f);
                    }
                    Rollback(ms, rollbackTarget);
                } else {
                    if (mMaster->GetAudio()->IsReady()) {
                        mMaster->GetAudio()->SetPaused(false);
                    }
                }
            }
        }
        if (wantPause) {
            TheTaskMgr.SetSeconds(
                TheTaskMgr.Seconds(TaskMgr::kRealTime), false
            );
        } else if (mRealtime) {
            SetTimeOffset();
        }
        if (wantPause) {
            TheGamePanel->Export(world_pause_msg, true);
        } else {
            TheGamePanel->Export(world_unpause_msg, true);
        }
        if (!wantPause) {
            while (!FileDiscSpinUp())
                ;
        }
        mIsPaused = wantPause;
    }
}

bool Game::HandleAudioLoad() {
    if (!unk120)
        return true;
    MasterAudio *audio = mMaster->GetAudio();
    if (audio->Fail())
        return false;
    if (!audio->IsReady() && !audio->IsFinished()) {
        TheSynth->Poll();
        return false;
    }
    if (unk120) {
        TheGamePanel->mDeJitter.Reset();
        if (mRealtime) {
            mTime.Restart();
            mTimeOffset = unkd8 - mTime.Ms();
        }
        if (!mHasIntro && TheGamePanel->mGameState != kGameOver) {
            unk6f = true;
            Go();
        }
        unk120 = false;
    }
    return true;
}

void Game::CheckSectionEnd(float ms) {
    float nextMs = unk140;
    if (nextMs != -1.0f && ms < nextMs)
        return;
    SongDB *songDB = TheSongDB;
    int numSections = (int)songDB->mPracticeSections.size();
    if (numSections != 0) {
        if (unk13c >= numSections)
            return;
        unk13c++;
        if (unk13c < numSections) {
            const PracticeSection &sec = songDB->mPracticeSections[unk13c];
            unk140 = TickToMs(sec.unk8);
            FOREACH (it, mAllActivePlayers) {
                (*it)->HandleNewSection(sec, unk13c, numSections);
            }
        }
    }
}

float Game::PollShuttle() {
    mShuttle->Poll();
    float shuttleMs = mShuttle->mMs;
    SongPos pos = mSongDB->GetData()->CalcSongPos(shuttleMs);
    mSongPos = pos;
    TheTaskMgr.SetSongPos(mSongPos);
    Jump(shuttleMs, false);
    return shuttleMs;
}

bool Game::IsWaiting() {
    MasterAudio *audio = mMaster->GetAudio();
    if (audio->Fail())
        return false;
    if (audio->IsReady())
        return false;
    return !audio->IsFinished();
}

float Game::GetFractionCompleted() const {
    // single-expression bool materialization (retail li/bne/li/clrlwi shape)
    bool resumed = mResumeTime != 0.0f;
    if (resumed) {
        return std::max(0.0f, mResumeTime / mSongDB->GetSongDurationMs());
    }
    if (unk124 == 0.0f) {
        return 1.0f;
    }
    return std::max(0.0f, unk124 / mSongDB->GetSongDurationMs());
}

void Game::AdjustForVocalPhrases(float &startMs, float &endMs) const {
    VocalNoteList *phrases = TheSongDB->GetVocalNoteList(0);
    float phraseEnd = 0.0f;
    int i = 0;
    int byteOff = 0;
    for (; (unsigned)i < phrases->mPhrases.size(); i++, byteOff += sizeof(VocalPhrase)) {
        const VocalPhrase &phrase = phrases->mPhrases[i];
        phraseEnd = phrase.unk0 + phrase.unk4;
        if (phraseEnd > endMs)
            break;
        if (phraseEnd > startMs && phrase.unk10 != phrase.unk14) {
            for (int part = 0; part < 3; part++) {
                VocalNoteList *noteList = TheSongDB->GetVocalNoteList(part);
                if (noteList != nullptr) {
                    int noteTick = noteList->HasNoteInRange(
                        (int)MsToTick(phrase.unk0), (int)MsToTick(phraseEnd)
                    );
                    if (noteTick != -1) {
                        float noteMs = TickToMs(noteTick);
                        if (noteMs < startMs)
                            startMs = noteMs;
                        if (endMs < phraseEnd)
                            endMs = phraseEnd;
                    }
                }
            }
        }
    }
    if ((unsigned)i >= (unsigned)(phrases->mPhrases.size() - 1) && endMs < phraseEnd) {
        endMs = phraseEnd;
    }
}

void Game::AddPlayer(BandUser *user) {
    if (TheGamePanel->mGameState == kGameOver) {
        DropUser(user);
        unk154.push_back(user);
        return;
    }
    if (mLoadState == kLoadingSong) {
        unk154.push_back(user);
        return;
    }
    Symbol trackSym = user->GetTrackSym();
    bool noPartInSong;
    MetaPerformer *perf = MetaPerformer::Current();
    if (perf != nullptr) {
        noPartInSong = true;
        if (perf->HasSong()) {
            if (perf->PartPlaysInSong(trackSym)) {
                noPartInSong = false;
            }
        }
        if (noPartInSong) {
            GetTrackPanel()->DoHandleAddPlayer(user);
            GetTrackPanel()->DoPostHandleAddPlayer(user);
            return;
        }
    }
    PlayerTrackConfigList *cfgList;
    SongData *songData = TheSongDB->GetData();
    cfgList = TheGameConfig->GetConfigList();
    TheGameConfig->AssignTrack(user);
    cfgList->ProcessConfig(user->GetUserGuid());
    songData->UpdatePlayerTrackConfigList(cfgList);
    Player *player = mBand->AddPlayerDynamically(mMaster, user);
    SetDrumKitBank(player, TheGamePanel->mDrumKitBank);
    mAllActivePlayers.push_back(player);
    player->Start();
    SetPaused(true, true, true);
    SetPaused(false, true, true);
    mTrackerManager->HandleAddPlayer(player);
}

void Game::ReconcilePlayers() {
    std::vector<BandUser *> users;
    TheBandUserMgr->GetParticipatingBandUsersInSession(users);
    for (int i = 0; i < users.size(); i++) {
        BandUser *user = users[i];
        if (!user->mPlayer) {
            if (!user->IsFullyInGame()) {
                DropUser(user);
            } else if (!TheGameConfig->GetConfigList()->UserPresent(user->GetUserGuid())) {
                AddUser(user);
                AddPlayer(user);
            }
            // Remove user from pending queue if present
            std::vector<BandUser *>::iterator uit = std::find(unk154.begin(), unk154.end(), user);
            if (uit != unk154.end()) {
                unk154.erase(uit);
            }
        }
    }
}

void Game::Poll() {
    if (mIsPaused) {
        if (!mRealtime)
            return;
        if (!unk148)
            return;
    }
    if (!HandleRollbackAnimation())
        return;
    if (HandleAudioLoad()) {
        if (!unk6f && !mIsPaused) {
            unk6f = true;
            if (!mHasIntro) {
                Go();
            }
        }
        mTime.Split();
        float songMs = 0.0f;
        if (TheGamePanel->unk150) {
            float audioMs;
            if (mRealtime) {
                audioMs = mTime.Ms() + mTimeOffset;
            } else {
                audioMs = mMaster->GetAudio()->GetTime();
            }
            float rawMs;
            songMs = TheGamePanel->mDeJitter.NewMs(GetSongToTaskMgrMs() + audioMs, rawMs);
            TheTaskMgr.SetSeconds(songMs * 0.001f, false);
        }
        // Retail keeps an UNUSED local static Symbol here (residue of a
        // stripped debug/mode check); the guard word + ctor are load-bearing.
        static Symbol drum_trainer("drum_trainer");
        if ((!mIsPaused && !mRealtime && IsReady()) || mProperties.mInDrumTrainer) {
            songMs = TheTaskMgr.Seconds(TaskMgr::kRealTime) * 1000.0f;
            mSongPos = mSongDB->GetData()->CalcSongPos(songMs);
            TheTaskMgr.SetSongPos(mSongPos);
        }
        if (songMs >= 0.0f) {
            mMaster->Poll(songMs);
            mBand->Poll(songMs, mSongPos);
            mTrackerManager->Poll(songMs);
        } else {
            mMaster->GetMidiParserMgr()->Poll();
        }
        // Retail-360 kiosk-demo auto-skip (Wii dev never read the demo fields).
        if (TheNetSession->IsInGame()) {
            if (MetaPerformer::Current()->IsPlayingDemo()
                && TheGamePanel->mGameState != kGameOver
                && (songMs * 100.0f / mSongDB->GetSongDurationMs() >= mDemoMaxPctComplete
                    || mMaster->GetAudio()->GetTime() >= mDemoMaxMs)) {
                TheNetSession->EndGame(kSkip, false, 0.0f);
            }
        }
        CheckSectionEnd(songMs);
        CheckRollbackEnd(songMs);
        mLastPollMs = songMs;
        if (mResumeTime == 0 && !mIsPaused) {
            unk130 = mLastPollMs / mSongDB->GetSongDurationMs();
        }
        if (!unk138) {
            float startMs = unk134;
            if (startMs > 0.0f && songMs > 0.0f) {
                unk138 = true;
                Jump(startMs, true);
            }
        }
    }
}

Game::Properties::Properties()
    : mInTrainer(TheGameMode->InMode("trainer")),
      mInDrumTrainer(TheGameMode->InMode("drum_trainer")),
      mInPracticeMode(TheGameMode->InMode("practice")),
      mAllowOverdrivePhrases(TheGameMode->Property("allow_overdrive_phrases", true)->Int()
      ),
      mEndWithSong(TheGameMode->Property("end_with_song", true)->Int()),
      mForceUseCymbals(TheGameMode->Property("force_use_cymbals", true)->Int()),
      mForceDontUseCymbals(TheGameMode->Property("force_dont_use_cymbals", true)->Int()),
      mAllowAutoVocals(TheGameMode->Property("allow_auto_vocals", true)->Int()),
      mHasSongSections(TheGameMode->Property("has_song_sections", true)->Int()),
      mLoadChars(TheGameMode->Property("load_chars", true)->Int()),
      mLetterbox(TheGameMode->Property("letterbox", true)->Int()),
      mCrowdReacts(TheGameMode->Property("crowd_reacts", true)->Int()),
      mIsPractice(TheGameMode->Property("is_practice", true)->Int()),
      mEnableWhammy(TheGameMode->Property("enable_whammy", true)->Int()),
      mEnableCapstrip(TheGameMode->Property("enable_capstrip", true)->Int()),
      mDisableGuitarFx(TheGameMode->Property("disable_guitar_fx", true)->Int()),
      mDisableKeysFx(TheGameMode->Property("disable_keys_fx", true)->Int()),
      mEnableOverdrive(TheGameMode->Property("enable_overdrive", true)->Int()),
      mEnableCoda(TheGameMode->Property("enable_coda", true)->Int()),
      mCanSolo(TheGameMode->Property("can_solo", true)->Int()),
      mHasBeatMask(TheGameMode->Property("has_beat_mask", true)->Int()),
      mCanLose(TheGameMode->Property("can_lose", true)->Int()),
      mEnableStreak(TheGameMode->Property("enable_streak", true)->Int()),
      mShowStars(TheGameMode->Property("show_stars", true)->Int()),
      mPlayStarSfx(TheGameMode->Property("play_star_sfx", true)->Int()) {}
