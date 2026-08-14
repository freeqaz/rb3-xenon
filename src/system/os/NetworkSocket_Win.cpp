#include "os/NetworkSocket_Win.h"
#include "os/Debug.h"
#include "os/NetworkSocket.h"
#include "utl/MakeString.h"
#include "xdk/xapilibi/handleapi.h"
#include "xdk/xapilibi/synchapi.h"
#include "xdk/xbdm/xbdm.h"
#include "xdk/xnet/winsockx.h"
#include <cstring>

struct XNDNS {
    int iStatus; // 0x0
    unsigned int cina; // 0x4
    unsigned int aina[8]; // 0x8
};

struct XNetStartupParams {
    unsigned char cfgSizeOfStruct;
    unsigned char cfgFlags;
    unsigned char cfgSockMaxDgramSockets;
    unsigned char cfgSockMaxStreamSockets;
    unsigned char cfgSockDefaultRecvBufsizeInK;
    unsigned char cfgSockDefaultSendBufsizeInK;
    unsigned char cfgKeyRegMax;
    unsigned char cfgSecRegMax;
    unsigned char cfgQosDataLimitDiv4;
    unsigned char cfgQosProbeTimeoutInSeconds;
    unsigned char cfgQosProbeRetries;
    unsigned char cfgQosSrvMaxSimultaneousResponses;
    unsigned char cfgQosPairWaitTimeInSeconds;
};

extern "C" {
int WSACreateEvent();
int XNetDnsLookup(const char *, HANDLE, XNDNS **);
int XNetDnsRelease(XNDNS *);
unsigned int inet_addr(const char *);
int XNetInAddrToString(unsigned int addr, char *buf, int len);
int XNetStartup(const XNetStartupParams *);
int WSAGetLastError();

SOCKET socket(int af, int type, int protocol);
int ioctlsocket(SOCKET s, long cmd, unsigned long *argp);
int connect(SOCKET s, const sockaddr_in *name, int namelen);
int select(
    int nfds,
    fd_set *readfds,
    fd_set *writefds,
    fd_set *exceptfds,
    const timeval *timeout
);
int shutdown(SOCKET s, int how);
int closesocket(SOCKET s);
int setsockopt(SOCKET s, int level, int optname, const char *optval, int optlen);
int bind(SOCKET s, const sockaddr_in *addr, int namelen);
int getsockname(SOCKET s, sockaddr_in *name, int *namelen);
int listen(SOCKET s, int backlog);
SOCKET accept(SOCKET s, sockaddr_in *addr, int *addrlen);
int getpeername(SOCKET s, sockaddr_in *name, int *namelen);
int send(SOCKET s, const char *buf, int len, int flags);
int recv(SOCKET s, char *buf, int len, int flags);
int sendto(
    SOCKET s, const char *buf, int len, int flags, const sockaddr_in *to, int tolen
);
int recvfrom(SOCKET s, char *buf, int len, int flags, sockaddr_in *from, int *fromlen);
}

bool WinSockSocket::sInit = false;

WinSockSocket::WinSockSocket(bool streaming) : mStreaming(streaming), mFail(false) {
    Init();
    if (!mStreaming) {
        mSocket = socket(AF_INET, 2, 0x11);
    } else {
        mSocket = socket(AF_INET, 1, 6);
    }
    MILO_ASSERT(mSocket != INVALID_SOCKET, 0xA8);
    unsigned long ul = 1;
    ioctlsocket(mSocket, 0x8004667E, &ul);
}

WinSockSocket::WinSockSocket(unsigned int s, bool streaming)
    : mSocket((SOCKET)s), mStreaming(streaming), mFail(false) {
    unsigned long ul = 1;
    ioctlsocket(mSocket, 0x8004667E, &ul);
}

WinSockSocket::~WinSockSocket() { Disconnect(); }

bool WinSockSocket::Connect(unsigned int ip, unsigned short port) {
    sockaddr_in addr;
    addr.sin_family = 2;
    addr.sin_port = port;
    addr.sin_addr.s_un.s_addr = ip;
    int res = connect(mSocket, &addr, 0x10);
    if (res == -1 && WSAGetLastError() != 0x2733) {
        mFail = true;
    }
    return res == 0;
}

bool WinSockSocket::Fail() const {
    timeval val;
    fd_set set;
    val.tv_sec = 0;
    val.tv_usec = 0;
    set.fd_count = 1;
    set.fd_array[0] = mSocket;
    // NOTE(laneF6s): 100% -- source-order of the cases (not just the compare
    // polarity) is load-bearing. `case -1` falling through into `case 1`'s
    // store (the shape that reads most naturally from the original comment,
    // "fallthrough matches retail: no break here") was stuck at 99.5%/2
    // diverging instrs no matter how the compare was phrased (5 variants
    // tried by prior lanes, all worse or unchanged -- see git history for
    // this file). What fixes it: write `case 1` FIRST with its own `break`,
    // then `case -1` with its own (duplicated) store statement -- i.e. no
    // fallthrough in source at all. This flips which case MSVC treats as the
    // "near" branch for the second compare, matching retail's `bne`-to-exit
    // polarity byte-for-byte (100.0% normalized, all 33 instrs equal).
    switch (select(0, nullptr, nullptr, &set, &val)) {
    case 1:
        const_cast<WinSockSocket *>(this)->mFail = true;
        break;
    case -1:
        MILO_LOG("select returned SOCKET_ERROR %d\n", WSAGetLastError());
        const_cast<WinSockSocket *>(this)->mFail = true;
        break;
    default:
        break;
    }
    return mFail;
}

void WinSockSocket::Disconnect() {
    if (mSocket != INVALID_SOCKET) {
        shutdown(mSocket, 2);
        closesocket(mSocket);
        mSocket = INVALID_SOCKET;
    }
}

void WinSockSocket::Bind(unsigned short port) {
    int val = 1;
    setsockopt(mSocket, 0xFFFF, 4, (char *)&val, 4);
    sockaddr_in addr;
    addr.sin_family = 2;
    addr.sin_port = port;
    addr.sin_addr.s_un.s_addr = 0;
    int ret = bind(mSocket, &addr, 0x10);
    if (ret == -1) {
        MILO_FAIL(
            "NetworkSocket::Bind(%d) could not bind (error = %d).\nTry rebooting your computer.",
            port,
            WSAGetLastError()
        );
    }
}

bool WinSockSocket::InqBoundPort(unsigned short &port) const {
    sockaddr_in addr;
    int namelen = 16;
    if (getsockname(mSocket, &addr, &namelen) != 0) {
        return false;
    } else {
        port = addr.sin_port;
        return true;
    }
}

void WinSockSocket::Listen() { listen(mSocket, 5); }

NetworkSocket *WinSockSocket::Accept() {
    sockaddr_in addr;
    int addrlen = 16;
    SOCKET s = accept(mSocket, &addr, &addrlen);
    if (s != INVALID_SOCKET) {
        return new WinSockSocket((unsigned int)s, mStreaming);
    } else {
        return nullptr;
    }
}

void WinSockSocket::GetRemoteIP(unsigned int &ip, unsigned short &port) {
    sockaddr_in addr;
    int namelen = 16;
    getpeername(mSocket, &addr, &namelen);
    ip = addr.sin_addr.s_un.s_addr;
    port = addr.sin_port;
}

bool WinSockSocket::CanSend() const {
    fd_set write;
    write.fd_array[0] = mSocket;
    timeval val;
    val.tv_sec = 0;
    val.tv_usec = 0;
    write.fd_count = 1;
    return select(0, nullptr, &write, nullptr, &val) == 1;
}

bool WinSockSocket::CanRead() const {
    fd_set read;
    read.fd_array[0] = mSocket;
    timeval val;
    val.tv_sec = 0;
    val.tv_usec = 0;
    read.fd_count = 1;
    return select(0, &read, nullptr, nullptr, &val) == 1;
}

int WinSockSocket::Send(const void *data, unsigned int len) {
    if (mFail) {
        return 0;
    }
    int ret = send(mSocket, (const char *)data, len, 0);
    if (ret == -1) {
        int err = WSAGetLastError();
        switch (err) {
        case 0x2733:
            return 0;
        case 0x2746:
        case 0x2749:
            mFail = true;
            return 0;
        default:
            MILO_FAIL("error in Send: %i", err);
            break;
        }
    }
    return ret;
}

int WinSockSocket::Recv(void *data, unsigned int len) {
    if (!mFail && CanRead()) {
        int ret = recv(mSocket, (char *)data, len, 0);
        if (ret > 0) {
            return ret;
        }
        mFail = true;
    }
    return 0;
}

int WinSockSocket::SendTo(
    const void *data, unsigned int len, unsigned int ip, unsigned short port
) {
    sockaddr_in addr;
    addr.sin_family = 2;
    addr.sin_port = port;
    addr.sin_addr.s_un.s_addr = ip;
    int ret = sendto(mSocket, (const char *)data, len, 0, &addr, 16);
    if (ret == -1) {
        int err = WSAGetLastError();
        if (err == 0x2733) {
            return 0;
        }
        if (err != 0x2751) {
            MILO_FAIL("error in Send: %i", err);
        } else {
            if (mStreaming) {
                mFail = true;
            }
            return 0;
        }
    }
    return ret;
}

int WinSockSocket::BroadcastTo(const void *data, unsigned int len, unsigned short port) {
    int val = 1;
    setsockopt(mSocket, 0xFFFF, 32, (const char *)&val, 4);
    return SendTo(data, len, -1, port);
}

// DEFERRED as codegen, lane INSDEL-3 (2026-08-14). 96.912%, 2 charges on the
// error path: retail REUSES the already-live 0 in r3 (materialized as the `li
// r3,0` return value) for the `stw r3,0(r31)` out-param store, while we
// materialize a separate `li r10,0` and store that. BOTH STORE 0 -- it is
// register reuse, with no source token corresponding to the charge. Per the
// NAMES-vs-IMPLIES rule this is not source-addressable; do not re-open.
int WinSockSocket::RecvFrom(
    void *data, unsigned int maxLen, unsigned int &ip, unsigned short &port
) {
    sockaddr_in addr;
    int fromlen = 16;
    int ret = recvfrom(mSocket, (char *)data, maxLen, 0, &addr, &fromlen);
    if (ret == -1) {
        int err = WSAGetLastError();
        if (err == 0x2733) {
            return 0;
        } else {
            MILO_FAIL("error in RecvFrom: %i", err);
            port = -1;
            ip = 0;
            return 0;
        }
    } else {
        ip = addr.sin_addr.s_un.s_addr;
        port = addr.sin_port;
    }
    return ret;
}

bool WinSockSocket::SetNoDelay(bool enabled) {
    int val = enabled ? 1 : 0;
    return setsockopt(mSocket, 6, 1, (const char *)&val, 4) == 0;
}

void WinSockSocket::Init() {
    if (!sInit) {
        sInit = true;
        XNetStartupParams params;
        memset(&params, 0, sizeof(XNetStartupParams));
        params.cfgSizeOfStruct = 0xD;
        params.cfgFlags = 1;
        XNetStartup(&params);
        WSADATA data;
        WSAStartup(0x202, &data);
    }
}

NetworkSocket::~NetworkSocket() {}

NetworkSocket *NetworkSocket::Create(bool streaming) {
    return new WinSockSocket(streaming);
}

unsigned int NetworkSocket::IPStringToInt(const String &ip) {
    WinSockSocket::Init();
    return inet_addr(ip.c_str());
}

String NetworkSocket::IPIntToString(unsigned int ip) {
    WinSockSocket::Init();
    char buf[32];
    buf[0] = '\0';
    memset(buf + 1, 0, 0x1f);
    XNetInAddrToString(ip, buf, 0x20);
    return String(buf);
}

unsigned int NetworkSocket::ResolveHostName(String name) {
    WinSockSocket::Init();
    HANDLE event = (HANDLE)WSACreateEvent();
    XNDNS *pDns = 0;
    int status = XNetDnsLookup(name.c_str(), event, &pDns);
    if (status != 0 || pDns == 0) {
        TheDebug << MakeString("XNetDnsLookup returned %d %x for %s\n", status, pDns, name.c_str());
        return 0;
    }
    unsigned int result = 0;
    WaitForSingleObject(event, 10000);
    int dnsStatus = pDns->iStatus;
    if (dnsStatus == 0x2AF9) {
        char *hostStr = (char *)name.c_str();
        TheDebug << MakeString("Host %s not found.", hostStr);
    } else if (dnsStatus == 0x274C) {
        char *hostStr = (char *)name.c_str();
        TheDebug << MakeString("Host %s lookup timed out.", hostStr);
    } else if (dnsStatus == 0) {
        result = pDns->aina[0];
    }
    if (XNetDnsRelease(pDns) != 0) {
        FormatString fmt("could not release XNDNS");
        TheDebug << fmt.Str();
    }
    CloseHandle(event);
    return result;
}
