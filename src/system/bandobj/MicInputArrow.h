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
    // NOTE(INSDEL-1): the residual charge in NewObject (`stw r3,0x54(r31)` retail
    // vs our 0x50) is the FloatKeys stack-slot-merge class in its ADDRESSABLE
    // direction -- retail keeps the discarded StaticClassName() Symbol temp at
    // 0x50 and homes the new-expression pointer at 0x54; we reuse the dead
    // Symbol's slot.  It is NOT source-addressable here, and the reason is
    // structural: the pair straddles an INLINING BOUNDARY (the Symbol temp is
    // created inside the inlined `operator new`, the pointer in NewObject), so
    // the "declare both at one function scope" lever has no handle on it.
    // Measured, both BYTE-IDENTICAL to this form (99.96429, same single charge):
    //   naming the pointer   -- `T *o = new T; return o;`
    //   naming BOTH          -- + `Symbol cn = StaticClassName();`
    // Decisive against a liveness reading: retail's Symbol temp is dead too and
    // still gets its own slot ⇒ compiler slot-colouring, not source liveness.
    // Same shape and same verdict for ScrollbarDisplay::NewObject (112 B).

    // Retail's class operator new is INLINED into NewObject and still evaluates
    // StaticClassName(): the target is `addi r3, r31, 0x50; bl
    // ?StaticClassName@MicInputArrow@; li r4, 0; li r3, 0x1f4; bl <MemAlloc>`.
    // The tree-wide OBJ_MEM_OVERLOAD is __declspec(noinline) AND its
    // `StaticClassName().Str()` argument is swallowed by the MemAlloc
    // debug-arg-stripping macro, so neither the call nor the inlining survives.
    // Spell it out locally instead of perturbing the shared macro.
    static void *operator new(unsigned int s) {
        StaticClassName();
        return MemAlloc(s, __FILE__, 0x21, "MicInputArrow", 0);
    }
    static void *operator new(unsigned int s, void *place) { return place; }
    DELETE_OVERLOAD;

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
