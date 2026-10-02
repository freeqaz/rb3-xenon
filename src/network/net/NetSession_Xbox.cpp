// XboxSession and XSessionData (retail RTTI .?AVXboxSession@@ vtables
// 0x82058434 / 0x8205848C, .?AVXSessionData@@ vtable 0x820582A4).
// Retail .text from 0x823EEAB8; the XboxSession code continues past 0x823EF5F8.
#include "net/NetSession.h"
#include "meta_band/BandNetGameData.h"
#include "net/Net.h"
#include "net/SessionJobs_Xbox.h"
#include "net/XSessionData.h"
#include "os/PlatformMgr.h"
#include "synth_xbox/Mic.h"
#include "synth_xbox/Synth.h"
#include "xdk/xonline/xonline.h"

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
// OnSetPublic 0x823EF3C0, OnMsg(VoiceDataMsg) 0x823F0580.
// The bodies from 0x823EF5F8 on sit under other units' pins (CheatProvider,
// CharIKSliderMidi) and are not written here, nor are the session-job
// factory at 0x823EEB58 or PrepareConnectSessionJob.
class XboxSession : public NetSession {
public:
    XboxSession();
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

    // 0x823EEB58: builds the MakeSessionJob that creates (host) or joins the
    // XSession. Not written.
    Job *NewMakeSessionJob(bool host);
    // 0x823EF1E0: set or clear XSESSION_CREATE_* bits and push them to the
    // live session. Name descriptive.
    void SetSessionFlag(DWORD flag, bool clear);

    HANDLE mSessionHandle; // 0x74
    DWORD mSessionFlags; // 0x78
    HANDLE mJoinSessionHandle; // 0x7c
};

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
