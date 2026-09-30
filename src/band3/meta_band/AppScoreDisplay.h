#pragma once
#include "bandobj/ScoreDisplay.h"

class AppScoreDisplay : public ScoreDisplay {
public:
    // implicit ctor: retail stores a literal 0 to the vtordisp slots
    OBJ_CLASSNAME(ScoreDisplay);
    OBJ_SET_TYPE(AppScoreDisplay);
    NEW_OBJ(AppScoreDisplay);
    virtual ~AppScoreDisplay() {}
    virtual void UpdateDisplay();
};