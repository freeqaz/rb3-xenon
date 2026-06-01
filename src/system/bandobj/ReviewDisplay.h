#pragma once
#include "obj/Data.h"
#include "obj/ObjMacros.h"
#include "ui/UIComponent.h"
#include "utl/Symbol.h"

// Minimal declaration for rb3-xenon (see StarDisplay.h note). MusicLibrary
// only uses SetToToken + base SetShowing on a dynamic_cast<ReviewDisplay*>.
class ReviewDisplay : public UIComponent {
public:
    ReviewDisplay();
    OBJ_CLASSNAME(ReviewDisplay)
    OBJ_SET_TYPE(ReviewDisplay)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual ~ReviewDisplay();

    void SetToToken(Symbol);
    void SetValues(int, bool);
};
