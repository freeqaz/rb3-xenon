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
    OnlineID *mOnlineID; // 0x2c
    UserGuid mUserGuid; // 0x30
    unsigned int mMachineID; // 0x40

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
    // match. TODO(TU5): recover the real name/semantics of this virtual.
    virtual const char *UnkTU5Virtual_beforeUserName() const;
    virtual const char *UserName() const = 0;

    unsigned int GetMachineID() const { return mMachineID; }
    void SetUserGuid(const UserGuid &);
    bool ComesBefore(const User *u) { return (mUserGuid < u->mUserGuid); }
    const UserGuid &GetUserGuid() const { return mUserGuid; }
    OnlineID *GetOnlineID() const { return mOnlineID; }
    // Wii-only online-ID refresh, referenced by Wii profile-swap game paths
    // (e.g. OvershellSlot::SelectGuestProfile). Decl-only; matches rb3-Wii User.
    void UpdateOnlineID();
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
    virtual void SyncLoad(BinStream &, unsigned int) {}
};
