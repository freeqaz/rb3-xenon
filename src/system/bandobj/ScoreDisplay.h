#pragma once
// Minimal stub for ScoreDisplay — included by meta_band/HeaderPerformanceProvider.h.
// Full class body deferred; needs UIComponent, UIListCustomTemplate, BandLabel.
#include "ui/UIComponent.h"
#include "ui/UIList.h"

class ScoreDisplay : public UIComponent, public UIListCustomTemplate {
public:
    ScoreDisplay() {}
    OBJ_CLASSNAME(ScoreDisplay);
    OBJ_SET_TYPE(ScoreDisplay);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~ScoreDisplay() {}
    // UIListCustomTemplate pure virtuals
    virtual void Custom(int, int, UIListCustom *, Hmx::Object *) {}
    NEW_OVERLOAD;
    DELETE_OVERLOAD;
};
