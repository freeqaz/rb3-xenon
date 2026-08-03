#pragma once
#include "bandobj/BandLabel.h"
#include "obj/ObjMacros.h"
#include "rndobj/Mat.h"
#include "rndobj/Mesh.h"
#include "rndobj/PropAnim.h"
#include "ui/UIColor.h"
#include "ui/UIComponent.h"

class InstrumentDifficultyDisplay : public UIComponent {
public:
    enum InstrumentState {
        kHidden,
        kName,
        kIcon
    };

    InstrumentDifficultyDisplay();
    OBJ_CLASSNAME(InstrumentDifficultyDisplay);
    OBJ_SET_TYPE(InstrumentDifficultyDisplay);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void DrawShowing();
    virtual ~InstrumentDifficultyDisplay();
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    virtual void Update();

    void SetValues(Symbol, int, int, bool);
    void UpdateDisplay();
    void SetInstrumentState(InstrumentState);

    static void Init();
    static void Register() { REGISTER_OBJ_FACTORY(InstrumentDifficultyDisplay); }
    NEW_OBJ(InstrumentDifficultyDisplay);
    DECLARE_REVS;
    // Retail's NewObject has the OBJ_MEM_OVERLOAD shape, not NEW_OVERLOAD's:
    // `bl StaticClassName` into a discarded Symbol temp, then a 2-arg
    // `MemAlloc(size, 0)` INLINE (`li r4,0; li r3,0x1b0; bl ...; stw r3,0x54(r31)`).
    // NEW_OVERLOAD instead emits a noinline class `operator new` (one `bl`,
    // `stw r3,0x50(r31)`) that retail has no function row for at all.
    // See the measured note at utl/MemMgr.h:285.
    OBJ_MEM_OVERLOAD(0x28);

    RndPropAnim *mDifficultyAnim;
    RndMesh *mVocalPartMesh;
    RndMat *mVocalPart1Mat;
    RndMat *mVocalPart2Mat;
    RndMat *mVocalPart3Mat;
    BandLabel *mInstrumentLabel;
    InstrumentState mInstrumentState;
    bool mHasPart;
    int mDifficulty;
    int mNumVocalParts;
    Symbol mInstrumentType;
    ObjPtr<UIColor> mInstrumentColorOverride;
};
