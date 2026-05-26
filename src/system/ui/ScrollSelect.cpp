#include "ui/ScrollSelect.h"
#include "UIComponent.h"
#include "obj/Object.h"
#include "os/Joypad.h"
#include "ui/UI.h"
#include "ui/Utl.h"
#include "utl/Symbol.h"

void ScrollSelect::Store() { mSelectedAux = SelectedAux(); }
void ScrollSelect::Reset() { mSelectedAux = -1; }
bool ScrollSelect::CanScroll() const { return !mSelectToScroll || mSelectedAux != -1; }

ScrollSelect::ScrollSelect() : mSelectToScroll(0) { Reset(); }

BEGIN_CUSTOM_HANDLERS(ScrollSelect)
    HANDLE_EXPR(is_scroll_selected, IsScrollSelected())
    HANDLE_ACTION(reset, Reset())
END_CUSTOM_HANDLERS

BEGIN_PROPSYNCS(ScrollSelect)
    SYNC_PROP(select_to_scroll, mSelectToScroll)
END_PROPSYNCS

DataNode ScrollSelect::SendScrollSelected(UIComponent *comp, LocalUser *user) {
    static UIComponentScrollSelectMsg scroll_select_msg(0, 0, 0);
    scroll_select_msg[0] = comp;
    scroll_select_msg[1] = user;
    scroll_select_msg[2] = mSelectedAux != -1;
    return TheUI->Handle(scroll_select_msg, false);
}

UIComponent::State ScrollSelect::DrawState(UIComponent *comp) const {
    static Symbol selected("selected");
    if (!mSelectToScroll || mSelectedAux == -1) {
        return comp->GetState();
    } else {
        return UIComponent::kSelected;
    }
}

bool ScrollSelect::CatchNavAction(JoypadAction act) const {
    return mSelectedAux != -1 && IsNavAction(act);
}

bool ScrollSelect::SelectScrollSelect(UIComponent *comp, LocalUser *user) {
    if (mSelectToScroll) {
        if (mSelectedAux == -1)
            Store();
        else
            Reset();
        SendScrollSelected(comp, user);
        return true;
    } else
        return false;
}

bool ScrollSelect::RevertScrollSelect(
    UIComponent *comp, LocalUser *user, Hmx::Object *obj
) {
    if (mSelectedAux != -1) {
        bool auxChanged = mSelectedAux != SelectedAux();
        SetSelectedAux(mSelectedAux);
        mSelectedAux = -1;
        DataNode node(kDataUnhandled, 0);
        if (auxChanged && obj) {
            node = obj->Handle(UIComponentScrollMsg(comp, user), false);
        }
        if (node.Type() == kDataUnhandled) {
            node = SendScrollSelected(comp, user);
        }
        if (auxChanged) {
            if (node.Type() == kDataUnhandled) {
                TheUI->Handle(UIComponentScrollMsg(comp, user), false);
            }
        }
        return true;
    } else
        return false;
}
