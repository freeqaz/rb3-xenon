#include "App.h"
#include "ChecksumData_xbox.h"
#include "bandobj/Band.h"
#include "bandobj/PatchDir.h"
#include "beatmatch/BeatMatch.h"
#include "char/Char.h"
#include "game/BandUserMgr.h"
#include "game/Game.h"
#include "game/GameMicManager.h"
#include "game/PresenceMgr.h"
#include "meta/Achievements.h"
#include "meta/FixedSizeSaveable.h"
#include "meta_band/AccomplishmentManager.h"
#include "meta_band/AssetMgr.h"
#include "meta_band/AuditionMgr.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/CharCache.h"
#include "meta_band/CharSync.h"
#include "meta_band/ClosetMgr.h"
#include "meta_band/ContextChecker.h"
#include "meta_band/LessonMgr.h"
#include "meta_band/MetaPanel.h"
#include "meta_band/MusicLibrary.h"
#include "meta_band/PrefabMgr.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/SaveLoadManager.h"
#include "meta_band/TrainingMgr.h"
#include "meta_band/UIStats.h"
#include "net/NetCore.h"
#include "net_band/EntityUploader.h"
#include "net_band/RockCentral.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Msg.h"
#include "movie/Movie.h"
#include "movie/Splash.h"
#include "obj/Task.h"
#include "os/Archive.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/FileCache.h"
#include "os/PlatformMgr.h"
#include "os/System.h"
#include "os/Timer.h"
#include "os/UsbMidiGuitar.h"
#include "os/UsbMidiKeyboard.h"
#include "rndobj/Rnd.h"
#include "synth/Synth.h"
#include "tour/QuestManager.h"
#include "track/TrackDir.h"
#include "ui/UI.h"
#include "utl/Cheats.h"
#include "utl/Loader.h"
#include "utl/Option.h"
#include "world/World.h"
#include "xdk/xapilibi/errhandlingapi.h"

// Archive permissions handed out as boot progresses. Retail keeps them as
// three adjacent .rdata arrays at 0x82000980.
static const int initArk[] = { 2 };
static const int charArk[] = { 5 };
static const int regularArks[] = { 3, 4, 5, 6, 8, 9, 10 };

App::~App() { TheDebug.Exit(0, true); }

void App::DrawRegular() {
    TheRnd.BeginDrawing();
    TheUI->Draw();
    TheRnd.EndDrawing();
}

// The retail frame loop. It has no exit; App::Run only reaches it through the
// unhandled-exception filter below.
void App::RunWithoutDebugging() {
    while (true) {
        SystemPoll(false);
        TheUIStats->Poll();
        TheAchievements->Poll();
        TheAccomplishmentMgr->Poll();
        PrefabMgr::GetPrefabMgr()->Poll();
        TheSaveLoadMgr->Poll();
        TheProfileMgr.Poll();
        TheMusicLibrary->Poll();
        TheSynth->Poll();
        TheNet.Poll();
        TheRockCentral.Poll();
        TheEntityUploader.Poll();
        TheAuditionMgr->Poll();
        TheUI->Poll();
        TheTaskMgr.Poll();
        DrawRegular();
    }
}

App *gApp; // 0x82CBC600
Debug::ModalCallbackFunc *gRealCallback; // 0x82CBC604
FileCache *gPersistentCache; // 0x82CBC608
LPTOP_LEVEL_EXCEPTION_FILTER gOldExceptionFilter; // 0x82CBC60C

// App::Run installs this, then faults on purpose: the game loop runs from
// inside the unhandled-exception dispatch. The filter puts the previous filter
// back and never returns. Retail App::Run stores the previous filter only after
// the faulting store, so the filter reinstalls whatever gOldExceptionFilter
// already held (null at that point).
static LONG AppExceptionFilter(EXCEPTION_POINTERS *) {
    SetUnhandledExceptionFilter(gOldExceptionFilter);
    gApp->RunWithoutDebugging();
}

void App::Run() {
    gApp = this;
    LPTOP_LEVEL_EXCEPTION_FILTER old = SetUnhandledExceptionFilter(AppExceptionFilter);
    *(int *)0 = 1;
    gOldExceptionFilter = old;
}

// Installed through Debug::SetModalCallback, so it has no direct callers.
// The logging branch of notify level 0 compiles to nothing in retail.
void AppDebugModal(bool &b, char *msg, bool b2) {
    if (!b) {
        static DataNode &notify_level = DataVariable("notify_level");
        int notif_lvl = notify_level.Int();
        if (notif_lvl == 2) {
            gRealCallback(b, msg, b2);
            return;
        } else if (notif_lvl == 1) {
            Hmx::Object *disp = ObjectDir::Main()->Find<Hmx::Object>("cheat_display", false);
            if (disp) {
                static Message show("show_prio", 0, 0);
                show[0] = msg;
                show[1] = DataNode(200);
                disp->Handle(show, false);
            }
        }
    } else
        gRealCallback(b, msg, b2);
}

App::App(int argc, char **argv) {
    Timer init_time;
    init_time.Start();
    EnableKeyCheats(false);
    SetFileChecksumData();
    SetSongMidiChecksumData();
    SystemPreInit(argc, argv, "config/band_preinit_keep.dta");
    TheRnd.PreInit();
    SynthPreInit();
    Movie::Init();
    TheRnd.SetClearColor(Hmx::Color(0, 0, 0, 1));
    Splash spl;
    if (OptionBool("fast", false)) {
        spl.SetWaitForSplash(false);
    }
    if (ThePlatformMgr.GetRegion() == kRegionNA) {
        spl.AddScreen("ui/startup/eng/startup_autosave_esrb_keep.milo", 5000);
    } else {
        spl.AddScreen("ui/startup/eng/startup_autosave_keep.milo", 5000);
    }
    if (spl.mThreaded) {
        spl.AddScreen("ui/startup/startup_movie_keep.milo", 4000);
    } else {
        spl.AddScreen("ui/startup/startup_harmonix_keep.milo", 4000);
    }
    spl.AddScreen("ui/startup/startup_madcatz_keep.milo", 4000);
    spl.PrepareNext();
    spl.BeginSplasher();
    init_time.SplitMs();
    if (TheArchive) {
        TheArchive->SetArchivePermission(1, initArk);
    }
    spl.PrepareRemaining();
    SystemInit("config/band_keep.dta");
    if (TheSplasher)
        TheSplasher->Poll();
    spl.Suspend();
    TheRnd.Init();
    spl.Resume();
    if (TheSplasher)
        TheSplasher->Poll();
    static DataNode &notify_level = DataVariable("notify_level");
    notify_level = DataNode(2);
    gRealCallback = TheDebug.SetModalCallback(AppDebugModal);
    FixedSizeSaveable::Init(152, 5688);
    BandUserMgrInit();
    if (TheSplasher)
        TheSplasher->Poll();
    TheNet.Init();
    if (TheSplasher)
        TheSplasher->Poll();
    TheRockCentral.Init(false);
    if (TheSplasher)
        TheSplasher->Poll();
    TheEntityUploader.Init();
    if (TheSplasher)
        TheSplasher->Poll();
    SynthInit();
    if (TheSplasher)
        TheSplasher->Poll();
    GameMicManager::Init();
    UsbMidiKeyboard::Init();
    UsbMidiGuitar::Init();
    if (TheSplasher)
        TheSplasher->Poll();
    {
        ObjDirPtr<ObjectDir> bank;
        bank.LoadFile(
            SystemConfig("sound", "banks", "common")->Str(1), false, true, kLoadFront, false
        );
        TheSynth->SetDir(bank);
        if (TheSplasher)
            TheSplasher->Poll();
    }
    SaveLoadManager::Init();
    CharInit();
    if (TheSplasher)
        TheSplasher->Poll();
    BeatMatchInit();
    if (TheSplasher)
        TheSplasher->Poll();
    TrackInit();
    if (TheSplasher)
        TheSplasher->Poll();
    WorldInit();
    if (TheSplasher)
        TheSplasher->Poll();
    BandInit();
    if (TheSplasher)
        TheSplasher->Poll();
    TheSongMgr.Init();
    MetaPanel::Init();
    if (TheSplasher)
        TheSplasher->Poll();
    GameInit();
    if (TheSplasher)
        TheSplasher->Poll();
    ContextCheckerInit();
    if (TheSplasher)
        TheSplasher->Poll();
    TheSynth->SetDolby(false, true);
    if (TheSplasher)
        TheSplasher->Poll();
    CharCache::Init();
    PrefabMgr::Init(nullptr);
    CharSync::Init(nullptr);
    AssetMgr::Init();
    LessonMgr::Init();
    ClosetMgr::Init();
    TrainingMgr::Init();
    DataArray *cacheCfg = SystemConfig("persistent_filecache");
    if (cacheCfg) {
        gPersistentCache = new FileCache(cacheCfg->Int(1), kLoadFront, false);
        gPersistentCache->StartSet(0);
        for (int i = 2; i < cacheCfg->Size(); i++) {
            gPersistentCache->Add(cacheCfg->Str(i), 1, "");
        }
        gPersistentCache->EndSet();
        gPersistentCache->PollUntilLoaded();
    }
    TheUI->Init();
    TheCharSync->UpdateCharCache();
    if (TheSplasher)
        TheSplasher->Poll();
    TheAuditionMgr->Init();
    ThePresenceMgr.Init();
    if (TheSplasher)
        TheSplasher->Poll();
    TheQuestMgr.Init(SystemConfig("tour"));
    if (TheSplasher)
        TheSplasher->Poll();
    PatchDir::Init();
    if (TheSplasher)
        TheSplasher->Poll();
    if (!NewFile("charnames.zbm", 0x10002)) {
        ThePlatformMgr.SetDiskError(kDiskError);
    }
    if (TheArchive) {
        TheArchive->SetArchivePermission(1, charArk);
    }
    TheLoadMgr.PollUntilEmpty();
    init_time.SplitMs();
    if (TheArchive) {
        TheArchive->SetArchivePermission(7, regularArks);
        Archive::DebugArkOrder();
    }
    spl.EndSplasher();
    EnableKeyCheats(true);
    AutoGlitchReport::EnableCallback();
}
