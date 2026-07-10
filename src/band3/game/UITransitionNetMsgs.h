#pragma once
#include "meta_band/LockMessages.h"
#include "ui/UI.h"
#include "ui/UIComponent.h"
#include "ui/UIScreen.h"

class StartTransitionMsg : public StartLockMsg, public LockData {
public:
    StartTransitionMsg() {}
    StartTransitionMsg(UIScreen *);
    virtual ~StartTransitionMsg() {}
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
// Retail: StartTransitionMsg keeps a user-declared inline dtor (its ??1 is a real
// 72-byte body with vtable stores, COMDAT-claimed by BandUI.obj at 0x82522AE0).
// The derived Net*ScreenMsg classes have NO user dtors: their implicit dtors
// inline into ??_D/??_G with the initial vtable-store pair ELIDED, which is what
// lets Goto+Sync share one ICF'd ??_G (0x825841E0, differing only in vbase offset).
