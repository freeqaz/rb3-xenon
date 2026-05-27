#include "os/NetworkSocket.h"
#include "os/Debug.h"
#include "utl/MakeString.h"
#include "xdk/xapilibi/handleapi.h"
#include "xdk/xapilibi/synchapi.h"
#include <cstring>

struct XNDNS {
    int iStatus; // 0x0
    unsigned int cina; // 0x4
    unsigned int aina[8]; // 0x8
};

extern "C" {
int WSACreateEvent();
int XNetDnsLookup(const char *, HANDLE, XNDNS **);
int XNetDnsRelease(XNDNS *);
unsigned int inet_addr(const char *);
int XNetInAddrToString(unsigned int addr, char *buf, int len);
}

class WinSockSocket {
public:
    static void Init();
};

NetworkSocket::~NetworkSocket() {}

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
