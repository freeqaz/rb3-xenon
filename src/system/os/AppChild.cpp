#include "os/AppChild.h"
#include "HolmesClient.h"
#include "NetStream.h"
#include "NetworkSocket.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "utl/Option.h"
#include "obj/DataFunc.h"

AppChild *TheAppChild;

AppChild::~AppChild() { delete mStream; }

// MILO_DEBUG force-define trap (see CLAUDE.md).  rb3-Wii's os/AppChild.cpp carries
//     #ifndef MILO_DEBUG
//     NetAddress HolmesResolveIP() { return NetAddress(); }   // "why"
//     #endif
// so in a RETAIL (non-dev) build the Holmes name resolver collapses to a trivial
// {0,0} that the compiler inlines and constant-folds.  Retail RB3-360 asm proves
// this is what shipped: ??0AppChild@@AAA@PBD@Z (target 108 B) contains NO
// `bl ?HolmesResolveIP@@YA?AVNetAddress@@XZ` and no mIP load -- it just does
// `li r29,0 / stw r29,0x58(r31)` for addr.mIP.  Our tree force-defines MILO_DEBUG,
// which deletes this stub and leaves the real os/HolmesClient.cpp resolver, costing
// a call, an `mr r11,r3` and an `lwz r11,0(r11)` (our body was 124 B).
//
// Gated on HX_NATIVE rather than MILO_DEBUG on purpose: os/HolmesClient.cpp also
// defines this symbol and the native port is the only target that actually links,
// so the native build must keep the real resolver and a single definition.
#ifndef HX_NATIVE
NetAddress HolmesResolveIP() { return NetAddress(); }
#endif

AppChild::AppChild(const char *str) : mEnabled(1), mStream(0), mSync(0) {
    NetAddress addr(HolmesResolveIP().mIP, 0x11BF);
    NetStream *stream = new NetStream();
    stream->ClientConnect(addr);
    mStream = stream;
    MILO_LOG("AppChild::Connect\n");
}

void AppChild::Sync() {
    short lol = 1;
    *mStream << lol;
    mStream->Flush();
    mSync = true;
}

void AppChild::Sync(unsigned short sh) {
    *mStream << sh;
    mStream->Flush();
    mSync = true;
}

DataNode EnableAppChild(DataArray *) {
    if (TheAppChild)
        TheAppChild->SetEnabled(true);
    return 0;
}

DataNode DisableAppChild(DataArray *) {
    if (TheAppChild)
        TheAppChild->SetEnabled(false);
    return 0;
}

DataNode SyncAppChild(DataArray *) {
    if (TheAppChild)
        TheAppChild->Sync();
    return 0;
}

void AppChild::Init() {
    bool appchildbool = OptionBool("app_child", false);
    if (appchildbool) {
        MILO_ASSERT(!TheAppChild, 0x3C);
        TheAppChild = new AppChild(OptionStr("pipe_name", 0));
    }
    DataVariable("app_child") = appchildbool;
    DataRegisterFunc("enable_app_child", EnableAppChild);
    DataRegisterFunc("disable_app_child", DisableAppChild);
    DataRegisterFunc("sync_app_child", SyncAppChild);
}

void AppChild::Terminate() { RELEASE(TheAppChild); }

void AppChild::Poll() {
    static Symbol tool_sync_cam("tool_sync_cam");
    if (mStream) {
        while (mEnabled && !mSync) {
            DataArrayPtr cmd;
            *mStream >> cmd;
            cmd->Execute();
        }
        mSync = false;
    }
}
