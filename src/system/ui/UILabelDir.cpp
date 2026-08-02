#include "ui/UILabelDir.h"
#include "UIColor.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Dir.h"
#include "rndobj/Font.h"
#include "rndobj/FontBase.h"
#include "ui/UIComponent.h"
#include "ui/UIFontImporter.h"
#include "utl/BinStream.h"
#include "utl/Str.h"
#include "utl/Symbol.h"

UIColor *gColor = nullptr;

UILabelDir::UILabelDir()
    : mDefaultColor(this), mTextObj(this), mFocusAnim(this), mPulseAnim(this),
      mTopLeftHighlightBone(this), mTopRightHighlightBone(this),
      mBottomLeftHighlightBone(this), mBottomRightHighlightBone(this),
      mHighlightMeshGroup(this), mFocusedBackgroundGroup(this),
      mUnfocusedBackgroundGroup(this), mAllowEditText(false) {
    for (int i = 0; i < UIComponent::kNumStates; i++) {
        mColors.push_back(ObjPtr<UIColor>(this));
    }
}

BEGIN_HANDLERS(UILabelDir)
    HANDLE_EXPR(font_obj, FontObj(_msg->Sym(2)))
    HANDLE_SUPERCLASS(UIFontImporter)
    HANDLE_SUPERCLASS(RndDir)
END_HANDLERS

BEGIN_PROPSYNCS(UILabelDir)
    SYNC_PROP(text_obj, mTextObj)
    SYNC_PROP(allow_edit_text, mAllowEditText)
    SYNC_PROP(focus_anim, mFocusAnim)
    SYNC_PROP(pulse_anim, mPulseAnim)
    SYNC_PROP(highlight_mesh_group, mHighlightMeshGroup)
    SYNC_PROP(top_left_highlight_bone, mTopLeftHighlightBone)
    SYNC_PROP(top_right_highlight_bone, mTopRightHighlightBone)
    SYNC_PROP(bottom_left_highlight_bone, mBottomLeftHighlightBone)
    SYNC_PROP(bottom_right_highlight_bone, mBottomRightHighlightBone)
    SYNC_PROP(focused_background_group, mFocusedBackgroundGroup)
    SYNC_PROP(unfocused_background_group, mUnfocusedBackgroundGroup)
    SYNC_PROP(default_color, mDefaultColor)
    SYNC_PROP_SET(
        normal_color,
        (Hmx::Object *)mColors[UIComponent::kNormal],
        mColors[UIComponent::kNormal] = _val.Obj<UIColor>()
    )
    SYNC_PROP_SET(
        focused_color,
        (Hmx::Object *)mColors[UIComponent::kFocused],
        mColors[UIComponent::kFocused] = _val.Obj<UIColor>()
    )
    SYNC_PROP_SET(
        disabled_color,
        (Hmx::Object *)mColors[UIComponent::kDisabled],
        mColors[UIComponent::kDisabled] = _val.Obj<UIColor>()
    )
    SYNC_PROP_SET(
        selecting_color,
        (Hmx::Object *)mColors[UIComponent::kSelecting],
        mColors[UIComponent::kSelecting] = _val.Obj<UIColor>()
    )
    SYNC_PROP_SET(
        selected_color,
        (Hmx::Object *)mColors[UIComponent::kSelected],
        mColors[UIComponent::kSelected] = _val.Obj<UIColor>()
    )
    SYNC_SUPERCLASS(UIFontImporter)
    SYNC_SUPERCLASS(RndDir)
END_PROPSYNCS

BEGIN_SAVES(UILabelDir)
    SAVE_REVS(9, 0)
    SAVE_SUPERCLASS(RndDir)
    bs << mTextObj;
    bs << mFocusAnim;
    bs << mPulseAnim;
    bs << mHighlightMeshGroup;
    bs << mTopLeftHighlightBone;
    bs << mTopRightHighlightBone;
    bs << mBottomLeftHighlightBone;
    bs << mBottomRightHighlightBone;
    bs << mFocusedBackgroundGroup;
    bs << mUnfocusedBackgroundGroup;
    bs << mAllowEditText;
    bs << mDefaultColor;
    for (int i = 0; i < UIComponent::kNumStates; i++) {
        bs << mColors[i];
    }
    SAVE_SUPERCLASS(UIFontImporter)
END_SAVES

BEGIN_COPYS(UILabelDir)
    COPY_SUPERCLASS(RndDir)
    COPY_SUPERCLASS(UIFontImporter)
    CREATE_COPY(UILabelDir)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mDefaultColor)
        COPY_MEMBER(mColors)
        COPY_MEMBER(mAllowEditText)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_LOADS(UILabelDir)
    ObjectDir::Load(bs);
END_LOADS

INIT_REVS(9, 0)

void UILabelDir::PreLoad(BinStream &bs) {
    LOAD_REVS(bs);
    ASSERT_REVS(9, 0);
    RndDir::PreLoad(d.stream);
    d.PushRev(this);
}

void UILabelDir::PostLoad(BinStream &bs) {
    BinStreamRev d(bs, bs.PopRev(this));
    RndDir::PostLoad(d.stream);
    d >> mTextObj;
    if (d.rev >= 3 && d.rev <= 8) {
        ObjPtr<RndFont> font(this);
        d >> font;
    }
    if (d.rev >= 1) {
        d >> mFocusAnim;
    }
    if (d.rev >= 2) {
        d >> mPulseAnim;
    }
    if (d.rev >= 4) {
        d >> mHighlightMeshGroup;
        d >> mTopLeftHighlightBone;
        d >> mTopRightHighlightBone;
    }
    if (d.rev >= 5) {
        d >> mBottomLeftHighlightBone;
        d >> mBottomRightHighlightBone;
    }
    if (d.rev >= 6) {
        d >> mFocusedBackgroundGroup;
        d >> mUnfocusedBackgroundGroup;
    }
    if (d.rev >= 7) {
        d >> mAllowEditText;
    }
    d >> mDefaultColor;
    for (int i = 0; i < UIComponent::kNumStates; i++) {
        ObjPtr<UIColor> color(this);
        d >> color;
        mColors[i] = color;
    }
    if (d.rev >= 8) {
        UIFontImporter::Load(d.stream);
    }
}

bool UILabelDir::AllowEditText() const { return mAllowEditText; }

RndFont *UILabelDir::FontObj(Symbol s) const {
    if (mGennedFonts.size() > 0) {
        return GetGennedFont(s);
    } else {
        MILO_NOTIFY("%s has no genned fonts", PathName(this));
        return nullptr;
    }
}

UIColor *UILabelDir::GetStateColor(UIComponent::State state) const {
    MILO_ASSERT(state < UIComponent::kNumStates, 0x39);
    UIColor *color = mColors[state];
    if (!color) {
        color = mDefaultColor;
        if (!mDefaultColor) {
            color = gColor;
        }
    }
    return color;
}

void UILabelDir::Init() {
    REGISTER_OBJ_FACTORY(UILabelDir);
    gColor = Hmx::Object::New<UIColor>();
    gColor->SetColor(Hmx::Color(1, 1, 1, 1));
}

DataNode UILabelDir::GetMatVariations(UILabelDir *dir) {
    int i3 = 0;
    if (dir) {
        i3 = dir->mMatVariations.size();
    }
    DataArray *arr = new DataArray(i3 + 1);
    arr->Node(0) = Symbol();
    for (int i = 1; i <= i3; i++) {
        arr->Node(i) = dir->GetMatVariationName(i - 1);
    }
    DataNode ret(arr);
    arr->Release();
    return ret;
}
