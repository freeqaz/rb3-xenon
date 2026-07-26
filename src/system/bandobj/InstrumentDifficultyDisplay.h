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
    NEW_OVERLOAD;
    DELETE_OVERLOAD;

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
