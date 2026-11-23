#pragma once
#include "types.h"
#include "obj/Data.h"
#include <vector>

// Values and names from config/macros.dta (kTourGameType_*); the tour data stores
// these numbers in each quest's game rules.
enum TourGameType {
    kTourGameType_Undef = 0,
    kTourGameType_Accuracy = 1,
    kTourGameType_AccuracyFocus = 2,
    kTourGameType_DeployCount = 3,
    kTourGameType_HopoCount = 4,
    kTourGameType_Stars = 5,
    kTourGameType_OverdriveTimer = 6,
    kTourGameType_Streak = 7,
    kTourGameType_StreakFocus = 8,
    kTourGameType_PerfectOverdrive = 9,
    kTourGameType_PerfectSection = 10,
    kTourGameType_UnisonCount = 11,
    kTourGameType_UpstrumCount = 12
};

class TourGameRules {
public:
    TourGameRules();
    virtual ~TourGameRules();
    virtual void Init(const DataArray *);

    TourGameType mGameType;
    TourGameType GetGameType() const;
    std::vector<float> m_vTargets;
    int GetNumTargets() const;
    float GetTarget(int) const;
    DataArray *mChallengeData;
    const DataArray *GetChallengeSpecificData() const;
};
