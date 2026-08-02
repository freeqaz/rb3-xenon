#include "meta_band/MetaPanel.h"
#include "AccomplishmentPanel.h"
#include <list>
#include "AppInlineHelp.h"
#include "AppLabel.h"
#include "AppMiniLeaderboardDisplay.h"
#include "AppScoreDisplay.h"
#include "BandScreen.h"
#include "BandStorePanel.h"
#include "BandStoreUIPanel.h"
#include "Calibration.h"
#include "CampaignCareerLeaderboardPanel.h"
#include "CampaignGoalsLeaderboardChoicePanel.h"
#include "CampaignSongInfoPanel.h"
#include "CharacterCreatorPanel.h"
#include "ChooseColorPanel.h"
#include "ClosetPanel.h"
#include "ContentDeletePanel.h"
#include "ContentLoadingPanel.h"
#include "CustomizePanel.h"
#include "EditSetlistPanel.h"
#include "GameTimePanel.h"
#include "ManageBandPanel.h"
#include "ModifierMgr.h"
#include "MultiSelectListPanel.h"
#include "NewAwardPanel.h"
#include "NextSongPanel.h"
#include "ParentalControlPanel.h"
#include "PassiveMessenger.h"
#include "PatchPanel.h"
#include "PatchSelectPanel.h"
#include "ProfileMgr.h"
#include "RetryAudioPanel.h"
#include "SaveLoadStatusPanel.h"
#include "SelectDifficultyPanel.h"
#include "SessionMgr.h"
#include "SetlistMergePanel.h"
#include "SetlistToStorePanel.h"
#include "SigninScreen.h"
#include "SongSelectPanel.h"
#include "SongSortMgr.h"
#include "StoreInfoPanel.h"
#include "StoreMainPanel.h"
#include "StoreMenuPanel.h"
#include "StoreRootPanel.h"
#include "TexLoadPanel.h"
#include "TokenRedemptionPanel.h"
#include "TrainingPanel.h"
#include "UGCPurchasePanel.h"
#include "UploadErrorMgr.h"
#include "Utl.h"
#include "VoiceoverPanel.h"
#include "game/BandUserMgr.h"
#include "game/GameMode.h"
// Retail RB3-360 built CreditsPanel.cpp's TU with MILO_DEBUG off (see that file's
// header comment: mCheatOn is a MILO_DEBUG-only member absent from retail's layout,
// -4 bytes vs a MILO_DEBUG build). This TU (MetaPanel.cpp) force-defines MILO_DEBUG
// tree-wide via macros.h like every other file, so scope the same undef/redefine
// trick tightly around just this include -- CreditsPanel.h is #pragma once and only
// reached here, so this affects nothing else in this 60-header TU.
#include "macros.h"
#undef MILO_DEBUG
#include "meta/CreditsPanel.h"
#define MILO_DEBUG
#include "meta/HAQManager.h"
#include "meta/HeldButtonPanel.h"
#include "meta/MemcardMgr.h"
#include "meta/Meta.h"
#include "meta/MetaMusicManager.h"
#include "meta/MoviePanel.h"
#include "meta_band/BandPreloadPanel.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/BandUI.h"
#include "meta_band/CampaignGoalsLeaderboardPanel.h"
#include "meta_band/EventDialogPanel.h"
#include "meta_band/InterstitialPanel.h"
#include "meta_band/MainHubPanel.h"
#include "meta_band/MetaNetMsgs.h"
#include "meta_band/MetaPerformer.h"
#include "meta_band/NameGenerator.h"
#include "meta_band/OvershellPanel.h"
#include "net/NetMessage.h"
#include "net/NetSession.h"
#include "net/WiiFriendMgr.h"
#include "rndobj/PostProc.h"
#include "synth/Faders.h"
#include "synth/Synth.h"
#include "ui/UI.h"
#include "ui/UIListProvider.h"
#include "ui/UIScreen.h"
#include "obj/Dir.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"
#include "obj/DataFunc.h"
#include "tour/QuestFilterPanel.h"
#include "tour/TourChallengeResultsPanel.h"
#include "utl/MakeString.h"
#include "utl/Symbols.h"

void UtlInit();

// Classes with no header yet — defined inline for factory registration
// TourDescPanel is NOT a UIPanel and has no filler member.  It is declared for
// real (with its one member) at src/band3/tour/TourDescPanel.cpp:126 as
// `class TourDescPanel : public TexLoadPanel { ... TourDescProvider
// *m_pTourDescProvider; }`, and that declaration is compiler-verified
// sizeof == 0x84 -- exactly what retail's TourDescPanel::NewObject allocates.
// The previous `: public UIPanel` + `char unk_pad[0x18]` stub here was an
// ODR-violating second definition of the same mangled class that happened to
// land on 0x80.  Mirror the real declaration instead of inventing filler.
class TourDescProvider;

class TourDescPanel : public TexLoadPanel {
public:
    TourDescPanel();
    OBJ_CLASSNAME(TourDescPanel);
    NEW_OBJ(TourDescPanel);
    TourDescProvider *m_pTourDescProvider; // 0x54
};

// JoinInvitePanel has exactly ONE 4-byte member, at 0x3c, zero-initialised.
// This is read directly off retail, not inferred: its constructor
// (fn_826308C0, build/45410914/asm/auto_03_826308B4_text.s) ends with
//   li   r29, 0x0  ...  stw r29, 0x3c(r30)     ; r30 == this
// and its virtual-base branch does `addi r3, r3, 0x44` before calling the
// Hmx::Object ctor, pinning the Object subobject at 0x44 and hence the
// vtordisp at 0x40.  So the derived block is [0x3c,0x40) -- one word -- giving
// sizeof = 0x3c + 4 + 4 + 0x28 = 0x6c, which is exactly what retail's
// JoinInvitePanel::NewObject allocates.  The old `char unk_pad[0x8]` was two
// words and therefore provably wrong.  The member's semantics are unrecoverable
// (no oracle decompiles this class -- rb3-Wii carries the same placeholder --
// and retail never reads the field anywhere), so it keeps the house unkNN name.
class JoinInvitePanel : public UIPanel {
public:
    JoinInvitePanel();
    OBJ_CLASSNAME(JoinInvitePanel);
    NEW_OBJ(JoinInvitePanel);
    int unk3c; // 0x3c -- zero-initialised by the retail ctor
};

class WiiFriendsScreen : public UIPanel {
public:
    static void Init();
    OBJ_CLASSNAME(WiiFriendsScreen);
    NEW_OBJ(WiiFriendsScreen);
};

class WiiProfilePanel : public UIPanel {
public:
    WiiProfilePanel();
    OBJ_CLASSNAME(WiiProfilePanel);
    NEW_OBJ(WiiProfilePanel);
    char unk_pad[0x44]; // sizeof(WiiProfilePanel) == 0x9c
};

class WiiFriendsDetailsProvider : public UIListProvider, public Hmx::Object {
public:
    WiiFriendsDetailsProvider();
    virtual ~WiiFriendsDetailsProvider();
    OBJ_CLASSNAME(WiiFriendsDetailsProvider);
    NEW_OBJ(WiiFriendsDetailsProvider);

    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual int NumData() const;
    virtual UIListWidgetState
    SlotElementStateOverride(int, int, class UIListWidget *, UIListWidgetState) const;

    int unk20; // 0x20
    int unk24; // 0x24
    int unk28; // 0x28
    String unk2c; // 0x2c
    char unk38[0x18]; // 0x38
};

class WiiFriendsProvider {
public:
    void Init();
    void Poll();
    int pad[1]; // size > 2 to avoid sda21 addressing
};
extern WiiFriendsProvider TheWiiFriendsProvider;

class WiiInvitationsProvider {
public:
    void Init();
    int pad[1]; // size > 2 to avoid sda21 addressing
};
extern WiiInvitationsProvider TheWiiInvitationsProvider;

bool MetaPanel::sUnlockAll;
bool MetaPanel::sIsPlaytest;
bool MetaPanel::sLaunchedGoalMsgsOnly;

NetMessage *BandEventPreviewMsg::NewNetMessage() { return new BandEventPreviewMsg(); }
NetMessage *TriggerBackSoundMsg::NewNetMessage() { return new TriggerBackSoundMsg(); }
NetMessage *VerifyBuildVersionMsg::NewNetMessage() { return new VerifyBuildVersionMsg(); }
NetMessage *AppendSongToSetlistMsg::NewNetMessage() {
    return new AppendSongToSetlistMsg();
}
NetMessage *RemoveLastSongFromSetlistMsg::NewNetMessage() {
    return new RemoveLastSongFromSetlistMsg();
}

DataNode MetaPanel::ToggleUnlockAll(DataArray *) { return sUnlockAll = !sUnlockAll; }
DataNode MetaPanel::ToggleIsPlaytest(DataArray *) { return sIsPlaytest = !sIsPlaytest; }
DataNode MetaPanel::ToggleLaunchedGoalMsgsOnly(DataArray *) {
    return sLaunchedGoalMsgsOnly = !sLaunchedGoalMsgsOnly;
}

void MetaPanel::Init() {
    MetaInit();
    REGISTER_OBJ_FACTORY(CampaignGoalsLeaderboardPanel);
    REGISTER_OBJ_FACTORY(CampaignCareerLeaderboardPanel);
    REGISTER_OBJ_FACTORY(CampaignGoalsLeaderboardChoicePanel);
    REGISTER_OBJ_FACTORY(CampaignSongInfoPanel);
    REGISTER_OBJ_FACTORY(AccomplishmentPanel);
    REGISTER_OBJ_FACTORY(NewAwardPanel);
    REGISTER_OBJ_FACTORY(BackdropPanel);
    REGISTER_OBJ_FACTORY(BandPreloadPanel);
    REGISTER_OBJ_FACTORY(BandScreen);
    REGISTER_OBJ_FACTORY(BandStorePanel);
    REGISTER_OBJ_FACTORY(BandStoreUIPanel);
    REGISTER_OBJ_FACTORY(CalibrationPanel);
    REGISTER_OBJ_FACTORY(CalibrationWelcomePanel);
    REGISTER_OBJ_FACTORY(CharacterCreatorPanel);
    REGISTER_OBJ_FACTORY(ChooseColorPanel);
    REGISTER_OBJ_FACTORY(ClosetPanel);
    REGISTER_OBJ_FACTORY(ContentDeletePanel);
    REGISTER_OBJ_FACTORY(ContentLoadingPanel);
    REGISTER_OBJ_FACTORY(CreditsPanel);
    REGISTER_OBJ_FACTORY(CustomizePanel);
    REGISTER_OBJ_FACTORY(EditSetlistPanel);
    REGISTER_OBJ_FACTORY(EventDialogPanel);
    REGISTER_OBJ_FACTORY(GameTimePanel);
    REGISTER_OBJ_FACTORY(HeldButtonPanel);
    REGISTER_OBJ_FACTORY(InterstitialPanel);
    REGISTER_OBJ_FACTORY(OvershellPanel);
    REGISTER_OBJ_FACTORY(MainHubPanel);
    REGISTER_OBJ_FACTORY(ManageBandPanel);
    REGISTER_OBJ_FACTORY(MetaPanel);
    REGISTER_OBJ_FACTORY(MoviePanel);
    REGISTER_OBJ_FACTORY(MultiSelectListPanel);
    REGISTER_OBJ_FACTORY(NextSongPanel);
    REGISTER_OBJ_FACTORY(PassiveMessagesPanel);
    REGISTER_OBJ_FACTORY(PatchPanel);
    REGISTER_OBJ_FACTORY(PatchSelectPanel);
    REGISTER_OBJ_FACTORY(ParentalControlPanel);
    REGISTER_OBJ_FACTORY(RetryAudioPanel);
    REGISTER_OBJ_FACTORY(QuestFilterPanel);
    REGISTER_OBJ_FACTORY(TourDescPanel);
    REGISTER_OBJ_FACTORY(TourChallengeResultsPanel);
    REGISTER_OBJ_FACTORY(JoinInvitePanel);
    REGISTER_OBJ_FACTORY(SaveLoadStatusPanel);
    REGISTER_OBJ_FACTORY(SetlistMergePanel);
    REGISTER_OBJ_FACTORY(SetlistToStorePanel);
    REGISTER_OBJ_FACTORY(SelectDifficultyPanel);
    REGISTER_OBJ_FACTORY(SigninScreen);
    REGISTER_OBJ_FACTORY(SongSelectPanel);
    REGISTER_OBJ_FACTORY(StoreInfoPanel);
    REGISTER_OBJ_FACTORY(StoreMainPanel);
    REGISTER_OBJ_FACTORY(StoreMenuPanel);
    REGISTER_OBJ_FACTORY(StoreRootPanel);
    REGISTER_OBJ_FACTORY(TexLoadPanel);
    REGISTER_OBJ_FACTORY(TokenRedemptionPanel);
    REGISTER_OBJ_FACTORY(TrainingPanel);
    REGISTER_OBJ_FACTORY(UGCPurchasePanel);
    REGISTER_OBJ_FACTORY(VoiceoverPanel);
    OvershellPanel::Init();
    WiiFriendsScreen::Init();
    REGISTER_OBJ_FACTORY(WiiFriendsScreen);
    TheWiiFriendsProvider.Init();
    TheWiiInvitationsProvider.Init();
    REGISTER_OBJ_FACTORY(WiiProfilePanel);
    REGISTER_OBJ_FACTORY(WiiFriendsDetailsProvider);
    GameModeInit();
    ModifierMgr::Init();
    SongSortMgr::Init();
    SessionMgr::Init();
    TheMemcardMgr.Init();
    TheProfileMgr.Init();
    MetaPerformer::Init();
    UploadErrorMgr::Init();
    REGISTER_OBJ_FACTORY(AppInlineHelp);
    REGISTER_OBJ_FACTORY(AppScoreDisplay);
    REGISTER_OBJ_FACTORY(AppLabel);
    AppMiniLeaderboardDisplay::Init();
    BandEventPreviewMsg::Register();
    TriggerBackSoundMsg::Register();
    VerifyBuildVersionMsg::Register();
    AppendSongToSetlistMsg::Register();
    RemoveLastSongFromSetlistMsg::Register();
    UtlInit();
    DataRegisterFunc("toggle_unlock_all", ToggleUnlockAll);
    DataRegisterFunc("toggle_playtest_flag", ToggleIsPlaytest);
    DataRegisterFunc("toggle_launched_goal_msgs_only", ToggleLaunchedGoalMsgsOnly);
}

MetaPanel::MetaPanel()
    : mTour(new Tour(SystemConfig("tour"), TheSongMgr, *TheBandUserMgr, true)),
      mCampaign(new Campaign(SystemConfig("campaign"))),
      mNameGenerator(new NameGenerator(SystemConfig("name_generator"))),
      mMetaMusicMgr(new MetaMusicManager(SystemConfig("synth", "metamusic"))),
      mHAQMgr(new HAQManager()), unk58(0), mMusic(0), mSongPreview(TheSongMgr), unkd4(0) {
    mSongPreview.SetName("song_preview", ObjectDir::Main());
    MusicLibrary::Init(mSongPreview);
    mRecentIndices.reserve(3);
    for (int i = 0; i < 3; i++)
        mRecentIndices.push_back(-1);
    ThePlatformMgr.AddSink(this, "xmp_state_changed");
    TheBandUI.AddSink(this, "current_screen_changed");
}

MetaPanel::~MetaPanel() {
    RELEASE(mTour);
    RELEASE(mCampaign);
    RELEASE(mNameGenerator);
    // laneCN-3: retail does NOT release mMetaMusicMgr here -- it emits only FOUR
    // RELEASEs, not five. objdiff alignment is decisive: our 4th release loads
    // -0x94(r30) where retail's 4th loads -0x90(r30) (idx 48 diff_arg), and our
    // 5th (-0x90, the SAME slot retail uses for its 4th) is 9 PURE inserts at idx
    // 57-65 with no target counterpart. So retail still HAS the member at -0x94
    // (otherwise its -0x90 member would have shifted down); it simply never
    // releases it. Leaving RELEASE(mHAQMgr) as the final one.
    RELEASE(mHAQMgr);
    TheBandUI.RemoveSink(this, "current_screen_changed");
}

void MetaPanel::Load() {
    UIPanel::Load();
    DataArray *cfg = SystemConfig("synth", "metamusic", "metamusic_loop");
    DataArray *loopArr = cfg->Array(PickLoopIndex(cfg->Size()));
    String filename(MakeString("%s", loopArr->Str(0)));
    float vol = loopArr->Float(1);
    mMusic = new MetaMusic("metamusic");
    mMusic->Load(filename.c_str(), vol, true, true);
    mSongPreview.Init();
    UpdateMusicMuteState();
}

void MetaPanel::PollForLoading() {
    UIPanel::PollForLoading();
    if (UIPanel::IsLoaded()) {
        mMusic->Poll();
    }
}

bool MetaPanel::IsLoaded() const {
    return UIPanel::IsLoaded() && mMusic && mMusic->Loaded();
}

void MetaPanel::FinishLoad() {
    UIPanel::FinishLoad();
    mMusic->AddFader(TheSynth->Find<Fader>("fade", true));
}

void MetaPanel::Unload() {
    UIPanel::Unload();
    RELEASE(mMusic);
    mSongPreview.Terminate();
    RndPostProc::Reset();
}

void MetaPanel::Draw() {}

void MetaPanel::Poll() {
    UIPanel::Poll();
    mMusic->Poll();
    mSongPreview.Poll();
    TheWiiFriendsProvider.Poll();
    SyncGameTimer();
    UpdatePostProc();
}

void MetaPanel::Enter() {
    UIPanel::Enter();
    TheTaskMgr.SetSecondsAndBeat(TheTaskMgr.UISeconds(), 0.0f, true);
}

void MetaPanel::Exit() {
    UIPanel::Exit();
    mMusic->Stop();

    extern void fn_8250916C(const void*);
    extern const void* lbl_82C926B8;

    fn_8250916C(&lbl_82C926B8);
}

bool MetaPanel::Exiting() const {
    if (GetState() != kDown) {
        return UIPanel::Exiting();
    } else {
        return (mMusic->IsPlaying() && mMusic->IsFading()) || UIPanel::Exiting();
    }
}

void MetaPanel::SyncGameTimer() {
    float s = TheTaskMgr.UISeconds();
    TheTaskMgr.SetSecondsAndBeat(s, s, false);
}

int MetaPanel::PickLoopIndex(int numLoops) {
    int prevSize = mRecentIndices.size();
    int idx;
    while (true) {
        idx = RandomInt(1, numLoops);
        if (numLoops < prevSize + 2)
            return idx;
        int count = 0;
        for (; count < prevSize; count++) {
            if (idx == mRecentIndices[count])
                break;
        }
        if (count == prevSize)
            break;
    }
    mRecentIndices[unk58++] = idx;
    if (unk58 == prevSize)
        unk58 = 0;
    return idx;
}

void MetaPanel::UpdatePostProc() {
    RndPostProc *found = 0;
    UIScreen *screen = TheUI->BottomScreen();
    if (screen) {
        for (std::list<PanelRef>::iterator it = screen->PanelList().begin();
             it != screen->PanelList().end();
             ++it) {
            bool hasProp = false;
            const DataNode *prop;
            bool active = it->mActive && it->mPanel->LoadedDir();
            if (active) {
                prop = it->mPanel->LoadedDir()->Property(postprocess, false);
                if (prop)
                    hasProp = true;
            }
            if (hasProp) {
                RndPostProc *pp = dynamic_cast<RndPostProc *>(prop->GetObj());
                if (pp)
                    found = pp;
            }
        }
    }
    if (found)
        found->Select();
}

void MetaPanel::OnSendBackSoundMsgToAll() {
    TriggerBackSoundMsg msg;
    TheNetSession->SendMsgToAll(msg, kReliable);
}

void MetaPanel::UpdateMusicMuteState() {
    if (mMusic) {
        if (unkd4)
            mMusic->Mute();
        else
            mMusic->UnMute();
    }
}

DataNode MetaPanel::OnMsg(const CurrentScreenChangedMsg &msg) {
    UpdateMetaMusic(msg.GetScreen());
    return DataNode(kDataUnhandled, 0);
}

DataNode MetaPanel::OnMsg(const XMPStateChangedMsg &msg) {
    unkd4 = msg.Success();
    UpdateMusicMuteState();
    return 0;
}

void MetaPanel::UpdateMetaMusic(Symbol screen) {
    MILO_ASSERT(TheMetaMusicManager, 0x219);
    if (mMusic) {
        Symbol scene = TheMetaMusicManager->GetSceneForScreen(screen);
        if (scene != gNullStr) {
            MetaMusicScene *pScene = TheMetaMusicManager->GetScene(scene);
            MILO_ASSERT(pScene, 0x224);
            mMusic->SetScene(pScene);
        } else {
            mMusic->SetScene(0);
        }
    }
}

BEGIN_HANDLERS(MetaPanel)
    HANDLE_EXPR(meta_music, mMusic)
    HANDLE_ACTION(send_back_sound_msg_to_all, OnSendBackSoundMsgToAll())
    HANDLE_ACTION(sync_game_timer, SyncGameTimer())
    HANDLE_MESSAGE(CurrentScreenChangedMsg)
    HANDLE_MESSAGE(XMPStateChangedMsg)
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_CHECK(0x246)
END_HANDLERS
