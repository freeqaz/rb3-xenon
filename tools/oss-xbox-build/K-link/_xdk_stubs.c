/* Auto-generated XDK import stubs for the XDK-free SI build.
   The reconstructed import libs export NetDll_* names; these plain-name XDK
   entrypoints (networking/content/keyboard/relaunch) are unused by the
   same-instrument crash fix, so stub them so the link completes.
   Close/overlapped-poll return TRUE to avoid caller hangs. */

int accept(){ return -1; }
int bind(){ return -1; }
int closesocket(){ return -1; }
int connect(){ return -1; }
int ioctlsocket(){ return -1; }
int listen(){ return -1; }
int recv(){ return -1; }
int recvfrom(){ return -1; }
int send(){ return -1; }
int sendto(){ return -1; }
int setsockopt(){ return -1; }
int shutdown(){ return -1; }
int socket(){ return -1; }
int WSAGetLastError(){ return -1; }
int WSAStartup(){ return -1; }
int XCloseHandle(){ return 1; }
int XContentDelete(){ return 0; }
int XContentGetDeviceData(){ return 0; }
int XHasOverlappedIoCompleted(){ return 1; }
int XInputGetKeystroke(){ return 0; }
int XLaunchNewImage(){ return 0; }
int XNetGetOpt(){ return -1; }
int XNetGetTitleXnAddr(){ return 0; }
int XNetQosLookup(){ return 0; }
int XNetQosServiceLookup(){ return 0; }
int XNetStartup(){ return 0; }
int XNetXnAddrToInAddr(){ return 0; }
int XShowFriendsUI(){ return 0; }
int XShowKeyboardUI(){ return 0; }
int XShowMessageBoxUI(){ return 0; }
int XUserGetName(){ return 0; }
int XUserGetSigninInfo(){ return 0; }
int XUserGetSigninState(){ return 0; }
int XUserGetXUID(){ return 0; }
