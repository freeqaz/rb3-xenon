#include "net/Server.h"
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

// Retail RTTI .?AVXboxServer@@ (vtable 0x8205793C; Hmx::Object vbase at 0x90).
// Retail's Server TU also carries XboxServer: its own range holds XboxServer
// methods (e.g. 0x823ECBB8, which logs the next pad in through members 0x74 /
// 0x7c), the ctor and GetPlayerID sit at 0x823EC638 / 0x823EC728, and Handle /
// OnMsg(SigninChangedMsg) follow at 0x823ED1E8 / 0x823ED2A8. Only those four
// bodies are written here; the rest of the class is Quazal login code not yet
// in this tree.
class XboxServer : public Server {
public:
    // 0x823EC638: zeroes 0x6c-0x78 and 0x80-0x88; 0x7c is left alone.
    XboxServer() : unk6c(0), unk70(0), mLoginContext(0), unk78(0), unk80(0), unk84(0), unk88(0) {}
    virtual DataNode Handle(DataArray *, bool);
    // Retail bodies are in the unwritten part of the TU.
    virtual void Poll();
    virtual void Login();
    virtual void Logout();
    // 0x823EC728
    virtual int GetPlayerID(int i) {
        if (!IsConnected())
            return 0;
        return mPlayerIDs[i];
    }

    DataNode OnMsg(const SigninChangedMsg &);
    void LoginNextPlayer(); // 0x823ECBB8, Quazal login call; not written

    int unk6c; // 0x6c
    int unk70; // 0x70
    Quazal::ProtocolCallContext *mLoginContext; // 0x74
    int unk78; // 0x78
    void *unk7c; // 0x7c
    int unk80; // 0x80
    int unk84; // 0x84
    int unk88; // 0x88
};

// Retail's global XboxServer is built by the dynamic initializer 0x82C3EC40.
XboxServer gXboxServer;

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
