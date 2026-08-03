#include "char/Waypoint.h"
#include "char/CharInterest.h"
#include "math/Rand.h"
#include "math/Rot.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Draw.h"
#include "rndobj/Mesh.h"
#include "rndobj/Trans.h"

#ifdef HX_NATIVE
std::list<Waypoint *> *Waypoint::sWaypoints;
#endif

Waypoint::Waypoint()
    : mFlags(0), mRadius(12), mYRadius(0), mAngRadius(0), mPad(0), mStrictAngDelta(0),
      mStrictRadiusDelta(0), mConnections(this) {
    if (RandomFloat() < 0.5f) {
        sWaypoints->push_back(this);
    } else
        sWaypoints->push_front(this);
}

Waypoint::~Waypoint() {
    if (sWaypoints) {
        for (std::list<Waypoint *>::iterator it = sWaypoints->begin();
             it != sWaypoints->end();
             ++it) {
            if (*it == this) {
                sWaypoints->erase(it);
                break;
            }
        }
    }
}

BEGIN_HANDLERS(CharInterest)
    HANDLE_SUPERCLASS(RndTransformable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_HANDLERS(Waypoint)
    HANDLE_SUPERCLASS(RndTransformable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(Waypoint)
    SYNC_PROP(flags, mFlags)
    SYNC_PROP(radius, mRadius)
    SYNC_PROP(y_radius, mYRadius)
    SYNC_PROP_SET(ang_radius, mAngRadius * RAD2DEG, mAngRadius = _val.Float() * DEG2RAD)
    SYNC_PROP(strict_radius_delta, mStrictRadiusDelta)
    SYNC_PROP_SET(
        strict_ang_delta,
        mStrictAngDelta * RAD2DEG,
        mStrictAngDelta = _val.Float() * DEG2RAD
    )
    SYNC_PROP(connections, mConnections)
    SYNC_SUPERCLASS(RndTransformable)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(Waypoint)
    SAVE_REVS(5, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndTransformable)
    bs << mFlags;
    bs << mConnections;
    bs << mRadius;
    bs << mYRadius;
    bs << mAngRadius;
    bs << mStrictRadiusDelta;
    bs << mStrictAngDelta;
END_SAVES

BEGIN_COPYS(Waypoint)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndTransformable)
    CREATE_COPY(Waypoint)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mFlags)
        COPY_MEMBER(mConnections)
        COPY_MEMBER(mRadius)
        COPY_MEMBER(mYRadius)
        COPY_MEMBER(mAngRadius)
        COPY_MEMBER(mStrictRadiusDelta)
        COPY_MEMBER(mStrictAngDelta)
    END_COPYING_MEMBERS
END_COPYS

// RB3-360 retail rev dialect (rb3-Wii/ObjMacros shape): the packed rev is split
// into two HALFWORDS stored four bytes apart onto ONE internal-linkage align(4)
// base, and the RAW incoming BinStream is forwarded to every read and to the
// superclass Load.  DC3's Object.h BinStreamRev stack decorator additionally
// emits ??0BinStream, a ??_7BinStreamRev@@6B@ vtable store and a ??1BinStream
// destructor that retail has none of, and dispatches each read on `&d`.
//
// Written longhand rather than by including obj/ObjMacros.h: that header also
// swaps the SYNC_PROP and HANDLE families, which are already byte-exact here.
// The pair MUST share one aggregate -- two separate file statics are laid out
// independently and will not fold onto a single base register.  No `#define
// gRev` alias: several of these TUs are scatter-INCLUDED into another unit
// (e.g. rndobj/Anim.cpp includes rndobj/MotionBlur.cpp) whose own gRev macro
// the alias would silently shadow for the rest of the amalgamated TU.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_Waypoint;
BEGIN_LOADS(Waypoint)
    int rev;
    bs >> rev;
    gRevs_Waypoint.rev = getHmxRev(rev);
    gRevs_Waypoint.altRev = getAltRev(rev);
    Hmx::Object::Load(bs);
    if (5 > gRevs_Waypoint.rev) {
        RndMesh *mesh = Hmx::Object::New<RndMesh>();
        mesh->RndDrawable::Load(bs);
        if (mesh) {
            delete mesh;
        }
    }
    RndTransformable::Load(bs);
    bs >> mFlags;
    bs >> mConnections;
    if (1 < gRevs_Waypoint.rev) {
        bs >> mRadius;
    } else
        mRadius = 12;
    if (2 < gRevs_Waypoint.rev) {
        bs >> mYRadius;
        bs >> mAngRadius;
    }
    if (3 < gRevs_Waypoint.rev) {
        bs >> mStrictRadiusDelta;
        bs >> mStrictAngDelta;
    }
END_LOADS

void Waypoint::Init() {
    REGISTER_OBJ_FACTORY(Waypoint);
    DataRegisterFunc("waypoint_find", OnWaypointFind);
    DataRegisterFunc("waypoint_nearest", OnWaypointNearest);
    DataRegisterFunc("waypoint_last", OnWaypointLast);
    sWaypoints = new std::list<Waypoint *>();
    TheDebug.AddExitCallback(Waypoint::Terminate);
}

void Waypoint::Terminate() { RELEASE(sWaypoints); }

Waypoint *Waypoint::Find(int flags2) {
    for (std::list<Waypoint *>::iterator i = sWaypoints->begin(); i != sWaypoints->end();
         ++i) {
        if ((*i)->mFlags & flags2)
            return *i;
    }
    return nullptr;
}

DataNode Waypoint::OnWaypointFind(DataArray *da) { return Waypoint::Find(da->Int(1)); }

DataNode Waypoint::OnWaypointNearest(DataArray *da) {
    return FindNearest(da->Obj<RndTransformable>(1)->WorldXfm().v, da->Int(2));
}

Waypoint *Waypoint::FindNearest(const Vector3 &pos, int flags) {
    Waypoint *best = nullptr;
    float bestDist = 1e30;
    for (std::list<Waypoint *>::iterator it = sWaypoints->begin(); it != sWaypoints->end();
         ++it) {
        Waypoint *wp = *it;
        if (wp->mFlags & flags) {
            const Vector3 &wpPos = wp->WorldXfm().v;
            float dy = pos.y - wpPos.y;
            float dz = pos.z - wpPos.z;
            float dx = pos.x - wpPos.x;
            float dist = (dy * dy + (dx * dx + dz * dz));
            if (bestDist > dist) {
                bestDist = dist;
                best = wp;
            }
        }
    }
    return best;
}

DataNode Waypoint::OnWaypointLast(DataArray *da) {
    Waypoint *wp = da->Obj<Waypoint>(1);
    for (std::list<Waypoint *>::iterator it = sWaypoints->begin(); it != sWaypoints->end();
         ++it) {
        if (*it == wp) {
            sWaypoints->splice(sWaypoints->end(), *sWaypoints, it);
            break;
        }
    }
    return DataNode(0);
}

void Waypoint::Highlight() {}

void Waypoint::Constrain(Transform &xfm) {
    float strictRadius = mStrictRadiusDelta;
    if (0.0f < strictRadius) {
        float yR = (0.0f < mYRadius) ? mYRadius + strictRadius : 0.0f;
        Vector3 delta;
        ShapeDeltaBox(xfm.v, mRadius + strictRadius, yR, delta);
        xfm.v += delta;
    }
    if (mStrictAngDelta > 0.0f) {
        float ang = GetZAngle(xfm.m);
        float deltaAng = ShapeDeltaAng(mAngRadius + mStrictAngDelta, ang);
        RotateAboutZ(xfm.m, deltaAng, xfm.m);
    }
}

float Waypoint::ShapeDeltaAng(float f1, float f2) {
    float limited = LimitAng(GetZAngle(WorldXfm().m) - f2);
    float clamped = Clamp(-f1, f1, limited);
    return limited - clamped;
}

float Waypoint::ShapeDelta(float f) { return ShapeDeltaAng(mAngRadius, f); }

void Waypoint::ShapeDelta(const Vector3 &v, Vector3 &vout) {
    ShapeDeltaBox(v, mRadius, mYRadius, vout);
}

void Waypoint::ShapeDeltaBox(const Vector3 &v1, float f1, float f2, Vector3 &res) {
    const Transform &world = WorldXfm();
    if (f2 > 0.0f) {
        Subtract(v1, WorldXfm().v, res);
        float dotx = Dot(res, world.m.x);
        float doty = Dot(world.m.y, res);
        float clamped1 = Clamp(-f1, f1, dotx);
        float clamped2 = Clamp(-f2, f2, doty);
        Scale(world.m.x, clamped1 - dotx, res);
        ScaleAdd(res, world.m.y, clamped2 - doty, res);
    } else {
        Subtract(WorldXfm().v, v1, res);
        res.z = 0;
        float lensq = LengthSquared(res);
        if (lensq <= f1 * f1)
            res.Zero();
        else
            res *= 1.0f - (f1 / sqrtf(lensq));
    }
}

// sw2 scatter-include (default/Waypoint <- synth/Sequence.cpp)
#define gRev gRev_Sequence
#define gAltRev gAltRev_Sequence
#include "synth/Sequence.cpp"
#undef gRev
#undef gAltRev
