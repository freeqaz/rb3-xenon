#pragma once
#include "obj/Object.h"
#include "obj/Msg.h"
#include "ui/UIListProvider.h"
#include "meta/FixedSizeSaveableStream.h"

class Modifier {
public:
    Modifier(DataArray *);

    bool IsHidden() const;
    bool CustomLocation() const;
    bool SaveValue() const;
    bool UseSaveValue() const;
    bool DefaultEnabled() const;
    bool DelayedEffect() const;

    void SetDefaultEnabled(bool value) { mDefaultEnabled = value; }

    DataArray *mData; // 0x0
    bool mDefaultEnabled; // 0x4
};

// Retail X360 layout, read directly out of the ModifierMgr ctor at 0x82589C48:
//   0x00 UIListProvider vfptr
//   0x04 MsgSource       (carries the vbptr; ctor ??0MsgSource@@QAA@XZ called on this+4)
//   0x1c mModifiers      (3-word vector, zeroed at 0x1c/0x20/0x24)
//   0x28 mModifiersList  (3-word vector, zeroed at 0x28/0x2c/0x30)
//   0x34 vtordisp
//   0x38 Hmx::Object     VIRTUAL base (??0Object@Hmx@@QAA@XZ called on this+0x38
//                        under the hidden vbase-ctor flag `cmplwi cr6, r4, 0`)
// i.e. Hmx::Object is reached virtually *through MsgSource*, not as a direct
// first base -- which is why mModifiers sits at 0x1c and not 0x2c.
class ModifierMgr : public UIListProvider, public MsgSource {
public:
    ModifierMgr();
    virtual ~ModifierMgr();
    virtual DataNode Handle(DataArray *, bool);
    virtual Symbol DataSymbol(int) const;
    virtual int NumData() const;
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual bool IsHidden(int) const;
    virtual bool IsActive(int) const;

    bool IsModifierActive(Symbol) const;
    Modifier *GetModifierAtListData(int) const;
    bool IsModifierUnlocked(Modifier *) const;
    bool IsModifierActive(Modifier *) const;
    bool HasModifier(Symbol);
    Modifier *GetModifier(Symbol, bool) const;
    void ToggleModifierEnabled(Symbol);
    bool IsModifierDelayedEffect(Symbol) const;
    int SaveSize(int);
    void DisableAutoVocals() const;
    void Save(FixedSizeSaveableStream &);
    void Load(FixedSizeSaveableStream &, int);

    static void Init();

    std::vector<Modifier *> mModifiers; // 0x20
    std::vector<Modifier *> mModifiersList; // 0x28
};

extern ModifierMgr *TheModifierMgr;
