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
    NEW_OVERLOAD;
    DELETE_OVERLOAD;
    static void LoadOldBandTextComp(BinStream &);
    static void Init();
    static void Register() { REGISTER_OBJ_FACTORY(BandLabel); }
    NEW_OBJ(BandLabel);

    Keys<float, float> unk1dc; // 0x238
    Symbol unk1e4; // 0x244
    String unk1e8; // 0x1e8
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
