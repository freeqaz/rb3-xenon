#include "rndobj/ShaderProgram.h"
#include "Memory.h"
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
    MemTemp tmp;
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

void RndShaderProgram::CopyErrorShader(ShaderType shader, const ShaderOptions &opts) {
    if (!MainThread()) {
        MILO_NOTIFY(
            "missing shader %s_%llx cannot be cached (not used in main thread).",
            ShaderTypeName(shader),
            opts.flags
        );
    }
    MILO_ASSERT(shader != kErrorShader && shader != kPostprocessErrorShader, 0x12F);

    // Determine the appropriate error shader type
    ShaderType errorType;
    if (shader == kPostprocessShader) {
        errorType = kPostprocessErrorShader;
    } else {
        errorType = kErrorShader;
    }

    // Build options mask for error shader, preserving specific flags
    u64 mask = 0;
    if (errorType == kErrorShader && (opts.flags & 0x1000)) {
        mask = 0x1000;
    }
    u64 display = TheShaderMgr.GetShaderErrorDisplay();
    mask = ((display & 1) << 0x23) | (mask & 0xfffffff7ffffffff);

    ShaderOptions newOpts(mask);
    RndShaderProgram &program = TheShaderMgr.FindShader(errorType, newOpts);
    if (!program.Cached()) {
        if (!TheShaderMgr.CacheShaders()) {
            const char *msg =
                "FAILURE: Error shader cannot be cached. Unable to handle missing shaders!\n";
            {
                FormatString fs(msg);
                TheDebug << fs.Str();
            }
            {
                FormatString fs(msg);
                TheDebug.Fail(fs.Str(), nullptr);
            }
        }
        Cache(errorType, newOpts, nullptr, nullptr);
    }
    Copy(program);
}

bool RndShaderProgram::Cache(
    ShaderType shaderType,
    const ShaderOptions &opts,
    RndShaderBuffer *vsBuffer,
    RndShaderBuffer *psBuffer
) {
    if (mCached)
        return true;
    mCached = true;
    Platform platform = TheLoadMgr.GetPlatform();
    if (platform != kPlatformNone && platform != kPlatformWii && GetGfxMode() != kOldGfx) {
        PhysMemTypeTracker tracker("D3D(phys):Shader");
        bool needsCompile =
            (vsBuffer == nullptr || vsBuffer->Size() == 0) ||
            (psBuffer == nullptr || psBuffer->Size() == 0);
        if (needsCompile) {
            if (!TheShaderMgr.CacheShaders()) {
                CopyErrorShader(shaderType, opts);
                String optsStr;
                ShaderMakeOptionsString(shaderType, opts, optsStr);
                const char *envName =
                    RndEnviron::Current()
                        ? PathName(static_cast<Hmx::Object *>(RndEnviron::Current()))
                        : nullptr;
                const char *matPath = PathName(NgMat::Current());
                MILO_NOTIFY(
                    "Missing shader %s_%llx\n(material: %s)\n(environment: %s)\n(compile options: %s)",
                    ShaderTypeName(shaderType),
                    opts.flags,
                    matPath,
                    envName,
                    optsStr.c_str()
                );
                if (UsingCD()) {
                    const char *shaderTypeName = ShaderTypeName(shaderType);
                    DataArray *cfg = SystemConfig("rnd", "title");
                    char *dataRoot = (char *)cfg->Node(1).Str(nullptr);
                    const char *envPath = PathName(
                        RndEnviron::Current()
                            ? static_cast<Hmx::Object *>(RndEnviron::Current())
                            : nullptr
                    );
                    const char *matPath2 = PathName(NgMat::Current());
                    const char *shaderHex = MakeString("%s_%llx", shaderTypeName, opts.flags);
                    const char *flagsHex = MakeString("%llx", opts.flags);
                    shaderTypeName = ShaderTypeName(shaderType);
                    const char *reportPath =
                        MakeString("debug/%s/rnd/missing_shaders", dataRoot);
                    SendDebugDataPoint(
                        reportPath,
                        "type", shaderTypeName,
                        "flags", flagsHex,
                        "shader", shaderHex,
                        "mat", matPath2,
                        "environ", envPath
                    );
                }
                return false;
            }
            AutoSlowFrame slowFrame("RndShaderProgram::Cache", 5.0f);
            char sourcePath[320];
            char cachedVsPath[256];
            char cachedPsPath[256];
            strcpy(sourcePath, ShaderSourcePath(ShaderTypeName(shaderType)));
            strcpy(cachedVsPath, ShaderCachedPath(sourcePath, opts.flags, false));
            strcpy(cachedPsPath, ShaderCachedPath(sourcePath, opts.flags, true));
            FileStat stat;
            unsigned int vsModTime = 0;
            if (FileGetStat(cachedVsPath, &stat) == 0) {
                vsModTime = stat.st_mtime;
            }
            unsigned int psModTime = vsModTime;
            if (FileGetStat(cachedPsPath, &stat) == 0) {
                if (stat.st_mtime < vsModTime) {
                    psModTime = stat.st_mtime;
                }
            } else {
                psModTime = 0;
            }
            if (psModTime < gModTime) {
                static DataNode *sCompileVerbose;
                if (!sCompileVerbose) {
                    sCompileVerbose = &DataVariable("shader_compile_print_opts");
                }
                if (sCompileVerbose->Int(nullptr) == 0) {
                    MILO_LOG(
                        "Compiling shader: %s_%llx (%s)\n",
                        ShaderTypeName(shaderType),
                        (s64)opts.flags,
                        PlatformSymbol(platform)
                    );
                } else {
                    String optsStr;
                    ShaderMakeOptionsString(shaderType, opts, optsStr);
                    MILO_LOG(
                        "Compiling shader: %s_%llx (%s) (compile options: %s)\n",
                        ShaderTypeName(shaderType),
                        (s64)opts.flags,
                        PlatformSymbol(platform),
                        optsStr.c_str()
                    );
                }
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
            if (vsBuffer) {
                vsBuffer->~RndShaderBuffer();
            }
            if (psBuffer) {
                psBuffer->~RndShaderBuffer();
            }
        } else {
            CreateVertexShader(*vsBuffer);
            CreatePixelShader(*psBuffer, shaderType);
        }
    }
    return true;
}
