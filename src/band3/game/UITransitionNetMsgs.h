#pragma once
#include "meta_band/LockMessages.h"
#include "ui/UI.h"
#include "ui/UIComponent.h"
#include "ui/UIScreen.h"

class StartTransitionMsg : public StartLockMsg, public LockData {
public:
    StartTransitionMsg() {}
    StartTransitionMsg(UIScreen *);
    // NO user dtor: retail's ??1 (0x82522AE0, 72B) is the COMPILER-GENERATED
    // implicit dtor — a real body exists only because mScreenName needs
    // destruction, and it has NO own-vtable stores (the user-empty-dtor
    // signature). Declaring `virtual ~StartTransitionMsg() {}` adds the
    // ??_7StartTransitionMsg vtable-store block and drops the match to 21.6%.
    virtual void Save(BinStream &) const;
    virtual void Load(BinStream &);
    virtual LockData *GetLockData();

    UIScreen *GetScreen() const;

    String mScreenName; // 0x1c
};

class NetGotoScreenMsg : public StartTransitionMsg {
public:
    NetGotoScreenMsg() {}
    NetGotoScreenMsg(UIScreen *, bool, bool);
    virtual void Save(BinStream &) const;
    virtual void Load(BinStream &);
    NETMSG_BYTECODE(NetGotoScreenMsg);
    NETMSG_NAME(NetGotoScreenMsg);
    NETMSG_NEWNETMSG(NetGotoScreenMsg);

    bool mForce; // 0x28
    bool mBack; // 0x29
};

class NetSyncScreenMsg : public StartTransitionMsg {
public:
    NetSyncScreenMsg() {}
    NetSyncScreenMsg(UIScreen *, int);
    virtual void Save(BinStream &) const;
    virtual void Load(BinStream &);
    NETMSG_BYTECODE(NetSyncScreenMsg);
    NETMSG_NAME(NetSyncScreenMsg);
    NETMSG_NEWNETMSG(NetSyncScreenMsg);

    bool mDepth; // 0x28
};

class NetPushScreenMsg : public StartTransitionMsg {
public:
    NetPushScreenMsg() {}
    NetPushScreenMsg(UIScreen *s) : StartTransitionMsg(s) {}
    NETMSG_BYTECODE(NetPushScreenMsg);
    NETMSG_NAME(NetPushScreenMsg);
    NETMSG_NEWNETMSG(NetPushScreenMsg);
};

class NetPopScreenMsg : public StartTransitionMsg {
public:
    NetPopScreenMsg() : StartTransitionMsg(nullptr) {}
    NetPopScreenMsg(UIScreen *s) : StartTransitionMsg(s) {}
    NETMSG_BYTECODE(NetPopScreenMsg);
    NETMSG_NAME(NetPopScreenMsg);
    NETMSG_NEWNETMSG(NetPopScreenMsg);
};
// Dtor note: NO class in this family declares a user dtor. StartTransitionMsg's
// ??1 survives as a real 72-byte COMDAT (claimed by BandUI.obj at 0x82522AE0)
// because mScreenName (String) needs destruction — but it is the IMPLICIT dtor
// (no own-vtable stores; a user-declared empty dtor would emit them). The
// derived Net*ScreenMsg implicit dtors have nothing to add, which is what lets
// Goto+Sync share one ICF'd ??_G (0x825841E0, differing only in vbase offset).
