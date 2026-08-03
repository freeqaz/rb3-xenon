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
#include "char/CharClip.h"
#include "char/CharClipSet.h"
#include "char/CharDriver.h"
#include "char/CharServoBone.h"
#include "char/Character.h"
#include "obj/Task.h"
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
#include "boot_invariants.h"
#include "os/System.h"
#include "rndobj/Cam.h"
#include "rndobj/Dir.h"
#include "rndobj/Draw.h"
#include "rndobj/Env.h"
#include "rndobj/PostProc.h"
#include "rndobj/Lit.h"
#include "rndobj/Mat.h"
#include "rndobj/Mesh.h"
#include "rndobj/Rnd.h"
#include "rndobj/Rnd_NG.h"
#include "rndobj/Tex.h"
#include "rndobj/Trans.h"
#include "rndobj/TransProxy.h"
#include "rndobj/MultiMesh.h"
#include "world/Crowd.h"
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
#include <set>
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

namespace {

    int gFailures = 0;
    bool gVerbose = false;

    // ---------------------------------------------------------------------
    // X4a: --postproc — select a REAL shipped RndPostProc before drawing.
    // ---------------------------------------------------------------------
    // X3 left RndPostProc entirely UNREACHED, and the X4 charter asks for a
    // per-subsystem VERIFIED/SYNTHESIZED/UNREACHED verdict backed by evidence.
    // For post-processing that evidence is available cheaply and honestly,
    // because the engine's own branch is a simple predicate:
    //
    //   Rnd_Wgpu.cpp:454  bool hasPostProc = RndPostProc::Current() != nullptr;
    //   Rnd_Wgpu.cpp:459-462  the colour attachment is retargeted to
    //                         mIntermediateView instead of FrameTarget()
    //   Rnd_Wgpu.cpp:508-509  mPostProcPass.Run(...) on the intermediate
    //
    // So selecting a real RndPostProc and re-rendering the SAME geometry is a
    // controlled A/B on exactly that branch: if the PNG changes, the pass ran.
    // A synthesised RndPostProc would prove much less (it would be our
    // parameters), so this loads RB3's shipped
    // world/shared/fx/gen/post_process_fx_venue.milo_xbox — 40 real PostProc
    // objects and, measurably, ZERO band3 classes, which is why it is loadable
    // in a target that compiles no band3 (see the venue note in
    // docs/plans/x4a-venue-render-2026-08-02.md).
    const char *gPostProcFile = nullptr;
    const char *gPostProcName = nullptr;
    ObjDirPtr<ObjectDir> gPostProcDir;
    bool gDumpCam = false;
    bool gManualCam = false;

    // ---- X4b: clip-driven character animation -------------------------------
    // crowd_female01 ships a CharDriver ('main.drv'), a CharServoBone
    // ('bone.servo') and two EMPTY CharClipSets ('female_base', 'male_base') --
    // measured with rb3-milo: 68 objects, 12 classes, ZERO CharClip. The clips
    // live in a separate asset that the character's 'crowd_clips.fm' FileMerger
    // pulls in. Enumerating the ark (Archive::Enumerate on char/crowd) finds it:
    //
    //     char/crowd/anim/gen/female_base.milo_xbox   44 CharClip, 12 CharClipGroup
    //
    // i.e. the CharClipSet name in the character IS the clip milo's basename.
    // That file is the animation source; it is real shipped RB3 content, not
    // synthesised, and it contains no band3 classes so it loads in this target.
    const char *gClipsFile = nullptr;
    const char *gClipName = nullptr;
    float gBeat = -1.0f; // <0 => no animation requested
    float gBpm = 120.0f;
    bool gBoneAudit = false;
    // X4d framing/isolation overrides (driver-only; no shared src/ involvement).
    const char *gOnlyMesh = nullptr;
    bool gDumpTree = false; // X5: per-dir object census of the loaded subdir tree
    const char *gFocus = nullptr; // X5: frame the camera on meshes matching this
    const char *gSceneClip = nullptr; // X5: drive ALL scene characters with this clip
    // X6: draw the venue's WorldCrowd objects. Default ON with an
    // RB3_NO_CROWD_DRAW opt-out, per the project's ack rule for native render
    // fixes -- the opt-out makes X5's venue frame an exact single-variable A/B.
    // X6: --crowd-all overrides the asset's mShowing flags to draw all six
    // WorldCrowds. Measured safe (zero position overlap); not the default
    // because overriding shipped flags is a judgement call. See the block in
    // the crowd-collection site.
    bool gCrowdShowAll = false;
    // X6: crowd archetype Characters -> their meshes. These are drawn once per
    // baked instance transform, and suppressed in the flat mesh loop.
    std::map<Character *, std::vector<RndMesh *> > gArchetypeMeshes;
    std::set<RndMesh *> gArchetypeMeshSet;
    float gDistScale = -1.0f;   // <0 = use the per-cell default
    float gAzimuth = -999.0f;
    float gElevation = -999.0f;
    ObjDirPtr<ObjectDir> gClipsDir;
    ObjDirPtr<ObjectDir> gPlayerDir; // X5: the player0 lighting stand-in
    const char *gPlayerStandIn = nullptr;

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

    // X4c: for a SKINNED mesh, `meshWorld * bindVert` is NOT where the geometry
    // is -- the palette places it, and the engine forces object.world to
    // identity for skinned meshes. The two agree only at bind pose (where
    // skin_b == meshWorld for every b, which is exactly the palette invariant),
    // so framing the camera on bind verts silently mis-frames every posed
    // character. Bounds are taken AFTER the pose (SceneBounds is called at the
    // call site below DriveCharacterClip), so the live palette is available.
    bool SkinnedMeshBounds(Bounds &b, RndMesh *m) {
        if (!m->IsSkinned()) return false;
        // Scoped to POSED renders only. At bind pose the two framings agree by
        // construction (skin_b == meshWorld for every b -- that IS the palette
        // invariant), so the only thing using the palette at bind would buy is a
        // float-ordering difference that spends X3's byte-identical evidence PNG
        // for nothing. Restricting it here keeps the bind cell reproducible and
        // still fixes the case that was actually broken.
        if (!(gClipsFile && gBeat >= 0.0f)) return false;
        RndMesh *owner = m->GetGeomOwner();
        if (!owner) owner = m;
        int ncv = owner->NumCompressedVerts();
        const unsigned char *data = owner->CompressedVerts();
        if (ncv <= 0 || !data) return false;
        int nb = m->NumBones();
        if (nb <= 0) return false;
        std::vector<Transform> pal((size_t)nb);
        for (int i = 0; i < nb; i++) {
            RndTransformable *bt = m->BoneTransAt(i);
            if (bt) Multiply(m->BoneOffsetAt(i), bt->WorldXfm(), pal[i]);
            else pal[i].Reset();
        }
        for (int i = 0; i < ncv; i++) {
            const unsigned char *rec = data + (size_t)i * 36;
            auto be32 = [&](int off) {
                unsigned int v;
                memcpy(&v, rec + off, 4);
                return __builtin_bswap32(v);
            };
            float p[3];
            for (int k = 0; k < 3; k++) {
                unsigned int v = be32(k * 4);
                float f;
                memcpy(&f, &v, 4);
                p[k] = f;
            }
            unsigned int wv = be32(28), iv = be32(32);
            float w[4] = { (wv & 0x3FF) / 1023.0f, ((wv >> 10) & 0x3FF) / 1023.0f,
                           ((wv >> 20) & 0x3FF) / 1023.0f, ((wv >> 30) & 0x3) / 3.0f };
            float acc[3] = { 0, 0, 0 };
            for (int k = 0; k < 4; k++) {
                int bi = (iv >> (k * 8)) & 0xFF;
                if (w[k] == 0.0f || bi >= nb) continue;
                const Transform &t = pal[bi];
                acc[0] += w[k] * (t.m.x.x * p[0] + t.m.y.x * p[1] + t.m.z.x * p[2] + t.v.x);
                acc[1] += w[k] * (t.m.x.y * p[0] + t.m.y.y * p[1] + t.m.z.y * p[2] + t.v.y);
                acc[2] += w[k] * (t.m.x.z * p[0] + t.m.y.z * p[1] + t.m.z.z * p[2] + t.v.z);
            }
            b.Add(acc[0], acc[1], acc[2]);
        }
        return true;
    }

    // ★ X5: collect every T in the dir tree, INCLUDING dir-typed OBJECTS.
    //
    // ObjDirItr(dir, recurse=true) descends only through mSubDirs (see
    // ObjectDir::NextSubDir, obj/Dir.h:536). That is FAITHFUL to retail and is
    // not a decomp defect -- retail never needs a flat mesh list because it
    // draws through the RndDrawable tree, where a Character draws its own
    // contents when its parent draws it.
    //
    // This driver does not have that tree; it draws a FLAT vector of RndMesh.
    // So for this driver -- and only for this driver -- the mSubDirs-only walk
    // silently omits every mesh that lives inside a dir-typed object, which is
    // exactly where a loaded Character's meshes live. Hence a fully resident
    // eight-member crowd censusing as "0 skinned meshes".
    //
    // `seen` is load-bearing, not defensive: char/crowd/anim/shared_clips.milo
    // is reachable under all eight crowd members, and several proxies are
    // reachable by more than one path. Without it this walk revisits shared
    // dirs and double-counts (and, with a cyclic proxy, would not terminate).
    template <class T>
    void CollectDeep(ObjectDir *dir, std::vector<T *> &out,
                     std::set<ObjectDir *> &seen, int depth) {
        if (!dir || depth > 16) return;
        if (!seen.insert(dir).second) return;
        for (ObjectDir::Entry *e = dir->HashTable().Begin(); e;
             e = dir->HashTable().Next(e)) {
            if (!e->obj) continue;
            T *t = dynamic_cast<T *>(e->obj);
            if (t) out.push_back(t);
            ObjectDir *od = dynamic_cast<ObjectDir *>(e->obj);
            if (od) CollectDeep<T>(od, out, seen, depth + 1);
        }
        for (int i = 0; i < (int)dir->SubDirs().size(); i++) {
            CollectDeep<T>(dir->SubDirs()[i].Ptr(), out, seen, depth + 1);
        }
    }

    template <class T> std::vector<T *> CollectDeep(ObjectDir *dir) {
        std::vector<T *> out;
        std::set<ObjectDir *> seen;
        CollectDeep<T>(dir, out, seen, 0);
        return out;
    }

    // ★ X5: POSITIVE evidence that the player-anchor chain actually bound.
    //
    // "the warning stopped printing" is an absence, and the charter's rule is
    // that silence is not success. RndTransProxy::Sync() (rndobj/TransProxy.cpp
    // :73-90) calls SetTransParent(target) ONLY when it resolved mProxy and
    // found mPart inside it; on failure it explicitly SetTransParent(nullptr).
    // So a non-null TransParent() is a direct, per-object assertion that the
    // proxy is bound AND that the named part was found inside the target.
    void ReportTransProxyBinding(ObjectDir *dir) {
        std::vector<RndTransProxy *> tps = CollectDeep<RndTransProxy>(dir);
        int bound = 0, unbound = 0, boundPlayer = 0;
        for (size_t i = 0; i < tps.size(); i++) {
            bool b = tps[i]->TransParent() != nullptr;
            if (b) {
                bound++;
                if (strstr(tps[i]->Name(), "player")) boundPlayer++;
            } else {
                unbound++;
            }
        }
        printf("  --- TransProxy binding: %d bound / %d unbound of %d "
               "(%d bound proxies are player anchors) ---\n",
               bound, unbound, (int)tps.size(), boundPlayer);
    }

    // ★ X5: object counts BY CLASS over the whole tree. The charter's preferred
    // scene oracle -- an absolute census that a coverage% or a mesh total
    // cannot substitute for.
    void ReportClassHistogram(ObjectDir *dir) {
        std::vector<Hmx::Object *> all = CollectDeep<Hmx::Object>(dir);
        std::map<std::string, int> hist;
        for (size_t i = 0; i < all.size(); i++)
            hist[all[i]->ClassName().Str()]++;
        printf("  --- class histogram (%d object(s), %d class(es)) ---\n",
               (int)all.size(), (int)hist.size());
        for (std::map<std::string, int>::const_iterator it = hist.begin();
             it != hist.end(); ++it)
            printf("    %-28s %d\n", it->first.c_str(), it->second);
    }

    // ★ X5: drive EVERY Character in the scene from the clip set the scene
    // itself ships.
    //
    // X4b/X4c drove one character from an externally supplied --clips file.
    // That is not needed in a venue: world/shared/chars.milo's crowd members
    // each carry char/crowd/anim/shared_clips.milo, whose male_base (52 clips)
    // and female_base (56 clips) CharClipSets are already resident and already
    // bound to the right skeleton. Driving from those is both cheaper and more
    // faithful than injecting a foreign clip file.
    //
    // Everything load-bearing from X4b is preserved deliberately:
    //   - StuffBones is NOT optional. Without it the driver plays into an empty
    //     bone set and the figure stays in BIND POSE while reporting success --
    //     the silent-success failure X4b called out.
    //   - the clock is STEPPED, not jumped: CharDriver blends between poll
    //     instants, so one huge jump evaluates a blend over the whole clip.
    //   - --beat is relative to clip->StartBeat().
    int DriveSceneCharacters(ObjectDir *dir, const char *clipName, float beat,
                             float bpm) {
        std::vector<Character *> chars = CollectDeep<Character>(dir);
        int driven = 0, noDriver = 0, noClip = 0, noServo = 0;
        printf("  --- scene clip drive: '%s' @ beat %.2f over %d character(s) ---\n",
               clipName, beat, (int)chars.size());
        for (size_t i = 0; i < chars.size(); i++) {
            Character *c = chars[i];
            CharDriver *drv = c->Driver();
            if (!drv) { noDriver++; continue; }

            // ★ Prefer the clip set the DRIVER IS ALREADY BOUND TO. Each crowd
            // member carries char/crowd/anim/shared_clips.milo, which holds
            // BOTH male_base and female_base, and `crowd_reaching_01` exists in
            // both. Picking the first set that happens to contain the name is
            // hash-order-dependent and gave crowd_male01 a female_base clip.
            // CharDriver::mClips is bound at load; honouring it is the faithful
            // choice and is stable.
            ObjectDir *bound = drv->ClipDir();
            CharClipSet *useSet = nullptr;
            CharClip *useClip = nullptr;
            const char *src = "driver-bound";
            int nClips = 0;
            if (bound) {
                std::vector<CharClip *> cl = CollectDeep<CharClip>(bound);
                nClips += (int)cl.size();
                for (size_t k = 0; k < cl.size(); k++) {
                    if (strcmp(cl[k]->Name(), clipName) == 0) {
                        useClip = cl[k];
                        useSet = dynamic_cast<CharClipSet *>(bound);
                        break;
                    }
                }
            }
            // ⛔ DELIBERATELY NO cross-set fallback. shared_clips.milo holds
            // BOTH male_base and female_base, and crowd_reaching_01 exists only
            // in female_base -- so a "first set that has the name" search hands
            // crowd_male01 a female clip. That renders, reports success, and is
            // wrong. A character whose OWN bound set lacks the clip is skipped
            // and says so.
            std::vector<CharClipSet *> sets = CollectDeep<CharClipSet>(c);
            if (!useClip) {
                std::string avail;
                for (size_t s = 0; s < sets.size(); s++) {
                    std::vector<CharClip *> cl = CollectDeep<CharClip>(sets[s]);
                    nClips += (int)cl.size();
                    for (size_t k = 0; k < cl.size() && k < 4; k++) {
                        avail += (avail.empty() ? "" : ", ");
                        avail += cl[k]->Name();
                    }
                }
                printf("    %-22s bound set '%s' has no clip '%s' (%d clip(s) in %d "
                       "set(s); e.g. %s) — SKIPPED\n",
                       c->Name(), bound ? bound->Name() : "(none)", clipName, nClips,
                       (int)sets.size(), avail.c_str());
                noClip++;
                continue;
            }

            if (useSet) drv->SetClips(useSet);
            CharServoBone *servo = c->Find<CharServoBone>("bone.servo", false);
            if (servo) {
                    std::vector<CharClip *> all =
                    CollectDeep<CharClip>(useSet ? (ObjectDir *)useSet : bound);
                for (size_t k = 0; k < all.size(); k++) all[k]->StuffBones(*servo);
            } else {
                noServo++;
            }
            drv->Enter();
            drv->Play(useClip, CharClip::kPlayNow | CharClip::kPlayLoop, -1.0f, 1e30f,
                      0.0f);
            printf("    %-22s clip '%s' from set '%s' [%s] (beats %.2f..%.2f)%s\n",
                   c->Name(), useClip->Name(),
                   useSet ? useSet->Name() : (bound ? bound->Name() : "?"), src,
                   useClip->StartBeat(), useClip->EndBeat(),
                   servo ? "" : "  ⚠ NO bone.servo — will not move");
            driven++;
        }
        if (driven == 0) {
            printf("    => drove NOTHING (%d without a CharDriver, %d without the clip)\n",
                   noDriver, noClip);
            return 0;
        }

        // Step the shared clock once for the whole scene, then Poll() every
        // driven character at each instant.
        float step = 0.1f;
        float stepSeconds = step * 60.0f / bpm;
        float b = 0.0f, seconds = 0.0f;
        TheTaskMgr.SetSecondsAndBeat(seconds, b, true);
        int polls = 0;
        while (b + step < beat) {
            b += step;
            seconds += stepSeconds;
            TheTaskMgr.SetSecondsAndBeat(seconds, b, false);
            for (size_t i = 0; i < chars.size(); i++)
                if (chars[i]->Driver()) chars[i]->Poll();
            polls++;
        }
        seconds = beat * 60.0f / bpm;
        TheTaskMgr.SetSecondsAndBeat(seconds, beat, false);
        for (size_t i = 0; i < chars.size(); i++)
            if (chars[i]->Driver()) chars[i]->Poll();
        polls++;
        printf("    => drove %d character(s), polled %d time(s) to beat %.2f "
               "(%d no-driver, %d no-clip, %d no-servo)\n",
               driven, polls, beat, noDriver, noClip, noServo);
        return driven;
    }

    // ★ X5: ABSOLUTE world placement of every Character in the scene.
    //
    // The charter's oracle rule: for a scene, prefer absolute checks (named
    // presence, counts by class, absolute world positions) over invariants.
    // Eight crowd members that all render at the SAME world position look, in
    // any aggregate count or coverage %, exactly like eight correctly placed
    // ones. Only printing the positions separates them.
    void ReportCharacterPlacement(ObjectDir *dir) {
        std::vector<Character *> chars = CollectDeep<Character>(dir);
        printf("  --- character placement (%d Character(s)) ---\n", (int)chars.size());
        std::map<std::string, int> atPos;
        for (size_t i = 0; i < chars.size(); i++) {
            Character *c = chars[i];
            const Vector3 &w = c->WorldXfm().v;
            std::vector<RndMesh *> cm = CollectDeep<RndMesh>(c);
            int skinned = 0;
            for (size_t j = 0; j < cm.size(); j++)
                if (cm[j]->IsSkinned()) skinned++;
            char key[96];
            snprintf(key, sizeof(key), "%.2f,%.2f,%.2f", w.x, w.y, w.z);
            atPos[key]++;
            printf("    %-24s world=(%8.2f %8.2f %8.2f)  meshes=%-3d skinned=%-3d "
                   "driver=%s\n",
                   c->Name() ? c->Name() : "(unnamed)", w.x, w.y, w.z, (int)cm.size(),
                   skinned, c->Driver() ? "yes" : "NO");
        }
        int distinct = (int)atPos.size();
        printf("    => %d character(s) at %d DISTINCT world position(s)%s\n",
               (int)chars.size(), distinct,
               (distinct <= 1 && chars.size() > 1)
                   ? "  <== ALL STACKED (nothing placed them)"
                   : "");
    }

    // ★ X6: ABSOLUTE census of the crowd's SHIPPED placement data.
    //
    // X5 handed off "WorldCrowd scatter" as procedural work: 6 WorldCrowd
    // objects load, none runs, so nothing places the crowd. Before writing a
    // scatter, measure whether a scatter is even the mechanism -- the charter's
    // rule is that an inherited cost estimate gets re-derived from the
    // mechanism, not from the handoff.
    //
    // WorldCrowd::Load (world/Crowd.cpp:361-368, rev >= 0xE) reads a
    // `std::list<Transform>` per CharData directly into mMMesh->Instances().
    // If those lists are non-empty, crowd placement is BAKED SHIPPED DATA that
    // is already resident, and no scatter needs to run at all.
    //
    // Printing distinct positions + an absolute bbox, not a count: X5's lesson
    // is that a count of N instances looks identical whether they are scattered
    // across the audience area or stacked on one spot.
    void ReportCrowdPlacement(ObjectDir *dir) {
        std::vector<WorldCrowd *> crowds = CollectDeep<WorldCrowd>(dir);
        printf("  --- crowd placement (%d WorldCrowd object(s)) ---\n",
               (int)crowds.size());
        int totalInst = 0;
        std::map<std::string, int> allPos;
        for (size_t i = 0; i < crowds.size(); i++) {
            WorldCrowd *wc = crowds[i];
            RndMesh *pm = wc->GetPlacementMesh();
            const std::list<WorldCrowd::CharData> &cds = wc->GetCharacters();
            int crowdInst = 0;
            printf("    %-22s placementMesh=%-22s charDefs=%d showing=%s dir=%s\n",
                   wc->Name() ? wc->Name() : "(unnamed)",
                   pm ? (pm->Name() ? pm->Name() : "(unnamed)") : "<NULL>",
                   (int)cds.size(), wc->Showing() ? "yes" : "NO",
                   wc->Dir() ? (wc->Dir()->Name() ? wc->Dir()->Name() : "(unnamed)")
                             : "<NULL>");
            for (std::list<WorldCrowd::CharData>::const_iterator it = cds.begin();
                 it != cds.end(); ++it) {
                RndMultiMesh *mm = it->mMMesh;
                Character *arch = it->mDef.mChar;
                int n = 0;
                float mnx = 0, mny = 0, mnz = 0, mxx = 0, mxy = 0, mxz = 0;
                if (mm) {
                    InstanceList &insts = mm->Instances();
                    for (InstanceList::iterator ii = insts.begin(); ii != insts.end();
                         ++ii) {
                        const Vector3 &p = ii->mXfm.v;
                        if (n == 0) {
                            mnx = mxx = p.x;
                            mny = mxy = p.y;
                            mnz = mxz = p.z;
                        } else {
                            if (p.x < mnx) mnx = p.x;
                            if (p.x > mxx) mxx = p.x;
                            if (p.y < mny) mny = p.y;
                            if (p.y > mxy) mxy = p.y;
                            if (p.z < mnz) mnz = p.z;
                            if (p.z > mxz) mxz = p.z;
                        }
                        char key[96];
                        snprintf(key, sizeof(key), "%.2f,%.2f,%.2f", p.x, p.y, p.z);
                        allPos[key]++;
                        n++;
                    }
                }
                crowdInst += n;
                printf("      archetype=%-20s mmesh=%-3s instances=%-5d "
                       "3d=%-4d bbox=(%.1f %.1f %.1f)..(%.1f %.1f %.1f)\n",
                       arch ? (arch->Name() ? arch->Name() : "(unnamed)") : "<NULL>",
                       mm ? "yes" : "NO", n, (int)it->m3DChars.size(), mnx, mny, mnz,
                       mxx, mxy, mxz);
            }
            printf("      -> %s total instances = %d\n",
                   wc->Name() ? wc->Name() : "(unnamed)", crowdInst);
            totalInst += crowdInst;
        }
        printf("    => %d crowd instance(s) at %d DISTINCT world position(s)%s\n",
               totalInst, (int)allPos.size(),
               (totalInst > 1 && (int)allPos.size() <= 1)
                   ? "  <== ALL STACKED"
                   : (totalInst == 0 ? "  <== NO SHIPPED PLACEMENT DATA" : ""));

        // ★ X6: position-set overlap between WorldCrowds.
        //
        // small_club_01 ships SIX WorldCrowds whose names look like two
        // families of three (`WorldCrowd[_frontrow]` + `_2_ps3` + `_4_ps3`,
        // with 8 / 2 / 4 archetypes). "Looks like a family" is a naming
        // inference, and the charter's rule is to measure rather than infer.
        // If two crowds' baked position SETS are identical they are the same
        // crowd authored at different archetype variety (a platform/LOD
        // variant), and drawing both puts two characters on every seat.
        std::vector<std::set<std::string> > posSets(crowds.size());
        for (size_t i = 0; i < crowds.size(); i++) {
            const std::list<WorldCrowd::CharData> &cds = crowds[i]->GetCharacters();
            for (std::list<WorldCrowd::CharData>::const_iterator it = cds.begin();
                 it != cds.end(); ++it) {
                if (!it->mMMesh) continue;
                InstanceList &insts = it->mMMesh->Instances();
                for (InstanceList::iterator ii = insts.begin(); ii != insts.end();
                     ++ii) {
                    char key[96];
                    snprintf(key, sizeof(key), "%.2f,%.2f,%.2f", ii->mXfm.v.x,
                             ii->mXfm.v.y, ii->mXfm.v.z);
                    posSets[i].insert(key);
                }
            }
        }
        printf("    --- position-set overlap (shared positions / smaller set) ---\n");
        for (size_t i = 0; i < crowds.size(); i++) {
            for (size_t j = i + 1; j < crowds.size(); j++) {
                int shared = 0;
                for (std::set<std::string>::iterator k = posSets[i].begin();
                     k != posSets[i].end(); ++k)
                    if (posSets[j].count(*k)) shared++;
                size_t smaller = posSets[i].size() < posSets[j].size() ? posSets[i].size()
                                                                      : posSets[j].size();
                if (shared == 0) continue;
                printf("      %-30s ^ %-30s %d/%d%s\n",
                       crowds[i]->Name() ? crowds[i]->Name() : "?",
                       crowds[j]->Name() ? crowds[j]->Name() : "?", shared,
                       (int)smaller,
                       (smaller && (size_t)shared == smaller) ? "  <== IDENTICAL SET"
                                                              : "");
            }
        }
    }

    // X5: gFocus restricts the CAMERA-FRAMING bounds to meshes whose name
    // contains the substring. Everything still DRAWS -- this only changes what
    // the camera is asked to frame, so the crowd can be inspected inside the
    // venue without hiding the venue. With gFocus unset the walk is the legacy
    // ObjDirItr one, so every X3/X4 bbox number reproduces byte-for-byte.
    Bounds SceneBounds(ObjectDir *dir) {
        Bounds b;
        std::vector<RndMesh *> src;
        if (gFocus) src = CollectDeep<RndMesh>(dir);
        else for (ObjDirItr<RndMesh> it(dir, true); it; ++it) src.push_back(it);
        for (size_t si = 0; si < src.size(); si++) {
            RndMesh *m = src[si];
            if (!m || !m->Showing()) continue;
            if (gFocus && !strstr(m->Name(), gFocus)) continue;
            const Transform &xfm = m->WorldXfm();
            RndMesh *owner = m->GetGeomOwner();
            if (!owner) owner = m;

            if (SkinnedMeshBounds(b, m)) {
                b.meshes++;
                continue;
            }

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
        int crowdDrawn = 0; // X6: WorldCrowd::Draw() calls issued
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

    // =====================================================================
    // X4b — THE BONE-LENGTH INVARIANT ORACLE
    // =====================================================================
    // A rigid skeleton cannot change its bone lengths. Whatever pose a clip
    // asks for, the world-space distance from a bone to its parent must equal
    // the length of that bone's LOCAL translation, because
    //
    //     WorldXfm(child) = LocalXfm(child) * WorldXfm(parent)
    //
    // puts the child's origin at  rotate(LocalXfm(child).v, parentRot) +
    // WorldXfm(parent).v, and a rotation preserves length. So
    //
    //     liveDist / |LocalXfm(child).v|  ==  1.000   exactly
    //
    // for every bone whose parent carries a pure rotation (no scale/shear).
    //
    // ★ WHY THIS IS THE RIGHT INSTRUMENT: it needs NO ground truth. There is no
    // reference PNG to diff, no retail capture to compare against, and no
    // "looks about right" judgement. Any ratio != 1.000 is a mathematical proof
    // that a transform on the compose path is wrong. It is the same oracle
    // rb3-Wii's native port used, and it is precisely the instrument that
    // catches an alias-unsafe Multiply -- which is how it earns its place here
    // (see the mtx.cpp fix landed alongside this milestone).
    //
    // Both quantities are read AT THE SAME INSTANT, after the pose is applied.
    // That is deliberate: sampling |LocalXfm().v| at bind instead would fold in
    // any clip-driven translation channel and turn a real signal into noise.
    struct BoneRatio {
        const char *name;
        const char *parent;
        float localLen;
        float liveDist;
        float ratio;
        // det(localXfm.m) and det(parentWorldXfm.m). For a rigid rig BOTH are
        // 1.000. They are carried because they SEPARATE the two ways this
        // invariant can break: a bad local pose (localDet != 1) versus a bad
        // world compose (localDet == 1 but parentDet != 1). Without them a
        // failing ratio says only "something is wrong".
        float localDet;
        float parentWorldDet;
        float selfWorldDet;
        int depth;
    };

    struct BoneAuditResult {
        int checked = 0;
        int skippedShort = 0;
        float maxRatio = 1.0f; // max over |ratio - 1| , reported as the ratio
        float maxDev = 0.0f;
        std::vector<BoneRatio> worst;
    };

    BoneAuditResult AuditBoneLengths(ObjectDir *dir) {
        BoneAuditResult res;
        std::vector<BoneRatio> all;
        for (ObjDirItr<RndTransformable> it(dir, true); it; ++it) {
            RndTransformable *t = it;
            RndTransformable *p = t->TransParent();
            if (!p) continue;
            const Vector3 &lv = t->LocalXfm().v;
            float localLen = sqrtf(lv.x * lv.x + lv.y * lv.y + lv.z * lv.z);
            // A zero-length local offset carries no length information (the
            // child sits exactly on its parent), so the ratio is undefined
            // rather than wrong. Counted and reported, never silently dropped.
            if (localLen < 1e-4f) { res.skippedShort++; continue; }
            const Vector3 &cw = t->WorldXfm().v;
            const Vector3 &pw = p->WorldXfm().v;
            float dx = cw.x - pw.x, dy = cw.y - pw.y, dz = cw.z - pw.z;
            float liveDist = sqrtf(dx * dx + dy * dy + dz * dz);
            float ratio = liveDist / localLen;
            int depth = 0;
            for (RndTransformable *q = p; q; q = q->TransParent()) depth++;
            BoneRatio br = { t->Name(),  p->Name(), localLen, liveDist, ratio,
                             Det(t->LocalXfm().m), Det(p->WorldXfm().m),
                             Det(t->WorldXfm().m), depth };
            all.push_back(br);
            res.checked++;
            float dev = fabsf(ratio - 1.0f);
            if (dev > res.maxDev) { res.maxDev = dev; res.maxRatio = ratio; }
        }
        // Sorted by DEPTH, not by badness. A collapse propagates down the
        // chain, so the interesting row is the FIRST one where the determinant
        // leaves 1.000 -- everything below it is a consequence, not a cause.
        // Sorting worst-first hides exactly the row you need.
        std::sort(all.begin(), all.end(), [](const BoneRatio &a, const BoneRatio &b) {
            return a.depth < b.depth;
        });
        res.worst = all;
        return res;
    }

    // -----------------------------------------------------------------------
    // X4c: the SKINNING PALETTE oracle.
    //
    // X4b's bone-length invariant proves the Character's RndTransformable
    // hierarchy is right. It says NOTHING about the palette the GPU actually
    // gets, which is a different quantity built from a different input --
    // RndBone::mOffset, read from the asset, not computed from the hierarchy.
    // The smear lived in exactly that gap, so it needs its own invariant.
    //
    // The invariant is exact and needs no ground truth. Mesh.cpp:1076-1083
    // builds mOffset as `meshWorld * Invert(boneWorld)`, therefore
    //
    //     skin_b := mOffset_b * boneWorld_b  ==  meshWorld     for EVERY b
    //
    // AT BIND POSE. So at bind every bone's skin matrix must be the SAME
    // matrix, and that matrix must be the mesh's own world transform. Two
    // falsifiable consequences, both checked here:
    //
    //   1. det(skin_b) == 1.000 for every b, at bind AND posed (rigid rig).
    //   2. at bind, spread(skin_b over b) == 0.
    //
    // A bone whose mOffset is garbage, or whose mBone failed to resolve, fails
    // (1) immediately -- which is the check that would have caught a truncated
    // or mis-resolved palette at load instead of at the picture.
    struct PaletteRow {
        const char *mesh;
        int idx;
        const char *bone;
        bool resolved;
        float detOffset;
        float detWorld;
        float detSkin;
        float skinTransMag;
        float devFromMesh; // max |skin - meshWorld| element, bind-pose check
    };

    struct PaletteAuditResult {
        int meshes = 0;
        int bones = 0;
        int unresolved = 0;
        int badDet = 0;
        float worstDetDev = 0.0f;
        float maxSkinTrans = 0.0f;
        std::vector<PaletteRow> rows;
    };

    PaletteAuditResult AuditPalette(ObjectDir *dir) {
        PaletteAuditResult res;
        for (ObjDirItr<RndMesh> it(dir, true); it; ++it) {
            RndMesh *m = it;
            if (!m->IsSkinned()) continue;
            res.meshes++;
            const Transform &mw = m->WorldXfm();
            for (int b = 0; b < m->NumBones(); b++) {
                PaletteRow row;
                row.mesh = m->Name();
                row.idx = b;
                RndTransformable *bt = m->BoneTransAt(b);
                row.resolved = (bt != nullptr);
                row.bone = bt ? bt->Name() : "(UNRESOLVED)";
                const Transform &off = m->BoneOffsetAt(b);
                row.detOffset = Det(off.m);
                if (!bt) {
                    res.unresolved++;
                    row.detWorld = row.detSkin = row.skinTransMag = row.devFromMesh = 0.0f;
                    res.rows.push_back(row);
                    res.bones++;
                    continue;
                }
                const Transform &bw = bt->WorldXfm();
                row.detWorld = Det(bw.m);
                Transform skin;
                Multiply(off, bw, skin);
                row.detSkin = Det(skin.m);
                row.skinTransMag = sqrtf(skin.v.x * skin.v.x + skin.v.y * skin.v.y
                                         + skin.v.z * skin.v.z);
                float dev = 0.0f;
                const float *s = &skin.m.x.x, *w = &mw.m.x.x;
                for (int k = 0; k < 9; k++) dev = std::max(dev, fabsf(s[k] - w[k]));
                dev = std::max(dev, fabsf(skin.v.x - mw.v.x));
                dev = std::max(dev, fabsf(skin.v.y - mw.v.y));
                dev = std::max(dev, fabsf(skin.v.z - mw.v.z));
                row.devFromMesh = dev;
                float dd = fabsf(row.detSkin - 1.0f);
                if (dd > 1e-2f) res.badDet++;
                if (dd > res.worstDetDev) res.worstDetDev = dd;
                if (row.skinTransMag > res.maxSkinTrans)
                    res.maxSkinTrans = row.skinTransMag;
                res.rows.push_back(row);
                res.bones++;
            }
        }
        return res;
    }

    // X4c: histogram the BLENDINDICES actually stored in the shipped compressed
    // vertex stream. The palette can be perfect and the draw still wrong if the
    // verts index it with a different convention than mBones' array order --
    // e.g. D3D9's 3-float4-rows-per-bone packing, where index == boneIdx*3.
    // 36-byte record, UBYTE4 BLENDINDICES at offset 32 (engine
    // VertexFormats.cpp:274; the field names are swapped in the struct).
    void ReportVertexBoneIndices(ObjectDir *dir) {
        printf("  --- BLENDINDICES histogram (shipped compressed verts) ---\n");
        for (ObjDirItr<RndMesh> it(dir, true); it; ++it) {
            RndMesh *m = it;
            if (!m->IsSkinned()) continue;
            RndMesh *owner = m->GetGeomOwner();
            if (!owner) owner = m;
            int ncv = owner->NumCompressedVerts();
            const unsigned char *data = owner->CompressedVerts();
            if (ncv <= 0 || !data) continue;
            int hist[256] = { 0 };
            int maxIdx = -1;
            for (int i = 0; i < ncv; i++) {
                const unsigned char *rec = data + (size_t)i * 36;
                unsigned int be;
                memcpy(&be, rec + 32, 4);
                be = __builtin_bswap32(be);
                for (int k = 0; k < 4; k++) {
                    int bi = (be >> (k * 8)) & 0xFF;
                    hist[bi]++;
                    if (bi > maxIdx) maxIdx = bi;
                }
            }
            printf("      %-34s nBones=%2d ncv=%5d maxIdx=%3d  used:", m->Name(),
                   m->NumBones(), ncv, maxIdx);
            int distinct = 0, gcd = 0;
            for (int i = 0; i < 256; i++) {
                if (!hist[i]) continue;
                distinct++;
                if (distinct <= 24) printf(" %d(%d)", i, hist[i]);
                int a = i, bg = gcd;
                while (bg) { int t = a % bg; a = bg; bg = t; }
                gcd = a;
            }
            printf("%s  [distinct=%d gcd=%d]\n", distinct > 24 ? " ..." : "", distinct,
                   gcd);
        }
    }

    // X4c: CPU linear-blend-skin the shipped verts with the LIVE palette and
    // report where the posed geometry actually lands. This is the measurement
    // that decides between "the math flings the mesh" and "the math is fine and
    // the draw is at fault" -- the two remaining explanations for the empty
    // posed frame. Mirrors the engine's decode exactly (VertexFormats.cpp:343).
    void ReportSkinnedBounds(ObjectDir *dir) {
        printf("  --- CPU-skinned bounds (LBS with the live palette) ---\n");
        for (ObjDirItr<RndMesh> it(dir, true); it; ++it) {
            RndMesh *m = it;
            if (!m->IsSkinned()) continue;
            RndMesh *owner = m->GetGeomOwner();
            if (!owner) owner = m;
            int ncv = owner->NumCompressedVerts();
            const unsigned char *data = owner->CompressedVerts();
            if (ncv <= 0 || !data) continue;
            int nb = m->NumBones();
            std::vector<Transform> pal((size_t)nb);
            for (int b = 0; b < nb; b++) {
                RndTransformable *bt = m->BoneTransAt(b);
                if (bt) Multiply(m->BoneOffsetAt(b), bt->WorldXfm(), pal[b]);
                else pal[b].Reset();
            }
            float mn[3] = { 1e30f, 1e30f, 1e30f }, mx[3] = { -1e30f, -1e30f, -1e30f };
            double wsumMin = 1e30, wsumMax = -1e30;
            int nan = 0;
            for (int i = 0; i < ncv; i++) {
                const unsigned char *rec = data + (size_t)i * 36;
                auto be32 = [&](int off) {
                    unsigned int v; memcpy(&v, rec + off, 4);
                    return __builtin_bswap32(v);
                };
                float p[3];
                for (int k = 0; k < 3; k++) {
                    unsigned int v = be32(k * 4); float f; memcpy(&f, &v, 4); p[k] = f;
                }
                unsigned int wv = be32(28), iv = be32(32);
                float w[4] = { (wv & 0x3FF) / 1023.0f, ((wv >> 10) & 0x3FF) / 1023.0f,
                               ((wv >> 20) & 0x3FF) / 1023.0f, ((wv >> 30) & 0x3) / 3.0f };
                double wsum = (double)w[0] + w[1] + w[2] + w[3];
                if (wsum < wsumMin) wsumMin = wsum;
                if (wsum > wsumMax) wsumMax = wsum;
                float acc[3] = { 0, 0, 0 };
                for (int k = 0; k < 4; k++) {
                    int bi = (iv >> (k * 8)) & 0xFF;
                    if (w[k] == 0.0f || bi >= nb) continue;
                    const Transform &t = pal[bi];
                    acc[0] += w[k] * (t.m.x.x * p[0] + t.m.y.x * p[1] + t.m.z.x * p[2] + t.v.x);
                    acc[1] += w[k] * (t.m.x.y * p[0] + t.m.y.y * p[1] + t.m.z.y * p[2] + t.v.y);
                    acc[2] += w[k] * (t.m.x.z * p[0] + t.m.y.z * p[1] + t.m.z.z * p[2] + t.v.z);
                }
                bool ok = true;
                for (int k = 0; k < 3; k++) if (acc[k] != acc[k]) ok = false;
                if (!ok) { nan++; continue; }
                for (int k = 0; k < 3; k++) {
                    if (acc[k] < mn[k]) mn[k] = acc[k];
                    if (acc[k] > mx[k]) mx[k] = acc[k];
                }
            }
            printf("      %-34s nb=%2d  skinned bbox (%.1f %.1f %.1f)..(%.1f %.1f %.1f)"
                   "  wsum %.3f..%.3f  nan=%d\n",
                   m->Name(), nb, mn[0], mn[1], mn[2], mx[0], mx[1], mx[2],
                   wsumMin, wsumMax, nan);
        }
    }

    // X4c: the bone-length invariant is INVARIANT UNDER A RIGID FLIP of the
    // whole skeleton -- every pairwise distance is preserved by a 180deg
    // rotation -- so it cannot tell a correct pose from an inverted one. This
    // prints absolute world positions of a few landmark bones, which can.
    void ReportBoneWorldPositions(ObjectDir *dir) {
        static const char *kLandmarks[] = { "bone_pelvis.mesh", "bone_spine.mesh",
                                            "bone_head.mesh",   "bone_L-ankle.mesh",
                                            "bone_R-ankle.mesh" };
        printf("  --- landmark bone WORLD positions (absolute; flip-sensitive) ---\n");
        for (ObjDirItr<RndTransformable> it(dir, true); it; ++it) {
            RndTransformable *t = it;
            if (!t->Name()) continue;
            for (size_t k = 0; k < sizeof(kLandmarks) / sizeof(kLandmarks[0]); k++) {
                if (strcmp(t->Name(), kLandmarks[k]) != 0) continue;
                const Vector3 &w = t->WorldXfm().v;
                const Vector3 &l = t->LocalXfm().v;
                printf("      %-22s world (%8.2f %8.2f %8.2f)  local (%7.2f %7.2f %7.2f)\n",
                       t->Name(), w.x, w.y, w.z, l.x, l.y, l.z);
            }
        }
    }

    void ReportPaletteAudit(const PaletteAuditResult &res) {
        printf("  --- skinning-palette invariant (skin = mOffset * boneWorld) ---\n");
        printf("      %-34s %3s %-26s %9s %9s %9s %11s %11s\n", "mesh", "idx", "bone",
               "detOffset", "detWorld", "detSkin", "|skin.v|", "devVsMeshW");
        for (size_t i = 0; i < res.rows.size(); i++) {
            const PaletteRow &r = res.rows[i];
            printf("      %-34s %3d %-26s %9.4f %9.4f %9.4f %11.4g %11.4g%s\n",
                   r.mesh ? r.mesh : "(unnamed)", r.idx, r.bone ? r.bone : "(null)",
                   r.detOffset, r.detWorld, r.detSkin, r.skinTransMag, r.devFromMesh,
                   (!r.resolved || fabsf(r.detSkin - 1.0f) > 1e-2f) ? "   <<< BAD" : "");
        }
        printf("      %d skinned mesh(es), %d bone(s), %d unresolved, %d bad det; "
               "worst |det-1| %.3e, max |skin.v| %.4g\n",
               res.meshes, res.bones, res.unresolved, res.badDet, res.worstDetDev,
               res.maxSkinTrans);
    }

    void ReportBoneAudit(const BoneAuditResult &res) {
        printf("  --- bone-length invariant (liveDist / |LocalXfm().v|, must be 1.000) ---\n");
        printf("      %-3s %-28s %-22s %9s %9s %8s %8s %10s\n", "dep", "bone", "parent",
               "authored", "live", "ratio", "detLocal", "detWorld");
        for (size_t i = 0; i < res.worst.size(); i++) {
            const BoneRatio &b = res.worst[i];
            printf("      %-3d %-28s %-22s %9.4f %9.4f %8.4f %8.4f %10.4g%s\n",
                   b.depth, b.name ? b.name : "(unnamed)", b.parent ? b.parent : "(none)",
                   b.localLen, b.liveDist, b.ratio, b.localDet, b.selfWorldDet,
                   fabsf(b.ratio - 1.0f) > 1e-3f ? "  <<" : "");
        }
        printf("      %d bone(s) checked, %d skipped (zero-length local offset)\n",
               res.checked, res.skippedShort);
    }

    // =====================================================================
    // X4b — drive a real CharClip through the engine's own poll path
    // =====================================================================
    // This is deliberately the ENGINE's path, not a hand-rolled evaluator:
    //   TheTaskMgr.SetSecondsAndBeat()  advances the clock
    //   Character::Poll()               -> RndDir::Poll() -> CharPollGroup::Poll()
    //                                   -> CharDriver -> CharServoBone -> IK ...
    // in CharPollableSorter dependency order. A bespoke "evaluate the clip and
    // write bones" loop would prove that OUR math works; this proves the
    // shipped rig works. Shape borrowed from dc3-decomp's milo_viewer
    // (native/src/viewer/milo_viewer.cpp:269-358 + ViewerAnimation.cpp:168),
    // which X3 named as the reference for this milestone.
    bool DriveCharacterClip(Character *chr, const char *&clipPlayedOut) {
        clipPlayedOut = nullptr;
        if (!chr) {
            printf("  ⚠ animation: --clips given but the scene has no Character\n");
            return false;
        }
        FilePath cf(gClipsFile);
        gClipsDir.LoadFile(cf, false, false, kLoadFront, false);
        ObjectDir *clips = gClipsDir;
        if (!clips) {
            printf("  ⚠ animation: clip file '%s' did NOT load\n", gClipsFile);
            return false;
        }
        printf("  clips: loaded '%s' (root '%s' [%s])\n", gClipsFile,
               clips->Name() ? clips->Name() : "(unnamed)", clips->ClassName().Str());

        CharDriver *driver = chr->Driver();
        if (!driver) {
            printf("  ⚠ animation: character '%s' has no CharDriver\n", chr->Name());
            return false;
        }
        driver->SetClips(clips);

        // Pick the clip. An explicit --clip name is honoured; otherwise the
        // first CharClip in the dir is used and the choice is PRINTED, so a
        // reader always knows which clip produced the picture.
        CharClip *clip = nullptr;
        int nClips = 0;
        for (ObjDirItr<CharClip> it(clips, true); it; ++it) {
            nClips++;
            if (gClipName) {
                if (strcmp(it->Name(), gClipName) == 0) clip = it;
            } else if (!clip) {
                clip = it;
            }
        }
        if (!clip) {
            printf("  ⚠ animation: no clip '%s' among %d CharClip(s) in %s\n",
                   gClipName ? gClipName : "(first)", nClips, gClipsFile);
            return false;
        }
        printf("  clips: %d CharClip(s) available; playing '%s' (beats %.2f..%.2f, "
               "%d frames)\n",
               nClips, clip->Name(), clip->StartBeat(), clip->EndBeat(),
               clip->NumFrames());

        // StuffBones registers each clip's animated channels with the servo's
        // bone array. Without it the driver plays into an empty bone set and
        // the character stays in bind pose while REPORTING that it animated --
        // the exact silent-success failure this milestone must not produce.
        CharServoBone *servo = chr->Find<CharServoBone>("bone.servo", false);
        if (servo) {
            for (ObjDirItr<CharClip> it(clips, true); it; ++it) it->StuffBones(*servo);
            printf("  clips: bones stuffed into CharServoBone '%s'\n", servo->Name());
        } else {
            printf("  ⚠ animation: no CharServoBone 'bone.servo' — bones will not move\n");
        }

        driver->Enter();
        driver->Play(clip, CharClip::kPlayNow | CharClip::kPlayLoop, -1.0f, 1e30f, 0.0f);

        // Step the clock to the requested beat. Stepping rather than jumping is
        // what the engine expects: CharDriver blends between poll instants, and
        // a single huge jump would evaluate a blend over the whole clip.
        float startBeat = clip->StartBeat();
        float targetBeat = startBeat + gBeat;
        float step = 0.1f;
        float beat = startBeat, seconds = 0.0f;
        float stepSeconds = step * 60.0f / gBpm;
        TheTaskMgr.SetSecondsAndBeat(seconds, beat, true);
        int polls = 0;
        while (beat + step < targetBeat) {
            beat += step;
            seconds += stepSeconds;
            TheTaskMgr.SetSecondsAndBeat(seconds, beat, false);
            chr->Poll();
            polls++;
        }
        seconds = (targetBeat - startBeat) * 60.0f / gBpm;
        TheTaskMgr.SetSecondsAndBeat(seconds, targetBeat, false);
        chr->Poll();
        polls++;
        printf("  clips: polled %d time(s) to beat %.3f (bpm %.1f, %.3f s)\n", polls,
               targetBeat, gBpm, seconds);
        clipPlayedOut = clip->Name();
        return true;
    }

    // ---- X5: an ABSOLUTE census of the loaded dir TREE ----
    // The venue's own graph chains venue -> world_chars -> chars, and
    // world/shared/chars.milo ships eight plain `Character` crowd members
    // (crowd_female01..crowd_male04) alongside BandCharacter player0..3. The
    // render reports 0 skinned meshes, which is consistent with BOTH "those
    // dirs never joined the tree" and "they joined it and are empty". An
    // aggregate mesh count cannot tell those apart; a per-dir object count can.
    // Empty PROXY dirs are called out explicitly because a proxy whose content
    // load was queued and never completed is exactly the shape that yields a
    // dir present in the tree with zero objects in it.
    void DumpDirTree(ObjectDir *dir, int depth, int &dirs, int &emptyProxies,
                     int &totalObjs) {
        if (!dir || depth > 12) return;
        dirs++;
        int objs = 0, meshes = 0, chars = 0;
        for (ObjectDir::Entry *e = dir->HashTable().Begin(); e;
             e = dir->HashTable().Next(e)) {
            if (!e->obj) continue;
            objs++;
            if (dynamic_cast<RndMesh *>(e->obj)) meshes++;
            if (dynamic_cast<Character *>(e->obj)) chars++;
        }
        totalObjs += objs;
        bool proxy = dir->IsProxy();
        bool empty = proxy && objs == 0;
        if (empty) emptyProxies++;
        const char *nm = dir->Name();
        if (!nm || !*nm) nm = PathName(dir);
        printf("    %*s%-30s [%-14s] objs=%-4d mesh=%-4d char=%-3d sub=%-3d%s%s\n",
               depth * 2, "", nm && *nm ? nm : "(unnamed)", dir->ClassName().Str(), objs,
               meshes, chars, (int)dir->SubDirs().size(), proxy ? "  PROXY" : "",
               empty ? "  <== EMPTY" : "");
        for (int i = 0; i < (int)dir->SubDirs().size(); i++) {
            ObjectDir *sd = dir->SubDirs()[i].Ptr();
            if (sd && sd != dir) DumpDirTree(sd, depth + 1, dirs, emptyProxies, totalObjs);
        }
        // ★ X5: also descend into dir-typed OBJECTS in the hash table. A
        // `Character` IS an ObjectDir (Character : RndDir : ObjectDir), so a
        // loaded crowd member is a fully-populated dir sitting in its parent's
        // hash table -- NOT in mSubDirs. Neither ObjDirItr's `recurse` flag nor
        // NextSubDir() ever reaches it, which is why an entire crowd can be
        // resident in the venue and still census as "0 skinned meshes".
        for (ObjectDir::Entry *e = dir->HashTable().Begin(); e;
             e = dir->HashTable().Next(e)) {
            if (!e->obj) continue;
            ObjectDir *od = dynamic_cast<ObjectDir *>(e->obj);
            if (od && od != dir) DumpDirTree(od, depth + 1, dirs, emptyProxies, totalObjs);
        }
    }

    CellResult RenderCell(const char *arkPath, const char *outDir, int frames,
                          float azimuth, float elevation, float distScale,
                          bool dumpRnd) {
        CellResult r;
        printf("\n=== %s ===\n", arkPath);

        // ★ X5: the player0 lighting stand-in, loaded BEFORE the venue.
        //
        // world/shared/chars.milo ships player0..3 as BandCharacter objects.
        // BandCharacter has no factory (its TU does not compile), so all four
        // are factory misses, and the 49 `.lit`/`.tp` refs to `player0` dangle.
        // X4d handed that off as "needs BandCharacter, i.e. the ScatterIncludes
        // lane".
        //
        // There is a cheaper seam that does NOT need BandCharacter and does NOT
        // repeat X4d's refuted base-class BIND. Binding BandCharacter to
        // Character::NewObject would make the FACTORY mis-parse BandCharacter's
        // payload as a Character -- exactly the rc=134 failure X4d measured for
        // BandCamShot/CamShot. We do not touch the factory at all: the four
        // misses stay misses and ReadDead still recovers them.
        //
        // Instead we exploit an ALREADY-PRESENT native fallback. ObjPtr's name
        // resolution (obj/ObjPtr_p.h:246) walks up the parent dir chain and
        // finally tries ObjectDir::Main(). So an ObjectDir NAMED `player0` and
        // registered in Main() BEFORE the venue loads is found by the venue's
        // own `.tp`/`.lit` refs as they load. A Character is an ObjectDir whose
        // contents are exactly the `bone_*.mesh` transforms the `.tp`s look up
        // via mPart, so a real shipped character serves.
        //
        // ⚠ STATED PLAINLY: this is a STAND-IN. It is a Character, not the
        // asset's BandCharacter, so its wardrobe/BandCharDesc state is ours,
        // not the asset's. It proves the anchor+lighting chain end to end and
        // it does not substitute for landing the real TU.
        if (gPlayerStandIn && !(ObjectDir *)gPlayerDir) {
            FilePath pf(gPlayerStandIn);
            gPlayerDir.LoadFile(pf, false, false, kLoadFront, false);
            ObjectDir *pd = gPlayerDir;
            if (pd) {
                pd->SetName("player0", ObjectDir::Main());
                printf("  player0 stand-in: '%s' [%s] registered in ObjectDir::Main()\n",
                       gPlayerStandIn, pd->ClassName().Str());
            } else {
                printf("  ⚠ player0 stand-in: '%s' did NOT load\n", gPlayerStandIn);
            }
        }

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

        if (gDumpTree) {
            ReportCharacterPlacement(dir);
            ReportCrowdPlacement(dir);
            ReportClassHistogram(dir);
            ReportTransProxyBinding(dir);
            int dirs = 0, emptyProxies = 0, totalObjs = 0;
            printf("  --- dir tree census ---\n");
            DumpDirTree(dir, 0, dirs, emptyProxies, totalObjs);
            printf("  --- %d dir(s), %d object(s), %d EMPTY proxy dir(s) ---\n", dirs,
                   totalObjs, emptyProxies);
        }

        // Census the drawable surface. This is the bridge back to X2's numbers:
        // if the mesh count here disagrees with rb3-milo's census, the render
        // is being asked about a different scene than the one X2 certified.
        // X5: `deep` additionally descends into dir-typed objects, which is
        // where a loaded Character's meshes live. Default ON; RB3_NO_DEEP_TREE
        // restores the mSubDirs-only walk for a like-for-like A/B against every
        // X3/X4a/X4b/X4c/X4d number.
        std::vector<RndMesh *> meshes;
        bool deep = getenv("RB3_NO_DEEP_TREE") == nullptr;
        if (deep) {
            meshes = CollectDeep<RndMesh>(dir);
        } else {
            for (ObjDirItr<RndMesh> it(dir, true); it; ++it) meshes.push_back(it);
        }
        printf("  mesh walk: %s -> %d mesh(es)\n",
               deep ? "DEEP (hash-table dirs + mSubDirs)" : "mSubDirs only (legacy)",
               (int)meshes.size());

        // ★ X6: collect the venue's WorldCrowd objects so the draw loop can
        // issue them.
        //
        // WHY THE CROWD WAS INVISIBLE, and why this is a DRIVER defect and not
        // a decomp one: rb3-render draws a flat std::vector<RndMesh*> and calls
        // DrawShowing() on each element. WorldCrowd is an RndDrawable but NOT
        // an RndMesh, so its DrawShowing() -- which is fully ported, 355 lines
        // at world/Crowd.cpp:1062 -- was never reached by any code path in this
        // driver. Retail never has this problem: a venue draws through the
        // RndDrawable tree, where WorldDir issues its drawables directly.
        // This is the same shape as X5's ObjDirItr finding: faithful engine
        // code, unfaithful driver.
        //
        // Nothing here places anything. The transforms come from
        // WorldCrowd::Load (world/Crowd.cpp:361-368), which deserializes a
        // std::list<Transform> per CharData straight into mMMesh->Instances().
        std::vector<WorldCrowd *> crowds;
        bool drawCrowd = getenv("RB3_NO_CROWD_DRAW") == nullptr;
        if (drawCrowd && deep) {
            crowds = CollectDeep<WorldCrowd>(dir);
            // ⛔ RETRACTED HYPOTHESIS, kept because the refutation is the
            // useful part. I first wrote this flag as a pure diagnostic on the
            // theory that small_club_01's six WorldCrowds are two families of
            // three (`WorldCrowd[_frontrow]` / `_2_ps3` / `_4_ps3`, 8/2/4
            // archetypes) holding the SAME baked positions at different
            // archetype variety -- so showing more than one per family would
            // put two characters on every seat. The naming is suggestive and
            // the theory was wrong.
            //
            // MEASURED (ReportCrowdPlacement's overlap matrix): all 15 pairs
            // share ZERO positions, and the six crowds hold 300 instances at
            // 300 DISTINCT positions. They PARTITION the audience area; they
            // do not duplicate it. So --crowd-all draws every shipped seat
            // exactly once and is legitimate, not a double-draw.
            //
            // It is still not the DEFAULT, for a different and narrower
            // reason: the asset's own mShowing flags mark 1 of the 6 live, and
            // honouring shipped flags is strictly more faithful than
            // overriding them. Retail very likely toggles these at runtime
            // (the `_ps3` names imply a platform/quality selector), but that
            // selector is not ported, so choosing which to show would be MY
            // judgement rather than the asset's. Both frames are in evidence.
            if (gCrowdShowAll) {
                for (size_t ci = 0; ci < crowds.size(); ci++)
                    crowds[ci]->SetShowing(true);
            }
            int showing = 0;
            for (size_t ci = 0; ci < crowds.size(); ci++)
                if (crowds[ci]->Showing()) showing++;
            printf("  crowd walk: %d WorldCrowd(s), %d showing%s\n", (int)crowds.size(),
                   showing, gCrowdShowAll ? "  [--crowd-all: DIAGNOSTIC]" : "");

            // ★ X6: build the placed-crowd draw list.
            //
            // ⚠ WHAT IS AND IS NOT SYNTHESIZED HERE. Every position below is
            // read from mMMesh->Instances()[i].mXfm -- asset data deserialized
            // by real engine code (world/Crowd.cpp:361-368). NO transform is
            // computed, guessed, or hand-picked by this driver.
            //
            // What IS substituted is the RASTERIZATION MECHANISM: retail draws
            // each crowd member as a camera-facing impostor billboard textured
            // from a render-to-texture snapshot of the archetype. That RTT
            // emits nothing on this native backend (measured: 300 instances
            // produce a byte-identical frame, §defects), so this driver draws
            // the archetype's REAL skinned geometry at each baked transform
            // instead. Retail has this concept as the "3D crowd" subset
            // (Draw3DChars / m3DChars); this applies it to every instance.
            //
            // The archetype meshes are also REMOVED from the flat mesh loop:
            // an archetype is a template, and drawing it at its own default
            // transform is what put eight coincident characters at the venue
            // origin in X4d/X5.
            for (size_t ci = 0; ci < crowds.size(); ci++) {
                const std::list<WorldCrowd::CharData> &cds =
                    crowds[ci]->GetCharacters();
                for (std::list<WorldCrowd::CharData>::const_iterator it = cds.begin();
                     it != cds.end(); ++it) {
                    Character *arch = it->mDef.mChar;
                    if (!arch || gArchetypeMeshes.count(arch)) continue;
                    std::vector<RndMesh *> am = CollectDeep<RndMesh>(arch);
                    gArchetypeMeshes[arch] = am;
                    for (size_t mi = 0; mi < am.size(); mi++)
                        gArchetypeMeshSet.insert(am[mi]);
                }
            }
            printf("  crowd archetypes: %d character(s), %d mesh(es) moved out of "
                   "the flat loop\n",
                   (int)gArchetypeMeshes.size(), (int)gArchetypeMeshSet.size());
        }
        for (size_t mi = 0; mi < meshes.size(); mi++) {
            RndMesh *m = meshes[mi];
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

        // ---- X4b: drive a clip, then prove the pose with the bone oracle ----
        // Order matters. The clip is applied BEFORE SceneBounds/PlaceCamera so
        // the camera frames the POSED figure, not the bind pose; and the bone
        // audit runs immediately after the pose so it measures the same instant
        // the picture is taken.
        const char *clipPlayed = nullptr;
        bool posed = false;
        if (gClipsFile && gBeat >= 0.0f) {
            posed = DriveCharacterClip(character, clipPlayed);
            Gate("clip-driven", posed,
                 posed ? "a real shipped CharClip was played through Character::Poll()"
                       : "no clip was applied — the figure below is BIND POSE");
        }
        // X5: drive every character in the scene from its OWN resident clip
        // set. Runs BEFORE SceneBounds/PlaceCamera so the camera frames the
        // posed figures, matching X4b's ordering rationale.
        if (gSceneClip) {
            int n = DriveSceneCharacters(dir, gSceneClip, gBeat < 0.0f ? 0.0f : gBeat,
                                         gBpm);
            char d[160];
            snprintf(d, sizeof(d), "%d scene character(s) driven by clip '%s'", n,
                     gSceneClip);
            Gate("scene-clip-driven", n > 0, d);
        }

        if (gBoneAudit) {
            ReportBoneWorldPositions(dir);
            ReportVertexBoneIndices(dir);
            ReportSkinnedBounds(dir);
            PaletteAuditResult pa = AuditPalette(dir);
            ReportPaletteAudit(pa);
            {
                char pd[192];
                snprintf(pd, sizeof(pd),
                         "%d bone(s) over %d mesh(es); %d unresolved, %d bad det "
                         "(worst |det-1| %.2e)",
                         pa.bones, pa.meshes, pa.unresolved, pa.badDet, pa.worstDetDev);
                Gate("palette-invariant", pa.unresolved == 0 && pa.badDet == 0, pd);
            }
            BoneAuditResult ba = AuditBoneLengths(dir);
            ReportBoneAudit(ba);
            char d[192];
            snprintf(d, sizeof(d),
                     "max ratio %.4f over %d bone(s) (deviation %.2e; tolerance 1e-3)",
                     ba.maxRatio, ba.checked, ba.maxDev);
            // A rigid skeleton has NO tolerance to spare in exact arithmetic;
            // 1e-3 is float round-off headroom over a multi-level compose, not
            // a fudge factor. A real transform bug misses by percent, not ppm.
            Gate("bone-length-invariant", ba.checked > 0 && ba.maxDev < 1e-3f, d);
        }

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
        // X4d: an explicit arkPath previously always got distScale 0.9, the
        // constant tuned for the WIDE FLAT track piece -- so rendering the tall
        // narrow character by path cropped its head and hands, while the same
        // asset framed correctly as a default cell (1.15). That is the framing
        // caveat the X4c coordinator review raised; these overrides make the
        // framing explicit and reproducible instead of implicit per-cell.
        if (gDistScale > 0.0f) distScale = gDistScale;
        if (gAzimuth > -900.0f) azimuth = gAzimuth;
        if (gElevation > -900.0f) elevation = gElevation;
        PlaceCamera(cam, b, azimuth, elevation, distScale);

        bool syntheticEnv = false;
        RndEnviron *env = EnsureEnv(dir, syntheticEnv);
        Vector3 origin(0, 0, 0);
        if (env) env->Select(&origin);

        // ---- X4a: post-processing, selected from a REAL shipped asset ------
        // Reported unconditionally (including "none") so a reader can always
        // tell which of the two A/B legs a given PNG is.
        RndPostProc *post = nullptr;
        if (gPostProcFile) {
            if (!(ObjectDir *)gPostProcDir) {
                FilePath pp(gPostProcFile);
                gPostProcDir.LoadFile(pp, false, false, kLoadFront, false);
            }
            ObjectDir *ppDir = gPostProcDir;
            if (!ppDir) {
                printf("  ⚠ postproc: '%s' did NOT load — rendering without one\n",
                       gPostProcFile);
            } else {
                int n = 0;
                for (ObjDirItr<RndPostProc> it(ppDir, true); it; ++it) {
                    n++;
                    if (!post || (gPostProcName && !strcmp(it->Name(), gPostProcName)))
                        if (!post || gPostProcName) post = it;
                }
                if (post) {
                    post->Select();
                    printf("  postproc: SELECTED '%s' from %s (%d PostProc object(s) "
                           "in the file) — RndPostProc::Current()=%p\n",
                           post->Name(), gPostProcFile, n,
                           (void *)RndPostProc::Current());
                } else {
                    printf("  ⚠ postproc: %s loaded but contains no RndPostProc\n",
                           gPostProcFile);
                }
            }
        }
        if (!post) {
            printf("  postproc: none selected — RndPostProc::Current()=%p (the "
                   "engine's Rnd_Wgpu.cpp:454 branch takes the no-postproc arm)\n",
                   (void *)RndPostProc::Current());
        }

        if (dumpRnd) DumpRndMembers("after scene setup, before first frame");

        // Draw. Several frames because the GPU-resource path is lazy: a mesh's
        // vertex buffer and a texture's GPU image are created on first use, and
        // the very first frame can legitimately draw a mesh whose texture has
        // not been uploaded yet.
        // ★ X4b: THE LOD RE-ISSUE WORKAROUND IS RETIRED — the engine fixed it.
        //
        // X3 and X4a carried a bypass here: any mesh whose name contained
        // "_lod" was re-issued through DrawMeshImmediate instead of
        // DrawShowing, because the engine's RndMesh::DrawShowing hardcoded a
        // `strstr(Name(), "_lod")` skip. That skip was a DC3 viewer heuristic
        // and it was WRONG for RB3: crowd_female01's entire body is ONE mesh
        // named `female_crowd_body01_lod02.mesh` — RB3's crowd characters are
        // authored AS the LOD-2 asset, with no higher-detail sibling to prefer.
        // Left alone it rendered as two disembodied hands (DC3's own viewer
        // produced exactly that on this asset).
        //
        // Engine `138e160` moved both hardcoded content name-filters (`_lod`
        // and `grid_80by60`) out of DrawShowing and behind the ShouldSkipMesh
        // seam, which rb3-xenon answers `false` unconditionally
        // (rb3_render_glue.cpp:45). DrawShowing therefore no longer drops
        // anything here, and the bypass is dead weight.
        //
        // RETIRED ON MEASUREMENT, NOT ON THE COMMIT MESSAGE. A/B at the pin
        // bump, bypass ON vs OFF, on the two X3 cells:
        //     crowd_female01     sha256 30692a8d02c1ada0…  IDENTICAL
        //     tracksystem_meshes sha256 cbdb29fa95a5b574…  IDENTICAL
        // crowd_female01 is the case that would break first — its body mesh is
        // the one that took the bypass — so this A/B is discriminating, not
        // vacuous. Coverage 11.07% / 17960 colours both ways.
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

        // X4c BISECT: kNewGfx has 22 consumers. Exactly ONE of them
        // (RndMesh::MaxBones, read in RndMesh::Load) is needed to stop the
        // bone truncation; the other 21 are read at DRAW time. Flipping the
        // global back to kOldGfx here -- after every Load(), before the first
        // draw -- separates the two populations with no src/ change at all.
        //   RB3_GFX_MODE=loadonly => 40 bones at load, kOldGfx at draw.
        {
            const char *gm = getenv("RB3_GFX_MODE");
            if (gm && !strcmp(gm, "loadonly")) {
                SetGfxMode(kOldGfx);
                printf("  gfx-mode: reverted to kOldGfx for DRAW (bones kept from load)\n");
            }
        }

        for (int f = 0; f < frames; f++) {
            TheRnd.BeginDrawing();
            r.drawn = 0;
            for (size_t i = 0; i < meshes.size(); i++) {
                RndMesh *m = meshes[i];
                if (!m->Showing()) continue;
                // X4d: --only-mesh <substr> isolates one drawable. A crowd
                // character ships SIX skinned meshes -- the body plus five
                // mutually-exclusive hand props (horns/fist/clap/lighter/
                // lighter.1) that the game picks ONE of per crowd member. This
                // driver draws all six at once, so anything that looks like a
                // stray skinning artifact has to be attributed per-mesh before
                // it is called a defect.
                if (gOnlyMesh && !strstr(m->Name(), gOnlyMesh)) continue;
                // X6: an archetype is a template, not a crowd member. Drawing
                // it at its own default transform is exactly what stacked
                // eight characters on the venue origin. It is drawn below,
                // once per baked instance transform, instead.
                if (gArchetypeMeshSet.count(m)) continue;
                m->DrawShowing();
                r.drawn++;
            }
            // ★ X6: the placed crowd. Positions come from the asset's baked
            // instance list; only the rasterization mechanism is ours.
            for (size_t ci = 0; ci < crowds.size(); ci++) {
                WorldCrowd *wc = crowds[ci];
                if (!wc->Showing()) continue;
                const std::list<WorldCrowd::CharData> &cds = wc->GetCharacters();
                for (std::list<WorldCrowd::CharData>::const_iterator it = cds.begin();
                     it != cds.end(); ++it) {
                    Character *arch = it->mDef.mChar;
                    RndMultiMesh *mm = it->mMMesh;
                    if (!arch || !mm) continue;
                    std::map<Character *, std::vector<RndMesh *> >::iterator ami =
                        gArchetypeMeshes.find(arch);
                    if (ami == gArchetypeMeshes.end()) continue;
                    const std::vector<RndMesh *> &am = ami->second;
                    InstanceList &insts = mm->Instances();
                    for (InstanceList::iterator ii = insts.begin(); ii != insts.end();
                         ++ii) {
                        arch->SetWorldXfm(ii->mXfm);
                        for (size_t mi = 0; mi < am.size(); mi++) {
                            RndMesh *cm = am[mi];
                            if (!cm->Showing()) continue;
                            if (gOnlyMesh && !strstr(cm->Name(), gOnlyMesh)) continue;
                            cm->DrawShowing();
                            r.crowdDrawn++;
                        }
                    }
                }
            }
            TheRnd.EndDrawing();
        }
        {
            char d[128];
            snprintf(d, sizeof(d),
                     "%d of %d meshes issued a draw, %d frame(s); crowd: %d placed "
                     "draw(s)",
                     r.drawn, r.meshes, frames, r.crowdDrawn / (frames ? frames : 1));
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
        else if (strcmp(argv[i], "--clips") == 0 && i + 1 < argc) gClipsFile = argv[++i];
        else if (strcmp(argv[i], "--clip") == 0 && i + 1 < argc) gClipName = argv[++i];
        else if (strcmp(argv[i], "--beat") == 0 && i + 1 < argc) gBeat = (float)atof(argv[++i]);
        else if (strcmp(argv[i], "--bpm") == 0 && i + 1 < argc) gBpm = (float)atof(argv[++i]);
        else if (strcmp(argv[i], "--bone-audit") == 0) gBoneAudit = true;
        else if (strcmp(argv[i], "--only-mesh") == 0 && i + 1 < argc)
            gOnlyMesh = argv[++i];
        else if (strcmp(argv[i], "--dump-tree") == 0) gDumpTree = true;
        else if (strcmp(argv[i], "--crowd-all") == 0) gCrowdShowAll = true;
        else if (strcmp(argv[i], "--focus") == 0 && i + 1 < argc) gFocus = argv[++i];
        else if (strcmp(argv[i], "--scene-clip") == 0 && i + 1 < argc)
            gSceneClip = argv[++i];
        else if (strcmp(argv[i], "--player-standin") == 0 && i + 1 < argc)
            gPlayerStandIn = argv[++i];
        else if (strcmp(argv[i], "--dist-scale") == 0 && i + 1 < argc)
            gDistScale = (float)atof(argv[++i]);
        else if (strcmp(argv[i], "--azimuth") == 0 && i + 1 < argc)
            gAzimuth = (float)atof(argv[++i]);
        else if (strcmp(argv[i], "--elevation") == 0 && i + 1 < argc)
            gElevation = (float)atof(argv[++i]);
        else if (strcmp(argv[i], "--postproc") == 0 && i + 1 < argc)
            gPostProcFile = argv[++i];
        else if (strcmp(argv[i], "--postproc-name") == 0 && i + 1 < argc)
            gPostProcName = argv[++i];
        else pos.push_back(argv[i]);
    }
    if (pos.size() < 2) {
        fprintf(stderr,
                "usage: %s <dataDir> <outDir> [<arkPath> ...] "
                "[--width N] [--height N] [--frames N] [--dump-rnd] [--dump-cam]\n"
                "       [--cam-manual] [--verbose]\n"
                "  X4b animation: --clips <arkPath> [--clip <name>] --beat <b>\n"
                "                 [--bpm <n>] [--bone-audit]\n"
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

    // ⛔ THE SKINNING SMEAR IS ROOT-CAUSED HERE, AND THE ONE-LINE FIX IS
    // DELIBERATELY NOT APPLIED. READ THIS BEFORE ADDING `SetGfxMode(kNewGfx)`.
    //
    // RndMesh::MaxBones() is `GetGfxMode() != kOldGfx ? 40 : 4`
    // (rndobj/Mesh.h:227), and RndMesh::Load ENFORCES it destructively --
    // rndobj/Mesh.cpp:567-578 does `mBones.resize(MaxBones())` after a
    // MILO_NOTIFY. gGfxMode is a zero-initialised global (os/System.cpp:53),
    // i.e. kOldGfx, and the ONLY thing that ever sets kNewGfx is
    // PreInitSystem (os/System.cpp:505) -- which this driver deliberately does
    // not call (see the StandUpConfig note above). So every skinned mesh in
    // this target was being TRUNCATED TO 4 BONES at load:
    //
    //     female_crowd_body01_lod02.mesh: exceeds bone limit (20 of 4)
    //     clap/fist/horns/lighter.mesh:   exceeds bone limit (12 of 4)
    //
    // X3 recorded that warning and left "nobody has explained the 4" open;
    // X4a carried it forward. This is the explanation, and it is not benign:
    // the bones are DELETED, so vertices weighted to bones 4..19 index palette
    // slots the engine fills with identity (BoneSetup.cpp:256-261) while
    // object.world is forced to identity for skinned meshes. Those vertices
    // stay pinned at bind coordinates while bones 0..3 animate -- which is
    // exactly the "clean at bind, smears as the pose deviates" signature X4b
    // measured before finding this.
    //
    // ★ SECOND INSTANCE OF THE SAME ROOT-CAUSE SHAPE IN THIS MILESTONE. The
    // trig-table defect (Trig.cpp) was also a SystemInit/PreInitSystem sub-init
    // that the hand-rolled bring-up skipped, also silent, also latent for four
    // milestones. PreInitSystem/SystemInit run ~10 such sub-inits; two have now
    // bitten. The rest are unaudited -- see the X4b doc's handoff table.
    //
    // ⛔ WHY THE FIX IS NOT APPLIED. `SetGfxMode(kNewGfx)` here was BUILT AND
    // MEASURED, and it is not a one-liner -- gGfxMode has 22 consumers across
    // Character, ShaderMgr, ShaderProgram, ShadowMap, rndobj/Utl, world/Crowd
    // and more, so it is a broad behavioural switch, not a targeted bone cap.
    //
    //   bind pose  coverage 11.07% -> 15.78%, 17960 -> 18882 colours.
    //              BETTER, and it CONFIRMS the diagnosis: the extra coverage is
    //              exactly the vertices that were pinned at bind coordinates by
    //              the truncation, now skinned by their real bones.
    //   POSED      coverage 23.12% -> 0.00%, 1 distinct colour. The frame goes
    //              EMPTY -- the geometry leaves the camera entirely.
    //              Not the engine's "skin fling clamp": re-measured with
    //              RB3_NO_SKIN_CLAMP=1 and it is still 0.00%.
    //
    // So restoring the bones is necessary but not sufficient: something else
    // gated on kNewGfx breaks the posed draw. Landing it would trade a smeared
    // character for no character and would regress X3's and X4a's evidence
    // PNGs, so it is recorded here and handed to X4c rather than applied.
    // The truncation above is the CAUSE of the smear; kNewGfx's blast radius
    // is the reason the cure needs its own lane.
    //
    // X4c: env-gated so the ON/OFF A/B is a run-time flag, not a recompile.
    //   RB3_GFX_MODE=new  -> SetGfxMode(kNewGfx)   (40 bones, no truncation)
    //   RB3_GFX_MODE=old  -> leave kOldGfx         (4 bones, the X4b baseline)
    {
        const char *gm = getenv("RB3_GFX_MODE");
        if (gm && !strcmp(gm, "old")) {
            printf("  gfx-mode: kOldGfx (RB3_GFX_MODE=old) — MaxBones()=4, meshes "
                   "TRUNCATED\n");
        } else {
            SetGfxMode(kNewGfx);
            printf("  gfx-mode: kNewGfx%s — MaxBones()=40\n",
                   (gm && !strcmp(gm, "loadonly")) ? " (RB3_GFX_MODE=loadonly)" : "");
        }
    }

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

    // X4c: run the boot invariants once the config and renderer are up. Both of
    // X4b's silent defects are covered here; see boot_invariants.h for why each
    // check is shaped the way it is. Advisory by default (a driver may skip a
    // sub-init it genuinely does not need); RB3_STRICT_BOOT=1 makes it fatal.
    BootInvariants::CheckAll(true);

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
