#include "meta_band/ContentDeletePanel.h"
#include "meta_band/BandSongMgr.h"
#include "obj/Data.h"
#include "obj/ObjMacros.h"
#include "os/ContentMgr.h"
#include "ui/UI.h"
#include "ui/UIPanel.h"
#include "ui/UIScreen.h"
#include "utl/Messages.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"

ContentDeletePanel::ContentDeletePanel()
    : unk40(0), mContent(gNullStr), unk48(0), unk49(0), mDeleteFailed(0) {}

void ContentDeletePanel::Enter() {
    UIPanel::Enter();
    unk40 = false;
    unk49 = false;
    mDeleteFailed = false;
    TheContentMgr.RegisterCallback(this, false);
}

DataNode ContentDeletePanel::OnMsg(const UITransitionCompleteMsg &) {
    if (TheContentMgr.DeleteContent(mContent)) {
        mDeleteFailed = true;
    }
    TheSongMgr.ClearFromCache(mContent);
    return DataNode(kDataUnhandled, 0);
}

void ContentDeletePanel::Poll() {
    UIPanel::Poll();
    if (!unk40) {
        if (mDeleteFailed || TheContentMgr.IsDeleteDone(mContent)) {
            unk40 = true;
            if (unk48) {
                TheContentMgr.StartRefresh();
                unk49 = true;
                static Message refresh_started_msg("refresh_started");
                HandleType(refresh_started_msg);
            } else {
                static Message refresh_done_msg("refresh_done");
                HandleType(refresh_done_msg);
            }
        }
    } else if (unk49 && TheContentMgr.RefreshDone()) {
        unk49 = false;
        static Message refresh_done_msg("refresh_done");
        HandleType(refresh_done_msg);
    }
}

void ContentDeletePanel::Exit() { TheContentMgr.UnregisterCallback(this, true); }

bool ContentDeletePanel::Exiting() const {
    return UIPanel::Exiting() || !unk40 || !TheContentMgr.RefreshDone();
}

void ContentDeletePanel::SetupDeletion(Symbol s, bool b) {
    mContent = s;
    unk48 = b;
}

void ContentDeletePanel::ContentFailed(const char *cc) {
    if (mContent == cc) {
        mDeleteFailed = true;
    }
}

BEGIN_HANDLERS(ContentDeletePanel)
    HANDLE_ACTION(setup_deletion, SetupDeletion(_msg->Sym(2), _msg->Int(3)))
    HANDLE_MESSAGE(UITransitionCompleteMsg)
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_CHECK(0xA2)
END_HANDLERS

// Retail's SyncProperty compares against FUNCTION-LOCAL static Symbols (guard word +
// ??__F atexit funclet per prop), not the centralized globals in utl/Symbols*.h --
// same divergence RB3_HANDLE_LOCAL_STATIC fixes for the HANDLE_* family. SYNC_PROP is
// not covered by that gate, so override it TU-locally (no other TU's codegen moves).
#undef SYNC_PROP
#define SYNC_PROP(symbol, member)                                                        \
    {                                                                                    \
        static Symbol _ps(#symbol);                                                      \
        if (sym == _ps)                                                                  \
            return PropSync(member, _val, _prop, _i + 1, _op);                           \
    }

BEGIN_PROPSYNCS(ContentDeletePanel)
    SYNC_PROP(content, mContent)
END_PROPSYNCS
