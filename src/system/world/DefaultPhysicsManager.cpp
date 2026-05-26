#include "world/DefaultPhysicsManager.h"
#include "math/Geo.h"
#include "math/Mtx.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Draw.h"
#include "rndobj/Mesh.h"
#include "world/PhysicsManager.h"
#include "world/PhysicsVolume.h"

#pragma region RayCastDefaultContainer

RayCastDefaultContainer::RayCastDefaultContainer(
    const Box &box,
    std::list<RndMesh *> meshes,
    std::map<Hmx::Object *, ObjectDir *> &objMap
) {
    FOREACH (it, meshes) {
        RndMesh *d = *it;
        if (d) {
            Sphere s;
            if (d->MakeWorldSphere(s, false)) {
                if (box.Contains(s)) {
                    MILO_ASSERT(objMap.find(d) != objMap.end(), 0x73);
                    mMeshes.push_back(std::make_pair(d, objMap[d]));
                }
            }
        }
    }
}

Hmx::Object *RayCastDefaultContainer::FindNearest(
    const Segment &s, float &f, Vector3 &v, Hmx::Object *&o
) {
    o = nullptr;
    f = 1;
    Segment localSegment = s;
    Hmx::Object *ret = nullptr;
    FOREACH (it, mMeshes) {
        RndMesh *curMesh = it->first;
        Vector3 curVec;
        Plane curPlane;
        if (curMesh->Collide(localSegment, curVec.x, curPlane)) {
            float xScalar = curVec.x;
            Interp(localSegment.start, localSegment.end, curVec.x, localSegment.end);
            v = reinterpret_cast<Vector3 &>(curPlane);
            o = curMesh;
            ret = it->second;
            f *= xScalar;
        }
    }
    return ret;
}

void RayCastDefaultContainer::SetFilter(int) {
    MILO_NOTIFY("Filters are unsupported as yet");
}

#pragma endregion
#pragma region DefaultDetectionVolume

DefaultDetectionVolume::DefaultDetectionVolume(DetectionVolumeListener *dvl)
    : mListener(dvl), mActiveState(false) {}

#pragma endregion
#pragma region DefaultPhysicsManager

DefaultPhysicsManager::DefaultPhysicsManager(RndDir *d)
    : PhysicsManager(d), mCollidables(this, kObjListOwnerControl) {}

bool DefaultPhysicsManager::Replace(ObjRef *from, Hmx::Object *to) {
    if (from->Parent() != &mCollidables) {
        if (to == nullptr) {
            Hmx::Object *obj = from->GetObj();
            mCollidables.remove(obj);
            RemoveCollidable(obj);
        }
        return true;
    } else {
        return Hmx::Object::Replace(from, to);
    }
}

void DefaultPhysicsManager::Poll() {
    for (auto it = mActiveCollidables.begin(); it != mActiveCollidables.end();) {
        RndMesh *d = *it;
        if (!IsShowing(d)) {
            auto prev_it = it;
            ++it;
            mActiveCollidables.erase(prev_it);
            MILO_ASSERT(std::find( mInactiveCollidables.begin(), mInactiveCollidables.end(), d) == mInactiveCollidables.end(), 0x41);
            mInactiveCollidables.push_front(d);
        } else {
            ++it;
        }
    }
    for (auto it = mInactiveCollidables.begin(); it != mInactiveCollidables.end();) {
        RndMesh *d = *it;
        if (IsShowing(d)) {
            auto prev_it = it;
            ++it;
            mInactiveCollidables.erase(prev_it);
            MILO_ASSERT(std::find( mActiveCollidables.begin(), mActiveCollidables.end(), d) == mActiveCollidables.end(), 0x55);
            mActiveCollidables.push_front(d);
        } else {
            ++it;
        }
    }
}

RayCastContainer *DefaultPhysicsManager::MakeContainer(const Box &box, unsigned int ui) {
    return new RayCastDefaultContainer(box, mActiveCollidables, mCollidableDirs);
}

DetectionVolume *DefaultPhysicsManager::MakeDetectionVolume(
    DetectionVolumeListener *dvl, const Transform &, PhysicsVolumeType, CollisionFilter
) {
    return new DefaultDetectionVolume(dvl);
}

void DefaultPhysicsManager::CastRays(RayCast *, int) {
#ifndef HX_NATIVE
    MILO_FAIL("not implemented");
#endif
}

void DefaultPhysicsManager::CastRays(
    const Segment *segments, RayCastListener *rcl, int count, unsigned int ui4
) {
    int i = 0;
    while (i < count) {
        Segment localSegment = segments[i];
        float collideFloat = 1.0f;
        FOREACH (it, mActiveCollidables) {
            Plane curPlane;
            RndDrawable *drawable = (*it)->Collide(localSegment, collideFloat, curPlane);
            if (drawable) {
                Hmx::Object *obj = static_cast<Hmx::Object *>(drawable);
                float t = rcl->OnRayHit(obj, mCollidableDirs[obj], curPlane, collideFloat);
                Interp(localSegment.start, localSegment.end, t, localSegment.end);
            }
        }
        i = i + 1;
    }
}

void DefaultPhysicsManager::ActivateCollidable(Hmx::Object *o) {
    auto it = std::find(mInactiveCollidables.begin(), mInactiveCollidables.end(), o);
    if (it != mInactiveCollidables.end()) {
        RndMesh *mesh = *it;
        mInactiveCollidables.erase(it);
        mActiveCollidables.insert(mActiveCollidables.begin(), mesh);
    }
}

void DefaultPhysicsManager::DeactivateCollidable(Hmx::Object *o) {
    auto it = std::find(mInactiveCollidables.begin(), mInactiveCollidables.end(), o);
    if (it != mInactiveCollidables.end()) {
        RndMesh *mesh = *it;
        mActiveCollidables.erase(it);
        mInactiveCollidables.insert(mInactiveCollidables.begin(), mesh);
    }
}

void DefaultPhysicsManager::AddCollidable(Hmx::Object *o, ObjectDir *dir, bool active) {
    RndMesh *mesh = dynamic_cast<RndMesh *>(o);
    if (mesh) {
        if (mCollidableDirs.find(mesh) == mCollidableDirs.end()) {
            mCollidableDirs[mesh] = dir;
            if (active) {
                mActiveCollidables.insert(mActiveCollidables.begin(), mesh);
            } else {
                mInactiveCollidables.insert(mInactiveCollidables.begin(), mesh);
            }
            mCollidables.insert(mCollidables.begin(), o);
        }
    }
}

void DefaultPhysicsManager::RemoveCollidable(Hmx::Object *o) {
    std::map<Hmx::Object *, ObjectDir *>::iterator mapIt = mCollidableDirs.find(o);
    if (mapIt == mCollidableDirs.end()) {
        return;
    }
    auto it = std::find(mActiveCollidables.begin(), mActiveCollidables.end(), o);
    if (it != mActiveCollidables.end()) {
        mActiveCollidables.erase(it);
    } else {
        it = std::find(mInactiveCollidables.begin(), mInactiveCollidables.end(), o);
        if (it != mInactiveCollidables.end()) {
            mInactiveCollidables.erase(it);
        }
    }
    mCollidableDirs.erase(mapIt);
    mCollidables.remove(o);
}

void DefaultPhysicsManager::RemoveAll() {
    mCollidableDirs.clear();
    mActiveCollidables.clear();
    mInactiveCollidables.clear();
    mCollidables.clear();
}
