#pragma once
#include "ui/UITransitionHandler.h"
#include "ui/UILabel.h"
#include "math/Key.h"

class BandLabel : public UILabel, public UITransitionHandler {
public:
    BandLabel();
    OBJ_CLASSNAME(BandLabel);
    OBJ_SET_TYPE(BandLabel);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual ~BandLabel();
    virtual void PreLoad(BinStream &);
    virtual void Poll();
    /** Retail overrides this (0x82340A38; primary vtable slot 18 differs from
        UILabel's 0x827F4778): UILabel::CopyMembers, then the two
        UITransitionHandler anims at 0x218/0x224. */
    virtual void CopyMembers(const UIComponent *, CopyType);
    virtual void SetDisplayText(const char *, bool);
    virtual void Count(int, int, float, Symbol);
    virtual void FinishCount();
    virtual bool IsEmptyValue() const;
    // Retail mangles this `MAA` (protected virtual) — matching the access of
    // the UITransitionHandler base declaration. Access is pure name-mangling
    // (no vtable/layout effect), but objdiff pairs by name, so a public
    // declaration here can never pair with the target symbol.
protected:
    virtual void FinishValueChange();

public:
    // Retail INLINES the class operator new into NewObject and still evaluates
    // StaticClassName() -- ObjMacros.h shape (b). Retail bytes at 0x82341ff8
    // (lane W16-BE, 2026-09-15):
    //     addi r3,r31,0x50 ; bl ?StaticClassName@BandLabel@@SA?AVSymbol@@XZ
    //     li r4,0 ; li r3,0x290 ; bl ?MemAlloc@@YAPAXHH@Z ; stw r3,0x54(r31)
    // 0x290 == 656 == the compiler's sizeof(BandLabel), so no layout defect.
    // NEW_OVERLOAD gave shape (a) and left the row at 5/25 words (fuzzy
    // 86.929) -- the same inherited spelling retail contradicts in
    // ObjMacros.h's NEW_OBJ record.
    // Delete is the INLINABLE form (_INLINE_DEL): retail's deleting destructor
    // for this class (reached as ??_GAppLabel, which inherits it) calls
    // ?MemFree@@YAXPAX@Z directly. The NewObject unwind funclet at 0x82342068
    // calls the out-of-line ICF survivor ??3BinStream@@SAXPAX@Z, and MSVC keeps
    // that funclet call out of line even with the inlinable delete -- measured
    // on our BandLabel.obj (funclet -> ??3BandLabel, ??_G -> MemFree) and in the
    // whole-binary A/B, where the funclet row stayed at 100 (lane W16-IE,
    // 2026-10-01). The earlier "plain OBJ_MEM_OVERLOAD because of the funclet"
    // reading assumed the funclet would inline too; it does not.
    OBJ_MEM_OVERLOAD_INLINE_DEL(0x1f);
    static void LoadOldBandTextComp(BinStream &);
    static void Init();
    static void Register() { REGISTER_OBJ_FACTORY(BandLabel); }
    NEW_OBJ(BandLabel);

    Keys<float, float> unk1dc; // 0x238
    Symbol unk1e4; // 0x244
    String unk1e8; // 0x248
    bool unk1f4; // 0x254
    // Retail vbase trailing reserve (see AppLabel::Handle vtordisp evidence):
    // retail AppLabel's Hmx::Object virtual base sits at 0x25C into the
    // complete object; ours sat at 0x1B0 (-172). The missing 0xAC bytes are
    // real UILabel/BandLabel members whose true split is not yet
    // reconstructed (retail BandLabel.s shows member traffic at
    // 0x214..0x258). Reserve them here — only AppLabel derives BandLabel,
    // and no currently-matched function can embed the old (wrong) size or
    // vbase offset, so this is layout-additive and zero-loss by
    // construction. Do NOT let new members grow the class past 0x25C
    // (non-vbase) without re-deriving this pad.
    // 0xAC moved into UILabel (pushes UITransitionHandler base 0x16c->0x218);
    // total object size unchanged so Hmx::Object/RndHighlightable vbases stay
    // at 0x25c/0x290. No reserve needed here anymore.
};

DECLARE_MESSAGE(BandLabelCountDoneMsg, "count_done")
BandLabelCountDoneMsg(BandLabel *label) : Message(Type(), label) {}
END_MESSAGE
