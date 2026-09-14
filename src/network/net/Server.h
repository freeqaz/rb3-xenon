#pragma once
#include "Services/Data.h"
#include <vector>
#include "obj/Msg.h"
#include "os/CritSec.h"
#include "os/OnlineID.h"
#include "network/Services/AccountManagementClient.h"
#include "network/Services/ServiceClient.h"
#include "network/Services/MatchMakingClient.h"
#include "network/Services/SecureConnectionClient.h"
#include "network/Services/CustomMatchMakingClient.h"
#include "network/Platform/Holder.h"

class Server : public MsgSource {
public:
    Server();
    virtual DataNode Handle(DataArray *, bool);
    virtual ~Server() {}
    virtual void Init();
    virtual void Terminate() {}
    virtual void Poll() = 0;
    virtual void Login() = 0;
    virtual void Logout() = 0;
    virtual bool IsConnected() { return mLoginState == 2; }
    virtual bool IsLoggingIn() { return mLoginState == 1; }
    virtual int GetPlayerID(int) {
        MILO_FAIL("not implemented for this platform");
        return 0;
    }
    // Slot [8] -- RETAIL-PROVEN SLOT, NAME UNATTESTED (lane W16-G, 2026-09-14).
    // The retail Server vtable (0x820577bc, 19 slots) holds a bare `blr` here
    // and XboxServer's override (0x823edd88, 120 B) erases the vector, then if
    // IsConnected() push_backs four unsigned ints from this+0x5c..0x68. The
    // slot's existence is proven by RockCentral::OnMsg(ServerStatusChangedMsg)
    // (retail fn_824FA350), which vcalls GetCompetitionClient at 0x38 (=14) and
    // GetPersistentStoreClient at 0x34 (=13) -- one slot above the rb3-Wii
    // header order (13 / 12). No caller in this tree uses the slot, so the
    // name is a placeholder chosen from the body's behaviour; the rb3-Wii
    // Server.h does not declare it (Wii had no such virtual).
    // Retail's table is 19 long, ours is now 21: two of the six tail virtuals
    // (GetSecureConnectionClient..GetCustomAuthData) are absent from retail,
    // but no call site in the binary reaches a Server slot above 14, so WHICH
    // two is unprovable from bytes -- the tail is left as the Wii oracle has it.
    virtual void GetPlayerIDs(std::vector<unsigned int> &) {}
    // fix all of these return types
    virtual int GetFriendsClient() {
        MILO_FAIL("not implemented for this platform");
        return 0;
    }
    virtual int GetMessagingClient() {
        MILO_FAIL("not implemented for this platform");
        return 0;
    }
    virtual Quazal::MatchMakingClient *GetMatchMakingClient() {
        MILO_FAIL("not implemented for this platform");
        return 0;
    }
    virtual Quazal::CustomMatchMakingClient *GetCustomMatchMakingClient() {
        MILO_FAIL("not implemented for this platform");
        return 0;
    }
    virtual Quazal::ServiceClient *GetPersistentStoreClient() {
        MILO_FAIL("not implemented for this platform");
        return nullptr;
    }
    virtual int GetCompetitionClient() {
        MILO_FAIL("not implemented for this platform");
        return 0;
    }
    virtual Quazal::SecureConnectionClient *GetSecureConnectionClient() {
        MILO_FAIL("not implemented for this platform");
        return 0;
    }
    virtual Quazal::AccountManagementClient *GetAccountManagementClient() {
        MILO_FAIL("not implemented for this platform");
        return 0;
    }
    virtual unsigned int GetMasterProfileID() {
        MILO_FAIL("not implemented for this platform");
        return 0;
    }
    virtual int CreateProfile(String) {
        MILO_FAIL("not implemented for this platform");
        return 0;
    }
    virtual int DeleteProfile(OnlineID &) {
        MILO_FAIL("not implemented for this platform");
        return 0;
    }
    virtual Quazal::Data *GetCustomAuthData() {
        MILO_FAIL("not implemented for this platform");
        static Quazal::AnyObjectHolder<Quazal::Data, Quazal::String> emptyDataHolder;
        return (Quazal::Data *)&emptyDataHolder;
    }

    CriticalSection mLogoutCritSec; // 0x1c
    int mLoginState; // 0x3c - enum
    const char *mKey; // 0x40
    String mAddress; // 0x44
    int mPort; // 0x50
    int mPadLoggingIn; // 0x54
    unsigned int mPlayerIDLoggingIn; // 0x58
    unsigned int mPlayerIDs[4]; // 0x5c
};

extern Server &TheServer;

DECLARE_MESSAGE(ServerStatusChangedMsg, "server_status_changed");
bool Success() const { return mData->Int(2); }
END_MESSAGE