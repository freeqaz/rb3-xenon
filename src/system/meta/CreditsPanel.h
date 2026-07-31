#pragma once

#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/Object.h"
#include "os/JoypadMsgs.h"
#include "synth/Stream.h"
#include "ui/UIListLabel.h"
#include "ui/UIListMesh.h"
#include "ui/UIListProvider.h"
#include "ui/UIPanel.h"
#include "utl/Symbol.h"

class CreditsPanel : public UIListProvider, public UIPanel {
private:
    CreditsPanel();
    // Hmx::Object
    virtual ~CreditsPanel();

public:
    OBJ_CLASSNAME(CreditsPanel)
    OBJ_SET_TYPE_ENGINE(CreditsPanel)
    virtual DataNode Handle(DataArray *, bool);

    // UIListProvider
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual RndMat *Mat(int, int, UIListMesh *) const;
    virtual int NumData() const;

    NEW_OBJ(CreditsPanel)

protected:
    DataNode OnMsg(const ButtonDownMsg &);

private:
    // UIPanel
    virtual void Load();
    virtual void Enter();
    virtual void Exit();
    virtual bool Exiting() const;
    virtual void Poll();
    virtual bool IsLoaded() const;
    virtual void Unload();
    virtual void FinishLoad();

    void PausePanel(bool);
    void DebugToggleAutoScroll();
    void SetAutoScroll(bool);

// ODR HAZARD, FIXED (lane CB-10/D): this member used to be a bare
// `#ifdef MILO_DEBUG`, which src/macros.h force-defines tree-wide.  CreditsPanel.cpp
// worked around that with a TU-local `#undef MILO_DEBUG`, so the class had TWO
// layouts in one binary -- measured with cl.exe /d1reportSingleClassLayoutCreditsPanel:
//     via src/system/meta/CreditsPanel.cpp   size(136), no mCheatOn, vbase Object @96
//     via src/band3/meta_band/MetaPanel.cpp  size(140), mCheatOn @64,  vbase Object @100
// i.e. every own member sat +4 and, worse, the virtual-base subobject moved, so a
// CreditsPanel*->Hmx::Object* upcast disagreed by 4 bytes between the two TUs.
// Retail RB3-360 was MILO_DEBUG-off, so 136 is the correct layout; gating on
// HX_NATIVE makes it uniform tree-wide and lets CreditsPanel.cpp drop the #undef.
// (The old `// 0x3c` comment was itself wrong -- the compiler puts it at 0x40.)
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
    bool mCheatOn; // 0x40 (dev-build only; retail RB3-360 release lacks it)
#endif
    DataLoader *mLoader; // 0x40
    DataArray *mNames; // 0x44
    UIList *mList; // 0x48
    Stream *mStream; // 0x4c
    bool mAutoScroll; // 0x50
    float mSavedSpeed; // 0x54
    /** Whether or not the panel is paused. */
    bool mPaused; // 0x58
};
