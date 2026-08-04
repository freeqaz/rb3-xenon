// rb3-xenon native — X2: load a real .milo_xbox from the mounted ark as a LIVE
// object graph and census it. LOAD ONLY. Nothing here draws.
//
// Usage:
//   ./build/rb3-milo <dataDir> <arkPath> [<arkPath> ...] [--verbose]
//
//   dataDir   directory containing gen/main_xbox.hdr + gen/main_xbox_*.ark
//   arkPath   archive-relative path, e.g. char/crowd/gen/crowd_female01.milo_xbox
//   --verbose print every object (name + class), not just the per-class census
//
// WHAT THIS PROVES, AND WHAT IT DELIBERATELY DOES NOT
// ---------------------------------------------------
// X1 stood the engine's GPU device up and cleared a frame while touching ZERO
// xenon rndobj members, so the Rnd/NgRnd member-layout question
// (src/system/rndobj/Rnd.h:354-360) stayed unmeasured. X2 comes at it from the
// other side: it exercises the xenon OBJECT graph -- DirLoader, the class
// factories, every PreLoad/PostLoad in rndobj/char/world -- with NO renderer at
// all. Neither milestone can produce a result whose correctness depends on that
// layout question, which is exactly what keeps X3's search space small when the
// first WgpuRnd frame comes out wrong.
//
// WHY THE PASS CRITERION IS "THE HEADER TABLE IS FULLY ACCOUNTED FOR"
// -------------------------------------------------------------------
// A .milo load can fail in a way that looks like success, so "exit 0" and even
// "printed a lot of objects" are both worthless as criteria. This driver reads
// the file's OWN object table twice, by two independent routes, and reconciles
// them:
//
//   (A) The ObjectDir HEADER, parsed straight off the ChunkStream: rev, dir
//       class, dir name, then N x {className, objName}. Reading it needs ZERO
//       factories, so it is ground truth for "what is in this file" no matter
//       how much of the fork compiles. It is the same byte sequence
//       DirLoader::LoadHeader + CreateObjects walk (obj/DirLoader.cpp:981,:916).
//
//   (B) The LIVE graph DirLoader actually built, walked with ObjDirItr.
//
// The gate is B + (classes with no registered factory) == A. That closes the
// silent-truncation hole: if the stream desynced halfway, B is short and the
// arithmetic fails even though nothing crashed.
//
// The distinction matters because the two failure modes are not equal. An
// unregistered LEAF class is bounded and recoverable -- DirLoader logs
// "Can't make <Class>" and ReadDead-skips its bytes to the next 0xADDEADDE
// marker (obj/DirLoader.cpp:927, :813). An unregistered *Dir SUBCLASS is not:
// it serializes a nested directory whose inner objects carry their own dead
// markers, so ReadDead stops at the first inner one and leaves the remainder in
// the stream; the parent desyncs and a later PreLoad reads a string length as a
// vector count -> runaway resize -> SIGSEGV. (Diagnosed on the rb3-Wii side;
// see rb3/native/src/rb3_game_object_factories.cpp.) So a *Dir gap is a hard
// FAIL here, while a leaf gap is reported and counted.
//
// Determinism: the census is printed from a std::map, i.e. sorted by class
// name, so two runs are diffable byte-for-byte by construction rather than by
// luck. The X2 gate runs it twice and compares.
//
// Exit codes: 0 = every requested milo loaded and reconciled. 1 = a gate failed.

#include "char/CharBoneDir.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/Dir.h"
#include "obj/DirLoader.h"
#include "obj/Object.h"
#include "os/Archive.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/System.h"
#include "utl/BinStream.h"
#include "utl/ChunkStream.h"
#include "utl/FilePath.h"
#include "utl/Loader.h"
#include "utl/Str.h"
#include "utl/Symbol.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

extern void InitMakeString();
// native/src/platform/File_Native.cpp
extern void NativeSetDataDir(const char *dir);
// native/src/platform/System_Native.cpp
extern void NativeArchiveInit();
// native/src/milo_object_factories.cpp
extern void RegisterMiloObjectFactories();
// os/System.cpp:58 -- the global system-config DTA tree SystemConfig() returns.
extern DataArray *gSystemConfig;

namespace {

    int gFailures = 0;

    void Gate(const char *name, bool ok, const char *detail) {
        printf("  [%s] %s%s%s\n", ok ? "PASS" : "FAIL", name,
               detail && *detail ? " — " : "", detail ? detail : "");
        if (!ok) gFailures++;
    }

    // ---------------------------------------------------------------------
    // (A) Ground truth: the milo's own ObjectDir header table.
    //
    // Layout for rev > 0xD (what these RB3-360 assets are):
    //     int    rev
    //     Symbol dirClass
    //     String dirName
    //     int    extSize1, extSize2      (heap reserve hints)
    //     int    numEntries
    //     numEntries * { Symbol className; String objName }
    // ---------------------------------------------------------------------
    struct HeaderTable {
        int rev = 0;
        std::string dirClass;
        std::string dirName;
        std::vector<std::string> classNames;
        bool ok = false;
    };

    HeaderTable ReadHeaderTable(const char *arkPath) {
        HeaderTable h;
        // kPlatformXBox: these are .milo_xbox, and the chunk header is
        // little-endian on disk while the body is big-endian (ReadEndian
        // handles the split). `false` for compress and cached, matching
        // DirLoader's own read of a milo.
        ChunkStream cs(arkPath, ChunkStream::kRead, 0x8000, false, kPlatformXBox, false);
        if (cs.Fail()) return h;

        // Drain chunk-boundary TempEofs before the first real read, exactly as
        // DirLoader::LoadHeader does.
        for (EofType t = cs.Eof(); t != NotEof; t = cs.Eof()) {
            if (t == RealEof) return h;
        }

        // No endian resolution here, deliberately: xenon's
        // DirLoader::LoadHeader reads the rev straight (obj/DirLoader.cpp:990)
        // and lets ChunkStream own the split -- the .milo_xbox CHUNK header is
        // little-endian on disk while the BODY is big-endian, and
        // BinStream::ReadEndian already handles that from GetPlatform(). Adding
        // a second, independent endian guess here would be a way to disagree
        // with the loader we are supposed to be checking.
        cs >> h.rev;
        if (h.rev < 7 || h.rev <= 0xD) return h; // pre-0xD layout not handled

        Symbol dirClass;
        cs >> dirClass;
        h.dirClass = dirClass.Str();
        String dirName;
        cs >> dirName;
        h.dirName = dirName.c_str();

        int extA = 0, extB = 0, count = 0;
        cs >> extA;
        cs >> extB;
        cs >> count;
        if (count < 0 || count > 200000) return h; // obviously-bogus guard

        h.classNames.reserve(count);
        for (int i = 0; i < count; i++) {
            Symbol cls;
            String name;
            cs >> cls;
            cs >> name;
            h.classNames.push_back(cls.Str());
        }
        h.ok = true;
        return h;
    }

    bool IsDirClass(const char *cls) {
        // Name-based, and honest about being so: the desync hazard is
        // specifically an ObjectDir subclass, and IsASubclass() cannot answer
        // for a class that has no factory (it is not in the class tree at all).
        // Every ObjectDir subclass in this tree ends in "Dir" -- RndDir,
        // WorldDir, CharBoneDir, PanelDir, ... -- checked against
        // src/system/**/*.h.
        size_t n = strlen(cls);
        return n >= 3 && strcmp(cls + n - 3, "Dir") == 0;
    }

    struct Census {
        std::map<std::string, int> byClass;
        int total = 0;
    };

    bool LoadAndCensus(const char *arkPath, bool verbose, Census &out) {
        // The "=== <path> ===" banner is printed by the caller, which must
        // emit it on the resolve-failure path too (before it `continue`s).

        // ---- (A) the file's own table ------------------------------------
        HeaderTable hdr = ReadHeaderTable(arkPath);
        if (!hdr.ok) {
            Gate("header-table", false, "could not parse the ObjectDir header");
            return false;
        }
        {
            char d[192];
            snprintf(d, sizeof(d), "rev %d, dir '%s' [%s], %d entries", hdr.rev,
                     hdr.dirName.c_str(), hdr.dirClass.c_str(),
                     (int)hdr.classNames.size());
            Gate("header-table", true, d);
        }

        // Which of those classes can we actually construct? RegisteredFactory
        // is the exact predicate DirLoader::CreateObjects tests
        // (obj/DirLoader.cpp:926), so this is a prediction of the load, made
        // BEFORE it runs, from the file's own contents.
        std::map<std::string, int> missing;
        for (size_t i = 0; i < hdr.classNames.size(); i++) {
            if (!Hmx::Object::RegisteredFactory(Symbol(hdr.classNames[i].c_str())))
                missing[hdr.classNames[i]]++;
        }
        int missingTotal = 0;
        bool anyDir = false;
        for (std::map<std::string, int>::const_iterator it = missing.begin();
             it != missing.end(); ++it) {
            missingTotal += it->second;
            anyDir = anyDir || IsDirClass(it->first.c_str());
        }
        if (!missing.empty()) {
            printf("--- classes with no registered factory (%d objects, %d classes) ---\n",
                   missingTotal, (int)missing.size());
            for (std::map<std::string, int>::const_iterator it = missing.begin();
                 it != missing.end(); ++it) {
                printf("  %5d  %-32s %s\n", it->second, it->first.c_str(),
                       IsDirClass(it->first.c_str())
                           ? "<-- *Dir: STREAM DESYNC RISK"
                           : "(leaf: ReadDead-skipped)");
            }
        }
        // A *Dir gap invalidates everything after it in the stream, so refuse
        // to run the load at all rather than print a census that cannot be
        // trusted (and would probably SIGSEGV first).
        Gate("no-unmakeable-Dir", !anyDir,
             anyDir ? "an ObjectDir subclass has no factory; the load would desync"
                    : (missing.empty() ? "every class in the file is constructible"
                                       : "all gaps are leaf classes"));
        if (anyDir) return false;

        // ---- (B) the live graph ------------------------------------------
        ObjectDir *dir = DirLoader::LoadObjects(FilePath(arkPath), nullptr, nullptr);
        if (!dir) {
            Gate("load", false, "DirLoader::LoadObjects returned null");
            return false;
        }
        printf("root: '%s'  [%s]\n", dir->Name() ? dir->Name() : "(unnamed)",
               dir->ClassName().Str());

        // TWO counts, deliberately. The recursive walk is the census -- it is
        // what "the object graph" means. But the header table only lists the
        // ROOT dir's own entries, so reconciling against the recursive total
        // would fail by however many objects arrived from a subdir or from a
        // FileMerger merge (crowd_female01 has one), and that failure would be
        // a bug in the check rather than in the load.
        int own = 0;
        for (ObjDirItr<Hmx::Object> it(dir, false); it; ++it) {
            if ((Hmx::Object *)it != dir) own++;
        }
        for (ObjDirItr<Hmx::Object> it(dir, true); it; ++it) {
            Hmx::Object *o = it;
            if (o == dir) continue;
            const char *cls = o->ClassName().Str();
            out.byClass[cls]++;
            out.total++;
            if (verbose) {
                printf("    %-40s [%s]\n",
                       o->Name() && *o->Name() ? o->Name() : "(unnamed)", cls);
            }
        }

        printf("--- object census (%d objects, %d distinct classes) ---\n", out.total,
               (int)out.byClass.size());
        for (std::map<std::string, int>::const_iterator it = out.byClass.begin();
             it != out.byClass.end(); ++it) {
            printf("  %5d  %s\n", it->second, it->first.c_str());
        }

        // ---- the reconciliation ------------------------------------------
        // Every entry in the file's table is either alive in the graph or was
        // skipped for a named missing class. Anything else means the stream
        // went off the rails and the census above is fiction.
        int expected = (int)hdr.classNames.size();
        int accounted = own + missingTotal;
        {
            char d[256];
            snprintf(d, sizeof(d),
                     "%d own + %d skipped = %d, header says %d"
                     " (+%d from subdirs/merges, counted in the census)",
                     own, missingTotal, accounted, expected, out.total - own);
            Gate("header-reconciled", accounted == expected, d);
        }
        Gate("nonempty-graph", out.total > 0, out.total > 0 ? "" : "loaded zero objects");
        return accounted == expected && out.total > 0;
    }
}

int main(int argc, char **argv) {
    // Line-buffer stdout: a SIGSEGV mid-load would otherwise discard the whole
    // report (block buffering when stdout is a pipe), leaving a crash with no
    // trace of how far the load got -- which is exactly the information needed.
    setvbuf(stdout, nullptr, _IOLBF, 0);

    bool verbose = false;
    std::vector<const char *> pos;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verbose") == 0) verbose = true;
        else pos.push_back(argv[i]);
    }
    if (pos.size() < 2) {
        fprintf(stderr,
                "usage: %s <dataDir> <arkPath> [<arkPath> ...] [--verbose]\n"
                "  e.g. %s ../../rb3/orig-assets/xbox-zip \\\n"
                "         char/crowd/gen/crowd_female01.milo_xbox \\\n"
                "         ui/track/gen/tracksystem_meshes.milo_xbox\n",
                argv[0], argv[0]);
        return 1;
    }
    const char *dataDir = pos[0];

    // ---- engine bring-up (identical prologue to main_ark.cpp) ------------
    InitMakeString();
    Symbol::Init();
    // ★ FileInit() is REQUIRED, not optional. It sets gRoot and
    // FilePath::Root() to "." (os/File.cpp:390-398). Without it both are the
    // empty string, so FilePath("char/crowd/gen/x.milo_xbox") becomes
    // FileMakePath("", "char/...") == "/char/..." -- an ABSOLUTE path, which
    // FileIsLocal() then routes to the host filesystem instead of the ark, and
    // DirLoader reports "Could not load" for a file that is demonstrably there
    // (rb3-ark reads it byte-exactly). main_ark.cpp gets away without it only
    // because it calls NewFile() with a raw relative string and never builds a
    // FilePath.
    FileInit();
    NativeSetDataDir(dataDir);
    // ★ MUST precede any NewFile(): os/File.cpp gates ArkFile creation on
    // UsingCD(). Without it NewFile falls through to the host filesystem and an
    // archive-relative path "fails to open" -- a missing-file symptom for what
    // is really a configuration mistake.
    SetUsingCD(true);

    printf("=== rb3-milo: object-graph load from the real RB3 archive ===\n");
    printf("dataDir : %s\n", dataDir);

    // DataInit() is the engine's own bring-up for the DTA layer: it registers
    // the data DataFuncs, the "dta"/"dtx" loader factories and TextFile, and
    // ends by calling ObjectDir::PreInit(19997, 150000) (obj/DataUtl.cpp:143),
    // which creates ObjectDir::Main(). Main() is not decoration -- the shipped
    // config DTBs execute script that resolves objects through it
    // (DataArray::Execute -> ObjectDir::FindObject), and with a null main dir
    // reading config/objects.dta segfaults inside the DTB's own script. It also
    // flips DirLoader::SetCacheMode(true) under UsingCD(), which is what makes
    // DirLoader::CachedPath resolve "foo.milo" to "gen/foo.milo_xbox".
    DataInit();

    NativeArchiveInit();
    if (!TheArchive) {
        fprintf(stderr, "FATAL: TheArchive is null after NativeArchiveInit()\n");
        return 1;
    }
    Gate("archive-mounted", true, TheArchive->GetArkfileName(0));

    // ---- system config ---------------------------------------------------
    // NOT optional, and not a nicety. DirLoader's own constructor dereferences
    // SystemConfig() unconditionally (obj/DirLoader.cpp:68, the
    // "force_milo_inline" check), so with a null gSystemConfig the very first
    // LoadObjects SIGSEGVs before it opens the file. Beyond that, PropSync of
    // typed properties reads SystemConfig("objects", <class>) for the per-class
    // type-defs, so a missing config does not merely crash -- it silently
    // degrades the load.
    //
    // We set gSystemConfig directly rather than calling PreInitSystem/
    // SystemInit: those also do DataSetMacro, OptionStr parsing,
    // DataRegisterFunc and SetGfxMode(kNewGfx) -- i.e. they start standing the
    // RENDERER up, which is the one thing X2 must not do. Same split rb3-Wii's
    // native harness makes (rb3/native/src/main_native.cpp:268).
    //
    // config/objects.dta is the shipped per-class type-def table, but it is the
    // CONTENTS of the `objects` section, not a whole system config -- its 74
    // top-level entries are the class blocks themselves. SystemConfig("objects")
    // therefore has to be given a wrapper, which is built here rather than
    // faked with DataReadString so the real shipped data is what property-sync
    // sees. RB3_SYSCFG overrides the source path for experiments.
    {
        // Comma-separated; every file's top-level entries are concatenated
        // into the one `objects` section. objects.dta carries the engine/game
        // class blocks and rnd_objects.dta the rndobj/char ones -- the shipped
        // boot merges them via DataMergeTags, and a class missing from the
        // merge makes SystemConfig("objects", <class>) MILO_FAIL and then
        // return null into a deref.
        const char *cfgList = getenv("RB3_SYSCFG");
        if (!cfgList) cfgList = "config/objects.dta,config/rnd_objects.dta";

        std::vector<DataArray *> bodies;
        std::vector<std::string> names;
        int total = 0;
        {
            std::string all(cfgList), one;
            size_t p0 = 0;
            while (p0 <= all.size()) {
                size_t c = all.find(',', p0);
                if (c == std::string::npos) c = all.size();
                one = all.substr(p0, c - p0);
                p0 = c + 1;
                if (one.empty()) continue;
                DataArray *b = DataReadFile(one.c_str(), true);
                if (!b) {
                    Gate("system-config", false, one.c_str());
                    printf("\nRESULT: FAILED (%d gate failure(s))\n", gFailures);
                    return 1;
                }
                bodies.push_back(b);
                names.push_back(one);
                total += b->Size();
            }
        }
        // (objects <block> <block> ...) -- the tag lives at Node(0), which is
        // what DataArray::FindArray(Symbol) matches on.
        DataArray *objects = new DataArray(total + 1);
        objects->Node(0) = Symbol("objects");
        int w = 1;
        for (size_t b = 0; b < bodies.size(); b++)
            for (int i = 0; i < bodies[b]->Size(); i++)
                objects->Node(w++) = bodies[b]->Node(i);
        gSystemConfig = new DataArray(1);
        gSystemConfig->Node(0) = DataNode(objects, kDataArray);
        DataVariable("syscfg") = gSystemConfig;

        if (getenv("RB3_SYSCFG_DUMP")) {
            printf("  (objects ...) class blocks:");
            for (int i = 1; i < objects->Size(); i++) {
                DataArray *b = objects->Node(i).Type() == kDataArray
                                   ? objects->Node(i).Array() : nullptr;
                printf(" %s", (b && b->Size() > 0) ? b->Sym(0).Str() : "?");
            }
            printf("\n");
        }
        char d[256];
        snprintf(d, sizeof(d), "%s -> (objects ...) with %d class blocks",
                 cfgList, total);
        Gate("system-config", SystemConfig()->FindArray(Symbol("objects"), false) != nullptr, d);
    }

    RegisterMiloObjectFactories();

    // A handful of classes keep FILE-SCOPE STATE that their Load path
    // dereferences, and that state is only set up by the class's own static
    // Init(). Registering a factory is not enough for those -- the object
    // constructs fine and then faults inside PreLoad. Each one here is added
    // because it was OBSERVED faulting, with the trace recorded, not
    // speculatively:
    //
    //   CharBoneDir::Init()  sets sCharClipTypes from
    //       SystemConfig("objects", "CharClip", "types") and creates the
    //       "char_resources" dir. Without it, CharServoBone::Load ->
    //       SetClipType -> CharBoneDir::StuffBones derefs a null
    //       sCharClipTypes (char/CharBoneDir.cpp:232). It also loads the
    //       shipped bone-resource milos named by (resource_path ...), which is
    //       real content the character rig genuinely needs -- not a side effect
    //       to be avoided.
    CharBoneDir::Init();

    Census combined;
    int loaded = 0;
    const int requested = (int)pos.size() - 1;
    for (size_t i = 1; i < pos.size(); i++) {
        // A requested milo may live in the ARK (disc content) or as a LOOSE
        // file on disk. Real DLC is the second case ALWAYS: it ships as loose
        // files inside an STFS container and is never in the .ark index, so an
        // archive-index lookup can NEVER succeed for it. Mods and
        // hand-authored assets arrive the same way. Requiring the archive here
        // was therefore a precondition no DLC could ever satisfy.
        //
        // The gate stays DISCRIMINATING rather than being deleted: a path
        // present in NEITHER the archive nor the filesystem still fails, which
        // is what catches a typo'd ark path -- the reason the check existed.
        //
        // Existence is tested with the engine's own FileGetStat, not a raw
        // stat() here, because FileGetStat qualifies through
        // FileQualifiedFilename -- the SAME qualification the loose-file
        // fallback in File_Native.cpp's FileIsLocal() uses. So this gate and
        // the loader NewFile() will run agree by construction, and the gate
        // cannot pass on a file the loader would then refuse.
        int arkNum = 0, fileSize = 0, ucSize = 0;
        unsigned long long byteOff = 0;
        const bool inArk = TheArchive->GetFileInfo(FileMakePath(".", pos[i]), arkNum,
                                                   byteOff, fileSize, ucSize);
        FileStat st;
        const bool onDisk = !inArk && FileGetStat(pos[i], &st) == 0;
        if (!inArk && !onDisk) {
            printf("\n=== %s ===\n", pos[i]);
            Gate("path-resolved", false,
                 "in neither the archive index nor the filesystem");
            continue;
        }
        printf("\n=== %s ===\n", pos[i]);
        Gate("path-resolved", true, inArk ? "archive" : "loose file");
        Census c;
        if (LoadAndCensus(pos[i], verbose, c)) loaded++;
        for (std::map<std::string, int>::const_iterator it = c.byClass.begin();
             it != c.byClass.end(); ++it) {
            combined.byClass[it->first] += it->second;
        }
        combined.total += c.total;
    }

    printf("\n=== combined: %d objects across %d/%d milo(s), %d distinct classes ===\n",
           combined.total, loaded, requested, (int)combined.byClass.size());
    Gate("all-milos-loaded", loaded == requested, "");

    printf("\nRESULT: %s (%d gate failure(s))\n",
           gFailures == 0 ? "ALL GATES PASSED" : "FAILED", gFailures);
    return gFailures == 0 ? 0 : 1;
}
