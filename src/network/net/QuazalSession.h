#pragma once
#include "Core/CallContext.h"
#include "os/CritSec.h"
#include "utl/JobMgr.h"

// Retail deletes the terminating NetZ through vtable slot 0 (scalar deleting
// dtor) in QuazalSession::Poll (0x823F2B80); nothing else of the class is used
// from this TU.
#include "Core/NetZ.h"

// QuazalSession+0x4.  Offsets read off retail HasHostLeft (0x823F2C28): a bool
// at +0x8 guarded by the CriticalSection at +0x18; ~QuazalSession (0x823F2AC0)
// deletes it through vtable slot 0.
class NetZCallback {
public:
    virtual ~NetZCallback();
    int unk4; // 0x4
    bool mHostLeft; // 0x8
    int unkc; // 0xc
    int unk10; // 0x10
    int unk14; // 0x14
    CriticalSection mCritSec; // 0x18
};

class QuazalSession {
public:
    QuazalSession(bool);
    ~QuazalSession();
    bool HasHostLeft();
    bool HaveClientsLeft(std::vector<int> &);

    int unk0; // Quazal::NetZ
    NetZCallback *mCallback; // 0x4

    static void KillSession();
    static bool StillDeleting();
    static void Poll();
    static void CancelJoinSession();
    static Quazal::CallContext *mTerminatingContext;
    static Quazal::NetZ *mTerminatingNetZ;
};

class MakeQuazalSessionJob : public Job {
public:
    MakeQuazalSessionJob(QuazalSession **, bool);
    virtual ~MakeQuazalSessionJob() {}
    virtual void Start() {}
    virtual bool IsFinished();
    virtual void Cancel(Hmx::Object *);
    virtual void OnCompletion(Hmx::Object *);

    QuazalSession **mSessionAddress; // 0x8
    bool mHosting; // 0xc
};