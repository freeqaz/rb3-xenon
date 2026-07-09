#include "AccomplishmentSetlist.h"

#include "os/Debug.h"
#include "system/utl/Symbols.h"
#include "system/utl/Symbols4.h"

AccomplishmentSetlist::AccomplishmentSetlist(DataArray *i_pConfig, int i)
    : Accomplishment(i_pConfig, i), mSetlist(""), mInstrument((ScoreType)10),
      mDifficulty((Difficulty)0), mMinStars(0) {
    AccomplishmentSetlist::Configure(i_pConfig);
}

AccomplishmentSetlist::~AccomplishmentSetlist() {}

void AccomplishmentSetlist::Configure(DataArray *i_pConfig) {
    MILO_ASSERT(i_pConfig, 0x1f);

    static Symbol s_setlist("setlist");
    i_pConfig->FindData(s_setlist, mSetlist, true);

    static Symbol s_difficulty("difficulty");
    int difficultyVal = 0;
    bool parsed = i_pConfig->FindData(s_difficulty, difficultyVal, false);
    if (parsed) {
        mDifficulty = (Difficulty)difficultyVal;
    }

    static Symbol s_instrument("instrument");
    int instrumentVal = 0;
    parsed = i_pConfig->FindData(s_instrument, instrumentVal, false);
    if (parsed) {
        mInstrument = (ScoreType)instrumentVal;
    }

    static Symbol s_min_stars("min_stars");
    i_pConfig->FindData(s_min_stars, mMinStars, false);
}

AccomplishmentType AccomplishmentSetlist::GetType() const {
    return kAccomplishmentTypeSetlist;
}

bool AccomplishmentSetlist::CanBeLaunched() const { return true; }

bool AccomplishmentSetlist::HasSpecificSongsToLaunch() const { return true; }

Difficulty AccomplishmentSetlist::GetRequiredDifficulty() const { return mDifficulty; }

bool AccomplishmentSetlist::InqRequiredScoreTypes(std::set<ScoreType> &o_rScoreTypes
) const {
    MILO_ASSERT(o_rScoreTypes.empty(), 0x52);

    if (mInstrument != 10) {
        o_rScoreTypes.insert(mInstrument);
    }

    return !o_rScoreTypes.empty();
}

bool AccomplishmentSetlist::CheckRequirements(
    ScoreType scoreType, Difficulty difficulty, int minStars
) {
    if (difficulty < mDifficulty) {
        return false;
    }

    if (scoreType != mInstrument) {
        return false;
    }

    return mMinStars <= minStars;
}