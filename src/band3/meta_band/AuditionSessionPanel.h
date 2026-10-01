#pragma once
#include "obj/ObjMacros.h"
#include "meta/Profile.h"
#include "os/PlatformMgr.h"
#include "ui/UIPanel.h"

/** The Rock Band Network audition-session screen (TU5; no surviving
    source). Retail registers it from MetaPanel::Init (0x82574E20, slot 5:
    StaticClassName 0x8256E828, NewObject 0x8256E8A8); NewObject allocates 0x6c
    bytes and runs the ctor at 0x826033F8. The bodies (retail 0x826033F8-
    0x82604798) are written from the retail XEX. RTTI: own vtable 0x820BEA3C
    (overrides Enter, Exit and Poll), Hmx::Object vbase vtable 0x820BE9E4
    (ClassName, SetType, Handle and the deleting dtor). It drives the slots of
    TheAuditionMgr's AuditionSessionBuilder (meta_band/AuditionMgr.h). Helper
    method names are ours. */
class AuditionSessionPanel : public UIPanel {
public:
    AuditionSessionPanel();
    OBJ_CLASSNAME(AuditionSessionPanel);
    OBJ_SET_TYPE(AuditionSessionPanel);
    NEW_OBJ(AuditionSessionPanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual void Enter();
    virtual void Exit();
    virtual void Poll();

    /** retail 0x82603CD8: sends `update` to this panel. */
    void Refresh();
    /** retail 0x826035A0. */
    Symbol GetSlotDiffSym(int slot);
    /** retail 0x82603760. */
    Symbol GetSlotTrackSym(int slot);
    /** retail 0x82603C68. */
    const char *GetInstIcon(int slot);
    /** retail 0x82603D90. Retail's Handle calls this one body for BOTH
        SigninChangedMsg and ProfileSwappedMsg: the two handlers are
        byte-identical and ICF-folded. */
    DataNode OnMsg(const SigninChangedMsg &);
    DataNode OnMsg(const ProfileSwappedMsg &);

private:
    int unk; // the 4 bytes past UIPanel (vbase at +0x44); the ctor does not set it
};
