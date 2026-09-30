#include "tour/Quest.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "tour/TourCondition.h"
#include "tour/TourQuestGameRules.h"
#include "utl/MakeString.h"
#include "utl/Symbol.h"

// Retail TU: 0x8235AE68-0x8235B978 (vtable .?AVQuest@@ at 0x8203CDC4).
// Ported from the rb3-Wii oracle (band3/tour/Quest.cpp). Retail differs from the
// oracle only in that every FindData tag is a function-local static Symbol
// declared at its point of use (one guard word, bits 0x1.. in call order).

Quest::Quest(DataArray *da)
    : mName(""), mDisplayName(gNullStr), mDescription(gNullStr),
      mLongDescription(gNullStr), mIngameDesc(gNullStr), mIntroVignette(""),
      mOutroVignette(""), mSuccess(""), mTier(-1), mGroup(""), mIsSpecial(0), mWeight(1),
      mIsUGCAllowed(1) {
    Configure(da);
}

Quest::~Quest() {}

void Quest::Configure(DataArray *i_pConfig) {
    MILO_ASSERT(i_pConfig, 37);
    mName = i_pConfig->Sym(0);
    static Symbol name_override("name_override");
    i_pConfig->FindData(name_override, mDisplayName, false);
    static Symbol desc_override("desc_override");
    i_pConfig->FindData(desc_override, mDescription, false);
    static Symbol ingamedesc_override("ingamedesc_override");
    i_pConfig->FindData(ingamedesc_override, mIngameDesc, false);
    static Symbol longdesc_override("longdesc_override");
    i_pConfig->FindData(longdesc_override, mLongDescription, false);
    static Symbol tier("tier");
    i_pConfig->FindData(tier, mTier, false);
    static Symbol group("group");
    i_pConfig->FindData(group, mGroup, true);
    static Symbol intro_vignette("intro_vignette");
    i_pConfig->FindData(intro_vignette, mIntroVignette, false);
    static Symbol outro_vignette("outro_vignette");
    i_pConfig->FindData(outro_vignette, mOutroVignette, false);
    static Symbol success_symbol("success_symbol");
    i_pConfig->FindData(success_symbol, mSuccess, false);
    static Symbol is_special("is_special");
    i_pConfig->FindData(is_special, mIsSpecial, false);
    static Symbol weight("weight");
    i_pConfig->FindData(weight, mWeight, false);
    static Symbol allow_ugc("allow_ugc");
    i_pConfig->FindData(allow_ugc, mIsUGCAllowed, false);
    static Symbol prereqs("prereqs");
    mPrerequisites.Init(i_pConfig->FindArray(prereqs, false));
    static Symbol success_reward("success_reward");
    mSuccessReward.Init(i_pConfig->FindArray(success_reward, false));
    static Symbol failure_reward("failure_reward");
    mFailureReward.Init(i_pConfig->FindArray(failure_reward, false));
    static Symbol game_rules("game_rules");
    // retail evaluates FindArray before loading mGameRules' vtable
    DataArray *pGameRules = i_pConfig->FindArray(game_rules);
    mGameRules.Init(pGameRules);
}

Symbol Quest::GetName() const { return mName; }

Symbol Quest::GetDisplayName() const {
    if (mDisplayName != gNullStr)
        return mDisplayName;
    else
        return mName;
}

Symbol Quest::GetLongDescription() const {
    if (mLongDescription != gNullStr)
        return mLongDescription;
    else
        return MakeString("%s_long_desc", mName);
}

Symbol Quest::GetDescription() const {
    if (mDescription != gNullStr)
        return mDescription;
    else
        return MakeString("%s_desc", mName);
}

int Quest::GetTier() const { return mTier; }
float Quest::GetWeight() const { return mWeight; }
const TourCondition *Quest::GetPrereqs() const { return &mPrerequisites; }

bool Quest::HasCustomIntro() const { return mIntroVignette != ""; }
bool Quest::HasCustomOutro() const { return mOutroVignette != ""; }

Symbol Quest::GetCustomIntro() const {
    MILO_ASSERT(HasCustomIntro(), 184);
    return mIntroVignette;
}

Symbol Quest::GetCustomOutro() const {
    MILO_ASSERT(HasCustomOutro(), 192);
    return mOutroVignette;
}

Symbol Quest::GetSuccessSymbol() const { return mSuccess; }
const TourQuestGameRules *Quest::GetGameRules() const { return &mGameRules; }
const TourReward *Quest::GetSuccessReward() const { return &mSuccessReward; }
const TourReward *Quest::GetFailureReward() const { return &mFailureReward; }
Symbol Quest::GetGroup() const { return mGroup; }
bool Quest::IsUGCAllowed() const { return mIsUGCAllowed; }
