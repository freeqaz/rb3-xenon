#pragma once
#include "obj/Object.h"
#include "utl/HxGuid.h"
#include "os/OnlineID.h"
#include "utl/Str.h"

// forward decs
class LocalUser;
class RemoteUser;

class User : public Hmx::Object {
protected:
    OnlineID *mOnlineID; // 0x2c
    UserGuid mUserGuid; // 0x30
    unsigned int mMachineID; // 0x40

public:
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
    virtual const char *UserName() const = 0;

    // unsigned int GetMachineID() const { return mMachineID; }
    // void SetUserGuid(const UserGuid &);
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
