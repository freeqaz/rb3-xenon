#pragma once
#include "net/SessionMessages.h"
#include "obj/ObjMacros.h"
#include "ui/UIPanel.h"

class LocalBandUser;

/** The "join from invite" screen. Every body here is written from the retail
    XEX (TU5,
    0x826308B8-0x82631A20). RTTI: own vtable 0x820CA374 (overrides Enter and
    Exit), Hmx::Object vbase vtable 0x820CA31C (ClassName, SetType, Handle and
    the deleting dtor). sizeof 0x6c: one member at 0x3c, the vtordisp at 0x40,
    the Object vbase at 0x44. Helper method names are ours. */
class JoinInvitePanel : public UIPanel {
public:
    JoinInvitePanel();
    // User-declared and inline: retail's ??_D (0x82630A98) re-stores both
    // vtables itself before ~UIPanel, i.e. this empty body is inlined there.
    virtual ~JoinInvitePanel() {}
    OBJ_CLASSNAME(JoinInvitePanel);
    OBJ_SET_TYPE(JoinInvitePanel);
    NEW_OBJ(JoinInvitePanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual void Enter();
    virtual void Exit();

    /** retail 0x82630CD8: the finding_presence_* token for a join state
        (1-8), or, for state 0, for the error in `msg`. */
    Symbol PresenceToken(int state, const JoinResultMsg *msg);
    /** retail 0x82630FC8: sends set_presence with that token to this panel. */
    void SetPresence(int state, const JoinResultMsg *msg);
    /** retail 0x82631288: validates the session's local users and joins the
        invite's session, or reports why it cannot. */
    void TryJoin();
    /** retail 0x82631160. */
    DataNode OnMsg(const JoinResultMsg &);

    LocalBandUser *mJoiningUser; // 0x3c (set_joining_user; the ctor zeroes it)
};
