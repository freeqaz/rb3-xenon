// Ported from rb3-Wii src/system/bandobj/DialogDisplay.cpp (MWCC -> MSVC X360).
#include "bandobj/DialogDisplay.h"
#include "ui/UI.h"
#include "utl/Symbols.h"

INIT_REVS(DialogDisplay);

void DialogDisplay::Init() {
    Register();
    TheUI->InitResources("DialogDisplay");
}

DialogDisplay::DialogDisplay()
    : mDialogLabel(this, 0), mTopBone(this, 0), mBottomBone(this, 0) {}

DialogDisplay::~DialogDisplay() {}

BEGIN_COPYS(DialogDisplay)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY_AS(DialogDisplay, pDisplay)
    MILO_ASSERT(pDisplay, 0x2D);
    COPY_MEMBER_FROM(pDisplay, mDialogLabel)
    COPY_MEMBER_FROM(pDisplay, mTopBone)
    COPY_MEMBER_FROM(pDisplay, mBottomBone)
END_COPYS

SAVE_OBJ(DialogDisplay, 0x3B)

BEGIN_LOADS(DialogDisplay)
    LOAD_REVS(bs)
    ASSERT_REVS(0, 0)
    bs >> mDialogLabel;
    bs >> mTopBone;
    bs >> mBottomBone;
    LOAD_SUPERCLASS(Hmx::Object)
END_LOADS

float DialogDisplay::GetLabelHeight() {
    float height = 0;
    if (mDialogLabel)
        height = mDialogLabel->GetDrawHeight();
    return height;
}

void DialogDisplay::Poll() {
    if (mTopBone && mBottomBone) {
        if (mDialogLabel) {
        float x = mTopBone->LocalXfm().v.x;
        float y = mTopBone->LocalXfm().v.y;
        auto _tmp0 = GetLabelHeight();
        float z = mTopBone->LocalXfm().v.z;
        z -= _tmp0;
        mBottomBone->SetLocalPos(x, y, z);
    }
    }
}

void DialogDisplay::SetLabel(UILabel *lbl) { mDialogLabel = lbl; }
void DialogDisplay::SetTopBone(RndMesh *mesh) { mTopBone = mesh; }
void DialogDisplay::SetBottomBone(RndMesh *mesh) { mBottomBone = mesh; }

BEGIN_HANDLERS(DialogDisplay)
    HANDLE_SUPERCLASS(RndPollable)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x8C)
END_HANDLERS

// Retail's SyncProperty compares against FUNCTION-LOCAL static Symbols (guard word +
// ??__F atexit funclet per prop), not the centralized globals in utl/Symbols*.h --
// same divergence RB3_HANDLE_LOCAL_STATIC fixes for the HANDLE_* family. SYNC_PROP* is
// not covered by that gate, so override it TU-locally (no other TU's codegen moves).
// (lane CT-4: this TU resolves SYNC_PROP_SET to ObjMacros.h:403, the bare-identifier
// form -- WHICH MACRO IS LIVE IS PER-TU AND DEPENDS ON INCLUDE ORDER.)
#undef SYNC_PROP_SET
#define SYNC_PROP_SET(symbol, member, func)                                              \
    {                                                                                    \
        static Symbol _ps(#symbol);                                                      \
        if (sym == _ps) {                                                                \
            if (_op == kPropSet) {                                                       \
                func;                                                                    \
            } else {                                                                     \
                if (_op == (PropOp)0x40)                                                 \
                    return false;                                                        \
                _val = DataNode(member);                                                 \
            }                                                                            \
            return true;                                                                 \
        }                                                                                \
    }

BEGIN_PROPSYNCS(DialogDisplay)
    SYNC_PROP_SET(dialog_label, GetLabel(), SetLabel(_val.Obj<UILabel>()))
    SYNC_PROP_SET(top_bone, GetTopBone(), SetTopBone(_val.Obj<RndMesh>()))
    SYNC_PROP_SET(bottom_bone, GetBottomBone(), SetBottomBone(_val.Obj<RndMesh>()))
    SYNC_SUPERCLASS(RndPollable)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS
