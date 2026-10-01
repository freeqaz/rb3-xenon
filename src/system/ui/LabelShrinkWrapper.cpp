// Retail inlines ObjPtr<T>(owner, ptr) at every mFoo(this) site in this TU.
#define RB3_OBJPTR_INLINE_TWOARG_CTOR
#include "ui/LabelShrinkWrapper.h"
#include "ui/UIComponent.h"
#include "macros.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Mesh.h"
#include "ui/UI.h"
#include "ui/UILabel.h"
#include "ui/UIPanel.h"
#include "ui/UIResource.h" // laneBS1: for UIResource::Dir(); UIComponent.h only fwd-declares it
#include "utl/BinStream.h"
#include "utl/Loader.h"
#include "utl/Symbol.h"

LabelShrinkWrapper::LabelShrinkWrapper()
    : m_pLabel(this), m_pShow(0), m_pTopLeftBone(0), m_pTopRightBone(0),
      m_pBottomLeftBone(0), m_pBottomRightBone(0) {}

LabelShrinkWrapper::~LabelShrinkWrapper() {}

BEGIN_HANDLERS(LabelShrinkWrapper)
    HANDLE_SUPERCLASS(UIComponent)
END_HANDLERS

BEGIN_PROPSYNCS(LabelShrinkWrapper)
    SYNC_PROP_SET(label, Label(), m_pLabel = _val.Obj<UILabel>())
    SYNC_PROP_SET(show, m_pShow, m_pShow = _val.Int())
    SYNC_SUPERCLASS(UIComponent)
END_PROPSYNCS

BEGIN_SAVES(LabelShrinkWrapper)
    SAVE_REVS(0, 0)
    bs << m_pLabel << m_pShow;
    SAVE_SUPERCLASS(UIComponent)
END_SAVES

// NOTE(laneGLM3): retail's Copy is RB3's own shape
// (ui/LabelShrinkWrapper.cpp), NOT the dc3-derived one.
// Three things are settled by the 112-byte retail body, whose 28 instructions
// are exhaustively accounted for by what is written below:
//   (1) UIComponent::Copy runs LAST, not first;
//   (2) it is passed the DERIVED pointer `c`, not the incoming `o` -- retail
//       emits a vbtable lookup (lwz 0x4(c) / lwz 0x4(r11) / add / addi 4) to
//       reach the virtual base Hmx::Object, which converting an already-adjusted
//       `o` would not need;
//   (3) there is NO null test on the cast (retail's `mr r31,r3` has no record
//       bit and no following `beq`) and NO Update() call -- the assert below is
//       codegen-free in the match build, and Update() is still driven from
//       PostLoad and SetTypeDef, so nothing is lost.
BEGIN_COPYS(LabelShrinkWrapper)
    CREATE_COPY_AS(LabelShrinkWrapper, c)
    MILO_ASSERT(c, 0x2F);
    COPY_MEMBER_FROM(c, m_pLabel)
    COPY_MEMBER_FROM(c, m_pShow)
    UIComponent::Copy(c, ty);
END_COPYS

BEGIN_LOADS(LabelShrinkWrapper)
    PreLoad(bs);
    PostLoad(bs);
END_LOADS

// RB3 retail keeps no BinStreamRev here: the packed rev is split into two
// mutable TU shorts (alt at +0, rev at +4), there is no version guard and no
// Push/PopRev -- the same dialect as ui/UIColor.cpp.
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

// RB3 retail 0x828278D0.
void LabelShrinkWrapper::PreLoad(BinStream &bs) {
    LOAD_REVS(bs)
    ASSERT_REVS(0, 0)
    bs >> m_pLabel;
    bs >> m_pShow;
    UIComponent::PreLoad(bs);
}

#pragma pop_macro("ASSERT_REVS")
#pragma pop_macro("LOAD_REVS")
#pragma pop_macro("INIT_REVS")

void LabelShrinkWrapper::PostLoad(BinStream &bs) {
    UIComponent::PostLoad(bs);
    Update();
}

void LabelShrinkWrapper::DrawShowing() {
    if (m_pLabel && m_pShow) {
        RndDir *pDir = mResource->Dir();
        MILO_ASSERT(pDir, 0xa7);
        UpdateAndDrawWrapper();
        pDir->SetWorldXfm(WorldXfm());
        pDir->Draw();
    }
}

void LabelShrinkWrapper::Enter() { UIComponent::Enter(); }

void LabelShrinkWrapper::Poll() { UIComponent::Poll(); }

void LabelShrinkWrapper::Update() {
    // NOTE(laneNCCC-f164): retail's Ghidra decomp calls UIComponent::Update()
    // unconditionally at entry and has NO if/else null-check branch at all
    // (RB3's own shape);
    // the dc3-derived if(pTypeDef && pDir){...}else{clear bones} shape does not
    // exist in retail. MILO_ASSERT no-ops here (HX_NATIVE undefined), so the
    // asserts generate no code either way.
    UIComponent::Update();
    const DataArray *pTypeDef = TypeDef();
    RndDir *pDir = mResource->Dir();
    static Symbol topleft_bone("topleft_bone");
    static Symbol topright_bone("topright_bone");
    static Symbol bottomleft_bone("bottomleft_bone");
    static Symbol bottomright_bone("bottomright_bone");
    m_pTopLeftBone = pDir->Find<RndMesh>(pTypeDef->FindStr(topleft_bone), true);
    MILO_ASSERT(m_pTopLeftBone, 0xc5);
    m_pTopRightBone = pDir->Find<RndMesh>(pTypeDef->FindStr(topright_bone), true);
    MILO_ASSERT(m_pTopRightBone, 0xc7);
    m_pBottomLeftBone = pDir->Find<RndMesh>(pTypeDef->FindStr(bottomleft_bone), true);
    MILO_ASSERT(m_pBottomLeftBone, 0xc9);
    m_pBottomRightBone =
        pDir->Find<RndMesh>(pTypeDef->FindStr(bottomright_bone), true);
    MILO_ASSERT(m_pBottomRightBone, 0xcb);
}

void LabelShrinkWrapper::Init() {
    REGISTER_OBJ_FACTORY(LabelShrinkWrapper)
    TheUI->InitResources("LabelShrinkWrapper");
}

void LabelShrinkWrapper::UpdateAndDrawWrapper() {
    // NOTE(laneBS1): RB3's own body
    // (ui/LabelShrinkWrapper.cpp). The previous body derived the
    // corners from RndText bounds plus the four mLeft/Right/Top/BottomBorder floats;
    // retail RB3 has no such members (see the header note), so it cannot be that shape.
    MILO_ASSERT(m_pLabel, 0x86);
    Vector3 vMin, vMax;
    float w = m_pLabel->GetDrawWidth();
    float h = m_pLabel->GetDrawHeight();
    m_pLabel->InqMinMaxFromWidthAndHeight(w, h, m_pLabel->Alignment(), vMin, vMax);
    float minX = vMin.x;
    float maxX = vMax.x;
    float maxZ = vMax.z;
    float minZ = vMin.z;
    SetWorldXfm(m_pLabel->WorldXfm());
    Vector3 tl(minX, 0.0f, maxZ);
    Vector3 tr(maxX, 0.0f, maxZ);
    Vector3 bl(minX, 0.0f, minZ);
    Vector3 br(maxX, 0.0f, minZ);
    m_pTopLeftBone->SetLocalPos(tl);
    m_pTopRightBone->SetLocalPos(tr);
    m_pBottomLeftBone->SetLocalPos(bl);
    m_pBottomRightBone->SetLocalPos(br);
}
