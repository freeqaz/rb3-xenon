#include "beatmatch/KeyboardController.h"
#include "beatmatch/BeatMatchControllerSink.h"
#include "beatmatch/HitSink.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "os/Joypad.h"
#include "os/System.h"
#include "os/User.h"

KeyboardController::KeyboardController(
    User *user, const DataArray *cfg, BeatMatchControllerSink *bsink, bool disabled
)
    : BeatMatchController(user, cfg, false), mDisabled(disabled), mFretButtons(),
      mWhammy(0.0f), mSink(bsink) {
    JoypadSubscribe(this);
}

KeyboardController::~KeyboardController() { JoypadUnsubscribe(this); }

void KeyboardController::Poll() {}

void KeyboardController::Disable(bool b) { mDisabled = b; }

float KeyboardController::GetWhammyBar() const { return -GetCapStrip(); }

float KeyboardController::GetCapStrip() const {
    float f;
    if (!mUser->IsLocal()) {
        f = 0.0f;
    } else {
        mUser->GetLocalUser();
        f = mWhammy;
    }
    return f;
}

int KeyboardController::OnMsg(const KeyboardKeyPressedMsg &msg) {
    if (mDisabled)
        return 0;
    if (!IsOurPadNum(msg.GetPadNum()))
        return 0;
    MILO_ASSERT(mSink, 0x52);
    RegisterKey(msg.GetMidiNote());
    int slot = MidiNoteToSlot(msg.GetMidiNote());
    mSink->NoteOn(msg.GetMidiNote());
    if (slot != -1) {
        mSink->FretButtonDown(slot, msg.GetNode3());
        mSink->Swing(slot, false, true, false, false, (GemHitFlags)0);
        mFretButtons |= 1 << slot;
    }
    return 0;
}

int KeyboardController::OnMsg(const KeyboardKeyReleasedMsg &msg) {
    if (mDisabled)
        return 0;
    if (!IsOurPadNum(msg.GetPadNum()))
        return 0;
    int slot = MidiNoteToSlot(msg.GetMidiNote());
    mSink->NoteOff(msg.GetMidiNote());
    if (slot != -1) {
        mSink->FretButtonUp(slot);
        mFretButtons &= ~(1 << slot);
    }
    return 0;
}

int KeyboardController::OnMsg(const KeyboardSustainMsg &msg) {
    if (IsDisabled())
        return 0;
    if (!IsOurPadNum(msg.GetPadNum()))
        return 0;
    mSink->ForceMercurySwitch(true);
    mSink->ForceMercurySwitch(false);
    return 0;
}

int KeyboardController::OnMsg(const KeyboardStompBoxMsg &msg) {
    if (IsDisabled())
        return 0;
    if (!IsOurPadNum(msg.GetPadNum()))
        return 0;
    mSink->ForceMercurySwitch(true);
    mSink->ForceMercurySwitch(false);
    return 0;
}

int KeyboardController::OnMsg(const KeyboardModMsg &msg) {
    if (IsDisabled())
        return 0;
    if (!IsOurPadNum(msg.GetPadNum()))
        return 0;
    mWhammy = msg.GetNode2() / 127.0f;
    return 0;
}

int KeyboardController::OnMsg(const ButtonDownMsg &msg) {
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

int KeyboardController::MidiNoteToSlot(int note) const {
    int slot = note - 0x30;
    if (slot < 0 || slot > 0x18)
        return -1;
    else
        return slot;
}

// Mirrors GuitarController/JoypadController: retail truncates the OnMsg return to a
// byte before DataNode construction (the retail OnMsg overloads returned bool); the
// Wii dev decomp declares them int. Reproduce the truncation locally rather than
// changing the shared header.
#undef HANDLE_MESSAGE
#define HANDLE_MESSAGE(msg)                                                              \
    if (sym == msg::Type())                                                              \
    _HANDLE_CHECKED((unsigned char)OnMsg(msg(_msg)))

BEGIN_HANDLERS(KeyboardController)
    HANDLE_MESSAGE(KeyboardKeyPressedMsg)
    HANDLE_MESSAGE(KeyboardKeyReleasedMsg)
    HANDLE_MESSAGE(KeyboardSustainMsg)
    HANDLE_MESSAGE(KeyboardStompBoxMsg)
    HANDLE_MESSAGE(KeyboardModMsg)
    HANDLE_MESSAGE(ButtonDownMsg)
    HANDLE_CHECK(0xC6)
END_HANDLERS
