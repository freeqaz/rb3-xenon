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
    virtual ~UIComponent() {}
    OBJ_CLASSNAME(UIComponent)
    OBJ_SET_TYPE(UIComponent)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    // RndDrawable
    virtual void Highlight() { RndDrawable::Highlight(); }
    // RndPollable
    virtual void Poll();
    virtual void Enter();
    virtual void Exit();
    virtual void SetState(UIComponent::State);
    virtual Symbol StateSym() const;
    virtual bool Entering() const { return false; }
    virtual bool Exiting() const { return mState == kSelecting; }
    virtual bool CanHaveFocus() { return true; }

    OBJ_MEM_OVERLOAD(0x19);
    NEW_OBJ(UIComponent)

    State GetState() { return mState; }
    UIComponent *NavRight() const { return mNavRight; }
    UIComponent *NavDown() const { return mNavDown; }
    bool Loading() const { return mLoading; }

    static void Init();

protected:
    virtual void OldResourcePreload(BinStream &);
    UIComponent();
    void SendSelect(LocalUser *);

    static int sSelectFrames;

    State mState; // 0xe0 (int-sized State enum, FIRST member)
    ObjPtr<UIComponent> mNavRight; // 0xe4 (ObjPtr is 0xc)
    ObjPtr<UIComponent> mNavDown; // 0xf0
    LocalUser *mSelectingUser; // 0xfc
    UIScreen *mSelectScreen; // 0x100
    UIResource *mResource; // 0x104
    int mSelected; // 0x108
    std::vector<UIMesh> mMeshes; // 0x10c (stride 0x18)
    String mResourceName; // 0x118
    ObjDirPtr<ObjectDir> mResourceDir; // 0x124 (ObjDirPtr is 0xc)
    String mResourcePath; // 0x130
    unsigned char mSelectCancelled; // 0x13c
    bool mLoading; // 0x13d
    bool mMockSelect; // 0x13e (1 byte, 0x13f padding)

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
