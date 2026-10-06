#include "App.h"
#include "meta/Achievements.h"
#include "meta_band/AccomplishmentManager.h"
#include "meta_band/AuditionMgr.h"
#include "meta_band/MusicLibrary.h"
#include "meta_band/PrefabMgr.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/SaveLoadManager.h"
#include "meta_band/UIStats.h"
#include "net/NetCore.h"
#include "net_band/EntityUploader.h"
#include "net_band/RockCentral.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Msg.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "os/System.h"
#include "rndobj/Rnd.h"
#include "synth/Synth.h"
#include "ui/UI.h"
#include "xdk/xapilibi/errhandlingapi.h"

// Written so far: the debug-modal hook, ~App, DrawRegular, the frame loop
// (RunWithoutDebugging), Run and its unhandled-exception filter. App::App
// (0x82270E68) is still unwritten.

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
Debug::ModalCallbackFunc *gRealCallback;
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
