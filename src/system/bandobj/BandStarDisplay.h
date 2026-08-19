#pragma once
#include "obj/ObjMacros.h"
#include "rndobj/Dir.h"
#include "rndobj/EventTrigger.h"
#include "synth/Sequence.h"

class BandStarDisplay : public RndDir {
public:
    BandStarDisplay();
    OBJ_CLASSNAME(BandStarDisplay)
    OBJ_SET_TYPE(BandStarDisplay)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, CopyType);
    virtual ~BandStarDisplay();
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    virtual void SyncObjects();

    void SetNumStars(float, bool);
    void SetStarType(Symbol, bool);
    void Reset();
    void SetupStars();
    void ResetStars();

    float GetNumStars() const { return mNumStars; }

    DECLARE_REVS;
    NEW_OVERLOAD;
    DELETE_OVERLOAD;
    NEW_OBJ(BandStarDisplay)
    static void Init() { Register(); }
    REGISTER_OBJ_FACTORY_FUNC(BandStarDisplay)

    float mNumStars; // 0x1dc
    ObjVector<ObjPtr<RndDir> > mStars; // 0x1e0
    ObjVector<ObjPtr<RndAnimatable> > mStarSweepAnims; // 0x19c
    ObjVector<ObjPtr<EventTrigger> > mStarFullTriggers; // 0x1a8
    ObjVector<ObjPtr<EventTrigger> > mStarGoldTriggers; // 0x1b4
    ObjPtr<RndAnimatable> mStarOffsetAnim; // 0x1c0
    ObjPtr<Sequence> mEarnStarSfx; // 0x1cc
    ObjPtr<Sequence> mEarnSpadeSfx; // 0x1d8
    Symbol mStarType; // 0x1e4
};