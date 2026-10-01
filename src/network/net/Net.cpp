#include "net/Net.h"
#include "NetworkEmulator.h"
#include "Platform/MemoryManager.h"
#include "Platform/SystemChecker.h"
#include "net/NetLog.h"
#include "net/NetMessenger.h"
#include "net/NetSession.h"
#include "net/Server.h"
#include "net/SessionSearcher.h"
#include "net/SyncStore.h"
#include "obj/Dir.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "utl/MemMgr.h"

// Retail TU: 0x823E02F0-0x823E08A0 (deleting dtor, TerminateTheNet, the two
// Quazal allocator hooks, Poll, SetGameData, Handle, Init, Terminate), plus
// TheNet's dynamic initializer at 0x82C3EB80 and its atexit at 0x82C44C90.

// Never emitted out of line in retail: TheNet's initializer (0x82C3EB80)
// inlines it, zeroing 0x28, 0x2c, 0x30 and 0x34 after Hmx::Object().
inline Net::Net() : mGameData(0), mSession(0), mSearcher(0), mServer(0) {}

Net TheNet;

// 0x823E0348: `lis/addi r3, TheNet; b Net::Terminate`.
void TerminateTheNet() { TheNet.Terminate(); }

// 0x823E0358: the block carries its own size in a leading word.
void *QuazalMemAlloc(unsigned long size) {
    size += 4;
    unsigned long *mem = (unsigned long *)MemOrPoolAlloc(size);
    *mem = size;
    return mem + 1;
}

// 0x823E0398: the size word sits just before the pointer Quazal holds.
void QuazalMemFree(void *v) {
    unsigned long *mem = (unsigned long *)v - 1;
    MemOrPoolFreeSTL(*mem, mem);
}

// 0x823E03A8. No overlay update: retail calls exactly these five polls.
void Net::Poll() {
    TheNetMessenger.Poll();
    mSyncStore->Poll();
    mSession->Poll();
    mSearcher->Poll();
    mServer->Poll();
}

// 0x823E0420
void Net::SetGameData(NetGameData *data) {
    mGameData = data;
    mSearcher->AllocateNetSearchResults();
}

// Inlined into Handle (0x823E04B0 flips NetLog's active byte); retail has no
// out-of-line body.
inline void Net::ToggleLogging() { NetLog.SetActive(!NetLog.IsActive()); }

// The failure report is compiled out, so the hook Init installs is an empty
// function (retail stores the shared empty body 0x826C3888).
void Net::SystemCheckCallback(const char *, const char *, unsigned int) {}

BEGIN_HANDLERS(Net)
    HANDLE_ACTION(toggle_logging, ToggleLogging())
    HANDLE_MEMBER_PTR(mSearcher)
    HANDLE_MEMBER_PTR(mSession)
    HANDLE_CHECK(0x17A)
END_HANDLERS

// 0x823E0648
void Net::Init() {
    SetName("net", ObjectDir::Main());
    Quazal::MemoryManager::s_fcnMalloc = QuazalMemAlloc;
    Quazal::MemoryManager::s_fcnFree = QuazalMemFree;
    Quazal::SystemChecker::s_pfGetSystemCheckInfo = SystemCheckCallback;
    mSyncStore = new SyncStore();
    mSession = NetSession::New();
    mSearcher = SessionSearcher::New();
    TheServer.Init();
    mServer = &TheServer;
    mEmulator = new NetworkEmulator();
    TheNetMessenger.Init();
    TheDebug.AddExitCallback(TerminateTheNet);
}

// 0x823E07B8
void Net::Terminate() {
    delete mEmulator;
    TheServer.Terminate();
    delete mSearcher;
    delete mSession;
    delete mSyncStore;
    TheDebug.RemoveExitCallback(TerminateTheNet);
}
