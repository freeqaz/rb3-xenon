#pragma once
#include "obj/Object.h"
#include "utl/JobMgr.h"
#include "xdk/XAPILIB.h"
#include "xdk/XONLINE.h"

namespace Quazal {
    class CallContext;
}
class SessionSettings;
class XSessionData;

// The Xbox session jobs.  Every class here is read off retail RTTI and the
// vtables at 0x82059CB4..0x82059E0C; each table is exactly five slots (~Job,
// Start, IsFinished, Cancel, OnCompletion -- `Job`'s own virtuals), so nothing
// below adds a virtual.  Retail .text 0x823F6198..0x823F70E0.
//
// CheckError is NOT virtual: no table has a sixth slot, and every call site is
// a direct inlined `if (res && res != ERROR_IO_PENDING)
// XGetOverlappedExtendedError(ov)` on `this`.

// Retail `.?AVXboxJob@@` : Job.  Abstract (slot 1 is _purecall).  Every class
// derived from it that declares no destructor shares its deleting destructor
// `??_GXboxJob` at 0x823F6CE8 and its IsFinished (0x823F6780) and Cancel
// (0x823F6250); their slot 4 is Job's empty OnCompletion.
class XboxJob : public Job {
public:
    XboxJob(void *);
    virtual ~XboxJob();
    virtual bool IsFinished();
    virtual void Cancel(Hmx::Object *);
    void CheckError(DWORD, XOVERLAPPED *);

protected:
    XOVERLAPPED mXOverlapped; // 0x8 (ctor 0x823F6198: memset(this + 8, 0, 0x1c))
    HANDLE mSession; // 0x24
#ifdef HX_NATIVE
    // Not in retail (StartSessionJob is allocated at 0x28 bytes, 0x823EF0D0).
    bool mSuccess;
#endif
};

class StartSessionJob : public XboxJob {
public:
    StartSessionJob(void *v);
    virtual void Start();
#ifdef HX_NATIVE
    virtual void OnCompletion(Hmx::Object *);
#endif
};

class EndSessionJob : public XboxJob {
public:
    EndSessionJob(void *v);
    virtual void Start();
#ifdef HX_NATIVE
    virtual void OnCompletion(Hmx::Object *);
#endif
};

// 0x34 bytes (allocation at 0x823EF23C); arguments in XSessionModify order.
class ModifySessionJob : public XboxJob {
public:
    ModifySessionJob(void *, DWORD, DWORD, DWORD);
    virtual void Start();

protected:
    DWORD mFlags; // 0x28
    DWORD mMaxPublicSlots; // 0x2c
    DWORD mMaxPrivateSlots; // 0x30
};

// 0x70 bytes (allocation at 0x823EF9F0).  Ctor 0x823F6680 fills two INT32
// properties (ids 0x1000800A / 0x1000800B) under one view.
class WriteTrueSkillJob : public XboxJob {
public:
    WriteTrueSkillJob(void *, XUID, int, int, int);
    virtual void Start();
#ifdef HX_NATIVE
    virtual void OnCompletion(Hmx::Object *);
#endif

protected:
    XUID mXUID; // 0x28
    XUSER_PROPERTY mUserProps[2]; // 0x30
    XSESSION_VIEW_PROPERTIES mSessionViewProp; // 0x60
};

// 0x48 bytes (allocation at 0x823EEC3C).  Derives from Job, not XboxJob: its
// XOVERLAPPED is at 0x24 and its deleting destructor (0x823F67F0) is its own.
class MakeSessionJob : public Job {
public:
    MakeSessionJob(HANDLE *, SessionSettings *, DWORD, DWORD, DWORD, DWORD, XSessionData *);
    virtual ~MakeSessionJob();
    virtual void Start();
    virtual bool IsFinished();
    virtual void Cancel(Hmx::Object *);
    virtual void OnCompletion(Hmx::Object *);
    void CheckError(DWORD, XOVERLAPPED *);

protected:
    HANDLE *mSession; // 0x8
    SessionSettings *mSettings; // 0xc
    DWORD mFlags; // 0x10 (XSESSION_CREATE_*)
    DWORD mUserIndex; // 0x14
    DWORD mPublicPropertyId; // 0x18 (property set to mSettings->mPublic)
    DWORD mMaxPublicSlots; // 0x1c
    XSessionData *mData; // 0x20
    XOVERLAPPED mXOverlapped; // 0x24
    Quazal::CallContext *mJoinContext; // 0x40
    bool mSuccess; // 0x44
};

// 0x2c bytes (allocation at 0x823EF658).  Derives from Job, not XboxJob, and
// issues XSessionDelete lazily from IsFinished; Start is empty.
class DeleteSessionJob : public Job {
public:
    DeleteSessionJob(void *v);
    virtual ~DeleteSessionJob();
    virtual void Start() {}
    virtual bool IsFinished();
    virtual void Cancel(Hmx::Object *);
    virtual void OnCompletion(Hmx::Object *);
    void CheckError(DWORD, XOVERLAPPED *);

protected:
    bool OverlappedFinished();

    XOVERLAPPED mXOverlapped; // 0x8
    HANDLE mSession; // 0x24
    bool mStarted; // 0x28
#ifdef HX_NATIVE
    bool mSuccess;
#endif
};

// 0x30 bytes (allocation at 0x823EEDB8).
class AddLocalPlayerJob : public XboxJob {
public:
    AddLocalPlayerJob(void *, int, bool);
    virtual void Start();
#ifdef HX_NATIVE
    virtual void OnCompletion(Hmx::Object *);
#endif

protected:
    DWORD mUserIndex; // 0x28
    BOOL mPrivateSlot; // 0x2c
};

// 0x38 bytes (allocation at 0x823EEE74).
class AddRemotePlayerJob : public XboxJob {
public:
    AddRemotePlayerJob(void *, XUID, bool);
    virtual void Start();

protected:
    XUID mXUID; // 0x28
    BOOL mPrivateSlot; // 0x30
};

// 0x2c bytes (allocation at 0x823EEF5C).
class RemoveLocalPlayerJob : public XboxJob {
public:
    RemoveLocalPlayerJob(void *, int);
    virtual void Start();
#ifdef HX_NATIVE
    virtual void OnCompletion(Hmx::Object *);
#endif

protected:
    DWORD mUserIndex; // 0x28
};

// 0x30 bytes (allocation at 0x823EF024).
class RemoveRemotePlayerJob : public XboxJob {
public:
    RemoveRemotePlayerJob(void *, XUID);
    virtual void Start();

protected:
    XUID mXUID; // 0x28
};

// 0x38 bytes (allocation at 0x823EF180).  Owns the registration-results
// buffer Start allocates at the size XSessionArbitrationRegister asks for.
class RegisterArbitrationJob : public XboxJob {
public:
    RegisterArbitrationJob(void *, ULONGLONG);
    virtual ~RegisterArbitrationJob();
    virtual void Start();
    virtual void OnCompletion(Hmx::Object *);

protected:
    ULONGLONG mNonce; // 0x28
    DWORD mUnk30; // 0x30 (no retail code in this family reads or writes it)
    XSESSION_REGISTRATION_RESULTS *mResults; // 0x34
};
