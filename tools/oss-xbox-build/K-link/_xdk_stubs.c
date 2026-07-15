/* XDK entrypoint shims for the XDK-free SI build.
   The XDK's winsock/XNet functions are thin static-lib wrappers around xam.xex
   NetDll_* exports with an XNCALLER type prepended; the Lane-L reconstructed
   xam.lib provides those exports, so wire the plain-name entrypoints through
   for real (they were return -1 stubs before -- which is why every RB3E
   socket call failed on hardware regardless of DashLaunch sockpatch).
   Non-network XDK entrypoints (content/UI/keyboard/relaunch) stay stubbed:
   unused by the same-instrument feature.
   All args are 32-bit ints/pointers on PPC, so int prototypes link fine. */

#define XNCALLER_TITLE 1

int NetDll_accept(int caller, int s, void *addr, int *addrlen);
int NetDll_bind(int caller, int s, const void *name, int namelen);
int NetDll_closesocket(int caller, int s);
int NetDll_connect(int caller, int s, const void *name, int namelen);
int NetDll_ioctlsocket(int caller, int s, int cmd, void *argp);
int NetDll_listen(int caller, int s, int backlog);
int NetDll_recv(int caller, int s, void *buf, int len, int flags);
int NetDll_recvfrom(int caller, int s, void *buf, int len, int flags, void *from, int *fromlen);
int NetDll_send(int caller, int s, const void *buf, int len, int flags);
int NetDll_sendto(int caller, int s, const void *buf, int len, int flags, const void *to, int tolen);
int NetDll_setsockopt(int caller, int s, int level, int optname, const void *optval, int optlen);
int NetDll_getsockopt(int caller, int s, int level, int optname, void *optval, int *optlen);
int NetDll_getsockname(int caller, int s, void *name, int *namelen);
int NetDll_select(int caller, int nfds, void *readfds, void *writefds, void *exceptfds, const void *timeout);
int NetDll_shutdown(int caller, int s, int how);
int NetDll_socket(int caller, int af, int type, int protocol);
int NetDll_WSAGetLastError(int caller);
int NetDll_WSAStartup(int caller, int wVersionRequested, void *wsaData);
int NetDll_WSACleanup(int caller);
int NetDll_XNetGetOpt(int caller, int optid, void *buf, int *len);
int NetDll_XNetGetTitleXnAddr(int caller, void *pxna);
int NetDll_XNetQosLookup(int caller, int cxna, void *apxna, void *apxnkid, void *apxnkey,
                         int cina, void *aina, void *adwServiceId, int cProbes,
                         int dwBitsPerSec, int dwFlags, void *hEvent, void *ppxnqos);
int NetDll_XNetQosServiceLookup(int caller, int dwFlags, void *hEvent, void *ppxnqos);
int NetDll_XNetStartup(int caller, void *pxnsp);
int NetDll_XNetXnAddrToInAddr(int caller, const void *pxna, const void *pxnkid, void *pina);

int accept(int s, void *addr, int *addrlen) { return NetDll_accept(XNCALLER_TITLE, s, addr, addrlen); }
int bind(int s, const void *name, int namelen) { return NetDll_bind(XNCALLER_TITLE, s, name, namelen); }
int closesocket(int s) { return NetDll_closesocket(XNCALLER_TITLE, s); }
int connect(int s, const void *name, int namelen) { return NetDll_connect(XNCALLER_TITLE, s, name, namelen); }
int ioctlsocket(int s, int cmd, void *argp) { return NetDll_ioctlsocket(XNCALLER_TITLE, s, cmd, argp); }
int listen(int s, int backlog) { return NetDll_listen(XNCALLER_TITLE, s, backlog); }
int recv(int s, void *buf, int len, int flags) { return NetDll_recv(XNCALLER_TITLE, s, buf, len, flags); }
int recvfrom(int s, void *buf, int len, int flags, void *from, int *fromlen) { return NetDll_recvfrom(XNCALLER_TITLE, s, buf, len, flags, from, fromlen); }
int send(int s, const void *buf, int len, int flags) { return NetDll_send(XNCALLER_TITLE, s, buf, len, flags); }
int sendto(int s, const void *buf, int len, int flags, const void *to, int tolen) { return NetDll_sendto(XNCALLER_TITLE, s, buf, len, flags, to, tolen); }
int setsockopt(int s, int level, int optname, const void *optval, int optlen) { return NetDll_setsockopt(XNCALLER_TITLE, s, level, optname, optval, optlen); }
int getsockopt(int s, int level, int optname, void *optval, int *optlen) { return NetDll_getsockopt(XNCALLER_TITLE, s, level, optname, optval, optlen); }
int getsockname(int s, void *name, int *namelen) { return NetDll_getsockname(XNCALLER_TITLE, s, name, namelen); }
int select(int nfds, void *readfds, void *writefds, void *exceptfds, const void *timeout) { return NetDll_select(XNCALLER_TITLE, nfds, readfds, writefds, exceptfds, timeout); }

/* classic winsock FD_ISSET backing: linear scan of the fd_array (fd_count at +0,
   fd_array[] at +4). Layout matches xdk-oss/xnet.h fd_set. */
int __WSAFDIsSet(unsigned int fd, void *set)
{
    unsigned int *p = (unsigned int *)set;   /* p[0]=fd_count, p[1..]=fd_array */
    unsigned int n = p[0];
    unsigned int i;
    for (i = 0; i < n; i++) {
        if (p[1 + i] == fd) return 1;
    }
    return 0;
}
int shutdown(int s, int how) { return NetDll_shutdown(XNCALLER_TITLE, s, how); }
int socket(int af, int type, int protocol) { return NetDll_socket(XNCALLER_TITLE, af, type, protocol); }
int WSAGetLastError(void) { return NetDll_WSAGetLastError(XNCALLER_TITLE); }
int WSAStartup(int wVersionRequested, void *wsaData) { return NetDll_WSAStartup(XNCALLER_TITLE, wVersionRequested, wsaData); }
int WSACleanup(void) { return NetDll_WSACleanup(XNCALLER_TITLE); }
int XNetGetOpt(int optid, void *buf, int *len) { return NetDll_XNetGetOpt(XNCALLER_TITLE, optid, buf, len); }
int XNetGetTitleXnAddr(void *pxna) { return NetDll_XNetGetTitleXnAddr(XNCALLER_TITLE, pxna); }
int XNetQosLookup(int cxna, void *apxna, void *apxnkid, void *apxnkey,
                  int cina, void *aina, void *adwServiceId, int cProbes,
                  int dwBitsPerSec, int dwFlags, void *hEvent, void *ppxnqos)
{
    return NetDll_XNetQosLookup(XNCALLER_TITLE, cxna, apxna, apxnkid, apxnkey,
                                cina, aina, adwServiceId, cProbes,
                                dwBitsPerSec, dwFlags, hEvent, ppxnqos);
}
int XNetQosServiceLookup(int dwFlags, void *hEvent, void *ppxnqos) { return NetDll_XNetQosServiceLookup(XNCALLER_TITLE, dwFlags, hEvent, ppxnqos); }
int XNetStartup(void *pxnsp) { return NetDll_XNetStartup(XNCALLER_TITLE, pxnsp); }
int XNetXnAddrToInAddr(const void *pxna, const void *pxnkid, void *pina) { return NetDll_XNetXnAddrToInAddr(XNCALLER_TITLE, pxna, pxnkid, pina); }

/* ---- non-network XDK entrypoints: still stubbed (unused by SI feature) ---- */
int XCloseHandle(){ return 1; }
int XContentDelete(){ return 0; }
int XContentGetDeviceData(){ return 0; }
int XHasOverlappedIoCompleted(){ return 1; }
int XInputGetKeystroke(){ return 0; }
int XLaunchNewImage(){ return 0; }
int XShowFriendsUI(){ return 0; }
int XShowKeyboardUI(){ return 0; }
int XShowMessageBoxUI(){ return 0; }
int XUserGetName(){ return 0; }
int XUserGetSigninInfo(){ return 0; }
int XUserGetSigninState(){ return 0; }
int XUserGetXUID(){ return 0; }
