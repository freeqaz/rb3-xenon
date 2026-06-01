#pragma once
#include "obj/Data.h"
#include "obj/ObjMacros.h"
#include "ui/UIComponent.h"
#include "utl/Symbol.h"

// Minimal declaration for rb3-xenon: MusicLibrary only needs the
// StarDisplay-specific SetValues/SetToToken entry points; SetShowing /
// SetProperty come from the UIComponent / Hmx::Object base. The full
// engine class lives in bandobj (not yet ported here); this header is
// declaration-only so MusicLibrary.cpp's dynamic_cast + calls compile.
class StarDisplay : public UIComponent {
public:
    StarDisplay();
    OBJ_CLASSNAME(StarDisplay)
    OBJ_SET_TYPE(StarDisplay)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual ~StarDisplay();

    void SetValues(int, int);
    void SetToToken(Symbol);
};
