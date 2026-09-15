#pragma once
#include "obj/ObjMacros.h"
#include "rndobj/Anim.h"
#include "rndobj/EventTrigger.h"
#include "ui/UIComponent.h"
#include <vector>

class MicManagerInterface;

class MicInputArrow : public UIComponent {
public:
    MicInputArrow();
    OBJ_CLASSNAME(MicInputArrow);
    OBJ_SET_TYPE(MicInputArrow);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void DrawShowing();
    virtual ~MicInputArrow() {}
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    virtual void Update();

    void SetMicMgr(MicManagerInterface *);
    void SetMicConnected(bool, int);
    void SetMicExtended(int);
    void SetMicPreview(int);
    void SetMicHidden(int);

    static void Init();
    static void Register() { REGISTER_OBJ_FACTORY(MicInputArrow); }
    NEW_OBJ(MicInputArrow);
    // DATED RECORD -- NOTE(INSDEL-1) used to conclude the `stw r3,0x54(r31)`
    // retail vs our 0x50 residual was "NOT source-addressable ... compiler
    // slot-colouring, not source liveness". That verdict is SCOPED TO THE TWO
    // LEVERS IT TRIED, both at NewObject scope and both measured byte-identical
    // at 99.96429: naming the pointer (`T *o = new T; return o;`) and
    // additionally naming the Symbol (`Symbol cn = StaticClassName();`). Its own
    // reasoning says why those cannot work -- the pair straddles an inlining
    // boundary, so a NewObject-scope declaration has no handle on it.
    // It never tested the lever that acts INSIDE operator new, which
    // utl/MemMgr.h records as measured on the 10 FxSend*360 classes: calling
    // `.Str()` on the temp and naming the `mem` local. That is what the shared
    // OBJ_MEM_OVERLOAD spells today, and the "tree-wide OBJ_MEM_OVERLOAD is
    // __declspec(noinline)" premise of the local copy below is stale -- only its
    // operator DELETE is noinline now. Lane W16-BE, 2026-09-15.

    // Retail's class operator new is INLINED into NewObject and still evaluates
    // StaticClassName(): the target is `addi r3, r31, 0x50; bl
    // ?StaticClassName@MicInputArrow@; li r4, 0; li r3, 0x1f4; bl <MemAlloc>`.
    // The local hand-rolled copy that used to sit here discarded the Symbol with
    // a bare `StaticClassName();`, which homes the temp into the SAME slot as
    // `mem` -- the 0x50-vs-0x54 residual. Plain OBJ_MEM_OVERLOAD keeps operator
    // delete noinline exactly as DELETE_OVERLOAD did, so ??_GMicInputArrow is
    // unperturbed; the unwind funclet at 0x82319348 loads mem from 0x54 and
    // calls the out-of-line ICF survivor ??3BinStream@@SAXPAX@Z, which is what
    // a noinline delete produces.
    OBJ_MEM_OVERLOAD(0x39);

    // Retail-360 layout, read off the target span 0x82318C70..0x82319810
    // (?Update@ member offsets + ??1MicInputArrow@ vector-free offsets):
    // UIComponent is 0x140; both flag vectors are 20-byte std::vector<bool>
    // (dtor loads _M_start@+0 / _M_end_of_storage@+0x10), the six object
    // vectors are 12-byte, and there is NO extra vector<int> between
    // mLevelAnims (0x1ac) and mMicEnergyNormalizer (0x1b8).
    int mArrowNum; // 0x140
    MicManagerInterface *mMicManagerInterface; // 0x144
    std::vector<bool> mConnectedFlags; // 0x148
    std::vector<bool> mHiddenFlags; // 0x15c
    std::vector<EventTrigger *> mConnectedTrigs; // 0x170
    std::vector<EventTrigger *> mDisconnectedTrigs; // 0x17c
    std::vector<EventTrigger *> mHiddenTrigs; // 0x188
    std::vector<EventTrigger *> mPreviewTrigs; // 0x194
    std::vector<EventTrigger *> mExtendedTrigs; // 0x1a0
    std::vector<RndAnimatable *> mLevelAnims; // 0x1ac
    float mMicEnergyNormalizer; // 0x1b8
    // NOTE: rb3-Wii's dev header also carries `bool unk160` + `float unk164[3]`
    // (a manual level-override path used by DrawShowing's `switch`). Retail-360
    // does NOT have them: the Hmx::Object virtual base sits at 0x1bc (read off
    // `lwz r11, -0x1bc(r30)` in ?SetType@MicInputArrow@), i.e. exactly 16 bytes
    // earlier than the Wii layout, and sizeof is 0x1f4.
};
