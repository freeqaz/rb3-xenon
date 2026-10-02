// Quazal NetZ - .\Stack\Core\BerkeleySocketDriver.cpp
//
// Retail TU: .text 0x82B3CD20..0x82B3E1C0, compiled /Od /Oi- /Ob1 /GR- /EHs-c-
// (see objects.json). Its .rdata is the __FILE__ string followed by the
// BerkeleySocket and SocketDriver::Socket vtables, with no EH tables (every
// function holds a ScopedCS) and no complete-object locators.
//
// The BerkeleySocketDriver vtable itself (sdd, Create, Delete, Poll) is not in
// this object: retail keeps the copy emitted by the transport that owns the
// driver.
//
// The classes are declared here with the layouts retail uses rather than taken
// from the shared Quazal headers.
//
// This TU is built /Od: its locals are laid out by a walk over the scope's
// symbol hash table, so the local NAMES below determine the stack offsets, and
// the allocation passes __FILE__/__LINE__, so #line reproduces retail's line.

extern "C" {
void *memset(void *, int, unsigned int);
int *_errno();

typedef unsigned int SOCKET;
typedef unsigned long u_long;
typedef unsigned short u_short;

struct in_addr {
    union {
        struct {
            unsigned char s_b1, s_b2, s_b3, s_b4;
        } S_un_b;
        struct {
            unsigned short s_w1, s_w2;
        } S_un_w;
        unsigned long S_addr;
    } S_un;
};

struct sockaddr {
    unsigned short sa_family;
    char sa_data[14];
};

struct sockaddr_in {
    short sin_family;
    u_short sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
};

struct timeval {
    long tv_sec;
    long tv_usec;
};

struct fd_set {
    unsigned int fd_count;
    SOCKET fd_array[64];
};

SOCKET socket(int, int, int);
int bind(SOCKET, const struct sockaddr *, int);
int recvfrom(SOCKET, char *, int, int, struct sockaddr *, int *);
int sendto(SOCKET, const char *, int, int, const struct sockaddr *, int);
int connect(SOCKET, const struct sockaddr *, int);
int recv(SOCKET, char *, int, int);
int send(SOCKET, const char *, int, int);
int closesocket(SOCKET);
int ioctlsocket(SOCKET, long, u_long *);
int setsockopt(SOCKET, int, int, const char *, int);
int select(int, fd_set *, fd_set *, fd_set *, const struct timeval *);
int __WSAFDIsSet(SOCKET, fd_set *);
int WSAGetLastError();
}

namespace Quazal {
    // sockaddr_in with the accessors this TU uses on its local address.
    struct SocketAddress : public sockaddr_in {
        unsigned long GetAddress() const { return sin_addr.S_un.S_addr; }
        unsigned short GetPort() const { return sin_port; }
        void SetAddress(unsigned long ulAddress) { sin_addr.S_un.S_addr = ulAddress; }
        void SetPort(unsigned short usPort) { sin_port = usPort; }
    };
}

#define AF_INET 2
#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17
#define IPPROTO_VDP 254
#define INADDR_ANY (u_long)0x00000000
#define INVALID_SOCKET (SOCKET)(~0)
#define SOCKET_ERROR (-1)
#define SOL_SOCKET 0xffff
#define SO_BROADCAST 0x0020
#define FIONBIO 0x8004667E
#define WSAEWOULDBLOCK 10035L
#define FD_SETSIZE 64
#define s_addr S_un.S_addr
#define errno (*_errno())

#define FD_ZERO(set) (((fd_set *)(set))->fd_count = 0)
#define FD_SET(fd, set)                                                                     \
    do {                                                                                    \
        unsigned int __i;                                                                   \
        for (__i = 0; __i < ((fd_set *)(set))->fd_count; __i++) {                          \
            if (((fd_set *)(set))->fd_array[__i] == (fd)) {                                 \
                break;                                                                      \
            }                                                                               \
        }                                                                                   \
        if (__i == ((fd_set *)(set))->fd_count) {                                          \
            if (((fd_set *)(set))->fd_count < FD_SETSIZE) {                                 \
                ((fd_set *)(set))->fd_array[__i] = (fd);                                    \
                ((fd_set *)(set))->fd_count++;                                              \
            }                                                                               \
        }                                                                                   \
    } while (0, 0)
#define FD_ISSET(fd, set) __WSAFDIsSet((SOCKET)(fd), (fd_set *)(set))

namespace Quazal {

    class RootObject {
    public:
        static void *operator new(unsigned int, const char *, unsigned int);
        static void operator delete(void *);
        static void operator delete(void *, const char *, unsigned int);
        ~RootObject() {}
    };

    class MutexPrimitive {
    public:
        static bool s_bNoOp;
    };

    class CriticalSection : public RootObject {
    public:
        CriticalSection(unsigned int);
        ~CriticalSection();
        void EnterImpl();
        void LeaveImpl();
        void Enter() {
            if (!MutexPrimitive::s_bNoOp)
                EnterImpl();
        }
        void Leave() {
            if (!MutexPrimitive::s_bNoOp)
                LeaveImpl();
        }

        char m_data[0x14];
    };

    class ScopedCS : public RootObject {
    public:
        ScopedCS(CriticalSection &cs) : m_bInScope(true), critSec(&cs) { critSec->Enter(); }
        ~ScopedCS() { EndScope(); }
        void EndScope() {
            if (m_bInScope) {
                critSec->Leave();
                m_bInScope = false;
            }
        }

        bool m_bInScope; // 0x0
        CriticalSection *critSec; // 0x4
    };

    class SocketDriver : public RootObject {
    public:
        enum _TrafficType {
            UDP = 0,
            TCP = 1,
            VDP = 2
        };
        enum _SocketResult {
            Success = 0,
            Error = 1,
            WouldBlock = 2,
            InProgress = 3
        };

        class InetAddress {
        public:
            unsigned int GetAddress() const { return m_uiAddress; }
            unsigned short GetPortNumber() const { return m_usPort; }
            void SetAddress(unsigned int uiAddress) { m_uiAddress = uiAddress; }
            void SetPortNumber(unsigned short usPort) { m_usPort = usPort; }

            unsigned int m_uiAddress; // 0x0
            unsigned short m_usPort; // 0x4
        };

        class Socket : public RootObject {
        public:
            Socket() {}
            virtual bool Open(_TrafficType) = 0;
            virtual void Close() = 0;
            virtual bool Bind(unsigned short) = 0;
            virtual _SocketResult RecvFrom(unsigned char *, unsigned int, InetAddress *, unsigned int *) = 0;
            virtual _SocketResult
            SendTo(unsigned char *, unsigned int, const InetAddress &, unsigned int *) = 0;
            virtual _SocketResult Connect(const InetAddress &) = 0;
            virtual _SocketResult Recv(unsigned char *, unsigned int, unsigned int *) = 0;
            virtual _SocketResult Send(unsigned char *, unsigned int, unsigned int *) = 0;
            virtual bool SetMulticastAddress(unsigned int) { return true; }
            virtual ~Socket() {}
        };

        struct PollInfo {
            Socket *m_pSocket; // 0x0
            int m_iFlags; // 0x4
            unsigned int m_uiResult; // 0x8
        };

        virtual ~SocketDriver() {}
        virtual Socket *Create() = 0;
        virtual void Delete(Socket *) = 0;
        virtual bool Poll(PollInfo *, unsigned int, unsigned int) = 0;
    };

    class BerkeleySocketDriver : public SocketDriver {
    public:
        class BerkeleySocket : public Socket {
        public:
            BerkeleySocket();
            virtual bool Open(_TrafficType);
            virtual void Close();
            virtual bool Bind(unsigned short);
            virtual _SocketResult RecvFrom(unsigned char *, unsigned int, InetAddress *, unsigned int *);
            virtual _SocketResult SendTo(unsigned char *, unsigned int, const InetAddress &, unsigned int *);
            virtual _SocketResult Connect(const InetAddress &);
            virtual _SocketResult Recv(unsigned char *, unsigned int, unsigned int *);
            virtual _SocketResult Send(unsigned char *, unsigned int, unsigned int *);
            virtual bool SetMulticastAddress(unsigned int);
            virtual ~BerkeleySocket();

            bool SetNonBlocking(bool);
            bool SetBroadcast(bool);
            int GetSocketError();
            SOCKET GetHandle() const { return m_hSocket; }

            SOCKET m_hSocket; // 0x4
            CriticalSection m_oCS; // 0x8
        };

        void Startup();
        void Shutdown();
        virtual Socket *Create();
        virtual void Delete(Socket *);
        virtual bool Poll(PollInfo *, unsigned int, unsigned int);
    };

}

using namespace Quazal;

BerkeleySocketDriver::BerkeleySocket::BerkeleySocket() : m_hSocket(INVALID_SOCKET), m_oCS(0) {}

BerkeleySocketDriver::BerkeleySocket::~BerkeleySocket() {}

bool BerkeleySocketDriver::BerkeleySocket::Open(_TrafficType eType) {
    ScopedCS csLock(m_oCS);
    switch (eType) {
    case UDP: {
        int iType = SOCK_DGRAM;
        int iProtocol = IPPROTO_UDP;
        m_hSocket = socket(AF_INET, iType, iProtocol);
        break;
    }
    case TCP:
        m_hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        break;
    case VDP:
        m_hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_VDP);
        break;
    }
    if (m_hSocket == INVALID_SOCKET)
        return false;
    SetNonBlocking(true);
    if (eType == UDP)
        SetBroadcast(true);
    return true;
}

bool BerkeleySocketDriver::BerkeleySocket::Bind(unsigned short usPort) {
    ScopedCS csLock(m_oCS);
    SocketAddress oAddress;
    memset(&oAddress, 0, sizeof(oAddress));
    oAddress.sin_family = AF_INET;
    oAddress.SetAddress(INADDR_ANY);
    oAddress.SetPort(usPort);
    int iResult = bind(m_hSocket, (sockaddr *)&oAddress, sizeof(oAddress));
    if (iResult != 0)
        return false;
    return true;
}

SocketDriver::_SocketResult BerkeleySocketDriver::BerkeleySocket::RecvFrom(
    unsigned char *pBuffer, unsigned int uiSize, InetAddress *pAddress, unsigned int *puiReceived
) {
    ScopedCS csLock(m_oCS);
    SocketAddress oAddress;
    memset(&oAddress, 0, sizeof(oAddress));
    oAddress.sin_family = AF_INET;
    int iAddressLength = sizeof(sockaddr_in);
    int iRet =
        recvfrom(m_hSocket, (char *)pBuffer, uiSize, 0, (sockaddr *)&oAddress, &iAddressLength);
    int iError = GetSocketError();
    if (iRet < 0) {
        return iError == WSAEWOULDBLOCK ? WouldBlock : Error;
    } else {
        if (pAddress) {
            pAddress->SetAddress(oAddress.GetAddress());
            pAddress->SetPortNumber(oAddress.GetPort());
        }
        if (puiReceived)
            *puiReceived = iRet;
        return Success;
    }
}

SocketDriver::_SocketResult BerkeleySocketDriver::BerkeleySocket::SendTo(
    unsigned char *pBuffer, unsigned int uiSize, const InetAddress &oDestination, unsigned int *puiSent
) {
    ScopedCS csLock(m_oCS);
    SocketAddress oAddress;
    memset(&oAddress, 0, sizeof(oAddress));
    oAddress.sin_family = AF_INET;
    oAddress.SetAddress(oDestination.GetAddress());
    oAddress.SetPort(oDestination.GetPortNumber());
    int iRet = sendto(
        m_hSocket, (const char *)pBuffer, uiSize, 0, (sockaddr *)&oAddress, sizeof(oAddress)
    );
    if (iRet < 0) {
        return GetSocketError() == WSAEWOULDBLOCK ? WouldBlock : Error;
    } else {
        if (puiSent)
            *puiSent = iRet;
        return Success;
    }
}

SocketDriver::_SocketResult BerkeleySocketDriver::BerkeleySocket::Connect(const InetAddress &oDestination) {
    ScopedCS csLock(m_oCS);
    SocketAddress oAddress;
    memset(&oAddress, 0, sizeof(oAddress));
    oAddress.sin_family = AF_INET;
    oAddress.SetAddress(oDestination.GetAddress());
    oAddress.SetPort(oDestination.GetPortNumber());
    int iRet = connect(m_hSocket, (sockaddr *)&oAddress, sizeof(oAddress));
    if (iRet < 0) {
        return GetSocketError() == WSAEWOULDBLOCK ? InProgress : Error;
    } else {
        return Success;
    }
}

SocketDriver::_SocketResult BerkeleySocketDriver::BerkeleySocket::Recv(
    unsigned char *pBuffer, unsigned int uiSize, unsigned int *puiReceived
) {
    ScopedCS csLock(m_oCS);
    int iRet = recv(m_hSocket, (char *)pBuffer, uiSize, 0);
    if (iRet < 0) {
        return GetSocketError() == WSAEWOULDBLOCK ? WouldBlock : Error;
    } else {
        if (puiReceived)
            *puiReceived = iRet;
        return Success;
    }
}

SocketDriver::_SocketResult BerkeleySocketDriver::BerkeleySocket::Send(
    unsigned char *pBuffer, unsigned int uiSize, unsigned int *puiSent
) {
    ScopedCS csLock(m_oCS);
    int iRet = send(m_hSocket, (const char *)pBuffer, uiSize, 0);
    if (iRet < 0) {
        return GetSocketError() == WSAEWOULDBLOCK ? WouldBlock : Error;
    } else {
        if (puiSent)
            *puiSent = iRet;
        return Success;
    }
}

void BerkeleySocketDriver::BerkeleySocket::Close() {
    ScopedCS csLock(m_oCS);
    closesocket(m_hSocket);
    m_hSocket = INVALID_SOCKET;
}

bool BerkeleySocketDriver::BerkeleySocket::SetMulticastAddress(unsigned int uiAddress) {
    ScopedCS csLock(m_oCS);
    return true;
}

bool BerkeleySocketDriver::BerkeleySocket::SetNonBlocking(bool bNonBlocking) {
    ScopedCS csLock(m_oCS);
    u_long ulNonBlocking = bNonBlocking;
    return ioctlsocket(m_hSocket, FIONBIO, &ulNonBlocking) == 0;
}

bool BerkeleySocketDriver::BerkeleySocket::SetBroadcast(bool bBroadcast) {
    ScopedCS csLock(m_oCS);
    int iValue = bBroadcast != false;
    if (setsockopt(m_hSocket, SOL_SOCKET, SO_BROADCAST, (const char *)&iValue, sizeof(iValue))
        == SOCKET_ERROR)
        return false;
    return true;
}

int BerkeleySocketDriver::BerkeleySocket::GetSocketError() {
    return WSAGetLastError();
    return errno;
}

void BerkeleySocketDriver::Startup() {}

void BerkeleySocketDriver::Shutdown() {}

SocketDriver::Socket *BerkeleySocketDriver::Create() {
#line 587
    return new (__FILE__, __LINE__) BerkeleySocket();
}

void BerkeleySocketDriver::Delete(Socket *pSocket) { delete pSocket; }

bool BerkeleySocketDriver::Poll(PollInfo *pInfo, unsigned int uiNbSockets, unsigned int uiTimeout) {
    fd_set oReadSet;
    fd_set oWriteSet;
    FD_ZERO(&oReadSet);
    FD_ZERO(&oWriteSet);
    unsigned int i;
    for (i = 0; i < uiNbSockets; i++) {
        pInfo[i].m_uiResult = 0;
        if (pInfo[i].m_iFlags & 1)
            FD_SET(((BerkeleySocket *)pInfo[i].m_pSocket)->GetHandle(), &oReadSet);
        if (pInfo[i].m_iFlags & 2)
            FD_SET(((BerkeleySocket *)pInfo[i].m_pSocket)->GetHandle(), &oWriteSet);
    }
    timeval oTimeout;
    oTimeout.tv_sec = 0;
    oTimeout.tv_usec = uiTimeout * 1000;
    int iNbReady = select(0, &oReadSet, &oWriteSet, 0, &oTimeout);
    if (iNbReady <= 0) {
        return false;
    } else {
        bool bSignaled = false;
        for (i = 0; i < uiNbSockets; i++) {
            if (FD_ISSET(((BerkeleySocket *)pInfo[i].m_pSocket)->GetHandle(), &oReadSet)) {
                pInfo[i].m_uiResult = 1;
                bSignaled = true;
            }
            if (FD_ISSET(((BerkeleySocket *)pInfo[i].m_pSocket)->GetHandle(), &oWriteSet)) {
                pInfo[i].m_uiResult = 2;
                bSignaled = true;
            }
        }
        return true;
    }
}
