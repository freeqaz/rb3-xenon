#pragma once
#include "obj/Object.h"
#include "rndobj/Anim.h"
#include "rndobj/Part.h"
#include "utl/MemMgr.h"

/** "Object that animates Particle System properties." */
class RndParticleSysAnim : public RndAnimatable {
public:
    // Hmx::Object
    virtual void Replace(ObjRef *, Hmx::Object *);
    OBJ_CLASSNAME(ParticleSysAnim);
    OBJ_SET_TYPE_ENGINE(ParticleSysAnim);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void Print();
    // RndAnimatable
    virtual void SetFrame(float, float);
    virtual float EndFrame();
    virtual Hmx::Object *AnimTarget() { return mParticleSys; }
    virtual void SetKey(float);

    OBJ_MEM_OVERLOAD(0x18)
    NEW_OBJ(RndParticleSysAnim)
    static void Init() { REGISTER_OBJ_FACTORY(RndParticleSysAnim) }

    void SetParticleSys(RndParticleSys *);
    Keys<Hmx::Color, Hmx::Color> &StartColorKeys() { return mKeysOwner->mStartColorKeys; }
    Keys<Hmx::Color, Hmx::Color> &EndColorKeys() { return mKeysOwner->mEndColorKeys; }
    Keys<Vector2, Vector2> &EmitRateKeys() { return mKeysOwner->mEmitRateKeys; }
    Keys<Vector2, Vector2> &SpeedKeys() { return mKeysOwner->mSpeedKeys; }
    Keys<Vector2, Vector2> &LifeKeys() { return mKeysOwner->mLifeKeys; }
    Keys<Vector2, Vector2> &StartSizeKeys() { return mKeysOwner->mStartSizeKeys; }
    RndParticleSysAnim *KeysOwner() const { return mKeysOwner; }

protected:
    RndParticleSysAnim();

    ObjPtr<RndParticleSys> mParticleSys; // 0x10
    Keys<Hmx::Color, Hmx::Color> mStartColorKeys; // 0x1c
    Keys<Hmx::Color, Hmx::Color> mEndColorKeys; // 0x28
    Keys<Vector2, Vector2> mEmitRateKeys; // 0x34
    Keys<Vector2, Vector2> mSpeedKeys; // 0x40
    Keys<Vector2, Vector2> mLifeKeys; // 0x4c
    Keys<Vector2, Vector2> mStartSizeKeys; // 0x58
    ObjOwnerPtr<RndParticleSysAnim> mKeysOwner; // 0x6c
};
