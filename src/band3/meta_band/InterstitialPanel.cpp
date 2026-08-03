#include "obj/ObjMacros.h"
#include "meta_band/InterstitialPanel.h"
#include "meta/DeJitterPanel.h"
#include "obj/Data.h"
#include "ui/PanelDir.h"
#include "ui/UIPanel.h"
#include "utl/Messages4.h"
#include "utl/Symbols.h"

InterstitialPanel::InterstitialPanel() : mCamshotDone(0), unk88(0), mShowing(1) {}

void InterstitialPanel::Load() { UIPanel::Load(); }

void InterstitialPanel::Enter() {
    DeJitterPanel::Enter();
    mCamshotDone = 0;
    unk88 = 0;
}

bool InterstitialPanel::Exiting() const {
    return UIPanel::Exiting() || !mCamshotDone || unk88 < 3;
}

void InterstitialPanel::Unload() {
    if (mLoader && mLoader->IsLoaded()) {
        mDir = dynamic_cast<PanelDir *>(mLoader->GetDir());
        RELEASE(mLoader);
    }
    UIPanel::Unload();
}

void InterstitialPanel::Draw() {
    if (mCamshotDone)
        unk88++;
    else if (mShowing)
        UIPanel::Draw();
}

void InterstitialPanel::SetCamshotDone() { mCamshotDone = true; }

BEGIN_HANDLERS(InterstitialPanel)
    HANDLE_ACTION(transition_camshot_done, SetCamshotDone())
    HANDLE_ACTION(set_showing, mShowing = _msg->Int(2))
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_CHECK(0x62)
END_HANDLERS

BackdropPanel::BackdropPanel() : mOutroDone(0) {}

void BackdropPanel::Enter() {
    DeJitterPanel::Enter();
    mOutroDone = true;
}

void BackdropPanel::Exit() {
    mOutroDone = false;
    // STORAGE-CLASS divergence: retail builds the message as a FUNCTION-LOCAL
    // STATIC here, not as the file-scope `vignette_outro_msg` global that
    // utl/Messages4.h declares (which is what the rb3-Wii oracle uses, so a
    // source diff shows nothing).  Read off the retail body at 0x8261FAC8:
    // guard word lbl_82E01040 bit 0x1, the Message object at lbl_82E01038, the
    // Symbol built as a STACK TEMP at r31+0x50 via ??0Symbol@@QAA@PBD@Z, and an
    // atexit(??__F thunk @0x8261FB84).  One guard bit + a stack Symbol temp is
    // the `static Message x("literal")` form, not the two-static
    // `static Symbol s; static Message m(s)` form -- see the measured
    // discrimination in bandtrack/TrackPanel.cpp:646.
    static Message vignette_outro_msg("vignette_outro");
    mDir->Handle(vignette_outro_msg, true);
    UIPanel::Exit();
}

bool BackdropPanel::Exiting() const { return UIPanel::Exiting() || !mOutroDone; }

void BackdropPanel::SetOutroDone() { mOutroDone = true; }

BEGIN_HANDLERS(BackdropPanel)
    HANDLE_ACTION(vignette_outro_done, SetOutroDone())
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_CHECK(0x8A)
END_HANDLERS
