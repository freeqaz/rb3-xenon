#pragma once
#include "obj/ObjMacros.h"
#include "ui/UIPanel.h"

// The Rock Band Network audition-session screen. Retail registers it from
// MetaPanel::Init (0x82574E20, slot 5: StaticClassName 0x8256E828, NewObject
// 0x8256E8A8); NewObject allocates 0x6c bytes and runs the ctor at 0x826033F8.
// Only the factory surface is declared here: the class body is not decompiled,
// so its single member beyond UIPanel is not identified.
class AuditionSessionPanel : public UIPanel {
public:
    AuditionSessionPanel();
    OBJ_CLASSNAME(AuditionSessionPanel);
    NEW_OBJ(AuditionSessionPanel);

private:
    int unk; // the 4 bytes past UIPanel; meaning unknown
};
