#include "ui/UIListArrow.h"
#include "math/Easing.h"
#include "obj/Object.h"
#include "ui/UIList.h"
#include "ui/UIListWidget.h"
#include "ui/UIList.h"

UIListArrow::UIListArrow()
    : mMesh(this), mScrollAnim(this), mPosition(kUIListArrowBack), mShowOnlyScroll(0),
      mOnHighlight(0) {}

BEGIN_HANDLERS(UIListArrow)
    HANDLE_SUPERCLASS(UIListWidget)
END_HANDLERS

BEGIN_PROPSYNCS(UIListArrow)
    SYNC_PROP(mesh, mMesh)
    SYNC_PROP(scroll_anim, mScrollAnim)
    SYNC_PROP_SET(position, mPosition, mPosition = (UIListArrowPosition)_val.Int())
    SYNC_PROP(show_only_scroll, mShowOnlyScroll)
    SYNC_PROP(on_highlight, mOnHighlight)
    SYNC_SUPERCLASS(UIListWidget)
END_PROPSYNCS

BEGIN_SAVES(UIListArrow)
    SAVE_REVS(1, 0)
    SAVE_SUPERCLASS(UIListWidget)
    bs << mMesh << mPosition << mShowOnlyScroll << mOnHighlight << mScrollAnim;
END_SAVES

BEGIN_COPYS(UIListArrow)
    COPY_SUPERCLASS(UIListWidget)
    CREATE_COPY_AS(UIListArrow, a);
    MILO_ASSERT(a, 0x5C);
    COPY_MEMBER_FROM(a, mMesh)
    COPY_MEMBER_FROM(a, mPosition)
    COPY_MEMBER_FROM(a, mShowOnlyScroll)
    COPY_MEMBER_FROM(a, mOnHighlight)
    COPY_MEMBER_FROM(a, mScrollAnim)
END_COPYS

INIT_REVS(1, 0)

BEGIN_LOADS(UIListArrow)
    LOAD_REVS(bs)
    ASSERT_REVS(1, 0)
    LOAD_SUPERCLASS(UIListWidget)
    int pos;
    bs >> mMesh;
    bs >> pos;
    bs >> mShowOnlyScroll >> mOnHighlight;
    mPosition = (UIListArrowPosition)pos;
    if (d.rev > 0) {
        bs >> mScrollAnim;
    }
END_LOADS

void UIListArrow::Draw(
    const UIListWidgetDrawState &drawState,
    const UIListState &listState,
    const Transform &tf,
    UIComponent::State uiState,
    Box *box,
    DrawCommand cmd
) {
    if (mMesh && cmd != kDrawFirst) {
        const Vector3 &vptr = mOnHighlight  ? drawState.mHighlightPos
            : mPosition == kUIListArrowBack ? drawState.mFirstPos
                                            : drawState.mLastPos;
        if (
            box || !mShowOnlyScroll
            || ((mPosition != kUIListArrowBack || listState.CanScrollBack(mOnHighlight))
                && (mPosition != kUIListArrowNext
                    || listState.CanScrollNext(mOnHighlight)))

        ) {
            Transform tf80 = mMesh->WorldXfm();
            Transform tfc0 = tf80;
            if (ParentList()) {
                ParentList()->AdjustTransSelected(tfc0);
            }
            CalcXfm(tf, vptr, tfc0);
            DrawMesh(mMesh, drawState.mHighlightElementState, uiState, tfc0, box);
            mMesh->SetWorldXfm(tf80);
        }
    }
}

void UIListArrow::StartScroll(int i, bool) {
    if (mScrollAnim
        && ((i < 0 && mPosition == kUIListArrowBack)
            || (i > 0 && mPosition == kUIListArrowNext))) {
        mScrollAnim->Animate(0, false, 0, 0, kEaseLinear, 0, 0);
    }
}
