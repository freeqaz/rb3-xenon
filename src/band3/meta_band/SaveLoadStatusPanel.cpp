#include "obj/ObjMacros.h"
#include "meta_band/SaveLoadStatusPanel.h"
#include "meta_band/SaveLoadManager.h"
#include "obj/ObjMacros.h"
#include "os/PlatformMgr.h"
#include "ui/UIPanel.h"
#include "ui/PanelDir.h"
#include "rndobj/EventTrigger.h"
#include "utl/Messages.h"
#include "utl/Messages2.h"
#include "utl/Messages4.h"

SaveLoadStatusPanel::SaveLoadStatusPanel() : unk38(0), unk70(0), unk71(0) {}

SaveLoadStatusPanel::~SaveLoadStatusPanel() {}

void SaveLoadStatusPanel::FinishLoad() {
    RndDir *icons = mDir->Find<RndDir>("saveload_icons", true);
    EventTrigger *start = icons->Find<EventTrigger>("start_saving.trig", true);
    EventTrigger *finish = icons->Find<EventTrigger>("finish_saving.trig", true);
    start->SetAnimRate(RndAnimatable::k30_fps_ui);
    finish->SetAnimRate(RndAnimatable::k30_fps_ui);
    UIPanel::FinishLoad();
#ifndef HX_NATIVE
    // TheSaveLoadMgr (SaveLoadManager) is in _NATIVE_FORK_EXCLUDE -> the pointer is
    // a zeroed DATA stub (null) on native, so AddSink derefs null. The save/load
    // status panel (Wii memcard write icon) has no offline meaning. Mirrors the
    // BandUI::Init / ProfileMgr::Init TheSaveLoadMgr AddSink gating.
    TheSaveLoadMgr->AddSink(this);
#endif
}

void SaveLoadStatusPanel::Draw() {
    UIPanel::Draw();
    if (unk71 && !unk70) {
        if (unk78.SplitMs() >= 3000.0f) {
            unk71 = false;
            static Message hide_physical_write_icon("hide_physical_write_icon");
            Handle(hide_physical_write_icon, true);
            QueueDeactivation();
        }
    }
    PollDeactivation();
}

void SaveLoadStatusPanel::Unload() {
    TheSaveLoadMgr->RemoveSink(this);
    UIPanel::Unload();
}

void SaveLoadStatusPanel::CancelDeactivation() {
    if (unk38)
        unk38 = false;
}

void SaveLoadStatusPanel::QueueDeactivation() {
    if (!unk38) {
        unk38 = true;
        unk40.Restart();
    }
}

void SaveLoadStatusPanel::PollDeactivation() {
    // Retail 0x82631EB0: Timer::SplitMs() out of line, and the message is a
    // function-local static.
    if (unk38) {
        if (unk40.SplitMs() >= 1000.0f) {
            unk38 = false;
            static Message deactivate_msg("deactivate");
            Handle(deactivate_msg, true);
        }
    }
}

DataNode SaveLoadStatusPanel::OnMsg(const SaveLoadMgrStatusUpdateMsg &msg) {
    switch (msg->Int(2)) {
    case 1:
        CancelDeactivation();
        if (!unk70) {
            unk70 = true;
            unk71 = true;
            unk78.Restart();
            static Message show_physical_write_icon("show_physical_write_icon");
            Handle(show_physical_write_icon, false);
        }
        break;
    case 2:
    case 5:
        unk70 = false;
        break;
    }
    return 0;
}

BEGIN_HANDLERS(SaveLoadStatusPanel)
    HANDLE_MESSAGE(SaveLoadMgrStatusUpdateMsg)
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_CHECK(0xA3)
END_HANDLERS