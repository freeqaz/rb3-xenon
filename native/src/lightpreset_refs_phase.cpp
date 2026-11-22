// rb3-xenon native -- a LightPreset holds its spotlights, environments, lights
// and spotlight drawers by reference.
//
// THE DEFECT. Retail puts the preset on the ref list of every object its four
// lists name, with Hmx::Object::AddRef(ObjRefOwner *) (bl 0x8275BD08): the
// Add* helpers (AddEnvironment 0x824B32A0, AddLight 0x824B33A0,
// AddSpotlightDrawer 0x824B3478, AddSpotlight 0x824B57BC), Copy (0x824B7820,
// 0x824B789C, 0x824B7928, 0x824B79B4) and Load (0x824B7CAC, 0x824B7DCC,
// 0x824B7EFC, 0x824B832C). The Remove* helpers and Clear release it
// (bl 0x8275B378). When a held object dies, ~Object's ring walk runs
// LightPreset::Replace (0x824B29B8): with no replacement it calls the matching
// Remove* (the entry and its keyframe/state slots go), with one it releases
// the old object, stores dynamic_cast<T>(to) and AddRefs `to`; then
// CacheFrames. Natively the four lists were raw std::vectors linked into no
// ring, so a dying object stayed in the preset as a dangling pointer.
//
// THE GATES. Each case builds a preset holding one object of each kind, then:
//   * ring-holds-refs   each object's ring names the preset once (retail AddRef).
//   * delete-<kind>     a plain delete of one object: its list and state
//                       vector are one shorter, the other three untouched.
//   * cascade-<kind>    the object dies in its dir's ~ObjectDir cascade.
//   * replace           spotA->ReplaceRefs(spotB): the slot names B, and the
//                       preset's ref moved from A's ring to B's.
//   * copy-refs         Copy into a second preset: each object's ring names
//                       both presets.
//   * load-refs         Save, then Load into a preset in the same dir: the
//                       loaded preset holds a ref on each object it found.
// Controls (pass with or without the fix): deleting an object the preset does
// not hold leaves its lists alone, and deleting the preset leaves no ref of
// its own on any object. cascade-all is an ASan smoke case (preset and its
// objects in one dir).

#include "obj/Dir.h"
#include "obj/Object.h"
#include "rndobj/Env.h"
#include "rndobj/Lit.h"
#include "utl/MemStream.h"
#include "world/LightPreset.h"
#include "world/Spotlight.h"
#include "world/SpotlightDrawer.h"

#include <cstdarg>
#include <cstdio>

typedef void (*GateFn)(const char *, bool, const char *);

namespace {

    GateFn gGate = nullptr;

    void Gate(const char *name, bool ok, const char *fmt, ...)
        __attribute__((format(printf, 3, 4)));
    void Gate(const char *name, bool ok, const char *fmt, ...) {
        char buf[512];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        gGate(name, ok, buf);
    }

    enum Kind { kSpot, kEnv, kLight, kDrawer, kNumKinds };
    const char *const kKindName[kNumKinds] = { "spotlight", "environ", "light", "drawer" };

    // Test access to the protected Add* helpers and the four lists.
    class PresetProbe : public LightPreset {
    public:
        PresetProbe() {}
        void Add(Kind k, Hmx::Object *o) {
            switch (k) {
            case kSpot:
                AddSpotlight(dynamic_cast<Spotlight *>(o), false);
                break;
            case kEnv:
                AddEnvironment(dynamic_cast<RndEnviron *>(o));
                break;
            case kLight:
                AddLight(dynamic_cast<RndLight *>(o));
                break;
            default:
                AddSpotlightDrawer(dynamic_cast<SpotlightDrawer *>(o));
                break;
            }
        }
        int Size(Kind k) const {
            switch (k) {
            case kSpot:
                return (int)mSpotlights.size();
            case kEnv:
                return (int)mEnvironments.size();
            case kLight:
                return (int)mLights.size();
            default:
                return (int)mSpotlightDrawers.size();
            }
        }
        int StateSize(Kind k) const {
            switch (k) {
            case kSpot:
                return (int)mSpotlightState.size();
            case kEnv:
                return (int)mEnvironmentState.size();
            case kLight:
                return (int)mLightState.size();
            default:
                return (int)mSpotlightDrawerState.size();
            }
        }
        Hmx::Object *At(Kind k, int i) const {
            switch (k) {
            case kSpot:
                return mSpotlights[i];
            case kEnv:
                return mEnvironments[i];
            case kLight:
                return mLights[i];
            default:
                return mSpotlightDrawers[i];
            }
        }
    };

    // rb3-milo has no renderer, and the SpotlightDrawer factory makes an
    // NgSpotlightDrawer whose ctor makes an RndCam (which needs TheRnd); the
    // base drawer is enough for the preset.
    class DrawerProbe : public SpotlightDrawer {
    public:
        DrawerProbe() {}
    };

    // Likewise ~RndFlare calls TheRnd.RemovePointTest, which rb3-milo's
    // zero-filled TheRnd cannot run; the probe leaves its flare alive (leaked)
    // so the spotlight itself can die.
    class SpotProbe : public Spotlight {
    public:
        SpotProbe() {}
        ~SpotProbe() { mFlare = nullptr; }
    };

    Hmx::Object *NewOf(Kind k) {
        switch (k) {
        case kSpot:
            return new SpotProbe();
        case kEnv:
            return Hmx::Object::New<RndEnviron>();
        case kLight:
            return Hmx::Object::New<RndLight>();
        default:
            return new DrawerProbe();
        }
    }

    // Refs in `target`'s ring whose owner is `owner`.
    int RefsOwnedBy(const Hmx::Object *target, const Hmx::Object *owner) {
        int n = 0;
        const ObjRefList &refs = target->Refs();
        for (ObjRefList::const_iterator it = refs.begin(); it != refs.end(); ++it) {
            if (it->RefOwner() == owner)
                n++;
        }
        return n;
    }

    struct Fixture {
        PresetProbe *preset;
        Hmx::Object *obj[kNumKinds];
    };

    // A preset holding one object of each kind. `dir`, if given, names the
    // objects (and the preset too when `presetInDir`).
    Fixture Make(ObjectDir *dir, bool presetInDir) {
        Fixture f;
        f.preset = new PresetProbe();
        if (dir && presetInDir)
            f.preset->SetName("lightpreset_refs.pst", dir);
        for (int k = 0; k != kNumKinds; k++) {
            f.obj[k] = NewOf((Kind)k);
            if (dir)
                f.obj[k]->SetName(MakeString("lightpreset_refs_%s", kKindName[k]), dir);
            f.preset->Add((Kind)k, f.obj[k]);
        }
        return f;
    }

    void RingCase() {
        Fixture f = Make(nullptr, false);
        int refs[kNumKinds];
        bool ok = true;
        for (int k = 0; k != kNumKinds; k++) {
            refs[k] = RefsOwnedBy(f.obj[k], f.preset);
            ok = ok && refs[k] == 1;
        }
        Gate("lightpreset-ring-holds-refs", ok,
             "refs owned by the preset: spotlight %d, environ %d, light %d, drawer %d "
             "(retail AddRef: 1 each)",
             refs[kSpot], refs[kEnv], refs[kLight], refs[kDrawer]);
        // The virtual-base conversion reads the vtable, so take it while the
        // preset is alive.
        const Hmx::Object *owner = f.preset;
        delete f.preset;
        int after = 0;
        for (int k = 0; k != kNumKinds; k++)
            after += RefsOwnedBy(f.obj[k], owner);
        Gate("control-lightpreset-delete-preset", after == 0,
             "after delete preset: %d ref(s) owned by it remain (retail Clear: 0)", after);
        for (int k = 0; k != kNumKinds; k++)
            delete f.obj[k];
    }

    // One object of kind `victim` dies (plain delete, or its dir's cascade).
    void DeleteCase(Kind victim, bool cascade) {
        ObjectDir *dir = cascade ? Hmx::Object::New<ObjectDir>() : nullptr;
        Fixture f = Make(nullptr, false);
        if (cascade)
            f.obj[victim]->SetName("lightpreset_refs_victim", dir);
        int before[kNumKinds];
        for (int k = 0; k != kNumKinds; k++)
            before[k] = f.preset->Size((Kind)k);
        if (cascade)
            delete dir;
        else
            delete f.obj[victim];
        bool ok = true;
        bool othersKept = true;
        for (int k = 0; k != kNumKinds; k++) {
            int want = k == victim ? 0 : 1;
            if (f.preset->Size((Kind)k) != want || f.preset->StateSize((Kind)k) != want)
                ok = false;
            if (k != victim && (f.preset->Size((Kind)k) != 1 || f.preset->At((Kind)k, 0) != f.obj[k]))
                othersKept = false;
        }
        const int after = f.preset->Size(victim);
        Gate(MakeString("lightpreset-%s-%s", cascade ? "cascade" : "delete", kKindName[victim]),
             ok && othersKept && before[victim] == 1,
             "%s killed the %s: its list %d -> %d, state %d (retail Replace -> Remove*: "
             "0 / 0); other lists %s%s",
             cascade ? "~ObjectDir's cascade" : "a plain delete", kKindName[victim],
             before[victim], after, f.preset->StateSize(victim),
             othersKept ? "kept" : "DISTURBED", after ? " -- DANGLING" : "");
        delete f.preset;
        for (int k = 0; k != kNumKinds; k++) {
            if (k != victim)
                delete f.obj[k];
        }
    }

    void UnheldControl() {
        Fixture f = Make(nullptr, false);
        Spotlight *stranger = new SpotProbe();
        delete stranger;
        bool ok = true;
        for (int k = 0; k != kNumKinds; k++)
            ok = ok && f.preset->Size((Kind)k) == 1 && f.preset->At((Kind)k, 0) == f.obj[k];
        Gate("control-lightpreset-unheld-delete", ok,
             "a spotlight the preset does not hold died: lists %d/%d/%d/%d (expect 1 each)",
             f.preset->Size(kSpot), f.preset->Size(kEnv), f.preset->Size(kLight),
             f.preset->Size(kDrawer));
        delete f.preset;
        for (int k = 0; k != kNumKinds; k++)
            delete f.obj[k];
    }

    void ReplaceCase() {
        Fixture f = Make(nullptr, false);
        Spotlight *b = new SpotProbe();
        Hmx::Object *a = f.obj[kSpot];
        a->ReplaceRefs(b);
        const bool slotB = f.preset->Size(kSpot) == 1 && f.preset->At(kSpot, 0) == b;
        const int aRefs = RefsOwnedBy(a, f.preset);
        const int bRefs = RefsOwnedBy(b, f.preset);
        Gate("lightpreset-replace", slotB && aRefs == 0 && bRefs == 1,
             "A->ReplaceRefs(B): the spotlight slot names %s; refs owned by the preset: "
             "A %d, B %d (retail: 0 / 1)",
             slotB ? "B" : (f.preset->Size(kSpot) && f.preset->At(kSpot, 0) == a ? "A" : "neither"),
             aRefs, bRefs);
        delete b;
        const int after = f.preset->Size(kSpot);
        Gate("lightpreset-replace-then-delete", after == 0,
             "then delete B: the preset holds %d spotlight(s) (retail: 0)", after);
        delete f.preset;
        for (int k = 0; k != kNumKinds; k++)
            delete f.obj[k];
    }

    void CopyCase() {
        Fixture f = Make(nullptr, false);
        PresetProbe *copy = new PresetProbe();
        copy->Copy(f.preset, Hmx::Object::kCopyDeep);
        int refs[kNumKinds];
        bool ok = true;
        for (int k = 0; k != kNumKinds; k++) {
            refs[k] = RefsOwnedBy(f.obj[k], copy);
            ok = ok && refs[k] == 1 && copy->Size((Kind)k) == 1;
        }
        Gate("lightpreset-copy-refs", ok,
             "refs owned by the copy: spotlight %d, environ %d, light %d, drawer %d "
             "(retail Copy AddRefs: 1 each)",
             refs[kSpot], refs[kEnv], refs[kLight], refs[kDrawer]);
        delete f.obj[kLight];
        Gate("lightpreset-copy-delete", copy->Size(kLight) == 0 && f.preset->Size(kLight) == 0,
             "delete the light: original holds %d, copy holds %d (retail: 0 / 0)",
             f.preset->Size(kLight), copy->Size(kLight));
        delete copy;
        delete f.preset;
        for (int k = 0; k != kNumKinds; k++) {
            if (k != kLight)
                delete f.obj[k];
        }
    }

    void LoadCase() {
        ObjectDir *dir = Hmx::Object::New<ObjectDir>();
        Fixture f = Make(dir, true);
        MemStream ms(false);
        f.preset->Save(ms);
        ms.Seek(0, BinStream::kSeekBegin);
        PresetProbe *loaded = new PresetProbe();
        loaded->SetName("lightpreset_refs_loaded.pst", dir);
        loaded->Load(ms);
        int refs[kNumKinds];
        int sizes[kNumKinds];
        bool ok = true;
        for (int k = 0; k != kNumKinds; k++) {
            refs[k] = RefsOwnedBy(f.obj[k], loaded);
            sizes[k] = loaded->Size((Kind)k);
            // Load drops a spotlight/env/light whose GetAnimateFromPreset is off;
            // whatever it keeps must be held.
            ok = ok && refs[k] == sizes[k];
        }
        ok = ok && sizes[kDrawer] == 1;
        Gate("lightpreset-load-refs", ok,
             "after Save/Load: kept spotlight %d, environ %d, light %d, drawer %d; refs "
             "owned by the loaded preset %d/%d/%d/%d (retail Load AddRefs each one kept)",
             sizes[kSpot], sizes[kEnv], sizes[kLight], sizes[kDrawer], refs[kSpot],
             refs[kEnv], refs[kLight], refs[kDrawer]);
        delete f.obj[kDrawer];
        Gate("lightpreset-load-delete", loaded->Size(kDrawer) == 0,
             "delete the drawer: the loaded preset holds %d drawer(s) (retail: 0)",
             loaded->Size(kDrawer));
        delete dir; // the presets and the remaining objects, in one cascade
        Gate("lightpreset-cascade-all", true,
             "a dir holding two presets and the objects they name deleted without incident");
    }
}

int RunLightPresetRefsPhase(GateFn gate) {
    gGate = gate;
    printf("\n=== LightPreset holds its spotlights, environments, lights and drawers by "
           "reference ===\n");
    RingCase();
    UnheldControl();
    for (int k = 0; k != kNumKinds; k++) {
        DeleteCase((Kind)k, false);
        DeleteCase((Kind)k, true);
    }
    ReplaceCase();
    CopyCase();
    LoadCase();
    return 0;
}
