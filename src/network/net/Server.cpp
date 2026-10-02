#include "net/Server.h"
#include "net/Server_Xbox.h"
#include "game/BandUser.h"
#include "xdk/xapilibi/xbox.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Msg.h"
#include "obj/ObjMacros.h"
#include "os/PlatformMgr.h"
#include "os/System.h"
#include "utl/Symbols.h"
#include "utl/Symbols3.h"

Server::Server() : mLoginState(0) {}

void Server::Init() {
    SetName("server", ObjectDir::Main());
    DataArray *cfg = SystemConfig("net", "server");
    mKey = cfg->FindStr("access_key");
    mPort = cfg->FindInt("port");
    mAddress = cfg->FindStr("address");
    for (int i = 0; i < 4; i++)
        mPlayerIDs[i] = 0;
}

BEGIN_HANDLERS(Server)
    HANDLE_ACTION(login, Login())
    HANDLE_ACTION(logout, Logout())
    HANDLE_EXPR(is_connected, IsConnected())
    HANDLE_SUPERCLASS(MsgSource)
    HANDLE_CHECK(0x32)
END_HANDLERS

// Retail 0x823EC5B8, called once from the Server TU's XboxServer init with
// the "filter" string from the net/server config: it assigns its by-value
// Quazal::String to the global at 0x82E103F4, which Quazal's XboxLSP login job
// reads. Retail keeps no names; both names here are descriptive.
extern Quazal::String gLSPLoginFilter;
void SetLSPLoginFilter(Quazal::String filter) { gLSPLoginFilter = filter; }


// ---------------------------------------------------------------------------
// XboxServer. Retail .text 0x823EC638-0x823EDE08 (with the shared Server code
// at 0x823EC418-0x823EC638).

// Retail .data 0x82C6EB4C, a pointer to "h7fyctiuucf" (0x820576B8, in this TU's
// .rdata): the key XboxServer::Login hands the LSP login. Name descriptive.
const char *gLSPLoginKey = "h7fyctiuucf";

// Retail's global XboxServer is built by the dynamic initializer 0x82C3EC40.
XboxServer gXboxServer;

int XboxServer::FindPadToLogin(char *name, unsigned long long *xuid) {
    for (int i = 0; i < 4; i++) {
        if (mPlayerIDs[i] == 0 && ThePlatformMgr.IsSignedIntoLive(i)
            && !ThePlatformMgr.IsPadAGuest(i)) {
            int xuidResult = XUserGetXUID(i, xuid);
            int nameResult = XUserGetName(i, name, 0x1e);
            if (xuidResult == 0 && nameResult == 0)
                return i;
        }
    }
    return -1;
}

void XboxServer::Init() {
    Server::Init();
    DataArray *cfg = SystemConfig("net", "server");
    Quazal::LSPBackEndServices::SetAccessKey(mKey);
    Quazal::String filter(cfg->FindArray("filter")->Str(1));
    SetLSPLoginFilter(filter);
    mBackEnd = new Quazal::LSPBackEndServices();
    mBackEnd->unk74 = 1001;
    mBackEnd->mLoginTimeout = cfg->FindArray("login_timeout")->Int(1);
    static Symbol signin_changed("signin_changed");
    ThePlatformMgr.AddSink(this, signin_changed);
}

void XboxServer::LoginNextPlayer() {
    if (!IsConnected())
        return;
    if (mLoginContext)
        return;
    unsigned long long xuid;
    char name[0x20];
    mPadLoggingIn = FindPadToLogin(name, &xuid);
    if (mPadLoggingIn == -1)
        return;
    mLoginContext = new Quazal::ProtocolCallContext();
    static Quazal::String sGamertag;
    sGamertag = name;
    bool ok = mBackEnd->LoginPlayer(mLoginContext, xuid, name, &mPlayerIDLoggingIn);
    if (!ok) {
        delete mLoginContext;
        mLoginContext = 0;
    }
}

void XboxServer::Login() {
    if (IsConnected()) {
        MsgSource::Handle(ServerStatusChangedMsg(true), false);
        return;
    }
    if (mLogoutContext) {
        mLogoutContext->Wait(-1);
        delete mLogoutContext;
        mLogoutContext = 0;
    }
    mLoginState = 1;
    mLoginCallContext = new Quazal::ProtocolCallContext();
    mCredentials = 0;
    mPadLoggingIn = 0;
    unsigned long long xuid;
    char name[0x20];
    mPadLoggingIn = FindPadToLogin(name, &xuid);
    bool started = false;
    if (mPadLoggingIn != -1) {
        started = mBackEnd->Login(
            mLoginCallContext,
            xuid,
            name,
            0x545607D1,
            mPort,
            &mCredentials,
            gLSPLoginKey,
            0,
            0
        );
    }
    if (!started) {
        mLoginState = 0;
        delete mLoginCallContext;
        mLoginCallContext = 0;
        MsgSource::Handle(ServerStatusChangedMsg(false), false);
    }
}

void XboxServer::Poll() {
    if (mLoginCallContext) {
        Quazal::CallContext::_State state = mLoginCallContext->GetState();
        if (state != Quazal::CallContext::CallPending) {
            if (state == Quazal::CallContext::CallSuccess) {
                delete mLoginCallContext;
                mLoginCallContext = 0;
                mMatchMakingClient = new Quazal::MatchMakingClient();
                if (!mMatchMakingClient->Bind(mCredentials))
                    goto fail;
                mPersistentStoreClient = new Quazal::PersistentStoreClient();
                if (!mPersistentStoreClient->Bind(mCredentials))
                    goto fail;
                mCompetitionClient = new Quazal::CompetitionClient(mMatchMakingClient);
                if (!mCompetitionClient->Bind(mCredentials))
                    goto fail;
                mLoginState = 2;
                mPlayerIDs[mPadLoggingIn] = mCredentials->mPrincipalID;
                static ServerStatusChangedMsg connectedMsg(true);
                MsgSource::Handle(connectedMsg, false);
                static UserLoginMsg loginMsg(mPadLoggingIn);
                loginMsg->Node(2) = mPadLoggingIn;
                MsgSource::Handle(loginMsg, false);
                LoginNextPlayer();
            } else {
                Quazal::qResult result = mLoginCallContext->unk20;
                delete mLoginCallContext;
                mLoginCallContext = 0;
                int code = 0x80030064;
                result.Equals(code);
                mLoginState = 0;
                static ServerStatusChangedMsg failedMsg(false);
                MsgSource::Handle(failedMsg, false);
            }
        }
    }
    if (mLoginContext) {
        Quazal::CallContext::_State state = mLoginContext->GetState();
        if (state != Quazal::CallContext::CallPending) {
            bool success = state == Quazal::CallContext::CallSuccess;
            delete mLoginContext;
            mLoginContext = 0;
            if (success) {
                mPlayerIDs[mPadLoggingIn] = mPlayerIDLoggingIn;
                static UserLoginMsg playerLoginMsg(mPadLoggingIn);
                playerLoginMsg->Node(2) = mPadLoggingIn;
                MsgSource::Handle(playerLoginMsg, false);
                LoginNextPlayer();
            }
        }
    }
    if (mLogoutContext && mLogoutContext->GetState() != Quazal::CallContext::CallPending) {
        delete mLogoutContext;
        mLogoutContext = 0;
    }
    if (IsConnected() && !mBackEnd->IsLoggedIn()) {
    fail:
        Logout();
    }
}

void XboxServer::LogoutImpl(bool notify) {
    mLoginState = 0;
    mLogoutCritSec.Enter();
    if (mMatchMakingClient)
        mMatchMakingClient->Unbind();
    delete mMatchMakingClient;
    mMatchMakingClient = 0;
    if (mPersistentStoreClient)
        mPersistentStoreClient->Unbind();
    delete mPersistentStoreClient;
    mPersistentStoreClient = 0;
    if (mCompetitionClient)
        mCompetitionClient->Unbind();
    delete mCompetitionClient;
    mCompetitionClient = 0;
    delete mLogoutContext;
    mLogoutContext = 0;
    mLogoutContext = new Quazal::ProtocolCallContext();
    mBackEnd->Logout(mLogoutContext, mCredentials);
    for (int i = 0; i < 4; i++)
        mPlayerIDs[i] = 0;
    delete mLoginCallContext;
    mLoginCallContext = 0;
    delete mLoginContext;
    mLoginContext = 0;
    mLogoutCritSec.Exit();
    static ServerStatusChangedMsg msg(false);
    if (notify)
        MsgSource::Handle(msg, false);
}

void XboxServer::Terminate() {
    if (IsConnected())
        LogoutImpl(false);
    ThePlatformMgr.RemoveSink(this);
    Quazal::ProtocolCallContext ctx;
    mBackEnd->Terminate(&ctx);
    ctx.Wait(-1);
    delete mLoginCallContext;
    delete mBackEnd;
}

void XboxServer::Logout() { LogoutImpl(true); }

void XboxServer::GetPlayerIDs(std::vector<unsigned int> &ids) {
    ids.erase(ids.begin(), ids.end());
    if (IsConnected()) {
        for (int i = 0; i < 4; i++) {
            unsigned int id = mPlayerIDs[i];
            if (id != 0)
                ids.push_back(id);
        }
    }
}

// 0x823ED1E8: a pad whose sign-in changed and is no longer on LIVE loses its
// player ID; then the next pad is logged in.
DataNode XboxServer::OnMsg(const SigninChangedMsg &msg) {
    if (!IsConnected())
        return 0;
    for (int i = 0; i < 4; i++) {
        if ((msg.GetChangedMask() & (1 << i)) && !ThePlatformMgr.IsSignedIntoLive(i)) {
            mPlayerIDs[i] = 0;
        }
    }
    LoginNextPlayer();
    return 1;
}

// 0x823ED2A8
BEGIN_HANDLERS(XboxServer)
    HANDLE_MESSAGE(SigninChangedMsg)
    HANDLE_SUPERCLASS(Server)
    HANDLE_CHECK(0)
END_HANDLERS
