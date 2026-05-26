#include "meta/HeldButtonPanel.h"
#include "meta/ButtonHolder.h"
#include "obj/Object.h"
#include "os/JoypadMsgs.h"
#include "ui/UI.h"
#include "ui/UIPanel.h"

HeldButtonPanel::HeldButtonPanel()
    : mHolder(new ButtonHolder(this, nullptr)), mHandling(false) {}

HeldButtonPanel::~HeldButtonPanel() { delete mHolder; }

BEGIN_HANDLERS(HeldButtonPanel)
    HANDLE_MESSAGE(ProcessedButtonDownMsg)
    if (!mHandling)
        HANDLE_MEMBER_PTR(mHolder)
    HANDLE_SUPERCLASS(UIPanel)
END_HANDLERS

void HeldButtonPanel::Enter() {
    std::vector<ActionRec> recs;
    static Symbol held_buttons("held_buttons");
    DataArray *heldButtonsArr = TypeDef()->FindArray(held_buttons, false);
    if (heldButtonsArr) {
        for (int i = 1; i < heldButtonsArr->Size(); i++) {
            DataArray *elem = heldButtonsArr->Array(i);
            MILO_ASSERT(elem, 0x27);
            float holdTime = elem->Float(1);
            if (holdTime > 0) {
                ActionRec rec((JoypadAction)elem->Int(0), holdTime, TheUserMgr);
                recs.push_back(rec);
            }
        }
    }
    mHolder->SetHoldActions(recs);
    UIPanel::Enter();
}

void HeldButtonPanel::Exit() {
    std::vector<ActionRec> recs;
    mHolder->SetHoldActions(recs);
    UIPanel::Exit();
}

void HeldButtonPanel::Poll() {
    if (TheUI->FocusPanel() == this)
        mHolder->Poll();
    else
        mHolder->ClearHeldButtons();
    UIPanel::Poll();
}

DataNode HeldButtonPanel::OnMsg(const ProcessedButtonDownMsg &msg) {
    if (msg.IsHeldDown()) {
        // Button held for longer than threshold; forward as held button message
        static Symbol on_button_held("on_button_held");
        static Message msgButtonHeld(on_button_held, 0, 0, 0, 0);
        msgButtonHeld[0] = msg.GetUser();
        msgButtonHeld[1] = msg.GetButton();
        msgButtonHeld[2] = msg.GetAction();
        msgButtonHeld[3] = msg.GetPadNum();
        Handle(msgButtonHeld, false);
    } else {
        // Button released or below hold threshold; forward as regular button down
        static ButtonDownMsg msgButtonDown(0, kPad_L2, kAction_None, 0);
        msgButtonDown[0] = msg.GetUser();
        msgButtonDown[1] = msg.GetButton();
        msgButtonDown[2] = msg.GetAction();
        msgButtonDown[3] = msg.GetPadNum();
        mHandling = true;
        Handle(msgButtonDown, false);
        mHandling = false;
    }
    return 1;
}
