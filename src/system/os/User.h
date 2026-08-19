#pragma once
#include "obj/Object.h"
#include "utl/HxGuid.h"
#include "os/OnlineID.h"
#include "utl/Str.h"

// forward decs
class LocalUser;
class RemoteUser;

class User : public Hmx::Object {
public:
    OnlineID *mOnlineID; // 0x28
    UserGuid mUserGuid; // 0x30
    unsigned int mMachineID; // 0x3c

    User();
    // Hmx::Object
    virtual ~User() {}
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    // User
    virtual void Reset();
    virtual void SyncSave(BinStream &, unsigned int) const;
    virtual bool IsLocal() const = 0;
    virtual LocalUser *GetLocalUser() = 0;
    virtual const LocalUser *GetLocalUser() const = 0;
    virtual RemoteUser *GetRemoteUser() = 0;
    virtual const RemoteUser *GetRemoteUser() const = 0;
    // TU5-added User virtual, sits in the vtable immediately BEFORE UserName(),
    // shifting UserName's dispatch slot 0x70->0x74 (and only UserName's — IsLocal
    // @0x5c and GetLocalUser @0x64 are unchanged between TU0 and TU5, verified via
    // UserMgr::GetLocalUsers). This re-matches User::SyncSave and AppLabel::SetUserName
    // under TU5. Absent from DC3/rb3-Wii User (a TU0->TU5 patch addition). Declared-only
    // (defined out-of-line in retail); its body is not needed for the dispatch-offset
    // match.
    //
    // IDENTIFIED (lane NCCC-0731-5f08/f54): this slot is IsNullUser(). In rb3-Wii/TU0
    // IsNullUser was introduced by BandUser (BandUser's own vtable slot 0); the TU0->TU5
    // patch hoisted it up into the User virtual base. Proof, from retail
    // BandPerformer::ComputeScoreData: `user->UserName()` (a known User virtual) on a
    // BandUser* emits the 6-instruction virtual-base adjust
    //     lwz r11,4(user); lwz r11,4(r11); add r11,r11,user; addi r3,r11,4;
    //     lwz r11,4(r11); lwz r11,0x74(r11)
    // which matches retail BYTE-FOR-BYTE at slot 0x74 (= User vtable slot 29, UserName).
    // In the SAME function retail's `user->IsNullUser()` emits that identical sequence
    // with the final load at 0x70 (= slot 28, this one) — so IsNullUser is dispatched
    // through the User virtual base, not through BandUser's own vtable. Result is tested
    // with `clrlwi. r11, r3, 24` (byte/bool), consistent with the OvershellSlot::UpdateView
    // reading `if (user->IsLocal() && !user->IsNullUser())`.
    virtual bool IsNullUser() const;
    virtual const char *UserName() const = 0;

    unsigned int GetMachineID() const { return mMachineID; }
    void SetUserGuid(const UserGuid &);
    bool ComesBefore(const User *u) { return (mUserGuid < u->mUserGuid); }
    const UserGuid &GetUserGuid() const { return mUserGuid; }
    OnlineID *GetOnlineID() const { return mOnlineID; }
};

class LocalUser : public virtual User {
protected:
    bool mHasOnlineID;

public:
    LocalUser();
    virtual ~LocalUser() {}
    virtual DataNode Handle(DataArray *, bool);
    virtual int GetPadNum() const;
    virtual bool IsJoypadConnected() const;
    virtual bool HasOnlinePrivilege() const;
    virtual bool IsGuest() const;
    virtual bool IsSignedIn() const;
    virtual bool IsSignedInOnline() const;
    virtual bool CanSaveData() const;
    virtual const char *UserName() const;
    virtual bool IsLocal() const;
    virtual LocalUser *GetLocalUser();
    virtual const LocalUser *GetLocalUser() const;
    virtual RemoteUser *GetRemoteUser();
    virtual const RemoteUser *GetRemoteUser() const;

    // Online-ID refresh. Declared on LocalUser, NOT on the virtual base User.
    // The old placement on User claimed to "match rb3-Wii User" -- that comment
    // was simply wrong: rb3-Wii declares it inside LocalUser (os/User.h) and
    // defines it as `void LocalUser::UpdateOnlineID()` (os/User.cpp:67).
    // Retail agrees INDEPENDENTLY of the oracle: in NetSession::AddLocalUser
    // retail calls it with `mr r3, r30` -- the RAW LocalUser* -- whereas a
    // member of the virtual base User requires the 4-instruction adjust
    // (lwz vbptr / lwz off / add / addi 4) that our build was emitting.
    // That adjust also PROVED the pointer non-null, which made MSVC elide the
    // null check retail keeps on the following LocalUser*->const User*
    // conversion -- so one misplaced declaration caused both divergences.
    // Decl-only, exactly as before; all four call sites hold a LocalUser*.
    void UpdateOnlineID();
};

// this is...never used anywhere in DC3
class RemoteUser : public virtual User {
protected:
    class String mUserName;

public:
    RemoteUser() {}
    virtual ~RemoteUser() {}
    virtual bool IsLocal() const { return false; }
    virtual LocalUser *GetLocalUser() {
        MILO_FAIL("Bad Conversion");
        return nullptr;
    }
    virtual const LocalUser *GetLocalUser() const {
        MILO_FAIL("Bad Conversion");
        return nullptr;
    }
    virtual RemoteUser *GetRemoteUser() { return this; }
    virtual const RemoteUser *GetRemoteUser() const { return this; }
    virtual const char *UserName() const { return mUserName.c_str(); }
    // Retail 0x82523E28 (emitted into BandUser.obj, so it stays a header COMDAT):
    // ReadEndian(&mMachineID, 4) via the virtual base, then BinStream::operator>>
    // for mUserName, then the free operator>> for *mOnlineID.
    virtual void SyncLoad(BinStream &bs, unsigned int) {
        bs >> mMachineID >> mUserName;
        bs >> *mOnlineID;
    }
};
