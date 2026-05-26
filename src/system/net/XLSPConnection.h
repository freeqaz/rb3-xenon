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
    Timer mReconnectTimer;
};
