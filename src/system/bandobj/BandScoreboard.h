#pragma once
#include "obj/ObjMacros.h"
#include "rndobj/Dir.h"
#include "rndobj/Mesh.h"
#include "bandobj/BandStarDisplay.h"

class BandScoreboard : public RndDir {
public:
    BandScoreboard();
    OBJ_CLASSNAME(BandScoreboard)
    OBJ_SET_TYPE(BandScoreboard)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, CopyType);
    virtual ~BandScoreboard();
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);

    void SetScore(int);
    void SetNumStars(float, bool);
    float GetNumStars() const;
    void Reset();
    void SetupScore();
    void ResetScore();

    DECLARE_REVS;
    NEW_OVERLOAD;
    DELETE_OVERLOAD;
    NEW_OBJ(BandScoreboard)
    static void Init() { Register(); }
    REGISTER_OBJ_FACTORY_FUNC(BandScoreboard)

    int mScore; // 0x1dc
    ObjPtr<RndMesh> mThousandsCommaMesh; // 0x1e0
    ObjPtr<RndMesh> mMillionsCommaMesh; // 0x1ec
    ObjVector<ObjPtr<RndMesh> > mNumMeshes; // 0x1f8
    ObjVector<ObjPtr<RndMesh> > mSrcMeshes; // 0x208
    ObjPtr<BandStarDisplay> mStarDisplay; // 0x218
};
