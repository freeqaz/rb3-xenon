#include "os/NetworkSocket.h"
#include "utl/Str.h"
#include <cstdlib>

// External declarations from Win32 API and other modules
extern int WSACreateEvent();
extern int XNetDnsLookup(int ip_ptr, int event, void *dns_info);
extern int XNetDnsRelease(void *dns_info);
extern void CloseHandle(int handle);
extern void WaitForSingleObject(int handle, int timeout);

// Forward declarations for merged functions and system functions
extern const char *TheDebugStr();
extern void PrintDebug(const char *str);

NetAddress NetworkSocket::SetIPPortFromHostPort(
    const char *host_port, const char *domain, unsigned short default_port
) {
    NetAddress addr;
    String str(host_port);
    unsigned int idx = str.find(':');
    if (idx != FixedString::npos) {
        String str2 = str.substr(idx + 1);
        str = str.substr(0, idx);
        addr.mPort = strtol(str2.c_str(), 0, 0);
    } else {
        addr.mPort = default_port;
    }
    unsigned int ip = IPStringToInt(str);
    if (ip != -1) {
        addr.mIP = ip;
    }
    if (addr.mIP == 0) {
        if (domain) {
            str += ".";
            str += domain;
        }
        addr.mIP = ResolveHostName(str);
    }
    return addr;
}

unsigned int NetworkSocket::ResolveHostName(String name) {
    unsigned int result = 0;

    // Initialize WinSock
    // ?Init@WinSockSocket@@SAXXZ();

    // Create a WSA event for async DNS lookup
    int event = WSACreateEvent();

    // Perform DNS lookup
    unsigned int dns_result = 0;
    void *dns_info = nullptr;

    int lookup_status = XNetDnsLookup((int)name.c_str(), event, &dns_info);

    if (lookup_status == 0 && dns_info != nullptr) {
        // Wait for DNS lookup to complete (10 second timeout = 0x2710 ms)
        WaitForSingleObject(event, 0x2710);

        // Check the DNS result status
        int *dns_status_ptr = (int *)dns_info;
        int dns_status = *dns_status_ptr;

        if (dns_status == 0) {
            // Success - extract IP address from offset 8
            int *ip_ptr = (int *)((char *)dns_info + 8);
            result = *ip_ptr;
        } else if (dns_status == 0x2AF9) {
            // Host not found
            result = 0;
        } else if (dns_status == 0x274C) {
            // Lookup timed out
            result = 0;
        }

        // Release DNS resources
        if (XNetDnsRelease(dns_info) != 0) {
            // Could not release XNDNS
        }

        CloseHandle(event);
    } else if (lookup_status != 0) {
        // XNetDnsLookup failed
    }

    // Clean up the input string
    // String destructor called automatically

    return result;
}
