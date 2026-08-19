#pragma once
#include "UIComponent.h"
#include "obj/Object.h"
#include "rndobj/Anim.h"
#include "rndobj/Dir.h"
#include "rndobj/FontBase.h"
#include "rndobj/Group.h"
#include "rndobj/Mesh.h"
#include "rndobj/Text.h"
#include "ui/UIColor.h"
#include "ui/UIFontImporter.h"
#include "utl/MemMgr.h"
#include "utl/Symbol.h"

/** "Top-level resource object for UILabels" */
class UILabelDir : public RndDir, public UIFontImporter {
public:
    // Hmx::Object
    OBJ_CLASSNAME(UILabelDir);
    OBJ_SET_TYPE(UILabelDir);
    NEW_OBJ(UILabelDir);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);

    OBJ_MEM_OVERLOAD(0x19);

    bool AllowEditText() const;
    RndFont *FontObj(Symbol) const;
    UIColor *GetStateColor(UIComponent::State) const;
    // ------------------------------------------------------------------
    // RB3 retail UILabelDir API (rb3-Wii oracle). DECLARATION-ONLY, non-virtual:
    // layout- and vtable-neutral; they let ui/UILabel.cpp compile in the RB3
    // shape. Bodies land when UILabelDir itself is ported.
    // ------------------------------------------------------------------
    RndText *TextObj(Symbol) const;
    // Retail keeps both of these OUT-OF-LINE: BandButton::Update calls them
    // through `bl` (retail 0x8280FD68 / 0x8280FD70), both inside this unit's
    // pinned .text span, rather than folding the member load into the caller.
    // So they must NOT be given inline bodies here.
    RndAnimatable *FocusAnim() const;
    RndAnimatable *PulseAnim() const;
    void GetStateColor(UIComponent::State, Hmx::Color &) const;
    RndGroup *HighlighMeshGroup() const;
    RndMesh *TopLeftHighlightBone() const;
    RndMesh *TopRightHighlightBone() const;
    RndMesh *BottomLeftHighlightBone() const;
    RndMesh *BottomRightHighlightBone() const;
    static DataNode GetMatVariations(UILabelDir *);
    static void Init();

protected:
    UILabelDir();

    /** "color to use when no other color is defined for a state" */
    ObjPtr<UIColor> mDefaultColor; // 0x2f0
    /** The colors to use depending on the label's state.
     * This vector is expected to be of size UIComponent::kNumStates.
     * Original _objects descriptions:
     * "color when label is normal"
     * "color when label is focused"
     * "color when label is disabled"
     * "color when label is selecting"
     * "color when label is selected"
     */
    std::vector<ObjPtr<UIColor> > mColors; // 0x304
    ObjPtr<RndText> mTextObj;
    ObjPtr<RndAnimatable> mFocusAnim;
    ObjPtr<RndAnimatable> mPulseAnim;
    ObjPtr<RndMesh> mTopLeftHighlightBone;
    ObjPtr<RndMesh> mTopRightHighlightBone;
    ObjPtr<RndMesh> mBottomLeftHighlightBone;
    ObjPtr<RndMesh> mBottomRightHighlightBone;
    ObjPtr<RndGroup> mHighlightMeshGroup;
    ObjPtr<RndGroup> mFocusedBackgroundGroup;
    ObjPtr<RndGroup> mUnfocusedBackgroundGroup;
    /** "allow non-localized text with this resource?" */
    bool mAllowEditText; // 0x350
};
