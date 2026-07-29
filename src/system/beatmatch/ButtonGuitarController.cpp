#include "beatmatch/ButtonGuitarController.h"
#include "beatmatch/BeatMatchControllerSink.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "os/Joypad.h"
#include "os/User.h"

ButtonGuitarController::ButtonGuitarController(
    User *user, const DataArray *cfg, BeatMatchControllerSink *bsink, bool b1, bool lefty
)
    : BeatMatchController(user, cfg, false), mDisabled(b1), mShifted(0), mSlotMask(0),
      mSink(bsink) {
    mLefty = lefty;
    JoypadSubscribe(this);
}

ButtonGuitarController::~ButtonGuitarController() {}

void ButtonGuitarController::Poll() {}

void ButtonGuitarController::Disable(bool b) { mDisabled = b; }

bool ButtonGuitarController::IsDisabled() const { return mDisabled; }

float ButtonGuitarController::GetWhammyBar() const { return 0.0f; }

int ButtonGuitarController::GetFretButtons() const { return 0; }

bool ButtonGuitarController::IsShifted() const { return mShifted; }

void ButtonGuitarController::SetAutoSoloButtons(bool) {}

int ButtonGuitarController::OnMsg(const RGSwingMsg &msg) {
    if (mDisabled)
        return 0;
    if (!mUser->IsLocal())
        return 0;
    LocalUser *lUser = mUser->GetLocalUser();
    int msgInt = msg.GetPadNum();
    int padnum = lUser->GetPadNum();
    if (msgInt != padnum)
        return 0;
    int slot = GetCurrentSlot();
    GemHitFlags flags = IsShifted() ? kGemHitFlagSolo : kGemHitFlagNone;
    mSink->Swing(slot, true, true, false, true, flags);
    return 0;
}

int ButtonGuitarController::OnMsg(const ButtonDownMsg &msg) {
    if (mDisabled)
        return 0;
    if (!mUser->IsLocal())
        return 0;
    LocalUser *lUser = mUser->GetLocalUser();
    int msgInt = msg.GetPadNum();
    int padnum = lUser->GetPadNum();
    if (msgInt != padnum)
        return 0;
    if (msg.GetButton() == kPad_Select)
        mSink->ForceMercurySwitch(true);
    return 0;
}

int ButtonGuitarController::OnMsg(const ButtonUpMsg &msg) {
    if (mDisabled)
        return 0;
    if (!mUser->IsLocal())
        return 0;
    LocalUser *lUser = mUser->GetLocalUser();
    int msgInt = msg.GetPadNum();
    int padnum = lUser->GetPadNum();
    if (msgInt != padnum)
        return 0;
    if (msg.GetButton() == kPad_Select)
        mSink->ForceMercurySwitch(false);
    return 0;
}

int ButtonGuitarController::OnMsg(const RGFretButtonDownMsg &msg) {
    if (mDisabled)
        return 0;
    if (!mUser->IsLocal())
        return 0;
    LocalUser *lUser = mUser->GetLocalUser();
    int msgInt = msg.GetPadNum();
    int padnum = lUser->GetPadNum();
    if (msgInt != padnum)
        return 0;
    MILO_ASSERT(mSink, 0x8A);
    int i1 = msg.GetNode2();
    mShifted = msg.GetShifted() != 0;
    mSlotMask |= (1 << i1);
    mSink->FretButtonDown(i1, -1);
    if (mShifted && mSink->Swing(i1, false, true, true, false, kGemHitFlagSolo) != 0)
        return 0;
    mSink->NonStrumSwing(i1, true, mShifted);
    return 0;
}

int ButtonGuitarController::OnMsg(const RGFretButtonUpMsg &msg) {
    if (mDisabled)
        return 0;
    if (!mUser->IsLocal())
        return 0;
    LocalUser *lUser = mUser->GetLocalUser();
    int msgInt = msg.GetPadNum();
    int padnum = lUser->GetPadNum();
    if (msgInt != padnum)
        return 0;
    MILO_ASSERT(mSink, 0xA4);
    int i1 = msg.GetNode2();
    mShifted = msg.GetShifted() != 0;
    mSlotMask &= ~(1 << i1);
    mSink->FretButtonUp(i1);
    if (mSlotMask != 0)
        mSink->NonStrumSwing(GetCurrentSlot(), false, mShifted);
    return 0;
}

int ButtonGuitarController::OnMsg(const RGAccelerometerMsg &msg) {
    if (mDisabled)
        return 0;
    if (!mUser->IsLocal())
        return 0;
    LocalUser *lUser = mUser->GetLocalUser();
    int msgInt = msg.GetPadNum();
    int padnum = lUser->GetPadNum();
    if (msgInt != padnum)
        return 0;
    MILO_ASSERT(mSink, 0xBB);
    mSink->MercurySwitch(msg.GetNode3() / 127.0f);
    return 0;
}

int ButtonGuitarController::OnMsg(const RGStompBoxMsg &msg) {
    if (IsDisabled())
        return 0;
    if (!IsOurPadNum(msg.GetPadNum()))
        return 0;
    mSink->ForceMercurySwitch(true);
    mSink->ForceMercurySwitch(false);
    return 0;
}

int ButtonGuitarController::GetCurrentSlot() const {
    int ret = -1;
    for (int i = 0; i < 5; i++) {
        if (mSlotMask & (1 << i))
            ret = i;
    }
    return ret;
}

// Mirrors GuitarController/JoypadController: retail truncates the OnMsg return to a
// byte before DataNode construction (the retail OnMsg overloads returned bool); the
// Wii dev decomp declares them int. Reproduce the truncation locally rather than
// changing the shared header.
#undef HANDLE_MESSAGE
#define HANDLE_MESSAGE(msg)                                                              \
    if (sym == msg::Type())                                                              \
    _HANDLE_CHECKED((unsigned char)OnMsg(msg(_msg)))

BEGIN_HANDLERS(ButtonGuitarController)
    HANDLE_MESSAGE(StringStrummedMsg)
    HANDLE_MESSAGE(StringStoppedMsg)
    HANDLE_MESSAGE(RGSwingMsg)
    HANDLE_MESSAGE(ButtonDownMsg)
    HANDLE_MESSAGE(ButtonUpMsg)
    HANDLE_MESSAGE(RGFretButtonDownMsg)
    HANDLE_MESSAGE(RGFretButtonUpMsg)
    HANDLE_MESSAGE(RGAccelerometerMsg)
    HANDLE_MESSAGE(RGStompBoxMsg)
    HANDLE_CHECK(0xDA)
END_HANDLERS
