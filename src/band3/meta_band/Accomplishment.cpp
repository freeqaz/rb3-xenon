#include "meta_band/Accomplishment.h"
#include "bandtrack/TrackPanel.h"
#include "game/Tracker.h"
#include "obj/Data.h"
#include "utl/MakeString.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"
#include "game/Defines.h"
#include "os/Debug.h"
#include <string.h>
#include "Campaign.h"

#include "decomp.h"

Accomplishment::Accomplishment(DataArray *i_pConfig, int index)
    : mName(gNullStr), mAccomplishmentType(0), mCategory(gNullStr), mAward(gNullStr),
      mUnitsToken(gNullStr), mUnitsTokenSingular(gNullStr), mIconOverride(gNullStr),
      mSecretCampaignLevelPrereq(gNullStr), mScoreType(kScoreBand),
      mLaunchableDifficulty(kDifficultyEasy), mPassiveMsgChannel(gNullStr),
      mPassiveMsgPriority(-1), mRequiresUnison(false), mRequiresBre(false),
      mPlayerCountMin(-1), mPlayerCountMax(-1), mDynamicAlwaysVisible(false),
      mDynamicPrereqsNumSongs(-1), mDynamicPrereqsFilter(gNullStr), mProgressStep(0),
      mGamerpicReward(-1), mAvatarAssetReward(-1), mShouldShowDenominator(true),
      mShowBestAfterEarn(true), mHideProgress(false), mIndex(index), mContextId(-1),
      mMetaScoreValue(gNullStr), mCanBeEarnedWithNoFail(true),
      mIsTrackedInLeaderboard(false) {
    Configure(i_pConfig);
}

Accomplishment::~Accomplishment() {}

void Accomplishment::Configure(DataArray *i_pConfig) {
    MILO_ASSERT(i_pConfig, 0x3e);
    mName = i_pConfig->Sym(0);

    // NOTE (lane W16-FY): every config key below is a FUNCTION-LOCAL
    // `static Symbol`, not one of the pre-interned `Symbols*.h` globals the
    // rb3-Wii DEV oracle uses.  Settled on retail bytes at 0x82594EF8: the
    // body contains 32 inline `Symbol::Symbol(const char*)` calls, each
    // guarded by a distinct bit of ONE packed guard word at 0x82DFEE58
    // (masks 0x1 .. 0x80000000, union 0xFFFFFFFF, popcount 32) -- that packed
    // bitfield is MSVC's local-static guard, and temporaries would need no
    // guard at all.  Order matters: MSVC assigns the guard bits in order of
    // first use, so these must stay in this sequence.
    static Symbol launchable_controller_types("launchable_controller_types");
    DataArray *controllerTypes = i_pConfig->FindArray(launchable_controller_types, false);
    if (controllerTypes != NULL) {
        for (int i = 1; i < controllerTypes->Size(); i++) {
            ControllerType controllerType = (ControllerType)controllerTypes->Int(i);
            mControllerTypes.push_back(controllerType);
        }
    }

    static Symbol launchable_scoretype("launchable_scoretype");
    int scoreType = 0;
    if (i_pConfig->FindData(launchable_scoretype, scoreType, false)) {
        mScoreType = (ScoreType)scoreType;
    }

    static Symbol launchable_difficulty("launchable_difficulty");
    int launchableDifficulty = 0;
    if (i_pConfig->FindData(launchable_difficulty, launchableDifficulty, false)) {
        mLaunchableDifficulty = (Difficulty)launchableDifficulty;
    }

    static Symbol launchable_playercount_min("launchable_playercount_min");
    i_pConfig->FindData(launchable_playercount_min, mPlayerCountMin, false);
    static Symbol launchable_playercount_max("launchable_playercount_max");
    i_pConfig->FindData(launchable_playercount_max, mPlayerCountMax, false);
    static Symbol launchable_requires_unison_ability("launchable_requires_unison_ability");
    i_pConfig->FindData(launchable_requires_unison_ability, mRequiresUnison, false);
    static Symbol launchable_requires_bre_ability("launchable_requires_bre_ability");
    i_pConfig->FindData(launchable_requires_bre_ability, mRequiresBre, false);
    static Symbol secret_campaignlevel_prereq("secret_campaignlevel_prereq");
    i_pConfig->FindData(secret_campaignlevel_prereq, mSecretCampaignLevelPrereq, false);

    static Symbol secret_prereqs("secret_prereqs");
    DataArray *secretPrereqs = i_pConfig->FindArray(secret_prereqs, false);
    if (secretPrereqs != NULL) {
        for (int i = 1; i < secretPrereqs->Size(); i++) {
            Symbol s = secretPrereqs->Sym(i);
            mSecretPrereqs.push_back(s);
        }
    }

    static Symbol dynamic_prereqs("dynamic_prereqs");
    DataArray *dynamicPrereqs = i_pConfig->FindArray(dynamic_prereqs, false);
    if (dynamicPrereqs != NULL) {
        static Symbol num_songs("num_songs");
        dynamicPrereqs->FindData(num_songs, mDynamicPrereqsNumSongs, false);
        static Symbol always_visible("always_visible");
        dynamicPrereqs->FindData(always_visible, mDynamicAlwaysVisible, false);
        static Symbol precached_filter("precached_filter");
        dynamicPrereqs->FindData(precached_filter, mDynamicPrereqsFilter, false);

        static Symbol songs("songs");
        DataArray *songsarr = dynamicPrereqs->FindArray(songs, false);
        if (songsarr != NULL) {
            for (int i = 1; i < songsarr->Size(); i++) {
                Symbol s = songsarr->Sym(i);
                mDynamicPrereqsSongs.push_back(s);
            }
            if (mDynamicPrereqsSongs.size() < mDynamicPrereqsNumSongs) {
                MILO_WARN(
                    "There are less songs in the dynamic prereq song list than the num_songs provided: %s\n",
                    mName.Str()
                );
                mDynamicPrereqsNumSongs = -1;
            }
        }
    }

    static Symbol passive_msg_channel("passive_msg_channel");
    i_pConfig->FindData(passive_msg_channel, mPassiveMsgChannel, false);
    static Symbol passive_msg_priority("passive_msg_priority");
    i_pConfig->FindData(passive_msg_priority, mPassiveMsgPriority, false);

    // NOT a named `bool noMsgChannel` temporary: retail tests the
    // Symbol::operator==(const char*) result directly (`clrlwi.` + `bne`).
    // Materialising the bool costs a non-record clrlwi + cntlzw + extrwi. and
    // inverts the branch (4 charged instructions, measured by lane W16-FY).
    if (mPassiveMsgChannel != gNullStr) {
        if (mPassiveMsgPriority < 1) {
            MILO_WARN(
                "Passive Message Priority for goal %s is less than the minimum: %i!\n",
                mName.Str(),
                1
            );
            mPassiveMsgPriority = 1;
        } else if (1000 < mPassiveMsgPriority) {
            MILO_WARN(
                "Passive Message Priority for goal %s is more than the maximum: %i!\n",
                mName.Str(),
                1000
            );
            mPassiveMsgPriority = 1000;
        }
    }

    static Symbol progress_step("progress_step");
    i_pConfig->FindData(progress_step, mProgressStep, false);
    static Symbol show_best_after_earn("show_best_after_earn");
    i_pConfig->FindData(show_best_after_earn, mShowBestAfterEarn, false);
    static Symbol show_denominator("show_denominator");
    i_pConfig->FindData(show_denominator, mShouldShowDenominator, false);
    static Symbol hide_progress("hide_progress");
    i_pConfig->FindData(hide_progress, mHideProgress, false);
    static Symbol can_be_earned_with_no_fail("can_be_earned_with_no_fail");
    i_pConfig->FindData(can_be_earned_with_no_fail, mCanBeEarnedWithNoFail, false);
    static Symbol leaderboard("leaderboard");
    i_pConfig->FindData(leaderboard, mIsTrackedInLeaderboard, false);
    // Retail-360 only (the rb3-Wii DEV source declares `xlast_id` in Symbols.h
    // but never reads it): the XLAST achievement context id, stored at
    // this+0x84 == mContextId and returned by GetContextID().
    static Symbol xlast_id("xlast_id");
    i_pConfig->FindData(xlast_id, mContextId, false);
    // Retail-360 only (absent from the rb3-Wii DEV source): the two reward ids
    // stored at this+0x74 / this+0x78, read back by
    // AccomplishmentProgress::GiveGamerpic / ::GiveAvatarAsset.  These are NOT
    // FindData lookups -- retail does FindArray + an explicit kDataInt type
    // check on node 1 + DataNode::Int() with the default NULL source.
    static Symbol gamerpic_reward("gamerpic_reward");
    DataArray *gamerpicReward = i_pConfig->FindArray(gamerpic_reward, false);
    if (gamerpicReward != NULL) {
        DataNode &n = gamerpicReward->Node(1);
        if (n.Type() == kDataInt) {
            mGamerpicReward = n.Int();
        }
    }
    static Symbol avatarasset_reward("avatarasset_reward");
    DataArray *avatarAssetReward = i_pConfig->FindArray(avatarasset_reward, false);
    if (avatarAssetReward != NULL) {
        DataNode &n = avatarAssetReward->Node(1);
        if (n.Type() == kDataInt) {
            mAvatarAssetReward = n.Int();
        }
    }
    static Symbol accomplishment_type("accomplishment_type");
    int accomplishmentType;
    i_pConfig->FindData(accomplishment_type, accomplishmentType, true);
    mAccomplishmentType = accomplishmentType;
    static Symbol category("category");
    i_pConfig->FindData(category, mCategory, true);
    static Symbol award("award");
    i_pConfig->FindData(award, mAward, false);
    static Symbol icon_override("icon_override");
    i_pConfig->FindData(icon_override, mIconOverride, false);
    static Symbol units_token("units_token");
    i_pConfig->FindData(units_token, mUnitsToken, false);
    static Symbol units_token_singular("units_token_singular");
    i_pConfig->FindData(units_token_singular, mUnitsTokenSingular, false);
    static Symbol metascore_value("metascore_value");
    i_pConfig->FindData(metascore_value, mMetaScoreValue, true);
}

AccomplishmentType Accomplishment::GetType() const { return kAccomplishmentTypeUnique; }

Symbol Accomplishment::GetName() const { return mName; }

Symbol Accomplishment::GetDescription() const { return MakeString("%s_desc", mName); }

DECOMP_FORCEACTIVE(Accomplishment, "%s_howto")

Symbol Accomplishment::GetSecretDescription() const { return acc_secretdesc; }

Symbol Accomplishment::GetFlavorText() const { return MakeString("%s_flavor", mName); }

bool Accomplishment::GetShouldShowDenominator() const { return mShouldShowDenominator; }

unsigned char Accomplishment::ShowBestAfterEarn() const { return mShowBestAfterEarn; }

bool Accomplishment::HideProgress() const { return mHideProgress; }

Symbol Accomplishment::GetSecretCampaignLevelPrereq() const {
    return mSecretCampaignLevelPrereq;
}

const std::vector<Symbol> &Accomplishment::GetSecretPrereqs() const {
    return mSecretPrereqs;
}

bool Accomplishment::IsDynamic() const {
    bool noFilter = !mDynamicPrereqsSongs.empty();
    if (!noFilter) {
        if (gNullStr) {
            noFilter = !strcmp(mDynamicPrereqsFilter.Str(), gNullStr);
        } else {
            noFilter = (mDynamicPrereqsFilter.Str() == gNullStr);
        }
        noFilter = !noFilter;
    }
    return noFilter;
}

bool Accomplishment::GetDynamicAlwaysVisible() const { return mDynamicAlwaysVisible; }

const std::vector<Symbol> &Accomplishment::GetDynamicPrereqsSongs() const {
    return mDynamicPrereqsSongs;
}

int Accomplishment::GetDynamicPrereqsNumSongs() const { return mDynamicPrereqsNumSongs; }

Symbol Accomplishment::GetDynamicPrereqsFilter() const { return mDynamicPrereqsFilter; }

Symbol Accomplishment::GetCategory() const { return mCategory; }

int Accomplishment::GetContextID() const { return mContextId; }

const char *Accomplishment::GetIconArt() const {
    bool noIconArt;
    if (gNullStr) {
        noIconArt = !strcmp(mIconOverride.Str(), gNullStr);
    } else {
        noIconArt = (mIconOverride.Str() == gNullStr);
    }

    if (!noIconArt) {
        return MakeString(
            "ui/accomplishments/accomplishment_art/%s_keep.png", mIconOverride.Str()
        );
    } else {
        return MakeString(
            "ui/accomplishments/accomplishment_art/%s_keep.png", mName.Str()
        );
    }
}

DECOMP_FORCEACTIVE(Accomplishment, "%s_gray")

bool Accomplishment::IsFulfilled(BandProfile *) const { return false; }

bool Accomplishment::IsRelevantForSong(Symbol) const { return false; }

bool Accomplishment::InqProgressValues(BandProfile *, int &, int &) { return false; }

Symbol Accomplishment::GetFirstUnfinishedAccomplishmentEntry(BandProfile *) const {
    return gNullStr;
}

bool Accomplishment::InqIncrementalSymbols(BandProfile *, std::vector<Symbol> &) const {
    return 0;
}

bool Accomplishment::IsSymbolEntryFulfilled(BandProfile *, Symbol) const { return false; }

bool Accomplishment::CanBeLaunched() const {
    if (mName == acc_calibrate) {
        return true;
    }

    if (mName == acc_charactercreate) {
        return true;
    }

    if (mName == acc_bandcreate) {
        return true;
    }

    if (mName == acc_bandlogo) {
        return true;
    }

    if (mName == acc_standins) {
        return true;
    }

    if (mName == acc_joinalabel) {
        return true;
    }

    if (mName == acc_startalabel) {
        return true;
    }

    if (mName == acc_createsetlist) {
        return true;
    }

    if (mName == acc_HMXrecommends) {
        return true;
    }

    if (mName == acc_multiplayersession) {
        return true;
    }

    if (mName == acc_guitartutorial01) {
        return true;
    }

    if (mName == acc_guitartutorial02) {
        return true;
    }

    if (mName == acc_guitartutorial03) {
        return true;
    }

    return false;
}

bool Accomplishment::HasSpecificSongsToLaunch() const { return false; }

Symbol Accomplishment::GetAward() const { return mAward; }

bool Accomplishment::HasAward() const { return !(mAward == ""); }

Symbol Accomplishment::GetMetaScoreValue() const { return mMetaScoreValue; }

const char *Accomplishment::GetIconPath() {
    return "ui/accomplishments/accomplishment_art/%s_keep.png";
}

bool Accomplishment::IsUserOnValidScoreType(LocalBandUser *i_pUser) const {
    bool returnValue = false;
    ControllerType controllerType = (*(BandUser **)i_pUser)->GetControllerType();

    std::set<ScoreType> scoreTypes;

    InqRequiredScoreTypes(scoreTypes);

    if (scoreTypes.empty()) {
        returnValue = true;
    } else {
        std::set<ScoreType>::iterator iterator = scoreTypes.begin();
        while (iterator != scoreTypes.end()) {
            TrackType trackType = ScoreTypeToTrackType(*iterator);
            ControllerType c = TrackTypeToControllerType(trackType);

            if (controllerType == c) {
                returnValue = true;
                break;
            }
            iterator++;
        }
    }
    return returnValue;
}

bool Accomplishment::IsUserOnValidController(LocalBandUser *i_pUser) const {
    MILO_ASSERT(i_pUser, 0x253);
    bool returnValue = false;
    ControllerType controllerType = i_pUser->GetControllerType();
    bool isValid = IsUserOnValidScoreType(i_pUser);
    if (mControllerTypes.empty()) {
        returnValue = true;
    } else {
        for (std::vector<ControllerType>::const_iterator iter = mControllerTypes.begin();
             iter != mControllerTypes.end();
             ++iter) {
            ControllerType type = *iter;
            if (controllerType == type) {
                returnValue = true;
            }
        }
    }
    return (isValid && returnValue);
}

Difficulty Accomplishment::GetRequiredDifficulty() const { return mLaunchableDifficulty; }

ScoreType Accomplishment::GetRequiredScoreType() const {
    std::set<ScoreType> scoreTypes;

    bool hasScoreType = InqRequiredScoreTypes(scoreTypes);
    if ((int)scoreTypes.size() == 1) {
        std::set<ScoreType>::iterator iterator = scoreTypes.begin();
        return *iterator;
    }
    return (ScoreType)10;
}

bool Accomplishment::InqRequiredScoreTypes(std::set<ScoreType> &o_rScoreTypes) const {
    MILO_ASSERT(o_rScoreTypes.empty(), 0x28d);

    if (mScoreType != 10) {
        o_rScoreTypes.insert(mScoreType);
    }

    return !o_rScoreTypes.empty();
}

int Accomplishment::GetRequiredMinPlayers() const { return mPlayerCountMin; }

int Accomplishment::GetRequiredMaxPlayers() const { return mPlayerCountMax; }

bool Accomplishment::GetRequiresUnisonAbility() const { return mRequiresUnison; }

bool Accomplishment::GetRequiresBREAbility() const { return mRequiresBre; }

void Accomplishment::
    InitializeMusicLibraryTask(MusicLibrary::MusicLibraryTask &, BandProfile *) const {}

void Accomplishment::InitializeTrackerDesc(TrackerDesc &trackerDesc) const {
    MILO_ASSERT(TheCampaign, 0x2b8);

    trackerDesc.mUser = TheCampaign->GetLaunchUser();
    Symbol nm = mName;
    trackerDesc.unkc = TrackPanel::kConfigScoreGoal;
    trackerDesc.mName = nm;
}

bool Accomplishment::CanBeEarnedWithNoFail() const { return mCanBeEarnedWithNoFail; }

bool Accomplishment::IsTrackedInLeaderboard() const { return mIsTrackedInLeaderboard; }

Symbol Accomplishment::GetUnitsToken(int useSingular) const {
    bool noToken;
    if (gNullStr) {
        noToken = !strcmp(mUnitsToken.Str(), gNullStr);
    } else {
        noToken = (mUnitsToken.Str() == gNullStr);
    }

    if (!noToken) {
        if (useSingular == 1) {
            bool noSingularToken;
            if (gNullStr) {
                noSingularToken = !strcmp(mUnitsTokenSingular.Str(), gNullStr);
            } else {
                noSingularToken = (mUnitsTokenSingular.Str() == gNullStr);
            }

            if (!noSingularToken) {
                return mUnitsTokenSingular;
            } else {
                return mUnitsToken;
            }
        } else {
            return mUnitsToken;
        }
    } else {
        if (useSingular == 1) {
            return campaign_goalunits_singular_default;
        }
        return campaign_goalunits_default;
    }
}

Symbol Accomplishment::GetPassiveMsgChannel() const { return mPassiveMsgChannel; }

int Accomplishment::GetPassiveMsgPriority() const { return mPassiveMsgPriority; }
int Accomplishment::GetGamerpicReward() const { return mGamerpicReward; }
int Accomplishment::GetAvatarAssetReward() const { return mAvatarAssetReward; }

// sw2 scatter-include (default/Accomplishment <- bandobj/BandCrowdMeter.cpp)
#define gRev gRev_BandCrowdMeter
#define gAltRev gAltRev_BandCrowdMeter
#include "bandobj/BandCrowdMeter.cpp"
#undef gRev
#undef gAltRev
