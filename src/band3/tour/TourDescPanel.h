#pragma once
#include "meta_band/TexLoadPanel.h"
#include "obj/ObjMacros.h"
#include "utl/Symbol.h"

class TourDescProvider;
class UIComponent;

// Declared here, not in TourDescPanel.cpp, because meta_band/MetaPanel.cpp
// needs the class to register its factory and used to carry its own partial
// declaration: same size, but without the Load/Enter/Unload/FinishLoad/
// SetType/Handle overrides, so the two TUs compiled TourDescPanel with
// different vftables (tools/layout_odr.py).  sizeof is 0x84, the size
// retail's TourDescPanel::NewObject allocates.
class TourDescPanel : public TexLoadPanel {
public:
    TourDescPanel();
    OBJ_CLASSNAME(TourDescPanel);
    OBJ_SET_TYPE(TourDescPanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual void Load();
    virtual void FinishLoad();
    virtual void Enter();
    virtual void Unload();

    NEW_OBJ(TourDescPanel);

    Symbol GetSelectedTourDesc(UIComponent *);
    void LoadIcons();
    void Refresh();
    bool IsTourAvailable();
    Symbol GetInitiallySelectedTour();
    void ClearInitiallySelectedTour();
    void SelectDefaultTour();
    void SelectTour(Symbol);
    void CheatWinTour();
    TourDescProvider *m_pTourDescProvider;
};
