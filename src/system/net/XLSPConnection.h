#pragma once
#include "os/ThreadCall.h"
#include "os/Timer.h"
#include "utl/Str.h"
#include "xdk/XNET.h"

class XLSPConnection : public ThreadCallback {
public:
    enum State {
    };
    XLSPConnection();
    virtual ~XLSPConnection();
    virtual int ThreadStart();
    virtual void ThreadDone(int);

    State GetState() { return mState; }
    int GetConnectionRequest() const { return mConnectionRequest; }
    void Poll();
    unsigned int GetServiceIP();
    void Connect(const char *, unsigned int);
    void Disconnect();

    static std::map<unsigned long, int> mXLSPRefCountMap;
    static bool SecureDisconnect(IN_ADDR);
    static int StartGatewayConnection(IN_ADDR);

    friend class DingoSvrXbox;

private:
    void SetState(State);
    void StartEnumeration();

    static const int kTitleServerEnumMaxCount;

    State mState;
    int mConnectionRequest;
    String mServerInfo;
    unsigned int mServiceId;
    HANDLE mEnumHandle;
    void *mEnumBuffer;
    DWORD mEnumBufferSize;
    int unk24;
    XOVERLAPPED mXOverlapped; // 0x28
    int unk44;
    // NB: retail RB3-360's XLSPConnection is 0x4c (76) bytes, ending here --
    // verified via NetCacheMgrInit's `new NetCacheMgrXbox()` allocation size
    // (li r3, 0xb4 = 180 = 0x64 NetCacheMgr base + 4 mDoneLoading/pad + 0x4c
    // XLSPConnection) and via Ghidra decompile of the retail XLSPConnection
    // ctor (fn 0x827d9998), which never touches anything past this field's
    // offset (0x48) -- no Symbol/Timer construction happens. DC3 (newer game)
    // added a reconnect-backoff `Timer mReconnectTimer` member here that RB3
    // retail does not have; ported verbatim from dc3 this bloats the class by
    // 52 bytes (Timer is 0x30 + 4 alignment pad) and desyncs every
    // sizeof(XLSPConnection)-dependent allocation. Do NOT re-add it here --
    // DingoSvrXbox (system/net/DingoSvr_Xbox.h) now owns its own
    // mReconnectTimer instead, since only it consumed this field.
};
