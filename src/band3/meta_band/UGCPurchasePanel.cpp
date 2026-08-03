#include "meta_band/UGCPurchasePanel.h"
#include "macros.h"
#include "meta/Profile.h"
#include "meta_band/BandSongMgr.h"
#include "meta/StoreOffer.h"
#include "meta_band/UIEventMgr.h"
#include "net_band/DataResults.h"
#include "net_band/RockCentral.h"
#include "net_band/RockCentralMsgs.h"
#include "obj/Data.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"
#include "ui/UIPanel.h"
#include "utl/Messages.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"

UGCPurchasePanel::UGCPurchasePanel()
    : mPurchaseState(0), mUser(0), mSong(gNullStr), mOfferID(0), mPurchaser(0),
      unk4c(0) {}

void UGCPurchasePanel::Enter() {
    MILO_ASSERT(kUninitialized == mPurchaseState, 0x22);
    UIPanel::Enter();
    ThePlatformMgr.AddSink(this, SigninChangedMsg::Type());
    XBackgroundDownloadSetMode(XBACKGROUND_DOWNLOAD_MODE_ALWAYS_ALLOW);
    unk4c = false;
    mPurchaseState = 1;
}

void UGCPurchasePanel::Exit() { UIPanel::Exit(); }

void UGCPurchasePanel::Unload() {
    mUser = NULL;
    mPurchaseState = 0;
    ThePlatformMgr.RemoveSink(this, SigninChangedMsg::Type());
    mResultList.Clear();
    RELEASE(mPurchaser);
    UIPanel::Unload();
}

// Retail fn_8263EDF0 (0x450). Three function-local statics share the packed guard
// word lbl_82E01730, and the bit indices give the declaration order (lane CF-7,
// read off the asm; strings from band.exe):
//   0x1 Symbol  demo_upgrade       -- initialised at function TOP, BEFORE the
//                                     UIPanel::Poll() call, so it must be declared
//                                     first even though its only use is in case 3
//   0x2 Message checkout_failed    -- case 5
//   0x4 Message checkout_finished  -- case 6
// MSVC emits the guard AT the declaration point and does not hoist, so the
// placement above is load-bearing, not cosmetic.
void UGCPurchasePanel::Poll() {
    static Symbol demo_upgrade("demo_upgrade");
    UIPanel::Poll();
    switch (mPurchaseState) {
    case 1:
        if (!ThePlatformMgr.GuideShowing()) {
            mPurchaseState = 2;
            mResultList.Clear();
            TheRockCentral.GetSongFullOffer(
                TheSongMgr.GetSongIDFromShortName(mSong, true), mResultList, this
            );
        }
        break;
    case 2:
        break;
    case 3: {
        // Retail (target fn 0x8263edf0, case 3) does substantially more than the
        // Wii-dev source (which is just `mPurchaseState = 5; break;` -- ../rb3
        // checked, confirms case 3 is a retail-360-only addition): it optionally
        // derives a flags/index value from a global singleton (DAT_82cbfaec in the
        // Ghidra decompile), then constructs an XboxPurchaser via placement new,
        // mirroring the StorePanel::CheckOut idiom (StorePanel.cpp).
        unsigned int flags = 0;
        // TODO(unresolved): retail guards this block on a global singleton
        // (Ghidra DAT_82cbfaec) -- `if (singleton && singleton->vtbl[0x14]())
        // flags = singleton->vtbl[0x1c](mUser->GetPadNum());`. Singleton's class
        // could not be identified within budget, so this defaults to flags=0
        // (matching StorePanel::CheckOut's non-guarded call site). This leaves the
        // guarded ~9-instruction prefix of case 3 unmatched but should recover the
        // bulk of the body (OfferStringToID/GetPadNum/alloc/ctor/Initiate).
        mPurchaseState = 4;
        void *mem = operator new(sizeof(XboxPurchaser));
        StorePurchaser *purchaser;
        if (mem) {
            purchaser = new (mem) XboxPurchaser(
                mUser->GetPadNum(),
                StorePurchaseable::OfferStringToID(mOfferID),
                0,
                0,
                demo_upgrade,
                flags
            );
        } else {
            purchaser = 0;
        }
        mPurchaser = purchaser;
        mPurchaser->Initiate();
        break;
    }
    case 4:
        MILO_ASSERT(mPurchaser, 0x71);
        mPurchaser->Poll();
        if (!mPurchaser->IsPurchasing()) {
            if (mPurchaser->PurchaseMade()) {
                mPurchaseState = 6;
                unk4c = mPurchaser->IsSuccess();
                if (unk4c) {
                    TheSongMgr.ClearFromCache(TheSongMgr.ContentName(mSong, true));
                }
            } else {
                mPurchaseState = 5;
            }
            RELEASE(mPurchaser);
            mPurchaser = 0;
        }
        break;
    case 5: {
        mPurchaseState = 0;
        // Retail uses a FUNCTION-LOCAL static here (guard bit 0x2 of lbl_82E01730,
        // ctor string lbl_820CE060 = "checkout_failed"), not the interned global.
        static Message checkout_failed("checkout_failed");
        Handle(checkout_failed, false);
        break;
    }
    case 6: {
        mPurchaseState = 0;
        static Message msg("checkout_finished", 0);
        msg[0] = unk4c;
        Handle(msg, false);
        break;
    }
    case 0:
        break;
    default:
        MILO_ASSERT(0, 0xA1);
        break;
    }
}

DataNode UGCPurchasePanel::OnMsg(const SigninChangedMsg &) {
    if (!ThePlatformMgr.IsUserSignedIn(mUser)) {
        static Symbol sign_out("sign_out");
        if (TheUIEventMgr->CurrentTransitionEvent() != sign_out) {
            static Message init("init", 0);
            init[0] = 0;
            TheUIEventMgr->TriggerEvent(sign_out, init);
        }
        return 1;
    }
    return DataNode(kDataUnhandled, 0);
}

DataNode UGCPurchasePanel::OnMsg(const RockCentralOpCompleteMsg &msg) {
    if (mPurchaseState == 2) {
        if (msg.Success()) {
            mResultList.Update(NULL);
            DataNode n28;
            DataResult *res = mResultList.GetDataResult(0);
            res->GetDataResultValue("offer_id", n28);
            if (n28.Type() == kDataString) {
                mOfferID = n28.Str();
                mPurchaseState = 3;
                return 1;
            }
        }
    }
    mPurchaseState = 5;
    return 1;
}

BEGIN_HANDLERS(UGCPurchasePanel)
    HANDLE_MESSAGE(SigninChangedMsg)
    HANDLE_MESSAGE(RockCentralOpCompleteMsg)
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_CHECK(0xD6)
END_HANDLERS

BEGIN_PROPSYNCS(UGCPurchasePanel)
    SYNC_PROP(song, mSong)
    SYNC_PROP(user, mUser)
END_PROPSYNCS
