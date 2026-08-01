// rb3-xenon native — X3: draw a real .milo_xbox through milo-native-engine's
// dc3 WebGPU backend, headless, from xenon's own target, and write a PNG.
//
// Usage:
//   ./build/rb3-render <dataDir> <outDir> [<arkPath> ...] [flags]
//
//   dataDir   directory containing gen/main_xbox.hdr + gen/main_xbox_*.ark
//   outDir    directory the PNGs are written into (created if absent)
//   arkPath   archive-relative .milo_xbox path; with none given, the two X3
//             cells run in order: tracksystem_meshes then crowd_female01
//
//   --width N --height N   framebuffer size (default 1280x720)
//   --frames N             frames drawn before readback (default 4)
//   --dump-rnd             print every Rnd/NgRnd member the dc3 backend reads
//   --verbose              per-mesh listing
//
// ---------------------------------------------------------------------------
// WHAT THIS MILESTONE IS ACTUALLY TESTING
// ---------------------------------------------------------------------------
// X1 proved the engine's GPU device works on this machine by CLEARING a frame
// and verifying all 57,600 pixels; it deliberately touched zero xenon rndobj
// members. X2 proved the xenon OBJECT GRAPH loads, with no renderer at all.
// Neither could be wrong for a reason involving the other, which is the whole
// point: when the first WgpuRnd frame comes out wrong, the device, the surface
// format, the readback stride and the PNG encoder are all ALREADY PROVEN, so
// the search space is the Rnd coupling alone.
//
// The coupling risk is NOT ABI. libmilo-engine.a is compiled with
// MILO_ENGINE_DECOMP_INCLUDE_DIRS pointing at xenon's own src/system, so both
// sides see one rndobj/Rnd.h and member offsets agree BY CONSTRUCTION. The live
// risk is SEMANTIC -- the dc3 backend reading a xenon member that exists at the
// right offset and means something else. `--dump-rnd` exists for exactly that:
// it prints the eight Rnd/NgRnd members the backend is measured to read
// (mWidth, mHeight, mClearColor, mDrawing, mWorldEnded, mDrawCount,
// mDefaultCam, mDefaultEnv), so a wrong one is READ OFF rather than guessed at.
//
// ---------------------------------------------------------------------------
// WHY "IT WROTE A PNG" IS NOT THE PASS CRITERION
// ---------------------------------------------------------------------------
// A renderer that draws nothing still produces a perfectly valid PNG of the
// clear colour, and that is the single most likely failure mode here. So each
// cell is gated on the IMAGE CONTENT, by two independent statistics that fail
// for different reasons:
//
//   coverage   the fraction of pixels that differ from the MODAL colour (which
//              on an empty frame is the clear colour and ~100% of the image).
//              Catches "nothing drew" and "one stray pixel drew".
//   distinct   the number of distinct RGB values. Catches the case where
//              something drew but with a single flat colour -- e.g. a mesh
//              rendered with no material binding at all, which would pass a
//              coverage-only check while being just as broken.
//
// Both are printed for every cell whether it passes or not, so a marginal
// result is legible instead of being reduced to PASS/FAIL.
//
// ---------------------------------------------------------------------------
// STANDING WgpuRnd UP -- THE ORDER IS NOT NEGOTIABLE
// ---------------------------------------------------------------------------
// Linking the engine pulls src/platform/Rnd_Wgpu.cpp, whose TheRnd/TheNgRnd are
// references to a FILE-SCOPE `static WgpuRnd gWgpuRndInstance` (Rnd_Wgpu.cpp:64)
// -- so a xenon-shaped WgpuRnd is CONSTRUCTED BEFORE main() with no chance to
// instrument it. That is not a hazard we can move, so it is one we have to
// audit, and the audit came out clean:
//
//   Hmx::Object::Object()  under HX_NATIVE is a member-init list of null/empty
//                          pointers plus mRefs.DetachSelf() -- an intrusive
//                          list self-link. obj/Object.cpp:156-165.
//   Rnd::Rnd()             is a member-init list ONLY; its body is a loop
//                          zeroing mDefaultTex[8]. No allocation, no Symbol
//                          interning, no SystemConfig, no MemMgr.
//                          rndobj/Rnd.cpp:145-168.
//   NgRnd::NgRnd()         member-init list only. rndobj/Rnd_NG.cpp:28-31.
//
// The only global any of them reads is `gNullStr`, which is
// `const char *gNullStr = ""` (os/System.cpp:50) -- CONSTANT-initialised, so it
// is live before any dynamic initialiser runs, in any TU order. The static ctor
// is therefore safe, and this driver's job is only to control everything AFTER
// it: nothing here touches TheRnd until the archive, the DTA layer, the system
// config and the object factories are all up (StandUpRenderer(), below).
//
// The one thing we do NOT get to control: WgpuRnd::Init() calls PreInit()
// itself (Rnd_Wgpu.cpp:216), so `TheRnd.Init()` is one call, not two, and
// Rnd::PreInit's config reads happen inside it. Hence the config must be real
// BEFORE Init -- see StandUpConfig().
//
// Exit codes: 0 = every requested cell rendered and passed its image gates.
//             1 = a gate failed.  2 = no usable GPU (reported, not faked).

#include "char/CharBoneDir.h"
#include "char/Character.h"
#include "math/Color.h"
#include "math/Vec.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/Dir.h"
#include "obj/DirLoader.h"
#include "obj/Object.h"
#include "os/Archive.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/System.h"
#include "rndobj/Cam.h"
#include "rndobj/Dir.h"
#include "rndobj/Draw.h"
#include "rndobj/Env.h"
#include "rndobj/Lit.h"
#include "rndobj/Mat.h"
#include "rndobj/Mesh.h"
#include "rndobj/Rnd.h"
#include "rndobj/Rnd_NG.h"
#include "rndobj/Tex.h"
#include "rndobj/Trans.h"
#include "utl/FilePath.h"
#include "utl/Str.h"
#include "utl/Symbol.h"

#include "gfx/Screenshot.h"
#include "platform/Rnd_Wgpu.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

extern void InitMakeString();
// native/src/platform/File_Native.cpp
extern void NativeSetDataDir(const char *dir);
// native/src/platform/System_Native.cpp
extern void NativeArchiveInit();
// native/src/milo_object_factories.cpp
extern void RegisterMiloObjectFactories();
// os/System.cpp:58
extern DataArray *gSystemConfig;

// milo-native-engine src/platform/Mesh_Wgpu.cpp:159 — the draw body WITHOUT the
// viewer-heuristic prefilter that RndMesh::DrawShowing applies first. Declared
// non-static there and already extern'd by the engine's own
// TransparentQueue.cpp:17, so this is a supported entry point, not a private
// symbol being reached around. See the LOD note in the draw loop.
extern void DrawMeshImmediate(RndMesh *mesh);

namespace {

    int gFailures = 0;
    bool gVerbose = false;
    bool gDumpCam = false;
    bool gManualCam = false;

    void Gate(const char *name, bool ok, const char *detail) {
        printf("  [%s] %s%s%s\n", ok ? "PASS" : "FAIL", name,
               detail && *detail ? " — " : "", detail ? detail : "");
        if (!ok) gFailures++;
    }

    // -----------------------------------------------------------------------
    // Config.
    //
    // X2 assembled a minimal `(objects ...)` by hand and deliberately did NOT
    // call PreInitSystem, because PreInitSystem starts standing the RENDERER up
    // and X2 must not. X3 wants the renderer, so that reason is spent -- but
    // the SHAPE of the config now matters much more, because Rnd::PreInit reads
    // it and one read is not tolerant of a stand-in:
    //
    //   Rnd::SetupFont (rndobj/Rnd.cpp:892) does mFont->Array(i + 66) for
    //   i in [0,26), i.e. it INDEXES ELEMENTS 66..123 of SystemConfig("rnd",
    //   "font") and writes 98..123. A synthesised or empty section is not a
    //   degraded font, it is an out-of-range Array() on a 0-element DataArray.
    //
    // So this reads the shipped preinit config whole, exactly as
    // PreInitSystem/ReadSystemConfig would (os/System.cpp:223-230 is literally
    // `DataReadFile(config, true)`), and skips only PreInitSystem's DataSetMacro
    // / OptionStr / DataRegisterFunc / SetGfxMode wrapper. Measured contents:
    // 25 top-level sections including `rnd`, `objects`, `system`, `ui`, `mem`.
    //
    // ⚠ config/band_keep.dta -- the SystemInit half -- is NOT read, and cannot
    // be: it pulls ui/dev_only/selvenue.dta, which is not in the shipped
    // archive (measured: "DataReadFile: Can't open ui/dev_only/selvenue.dta"
    // then a hard failure inside ui/init.dta). That is a property of the retail
    // disc, not of this harness. Everything the render path needs is in the
    // preinit half.
    bool StandUpConfig() {
        const char *cfg = getenv("RB3_SYSCFG");
        if (!cfg) cfg = "config/band_preinit_keep.dta";

        gSystemConfig = DataReadFile(cfg, true);
        if (!gSystemConfig) {
            Gate("system-config", false, cfg);
            return false;
        }
        DataVariable("syscfg") = gSystemConfig;

        DataArray *objects = gSystemConfig->FindArray(Symbol("objects"), false);

        // Merge the per-class type-def blocks the SystemInit half would have
        // merged. The shipped boot is PreInitSystem(band_preinit_keep) then
        // InitSystem(band_keep) + DataMergeTags/DataReplaceTags
        // (os/System.cpp:458-464); band_keep is unreadable here (it pulls
        // ui/dev_only/selvenue.dta, absent from the retail archive), so its
        // contribution is reconstructed from the two shipped class tables
        // directly. Same source data, one merge step instead of two.
        //
        // This is not a nicety. The preinit `objects` section HAS a `Mat` block
        // but it does NOT carry `metamaterial_path`, and RndMat::Init derefs the
        // result of loading that path without a null check.
        //
        // REPLACE, not append: DataArray::FindArray returns the FIRST match, so
        // appending a second `Mat` block would be inert.
        if (objects) {
            const char *extra = getenv("RB3_SYSCFG_OBJECTS");
            if (!extra) extra = "config/rnd_objects.dta,config/objects.dta";
            int replaced = 0, added = 0;
            std::string all(extra);
            size_t p0 = 0;
            std::vector<DataArray *> appendNodes;
            while (p0 <= all.size()) {
                size_t c = all.find(',', p0);
                if (c == std::string::npos) c = all.size();
                std::string one = all.substr(p0, c - p0);
                p0 = c + 1;
                if (one.empty()) continue;
                DataArray *body = DataReadFile(one.c_str(), true);
                if (!body) continue;
                for (int i = 0; i < body->Size(); i++) {
                    if (body->Node(i).Type() != kDataArray) continue;
                    DataArray *blk = body->Node(i).Array();
                    if (!blk || blk->Size() < 1) continue;
                    Symbol tag = blk->Sym(0);
                    bool hit = false;
                    for (int j = 1; j < objects->Size(); j++) {
                        if (objects->Node(j).Type() != kDataArray) continue;
                        DataArray *cur = objects->Node(j).Array();
                        if (cur && cur->Size() > 0 && cur->Sym(0) == tag) {
                            objects->Node(j) = body->Node(i);
                            replaced++;
                            hit = true;
                            break;
                        }
                    }
                    if (!hit) {
                        appendNodes.push_back(blk);
                        added++;
                    }
                }
            }
            if (!appendNodes.empty()) {
                int base = objects->Size();
                objects->Resize(base + (int)appendNodes.size());
                for (size_t i = 0; i < appendNodes.size(); i++)
                    objects->Node(base + (int)i) = DataNode(appendNodes[i], kDataArray);
            }
            printf("  objects merge: %s — %d class block(s) replaced, %d added\n", extra,
                   replaced, added);
        }

        DataArray *rnd = gSystemConfig->FindArray(Symbol("rnd"), false);
        DataArray *font = rnd ? rnd->FindArray(Symbol("font"), false) : nullptr;
        char d[256];
        snprintf(d, sizeof(d),
                 "%s — %d sections, objects=%d blocks, rnd=%s, rnd/font=%d entries", cfg,
                 gSystemConfig->Size(), objects ? objects->Size() - 1 : -1,
                 rnd ? "yes" : "MISSING", font ? font->Size() : -1);
        // 124 is the hard floor: SetupFont writes mFont->Node(i + 98) for
        // i in [0,26), so element 123 must exist.
        bool ok = objects && rnd && font && font->Size() >= 124;
        Gate("system-config", ok, d);
        if (!ok) return false;

        // NOT a gate — an observation, reported because it is the input to a
        // real finding. RndMat::Init derefs LoadMetaMaterials()'s result
        // unchecked, and that result is NULL exactly when this key is absent.
        // It IS absent on RB3 (measured across every shipped config DTA), and
        // RB3 ships no metamaterials.milo either, while DC3 ships both — see
        // the HX_NATIVE note now in rndobj/Mat.cpp:285. Printing it here means
        // a future regression in that area reads as "the key came back" rather
        // than as an unexplained crash.
        {
            DataArray *matCfg = objects ? objects->FindArray(Symbol("Mat"), false) : nullptr;
            const char *mmPath = "";
            bool found = matCfg && matCfg->FindData("metamaterial_path", mmPath, false);
            printf("  note: objects/Mat/metamaterial_path = '%s' (absent is CORRECT on "
                   "RB3; DC3-only content)\n",
                   found ? mmPath : "(absent)");
        }
        return true;
    }

    // -----------------------------------------------------------------------
    // Renderer stand-up. See the header note on static-init safety.
    // -----------------------------------------------------------------------
    bool StandUpRenderer(int width, int height) {
        // GpuDevice reads these in WgpuRnd::Init (Rnd_Wgpu.cpp:247-256). Set
        // here rather than requiring the caller to export them, so a run is
        // reproducible from the command line alone.
        setenv("MILO_RENDER", "1", 1);
        setenv("MILO_HEADLESS", "1", 1);
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", width);
        setenv("MILO_WIDTH", buf, 1);
        snprintf(buf, sizeof(buf), "%d", height);
        setenv("MILO_HEIGHT", buf, 1);

        // ⚠ The ONE call. WgpuRnd::Init overrides Rnd::Init and does NOT chain
        // to it; it calls PreInit() (-> NgRnd::PreInit -> Rnd::PreInit, which
        // registers every rndobj factory, reads the `rnd` config section, and
        // builds the default cam/env/mat) and then TheShaderMgr.Init() and the
        // GPU device. So there is no PreInit/Init split to sequence here, and
        // adding a separate TheRnd.PreInit() first would only make NgRnd's
        // mInited guard swallow the second one.
        TheRnd.Init();

        if (!gWgpuRnd || !gWgpuRnd->Gpu().IsReady()) {
            Gate("gpu-ready", false,
                 "GPU init failed — every render below would be a blank frame "
                 "reported as a success, so the run stops here");
            return false;
        }
        // Refuse the null backend for the same reason rb3-frame does: a CPU/null
        // adapter reports success and produces nothing, which is the single
        // best way to manufacture a fictional pass.
        if (gWgpuRnd->Gpu().IsNullBackend()) {
            Gate("gpu-real-adapter", false,
                 "fell back to the Null backend — no real GPU (sandboxed Vulkan ICD?)");
            return false;
        }
        char d[160];
        snprintf(d, sizeof(d), "%dx%d headless, %s", gWgpuRnd->Gpu().WindowWidth(),
                 gWgpuRnd->Gpu().WindowHeight(),
                 gWgpuRnd->Gpu().HasBCCompression() ? "BC textures" : "software DXT");
        Gate("gpu-real-adapter", true, d);
        return true;
    }

    // -----------------------------------------------------------------------
    // The semantic-coupling instrument.
    //
    // These eight are not a guess at what "might" matter: they are the exact
    // Rnd/NgRnd-owned members that appear in milo-native-engine's
    // src/platform/Rnd_Wgpu.cpp, measured by name (everything else it names is
    // its own WgpuRnd member, an RndCam member, or an RndShaderMgr member).
    // Printing them makes a semantic mismatch READABLE instead of inferred from
    // a wrong-looking picture.
    // -----------------------------------------------------------------------
    void DumpRndMembers(const char *when) {
        printf("--- Rnd/NgRnd members the dc3 backend reads (%s) ---\n", when);
        printf("  mWidth      = %d      (Rnd virtual res; backend compares vs GPU surface)\n",
               TheRnd.Width());
        printf("  mHeight     = %d\n", TheRnd.Height());
        const Hmx::Color &c = TheRnd.GetClearColor();
        printf("  mClearColor = %.3f %.3f %.3f %.3f\n", c.red, c.green, c.blue, c.alpha);
        printf("  mDrawing    = %d      (BeginDrawing/EndDrawing latch)\n",
               (int)TheRnd.Drawing());
        printf("  mDefaultCam = %p\n", (void *)TheRnd.GetDefaultCam());
        printf("  mDefaultEnv = %p\n", (void *)TheRnd.DefaultEnv());
        printf("  RndCam::Current()     = %p\n", (void *)RndCam::Current());
        printf("  RndEnviron::Current() = %p\n", (void *)RndEnviron::Current());
    }

    // -----------------------------------------------------------------------
    // Scene framing.
    //
    // Ported from dc3-decomp/native/src/viewer/ViewerScene.cpp
    // (AutoFrameCamera). The compressed-vertex branch matters: RB3-360 meshes
    // ship as 36-byte big-endian XboxCVert blobs with NO decompressed Vert
    // array, so a bbox built only from NumVerts() would be empty for exactly
    // the assets this milestone renders, and the camera would frame nothing.
    // -----------------------------------------------------------------------
    struct Bounds {
        // rawMin/rawMax are the naive extremes (kept only to REPORT the
        // outliers); minv/maxv are the robust percentile bounds the camera
        // actually uses. See the note above Percentile().
        float rawMin[3];
        float rawMax[3];
        float minv[3];
        float maxv[3];
        std::vector<float> axis[3];
        int meshes = 0;
        bool valid = false;
        Bounds() {
            for (int i = 0; i < 3; i++) {
                rawMin[i] = 1e30f;
                rawMax[i] = -1e30f;
                minv[i] = 0.0f;
                maxv[i] = 0.0f;
            }
        }
        void Add(float x, float y, float z) {
            const float v[3] = { x, y, z };
            for (int i = 0; i < 3; i++) {
                if (v[i] != v[i]) return; // NaN: never let it into the sample
                if (v[i] < rawMin[i]) rawMin[i] = v[i];
                if (v[i] > rawMax[i]) rawMax[i] = v[i];
            }
            for (int i = 0; i < 3; i++) axis[i].push_back(v[i]);
            valid = true;
        }
        void Finish();
        // Only true when trimming changed the picture MATERIALLY. A percentile
        // bound always shaves a little off a healthy mesh (the crowd cell loses
        // ~1 unit of 72), and reporting that as an anomaly would train the
        // reader to ignore the warning. 2x is the threshold: the tracksystem
        // outlier inflates the raw Y span by ~300x.
        bool Trimmed() const {
            for (int i = 0; i < 3; i++) {
                float robust = maxv[i] - minv[i];
                float span = rawMax[i] - rawMin[i];
                if (robust > 0.0f && span > robust * 2.0f) return true;
            }
            return false;
        }
    };

    void AddWorldPoint(Bounds &b, const Transform &xfm, float x, float y, float z) {
        b.Add(xfm.m.x.x * x + xfm.m.y.x * y + xfm.m.z.x * z + xfm.v.x,
              xfm.m.x.y * x + xfm.m.y.y * y + xfm.m.z.y * z + xfm.v.y,
              xfm.m.x.z * x + xfm.m.y.z * y + xfm.m.z.z * z + xfm.v.z);
    }

    // -----------------------------------------------------------------------
    // ROBUST bounds.
    //
    // ⚠ This is not defensive programming for its own sake; it is a fix for a
    // KNOWN, INDEPENDENTLY-OBSERVED artifact. DC3's milo-viewer rendered
    // tracksystem_meshes.milo_xbox in May 2026 and reported (rb3-xenon
    // docs/plans/engine-reuse-and-asset-rendering.md): "Frame blank: one mesh
    // parsed a garbage Y (121458) -> degenerate auto-frame bbox -> orbit camera
    // parked 243180 units out."
    //
    // This driver's own naive min/max reproduced **121458.38** — the same
    // number, to the decimal, from a completely separate decoder. That
    // agreement is worth more than either measurement alone: it says the
    // outlier is in the ASSET (or in a vertex-format branch both engines get
    // wrong identically), not in this harness's compressed-vertex reader.
    //
    // So the fix is at the framing layer, not the decode layer: bounds are the
    // 0.5th..99.5th percentile of world-space vertex coordinates per axis. A
    // handful of nonsense vertices cannot move the camera, and the raw extremes
    // are still PRINTED so the artifact stays visible instead of being quietly
    // clipped away. Whoever eventually fixes the decode gets a before/after
    // number rather than a rumour.
    float Percentile(std::vector<float> &v, double p) {
        if (v.empty()) return 0.0f;
        size_t k = (size_t)(p * (double)(v.size() - 1));
        std::nth_element(v.begin(), v.begin() + k, v.end());
        return v[k];
    }

    void Bounds::Finish() {
        for (int i = 0; i < 3; i++) {
            if (axis[i].empty()) {
                minv[i] = maxv[i] = 0.0f;
                continue;
            }
            minv[i] = Percentile(axis[i], 0.005);
            maxv[i] = Percentile(axis[i], 0.995);
            if (maxv[i] < minv[i]) {
                float t = minv[i];
                minv[i] = maxv[i];
                maxv[i] = t;
            }
        }
    }

    Bounds SceneBounds(ObjectDir *dir) {
        Bounds b;
        for (ObjDirItr<RndMesh> it(dir, true); it; ++it) {
            RndMesh *m = it;
            if (!m || !m->Showing()) continue;
            const Transform &xfm = m->WorldXfm();
            RndMesh *owner = m->GetGeomOwner();
            if (!owner) owner = m;

            int nv = owner->NumVerts();
            int ncv = owner->NumCompressedVerts();
            if (nv > 0) {
                for (int i = 0; i < nv; i++) {
                    const RndMesh::Vert &v = owner->Verts(i);
                    AddWorldPoint(b, xfm, v.pos.x, v.pos.y, v.pos.z);
                }
            } else if (ncv > 0 && owner->CompressedVerts()) {
                // 36-byte record, position = 3 big-endian floats at offset 0.
                const unsigned char *data = owner->CompressedVerts();
                const int stride = 36;
                for (int i = 0; i < ncv; i++) {
                    const unsigned char *p = data + (size_t)i * stride;
                    float f[3];
                    for (int k = 0; k < 3; k++) {
                        unsigned int be;
                        memcpy(&be, p + k * 4, 4);
                        be = __builtin_bswap32(be);
                        memcpy(&f[k], &be, 4);
                    }
                    AddWorldPoint(b, xfm, f[0], f[1], f[2]);
                }
            } else {
                b.Add(xfm.v.x, xfm.v.y, xfm.v.z);
            }
            b.meshes++;
        }
        b.Finish();
        return b;
    }

    // Milo world convention is Z-up, camera basis m.x = right, m.y = forward,
    // m.z = up (dc3 ViewerCamera.cpp:66). Placement is fully derived from the
    // scene bbox and two fixed angles -- no clock, no input, no randomness --
    // which is what makes two runs byte-identical.
    void PlaceCamera(RndCam *cam, const Bounds &b, float azimuth, float elevation,
                     float distanceScale) {
        float cx = 0, cy = 0, cz = 0, dist = 10.0f;
        if (b.valid) {
            cx = (b.minv[0] + b.maxv[0]) * 0.5f;
            cy = (b.minv[1] + b.maxv[1]) * 0.5f;
            cz = (b.minv[2] + b.maxv[2]) * 0.5f;
            float sx = b.maxv[0] - b.minv[0];
            float sy = b.maxv[1] - b.minv[1];
            float sz = b.maxv[2] - b.minv[2];
            float diag = sqrtf(sx * sx + sy * sy + sz * sz);
            dist = diag * distanceScale;
            if (dist < 1.0f) dist = 1.0f;
        }

        float cosEl = cosf(elevation);
        float eyeX = cx + dist * cosEl * sinf(azimuth);
        float eyeY = cy + dist * cosEl * cosf(azimuth);
        float eyeZ = cz + dist * sinf(elevation);

        Vector3 eye(eyeX, eyeY, eyeZ), tgt(cx, cy, cz), fwd, right, up;
        Subtract(tgt, eye, fwd);
        Normalize(fwd, fwd);
        Vector3 worldUp(0, 0, 1);
        Cross(fwd, worldUp, right);
        if (Length(right) < 0.001f) {
            worldUp.Set(0, 1, 0);
            Cross(fwd, worldUp, right);
        }
        Normalize(right, right);
        Cross(right, fwd, up);
        Normalize(up, up);

        Transform xfm;
        xfm.m.x.Set(right.x, right.y, right.z);
        xfm.m.y.Set(fwd.x, fwd.y, fwd.z);
        xfm.m.z.Set(up.x, up.y, up.z);
        xfm.v.Set(eyeX, eyeY, eyeZ);
        cam->SetLocalXfm(xfm);

        // ⚠ near is derived from far, not chosen freely: RndCam::SetFrustum
        // (rndobj/Cam.cpp:286) CLAMPS when far > sMaxFarNearPlaneRatio * near,
        // and sMaxFarNearPlaneRatio is 1000 by default. RB3's `rnd` config has
        // neither cam_default_near_plane nor cam_max_far_near_ratio (both
        // MILO_FAIL at bring-up and leave Cam.cpp:29-30's 1 / 1000), so a ratio
        // of 1/900 sits just inside the clamp and the frustum we ask for is the
        // frustum we get.
        float farD = dist * 8.0f;
        if (farD < 1000.0f) farD = 1000.0f;
        float nearD = farD / 900.0f;
        if (nearD < 0.05f) nearD = 0.05f;
        cam->SetFrustum(nearD, farD, 0.6024f, 1.0f);
        cam->Select();

        printf("  camera: target (%.2f %.2f %.2f) dist %.2f  near %.3f far %.1f\n", cx, cy,
               cz, dist, nearD, farD);

        // -------------------------------------------------------------------
        // The view-projection matrix: TWO paths, and the default is the
        // engine's own.
        //
        // WgpuRnd::WriteSceneUniforms (Rnd_Wgpu.cpp:1252-1271) branches on
        // whether RndCam::GetViewProjMatrix() is the identity. If it is not, it
        // uses that verbatim — the path DC3's milo-viewer takes, building
        // view*proj by hand in ViewerCamera.cpp:79-108. If it IS the identity,
        // the engine calls xenon's own RndCam::GetViewProjectXfms.
        //
        // X3 uses the ENGINE PATH by default, because the reason it did not
        // work turned out to be a locatable one-slot defect in xenon's
        // GetViewProjectXfms (projMtx.y.y read mLocalProjectXfm.v.x, which
        // UpdateLocal zeroes, instead of m.z.y — see rndobj/Cam.cpp:468 and
        // §"semantic findings" in the X3 write-up). Routing around it with the
        // hand-built matrix would have produced the same PNG and buried the
        // bug, which is the failure mode this ladder exists to avoid.
        //
        // --cam-manual keeps DC3's hand-built matrix available as the control:
        // it is the A/B that proved the defect was in the projection and not in
        // the mesh upload, the material bind, or the camera placement.
        // --dump-cam prints both matrices side by side.
        {
            float dr = -Dot(right, eye), df = -Dot(fwd, eye), du = -Dot(up, eye);
            float view[16] = { right.x, fwd.x, up.x, 0, right.y, fwd.y, up.y, 0,
                               right.z, fwd.z, up.z, 0, dr,      df,    du,   1 };
            float aspect = 16.0f / 9.0f;
            float cot = 1.0f / tanf(cam->YFov() * 0.5f);
            float zr = farD - nearD;
            float proj[16] = { cot / aspect, 0, 0,          0,
                               0,            0, farD / zr,  1,
                               0,            cot, 0,        0,
                               0,            0, -nearD * farD / zr, 0 };
            float vp[16];
            for (int i = 0; i < 4; i++)
                for (int j = 0; j < 4; j++) {
                    float s = 0;
                    for (int k = 0; k < 4; k++) s += view[i * 4 + k] * proj[k * 4 + j];
                    vp[i * 4 + j] = s;
                }
            if (gDumpCam) {
                const Hmx::Matrix4 &engineVp = cam->GetViewProjMatrix();
                printf("  cam viewProj BEFORE SetViewProj (engine state):\n");
                const float *e = (const float *)&engineVp;
                for (int i = 0; i < 4; i++)
                    printf("      %9.4f %9.4f %9.4f %9.4f\n", e[i * 4], e[i * 4 + 1],
                           e[i * 4 + 2], e[i * 4 + 3]);
                Transform vX;
                Hmx::Matrix4 pM;
                cam->UpdatedWorldXfm();
                cam->GetViewProjectXfms(vX, pM);
                const float *p = (const float *)&pM;
                printf("  cam GetViewProjectXfms proj (what the engine would use):\n");
                for (int i = 0; i < 4; i++)
                    printf("      %9.4f %9.4f %9.4f %9.4f\n", p[i * 4], p[i * 4 + 1],
                           p[i * 4 + 2], p[i * 4 + 3]);
                printf("  cam viewProj we install (DC3 ViewerCamera math):\n");
                for (int i = 0; i < 4; i++)
                    printf("      %9.4f %9.4f %9.4f %9.4f\n", vp[i * 4], vp[i * 4 + 1],
                           vp[i * 4 + 2], vp[i * 4 + 3]);
            }
            if (gManualCam) {
                Hmx::Matrix4 m;
                memcpy(&m, vp, 64);
                cam->SetViewProj(m);
                printf("  camera: --cam-manual — DC3-style hand-built viewProj installed "
                       "(engine's GetViewProjectXfms BYPASSED)\n");
            }
        }
    }

    // -----------------------------------------------------------------------
    // Lighting. A .milo that ships no RndEnviron renders black under any
    // correct lighting model, so a synthetic ambient + key light is added when
    // the scene has none. It is announced in the log every time, because an
    // added light is a deviation from the asset and a reader must be able to
    // tell "this is what the file looks like" from "this is what the file looks
    // like plus my flashlight".
    // -----------------------------------------------------------------------
    // -----------------------------------------------------------------------
    // Fallback material.
    //
    // Mesh_Wgpu.cpp:167-172 hard-skips any mesh whose Mat() is null ("no
    // material"), which is correct for a game — a material-less mesh is not
    // renderable and the real venue supplies one. But it makes an entire class
    // of RB3 asset invisible to an asset viewer, and the X3 static cell is
    // exactly that class: ui/track/gen/tracksystem_meshes.milo_xbox is 130
    // meshes and (MEASURED, and corroborated by X2's census) ZERO Mat objects.
    // It is a geometry LIBRARY whose materials are supplied by whatever venue
    // milo instantiates it.
    //
    // So a neutral prelit grey is attached to material-less meshes, and it is
    // announced every time, because the reader must be able to tell "this is
    // what the asset looks like" from "this is the asset's geometry under my
    // own material". Prelit deliberately: it takes the lighting model out of
    // the picture for the cell whose job is to prove GEOMETRY reaches the GPU.
    RndMat *MakeFallbackMat(ObjectDir *dir) {
        RndMat *m = Hmx::Object::New<RndMat>();
        m->SetName("x3_fallback_mat", dir);
        m->SetColor(0.72f, 0.72f, 0.74f);
        m->SetPreLit(true);
        m->SetUseEnv(false);
        m->SetZMode(kZModeNormal);
        m->SetBlend(BaseMaterial::kBlendSrc);
        m->SetAlphaCut(false);
        return m;
    }

    RndEnviron *FindEnv(ObjectDir *dir) {
        RndDir *rd = dynamic_cast<RndDir *>(dir);
        if (rd && rd->GetEnv()) return rd->GetEnv();
        for (ObjDirItr<RndEnviron> it(dir, true); it; ++it) return it;
        return nullptr;
    }

    RndEnviron *EnsureEnv(ObjectDir *dir, bool &synthetic) {
        RndEnviron *env = FindEnv(dir);
        synthetic = false;
        if (env) {
            printf("  environ: scene's own '%s'\n", env->Name());
            return env;
        }
        synthetic = true;
        env = Hmx::Object::New<RndEnviron>();
        env->SetName("x3_synth_env", dir);

        Hmx::Color amb;
        amb.Set(0.35f, 0.35f, 0.40f);
        env->SetAmbientColor(amb);

        RndLight *key = Hmx::Object::New<RndLight>();
        key->SetName("x3_synth_key", dir);
        key->SetLightType(RndLight::kDirectional);
        Hmx::Color col;
        col.Set(0.95f, 0.92f, 0.88f);
        key->SetColor(col);
        key->SetShowing(true);
        {
            Transform xfm;
            xfm.Reset();
            Vector3 d(0.4f, 0.8f, -0.45f);
            Normalize(d, d);
            xfm.m.z = d;
            Vector3 upv(0, 0, 1);
            Cross(d, upv, xfm.m.x);
            Normalize(xfm.m.x, xfm.m.x);
            Cross(xfm.m.x, d, xfm.m.y);
            key->SetLocalXfm(xfm);
        }
        env->AddLight(key);
        printf("  environ: SYNTHETIC (scene has no RndEnviron) — ambient 0.35 + 1 "
               "directional key\n");
        return env;
    }

    // -----------------------------------------------------------------------
    // Image statistics. See the header note on why "a PNG exists" is not a gate.
    // -----------------------------------------------------------------------
    struct ImageStats {
        size_t distinct = 0;
        double coverage = 0.0; // fraction of pixels != modal colour
        unsigned int modal = 0;
    };

    ImageStats Analyse(const uint8_t *px, int w, int h) {
        std::map<unsigned int, size_t> hist;
        size_t n = (size_t)w * h;
        for (size_t i = 0; i < n; i++) {
            unsigned int rgb = ((unsigned int)px[i * 4] << 16) |
                ((unsigned int)px[i * 4 + 1] << 8) | (unsigned int)px[i * 4 + 2];
            hist[rgb]++;
        }
        ImageStats s;
        s.distinct = hist.size();
        size_t best = 0;
        for (std::map<unsigned int, size_t>::const_iterator it = hist.begin();
             it != hist.end(); ++it) {
            if (it->second > best) {
                best = it->second;
                s.modal = it->first;
            }
        }
        s.coverage = n ? (double)(n - best) / (double)n : 0.0;
        return s;
    }

    // -----------------------------------------------------------------------
    // One cell.
    // -----------------------------------------------------------------------
    struct CellResult {
        bool ok = false;
        std::string png;
        ImageStats stats;
        int meshes = 0;
        int drawn = 0;
        int skinned = 0;
        int withMat = 0;
        int withTex = 0;
    };

    // These two thresholds are deliberately LOW. The bar for X3 is "visible
    // geometry", not "retail quality" -- a degraded character (untextured,
    // T-posed) is an acceptable X3 outcome and must not be failed by a
    // threshold tuned for a good one. Anything above them is reported with its
    // real numbers so the reader judges the picture, not the constant.
    const double kMinCoverage = 0.01; // >=1% of pixels differ from background
    const size_t kMinDistinct = 16;

    CellResult RenderCell(const char *arkPath, const char *outDir, int frames,
                          float azimuth, float elevation, float distScale,
                          bool dumpRnd) {
        CellResult r;
        printf("\n=== %s ===\n", arkPath);

        ObjDirPtr<ObjectDir> dirPtr;
        FilePath fp(arkPath);
        dirPtr.LoadFile(fp, false, false, kLoadFront, false);
        ObjectDir *dir = dirPtr;
        if (!dir) {
            Gate("load", false, "DirLoader returned null");
            return r;
        }
        printf("root: '%s'  [%s]\n", dir->Name() ? dir->Name() : "(unnamed)",
               dir->ClassName().Str());

        // SyncObjects is what wires a RndDir's draw list; without it the
        // meshes exist but nothing is registered to be drawn.
        RndDir *rndDir = dynamic_cast<RndDir *>(dir);
        if (rndDir) rndDir->SyncObjects();
        Gate("rnd-dir", rndDir != nullptr,
             rndDir ? "loaded dir is an RndDir (SyncObjects run)"
                    : "loaded dir is NOT an RndDir — nothing here can draw");

        // Census the drawable surface. This is the bridge back to X2's numbers:
        // if the mesh count here disagrees with rb3-milo's census, the render
        // is being asked about a different scene than the one X2 certified.
        std::vector<RndMesh *> meshes;
        for (ObjDirItr<RndMesh> it(dir, true); it; ++it) {
            RndMesh *m = it;
            meshes.push_back(m);
            r.meshes++;
            if (m->IsSkinned()) r.skinned++;
            RndMat *mat = m->Mat();
            if (mat) {
                r.withMat++;
                if (mat->GetDiffuseTex()) r.withTex++;
            }
            if (gVerbose) {
                printf("    mesh '%-38s showing=%d verts=%d cverts=%d faces=%d "
                       "bones=%d mat=%s\n",
                       (std::string(m->Name()) + "'").c_str(), (int)m->Showing(),
                       m->NumVerts(), m->NumCompressedVerts(), m->NumFaces(),
                       m->NumBones(), mat ? mat->Name() : "(none)");
            }
        }
        {
            char d[192];
            snprintf(d, sizeof(d), "%d meshes (%d skinned), %d with a Mat, %d with a "
                                   "diffuse Tex",
                     r.meshes, r.skinned, r.withMat, r.withTex);
            Gate("drawable-census", r.meshes > 0, d);
        }
        if (r.meshes == 0) return r;

        if (r.withMat < r.meshes) {
            RndMat *fallback = MakeFallbackMat(dir);
            int patched = 0;
            for (size_t i = 0; i < meshes.size(); i++) {
                if (!meshes[i]->Mat()) {
                    meshes[i]->SetMat(fallback);
                    patched++;
                }
            }
            printf("  ⚠ material fallback: %d of %d meshes ship NO Mat and were given a "
                   "neutral prelit grey — their appearance below is OUR material, not "
                   "the asset's\n",
                   patched, r.meshes);
        }

        Character *character = nullptr;
        for (ObjDirItr<Character> it(dir, true); it; ++it) {
            character = it;
            break;
        }
        if (character) printf("  character: '%s'\n", character->Name());

        Bounds b = SceneBounds(dir);
        if (b.valid) {
            printf("  bbox: (%.2f %.2f %.2f) .. (%.2f %.2f %.2f) over %d meshes "
                   "[robust, 0.5-99.5 pct of %zu verts]\n",
                   b.minv[0], b.minv[1], b.minv[2], b.maxv[0], b.maxv[1], b.maxv[2],
                   b.meshes, b.axis[0].size());
            if (b.Trimmed()) {
                printf("  ⚠ bbox outliers TRIMMED (raw span > 2x robust span) — raw "
                       "extremes were (%.2f %.2f %.2f) .. (%.2f %.2f %.2f). On "
                       "tracksystem_meshes DC3's viewer reported the identical "
                       "artifact: \"one mesh parsed a garbage Y (121458)\".\n",
                       b.rawMin[0], b.rawMin[1], b.rawMin[2], b.rawMax[0], b.rawMax[1],
                       b.rawMax[2]);
            }
        }
        Gate("bbox", b.valid,
             b.valid ? "" : "no mesh contributed a vertex — the camera would frame nothing");

        RndCam *cam = Hmx::Object::New<RndCam>();
        cam->SetName("x3_cam", dir);
        PlaceCamera(cam, b, azimuth, elevation, distScale);

        bool syntheticEnv = false;
        RndEnviron *env = EnsureEnv(dir, syntheticEnv);
        Vector3 origin(0, 0, 0);
        if (env) env->Select(&origin);

        if (dumpRnd) DumpRndMembers("after scene setup, before first frame");

        // Draw. Several frames because the GPU-resource path is lazy: a mesh's
        // vertex buffer and a texture's GPU image are created on first use, and
        // the very first frame can legitimately draw a mesh whose texture has
        // not been uploaded yet.
        // ⚠ THE ENGINE'S LOD NAME-FILTER MISFIRES ON RB3 CROWD CHARACTERS.
        //
        // Mesh_Wgpu.cpp:135 drops any mesh whose name contains "_lod", with the
        // comment "drawn by Character::DrawLod in the full engine, but we
        // iterate all meshes directly in the viewer". That is a reasonable DC3
        // heuristic and it is WRONG for RB3: crowd_female01's entire body is
        // ONE mesh named `female_crowd_body01_lod02.mesh` — RB3's crowd
        // characters are authored AS the LOD-2 asset, there is no higher-detail
        // sibling to prefer. Left alone, the character renders as two
        // disembodied hands. (DC3's own viewer produced exactly that on this
        // asset; the oracle screenshot shows the same two hands.)
        //
        // So name-filtered meshes are re-issued through DrawMeshImmediate,
        // which is the same draw body minus the prefilter, and the count is
        // REPORTED so the deviation is never silent. The real fix is upstream —
        // either the filter learns that a lod-suffixed mesh with no sibling is
        // the only geometry there is, or the consumer drives
        // Character::DrawShowing and lets DrawLodOrShadow choose. Both are
        // X4-shaped; neither is a xenon change.
        // ⚠ THE VIEWPORT DEPTH RANGE IS [0,0] UNLESS SOMEONE SETS IT.
        //
        // NgRnd::Viewport's default ctor zeroes all six fields (rndobj/Rnd_NG.h:18
        // — and DC3's copy is byte-identical, so this is a SHARED default, not a
        // xenon divergence). WgpuRnd::ApplyViewport (Rnd_Wgpu.cpp:566-574) papers
        // over zero Width/Height by substituting the render-target size, but it
        // passes MinZ/MaxZ THROUGH: wgpu SetViewport(x, y, w, h, 0.0f, 0.0f).
        //
        // A [0,0] depth range is legal and does not stop anything rasterising —
        // which is exactly why it is dangerous. Every fragment's depth is forced
        // to 0, so the depth buffer stops discriminating and draw order silently
        // becomes paint order. On a single-mesh subject nothing looks wrong; on a
        // venue it is wrong everywhere and looks like a material bug.
        //
        // Setting it here rather than reporting it is deliberate: the viewport is
        // the CONSUMER's to establish (on X360, Rnd's own device bring-up does
        // it), and DC3's viewer gets away without one only because its scenes
        // have not needed depth to be right. Measured A/B on these two cells: the
        // PNGs are byte-identical either way, so this changes nothing X3 shows —
        // it removes a trap X4 would otherwise walk into with a venue.
        {
            NgRnd::Viewport v;
            v.X = 0;
            v.Y = 0;
            v.Width = (unsigned int)gWgpuRnd->Gpu().WindowWidth();
            v.Height = (unsigned int)gWgpuRnd->Gpu().WindowHeight();
            v.MinZ = 0.0f;
            v.MaxZ = 1.0f;
            TheNgRnd.SetViewport(v);
        }

        int lodForced = 0;
        for (int f = 0; f < frames; f++) {
            TheRnd.BeginDrawing();
            r.drawn = 0;
            lodForced = 0;
            for (size_t i = 0; i < meshes.size(); i++) {
                RndMesh *m = meshes[i];
                if (!m->Showing()) continue;
                if (m->Mat() && strstr(m->Name(), "_lod")) {
                    DrawMeshImmediate(m);
                    lodForced++;
                } else {
                    m->DrawShowing();
                }
                r.drawn++;
            }
            TheRnd.EndDrawing();
        }
        if (lodForced) {
            printf("  ⚠ LOD filter bypassed for %d mesh(es) — the engine's \"_lod\" name "
                   "skip would have dropped them, and on RB3 crowd characters that IS "
                   "the whole body\n",
                   lodForced);
        }
        {
            char d[128];
            snprintf(d, sizeof(d), "%d of %d meshes issued a draw, %d frame(s)", r.drawn,
                     r.meshes, frames);
            Gate("draws-issued", r.drawn > 0, d);
        }

        if (dumpRnd) DumpRndMembers("after the last EndDrawing");

        // Readback + PNG.
        int w = gWgpuRnd->Gpu().WindowWidth();
        int h = gWgpuRnd->Gpu().WindowHeight();
        size_t bytes = (size_t)w * h * 4;
        std::vector<uint8_t> px(bytes);
        if (!gWgpuRnd->Gpu().ReadbackHeadlessFrame(px.data(), bytes)) {
            Gate("readback", false, "ReadbackHeadlessFrame failed");
            return r;
        }

        std::string base(arkPath);
        size_t slash = base.find_last_of('/');
        if (slash != std::string::npos) base = base.substr(slash + 1);
        size_t dot = base.find('.');
        if (dot != std::string::npos) base = base.substr(0, dot);
        r.png = std::string(outDir) + "/" + base + ".png";

        if (!WriteScreenshot(r.png.c_str(), px.data(), w, h)) {
            Gate("png", false, r.png.c_str());
            return r;
        }
        Gate("png", true, r.png.c_str());

        r.stats = Analyse(px.data(), w, h);
        {
            char d[224];
            snprintf(d, sizeof(d),
                     "coverage %.2f%% (>= %.0f%%), %zu distinct colours (>= %zu), "
                     "background #%06x",
                     r.stats.coverage * 100.0, kMinCoverage * 100.0, r.stats.distinct,
                     kMinDistinct, r.stats.modal);
            Gate("image-not-empty",
                 r.stats.coverage >= kMinCoverage && r.stats.distinct >= kMinDistinct, d);
        }

        r.ok = r.stats.coverage >= kMinCoverage && r.stats.distinct >= kMinDistinct;

        // Drop the scene before the next cell: two milos alive at once is a
        // different (and untested) thing from one at a time, and the second
        // cell's numbers must describe the second cell.
        dirPtr = nullptr;
        return r;
    }

} // namespace

int main(int argc, char **argv) {
    // Line-buffer: a SIGSEGV inside the renderer would otherwise discard the
    // whole report and leave no trace of how far it got.
    setvbuf(stdout, nullptr, _IOLBF, 0);

    int width = 1280, height = 720, frames = 4;
    bool dumpRnd = false;
    std::vector<const char *> pos;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verbose") == 0) gVerbose = true;
        else if (strcmp(argv[i], "--dump-rnd") == 0) dumpRnd = true;
        else if (strcmp(argv[i], "--dump-cam") == 0) gDumpCam = true;
        else if (strcmp(argv[i], "--cam-manual") == 0) gManualCam = true;
        else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) width = atoi(argv[++i]);
        else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) height = atoi(argv[++i]);
        else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) frames = atoi(argv[++i]);
        else pos.push_back(argv[i]);
    }
    if (pos.size() < 2) {
        fprintf(stderr,
                "usage: %s <dataDir> <outDir> [<arkPath> ...] "
                "[--width N] [--height N] [--frames N] [--dump-rnd] [--dump-cam]\n"
                "       [--cam-manual] [--verbose]\n"
                "  with no arkPath the two X3 cells run:\n"
                "    ui/track/gen/tracksystem_meshes.milo_xbox   (130 static meshes)\n"
                "    char/crowd/gen/crowd_female01.milo_xbox     (skinned character)\n",
                argv[0]);
        return 1;
    }
    const char *dataDir = pos[0];
    const char *outDir = pos[1];
    // mkdir -p. A single mkdir() silently no-ops on a missing PARENT and the
    // failure only surfaces four gates later as "coverage 0.00%", which reads
    // like a renderer bug. (It did, once, during X3 bring-up.)
    {
        std::string path(outDir);
        for (size_t i = 1; i <= path.size(); i++) {
            if (i == path.size() || path[i] == '/') {
                std::string part = path.substr(0, i);
                mkdir(part.c_str(), 0755);
            }
        }
    }

    // Per-cell camera framing. The static-mesh cell is a wide flat track piece
    // and the character is a tall narrow figure, so one distance multiplier
    // cannot serve both; these are the two constants and they are the only
    // per-asset tuning in the file.
    struct Cell {
        const char *path;
        float azimuth, elevation, distScale;
    };
    std::vector<Cell> cells;
    if (pos.size() == 2) {
        cells.push_back({"ui/track/gen/tracksystem_meshes.milo_xbox", 0.45f, 0.35f, 0.9f});
        cells.push_back({"char/crowd/gen/crowd_female01.milo_xbox", 0.35f, 0.10f, 1.15f});
    } else {
        for (size_t i = 2; i < pos.size(); i++)
            cells.push_back({pos[i], 0.45f, 0.30f, 0.9f});
    }

    // ---- bring-up: the exact X2 prologue, in the exact X2 order -----------
    // Every step here is load-bearing and each was found by a crash; the
    // reasoning is documented at length in main_milo.cpp and is not repeated.
    // The ONLY additions X3 makes are StandUpConfig() (a real config instead of
    // a synthesised `objects` section, because Rnd::PreInit reads it) and
    // StandUpRenderer().
    InitMakeString();
    Symbol::Init();
    FileInit();
    NativeSetDataDir(dataDir);
    SetUsingCD(true);

    printf("=== rb3-render: X3 first rendered frame, dc3 WebGPU backend ===\n");
    printf("dataDir : %s\noutDir  : %s\n", dataDir, outDir);

    DataInit();
    NativeArchiveInit();
    if (!TheArchive) {
        fprintf(stderr, "FATAL: TheArchive is null after NativeArchiveInit()\n");
        return 1;
    }
    Gate("archive-mounted", true, TheArchive->GetArkfileName(0));

    if (!StandUpConfig()) {
        printf("\nRESULT: FAILED (%d gate failure(s))\n", gFailures);
        return 1;
    }

    // Order matters and is the reverse of what looks natural: the factories go
    // in BEFORE the renderer. Rnd::PreInit registers the rndobj factories
    // itself, but it also creates default objects through them, and
    // RegisterMiloObjectFactories additionally covers char/ and world/ classes
    // PreInit never touches. Registering twice is harmless (a map overwrite).
    RegisterMiloObjectFactories();

    if (!StandUpRenderer(width, height)) {
        printf("\nRESULT: NO GPU (%d gate failure(s))\n", gFailures);
        return 2;
    }
    if (dumpRnd) DumpRndMembers("immediately after TheRnd.Init()");

    // CharBoneDir::Init() — see main_milo.cpp: without it CharServoBone::Load
    // dereferences a null sCharClipTypes. It also loads the shipped bone
    // resource milos, which the character rig genuinely needs.
    CharBoneDir::Init();

    int passed = 0;
    std::vector<CellResult> results;
    for (size_t i = 0; i < cells.size(); i++) {
        int arkNum = 0, fileSize = 0, ucSize = 0;
        unsigned long long byteOff = 0;
        if (!TheArchive->GetFileInfo(FileMakePath(".", cells[i].path), arkNum, byteOff,
                                     fileSize, ucSize)) {
            printf("\n=== %s ===\n", cells[i].path);
            Gate("archive-lookup", false, "not present in the archive index");
            results.push_back(CellResult());
            continue;
        }
        CellResult r = RenderCell(cells[i].path, outDir, frames, cells[i].azimuth,
                                  cells[i].elevation, cells[i].distScale, dumpRnd);
        results.push_back(r);
        if (r.ok) passed++;
    }

    printf("\n=== summary ===\n");
    for (size_t i = 0; i < cells.size(); i++) {
        const CellResult &r = results[i];
        printf("  %-8s %-46s %s\n", r.ok ? "RENDER" : "EMPTY", cells[i].path,
               r.png.empty() ? "(no png)" : r.png.c_str());
        if (!r.png.empty()) {
            printf("           %d meshes (%d skinned, %d textured), %d drawn, "
                   "coverage %.2f%%, %zu colours\n",
                   r.meshes, r.skinned, r.withTex, r.drawn, r.stats.coverage * 100.0,
                   r.stats.distinct);
        }
    }
    Gate("all-cells-rendered", passed == (int)cells.size(), "");

    printf("\nRESULT: %s (%d gate failure(s))\n",
           gFailures == 0 ? "ALL GATES PASSED" : "FAILED", gFailures);

    // Release GPU objects, then _exit WITHOUT running static destructors.
    //
    // Both halves matter. Terminate() drops the renderer's own wgpu handles in
    // the order Dawn wants. _exit then skips the remaining static dtors --
    // MEASURED: letting them run segfaults inside
    // ~unordered_map<RndTex*, GpuTexData> -> wgpu::Texture::WGPURelease ->
    // dawn::native::vulkan::VulkanInstance::~VulkanInstance, i.e. the texture
    // cache outlives the Vulkan instance that owns its objects. It is a
    // teardown-ordering bug in the engine's static-lifetime GPU caches, it is
    // NOT ours to fix from a consumer, and it would otherwise turn a clean
    // rc=0 run into a core dump AFTER the verdict was printed -- which is the
    // most confusing possible way to report success. dc3's milo-viewer ends
    // with _exit(rc) for the same reason (milo_viewer.cpp:488).
    // Reported as an engine-backlog item in the X3 write-up.
    if (gWgpuRnd) gWgpuRnd->Terminate();
    fflush(stdout);
    fflush(stderr);
    _exit(gFailures == 0 ? 0 : 1);
    return gFailures == 0 ? 0 : 1;
}
