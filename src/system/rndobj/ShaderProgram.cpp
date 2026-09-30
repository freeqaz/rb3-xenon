#include "rndobj/ShaderProgram.h"
#include "../../Memory.h"
#include "ShaderMgr.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/OSFuncs.h"
#include "os/System.h"
#include "os/Timer.h"
#include "rndobj/Env.h"
#include "rndobj/Mat_NG.h"
#include "rndobj/ShaderOptions.h"
#include "utl/BinStream.h"
#include "utl/DataPointMgr.h"
#include "utl/FileStream.h"
#include "utl/Loader.h"
#include "utl/MemMgr.h"
#include "math/Utl.h"
#include "obj/Data.h"

void RndShaderProgram::SaveShaderBuffer(const char *file, RndShaderBuffer &buffer) {
    FileMkDir(FileGetPath(file));
    File *f = NewFile(file, 0x301);
    f->Write(buffer.Storage(), buffer.Size());
    delete f;
}

void RndShaderProgram::LoadShaderBuffer(
    BinStream &bs, int size, RndShaderBuffer *&buffer
) {
    // Retail: the EMPTY MemDoTempAllocations guard (bare `bl 0x827BC270`
    // == ?MemPushTemp@@YAXXZ, scope-exit fn_827BC2A0), NOT the out-of-line
    // MemTemp guard — see MemMgr.h.
    MemDoTempAllocations tmp;
    buffer = NewBuffer(size);
    bs.Read(buffer->Storage(), size);
}

void RndShaderProgram::LoadShaderBuffer(const char *cc, RndShaderBuffer *&buffer) {
    FileStream stream(cc, FileStream::kReadNoArk, true);
    LoadShaderBuffer(stream, stream.Size(), buffer);
}

unsigned long gModTime;

void ShaderRecurseCB(const char *dir, const char *file) {
    FileStat stat;
    MILO_ASSERT(FileGetStat(MakeString("%s/%s", dir, file), &stat) == 0, 0x1B);
    if (stat.st_mtime > gModTime) {
        gModTime = stat.st_mtime;
    }
}

unsigned long RndShaderProgram::InitModTime() {
    gModTime = 0;
    if (TheShaderMgr.CacheShaders()) {
        FileRecursePattern(
            MakeString("%s/shaders/*.fx", FileSystemRoot()), ShaderRecurseCB, false
        );
    }
    return gModTime;
}

// Retail order: Cache (0x824A4D30) precedes CopyErrorShader (0x824A5168), and
// retail inlines Cache into CopyErrorShader's `program.Cache(...)` call, which
// needs the definition in scope first.
bool RndShaderProgram::Cache(
    ShaderType shaderType,
    const ShaderOptions &opts,
    RndShaderBuffer *vsBuffer,
    RndShaderBuffer *psBuffer
) {
    // Retail (RB3) has no platform / GetGfxMode() gate here -- that is DC3's.
    if (mCached)
        return true;
    mCached = true;
    PhysMemTypeTracker tracker("D3D(phys):Shader");
    if (vsBuffer && vsBuffer->Size() && psBuffer && psBuffer->Size()) {
        CreateVertexShader(*vsBuffer);
        CreatePixelShader(*psBuffer, shaderType);
        return true;
    }
    if (!TheShaderMgr.CacheShaders()) {
        CopyErrorShader(shaderType, opts);
        // RB3's message is the older "(%s in %s)" form (material in
        // environment): no ShaderMakeOptionsString, and the notify argument is
        // itself a MakeString that the stripped MILO_NOTIFY discards.
        MILO_NOTIFY(MakeString(
            "Missing shader %s_%llx (%s in %s)",
            ShaderTypeName(shaderType),
            opts.flags,
            PathName(NgMat::Current()),
            PathName(RndEnviron::Current())
        ));
        // No UsingCD() gate in retail; the data point's arguments are evaluated
        // and the send itself is compiled out.
        Hmx::Object *env = RndEnviron::Current();
        Hmx::Object *mat = NgMat::Current();
        DataArray *cfg = SystemConfig("rnd", "title");
        const char *dataRoot = cfg->Node(1).Str(cfg);
        const char *envPath = PathName(env);
        const char *matPath = PathName(mat);
        const char *shaderHex =
            MakeString("%s_%llx", ShaderTypeName(shaderType), opts.flags);
        const char *flagsHex = MakeString("%llx", opts.flags);
        const char *typeName = ShaderTypeName(shaderType);
        const char *reportPath = MakeString("debug/%s/rnd/missing_shaders", dataRoot);
        return false;
    }
    // Loaded once (`ld r27, 0(r28)`) and passed in a register to both
    // ShaderCachedPath calls.
    u64 flags = opts.flags;
    char sourcePath[256];
    char cachedVsPath[256];
    char cachedPsPath[256];
    strcpy(sourcePath, ShaderSourcePath(ShaderTypeName(shaderType)));
    strcpy(cachedVsPath, ShaderCachedPath(sourcePath, flags, false));
    strcpy(cachedPsPath, ShaderCachedPath(sourcePath, flags, true));
    FileStat stat;
    unsigned int modTime = 0;
    if (FileGetStat(cachedVsPath, &stat) == 0) {
        modTime = stat.st_mtime;
    }
    if (FileGetStat(cachedPsPath, &stat) == 0) {
        if (stat.st_mtime < modTime) {
            modTime = stat.st_mtime;
        }
    } else {
        modTime = 0;
    }
    if (gModTime > modTime) {
        // Retail evaluates PlatformSymbol BEFORE ShaderTypeName, i.e. in
        // function-argument (right-to-left) order -- MiloStripEval, not the
        // comma-form MILO_LOG. MiloStripEval exists only #ifndef HX_NATIVE
        // (os/Debug.h); natively this is the real log.
#ifdef HX_NATIVE
        MILO_LOG(
            "Compiling shader: %s_%llx (%s)\n",
            ShaderTypeName(shaderType),
            flags,
            PlatformSymbol(kPlatformXBox)
        );
#else
        MiloStripEval(
            "Compiling shader: %s_%llx (%s)\n",
            ShaderTypeName(shaderType),
            flags,
            PlatformSymbol(kPlatformXBox)
        );
#endif
        if (!MainThread() || !Compile(shaderType, opts, vsBuffer, psBuffer)) {
            CopyErrorShader(shaderType, opts);
            return false;
        }
        SaveShaderBuffer(cachedVsPath, *vsBuffer);
        SaveShaderBuffer(cachedPsPath, *psBuffer);
    } else {
        LoadShaderBuffer(cachedVsPath, vsBuffer);
        LoadShaderBuffer(cachedPsPath, psBuffer);
    }
    CreateVertexShader(*vsBuffer);
    CreatePixelShader(*psBuffer, shaderType);
    // `delete` (vtable slot 0 with flag 1), not an explicit destructor call.
    delete vsBuffer;
    delete psBuffer;
    return true;
}

void RndShaderProgram::CopyErrorShader(ShaderType shader, const ShaderOptions &opts) {
    if (!MainThread()) {
        MILO_NOTIFY(
            "missing shader %s_%llx cannot be cached (not used in main thread).",
            ShaderTypeName(shader),
            opts.flags
        );
    }
    MILO_ASSERT(shader != kErrorShader && shader != kPostprocessErrorShader, 0x12F);
    ShaderType errorType = shader == kPostprocessShader ? kPostprocessErrorShader : kErrorShader;
    ShaderOptions newOpts(0);
    if (errorType == kErrorShader && (opts.flags & 0x1000)) {
        newOpts.flags = 0x1000;
    }
    newOpts.flags = ((u64)(TheShaderMgr.GetShaderErrorDisplay() & 1) << 0x23)
        | (newOpts.flags & 0xfffffff7ffffffff);
    RndShaderProgram &program = TheShaderMgr.FindShader(errorType, newOpts);
    // BEHAVIOURAL FIX: retail caches the error PROGRAM (`stb 1, 0x18(r27)` on
    // FindShader's result), not `this`; and there is no CacheShaders() / FAIL
    // block here in RB3.
    if (!program.Cached()) {
        program.Cache(errorType, newOpts, nullptr, nullptr);
    }
    Copy(program);
}
