#pragma once
#include "os/JoypadMsgs.h"
#include "rndobj/Mesh.h"
#include "ui/UIComponent.h"
#include "ui/UIList.h"
#include "ui/UIPanel.h"

class MultiSelectListPanel : public UIPanel {
public:
    MultiSelectListPanel();
    OBJ_CLASSNAME(MultiSelectListPanel);
    OBJ_SET_TYPE(MultiSelectListPanel);
    NEW_OBJ(MultiSelectListPanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~MultiSelectListPanel() {}
    virtual void Unload();
    virtual void FinishLoad();

    void ResetSelectRect(int);
    void FakeComponentSelect();
    void FakeComponentScroll();
    void UnChoose();
    DataNode OnMsg(const UIComponentSelectMsg &);
    DataNode OnMsg(const UIComponentScrollMsg &);
    DataNode OnMsg(const ButtonDownMsg &);

    RndMesh *mSelectionMesh; // 0x3c
    UIList *mScrollList; // 0x40
    int mStartSection; // 0x44
    float mSpacing; // 0x48
    float mHeightMultiplier; // 0x4c
    Vector3 mSelectionStart; // 0x50
};