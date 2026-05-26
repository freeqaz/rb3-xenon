#include "net/DingoSvr.h"
#include "net/DingoJob.h"
#include "net/DingoAuthJob.h"
#include "WebSvcReq.h"
#include "WebSvcMgr.h"
#include "meta/ConnectionStatusPanel.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"
#include "os/System.h"
#include "utl/DataPointMgr.h"
#include "utl/Std.h"
#include "utl/Symbol.h"
#include <cstring>

DingoServer::DingoServer() : mAuthState(kServerUnauthed), mPort(0), mPendingPadNum(-1), mAuthedPadNum(-1) {
    for (int i = 0; i < DIM(mPadAuthed); i++) {
        mPadAuthed[i] = false;
    }
}

BEGIN_HANDLERS(DingoServer)
    HANDLE_MESSAGE(SigninChangedMsg)
    HANDLE_MESSAGE(ConnectionStatusChangedMsg)
    HANDLE_EXPR(is_authed, IsAuthenticated())
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

void DingoServer::Init() {
    SetName("server", ObjectDir::Main());
    mLocale = PlatformRegionToSymbol(ThePlatformMgr.GetRegion());
    ThePlatformMgr.AddSink(this, SigninChangedMsg::Type());
    ThePlatformMgr.AddSink(this, ConnectionStatusChangedMsg::Type());
    mLanguage = SystemLanguage();
}

void DingoServer::Logout() {
    unk40 = "";
    mAuthState = kServerUnauthed;
    mAuthedPadNum = -1;
    for (int i = 0; i < DIM(mPadAuthed); i++) {
        mPadAuthed[i] = false;
    }
    mOnlineId.Clear();
}

void DingoServer::ManageJob(DingoJob *job) {
    MILO_ASSERT(job, 0xd0);
    bool isUrlDisabled = false;
    FOREACH (it, mDisabledUrls) {
        String cur(*it);
        if (strncmp(job->GetBaseURL(), cur.c_str(), cur.length()) == 0) {
            isUrlDisabled = true;
            break;
        }
    }
    bool shouldSendFailureCallback = true;
    bool authSucceeded = true;
    bool justAuthenticated = false;
    if (!isUrlDisabled) {
        if (!IsAuthenticated()) {
            MILO_NOTIFY("ManageJob without authentication.");
            if (ThePlatformMgr.IsConnected()) {
                authSucceeded = TheServer.Authenticate(mAuthedPadNum);
                justAuthenticated = true;
            } else {
                authSucceeded = false;
            }
        }
        if (authSucceeded && !job->GetHttpReq()) {
            shouldSendFailureCallback = !InitAndAddJob(job, false, justAuthenticated);
        }
    }
    if (shouldSendFailureCallback) {
        job->SendCallback(false, false);
        delete job;
    }
}

void DingoServer::FillAuthParams(DataPoint &point) {
    static Symbol locale("locale");
    point.AddPair(locale, mLocale.c_str());
    static Symbol language("language");
    point.AddPair(language, mLanguage.c_str());
}

void DingoServer::DoAdditionalLogin() {
    MILO_ASSERT(mAuthUrl.length() > 0, 0xa9);
    MILO_ASSERT(mAuthState == kServerAuthed, 0xAA);
    if (mAuthState == kServerAuthed) {
        if (mAuthUrl.length() != 0) {
            for (int i = 0; i < 4; i++) {
                if (!mPadAuthed[i]) {
                    DataPoint pt;
                    if (FillAuthParamsFromPadNum(pt, i)
                        && SendAuthenticateMsg(mAuthUrl.c_str(), pt, nullptr)) {
                        mPadAuthed[i] = true;
                    }
                }
            }
        }
    }
}

void DingoServer::DelayJob(DingoJob *job) { mDelayedJobs.push_back(job); }

void DingoServer::CancelDelayedCalls() {
    FOREACH (it, mDelayedJobs) {
        DingoJob *cur = *it;
        cur->Cancel(true);
        delete cur;
    }
    mDelayedJobs.clear();
}

void DingoServer::AddDelayedCalls() {
    for (std::vector<DingoJob *>::iterator it = mDelayedJobs.begin();
         it != mDelayedJobs.end();
         ++it) {
        DingoJob *job = *it;
        if (!TheWebSvcMgr.AddRequest(job, job->GetTimeoutMs(), false, false)) {
            MILO_NOTIFY("Unable to add delayed job!");
            job->Cancel(true);
            delete job;
        }
    }
    mDelayedJobs.erase(mDelayedJobs.begin(), mDelayedJobs.end());
}

DataNode DingoServer::OnMsg(const ConnectionStatusChangedMsg &msg) {
    if (msg.Data()->Int(2) != 0) {
        return DataNode(0);
    }
    Disconnect();
    return DataNode(1);
}

DataNode DingoServer::OnMsg(const SigninChangedMsg &msg) {
    unsigned int signedInMask = msg.Data()->Int(2);
    unsigned int signinMask = msg.Data()->Int(3);

    if (!IsAuthenticated()) {
        return DataNode(0);
    }

    if (((1 << mAuthedPadNum) & signedInMask) == 0) {
        Logout();
    } else {
        for (int i = 0; i < 4; i++) {
            if (i != mAuthedPadNum && ((1 << i) & signinMask) != 0) {
                mPadAuthed[i] = false;
            }
        }
        DoAdditionalLogin();
    }
    return DataNode(1);
}

bool DingoServer::InitAndAddJob(DingoJob *job, bool immediate, bool delay) {
    ReqType reqType = kHttpReqType_POST;
    unsigned short port = GetPort();
    if (GetSSLEnable()) {
        reqType = kHttpReqType_PUT;
        port = 443;
    }
    bool initSuccess;
    if (GetIPAddr() != 0) {
        initSuccess = TheWebSvcMgr.InitRequest(job, reqType, GetIPAddr(), port, 0, 0);
    } else {
        initSuccess = TheWebSvcMgr.InitRequest(job, reqType, GetHostName(), port, 0, 0);
    }
    if (initSuccess) {
        job->SetUserAgent(mUserAgent.c_str());
        if (delay) {
            DelayJob(job);
            return true;
        }
        return TheWebSvcMgr.AddRequest(job, job->GetTimeoutMs(), immediate, false);
    }
    MILO_NOTIFY("InitAndAddJob failed.");
    return false;
}

DataNode DingoServer::OnMsg(const DingoJobCompleteMsg &msg) {
    if (mAuthState != kServerAuthenticating) {
        MILO_NOTIFY("Got auth response in wrong state: %d.", mAuthState);
    } else {
        if (msg.Data()->Int(3) != 0) {
            AuthenticateReqJob *job = dynamic_cast<AuthenticateReqJob *>(msg.Data()->Obj<DingoJob>(2));
            MILO_ASSERT(job, 0x14e);
            job->ParseResponse();
            if (job->mResult == 1) {
                unk40 = job->mSessionID.c_str();
                mAuthState = kServerAuthed;
                OnAuthSuccess();
                AddDelayedCalls();
                Handle(ServerStatusChangedMsg(kServerStatusConnected).Data(), false);
                DoAdditionalLogin();
            } else {
                DataPoint pt("svr_sent_non_success_on_auth");
                pt.AddPair("location", "DingoSvr::OnMsg1");
                pt.AddPair("result", job->mResult);
                pt.AddPair("response_str", job->GetResponseString());
                pt.AddPair("mBaseUrl", job->GetBaseURL());
                pt.AddPair("mResponseStatusCode", (int)job->GetResponseStatusCode());
                pt.AddPair("severity", "warn");
                pt.AddPair("project", "sync");
                TheDataPointMgr.RecordDebugDataPoint(pt);
                CancelDelayedCalls();
                TheWebSvcMgr.CancelOutstandingCalls();
                Disconnect();
                Handle(ServerStatusChangedMsg(kServerStatusDisconnected).Data(), false);
                return DataNode(0);
            }
        } else {
            SendDebugDataPoint("auth_msg_send_failure", "location", "DingoSvr::OnMsg2", "severity", "warn", "project", "sync");
            CancelDelayedCalls();
            TheWebSvcMgr.CancelOutstandingCalls();
            Disconnect();
            Handle(ServerStatusChangedMsg(kServerStatusDisconnected).Data(), false);
            return DataNode(0);
        }
    }
    return DataNode(1);
}
bool DingoServer::Authenticate(int padnum, const char *url) {
    if (mAuthState != 0) {
        return true;
    }

    mAuthState = kServerAuthenticating;
    mAuthUrl = url;

    DataPoint pt;
    if (padnum < 0) {
        FillAuthParams(pt);
    } else if (!FillAuthParamsFromPadNum(pt, padnum)) {
        return false;
    }
    return SendAuthenticateMsg(url, pt, this);
}

bool DingoServer::SendAuthenticateMsg(const char *url, DataPoint &pt, Hmx::Object *callback) {
    AuthenticateReqJob *job = new AuthenticateReqJob(url, pt, callback);
    return InitAndAddJob(job, true, false);
}
