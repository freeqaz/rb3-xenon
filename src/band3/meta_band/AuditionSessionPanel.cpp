#include "meta_band/AuditionSessionPanel.h"
#include "game/BandUser.h"
#include "meta_band/AuditionMgr.h"
#include "meta/Profile.h"
#include "meta_band/Utl.h"
#include "obj/Data.h"
#include "obj/Msg.h"
#include "obj/ObjMacros.h"
#include "os/ContentMgr.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"
#include "ui/UIPanel.h"
#include "utl/Symbol.h"

// Every body here is written from the retail XEX (TU5); no surviving source has
// this class. Addresses are the retail function starts.

// 0x826033F8: only the vtable stores (and the virtual-base construction).
AuditionSessionPanel::AuditionSessionPanel() {}

// 0x82603518
void AuditionSessionPanel::Exit() {
    TheAuditionMgr->mBuilder->mPanel = NULL;
    ThePlatformMgr.RemoveSink(this);
    UIPanel::Exit();
}

// 0x82603590: a bare `b UIPanel::Poll`.
void AuditionSessionPanel::Poll() { UIPanel::Poll(); }

// 0x826035A0
Symbol AuditionSessionPanel::GetSlotDiffSym(int slot) {
    static Symbol overshell_easy("overshell_easy");
    static Symbol overshell_medium("overshell_medium");
    static Symbol overshell_hard("overshell_hard");
    static Symbol overshell_expert("overshell_expert");
    switch (TheAuditionMgr->mBuilder->GetSlotDifficulty(slot)) {
    case 0:
        return overshell_easy;
    case 1:
        return overshell_medium;
    case 2:
        return overshell_hard;
    case 3:
        return overshell_expert;
    default:
        return Symbol();
    }
}

// 0x82603760
Symbol AuditionSessionPanel::GetSlotTrackSym(int slot) {
    static Symbol track_guitar("track_guitar");
    static Symbol track_bass("track_bass");
    static Symbol keys("keys");
    static Symbol real_keys("real_keys");
    static Symbol track_drum("track_drum");
    static Symbol real_drum("real_drum");
    static Symbol track_vocals("track_vocals");
    static Symbol overshell_vocal_harmony("overshell_vocal_harmony");
    switch (TheAuditionMgr->mBuilder->GetSlotTrack(slot)) {
    case 0:
        return track_guitar;
    case 1:
        return track_bass;
    case 2:
        return keys;
    case 3:
        return real_keys;
    case 4:
        return track_drum;
    case 5:
        return real_drum;
    case 6:
        return track_vocals;
    case 7:
        return overshell_vocal_harmony;
    default:
        return Symbol();
    }
}

// 0x82603C68: the slot user's controller, or the controller the slot's track
// implies when the slot is empty.
const char *AuditionSessionPanel::GetInstIcon(int slot) {
    AuditionSessionBuilder *builder = TheAuditionMgr->mBuilder;
    LocalBandUser *user = builder->mSlots[slot]->mUser;
    ControllerType type;
    if (user) {
        type = user->GetControllerType();
    } else {
        type = (ControllerType)AuditionSessionBuilder::SlotTrackToControllerType(
            builder->GetSlotTrack(slot)
        );
    }
    return GetFontCharFromControllerType(type, 0);
}

// 0x82603CD8
void AuditionSessionPanel::Refresh() {
    static Message update("update");
    HandleType(update);
}

// 0x82603D90 (shared by both message types, see the header).
DataNode AuditionSessionPanel::OnMsg(const SigninChangedMsg &) {
    Refresh();
    return 0;
}

DataNode AuditionSessionPanel::OnMsg(const ProfileSwappedMsg &) {
    Refresh();
    return 0;
}

// 0x82603DE0 (reached through the vtordisp thunk 0x826046C8).
BEGIN_HANDLERS(AuditionSessionPanel)
    HANDLE_EXPR(is_slot_empty, TheAuditionMgr->mBuilder->GetSlotState(_msg->Int(2)) == 0)
    HANDLE_EXPR(
        is_slot_autoplay, TheAuditionMgr->mBuilder->GetSlotState(_msg->Int(2)) == 2
    )
    HANDLE_EXPR(
        get_slot_has_part, TheAuditionMgr->mBuilder->mSlots[_msg->Int(2)]->mHasPart
    )
    HANDLE_EXPR(get_user, TheAuditionMgr->mBuilder->mSlots[_msg->Int(2)]->mUser)
    HANDLE_EXPR(get_inst_icon, GetInstIcon(_msg->Int(2)))
    HANDLE_EXPR(get_slot_diff_sym, GetSlotDiffSym(_msg->Int(2)))
    HANDLE_EXPR(get_slot_track_sym, GetSlotTrackSym(_msg->Int(2)))
    HANDLE_ACTION(
        add_autoplayer, TheAuditionMgr->mBuilder->SetSlotAutoplay(_msg->Int(2), true)
    )
    HANDLE_ACTION(
        remove_autoplayer, TheAuditionMgr->mBuilder->SetSlotAutoplay(_msg->Int(2), false)
    )
    HANDLE_ACTION(
        next_slot_diff, TheAuditionMgr->mBuilder->NextSlotDifficulty(_msg->Int(2))
    )
    HANDLE_ACTION(next_slot_track, TheAuditionMgr->mBuilder->NextSlotTrack(_msg->Int(2)))
    HANDLE_ACTION(session_data_changed, Refresh())
    HANDLE_MESSAGE(SigninChangedMsg)
    HANDLE_MESSAGE(ProfileSwappedMsg)
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_CHECK(0)
END_HANDLERS

// 0x826046D8
void AuditionSessionPanel::Enter() {
    UIPanel::Enter();
    AuditionSessionBuilder *builder = TheAuditionMgr->mBuilder;
    builder->Reset();
    builder->mPanel = this;
    ThePlatformMgr.AddSink(this);
    MILO_ASSERT(!TheContentMgr.RefreshInProgress(), 0);
    Refresh();
}
