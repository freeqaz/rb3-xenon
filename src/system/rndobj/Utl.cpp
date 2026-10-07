#include "Rnd.h"
#include "math/Color.h"
#include "math/Mtx.h"
#include "math/Utl.h"
#include "math/Vec.h"
#include "obj/DataFunc.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/Endian.h"
#include "os/FileCache.h"
#include "os/HolmesClient.h"
#include "os/Platform.h"
#include "os/System.h"
#include "rndobj/AmbientOcclusion.h"
#include "rndobj/Bitmap.h"
#include "rndobj/Cam.h"
#include "rndobj/CamAnim.h"
#include "rndobj/Dir.h"
#include "rndobj/Draw.h"
#include "rndobj/Env.h"
#include "rndobj/Flare.h"
#include "rndobj/Group.h"
#include "rndobj/Line.h"
#include "rndobj/Mat.h"
#include "rndobj/Mesh.h"
#include "rndobj/MeshAnim.h"
#include "rndobj/MetaMaterial.h"
#include "rndobj/Morph.h"
#include "rndobj/Part.h"
#include "rndobj/PartAnim.h"
#include "rndobj/Tex.h"
#include "utl/Cache.h"
#include "utl/Std.h"
#include "rndobj/Utl.h"
#include "rndobj/ShaderMgr.h"
#include "rndobj/LitAnim.h"
#include "rndobj/Gen.h"
#include "rndobj/MatAnim.h"
#include <set>
#include "math/Key.h"
#include <math.h>
#include "os/File.h"
#include "obj/Data.h"
#include "obj/Utl.h"

#include "math/Rand.h"

// Retail evaluated MILO_{NOTIFY,NOTIFY_ONCE} args (side-effecting calls
// like PathName() survive, pure accessors are DCE'd) rather than dropping them
// entirely via the global sizeof (unevaluated) no-op.
// MILO_WARN is intentionally NOT overridden here: Debug.h's global definition
// already routes WARN through MiloStripEval (by-value params force the copy
// ctor + destructible temp retail's stripped WARN residue shows), verified
// +33 strict whole-binary (see Debug.h comment). TestTexturePaths's two
// "%s: %s is outside project path" MILO_WARN(...) sites copy-construct the
// `relative` String temp in retail (0x8243e988) -- overriding WARN to the
// comma form here dropped that copy + reordered the PathName() call.
#undef MILO_NOTIFY_ONCE
#define MILO_NOTIFY_ONCE(...) ((void)(__VA_ARGS__))
#undef MILO_NOTIFY
#define MILO_NOTIFY(...) ((void)(__VA_ARGS__))

typedef void (*SplashFunc)(void);

class ResourceFileCacheHelper : public FileCacheHelper {
public:
    virtual const char *CacheFile(const char *);
};

ResourceFileCacheHelper gResourceFileCacheHelper;
float gLimitUVRange;
int gDxtCacher;
// Retail keeps sSphereDir in the word below sSphereMesh (0x82CC2AD0/0x82CC2AD4):
// RndUtlTerminate reaches it at -4 off sSphereMesh. The explicit zero-init makes
// declaration order the .bss order; uninitialised, the code generator places them.
static ObjectDir *sSphereDir = nullptr;
static RndMesh *sSphereMesh = nullptr;
static ObjectDir *sCylinderDir;
static RndMesh *sCylinderMesh;
std::list<BuildPoly> gChildPolys;
std::list<BuildPoly> gParentPolys;
SplashFunc gSplashPoll;
SplashFunc gSplashSuspend;
SplashFunc gSplashResume;
Vector3 gUtlXfms;

RndGroup *GroupOwner(Hmx::Object *o) {
    FOREACH (it, o->Refs()) {
#ifdef HX_NATIVE
        RndGroup *grp = dynamic_cast<RndGroup *>(it->RefOwner());
#else
        // X360: mRefs entries are the ring-refs; each carries RefOwner().
        RndGroup *grp = dynamic_cast<RndGroup *>(RefPtrOf(it)->RefOwner());
#endif
        if (grp) {
            if (grp->HasObject(o)) {
                return grp;
            }
        }
    }
    return nullptr;
}

DataNode OnGroupOwner(DataArray *da) { return GroupOwner(da->Obj<Hmx::Object>(1)); }

RndEnviron *FindEnviron(RndDrawable *d) {
    RndGroup *owner = GroupOwner(d);
    if (owner) {
        // a group's own environment wins; otherwise ask the group's owner
        RndEnviron *env = owner->GetEnv();
        if (env)
            return env;
        return FindEnviron(owner);
    } else {
        RndDir *rdir = dynamic_cast<RndDir *>(d->Dir());
        if (rdir) {
            std::list<RndDrawable *> children;
            rdir->ListDrawChildren(children);
            if (ListFind(children, d)) {
                return rdir->GetEnv();
            }
        }
        MILO_NOTIFY("Need to find environment of draw parent");
    }
    return nullptr;
}

DataNode DataFindEnviron(DataArray *da) { return FindEnviron(da->Obj<RndDrawable>(1)); }

bool GroupedUnder(RndGroup *grp, Hmx::Object *o) {
    FOREACH (it, grp->Objects()) {
        if (*it == o)
            return true;
        RndGroup *casted = dynamic_cast<RndGroup *>(*it);
        if (casted && GroupedUnder(casted, o))
            return true;
    }
    return false;
}

void SetRndSplasherCallback(
    SplashFunc pollFunc, SplashFunc suspendFunc, SplashFunc resumeFunc
) {
    gSplashPoll = pollFunc;
    gSplashSuspend = suspendFunc;
    gSplashResume = resumeFunc;
}

void RndSplasherPoll() {
    if (gSplashPoll)
        gSplashPoll();
}

void RndSplasherSuspend() {
    if (gSplashSuspend)
        gSplashSuspend();
}

void RndSplasherResume() {
    if (gSplashResume)
        gSplashResume();
}

const char *CacheResource(const char *, CacheResourceResult &);

Loader *ResourceFactory(const FilePath &f, LoaderPos p) {
    return new FileLoader(f, CacheResource(f.c_str(), nullptr), p, 0, false, true, nullptr);
}

void RndUtlPreInit() {
    SystemConfig("rnd")->FindData("limit_uv_range", gLimitUVRange, true);
    TheLoadMgr.RegisterFactory("bmp", ResourceFactory);
    TheLoadMgr.RegisterFactory("png", ResourceFactory);
    TheLoadMgr.RegisterFactory("xbv", ResourceFactory);
    TheLoadMgr.RegisterFactory("jpg", ResourceFactory);
    TheLoadMgr.RegisterFactory("tif", ResourceFactory);
    TheLoadMgr.RegisterFactory("tiff", ResourceFactory);
    TheLoadMgr.RegisterFactory("psd", ResourceFactory);
    TheLoadMgr.RegisterFactory("gif", ResourceFactory);
    TheLoadMgr.RegisterFactory("tga", ResourceFactory);
    DataRegisterFunc("find_environ", DataFindEnviron);
    DataRegisterFunc("group_owner", OnGroupOwner);
}

// Retail's RndUtlInit is an empty function: Rnd::Init's call to it lands on
// the shared empty-body survivor (0x826C3888), and the image carries no
// "sphere.milo"/"cylinder.milo" path, so nothing is loaded and no resource
// cache helper is registered. Native keeps the loader.
void RndUtlInit() {
#ifdef HX_NATIVE
    FileCache::RegisterResourceCacheHelper(&gResourceFileCacheHelper);
    if (!UsingCD()) {
        sCylinderDir = DirLoader::LoadObjects(
            FilePath(FileSystemRoot(), "rndobj/cylinder.milo"), 0, 0
        );
    }
    sSphereDir =
        DirLoader::LoadObjects(FilePath(FileSystemRoot(), "rndobj/sphere.milo"), 0, 0);
    if (sSphereDir) {
        sSphereMesh = sSphereDir->Find<RndMesh>("sphere.mesh", true);
    }
    if (sCylinderDir) {
        sCylinderMesh = sSphereDir->Find<RndMesh>("Cylinder.mesh", true);
    }
#endif
}

// Retail (0x824399C0, called only from TerminateCallback) releases the sphere
// dir and clears the sphere dir/mesh pair; there is no cylinder pair.
void RndUtlTerminate() {
    if (sSphereDir) {
        delete sSphereDir;
    }
    sSphereDir = 0;
    sSphereMesh = 0;
#ifdef HX_NATIVE
    if (sCylinderDir) {
        delete sCylinderDir;
    }
    sCylinderDir = 0;
    sCylinderMesh = 0;
#endif
}

MatShaderOptions GetDefaultMatShaderOpts(const Hmx::Object *obj, RndMat *mat) {
    MatShaderOptions opts;
    const RndMesh *mesh = dynamic_cast<const RndMesh *>(obj);
    if (mesh) {
        if (mesh->Mat() == mat) {
            opts.SetLast5(0x12);
            auto _tmp4 = mesh->NumBones();
            opts.SetHasBones(_tmp4 != (int)0);
            opts.SetHasAOCalc(mesh->HasAOCalc());
        }
    } else {
        const RndMultiMesh *multimesh = dynamic_cast<const RndMultiMesh *>(obj);
        if (multimesh) {
            const RndMesh *mesh = multimesh->Mesh();
            if (mesh && mesh->Mat()) {
                if (mesh->Mat() == mat) {
                    int mask = mesh->TransConstraint()
                            == RndTransformable::kConstraintFastBillboardXYZ
                        ? 0xD
                        : 0xC;
                    opts.SetLast5(mask);
                    opts.SetHasBones(false);
                    opts.SetHasAOCalc(mesh->HasAOCalc());
                }
            }
        } else {
            const RndParticleSys *partSys = dynamic_cast<const RndParticleSys *>(obj);
            if (partSys) {
                if (partSys->GetMat() == mat) {
                    opts.SetLast5(0xE);
                }
            } else {
                const RndFlare *flare = dynamic_cast<const RndFlare *>(obj);
                if (flare) {
                    if (flare->GetMat() == mat) {
                        opts.SetLast5(6);
                    }
                }
            }
        }
    }
    return opts;
}

// noinline: retail keeps this out of line (0x82439968) even though its only
// caller, CacheResource, is in this TU and /O1 would otherwise inline it.
__declspec(noinline) const char *MovieExtension(const char *name, Platform p) {
    const char *ext;
    if (stricmp(name, "xbv") == 0) {
        // xbox, pc, ps3, or wii only
        if (p >= kPlatformXBox && p <= kPlatformWii) {
            return "xbv";
        }
        return name;
    } else
        return nullptr;
}

float ConvertFov(float a, float b) {
    float x = tanf(0.5f * a);
    return atanf(b * x) * 2;
}

void PreMultiplyAlpha(Hmx::Color &c) {
    c.red *= c.alpha;
    c.green *= c.alpha;
    c.blue *= c.alpha;
}

int GenerationCount(RndTransformable *t1, RndTransformable *t2) {
    if (t1 && t2) {
        int count = 0;
        for (; t2 != nullptr; t2 = t2->TransParent()) {
            if (t2 == t1)
                return count;
            count++;
        }
    }
    return 0;
}

RndAnimatable *AnimController(Hmx::Object *o) {
    // The natural form is FOREACH_OBJREF (reverse walk of a std::vector<ObjRef*>).
    // This tree's Hmx::Object::Refs() is a DC3-era intrusive next/prev ring with
    // only a forward iterator (see UIFontImporter::FindFontForMat for the same
    // adaptation), so this walks forward via RefPtrOf() instead.
    FOREACH (it, o->Refs()) {
#ifdef HX_NATIVE
        RndAnimatable *a = dynamic_cast<RndAnimatable *>(it->RefOwner());
#else
        RndAnimatable *a = dynamic_cast<RndAnimatable *>(RefPtrOf(it)->RefOwner());
#endif
        if (a && a->AnimTarget() == o)
            return a;
    }
    return nullptr;
}

void AddMotionSphere(RndTransformable *t, Sphere &s) {
    RndTransAnim *anim = dynamic_cast<RndTransAnim *>(AnimController(t));
    if (anim) {
        Sphere s_loc;
        CalcSphere(anim, s_loc);
        if (s_loc.GetRadius()) {
            if (s.GetRadius()) {
                s.radius += s_loc.GetRadius();
                Subtract(s.center, t->WorldXfm().v, s.center);
                Add(s_loc.center, s.center, s.center);
            } else
                s = s_loc;
        }
    }
    RndTransformable *parent = t->TransParent();
    if (parent)
        AddMotionSphere(parent, s);
}

#ifdef HX_NATIVE
// No-op shim -- see rndobj/Utl.h. Retail has no such function.
void CreateAndSetMetaMat(RndMat *) {}
#endif

bool ShouldStrip(RndTransformable *trans) {
    if (!trans)
        return false;
    const char *name = trans->Name();
    if (!name)
        return false;
    return strnicmp("bone_", name, 5) == 0 || strnicmp("exo_", name, 4) == 0
        || strncmp("spot_", name, 5) == 0;
}

bool AnimContains(const RndAnimatable *anim1, const RndAnimatable *anim2) {
    if (anim1 == anim2)
        return true;
    else {
        std::list<RndAnimatable *> children;
        anim1->ListAnimChildren(children);
        FOREACH (it, children) {
            if (AnimContains(*it, anim2))
                return true;
        }
        return false;
    }
}

RndMat *GetMat(RndDrawable *draw) {
    std::list<RndMat *> mats;
    draw->Mats(mats, false);
    RndMat *ret;
    if (mats.empty())
        ret = 0;
    else
        ret = mats.front();
    return ret;
}

bool SortDraws(RndDrawable *draw1, RndDrawable *draw2) {
#ifdef HX_NATIVE
    // NullifyAllRefs() can null ObjPtrList entries during cascade destruction.
    // Sort nulls to the end so they can be cleaned up after.
    if (!draw1 || !draw2)
        return draw1 > draw2;
#endif
    if (draw1->GetOrder() != draw2->GetOrder())
        return draw1->GetOrder() < draw2->GetOrder();
    else {
        RndMat *mat1 = GetMat(draw1);
        RndMat *mat2 = GetMat(draw2);
        if (mat1 != mat2) {
            return mat1 < mat2;
        } else
            return strcmp(draw1->Name(), draw2->Name()) < 0;
    }
}

// 0x82439CB0, the comparator RndDir::SyncObjects passes to std::sort
// (0x824060A0). RB3 polls CharTransCopy objects ahead of everything else, then
// orders by name; there is no PollEnabled test. Read off retail: a guarded
// function-local Symbol, a 0.0/1.0 key per side from ClassName(), fcmpu, and a
// strcmp of the names on ties.
bool SortPolls(const RndPollable *p1, const RndPollable *p2) {
    static Symbol charTransCopy("CharTransCopy");
    float order1 = p1->ClassName() == charTransCopy ? 0.0f : 1.0f;
    float order2 = p2->ClassName() == charTransCopy ? 0.0f : 1.0f;
    if (order1 != order2) {
        return order1 < order2;
    } else {
        return strcmp(p1->Name(), p2->Name()) < 0;
    }
}

bool LeftHanded(const Hmx::Matrix3 &m) {
    Vector3 cross;
    Cross(m.x, m.y, cross);
    float det = Dot(m.z, cross);
    return det < 0;
}

float AngleBetween(const Hmx::Quat &q1, const Hmx::Quat &q2) {
    Hmx::Quat qtmp;
    Negate(q1, qtmp);
    Multiply(q2, qtmp, qtmp);
    if (qtmp.w > 1.0f) {
        return 0;
    } else {
        return acosf(qtmp.w) * 2.0f;
    }
}

bool BadUV(Vector2 &v) {
    bool xIsNaN = v.x != v.x;
    if (xIsNaN)
        return true;
    bool yIsNaN = v.y != v.y;
    if (yIsNaN)
        return true;

    if (fabsf(v.x) > 1000.0f || fabsf(v.y) > 1000.0f) {
        return true;
    }

    bool xIsSmall = fabsf(v.x) < 0.0001f;
    if (xIsSmall) {
        v.x = 0;
    }
    bool yIsSmall = fabsf(v.y) < 0.0001f;
    if (yIsSmall) {
        v.y = 0;
    }

    return false;
}

void SetLocalScale(RndTransformable *t, const Vector3 &vec) {
    Hmx::Matrix3 m;
    Normalize(t->LocalXfm().m, m);
    Scale(vec, m, m);
    t->SetLocalRot(m);
}

void CalcBox(RndMesh *m, Box &b) {
    FOREACH (it, m->Verts()) {
        Vector3 vec;
        Multiply(it->pos, m->WorldXfm(), vec);
        b.GrowToContain(vec, it == m->Verts().begin());
    }
}

void ClearAO(RndMesh *m) {
    if (m->HasAOCalc()) {
        for (uint i = 0; i < m->Verts().size(); i++) {
            m->Verts(i).color.Set(1, 1, 1, 1);
        }
        m->SetHasAOCalc(false);
        m->Sync(0x1F);
    }
}

void ListDrawGroups(RndDrawable *draw, ObjectDir *dir, std::list<RndGroup *> &gList) {
    for (ObjDirItr<RndGroup> it(dir, true); it != 0; ++it) {
        if (VectorFind(it->Draws(), draw)) {
            gList.push_back(it);
        }
    }
}

void ResetColors(std::vector<Hmx::Color> &colors, int newNumColors) {
    Hmx::Color reset(1, 1, 1, 1);
    colors.resize(newNumColors);
    for (int i = 0; i < newNumColors; i++) {
        colors[i] = reset;
    }
}

void UtilDrawString(const char *c, const Vector3 &v, const Hmx::Color &col) {
    Vector2 v2;
    if (RndCam::Current()->WorldToScreen(v, v2) > 0) {
        v2.x *= TheRnd.Width();
        v2.y *= TheRnd.Height();
        TheRnd.DrawString(c, v2, col, true);
    }
}

void UtilDrawBox(const Transform &tf, const Box &box, const Hmx::Color &col, bool b4) {
    Vector3 vecs[8] = { Vector3(box.mMin.x, box.mMin.y, box.mMin.z),
                        Vector3(box.mMin.x, box.mMax.y, box.mMin.z),
                        Vector3(box.mMax.x, box.mMax.y, box.mMin.z),
                        Vector3(box.mMax.x, box.mMin.y, box.mMin.z),
                        Vector3(box.mMin.x, box.mMin.y, box.mMax.z),
                        Vector3(box.mMin.x, box.mMax.y, box.mMax.z),
                        Vector3(box.mMax.x, box.mMax.y, box.mMax.z),
                        Vector3(box.mMax.x, box.mMin.y, box.mMax.z) };
    for (int i = 0; i < 8; i++) {
        Multiply(vecs[i], tf, vecs[i]);
    }
    TheRnd.DrawLine(vecs[0], vecs[1], col, b4);
    TheRnd.DrawLine(vecs[1], vecs[2], col, b4);
    TheRnd.DrawLine(vecs[2], vecs[3], col, b4);
    TheRnd.DrawLine(vecs[3], vecs[0], col, b4);

    TheRnd.DrawLine(vecs[0], vecs[4], col, b4);
    TheRnd.DrawLine(vecs[1], vecs[5], col, b4);
    TheRnd.DrawLine(vecs[2], vecs[6], col, b4);
    TheRnd.DrawLine(vecs[3], vecs[7], col, b4);

    TheRnd.DrawLine(vecs[4], vecs[5], col, b4);
    TheRnd.DrawLine(vecs[5], vecs[6], col, b4);
    TheRnd.DrawLine(vecs[6], vecs[7], col, b4);
    TheRnd.DrawLine(vecs[7], vecs[4], col, b4);
}

void UtilDrawAxes(const Transform &tf, float f, const Hmx::Color &c) {
    Vector3 vec38;
    Hmx::Color c48;
    ScaleAdd(tf.v, tf.m.x, f, vec38);
    Interp(c, Hmx::Color(1, 0, 0), 0.8f, c48);
    TheRnd.DrawLine(tf.v, vec38, c48, false);

    ScaleAdd(tf.v, tf.m.y, f, vec38);
    Interp(c, Hmx::Color(0, 1, 0), 0.8f, c48);
    TheRnd.DrawLine(tf.v, vec38, c48, false);

    ScaleAdd(tf.v, tf.m.z, f, vec38);
    Interp(c, Hmx::Color(0, 0, 1), 0.8f, c48);
    TheRnd.DrawLine(tf.v, vec38, c48, false);
}

void UtilDrawLine(const Vector2 &v1, const Vector2 &v2, const Hmx::Color &color) {
    RndCam *cam = RndCam::Current();
    float planeRatio = (cam->FarPlane() - cam->NearPlane()) / 10.0f + cam->NearPlane();
    Vector3 v3_1, v3_2;
    cam->ScreenToWorld(v1, planeRatio, v3_1);
    cam->ScreenToWorld(v2, planeRatio, v3_2);
    TheRnd.DrawLine(v3_1, v3_2, color, false);
}

void UtilDrawRect2D(const Vector2 &v1, const Vector2 &v2, const Hmx::Color &color) {
    Vector2 cross1(v2.x, v1.y);
    Vector2 cross2(v1.x, v2.y);
    UtilDrawLine(v1, cross1, color);
    UtilDrawLine(cross1, v2, color);
    UtilDrawLine(v2, cross2, color);
    UtilDrawLine(cross2, v1, color);
}

void UtilDrawCircle2D(
    const Vector2 &center, float radius, const Hmx::Color &color, int segments
) {
    std::vector<Vector2> pts(segments + 1);
    float aspect = TheRnd.YRatio();
    for (int i = 0; i <= segments; i++) {
        float angle = (float)i * 6.2831854820251465f / (float)segments;
        float cosVal = FastSin(angle + 1.5707963705062866f);
        float sinVal = FastSin(angle);
        pts[i].x = cosVal * aspect * radius + center.x;
        pts[i].y = sinVal * radius + center.y;
    }
    for (int i = 0; i < segments; i++) {
        UtilDrawLine(pts[i], pts[i + 1], color);
    }
}

void CalcSphere(RndTransAnim *a, Sphere &s) {
    s.Zero();
    if (!a->TransKeys().empty()) {
        RndTransformable *trans = a->Trans() ? a->Trans()->TransParent() : nullptr;
        Box box;
        Vector3 vec;
        FOREACH (it, a->TransKeys()) {
            if (trans) {
                Multiply(it->value, trans->WorldXfm(), vec);
            } else
                vec = it->value;
            box.GrowToContain(vec, it == a->TransKeys().begin());
        }
        Vector3 vres;
        CalcBoxCenter(vres, box);
        Subtract(box.mMax, vres, vec);
        Vector3 vsphere;
        float fmax = Max(vec.x, vec.y, vec.z);
        CalcBoxCenter(vsphere, box);
        s.Set(vsphere, fmax);
    }
}

// MSVC X360 cl 16.00.10224 loses the *memory provenance* of a value returned by
// an inlined accessor: a loop bound written `c.end()` becomes a loop-invariant
// value and is hoisted, while the same member read directly stays a memory
// reference that the loop's `stfs` is assumed to kill, so it is reloaded every
// iteration. Retail has the reloaded (memory-reference) form -- see the probe
// pair in this lane's notes. Reading _M_start/_M_finish through this alias
// reproduces it. Layout-checked: STLport _Vector_base is {_M_start, _M_finish,
// _M_end_of_storage} with no vptr.
template <class T>
struct VecRaw {
    T *start;
    T *finish;
    T *eos;
};
#define RB3_VEC_RAW(T, expr) (reinterpret_cast<VecRaw<T> &>(expr))

void SpliceKeys(
    RndTransAnim *anim1, RndTransAnim *anim2, float firstFrame, float lastFrame
) {
    float start = anim1->StartFrame();
    float end = anim1->EndFrame();
    if (start < 0.0f || end > lastFrame)
        MILO_NOTIFY("%s has keyframes outside (0, %f)", anim1->Name(), lastFrame);
    else {
        RndTransformable *trans = anim1->Trans();
        if (!anim1->TransKeys().empty()) {
            if (anim1->TransKeys().front().frame != 0.0f) {
                anim1->TransKeys().Add(anim1->TransKeys().front().value, 0.0f, false);
            }
            if (anim1->TransKeys().back().frame != lastFrame) {
                anim1->TransKeys().Add(anim1->TransKeys().back().value, lastFrame, false);
            }
        } else if (trans) {
            anim1->TransKeys().Add(trans->LocalXfm().v, 0.0f, false);
            anim1->TransKeys().Add(trans->LocalXfm().v, lastFrame, false);
        } else {
            anim1->TransKeys().Add(Vector3(0.0f, 0.0f, 0.0f), 0.0f, false);
            anim1->TransKeys().Add(Vector3(0.0f, 0.0f, 0.0f), lastFrame, false);
        }

        if (!anim1->RotKeys().empty()) {
            if (anim1->RotKeys().front().frame != 0.0f) {
                anim1->RotKeys().Add(anim1->RotKeys().front().value, 0.0f, false);
            }
            if (anim1->RotKeys().back().frame != lastFrame) {
                anim1->RotKeys().Add(anim1->RotKeys().back().value, lastFrame, false);
            }
        } else if (trans) {
            Hmx::Quat q(trans->LocalXfm().m);
            anim1->RotKeys().Add(q, 0.0f, false);
            anim1->RotKeys().Add(q, lastFrame, false);
        } else {
            anim1->RotKeys().Add(Hmx::Quat(0.0f, 0.0f, 0.0f, 1.0f), 0.0f, false);
            anim1->RotKeys().Add(Hmx::Quat(0.0f, 0.0f, 0.0f, 1.0f), lastFrame, false);
        }

        if (!anim1->ScaleKeys().empty()) {
            if (anim1->ScaleKeys().front().frame != 0.0f) {
                anim1->ScaleKeys().Add(anim1->ScaleKeys().front().value, 0.0f, false);
            }
            if (anim1->ScaleKeys().back().frame != lastFrame) {
                anim1->ScaleKeys().Add(anim1->ScaleKeys().back().value, lastFrame, false);
            }
        } else if (trans) {
            Vector3 v;
            MakeScale(trans->LocalXfm().m, v);
            anim1->ScaleKeys().Add(v, 0.0f, false);
            anim1->ScaleKeys().Add(v, lastFrame, false);
        } else {
            anim1->ScaleKeys().Add(Vector3(1.0f, 1.0f, 1.0f), 0.0f, false);
            anim1->ScaleKeys().Add(Vector3(1.0f, 1.0f, 1.0f), lastFrame, false);
        }

        for (Key<Vector3> *it = RB3_VEC_RAW(Key<Vector3>, anim1->TransKeys()).start;
             it != RB3_VEC_RAW(Key<Vector3>, anim1->TransKeys()).finish;
             it++) {
            (*it).frame += firstFrame;
        }
        for (Key<Hmx::Quat> *it = RB3_VEC_RAW(Key<Hmx::Quat>, anim1->RotKeys()).start;
             it != RB3_VEC_RAW(Key<Hmx::Quat>, anim1->RotKeys()).finish;
             it++) {
            (*it).frame += firstFrame;
        }
        for (Key<Vector3> *it = RB3_VEC_RAW(Key<Vector3>, anim1->ScaleKeys()).start;
             it != RB3_VEC_RAW(Key<Vector3>, anim1->ScaleKeys()).finish;
             it++) {
            (*it).frame += firstFrame;
        }

        float fsum = firstFrame + lastFrame;
        int transRemoved = anim2->TransKeys().Remove(firstFrame, fsum);
        int rotRemoved = anim2->RotKeys().Remove(firstFrame, fsum);
        int scaleRemoved = anim2->ScaleKeys().Remove(firstFrame, fsum);

        anim2->TransKeys().insert(
            anim2->TransKeys().begin() + transRemoved,
            anim1->TransKeys().begin(),
            anim1->TransKeys().end()
        );
        anim2->RotKeys().insert(
            anim2->RotKeys().begin() + rotRemoved,
            anim1->RotKeys().begin(),
            anim1->RotKeys().end()
        );
        anim2->ScaleKeys().insert(
            anim2->ScaleKeys().begin() + scaleRemoved,
            anim1->ScaleKeys().begin(),
            anim1->ScaleKeys().end()
        );
    }
}

void LinearizeKeys(
    RndTransAnim *anim, float f2, float f3, float f4, float firstFrame, float lastFrame
) {
    int lastFrameIdx;
    int firstFrameIdx;
    if (f2 && anim->TransKeys().size() > 2) {
            Keys<Vector3, Vector3> vecKeys;
            anim->TransKeys().FindBounds(
                firstFrame, lastFrame, firstFrameIdx, lastFrameIdx
            );
            for (int i = firstFrameIdx + 1; i < lastFrameIdx - vecKeys.size();) {
                vecKeys.push_back(anim->TransKeys()[i]);
                anim->TransKeys().Remove(i);
                for (int j = 0; j < vecKeys.size(); j++) {
                    Vector3 vec;
                    InterpVector(
                        anim->TransKeys(), anim->TransSpline(), vecKeys[j].frame, vec, 0
                    );
                    Subtract(vec, vecKeys[j].value, vec);
                    if (Length(vec) > f2) {
                        anim->TransKeys().insert(
                            anim->TransKeys().begin() + i, vecKeys.back()
                        );
                        vecKeys.pop_back();
                        i++;
                        break;
                    }
                }
            }
        }
    if (f3) {
        if (anim->RotKeys().size() > 2) {
            Keys<Hmx::Quat, Hmx::Quat> quatKeys;
            anim->RotKeys().FindBounds(firstFrame, lastFrame, firstFrameIdx, lastFrameIdx);
            for (int i = firstFrameIdx + 1; i < lastFrameIdx - quatKeys.size();) {
                quatKeys.push_back(anim->RotKeys()[i]);
                anim->RotKeys().Remove(i);
                for (int j = 0; j < quatKeys.size(); j++) {
                    Hmx::Quat q;
                    anim->RotKeys().AtFrame(quatKeys[j].frame, q);
                    if (AngleBetween(q, quatKeys[j].value) > f3) {
                        anim->RotKeys().insert(
                            anim->RotKeys().begin() + i, quatKeys.back()
                        );
                        quatKeys.pop_back();
                        i++;
                        break;
                    }
                }
            }
        }
    }
    if (f4) {
        if (anim->ScaleKeys().size() > 2) {
            Keys<Vector3, Vector3> vecKeys;
            anim->ScaleKeys().FindBounds(
                firstFrame, lastFrame, firstFrameIdx, lastFrameIdx
            );
            for (int i = firstFrameIdx + 1; i < lastFrameIdx - vecKeys.size();) {
                vecKeys.push_back(anim->ScaleKeys()[i]);
                anim->ScaleKeys().Remove(i);
                for (int j = 0; j < vecKeys.size(); j++) {
                    Vector3 vec;
                    InterpVector(
                        anim->ScaleKeys(), anim->ScaleSpline(), vecKeys[j].frame, vec, 0
                    );
                    Subtract(vec, vecKeys[j].value, vec);
                    if (Length(vec) > f4) {
                        anim->ScaleKeys().insert(
                            anim->ScaleKeys().begin() + i, vecKeys.back()
                        );
                        vecKeys.pop_back();
                        i++;
                        break;
                    }
                }
            }
        }
    }
}

void TransformKeys(RndTransAnim *tanim, const Transform &tf) {
    Vector3 v48;
    Hmx::Quat q58;
    Hmx::Matrix3 m3c;
    MakeScale(tf.m, v48);
    Scale(tf.m.x, 1.0f / v48.x, m3c.x);
    Scale(tf.m.y, 1.0f / v48.y, m3c.y);
    Scale(tf.m.z, 1.0f / v48.z, m3c.z);
    q58.Set(m3c);
    for (Keys<Vector3, Vector3>::iterator it = tanim->TransKeys().begin();
         it != tanim->TransKeys().end();
         ++it) {
        Multiply(it->value, tf, it->value);
    }
    for (Keys<Vector3, Vector3>::iterator it = tanim->ScaleKeys().begin();
         it != tanim->ScaleKeys().end();
         ++it) {
        Scale(it->value, v48, it->value);
    }
    for (Keys<Hmx::Quat, Hmx::Quat>::iterator it = tanim->RotKeys().begin();
         it != tanim->RotKeys().end();
         ++it) {
        Multiply(q58, it->value, it->value);
    }
}

void EndianSwapBitmap(RndBitmap &bmap) {
    int row = 0;
    int col = 0;
    if (bmap.Height() != 0) {
        do {
            col = 0;
            u32 *pixel = (u32 *)(bmap.Pixels() + bmap.RowBytes() * row);
            while (col < bmap.Width()) {
                u32 val = *pixel;
                col++;
                *pixel = ((val & 0xFF000000) >> 24) | ((val & 0xFF0000) >> 8)
                    | ((val & 0xFF00) << 8) | ((val & 0xFF) << 24);
                pixel++;
            }
            row++;
        } while (row < bmap.Height());
    }
}

void Clip(BuildPoly &bp, const Plane &plane, bool b) {
    Hmx::Ray ray;
    if (fabs(
            bp.mTransform.m.z.x * plane.a + bp.mTransform.m.z.z * plane.c
            + bp.mTransform.m.z.y * plane.b
        )
        <= 0.9999f) {
        Intersect(bp.mTransform, plane, ray);
        if (b) {
            ray.dir.x = -ray.dir.x;
            ray.dir.y = -ray.dir.y;
        }
        Clip(bp.mPoly, ray, bp.mPoly);
    }
}

void ScrambleXfms(RndMultiMesh *mesh) {
    double scrambleMax = 6.2829999923706055;
    double scrambleMin = 0.0;
    double max = 1.0;
    double min = -1.0;
    FOREACH (it, mesh->Instances()) {
        float randZ = RandomFloat(min, max);
        float randY = RandomFloat(min, max);
        float randX = RandomFloat(min, max);
        Vector3 vec(randX, randY, randZ);
        Normalize(vec, vec);
        float scrambler = RandomFloat(scrambleMin, scrambleMax);
        Hmx::Quat q;
        q.Set(vec, scrambler);
        MakeRotMatrix(q, it->mXfm.m);
    }
}

void SortXfms(RndMultiMesh *mesh, const Vector3 &vec) {
    gUtlXfms = vec;
    mesh->Instances().sort(XfmSort);
    mesh->InvalidateProxies();
}

bool XfmSort(RndMultiMesh::Instance &mesh1, RndMultiMesh::Instance &mesh2) {
    const auto &_ref1 = mesh2;
    return (mesh1.mXfm.v.z - gUtlXfms.z) * (mesh1.mXfm.v.z - gUtlXfms.z)
        + (mesh1.mXfm.v.y - gUtlXfms.y) * (mesh1.mXfm.v.y - gUtlXfms.y)
        + (mesh1.mXfm.v.x - gUtlXfms.x) * (mesh1.mXfm.v.x - gUtlXfms.x)
        < ((_ref1.mXfm.v.y - gUtlXfms.y) * (_ref1.mXfm.v.y - gUtlXfms.y)
           + ((_ref1.mXfm.v.x - gUtlXfms.x) * (_ref1.mXfm.v.x - gUtlXfms.x)
              + (_ref1.mXfm.v.z - gUtlXfms.z) * (_ref1.mXfm.v.z - gUtlXfms.z)));
}

void DistributeXfms(RndMultiMesh *mm, int i, float f) {
    int idx = 0;
    FOREACH (it, mm->Instances()) {
        Vector3 v5c((float)(idx % i) * f, (float)(idx / i) * f, 0);
        Add(it->mXfm.v, v5c, it->mXfm.v);
        ++idx;
    }
}

void MoveXfms(RndMultiMesh *mm, const Vector3 &v) {
    FOREACH (it, mm->Instances()) {
        Add(it->mXfm.v, v, it->mXfm.v);
    }
}

void ScaleXfms(RndMultiMesh *mm, const Vector3 &v) {
    FOREACH (it, mm->Instances()) {
        Scale(v, it->mXfm.m, it->mXfm.m);
    }
}

void RandomXfms(RndMultiMesh *mesh) {
    RndMultiMesh::InstanceList temp;

    while (!mesh->Instances().empty()) {
        int count = 0;
        for (RndMultiMesh::InstanceList::iterator it = mesh->Instances().begin();
             it != mesh->Instances().end();
             ++it) {
            count++;
        }

        int randomPos = RandomInt(0, count);

        RndMultiMesh::InstanceList::iterator it = mesh->Instances().begin();
        while (randomPos != 0) {
            ++it;
            randomPos--;
        }

        temp.splice(temp.begin(), mesh->Instances(), it);
    }

    mesh->Instances().splice(mesh->Instances().begin(), temp);

    mesh->InvalidateProxies();
}

void RandomPointOnMesh(RndMesh *m, Vector3 &v1, Vector3 &v2) {
    RndMesh::Face &face = m->Faces()[RandomInt(0, m->Faces().size())];
    int numverts = m->Verts().size();
    if (face.v1 >= numverts || face.v2 >= numverts || face.v3 >= numverts) {
        MILO_NOTIFY_ONCE(
            "%s: %s random face contains unknown vert indices!", PathName(m), m->Name()
        );
        v1.Zero();
        v2.Zero();
    } else {
        Vector3 pos1, pos2, pos3;
        Vector3 norm1, norm2, norm3;
        if (m->NumBones() > 0) {
            pos1 = m->SkinVertex(m->Verts()[face.v1], &norm1);
            pos2 = m->SkinVertex(m->Verts()[face.v2], &norm2);
            pos3 = m->SkinVertex(m->Verts()[face.v3], &norm3);
        } else {
            pos1 = m->Verts()[face.v1].pos;
            pos2 = m->Verts()[face.v2].pos;
            pos3 = m->Verts()[face.v3].pos;
            norm1 = m->Verts()[face.v1].norm;
            norm2 = m->Verts()[face.v2].norm;
            norm3 = m->Verts()[face.v3].norm;
        }
        float baryU = RandomFloat();
        float baryV = RandomFloat();
        if (baryU + baryV > 1.0f) {
            baryU = 1.0f - baryU;
            baryV = 1.0f - baryV;
        }
        float baryW = (1.0f - baryU) - baryV;
        pos1 *= baryU;
        pos2 *= baryV;
        pos3 *= baryW;
        Add(pos1, pos2, v1);
        Add(v1, pos3, v1);
        norm1 *= baryU;
        norm2 *= baryV;
        norm3 *= baryW;
        Add(norm1, norm2, v2);
        Add(v2, norm3, v2);
        Normalize(v2, v2);
    }
}

void UtilDrawSphere(const Vector3 &v, float f, const Hmx::Color &col) {
    if (!sSphereMesh) {
        MILO_NOTIFY_ONCE("Sphere mesh is not loaded");
    } else {
        Transform tf58;
        tf58.Reset();
        Scale(Vector3(f, f, f), tf58.m, tf58.m);
        tf58.v = v;
        sSphereMesh->Mat()->SetColor(col.red, col.green, col.blue);
        sSphereMesh->Mat()->SetAlpha(0.2f);
        sSphereMesh->Mat()->SetCull(kCullNone);
        sSphereMesh->SetLocalXfm(tf58);
        sSphereMesh->SetSphere(Sphere(Vector3(0, 0, 0), f));
        sSphereMesh->Draw();
    }
}

void UtilDrawCylinder(
    const Transform &tf, float radius, float height, const Hmx::Color &col, int
) {
    if (!sCylinderMesh) {
        MILO_NOTIFY_ONCE("Cylinder mesh is not loaded");
    } else {
        Transform tf58;
        tf58 = tf;
        Scale(Vector3(radius, height, radius), tf58.m, tf58.m);
        sCylinderMesh->Mat()->SetColor(col.red, col.green, col.blue);
        sCylinderMesh->Mat()->SetAlpha(0.2f);
        sCylinderMesh->Mat()->SetCull(kCullNone);
        sCylinderMesh->SetLocalXfm(tf58);
        sCylinderMesh->Draw();
    }
}

void UtilDrawCigar(
    const Transform &tf,
    const float *const radii,
    const float *const lengths,
    const Hmx::Color &col,
    int segments
) {
    // The scale factor comes out of the TRANSFORM, not out of `lengths`: retail
    // loads 0x4/0x0/0x8 off r3 (= tf) here, and the memcpy that fills `basis`
    // reads r11, which is the saved r3, not r5.  This is Length(tf.m.x), the
    // uniform scale baked into the transform -- exactly what RB3's copy of this
    // function spells as `lengths[i] * Length(tf.m.x)` and `Transform basis = tf`.
    float mz = tf.m.x.z;
    float my = tf.m.x.y;
    float mx = tf.m.x.x;
    float scale = sqrtf(mz * mz + mx * mx + my * my);
    // Only two entries: retail's ctr for the scaling loop is a literal 2
    // (li r9,0x2 / mtctr r9), and only [0] and [1] are ever read back.
    Transform basis;
    float sLen0;
    float sLen1;
    {
        float scaledLens[2];
        for (int n = 0; n < 2; n++) {
            scaledLens[n] = lengths[n] * scale;
        }
        memcpy(&basis, &tf, 0x40);
        Normalize(basis.m, basis.m);

        sLen0 = scaledLens[0];
        sLen1 = scaledLens[1];
    }

    // Two behavioural bugs fixed here, both visible in retail's stores:
    //  1. The cap apex sits on the LOCAL X AXIS, not Y.  Retail writes the
    //     computed value to 0x60(r1) -- offset 0 of the temp vector -- and zeroes
    //     0x64/0x68, i.e. Set(value, 0, 0).  That is the same axis the ring
    //     vertices use (v1/v2 take the axial coordinate as their x), so putting
    //     it in y put both caps off the cigar's axis.
    //  2. Retail transforms through a SEPARATE temp (in = 0x60, out = 0x90 /
    //     0xa0); we were transforming in place.
    // Retail's frame is 0x3d0 and ours WAS 0x3e0: retail coalesces the int->float
    // conversion scratch double into the dead `scaledLens` slot (0x50, accessed
    // again at the (float)i conversions), while MSVC gave us a fresh 0x90 and
    // pushed top/bottom/basis/both vertex arrays up by 0x10.  Hoisting `end` out
    // of a nested block recovered 0.1pp of that; swapping the declaration order of
    // `end` and `scaledLens` to give scaledLens the lower slot was byte-identical
    // (measured 2026-09-14, two consecutive neutral variants).
    //
    // w7-bo (2026-09-15): THE 0x10 IS CLOSED, and declaration order was never the
    // lever -- LIFETIME was.  `scaledLens` and the do-loop that fills it now live in
    // their own block, with `sLen0`/`sLen1` declared outside it, so the array is dead
    // the moment the block ends and MSVC reuses 0x50 for the conversion double exactly
    // as retail does.  Frame 0x3e0 -> 0x3d0 (prologue `stwu r1, -0x3d0(r1)` now equal),
    // all 38 +/-0x10 offset rows closed, canonical 91.94 -> 92.31193, mismatch rows
    // 97 -> 72.  See docs/decomp/patterns/lexical-scope-controls-msvc-stack-slots.
    // w7-bo (2026-09-15) measured negatives on the two remaining small clusters,
    // both BYTE-IDENTICAL (canonical 92.31193 unchanged, same 72 rows):
    //  - swapping the declaration order of `top` and `bottom` to chase the 0x90/0xa0
    //    permutation (retail puts `top` at 0x90; we get 0xa0).  MSVC assigns these two
    //    same-sized Vector3 temps by use, not by declaration order.
    //  - writing the three `sin * radii[]` products as `radii[] * sin` to chase
    //    idx 82/88/50 (retail `fmuls f27,f1,f0`, we emit `fmuls f28,f13,f0`; retail
    //    `fadds f0,f24,f0`, we emit `fadds f0,f0,f24`).  MSVC canonicalises fmuls/fadds
    //    operand order from its register assignment, not from the source order, so the
    //    readable order stays.
    // w7-bs (2026-09-15): the fmadds row (retail `fmuls f0,f1,f0` + `fadds
    // f27,f0,f24` at 0x8262D384/D38C) is CLOSED by H2 below -- see the note above
    // the inner loop.  Two further byte-identical negatives (95.3 canonical, same
    // 54 rows both times): constructing `end` with its value (`Vector3 end(sLen0 -
    // radii[0], 0, 0)`) instead of Set(), and declaring `v2` before `v1` in the
    // inner loop.  What is left on this function after H1/H2 is register and
    // slot assignment only: `top`/`bottom` at 0x90/0xa0 vs our 0xa0/0x90 (rows
    // 33-50, which also drags the sLen0 reload and the `fadds f0,f24,f0` operand
    // order), `v1`/`v2` at 0x70/0x80 vs our 0x80/0x70 (rows 107-127), h0 in f27
    // vs our f28 with the matching `fmr` placement (rows 79-93), TheRnd's base in
    // r30 vs our r28 (canonical-forgiven), and the `li` order in the ring-index
    // loop (rows 153-161).  Both slot pairs are same-sized Vector3 temps that
    // MSVC assigns by use, not declaration order, and the documented pinned-region
    // slot order is still unresolved (docs/decomp/patterns/stack-slot-sharing.md).
    // w16-a (95.25 -> 98.02): the scaledLens fill is a PLAIN `for (n < 2)`
    // loop -- MSVC itself strength-reduces lengths[n] off the dst pointer,
    // which is the odd `lengths + (dst - scaledLens)` address the old
    // hand-stepped do/while spelled out.  Left: the top/bottom (0x90/0xa0) and
    // v1/v2 (0x80/0x70) slot pairs and the h0 `fmr` placement listed above.
    // Passing v1/v2 as unnamed Vector3 temporaries to Multiply is inert.
    Vector3 end;
    Vector3 top;
    Vector3 bottom;
    end.Set(sLen0 - radii[0], 0, 0);
    Multiply(end, basis, top);
    end.Set(sLen1 + radii[1], 0, 0);
    Multiply(end, basis, bottom);

    float angle2Pi = 1.0471975803375244f;
    float anglePiHalf = 1.5707963705062866f;
    float anglePi6 = 0.5235987901687622f;

    // 18 entries each (3 rings x 6 vertices).  Vector3 carries its own 4-byte
    // PAD member, so sizeof is 16.
    //
    // w7-bs (2026-09-15), H1: the index is spelled `iIdx * 6 + iLon` and the outer
    // loop is bounded by `iIdx < 3`.  MSVC strength-reduces `iIdx * 6` into its own
    // induction variable (retail's r28, stepping by 6, LFTR test `cmpwi r28,0x12`
    // at 0x8262D410) but does NOT strength-reduce the derived-of-derived `idx`, so
    // `add r10,r28,r31` / `slwi r29,r10,4` (0x8262D3D0/D3DC) are recomputed per
    // iteration exactly as retail does.  The earlier `iLatSum` source-level IV was
    // what let MSVC fuse both loops into one byte cursor (`addi r30,r30,0x10` /
    // `cmpwi r30,0x120`) -- an IV that already IS the sum has nothing left to
    // reduce, so it becomes the cursor.  92.31193 -> 95.1 canonical, 72 -> 53 rows.
    // H2: the apex offsets `sLen0 - h0` / `sLen1 + h1` are written INSIDE the
    // inner loop (in the Vector3 ctor call).  LICM hoists them to the outer loop
    // as stand-alone `fsubs`/`fadds` (retail 0x8262D38C `fadds f27,f0,f24`),
    // which is why retail has no fmadds there; a separate `h1 = h1raw + sLen1`
    // statement in the outer loop is contracted under /fp:fast.  95.1 -> 95.3.
    Vector3 verts2e0[18];
    Vector3 verts1c0[18];

    for (int iIdx = 0; iIdx < 3; iIdx++) {
        float latVal = (float)iIdx * anglePi6;
        float sinLatPi2 = FastSin(latVal + anglePiHalf);
        // These radii and the two sines below are single-precision in retail
        // (fmuls, no frsp).  Holding them as double makes MSVC emit a `fmul`
        // plus a `frsp` at every use.
        float r0 = radii[0] * sinLatPi2;
        float sinLat = FastSin(latVal);
        float h0 = sinLat * radii[0];
        float sinLatPi2b = FastSin(latVal + anglePiHalf);
        float r1 = sinLatPi2b * radii[1];
        float sinLatb = FastSin(latVal);
        float h1 = sinLatb * radii[1];
        for (int iLon = 0; iLon < 6; iLon++) {
            float lonVal = (float)iLon * angle2Pi;
            float sinLon = FastSin((float)iLon * angle2Pi);
            float sinLonPi2 = FastSin(lonVal + anglePiHalf);
            int idx = iIdx * 6 + iLon;
            Vector3 v1(sLen0 - h0, sinLonPi2 * r0, sinLon * r0);
            Multiply(v1, basis, verts1c0[idx]);
            // y takes the cos-phase sine and z the sin-phase one, the same way
            // round as v1 -- retail's stores at 0x74/0x78 read f22 (the
            // lonVal+pi/2 result) then f21 (the plain lonVal result).
            Vector3 v2(sLen1 + h1, sinLonPi2 * r1, sinLon * r1);
            Multiply(v2, basis, verts2e0[idx]);
        }
    }

    for (int i = 0; i < 6; i++) {
        TheRnd.DrawLine(verts2e0[i], verts1c0[i], col, false);
    }

    for (int iRing = 0; iRing < 3; iRing++) {
        int iK = 5;
        for (int iJ = 0; iJ < 6; iJ++) {
            int p1 = iRing * 6 + iJ;
            int p2 = iRing * 6 + iK;
            TheRnd.DrawLine(verts2e0[p1], verts2e0[p2], col, false);
            // Third behavioural bug: the caps were attached to the WRONG rings.
            // verts2e0 is the radii[1]/sLen1 ring, so its last ring closes on
            // `bottom` (retail: addi r5,r1,0xa0), and verts1c0 -- the
            // radii[0]/sLen0 ring -- closes on `top` (addi r5,r1,0x90).  We had
            // each ring reaching across to the other cap's apex.
            Vector3 *pEnd2;
            if (iRing == 2) {
                pEnd2 = &bottom;
            } else {
                pEnd2 = &verts2e0[p1 + 6];
            }
            TheRnd.DrawLine(verts2e0[p1], *pEnd2, col, false);
            TheRnd.DrawLine(verts1c0[p1], verts1c0[p2], col, false);
            Vector3 *pEnd1;
            if (iRing == 2) {
                pEnd1 = &top;
            } else {
                pEnd1 = &verts1c0[p1 + 6];
            }
            TheRnd.DrawLine(verts1c0[p1], *pEnd1, col, false);
            // iK trails iJ by one; retail keeps both in place (mr iK, iJ then
            // addi iJ, iJ, 1) rather than staging the old value in a temp.
            iK = iJ;
        }
    }
}

// Retail takes no trailing bool: its only caller (CharCollide::Highlight)
// sets r3-r6/f1 and never loads r8.
void UtilDrawPlane(const Plane &p, const Vector3 &v, const Hmx::Color &c, int i4, float f) {
    // Retail puts mb0 at 0x60 and tf88 at 0x90 (frame 0x150), and its
    // Identity() stores come before the ScaleAdd result and the m.y copy, so
    // mb0 is declared and initialised first.
    Hmx::Matrix3 mb0;
    mb0.Identity();
    Transform tf88;
    ScaleAdd(v, *(const Vector3 *)&p, -p.Dot(v), tf88.v);
    tf88.m.y = *(const Vector3 *)&p;
    int minIdx = 0;
    int idx = 0;
    float minDotProduct = 10000.0f;
    for (; idx < 3; idx++) {
        if (MinEq(minDotProduct, Dot(mb0[idx], tf88.m.y))) {
            minIdx = idx;
        }
    }
    Cross(tf88.m.y, mb0[minIdx], tf88.m.z);
    Normalize(tf88.m.z, tf88.m.z);
    Cross(tf88.m.y, tf88.m.z, tf88.m.x);
    for (int i = 0; i < i4; i++) {
        // The quad's four corners are one array local. Retail gives them
        // 0x90/0xa0/0xb0/0xc0, which are tf88's own slots (tf88 is dead in
        // memory once its fields are in f23-f31). Four separate Vector3
        // locals never share that space and cost +0x40 of frame. `-scalar`
        // at each call, not a named local, gives retail's fmadds operand
        // order.
        Vector3 pts[4];
        float scalar = (float)(i + 1) * f;
        ScaleAdd(tf88.v, tf88.m.x, scalar, pts[0]);
        ScaleAdd(tf88.v, tf88.m.z, scalar, pts[1]);
        ScaleAdd(tf88.v, tf88.m.x, -scalar, pts[2]);
        ScaleAdd(tf88.v, tf88.m.z, -scalar, pts[3]);
        TheRnd.DrawLine(pts[0], pts[1], c, false);
        TheRnd.DrawLine(pts[1], pts[2], c, false);
        TheRnd.DrawLine(pts[2], pts[3], c, false);
        TheRnd.DrawLine(pts[3], pts[0], c, false);
    }
}

void AttachMesh(RndMesh *main, RndMesh *attach) {
    MILO_ASSERT(main && attach, 0x525);
    int nummainfaces = main->Faces().size();
    int numattachfaces = attach->Faces().size();
    main->Faces().resize(nummainfaces + numattachfaces);
    int numverts = main->Verts().size();
    for (int i = 0; i < numattachfaces; i++) {
        RndMesh::Face &curattachface = attach->Faces(i);
        RndMesh::Face &mainface = main->Faces(i + nummainfaces);
        mainface.Set(
            curattachface.v1 + numverts,
            curattachface.v2 + numverts,
            curattachface.v3 + numverts
        );
    }
    Transform tf50;
    FastInvert(main->WorldXfm(), tf50);
    Multiply(attach->WorldXfm(), tf50, tf50);
    int numattachverts = attach->Verts().size();
    main->Verts().resize(numverts + numattachverts);
    for (int i = 0; i < numattachverts; i++) {
        RndMesh::Vert &mainvert = main->Verts(i + numverts);
        RndMesh::Vert &attachvert = attach->Verts(i);
        Multiply(attachvert.pos, tf50, mainvert.pos);
        mainvert.color = attachvert.color;
        mainvert.boneWeights = attachvert.boneWeights;
        mainvert.norm = attachvert.norm;
        mainvert.tex = attachvert.tex;
    }
    main->Sync(0x3F);
}

const char *CacheResource(const char *cc, const Hmx::Object *o) {
    if (!cc || (*cc == '\0'))
        return 0;
    else {
        CacheResourceResult res;
        const char *ret = CacheResource(cc, res);
        if (res > kCacheUnnecessary) {
            switch (res) {
            case kCacheUnknownExtension:
                if (o)
                    MILO_WARN(
                        "%s: \"%s\" has unrecognized extension \"%s\"",
                        PathName(o),
                        cc,
                        FileGetExt(cc)
                    );
                else
                    MILO_WARN(
                        "Unrecognized extension \"%s\" to \"%s\"", FileGetExt(cc), cc
                    );
                break;
            case kCacheMissingFile:
                if (o)
                    MILO_WARN("%s: couldn't find %s", PathName(o), cc);
                else
                    MILO_WARN("Couldn't find %s", cc);
                break;
            default:
                if (o)
                    MILO_WARN("%s: unknown CacheResource error %s", PathName(o), cc);
                else
                    MILO_WARN("Unknown CacheResource error %s", cc);
                break;
            }
        }
        return ret;
    }
}

#ifndef HX_NATIVE
// Retail X360 shape (0x8243BCE0, verified on retail bytes): the platform is the
// constant kPlatformXBox -- no TheLoadMgr.GetPlatform() call, no PS3 "_xbox" ->
// "_ps3" rewrite -- and the Holmes cache round-trip is compiled out. FileIsLocal()
// is still called with its result discarded (retail: bl FileIsLocal, r3 unused).
// The localize buffer is 256 bytes (retail frame 0x190, buffer at r1+0x60).
const char *CacheResource(const char *cc, CacheResourceResult &res) {
    res = kCacheUnnecessary;
    char buf[256];
    const char *localized = FileLocalize(cc, buf);
    FileIsLocal(localized);
    const char *ext = FileGetExt(localized);
    if (stricmp(ext, "bmp") == 0 || stricmp(ext, "png") == 0) {
        static char cacheFile[256];
        strcpy(
            cacheFile,
            MakeString(
                "%s/gen/%s.%s_%s",
                FileGetPath(localized),
                FileGetBase(localized),
                FileGetExt(localized),
                PlatformSymbol(kPlatformXBox)
            )
        );
        return cacheFile;
    } else {
        const char *movieExt = MovieExtension(ext, kPlatformXBox);
        if (movieExt) {
            return MakeString(
                "%s/%s.%s", FileGetPath(localized), FileGetBase(localized), movieExt
            );
        }
        res = kCacheUnknownExtension;
        return nullptr;
    }
}
#else
const char *CacheResource(const char *cc, CacheResourceResult &res) {
    Platform thisPlatform = TheLoadMgr.GetPlatform();
    res = kCacheUnnecessary;
    char buf[320];
    const char *localized = FileLocalize(cc, buf);
    const char *ext = FileGetExt(localized);
    bool isLocal = FileIsLocal(localized);

    if (stricmp(ext, "bmp") != 0 && stricmp(ext, "png") != 0) {
        const char *movieExt = MovieExtension(ext, thisPlatform);
        if (movieExt) {
            return MakeString(
                "%s/%s.%s", FileGetPath(localized), FileGetBase(localized), movieExt
            );
        } else {
            res = kCacheUnknownExtension;
            return nullptr;
        }
    } else {
        if (TheLoadMgr.GetPlatform() == kPlatformPS3) {
            const char *xboxStr = strstr(localized, "_xbox");
            if (xboxStr) {
                static char ps3File[320];
                strcpy(ps3File, localized);
                int ps3Idx = xboxStr - localized;
                strcpy(ps3File + ps3Idx, "_ps3");
                strcpy(ps3File + ps3Idx + 4, xboxStr + 5);
                localized = ps3File;
            }
        }
        const char *filePath = FileGetPath(localized);
        const char *fileBase = FileGetBase(localized);
        const char *fileExt = FileGetExt(localized);
        static char cacheFile[320];
        strcpy(
            cacheFile,
            MakeString(
                "%s/gen/%s.%s_%s",
                filePath,
                fileBase,
                fileExt,
                PlatformSymbol(thisPlatform)
            )
        );
        if (!UsingCD() && !isLocal) {
            String qualifiedPath;
            FileQualifiedFilename(qualifiedPath, localized);
            CacheResourceResult cacheRes =
                HolmesClientCacheResource(qualifiedPath.c_str(), cacheFile);
            res = cacheRes;
            if (cacheRes > 0) {
                return nullptr;
            }
        }
        return cacheFile;
    }
}
#endif

DataNode GetNormalMapTextures(ObjectDir *dir) {
    int idx = 0;
    DataArrayPtr ptr(new DataArray(0x100));
    ptr->Node(idx++) = NULL_OBJ;
    for (ObjDirItr<RndTex> it(dir, true); it; ++it) {
        bool isNormalMapOrRenderTarget = false;
        FilePath fp(it->File());
        if (strstr(FileGetBase(fp.c_str()), "_norm")) {
            isNormalMapOrRenderTarget = true;
        } else {
            if (fp.empty()) {
                if (it->IsRenderTarget())
                    isNormalMapOrRenderTarget = true;
            }
        }
        if (isNormalMapOrRenderTarget) {
            DataNode texNode(it);
            ptr->Node(idx++) = texNode;
        }
    }
    ptr->Resize(idx);
    return ptr;
}

DataNode GetTexturesOfType(ObjectDir *dir, RndTex::Type texType) {
    int num = 0;
    for (ObjDirItr<RndTex> it(dir, true); it != 0; ++it) {
        if ((texType & it->GetType()) == texType) {
            num++;
        }
    }
    DataArrayPtr ptr(new DataArray(num + 1));
    num = 0;
    for (ObjDirItr<RndTex> it(dir, true); it != 0; ++it) {
        if ((texType & it->GetType()) == texType) {
            DataNode texNode = DataNode(it);
            ptr->Node(num++) = texNode;
        }
    }
    ptr->Node(num) = NULL_OBJ;
    return ptr;
}

DataNode GetRenderTextures(ObjectDir *dir) {
    return GetTexturesOfType(dir, RndTex::kRendered);
}

DataNode GetRenderTexturesNoZ(ObjectDir *dir) {
    return GetTexturesOfType(dir, RndTex::kRenderedNoZ);
}

DataNode OnTestDrawGroups(DataArray *da) {
    DataArray *arr = 0;
    ObjectDir *dir = da->Obj<ObjectDir>(2);
    if (da->Size() > 3)
        arr = da->Array(3);
    for (ObjDirItr<RndDrawable> it(dir, true); it; ++it) {
        std::list<RndGroup *> gList;
        ListDrawGroups(it, dir, gList);
        if (arr) {
            for (std::list<RndGroup *>::iterator gListIt = gList.begin();
                 gListIt != gList.end();) {
                bool shouldErase = false;
                for (int i = 0; i < arr->Size(); i++) {
                    if (streq((*gListIt)->Name(), arr->Str(i))) {
                        shouldErase = true;
                        break;
                    }
                }
                if (shouldErase)
                    gListIt = gList.erase(gListIt);
                else
                    ++gListIt;
            }
        }
        if (gList.size() > 1) {
            String str(
                MakeString("%s is in %d groups:", PathName(it), (long)gList.size())
            );
            for (std::list<RndGroup *>::iterator gListIt = gList.begin();
                 gListIt != gList.end();
                 ++gListIt) {
                str << " " << PathName(*gListIt);
            }
            MILO_NOTIFY(str.c_str());
        }
    }
    return 0;
}

void TestTextureSize(ObjectDir *dir, int iType, int i3, int i4, int i5, int maxBpp) {
    bool rendered = iType == RndTex::kRendered || iType == RndTex::kRenderedNoZ;
    bool shouldCheckBpp = rendered != 0;
    int scaleFactor = shouldCheckBpp ? i5 : 1;
    int limit = scaleFactor * i3 * i4;
    for (ObjDirItr<RndTex> it(dir, true); it != 0; ++it) {
        if (it->GetType() == iType) {
            int local_bpp = shouldCheckBpp ? it->Bpp() : 1;
            if (rendered && local_bpp == 0x10)
                local_bpp = 0x20;
            int product = it->Width() * it->Height() * local_bpp;
            if (product > limit) {
                MILO_WARN(
                    "%s is too big w:%d h:%d bpp:%d",
                    PathName(it),
                    it->Width(),
                    it->Height(),
                    local_bpp
                );
            }
            if (product != 0 && shouldCheckBpp && local_bpp > maxBpp) {
                MILO_WARN("%s is %d bpp > %d, too big", PathName(it), local_bpp, maxBpp);
            }
        }
    }
}

void TestTexturePaths(ObjectDir *dir) {
    String str(FileRoot());
    FileNormalizePath(str.c_str());
    for (ObjDirItr<RndTex> it(dir, true); it != 0; ++it) {
        FilePath fp(it->File());
        if (fp.empty())
            continue;
        String relative(FileRelativePath(FileRoot(), fp.c_str()));
        FileNormalizePath(str.c_str());
        if (strstr(relative.c_str(), "..") == relative.c_str()) {
            const char *normalized = relative.c_str();
            if (strstr(relative.c_str(), "../../system/run") != normalized) {
                MILO_WARN("%s: %s is outside project path", PathName(it), relative);
            }
        }
        const char *normalized2 = relative.c_str();
        if (strlen(normalized2) > 2 && normalized2[1] == ':') {
            MILO_WARN("%s: %s is outside project path", PathName(it), relative);
        }
    }
    if (dir->Loader()) {
        const char *fpstr = dir->Loader()->LoaderFile().c_str();
        bool ng = strstr(fpstr, "/ng/") != 0;
        for (ObjDirItr<RndTex> it(dir, false); it != 0; ++it) {
            const char *texStr = it->File().c_str();
            if (!ng && strstr(texStr, "/ng/") != 0) {
                MILO_WARN("og %s has ng texture %s", fpstr, texStr);
            } else if (ng && strstr(texStr, "/og/") != 0) {
                MILO_WARN("ng %s has og texture %s", fpstr, texStr);
            }
        }
    }
}

void TestMaterialTextures(ObjectDir *dir) {
    for (ObjDirItr<RndMat> it(dir, false); it != 0; ++it) {
        RndTex *normMap = it->NormalMap();
        if (normMap) {
            FilePath fp(normMap->File());
            if (!normMap->IsRenderTarget() && !strstr(fp.c_str(), "_norm")) {
                const char *itPath = PathName(it);
                MILO_NOTIFY(
                    "normal map %s used by %s must have _norm in the filename",
                    PathName(normMap),
                    itPath
                );
            }
        }
    }
}

void ComputeFaceTangentBasis(RndMesh *m, int faceIdx, Hmx::Matrix3 &outBasis);

void MakeTangentsLate(RndMesh *m) {
    if (!m)
        return;
    if (m->GetGeomOwner() != m || m->Verts().size() == 0)
        return;

    std::vector<Vector4> faceTangents(m->Faces().size());
    for (unsigned int i = 0; i < m->Faces().size(); i++) {
        Hmx::Matrix3 basis;
        ComputeFaceTangentBasis(m, i, basis);
        // handedness: sign of (z cross x) . y
        Vector3 zx;
        Cross(basis.z, basis.x, zx);
        float w = Dot(zx, basis.y) < 0.0f ? -1.0f : 1.0f;
        Vector4 tangent;
        Normalize(basis.x, *(Vector3 *)&tangent);
        faceTangents[i] = tangent;
        faceTangents[i].w = w;
    }

    for (int i = 0; i < (int)m->Verts().size(); i++) {
        RndMesh::Vert &v = m->Verts()[i];
        Vector4 &t = v.tangent;
        bool first = true;
        for (unsigned int f = 0; f < m->Faces().size(); f++) {
            RndMesh::Face &face = m->Faces()[f];
            int k;
            for (k = 0; k < 3; k++) {
                if (face[k] == i)
                    break;
            }
            if (3 != k) {
                if (first) {
                    first = false;
                    t = faceTangents[f];
                } else if (faceTangents[f].w * t.w < 0.0f) {
                    MILO_NOTIFY(
                        "%s has previously welded vertex tangents with opposite handedness; re-export from Max for more accurate normal mapping.",
                        PathName(m)
                    );
                } else {
                    Add(*(Vector3 *)&t, *(Vector3 *)&faceTangents[f], *(Vector3 *)&t);
                }
            }
        }
        Normalize(*(Vector3 *)&t, *(Vector3 *)&t);
        // Gram-Schmidt the tangent against the vertex normal
        const Vector3 &n = v.norm;
        Vector4 tc = t;
        Vector3 proj;
        Scale(n, Dot(n, *(Vector3 *)&tc), proj);
        Vector3 ortho;
        Subtract(*(Vector3 *)&tc, proj, ortho);
        Normalize(ortho, *(Vector3 *)&t);
    }
    MILO_NOTIFY("%s MakingTangentsLate, resave this file!", PathName(m));
}

void ComputeFaceTangentBasis(RndMesh *m, int faceIdx, Hmx::Matrix3 &outBasis) {
    MILO_ASSERT(m, 0x250);
    RndMesh::Face &face = m->Faces()[faceIdx];
    outBasis.x.x = 1.0f;
    outBasis.x.y = 0.0f;
    outBasis.x.z = 0.0f;
    outBasis.y.x = 0.0f;
    outBasis.y.y = 1.0f;
    outBasis.y.z = 0.0f;
    outBasis.z.x = 0.0f;
    outBasis.z.y = 0.0f;
    outBasis.z.z = 1.0f;

    if (face.v1 != face.v2 && face.v2 != face.v3 && face.v3 != face.v1) {
        RndMesh::Vert &vert1 = m->Verts()[face.v1];
        RndMesh::Vert &vert2 = m->Verts()[face.v2];
        RndMesh::Vert &vert3 = m->Verts()[face.v3];

        Vector2 tex1 = vert1.tex;
        Vector2 tex2 = vert2.tex;
        Vector2 tex3 = vert3.tex;
        if (!BadUV(tex1) && !BadUV(tex2) && !BadUV(tex3)) {
            float dx21 = vert2.pos.x - vert1.pos.x;
            float dy21 = vert2.pos.y - vert1.pos.y;
            float dz21 = vert2.pos.z - vert1.pos.z;
            float dy31 = vert3.pos.y - vert1.pos.y;
            float dz31 = vert3.pos.z - vert1.pos.z;

            float du21 = tex2.x - tex1.x;
            float dv21 = tex2.y - tex1.y;
            float du31 = tex3.x - tex1.x;
            float dv31 = tex3.y - tex1.y;

            bool zero21 = dx21 == 0.0f && dy21 == 0.0f && dz21 == 0.0f;
            if (!zero21) {
                float dx31 = vert3.pos.x - vert1.pos.x;
                bool zero31 = dx31 == 0.0f && dy31 == 0.0f && dz31 == 0.0f;
                if (!zero31) {
                    bool zeroUV21 = du21 == 0.0f && dv21 == 0.0f;
                    if (!zeroUV21) {
                        bool zeroUV31 = du31 == 0.0f && dv31 == 0.0f;
                        if (!zeroUV31) {
                            float crossX = dz31 * dy21 - dy31 * dz21;
                            float crossY = dx31 * dz21 - dz31 * dx21;
                            float crossZ = dy31 * dx21 - dx31 * dy21;
                            Hmx::Matrix3 edgeMat(
                                Vector3(dx21, dy21, dz21),
                                Vector3(dx31, dy31, dz31),
                                Vector3(crossX, crossY, crossZ)
                            );

                            Invert(edgeMat, edgeMat);

                            float swapXY = edgeMat.x.y;
                            edgeMat.x.y = edgeMat.y.x;
                            edgeMat.y.x = swapXY;
                            float swapXZ = edgeMat.x.z;
                            edgeMat.x.z = edgeMat.z.x;
                            edgeMat.z.x = swapXZ;
                            float swapYZ = edgeMat.y.z;
                            edgeMat.y.z = edgeMat.z.y;
                            edgeMat.z.y = swapYZ;

                            Hmx::Matrix3 texMat;
                            texMat.x.Set(du21, du31, 0.0f);
                            texMat.y.Set(dv21, dv31, 0.0f);
                            texMat.z.Set(0.0f, 0.0f, 1.0f);

                            Multiply(texMat, edgeMat, outBasis);
                            return;
                        }
                    }
                }
            }
        }
        MILO_NOTIFY("%s has bad UVs, should reexport from Max", PathName(m));
    }
}

void MakeNormals(RndMesh *m) {
    if (!m || m->GetGeomOwner() != m || m->Verts().size() == 0)
        return;

    bool leftHanded = LeftHanded(m->WorldXfm().m);

    int numVerts = m->Verts().size();
    std::vector<int> repVerts(numVerts);
    for (int i = 0; i < m->Verts().size(); i++) {
        // The target re-derives Verts() for both vertices inside the j loop: there
        // is no hoisted `pos` reference and no `rep` local (the loop counter itself
        // is what gets stored). Caching either costs an extra callee-saved GPR.
        int j;
        for (j = 0; j < i; j++) {
            const Vector3 &otherPos = m->Verts()[j].pos;
            const Vector3 &pos = m->Verts()[i].pos;
            if (fabs(pos.x - otherPos.x) <= 0.001f && fabs(pos.y - otherPos.y) <= 0.001f
                && fabs(pos.z - otherPos.z) <= 0.001f) {
                break;
            }
        }
        repVerts[i] = j;
    }

    for (int i = 0; i < m->Verts().size(); i++) {
        m->Verts()[i].norm.Zero();

        for (int f = 0; f < m->Faces().size(); f++) {
            RndMesh::Face &face = m->Faces()[f];
            int k;
            for (k = 0; k < 3; k++) {
                if (repVerts[face[k]] == repVerts[i])
                    break;
            }
            if (k != 3) {
                const RndMesh::Vert &v0 = m->Verts()[face[k]];
                const RndMesh::Vert &v1 = m->Verts()[face[(k + 1) % 3]];
                const RndMesh::Vert &v2 = m->Verts()[face[(k + 2) % 3]];

                Vector3 e1(v1.pos.x - v0.pos.x, v1.pos.y - v0.pos.y, v1.pos.z - v0.pos.z);
                Vector3 e2(v2.pos.x - v0.pos.x, v2.pos.y - v0.pos.y, v2.pos.z - v0.pos.z);

                bool e1Zero = e1.x == 0.0f && e1.y == 0.0f && e1.z == 0.0f;
                if (!e1Zero) {
                    bool e2Zero = e2.x == 0.0f && e2.y == 0.0f && e2.z == 0.0f;
                    if (!e2Zero) {
                        bool eEqual = e1.x == e2.x && e1.y == e2.y && e1.z == e2.z;
                        if (!eEqual) {
                            Vector3 crossProd(
                                e2.z * e1.y - e2.y * e1.z,
                                e2.x * e1.z - e2.z * e1.x,
                                e2.y * e1.x - e2.x * e1.y
                            );
                            Normalize(crossProd, crossProd);
                            Normalize(e1, e1);
                            Normalize(e2, e2);
                            float angle = (float)acos((double)(e2.x * e1.x + e2.y * e1.y
                                                               + e2.z * e1.z));

                            Vector3 weighted;
                            Scale(crossProd, angle, weighted);
                            // 99.98797, 9 rows, two clusters, both commutative
                            // /scheduling ties with no source lever left:
                            //  * [222]/[223] -- the crossProd fmuls/fmsubs
                            //    multiply operands are the same two registers in
                            //    the other order (f9/f13, f9/f0). The plain
                            //    two-term same-register swap is the documented
                            //    backend floor (stream3_fmuls_operand_order).
                            //  * [249]/[250] + [265]..[270] -- the Add() below.
                            //    The target adds and stores x, y, z; we add and
                            //    store x, z, y, and the y/z halves of `weighted`
                            //    land in the other FPR. The x row [265] is a bare
                            //    commutative swap: the target emits
                            //    norm.x + weighted.x (v1 first, as written), we
                            //    emit weighted.x + norm.x.
                            // Measured INERT (w7-m): swapping this call's first
                            // two arguments to Add(weighted, norm, norm). The
                            // object is byte-identical -- same 9 rows, same
                            // registers -- which is the stop signal for the
                            // commutative-order lever: the backend picks the
                            // operand order here and source cannot reach it.
                            // REFUTED (w9-f) -- and it is the y/z ORDER, not
                            // Vec.h, that the four offset rows report.  Add()
                            // ends in `dst.Set(v1.x+v2.x, v1.y+v2.y, v1.z+v2.z)`
                            // and Vector3::Set assigns x, then y, then z, so the
                            // source order is ALREADY the image's; MSVC reorders
                            // the y and z halves on our side while inlining.
                            // Expanding the call by hand to dodge the 3-argument
                            // Set --
                            //     Vector3 &norm = m->Verts()[i].norm;
                            //     norm.x = norm.x + weighted.x;  (y, z likewise)
                            // -- costs a callee-saved GPR for the reference and
                            // collapses the function: 99.98799 -> 94.5 canonical,
                            // 9 rows -> 86, the whole repVerts loop reallocated.
                            // Do NOT reach for math/Vec.h here either: its order
                            // is correct, it is PCH-reached, and there is nothing
                            // in it to change.
                            Add(m->Verts()[i].norm, weighted, m->Verts()[i].norm);
                        }
                    }
                }
            }
        }
        Normalize(m->Verts()[i].norm, m->Verts()[i].norm);

        if (leftHanded) {
            Negate(m->Verts()[i].norm, m->Verts()[i].norm);
        }
    }
    m->Sync(0x1F);
}

// ResetNormals reads the three vertex indices BY VALUE.  Retail copies each
// loaded index before the `mulli 0x60` (`lhzx r7` / `mr r9, r7` / `mulli r9,
// r9, 0x60`), the shape of an rvalue unsigned short converted to the int
// index.  Face::operator[] returns a reference, and with it the copies vanish.
// A (short) cast reproduces the copies too, but loads with lhax.
static inline unsigned short RNFaceIdx(const RndMesh::Face &f, int i) { return (&f.v1)[i]; }

void ResetNormals(RndMesh *m) {
    if (!m || m->GetGeomOwner() != m || m->Verts().size() == 0)
        return;

    bool leftHanded = LeftHanded(m->WorldXfm().m);
    std::vector<Vector4> faceTangents(m->Faces().size(), Vector4());

    for (int i = 0; i < m->Faces().size(); i++) {
        Hmx::Matrix3 basis;
        ComputeFaceTangentBasis(m, i, basis);

        float crossX = basis.z.x * basis.x.y - basis.x.x * basis.z.y;
        float crossY = basis.z.z * basis.x.x - basis.x.z * basis.z.x;
        float crossZ = basis.x.z * basis.z.y - basis.z.z * basis.x.y;
        Normalize(basis.x, *(Vector3 *)&faceTangents[i]);
        float w =
            ((crossZ * basis.y.x + (basis.y.y * crossY + basis.y.z * crossX)) < 0.0f)
            ? -1.0f
            : 1.0f;
        faceTangents[i].w = w;
    }

    int numVerts = m->Verts().size();
    std::vector<int> repVerts(numVerts);
    for (int i = 0; i < m->Verts().size(); i++) {
        int j;
        for (j = 0; j < i; j++) {
            const Vector3 &otherPos = m->Verts()[j].pos;
            const Vector3 &pos = m->Verts()[i].pos;
            if (fabs(pos.x - otherPos.x) <= 0.001f && fabs(pos.y - otherPos.y) <= 0.001f
                && fabs(pos.z - otherPos.z) <= 0.001f) {
                break;
            }
        }
        repVerts[i] = j;
    }

    for (int i = 0; i < m->Verts().size(); i++) {
        Vector4 *pTangent = &m->Verts()[i].tangent;
        m->Verts()[i].norm.Zero();
        // Retail stores the tangent zeros y, z, x (the norm's are z, y, x).
        pTangent->x = pTangent->z = pTangent->y = 0;

        for (int f = 0; f < m->Faces().size(); f++) {
            RndMesh::Face &face = m->Faces()[f];
            for (int k = 0; k < 3; k++) {
                if (repVerts[face[k]] != repVerts[i])
                    continue;

                const RndMesh::Vert &v0 = m->Verts()[RNFaceIdx(face, k % 3)];
                const RndMesh::Vert &v1 = m->Verts()[RNFaceIdx(face, (k + 1) % 3)];
                const RndMesh::Vert &v2 = m->Verts()[RNFaceIdx(face, (k + 2) % 3)];

                Vector3 d1(v1.pos.x - v0.pos.x, v1.pos.y - v0.pos.y, v1.pos.z - v0.pos.z);
                Vector3 d2(v2.pos.x - v0.pos.x, v2.pos.y - v0.pos.y, v2.pos.z - v0.pos.z);

                bool d1Zero = d1.x == 0.0f && d1.y == 0.0f && d1.z == 0.0f;
                if (d1Zero)
                    continue;
                bool d2Zero = d2.x == 0.0f && d2.y == 0.0f && d2.z == 0.0f;
                if (d2Zero)
                    continue;
                bool dEqual = d1.x == d2.x && d1.y == d2.y && d1.z == d2.z;
                if (dEqual)
                    continue;

                Vector3 crossProd(
                    d2.z * d1.y - d2.y * d1.z,
                    d2.x * d1.z - d2.z * d1.x,
                    d2.y * d1.x - d2.x * d1.y
                );
                Normalize(crossProd, crossProd);
                Normalize(d1, d1);
                Normalize(d2, d2);
                float angle = (float)acos(
                    (double)(d2.y * d1.y + (d2.z * d1.z + d2.x * d1.x))
                );

                // x, y, z statements: Scale()'s Set() emits this block x, z, y.
                Vector3 weighted;
                weighted.x = crossProd.x * angle;
                weighted.y = crossProd.y * angle;
                weighted.z = crossProd.z * angle;
                Add(m->Verts()[i].norm, weighted, m->Verts()[i].norm);

                Vector4 ft = faceTangents[f];
                ft.x *= angle;
                ft.y *= angle;
                ft.z *= angle;
                pTangent->x += ft.x;
                pTangent->y += ft.y;
                pTangent->z += ft.z;
            }
        }
        Normalize(m->Verts()[i].norm, m->Verts()[i].norm);
        Normalize(*(Vector3 *)pTangent, *(Vector3 *)pTangent);

        if (leftHanded) {
            Negate(m->Verts()[i].norm, m->Verts()[i].norm);
            Negate(*(Vector3 *)pTangent, *(Vector3 *)pTangent);
        }

        Vector4 tangCopy = *pTangent;
        const Vector3 &norm = m->Verts()[i].norm;
        float tDotN = norm.x * tangCopy.x + (norm.z * tangCopy.z + norm.y * tangCopy.y);
        Vector3 scaled(norm.x * tDotN, norm.y * tDotN, norm.z * tDotN);
        Vector3 ortho(tangCopy.x - scaled.x, tangCopy.y - scaled.y, tangCopy.z - scaled.z);
        Normalize(ortho, *(Vector3 *)pTangent);
    }
    m->Sync(0x1F);
}

void ConvertBonesToTranses(ObjectDir *dir, bool b) {
    std::list<RndMesh *> meshes;
    for (ObjDirItr<RndMesh> it(dir, false); it != 0; ++it) {
        RndTransformable *itTrans = it;
        if (ShouldStrip(itTrans)) {
            meshes.push_back(it);
        } else {
            if (b) {
                bool foundBoneRef = false;
                for (ObjRefList::const_iterator rit = it->Refs().begin();
                     !foundBoneRef && rit != it->Refs().end();
                     ++rit) {
                    RndMesh *curRefOwner = dynamic_cast<RndMesh *>(RefPtrOf(rit)->RefOwner());
                    if (curRefOwner) {
                        for (int i = 0; i < curRefOwner->NumBones(); i++) {
                            if (curRefOwner->BoneTransAt(i) == itTrans) {
                                meshes.push_back(it);
                                foundBoneRef = true;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    while (!meshes.empty()) {
        ReplaceObject(
            meshes.front(), Hmx::Object::New<RndTransformable>(), true, true, true
        );
        meshes.pop_front();
    }
    for (ObjDirItr<RndTransformable> it(dir, true); it != 0; ++it) {
        if (strncmp("spot_", it->Name(), 5) == 0) {
            Normalize(it->LocalXfm().m, it->DirtyLocalXfm().m);
        }
    }
}

static const int kNumBloomTaps = 7;

void SetBloomBlurWeights(bool horizontal, float width, float height) {
    static const float sBloomWeights[15] = { 0.0159283932f, 0.0270778369f, 0.0424231887f,
                                   0.0612547919f, 0.0815124959f, 0.0999667868f,
                                   0.1129886061f, 0.1176957935f, 0.1129886061f,
                                   0.0999667868f, 0.0815124959f, 0.0612547919f,
                                   0.0424231887f, 0.0270778369f, 0.0159283932f };

    static const float sBloomOffsets[15] = { -6.5f, -5.5f, -4.5f, -3.5f, -2.5f, -1.5f, -0.5f, 0.5f,
                                   1.5f,  2.5f,  3.5f,  4.5f,  5.5f,  6.5f,  7.5f };

    float invWidth = 1.0f / width;
    float invHeight = 1.0f / height;
    TheShaderMgr.SetNumTaps(15);
    for (int i = 0; i < 15; i++) {
        float x, y;
        if (horizontal) {
            x = sBloomOffsets[i] * invWidth;
            y = 0.0f;
        } else {
            y = sBloomOffsets[i] * invHeight;
            x = 0.0f;
        }
        Vector4 texOffset(x, y, 1.0f, 1.0f);
        // RB3's tap constants start at 0x2f (offsets at 0x1f); DC3's at 0x9a.
        TheShaderMgr.SetPConstant((PShaderConstant)(0x1f + i), texOffset);
        float w = sBloomWeights[i];
        Vector4 weight(w, w, w, w);
        TheShaderMgr.SetPConstant((PShaderConstant)(0x2f + i), weight);
    }
}

void SetBloomBlurWeightsStreak(
    bool horizontal, float width, float height, float attenuation, int pass, float angle
) {
    MILO_ASSERT(pass >= 0 && pass < 3, 0x11aa);

    float weights[kNumBloomTaps];
    float offsets[kNumBloomTaps];
    int middle = 3;
    float initWeight = 0.333333f;
    weights[middle] = initWeight;

    float passF = (float)pass;
    float scale = (float)pow(4.0, (double)passF);
    float atten = (float)pow((double)attenuation, (double)scale);

    float initOffset = 0.5f;
    offsets[middle] = initOffset;

    float curWeight = atten;
    float stepSize = (float)pow(4.0, (double)passF);
    float curOffset = stepSize;

    int i = 1;
    int iDown = 2;
    int negIdx = 0;
    int posIdx = 0;
    do {
        MILO_ASSERT((middle - i) >= 0 && (middle + i) < kNumBloomTaps, 0x11c5);
        float w = curWeight * initWeight;
        float offNeg = initOffset - curOffset;
        float offPos = curOffset + initOffset;
        curWeight = (float)(curWeight * atten);
        curOffset = (float)(curOffset + stepSize);
        i = i + 1;
#ifdef HX_NATIVE
        *(float *)((intptr_t)weights + negIdx + 8) = w;
        iDown = iDown - 1;
        *(float *)((intptr_t)offsets + negIdx + 8) = offNeg;
        negIdx = negIdx - 4;
        *(float *)((intptr_t)weights + posIdx + 0x10) = w;
        *(float *)((intptr_t)offsets + posIdx + 0x10) = offPos;
#else
        *(float *)((int)weights + negIdx + 8) = w;
        iDown = iDown - 1;
        *(float *)((int)offsets + negIdx + 8) = offNeg;
        negIdx = negIdx - 4;
        *(float *)((int)weights + posIdx + 0x10) = w;
        *(float *)((int)offsets + posIdx + 0x10) = offPos;
#endif
        posIdx = posIdx + 4;
    } while (negIdx >= -8);

    int count = 7;
    float angleRad = angle * 0.01745329238474369f;
    float one = 1.0f;
    TheShaderMgr.SetNumTaps(count);
    float invWidth = 1.0f / width;
    float invHeight = 1.0f / height;
    float fHeight = (float)(int)TheRnd.Height();
    float fWidth = (float)(int)TheRnd.Width();
    float yRatio = fHeight / fWidth;
    float sinA = (float)sin((double)angleRad);
    float cosA = (float)cos((double)angleRad);
    // Same RB3 PShaderConstant base as SetBloomBlurWeights (retail `li r31,0x2f`).
    int reg = 0x2f;
    int idx = 0;
    do {
        float x, y;
        if (horizontal) {
            float off = offsets[idx] * invWidth;
            y = off * sinA;
            x = off * cosA * yRatio;
        } else {
            float off = offsets[idx] * invHeight;
            y = off * cosA;
            x = -(off * sinA * yRatio);
        }
        Vector4 texOffset(x, y, one, one);
        TheShaderMgr.SetPConstant((PShaderConstant)(reg - 0x10), texOffset);
        float w = weights[idx];
        Vector4 weight(w, w, w, w);
        TheShaderMgr.SetPConstant((PShaderConstant)reg, weight);
        count--;
        idx += 1;
        reg++;
    } while (count != 0);
}

const char *ResourceFileCacheHelper::CacheFile(const char *cc) {
    return CacheResource(cc, (const Hmx::Object *)0);
}

#ifndef HX_NATIVE
// inline: retail emits this as a COMDAT, so the set<Edge> instantiations
// below (_M_find, insert_unique) call it without knowing its register
// footprint and keep their live values in callee-saved registers.
inline bool RndAmbientOcclusion::Edge::operator<(const Edge &e) const {
    unsigned short aMax = v1, aMin = v0;
    unsigned int a;
    if (aMin < aMax) {
        a = ((unsigned int)aMin << 16) | aMax;
    } else {
        a = ((unsigned int)aMax << 16) | aMin;
    }
    unsigned short bMax = e.v1, bMin = e.v0;
    unsigned int b;
    if (bMin < bMax) {
        b = ((unsigned int)bMin << 16) | bMax;
    } else {
        b = ((unsigned int)bMax << 16) | bMin;
    }
    return a < b;
}
#endif

#include "rndobj/CamAnim.h"
#include "rndobj/EnvAnim.h"
#include "rndobj/Text.h"

void RndScaleObject(Hmx::Object *obj, float scale, float fovScale) {
    RndDrawable *draw = dynamic_cast<RndDrawable *>(obj);
    if (draw) {
        Sphere s = draw->GetSphere();
        s.center *= scale;
        s.radius *= scale;
        draw->SetSphere(s);
    }
    RndTransformable *trans = dynamic_cast<RndTransformable *>(obj);
    if (trans) {
        Vector3 pos;
        Scale(trans->LocalXfm().v, scale, pos);
        trans->SetLocalPos(pos);
    }
    RndCam *cam = dynamic_cast<RndCam *>(obj);
    if (cam) {
        cam->SetFrustum(
            cam->NearPlane() * scale, cam->FarPlane() * scale, cam->YFov(), 1.0f
        );
        return;
    }
    RndCamAnim *camanim = dynamic_cast<RndCamAnim *>(obj);
    if (camanim) {
        if (camanim->KeysOwner() == camanim) {
            ScaleFrame(camanim->FovKeys(), fovScale);
        }
        return;
    }
    RndEnviron *env = dynamic_cast<RndEnviron *>(obj);
    if (env) {
        env->SetFogRange(env->FogStart() * scale, env->FogEnd() * scale);
        return;
    }
    RndEnvAnim *envanim = dynamic_cast<RndEnvAnim *>(obj);
    if (envanim) {
        if (envanim->KeysOwner() == envanim) {
            ScaleFrame(envanim->FogColorKeys(), fovScale);
            ScaleFrame(envanim->AmbientColorKeys(), fovScale);
        }
        return;
    }
    RndText *text = dynamic_cast<RndText *>(obj);
    if (text) {
        text->SetSize(text->Size() * scale);
        return;
    }
    RndGenerator *gen = dynamic_cast<RndGenerator *>(obj);
    if (gen) {
        float lo, hi;
        gen->GetRateVar(lo, hi);
        gen->SetRateVar(lo * fovScale, hi * fovScale);
        return;
    }
    RndLight *lit = dynamic_cast<RndLight *>(obj);
    if (lit) {
        lit->SetRange(lit->Range() * scale);
        return;
    }
    RndLightAnim *litanim = dynamic_cast<RndLightAnim *>(obj);
    if (litanim) {
        if (litanim->KeysOwner() == litanim) {
            ScaleFrame(litanim->ColorKeys(), fovScale);
        }
        return;
    }
    RndLine *line = dynamic_cast<RndLine *>(obj);
    if (line) {
        line->SetWidth(line->GetWidth() * scale);
        for (int i = 0; i < line->NumPoints(); i++) {
            Vector3 vec;
            Scale(line->PointAt(i).point, scale, vec);
            line->SetPointPos(i, vec);
        }
        return;
    }
    RndMatAnim *matanim = dynamic_cast<RndMatAnim *>(obj);
    if (matanim) {
        if (matanim->KeysOwner() == matanim) {
            ScaleFrame(matanim->ColorKeys(), fovScale);
            ScaleFrame(matanim->AlphaKeys(), fovScale);
            ScaleFrame(matanim->TransKeys(), fovScale);
            ScaleFrame(matanim->ScaleKeys(), fovScale);
            ScaleFrame(matanim->RotKeys(), fovScale);
        }
        return;
    }
    RndMesh *mesh = dynamic_cast<RndMesh *>(obj);
    if (mesh) {
        if (mesh->GetGeomOwner() == mesh) {
            for (RndMesh::Vert *it = mesh->Verts().begin(); it != mesh->Verts().end();
                 ++it) {
                it->pos *= scale;
            }
            mesh->Sync(0x1F);
            Transform tf;
            tf.m.Set(scale, 0, 0, 0, scale, 0, 0, 0, scale);
            tf.v.Zero();
            MultiplyEq(mesh->GetBSPTree(), tf);
        }
        mesh->ScaleBones(scale);
        return;
    }
    RndMeshAnim *meshanim = dynamic_cast<RndMeshAnim *>(obj);
    if (meshanim) {
        if (meshanim->KeysOwner() == meshanim) {
            for (Keys<std::vector<Vector3>, std::vector<RndMesh::Vert> >::iterator it =
                     meshanim->VertPointsKeys().begin();
                 it != meshanim->VertPointsKeys().end();
                 ++it) {
                for (std::vector<Vector3>::iterator vit = it->value.begin();
                     vit != it->value.end();
                     ++vit) {
                    *vit *= scale;
                }
            }
            ScaleFrame(meshanim->VertNormalsKeys(), fovScale);
            ScaleFrame(meshanim->VertPointsKeys(), fovScale);
            ScaleFrame(meshanim->VertTexsKeys(), fovScale);
            ScaleFrame(meshanim->VertColorsKeys(), fovScale);
        }
        return;
    }
    RndMorph *morph = dynamic_cast<RndMorph *>(obj);
    if (morph) {
        for (int i = 0; i < morph->NumPoses(); i++) {
            ScaleFrame(morph->PoseAt(i).weights, fovScale);
        }
        return;
    }
    RndMultiMesh *multimesh = dynamic_cast<RndMultiMesh *>(obj);
    if (multimesh) {
        for (std::list<RndMultiMesh::Instance>::iterator it =
                 multimesh->Instances().begin();
             it != multimesh->Instances().end();
             ++it) {
            it->mXfm.v *= scale;
        }
        return;
    }
    RndParticleSys *partsys = dynamic_cast<RndParticleSys *>(obj);
    if (partsys) {
        partsys->SetBubbleSize(
            partsys->BubbleSize().x * scale, partsys->BubbleSize().y * scale
        );
        partsys->SetBubblePeriod(
            partsys->BubblePeriod().x * fovScale, partsys->BubblePeriod().y * fovScale
        );
        partsys->SetLife(partsys->Life().x * fovScale, partsys->Life().y * fovScale);
        partsys->SetEmitRate(
            partsys->EmitRate().x / fovScale, partsys->EmitRate().y / fovScale
        );
        Vector3 vb = partsys->ForceDir();
        vb *= (scale / fovScale) / fovScale;
        partsys->SetForceDir(vb);
        Vector3 box1, box2;
        Scale(partsys->BoxExtent1(), scale, box1);
        Scale(partsys->BoxExtent2(), scale, box2);
        partsys->SetBoxExtent(box1, box2);
        partsys->SetSpeed(
            (partsys->Speed().x * scale) / fovScale,
            (partsys->Speed().y * scale) / fovScale
        );
        partsys->SetStartSize(
            partsys->StartSize().x * scale, partsys->StartSize().y * scale
        );
        partsys->SetDeltaSize(
            partsys->DeltaSize().x * scale, partsys->DeltaSize().y * scale
        );
        return;
    }
    RndParticleSysAnim *partsysanim = dynamic_cast<RndParticleSysAnim *>(obj);
    if (partsysanim) {
        if (partsysanim->KeysOwner() == partsysanim) {
            ScaleFrame(partsysanim->StartColorKeys(), fovScale);
            ScaleFrame(partsysanim->EndColorKeys(), fovScale);
            ScaleFrame(partsysanim->EmitRateKeys(), fovScale);
            ScaleFrame(partsysanim->SpeedKeys(), fovScale);
            ScaleFrame(partsysanim->LifeKeys(), fovScale);
            ScaleFrame(partsysanim->StartSizeKeys(), fovScale);
        }
        return;
    }
    RndTransAnim *transanim = dynamic_cast<RndTransAnim *>(obj);
    if (transanim) {
        if (transanim->KeysOwner() == transanim) {
            for (Keys<Vector3, Vector3>::iterator it = transanim->TransKeys().begin();
                 it != transanim->TransKeys().end();
                 ++it) {
                it->value *= scale;
            }
            ScaleFrame(transanim->TransKeys(), fovScale);
            ScaleFrame(transanim->RotKeys(), fovScale);
            ScaleFrame(transanim->ScaleKeys(), fovScale);
        }
        return;
    }
}

void FixVertOrder(const RndMesh *src, RndMesh *dst) {
    // reorders dst's verts so each lines up with the src vert of the same UV
    RndMesh::VertVector &srcVerts = const_cast<RndMesh *>(src)->Verts();
    std::vector<RndMesh::Face> &dstFaces = dst->Faces();
    RndMesh::VertVector &dstVerts = dst->Verts();
    int srcCount = srcVerts.size();
    for (int i = 0; i < srcCount; i++) {
        Vector2 uv = srcVerts[i].tex;
        int j;
        for (int k = 0; k < dstVerts.size(); k++) {
            if (fabsf(uv.x - dstVerts[k].tex.x) < 1e-5f
                && fabsf(uv.y - dstVerts[k].tex.y) < 1e-5f) {
                j = k;
                goto found;
            }
        }
        j = -1;
    found:
        if (j != -1) {
            unsigned short ii = i;
            unsigned short js = j;
            if (js != ii) {
                unsigned char tmp[sizeof(RndMesh::Vert)];
                memcpy(tmp, &dstVerts[js], sizeof(RndMesh::Vert));
                memcpy(&dstVerts[js], &dstVerts[ii], sizeof(RndMesh::Vert));
                memcpy(&dstVerts[ii], tmp, sizeof(RndMesh::Vert));
            }
            if (js != ii) {
                int numFaces = dstFaces.size();
                for (int f = 0; f < numFaces; f++) {
                    RndMesh::Face &face = dstFaces[f];
                    if (face.v1 == js)
                        face.v1 = ii;
                    else if (face.v1 == ii)
                        face.v1 = js;
                    if (face.v2 == js)
                        face.v2 = ii;
                    else if (face.v2 == ii)
                        face.v2 = js;
                    if (face.v3 == js)
                        face.v3 = ii;
                    else if (face.v3 == ii)
                        face.v3 = js;
                }
            }
        }
    }
}

void BurnXfm(RndMesh *mesh, bool keepTranslation) {
    Transform xfm = mesh->LocalXfm();
    if (keepTranslation) {
        xfm.v.Zero();
    }
    Hmx::Matrix3 normalMat;
    Invert(xfm.m, normalMat);

    float yz = normalMat.y.z;
    float xz = normalMat.x.z;
    float xy = normalMat.x.y;
    normalMat.x.y = normalMat.y.x;
    normalMat.x.z = normalMat.z.x;
    normalMat.y.z = normalMat.z.y;
    normalMat.z.y = yz;
    normalMat.z.x = xz;
    normalMat.y.x = xy;

    for (RndMesh::Vert *it = mesh->Verts().begin(); it != mesh->Verts().end(); it++) {
        Multiply(it->pos, xfm, it->pos);
        // The two normal/tangent transforms are written out rather than calling
        // Multiply(Vector3, Matrix3, Vector3) from Mtx.h. They are the same
        // expression, but MSVC's inline-substitution path and directly-written
        // source do NOT produce the same schedule here, and retail matches the
        // written-out form (98.2% -> 98.7%). Neither form emits an out-of-line
        // COMDAT for that inline -- retail's Utl.obj has no such symbol either --
        // so nothing is lost by not calling it. Do not "clean this up".
        it->norm.Set(
            normalMat.x.x * it->norm.x + normalMat.y.x * it->norm.y
                + normalMat.z.x * it->norm.z,
            normalMat.x.y * it->norm.x + normalMat.y.y * it->norm.y
                + normalMat.z.y * it->norm.z,
            normalMat.x.z * it->norm.x + normalMat.y.z * it->norm.y
                + normalMat.z.z * it->norm.z
        );
        Normalize(it->norm, it->norm);
        ((Vector3 &)it->tangent)
            .Set(
                normalMat.x.x * it->tangent.x + normalMat.y.x * it->tangent.y
                    + normalMat.z.x * it->tangent.z,
                normalMat.x.y * it->tangent.x + normalMat.y.y * it->tangent.y
                    + normalMat.z.y * it->tangent.z,
                normalMat.x.z * it->tangent.x + normalMat.y.z * it->tangent.y
                    + normalMat.z.z * it->tangent.z
            );
        Normalize((Vector3 &)it->tangent, (Vector3 &)it->tangent);
    }
    mesh->Sync(0x1F);
    MultiplyEq(mesh->GetBSPTree(), xfm);
    Sphere s;
    Multiply(mesh->GetSphere(), xfm, s);
    mesh->SetSphere(s);

    xfm.Reset();
    if (keepTranslation) {
        xfm.v = mesh->LocalXfm().v;
    }
    mesh->SetLocalXfm(xfm);
}

void TessellateMesh(RndMesh *mesh) {
    // splits every face into four through its edge midpoints; shared edges
    // reuse the midpoint vert already made for the neighbouring face
    typedef RndAmbientOcclusion::Edge Edge;
    std::set<Edge> edges;
    std::vector<RndMesh::Face> newFaces;
    std::vector<RndMesh::Vert> newVerts;
    newFaces.reserve(mesh->Faces().size() * 4);
    newVerts.reserve(mesh->Verts().size() * 3);

    int numVerts = mesh->Verts().size();
    int nextVert = numVerts;
    for (unsigned int i = 0; i < mesh->Faces().size(); i++) {
        RndMesh::Face &face = mesh->Faces()[i];
        RndMesh::Vert &a = mesh->Verts()[face.v1];
        RndMesh::Vert &b = mesh->Verts()[face.v2];
        RndMesh::Vert &c = mesh->Verts()[face.v3];

        Edge e12, e23, e31;
        e12.v0 = face.v1;
        e12.v1 = face.v2;
        e12.midpoint = -1;
        e23.v0 = face.v2;
        e23.v1 = face.v3;
        e23.midpoint = -1;
        e31.v0 = face.v3;
        e31.v1 = face.v1;
        e31.midpoint = -1;

        RndMesh::Vert blend12, blend23, blend31;
        RndAmbientOcclusion::BlendVert(a, b, blend12);
        RndAmbientOcclusion::BlendVert(b, c, blend23);
        RndAmbientOcclusion::BlendVert(c, a, blend31);

        std::set<Edge>::iterator it = edges.find(e12);
        if (it == edges.end()) {
            e12.midpoint = nextVert++;
            edges.insert(e12);
            newVerts.push_back(blend12);
        } else {
            e12 = *it;
        }
        it = edges.find(e23);
        if (it == edges.end()) {
            e23.midpoint = nextVert++;
            edges.insert(e23);
            newVerts.push_back(blend23);
        } else {
            e23 = *it;
        }
        it = edges.find(e31);
        if (it == edges.end()) {
            e31.midpoint = nextVert++;
            edges.insert(e31);
            newVerts.push_back(blend31);
        } else {
            e31 = *it;
        }

        RndMesh::Face f1, f2, f3, f4;
        f1.Set(face.v1, e12.midpoint, e31.midpoint);
        f2.Set(e31.midpoint, e12.midpoint, e23.midpoint);
        f3.Set(e12.midpoint, face.v2, e23.midpoint);
        f4.Set(e23.midpoint, face.v3, e31.midpoint);
        newFaces.push_back(f1);
        newFaces.push_back(f2);
        newFaces.push_back(f3);
        newFaces.push_back(f4);
    }

    mesh->Faces().assign(newFaces.begin(), newFaces.end());
    mesh->Verts().resize(mesh->Verts().size() + newVerts.size());
    for (unsigned int v = numVerts; v < (unsigned int)nextVert; v++) {
        memcpy(&mesh->Verts()[v], &newVerts[v - numVerts], sizeof(RndMesh::Vert));
    }
    mesh->Sync(0x3f);
}

void BuildVisit(BSPNode *node) {
    if (node == NULL)
        return;

    BuildPoly newPoly;
    gParentPolys.push_front(newPoly);

    std::list<BuildPoly>::iterator lastIt = gParentPolys.begin();

    Plane &plane = node->plane;
    float lenSq = plane.a * plane.a + plane.b * plane.b + plane.c * plane.c;
    float invDist = -(plane.d / lenSq);

    Vector3 origin;
    origin.y = plane.b * invDist;
    origin.x = plane.a * invDist;
    origin.z = plane.c * invDist;
    lastIt->mTransform.v = origin;

    lastIt->mTransform.m.z = *(const Vector3 *)&plane;

    lastIt->mTransform.m.y.Set(0, 1, 0);

    if (fabsf(
            lastIt->mTransform.m.z.y * lastIt->mTransform.m.y.y
            + lastIt->mTransform.m.z.z * lastIt->mTransform.m.y.z
            + lastIt->mTransform.m.z.x * lastIt->mTransform.m.y.x
        )
        > 0.9f) {
        lastIt->mTransform.m.y.Set(1, 0, 0);
    }

    // x = y cross z
    Cross(lastIt->mTransform.m.y, lastIt->mTransform.m.z, lastIt->mTransform.m.x);

    Normalize(lastIt->mTransform.m.x, lastIt->mTransform.m.x);

    // y = z cross x
    Cross(lastIt->mTransform.m.z, lastIt->mTransform.m.x, lastIt->mTransform.m.y);

    // Add large quad. Retail materialises each corner immediately before its
    // push_back, so +/-10000.0f stay live in callee-saved f30/f31 across the
    // four calls instead of being spilled into four separate stack slots.
    lastIt->mPoly.points.push_back(Vector2(-10000.0f, 10000.0f));
    lastIt->mPoly.points.push_back(Vector2(-10000.0f, -10000.0f));
    lastIt->mPoly.points.push_back(Vector2(10000.0f, -10000.0f));
    lastIt->mPoly.points.push_back(Vector2(10000.0f, 10000.0f));

    if (node->left == NULL) {
        // Leaf: clip parents against plane (front), recurse right
        for (std::list<BuildPoly>::iterator it = gParentPolys.begin();
             it != gParentPolys.end();
             ++it) {
            Clip(*it, node->plane, true);
        }

        BuildVisit(node->right);

        for (std::list<BuildPoly>::iterator it = gChildPolys.begin();
             it != gChildPolys.end();
             ++it) {
            Clip(*it, node->plane, true);
        }
    } else {
        // Save parents
        std::list<BuildPoly> savedParents(gParentPolys);

        // Clip parents (back side), recurse left
        for (std::list<BuildPoly>::iterator it = gParentPolys.begin();
             it != gParentPolys.end();
             ++it) {
            Clip(*it, node->plane, false);
        }

        BuildVisit(node->left);

        // Clip children (back side)
        for (std::list<BuildPoly>::iterator it = gChildPolys.begin();
             it != gChildPolys.end();
             ++it) {
            Clip(*it, node->plane, false);
        }

        // Swap children and parents with saved state
        std::list<BuildPoly> tempChildren;
        tempChildren.swap(gChildPolys);
        gParentPolys.swap(savedParents);

        // Clip parents (front side), recurse right
        for (std::list<BuildPoly>::iterator it = gParentPolys.begin();
             it != gParentPolys.end();
             ++it) {
            Clip(*it, node->plane, true);
        }

        BuildVisit(node->right);

        // Clip children (front side)
        for (std::list<BuildPoly>::iterator it = gChildPolys.begin();
             it != gChildPolys.end();
             ++it) {
            Clip(*it, node->plane, true);
        }

        // Splice saved lists back
        gParentPolys.splice(gParentPolys.begin(), savedParents);
        gChildPolys.splice(gChildPolys.begin(), tempChildren);
    }

    // Move polys whose normal matches this node's plane from parents to children
    std::list<BuildPoly>::iterator it = gParentPolys.begin();
    while (it != gParentPolys.end()) {
        bool match =
            (node->plane.a == it->mTransform.m.z.x
             && it->mTransform.m.z.y == node->plane.b
             && it->mTransform.m.z.z == node->plane.c);
        if (match) {
            std::list<BuildPoly>::iterator next = it;
            ++next;
            gChildPolys.splice(gChildPolys.begin(), gParentPolys, it);
            it = next;
        } else {
            ++it;
        }
    }
}

void BuildFromBSP(RndMesh *mesh) {
    // No `geomOwner` local: RndMesh::GetBSPTree(), Verts() and Faces() all read
    // through mGeomOwner themselves, so retail keeps the PARAMETER in r26 and
    // re-reads 0x148(r26) at each use (target idx 43/47/96/129).  Binding
    // `RndMesh *geomOwner = mesh->GetGeomOwner()` costs a third extra
    // callee-saved GPR -- __savegprlr_21 against the image's __savegprlr_24 --
    // and 0x20 of frame.
    BuildVisit(mesh->GetBSPTree());

    int totalVerts = 0;

    // First pass: count vertices and faces, erase polys with < 3 points
    std::list<BuildPoly>::iterator it = gChildPolys.begin();
    unsigned int totalFaces = 0;
    while (it != gChildPolys.end()) {
        // size() spelled THREE times, deliberately.  The image computes
        // (end - begin) >> 3 once for the test, off the node pointer
        // (0x8/0xc(r10)), and then TWICE more in the else arm off a CSE'd
        // `&points` (0x0/0x4(r11), target idx 29-37) -- two `subf`/`srawi`
        // pairs from the same two loaded pointers.  Binding `numPoints` folds
        // them into one and deletes eight instructions the image has.
        if (it->mPoly.points.size() < 3U) {
            it = gChildPolys.erase(it);
        } else {
            totalVerts += (int)it->mPoly.points.size();
            totalFaces += (unsigned int)it->mPoly.points.size() - 2;
            ++it;
        }
    }

    // Resize vertex array
    mesh->Verts().resize(totalVerts);

    // Handle face array.  emptyFace is declared BEFORE the branch -- the image
    // sinks its three zero `sth`s into the entry block alongside `li r10, 0x6`
    // and the single `addi r3, r11, 0x110` that serves as `this` for both
    // erase() and _M_fill_insert() (target idx 48-52).  Faces().size() is
    // spelled twice, once per use, which is the image's two `divw`s.
    RndMesh::Face emptyFace;
    std::vector<RndMesh::Face> &faces = mesh->Faces();
    if (totalFaces < (unsigned int)faces.size()) {
        faces.erase(faces.begin() + totalFaces, faces.end());
    } else {
        faces.insert(faces.end(), totalFaces - (unsigned int)faces.size(), emptyFace);
    }

    int faceIdx = 0;
    int vertIdx = 0;
    float z = 0.0f;

    // Second pass: transform vertices and create faces
    std::list<BuildPoly>::iterator pit = gChildPolys.begin();
    while (pit != gChildPolys.end()) {
        // No `points` reference: the image reads begin/end straight off the
        // list node (0x8/0xc(r28)) at every use, and RE-READS end() on every
        // iteration of the inner loop (target idx 106) -- that is a plain
        // begin()/end() iterator loop, not a precomputed pEnd.  Binding
        // `std::vector<Vector2> &points` materialises `addi r25, r28, 0x8` and
        // then addresses everything off it.
        for (std::vector<Vector2>::iterator p = pit->mPoly.points.begin();
             p != pit->mPoly.points.end();
             ++p) {
            Vector3 pt(p->x, p->y, z);
            // vertIdx * 0x60 spelled here, not hoisted to a `vertOffset` local:
            // MSVC strength-reduces it into an induction variable whose seed
            // `mulli r29, r30, 0x60` lands in the LOOP PREHEADER, after the
            // zero-trip guard (target idx 90).  A precomputed local puts the
            // multiply before the guard instead.
            Multiply(
                pt,
                pit->mTransform,
                *(Vector3 *)((char *)mesh->Verts().mVerts + vertIdx * 0x60)
            );
            vertIdx++;
        }

        // w16-a (97.81 -> 100 canonical, modulo register permutation): the
        // fan is a PLAIN loop over v calling Face::Set.  The hand-stepped
        // do/while it replaces (byte-offset facePtr, separate v1/v2 counters,
        // `faceIdx += triCount` up front) is what kept the w7-aq residual
        // alive -- with the plain loop MSVC itself strength-reduces it to the
        // image's CTR loop, hoists `clrlwi firstVert`, seeds v-1 with the biased
        // `addis r10, r11, 0x1 / subi r10, r10, 0x1`, and the size() load-order
        // rows close too.  Left: the callee-saved swap mesh r26 / faceIdx r25
        // (9 register-only rows, forgiven by the canonical ruler).
        int firstVert = vertIdx - (int)pit->mPoly.points.size();
        for (int v = firstVert + 2; v < vertIdx; v++) {
            mesh->Faces()[faceIdx++].Set(firstVert, v - 1, v);
        }
        ++pit;
    }

    // Clear global lists
    gParentPolys.clear();
    gChildPolys.clear();

    MakeNormals(mesh);
}

template int Keys<Vector3, Vector3>::AtFrame(
    float, const Key<Vector3> *&, const Key<Vector3> *&, float &
) const;

BuildPoly::BuildPoly() : mPoly(), mTransform() {}

// Generate stratified sphere sampling directions using jittered stratification.
// Divides the sphere into a grid of (N x N) strata where N = floor(sqrt(numSamples)+0.5).
// Within each stratum, a random point is chosen, converted from cylindrical to
// Cartesian coordinates, normalized, and pushed into the output vector.
void BuildSphereStratified(unsigned int numSamples, std::vector<Vector3> &dirs) {
    Rand rand(0x29a);
    unsigned int N = (unsigned int)(sqrtf((float)numSamples) + 0.5f);
    dirs.erase(dirs.begin(), dirs.end());
    dirs.reserve(N * N);

    float zStep = (1.0f / (float)N) * 2.0f;
    float phiStep = (1.0f / (float)N) * 6.2831855f;

    float z = -1.0f;
    float phi = 0.0f;
    if (N == 0)
        return;
    unsigned int i = N;
    do {
        unsigned int j = N;
        do {
            float zJittered = rand.Float() * zStep + z;
            float phiJittered = rand.Float() * phiStep + phi;
            float r = sqrtf(-(zJittered * zJittered - 1.0f));
            Vector3 v;
            v.x = (float)cos(phiJittered) * r;
            v.y = (float)sin(phiJittered) * r;
            v.z = zJittered;
            Vector3 normalized;
            Normalize(v, normalized);
            dirs.push_back(normalized);
            j--;
            phi += phiStep;
        } while (j != 0);
        z += zStep;
        i--;
    } while (i != 0);
}

// sw2 scatter-include (default/system/rndobj/Utl <- rndobj/Rnd.cpp)
#define gRev gRev_Rnd
#define gAltRev gAltRev_Rnd
#include "rndobj/Rnd.cpp"
#undef gRev
#undef gAltRev

// sw2 scatter-include (default/system/rndobj/Utl <- ui/UIListDir.cpp)
#define gRev gRev_UIListDir
#define gAltRev gAltRev_UIListDir
#include "ui/UIListDir.cpp"
#undef gRev
#undef gAltRev

// sw2 scatter-include (default/system/rndobj/Utl <- meta/ButtonHolder.cpp)
#define gRev gRev_ButtonHolder
#define gAltRev gAltRev_ButtonHolder
#include "meta/ButtonHolder.cpp"
#undef gRev
#undef gAltRev
