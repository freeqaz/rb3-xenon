#include "meta_band/AccomplishmentOneShot.h"
#include "AccomplishmentConditional.h"
#include "bandtrack/TrackPanel.h"
#include "game/Performer.h"
#include "game/Tracker.h"
#include "Campaign.h"
#include "ProfileMgr.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"

AccomplishmentOneShot::AccomplishmentOneShot(DataArray *i_pConfig, int i)
    : AccomplishmentConditional(i_pConfig, i), mOneShotSong(gNullStr),
      mOneShotPlayerMin(0) {
    AccomplishmentOneShot::Configure(i_pConfig);
}

AccomplishmentOneShot::~AccomplishmentOneShot() {}

void AccomplishmentOneShot::Configure(DataArray *i_pConfig) {
    MILO_ASSERT(i_pConfig, 0x23);

    static Symbol oneshot_song("oneshot_song");
    i_pConfig->FindData(oneshot_song, mOneShotSong, false);
    static Symbol oneshot_playermin("oneshot_playermin");
    i_pConfig->FindData(oneshot_playermin, mOneShotPlayerMin, false);
}

AccomplishmentType AccomplishmentOneShot::GetType() const {
    return kAccomplishmentTypeOneShot;
}

bool AccomplishmentOneShot::AreOneShotConditionsMet(
    ScoreType score, Difficulty diff, Performer *i_pPerformer, Symbol s, int i
) {
    MILO_ASSERT(i_pPerformer, 0x3c);
    const Stats &stats = i_pPerformer->GetStats();
    for (std::list<AccomplishmentCondition>::iterator it = m_lConditions.begin();
         it != m_lConditions.end();
         ++it) {
        Symbol sym = it->mCondition;
        int iii = it->mValue;
        if ((mOneShotSong == gNullStr || mOneShotSong == s) && i >= mOneShotPlayerMin
            && diff >= it->mDifficulty) {
            if (sym == stars) {
                if (it->mScoreType == score && i_pPerformer->GetNumStars() >= iii)
                    return true;
            } else if (sym == unison_percent) {
                if (stats.GetUnisonPhrasePercent() >= iii)
                    return true;
            } else if (sym == unison_phrases) {
                if (stats.mUnisonPhraseCount >= iii)
                    return true;
            } else if (sym == upstrum_percent) {
                if (it->mScoreType == score) {
                    int i4 = stats.mHitCount + stats.m0x08;
                    if (i4 > 0)
                        i4 = (float)stats.mUpstrumCount * 100.0f / (float)i4;
                    else
                        i4 = 0;
                    if (i4 >= iii)
                        return true;
                }
            } else if (sym == times_revived) {
                if (stats.mTimesSaved >= iii)
                    return true;
            } else if (sym == saves) {
                if (stats.mPlayersSaved >= iii)
                    return true;
            } else if (sym == awesomes) {
                if ((score - 3 <= 1U) && it->mScoreType == score
                    && stats.mHitCount >= iii)
                    return true;
            } else if (sym == double_awesomes) {
                if (it->mScoreType == score && stats.mDoubleHarmonyHit >= iii)
                    return true;
            } else if (sym == all_double_awesomes) {
                if (it->mScoreType == score
                    && stats.mDoubleHarmonyHit >= stats.mDoubleHarmonyPhraseCount)
                    return true;
            } else if (sym == triple_awesomes) {
                if (it->mScoreType == score && stats.mTripleHarmonyHit >= iii)
                    return true;
            } else if (sym == all_triple_awesomes) {
                if (it->mScoreType == score
                    && stats.mTripleHarmonyHit >= stats.mTripleHarmonyPhraseCount)
                    return true;
            } else if (sym == full_combo) {
                if (it->mScoreType == score && stats.mFullCombo)
                    return true;
            } else {
                MILO_WARN(
                    "GOAL: %s - Condition is not currently supported: %s \n", mName, sym
                );
                return false;
            }
        }
    }
    return false;
}

void AccomplishmentOneShot::InitializeTrackerDesc(TrackerDesc &desc) const {
    // Retail declares the whole condition-name set as function-local statics at
    // the top of this function (9 guard bits in one word at 0x82DFFFAC), even
    // though only the first three are read below: `Symbol(const char*)` interns
    // into the global symbol table, so the ctor has side effects and MSVC cannot
    // elide the guarded init of an otherwise-unread static.
    static Symbol stars("stars");
    static Symbol unison_phrases("unison_phrases");
    static Symbol upstrum_percent("upstrum_percent");
    static Symbol times_revived("times_revived");
    static Symbol saves("saves");
    static Symbol awesomes("awesomes");
    static Symbol double_awesomes("double_awesomes");
    static Symbol triple_awesomes("triple_awesomes");
    static Symbol full_combo("full_combo");
    Accomplishment::InitializeTrackerDesc(desc);
    MILO_ASSERT(!m_lConditions.empty(), 0xe6);
    const AccomplishmentCondition &condition = m_lConditions.front();
    MILO_ASSERT(TheCampaign, 0xe9);
    LocalBandUser *pUser = TheCampaign->GetUser();
    MILO_ASSERT(pUser, 0xeb);
    Profile *pProfile = TheProfileMgr.GetProfileForUser(pUser);
    MILO_ASSERT(pProfile, 0xee);

    Symbol cond = condition.mCondition;
    if (cond == upstrum_percent) {
        desc.mType = kTrackerType_UpstrumPercent;
        desc.unk18.push_back(condition.mValue);
    } else if (cond == stars)
        desc.unkc = TrackPanel::kConfigScoreStars;
    else if (cond == unison_phrases) {
        desc.mType = kTrackerType_UnisonCount;
        desc.unk18.push_back(condition.mValue);
    }
}

bool AccomplishmentOneShot::HasSpecificSongsToLaunch() const {
    return mOneShotSong != gNullStr;
}
