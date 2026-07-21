#pragma once
#include "obj/Object.h"
#include "rndobj/Anim.h"
#include "rndobj/Env.h"
#include "math/Key.h"
#include "utl/BinStream.h"

/**
 * @brief Animates RndEnviron ambient/fog color and fog range over time.
 * Original _objects description:
 * "EnvAnim objects animate environment properties."
 */
class RndEnvAnim : public RndAnimatable {
public:
    // Hmx::Object
    virtual void Replace(ObjRef *, Hmx::Object *);
    OBJ_CLASSNAME(EnvAnim);
    OBJ_SET_TYPE(EnvAnim);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void Print();
    // RndAnimatable
    virtual void SetFrame(float, float);
    virtual float EndFrame();
    virtual Hmx::Object *AnimTarget() { return mEnviron; }
    virtual void SetKey(float);

    NEW_OVERLOAD;
    DELETE_OVERLOAD;
    NEW_OBJ(RndEnvAnim)
    static void Init() { REGISTER_OBJ_FACTORY(RndEnvAnim) }

    Keys<Hmx::Color, Hmx::Color> &AmbientColorKeys() {
        return mKeysOwner->mAmbientColorKeys;
    }
    Keys<Hmx::Color, Hmx::Color> &FogColorKeys() { return mKeysOwner->mFogColorKeys; }
    Keys<Vector2, Vector2> &FogRangeKeys() { return mKeysOwner->mFogRangeKeys; }
    RndEnvAnim *KeysOwner() const { return mKeysOwner; }

protected:
    RndEnvAnim();

    /** The RndEnviron to animate. */
    ObjPtr<RndEnviron> mEnviron; // 0x10
    /** The collection of fog color keys. */
    Keys<Hmx::Color, Hmx::Color> mFogColorKeys;
    /** The collection of fog range keys. */
    Keys<Vector2, Vector2> mFogRangeKeys;
    /** The collection of ambient color keys. */
    Keys<Hmx::Color, Hmx::Color> mAmbientColorKeys;
    /** The EnvAnim that owns all of these keys. */
    ObjOwnerPtr<RndEnvAnim> mKeysOwner;
};
