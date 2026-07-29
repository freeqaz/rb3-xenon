#include "tour/TourGameRules.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "utl/Symbol.h"

TourGameRules::TourGameRules() : mGameType(), mChallengeData(NULL) {}
TourGameRules::~TourGameRules() {}

void TourGameRules::Init(const DataArray *i_pConfig) {
    // Retail spells these as function-local statics (guard word + one `??__F`
    // funclet each -- three of them at 0x82366098/0xB8/0xD8), at point of use.
    MILO_ASSERT(i_pConfig, 28);
    static Symbol type("type");
    int x = 0;
    i_pConfig->FindData(type, x, true);
    mGameType = (TourGameType)x;
    static Symbol target("target");
    DataArray *pTargetArray = i_pConfig->FindArray(target);
    MILO_ASSERT(pTargetArray, 37);
    MILO_ASSERT(pTargetArray->Size() > 1, 40);
    for (int i = 1; i < pTargetArray->Size(); i++) {
        float f = pTargetArray->Node(i).Float();
        if (m_vTargets.size() < 2)
            m_vTargets.push_back(f);
        else
            MILO_WARN("Too many targets specified for game type: %i", mGameType);
    }
    int i = m_vTargets.size();
    if (i < 2) {
        MILO_WARN("Not enough targets specified for game type: %i", mGameType);
        for (i; i < 2; i++) {
            m_vTargets.push_back(0);
        }
    }
    static Symbol challenge_specific_data("challenge_specific_data");
    mChallengeData = i_pConfig->FindArray(challenge_specific_data, false);
}

TourGameType TourGameRules::GetGameType() const { return mGameType; }
int TourGameRules::GetNumTargets() const { return m_vTargets.size(); }
float TourGameRules::GetTarget(int i_iIndex) const {
    MILO_ASSERT(i_iIndex < m_vTargets.size(), 90);
    return m_vTargets[i_iIndex];
}
const DataArray *TourGameRules::GetChallengeSpecificData() const { return mChallengeData; }
