#pragma once
#include "obj/Object.h"
#include "rndobj/Dir.h"
#include "rndobj/Mesh.h"
#include "ui/ResourceDirPtr.h"
#include "ui/UIComponent.h"
#include "ui/UILabel.h"

/** "a mesh shrink wrapped to selected label" */
class LabelShrinkWrapper : public UIComponent {
public:
    // Hmx::Object
    virtual ~LabelShrinkWrapper();
    OBJ_CLASSNAME(LabelShrinkWrapper)
    OBJ_SET_TYPE_ENGINE(LabelShrinkWrapper)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void SetTypeDef(DataArray *);
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    // RndDrawable
    virtual void DrawShowing();
    // RndPollable
    virtual void Poll();
    virtual void Enter();

    NEW_OBJ(LabelShrinkWrapper)
    OBJ_MEM_OVERLOAD(0x14)

    static void Init();

    UILabel *Label() const { return m_pLabel.Ptr() ? m_pLabel.Ptr() : nullptr; }

protected:
    // UIComponent
    UICOMP_DC3_VIRTUAL void OldResourcePreload(BinStream &);
    LabelShrinkWrapper();
    void Update();
    void UpdateAndDrawWrapper();

    // NOTE(laneBS1): a `ResourceDirPtr<RndDir> mResourceDir` (16 bytes, at 0x140) and
    // four `float mLeft/mRight/mTop/mBottomBorder` (16 bytes, at 0x160..0x16c) used to
    // sit here. Retail RB3 has NEITHER -- both are DC3-era additions, and together they
    // are exactly the 32 bytes by which our layout was too large. Confirmed three ways:
    // (1) the compiler layout report put `(vtordisp for vbase Object)` at 384 with them
    // present and at 352 without, and retail's ?SetType@LabelShrinkWrapper@@ (0x82826e50)
    // uses the vbase-displacement immediate -0x160 = 352 where we emitted -0x180 = 384,
    // a uniform -32 across all 12 differing words; (2) the rb3-Wii RB3 oracle
    // (../rb3/src/system/ui/LabelShrinkWrapper.h) declares only m_pLabel, m_pShow and
    // the four bones -- neither the dir ptr nor the borders -- so removing them also
    // restores retail's member ORDER; (3) that oracle's PreLoad is ASSERT_REVS(0,0) and
    // streams only m_pLabel/m_pShow, i.e. retail has no stream field for either, which
    // is why the rev>=1/rev>=2 reads went with them.
    // Like MeterDisplay (lane BQ-2), RB3 reaches the dir through the INHERITED
    // UIComponent::mResource (a UIResource* at 0x108) via mResource->Dir().
    ObjPtr<UILabel> m_pLabel; // 0x140
    bool m_pShow; // 0x14c
    RndMesh *m_pTopLeftBone; // 0x150
    RndMesh *m_pTopRightBone; // 0x154
    RndMesh *m_pBottomLeftBone; // 0x158
    RndMesh *m_pBottomRightBone; // 0x15c
};
