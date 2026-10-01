#pragma once
#include "meta/FixedSizeSaveable.h"
#include "obj/Object.h"
#include "os/User.h"

enum ProfileSaveState {
    kMetaProfileUnloaded = 0,
    kMetaProfileLoaded = 1,
    kMetaProfileError = 2,
    kMetaProfileDelete = 3,
    kMetaProfileUnchanged = -1
};

class Profile : public FixedSizeSaveable, public virtual Hmx::Object {
public:
    Profile(int);
    // FixedSizeSaveable
    virtual ~Profile();
    // Hmx::Object
    virtual DataNode Handle(DataArray *, bool);
    // Profile
    virtual bool HasCheated() const { return false; }
    virtual bool IsUnsaved() const;
    virtual void SaveLoadComplete(ProfileSaveState);
    virtual bool HasSomethingToUpload() { return false; }
    virtual void DeleteAll() { mDirty = true; }
    virtual void PreLoad() {}

    bool IsAutosaveEnabled() const;
    bool HasValidSaveData() const;
    ProfileSaveState GetSaveState() const;
    void SetSaveState(ProfileSaveState);
    void MakeDirty();

    void SetDirty(bool b) { mDirty = b; }

    int GetPadNum() const;
    const char *GetName() const;

protected:
    mutable bool mDirty; // 0xc (mutable: written from const SaveFixed)
    mutable int mPadNum; // 0x10
    ProfileSaveState mState; // 0x14
};

#include "obj/Msg.h"

DECLARE_MESSAGE(ProfileSwappedMsg, "profile_swapped")
#ifdef HX_NATIVE
ProfileSwappedMsg(LocalUser *u1, LocalUser *u2) : Message(Type(), u1, u2) {}
#else
// Retail (0x8251CD08) stores both LocalUser pointers into the object nodes
// unadjusted -- no null test, no step to the virtual Hmx::Object base --
// i.e. the cast was resolved where LocalUser is an incomplete type.
ProfileSwappedMsg(LocalUser *u1, LocalUser *u2)
    : Message(
          Type(),
          reinterpret_cast<Hmx::Object *>(u1),
          reinterpret_cast<Hmx::Object *>(u2)
      ) {}
#endif
LocalUser *GetUser1() const;
LocalUser *GetUser2() const;
END_MESSAGE
