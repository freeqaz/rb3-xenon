#include "net/DingoSvr_Xbox.h"
#include "meta/ConnectionStatusPanel.h"
#include "net/DingoJob.h"
#include "net/DingoSvr.h"
#include "net/SessionJobs_Xbox.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"
#include "os/System.h"
#include "utl/DataPointMgr.h"
#include "xdk/XAPILIB.h"

DingoSvrXbox gDingoSvrXbox;
DingoServer &TheServer = gDingoSvrXbox;

DingoSvrXbox::DingoSvrXbox()
    : mXLSPState(0), mXUID(0), mDingoServiceId(0), mJobMgr(this), mJobState(0), mScoreXUID(0), mCareerScore(0),
      mSessionHandle(0), mMsBetweenReconnDingo(0), mLeaderboardID(-1),
      mLeaderboardScorePropID(-1) {}

BEGIN_HANDLERS(DingoSvrXbox)
    HANDLE_MESSAGE(DingoJobCompleteMsg)
    HANDLE_MESSAGE(ConnectionStatusChangedMsg)
    HANDLE_SUPERCLASS(DingoServer)
END_HANDLERS

void DingoSvrXbox::Init() {
    DingoServer::Init();
    DataArray *cfg = SystemConfig("net", "dingo");
    mPort = cfg->FindInt("port");
    mHostName = cfg->FindStr("hostname");
    mUserAgent = cfg->FindStr("user_agent");
    mXLSPFilter = cfg->FindStr("xlsp_filter");
    mMsBetweenReconnDingo = cfg->FindFloat("ms_between_reconn_dingo");
}

bool DingoSvrXbox::Authenticate(int i1) {
    if (mXLSPState != 2) {
        SendDebugDataPoint(
            "no_xlsp_connection",
            "location",
            "DingoSvrXbox::Authenticate",
            "severity",
            "warn",
            "project",
            "sync"
        );
        Export(ServerStatusChangedMsg((ServerStatusResult)4), false);
        return false;
    } else {
        return DingoServer::Authenticate(i1, "auth/authenticate/");
    }
}

void DingoSvrXbox::Logout() {
    DingoServer::Logout();
    mXUID = 0;
    mUserName.erase();
}

void DingoSvrXbox::Disconnect() {
    mXLSPState = 0;
    mXLSPConnection.Disconnect();
}

bool DingoSvrXbox::HasValidLoginCandidate() const {
    u64 xuid;
    char buf[32];
    return GetValidLoginCandidate(buf, xuid) != -1;
}

bool DingoSvrXbox::IsValidLoginCandidate(int pad) const {
    if (!ThePlatformMgr.IsSignedIntoLive(pad)) {
        return false;
    } else {
        return !ThePlatformMgr.IsPadAGuest(pad);
    }
}

void DingoSvrXbox::MakeSessionJobComplete(bool b1) {
    if (b1 && mSessionHandle) {
        mJobMgr.QueueJob(new AddLocalPlayerJob(mSessionHandle, mAuthedPadNum, false));
        mJobState = 2;
    } else {
        mJobState = 0;
    }
}

void DingoSvrXbox::JoinSessionComplete(bool b1) {
    if (b1) {
        mJobMgr.QueueJob(new StartSessionJob(mSessionHandle));
        mJobState = 3;
    } else {
        mJobState = 0;
    }
}

void DingoSvrXbox::StartSessionComplete(bool b1) {
    MILO_ASSERT(mLeaderboardID != -1, 0x207);
    MILO_ASSERT(mLeaderboardScorePropID != -1, 0x208);
    if (b1) {
        mJobMgr.QueueJob(new WriteCareerLeaderboardJob(
            mSessionHandle, mLeaderboardID, mLeaderboardScorePropID, mScoreXUID, mCareerScore
        ));
        mJobState = 4;
    } else {
        mJobState = 0;
    }
}

void DingoSvrXbox::WriteCareerLeaderboardComplete(bool b1) {
    if (b1) {
        mJobMgr.QueueJob(new EndSessionJob(mSessionHandle));
        mJobState = 5;
    } else {
        mJobState = 0;
    }
}

void DingoSvrXbox::LeaveSessionComplete(bool b1) {
    if (b1) {
        mJobMgr.QueueJob(new DeleteSessionJob(mSessionHandle));
        mJobState = 7;
    } else {
        mJobState = 0;
    }
}

void DingoSvrXbox::EndSessionComplete(bool b1) {
    if (b1) {
        mJobMgr.QueueJob(new RemoveLocalPlayerJob(mSessionHandle, mAuthedPadNum));
        mJobState = 6;
    } else {
        mJobState = 0;
    }
}

void DingoSvrXbox::Poll() {
    if (!ThePlatformMgr.IsConnected()) {
        return;
    }
    mJobMgr.Poll();
    switch (mXLSPState) {
    case 0: {
        bool found;
        {
            String svc("dingo");
            found = ThePlatformMgr.GetServiceID(svc, (unsigned int &)mDingoServiceId);
        }
        if (found) {
            if (*mXLSPFilter.c_str() == '\0') {
                MILO_WARN("DingoSvrXbox: Empty XLSP filter string.");
            } else if ((unsigned int)mDingoServiceId == 0U) {
                MILO_NOTIFY("DingoSvrXbox: Invalid Dingo service ID.");
            } else {
                mXLSPConnection.Connect(mXLSPFilter.c_str(), mDingoServiceId);
                mXLSPState = 1;
            }
        }
        break;
    }
    case 1:
        if (mXLSPConnection.GetState() == 3) {
            mXLSPState = 2;
            mIPAddr = mXLSPConnection.GetServiceIP();
            mHostName.erase();
        }
        break;
    case 2:
        break;
    default:
        MILO_FAIL("DingoSvrXbox: State %d unhandled.", mXLSPState);
        break;
    }
    mXLSPConnection.Poll();
    if (mXLSPConnection.GetState() == 4
        && !(mXLSPConnection.mReconnectTimer.SplitMs() < mMsBetweenReconnDingo)) {
        Disconnect();
    }
}

void DingoSvrXbox::DeleteSessionComplete(bool b1) { mJobState = 0; }

void DingoSvrXbox::StartUploadCareerScore(u64 u) {
    if (mJobState == 0) {
        mCareerScore = u;
        mScoreXUID = mXUID;
        CreateSession();
    }
}

void DingoSvrXbox::FillAuthParams(DataPoint &pt) {
    DingoServer::FillAuthParams(pt);
    if (HasValidLoginCandidate()) {
        char buf[32];
        mPendingPadNum = GetValidLoginCandidate(buf, mXUID);
        mUserName = buf;
    }
    static Symbol username("username");
    pt.AddPair(username, mUserName.c_str());
    static Symbol platform_uid("platform_uid");
    String str70;
    if (mXUID == 0) {
        MILO_NOTIFY("Sending a zero XUID to the server!\n");
    }
    str70 << mXUID;
    pt.AddPair(platform_uid, str70.c_str());
}

bool DingoSvrXbox::FillAuthParamsFromPadNum(DataPoint &pt, int padnum) {
    DingoServer::FillAuthParams(pt);
    if (padnum < 0) {
        MILO_NOTIFY("Bad auth attempt with padnum = %d.", padnum);
        return false;
    } else {
        if (ThePlatformMgr.IsSignedIntoLive(padnum)
            && !ThePlatformMgr.IsPadAGuest(padnum)) {
            String str70;
            XUID xuid;
            HRESULT i3 = XUserGetXUID(padnum, &xuid);
            char name[32];
            HRESULT i4 = XUserGetName(padnum, name, 0x1E);
            bool ret;
            if (i3 == 0 && i4 == 0) {
                str70 = name;
                if (mAuthedPadNum == -1) {
                    mPendingPadNum = padnum;
                    mUserName = name;
                    mXUID = xuid;
                }
                static Symbol username("username");
                pt.AddPair(username, str70.c_str());
                static Symbol platform_uid("platform_uid");
                String str80;
                MILO_ASSERT(xuid, 0x101);
                str80 << xuid;
                pt.AddPair(platform_uid, str80.c_str());
                ret = true;
            } else {
                ret = false;
            }
            return ret;
        }
    }
    return false;
}

void DingoSvrXbox::OnAuthSuccess() {
    mPadAuthed[mPendingPadNum] = true;
    mOnlineId.SetXUID(mXUID);
    mOnlineId.SetPlayerName(ThePlatformMgr.GetName(mPendingPadNum));
    int old = mPendingPadNum;
    mPendingPadNum = -1;
    mAuthedPadNum = old;
}

void DingoSvrXbox::CreateSession() {
    XUserSetContext(mAuthedPadNum, 0x800A, 1);
    mJobMgr.QueueJob(new MakeSessionJob(&mSessionHandle, 0x706, mAuthedPadNum));
    mJobState = 1;
}

int DingoSvrXbox::GetValidLoginCandidate(char *name, u64 &xuid) const {
    for (int i = 0; i < 4; i++) {
        if (mPadAuthed[i]) {
            return -1;
        }
    }
    for (int i = 0; i < 4; i++) {
        if (ThePlatformMgr.IsSignedIntoLive(i) && !ThePlatformMgr.IsPadAGuest(i)) {
            HRESULT xRes = XUserGetXUID(i, &xuid);
            HRESULT nameRes = XUserGetName(i, name, 0x1E);
            if (xRes == 0 && nameRes == 0) {
                return i;
            }
        }
    }
    return -1;
}
