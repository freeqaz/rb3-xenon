// DC3 Native Port - NetworkSocket Stub
// Replaces NetworkSocket_Win.cpp

#include "os/NetworkSocket.h"
#include "utl/Str.h"
#include <cstdio>

NetworkSocket::~NetworkSocket() {}

// Stub socket that does nothing
class NativeNetworkSocket : public NetworkSocket {
public:
    virtual ~NativeNetworkSocket() {}
    virtual bool Connect(unsigned int, unsigned short) { return false; }
    virtual bool Fail() const { return true; }
    virtual void Disconnect() {}
    virtual void Bind(unsigned short) {}
    virtual bool InqBoundPort(unsigned short &) const { return false; }
    virtual void Listen() {}
    virtual NetworkSocket *Accept() { return nullptr; }
    virtual void GetRemoteIP(unsigned int &ip, unsigned short &port) { ip = 0; port = 0; }
    virtual bool CanSend() const { return false; }
    virtual bool CanRead() const { return false; }
    virtual int Send(const void *, unsigned int) { return -1; }
    virtual int Recv(void *, unsigned int) { return -1; }
    virtual int SendTo(const void *, unsigned int, unsigned int, unsigned short) { return -1; }
    virtual int BroadcastTo(const void *, unsigned int, unsigned short) { return -1; }
    virtual int RecvFrom(void *, unsigned int, unsigned int &, unsigned short &) { return -1; }
    virtual bool SetNoDelay(bool) { return false; }
};

NetworkSocket *NetworkSocket::Create(bool) {
    return new NativeNetworkSocket();
}

unsigned int NetworkSocket::IPStringToInt(const String &ip) {
    unsigned int a, b, c, d;
    if (sscanf(ip.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
        return (a << 24) | (b << 16) | (c << 8) | d;
    }
    return 0;
}

String NetworkSocket::IPIntToString(unsigned int ip) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
             (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
             (ip >> 8) & 0xFF, ip & 0xFF);
    return String(buf);
}

String NetworkSocket::GetHostName() {
    return String("localhost");
}
