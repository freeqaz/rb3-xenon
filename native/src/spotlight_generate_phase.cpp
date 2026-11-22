// rb3-xenon native -- A13: Spotlight::Generate runs retail's body on native.
//
// THE REFERENCE is band.exe (TU5, base 0x82000000), Spotlight::Generate at
// 0x824DD920 (0x8C bytes): `lwz r11,0x1b4(r3)` (mBeam.mBeam), `bne` past
// everything when it is set, otherwise store null, `fcmpu` mLength against
// 0.0f, `bl BuildNGShaft`, then two virtual calls (UpdateBounds,
// UpdateSphere). There is no TheLoadMgr.EditMode() test and no
// SpotlightDrawer::DrawNGSpotlights() choice; BuildCone and BuildBeam are not
// in the retail objects.
//
// WHAT THE NATIVE ARM CHANGED. It rebuilt on `!mBeam.mBeam ||
// TheLoadMgr.EditMode()` and chose BuildNGShaft only when
// SpotlightDrawer::DrawNGSpotlights() (kNewGfx and not kPlatformPC), else
// BuildCone / BuildBeam. EditMode() is constant false natively
// (LoadMgr::SetEditMode is retail's and ignores its argument), and play runs
// kNewGfx on kPlatformXBox, so the only reachable difference is kOldGfx (or the
// null_platform option): there the native arm built a cone/beam mesh that
// retail never builds.
//
// Cases, in a forked child (rb3-milo links no renderer; a crash is a failed
// gate, not a dead runner):
//   spot-generate-builds-shaft        a beam with a length gets a mesh (control)
//   spot-generate-oldgfx-same-shaft   the same spotlight under kOldGfx gets the
//                                     same mesh shape (vert count, face count,
//                                     constraint) as under kNewGfx: retail calls
//                                     BuildNGShaft with no gfx-mode test
//   spot-generate-no-length-no-mesh   length 0 builds nothing (retail fcmpu/ble)

#include "obj/Object.h"
#include "os/System.h"
#include "world/Spotlight.h"

#include <cstdarg>
#include <cstdio>
#include <sys/wait.h>
#include <unistd.h>

typedef void (*GateFn)(const char *, bool, const char *);

namespace {
    GateFn gGate;

    void Gate(const char *name, bool pass, const char *fmt, ...) {
        char buf[512];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        gGate(name, pass, buf);
    }

    // mBeam is protected and GetBeam() returns a BeamDef by value, whose copy
    // ctor deep-copies the mesh into a NEW RndMesh (and whose dtor releases
    // it), so it cannot answer "is this the same mesh". Read the member.
    struct SpotBeam : Spotlight {
        static RndMesh *Of(Spotlight *s) { return static_cast<SpotBeam *>(s)->mBeam.mBeam; }
    };

    // Child results, one bit each.
    enum {
        kBuilt = 1 << 0,     // length 10 -> mBeam.mBeam non-null
        kSame = 1 << 1,      // kOldGfx builds the kNewGfx shape
        kNoLength = 1 << 2,  // length 0 -> mBeam.mBeam null
        kRan = 1 << 3,       // the child reached the end
    };

    struct Shape {
        int verts = -1, faces = -1, constraint = -1;
    };

    Shape BuildIn(GfxMode mode) {
        SetGfxMode(mode);
        Spotlight *spot = Hmx::Object::New<Spotlight>();
        spot->SetProperty("length", DataNode(10.0f)); // SYNC_PROP_MODIFY -> Generate()
        Shape s;
        if (RndMesh *m = SpotBeam::Of(spot)) {
            s.verts = m->Verts().size();
            s.faces = m->NumFaces();
            s.constraint = m->TransConstraint();
        }
        return s;
    }

    int GenerateChild() {
        int bits = 0;
        Shape ng = BuildIn(kNewGfx); // what PreInitSystem sets in play (os/System.cpp)
        Shape old = BuildIn(kOldGfx);
        SetGfxMode(kNewGfx);
        if (ng.verts > 0) bits |= kBuilt;
        if (ng.verts > 0 && ng.verts == old.verts && ng.faces == old.faces
            && ng.constraint == old.constraint)
            bits |= kSame;
        fprintf(stderr,
                "  [spot-generate] kNewGfx: %d verts %d faces constraint %d; kOldGfx: %d "
                "verts %d faces constraint %d\n",
                ng.verts, ng.faces, ng.constraint, old.verts, old.faces, old.constraint);

        Spotlight *empty = Hmx::Object::New<Spotlight>();
        empty->SetProperty("length", DataNode(0.0f));
        if (!SpotBeam::Of(empty)) bits |= kNoLength;
        bits |= kRan;
        return bits;
    }
}

int RunSpotlightGeneratePhase(GateFn gate) {
    gGate = gate;
    printf("\n=== A13: Spotlight::Generate is retail's body (band.exe 0x824DD920) ===\n");
    fflush(stdout);
    fflush(stderr);
    pid_t pid = fork();
    if (pid == 0) {
        alarm(20);
        _exit(GenerateChild());
    }
    int status = 0;
    waitpid(pid, &status, 0);
    bool exited = WIFEXITED(status);
    int bits = exited ? WEXITSTATUS(status) : 0;
    const char *how = exited ? "exited" : "crashed";
    Gate("spot-generate-builds-shaft", exited && (bits & kRan) && (bits & kBuilt),
         "a beam of length 10 gets an NG shaft mesh (child %s, bits 0x%02x)", how, bits);
    Gate("spot-generate-oldgfx-same-shaft", exited && (bits & kSame),
         "kOldGfx builds the same NG shaft as kNewGfx (retail has no gfx-mode test; "
         "BuildCone/BuildBeam are not in band.exe)");
    Gate("spot-generate-no-length-no-mesh", exited && (bits & kNoLength),
         "length 0 builds no beam (retail fcmpu mLength, 0.0f)");
    return 0;
}
