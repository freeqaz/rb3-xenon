#include "game/GameMode.h"
#include "obj/Dir.h"
#include "obj/DataUtl.h"
#include "obj/ObjMacros.h"
#include "os/PlatformMgr.h"
#include "os/System.h"
#include "utl/Symbols.h"
#include "utl/Messages.h"

GameMode *TheGameMode;

void GameModeInit() {
    MILO_ASSERT(TheGameMode == NULL, 0x1A);
    TheGameMode = new GameMode();
}

GameMode::GameMode() {
    SetName("gamemode", ObjectDir::Main());
    SetMode("init");
}

GameMode::~GameMode() {}

bool GameMode::InMode(Symbol target) {
    if (mMode == target)
        return true;

    DataArray *modes = SystemConfig("modes");
    Symbol iter = mMode;
    static Symbol parent_mode("parent_mode");

    while (modes->FindArray(iter)->FindArray(parent_mode, false)) {
        iter = modes->FindArray(iter)->FindArray(parent_mode)->Sym(1);
        if (iter == target)
            return true;
    }

    return false;
}

DataNode GameMode::OnSetMode(const DataArray *a) {
    MILO_ASSERT(a->Size() == 3, 0x45);
    SetMode(a->Sym(2));
    return DataNode(0);
}

void GameMode::SetMode(Symbol mode) {
    if (mMode != mode) {
        DataArray *cfg = SystemConfig("modes");
        static Message exit_msg("exit");
        HandleType(exit_msg);
        mMode = mode;
        DataArray *cloned = cfg->FindArray(mMode)->Clone(true, false, 0);
        static Symbol parent_only("parent_only");
        if (cloned->FindArray(parent_only, false)) {
            if (cloned->FindArray(parent_only)->Int(1)) {
                MILO_FAIL("Trying to set mode %s, which is a parent_only mode!\n", mMode);
            }
        }
        static Symbol parent_mode("parent_mode");
        Symbol iter = mMode;
        while (cfg->FindArray(iter)->FindArray(parent_mode, false)) {
            iter = cfg->FindArray(iter)->FindArray(parent_mode)->Sym(1);
            DataMergeTags(cloned, cfg->FindArray(iter));
        }
        DataMergeTags(cloned, cfg->FindArray("defaults"));
        SetTypeDef(cloned);
        cloned->Release();
        static Message enter_msg("enter");
        HandleType(enter_msg);
        static ModeChangedMsg msg;
        MsgSource::Handle(msg, false);
    }
}

BEGIN_HANDLERS(GameMode)
    HANDLE_EXPR(in_mode, InMode(_msg->Sym(2)))
    // NOTE: rb3-Wii's DEV build also carries HANDLE_EXPR(get_mode, mMode.Str()),
    // but retail X360's GameMode::Handle has exactly TWO handler symbol compares
    // (two static Symbols, two ??__F atexit thunks clearing guard bits 1 and 2)
    // before falling through to the superclass chain -- no get_mode.
    HANDLE(set_mode, OnSetMode)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_SUPERCLASS(MsgSource)
    HANDLE_CHECK(0xBC)
END_HANDLERS
