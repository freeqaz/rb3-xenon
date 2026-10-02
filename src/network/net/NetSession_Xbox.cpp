// XboxSession and XSessionData (retail RTTI .?AVXboxSession@@ vtables
// 0x82058434 / 0x8205848C, .?AVXSessionData@@ vtable 0x820582A4).
// Retail .text 0x823EEAB8-0x823F0A00.
#include "net/NetSession.h"
#include "Platform/String.h"
#include "meta_band/BandNetGameData.h"
#include "net/MatchmakingSettings.h"
#include "net/Net.h"
#include "net/NetMessenger.h"
#include "net/NetSearchResult.h"
#include "net/SessionJobs_Xbox.h"
#include "net/XSessionData.h"
#include "obj/DataFunc.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"
#include "os/UserMgr.h"
#include "synth_xbox/Mic.h"
#include "synth_xbox/Synth.h"
#include "utl/MemStream.h"
#include "xdk/xapilibi/xbox.h"
#include "xdk/xnet/xnetapi.h"
#include "xdk/xonline/xonline.h"

// Quazal-side calls. The callees are anonymous in retail, so these are
// declared from their call sites, with the retail address beside each.
namespace Quazal {
    // 0x82A8F3D0: runs pending calls (0x82A8F300) until none is left;
    // returns whether any ran.
    bool FlushPendingCalls();
    // 0x82A8EEF8: XNetRegisterKey for a session id and its key-exchange key.
    void RegisterXNetKey(const XNKID *, const XNKEY *);
    // 0x82A8EC08 / 0x82A8ECB8: store a flag byte (0x82E1041B / 0x82E1041A).
    void SetXboxNetFlagA(bool);
    void SetXboxNetFlagB(bool);
    // 0x82A8EC88: clears the byte at 0x82C99240 that RegisterXNetKey tests.
    void ClearXNetKeyFlag();
    // 0x82A8F7C0 finds a named option; 0x82A8F788 sets it (false if locked).
    class Option {
    public:
        bool Set(bool);
    };
    Option *FindOption(const String &);
}

// ---------------------------------------------------------------------------
// XSessionData. Retail sizeof 0x50 (SessionData::New). Its vtable slot 0 is
// the shared SessionData deleting dtor at 0x823EEAB8: XSessionData's own is
// identical once the dead derived-vptr store goes, and ICF folds it there.

// 0x823EF2C8
void XSessionData::CopyInto(SessionData *data) {
    XSessionData *other = dynamic_cast<XSessionData *>(data);
    memcpy(&other->mInfo, &mInfo, sizeof(XSESSION_INFO));
    other->mNonce = mNonce;
}

// 0x823EF4D8
void XSessionData::Save(BinStream &bs) const {
    bs << mNonce;
    bs.Write(&mInfo, sizeof(XSESSION_INFO));
}

// 0x823EF538
void XSessionData::Load(BinStream &bs) {
    bs >> mNonce;
    bs.Read(&mInfo, sizeof(XSESSION_INFO));
}

// 0x823EF338: the session info alone decides it; the nonce is not compared.
bool XSessionData::Equals(const SessionData *data) const {
    const XSessionData *other = dynamic_cast<const XSessionData *>(data);
    return memcmp(&mInfo, &other->mInfo, sizeof(XSESSION_INFO)) == 0;
}

// 0x823EF498
SessionData *SessionData::New() { return new XSessionData(); }

// ---------------------------------------------------------------------------
// XboxSession. Its primary vtable 0x8205848C, slot by slot in NetSession's
// order: Poll 0x823F06F0, WriteStats 0x823EF9A8, SetInvitesAllowed 0x823EF3E0,
// InviteFriend (base, empty), PrepareRegisterHostSessionJob 0x823EF3B8,
// AddLocalToSession 0x823EED38, AddRemoteToSession 0x823EEE40,
// RemoveLocalFromSession 0x823EEEF8, RemoveRemoteFromSession 0x823EEFE8,
// StartSession 0x823EF0A8, EndSession 0x823F08D0, DeleteSession 0x823EF5F8,
// PrepareConnectSessionJob 0x823EECF8, FinishJoin 0x823EF6D8,
// PrepareRegisterArbitrationJob 0x823EF148, UpdateSettings 0x823EF7E8,
// OnSetPublic 0x823EF3C0, OnMsg(VoiceDataMsg) 0x823F0580. The Hmx::Object
// vtable 0x82058434 holds the $4 thunks to the deleting dtor (0x823EFF80 ->
// 0x823F0310) and Handle (0x823EFF90 -> 0x823EFB68). sizeof 0xCC
// (NetSession::New).
class XboxSession : public NetSession {
public:
    XboxSession();
    virtual DataNode Handle(DataArray *, bool);
    virtual ~XboxSession();
    virtual void Poll();
    virtual void WriteStats(const std::vector<UserStat> &);
    virtual void SetInvitesAllowed(bool);
    virtual Job *PrepareRegisterHostSessionJob();
    virtual void AddLocalToSession(LocalUser *);
    virtual void AddRemoteToSession(RemoteUser *);
    virtual void RemoveLocalFromSession(LocalUser *);
    virtual void RemoveRemoteFromSession(RemoteUser *);
    virtual void StartSession();
    virtual void EndSession(bool);
    virtual void DeleteSession();
    virtual Job *PrepareConnectSessionJob();
    virtual void FinishJoin(const JoinResponseMsg &);
    virtual Job *PrepareRegisterArbitrationJob();
    virtual void UpdateSettings();
    virtual void OnSetPublic(bool);
    virtual bool OnMsg(const VoiceDataMsg &);

    bool OnMsg(const SigninChangedMsg &);

    // 0x823EEB58: builds the MakeSessionJob that creates (host) or joins the
    // XSession.
    Job *NewMakeSessionJob(bool host);
    // 0x823EF1E0: set or clear XSESSION_CREATE_* bits and push them to the
    // live session. Name descriptive.
    void SetSessionFlag(DWORD flag, bool clear);
    // Voice chat. The names are ours.
    // 0x823EEB00: the remote user's privacy settings allow chat.
    bool HasChatPrivilege(User *);
    // 0x823F0108: some local user has the remote user on their mute list.
    bool IsMuted(User *);
    // 0x823F02B0
    bool CanTalkTo(User *);
    // 0x823F0360: deliver to each remote machine once, skipping machines
    // with a user we cannot talk to.
    void SendToTalkers(const NetMessage &);

    HANDLE mSessionHandle; // 0x74
    DWORD mSessionFlags; // 0x78
    HANDLE mJoinSessionHandle; // 0x7c
    // 0x80: the serialized NetSearchResult advertised through QoS listening.
    MemStream mSearchResultStream;
};

// 0x82C6EC0B; set by the set_reliable DataFunc.
static bool sVoiceReliable = true;

// 0x823EF400. Retail keeps both QoS helpers out of line.
__declspec(noinline) static void
QosListenSetData(const XNKID *id, const void *data, int size) {
    DWORD flags = XNET_QOS_LISTEN_ENABLE;
    if (data)
        flags = XNET_QOS_LISTEN_ENABLE | XNET_QOS_LISTEN_SET_DATA;
    int err = XNetQosListen(id, (const BYTE *)data, size, 0, flags);
    if (err != 0 && err != ERROR_IO_PENDING)
        MILO_WARN("XNetQosListen failed: %x", XGetOverlappedExtendedError(nullptr));
}

// 0x823EF450
__declspec(noinline) static void QosListenRelease(const XNKID *id) {
    int err = XNetQosListen(id, nullptr, 0, 0, XNET_QOS_LISTEN_RELEASE);
    if (err != 0 && err != ERROR_IO_PENDING)
        MILO_WARN("XNetQosListen failed: %x", XGetOverlappedExtendedError(nullptr));
}

// 0x823EF590
static DataNode SetReliable(DataArray *a) {
    sVoiceReliable = a->Int(1);
    return DataNode(0);
}

// 0x823EEB00
bool XboxSession::HasChatPrivilege(User *user) {
    MILO_ASSERT(!user->IsLocal(), 0);
    return ThePlatformMgr.CanCommunicateWith(user->GetOnlineID());
}

// 0x823EEB58
Job *XboxSession::NewMakeSessionJob(bool host) {
    int pad = mLocalHost->GetPadNum();
    mSessionFlags = 0;
    if (host)
        mSessionFlags = XSESSION_CREATE_HOST;
    mSessionFlags |= XSESSION_CREATE_INVITES_DISABLED | XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED;
    if (!mSettings->Ranked())
        mSessionFlags |= XSESSION_CREATE_USES_PRESENCE | XSESSION_CREATE_USES_STATS
            | XSESSION_CREATE_USES_MATCHMAKING | XSESSION_CREATE_USES_PEER_NETWORK;
    else
        mSessionFlags |= XSESSION_CREATE_USES_PRESENCE | XSESSION_CREATE_USES_STATS
            | XSESSION_CREATE_USES_MATCHMAKING | XSESSION_CREATE_USES_ARBITRATION
            | XSESSION_CREATE_USES_PEER_NETWORK;
    HANDLE *handle;
    XSessionData *data;
    if (host) {
        handle = &mSessionHandle;
        data = dynamic_cast<XSessionData *>(mData);
    } else {
        handle = &mJoinSessionHandle;
        data = dynamic_cast<XSessionData *>(mJoinData);
    }
    if (!ThePlatformMgr.IsUserSignedIn(mLocalHost) || ThePlatformMgr.IsUserAGuest(mLocalHost))
        handle = nullptr;
    return new MakeSessionJob(
        handle,
        mSettings,
        mSessionFlags,
        pad,
        TheNet.GetGameData()->GetNumPlayersAllowed(),
        TheNet.GetGameData()->PublicPropertyID(),
        data
    );
}

// 0x823EECF8
Job *XboxSession::PrepareConnectSessionJob() {
    Quazal::FlushPendingCalls();
    return NewMakeSessionJob(false);
}

// 0x823EF3E0
void XboxSession::SetInvitesAllowed(bool allowed) {
    if (mSessionHandle != INVALID_HANDLE_VALUE)
        SetSessionFlag(XSESSION_CREATE_INVITES_DISABLED, allowed);
}

// 0x823EF3C0
void XboxSession::OnSetPublic(bool isPublic) {
    if (mSessionHandle != INVALID_HANDLE_VALUE)
        SetSessionFlag(XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED, isPublic);
}

// 0x823EF3B8
Job *XboxSession::PrepareRegisterHostSessionJob() { return NewMakeSessionJob(true); }

// 0x823EED38
void XboxSession::AddLocalToSession(LocalUser *user) {
    NetSession::AddLocalToSession(user);
    if (IsOnlineEnabled()) {
        MILO_ASSERT(ThePlatformMgr.UserHasOnlinePrivilege(user), 0);
        int pad = user->GetPadNum();
        ThePlatformMgr.SetRankedContext(user, mSettings->Ranked());
        ThePlatformMgr.SetGameModeContext(user, mSettings->ModeFilter());
        mJobMgr.QueueJob(new AddLocalPlayerJob(mSessionHandle, pad, false));
        TheXboxSynth->ActivateLocalChat(pad, true);
    }
}

// 0x823EEE40
void XboxSession::AddRemoteToSession(RemoteUser *user) {
    NetSession::AddRemoteToSession(user);
    XUID xuid = user->GetOnlineID()->GetXUID();
    mJobMgr.QueueJob(new AddRemotePlayerJob(mSessionHandle, xuid, false));
    MicManagerXbox::GetInstance()->AddRemoteMic(xuid, 0);
}

// 0x823EEEF8
void XboxSession::RemoveLocalFromSession(LocalUser *user) {
    int pad = user->GetPadNum();
    if (IsOnlineEnabled() && ThePlatformMgr.IsUserSignedIn(user)
        && mSessionHandle != INVALID_HANDLE_VALUE) {
        mJobMgr.QueueJob(new RemoveLocalPlayerJob(mSessionHandle, pad));
    }
    TheXboxSynth->ActivateLocalChat(pad, false);
    NetSession::RemoveLocalFromSession(user);
}

// 0x823EEFE8
void XboxSession::RemoveRemoteFromSession(RemoteUser *user) {
    XUID xuid = user->GetOnlineID()->GetXUID();
    if (mSessionHandle != INVALID_HANDLE_VALUE)
        mJobMgr.QueueJob(new RemoveRemotePlayerJob(mSessionHandle, xuid));
    MicManagerXbox::GetInstance()->RemoveRemoteMic(xuid);
    NetSession::RemoveRemoteFromSession(user);
}

// 0x823EF0A8
void XboxSession::StartSession() {
    if (mSessionHandle != INVALID_HANDLE_VALUE)
        mJobMgr.QueueJob(new StartSessionJob(mSessionHandle));
}

// 0x823EF148
Job *XboxSession::PrepareRegisterArbitrationJob() {
    XSessionData *data = dynamic_cast<XSessionData *>(mData);
    return new RegisterArbitrationJob(mSessionHandle, data->mNonce);
}

// 0x823EF1E0
void XboxSession::SetSessionFlag(DWORD flag, bool clear) {
    if (IsOnlineEnabled() && !mSettings->Ranked()) {
        DWORD oldFlags = mSessionFlags;
        if (clear)
            mSessionFlags = oldFlags & ~flag;
        else
            mSessionFlags = oldFlags | flag;
        if (oldFlags != mSessionFlags) {
            mJobMgr.QueueJob(new ModifySessionJob(
                mSessionHandle,
                mSessionFlags,
                TheNet.GetGameData()->GetNumPlayersAllowed(),
                0
            ));
        }
    }
}

// 0x823EF5F8
void XboxSession::DeleteSession() {
    if (mSessionHandle != INVALID_HANDLE_VALUE) {
        if (IsHost()) {
            Quazal::FlushPendingCalls();
            XSessionData *data = dynamic_cast<XSessionData *>(mData);
            QosListenRelease(&data->mInfo.sessionID);
        }
        mJobMgr.QueueJob(new DeleteSessionJob(mSessionHandle));
        mSessionHandle = INVALID_HANDLE_VALUE;
    }
}

// 0x823EF6D8
void XboxSession::FinishJoin(const JoinResponseMsg &msg) {
    HANDLE oldHandle;
    if (msg.Joined()) {
        if (mData) {
            XSessionData *data = dynamic_cast<XSessionData *>(mData);
            QosListenRelease(&data->mInfo.sessionID);
        }
        oldHandle = mSessionHandle;
        mSessionHandle = mJoinSessionHandle;
    } else {
        if (mData) {
            XSessionData *data = dynamic_cast<XSessionData *>(mData);
            Quazal::RegisterXNetKey(&data->mInfo.sessionID, &data->mInfo.keyExchangeKey);
        }
        oldHandle = mJoinSessionHandle;
        mSessionFlags |= XSESSION_CREATE_HOST;
    }
    mJoinSessionHandle = INVALID_HANDLE_VALUE;
    if (oldHandle != INVALID_HANDLE_VALUE)
        mJobMgr.QueueJob(new DeleteSessionJob(oldHandle));
}

// 0x823EF7E8
void XboxSession::UpdateSettings() {
    if (IsHost() && !IsBusy() && mSessionHandle != INVALID_HANDLE_VALUE) {
        NetSearchResult *result = NetSearchResult::New();
        mData->CopyInto(result->mSessionData);
        mSearchResultStream.Seek(0, BinStream::kSeekBegin);
        result->Save(mSearchResultStream);
        XSessionData *data = dynamic_cast<XSessionData *>(mData);
        QosListenSetData(
            &data->mInfo.sessionID,
            mSearchResultStream.Buffer(),
            mSearchResultStream.Size()
        );
        delete result;
    }
    int pad = mLocalHost->GetPadNum();
    XUserSetContext(pad, X_CONTEXT_GAME_MODE, mSettings->ModeFilter());
    DWORD isPublic = mSettings->mPublic != 0;
    XUserSetProperty(pad, TheNet.GetGameData()->PublicPropertyID(), sizeof(isPublic), &isPublic);
    for (int i = 0; i < mSettings->NumCustomSettings(); i++) {
        int value = mSettings->GetCustomValue(i);
        // Retail calls GetPadNum here and drops the result.
        MILO_ASSERT(mLocalHost->GetPadNum() == pad, 0);
        XUserSetProperty(pad, mSettings->GetCustomID(i), sizeof(value), &value);
    }
}

// 0x823EF9A8
void XboxSession::WriteStats(const std::vector<UserStat> &stats) {
    MILO_ASSERT(IsInGame(), 0);
    if (mSessionHandle != INVALID_HANDLE_VALUE) {
        for (int i = 0; i < stats.size(); i++) {
            const UserStat &stat = stats[i];
            mJobMgr.QueueJob(new WriteTrueSkillJob(
                mSessionHandle,
                stat.mUser->GetOnlineID()->GetXUID(),
                stat.mTeam,
                stat.mScore,
                stat.mViewID
            ));
        }
    }
}

// 0x823EFAA8: a signin change on the host's pad drops the session.
bool XboxSession::OnMsg(const SigninChangedMsg &msg) {
    if (mSessionHandle && mSessionHandle != INVALID_HANDLE_VALUE) {
        int pad = mLocalHost->GetPadNum();
        if (msg.GetChangedMask() & (1 << pad)) {
            if (mJoinSessionHandle != INVALID_HANDLE_VALUE)
                mJoinSessionHandle = INVALID_HANDLE_VALUE;
            else
                mSessionHandle = INVALID_HANDLE_VALUE;
            Disconnect();
        }
    }
    MsgSource::Handle(msg, false);
    return true;
}

// 0x823EFB68
BEGIN_HANDLERS(XboxSession)
    HANDLE_MESSAGE(SigninChangedMsg)
    HANDLE_SUPERCLASS(NetSession)
    HANDLE_CHECK(0)
END_HANDLERS

// 0x823EFD28
XboxSession::XboxSession()
    : mSessionHandle(INVALID_HANDLE_VALUE), mJoinSessionHandle(INVALID_HANDLE_VALUE),
      mSearchResultStream(false) {
    static Symbol signin_changed("signin_changed");
    ThePlatformMgr.AddSink(this, signin_changed);
    Quazal::SetXboxNetFlagA(true);
    Quazal::ClearXNetKeyFlag();
    Quazal::Option *option = Quazal::FindOption("UseDistinctPortForStreamManager");
    option->Set(true);
    Quazal::SetXboxNetFlagB(true);
    DataRegisterFunc("set_reliable", SetReliable);
}

// 0x823EFFA8
XboxSession::~XboxSession() {
    DeleteSession();
    static Symbol signin_changed("signin_changed");
    ThePlatformMgr.RemoveSink(this, signin_changed);
}

// 0x823F0108
bool XboxSession::IsMuted(User *user) {
    MILO_ASSERT(!user->IsLocal(), 0);
    XUID xuid = user->GetOnlineID()->GetXUID();
    std::vector<LocalUser *> users;
    GetLocalUserList(users);
    FOREACH (it, users) {
        int pad = (*it)->GetPadNum();
        BOOL muted = 0;
        XUserMuteListQuery(pad, xuid, &muted);
        if (muted)
            return true;
    }
    return false;
}

// 0x823F0238
NetSession *NetSession::New() { return new XboxSession(); }

// 0x823F02B0
bool XboxSession::CanTalkTo(User *user) { return !IsMuted(user) && HasChatPrivilege(user); }

// 0x823F0360
void XboxSession::SendToTalkers(const NetMessage &msg) {
    std::vector<unsigned int> machines;
    std::vector<User *> users;
    GetUserList(users);
    FOREACH (it, users) {
        if (!(*it)->IsLocal() && !CanTalkTo(*it))
            machines.push_back((*it)->GetMachineID());
    }
    FOREACH (it, users) {
        if (!(*it)->IsLocal()) {
            bool found = false;
            FOREACH (m, machines) {
                if (*m == (*it)->GetMachineID()) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                TheNetMessenger.DeliverMsg(
                    (*it)->GetMachineID(), msg, sVoiceReliable ? kReliable : kUnreliable
                );
                machines.push_back((*it)->GetMachineID());
            }
        }
    }
}

// 0x823F0580
bool XboxSession::OnMsg(const VoiceDataMsg &msg) {
    MicManagerXbox *mics = MicManagerXbox::GetInstance();
    String guid(msg.mUserGuid.ToString());
    User *user = TheUserMgr->GetUser(msg.mUserGuid, false);
    if (!user || !HasUser(user)) {
        MILO_WARN("Voice data from unknown user %s", guid);
        return false;
    }
    MILO_ASSERT(!user->IsLocal(), 0);
    if (CanTalkTo(user)) {
        XUID xuid = user->GetOnlineID()->GetXUID();
        MemStream data(false);
        msg.GetVoiceData(data);
        mics->AddRemoteChatData(xuid, data.Buffer(), data.Size());
    }
    return true;
}

// 0x823F06F0
void XboxSession::Poll() {
    NetSession::Poll();
    if (!IsJoining()) {
        std::vector<LocalUser *> users;
        GetLocalUserList(users);
        static DataNode &fake_controllers = DataVariable("fake_controllers");
        FOREACH (it, users) {
            int pad = (*it)->GetPadNum();
            if (!fake_controllers.Int() || pad != -1) {
                char buf[100];
                int size = TheXboxSynth->GetChatData(pad, buf, sizeof(buf));
                if (size > 0) {
                    VoiceDataMsg voiceMsg(buf, size, *it);
                    SendToTalkers(voiceMsg);
                }
            }
        }
    }
}

// 0x823F08D0
void XboxSession::EndSession(bool writeStats) {
    if (mSessionHandle != INVALID_HANDLE_VALUE && writeStats) {
        std::vector<UserStat> stats;
        TheNet.GetGameData()->GetEndGameStats(stats);
        WriteStats(stats);
    }
    if (mSessionHandle != INVALID_HANDLE_VALUE)
        mJobMgr.QueueJob(new EndSessionJob(mSessionHandle));
}
