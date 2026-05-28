#pragma once
// Minimal stub for OvershellDir — referenced as pointer member in OvershellSlot.h.
// Full class body deferred; needs PanelDir + BandList dependencies.
#include "ui/PanelDir.h"

class BandList;

class OvershellDir : public PanelDir {
public:
    OvershellDir() {}
    OBJ_CLASSNAME(OvershellDir);
    OBJ_SET_TYPE(OvershellDir);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~OvershellDir() {}
    DECLARE_REVS;
    NEW_OVERLOAD;
    DELETE_OVERLOAD;
};
