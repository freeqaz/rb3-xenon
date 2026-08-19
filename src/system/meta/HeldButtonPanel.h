#pragma once
#include "meta/ButtonHolder.h"
#include "obj/Object.h"
#include "ui/UIPanel.h"

class HeldButtonPanel : public UIPanel {
public:
    HeldButtonPanel();
    // Hmx::Object
    virtual ~HeldButtonPanel();
    OBJ_CLASSNAME(HeldButtonPanel);
    OBJ_SET_TYPE_ENGINE(HeldButtonPanel);
    virtual DataNode Handle(DataArray *, bool);
    // UIPanel
    virtual void Enter();
    virtual void Exit();
    virtual void Poll();

    NEW_OBJ(HeldButtonPanel)

private:
    DataNode OnMsg(const ProcessedButtonDownMsg &);

    ButtonHolder *mHolder; // 0x3c
    bool mHandling; // 0x40
};
