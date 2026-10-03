#pragma once
#include "ui/UIPanel.h"
#include "ui/UIScreen.h"
#include "utl/MemMgr.h"

class BandScreen : public UIScreen {
public:
    BandScreen() {}
    // No user-declared destructor: retail ~BandScreen (implicit) does not
    // re-store the vptr before ~UIScreen, which a user-declared one does.
    OBJ_CLASSNAME(BandScreen);
    OBJ_SET_TYPE(BandScreen);
    virtual DataNode Handle(DataArray *, bool);
    virtual void LoadPanels();
    virtual bool CheckIsLoaded();
    virtual bool IsLoaded() const;
    virtual void Enter(UIScreen *);
    virtual bool Entering() const;
    virtual void Exit(UIScreen *);
    virtual bool Exiting() const;

    void LoadInterstitials();
    void UnloadInterstitials();

    NEW_OVERLOAD;
    DELETE_OVERLOAD;
    NEW_OBJ(BandScreen);
    static void Init() { REGISTER_OBJ_FACTORY(BandScreen); }

    std::vector<UIPanel *> mExtraPanels; // 0x40
};

#include "obj/Msg.h"

DECLARE_MESSAGE(CurrentScreenChangedMsg, "current_screen_changed");
CurrentScreenChangedMsg(Symbol);
Symbol GetScreen() const;
END_MESSAGE