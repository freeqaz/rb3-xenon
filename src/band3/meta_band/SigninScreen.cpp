#include "meta_band/SigninScreen.h"
#include "BandScreen.h"
#include "math/Utl.h"
#include "game/BandUser.h"
#include "game/BandUserMgr.h"
#include "meta/Profile.h"
#include "meta_band/InputMgr.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"
#include "os/User.h"
#include "ui/UIScreen.h"
#include "utl/Messages2.h"
#include "utl/Messages3.h"
#include "utl/Symbols.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"

SigninScreen::SigninScreen()
    : mMustNotBeAGuest(0), mMustBeOnline(0), mMustBeMultiplayerCapable(0),
      mHandleSignOuts(0), mLimitUserSignin(0), unk41(0) {}

SigninScreen::~SigninScreen() {}

void SigninScreen::Poll() { UIScreen::Poll(); }

LocalBandUser *SigninScreen::GetUser() {
    LocalBandUser *user = nullptr;
    static Symbol signing_in_user("signing_in_user");
    const DataNode *prop = Property(signing_in_user, false);
    if (prop) {
        user = prop->Obj<LocalBandUser>();
    }
    if (!user) {
        if (TheInputMgr->GetUser()) {
            user = TheInputMgr->GetUser()->GetLocalBandUser();
        } else
            user = nullptr;
    }
    return user;
}

void SigninScreen::Enter(UIScreen *screen) {
    BandScreen::Enter(screen);
    ThePlatformMgr.AddSink(this);
    static Symbol limit_user_signin("limit_user_signin");
    mLimitUserSignin = Property(limit_user_signin)->Int();
    if (mLimitUserSignin) {
        BandUser *pUser = GetUser();
        if (!pUser) {
            MILO_FAIL("SigninScreen %s got NULL from GetUser()\n", Name());
        } else
            MILO_ASSERT(pUser->IsLocal(), 0x49);
    }
    static Symbol must_not_be_a_guest("must_not_be_a_guest");
    mMustNotBeAGuest = Property(must_not_be_a_guest)->Int();
    static Symbol must_be_online("must_be_online");
    mMustBeOnline = Property(must_be_online)->Int();
    static Symbol must_be_multiplayer_capable("must_be_multiplayer_capable");
    mMustBeMultiplayerCapable = Property(must_be_multiplayer_capable)->Int();
    static Symbol handle_sign_outs("handle_sign_outs");
    mHandleSignOuts = Property(handle_sign_outs)->Int();
}

void SigninScreen::Exit(UIScreen *s) {
    ThePlatformMgr.RemoveSink(this);
    BandScreen::Exit(s);
}

void SigninScreen::ReEvaluateState() {
    if (!GetUser())
        return;

    LocalBandUser *user = GetUser()->GetLocalBandUser();
    bool ok = false;
    if (mMustBeOnline && ThePlatformMgr.IsUserSignedIntoLive(user)) {
        ok = true;
    } else if (mMustBeMultiplayerCapable && ThePlatformMgr.IsUserSignedIn(user)
               && ThePlatformMgr.UserHasOnlinePrivilege(user)) {
        ok = true;
    }
    if (ok) {
        ok = !(mMustNotBeAGuest && ThePlatformMgr.IsUserAGuest(user));
    }
    if (ok) {
        static Message on_signed_in_msg("on_signed_in");
        Handle(on_signed_in_msg, true);
    }
}

DataNode SigninScreen::OnMsg(const SigninChangedMsg &msg) {
    int state = 0;
    std::vector<LocalBandUser *> &users = TheBandUserMgr->GetLocalBandUsers();
    FOREACH (it, users) {
        LocalBandUser *pUser = *it;
        MILO_ASSERT(pUser, 0xA4);
        if (!mLimitUserSignin || pUser == GetUser()) {
            int bit = 1 << pUser->GetPadNum();
            bool isSignedIn = bit & msg.GetMask();
            MILO_ASSERT(isSignedIn == ThePlatformMgr.IsUserSignedIn(pUser), 0xAE);
            if (isSignedIn) {
                if (mMustNotBeAGuest && ThePlatformMgr.IsUserAGuest(pUser)) {
                    state = Max(state, 1);
                } else if (mMustBeOnline && !ThePlatformMgr.IsUserSignedIntoLive(pUser)) {
                    state = Max(state, 2);
                } else if (mMustBeMultiplayerCapable
                           && !ThePlatformMgr.UserHasOnlinePrivilege(pUser)) {
                    state = Max(state, 3);
                } else {
                    state = Max(state, 4);
                }
                unk41 = false;
            }
        }
    }
    switch (state) {
    case 1: {
        static Message on_signed_into_guest_msg("on_signed_into_guest");
        Handle(on_signed_into_guest_msg, true);
        break;
    }
    case 2: {
        static Message on_not_online_msg("on_not_online");
        Handle(on_not_online_msg, true);
        break;
    }
    case 3: {
        static Message on_not_multiplayer_capable_msg("on_not_multiplayer_capable");
        Handle(on_not_multiplayer_capable_msg, true);
        break;
    }
    case 4: {
        static Message on_signed_in_msg("on_signed_in");
        Handle(on_signed_in_msg, true);
        break;
    }
    }
    if (ThePlatformMgr.GuideShowing()) {
        unk41 = true;
    }
    return 0;
}

DataNode SigninScreen::OnMsg(const UIChangedMsg &msg) {
    if (msg->Int(2) == 0) {
        if (unk41 && mHandleSignOuts
            && !ThePlatformMgr.IsUserSignedIn(GetUser())) {
            static Message on_signed_out_msg("on_signed_out");
            Handle(on_signed_out_msg, true);
        } else {
            ReEvaluateState();
        }
        unk41 = false;
    }
    return 0;
}

BEGIN_HANDLERS(SigninScreen)
    HANDLE_MESSAGE(SigninChangedMsg)
    HANDLE_MESSAGE(UIChangedMsg)
    HANDLE_SUPERCLASS(BandScreen)
    HANDLE_CHECK(0x11C)
END_HANDLERS
