#pragma once
#include "obj/Object.h"
#include "ui/UILabel.h"
#include "os/JoypadMsgs.h"
#include "utl/MemMgr.h"

/**
 * @brief A base implementation of a button.
 * Original _objects description:
 * "Simple button, basically just a
 * label that can be selected"
 */
class UIButton : public UILabel {
public:
    OBJ_CLASSNAME(UIButton);
    OBJ_SET_TYPE(UIButton);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);

    // INLINE_DEL, adjudicated on retail bytes (lane W16-X 2026-09-14):
    // retail folded ??_GUIButton into ??_GUILabel at 0x827f5348 (both classes'
    // RTTI vtable slot at subobject 536 holds the one thunk 0x827f42a8), and that
    // single 80 B body calls ?MemFree@@YAXPAX@Z DIRECTLY at +48.  Plain
    // OBJ_MEM_OVERLOAD is __declspec(noinline) and emitted bl ??3UIButton@@SAXPAX@Z
    // there, so our two ??_G bodies could not fold.  This is the per-class call
    // the macro comment in utl/MemMgr.h asks to be decided on retail bytes.
    OBJ_MEM_OVERLOAD_INLINE_DEL(0x15);
    NEW_OBJ(UIButton)

    static void Init();

private:
    DataNode OnMsg(const ButtonDownMsg &);

protected:
    UIButton();
};
