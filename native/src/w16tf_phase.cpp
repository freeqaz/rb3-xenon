// rb3-xenon native -- W16-TF: gates for engine math/utl gap rows that no native
// target entered.
//
// tools/native_runtime_rank.py (rerun by W16-TF, after its rb3-render profile
// fix) listed these in-scope gap rows as linked into native targets but never
// executed. Each gate has a reference answer that does not come from the code
// under test:
//
//   geo-bsp-*      MakeBSPTree / BSPFace::Set+Update over two closed meshes, a
//                  cube and a concave L prism. Each mesh is validated first by
//                  its signed volume (8 and 3). The tree is then checked against
//                  the solid's analytic inside test on fixed and 4,000 random
//                  points, CheckBSPTree on a box that encloses the solid (true)
//                  and one that does not (false), and Intersect(Segment, BSP)
//                  against the analytic entry parameter and face plane.
//                  MakeBSPTree was `return false;` natively until this lane.
//   geo-clip-*     Clip(Polygon, Ray, Polygon), copy and in-place, against the
//                  shoelace area and half-plane membership of the result.
//   geo-tri-box    Intersect(Triangle, Box): inside, far, separated by the face
//                  plane, separated only by an edge-cross axis, and crossing.
//   geo-seg-tri    Intersect(Segment, Triangle, bool, float&): the hit
//                  parameter, the back-face flag, outside, past the end, parallel.
//   geo-ray-box    Intersect(origin, dir, Box, tmin, tmax): slab entry/exit.
//   geo-sphere-*   Sphere::GrowToContain: the minimal enclosing sphere.
//   geo-frustum-*  Frustum::Set + operator>(Sphere, Frustum) against the
//                  frustum's own half-space definition, perspective and ortho.
//   geo-plane-xform Multiply(Plane, Transform, Plane): transformed points lie on
//                  the transformed plane, and sides are preserved.
//   rot-make-rot-quat MakeRotQuat: the half-angle quaternion about v1 x v2.
//   interp-*       LinearInterpolator::Reset(DataArray), ATanInterpolator::
//                  Reset/Eval, InvExpInterpolator::Eval against closed forms.
//   key-interp-tangent InterpTangent against a numerical derivative of the
//                  cubic Hermite curve.
//   utf8-to-ascii  UTF8toASCIIs: Latin-1 passes, the rest is substituted, and
//                  the output is truncated at len - 1.
//   sfs-*          SuperFormatString's placeholder grammar on literal strings.
//
// Each gate can fail. See docs/decomp/W16TF_NATIVE_RUNTIME_COVERAGE_2026-10-07.md.

#include "math/Geo.h"
#include "math/Interp.h"
#include "math/Key.h"
#include "math/Mtx.h"
#include "math/Rot.h"
#include "math/Sphere.h"
#include "math/Vec.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "utl/SuperFormatString.h"
#include "utl/UTF8.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <list>
#include <vector>

// Defined in math/Geo.cpp with external linkage; Geo.h declares only the
// DataArray overload.
void SetBSPParams(float, float, int, int, float);

namespace {

typedef void (*GateFn)(const char *, bool, const char *);
GateFn gGate = nullptr;
char gBuf[512];

void Gate(const char *name, bool ok, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
void Gate(const char *name, bool ok, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(gBuf, sizeof(gBuf), fmt, ap);
    va_end(ap);
    gGate(name, ok, gBuf);
}

bool Close(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

// ------------------------------------------------------------------ BSP ----

struct Tri {
    Vector3 a, b, c;
};

void Quad(std::vector<Tri> &t, const Vector3 &a, const Vector3 &b, const Vector3 &c,
          const Vector3 &d) {
    t.push_back({ a, b, c });
    t.push_back({ a, c, d });
}

struct Rect2 {
    float x0, y0, x1, y1;
};

// A prism over a counter-clockwise polygon, z0..z1, with outward-facing
// triangles (cross(b - a, c - a) points out). The caps are given as rectangles
// that tile the polygon.
std::vector<Tri> Prism(const std::vector<Vector2> &poly, const std::vector<Rect2> &caps,
                       float z0, float z1) {
    std::vector<Tri> t;
    for (size_t i = 0; i < poly.size(); i++) {
        const Vector2 &p = poly[i], &q = poly[(i + 1) % poly.size()];
        Quad(t, Vector3(p.x, p.y, z0), Vector3(q.x, q.y, z0), Vector3(q.x, q.y, z1),
             Vector3(p.x, p.y, z1));
    }
    for (const Rect2 &r : caps) {
        Quad(t, Vector3(r.x0, r.y0, z1), Vector3(r.x1, r.y0, z1), Vector3(r.x1, r.y1, z1),
             Vector3(r.x0, r.y1, z1));
        Quad(t, Vector3(r.x0, r.y0, z0), Vector3(r.x0, r.y1, z0), Vector3(r.x1, r.y1, z0),
             Vector3(r.x1, r.y0, z0));
    }
    return t;
}

// Divergence theorem: sum of a . (b x c) / 6 over an outward-oriented closed
// surface is its volume. This validates the fixture, not the code under test.
double SignedVolume(const std::vector<Tri> &t) {
    double v = 0;
    for (const Tri &f : t) {
        double bx = f.b.x, by = f.b.y, bz = f.b.z, cx = f.c.x, cy = f.c.y, cz = f.c.z;
        v += f.a.x * (by * cz - bz * cy) + f.a.y * (bz * cx - bx * cz)
            + f.a.z * (bx * cy - by * cx);
    }
    return v / 6.0;
}

bool InCube(const Vector3 &p) {
    return std::fabs(p.x) < 1 && std::fabs(p.y) < 1 && std::fabs(p.z) < 1;
}
double CubeMargin(const Vector3 &p) {
    return std::fmin(std::fmin(std::fabs(std::fabs(p.x) - 1), std::fabs(std::fabs(p.y) - 1)),
                     std::fabs(std::fabs(p.z) - 1));
}
// L = [0,2]x[0,1] u [0,1]x[1,2], z in [0,1].
bool InL(const Vector3 &p) {
    if (p.z <= 0 || p.z >= 1 || p.x <= 0 || p.y <= 0)
        return false;
    return (p.x < 2 && p.y < 1) || (p.x < 1 && p.y < 2);
}
double LMargin(const Vector3 &p) {
    const double planes[] = { p.x, p.y, p.z, p.z - 1, p.x - 2, p.y - 2, p.x - 1, p.y - 1 };
    double m = 1e9;
    for (double d : planes)
        m = std::fmin(m, std::fabs(d));
    return m;
}

struct Lcg {
    unsigned s;
    float Next(float lo, float hi) {
        s = s * 1664525u + 1013904223u;
        return lo + (hi - lo) * ((s >> 8) / 16777216.0f);
    }
};

BSPNode *BuildTree(const std::vector<Tri> &tris, bool &ok) {
    std::list<BSPFace> faces;
    for (const Tri &f : tris) {
        BSPFace face;
        face.Set(f.a, f.b, f.c);
        faces.push_back(face);
    }
    BSPNode *tree = nullptr;
    ok = MakeBSPTree(tree, faces, 0);
    return tree;
}

// Segment entry: the tree's answer against the analytic entry parameter, and
// the reported plane must pass through the hit point along `axis`.
void SegGate(const char *name, const BSPNode *tree, const Vector3 &a, const Vector3 &b,
             bool wantHit, float wantT, int axis) {
    Segment seg;
    seg.start = a;
    seg.end = b;
    float t = -1;
    Plane pl;
    pl.Set(0, 0, 0, 0);
    bool hit = Intersect(seg, tree, t, pl);
    if (!wantHit) {
        Gate(name, !hit, "hit %d (want 0)", hit);
        return;
    }
    Vector3 p(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
    float n[3] = { pl.a, pl.b, pl.c };
    bool planeOk = true;
    if (axis >= 0) {
        planeOk = Close(std::fabs(n[axis]), 1, 1e-4)
            && Close(std::fabs(n[(axis + 1) % 3]) + std::fabs(n[(axis + 2) % 3]), 0, 1e-4)
            && Close(pl.Dot(p), 0, 1e-4);
    }
    Gate(name, hit && Close(t, wantT, 1e-4) && planeOk,
         "hit %d t %.5f (want %.5f), plane (%.3f %.3f %.3f %.3f)%s", hit, t, wantT, pl.a, pl.b,
         pl.c, pl.d, axis >= 0 ? " (want a unit axis plane through the hit)" : "");
}

void BSPGates() {
    // The source defaults (Geo.cpp), so CheckBSPTree really scales the box.
    SetBSPParams(0.01f, 0.985f, 20, 40, 1.1f);

    std::vector<Vector2> sq = { Vector2(-1, -1), Vector2(1, -1), Vector2(1, 1), Vector2(-1, 1) };
    std::vector<Tri> cube = Prism(sq, { { -1, -1, 1, 1 } }, -1, 1);
    std::vector<Vector2> lp = { Vector2(0, 0), Vector2(2, 0), Vector2(2, 1),
                                Vector2(1, 1), Vector2(1, 2), Vector2(0, 2) };
    std::vector<Tri> ell = Prism(lp, { { 0, 0, 2, 1 }, { 0, 1, 1, 2 } }, 0, 1);
    double vc = SignedVolume(cube), vl = SignedVolume(ell);
    Gate("geo-bsp-fixture", Close(vc, 8, 1e-5) && Close(vl, 3, 1e-5),
         "signed volume cube %.4f (want 8), L %.4f (want 3)", vc, vl);

    struct Mesh {
        const char *name;
        const std::vector<Tri> *tris;
        bool (*in)(const Vector3 &);
        double (*margin)(const Vector3 &);
        Box bound, inner;
    } meshes[2] = {
        { "cube", &cube, InCube, CubeMargin, Box(Vector3(-1, -1, -1), Vector3(1, 1, 1)),
          Box(Vector3(-0.5f, -0.5f, -0.5f), Vector3(0.5f, 0.5f, 0.5f)) },
        { "L", &ell, InL, LMargin, Box(Vector3(0, 0, 0), Vector3(2, 2, 1)),
          Box(Vector3(0.2f, 0.2f, 0.2f), Vector3(0.8f, 0.8f, 0.8f)) },
    };
    BSPNode *trees[2] = { nullptr, nullptr };
    for (int m = 0; m < 2; m++) {
        Mesh &mh = meshes[m];
        bool ok = false;
        BSPNode *tree = BuildTree(*mh.tris, ok);
        trees[m] = tree;
        int nodes = 0, depth = 0;
        if (tree)
            NumNodes(tree, nodes, depth);
        char nm[64];
        snprintf(nm, sizeof nm, "geo-bsp-build-%s", mh.name);
        Gate(nm, ok && tree != nullptr && nodes > 0, "MakeBSPTree %d over %d faces: %d nodes, depth %d",
             ok, (int)mh.tris->size(), nodes, depth);
        if (!tree)
            continue;

        // Classification: 4,000 seeded points in the bound grown by 0.5, those
        // within 1e-3 of a face skipped.
        Lcg r = { 12345u + (unsigned)m };
        int tested = 0, wrong = 0, inside = 0;
        for (int i = 0; i < 4000; i++) {
            Vector3 p(r.Next(mh.bound.mMin.x - 0.5f, mh.bound.mMax.x + 0.5f),
                      r.Next(mh.bound.mMin.y - 0.5f, mh.bound.mMax.y + 0.5f),
                      r.Next(mh.bound.mMin.z - 0.5f, mh.bound.mMax.z + 0.5f));
            if (mh.margin(p) < 1e-3)
                continue;
            tested++;
            bool want = mh.in(p);
            inside += want;
            if (Intersect(p, tree) != want)
                wrong++;
        }
        snprintf(nm, sizeof nm, "geo-bsp-classify-%s", mh.name);
        Gate(nm, wrong == 0 && inside > 100 && tested - inside > 100,
             "%d of %d seeded points misclassified (%d inside)", wrong, tested, inside);

        bool encl = CheckBSPTree(tree, mh.bound);
        bool tight = CheckBSPTree(tree, mh.inner);
        snprintf(nm, sizeof nm, "geo-bsp-check-%s", mh.name);
        Gate(nm, encl && !tight,
             "CheckBSPTree(bounding box) %d (want 1), (box inside the solid) %d (want 0)", encl,
             tight);
    }
    if (trees[0]) {
        SegGate("geo-bsp-seg-cube", trees[0], Vector3(-3, 0.2f, 0.3f), Vector3(3, 0.2f, 0.3f), true,
                1.0f / 3.0f, 0);
        SegGate("geo-bsp-seg-cube-miss", trees[0], Vector3(-3, 1.5f, 0), Vector3(3, 1.5f, 0), false,
                0, -1);
    }
    if (trees[1]) {
        // Down through the notch (outside), into the L's lower arm at y = 1.
        SegGate("geo-bsp-seg-L-notch", trees[1], Vector3(1.5f, 3, 0.5f), Vector3(1.5f, -1, 0.5f),
                true, 0.5f, 1);
        // Starts inside: the tree reports t = 0.
        SegGate("geo-bsp-seg-L-inside", trees[1], Vector3(0.5f, 0.5f, 0.5f), Vector3(5, 5, 5), true,
                0.0f, -1);
    }
    delete trees[0];
    delete trees[1];
}

// ----------------------------------------------------------------- Clip ----

double Area(const Hmx::Polygon &p) {
    double a = 0;
    size_t n = p.points.size();
    for (size_t i = 0; i < n; i++) {
        const Vector2 &u = p.points[i], &v = p.points[(i + 1) % n];
        a += (double)u.x * v.y - (double)v.x * u.y;
    }
    return std::fabs(a) * 0.5;
}

void ClipGates() {
    Hmx::Polygon sq, out;
    sq.points = { Vector2(0, 0), Vector2(1, 0), Vector2(1, 1), Vector2(0, 1) };
    Hmx::Ray ray;
    ray.base.Set(0.25f, 0);
    ray.dir.Set(1, 0); // keeps (p - base) . dir >= 0, i.e. x >= 0.25
    Clip(sq, ray, out);
    bool half = true;
    for (const Vector2 &v : out.points)
        half &= v.x >= 0.25f - 1e-6f;
    Gate("geo-clip-copy", out.points.size() == 4 && Close(Area(out), 0.75, 1e-6) && half,
         "%d points, area %.4f (want 4, 0.75), all x >= 0.25: %d", (int)out.points.size(),
         Area(out), half);

    Hmx::Polygon tri;
    tri.points = { Vector2(0, 0), Vector2(2, 0), Vector2(0, 2) };
    ray.base.Set(0, 0.5f);
    ray.dir.Set(0, -1); // keep y <= 0.5
    Clip(tri, ray, tri);
    bool below = true;
    for (const Vector2 &v : tri.points)
        below &= v.y <= 0.5f + 1e-6f;
    // area of the triangle below y = 0.5: 2 - 1.5*1.5/2 = 0.875
    Gate("geo-clip-inplace", tri.points.size() == 4 && Close(Area(tri), 0.875, 1e-6) && below,
         "%d points, area %.4f (want 4, 0.875), all y <= 0.5: %d", (int)tri.points.size(),
         Area(tri), below);

    ray.base.Set(2, 0);
    ray.dir.Set(1, 0);
    Clip(sq, ray, out);
    Hmx::Polygon empty, out2;
    out2.points = { Vector2(9, 9) };
    Clip(empty, ray, out2);
    Gate("geo-clip-empty", out.points.empty() && out2.points.empty(),
         "square wholly behind the ray: %d points; empty input: %d points (want 0, 0)",
         (int)out.points.size(), (int)out2.points.size());
}

// ------------------------------------------------- triangle / box / ray ----

Triangle Tri3(const Vector3 &a, const Vector3 &b, const Vector3 &c) {
    Triangle t;
    t.Set(a, b, c);
    return t;
}

void TriBoxGates() {
    Box box(Vector3(-1, -1, -1), Vector3(1, 1, 1));
    struct {
        Vector3 a, b, c;
        bool want;
        const char *what;
    } cases[] = {
        { Vector3(-0.5f, -0.5f, 0), Vector3(0.5f, -0.5f, 0), Vector3(0, 0.5f, 0), true, "inside" },
        { Vector3(4.5f, -0.5f, 0), Vector3(5.5f, -0.5f, 0), Vector3(5, 0.5f, 0), false, "far" },
        { Vector3(3.5f, 0, 0), Vector3(0, 3.5f, 0), Vector3(0, 0, 3.5f), false, "face plane" },
        { Vector3(2.5f, 0, 0), Vector3(0, 2.5f, 0), Vector3(3, 3, 0), false, "edge axis" },
        { Vector3(-3, -3, 0), Vector3(3, -3, 0), Vector3(0, 3, 0), true, "crossing" },
    };
    int bad = 0;
    char detail[256] = "";
    for (auto &k : cases) {
        bool got = Intersect(Tri3(k.a, k.b, k.c), box);
        if (got != k.want) {
            bad++;
            strncat(detail, k.what, sizeof(detail) - strlen(detail) - 2);
            strncat(detail, " ", sizeof(detail) - strlen(detail) - 1);
        }
    }
    Gate("geo-tri-box", bad == 0, "%d of 5 cases wrong %s", bad, detail);
}

void SegTriGates() {
    Triangle t = Tri3(Vector3(0, 0, 0), Vector3(1, 0, 0), Vector3(0, 1, 0)); // normal +z
    auto seg = [](const Vector3 &a, const Vector3 &b) {
        Segment s;
        s.start = a;
        s.end = b;
        return s;
    };
    float t1 = -1, t2 = -1, t3 = -1, tx;
    bool down = Intersect(seg(Vector3(0.25f, 0.25f, 1), Vector3(0.25f, 0.25f, -1)), t, true, t1);
    bool upCull = Intersect(seg(Vector3(0.25f, 0.25f, -1), Vector3(0.25f, 0.25f, 1)), t, true, tx);
    bool up = Intersect(seg(Vector3(0.25f, 0.25f, -1), Vector3(0.25f, 0.25f, 3)), t, false, t2);
    bool outside = Intersect(seg(Vector3(0.8f, 0.8f, 1), Vector3(0.8f, 0.8f, -1)), t, false, tx);
    bool shortSeg = Intersect(seg(Vector3(0.25f, 0.25f, 3), Vector3(0.25f, 0.25f, 1)), t, false, t3);
    bool parallel = Intersect(seg(Vector3(-1, 0.2f, 0.5f), Vector3(2, 0.2f, 0.5f)), t, false, tx);
    Gate("geo-seg-tri",
         down && Close(t1, 0.5, 1e-6) && !upCull && up && Close(t2, 0.25, 1e-6) && !outside
             && !shortSeg && Close(t3, 1.5, 1e-6) && !parallel,
         "front hit %d t %.4f (1, 0.5); back face culled %d (0); back face %d t %.4f (1, 0.25); "
         "outside %d (0); past the end %d t %.3f (0, 1.5); parallel %d (0)",
         down, t1, upCull, up, t2, outside, shortSeg, t3, parallel);
}

void RayBoxGates() {
    Box box(Vector3(-1, -1, -1), Vector3(1, 1, 1));
    float n0, f0, n1, f1, n2, f2, n3, f3;
    bool through = Intersect(Vector3(-5, 0.5f, 0.25f), Vector3(2, 0, 0), box, n0, f0);
    bool miss = Intersect(Vector3(-5, 1.5f, 0), Vector3(1, 0, 0), box, n1, f1);
    bool behind = Intersect(Vector3(5, 0, 0), Vector3(1, 0, 0), box, n2, f2);
    bool inside = Intersect(Vector3(0, 0, 0.5f), Vector3(0, 0, 1), box, n3, f3);
    Gate("geo-ray-box",
         through && Close(n0, 2, 1e-6) && Close(f0, 3, 1e-6) && !miss && !behind && inside
             && Close(n3, 1.1920929e-07, 1e-9) && Close(f3, 0.5, 1e-6),
         "through %d [%.3f, %.3f] (1, [2, 3]); miss %d (0); behind %d (0); from inside %d "
         "[%.3g, %.3f] (1, [FLT_EPSILON, 0.5])",
         through, n0, f0, miss, behind, inside, n3, f3);
}

// --------------------------------------------------------------- sphere ----

bool SphereIs(const Sphere &s, float x, float y, float z, float r) {
    return Close(s.center.x, x, 1e-5) && Close(s.center.y, y, 1e-5) && Close(s.center.z, z, 1e-5)
        && Close(s.radius, r, 1e-5);
}

void SphereGates() {
    Sphere a(Vector3(0, 0, 0), 1);
    a.GrowToContain(Sphere(Vector3(4, 0, 0), 1));
    Sphere b(Vector3(0, 0, 0), 5);
    b.GrowToContain(Sphere(Vector3(1, 0, 0), 1));
    Sphere c(Vector3(0, 0, 0), 1);
    c.GrowToContain(Sphere(Vector3(1, 0, 0), 5));
    Sphere d(Vector3(7, 7, 7), 0);
    d.GrowToContain(Sphere(Vector3(1, 2, 3), 2));
    Sphere e(Vector3(1, 2, 3), 2);
    e.GrowToContain(Sphere(Vector3(9, 9, 9), 0));
    // dist 5 along (0.6, 0.8, 0): far points (-0.2, 0.4, 3) and (4.6, 6.8, 3)
    Sphere f(Vector3(1, 2, 3), 2);
    f.GrowToContain(Sphere(Vector3(4, 6, 3), 1));
    Gate("geo-sphere-grow",
         SphereIs(a, 2, 0, 0, 3) && SphereIs(b, 0, 0, 0, 5) && SphereIs(c, 1, 0, 0, 5)
             && SphereIs(d, 1, 2, 3, 2) && SphereIs(e, 1, 2, 3, 2) && SphereIs(f, 2.2f, 3.6f, 3, 4),
         "disjoint (%.2f %.2f %.2f r%.2f) want (2 0 0 r3); contains (%.2f r%.2f) want (0 r5); "
         "contained (%.2f r%.2f) want (1 r5); general (%.3f %.3f %.3f r%.3f) want (2.2 3.6 3 r4)",
         a.center.x, a.center.y, a.center.z, a.radius, b.center.x, b.radius, c.center.x, c.radius,
         f.center.x, f.center.y, f.center.z, f.radius);
}

// -------------------------------------------------------------- frustum ----

void FrustumGates() {
    // Perspective: view along +y, near 1, far 100, fovY 90 degrees, ratio 0.75.
    // Inside means near <= y <= far, |z| <= y tan(45), and the left/right planes
    // through the origin with normal ~ (1, k), k = tan(45) / ratio.
    Frustum fr;
    fr.Set(1, 100, (float)M_PI / 2, 0.75f);
    const double k = 1.0 / 0.75;
    struct {
        Vector3 c;
        float r;
    } pts[] = {
        { Vector3(0, 10, 0), 0.5f },      { Vector3(0, 0.2f, 0), 0.5f },
        { Vector3(0, 0.6f, 0), 0.5f },    { Vector3(0, 101, 0), 0.5f },
        { Vector3(0, 100.4f, 0), 0.5f },  { Vector3(0, 10, 11), 0.5f },
        { Vector3(0, 10, 10.5f), 0.5f },  { Vector3(0, 10, -11), 0.5f },
        { Vector3(-14.5f, 10, 0), 0.5f }, { Vector3(-13.5f, 10, 0), 0.5f },
        { Vector3(14.5f, 10, 0), 0.5f },  { Vector3(13.5f, 10, 0), 0.5f },
    };
    int bad = 0, culled = 0;
    for (auto &p : pts) {
        double x = p.c.x, y = p.c.y, z = p.c.z, r = p.r, s = std::sqrt(0.5);
        bool out = (y - 1) < -r || (100 - y) < -r || (s * y - s * z) < -r || (s * y + s * z) < -r
            || (x + k * y) / std::sqrt(1 + k * k) < -r || (-x + k * y) / std::sqrt(1 + k * k) < -r;
        Sphere sp(p.c, p.r);
        bool got = sp > fr;
        culled += out;
        if (got != out)
            bad++;
    }
    Gate("geo-frustum-persp", bad == 0 && culled == 6, "%d of 12 spheres wrong (%d of 12 outside)",
         bad, culled);

    // fovY 0 is orthographic: |x| <= 1, |z| <= ratio.
    Frustum ortho;
    ortho.Set(1, 100, 0, 5);
    bool in = Sphere(Vector3(0.9f, 10, 4.9f), 0) > ortho;
    bool outZ = Sphere(Vector3(0, 10, 5.2f), 0.1f) > ortho;
    bool outX = Sphere(Vector3(-1.2f, 10, 0), 0.1f) > ortho;
    Gate("geo-frustum-ortho", !in && outZ && outX,
         "(0.9,10,4.9) culled %d (0); z 5.2 culled %d (1); x -1.2 culled %d (1)", in, outZ, outX);
}

void PlaneXformGates() {
    // Rotate +90 about x (y -> z, z -> -y), scale x by 2, translate (1, 2, 3).
    Transform t;
    t.m.Set(2, 0, 0, 0, 0, 1, 0, -1, 0);
    t.v.Set(1, 2, 3);
    Plane p;
    p.Set(0, 0, 1, -2); // z = 2, +z side positive
    Plane q;
    Multiply(p, t, q);
    Vector3 on[3] = { Vector3(0, 0, 2), Vector3(1, 0, 2), Vector3(0, 1, 2) };
    double worst = 0;
    for (const Vector3 &v : on) {
        Vector3 w;
        Multiply(v, t, w);
        worst = std::fmax(worst, std::fabs(q.Dot(w)));
    }
    Vector3 above, below;
    Multiply(Vector3(0.3f, 0.7f, 5), t, above);
    Multiply(Vector3(0.3f, 0.7f, -1), t, below);
    float sa = q.Dot(above), sb = q.Dot(below);
    Gate("geo-plane-xform", worst < 1e-5 && sa > 0 && sb < 0,
         "plane (%.3f %.3f %.3f %.3f): worst on-plane residual %.2g (want 0); sides %.3f > 0, %.3f < 0",
         q.a, q.b, q.c, q.d, worst, sa, sb);
}

// ------------------------------------------------------------------ Rot ----

void RotGates() {
    const double h = std::sqrt(0.5);
    struct {
        Vector3 a, b;
        double x, y, z, w;
    } cases[] = {
        { Vector3(1, 0, 0), Vector3(0, 2, 0), 0, 0, h, h },        // 90 about +z
        { Vector3(1, 1, 0), Vector3(0, 0, 3), 0.5, -0.5, 0, h },   // 90 about (1,-1,0)/sqrt2
        { Vector3(0, 3, 0), Vector3(0, 1, 0), 0, 0, 0, 1 },        // parallel: identity
        { Vector3(1, 0, 0), Vector3(-2, 0, 0), 0, 0, 1, 0 },       // opposite: the fallback
    };
    int bad = 0;
    char detail[200] = "";
    for (int i = 0; i < 4; i++) {
        Hmx::Quat q;
        MakeRotQuat(cases[i].a, cases[i].b, q);
        if (!(Close(q.x, cases[i].x, 1e-5) && Close(q.y, cases[i].y, 1e-5)
              && Close(q.z, cases[i].z, 1e-5) && Close(q.w, cases[i].w, 1e-5))) {
            bad++;
            snprintf(detail + strlen(detail), sizeof(detail) - strlen(detail),
                     " #%d=(%.3f %.3f %.3f %.3f)", i, q.x, q.y, q.z, q.w);
        }
    }
    Gate("rot-make-rot-quat", bad == 0, "%d of 4 quaternions wrong%s", bad, detail);
}

// --------------------------------------------------------------- Interp ----

void InterpGates() {
    DataArray *a = DataReadString("(lin 10.0 20.0 0.0 4.0) (flat 5.0 9.0 3.0 3.0)");
    LinearInterpolator lin, flat;
    lin.Reset(a->Array(0));
    flat.Reset(a->Array(1));
    Gate("interp-linear-dta",
         lin.Eval(0) == 10 && lin.Eval(4) == 20 && Close(lin.Eval(1), 12.5, 1e-6)
             && flat.Eval(3) == 5 && flat.Eval(100) == 5,
         "(10 20 0 4): %.3f %.3f %.3f (want 10 20 12.5); zero run: %.3f %.3f (want 5 5)",
         lin.Eval(0), lin.Eval(4), lin.Eval(1), flat.Eval(3), flat.Eval(100));
    a->Release();

    // y0 + (y1 - y0) * (atan(s(2u - 1)) + atan(s)) / (2 atan(s))
    ATanInterpolator at(0, 10, 0, 2, 4);
    auto ref = [](double x) {
        double u = x / 2, s = 4;
        return 10 * (std::atan(s * (2 * u - 1)) + std::atan(s)) / (2 * std::atan(s));
    };
    double worst = 0;
    for (double x : { 0.0, 0.5, 1.0, 1.3, 2.0 })
        worst = std::fmax(worst, std::fabs(at.Eval((float)x) - ref(x)));
    Gate("interp-atan", worst < 1e-4, "worst |Eval - closed form| over 5 points %.2g (want < 1e-4)",
         worst);

    // y0 + (y1 - y0) * (1 - (1 - u)^p)
    double worstE = 0;
    for (double p : { 2.0, 3.0 }) {
        InvExpInterpolator ie(2, 6, 1, 3, (float)p);
        for (double u : { 0.0, 0.3, 0.75, 1.0 }) {
            double want = 2 + 4 * (1 - std::pow(1 - u, p));
            worstE = std::fmax(worstE, std::fabs(ie.Eval((float)(1 + 2 * u)) - want));
        }
    }
    Gate("interp-invexp", worstE < 1e-5, "worst |Eval - closed form| over 8 points %.2g", worstE);
}

void KeyGates() {
    // Hermite: H(t) = h00 p0 + h10 m0 + h01 p1 + h11 m1.
    Vector3 p0(1, 2, 3), m0(0.5f, -1, 2), p1(4, 0, -1), m1(-2, 1, 0.5f);
    auto H = [&](double t, int i) {
        double t2 = t * t, t3 = t2 * t;
        double v[4] = { p0[i], m0[i], p1[i], m1[i] };
        return (2 * t3 - 3 * t2 + 1) * v[0] + (t3 - 2 * t2 + t) * v[1] + (-2 * t3 + 3 * t2) * v[2]
            + (t3 - t2) * v[3];
    };
    double worst = 0;
    for (double t : { 0.0, 0.3, 0.5, 0.9, 1.0 }) {
        Vector3 d;
        InterpTangent(p0, m0, p1, m1, (float)t, d);
        for (int i = 0; i < 3; i++) {
            double e = 1e-4, num = (H(t + e, i) - H(t - e, i)) / (2 * e);
            worst = std::fmax(worst, std::fabs(d[i] - num));
        }
    }
    Gate("key-interp-tangent", worst < 1e-4,
         "worst |InterpTangent - dH/dt| over 5 t x 3 axes %.2g (want < 1e-4)", worst);
}

// ----------------------------------------------------------------- text ----

void TextGates() {
    char out[32];
    // h, e-acute (U+00E9), l l o, space, euro sign (U+20AC), !
    const char *in = "h\xc3\xa9llo \xe2\x82\xac!";
    UTF8toASCIIs(out, sizeof out, in, '?');
    bool full = strcmp(out, "h\xe9llo ?!") == 0;
    char shortOut[4];
    UTF8toASCIIs(shortOut, sizeof shortOut, in, '?');
    bool trunc = strcmp(shortOut, "h\xe9l") == 0;
    Gate("utf8-to-ascii", full && trunc, "full %s, len 4 %s", full ? "ok" : "WRONG",
         trunc ? "ok" : "WRONG");

    DataArray *da = DataReadString("(who \"Bob\") (n 7) (f 1.5)");
    struct {
        const char *fmt, *want;
    } cases[] = {
        { "Hi {string:who}, {int:3:n} pts, {float:.2:f}, {{x}", "Hi Bob,   7 pts, 1.50, {x}" },
        { "[{int::nope}]", "[{missing:nope}]" },
        { "[{int::who}]", "[{missing:who}]" },
        { "a {string:zz", "a {badfmt:zz" },
    };
    const char *names[] = { "sfs-fill", "sfs-missing", "sfs-wrong-type", "sfs-unterminated" };
    for (int i = 0; i < 4; i++) {
        SuperFormatString s(cases[i].fmt, da, false);
        const char *got = s.RawFmt();
        Gate(names[i], strcmp(got, cases[i].want) == 0, "'%s' -> '%s' (want '%s')", cases[i].fmt,
             got, cases[i].want);
    }
    da->Release();
}

} // namespace

int RunW16TFPhase(void (*gate)(const char *, bool, const char *)) {
    gGate = gate;
    printf("=== W16-TF phase: engine math/utl rows no native target entered ===\n");
    BSPGates();
    ClipGates();
    TriBoxGates();
    SegTriGates();
    RayBoxGates();
    SphereGates();
    FrustumGates();
    PlaneXformGates();
    RotGates();
    InterpGates();
    KeyGates();
    TextGates();
    return 0;
}
