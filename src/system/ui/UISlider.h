#pragma once
#include "ResourceDirPtr.h"
#include "obj/Object.h"
#include "rndobj/Dir.h"
#include "rndobj/Mat.h"
#include "rndobj/Mesh.h"
#include "ui/UIComponent.h"
#include "ui/ScrollSelect.h"
#include "os/JoypadMsgs.h"
#include "utl/MemMgr.h"

class RndMesh;
class RndMat;

/** "A component with animatable whose frames correspond to a
 *  range of values. The resources don't have to look like a slider;
 *  they could easily be a knob, dial, etc." */
class UISlider : public UIComponent, public ScrollSelect {
public:
    // Hmx::Object
    OBJ_CLASSNAME(UISlider)
    OBJ_SET_TYPE(UISlider)
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
    virtual RndDrawable *CollideShowing(const Segment &, float &, Plane &);
    virtual int CollidePlane(const Plane &);
    // UIComponent
    virtual void Enter();
    virtual void SetCurrent(int);
    // ScrollSelect
    virtual int SelectedAux() const;

    OBJ_MEM_OVERLOAD(0x18);
    NEW_OBJ(UISlider)
    static void Init();

    float Frame() const;
    void SetNumSteps(int);
    void SetFrame(float);
    int Current() const;

private:
    void SyncSlider();

protected:
    UISlider();

    virtual void SetSelectedAux(int);
    UICOMP_DC3_VIRTUAL void OldResourcePreload(BinStream &);

    void Update();
    DataNode OnMsg(const ButtonDownMsg &);

    // Retail X360 layout: UIComponent [0,0x140), ScrollSelect base @0x140.
    // sizeof(UISlider) = 0x190 (retail New allocs 400 bytes via fn_827E4BB0).
    // UISlider has NO own resource member (rb3-Wii oracle confirms only the 3
    // scalars). The funclets fn_827E4518/fn_827E455C destroy UISlider's *virtual
    // bases* — Hmx::Object @0x15c and RndHighlightable @0x190 — which sit at the
    // object tail (MSVC places virtual bases last). RndHighlightable/Hmx::Object
    // reach UISlider virtually via UIComponent->RndTransformable->RndHighlightable
    // (RndTransformable : public virtual RndHighlightable : public virtual
    // Hmx::Object). The slider's visuals use UIComponent's inherited mResource.
    int mCurrent; // 0x14c
    int mNumSteps; // 0x150
    bool mVertical; // 0x154 (lbz 0x154 confirmed); +4 pad to virtual-base region @0x15c
};
