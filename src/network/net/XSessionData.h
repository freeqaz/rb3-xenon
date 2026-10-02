#pragma once
#include "net/NetSession.h"
#include "xdk/XAPILIB.h"
#include "xdk/XNET.h"

// The Xbox SessionData (retail RTTI `.?AVXSessionData@@`, vtable 0x820582A4:
// slot 0 the shared `??_GSessionData` at 0x823EEAB8, slots 1-4 at 0x823EF2C8,
// 0x823EF4D8, 0x823EF538, 0x823EF338).  Only the two members MakeSessionJob
// hands to XSessionCreate are declared: Start (0x823F6840) passes `mData + 8`
// as pqwSessionNonce and `mData + 0x10` as pSessionInfo, and IsFinished /
// OnCompletion copy 0x3c bytes (sizeof(XSESSION_INFO)) from `mData + 0x10`.
class XSessionData : public SessionData {
public:
    virtual ~XSessionData() {}
    virtual void CopyInto(SessionData *);
    virtual void Save(BinStream &) const;
    virtual void Load(BinStream &);
    virtual bool Equals(const SessionData *) const;

    ULONGLONG mNonce; // 0x8
    XSESSION_INFO mInfo; // 0x10
};

// XNetQosListen on a session id (retail 0x823EF400 / 0x823EF450, in
// NetSession_Xbox.cpp's .text): enable it, advertising `data` when given, or
// release it. XboxSession (DeleteSession, FinishJoin, UpdateSettings) and
// MakeSessionJob::OnCompletion call them. The names are ours.
void QosListenSetData(const XNKID *, const void *, int);
void QosListenRelease(const XNKID *);

namespace Quazal {
    // Retail 0x82A8EEF8 (anonymous): XNetRegisterKey for a session id and its
    // key-exchange key. FinishJoin and MakeSessionJob::OnCompletion call it.
    void RegisterXNetKey(const XNKID *, const XNKEY *);
}
