#pragma once
#include "NetworkEmulator.h"
#include "meta_band/BandNetGameData.h"
#include "net/NetSession.h"
#include "net/Server.h"
#include "net/SessionSearcher.h"
#include "net/SyncStore.h"
#include "net/VoiceChatMgr.h"
#include "obj/Object.h"
#include "rndobj/Overlay.h"

class Net : public Hmx::Object {
public:
    Net();
    virtual ~Net() {}
    virtual DataNode Handle(DataArray *, bool);

    void Init();
    void Terminate();
    void Poll();
    NetGameData *GetGameData();
    void UpdateNetOverlay();
    void SetGameData(NetGameData *);
    void ToggleLogging();
    NetSession *GetNetSession() const { return mSession; }
    Server *GetServer() const { return mServer; }
    SessionSearcher *GetSearcher() const { return mSearcher; }

    static void SystemCheckCallback(char const *, char const *, unsigned int);

    NetGameData *mGameData; // 0x1c
    NetSession *mSession; // 0x20
    SessionSearcher *mSearcher; // 0x24
    Server *mServer; // 0x28
    NetworkEmulator *mEmulator; // 0x2c
    VoiceChatMgr *mVoiceChatMgr; // 0x30
    SyncStore *mSyncStore; // 0x34
    unsigned char *mThreadStack; // 0x38
    int unk3c;
    int unk40;
    int unk44;
    // rb3-Wii had `OSThread mThread` (Wii RVL SDK) here, spanning 0x48..0x360.
    // That pulled the Wii revolution/ SDK into a 360 build. The retail X360 Net
    // uses Xbox threading and almost certainly a different layout past 0x48, but
    // the only consumer (game/Singer.cpp) just calls GetNetSession() -> mSession
    // (0x20), which is BEFORE this field, so the thread representation does not
    // affect its codegen. Keep an opaque byte span of the Wii size so the few
    // trailing members keep their offsets until the network layer is ported.
    unsigned char mThread[0x360 - 0x48]; // 0x48
    RndOverlay *mNetOverlay; // 0x360
};

void TerminateTheNet();

extern Net TheNet;