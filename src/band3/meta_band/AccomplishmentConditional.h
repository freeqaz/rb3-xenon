#pragma once
#include "Accomplishment.h"
#include <list>

struct AccomplishmentCondition {
    Symbol mCondition; // 0x00
    int mValue; // 0x04
    ScoreType mScoreType; // 0x08
    Difficulty mDifficulty; // 0x0c
};

class AccomplishmentConditional : public Accomplishment {
public:
    AccomplishmentConditional(DataArray *, int);
    virtual ~AccomplishmentConditional();
    void UpdateConditionOptionalData(AccomplishmentCondition &, DataArray *);
    void Configure(DataArray *);
    virtual bool CanBeLaunched() const;
    virtual bool InqRequiredScoreTypes(std::set<ScoreType> &) const;
    virtual Difficulty GetRequiredDifficulty() const;

    std::list<AccomplishmentCondition> m_lConditions; // 0x90
};
