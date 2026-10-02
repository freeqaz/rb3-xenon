#include "net/SessionJobs_Xbox.h"
#include "Core/CallContext.h"
#include "net/DingoSvr.h"
#include "net/MatchmakingSettings.h"
#include "net/NetSession.h"
#include "net/QuazalSession.h"
#include "net/XSessionData.h"
#include "obj/Object.h"
#include "xdk/win_types.h"
#include "xdk/XAPILIB.h"
#include "xdk/XNET.h"

// Callees retail reaches from MakeSessionJob that no symbol, string or RTTI
// identifies (the ones at 0x82A7xxxx-0x82AAxxxx sit in the /Od Quazal region).
// The declarations record only the call shapes retail uses; each is named for
// its retail address. (0x823EF400 and 0x82A8EEF8 are QosListenSetData and
// Quazal::RegisterXNetKey, declared in net/XSessionData.h.)
//   * the function-local static StationURL at 0x82CC0030: constructed by 0x82AA1658
//     (guard bit 0 of 0x82CC0094, destroyed through atexit), filled by
//     0x82AA2BE8 (this, &hostAddress, &sessionID, &keyExchangeKey), then
//     handed with a fresh Quazal::CallContext to Quazal::Session::JoinSession
//     (0x82A78668).
namespace Quazal {
    // The three members this TU calls (0x82AA1658, 0x82AA2BE8, the dtor), spelled
    // as the StationURL TU defines them.
    class StationURL {
    public:
        StationURL();
        ~StationURL();
        void SetXboxAddress(const XNADDR *, const XNKID *, const XNKEY *);
    };

    // The one Session member this TU calls (0x82A78668), as the Session TU
    // defines it.
    class Session {
    public:
        static bool JoinSession(CallContext *, const StationURL &);
    };
}

// ------------------------------------------------------------------ XboxJob

// retail's ctor stores only mSession (no mSuccess init); native keeps it
XboxJob::XboxJob(void *v)
    : mSession(v)
#ifdef HX_NATIVE
      ,
      mSuccess(true)
#endif
{
    memset(&mXOverlapped, 0, sizeof(XOVERLAPPED));
}

XboxJob::~XboxJob() {
    if (mXOverlapped.InternalLow == ERROR_IO_PENDING) {
        XCancelOverlapped(&mXOverlapped);
    }
}

void XboxJob::Cancel(Hmx::Object *) { XCancelOverlapped(&mXOverlapped); }

void XboxJob::CheckError(DWORD err, XOVERLAPPED *overlapped) {
    if (err && err != ERROR_IO_PENDING) {
        MILO_NOTIFY(
            "Error %i in Xbox Session: %x ", err, XGetOverlappedExtendedError(overlapped)
        );
#ifdef HX_NATIVE
        mSuccess = false;
#endif
    }
}

// ----------------------------------------------------------- MakeSessionJob

MakeSessionJob::MakeSessionJob(
    HANDLE *session,
    SessionSettings *settings,
    DWORD flags,
    DWORD userIndex,
    DWORD maxPublicSlots,
    DWORD publicPropertyId,
    XSessionData *data
)
    : mSession(session), mSettings(settings), mFlags(flags), mUserIndex(userIndex),
      mPublicPropertyId(publicPropertyId), mMaxPublicSlots(maxPublicSlots), mData(data),
      mJoinContext(nullptr), mSuccess(true) {
    memset(&mXOverlapped, 0, sizeof(XOVERLAPPED));
}

MakeSessionJob::~MakeSessionJob() { delete mJoinContext; }

void MakeSessionJob::CheckError(DWORD err, XOVERLAPPED *overlapped) {
    if (err && err != ERROR_IO_PENDING) {
        MILO_NOTIFY(
            "Error %i in Xbox Session: %x ", err, XGetOverlappedExtendedError(overlapped)
        );
    }
}

// --------------------------------------------------------- DeleteSessionJob

DeleteSessionJob::DeleteSessionJob(void *v) : mSession(v) {
    memset(&mXOverlapped, 0, sizeof(XOVERLAPPED));
    mStarted = false;
#ifdef HX_NATIVE
    mSuccess = true;
#endif
}

DeleteSessionJob::~DeleteSessionJob() {
    if (mStarted && mXOverlapped.InternalLow == ERROR_IO_PENDING) {
        XCancelOverlapped(&mXOverlapped);
    }
}

void DeleteSessionJob::OnCompletion(Hmx::Object *) {
    CloseHandle(mSession);
#ifdef HX_NATIVE
    TheServer.DeleteSessionComplete(mSuccess);
#endif
}

void DeleteSessionJob::Cancel(Hmx::Object *) {
    if (mStarted) {
        XCancelOverlapped(&mXOverlapped);
    }
}

void DeleteSessionJob::CheckError(DWORD err, XOVERLAPPED *overlapped) {
    if (err && err != ERROR_IO_PENDING) {
        MILO_NOTIFY(
            "Error %i in Xbox Session: %x ", err, XGetOverlappedExtendedError(overlapped)
        );
#ifdef HX_NATIVE
        mSuccess = false;
#endif
    }
}

// ------------------------------------------------------------- constructors

ModifySessionJob::ModifySessionJob(
    void *v, DWORD flags, DWORD maxPublicSlots, DWORD maxPrivateSlots
)
    : XboxJob(v), mFlags(flags), mMaxPublicSlots(maxPublicSlots),
      mMaxPrivateSlots(maxPrivateSlots) {}

AddLocalPlayerJob::AddLocalPlayerJob(void *v, int i, bool b)
    : XboxJob(v), mUserIndex(i), mPrivateSlot(b) {}

AddRemotePlayerJob::AddRemotePlayerJob(void *v, XUID xuid, bool b)
    : XboxJob(v), mXUID(xuid), mPrivateSlot(b) {}

RemoveLocalPlayerJob::RemoveLocalPlayerJob(void *v, int i)
    : XboxJob(v), mUserIndex(i) {}

RemoveRemotePlayerJob::RemoveRemotePlayerJob(void *v, XUID xuid)
    : XboxJob(v), mXUID(xuid) {}

RegisterArbitrationJob::RegisterArbitrationJob(void *v, ULONGLONG nonce)
    : XboxJob(v), mNonce(nonce), mResults(nullptr) {}

RegisterArbitrationJob::~RegisterArbitrationJob() { delete mResults; }

// The 12-byte OnCompletion at 0x823F6670 tail-calls 0x823E6DE0 on
// TheNetSession: an unported NetSession member that, on the host, begins the
// game-start countdown and otherwise enters kHostArbitrating and messages the
// host.
void RegisterArbitrationJob::OnCompletion(Hmx::Object *) {
    TheNetSession->OnRegisterArbitrationJobComplete();
}

WriteTrueSkillJob::WriteTrueSkillJob(void *v, XUID xuid, int b, int a, int viewId)
    : XboxJob(v), mXUID(xuid) {
    mUserProps[0].value.nData = a;
    mUserProps[1].value.nData = b;
    mUserProps[0].value.type = XUSER_DATA_TYPE_INT32;
    mUserProps[1].value.type = XUSER_DATA_TYPE_INT32;
    mSessionViewProp.dwViewId = viewId;
    mSessionViewProp.pProperties = mUserProps;
    mUserProps[0].dwPropertyId = 0x1000800A;
    mUserProps[1].dwPropertyId = 0x1000800B;
    mSessionViewProp.dwNumProperties = 2;
}

StartSessionJob::StartSessionJob(void *v) : XboxJob(v) {}

EndSessionJob::EndSessionJob(void *v) : XboxJob(v) {}

// ---------------------------------------------------------- polling bodies

bool XboxJob::IsFinished() {
    DWORD dw;
    DWORD res = XGetOverlappedResult(&mXOverlapped, &dw, false);
    bool result = res != ERROR_IO_INCOMPLETE;
    if (!result == false) {
        CheckError(res, &mXOverlapped);
    }
    return result;
}

void MakeSessionJob::Start() {
    if (mSession) {
        XUserSetContext(
            mUserIndex,
            X_CONTEXT_GAME_TYPE,
            (mFlags & XSESSION_CREATE_USES_ARBITRATION) ? X_CONTEXT_GAME_TYPE_RANKED
                                                        : X_CONTEXT_GAME_TYPE_STANDARD
        );
        XUserSetContext(mUserIndex, X_CONTEXT_GAME_MODE, mSettings->ModeFilter());
        DWORD isPublic = mSettings->mPublic != 0;
        XUserSetProperty(mUserIndex, mPublicPropertyId, sizeof(DWORD), &isPublic);
        for (int i = 0; i < mSettings->NumCustomSettings(); i++) {
            DWORD value = mSettings->GetCustomValue(i);
            XUserSetProperty(
                mUserIndex, mSettings->GetCustomID(i), sizeof(DWORD), &value
            );
        }
        DWORD res = XSessionCreate(
            mFlags,
            mUserIndex,
            mMaxPublicSlots,
            0,
            &mData->mNonce,
            &mData->mInfo,
            &mXOverlapped,
            mSession
        );
        CheckError(res, &mXOverlapped);
    }
}

bool MakeSessionJob::IsFinished() {
    DWORD dw;
    DWORD res = XGetOverlappedResult(&mXOverlapped, &dw, false);
    if (res == ERROR_IO_INCOMPLETE) {
        return false;
    }
    if (res != 0) {
        *mSession = INVALID_HANDLE_VALUE;
        mSession = nullptr;
        mSuccess = false;
    } else if (!(mFlags & XSESSION_CREATE_HOST)) {
        if (!mJoinContext) {
            if (mSession) {
                // a joining client: connect to the host the session describes
                XSESSION_INFO info = mData->mInfo;
                static Quazal::StationURL sTarget;
                sTarget.SetXboxAddress(
                    &info.hostAddress, &info.sessionID, &info.keyExchangeKey
                );
                mJoinContext = new Quazal::CallContext();
                Quazal::Session::JoinSession(mJoinContext, sTarget);
                return false;
            }
        } else {
            Quazal::CallContext::_State state = mJoinContext->GetState();
            if (state == Quazal::CallContext::CallPending) {
                return false;
            }
            mSuccess = state == Quazal::CallContext::CallSuccess;
        }
    }
    return true;
}

void MakeSessionJob::Cancel(Hmx::Object *) {
    if (!mJoinContext) {
        XCancelOverlapped(&mXOverlapped);
        if (mSession) {
            *mSession = INVALID_HANDLE_VALUE;
        }
    } else {
        QuazalSession::CancelJoinSession();
    }
    if (mFlags & XSESSION_CREATE_HOST) {
        TheNetSession->OnRegisterSessionJobComplete(false);
    } else {
        TheNetSession->OnConnectSessionJobComplete(false);
    }
}

void MakeSessionJob::OnCompletion(Hmx::Object *) {
    if (mSession && (mFlags & XSESSION_CREATE_HOST)) {
        XSESSION_INFO info = mData->mInfo;
        QosListenSetData(&info.sessionID, nullptr, 0);
        Quazal::RegisterXNetKey(&info.sessionID, &info.keyExchangeKey);
    }
    if (mFlags & XSESSION_CREATE_HOST) {
        TheNetSession->OnRegisterSessionJobComplete(mSuccess);
    } else {
        TheNetSession->OnConnectSessionJobComplete(mSuccess);
    }
}

// Retail's DeleteSessionJob::IsFinished (0x823F6C60) ends in `bl 0x823F6780`,
// XboxJob::IsFinished's body, although DeleteSessionJob is no XboxJob: its own
// polling helper is byte-identical (same XOVERLAPPED offset) and was folded
// into that address.
#pragma auto_inline(off)
bool DeleteSessionJob::OverlappedFinished() {
    DWORD dw;
    DWORD res = XGetOverlappedResult(&mXOverlapped, &dw, false);
    bool result = res != ERROR_IO_INCOMPLETE;
    if (!result == false) {
        CheckError(res, &mXOverlapped);
    }
    return result;
}
#pragma auto_inline(on)

bool DeleteSessionJob::IsFinished() {
    if (!mStarted) {
        if (!QuazalSession::StillDeleting()) {
            mStarted = true;
            DWORD res = XSessionDelete(mSession, &mXOverlapped);
            CheckError(res, &mXOverlapped);
        } else {
            return false;
        }
    }
    return OverlappedFinished();
}

// ------------------------------------------------------------------- Starts

void ModifySessionJob::Start() {
    DWORD res = XSessionModify(
        mSession, mFlags, mMaxPublicSlots, mMaxPrivateSlots, &mXOverlapped
    );
    CheckError(res, &mXOverlapped);
}

void AddLocalPlayerJob::Start() {
    DWORD res = XSessionJoinLocal(mSession, 1, &mUserIndex, &mPrivateSlot, &mXOverlapped);
    CheckError(res, &mXOverlapped);
}

#ifdef HX_NATIVE
void AddLocalPlayerJob::OnCompletion(Hmx::Object *) {
    TheServer.JoinSessionComplete(mSuccess);
}
#endif

void AddRemotePlayerJob::Start() {
    DWORD res = XSessionJoinRemote(mSession, 1, &mXUID, &mPrivateSlot, &mXOverlapped);
    CheckError(res, &mXOverlapped);
}

void RemoveLocalPlayerJob::Start() {
    DWORD res = XSessionLeaveLocal(mSession, 1, &mUserIndex, &mXOverlapped);
    CheckError(res, &mXOverlapped);
}

#ifdef HX_NATIVE
void RemoveLocalPlayerJob::OnCompletion(Hmx::Object *) {
    TheServer.LeaveSessionComplete(mSuccess);
}
#endif

void RemoveRemotePlayerJob::Start() {
    DWORD res = XSessionLeaveRemote(mSession, 1, &mXUID, &mXOverlapped);
    CheckError(res, &mXOverlapped);
}

// Two passes: the first, with no buffer and no XOVERLAPPED, only reports the
// results size.
void RegisterArbitrationJob::Start() {
    DWORD size = 0;
    XSessionArbitrationRegister(mSession, 0, mNonce, &size, nullptr, nullptr);
    mResults = (XSESSION_REGISTRATION_RESULTS *)operator new(size);
    DWORD res = XSessionArbitrationRegister(
        mSession, 0, mNonce, &size, mResults, &mXOverlapped
    );
    CheckError(res, &mXOverlapped);
}

void WriteTrueSkillJob::Start() {
    DWORD res = XSessionWriteStats(mSession, mXUID, 1, &mSessionViewProp, &mXOverlapped);
    CheckError(res, &mXOverlapped);
}

#ifdef HX_NATIVE
void WriteTrueSkillJob::OnCompletion(Hmx::Object *) {
    TheServer.WriteCareerLeaderboardComplete(mSuccess);
}
#endif

void StartSessionJob::Start() {
    DWORD res = XSessionStart(mSession, 0, &mXOverlapped);
    CheckError(res, &mXOverlapped);
}

#ifdef HX_NATIVE
void StartSessionJob::OnCompletion(Hmx::Object *obj) {
    TheServer.StartSessionComplete(mSuccess);
}
#endif

void EndSessionJob::Start() {
    DWORD res = XSessionEnd(mSession, &mXOverlapped);
    CheckError(res, &mXOverlapped);
}

#ifdef HX_NATIVE
void EndSessionJob::OnCompletion(Hmx::Object *) {
    TheServer.EndSessionComplete(mSuccess);
}
#endif
