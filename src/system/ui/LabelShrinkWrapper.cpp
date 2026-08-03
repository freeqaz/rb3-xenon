#include "ui/LabelShrinkWrapper.h"
#include "ui/UIComponent.h"
#include "macros.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Mesh.h"
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
    SAVE_REVS(2, 0)
    bs << m_pLabel << m_pShow;
    SAVE_SUPERCLASS(UIComponent)
END_SAVES

void LabelShrinkWrapper::Copy(const Hmx::Object *o, Hmx::Object::CopyType ty) {
    UIComponent::Copy(o, ty);
    const LabelShrinkWrapper *c = dynamic_cast<const LabelShrinkWrapper *>(o);
    if (c) {
        m_pLabel = c->m_pLabel;
        m_pShow = c->m_pShow;
    }
    Update();
}

BEGIN_LOADS(LabelShrinkWrapper)
    PreLoad(bs);
    PostLoad(bs);
END_LOADS

void LabelShrinkWrapper::SetTypeDef(DataArray *d) {
    Hmx::Object::SetTypeDef(d);
    Update();
}

INIT_REVS(2, 0)

void LabelShrinkWrapper::PreLoad(BinStream &bs) {
    LOAD_REVS(bs)
    ASSERT_REVS(2, 0)
    bs >> m_pLabel;
    bs >> m_pShow;
    UIComponent::PreLoad(d.stream);
    d.PushRev(this);
}

void LabelShrinkWrapper::PostLoad(BinStream &bs) {
    bs.PopRev(this);
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
    // (matches rb3-Wii's shape, ../rb3/src/system/ui/LabelShrinkWrapper.cpp:81);
    // the dc3-derived if(pTypeDef && pDir){...}else{clear bones} shape does not
    // exist in retail. MILO_ASSERT no-ops here (HX_NATIVE undefined), so the
    // Wii oracle's asserts generate no code either way.
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

void LabelShrinkWrapper::Init() { REGISTER_OBJ_FACTORY(LabelShrinkWrapper) }

void LabelShrinkWrapper::UpdateAndDrawWrapper() {
    // NOTE(laneBS1): ported from the rb3-Wii RB3 oracle
    // (../rb3/src/system/ui/LabelShrinkWrapper.cpp:49). The previous body derived the
    // corners from RndText bounds plus the four mLeft/Right/Top/BottomBorder floats;
    // retail RB3 has no such members (see the header note), so it cannot be that shape.
    MILO_ASSERT(m_pLabel, 0x86);
    UILabel *label = m_pLabel;
    Vector3 vMin, vMax;
    float w = label->GetDrawWidth();
    float h = label->GetDrawHeight();
    label->InqMinMaxFromWidthAndHeight(w, h, label->Alignment(), vMin, vMax);
    float minX = vMin.x;
    float maxX = vMax.x;
    float maxZ = vMax.z;
    float minZ = vMin.z;
    SetWorldXfm(label->WorldXfm());
    m_pTopLeftBone->SetLocalPos(minX, 0.0f, maxZ);
    m_pTopRightBone->SetLocalPos(maxX, 0.0f, maxZ);
    m_pBottomLeftBone->SetLocalPos(minX, 0.0f, minZ);
    m_pBottomRightBone->SetLocalPos(maxX, 0.0f, minZ);
}
