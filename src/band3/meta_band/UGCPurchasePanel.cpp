#include "meta_band/UGCPurchasePanel.h"
#include "macros.h"
#include "meta/Profile.h"
#include "meta_band/BandSongMgr.h"
#include "meta/StoreOffer.h"
#include "meta_band/UIEventMgr.h"
#include "net_band/DataResults.h"
#include "net_band/RockCentral.h"
#include "net_band/RockCentralMsgs.h"
#include "net/Net.h"
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

void UGCPurchasePanel::Exit() {
    // The Wii oracle has this as a bare `UIPanel::Exit();` -- character-identical
    // to what we had -- and structurally CANNOT contain this line: it is a 360-only
    // XAM call. Retail's `li r3,2; bl __imp_XamBackgroundDownloadSetMode` says
    // Exit restores AUTO, undoing the ALWAYS_ALLOW that Enter above sets.
    XBackgroundDownloadSetMode(XBACKGROUND_DOWNLOAD_MODE_AUTO);
    UIPanel::Exit();
}

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
        // RESOLVED (lane W16-CA, 2026-09-15). The "unidentified global singleton"
        // is `TheNet`: objdiff names retail's two relocations here outright as
        // ?TheNet@@3VNet@@A@h / @l, and Ghidra's DAT_82cbfaec is simply
        // TheNet + 0x34 (0x82cbfaec - 0x34 == 0x82cbfab8 == &TheNet).
        // Net + 0x34 is `Server *mServer` -- VERIFIED by the compiler
        // (cl /d1reportSingleClassLayoutNet), not by the header comments.
        // The two vcalls are `lwz r11,0x14(vptr)` = slot 5 and
        // `lwz r11,0x1c(vptr)` = slot 7 of the PRIMARY (Server@Server@) vtable,
        // which cl /d1reportSingleClassLayoutServer prints as
        //   [ 5] Server::IsConnected      [ 7] Server::GetPlayerID
        // Corroborated three ways: the slot arithmetic agrees with this header's
        // own retail-attested anchors (GetPersistentStoreClient 0x34 = 13,
        // GetCompetitionClient 0x38 = 14, recorded by lane W16-G); retail tests
        // slot 5's result with `clrlwi. r11,r3,24`, i.e. a bool, and IsConnected
        // is the only bool-returning slot in that neighbourhood; and slot 7 is
        // handed mUser->GetPadNum() and its result is what feeds the ctor's
        // trailing flags argument, which is what GetPlayerID(int) is for.
        unsigned int flags = 0;
        Server *server = TheNet.GetServer();
        if (server && server->IsConnected()) {
            flags = server->GetPlayerID(mUser->GetPadNum());
        }
        mPurchaseState = 4;
        // ONE temporary, not two.  The `void *mem` + separate `purchaser` idiom
        // (copied here from StorePanel.cpp:302) costs a second 4-byte stack slot
        // at 0x60: retail stores the allocation pointer once (`mr. r25,r3;
        // stw r25,0x5c(r31)`) where we stored it at 0x5c AND 0x60.  That one
        // extra slot pushed every later local by +8 and rounded the frame
        // 0xc0 -> 0xd0, which was 17 of this function's 35 charged sites.
        mPurchaser = new XboxPurchaser(
            mUser->GetPadNum(),
            StorePurchaseable::OfferStringToID(mOfferID),
            0,
            0,
            demo_upgrade,
            flags
        );
        mPurchaser->Initiate();
        break;
    }
    case 4:
        MILO_ASSERT(mPurchaser, 0x71);
        mPurchaser->Poll();
        if (!mPurchaser->IsPurchasing()) {
            // slot 3 (0xc) is IsSuccess, slot 4 (0x10) is PurchaseMade -- see
            // StorePurchaser.h.  Retail @ 0x8263f018 dispatches 0xc, stores 6 to
            // +0x3c, then dispatches 0x10 and `stb r3, 0x50`.  Swapped with the
            // declaration order, so the emitted offsets are unchanged.
            if (mPurchaser->IsSuccess()) {
                mPurchaseState = 6;
                unk4c = mPurchaser->PurchaseMade();
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
