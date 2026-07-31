#pragma once
#include "math/Vec.h"
#include "obj/Object.h"
#include "rndobj/Highlight.h"
#include "rndobj/Trans.h"
#include "utl/MemMgr.h"

/** "Object representing blowing wind, CharHair and Fur can point at them." */
class RndWind : public Hmx::Object {
public:
    virtual ~RndWind();
    virtual void Replace(ObjRef *, Hmx::Object *);
    OBJ_CLASSNAME(Wind);
    OBJ_SET_TYPE(Wind);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);

    OBJ_MEM_OVERLOAD(0x1A);
    NEW_OBJ(RndWind)
    static void Init();

    void SetWindOwner(RndWind *wind);
    /** "zero out the wind" */
    void Zero();
    /** "set defaults for outside" */
    void SetDefaults();
    static float GetWind(float);
    static float GetWhiteNoise(float);
    void GetWind(const Vector3 &v, float f, Vector3 &v2) {
        mWindOwner->SelfGetWind(v, f, v2);
    }
protected:
    RndWind();
    void SelfGetWind(const Vector3 &, float, Vector3 &);
    void SyncLoops();

    /** "Prevailing wind in units/sec, along each world space axis,
        adds to random component, 1 mph == 17 inches/sec == .5 meter/sec" */
    Vector3 mPrevailing; // 0x28
    /** "Random wind speed in units/sec, along each world axis,
        adds to prevailing wind, 1 mph == 17 inches/sec == .5 meter/sec" */
    Vector3 mRandom; // 0x38
    /** "how long in seconds before the wind loops, 50 is a nice default" */
    float mTimeLoop; // 0x48
    /** "how far in units before the wind loops, 100 is a nice default" */
    float mSpaceLoop; // 0x4c
    Vector3 mTimeRate; // 0x50
    Vector3 mSpaceRate; // 0x60
    /** "Wind owner for the wind, properties shown are not for the owner,
        however, you must edit it directly" */
    ObjOwnerPtr<RndWind> mWindOwner; // 0x70
};
