#pragma once
#include "obj/Object.h"
#include "rndobj/Anim.h"
#include "rndobj/Mesh.h"
#include "ui/UIListWidget.h"
#include "utl/MemMgr.h"

enum UIListArrowPosition {
    kUIListArrowBack,
    kUIListArrowNext
};

/**
 * @brief Selector icon for UIList.
 * Original _objects description:
 * "Arrow widget for use with UIList"
 */
class UIListArrow : public UIListWidget {
public:
    OBJ_CLASSNAME(UIListArrow);
    OBJ_SET_TYPE_ENGINE(UIListArrow);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void Draw(
        const UIListWidgetDrawState &,
        const UIListState &,
        const Transform &,
        UIComponent::State,
        Box *,
        DrawCommand
    );
    void StartScroll(int, bool);

    NEW_OBJ(UIListArrow)
    OBJ_MEM_OVERLOAD(0x18)

protected:
    UIListArrow();

    /** "arrow mesh to draw/transform" */
    ObjPtr<RndMesh> mMesh; // 0x50
    /** "animation to play on scroll" */
    ObjPtr<RndAnimatable> mScrollAnim; // 0x5c
    /** "whether to position relative to first or last element" */
    UIListArrowPosition mPosition; // 0x68
    /** "show only when list is scrollable" */
    bool mShowOnlyScroll; // 0x6c
    /** "position arrow relative to higlight" */
    bool mOnHighlight; // 0x6d
};
