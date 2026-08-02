#include "bandobj/CheckboxDisplay.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Dir.h"
#include "rndobj/Mesh.h"
#include "ui/UI.h"
#include "ui/UIComponent.h"
#include "ui/UIResource.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"

INIT_REVS(CheckboxDisplay)

void CheckboxDisplay::Init() {
    Register();
    TheUI->InitResources("CheckboxDisplay");
}

CheckboxDisplay::CheckboxDisplay() : mChecked(1) {}

CheckboxDisplay::~CheckboxDisplay() {}

BEGIN_COPYS(CheckboxDisplay)
    COPY_SUPERCLASS(UIComponent)
END_COPYS

void CheckboxDisplay::CopyMembers(const UIComponent *o, Hmx::Object::CopyType ty) {
    UIComponent::CopyMembers(o, ty);
    CREATE_COPY_AS(CheckboxDisplay, pDisplay);
    MILO_ASSERT(pDisplay, 0x30);
    COPY_MEMBER_FROM(pDisplay, mChecked);
}

BEGIN_SAVES(CheckboxDisplay)
    SAVE_REVS(0, 0)
    bs << mChecked;
    SAVE_SUPERCLASS(UIComponent)
END_SAVES

BEGIN_LOADS(CheckboxDisplay)
    PreLoad(bs);
    PostLoad(bs);
END_LOADS

void CheckboxDisplay::PreLoad(BinStream &bs) {
    LOAD_REVS(bs);
    ASSERT_REVS(0, 0);
    bs >> mChecked;
    UIComponent::PreLoad(bs);
}

void CheckboxDisplay::PostLoad(BinStream &bs) {
    UIComponent::PostLoad(bs);
    Update();
}

void CheckboxDisplay::Enter() { UIComponent::Enter(); }
void CheckboxDisplay::SetChecked(bool b) { mChecked = b; }

void CheckboxDisplay::DrawShowing() {
    RndDir *pDir = mResource->Dir();
    MILO_ASSERT(pDir, 0x6E);
    mCheckMesh->SetShowing(mChecked);
    pDir->SetWorldXfm(WorldXfm());
    pDir->Draw();
}

void CheckboxDisplay::Update() {
    UIComponent::Update();
    const DataArray *pTypeDef = TypeDef();
    MILO_ASSERT(pTypeDef, 0x7C);
    RndDir *pDir = mResource->Dir();
    MILO_ASSERT(pDir, 0x7F);
    static Symbol check_mesh("check_mesh");
    mCheckMesh = pDir->Find<RndMesh>(pTypeDef->FindStr(check_mesh), true);
}

BEGIN_HANDLERS(CheckboxDisplay)
    HANDLE_SUPERCLASS(UIComponent)
    HANDLE_CHECK(0x87)
END_HANDLERS

BEGIN_PROPSYNCS(CheckboxDisplay)
    SYNC_PROP_SET(checked, mChecked, SetChecked(_val.Int()))
    SYNC_SUPERCLASS(UIComponent)
END_PROPSYNCS
