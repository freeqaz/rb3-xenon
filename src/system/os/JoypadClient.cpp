#include "os/JoypadClient.h"
#include "JoypadClient.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/Joypad.h"
#include "os/JoypadMsgs.h"
#include "os/PlatformMgr.h"
#include "os/System.h"
#include "os/User.h"
#include "os/UserMgr.h"
#include <vector>

float gDefaultHoldMs = -1.0f;
float gDefaultRepeatMs = -1.0f;

namespace {
    bool gInited = false;
    std::vector<JoypadClient *> gClients;

    JoypadButton LeftStickToDpad(JoypadButton btn) {
        JoypadButton ret;
        switch (btn) {
        case kPad_LStickUp:
            return kPad_DUp;
        case kPad_LStickDown:
            return kPad_DDown;
        case kPad_LStickLeft:
            return kPad_DLeft;
        case kPad_LStickRight:
            return kPad_DRight;
        default:
            MILO_FAIL("illegal button");
            return kPad_NumButtons;
        }
    }
}

JoypadRepeat::JoypadRepeat()
    : mLastBtn(kPad_NumButtons), mLastAction(kAction_None), mLastPad(-1) {}

void JoypadRepeat::Start(JoypadButton btn, JoypadAction act, int pad) {
    mHoldTimer.Reset();
    mRepeatTimer.Reset();
    mHoldTimer.Start();
    mLastBtn = btn;
    mLastAction = act;
    mLastPad = pad;
}

void JoypadRepeat::SendRepeat(Hmx::Object *o, int i) {
    LocalUser *user = TheUserMgr ? TheUserMgr->GetLocalUserFromPadNum(i) : nullptr;
    o->Handle(ButtonDownMsg(user, mLastBtn, mLastAction, mLastPad), false);
}

void JoypadRepeat::Poll(float f1, float f2, Hmx::Object *o, int i4) {
    mHoldTimer.Pause();
    mRepeatTimer.Pause();
    float holdMs = mHoldTimer.Ms();
    float repeatMs = mRepeatTimer.Ms();
    mHoldTimer.Resume();
    mRepeatTimer.Resume();
    if (holdMs >= f1) {
        SendRepeat(o, i4);
        mHoldTimer.Reset();
        mRepeatTimer.Start();
    } else if (repeatMs >= f2) {
        SendRepeat(o, i4);
        mRepeatTimer.Reset();
        mRepeatTimer.Start();
    }
}

void JoypadRepeat::Reset(JoypadButton btn) {
    if (mLastBtn == btn) {
        mHoldTimer.Reset();
        mRepeatTimer.Reset();
    }
}

JoypadClient::JoypadClient(Hmx::Object *sink)
    : mUser(), mSink(sink), mBtnMask(0), mHoldMs(-1.0f), mRepeatMs(-1.0f),
      mVirtualDpad(0), mFilterAllButStart(0) {
    MILO_ASSERT(sink, 0x88);
    Init();
}

JoypadClient::~JoypadClient() {
    JoypadUnsubscribe(this);
    gClients.erase(std::find(gClients.begin(), gClients.end(), this));
}

BEGIN_HANDLERS(JoypadClient)
    HANDLE_MESSAGE(ButtonDownMsg)
    HANDLE_MESSAGE(ButtonUpMsg)
    HANDLE_MEMBER_PTR(mSink)
END_HANDLERS

void JoypadClient::SetVirtualDpad(bool b) { mVirtualDpad = b; }

int JoypadClient::OnMsg(const ButtonDownMsg &msg) {
    if (mFilterAllButStart && msg.GetAction() != kAction_Start)
        return 0;
    LocalUser *btnUser = msg.GetUser();
    if (mUser && mUser != btnUser)
        return 0;

    JoypadButton btn = msg.GetButton();
    if (mVirtualDpad && MovedLeftStick(btn)) {
        JoypadButton dpadbtn = LeftStickToDpad(btn);
        mSink->Handle(
            ButtonDownMsg(btnUser, dpadbtn, msg.GetAction(), msg.GetPadNum()), false
        );
    } else {
        mSink->Handle(msg, false);
    }
    if (!btnUser)
        return 0;

    if (((1 << btn) & mBtnMask)) {
        int padNum = btnUser->GetPadNum();
        mRepeats[padNum].Start(btn, msg.GetAction(), msg.GetPadNum());
        JoypadAction act = msg.GetAction();
        if (!((act != kAction_Up) && (act != kAction_Right)
           && (act != kAction_Down) && (act != kAction_Left))) {
            for (int i = 0; i < 4; i++) {
                if (i != padNum) {
                    JoypadAction otherAct = ButtonToAction(
                            mRepeats[i].mLastBtn, JoypadControllerTypePadNum(i)
                        );
                    if (!((otherAct != kAction_Up) && (otherAct != kAction_Right)
                       && (otherAct != kAction_Down) && (otherAct != kAction_Left))) {
                        mRepeats[i].mHoldTimer.Reset();
                        mRepeats[i].mRepeatTimer.Reset();
                    }
                }
            }
        }
    }
    return 0;
}

int JoypadClient::OnMsg(const ButtonUpMsg &msg) {
    if (mFilterAllButStart && msg.GetAction() != kAction_Start)
        return 0;
    LocalUser *btnUser = msg.GetUser();
    if (mUser && btnUser != mUser)
        return 0;

    JoypadButton btn = msg.GetButton();
    if (mVirtualDpad && MovedLeftStick(btn)) {
        JoypadButton dpadbtn = LeftStickToDpad(btn);
        mSink->Handle(
            ButtonUpMsg(btnUser, dpadbtn, msg.GetAction(), msg.GetPadNum()), false
        );
    } else {
        mSink->Handle(msg, false);
    }
    if (!btnUser)
        return 0;
    if (((1 << btn) & mBtnMask)) {
        mRepeats[btnUser->GetPadNum()].Reset(btn);
    }
    return 0;
}

void JoypadClient::Poll() {
    int i = 0;
    JoypadRepeat *pRepeat = mRepeats;
    do {
        if (ThePlatformMgr.GuideShowing()) {
            pRepeat->mHoldTimer.Reset();
            pRepeat->mRepeatTimer.Reset();
        } else {
            pRepeat->Poll(mHoldMs, mRepeatMs, mSink, i);
        }
        pRepeat++;
        i++;
    } while (i < 4);
}

void JoypadClient::Init() {
    JoypadSubscribe(this);
    if (gDefaultHoldMs < 0) {
        if (SystemConfig()) {
            SystemConfig("joypad")->FindData("hold_ms", gDefaultHoldMs, true);
        } else {
            gDefaultHoldMs = 500;
        }
    }
    if (gDefaultRepeatMs < 0) {
        if (SystemConfig()) {
            SystemConfig("joypad")->FindData("repeat_ms", gDefaultRepeatMs, true);
        } else {
            gDefaultRepeatMs = 50;
        }
    }
    mHoldMs = gDefaultHoldMs;
    mRepeatMs = gDefaultRepeatMs;
    gClients.push_back(this);
}

void JoypadClientPoll() {
    for (int i = 0; i < gClients.size(); i++) {
        gClients[i]->PollClient();
    }
}

void JoypadClient::SetRepeatMask(int mask) { mBtnMask = mask; }
