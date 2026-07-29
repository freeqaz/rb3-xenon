#pragma once
#include "math/Mtx.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "rndobj/Dir.h"
#include "rndobj/Env.h"
#include "rndobj/Trans.h"
#include "ui/UIComponent.h"
#include "utl/FilePath.h"
#include "utl/MemMgr.h"

/** "A UIProxy object allows artists to position dynamically
 *  loaded resources (e.g. a character) in Milo. The app will then load
 *  the appropriate resources into it."
 *
 * Retail Xbox 360 layout (derived from the retail ctor fn_82823430):
 *   UIComponent [0x0,0x140)  mDir@0x140  mEnv@0x14c  mMainTrans@0x158
 *   mOldXfm@0x15c  mSyncOnMove@0x19c  mPolled@0x19d   total size 0x1a0.
 */
class UIProxy : public UIComponent {
public:
    // Hmx::Object
    virtual ~UIProxy() {}
    OBJ_CLASSNAME(UIProxy)
    OBJ_SET_TYPE(UIProxy)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void SetTypeDef(DataArray *);
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    // RndDrawable
    virtual void DrawShowing();
    virtual RndDrawable *CollideShowing(const Segment &, float &, Plane &);
    virtual int CollidePlane(const Plane &);
    // RndPollable
    virtual void Poll();

    NEW_OBJ(UIProxy)
    OBJ_MEM_OVERLOAD(0x10F)

    void UpdateDir();
    void SyncDir();
    void SetProxyDir(const FilePath &, bool);
    void SetProxyDir(RndDir *);
    DataNode OnSetProxyDir(DataArray *);
    RndDir *ProxyDir() const { return mDir; }

    static void Init();

protected:
    UIProxy();

    ObjDirPtr<RndDir> mDir; // 0x140
    /** "environment to use on it" */
    ObjPtr<RndEnviron> mEnv; // 0x14c
    RndTransformable *mMainTrans; // 0x158
    Transform mOldXfm; // 0x15c
    bool mSyncOnMove; // 0x19c
    bool mPolled; // 0x19d
};
