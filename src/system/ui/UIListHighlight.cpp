#include "ui/UIListHighlight.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "ui/UIListWidget.h"
#include "ui/UIList.h"

UIListHighlight::UIListHighlight() : mMesh(this) {}

BEGIN_HANDLERS(UIListHighlight)
    HANDLE_SUPERCLASS(UIListWidget)
END_HANDLERS

BEGIN_PROPSYNCS(UIListHighlight)
    SYNC_PROP(mesh, mMesh)
    SYNC_SUPERCLASS(UIListWidget)
END_PROPSYNCS

BEGIN_SAVES(UIListHighlight)
    SAVE_REVS(0, 0)
    SAVE_SUPERCLASS(UIListWidget)
    bs << mMesh;
END_SAVES

BEGIN_COPYS(UIListHighlight)
    COPY_SUPERCLASS(UIListWidget)
    CREATE_COPY_AS(UIListHighlight, h)
    MILO_ASSERT(h, 0x38);
    COPY_MEMBER_FROM(h, mMesh)
END_COPYS

// RB3-360 retail rev dialect (rb3-Wii/ObjMacros shape): the packed rev is split
// into two HALFWORDS stored four bytes apart onto ONE internal-linkage align(4)
// base, and the RAW incoming BinStream is forwarded to every read and to the
// superclass Load.  DC3's Object.h BinStreamRev stack decorator additionally
// emits ??0BinStream, a ??_7BinStreamRev@@6B@ vtable store and a ??1BinStream
// destructor that retail has none of, and dispatches each read on `&d`.
//
// Written longhand rather than by including obj/ObjMacros.h: that header also
// swaps the SYNC_PROP and HANDLE families, which are already byte-exact here.
// The pair MUST share one aggregate -- two separate file statics are laid out
// independently and will not fold onto a single base register.  No `#define
// gRev` alias: several of these TUs are scatter-INCLUDED into another unit
// (e.g. rndobj/Anim.cpp includes rndobj/MotionBlur.cpp) whose own gRev macro
// the alias would silently shadow for the rest of the amalgamated TU.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_UIListHighlight;
BEGIN_LOADS(UIListHighlight)
    int rev;
    bs >> rev;
    gRevs_UIListHighlight.rev = getHmxRev(rev);
    gRevs_UIListHighlight.altRev = getAltRev(rev);
    UIListWidget::Load(bs);
    bs >> mMesh;
END_LOADS

void UIListHighlight::Draw(
    const UIListWidgetDrawState &drawstate,
    const UIListState &liststate,
    const Transform &tf,
    UIComponent::State compstate,
    Box *box,
    DrawCommand cmd
) {
    if (mMesh && cmd != kDrawFirst) {
        Transform tf70 = mMesh->WorldXfm();
        Transform tfb0 = tf70;
        if (ParentList()) {
            ParentList()->AdjustTransSelected(tfb0);
        }
        CalcXfm(tf, drawstate.mHighlightPos, tfb0);
        DrawMesh(mMesh, drawstate.mHighlightElementState, compstate, tfb0, box);
        mMesh->SetWorldXfm(tf70);
    }
}

// ---------------------------------------------------------------------------
// lane-AE batch-3 (sw3) scatter-include: retail placed
//   ??1UIListLabelElement@@UAA@XZ  (128 B)
// inside the .text span pinned to default/UIListHighlight. The definition is
// out-of-line in ui/UIListLabel.cpp:108, so only pulling the owner TU in can
// make our obj define the symbol. gRev/gAltRev come from SAVE_REVS in both TUs.
#define gRev gRev_UIListLabel
#define gAltRev gAltRev_UIListLabel
#include "ui/UIListLabel.cpp"
#undef gRev
#undef gAltRev
