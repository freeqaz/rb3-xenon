// Retail inlines this TU's owner-only ObjPtr ctor(s) with the vtable
// materialization pinned AFTER the member stores -- the
// RB3_OBJPTR_FORCEINLINE_CTOR signature (see obj/ObjPtr_p.h). The
// extent census shows delta ~= -16 * (surplus bl) for this TU's ctor,
// i.e. one un-inlined ObjPtr ctor per surplus call.
#define RB3_OBJPTR_FORCEINLINE_CTOR

#include "rndobj/Group.h"
#include "Rnd.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "rndobj/Anim.h"
#include "rndobj/Cam.h"
#include "rndobj/Draw.h"
#include "rndobj/Env.h"
#include "rndobj/Trans.h"
#include "rndobj/Utl.h"
#include "utl/Std.h"

struct GroupDrawDist {
    RndDrawable *draw;
    float dist;
};

bool SortInWorld(const GroupDrawDist &a, const GroupDrawDist &b) {
    return a.dist < b.dist;
}

bool gInReplace;

RndGroup::RndGroup()
    : mObjects(this, kObjListOwnerControl), mEnv(this), mDrawOnly(this), mLod(this),
      mLodScreenSize(0), mDrawLod(false), mSortInWorld(false) {}

// Retail calls the base FIRST and UNCONDITIONALLY, then tests membership by
// VALUE (an inlined mObjects.find(from) != end() walk over Node{mObject@0,
// next@4}), and has no !obj branch at all. The ref->Parent() == &mObjects
// identity test, the !obj arm and the base call as a trailing else are all DC3's
// -- DC3 postdates RB3 and rewrote this alongside the bool-returning Replace
// protocol. rb3-Wii's RB3-era body is character-for-character what the retail
// bytes decode to, which is what settled it.
void RndGroup::Replace(ObjRef *ref, Hmx::Object *obj) {
    RndTransformable::Replace(ref, obj);
#ifdef HX_NATIVE
    Hmx::Object *from = ref->GetObj();
#else
    // Retail's first parameter is a Hmx::Object* ("from"); this tree types the
    // slot as ObjRef* project-wide, so the compare is against the raw argument.
    Hmx::Object *from = reinterpret_cast<Hmx::Object *>(ref);
#endif
    if (mObjects.find(from) != mObjects.end()) {
        AddObject(obj, from);
        gInReplace = true;
        RemoveObject(from);
        gInReplace = false;
    }
}

// Same class of defect as RndMat::Handle: our block had been stripped to the
// four HANDLE_SUPERCLASS lines while both oracles carry real handlers.
// Adjudicated on retail bytes: the six literals below sit in ONE contiguous
// .rdata cluster at 0x82068AB0..0x82068AF4 (has_object, get_draws,
// clear_objects, remove_object, add_object, sort_draws) -- the signature of a
// single function's static Symbols. DC3's three extra handlers
// ("insert_object", "num_objects", "get_group_children") occur ZERO times in
// orig/45410914/band.exe, and DC3 also swapped get_draws out for
// get_group_children. So retail == rb3-Wii here, and DC3's version postdates
// RB3. ("move_object" appears to be present only because it is the tail of
// "remove_object" -- the linker tail-merges string literals.)
BEGIN_HANDLERS(RndGroup)
    HANDLE_ACTION(sort_draws, SortDraws())
    HANDLE_ACTION(add_object, AddObject(_msg->Obj<Hmx::Object>(2)))
    HANDLE_ACTION(remove_object, RemoveObject(_msg->Obj<Hmx::Object>(2)))
    HANDLE_ACTION(clear_objects, ClearObjects())
    HANDLE(get_draws, OnGetDraws)
    HANDLE_EXPR(has_object, mObjects.find(_msg->Obj<Hmx::Object>(2)) != mObjects.end())
    HANDLE_SUPERCLASS(RndAnimatable)
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(RndTransformable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

void RndGroup::UpdateLODState() {
    if (mLod && mLodScreenSize > 0) {
        mDrawLod = true;
    } else
        mDrawLod = false;
}

// Retail-verified against fn_82453578: SIX properties in this order (guard
// bits 0..5 of one word at lbl_82CC2F20), not the three we carried.
// NOTE: this TU is in the Object.h macro dialect, whose SYNC_PROP already
// stringifies into a function-local static and whose SYNC_PROP_MODIFY is the
// positive-first (_ALT) shape -- so RB3_SYNCPROP_LOCAL_STATIC is INERT here
// and SYNC_PROP_MODIFY_ALT does not exist.  (Predicted this was the same
// local-static lever as VocalTrackDir; it is not.)
// The rb3-Wii DEV oracle spells sort_in_world as a hand-rolled inline block --
// RETAIL DOES NOT: it is a plain SYNC_PROP calling PropSync(bool&)
// (fn_82280290).  lod_screen_size cross-jumps into lod's tail, and
// UpdateLODState() is inlined at that shared tail.
BEGIN_PROPSYNCS(RndGroup)
    SYNC_PROP_MODIFY(objects, mObjects, Update())
    SYNC_PROP(environ, mEnv)
    SYNC_PROP(draw_only, mDrawOnly)
    SYNC_PROP_MODIFY(lod, mLod, UpdateLODState())
    SYNC_PROP_MODIFY(lod_screen_size, mLodScreenSize, UpdateLODState())
    SYNC_PROP(sort_in_world, mSortInWorld)
    SYNC_SUPERCLASS(RndDrawable)
    SYNC_SUPERCLASS(RndTransformable)
    SYNC_SUPERCLASS(RndAnimatable)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(RndGroup)
    SAVE_REVS(0x10, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndAnimatable)
    SAVE_SUPERCLASS(RndTransformable)
    SAVE_SUPERCLASS(RndDrawable)
    bs << mObjects;
    bs << mDrawOnly;
    bs << mSortInWorld;
END_SAVES

BEGIN_COPYS(RndGroup)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndAnimatable)
    COPY_SUPERCLASS(RndDrawable)
    COPY_SUPERCLASS(RndTransformable)
    CREATE_COPY(RndGroup)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mEnv)
        COPY_MEMBER(mDrawOnly)
        COPY_MEMBER(mLod)
        COPY_MEMBER(mLodScreenSize)
        COPY_MEMBER(mSortInWorld)
        if (ty == kCopyDeep)
            COPY_MEMBER(mObjects)
        else if (ty == kCopyFromMax)
            Merge(c);
    END_COPYING_MEMBERS
    Update();
END_COPYS

INIT_REVS(0x10, 0)

BEGIN_LOADS(RndGroup)
    LOAD_REVS(bs)
    ASSERT_REVS(0x10, 0)
    if (d.rev > 7) {
        LOAD_SUPERCLASS(Hmx::Object)
    }
    LOAD_SUPERCLASS(RndAnimatable)
    LOAD_SUPERCLASS(RndTransformable)
    LOAD_SUPERCLASS(RndDrawable)
    if (d.rev > 10) {
        bs >> mObjects;
        if (d.rev < 0x10) {
            ObjPtr<RndEnviron> env(this);
            bs >> env;
            if (env) {
                mObjects.push_back(env);
            }
        }
        if (d.rev > 0xC) {
            bs >> mDrawOnly;
        } else {
            mDrawOnly = nullptr;
        }
        Update();
    }
    if (d.rev > 0xB && d.rev < 0xF) {
        String str;
        float x;
        bs >> str;
        bs >> x;
    }
    if (d.rev > 0xD) {
        d >> mSortInWorld;
    }
END_LOADS

void RndGroup::StartAnim() {
    for (std::vector<RndAnimatable *>::iterator it = mAnims.begin(); it != mAnims.end();
         ++it) {
        (*it)->StartAnim();
    }
}

void RndGroup::EndAnim() {
    for (std::vector<RndAnimatable *>::iterator it = mAnims.begin(); it != mAnims.end();
         ++it) {
        (*it)->EndAnim();
    }
}

void RndGroup::SetFrame(float frame, float blend) {
    if (Showing()) {
        RndAnimatable::SetFrame(frame, blend);
        for (std::vector<RndAnimatable *>::iterator it = mAnims.begin();
             it != mAnims.end();
             ++it) {
            (*it)->SetFrame(frame, blend);
        }
    }
}

float RndGroup::EndFrame() {
    float end = 0;
    for (std::vector<RndAnimatable *>::iterator it = mAnims.begin(); it != mAnims.end();
         ++it) {
        MaxEq(end, (*it)->EndFrame());
    }
    return end;
}

void RndGroup::ListAnimChildren(std::list<RndAnimatable *> &children) const {
    children.insert(children.end(), mAnims.begin(), mAnims.end());
}

float RndGroup::GetDistanceToPlane(const Plane &p, Vector3 &v) {
    if (mDraws.empty())
        return 0;
    else {
        float ret = 0;
        bool first = true;
        for (std::vector<RndDrawable *>::iterator it = mDraws.begin(); it != mDraws.end();
             ++it) {
            Vector3 locvec;
            float dist = (*it)->GetDistanceToPlane(p, locvec);
            if (first || (std::fabs(dist) < std::fabs(ret))) {
                first = false;
                ret = dist;
                v = locvec;
            }
        }
        return ret;
    }
}

bool RndGroup::MakeWorldSphere(Sphere &s, bool b) {
    if (b) {
        s.Zero();
        for (std::vector<RndDrawable *>::iterator it = mDraws.begin(); it != mDraws.end();
             ++it) {
            Sphere local_s;
            (*it)->MakeWorldSphere(local_s, true);
            s.GrowToContain(local_s);
        }
        return true;
    } else {
        return false;
    }
}

void RndGroup::DrawShowing() {
    if (mDraws.empty())
        return;
    RndEnvironTracker tracker(mEnv, &WorldXfm().v);
    if (mDrawOnly) {
        mDrawOnly->Draw();
    } else if (!mSortInWorld) {
        for (std::vector<RndDrawable *>::iterator it = mDraws.begin(); it != mDraws.end();
             ++it) {
            (*it)->Draw();
        }
    } else {
        std::vector<GroupDrawDist> sorted;
        sorted.reserve(mDraws.size());
        const Transform &camXfm = RndCam::Current()->WorldXfm();
        const Vector3 &camXfmV = camXfm.v;
        for (std::vector<RndDrawable *>::iterator it = mDraws.begin(); it != mDraws.end();
             ++it) {
            RndTransformable *trans = dynamic_cast<RndTransformable *>(*it);
            Vector3 pos = trans ? trans->WorldXfm().v : Vector3(0.0f, 0.0f, 0.0f);
            Vector3 delta;
            Subtract(camXfmV, pos, delta);
            GroupDrawDist gdd;
            gdd.draw = *it;
            gdd.dist = LengthSquared(delta);
            sorted.push_back(gdd);
        }
        std::sort(sorted.begin(), sorted.end(), SortInWorld);
        for (std::vector<GroupDrawDist>::iterator it = sorted.begin(); it != sorted.end();
             ++it) {
            it->draw->Draw();
        }
    }
}

void RndGroup::Draw() {
    if (mShowing) {
        RndGroup::DrawShowing();
    }
}

void RndGroup::ListDrawChildren(std::list<RndDrawable *> &children) {
    children.insert(children.end(), mDraws.begin(), mDraws.end());
}

RndDrawable *RndGroup::CollideShowing(const Segment &seg, float &f, Plane &p) {
    RndDrawable *ret = nullptr;
    Segment localseg(seg);
    f = 1.0f;
    for (std::vector<RndDrawable *>::iterator it = mDraws.begin(); it != mDraws.end();
         ++it) {
        float locf;
        RndDrawable *collided = (*it)->Collide(localseg, locf, p);
        if (collided) {
            ret = collided;
            Interp(localseg.start, localseg.end, locf, localseg.end);
            f *= locf;
        }
    }
    return ret;
}

void RndGroup::CollideList(const Segment &seg, std::list<Collision> &colls) {
    if (CollideSphere(seg)) {
        for (std::vector<RndDrawable *>::iterator it = mDraws.begin(); it != mDraws.end();
             ++it) {
            (*it)->CollideList(seg, colls);
        }
    }
}

void RndGroup::Update() {
    mAnims.clear();
    mDraws.clear();
    for (ObjPtrList<Hmx::Object>::iterator it = mObjects.begin(); it != mObjects.end();
         ++it) {
        RndAnimatable *anim = dynamic_cast<RndAnimatable *>(*it);
        if (anim)
            mAnims.push_back(anim);
        RndDrawable *draw = dynamic_cast<RndDrawable *>(*it);
        if (draw)
            mDraws.push_back(draw);
    }
    if (mDrawOnly && !VectorFind(mDraws, mDrawOnly.Ptr())) {
        mDrawOnly = nullptr;
    }
}

void RndGroup::AddObject(Hmx::Object *o1, Hmx::Object *o2) {
    if (o1 && o1 != this) {
        if (mObjects.find(o1) != mObjects.end()) {
            if (!o2)
                return;
            RemoveObject(o1);
        }
        if (o2) {
            mObjects.insert(mObjects.find(o2), o1);
            Update();
        } else {
            mObjects.push_back(o1);
            RndAnimatable *anim = dynamic_cast<RndAnimatable *>(o1);
            if (anim)
                mAnims.push_back(anim);
            RndDrawable *draw = dynamic_cast<RndDrawable *>(o1);
            if (draw)
                mDraws.push_back(draw);
        }
    }
}

void RndGroup::RemoveObject(Hmx::Object *obj) {
    mObjects.remove(obj);
    VectorRemove(mDraws, obj);
    VectorRemove(mAnims, obj);
    if (mDrawOnly == obj && !gInReplace) {
        mDrawOnly = nullptr;
    }
}

void RndGroup::ClearObjects() {
    mObjects.clear();
    Update();
}

void RndGroup::Merge(const RndGroup *group) {
    if (group) {
        for (ObjPtrList<Hmx::Object>::iterator it = group->mObjects.begin();
             it != group->mObjects.end();
             ++it) {
            AddObject(*it);
        }
    }
}

void RndGroup::SortDraws() {
    for (std::vector<RndDrawable *>::iterator it = mDraws.begin(); it != mDraws.end();
         ++it) {
        mObjects.remove(*it);
    }
    std::sort(mDraws.begin(), mDraws.end(), ::SortDraws);
    for (std::vector<RndDrawable *>::iterator it = mDraws.begin(); it != mDraws.end();
         ++it) {
        mObjects.push_back(*it);
    }
    mAnims.clear();
    for (ObjPtrList<Hmx::Object>::iterator it = mObjects.begin(); it != mObjects.end();
         ++it) {
        RndAnimatable *anim = dynamic_cast<RndAnimatable *>(*it);
        if (anim)
            mAnims.push_back(anim);
    }
}

int RndGroup::CollidePlane(const Plane &p) {
    Sphere s;
    int ret = -1;
    bool first = false;
    auto _tmp1 = mDraws.end();
    for (std::vector<RndDrawable *>::iterator it = mDraws.begin(); it != _tmp1;
         ++it) {
        if ((*it)->Showing() & (*it)->MakeWorldSphere(s, false)) {
            if (!first) {
                ret = (*it)->CollidePlane(p);
                first = true;
            } else if (ret != (*it)->CollidePlane(p)) {
                return 0;
            }
        }
    }
    return ret;
}

int RndGroup::MoveObject(Hmx::Object *obj, int delta) {
    typedef ObjPtrList<Hmx::Object>::Node Node;
    Node *node;
    for (node = mObjects.mNodes; node != nullptr; node = node->next) {
        if (node->Obj() == obj)
            break;
    }
    if (!node) {
        return 0;
    }
    Node *target = node;
    int remaining = delta;
    if (delta > 0) {
        target = node->next;
        do {
            if (target == nullptr)
                break;
            target = target->next;
        } while (--remaining != 0);
    } else if (delta != 0) {
        do {
            if (target == mObjects.mNodes)
                break;
            target = target->prev;
            remaining++;
        } while (remaining != 0);
    }
    if (node != target) {
        mObjects.Unlink(node);
        mObjects.Link(ObjPtrList<Hmx::Object>::iterator(target), node);
    }
    Update();
    return delta - remaining;
}

DataNode RndGroup::GetGroupChildren() {
    DataArrayPtr ptr(new DataArray(mObjects.size()));
    int idx = 0;
    for (ObjPtrList<Hmx::Object>::iterator it = mObjects.begin(); it != mObjects.end();
         ++it, ++idx) {
        ptr->Node(idx) = *it;
    }
    return ptr;
}

DataNode RndGroup::OnGetDraws(DataArray *) {
    DataArray *ret = new DataArray(mDraws.size() + 1);
    ret->Node(0) = NULL_OBJ;
    int idx = 0;
    for (std::vector<RndDrawable *>::iterator it = mDraws.begin(); it != mDraws.end();
         ++it) {
        ret->Node(++idx) = *it;
    }
    DataNode retNode(ret, kDataArray);
    ret->Release();
    return retNode;
}

// sw2 scatter-include (default/Group <- math/Geo.cpp)
#define gRev gRev_Geo
#define gAltRev gAltRev_Geo
#include "math/Geo.cpp"
#undef gRev
#undef gAltRev

// sw2 scatter-include (default/Group <- obj/PropSync.cpp)
// ⚠ NATIVE: guarded because THIS INCLUDEE HAS MULTIPLE UNCONDITIONAL
// INCLUDERS in the native fork surface, which cmake/ScatterIncludes.cmake
// cannot resolve by pruning: its rule drops an includee that is emitted by an
// includer in the same target, and with N>1 includers that still leaves N
// copies. Guarding EVERY includer makes the edges inert natively, so
// obj/PropSync.cpp is compiled standalone exactly once -- which is the shape
// every native target had before X2 widened the glob. X360 is untouched: the
// scatter-include is a COMDAT-placement device for the match build, and it
// stays fully active there.
#ifndef HX_NATIVE
#define gRev gRev_PropSync
#define gAltRev gAltRev_PropSync
#include "obj/PropSync.cpp"
#undef gRev
#undef gAltRev
#endif

// sw2 scatter-include (default/Group <- rndobj/Flare.cpp)
#define gRev gRev_Flare
#define gAltRev gAltRev_Flare
#include "rndobj/Flare.cpp"
#undef gRev
#undef gAltRev
