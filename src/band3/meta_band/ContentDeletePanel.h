#pragma once
#include "os/ContentMgr.h"
#include "ui/UIPanel.h"
#include "ui/UIScreen.h"
#include "utl/Symbol.h"

class ContentDeletePanel : public UIPanel, public ContentMgr::Callback {
public:
    ContentDeletePanel();
    OBJ_CLASSNAME(ContentDeletePanel);
    OBJ_SET_TYPE(ContentDeletePanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual void Enter();
    virtual void Exit();
    virtual bool Exiting() const;
    virtual void Poll();
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual const char *ContentDir() { return nullptr; }
    virtual void ContentFailed(const char *);

    void SetupDeletion(Symbol, bool);

    DataNode OnMsg(const UITransitionCompleteMsg &);
    NEW_OBJ(ContentDeletePanel);
    static void Init() { REGISTER_OBJ_FACTORY(ContentDeletePanel); }

    // Retail-360 layout (proven from 0x82612EC8-0x826139B4 asm): the Wii dev
    // build's `int mContent` + `String mContentNames[2]` are NOT present here.
    // The ctor constructs exactly one Symbol at +0x44 from gNullStr, Poll/OnMsg
    // pass that Symbol straight to ContentMgr::DeleteContent/IsDeleteDone, and
    // SyncProperty calls PropSync(Symbol&,...) — see ?SyncProperty@...@Z.
    bool unk40; // 0x40
    Symbol mContent; // 0x44
    bool unk48; // 0x48
    bool unk49; // 0x49
    bool mDeleteFailed; // 0x4a
};
