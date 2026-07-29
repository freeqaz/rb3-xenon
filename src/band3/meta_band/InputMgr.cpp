#include "meta_band/InputMgr.h"
#include "game/BandUser.h"
#include "game/BandUserMgr.h"
#include "game/Defines.h"
#include "meta_band/BandUI.h"
#include "meta_band/ModifierMgr.h"
#include "meta_band/NetSync.h"
#include "meta_band/SessionMgr.h"
#include "meta_band/UIEventMgr.h"
#include "meta_band/Utl.h"
#include "net/NetSession.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Msg.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "os/Joypad.h"
#include "os/JoypadMsgs.h"
#include "ui/UI.h"
#include "utl/Symbols.h"

// ---------------------------------------------------------------------------
// RB3-360 retail: .text 0x825B0518..0x825B22A8.
//
// Retail-vs-Wii-dev divergences confirmed by reading the target:
//  * AllowInput (0x825B0B18) has NO ThePlatformMgr.mHomeMenuWii /
//    TheVirtualKeyboard.IsKeyboardShowing() arm -- that is Wii-platform only.
//  * IsValidButtonForShell (0x825B0BA8) does NOT call
//    BandUserMgr::DebugGetControllerTypeOverride; the R1/RStick arm is just
//    `(unsigned)userType > 1` (0x825B0BEC..0x825B0C04).
//  * GetUserWithInvalidController (0x825B15A8) has NO
//    DataVariable(fake_controllers) early-out -- it opens straight on mUser.
//  * MILO_ASSERT is (void)(cond) in retail: no assert strings anywhere in the
//    span.
//  * Handle's 9 dispatch Symbols are FUNCTION-LOCAL statics (guard word
//    0x82DFF420 bits 0..8, cleared by the nine 32-byte ??__F funclets at
//    0x825B2144..0x825B2264) -- hence /DRB3_HANDLE_LOCAL_STATIC on this object.
// ---------------------------------------------------------------------------

InputMgr *TheInputMgr;

void InputMgr::Init() {
    MILO_ASSERT(!TheInputMgr, 0x28);
    TheInputMgr = new InputMgr(TheBandUserMgr, TheUIEventMgr, TheNetSync, TheSessionMgr);
    TheInputMgr->SetName("input_mgr", ObjectDir::Main());
}

void InputMgr::Terminate() { RELEASE(TheInputMgr); }

InputMgr::InputMgr(BandUserMgr *umgr, UIEventMgr *emgr, NetSync *sync, SessionMgr *smgr)
    : mBandUserMgr(umgr), mEventMgr(emgr), mNetSync(sync), mSessionMgr(smgr),
      mAutoVocalsConfirmAllowed(0), unk2d(0), mUser(0) {
    if (mSessionMgr) {
        mSessionMgr->AddSink(this, LocalUserLeftMsg::Type());
        mSessionMgr->AddSink(this, SigninChangedMsg::Type());
    }
}

InputMgr::~InputMgr() {
    if (mSessionMgr) {
        mSessionMgr->RemoveSink(this, SigninChangedMsg::Type());
        mSessionMgr->RemoveSink(this, LocalUserLeftMsg::Type());
    }
}

bool InputMgr::IsActiveAndConnected(ControllerType ct) const {
    MILO_ASSERT((kControllerDrum) <= (ct) && (ct) < (kControllerNone), 0x50);
    bool canexitremotely = AllowRemoteExit();
    std::vector<BandUser *> bandusers;
    if (mBandUserMgr) {
        mBandUserMgr->GetBandUsers(&bandusers, 0);
    }
    for (std::vector<BandUser *>::iterator it = bandusers.begin(); it != bandusers.end();
         ++it) {
        BandUser *cur = *it;
        if (cur->IsLocal()) {
            LocalBandUser *localUser = cur->GetLocalBandUser();
            if (HasValidController(localUser, ct)) {
                if (AllowInput(cur))
                    return true;
                if (canexitremotely) {
                    if (mSessionMgr->HasUser(cur))
                        return true;
                }
            }
        }
    }
    return false;
}

void InputMgr::CheckTriggerAutoVocalsConfirm() {
    static Symbol mod_auto_vocals("mod_auto_vocals");
    if (TheModifierMgr->IsModifierActive(mod_auto_vocals))
        return;
    if (!mAutoVocalsConfirmAllowed)
        return;
    if (!TheBandUI.GetOvershell()->IsAutoVocalsAllowed())
        return;
    if (unk2d)
        return;
    int i1 = 0;
    MILO_ASSERT(mBandUserMgr, 0x82);
    std::vector<LocalBandUser *> users;
    mBandUserMgr->GetLocalUsersWithAnyController(users);
    for (int i = 0; i < users.size(); i++) {
        ControllerType ty = users[i]->ConnectedControllerType();
        if (ty - 3U <= 1 || ty == 1)
            i1++;
    }
    if (i1 >= 3) {
        unk2d = true;
        static Symbol auto_vocals_confirm("auto_vocals_confirm");
        TheUIEventMgr->TriggerEvent(auto_vocals_confirm, 0);
    }
}

BandUser *InputMgr::GetUser() { return mUser; }

void InputMgr::SetUser(BandUser *user) {
    mUser = user;
    ExportStatusChangedMsg();
}

LocalBandUser *InputMgr::GetUserWithInvalidController() const {
    if (!mUser)
        return nullptr;
    bool b1 = false;
    if (mUser && mUser->IsLocal())
        b1 = true;
    BandUser *user = b1 ? mUser : 0;
    std::vector<LocalBandUser *> users;
    if (mBandUserMgr)
        mBandUserMgr->GetLocalBandUsers(&users, 0);
    for (std::vector<LocalBandUser *>::iterator it = users.begin(); it != users.end();
         ++it) {
        LocalBandUser *cur = *it;
        if (mSessionMgr->HasUser(cur) || (user == cur)) {
            cur->GetControllerType();
            if (!HasValidController(cur, cur->GetControllerType()))
                return cur;
        }
    }
    return nullptr;
}

void InputMgr::SetInvalidMessageSink(Hmx::Object *) {}
void InputMgr::ClearInvalidMessageSink() {}

bool InputMgr::AllowRemoteExit() const {
    bool hasRemoteUsers = false;
    if (mNetSync && mNetSync->GetUIState() == (NetUIState)20) {
        hasRemoteUsers = true;
    }
    bool notLocal = !TheNetSession->IsLocal();
    if (mEventMgr && !mEventMgr->HasActiveEvent()
        && ((mNetSync && mNetSync->IsEnabled()) || hasRemoteUsers)) {
        if (mUser == NULL) {
            if (hasRemoteUsers && notLocal)
                return true;
            if (!IsLeaderLocal())
                return true;
        } else if (mUser) {
            if (!mUser->IsLocal())
                return true;
        }
    }
    return false;
}

DataNode InputMgr::OnMsg(const LocalUserLeftMsg &msg) {
    if (mUser && mUser->IsLocal()) {
        if (mUser->GetLocalUser() == msg.GetUser()) {
            ExportUserLeftMsg();
        }
    }
    return 1;
}

DataNode InputMgr::OnMsg(const SigninChangedMsg &msg) {
    if (mUser && mUser->IsLocal()) {
        if (!mUser->GetLocalBandUser()->CanSaveData()) {
            ExportUserLeftMsg();
        }
    }
    return 1;
}

DataNode InputMgr::OnMsg(const JoypadConnectionMsg &msg) {
    ExportStatusChangedMsg();
    CheckTriggerAutoVocalsConfirm();
    return DataNode(kDataUnhandled, 0);
}

DataNode InputMgr::OnMsg(const ButtonDownMsg &msg) {
    MILO_ASSERT(mBandUserMgr, 0x125);
    BandUser *pUser = BandUserMgr::GetBandUser(msg.GetUser());
    MILO_ASSERT(pUser && pUser->IsLocal(), 0x127);
    LocalBandUser *pLocalBandUser = pUser->GetLocalBandUser();
    if (msg.GetAction() == kAction_Cancel && mEventMgr
        && !mEventMgr->HasActiveDialogEvent()
        && TheUI->GetTransitionState() == UIManager::kTransitionNone
        && !TheUI->InComponentSelect() && AllowRemoteExit()) {
        if (mSessionMgr->HasUser(pLocalBandUser) && pLocalBandUser->mOvershellState == 5) {
            static Symbol remote_exit("remote_exit");
            mEventMgr->TriggerEvent(remote_exit, 0);
            return 0;
        }
    }
    if (!AllowInput(pLocalBandUser))
        return 0;
    else
        return DataNode(kDataUnhandled, 0);
}

DataNode InputMgr::OnMsg(const ButtonUpMsg &msg) {
    BandUser *pUser = BandUserMgr::GetBandUser(msg.GetUser());
    MILO_ASSERT(pUser && pUser->IsLocal(), 0x146);
    LocalBandUser *pLocalBandUser = pUser->GetLocalBandUser();
    if (!AllowInput(pLocalBandUser))
        return 0;
    else
        return DataNode(kDataUnhandled, 0);
}

void InputMgr::ExportStatusChangedMsg() {
    static InputStatusChangedMsg msg;
    Export(msg, false);
}

void InputMgr::ExportUserLeftMsg() {
    static InputUserLeftMsg msg;
    Export(msg, false);
}

bool InputMgr::HasValidController(LocalBandUser *user, ControllerType ty) const {
    if (!user->IsJoypadConnected())
        return false;
    else if (!mUser || (mEventMgr && mEventMgr->HasActiveEvent())) {
        bool ret = false;
        if (ty == 5 || ty == user->ConnectedControllerType())
            ret = true;
        return ret;
    } else {
        ControllerType uTy = user->GetControllerType();
        bool ret = false;
        if ((uTy == 5 || uTy == ty) && (ty == 5 || ty == user->ConnectedControllerType()))
            ret = true;
        return ret;
    }
}

bool InputMgr::AllowInput(BandUser *user) const {
    if (mNetSync && mNetSync->IsBlockingTransition())
        return false;
    if (mEventMgr && mEventMgr->HasActiveDialogEvent())
        return true;
    else if (mUser)
        return user == mUser;
    else
        return true;
}

bool InputMgr::IsValidButtonForShell(JoypadButton btn, LocalBandUser *user) {
    ControllerType userType = user->ConnectedControllerType();
    switch (btn) {
    case kPad_L2:
    case kPad_R2:
    case kPad_L1:
    case kPad_Tri:
    case kPad_Circle:
    case kPad_X:
    case kPad_Square:
    case kPad_Select:
    case kPad_L3:
    case kPad_R3:
    case kPad_Start:
    case kPad_DUp:
    case kPad_DRight:
    case kPad_DDown:
    case kPad_DLeft:
    case kPad_LStickUp:
    case kPad_LStickRight:
    case kPad_LStickDown:
    case kPad_LStickLeft:
        return true;
    case kPad_R1:
    case kPad_RStickUp:
    case kPad_RStickRight:
    case kPad_RStickDown:
    case kPad_RStickLeft:
        return (unsigned int)userType > 1;
    default:
        return false;
    }
}

BEGIN_HANDLERS(InputMgr)
    HANDLE_MESSAGE(LocalUserLeftMsg)
    HANDLE_MESSAGE(SigninChangedMsg)
    HANDLE_MESSAGE(JoypadConnectionMsg)
    HANDLE_MESSAGE(ButtonDownMsg)
    HANDLE_MESSAGE(ButtonUpMsg)
    HANDLE_EXPR(has_user, mUser != nullptr)
    HANDLE_EXPR(get_user, mUser)
    HANDLE_ACTION(set_user, SetUser(_msg->Obj<BandUser>(2)))
    HANDLE_ACTION(clear_user, SetUser(nullptr))
    HANDLE_EXPR(get_user_with_invalid_controller, GetUserWithInvalidController())
    HANDLE_EXPR(allow_remote_exit, AllowRemoteExit())
    HANDLE_EXPR(
        is_valid_button_for_shell,
        IsValidButtonForShell((JoypadButton)_msg->Int(2), _msg->Obj<LocalBandUser>(3))
    )
    HANDLE_ACTION(check_trigger_auto_vocals_confirm, CheckTriggerAutoVocalsConfirm())
    HANDLE_ACTION(set_auto_vocals_confirm_allowed, mAutoVocalsConfirmAllowed = true)
    HANDLE_SUPERCLASS(MsgSource)
    HANDLE_CHECK(0x2B1)
END_HANDLERS
