#pragma once
#include "obj/Dir.h"
#include "obj/Object.h"
#include "os/User.h"
#include "rndobj/Draw.h"
#include "rndobj/Trans.h"
#include "rndobj/Poll.h"
#include "rndobj/Mesh.h"
#include "utl/MemMgr.h"
#include "ui/UIScreen.h"
#include <vector>

class PanelDir;
class UIResource;

/**
 * @brief A base implementation of a UI object.
 * Original _objects description:
 * "Base class of all UI components,
 * defines navigation and component state"
 *
 * Retail Xbox 360 layout (verified against UIComponent dtor fn_827DABC0):
 *   RndDrawable [0x0,0x24)  RndTransformable [0x24,0xd8)  RndPollable [0xd8,0xe4)
 *   total size 0x140. ObjDirPtr is 0xc, ObjPtr is 0xc.
 */
class UIComponent : public RndDrawable,
                    public RndTransformable,
                    public RndPollable {
public:
    enum State {
        kNormal = 0,
        kFocused = 1,
        kDisabled = 2,
        kSelecting = 3,
        kSelected = 4,
        kNumStates = 5,
    };

    // size 0x18 (RndMesh* + RndMat*[kNumStates])
    class UIMesh {
    public:
        RndMesh *mMesh; // 0x0
        RndMat *mMats[kNumStates]; // 0x4
    };

    // Hmx::Object
    virtual ~UIComponent();
    OBJ_CLASSNAME(UIComponent)
    OBJ_SET_TYPE_ENGINE(UIComponent)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    // retail-360 UIComponent overrides SetTypeDef (Object-vbase vtable slot;
    // slot-neutral — see docs/decomp/research/2026-06-11-uicomponent-virtuals.md
    // §1.2 slot 15 = fn_827DAB68 = retail UIComponent::SetTypeDef thunk. Was
    // deferred as "Phase B"; ported here to fix UIProxy::SetTypeDef's callsite
    // this-adjustment (fixed offset vs our former Object-vbase runtime lookup).
    virtual void SetTypeDef(DataArray *);
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    // RndDrawable
    virtual void Highlight() { RndDrawable::Highlight(); }
    // RndPollable
    virtual void Poll();
    virtual void Enter();
    virtual void Exit();
    // UIComponent own-virtuals — retail-360 order/set verified from the retail
    // vtable @0x8211D4A4 (20 slots; own region slots 12-19 = 0x30..0x4c), see
    // docs/decomp/research/2026-06-11-uicomponent-virtuals.md
    virtual void ResourceCopy(const UIComponent *);              // slot 12, 0x30
    virtual void SetState(UIComponent::State);                   // slot 13, 0x34
    virtual Symbol StateSym() const;                             // slot 14, 0x38
    virtual bool Entering() const { return false; }              // slot 15, 0x3c
    virtual bool Exiting() const { return mState == kSelecting; }// slot 16, 0x40
    virtual bool CanHaveFocus() { return true; }                 // slot 17, 0x44
    virtual void CopyMembers(const UIComponent *, Hmx::Object::CopyType); // slot 18, 0x48
    virtual void Update();                                       // slot 19, 0x4c

    OBJ_MEM_OVERLOAD(0x19);
    NEW_OBJ(UIComponent)

    State GetState() { return mState; }
    UIComponent *NavRight() const { return mNavRight; }
    UIComponent *NavDown() const { return mNavDown; }
    bool Loading() const { return mLoading; }

    const char *GetResourcesPath();
    DataNode OnGetResourcesPath(DataArray *);
    class ObjectDir *ResourceDir();
    void ResourceFileUpdated(bool);
    void UpdateResource();
    void UpdateMeshes(State);
    void MockSelect();

    static void Init();

protected:
    // OldResourcePreload is a DC3-only virtual; retail-360 UIComponent has NO
    // such vtable slot (verified: retail primary vtable @0x8211D4A4 is exactly
    // 20 slots with no OldResourcePreload). Gate the `virtual` behind HX_NATIVE
    // exactly like DRAW_DC3_VIRTUAL (rndobj/Draw.h). All derived-class
    // OldResourcePreload overrides use the same macro so they don't insert a
    // bogus first-class virtual slot.
#ifdef HX_NATIVE
#define UICOMP_DC3_VIRTUAL virtual
#else
#define UICOMP_DC3_VIRTUAL
#endif
    UICOMP_DC3_VIRTUAL void OldResourcePreload(BinStream &);
    UIComponent();
    void SendSelect(LocalUser *);

    static int sSelectFrames;

    State mState; // 0xe0 (int-sized State enum, FIRST member)
    ObjPtr<UIComponent> mNavRight; // 0xe4 (ObjPtr is 0xc)
    ObjPtr<UIComponent> mNavDown; // 0xf0
    LocalUser *mSelectingUser; // 0xfc
    UIScreen *mSelectScreen; // 0x100
    int mSelected; // 0x104 (verified: Enter() stores 0 here; ResourceCopy reads c->[0x108]=mResource)
    UIResource *mResource; // 0x108
    std::vector<UIMesh> mMeshes; // 0x10c (stride 0x18)
    String mResourceName; // 0x118
    ObjDirPtr<ObjectDir> mResourceDir; // 0x124 (ObjDirPtr is 0xc)
    String mResourcePath; // 0x130
    bool mLoading; // 0x13c (verified: UpdateResource reads 0x13c twice as mLoading)
    unsigned char mSelectCancelled; // 0x13d — Wii name mMockSelect, same byte:
                                    // MockSelect() sets it, FinishSelecting() tests+clears.
                                    // (no third flag; 0x13e/0x13f = padding)

private:
    void FinishSelecting();
};

UIComponent::State SymToUIComponentState(Symbol s);

#include "obj/Msg.h"

DECLARE_MESSAGE(UIComponentScrollMsg, "component_scroll");
UIComponentScrollMsg(UIComponent *comp, LocalUser *user) : Message(Type(), comp, user) {}
UIComponent *GetUIComponent() const { return mData->Obj<UIComponent>(2); }
LocalUser *GetUser() const { return mData->Obj<LocalUser>(3); }
END_MESSAGE

DECLARE_MESSAGE(UIComponentSelectMsg, "component_select");
UIComponentSelectMsg(UIComponent *comp, LocalUser *user) : Message(Type(), comp, user) {}
UIComponent *GetComponent() const { return mData->Obj<UIComponent>(2); }
LocalUser *GetUser() const { return mData->Obj<LocalUser>(3); }
END_MESSAGE

DECLARE_MESSAGE(UIComponentSelectDoneMsg, "component_select_done");
UIComponentSelectDoneMsg(UIComponent *comp, LocalUser *user)
    : Message(Type(), comp, user) {}
UIComponent *GetComponent() const { return mData->Obj<UIComponent>(2); }
LocalUser *GetUser() const { return mData->Obj<LocalUser>(3); }
END_MESSAGE

DECLARE_MESSAGE(UIComponentScrollSelectMsg, "component_scroll_select");
UIComponentScrollSelectMsg(UIComponent *comp, LocalUser *user, bool selected)
    : Message(Type(), comp, user, selected) {}
UIComponent *GetComponent() const { return mData->Obj<UIComponent>(2); }
LocalUser *GetUser() const { return mData->Obj<LocalUser>(3); }
bool GetSelected() const { return mData->Int(4); }
END_MESSAGE

DECLARE_MESSAGE(UIComponentScrollStartMsg, "component_scroll_start");
UIComponentScrollStartMsg(UIComponent *comp, LocalUser *user)
    : Message(Type(), comp, user) {}
UIComponent *GetComponent() const { return mData->Obj<UIComponent>(2); }
LocalUser *GetUser() const { return mData->Obj<LocalUser>(3); }
END_MESSAGE
