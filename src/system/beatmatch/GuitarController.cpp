#include "beatmatch/GuitarController.h"
#include "beatmatch/BeatMatchControllerSink.h"
#include "beatmatch/HitSink.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "os/Joypad.h"
#include "os/System.h"
#include "os/User.h"
#include <algorithm>

// Retail RB3 compiled this TU's MILO_WARN with arguments EVALUATED for side
// effects (the global sizeof()-form strips arg evaluation, which also kills the
// HANDLE_CHECK `if (_warn)` guard in Handle). The retail target keeps the
// _warn-conditional PathName(this) evaluation; mirror MILO_FAIL's (void)(args)
// comma form per-TU so the side-effecting args (PathName vcall) survive while
// the message string vanishes.
#ifndef HX_NATIVE
#undef MILO_WARN
#define MILO_WARN(...) ((void)(__VA_ARGS__))
#endif

GuitarController::GuitarController(
    User *user,
    const DataArray *cfg,
    BeatMatchControllerSink *bsink,
    bool disabled,
    bool lefty
)
    : BeatMatchController(user, cfg, lefty), mDisabled(disabled), mAutoSoloButtons(0),
      mFretMask(0), mShiftButtonMask(0), mSink(bsink), mControllerStyle(kPS2),
      mStrumBarButtons(), mMercuryButton(kPad_L2) {
    JoypadSubscribe(this);
    Symbol cntType;
    if (cfg->FindData("controller_style", cntType, false)) {
        if (cntType == "ps2")
            mControllerStyle = kPS2;
        else if (cntType == "hx_ps3")
            mControllerStyle = kPS3;
        else if (cntType == "ro_ps3")
            mControllerStyle = kRoPS3;
        else if (cntType == "ro_xbox")
            mControllerStyle = kRoXbox;
        else if (cntType == "hx_xbox")
            mControllerStyle = kHxXbox;
        else if (cntType == "hx_wii")
            mControllerStyle = kHxWii;
    }
    if (mControllerStyle == kHxXbox)
        mMercuryButton = kPad_Xbox_RB;
    if (mControllerStyle == kPS3)
        mMercuryButton = kPad_R1;
    if (mControllerStyle == kHxWii)
        mMercuryButton = kPad_R1;
    if (!disabled)
        ReconcileFretState();
    DataArray *strum_buttons = cfg->FindArray("strum_buttons", false);
    if (strum_buttons) {
        mStrumBarButtons.reserve(strum_buttons->Size() - 1);
        for (int i = 1; i < strum_buttons->Size(); i++) {
            mStrumBarButtons.push_back((JoypadButton)strum_buttons->Int(i));
        }
    } else {
        mStrumBarButtons.reserve(2);
        if (lefty) {
            mStrumBarButtons.push_back(kPad_DDown);
            mStrumBarButtons.push_back(kPad_DUp);
        } else {
            mStrumBarButtons.push_back(kPad_DUp);
            mStrumBarButtons.push_back(kPad_DDown);
        }
    }

    DataArray *shift_button_cfg = cfg->FindArray("shift_button", false);
    if (shift_button_cfg) {
        mShiftButtonMask = 1 << shift_button_cfg->Int(1);
    }
}

GuitarController::~GuitarController() { JoypadUnsubscribe(this); }

void GuitarController::Disable(bool b) {
    mDisabled = b;
    if (b) {
        for (int i = 0; i < 5; i++) {
            if (mFretMask & 1 << i) {
                mSink->FretButtonUp(i);
            }
        }
        mFretMask = 0;
    } else
        ReconcileFretState();
}

float guitarwhammyprobs = 0.0f;

float GuitarController::GetWhammyBar() const {
    if (!mUser->IsLocal())
        return 0;
    else {
        LocalUser *lUser = mUser->GetLocalUser();
        if (!UserHasController(lUser))
            return 0.0f;
        JoypadData *thePadData = JoypadGetPadData(lUser->GetPadNum());
        DataArray *controllerArr = SystemConfig("joypad")->FindArray(
            Symbol("controllers"), JoypadControllerTypePadNum(lUser->GetPadNum())
        );
        bool b38;
        float f18;
        if (controllerArr->FindData("ly_whammy", b38, false)) {
            f18 = thePadData->GetLY();
        } else if (controllerArr->FindData("negative_rx_whammy_val", b38, false)) {
            f18 = -thePadData->GetRX();
        } else if (controllerArr->FindData("traditional_whammy_val", b38, false)) {
            f18 = -(thePadData->GetRX() + 1.0f) / 2.0f;
        }
        return std::min(0.0f, f18);
    }
}

float GuitarController::GetCapStrip() const { return 0.0f; }

void GuitarController::Poll() {
    bool idk;
    if (!mUser->IsLocal())
        return;
    else {
        LocalUser *lUser = mUser->GetLocalUser();
        if (!UserHasController(lUser))
            return;
        if (mDisabled)
            return;
        JoypadData *thePadData = JoypadGetPadData(lUser->GetPadNum());
        DataArray *found = SystemConfig("joypad")->FindArray(
            Symbol("controllers"), JoypadControllerTypePadNum(lUser->GetPadNum())
        );
        if (found->FindData("xbox_mercury_switch", idk, false)) {
            mSink->MercurySwitch(-thePadData->GetRY());
        } else if (found->FindData("ps3_mercury_switch", idk, false)) {
            mSink->MercurySwitch(thePadData->GetSX() * -4.65f);
        }
    }
}

int GuitarController::OnMsg(const ButtonDownMsg &msg) {
    bool &_ref0 = mDisabled;
    if (_ref0)
        return 0;
    if (!mUser->IsLocal())
        return 0;
    LocalUser *lUser = mUser->GetLocalUser();
    if (msg.GetUser() != lUser)
        return 0;
    MILO_ASSERT(mSink, 0xDB);
    int btn = msg.GetButton();
    const std::vector<int> &strum = mStrumBarButtons;
    std::vector<int>::const_iterator btnIter =
        std::find(strum.begin(), strum.end(), btn);
    if (btnIter != strum.end()) {
        int slot = GetCurrentSlot();
        unsigned char b8 = (unsigned char)(btnIter != strum.begin());
        bool b1 = b8;
        if (mLefty)
            b1 = !b8;
        mSink->Swing(
            slot, true, b8, false, true, IsShifted() ? kGemHitFlagSolo : kGemHitFlagNone
        );
        RegisterHit(b1 ? kHitDownstrum : kHitUpstrum);
    } else {
        if (btn == mForceMercuryBut) {
            mSink->ForceMercurySwitch(true);
            RegisterHit(kHitSelect);
        } else if (mControllerStyle != kRoXbox && btn == mMercuryButton) {
            mSink->MercurySwitch(1);
        } else {
            int slot = ButtonToSlot((JoypadButton)btn);
            if (slot != -1) {
                mFretMask |= 1 << slot;
                mSink->FretButtonDown(slot, -1);
                if (_ref0)
                    return 0;
                else {
                    lUser->GetPadNum();
                    bool shifted = IsShifted();
                    switch (slot) {
                    case 0:
                        RegisterHit(shifted ? kHitHighGreenFret : kHitGreenFret);
                        break;
                    case 1:
                        RegisterHit(shifted ? kHitHighRedFret : kHitRedFret);
                        break;
                    case 2:
                        RegisterHit(shifted ? kHitHighYellowFret : kHitYellowFret);
                        break;
                    case 3:
                        RegisterHit(shifted ? kHitHighBlueFret : kHitBlueFret);
                        break;
                    case 4:
                        RegisterHit(shifted ? kHitHighOrangeFret : kHitOrangeFret);
                        break;
                    }
                    if (shifted
                        && mSink->Swing(slot, false, true, true, false, kGemHitFlagSolo))
                        return 0;
                    mSink->NonStrumSwing(slot, true, shifted);
                }
            }
        }
    }
    return 0;
}

int GuitarController::OnMsg(const ButtonUpMsg &msg) {
    if (mDisabled)
        return 0;
    if (!mUser->IsLocal())
        return 0;
    LocalUser *lUser = mUser->GetLocalUser();
    if (msg.GetUser() != lUser)
        return 0;
    int btn = msg.GetButton();
    std::vector<int>::iterator btnIter =
        std::find(mStrumBarButtons.begin(), mStrumBarButtons.end(), btn);
    if (btnIter != mStrumBarButtons.end()) {
        mSink->ReleaseSwing();
    } else if (mControllerStyle != kRoXbox && btn == mMercuryButton) {
        mSink->MercurySwitch(0);
    } else if (btn == mForceMercuryBut) {
        mSink->ForceMercurySwitch(false);
    } else {
        int slot = ButtonToSlot((JoypadButton)btn);
        if (slot != -1) {
            mFretMask &= ~(1 << slot);
            mSink->FretButtonUp(slot);
            if (mFretMask) {
                int curSlot = GetCurrentSlot();
                mSink->NonStrumSwing(curSlot, false, IsShifted());
            }
        }
    }
    return 0;
}

void GuitarController::ReconcileFretState() {
    if (mUser->IsLocal()) {
        LocalUser *lUser = mUser->GetLocalUser();
        if (UserHasController(lUser)) {
            auto _tmp0 = lUser->GetPadNum();
            JoypadData *padData = JoypadGetPadData(_tmp0);
            int mask = 0;
            for (int i = 0; i < 5; i++) {
                int fretmask = mFretMask;
                bool wasInMask = (fretmask & 1 << i);
                bool inMask = padData->IsButtonInMask(SlotToButton(i));
                if (inMask) {
                    mask |= 1 << i;
                }
                if (wasInMask != inMask) {
                    if (inMask) {
                        mSink->FretButtonDown(i, -1);
                    } else {
                        mSink->FretButtonUp(i);
                    }
                }
            }
            mFretMask = mask;
            mSink->ForceMercurySwitch(padData->IsButtonInMask(mForceMercuryBut));
            if (mControllerStyle != kRoXbox) {
                mSink->MercurySwitch(padData->IsButtonInMask(mMercuryButton) ? 1.0f : 0.0f);
            }
        }
    }
}

int GuitarController::GetCurrentSlot() const {
    int ret = -1;
    for (int i = 0; i < 5; i++) {
        if (mFretMask & (1 << i))
            ret = i;
    }
    return ret;
}

bool GuitarController::IsShifted() const {
    if (!mUser->IsLocal())
        return false;
    else if (mAutoSoloButtons)
        return true;
    else {
        JoypadData *thePadData = JoypadGetPadData(mUser->GetLocalUser()->GetPadNum());
        unsigned int btns = thePadData->mButtons;
        return btns & mShiftButtonMask;
    }
}

// Retail RB3 did NOT compile the BEGIN_HANDLERS MessageTimer instrumentation
// (our macros.h force-defines MILO_DEBUG tree-wide, which the #ifdef-MILO_DEBUG
// arm of BEGIN_HANDLERS expands into a per-Handle MessageTimer + Timer::Restart;
// the retail target's GuitarController::Handle has zero Timer references). Use
// the MILO_DEBUG-off form of BEGIN_HANDLERS for this TU only.
#undef BEGIN_HANDLERS
#define BEGIN_HANDLERS(objType)                                                          \
    DataNode objType::Handle(DataArray *_msg, bool _warn) {                              \
        Symbol sym = _msg->Sym(1);

BEGIN_HANDLERS(GuitarController)
    HANDLE_MESSAGE(ButtonDownMsg)
    HANDLE_MESSAGE(ButtonUpMsg)
    HANDLE_CHECK(0x1BA)
END_HANDLERS
