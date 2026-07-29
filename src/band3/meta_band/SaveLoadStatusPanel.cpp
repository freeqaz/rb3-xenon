#include "obj/ObjMacros.h"
#include "meta_band/SaveLoadStatusPanel.h"
#include "meta_band/SaveLoadManager.h"
#include "obj/ObjMacros.h"
#include "os/PlatformMgr.h"
#include "ui/UIPanel.h"
#include "utl/Messages.h"
#include "utl/Messages2.h"
#include "utl/Messages4.h"

SaveLoadStatusPanel::SaveLoadStatusPanel() : unk38(0), unk70(0), unk71(0) {}

SaveLoadStatusPanel::~SaveLoadStatusPanel() {}

void SaveLoadStatusPanel::FinishLoad() {
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
        unk78.Split();
        if (unk78.Ms() >= 3000.0f) {
            unk71 = false;
            Handle(hide_physical_write_icon_msg, true);
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
    if (unk38) {
        unk40.Split();
        if (unk40.Ms() >= 1000.0f) {
            unk38 = false;
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
            Handle(show_physical_write_icon_msg, false);
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