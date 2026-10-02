#pragma once
#include "net/Server.h"
#include "os/PlatformMgr.h"
#include "Platform/String.h"
#include "Protocol/ProtocolCallContext.h"
#include "Services/MatchMakingClient.h"
#include "Services/SecureConnectionClient.h"
#include "Services/ServiceClient.h"

// The Quazal objects the Xbox server owns. Quazal's NetZ code is not decompiled
// here (its units are map-only scaffolds), so every function below is declared
// from its call sites in the XboxServer code; the retail addresses are given so
// each can be checked. Retail keeps no names for any of them -- the names are
// descriptive and the parameter lists are read off the call sites.
namespace Quazal {

    // XboxServer +0x6c. Its word at +0x8 is the principal ID XboxServer::Poll
    // stores for the pad that just logged in.
    class Credentials {
    public:
        unsigned int unk0; // 0x0
        unsigned int unk4; // 0x4
        unsigned int mPrincipalID; // 0x8
    };

    // XboxServer +0x7c. sizeof 0xB0 (`li r3, 0xb0` in XboxServer::Init), ctor
    // 0x82A87F20. The calls land in and around Quazal's
    // XboxLSP/Client/LSPBackEndServices.cpp (pinned 0x82A880A8-0x82A887F4).
    class LSPBackEndServices : public RootObject {
    public:
        LSPBackEndServices(); // 0x82A87F20
        virtual ~LSPBackEndServices();

        // 0x82A88038: logs the first pad in; returns false if the call could not start.
        bool Login(
            ProtocolCallContext *,
            unsigned long long xuid,
            const String &gamertag,
            unsigned int titleID,
            unsigned short port,
            Credentials **,
            const String &,
            int,
            int
        );
        // 0x82A88450: adds another pad to the logged-in session.
        bool LoginPlayer(
            ProtocolCallContext *, unsigned long long xuid, const String &gamertag, unsigned int *
        );
        void Logout(ProtocolCallContext *, Credentials *); // 0x82A885B8
        bool IsLoggedIn(); // 0x82A88CE8
        void Terminate(ProtocolCallContext *); // 0x82A89AD0
        SecureConnectionClient *GetSecureConnectionClient(); // 0x82A89FF8
        static void SetAccessKey(const String &); // 0x82A88AB0

        unsigned char unk4[0x68]; // 0x4
        int mLoginTimeout; // 0x6c, "login_timeout" from the net/server config
        int unk70; // 0x70
        unsigned short unk74; // 0x74, set to 1001 by XboxServer::Init
        unsigned char unk76[0x3a]; // 0x76
    };

    // XboxServer +0x84: sizeof 0x54, ctor 0x82A8CCE0.
    class PersistentStoreClient : public ServiceClient {
    public:
        PersistentStoreClient();
        virtual ~PersistentStoreClient();
        unsigned char unk4[0x50]; // 0x4
    };

    // XboxServer +0x88: sizeof 0x54, ctor 0x82A8C970 (the first function of the
    // CompetitionClient.cpp pin), which takes the match-making client.
    class CompetitionClient : public ServiceClient {
    public:
        CompetitionClient(MatchMakingClient *);
        virtual ~CompetitionClient();
        unsigned char unk4[0x50]; // 0x4
    };
}

// Retail RTTI .?AVXboxServer@@ (vtables 0x8205793C, and 0x820578E4 for the
// Hmx::Object vbase at 0x90). Its own slots, in Server's order: Init 0x823EC980,
// Terminate 0x823EDC68, Poll 0x823ED490, Login 0x823ECF30, Logout 0x823EDD80,
// GetPlayerID 0x823EC728, GetPlayerIDs 0x823EDD88, and the client getters
// (slots 11, 13, 14 fold with other one-load getters; slot 15 is 0x823EC788).
class XboxServer : public Server {
public:
    // 0x823EC638: zeroes 0x6c-0x78 and 0x80-0x88; 0x7c is left alone.
    XboxServer()
        : mCredentials(0), mLoginCallContext(0), mLoginContext(0), mLogoutContext(0),
          mMatchMakingClient(0), mPersistentStoreClient(0), mCompetitionClient(0) {}
    virtual DataNode Handle(DataArray *, bool);
    virtual void Init();
    virtual void Terminate();
    virtual void Poll();
    virtual void Login();
    virtual void Logout();
    // 0x823EC728
    virtual int GetPlayerID(int i) {
        if (!IsConnected())
            return 0;
        return mPlayerIDs[i];
    }
    virtual void GetPlayerIDs(std::vector<unsigned int> &);
    virtual Quazal::MatchMakingClient *GetMatchMakingClient() { return mMatchMakingClient; }
    virtual Quazal::ServiceClient *GetPersistentStoreClient() {
        return mPersistentStoreClient;
    }
    virtual Quazal::ServiceClient *GetCompetitionClient() { return mCompetitionClient; }
    // 0x823EC788
    virtual Quazal::SecureConnectionClient *GetSecureConnectionClient() {
        return mBackEnd->GetSecureConnectionClient();
    }

    DataNode OnMsg(const SigninChangedMsg &);
    // 0x823ECBB8: logs in the next signed-in pad that has no player ID yet.
    void LoginNextPlayer();
    // 0x823EC790: the first pad that is signed into LIVE, not a guest and has no
    // player ID, with its XUID and gamertag; -1 if there is none.
    int FindPadToLogin(char *name, unsigned long long *xuid);
    // 0x823EDA20: Logout() passes true, Terminate() false. Name descriptive.
    void LogoutImpl(bool notify);

    Quazal::Credentials *mCredentials; // 0x6c
    Quazal::ProtocolCallContext *mLoginCallContext; // 0x70
    Quazal::ProtocolCallContext *mLoginContext; // 0x74, the per-pad login
    Quazal::ProtocolCallContext *mLogoutContext; // 0x78
    Quazal::LSPBackEndServices *mBackEnd; // 0x7c
    Quazal::MatchMakingClient *mMatchMakingClient; // 0x80
    Quazal::PersistentStoreClient *mPersistentStoreClient; // 0x84
    Quazal::CompetitionClient *mCompetitionClient; // 0x88
};
