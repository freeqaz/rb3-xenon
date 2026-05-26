#include "net/XLSPConnection.h"
#include "math/Rand.h"
#include "utl/MemMgr.h"
#include "xdk/XAPILIB.h"
#include "xdk/XNET.h"
#include "xdk/XONLINE.h"
#include <utility>

const int XLSPConnection::kTitleServerEnumMaxCount = 8;
std::map<unsigned long, int> XLSPConnection::mXLSPRefCountMap;

XLSPConnection::XLSPConnection()
    : mState((State)-1), mConnectionRequest(0), mServiceId(0), mEnumHandle(INVALID_HANDLE_VALUE), mEnumBuffer(0),
      mEnumBufferSize(0) {
    memset(&mXOverlapped, 0, sizeof(XOVERLAPPED));
    unk44 = 0;
    mReconnectTimer.Reset();
    SetState((State)0);
}

XLSPConnection::~XLSPConnection() { SetState((State)-1); }

int XLSPConnection::ThreadStart() {
    if (mState != 5) {
        MILO_FAIL("Unhandled state %d in ThreadStart", mState);
    } else {
        XCancelOverlapped(&mXOverlapped);
    }
    return 0;
}

void XLSPConnection::ThreadDone(int i1) {
    if (mState != 5) {
        MILO_FAIL("Unhandled state %d in ThreadStart", mState);
    } else {
        memset(&mXOverlapped, 0, sizeof(XOVERLAPPED));
        CloseHandle(mEnumHandle);
        mEnumBufferSize = 0;
        mEnumHandle = INVALID_HANDLE_VALUE;
        if (mEnumBuffer) {
            MemFree(mEnumBuffer, __FILE__, 299);
            mEnumBuffer = nullptr;
        }
        SetState((State)0);
    }
}

void XLSPConnection::Connect(const char *cc, unsigned int ui) {
    mServerInfo = cc;
    mServiceId = ui;
    if (mConnectionRequest != 3) {
        mConnectionRequest = 3;
    }
    if (mState == 0) {
        SetState((State)1);
    }
}

unsigned int XLSPConnection::GetServiceIP() { return unk44; }

void XLSPConnection::Disconnect() {
    if (mConnectionRequest != 0) {
        mConnectionRequest = 0;
    }
    if (mState > 0 && mState <= 4) {
        SetState((State)5);
    }
}

void XLSPConnection::SetState(State s) {
    if (mState == s)
        return;
    do {
        State oldState = mState;
        State newState = s;
        bool skipCleanup = false;
        if (oldState == 1) {
            if (mEnumHandle != INVALID_HANDLE_VALUE) {
                if (mXOverlapped.InternalLow == ERROR_IO_PENDING) {
                    if (newState == 5) {
                        skipCleanup = true;
                    } else {
                        XCancelOverlapped(&mXOverlapped);
                    }
                }
                if (!skipCleanup) {
                    memset(&mXOverlapped, 0, sizeof(XOVERLAPPED));
                    CloseHandle(mEnumHandle);
                    mEnumHandle = INVALID_HANDLE_VALUE;
                }
            }
            if (!skipCleanup) {
                mEnumBufferSize = 0;
                if (mEnumBuffer) {
                    MemFree(mEnumBuffer, __FILE__, __LINE__);
                    mEnumBuffer = nullptr;
                }
            }
        } else if (oldState == 2) {
            if (newState != 3) {
                SecureDisconnect(*(in_addr *)&unk44);
            }
        } else if (oldState == 3) {
            SecureDisconnect(*(in_addr *)&unk44);
        }

        mState = newState;

        if (newState == 1) {
            StartEnumeration();
            return;
        } else if (newState == 2) {
            int ret = StartGatewayConnection(*(in_addr *)&unk44);
            if (ret == 0) {
                return;
            }
            s = (State)4;
        } else if (newState == 4) {
            mReconnectTimer.Restart();
            return;
        } else if (newState == 5) {
            if (mEnumHandle != INVALID_HANDLE_VALUE) {
                ThreadCall(this);
                return;
            }
            s = (State)0;
        } else {
            return;
        }
    } while (mState != s);
}

void XLSPConnection::Poll() {
    switch (mState) {
    case 0:
        if (mConnectionRequest == 3) {
            SetState((State)1);
        }
        return;
    case 1:
        if (mEnumHandle != INVALID_HANDLE_VALUE && mXOverlapped.InternalLow != ERROR_IO_PENDING) {
            DWORD count = 0;
            DWORD result = XGetOverlappedResult(&mXOverlapped, &count, false);
            if (result != 0) {
                XGetOverlappedExtendedError(&mXOverlapped);
            } else {
                memset(&mXOverlapped, 0, sizeof(XOVERLAPPED));
                if (count > 0) {
                    int idx = RandomInt(0, (int)count);
                    XTITLE_SERVER_INFO *servers = (XTITLE_SERVER_INFO *)mEnumBuffer;
                    int ret = XNetServerToInAddr(servers[idx].inaServer, mServiceId, (IN_ADDR *)&unk44);
                    if ((unsigned int)ret == 0) {
                        CloseHandle(mEnumHandle);
                        mEnumHandle = INVALID_HANDLE_VALUE;
                        if (mEnumBuffer) {
                            MemFree(mEnumBuffer, __FILE__, 0xAA);
                            mEnumBuffer = nullptr;
                        }
                        SetState((State)2);
                        return;
                    }
                }
            }
            SetState((State)4);
            return;
        }
        if (mConnectionRequest != 0)
            return;
        break;
    case 3:
        if (mConnectionRequest != 0) {
            DWORD status = XNetGetConnectStatus(*(IN_ADDR *)&unk44);
            if (status <= 1) {
                MILO_NOTIFY("XLSPConnection: Idle/establishing status while connected?");
            } else if (status == 2) {
                return;
            } else if (status == 3) {
            } else {
                MILO_NOTIFY("XNetGetConnectStatus() unhandled return: %d", status);
                return;
            }
            SetState((State)4);
            return;
        }
        break;
    case 2:
        if (mConnectionRequest != 0) {
            DWORD status = XNetGetConnectStatus(*(IN_ADDR *)&unk44);
            if (status == 0) {
            } else if (status == 1) {
                return;
            } else if (status < 3) {
                SetState((State)3);
                return;
            } else if (status == 3) {
            } else {
                MILO_NOTIFY("XNetGetConnectStatus() unhandled return: %d", status);
                return;
            }
            SetState((State)4);
            return;
        }
        break;
    case 4:
        if (mConnectionRequest != 0)
            return;
        break;
    default:
        return;
    }
    SetState((State)5);
}

void XLSPConnection::StartEnumeration() {
    DWORD res = XTitleServerCreateEnumerator(mServerInfo.c_str(), 8, &mEnumBufferSize, &mEnumHandle);
    if (res != ERROR_SUCCESS) {
        MILO_NOTIFY("XTitleServerCreateEnumerator failed with error %d", res);
        SetState((State)4);
    } else {
        mEnumBuffer = _MemAllocTemp(mEnumBufferSize, __FILE__, 0x1CB, "XLSPConnection", 0);
        res = XEnumerate(mEnumHandle, mEnumBuffer, mEnumBufferSize, nullptr, &mXOverlapped);
        if (res != ERROR_IO_PENDING) {
            MILO_NOTIFY("XEnumerate failed with error %d", res);
            SetState((State)4);
        }
    }
}

bool XLSPConnection::SecureDisconnect(in_addr a) {
    bool ret = true;
#ifdef HX_NATIVE
    auto it = mXLSPRefCountMap.find(a.s_addr);
#else
    auto it = mXLSPRefCountMap.find(a.s_un.s_addr);
#endif
    if (it != mXLSPRefCountMap.end()) {
        it->second--;
        if (it->second == 0) {
            mXLSPRefCountMap.erase(it);
            if (XNetGetConnectStatus(a) != 3) {
                XNetUnregisterInAddr(a);
            }
        }
    } else {
        ret = false;
        MILO_NOTIFY("XLSPConnection::SecureDisconnect() - connection not found!");
    }
    return ret;
}

int XLSPConnection::StartGatewayConnection(in_addr a) {
    int ret;
#ifdef HX_NATIVE
    auto it = mXLSPRefCountMap.find(a.s_addr);
#else
    auto it = mXLSPRefCountMap.find(a.s_un.s_addr);
#endif
    if (it != mXLSPRefCountMap.end()) {
        ret = 0;
        it->second++;
    } else {
        ret = XNetConnect(a);
        if (ret == 0) {
#ifdef HX_NATIVE
            mXLSPRefCountMap.insert(std::make_pair(a.s_addr, 1));
#else
            mXLSPRefCountMap.insert(std::make_pair(a.s_un.s_addr, 1));
#endif
        } else {
#ifdef HX_NATIVE
            unsigned char *bytes = (unsigned char *)&a.s_addr;
            MILO_NOTIFY(
                "XNetConnect(%d.%d.%d.%d) failed with %d",
                bytes[0],
                bytes[1],
                bytes[2],
                bytes[3],
                ret
            );
#else
            MILO_NOTIFY(
                "XNetConnect(%d.%d.%d.%d) failed with %d",
                a.s_un.s_un_b.s_b1,
                a.s_un.s_un_b.s_b2,
                a.s_un.s_un_b.s_b3,
                a.s_un.s_un_b.s_b4,
                ret
            );
#endif
        }
    }
    return ret;
}
