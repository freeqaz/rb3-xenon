#pragma once
// The Net class body on its own: every member is a pointer, so it needs only
// forward declarations. Engine TUs that read TheNet (StorePanel::CheckOut)
// include this instead of net/Net.h, whose include list reaches band3 headers
// written in the other obj-macro dialect.
#include "obj/Object.h"

class NetGameData;
class NetSession;
class SessionSearcher;
class Server;
class NetworkEmulator;
class SyncStore;

class Net : public Hmx::Object {
public:
    Net();
    virtual ~Net() {}
    virtual DataNode Handle(DataArray *, bool);

    void Init();
    void Terminate();
    void Poll();
    NetGameData *GetGameData();
    void SetGameData(NetGameData *);
    void ToggleLogging();
    NetSession *GetNetSession() const { return mSession; }
    Server *GetServer() const { return mServer; }
    SessionSearcher *GetSearcher() const { return mSearcher; }

    static void SystemCheckCallback(char const *, char const *, unsigned int);

    // Retail layout, read off TheNet's dynamic initializer (0x82C3EB80, which
    // zeroes 0x28-0x34), Net::Init (0x823E0648) and Net::Terminate
    // (0x823E07B8): no voice-chat manager, no Quazal thread and no overlay.
    NetGameData *mGameData; // 0x28
    NetSession *mSession; // 0x2c
    SessionSearcher *mSearcher; // 0x30
    Server *mServer; // 0x34
    NetworkEmulator *mEmulator; // 0x38
    SyncStore *mSyncStore; // 0x3c
};

void TerminateTheNet();

extern Net TheNet;
