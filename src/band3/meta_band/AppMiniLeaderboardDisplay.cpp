#include "meta_band/AppMiniLeaderboardDisplay.h"
#include "bandobj/BandList.h"
#include "game/Defines.h"
#include "meta_band/AppLabel.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/Utl.h"
#include "net_band/RockCentral.h"
#include "obj/ObjMacros.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "rndobj/EventTrigger.h"
#include "rndobj/Group.h"
#include "ui/UI.h"
#include "ui/UIList.h"
#include "ui/UIListProvider.h"
#include "ui/UIResource.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"

void AppMiniLeaderboardDisplay::Init() {
    REGISTER_OBJ_FACTORY(AppMiniLeaderboardDisplay)
}

AppMiniLeaderboardDisplay::AppMiniLeaderboardDisplay()
    : mStatus(kLeaderboardUnloaded), mLeaderboardList(nullptr), mLeaderboard(nullptr),
      mSongID(0), mScoreType((ScoreType)2), mUpdateTime(0.0f) {}

AppMiniLeaderboardDisplay::~AppMiniLeaderboardDisplay() {
    if (mLeaderboardList) {
        mLeaderboardList->SetProvider(mLeaderboardList);
    }
    if (mLeaderboard) {
        delete mLeaderboard;
    }
    MILO_ASSERT(mLeaderboardList, 0x63);
    mLeaderboardList->SetProvider(mLeaderboardList);
}

void AppMiniLeaderboardDisplay::Poll() {
    UIComponent::Poll();
    if (mSongID != 0 && mStatus == kLeaderboardUnloaded) {
        float t = TheTaskMgr.UISeconds();
        if (mUpdateTime > t) {
            mUpdateTime = t;
        }
        if (t - mUpdateTime >= 1.0f) {
            UpdateLeaderboardOnline(mSongID);
        }
    }
    if (mLeaderboard) {
        mLeaderboard->Poll();
    }
}

void AppMiniLeaderboardDisplay::DrawShowing() {
    RndDir *d = mResource->Dir();
    MILO_ASSERT(d, 0x8d);
    d->SetWorldXfm(WorldXfm());
    d->Draw();
}

void AppMiniLeaderboardDisplay::SetLeaderboardStatus(LeaderboardStatus status) {
    if (status == mStatus)
        return;
    mStatus = status;
    if (mPendingGroup) {
        mPendingGroup->SetShowing(status == kLeaderboardLoading);
    }
    MILO_ASSERT(mLeaderboardList, 0x9e);
    mLeaderboardList->SetShowing(mStatus == kLeaderboardReady);
    switch (mStatus) {
    case kLeaderboardError:
    case kLeaderboardUnloaded:
        if (mFadeOutTrigger)
            mFadeOutTrigger->Trigger();
        break;
    case kLeaderboardReady:
    case kLeaderboardLoading:
        if (mFadeInTrigger)
            mFadeInTrigger->Trigger();
        break;
    }
}

void AppMiniLeaderboardDisplay::UpdateLeaderboardOnline(int songID) {
#ifdef HX_NATIVE
    // Offline (no RockCentral session) there is no server to enumerate friend
    // scores from, so StartEnumerate() would push the leaderboard into the
    // kEnumState2 "waiting on server" state that never completes — leaving the
    // "FRIEND RANKINGS" panel faded-in and stuck over the song list / difficulty
    // grid. On the Wii the offline enumerate fails fast; here we mirror that by
    // failing immediately, which fades the panel out (kLeaderboardError ->
    // mFadeOutTrigger). Online play still takes the real path below.
    if (!TheRockCentral.IsOnline()) {
        SetLeaderboardStatus(kLeaderboardLoading);
        mLeaderboardList->SetProvider(mLeaderboardList);
        delete mLeaderboard;
        mLeaderboard = nullptr;
        ResultFailure();
        return;
    }
#endif
    SetLeaderboardStatus(kLeaderboardLoading);
    BandProfile *p = TheProfileMgr.GetPrimaryProfile();
    mLeaderboardList->SetProvider(mLeaderboardList);
    delete mLeaderboard;
    mLeaderboard = nullptr;
    if (p) {
        MILO_ASSERT(mLeaderboardList, 0xc9);
        mLeaderboard = new PlayerMiniLeaderboard(
            p, this, mScoreType, mSongID, mLeaderboardList->NumDisplay()
        );
        mLeaderboardList->SetProvider(mLeaderboard);
        mLeaderboard->StartEnumerate();
    } else {
        ResultFailure();
    }
}

bool AppMiniLeaderboardDisplay::UpdateLeaderboard(int songID, ScoreType scoreType) {
    if (songID == mSongID && scoreType == mScoreType)
        return 0;
    mScoreType = scoreType;
    mSongID = songID;
    static Symbol mini_leaderboards_title_friends("mini_leaderboards_title_friends");
    if (mTitleLabel) {
        mTitleLabel->SetTextToken(mini_leaderboards_title_friends);
    }
    if (mIconsLabel) {
        mIconsLabel->SetTextToken(gNullStr);
        if (mScoreType == kScoreBand) {
            for (int i = 0; i < 4; i++) {
                mIconsLabel->AppendIcon(*GetFontCharFromTrackType((TrackType)i, 0));
            }
        } else {
            mIconsLabel->AppendIcon(
                *GetFontCharFromTrackType(ScoreTypeToTrackType(mScoreType), 0)
            );
        }
    }
    if (mSongID == 0)
        return 1;
    CancelOldServerRequest();
    mScoreType = scoreType;
    SetLeaderboardStatus(kLeaderboardUnloaded);
    mUpdateTime = TheTaskMgr.UISeconds();
    return 1;
}

bool AppMiniLeaderboardDisplay::IsReady() { return mStatus == kLeaderboardReady; }

bool AppMiniLeaderboardDisplay::HasRows() {
    bool result = false;
    if (mLeaderboard && mLeaderboard->NumData()) {
        result = true;
    }
    return result;
}

void AppMiniLeaderboardDisplay::ResultSuccess(bool, bool, bool) {
    MILO_ASSERT(mLeaderboardList, 0x118);
    mLeaderboardList->Refresh(false);
    int selfRow = mLeaderboard->GetSelfRow();
    if (selfRow >= 0) {
        mLeaderboardList->SetSelected(selfRow, -1);
    } else {
        mLeaderboardList->SetSelected(0, -1);
    }
    SetLeaderboardStatus(kLeaderboardReady);
}

void AppMiniLeaderboardDisplay::ResultFailure() {
    SetLeaderboardStatus(kLeaderboardError);
}

void AppMiniLeaderboardDisplay::Update() {
    UIComponent::Update();
    DataArray *t = const_cast<DataArray *>(TypeDef());
    MILO_ASSERT(t, 0x132);
    ObjectDir *dir = mResource->Dir();
    MILO_ASSERT(dir, 0x135);
    static Symbol leaderboard("leaderboard");
    Symbol leaderboardTag = leaderboard;
    mLeaderboardList = dir->Find<BandList>(t->FindArray(leaderboardTag, true)->Str(1), true);
    static Symbol title_label("title_label");
    Symbol titleTag = title_label;
    const char *titleLabelName = t->FindArray(titleTag, true)->Str(1);
    mTitleLabel = dir->Find<AppLabel>(titleLabelName, false);
    static Symbol icons_label("icons_label");
    Symbol iconsTag = icons_label;
    const char *iconsLabelName = t->FindArray(iconsTag, true)->Str(1);
    mIconsLabel = dir->Find<AppLabel>(iconsLabelName, false);
    static Symbol reset_trigger("reset_trigger");
    Symbol resetTag = reset_trigger;
    const char *resetTriggerName = t->FindArray(resetTag, true)->Str(1);
    mResetTrigger = dir->Find<EventTrigger>(resetTriggerName, false);
    static Symbol fade_in_trigger("fade_in_trigger");
    Symbol fadeInTag = fade_in_trigger;
    const char *fadeInTriggerName = t->FindArray(fadeInTag, true)->Str(1);
    mFadeInTrigger = dir->Find<EventTrigger>(fadeInTriggerName, false);
    static Symbol fade_out_trigger("fade_out_trigger");
    Symbol fadeOutTag = fade_out_trigger;
    const char *fadeOutTriggerName = t->FindArray(fadeOutTag, true)->Str(1);
    mFadeOutTrigger = dir->Find<EventTrigger>(fadeOutTriggerName, false);
    static Symbol pending_group("pending_group");
    Symbol pendingTag = pending_group;
    const char *pendingGroupName = t->FindArray(pendingTag, true)->Str(1);
    mPendingGroup = dir->Find<RndGroup>(pendingGroupName, false);
    if (mResetTrigger) {
        mResetTrigger->Trigger();
    }
}

void AppMiniLeaderboardDisplay::Exit() {
    UIComponent::Exit();
    CancelOldServerRequest();
    mSongID = 0;
    mUpdateTime = 0.0f;
}

void AppMiniLeaderboardDisplay::CancelOldServerRequest() {
    if (mLeaderboard) {
        mLeaderboard->CancelEnumerate();
    }
}

BEGIN_HANDLERS(AppMiniLeaderboardDisplay)
    HANDLE_ACTION(fade_in, mFadeInTrigger ? mFadeInTrigger->Trigger() : (void)0)
    HANDLE_ACTION(fade_out, mFadeOutTrigger ? mFadeOutTrigger->Trigger() : (void)0)
    HANDLE_EXPR(update_leaderboard, UpdateLeaderboard(_msg->Int(2), (ScoreType)_msg->Int(3)))
    HANDLE_SUPERCLASS(MiniLeaderboardDisplay)
    HANDLE_CHECK(0x16c)
END_HANDLERS

bool Leaderboard::IsEnumComplete() const { return mEnumState == kEnumDone; }

bool Leaderboard::ShowsDifficultyAndPct() const { return false; }

void PlayerMiniLeaderboard::EnumerateFromID() {
    mDataResultList.Clear();
    std::vector<int> ids;
    GetPlayerIds(ids);
    TheRockCentral.GetLeaderboardByPlayer(
        ids, mSongID, mScoreType, kSong, (LeaderboardMode)kFriends, mNotesPct,
        mDataResultList, this
    );
}
