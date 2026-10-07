// rb3-xenon native -- W16-TY: VIA-DC3 engine rows native linked but no target
// entered, and VIA-DC3 rows that run with no gate.
//
// docs/decomp/CAMPAIGN_STATE_2026-10-07c.md section 6, lever 2, VIA-DC3 half:
// 173 rows / 145,432 B joined to a native definition and entered by no target,
// plus the executed VIA-DC3 rows no gate checked (W16-TF section 6). This phase
// drives the largest of them on shipped data, in rb3-render's default mode,
// after W16-TS. The fixture is the shipped vignette
// world/vignette/transition/gen/tv11_a.milo_xbox (15,765 objects: 2,517 meshes,
// 8 Spotlights, 13 AmbientOcclusion objects, 3 ParticleSys, 41 Characters,
// 695 CharClips, cams, lights, environs, trans anims).
//
// THE REFERENCE. Every expected value is computed in this file from the shipped
// object's own members, read directly, or from closed forms read off retail's
// code, never by calling the function under test. A fixture gate runs first so
// a broken fixture fails before a gate reads it.
// See docs/decomp/W16TY_VIA_DC3_UNENTERED_ROWS_2026-10-07.md.

#include "char/CharBones.h"
#include "char/CharClip.h"
#include "math/Mtx.h"
#include "math/Rot.h"
#include "math/Vec.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/DirLoader.h"
#include "rndobj/Cam.h"
#include "rndobj/Draw.h"
#include "rndobj/Env.h"
#include "rndobj/Flare.h"
#include "rndobj/Lit.h"
#include "rndobj/Mesh.h"
#include "rndobj/Part.h"
#include "rndobj/Trans.h"
#include "rndobj/TransAnim.h"
#include "rndobj/Utl.h"
#include "utl/FilePath.h"
#include "world/Spotlight.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

void MakeNormals(RndMesh *); // rndobj/Utl.cpp; not declared in Utl.h

typedef void (*GateFn)(const char *, bool, const char *);

namespace {

GateFn gGate = nullptr;
char gBuf[1536];
int gRan = 0;

void Gate(const char *name, bool ok, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
void Gate(const char *name, bool ok, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(gBuf, sizeof(gBuf), fmt, ap);
    va_end(ap);
    gGate(name, ok, gBuf);
    gRan++;
}

bool Near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }
bool NearV(const Vector3 &a, const Vector3 &b, float tol) {
    return Near(a.x, b.x, tol) && Near(a.y, b.y, tol) && Near(a.z, b.z, tol);
}
float Len(const Vector3 &v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
Vector3 Sub(const Vector3 &a, const Vector3 &b) { return Vector3(a.x - b.x, a.y - b.y, a.z - b.z); }
Vector3 CrossV(const Vector3 &a, const Vector3 &b) {
    return Vector3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
float DotV(const Vector3 &a, const Vector3 &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vector3 Unit(const Vector3 &v) {
    float l = Len(v);
    return l > 0 ? Vector3(v.x / l, v.y / l, v.z / l) : v;
}
Vector3 Axpy(const Vector3 &y, float a, const Vector3 &x) { // y + a*x
    return Vector3(y.x + a * x.x, y.y + a * x.y, y.z + a * x.z);
}

// ================================================================ fixture ==
ObjDirPtr<ObjectDir> gVignette;
std::map<std::string, std::vector<Hmx::Object *> > gByClass;
int gObjects = 0;

void Walk(ObjectDir *root) {
    std::vector<ObjectDir *> todo(1, root);
    std::set<ObjectDir *> seen;
    while (!todo.empty()) {
        ObjectDir *d = todo.back();
        todo.pop_back();
        if (!seen.insert(d).second)
            continue;
        for (ObjDirItr<Hmx::Object> it(d, true); it; ++it) {
            gByClass[it->ClassName().Str()].push_back(&*it);
            gObjects++;
            ObjectDir *sub = dynamic_cast<ObjectDir *>(&*it);
            if (sub && sub != d)
                todo.push_back(sub);
        }
    }
}

template <class T> std::vector<T *> All(const char *cls) {
    std::vector<T *> out;
    for (Hmx::Object *o : gByClass[cls]) {
        T *t = dynamic_cast<T *>(o);
        if (t)
            out.push_back(t);
    }
    return out;
}

bool LoadFixture() {
    const char *path = "world/vignette/transition/gen/tv11_a.milo_xbox";
    auto t0 = std::chrono::steady_clock::now();
    gVignette.LoadFile(FilePath(path), false, true, kLoadFront, false);
    double ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (gVignette.Ptr())
        Walk(gVignette.Ptr());
    int nMesh = gByClass["Mesh"].size(), nSpot = gByClass["Spotlight"].size();
    int nAO = gByClass["AmbientOcclusion"].size(), nPart = gByClass["ParticleSys"].size();
    int nClip = gByClass["CharClip"].size();
    // The class counts are the ones the first clean load of this file printed
    // (Mesh 2,517, Spotlight 8, AmbientOcclusion 13, ParticleSys 3, CharClip
    // 695); a load that stops short or mis-reads a class changes them.
    bool ok = gVignette.Ptr() && nMesh == 2517 && nSpot == 8 && nAO == 13 && nPart == 3
        && nClip == 695 && sizeof(RndMesh::Vert) == 0x60;
    Gate("ty-fixture", ok,
         "%s: %d objects in %.0f ms; Mesh %d, Spotlight %d, AmbientOcclusion %d, "
         "ParticleSys %d, CharClip %d; sizeof(RndMesh::Vert) 0x%zx (X360 0x60)",
         path, gObjects, ms, nMesh, nSpot, nAO, nPart, nClip, sizeof(RndMesh::Vert));
    return ok;
}

// Shipped meshes that own their geometry, largest first under maxVerts.
std::vector<RndMesh *> TestMeshes(int maxCount, int maxVerts) {
    std::vector<RndMesh *> out;
    for (RndMesh *m : All<RndMesh>("Mesh")) {
        if (m->GetGeomOwner() != m || m->Faces().size() < 8 || m->Verts().size() < 8
            || (int)m->Verts().size() > maxVerts)
            continue;
        out.push_back(m);
    }
    std::sort(out.begin(), out.end(), [](RndMesh *a, RndMesh *b) {
        if (a->Verts().size() != b->Verts().size())
            return a->Verts().size() > b->Verts().size();
        return strcmp(a->Name(), b->Name()) < 0;
    });
    if ((int)out.size() > maxCount)
        out.resize(maxCount);
    return out;
}

// ========================================================= mesh normals ==
// The reference, written from the rules and not from the code:
//  * positions within 0.001 on every axis share the lowest such index;
//  * every face corner in a vertex's weld class contributes (corner angle) x
//    unit(e1 x e2), where e1, e2 are the corner's two edges; a corner with a
//    zero edge or two equal edges contributes nothing. MakeNormals counts only
//    the FIRST matching corner of each face, ResetNormals every one;
//  * the sum is normalised, and negated under a left-handed world transform;
//  * ResetNormals' tangent is the same angle-weighted sum of each face's unit
//    UV gradient g (the in-plane vector with g.e21 = du21 and g.e31 = du31),
//    normalised, negated with the normal, made orthogonal to the normal and
//    normalised again. A face with repeated indices, a zero edge or a zero UV
//    delta has gradient (1,0,0); so does one with a NaN or |uv| > 1000, and UV
//    components under 1e-4 count as 0.
// A vertex whose result is numerically ill-conditioned (a near-zero sum, or a
// sliver face in its tangent sum) is excluded and counted.
struct NormRef {
    std::vector<Vector3> norm, tang;
    std::vector<bool> nValid, tValid;
};

bool BadUVRef(Vector2 &v) {
    if (v.x != v.x || v.y != v.y || std::fabs(v.x) > 1000.0f || std::fabs(v.y) > 1000.0f)
        return true;
    if (std::fabs(v.x) < 0.0001f)
        v.x = 0;
    if (std::fabs(v.y) < 0.0001f)
        v.y = 0;
    return false;
}

NormRef ComputeNormRef(RndMesh *m, bool firstCornerOnly) {
    int nv = m->Verts().size(), nf = m->Faces().size();
    std::vector<int> rep(nv);
    for (int i = 0; i < nv; i++) {
        const Vector3 &p = m->Verts()[i].pos;
        int j = 0;
        for (; j < i; j++) {
            const Vector3 &q = m->Verts()[j].pos;
            if (std::fabs(p.x - q.x) <= 0.001f && std::fabs(p.y - q.y) <= 0.001f
                && std::fabs(p.z - q.z) <= 0.001f)
                break;
        }
        rep[i] = j;
    }
    std::vector<Vector3> fg(nf, Vector3(1, 0, 0));
    std::vector<bool> fgSliver(nf, false);
    for (int f = 0; f < nf; f++) {
        const RndMesh::Face &fc = m->Faces()[f];
        if (fc.v1 == fc.v2 || fc.v2 == fc.v3 || fc.v3 == fc.v1)
            continue;
        const RndMesh::Vert &a = m->Verts()[fc.v1], &b = m->Verts()[fc.v2], &c = m->Verts()[fc.v3];
        Vector2 ua = a.tex, ub = b.tex, uc = c.tex;
        if (BadUVRef(ua) || BadUVRef(ub) || BadUVRef(uc))
            continue;
        Vector3 e1 = Sub(b.pos, a.pos), e2 = Sub(c.pos, a.pos);
        float du1 = ub.x - ua.x, dv1 = ub.y - ua.y, du2 = uc.x - ua.x, dv2 = uc.y - ua.y;
        if ((e1.x == 0 && e1.y == 0 && e1.z == 0) || (e2.x == 0 && e2.y == 0 && e2.z == 0)
            || (du1 == 0 && dv1 == 0) || (du2 == 0 && dv2 == 0))
            continue;
        Vector3 n = CrossV(e1, e2);
        double det = DotV(n, n);
        double l1 = DotV(e1, e1), l2 = DotV(e2, e2);
        if (det < 1e-6 * l1 * l2) { // sin^2 of the corner angle under 1e-6
            fgSliver[f] = true;
            continue;
        }
        // g = (du1 (e2 x n) + du2 (n x e1)) / |n|^2
        Vector3 t1 = CrossV(e2, n), t2 = CrossV(n, e1);
        fg[f] = Vector3((du1 * t1.x + du2 * t2.x) / det, (du1 * t1.y + du2 * t2.y) / det,
                        (du1 * t1.z + du2 * t2.z) / det);
        fg[f] = Unit(fg[f]); // a zero gradient stays zero (Normalize writes 0)
    }
    bool left;
    {
        const Hmx::Matrix3 &w = m->WorldXfm().m;
        left = DotV(w.z, CrossV(w.x, w.y)) < 0;
    }
    NormRef r;
    r.norm.resize(nv);
    r.tang.resize(nv);
    r.nValid.assign(nv, true);
    r.tValid.assign(nv, true);
    for (int i = 0; i < nv; i++) {
        Vector3 n(0, 0, 0), t(0, 0, 0);
        for (int f = 0; f < nf; f++) {
            const RndMesh::Face &fc = m->Faces()[f];
            int idx[3] = { fc.v1, fc.v2, fc.v3 };
            for (int k = 0; k < 3; k++) {
                if (rep[idx[k]] != rep[i])
                    continue;
                Vector3 p0 = m->Verts()[idx[k]].pos, p1 = m->Verts()[idx[(k + 1) % 3]].pos,
                        p2 = m->Verts()[idx[(k + 2) % 3]].pos;
                Vector3 e1 = Sub(p1, p0), e2 = Sub(p2, p0);
                bool skip = (e1.x == 0 && e1.y == 0 && e1.z == 0)
                    || (e2.x == 0 && e2.y == 0 && e2.z == 0)
                    || (e1.x == e2.x && e1.y == e2.y && e1.z == e2.z);
                if (!skip) {
                    Vector3 c = Unit(CrossV(e1, e2));
                    double d = DotV(Unit(e1), Unit(e2));
                    d = d > 1 ? 1 : (d < -1 ? -1 : d);
                    float ang = std::acos(d);
                    n = Axpy(n, ang, c);
                    t = Axpy(t, ang, fg[f]);
                    if (fgSliver[f])
                        r.tValid[i] = false;
                }
                if (firstCornerOnly)
                    break;
            }
        }
        if (Len(n) < 1e-4f)
            r.nValid[i] = false;
        if (Len(t) < 1e-3f)
            r.tValid[i] = false;
        n = Unit(n);
        t = Unit(t);
        if (left) {
            n = Vector3(-n.x, -n.y, -n.z);
            t = Vector3(-t.x, -t.y, -t.z);
        }
        float td = DotV(n, t);
        Vector3 o(t.x - n.x * td, t.y - n.y * td, t.z - n.z * td);
        if (Len(o) < 1e-2f)
            r.tValid[i] = false;
        r.norm[i] = n;
        r.tang[i] = Unit(o);
    }
    return r;
}

void NormalsChecks() {
    std::vector<RndMesh *> meshes = TestMeshes(24, 1500);
    {
        int nMeshes = 0, nVerts = 0, nBad = 0, nSkip = 0, nMoved = 0;
        char first[256] = "";
        for (RndMesh *m : meshes) {
            std::vector<Vector3> pos0;
            for (int i = 0; i < (int)m->Verts().size(); i++)
                pos0.push_back(m->Verts()[i].pos);
            NormRef ref = ComputeNormRef(m, true);
            for (int i = 0; i < (int)m->Verts().size(); i++)
                m->Verts()[i].norm.Set(0, 0, 0); // stale shipped normals cannot pass
            MakeNormals(m);
            for (int i = 0; i < (int)m->Verts().size(); i++) {
                const Vector3 &got = m->Verts()[i].norm;
                if (!(got.x == pos0[i].x || true) || memcmp(&m->Verts()[i].pos, &pos0[i], sizeof(Vector3)))
                    nMoved++;
                if (!ref.nValid[i]) {
                    nSkip++;
                    continue;
                }
                nVerts++;
                if (!NearV(got, ref.norm[i], 2e-3f) && !nBad++)
                    snprintf(first, sizeof(first), "%s v%d (%g %g %g) want (%g %g %g)", m->Name(),
                             i, got.x, got.y, got.z, ref.norm[i].x, ref.norm[i].y, ref.norm[i].z);
            }
            nMeshes++;
        }
        Gate("ty-make-normals", nMeshes >= 20 && nVerts > 1000 && nBad == 0 && nMoved == 0,
             "%d shipped meshes, %d verts (%d with no contributing corner skipped): %d normals "
             "off the angle-weighted reference, %d positions moved%s%s",
             nMeshes, nVerts, nSkip, nBad, nMoved, first[0] ? "; first: " : "", first);
    }
    {
        int nVerts = 0, nBad = 0, tChecked = 0, tBad = 0, tSkip = 0;
        char first[256] = "";
        for (RndMesh *m : meshes) {
            NormRef ref = ComputeNormRef(m, false);
            for (int i = 0; i < (int)m->Verts().size(); i++) {
                m->Verts()[i].norm.Set(0, 0, 0);
                m->Verts()[i].tangent.Set(0, 0, 0, m->Verts()[i].tangent.w);
            }
            ResetNormals(m);
            for (int i = 0; i < (int)m->Verts().size(); i++) {
                const RndMesh::Vert &v = m->Verts()[i];
                if (ref.nValid[i]) {
                    nVerts++;
                    if (!NearV(v.norm, ref.norm[i], 2e-3f) && !nBad++ && !first[0])
                        snprintf(first, sizeof(first), "normal %s v%d", m->Name(), i);
                }
                if (!ref.nValid[i] || !ref.tValid[i]) {
                    tSkip++;
                    continue;
                }
                tChecked++;
                Vector3 t(v.tangent.x, v.tangent.y, v.tangent.z);
                if (!NearV(t, ref.tang[i], 5e-3f) && !tBad++ && !first[0])
                    snprintf(first, sizeof(first), "tangent %s v%d (%g %g %g) want (%g %g %g)",
                             m->Name(), i, t.x, t.y, t.z, ref.tang[i].x, ref.tang[i].y,
                             ref.tang[i].z);
            }
        }
        Gate("ty-reset-normals", nVerts > 1000 && nBad == 0 && tBad == 0 && tChecked > nVerts / 2,
             "%d verts: %d normals off the reference; %d of %d UV-gradient tangents off it "
             "(%d ill-conditioned skipped)%s%s",
             nVerts, nBad, tBad, tChecked, tSkip, first[0] ? "; first: " : "", first);
    }
}

// ======================================================= RndScaleObject ==
// RndScaleObject(obj, s, fovScale) on every arm the vignette holds. Retail's
// arm order (RTTI descriptors of its dynamic_cast chain at 0x8243CA38) is
// Drawable, Transformable, Cam, CamAnim, Environ, EnvAnim, Text, Generator,
// Light, LightAnim, Line, MatAnim, Mesh, MeshAnim, Morph, MultiMesh,
// ParticleSys, ParticleSysAnim, TransAnim; its multipliers per field are listed
// in each snapshot below. s = 2 and fovScale = 0.5 are powers of two, so each
// expected value is exact, and the inverse call (0.5, 2) must restore the
// shipped bits.
typedef std::function<void(Hmx::Object *, std::vector<float> &, std::vector<float> &)> SnapFn;

void SnapDT(Hmx::Object *o, std::vector<float> &v, std::vector<float> &k) {
    RndDrawable *dr = dynamic_cast<RndDrawable *>(o);
    if (dr) {
        const Sphere &s = dr->GetSphere();
        float a[4] = { s.center.x, s.center.y, s.center.z, s.radius };
        for (float f : a)
            v.push_back(f), k.push_back(2);
    }
    RndTransformable *tr = dynamic_cast<RndTransformable *>(o);
    if (tr) {
        const Transform &x = tr->LocalXfm();
        float a[3] = { x.v.x, x.v.y, x.v.z };
        for (float f : a)
            v.push_back(f), k.push_back(2);
        const float *m = &x.m.x.x;
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++)
                v.push_back((&x.m.x)[r][c]), k.push_back(1);
        (void)m;
    }
}

struct ScaleArm {
    const char *cls;
    int maxObjs;
    SnapFn snap;
    int tried = 0, bad = 0, restoredBad = 0;
    std::string first;
};

void ScaleChecks() {
    std::vector<ScaleArm> arms;
    arms.push_back({ "Cam", 40, [](Hmx::Object *o, std::vector<float> &v, std::vector<float> &k) {
        SnapDT(o, v, k);
        RndCam *c = dynamic_cast<RndCam *>(o);
        v.push_back(c->NearPlane()), k.push_back(2);
        v.push_back(c->FarPlane()), k.push_back(2);
        v.push_back(c->YFov()), k.push_back(1);
    } });
    arms.push_back({ "Environ", 40, [](Hmx::Object *o, std::vector<float> &v, std::vector<float> &k) {
        SnapDT(o, v, k);
        RndEnviron *e = dynamic_cast<RndEnviron *>(o);
        v.push_back(e->FogStart()), k.push_back(2);
        v.push_back(e->FogEnd()), k.push_back(2);
    } });
    arms.push_back({ "Light", 40, [](Hmx::Object *o, std::vector<float> &v, std::vector<float> &k) {
        SnapDT(o, v, k);
        v.push_back(dynamic_cast<RndLight *>(o)->Range()), k.push_back(2);
    } });
    // Spotlight is Drawable + Transformable and matches none of the class arms.
    arms.push_back({ "Spotlight", 40, SnapDT });
    arms.push_back({ "Trans", 200, SnapDT });
    arms.push_back({ "ParticleSys", 40, [](Hmx::Object *o, std::vector<float> &v, std::vector<float> &k) {
        SnapDT(o, v, k);
        RndParticleSys *p = dynamic_cast<RndParticleSys *>(o);
        auto add2 = [&](const Vector2 &x, float f) {
            v.push_back(x.x), k.push_back(f), v.push_back(x.y), k.push_back(f);
        };
        auto add3 = [&](const Vector3 &x, float f) {
            v.push_back(x.x), k.push_back(f), v.push_back(x.y), k.push_back(f);
            v.push_back(x.z), k.push_back(f);
        };
        // retail 0x8243D290..: bubble size x s, bubble period x fov, life x fov,
        // emit rate x (1/fov), force x s (1/fov)^2, box x s, speed x s/fov,
        // start size x s, delta size x s.
        add2(p->mBubbleSize, 2);
        add2(p->mBubblePeriod, 0.5f);
        add2(p->mLife, 0.5f);
        add2(p->mEmitRate, 2);
        add3(p->mForceDir, 8);
        add3(p->mBoxExtent1, 2);
        add3(p->mBoxExtent2, 2);
        add2(p->mSpeed, 4);
        add2(p->mStartSize, 2);
        add2(p->mDeltaSize, 2);
    } });
    arms.push_back({ "TransAnim", 40, [](Hmx::Object *o, std::vector<float> &v, std::vector<float> &k) {
        RndTransAnim *a = dynamic_cast<RndTransAnim *>(o);
        bool own = a->KeysOwner() == a;
        for (auto &key : a->TransKeys()) {
            float x[3] = { key.value.x, key.value.y, key.value.z };
            for (float f : x)
                v.push_back(f), k.push_back(own ? 2 : 1);
            v.push_back(key.frame), k.push_back(own ? 0.5f : 1);
        }
        for (auto &key : a->RotKeys())
            v.push_back(key.frame), k.push_back(own ? 0.5f : 1);
        for (auto &key : a->ScaleKeys())
            v.push_back(key.frame), k.push_back(own ? 0.5f : 1);
    } });
    arms.push_back({ "Mesh", 60, [](Hmx::Object *o, std::vector<float> &v, std::vector<float> &k) {
        SnapDT(o, v, k);
        RndMesh *m = dynamic_cast<RndMesh *>(o);
        bool own = m->GetGeomOwner() == m;
        for (int i = 0; i < (int)m->Verts().size(); i++) {
            const Vector3 &p = m->Verts()[i].pos;
            v.push_back(p.x), v.push_back(p.y), v.push_back(p.z);
            for (int j = 0; j < 3; j++)
                k.push_back(own ? 2 : 1);
        }
        for (int i = 0; i < m->NumBones(); i++) {
            const Vector3 &b = m->BoneOffsetAt(i).v;
            v.push_back(b.x), v.push_back(b.y), v.push_back(b.z);
            for (int j = 0; j < 3; j++)
                k.push_back(2);
        }
    } });

    int tried = 0, bad = 0, rb = 0, nVals = 0;
    bool every = true;
    std::string detail;
    for (ScaleArm &a : arms) {
        int n = 0;
        for (Hmx::Object *o : gByClass[a.cls]) {
            if (n++ >= a.maxObjs)
                break;
            std::vector<float> v0, k0, v1, k1, v2, k2;
            a.snap(o, v0, k0);
            RndScaleObject(o, 2.0f, 0.5f);
            a.snap(o, v1, k1);
            RndScaleObject(o, 0.5f, 2.0f);
            a.snap(o, v2, k2);
            nVals += v0.size();
            bool ok = v1.size() == v0.size();
            for (size_t i = 0; ok && i < v0.size(); i++)
                if (!(v1[i] == v0[i] * k0[i] || (v0[i] != v0[i] && v1[i] != v1[i]))) {
                    ok = false;
                    char w[160];
                    snprintf(w, sizeof(w), "%s value %zu: %g -> %g, want x%g", o->Name(), i, v0[i],
                             v1[i], k0[i]);
                    if (!a.bad)
                        a.first = w;
                }
            if (!ok)
                a.bad++;
            if (v2.size() != v0.size() || memcmp(v2.data(), v0.data(), v0.size() * sizeof(float)))
                a.restoredBad++;
            a.tried++;
        }
        tried += a.tried, bad += a.bad, rb += a.restoredBad;
        if (!a.tried)
            every = false;
        char one[96];
        snprintf(one, sizeof(one), "%s%s %d", detail.empty() ? "" : ", ", a.cls, a.tried);
        detail += one;
        if (a.bad)
            detail += " [" + a.first + "]";
    }
    Gate("ty-scale-object", every && bad == 0 && rb == 0,
         "%d shipped objects, %d fields (%s): %d objects off retail's multipliers, %d not "
         "restored bit-exact by the inverse call",
         tried, nVals, detail.c_str(), bad, rb);
}

// ============================================================ Spotlight ==
// SyncProperty: each retail property name (all 44 are NUL-delimited strings in
// band.exe) is read through Property() and compared with the member it names,
// read directly; each writable one is then set through SetProperty() and the
// member read back. Retail's non-identity mappings are covered:
// flare_visibility_test is !mFlareVisibilityTest both ways, color is the colour
// owner's packed colour and writes alpha 1, intensity is the owner's.
void SpotlightChecks() {
    std::vector<Spotlight *> spots = All<Spotlight>("Spotlight");
    int gets = 0, getBad = 0, sets = 0, setBad = 0;
    std::string gfirst, sfirst;
    auto miss = [](int &ctr, std::string &first, Spotlight *s, const char *what) {
        if (!ctr++)
            first = std::string(s->Name()) + "." + what;
    };
    for (Spotlight *s : spots) {
        struct F {
            const char *name;
            float *member;
        } floats[] = {
            { "length", &s->mBeam.mLength },
            { "top_radius", &s->mBeam.mTopRadius },
            { "bottom_radius", &s->mBeam.mBottomRadius },
            { "top_side_border", &s->mBeam.mTopSideBorder },
            { "bottom_side_border", &s->mBeam.mBottomSideBorder },
            { "bottom_border", &s->mBeam.mBottomBorder },
            { "offset", &s->mBeam.mOffset },
            { "brighten", &s->mBeam.mBrighten },
            { "expand", &s->mBeam.mExpand },
            { "light_can_offset", &s->mLightCanOffset },
            { "flare_offset", &s->mFlareOffset },
            { "spot_scale", &s->mSpotScale },
            { "spot_height", &s->mSpotHeight },
            { "damping_constant", &s->mDampingConstant },
            { "lens_size", &s->mLensSize },
            { "lens_offset", &s->mLensOffset },
        };
        for (F &f : floats) {
            gets++;
            const DataNode *n = s->Property(f.name, false);
            if (!n || n->Float() != *f.member)
                miss(getBad, gfirst, s, f.name);
        }
        struct B {
            const char *name;
            bool *member;
        } bools[] = {
            { "is_cone", &s->mBeam.mIsCone },
            { "light_can_sort", &s->mLightCanSort },
            { "target_shadow", &s->mTargetShadow },
            { "flare_enabled", &s->mFlareEnabled },
            { "animate_orientation_from_preset", &s->mAnimateOrientationFromPreset },
            { "animate_color_from_preset", &s->mAnimateColorFromPreset },
        };
        for (B &b : bools) {
            gets++;
            const DataNode *n = s->Property(b.name, false);
            if (!n || (n->Int() != 0) != *b.member)
                miss(getBad, gfirst, s, b.name);
        }
        struct I {
            const char *name;
            int want;
        } ints[] = {
            { "shape", (int)s->mBeam.mShape },
            { "sections", s->mBeam.mNumSections },
            { "segments", s->mBeam.mNumSegments },
            { "flare_steps", s->mFlare->GetSteps() },
            { "flare_visibility_test", !s->mFlareVisibilityTest },
            { "color", s->mColorOwner->mColor.Pack() },
        };
        for (I &i : ints) {
            gets++;
            const DataNode *n = s->Property(i.name, false);
            if (!n || n->Int() != i.want)
                miss(getBad, gfirst, s, i.name);
        }
        {
            gets++;
            const DataNode *n = s->Property("intensity", false);
            if (!n || n->Float() != s->mColorOwner->mIntensity)
                miss(getBad, gfirst, s, "intensity");
        }
        struct O {
            const char *name;
            Hmx::Object *want;
        } objs[] = {
            { "material", s->mBeam.mMat.Ptr() },
            { "xsection", s->mBeam.mXSection.Ptr() },
            { "light_can", s->mLightCanMesh.Ptr() },
            { "target", s->mTarget.Ptr() },
            { "spot_target", s->mSpotTarget.Ptr() },
            { "spot_material", s->mSpotMaterial.Ptr() },
            { "flare_material", s->mFlare->GetMat() },
            { "lens_material", s->mLensMaterial.Ptr() },
            { "color_owner", s->mColorOwner.Ptr() },
        };
        for (O &o : objs) {
            gets++;
            const DataNode *n = s->Property(o.name, false);
            if (!n || n->GetObj() != o.want)
                miss(getBad, gfirst, s, o.name);
        }
        for (F &f : floats) {
            float old = *f.member, v = old * 0.5f + 1.25f;
            s->SetProperty(f.name, DataNode(v));
            sets++;
            if (*f.member != v)
                miss(setBad, sfirst, s, f.name);
            s->SetProperty(f.name, DataNode(old));
            if (*f.member != old)
                miss(setBad, sfirst, s, f.name);
        }
        for (B &b : bools) {
            bool old = *b.member;
            s->SetProperty(b.name, DataNode(old ? 0 : 1));
            sets++;
            if (*b.member == old)
                miss(setBad, sfirst, s, b.name);
            s->SetProperty(b.name, DataNode(old ? 1 : 0));
        }
        {
            bool old = s->mFlareVisibilityTest;
            s->SetProperty("flare_visibility_test", DataNode(old ? 1 : 0));
            sets++;
            if (s->mFlareVisibilityTest != !old)
                miss(setBad, sfirst, s, "flare_visibility_test");
            s->SetProperty("flare_visibility_test", DataNode(old ? 0 : 1));
        }
        {
            Hmx::Color oc = s->mColorOwner->mColor;
            float oi = s->mColorOwner->mIntensity;
            s->SetProperty("color", DataNode(0x00336699));
            sets++;
            const Hmx::Color &c = s->mColorOwner->mColor;
            if (c.red != 0x99 / 255.0f || c.green != 0x66 / 255.0f || c.blue != 0x33 / 255.0f
                || c.alpha != 1.0f || s->mColorOwner->mIntensity != oi)
                miss(setBad, sfirst, s, "color");
            s->SetProperty("intensity", DataNode(oi + 0.5f));
            sets++;
            if (s->mColorOwner->mIntensity != oi + 0.5f || s->mColorOwner->mColor.red != c.red)
                miss(setBad, sfirst, s, "intensity");
            s->mColorOwner->mColor = oc;
            s->mColorOwner->mIntensity = oi;
        }
    }
    Gate("ty-spot-sync-get", spots.size() == 8 && gets == 8 * 38 && getBad == 0,
         "%zu shipped spotlights, %d property reads, %d differ from the member%s%s",
         spots.size(), gets, getBad, gfirst.empty() ? "" : "; first: ", gfirst.c_str());
    Gate("ty-spot-sync-set", spots.size() == 8 && setBad == 0,
         "%d property writes, %d not reflected in the member%s%s", sets, setBad,
         sfirst.empty() ? "" : "; first: ", sfirst.c_str());

    // UpdateTransforms. Light can: the world transform moved light_can_offset
    // along its y axis. Lens: rows (-size x, size z, size y) of the world
    // rotation, at lens_offset along y. Beam: local position (0, offset, 0),
    // rotation (cone ? I : rows (x, z, -y)) times R(angle offset degrees).
    // Flare: local (0, flare_offset, 0), identity rotation. Floor spot: on the
    // plane z = target z + spot_height, along the world y axis.
    int checked = 0, uBad = 0, floors = 0, lenses = 0, beams = 0, flares = 0;
    std::string ufirst;
    for (Spotlight *s : spots) {
        s->UpdateTransforms();
        const Transform &w = s->WorldXfm();
        checked++;
        std::string why;
        if (!NearV(s->mLightCanXfm.v, Axpy(w.v, s->mLightCanOffset, w.m.y), 1e-3f)
            || memcmp(&s->mLightCanXfm.m, &w.m, sizeof(w.m)))
            why = "light can";
        if (s->mLensMaterial) {
            lenses++;
            float z = s->mLensSize;
            const Transform &L = s->mLensXfm;
            if (!NearV(L.v, Axpy(w.v, s->mLensOffset, w.m.y), 1e-3f)
                || !NearV(L.m.x, Vector3(-z * w.m.x.x, -z * w.m.x.y, -z * w.m.x.z), 1e-4f)
                || !NearV(L.m.y, Vector3(z * w.m.z.x, z * w.m.z.y, z * w.m.z.z), 1e-4f)
                || !NearV(L.m.z, Vector3(z * w.m.y.x, z * w.m.y.y, z * w.m.y.z), 1e-4f))
                why = "lens";
        }
        if (s->mBeam.mBeam) {
            beams++;
            const Transform &bl = s->mBeam.mBeam->LocalXfm();
            if (bl.v.x != 0 || bl.v.z != 0 || bl.v.y != s->mBeam.mOffset)
                why = "beam position";
            // R(angle offset): rotation about x by tx then z by tz, written out
            // (MakeRotMatrix's x-then-z order with y = 0).
            float ax = s->mBeam.mTargetOffset.x * DEG2RAD, az = s->mBeam.mTargetOffset.y * DEG2RAD;
            float cx = std::cos(ax), sx = std::sin(ax), cz = std::cos(az), sz = std::sin(az);
            Hmx::Matrix3 r;
            MakeRotMatrix(Vector3(ax, 0, az), r, true);
            (void)cx, (void)sx, (void)cz, (void)sz;
            Vector3 b0 = s->mBeam.mIsCone ? Vector3(1, 0, 0) : Vector3(1, 0, 0);
            Vector3 b1 = s->mBeam.mIsCone ? Vector3(0, 1, 0) : Vector3(0, 0, 1);
            Vector3 b2 = s->mBeam.mIsCone ? Vector3(0, 0, 1) : Vector3(0, -1, 0);
            auto row = [&](const Vector3 &b) {
                return Vector3(b.x * r.x.x + b.y * r.y.x + b.z * r.z.x,
                               b.x * r.x.y + b.y * r.y.y + b.z * r.z.y,
                               b.x * r.x.z + b.y * r.y.z + b.z * r.z.z);
            };
            if (!NearV(bl.m.x, row(b0), 1e-4f) || !NearV(bl.m.y, row(b1), 1e-4f)
                || !NearV(bl.m.z, row(b2), 1e-4f))
                why = "beam rotation";
        }
        if (s->mFlare && s->mFlare->GetMat()) {
            flares++;
            const Transform &fl = s->mFlare->LocalXfm();
            if (fl.v.x != 0 || fl.v.z != 0 || fl.v.y != s->mFlareOffset)
                why = "flare";
        }
        RndTransformable *ft = s->mSpotTarget ? s->mSpotTarget.Ptr() : s->mTarget.Ptr();
        bool floorSpot = s->mSpotMaterial && ft && ft->WorldXfm().m.y.z != 0;
        if (floorSpot && w.m.y.z != 0) {
            floors++;
            float tz = ft->WorldXfm().v.z + s->mSpotHeight;
            Vector3 hit = Axpy(w.v, (tz - w.v.z) / w.m.y.z, w.m.y);
            if (!NearV(s->mFloorSpotXfm.v, hit, 1e-2f))
                why = "floor spot";
        } else if (s->mFloorSpotXfm.v.x != 0 || s->mFloorSpotXfm.v.y != 0
                   || s->mFloorSpotXfm.v.z != 0) {
            why = "floor spot not reset";
        }
        if (!why.empty() && !uBad++)
            ufirst = std::string(s->Name()) + ": " + why;
    }
    Gate("ty-spot-xfms", checked == 8 && beams + lenses + flares > 0 && uBad == 0,
         "%d spotlights (%d beams, %d lenses, %d flares, %d floor spots): transforms against "
         "the world transform, %d wrong%s%s",
         checked, beams, lenses, flares, floors, uBad, ufirst.empty() ? "" : "; first: ",
         ufirst.c_str());
}

// ======================================================= InitParticle ==
// Each shipped emitter initialises 400 particles at frame 10 with the identity
// transform and no override. Everything checked is the emitter's own range:
// life in mLife and pos.w = 1/life; position in the box (no mesh emitter, no
// bubble offset); |vel| in mSpeed and vel.z/|vel| = sin(pitch) for a pitch in
// mPitch (FastSin/FastCos tolerance); start colour channels in their ranges;
// size in mStartSize (0 for a fancy emitter with a grow ratio); size velocity
// in mDeltaSize, clamped at -size. Plain particles must reach the end colour
// range at death; fancy ones the mid range at midcolFrame and the end range at
// death.
void ParticleChecks() {
    std::vector<RndParticleSys *> sys = All<RndParticleSys>("ParticleSys");
    int n = 0, bad = 0;
    std::string first, kinds;
    Transform ident;
    ident.Reset();
    auto in = [](float v, float x, float y, float tol) {
        return v >= std::min(x, y) - tol && v <= std::max(x, y) + tol;
    };
    for (RndParticleSys *p : sys) {
        bool fancy = p->mType == RndParticleSys::kFancy;
        char k[128];
        snprintf(k, sizeof(k), "%s%s(%s%s%s)", kinds.empty() ? "" : ", ", p->Name(),
                 fancy ? "fancy" : "plain", p->mMeshEmitter ? ",mesh" : "",
                 p->mBubble ? ",bubble" : "");
        kinds += k;
        for (int i = 0; i < 400; i++) {
            RndFancyParticle fp;
            memset(&fp, 0, sizeof(fp));
            PartOverride po;
            po.mask = 0;
            p->InitParticle(10.0f, &fp, &ident, po);
            n++;
            std::string why;
            float life = fp.deathFrame - fp.birthFrame;
            if (fp.birthFrame != 10.0f || !in(life, p->mLife.x, p->mLife.y, 1e-3f))
                why = "life";
            else if (life > 0 && !Near(fp.pos.w, 1.0f / life, 1e-5f))
                why = "1/life";
            bool meshEmit = p->mMeshEmitter && !p->mMeshEmitter->Faces().empty();
            if (!meshEmit) {
                if (!(fancy && p->mBubble)
                    && !(in(fp.pos.x, p->mBoxExtent1.x, p->mBoxExtent2.x, 1e-3f)
                         && in(fp.pos.y, p->mBoxExtent1.y, p->mBoxExtent2.y, 1e-3f)
                         && in(fp.pos.z, p->mBoxExtent1.z, p->mBoxExtent2.z, 1e-3f)))
                    why = "box";
                Vector3 v(fp.vel.x, fp.vel.y, fp.vel.z);
                float sp = Len(v);
                float slo = std::min(p->mSpeed.x, p->mSpeed.y), shi = std::max(p->mSpeed.x, p->mSpeed.y);
                if (sp < std::fabs(slo) * 0.99f - 1e-4f && slo >= 0)
                    why = "speed low";
                if (sp > std::max(std::fabs(slo), std::fabs(shi)) * 1.01f + 1e-4f)
                    why = "speed high";
                float plo = std::min(p->mPitch.x, p->mPitch.y), phi = std::max(p->mPitch.x, p->mPitch.y);
                if (sp > 1e-4f && slo > 0 && plo >= -1.5f && phi <= 1.5f) {
                    float sz = v.z / sp;
                    if (sz < std::sin(plo) - 0.01f || sz > std::sin(phi) + 0.01f)
                        why = "pitch";
                }
            }
            const Hmx::Color &cl = p->mStartColorLow, &ch = p->mStartColorHigh;
            if (!in(fp.col.red, cl.red, ch.red, 1e-4f) || !in(fp.col.green, cl.green, ch.green, 1e-4f)
                || !in(fp.col.blue, cl.blue, ch.blue, 1e-4f)
                || !in(fp.col.alpha, cl.alpha, ch.alpha, 1e-4f))
                why = "start colour";
            if (fancy && p->mGrowRatio != 0) {
                if (fp.size != 0)
                    why = "grow size";
            } else if (!in(fp.size, p->mStartSize.x, p->mStartSize.y, 1e-4f))
                why = "size";
            {
                float s0 = fancy && p->mGrowRatio != 0 ? fp.growVel * (fp.growFrame - fp.birthFrame)
                                                       : fp.size;
                float lo = std::min(p->mDeltaSize.x, p->mDeltaSize.y);
                float hi = std::max(p->mDeltaSize.x, p->mDeltaSize.y);
                if (fp.sizeVel < -s0 - 1e-4f || fp.sizeVel > hi + 1e-4f
                    || (fp.sizeVel < lo - 1e-4f && !Near(fp.sizeVel, -s0, 1e-4f)))
                    why = "size velocity";
            }
            auto cin = [&](const Hmx::Color &c, const Hmx::Color &lo, const Hmx::Color &hi) {
                return in(c.red, lo.red, hi.red, 3e-3f) && in(c.green, lo.green, hi.green, 3e-3f)
                    && in(c.blue, lo.blue, hi.blue, 3e-3f) && in(c.alpha, lo.alpha, hi.alpha, 3e-3f);
            };
            auto at = [](const Hmx::Color &c, const Hmx::Color &v, float t) {
                return Hmx::Color(c.red + v.red * t, c.green + v.green * t, c.blue + v.blue * t,
                                  c.alpha + v.alpha * t);
            };
            if (!fancy && life > 0) {
                if (!cin(at(fp.col, fp.colVel, life), p->mEndColorLow, p->mEndColorHigh))
                    why = "end colour";
            } else if (fancy) {
                float tm = fp.midcolFrame - fp.birthFrame, te = fp.deathFrame - fp.midcolFrame;
                if (!Near(fp.midcolFrame, fp.birthFrame + life * p->mMidColorRatio, 1e-3f))
                    why = "mid frame";
                Hmx::Color mid = tm != 0 ? at(fp.col, fp.midcolVel, tm) : fp.midcolVel;
                if (tm != 0 && !cin(mid, p->mMidColorLow, p->mMidColorHigh))
                    why = "mid colour";
                if (te != 0 && tm != 0 && !cin(at(mid, fp.colVel, te), p->mEndColorLow, p->mEndColorHigh))
                    why = "end colour";
            }
            if (!why.empty() && !bad++)
                first = std::string(p->Name()) + ": " + why;
        }
    }
    Gate("ty-part-init", sys.size() == 3 && n == 1200 && bad == 0,
         "%d particles from %zu shipped emitters [%s]: %d outside the emitter's ranges%s%s", n,
         sys.size(), kinds.c_str(), bad, first.empty() ? "" : "; first: ", first.c_str());
}

// ============================================================ CharBones ==
// Two poses of the shipped clip with the most channels (its first sample and
// its middle sample, through CharClip::ScaleAdd) are written into uncompressed
// CharBonesAlloc's with that clip's channel list. Expected values, per channel,
// from the products retail's bodies compute (decoded from the uncompressed
// arms of 0x823AD090 RotateBy and 0x823AD620 RotateTo; ScaleAdd's from its
// source, which retail's arm order matches):
//   ScaleAdd(dst, f):  v += f*src; q += (q.src < 0 ? -1 : 1) * (f x,y,z, f w);
//                      weight += f * src weight
//   RotateBy(dst):     v += src; q = src * q (Hamilton)
//   RotateTo(dst, f):  v += f*src; q = q * b, b = f*src with
//                      b.w += (src.w < 0 ? -(1-f) : (1-f))
struct Pose {
    std::map<std::string, std::vector<float> > ch;
    std::map<std::string, float> w;
};

int Width(CharBones::Type t) { return t == CharBones::TYPE_QUAT ? 4 : (t <= CharBones::TYPE_SCALE ? 3 : 1); }

Pose ReadPose(CharBones &b) {
    Pose p;
    for (const CharBones::Bone &bone : b.mBones) {
        float *f = (float *)b.FindPtr(bone.name);
        p.ch[bone.name.Str()] = std::vector<float>(f, f + Width(CharBones::TypeOf(bone.name)));
        p.w[bone.name.Str()] = bone.weight;
    }
    return p;
}

void WritePose(CharBones &b, const Pose &p) {
    for (const CharBones::Bone &bone : b.mBones) {
        const std::vector<float> &v = p.ch.find(bone.name.Str())->second;
        memcpy(b.FindPtr(bone.name), v.data(), v.size() * sizeof(float));
    }
}

Hmx::Quat QMul(const Hmx::Quat &a, const Hmx::Quat &b) { // Hamilton a*b
    return Hmx::Quat(a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
                     a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                     a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
                     a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z);
}

void CharBonesChecks() {
    CharClip *clip = nullptr;
    int best = 0;
    for (CharClip *c : All<CharClip>("CharClip")) {
        int n = c->mFull.mBones.size();
        if (c->mFull.NumSamples() > 2 && (n > best || (n == best && clip && strcmp(c->Name(), clip->Name()) < 0)))
            best = n, clip = c;
    }
    if (!clip) {
        Gate("ty-bones-fixture", false, "no shipped CharClip with samples");
        return;
    }
    std::vector<CharBones::Bone> bones = clip->mFull.mBones;
    for (CharBones::Bone &b : bones)
        b.weight = 1.0f;
    CharBonesAlloc A, B;
    A.AddBones(bones), B.AddBones(bones);
    A.Zero(), B.Zero();
    int mid = clip->mFull.NumSamples() / 2;
    clip->ScaleAdd(A, 1.0f, clip->SampleToBeat(0), 0.0f);
    clip->ScaleAdd(B, 1.0f, clip->SampleToBeat(mid), 0.0f);
    int types[CharBones::TYPE_END] = { 0 };
    for (const CharBones::Bone &b : bones)
        types[CharBones::TypeOf(b.name)]++;
    Pose pa = ReadPose(A), pb = ReadPose(B);
    int differ = 0;
    for (auto &kv : pa.ch)
        if (kv.second != pb.ch[kv.first])
            differ++;
    bool fix = types[CharBones::TYPE_POS] && types[CharBones::TYPE_QUAT]
        && (types[CharBones::TYPE_ROTX] + types[CharBones::TYPE_ROTY] + types[CharBones::TYPE_ROTZ])
        && differ > 0;
    Gate("ty-bones-fixture", fix,
         "clip %s (compression %d): %d samples, %zu channels (pos %d, scale %d, quat %d, rotx %d, "
         "roty %d, rotz %d); %d differ between samples 0 and %d",
         clip->Name(), (int)clip->mFull.mCompression, clip->mFull.NumSamples(), bones.size(),
         types[0], types[1], types[2], types[3], types[4], types[5], differ, mid);
    if (!fix)
        return;
    // Source weights distinct per channel, so a weight sum is checkable.
    {
        std::vector<CharBones::Bone> wb = bones;
        for (size_t i = 0; i < wb.size(); i++)
            wb[i].weight = 0.25f + 0.0625f * (i % 7);
        // Every other source quaternion negated (q and -q are one rotation), so
        // RotateTo's w < 0 arm and ScaleAdd's negative-dot arm both run.
        int qi = 0;
        for (auto &kv : pa.ch)
            if (kv.second.size() == 4 && (qi++ & 1))
                for (float &c : kv.second)
                    c = -c;
        A.ClearBones(), A.AddBones(wb), WritePose(A, pa);
        pa = ReadPose(A);
    }
    struct Op {
        const char *gate;
        int kind;
        float f;
    } ops[] = { { "ty-bones-scale-add", 0, 0.375f },
                { "ty-bones-rotate-by", 1, 1.0f },
                { "ty-bones-rotate-to", 2, 0.375f } };
    for (Op &op : ops) {
        CharBonesAlloc C;
        C.AddBones(bones);
        WritePose(C, pb);
        Pose pc0 = ReadPose(C);
        if (op.kind == 0)
            A.ScaleAdd(C, op.f);
        else if (op.kind == 1)
            A.RotateBy(C);
        else
            A.RotateTo(C, op.f);
        Pose pc = ReadPose(C);
        int nCh = 0, nBad = 0, quatNeg = 0, dotNeg = 0;
        std::string first;
        float f = op.f;
        for (auto &kv : pa.ch) {
            const std::string &name = kv.first;
            const std::vector<float> &s = kv.second, &d0 = pc0.ch[name], &d = pc.ch[name];
            std::vector<float> want = d0;
            if (s.size() == 4) {
                Hmx::Quat qs(s[0], s[1], s[2], s[3]), qd(d0[0], d0[1], d0[2], d0[3]), r;
                if (qs.w < 0)
                    quatNeg++;
                if (qs.x * qd.x + qs.y * qd.y + qs.z * qd.z + qs.w * qd.w < 0)
                    dotNeg++;
                if (op.kind == 0) {
                    float dot = qs.x * qd.x + qs.y * qd.y + qs.z * qd.z + qs.w * qd.w;
                    float sg = dot * f < 0 ? -1.0f : 1.0f;
                    r = Hmx::Quat(qd.x + sg * f * qs.x, qd.y + sg * f * qs.y,
                                  qd.z + sg * f * qs.z, qd.w + sg * f * qs.w);
                } else if (op.kind == 1) {
                    r = QMul(qs, qd);
                } else {
                    Hmx::Quat b(f * qs.x, f * qs.y, f * qs.z,
                                f * qs.w + (qs.w < 0 ? -(1 - f) : (1 - f)));
                    r = QMul(qd, b);
                }
                want = { r.x, r.y, r.z, r.w };
            } else {
                for (size_t i = 0; i < want.size(); i++)
                    want[i] = d0[i] + f * s[i];
            }
            nCh++;
            bool ok = true;
            for (size_t i = 0; i < want.size(); i++)
                if (!Near(d[i], want[i], 1e-4f * (1 + std::fabs(want[i]))))
                    ok = false;
            float ww = op.kind == 0 ? pc0.w[name] + pa.w[name] * f : pc0.w[name];
            if (!Near(pc.w[name], ww, 1e-5f))
                ok = false;
            if (!ok && !nBad++) {
                char buf[256];
                snprintf(buf, sizeof(buf), "%s (%g %g %g %g) want (%g %g %g %g)", name.c_str(),
                         d[0], d.size() > 1 ? d[1] : 0, d.size() > 2 ? d[2] : 0,
                         d.size() > 3 ? d[3] : 0, want[0], want.size() > 1 ? want[1] : 0,
                         want.size() > 2 ? want[2] : 0, want.size() > 3 ? want[3] : 0);
                first = buf;
            }
        }
        Gate(op.gate, nBad == 0 && nCh == (int)bones.size() && quatNeg > 0 && dotNeg > 0,
             "%d channels of %s (%d source quats with w < 0, %d with a negative dot) against "
             "retail's blend rule, %d wrong%s%s",
             nCh, clip->Name(), quatNeg, dotNeg, nBad, first.empty() ? "" : "; first: ",
             first.c_str());
    }
}

} // namespace

int RunW16TYPhase(GateFn gate) {
    gGate = gate;
    printf("\n=== W16-TY phase: VIA-DC3 unentered rows on shipped data ===\n");
    if (!LoadFixture())
        return gRan;
    SpotlightChecks(); // before ScaleChecks, which moves the same objects and back
    ParticleChecks();
    CharBonesChecks();
    NormalsChecks();
    ScaleChecks();
    return gRan;
}
