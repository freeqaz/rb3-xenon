#include "ui/Utl.h"
#include "os/Joypad.h"
#include "ui/UI.h"

int PageDirection(JoypadAction act) {
    if (act == kAction_PageDown)
        return 1;
    if (act == kAction_PageUp)
        return -1;
    return 0;
}

bool IsNavAction(JoypadAction act) {
    return act == kAction_Up || act == kAction_Down || act == kAction_Left
        || act == kAction_Right;
}

int ScrollDirection(const ButtonDownMsg &msg, bool b1, bool b2, int i) {
    int action;
    bool overload;

    action = msg.mData->Int(4);

    if (!b2) {
        int button = msg.mData->Int(3);
        overload = TheUI->OverloadHorizontalNav((JoypadAction)action, (JoypadButton)button, b1);
        if (overload) {
            if (action == kAction_Up) {
                action = kAction_Left;
            } else if (action == kAction_Down) {
                action = kAction_Right;
            }
        }
    }

    int negAction = b2 ? kAction_Up : kAction_Left;
    int posAction = b2 ? kAction_Down : kAction_Right;
    int secNeg = b2 ? kAction_Left : kAction_Up;
    int secPos = b2 ? kAction_Right : kAction_Down;

    if (action == negAction) {
        return -i;
    } else if (action == posAction) {
        return i;
    } else if (action == secNeg && i > 1) {
        return -1;
    } else if (action == secPos && i > 1) {
        return 1;
    }
    return 0;
}
