#include "net/Synchronize.h"
#include "net/NetSession.h"
#include "net/SyncStore.h"
#include "os/Debug.h"
#include "os/UserMgr.h"

// Retail's release MILO_FAIL/MILO_WARN discarded the format string but still
// materialized the by-value String argument of the (inlined-away) formatter.
// A by-value class param of an inlined empty callee is what makes MSVC forward
// the copy-ctor's returned `this` in r3 straight into the dtor call (no reload
// of the temp's address) -- exactly what the target does here.
static inline void MiloDiscardedFmtArg(const char *, String) {}

Synchronizable::Synchronizable(const char *cc) : mDirtyMask(0) {
    if (strcmp(cc, ""))
        Publish(cc);
}

Synchronizable::~Synchronizable() {
    if (mTag.length() != 0)
        Unpublish();
}

void Synchronizable::Publish(const char *cc) {
    MILO_ASSERT(mTag.length() == 0, 0x1E);
    mTag = cc;
    MILO_ASSERT(mTag.length(), 0x20);
#ifndef HX_NATIVE
    // TheSyncStore is the network/ online sync registry — null/off-link on native.
    // Keep mTag bookkeeping (so ~Synchronizable's length check is consistent) but
    // skip the registry insert; nothing synchronizes offline.
    TheSyncStore->AddSyncObj(this, mTag);
#endif
}

void Synchronizable::Unpublish() {
    MILO_ASSERT(mTag.length(), 0x26);
#ifndef HX_NATIVE
    TheSyncStore->RemoveSyncObj(mTag);
#endif
    mTag = "";
}

void Synchronizable::SetSyncDirty(unsigned int ui, bool b) {
    if (HasSyncPermission()) {
        mDirtyMask |= ui;
        if (b)
            SynchronizeIfDirty();
    } else
        MiloDiscardedFmtArg(
            "Obj %s cannot SetSyncDirty without permission!", String(mTag)
        );
}

void Synchronizable::SynchronizeIfDirty() {
#ifdef HX_NATIVE
    // Online sync send (SyncObjMsg over TheNetSession to peers) — no offline meaning;
    // TheUserMgr/TheNetSession/SyncObjMsg live in the un-globbed network/ subsystem.
    mDirtyMask = 0;
    mDirtyUsers.clear();
#else
    while (!mDirtyUsers.empty()) {
        User *u = TheUserMgr->GetUser(mDirtyUsers.back(), true);
        if (HasSyncPermission() && u) {
            if (TheNetSession->HasUser(u) && ~(mDirtyMask)) {
                SyncObjMsg msg(mTag, ~(mDirtyMask), this);
                TheNetSession->SendMsg(u, msg, kReliable);
            }
        }
        mDirtyUsers.pop_back();
    }
    if (mDirtyMask) {
        if (HasSyncPermission()) {
            SyncObjMsg msg(mTag, mDirtyMask, this);
            TheNetSession->SendMsgToAll(msg, kReliable);
            OnSynchronizing(mDirtyMask);
        }
        mDirtyMask = 0;
    }
#endif
}

const char *Synchronizable::GetUniqueTag() const { return mTag.c_str(); }

void Synchronizable::AddDirtyUser(const UserGuid &user) {
    mDirtyUsers.push_back(user);
    SynchronizeIfDirty();
}
