#include "beatmatch/JoypadMidiController.h"
#include "beatmatch/BeatMatchControllerSink.h"
#include "beatmatch/HitSink.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "os/Joypad.h"
#include "os/System.h"
#include "os/User.h"
#include <algorithm>

JoypadMidiController::JoypadMidiController(
    User *user, const DataArray *cfg, BeatMatchControllerSink *bsink, bool disabled
)
    : JoypadController(user, cfg, bsink, disabled, false), mWhammy(0.0f) {
    JoypadSubscribe(this);
}

JoypadMidiController::~JoypadMidiController() { JoypadUnsubscribe(this); }

float JoypadMidiController::GetWhammyBar() const { return -mWhammy; }

float JoypadMidiController::GetCapStrip() const {
    float f;
    if (!mUser->IsLocal()) {
        f = 0.0f;
    } else {
        mUser->GetLocalUser();
        f = mWhammy;
    }
    return f;
}

int JoypadMidiController::OnMsg(const KeyboardKeyPressedMsg &msg) {
    if (IsDisabled())
        return 0;
    if (!IsOurPadNum(msg.GetPadNum()))
        return 0;
    JoypadButton btn = MidiNoteToButton(msg.GetMidiNote());
    if (btn == kPad_Circle) {
        mSink->OutOfRangeSwing();
    } else {
        ButtonDownMsg m(mLocalUser, btn, kAction_None, mLocalUser->GetPadNum());
        JoypadController::OnMsg(m);
    }
    return 0;
}

int JoypadMidiController::OnMsg(const KeyboardKeyReleasedMsg &msg) {
    if (IsDisabled())
        return 0;
    if (!IsOurPadNum(msg.GetPadNum()))
        return 0;
    JoypadButton btn = MidiNoteToButton(msg.GetMidiNote());
    ButtonUpMsg m(mLocalUser, btn, kAction_None, mLocalUser->GetPadNum());
    JoypadController::OnMsg(m);
    return 0;
}

int JoypadMidiController::OnMsg(const KeyboardSustainMsg &msg) {
    if (IsDisabled())
        return 0;
    if (!IsOurPadNum(msg.GetPadNum()))
        return 0;
    mSink->ForceMercurySwitch(true);
    mSink->ForceMercurySwitch(false);
    return 0;
}

int JoypadMidiController::OnMsg(const KeyboardStompBoxMsg &msg) {
    if (IsDisabled())
        return 0;
    if (!IsOurPadNum(msg.GetPadNum()))
        return 0;
    mSink->ForceMercurySwitch(true);
    mSink->ForceMercurySwitch(false);
    return 0;
}

int JoypadMidiController::OnMsg(const KeyboardModMsg &msg) {
    if (IsDisabled())
        return 0;
    if (!IsOurPadNum(msg.GetPadNum()))
        return 0;
    mWhammy = msg.GetNode2() / 127.0f;
    return 0;
}

JoypadButton JoypadMidiController::MidiNoteToButton(int note) const {
    static DataNode &keyboard_wide_frets = DataVariable("keyboard_wide_frets");
    if (keyboard_wide_frets.Int()) {
        int octave = note / 12;
        int pad = mLocalUser->GetPadNum();
        if (pad >= 0 && pad <= 1 && (octave % 2) != pad) {
            return kPad_Circle;
        }
    }
    switch (note % 12) {
    case 0:
        return kPad_L2;
    case 2:
        return kPad_R2;
    case 4:
        return kPad_L1;
    case 5:
        return kPad_R1;
    case 7:
        return kPad_Tri;
    default:
        return kPad_Circle;
    }
}

int JoypadMidiController::OnMsg(const ButtonDownMsg &msg) {
    if (IsDisabled())
        return 0;
    if (!IsOurPadNum(msg.GetPadNum()))
        return 0;
    JoypadButton btn = msg.GetButton();
    if (btn == mForceMercuryBut) {
        mSink->ForceMercurySwitch(true);
        mSink->ForceMercurySwitch(false);
    }
    return 0;
}

// Mirrors GuitarController/JoypadController: retail truncates the OnMsg return to a
// byte before DataNode construction (the retail OnMsg overloads returned bool); the
// Wii dev decomp declares them int. Reproduce the truncation locally rather than
// changing the shared header.
#undef HANDLE_MESSAGE
#define HANDLE_MESSAGE(msg)                                                              \
    if (sym == msg::Type())                                                              \
    _HANDLE_CHECKED((unsigned char)OnMsg(msg(_msg)))

BEGIN_HANDLERS(JoypadMidiController)
    HANDLE_MESSAGE(KeyboardKeyPressedMsg)
    HANDLE_MESSAGE(KeyboardKeyReleasedMsg)
    HANDLE_MESSAGE(KeyboardModMsg)
    HANDLE_MESSAGE(KeyboardSustainMsg)
    HANDLE_MESSAGE(KeyboardStompBoxMsg)
    HANDLE_MESSAGE(ButtonDownMsg)
    HANDLE_CHECK(0xDB)
END_HANDLERS
