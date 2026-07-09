#pragma once
#include "math/Geo.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "rndobj/Anim.h"
#include "rndobj/Draw.h"
#include "rndobj/Env.h"
#include "rndobj/Poll.h"
#include "rndobj/Trans.h"
#include "obj/Msg.h"
#include "utl/MemMgr.h"

// RndDir derives MsgSource in retail RB3 (and the faithful rb3-Wii decomp). DC3
// (our source provenance) DROPPED the MsgSource base and ADDED an `mEnters`
// vector + HarvestPollables — making our RndDir 0xc too small (MsgSource
// subobject 0x18 - mEnters 0xc = +0xc deficit). Restored to match retail.
class RndDir : public ObjectDir,
               public RndDrawable,
               public RndAnimatable,
               public RndTransformable,
               public RndPollable,
               public MsgSource {
public:
    // Hmx::Object
    virtual bool Replace(ObjRef *, Hmx::Object *);
    OBJ_CLASSNAME(RndDir);
    OBJ_SET_TYPE(RndDir);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, CopyType);
    virtual void Load(BinStream &);
    virtual void Export(DataArray *, bool);
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    // ObjectDir
    virtual void SetSubDir(bool);
    virtual void SyncObjects();
    virtual void ChainSourceSubdir(Hmx::Object *, ObjectDir *);
    // NON-virtual in retail RB3 (and rb3-Wii oracle): confirmed absent from all
    // retail vtables (0 refs to RndDir::CollideListSubParts base impl 0x823f0890
    // across the whole .rdata). DC3 provenance made it virtual, inserting a bogus
    // slot into every RndDir-descendant vtable and shifting the class-specific
    // tail +1 (breaks TrackDir SetDisplayRange/GetChordMesh vcall offsets).
    void
    CollideListSubParts(const Segment &, std::list<RndDrawable::Collision> &);
    // RndDrawable
    virtual void UpdateSphere();
    virtual float GetDistanceToPlane(const Plane &, Vector3 &);
    virtual bool MakeWorldSphere(Sphere &, bool);
    virtual void DrawShowing();
    virtual void ListDrawChildren(std::list<RndDrawable *> &);
    virtual RndDrawable *CollideShowing(const Segment &, float &, Plane &);
    virtual int CollidePlane(const Plane &);
    virtual void CollideList(const Segment &, std::list<Collision> &);
    virtual void Highlight() { RndDrawable::Highlight(); }
    // RndAnimatable
    virtual void SetFrame(float, float);
    virtual float EndFrame();
    virtual void ListAnimChildren(std::list<RndAnimatable *> &) const;
    // RndTransformable
    // RndPollable
    virtual void Poll();
    virtual void Enter();
    virtual void Exit(); // 0x1ac
    virtual void ListPollChildren(std::list<RndPollable *> &) const;

    OBJ_MEM_OVERLOAD(0x19);
    NEW_OBJ(RndDir)
    static void Init() { REGISTER_OBJ_FACTORY(RndDir) }

    void SetEnv(RndEnviron *env) { mEnv = env; }
    RndEnviron *GetEnv() const { return mEnv; }
    void SyncDrawables();
#ifdef HX_NATIVE
    int NumDraws() const { return mDraws.size(); }
    RndDrawable *GetDraw(int i) const { return mDraws[i]; }
    // Add a drawable to this dir's draw list (native only).
    // Used to inject subdir drawables that SyncDrawables misses.
    void NativeAddDraw(RndDrawable *d) { mDraws.push_back(d); }
#endif

protected:
    RndDir();

    virtual void RemovingObject(Hmx::Object *);
    virtual void OldLoadProxies(BinStream &, int);

    void HarvestPollables(std::vector<RndPollable *> &);

    DataNode OnShowObjects(DataArray *);
    DataNode OnSupportedEvents(DataArray *);

    /** "List of all the draws" */
    std::vector<RndDrawable *> mDraws; // 0x1b4
    /** Animations for this dir. */
    std::vector<RndAnimatable *> mAnims; // 0x1c0
    /** "List of all the polls" */
    std::vector<RndPollable *> mPolls; // 0x1cc
    /** The dedicated RndEnviron for this dir. */
    ObjPtr<RndEnviron> mEnv; // 0x1e4
    /** "Test event" */
    Symbol mTestEvent; // 0x1f8
};
