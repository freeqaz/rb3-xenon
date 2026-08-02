#pragma once
#include "beatmatch/TrackType.h"
#include "game/Defines.h"
#include "meta_band/GameplayOptions.h"
#include "meta_band/OvershellSlotState.h"
#include "meta_band/CharData.h"
#include "os/User.h"
#include "tour/TourCharLocal.h"
#include "tour/TourCharRemote.h"
#include "types.h"
#include "net/WiiFriendMgr.h"

class BandCharDesc;
class SessionMgr;
class LocalBandUser;
class RemoteBandUser;
class NullLocalBandUser;
class Player;
class Track;

class BandUser : public virtual User {
public:
    BandUser();
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual ~BandUser();
    // OVERRIDE of User::IsNullUser (User vtable slot 28 = 0x70) — NOT a BandUser-introduced
    // virtual. In rb3-Wii/TU0 this was BandUser's own slot 0; TU5 hoisted it into the User
    // virtual base, so retail dispatches it via the vbptr adjust + 0x70, not via BandUser's
    // own vtable slot 0. See the proof in src/system/os/User.h.
    // NOTE: this is an OVERRIDE of User::IsNullUser (a TU5 addition to the
    // virtual base — see os/User.h slot 28 / 0x70), NOT a new virtual introduced
    // here as rb3-Wii/TU0 had it. Consequently it does NOT occupy a slot in
    // BandUser's own vftable; retail reaches it through the vbptr/vbtable adjust.
    virtual bool IsNullUser() const { return false; }
    // BandUser's own vftable slot 0 (0x0), vacated when IsNullUser moved up to
    // User. Required to keep IsParticipating at the verified +0x8 (two slots
    // precede it; BandUserMgr::GetNumParticipants matches 100% emitting
    // `lwz r11,0x0(r3); lwz r11,0x8(r11)`). IDENTIFIED (lane NCCC-0731-5f08/f76):
    // retail InputMgr::IsActiveAndConnected and InputMgr::GetUserWithInvalidController
    // call this slot as `cur-><slot0>(mSessionMgr)` -- this=the BandUser subobject,
    // arg=the raw SessionMgr* -- where rb3-Wii dev source has
    // `mSessionMgr->HasUser(cur)`. Declared-only (out-of-line in retail); the body
    // is not needed for the dispatch-offset match. TODO(TU5): recover the real name.
    virtual bool IsInSession(SessionMgr *) const;
    virtual bool UnkTU5Virtual() const { return true; } // TODO: TU5-inserted virtual (confirmed via Ghidra decompile of retail TrackPanel::CreateTracks: IsParticipating() call resolves to vtable+0x8, not +0x4, so this slot precedes IsParticipating, not GetLocalBandUser; still nets the same +4 shift for everything after it, incl. GetLocalBandUser 0x18->0x1c). GameConfig::AutoAssignMissingSlots (va 0x82688e68) calls this slot (offset+0x4) in place of TheNetSession->HasUser(pUser) and tests the bool result -- real identity/name still unknown, signature widened from void to bool to match that call site.
    virtual bool IsParticipating() const { return mParticipating; }
    virtual int GetCurrentInstrumentCareerScore() const = 0;
    virtual int GetCurrentHardcoreIconLevel() const = 0;
    virtual int GetCymbalConfiguration() const = 0;
    virtual LocalBandUser *GetLocalBandUser() = 0;
    virtual LocalBandUser *GetLocalBandUser() const = 0;
    virtual RemoteBandUser *GetRemoteBandUser() = 0;
    virtual RemoteBandUser *GetRemoteBandUser() const = 0;
    virtual const std::vector<unsigned long long> &GetFriendsConsoleCodes() const = 0;
    virtual void Reset();
    virtual void SyncSave(BinStream &, unsigned int) const;

    const char *ProfileName() const;
    bool IsFullyInGame() const;
    ControllerType GetControllerType() const;
    Difficulty GetDifficulty() const;
    Symbol GetDifficultySym() const;
    TrackType GetTrackType() const;
    Symbol GetTrackSym() const;
    bool HasChar();
    void SetDifficulty(Difficulty);
    void UpdateData(unsigned int);
    void SetDifficulty(Symbol);
    void SetTrackType(TrackType);
    void SetTrackType(Symbol);
    const char *GetOvershellFocus();
    ScoreType GetPreferredScoreType() const;
    void SetPreferredScoreType(ScoreType);
    Symbol GetControllerSym() const;
    void SetControllerType(ControllerType);
    void SetControllerType(Symbol);
    void SetHasButtonGuitar(bool);
    void SetHas22FretGuitar(bool);
    CharData *GetChar();
    void SetChar(CharData *);
    const char *IntroName() const;
    int GetSlot() const;
    const char *GetTrackIcon() const;
    void SetOvershellSlotState(OvershellSlotStateID);
    GameplayOptions *GetGameplayOptions();
    void SetLoadedPrefabChar(int);
    void DeletePlayer();
    TourCharLocal *GetCharLocal();

    float GetLastHitFraction() const { return mLastHitFraction; }
    void SetLastHitFraction(float f) { mLastHitFraction = f; }
    Player *GetPlayer() const { return mPlayer; }
    Track *GetTrack() const { return mTrack; }
    void SetPlayer(Player *p) { mPlayer = p; }
    void SetTrack(Track *trk) { mTrack = trk; }
    void SetAutoplay(bool play) { mAutoplay = play; }
    void SetParticipating(bool part) { mParticipating = part; }
    OvershellSlotStateID GetOvershellState() const { return mOvershellState; }

    static LocalBandUser *NewLocalBandUser();
    static RemoteBandUser *NewRemoteBandUser();
    static NullLocalBandUser *NewNullLocalBandUser();

    DataNode OnSetDifficulty(DataArray *);
    DataNode OnSetTrackType(DataArray *);
    DataNode OnSetHas22FretGuitar(DataArray *);
    DataNode OnSetPreferredScoreType(DataArray *);
    DataNode OnSetControllerType(DataArray *);
    DataNode OnSetPrefabChar(DataArray *);

    Difficulty mDifficulty; // 0x8
    u8 unk_0xC; // 0xC
    TrackType mTrackType; // 0x10
    ControllerType mControllerType; // 0x14
    bool mHasButtonGuitar; // 0x18
    bool mHas22FretGuitar; // 0x19
    ScoreType mPreferredScoreType; // 0x1C
    OvershellSlotStateID mOvershellState; // 0x20
    String mOvershellFocus; // 0x24
    CharData *mChar; // 0x30 - CharData*
    GameplayOptions mGameplayOptions; // 0x34
    bool mAutoplay; // 0x70
    Symbol mPreviousAward; // 0x74
    float mLastHitFraction; // 0x78
    Track *mTrack; // 0x7c
    Player *mPlayer; // 0x80
    bool mParticipating; // 0x84
    bool mIsWiiRemoteController; // 0x85
    bool mJustDisconnected; // 0x86
};

class LocalBandUser : public virtual BandUser, public virtual LocalUser {
public:
    LocalBandUser();
    virtual DataNode Handle(DataArray *, bool);
    virtual ~LocalBandUser() {}
    virtual LocalBandUser *GetLocalBandUser();
    virtual LocalBandUser *GetLocalBandUser() const;
    virtual RemoteBandUser *GetRemoteBandUser();
    virtual RemoteBandUser *GetRemoteBandUser() const;
    virtual const std::vector<unsigned long long> &GetFriendsConsoleCodes() const;
    virtual void Reset();
    virtual ControllerType ConnectedControllerType() const;
    virtual int GetCurrentInstrumentCareerScore() const;
    virtual int GetCurrentHardcoreIconLevel() const;
    virtual int GetCymbalConfiguration() const;

    bool HasSeenRealGuitarPrompt() const;
    void SetHasSeenRealGuitarPrompt();
    void SetOvershellFocus(const char *);
    ControllerType DebugGetControllerTypeOverride() const;
    void DebugSetControllerTypeOverride(ControllerType);
    bool HasShownIntroHelp(TrackType) const;
    void SetShownIntroHelp(TrackType, bool);
    bool CanGetAchievements() const { return CanSaveData(); }
    // retail-360-only (target fn at 0x8268B2A8). Lives on LocalBandUser, NOT
    // BandUser: the retail body reaches GetPadNum through vbtable entry 0xc,
    // and BandUser's vbtable has only entries 0x0/0x4 (one virtual base).
    // LocalBandUser's entry 0xc is LocalUser (+256), whose vftable slot 0 is
    // LocalUser::GetPadNum -- exactly what the target calls.
    bool HasAsFriend(BandUser *) const;

    bool unkc; // 0x8   (compiler-verified; the old "0xc" comment was WRONG)
    bool mHasSeenRealGuitarPrompt; // 0x9
    std::set<TrackType> mShownIntrosSet; // 0xc .. 0x24  (_Rb_tree sizeof 0x18)
    // RETAIL HAS NO `ControllerType mControllerTypeOverride` HERE.  rb3-Wii DEV
    // carries it; the retail 360 build compiled it out.  PROVEN (lane CP-3B) by
    // disassembling retail's real ctor LocalBandUser::LocalBandUser @ 0x8268E678
    // -- its complete this-relative store census over [0x8,0x28) is:
    //     0x8  stb (=1)  unkc              0x1c stw       _M_node_count
    //     0x9  stb (=0)  mHasSeenRealGuitar 0x20 stb      _Rb_tree::_M_key_compare
    //     0xc/0x10/0x14/0x18  _M_header          (empty less<TrackType>, 1 BYTE)
    // and NOTHING at 0x24 but the vtordisp `stwx`.  The store at 0x20 being a
    // BYTE refutes the rival hypothesis that retail's _Rb_tree is 0x14 (comparator
    // empty-base-optimised) with the ControllerType member living at 0x20 -- that
    // would require a 4-byte `stw`.  Positive control: the same census DOES report
    // 4-byte `stw` members in this very range (0x1c, four bytes away), so it could
    // have returned the other answer.
    // Retail layout therefore: set 0xc..0x24, vtordisp(User) 0x24, User 0x28,
    // vtordisp(BandUser) 0x68, BandUser 0x6c, LocalUser 0x100 (no vtordisp),
    // sizeof = 0x10C -- confirmed by retail's real factory NewLocalBandUser
    // @ 0x8268EFD0 doing `li r3,0x10c`.  (The `li r3,0x110` cited by lane CO-1 as
    // proof of a coupled "tail defect" is at 0x8268F050, which is really
    // NewNullLocalBandUser -- sizeof(NullLocalBandUser) IS 0x110.  There is no
    // tail defect and the two halves are NOT coupled.)
};

class RemoteBandUser : public virtual BandUser, public virtual RemoteUser {
public:
    RemoteBandUser();
    virtual DataNode Handle(DataArray *, bool);
    virtual ~RemoteBandUser();
    virtual LocalBandUser *GetLocalBandUser();
    virtual LocalBandUser *GetLocalBandUser() const;
    virtual RemoteBandUser *GetRemoteBandUser();
    virtual RemoteBandUser *GetRemoteBandUser() const;
    virtual const std::vector<unsigned long long> &GetFriendsConsoleCodes() const;
    virtual int GetCurrentInstrumentCareerScore() const;
    virtual int GetCurrentHardcoreIconLevel() const;
    virtual int GetCymbalConfiguration() const;
    virtual void Reset();
    virtual void SyncLoad(BinStream &, unsigned int);

    void ShowCustomCharacter();

    DataNode OnMsg(const WiiFriendsListChangedMsg &);

    TourCharRemote *mRemoteChar; // 0xc
    std::vector<unsigned long long> mFriendsConsoleCodes; // 0x10
    bool unk18;
    bool unk19;
    bool unk1a;
    int mCurrentInstrumentCareerScore; // 0x1c
    int mCurrentHardcoreIconLevel; // 0x20
    unsigned int mCymbalConfiguration; // 0x24
};

class NullLocalBandUser : public LocalBandUser {
public:
    NullLocalBandUser() {}
    virtual ~NullLocalBandUser() {}
    virtual bool IsNullUser() const { return true; }
    virtual bool IsJoypadConnected() const { return false; }
    virtual bool CanSaveData() const { return false; }
    virtual const char *UserName() const { return ""; }
};

#include "obj/Msg.h"

DECLARE_MESSAGE(NewRemoteUserMsg, "new_remote_user")
NewRemoteUserMsg(RemoteUser *u) : Message(Type(), u) {}
RemoteUser *GetUser() const { return mData->Obj<RemoteUser>(2); }
END_MESSAGE

DECLARE_MESSAGE(RemovingRemoteUserMsg, "removing_remote_user")
RemovingRemoteUserMsg(RemoteUser *u) : Message(Type(), u) {}
RemoteUser *GetUser() const { return mData->Obj<RemoteUser>(2); }
END_MESSAGE

DECLARE_MESSAGE(RemoteUserUpdatedMsg, "remote_user_updated")
RemoteUserUpdatedMsg(RemoteUser *u) : Message(Type(), u) {}
END_MESSAGE

DECLARE_MESSAGE(LocalUserLeftMsg, "local_user_left")
LocalUserLeftMsg(LocalUser *u) : Message(Type(), u) {}
LocalUser *GetUser() const { return mData->Obj<LocalUser>(2); }
END_MESSAGE

DECLARE_MESSAGE(RemoteUserLeftMsg, "remote_user_left")
RemoteUserLeftMsg(RemoteUser *u) : Message(Type(), u) {}
RemoteUser *GetUser() const { return mData->Obj<RemoteUser>(2); }
END_MESSAGE

DECLARE_MESSAGE(RemoteLeaderLeftMsg, "remote_leader_left_msg")
RemoteLeaderLeftMsg() : Message(Type()) {}
END_MESSAGE

DECLARE_MESSAGE(UserLoginMsg, "user_login")
UserLoginMsg() : Message(Type()) {}
int GetPadNum() const { return mData->Int(2); }
END_MESSAGE

DECLARE_MESSAGE(AddUserResultMsg, "add_user_result")
AddUserResultMsg(int i) : Message(Type(), i) {}
AddUserResultMsg(int i, User *u) : Message(Type(), i, u) {}
int GetResult() const { return mData->Int(2); }
END_MESSAGE