#include "meta_band/SongSelectPanel.h"
#include "AppMiniLeaderboardDisplay.h"
#include "game/BandUser.h"
#include "meta_band/Leaderboard.h"
#include "meta_band/MusicLibrary.h"
#include "meta_band/PlayerLeaderboards.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/SongSortNode.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/ObjMacros.h"
#include "obj/Task.h"
#include "os/ContentMgr.h"
#include "os/Debug.h"
#include "os/JoypadMsgs.h"
#include "ui/UIList.h"
#include "ui/UIPanel.h"
#include "utl/Messages2.h"
#include "utl/Symbols.h"
#include "utl/Symbols3.h"
#ifdef HX_NATIVE
#include "rndobj/Group.h"
#endif

SongSelectPanel::SongSelectPanel()
    : mLeaderboard(0), unk48(0), unk4c(0), unk50(0), unk54(0), unk58(-1) {}

void SongSelectPanel::Load() {
    UIPanel::Load();
    TheMusicLibrary->OnLoad();
    TheContentMgr.StartRefresh();
}

bool SongSelectPanel::IsLoaded() const {
    return UIPanel::IsLoaded() && !TheContentMgr.RefreshInProgress();
}

void SongSelectPanel::FinishLoad() {
    UIPanel::FinishLoad();
#ifdef HX_NATIVE
    // The mini-leaderboard display (online scores) is absent from the 360-ARK
    // extract's ui/song_select/song_select.milo, and online leaderboards don't
    // exist offline anyway. Find it non-failing; unk48 stays null and the
    // leaderboard-rotation Poll path is gated on unk58 >= 0 (only armed once a
    // real online leaderboard enumerates, which never happens offline), so a null
    // unk48 is safe. (Same tolerant-asset pattern as MetaPanel metamusic.)
    unk48 = mDir->Find<AppMiniLeaderboardDisplay>("leaderboard.mld", false);
#else
    unk48 = mDir->Find<AppMiniLeaderboardDisplay>("leaderboard.mld", true);
#endif
    unk4c = TypeDef()->FindFloat("mini_leaderboard_rotation_off");
    unk50 = TypeDef()->FindFloat("mini_leaderboard_rotation_on");
#ifdef HX_NATIVE
    // The mini-leaderboard panel group (live_lb.grp, "FRIEND RANKINGS" title +
    // online score rows) is authored showing-by-default in the milo. On the Wii
    // the leaderboard_hide.trg EventTrigger anim fades it out (env alpha) until
    // the rotation timer swaps it in once online scores enumerate. The native
    // renderer doesn't honor that env-alpha fade for hide, so the group stays
    // fully visible and overlaps the difficulty grid — and offline it should
    // never appear at all (online enumerate never completes). Force the same
    // initial state the rotation expects: leaderboard hidden, difficulty grid
    // shown. The Poll() show-path (set_mini_leaderboard_showing 1) re-shows it
    // explicitly when an online leaderboard actually becomes ready.
    SetMiniLeaderboardGroupShowing(false);
#endif
}

#ifdef HX_NATIVE
void SongSelectPanel::SetMiniLeaderboardGroupShowing(bool showing) {
    if (RndGroup *lb = mDir->Find<RndGroup>("live_lb.grp", false))
        lb->SetShowing(showing);
    if (RndGroup *diffs = mDir->Find<RndGroup>("live_diffs.grp", false))
        diffs->SetShowing(!showing);
}
#endif

bool SongSelectPanel::Exiting() const {
    return UIPanel::Exiting() || TheMusicLibrary->IsExiting();
}

void SongSelectPanel::Unload() {
    RELEASE(mLeaderboard);
    unk48 = nullptr;
    TheMusicLibrary->OnUnload();
    UIPanel::Unload();
}

DataNode SongSelectPanel::OnMsg(const ButtonDownMsg &) {
    if (TheMusicLibrary->IsPurchasing()) {
        return 1;
    } else if (TheContentMgr.RefreshInProgress()) {
        static Message msg("set_blocking", 1);
        UIPanel *clp = ObjectDir::Main()->Find<UIPanel>("content_loading_panel", true);
        MILO_ASSERT(clp, 0x6C);
        clp->Handle(msg, true);
        return 1;
    } else
        return DataNode(kDataUnhandled, 0);
}

Leaderboard *SongSelectPanel::GetLeaderboard(
    LocalBandUser *u, ScoreType s, int i, Leaderboard::Mode m
) {
    RELEASE(mLeaderboard);
    switch (TheMusicLibrary->GetHighlightedNode()->GetType()) {
    case kNodeSong:
        mLeaderboard =
            new PlayerSongLeaderboard(TheProfileMgr.GetProfileForUser(u), this, s, i);
        break;
    case kNodeSetlist:
        mLeaderboard =
            new PlayerBattleLeaderboard(TheProfileMgr.GetProfileForUser(u), this, i);
        break;
    default:
        MILO_FAIL("No leaderboard for the highlighted SongNodeType!");
        break;
    }
    MILO_ASSERT(mLeaderboard, 0x8A);
    mLeaderboard->SetMode(m, false);
    mLeaderboard->StartEnumerate();
    return mLeaderboard;
}

void SongSelectPanel::ResultSuccess(bool b1, bool b2, bool b3) {
    static Message success("lb_success", 0, 0, 0);
    success[0] = b1;
    success[1] = b2;
    success[2] = b3;
    HandleType(success);
}

void SongSelectPanel::ResultFailure() {
    static Message lb_failure_msg("lb_failure");
    HandleType(lb_failure_msg);
}

void SongSelectPanel::Poll() {
    HeldButtonPanel::Poll();
    if (mLeaderboard)
        mLeaderboard->Poll();
    if (unk58 >= 0.0f && GetState() == kUp) {
        float diff = TheTaskMgr.UISeconds() - unk58;
        if (!unk54 && diff > unk4c && unk48->IsReady() && unk48->HasRows()) {
            unk58 = TheTaskMgr.UISeconds();
            static Symbol set_mini_leaderboard_showing("set_mini_leaderboard_showing");
            static Message msg(set_mini_leaderboard_showing, 0);
            unk54 = true;
            msg[0] = 1;
            HandleType(msg);
#ifdef HX_NATIVE
            // Online + ready: swap the leaderboard group in (env-alpha anim alone
            // doesn't reveal it natively). Only reached when scores enumerate.
            SetMiniLeaderboardGroupShowing(true);
#endif
        } else if (unk54 && diff > unk50) {
            RestartLeaderboardTimer();
        }
    }
}

void SongSelectPanel::RestartLeaderboardTimer() {
    unk58 = TheTaskMgr.UISeconds();
    static Symbol set_mini_leaderboard_showing("set_mini_leaderboard_showing");
    static Message msg(set_mini_leaderboard_showing, 0);
    unk54 = false;
    msg[0] = 0;
    HandleType(msg);
#ifdef HX_NATIVE
    SetMiniLeaderboardGroupShowing(false);
#endif
}

void SongSelectPanel::CancelLeaderboardTimer() {
    unk58 = -1.0f;
    static Symbol set_mini_leaderboard_showing("set_mini_leaderboard_showing");
    static Message msg(set_mini_leaderboard_showing, 0);
    unk54 = false;
    msg[0] = 0;
    HandleType(msg);
#ifdef HX_NATIVE
    SetMiniLeaderboardGroupShowing(false);
#endif
}

BEGIN_HANDLERS(SongSelectPanel)
    HANDLE_EXPR(
        get_leaderboard,
        GetLeaderboard(
            _msg->Obj<LocalBandUser>(2),
            (ScoreType)_msg->Int(3),
            _msg->Int(4),
            (Leaderboard::Mode)_msg->Int(5)
        )
    )
    HANDLE_ACTION(
        set_to_starting_lb_ix,
        (MILO_ASSERT(mLeaderboard, 0xB6),
         _msg->Obj<UIList>(2)->SetSelected(mLeaderboard->GetStartingRow(), -1))
    )
    HANDLE_ACTION(
        set_leaderboard_mode,
        (MILO_ASSERT(mLeaderboard, 0xB8),
         mLeaderboard->SetMode((Leaderboard::Mode)_msg->Int(2), true))
    )
    HANDLE_ACTION_IF(
        select_lb_row, mLeaderboard,
        mLeaderboard->OnSelectRow(_msg->Int(2), _msg->Obj<BandUser>(3))
    )
    HANDLE_ACTION(restart_leaderboard_timer, RestartLeaderboardTimer())
    HANDLE_ACTION(cancel_leaderboard_timer, CancelLeaderboardTimer())
    HANDLE_EXPR(scroll_lb_up, mLeaderboard && mLeaderboard->EnumerateLowerRankRange())
    HANDLE_EXPR(scroll_lb_down, mLeaderboard && mLeaderboard->EnumerateHigherRankRange())
    HANDLE_MESSAGE(ButtonDownMsg)
    HANDLE_SUPERCLASS(HeldButtonPanel)
    HANDLE_CHECK(0xC5)
END_HANDLERS
// sw2 scatter-include (default/band3/meta_band/SongSelectPanel <- band3/meta_band/SessionUsersProviders.cpp)
#define gRev gRev_SessionUsersProviders
#define gAltRev gAltRev_SessionUsersProviders
#include "band3/meta_band/SessionUsersProviders.cpp"
#undef gRev
#undef gAltRev
