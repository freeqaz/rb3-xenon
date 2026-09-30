#include "ui/UIButton.h"
#include "obj/Object.h"
#include "ui/UI.h"
#include "ui/UILabel.h"
#include "utl/BinStream.h"

void UIButton::Load(BinStream &bs) {
    PreLoad(bs);
    PostLoad(bs);
}

void UIButton::PostLoad(BinStream &bs) { UILabel::PostLoad(bs); }

BEGIN_COPYS(UIButton)
    CREATE_COPY_AS(UIButton, f);
    MILO_ASSERT(f, 0x25);
    COPY_SUPERCLASS(UILabel)
END_COPYS

void UIButton::Save(BinStream &bs) {
    bs << 0;
    SAVE_SUPERCLASS(UILabel)
}

// RB3 retail (0x8280F888) keeps no BinStreamRev here: the packed rev is split
// into two mutable TU shorts (alt at +0, rev at +4), no guard, no Push/PopRev --
// the ui/LabelShrinkWrapper.cpp dialect.
#pragma push_macro("INIT_REVS")
#pragma push_macro("LOAD_REVS")
#pragma push_macro("ASSERT_REVS")
#undef INIT_REVS
#undef LOAD_REVS
#undef ASSERT_REVS
#define INIT_REVS(rev, alt)                                                              \
    static unsigned short gAltRev = alt;                                                 \
    static unsigned short gRev = rev;
#define LOAD_REVS(bs)                                                                    \
    int rev;                                                                             \
    bs >> rev;                                                                           \
    gRev = getHmxRev(rev);                                                               \
    gAltRev = getAltRev(rev);
#define ASSERT_REVS(rev1, rev2)

INIT_REVS(0, 0)

void UIButton::PreLoad(BinStream &bs) {
    LOAD_REVS(bs)
    ASSERT_REVS(0, 0)
    UILabel::PreLoad(bs);
}

#pragma pop_macro("ASSERT_REVS")
#pragma pop_macro("LOAD_REVS")
#pragma pop_macro("INIT_REVS")

BEGIN_PROPSYNCS(UIButton)
    SYNC_SUPERCLASS(UILabel)
END_PROPSYNCS

DataNode UIButton::OnMsg(const ButtonDownMsg &msg) {
    if (msg.GetAction() == kAction_Confirm && GetState() == UIComponent::kFocused) {
        SendSelect(msg.GetUser());
        return 1;
    }
    return DATA_UNHANDLED;
}

BEGIN_HANDLERS(UIButton)
    HANDLE_MESSAGE(ButtonDownMsg)
    HANDLE_SUPERCLASS(UILabel)
END_HANDLERS

UIButton::UIButton() {}

void UIButton::Init() {
    TheUI->InitResources("UIButton");
    REGISTER_OBJ_FACTORY(UIButton)
}
